#include "penumbra_vr/build_catalog.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

struct ExpectedBuild {
    std::string_view sha256;
    penumbra_vr::GameId game;
    std::string_view id;
    bool black_plague_probe_allowed;
};

} // namespace

int main() {
    constexpr ExpectedBuild kExpectedBuilds[]{
        {
            "95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448",
            penumbra_vr::GameId::overture,
            "overture-retail-observed",
            false,
        },
        {
            "A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71",
            penumbra_vr::GameId::overture,
            "overture-vr-rework-v0.1.0",
            false,
        },
        {
            "FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF",
            penumbra_vr::GameId::black_plague,
            "black-plague-steam-observed",
            true,
        },
        {
            "B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2",
            penumbra_vr::GameId::requiem,
            "requiem-steam-observed",
            false,
        },
    };

    for (const ExpectedBuild& expected : kExpectedBuilds) {
        const penumbra_vr::KnownBuild* build =
            penumbra_vr::FindKnownBuild(expected.sha256);
        if (build == nullptr || build->game != expected.game ||
            build->id != expected.id ||
            build->black_plague_probe_allowed !=
                expected.black_plague_probe_allowed) {
            std::cerr << "Known-build catalogue entry does not match "
                      << expected.id << '\n';
            return 1;
        }
    }

    if (penumbra_vr::FindKnownBuild(
            "fd316f7586737a63eba989ece2271280fe6a98582a1319fe2151385a3df97bff") !=
            nullptr ||
        penumbra_vr::FindKnownBuild(
            "0000000000000000000000000000000000000000000000000000000000000000") !=
            nullptr) {
        std::cerr << "Unknown or non-canonical hashes did not fail closed\n";
        return 2;
    }

    if (penumbra_vr::GameDisplayName(penumbra_vr::GameId::overture) !=
            L"Penumbra: Overture" ||
        penumbra_vr::GameDisplayName(penumbra_vr::GameId::black_plague) !=
            L"Penumbra: Black Plague" ||
        penumbra_vr::GameDisplayName(penumbra_vr::GameId::requiem) !=
            L"Penumbra: Requiem" ||
        penumbra_vr::GameDisplayName(penumbra_vr::GameId::unknown) !=
            L"Unknown game") {
        std::cerr << "Game display-name mapping is inconsistent\n";
        return 3;
    }

    const std::filesystem::path fixture =
        std::filesystem::current_path() / "pvr_build_catalog_sha256_fixture.txt";
    {
        std::ofstream output(fixture, std::ios::binary | std::ios::trunc);
        output << "abc";
        if (!output) {
            std::cerr << "Could not create SHA-256 test fixture\n";
            return 4;
        }
    }

    std::string sha256;
    std::wstring error;
    const bool hash_ok =
        penumbra_vr::ComputeFileSha256(fixture.wstring(), sha256, error);
    std::error_code remove_error;
    std::filesystem::remove(fixture, remove_error);
    if (!hash_ok ||
        sha256 !=
            "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD" ||
        !error.empty()) {
        std::wcerr << L"SHA-256 calculation failed: " << error << L'\n';
        return 5;
    }

    if (penumbra_vr::ComputeFileSha256(
            L"Z:\\path-that-must-not-exist\\Penumbra.exe", sha256, error) ||
        error.empty()) {
        std::cerr << "Missing executable did not fail closed\n";
        return 6;
    }

    std::cout << "Known-build catalogue and SHA-256 checks passed\n";
    return 0;
}
