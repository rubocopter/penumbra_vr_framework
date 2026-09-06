#include "vr_settings_store.hpp"

#include "vr_settings.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string_view>
#include <vector>

namespace penumbra_vr::launcher {
namespace {

[[nodiscard]] bool EqualsCaseInsensitive(
    std::wstring_view left,
    std::wstring_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        wchar_t left_character = left[index];
        wchar_t right_character = right[index];
        if (left_character >= L'A' && left_character <= L'Z') {
            left_character = static_cast<wchar_t>(left_character - L'A' + L'a');
        }
        if (right_character >= L'A' && right_character <= L'Z') {
            right_character = static_cast<wchar_t>(right_character - L'A' + L'a');
        }
        if (left_character != right_character) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::wstring Win32Error(
    std::wstring_view operation,
    DWORD error_code) {
    return std::wstring(operation) + L" failed with Win32 error " +
        std::to_wstring(error_code);
}

} // namespace

std::filesystem::path DefaultVrSettingsPath(std::wstring& error) {
    error.clear();
    const DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (required == 0) {
        error = L"LOCALAPPDATA is unavailable";
        return {};
    }

    std::vector<wchar_t> buffer(required);
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        error = L"LOCALAPPDATA changed while it was being read";
        return {};
    }

    return std::filesystem::path(buffer.data()) /
        L"PenumbraVR" / L"settings.ini";
}

bool LoadMonitorMirrorSetting(
    const std::filesystem::path& path,
    bool& enabled,
    std::wstring& error) {
    enabled = runtime::VrSettings{}.monitor_mirror;
    error.clear();
    if (path.empty()) {
        error = L"The VR settings path is empty";
        return false;
    }

    std::error_code filesystem_error;
    const bool exists = std::filesystem::exists(path, filesystem_error);
    if (filesystem_error) {
        const std::string message = filesystem_error.message();
        error = L"Could not inspect the VR settings file: " +
            std::wstring(message.begin(), message.end());
        return false;
    }
    if (!exists) {
        return true;
    }
    if (!std::filesystem::is_regular_file(path, filesystem_error) ||
        filesystem_error) {
        error = L"The VR settings path is not a regular file";
        return false;
    }

    wchar_t value[32]{};
    const DWORD length = GetPrivateProfileStringW(
        L"VR",
        L"MonitorMirror",
        L"",
        value,
        static_cast<DWORD>(sizeof(value) / sizeof(value[0])),
        path.c_str());
    if (length == 0) {
        return true;
    }

    const std::wstring_view text(value, length);
    if (EqualsCaseInsensitive(text, L"true") || text == L"1" ||
        EqualsCaseInsensitive(text, L"on") ||
        EqualsCaseInsensitive(text, L"yes")) {
        enabled = true;
        return true;
    }
    if (EqualsCaseInsensitive(text, L"false") || text == L"0" ||
        EqualsCaseInsensitive(text, L"off") ||
        EqualsCaseInsensitive(text, L"no")) {
        enabled = false;
        return true;
    }

    error = L"VR/MonitorMirror must be true or false in " + path.wstring();
    return false;
}

bool SaveMonitorMirrorSetting(
    const std::filesystem::path& path,
    bool enabled,
    std::wstring& error) {
    error.clear();
    if (path.empty() || path.parent_path().empty()) {
        error = L"The VR settings path has no parent directory";
        return false;
    }

    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        const std::string message = filesystem_error.message();
        error = L"Could not create the VR settings directory: " +
            std::wstring(message.begin(), message.end());
        return false;
    }

    SetLastError(ERROR_SUCCESS);
    if (!WritePrivateProfileStringW(
            L"VR",
            L"MonitorMirror",
            enabled ? L"true" : L"false",
            path.c_str())) {
        error = Win32Error(L"WritePrivateProfileStringW", GetLastError());
        return false;
    }
    return true;
}

} // namespace penumbra_vr::launcher
