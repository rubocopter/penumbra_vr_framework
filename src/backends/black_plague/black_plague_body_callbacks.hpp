#pragma once

#include <array>
#include <cstdint>

namespace penumbra_vr::backends::black_plague {

struct BlackPlaguePhysicalTickObservation {
    bool request_injected = false;
    std::array<float, 3> requested_displacement{};
    std::array<float, 3> position_before_injection{};
    std::array<float, 3> position_after_injection{};
};

// Native input calls these from the two already-mapped
// cButtonHandler::Update callsites. They preserve the native amount/timestep
// and return false so the owner can use the untouched native target when the
// adapter is inactive or the player/body chain changed. The body-update owner
// publishes its post-original observation through the final callback below.
// The adapter never installs a competing hook over either exact-build site.
[[nodiscard]] bool PublishBlackPlagueForwardIntent(
    void* player, float amount, float delta_seconds) noexcept;
[[nodiscard]] bool PublishBlackPlagueSidewaysIntent(
    void* player, float amount, float delta_seconds) noexcept;
// Called immediately after the one native D460A -> D6E00 update returns. No
// game memory is written here.
void ObserveBlackPlagueNativeBodyTick(
    void* player,
    void* character_body,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    const std::array<float, 3>& feet_after,
    float delta_seconds,
    const BlackPlaguePhysicalTickObservation& physical_tick = {}) noexcept;

} // namespace penumbra_vr::backends::black_plague
