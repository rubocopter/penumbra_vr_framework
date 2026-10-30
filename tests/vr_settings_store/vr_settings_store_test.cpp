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

    // A malformed newly recognized integer must fail closed without rewriting
    // the user's existing profile, including keys unknown to this version.
    std::filesystem::create_directories(settings_path.parent_path());
    const auto malformed_path = settings_path.parent_path() / L"malformed.ini";
    const wchar_t* integer_keys[] = {L"NeuralInternalHeight", L"NeuralScalePercent",
        L"NeuralIntensityPercent", L"NeuralPasses", L"NeuralStyle",
        L"NeuralFoveaPercent", L"NeuralOuterScalePercent"};
    const wchar_t* malformed_values[] = {L"1.5", L"2passes", L"+", L"",
        L"2147483648", L"-2147483649", L"9999999999999999999999999",
        L"000000000000000000000000000000000000000000000000000000000000000000x"};
    if (!WritePrivateProfileStringW(L"VR", L"MoveSpeed", L"1.75", malformed_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"FutureOption", L"keep", malformed_path.c_str())) return 18;
    for (const auto* key : integer_keys) {
        for (const auto* value : malformed_values) {
            penumbra_vr::runtime::VrSettings rejected;
            if (!WritePrivateProfileStringW(L"VR", key, value, malformed_path.c_str()) ||
                penumbra_vr::launcher::LoadVrSettings(malformed_path, rejected, error) ||
                error.find(key) == std::wstring::npos) {
                std::wcerr << L"Malformed integer accepted or misdiagnosed: " << key
                    << L"=" << value << L'\n';
                return 18;
            }
            wchar_t prior[32]{};
            GetPrivateProfileStringW(L"VR", L"MoveSpeed", L"", prior, 32, malformed_path.c_str());
            if (std::wstring(prior) != L"1.75") return 19;
            GetPrivateProfileStringW(L"VR", L"FutureOption", L"", prior, 32, malformed_path.c_str());
            if (std::wstring(prior) != L"keep") return 19;
        }
        if (!WritePrivateProfileStringW(L"VR", key, nullptr, malformed_path.c_str())) return 18;
    }

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
    for (const auto& entry :
            std::filesystem::directory_iterator(settings_path.parent_path())) {
        if (entry.path().filename().wstring().rfind(
                L"settings.ini.tmp.", 0) == 0) {
            std::cerr << "A completed settings transaction left a temporary file\n";
            return 17;
        }
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
    expected.neural = {true, 1024, 65, 85, 3, 2, 40, true, 15};
    if (!WritePrivateProfileStringW(
            L"Unrelated", L"Preserved", L"yes", settings_path.c_str())) {
        std::cerr << "Could not create the unrelated-section fixture\n";
        return 9;
    }
    if (!WritePrivateProfileStringW(L"VR", L"FutureOption", L"keep", settings_path.c_str())) return 20;
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
        settings.player_height!=expected.player_height || settings.hrtf_mode!=expected.hrtf_mode ||
        settings.neural != expected.neural) {
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
    GetPrivateProfileStringW(L"VR", L"FutureOption", L"", preserved, 16, settings_path.c_str());
    if (std::wstring(preserved) != L"keep") return 20;

    // Check public INI names and units independently of the loader so a
    // save/load pair using the same wrong key cannot mask an integration bug.
    struct SavedValue { const wchar_t* key; const wchar_t* text; };
    const SavedValue saved_values[] = {
        {L"NeuralEnabled", L"true"}, {L"NeuralInternalHeight", L"1024"},
        {L"NeuralScalePercent", L"65"}, {L"NeuralIntensityPercent", L"85"},
        {L"NeuralPasses", L"3"}, {L"NeuralStyle", L"2"},
        {L"NeuralFoveaPercent", L"40"}, {L"NeuralOuterEnabled", L"true"},
        {L"NeuralOuterScalePercent", L"15"},
    };
    for (const auto& saved : saved_values) {
        wchar_t value[32]{};
        GetPrivateProfileStringW(L"VR", saved.key, L"", value, 32, settings_path.c_str());
        if (std::wstring(value) != saved.text) return 21;
    }
    const auto other_game_path = settings_path.parent_path() / L"other-game.ini";
    penumbra_vr::runtime::VrSettings other_game;
    if (!penumbra_vr::launcher::SaveVrSettings(other_game_path, other_game, error) ||
        !penumbra_vr::launcher::LoadVrSettings(settings_path, settings, error) ||
        settings.neural != expected.neural ||
        !penumbra_vr::launcher::LoadVrSettings(other_game_path, other_game, error) ||
        other_game.neural.enabled || other_game.neural.internal_height != 896 ||
        other_game.neural.scale_percent != 60 || other_game.neural.intensity_percent != 90 ||
        other_game.neural.passes != 1 || other_game.neural.style != 2 ||
        other_game.neural.fovea_percent != 100 || other_game.neural.outer_enabled ||
        other_game.neural.outer_scale_percent != 25) return 22;

    // Signed integral values are syntactically valid and normalize at the
    // schema boundary; decimals/overflow above are malformed and never clamp.
    if (!WritePrivateProfileStringW(L"VR", L"NeuralInternalHeight", L"-5", other_game_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"NeuralScalePercent", L"+101", other_game_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"NeuralIntensityPercent", L"-1", other_game_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"NeuralPasses", L"5", other_game_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"NeuralStyle", L"-1", other_game_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"NeuralFoveaPercent", L"10", other_game_path.c_str()) ||
        !WritePrivateProfileStringW(L"VR", L"NeuralOuterScalePercent", L"101", other_game_path.c_str()) ||
        !penumbra_vr::launcher::LoadVrSettings(other_game_path, other_game, error) ||
        other_game.neural.internal_height != 64 || other_game.neural.scale_percent != 100 ||
        other_game.neural.intensity_percent != 0 || other_game.neural.passes != 4 ||
        other_game.neural.style != 0 || other_game.neural.fovea_percent != 20 ||
        other_game.neural.outer_scale_percent != 100) return 23;
    for (const auto* key : {L"NeuralEnabled", L"NeuralOuterEnabled"}) {
        if (!WritePrivateProfileStringW(L"VR", key, L"maybe", other_game_path.c_str()) ||
            penumbra_vr::launcher::LoadVrSettings(other_game_path, other_game, error) ||
            error.find(key) == std::wstring::npos ||
            !WritePrivateProfileStringW(L"VR", key, L"false", other_game_path.c_str())) return 24;
    }

    // Old profiles without Neural keys retain safe defaults and old values.
    if (!penumbra_vr::launcher::LoadVrSettings(malformed_path, other_game, error) ||
        other_game.neural != penumbra_vr::runtime::NeuralPostSettings{} ||
        other_game.move_speed != 1.75F) return 25;

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

    penumbra_vr::runtime::VrSettings bp_profile, rq_profile, reloaded;
    bp_profile.neural.enabled = true;
    bp_profile.neural.intensity_percent = 35;
    rq_profile.neural.intensity_percent = 80;
    if (!penumbra_vr::launcher::SaveVrSettings(other_game_path, bp_profile, error, L"Neural.BlackPlague") ||
        !penumbra_vr::launcher::SaveVrSettings(other_game_path, rq_profile, error, L"Neural.Requiem") ||
        !penumbra_vr::launcher::LoadVrSettings(other_game_path, reloaded, error, L"Neural.BlackPlague") ||
        reloaded.neural != bp_profile.neural ||
        !penumbra_vr::launcher::LoadVrSettings(other_game_path, reloaded, error, L"Neural.Requiem") ||
        reloaded.neural != rq_profile.neural) {
        std::cerr << "Game Neural profiles interfere inside the shared settings file\n";
        return 26;
    }
    std::cout << "Persistent VR settings passed\n";
    return 0;
}
