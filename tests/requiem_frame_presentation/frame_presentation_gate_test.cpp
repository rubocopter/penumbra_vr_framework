#include "frame_presentation_gate.hpp"

#include <iostream>

int main() {
    using penumbra_vr::backends::requiem::FramePresentationGate;

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
