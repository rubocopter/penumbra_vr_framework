#pragma once

#include "vr_grab_pose.hpp"

namespace penumbra_vr::backends::requiem {

// The installed Requiem/BP HUD flashlight shares model-space light nodes:
// spotlight (0,-0.103966,0), ray (0,-0.203767,0). The Requiem headset trial
// confirms their alignment but disproves BP's model -Y -> socket -Z direction
// here. Turn the installed model toward socket +Z around its measured grip.
// Retain Requiem's prior visual size and grip radius; Rework's modified DAE
// translation cannot be reused for this installed model.
inline constexpr float kInstalledFlashlightScale = 1.6F;
inline constexpr float kInstalledFlashlightGripRadius = 0.022F;
inline constexpr runtime::VrAttachmentSocketProfile kInstalledFlashlightSocket{
    {1, 0, 0,
     0, 0, 1,
     0, -1, 0},
    {0, -0.016669F * kInstalledFlashlightScale, 0}};

[[nodiscard]] inline runtime::VrMatrix44 InstalledFlashlightToolPose(
    const runtime::VrMatrix44& hand_socket) noexcept {
    // Scale around the grip, not the model origin, so model and native child
    // lights retain one transform without moving the handle out of the hand.
    auto scale = runtime::IdentityMatrix();
    scale.values[0] = scale.values[5] = scale.values[10] =
        kInstalledFlashlightScale;
    return runtime::Multiply(runtime::ComposeAttachmentSocketPose(
        hand_socket, kInstalledFlashlightSocket), scale);
}

} // namespace penumbra_vr::backends::requiem
