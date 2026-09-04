#include "spatial_audio.hpp"

#include <algorithm>

namespace penumbra_vr::audio {

std::string BuildOpenAlSoftConfig(HrtfMode mode) {
    const char* value = "auto";
    if (mode == HrtfMode::on) {
        value = "true";
    } else if (mode == HrtfMode::off) {
        value = "false";
    }
    return std::string("[general]\nhrtf = ") + value + "\n";
}

float CalculateOcclusionDistanceGainHf(
    float authored_block_volume,
    float current_block_fade,
    float listener_distance,
    float minimum_distance,
    float maximum_distance) noexcept {
    const float block_volume = std::clamp(authored_block_volume, 0.0F, 1.0F);
    const float block_fade = std::clamp(current_block_fade, 0.0F, 1.0F);
    const float combined_block = block_volume +
        block_fade * (1.0F - block_volume);
    const float occlusion_hf = 0.2F + combined_block * 0.8F;

    float range = 0.0F;
    const float span = maximum_distance - minimum_distance;
    if (span > 0.0F && listener_distance > minimum_distance) {
        range = std::clamp(
            (listener_distance - minimum_distance) / span, 0.0F, 1.0F);
    }
    const float distance_hf = 1.0F - range * 0.45F;
    return std::clamp(occlusion_hf * distance_hf, 0.0F, 1.0F);
}

} // namespace penumbra_vr::audio
