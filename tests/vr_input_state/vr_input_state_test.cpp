#include "vr_input_state.hpp"
#include "vr_crouch_policy.hpp"

#include <cmath>
#include <iostream>

namespace {

using penumbra_vr::runtime::VrButtonState;
using penumbra_vr::runtime::VrCrouchMode;
using penumbra_vr::runtime::VrPhysicalCrouchPolicy;
using penumbra_vr::runtime::VrHand;
using penumbra_vr::runtime::VrInputContext;
using penumbra_vr::runtime::VrInputRouter;
using penumbra_vr::runtime::VrInputState;
using penumbra_vr::runtime::VrInputUpdateStatus;

[[nodiscard]] bool NearlyEqual(float left, float right) noexcept {
    return std::fabs(left - right) <= 0.00001F;
}

[[nodiscard]] VrButtonState PressedButton() noexcept {
    return penumbra_vr::runtime::MakeVrButtonState(true, true, true);
}

[[nodiscard]] bool TestDeadZone() {
    penumbra_vr::runtime::VrAnalogState inactive{false, 1.0F, -1.0F};
    penumbra_vr::runtime::ApplyStickDeadZone(inactive, 0.15F);
    if (!NearlyEqual(inactive.x, 0.0F) ||
        !NearlyEqual(inactive.y, 0.0F)) {
        return false;
    }

    penumbra_vr::runtime::VrAnalogState inside{true, 0.1F, 0.0F};
    penumbra_vr::runtime::ApplyStickDeadZone(inside, 0.15F);
    if (!NearlyEqual(inside.x, 0.0F) || !NearlyEqual(inside.y, 0.0F)) {
        return false;
    }

    penumbra_vr::runtime::VrAnalogState halfway{true, 0.575F, 0.0F};
    penumbra_vr::runtime::ApplyStickDeadZone(halfway, 0.15F);
    if (!NearlyEqual(halfway.x, 0.5F) || !NearlyEqual(halfway.y, 0.0F)) {
        return false;
    }

    penumbra_vr::runtime::VrAnalogState diagonal{true, 1.0F, 1.0F};
    penumbra_vr::runtime::ApplyStickDeadZone(diagonal, 0.15F);
    const float magnitude = std::sqrt(
        diagonal.x * diagonal.x + diagonal.y * diagonal.y);
    return NearlyEqual(magnitude, 1.0F) &&
        NearlyEqual(penumbra_vr::runtime::ClampStickDeadZone(-1.0F), 0.0F) &&
        NearlyEqual(penumbra_vr::runtime::ClampStickDeadZone(2.0F), 0.9F);
}

[[nodiscard]] bool TestContextAndHandednessLatches() {
    VrInputRouter router;
    VrInputState first;
    first.sprint = PressedButton();
    first.recenter = PressedButton();
    auto result = router.Update(
        first, VrInputContext::gameplay, VrHand::right, true, true, 100);
    if (result.status != VrInputUpdateStatus::actions_active ||
        !result.state.sprint.pressed || result.state.sprint.just_pressed ||
        !result.state.recenter.just_pressed) {
        return false;
    }

    VrInputState steady;
    steady.inventory = PressedButton();
    result = router.Update(
        steady, VrInputContext::gameplay, VrHand::right, true, true, 110);
    if (!result.state.inventory.just_pressed) {
        return false;
    }

    VrInputState mirrored;
    mirrored.quick_light = PressedButton();
    mirrored.recenter = PressedButton();
    result = router.Update(
        mirrored, VrInputContext::gameplay, VrHand::left, true, true, 120);
    return result.status == VrInputUpdateStatus::actions_active &&
        result.state.quick_light.pressed &&
        !result.state.quick_light.just_pressed &&
        result.state.recenter.pressed && !result.state.recenter.just_pressed;
}

[[nodiscard]] bool TestLostInteractionPoseReleasesCleanly() {
    VrInputRouter router;
    router.SetInteractSourceHand(VrHand::right);

    VrInputState held;
    held.interact = PressedButton();
    auto result = router.Update(
        held, VrInputContext::gameplay, VrHand::right, true, true, 100);
    if (!result.state.interact.pressed) {
        return false;
    }

    held.interact.just_pressed = false;
    result = router.Update(
        held, VrInputContext::gameplay, VrHand::right, true, false, 110);
    return !result.state.interact.pressed &&
        !result.state.interact.just_pressed &&
        result.state.interact.just_released;
}

[[nodiscard]] bool TestLostUiPoseReleasesPointerOnly() {
    VrInputRouter router;
    VrInputState held;
    held.ui_select = PressedButton();
    held.ui_drag = PressedButton();
    held.ui_back = PressedButton();
    auto result = router.Update(
        held, VrInputContext::ui, VrHand::right, true, true, 100);
    if (!result.state.ui_select.pressed || !result.state.ui_drag.pressed) {
        return false;
    }

    held.ui_select.just_pressed = false;
    held.ui_drag.just_pressed = false;
    held.ui_back.just_pressed = false;
    result = router.Update(
        held, VrInputContext::ui, VrHand::right, true, false, 110);
    if (!result.state.ui_select.pressed || !result.state.ui_drag.pressed ||
        !result.state.ui_back.pressed) {
        return false;
    }

    result = router.Update(
        held, VrInputContext::ui, VrHand::right, false, false, 120);
    return !result.state.ui_select.pressed &&
        result.state.ui_select.just_released &&
        !result.state.ui_drag.pressed && result.state.ui_drag.just_released &&
        result.state.ui_back.pressed;
}

[[nodiscard]] bool TestActionIdleGrace() {
    VrInputRouter router;
    VrInputState active;
    active.move = {true, 0.6F, 0.0F};
    active.sprint = PressedButton();
    auto result = router.Update(
        active, VrInputContext::gameplay, VrHand::right, true, true, 1000);
    if (result.status != VrInputUpdateStatus::actions_active ||
        !router.using_actions()) {
        return false;
    }

    result = router.Update(
        {}, VrInputContext::gameplay, VrHand::right, true, true, 1100);
    if (result.status != VrInputUpdateStatus::action_idle_grace ||
        !result.state.sprint.pressed || result.state.sprint.just_pressed) {
        return false;
    }

    result = router.Update(
        {}, VrInputContext::gameplay, VrHand::right, true, true, 1599);
    if (result.status != VrInputUpdateStatus::action_idle_grace) {
        return false;
    }

    // A reset or wrap in the supplied clock must not underflow into fallback.
    result = router.Update(
        {}, VrInputContext::gameplay, VrHand::right, true, true, 1000);
    if (result.status != VrInputUpdateStatus::action_idle_grace) {
        return false;
    }

    result = router.Update(
        {}, VrInputContext::gameplay, VrHand::right, true, true, 1600);
    return result.status == VrInputUpdateStatus::fallback_required &&
        !router.using_actions() && !result.state.sprint.pressed;
}

[[nodiscard]] bool TestPhysicalCrouchPolicy() {
    VrPhysicalCrouchPolicy policy;
    VrButtonState button;

    auto state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.70F);
    auto status = policy.status();
    if (state.pressed || state.just_pressed || !status.standing_height_known ||
        !NearlyEqual(status.standing_height, 1.70F) ||
        !NearlyEqual(status.enter_height, 1.45F) ||
        !NearlyEqual(status.exit_height, 1.53F)) {
        return false;
    }

