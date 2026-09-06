#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::graphics {

enum class Eye : std::uint8_t {
    left,
    right,
};

struct OpenGlEyeTarget {
    std::uint32_t framebuffer = 0;
    std::uint32_t color_texture = 0;
    std::uint32_t depth_stencil_renderbuffer = 0;
};

struct OpenGlEyeBinding {
    void* context = nullptr;
    std::int32_t previous_framebuffer = 0;
    std::array<std::int32_t, 4> previous_viewport{};
    std::array<std::int32_t, 4> previous_scissor{};
    bool previous_scissor_enabled = false;
    bool active = false;
};

class OpenGlEyeTargets final {
public:
    OpenGlEyeTargets() noexcept = default;
    ~OpenGlEyeTargets() noexcept;
    OpenGlEyeTargets(const OpenGlEyeTargets&) = delete;
    OpenGlEyeTargets& operator=(const OpenGlEyeTargets&) = delete;

    [[nodiscard]] bool CreateOrResize(
        std::uint32_t width,
        std::uint32_t height,
        std::string& error) noexcept;
    [[nodiscard]] bool Destroy(std::string& error) noexcept;

    [[nodiscard]] bool BeginEye(
        Eye eye,
        OpenGlEyeBinding& binding,
        std::string& error) const noexcept;
    [[nodiscard]] bool EndEye(
        OpenGlEyeBinding& binding,
        std::string& error) const noexcept;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] const OpenGlEyeTarget& target(Eye eye) const noexcept;

private:
    [[nodiscard]] bool OwnsFramebuffer(std::uint32_t object) const noexcept;
    [[nodiscard]] bool OwnsTexture(std::uint32_t object) const noexcept;
    [[nodiscard]] bool OwnsRenderbuffer(std::uint32_t object) const noexcept;

    void* context_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::array<OpenGlEyeTarget, 2> targets_{};
};

} // namespace penumbra_vr::graphics
