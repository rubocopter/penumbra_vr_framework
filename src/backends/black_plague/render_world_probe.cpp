#include "render_world_probe.hpp"
#include "black_plague_body_adapter.hpp"
#include "hand_contact_probe.hpp"

#include "camera_matrix_override.hpp"
#include "opengl_eye_scissor.hpp"
#include "opengl_menu_frame.hpp"
#include "opengl_tracked_hands.hpp"
#include "native_input_bridge.hpp"
#include "particle_stereo_refresh.hpp"
#include "presentation_timing.hpp"
#include "spatial_interaction.hpp"
#include "spawn_yaw_rebase.hpp"
#include "telemetry_try_lock.hpp"
#include "ui_presentation.hpp"
#include "vr_grab_pose.hpp"
#include "rel32_call_hook.hpp"
#include "stereo_render_policy.hpp"
#include "vr_math.hpp"
#include "vr_panel_policy.hpp"
#include "vr_play_mode_policy.hpp"
#include "vr_tracking_space.hpp"
#include "iat_hook.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace penumbra_vr::backends::black_plague {
namespace {

using PerformanceClock = std::chrono::steady_clock;

[[nodiscard]] std::uint64_t ElapsedNanoseconds(
    const PerformanceClock::time_point& start) noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            PerformanceClock::now() - start).count());
}

constexpr std::uintptr_t kRenderWorldRva = 0x0012CB10;
constexpr std::uintptr_t kRenderWorldCallSiteRva = 0x000EE010;
constexpr std::uintptr_t kUpdateRenderListRva = 0x0012A8F0;
constexpr std::uintptr_t kUpdateRenderListCallSiteRva = 0x000EDF84;
constexpr std::uintptr_t kUpdaterPostSceneDrawRva = 0x000E1C40;
constexpr std::uintptr_t kUpdaterPostSceneDrawCallSiteRva = 0x000EE01B;
constexpr std::uintptr_t kGraphicsGetDrawerRva = 0x000DF730;
constexpr std::uintptr_t kGraphicsGetDrawerCallSiteRva = 0x000EE03B;
constexpr std::uintptr_t kGraphicsDrawerDrawAllRva = 0x000F3920;
constexpr std::uintptr_t kGraphicsDrawerDrawAllCallSiteRva = 0x000EE042;
constexpr std::uintptr_t kMapLoadSetStartPosCallSiteRva = 0x000916EC;
constexpr std::uintptr_t kPlayerSetStartPosRva = 0x0009DFC0;
constexpr std::uintptr_t kSpawnCameraSetYawCallSiteRva = 0x0009E1C0;
constexpr std::uintptr_t kCameraSetYawRva = 0x001139F0;
constexpr std::uintptr_t kCameraSetPositionRva = 0x00113BB0;
constexpr std::uintptr_t kCameraYawOffset = 0x24;
constexpr std::uintptr_t kParticleGetModelMatrixSlotRva = 0x002875B8;
constexpr std::uintptr_t kParticleUpdateGraphicsSlotRva = 0x002875C0;
constexpr std::uintptr_t kParticleGetModelMatrixRva = 0x00169C50;
constexpr std::uintptr_t kParticleUpdateGraphicsRva = 0x00169FF0;
constexpr GLenum kGlFramebufferBinding = 0x8CA6;
constexpr GLenum kGlMaxRenderbufferSize = 0x84E8;
constexpr std::array<std::uint8_t, 5> kExpectedRenderWorldCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};
constexpr std::array<std::uint8_t, 5> kExpectedUpdateRenderListCall{
    0xE8, 0x67, 0xC9, 0x03, 0x00,
};
constexpr std::array<std::uint8_t, 5> kExpectedUpdaterPostSceneDrawCall{
    0xE8, 0x20, 0x3C, 0xFF, 0xFF,
};
constexpr std::array<std::uint8_t, 5> kExpectedGraphicsGetDrawerCall{
    0xE8, 0xF0, 0x16, 0xFF, 0xFF,
};
constexpr std::array<std::uint8_t, 5> kExpectedGraphicsDrawerDrawAllCall{
    0xE8, 0xD9, 0x58, 0x00, 0x00,
};
constexpr std::array<std::uint8_t, 5> kExpectedMapLoadSetStartPosCall{
    0xE8, 0xCF, 0xC8, 0x00, 0x00,
};
constexpr std::array<std::uint8_t, 5> kExpectedSpawnCameraSetYawCall{
    0xE8, 0x2B, 0x58, 0x07, 0x00,
};
constexpr std::array<std::uint8_t, 3> kExpectedCameraYawStore{
    0x89, 0x41, 0x24,
};
constexpr std::array<std::uint8_t, 24> kExpectedCameraPositionStore{
    0x8B, 0x54, 0x24, 0x04, 0x56, 0x8B, 0x32, 0x8D,
    0x41, 0x04, 0x89, 0x30, 0x8B, 0x72, 0x04, 0x89,
    0x70, 0x04, 0x8B, 0x52, 0x08, 0x89, 0x50, 0x08,
};
constexpr float kVisibilityAngularGuardRadians = 0.087266463F; // 5 degrees.
constexpr float kRotationOnlyTranslationScale = 0.0F;
constexpr float kNativeYawRebaseEpsilonRadians = 0.0001F;
std::atomic<float> g_tracking_height_offset{
    runtime::vr_setting_limits::kHeightOffset.default_value};
std::atomic<float> g_tracking_crouch_depth{
    runtime::vr_setting_limits::kPhysicalCrouchDepth.default_value};
std::atomic<runtime::VrPlayMode> g_tracking_play_mode{
    runtime::VrPlayMode::standing};
std::atomic<float> g_tracking_player_height{
    runtime::vr_setting_limits::kPlayerHeight.default_value};
SRWLOCK g_play_mode_lock = SRWLOCK_INIT;
runtime::VrPlayModePolicy g_play_mode_policy;

using RenderWorld = void(__thiscall*)(void* renderer, void* world, void* camera, float frame_time);
using UpdateRenderList = void(__thiscall*)(
    void* renderer,
    void* world,
    void* camera,
    float frame_time);
using GraphicsDrawerDrawAll = void(__thiscall*)(void* drawer);
using CameraSetYaw = void(__thiscall*)(void* camera, float yaw);
using ParticleGetModelMatrix = void*(__thiscall*)(void* emitter, void* camera);

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

[[nodiscard]] runtime::VrMatrix34 CollapseMatrix(
    const runtime::VrMatrix44& matrix) noexcept {
    runtime::VrMatrix34 result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            result.values[row * 4U + column] =
                matrix.values[row * 4U + column];
        }
    }
    return result;
}

[[nodiscard]] float TrackingWorldYaw(
    const runtime::VrMatrix44& game_head_view,
    const runtime::VrMatrix34& tracking_anchor) noexcept {
    // For a rigid view, these entries are the world-pose forward X/Z after
    // transposition by the inverse.  This is the same yaw alignment used by
    // ComposeYawRecenteredTrackedHeadView.
    const float game_forward_x = -game_head_view.values[8];
    const float game_forward_z = -game_head_view.values[10];
    const float anchor_forward_x = -tracking_anchor.values[2];
    const float anchor_forward_z = -tracking_anchor.values[10];
    return std::atan2(
        anchor_forward_z * game_forward_x -
            anchor_forward_x * game_forward_z,
        anchor_forward_x * game_forward_x +
            anchor_forward_z * game_forward_z);
}

[[nodiscard]] bool ResolveBlackPlagueRoomScalePlacement(
    const runtime::VrMatrix44& game_head_view,
    const runtime::VrMatrix34& tracking_anchor,
    const runtime::VrMatrix34& current_tracking_pose,
    const runtime::VrTrackingSampleIdentity& current_identity,
    const BlackPlagueRoomScaleCameraSample& room_scale,
    bool& placement_available,
    std::array<float, 3>& world_translation,
    std::array<float, 3>& render_prediction,
    std::array<float, 3>& render_head_anchor,
    std::string& error) noexcept {
    world_translation = {};
    render_prediction = {};
    render_head_anchor = {};
    placement_available = false;
    if (!room_scale.enabled || !room_scale.valid) return true;
    if (room_scale.tracking_identity.sequence != 0 &&
        !runtime::SameTrackingEpoch(
            room_scale.tracking_identity, current_identity)) {
        return true;
    }

    const float tracking_delta_x = current_tracking_pose.values[3] -
        room_scale.observed_tracking_pose.values[3];
    const float tracking_delta_z = current_tracking_pose.values[11] -
        room_scale.observed_tracking_pose.values[11];
    const float tracking_delta_length =
        std::hypot(tracking_delta_x, tracking_delta_z);
    if (!std::isfinite(tracking_delta_length) || tracking_delta_length >
            runtime::vr_locomotion_policy::kMaximumHeadBodySeparation) {
        // A recenter/tracking discontinuity must not be extrapolated into the
        // world before the body owner has rebased the reconciliation sample.
        return true;
    }

    // Black Plague's character body advances at ~60 Hz while the HMD can be
    // sampled faster. PID 14212 showed that withholding all between-tick X/Z
    // made rejected wall pressure feel like a force pulling the head back.
    // Restore the previously headset-exercised continuation and remove only
    // the component that continues into the last rejected physical direction.
    // Tangential slide and retreat remain render-rate responsive.
    const float world_yaw = TrackingWorldYaw(game_head_view, tracking_anchor);
    const float cosine = std::cos(world_yaw);
    const float sine = std::sin(world_yaw);
    render_prediction = {
        cosine * tracking_delta_x + sine * tracking_delta_z,
        0.0F,
        -sine * tracking_delta_x + cosine * tracking_delta_z,
    };
    render_prediction = runtime::FilterPhysicalRenderPrediction(
        render_prediction, room_scale.physical_reconciliation);
    render_head_anchor = room_scale.predicted_head_anchor;
    render_head_anchor[0] += render_prediction[0];
    render_head_anchor[2] += render_prediction[2];

    // Rework's player-world pose is the reconciled feet anchor. Its shared
    // tracking transform supplies the physical HMD height continuously and
    // zeroes raw horizontal translation because X/Z is already integrated in
    // render_head_anchor. This also prevents Black Plague's full native
    // crouch-camera drop from being added to a real physical crouch.
    runtime::VrTrackingSpace tracking_space;
    tracking_space.SetHeadTrackingPose(current_tracking_pose);
    tracking_space.SetPlayerWorldPosition(render_head_anchor);
    tracking_space.SetHeightCalibration(
        g_tracking_height_offset.load(std::memory_order_acquire));
    AcquireSRWLockExclusive(&g_play_mode_lock);
    const auto play_mode = g_play_mode_policy.Update(
        g_tracking_play_mode.load(std::memory_order_acquire),
        current_tracking_pose.values[7],
        g_tracking_player_height.load(std::memory_order_acquire));
    ReleaseSRWLockExclusive(&g_play_mode_lock);
    tracking_space.SetSeatedOffset(play_mode.seated_offset);
    const auto crouch = ReadNativePhysicalCrouchStatus();
    if (crouch.native_crouched && !crouch.policy.physical_crouch) {
        tracking_space.SetPostureOffset(
            -g_tracking_crouch_depth.load(std::memory_order_acquire));
    }
    runtime::VrMatrix44 tracked_head_world;
    if (!tracking_space.HeadWorldPose(tracked_head_world, error)) {
        error = "Could not compose Rework tracking height for Black Plague: " +
            error;
        return false;
    }
    render_head_anchor[1] = tracked_head_world.values[7];

    runtime::VrMatrix44 game_head_pose;
    if (!runtime::InvertRigidTransform(
            CollapseMatrix(game_head_view), game_head_pose, error)) {
        error = "The native Black Plague camera view is not rigid: " + error;
        return false;
    }
    world_translation = {
        render_head_anchor[0] - game_head_pose.values[3],
        render_head_anchor[1] - game_head_pose.values[7],
        render_head_anchor[2] - game_head_pose.values[11],
    };
    if (!std::isfinite(world_translation[0]) ||
        !std::isfinite(world_translation[1]) ||
        !std::isfinite(world_translation[2])) {
        error = "The predicted Black Plague room-scale placement is non-finite";
        return false;
    }
    placement_available = true;
    return true;
}

