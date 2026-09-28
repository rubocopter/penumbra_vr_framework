#pragma once

#include "opengl_refraction_copy.hpp"

#include <array>
#include <cstdint>

namespace penumbra_vr::backends::black_plague {

// Return sites of the two exact-build Renderer3D calls to the low-level
// CopyContextToTexure vtable slot. No other screen-copy consumer may use the
// eye-sized capture path.
using RefractionCopyRequest = graphics::RefractionCopyRequest;

[[nodiscard]] constexpr bool IsRefractionCopyReturn(
    std::uintptr_t image_base, std::uintptr_t return_address) noexcept {
    if (image_base == 0 || return_address < image_base) return false;
    const auto site = return_address - image_base;
    return site == 0x12BC7F || site == 0x12BCCE;
}

[[nodiscard]] constexpr bool ShouldCaptureRefractionEye(
    const RefractionCopyRequest& request) noexcept {
    return IsRefractionCopyReturn(request.image_base, request.return_address) &&
        graphics::RefractionEyeGeometryMatches(request);
}

} // namespace penumbra_vr::backends::black_plague
