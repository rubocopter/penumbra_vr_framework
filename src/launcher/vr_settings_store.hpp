#pragma once
#include "vr_settings.hpp"

#include <filesystem>
#include <string>

namespace penumbra_vr::launcher {

[[nodiscard]] std::filesystem::path DefaultVrSettingsPath(
    std::wstring& error);

[[nodiscard]] bool LoadMonitorMirrorSetting(
    const std::filesystem::path& path,
    bool& enabled,
    std::wstring& error);

[[nodiscard]] bool SaveMonitorMirrorSetting(
    const std::filesystem::path& path,
    bool enabled,
    std::wstring& error);

// Loads the input, menu and render subset currently consumed by the binary backends.
// Missing keys retain normalized Rework defaults; malformed keys fail closed.
[[nodiscard]] bool LoadVrInputSettings(
    const std::filesystem::path& path,
    runtime::VrSettings& settings,
    std::wstring& error);

} // namespace penumbra_vr::launcher
