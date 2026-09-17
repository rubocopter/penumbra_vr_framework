#include "openal_soft_config.hpp"

#include "spatial_audio.hpp"

#include <fstream>
#include <iterator>
#include <string>

namespace penumbra_vr::launcher {
namespace {

[[nodiscard]] audio::HrtfMode ToAudioMode(runtime::VrHrtfMode mode) noexcept {
    switch (mode) {
    case runtime::VrHrtfMode::on:
        return audio::HrtfMode::on;
    case runtime::VrHrtfMode::off:
        return audio::HrtfMode::off;
    case runtime::VrHrtfMode::automatic:
    default:
        return audio::HrtfMode::automatic;
    }
}

[[nodiscard]] std::string ExpectedConfig(runtime::VrHrtfMode mode) {
    return audio::BuildOpenAlSoftConfig(ToAudioMode(mode));
}

} // namespace

std::filesystem::path OpenAlSoftConfigPathForGame(
    const std::filesystem::path& game_executable) {
    return game_executable.parent_path() / L"alsoft.ini";
}

bool WriteOpenAlSoftHrtfConfig(
    const std::filesystem::path& game_executable,
    runtime::VrHrtfMode mode,
    std::wstring& error) {
    error.clear();
    const auto path = OpenAlSoftConfigPathForGame(game_executable);
    if (path.parent_path().empty()) {
        error = L"Cannot resolve the Black Plague directory for OpenAL Soft configuration";
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = L"Cannot write OpenAL Soft HRTF configuration: " + path.wstring();
        return false;
    }

    const std::string config = ExpectedConfig(mode);
    output.write(config.data(), static_cast<std::streamsize>(config.size()));
    output.close();
    if (!output) {
        error = L"Failed while writing OpenAL Soft HRTF configuration: " + path.wstring();
        return false;
    }
    return true;
}

bool OpenAlSoftHrtfConfigMatches(
    const std::filesystem::path& game_executable,
    runtime::VrHrtfMode mode,
    bool& matches,
    std::wstring& error) {
    matches = false;
    error.clear();
    const auto path = OpenAlSoftConfigPathForGame(game_executable);

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) {
            error = L"Cannot inspect OpenAL Soft HRTF configuration: " + path.wstring();
            return false;
        }
        return true;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = L"Cannot read OpenAL Soft HRTF configuration: " + path.wstring();
        return false;
    }
    const std::string actual{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (!input.eof() && input.fail()) {
        error = L"Failed while reading OpenAL Soft HRTF configuration: " + path.wstring();
        return false;
    }
    matches = actual == ExpectedConfig(mode);
    return true;
}

} // namespace penumbra_vr::launcher
