#pragma once

#include <array>
#include <cstdint>

namespace penumbra_vr::runtime {
struct VrMatrix34 {
    std::array<float, 12> values{};
};

struct VrTrackingSampleIdentity {
    std::uint64_t sequence = 0;
    std::uint64_t timestamp_ms = 0;
    std::uint64_t pose_epoch = 0;
    std::uint64_t yaw_epoch = 0;
};

[[nodiscard]] constexpr bool SameTrackingEpoch(
    const VrTrackingSampleIdentity& left,
    const VrTrackingSampleIdentity& right) noexcept {
    return left.pose_epoch != 0 && left.yaw_epoch != 0 &&
        left.pose_epoch == right.pose_epoch &&
        left.yaw_epoch == right.yaw_epoch;
}

struct VrHmdPose {
    VrMatrix34 device_to_absolute;
    std::array<float, 3> velocity{};
    std::array<float, 3> angular_velocity{};
    std::uint32_t tracking_result = 0;
    bool pose_valid = false;
    bool device_connected = false;
    VrTrackingSampleIdentity identity{};
};
} // namespace penumbra_vr::runtime
