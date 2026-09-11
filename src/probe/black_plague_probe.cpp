#include "log.hpp"
#include "penumbra_vr/build_catalog.hpp"
#include "opengl_matrix_telemetry.hpp"
#include "openvr_session.hpp"
#include "render_target_policy.hpp"
#include "render_world_probe.hpp"
#include "native_input_bridge.hpp"
#include "body_collision_probe.hpp"
#include "black_plague_body_adapter.hpp"
#include "movement_ownership_probe.hpp"
#include "spatial_interaction.hpp"
#include "sdl_frame_hook.hpp"
#include "vr_math.hpp"
#include "vr_settings_store.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

namespace {

volatile LONG g_state = 0;
volatile LONG g_presentation_state = 0;
HINSTANCE g_instance = nullptr;
penumbra_vr::runtime::OpenVrSession g_openvr_session;
penumbra_vr::runtime::VrSettings g_vr_settings;

std::wstring OpenVrLoaderPath() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        g_instance, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }
    path.resize(length);
    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return {};
    }
    path.resize(separator + 1);
    path += L"openvr_api.dll";
    return path;
}

const char* FramebufferApiName(
    penumbra_vr::backends::black_plague::FramebufferApi api) noexcept {
    using penumbra_vr::backends::black_plague::FramebufferApi;
    switch (api) {
        case FramebufferApi::core:
            return "core";
        case FramebufferApi::ext:
            return "EXT";
        default:
            return "unavailable";
    }
}

