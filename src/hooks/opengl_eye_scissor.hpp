#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::hooks {

using ScissorRect = std::array<std::int32_t, 4>;

// HPL supplies bottom-left GL coordinates in desktop pixels. Expand by one
// source pixel to cover its inclusive bounds and legacy Y conversion, then
// round outwards. Empty rectangles stay empty; invalid input is not remapped.
[[nodiscard]] bool MapEyeScissor(
    const ScissorRect& source, const std::array<std::int32_t, 2>& source_size,
    const ScissorRect& viewport, ScissorRect& result) noexcept;

[[nodiscard]] bool InstallOpenGlEyeScissor(std::string& error) noexcept;
[[nodiscard]] bool RemoveOpenGlEyeScissor(std::string& error) noexcept;

// Stack-scoped and thread-local: never applies to the monitor/UI pass or a
// different context, framebuffer or viewport used by an intermediate effect.
class ScopedEyeScissor final {
public:
    explicit ScopedEyeScissor(std::array<std::int32_t, 2> source_size) noexcept;
    ~ScopedEyeScissor() noexcept;
    ScopedEyeScissor(const ScopedEyeScissor&) = delete;
    ScopedEyeScissor& operator=(const ScopedEyeScissor&) = delete;

    [[nodiscard]] ScissorRect Remap(const ScissorRect& source) noexcept;
    [[nodiscard]] std::uint32_t remapped() const noexcept { return remapped_; }
    [[nodiscard]] std::uint32_t bypassed() const noexcept { return bypassed_; }

private:
    ScopedEyeScissor* previous_ = nullptr;
    void* context_ = nullptr;
    std::int32_t framebuffer_ = 0;
    ScissorRect viewport_{};
    std::array<std::int32_t, 2> source_size_{};
    std::uint32_t remapped_ = 0;
    std::uint32_t bypassed_ = 0;
};

} // namespace penumbra_vr::hooks
