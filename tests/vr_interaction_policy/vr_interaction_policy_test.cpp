#include "vr_interaction_policy.hpp"
#include "vr_magnetic_pickup_policy.hpp"
#include "vr_mechanism_policy.hpp"

#include <cmath>
#include <iostream>

namespace {

[[nodiscard]] bool Near(float actual, float expected, float tolerance = 1.0e-4F) {
    if (std::fabs(actual - expected) <= tolerance) return true;
    std::cerr << "Expected " << expected << " but got " << actual << '\n';
    return false;
}

[[nodiscard]] bool TestMagneticPickupPolicy() {
    using namespace penumbra_vr::runtime::vr_magnetic_pickup_policy;
    const auto consumable = Profile(VrMagneticPickupClass::consumable);
    const auto ordinary = Profile(VrMagneticPickupClass::ordinary);
    const auto equipment = Profile(VrMagneticPickupClass::equipment);
    const auto unsupported = Profile(VrMagneticPickupClass::unsupported);
    if (!consumable.eligible || !Near(consumable.range, 2.35F) ||
        !Near(consumable.priority_bias, -0.20F) ||
        !ordinary.eligible || !Near(ordinary.range, 1.90F) ||
        !Near(ordinary.priority_bias, -0.10F) ||
        !equipment.eligible || !Near(equipment.range, 1.45F) ||
        !Near(equipment.priority_bias, 0.0F) || unsupported.eligible) {
        return false;
    }
    if (ForwardDistanceEligible(0.08F, 2.35F) ||
        !ForwardDistanceEligible(0.081F, 2.35F) ||
        ForwardDistanceEligible(2.351F, 2.35F) ||
        !Near(BodyAllowance(0.20F), 0.12F) ||
        !Near(ConeRadius(1.0F, 0.20F), 0.31F) ||
        !InsideAimCone(0.09F, 0.31F) || InsideAimCone(0.10F, 0.31F)) {
        return false;
    }
    const auto sample = VisibleSample(
        {2.0F, -2.0F, 0.5F}, {-1.0F, -1.0F, -1.0F},
        {1.0F, 1.0F, 1.0F}, {0.0F, 0.0F, 0.0F});
    if (!Near(sample[0], 0.9F) || !Near(sample[1], -0.9F) ||
        !Near(sample[2], 0.45F)) {
        return false;
    }
    const float centred = CandidateScore(0.0F, 0.30F, 1.0F, -0.20F);
    const float off_axis = CandidateScore(0.04F, 0.30F, 1.0F, -0.20F);
    return centred < off_axis && kRankedCandidateCount == 5U &&
        Near(kSearchPadding, 0.50F) && Near(kSightOvershoot, 0.03F);
}

[[nodiscard]] bool TestMechanismPolicy() {
    using namespace penumbra_vr::runtime::vr_mechanism_policy;
    const auto slider = PlanSlider({0.5F, 1.0F, 0.0F}, {2.0F, 0.0F, 0.0F});
    if (!slider.valid || !Near(slider.linear_velocity[0], 3.5F) ||
        !Near(slider.linear_velocity[1], 0.0F) ||
        !Near(slider.linear_velocity[2], 0.0F) ||
        slider.angular_velocity != Vec3{}) {
        return false;
    }

    const auto drag = PlanUnconstrainedJointDrag({0.0F, 0.0F, -1.0F});
    if (!drag.valid || !Near(drag.linear_velocity[2], -3.5F)) return false;

    // Y-axis hinge, handle one metre along X, hand pulling along -Z. The
    // resulting angular velocity must stay on the hinge pin and the body's
    // linear velocity must be the corresponding tangent.
    const auto hinge = PlanHinge(
        {0.0F, 0.0F, -0.1F}, {0.0F, 2.0F, 0.0F}, {0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 1.35F);
    if (!hinge.valid || !Near(hinge.hinge_radius, 1.0F) ||
        !Near(hinge.angular_velocity[0], 0.0F) ||
        !Near(hinge.angular_velocity[1], 1.35F) ||
        !Near(hinge.angular_velocity[2], 0.0F) ||
        !Near(hinge.linear_velocity[0], 0.0F) ||
        !Near(hinge.linear_velocity[2], -1.35F)) {
        return false;
    }

    const auto fallback_radius = PlanHinge(
        {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {}, {}, {}, 1.0F);
    if (!fallback_radius.valid || !Near(fallback_radius.hinge_radius, 0.30F)) {
        return false;
    }
    const auto invalid = PlanSlider({1.0F, 0.0F, 0.0F}, {});
    if (invalid.valid) return false;
    // Z rotation by +90 degrees maps local COM (.2,.4,0) to (-.4,.2,0).
    // A -0.5 rad/s turn requires COM velocity (.1,.2,0), while the
    // model origin at the pivot remains stationary. Translation is excluded.
    const std::array<float,16> pose{0,-1,0,7, 1,0,0,8, 0,0,1,9, 0,0,0,1};
    auto offset = PlanHinge({0,-0.02F,0},{0,0,1},{7,8,9},{7.4F,8,9},{7,8,9},1);
    if (!CorrectHingeMassCentreVelocity(pose,{0.2F,0.4F,0},offset) ||
        !Near(offset.linear_velocity[0],0.1F) ||
        !Near(offset.linear_velocity[1],0.2F) ||
        !Near(offset.linear_velocity[2],0)) return false;
    auto zero = PlanHinge({0,-0.02F,0},{0,0,-1},{7,8,9},{7.4F,8,9},{7,8,9},1);
    if (!CorrectHingeMassCentreVelocity(pose,{},zero) || zero.linear_velocity != Vec3{} ||
        !Near(zero.angular_velocity[2],-0.5F)) return false;
    auto bad = offset;
    return !CorrectHingeMassCentreVelocity(pose,{NAN,0,0},bad);
}

} // namespace

int main() {
    if (!TestMagneticPickupPolicy() || !TestMechanismPolicy()) return 1;
    std::cout << "Rework magnetic-pickup and mechanism policies passed\n";
    return 0;
}