const char* EyeTargetEventName(
    penumbra_vr::backends::black_plague::EyeTargetProbeEvent event) noexcept {
    using penumbra_vr::backends::black_plague::EyeTargetProbeEvent;
    switch (event) {
        case EyeTargetProbeEvent::transient_validation:
            return "transient_validation";
        case EyeTargetProbeEvent::persistent_created:
            return "persistent_created";
        case EyeTargetProbeEvent::persistent_destroyed:
            return "persistent_destroyed";
        default:
            return "none";
    }
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "<path conversion failed>";
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void OnFrame(std::uint64_t frame_number) noexcept {
    if (frame_number%300==0) {
        const auto spatial=penumbra_vr::backends::black_plague::ConsumeSpatialDiagnostics();
        penumbra_vr::probe::WriteLog("spatial tools_attached=%llu tools_native=%llu invalid_tool_pose=%llu blocked_unsafe_grabs=%llu grabs_acquired=%llu grabs_released=%llu guarded_releases=%llu collision_restore_failures=%llu contact_rays=%llu contact_reach_m=0.180",
            static_cast<unsigned long long>(spatial.tools_attached),static_cast<unsigned long long>(spatial.tools_native),
            static_cast<unsigned long long>(spatial.invalid_tool_pose),static_cast<unsigned long long>(spatial.blocked_grabs),
            static_cast<unsigned long long>(spatial.grabs_acquired),static_cast<unsigned long long>(spatial.grabs_released),
            static_cast<unsigned long long>(spatial.guarded_releases),static_cast<unsigned long long>(spatial.collision_restore_failures),
            static_cast<unsigned long long>(spatial.contact_rays));
    }
    const auto timing=penumbra_vr::backends::black_plague::ConsumeNativeUpdateTiming();
    if (timing.ready) penumbra_vr::probe::WriteLog(
        "native_update_timing ticks=%llu simulation_seconds=%.4f wall_seconds=%.4f ratio=%.4f (diagnostic only)",
        static_cast<unsigned long long>(timing.ticks),timing.simulated_seconds,timing.wall_seconds,timing.ratio);
    // The main menu has no RenderWorld calls. Service allocation requests here
    // too so startup does not require a loaded map. Do not double-count frames.
    penumbra_vr::backends::black_plague::ProcessEyeTargetRequestsOnRenderThread(false);
    const penumbra_vr::backends::black_plague::RenderWorldFrameTelemetry render_world =
        penumbra_vr::backends::black_plague::ConsumeRenderWorldFrameTelemetry();
    penumbra_vr::backends::black_plague::PresentTrackedMenuOnRenderThread(render_world.calls != 0 &&
        !penumbra_vr::backends::black_plague::NativeInputUiActive());
    const penumbra_vr::hooks::OpenGlFrameTelemetry telemetry =
        penumbra_vr::hooks::ConsumeOpenGlFrameTelemetry();
    const bool bounded_stereo_activity =
        !render_world.persistent_stereo_active &&
        (render_world.stereo_eye_passes != 0 ||
         render_world.compositor_submitted_frames != 0);
    const bool jump_burst_complete =
        penumbra_vr::backends::black_plague::IsBodyJumpBurstComplete();
    if (frame_number % 300 == 0) {
        const auto shadow = penumbra_vr::backends::black_plague::
            ConsumeBlackPlagueShadowTelemetry();
        if (shadow.observed_ticks != 0) {
            const auto& s = shadow.latest;
            penumbra_vr::probe::WriteLog(
                "body_reconciliation_shadow ticks=%llu resets=%llu valid=%u "
                "physical_delta=[%.4f,%.4f] physical_plan=[%.4f,%.4f] "
                "native_accepted=[%.4f,%.4f,%.4f] body=[%.4f,%.4f,%.4f] "
                "predicted_anchor=[%.4f,%.4f,%.4f] native_correction=[%.4f,%.4f] "
                "separation=%.4f rebase=%u physical_observation=0 positional_translation_enabled=0",
                static_cast<unsigned long long>(shadow.observed_ticks),
                static_cast<unsigned long long>(shadow.resets), s.valid ? 1U : 0U,
                s.physical_delta[0], s.physical_delta[2],
                s.plan.physical_request[0], s.plan.physical_request[2],
                s.native_motion.accepted_displacement[0],
                s.native_motion.accepted_displacement[1],
                s.native_motion.accepted_displacement[2],
                s.native_motion.body_after[0], s.native_motion.body_after[1],
                s.native_motion.body_after[2],
                s.predicted_anchor[0], s.predicted_anchor[1], s.predicted_anchor[2],
                s.native_anchor_correction[0], s.native_anchor_correction[2],
                s.separation, s.plan.rebased ? 1U : 0U);
        }
    }
    if (frame_number <= 10 || frame_number % 300 == 0 ||
        bounded_stereo_activity ||
        jump_burst_complete ||
        render_world.stereo_failed ||
        !render_world.stereo_camera_restored ||
        render_world.hmd_visibility_failures != 0 ||
        render_world.eye_targets.event !=
            penumbra_vr::backends::black_plague::EyeTargetProbeEvent::none) {
        penumbra_vr::probe::WriteLog(
            "frame=%llu render_world_calls=%lu renderer=%p world=%p camera=%p frame_time=%.6f "
            "gl_context=%u gl_version=%s framebuffer_api=%s viewport=[%ld,%ld,%ld,%ld] "
            "framebuffer=%ld max_texture=%ld max_renderbuffer=%ld max_viewport=[%ld,%ld] "
            "persistent_eye_targets=%u persistent_size=%lux%lu persistent_frames=%llu "
            "stereo_frames=%lu menu_frames=%lu stereo_eye_passes=%lu stereo_lifetime_frames=%llu "
            "stereo_camera_restored=%u "
            "submitted_frames=%lu submitted_pose_valid=%u "
            "tracked_head_frames=%lu tracking_anchor_captured=%u "
            "hmd_anchor_m=[%.4f,%.4f,%.4f] hmd_position_m=[%.4f,%.4f,%.4f] "
            "hmd_horizontal_delta_m=%.4f positional_world_units_per_meter=%.3f "
            "persistent_stereo_active=%u monitor_mirror=%u monitor_world_passes=%lu "
            "suppressed_monitor_world_passes=%lu eye_owned_frame_time_frames=%lu "
            "stereo_failed=%u stereo_error=%s "
            "hmd_visibility_updates=%lu hmd_visibility_failures=%lu "
            "hmd_visibility_camera_restored=%u hmd_visibility_error=%s "
            "eye_scissor_remapped=%lu eye_scissor_bypassed=%lu "
            "controller_samples=%lu controller_failures=%lu controller_focus=%u "
            "controller_grips=%u controller_aims=%u controller_move=[%.3f,%.3f] controller_turn=%.3f controller_error=%s "
            "matrix_modes=%lu projection_loads=%lu model_view_loads=%lu "
            "model_view_unique=%lu model_view_dropped=%lu texture_loads=%lu ortho_calls=%lu",
            frame_number,
            static_cast<unsigned long>(render_world.calls),
            reinterpret_cast<void*>(render_world.renderer),
            reinterpret_cast<void*>(render_world.world),
            reinterpret_cast<void*>(render_world.camera),
            render_world.frame_time,
            render_world.has_current_gl_context ? 1U : 0U,
            render_world.open_gl_version.data(),
            FramebufferApiName(render_world.framebuffer_api),
            static_cast<long>(render_world.viewport[0]),
            static_cast<long>(render_world.viewport[1]),
            static_cast<long>(render_world.viewport[2]),
            static_cast<long>(render_world.viewport[3]),
            static_cast<long>(render_world.framebuffer_binding),
            static_cast<long>(render_world.max_texture_size),
            static_cast<long>(render_world.max_renderbuffer_size),
            static_cast<long>(render_world.max_viewport_dimensions[0]),
            static_cast<long>(render_world.max_viewport_dimensions[1]),
            render_world.eye_targets.persistent_active ? 1U : 0U,
            static_cast<unsigned long>(render_world.eye_targets.width),
            static_cast<unsigned long>(render_world.eye_targets.height),
            render_world.eye_targets.persistent_frames,
            static_cast<unsigned long>(render_world.stereo_frames),
            static_cast<unsigned long>(render_world.menu_frames),
            static_cast<unsigned long>(render_world.stereo_eye_passes),
            render_world.stereo_lifetime_frames,
            render_world.stereo_camera_restored ? 1U : 0U,
            static_cast<unsigned long>(render_world.compositor_submitted_frames),
            render_world.compositor_hmd_pose_valid ? 1U : 0U,
            static_cast<unsigned long>(render_world.tracked_head_frames),
            render_world.tracking_anchor_captured ? 1U : 0U,
            render_world.hmd_tracking_anchor_m[0],
            render_world.hmd_tracking_anchor_m[1],
            render_world.hmd_tracking_anchor_m[2],
            render_world.hmd_tracking_position_m[0],
            render_world.hmd_tracking_position_m[1],
            render_world.hmd_tracking_position_m[2],
            render_world.hmd_horizontal_delta_m,
            render_world.positional_world_units_per_meter,
            render_world.persistent_stereo_active ? 1U : 0U,
            render_world.monitor_mirror_enabled ? 1U : 0U,
            static_cast<unsigned long>(render_world.monitor_world_passes),
            static_cast<unsigned long>(
                render_world.suppressed_monitor_world_passes),
            static_cast<unsigned long>(render_world.eye_owned_frame_time_frames),
            render_world.stereo_failed ? 1U : 0U,
            render_world.stereo_error.data(),
            static_cast<unsigned long>(render_world.hmd_visibility_updates),
            static_cast<unsigned long>(render_world.hmd_visibility_failures),
            render_world.hmd_visibility_camera_restored ? 1U : 0U,
            render_world.hmd_visibility_error.data(),
            static_cast<unsigned long>(render_world.eye_scissor_remapped),
            static_cast<unsigned long>(render_world.eye_scissor_bypassed),
            static_cast<unsigned long>(render_world.controller_samples),
            static_cast<unsigned long>(render_world.controller_failures),
            render_world.controller_frame.focused ? 1U : 0U,
            (render_world.controller_frame.hands[0].grip.pose_valid ? 1U : 0U) |
                (render_world.controller_frame.hands[1].grip.pose_valid ? 2U : 0U),
            (render_world.controller_frame.hands[0].aim.pose_valid ? 1U : 0U) |
                (render_world.controller_frame.hands[1].aim.pose_valid ? 2U : 0U),
            render_world.controller_frame.input.state.move.x,
            render_world.controller_frame.input.state.move.y,
            render_world.controller_frame.input.state.turn.x,
            render_world.controller_error.data(),
            static_cast<unsigned long>(telemetry.matrix_mode_calls),
            static_cast<unsigned long>(telemetry.projection_loads),
            static_cast<unsigned long>(telemetry.model_view_loads),
            static_cast<unsigned long>(telemetry.unique_model_view_matrices),
            static_cast<unsigned long>(telemetry.dropped_model_view_matrices),
            static_cast<unsigned long>(telemetry.texture_loads),
            static_cast<unsigned long>(telemetry.ortho_calls));
        const auto body =
            penumbra_vr::backends::black_plague::ConsumeBodyCollisionTelemetry();
        if (body.valid) {
            const float head_body_x = render_world.hmd_tracking_position_m[0] -
                render_world.hmd_tracking_anchor_m[0];
            const float head_body_z = render_world.hmd_tracking_position_m[2] -
                render_world.hmd_tracking_anchor_m[2];
            penumbra_vr::probe::WriteLog(
                "body_collision updates=%llu horizontal_collision_requests=%llu "
                "player=%p character_body=%p physics_body=%p physics_world=%p "
                "dt=%.6f character_size=[%.4f,%.4f,%.4f] "
                "native_shape=%s shape_radius=%.4f "
                "body_before=[%.4f,%.4f,%.4f] body_after=[%.4f,%.4f,%.4f] "
                "feet_before=[%.4f,%.4f,%.4f] feet_after=[%.4f,%.4f,%.4f] "
                "collision_sample_valid=%u requested_delta=[%.5f,%.5f,%.5f] "
                "solver_delta=[%.5f,%.5f,%.5f] accepted_delta=[%.5f,%.5f,%.5f] "
                "unapplied_hmd_body_divergence_m=[%.5f,%.5f] "
                "unapplied_hmd_body_divergence_horizontal_m=%.5f "
                "positional_translation_enabled=0",
                static_cast<unsigned long long>(body.character_updates),
                static_cast<unsigned long long>(
                    body.horizontal_collision_requests),
                reinterpret_cast<void*>(body.player),
                reinterpret_cast<void*>(body.character_body),
                reinterpret_cast<void*>(body.physics_body),
                reinterpret_cast<void*>(body.physics_world),
                body.delta_seconds,
                body.character_size[0], body.character_size[1],
                body.character_size[2],
                body.native_shape_is_cylinder ? "cylinder" : "sphere",
                body.shape_radius,
                body.body_position_before[0], body.body_position_before[1],
                body.body_position_before[2], body.body_position_after[0],
                body.body_position_after[1], body.body_position_after[2],
                body.feet_position_before[0], body.feet_position_before[1],
                body.feet_position_before[2], body.feet_position_after[0],
                body.feet_position_after[1], body.feet_position_after[2],
                body.collision_sample_valid ? 1U : 0U,
                body.requested_displacement[0],
                body.requested_displacement[1],
                body.requested_displacement[2],
                body.collision_resolved_displacement[0],
                body.collision_resolved_displacement[1],
                body.collision_resolved_displacement[2],
                body.accepted_displacement[0],
                body.accepted_displacement[1],
                body.accepted_displacement[2],
                head_body_x, head_body_z,
                render_world.hmd_horizontal_delta_m);
        }
        if (jump_burst_complete) {
            const auto burst =
                penumbra_vr::backends::black_plague::ConsumeBodyJumpBurstTelemetry();
            penumbra_vr::probe::WriteLog(
                "body_jump_burst samples=%llu sequence_range=[%llu,%llu]",
                static_cast<unsigned long long>(burst.count),
                static_cast<unsigned long long>(burst.samples[0].body_update_sequence),
                static_cast<unsigned long long>(burst.samples[burst.count - 1].body_update_sequence));
            for (std::size_t index = 0; index < burst.count; ++index) {
                const auto& sample = burst.samples[index];
                const auto& body_sample = sample.body;
                const float vertical_speed_from_accepted_delta =
                    body_sample.delta_seconds > 0.0F
                    ? body_sample.accepted_displacement[1] / body_sample.delta_seconds
                    : 0.0F;
                penumbra_vr::probe::WriteLog(
                    "body_jump_tick sequence=%llu dt=%.6f player=%p body=%p "
                    "body=[%.5f,%.5f,%.5f] feet=[%.5f,%.5f,%.5f] "
                    "requested=[%.5f,%.5f,%.5f] solver=[%.5f,%.5f,%.5f] accepted=[%.5f,%.5f,%.5f] "
                    "vertical_speed_from_accepted_delta=%.5f player_268=%ld player_26c=%u "
                    "jump=[down_1fc=%u count_200=%.6f max_204=%.6f] move_state=[index_2d0=%ld object=%p]",
                    static_cast<unsigned long long>(sample.body_update_sequence),
                    body_sample.delta_seconds,
                    reinterpret_cast<void*>(body_sample.player),
                    reinterpret_cast<void*>(body_sample.character_body),
                    body_sample.body_position_after[0], body_sample.body_position_after[1], body_sample.body_position_after[2],
                    body_sample.feet_position_after[0], body_sample.feet_position_after[1], body_sample.feet_position_after[2],
                    body_sample.requested_displacement[0], body_sample.requested_displacement[1], body_sample.requested_displacement[2],
                    body_sample.collision_resolved_displacement[0], body_sample.collision_resolved_displacement[1], body_sample.collision_resolved_displacement[2],
                    body_sample.accepted_displacement[0], body_sample.accepted_displacement[1], body_sample.accepted_displacement[2],
                    vertical_speed_from_accepted_delta,
                    static_cast<long>(sample.player_268),
                    static_cast<unsigned int>(sample.player_26c),
                    static_cast<unsigned int>(sample.jump_button_down_1fc),
                    sample.jump_count_200, sample.max_jump_count_204,
                    static_cast<long>(sample.move_state_index_2d0),
                    reinterpret_cast<void*>(sample.move_state));
            }
        }
        const auto ownership = penumbra_vr::backends::black_plague::ConsumeMovementOwnershipTelemetry();
        if (ownership.valid) penumbra_vr::probe::WriteLog(
            "movement_ownership sequence=%llu jump=%llu hold=%llu sprint=[%llu,%llu] crouch=[pressed=%llu release_or_not_held=%llu] gravity_disabled_sync=[camera=%llu entity=%llu] player=%p body=%p character_camera=%p",
            static_cast<unsigned long long>(ownership.sequence), static_cast<unsigned long long>(ownership.jump), static_cast<unsigned long long>(ownership.jump_hold), static_cast<unsigned long long>(ownership.sprint_start), static_cast<unsigned long long>(ownership.sprint_stop), static_cast<unsigned long long>(ownership.crouch_pressed), static_cast<unsigned long long>(ownership.crouch_release_or_not_held), static_cast<unsigned long long>(ownership.gravity_disabled_camera_sync), static_cast<unsigned long long>(ownership.gravity_disabled_entity_sync), reinterpret_cast<void*>(ownership.player), reinterpret_cast<void*>(ownership.body), reinterpret_cast<void*>(ownership.camera));
        if (telemetry.has_projection) {
            const auto& m = telemetry.last_projection;
            penumbra_vr::probe::WriteLog(
                "projection=[%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] "
                "[%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f]",
                m[0], m[1], m[2], m[3],
                m[4], m[5], m[6], m[7],
                m[8], m[9], m[10], m[11],
                m[12], m[13], m[14], m[15]);
            const auto& stack = telemetry.projection_call_stack;
            penumbra_vr::probe::WriteLog(
                "projection_call_stack_depth=%u frames=%p,%p,%p,%p,%p,%p,%p,%p",
                static_cast<unsigned int>(telemetry.projection_call_stack_depth),
                reinterpret_cast<void*>(stack[0]),
                reinterpret_cast<void*>(stack[1]),
                reinterpret_cast<void*>(stack[2]),
                reinterpret_cast<void*>(stack[3]),
                reinterpret_cast<void*>(stack[4]),
                reinterpret_cast<void*>(stack[5]),
                reinterpret_cast<void*>(stack[6]),
                reinterpret_cast<void*>(stack[7]));
        }
        if (telemetry.has_dominant_model_view) {
            const auto& m = telemetry.dominant_model_view;
            penumbra_vr::probe::WriteLog(
                "dominant_model_view_loads=%lu matrix=[%.6f %.6f %.6f %.6f] "
                "[%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] "
                "[%.6f %.6f %.6f %.6f]",
                static_cast<unsigned long>(telemetry.dominant_model_view_loads),
                m[0], m[1], m[2], m[3],
                m[4], m[5], m[6], m[7],
                m[8], m[9], m[10], m[11],
                m[12], m[13], m[14], m[15]);
        }
        if (render_world.eye_targets.event !=
            penumbra_vr::backends::black_plague::EyeTargetProbeEvent::none) {
            penumbra_vr::probe::WriteLog(
                "eye_target_event=%s result=%s state_restored=%u size=%lux%lu error=%s",
                EyeTargetEventName(render_world.eye_targets.event),
                render_world.eye_targets.event_passed ? "passed" : "failed",
                render_world.eye_targets.state_restored ? 1U : 0U,
                static_cast<unsigned long>(render_world.eye_targets.width),
                static_cast<unsigned long>(render_world.eye_targets.height),
                render_world.eye_targets.error.data());
        }
    }
}

enum class StereoExperiment : std::uint8_t {
    offscreen_matrices,
    compositor_submission,
    tracked_compositor_submission,
};

[[nodiscard]] bool CreateAdaptiveEyeTargets(
    penumbra_vr::runtime::VrRenderTargetSize recommended,
    float render_scale,
    penumbra_vr::runtime::VrRenderTargetSize& selected,
    std::string& error) noexcept {
    selected = {};
    std::vector<penumbra_vr::runtime::VrRenderTargetSize> candidates;
    penumbra_vr::runtime::RenderTargetPolicy policy;
    policy.scale = render_scale;
    if (!penumbra_vr::runtime::BuildRenderTargetCandidates(
            recommended, policy, candidates, error)) {
        return false;
    }

    std::string last_error;
    for (const auto& candidate : candidates) {
        penumbra_vr::probe::WriteLog(
            "Trying VR eye targets at %lux%lu (recommended %lux%lu x %.2f)",
            static_cast<unsigned long>(candidate.width),
            static_cast<unsigned long>(candidate.height),
            static_cast<unsigned long>(recommended.width),
            static_cast<unsigned long>(recommended.height),
            render_scale);
        if (penumbra_vr::backends::black_plague::RequestPersistentEyeTargets(
                candidate.width, candidate.height, last_error)) {
            selected = candidate;
            return true;
        }
        penumbra_vr::probe::WriteLog(
            "VR eye-target allocation at %lux%lu failed: %s",
            static_cast<unsigned long>(candidate.width),
            static_cast<unsigned long>(candidate.height),
            last_error.c_str());
    }

    error = "All adaptive VR eye-target allocation attempts failed";
    if (!last_error.empty()) {
        error += ": " + last_error;
    }
    return false;
}

[[nodiscard]] bool StartPersistentVrPresentation(std::string& error) noexcept {
    error.clear();
    if (g_openvr_session.initialized() ||
        penumbra_vr::backends::black_plague::PersistentEyeTargetsActive()) {
        error = "Persistent VR presentation requires an idle OpenVR and eye-target state";
        return false;
    }

    const std::wstring loader_path = OpenVrLoaderPath();
    if (loader_path.empty()) {
        error = "Could not resolve the probe directory";
        return false;
    }
    if (!g_openvr_session.Initialize(loader_path, error)) {
        return false;
    }

    const std::wstring manifest_path = loader_path.substr(0, loader_path.find_last_of(L"\\/") + 1) + L"vr\\actions.json";
    std::string input_error;
    const bool input_ready = g_openvr_session.InitializeControllerInput(manifest_path, input_error);
    penumbra_vr::probe::WriteLog("controller_actions_initialized=%u error=%s",
        input_ready ? 1U : 0U, input_error.c_str());

    std::array<penumbra_vr::runtime::VrEyeConfiguration, 2> eyes{};
    bool success = g_openvr_session.ReadEyeConfiguration(eyes, error);
    penumbra_vr::runtime::VrRenderTargetSize selected;
    if (success) {
        success = CreateAdaptiveEyeTargets(
            g_openvr_session.recommended_render_target_size(),
            g_vr_settings.render_scale,
            selected,
            error);
    }
    if (success) {
        success = penumbra_vr::backends::black_plague::
            StartTrackedStereoPresentation(
                g_openvr_session, eyes, 0.05F, error);
    }
    if (success) {
        if (input_ready) penumbra_vr::backends::black_plague::ConnectNativeInput(&g_openvr_session);
        penumbra_vr::probe::WriteLog(
            "Persistent tracked VR presentation started at %lux%lu per eye; monitor mirror %s",
            static_cast<unsigned long>(selected.width),
            static_cast<unsigned long>(selected.height),
            penumbra_vr::backends::black_plague::
                    TrackedStereoMonitorMirrorEnabled()
                ? "enabled"
                : "disabled");
        return true;
    }

    const std::string operation_error = error;
    std::string cleanup_error;
    if (!penumbra_vr::backends::black_plague::DestroyPersistentEyeTargets(
            cleanup_error)) {
        error = operation_error + "; eye-target cleanup also failed: " + cleanup_error;
    }
    cleanup_error.clear();
    if (!g_openvr_session.Shutdown(cleanup_error)) {
        error = operation_error + "; OpenVR cleanup also failed: " + cleanup_error;
    } else if (error.empty()) {
        error = operation_error;
    }
    return false;
}

[[nodiscard]] bool StopPersistentVrPresentation(std::string& error) noexcept {
    penumbra_vr::backends::black_plague::ConnectNativeInput(nullptr);
    error.clear();
    bool success = penumbra_vr::backends::black_plague::
        StopTrackedStereoPresentation(error);
    // Never destroy eye textures or shut down OpenVR while a frame is in flight.
    if (!success) return false;
    const std::string operation_error = error;

    std::string cleanup_error;
    if (!penumbra_vr::backends::black_plague::DestroyPersistentEyeTargets(
            cleanup_error)) {
        error = operation_error.empty()
            ? "Eye-target cleanup failed: " + cleanup_error
            : operation_error + "; eye-target cleanup also failed: " + cleanup_error;
        success = false;
    }
    cleanup_error.clear();
    if (!g_openvr_session.Shutdown(cleanup_error)) {
        if (!error.empty()) {
            error += "; ";
        }
        error += "OpenVR cleanup failed: " + cleanup_error;
        success = false;
    }
    return success;
}

[[nodiscard]] bool RunStereoExperiment(
    StereoExperiment experiment,
    std::uint32_t frames,
    std::string& error) noexcept {
    error.clear();
    const bool submit = experiment != StereoExperiment::offscreen_matrices;
    const bool track_head =
        experiment == StereoExperiment::tracked_compositor_submission;
    if (g_openvr_session.initialized() ||
        penumbra_vr::backends::black_plague::PersistentEyeTargetsActive()) {
        error = "The stereo experiment requires an idle OpenVR and eye-target state";
        return false;
    }

    const std::wstring loader_path = OpenVrLoaderPath();
    if (loader_path.empty()) {
        error = "Could not resolve the probe directory";
        return false;
    }
    if (!g_openvr_session.Initialize(loader_path, error)) {
        return false;
    }

    std::array<penumbra_vr::runtime::VrEyeConfiguration, 2> eyes{};
    bool success = g_openvr_session.ReadEyeConfiguration(eyes, error);
    if (success) {
        for (std::size_t index = 0; index < eyes.size(); ++index) {
            const auto& eye = eyes[index];
            penumbra_vr::runtime::VrMatrix44 projection;
            std::string projection_error;
            const bool projection_ready =
                penumbra_vr::runtime::BuildHplInfiniteProjection(
                    eye, 0.05F, projection, projection_error);
            penumbra_vr::probe::WriteLog(
                "stereo_eye=%s tangents=[%.6f %.6f %.6f %.6f] "
                "eye_to_head_translation=[%.6f %.6f %.6f] "
                "projection_ready=%u projection_x=[%.6f %.6f] error=%s",
                index == 0 ? "left" : "right",
                eye.left_tangent,
                eye.right_tangent,
                eye.top_tangent,
                eye.bottom_tangent,
                eye.eye_to_head.values[3],
                eye.eye_to_head.values[7],
                eye.eye_to_head.values[11],
                projection_ready ? 1U : 0U,
                projection.values[0],
                projection.values[2],
                projection_error.c_str());
            if (!projection_ready) {
                error = projection_error;
                success = false;
                break;
            }
        }
    }

    if (success) {
        success =
            penumbra_vr::backends::black_plague::RequestPersistentEyeTargets(
                512, 512, error);
    }
    if (success) {
        if (track_head) {
            success = penumbra_vr::backends::black_plague::
                ValidateControlledTrackedStereoSubmission(
                    g_openvr_session, eyes, 0.05F, frames, error);
        } else if (submit) {
            success = penumbra_vr::backends::black_plague::
                ValidateControlledStereoSubmission(
                    g_openvr_session, eyes, 0.05F, frames, error);
        } else {
            success = penumbra_vr::backends::black_plague::
                ValidateControlledStereoMatrices(
                    eyes, 0.05F, frames, error);
        }
    }

    const std::string operation_error = error;
    std::string cleanup_error;
    if (!penumbra_vr::backends::black_plague::DestroyPersistentEyeTargets(
            cleanup_error)) {
        if (success) {
            error = "Stereo target cleanup failed: " + cleanup_error;
        } else {
            error = operation_error + "; stereo target cleanup also failed: " +
                cleanup_error;
        }
        success = false;
    }
    cleanup_error.clear();
    if (!g_openvr_session.Shutdown(cleanup_error)) {
        if (success) {
            error = "OpenVR cleanup failed: " + cleanup_error;
        } else {
            error += "; OpenVR cleanup also failed: " + cleanup_error;
        }
        success = false;
    }
    if (!success && error.empty()) {
        error = operation_error.empty()
            ? "The stereo experiment failed without an error description"
            : operation_error;
    }
    if (success) {
        penumbra_vr::probe::WriteLog(
            "%s passed for %lu frames with camera restoration after every eye",
            track_head ? "Controlled recentered rotation-only OpenVR stereo submission" :
            submit ? "Controlled OpenVR stereo submission" :
                     "Controlled per-eye HPL matrix validation",
            static_cast<unsigned long>(frames));
    }
    return success;
}

} // namespace

