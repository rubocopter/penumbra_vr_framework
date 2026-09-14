#include "vr_hand_contact.hpp"
#include "vr_interaction_policy.hpp"

#include <cmath>
#include <iostream>

namespace {

using penumbra_vr::runtime::VrHandContactAccumulator;
using penumbra_vr::runtime::VrHandContactVector;

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
    std::cout << "VR hand contact policy passed\n";
    return 0;
}
