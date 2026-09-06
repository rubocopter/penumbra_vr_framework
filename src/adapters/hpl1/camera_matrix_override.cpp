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

[[nodiscard]] bool HasVisibilityScalars(const CameraLayout& layout) noexcept {
    return layout.fov_offset != layout.aspect_offset &&
        layout.fov_offset != layout.view_matrix_offset &&
        layout.fov_offset != layout.projection_matrix_offset &&
        layout.fov_offset != layout.flags_offset &&
        layout.aspect_offset != layout.view_matrix_offset &&
        layout.aspect_offset != layout.projection_matrix_offset &&
        layout.aspect_offset != layout.flags_offset;
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

CameraVisibilityOverride::CameraVisibilityOverride(CameraLayout layout) noexcept
    : layout_(layout), matrix_override_(layout) {}

CameraVisibilityOverride::~CameraVisibilityOverride() noexcept {
    std::string ignored_error;
    static_cast<void>(Restore(ignored_error));
}

bool CameraVisibilityOverride::Apply(
    void* camera,
    const runtime::VrMatrix44& view,
    const runtime::VrMatrix44& projection,
    float vertical_fov_radians,
    float aspect,
    std::string& error) noexcept {
    error.clear();
    if (active()) {
        error = "A camera visibility override is already active";
        return false;
    }
    if (camera == nullptr) {
        error = "The HPL1 camera pointer is null";
        return false;
    }
    if (!HasVisibilityScalars(layout_)) {
        error = "The selected HPL1 visibility layout is invalid";
        return false;
    }
    if (!std::isfinite(vertical_fov_radians) ||
        !std::isfinite(aspect) ||
        vertical_fov_radians <= 0.0F || aspect <= 0.0F) {
        error = "The visibility FOV and aspect must be finite and positive";
        return false;
    }

    std::uint8_t* bytes = Bytes(camera);
    std::memcpy(&original_fov_, bytes + layout_.fov_offset, sizeof(float));
    std::memcpy(&original_aspect_, bytes + layout_.aspect_offset, sizeof(float));
    if (!matrix_override_.Apply(camera, view, projection, error)) {
        return false;
    }
    std::memcpy(
        bytes + layout_.fov_offset, &vertical_fov_radians, sizeof(float));
    std::memcpy(bytes + layout_.aspect_offset, &aspect, sizeof(float));
    camera_ = camera;
    return true;
}

bool CameraVisibilityOverride::Restore(std::string& error) noexcept {
    error.clear();
    if (!active()) {
        return true;
    }

    void* camera = camera_;
    std::uint8_t* bytes = Bytes(camera);
    std::memcpy(bytes + layout_.fov_offset, &original_fov_, sizeof(float));
    std::memcpy(bytes + layout_.aspect_offset, &original_aspect_, sizeof(float));
    camera_ = nullptr;

    std::string matrix_error;
    const bool matrix_restored = matrix_override_.Restore(matrix_error);
    const bool scalars_restored =
        std::memcmp(
            bytes + layout_.fov_offset, &original_fov_, sizeof(float)) == 0 &&
        std::memcmp(
            bytes + layout_.aspect_offset, &original_aspect_, sizeof(float)) == 0;
    if (!matrix_restored || !scalars_restored) {
        error = !matrix_restored
            ? "Could not restore the HPL camera matrices: " + matrix_error
            : "Could not restore the HPL camera visibility scalars";
        return false;
    }
    return true;
}

bool CameraVisibilityOverride::active() const noexcept {
    return camera_ != nullptr;
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
