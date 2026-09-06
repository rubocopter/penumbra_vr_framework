#pragma once

#include <cstdint>
#include <string_view>

namespace penumbra_vr::runtime {

enum class VrTurnMode : std::uint8_t {
    disabled,
    snap,
    smooth,
};

enum class VrCrouchMode : std::uint8_t {
    physical,
    button,
    hybrid,
};

enum class VrHandedness : std::uint8_t {
    left,
    right,
};

enum class VrPlayMode : std::uint8_t {
    standing,
    seated,
};

enum class VrHrtfMode : std::uint8_t {
    automatic,
    on,
    off,
};

struct VrFloatSettingRange {
    float minimum;
    float maximum;
    float default_value;
};

namespace vr_setting_limits {

inline constexpr VrFloatSettingRange kMoveSpeed{0.25F, 3.0F, 1.0F};
inline constexpr VrFloatSettingRange kMoveDeadZone{0.0F, 0.9F, 0.15F};
inline constexpr VrFloatSettingRange kHeightOffset{-0.5F, 0.5F, 0.0F};
inline constexpr VrFloatSettingRange kSnapTurnAngle{15.0F, 90.0F, 45.0F};
inline constexpr VrFloatSettingRange kSmoothTurnSpeed{30.0F, 360.0F, 90.0F};
inline constexpr VrFloatSettingRange kTurnDeadZone{0.0F, 0.9F, 0.20F};
inline constexpr VrFloatSettingRange kUiDistance{0.75F, 3.0F, 1.75F};
inline constexpr VrFloatSettingRange kUiScale{0.5F, 2.0F, 1.0F};
inline constexpr VrFloatSettingRange kRenderScale{0.5F, 2.0F, 1.0F};
inline constexpr VrFloatSettingRange kPhysicalCrouchDepth{0.10F, 0.60F, 0.25F};
inline constexpr VrFloatSettingRange kSubtitleScale{0.75F, 2.0F, 1.35F};
inline constexpr VrFloatSettingRange kPlayerHeight{1.40F, 2.10F, 1.70F};

} // namespace vr_setting_limits

inline constexpr std::uint32_t kCurrentVrSettingsVersion = 1;

// Runtime-owned settings adapted from Overture Rework. Storage, user interface
// and application to an individual game remain adapter/backend concerns.
struct VrSettings {
    float move_speed = vr_setting_limits::kMoveSpeed.default_value;
    float move_dead_zone = vr_setting_limits::kMoveDeadZone.default_value;
    float height_offset = vr_setting_limits::kHeightOffset.default_value;
    VrTurnMode turn_mode = VrTurnMode::snap;
    float snap_turn_angle = vr_setting_limits::kSnapTurnAngle.default_value;
    float smooth_turn_speed = vr_setting_limits::kSmoothTurnSpeed.default_value;
    float turn_dead_zone = vr_setting_limits::kTurnDeadZone.default_value;
    float ui_distance = vr_setting_limits::kUiDistance.default_value;
    float ui_scale = vr_setting_limits::kUiScale.default_value;
    float render_scale = vr_setting_limits::kRenderScale.default_value;
    bool enhanced_visuals = false;
    bool monitor_mirror = false;
    VrCrouchMode crouch_mode = VrCrouchMode::hybrid;
    float physical_crouch_depth =
        vr_setting_limits::kPhysicalCrouchDepth.default_value;
    float subtitle_scale = vr_setting_limits::kSubtitleScale.default_value;
    VrHandedness handedness = VrHandedness::right;
    VrPlayMode play_mode = VrPlayMode::standing;
    float player_height = vr_setting_limits::kPlayerHeight.default_value;
    VrHrtfMode hrtf_mode = VrHrtfMode::automatic;
};

void NormalizeVrSettings(
    VrSettings& settings,
    std::uint32_t source_version = kCurrentVrSettingsVersion) noexcept;

[[nodiscard]] VrTurnMode ParseVrTurnMode(std::string_view value) noexcept;
[[nodiscard]] VrCrouchMode ParseVrCrouchMode(std::string_view value) noexcept;
[[nodiscard]] VrHandedness ParseVrHandedness(std::string_view value) noexcept;
[[nodiscard]] VrPlayMode ParseVrPlayMode(std::string_view value) noexcept;
[[nodiscard]] VrHrtfMode ParseVrHrtfMode(std::string_view value) noexcept;

[[nodiscard]] std::string_view ToConfigValue(VrTurnMode value) noexcept;
[[nodiscard]] std::string_view ToConfigValue(VrCrouchMode value) noexcept;
[[nodiscard]] std::string_view ToConfigValue(VrHandedness value) noexcept;
[[nodiscard]] std::string_view ToConfigValue(VrPlayMode value) noexcept;
[[nodiscard]] std::string_view ToConfigValue(VrHrtfMode value) noexcept;

} // namespace penumbra_vr::runtime
