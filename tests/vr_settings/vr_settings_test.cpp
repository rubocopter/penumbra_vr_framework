#include "vr_settings.hpp"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using penumbra_vr::runtime::VrCrouchMode;
using penumbra_vr::runtime::VrHandedness;
using penumbra_vr::runtime::VrHrtfMode;
using penumbra_vr::runtime::VrPlayMode;
using penumbra_vr::runtime::VrSettings;
using penumbra_vr::runtime::VrTurnMode;

[[nodiscard]] bool NearlyEqual(float left, float right) noexcept {
    return std::fabs(left - right) <= 0.00001F;
}

[[nodiscard]] bool TestDefaults() {
    const VrSettings settings;
    return NearlyEqual(settings.move_speed, 1.0F) &&
        NearlyEqual(settings.move_dead_zone, 0.15F) &&
        NearlyEqual(settings.height_offset, 0.0F) &&
        settings.turn_mode == VrTurnMode::snap &&
        NearlyEqual(settings.snap_turn_angle, 45.0F) &&
        NearlyEqual(settings.smooth_turn_speed, 90.0F) &&
        NearlyEqual(settings.turn_dead_zone, 0.20F) &&
        NearlyEqual(settings.ui_distance, 1.75F) &&
        NearlyEqual(settings.ui_scale, 1.0F) &&
        NearlyEqual(settings.render_scale, 1.0F) &&
        !settings.enhanced_visuals && !settings.monitor_mirror &&
        settings.crouch_mode == VrCrouchMode::hybrid &&
        NearlyEqual(settings.physical_crouch_depth, 0.25F) &&
        NearlyEqual(settings.subtitle_scale, 1.35F) &&
        settings.handedness == VrHandedness::right &&
        settings.play_mode == VrPlayMode::standing &&
        NearlyEqual(settings.player_height, 1.70F) &&
        settings.hrtf_mode == VrHrtfMode::automatic;
}

[[nodiscard]] bool TestNormalization() {
    VrSettings settings;
    settings.move_speed = -4.0F;
    settings.move_dead_zone = 4.0F;
    settings.height_offset = std::numeric_limits<float>::quiet_NaN();
    settings.snap_turn_angle = 200.0F;
    settings.smooth_turn_speed = -1.0F;
    settings.turn_dead_zone = -2.0F;
    settings.ui_distance = 9.0F;
    settings.ui_scale = 0.1F;
    settings.render_scale = std::numeric_limits<float>::infinity();
    settings.physical_crouch_depth = 0.0F;
    settings.subtitle_scale = 9.0F;
    settings.player_height = -9.0F;
    settings.turn_mode = static_cast<VrTurnMode>(255);
    settings.crouch_mode = static_cast<VrCrouchMode>(255);
    settings.handedness = static_cast<VrHandedness>(255);
    settings.play_mode = static_cast<VrPlayMode>(255);
    settings.hrtf_mode = static_cast<VrHrtfMode>(255);

    penumbra_vr::runtime::NormalizeVrSettings(settings);
    return NearlyEqual(settings.move_speed, 0.25F) &&
        NearlyEqual(settings.move_dead_zone, 0.9F) &&
        NearlyEqual(settings.height_offset, 0.0F) &&
        NearlyEqual(settings.snap_turn_angle, 90.0F) &&
        NearlyEqual(settings.smooth_turn_speed, 30.0F) &&
        NearlyEqual(settings.turn_dead_zone, 0.0F) &&
        NearlyEqual(settings.ui_distance, 3.0F) &&
        NearlyEqual(settings.ui_scale, 0.5F) &&
        NearlyEqual(settings.render_scale, 1.0F) &&
        NearlyEqual(settings.physical_crouch_depth, 0.10F) &&
        NearlyEqual(settings.subtitle_scale, 2.0F) &&
        NearlyEqual(settings.player_height, 1.40F) &&
        settings.turn_mode == VrTurnMode::snap &&
        settings.crouch_mode == VrCrouchMode::hybrid &&
        settings.handedness == VrHandedness::right &&
        settings.play_mode == VrPlayMode::standing &&
        settings.hrtf_mode == VrHrtfMode::automatic;
}

[[nodiscard]] bool TestLegacyMigration() {
    VrSettings legacy;
    legacy.smooth_turn_speed = 120.0F;
    penumbra_vr::runtime::NormalizeVrSettings(legacy, 0);
    if (!NearlyEqual(legacy.smooth_turn_speed, 90.0F)) {
        return false;
    }

    VrSettings current;
    current.smooth_turn_speed = 120.0F;
    penumbra_vr::runtime::NormalizeVrSettings(current, 1);
    return NearlyEqual(current.smooth_turn_speed, 120.0F);
}

