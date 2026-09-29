#pragma once

// Overture's Newton 1.x boundary. Loose-prop tuning remains the Rework baseline.
inline float VRNudgePointDeltaVelocity(float deltaVelocity, float mass,
    bool constrainedJoint)
{
    // NewtonAddBodyImpulse(pointDeltaVeloc, pointPosit) already solves the
    // body's mass/inertia. Multiplying a mechanism's delta-v by mass launches
    // heavy hinges/sliders beyond the bounded contact-speed request.
    return constrainedJoint ? deltaVelocity : deltaVelocity * mass;
}
