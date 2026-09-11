#include "vr_settings_editor.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace penumbra_vr::runtime {
namespace {

constexpr std::array<VrSettingDescriptor, kVrSettingCount> kDescriptors{{
    {VrSettingId::handedness, "VRHandedness"},
    {VrSettingId::play_mode, "VRPlayMode"},
    {VrSettingId::player_height, "VRPlayerHeight"},
    {VrSettingId::turn_mode, "VRTurnMode"},
    {VrSettingId::snap_turn_angle, "VRSnapAngle"},
    {VrSettingId::smooth_turn_speed, "VRSmoothSpeed"},
    {VrSettingId::turn_dead_zone, "VRTurnDeadZone"},
    {VrSettingId::move_speed, "VRMoveSpeed"},
    {VrSettingId::move_dead_zone, "VRMoveDeadZone"},
    {VrSettingId::crouch_mode, "VRCrouchMode"},
    {VrSettingId::physical_crouch_depth, "VRPhysicalCrouchDepth"},
    {VrSettingId::height_offset, "VRHeightOffset"},
    {VrSettingId::ui_distance, "VRUIDistance"},
    {VrSettingId::ui_scale, "VRUIScale"},
    {VrSettingId::render_scale, "VRRenderScale"},
    {VrSettingId::enhanced_visuals, "VREnhancedVisuals"},
    {VrSettingId::hrtf, "VRHRTF"},
    {VrSettingId::subtitle_scale, "VRSubtitleScale"},
}};

[[nodiscard]] std::size_t Index(VrSettingId id) noexcept {
    return static_cast<std::size_t>(id);
}

template <typename Enum>
[[nodiscard]] Enum WrapEnum(Enum value, int direction, Enum first, Enum last) noexcept {
    int next = static_cast<int>(value) + direction;
    if (next < static_cast<int>(first)) {
        next = static_cast<int>(last);
    }
    if (next > static_cast<int>(last)) {
        next = static_cast<int>(first);
    }
    return static_cast<Enum>(next);
}

template <typename... Args>
[[nodiscard]] std::string Format(const char* format, Args... args) {
    std::array<char, 64> buffer{};
    const int count = std::snprintf(buffer.data(), buffer.size(), format, args...);
    if (count <= 0) {
        return {};
    }
    return std::string(buffer.data(), static_cast<std::size_t>(
        std::min<int>(count, static_cast<int>(buffer.size() - 1))));
}

} // namespace

VrSettingCapabilities AllVrSettingCapabilities() noexcept {
    VrSettingCapabilities capabilities;
    capabilities.supported.fill(true);
    return capabilities;
}

const std::array<VrSettingDescriptor, kVrSettingCount>&
VrSettingDescriptors() noexcept {
    return kDescriptors;
}

bool IsVrSettingAvailable(
    VrSettingId id,
    const VrSettings& settings,
    const VrSettingCapabilities& capabilities) noexcept {
    const std::size_t index = Index(id);
    if (index >= capabilities.supported.size() || !capabilities.supported[index]) {
        return false;
    }
    if (id == VrSettingId::snap_turn_angle) {
        return settings.turn_mode == VrTurnMode::snap;
    }
    if (id == VrSettingId::smooth_turn_speed) {
        return settings.turn_mode == VrTurnMode::smooth;
    }
    return id != VrSettingId::count;
}

bool AdjustVrSetting(
    VrSettings& settings,
    VrSettingId id,
    int direction) noexcept {
    if (direction == 0 || id == VrSettingId::count) {
        return false;
    }
    direction = direction < 0 ? -1 : 1;
    NormalizeVrSettings(settings);

    switch (id) {
    case VrSettingId::handedness:
        settings.handedness = WrapEnum(
            settings.handedness, direction, VrHandedness::left, VrHandedness::right);
        break;
    case VrSettingId::play_mode:
        settings.play_mode = WrapEnum(
            settings.play_mode, direction, VrPlayMode::standing, VrPlayMode::seated);
        break;
    case VrSettingId::player_height:
        settings.player_height += 0.05F * direction;
        break;
    case VrSettingId::turn_mode:
        settings.turn_mode = WrapEnum(
            settings.turn_mode, direction, VrTurnMode::disabled, VrTurnMode::smooth);
        break;
    case VrSettingId::snap_turn_angle:
        settings.snap_turn_angle += 15.0F * direction;
        break;
    case VrSettingId::smooth_turn_speed:
        settings.smooth_turn_speed += 30.0F * direction;
        break;
    case VrSettingId::turn_dead_zone:
        settings.turn_dead_zone += 0.05F * direction;
        break;
    case VrSettingId::move_speed:
        settings.move_speed += 0.25F * direction;
        break;
    case VrSettingId::move_dead_zone:
        settings.move_dead_zone += 0.05F * direction;
        break;
    case VrSettingId::crouch_mode:
        settings.crouch_mode = WrapEnum(
            settings.crouch_mode, direction, VrCrouchMode::physical, VrCrouchMode::hybrid);
        break;
    case VrSettingId::physical_crouch_depth:
        settings.physical_crouch_depth += 0.05F * direction;
        break;
    case VrSettingId::height_offset:
        settings.height_offset += 0.05F * direction;
        break;
    case VrSettingId::ui_distance:
        settings.ui_distance += 0.25F * direction;
        break;
    case VrSettingId::ui_scale:
        settings.ui_scale += 0.10F * direction;
        break;
    case VrSettingId::render_scale:
        settings.render_scale += 0.05F * direction;
        break;
    case VrSettingId::enhanced_visuals:
        settings.enhanced_visuals = !settings.enhanced_visuals;
        break;
    case VrSettingId::hrtf:
        settings.hrtf_mode = WrapEnum(
            settings.hrtf_mode, direction, VrHrtfMode::automatic, VrHrtfMode::off);
        break;
    case VrSettingId::subtitle_scale:
        settings.subtitle_scale += 0.10F * direction;
        break;
    case VrSettingId::count:
        return false;
    }

    NormalizeVrSettings(settings);
    return true;
}

