#include "vr_locomotion.hpp"
#include "vr_interaction_policy.hpp"
#include "vr_tracking_space.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

using penumbra_vr::runtime::VrMatrix34;
using penumbra_vr::runtime::VrMatrix44;
using penumbra_vr::runtime::VrTrackingSpace;

[[nodiscard]] VrMatrix34 Pose(float x, float y, float z, float yaw = 0.0F) {
    const float cosine = std::cos(yaw);
    const float sine = std::sin(yaw);
    return {{
        cosine, 0.0F, sine, x,
        0.0F, 1.0F, 0.0F, y,
        -sine, 0.0F, cosine, z,
    }};
}

[[nodiscard]] bool Near(float actual, float expected, float tolerance = 1.0e-4F) {
    if (std::fabs(actual - expected) <= tolerance) {
        return true;
    }
    std::cerr << "Expected " << expected << " but got " << actual << '\n';
    return false;
}

[[nodiscard]] bool TestTrackingSpace() {
    VrTrackingSpace tracking;
    VrMatrix44 world;
    std::string error;
    if (!tracking.HeadWorldPose(world, error) || !Near(world.values[7], -0.213F)) {
        std::cerr << "Default legacy floor mapping failed: " << error << '\n';
        return false;
    }

    tracking.SetHeadTrackingPose(Pose(0.3F, 1.0F, -0.2F));
    tracking.SetHeightCalibration(0.05F);
    tracking.SetPostureOffset(0.01F);
    tracking.SetSeatedOffset(0.02F);
    tracking.SetPlayerWorldPosition({5.0F, 0.0F, 3.0F});
    if (!tracking.HeadWorldPose(world, error) ||
        !Near(world.values[3], 5.0F) ||
        !Near(world.values[7], 0.932F) ||
        !Near(world.values[11], 3.0F)) {
        std::cerr << "Height/player composition failed: " << error << '\n';
        return false;
    }

    tracking = {};
    tracking.SetWorldYaw(2.5F * 3.14159265358979323846F);
    if (!Near(tracking.world_yaw(), 0.5F * 3.14159265358979323846F)) {
        return false;
    }
    const auto direction = tracking.TrackingDirectionToWorld({0.0F, 0.0F, -1.0F});
    if (!Near(direction[0], -1.0F) || !Near(direction[2], 0.0F)) {
        return false;
    }

    tracking = {};
    tracking.SetHeadTrackingPose(Pose(0.0F, 1.7F, 0.0F, 0.6F));
    if (!tracking.RecenterOrientation(error) ||
        !tracking.HeadWorldPose(world, error) ||
        !Near(-world.values[2], 0.0F) ||
        !Near(-world.values[10], -1.0F)) {
        std::cerr << "Yaw recenter failed: " << error << '\n';
        return false;
    }

    VrMatrix34 invalid = Pose(0.0F, 1.7F, 0.0F);
    invalid.values[0] = 2.0F;
    tracking.SetHeadTrackingPose(invalid);
    if (tracking.HeadWorldPose(world, error) || error.empty()) {
        std::cerr << "Scaled tracking pose was accepted\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestLocomotionPolicy() {
    using namespace penumbra_vr::runtime;
    const auto identity = IdentityMatrix();
    const auto forward = HeadRelativeMoveDirection(identity, {true, 0.0F, 1.0F});
    if (!Near(forward[0], 0.0F) || !Near(forward[2], -1.0F)) {
        return false;
    }
    const auto walk = LocomotionDisplacement(forward, 0.1F, 1.0F, false, false);
    const auto run = LocomotionDisplacement(forward, 0.1F, 1.0F, false, true);
    const auto constrained = LocomotionDisplacement(forward, 0.1F, 1.0F, true, true);
    if (!Near(walk[2], -0.15F) || !Near(run[2], -0.225F) ||
        !Near(constrained[2], -0.05F)) {
        return false;
    }
    const auto step = ClampPhysicalBodyStep({1.0F, 4.0F, 0.0F}, {});
    if (!Near(step[0], 0.05F) || !Near(step[1], 0.0F) ||
        !Near(AcceptedDistanceAlongRequest(step, {0.02F, 0.0F, 0.0F}), 0.02F)) {
        return false;
    }
    if (!ShouldCarryHeadAnchorWithLocomotion(
            {0.1F, 0.0F, 0.0F}, {}, {0.0F, 0.0F, -0.2F}) ||
        ShouldCarryHeadAnchorWithLocomotion(
            {0.1F, 0.0F, 0.0F}, {}, {0.05F, 0.0F, 0.0F})) {
        return false;
    }
    if (!Near(vr_interaction_policy::ClampPhysicalInteractionReach(1.9F), 0.18F) ||
        !Near(vr_interaction_policy::ClampPhysicalInteractionReach(0.1F), 0.1F) ||
        vr_interaction_policy::ClampPhysicalInteractionReach(-1.0F) != 0.0F ||
        !Near(vr_interaction_policy::kMagneticItemRange, 2.35F)) {
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!TestTrackingSpace() || !TestLocomotionPolicy()) {
        return 1;
    }
    std::cout << "Rework tracking-space and locomotion policies passed\n";
    return 0;
}
