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
    return descriptors.size() == 18 &&
        descriptors.front().id == VrSettingId::handedness &&
        descriptors.back().id == VrSettingId::subtitle_scale &&
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

} // namespace

int main() {
    if (!TestDescriptorOrderAndFormatting()) return 1;
    if (!TestReworkAdjustmentStepsAndWrapping()) return 2;
    if (!TestAvailabilityAndLabels()) return 3;
    if (!TestClampingAtLimits()) return 4;
    if (!TestCapabilityScopedReset()) return 5;
    std::cout << "VR settings editor policy matches the Overture Rework menu baseline\n";
    return 0;
}
