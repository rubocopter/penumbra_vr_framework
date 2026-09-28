#pragma once

#include "vr_grab_pose.hpp"

namespace penumbra_vr::backends::requiem {

// The installed Requiem/BP HUD flashlight shares model-space light nodes:
// spotlight (0,-0.103966,0), ray (0,-0.203767,0). The Requiem headset trial
// confirms their alignment but disproves BP's model -Y -> socket -Z direction
// here. Turn the installed model toward socket +Z around its measured grip.
// Keep the parent rigid: HPL axis billboards normalize their rendered basis
// and retain the installed size while their centres inherit parent scaling.
// Rework's modified DAE/1.6 scale cannot be reused for this installed model.
inline constexpr float kInstalledFlashlightGripRadius = 0.020F;
inline constexpr runtime::VrAttachmentSocketProfile kInstalledFlashlightSocket{
    {1, 0, 0,
     0, 0, 1,
     0, -1, 0},
    {0, -0.016669F, 0}};

[[nodiscard]] inline runtime::VrMatrix44 InstalledFlashlightToolPose(
    const runtime::VrMatrix44& hand_socket) noexcept {
    return runtime::ComposeAttachmentSocketPose(
        hand_socket, kInstalledFlashlightSocket);
}

} // namespace penumbra_vr::backends::requiem
