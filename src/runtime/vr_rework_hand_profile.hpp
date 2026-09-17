#pragma once

#include "vr_math.hpp"

#include <cmath>

namespace penumbra_vr::runtime::rework_hand_profile {

// Overture Rework 23c890f hand-model profile. These values place the imported
// HAND_Low mesh relative to the tracked controller. The mesh scale remains a
// renderer concern; all physics/interaction transforms stay rigid metres.
inline constexpr float kVisualTranslationZ = 0.05F;
inline constexpr float kVisualRotationY = 0.525F;
inline constexpr float kVisualRotationZ = -1.1F;
inline constexpr float kMeshScale = 0.006F;

// Rework applies this final local translation when constructing the palm box.
inline constexpr float kCollisionTranslationX = 0.011F;
inline constexpr float kCollisionTranslationY = 0.002F;
inline constexpr float kCollisionTranslationZ = -0.011F;

// Rework 23c890f fits attachment handles through the cylinder formed by the
// four long fingers, rather than through the controller/palm origin.  These
// offsets belong to the imported hand rig; model-specific grip points and
// radii remain backend profile data.
inline constexpr float kGripSocketX = 0.050F;
inline constexpr float kLeftGripSocketY = 0.019F;
inline constexpr float kRightGripSocketY = -0.014F;

[[nodiscard]] inline VrMatrix44 VisualLocalPose() noexcept {
    const float cy = std::cos(kVisualRotationY);
    const float sy = std::sin(kVisualRotationY);
    const float cz = std::cos(kVisualRotationZ);
    const float sz = std::sin(kVisualRotationZ);

    // HPL's XYZ Euler convention used by Rework produces Rz * Ry * Rx.
    // Rotation X is zero for this profile, so write the rigid matrix directly.
    VrMatrix44 result = IdentityMatrix();
    result.values = {
        cz * cy, -sz, cz * sy, 0.0F,
        sz * cy,  cz, sz * sy, 0.0F,
             -sy, 0.0F,      cy, kVisualTranslationZ,
        0.0F, 0.0F, 0.0F, 1.0F,
    };
    return result;
}

[[nodiscard]] inline VrMatrix44 CollisionLocalPose() noexcept {
    VrMatrix44 collision_offset = IdentityMatrix();
    collision_offset.values[3] = kCollisionTranslationX;
    collision_offset.values[7] = kCollisionTranslationY;
    collision_offset.values[11] = kCollisionTranslationZ;
    return Multiply(VisualLocalPose(), collision_offset);
}

[[nodiscard]] inline VrMatrix44 ApplyVisualLocalPose(
    const VrMatrix44& tracked_pose) noexcept {
    return Multiply(tracked_pose, VisualLocalPose());
}

[[nodiscard]] inline VrMatrix44 AttachmentGripLocalPose(
    bool left,
    float open_centre_offset) noexcept {
    VrMatrix44 socket = IdentityMatrix();
    socket.values[3] = kGripSocketX;
    socket.values[7] = left
        ? kLeftGripSocketY + open_centre_offset
        : kRightGripSocketY - open_centre_offset;
    return Multiply(VisualLocalPose(), socket);
}

[[nodiscard]] inline VrMatrix44 ApplyAttachmentGripLocalPose(
    const VrMatrix44& tracked_pose,
    bool left,
    float open_centre_offset) noexcept {
    return Multiply(
        tracked_pose,
        AttachmentGripLocalPose(left, open_centre_offset));
}

} // namespace penumbra_vr::runtime::rework_hand_profile
