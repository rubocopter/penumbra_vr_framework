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

// Independent procedural articulation, not copied from a third-party rig.
// Thumb/index/middle/ring/little. Degrees are local flexion magnitudes;
// a mesh adapter must map them to that mesh's bind-pose axes, not Euler-copy.
struct VrFingerArticulation {
    std::array<float,3> flexion_degrees{};
    float spread_degrees=0;
};
struct VrHandArticulation {
    std::array<VrFingerArticulation,5> fingers{};
    float thumb_yaw_degrees=0;
};
// Articulation consumes already-conditioned curls. Limits are visual tuning
// values, not medically validated anatomical limits.
[[nodiscard]] VrHandArticulation ArticulateVrHand(const std::array<float,5>& curls, bool left) noexcept;
}
