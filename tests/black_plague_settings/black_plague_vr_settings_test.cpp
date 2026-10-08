#include "vr_settings_capabilities.hpp"
#include "ui_presentation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <iostream>

namespace {

using penumbra_vr::backends::black_plague::BlackPlagueVrSettingCapabilities;
using penumbra_vr::backends::black_plague::BlackPlagueVrMenuSettings;
using penumbra_vr::backends::black_plague::BlackPlagueCalibrationPoseUsable;
using penumbra_vr::backends::black_plague::BlackPlagueVrMenuNeedsRebind;
using penumbra_vr::backends::black_plague::BlackPlagueVrMenuNeedsRebindOnStateTransition;
using penumbra_vr::backends::black_plague::TryCalibratePlayerHeight;
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
    enable(VrSettingId::play_mode);
    enable(VrSettingId::player_height);
    enable(VrSettingId::height_offset);
    enable(VrSettingId::crouch_mode);
    enable(VrSettingId::physical_crouch_depth);
    enable(VrSettingId::ui_distance);
    enable(VrSettingId::ui_scale);
    enable(VrSettingId::subtitle_scale);
    enable(VrSettingId::render_scale);
    enable(VrSettingId::hrtf);

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
        IsVrSettingAvailable(VrSettingId::play_mode, settings, capabilities) &&
        IsVrSettingAvailable(VrSettingId::player_height, settings, capabilities) &&
        IsVrSettingAvailable(VrSettingId::crouch_mode, settings, capabilities) &&
        IsVrSettingAvailable(VrSettingId::physical_crouch_depth, settings, capabilities) &&
        !IsVrSettingAvailable(VrSettingId::enhanced_visuals, settings, capabilities) &&
        IsVrSettingAvailable(VrSettingId::hrtf, settings, capabilities);
}

[[nodiscard]] bool TestNativeMenuUsesExactCapabilitySurface() {
    const auto capabilities = BlackPlagueVrSettingCapabilities();
    const auto& rows = BlackPlagueVrMenuSettings();
    if (rows.size() != 17) return false;

    std::array<bool, penumbra_vr::runtime::kVrSettingCount> seen{};
    for (const auto id : rows) {
        const auto index = static_cast<std::size_t>(id);
        if (index >= seen.size() || seen[index] ||
            !capabilities.supported[index]) {
            return false;
        }
        seen[index] = true;
    }
    return seen == capabilities.supported;
}

[[nodiscard]] bool TestReworkInventoryAndSubtitleGeometry() {
    using namespace penumbra_vr::backends::black_plague;
    const auto inventory = BlackPlagueInventoryPanel(1.0F);
    const auto notebook = BlackPlagueNotebookPanel(1.0F);
    const auto fullscreen = BlackPlagueFullscreenPanel(1.0F, 1.75F);
    const auto subtitles = BlackPlagueGameplayOverlay(1.35F,1.75F);
    return std::abs(inventory.distance - 1.1F) < 0.0001F &&
        std::abs(inventory.width - 800.0F / 750.0F) < 0.0001F &&
        std::abs(inventory.center_y + 100.0F / 750.0F) < 0.0001F &&
        std::abs(notebook.width - 800.0F / 1450.0F) < 0.0001F &&
        std::abs(fullscreen.width - 2.4F) < 0.0001F &&
        std::abs(subtitles.left + 400.0F / 750.0F * 1.35F) < 0.0001F &&
        std::abs(subtitles.distance - 1.75F) < 0.0001F &&
        std::abs(subtitles.top + subtitles.bottom) < 0.0001F;
}

[[nodiscard]] bool TestWorldPanelClosingFrame() {
    using penumbra_vr::backends::black_plague::BlackPlaguePreserveWorldPanelOnExit;
    return BlackPlaguePreserveWorldPanelOnExit(false,false,true) &&
        !BlackPlaguePreserveWorldPanelOnExit(true,true,true) &&
        !BlackPlaguePreserveWorldPanelOnExit(false,true,true) &&
        !BlackPlaguePreserveWorldPanelOnExit(false,false,false);
}