[[nodiscard]] bool ComposeBlackPlagueTrackedHeadView(
    const runtime::VrMatrix44& game_head_view,
    const runtime::VrMatrix34& anchor,
    const runtime::VrMatrix34& current,
    const runtime::VrTrackingSampleIdentity& current_identity,
    const BlackPlagueRoomScaleCameraSample& room_scale,
    runtime::VrMatrix44& tracked_head_view,
    bool& positional_translation_applied,
    std::array<float, 3>& world_translation,
    std::array<float, 3>& render_prediction,
    std::array<float, 3>& render_head_anchor,
    std::string& error) noexcept {
    positional_translation_applied = false;
    world_translation = {};
    render_prediction = {};
    render_head_anchor = {};
    if (!runtime::ComposeYawRecenteredTrackedHeadView(
            game_head_view, anchor, current, kRotationOnlyTranslationScale,
            tracked_head_view, error)) {
        return false;
    }
    if (!room_scale.enabled || !room_scale.valid) return true;
    bool placement_available = false;
    if (!ResolveBlackPlagueRoomScalePlacement(game_head_view, anchor, current,
            current_identity,
            room_scale, placement_available, world_translation, render_prediction,
            render_head_anchor, error)) {
        return false;
    }
    if (!placement_available) return true;
    runtime::VrMatrix44 translated;
    if (!runtime::ApplyWorldTranslationToView(
            tracked_head_view, world_translation,
            translated, error)) {
        return false;
    }
    tracked_head_view = translated;
    positional_translation_applied = true;
    return true;
}

hooks::Rel32CallHook g_hook;
hooks::Rel32CallHook g_visibility_hook;
hooks::Rel32CallHook g_draw_all_hook;
hooks::Rel32CallHook g_spawn_yaw_hook;
hooks::IatHook g_particle_get_model_matrix_hook;
hooks::IatHook g_particle_update_graphics_hook;
std::atomic<void*> g_original_target{nullptr};
std::atomic<void*> g_original_visibility_target{nullptr};
std::atomic<void*> g_original_draw_all_target{nullptr};
std::atomic<void*> g_original_spawn_yaw_target{nullptr};
std::atomic<void*> g_original_particle_get_model_matrix{nullptr};
std::atomic<void*> g_original_particle_update_graphics{nullptr};
ParticleStereoRefreshState g_particle_stereo_refresh;
std::atomic<std::uint64_t> g_particle_update_calls{0};
std::atomic<std::uint64_t> g_particle_eye_refreshes{0};
std::atomic<std::uint64_t> g_particle_refresh_misses{0};
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
BlackPlagueRoomScaleCameraSample g_stereo_room_scale_sample{};
struct PendingGameplayOverlaySubmission {
    bool active = false;
    runtime::OpenVrSession* session = nullptr;
    runtime::VrMatrix44 head_view{};
    NativeUiSurface ui_surface = NativeUiSurface::fullscreen;
    runtime::VrMatrix44 ui_panel_world_pose{};
    std::uint64_t presentation_sequence = 0;
    std::uint64_t presentation_timestamp_ms = 0;
    bool presentation_from_visibility = false;
};
PendingGameplayOverlaySubmission g_pending_gameplay_overlay_submission;
struct PresentationSnapshot {
    bool valid = false;
    runtime::VrHmdPose pose{};
    BlackPlagueRoomScaleCameraSample room_scale{};
    runtime::VrMatrix34 effective_tracking_anchor{};
};
SRWLOCK g_presentation_lock = SRWLOCK_INIT;
PresentationSnapshot g_presentation_snapshot;
std::atomic<std::uint64_t> g_presentation_sequence{0};
std::atomic<std::uint64_t> g_last_submitted_presentation_sequence{0};
std::atomic<std::uint64_t> g_presentation_pose_epoch{1};
std::atomic<std::uint64_t> g_presentation_yaw_epoch{1};
SRWLOCK g_tracking_yaw_lock = SRWLOCK_INIT;
runtime::VrTrackingSpace g_tracking_yaw_space;
float g_native_tracking_yaw = 0.0F;
bool g_native_tracking_yaw_known = false;
float g_pending_spawn_native_yaw_delta = 0.0F;
bool g_pending_spawn_yaw_rebase = false;
bool g_menu_anchor_valid = false;
runtime::VrMatrix34 g_menu_anchor{};
bool g_world_ui_panel_valid = false;
NativeUiSurface g_world_ui_surface = NativeUiSurface::fullscreen;
runtime::VrMatrix44 g_world_ui_panel_pose{};
SRWLOCK g_menu_pointer_lock = SRWLOCK_INIT;
runtime::VrMatrix34 g_menu_pointer_anchor{};
bool g_menu_pointer_world_panel = false;
runtime::VrMatrix44 g_menu_pointer_world_from_tracking{};
runtime::VrMatrix44 g_menu_pointer_world_panel_pose{};
float g_menu_pointer_aspect = 0;
float g_menu_pointer_distance = runtime::vr_setting_limits::kUiDistance.default_value;
float g_menu_pointer_width = 2.4F * runtime::vr_setting_limits::kUiScale.default_value;
float g_menu_pointer_center_y = 0.0F;
std::atomic<float> g_menu_distance{runtime::vr_setting_limits::kUiDistance.default_value};
std::atomic<float> g_menu_scale{runtime::vr_setting_limits::kUiScale.default_value};
std::atomic<float> g_subtitle_scale{runtime::vr_setting_limits::kSubtitleScale.default_value};
std::atomic<bool> g_recenter_requested{false};
SRWLOCK g_world_tracking_lock = SRWLOCK_INIT;
runtime::VrMatrix44 g_world_game_view;
runtime::VrMatrix34 g_world_anchor;
runtime::VrMatrix44 g_world_head_pose;
float g_world_movement_yaw = 0;
bool g_world_movement_yaw_valid = false;
bool g_world_head_pose_valid = false;
std::uint64_t g_world_tracking_time = 0;

void ResetTrackedWorldYawForRecenter() noexcept {
    AcquireSRWLockExclusive(&g_tracking_yaw_lock);
    g_tracking_yaw_space.SetWorldYaw(0.0F);
    g_native_tracking_yaw = 0.0F;
    g_native_tracking_yaw_known = false;
    g_pending_spawn_native_yaw_delta = 0.0F;
    g_pending_spawn_yaw_rebase = false;
    ReleaseSRWLockExclusive(&g_tracking_yaw_lock);
}

[[nodiscard]] bool ResolvePresentationTrackingYaw(
    const runtime::VrMatrix44& game_head_view,
    PresentationSnapshot& snapshot,
    std::string& error) noexcept {
    if (!g_stereo_tracking_anchor_valid) {
        error = "The tracking yaw anchor is unavailable";
        return false;
    }
    const float native_yaw = TrackingWorldYaw(
        game_head_view, g_stereo_tracking_anchor);
    if (!std::isfinite(native_yaw)) {
        error = "The native camera yaw is non-finite";
        return false;
    }

    float tracking_world_yaw = 0.0F;
    bool native_rebased = false;
    AcquireSRWLockExclusive(&g_tracking_yaw_lock);
    if (g_pending_spawn_yaw_rebase) {
        const auto rebase = ComputeSpawnYawRebase(
            g_tracking_yaw_space.world_yaw(),
            0.0F,
            g_pending_spawn_native_yaw_delta);
        if (rebase.valid) {
            g_tracking_yaw_space.SetWorldYaw(rebase.world_yaw);
            native_rebased =
                std::abs(rebase.native_delta) > kNativeYawRebaseEpsilonRadians;
        }
        g_pending_spawn_native_yaw_delta = 0.0F;
        g_pending_spawn_yaw_rebase = false;
        g_native_tracking_yaw = native_yaw;
        g_native_tracking_yaw_known = true;
    } else if (!g_native_tracking_yaw_known) {
        g_native_tracking_yaw = native_yaw;
        g_native_tracking_yaw_known = true;
    } else {
        const float delta = std::remainder(
            native_yaw - g_native_tracking_yaw,
            6.28318530717958647692F);
        if (std::abs(delta) > kNativeYawRebaseEpsilonRadians) {
            g_native_tracking_yaw = native_yaw;
            native_rebased = true;
        }
    }
    tracking_world_yaw = g_tracking_yaw_space.world_yaw();
    if (native_rebased) {
        g_presentation_yaw_epoch.fetch_add(1, std::memory_order_acq_rel);
    }
    snapshot.pose.identity.yaw_epoch =
        g_presentation_yaw_epoch.load(std::memory_order_acquire);
    ReleaseSRWLockExclusive(&g_tracking_yaw_lock);

    if (!runtime::RotateTrackingPoseYaw(
            g_stereo_tracking_anchor,
            -tracking_world_yaw,
            snapshot.effective_tracking_anchor,
            error)) {
        error = "Could not apply tracking-world yaw to the presentation anchor: " +
            error;
        return false;
    }
    snapshot.valid = true;
    AcquireSRWLockExclusive(&g_presentation_lock);
    g_presentation_snapshot = snapshot;
    ReleaseSRWLockExclusive(&g_presentation_lock);
    return true;
}

