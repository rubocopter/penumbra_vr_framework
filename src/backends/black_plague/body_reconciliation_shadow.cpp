#include "body_reconciliation_shadow.hpp"

#include <cmath>

namespace penumbra_vr::backends::black_plague {
namespace {

[[nodiscard]] bool Finite(const std::array<float, 3>& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

[[nodiscard]] float HorizontalDistance(
    const std::array<float, 3>& left,
    const std::array<float, 3>& right) noexcept {
    return std::hypot(left[0] - right[0], left[2] - right[2]);
}

[[nodiscard]] bool SamePosition(
    const std::array<float, 3>& left,
    const std::array<float, 3>& right) noexcept {
    constexpr float epsilon = 0.00001F;
    return std::abs(left[0] - right[0]) <= epsilon &&
        std::abs(left[1] - right[1]) <= epsilon &&
        std::abs(left[2] - right[2]) <= epsilon;
}

} // namespace

void BodyReconciliationShadow::Reset() noexcept {
    *this = {};
}

BodyReconciliationTickPlan BodyReconciliationShadow::PrepareTick(
    const runtime::VrMatrix34& head_tracking_pose,
    float tracking_world_yaw,
    std::uint64_t body_generation,
    const std::array<float, 3>& body_before,
    float delta_seconds,
    runtime::VrTrackingSampleIdentity tracking_identity) noexcept {
    BodyReconciliationTickPlan tick;
    if (tick_pending_ || body_generation == 0 || !Finite(body_before) ||
        !std::isfinite(delta_seconds) || delta_seconds <= 0.0F ||
        delta_seconds > 0.25F || !std::isfinite(tracking_world_yaw)) {
        Reset();
        return {};
    }

    runtime::VrTrackingSpace tracking;
    tracking.SetHeadTrackingPose(head_tracking_pose);
    tracking.SetWorldYaw(tracking_world_yaw);
    runtime::VrMatrix44 validated_head;
    std::string error;
    if (!tracking.HeadWorldPose(validated_head, error)) {
        Reset();
        return {};
    }

    tick.body_generation = body_generation;
    tick.tracking_identity = tracking_identity;
    tick.tracking_pose = head_tracking_pose;
    tick.body_before = body_before;
    tick.head_tracking_position = {
        head_tracking_pose.values[3],
        head_tracking_pose.values[7],
        head_tracking_pose.values[11],
    };
    if (!Finite(tick.head_tracking_position)) {
        Reset();
        return {};
    }

    constexpr float limit =
        runtime::vr_locomotion_policy::kMaximumHeadBodySeparation;
    tick.reset = !initialized_ || generation_ != body_generation ||
        HorizontalDistance(previous_body_, body_before) > limit ||
        HorizontalDistance(previous_head_, tick.head_tracking_position) > limit;
    if (tick.reset) {
        tick.reconciliation = runtime::PlanBodyReconciliation(
            body_before, body_before, {});
    } else {
        tick.physical_delta = tracking.TrackingDirectionToWorld({
            tick.head_tracking_position[0] - previous_head_[0],
            0.0F,
            tick.head_tracking_position[2] - previous_head_[2],
        });
        tick.reconciliation = runtime::PlanBodyReconciliation(
            anchor_, body_before, tick.physical_delta);
    }
    if (!tick.reconciliation.valid || !Finite(tick.physical_delta)) {
        Reset();
        return {};
    }

    tick.sequence = ++next_sequence_;
    tick.valid = true;
    tick_pending_ = true;
    pending_sequence_ = tick.sequence;
    return tick;
}

BodyReconciliationShadowSample BodyReconciliationShadow::CompleteTick(
    const BodyReconciliationTickPlan& tick,
    const runtime::VrAcceptedBodyMotion& native_motion,
    const std::array<float, 3>& feet_after,
    bool physical_observation_available,
    const runtime::VrAcceptedBodyMotion& physical_motion) noexcept {
    BodyReconciliationShadowSample sample;
    runtime::VrAcceptedBodyMotion checked;
    if (!tick.valid || !tick_pending_ || tick.sequence == 0 ||
        tick.sequence != pending_sequence_ || !Finite(feet_after) ||
        !runtime::ObserveAcceptedBodyMotion(
            native_motion.body_before, native_motion.body_after, checked) ||
        checked.accepted_displacement != native_motion.accepted_displacement ||
        !SamePosition(checked.body_before, tick.body_before)) {
        Reset();
        return {};
    }

    tick_pending_ = false;
    pending_sequence_ = 0;
    sample.reset = tick.reset;
    sample.plan = tick.reconciliation;
    sample.physical_delta = tick.physical_delta;
    sample.native_motion = checked;

    if (tick.reset) {
        anchor_ = checked.body_after;
    } else {
        anchor_ = tick.reconciliation.head_anchor;
        if (physical_observation_available) {
            const auto reconciliation = runtime::ReconcilePhysicalBodyMotion(
                tick.reconciliation, physical_motion);
            if (!reconciliation.valid) {
                Reset();
                return {};
            }
            sample.physical_observation_available = true;
            sample.physical_motion = physical_motion;
            sample.physical_reconciliation = reconciliation;
            anchor_ = reconciliation.head_anchor;
        }

        // Black Plague exposes one combined collision solve. The whole accepted
        // displacement is observed; the physical/stick split supplied here is
        // derived from that solve. Carry only the derived non-physical part.
        auto locomotion_motion = checked;
        if (physical_observation_available) {
            for (const auto axis : {0U, 2U}) {
                locomotion_motion.accepted_displacement[axis] -=
                    physical_motion.accepted_displacement[axis];
                locomotion_motion.body_after[axis] =
                    locomotion_motion.body_before[axis] +
                    locomotion_motion.accepted_displacement[axis];
            }
        }
        locomotion_motion.accepted_displacement[1] = 0.0F;
        locomotion_motion.body_after[1] = locomotion_motion.body_before[1];
        sample.locomotion_carry_displacement =
            locomotion_motion.accepted_displacement;
        anchor_ = runtime::CarryHeadAnchorWithLocomotion(
            anchor_, locomotion_motion);
    }

    anchor_[1] = feet_after[1];
    for (const auto axis : {0U, 2U}) {
        sample.native_anchor_correction[axis] =
            anchor_[axis] - tick.reconciliation.head_anchor[axis];
    }
    previous_head_ = tick.head_tracking_position;
    previous_body_ = checked.body_after;
    generation_ = tick.body_generation;
    initialized_ = true;
    sample.predicted_anchor = anchor_;
    sample.separation = HorizontalDistance(anchor_, checked.body_after);
    sample.valid = tick.reconciliation.valid && Finite(anchor_) &&
        std::isfinite(sample.separation);
    if (!sample.valid) {
        Reset();
        return {};
    }
    return sample;
}

} // namespace penumbra_vr::backends::black_plague
