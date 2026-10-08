#pragma once

#include <array>
#include <cstdint>
#include <string>
#include "vr_play_mode_policy.hpp"

namespace penumbra_vr::backends::requiem {

struct RequiemGameplayContract final {
    std::uintptr_t button_handler_update_rva{};
    std::uintptr_t button_handler_vtable_entry_rva{};
    std::uintptr_t handler_player_offset{};
    std::uintptr_t forward_callsite_rva{};
    std::uintptr_t move_forward_rva{};
    std::uintptr_t sideways_callsite_rva{};
    std::uintptr_t move_sideways_rva{};
    std::uintptr_t primary_state_vector_offset{};
    std::uintptr_t primary_state_index_offset{};
    std::uintptr_t primary_forward_gate_slot{};
    std::uintptr_t primary_sideways_gate_slot{};
    std::uintptr_t secondary_state_vector_offset{};
    std::uintptr_t secondary_state_index_offset{};
    std::uintptr_t secondary_forward_gate_slot{};
    std::uintptr_t secondary_sideways_gate_slot{};
    std::uintptr_t movement_count_offset{};
    std::uintptr_t movement_override_offset{};
    std::uintptr_t accepted_motion_offset{};
    std::uintptr_t character_body_offset{};
    std::uintptr_t character_update_callsite_rva{};
    std::uintptr_t character_update_rva{};
    std::uintptr_t character_position_offset{};
    std::uintptr_t physical_move_owner_rva{};
    std::array<std::uint8_t, 5> physical_move_owner_window{};
    std::uintptr_t collision_callsite_rva{};
    std::uintptr_t collision_rva{};
    std::uintptr_t physical_step_decision_rva{};
    std::uintptr_t move_state_index_offset{};
    std::uintptr_t change_move_state_rva{};
    std::array<std::uint8_t, 13> change_move_state_window{};
    std::uintptr_t crouch_state_proof_rva{};
    std::array<std::uint8_t, 8> crouch_state_proof_window{};
    std::uintptr_t character_size_y_offset{};
    std::uintptr_t get_character_size_y_rva{};
    std::array<std::uint8_t, 18> get_character_size_y_window{};
    std::uintptr_t set_active_size_y_rva{};
    std::array<std::uint8_t, 12> set_active_size_y_window{};
};

enum class NativeCrouchTransition {
    none,
    enter,
    stand,
};

enum class RequiemUiSurface {
    none,
    fullscreen,
    inventory,
    notebook,
};

struct RequiemUiClassification final {
    bool active{};
    RequiemUiSurface surface{RequiemUiSurface::none};
};

struct DirectLocomotionPublication final {
    float move_x{};
    float move_y{};
    bool publish{};
};

struct RequiemPushAxes final {
    float forward{};
    float sideways{};
    bool valid{};
};

[[nodiscard]] RequiemPushAxes RequiemPushAxisProjection(
    const std::array<float,3>& desired_world_direction,
    const std::array<float,3>& native_forward,
    const std::array<float,3>& native_right) noexcept;
[[nodiscard]] std::array<float,3> RequiemPushHandForce(
    const std::array<float,3>& palm_position,
    const std::array<float,3>& body_position,
    const std::array<float,3>& body_relative_contact) noexcept;

[[nodiscard]] const RequiemGameplayContract& GameplayContract() noexcept;
[[nodiscard]] std::array<float, 3> RequiemLocomotionDisplacement(
    const std::array<float, 3>& world_direction,
    float delta_seconds,
    bool sprinting,
    bool pushing = false) noexcept;
[[nodiscard]] DirectLocomotionPublication PlanDirectLocomotionPublication(
    bool native_axis_observed,
    float requested_x,
    float requested_y,
    bool sideways_allowed,
    bool forward_allowed,
    bool tracked_head_pose_fresh) noexcept;
[[nodiscard]] bool TrackedHeadWorldPoseFresh(
    bool valid,
    std::uint64_t sampled_at_ms,
    std::uint64_t now_ms) noexcept;
[[nodiscard]] bool ShouldMarkDirectLocomotionAccepted(
    const DirectLocomotionPublication& plan,
    bool publication_succeeded) noexcept;
[[nodiscard]] RequiemUiClassification ClassifyRequiemUi(
    bool in_game,
    bool inventory_active,
    bool notebook_active,
    bool native_pointer_observed) noexcept;
[[nodiscard]] NativeCrouchTransition PlanNativeCrouchTransition(
    bool body_present,
    int move_state,
    bool desired_crouch,
    bool vr_stance_owned) noexcept;
[[nodiscard]] bool ComposeRequiemTrackedHeadHeight(
    float body_center_y,
    float active_size_y,
    float tracking_y,
    float height_calibration,
    bool native_crouched,
    bool physical_crouch,
    float& world_y,
    std::string& error,
    float seated_offset = 0.0F,
    float button_crouch_depth = runtime::vr_setting_limits::kPhysicalCrouchDepth.default_value) noexcept;

// Rework/BP feet-anchored height. Native cinematic camera height is not a
// calibration input; user profile and physical tracking own VR eye height.
[[nodiscard]] bool ComposeRequiemConfiguredHeadHeight(
    float body_center_y, float active_size_y, float tracking_y,
    const runtime::VrSettings& settings, runtime::VrPlayModePolicy& play_mode,
    bool native_crouched, bool physical_crouch,
    float& world_y, std::string& error) noexcept;
[[nodiscard]] std::array<float, 3> AttributeRequiemPhysicalAcceptance(
    const std::array<float, 3>& physical_request,
    const std::array<float, 3>& direct_request,
    const std::array<float, 3>& accepted_combined) noexcept;

// Exact-build host harness. It validates only the reverse-engineered Requiem
// ABI used by the gameplay backend and never attaches to a running process.
[[nodiscard]] bool RunRequiemGameplayContractHarness(
    std::string& error) noexcept;

} // namespace penumbra_vr::backends::requiem
