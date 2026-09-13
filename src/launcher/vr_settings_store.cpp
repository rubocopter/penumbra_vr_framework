#include "vr_settings_store.hpp"

#include "vr_settings.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string_view>
#include <vector>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <limits>

namespace penumbra_vr::launcher {
namespace {

std::atomic<std::uint64_t> g_settings_transaction_sequence{0};

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

[[nodiscard]] std::wstring ReadValue(const std::filesystem::path& path,
    const wchar_t* key) {
    wchar_t value[64]{};
    const DWORD length = GetPrivateProfileStringW(
        L"VR", key, L"", value,
        static_cast<DWORD>(std::size(value)), path.c_str());
    return std::wstring(value, length);
}

[[nodiscard]] bool ReadFloat(
    const std::filesystem::path& path,
    const wchar_t* key,
    float& value,
    std::wstring& error) {
    const auto text = ReadValue(path, key);
    if (text.empty()) {
        return true;
    }
    wchar_t* end = nullptr;
    errno = 0;
    const float parsed = std::wcstof(text.c_str(), &end);
    if (errno == ERANGE || end != text.c_str() + text.size() ||
        !std::isfinite(parsed)) {
        error = std::wstring(L"VR/") + key +
            L" must be a finite number in " + path.wstring();
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool ParseBool(
    std::wstring_view text,
    bool& value) noexcept {
    if (EqualsCaseInsensitive(text, L"true") || text == L"1" ||
        EqualsCaseInsensitive(text, L"on") ||
        EqualsCaseInsensitive(text, L"yes")) {
        value = true;
        return true;
    }
    if (EqualsCaseInsensitive(text, L"false") || text == L"0" ||
        EqualsCaseInsensitive(text, L"off") ||
        EqualsCaseInsensitive(text, L"no")) {
        value = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool ReadBool(
    const std::filesystem::path& path,
    const wchar_t* key,
    bool& value,
    std::wstring& error) {
    const auto text = ReadValue(path, key);
    if (text.empty()) {
        return true;
    }
    if (ParseBool(text, value)) {
        return true;
    }
    error = std::wstring(L"VR/") + key +
        L" must be true or false in " + path.wstring();
    return false;
}

[[nodiscard]] bool ReadVersion(
    const std::filesystem::path& path,
    std::uint32_t& version,
    std::wstring& error) {
    const auto text = ReadValue(path, L"SettingsVersion");
    if (text.empty()) {
        version = 0;
        return true;
    }
    if (text.front() == L'+' || text.front() == L'-') {
        error = L"VR/SettingsVersion must be a non-negative integer in " +
            path.wstring();
        return false;
    }
    wchar_t* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::wcstoul(text.c_str(), &end, 10);
    if (errno == ERANGE || end != text.c_str() + text.size() ||
        parsed > (std::numeric_limits<std::uint32_t>::max)()) {
        error = L"VR/SettingsVersion must be a non-negative integer in " +
            path.wstring();
        return false;
    }
    version = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool WriteValue(
    const std::filesystem::path& path,
    const wchar_t* key,
    const std::wstring& value,
    std::wstring& error) {
    SetLastError(ERROR_SUCCESS);
    if (!WritePrivateProfileStringW(L"VR", key, value.c_str(), path.c_str())) {
        error = Win32Error(L"WritePrivateProfileStringW", GetLastError());
        return false;
    }
    return true;
}

void DiscardSettingsTransaction(const std::filesystem::path& path) noexcept {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

[[nodiscard]] bool BeginSettingsTransaction(
    const std::filesystem::path& path,
    std::filesystem::path& transaction_path,
    std::wstring& error) {
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

    const std::uint64_t sequence =
        g_settings_transaction_sequence.fetch_add(1, std::memory_order_relaxed);
    transaction_path = path.parent_path() /
        (path.filename().wstring() + L".tmp." +
            std::to_wstring(GetCurrentProcessId()) + L"." +
            std::to_wstring(GetCurrentThreadId()) + L"." +
            std::to_wstring(GetTickCount64()) + L"." +
            std::to_wstring(sequence));
    DiscardSettingsTransaction(transaction_path);

    const bool exists = std::filesystem::exists(path, filesystem_error);
    if (filesystem_error) {
        const std::string message = filesystem_error.message();
        error = L"Could not inspect the VR settings file: " +
            std::wstring(message.begin(), message.end());
        return false;
    }
    if (exists) {
        std::filesystem::copy_file(path, transaction_path,
            std::filesystem::copy_options::none, filesystem_error);
        if (filesystem_error) {
            const std::string message = filesystem_error.message();
            error = L"Could not prepare the VR settings transaction: " +
                std::wstring(message.begin(), message.end());
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool CommitSettingsTransaction(
    const std::filesystem::path& path,
    const std::filesystem::path& transaction_path,
    std::wstring& error) {
    // Win32 documents zero as the expected return value when this special
    // all-null call flushes the private-profile cache. It is therefore not a
    // success/failure result and GetLastError must not be interpreted here.
    static_cast<void>(WritePrivateProfileStringW(
        nullptr, nullptr, nullptr, transaction_path.c_str()));

    HANDLE transaction = CreateFileW(
        transaction_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (transaction == INVALID_HANDLE_VALUE) {
        error = Win32Error(
            L"Opening the VR settings transaction for durable flush",
            GetLastError());
        return false;
    }
    if (!FlushFileBuffers(transaction)) {
        const DWORD flush_error = GetLastError();
        CloseHandle(transaction);
        error = Win32Error(
            L"Flushing the VR settings transaction file", flush_error);
        return false;
    }
    if (!CloseHandle(transaction)) {
        error = Win32Error(
            L"Closing the flushed VR settings transaction", GetLastError());
        return false;
    }

    if (ReplaceFileW(path.c_str(), transaction_path.c_str(), nullptr,
            REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
        return true;
    }
    const DWORD replace_error = GetLastError();
    if (replace_error != ERROR_FILE_NOT_FOUND &&
        replace_error != ERROR_PATH_NOT_FOUND) {
        error = Win32Error(L"ReplaceFileW", replace_error);
        return false;
    }
    if (!MoveFileExW(transaction_path.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = Win32Error(L"MoveFileExW", GetLastError());
        return false;
    }
    return true;
}

[[nodiscard]] std::wstring FloatText(float value) {
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%.9g", static_cast<double>(value));
    return buffer;
}

[[nodiscard]] std::wstring ConfigText(std::string_view value) {
    return std::wstring(value.begin(), value.end());
}

[[nodiscard]] bool ValidateSettingsPath(
    const std::filesystem::path& path,
    bool allow_missing,
    std::wstring& error) {
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
        return allow_missing;
    }
    if (!std::filesystem::is_regular_file(path, filesystem_error) ||
        filesystem_error) {
        error = L"The VR settings path is not a regular file";
        return false;
    }
    return true;
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
    if (!ValidateSettingsPath(path, true, error)) {
        return false;
    }
    if (!std::filesystem::exists(path)) {
        return true;
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
    if (ParseBool(text, enabled)) {
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
    std::filesystem::path transaction_path;
    if (!BeginSettingsTransaction(path, transaction_path, error)) {
        return false;
    }
    if (!WriteValue(transaction_path, L"MonitorMirror",
            enabled ? L"true" : L"false", error) ||
        !CommitSettingsTransaction(path, transaction_path, error)) {
        DiscardSettingsTransaction(transaction_path);
        return false;
    }
    return true;
}

bool LoadVrSettings(
    const std::filesystem::path& path,
    runtime::VrSettings& settings,
    std::wstring& error) {
    settings = runtime::VrSettings{};
    error.clear();
    if (!ValidateSettingsPath(path, true, error)) {
        return false;
    }
    if (!std::filesystem::exists(path)) {
        return true;
    }
    std::uint32_t source_version = 0;
    if (!ReadVersion(path, source_version, error)) {
        return false;
    }
    if (!ReadFloat(path, L"MoveSpeed", settings.move_speed, error) ||
        !ReadFloat(path, L"MoveDeadZone", settings.move_dead_zone, error) ||
        !ReadFloat(path, L"HeightOffset", settings.height_offset, error) ||
        !ReadFloat(path, L"SnapTurnAngle", settings.snap_turn_angle, error) ||
        !ReadFloat(path, L"SmoothTurnSpeed", settings.smooth_turn_speed, error) ||
        !ReadFloat(path, L"TurnDeadZone", settings.turn_dead_zone, error) ||
        !ReadFloat(path, L"UiDistance", settings.ui_distance, error) ||
        !ReadFloat(path, L"UiScale", settings.ui_scale, error) ||
        !ReadFloat(path, L"RenderScale", settings.render_scale, error) ||
        !ReadBool(path, L"EnhancedVisuals", settings.enhanced_visuals, error) ||
        !ReadBool(path, L"MonitorMirror", settings.monitor_mirror, error) ||
        !ReadFloat(path, L"PhysicalCrouchDepth", settings.physical_crouch_depth, error) ||
        !ReadFloat(path, L"SubtitleScale", settings.subtitle_scale, error) ||
        !ReadFloat(path, L"PlayerHeight", settings.player_height, error)) {
        return false;
    }
    const auto turn = ReadValue(path, L"TurnMode");
    if (!turn.empty()) {
        if (EqualsCaseInsensitive(turn, L"disabled")) {
            settings.turn_mode = runtime::VrTurnMode::disabled;
        } else if (EqualsCaseInsensitive(turn, L"snap")) {
            settings.turn_mode = runtime::VrTurnMode::snap;
        } else if (EqualsCaseInsensitive(turn, L"smooth")) {
            settings.turn_mode = runtime::VrTurnMode::smooth;
        } else {
            error = L"VR/TurnMode must be Disabled, Snap or Smooth in " +
                path.wstring();
            return false;
        }
    }
    const auto hand = ReadValue(path, L"Handedness");
    if (!hand.empty()) {
        if (EqualsCaseInsensitive(hand, L"left")) {
            settings.handedness = runtime::VrHandedness::left;
        } else if (EqualsCaseInsensitive(hand, L"right")) {
            settings.handedness = runtime::VrHandedness::right;
        } else {
            error = L"VR/Handedness must be Left or Right in " + path.wstring();
            return false;
        }
    }
    const auto crouch = ReadValue(path, L"CrouchMode");
    if (!crouch.empty()) {
        if (EqualsCaseInsensitive(crouch, L"physical")) {
            settings.crouch_mode = runtime::VrCrouchMode::physical;
        } else if (EqualsCaseInsensitive(crouch, L"button")) {
            settings.crouch_mode = runtime::VrCrouchMode::button;
        } else if (EqualsCaseInsensitive(crouch, L"hybrid")) {
            settings.crouch_mode = runtime::VrCrouchMode::hybrid;
        } else {
            error = L"VR/CrouchMode must be Physical, Button or Hybrid in " +
                path.wstring();
            return false;
        }
    }
    const auto play = ReadValue(path, L"PlayMode");
    if (!play.empty()) {
        if (EqualsCaseInsensitive(play, L"standing")) {
            settings.play_mode = runtime::VrPlayMode::standing;
        } else if (EqualsCaseInsensitive(play, L"seated")) {
            settings.play_mode = runtime::VrPlayMode::seated;
        } else {
            error = L"VR/PlayMode must be Standing or Seated in " + path.wstring();
            return false;
        }
    }
    const auto hrtf = ReadValue(path, L"HRTF");
    if (!hrtf.empty()) {
        if (EqualsCaseInsensitive(hrtf, L"auto")) {
            settings.hrtf_mode = runtime::VrHrtfMode::automatic;
        } else if (EqualsCaseInsensitive(hrtf, L"on")) {
            settings.hrtf_mode = runtime::VrHrtfMode::on;
        } else if (EqualsCaseInsensitive(hrtf, L"off")) {
            settings.hrtf_mode = runtime::VrHrtfMode::off;
        } else {
            error = L"VR/HRTF must be Auto, On or Off in " + path.wstring();
            return false;
        }
    }
    runtime::NormalizeVrSettings(settings, source_version);
    return true;
}

bool SaveVrSettings(
    const std::filesystem::path& path,
    const runtime::VrSettings& source,
    std::wstring& error) {
    error.clear();
    std::filesystem::path transaction_path;
    if (!BeginSettingsTransaction(path, transaction_path, error)) {
        return false;
    }

    runtime::VrSettings settings = source;
    runtime::NormalizeVrSettings(settings);
    const auto write_float = [&](const wchar_t* key, float value) {
        return WriteValue(transaction_path, key, FloatText(value), error);
    };
    const auto write_bool = [&](const wchar_t* key, bool value) {
        return WriteValue(transaction_path, key, value ? L"true" : L"false", error);
    };

    const bool written = write_float(L"MoveSpeed", settings.move_speed) &&
        write_float(L"MoveDeadZone", settings.move_dead_zone) &&
        write_float(L"HeightOffset", settings.height_offset) &&
        WriteValue(transaction_path, L"TurnMode", ConfigText(runtime::ToConfigValue(settings.turn_mode)), error) &&
        write_float(L"SnapTurnAngle", settings.snap_turn_angle) &&
        write_float(L"SmoothTurnSpeed", settings.smooth_turn_speed) &&
        write_float(L"TurnDeadZone", settings.turn_dead_zone) &&
        write_float(L"UiDistance", settings.ui_distance) &&
        write_float(L"UiScale", settings.ui_scale) &&
        write_float(L"RenderScale", settings.render_scale) &&
        write_bool(L"EnhancedVisuals", settings.enhanced_visuals) &&
        write_bool(L"MonitorMirror", settings.monitor_mirror) &&
        WriteValue(transaction_path, L"CrouchMode", ConfigText(runtime::ToConfigValue(settings.crouch_mode)), error) &&
        write_float(L"PhysicalCrouchDepth", settings.physical_crouch_depth) &&
        write_float(L"SubtitleScale", settings.subtitle_scale) &&
        WriteValue(transaction_path, L"Handedness", ConfigText(runtime::ToConfigValue(settings.handedness)), error) &&
        WriteValue(transaction_path, L"PlayMode", ConfigText(runtime::ToConfigValue(settings.play_mode)), error) &&
        write_float(L"PlayerHeight", settings.player_height) &&
        WriteValue(transaction_path, L"HRTF", ConfigText(runtime::ToConfigValue(settings.hrtf_mode)), error) &&
        WriteValue(transaction_path, L"SettingsVersion",
            std::to_wstring(runtime::kCurrentVrSettingsVersion), error);
    if (!written || !CommitSettingsTransaction(path, transaction_path, error)) {
        DiscardSettingsTransaction(transaction_path);
        return false;
    }
    return true;
}

bool LoadVrInputSettings(
    const std::filesystem::path& path,
    runtime::VrSettings& settings,
    std::wstring& error) {
    return LoadVrSettings(path, settings, error);
}

} // namespace penumbra_vr::launcher
