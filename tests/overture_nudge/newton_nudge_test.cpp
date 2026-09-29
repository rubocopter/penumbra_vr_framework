#include "Newton.h"
#include "VRHandNudgePolicy.h"
#include <cmath>
#include <iostream>

int main() {
    const NewtonWorld* world = NewtonCreate(nullptr, nullptr);
    NewtonCollision* shape = NewtonCreateBox(world, 1, 2, 0.1F, nullptr);
    constexpr float position[3]{};
    for (float mass : {1.0F, 3.0F, 25.0F, 100.0F}) {
        NewtonBody* body = NewtonCreateBody(world, shape);
        NewtonBodySetMassMatrix(body, mass, mass, mass, mass);
        const float requested = VRNudgePointDeltaVelocity(0.36F, mass, true);
        const float delta[3]{requested, 0, 0};
        NewtonAddBodyImpulse(body, delta, position);
        float velocity[3]{};
        NewtonBodyGetVelocity(body, velocity);
        if (std::fabs(velocity[0] - 0.36F) > 1.0e-4F) {
            std::cerr << "Mechanism nudge exceeds contact delta-v: mass="
                      << mass << " dv=" << velocity[0] << '\n';
            NewtonDestroy(world);
            return 1;
        }
        NewtonDestroyBody(world, body);
    }
    // This release fix is scoped to native mechanisms, preserving loose props.
    if (std::fabs(VRNudgePointDeltaVelocity(0.1F, 25.0F, false) - 2.5F) > 1.0e-4F)
        return 1;
    NewtonReleaseCollision(world, shape);
    NewtonDestroy(world);
    return 0;
}
