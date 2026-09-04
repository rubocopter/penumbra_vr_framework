#include "camera_matrix_override.hpp"

#include <cmath>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::size_t kViewOffset = 0x44;
constexpr std::size_t kProjectionOffset = 0x84;
constexpr std::size_t kCameraFlagsOffset = 0x8D0;
constexpr std::size_t kViewUpdatedFlagIndex = 1;
constexpr std::size_t kProjectionUpdatedFlagIndex = 2;

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

void Capture(const void* camera, CameraMatrixSnapshot& snapshot) noexcept {
    const std::uint8_t* bytes = Bytes(camera);
    std::memcpy(
        snapshot.view.values.data(),
        bytes + kViewOffset,
        sizeof(snapshot.view.values));
    std::memcpy(
        snapshot.projection.values.data(),
        bytes + kProjectionOffset,
        sizeof(snapshot.projection.values));
    std::memcpy(
        snapshot.flags.data(),
        bytes + kCameraFlagsOffset,
        sizeof(snapshot.flags));
}

void WriteSnapshot(void* camera, const CameraMatrixSnapshot& snapshot) noexcept {
    std::uint8_t* bytes = Bytes(camera);
    std::memcpy(
        bytes + kViewOffset,
        snapshot.view.values.data(),
        sizeof(snapshot.view.values));
    std::memcpy(
        bytes + kProjectionOffset,
        snapshot.projection.values.data(),
        sizeof(snapshot.projection.values));
    std::memcpy(
        bytes + kCameraFlagsOffset,
        snapshot.flags.data(),
        sizeof(snapshot.flags));
}

} // namespace

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
        error = "The RenderWorld camera pointer is null";
        return false;
    }
    if (!IsFinite(view) || !IsFinite(projection)) {
        error = "A camera override matrix contains a non-finite value";
        return false;
    }

    if (!CaptureCameraMatrices(camera, snapshot_, error)) {
        return false;
    }
    camera_ = camera;

    std::uint8_t* bytes = Bytes(camera_);
    std::memcpy(bytes + kViewOffset, view.values.data(), sizeof(view.values));
    std::memcpy(
        bytes + kProjectionOffset,
        projection.values.data(),
        sizeof(projection.values));
    bytes[kCameraFlagsOffset + kViewUpdatedFlagIndex] = 0;
    bytes[kCameraFlagsOffset + kProjectionUpdatedFlagIndex] = 0;
    return true;
}

bool CameraMatrixOverride::Restore(std::string& error) noexcept {
    error.clear();
    if (!active()) {
        return true;
    }

    void* camera = camera_;
    WriteSnapshot(camera, snapshot_);
    camera_ = nullptr;
    if (!CameraMatchesSnapshot(camera, snapshot_)) {
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
    CameraMatrixSnapshot& snapshot,
    std::string& error) noexcept {
    error.clear();
    snapshot = {};
    if (camera == nullptr) {
        error = "The RenderWorld camera pointer is null";
        return false;
    }
    Capture(camera, snapshot);
    if (!IsFinite(snapshot.view) || !IsFinite(snapshot.projection)) {
        snapshot = {};
        error = "The captured HPL camera contains a non-finite matrix";
        return false;
    }
    return true;
}

bool CameraMatchesSnapshot(
    const void* camera,
    const CameraMatrixSnapshot& snapshot) noexcept {
    if (camera == nullptr) {
        return false;
    }
    const std::uint8_t* bytes = Bytes(camera);
    return std::memcmp(
               bytes + kViewOffset,
               snapshot.view.values.data(),
               sizeof(snapshot.view.values)) == 0 &&
        std::memcmp(
               bytes + kProjectionOffset,
               snapshot.projection.values.data(),
               sizeof(snapshot.projection.values)) == 0 &&
        std::memcmp(
               bytes + kCameraFlagsOffset,
               snapshot.flags.data(),
               sizeof(snapshot.flags)) == 0;
}

} // namespace penumbra_vr::backends::black_plague
