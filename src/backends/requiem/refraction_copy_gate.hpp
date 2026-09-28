#pragma once

#include <array>
#include <cstdint>

namespace penumbra_vr::backends::requiem {

// Return sites of the two exact-build Renderer3D calls to the low-level
// CopyContextToTexure vtable slot. No other screen-copy consumer may use the
// eye-sized capture path.
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

[[nodiscard]] constexpr bool ShouldCaptureRefractionEye(
    const RefractionCopyRequest& request) noexcept {
    if (request.image_base == 0 ||
        request.return_address < request.image_base ||
        !request.inside_eye ||
        request.expected_framebuffer <= 0 ||
        request.current_framebuffer != request.expected_framebuffer ||
        request.rectangle_texture == 0 ||
        request.eye_width <= 0 || request.eye_height <= 0 ||
        request.native_width <= 0 || request.native_height <= 0 ||
        request.eye_width > request.max_rectangle_size ||
        request.eye_height > request.max_rectangle_size ||
        (request.eye_width == request.native_width &&
            request.eye_height == request.native_height) ||
        request.viewport != std::array<std::int32_t, 4>{
            0, 0, request.eye_width, request.eye_height}) return false;

    const auto site = request.return_address - request.image_base;
    return site == 0x12C27F || site == 0x12C2CE;
}

} // namespace penumbra_vr::backends::requiem
