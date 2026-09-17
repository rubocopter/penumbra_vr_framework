#pragma once

#include "opengl_eye_targets.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::graphics {

struct OpenGlEnhancedEyeBinding {
    void* context = nullptr;
    std::int32_t output_framebuffer = 0;
    std::array<std::int32_t, 4> output_viewport{};
    Eye eye = Eye::left;
    bool active = false;
};

// Rework 23c890f's game-neutral eye stage: render the scene into RGBA16F with
// 2x MSAA, resolve, then apply the exact bounded five-tap/dark-SDR final pass.
// HPL material/light substitutions remain backend/game-specific work.
class OpenGlEnhancedEyeStage final {
public:
    OpenGlEnhancedEyeStage() noexcept = default;
    ~OpenGlEnhancedEyeStage() noexcept;
    OpenGlEnhancedEyeStage(const OpenGlEnhancedEyeStage&) = delete;
    OpenGlEnhancedEyeStage& operator=(const OpenGlEnhancedEyeStage&) = delete;

    [[nodiscard]] bool CreateOrResize(
        std::uint32_t width,
        std::uint32_t height,
        std::string& error) noexcept;
    [[nodiscard]] bool Destroy(std::string& error) noexcept;

    [[nodiscard]] bool BeginEye(
        Eye eye,
        OpenGlEnhancedEyeBinding& binding,
        std::string& error) const noexcept;
    [[nodiscard]] bool EndEye(
        OpenGlEnhancedEyeBinding& binding,
        std::string& error) const noexcept;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

    // Public only so the translation unit can use small allocation/cleanup
    // helpers without duplicating the OpenGL ownership code.
    struct EyeResources {
        std::uint32_t multisample_framebuffer = 0;
        std::uint32_t multisample_color = 0;
        std::uint32_t multisample_depth_stencil = 0;
        std::uint32_t hdr_framebuffer = 0;
        std::uint32_t hdr_texture = 0;
    };

private:
    void* context_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t program_ = 0;
    std::array<EyeResources, 2> eyes_{};
};

} // namespace penumbra_vr::graphics
