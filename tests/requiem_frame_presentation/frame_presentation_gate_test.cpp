#include "frame_presentation_gate.hpp"
#include "tool_visibility_tracking.hpp"

#include <cmath>
#include <iostream>

int main() {
    using penumbra_vr::backends::requiem::FramePresentationGate;
    using penumbra_vr::backends::requiem::ComposeToolVisibilityTracking;
    using penumbra_vr::runtime::IdentityMatrix;
    using penumbra_vr::runtime::VrMatrix34;
    using penumbra_vr::runtime::VrMatrix44;

    auto head_view = IdentityMatrix();
    head_view.values[3] = -10.0F;
    head_view.values[7] = -3.0F;
    head_view.values[11] = -5.0F;
    VrMatrix34 tracked_head{{1, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 3}};
    VrMatrix44 tracking_world{};
    std::string tracking_error;
    if (!ComposeToolVisibilityTracking(head_view, tracked_head,
            tracking_world, tracking_error) ||
        std::abs(tracking_world.values[3] - 9.0F) > 0.0001F ||
        std::abs(tracking_world.values[7] - 1.0F) > 0.0001F ||
        std::abs(tracking_world.values[11] - 2.0F) > 0.0001F) {
        std::cerr << "Requiem tool visibility uses a different tracking basis from the eye\n";
        return 6;
    }
    tracked_head.values[0] = 2.0F;
    if (ComposeToolVisibilityTracking(head_view, tracked_head,
            tracking_world, tracking_error)) {
        std::cerr << "Requiem tool visibility accepted a non-rigid HMD pose\n";
        return 7;
    }

    FramePresentationGate gate;

    if (gate.ConsumeWorldPresentedAtSwap()) {
        std::cerr << "A fresh swap incorrectly reports a world presentation\n";
        return 1;
    }

    gate.MarkWorldPresented();
    if (!gate.ConsumeWorldPresentedAtSwap()) {
        std::cerr << "A world presentation was not observed by the next swap\n";
        return 2;
    }

    if (gate.ConsumeWorldPresentedAtSwap()) {
        std::cerr << "World presentation state leaked into the following swap\n";
        return 3;
    }

    gate.MarkWorldPresented();
    gate.MarkWorldPresented();
    if (!gate.ConsumeWorldPresentedAtSwap() ||
        gate.ConsumeWorldPresentedAtSwap()) {
        std::cerr << "Multiple world passes did not collapse to one swap decision\n";
        return 4;
    }

    gate.MarkWorldPresented();
    gate.Reset();
    if (gate.ConsumeWorldPresentedAtSwap()) {
        std::cerr << "Reset did not clear stale world presentation state\n";
        return 5;
    }

    std::cout << "Requiem frame presentation gate passed\n";
    return 0;
}
