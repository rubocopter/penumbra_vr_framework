#pragma once

#include <array>
#include <cstdint>

namespace penumbra_vr::graphics {

// Callsite identity belongs to each backend. These checks describe only the
// bound eye and screen texture after the native copy has executed.
struct RefractionCopyRequest final {
    std::uintptr_t image_base = 0;
    std::uintptr_t return_address = 0;
    bool inside_eye = false;
    std::int32_t expected_framebuffer = 0;
    std::int32_t current_framebuffer = 0;
    std::array<std::int32_t, 4> viewport{};
    std::int32_t eye_width = 0;
    std::int32_t eye_height = 0;
    std::int32_t native_width = 0;
    std::int32_t native_height = 0;
    std::uint32_t rectangle_texture = 0;
    std::int32_t max_rectangle_size = 0;
};

[[nodiscard]] constexpr bool RefractionEyeGeometryMatches(
    const RefractionCopyRequest& r) noexcept {
    return r.inside_eye && r.expected_framebuffer > 0 &&
        r.current_framebuffer == r.expected_framebuffer &&
        r.rectangle_texture != 0 && r.eye_width > 0 && r.eye_height > 0 &&
        r.native_width > 0 && r.native_height > 0 &&
        r.eye_width <= r.max_rectangle_size &&
        r.eye_height <= r.max_rectangle_size &&
        (r.eye_width != r.native_width || r.eye_height != r.native_height) &&
        r.viewport == std::array<std::int32_t, 4>{0, 0, r.eye_width, r.eye_height};
}

struct RefractionEyeCopyContext final {
    void* context = nullptr;
    std::int32_t framebuffer = 0;
    std::int32_t eye_width = 0;
    std::int32_t eye_height = 0;
    std::int32_t native_width = 0;
    std::int32_t native_height = 0;
};

enum class RefractionEyeCopyResult { skipped, copied, resized };

// Caller must first validate its own native refraction callsite. Never binds
// a texture or changes framebuffer/viewport/texture-unit state.
[[nodiscard]] RefractionEyeCopyResult CaptureBoundRefractionEye(
    const RefractionEyeCopyContext& eye) noexcept;

} // namespace penumbra_vr::graphics
