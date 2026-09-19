#pragma once

#include "penumbra_vr/black_plague_probe_capabilities.hpp"

#include <array>
#include <cstdint>

namespace penumbra_vr {

enum class BlackPlagueProbeLifecycle : std::uint32_t {
    clean = 0,
    initializing = 1,
    ready = 2,
    shutting_down = 3,
    partial = 4,
};

[[nodiscard]] constexpr bool BlackPlagueProbeCallbacksAllowed(
    BlackPlagueProbeLifecycle lifecycle) noexcept {
    return lifecycle == BlackPlagueProbeLifecycle::ready;
}

[[nodiscard]] constexpr bool BlackPlagueProbeCanBeginShutdown(
    BlackPlagueProbeLifecycle lifecycle) noexcept {
    return lifecycle == BlackPlagueProbeLifecycle::ready ||
        lifecycle == BlackPlagueProbeLifecycle::partial;
}

[[nodiscard]] constexpr bool BlackPlagueProbeRequiredSetInstalled(
    std::uint32_t capabilities) noexcept {
    return (capabilities & kBlackPlagueProbeRequiredCapabilities) ==
        kBlackPlagueProbeRequiredCapabilities;
}

[[nodiscard]] constexpr BlackPlagueProbeLifecycle
BlackPlagueProbeStateAfterTeardown(
    std::uint32_t capabilities,
    std::uint32_t cleanup_ledger) noexcept {
    return capabilities == 0 && cleanup_ledger == 0
        ? BlackPlagueProbeLifecycle::clean
        : BlackPlagueProbeLifecycle::partial;
}

// Bootstrap is the deliberate exception to dependency-first installation: the
// lifecycle-gated SDL frame owner is installed first so the probe can observe a
// completed native SwapBuffers before touching OpenGL/RenderWorld callsites.
// Once that readiness gate passes, installation proceeds through the remaining
// dependencies toward callback producers. A component is entered in the cleanup
// ledger before its installer runs, so a failed installer can still leave
// teardown work even when its capability bit was never published.
inline constexpr std::array<BlackPlagueProbeCapability, 10>
    kBlackPlagueProbeInstallOrder{
        BlackPlagueProbeCapability::frame_hook,
        BlackPlagueProbeCapability::matrix_telemetry,
        BlackPlagueProbeCapability::render_world,
        BlackPlagueProbeCapability::native_input,
        BlackPlagueProbeCapability::vr_settings_menu,
        BlackPlagueProbeCapability::body_collision,
        BlackPlagueProbeCapability::body_adapter,
        BlackPlagueProbeCapability::movement_ownership,
        BlackPlagueProbeCapability::spatial_interaction,
        BlackPlagueProbeCapability::audio_environment,
    };

// The probe removes callback producers before every dependency they can call.
// This table is also the durable retry order after a partial teardown.
inline constexpr std::array<BlackPlagueProbeCapability, 10>
    kBlackPlagueProbeTeardownOrder{
        BlackPlagueProbeCapability::audio_environment,
        BlackPlagueProbeCapability::spatial_interaction,
        BlackPlagueProbeCapability::movement_ownership,
        BlackPlagueProbeCapability::body_adapter,
        BlackPlagueProbeCapability::body_collision,
        BlackPlagueProbeCapability::vr_settings_menu,
        BlackPlagueProbeCapability::native_input,
        BlackPlagueProbeCapability::frame_hook,
        BlackPlagueProbeCapability::render_world,
        BlackPlagueProbeCapability::matrix_telemetry,
    };

[[nodiscard]] constexpr const char* BlackPlagueProbeLifecycleName(
    BlackPlagueProbeLifecycle lifecycle) noexcept {
    switch (lifecycle) {
        case BlackPlagueProbeLifecycle::clean: return "clean";
        case BlackPlagueProbeLifecycle::initializing: return "initializing";
        case BlackPlagueProbeLifecycle::ready: return "ready";
        case BlackPlagueProbeLifecycle::shutting_down: return "shutting_down";
        case BlackPlagueProbeLifecycle::partial: return "partial";
    }
    return "unknown";
}

} // namespace penumbra_vr
