#include "vr_hand_pose.hpp"
#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {
float UnitInput(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : 0.0F;
}
}

float RemapVrFingerCurl(float value, float lead, float full) noexcept {
    value = UnitInput(value);
    lead = UnitInput(lead);
    full = UnitInput(full);
    const float range = full - lead;
    float t = range > 0.0001F ? (value - lead) / range : value;
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

float ApplyVrFingerCurlDeadzone(float value) noexcept {
    constexpr float kDeadzone = 0.08F;
    value = UnitInput(value);
    if (value <= kDeadzone) return 0.0F;
    return (value - kDeadzone) / (1.0F - kDeadzone);
}

std::array<float,5> BuildVrHandCurlTargets(const VrHandCurlInput& input) noexcept {
    std::array<float,5> target{};
    if (!input.valid) return target;

    const float grip = UnitInput(input.grip);
    const float trigger = UnitInput(input.trigger);
    const float thumb_from_grip = RemapVrFingerCurl(grip, 0.15F, 0.98F);
    if (input.skeletal) {
        for (std::size_t finger = 0; finger < target.size(); ++finger)
            target[finger] = ApplyVrFingerCurlDeadzone(input.finger_curl[finger]);
        target[0] = std::max(target[0], thumb_from_grip);
        return target;
    }

    target[0] = thumb_from_grip;
    target[1] = RemapVrFingerCurl(trigger, 0.00F, 0.85F);
    target[2] = RemapVrFingerCurl(grip, 0.10F, 0.95F);
    target[3] = RemapVrFingerCurl(grip, 0.05F, 0.88F);
    target[4] = RemapVrFingerCurl(grip, 0.00F, 0.80F);
    return target;
}

float VrHandCurlSmoothingBlend(float dt) noexcept {
    if (!std::isfinite(dt)) return 0.0F;
    dt = std::clamp(dt, 0.0F, 0.1F);
    return 1.0F - std::exp(-dt / 0.07F);
}

void SmoothVrHandCurls(std::array<float,5>& current,
    const std::array<float,5>& target, float dt) noexcept {
    const float blend = VrHandCurlSmoothingBlend(dt);
    for (std::size_t finger = 0; finger < current.size(); ++finger) {
        const float from = UnitInput(current[finger]);
        const float to = UnitInput(target[finger]);
        current[finger] = from + (to - from) * blend;
    }
}

VrHandArticulation ArticulateVrHand(
    const std::array<float,5>& curls, bool /*left*/) noexcept {
    VrHandArticulation result;
    for (std::size_t finger=0;finger<curls.size();++finger) {
        const float curl=std::isfinite(curls[finger]) ?
            std::clamp(curls[finger],0.0F,1.0F) : 0.0F;
        auto& pose=result.fingers[finger];
        if (finger==0) {
            // Rework 23c890f authored the imported rigid-skin thumb around its
            // diagonal bone axis at 22/28/16 degrees. Preserve Framework's
            // independent thumb curl while keeping that proven geometry.
            pose.flexion_degrees={22.0F*curl,28.0F*curl,16.0F*curl};
        } else if (finger==1) {
            // Rework's trigger/index pose. Higher procedural ranges fold this
            // old skin over itself and were visible as contorted fingers.
            pose.flexion_degrees={48.0F*curl,62.0F*curl,36.0F*curl};
        } else {
            // Middle, ring and little share the proven pure-flexion axis and
            // 52/66/38 degree grab range. Five skeletal channels remain
            // independent; only the imported mesh's safe range is shared.
            pose.flexion_degrees={52.0F*curl,66.0F*curl,38.0F*curl};
        }
    }
    return result;
}
}
