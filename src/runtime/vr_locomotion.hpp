#pragma once

#include "vr_input_state.hpp"
#include "vr_math.hpp"

#include <array>

namespace penumbra_vr::runtime {

namespace vr_locomotion_policy {

// Values preserved from Rework Player.cpp at revision 23c890f.
inline constexpr float kWorldUnitsPerMeter = 1.0F;
inline constexpr float kMaximumHeadBodySeparation = 0.8F;
inline constexpr float kMaximumPhysicalBodyStep = 0.05F;
inline constexpr float kRejectedMotionEpsilon = 0.002F;
inline constexpr float kNormalSpeedMetersPerSecond = 1.5F;
inline constexpr float kConstrainedSpeedMetersPerSecond = 0.5F;
inline constexpr float kSprintMultiplier = 1.5F;

} // namespace vr_locomotion_policy

// Game-neutral result of one native horizontal-body phase.  Adapters provide
// positions around their own engine tick; runtime policy only reasons about
// the displacement the game actually accepted.  This deliberately contains no
// layout, RVA, collision flag, or native-update ownership.
struct VrAcceptedBodyMotion {
    std::array<float, 3> body_before{};
    std::array<float, 3> body_after{};
    std::array<float, 3> accepted_displacement{};
};

[[nodiscard]] bool ObserveAcceptedBodyMotion(
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    VrAcceptedBodyMotion& observation) noexcept;

[[nodiscard]] std::array<float, 3> HeadRelativeMoveDirection(
    const VrMatrix44& head_world_pose,
    const VrAnalogState& move) noexcept;
[[nodiscard]] std::array<float, 3> LocomotionDisplacement(
    const std::array<float, 3>& world_direction,
    float delta_seconds,
    float move_scale,
    bool constrained,
    bool sprinting) noexcept;
[[nodiscard]] std::array<float, 3> ClampPhysicalBodyStep(
    const std::array<float, 3>& desired_head_position,
    const std::array<float, 3>& body_position) noexcept;
[[nodiscard]] float AcceptedDistanceAlongRequest(
    const std::array<float, 3>& requested_displacement,
    const std::array<float, 3>& accepted_displacement) noexcept;
[[nodiscard]] bool ShouldCarryHeadAnchorWithLocomotion(
    const std::array<float, 3>& head_anchor,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& accepted_displacement) noexcept;

} // namespace penumbra_vr::runtime
