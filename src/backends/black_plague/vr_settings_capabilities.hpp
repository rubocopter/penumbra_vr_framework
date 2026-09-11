#pragma once

#include "vr_settings_editor.hpp"

namespace penumbra_vr::backends::black_plague {

// Settings exposed by a future Black Plague VR settings UI must reflect only
// behavior that the backend currently applies. Persisted-but-unwired settings
// remain hidden until their backend ownership is demonstrated and implemented.
[[nodiscard]] runtime::VrSettingCapabilities
BlackPlagueVrSettingCapabilities() noexcept;

} // namespace penumbra_vr::backends::black_plague
