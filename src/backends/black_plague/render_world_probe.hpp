#pragma once

#include "eye_target_probe.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

enum class FramebufferApi : std::uint8_t {
    unavailable,
    core,
    ext,
};

struct RenderWorldFrameTelemetry {
    std::uint32_t calls = 0;
    std::uintptr_t renderer = 0;
    std::uintptr_t world = 0;
    std::uintptr_t camera = 0;
    float frame_time = 0.0F;
    bool has_current_gl_context = false;
    FramebufferApi framebuffer_api = FramebufferApi::unavailable;
    std::array<std::int32_t, 4> viewport{};
    std::array<std::int32_t, 2> max_viewport_dimensions{};
    std::int32_t framebuffer_binding = 0;
    std::int32_t max_texture_size = 0;
    std::int32_t max_renderbuffer_size = 0;
    std::array<char, 64> open_gl_version{};
    EyeTargetProbeTelemetry eye_targets;
};

[[nodiscard]] bool InstallRenderWorldProbe(std::string& error) noexcept;
[[nodiscard]] bool RemoveRenderWorldProbe(std::string& error) noexcept;
[[nodiscard]] bool ValidateControlledWorldDuplication(
    std::uint32_t frames,
    std::string& error) noexcept;
[[nodiscard]] RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
