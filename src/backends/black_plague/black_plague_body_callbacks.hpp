#pragma once

#include <array>
#include <cstdint>

namespace penumbra_vr::backends::black_plague {

struct BlackPlaguePhysicalTickObservation {
    bool request_injected = false;
    std::array<float, 3> physical_requested_displacement{};
    std::array<float, 3> locomotion_requested_displacement{};
    std::array<float, 3> physical_accepted_displacement{};
    std::array<float, 3> locomotion_accepted_displacement{};
    std::array<float, 3> position_before_injection{};
    std::array<float, 3> position_after_injection{};
};

struct BlackPlagueDirectLocomotionIntent {
    std::array<float, 2> move{};
    std::array<float, 16> head_world_pose{};
    float move_scale = 1.0F;
    bool constrained = false;
    bool sprinting = false;
    std::uint64_t player_generation = 0;
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
// Available only for the transient active room-scale gate. The input owner
// publishes logical analog state and the current head transform; the existing
// body-tick owner converts it to metric displacement with physics delta-time.
[[nodiscard]] bool BlackPlagueDirectLocomotionAvailable(void* player) noexcept;
[[nodiscard]] bool PublishBlackPlagueDirectLocomotionIntent(
    void* player,
    const BlackPlagueDirectLocomotionIntent& intent) noexcept;
void InvalidateBlackPlagueDirectLocomotionIntent() noexcept;
// Called by the existing D460A owner before its one native D6E00 update. This
// resolves the current body generation and plans/queues the same-tick physical
// request from B0 and the latest published tracking sample.
void PrepareBlackPlagueNativeBodyTick(
    void* player,
    void* character_body,
    const std::array<float, 3>& body_before,
    float delta_seconds,
    std::uint64_t tick_sequence,
    std::uint64_t player_generation) noexcept;
// Called immediately after the one native D460A -> D6E00 update returns. No
// game memory is written here.
void ObserveBlackPlagueNativeBodyTick(
    void* player,
    void* character_body,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    const std::array<float, 3>& feet_after,
    std::uint64_t tick_sequence,
    const BlackPlaguePhysicalTickObservation& physical_tick = {}) noexcept;

} // namespace penumbra_vr::backends::black_plague
