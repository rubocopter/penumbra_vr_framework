#include "camera_matrix_override.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

namespace {

constexpr std::size_t kCameraSize = 0x8D3;
constexpr std::size_t kFovOffset = 0x10;
constexpr std::size_t kAspectOffset = 0x14;
constexpr std::size_t kViewOffset = 0x44;
constexpr std::size_t kProjectionOffset = 0x84;
constexpr std::size_t kFlagsOffset = 0x8D0;

using penumbra_vr::backends::black_plague::CameraMatrixOverride;
using penumbra_vr::backends::black_plague::CameraVisibilityOverride;
using penumbra_vr::backends::black_plague::kCameraLayout;
using penumbra_vr::runtime::VrMatrix44;

[[nodiscard]] VrMatrix44 FilledMatrix(float first) {
    VrMatrix44 matrix;
    for (std::size_t index = 0; index < matrix.values.size(); ++index) {
        matrix.values[index] = first + static_cast<float>(index);
    }
    return matrix;
}

void SetOriginalCamera(std::array<std::uint8_t, kCameraSize>& camera) {
    constexpr float kFov = 1.2F;
    constexpr float kAspect = 1.6F;
    std::memcpy(camera.data() + kFovOffset, &kFov, sizeof(kFov));
    std::memcpy(camera.data() + kAspectOffset, &kAspect, sizeof(kAspect));
    const VrMatrix44 view = FilledMatrix(10.0F);
    const VrMatrix44 projection = FilledMatrix(50.0F);
    std::memcpy(camera.data() + kViewOffset, view.values.data(), sizeof(view.values));
    std::memcpy(
        camera.data() + kProjectionOffset,
        projection.values.data(),
        sizeof(projection.values));
    camera[kFlagsOffset] = 1;
    camera[kFlagsOffset + 1] = 1;
    camera[kFlagsOffset + 2] = 1;
}

[[nodiscard]] bool TestVisibilityOverride() {
    std::array<std::uint8_t, kCameraSize> camera{};
    SetOriginalCamera(camera);
    const auto original = camera;
    constexpr float kCullFov = 2.0F;
    constexpr float kCullAspect = 1.4F;
    std::string error;

    CameraVisibilityOverride override(kCameraLayout);
    if (!override.Apply(
            camera.data(),
            FilledMatrix(100.0F),
            FilledMatrix(200.0F),
            kCullFov,
            kCullAspect,
            error)) {
        std::cerr << "Could not apply camera visibility override: " << error << '\n';
        return false;
    }
    float actual_fov = 0.0F;
    float actual_aspect = 0.0F;
    std::memcpy(&actual_fov, camera.data() + kFovOffset, sizeof(actual_fov));
    std::memcpy(
        &actual_aspect, camera.data() + kAspectOffset, sizeof(actual_aspect));
    if (actual_fov != kCullFov || actual_aspect != kCullAspect) {
        std::cerr << "Camera visibility scalars were not written exactly\n";
        return false;
    }
    if (!override.Restore(error) || camera != original || override.active()) {
        std::cerr << "Camera visibility restore failed: " << error << '\n';
        return false;
    }

    if (override.Apply(
            camera.data(),
            FilledMatrix(100.0F),
            FilledMatrix(200.0F),
            -1.0F,
            kCullAspect,
            error) || error.empty()) {
        std::cerr << "An invalid visibility FOV was accepted\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestExplicitRestore() {
    std::array<std::uint8_t, kCameraSize> camera{};
    SetOriginalCamera(camera);
    const auto original = camera;
    const VrMatrix44 view = FilledMatrix(100.0F);
    const VrMatrix44 projection = FilledMatrix(200.0F);

    penumbra_vr::backends::black_plague::CameraMatrixSnapshot snapshot;
    std::string error;
    if (!penumbra_vr::adapters::hpl1::CaptureCameraMatrices(
            camera.data(), kCameraLayout, snapshot, error) ||
        snapshot.flags != std::array<std::uint8_t, 3>{1, 1, 1}) {
        std::cerr << "Could not capture the original camera: " << error << '\n';
        return false;
    }

    CameraMatrixOverride override(kCameraLayout);
    if (!override.Apply(camera.data(), view, projection, error)) {
        std::cerr << "Could not apply camera override: " << error << '\n';
        return false;
    }
    if (std::memcmp(
            camera.data() + kViewOffset,
            view.values.data(),
            sizeof(view.values)) != 0 ||
        std::memcmp(
            camera.data() + kProjectionOffset,
            projection.values.data(),
            sizeof(projection.values)) != 0) {
        std::cerr << "Camera matrices were not written exactly\n";
        return false;
    }
    if (camera[kFlagsOffset] != 1 || camera[kFlagsOffset + 1] != 0 ||
        camera[kFlagsOffset + 2] != 0) {
        std::cerr << "Camera dirty flags were not changed as expected\n";
        return false;
    }
    if (!override.Restore(error)) {
        std::cerr << "Could not restore camera override: " << error << '\n';
        return false;
    }
    if (camera != original || override.active()) {
        std::cerr << "Explicit restore did not reproduce the original camera bytes\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestDestructorRestore() {
    std::array<std::uint8_t, kCameraSize> camera{};
    SetOriginalCamera(camera);
    const auto original = camera;
    std::string error;
    {
        CameraMatrixOverride override(kCameraLayout);
        if (!override.Apply(
                camera.data(), FilledMatrix(100.0F), FilledMatrix(200.0F), error)) {
            std::cerr << "Could not apply scoped camera override: " << error << '\n';
            return false;
        }
    }
    if (camera != original) {
        std::cerr << "The override destructor did not restore the camera\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestRejections() {
    std::array<std::uint8_t, kCameraSize> camera{};
    SetOriginalCamera(camera);
    VrMatrix44 invalid = FilledMatrix(100.0F);
    invalid.values[3] = std::numeric_limits<float>::infinity();
    CameraMatrixOverride override(kCameraLayout);
    std::string error;
    if (override.Apply(
            camera.data(), invalid, FilledMatrix(200.0F), error) || error.empty()) {
        std::cerr << "A non-finite camera matrix was accepted\n";
        return false;
    }
    if (override.Apply(nullptr, FilledMatrix(100.0F), FilledMatrix(200.0F), error) ||
        error.empty()) {
        std::cerr << "A null camera was accepted\n";
        return false;
    }
    if (!override.Apply(
            camera.data(), FilledMatrix(100.0F), FilledMatrix(200.0F), error)) {
        std::cerr << "The valid camera override failed: " << error << '\n';
        return false;
    }
    if (override.Apply(
            camera.data(), FilledMatrix(300.0F), FilledMatrix(400.0F), error) ||
        error.empty()) {
        std::cerr << "A second active override was accepted\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!TestExplicitRestore() || !TestDestructorRestore() ||
        !TestVisibilityOverride() || !TestRejections()) {
        return 1;
    }
    std::cout << "Camera matrix override tests passed\n";
    return 0;
}
