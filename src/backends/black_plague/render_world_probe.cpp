#include "render_world_probe.hpp"

#include "opengl_eye_targets.hpp"
#include "rel32_call_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <atomic>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::uintptr_t kRenderWorldRva = 0x0012CB10;
constexpr std::uintptr_t kRenderWorldCallSiteRva = 0x000EE010;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlMaxRenderbufferSize = 0x84E8;
constexpr GLenum kGlRenderbufferBinding = 0x8CA7;
constexpr std::array<std::uint8_t, 5> kExpectedCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};

using RenderWorld = void(__thiscall*)(void* renderer, void* world, void* camera, float frame_time);

hooks::Rel32CallHook g_hook;
std::atomic<void*> g_original_target{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
RenderWorldFrameTelemetry g_telemetry;
std::atomic<bool> g_capabilities_initialized{false};
FramebufferApi g_framebuffer_api = FramebufferApi::unavailable;
std::array<char, 64> g_open_gl_version{};
std::array<GLint, 2> g_max_viewport_dimensions{};
GLint g_max_texture_size = 0;
GLint g_max_renderbuffer_size = 0;

enum class EyeTargetValidationState : std::uint8_t {
    idle,
    pending,
    processing,
    passed,
    failed,
};

std::atomic<EyeTargetValidationState> g_eye_target_validation_state{
    EyeTargetValidationState::idle};
std::array<char, 192> g_eye_target_validation_error{};

[[nodiscard]] bool HasOpenGlProcedure(const char* name) noexcept {
    const PROC procedure = wglGetProcAddress(name);
    return procedure != nullptr &&
        procedure != reinterpret_cast<PROC>(1) &&
        procedure != reinterpret_cast<PROC>(2) &&
        procedure != reinterpret_cast<PROC>(3) &&
        procedure != reinterpret_cast<PROC>(-1);
}

[[nodiscard]] bool HasFramebufferProcedures(const char* suffix) noexcept {
    constexpr std::array<const char*, 10> kNames{
        "glGenFramebuffers",
        "glDeleteFramebuffers",
        "glBindFramebuffer",
        "glCheckFramebufferStatus",
        "glFramebufferTexture2D",
        "glFramebufferRenderbuffer",
        "glGenRenderbuffers",
        "glDeleteRenderbuffers",
        "glBindRenderbuffer",
        "glRenderbufferStorage",
    };

    std::array<char, 64> procedure_name{};
    for (const char* base_name : kNames) {
        if (sprintf_s(procedure_name.data(), procedure_name.size(), "%s%s", base_name, suffix) < 0 ||
            !HasOpenGlProcedure(procedure_name.data())) {
            return false;
        }
    }
    return true;
}

void InitializeOpenGlCapabilities() noexcept {
    if (g_capabilities_initialized.load(std::memory_order_acquire) ||
        wglGetCurrentContext() == nullptr) {
        return;
    }

    FramebufferApi framebuffer_api = FramebufferApi::unavailable;
    if (HasFramebufferProcedures("")) {
        framebuffer_api = FramebufferApi::core;
    } else if (HasFramebufferProcedures("EXT")) {
        framebuffer_api = FramebufferApi::ext;
    }

    std::array<char, 64> version{};
    const GLubyte* version_bytes = glGetString(GL_VERSION);
    if (version_bytes != nullptr) {
        strncpy_s(
            version.data(),
            version.size(),
            reinterpret_cast<const char*>(version_bytes),
            _TRUNCATE);
    }

    std::array<GLint, 2> max_viewport_dimensions{};
    GLint max_texture_size = 0;
    GLint max_renderbuffer_size = 0;
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_dimensions.data());
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv(kGlMaxRenderbufferSize, &max_renderbuffer_size);

    AcquireSRWLockExclusive(&g_telemetry_lock);
    if (!g_capabilities_initialized.load(std::memory_order_relaxed)) {
        g_framebuffer_api = framebuffer_api;
        g_open_gl_version = version;
        g_max_viewport_dimensions = max_viewport_dimensions;
        g_max_texture_size = max_texture_size;
        g_max_renderbuffer_size = max_renderbuffer_size;
        g_capabilities_initialized.store(true, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

struct OpenGlStateSnapshot {
    GLint framebuffer = 0;
    GLint renderbuffer = 0;
    GLint texture = 0;
    std::array<GLint, 4> viewport{};
};

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

[[nodiscard]] bool ValidateEyeTargetsOnRenderThread(
    bool& state_restored,
    std::string& error) noexcept {
    error.clear();
    state_restored = false;
    if (wglGetCurrentContext() == nullptr) {
        error = "No current OpenGL context at RenderWorld";
        return false;
    }

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
    const OpenGlStateSnapshot after = CaptureOpenGlState();
    state_restored = SameOpenGlState(before, after);

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

void ProcessPendingEyeTargetValidation() noexcept {
    EyeTargetValidationState expected = EyeTargetValidationState::pending;
    if (!g_eye_target_validation_state.compare_exchange_strong(
            expected,
            EyeTargetValidationState::processing,
            std::memory_order_acq_rel)) {
        return;
    }

    bool state_restored = false;
    std::string error;
    const bool passed = ValidateEyeTargetsOnRenderThread(state_restored, error);
    strncpy_s(
        g_eye_target_validation_error.data(),
        g_eye_target_validation_error.size(),
        error.c_str(),
        _TRUNCATE);

    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry.eye_target_validation_completed = true;
    g_telemetry.eye_target_validation_passed = passed;
    g_telemetry.eye_target_state_restored = state_restored;
    g_telemetry.eye_target_validation_error = g_eye_target_validation_error;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    g_eye_target_validation_state.store(
        passed ? EyeTargetValidationState::passed : EyeTargetValidationState::failed,
        std::memory_order_release);
}

class ActiveCall final {
public:
    ActiveCall() noexcept {
        g_active_calls.fetch_add(1, std::memory_order_acq_rel);
    }
    ActiveCall(const ActiveCall&) = delete;
    ActiveCall& operator=(const ActiveCall&) = delete;
    ~ActiveCall() {
        g_active_calls.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void __fastcall HookedRenderWorld(
    void* renderer,
    void*,
    void* world,
    void* camera,
    float frame_time) noexcept {
    ActiveCall active_call;
    InitializeOpenGlCapabilities();
    ProcessPendingEyeTargetValidation();

    std::array<GLint, 4> viewport{};
    GLint framebuffer_binding = 0;
    const bool has_current_gl_context = wglGetCurrentContext() != nullptr;
    if (has_current_gl_context) {
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        glGetIntegerv(kGlFramebufferBinding, &framebuffer_binding);
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.calls;
    g_telemetry.renderer = reinterpret_cast<std::uintptr_t>(renderer);
    g_telemetry.world = reinterpret_cast<std::uintptr_t>(world);
    g_telemetry.camera = reinterpret_cast<std::uintptr_t>(camera);
    g_telemetry.frame_time = frame_time;
    g_telemetry.has_current_gl_context = has_current_gl_context;
    g_telemetry.framebuffer_api = g_framebuffer_api;
    g_telemetry.viewport = {
        viewport[0], viewport[1], viewport[2], viewport[3],
    };
    g_telemetry.max_viewport_dimensions = {
        g_max_viewport_dimensions[0], g_max_viewport_dimensions[1],
    };
    g_telemetry.framebuffer_binding = framebuffer_binding;
    g_telemetry.max_texture_size = g_max_texture_size;
    g_telemetry.max_renderbuffer_size = g_max_renderbuffer_size;
    g_telemetry.open_gl_version = g_open_gl_version;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    const auto original = reinterpret_cast<RenderWorld>(
        g_original_target.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(renderer, world, camera, frame_time);
    }
}

[[nodiscard]] void* DecodeExpectedTarget(std::uint8_t* instruction) noexcept {
    std::int32_t displacement = 0;
    std::memcpy(&displacement, kExpectedCall.data() + 1, sizeof(displacement));
    const std::intptr_t next_instruction =
        reinterpret_cast<std::intptr_t>(instruction + kExpectedCall.size());
    return reinterpret_cast<void*>(next_instruction + displacement);
}

} // namespace

bool InstallRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (g_hook.installed()) {
        error = "The Black Plague RenderWorld probe is already installed";
        return false;
    }

    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "GetModuleHandleW(NULL) failed";
        return false;
    }
    std::uint8_t* call_site = image + kRenderWorldCallSiteRva;
    void* expected_target = image + kRenderWorldRva;
    if (DecodeExpectedTarget(call_site) != expected_target) {
        error = "The manifest call displacement does not target RenderWorld";
        return false;
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_active_calls.store(0, std::memory_order_release);
    g_capabilities_initialized.store(false, std::memory_order_release);
    g_framebuffer_api = FramebufferApi::unavailable;
    g_open_gl_version = {};
    g_max_viewport_dimensions = {};
    g_max_texture_size = 0;
    g_max_renderbuffer_size = 0;
    g_eye_target_validation_error = {};
    g_eye_target_validation_state.store(
        EyeTargetValidationState::idle, std::memory_order_release);
    g_original_target.store(expected_target, std::memory_order_release);

    if (!hooks::InstallRel32CallHook(
            call_site,
            kExpectedCall,
            reinterpret_cast<void*>(&HookedRenderWorld),
            g_hook,
            error)) {
        g_original_target.store(nullptr, std::memory_order_release);
        return false;
    }
    return true;
}

bool RemoveRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (!hooks::RemoveRel32CallHook(g_hook, error)) {
        return false;
    }

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            g_original_target.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for an active RenderWorld probe call to finish";
    return false;
}

bool RequestEyeTargetValidation(std::string& error) noexcept {
    error.clear();
    EyeTargetValidationState expected = EyeTargetValidationState::idle;
    if (!g_eye_target_validation_state.compare_exchange_strong(
            expected,
            EyeTargetValidationState::pending,
            std::memory_order_acq_rel)) {
        error = "An eye-target validation request is already active or complete";
        return false;
    }

    constexpr DWORD kTimeoutMilliseconds = 5000;
    for (DWORD elapsed = 0; elapsed < kTimeoutMilliseconds; ++elapsed) {
        const EyeTargetValidationState state =
            g_eye_target_validation_state.load(std::memory_order_acquire);
        if (state == EyeTargetValidationState::passed ||
            state == EyeTargetValidationState::failed) {
            const bool passed = state == EyeTargetValidationState::passed;
            if (!passed) {
                error = g_eye_target_validation_error.data();
            }
            g_eye_target_validation_state.store(
                EyeTargetValidationState::idle, std::memory_order_release);
            return passed;
        }
        Sleep(1);
    }

    expected = EyeTargetValidationState::pending;
    static_cast<void>(g_eye_target_validation_state.compare_exchange_strong(
        expected,
        EyeTargetValidationState::idle,
        std::memory_order_acq_rel));
    error = "Timed out waiting for RenderWorld to process the eye-target validation";
    return false;
}

RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    const RenderWorldFrameTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
