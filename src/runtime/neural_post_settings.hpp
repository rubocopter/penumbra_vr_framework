#pragma once

// Kept independent of VrSettings and C++17 so source products can consume the
// same snapshot without depending on Framework's storage/editor interfaces.
namespace penumbra_vr {
namespace runtime {

struct NeuralPostSettings {
    bool enabled = false;
    int internal_height = 896;
    int scale_percent = 60;
    int intensity_percent = 90;
    int passes = 1;
    int style = 2;
    int fovea_percent = 100;
    bool outer_enabled = false;
    int outer_scale_percent = 25;
};

inline void NormalizeNeuralPostSettings(NeuralPostSettings& settings) noexcept {
    const auto clamp = [](int value, int minimum, int maximum) {
        return value < minimum ? minimum : (value > maximum ? maximum : value);
    };
    settings.internal_height = clamp(settings.internal_height, 64, 1280);
    settings.scale_percent = clamp(settings.scale_percent, 25, 100);
    settings.intensity_percent = clamp(settings.intensity_percent, 0, 100);
    settings.passes = clamp(settings.passes, 1, 4);
    settings.style = clamp(settings.style, 0, 2);
    settings.fovea_percent = clamp(settings.fovea_percent, 20, 100);
    settings.outer_scale_percent = clamp(settings.outer_scale_percent, 10, 100);
}

inline bool operator==(
    const NeuralPostSettings& left, const NeuralPostSettings& right) noexcept {
    return left.enabled == right.enabled &&
        left.internal_height == right.internal_height &&
        left.scale_percent == right.scale_percent &&
        left.intensity_percent == right.intensity_percent &&
        left.passes == right.passes && left.style == right.style &&
        left.fovea_percent == right.fovea_percent &&
        left.outer_enabled == right.outer_enabled &&
        left.outer_scale_percent == right.outer_scale_percent;
}

inline bool operator!=(
    const NeuralPostSettings& left, const NeuralPostSettings& right) noexcept {
    return !(left == right);
}

} // namespace runtime
} // namespace penumbra_vr
