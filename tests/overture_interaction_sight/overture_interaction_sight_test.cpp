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

    std::cout << "Overture interaction sight policy passed\n";
    return 0;
}
