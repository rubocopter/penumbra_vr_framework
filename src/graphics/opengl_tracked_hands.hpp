#pragma once
#include "vr_math.hpp"
#include <array>
#include <string>
namespace penumbra_vr::graphics {
struct TrackedHandVisual {
    runtime::VrMatrix44 palm;
    runtime::VrMatrix44 aim;
    std::array<float,5> curl{};
    float hold_pose_weight = 0.0F;
    bool visible = false;
    bool ray = false;
    bool colored_ray = false;
    bool ray_usable = false;
    std::array<float,3> ray_from{};
    std::array<float,3> ray_to{};
};
// Uses the proven Rework hand mesh/rig with Framework-owned five-finger
// articulation. The world's depth buffer remains authoritative and the caller's
// complete GL state is preserved.
[[nodiscard]] bool DrawTrackedHands(const std::array<TrackedHandVisual,2>& hands,
    const runtime::VrMatrix44& view, const runtime::VrMatrix44& projection,
    std::string& error) noexcept;
}
