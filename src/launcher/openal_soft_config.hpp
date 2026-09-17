#pragma once

#include "vr_settings.hpp"

#include <filesystem>
#include <string>

namespace penumbra_vr::launcher {

[[nodiscard]] std::filesystem::path OpenAlSoftConfigPathForGame(
    const std::filesystem::path& game_executable);

[[nodiscard]] bool WriteOpenAlSoftHrtfConfig(
    const std::filesystem::path& game_executable,
    runtime::VrHrtfMode mode,
    std::wstring& error);

[[nodiscard]] bool OpenAlSoftHrtfConfigMatches(
    const std::filesystem::path& game_executable,
    runtime::VrHrtfMode mode,
    bool& matches,
    std::wstring& error);

} // namespace penumbra_vr::launcher
