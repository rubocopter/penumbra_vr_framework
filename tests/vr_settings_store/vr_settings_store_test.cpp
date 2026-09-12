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
    penumbra_vr::runtime::VrSettings expected;
    expected.move_speed = 0.85F;
    expected.move_dead_zone = 0.20F;
    expected.height_offset = -0.15F;
    expected.turn_mode = penumbra_vr::runtime::VrTurnMode::smooth;
    expected.snap_turn_angle = 60.0F;
    expected.smooth_turn_speed = 75.0F;
    expected.turn_dead_zone = 0.25F;
    expected.ui_distance = 2.25F;
    expected.ui_scale = 1.20F;
    expected.render_scale = 0.80F;
    expected.enhanced_visuals = true;
    expected.monitor_mirror = true;
    expected.crouch_mode = penumbra_vr::runtime::VrCrouchMode::button;
    expected.physical_crouch_depth = 0.40F;
    expected.subtitle_scale = 1.30F;
    expected.handedness = penumbra_vr::runtime::VrHandedness::left;
    expected.play_mode = penumbra_vr::runtime::VrPlayMode::seated;
    expected.player_height = 1.85F;
    expected.hrtf_mode = penumbra_vr::runtime::VrHrtfMode::on;
    if (!WritePrivateProfileStringW(
            L"Unrelated", L"Preserved", L"yes", settings_path.c_str())) {
        std::cerr << "Could not create the unrelated-section fixture\n";
        return 9;
    }
    if (!penumbra_vr::launcher::SaveVrSettings(settings_path, expected, error) ||
        !error.empty()) {
        std::wcerr << L"Could not save the complete VR profile: " << error << L'\n';
        return 9;
    }
    penumbra_vr::runtime::VrSettings settings;
    if (!penumbra_vr::launcher::LoadVrSettings(settings_path,settings,error) ||
        !error.empty() || settings.move_speed!=expected.move_speed ||
        settings.move_dead_zone!=expected.move_dead_zone ||
        settings.height_offset!=expected.height_offset ||
        settings.turn_mode!=expected.turn_mode ||
        settings.snap_turn_angle!=expected.snap_turn_angle ||
        settings.smooth_turn_speed!=expected.smooth_turn_speed ||
        settings.turn_dead_zone!=expected.turn_dead_zone ||
        settings.ui_distance!=expected.ui_distance || settings.ui_scale!=expected.ui_scale ||
        settings.render_scale!=expected.render_scale ||
        settings.enhanced_visuals!=expected.enhanced_visuals ||
        settings.monitor_mirror!=expected.monitor_mirror ||
        settings.crouch_mode!=expected.crouch_mode ||
        settings.physical_crouch_depth!=expected.physical_crouch_depth ||
        settings.subtitle_scale!=expected.subtitle_scale ||
        settings.handedness!=expected.handedness || settings.play_mode!=expected.play_mode ||
        settings.player_height!=expected.player_height || settings.hrtf_mode!=expected.hrtf_mode) {
        std::cerr << "The complete VR profile did not round trip\n";
        return 10;
    }
    wchar_t preserved[16]{};
    if (GetPrivateProfileStringW(L"Unrelated",L"Preserved",L"",
            preserved,static_cast<DWORD>(sizeof(preserved)/sizeof(*preserved)),
            settings_path.c_str()) == 0 || std::wstring(preserved) != L"yes") {
        std::cerr << "The settings transaction discarded an unrelated section\n";
        return 15;
    }

    if (!WritePrivateProfileStringW(L"VR",L"Handedness",L"ambidextrous",settings_path.c_str()) ||
        penumbra_vr::launcher::LoadVrSettings(settings_path,settings,error) || error.empty()) return 11;
    if (!WritePrivateProfileStringW(L"VR",L"Handedness",L"Right",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"CrouchMode",L"floating",settings_path.c_str()) ||
        penumbra_vr::launcher::LoadVrSettings(settings_path,settings,error) || error.empty()) return 12;
    if (!WritePrivateProfileStringW(L"VR",L"CrouchMode",L"Hybrid",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"HRTF",L"maybe",settings_path.c_str()) ||
        penumbra_vr::launcher::LoadVrSettings(settings_path,settings,error) || error.empty()) return 13;

    if (!WritePrivateProfileStringW(L"VR",L"HRTF",L"Auto",settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"SettingsVersion",nullptr,settings_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR",L"SmoothTurnSpeed",L"120",settings_path.c_str()) ||
        !penumbra_vr::launcher::LoadVrSettings(settings_path,settings,error) ||
        settings.smooth_turn_speed != 90.0F) {
        std::cerr << "The legacy smooth-turn default did not migrate\n";
        return 14;
    }

    if (!WritePrivateProfileStringW(
            L"VR",L"SettingsVersion",L"-1",settings_path.c_str()) ||
        penumbra_vr::launcher::LoadVrSettings(settings_path,settings,error) ||
        error.empty()) {
        std::cerr << "A negative settings version was accepted\n";
        return 16;
    }

    std::wstring path_error;
    const std::filesystem::path default_path =
        penumbra_vr::launcher::DefaultVrSettingsPath(path_error);
    if (default_path.empty() || !path_error.empty() ||
        default_path.filename() != L"settings.ini" ||
        default_path.parent_path().filename() != L"PenumbraVR") {
        std::wcerr << L"The default settings path is invalid: " << path_error << L'\n';
        return 8;
    }

    std::cout << "Persistent VR settings passed\n";
    return 0;
}
