#pragma once
#include <array>

namespace penumbra_vr::runtime {
// Shared controller-input convention: Thumb, Index, Middle, Ring, Little.
// Grip/trigger are normalized 0..1 values. Backends that do not expose those
// analog values should only use the skeletal path until they have real input.
struct VrHandCurlInput {
    bool valid = false;
    bool skeletal = false;
    std::array<float,5> finger_curl{};
    float grip = 0;
    float trigger = 0;
};

// Rework-proven conditioning policy. These helpers are SDK/rig neutral: they
// produce normalized curls only; mesh bind poses and bone axes stay backend
// or product owned.
[[nodiscard]] float RemapVrFingerCurl(float value, float lead, float full) noexcept;
[[nodiscard]] float ApplyVrFingerCurlDeadzone(float value) noexcept;
[[nodiscard]] std::array<float,5> BuildVrHandCurlTargets(const VrHandCurlInput& input) noexcept;
[[nodiscard]] float VrHandCurlSmoothingBlend(float dt) noexcept;
void SmoothVrHandCurls(std::array<float,5>& current,
    const std::array<float,5>& target, float dt) noexcept;

// Framework preserves five independent finger channels. Black Plague uses its
// richer free-hand curl amplitudes while held tools retain Rework's authored
// pose. Mesh adapters own bind-pose axes and any rig-specific spread/opposition.
struct VrFingerArticulation {
    std::array<float,3> flexion_degrees{};
    float spread_degrees=0;
};
struct VrHandArticulation {
    std::array<VrFingerArticulation,5> fingers{};
    float thumb_yaw_degrees=0;
};
[[nodiscard]] VrHandArticulation ArticulateVrHand(
    const std::array<float,5>& curls, bool left,
    float hold_pose_weight = 0.0F) noexcept;
}
