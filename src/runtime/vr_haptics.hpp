#pragma once

#include <cstddef>
#include <cstdint>

namespace penumbra_vr::runtime {

// Gameplay-level haptic meanings and their proven Overture Rework 23c890f
// profiles. Backends still own device availability, pose validity and the
// actual OpenVR/native submission boundary.
enum class VrHapticEvent : std::uint8_t {
    ui_select,
    object_pickup,
    object_drop,
    interaction,
    light_toggle,
    melee_impact,
    damage,
    count,
};

struct VrHapticProfile {
    float duration_seconds = 0.0F;
    float frequency_hz = 0.0F;
    float amplitude = 0.0F;
    std::uint32_t min_interval_ms = 0;
    const char* name = "unknown";
};

inline constexpr std::size_t kVrHapticEventCount =
    static_cast<std::size_t>(VrHapticEvent::count);

[[nodiscard]] constexpr bool IsKnownHapticEvent(VrHapticEvent event) noexcept {
    return static_cast<std::size_t>(event) < kVrHapticEventCount;
}

[[nodiscard]] constexpr std::size_t HapticEventIndex(VrHapticEvent event) noexcept {
    return static_cast<std::size_t>(event);
}

[[nodiscard]] constexpr VrHapticProfile HapticProfile(VrHapticEvent event) noexcept {
    switch (event) {
    case VrHapticEvent::ui_select:
        return {0.025F, 120.0F, 0.18F, 60, "ui_select"};
    case VrHapticEvent::object_pickup:
        return {0.045F, 90.0F, 0.30F, 80, "object_pickup"};
    case VrHapticEvent::object_drop:
        return {0.025F, 80.0F, 0.20F, 80, "object_drop"};
    case VrHapticEvent::interaction:
        return {0.040F, 95.0F, 0.28F, 100, "interaction"};
    case VrHapticEvent::light_toggle:
        return {0.035F, 100.0F, 0.24F, 150, "light_toggle"};
    case VrHapticEvent::melee_impact:
        return {0.070F, 65.0F, 0.75F, 250, "melee_impact"};
    case VrHapticEvent::damage:
        return {0.120F, 45.0F, 1.00F, 200, "damage"};
    default:
        return {0.030F, 90.0F, 0.20F, 100, "unknown"};
    }
}

[[nodiscard]] constexpr float ClampHapticStrength(float value) noexcept {
    if (value < 0.0F) return 0.0F;
    if (value > 1.0F) return 1.0F;
    return value;
}

[[nodiscard]] constexpr float ScaleHapticAmplitude(
    VrHapticEvent event,
    float strength) noexcept {
    return HapticProfile(event).amplitude * ClampHapticStrength(strength);
}

[[nodiscard]] constexpr bool HapticCooldownReady(
    VrHapticEvent event,
    bool has_submitted,
    std::uint32_t last_submission_ms,
    std::uint32_t now_ms) noexcept {
    return !has_submitted ||
           static_cast<std::uint32_t>(now_ms - last_submission_ms) >=
               HapticProfile(event).min_interval_ms;
}

} // namespace penumbra_vr::runtime
