#include "gameplay_contract.hpp"

#include "vr_locomotion.hpp"
#include "vr_settings.hpp"
#include "vr_tracking_space.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::backends::requiem {
namespace {

constexpr RequiemGameplayContract kContract{
    0x3C30,
    0x273A64,
    0x38,
    0x51A3,
    0x9D020,
    0x51FD,
    0x9D0C0,
    0x2C8,
    0x2C0,
    0x4C,
    0x50,
    0x2DC,
    0x2D4,
    0x0C,
    0x10,
    0x268,
    0x270,
    0x264,
    0x278,
    0xD4F6A,
    0xD77A0,
    0x48,
    0xD7C21,
    {0xD9, 0x07, 0xD9, 0x46, 0x54},
    0xD7CB2,
    0xD5190,
    0xD8112,
    0x2D4,
    0x9CBA0,
    {0x56, 0x8B, 0xF1, 0x8B, 0x96, 0xD4, 0x02, 0x00, 0x00,
     0x8B, 0x4C, 0x24, 0x08},
    0xAF364,
    {0x83, 0xFA, 0x04, 0x75, 0x08, 0x6A, 0x00, 0xE8},
    0xC8,
    0xD59DC,
    {0x8B, 0x91, 0xC8, 0x00, 0x00, 0x00, 0x89, 0x50, 0x04,
     0x8B, 0x89, 0xCC, 0x00, 0x00, 0x00, 0x89, 0x48, 0x08},
    0xD6044,
    {0x8B, 0x8E, 0x3C, 0x02, 0x00, 0x00, 0x89, 0x86,
     0xC8, 0x00, 0x00, 0x00},
};

} // namespace

const RequiemGameplayContract& GameplayContract() noexcept {
    return kContract;
}

RequiemPushAxes RequiemPushAxisProjection(
    const std::array<float,3>& desired_world_direction,
    const std::array<float,3>& native_forward,
    const std::array<float,3>& native_right) noexcept {
    const auto axes = runtime::ProjectNativePushAxes(
        desired_world_direction, native_forward, native_right);
    return {axes.forward, axes.sideways, axes.valid};
}

std::array<float,3> RequiemPushHandForce(
    const std::array<float,3>& palm_position,
    const std::array<float,3>& body_position,
    const std::array<float,3>& body_relative_contact) noexcept {
    std::array<float,3> delta{};
    for (std::size_t axis = 0; axis < delta.size(); ++axis) {
        delta[axis] = palm_position[axis] -
            body_position[axis] - body_relative_contact[axis];
        if (!std::isfinite(delta[axis])) return {};
    }
    const float length = std::hypot(
        std::hypot(delta[0], delta[1]), delta[2]);
    if (!std::isfinite(length) || length <= 1.0e-5F) return {};
    // Rework Push::OnUpdate normalizes in 3D, then removes the vertical force.
    return {delta[0] / length * 300.0F, 0.0F,
        delta[2] / length * 300.0F};
}

std::array<float, 3> RequiemLocomotionDisplacement(
    const std::array<float, 3>& world_direction,
    float delta_seconds,
    bool sprinting,
    bool pushing) noexcept {
    // Requiem requests twice the standard 2.25 m/s sprint; walking stays 1.5 m/s.
    constexpr float kRequiemSprintMultiplier = 2.0F;
    return runtime::LocomotionDisplacement(
        world_direction, delta_seconds,
        pushing ? 1.0F : sprinting ? kRequiemSprintMultiplier : 1.0F,
        pushing, sprinting);
}

DirectLocomotionPublication PlanDirectLocomotionPublication(
    bool native_axis_observed,
    float requested_x,
    float requested_y,
    bool sideways_allowed,
    bool forward_allowed,
    bool tracked_head_pose_fresh) noexcept {
    DirectLocomotionPublication result;
    if (native_axis_observed || !tracked_head_pose_fresh ||
        !std::isfinite(requested_x) || !std::isfinite(requested_y)) {
        return result;
    }
    result.move_x = sideways_allowed ? requested_x : 0.0F;
    result.move_y = forward_allowed ? requested_y : 0.0F;
    result.publish = std::hypot(result.move_x, result.move_y) > 0.00001F;
    return result;
}