void InvalidateWorldTracking() {
    InvalidateBlackPlagueShadowTracking();
    AcquireSRWLockExclusive(&g_presentation_lock);
    g_presentation_snapshot = {};
    ReleaseSRWLockExclusive(&g_presentation_lock);
    g_presentation_pose_epoch.fetch_add(1, std::memory_order_acq_rel);
    AcquireSRWLockExclusive(&g_tracking_yaw_lock);
    g_native_tracking_yaw_known = false;
    ReleaseSRWLockExclusive(&g_tracking_yaw_lock);
    AcquireSRWLockExclusive(&g_world_tracking_lock);
    g_world_tracking_time = 0;
    g_world_movement_yaw_valid = false;
    g_world_head_pose_valid = false;
    ReleaseSRWLockExclusive(&g_world_tracking_lock);
}
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
RenderWorldFrameTelemetry g_telemetry;
PresentationTimingTracker g_presentation_timing;
std::atomic<std::uint64_t> g_telemetry_dropped_updates{0};
std::atomic<bool> g_capabilities_initialized{false};
FramebufferApi g_framebuffer_api = FramebufferApi::unavailable;
std::array<char, 64> g_open_gl_version{};
std::array<GLint, 2> g_max_viewport_dimensions{};
GLint g_max_texture_size = 0;
GLint g_max_renderbuffer_size = 0;

template <typename Callback>
void TryRecordTelemetry(Callback&& callback) noexcept {
    TelemetryTryLock lock(g_telemetry_lock);
    if (!lock.acquired()) {
        g_telemetry_dropped_updates.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    callback();
}

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

void __fastcall HookedParticleUpdateGraphics(
    void* emitter,
    void*,
    void* camera,
    float frame_time,
    void* render_list) noexcept {
    ActiveCall active_call;
    g_particle_update_calls.fetch_add(1, std::memory_order_relaxed);
    g_particle_stereo_refresh.Capture(camera, frame_time, render_list);
    const auto original = reinterpret_cast<ParticleUpdateGraphics>(
        g_original_particle_update_graphics.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(emitter, camera, frame_time, render_list);
    }
}

void* __fastcall HookedParticleGetModelMatrix(
    void* emitter,
    void*,
    void* camera) noexcept {
    ActiveCall active_call;
    const void* const update_target =
        g_original_particle_update_graphics.load(std::memory_order_acquire);
    const bool refreshed = g_particle_stereo_refresh.Refresh(
        emitter,
        camera,
        const_cast<void*>(update_target));
    if (refreshed) {
        g_particle_eye_refreshes.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_particle_refresh_misses.fetch_add(1, std::memory_order_relaxed);
    }
    const auto original = reinterpret_cast<ParticleGetModelMatrix>(
        g_original_particle_get_model_matrix.load(std::memory_order_acquire));
    return original != nullptr ? original(emitter, camera) : nullptr;
}

void __fastcall HookedSpawnCameraSetYaw(
    void* camera,
    void*,
    float requested_yaw) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<CameraSetYaw>(
        g_original_spawn_yaw_target.load(std::memory_order_acquire));
    if (original == nullptr || camera == nullptr) return;

    float old_yaw = 0.0F;
    std::memcpy(
        &old_yaw,
        static_cast<const std::uint8_t*>(camera) + kCameraYawOffset,
        sizeof(old_yaw));
    original(camera, requested_yaw);
    float new_yaw = 0.0F;
    std::memcpy(
        &new_yaw,
        static_cast<const std::uint8_t*>(camera) + kCameraYawOffset,
        sizeof(new_yaw));

    const auto rebase = ComputeSpawnYawRebase(0.0F, old_yaw, new_yaw);
    if (!rebase.valid ||
        std::abs(rebase.native_delta) <= kNativeYawRebaseEpsilonRadians) {
        return;
    }
    AcquireSRWLockExclusive(&g_tracking_yaw_lock);
    g_pending_spawn_native_yaw_delta = WrapSpawnYaw(
        g_pending_spawn_native_yaw_delta + rebase.native_delta);
    g_pending_spawn_yaw_rebase = true;
    ReleaseSRWLockExclusive(&g_tracking_yaw_lock);
}

