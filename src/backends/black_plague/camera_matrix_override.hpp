#pragma once

#include "../../adapters/hpl1/camera_matrix_override.hpp"

namespace penumbra_vr::backends::black_plague {

inline constexpr adapters::hpl1::CameraLayout kCameraLayout{
    .view_matrix_offset = 0x44,
    .projection_matrix_offset = 0x84,
    .flags_offset = 0x8D0,
    .view_updated_flag_index = 1,
    .projection_updated_flag_index = 2,
};

using CameraMatrixSnapshot = adapters::hpl1::CameraMatrixSnapshot;
using CameraMatrixOverride = adapters::hpl1::CameraMatrixOverride;

} // namespace penumbra_vr::backends::black_plague
