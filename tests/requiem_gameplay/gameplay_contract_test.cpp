#include "gameplay_contract.hpp"
#include "tool_socket_profile.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

int main() {
    std::string error;
    float tracked_height = 0.0F;
    using penumbra_vr::backends::requiem::ComposeRequiemTrackedHeadHeight;
    const auto near = [](float actual, float expected) {
        return std::fabs(actual - expected) < 0.0001F;
    };
    // The installed BP/Requiem HUD flashlight points down model -Y. Its
    // spotlight must face tracked hand -Z after the target-owned socket pose.
    const auto flashlight = penumbra_vr::backends::requiem::
        InstalledFlashlightToolPose(penumbra_vr::runtime::IdentityMatrix());
    const auto light_z = flashlight.values[9] * -0.103966F +
        flashlight.values[11];
    const auto ray_z = flashlight.values[9] * -0.203767F +
        flashlight.values[11];
    if (!near(flashlight.values[6], -1.0F) ||
        !near(flashlight.values[9], 1.0F) ||
        !(ray_z < light_z && light_z < 0.0F)) {
        std::cerr << "Requiem flashlight light nodes face away from the hand\n";
        return 1;
    }
    using penumbra_vr::backends::requiem::RequiemLocomotionDisplacement;
    using penumbra_vr::backends::requiem::RequiemPushAxisProjection;
    using penumbra_vr::backends::requiem::RequiemPushHandForce;
    const auto sideways_push = RequiemPushAxisProjection(
        {1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 0.0F});
    const auto pull = RequiemPushAxisProjection(
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 0.0F});
    const auto force = RequiemPushHandForce(
        {1.0F, 1.0F, 0.5F}, {0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 0.5F});
    if (!sideways_push.valid || !near(sideways_push.forward, 0.0F) ||
        !near(sideways_push.sideways, 1.0F) ||
        !pull.valid || !near(pull.forward, -1.0F) ||
        !near(pull.sideways, 0.0F) ||
        !near(force[0], 300.0F / std::sqrt(2.0F)) ||
        !near(force[1], 0.0F) || !near(force[2], 0.0F)) {
        std::cerr << "Requiem Push direction/hand force drifted\n";
        return 1;
    }
    const auto push_step = RequiemLocomotionDisplacement(
        {0.0F, 0.0F, -1.0F}, 0.1F, true, true);
    if (!near(push_step[2], -0.05F)) {
        std::cerr << "Requiem Push did not use constrained speed\n";
        return 1;
    }
    // Integrate one second of requested travel through the production helper.
    // 120 Hz keeps each request below the native queue's 5 cm step cap.
    for (const bool sprinting : {false, true}) {
        const float expected_distance = sprinting ? 4.5F : 1.5F;
        for (const std::array<float, 3> direction : {
                 std::array<float, 3>{0.0F, 0.0F, -1.0F},
                 std::array<float, 3>{1.0F, 0.0F, 0.0F},
                 std::array<float, 3>{-0.6F, 0.0F, 0.8F}}) {
            std::array<float, 3> travel{};
            for (int tick = 0; tick < 120; ++tick) {
                const auto step = RequiemLocomotionDisplacement(
                    direction, 1.0F / 120.0F, sprinting);
                for (std::size_t axis = 0; axis < travel.size(); ++axis)
                    travel[axis] += step[axis];
            }
            if (!near(travel[0], direction[0] * expected_distance) ||
                !near(travel[1], 0.0F) ||
                !near(travel[2], direction[2] * expected_distance)) {
                std::cerr << "Requiem " << (sprinting ? "sprint" : "walk")
                          << " one-second travel drifted: "
                          << std::hypot(travel[0], travel[2]) << " m\n";
                return 1;
            }
        }
    }
    // The native body position is the capsule centre. Standing (1.65 m)
    // and crouched (0.95 m) centres below describe the same 5.0 m floor.
    if (!ComposeRequiemTrackedHeadHeight(5.825F, 1.65F, 1.7F, 0.0F,
            false, false, tracked_height, error) ||
        !near(tracked_height, 6.5975F) ||
        !ComposeRequiemTrackedHeadHeight(5.475F, 0.95F, 1.3F, 0.0F,
            true, true, tracked_height, error) ||
        !near(tracked_height, 6.1715F) ||
        !ComposeRequiemTrackedHeadHeight(5.475F, 0.95F, 1.7F, 0.0F,
            true, false, tracked_height, error) ||
        !near(tracked_height, 6.3475F) ||
        !ComposeRequiemTrackedHeadHeight(6.325F, 1.65F, 1.7F, 0.0F,
            false, false, tracked_height, error) ||
        !near(tracked_height, 7.0975F)) {
        std::cerr << "Requiem body/HMD height or crouch posture drifted: "
                  << error << '\n';
        return 1;
    }
    if (!penumbra_vr::backends::requiem::
            RunRequiemGameplayContractHarness(error)) {
        std::cerr << "Requiem gameplay contract failed: " << error << '\n';
        return 1;
    }

    const auto& contract =
        penumbra_vr::backends::requiem::GameplayContract();
    constexpr std::array<std::uint8_t, 5> physical_owner_window{
        0xD9, 0x07, 0xD9, 0x46, 0x54};
    if (contract.physical_move_owner_window != physical_owner_window) {
        std::cerr << "Requiem physical movement owner window drifted\n";
        return 1;
    }

    using penumbra_vr::backends::requiem::AttributeRequiemPhysicalAcceptance;
    const std::array<float, 3> physical{0.04F, 0.0F, 0.0F};
    const std::array<float, 3> no_direct{};
    const std::array<float, 3> opposing_direct{-0.02F, 0.0F, 0.0F};
    if (!near(AttributeRequiemPhysicalAcceptance(
            physical, no_direct, physical)[0], 0.04F) ||
        !near(AttributeRequiemPhysicalAcceptance(
            physical, no_direct, no_direct)[0], 0.0F) ||
        !near(AttributeRequiemPhysicalAcceptance(
            physical, no_direct, {0.02F, 0.0F, 0.02F})[0], 0.02F) ||
        !near(AttributeRequiemPhysicalAcceptance(
            physical, opposing_direct, {0.02F, 0.0F, 0.0F})[0], 0.04F) ||
        !near(AttributeRequiemPhysicalAcceptance(
            physical, opposing_direct, no_direct)[0], 0.02F)) {
        std::cerr << "Requiem physical collision attribution drifted\n";
        return 1;
    }

    using penumbra_vr::backends::requiem::NativeCrouchTransition;
    using penumbra_vr::backends::requiem::PlanNativeCrouchTransition;
    if (PlanNativeCrouchTransition(false, 0, true, false) !=
            NativeCrouchTransition::none ||
        PlanNativeCrouchTransition(true, -1, true, false) !=
            NativeCrouchTransition::none ||
        PlanNativeCrouchTransition(true, 3, true, false) !=
            NativeCrouchTransition::none ||
        PlanNativeCrouchTransition(true, 0, true, false) !=
            NativeCrouchTransition::enter ||
        PlanNativeCrouchTransition(true, 4, false, false) !=
            NativeCrouchTransition::none ||
        PlanNativeCrouchTransition(true, 4, false, true) !=
            NativeCrouchTransition::stand) {
        std::cerr << "Requiem native crouch ownership plan drifted\n";
        return 1;
    }

    using penumbra_vr::backends::requiem::PlanDirectLocomotionPublication;
    using penumbra_vr::backends::requiem::TrackedHeadWorldPoseFresh;
    using penumbra_vr::backends::requiem::ShouldMarkDirectLocomotionAccepted;

    if (!TrackedHeadWorldPoseFresh(true, 1000, 1249) ||
        !TrackedHeadWorldPoseFresh(true, 1000, 1250) ||
        TrackedHeadWorldPoseFresh(true, 1000, 1251) ||
        TrackedHeadWorldPoseFresh(false, 1000, 1100) ||
        TrackedHeadWorldPoseFresh(true, 0, 100)) {
        std::cerr << "Requiem tracked head pose freshness contract drifted\n";
        return 1;
    }

    const auto native_priority = PlanDirectLocomotionPublication(
        true, 0.75F, -0.5F, true, true, true);
    if (native_priority.publish || native_priority.move_x != 0.0F ||
        native_priority.move_y != 0.0F) {
        std::cerr << "native movement did not retain priority\n";
        return 1;
    }

    const auto gated = PlanDirectLocomotionPublication(
        false, 0.75F, -0.5F, true, false, true);
    if (!gated.publish || gated.move_x != 0.75F || gated.move_y != 0.0F) {
        std::cerr << "Requiem movement gates were not applied per axis\n";
        return 1;
    }

    const auto stale_tracking = PlanDirectLocomotionPublication(
        false, 0.75F, -0.5F, true, true, false);
    if (stale_tracking.publish) {
        std::cerr << "stale head tracking was accepted for direct locomotion\n";
        return 1;
    }

    const auto accepted = PlanDirectLocomotionPublication(
        false, 0.75F, -0.5F, true, true, true);
    if (!accepted.publish || ShouldMarkDirectLocomotionAccepted(accepted, false) ||
        !ShouldMarkDirectLocomotionAccepted(accepted, true)) {
        std::cerr << "accepted-motion marking is not coupled to publication\n";
        return 1;
    }

    std::cout << "Requiem gameplay ABI/locomotion contract passed\n";
    return 0;
}
