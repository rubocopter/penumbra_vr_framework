#pragma once
#include <array>

namespace penumbra_vr::runtime {
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
// Instantaneous input response: no temporal lag, grip override or extra deadzone.
// Limits are visual tuning values, not medically validated anatomical limits.
[[nodiscard]] VrHandArticulation ArticulateVrHand(const std::array<float,5>& curls, bool left) noexcept;
}
