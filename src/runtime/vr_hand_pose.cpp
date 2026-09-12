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

VrHandArticulation ArticulateVrHand(const std::array<float,5>& curls, bool left) noexcept {
    VrHandArticulation result;
    const float mirror=left ? -1.0F : 1.0F;
    constexpr std::array<float,5> open_spread{0,4,0,-3,-7};
    for (std::size_t finger=0;finger<curls.size();++finger) {
        const float curl=std::isfinite(curls[finger]) ? std::clamp(curls[finger],0.0F,1.0F) : 0;
        auto& pose=result.fingers[finger];
        if (finger==0) {
            // Thumb: metacarpal opposition plus two phalange joints; do not
            // treat it as a fourth identical three-phalange finger chain.
            pose.flexion_degrees={20*curl,45*curl,60*curl};
            result.thumb_yaw_degrees=mirror*(48-33*curl);
        } else {
            // Separate proximal/middle/distal flexion. Distal flexion follows
            // the middle joint progressively rather than folding in lockstep.
            pose.flexion_degrees={65*curl,85*curl,50*curl*curl};
            pose.spread_degrees=mirror*open_spread[finger]*(1-curl);
        }
    }
    return result;
}
}
