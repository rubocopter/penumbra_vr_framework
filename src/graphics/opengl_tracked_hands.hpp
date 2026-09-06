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
// Lightweight procedural gloves, not the original HPL/Rework skinned assets.
// Uses the world's depth buffer and preserves the caller's complete GL state.
[[nodiscard]] bool DrawTrackedHands(const std::array<TrackedHandVisual,2>& hands,
    const runtime::VrMatrix44& view, const runtime::VrMatrix44& projection,
    std::string& error) noexcept;
}
