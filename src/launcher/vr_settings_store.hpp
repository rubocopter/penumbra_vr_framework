#pragma once

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

} // namespace penumbra_vr::launcher
