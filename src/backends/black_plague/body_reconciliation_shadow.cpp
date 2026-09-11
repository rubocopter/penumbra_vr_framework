#include "body_reconciliation_shadow.hpp"

#include <cmath>

namespace penumbra_vr::backends::black_plague {
namespace {
bool Finite(const std::array<float, 3>& v) noexcept {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
float HorizontalDistance(const std::array<float, 3>& a,
    const std::array<float, 3>& b) noexcept {
    return std::hypot(a[0] - b[0], a[2] - b[2]);
}
} // namespace

void BodyReconciliationShadow::Reset() noexcept {
    *this = {};
}

BodyReconciliationShadowSample BodyReconciliationShadow::Observe(
    const runtime::VrMatrix34& head_tracking_pose,
    float tracking_world_yaw,
    std::uint64_t body_generation,
    const runtime::VrAcceptedBodyMotion& native_motion,
    const std::array<float, 3>& feet_after,
    float delta_seconds) noexcept {
    runtime::VrTrackingSpace tracking;
    tracking.SetHeadTrackingPose(head_tracking_pose);
    runtime::VrMatrix44 validated_head;
    std::string error;
    runtime::VrAcceptedBodyMotion checked;
    if (body_generation == 0 || !std::isfinite(delta_seconds) ||
        delta_seconds <= 0.0F || delta_seconds > 0.25F ||
        !std::isfinite(tracking_world_yaw) || !Finite(feet_after) ||
        !tracking.HeadWorldPose(validated_head, error) ||
        !runtime::ObserveAcceptedBodyMotion(native_motion.body_before,
            native_motion.body_after, checked) ||
        checked.accepted_displacement != native_motion.accepted_displacement) {
        Reset();
        return {};
    }
    tracking.SetWorldYaw(tracking_world_yaw);
    const std::array<float, 3> head{head_tracking_pose.values[3],
        head_tracking_pose.values[7], head_tracking_pose.values[11]};
    BodyReconciliationShadowSample sample;
    sample.native_motion = checked;
    // A lifecycle/tracking discontinuity is not collision rejection. Discard
    // the interval, seed at the new body/feet and never extrapolate across it.
    constexpr float limit = runtime::vr_locomotion_policy::kMaximumHeadBodySeparation;
    sample.reset = !initialized_ || generation_ != body_generation ||
        HorizontalDistance(previous_body_, checked.body_before) > limit ||
        HorizontalDistance(checked.body_before, checked.body_after) > limit ||
        HorizontalDistance(previous_head_, head) > limit;
    if (sample.reset) {
        anchor_ = checked.body_after;
        anchor_[1] = feet_after[1];
        sample.plan = runtime::PlanBodyReconciliation(anchor_, checked.body_after, {});
    } else {
        sample.physical_delta = tracking.TrackingDirectionToWorld({
            head[0] - previous_head_[0], 0.0F, head[2] - previous_head_[2]});
        sample.plan = runtime::PlanBodyReconciliation(
            anchor_, checked.body_before, sample.physical_delta);
        if (!sample.plan.valid) {
            Reset();
            return {};
        }
        // This native tick is NOT an observation of plan.physical_request.
        // No request was injected; physical acceptance/rejection is unknown.
        // Preserve Rework's 3D carry comparison but strip native jump Y from
        // horizontal locomotion. Feet Y is observed separately below.
        auto horizontal_motion = checked;
        horizontal_motion.accepted_displacement[1] = 0.0F;
        anchor_ = runtime::CarryHeadAnchorWithLocomotion(
            sample.plan.head_anchor, horizontal_motion);
        for (const auto axis : {0U, 2U})
            sample.native_anchor_correction[axis] =
                anchor_[axis] - sample.plan.head_anchor[axis];
        anchor_[1] = feet_after[1];
    }
    previous_head_ = head;
    previous_body_ = checked.body_after;
    generation_ = body_generation;
    initialized_ = true;
    sample.predicted_anchor = anchor_;
    sample.separation = HorizontalDistance(anchor_, checked.body_after);
    sample.valid = sample.plan.valid && Finite(anchor_) && std::isfinite(sample.separation);
    if (!sample.valid) { Reset(); return {}; }
    return sample;
}

} // namespace penumbra_vr::backends::black_plague