[[nodiscard]] bool TestPlayerHeightCalibrationPolicy() {
    VrSettings settings;
    settings.player_height = 1.72F;
    if (!TryCalibratePlayerHeight(settings, 1.83F) ||
        std::abs(settings.player_height - 1.83F) > 0.0001F) {
        return false;
    }
    if (!TryCalibratePlayerHeight(settings, 2.15F) ||
        std::abs(settings.player_height - 2.10F) > 0.0001F) {
        return false;
    }

    settings.player_height = 1.77F;
    if (TryCalibratePlayerHeight(settings, 0.90F) ||
        TryCalibratePlayerHeight(settings, 2.20F) ||
        TryCalibratePlayerHeight(settings,
            std::numeric_limits<float>::quiet_NaN()) ||
        std::abs(settings.player_height - 1.77F) > 0.0001F) {
        return false;
    }
    return true;
}

[[nodiscard]] bool TestNativeMenuRebindPolicy() {
    return BlackPlagueVrMenuNeedsRebind(false, false) &&
        !BlackPlagueVrMenuNeedsRebind(true, true) &&
        BlackPlagueVrMenuNeedsRebind(true, false);
}

[[nodiscard]] bool TestNativeMenuRebindUsesSetStateDestination() {
    constexpr int kStartState = 0;
    return BlackPlagueVrMenuNeedsRebindOnStateTransition(
               kStartState, false, false) &&
        BlackPlagueVrMenuNeedsRebindOnStateTransition(
            kStartState, true, false) &&
        !BlackPlagueVrMenuNeedsRebindOnStateTransition(
            kStartState, true, true) &&
        !BlackPlagueVrMenuNeedsRebindOnStateTransition(8, false, false) &&
        !BlackPlagueVrMenuNeedsRebindOnStateTransition(7, false, false);
}

[[nodiscard]] bool TestCalibrationCanUseLastValidTrackedPose() {
    constexpr std::uint64_t kFreshMenuAgeMs = 100;
    return BlackPlagueCalibrationPoseUsable(true, 1.78F, kFreshMenuAgeMs) &&
        BlackPlagueCalibrationPoseUsable(true, 1.78F,
            penumbra_vr::backends::black_plague::kBlackPlagueCalibrationMaxPoseAgeMs) &&
        !BlackPlagueCalibrationPoseUsable(true, 1.78F,
            penumbra_vr::backends::black_plague::kBlackPlagueCalibrationMaxPoseAgeMs + 1) &&
        !BlackPlagueCalibrationPoseUsable(false, 1.78F, 0) &&
        !BlackPlagueCalibrationPoseUsable(
            true, std::numeric_limits<float>::quiet_NaN(), kFreshMenuAgeMs);
}

[[nodiscard]] bool TestNativeMenuRecreationReusesRootAddress() {
    using namespace penumbra_vr::backends::black_plague;
    int root_storage = 0;
    int button_vtable = 0;
    int other_vtable = 0;
    const auto retain = [&](const void* live, const void* vtable, int target) {
        return BlackPlagueVrMenuRootBindingMatches(
            &root_storage, live, vtable, &button_vtable, target);
    };
    if (!retain(&root_storage, &button_vtable, kBlackPlagueVrMenuRootSentinel))
        return false;
    // CreateWidgets frees the root and constructs a normal Start button in
    // the same allocation. Membership alone must not preserve stale rows.
    if (retain(&root_storage, &button_vtable, 0) ||
        retain(&root_storage, &other_vtable, kBlackPlagueVrMenuRootSentinel) ||
        retain(nullptr, &button_vtable, kBlackPlagueVrMenuRootSentinel))
        return false;
    return BlackPlagueVrMenuNeedsRebindOnStateTransition(
        0, retain(&root_storage, &button_vtable, 0), true);
}

} // namespace

int main() {
    if (!TestExactBackendCapabilitySet()) return 1;
    if (!TestDependentTurnRowsRemainRuntimeOwned()) return 2;
    if (!TestNativeMenuUsesExactCapabilitySurface()) return 3;
    if (!TestReworkInventoryAndSubtitleGeometry()) return 4;
    if (!TestWorldPanelClosingFrame()) return 5;
    if (!TestPlayerHeightCalibrationPolicy()) return 6;
    if (!TestNativeMenuRebindPolicy()) return 7;
    if (!TestNativeMenuRebindUsesSetStateDestination()) return 8;
    if (!TestCalibrationCanUseLastValidTrackedPose()) return 9;
    if (!TestNativeMenuRecreationReusesRootAddress()) return 10;
    std::cout << "Black Plague VR settings expose only backend-wired capabilities\n";
    return 0;
}
