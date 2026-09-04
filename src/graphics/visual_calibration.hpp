#pragma once

#include <array>

namespace penumbra_vr::graphics {

using LinearRgb = std::array<float, 3>;

// CPU reference for the accepted Overture Rework v4 calibration. The GPU
// implementations for each HPL integration must remain numerically aligned
// with these functions.
[[nodiscard]] LinearRgb ApplyEnhancedFinalTone(LinearRgb color) noexcept;
[[nodiscard]] float ApplyBoundedAmbientPreconditioning(float ambient) noexcept;
[[nodiscard]] LinearRgb RecoverDarkDiffuse(LinearRgb diffuse) noexcept;
[[nodiscard]] float BoundedSharpen(
    float center,
    const std::array<float, 4>& neighbours) noexcept;
[[nodiscard]] float GlowstickHaloEnvelope(float normalized_radius) noexcept;

} // namespace penumbra_vr::graphics
