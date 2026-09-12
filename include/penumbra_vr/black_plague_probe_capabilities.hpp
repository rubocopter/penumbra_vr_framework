#pragma once

#include <cstdint>

namespace penumbra_vr {

enum class BlackPlagueProbeCapability : std::uint32_t {
    matrix_telemetry = 1U << 0,
    render_world = 1U << 1,
    frame_hook = 1U << 2,
    native_input = 1U << 3,
    body_collision = 1U << 4,
    body_adapter = 1U << 5,
    movement_ownership = 1U << 6,
    spatial_interaction = 1U << 7,
};

[[nodiscard]] constexpr std::uint32_t BlackPlagueProbeCapabilityMask(
    BlackPlagueProbeCapability capability) noexcept {
    return static_cast<std::uint32_t>(capability);
}

[[nodiscard]] constexpr bool HasBlackPlagueProbeCapability(
    std::uint32_t capabilities,
    BlackPlagueProbeCapability capability) noexcept {
    return (capabilities & BlackPlagueProbeCapabilityMask(capability)) != 0;
}

inline constexpr std::uint32_t kBlackPlagueProbeRequiredCapabilities =
    BlackPlagueProbeCapabilityMask(BlackPlagueProbeCapability::matrix_telemetry) |
    BlackPlagueProbeCapabilityMask(BlackPlagueProbeCapability::render_world) |
    BlackPlagueProbeCapabilityMask(BlackPlagueProbeCapability::frame_hook);

} // namespace penumbra_vr
