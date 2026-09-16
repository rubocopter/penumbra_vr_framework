#pragma once
#include "vr_math.hpp"
#include <array>
#include <string>
namespace penumbra_vr::graphics {
struct TrackedHandVisual {
    runtime::VrMatrix44 palm;
    runtime::VrMatrix44 aim;
    std::array<float,5> curl{};
    bool visible = false;
    bool ray = false;
};
// Uses the proven Rework hand mesh/rig with Framework-owned five-finger
// articulation. The world's depth buffer remains authoritative and the caller's
// complete GL state is preserved.
[[nodiscard]] bool DrawTrackedHands(const std::array<TrackedHandVisual,2>& hands,
    const runtime::VrMatrix44& view, const runtime::VrMatrix44& projection,
    std::string& error) noexcept;
}
