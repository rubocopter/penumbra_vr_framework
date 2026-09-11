#include "render_world_probe.hpp"
#include "black_plague_body_adapter.hpp"

#include "camera_matrix_override.hpp"
#include "opengl_eye_scissor.hpp"
#include "opengl_menu_frame.hpp"
#include "opengl_tracked_hands.hpp"
#include "native_input_bridge.hpp"
#include "vr_grab_pose.hpp"
#include "rel32_call_hook.hpp"
#include "stereo_render_policy.hpp"
#include "vr_math.hpp"
#include "vr_panel_policy.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::uintptr_t kRenderWorldRva = 0x0012CB10;
constexpr std::uintptr_t kRenderWorldCallSiteRva = 0x000EE010;
constexpr std::uintptr_t kUpdateRenderListRva = 0x0012A8F0;
constexpr std::uintptr_t kUpdateRenderListCallSiteRva = 0x000EDF84;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlMaxRenderbufferSize = 0x84E8;
constexpr std::array<std::uint8_t, 5> kExpectedRenderWorldCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};
constexpr std::array<std::uint8_t, 5> kExpectedUpdateRenderListCall{
    0xE8, 0x67, 0xC9, 0x03, 0x00,
};
constexpr float kVisibilityAngularGuardRadians = 0.087266463F; // 5 degrees.
// Positional tracking is deliberately disabled until the exact-build HPL
// character-body adapter can reconcile requested and collision-resolved motion.
constexpr float kPositionalWorldUnitsPerMeter = 0.0F;
static_assert(kPositionalWorldUnitsPerMeter == 0.0F,
    "Tracking/body shadow validation must not enable positional translation");

using RenderWorld = void(__thiscall*)(void* renderer, void* world, void* camera, float frame_time);
using UpdateRenderList = void(__thiscall*)(
    void* renderer,
    void* world,
    void* camera,
    float frame_time);

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
    stopping,
    passed,
    failed,
};

struct StereoProcessingResult {
    bool completed = false;
    bool frame_time_consumed_by_eye = false;
    bool suppress_original_world = false;
};

hooks::Rel32CallHook g_hook;
hooks::Rel32CallHook g_visibility_hook;
std::atomic<void*> g_original_target{nullptr};
std::atomic<void*> g_original_visibility_target{nullptr};
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
std::atomic<bool> g_stereo_persistent{false};
std::atomic<bool> g_stereo_monitor_mirror{false};
std::atomic<std::uint32_t> g_stereo_frame_calls{0};
std::array<runtime::VrEyeConfiguration, 2> g_stereo_eyes{};
std::array<runtime::VrMatrix44, 2> g_stereo_projections{};
std::array<char, 192> g_stereo_error{};
std::atomic<runtime::OpenVrSession*> g_stereo_session{nullptr};
bool g_stereo_track_head_rotation = false;
bool g_stereo_tracking_anchor_valid = false;
runtime::VrMatrix34 g_stereo_tracking_anchor{};
bool g_stereo_latest_pose_valid = false;
runtime::VrMatrix34 g_stereo_latest_pose{};
bool g_menu_anchor_valid = false;
runtime::VrMatrix34 g_menu_anchor{};
SRWLOCK g_menu_pointer_lock = SRWLOCK_INIT;
runtime::VrMatrix34 g_menu_pointer_anchor{};
float g_menu_pointer_aspect = 0;
float g_menu_pointer_distance = runtime::vr_setting_limits::kUiDistance.default_value;
float g_menu_pointer_width = 2.4F * runtime::vr_setting_limits::kUiScale.default_value;
std::atomic<float> g_menu_distance{runtime::vr_setting_limits::kUiDistance.default_value};
std::atomic<float> g_menu_scale{runtime::vr_setting_limits::kUiScale.default_value};
std::atomic<bool> g_recenter_requested{false};
SRWLOCK g_world_tracking_lock = SRWLOCK_INIT;
runtime::VrMatrix44 g_world_game_view;
runtime::VrMatrix34 g_world_anchor;
float g_world_movement_yaw = 0;
std::uint64_t g_world_tracking_time = 0;
void InvalidateWorldTracking() {
    InvalidateBlackPlagueShadowTracking();
    AcquireSRWLockExclusive(&g_world_tracking_lock);
    g_world_tracking_time = 0;
    ReleaseSRWLockExclusive(&g_world_tracking_lock);
}
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
        state == StereoMatrixState::processing ||
        state == StereoMatrixState::stopping;
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
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry.stereo_failed = true;
    strncpy_s(
        g_telemetry.stereo_error.data(),
        g_telemetry.stereo_error.size(),
        error.c_str(),
        _TRUNCATE);
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_stereo_state.store(StereoMatrixState::failed, std::memory_order_release);
}