    // Rework lets an initially low calibration settle upward, never downward.
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.75F);
    status = policy.status();
    if (!NearlyEqual(status.standing_height, 1.75F)) return false;
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.65F);
    if (!NearlyEqual(policy.status().standing_height, 1.75F)) return false;

    // Enter at standing-depth, retain through the 8 cm hysteresis band, leave
    // only when the exit height is reached.
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.50F);
    if (!state.pressed || !state.just_pressed || state.just_released) return false;
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.56F);
    if (!state.pressed || state.just_pressed || state.just_released) return false;
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.58F);
    if (state.pressed || state.just_pressed || !state.just_released) return false;

    // Invalid tracking cannot invent a calibration or transition.
    policy.Reset();
    state = policy.Update(
        button, VrCrouchMode::physical, 0.25F, true, true, 0.70F);
    if (policy.status().standing_height_known || state.pressed) return false;
    state = policy.Update(
        button, VrCrouchMode::physical, 0.25F, true, false, 1.70F);
    if (policy.status().standing_height_known || state.pressed) return false;

    // Hybrid composition must not release while either latched source still
    // requests crouch, and must not generate a second edge for the other source.
    policy.Reset();
    static_cast<void>(policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.70F));
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.45F);
    if (!state.just_pressed) return false;
    button = penumbra_vr::runtime::MakeVrButtonState(true, true, true);
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.45F);
    if (!state.pressed || state.just_pressed || state.just_released) return false;
    button = penumbra_vr::runtime::MakeVrButtonState(true, true, false);
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.60F);
    if (!state.pressed || state.just_released) return false;
    button = penumbra_vr::runtime::MakeVrButtonState(true, false, true);
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.60F);
    if (!state.pressed || state.just_released) return false;
    button = penumbra_vr::runtime::MakeVrButtonState(true, true, true);
    state = policy.Update(
        button, VrCrouchMode::hybrid, 0.25F, true, true, 1.60F);
    if (state.pressed || !state.just_released) return false;

    // A rejected native stand keeps logical ownership and freezes the standing
    // baseline until the backend reports that standing is possible again.
    policy.Reset();
    button = {};
    static_cast<void>(policy.Update(
        button, VrCrouchMode::physical, 0.25F, true, true, 1.70F));
    state = policy.Update(
        button, VrCrouchMode::physical, 0.25F, true, true, 1.40F);
    if (!state.pressed || !state.just_pressed) return false;
    state = policy.Update(
        button, VrCrouchMode::physical, 0.25F, true, true, 1.90F, true);
    status = policy.status();
    if (!state.pressed || state.just_released || status.desired_crouch ||
        !status.stand_blocked || !status.stand_release_pending ||
        !NearlyEqual(status.standing_height, 1.70F)) {
        return false;
    }
    state = policy.Update(
        button, VrCrouchMode::physical, 0.25F, true, true, 1.90F, false);
    if (state.pressed || !state.just_released ||
        policy.status().stand_release_pending) {
        return false;
    }

    // Rework button-only crouch toggles on press and ignores release/held state.
    policy.Reset();
    button = penumbra_vr::runtime::MakeVrButtonState(true, true, true);
    state = policy.Update(
        button, VrCrouchMode::button, 0.25F, true, true, 1.20F);
    if (!state.pressed || !state.just_pressed ||
        !policy.status().button_latched || policy.status().physical_crouch) {
        return false;
    }
    button = penumbra_vr::runtime::MakeVrButtonState(true, false, true);
    state = policy.Update(
        button, VrCrouchMode::button, 0.25F, true, true, 1.70F);
    if (!state.pressed || state.just_released) return false;
    button = penumbra_vr::runtime::MakeVrButtonState(true, true, true);
    state = policy.Update(
        button, VrCrouchMode::button, 0.25F, true, true, 1.70F);
    return !state.pressed && state.just_released &&
        !policy.status().button_latched;
}

} // namespace

int main() {
    if (!TestDeadZone()) {
        std::cerr << "VR stick dead-zone behavior failed\n";
        return 1;
    }
    if (!TestContextAndHandednessLatches()) {
        std::cerr << "VR context or handedness edge latching failed\n";
        return 2;
    }
    if (!TestLostInteractionPoseReleasesCleanly()) {
        std::cerr << "VR interaction did not release after pose loss\n";
        return 3;
    }
    if (!TestLostUiPoseReleasesPointerOnly()) {
        std::cerr << "VR UI pose-loss routing failed\n";
        return 4;
    }
    if (!TestActionIdleGrace()) {
        std::cerr << "VR action idle grace or fallback timing failed\n";
        return 5;
    }
    if (!TestPhysicalCrouchPolicy()) {
        std::cerr << "VR physical crouch policy failed\n";
        return 6;
    }
    if (penumbra_vr::runtime::OppositeHand(VrHand::left) != VrHand::right ||
        penumbra_vr::runtime::OppositeHand(VrHand::right) != VrHand::left) {
        std::cerr << "VR opposite-hand mapping failed\n";
        return 7;
    }

    std::cout << "VR logical input routing passed\n";
    return 0;
}
