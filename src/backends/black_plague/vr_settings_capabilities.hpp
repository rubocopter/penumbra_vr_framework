#pragma once

#include "vr_settings_editor.hpp"

#include <array>

namespace penumbra_vr::backends::black_plague {

// Settings exposed by the Black Plague VR settings UI must reflect only
// behavior that the backend currently applies. Persisted-but-unwired settings
// remain hidden until their backend ownership is demonstrated and implemented.
[[nodiscard]] runtime::VrSettingCapabilities
BlackPlagueVrSettingCapabilities() noexcept;

inline constexpr std::size_t kBlackPlagueVrMenuSettingCount = 17;

// Stable native-menu order. Keeping the surface here makes both the injected
// menu and host tests consume the exact backend-owned capability list.
[[nodiscard]] const std::array<runtime::VrSettingId,
    kBlackPlagueVrMenuSettingCount>& BlackPlagueVrMenuSettings() noexcept;

} // namespace penumbra_vr::backends::black_plague
