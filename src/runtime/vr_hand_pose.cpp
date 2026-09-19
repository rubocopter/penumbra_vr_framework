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
    const std::array<float,5>& curls, bool /*left*/, float hold_pose_weight) noexcept {
    VrHandArticulation result;
    const float hold = UnitInput(hold_pose_weight);
    for (std::size_t finger=0;finger<curls.size();++finger) {
        const float curl=std::max(UnitInput(curls[finger]), hold);
        auto& pose=result.fingers[finger];
        if (hold > 0.0F && finger==0) {
            // Rework 23c890f switches attached tools to its dedicated hold
            // rotations instead of merely driving the normal hand pose harder.
            pose.flexion_degrees={28.0F*curl,34.0F*curl,20.0F*curl};
        } else if (hold > 0.0F && finger==1) {
            pose.flexion_degrees={52.0F*curl,66.0F*curl,38.0F*curl};
        } else if (hold > 0.0F) {
            pose.flexion_degrees={55.0F*curl,70.0F*curl,40.0F*curl};
        } else if (finger==0) {
            // Black Plague's provisional hand was the stronger headset-tested
            // free-hand reference for curl amplitude. Keep its full three-joint
            // thumb motion, but let the imported Rework rig provide opposition
            // through its authored diagonal thumb axis. Adding the procedural
            // hand's separate yaw on this rigid skin visibly twists the web.
            pose.flexion_degrees={20.0F*curl,45.0F*curl,60.0F*curl};
        } else {
            // Preserve BP's richer independent finger response. Distal flexion
            // follows progressively so partial sensor curls do not fold every
            // phalanx in lockstep. Rework's rigidly weighted web cannot safely
            // consume the provisional hand's per-finger spread, especially on
            // the little finger, so spread remains zero for this mesh.
            pose.flexion_degrees={65.0F*curl,85.0F*curl,50.0F*curl*curl};
        }
    }
    return result;
}
}
