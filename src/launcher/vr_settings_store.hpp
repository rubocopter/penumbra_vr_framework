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

// Loads the complete game-neutral VR settings schema shared with the Overture
// reference implementation. Missing keys retain normalized Rework defaults;
// malformed recognized keys fail closed.
[[nodiscard]] bool LoadVrSettings(
    const std::filesystem::path& path,
    runtime::VrSettings& settings,
    std::wstring& error);

[[nodiscard]] bool SaveVrSettings(
    const std::filesystem::path& path,
    const runtime::VrSettings& settings,
    std::wstring& error);

// Compatibility alias retained for existing binary-backend callers while the
// shared settings editor migrates them to the complete schema.
[[nodiscard]] bool LoadVrInputSettings(
    const std::filesystem::path& path,
    runtime::VrSettings& settings,
    std::wstring& error);

} // namespace penumbra_vr::launcher
