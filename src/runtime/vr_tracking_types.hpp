#pragma once

#include <array>
#include <cstdint>

namespace penumbra_vr::runtime {
struct VrMatrix34 {
    std::array<float, 12> values{};
};

struct VrHmdPose {
    VrMatrix34 device_to_absolute;
    std::array<float, 3> velocity{};
    std::array<float, 3> angular_velocity{};
    std::uint32_t tracking_result = 0;
    bool pose_valid = false;
    bool device_connected = false;
};
} // namespace penumbra_vr::runtime
