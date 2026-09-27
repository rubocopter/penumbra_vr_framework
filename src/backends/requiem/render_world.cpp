#include "render_world.hpp"

#include "camera_matrix_override.hpp"
#include "frame_presentation_gate.hpp"
#include "opengl_menu_frame.hpp"
#include "opengl_eye_targets.hpp"
#include "opengl_eye_scissor.hpp"
#include "rel32_call_hook.hpp"
#include "stereo_render_policy.hpp"
#include "vr_math.hpp"
#include "vr_panel_policy.hpp"
#include "log.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::requiem {
namespace {

// All addresses/signatures below belong only to the initialized, allowlisted
// Requiem Steam image. The same HPL contracts have different Black Plague RVAs.
constexpr std::uintptr_t kRenderWorldCallRva = 0x000EDE90;
constexpr std::uintptr_t kRenderWorldRva = 0x0012D110;
constexpr std::array<std::uint8_t, 5> kRenderWorldCall{
    0xE8, 0x7B, 0xF2, 0x03, 0x00};
constexpr std::uintptr_t kUpdateRenderListCallRva = 0x000EDE04;
constexpr std::uintptr_t kUpdateRenderListRva = 0x0012AED0;
constexpr std::array<std::uint8_t, 5> kUpdateRenderListCall{
    0xE8, 0xC7, 0xD0, 0x03, 0x00};
constexpr float kVisibilityAngularGuardRadians = 0.087266463F;
constexpr ULONGLONG kMaximumVisibilityPoseAgeMs = 250;
constexpr float kMenuDistance = 1.75F;
constexpr float kMenuWidth = 2.4F;
constexpr float kMenuCenterY = 0.0F;
constexpr adapters::hpl1::CameraLayout kCameraLayout{
    .position_offset = 0x04,
    .fov_offset = 0x10,
    .aspect_offset = 0x14,
    .view_matrix_offset = 0x44,
    .projection_matrix_offset = 0x84,
    .flags_offset = 0x8D0,
    .view_updated_flag_index = 1,
    .projection_updated_flag_index = 2,
};
constexpr float kHplNearClip = 0.05F;

using RenderWorld = void(__thiscall*)(void*, void*, void*, float);
using UpdateRenderList = void(__thiscall*)(void*, void*, void*, float);
hooks::Rel32CallHook g_hook;
hooks::Rel32CallHook g_visibility_hook;
std::atomic<RenderWorld> g_original{nullptr};
std::atomic<UpdateRenderList> g_original_visibility{nullptr};
std::atomic<runtime::OpenVrSession*> g_session{nullptr};
std::atomic<bool> g_presenting{false};
std::atomic<bool> g_destroy_requested{false};
std::atomic<bool> g_targets_destroyed{true};
std::atomic<std::uint32_t> g_active_calls{0};
std::atomic<bool> g_error_logged{false};
std::atomic<bool> g_first_submit_logged{false};
std::atomic<bool> g_first_visibility_logged{false};
std::atomic<bool> g_first_visibility_reuse_logged{false};
std::atomic<bool> g_first_menu_submit_logged{false};
std::atomic<std::uint64_t> g_presentation_generation{0};
FramePresentationGate g_frame_presentation;
graphics::OpenGlEyeTargets g_targets;
std::array<runtime::VrEyeConfiguration, 2> g_eyes{};
std::array<runtime::VrMatrix44, 2> g_projections{};
runtime::VrMatrix34 g_tracking_anchor{};
bool g_tracking_anchor_valid = false; // Render thread only.
runtime::VrMatrix34 g_menu_anchor{};
bool g_menu_anchor_valid = false; // Render thread only.
struct VisibilityPresentation {
    void* renderer = nullptr;
    void* world = nullptr;
    void* camera = nullptr;
    runtime::VrHmdPose pose{};
    ULONGLONG sampled_at_ms = 0;
    std::uint64_t generation = 0;
    bool valid = false;
};
thread_local VisibilityPresentation g_pending_visibility;
thread_local bool g_inside_stereo_eye = false;

class ActiveCall final {
public:
    ActiveCall() noexcept { g_active_calls.fetch_add(1, std::memory_order_acq_rel); }
    ~ActiveCall() { g_active_calls.fetch_sub(1, std::memory_order_acq_rel); }
    ActiveCall(const ActiveCall&) = delete;
    ActiveCall& operator=(const ActiveCall&) = delete;
};

void LogFailureOnce(const char* phase, const std::string& error) noexcept {
    if (!g_error_logged.exchange(true, std::memory_order_acq_rel)) {
        probe::WriteLog("Requiem %s failed: %s", phase, error.c_str());
    }
}

[[nodiscard]] bool LooksLikeMappedGameplayCamera(
    const adapters::hpl1::CameraMatrixSnapshot& snapshot) noexcept {
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

[[nodiscard]] bool ComposeTrackedHeadView(
    const runtime::VrMatrix44& native_view,
    const runtime::VrHmdPose& pose,
    runtime::VrMatrix44& head_view,
    std::string& error) noexcept {
    if (!g_tracking_anchor_valid) {
        g_tracking_anchor = pose.device_to_absolute;
        g_tracking_anchor_valid = true;
    }
    // Positional tracking requires Requiem's accepted-body-motion boundary.
    return runtime::ComposeYawRecenteredTrackedHeadView(
        native_view, g_tracking_anchor, pose.device_to_absolute,
        0.0F, head_view, error);
}

void __fastcall HookedUpdateRenderList(void* renderer, void*,
    void* world, void* camera, float frame_time) noexcept {
    ActiveCall active_call;
    const auto original = g_original_visibility.load(std::memory_order_acquire);
    if (original == nullptr) return;
    if (!g_presenting.load(std::memory_order_acquire)) {
        original(renderer, world, camera, frame_time);
        return;
    }

    const bool inside_eye = g_inside_stereo_eye;
    if (!inside_eye) g_pending_visibility.valid = false;
    adapters::hpl1::CameraMatrixSnapshot camera_snapshot;
    std::string error;
    if (!adapters::hpl1::CaptureCameraMatrices(
            camera, kCameraLayout, camera_snapshot, error)) {
        LogFailureOnce("visibility camera capture", error);
        original(renderer, world, camera, frame_time);
        return;
    }
    if (!LooksLikeMappedGameplayCamera(camera_snapshot)) {
        original(renderer, world, camera, frame_time);
        return;
    }

    runtime::VrHmdPose pose;
    runtime::VrMatrix44 head_view = camera_snapshot.view;
    ULONGLONG sampled_at_ms = 0;
    if (!inside_eye) {
        auto* session = g_session.load(std::memory_order_acquire);
        if (session == nullptr || !session->WaitForHmdPose(pose, error) ||
            !pose.device_connected || !pose.pose_valid ||
            !ComposeTrackedHeadView(
                camera_snapshot.view, pose, head_view, error)) {
            LogFailureOnce("visibility HMD pose", error.empty()
                ? "The HMD pose is not tracked" : error);
            original(renderer, world, camera, frame_time);
            return;
        }
        sampled_at_ms = GetTickCount64();
    }

    runtime::VrCullFrustum frustum;
    if (!runtime::BuildConservativeStereoCullFrustum(
            g_eyes, kVisibilityAngularGuardRadians, frustum, error)) {
        LogFailureOnce("visibility frustum", error);
        original(renderer, world, camera, frame_time);
        return;
    }
    adapters::hpl1::CameraVisibilityOverride override(kCameraLayout);
    if (!override.Apply(camera, head_view, g_projections[0],
            frustum.vertical_fov_radians, frustum.aspect, error)) {
        LogFailureOnce("visibility camera override", error);
        original(renderer, world, camera, frame_time);
        return;
    }
    original(renderer, world, camera, frame_time);
    if (!override.Restore(error)) {
        LogFailureOnce("visibility camera restore", error);
        return;
    }

    if (!inside_eye) {
        g_pending_visibility = {
            renderer, world, camera, pose, sampled_at_ms,
            g_presentation_generation.load(std::memory_order_acquire), true};
        if (!g_first_visibility_logged.exchange(true, std::memory_order_acq_rel)) {
            probe::WriteLog("Requiem first HMD visibility list prepared");
        }
    }
}

class StereoEyeScope final {
public:
    StereoEyeScope() noexcept : previous_(g_inside_stereo_eye) {
        g_inside_stereo_eye = true;
    }
    ~StereoEyeScope() { g_inside_stereo_eye = previous_; }
    StereoEyeScope(const StereoEyeScope&) = delete;
    StereoEyeScope& operator=(const StereoEyeScope&) = delete;
private:
    bool previous_ = false;
};

[[nodiscard]] bool RenderEye(RenderWorld original,
    void* renderer, void* world, void* camera,
    const runtime::VrMatrix44& head_view, std::size_t index,
    float frame_time, bool& world_rendered, std::string& error) noexcept {
    runtime::VrMatrix44 eye_view;
    if (!runtime::ComposeEyeViewFromHeadView(
            head_view, g_eyes[index].eye_to_head, eye_view, error)) return false;

    graphics::OpenGlEyeBinding binding;
    const graphics::Eye eye = index == 0
        ? graphics::Eye::left : graphics::Eye::right;
    if (!g_targets.BeginEye(eye, binding, error)) return false;

    adapters::hpl1::CameraMatrixOverride camera_override(kCameraLayout);
    const bool applied = camera_override.Apply(
        camera, eye_view, g_projections[index], error);
    if (applied) {
        hooks::ScopedEyeScissor scissor({
            binding.previous_viewport[2], binding.previous_viewport[3]});
        StereoEyeScope eye_scope;
        original(renderer, world, camera, frame_time);
        world_rendered = true;
    }

    std::string camera_restore_error;
    const bool camera_restored = !applied ||
        camera_override.Restore(camera_restore_error);
    std::string eye_restore_error;
    const bool eye_restored = g_targets.EndEye(binding, eye_restore_error);
    if (!camera_restored || !eye_restored) {
        error = !camera_restored
            ? "Could not restore Requiem camera: " + camera_restore_error
            : "Could not restore Requiem eye framebuffer: " + eye_restore_error;
    }
    return applied && camera_restored && eye_restored;
}

[[nodiscard]] bool RenderStereo(RenderWorld original,
    void* renderer, void* world, void* camera, float frame_time,
    bool& frame_time_consumed, std::string& error) noexcept {
    auto* session = g_session.load(std::memory_order_acquire);
    if (session == nullptr || wglGetCurrentContext() == nullptr) {
        error = "OpenVR session or OpenGL context is unavailable";
        return false;
    }
    const auto size = session->recommended_render_target_size();
    if (!g_targets.CreateOrResize(size.width, size.height, error)) return false;
    g_targets_destroyed.store(false, std::memory_order_release);

    adapters::hpl1::CameraMatrixSnapshot camera_snapshot;
    if (!adapters::hpl1::CaptureCameraMatrices(
            camera, kCameraLayout, camera_snapshot, error)) return false;
    if (!LooksLikeMappedGameplayCamera(camera_snapshot)) {
        error = "The Requiem world camera is not the mapped perspective camera";
        return false;
    }
    const VisibilityPresentation pending = g_pending_visibility;
    g_pending_visibility.valid = false;
    const ULONGLONG now = GetTickCount64();
    const bool reuse_visibility = pending.valid &&
        pending.renderer == renderer && pending.world == world &&
        pending.camera == camera &&
        pending.generation == g_presentation_generation.load(std::memory_order_acquire) &&
        now >= pending.sampled_at_ms &&
        now - pending.sampled_at_ms <= kMaximumVisibilityPoseAgeMs;
    runtime::VrMatrix44 head_view;
    if (reuse_visibility) {
        // Reuse the compositor pose, but align it to the camera as it stands
        // at RenderWorld. Native mouse yaw may change after visibility setup.
        if (!ComposeTrackedHeadView(camera_snapshot.view,
                pending.pose, head_view, error)) return false;
        if (!g_first_visibility_reuse_logged.exchange(
                true, std::memory_order_acq_rel)) {
            probe::WriteLog("Requiem stereo world reused its HMD visibility pose");
        }
    } else {
        runtime::VrHmdPose pose;
        if (!session->WaitForHmdPose(pose, error)) return false;
        if (!pose.device_connected || !pose.pose_valid) {
            error = "The HMD pose is not tracked";
            return false;
        }
        if (!ComposeTrackedHeadView(
                camera_snapshot.view, pose, head_view, error)) return false;
    }

    const auto plan = runtime::PlanStereoWorldRendering(
        frame_time, true, false);
    for (std::size_t index = 0; index < 2; ++index) {
        bool world_rendered = false;
        if (!RenderEye(original, renderer, world, camera, head_view,
                index, plan.eye_frame_times[index], world_rendered, error)) {
            if (world_rendered && index == 0) frame_time_consumed = true;
            return false;
        }
        if (index == 0) frame_time_consumed = true;
    }
    if (!adapters::hpl1::CameraMatchesSnapshot(
            camera, kCameraLayout, camera_snapshot)) {
        error = "Requiem camera changed across the stereo pair";
        return false;
    }

    const std::array<std::uint32_t, 2> textures{
        g_targets.target(graphics::Eye::left).color_texture,
        g_targets.target(graphics::Eye::right).color_texture};
    if (!session->SubmitOpenGlEyeTextures(textures, error)) return false;
    glFlush();
    if (!g_first_submit_logged.exchange(true, std::memory_order_acq_rel)) {
        probe::WriteLog("Requiem first stereo world pair submitted (%lu x %lu)",
            static_cast<unsigned long>(size.width),
            static_cast<unsigned long>(size.height));
    }
    return true;
}

void __fastcall HookedRenderWorld(void* renderer, void*,
    void* world, void* camera, float frame_time) noexcept {
    ActiveCall active_call;
    const auto original = g_original.load(std::memory_order_acquire);
    if (original == nullptr) return;
    if (!g_presenting.load(std::memory_order_acquire)) {
        original(renderer, world, camera, frame_time);
        return;
    }
    bool frame_time_consumed = false;
    std::string error;
    if (RenderStereo(original, renderer, world, camera, frame_time,
            frame_time_consumed, error)) {
        g_frame_presentation.MarkWorldPresented();
        return;
    }
    LogFailureOnce("stereo world", error);
    original(renderer, world, camera,
        frame_time_consumed ? 0.0F : frame_time);
}

} // namespace

bool InstallRenderWorld(std::string& error) noexcept {
    error.clear();
    if (g_hook.installed() || g_visibility_hook.installed()) {
        error = "Requiem render hooks are already installed";
        return false;
    }
    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "Requiem main image is unavailable";
        return false;
    }
    auto* call = image + kRenderWorldCallRva;
    auto* visibility_call = image + kUpdateRenderListCallRva;
    if (!std::equal(kRenderWorldCall.begin(), kRenderWorldCall.end(), call)) {
        error = "Requiem initialized RenderWorld call does not match the exact build";
        return false;
    }
    if (!std::equal(kUpdateRenderListCall.begin(),
            kUpdateRenderListCall.end(), visibility_call)) {
        error = "Requiem initialized UpdateRenderList call does not match the exact build";
        return false;
    }
    g_original.store(reinterpret_cast<RenderWorld>(image + kRenderWorldRva),
        std::memory_order_release);
    g_original_visibility.store(
        reinterpret_cast<UpdateRenderList>(image + kUpdateRenderListRva),
        std::memory_order_release);
    if (!hooks::InstallRel32CallHook(visibility_call, kUpdateRenderListCall,
            reinterpret_cast<void*>(&HookedUpdateRenderList),
            g_visibility_hook, error)) {
        g_original.store(nullptr, std::memory_order_release);
        g_original_visibility.store(nullptr, std::memory_order_release);
        return false;
    }
    if (!hooks::InstallRel32CallHook(call, kRenderWorldCall,
            reinterpret_cast<void*>(&HookedRenderWorld), g_hook, error)) {
        std::string rollback_error;
        if (!hooks::RemoveRel32CallHook(g_visibility_hook, rollback_error)) {
            error += "; visibility rollback failed: " + rollback_error;
        } else {
            g_original.store(nullptr, std::memory_order_release);
            g_original_visibility.store(nullptr, std::memory_order_release);
        }
        return false;
    }
    return true;
}

