#pragma once

#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

struct RenderWorldFrameTelemetry {
    std::uint32_t calls = 0;
    std::uintptr_t renderer = 0;
    std::uintptr_t world = 0;
    std::uintptr_t camera = 0;
    float frame_time = 0.0F;
};

[[nodiscard]] bool InstallRenderWorldProbe(std::string& error) noexcept;
[[nodiscard]] bool RemoveRenderWorldProbe(std::string& error) noexcept;
[[nodiscard]] RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