bool TrackedHeadWorldPoseFresh(
    bool valid,
    std::uint64_t sampled_at_ms,
    std::uint64_t now_ms) noexcept {
    constexpr std::uint64_t kMaximumAgeMs = 250;
    return valid && sampled_at_ms != 0 && now_ms >= sampled_at_ms &&
        now_ms - sampled_at_ms <= kMaximumAgeMs;
}

bool ShouldMarkDirectLocomotionAccepted(
    const DirectLocomotionPublication& plan,
    bool publication_succeeded) noexcept {
    return plan.publish && publication_succeeded;
}

NativeCrouchTransition PlanNativeCrouchTransition(
    bool body_present,
    int move_state,
    bool desired_crouch,
    bool vr_stance_owned) noexcept {
    if (!body_present || move_state < 0)
        return NativeCrouchTransition::none;
    if (desired_crouch) {
        if (move_state != 3 && move_state != 4)
            return NativeCrouchTransition::enter;
    } else if (vr_stance_owned && move_state == 4) {
        return NativeCrouchTransition::stand;
    }
    return NativeCrouchTransition::none;
}

bool ComposeRequiemTrackedHeadHeight(
    float body_center_y,
    float active_size_y,
    float tracking_y,
    float height_calibration,
    bool native_crouched,
    bool physical_crouch,
    float& world_y,
    std::string& error) noexcept {
    world_y = 0.0F;
    if (!std::isfinite(body_center_y) || !std::isfinite(active_size_y) ||
        active_size_y <= 0.0F || !std::isfinite(tracking_y) ||
        !std::isfinite(height_calibration)) {
        error = "Requiem body, capsule, tracking or calibration height is invalid";
        return false;
    }
    runtime::VrMatrix34 tracking_pose{{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, tracking_y,
        0.0F, 0.0F, 1.0F, 0.0F,
    }};
    runtime::VrTrackingSpace tracking_space;
    tracking_space.SetHeadTrackingPose(tracking_pose);
    // Rework anchors tracking to iCharacterBody::GetFeetPosition(). Requiem
    // exposes its native position as the centre of the active capsule.
    tracking_space.SetPlayerWorldPosition({
        0.0F, body_center_y - active_size_y * 0.5F, 0.0F});
    tracking_space.SetHeightCalibration(height_calibration);
    if (native_crouched && !physical_crouch) {
        tracking_space.SetPostureOffset(
            -runtime::vr_setting_limits::kPhysicalCrouchDepth.default_value);
    }
    runtime::VrMatrix44 head_world{};
    if (!tracking_space.HeadWorldPose(head_world, error)) return false;
    world_y = head_world.values[7];
    if (!std::isfinite(world_y)) {
        error = "Requiem tracked head height is non-finite";
        return false;
    }
    return true;
}

std::array<float, 3> AttributeRequiemPhysicalAcceptance(
    const std::array<float, 3>& physical,
    const std::array<float, 3>& direct,
    const std::array<float, 3>& accepted) noexcept {
    if (!std::isfinite(physical[0]) || !std::isfinite(physical[2]) ||
        !std::isfinite(direct[0]) || !std::isfinite(direct[2]) ||
        !std::isfinite(accepted[0]) || !std::isfinite(accepted[2])) return {};
    const float length = std::hypot(physical[0], physical[2]);
    if (length <= 0.000001F) return {};
    float accepted_along = 0.0F;
    if (std::hypot(direct[0], direct[2]) <= 0.000001F) {
        accepted_along = std::clamp(
            (accepted[0] * physical[0] +
                accepted[2] * physical[2]) / length,
            0.0F, length);
    } else {
        // Requiem has the same combined collision solve as Black Plague.
        // Keep the physical share when opposing stick input cancels movement.
        const float rejected_x = physical[0] + direct[0] - accepted[0];
        const float rejected_z = physical[2] + direct[2] - accepted[2];
        const float rejected_along = std::max(0.0F,
            (rejected_x * physical[0] + rejected_z * physical[2]) / length);
        const float direct_along = std::max(0.0F,
            (direct[0] * physical[0] + direct[2] * physical[2]) / length);
        accepted_along = length - std::clamp(
            rejected_along - direct_along, 0.0F, length);
    }
    const float scale = accepted_along / length;
    return {physical[0] * scale, 0.0F, physical[2] * scale};
}

