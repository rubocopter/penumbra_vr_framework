#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::hooks {

struct OpenGlFrameTelemetry {
    std::uint32_t matrix_mode_calls = 0;
    std::uint32_t projection_loads = 0;
    std::uint32_t model_view_loads = 0;
    std::uint32_t texture_loads = 0;
    std::uint32_t ortho_calls = 0;
    bool has_projection = false;
    std::array<float, 16> last_projection{};
    std::uint32_t unique_model_view_matrices = 0;
    std::uint32_t dropped_model_view_matrices = 0;
    std::uint32_t dominant_model_view_loads = 0;
    bool has_dominant_model_view = false;
    std::array<float, 16> dominant_model_view{};
};

[[nodiscard]] bool InstallOpenGlMatrixTelemetry(std::string& error) noexcept;
[[nodiscard]] bool RemoveOpenGlMatrixTelemetry(std::string& error) noexcept;
[[nodiscard]] OpenGlFrameTelemetry ConsumeOpenGlFrameTelemetry() noexcept;

} // namespace penumbra_vr::hooks
