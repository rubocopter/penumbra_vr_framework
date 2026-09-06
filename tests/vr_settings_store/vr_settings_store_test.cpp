#include "vr_settings_store.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

class TemporaryDirectory final {
public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
            (L"PenumbraVR-settings-store-test-" +
             std::to_wstring(GetCurrentProcessId()) + L"-" +
             std::to_wstring(GetTickCount64()));
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

} // namespace

int main() {
    TemporaryDirectory temporary;
    const std::filesystem::path settings_path =
        temporary.path() / L"nested" / L"settings.ini";
    std::wstring error;
    bool enabled = true;

    if (!penumbra_vr::launcher::LoadMonitorMirrorSetting(
            settings_path, enabled, error) || enabled || !error.empty()) {
        std::cerr << "A missing settings file did not use the safe default\n";
        return 1;
    }
    if (!penumbra_vr::launcher::SaveMonitorMirrorSetting(
            settings_path, true, error) || !error.empty()) {
        std::wcerr << L"Could not persist the enabled mirror: " << error << L'\n';
        return 2;
    }
    enabled = false;
    if (!penumbra_vr::launcher::LoadMonitorMirrorSetting(
            settings_path, enabled, error) || !enabled || !error.empty()) {
        std::cerr << "The enabled mirror did not round trip\n";
        return 3;
    }
    if (!penumbra_vr::launcher::SaveMonitorMirrorSetting(
            settings_path, false, error) || !error.empty()) {
        std::wcerr << L"Could not persist the disabled mirror: " << error << L'\n';
        return 4;
    }
    enabled = true;
    if (!penumbra_vr::launcher::LoadMonitorMirrorSetting(
            settings_path, enabled, error) || enabled || !error.empty()) {
        std::cerr << "The disabled mirror did not round trip\n";
        return 5;
    }

    if (!WritePrivateProfileStringW(
            L"VR", L"MonitorMirror", L"invalid", settings_path.c_str())) {
        std::cerr << "Could not create the invalid-value fixture\n";
        return 6;
    }
    if (penumbra_vr::launcher::LoadMonitorMirrorSetting(
            settings_path, enabled, error) || error.empty()) {
        std::cerr << "An invalid mirror value was accepted\n";
        return 7;
    }
    if (!WritePrivateProfileStringW(L"VR",L"MonitorMirror",L"true",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"MoveSpeed",L"0.85",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"MoveDeadZone",L"0.20",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"TurnMode",L"Smooth",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"SmoothTurnSpeed",L"75",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"TurnDeadZone",L"0.25",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"UiDistance",L"2.25",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"UiScale",L"1.20",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"RenderScale",L"0.80",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"Handedness",L"Left",settings_path.c_str())) return 9;
    penumbra_vr::runtime::VrSettings settings;
    if (!penumbra_vr::launcher::LoadVrInputSettings(settings_path,settings,error) ||
        !settings.monitor_mirror || settings.move_speed!=0.85F || settings.move_dead_zone!=0.20F ||
        settings.turn_mode!=penumbra_vr::runtime::VrTurnMode::smooth ||
        settings.smooth_turn_speed!=75 || settings.turn_dead_zone!=0.25F ||
        settings.ui_distance!=2.25F || settings.ui_scale!=1.20F ||
        settings.render_scale!=0.80F ||
        settings.handedness!=penumbra_vr::runtime::VrHandedness::left) return 10;
    if (!WritePrivateProfileStringW(L"VR",L"Handedness",L"ambidextrous",settings_path.c_str()) ||
        penumbra_vr::launcher::LoadVrInputSettings(settings_path,settings,error) || error.empty()) return 11;

    std::wstring path_error;
    const std::filesystem::path default_path =
        penumbra_vr::launcher::DefaultVrSettingsPath(path_error);
    if (default_path.empty() || !path_error.empty() ||
        default_path.filename() != L"settings.ini" ||
        default_path.parent_path().filename() != L"PenumbraVR") {
        std::wcerr << L"The default settings path is invalid: " << path_error << L'\n';
        return 8;
    }

    std::cout << "Persistent monitor-mirror settings passed\n";
    return 0;
}
