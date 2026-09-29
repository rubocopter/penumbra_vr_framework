#include "camera_capture.hpp"
#include "vr_math.hpp"
#include <array>
#include <cstring>
#include <iostream>

int main() {
    using namespace penumbra_vr;
    std::array<unsigned char, 0x8D3> camera{};
    const std::array<float, 3> position{0, 1.65F, 0};
    auto view = runtime::IdentityMatrix();
    std::memcpy(camera.data()+0x04, position.data(), sizeof(position));
    std::memcpy(camera.data()+0x44, &view, sizeof(view));
    std::memcpy(camera.data()+0x84, &view, sizeof(view));
    camera[0x8D1] = 1; // Native SetPosition invalidated the cached view.
    auto refresh = [&](void*) {
        view.values[7] = -position[1];
        std::memcpy(camera.data()+0x44, &view, sizeof(view));
        camera[0x8D1] = 0;
    };
    adapters::hpl1::CameraMatrixSnapshot snapshot;
    std::string error;
    if (!backends::requiem::CaptureGameplayCamera(camera.data(), refresh,
            snapshot, error) || std::fabs(snapshot.view.values[7]+1.65F)>1.0e-5F) {
        std::cerr << "Spawn camera captured stale floor-height view: " << error << '\n';
        return 1;
    }
    const auto fresh = camera;
    if (!backends::requiem::CaptureGameplayCamera(camera.data(), refresh,
            snapshot, error) || fresh != camera) return 1;
    return 0;
}
