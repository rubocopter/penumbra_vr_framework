#pragma once
#include "body_adapter_boundary.hpp"
#include "openvr_session.hpp"
#include "vr_crouch_policy.hpp"
#include "vr_update_timing.hpp"
#include "vr_settings.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace penumbra_vr::backends::black_plague {
struct BlackPlagueNativeCrouchStatus {
    runtime::VrPhysicalCrouchStatus policy{};
    bool native_shape_known = false;
    bool native_crouched = false;
    bool native_move_state_known = false;
    std::int32_t native_move_state = -1;
    bool vr_stance_owned = false;
    bool stand_blocked = false;
    std::uint64_t session_generation = 0;
    std::uint64_t player_generation = 0;
    std::uintptr_t owner_player = 0;
    std::uint64_t native_crouch_entries = 0;
    std::uint64_t native_crouch_exits = 0;
    std::uint64_t stand_retries = 0;
    std::uint64_t desired_shape_mismatch_frames = 0;
};

struct BlackPlagueCrouchOwnershipWindow {
    bool session_connected = false;
    bool ui_active = true;
    bool focused = false;
    bool disconnect_release_pending = false;
    bool player_available = false;
};

struct BlackPlagueCrouchEdgeToken {
    std::uint64_t session_generation = 0;
    std::uint64_t player_generation = 0;
};

[[nodiscard]] constexpr bool BlackPlagueVrOwnsCrouchQuery(
    const BlackPlagueCrouchOwnershipWindow& window) noexcept {
    return window.session_connected && !window.ui_active && window.focused &&
        !window.disconnect_release_pending && window.player_available;
}

[[nodiscard]] constexpr BlackPlagueCrouchEdgeToken
BlackPlagueNativeCrouchEdgeToken(
    bool native_pressed,
    bool query_owned_by_vr,
    bool duplicate_openvr_press,
    std::uint64_t session_generation,
    std::uint64_t player_generation) noexcept {
    return native_pressed && query_owned_by_vr && !duplicate_openvr_press
        ? BlackPlagueCrouchEdgeToken{session_generation, player_generation}
        : BlackPlagueCrouchEdgeToken{};
}

[[nodiscard]] constexpr bool BlackPlagueConsumeCrouchEdge(
    const BlackPlagueCrouchEdgeToken& pending,
    std::uint64_t current_session_generation,
    std::uint64_t current_player_generation) noexcept {
    return pending.session_generation != 0 && pending.player_generation != 0 &&
        pending.session_generation == current_session_generation &&
        pending.player_generation == current_player_generation;
}

[[nodiscard]] bool InstallNativeInputBridge(std::string& error) noexcept;
void ConfigureNativeInputBridge(runtime::VrSettings settings) noexcept;
[[nodiscard]] bool RemoveNativeInputBridge(std::string& error) noexcept;
void ConnectNativeInput(runtime::OpenVrSession* session) noexcept;
[[nodiscard]] bool NativeInputUiActive() noexcept;
[[nodiscard]] runtime::VrControllerFrame ReadNativeControllerFrame() noexcept;
[[nodiscard]] BlackPlagueNativeCrouchStatus
ReadNativePhysicalCrouchStatus() noexcept;
// Last cPlayer observed at the exact-build ButtonHandler boundary. Consumers
// may compare it with native object links, but must not call game methods from
// a non-game thread.
[[nodiscard]] void* NativePlayerPointer() noexcept;
[[nodiscard]] std::uint64_t NativePlayerGeneration() noexcept;
void NativeControllerHaptic(runtime::VrHand hand, bool pickup) noexcept;
[[nodiscard]] runtime::VrUpdateTimingSample ConsumeNativeUpdateTiming() noexcept;
// The input bridge is the sole owner of MoveForward/MoveSideways callsites.
// Consumers use this status to bind fan-out behavior without re-patching them.
[[nodiscard]] NativeMovementBoundaryStatus
ReadNativeMovementBoundaryStatus() noexcept;
}