bool RunRequiemGameplayContractHarness(std::string& error) noexcept {
    error.clear();
    const auto& contract = GameplayContract();
    const auto require = [&](bool condition, const char* detail) noexcept {
        if (condition) return true;
        error = detail;
        return false;
    };

    if (!require(contract.button_handler_vtable_entry_rva == 0x273A64 &&
                 contract.button_handler_update_rva == 0x3C30,
            "Requiem ButtonHandler Update ABI drifted")) return false;
    if (!require(contract.handler_player_offset == 0x38,
            "Requiem ButtonHandler player field drifted")) return false;
    if (!require(contract.character_size_y_offset == 0xC8 &&
                 contract.get_character_size_y_rva == 0xD59DC &&
                 contract.set_active_size_y_rva == 0xD6044,
            "Requiem active capsule height contract drifted")) return false;
    if (!require(contract.character_update_callsite_rva == 0xD4F6A &&
                 contract.character_update_rva == 0xD77A0 &&
                 contract.character_position_offset == 0x48,
            "Requiem native body-update ownership drifted")) return false;
    if (!require(contract.forward_callsite_rva == 0x51A3 &&
                 contract.move_forward_rva == 0x9D020 &&
                 contract.sideways_callsite_rva == 0x51FD &&
                 contract.move_sideways_rva == 0x9D0C0,
            "Requiem native movement callbacks drifted")) return false;
    if (!require(contract.primary_state_vector_offset == 0x2C8 &&
                 contract.primary_state_index_offset == 0x2C0 &&
                 contract.primary_forward_gate_slot == 0x4C &&
                 contract.primary_sideways_gate_slot == 0x50,
            "Requiem primary movement-state contract drifted")) return false;
    if (!require(contract.secondary_state_vector_offset == 0x2DC &&
                 contract.secondary_state_index_offset == 0x2D4 &&
                 contract.secondary_forward_gate_slot == 0x0C &&
                 contract.secondary_sideways_gate_slot == 0x10,
            "Requiem secondary movement-state contract drifted")) return false;
    if (!require(contract.movement_count_offset == 0x268 &&
                 contract.movement_override_offset == 0x270 &&
                 contract.accepted_motion_offset == 0x264 &&
                 contract.character_body_offset == 0x278,
            "Requiem player movement layout drifted")) return false;
    constexpr std::array<std::uint8_t, 5> physical_owner_window{
        0xD9, 0x07, 0xD9, 0x46, 0x54};
    if (!require(contract.physical_move_owner_rva == 0xD7C21 &&
                 contract.physical_move_owner_window == physical_owner_window &&
                 contract.collision_callsite_rva == 0xD7CB2 &&
                 contract.collision_rva == 0xD5190 &&
                 contract.physical_step_decision_rva == 0xD8112,
            "Requiem native body movement contract drifted")) return false;
    constexpr std::array<std::uint8_t, 13> change_move_state_window{
        0x56, 0x8B, 0xF1, 0x8B, 0x96, 0xD4, 0x02, 0x00, 0x00,
        0x8B, 0x4C, 0x24, 0x08};
    constexpr std::array<std::uint8_t, 8> crouch_state_proof_window{
        0x83, 0xFA, 0x04, 0x75, 0x08, 0x6A, 0x00, 0xE8};
    if (!require(contract.move_state_index_offset == 0x2D4 &&
                 contract.change_move_state_rva == 0x9CBA0 &&
                 contract.change_move_state_window == change_move_state_window &&
                 contract.crouch_state_proof_rva == 0xAF364 &&
                 contract.crouch_state_proof_window == crouch_state_proof_window,
            "Requiem native crouch contract drifted")) return false;
    return true;
}

} // namespace penumbra_vr::backends::requiem
