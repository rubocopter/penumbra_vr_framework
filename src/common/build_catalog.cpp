#include "penumbra_vr/build_catalog.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <cstdint>
#include <vector>

namespace penumbra_vr {
namespace {

constexpr std::array<KnownBuild, 6> kKnownBuilds{{
    {GameId::overture,
     "overture-retail-observed",
     "95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448",
     BuildVariant::observed,
     "95ACB863441A17E701AF2CD1B1EF301C55C1AC620269A167275580EB6954A448",
     false},
    {GameId::overture,
     "overture-vr-rework-v0.1.0",
     "A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71",
     BuildVariant::observed,
     "A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71",
     false},
    {GameId::black_plague,
     "black-plague-steam-observed",
     "FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF",
     BuildVariant::observed,
     "FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF",
     true},
    {GameId::black_plague,
     "black-plague-steam-observed",
     "DB086CC7A4C7B10864DE0FEBBE2D71A3E4EFF1EC8D067811A6A59EDC1C617196",
     BuildVariant::large_address_aware,
     "FD316F7586737A63EBA989ECE2271280FE6A98582A1319FE2151385A3DF97BFF",
     true},
    {GameId::requiem,
     "requiem-steam-observed",
     "B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2",
     BuildVariant::observed,
     "B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2",
     false},
    {GameId::requiem,
     "requiem-steam-observed",
     "577D1D7780872CD6C5B99B45759CDC48FEE486A1CCBF319E8F6CF0EAED54E955",
     BuildVariant::large_address_aware,
     "B64232D751CEE376E1384CFE5A4A81DBD7DEDDF03983CC11D0D0A34D5825EEA2",
     false},
}};

std::wstring NtStatusError(const wchar_t* operation, NTSTATUS status) {
    wchar_t buffer[128]{};
    swprintf_s(buffer, L"%s failed with NTSTATUS 0x%08lX", operation, static_cast<unsigned long>(status));
    return buffer;
}

} // namespace

bool ComputeFileSha256(const std::wstring& path, std::string& sha256, std::wstring& error) {
    sha256.clear();
    error.clear();

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    HANDLE file = INVALID_HANDLE_VALUE;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status < 0) {
        error = NtStatusError(L"BCryptOpenAlgorithmProvider", status);
        return false;
    }

    DWORD object_size = 0;
    DWORD result_size = 0;
    status = BCryptGetProperty(
        algorithm,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size),
        sizeof(object_size),
        &result_size,
        0);
    if (status < 0) {
        error = NtStatusError(L"BCryptGetProperty(BCRYPT_OBJECT_LENGTH)", status);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    DWORD digest_size = 0;
    status = BCryptGetProperty(
        algorithm,
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&digest_size),
        sizeof(digest_size),
        &result_size,
        0);
    if (status < 0) {
        error = NtStatusError(L"BCryptGetProperty(BCRYPT_HASH_LENGTH)", status);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    std::vector<std::uint8_t> hash_object(object_size);
    std::vector<std::uint8_t> digest(digest_size);
    status = BCryptCreateHash(
        algorithm,
        &hash,
        hash_object.data(),
        static_cast<ULONG>(hash_object.size()),
        nullptr,
        0,
        0);
    if (status < 0) {
        error = NtStatusError(L"BCryptCreateHash", status);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"CreateFileW failed with Win32 error " + std::to_wstring(GetLastError());
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return false;
    }

    std::array<std::uint8_t, 64 * 1024> buffer{};
    for (;;) {
        DWORD bytes_read = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr)) {
            error = L"ReadFile failed with Win32 error " + std::to_wstring(GetLastError());
            CloseHandle(file);
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }
        if (bytes_read == 0) {
            break;
        }

        status = BCryptHashData(hash, buffer.data(), bytes_read, 0);
        if (status < 0) {
            error = NtStatusError(L"BCryptHashData", status);
            CloseHandle(file);
            BCryptDestroyHash(hash);
            BCryptCloseAlgorithmProvider(algorithm, 0);
            return false;
        }
    }

    CloseHandle(file);
    status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) {
        error = NtStatusError(L"BCryptFinishHash", status);
        return false;
    }

    constexpr char kHex[] = "0123456789ABCDEF";
    sha256.reserve(digest.size() * 2);
    for (const std::uint8_t byte : digest) {
        sha256.push_back(kHex[byte >> 4]);
        sha256.push_back(kHex[byte & 0x0F]);
    }
    return true;
}

const KnownBuild* FindKnownBuild(std::string_view sha256) noexcept {
    for (const KnownBuild& build : kKnownBuilds) {
        if (build.sha256 == sha256) {
            return &build;
        }
    }
    return nullptr;
}

std::wstring_view GameDisplayName(GameId game) noexcept {
    switch (game) {
        case GameId::overture:
            return L"Penumbra: Overture";
        case GameId::black_plague:
            return L"Penumbra: Black Plague";
        case GameId::requiem:
            return L"Penumbra: Requiem";
        default:
            return L"Unknown game";
    }
}

} // namespace penumbra_vr
