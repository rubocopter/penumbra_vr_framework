#include "openal_soft_config.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

struct TempDirectory {
    std::filesystem::path path;
    TempDirectory() = default;
    explicit TempDirectory(std::filesystem::path value) : path(std::move(value)) {}
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&& other) noexcept : path(std::move(other.path)) {
        other.path.clear();
    }
    TempDirectory& operator=(TempDirectory&&) = delete;
    ~TempDirectory() {
        if (path.empty()) return;
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

[[nodiscard]] TempDirectory MakeTempDirectory() {
    const auto stamp = std::chrono::high_resolution_clock::now()
        .time_since_epoch().count();
    TempDirectory result{
        std::filesystem::temp_directory_path() /
        (L"penumbra-vr-openal-test-" + std::to_wstring(stamp))};
    std::filesystem::create_directories(result.path);
    return result;
}

[[nodiscard]] std::string ReadAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

int main() {
    using penumbra_vr::launcher::OpenAlSoftConfigPathForGame;
    using penumbra_vr::launcher::OpenAlSoftHrtfConfigMatches;
    using penumbra_vr::launcher::WriteOpenAlSoftHrtfConfig;
    using penumbra_vr::runtime::VrHrtfMode;

    auto temp = MakeTempDirectory();
    const auto game = temp.path / L"Penumbra.exe";
    {
        std::ofstream fake_game(game, std::ios::binary);
        fake_game << "test";
    }

    std::wstring error;
    if (!WriteOpenAlSoftHrtfConfig(game, VrHrtfMode::on, error)) {
        std::wcerr << error << L'\n';
        return 1;
    }
    const auto config = OpenAlSoftConfigPathForGame(game);
    if (ReadAll(config) != "[general]\nhrtf = true\n") return 2;

    bool matches = false;
    if (!OpenAlSoftHrtfConfigMatches(game, VrHrtfMode::on, matches, error) ||
        !matches) return 3;
    if (!OpenAlSoftHrtfConfigMatches(game, VrHrtfMode::off, matches, error) ||
        matches) return 4;

    if (!WriteOpenAlSoftHrtfConfig(game, VrHrtfMode::automatic, error) ||
        ReadAll(config) != "[general]\nhrtf = auto\n") return 5;

    std::cout << "OpenAL Soft HRTF launch configuration matches Rework semantics\n";
    return 0;
}
