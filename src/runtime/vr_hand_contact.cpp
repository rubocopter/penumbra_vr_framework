#include "vr_hand_contact.hpp"

#include "vr_interaction_policy.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

[[nodiscard]] bool Finite(const VrHandContactVector& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

[[nodiscard]] float Distance(
    const VrHandContactVector& left,
    const VrHandContactVector& right) noexcept {
    return Length(Subtract(left, right));
}

[[nodiscard]] VrHandContactVector Translation(const VrMatrix44& pose) noexcept {
    return {pose.values[3], pose.values[7], pose.values[11]};
}

void SetTranslation(VrMatrix44& pose, const VrHandContactVector& value) noexcept {
    pose.values[3] = value[0];
    pose.values[7] = value[1];
    pose.values[11] = value[2];
}

[[nodiscard]] bool SamePose(
    const VrMatrix44& left,
    const VrMatrix44& right) noexcept {
    return left.values == right.values;
}

struct Quaternion {
    float w = 1.0F;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

[[nodiscard]] bool Normalize(Quaternion& value) noexcept {
    const float length = std::sqrt(value.w * value.w + value.x * value.x +
        value.y * value.y + value.z * value.z);
    if (!std::isfinite(length) || length <= 0.00001F) return false;
    value.w /= length;
    value.x /= length;
    value.y /= length;
    value.z /= length;
    return true;
}

[[nodiscard]] bool PoseQuaternion(
    const VrMatrix44& pose,
    Quaternion& result) noexcept {
    const float m00 = pose.values[0];
    const float m01 = pose.values[1];
    const float m02 = pose.values[2];
    const float m10 = pose.values[4];
    const float m11 = pose.values[5];
    const float m12 = pose.values[6];
    const float m20 = pose.values[8];
    const float m21 = pose.values[9];
    const float m22 = pose.values[10];
    const float trace = m00 + m11 + m22;
    if (trace > 0.0F) {
        const float scale = std::sqrt(trace + 1.0F) * 2.0F;
        if (!std::isfinite(scale) || scale <= 0.00001F) return false;
        result.w = 0.25F * scale;
        result.x = (m21 - m12) / scale;
        result.y = (m02 - m20) / scale;
        result.z = (m10 - m01) / scale;
    } else if (m00 > m11 && m00 > m22) {
        const float scale = std::sqrt(1.0F + m00 - m11 - m22) * 2.0F;
        if (!std::isfinite(scale) || scale <= 0.00001F) return false;
        result.w = (m21 - m12) / scale;
        result.x = 0.25F * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    } else if (m11 > m22) {
        const float scale = std::sqrt(1.0F + m11 - m00 - m22) * 2.0F;
        if (!std::isfinite(scale) || scale <= 0.00001F) return false;
        result.w = (m02 - m20) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25F * scale;
        result.z = (m12 + m21) / scale;
    } else {
        const float scale = std::sqrt(1.0F + m22 - m00 - m11) * 2.0F;
        if (!std::isfinite(scale) || scale <= 0.00001F) return false;
        result.w = (m10 - m01) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25F * scale;
    }
    return Normalize(result);
}

[[nodiscard]] VrMatrix44 QuaternionPose(
    const Quaternion& value,
    const VrHandContactVector& position) noexcept {
    VrMatrix44 pose = IdentityMatrix();
    const float xx = value.x * value.x;
    const float yy = value.y * value.y;
    const float zz = value.z * value.z;
    const float xy = value.x * value.y;
    const float xz = value.x * value.z;
    const float yz = value.y * value.z;
    const float wx = value.w * value.x;
    const float wy = value.w * value.y;
    const float wz = value.w * value.z;
    pose.values[0] = 1.0F - 2.0F * (yy + zz);
    pose.values[1] = 2.0F * (xy - wz);
    pose.values[2] = 2.0F * (xz + wy);
    pose.values[4] = 2.0F * (xy + wz);
    pose.values[5] = 1.0F - 2.0F * (xx + zz);
    pose.values[6] = 2.0F * (yz - wx);
    pose.values[8] = 2.0F * (xz - wy);
    pose.values[9] = 2.0F * (yz + wx);
    pose.values[10] = 1.0F - 2.0F * (xx + yy);
    SetTranslation(pose, position);
    return pose;
}

[[nodiscard]] float QuaternionDot(
    const Quaternion& left,
    const Quaternion& right) noexcept {
    return left.w * right.w + left.x * right.x + left.y * right.y +
        left.z * right.z;
}

[[nodiscard]] VrMatrix44 InterpolateRotation(
    const VrMatrix44& start,
    const VrMatrix44& goal,
    float t,
    const VrHandContactVector& position) noexcept {
    Quaternion from{};
    Quaternion to{};
    if (!PoseQuaternion(start, from) || !PoseQuaternion(goal, to)) {
        VrMatrix44 fallback = start;
        SetTranslation(fallback, position);
        return fallback;
    }
    float dot = QuaternionDot(from, to);
    if (dot < 0.0F) {
        to.w = -to.w;
        to.x = -to.x;
        to.y = -to.y;
        to.z = -to.z;
        dot = -dot;
    }
    dot = std::clamp(dot, 0.0F, 1.0F);
    Quaternion value{};
    if (dot > 0.9995F) {
        value.w = from.w * (1.0F - t) + to.w * t;
        value.x = from.x * (1.0F - t) + to.x * t;
        value.y = from.y * (1.0F - t) + to.y * t;
        value.z = from.z * (1.0F - t) + to.z * t;
    } else {
        const float angle = std::acos(dot);
        const float sine = std::sin(angle);
        const float start_weight = std::sin((1.0F - t) * angle) / sine;
        const float goal_weight = std::sin(t * angle) / sine;
        value.w = from.w * start_weight + to.w * goal_weight;
        value.x = from.x * start_weight + to.x * goal_weight;
        value.y = from.y * start_weight + to.y * goal_weight;
        value.z = from.z * start_weight + to.z * goal_weight;
    }
    if (!Normalize(value)) value = from;
    return QuaternionPose(value, position);
}

[[nodiscard]] float RotationDistance(
    const VrMatrix44& start,
    const VrMatrix44& goal) noexcept {
    Quaternion from{};
    Quaternion to{};
    if (!PoseQuaternion(start, from) || !PoseQuaternion(goal, to)) return 0.0F;
    const float dot = std::clamp(std::fabs(QuaternionDot(from, to)), 0.0F, 1.0F);
    return 2.0F * std::acos(dot);
}

[[nodiscard]] bool UsablePose(
    const VrMatrix44& pose,
    const VrHandResolverFrame& frame) noexcept {
    for (const float value : pose.values) {
        if (!std::isfinite(value)) return false;
    }
    Quaternion ignored{};
    if (!PoseQuaternion(pose, ignored)) return false;
    const auto position = Translation(pose);
    if (!Finite(position)) return false;
    if (frame.head_basis_valid) {
        const float distance = Distance(position, frame.head);
        if (!std::isfinite(distance) ||
            distance > vr_interaction_policy::kMaxTrackedHandDistanceFromHead) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] VrHandContactVector FallbackAnchor(
    const VrHandResolverFrame& frame) noexcept {
    const float side = frame.left_hand ? -0.13F : 0.13F;
    return Add(Add(Add(frame.head, Scale(frame.right, side)),
                   Scale(frame.up, -0.18F)),
        Scale(frame.forward, 0.04F));
}

[[nodiscard]] bool Query(
    void* context,
    VrHandCollisionQuery query,
    const VrMatrix44& pose,
    const VrHandContactVector& motion,
    float tolerance,
    VrHandCollisionDecision& decision) noexcept {
    decision = {};
    if (query == nullptr || !query(context, pose, motion, tolerance, decision)) {
        return false;
    }
    if (!Finite(decision.normal) || !std::isfinite(decision.depth) ||
        decision.depth < 0.0F) {
        return false;
    }
    return true;
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

void ResetVrHandResolveState(VrHandResolveState& state) noexcept {
    state = {};
    state.raw_pose = IdentityMatrix();
    state.resolved_pose = IdentityMatrix();
}

bool ResolveVrHandPose(
    VrHandResolveState& state,
    const VrMatrix44& raw_pose,
    const VrHandResolverFrame& frame,
    void* query_context,
    VrHandCollisionQuery query,
    VrMatrix44& resolved_pose) noexcept {
    using namespace vr_interaction_policy;

    state.last_tracking_reanchor = false;
    state.last_recovery_anchor = false;
    state.last_pullback_recovery = false;
    state.last_interaction_assist = false;

    if (!UsablePose(raw_pose, frame)) {
        if (state.valid) {
            resolved_pose = state.resolved_pose;
        } else {
            resolved_pose = IdentityMatrix();
            if (frame.head_basis_valid) {
                SetTranslation(resolved_pose, FallbackAnchor(frame));
            }
        }
        return true;
    }
    if (state.valid && SamePose(raw_pose, state.raw_pose)) {
        resolved_pose = state.resolved_pose;
        return true;
    }

    const auto raw_position = Translation(raw_pose);
    const bool first_pose = !state.valid;
    const bool reanchor = !first_pose &&
        Distance(Translation(state.raw_pose), raw_position) >
            kTrackingReanchorDistance;
    state.last_tracking_reanchor = reanchor;

    bool initial_overlap = false;
    if (first_pose) {
        VrHandCollisionDecision decision;
        if (!Query(query_context, query, raw_pose, {}, kContactTolerance,
                decision)) {
            resolved_pose = frame.head_basis_valid ? raw_pose : IdentityMatrix();
            if (frame.head_basis_valid) SetTranslation(resolved_pose, FallbackAnchor(frame));
            return false;
        }
        initial_overlap = decision.blocking;
    }

    bool pullback_recovery = false;
    if (!first_pose && frame.head_basis_valid) {
        const auto candidates = VrHandRecoveryCandidates(frame.head, frame.right,
            frame.up, frame.forward, frame.left_hand);
        pullback_recovery = ShouldUseRecoveryAnchor(state.constrained_frames,
            Distance(raw_position, candidates[0]),
            Distance(Translation(state.raw_pose), candidates[0]));
    }

    VrMatrix44 recovery_pose = raw_pose;
    bool using_recovery = false;
    if ((initial_overlap || reanchor || pullback_recovery) &&
        frame.head_basis_valid) {
        const auto candidates = VrHandRecoveryCandidates(frame.head, frame.right,
            frame.up, frame.forward, frame.left_hand);
        for (const auto& candidate : candidates) {
            VrMatrix44 test = raw_pose;
            SetTranslation(test, candidate);
            VrHandCollisionDecision decision;
            if (!Query(query_context, query, test, {}, kContactTolerance,
                    decision)) {
                resolved_pose = state.valid ? state.resolved_pose : test;
                return false;
            }
            if (!decision.blocking) {
                recovery_pose = test;
                using_recovery = true;
                break;
            }
        }
    }
    state.last_recovery_anchor = using_recovery;
    state.last_pullback_recovery = using_recovery && pullback_recovery;

    VrHandContactVector start_position{};
    if (using_recovery) {
        start_position = Translation(recovery_pose);
    } else if (first_pose) {
        start_position = raw_position;
    } else if (reanchor && frame.head_basis_valid) {
        start_position = FallbackAnchor(frame);
    } else if (state.valid) {
        start_position = Translation(state.resolved_pose);
    } else {
        start_position = raw_position;
    }

    VrMatrix44 sweep_pose = (reanchor || first_pose || using_recovery)
        ? raw_pose : state.resolved_pose;
    SetTranslation(sweep_pose, start_position);
    bool interaction_assist = frame.interaction_assist;
    if (frame.interaction_target_valid) {
        const auto to_target = Subtract(frame.interaction_target, start_position);
        const auto controller_motion = Subtract(raw_position, start_position);
        interaction_assist =
            Length(to_target) <= kInteractionTargetDistance &&
            Dot(controller_motion, controller_motion) > 0.000001F &&
            Dot(controller_motion, to_target) > 0.0F;
    }
    state.last_interaction_assist = interaction_assist;
    const float tolerance = ContactTolerance(interaction_assist);

    for (int iteration = 0; iteration < kOverlapResolveIterations; ++iteration) {
        SetTranslation(sweep_pose, start_position);
        VrHandCollisionDecision decision;
        if (!Query(query_context, query, sweep_pose, {}, tolerance, decision)) {
            resolved_pose = state.valid ? state.resolved_pose : sweep_pose;
            return false;
        }
        if (!decision.blocking) break;
        if (Length(decision.normal) < 0.001F || !std::isfinite(decision.depth)) break;
        const float push = decision.depth - tolerance + kDepenetrationSlop;
        start_position = Add(start_position, Scale(decision.normal, push));
    }

    VrHandContactVector current = start_position;
    VrHandContactVector goal = raw_position;
    for (int slide = 0; slide < kSlideIterations; ++slide) {
        const auto delta = Subtract(goal, current);
        const float distance = Length(delta);
        if (!std::isfinite(distance)) {
            current = state.valid ? Translation(state.resolved_pose) : start_position;
            break;
        }
        if (distance < 0.0001F) break;

        const int steps = SweepStepCount(distance);
        float safe_t = 0.0F;
        float hit_t = 1.0F;
        VrHandContactVector hit_normal{};
        bool hit = false;
        for (int step = 1; step <= steps; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            VrMatrix44 test = sweep_pose;
            SetTranslation(test, Add(current, Scale(delta, t)));
            VrHandCollisionDecision decision;
            if (!Query(query_context, query, test, delta, tolerance, decision)) {
                resolved_pose = state.valid ? state.resolved_pose : sweep_pose;
                return false;
            }
            if (decision.blocking) {
                hit = true;
                hit_t = t;
                hit_normal = decision.normal;
                break;
            }
            safe_t = t;
        }
        if (!hit) {
            current = goal;
            break;
        }
        for (int refine = 0; refine < kSweepRefineIterations; ++refine) {
            const float mid_t = (safe_t + hit_t) * 0.5F;
            VrMatrix44 test = sweep_pose;
            SetTranslation(test, Add(current, Scale(delta, mid_t)));
            VrHandCollisionDecision decision;
            if (!Query(query_context, query, test, delta, tolerance, decision)) {
                resolved_pose = state.valid ? state.resolved_pose : sweep_pose;
                return false;
            }
            if (decision.blocking) {
                hit_t = mid_t;
                hit_normal = decision.normal;
            } else {
                safe_t = mid_t;
            }
        }
        current = Add(current, Scale(delta, safe_t));
        if (Length(hit_normal) < 0.001F) break;
        auto remaining = Subtract(goal, current);
        const float into_surface = Dot(remaining, hit_normal);
        if (into_surface >= 0.0F) break;
        remaining = Subtract(remaining, Scale(hit_normal, into_surface));
        if (Length(remaining) < 0.0001F) break;
        goal = Add(current, remaining);
    }

    VrMatrix44 rotation_start = sweep_pose;
    SetTranslation(rotation_start, current);
    VrMatrix44 rotation_goal = raw_pose;
    SetTranslation(rotation_goal, current);
    const float rotation_distance = RotationDistance(rotation_start, rotation_goal);
    int rotation_steps = static_cast<int>(rotation_distance / kRotationStepRadians) + 1;
    if (rotation_steps < 1) rotation_steps = 1;
    float safe_rotation_t = 0.0F;
    float hit_rotation_t = 1.0F;
    bool rotation_hit = false;
    for (int step = 1; step <= rotation_steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(rotation_steps);
        const auto test = InterpolateRotation(rotation_start, rotation_goal, t, current);
        VrHandCollisionDecision decision;
        if (!Query(query_context, query, test, {}, tolerance, decision)) {
            resolved_pose = state.valid ? state.resolved_pose : rotation_start;
            return false;
        }
        if (decision.blocking) {
            rotation_hit = true;
            hit_rotation_t = t;
            break;
        }
        safe_rotation_t = t;
    }
    if (rotation_hit) {
        for (int refine = 0; refine < kRotationRefineIterations; ++refine) {
            const float mid_t = (safe_rotation_t + hit_rotation_t) * 0.5F;
            const auto test = InterpolateRotation(
                rotation_start, rotation_goal, mid_t, current);
            VrHandCollisionDecision decision;
            if (!Query(query_context, query, test, {}, tolerance, decision)) {
                resolved_pose = state.valid ? state.resolved_pose : rotation_start;
                return false;
            }
            if (decision.blocking) hit_rotation_t = mid_t;
            else safe_rotation_t = mid_t;
        }
    } else {
        safe_rotation_t = 1.0F;
    }

    if (!Finite(current)) {
        current = state.valid ? Translation(state.resolved_pose) : start_position;
    }
    resolved_pose = InterpolateRotation(
        rotation_start, rotation_goal, safe_rotation_t, current);
    if (!UsablePose(resolved_pose, frame)) {
        resolved_pose = state.valid ? state.resolved_pose : sweep_pose;
        return false;
    }

    if (using_recovery) state.constrained_frames = 0;
    if (Distance(raw_position, current) > kConstrainedHandDistance) {
        if (state.constrained_frames < 1'000'000) ++state.constrained_frames;
    } else {
        state.constrained_frames = 0;
    }
    state.raw_pose = raw_pose;
    state.resolved_pose = resolved_pose;
    state.valid = true;
    return true;
}

} // namespace penumbra_vr::runtime
