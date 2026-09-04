#include "spatial_audio.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool Near(float left, float right, float tolerance = 1.0e-6F) {
    return std::fabs(left - right) <= tolerance;
}

} // namespace

int main() {
    using namespace penumbra_vr::audio;

    if (BuildOpenAlSoftConfig(HrtfMode::automatic) !=
            "[general]\nhrtf = auto\n" ||
        BuildOpenAlSoftConfig(HrtfMode::on) !=
            "[general]\nhrtf = true\n" ||
        BuildOpenAlSoftConfig(HrtfMode::off) !=
            "[general]\nhrtf = false\n") {
        std::cerr << "OpenAL Soft HRTF configuration drifted from the Rework format\n";
        return 1;
    }

    if (!Near(CalculateOcclusionDistanceGainHf(1.0F, 1.0F, 0.0F, 1.0F, 11.0F), 1.0F) ||
        !Near(CalculateOcclusionDistanceGainHf(0.0F, 0.0F, 0.0F, 1.0F, 11.0F), 0.2F) ||
        !Near(CalculateOcclusionDistanceGainHf(1.0F, 1.0F, 11.0F, 1.0F, 11.0F), 0.55F) ||
        !Near(CalculateOcclusionDistanceGainHf(0.0F, 0.0F, 11.0F, 1.0F, 11.0F), 0.11F)) {
        std::cerr << "Occlusion/distance low-pass reference values are incorrect\n";
        return 2;
    }

    float previous = 1.0F;
    for (int step = 0; step <= 100; ++step) {
        const float distance = static_cast<float>(step) / 10.0F + 1.0F;
        const float gain = CalculateOcclusionDistanceGainHf(
            1.0F, 1.0F, distance, 1.0F, 11.0F);
        if (gain > previous + 1.0e-6F || gain < 0.0F || gain > 1.0F) {
            std::cerr << "Distance absorption is not monotonic and bounded\n";
            return 3;
        }
        previous = gain;
    }

    if (!Near(kMineGalleryReverb.decay_time, 2.6F) ||
        !Near(kMineGalleryReverb.bus_gain, 0.32F) ||
        !Near(kMineGalleryReverb.air_absorption_gain_hf, 0.894F)) {
        std::cerr << "Mine-gallery EFX preset drifted from the accepted Rework values\n";
        return 4;
    }

    std::cout << "Shared HRTF, occlusion and environmental-audio references passed\n";
    return 0;
}
