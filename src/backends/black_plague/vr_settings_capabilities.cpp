#include "vr_settings_capabilities.hpp"
#include "../../adapters/hpl1/native_vr_menu_policy.hpp"

#include <cmath>
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

bool TryCalibratePlayerHeight(
    runtime::VrSettings& settings,
    float tracked_height) noexcept {
    if (!std::isfinite(tracked_height) ||
        tracked_height <= 0.90F || tracked_height >= 2.20F) {
        return false;
    }

    settings.player_height = tracked_height;
    runtime::NormalizeVrSettings(settings);
    return true;
}

bool BlackPlagueVrMenuNeedsRebind(
    bool has_bound_root,
    bool same_menu) noexcept {
    return !has_bound_root || !same_menu;
}

bool BlackPlagueVrMenuRootBindingMatches(
    const void* bound_root, const void* live_widget,
    const void* widget_vtable, const void* native_button_vtable,
    int target_state) noexcept {
    return adapters::hpl1::NativeVrMenuRootBindingMatches(
        bound_root, live_widget, widget_vtable, native_button_vtable, target_state);
}

bool BlackPlagueVrMenuNeedsRebindOnStateTransition(
    int next_state,
    bool has_bound_root,
    bool same_menu) noexcept {
    return adapters::hpl1::NativeVrMenuNeedsRebindOnStateTransition(
        next_state, has_bound_root, same_menu);
}

bool BlackPlagueCalibrationPoseUsable(
    bool has_sample,
    float tracked_height,
    std::uint64_t age_ms) noexcept {
    return has_sample && std::isfinite(tracked_height) &&
        age_ms <= kBlackPlagueCalibrationMaxPoseAgeMs;
}

} // namespace penumbra_vr::backends::black_plague
