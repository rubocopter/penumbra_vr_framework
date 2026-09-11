#pragma once

#include "vr_settings.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace penumbra_vr::runtime {

enum class VrSettingId : std::uint8_t {
    handedness,
    play_mode,
    player_height,
    turn_mode,
    snap_turn_angle,
    smooth_turn_speed,
    turn_dead_zone,
    move_speed,
    move_dead_zone,
    crouch_mode,
    physical_crouch_depth,
    height_offset,
    ui_distance,
    ui_scale,
    render_scale,
    enhanced_visuals,
    hrtf,
    subtitle_scale,
    count,
};

inline constexpr std::size_t kVrSettingCount =
    static_cast<std::size_t>(VrSettingId::count);

struct VrSettingDescriptor {
    VrSettingId id;
    std::string_view translation_key;
};

struct VrSettingCapabilities {
    std::array<bool, kVrSettingCount> supported{};
};

[[nodiscard]] VrSettingCapabilities AllVrSettingCapabilities() noexcept;
[[nodiscard]] const std::array<VrSettingDescriptor, kVrSettingCount>&
VrSettingDescriptors() noexcept;

// Capability is supplied by the owning backend. Runtime additionally applies
// the same dependent-row policy as Overture Rework (snap/smooth turn rows).
[[nodiscard]] bool IsVrSettingAvailable(
    VrSettingId id,
    const VrSettings& settings,
    const VrSettingCapabilities& capabilities) noexcept;

// Applies one Overture-Rework menu step. Direction is reduced to -1/+1 so a
// backend cannot accidentally multiply a single UI action into several steps.
[[nodiscard]] bool AdjustVrSetting(
    VrSettings& settings,
    VrSettingId id,
    int direction) noexcept;

// Restores only settings owned by the supplied backend capability surface.
// Persisted settings that a backend does not currently consume are preserved.
void ResetVrSettings(
    VrSettings& settings,
    const VrSettingCapabilities& capabilities) noexcept;

[[nodiscard]] std::string FormatVrSettingValue(
    VrSettingId id,
    const VrSettings& settings);

// Overture changes the crouch-depth label in physical mode while preserving
// the same setting/value and edit semantics.
[[nodiscard]] std::string_view VrSettingTranslationKey(
    VrSettingId id,
    const VrSettings& settings) noexcept;

} // namespace penumbra_vr::runtime
