#include "vr_tracking_space.hpp"

#include <cmath>

namespace penumbra_vr::runtime {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMinimumHorizontalForward = 1.0e-6F;

[[nodiscard]] VrMatrix34 Collapse(const VrMatrix44& matrix) noexcept {
    VrMatrix34 result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            result.values[row * 4U + column] =
                matrix.values[row * 4U + column];
        }
    }
    return result;
}

[[nodiscard]] VrMatrix44 YawRotation(float radians) noexcept {
    VrMatrix44 result = IdentityMatrix();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.values[0] = cosine;
    result.values[2] = sine;
    result.values[8] = -sine;
    result.values[10] = cosine;
    return result;
}

[[nodiscard]] bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

} // namespace

void VrTrackingSpace::SetHeadTrackingPose(const VrMatrix34& pose) noexcept {
    head_tracking_pose_ = pose;
}

const VrMatrix34& VrTrackingSpace::head_tracking_pose() const noexcept {
    return head_tracking_pose_;
}

bool VrTrackingSpace::SetPlayerWorldPose(
    const VrMatrix44& pose,
    std::string& error) noexcept {
    VrMatrix44 inverse;
    if (!InvertRigidTransform(Collapse(pose), inverse, error)) {
        error = "The player world pose is invalid: " + error;
        return false;
    }
    player_world_pose_ = pose;
    return true;
}

void VrTrackingSpace::SetPlayerWorldPosition(
    const std::array<float, 3>& position) noexcept {
    player_world_pose_ = IdentityMatrix();
    player_world_pose_.values[3] = position[0];
    player_world_pose_.values[7] = position[1];
    player_world_pose_.values[11] = position[2];
}

const VrMatrix44& VrTrackingSpace::player_world_pose() const noexcept {
    return player_world_pose_;
}

void VrTrackingSpace::SetHeightCalibration(float height) noexcept {
    height_calibration_ = IsFinite(height) ? height : 0.0F;
}

void VrTrackingSpace::SetPostureOffset(float height) noexcept {
    posture_offset_ = IsFinite(height) ? height : 0.0F;
}

void VrTrackingSpace::SetSeatedOffset(float height) noexcept {
    seated_offset_ = IsFinite(height) ? height : 0.0F;
}

float VrTrackingSpace::height_calibration() const noexcept {
    return height_calibration_;
}

float VrTrackingSpace::posture_offset() const noexcept {
    return posture_offset_;
}

float VrTrackingSpace::seated_offset() const noexcept {
    return seated_offset_;
}

void VrTrackingSpace::SetWorldYaw(float radians) noexcept {
    if (!IsFinite(radians)) {
        return;
    }
    world_yaw_ = std::fmod(radians + kPi, 2.0F * kPi);
    if (world_yaw_ < 0.0F) {
        world_yaw_ += 2.0F * kPi;
    }
    world_yaw_ -= kPi;
}

void VrTrackingSpace::AddWorldYaw(float radians) noexcept {
    SetWorldYaw(world_yaw_ + radians);
}

float VrTrackingSpace::world_yaw() const noexcept {
    return world_yaw_;
}

std::array<float, 3> VrTrackingSpace::TrackingDirectionToWorld(
    const std::array<float, 3>& direction) const noexcept {
    const float cosine = std::cos(world_yaw_);
    const float sine = std::sin(world_yaw_);
    return {
        cosine * direction[0] + sine * direction[2],
        direction[1],
        -sine * direction[0] + cosine * direction[2],
    };
}

bool VrTrackingSpace::RecenterOrientation(std::string& error) noexcept {
    VrMatrix44 head;
    if (!HeadWorldPose(head, error)) {
        return false;
    }

    float forward_x = -head.values[2];
    float forward_z = -head.values[10];
    const float length = std::hypot(forward_x, forward_z);
    if (!IsFinite(length) || length <= kMinimumHorizontalForward) {
        error = "The HMD is too close to vertical to recenter yaw";
        return false;
    }
    forward_x /= length;
    forward_z /= length;
    AddWorldYaw(std::atan2(forward_x, -forward_z));
    error.clear();
    return true;
}

bool VrTrackingSpace::TrackingToWorldTransform(
    VrMatrix44& transform,
    std::string& error) const noexcept {
    VrMatrix44 absolute_to_head;
    if (!InvertRigidTransform(
            head_tracking_pose_, absolute_to_head, error)) {
        transform = {};
        error = "The tracking head pose is invalid: " + error;
        return false;
    }

    VrMatrix44 calibrated_head = ExpandMatrix(head_tracking_pose_);
    calibrated_head.values[3] = 0.0F;
    calibrated_head.values[7] =
        (head_tracking_pose_.values[7] - 0.2F) * 1.065F +
        height_calibration_ + posture_offset_ + seated_offset_;
    calibrated_head.values[11] = 0.0F;

    const VrMatrix44 rotated_head = Multiply(
        YawRotation(world_yaw_), calibrated_head);
    const VrMatrix44 head_world = Multiply(
        player_world_pose_, rotated_head);
    transform = Multiply(head_world, absolute_to_head);
    error.clear();
    return true;
}

bool VrTrackingSpace::TrackingToWorld(
    const VrMatrix34& tracking_pose,
    VrMatrix44& world_pose,
    std::string& error) const noexcept {
    VrMatrix44 transform;
    if (!TrackingToWorldTransform(transform, error)) {
        world_pose = {};
        return false;
    }
    VrMatrix44 ignored_inverse;
    if (!InvertRigidTransform(tracking_pose, ignored_inverse, error)) {
        world_pose = {};
        error = "The tracking pose is invalid: " + error;
        return false;
    }
    world_pose = Multiply(transform, ExpandMatrix(tracking_pose));
    error.clear();
    return true;
}

bool VrTrackingSpace::HeadWorldPose(
    VrMatrix44& world_pose,
    std::string& error) const noexcept {
    return TrackingToWorld(head_tracking_pose_, world_pose, error);
}

} // namespace penumbra_vr::runtime