bool RenderHooksInstalled() noexcept {
    return g_hook.installed() || g_visibility_hook.installed();
}

bool RemoveRenderWorld(std::string& error) noexcept {
    error.clear();
    if (g_presenting.load(std::memory_order_acquire) ||
        !g_targets_destroyed.load(std::memory_order_acquire)) {
        error = "Requiem presentation still owns OpenGL targets";
        return false;
    }
    if (!hooks::RemoveRel32CallHook(g_hook, error)) return false;
    if (!hooks::RemoveRel32CallHook(g_visibility_hook, error)) return false;
    for (unsigned elapsed = 0; elapsed < 2000; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            g_original.store(nullptr, std::memory_order_release);
            g_original_visibility.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for Requiem RenderWorld callbacks";
    return false;
}

bool StartPresentation(runtime::OpenVrSession& session,
    std::string& error) noexcept {
    error.clear();
    if (!g_hook.installed() || !g_visibility_hook.installed() ||
        !session.initialized()) {
        error = "Requiem render hooks or OpenVR session are unavailable";
        return false;
    }
    if (g_presenting.load(std::memory_order_acquire)) {
        error = "Requiem presentation is already active";
        return false;
    }
    if (!session.ReadEyeConfiguration(g_eyes, error)) return false;
    for (std::size_t index = 0; index < 2; ++index) {
        if (!runtime::BuildHplInfiniteProjection(
                g_eyes[index], kHplNearClip, g_projections[index], error)) {
            return false;
        }
    }
    g_tracking_anchor_valid = false;
    g_menu_anchor_valid = false;
    g_frame_presentation.Reset();
    g_error_logged.store(false, std::memory_order_release);
    g_first_submit_logged.store(false, std::memory_order_release);
    g_first_visibility_logged.store(false, std::memory_order_release);
    g_first_visibility_reuse_logged.store(false, std::memory_order_release);
    g_first_menu_submit_logged.store(false, std::memory_order_release);
    g_presentation_generation.fetch_add(1, std::memory_order_acq_rel);
    g_destroy_requested.store(false, std::memory_order_release);
    g_session.store(&session, std::memory_order_release);
    g_presenting.store(true, std::memory_order_release);
    return true;
}

bool StopPresentation(std::string& error) noexcept {
    error.clear();
    g_presenting.store(false, std::memory_order_release);
    g_destroy_requested.store(true, std::memory_order_release);
    for (unsigned elapsed = 0; elapsed < 2000; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0 &&
            g_targets_destroyed.load(std::memory_order_acquire)) {
            g_session.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for Requiem eye targets to close on the GL thread";
    return false;
}

void OnSdlSwap(std::uint64_t) noexcept {
    if (g_destroy_requested.load(std::memory_order_acquire)) {
        if (g_active_calls.load(std::memory_order_acquire) != 0 ||
            g_targets_destroyed.load(std::memory_order_acquire)) return;
        std::string error;
        if (g_targets.Destroy(error)) {
            g_tracking_anchor_valid = false;
            g_menu_anchor_valid = false;
            g_frame_presentation.Reset();
            g_targets_destroyed.store(true, std::memory_order_release);
        } else {
            LogFailureOnce("eye-target teardown", error);
        }
        return;
    }

    if (!g_presenting.load(std::memory_order_acquire)) return;
    const bool world_presented = g_frame_presentation.ConsumeWorldPresentedAtSwap();
    if (world_presented) {
        g_menu_anchor_valid = runtime::PlanStablePanelAnchor(
            false, false, g_menu_anchor_valid).anchor_valid_after;
        return;
    }

    auto* session = g_session.load(std::memory_order_acquire);
    if (session == nullptr || wglGetCurrentContext() == nullptr) return;

    std::string error;
    runtime::VrHmdPose pose;
    if (!session->WaitForHmdPose(pose, error)) {
        LogFailureOnce("menu HMD pose", error);
        return;
    }
    if (!pose.device_connected || !pose.pose_valid) {
        LogFailureOnce("menu HMD pose", "The HMD pose is not tracked");
        return;
    }

    const auto anchor_plan = runtime::PlanStablePanelAnchor(
        true, false, g_menu_anchor_valid);
    if (anchor_plan.capture_current_pose) {
        g_menu_anchor = pose.device_to_absolute;
    }
    g_menu_anchor_valid = anchor_plan.anchor_valid_after;

    const auto size = session->recommended_render_target_size();
    if (!g_targets.CreateOrResize(size.width, size.height, error)) {
        LogFailureOnce("menu eye targets", error);
        return;
    }
    g_targets_destroyed.store(false, std::memory_order_release);

    runtime::VrMatrix44 head_view;
    if (!runtime::ComposeYawRecenteredTrackedHeadView(
            runtime::IdentityMatrix(), g_menu_anchor,
            pose.device_to_absolute, 1.0F, head_view, error)) {
        LogFailureOnce("menu head view", error);
        return;
    }

    graphics::OpenGlMenuFrame menu;
    if (!menu.Capture(error)) {
        LogFailureOnce("menu capture", error);
        return;
    }

    bool success = true;
    for (std::size_t index = 0; index < 2 && success; ++index) {
        runtime::VrMatrix44 eye_view;
        success = runtime::ComposeEyeViewFromHeadView(
            head_view, g_eyes[index].eye_to_head, eye_view, error);
        graphics::OpenGlEyeBinding binding;
        if (success) {
            success = g_targets.BeginEye(
                index == 0 ? graphics::Eye::left : graphics::Eye::right,
                binding, error);
        }
        if (success) {
            success = menu.Draw(eye_view, g_projections[index],
                kMenuDistance, kMenuWidth, kMenuCenterY, error);
        }
        if (binding.active) {
            std::string restore_error;
            if (!g_targets.EndEye(binding, restore_error)) {
                success = false;
                error = "Could not restore Requiem menu eye framebuffer: " +
                    restore_error;
            }
        }
    }
    if (!success) {
        LogFailureOnce("menu eye render", error);
        return;
    }

    const std::array<std::uint32_t, 2> textures{
        g_targets.target(graphics::Eye::left).color_texture,
        g_targets.target(graphics::Eye::right).color_texture};
    if (!session->SubmitOpenGlEyeTextures(textures, error)) {
        LogFailureOnce("menu submit", error);
        return;
    }
    glFlush();
    if (!g_first_menu_submit_logged.exchange(true, std::memory_order_acq_rel)) {
        probe::WriteLog("Requiem first tracked menu frame submitted (%lu x %lu)",
            static_cast<unsigned long>(size.width),
            static_cast<unsigned long>(size.height));
    }
}

} // namespace penumbra_vr::backends::requiem