class DeferredStereoFrameRelease final {
public:
    DeferredStereoFrameRelease() = default;
    DeferredStereoFrameRelease(const DeferredStereoFrameRelease&) = delete;
    DeferredStereoFrameRelease& operator=(const DeferredStereoFrameRelease&) = delete;
    ~DeferredStereoFrameRelease() {
        g_stereo_frame_calls.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void RecordGameplayOverlayFailure(const std::string& error) noexcept {
    TryRecordTelemetry([&]() noexcept {
        ++g_telemetry.gameplay_overlay_failures;
        strncpy_s(
            g_telemetry.gameplay_overlay_error.data(),
            g_telemetry.gameplay_overlay_error.size(),
            error.c_str(),
            _TRUNCATE);
    });
}

void FailStereoMatrixValidation(const std::string& error) noexcept;

[[nodiscard]] bool CompositeGameplayOverlayIntoEyes(
    const PendingGameplayOverlaySubmission& pending,
    std::string& error) noexcept {
    std::uint32_t overlay_texture = 0;
    if (!GetGameplayOverlayColorTexture(overlay_texture, error)) {
        return false;
    }

    runtime::VrMatrix44 head_pose;
    if (!runtime::InvertRigidTransform(
            CollapseMatrix(pending.head_view), head_pose, error)) {
        return false;
    }

    for (std::size_t eye_index = 0; eye_index < 2; ++eye_index) {
        runtime::VrMatrix44 eye_view;
        if (!runtime::ComposeEyeViewFromHeadView(
                pending.head_view,
                g_stereo_eyes[eye_index].eye_to_head,
                eye_view,
                error)) {
            return false;
        }
        const bool world_ui=pending.ui_surface==NativeUiSurface::inventory ||
            pending.ui_surface==NativeUiSurface::notebook;
        const runtime::VrMatrix44 model_view=runtime::Multiply(
            eye_view,world_ui ? pending.ui_panel_world_pose : head_pose);
        graphics::OpenGlEyeBinding eye_binding;
        if (!BeginPersistentEyeTarget(
                eye_index == 0 ? graphics::Eye::left : graphics::Eye::right,
                eye_binding,
                error)) {
            return false;
        }
        const auto overlay = BlackPlagueGameplayOverlay(
            g_subtitle_scale.load(std::memory_order_acquire),
            g_menu_distance.load(std::memory_order_acquire));
        const auto panel=pending.ui_surface==NativeUiSurface::notebook
            ? BlackPlagueNotebookPanel(g_menu_scale.load(std::memory_order_acquire))
            : BlackPlagueInventoryPanel(g_menu_scale.load(std::memory_order_acquire));
        const float left=world_ui ? -panel.width*0.5F : overlay.left;
        const float right=world_ui ? panel.width*0.5F : overlay.right;
        const float bottom=world_ui ? panel.center_y-panel.width*0.375F : overlay.bottom;
        const float top=world_ui ? panel.center_y+panel.width*0.375F : overlay.top;
        const float distance=world_ui ? panel.distance : overlay.distance;
        const bool drawn = graphics::DrawTransparentOverlay(
            overlay_texture,
            model_view,
            g_stereo_projections[eye_index],
            left,right,bottom,top,distance,
            error);
        const std::string draw_error = error;
        std::string restore_error;
        const bool restored = EndPersistentEyeTarget(eye_binding, restore_error);
        if (!drawn || !restored) {
            error = drawn ? "Could not restore the eye after gameplay overlay: " + restore_error
                          : draw_error;
            if (!drawn && !restored && !restore_error.empty()) {
                error += "; eye restore also failed: " + restore_error;
            }
            return false;
        }
    }
    return true;
}

void __fastcall HookedGraphicsDrawerDrawAll(void* drawer, void*) noexcept {
    ActiveCall active_call;
    const auto original = reinterpret_cast<GraphicsDrawerDrawAll>(
        g_original_draw_all_target.load(std::memory_order_acquire));
    if (original == nullptr) {
        if (g_pending_gameplay_overlay_submission.active) {
            g_pending_gameplay_overlay_submission = {};
            g_stereo_frame_calls.fetch_sub(1, std::memory_order_acq_rel);
        }
        return;
    }
    if (!g_pending_gameplay_overlay_submission.active) {
        original(drawer);
        return;
    }

    const PendingGameplayOverlaySubmission pending =
        g_pending_gameplay_overlay_submission;
    g_pending_gameplay_overlay_submission = {};
    DeferredStereoFrameRelease deferred_frame_release;
    const auto overlay_start = PerformanceClock::now();
    bool captured = false;
    bool capture_binding_failed = false;
    std::string error;
    graphics::OpenGlEyeBinding overlay_binding;
    if (BeginGameplayOverlayTarget(overlay_binding, error)) {
        glPushAttrib(
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT | GL_SCISSOR_BIT);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(0, 0, 0, 0);
        glClearDepth(1.0);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glPopAttrib();
        original(drawer);
        std::string restore_error;
        if (EndGameplayOverlayTarget(overlay_binding, restore_error)) {
            captured = true;
        } else {
            error = "Could not restore GL state after native gameplay overlay: " +
                restore_error;
            capture_binding_failed = true;
        }
    } else {
        // The native drawer must still consume its queue exactly once. If the
        // auxiliary surface is unavailable, preserve gameplay and submit the
        // already-rendered eyes without the overlay for this frame.
        original(drawer);
    }

    bool overlay_composited = false;
    if (captured) {
        overlay_composited = CompositeGameplayOverlayIntoEyes(pending, error);
        if (!overlay_composited) {
            capture_binding_failed = true;
        }
    }
    if (!captured || !overlay_composited) {
        RecordGameplayOverlayFailure(error.empty()
            ? "Gameplay overlay capture was unavailable"
            : error);
    }

    bool submitted = false;
    std::uint64_t submit_cpu_ns = 0;
    if (!capture_binding_failed && pending.session != nullptr &&
        !g_stereo_cancel.load(std::memory_order_acquire)) {
        std::array<std::uint32_t, 2> color_textures{};
        if (GetPersistentEyeColorTextures(color_textures, error)) {
            const auto submit_start = PerformanceClock::now();
            submitted = pending.session->SubmitOpenGlEyeTextures(
                color_textures, error);
            submit_cpu_ns = ElapsedNanoseconds(submit_start);
            if (submitted) {
                glFlush();
                if (pending.presentation_from_visibility) {
                    g_last_submitted_presentation_sequence.store(
                        pending.presentation_sequence,
                        std::memory_order_release);
                }
            }
        }
    }

    TryRecordTelemetry([&]() noexcept {
        g_telemetry.gameplay_overlay_cpu_ns += ElapsedNanoseconds(overlay_start);
        if (captured && overlay_composited) {
            ++g_telemetry.gameplay_overlay_frames;
            g_telemetry.gameplay_overlay_error = {};
        }
        if (submitted) {
            g_presentation_timing.RecordSubmitAge(
                pending.presentation_timestamp_ms, GetTickCount64());
            ++g_telemetry.deferred_compositor_submits;
            ++g_telemetry.compositor_submitted_frames;
            g_telemetry.compositor_hmd_pose_valid = true;
            g_telemetry.compositor_submit_cpu_ns += submit_cpu_ns;
        }
    });

    if (!capture_binding_failed && pending.session != nullptr &&
        !g_stereo_cancel.load(std::memory_order_acquire) && !submitted) {
        FailStereoMatrixValidation(
            "Deferred gameplay-overlay compositor submit failed: " + error);
    } else if (capture_binding_failed) {
        FailStereoMatrixValidation(
            "Gameplay overlay composition failed: " + error);
    }
}

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
    TryRecordTelemetry([]() noexcept {
        g_telemetry.stereo_camera_restored = false;
    });
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
    TryRecordTelemetry([&]() noexcept {
        g_telemetry.stereo_failed = true;
        strncpy_s(
            g_telemetry.stereo_error.data(),
            g_telemetry.stereo_error.size(),
            error.c_str(),
            _TRUNCATE);
    });
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
    TryRecordTelemetry([&]() noexcept {
        ++g_telemetry.hmd_visibility_failures;
        g_telemetry.hmd_visibility_camera_restored = camera_restored;
        strncpy_s(
            g_telemetry.hmd_visibility_error.data(),
            g_telemetry.hmd_visibility_error.size(),
            error.c_str(),
            _TRUNCATE);
    });
}

[[nodiscard]] bool AcquirePresentationSnapshot(
    PresentationSnapshot& snapshot,
    bool& recentered,
    std::string& error) noexcept {
    snapshot = {};
    recentered = false;
    auto* const session = g_stereo_session.load(std::memory_order_acquire);
    if (session == nullptr || !session->WaitForHmdPose(snapshot.pose, error) ||
        !snapshot.pose.device_connected || !snapshot.pose.pose_valid) {
        AcquireSRWLockExclusive(&g_presentation_lock);
        g_presentation_snapshot = {};
        ReleaseSRWLockExclusive(&g_presentation_lock);
        return false;
    }

    recentered = g_recenter_requested.exchange(false, std::memory_order_acq_rel) ||
        !g_stereo_tracking_anchor_valid;
    if (recentered) {
        g_stereo_tracking_anchor = snapshot.pose.device_to_absolute;
        g_stereo_tracking_anchor_valid = true;
        ResetTrackedWorldYawForRecenter();
        g_presentation_pose_epoch.fetch_add(1, std::memory_order_acq_rel);
        g_presentation_yaw_epoch.fetch_add(1, std::memory_order_acq_rel);
    }
    snapshot.pose.identity.sequence =
        g_presentation_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    snapshot.pose.identity.timestamp_ms = GetTickCount64();
    TryRecordTelemetry([&]() noexcept {
        g_presentation_timing.RecordAcquisition(
            snapshot.pose.identity.timestamp_ms);
    });
    snapshot.pose.identity.pose_epoch =
        g_presentation_pose_epoch.load(std::memory_order_acquire);
    snapshot.pose.identity.yaw_epoch =
        g_presentation_yaw_epoch.load(std::memory_order_acquire);
    snapshot.room_scale = ReadBlackPlagueRoomScaleCameraSample();
    if (recentered) {
        snapshot.room_scale.valid = false;
        snapshot.room_scale.horizontal_world_offset = {};
    }
    g_stereo_latest_pose = snapshot.pose.device_to_absolute;
    g_stereo_latest_pose_valid = true;
    g_stereo_room_scale_sample = snapshot.room_scale;
    return true;
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
        const auto ui_surface=NativeInputUiSurface();
        const bool world_ui=ui_surface==NativeUiSurface::inventory ||
            ui_surface==NativeUiSurface::notebook;
        return g_stereo_persistent.load(std::memory_order_acquire) &&
            g_stereo_track_head_rotation &&
            (!NativeInputUiActive() || world_ui) &&
            (g_stereo_state.load(std::memory_order_acquire) ==
                StereoMatrixState::pending ||
             g_stereo_state.load(std::memory_order_acquire) ==
                StereoMatrixState::processing);
    };
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

    // UpdateRenderList is also reached from the two RenderWorld eye passes.
    // Those nested calls must reuse the presentation sample acquired by the
    // pre-RenderWorld visibility owner. Calling WaitGetPoses again here moves
    // compositor ownership forward and can leave the next world Submit on a
    // frame whose eyes were already submitted (VRCompositorError 108).
    const bool reuse_presentation =
        g_stereo_frame_calls.load(std::memory_order_acquire) != 0;
    ActiveStereoFrame active_stereo_frame;
    if (!visibility_active()) {
        original(renderer, world, camera, frame_time);
        return;
    }

    StereoMatrixState pending = StereoMatrixState::pending;
    static_cast<void>(g_stereo_state.compare_exchange_strong(
        pending, StereoMatrixState::processing, std::memory_order_acq_rel));
    PresentationSnapshot presentation;
    bool recentered = false;
    if (reuse_presentation) {
        AcquireSRWLockShared(&g_presentation_lock);
        presentation = g_presentation_snapshot;
        ReleaseSRWLockShared(&g_presentation_lock);
        if (!presentation.valid) {
            RecordHmdVisibilityFailure(
                "The active stereo frame has no presentation snapshot", true);
            original(renderer, world, camera, frame_time);
            return;
        }
        TryRecordTelemetry([]() noexcept {
            ++g_telemetry.presentation_pose_reuses;
        });
    } else {
        if (!AcquirePresentationSnapshot(presentation, recentered, error)) {
            RecordHmdVisibilityFailure(
                error.empty() ? "The presentation HMD pose is unavailable" : error,
                true);
            original(renderer, world, camera, frame_time);
            return;
        }
        TryRecordTelemetry([]() noexcept {
            ++g_telemetry.presentation_pose_acquisitions;
        });
        if (!ResolvePresentationTrackingYaw(
                camera_snapshot.view, presentation, error)) {
            RecordHmdVisibilityFailure(error, true);
            original(renderer, world, camera, frame_time);
            return;
        }
    }

    runtime::VrMatrix44 tracked_head_view = camera_snapshot.view;
    if (!reuse_presentation) {
        bool positional_translation_applied = false;
        std::array<float, 3> world_translation{};
        std::array<float, 3> render_prediction{};
        std::array<float, 3> render_head_anchor{};
        if (!ComposeBlackPlagueTrackedHeadView(
                camera_snapshot.view,
                presentation.effective_tracking_anchor,
                presentation.pose.device_to_absolute,
                presentation.pose.identity,
                presentation.room_scale,
                tracked_head_view,
                positional_translation_applied,
                world_translation,
                render_prediction,
                render_head_anchor,
                error)) {
            RecordHmdVisibilityFailure(
                "Could not compose the HMD visibility view: " + error, true);
            original(renderer, world, camera, frame_time);
            return;
        }
    }
    // Inside RenderStereoEye the camera already contains this eye's tracked
    // view. Composing HMD tracking again here moved the render-list camera a
    // second time, so camera-facing billboards were prepared for a different
    // position from the eye that drew them. Only the standalone pre-eye
    // visibility pass needs to compose tracking from the native game camera.

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

    TryRecordTelemetry([]() noexcept {
        ++g_telemetry.hmd_visibility_updates;
        g_telemetry.hmd_visibility_camera_restored = true;
        g_telemetry.hmd_visibility_error = {};
    });
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
    std::uint64_t& world_render_cpu_ns,
    std::uint64_t& hand_draw_cpu_ns,
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
    graphics::OpenGlEnhancedEyeBinding enhanced_binding;
    if (!BeginPersistentEnhancedEyeScene(eye, enhanced_binding, error)) {
        const std::string operation_error = error;
        std::string binding_error;
        static_cast<void>(EndPersistentEyeTarget(binding, binding_error));
        error = operation_error;
        if (!binding_error.empty()) {
            error += "; eye binding cleanup also failed: " + binding_error;
        }
        return false;
    }

    CameraMatrixOverride camera_override(kCameraLayout);
    if (!camera_override.Apply(
            camera, eye_view, g_stereo_projections[eye_index], error)) {
        const std::string operation_error = error;
        std::string enhanced_error;
        static_cast<void>(EndPersistentEnhancedEyeScene(
            enhanced_binding, enhanced_error));
        std::string binding_error;
        static_cast<void>(EndPersistentEyeTarget(binding, binding_error));
        error = operation_error;
        if (!enhanced_error.empty()) {
            error += "; enhanced eye cleanup also failed: " + enhanced_error;
        }
        if (!binding_error.empty()) {
            error += "; eye binding cleanup also failed: " + binding_error;
        }
        return false;
    }

