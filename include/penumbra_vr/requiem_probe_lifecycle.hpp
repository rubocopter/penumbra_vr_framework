#pragma once

#include <cstdint>

namespace penumbra_vr {

[[nodiscard]] constexpr bool RequiemOpenVrBootstrapReady(
    std::uint64_t completed_swaps) noexcept {
    // Delay OpenVR until two native SDL swaps have completed. This mirrors the
    // Black Plague bootstrap boundary; whether it affects Requiem's intermittent
    // SDL_mutexP startup fault still requires runtime evidence.
    return completed_swaps >= 2;
}

} // namespace penumbra_vr
