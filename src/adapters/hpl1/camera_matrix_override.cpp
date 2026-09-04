#include "camera_matrix_override.hpp"

#include <cmath>
#include <cstring>

namespace penumbra_vr::adapters::hpl1 {
namespace {

[[nodiscard]] std::uint8_t* Bytes(void* address) noexcept {
    return static_cast<std::uint8_t*>(address);
}

[[nodiscard]] const std::uint8_t* Bytes(const void* address) noexcept {
    return static_cast<const std::uint8_t*>(address);
}

[[nodiscard]] bool IsFinite(const runtime::VrMatrix44& matrix) noexcept {
    for (const float value : matrix.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsUsable(const CameraLayout& layout) noexcept {
    return layout.view_matrix_offset != layout.projection_matrix_offset &&
        layout.flags_offset != layout.view_matrix_offset &&
        layout.flags_offset != layout.projection_matrix_offset &&
        layout.view_updated_flag_index < 3 &&
        layout.projection_updated_flag_index < 3 &&
        layout.view_updated_flag_index != layout.projection_updated_flag_index;
}

void Capture(
    const void* camera,
    const CameraLayout& layout,
    CameraMatrixSnapshot& snapshot) noexcept {
    const std::uint8_t* bytes = Bytes(camera);
    std::memcpy(
        snapshot.view.values.data(),
        bytes + layout.view_matrix_offset,
        sizeof(snapshot.view.values));
    std::memcpy(
        snapshot.projection.values.data(),
        bytes + layout.projection_matrix_offset,
        sizeof(snapshot.projection.values));
    std::memcpy(
        snapshot.flags.data(),
        bytes + layout.flags_offset,
        sizeof(snapshot.flags));
}

void WriteSnapshot(
    void* camera,
    const CameraLayout& layout,
    const CameraMatrixSnapshot& snapshot) noexcept {
    std::uint8_t* bytes = Bytes(camera);
    std::memcpy(
        bytes + layout.view_matrix_offset,
        snapshot.view.values.data(),
        sizeof(snapshot.view.values));
    std::memcpy(
        bytes + layout.projection_matrix_offset,
        snapshot.projection.values.data(),
        sizeof(snapshot.projection.values));
    std::memcpy(
        bytes + layout.flags_offset,
        snapshot.flags.data(),
        sizeof(snapshot.flags));
}

} // namespace

CameraMatrixOverride::CameraMatrixOverride(CameraLayout layout) noexcept
    : layout_(layout) {}

CameraMatrixOverride::~CameraMatrixOverride() noexcept {
    std::string ignored_error;
    static_cast<void>(Restore(ignored_error));
}

bool CameraMatrixOverride::Apply(
    void* camera,
    const runtime::VrMatrix44& view,
    const runtime::VrMatrix44& projection,
    std::string& error) noexcept {
    error.clear();
    if (active()) {
        error = "A camera matrix override is already active";
        return false;
    }
    if (camera == nullptr) {
        error = "The HPL1 camera pointer is null";
        return false;
    }
    if (!IsUsable(layout_)) {
        error = "The selected HPL1 camera layout is invalid";
        return false;
    }
    if (!IsFinite(view) || !IsFinite(projection)) {
        error = "A camera override matrix contains a non-finite value";
        return false;
    }

    if (!CaptureCameraMatrices(camera, layout_, snapshot_, error)) {
        return false;
    }
    camera_ = camera;

    std::uint8_t* bytes = Bytes(camera_);
    std::memcpy(
        bytes + layout_.view_matrix_offset,
        view.values.data(),
        sizeof(view.values));
    std::memcpy(
        bytes + layout_.projection_matrix_offset,
        projection.values.data(),
        sizeof(projection.values));
    bytes[layout_.flags_offset + layout_.view_updated_flag_index] = 0;
    bytes[layout_.flags_offset + layout_.projection_updated_flag_index] = 0;
    return true;
}

bool CameraMatrixOverride::Restore(std::string& error) noexcept {
    error.clear();
    if (!active()) {
        return true;
    }

    void* camera = camera_;
    WriteSnapshot(camera, layout_, snapshot_);
    camera_ = nullptr;
    if (!CameraMatchesSnapshot(camera, layout_, snapshot_)) {
        error = "Camera matrix restoration did not reproduce the captured bytes";
        return false;
    }
    return true;
}

bool CameraMatrixOverride::active() const noexcept {
    return camera_ != nullptr;
}

const CameraMatrixSnapshot& CameraMatrixOverride::snapshot() const noexcept {
    return snapshot_;
}

bool CaptureCameraMatrices(
    const void* camera,
    const CameraLayout& layout,
    CameraMatrixSnapshot& snapshot,
    std::string& error) noexcept {
    error.clear();
    snapshot = {};
    if (camera == nullptr) {
        error = "The HPL1 camera pointer is null";
        return false;
    }
    if (!IsUsable(layout)) {
        error = "The selected HPL1 camera layout is invalid";
        return false;
    }
    Capture(camera, layout, snapshot);
    if (!IsFinite(snapshot.view) || !IsFinite(snapshot.projection)) {
        snapshot = {};
        error = "The captured HPL1 camera contains a non-finite matrix";
        return false;
    }
    return true;
}

bool CameraMatchesSnapshot(
    const void* camera,
    const CameraLayout& layout,
    const CameraMatrixSnapshot& snapshot) noexcept {
    if (camera == nullptr || !IsUsable(layout)) {
        return false;
    }
    const std::uint8_t* bytes = Bytes(camera);
    return std::memcmp(
               bytes + layout.view_matrix_offset,
               snapshot.view.values.data(),
               sizeof(snapshot.view.values)) == 0 &&
        std::memcmp(
               bytes + layout.projection_matrix_offset,
               snapshot.projection.values.data(),
               sizeof(snapshot.projection.values)) == 0 &&
        std::memcmp(
               bytes + layout.flags_offset,
               snapshot.flags.data(),
               sizeof(snapshot.flags)) == 0;
}

} // namespace penumbra_vr::adapters::hpl1
