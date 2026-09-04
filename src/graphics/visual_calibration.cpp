#include "visual_calibration.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::graphics {
namespace {

[[nodiscard]] float Saturate(float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

LinearRgb ApplyEnhancedFinalTone(LinearRgb color) noexcept {
    for (float& channel : color) {
        const float exposed = std::max(channel, 0.0F) * 1.25F;
        channel = (exposed * (2.51F * exposed + 0.03F)) /
            (exposed * (2.43F * exposed + 0.59F) + 0.14F);
    }

    const float luminance =
        color[0] * 0.2126F + color[1] * 0.7152F + color[2] * 0.0722F;
    for (float& channel : color) {
        const float saturated = luminance + (channel - luminance) * 1.12F;
        const float contrasted = (saturated - 0.5F) * 1.08F + 0.5F;
        channel = std::pow(Saturate(contrasted), 0.94F);
    }
    return color;
}

float ApplyBoundedAmbientPreconditioning(float ambient) noexcept {
    const float value = std::max(ambient, 0.0F);
    return value * 0.38F + std::min(value * 3.0F, 0.031F);
}

LinearRgb RecoverDarkDiffuse(LinearRgb diffuse) noexcept {
    const float luminance =
        diffuse[0] * 0.299F + diffuse[1] * 0.587F + diffuse[2] * 0.114F;
    const float recovery = (1.0F - Saturate(luminance / 0.28F)) * 0.06F;
    for (float& channel : diffuse) {
        channel += recovery;
    }
    return diffuse;
}

float BoundedSharpen(
    float center,
    const std::array<float, 4>& neighbours) noexcept {
    float neighbour_average = 0.0F;
    float local_minimum = center;
    float local_maximum = center;
    for (const float sample : neighbours) {
        neighbour_average += sample * 0.25F;
        local_minimum = std::min(local_minimum, sample);
        local_maximum = std::max(local_maximum, sample);
    }
    return std::max(
        std::clamp(
            center + (center - neighbour_average) * 0.65F,
            local_minimum,
            local_maximum),
        0.0F);
}

float GlowstickHaloEnvelope(float normalized_radius) noexcept {
    const float radius = std::max(normalized_radius, 0.0F);
    const float base = Saturate(1.0F - radius * radius);
    return base * base * base;
}

} // namespace penumbra_vr::graphics
