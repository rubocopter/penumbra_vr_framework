#include "native_input_bridge.hpp"
#include "black_plague_body_callbacks.hpp"
#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#include "vr_haptics.hpp"
#include "vr_hand_pose.hpp"
#include "vr_locomotion.hpp"
#include "vr_native_intents.hpp"
#include "legacy_input_abi.hpp"
#include "render_world_probe.hpp"
#include "spatial_interaction.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {
runtime::VrUpdateTiming g_update_timing;
runtime::VrUpdateTimingSample g_timing_sample;
runtime::VrSettings g_settings;
std::atomic<runtime::VrHandedness> g_dominant_handedness{
    runtime::VrHandedness::right};
using A = runtime::NativeVrAction;
using Q = runtime::NativeVrQuery;
struct Entry { std::uintptr_t site; A action; Q query = Q::pressed; };
constexpr std::uintptr_t kInventoryLeftPressSite = 0x4CF2;
constexpr std::uintptr_t kInventoryLeftConsumeSite = 0x4D22;
constexpr std::uintptr_t kInventoryDoubleQuerySite = 0x4D43;
constexpr std::uintptr_t kInventoryLeftReleaseSite = 0x4D73;
constexpr std::uintptr_t kInventoryDoubleQueryRva = 0xDA650;
constexpr std::uintptr_t kInventoryRightPressSite = 0x4DA3;
constexpr std::uintptr_t kInventoryRightGuardRva = 0x6EEC8;
constexpr std::array<std::uint8_t, 2> kInventoryRightGuardStock{0x74, 0x7B};
constexpr std::array<std::uint8_t, 2> kInventoryRightGuardEnabled{0x90, 0x90};
// Exact initialized FD316F... image. Each entry is a decoded E8 instruction
// inside cButtonHandler::Update; do not extend by scanning arbitrary byte hits.
constexpr Entry kQueries[] = {
    {0x3DEC,A::pause},{0x3EA0,A::pause},{0x3FC9,A::pause},{0x4065,A::pause},
    {0x40E7,A::pause},{0x42C4,A::pause},{0x449D,A::pause},{0x47C9,A::pause},
    {0x4838,A::pause},{0x4992,A::pause},{0x49FF,A::pause},{0x4B4C,A::pause},
    {0x4CC4,A::pause},{0x500F,A::pause},
    {0x3E1A,A::select},{0x3ECE,A::select},{0x3F57,A::select},{0x3FF7,A::select},
    {0x40A5,A::select},{0x41C6,A::select},{0x43A3,A::select},{0x46B5,A::select},
    {0x48C6,A::select},{0x4A5B,A::select},{0x4B7A,A::select},{kInventoryLeftPressSite,A::drag},
    {0x41F6,A::select},{0x43D3,A::select},{0x46DE,A::select},{0x48F6,A::select},
    {0x4A8B,A::select},{0x4BAA,A::select},{kInventoryLeftConsumeSite,A::drag},
    {0x4212,A::select,Q::released},{0x43EF,A::select,Q::released},
    {0x46FA,A::select,Q::released},{0x4912,A::select,Q::released},
    {0x4AA7,A::select,Q::released},{kInventoryLeftReleaseSite,A::drag,Q::released},
    {0x3E4A,A::back},{0x3EFE,A::back},{0x3F29,A::back},{0x3F87,A::back},
    {0x4027,A::back},{0x4085,A::back},{0x4115,A::back},{0x42F2,A::back},
    {0x4619,A::back},{0x487A,A::back},{0x4A2D,A::back},{0x4DA3,A::back},
    {0x4145,A::back},{0x4322,A::back},{0x4642,A::back},{0x48AA,A::back},{0x4DD3,A::back},
    {0x4161,A::back,Q::released},{0x433E,A::back,Q::released},
    {0x465E,A::back,Q::released},{0x4DEF,A::back,Q::released},
    {0x5096,A::inventory},{0x50BE,A::notebook},{0x5131,A::light},
    {0x5299,A::jump},{0x52C1,A::jump,Q::held},{0x56E9,A::jump,Q::held},
    {0x52EB,A::sprint},{0x5313,A::sprint,Q::released},
    {0x533B,A::crouch},{0x536A,A::crouch,Q::released},{0x537E,A::crouch,Q::held},
    {0x542A,A::interact},{0x546E,A::interact,Q::released},
    {0x5496,A::examine},{0x54BE,A::examine,Q::released},{0x54E6,A::holster}
};
constexpr std::uintptr_t Target(Q query) {
    return query == Q::held ? 0xDA470 : query == Q::released ? 0xDA510 : 0xDA5B0;
}
std::array<std::uint8_t, 5> CallBytes(std::uintptr_t site, std::uintptr_t target) {
    std::array<std::uint8_t, 5> bytes{0xE8};
    const auto displacement = static_cast<std::int32_t>(target - site - 5);
    std::memcpy(bytes.data() + 1, &displacement, 4); return bytes;
}
// MSVC 2003 std::string is passed by value as 28 stack bytes. Never construct
// or destroy it with the framework's modern CRT: the original query owns it.
using LegacyString = adapters::hpl1::LegacyInputString;
using Query = adapters::hpl1::LegacyInputQuery;
using DoubleQuery = bool(__thiscall*)(void*, LegacyString, float);
using Update = void(__thiscall*)(void*, float);
using Move = void(__thiscall*)(void*, float, float);
using MoveStateGate = bool(__thiscall*)(void*, float, float);
using ChangeMoveState = void(__thiscall*)(void*, std::int32_t, bool);
constexpr std::uintptr_t kPlayerCharacterBodyOffset = 0x274;
constexpr std::uintptr_t kCharacterSizeYOffset = 0xC8;
constexpr std::uintptr_t kPlayerActionStateIndexOffset = 0x2BC;
constexpr std::uintptr_t kPlayerMoveStateIndexOffset = 0x2D0;
constexpr std::uintptr_t kChangeMoveStateRva = 0x9C750;
constexpr std::uintptr_t kPlayerDamageRva = 0x9BB80;
constexpr std::array<std::uintptr_t, 10> kPlayerDamageCallsites{
    0x26EB, 0x2F4D, 0x5058, 0x17054, 0x21CD9,
    0x42B18, 0x47C8B, 0xA5507, 0xA555E, 0xA55B2};
constexpr std::uintptr_t kGameEntityDamageRva = 0x29E30;
constexpr std::uintptr_t kMeleeEnemyDamageCallsite = 0x603D5;
constexpr std::uintptr_t kMeleeHitBodyRva = 0x5F000;
constexpr std::array<std::uintptr_t, 2> kMeleeHitBodyCallsites{
    0x60747, 0x608CF};
constexpr std::uintptr_t kPhysicsBodyUserDataOffset = 0x414;
constexpr std::uintptr_t kGameEntityTypeOffset = 0xC0;
constexpr std::int32_t kGameEntityEnemyType = 7;
constexpr std::int32_t kPushActionState = 1;
constexpr std::int32_t kMoveActionState = 2;
constexpr std::int32_t kWalkMoveState = 0;
constexpr std::int32_t kJumpMoveState = 3;
constexpr std::int32_t kCrouchMoveState = 4;
std::uint8_t* g_image = nullptr;
bool g_inventory_right_guard_patched = false;
hooks::IatHook g_update_hook;
std::array<hooks::Rel32CallHook, std::size(kQueries)> g_query_hooks;
hooks::Rel32CallHook g_inventory_double_query_hook;
std::array<hooks::Rel32CallHook, 2> g_move_hooks;
std::array<hooks::Rel32CallHook, kPlayerDamageCallsites.size()> g_damage_hooks;
struct PointerEntry { std::uintptr_t site, target, cursor; };
constexpr PointerEntry kPointers[]{
    {0x4477,0x797B0,0xA0},
    // The exact-build death screen and adjacent full-screen overlay both use
    // cGameMenu::AddMousePos (0x8AC0). Their cursor lives at +0x38/+0x3C.
    {0x4965,0x8AC0,0x38},
    {0x4AFA,0x8AC0,0x38},
    {0x4C70,0x945F0,0x84},
    {0x4FF2,0x6C7C0,0xAC}};
std::array<hooks::Rel32CallHook, std::size(kPointers)> g_pointer_hooks;
hooks::Rel32CallHook g_melee_enemy_damage_hook;
std::array<hooks::Rel32CallHook, kMeleeHitBodyCallsites.size()>
    g_melee_hit_body_hooks;
