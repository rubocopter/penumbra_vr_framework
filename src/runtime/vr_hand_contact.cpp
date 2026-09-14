#include "vr_hand_contact.hpp"

#include "vr_interaction_policy.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {

[[nodiscard]] float Length(const VrHandContactVector& value) noexcept {
    return std::sqrt(value[0] * value[0] + value[1] * value[1] +
        value[2] * value[2]);
}

[[nodiscard]] float Dot(
    const VrHandContactVector& left,
    const VrHandContactVector& right) noexcept {
    return left[0] * right[0] + left[1] * right[1] +
        left[2] * right[2];
}

[[nodiscard]] VrHandContactVector Scale(
    const VrHandContactVector& value,
    float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

[[nodiscard]] VrHandContactVector Add(
    const VrHandContactVector& left,
    const VrHandContactVector& right) noexcept {
    return {left[0] + right[0], left[1] + right[1],
        left[2] + right[2]};
}

[[nodiscard]] VrHandContactVector Subtract(
    const VrHandContactVector& left,
    const VrHandContactVector& right) noexcept {
    return {left[0] - right[0], left[1] - right[1],
        left[2] - right[2]};
}

} // namespace

VrHandContactAccumulator::VrHandContactAccumulator(
    const VrHandContactVector& motion,
    float tolerance) noexcept {
    summary_.tolerance = std::isfinite(tolerance) && tolerance > 0.0F
        ? tolerance : 0.0F;
    const float length = Length(motion);
    if (std::isfinite(length) && length > 0.00001F) {
        summary_.motion_direction = Scale(motion, 1.0F / length);
    }
}

void VrHandContactAccumulator::Add(
    float depth,
    const VrHandContactVector& normal) noexcept {
    if (!std::isfinite(depth) || depth < 0.0F) return;
    summary_.has_contact = true;
    summary_.max_depth = std::max(summary_.max_depth, depth);
    if (depth <= summary_.tolerance) return;

    const float normal_length = Length(normal);
    if (!std::isfinite(normal_length) || normal_length <= 0.00001F) return;
    const auto normalized = Scale(normal, 1.0F / normal_length);
    const bool has_motion = Length(summary_.motion_direction) > 0.0F;
    const float score = has_motion
        ? -Dot(summary_.motion_direction, normalized) : depth;
    if (!summary_.has_blocking_normal ||
        score > summary_.best_score + 0.0001F ||
        (std::fabs(score - summary_.best_score) <= 0.0001F &&
            depth > summary_.best_depth)) {
        summary_.best_score = score;
        summary_.best_depth = depth;
        summary_.best_normal = normalized;
        summary_.has_blocking_normal = true;
    }
}

const VrHandContactSummary& VrHandContactAccumulator::summary() const noexcept {
    return summary_;
}

VrHandCollisionDecision ResolveVrHandCollision(
    bool world_collided,
    const VrHandContactVector& requested_position,
    const VrHandContactVector& corrected_position,
    const VrHandContactSummary& contacts) noexcept {
    VrHandCollisionDecision result;
    if (!world_collided) return result;

    if (contacts.has_contact) {
        if (contacts.max_depth <= contacts.tolerance) return result;
        if (contacts.has_blocking_normal) {
            result.blocking = true;
            result.normal = contacts.best_normal;
            result.depth = contacts.best_depth;
            return result;
        }
    }

    const auto correction = Subtract(corrected_position, requested_position);
    const float correction_length = Length(correction);
    if (std::isfinite(correction_length) &&
        correction_length <= contacts.tolerance) {
        return result;
    }
    result.blocking = true;
    if (std::isfinite(correction_length) && correction_length > 0.00001F) {
        result.normal = Scale(correction, 1.0F / correction_length);
        result.depth = correction_length;
    }
    return result;
}

std::array<VrHandContactVector, 4> VrHandRecoveryCandidates(
    const VrHandContactVector& head,
    const VrHandContactVector& right,
    const VrHandContactVector& up,
    const VrHandContactVector& forward,
    bool left_hand) noexcept {
    const float side = left_hand ? -1.0F : 1.0F;
    using namespace vr_interaction_policy;
    return {
        Add(Add(Add(head,
                    Scale(right, kRecoveryAnchorSide * side)),
                Scale(up, -kRecoveryAnchorDown)),
            Scale(forward, kRecoveryAnchorForward)),
        Add(Add(Add(head, Scale(right, 0.10F * side)),
                Scale(up, -0.32F)),
            Scale(forward, 0.02F)),
        Add(Add(Add(head, Scale(right, 0.20F * side)),
                Scale(up, -0.20F)),
            Scale(forward, -0.02F)),
        Add(head, Scale(up, -0.30F)),
    };
}

} // namespace penumbra_vr::runtime