[[nodiscard]] bool TestNeuralNormalizationAndChangeDetection() {
    using namespace penumbra_vr::runtime;
    VrSettings settings;
    settings.neural = {true, -1, 101, -1, 5, -1, 19, true, 101};
    NormalizeVrSettings(settings);
    const NeuralPostSettings first{true, 64, 100, 0, 4, 0, 20, true, 100};
    if (settings.neural != first) return false;
    settings.neural = {false, 1281, 24, 101, 0, 3, 101, false, 9};
    NormalizeNeuralPostSettings(settings.neural);
    const NeuralPostSettings second{false, 1280, 25, 100, 1, 2, 100, false, 10};
    if (settings.neural != second) return false;
    NormalizeNeuralPostSettings(settings.neural);
    if (settings.neural != second) return false;

    // Every field can trigger a render-owner revision; overlooking a field in
    // equality would silently drop an accepted change.
    const NeuralPostSettings baseline;
    for (int field = 0; field < 9; ++field) {
        auto changed = baseline;
        switch (field) {
        case 0: changed.enabled = true; break;
        case 1: changed.internal_height = 960; break;
        case 2: changed.scale_percent = 50; break;
        case 3: changed.intensity_percent = 65; break;
        case 4: changed.passes = 2; break;
        case 5: changed.style = 1; break;
        case 6: changed.fovea_percent = 50; break;
        case 7: changed.outer_enabled = true; break;
        case 8: changed.outer_scale_percent = 30; break;
        }
        if (baseline == changed || !(baseline != changed) || changed == baseline) return false;
    }
    return baseline == NeuralPostSettings{} && !(baseline != NeuralPostSettings{});
}

[[nodiscard]] bool TestTextValues() {
    using namespace penumbra_vr::runtime;
    return ParseVrTurnMode("SMOOTH") == VrTurnMode::smooth &&
        ParseVrTurnMode("unexpected") == VrTurnMode::snap &&
        ParseVrCrouchMode("physical") == VrCrouchMode::physical &&
        ParseVrCrouchMode("BUTTON") == VrCrouchMode::button &&
        ParseVrCrouchMode("") == VrCrouchMode::hybrid &&
        ParseVrHandedness("LEFT") == VrHandedness::left &&
        ParseVrHandedness("unexpected") == VrHandedness::right &&
        ParseVrPlayMode("Seated") == VrPlayMode::seated &&
        ParseVrPlayMode("unexpected") == VrPlayMode::standing &&
        ParseVrHrtfMode("ON") == VrHrtfMode::on &&
        ParseVrHrtfMode("off") == VrHrtfMode::off &&
        ParseVrHrtfMode("unexpected") == VrHrtfMode::automatic &&
        ToConfigValue(VrTurnMode::disabled) == "Disabled" &&
        ToConfigValue(VrTurnMode::snap) == "Snap" &&
        ToConfigValue(VrTurnMode::smooth) == "Smooth" &&
        ToConfigValue(VrCrouchMode::physical) == "Physical" &&
        ToConfigValue(VrCrouchMode::button) == "Button" &&
        ToConfigValue(VrCrouchMode::hybrid) == "Hybrid" &&
        ToConfigValue(VrHandedness::left) == "Left" &&
        ToConfigValue(VrHandedness::right) == "Right" &&
        ToConfigValue(VrPlayMode::standing) == "Standing" &&
        ToConfigValue(VrPlayMode::seated) == "Seated" &&
        ToConfigValue(VrHrtfMode::automatic) == "Auto" &&
        ToConfigValue(VrHrtfMode::on) == "On" &&
        ToConfigValue(VrHrtfMode::off) == "Off";
}

} // namespace

int main() {
    if (!TestDefaults()) {
        std::cerr << "VR setting defaults do not match the Rework baseline\n";
        return 1;
    }
    if (!TestNormalization()) {
        std::cerr << "VR setting normalization failed\n";
        return 2;
    }
    if (!TestLegacyMigration()) {
        std::cerr << "VR setting migration failed\n";
        return 3;
    }
    if (!TestTextValues()) {
        std::cerr << "VR setting text conversion failed\n";
        return 4;
    }
    if (!TestNeuralNormalizationAndChangeDetection()) {
        std::cerr << "Neural settings bounds or revision comparison failed\n";
        return 5;
    }

    std::cout << "VR setting defaults, normalization and migration passed\n";
    return 0;
}