class ActiveStereoFrame final {
public:
    ActiveStereoFrame() noexcept {
        g_stereo_frame_calls.fetch_add(1, std::memory_order_acq_rel);
    }
    ActiveStereoFrame(const ActiveStereoFrame&) = delete;
    ActiveStereoFrame& operator=(const ActiveStereoFrame&) = delete;
    ~ActiveStereoFrame() {
        g_stereo_frame_calls.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void RecordHmdVisibilityFailure(
    const std::string& error,
    bool camera_restored) noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.hmd_visibility_failures;
    g_telemetry.hmd_visibility_camera_restored = camera_restored;
    strncpy_s(
        g_telemetry.hmd_visibility_error.data(),
        g_telemetry.hmd_visibility_error.size(),
        error.c_str(),
        _TRUNCATE);
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

void __fastcall HookedUpdateRenderList(
    void* renderer,
    void*,
    void* world,
    void* camera,
    float frame_time) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<UpdateRenderList>(
        g_original_visibility_target.load(std::memory_order_acquire));
    if (original == nullptr) {
        return;
    }

    const auto visibility_active = []() noexcept {
        return g_stereo_persistent.load(std::memory_order_acquire) &&
            g_stereo_track_head_rotation &&
            g_stereo_tracking_anchor_valid &&
            g_stereo_latest_pose_valid &&
            g_stereo_state.load(std::memory_order_acquire) ==
                StereoMatrixState::processing;
    };
    if (!visibility_active()) {
        original(renderer, world, camera, frame_time);
        return;
    }

    ActiveStereoFrame active_stereo_frame;
    if (!visibility_active()) {
        original(renderer, world, camera, frame_time);
        return;
    }

    CameraMatrixSnapshot camera_snapshot;
    std::string error;
    if (!adapters::hpl1::CaptureCameraMatrices(
            camera, kCameraLayout, camera_snapshot, error) ||
        !LooksLikeMappedGameplayCamera(camera_snapshot)) {
        if (error.empty()) {
            error = "The visibility camera does not match the mapped layout";
        }
        RecordHmdVisibilityFailure(error, true);
        original(renderer, world, camera, frame_time);
        return;
    }

    runtime::VrMatrix44 tracked_head_view;
    if (!runtime::ComposeYawRecenteredTrackedHeadView(
            camera_snapshot.view,
            g_stereo_tracking_anchor,
            g_stereo_latest_pose,
            kPositionalWorldUnitsPerMeter,
            tracked_head_view,
            error)) {
        RecordHmdVisibilityFailure(
            "Could not compose the HMD visibility view: " + error, true);
        original(renderer, world, camera, frame_time);
        return;
    }

    runtime::VrCullFrustum cull_frustum;
    if (!runtime::BuildConservativeStereoCullFrustum(
            g_stereo_eyes,
            kVisibilityAngularGuardRadians,
            cull_frustum,
            error)) {
        RecordHmdVisibilityFailure(
            "Could not build the HMD visibility frustum: " + error, true);
        original(renderer, world, camera, frame_time);
        return;
    }

    CameraVisibilityOverride visibility_override(kCameraLayout);
    if (!visibility_override.Apply(
            camera,
            tracked_head_view,
            g_stereo_projections[0],
            cull_frustum.vertical_fov_radians,
            cull_frustum.aspect,
            error)) {
        RecordHmdVisibilityFailure(
            "Could not apply the HMD visibility camera: " + error, true);
        original(renderer, world, camera, frame_time);
        return;
    }

    original(renderer, world, camera, frame_time);

    std::string restore_error;
    if (!visibility_override.Restore(restore_error)) {
        const std::string failure =
            "Could not restore the camera after HMD visibility update: " +
            restore_error;
        RecordHmdVisibilityFailure(failure, false);
        FailStereoMatrixValidation(failure);
        return;
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.hmd_visibility_updates;
    g_telemetry.hmd_visibility_camera_restored = true;
    g_telemetry.hmd_visibility_error = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

[[nodiscard]] bool RenderStereoEye(
    RenderWorld original,
    void* renderer,
    void* world,
    void* camera,
    const runtime::VrMatrix44& head_view,
    std::size_t eye_index,
    float frame_time,
    bool& world_rendered,
    std::string& error) noexcept {
    world_rendered = false;
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

    CameraMatrixOverride camera_override(kCameraLayout);
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

    {
        hooks::ScopedEyeScissor scissor({
            binding.previous_viewport[2], binding.previous_viewport[3]});
        original(renderer, world, camera, frame_time);
        world_rendered = true;
        AcquireSRWLockExclusive(&g_telemetry_lock);
        g_telemetry.eye_scissor_remapped += scissor.remapped();
        g_telemetry.eye_scissor_bypassed += scissor.bypassed();
        ReleaseSRWLockExclusive(&g_telemetry_lock);
    }

    if (g_stereo_persistent.load(std::memory_order_acquire)) {
        const auto frame=ReadNativeControllerFrame();
        std::array<graphics::TrackedHandVisual,2> hands{};
        if (frame.focused) for (std::size_t i=0;i<hands.size();++i) {
            auto& hand=hands[i];
            std::array<float,3> velocity{},angular{};
            hand.visible=ControllerWorldPose(frame.hands[i].grip,hand.palm,velocity,angular);
            hand.ray=ControllerWorldPose(frame.hands[i].aim,hand.aim,velocity,angular) &&
                i==(frame.interact_source==runtime::VrHand::left ? 0U : 1U);
            if (frame.hands[i].skeleton_valid) hand.curl=frame.hands[i].finger_curl;
            else hand.curl.fill(frame.input.state.interact.pressed && hand.ray ? 0.8F : 0.1F);
        }
        // Hands are optional decoration: never interrupt the world/compositor
        // if the fixed-function compatibility path is unavailable.
        std::string hand_error;
        static_cast<void>(graphics::DrawTrackedHands(hands,eye_view,g_stereo_projections[eye_index],hand_error));
    }

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

[[nodiscard]] StereoProcessingResult ProcessControlledStereoMatrices(
    RenderWorld original,
    void* renderer,
    void* world,
    void* camera,
    float frame_time) noexcept {
    StereoProcessingResult result;
    // Let the game draw its UI over one desktop world pass, then present that
    // complete menu as a tracked panel at SwapBuffers (inventory/pause included).
    if (g_stereo_persistent.load(std::memory_order_acquire) && NativeInputUiActive()) {
        InvalidateWorldTracking();
        return result;
    }
    StereoMatrixState state = g_stereo_state.load(std::memory_order_acquire);
    if (state == StereoMatrixState::pending) {
        StereoMatrixState expected = StereoMatrixState::pending;
        if (!g_stereo_state.compare_exchange_strong(
                expected,
                StereoMatrixState::processing,
                std::memory_order_acq_rel)) {
            return result;
        }
        state = StereoMatrixState::processing;
    }
    if (state != StereoMatrixState::processing) {
        return result;
    }
    ActiveStereoFrame active_stereo_frame;
    if (g_stereo_state.load(std::memory_order_acquire) !=
        StereoMatrixState::processing) {
        return result;
    }
    if (g_stereo_cancel.load(std::memory_order_acquire)) {
        if (g_stereo_persistent.load(std::memory_order_acquire)) {
            return result;
        }
        FailStereoMatrixValidation("Controlled stereo-matrix validation was cancelled");
        return result;
    }
    if (original == nullptr) {
        FailStereoMatrixValidation("The original RenderWorld target is unavailable");
        return result;
    }

    runtime::OpenVrSession* session =
        g_stereo_session.load(std::memory_order_acquire);
    runtime::VrHmdPose pose;
    std::string error;
    if (session != nullptr) {
        if (!session->WaitForHmdPose(pose, error)) {
            FailStereoMatrixValidation(
                "Could not acquire the compositor frame pose: " + error);
            return result;
        }
        if (!pose.device_connected || !pose.pose_valid) {
            InvalidateWorldTracking();
            // SteamVR may start before the user puts on the headset. Persistent
            // presentation waits for tracking to return; bounded diagnostics fail.
            if (g_stereo_persistent.load(std::memory_order_acquire)) return result;
            FailStereoMatrixValidation(
                "The compositor frame did not contain a valid connected HMD pose");
            return result;
        }
    }

    CameraMatrixSnapshot camera_snapshot;
    if (!adapters::hpl1::CaptureCameraMatrices(
            camera, kCameraLayout, camera_snapshot, error)) {
        FailStereoMatrixValidation("Could not capture the HPL camera: " + error);
        return result;
    }
    if (!LooksLikeMappedGameplayCamera(camera_snapshot)) {
        FailStereoMatrixValidation(
            "The active camera does not match the mapped infinite-perspective layout");
        return result;
    }

    runtime::VrMatrix44 head_view = camera_snapshot.view;
    if (g_stereo_track_head_rotation) {
        if (g_recenter_requested.exchange(false, std::memory_order_acq_rel))
            g_stereo_tracking_anchor_valid = false;
        const bool shadow_recentered = !g_stereo_tracking_anchor_valid;
        if (!g_stereo_tracking_anchor_valid) {
            g_stereo_tracking_anchor = pose.device_to_absolute;
            g_stereo_tracking_anchor_valid = true;
        }
        if (!runtime::ComposeYawRecenteredTrackedHeadView(
                camera_snapshot.view,
                g_stereo_tracking_anchor,
                pose.device_to_absolute,
                kPositionalWorldUnitsPerMeter,
                head_view,
                error)) {
            FailStereoMatrixValidation(
                "Could not compose the yaw-recentered HMD rotation: " + error);
            return result;
        }
        g_stereo_latest_pose = pose.device_to_absolute;
        g_stereo_latest_pose_valid = true;
        AcquireSRWLockExclusive(&g_world_tracking_lock);
        g_world_game_view = camera_snapshot.view;
        g_world_anchor = g_stereo_tracking_anchor;
        // Express horizontal HMD forward in the native camera's horizontal
        // basis. Pitch/roll must not steer the walking direction.
        const float gx = -camera_snapshot.view.values[8], gz = -camera_snapshot.view.values[10];
        const float hx = -head_view.values[8], hz = -head_view.values[10];
        if (std::hypot(gx, gz) > 0.001F && std::hypot(hx, hz) > 0.001F)
            g_world_movement_yaw = std::atan2(-hx * gz + hz * gx, hx * gx + hz * gz);
        // The world camera currently uses rotation-only head tracking. Keep
        // hands relative to the current physical head, not its old recenter
        // position, so leaning cannot detach the palms from the rendered head.
        for (const auto index : {3U,7U,11U})
            g_world_anchor.values[index] = pose.device_to_absolute.values[index];
        g_world_tracking_time = GetTickCount64();
        ReleaseSRWLockExclusive(&g_world_tracking_lock);
        // Same yaw basis as ComposeYawRecenteredTrackedHeadView. Use the
        // original rotational anchor, not g_world_anchor whose translation
        // is deliberately overwritten for rotation-only hand rendering.
        const float ax = -g_stereo_tracking_anchor.values[2];
        const float az = -g_stereo_tracking_anchor.values[10];
        const float shadow_yaw = std::atan2(az * gx - ax * gz,
            ax * gx + az * gz);
        PublishBlackPlagueShadowTracking(pose.device_to_absolute,
            shadow_yaw, shadow_recentered);
    }

    // Read the snapshot sampled once by ButtonHandler::Update. Never consume
    // OpenVR button edges a second time from rendering or from another eye.
    if (session != nullptr && session->controller_input_initialized() &&
        g_stereo_persistent.load(std::memory_order_acquire)) {
        const auto controllers = ReadNativeControllerFrame();
        AcquireSRWLockExclusive(&g_telemetry_lock);
        ++g_telemetry.controller_samples;
        g_telemetry.controller_frame = controllers;
        g_telemetry.controller_error = {};
        ReleaseSRWLockExclusive(&g_telemetry_lock);
    }
    const bool persistent_stereo =
        g_stereo_persistent.load(std::memory_order_acquire);
    const runtime::StereoRenderPlan render_plan =
        runtime::PlanStereoWorldRendering(
            frame_time,
            persistent_stereo,
            false); // Mirror now copies an eye at swap, not a third native world pass.

    std::uint32_t completed_eye_passes = 0;
    for (std::size_t eye_index = 0; eye_index < g_stereo_eyes.size(); ++eye_index) {
        bool world_rendered = false;
        if (!RenderStereoEye(
                original,
                renderer,
                world,
                camera,
                head_view,
                eye_index,
                render_plan.eye_frame_times[eye_index],
                world_rendered,
                error)) {
            if (world_rendered && eye_index == 0 &&
                render_plan.frame_time_owned_by_first_eye) {
                result.frame_time_consumed_by_eye = true;
            }
            FailStereoMatrixValidation("Controlled stereo eye pass failed: " + error);
            return result;
        }
        if (eye_index == 0 && render_plan.frame_time_owned_by_first_eye) {
            result.frame_time_consumed_by_eye = true;
        }
        ++completed_eye_passes;
    }

    if (!adapters::hpl1::CameraMatchesSnapshot(
            camera, kCameraLayout, camera_snapshot)) {
        RecordStereoCameraRestorationFailure();
        FailStereoMatrixValidation(
            "The camera bytes changed after the controlled stereo pair");
        return result;
    }

    if (session != nullptr) {
        std::array<std::uint32_t, 2> color_textures{};
        if (!GetPersistentEyeColorTextures(color_textures, error)) {
            FailStereoMatrixValidation(
                "Could not obtain the rendered eye textures: " + error);
            return result;
        }
        if (!session->SubmitOpenGlEyeTextures(color_textures, error)) {
            FailStereoMatrixValidation(
                "Could not submit the rendered stereo pair: " + error);
            return result;
        }
        glFlush();
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.stereo_frames;
    g_telemetry.stereo_eye_passes += completed_eye_passes;
    if (session != nullptr) {
        ++g_telemetry.compositor_submitted_frames;
        g_telemetry.compositor_hmd_pose_valid = true;
    }
    if (g_stereo_track_head_rotation) {
        ++g_telemetry.tracked_head_frames;
        g_telemetry.tracking_anchor_captured = g_stereo_tracking_anchor_valid;
        g_telemetry.hmd_tracking_anchor_m = {
            g_stereo_tracking_anchor.values[3],
            g_stereo_tracking_anchor.values[7],
            g_stereo_tracking_anchor.values[11],
        };
        g_telemetry.hmd_tracking_position_m = {
            pose.device_to_absolute.values[3],
            pose.device_to_absolute.values[7],
            pose.device_to_absolute.values[11],
        };
        g_telemetry.hmd_horizontal_delta_m = std::hypot(
            pose.device_to_absolute.values[3] -
                g_stereo_tracking_anchor.values[3],
            pose.device_to_absolute.values[11] -
                g_stereo_tracking_anchor.values[11]);
        g_telemetry.positional_world_units_per_meter =
            kPositionalWorldUnitsPerMeter;
    }
    g_telemetry.stereo_camera_restored = true;
    g_telemetry.persistent_stereo_active = persistent_stereo;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    const std::uint32_t completed =
        g_stereo_completed_frames.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (!persistent_stereo &&
        completed >= g_stereo_requested_frames.load(std::memory_order_acquire)) {
        g_stereo_state.store(StereoMatrixState::passed, std::memory_order_release);
    }
    result.completed = true;
    result.suppress_original_world = !render_plan.render_monitor_world;
    return result;
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
        const StereoProcessingResult stereo = ProcessControlledStereoMatrices(
            original, renderer, world, camera, frame_time);
        ProcessControlledWorldDuplication(
            original, renderer, world, camera);
        if (stereo.completed && stereo.suppress_original_world) {
            AcquireSRWLockExclusive(&g_telemetry_lock);
            ++g_telemetry.suppressed_monitor_world_passes;
            ++g_telemetry.eye_owned_frame_time_frames;
            ReleaseSRWLockExclusive(&g_telemetry_lock);
        } else {
            const float original_frame_time =
                stereo.frame_time_consumed_by_eye ? 0.0F : frame_time;
            original(renderer, world, camera, original_frame_time);
            AcquireSRWLockExclusive(&g_telemetry_lock);
            ++g_telemetry.monitor_world_passes;
            if (stereo.frame_time_consumed_by_eye) {
                ++g_telemetry.eye_owned_frame_time_frames;
            }
            ReleaseSRWLockExclusive(&g_telemetry_lock);
        }
    }
}

[[nodiscard]] void* DecodeExpectedTarget(
    std::uint8_t* instruction,
    const std::array<std::uint8_t, 5>& expected_call) noexcept {
    std::int32_t displacement = 0;
    std::memcpy(
        &displacement, expected_call.data() + 1, sizeof(displacement));
    const std::intptr_t next_instruction =
        reinterpret_cast<std::intptr_t>(instruction + expected_call.size());
    return reinterpret_cast<void*>(next_instruction + displacement);
}

} // namespace

bool InstallRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (g_hook.installed() || g_visibility_hook.installed()) {
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
    if (DecodeExpectedTarget(call_site, kExpectedRenderWorldCall) !=
        expected_target) {
        error = "The manifest call displacement does not target RenderWorld";
        return false;
    }
    std::uint8_t* visibility_call_site =
        image + kUpdateRenderListCallSiteRva;
    void* expected_visibility_target = image + kUpdateRenderListRva;
    if (DecodeExpectedTarget(
            visibility_call_site, kExpectedUpdateRenderListCall) !=
        expected_visibility_target) {
        error =
            "The manifest call displacement does not target UpdateRenderList";
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
    g_stereo_session.store(nullptr, std::memory_order_release);
    g_stereo_track_head_rotation = false;
    g_stereo_tracking_anchor_valid = false;
    g_stereo_tracking_anchor = {};
    g_stereo_latest_pose_valid = false;
    g_stereo_latest_pose = {};
    g_stereo_requested_frames.store(0, std::memory_order_release);
    g_stereo_completed_frames.store(0, std::memory_order_release);
    g_stereo_cancel.store(false, std::memory_order_release);
    g_stereo_persistent.store(false, std::memory_order_release);
    g_stereo_monitor_mirror.store(false, std::memory_order_release);
    g_stereo_frame_calls.store(0, std::memory_order_release);
    g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
    ResetEyeTargetProbe();
    g_original_target.store(expected_target, std::memory_order_release);
    g_original_visibility_target.store(
        expected_visibility_target, std::memory_order_release);

    if (!hooks::InstallRel32CallHook(
            call_site,
            kExpectedRenderWorldCall,
            reinterpret_cast<void*>(&HookedRenderWorld),
            g_hook,
            error)) {
        g_original_target.store(nullptr, std::memory_order_release);
        g_original_visibility_target.store(nullptr, std::memory_order_release);
        return false;
    }
    if (!hooks::InstallRel32CallHook(
            visibility_call_site,
            kExpectedUpdateRenderListCall,
            reinterpret_cast<void*>(&HookedUpdateRenderList),
            g_visibility_hook,
            error)) {
        std::string rollback_error;
        if (!hooks::RemoveRel32CallHook(g_hook, rollback_error) &&
            !rollback_error.empty()) {
            error += "; RenderWorld hook rollback also failed: " + rollback_error;
        }
        g_original_target.store(nullptr, std::memory_order_release);
        g_original_visibility_target.store(nullptr, std::memory_order_release);
        return false;
    }
    if (!hooks::InstallOpenGlEyeScissor(error)) {
        std::string rollback_error;
        if (!RemoveRenderWorldProbe(rollback_error)) {
            error += "; render hook rollback also failed: " + rollback_error;
        }
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
    std::string visibility_error;
    const bool visibility_removed =
        hooks::RemoveRel32CallHook(g_visibility_hook, visibility_error);
    std::string render_error;
    const bool render_removed = hooks::RemoveRel32CallHook(g_hook, render_error);
    if (!visibility_removed || !render_removed) {
        error = !visibility_removed
            ? "Could not remove the UpdateRenderList hook: " + visibility_error
            : "Could not remove the RenderWorld hook: " + render_error;
        if (!visibility_removed && !render_removed) {
            error += "; RenderWorld hook removal also failed: " + render_error;
        }
        return false;
    }

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            g_original_target.store(nullptr, std::memory_order_release);
            g_original_visibility_target.store(nullptr, std::memory_order_release);
            return hooks::RemoveOpenGlEyeScissor(error);
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
    do {
        state = g_duplication_state.load(std::memory_order_acquire);
        if (state != DuplicationState::preparing &&
            state != DuplicationState::pending &&
            state != DuplicationState::processing) {
            break;
        }
        Sleep(1);
    } while (true);
    g_duplication_state.store(DuplicationState::idle, std::memory_order_release);
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
    g_stereo_persistent.store(false, std::memory_order_release);
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
    do {
        state = g_stereo_state.load(std::memory_order_acquire);
        if (state != StereoMatrixState::preparing &&
            state != StereoMatrixState::pending &&
            state != StereoMatrixState::processing) {
            break;
        }
        Sleep(1);
    } while (true);
    g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
    error = "Timed out waiting for controlled stereo-matrix validation";
    return false;
}

bool ValidateControlledStereoSubmission(
    runtime::OpenVrSession& session,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::uint32_t frames,
    std::string& error) noexcept {
    error.clear();
    if (!session.initialized()) {
        error = "OpenVR must be initialized before compositor submission";
        return false;
    }

    runtime::OpenVrSession* expected = nullptr;
    if (!g_stereo_session.compare_exchange_strong(
            expected, &session, std::memory_order_acq_rel)) {
        error = "Another compositor-submission request is active";
        return false;
    }

    const bool result = ValidateControlledStereoMatrices(
        eyes, near_clip, frames, error);
    g_stereo_session.store(nullptr, std::memory_order_release);
    return result;
}

bool ValidateControlledTrackedStereoSubmission(
    runtime::OpenVrSession& session,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::uint32_t frames,
    std::string& error) noexcept {
    error.clear();
    if (!session.initialized()) {
        error = "OpenVR must be initialized before tracked compositor submission";
        return false;
    }

    runtime::OpenVrSession* expected = nullptr;
    if (!g_stereo_session.compare_exchange_strong(
            expected, &session, std::memory_order_acq_rel)) {
        error = "Another compositor-submission request is active";
        return false;
    }

    g_stereo_track_head_rotation = true;
    g_stereo_tracking_anchor_valid = false;
    g_stereo_tracking_anchor = {};
    g_stereo_latest_pose_valid = false;
    g_stereo_latest_pose = {};
    const bool result = ValidateControlledStereoMatrices(
        eyes, near_clip, frames, error);
    g_stereo_track_head_rotation = false;
    g_stereo_tracking_anchor_valid = false;
    g_stereo_tracking_anchor = {};
    g_stereo_latest_pose_valid = false;
    g_stereo_latest_pose = {};
    g_stereo_session.store(nullptr, std::memory_order_release);
    return result;
}

bool StartTrackedStereoPresentation(
    runtime::OpenVrSession& session,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::string& error) noexcept {
    error.clear();
    if (!session.initialized()) {
        error = "OpenVR must be initialized before tracked presentation";
        return false;
    }
    if (!PersistentEyeTargetsActive()) {
        error = "Persistent eye targets must be active before tracked presentation";
        return false;
    }
    if (DuplicationActive()) {
        error = "A controlled world-duplication request is active";
        return false;
    }

    runtime::OpenVrSession* expected_session = nullptr;
    if (!g_stereo_session.compare_exchange_strong(
            expected_session, &session, std::memory_order_acq_rel)) {
        error = "Another compositor-submission request is active";
        return false;
    }

    StereoMatrixState state = g_stereo_state.load(std::memory_order_acquire);
    if (state == StereoMatrixState::passed || state == StereoMatrixState::failed) {
        static_cast<void>(g_stereo_state.compare_exchange_strong(
            state, StereoMatrixState::idle, std::memory_order_acq_rel));
    }
    StereoMatrixState expected_state = StereoMatrixState::idle;
    if (!g_stereo_state.compare_exchange_strong(
            expected_state,
            StereoMatrixState::preparing,
            std::memory_order_acq_rel)) {
        g_stereo_session.store(nullptr, std::memory_order_release);
        error = "Another controlled stereo request is active";
        return false;
    }

    std::array<runtime::VrMatrix44, 2> projections{};
    for (std::size_t eye_index = 0; eye_index < eyes.size(); ++eye_index) {
        if (!runtime::BuildHplInfiniteProjection(
                eyes[eye_index], near_clip, projections[eye_index], error)) {
            g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
            g_stereo_session.store(nullptr, std::memory_order_release);
            return false;
        }
    }

    g_stereo_error = {};
    g_stereo_eyes = eyes;
    g_stereo_projections = projections;
    g_stereo_requested_frames.store(
        std::numeric_limits<std::uint32_t>::max(), std::memory_order_release);
    g_stereo_completed_frames.store(0, std::memory_order_release);
    g_stereo_cancel.store(false, std::memory_order_release);
    g_stereo_track_head_rotation = true;
    g_stereo_tracking_anchor_valid = false;
    g_stereo_tracking_anchor = {};
    g_stereo_latest_pose_valid = false;
    g_stereo_latest_pose = {};
    g_stereo_persistent.store(true, std::memory_order_release);
    g_menu_anchor_valid = false;
    g_stereo_state.store(StereoMatrixState::pending, std::memory_order_release);
    return true;
}

bool StopTrackedStereoPresentation(std::string& error) noexcept {
    error.clear();
    InvalidateWorldTracking();
    if (!g_stereo_persistent.load(std::memory_order_acquire)) {
        return true;
    }

    g_stereo_cancel.store(true, std::memory_order_release);
    g_stereo_state.store(StereoMatrixState::stopping, std::memory_order_release);

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_stereo_frame_calls.load(std::memory_order_acquire) == 0) {
            g_stereo_session.store(nullptr, std::memory_order_release);
            g_stereo_track_head_rotation = false;
            g_stereo_tracking_anchor_valid = false;
            g_stereo_tracking_anchor = {};
            g_stereo_latest_pose_valid = false;
            g_stereo_latest_pose = {};
            g_stereo_persistent.store(false, std::memory_order_release);
            g_stereo_cancel.store(false, std::memory_order_release);
            g_stereo_state.store(StereoMatrixState::idle, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }

    error = "Timed out waiting for a tracked stereo frame to finish";
    return false;
}

bool TrackedStereoPresentationActive() noexcept {
    return g_stereo_persistent.load(std::memory_order_acquire) &&
        (g_stereo_state.load(std::memory_order_acquire) ==
             StereoMatrixState::pending ||
         g_stereo_state.load(std::memory_order_acquire) ==
             StereoMatrixState::processing);
}

void ConfigureTrackedPresentation(const runtime::VrSettings& source) noexcept {
    runtime::VrSettings settings = source;
    runtime::NormalizeVrSettings(settings);
    g_menu_distance.store(settings.ui_distance, std::memory_order_release);
    g_menu_scale.store(settings.ui_scale, std::memory_order_release);
}

void PresentTrackedMenuOnRenderThread(bool world_rendered) noexcept {
    if (!TrackedStereoPresentationActive()) return;
    ActiveStereoFrame active;
    if (!TrackedStereoPresentationActive() || g_stereo_cancel.load(std::memory_order_acquire)) return;
    if (world_rendered) {
        if (TrackedStereoMonitorMirrorEnabled()) {
            std::array<std::uint32_t,2> textures{}; std::string error;
            if (!GetPersistentEyeColorTextures(textures,error) ||
                !graphics::DrawMonitorMirror(textures[0],error)) {
                AcquireSRWLockExclusive(&g_telemetry_lock);
                strncpy_s(g_telemetry.stereo_error.data(),g_telemetry.stereo_error.size(),error.c_str(),_TRUNCATE);
                ReleaseSRWLockExclusive(&g_telemetry_lock);
            }
        }
        g_menu_anchor_valid = runtime::PlanStablePanelAnchor(
            false, false, g_menu_anchor_valid).anchor_valid_after;
        AcquireSRWLockExclusive(&g_menu_pointer_lock);
        g_menu_pointer_aspect = 0;
        ReleaseSRWLockExclusive(&g_menu_pointer_lock);
        return;
    }
    InvalidateWorldTracking();
    auto* session = g_stereo_session.load(std::memory_order_acquire);
    if (!session) return;
    runtime::VrHmdPose pose;
    std::string error;
    if (!session->WaitForHmdPose(pose, error) || !pose.device_connected || !pose.pose_valid) return;
    const bool recenter_requested =
        g_recenter_requested.exchange(false, std::memory_order_acq_rel);
    const auto anchor_plan = runtime::PlanStablePanelAnchor(
        true, recenter_requested, g_menu_anchor_valid);
    if (anchor_plan.capture_current_pose) {
        g_menu_anchor = pose.device_to_absolute;
    }
    g_menu_anchor_valid = anchor_plan.anchor_valid_after;
    const float menu_distance = g_menu_distance.load(std::memory_order_acquire);
    const float menu_scale = g_menu_scale.load(std::memory_order_acquire);
    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    AcquireSRWLockExclusive(&g_menu_pointer_lock);
    g_menu_pointer_anchor = g_menu_anchor;
    g_menu_pointer_aspect = viewport[3] > 0 ? static_cast<float>(viewport[2]) / viewport[3] : 0;
    g_menu_pointer_distance = menu_distance;
    g_menu_pointer_width = 2.4F * menu_scale;
    ReleaseSRWLockExclusive(&g_menu_pointer_lock);
    runtime::VrMatrix44 head_view;
    bool success = runtime::ComposeYawRecenteredTrackedHeadView(runtime::IdentityMatrix(),
        g_menu_anchor, pose.device_to_absolute, 1.0F, head_view, error);
    graphics::OpenGlMenuFrame menu;
    if (success) success = menu.Capture(error);
    for (std::size_t i = 0; success && i < 2; ++i) {
        runtime::VrMatrix44 view;
        success = runtime::ComposeEyeViewFromHeadView(head_view, g_stereo_eyes[i].eye_to_head, view, error);
        graphics::OpenGlEyeBinding binding;
        if (success) success = BeginPersistentEyeTarget(i == 0 ? graphics::Eye::left : graphics::Eye::right,
                                                       binding, error);
        if (success) success = menu.Draw(
            view, g_stereo_projections[i], menu_distance, menu_scale, error);
        if (binding.active) {
            std::string restore_error;
            if (!EndPersistentEyeTarget(binding, restore_error)) { success = false; error += restore_error; }
        }
    }
    std::array<std::uint32_t, 2> textures{};
    if (success) success = GetPersistentEyeColorTextures(textures, error) &&
                           session->SubmitOpenGlEyeTextures(textures, error);
    AcquireSRWLockExclusive(&g_telemetry_lock);
    if (success) {
        ++g_telemetry.menu_frames;
        ++g_telemetry.compositor_submitted_frames;
        g_telemetry.compositor_hmd_pose_valid = true;
    } else {
        g_telemetry.stereo_failed = true;
        strncpy_s(g_telemetry.stereo_error.data(), g_telemetry.stereo_error.size(), error.c_str(), _TRUNCATE);
    }
    ReleaseSRWLockExclusive(&g_telemetry_lock);
}

bool TrackedMenuPointer(const runtime::VrHmdPose& aim, std::array<float, 2>& uv) noexcept {
    uv = {};
    if (!aim.pose_valid || !aim.device_connected) return false;
    AcquireSRWLockShared(&g_menu_pointer_lock);
    const auto anchor = g_menu_pointer_anchor;
    const float aspect = g_menu_pointer_aspect;
    const float distance = g_menu_pointer_distance;
    const float width = g_menu_pointer_width;
    ReleaseSRWLockShared(&g_menu_pointer_lock);
    return runtime::ProjectAimOnMenu(
        anchor, aim.device_to_absolute, aspect, distance, width, uv);
}
void RequestTrackedRecenter() noexcept { g_recenter_requested.store(true, std::memory_order_release); }

bool TrackedMovementYaw(float& yaw) noexcept {
    AcquireSRWLockShared(&g_world_tracking_lock);
    yaw = g_world_movement_yaw;
    const auto time = g_world_tracking_time;
    ReleaseSRWLockShared(&g_world_tracking_lock);
    return time != 0 && GetTickCount64() - time <= 250 && std::isfinite(yaw);
}
bool ControllerWorldPose(const runtime::VrHmdPose& controller, runtime::VrMatrix44& pose,
    std::array<float,3>& velocity, std::array<float,3>& angular) noexcept {
    pose = {}; velocity = {}; angular = {};
    if (!TrackedStereoPresentationActive() || NativeInputUiActive()) return false;
    AcquireSRWLockShared(&g_world_tracking_lock);
    const auto view = g_world_game_view;
    const auto anchor = g_world_anchor;
    const auto time = g_world_tracking_time;
    ReleaseSRWLockShared(&g_world_tracking_lock);
    if (!time || GetTickCount64() - time > 250) return false;
    std::string error;
    return runtime::ControllerPoseInGame(view, anchor, controller, pose, velocity, angular, error);
}

void SetTrackedStereoMonitorMirror(bool enabled) noexcept {
    g_stereo_monitor_mirror.store(enabled, std::memory_order_release);
}

bool TrackedStereoMonitorMirrorEnabled() noexcept {
    return g_stereo_monitor_mirror.load(std::memory_order_acquire);
}

RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    RenderWorldFrameTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    result.eye_targets = ConsumeEyeTargetProbeTelemetry();
    result.persistent_stereo_active = TrackedStereoPresentationActive();
    result.monitor_mirror_enabled = TrackedStereoMonitorMirrorEnabled();
    result.stereo_lifetime_frames =
        g_stereo_completed_frames.load(std::memory_order_acquire);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
