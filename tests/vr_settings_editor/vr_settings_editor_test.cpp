#include "vr_settings_editor.hpp"

#include <cmath>
#include <iostream>

namespace {

using namespace penumbra_vr::runtime;

[[nodiscard]] bool NearlyEqual(float left, float right) noexcept {
    return std::fabs(left - right) <= 0.0001F;
}

[[nodiscard]] bool TestDescriptorOrderAndFormatting() {
    const auto& descriptors = VrSettingDescriptors();
    const VrSettings settings;
    return descriptors.size() == 27 &&
        descriptors.front().id == VrSettingId::handedness &&
        descriptors[17].id == VrSettingId::subtitle_scale &&
        descriptors[18].id == VrSettingId::neural_enabled &&
        descriptors.back().id == VrSettingId::neural_outer_scale &&
        FormatVrSettingValue(VrSettingId::handedness, settings) == "Right" &&
        FormatVrSettingValue(VrSettingId::player_height, settings) == "1.70 m" &&
        FormatVrSettingValue(VrSettingId::snap_turn_angle, settings) == "45 deg" &&
        FormatVrSettingValue(VrSettingId::smooth_turn_speed, settings) == "90 deg/s" &&
        FormatVrSettingValue(VrSettingId::turn_dead_zone, settings) == "20%" &&
        FormatVrSettingValue(VrSettingId::move_speed, settings) == "1.00x" &&
        FormatVrSettingValue(VrSettingId::height_offset, settings) == "+0.00 m" &&
        FormatVrSettingValue(VrSettingId::ui_distance, settings) == "1.75 m" &&
        FormatVrSettingValue(VrSettingId::ui_scale, settings) == "100%" &&
        FormatVrSettingValue(VrSettingId::render_scale, settings) == "1.00x" &&
        FormatVrSettingValue(VrSettingId::enhanced_visuals, settings) == "Off" &&
        FormatVrSettingValue(VrSettingId::hrtf, settings) == "Auto" &&
        FormatVrSettingValue(VrSettingId::subtitle_scale, settings) == "135%";
}

[[nodiscard]] bool TestReworkAdjustmentStepsAndWrapping() {
    VrSettings settings;
    return AdjustVrSetting(settings, VrSettingId::move_speed, 4) &&
        NearlyEqual(settings.move_speed, 1.25F) &&
        AdjustVrSetting(settings, VrSettingId::move_dead_zone, -8) &&
        NearlyEqual(settings.move_dead_zone, 0.10F) &&
        AdjustVrSetting(settings, VrSettingId::snap_turn_angle, 1) &&
        NearlyEqual(settings.snap_turn_angle, 60.0F) &&
        AdjustVrSetting(settings, VrSettingId::smooth_turn_speed, 1) &&
        NearlyEqual(settings.smooth_turn_speed, 120.0F) &&
        AdjustVrSetting(settings, VrSettingId::ui_distance, -1) &&
        NearlyEqual(settings.ui_distance, 1.50F) &&
        AdjustVrSetting(settings, VrSettingId::ui_scale, 1) &&
        NearlyEqual(settings.ui_scale, 1.10F) &&
        AdjustVrSetting(settings, VrSettingId::render_scale, -1) &&
        NearlyEqual(settings.render_scale, 0.95F) &&
        AdjustVrSetting(settings, VrSettingId::subtitle_scale, 1) &&
        NearlyEqual(settings.subtitle_scale, 1.45F) &&
        AdjustVrSetting(settings, VrSettingId::handedness, 1) &&
        settings.handedness == VrHandedness::left &&
        AdjustVrSetting(settings, VrSettingId::play_mode, -1) &&
        settings.play_mode == VrPlayMode::seated &&
        AdjustVrSetting(settings, VrSettingId::turn_mode, -1) &&
        settings.turn_mode == VrTurnMode::disabled &&
        AdjustVrSetting(settings, VrSettingId::crouch_mode, 1) &&
        settings.crouch_mode == VrCrouchMode::physical &&
        AdjustVrSetting(settings, VrSettingId::hrtf, -1) &&
        settings.hrtf_mode == VrHrtfMode::off &&
        AdjustVrSetting(settings, VrSettingId::enhanced_visuals, 1) &&
        settings.enhanced_visuals;
}

[[nodiscard]] bool TestAvailabilityAndLabels() {
    VrSettings settings;
    auto capabilities = AllVrSettingCapabilities();
    if (!IsVrSettingAvailable(VrSettingId::snap_turn_angle, settings, capabilities) ||
        IsVrSettingAvailable(VrSettingId::smooth_turn_speed, settings, capabilities)) {
        return false;
    }
    settings.turn_mode = VrTurnMode::smooth;
    if (IsVrSettingAvailable(VrSettingId::snap_turn_angle, settings, capabilities) ||
        !IsVrSettingAvailable(VrSettingId::smooth_turn_speed, settings, capabilities)) {
        return false;
    }
    capabilities.supported[static_cast<std::size_t>(VrSettingId::move_speed)] = false;
    if (IsVrSettingAvailable(VrSettingId::move_speed, settings, capabilities)) {
        return false;
    }
    settings.crouch_mode = VrCrouchMode::physical;
    if (VrSettingTranslationKey(VrSettingId::physical_crouch_depth, settings) !=
        "VRDetectionDepth") {
        return false;
    }
    settings.crouch_mode = VrCrouchMode::hybrid;
    return VrSettingTranslationKey(VrSettingId::physical_crouch_depth, settings) ==
        "VRPhysicalCrouchDepth";
}

[[nodiscard]] bool TestClampingAtLimits() {
    VrSettings settings;
    settings.move_speed = vr_setting_limits::kMoveSpeed.maximum;
    settings.player_height = vr_setting_limits::kPlayerHeight.minimum;
    return AdjustVrSetting(settings, VrSettingId::move_speed, 1) &&
        NearlyEqual(settings.move_speed, vr_setting_limits::kMoveSpeed.maximum) &&
        AdjustVrSetting(settings, VrSettingId::player_height, -1) &&
        NearlyEqual(settings.player_height, vr_setting_limits::kPlayerHeight.minimum) &&
        !AdjustVrSetting(settings, VrSettingId::move_speed, 0) &&
        !AdjustVrSetting(settings, VrSettingId::count, 1);
}

[[nodiscard]] bool TestCapabilityScopedReset() {
    VrSettings settings;
    settings.move_speed = 2.5F;
    settings.turn_mode = VrTurnMode::smooth;
    settings.player_height = 2.05F;
    settings.enhanced_visuals = true;

    VrSettingCapabilities capabilities;
    capabilities.supported[static_cast<std::size_t>(VrSettingId::move_speed)] = true;
    capabilities.supported[static_cast<std::size_t>(VrSettingId::turn_mode)] = true;
    ResetVrSettings(settings, capabilities);

    return NearlyEqual(settings.move_speed, vr_setting_limits::kMoveSpeed.default_value) &&
        settings.turn_mode == VrTurnMode::snap &&
        NearlyEqual(settings.player_height, 2.05F) &&
        settings.enhanced_visuals;
}

[[nodiscard]] bool TestNeuralEditsAndReset() {
    struct EditCase { VrSettingId id; int NeuralPostSettings::*field; int value; int increased;
        int minimum; int maximum; };
    const EditCase cases[] = {
        {VrSettingId::neural_internal_height, &NeuralPostSettings::internal_height, 896, 960, 64, 1280},
        {VrSettingId::neural_scale, &NeuralPostSettings::scale_percent, 50, 55, 25, 100},
        {VrSettingId::neural_intensity, &NeuralPostSettings::intensity_percent, 60, 65, 0, 100},
        {VrSettingId::neural_passes, &NeuralPostSettings::passes, 2, 3, 1, 4},
        {VrSettingId::neural_fovea, &NeuralPostSettings::fovea_percent, 50, 55, 20, 100},
        {VrSettingId::neural_outer_scale, &NeuralPostSettings::outer_scale_percent, 25, 30, 10, 100},
    };
    for (const auto& edit : cases) {
        VrSettings settings;
        settings.neural.*(edit.field) = edit.value;
        if (!AdjustVrSetting(settings, edit.id, 8) ||
            settings.neural.*(edit.field) != edit.increased ||
            !AdjustVrSetting(settings, edit.id, -8) ||
            settings.neural.*(edit.field) != edit.value) return false;
        settings.neural.*(edit.field) = edit.minimum;
        if (!AdjustVrSetting(settings, edit.id, -1) ||
            settings.neural.*(edit.field) != edit.minimum) return false;
        settings.neural.*(edit.field) = edit.maximum;
        if (!AdjustVrSetting(settings, edit.id, 1) ||
            settings.neural.*(edit.field) != edit.maximum) return false;
    }
    VrSettings settings;
    if (!AdjustVrSetting(settings, VrSettingId::neural_enabled, -1) || !settings.neural.enabled ||
        !AdjustVrSetting(settings, VrSettingId::neural_enabled, 1) || settings.neural.enabled ||
        !AdjustVrSetting(settings, VrSettingId::neural_outer_enabled, 1) || !settings.neural.outer_enabled ||
        !AdjustVrSetting(settings, VrSettingId::neural_outer_enabled, -1) || settings.neural.outer_enabled ||
        !AdjustVrSetting(settings, VrSettingId::neural_style, -1) || settings.neural.style != 1 ||
        !AdjustVrSetting(settings, VrSettingId::neural_style, 1) || settings.neural.style != 2 ||
        !AdjustVrSetting(settings, VrSettingId::neural_style, 1) || settings.neural.style != 0) return false;
    const auto snapshot = settings.neural;
    if (AdjustVrSetting(settings, VrSettingId::neural_intensity, 0) || settings.neural != snapshot) return false;

    // Disabled processing must still allow configuring the next enable.
    auto capabilities = AllVrSettingCapabilities();
    for (const auto& descriptor : VrSettingDescriptors()) {
        if (descriptor.id >= VrSettingId::neural_enabled &&
            (!IsVrSettingAvailable(descriptor.id, settings, capabilities) ||
                VrSettingTranslationKey(descriptor.id, settings).empty() ||
                FormatVrSettingValue(descriptor.id, settings).empty())) return false;
    }
    settings.neural = {true, 1024, 50, 80, 3, 2, 40, true, 20};
    struct DisplayCase { VrSettingId id; const char* text; };
    const DisplayCase displays[] = {
        {VrSettingId::neural_enabled, "On"},
        {VrSettingId::neural_internal_height, "1024 px"},
        {VrSettingId::neural_scale, "50%"},
        {VrSettingId::neural_intensity, "80%"},
        {VrSettingId::neural_passes, "3"},
        {VrSettingId::neural_style, "2"},
        {VrSettingId::neural_fovea, "40%"},
        {VrSettingId::neural_outer_enabled, "On"},
        {VrSettingId::neural_outer_scale, "20%"},
    };
    for (const auto& display : displays) {
        if (FormatVrSettingValue(display.id, settings) != display.text) return false;
    }
    settings.move_speed = 2.5F;
    VrSettingCapabilities partial;
    partial.supported[static_cast<std::size_t>(VrSettingId::neural_intensity)] = true;
    ResetVrSettings(settings, partial);
    if (settings.neural.intensity_percent != 90 || settings.neural.internal_height != 1024 ||
        !settings.neural.enabled || settings.neural.style != 2 || !settings.neural.outer_enabled ||
        settings.neural.outer_scale_percent != 20 || settings.move_speed != 2.5F) return false;
    VrSettingCapabilities neural_only;
    for (const auto& descriptor : VrSettingDescriptors()) {
        if (descriptor.id >= VrSettingId::neural_enabled) {
            neural_only.supported[static_cast<std::size_t>(descriptor.id)] = true;
        }
    }
    ResetVrSettings(settings, neural_only);
    return settings.neural == NeuralPostSettings{} && settings.move_speed == 2.5F;
}

} // namespace

int main() {
    if (!TestDescriptorOrderAndFormatting()) return 1;
    if (!TestReworkAdjustmentStepsAndWrapping()) return 2;
    if (!TestAvailabilityAndLabels()) return 3;
    if (!TestClampingAtLimits()) return 4;
    if (!TestCapabilityScopedReset()) return 5;
    if (!TestNeuralEditsAndReset()) return 6;
    std::cout << "VR settings editor policy matches the Overture Rework menu baseline\n";
    return 0;
}
