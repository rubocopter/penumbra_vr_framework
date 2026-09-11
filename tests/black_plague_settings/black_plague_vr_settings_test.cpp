#include "vr_settings_capabilities.hpp"

#include <array>
#include <cstddef>
#include <iostream>

namespace {

using penumbra_vr::backends::black_plague::BlackPlagueVrSettingCapabilities;
using penumbra_vr::runtime::IsVrSettingAvailable;
using penumbra_vr::runtime::VrSettingId;
using penumbra_vr::runtime::VrSettings;
using penumbra_vr::runtime::VrTurnMode;

[[nodiscard]] bool TestExactBackendCapabilitySet() {
    const auto capabilities = BlackPlagueVrSettingCapabilities();
    std::array<bool, penumbra_vr::runtime::kVrSettingCount> expected{};

    const auto enable = [&expected](VrSettingId id) noexcept {
        expected[static_cast<std::size_t>(id)] = true;
    };

    enable(VrSettingId::handedness);
    enable(VrSettingId::turn_mode);
    enable(VrSettingId::snap_turn_angle);
    enable(VrSettingId::smooth_turn_speed);
    enable(VrSettingId::turn_dead_zone);
    enable(VrSettingId::move_speed);
    enable(VrSettingId::move_dead_zone);
    enable(VrSettingId::ui_distance);
    enable(VrSettingId::ui_scale);
    enable(VrSettingId::render_scale);

    return capabilities.supported == expected;
}

[[nodiscard]] bool TestDependentTurnRowsRemainRuntimeOwned() {
    const auto capabilities = BlackPlagueVrSettingCapabilities();
    VrSettings settings;

    if (!IsVrSettingAvailable(VrSettingId::snap_turn_angle, settings, capabilities) ||
        IsVrSettingAvailable(VrSettingId::smooth_turn_speed, settings, capabilities)) {
        return false;
    }

    settings.turn_mode = VrTurnMode::smooth;
    return !IsVrSettingAvailable(VrSettingId::snap_turn_angle, settings, capabilities) &&
        IsVrSettingAvailable(VrSettingId::smooth_turn_speed, settings, capabilities) &&
        !IsVrSettingAvailable(VrSettingId::play_mode, settings, capabilities) &&
        !IsVrSettingAvailable(VrSettingId::crouch_mode, settings, capabilities) &&
        !IsVrSettingAvailable(VrSettingId::hrtf, settings, capabilities);
}

} // namespace

int main() {
    if (!TestExactBackendCapabilitySet()) return 1;
    if (!TestDependentTurnRowsRemainRuntimeOwned()) return 2;
    std::cout << "Black Plague VR settings expose only backend-wired capabilities\n";
    return 0;
}
