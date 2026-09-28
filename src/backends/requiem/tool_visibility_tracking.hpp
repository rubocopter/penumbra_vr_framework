#pragma once

#include "vr_math.hpp"

#include <cstddef>
#include <string>

namespace penumbra_vr::backends::requiem {

// Tool and visible hand must consume the same palm publication. A stale palm
// from an earlier world-yaw epoch cannot be used during a snap turn.
[[nodiscard]] inline runtime::VrMatrix44 SelectToolPalmPose(
    const runtime::VrMatrix44& raw,
    const runtime::VrMatrix44& resolved,
    bool resolved_valid,
    bool same_yaw_epoch) noexcept {
    return resolved_valid && same_yaw_epoch ? resolved : raw;
}

// Use the same HMD sample to place a native tool before visibility/light
// collection and to draw its hand in the subsequent stereo eye pass.
[[nodiscard]] inline bool ComposeToolVisibilityTracking(
    const runtime::VrMatrix44& head_view,
    const runtime::VrMatrix34& tracked_head,
    runtime::VrMatrix44& world_from_tracking,
    std::string& error) noexcept {
    runtime::VrMatrix34 rigid_view{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            rigid_view.values[row * 4U + column] =
                head_view.values[row * 4U + column];
        }
    }
    runtime::VrMatrix44 world_from_head{}, tracking_from_head{};
    if (!runtime::InvertRigidTransform(rigid_view, world_from_head, error) ||
        !runtime::InvertRigidTransform(tracked_head, tracking_from_head,
            error)) return false;
    world_from_tracking = runtime::Multiply(
        world_from_head, tracking_from_head);
    return true;
}

} // namespace penumbra_vr::backends::requiem