    {
        hooks::ScopedEyeScissor scissor({
            binding.previous_viewport[2], binding.previous_viewport[3]});
        const auto world_render_start = PerformanceClock::now();
        original(renderer, world, camera, frame_time);
        world_render_cpu_ns += ElapsedNanoseconds(world_render_start);
        world_rendered = true;
        TryRecordTelemetry([&]() noexcept {
            g_telemetry.eye_scissor_remapped += scissor.remapped();
            g_telemetry.eye_scissor_bypassed += scissor.bypassed();
        });
    }

    if (g_stereo_persistent.load(std::memory_order_acquire)) {
        const auto frame=ReadNativeControllerFrame();
        std::array<graphics::TrackedHandVisual,2> hands{};
        std::array<runtime::VrMatrix44,2> raw_palms{};
        std::array<bool,2> raw_palm_valid{};
        runtime::VrMatrix44 head_pose{};
        const bool head_valid=TrackedHeadWorldPose(head_pose);
        if (frame.focused) for (std::size_t i=0;i<hands.size();++i) {
            auto& hand=hands[i];
            std::array<float,3> velocity{},angular{};
            raw_palm_valid[i]=ControllerWorldPose(
                frame.hands[i].grip,raw_palms[i],velocity,angular);
            hand.visible=raw_palm_valid[i];
            hand.palm=raw_palms[i];
            hand.ray=ControllerWorldPose(frame.hands[i].aim,hand.aim,velocity,angular) &&
                i==(frame.interact_source==runtime::VrHand::left ? 0U : 1U);
            if (frame.hands[i].skeleton_valid) hand.curl=frame.hands[i].finger_curl;
            else hand.curl.fill(frame.input.state.interact.pressed && hand.ray ? 0.8F : 0.1F);
            float attached_grip=0.0F;
            if (ReadAttachedToolGrip(i,attached_grip)) {
                hand.hold_pose_weight=attached_grip;
            }
        }
        std::array<float,3> use_item_from{},use_item_to{};
        bool use_item_usable=false;
        if (frame.focused && ReadUseItemLaser(
                use_item_from,use_item_to,use_item_usable)) {
            const std::size_t hand_index=
                frame.interact_source==runtime::VrHand::left ? 0U : 1U;
            hands[hand_index].colored_ray=true;
            hands[hand_index].ray_usable=use_item_usable;
            hands[hand_index].ray_from=use_item_from;
            hands[hand_index].ray_to=use_item_to;
        }
        PublishGameplayPalmTracking(
            raw_palms,raw_palm_valid,head_pose,head_valid,
            g_presentation_yaw_epoch.load(std::memory_order_acquire));
        for (std::size_t i=0;i<hands.size();++i) {
            runtime::VrMatrix44 resolved{};
            if (hands[i].visible && !NativeInputUiActive() &&
                ReadGameplayPalmPose(i,resolved)) {
                hands[i].palm=resolved;
            }
        }
        // Hands are optional decoration: never interrupt the world/compositor
        // if the fixed-function compatibility path is unavailable.
        std::string hand_error;
        const auto hand_draw_start = PerformanceClock::now();
        static_cast<void>(graphics::DrawTrackedHands(hands,eye_view,g_stereo_projections[eye_index],hand_error));
        hand_draw_cpu_ns += ElapsedNanoseconds(hand_draw_start);
    }

