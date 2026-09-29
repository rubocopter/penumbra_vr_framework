#pragma once
#include "camera_matrix_override.hpp"

namespace penumbra_vr::backends::requiem {
// Exact Requiem Steam camera layout, recorded in the build manifest.
inline constexpr adapters::hpl1::CameraLayout kGameplayCameraLayout{
    .position_offset = 0x04, .fov_offset = 0x10, .aspect_offset = 0x14,
    .view_matrix_offset = 0x44, .projection_matrix_offset = 0x84,
    .flags_offset = 0x8D0, .view_updated_flag_index = 1,
    .projection_updated_flag_index = 2,
};

template<class Refresh>
[[nodiscard]] bool CaptureGameplayCamera(void* camera, Refresh refresh,
    adapters::hpl1::CameraMatrixSnapshot& snapshot, std::string& error) noexcept {
    if (camera == nullptr) {
        error = "The Requiem gameplay camera is null";
        return false;
    }
    // Native matrices are lazy caches. Resolve SetPosition/SetRotation before
    // visibility takes a snapshot or calibrates height from the inverse view.
    refresh(camera);
    return adapters::hpl1::CaptureCameraMatrices(
        camera, kGameplayCameraLayout, snapshot, error);
}
}
