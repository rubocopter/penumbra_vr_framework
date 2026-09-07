#pragma once

#include "vr_math.hpp"

#include <array>
#include <string>

namespace penumbra_vr::runtime {

// Runtime-neutral port of Rework's cVRTrackingSpace. Tracking poses always
// remain in metres and world scale is deliberately fixed at 1.0.
class VrTrackingSpace final {
public:
    void SetHeadTrackingPose(const VrMatrix34& pose) noexcept;
    [[nodiscard]] const VrMatrix34& head_tracking_pose() const noexcept;

    [[nodiscard]] bool SetPlayerWorldPose(
        const VrMatrix44& pose,
        std::string& error) noexcept;
    void SetPlayerWorldPosition(const std::array<float, 3>& position) noexcept;
    [[nodiscard]] const VrMatrix44& player_world_pose() const noexcept;

    void SetHeightCalibration(float height) noexcept;
    void SetPostureOffset(float height) noexcept;
    void SetSeatedOffset(float height) noexcept;
    [[nodiscard]] float height_calibration() const noexcept;
    [[nodiscard]] float posture_offset() const noexcept;
    [[nodiscard]] float seated_offset() const noexcept;

    void SetWorldYaw(float radians) noexcept;
    void AddWorldYaw(float radians) noexcept;
    [[nodiscard]] float world_yaw() const noexcept;
    [[nodiscard]] std::array<float, 3> TrackingDirectionToWorld(
        const std::array<float, 3>& direction) const noexcept;
    [[nodiscard]] bool RecenterOrientation(std::string& error) noexcept;

    [[nodiscard]] bool TrackingToWorldTransform(
        VrMatrix44& transform,
        std::string& error) const noexcept;
    [[nodiscard]] bool TrackingToWorld(
        const VrMatrix34& tracking_pose,
        VrMatrix44& world_pose,
        std::string& error) const noexcept;
    [[nodiscard]] bool HeadWorldPose(
        VrMatrix44& world_pose,
        std::string& error) const noexcept;

    [[nodiscard]] static constexpr float world_units_per_meter() noexcept {
        return 1.0F;
    }

private:
    VrMatrix34 head_tracking_pose_{{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    }};
    VrMatrix44 player_world_pose_ = IdentityMatrix();
    float height_calibration_ = 0.0F;
    float posture_offset_ = 0.0F;
    float seated_offset_ = 0.0F;
    float world_yaw_ = 0.0F;
};

} // namespace penumbra_vr::runtime
