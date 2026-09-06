#include "eye_target_probe.hpp"

#include "opengl_eye_targets.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <atomic>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlRenderbufferBinding = 0x8CA7;

enum class RequestState : std::uint8_t {
    idle,
    preparing,
    pending,
    processing,
    passed,
    failed,
};

enum class RequestKind : std::uint8_t {
    transient_validation,
    persistent_create,
    persistent_destroy,
};

struct OpenGlStateSnapshot {
    GLint framebuffer = 0;
    GLint renderbuffer = 0;
    GLint texture = 0;
    std::array<GLint, 4> viewport{};
};

graphics::OpenGlEyeTargets g_persistent_targets;
std::atomic<RequestState> g_request_state{RequestState::idle};
RequestKind g_request_kind = RequestKind::transient_validation;
std::uint32_t g_requested_width = 0;
std::uint32_t g_requested_height = 0;
std::array<char, 192> g_request_error{};

std::atomic<bool> g_persistent_active{false};
std::atomic<std::uint32_t> g_persistent_width{0};
std::atomic<std::uint32_t> g_persistent_height{0};
std::atomic<std::uint64_t> g_persistent_frames{0};
std::atomic<std::uint64_t> g_last_lifetime_frames{0};

SRWLOCK g_event_lock = SRWLOCK_INIT;
EyeTargetProbeTelemetry g_event;

[[nodiscard]] OpenGlStateSnapshot CaptureOpenGlState() noexcept {
    OpenGlStateSnapshot state;
    glGetIntegerv(kGlFramebufferBinding, &state.framebuffer);
    glGetIntegerv(kGlRenderbufferBinding, &state.renderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture);
    glGetIntegerv(GL_VIEWPORT, state.viewport.data());
    return state;
}

[[nodiscard]] bool SameOpenGlState(
    const OpenGlStateSnapshot& left,
    const OpenGlStateSnapshot& right) noexcept {
    return left.framebuffer == right.framebuffer &&
        left.renderbuffer == right.renderbuffer &&
        left.texture == right.texture &&
        left.viewport == right.viewport;
}

[[nodiscard]] bool ValidateTransientTargets(
    bool& state_restored,
    std::string& error) noexcept {
    const OpenGlStateSnapshot before = CaptureOpenGlState();
    graphics::OpenGlEyeTargets targets;
    graphics::OpenGlEyeBinding binding;
    bool success = targets.CreateOrResize(512, 512, error);
    if (success) {
        success = targets.BeginEye(graphics::Eye::left, binding, error);
    }
    if (success) {
        success = targets.EndEye(binding, error);
    }
    if (success) {
        success = targets.CreateOrResize(640, 480, error);
    }
    if (success) {
        success = targets.BeginEye(graphics::Eye::right, binding, error);
    }
    if (success) {
        success = targets.EndEye(binding, error);
    }

    const std::string operation_error = error;
    std::string cleanup_error;
    if (binding.active) {
        static_cast<void>(targets.EndEye(binding, cleanup_error));
    }
    const bool destroyed = targets.Destroy(cleanup_error);
    state_restored = SameOpenGlState(before, CaptureOpenGlState());

    if (!success) {
        error = operation_error;
    } else if (!destroyed) {
        error = "Eye target cleanup failed: " + cleanup_error;
        success = false;
    } else if (!state_restored) {
        error = "Eye target validation did not restore the incoming OpenGL state";
        success = false;
    }
    return success;
}

[[nodiscard]] bool CreatePersistentTargets(
    std::uint32_t width,
    std::uint32_t height,
    bool& state_restored,
    std::string& error) noexcept {
    if (g_persistent_active.load(std::memory_order_acquire)) {
        error = "Persistent eye targets are already active";
        return false;
    }

    const OpenGlStateSnapshot before = CaptureOpenGlState();
    const bool created = g_persistent_targets.CreateOrResize(width, height, error);
    state_restored = SameOpenGlState(before, CaptureOpenGlState());
    if (!created || !state_restored) {
        const std::string operation_error = error;
        std::string cleanup_error;
        static_cast<void>(g_persistent_targets.Destroy(cleanup_error));
        if (created) {
            error = "Persistent eye target creation did not restore OpenGL state";
        } else {
            error = operation_error;
        }
        return false;
    }

    g_persistent_width.store(width, std::memory_order_release);
    g_persistent_height.store(height, std::memory_order_release);
    g_persistent_frames.store(0, std::memory_order_release);
    g_last_lifetime_frames.store(0, std::memory_order_release);
    g_persistent_active.store(true, std::memory_order_release);
    return true;
}

