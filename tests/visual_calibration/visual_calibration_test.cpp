#include "visual_calibration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <random>

namespace {

[[nodiscard]] bool Near(float left, float right, float tolerance = 1.0e-6F) {
    return std::fabs(left - right) <= tolerance;
}

} // namespace

int main() {
    using namespace penumbra_vr::graphics;

    const LinearRgb black = ApplyEnhancedFinalTone({0.0F, 0.0F, 0.0F});
    if (!Near(black[0], 0.0F) || !Near(black[1], 0.0F) ||
        !Near(black[2], 0.0F)) {
        std::cerr << "The accepted tone curve no longer preserves black\n";
        return 1;
    }

    float previous = 0.0F;
    for (int index = 0; index <= 1024; ++index) {
        const float exponent = -6.0F + static_cast<float>(index) *
            (6.0F + std::log10(65504.0F)) / 1024.0F;
        const float level = std::pow(10.0F, exponent);
        const LinearRgb mapped = ApplyEnhancedFinalTone({level, level, level});
        if (!std::isfinite(mapped[0]) || mapped[0] < previous - 1.0e-5F ||
            mapped[0] < 0.0F || mapped[0] > 1.0F ||
            !Near(mapped[0], mapped[1]) || !Near(mapped[1], mapped[2])) {
            std::cerr << "The neutral tone curve is not finite and monotonic\n";
            return 2;
        }
        previous = mapped[0];
    }

    for (int index = 0; index <= 100; ++index) {
        const float sample = static_cast<float>(index) / 100.0F;
        const LinearRgb recovered = RecoverDarkDiffuse({sample, sample, sample});
        if (recovered[0] < sample ||
            (sample >= 0.28F && !Near(recovered[0], sample))) {
            std::cerr << "Dark-diffuse recovery escaped its intended range\n";
            return 3;
        }
        const float old_ambient = sample * 0.38F +
            std::min(sample * 3.0F, 0.028F);
        const float new_ambient = ApplyBoundedAmbientPreconditioning(sample);
        if (new_ambient < old_ambient || new_ambient - old_ambient > 0.003001F) {
            std::cerr << "Ambient v4 lift exceeds its +0.003 pre-tone bound\n";
            return 4;
        }
    }

    for (const float uniform : {0.0F, 0.01F, 0.1F, 0.5F, 1.0F, 8.0F}) {
        if (!Near(BoundedSharpen(uniform, {uniform, uniform, uniform, uniform}), uniform)) {
            std::cerr << "Sharpening changed a uniform region\n";
            return 5;
        }
    }
    if (!Near(BoundedSharpen(0.0F, {1.0F, 0.0F, 0.0F, 0.0F}), 0.0F)) {
        std::cerr << "Sharpening introduced a halo around black\n";
        return 6;
    }

    std::mt19937 generator(1942);
    std::uniform_real_distribution<float> distribution(0.0F, 8.0F);
    for (int iteration = 0; iteration < 2048; ++iteration) {
        const float center = distribution(generator);
        const std::array<float, 4> neighbours{
            distribution(generator), distribution(generator),
            distribution(generator), distribution(generator),
        };
        const float sharpened = BoundedSharpen(center, neighbours);
        float minimum = center;
        float maximum = center;
        for (const float value : neighbours) {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
        if (sharpened < minimum || sharpened > maximum) {
            std::cerr << "Sharpening created a new local extremum\n";
            return 7;
        }
    }

    float last_halo = 1.0F;
    for (int index = 0; index <= 100; ++index) {
        const float radius = static_cast<float>(index) / 100.0F;
        const float halo = GlowstickHaloEnvelope(radius);
        if (halo < 0.0F || halo > last_halo + 1.0e-6F) {
            std::cerr << "Glowstick halo is not radially decreasing\n";
            return 8;
        }
        last_halo = halo;
    }
    if (!Near(last_halo, 0.0F) || !Near(GlowstickHaloEnvelope(2.0F), 0.0F)) {
        std::cerr << "Glowstick halo does not reach zero at its boundary\n";
        return 9;
    }

    std::cout << "Overture Rework v4 visual calibration references passed\n";
    return 0;
}
