#include "vr_panel_policy.hpp"

#include <iostream>

int main() {
    using namespace penumbra_vr::runtime;

    const auto opening = PlanStablePanelAnchor(true, false, false);
    const auto retained = PlanStablePanelAnchor(true, false, true);
    const auto recentered = PlanStablePanelAnchor(true, true, true);
    const auto closed = PlanStablePanelAnchor(false, false, true);
    if (!opening.capture_current_pose || !opening.anchor_valid_after ||
        retained.capture_current_pose || !retained.anchor_valid_after ||
        !recentered.capture_current_pose || !recentered.anchor_valid_after ||
        closed.capture_current_pose || closed.anchor_valid_after) {
        std::cerr << "Stable panel anchor lifetime changed\n";
        return 1;
    }

    const auto acquire = PlanTransientOverlayHandoff(true, false, false);
    const auto refresh = PlanTransientOverlayHandoff(true, false, true);
    const auto handoff = PlanTransientOverlayHandoff(true, true, true);
    const auto restore = PlanTransientOverlayHandoff(false, false, true);
    const auto blocked_close = PlanTransientOverlayHandoff(false, true, true);
    if (acquire.action != VrTransientOverlayAction::anchor_world_position ||
        !acquire.anchored_after ||
        refresh.action != VrTransientOverlayAction::anchor_world_position ||
        !refresh.anchored_after ||
        handoff.action != VrTransientOverlayAction::none || handoff.anchored_after ||
        restore.action != VrTransientOverlayAction::restore_face_locked ||
        restore.anchored_after ||
        blocked_close.action != VrTransientOverlayAction::none ||
        blocked_close.anchored_after) {
        std::cerr << "Transient overlay ownership handoff changed\n";
        return 2;
    }

    std::cout << "VR panel anchor and overlay handoff policy passed\n";
    return 0;
}
