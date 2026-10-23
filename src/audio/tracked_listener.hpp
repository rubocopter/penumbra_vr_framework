#pragma once

#include "vr_math.hpp"

#include <array>

namespace penumbra_vr::audio {

struct TrackedListenerPose {
    std::array<float, 3> position{};
    std::array<float, 6> orientation{}; // OpenAL: forward, then up.
};

// Rework 23c890f Scene.cpp listener path, including LowLevelSoundOpenAL's
// final forward inversion. Input is the composed world head, never an eye.
[[nodiscard]] bool BuildTrackedListenerPose(
    const runtime::VrMatrix44& head_world, TrackedListenerPose& listener) noexcept;

} // namespace penumbra_vr::audio
