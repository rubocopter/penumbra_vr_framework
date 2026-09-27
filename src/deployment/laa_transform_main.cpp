#include "pe_large_address.hpp"

#include "penumbra_vr/build_catalog.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 3) {
        std::wcerr << L"Usage: PenumbraVR.LaaTransform.exe <canonical.exe> <new-output.exe>\n";
        return 2;
    }
    const std::wstring source = argv[1];
    const std::wstring output = argv[2];
    std::string source_hash;
    std::wstring hash_error;
    if (!penumbra_vr::ComputeFileSha256(source, source_hash, hash_error)) {
        std::wcerr << hash_error << L'\n';
        return 3;
    }
    const auto* build = penumbra_vr::FindKnownBuild(source_hash);
    if (build == nullptr || build->variant != penumbra_vr::BuildVariant::observed ||
        (build->game != penumbra_vr::GameId::black_plague &&
         build->game != penumbra_vr::GameId::requiem)) {
        std::wcerr << L"Only canonical allowlisted Black Plague/Requiem images may be transformed.\n";
        return 4;
    }
    std::ifstream input(std::filesystem::path(source), std::ios::binary);
    if (!input) {
        std::wcerr << L"Could not read the canonical image.\n";
        return 5;
    }
    std::vector<std::uint8_t> bytes(
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    if (!input.eof() && input.fail()) {
        std::wcerr << L"Reading the canonical image failed.\n";
        return 5;
    }
    std::string transform_error;
    bool changed = false;
    if (!penumbra_vr::deployment::EnablePeLargeAddressAware(
            bytes, changed, transform_error) || !changed) {
        std::cerr << "LAA transform rejected the input: " << transform_error << '\n';
        return 6;
    }

    HANDLE file = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Output must be a new file (Win32 error " << GetLastError() << L").\n";
        return 7;
    }
    bool wrote = true;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - offset, 1024U * 1024U));
        DWORD actual = 0;
        if (!WriteFile(file, bytes.data() + offset, count, &actual, nullptr) ||
            actual != count) {
            wrote = false;
            break;
        }
        offset += actual;
    }
    if (!CloseHandle(file)) wrote = false;
    if (!wrote) {
        DeleteFileW(output.c_str());
        std::wcerr << L"Writing the transformed image failed.\n";
        return 8;
    }

    std::string output_hash;
    if (!penumbra_vr::ComputeFileSha256(output, output_hash, hash_error)) {
        DeleteFileW(output.c_str());
        std::wcerr << L"Could not hash the transformed image: " << hash_error << L'\n';
        return 9;
    }
    const auto* transformed = penumbra_vr::FindKnownBuild(output_hash);
    if (transformed == nullptr ||
        transformed->variant != penumbra_vr::BuildVariant::large_address_aware ||
        transformed->game != build->game ||
        transformed->canonical_sha256 != build->sha256) {
        DeleteFileW(output.c_str());
        std::wcerr << L"Transformed image hash is not the recorded exact variant.\n";
        return 10;
    }
    std::cout << output_hash << '\n';
    return 0;
}
