#pragma once
#include "vr_settings_editor.hpp"
#include <array>

namespace penumbra_vr::backends::requiem {

inline constexpr std::size_t kRequiemVrMenuSettingCount = 9;
inline const std::array<runtime::VrSettingId, kRequiemVrMenuSettingCount>&
RequiemVrMenuSettings() noexcept {
    using I = runtime::VrSettingId;
    static constexpr std::array<I, kRequiemVrMenuSettingCount> settings{
        I::play_mode, I::player_height, I::turn_mode, I::snap_turn_angle,
        I::smooth_turn_speed, I::turn_dead_zone, I::crouch_mode,
        I::physical_crouch_depth, I::height_offset};
    return settings;
}

inline runtime::VrSettingCapabilities RequiemVrSettingCapabilities() noexcept {
    runtime::VrSettingCapabilities result;
    for (const auto id : RequiemVrMenuSettings())
        result.supported[static_cast<std::size_t>(id)] = true;
    return result;
}

} // namespace penumbra_vr::backends::requiem
