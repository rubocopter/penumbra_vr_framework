#include "vr_settings_capabilities.hpp"

#include <cstddef>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::array<runtime::VrSettingId, kBlackPlagueVrMenuSettingCount>
    kMenuSettings{{
        runtime::VrSettingId::handedness,
        runtime::VrSettingId::play_mode,
        runtime::VrSettingId::player_height,
        runtime::VrSettingId::turn_mode,
        runtime::VrSettingId::snap_turn_angle,
        runtime::VrSettingId::smooth_turn_speed,
        runtime::VrSettingId::turn_dead_zone,
        runtime::VrSettingId::move_speed,
        runtime::VrSettingId::move_dead_zone,
        runtime::VrSettingId::crouch_mode,
        runtime::VrSettingId::physical_crouch_depth,
        runtime::VrSettingId::height_offset,
        runtime::VrSettingId::ui_distance,
        runtime::VrSettingId::ui_scale,
        runtime::VrSettingId::subtitle_scale,
        runtime::VrSettingId::render_scale,
        runtime::VrSettingId::hrtf,
    }};

} // namespace

const std::array<runtime::VrSettingId, kBlackPlagueVrMenuSettingCount>&
BlackPlagueVrMenuSettings() noexcept {
    return kMenuSettings;
}

runtime::VrSettingCapabilities BlackPlagueVrSettingCapabilities() noexcept {
    runtime::VrSettingCapabilities capabilities;
    for (const auto id : kMenuSettings) {
        capabilities.supported[static_cast<std::size_t>(id)] = true;
    }

    return capabilities;
}

} // namespace penumbra_vr::backends::black_plague