std::atomic<bool> g_ui{true};
std::atomic<bool> g_installed{false};
std::atomic<unsigned> g_active_callbacks{0};
struct CallbackScope {
    CallbackScope() { g_active_callbacks.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackScope() { g_active_callbacks.fetch_sub(1, std::memory_order_acq_rel); }
};
std::atomic<void*> g_player{nullptr};
SRWLOCK g_session_lock = SRWLOCK_INIT;
runtime::OpenVrSession* g_session = nullptr;
runtime::VrControllerFrame g_frame;
std::array<std::array<float, 5>, 2> g_hand_curls{};
std::array<std::array<std::uint32_t, 2>, runtime::kVrHapticEventCount>
    g_haptic_last_submission{};
std::array<std::array<bool, 2>, runtime::kVrHapticEventCount>
    g_haptic_has_submitted{};
NativeHapticDiagnostics g_haptic_diagnostics{};
runtime::VrInputState g_disconnect_release;
std::atomic<std::uint64_t> g_release_pending{0};
std::uint64_t g_release_generation = 0;
std::atomic<std::uint64_t> g_session_generation{0};
runtime::VrSnapTurn g_turn;
SRWLOCK g_crouch_lock = SRWLOCK_INIT;
runtime::VrPhysicalCrouchPolicy g_crouch_policy;
BlackPlagueNativeCrouchStatus g_crouch_status;
SRWLOCK g_crouch_edge_lock = SRWLOCK_INIT;
BlackPlagueCrouchEdgeToken g_native_crouch_press_token;
bool g_vr_crouch_owned = false;
void* g_vr_crouch_owner_player = nullptr;
std::atomic<std::uint64_t> g_player_generation{0};
std::uint64_t g_native_crouch_entries = 0;
std::uint64_t g_native_crouch_exits = 0;
std::uint64_t g_native_crouch_stand_retries = 0;
std::uint64_t g_native_crouch_mismatch_frames = 0;
thread_local runtime::VrNativeIntents* g_intents = nullptr;
thread_local void* g_input_player = nullptr;
thread_local void* g_input_handler = nullptr;
thread_local bool g_direct_locomotion = false;
thread_local bool g_direct_native_axis_observed = false;
thread_local bool g_direct_forward_allowed = false;
thread_local bool g_direct_sideways_allowed = false;
thread_local runtime::VrAnalogState g_direct_move{};
thread_local runtime::VrMatrix44 g_direct_head_world_pose{};
thread_local float g_direct_move_scale = 1.0F;
thread_local bool g_direct_sprinting = false;
thread_local std::uint64_t g_direct_player_generation = 0;
thread_local bool g_pointer_valid = false;
thread_local std::array<float, 2> g_pointer_uv{};
thread_local std::uint64_t g_mouse_override_until = 0;
thread_local bool g_openvr_crouch_press_this_update = false;
thread_local bool g_vr_crouch_query_owner = false;
thread_local std::uint64_t g_crouch_query_session_generation = 0;
thread_local std::uint64_t g_crouch_query_player_generation = 0;

bool ReadBytes(const void* source, void* dest, std::size_t size) noexcept {
    if (!source) return false;
    __try { std::memcpy(dest, source, size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool PatchInventoryRightClickGuard(
    std::uint8_t* image,
    bool enable,
    std::string& error) noexcept {
    error.clear();
    if (image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }
    auto* const address = image + kInventoryRightGuardRva;
    const auto& expected = enable
        ? kInventoryRightGuardStock : kInventoryRightGuardEnabled;
    const auto& replacement = enable
        ? kInventoryRightGuardEnabled : kInventoryRightGuardStock;
    std::array<std::uint8_t, 2> actual{};
    if (!ReadBytes(address, actual.data(), actual.size()) || actual != expected) {
        error = enable
            ? "Native inventory RightClick guard does not match the exact build"
            : "Native inventory RightClick guard was modified before restoration";
        return false;
    }
    DWORD old_protect = 0;
    if (!VirtualProtect(address, replacement.size(), PAGE_EXECUTE_READWRITE,
            &old_protect)) {
        error = "Could not make the native inventory RightClick guard writable";
        return false;
    }
    std::memcpy(address, replacement.data(), replacement.size());
    const bool flushed = FlushInstructionCache(
        GetCurrentProcess(), address, replacement.size()) != FALSE;
    DWORD ignored = 0;
    const bool restored = VirtualProtect(
        address, replacement.size(), old_protect, &ignored) != FALSE;
    if (!flushed || !restored) {
        error = "Could not finalize the native inventory RightClick guard patch";
        return false;
    }
    return true;
}
template<class T> T Read(const void* object, std::uintptr_t offset) noexcept {
    T result{};
    if (object) static_cast<void>(ReadBytes(static_cast<const std::uint8_t*>(object) + offset, &result, sizeof(result)));
    return result;
}

struct NativeLightState {
    bool known = false;
    bool glow = false;
    bool flashlight = false;
};

[[nodiscard]] NativeLightState ReadNativeLightState(void* player) noexcept {
    NativeLightState state;
    if (player == nullptr) return state;
    auto* const glow = Read<void*>(player, 0x29C);
    auto* const flashlight = Read<void*>(player, 0x290);
    if (glow == nullptr || flashlight == nullptr) return state;
    if (!ReadBytes(glow, &state.glow, sizeof(state.glow)) ||
        !ReadBytes(static_cast<const std::uint8_t*>(flashlight) + 4,
                   &state.flashlight, sizeof(state.flashlight))) {
        return {};
    }
    state.known = true;
    return state;
}

[[nodiscard]] bool NativeLightStateChanged(
    const NativeLightState& before,
    const NativeLightState& after) noexcept {
    return before.known && after.known &&
           (before.glow != after.glow ||
            before.flashlight != after.flashlight);
}

[[nodiscard]] runtime::VrHand BlackPlagueOffHand(
    runtime::VrHand interact_source) noexcept {
    return interact_source == runtime::VrHand::left
        ? runtime::VrHand::right : runtime::VrHand::left;
}

[[nodiscard]] runtime::VrHand BlackPlagueDominantHand() noexcept {
    return g_dominant_handedness.load(std::memory_order_acquire) ==
            runtime::VrHandedness::left
        ? runtime::VrHand::left : runtime::VrHand::right;
}

[[nodiscard]] bool BlackPlagueMeleeBodyCanImpact(void* body) noexcept {
    if (body == nullptr) return false;
    auto* const entity = Read<void*>(body, kPhysicsBodyUserDataOffset);
    if (entity == nullptr) return true;
    std::int32_t entity_type = 0;
    return ReadBytes(static_cast<const std::uint8_t*>(entity) +
                         kGameEntityTypeOffset,
                     &entity_type, sizeof(entity_type)) &&
           entity_type != kGameEntityEnemyType;
}

[[nodiscard]] float BlackPlagueDamageHapticStrength(
    void* player, float damage) noexcept {
    if (player == nullptr || !std::isfinite(damage) || damage <= 0.0F)
        return 0.0F;
    auto* const init = Read<void*>(player, 0x70);
    if (init == nullptr) return 0.0F;
    const auto difficulty = Read<std::int32_t>(init, 0x80);
    if (difficulty == 0) damage *= 0.5F;
    else if (difficulty == 2) damage *= 2.0F;
    return 0.35F + std::min(damage, 50.0F) / 50.0F * 0.65F;
}

[[nodiscard]] bool ConstrainedDirectLocomotion(void* player) noexcept {
    if (player == nullptr) return false;
    const auto action_state =
        Read<std::int32_t>(player, kPlayerActionStateIndexOffset);
    return action_state == kPushActionState || action_state == kMoveActionState;
}

[[nodiscard]] bool ReadNativeCrouchShape(
    void* player, bool& crouched) noexcept {
    crouched = false;
    auto* const body = Read<void*>(player, kPlayerCharacterBodyOffset);
    const float height = Read<float>(body, kCharacterSizeYOffset);
    if (!std::isfinite(height)) return false;
    if (std::abs(height - 0.95F) <= 0.05F) {
        crouched = true;
        return true;
    }
    return std::abs(height - 1.65F) <= 0.05F;
}

void ServiceNativeVrCrouch(void* player, bool desired,
    BlackPlagueNativeCrouchStatus& status) noexcept {
    const std::uint64_t player_generation =
        g_player_generation.load(std::memory_order_acquire);
    const std::uint64_t session_generation =
        g_session_generation.load(std::memory_order_acquire);
    if (g_vr_crouch_owned && g_vr_crouch_owner_player != player) {
        // Never carry a VR-owned stance across a cPlayer replacement. The old
        // object may already be dead; the new generation must acquire its own
        // stance through current policy on the game thread.
        g_vr_crouch_owned = false;
        g_vr_crouch_owner_player = nullptr;
    }
    bool crouched = false;
    bool known = ReadNativeCrouchShape(player, crouched);
    const bool move_state_known = player != nullptr;
    std::int32_t move_state = move_state_known
        ? Read<std::int32_t>(player, kPlayerMoveStateIndexOffset) : -1;

    if (desired) {
        if (move_state == kCrouchMoveState) {
            g_vr_crouch_owned = true;
            g_vr_crouch_owner_player = player;
        } else if (move_state_known && move_state != kJumpMoveState &&
                   player != nullptr && g_image != nullptr) {
            // Rework owns one persistent desired crouch state and applies the
            // actual move state directly. Black Plague exposes the equivalent
            // cPlayer::ChangeMoveState boundary at 0x9C750; its native crouch
            // handlers prove state 4=crouch and state 0=walk. Do not route the
            // persistent policy back through configurable press/release toggle
            // callbacks, which can undo the state on the next native edge.
            reinterpret_cast<ChangeMoveState>(g_image + kChangeMoveStateRva)(
                player, kCrouchMoveState, false);
            move_state = Read<std::int32_t>(player, kPlayerMoveStateIndexOffset);
            if (move_state == kCrouchMoveState) {
                g_vr_crouch_owned = true;
                g_vr_crouch_owner_player = player;
                ++g_native_crouch_entries;
            }
            known = ReadNativeCrouchShape(player, crouched);
        }
    } else if (g_vr_crouch_owned) {
        if (move_state == kCrouchMoveState && player != nullptr && g_image != nullptr) {
            reinterpret_cast<ChangeMoveState>(g_image + kChangeMoveStateRva)(
                player, kWalkMoveState, false);
            move_state = Read<std::int32_t>(player, kPlayerMoveStateIndexOffset);
            known = ReadNativeCrouchShape(player, crouched);
            if (move_state != kCrouchMoveState) {
                g_vr_crouch_owned = false;
                g_vr_crouch_owner_player = nullptr;
                ++g_native_crouch_exits;
            } else {
                ++g_native_crouch_stand_retries;
            }
        } else if (move_state_known && move_state != kCrouchMoveState) {
            g_vr_crouch_owned = false;
            g_vr_crouch_owner_player = nullptr;
            ++g_native_crouch_exits;
        }
    }

    if (move_state_known && desired != (move_state == kCrouchMoveState))
        ++g_native_crouch_mismatch_frames;
    status.native_shape_known = known;
    status.native_crouched = known && crouched;
    status.native_move_state_known = move_state_known;
    status.native_move_state = move_state;
    status.vr_stance_owned = g_vr_crouch_owned;
    status.stand_blocked = !desired && g_vr_crouch_owned &&
        move_state == kCrouchMoveState;
    status.session_generation = session_generation;
    status.player_generation = player_generation;
    status.owner_player = reinterpret_cast<std::uintptr_t>(
        g_vr_crouch_owner_player);
    status.native_crouch_entries = g_native_crouch_entries;
    status.native_crouch_exits = g_native_crouch_exits;
    status.stand_retries = g_native_crouch_stand_retries;
    status.desired_shape_mismatch_frames = g_native_crouch_mismatch_frames;
}

[[nodiscard]] void* CurrentMoveState(
    void* player, std::uintptr_t table_offset,
    std::uintptr_t index_offset) noexcept {
    auto* const table = Read<void*>(player, table_offset);
    const auto index = Read<std::int32_t>(player, index_offset);
    if (table == nullptr || index < 0) return nullptr;
    return Read<void*>(table,
        static_cast<std::uintptr_t>(index) * sizeof(void*));
}

[[nodiscard]] bool CallMoveStateGate(
    void* state, std::uintptr_t slot,
    float amount, float delta_seconds) noexcept {
    if (state == nullptr) return false;
    auto* const vtable = Read<void*>(state, 0);
    auto* const target = Read<void*>(vtable, slot);
    if (target == nullptr) return false;
    __try {
        return reinterpret_cast<MoveStateGate>(target)(
            state, amount, delta_seconds);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Exact FD316F... MoveForward/MoveSideways prefix. Both native methods call
// these two state gates, then require +0x268 > 0 or +0x26C != 0, before their
// final amount != 0 branch reaches iCharacterBody::Move and writes +0x264.
// Direct VR locomotion cannot use +0x264 as a permission oracle after removing
// its analog amount from the native axis, because amount == 0 returns before
// that write. Re-evaluate only the proven pre-Move predicate at the original
// axis callback and leave the body write to the single 0xD7281 owner.
[[nodiscard]] bool NativeMoveAxisAllowsDirectLocomotion(
    void* player, float amount, float delta_seconds,
    bool sideways) noexcept {
    if (player == nullptr || !std::isfinite(amount) ||
        !std::isfinite(delta_seconds) ||
        std::abs(amount) <= 0.00001F || delta_seconds <= 0.0F) {
        return false;
    }

    // MoveForward/MoveSideways load the current interaction state as
    //   vector = player+0x2C4, index = player+0x2BC
    // before dispatching OnMoveForward/OnMoveSideways.
    auto* const primary = CurrentMoveState(player, 0x2C4, 0x2BC);
    if (!CallMoveStateGate(primary, sideways ? 0x50 : 0x4C,
                           amount, delta_seconds)) {
        return false;
    }
    // They then load the current locomotion state as
    //   vector = player+0x2D8, index = player+0x2D0.
    auto* const secondary = CurrentMoveState(player, 0x2D8, 0x2D0);
    if (!CallMoveStateGate(secondary, sideways ? 0x10 : 0x0C,
                           amount, delta_seconds)) {
        return false;
    }
    return Read<std::int32_t>(player, 0x268) > 0 ||
           Read<std::uint8_t>(player, 0x26C) != 0;
}

void MarkDirectLocomotionAccepted(void* player) noexcept {
    if (player == nullptr) return;
    __try {
        *(static_cast<std::uint8_t*>(player) + 0x264) = 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}
[[nodiscard]] std::uintptr_t DecodeCallTarget(
    const std::array<std::uint8_t, 5>& bytes, std::uintptr_t call) noexcept {
    if (bytes[0] != 0xE8) return 0;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
    return call + bytes.size() + displacement;
}
bool UiContext(void* handler) {
    if (Read<int>(handler, 0x3C) != 1) return true;
    const auto* init = Read<void*>(handler, 0x2C);
    const auto* player = Read<void*>(handler, 0x38);
    if (!init || !player || !Read<bool>(player, 0x1DC)) return true;
    if (Read<bool>(Read<void*>(player, 0x28C), 0)) return true;
    // These are the same overlay checks/ordering as the mapped native handler.
    return Read<int>(Read<void*>(init, 0x1AC), 0x30) != 0 ||
        Read<bool>(Read<void*>(init, 0x180), 0x2D) ||
        Read<bool>(Read<void*>(init, 0x17C), 0x31) ||
        Read<bool>(Read<void*>(init, 0x178), 0x44) ||
        Read<bool>(Read<void*>(init, 0x164), 0x5C);
}

bool InventoryContextActive(void* handler) noexcept {
    if (!handler) return false;
    auto* const init = Read<void*>(handler, 0x2C);
    auto* const inventory = init ? Read<void*>(init, 0x164) : nullptr;
    auto* const context = inventory ? Read<void*>(inventory, 0x28) : nullptr;
    // Exact-build cInventory::OnMouseDown/OnDoubleClick both read the context
    // object's active byte at +0x30 through the inventory pointer at +0x28.
    return context && Read<bool>(context, 0x30);
}

std::uint64_t ObserveNativePlayerForUpdate(void* current_player) noexcept {
    void* const previous_published_player = g_player.exchange(
        current_player, std::memory_order_acq_rel);
    if (previous_published_player != current_player) {
        g_player_generation.fetch_add(1, std::memory_order_acq_rel);
        AcquireSRWLockExclusive(&g_crouch_lock);
        g_crouch_policy.Reset();
        g_crouch_status = {};
        g_vr_crouch_owned = false;
        g_vr_crouch_owner_player = nullptr;
        ReleaseSRWLockExclusive(&g_crouch_lock);
        AcquireSRWLockExclusive(&g_crouch_edge_lock);
        g_native_crouch_press_token = {};
        ReleaseSRWLockExclusive(&g_crouch_edge_lock);
        InvalidateBlackPlagueDirectLocomotionIntent();
    }
    return g_player_generation.load(std::memory_order_acquire);
}

bool HandleMappedQuery(const Entry& entry, void* input, LegacyString name) {
    // Always execute native query first, including its string destructor and
    // native edge bookkeeping, even when VR has a matching pressed button.
    const bool native = reinterpret_cast<Query>(g_image + Target(entry.query))(
        input, name);
    if (entry.action == A::crouch && !g_vr_crouch_query_owner) {
        // Outside an active/focused VR gameplay ownership window, preserve
        // Black Plague's native crouch query result exactly.
        return native;
    }
    const bool duplicate_crouch_press = native &&
        entry.action == A::crouch && entry.query == Q::pressed &&
        g_openvr_crouch_press_this_update;
    const auto native_crouch_token =
        BlackPlagueNativeCrouchEdgeToken(
            native && entry.action == A::crouch && entry.query == Q::pressed,
            g_vr_crouch_query_owner,
            duplicate_crouch_press,
            g_crouch_query_session_generation,
            g_crouch_query_player_generation);
    if (native_crouch_token.session_generation != 0) {
        // Rework folds the legacy crouch trigger into the same toggle latch
        // as the VR action. The exact query has consumed its own edge and
        // string lifetime; publish it to shared policy for the following
        // game tick instead of letting a second native toggle own posture.
        AcquireSRWLockExclusive(&g_crouch_edge_lock);
        g_native_crouch_press_token = native_crouch_token;
        ReleaseSRWLockExclusive(&g_crouch_edge_lock);
    }
    if (entry.action == A::crouch) {
        // The three exact Black Plague query sites can dispatch both native
        // StartCrouch and StopCrouch according to its legacy hold/toggle
        // option. Shared policy now owns one persistent desired stance, so
        // those action branches must not race ServiceNativeVrCrouch. Native
        // query bookkeeping still ran above, and its press is applied by
        // shared policy on the next game tick.
        return false;
    }
    bool vr = g_intents && g_intents->Query(entry.action, entry.query);
    if (g_intents && entry.site == kInventoryLeftPressSite &&
        entry.query == Q::pressed && InventoryContextActive(g_input_handler)) {
        // Rework activates the highlighted inventory-context row with the same
        // uiSelect edge that otherwise performs a default action. Consume that
        // edge here only while the native context is actually open.
        vr = g_intents->Query(A::select, Q::pressed) || vr;
    }
    if (vr && !native && entry.action == A::light && g_input_player) {
        auto* glow = Read<void*>(g_input_player,0x29C);
        auto* flashlight = Read<void*>(g_input_player,0x290);
        if (!glow || !flashlight) return native;
        const auto plan = runtime::PlanQuickLight(
            Read<bool>(glow,0),Read<bool>(flashlight,4));
        if (plan.toggle_glow)
            reinterpret_cast<void(__thiscall*)(void*)>(g_image+0x9BD80)(
                g_input_player);
        // The native handler executes StartFlashLightButton on true.
        return plan.toggle_flashlight;
    }
    if (vr && entry.action == A::interact && entry.query == Q::pressed)
        RefreshVrSelectionBeforeInteract(g_input_player);
    return native || vr;
}

bool HandleInventoryDoubleQuery(void* input, LegacyString name,
    float window_seconds) {
    // Keep legacy string destruction and native double-click timing owned by
    // the exact game function, then add Rework's explicit uiSelect default
    // action only while no context row is open.
    const bool native = reinterpret_cast<DoubleQuery>(
        g_image + kInventoryDoubleQueryRva)(input, name, window_seconds);
    const bool vr = g_intents && !InventoryContextActive(g_input_handler) &&
        g_intents->Query(A::select, Q::pressed);
    return native || vr;
}

bool __fastcall HookedInventoryDoubleQuery(void* input, void*, LegacyString name,
    float window_seconds) {
    CallbackScope scope;
    return HandleInventoryDoubleQuery(input, name, window_seconds);
}
bool __fastcall HookedQuery(void* input, void*, LegacyString name) {
    CallbackScope scope;
    const auto return_rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) -
                            reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& entry : kQueries) {
        if (entry.site + 5 != return_rva) continue;
        return HandleMappedQuery(entry, input, name);
    }
    return reinterpret_cast<Query>(g_image + Target(Q::pressed))(input, name);
}
void __fastcall HookedForward(void* player, void*, float amount, float dt) {
    CallbackScope scope;
    if (g_direct_locomotion && std::abs(amount) > 0.00001F)
        g_direct_native_axis_observed = true;
    if (g_direct_locomotion && !g_direct_native_axis_observed &&
        std::abs(g_direct_move.y) > 0.00001F) {
        g_direct_forward_allowed = NativeMoveAxisAllowsDirectLocomotion(
            player, g_direct_move.y * g_direct_move_scale, dt, false);
        return;
    }
    const float intent = g_direct_locomotion
        ? amount : (g_intents ? g_intents->Move(amount, false) : amount);
    if (!PublishBlackPlagueForwardIntent(player, intent, dt)) {
        reinterpret_cast<Move>(g_image + 0x9CBC0)(player, intent, dt);
    }
}
void __fastcall HookedSideways(void* player, void*, float amount, float dt) {
    CallbackScope scope;
    if (g_direct_locomotion && std::abs(amount) > 0.00001F)
        g_direct_native_axis_observed = true;
    if (g_direct_locomotion && !g_direct_native_axis_observed &&
        std::abs(g_direct_move.x) > 0.00001F) {
        g_direct_sideways_allowed = NativeMoveAxisAllowsDirectLocomotion(
            player, g_direct_move.x * g_direct_move_scale, dt, true);
    } else {
        const float intent = g_direct_locomotion
            ? amount : (g_intents ? g_intents->Move(amount, true) : amount);
        if (!PublishBlackPlagueSidewaysIntent(player, intent, dt)) {
            reinterpret_cast<Move>(g_image + 0x9CC60)(player, intent, dt);
        }
    }

    // Both native axis callbacks have now run. Native keyboard/controller axes
    // retain priority. Otherwise publish only the logical VR components whose
    // exact-build state predicate accepted them; the body tick owns metres/time.
    if (g_direct_locomotion && !g_direct_native_axis_observed) {
        auto permitted = g_direct_move;
        if (std::abs(permitted.y) > 0.00001F && !g_direct_forward_allowed)
            permitted.y = 0.0F;
        if (std::abs(permitted.x) > 0.00001F && !g_direct_sideways_allowed)
            permitted.x = 0.0F;
        BlackPlagueDirectLocomotionIntent intent;
        intent.move = {permitted.x, permitted.y};
        intent.head_world_pose = g_direct_head_world_pose.values;
        intent.move_scale = g_direct_move_scale;
        intent.constrained = ConstrainedDirectLocomotion(player);
        intent.sprinting = g_direct_sprinting;
        intent.player_generation = g_direct_player_generation;
        if (std::hypot(permitted.x, permitted.y) > 0.0F &&
            PublishBlackPlagueDirectLocomotionIntent(player, intent)) {
            MarkDirectLocomotionAccepted(player);
        } else {
            InvalidateBlackPlagueDirectLocomotionIntent();
        }
    } else if (g_direct_locomotion) {
        InvalidateBlackPlagueDirectLocomotionIntent();
    }
}
void __fastcall HookedPointer(void* menu, void*, const std::array<float, 2>* physical_delta) {
    CallbackScope scope;
    using Pointer = void(__thiscall*)(void*, const std::array<float, 2>*);
    const auto return_rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& entry : kPointers) {
        if (entry.site + 5 != return_rva) continue;
        std::array<float, 2> delta{};
        if (!ReadBytes(physical_delta, delta.data(), sizeof(delta))) return;
        const auto now = GetTickCount64();
        if (std::abs(delta[0]) > 0.01F || std::abs(delta[1]) > 0.01F) g_mouse_override_until = now + 1500;
        if (g_intents && g_pointer_valid && now >= g_mouse_override_until) {
            const float x = Read<float>(menu, entry.cursor), y = Read<float>(menu, entry.cursor + 4);
            std::array<float, 2> smoothed{};
            if (runtime::SmoothMenuPointerUv(
                    {x / 800.0F, y / 600.0F}, g_pointer_uv, 0.40F, smoothed)) {
                delta = {smoothed[0] * 800.0F - x, smoothed[1] * 600.0F - y};
            }
        }
        reinterpret_cast<Pointer>(g_image + entry.target)(menu, &delta);
        return;
    }
}

void __fastcall HookedPlayerDamage(
    void* player, void*, float damage, std::int32_t damage_type) {
    CallbackScope scope;
    using PlayerDamage = void(__thiscall*)(void*, float, std::int32_t);
    const float health_before = Read<float>(player, 0x310);
    const float strength = BlackPlagueDamageHapticStrength(player, damage);
    reinterpret_cast<PlayerDamage>(g_image + kPlayerDamageRva)(
        player, damage, damage_type);
    const float health_after = Read<float>(player, 0x310);
    // Rework emits Damage only after positive damage has survived the native
    // gameplay guards and difficulty scaling. Preserve that success boundary:
    // the exact BP method remains authoritative, and feedback is emitted only
    // when its own health field actually decreased. Strength uses BP's exact
    // 0.5x/1x/2x difficulty transform before the shared Rework profile.
    if (strength > 0.0F && std::isfinite(health_before) &&
        std::isfinite(health_after) && health_after < health_before) {
        NativeControllerHaptic(
            runtime::VrHand::left, runtime::VrHapticEvent::damage, strength);
        NativeControllerHaptic(
            runtime::VrHand::right, runtime::VrHapticEvent::damage, strength);
    }
}

void __fastcall HookedMeleeEnemyDamage(
    void* entity, void*, float damage, std::int32_t attack_strength) {
    CallbackScope scope;
    using GameEntityDamage = void(__thiscall*)(void*, float, std::int32_t);
    reinterpret_cast<GameEntityDamage>(g_image + kGameEntityDamageRva)(
        entity, damage, attack_strength);
    // Rework raises MeleeImpact only after the enemy collision path has
    // reached Damage. This callsite is the equivalent post-contact boundary
    // in the supported Black Plague image; keep native damage authoritative.
    NativeControllerHaptic(
        BlackPlagueDominantHand(), runtime::VrHapticEvent::melee_impact);
}

void __fastcall HookedMeleeHitBody(void* melee, void*, void* body) {
    CallbackScope scope;
    using MeleeHitBody = void(__thiscall*)(void*, void*);
    const bool can_impact = BlackPlagueMeleeBodyCanImpact(body);
    reinterpret_cast<MeleeHitBody>(g_image + kMeleeHitBodyRva)(melee, body);
    // Both owned callsites enter this resolver only after Black Plague has
    // accepted the physical contact. The native resolver explicitly ignores
    // enemy-backed bodies (type 7), whose separate Damage path above owns the
    // impact, so mirror that exclusion to avoid a false or duplicate pulse.
    if (can_impact) {
        NativeControllerHaptic(
            BlackPlagueDominantHand(), runtime::VrHapticEvent::melee_impact);
    }
}

void __fastcall HookedUpdate(void* handler, void*, float dt) {
    CallbackScope scope;
    const bool ui = UiContext(handler);
    const auto context = ui ? runtime::VrInputContext::ui : runtime::VrInputContext::gameplay;
    g_ui.store(ui, std::memory_order_release);
    void* const current_player = Read<void*>(handler, 0x38);
    const std::uint64_t player_generation =
        ObserveNativePlayerForUpdate(current_player);
    runtime::VrControllerFrame frame;
    runtime::VrAnalogState raw_move;
    float move_scale = 1.0F;
    runtime::VrCrouchMode crouch_mode = runtime::VrCrouchMode::hybrid;
    float physical_crouch_depth =
        runtime::vr_setting_limits::kPhysicalCrouchDepth.default_value;
    std::uint64_t consumed_release = 0;
    bool session_connected = false;
    AcquireSRWLockExclusive(&g_session_lock);
    if (g_release_pending.load(std::memory_order_acquire)) {
        consumed_release = g_release_pending.load(std::memory_order_acquire);
        frame.input.state = g_disconnect_release;
        g_disconnect_release = {};
        g_frame = {};
        g_hand_curls = {};
    } else if (g_session) {
        session_connected = true;
        std::string error;
        g_session->SetControllerMoveDeadZone(g_settings.move_dead_zone);
        const auto handedness = g_settings.handedness == runtime::VrHandedness::left ?
            runtime::VrHand::left : runtime::VrHand::right;
        static_cast<void>(g_session->ReadControllerInput(context, handedness,
                                                        GetTickCount64(), frame, error));
        for (std::size_t hand_index = 0; hand_index < frame.hands.size(); ++hand_index) {
            auto& hand = frame.hands[hand_index];
            if (!hand.skeleton_valid) {
                g_hand_curls[hand_index] = {};
                continue;
            }
            runtime::VrHandCurlInput curl_input;
            curl_input.valid = true;
            curl_input.skeletal = true;
            curl_input.finger_curl = hand.finger_curl;
            // Black Plague's provisional hand already demonstrated richer,
            // hardware-driven five-finger response than Rework. Preserve the
            // controller skeletal channels directly here; do not collapse them
            // into Rework's trigger/grip-derived fallback semantics.
            const auto target = runtime::BuildVrHandCurlTargets(curl_input);
            runtime::SmoothVrHandCurls(g_hand_curls[hand_index], target, dt);
            hand.finger_curl = g_hand_curls[hand_index];
        }
        g_frame = frame;
    }
    raw_move = frame.input.state.move;
    move_scale = g_settings.move_speed;
    crouch_mode = g_settings.crouch_mode;
    physical_crouch_depth = g_settings.physical_crouch_depth;
    if (frame.input.state.move.active) {
        frame.input.state.move.x *= g_settings.move_speed;
        frame.input.state.move.y *= g_settings.move_speed;
        g_frame.input.state.move = frame.input.state.move;
    }
    const float turn = g_turn.Update(
        frame.input.state.turn, !ui && frame.focused,
        g_settings.turn_mode, dt, g_settings.snap_turn_angle, g_settings.smooth_turn_speed,
        g_settings.turn_dead_zone);
    const auto timing=g_update_timing.Update(dt,GetTickCount64(),!ui && frame.focused);
    if (timing.ready) g_timing_sample=timing;
    ReleaseSRWLockExclusive(&g_session_lock);
    runtime::VrPhysicalCrouchStatus crouch_policy_status;
    if (consumed_release == 0) {
        const bool openvr_crouch_press = frame.input.state.crouch.just_pressed;
        float head_height = 0.0F;
        const bool gameplay_active = !ui && frame.focused;
        const bool tracking_valid = gameplay_active &&
            TrackedHeadTrackingHeight(head_height);
        BlackPlagueCrouchEdgeToken native_crouch_token;
        AcquireSRWLockExclusive(&g_crouch_edge_lock);
        native_crouch_token = g_native_crouch_press_token;
        g_native_crouch_press_token = {};
        ReleaseSRWLockExclusive(&g_crouch_edge_lock);
        if (BlackPlagueConsumeCrouchEdge(
                native_crouch_token,
                g_session_generation.load(std::memory_order_acquire),
                player_generation)) {
            frame.input.state.crouch.active = true;
            frame.input.state.crouch.just_pressed = true;
        }
        AcquireSRWLockExclusive(&g_crouch_lock);
        const bool stand_blocked = g_crouch_status.stand_blocked &&
            g_crouch_status.player_generation == player_generation;
        frame.input.state.crouch = g_crouch_policy.Update(
            frame.input.state.crouch,
            crouch_mode,
            physical_crouch_depth,
            gameplay_active,
            tracking_valid,
            head_height,
            stand_blocked);
        crouch_policy_status = g_crouch_policy.status();
        ReleaseSRWLockExclusive(&g_crouch_lock);
        g_openvr_crouch_press_this_update = openvr_crouch_press;
    } else {
        AcquireSRWLockExclusive(&g_crouch_lock);
        g_crouch_policy.Reset();
        crouch_policy_status = g_crouch_policy.status();
        ReleaseSRWLockExclusive(&g_crouch_lock);
        g_openvr_crouch_press_this_update = false;
    }
    const bool desired_vr_crouch = crouch_policy_status.desired_crouch;
    const auto pointer = runtime::SelectUiPointerPose(frame, frame.interact_source);
    g_pointer_valid = ui && frame.focused &&
        pointer.valid && TrackedMenuPointer(pointer.pose, g_pointer_uv);
    if (g_pointer_valid && frame.input.state.ui_select.just_pressed) {
        NativeControllerHaptic(
            pointer.hand, runtime::VrHapticEvent::ui_select);
    }
    if (frame.input.state.recenter.just_pressed) RequestTrackedRecenter();
    if (ui && !g_pointer_valid) {
        frame.input.state.ui_select.pressed = false;
        frame.input.state.ui_select.just_pressed = false;
        frame.input.state.ui_drag.pressed = false;
        frame.input.state.ui_drag.just_pressed = false;
    }
    runtime::VrNativeIntents intents;
    float movement_yaw = 0;
    auto* previous = g_intents;
    auto* previous_player = g_input_player;
    g_input_player = current_player;
    BlackPlagueNativeCrouchStatus crouch_status;
    crouch_status.policy = crouch_policy_status;
    ServiceNativeVrCrouch(g_input_player, desired_vr_crouch, crouch_status);
    AcquireSRWLockExclusive(&g_crouch_lock);
    g_crouch_status = crouch_status;
    ReleaseSRWLockExclusive(&g_crouch_lock);
    // Shared policy owns VR/legacy crouch state. Do not feed its held/released
    // representation back through Black Plague's configurable toggle handler;
    // the direct ChangeMoveState boundary above applies the desired move state.
    frame.input.state.crouch = {};
    g_direct_locomotion = false;
    g_direct_native_axis_observed = false;
    g_direct_forward_allowed = false;
    g_direct_sideways_allowed = false;
    g_direct_move = {};
    g_direct_head_world_pose = {};
    g_direct_move_scale = 1.0F;
    g_direct_sprinting = false;
    g_direct_player_generation = 0;
    runtime::VrMatrix44 head_world_pose;
    if (!ui && frame.focused && raw_move.active &&
        BlackPlagueDirectLocomotionAvailable(g_input_player) &&
        TrackedHeadWorldPose(head_world_pose)) {
        g_direct_move = raw_move;
        g_direct_head_world_pose = head_world_pose;
        g_direct_move_scale = move_scale;
        g_direct_sprinting = frame.input.state.sprint.pressed;
        g_direct_player_generation = player_generation;
        g_direct_locomotion = std::hypot(raw_move.x, raw_move.y) > 0.0F;
    }
    if (g_direct_locomotion) {
        // Buttons and sprint still enter their native owners; only the VR
        // analog move is replaced by the collision-aware metric request.
        frame.input.state.move = {};
    } else if (!ui && !TrackedMovementYaw(movement_yaw)) {
        frame.input.state.move = {};
    }
    if (!g_direct_locomotion) {
        InvalidateBlackPlagueDirectLocomotionIntent();
    }
    intents.Begin(frame.input.state, context, movement_yaw);
    g_intents = &intents;
    g_vr_crouch_query_owner = BlackPlagueVrOwnsCrouchQuery({
        session_connected, ui, frame.focused,
        consumed_release != 0, current_player != nullptr});
    g_crouch_query_session_generation =
        g_session_generation.load(std::memory_order_acquire);
    g_crouch_query_player_generation = player_generation;
    // Release spatial ownership even when a UI context filters gameplay edges.
    ServiceSpatialInteraction(g_input_player, ui);
    if (turn != 0) AddTrackedWorldYaw(-turn);
    // Rework emits LightToggle only after StartFlashLightButton /
    // StartGlowStickButton actually changed the native light state, and routes
    // it to the off hand. Black Plague already owns the complete native button
    // update here, including the VR quick-light query adaptation above, so a
    // pre/post state observation gives the same success boundary without
    // stacking a second hook over either game method.
    const auto light_before = session_connected && !ui && frame.focused
        ? ReadNativeLightState(g_input_player) : NativeLightState{};
    auto* const previous_handler = g_input_handler;
    g_input_handler = handler;
    reinterpret_cast<Update>(g_image + 0x3BF0)(handler, dt);
    g_input_handler = previous_handler;
    const auto light_after = light_before.known
        ? ReadNativeLightState(g_input_player) : NativeLightState{};
    if (NativeLightStateChanged(light_before, light_after)) {
        NativeControllerHaptic(
            BlackPlagueOffHand(frame.interact_source),
            runtime::VrHapticEvent::light_toggle);
    }
    ServiceSpatialInteraction(g_input_player, UiContext(handler));
    ServiceBlackPlagueVrFootstep(g_input_player);
    g_intents = previous;
    g_input_player = previous_player;
    g_direct_locomotion = false;
    g_direct_native_axis_observed = false;
    g_direct_forward_allowed = false;
    g_direct_sideways_allowed = false;
    g_direct_move = {};
    g_direct_head_world_pose = {};
    g_direct_move_scale = 1.0F;
    g_direct_sprinting = false;
    g_direct_player_generation = 0;
    g_openvr_crouch_press_this_update = false;
    g_vr_crouch_query_owner = false;
    g_crouch_query_session_generation = 0;
    g_crouch_query_player_generation = 0;
    if (consumed_release != 0) static_cast<void>(g_release_pending.compare_exchange_strong(
        consumed_release, 0, std::memory_order_acq_rel));
    // A button can open an inventory/menu during this update. Publish the new
    // context before rendering so its desktop UI is visible in the headset.
    g_ui.store(UiContext(handler), std::memory_order_release);
}
}

runtime::VrUpdateTimingSample ConsumeNativeUpdateTiming() noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    const auto result=g_timing_sample; g_timing_sample={};
    ReleaseSRWLockExclusive(&g_session_lock);
    return result;
}
NativeMovementBoundaryStatus ReadNativeMovementBoundaryStatus() noexcept {
    NativeMovementBoundaryStatus result;
    constexpr std::array<std::uintptr_t, 2> sites{0x51CD, 0x5227};
    constexpr std::array<std::uintptr_t, 2> targets{0x9CBC0, 0x9CC60};
    const std::array<void*, 2> replacements{
        reinterpret_cast<void*>(&HookedForward),
        reinterpret_cast<void*>(&HookedSideways)};
    const bool installed = g_installed.load(std::memory_order_acquire);
    auto* const image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    result.initialized = installed;
    for (std::size_t index = 0; index < sites.size(); ++index) {
        auto& callsite = result.callsites[index];
        callsite.rva = sites[index];
        callsite.expected = CallBytes(sites[index], targets[index]);
        callsite.expected_target = targets[index];
        callsite.owner_installed = installed;
        if (image != nullptr) {
            static_cast<void>(ReadBytes(image + sites[index],
                callsite.live.data(), callsite.live.size()));
        }
        callsite.live_target = DecodeCallTarget(callsite.live, sites[index]);
        void* live_target = nullptr;
        if (callsite.live[0] == 0xE8 && image != nullptr) {
            std::int32_t displacement = 0;
            std::memcpy(&displacement, callsite.live.data() + 1,
                sizeof(displacement));
            live_target = image + sites[index] + callsite.live.size() +
                displacement;
        }
        callsite.owner_matches_live = callsite.owner_installed &&
            live_target == replacements[index];
        result.initialized = result.initialized && callsite.owner_matches_live;
    }
    return result;
}
void ConfigureNativeInputBridge(runtime::VrSettings settings) noexcept {
    runtime::NormalizeVrSettings(settings);
    AcquireSRWLockExclusive(&g_session_lock);
    g_settings = settings;
    ReleaseSRWLockExclusive(&g_session_lock);
    g_dominant_handedness.store(settings.handedness, std::memory_order_release);
}
bool InstallNativeInputBridge(std::string& error) noexcept {
    error.clear();
    const auto all_installed = [](const auto& hooks) noexcept {
        return std::all_of(hooks.begin(), hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); });
    };
    const auto any_installed = [](const auto& hooks) noexcept {
        return std::any_of(hooks.begin(), hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); });
    };
    const bool complete = g_inventory_right_guard_patched &&
        g_update_hook.installed() &&
        all_installed(g_query_hooks) && all_installed(g_move_hooks) &&
        all_installed(g_pointer_hooks) && all_installed(g_damage_hooks) &&
        g_inventory_double_query_hook.installed() &&
        g_melee_enemy_damage_hook.installed() &&
        all_installed(g_melee_hit_body_hooks);
    const bool any = g_inventory_right_guard_patched ||
        g_update_hook.installed() ||
        any_installed(g_query_hooks) || any_installed(g_move_hooks) ||
        any_installed(g_pointer_hooks) || any_installed(g_damage_hooks) ||
        g_inventory_double_query_hook.installed() ||
        g_melee_enemy_damage_hook.installed() ||
        any_installed(g_melee_hit_body_hooks);
    if (g_installed.load(std::memory_order_acquire)) {
        if (complete) return true;
        error = "Native input bridge installation state is incomplete";
        return false;
    }
    if (any) {
        error = "Native input bridge is only partially installed";
        return false;
    }
    auto* const image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }
    if (g_image == nullptr) {
        // Once callbacks have been published, keep the process-resident image
        // base immutable so a callback from a just-removed installation cannot
        // race a subsequent installation rewriting the same global pointer.
        g_image = image;
    } else if (g_image != image) {
        error = "The Black Plague image base changed after native input publication";
        return false;
    }
    void* update = nullptr;
    if (!ReadBytes(g_image + 0x272A84, &update, sizeof(update)) || update != g_image + 0x3BF0) {
        error = "ButtonHandler Update vtable does not match the exact build"; return false;
    }
    for (const auto& entry : kQueries) {
        const auto expected = CallBytes(entry.site, Target(entry.query));
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + entry.site, actual.data(), actual.size()) || actual != expected) {
            error = "Native input query mismatch at RVA " + std::to_string(entry.site); return false;
        }
    }
    {
        const auto expected = CallBytes(
            kInventoryDoubleQuerySite, kInventoryDoubleQueryRva);
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + kInventoryDoubleQuerySite,
                actual.data(), actual.size()) || actual != expected) {
            error = "Native inventory double-trigger query mismatch";
            return false;
        }
    }
    for (const auto& entry : kPointers) {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + entry.site, actual.data(), actual.size()) || actual != CallBytes(entry.site, entry.target)) {
            error = "Native menu pointer call mismatch"; return false;
        }
    }
    for (const auto site : kPlayerDamageCallsites) {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + site, actual.data(), actual.size()) ||
            actual != CallBytes(site, kPlayerDamageRva)) {
            error = "Native player Damage call mismatch";
            return false;
        }
    }
    {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + kMeleeEnemyDamageCallsite,
                actual.data(), actual.size()) ||
            actual != CallBytes(kMeleeEnemyDamageCallsite,
                                kGameEntityDamageRva)) {
            error = "Native melee enemy Damage call mismatch";
            return false;
        }
    }
    for (const auto site : kMeleeHitBodyCallsites) {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + site, actual.data(), actual.size()) ||
            actual != CallBytes(site, kMeleeHitBodyRva)) {
            error = "Native melee body-impact call mismatch";
            return false;
        }
    }
    constexpr std::array<std::uintptr_t, 2> sites{0x51CD, 0x5227}, targets{0x9CBC0, 0x9CC60};
    for (std::size_t i = 0; i < sites.size(); ++i) {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + sites[i], actual.data(), actual.size()) || actual != CallBytes(sites[i], targets[i])) {
            error = "Native movement call mismatch"; return false;
        }
    }
    // Validate the remaining exact native input entries before publishing hooks.
    for (const auto& light : {std::array<std::uintptr_t,2>{0x513D,0x9BCD0},{0x5165,0x9BD80}}) {
        std::array<std::uint8_t,5> actual{};
        if (!ReadBytes(g_image+light[0],actual.data(),actual.size()) || actual!=CallBytes(light[0],light[1])) {
            error="Native light button call mismatch"; return false;
        }
    }
    constexpr std::array<std::uint8_t, 13> change_move_state{
        0x56, 0x8B, 0xF1, 0x8B, 0x96, 0xD0, 0x02, 0x00, 0x00,
        0x8B, 0x4C, 0x24, 0x08};
    std::array<std::uint8_t, change_move_state.size()> actual_change_move_state{};
    if (!ReadBytes(g_image + kChangeMoveStateRva,
            actual_change_move_state.data(), actual_change_move_state.size()) ||
        actual_change_move_state != change_move_state) {
        error = "Native ChangeMoveState entry mismatch";
        return false;
    }
    constexpr std::array<std::uint8_t, 8> crouch_state_proof{
        0x83, 0xFA, 0x04, 0x75, 0x08, 0x6A, 0x00, 0xE8};
    std::array<std::uint8_t, crouch_state_proof.size()> actual_crouch_state_proof{};
    if (!ReadBytes(g_image + 0xAEEE4, actual_crouch_state_proof.data(),
            actual_crouch_state_proof.size()) ||
        actual_crouch_state_proof != crouch_state_proof) {
        error = "Native crouch move-state mapping mismatch";
        return false;
    }
    if (!PatchInventoryRightClickGuard(g_image, true, error)) {
        return false;
    }
    g_inventory_right_guard_patched = true;
    bool ok = true;
    for (std::size_t i = 0; ok && i < std::size(kQueries); ++i) {
        const auto& entry = kQueries[i];
        ok = hooks::InstallRel32CallHook(g_image + entry.site, CallBytes(entry.site, Target(entry.query)),
                                        reinterpret_cast<void*>(&HookedQuery), g_query_hooks[i], error);
    }
    if (ok) {
        ok = hooks::InstallRel32CallHook(
            g_image + kInventoryDoubleQuerySite,
            CallBytes(kInventoryDoubleQuerySite, kInventoryDoubleQueryRva),
            reinterpret_cast<void*>(&HookedInventoryDoubleQuery),
            g_inventory_double_query_hook, error);
    }
    const std::array<void*, 2> replacements{reinterpret_cast<void*>(&HookedForward), reinterpret_cast<void*>(&HookedSideways)};
    for (std::size_t i = 0; ok && i < sites.size(); ++i)
        ok = hooks::InstallRel32CallHook(g_image + sites[i], CallBytes(sites[i], targets[i]), replacements[i], g_move_hooks[i], error);
    for (std::size_t i = 0; ok && i < std::size(kPointers); ++i) {
        const auto& entry = kPointers[i];
        ok = hooks::InstallRel32CallHook(g_image + entry.site, CallBytes(entry.site, entry.target),
            reinterpret_cast<void*>(&HookedPointer), g_pointer_hooks[i], error);
    }
    for (std::size_t i = 0; ok && i < kPlayerDamageCallsites.size(); ++i) {
        const auto site = kPlayerDamageCallsites[i];
        ok = hooks::InstallRel32CallHook(
            g_image + site, CallBytes(site, kPlayerDamageRva),
            reinterpret_cast<void*>(&HookedPlayerDamage), g_damage_hooks[i], error);
    }
    if (ok) {
        ok = hooks::InstallRel32CallHook(
            g_image + kMeleeEnemyDamageCallsite,
            CallBytes(kMeleeEnemyDamageCallsite, kGameEntityDamageRva),
            reinterpret_cast<void*>(&HookedMeleeEnemyDamage),
            g_melee_enemy_damage_hook, error);
    }
    for (std::size_t i = 0; ok && i < kMeleeHitBodyCallsites.size(); ++i) {
        const auto site = kMeleeHitBodyCallsites[i];
        ok = hooks::InstallRel32CallHook(
            g_image + site, CallBytes(site, kMeleeHitBodyRva),
            reinterpret_cast<void*>(&HookedMeleeHitBody),
            g_melee_hit_body_hooks[i], error);
    }
    if (ok) ok = hooks::InstallPointerHook(reinterpret_cast<void**>(g_image + 0x272A84),
            g_image + 0x3BF0, reinterpret_cast<void*>(&HookedUpdate), g_update_hook, error);
    if (!ok) {
        std::string rollback;
        if (!RemoveNativeInputBridge(rollback)) error += "; rollback failed: " + rollback;
    } else {
        AcquireSRWLockExclusive(&g_crouch_lock);
        g_crouch_policy.Reset();
        g_crouch_status = {};
        g_vr_crouch_owned = false;
        g_native_crouch_entries = 0;
        g_native_crouch_exits = 0;
        g_native_crouch_stand_retries = 0;
        g_native_crouch_mismatch_frames = 0;
        ReleaseSRWLockExclusive(&g_crouch_lock);
        AcquireSRWLockExclusive(&g_crouch_edge_lock);
        g_native_crouch_press_token = {};
        ReleaseSRWLockExclusive(&g_crouch_edge_lock);
    }
    g_installed.store(ok, std::memory_order_release);
    return ok;
}
bool RemoveNativeInputBridge(std::string& error) noexcept {
    error.clear();
    g_installed.store(false, std::memory_order_release);
    g_player.store(nullptr, std::memory_order_release);
    ConnectNativeInput(nullptr);
    // Let a running game consume the pending release in its own update thread.
    // Never invoke player/physics methods from this remote control thread.
    const auto deadline = GetTickCount64() + 1000;
    while (g_release_pending.load(std::memory_order_acquire) && GetTickCount64() < deadline) Sleep(1);
    if (g_release_pending.load(std::memory_order_acquire)) {
        error = "Waiting for the native update thread to release VR input before detaching";
        return false;
    }
    const auto append_error = [&](const std::string& next) {
        if (next.empty()) return;
        if (!error.empty()) error += "; ";
        error += next;
    };
    bool ok = true;
    std::string next;
    if (!hooks::RemoveIatHook(g_update_hook, next)) {
        ok = false;
        append_error(next);
    }
    for (auto& hook : g_query_hooks) {
        next.clear();
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; append_error(next); }
    }
    next.clear();
    if (!hooks::RemoveRel32CallHook(g_inventory_double_query_hook, next)) {
        ok = false;
        append_error(next);
    }
    for (auto& hook : g_move_hooks) {
        next.clear();
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; append_error(next); }
    }
    for (auto& hook : g_pointer_hooks) {
        next.clear();
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; append_error(next); }
    }
    for (auto& hook : g_damage_hooks) {
        next.clear();
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; append_error(next); }
    }
    next.clear();
    if (!hooks::RemoveRel32CallHook(g_melee_enemy_damage_hook, next)) {
        ok = false;
        append_error(next);
    }
    for (auto& hook : g_melee_hit_body_hooks) {
        next.clear();
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; append_error(next); }
    }
    if (g_inventory_right_guard_patched) {
        next.clear();
        if (!PatchInventoryRightClickGuard(g_image, false, next)) {
            ok = false;
            append_error(next);
        } else {
            g_inventory_right_guard_patched = false;
        }
    }
    const bool any_hook = g_update_hook.installed() ||
        std::any_of(g_query_hooks.begin(), g_query_hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); }) ||
        g_inventory_double_query_hook.installed() ||
        std::any_of(g_move_hooks.begin(), g_move_hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); }) ||
        std::any_of(g_pointer_hooks.begin(), g_pointer_hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); }) ||
        std::any_of(g_damage_hooks.begin(), g_damage_hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); }) ||
        g_melee_enemy_damage_hook.installed() ||
        std::any_of(g_melee_hit_body_hooks.begin(), g_melee_hit_body_hooks.end(),
            [](const auto& hook) noexcept { return hook.installed(); }) ||
        g_inventory_right_guard_patched;
    if (any_hook) {
        append_error("Native input bridge remains partially installed");
        return false;
    }
    const auto callback_deadline = GetTickCount64() + 1000;
    while (g_active_callbacks.load(std::memory_order_acquire) &&
           GetTickCount64() < callback_deadline) {
        Sleep(1);
    }
    if (g_active_callbacks.load(std::memory_order_acquire)) {
        ok = false;
        append_error("Native input callbacks are still active");
    }
    // DLL and original pointers remain resident for callbacks already in flight.
    return ok;
}
void ConnectNativeInput(runtime::OpenVrSession* session) noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    if (g_session != session) {
        g_session_generation.fetch_add(1, std::memory_order_acq_rel);
        g_haptic_last_submission = {};
        g_haptic_has_submitted = {};
    }
    if (!session && g_session) {
        g_disconnect_release = runtime::MakeReleasedVrInputState(g_frame.input.state);
        AcquireSRWLockShared(&g_crouch_lock);
        const bool effective_crouch = g_crouch_status.policy.effective_crouch;
        ReleaseSRWLockShared(&g_crouch_lock);
        if (effective_crouch) {
            g_disconnect_release.crouch.active = true;
            g_disconnect_release.crouch.just_released = true;
        }
        g_release_pending.store(++g_release_generation, std::memory_order_release);
    }
    g_session = session; g_frame = {}; g_turn = {};
    ReleaseSRWLockExclusive(&g_session_lock);
}
bool NativeInputUiActive() noexcept {
    return g_installed.load(std::memory_order_acquire) && g_ui.load(std::memory_order_acquire);
}
runtime::VrControllerFrame ReadNativeControllerFrame() noexcept {
    AcquireSRWLockShared(&g_session_lock);
    const auto frame = g_frame;
    ReleaseSRWLockShared(&g_session_lock);
    return frame;
}
BlackPlagueNativeCrouchStatus ReadNativePhysicalCrouchStatus() noexcept {
    AcquireSRWLockShared(&g_crouch_lock);
    const auto status = g_crouch_status;
    ReleaseSRWLockShared(&g_crouch_lock);
    return status;
}
void* NativePlayerPointer() noexcept {
    return g_player.load(std::memory_order_acquire);
}
std::uint64_t NativePlayerGeneration() noexcept {
    return g_player_generation.load(std::memory_order_acquire);
}
void NativeControllerHaptic(runtime::VrHand hand, runtime::VrHapticEvent event,
    float strength) noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    if (runtime::IsKnownHapticEvent(event)) {
        const std::size_t hand_index = hand == runtime::VrHand::left ? 0U : 1U;
        const auto event_index = runtime::HapticEventIndex(event);
        ++g_haptic_diagnostics.attempts[event_index];
        const auto now = static_cast<std::uint32_t>(GetTickCount64());
        std::string error;
        const auto profile = runtime::HapticProfile(event);
        const float amplitude = runtime::ScaleHapticAmplitude(event, strength);
        const bool ready = g_session && BlackPlagueHapticReady(
                g_frame, hand, event,
                g_haptic_has_submitted[event_index][hand_index],
                g_haptic_last_submission[event_index][hand_index], now,
                strength);
        if (!ready) {
            ++g_haptic_diagnostics.policy_rejections[event_index];
        } else if (g_session->TriggerHaptic(
                hand,
                profile.duration_seconds,
                profile.frequency_hz,
                amplitude,
                error)) {
            g_haptic_has_submitted[event_index][hand_index] = true;
            g_haptic_last_submission[event_index][hand_index] = now;
            ++g_haptic_diagnostics.submissions[event_index];
            if (hand == runtime::VrHand::left) ++g_haptic_diagnostics.left_submissions;
            else ++g_haptic_diagnostics.right_submissions;
        } else {
            ++g_haptic_diagnostics.submit_failures[event_index];
        }
    }
    ReleaseSRWLockExclusive(&g_session_lock);
}

