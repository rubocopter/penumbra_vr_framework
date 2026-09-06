#pragma once

#include "../../adapters/hpl1/camera_matrix_override.hpp"

namespace penumbra_vr::backends::black_plague {

inline constexpr adapters::hpl1::CameraLayout kCameraLayout{
    .fov_offset = 0x10,
    .aspect_offset = 0x14,
    .view_matrix_offset = 0x44,
    .projection_matrix_offset = 0x84,
    .flags_offset = 0x8D0,
    .view_updated_flag_index = 1,
    .projection_updated_flag_index = 2,
};

using CameraMatrixSnapshot = adapters::hpl1::CameraMatrixSnapshot;
using CameraMatrixOverride = adapters::hpl1::CameraMatrixOverride;
using CameraVisibilityOverride = adapters::hpl1::CameraVisibilityOverride;

} // namespace penumbra_vr::backends::black_plague
