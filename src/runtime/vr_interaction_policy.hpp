#pragma once

namespace penumbra_vr::runtime::vr_interaction_policy {

// Proven Overture palm collision policy from Rework 23c890f. The runtime owns
// these game-neutral dimensions, sweep/refinement limits and recovery rules;
// each backend still owns its native shape/contact queries and body exclusions.
inline constexpr float kCollisionSizeX = 0.190F;
inline constexpr float kCollisionSizeY = 0.052F;
inline constexpr float kCollisionSizeZ = 0.125F;

inline constexpr float kSweepStep = 0.020F;
inline constexpr int kSlideIterations = 4;
inline constexpr int kSweepRefineIterations = 7;

inline constexpr float kContactTolerance = 0.002F;
inline constexpr float kInteractionContactTolerance = 0.008F;
inline constexpr float kInteractionTargetDistance = 0.40F;
inline constexpr float kDepenetrationSlop = 0.00025F;
inline constexpr int kOverlapResolveIterations = 4;

inline constexpr float kRotationStepRadians = 0.130899694F;
inline constexpr int kRotationRefineIterations = 7;

inline constexpr float kTrackingReanchorDistance = 0.75F;
inline constexpr float kRecoveryAnchorSide = 0.15F;
inline constexpr float kRecoveryAnchorDown = 0.24F;
inline constexpr float kRecoveryAnchorForward = 0.03F;
inline constexpr float kConstrainedHandDistance = 0.10F;
inline constexpr int kConstrainedRecoveryFrames = 12;
inline constexpr float kRecoveryPullbackProgress = 0.0015F;
inline constexpr float kMaxTrackedHandDistanceFromHead = 2.50F;

// Rework permits collision-constrained palms to follow the raw controller by
// at most this distance when resolving a physical interaction target.
inline constexpr float kMaximumCollisionInteractionReach = 0.18F;
inline constexpr float kMaxCollisionInteractionReach =
    kMaximumCollisionInteractionReach;

// Long-range assistance is a separate, item-only path. Backends must not use
// this value for ordinary props or mechanisms.
inline constexpr float kMagneticItemRange = 2.35F;

// Rework 23c890f maps measured handle radius to the forced hand closure used
// while an attachment is held. Geometry, grip points and twist remain profile
// data owned by each game/backend.
inline constexpr float kDefaultGripRadius = 0.018F;
inline constexpr float kMinimumGripPoseWeight = 0.72F;
inline constexpr float kMaximumGripPoseWeight = 1.0F;
inline constexpr float kGripPoseBase = 1.15F;
inline constexpr float kGripRadiusPoseScale = 12.5F;
inline constexpr float kGripOpenCentreScale = 0.0325F;

[[nodiscard]] constexpr float GripPoseWeight(float radius) noexcept {
    const float weight = kGripPoseBase - radius * kGripRadiusPoseScale;
    return weight < kMinimumGripPoseWeight ? kMinimumGripPoseWeight :
        weight > kMaximumGripPoseWeight ? kMaximumGripPoseWeight : weight;
}

[[nodiscard]] constexpr float GripOpenCentreOffset(float radius) noexcept {
    return (1.0F - GripPoseWeight(radius)) * kGripOpenCentreScale;
}

[[nodiscard]] constexpr float ContactTolerance(
    bool interaction_assist) noexcept {
    return interaction_assist ? kInteractionContactTolerance : kContactTolerance;
}

[[nodiscard]] constexpr bool IsBlockingPenetration(
    float depth,
    bool interaction_assist) noexcept {
    return depth > ContactTolerance(interaction_assist);
}

[[nodiscard]] constexpr int SweepStepCount(float distance) noexcept {
    if (distance <= 0.0F) {
        return 1;
    }
    return static_cast<int>(distance / kSweepStep) + 1;
}

[[nodiscard]] constexpr bool ShouldUseRecoveryAnchor(
    int constrained_frames,
    float current_distance_to_anchor,
    float previous_distance_to_anchor) noexcept {
    return constrained_frames >= kConstrainedRecoveryFrames &&
           current_distance_to_anchor + kRecoveryPullbackProgress <
               previous_distance_to_anchor;
}

[[nodiscard]] float ClampPhysicalInteractionReach(
    float native_reach) noexcept;

} // namespace penumbra_vr::runtime::vr_interaction_policy
