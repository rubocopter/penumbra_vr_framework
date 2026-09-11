#include "vr_settings_capabilities.hpp"

#include <cstddef>

namespace penumbra_vr::backends::black_plague {

runtime::VrSettingCapabilities BlackPlagueVrSettingCapabilities() noexcept {
    runtime::VrSettingCapabilities capabilities;

    const auto enable = [&capabilities](runtime::VrSettingId id) noexcept {
        capabilities.supported[static_cast<std::size_t>(id)] = true;
    };

    enable(runtime::VrSettingId::handedness);
    enable(runtime::VrSettingId::turn_mode);
    enable(runtime::VrSettingId::snap_turn_angle);
    enable(runtime::VrSettingId::smooth_turn_speed);
    enable(runtime::VrSettingId::turn_dead_zone);
    enable(runtime::VrSettingId::move_speed);
    enable(runtime::VrSettingId::move_dead_zone);
    enable(runtime::VrSettingId::ui_distance);
    enable(runtime::VrSettingId::ui_scale);
    enable(runtime::VrSettingId::render_scale);

    return capabilities;
}

} // namespace penumbra_vr::backends::black_plague