extern "C" DWORD WINAPI PenumbraVR_Initialize(void*) {
    if (InterlockedCompareExchange(&g_state, 1, 0) != 0) {
        return g_state == 2 ? 1UL : 0UL;
    }

    std::wstring log_path;
    std::wstring error;
    if (!penumbra_vr::probe::OpenLog(log_path, error)) {
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    wchar_t executable_path[MAX_PATH]{};
    const DWORD path_length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (path_length == 0 || path_length >= MAX_PATH) {
        penumbra_vr::probe::WriteLog("GetModuleFileNameW failed with Win32 error %lu", GetLastError());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    std::string sha256;
    if (!penumbra_vr::ComputeFileSha256(executable_path, sha256, error)) {
        penumbra_vr::probe::WriteLog("Executable hashing failed: %s", WideToUtf8(error).c_str());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    const penumbra_vr::KnownBuild* build = penumbra_vr::FindKnownBuild(sha256);
    penumbra_vr::probe::WriteLog(
        "Host path=%s module_base=%p sha256=%s",
        WideToUtf8(executable_path).c_str(),
        GetModuleHandleW(nullptr),
        sha256.c_str());
    if (build == nullptr || build->game != penumbra_vr::GameId::black_plague ||
        !build->black_plague_probe_allowed) {
        penumbra_vr::probe::WriteLog("Refusing to hook an unknown or non-Black-Plague host");
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    std::wstring settings_error;
    const auto settings_path =
        penumbra_vr::launcher::DefaultVrSettingsPath(settings_error);
    if (settings_path.empty() || !penumbra_vr::launcher::LoadVrSettings(
            settings_path, g_vr_settings, settings_error)) {
        penumbra_vr::probe::WriteLog("VR input settings fallback to defaults: %s",
            WideToUtf8(settings_error).c_str());
        g_vr_settings = {};
    }
    penumbra_vr::backends::black_plague::ConfigureNativeInputBridge(g_vr_settings);
    penumbra_vr::backends::black_plague::ConfigureTrackedPresentation(g_vr_settings);
    penumbra_vr::probe::WriteLog(
        "VR input profile handedness=%s move_speed=%.3f move_dead_zone=%.3f turn_mode=%.*s snap_angle=%.1f smooth_speed=%.1f turn_dead_zone=%.3f",
        g_vr_settings.handedness == penumbra_vr::runtime::VrHandedness::left ?
            "left" : "right",
        g_vr_settings.move_speed, g_vr_settings.move_dead_zone,
        static_cast<int>(penumbra_vr::runtime::ToConfigValue(g_vr_settings.turn_mode).size()),
        penumbra_vr::runtime::ToConfigValue(g_vr_settings.turn_mode).data(),
        g_vr_settings.snap_turn_angle, g_vr_settings.smooth_turn_speed,
        g_vr_settings.turn_dead_zone);
    penumbra_vr::probe::WriteLog(
        "VR presentation profile render_scale=%.2f ui_distance=%.2f ui_scale=%.2f",
        g_vr_settings.render_scale, g_vr_settings.ui_distance,
        g_vr_settings.ui_scale);

    std::string hook_error;
    if (!penumbra_vr::hooks::InstallOpenGlMatrixTelemetry(hook_error)) {
        penumbra_vr::probe::WriteLog("OpenGL matrix telemetry failed: %s", hook_error.c_str());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::InstallRenderWorldProbe(hook_error)) {
        penumbra_vr::probe::WriteLog("RenderWorld probe failed: %s", hook_error.c_str());
        std::string ignored;
        static_cast<void>(penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(ignored));
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }
    if (!penumbra_vr::hooks::InstallSdlSwapHook(&OnFrame, hook_error)) {
        penumbra_vr::probe::WriteLog("SDL frame hook failed: %s", hook_error.c_str());
        std::string ignored;
        static_cast<void>(penumbra_vr::backends::black_plague::RemoveRenderWorldProbe(ignored));
        static_cast<void>(penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(ignored));
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    const bool native_input_ready = penumbra_vr::backends::black_plague::InstallNativeInputBridge(hook_error);
    penumbra_vr::probe::WriteLog("Native controller input bridge installed=%u error=%s",
        native_input_ready ? 1U : 0U, hook_error.c_str());
    if (native_input_ready) {
        const bool body_probe_ready =
            penumbra_vr::backends::black_plague::InstallBodyCollisionProbe(
                hook_error);
        penumbra_vr::probe::WriteLog(
            "Body/collision telemetry installed=%u error=%s",
            body_probe_ready ? 1U : 0U, hook_error.c_str());
        const bool body_adapter_ready = body_probe_ready &&
            penumbra_vr::backends::black_plague::InstallBlackPlagueBodyAdapter(
                hook_error);
        if (!body_probe_ready) {
            hook_error = "body/collision observer was not installed";
        }
        const auto shadow_status = penumbra_vr::backends::black_plague::
            ReadBlackPlagueShadowStatus();
        const char* shadow_source = "disabled";
        if (shadow_status.source == penumbra_vr::backends::black_plague::
                BlackPlagueShadowRequestSource::environment) {
            shadow_source = "environment";
        } else if (shadow_status.source == penumbra_vr::backends::black_plague::
                BlackPlagueShadowRequestSource::mutex) {
            shadow_source = "mutex";
        }
        penumbra_vr::probe::WriteLog(
            "Black Plague body adapter installed=%u error=%s body_reconciliation_shadow "
            "enabled=%u source=%s",
            body_adapter_ready ? 1U : 0U, hook_error.c_str(),
            shadow_status.enabled ? 1U : 0U, shadow_source);
        const bool ownership_probe_ready =
            penumbra_vr::backends::black_plague::InstallMovementOwnershipProbe(hook_error);
        penumbra_vr::probe::WriteLog("Movement ownership telemetry installed=%u error=%s",
            ownership_probe_ready ? 1U : 0U, hook_error.c_str());
        const bool spatial_ready = penumbra_vr::backends::black_plague::InstallSpatialInteraction(hook_error);
        penumbra_vr::probe::WriteLog("Spatial interaction installed=%u error=%s",
            spatial_ready ? 1U : 0U, hook_error.c_str());
    }
    penumbra_vr::probe::WriteLog(
        "Probe initialized build=%.*s log=%s",
        static_cast<int>(build->id.size()),
        build->id.data(),
        WideToUtf8(log_path).c_str());
    InterlockedExchange(&g_state, 2);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_Shutdown(void*) {
    if (InterlockedCompareExchange(&g_state, 3, 2) != 2) {
        return 0;
    }

    std::string error;
    if (InterlockedCompareExchange(&g_presentation_state, 3, 2) == 2) {
        if (!StopPersistentVrPresentation(error)) {
            penumbra_vr::probe::WriteLog(
                "Persistent VR presentation shutdown failed: %s", error.c_str());
            InterlockedExchange(&g_presentation_state, 2);
            InterlockedExchange(&g_state, 2);
            return 0;
        }
        InterlockedExchange(&g_presentation_state, 0);
    }
    if (!penumbra_vr::backends::black_plague::DestroyPersistentEyeTargets(error)) {
        penumbra_vr::probe::WriteLog(
            "Render-thread eye-target cleanup failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    const std::uint64_t persistent_frames =
        penumbra_vr::backends::black_plague::PersistentEyeTargetLifetimeFrames();
    if (persistent_frames != 0) {
        penumbra_vr::probe::WriteLog(
            "Persistent eye targets destroyed on the render thread after %llu frames",
            persistent_frames);
    }
    if (!g_openvr_session.Shutdown(error)) {
        penumbra_vr::probe::WriteLog(
            "OpenVR shutdown failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::RemoveSpatialInteraction(error)) {
        penumbra_vr::probe::WriteLog("Spatial interaction removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::RemoveMovementOwnershipProbe(error)) {
        penumbra_vr::probe::WriteLog("Movement ownership telemetry removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::RemoveBlackPlagueBodyAdapter(error)) {
        penumbra_vr::probe::WriteLog(
            "Black Plague body adapter removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::RemoveBodyCollisionProbe(error)) {
        penumbra_vr::probe::WriteLog(
            "Body/collision telemetry removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::RemoveNativeInputBridge(error)) {
        penumbra_vr::probe::WriteLog("Native input bridge removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::hooks::RemoveSdlSwapHook(error)) {
        penumbra_vr::probe::WriteLog("SDL frame hook removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::backends::black_plague::RemoveRenderWorldProbe(error)) {
        penumbra_vr::probe::WriteLog("RenderWorld probe removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(error)) {
        penumbra_vr::probe::WriteLog("OpenGL telemetry removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }

    penumbra_vr::probe::WriteLog(
        "Probe shut down after %llu observed frames",
        penumbra_vr::hooks::ObservedFrameCount());
    penumbra_vr::probe::CloseLog();
    InterlockedExchange(&g_state, 0);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_StartPresentation(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    if (InterlockedCompareExchange(&g_presentation_state, 1, 0) != 0) {
        return g_presentation_state == 2 ? 1UL : 0UL;
    }

    std::string error;
    if (!StartPersistentVrPresentation(error)) {
        penumbra_vr::probe::WriteLog(
            "Persistent VR presentation startup failed: %s", error.c_str());
        InterlockedExchange(&g_presentation_state, 0);
        return 0;
    }
    InterlockedExchange(&g_presentation_state, 2);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_StopPresentation(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    const LONG previous = InterlockedCompareExchange(&g_presentation_state, 3, 2);
    if (previous == 0) {
        return 1;
    }
    if (previous != 2) {
        return 0;
    }

    std::string error;
    if (!StopPersistentVrPresentation(error)) {
        penumbra_vr::probe::WriteLog(
            "Persistent VR presentation stop failed: %s", error.c_str());
        InterlockedExchange(&g_presentation_state, 2);
        return 0;
    }
    penumbra_vr::probe::WriteLog("Persistent tracked VR presentation stopped cleanly");
    InterlockedExchange(&g_presentation_state, 0);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_EnableMonitorMirror(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    penumbra_vr::backends::black_plague::SetTrackedStereoMonitorMirror(true);
    penumbra_vr::probe::WriteLog(
        "VR monitor mirror enabled; desktop will display the left eye at swap");
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_QueryMonitorMirror(void*) {
    if (InterlockedCompareExchange(&g_state,2,2)!=2) return 0;
    const bool enabled=penumbra_vr::backends::black_plague::TrackedStereoMonitorMirrorEnabled();
    penumbra_vr::probe::WriteLog("VR monitor mirror readback: %s",enabled ? "enabled" : "disabled");
    return enabled ? 1 : 2;
}

extern "C" DWORD WINAPI PenumbraVR_DisableMonitorMirror(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    penumbra_vr::backends::black_plague::SetTrackedStereoMonitorMirror(false);
    penumbra_vr::probe::WriteLog(
        "VR monitor mirror disabled; continuous stereo will use two world passes");
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_ValidateEyeTargets(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }

    std::string error;
    if (!penumbra_vr::backends::black_plague::RequestTransientEyeTargetValidation(error)) {
        penumbra_vr::probe::WriteLog(
            "In-game eye-target validation failed: %s", error.c_str());
        return 0;
    }
    penumbra_vr::probe::WriteLog("In-game eye-target validation passed");
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_CreatePersistentEyeTargets(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }

    std::string error;
    if (!penumbra_vr::backends::black_plague::RequestPersistentEyeTargets(
            512, 512, error)) {
        penumbra_vr::probe::WriteLog(
            "Persistent eye-target creation failed: %s", error.c_str());
        return 0;
    }
    penumbra_vr::probe::WriteLog(
        "Persistent 512x512 diagnostic eye targets created on the render thread");
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_ValidateWorldDuplication(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }

    std::string error;
    if (!penumbra_vr::backends::black_plague::RequestPersistentEyeTargets(
            512, 512, error)) {
        penumbra_vr::probe::WriteLog(
            "World-duplication target creation failed: %s", error.c_str());
        return 0;
    }

    constexpr std::uint32_t kValidationFrames = 120;
    const bool duplicated =
        penumbra_vr::backends::black_plague::ValidateControlledWorldDuplication(
            kValidationFrames, error);
    const std::string duplication_error = error;

    std::string cleanup_error;
    const bool destroyed =
        penumbra_vr::backends::black_plague::DestroyPersistentEyeTargets(cleanup_error);
    if (!duplicated) {
        penumbra_vr::probe::WriteLog(
            "Controlled world duplication failed: %s",
            duplication_error.c_str());
        if (!destroyed) {
            penumbra_vr::probe::WriteLog(
                "World-duplication target cleanup also failed: %s",
                cleanup_error.c_str());
        }
        return 0;
    }
    if (!destroyed) {
        penumbra_vr::probe::WriteLog(
            "World-duplication target cleanup failed: %s",
            cleanup_error.c_str());
        return 0;
    }

    penumbra_vr::probe::WriteLog(
        "Controlled duplicate RenderWorld validation passed for %lu frames",
        static_cast<unsigned long>(kValidationFrames));
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_ValidateStereoMatrices(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    std::string error;
    constexpr std::uint32_t kValidationFrames = 60;
    if (!RunStereoExperiment(
            StereoExperiment::offscreen_matrices,
            kValidationFrames,
            error)) {
        penumbra_vr::probe::WriteLog(
            "Controlled stereo-matrix validation failed: %s",
            error.c_str());
        return 0;
    }
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_ValidateStereoSubmission(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    std::string error;
    constexpr std::uint32_t kSubmissionFrames = 300;
    if (!RunStereoExperiment(
            StereoExperiment::compositor_submission,
            kSubmissionFrames,
            error)) {
        penumbra_vr::probe::WriteLog(
            "Controlled OpenVR stereo submission failed: %s",
            error.c_str());
        return 0;
    }
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_ValidateTrackedStereoSubmission(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }
    std::string error;
    constexpr std::uint32_t kSubmissionFrames = 300;
    if (!RunStereoExperiment(
            StereoExperiment::tracked_compositor_submission,
            kSubmissionFrames,
            error)) {
        penumbra_vr::probe::WriteLog(
            "Controlled tracked OpenVR stereo submission failed: %s",
            error.c_str());
        return 0;
    }
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_CreateOpenVrEyeTargets(void*) {
    if (InterlockedCompareExchange(&g_state, 2, 2) != 2) {
        return 0;
    }

    const std::wstring loader_path = OpenVrLoaderPath();
    if (loader_path.empty()) {
        penumbra_vr::probe::WriteLog("Could not resolve the probe directory");
        return 0;
    }

    std::string error;
    if (!g_openvr_session.Initialize(loader_path, error)) {
        penumbra_vr::probe::WriteLog(
            "OpenVR initialization failed: %s", error.c_str());
        return 0;
    }

    const penumbra_vr::runtime::VrRenderTargetSize size =
        g_openvr_session.recommended_render_target_size();
    penumbra_vr::probe::WriteLog(
        "OpenVR initialized; recommended eye size is %lux%lu",
        static_cast<unsigned long>(size.width),
        static_cast<unsigned long>(size.height));
    if (!penumbra_vr::backends::black_plague::RequestPersistentEyeTargets(
            size.width, size.height, error)) {
        penumbra_vr::probe::WriteLog(
            "OpenVR-sized eye-target creation failed: %s", error.c_str());
        std::string shutdown_error;
        if (!g_openvr_session.Shutdown(shutdown_error)) {
            penumbra_vr::probe::WriteLog(
                "OpenVR cleanup after eye-target failure also failed: %s",
                shutdown_error.c_str());
        }
        return 0;
    }

    penumbra_vr::probe::WriteLog(
        "Persistent OpenVR-sized eye targets created at %lux%lu",
        static_cast<unsigned long>(size.width),
        static_cast<unsigned long>(size.height));
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
