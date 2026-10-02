#include "VRInteractionSightPolicy.h"

#include <iostream>

int main() {
    using OvertureVRInteractionSightPolicy::ShouldRayTestBody;

    if (!ShouldRayTestBody(true, true, false)) {
        std::cerr << "Candidate body must remain ray-testable\n";
        return 1;
    }
    if (ShouldRayTestBody(false, true, true)) {
        std::cerr << "Sibling body from the target entity must not occlude the target\n";
        return 1;
    }
    if (!ShouldRayTestBody(false, false, true)) {
        std::cerr << "Unrelated colliding geometry must still occlude the target\n";
        return 1;
    }
    if (ShouldRayTestBody(false, false, false)) {
        std::cerr << "Unrelated non-colliding helpers must not occlude the target\n";
        return 1;
    }

    // Two drawer fronts overlap the acquisition volume. The nearer one is
    // behind the frame; it must not suppress the accessible front in either
    // portal iteration order. Keep the existing nearest-visible rule.
    for (bool hidden_first : {false, true}) {
        float best = 9999.0f;
        int selected = -1;
        for (int index = 0; index != 2; ++index) {
            const bool hidden = (index == 0) == hidden_first;
            const float distance = hidden ? 0.01f : 0.02f;
            if (OvertureVRInteractionSightPolicy::IsBetterVisibleCandidate(
                    distance, best, [hidden] { return !hidden; })) {
                best = distance;
                selected = hidden ? 0 : 1;
            }
        }
        if (selected != 1) {
            std::cerr << "Occluded drawer must not suppress accessible contact\n";
            return 1;
        }
    }
    int ray_tests = 0;
    if (OvertureVRInteractionSightPolicy::IsBetterVisibleCandidate(
            0.03f, 0.02f, [&] { ++ray_tests; return true; }) || ray_tests != 0) {
        std::cerr << "Worse candidate must not issue an unnecessary sight query\n";
        return 1;
    }

    std::cout << "Overture interaction sight policy passed\n";
    return 0;
}