[[nodiscard]] bool DestroyPersistentTargetsOnRenderThread(
    bool& state_restored,
    std::string& error) noexcept {
    if (!g_persistent_active.load(std::memory_order_acquire)) {
        state_restored = true;
        return true;
    }

    const OpenGlStateSnapshot before = CaptureOpenGlState();
    const std::uint64_t lifetime =
        g_persistent_frames.load(std::memory_order_acquire);
    const bool destroyed = g_persistent_targets.Destroy(error);
    state_restored = SameOpenGlState(before, CaptureOpenGlState());
    if (destroyed) {
        g_last_lifetime_frames.store(lifetime, std::memory_order_release);
        g_persistent_active.store(false, std::memory_order_release);
        g_persistent_width.store(0, std::memory_order_release);
        g_persistent_height.store(0, std::memory_order_release);
    }
    if (destroyed && !state_restored) {
        error = "Persistent eye target destruction did not restore OpenGL state";
        return false;
    }
    return destroyed;
}

void RecordEvent(
    EyeTargetProbeEvent event,
    bool passed,
    bool state_restored,
    std::uint32_t width,
    std::uint32_t height,
    const std::string& error) noexcept {
    AcquireSRWLockExclusive(&g_event_lock);
    g_event.event = event;
    g_event.event_passed = passed;
    g_event.state_restored = state_restored;
    g_event.width = width;
    g_event.height = height;
    strncpy_s(g_event.error.data(), g_event.error.size(), error.c_str(), _TRUNCATE);
    ReleaseSRWLockExclusive(&g_event_lock);
}

[[nodiscard]] EyeTargetProbeEvent EventForRequest(RequestKind kind) noexcept {
    switch (kind) {
        case RequestKind::persistent_create:
            return EyeTargetProbeEvent::persistent_created;
        case RequestKind::persistent_destroy:
            return EyeTargetProbeEvent::persistent_destroyed;
        default:
            return EyeTargetProbeEvent::transient_validation;
    }
}

[[nodiscard]] bool SubmitRequest(
    RequestKind kind,
    std::uint32_t width,
    std::uint32_t height,
    std::string& error) noexcept {
    error.clear();

    RequestState state = g_request_state.load(std::memory_order_acquire);
    if (state == RequestState::passed || state == RequestState::failed) {
        static_cast<void>(g_request_state.compare_exchange_strong(
            state, RequestState::idle, std::memory_order_acq_rel));
    }

    RequestState expected = RequestState::idle;
    if (!g_request_state.compare_exchange_strong(
            expected, RequestState::preparing, std::memory_order_acq_rel)) {
        error = "Another eye-target request is already active";
        return false;
    }
    g_request_kind = kind;
    g_requested_width = width;
    g_requested_height = height;
    g_request_error = {};
    g_request_state.store(RequestState::pending, std::memory_order_release);

    constexpr DWORD kTimeoutMilliseconds = 5000;
    for (DWORD elapsed = 0; elapsed < kTimeoutMilliseconds; ++elapsed) {
        state = g_request_state.load(std::memory_order_acquire);
        if (state == RequestState::passed || state == RequestState::failed) {
            const bool passed = state == RequestState::passed;
            if (!passed) {
                error = g_request_error.data();
            }
            g_request_state.store(RequestState::idle, std::memory_order_release);
            return passed;
        }
        Sleep(1);
    }

    expected = RequestState::pending;
    static_cast<void>(g_request_state.compare_exchange_strong(
        expected, RequestState::idle, std::memory_order_acq_rel));
    error = "Timed out waiting for RenderWorld to process the eye-target request";
    return false;
}

} // namespace

void ResetEyeTargetProbe() noexcept {
    g_request_kind = RequestKind::transient_validation;
    g_requested_width = 0;
    g_requested_height = 0;
    g_request_error = {};
    g_request_state.store(RequestState::idle, std::memory_order_release);
    g_persistent_active.store(false, std::memory_order_release);
    g_persistent_width.store(0, std::memory_order_release);
    g_persistent_height.store(0, std::memory_order_release);
    g_persistent_frames.store(0, std::memory_order_release);
    g_last_lifetime_frames.store(0, std::memory_order_release);
    AcquireSRWLockExclusive(&g_event_lock);
    g_event = {};
    ReleaseSRWLockExclusive(&g_event_lock);
}

