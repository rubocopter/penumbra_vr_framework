#include "vr_hand_contact.hpp"
#include "vr_interaction_policy.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using penumbra_vr::runtime::VrHandContactAccumulator;
using penumbra_vr::runtime::VrHandContactVector;
using penumbra_vr::runtime::VrHandCollisionDecision;
using penumbra_vr::runtime::VrHandResolveState;
using penumbra_vr::runtime::VrHandResolverFrame;
using penumbra_vr::runtime::VrMatrix44;

[[nodiscard]] bool Near(float left, float right, float epsilon = 1.0e-5F) {
    return std::fabs(left - right) <= epsilon;
}

[[nodiscard]] bool TestBlockingNormalSelection() {
    VrHandContactAccumulator contacts({1.0F, 0.0F, 0.0F}, 0.002F);
    contacts.Add(0.010F, {0.0F, 1.0F, 0.0F});
    contacts.Add(0.008F, {-2.0F, 0.0F, 0.0F});
    contacts.Add(0.020F, {1.0F, 0.0F, 0.0F});
    const auto& summary = contacts.summary();
    return summary.has_contact && summary.has_blocking_normal &&
        Near(summary.max_depth, 0.020F) && Near(summary.best_depth, 0.008F) &&
        Near(summary.best_normal[0], -1.0F);
}

[[nodiscard]] bool TestToleranceAndCorrectionFallback() {
    VrHandContactAccumulator skin({}, 0.002F);
    skin.Add(0.001F, {0.0F, 1.0F, 0.0F});
    auto decision = penumbra_vr::runtime::ResolveVrHandCollision(
        true, {}, {0.0F, 0.001F, 0.0F}, skin.summary());
    if (decision.blocking) return false;

    VrHandContactAccumulator missing_contact({}, 0.002F);
    decision = penumbra_vr::runtime::ResolveVrHandCollision(
        true, {}, {0.0F, -0.010F, 0.0F}, missing_contact.summary());
    return decision.blocking && Near(decision.depth, 0.010F) &&
        Near(decision.normal[1], -1.0F);
}

[[nodiscard]] bool TestRecoveryCandidates() {
    const VrHandContactVector head{1.0F, 2.0F, 3.0F};
    const VrHandContactVector right{1.0F, 0.0F, 0.0F};
    const VrHandContactVector up{0.0F, 1.0F, 0.0F};
    const VrHandContactVector forward{0.0F, 0.0F, 1.0F};
    const auto right_hand = penumbra_vr::runtime::VrHandRecoveryCandidates(
        head, right, up, forward, false);
    const auto left_hand = penumbra_vr::runtime::VrHandRecoveryCandidates(
        head, right, up, forward, true);
    using namespace penumbra_vr::runtime::vr_interaction_policy;
    return Near(right_hand[0][0], 1.0F + kRecoveryAnchorSide) &&
        Near(left_hand[0][0], 1.0F - kRecoveryAnchorSide) &&
        Near(right_hand[0][1], 2.0F - kRecoveryAnchorDown) &&
        Near(right_hand[0][2], 3.0F + kRecoveryAnchorForward) &&
        Near(right_hand[1][0], 1.10F) && Near(right_hand[1][1], 1.68F) &&
        Near(right_hand[1][2], 3.02F) && Near(right_hand[2][0], 1.20F) &&
        Near(right_hand[2][1], 1.80F) && Near(right_hand[2][2], 2.98F) &&
        Near(right_hand[3][0], 1.0F) && Near(right_hand[3][1], 1.70F) &&
        Near(right_hand[3][2], 3.0F);
}

struct QueryScene {
    float wall_x = 100.0F;
    float rotation_limit = 100.0F;
    bool malformed = false;
    int calls = 0;
    float last_tolerance = 0.0F;
};

[[nodiscard]] bool TestQuery(void* context, const VrMatrix44& pose,
    const VrHandContactVector&, float tolerance,
    VrHandCollisionDecision& decision) noexcept {
    auto& scene = *static_cast<QueryScene*>(context);
    ++scene.calls;
    scene.last_tolerance = tolerance;
    if (scene.malformed) {
        decision.blocking = true;
        decision.normal = {std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F};
        decision.depth = 0.1F;
        return true;
    }
    const float x = pose.values[3];
    const float penetration = x - scene.wall_x;
    if (penetration > tolerance + 1.0e-5F) {
        decision.blocking = true;
        decision.normal = {-1.0F, 0.0F, 0.0F};
        decision.depth = penetration;
        return true;
    }
    const float angle = std::fabs(std::atan2(pose.values[4], pose.values[0]));
    if (angle > scene.rotation_limit) {
        decision.blocking = true;
        decision.normal = {-1.0F, 0.0F, 0.0F};
        decision.depth = 0.01F;
    }
    return true;
}

[[nodiscard]] VrMatrix44 Pose(float x, float y, float z, float yaw = 0.0F) {
    auto pose = penumbra_vr::runtime::IdentityMatrix();
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    pose.values[0] = cosine;
    pose.values[1] = -sine;
    pose.values[4] = sine;
    pose.values[5] = cosine;
    pose.values[3] = x;
    pose.values[7] = y;
    pose.values[11] = z;
    return pose;
}

