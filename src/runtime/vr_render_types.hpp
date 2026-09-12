#pragma once

#include "vr_tracking_types.hpp"

#include <cstdint>

namespace penumbra_vr::runtime {

struct VrRenderTargetSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct VrEyeConfiguration {
    float left_tangent = 0.0F;
    float right_tangent = 0.0F;
    float top_tangent = 0.0F;
    float bottom_tangent = 0.0F;
    VrMatrix34 eye_to_head;
};

} // namespace penumbra_vr::runtime