void ProcessEyeTargetRequestsOnRenderThread(bool count_frame) noexcept {
    RequestState expected = RequestState::pending;
    if (g_request_state.compare_exchange_strong(
            expected, RequestState::processing, std::memory_order_acq_rel)) {
        const RequestKind kind = g_request_kind;
        const std::uint32_t requested_width = g_requested_width;
        const std::uint32_t requested_height = g_requested_height;
        std::uint32_t event_width = requested_width;
        std::uint32_t event_height = requested_height;
        bool state_restored = false;
        bool passed = false;
        std::string error;

        if (wglGetCurrentContext() == nullptr) {
            error = "No current OpenGL context at render-thread boundary";
        } else {
            switch (kind) {
                case RequestKind::persistent_create:
                    passed = CreatePersistentTargets(
                        requested_width, requested_height, state_restored, error);
                    break;
                case RequestKind::persistent_destroy:
                    event_width = g_persistent_width.load(std::memory_order_acquire);
                    event_height = g_persistent_height.load(std::memory_order_acquire);
                    passed = DestroyPersistentTargetsOnRenderThread(
                        state_restored, error);
                    break;
                default:
                    passed = ValidateTransientTargets(state_restored, error);
                    break;
            }
        }

        strncpy_s(
            g_request_error.data(),
            g_request_error.size(),
            error.c_str(),
            _TRUNCATE);
        RecordEvent(
            EventForRequest(kind),
            passed,
            state_restored,
            event_width,
            event_height,
            error);
        g_request_state.store(
            passed ? RequestState::passed : RequestState::failed,
            std::memory_order_release);
    }

    if (count_frame && g_persistent_active.load(std::memory_order_acquire)) {
        g_persistent_frames.fetch_add(1, std::memory_order_acq_rel);
    }
}

bool RequestTransientEyeTargetValidation(std::string& error) noexcept {
    return SubmitRequest(RequestKind::transient_validation, 640, 480, error);
}

bool RequestPersistentEyeTargets(
    std::uint32_t width,
    std::uint32_t height,
    std::string& error) noexcept {
    if (g_persistent_active.load(std::memory_order_acquire)) {
        error = "Persistent eye targets are already active";
        return false;
    }
    return SubmitRequest(RequestKind::persistent_create, width, height, error);
}

bool DestroyPersistentEyeTargets(std::string& error) noexcept {
    if (!g_persistent_active.load(std::memory_order_acquire) &&
        g_request_state.load(std::memory_order_acquire) == RequestState::idle) {
        error.clear();
        return true;
    }
    return SubmitRequest(RequestKind::persistent_destroy, 0, 0, error);
}

bool BeginPersistentEyeTarget(
    graphics::Eye eye,
    graphics::OpenGlEyeBinding& binding,
    std::string& error) noexcept {
    if (!g_persistent_active.load(std::memory_order_acquire)) {
        error = "Persistent eye targets are not active";
        return false;
    }
    return g_persistent_targets.BeginEye(eye, binding, error);
}

bool EndPersistentEyeTarget(
    graphics::OpenGlEyeBinding& binding,
    std::string& error) noexcept {
    return g_persistent_targets.EndEye(binding, error);
}

bool GetPersistentEyeColorTextures(
    std::array<std::uint32_t, 2>& color_textures,
    std::string& error) noexcept {
    error.clear();
    color_textures = {};
    if (!g_persistent_active.load(std::memory_order_acquire) ||
        !g_persistent_targets.ready()) {
        error = "Persistent eye targets are not active";
        return false;
    }
    color_textures = {
        g_persistent_targets.target(graphics::Eye::left).color_texture,
        g_persistent_targets.target(graphics::Eye::right).color_texture,
    };
    if (color_textures[0] == 0 || color_textures[1] == 0) {
        color_textures = {};
        error = "A persistent eye target has no color texture";
        return false;
    }
    return true;
}

bool PersistentEyeTargetsActive() noexcept {
    return g_persistent_active.load(std::memory_order_acquire);
}

std::uint64_t PersistentEyeTargetLifetimeFrames() noexcept {
    if (g_persistent_active.load(std::memory_order_acquire)) {
        return g_persistent_frames.load(std::memory_order_acquire);
    }
    return g_last_lifetime_frames.load(std::memory_order_acquire);
}

EyeTargetProbeTelemetry ConsumeEyeTargetProbeTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_event_lock);
    EyeTargetProbeTelemetry result = g_event;
    g_event = {};
    ReleaseSRWLockExclusive(&g_event_lock);
    result.persistent_active =
        g_persistent_active.load(std::memory_order_acquire);
    if (result.persistent_active) {
        result.width = g_persistent_width.load(std::memory_order_acquire);
        result.height = g_persistent_height.load(std::memory_order_acquire);
    }
    result.persistent_frames =
        g_persistent_frames.load(std::memory_order_acquire);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
