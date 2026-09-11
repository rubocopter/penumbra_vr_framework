#include "vr_locomotion.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {

[[nodiscard]] bool FiniteVector(const std::array<float, 3>& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

[[nodiscard]] float HorizontalLength(
    const std::array<float, 3>& value) noexcept {
    return std::hypot(value[0], value[2]);
}

[[nodiscard]] float Distance(
    const std::array<float, 3>& first,
    const std::array<float, 3>& second) noexcept {
    return std::hypot(
        std::hypot(first[0] - second[0], first[1] - second[1]),
        first[2] - second[2]);
}

} // namespace

bool ObserveAcceptedBodyMotion(
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    VrAcceptedBodyMotion& observation) noexcept {
    observation = {};
    if (!FiniteVector(body_before) || !FiniteVector(body_after)) {
        return false;
    }
    observation.body_before = body_before;
    observation.body_after = body_after;
    observation.accepted_displacement = {
        body_after[0] - body_before[0],
        body_after[1] - body_before[1],
        body_after[2] - body_before[2],
    };
    return FiniteVector(observation.accepted_displacement);
}

std::array<float, 3> HeadRelativeMoveDirection(
    const VrMatrix44& head,
    const VrAnalogState& move) noexcept {
    if (!move.active || !std::isfinite(move.x) || !std::isfinite(move.y)) {
        return {};
    }

    std::array<float, 3> forward{-head.values[2], 0.0F, -head.values[10]};
    std::array<float, 3> right{head.values[0], 0.0F, head.values[8]};
    const float forward_length = HorizontalLength(forward);
    const float right_length = HorizontalLength(right);
    if (!std::isfinite(forward_length) || !std::isfinite(right_length) ||
        forward_length <= 1.0e-6F || right_length <= 1.0e-6F) {
        return {};
    }
    forward[0] /= forward_length;
    forward[2] /= forward_length;
    right[0] /= right_length;
    right[2] /= right_length;

    std::array<float, 3> result{
        forward[0] * move.y + right[0] * move.x,
        0.0F,
        forward[2] * move.y + right[2] * move.x,
    };
    const float length = HorizontalLength(result);
    if (length > 1.0F) {
        result[0] /= length;
        result[2] /= length;
    }
    return result;
}

std::array<float, 3> LocomotionDisplacement(
    const std::array<float, 3>& world_direction,
    float delta_seconds,
    float move_scale,
    bool constrained,
    bool sprinting) noexcept {
    if (!FiniteVector(world_direction) || !std::isfinite(delta_seconds) ||
        !std::isfinite(move_scale) || delta_seconds <= 0.0F ||
        delta_seconds > 0.25F || move_scale < 0.0F) {
        return {};
    }
    const float base_speed = constrained
        ? vr_locomotion_policy::kConstrainedSpeedMetersPerSecond
        : vr_locomotion_policy::kNormalSpeedMetersPerSecond;
    const float sprint = !constrained && sprinting
        ? vr_locomotion_policy::kSprintMultiplier : 1.0F;
    const float multiplier = delta_seconds * move_scale * base_speed * sprint;
    return {
        world_direction[0] * multiplier,
        0.0F,
        world_direction[2] * multiplier,
    };
}

std::array<float, 3> ClampPhysicalBodyStep(
    const std::array<float, 3>& desired_head_position,
    const std::array<float, 3>& body_position) noexcept {
    if (!FiniteVector(desired_head_position) || !FiniteVector(body_position)) {
        return {};
    }
    std::array<float, 3> delta{
        desired_head_position[0] - body_position[0],
        0.0F,
        desired_head_position[2] - body_position[2],
    };
    const float length = HorizontalLength(delta);
    if (length > vr_locomotion_policy::kMaximumPhysicalBodyStep) {
        const float scale =
            vr_locomotion_policy::kMaximumPhysicalBodyStep / length;
        delta[0] *= scale;
        delta[2] *= scale;
    }
    return delta;
}

float AcceptedDistanceAlongRequest(
    const std::array<float, 3>& requested,
    const std::array<float, 3>& accepted) noexcept {
    if (!FiniteVector(requested) || !FiniteVector(accepted)) {
        return 0.0F;
    }
    const float requested_length = HorizontalLength(requested);
    if (requested_length <= 1.0e-6F) {
        return 0.0F;
    }
    const float projected =
        (accepted[0] * requested[0] + accepted[2] * requested[2]) /
        requested_length;
    return std::clamp(projected, 0.0F, requested_length);
}

bool ShouldCarryHeadAnchorWithLocomotion(
    const std::array<float, 3>& head_anchor,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& accepted) noexcept {
    if (!FiniteVector(head_anchor) || !FiniteVector(body_before) ||
        !FiniteVector(accepted)) {
        return false;
    }
    const std::array<float, 3> body_after{
        body_before[0] + accepted[0],
        body_before[1] + accepted[1],
        body_before[2] + accepted[2],
    };
    const std::array<float, 3> carried_head{
        head_anchor[0] + accepted[0],
        head_anchor[1] + accepted[1],
        head_anchor[2] + accepted[2],
    };
    // Preserve Rework's exact 3D comparison. The anchor normally sits at feet
    // height while the character position is at capsule centre, so removing
    // the shared vertical term would subtly change the 1 mm hysteresis.
    return Distance(head_anchor, body_after) + 0.001F >
        Distance(carried_head, body_after);
}

} // namespace penumbra_vr::runtime