void ResetVrSettings(
    VrSettings& settings,
    const VrSettingCapabilities& capabilities) noexcept {
    const VrSettings defaults;
    for (const auto& descriptor : kDescriptors) {
        if (!capabilities.supported[Index(descriptor.id)]) {
            continue;
        }
        switch (descriptor.id) {
        case VrSettingId::handedness: settings.handedness = defaults.handedness; break;
        case VrSettingId::play_mode: settings.play_mode = defaults.play_mode; break;
        case VrSettingId::player_height: settings.player_height = defaults.player_height; break;
        case VrSettingId::turn_mode: settings.turn_mode = defaults.turn_mode; break;
        case VrSettingId::snap_turn_angle: settings.snap_turn_angle = defaults.snap_turn_angle; break;
        case VrSettingId::smooth_turn_speed: settings.smooth_turn_speed = defaults.smooth_turn_speed; break;
        case VrSettingId::turn_dead_zone: settings.turn_dead_zone = defaults.turn_dead_zone; break;
        case VrSettingId::move_speed: settings.move_speed = defaults.move_speed; break;
        case VrSettingId::move_dead_zone: settings.move_dead_zone = defaults.move_dead_zone; break;
        case VrSettingId::crouch_mode: settings.crouch_mode = defaults.crouch_mode; break;
        case VrSettingId::physical_crouch_depth:
            settings.physical_crouch_depth = defaults.physical_crouch_depth;
            break;
        case VrSettingId::height_offset: settings.height_offset = defaults.height_offset; break;
        case VrSettingId::ui_distance: settings.ui_distance = defaults.ui_distance; break;
        case VrSettingId::ui_scale: settings.ui_scale = defaults.ui_scale; break;
        case VrSettingId::render_scale: settings.render_scale = defaults.render_scale; break;
        case VrSettingId::enhanced_visuals:
            settings.enhanced_visuals = defaults.enhanced_visuals;
            break;
        case VrSettingId::hrtf: settings.hrtf_mode = defaults.hrtf_mode; break;
        case VrSettingId::subtitle_scale: settings.subtitle_scale = defaults.subtitle_scale; break;
        case VrSettingId::count: break;
        }
    }
    NormalizeVrSettings(settings);
}

std::string FormatVrSettingValue(
    VrSettingId id,
    const VrSettings& settings) {
    switch (id) {
    case VrSettingId::handedness:
        return std::string(ToConfigValue(settings.handedness));
    case VrSettingId::play_mode:
        return std::string(ToConfigValue(settings.play_mode));
    case VrSettingId::player_height:
        return Format("%.2f m", settings.player_height);
    case VrSettingId::turn_mode:
        return std::string(ToConfigValue(settings.turn_mode));
    case VrSettingId::snap_turn_angle:
        return Format("%.0f deg", settings.snap_turn_angle);
    case VrSettingId::smooth_turn_speed:
        return Format("%.0f deg/s", settings.smooth_turn_speed);
    case VrSettingId::turn_dead_zone:
        return Format("%.0f%%", settings.turn_dead_zone * 100.0F);
    case VrSettingId::move_speed:
        return Format("%.2fx", settings.move_speed);
    case VrSettingId::move_dead_zone:
        return Format("%.0f%%", settings.move_dead_zone * 100.0F);
    case VrSettingId::crouch_mode:
        return std::string(ToConfigValue(settings.crouch_mode));
    case VrSettingId::physical_crouch_depth:
        return Format("%.2f m", settings.physical_crouch_depth);
    case VrSettingId::height_offset:
        return Format("%+.2f m", settings.height_offset);
    case VrSettingId::ui_distance:
        return Format("%.2f m", settings.ui_distance);
    case VrSettingId::ui_scale:
        return Format("%.0f%%", settings.ui_scale * 100.0F);
    case VrSettingId::render_scale:
        return Format("%.2fx", settings.render_scale);
    case VrSettingId::enhanced_visuals:
        return settings.enhanced_visuals ? "On" : "Off";
    case VrSettingId::hrtf:
        return std::string(ToConfigValue(settings.hrtf_mode));
    case VrSettingId::subtitle_scale:
        return Format("%.0f%%", settings.subtitle_scale * 100.0F);
    case VrSettingId::count:
        return {};
    }
    return {};
}

std::string_view VrSettingTranslationKey(
    VrSettingId id,
    const VrSettings& settings) noexcept {
    if (id == VrSettingId::physical_crouch_depth &&
        settings.crouch_mode == VrCrouchMode::physical) {
        return "VRDetectionDepth";
    }
    const std::size_t index = Index(id);
    return index < kDescriptors.size() ? kDescriptors[index].translation_key : std::string_view{};
}

} // namespace penumbra_vr::runtime
