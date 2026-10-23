#pragma once

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {

struct VrStepClimbAdvance {
    float distance{};
    bool climbing{};
};

// Rework accepts a positive measured surface height rather than advancing past
// it. Binary HPL hosts retain their native climb rate and single physics update;
// clamp only that accepted ascent to the remaining ray-measured rise. Floor and
// downward hits must not produce upward motion or suppress native gravity.
[[nodiscard]] inline VrStepClimbAdvance ResolveVrStepClimbAdvance(
    float native_advance, float surface_height) noexcept {
    if (!std::isfinite(native_advance) || native_advance <= 0.0F ||
        !std::isfinite(surface_height) || surface_height <= 0.0F) return {};
    return {std::min(native_advance, surface_height), true};
}

} // namespace penumbra_vr::runtime