    std::string enhanced_error;
    const bool enhanced_restored = EndPersistentEnhancedEyeScene(
        enhanced_binding, enhanced_error);
    std::string camera_error;
    const bool camera_restored = camera_override.Restore(camera_error);
    std::string binding_error;
    const bool binding_restored = EndPersistentEyeTarget(binding, binding_error);
    if (!enhanced_restored || !camera_restored || !binding_restored) {
        if (!enhanced_restored) {
            error = "Could not resolve the enhanced eye stage: " + enhanced_error;
        }
        if (!camera_restored) {
            RecordStereoCameraRestorationFailure();
            if (!error.empty()) error += "; ";
            error += "Could not restore the HPL camera after an eye pass: " +
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
    // Full-screen menus still use the desktop capture at SwapBuffers.
    // Inventory/notebook keep the stereoscopic world and their native 800x600
    // DrawAll queue is composited as an alpha surface after the eye passes.
    const auto ui_surface=NativeInputUiSurface();
    const bool world_ui=NativeInputUiActive() &&
        (ui_surface==NativeUiSurface::inventory ||
         ui_surface==NativeUiSurface::notebook);
    const bool closing_world_ui=BlackPlaguePreserveWorldPanelOnExit(
        world_ui,NativeInputUiActive(),g_world_ui_panel_valid);
    const auto closing_panel_surface=g_world_ui_surface;
    const auto closing_panel_pose=g_world_ui_panel_pose;
    if (g_stereo_persistent.load(std::memory_order_acquire) &&
        NativeInputUiActive() && !world_ui) {
        InvalidateWorldTracking();
        g_world_ui_panel_valid=false;
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
    const auto stereo_start = PerformanceClock::now();

    runtime::OpenVrSession* session =
        g_stereo_session.load(std::memory_order_acquire);
    std::string error;
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

    runtime::VrHmdPose pose;
    PresentationSnapshot presentation;
    bool presentation_from_visibility = false;
    bool presentation_recentered = false;
    if (session != nullptr && g_stereo_persistent.load(std::memory_order_acquire) &&
        g_stereo_track_head_rotation) {
        AcquireSRWLockShared(&g_presentation_lock);
        presentation = g_presentation_snapshot;
        ReleaseSRWLockShared(&g_presentation_lock);
        const auto now = GetTickCount64();
        TryRecordTelemetry([&]() noexcept {
            g_presentation_timing.RecordRenderAge(
                presentation.pose.identity.timestamp_ms, now);
        });
        if (!presentation.valid || presentation.pose.identity.timestamp_ms == 0 ||
            now < presentation.pose.identity.timestamp_ms ||
            now - presentation.pose.identity.timestamp_ms > 250) {
            InvalidateWorldTracking();
            return result;
        }
        const auto last_submitted =
            g_last_submitted_presentation_sequence.load(std::memory_order_acquire);
        if (!runtime::IsFreshPresentationSequence(
                presentation.pose.identity.sequence, last_submitted)) {
            if (!runtime::ShouldRefreshPresentationSequence(
                    presentation.pose.identity.sequence, last_submitted)) {
                TryRecordTelemetry([]() noexcept {
                    ++g_telemetry.presentation_pose_stale_rejects;
                });
                return result;
            }
            if (!AcquirePresentationSnapshot(
                    presentation, presentation_recentered, error) ||
                !ResolvePresentationTrackingYaw(
                    camera_snapshot.view, presentation, error)) {
                InvalidateWorldTracking();
                return result;
            }
            TryRecordTelemetry([]() noexcept {
                ++g_telemetry.presentation_pose_acquisitions;
            });
        }
        pose = presentation.pose;
        presentation_from_visibility = true;
    } else if (session != nullptr) {
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

    runtime::VrMatrix44 head_view = camera_snapshot.view;
    BlackPlagueRoomScaleCameraSample room_scale;
    bool positional_translation_applied = false;
    std::array<float, 3> room_scale_world_translation{};
    std::array<float, 3> room_scale_render_prediction{};
    std::array<float, 3> room_scale_render_head_anchor{};
    if (g_stereo_track_head_rotation) {
        if (!g_stereo_tracking_anchor_valid) {
            g_stereo_tracking_anchor = pose.device_to_absolute;
            g_stereo_tracking_anchor_valid = true;
            presentation_recentered = true;
        }
        runtime::VrMatrix34 frame_tracking_anchor = g_stereo_tracking_anchor;
        if (presentation_from_visibility) {
            room_scale = presentation.room_scale;
            frame_tracking_anchor = presentation.effective_tracking_anchor;
        } else {
            room_scale = ReadBlackPlagueRoomScaleCameraSample();
        }
        if (presentation_recentered) {
            room_scale.valid = false;
            room_scale.horizontal_world_offset = {};
        }
        // UpdateRenderList runs inside the eye passes below. Cache the same
        // reconciled sample used by the rendered head so visibility cannot
        // observe a different body tick halfway through this stereo frame.
        g_stereo_room_scale_sample = room_scale;
        if (!ComposeBlackPlagueTrackedHeadView(
                camera_snapshot.view,
                frame_tracking_anchor,
                pose.device_to_absolute,
                pose.identity,
                room_scale,
                head_view,
                positional_translation_applied,
                room_scale_world_translation,
                room_scale_render_prediction,
                room_scale_render_head_anchor,
                error)) {
            FailStereoMatrixValidation(
                "Could not compose the yaw-recentered HMD rotation: " + error);
            return result;
        }
        g_stereo_latest_pose = pose.device_to_absolute;
        g_stereo_latest_pose_valid = true;
        runtime::VrMatrix44 controller_game_view = camera_snapshot.view;
        if (positional_translation_applied &&
            !runtime::ApplyWorldTranslationToView(
                camera_snapshot.view, room_scale_world_translation,
                controller_game_view, error)) {
            FailStereoMatrixValidation(
                "Could not compose the room-scale controller basis: " + error);
            return result;
        }
        runtime::VrMatrix44 movement_head_pose;
        std::string movement_pose_error;
        const bool movement_head_pose_valid = runtime::InvertRigidTransform(
            CollapseMatrix(head_view), movement_head_pose,
            movement_pose_error);
        AcquireSRWLockExclusive(&g_world_tracking_lock);
        g_world_game_view = controller_game_view;
        g_world_anchor = frame_tracking_anchor;
        g_world_head_pose = movement_head_pose;
        g_world_head_pose_valid = movement_head_pose_valid;
        // Rework steers from the current HMD world heading. Keep the same
        // tracking-only basis here instead of feeding the rendered game camera
        // back into the native-input remap.
        g_world_movement_yaw_valid = runtime::HorizontalTrackingYawDelta(
            frame_tracking_anchor, pose.device_to_absolute,
            g_world_movement_yaw);
        const float gx = -camera_snapshot.view.values[8];
        const float gz = -camera_snapshot.view.values[10];
        // Keep hands relative to the current physical head. The game-view
        // basis already includes any reconciled room-scale offset above.
        for (const auto index : {3U,7U,11U})
            g_world_anchor.values[index] = pose.device_to_absolute.values[index];
        g_world_tracking_time = GetTickCount64();
        ReleaseSRWLockExclusive(&g_world_tracking_lock);
        // Same yaw basis as ComposeYawRecenteredTrackedHeadView. Use the
        // original rotational anchor, not g_world_anchor whose translation
        // is deliberately overwritten for rotation-only hand rendering.
        const float ax = -frame_tracking_anchor.values[2];
        const float az = -frame_tracking_anchor.values[10];
        const float shadow_yaw = std::atan2(az * gx - ax * gz,
            ax * gx + az * gz);
        PublishBlackPlagueShadowTracking(pose.device_to_absolute,
            shadow_yaw, presentation_recentered, pose.identity);
    }

    if (world_ui) {
        runtime::VrMatrix44 head_world_pose{},tracking_from_head{};
        std::string panel_error;
        if (!runtime::InvertRigidTransform(
                CollapseMatrix(head_view),head_world_pose,panel_error) ||
            !runtime::InvertRigidTransform(
                pose.device_to_absolute,tracking_from_head,panel_error)) {
            FailStereoMatrixValidation(
                "Could not place the native UI in the VR world: "+panel_error);
            return result;
        }
        const auto world_from_tracking=runtime::Multiply(
            head_world_pose,tracking_from_head);
        if (ui_surface==NativeUiSurface::inventory) {
            if (!g_world_ui_panel_valid ||
                g_world_ui_surface!=NativeUiSurface::inventory ||
                presentation_recentered)
                g_world_ui_panel_pose=head_world_pose;
        } else {
            const auto frame=ReadNativeControllerFrame();
            const std::size_t off_hand=frame.interact_source==runtime::VrHand::left
                ? 1U : 0U;
            const auto& grip=frame.hands[off_hand].grip;
            if (frame.focused && grip.device_connected && grip.pose_valid) {
                const float scale=g_menu_scale.load(std::memory_order_acquire);
                runtime::VrMatrix44 book_offset=runtime::IdentityMatrix();
                book_offset.values[3]=
                    (off_hand==0U ? 175.0F : -175.0F)/1450.0F*scale;
                // Rework Notebook.cpp rotates the 800x600 book plane down
                // ninety degrees about the off hand before centering it.
                runtime::VrMatrix44 book_pitch=runtime::IdentityMatrix();
                book_pitch.values[5]=0.0F;
                book_pitch.values[6]=1.0F;
                book_pitch.values[9]=-1.0F;
                book_pitch.values[10]=0.0F;
                g_world_ui_panel_pose=runtime::Multiply(
                    runtime::Multiply(world_from_tracking,
                        runtime::Multiply(
                            runtime::ExpandMatrix(grip.device_to_absolute),book_pitch)),
                    book_offset);
            } else if (!g_world_ui_panel_valid ||
                       g_world_ui_surface!=NativeUiSurface::notebook) {
                g_world_ui_panel_pose=head_world_pose;
            }
        }
        g_world_ui_panel_valid=true;
        g_world_ui_surface=ui_surface;
        const auto panel=ui_surface==NativeUiSurface::notebook
            ? BlackPlagueNotebookPanel(g_menu_scale.load(std::memory_order_acquire))
            : BlackPlagueInventoryPanel(g_menu_scale.load(std::memory_order_acquire));
        AcquireSRWLockExclusive(&g_menu_pointer_lock);
        g_menu_pointer_world_panel=true;
        g_menu_pointer_world_from_tracking=world_from_tracking;
        g_menu_pointer_world_panel_pose=g_world_ui_panel_pose;
        g_menu_pointer_aspect=4.0F/3.0F;
        g_menu_pointer_distance=panel.distance;
        g_menu_pointer_width=panel.width;
        g_menu_pointer_center_y=panel.center_y;
        ReleaseSRWLockExclusive(&g_menu_pointer_lock);
    } else {
        g_world_ui_panel_valid=false;
    }

    // Read the snapshot sampled once by ButtonHandler::Update. Never consume
    // OpenVR button edges a second time from rendering or from another eye.
    if (session != nullptr && session->controller_input_initialized() &&
        g_stereo_persistent.load(std::memory_order_acquire)) {
        const auto controllers = ReadNativeControllerFrame();
        TryRecordTelemetry([&]() noexcept {
            ++g_telemetry.controller_samples;
            g_telemetry.controller_frame = controllers;
            g_telemetry.controller_error = {};
        });
    }
    const bool persistent_stereo =
        g_stereo_persistent.load(std::memory_order_acquire);
    const runtime::StereoRenderPlan render_plan =
        runtime::PlanStereoWorldRendering(
            frame_time,
            persistent_stereo,
            false); // Mirror now copies an eye at swap, not a third native world pass.

    std::uint32_t completed_eye_passes = 0;
    std::uint64_t eye_world_cpu_ns = 0;
    std::uint64_t hand_draw_cpu_ns = 0;
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
                eye_world_cpu_ns,
                hand_draw_cpu_ns,
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

    std::uint64_t compositor_submit_cpu_ns = 0;
    bool compositor_submit_deferred = false;
    if (session != nullptr) {
        if (persistent_stereo && g_draw_all_hook.installed()) {
            if (g_pending_gameplay_overlay_submission.active) {
                FailStereoMatrixValidation(
                    "A previous gameplay-overlay submission is still pending");
                return result;
            }
            g_pending_gameplay_overlay_submission.active = true;
            g_pending_gameplay_overlay_submission.session = session;
            g_pending_gameplay_overlay_submission.head_view = head_view;
            g_pending_gameplay_overlay_submission.ui_surface =
                world_ui ? ui_surface :
                (closing_world_ui ? closing_panel_surface : NativeUiSurface::fullscreen);
            if (world_ui || closing_world_ui)
                g_pending_gameplay_overlay_submission.ui_panel_world_pose =
                    world_ui ? g_world_ui_panel_pose : closing_panel_pose;
            g_pending_gameplay_overlay_submission.presentation_sequence =
                presentation.pose.identity.sequence;
            g_pending_gameplay_overlay_submission.presentation_timestamp_ms =
                presentation.pose.identity.timestamp_ms;
            g_pending_gameplay_overlay_submission.presentation_from_visibility =
                presentation_from_visibility;
            // Hold tracked-stereo lifetime ownership until the native DrawAll
            // owner has composed HPL's 2D queue and submitted this frame.
            g_stereo_frame_calls.fetch_add(1, std::memory_order_acq_rel);
            compositor_submit_deferred = true;
        } else {
            std::array<std::uint32_t, 2> color_textures{};
            if (!GetPersistentEyeColorTextures(color_textures, error)) {
                FailStereoMatrixValidation(
                    "Could not obtain the rendered eye textures: " + error);
                return result;
            }
            const auto compositor_submit_start = PerformanceClock::now();
            if (!session->SubmitOpenGlEyeTextures(color_textures, error)) {
                FailStereoMatrixValidation(
                    "Could not submit the rendered stereo pair: " + error);
                return result;
            }
            if (presentation_from_visibility) {
                g_last_submitted_presentation_sequence.store(
                    presentation.pose.identity.sequence, std::memory_order_release);
            }
            glFlush();
            compositor_submit_cpu_ns = ElapsedNanoseconds(compositor_submit_start);
        }
    }

    const std::uint64_t stereo_cpu_ns = ElapsedNanoseconds(stereo_start);

    TryRecordTelemetry([&]() noexcept {
        ++g_telemetry.stereo_frames;
        g_telemetry.stereo_eye_passes += completed_eye_passes;
        g_telemetry.stereo_cpu_ns += stereo_cpu_ns;
        g_telemetry.eye_world_cpu_ns += eye_world_cpu_ns;
        g_telemetry.hand_draw_cpu_ns += hand_draw_cpu_ns;
        g_telemetry.compositor_submit_cpu_ns += compositor_submit_cpu_ns;
        if (session != nullptr && !compositor_submit_deferred) {
            g_presentation_timing.RecordSubmitAge(
                presentation.pose.identity.timestamp_ms, GetTickCount64());
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
                positional_translation_applied
                    ? runtime::vr_locomotion_policy::kWorldUnitsPerMeter
                    : 0.0F;
            g_telemetry.room_scale_enabled = room_scale.enabled;
            g_telemetry.room_scale_sample_valid = room_scale.valid;
            g_telemetry.positional_translation_applied =
                positional_translation_applied;
            g_telemetry.room_scale_body_generation =
                room_scale.body_generation;
            g_telemetry.room_scale_camera_offset_m =
                room_scale_world_translation;
            g_telemetry.room_scale_reconciled_offset_m =
                room_scale.horizontal_world_offset;
            g_telemetry.room_scale_render_prediction_m =
                room_scale_render_prediction;
            g_telemetry.room_scale_head_anchor_m =
                room_scale_render_head_anchor;
        }
        g_telemetry.stereo_camera_restored = true;
        g_telemetry.persistent_stereo_active = persistent_stereo;
    });

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

    TryRecordTelemetry([&]() noexcept {
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
    });

    const auto original = reinterpret_cast<RenderWorld>(
        g_original_target.load(std::memory_order_acquire));
    if (original != nullptr) {
        const StereoProcessingResult stereo = ProcessControlledStereoMatrices(
            original, renderer, world, camera, frame_time);
        ProcessControlledWorldDuplication(
            original, renderer, world, camera);
        if (stereo.completed && stereo.suppress_original_world) {
            TryRecordTelemetry([]() noexcept {
                ++g_telemetry.suppressed_monitor_world_passes;
                ++g_telemetry.eye_owned_frame_time_frames;
            });
        } else {
            const float original_frame_time =
                stereo.frame_time_consumed_by_eye ? 0.0F : frame_time;
            original(renderer, world, camera, original_frame_time);
            TryRecordTelemetry([&]() noexcept {
                ++g_telemetry.monitor_world_passes;
                if (stereo.frame_time_consumed_by_eye) {
                    ++g_telemetry.eye_owned_frame_time_frames;
                }
            });
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
    if (g_hook.installed() || g_visibility_hook.installed() ||
        g_draw_all_hook.installed() || g_spawn_yaw_hook.installed() ||
        g_particle_get_model_matrix_hook.installed() ||
        g_particle_update_graphics_hook.installed()) {
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
    std::uint8_t* post_scene_draw_call_site =
        image + kUpdaterPostSceneDrawCallSiteRva;
    if (DecodeExpectedTarget(
            post_scene_draw_call_site, kExpectedUpdaterPostSceneDrawCall) !=
        image + kUpdaterPostSceneDrawRva) {
        error = "The manifest call displacement does not target Updater::OnPostSceneDraw";
        return false;
    }
    std::uint8_t* get_drawer_call_site = image + kGraphicsGetDrawerCallSiteRva;
    if (DecodeExpectedTarget(
            get_drawer_call_site, kExpectedGraphicsGetDrawerCall) !=
        image + kGraphicsGetDrawerRva) {
        error = "The manifest call displacement does not target Graphics::GetDrawer";
        return false;
    }
    std::uint8_t* draw_all_call_site = image + kGraphicsDrawerDrawAllCallSiteRva;
    void* expected_draw_all_target = image + kGraphicsDrawerDrawAllRva;
    if (DecodeExpectedTarget(
            draw_all_call_site, kExpectedGraphicsDrawerDrawAllCall) !=
        expected_draw_all_target) {
        error = "The manifest call displacement does not target GraphicsDrawer::DrawAll";
        return false;
    }
    std::uint8_t* map_start_call_site =
        image + kMapLoadSetStartPosCallSiteRva;
    if (DecodeExpectedTarget(
            map_start_call_site, kExpectedMapLoadSetStartPosCall) !=
        image + kPlayerSetStartPosRva) {
        error = "The map-load call displacement does not target Player::SetStartPos";
        return false;
    }
    std::uint8_t* spawn_yaw_call_site =
        image + kSpawnCameraSetYawCallSiteRva;
    void* const expected_spawn_yaw_target = image + kCameraSetYawRva;
    if (DecodeExpectedTarget(
            spawn_yaw_call_site, kExpectedSpawnCameraSetYawCall) !=
        expected_spawn_yaw_target) {
        error = "The spawn call displacement does not target Camera3D::SetYaw";
        return false;
    }
    if (!std::equal(
            kExpectedCameraYawStore.begin(),
            kExpectedCameraYawStore.end(),
            image + kCameraSetYawRva + 0x0D)) {
        error = "The Camera3D yaw field store does not match the exact build";
        return false;
    }
    if (!std::equal(
            kExpectedCameraPositionStore.begin(),
            kExpectedCameraPositionStore.end(),
            image + kCameraSetPositionRva)) {
        error = "The Camera3D position field does not match the exact build";
        return false;
    }
    auto** const particle_get_model_matrix_slot = reinterpret_cast<void**>(
        image + kParticleGetModelMatrixSlotRva);
    auto** const particle_update_graphics_slot = reinterpret_cast<void**>(
        image + kParticleUpdateGraphicsSlotRva);
    void* const expected_particle_get_model_matrix =
        image + kParticleGetModelMatrixRva;
    void* const expected_particle_update_graphics =
        image + kParticleUpdateGraphicsRva;
    if (*particle_get_model_matrix_slot != expected_particle_get_model_matrix ||
        *particle_update_graphics_slot != expected_particle_update_graphics) {
        error = "The ParticleEmitter3D vtable does not match the exact build";
        return false;
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    g_presentation_timing.Reset();
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_telemetry_dropped_updates.store(0, std::memory_order_release);
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
    g_stereo_room_scale_sample = {};
    g_pending_gameplay_overlay_submission = {};
    g_last_submitted_presentation_sequence.store(0, std::memory_order_release);
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
    g_original_draw_all_target.store(
        expected_draw_all_target, std::memory_order_release);
    g_original_spawn_yaw_target.store(
        expected_spawn_yaw_target, std::memory_order_release);
    g_original_particle_get_model_matrix.store(
        expected_particle_get_model_matrix, std::memory_order_release);
    g_original_particle_update_graphics.store(
        expected_particle_update_graphics, std::memory_order_release);
    g_particle_stereo_refresh.Reset();
    g_particle_update_calls.store(0, std::memory_order_release);
    g_particle_eye_refreshes.store(0, std::memory_order_release);
    g_particle_refresh_misses.store(0, std::memory_order_release);

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
        // A RenderWorld wrapper may already have entered before rollback
        // restored the callsite. Keep its process-resident original targets
        // available so that an in-flight callback can finish safely.
        return false;
    }
    if (!hooks::InstallRel32CallHook(
            draw_all_call_site,
            kExpectedGraphicsDrawerDrawAllCall,
            reinterpret_cast<void*>(&HookedGraphicsDrawerDrawAll),
            g_draw_all_hook,
            error)) {
        std::string visibility_rollback_error;
        std::string render_rollback_error;
        const bool visibility_rolled_back = hooks::RemoveRel32CallHook(
            g_visibility_hook, visibility_rollback_error);
        const bool render_rolled_back = hooks::RemoveRel32CallHook(
            g_hook, render_rollback_error);
        if (!visibility_rolled_back && !visibility_rollback_error.empty()) {
            error += "; UpdateRenderList hook rollback also failed: " +
                visibility_rollback_error;
        }
        if (!render_rolled_back && !render_rollback_error.empty()) {
            error += "; RenderWorld hook rollback also failed: " +
                render_rollback_error;
        }
        return false;
    }
    if (!hooks::InstallRel32CallHook(
            spawn_yaw_call_site,
            kExpectedSpawnCameraSetYawCall,
            reinterpret_cast<void*>(&HookedSpawnCameraSetYaw),
            g_spawn_yaw_hook,
            error)) {
        std::string draw_all_rollback_error;
        std::string visibility_rollback_error;
        std::string render_rollback_error;
        const bool draw_all_rolled_back = hooks::RemoveRel32CallHook(
            g_draw_all_hook, draw_all_rollback_error);
        const bool visibility_rolled_back = hooks::RemoveRel32CallHook(
            g_visibility_hook, visibility_rollback_error);
        const bool render_rolled_back = hooks::RemoveRel32CallHook(
            g_hook, render_rollback_error);
        if (!draw_all_rolled_back && !draw_all_rollback_error.empty()) {
            error += "; DrawAll hook rollback also failed: " +
                draw_all_rollback_error;
        }
        if (!visibility_rolled_back && !visibility_rollback_error.empty()) {
            error += "; UpdateRenderList hook rollback also failed: " +
                visibility_rollback_error;
        }
        if (!render_rolled_back && !render_rollback_error.empty()) {
            error += "; RenderWorld hook rollback also failed: " +
                render_rollback_error;
        }
        return false;
    }
    if (!hooks::InstallOpenGlEyeScissor(error)) {
        std::string rollback_error;
        if (!RemoveRenderWorldProbe(rollback_error)) {
            error += "; render hook rollback also failed: " + rollback_error;
        }
        return false;
    }
    if (!hooks::InstallPointerHook(
            particle_update_graphics_slot,
            expected_particle_update_graphics,
            reinterpret_cast<void*>(&HookedParticleUpdateGraphics),
            g_particle_update_graphics_hook,
            error)) {
        std::string rollback_error;
        if (!RemoveRenderWorldProbe(rollback_error)) {
            error += "; render hook rollback also failed: " + rollback_error;
        }
        return false;
    }
    if (!hooks::InstallPointerHook(
            particle_get_model_matrix_slot,
            expected_particle_get_model_matrix,
            reinterpret_cast<void*>(&HookedParticleGetModelMatrix),
            g_particle_get_model_matrix_hook,
            error)) {
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
    std::string particle_get_model_matrix_error;
    const bool particle_get_model_matrix_removed = hooks::RemoveIatHook(
        g_particle_get_model_matrix_hook, particle_get_model_matrix_error);
    std::string particle_update_graphics_error;
    const bool particle_update_graphics_removed = hooks::RemoveIatHook(
        g_particle_update_graphics_hook, particle_update_graphics_error);
    std::string draw_all_error;
    std::string spawn_yaw_error;
    const bool spawn_yaw_removed =
        hooks::RemoveRel32CallHook(g_spawn_yaw_hook, spawn_yaw_error);
    const bool draw_all_removed =
        hooks::RemoveRel32CallHook(g_draw_all_hook, draw_all_error);
    std::string visibility_error;
    const bool visibility_removed =
        hooks::RemoveRel32CallHook(g_visibility_hook, visibility_error);
    std::string render_error;
    const bool render_removed = hooks::RemoveRel32CallHook(g_hook, render_error);
    if (!particle_get_model_matrix_removed || !particle_update_graphics_removed ||
        !spawn_yaw_removed || !draw_all_removed || !visibility_removed ||
        !render_removed) {
        error = !particle_get_model_matrix_removed
            ? "Could not remove the ParticleEmitter3D GetModelMatrix hook: " +
                particle_get_model_matrix_error
            : !particle_update_graphics_removed
            ? "Could not remove the ParticleEmitter3D UpdateGraphics hook: " +
                particle_update_graphics_error
            : !spawn_yaw_removed
            ? "Could not remove the spawn-yaw hook: " + spawn_yaw_error
            : !draw_all_removed
            ? "Could not remove the GraphicsDrawer::DrawAll hook: " + draw_all_error
            : !visibility_removed
            ? "Could not remove the UpdateRenderList hook: " + visibility_error
            : "Could not remove the RenderWorld hook: " + render_error;
        if (!particle_update_graphics_removed &&
            !particle_get_model_matrix_removed) {
            error += "; UpdateGraphics hook removal also failed: " +
                particle_update_graphics_error;
        }
        if (!spawn_yaw_removed &&
            (!particle_get_model_matrix_removed ||
             !particle_update_graphics_removed)) {
            error += "; spawn-yaw hook removal also failed: " + spawn_yaw_error;
        }
        if (!draw_all_removed &&
            (!particle_get_model_matrix_removed ||
             !particle_update_graphics_removed || !spawn_yaw_removed)) {
            error += "; DrawAll hook removal also failed: " + draw_all_error;
        }
        if (!visibility_removed &&
            (!particle_get_model_matrix_removed ||
             !particle_update_graphics_removed || !spawn_yaw_removed ||
             !draw_all_removed)) {
            error += "; UpdateRenderList hook removal also failed: " + visibility_error;
        }
        if (!render_removed &&
            (!particle_get_model_matrix_removed ||
             !particle_update_graphics_removed || !spawn_yaw_removed ||
             !draw_all_removed || !visibility_removed)) {
            error += "; RenderWorld hook removal also failed: " + render_error;
        }
        return false;
    }

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            // Keep the process-resident native targets published. A thread can
            // already have branched into a wrapper when its callsite is
            // restored but before ActiveCall increments the counter.
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
    g_stereo_room_scale_sample = {};
    const bool result = ValidateControlledStereoMatrices(
        eyes, near_clip, frames, error);
    g_stereo_track_head_rotation = false;
    g_stereo_tracking_anchor_valid = false;
    g_stereo_tracking_anchor = {};
    g_stereo_latest_pose_valid = false;
    g_stereo_latest_pose = {};
    g_stereo_room_scale_sample = {};
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
    g_stereo_room_scale_sample = {};
    g_last_submitted_presentation_sequence.store(0, std::memory_order_release);
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_presentation_timing.ResetHistory();
    ReleaseSRWLockExclusive(&g_telemetry_lock);
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
            g_stereo_room_scale_sample = {};
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
    g_subtitle_scale.store(settings.subtitle_scale, std::memory_order_release);
    g_tracking_height_offset.store(
        settings.height_offset, std::memory_order_release);
    g_tracking_crouch_depth.store(
        settings.physical_crouch_depth, std::memory_order_release);
    g_tracking_play_mode.store(settings.play_mode, std::memory_order_release);
    // Black Plague has no validated pre-tone lighting preparation yet. Its
    // Rework final pass alone makes live scenes too dark and saturated.
    ConfigurePersistentEyeEnhancedVisuals(false);
    g_tracking_player_height.store(
        settings.player_height, std::memory_order_release);
    AcquireSRWLockExclusive(&g_play_mode_lock);
    g_play_mode_policy.Reset();
    ReleaseSRWLockExclusive(&g_play_mode_lock);
}

void PresentTrackedMenuOnRenderThread(bool world_rendered) noexcept {
    if (!TrackedStereoPresentationActive()) return;
    ActiveStereoFrame active;
    if (!TrackedStereoPresentationActive() || g_stereo_cancel.load(std::memory_order_acquire)) return;
    if (world_rendered) {
        std::string error;
        bool monitor_ok = true;
        if (TrackedStereoMonitorMirrorEnabled()) {
            std::array<std::uint32_t,2> textures{};
            monitor_ok = GetPersistentEyeColorTextures(textures,error) &&
                graphics::DrawMonitorMirror(textures[0],error);
        } else {
            monitor_ok = graphics::ClearMonitorBackbuffer(error);
        }
        if (!monitor_ok) {
            TryRecordTelemetry([&]() noexcept {
                strncpy_s(
                    g_telemetry.stereo_error.data(),
                    g_telemetry.stereo_error.size(),
                    error.c_str(),
                    _TRUNCATE);
            });
        }
        g_menu_anchor_valid = runtime::PlanStablePanelAnchor(
            false, false, g_menu_anchor_valid).anchor_valid_after;
        if (!NativeInputUiActive()) {
            AcquireSRWLockExclusive(&g_menu_pointer_lock);
            g_menu_pointer_aspect = 0;
            g_menu_pointer_world_panel=false;
            ReleaseSRWLockExclusive(&g_menu_pointer_lock);
        }
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
    const float menu_scale = g_menu_scale.load(std::memory_order_acquire);
    const auto panel = NativeInputUiSurface() == NativeUiSurface::inventory
        ? BlackPlagueInventoryPanel(menu_scale)
        : BlackPlagueFullscreenPanel(
            menu_scale, g_menu_distance.load(std::memory_order_acquire));
    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    AcquireSRWLockExclusive(&g_menu_pointer_lock);
    g_menu_pointer_anchor = g_menu_anchor;
    g_menu_pointer_world_panel=false;
    g_menu_pointer_aspect = viewport[3] > 0 ? static_cast<float>(viewport[2]) / viewport[3] : 0;
    g_menu_pointer_distance = panel.distance;
    g_menu_pointer_width = panel.width;
    g_menu_pointer_center_y = panel.center_y;
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
            view, g_stereo_projections[i], panel.distance,
            panel.width, panel.center_y, error);
        if (binding.active) {
            std::string restore_error;
            if (!EndPersistentEyeTarget(binding, restore_error)) { success = false; error += restore_error; }
        }
    }
    std::array<std::uint32_t, 2> textures{};
    if (success) success = GetPersistentEyeColorTextures(textures, error) &&
                           session->SubmitOpenGlEyeTextures(textures, error);
    TryRecordTelemetry([&]() noexcept {
        if (success) {
            ++g_telemetry.menu_frames;
            ++g_telemetry.compositor_submitted_frames;
            g_telemetry.compositor_hmd_pose_valid = true;
        } else {
            g_telemetry.stereo_failed = true;
            strncpy_s(
                g_telemetry.stereo_error.data(),
                g_telemetry.stereo_error.size(),
                error.c_str(),
                _TRUNCATE);
        }
    });
}

bool TrackedMenuPointer(const runtime::VrHmdPose& pointer_pose, std::array<float, 2>& uv) noexcept {
    uv = {};
    if (!pointer_pose.pose_valid || !pointer_pose.device_connected) return false;
    AcquireSRWLockShared(&g_menu_pointer_lock);
    const auto anchor = g_menu_pointer_anchor;
    const float aspect = g_menu_pointer_aspect;
    const float distance = g_menu_pointer_distance;
    const float width = g_menu_pointer_width;
    const float center_y = g_menu_pointer_center_y;
    const bool world_panel=g_menu_pointer_world_panel;
    const auto world_from_tracking=g_menu_pointer_world_from_tracking;
    const auto panel_world_pose=g_menu_pointer_world_panel_pose;
    ReleaseSRWLockShared(&g_menu_pointer_lock);
    if (world_panel)
        return runtime::ProjectAimOnWorldPanel(
            world_from_tracking,panel_world_pose,pointer_pose.device_to_absolute,
            aspect,distance,width,uv,center_y);
    return runtime::ProjectAimOnMenu(
        anchor, pointer_pose.device_to_absolute, aspect, distance, width, uv,
        center_y);
}
void RequestTrackedRecenter() noexcept { g_recenter_requested.store(true, std::memory_order_release); }

void AddTrackedWorldYaw(float radians) noexcept {
    if (!std::isfinite(radians) || std::abs(radians) <=
            kNativeYawRebaseEpsilonRadians) {
        return;
    }
    AcquireSRWLockExclusive(&g_tracking_yaw_lock);
    const float before = g_tracking_yaw_space.world_yaw();
    g_tracking_yaw_space.AddWorldYaw(radians);
    const float after = g_tracking_yaw_space.world_yaw();
    const float applied = std::remainder(
        after - before, 6.28318530717958647692F);
    if (std::abs(applied) > kNativeYawRebaseEpsilonRadians) {
        g_presentation_yaw_epoch.fetch_add(1, std::memory_order_acq_rel);
    }
    ReleaseSRWLockExclusive(&g_tracking_yaw_lock);
}

bool TrackedMovementYaw(float& yaw) noexcept {
    AcquireSRWLockShared(&g_world_tracking_lock);
    yaw = g_world_movement_yaw;
    const bool valid = g_world_movement_yaw_valid;
    const auto time = g_world_tracking_time;
    ReleaseSRWLockShared(&g_world_tracking_lock);
    return valid && time != 0 && GetTickCount64() - time <= 250 && std::isfinite(yaw);
}
bool TrackedHeadWorldPose(runtime::VrMatrix44& pose) noexcept {
    AcquireSRWLockShared(&g_world_tracking_lock);
    pose = g_world_head_pose;
    const bool valid = g_world_head_pose_valid;
    const auto time = g_world_tracking_time;
    ReleaseSRWLockShared(&g_world_tracking_lock);
    return valid && time != 0 && GetTickCount64() - time <= 250;
}
bool TrackedHeadTrackingHeight(float& height) noexcept {
    AcquireSRWLockShared(&g_world_tracking_lock);
    height = g_world_anchor.values[7];
    const auto time = g_world_tracking_time;
    ReleaseSRWLockShared(&g_world_tracking_lock);
    return time != 0 && GetTickCount64() - time <= 250 && std::isfinite(height);
}
bool ControllerWorldPose(const runtime::VrHmdPose& controller, runtime::VrMatrix44& pose,
    std::array<float,3>& velocity, std::array<float,3>& angular) noexcept {
    pose = {}; velocity = {}; angular = {};
    if (!TrackedStereoPresentationActive() ||
        (NativeInputUiActive() && NativeInputUiSurface()!=NativeUiSurface::notebook))
        return false;
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
    const PresentationTimingWindow timing =
        g_presentation_timing.ConsumeWindow();
    result.presentation_pose_interval_valid = timing.acquisition_interval_valid;
    result.presentation_pose_interval_ms = timing.acquisition_interval_ms;
    result.presentation_pose_jitter_valid = timing.acquisition_jitter_valid;
    result.presentation_pose_jitter_ms = timing.acquisition_jitter_ms;
    result.presentation_render_age_valid = timing.render_age_valid;
    result.presentation_render_age_ms = timing.render_age_ms;
    result.presentation_submit_age_valid = timing.submit_age_valid;
    result.presentation_submit_age_ms = timing.submit_age_ms;
    result.particle_update_calls =
        g_particle_update_calls.exchange(0, std::memory_order_acq_rel);
    result.particle_eye_refreshes =
        g_particle_eye_refreshes.exchange(0, std::memory_order_acq_rel);
    result.particle_refresh_misses =
        g_particle_refresh_misses.exchange(0, std::memory_order_acq_rel);
    result.dropped_updates =
        g_telemetry_dropped_updates.exchange(0, std::memory_order_acq_rel);
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
