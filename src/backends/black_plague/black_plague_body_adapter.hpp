#pragma once

#include "vr_locomotion.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

// Exact-build boundary for the native Black Plague character body.  It only
// forwards horizontal intent through cPlayer::MoveForward/MoveSideways and
// observes the result around the game's existing D460A -> D6E00 tick.  It
// never invokes iCharacterBody::Update itself.
struct BlackPlagueBodyMotion {
    bool valid = false;
    bool intent_published = false;
    std::uint64_t native_tick_sequence = 0;
    std::array<float, 2> native_horizontal_intent{};
    std::array<float, 3> feet_after{};
    runtime::VrAcceptedBodyMotion accepted{};
};

[[nodiscard]] bool InstallBlackPlagueBodyAdapter(std::string& error) noexcept;
[[nodiscard]] bool RemoveBlackPlagueBodyAdapter(std::string& error) noexcept;

// These are called from the two already-mapped cButtonHandler::Update
// callsites.  They preserve the native amount/timestep and return false so
// callers can fall back to the untouched native target if the adapter is not
// active or the player/body chain has changed.
[[nodiscard]] bool PublishBlackPlagueForwardIntent(
    void* player, float amount, float delta_seconds) noexcept;
[[nodiscard]] bool PublishBlackPlagueSidewaysIntent(
    void* player, float amount, float delta_seconds) noexcept;

// Called by the existing D460A observation wrapper immediately after the one
// native Update call returns.  No game memory is written here.
void ObserveBlackPlagueNativeBodyTick(
    void* player,
    void* character_body,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    const std::array<float, 3>& feet_after) noexcept;
[[nodiscard]] BlackPlagueBodyMotion ConsumeBlackPlagueBodyMotion() noexcept;

} // namespace penumbra_vr::backends::black_plague