[[nodiscard]] bool TestResolverSweepSlideAndIdempotence() {
    VrHandResolveState state;
    penumbra_vr::runtime::ResetVrHandResolveState(state);
    VrHandResolverFrame frame;
    QueryScene scene;
    VrMatrix44 resolved{};
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.0F, 0.0F, 0.0F), frame, &scene, TestQuery, resolved)) {
        return false;
    }
    const int first_calls = scene.calls;
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.0F, 0.0F, 0.0F), frame, &scene, TestQuery, resolved) ||
        scene.calls != first_calls) {
        return false;
    }
    scene.wall_x = 0.5F;
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(1.0F, 0.0F, 1.0F), frame, &scene, TestQuery, resolved)) {
        return false;
    }
    const bool ok = resolved.values[3] <= 0.503F && resolved.values[3] >= 0.48F &&
        Near(resolved.values[11], 1.0F, 0.002F);
    if (!ok) {
        std::cerr << "resolver slide result x=" << resolved.values[3]
                  << " z=" << resolved.values[11] << " calls=" << scene.calls << '\n';
    }
    return ok;
}

[[nodiscard]] bool TestResolverOverlapAndRecovery() {
    VrHandResolveState state;
    penumbra_vr::runtime::ResetVrHandResolveState(state);
    VrHandResolverFrame frame;
    QueryScene scene;
    scene.wall_x = 0.5F;
    VrMatrix44 resolved{};
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.7F, 0.0F, 0.0F), frame, &scene, TestQuery, resolved) ||
        resolved.values[3] > 0.503F) {
        return false;
    }

    frame.head_basis_valid = true;
    frame.head = {0.0F, 1.7F, 0.0F};
    state.raw_pose = Pose(0.7F, 1.46F, 0.03F);
    state.resolved_pose = Pose(0.49F, 1.46F, 0.03F);
    state.valid = true;
    state.constrained_frames =
        penumbra_vr::runtime::vr_interaction_policy::kConstrainedRecoveryFrames;
    scene.wall_x = 100.0F;
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.2F, 1.46F, 0.03F), frame, &scene, TestQuery, resolved)) {
        return false;
    }
    return state.constrained_frames == 0 && Near(resolved.values[3], 0.2F, 0.002F);
}

[[nodiscard]] bool TestResolverInteractionAssistUsesChosenStart() {
    using namespace penumbra_vr::runtime::vr_interaction_policy;
    VrHandResolveState state;
    penumbra_vr::runtime::ResetVrHandResolveState(state);
    state.raw_pose = Pose(0.70F, 1.46F, 0.03F);
    state.resolved_pose = Pose(0.49F, 1.46F, 0.03F);
    state.valid = true;
    state.constrained_frames = kConstrainedRecoveryFrames;

    VrHandResolverFrame frame;
    frame.head_basis_valid = true;
    frame.head = {0.0F, 1.70F, 0.0F};
    frame.interaction_target = {0.30F, 1.46F, 0.03F};
    frame.interaction_target_valid = true;

    QueryScene scene;
    VrMatrix44 resolved{};
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.20F, 1.46F, 0.03F), frame, &scene, TestQuery, resolved)) {
        return false;
    }
    return state.last_recovery_anchor && state.last_interaction_assist &&
        Near(scene.last_tolerance, kInteractionContactTolerance);
}

[[nodiscard]] bool TestResolverRotationAndMalformedFailSafe() {
    VrHandResolveState state;
    penumbra_vr::runtime::ResetVrHandResolveState(state);
    VrHandResolverFrame frame;
    QueryScene scene;
    VrMatrix44 resolved{};
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.0F, 0.0F, 0.0F), frame, &scene, TestQuery, resolved)) {
        return false;
    }
    scene.rotation_limit = 0.35F;
    if (!penumbra_vr::runtime::ResolveVrHandPose(
            state, Pose(0.0F, 0.0F, 0.0F, 1.0F), frame, &scene, TestQuery, resolved)) {
        return false;
    }
    const float stopped_angle = std::fabs(std::atan2(resolved.values[4], resolved.values[0]));
    if (!(stopped_angle > 0.30F && stopped_angle <= 0.36F)) return false;
    const auto safe = resolved;
    scene.malformed = true;
    const bool success = penumbra_vr::runtime::ResolveVrHandPose(
        state, Pose(0.2F, 0.0F, 0.0F, 1.1F), frame, &scene, TestQuery, resolved);
    return !success && resolved.values == safe.values;
}

} // namespace

int main() {
    if (!TestBlockingNormalSelection()) {
        std::cerr << "VR hand blocking-normal selection failed\n";
        return 1;
    }
    if (!TestToleranceAndCorrectionFallback()) {
        std::cerr << "VR hand contact tolerance/fallback failed\n";
        return 2;
    }
    if (!TestRecoveryCandidates()) {
        std::cerr << "VR hand recovery candidates failed\n";
        return 3;
    }
    if (!TestResolverSweepSlideAndIdempotence()) {
        std::cerr << "VR hand resolver sweep/slide/idempotence failed\n";
        return 4;
    }
    if (!TestResolverOverlapAndRecovery()) {
        std::cerr << "VR hand resolver overlap/recovery failed\n";
        return 5;
    }
    if (!TestResolverRotationAndMalformedFailSafe()) {
        std::cerr << "VR hand resolver rotation/fail-safe failed\n";
        return 6;
    }
    if (!TestResolverInteractionAssistUsesChosenStart()) {
        std::cerr << "VR hand interaction assistance/recovery start failed\n";
        return 7;
    }
    std::cout << "VR hand contact policy passed\n";
    return 0;
}
