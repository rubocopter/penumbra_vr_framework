#pragma once

#include "vr_settings.hpp"

#include <string>

namespace penumbra_vr::backends::black_plague {

using CommitNativeVrSettings = bool (*)(
    const runtime::VrSettings& settings,
    std::string& error) noexcept;

// Settings storage is owned by the probe. The native page edits that same
// profile and asks the probe to persist/apply each accepted step.
void ConfigureNativeVrSettingsMenu(
    runtime::VrSettings* settings,
    CommitNativeVrSettings commit) noexcept;

[[nodiscard]] bool InstallNativeVrSettingsMenu(std::string& error) noexcept;
[[nodiscard]] bool RemoveNativeVrSettingsMenu(std::string& error) noexcept;

} // namespace penumbra_vr::backends::black_plague