NativeHapticDiagnostics ConsumeNativeHapticDiagnostics() noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    const auto diagnostics = g_haptic_diagnostics;
    g_haptic_diagnostics = {};
    ReleaseSRWLockExclusive(&g_session_lock);
    return diagnostics;
}

#if defined(PVR_NATIVE_INPUT_BRIDGE_TEST_ACCESS)
namespace {
bool g_contract_native_query_result = false;
bool g_contract_block_stand = false;

template<class T>
void ContractWrite(void* object, std::uintptr_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset,
        &value, sizeof(value));
}

bool __fastcall ContractNativeQuery(void*, void*, LegacyString) noexcept {
    return g_contract_native_query_result;
}

bool __fastcall ContractNativeDoubleQuery(
    void*, void*, LegacyString, float) noexcept {
    return g_contract_native_query_result;
}

void __fastcall ContractChangeMoveState(
    void* player, void*, std::int32_t state, bool) noexcept {
    auto* const body = Read<void*>(player, kPlayerCharacterBodyOffset);
    if (state == kWalkMoveState && g_contract_block_stand) return;
    ContractWrite(player, kPlayerMoveStateIndexOffset, state);
    if (body != nullptr) {
        const float height = state == kCrouchMoveState ? 0.95F : 1.65F;
        ContractWrite(body, kCharacterSizeYOffset, height);
    }
}

