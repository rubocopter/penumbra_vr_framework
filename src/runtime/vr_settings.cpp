#include "vr_settings.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {

[[nodiscard]] bool EqualsAsciiCaseInsensitive(
    std::string_view left,
    std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        char left_character = left[index];
        char right_character = right[index];
        if (left_character >= 'A' && left_character <= 'Z') {
            left_character = static_cast<char>(left_character - 'A' + 'a');
        }
        if (right_character >= 'A' && right_character <= 'Z') {
            right_character = static_cast<char>(right_character - 'A' + 'a');
        }
        if (left_character != right_character) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] float NormalizeFloat(
    float value,
    const VrFloatSettingRange& range) noexcept {
    if (!std::isfinite(value)) {
        return range.default_value;
    }
    return std::clamp(value, range.minimum, range.maximum);
}

} // namespace

void NormalizeVrSettings(
    VrSettings& settings,
    std::uint32_t source_version) noexcept {
    settings.move_speed = NormalizeFloat(
        settings.move_speed, vr_setting_limits::kMoveSpeed);
    settings.move_dead_zone = NormalizeFloat(
        settings.move_dead_zone, vr_setting_limits::kMoveDeadZone);
    settings.height_offset = NormalizeFloat(
        settings.height_offset, vr_setting_limits::kHeightOffset);
    settings.snap_turn_angle = NormalizeFloat(
        settings.snap_turn_angle, vr_setting_limits::kSnapTurnAngle);
    settings.smooth_turn_speed = NormalizeFloat(
        settings.smooth_turn_speed, vr_setting_limits::kSmoothTurnSpeed);
    if (source_version < 1 &&
        settings.smooth_turn_speed > 119.9F &&
        settings.smooth_turn_speed < 120.1F) {
        settings.smooth_turn_speed =
            vr_setting_limits::kSmoothTurnSpeed.default_value;
    }
    settings.turn_dead_zone = NormalizeFloat(
        settings.turn_dead_zone, vr_setting_limits::kTurnDeadZone);
    settings.ui_distance = NormalizeFloat(
        settings.ui_distance, vr_setting_limits::kUiDistance);
    settings.ui_scale = NormalizeFloat(
        settings.ui_scale, vr_setting_limits::kUiScale);
    settings.render_scale = NormalizeFloat(
        settings.render_scale, vr_setting_limits::kRenderScale);
    settings.physical_crouch_depth = NormalizeFloat(
        settings.physical_crouch_depth,
        vr_setting_limits::kPhysicalCrouchDepth);
    settings.subtitle_scale = NormalizeFloat(
        settings.subtitle_scale, vr_setting_limits::kSubtitleScale);
    settings.player_height = NormalizeFloat(
        settings.player_height, vr_setting_limits::kPlayerHeight);

    switch (settings.turn_mode) {
    case VrTurnMode::disabled:
    case VrTurnMode::snap:
    case VrTurnMode::smooth:
        break;
    default:
        settings.turn_mode = VrTurnMode::snap;
        break;
    }
    switch (settings.crouch_mode) {
    case VrCrouchMode::physical:
    case VrCrouchMode::button:
    case VrCrouchMode::hybrid:
        break;
    default:
        settings.crouch_mode = VrCrouchMode::hybrid;
        break;
    }
    switch (settings.handedness) {
    case VrHandedness::left:
    case VrHandedness::right:
        break;
    default:
        settings.handedness = VrHandedness::right;
        break;
    }
    switch (settings.play_mode) {
    case VrPlayMode::standing:
    case VrPlayMode::seated:
        break;
    default:
        settings.play_mode = VrPlayMode::standing;
        break;
    }
    switch (settings.hrtf_mode) {
    case VrHrtfMode::automatic:
    case VrHrtfMode::on:
    case VrHrtfMode::off:
        break;
    default:
        settings.hrtf_mode = VrHrtfMode::automatic;
        break;
    }
}

VrTurnMode ParseVrTurnMode(std::string_view value) noexcept {
    if (EqualsAsciiCaseInsensitive(value, "disabled")) {
        return VrTurnMode::disabled;
    }
    if (EqualsAsciiCaseInsensitive(value, "smooth")) {
        return VrTurnMode::smooth;
    }
    return VrTurnMode::snap;
}

VrCrouchMode ParseVrCrouchMode(std::string_view value) noexcept {
    if (EqualsAsciiCaseInsensitive(value, "physical")) {
        return VrCrouchMode::physical;
    }
    if (EqualsAsciiCaseInsensitive(value, "button")) {
        return VrCrouchMode::button;
    }
    return VrCrouchMode::hybrid;
}

VrHandedness ParseVrHandedness(std::string_view value) noexcept {
    return EqualsAsciiCaseInsensitive(value, "left")
        ? VrHandedness::left
        : VrHandedness::right;
}

VrPlayMode ParseVrPlayMode(std::string_view value) noexcept {
    return EqualsAsciiCaseInsensitive(value, "seated")
        ? VrPlayMode::seated
        : VrPlayMode::standing;
}

VrHrtfMode ParseVrHrtfMode(std::string_view value) noexcept {
    if (EqualsAsciiCaseInsensitive(value, "on")) {
        return VrHrtfMode::on;
    }
    if (EqualsAsciiCaseInsensitive(value, "off")) {
        return VrHrtfMode::off;
    }
    return VrHrtfMode::automatic;
}

std::string_view ToConfigValue(VrTurnMode value) noexcept {
    switch (value) {
    case VrTurnMode::disabled:
        return "Disabled";
    case VrTurnMode::smooth:
        return "Smooth";
    default:
        return "Snap";
    }
}

std::string_view ToConfigValue(VrCrouchMode value) noexcept {
    switch (value) {
    case VrCrouchMode::physical:
        return "Physical";
    case VrCrouchMode::button:
        return "Button";
    default:
        return "Hybrid";
    }
}

std::string_view ToConfigValue(VrHandedness value) noexcept {
    return value == VrHandedness::left ? "Left" : "Right";
}

std::string_view ToConfigValue(VrPlayMode value) noexcept {
    return value == VrPlayMode::seated ? "Seated" : "Standing";
}

std::string_view ToConfigValue(VrHrtfMode value) noexcept {
    switch (value) {
    case VrHrtfMode::on:
        return "On";
    case VrHrtfMode::off:
        return "Off";
    default:
        return "Auto";
    }
}

} // namespace penumbra_vr::runtime
