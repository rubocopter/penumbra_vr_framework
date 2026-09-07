#pragma once

namespace penumbra_vr::runtime::vr_interaction_policy {

// Rework permits collision-constrained palms to follow the raw controller by
// at most this distance when resolving a physical interaction target.
inline constexpr float kMaximumCollisionInteractionReach = 0.18F;

// Long-range assistance is a separate, item-only path. Backends must not use
// this value for ordinary props or mechanisms.
inline constexpr float kMagneticItemRange = 2.35F;

[[nodiscard]] float ClampPhysicalInteractionReach(
    float native_reach) noexcept;

} // namespace penumbra_vr::runtime::vr_interaction_policy