bool ContractJump(void* source, void* target) noexcept {
    if (source == nullptr || target == nullptr) return false;
    auto* const entry = static_cast<std::uint8_t*>(source);
    entry[0] = 0xE9;
    const auto displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(target) -
        reinterpret_cast<std::uintptr_t>(entry + 5));
    std::memcpy(entry + 1, &displacement, sizeof(displacement));
    return FlushInstructionCache(GetCurrentProcess(), entry, 5) != FALSE;
}
} // namespace

bool RunNativeInputBridgeContractHarness(std::string& error) noexcept {
    error.clear();
    if (g_image != nullptr || g_installed.load(std::memory_order_acquire)) {
        error = "native input bridge contract harness requires an uninstalled bridge";
        return false;
    }

    constexpr std::size_t kImageSize = 0x100000;
    auto* const image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, kImageSize, MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    if (image == nullptr) {
        error = "could not allocate the synthetic Black Plague image";
        return false;
    }
    g_image = image;
    const auto cleanup = [&]() noexcept {
        g_session = nullptr;
        g_frame = {};
        g_disconnect_release = {};
        g_release_pending.store(0, std::memory_order_release);
        g_release_generation = 0;
        g_session_generation.store(0, std::memory_order_release);
        g_player.store(nullptr, std::memory_order_release);
        g_player_generation.store(0, std::memory_order_release);
        g_vr_crouch_query_owner = false;
        g_crouch_query_session_generation = 0;
        g_crouch_query_player_generation = 0;
        g_openvr_crouch_press_this_update = false;
        AcquireSRWLockExclusive(&g_crouch_edge_lock);
        g_native_crouch_press_token = {};
        ReleaseSRWLockExclusive(&g_crouch_edge_lock);
        AcquireSRWLockExclusive(&g_crouch_lock);
        g_crouch_policy.Reset();
        g_crouch_status = {};
        g_vr_crouch_owned = false;
        g_vr_crouch_owner_player = nullptr;
        ReleaseSRWLockExclusive(&g_crouch_lock);
        g_contract_native_query_result = false;
        g_contract_block_stand = false;
        g_intents = nullptr;
        g_input_player = nullptr;
        g_input_handler = nullptr;
        g_ui.store(true, std::memory_order_release);
        AcquireSRWLockExclusive(&g_session_lock);
        g_haptic_diagnostics = {};
        ReleaseSRWLockExclusive(&g_session_lock);
        g_image = nullptr;
        VirtualFree(image, 0, MEM_RELEASE);
    };
    const auto fail = [&](const char* detail) noexcept {
        error = detail;
        cleanup();
        return false;
    };

    const auto has_pointer_entry = [](std::uintptr_t site,
                                      std::uintptr_t target,
                                      std::uintptr_t cursor) noexcept {
        return std::any_of(std::begin(kPointers), std::end(kPointers),
            [=](const PointerEntry& entry) noexcept {
                return entry.site == site && entry.target == target &&
                    entry.cursor == cursor;
            });
    };
    const auto has_query_entry = [](std::uintptr_t site, A action,
                                    Q query) noexcept {
        return std::any_of(std::begin(kQueries), std::end(kQueries),
            [=](const Entry& entry) noexcept {
                return entry.site == site && entry.action == action &&
                    entry.query == query;
            });
    };
    if (!has_query_entry(0x4CF2, A::drag, Q::pressed) ||
        !has_query_entry(0x4D73, A::drag, Q::released)) {
        return fail("inventory left-button callsites do not preserve VR drag semantics");
    }
    if (!has_query_entry(0x4DA3, A::back, Q::pressed) ||
        !has_query_entry(0x4DEF, A::back, Q::released)) {
        return fail("inventory right-button callsites do not preserve VR context semantics");
    }
    if (!has_pointer_entry(0x4965, 0x8AC0, 0x38)) {
        return fail("death-screen pointer call is not hooked");
    }
    if (!has_pointer_entry(0x4AFA, 0x8AC0, 0x38)) {
        return fail("secondary overlay pointer call is not hooked");
    }

    // Model only the exact fields read by UiContext. This proves that the
    // native inventory/notebook active bytes drive the existing tracked-menu
    // context without attaching to the game or adding another UI owner.
    std::array<std::uint8_t, 0x80> handler{};
    std::array<std::uint8_t, 0x220> init{};
    std::array<std::uint8_t, 0x300> ui_player{};
    std::array<std::uint8_t, 0x80> player_overlay{};
    std::array<std::uint8_t, 0x80> notebook{};
    std::array<std::uint8_t, 0x120> inventory{};
    std::array<std::uint8_t, 0x80> inventory_context{};
    ContractWrite(handler.data(), 0x3C, std::int32_t{1});
    void* const init_pointer = init.data();
    void* const ui_player_pointer = ui_player.data();
    void* const overlay_pointer = player_overlay.data();
    void* const notebook_pointer = notebook.data();
    void* const inventory_pointer = inventory.data();
    void* const inventory_context_pointer = inventory_context.data();
    ContractWrite(handler.data(), 0x2C, init_pointer);
    ContractWrite(handler.data(), 0x38, ui_player_pointer);
    ContractWrite(ui_player.data(), 0x1DC, true);
    ContractWrite(ui_player.data(), 0x28C, overlay_pointer);
    ContractWrite(init.data(), 0x178, notebook_pointer);
    ContractWrite(init.data(), 0x164, inventory_pointer);
    ContractWrite(inventory.data(), 0x28, inventory_context_pointer);
    if (UiContext(handler.data())) {
        return fail("normal Black Plague gameplay was treated as a UI context");
    }
    ContractWrite(notebook.data(), 0x44, true);
    if (!UiContext(handler.data())) {
        return fail("notebook active byte did not enter the tracked UI context");
    }
    ContractWrite(notebook.data(), 0x44, false);
    ContractWrite(inventory.data(), 0x5C, true);
    if (!UiContext(handler.data())) {
        return fail("inventory active byte did not enter the tracked UI context");
    }
    g_ui.store(UiContext(handler.data()), std::memory_order_release);
    if (!g_ui.load(std::memory_order_acquire)) {
        return fail("published Black Plague UI context lost active inventory state");
    }

    runtime::VrControllerFrame haptic_frame;
    haptic_frame.focused = true;
    haptic_frame.hands[0].grip.device_connected = true;
    haptic_frame.hands[0].grip.pose_valid = true;
    if (!BlackPlagueHapticReady(
            haptic_frame, runtime::VrHand::left,
            runtime::VrHapticEvent::ui_select,
            false, 0, 100, 1.0F) ||
        BlackPlagueHapticReady(
            haptic_frame, runtime::VrHand::left,
            runtime::VrHapticEvent::ui_select,
            true, 100, 159, 1.0F) ||
        !BlackPlagueHapticReady(
            haptic_frame, runtime::VrHand::left,
            runtime::VrHapticEvent::ui_select,
            true, 100, 160, 1.0F)) {
        return fail("Black Plague haptic cooldown policy drifted");
    }
    haptic_frame.hands[0].grip.pose_valid = false;
    if (BlackPlagueHapticReady(
            haptic_frame, runtime::VrHand::left,
            runtime::VrHapticEvent::interaction,
            false, 0, 100, 1.0F)) {
        return fail("Black Plague haptic policy accepted an invalid pose");
    }
    haptic_frame.hands[0].grip.pose_valid = true;
    haptic_frame.focused = false;
    if (BlackPlagueHapticReady(
            haptic_frame, runtime::VrHand::left,
            runtime::VrHapticEvent::interaction,
            false, 0, 100, 1.0F)) {
        return fail("Black Plague haptic policy ignored input focus");
    }
    g_frame = haptic_frame;
    NativeControllerHaptic(
        runtime::VrHand::left, runtime::VrHapticEvent::ui_select);
    const auto haptic_diagnostics = ConsumeNativeHapticDiagnostics();
    const auto ui_haptic_index =
        runtime::HapticEventIndex(runtime::VrHapticEvent::ui_select);
    if (haptic_diagnostics.attempts[ui_haptic_index] != 1 ||
        haptic_diagnostics.policy_rejections[ui_haptic_index] != 1 ||
        haptic_diagnostics.submissions[ui_haptic_index] != 0 ||
        haptic_diagnostics.submit_failures[ui_haptic_index] != 0 ||
        haptic_diagnostics.left_submissions != 0 ||
        haptic_diagnostics.right_submissions != 0) {
        return fail("Black Plague haptic diagnostics did not record a rejected request");
    }
    const auto cleared_haptic_diagnostics = ConsumeNativeHapticDiagnostics();
    if (cleared_haptic_diagnostics.attempts[ui_haptic_index] != 0 ||
        cleared_haptic_diagnostics.policy_rejections[ui_haptic_index] != 0) {
        return fail("Black Plague haptic diagnostics did not reset after consumption");
    }

    std::array<std::uint8_t, 0x400> light_player{};
    std::array<std::uint8_t, 0x20> glow{};
    std::array<std::uint8_t, 0x20> flashlight{};
    void* const glow_pointer = glow.data();
    void* const flashlight_pointer = flashlight.data();
    ContractWrite(light_player.data(), 0x29C, glow_pointer);
    ContractWrite(light_player.data(), 0x290, flashlight_pointer);
    ContractWrite(glow.data(), 0, false);
    ContractWrite(flashlight.data(), 4, false);
    const auto light_off = ReadNativeLightState(light_player.data());
    if (!light_off.known || light_off.glow || light_off.flashlight) {
        return fail("Black Plague light-state snapshot lost the native off state");
    }
    ContractWrite(glow.data(), 0, true);
    const auto light_glow = ReadNativeLightState(light_player.data());
    if (!NativeLightStateChanged(light_off, light_glow) ||
        BlackPlagueOffHand(runtime::VrHand::right) != runtime::VrHand::left ||
        BlackPlagueOffHand(runtime::VrHand::left) != runtime::VrHand::right) {
        return fail("Black Plague light-toggle haptic success/off-hand policy drifted");
    }
    if (NativeLightStateChanged(light_glow, light_glow)) {
        return fail("Black Plague light-toggle haptic accepted an unchanged state");
    }
    ContractWrite(glow.data(), 0, false);
    ContractWrite(flashlight.data(), 4, true);
    const auto light_flashlight = ReadNativeLightState(light_player.data());
    if (!NativeLightStateChanged(light_glow, light_flashlight)) {
        return fail("Black Plague light-state snapshot lost the glow-to-flashlight transition");
    }

    g_dominant_handedness.store(
        runtime::VrHandedness::right, std::memory_order_release);
    if (BlackPlagueDominantHand() != runtime::VrHand::right) {
        return fail("Black Plague melee haptic lost right-hand dominance");
    }
    g_dominant_handedness.store(
        runtime::VrHandedness::left, std::memory_order_release);
    if (BlackPlagueDominantHand() != runtime::VrHand::left) {
        return fail("Black Plague melee haptic lost left-hand dominance");
    }
    std::array<std::uint8_t, 0x420> melee_body{};
    std::array<std::uint8_t, 0x100> melee_entity{};
    if (!BlackPlagueMeleeBodyCanImpact(melee_body.data())) {
        return fail("Black Plague melee haptic rejected an ordinary physics body");
    }
    void* const melee_entity_pointer = melee_entity.data();
    ContractWrite(melee_body.data(), kPhysicsBodyUserDataOffset,
        melee_entity_pointer);
    ContractWrite(melee_entity.data(), kGameEntityTypeOffset,
        std::int32_t{0});
    if (!BlackPlagueMeleeBodyCanImpact(melee_body.data())) {
        return fail("Black Plague melee haptic rejected a non-enemy game entity");
    }
    ContractWrite(melee_entity.data(), kGameEntityTypeOffset,
        kGameEntityEnemyType);
    if (BlackPlagueMeleeBodyCanImpact(melee_body.data())) {
        return fail("Black Plague melee body path duplicated the enemy impact owner");
    }

    std::array<std::uint8_t, 0x400> damage_player{};
    std::array<std::uint8_t, 0x100> damage_init{};
    void* const damage_init_pointer = damage_init.data();
    ContractWrite(damage_player.data(), 0x70, damage_init_pointer);
    ContractWrite(damage_init.data(), 0x80, std::int32_t{1});
    const float normal_damage_strength =
        BlackPlagueDamageHapticStrength(damage_player.data(), 10.0F);
    ContractWrite(damage_init.data(), 0x80, std::int32_t{0});
    const float easy_damage_strength =
        BlackPlagueDamageHapticStrength(damage_player.data(), 10.0F);
    ContractWrite(damage_init.data(), 0x80, std::int32_t{2});
    const float hard_damage_strength =
        BlackPlagueDamageHapticStrength(damage_player.data(), 10.0F);
    if (std::abs(normal_damage_strength - 0.48F) > 0.0001F ||
        std::abs(easy_damage_strength - 0.415F) > 0.0001F ||
        std::abs(hard_damage_strength - 0.61F) > 0.0001F ||
        BlackPlagueDamageHapticStrength(damage_player.data(), 0.0F) != 0.0F) {
        return fail("Black Plague damage haptic difficulty/strength mapping drifted");
    }

    std::array<std::uint8_t, 0x400> locomotion_player{};
    ContractWrite(locomotion_player.data(), kPlayerActionStateIndexOffset,
        std::int32_t{0});
    if (ConstrainedDirectLocomotion(locomotion_player.data())) {
        return fail("normal Black Plague state was treated as constrained locomotion");
    }
    ContractWrite(locomotion_player.data(), kPlayerActionStateIndexOffset,
        kPushActionState);
    if (!ConstrainedDirectLocomotion(locomotion_player.data())) {
        return fail("Black Plague Push state lost constrained locomotion");
    }
    ContractWrite(locomotion_player.data(), kPlayerActionStateIndexOffset,
        kMoveActionState);
    if (!ConstrainedDirectLocomotion(locomotion_player.data())) {
        return fail("Black Plague Move state lost constrained locomotion");
    }
    ContractWrite(locomotion_player.data(), kPlayerActionStateIndexOffset,
        std::int32_t{6});
    if (ConstrainedDirectLocomotion(locomotion_player.data())) {
        return fail("Black Plague Grab state was treated as constrained locomotion");
    }

    for (const auto query : {Q::pressed, Q::released, Q::held}) {
        if (!ContractJump(image + Target(query),
                reinterpret_cast<void*>(&ContractNativeQuery))) {
            return fail("could not create a synthetic native query entry");
        }
    }
    if (!ContractJump(image + kInventoryDoubleQueryRva,
            reinterpret_cast<void*>(&ContractNativeDoubleQuery))) {
        return fail("could not create a synthetic inventory double query entry");
    }
    if (!ContractJump(image + kChangeMoveStateRva,
            reinterpret_cast<void*>(&ContractChangeMoveState))) {
        return fail("could not create synthetic ChangeMoveState");
    }

    LegacyString legacy_name{};
    runtime::VrNativeIntents inventory_intents;
    runtime::VrInputState inventory_input{};
    g_contract_native_query_result = false;
    g_intents = &inventory_intents;
    g_input_handler = handler.data();

    // The exact Black Plague cInventory::OnMouseDown entry has a single
    // `cmp button, 2 / je return` guard at 0x46EEC5/0x46EEC8. Rework routes
    // uiBack through this inventory-level method, so the framework only removes
    // that guard and leaves the native context/widget dispatch untouched.
    std::memcpy(image + kInventoryRightGuardRva,
        kInventoryRightGuardStock.data(), kInventoryRightGuardStock.size());
    std::string guard_error;
    if (!PatchInventoryRightClickGuard(image, true, guard_error) ||
        std::memcmp(image + kInventoryRightGuardRva,
            kInventoryRightGuardEnabled.data(),
            kInventoryRightGuardEnabled.size()) != 0) {
        return fail("inventory RightClick guard was not enabled exactly");
    }
    if (!PatchInventoryRightClickGuard(image, false, guard_error) ||
        std::memcmp(image + kInventoryRightGuardRva,
            kInventoryRightGuardStock.data(),
            kInventoryRightGuardStock.size()) != 0) {
        return fail("inventory RightClick guard was not restored exactly");
    }

    inventory_input.ui_back = {true, true, true, false};
    ContractWrite(inventory_context.data(), 0x30, false);
    inventory_intents.Begin(inventory_input, runtime::VrInputContext::ui);
    if (!HandleMappedQuery(
            {kInventoryRightPressSite, A::back, Q::pressed},
            nullptr, legacy_name)) {
        return fail("inventory VR RightClick did not reach the native query branch");
    }

    inventory_input.ui_drag = {true, true, true, false};
    inventory_intents.Begin(inventory_input, runtime::VrInputContext::ui);
    if (!HandleMappedQuery(
            {kInventoryLeftPressSite, A::drag, Q::pressed},
            nullptr, legacy_name) ||
        HandleInventoryDoubleQuery(nullptr, legacy_name, 0.2F)) {
        return fail("inventory drag edge leaked into the default-action path");
    }

    inventory_input = {};
    inventory_input.ui_select = {true, true, true, false};
    ContractWrite(inventory_context.data(), 0x30, false);
    inventory_intents.Begin(inventory_input, runtime::VrInputContext::ui);
    if (HandleMappedQuery(
            {kInventoryLeftPressSite, A::drag, Q::pressed},
            nullptr, legacy_name) ||
        !HandleInventoryDoubleQuery(nullptr, legacy_name, 0.2F) ||
        HandleInventoryDoubleQuery(nullptr, legacy_name, 0.2F)) {
        return fail("inventory select did not produce one native default action");
    }

    ContractWrite(inventory_context.data(), 0x30, true);
    inventory_intents.Begin(inventory_input, runtime::VrInputContext::ui);
    if (!HandleMappedQuery(
            {kInventoryLeftPressSite, A::drag, Q::pressed},
            nullptr, legacy_name) ||
        HandleInventoryDoubleQuery(nullptr, legacy_name, 0.2F)) {
        return fail("inventory context select did not route through native left mouse-down");
    }
    ContractWrite(inventory_context.data(), 0x30, false);
    g_intents = nullptr;
    g_input_handler = nullptr;

    const Entry crouch_press{0, A::crouch, Q::pressed};
    g_contract_native_query_result = true;

    g_vr_crouch_query_owner = false;
    if (!HandleMappedQuery(crouch_press, nullptr, legacy_name)) {
        return fail("native crouch was suppressed without VR ownership");
    }

    constexpr std::uint64_t session_generation = 11;
    constexpr std::uint64_t player_generation = 17;
    g_session_generation.store(session_generation, std::memory_order_release);
    g_player_generation.store(player_generation, std::memory_order_release);
    g_crouch_query_session_generation = session_generation;
    g_crouch_query_player_generation = player_generation;
    g_vr_crouch_query_owner = BlackPlagueVrOwnsCrouchQuery(
        {true, false, true, false, true});
    if (HandleMappedQuery(crouch_press, nullptr, legacy_name)) {
        return fail("VR-owned native crouch reached the legacy toggle path");
    }
    AcquireSRWLockShared(&g_crouch_edge_lock);
    const auto focused_edge = g_native_crouch_press_token;
    ReleaseSRWLockShared(&g_crouch_edge_lock);
    if (!BlackPlagueConsumeCrouchEdge(
            focused_edge, session_generation, player_generation)) {
        return fail("focused native crouch edge lost its generation");
    }

    AcquireSRWLockExclusive(&g_crouch_edge_lock);
    g_native_crouch_press_token = {};
    ReleaseSRWLockExclusive(&g_crouch_edge_lock);
    g_openvr_crouch_press_this_update = true;
    if (HandleMappedQuery(crouch_press, nullptr, legacy_name)) {
        return fail("duplicate crouch press reached the legacy toggle path");
    }
    AcquireSRWLockShared(&g_crouch_edge_lock);
    const auto duplicate_edge = g_native_crouch_press_token;
    ReleaseSRWLockShared(&g_crouch_edge_lock);
    g_openvr_crouch_press_this_update = false;
    if (duplicate_edge.session_generation != 0) {
        return fail("OpenVR/native double edge published two crouch latches");
    }

    g_vr_crouch_query_owner = BlackPlagueVrOwnsCrouchQuery(
        {true, false, false, false, true});
    if (!HandleMappedQuery(crouch_press, nullptr, legacy_name)) {
        return fail("native crouch was suppressed after focus loss");
    }

    ConnectNativeInput(reinterpret_cast<runtime::OpenVrSession*>(1));
    g_frame.input.state.crouch.active = true;
    g_frame.input.state.crouch.pressed = true;
    ConnectNativeInput(nullptr);
    if (g_release_pending.load(std::memory_order_acquire) == 0 ||
        BlackPlagueVrOwnsCrouchQuery({true, false, true, true, true})) {
        return fail("disconnect did not create a release ownership gap");
    }
    g_release_pending.store(0, std::memory_order_release);

    std::array<std::uint8_t, 0x400> player_a{};
    std::array<std::uint8_t, 0x400> player_b{};
    const auto generation_a = ObserveNativePlayerForUpdate(player_a.data());
    AcquireSRWLockExclusive(&g_crouch_edge_lock);
    g_native_crouch_press_token = {session_generation, generation_a};
    ReleaseSRWLockExclusive(&g_crouch_edge_lock);
    g_vr_crouch_owned = true;
    g_vr_crouch_owner_player = player_a.data();
    const auto generation_b = ObserveNativePlayerForUpdate(player_b.data());
    AcquireSRWLockShared(&g_crouch_edge_lock);
    const auto replacement_edge = g_native_crouch_press_token;
    ReleaseSRWLockShared(&g_crouch_edge_lock);
    if (generation_b == generation_a || replacement_edge.session_generation != 0 ||
        g_vr_crouch_owned || g_vr_crouch_owner_player != nullptr) {
        return fail("player replacement retained old crouch ownership");
    }

    std::array<std::uint8_t, 0x400> body{};
    void* const body_pointer = body.data();
    ContractWrite(player_b.data(), kPlayerCharacterBodyOffset, body_pointer);
    ContractWrite(player_b.data(), kPlayerMoveStateIndexOffset, kWalkMoveState);
    ContractWrite(body.data(), kCharacterSizeYOffset, 1.65F);
    BlackPlagueNativeCrouchStatus status;
    ServiceNativeVrCrouch(player_b.data(), true, status);
    if (!status.vr_stance_owned || status.native_move_state != kCrouchMoveState ||
        !status.native_shape_known || !status.native_crouched) {
        return fail("crouch entry did not reach ChangeMoveState(4)");
    }
    g_contract_block_stand = true;
    ServiceNativeVrCrouch(player_b.data(), false, status);
    if (!status.vr_stance_owned || !status.stand_blocked ||
        status.native_move_state != kCrouchMoveState ||
        status.stand_retries == 0) {
        return fail("blocked stand did not remain owned and retryable");
    }
    g_contract_block_stand = false;
    ServiceNativeVrCrouch(player_b.data(), false, status);
    if (status.vr_stance_owned || status.stand_blocked ||
        status.native_move_state != kWalkMoveState ||
        !status.native_shape_known || status.native_crouched) {
        return fail("stand retry did not reach ChangeMoveState(0)");
    }

    cleanup();
    return true;
}
#endif
}
