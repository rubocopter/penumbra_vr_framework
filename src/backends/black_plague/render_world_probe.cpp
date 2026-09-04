#include "render_world_probe.hpp"

#include "camera_matrix_override.hpp"
#include "rel32_call_hook.hpp"
#include "vr_math.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::uintptr_t kRenderWorldRva = 0x0012CB10;
constexpr std::uintptr_t kRenderWorldCallSiteRva = 0x000EE010;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlMaxRenderbufferSize = 0x84E8;
constexpr std::array<std::uint8_t, 5> kExpectedCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};

using RenderWorld = void(__thiscall*)(void* renderer, void* world, void* camera, float frame_time);

enum class DuplicationState : std::uint8_t {
    idle,
    preparing,
    pending,
    processing,
    passed,
    failed,
};

enum class StereoMatrixState : std::uint8_t {
    idle,
    preparing,
    pending,
    processing,
    passed,
    failed,
};

hooks::Rel32CallHook g_hook;
std::atomic<void*> g_original_target{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
std::atomic<DuplicationState> g_duplication_state{DuplicationState::idle};
std::atomic<std::uint32_t> g_duplication_requested_frames{0};
std::atomic<std::uint32_t> g_duplication_completed_frames{0};
std::atomic<bool> g_duplication_cancel{false};
std::array<char, 192> g_duplication_error{};
std::atomic<StereoMatrixState> g_stereo_state{StereoMatrixState::idle};
std::atomic<std::uint32_t> g_stereo_requested_frames{0};
std::atomic<std::uint32_t> g_stereo_completed_frames{0};
std::atomic<bool> g_stereo_cancel{false};
std::array<runtime::VrEyeConfiguration, 2> g_stereo_eyes{};
std::array<runtime::VrMatrix44, 2> g_stereo_projections{};
std::array<char, 192> g_stereo_error{};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
RenderWorldFrameTelemetry g_telemetry;
std::atomic<bool> g_capabilities_initialized{false};
FramebufferApi g_framebuffer_api = FramebufferApi::unavailable;
std::array<char, 64> g_open_gl_version{};
std::array<GLint, 2> g_max_viewport_dimensions{};
GLint g_max_texture_size = 0;
GLint g_max_renderbuffer_size = 0;

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

void FailDuplication(const std::string& error) noexcept {
    strncpy_s(
        g_duplication_error.data(),
        g_duplication_error.size(),
        error.c_str(),
        _TRUNCATE);
    g_duplication_state.store(DuplicationState::failed, std::memory_order_release);
}

[[nodiscard]] bool DuplicationActive() noexcept {
    const DuplicationState state =
        g_duplication_state.load(std::memory_order_acquire);
    return state == DuplicationState::preparing ||
        state == DuplicationState::pending ||
        state == DuplicationState::processing;
}

[[nodiscard]] bool StereoMatrixValidationActive() noexcept {
    const StereoMatrixState state =
        g_stereo_state.load(std::memory_order_acquire);
    return state == StereoMatrixState::preparing ||
        state == StereoMatrixState::pending ||
        state == StereoMatrixState::processing;
}

void RecordStereoCameraRestorationFailure() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry.stereo_camera_restored = false;
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

[[nodiscard]] bool LooksLikeMappedGameplayCamera(
    const CameraMatrixSnapshot& snapshot) noexcept {
    constexpr float kTolerance = 1.0e-3F;
    const auto& projection = snapshot.projection.values;
    const auto& view = snapshot.view.values;
    return snapshot.flags[0] == 1 &&
        snapshot.flags[1] <= 1 && snapshot.flags[2] <= 1 &&
        std::fabs(projection[10] + 1.0F) <= kTolerance &&
        projection[11] < 0.0F &&
        std::fabs(projection[14] + 1.0F) <= kTolerance &&
        std::fabs(projection[15]) <= kTolerance &&
        std::fabs(view[12]) <= kTolerance &&
        std::fabs(view[13]) <= kTolerance &&
        std::fabs(view[14]) <= kTolerance &&
        std::fabs(view[15] - 1.0F) <= kTolerance;
}

void FailStereoMatrixValidation(const std::string& error) noexcept {
    strncpy_s(
        g_stereo_error.data(),
        g_stereo_error.size(),
        error.c_str(),
        _TRUNCATE);
    g_stereo_state.store(StereoMatrixState::failed, std::memory_order_release);
}

[[nodiscard]] bool RenderStereoEye(
    RenderWorld original,
    void* renderer,
    void* world,
    void* camera,
    const runtime::VrMatrix44& head_view,
    std::size_t eye_index,
    std::string& error) noexcept {
    runtime::VrMatrix44 eye_view;
    if (!runtime::ComposeEyeViewFromHeadView(
            head_view, g_stereo_eyes[eye_index].eye_to_head, eye_view, error)) {
        return false;
    }

    const graphics::Eye eye = eye_index == 0
        ? graphics::Eye::left
        : graphics::Eye::right;
    graphics::OpenGlEyeBinding binding;
    if (!BeginPersistentEyeTarget(eye, binding, error)) {
        return false;
    }

    CameraMatrixOverride camera_override;
    if (!camera_override.Apply(
            camera, eye_view, g_stereo_projections[eye_index], error)) {
        const std::string operation_error = error;
        std::string binding_error;
        static_cast<void>(EndPersistentEyeTarget(binding, binding_error));
        error = operation_error;
        if (!binding_error.empty()) {
            error += "; eye binding cleanup also failed: " + binding_error;
        }
        return false;
    }

    original(renderer, world, camera, 0.0F);

    std::string camera_error;
    const bool camera_restored = camera_override.Restore(camera_error);
    std::string binding_error;
    const bool binding_restored = EndPersistentEyeTarget(binding, binding_error);
    if (!camera_restored || !binding_restored) {
        if (!camera_restored) {
            RecordStereoCameraRestorationFailure();
            error = "Could not restore the HPL camera after an eye pass: " +
                camera_error;
        }
        if (!binding_restored) {
            if (!error.empty()) {
                error += "; ";
            }
            error += "Could not restore GL state after an eye pass: " +
                binding_error;
        }
        return false;
    }
    return true;
}

void ProcessControlledStereoMatrices(
    RenderWorld original,
    void* renderer,
    void* world,
    void* camera) noexcept {
    StereoMatrixState state = g_stereo_state.load(std::memory_order_acquire);
    if (state == StereoMatrixState::pending) {
        StereoMatrixState expected = StereoMatrixState::pending;
        if (!g_stereo_state.compare_exchange_strong(
                expected,
                StereoMatrixState::processing,
                std::memory_order_acq_rel)) {
            return;
        }
        state = StereoMatrixState::processing;
    }
    if (state != StereoMatrixState::processing) {
        return;
    }
    if (g_stereo_cancel.load(std::memory_order_acquire)) {
        FailStereoMatrixValidation("Controlled stereo-matrix validation was cancelled");
        return;
    }
    if (original == nullptr) {
        FailStereoMatrixValidation("The original RenderWorld target is unavailable");
        return;
    }

    CameraMatrixSnapshot camera_snapshot;
    std::string error;
    if (!CaptureCameraMatrices(camera, camera_snapshot, error)) {
        FailStereoMatrixValidation("Could not capture the HPL camera: " + error);
        return;
    }
    if (!LooksLikeMappedGameplayCamera(camera_snapshot)) {
        FailStereoMatrixValidation(
            "The active camera does not match the mapped infinite-perspective layout");
        return;
    }

    std::uint32_t completed_eye_passes = 0;
    for (std::size_t eye_index = 0; eye_index < g_stereo_eyes.size(); ++eye_index) {
        if (!RenderStereoEye(
                original,
                renderer,
                world,
                camera,
                camera_snapshot.view,
                eye_index,
                error)) {
            FailStereoMatrixValidation("Controlled stereo eye pass failed: " + error);
            return;
        }
        ++completed_eye_passes;
    }

    if (!CameraMatchesSnapshot(camera, camera_snapshot)) {
        RecordStereoCameraRestorationFailure();
        FailStereoMatrixValidation(
            "The camera bytes changed after the controlled stereo pair");
        return;
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.stereo_frames;
    g_telemetry.stereo_eye_passes += completed_eye_passes;
    g_telemetry.stereo_camera_restored = true;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    const std::uint32_t completed =
        g_stereo_completed_frames.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (completed >= g_stereo_requested_frames.load(std::memory_order_acquire)) {
        g_stereo_state.store(StereoMatrixState::passed, std::memory_order_release);
    }
}

void ProcessControlledWorldDuplication(
    RenderWorld original,
    void* renderer,
    void* world,
    void* camera) noexcept {
    DuplicationState state = g_duplication_state.load(std::memory_order_acquire);
    if (state == DuplicationState::pending) {
        DuplicationState expected = DuplicationState::pending;
        if (!g_duplication_state.compare_exchange_strong(
                expected,
                DuplicationState::processing,
                std::memory_order_acq_rel)) {
            return;
        }
        state = DuplicationState::processing;
    }
    if (state != DuplicationState::processing) {
        return;
    }
    if (g_duplication_cancel.load(std::memory_order_acquire)) {
        FailDuplication("Controlled world duplication was cancelled");
        return;
    }
    if (original == nullptr) {
        FailDuplication("The original RenderWorld target is unavailable");
        return;
    }

    graphics::OpenGlEyeBinding binding;
    std::string error;
    if (!BeginPersistentEyeTarget(graphics::Eye::left, binding, error)) {
        FailDuplication("Could not bind the diagnostic world target: " + error);
        return;
    }

    original(renderer, world, camera, 0.0F);

    if (!EndPersistentEyeTarget(binding, error)) {
        FailDuplication("Could not restore GL state after the duplicate world pass: " + error);
        return;
    }

    const std::uint32_t completed =
        g_duplication_completed_frames.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (completed >=
        g_duplication_requested_frames.load(std::memory_order_acquire)) {
        g_duplication_state.store(DuplicationState::passed, std::memory_order_release);
    }
}

void __fastcall HookedRenderWorld(
    void* renderer,
    void*,
    void* world,
    void* camera,
    float frame_time) noexcept {
    ActiveCall active_call;
    InitializeOpenGlCapabilities();
    ProcessEyeTargetRequestsOnRenderThread();

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
        ProcessControlledStereoMatrices(
            original, renderer, world, camera);
        ProcessControlledWorldDuplication(
            original, renderer, world, camera);
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
    g_capabilities_initialized.store(false, std::memory_order_release);
    g_framebuffer_api = FramebufferApi::unavailable;
    g_open_gl_version = {};
    g_max_viewport_dimensions = {};
    g_max_texture_size = 0;
    g_max_renderbuffer_size = 0;
    g_duplication_error = {};
    g_duplication_requested_frames.store(0, std::memory_order_release);
    g_duplication_completed_frames.store(0, std::memory_order_release);
    g_duplication_cancel.store(false, std::memory_order_release);
    g_duplication_state.store(DuplicationState::idle, std::memory_order_release);
    g_stereo_error = {};
    g_stereo_eyes = {};
    g_stereo_projections = {};
    g_stereo_requested_frames.store(0, std::memory_order_release);
    g_stereo_completed_frames.store(0, std::memory_order_release);
    g_stereo_cancel.store(false, std::memory_order_release);
    g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
    ResetEyeTargetProbe();
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
    if (DuplicationActive()) {
        error = "A controlled world-duplication request is still active";
        return false;
    }
    if (StereoMatrixValidationActive()) {
        error = "A controlled stereo-matrix request is still active";
        return false;
    }
    if (!hooks::RemoveRel32CallHook(g_hook, error)) {
        return false;
    }

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for an active RenderWorld probe call to finish";
    return false;
}

bool ValidateControlledWorldDuplication(
    std::uint32_t frames,
    std::string& error) noexcept {
    error.clear();
    if (frames == 0 || frames > 600) {
        error = "Controlled world duplication requires between 1 and 600 frames";
        return false;
    }
    if (!PersistentEyeTargetsActive()) {
        error = "Persistent eye targets must be active before world duplication";
        return false;
    }
    if (StereoMatrixValidationActive()) {
        error = "A controlled stereo-matrix request is active";
        return false;
    }

    DuplicationState state = g_duplication_state.load(std::memory_order_acquire);
    if (state == DuplicationState::passed || state == DuplicationState::failed) {
        static_cast<void>(g_duplication_state.compare_exchange_strong(
            state, DuplicationState::idle, std::memory_order_acq_rel));
    }
    DuplicationState expected = DuplicationState::idle;
    if (!g_duplication_state.compare_exchange_strong(
            expected, DuplicationState::preparing, std::memory_order_acq_rel)) {
        error = "Another controlled world-duplication request is active";
        return false;
    }

    g_duplication_error = {};
    g_duplication_requested_frames.store(frames, std::memory_order_release);
    g_duplication_completed_frames.store(0, std::memory_order_release);
    g_duplication_cancel.store(false, std::memory_order_release);
    g_duplication_state.store(DuplicationState::pending, std::memory_order_release);

    constexpr DWORD kTimeoutMilliseconds = 15000;
    for (DWORD elapsed = 0; elapsed < kTimeoutMilliseconds; ++elapsed) {
        state = g_duplication_state.load(std::memory_order_acquire);
        if (state == DuplicationState::passed || state == DuplicationState::failed) {
            const bool passed = state == DuplicationState::passed;
            if (!passed) {
                error = g_duplication_error.data();
            }
            g_duplication_state.store(DuplicationState::idle, std::memory_order_release);
            return passed;
        }
        Sleep(1);
    }

    g_duplication_cancel.store(true, std::memory_order_release);
    expected = DuplicationState::pending;
    static_cast<void>(g_duplication_state.compare_exchange_strong(
        expected, DuplicationState::failed, std::memory_order_acq_rel));
    error = "Timed out waiting for controlled world duplication";
    return false;
}

bool ValidateControlledStereoMatrices(
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::uint32_t frames,
    std::string& error) noexcept {
    error.clear();
    if (frames == 0 || frames > 300) {
        error = "Controlled stereo matrices require between 1 and 300 frames";
        return false;
    }
    if (!PersistentEyeTargetsActive()) {
        error = "Persistent eye targets must be active before stereo validation";
        return false;
    }
    if (DuplicationActive()) {
        error = "A controlled world-duplication request is active";
        return false;
    }

    StereoMatrixState state = g_stereo_state.load(std::memory_order_acquire);
    if (state == StereoMatrixState::passed || state == StereoMatrixState::failed) {
        static_cast<void>(g_stereo_state.compare_exchange_strong(
            state, StereoMatrixState::idle, std::memory_order_acq_rel));
    }
    StereoMatrixState expected = StereoMatrixState::idle;
    if (!g_stereo_state.compare_exchange_strong(
            expected,
            StereoMatrixState::preparing,
            std::memory_order_acq_rel)) {
        error = "Another controlled stereo-matrix request is active";
        return false;
    }

    std::array<runtime::VrMatrix44, 2> projections{};
    for (std::size_t eye_index = 0; eye_index < eyes.size(); ++eye_index) {
        if (!runtime::BuildHplInfiniteProjection(
                eyes[eye_index], near_clip, projections[eye_index], error)) {
            g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
            return false;
        }
    }

    g_stereo_error = {};
    g_stereo_eyes = eyes;
    g_stereo_projections = projections;
    g_stereo_requested_frames.store(frames, std::memory_order_release);
    g_stereo_completed_frames.store(0, std::memory_order_release);
    g_stereo_cancel.store(false, std::memory_order_release);
    g_stereo_state.store(StereoMatrixState::pending, std::memory_order_release);

    constexpr DWORD kTimeoutMilliseconds = 15000;
    for (DWORD elapsed = 0; elapsed < kTimeoutMilliseconds; ++elapsed) {
        state = g_stereo_state.load(std::memory_order_acquire);
        if (state == StereoMatrixState::passed ||
            state == StereoMatrixState::failed) {
            const bool passed = state == StereoMatrixState::passed;
            if (!passed) {
                error = g_stereo_error.data();
            }
            g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
            return passed;
        }
        Sleep(1);
    }

    g_stereo_cancel.store(true, std::memory_order_release);
    expected = StereoMatrixState::pending;
    static_cast<void>(g_stereo_state.compare_exchange_strong(
        expected, StereoMatrixState::failed, std::memory_order_acq_rel));
    error = "Timed out waiting for controlled stereo-matrix validation";
    return false;
}

RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    RenderWorldFrameTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    result.eye_targets = ConsumeEyeTargetProbeTelemetry();
    return result;
}

} // namespace penumbra_vr::backends::black_plague
