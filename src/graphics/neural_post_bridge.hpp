#pragma once

#include "../runtime/vr_render_types.hpp"
#include "../runtime/neural_post_settings.hpp"

#include <array>
#include <cstdint>

namespace penumbra_vr::neural_post {
// UI callbacks publish preferences only; GL and worker lifetimes stay render-owned.
void Configure(const runtime::NeuralPostSettings& settings) noexcept;
[[nodiscard]] const char* StatusText() noexcept;
void ResetHistory() noexcept;
// Returns a fully processed stereo pair, or false to submit the original pair.
// Requires the same current GL context used to render both source textures.
// The experiment is disabled unless NeuralPost.ini explicitly enables it.
[[nodiscard]] bool TryProcess(
    const std::array<std::uint32_t, 2>& source,
    const runtime::VrHmdPose& pose,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    std::array<std::uint32_t, 2>& output) noexcept;
} // namespace penumbra_vr::neural_post
