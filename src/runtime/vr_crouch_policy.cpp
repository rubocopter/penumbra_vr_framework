#include "vr_crouch_policy.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {

[[nodiscard]] float NormalizePhysicalCrouchDepth(float depth) noexcept {
    if (!std::isfinite(depth)) {
        return vr_setting_limits::kPhysicalCrouchDepth.default_value;
    }
    return std::clamp(
        depth,
        vr_setting_limits::kPhysicalCrouchDepth.minimum,
        vr_setting_limits::kPhysicalCrouchDepth.maximum);
}

[[nodiscard]] bool PlausibleHeadHeight(float height) noexcept {
    return std::isfinite(height) &&
        height > vr_crouch_policy::kMinimumPlausibleHeadHeight &&
        height < vr_crouch_policy::kMaximumPlausibleHeadHeight;
}

} // namespace

VrButtonState VrPhysicalCrouchPolicy::Update(
    const VrButtonState& button,
    VrCrouchMode mode,
    float physical_crouch_depth,
    bool gameplay_active,
    bool tracking_valid,
    float head_height) noexcept {
    const bool physical_enabled = mode != VrCrouchMode::button;
    const bool button_enabled = mode != VrCrouchMode::physical;
    bool physical_changed = false;

    if (button_enabled && gameplay_active && button.just_pressed) {
        // Rework owns crouch as a toggle at the VR-policy boundary. Feeding a
        // held/released action into a game's legacy toggle option can invert
        // the requested stance on the following physical transition.
        button_latched_ = !button_latched_;
    } else if (!button_enabled) {
        button_latched_ = false;
    }

    if (!physical_enabled) {
        physical_changed = physical_crouch_;
        physical_crouch_ = false;
    } else if (gameplay_active && tracking_valid && PlausibleHeadHeight(head_height)) {
        if (!standing_height_known_) {
            standing_height_known_ = true;
            standing_height_ = head_height;
        } else if (!physical_crouch_ && !button_latched_ &&
                   head_height > standing_height_) {
            // Match Rework: let an initially low standing sample settle upward,
            // but never let a ducking motion drag the standing baseline down.
            standing_height_ = head_height;
        }

        const float depth = NormalizePhysicalCrouchDepth(physical_crouch_depth);
        const float enter_height = standing_height_ - depth;
        const float exit_height = enter_height + vr_crouch_policy::kExitHysteresis;
        if (!physical_crouch_ && head_height <= enter_height) {
            physical_crouch_ = true;
            physical_changed = true;
        } else if (physical_crouch_ && head_height >= exit_height) {
            physical_crouch_ = false;
            physical_changed = true;
        }
    }

    if (physical_changed) {
        if (physical_crouch_) ++physical_entries_;
        else ++physical_exits_;
    }

    const bool effective_crouch = physical_crouch_ || button_latched_;

    VrButtonState result;
    result.active = physical_enabled || (button_enabled && button.active);
    result.pressed = effective_crouch;
    result.just_pressed = !effective_crouch_ && effective_crouch;
    result.just_released = effective_crouch_ && !effective_crouch;
    effective_crouch_ = effective_crouch;

    const float depth = NormalizePhysicalCrouchDepth(physical_crouch_depth);
    status_ = {physical_enabled, tracking_valid, head_height,
        standing_height_known_, standing_height_,
        standing_height_known_ ? standing_height_ - depth : 0.0F,
        standing_height_known_ ? standing_height_ - depth +
            vr_crouch_policy::kExitHysteresis : 0.0F,
        physical_crouch_, button_latched_, effective_crouch_,
        physical_entries_, physical_exits_};
    return result;
}

void VrPhysicalCrouchPolicy::Reset() noexcept {
    standing_height_known_ = false;
    standing_height_ = 0.0F;
    physical_crouch_ = false;
    button_latched_ = false;
    effective_crouch_ = false;
    physical_entries_ = 0;
    physical_exits_ = 0;
    status_ = {};
}

const VrPhysicalCrouchStatus& VrPhysicalCrouchPolicy::status() const noexcept {
    return status_;
}

} // namespace penumbra_vr::runtime
