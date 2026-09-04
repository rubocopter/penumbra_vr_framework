#pragma once

#include <string>

namespace penumbra_vr::audio {

enum class HrtfMode {
    automatic,
    on,
    off,
};

struct ReverbProfile {
    float density = 0.70F;
    float diffusion = 0.90F;
    float gain = 0.28F;
    float gain_hf = 0.77F;
    float gain_lf = 1.0F;
    float decay_time = 2.6F;
    float decay_hf_ratio = 0.55F;
    float decay_lf_ratio = 1.0F;
    float reflections_gain = 0.15F;
    float reflections_delay = 0.016F;
    float late_reverb_gain = 0.85F;
    float late_reverb_delay = 0.030F;
    float air_absorption_gain_hf = 0.894F;
    float hf_reference = 5000.0F;
    float lf_reference = 250.0F;
    float bus_gain = 0.32F;
};

inline constexpr ReverbProfile kMineGalleryReverb{};

[[nodiscard]] std::string BuildOpenAlSoftConfig(HrtfMode mode);

// Returns the OpenAL low-pass high-frequency gain used by the accepted
// Rework audio pass. Inputs are normalized defensively at the shared boundary.
[[nodiscard]] float CalculateOcclusionDistanceGainHf(
    float authored_block_volume,
    float current_block_fade,
    float listener_distance,
    float minimum_distance,
    float maximum_distance) noexcept;

} // namespace penumbra_vr::audio
