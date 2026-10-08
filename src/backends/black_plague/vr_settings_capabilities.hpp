#pragma once

#include "vr_settings_editor.hpp"

#include <array>
#include <cstdint>

namespace penumbra_vr::backends::black_plague {

// Settings exposed by the Black Plague VR settings UI must reflect only
// behavior that the backend currently applies. Persisted-but-unwired settings
// remain hidden until their backend ownership is demonstrated and implemented.
[[nodiscard]] runtime::VrSettingCapabilities
BlackPlagueVrSettingCapabilities() noexcept;

inline constexpr std::size_t kBlackPlagueVrMenuSettingCount = 17;
inline constexpr int kBlackPlagueVrMenuRootSentinel = 0x70000000;

// A listed address may have been recycled for a new native widget. Retain a
// binding only when the live widget still has the injected root identity.
[[nodiscard]] bool BlackPlagueVrMenuRootBindingMatches(
    const void* bound_root, const void* live_widget,
    const void* widget_vtable, const void* native_button_vtable,
    int target_state) noexcept;

// Stable native-menu order. Keeping the surface here makes both the injected
// menu and host tests consume the exact backend-owned capability list.
[[nodiscard]] const std::array<runtime::VrSettingId,
    kBlackPlagueVrMenuSettingCount>& BlackPlagueVrMenuSettings() noexcept;

// Applies the same raw standing-height plausibility gate used by Overture,
// then normalizes through the shared settings contract before persistence.
[[nodiscard]] bool TryCalibratePlayerHeight(
    runtime::VrSettings& settings,
    float tracked_height) noexcept;

// The main menu and pause menu can be distinct native cMainMenu instances.
// Rebind the injected page whenever the root menu is entered through another owner.
[[nodiscard]] bool BlackPlagueVrMenuNeedsRebind(
    bool has_bound_root,
    bool same_menu) noexcept;

// The exact Black Plague cMainMenu::SetState boundary is the authoritative
// lifecycle signal for front-end and pause menus. The VR entry is in the
// first menu level (exact-build Start state 0).
[[nodiscard]] bool BlackPlagueVrMenuNeedsRebindOnStateTransition(
    int next_state,
    bool has_bound_root,
    bool same_menu) noexcept;

// Height calibration uses the last freshly published HMD tracking-space pose,
// including poses sampled during native full-screen menus. It must not reuse
// a world-only sample invalidated on entry to Options or a stale session pose.
inline constexpr std::uint64_t kBlackPlagueCalibrationMaxPoseAgeMs = 500;
[[nodiscard]] bool BlackPlagueCalibrationPoseUsable(
    bool has_sample,
    float tracked_height,
    std::uint64_t age_ms) noexcept;

} // namespace penumbra_vr::backends::black_plague
