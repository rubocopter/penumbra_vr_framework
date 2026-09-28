#pragma once

#include "vr_grab_pose.hpp"

namespace penumbra_vr::backends::requiem {

// The installed Requiem/BP HUD flashlight shares model-space light nodes:
// spotlight (0,-0.103966,0), ray (0,-0.203767,0). The BP measured socket
// turns model -Y toward hand -Z. Rework's modified flashlight DAE and -90 deg
// profile have a different mesh/light layout and cannot be copied here.
inline constexpr float kInstalledFlashlightGripRadius = 0.020F;
inline constexpr runtime::VrAttachmentSocketProfile kInstalledFlashlightSocket{
    {1, 0, 0,
     0, 0, -1,
     0, 1, 0},
    {0, -0.016669F, 0}};

[[nodiscard]] inline runtime::VrMatrix44 InstalledFlashlightToolPose(
    const runtime::VrMatrix44& hand_socket) noexcept {
    return runtime::ComposeAttachmentSocketPose(
        hand_socket, kInstalledFlashlightSocket);
}

} // namespace penumbra_vr::backends::requiem
