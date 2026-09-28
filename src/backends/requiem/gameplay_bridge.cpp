#include "gameplay_bridge.hpp"

#include "gameplay_contract.hpp"
#include "hand_contact_probe.hpp"
#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#include "render_world.hpp"
#include "vr_locomotion.hpp"
#include "vr_crouch_policy.hpp"
#include "vr_native_intents.hpp"
#include "legacy_input_abi.hpp"
#include "vr_action_input.hpp"
#include "log.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace penumbra_vr::backends::requiem {
namespace {

using Update = void(__thiscall*)(void*, float);
using Move = void(__thiscall*)(void*, float, float);
using MoveStateGate = bool(__thiscall*)(void*, float, float);
using ChangeMoveState = void(__thiscall*)(void*, std::int32_t, bool);
using CharacterUpdate = void(__thiscall*)(void*, float);
using LegacyString = adapters::hpl1::LegacyInputString;
using Query = adapters::hpl1::LegacyInputQuery;
using NativeAction = runtime::NativeVrAction;
using NativeQuery = runtime::NativeVrQuery;
struct QueryEntry final {
    std::uintptr_t site;
    std::uintptr_t target;
    NativeAction action;
    NativeQuery query;
};
// Decoded from the initialized Requiem ButtonHandler::Update. Each site
// constructs the named legacy action and calls the exact native query.
constexpr QueryEntry kGameplayQueries[]{
    // Requiem cButtonHandler::Update UI branches, decoded from this build.
    // The original native query still runs first at each owned callsite.
    {0x3E2C, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x3E5A, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x3E8A, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x3EE0, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x3F0E, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x3F3E, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x3F97, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x3FC7, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x4009, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x4037, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x4067, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x40A5, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x40C5, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x40E5, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x4127, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x4155, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x41A1, 0xDAF10, NativeAction::back, NativeQuery::released},
    {0x4206, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x4252, 0xDAF10, NativeAction::select, NativeQuery::released},
    {0x4304, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x4332, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x437E, 0xDAF10, NativeAction::back, NativeQuery::released},
    {0x43E3, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x442F, 0xDAF10, NativeAction::select, NativeQuery::released},
    {0x44DD, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x458A, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x45CF, 0xDAF10, NativeAction::back, NativeQuery::released},
    {0x4626, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x466B, 0xDAF10, NativeAction::select, NativeQuery::released},
    {0x473A, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x47AC, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x47EE, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x483A, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x4886, 0xDAF10, NativeAction::select, NativeQuery::released},
    {0x4906, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x4973, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x49A1, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x49CF, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x4A1B, 0xDAF10, NativeAction::select, NativeQuery::released},
    {0x4AC0, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x4AEE, 0xDAFB0, NativeAction::select, NativeQuery::pressed},
    {0x4C0D, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x4C3B, 0xDAFB0, NativeAction::drag, NativeQuery::pressed},
    {0x4CBC, 0xDAF10, NativeAction::drag, NativeQuery::released},
    {0x4CEC, 0xDAFB0, NativeAction::back, NativeQuery::pressed},
    {0x4D38, 0xDAF10, NativeAction::back, NativeQuery::released},
    {0x4F58, 0xDAFB0, NativeAction::pause, NativeQuery::pressed},
    {0x507F, 0xDAFB0, NativeAction::inventory, NativeQuery::pressed},
    {0x50A7, 0xDAFB0, NativeAction::notebook, NativeQuery::pressed},
    {0x5107, 0xDAFB0, NativeAction::light, NativeQuery::pressed},
    {0x526F, 0xDAFB0, NativeAction::jump, NativeQuery::pressed},
    {0x5297, 0xDAE70, NativeAction::jump, NativeQuery::held},
    {0x52C1, 0xDAFB0, NativeAction::sprint, NativeQuery::pressed},
    {0x52E9, 0xDAF10, NativeAction::sprint, NativeQuery::released},
    {0x5400, 0xDAFB0, NativeAction::interact, NativeQuery::pressed},
    {0x5444, 0xDAF10, NativeAction::interact, NativeQuery::released},
    {0x546C, 0xDAFB0, NativeAction::examine, NativeQuery::pressed},
    {0x5494, 0xDAF10, NativeAction::examine, NativeQuery::released},
    {0x54BC, 0xDAFB0, NativeAction::holster, NativeQuery::pressed},
};
struct PointerEntry final {
    std::uintptr_t site;
    std::uintptr_t target;
    std::uintptr_t cursor;
};
constexpr PointerEntry kPointers[]{
    {0x44B7, 0x79CA0, 0xA0},
    {0x48D9, 0x08C60, 0x38},
    {0x4A6E, 0x08C60, 0x38},
    {0x4BB9, 0x948C0, 0x84},
    {0x4F3B, 0x6D030, 0xAC},
};
constexpr std::uintptr_t kInventoryDoubleQuerySite = 0x4C8C;
constexpr std::uintptr_t kInventoryDoubleQueryRva = 0xDB050;
using DoubleQuery = bool(__thiscall*)(void*, LegacyString, float);

struct Vec3 final {
    float x{};
    float y{};
    float z{};
};

struct PendingLocomotion final {
    void* character_body = nullptr;
    std::array<float, 3> displacement{};
    std::array<float, 3> direct_request{};
    std::array<float, 3> physical_request{};
    bool active = false;
};

struct HeadTrackingInput final {
    runtime::VrHmdPose pose{};
    float world_yaw = 0.0F;
    std::uint64_t sampled_at_ms = 0;
    bool valid = false;
};

struct PhysicalState final { // Native physics thread only.
    void* body = nullptr;
    std::uint64_t generation = 0;
    runtime::VrTrackingSampleIdentity identity{};
    std::array<float, 3> previous_tracking{};
    std::array<float, 3> previous_body{};
    std::array<float, 3> head_anchor{};
    bool initialized = false;
};

struct PhysicalTick final { // The same thread enters the native update and D7C21.
    void* body = nullptr;
    std::array<float, 3> position_before_injection{};
    std::array<float, 3> physical_request{};
    std::array<float, 3> direct_request{};
    bool injected = false;
};

std::uint8_t* g_image = nullptr;
hooks::IatHook g_update_hook;
std::array<hooks::Rel32CallHook, 2> g_move_hooks{};
std::array<hooks::Rel32CallHook, std::size(kGameplayQueries)> g_query_hooks{};
std::array<hooks::Rel32CallHook, std::size(kPointers)> g_pointer_hooks{};
hooks::Rel32CallHook g_inventory_double_query_hook;
hooks::Rel32CallHook g_character_update_hook;
hooks::Rel32JumpHook g_physical_move_hook;
std::uintptr_t g_physical_move_resume = 0;
std::atomic<bool> g_installed{false};
std::atomic<unsigned> g_active_callbacks{0};
std::atomic<bool> g_crouch_reset_requested{false};
std::atomic<bool> g_native_ui_active{false};
std::atomic<NativeUiSurface> g_native_ui_surface{NativeUiSurface::none};
std::atomic<bool> g_recenter_body_requested{false};
std::atomic<std::uint64_t> g_vr_interact_presses{0};
std::atomic<std::uint64_t> g_vr_inventory_presses{0};
std::atomic<std::uint64_t> g_vr_jump_presses{0};
std::atomic<std::uint64_t> g_vr_jump_held_queries{0};
std::atomic<std::uint64_t> g_ui_updates{0};
std::atomic<std::uint64_t> g_native_move_enters{0};
runtime::VrPhysicalCrouchPolicy g_crouch_policy; // ButtonHandler update thread.
runtime::VrSnapTurn g_turn; // ButtonHandler update thread.
void* g_crouch_player = nullptr;
void* g_crouch_body = nullptr;
void* g_crouch_owner_player = nullptr;
bool g_vr_crouch_owned = false;
bool g_stand_blocked = false;
SRWLOCK g_tracking_state_lock = SRWLOCK_INIT;
RequiemBodyTrackingSample g_tracking_state;
std::uint64_t g_tracking_body_generation = 0;
std::atomic<void*> g_tracked_player_body{nullptr};
SRWLOCK g_head_tracking_lock = SRWLOCK_INIT;
HeadTrackingInput g_head_tracking;
PhysicalState g_physical_state;
thread_local PhysicalTick g_physical_tick;

SRWLOCK g_session_lock = SRWLOCK_INIT;
runtime::OpenVrSession* g_session = nullptr;
SRWLOCK g_frame_lock = SRWLOCK_INIT;
runtime::VrControllerFrame g_frame;

SRWLOCK g_pending_lock = SRWLOCK_INIT;
PendingLocomotion g_pending;

thread_local bool g_direct_locomotion = false;
thread_local runtime::VrNativeIntents* g_intents = nullptr;
thread_local void* g_current_player = nullptr;
thread_local bool g_pointer_valid = false;
thread_local std::array<float, 2> g_pointer_uv{};
thread_local std::uint64_t g_mouse_override_until = 0;
thread_local bool g_native_axis_observed = false;
thread_local bool g_forward_allowed = false;
thread_local bool g_sideways_allowed = false;
thread_local runtime::VrAnalogState g_direct_move{};
thread_local runtime::VrMatrix44 g_direct_head_world_pose{};
thread_local bool g_direct_sprinting = false;

struct CallbackScope final {
    CallbackScope() noexcept {
        g_active_callbacks.fetch_add(1, std::memory_order_acq_rel);
    }
    ~CallbackScope() {
        g_active_callbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void __cdecl BeginPhysicalMoveCallback() noexcept {
    g_active_callbacks.fetch_add(1, std::memory_order_acq_rel);
}

void __cdecl EndPhysicalMoveCallback() noexcept {
    g_active_callbacks.fetch_sub(1, std::memory_order_acq_rel);
}

template <typename T>
[[nodiscard]] T Read(const void* base, std::uintptr_t offset) noexcept {
    if (base == nullptr) return T{};
    __try {
        return *reinterpret_cast<const T*>(
            static_cast<const std::uint8_t*>(base) + offset);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return T{};
    }
}

template <std::size_t N>
[[nodiscard]] bool ImageBytesMatch(
    std::uintptr_t rva,
    const std::array<std::uint8_t, N>& expected) noexcept {
    for (std::size_t index = 0; index < N; ++index) {
        if (Read<std::uint8_t>(g_image, rva + index) != expected[index])
            return false;
    }
    return true;
}

void ServiceNativeVrCrouch(void* player, bool desired) noexcept {
    const auto& contract = GameplayContract();
    const bool body_present =
        Read<void*>(player, contract.character_body_offset) != nullptr;
    if (!body_present) {
        g_vr_crouch_owned = false;
        g_crouch_owner_player = nullptr;
        g_stand_blocked = false;
        return;
    }
    if (g_crouch_owner_player != player) {
        g_vr_crouch_owned = false;
        g_stand_blocked = false;
    }
    if (desired) g_stand_blocked = false;
    const auto move_state = Read<std::int32_t>(
        player, contract.move_state_index_offset);
    if (desired && move_state == 4) {
        g_vr_crouch_owned = true;
        g_crouch_owner_player = player;
        g_stand_blocked = false;
        return;
    }
    const auto transition = PlanNativeCrouchTransition(
        body_present, move_state, desired, g_vr_crouch_owned);
    const bool was_blocked = g_stand_blocked;
    if (transition != NativeCrouchTransition::none) {
        reinterpret_cast<ChangeMoveState>(
            g_image + contract.change_move_state_rva)(player,
                transition == NativeCrouchTransition::enter ? 4 : 0, false);
    }
    const auto resulting_state = Read<std::int32_t>(
        player, contract.move_state_index_offset);
    if (desired && resulting_state == 4) {
        g_vr_crouch_owned = true;
        g_crouch_owner_player = player;
        g_stand_blocked = false;
    } else if (!desired && g_vr_crouch_owned) {
        g_stand_blocked = resulting_state == 4;
        if (!g_stand_blocked) {
            g_vr_crouch_owned = false;
            g_crouch_owner_player = nullptr;
        }
    }
    if (transition != NativeCrouchTransition::none &&
        (resulting_state != move_state || g_stand_blocked != was_blocked)) {
        probe::WriteLog(
            "Requiem VR crouch request=%s state=%ld->%ld stand_blocked=%u",
            transition == NativeCrouchTransition::enter ? "enter" : "stand",
            static_cast<long>(move_state),
            static_cast<long>(resulting_state),
            g_stand_blocked ? 1U : 0U);
    }
}

[[nodiscard]] std::array<std::uint8_t, 5> CallBytes(
    std::uintptr_t site, std::uintptr_t target) noexcept {
    std::array<std::uint8_t, 5> bytes{0xE8};
    const auto displacement = static_cast<std::int32_t>(target - site - 5);
    std::memcpy(bytes.data() + 1, &displacement, sizeof(displacement));
    return bytes;
}

void ResetThreadDirectLocomotion() noexcept {
    g_direct_locomotion = false;
    g_native_axis_observed = false;
    g_forward_allowed = false;
    g_sideways_allowed = false;
    g_direct_move = {};
    g_direct_head_world_pose = {};
    g_direct_sprinting = false;
}

void InvalidatePendingLocomotion() noexcept {
    AcquireSRWLockExclusive(&g_pending_lock);
    g_pending = {};
    ReleaseSRWLockExclusive(&g_pending_lock);
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

[[nodiscard]] bool NativeMoveAxisAllowsDirectLocomotion(
    void* player, float amount, float delta_seconds,
    bool sideways) noexcept {
    if (player == nullptr || !std::isfinite(amount) ||
        !std::isfinite(delta_seconds) ||
        std::abs(amount) <= 0.00001F || delta_seconds <= 0.0F) {
        return false;
    }

    const auto& contract = GameplayContract();
    auto* const primary = CurrentMoveState(
        player, contract.primary_state_vector_offset,
        contract.primary_state_index_offset);
    if (Read<std::int32_t>(player,
            contract.primary_state_index_offset) == 1) {
        // Native Push applies force in the camera axes captured on entry.
        // Direct VR locomotion moves in HMD axes, so feed those same world
        // directions to the native Push gates before the body step.
        if (Read<void*>(primary, 0) != g_image + 0x27E178) return false;
        runtime::VrAnalogState axis{};
        axis.active = true;
        if (sideways) axis.x = amount;
        else axis.y = amount;
        const auto desired = runtime::HeadRelativeMoveDirection(
            g_direct_head_world_pose, axis);
        const auto forward = Read<std::array<float,3>>(primary, 0x14);
        const auto right = Read<std::array<float,3>>(primary, 0x20);
        const auto projection = RequiemPushAxisProjection(
            desired, forward, right);
        if (!projection.valid) return false;
        bool accepted = false;
        if (std::abs(projection.forward) > 0.00001F) {
            const bool forward_accepted = CallMoveStateGate(primary,
                contract.primary_forward_gate_slot,
                projection.forward, delta_seconds);
            accepted = accepted || forward_accepted;
        }
        if (std::abs(projection.sideways) > 0.00001F) {
            const bool sideways_accepted = CallMoveStateGate(primary,
                contract.primary_sideways_gate_slot,
                projection.sideways, delta_seconds);
            accepted = accepted || sideways_accepted;
        }
        if (!accepted) return false;
    } else if (!CallMoveStateGate(primary,
                   sideways ? contract.primary_sideways_gate_slot
                            : contract.primary_forward_gate_slot,
                   amount, delta_seconds)) {
        return false;
    }

    auto* const secondary = CurrentMoveState(
        player, contract.secondary_state_vector_offset,
        contract.secondary_state_index_offset);
    if (!CallMoveStateGate(secondary,
            sideways ? contract.secondary_sideways_gate_slot
                     : contract.secondary_forward_gate_slot,
            amount, delta_seconds)) {
        return false;
    }

    return Read<std::int32_t>(player, contract.movement_count_offset) > 0 ||
        Read<std::uint8_t>(player, contract.movement_override_offset) != 0;
}

void MarkDirectLocomotionAccepted(void* player) noexcept {
    if (player == nullptr) return;
    const auto offset = GameplayContract().accepted_motion_offset;
    __try {
        *(static_cast<std::uint8_t*>(player) + offset) = 1;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}

[[nodiscard]] bool QueueLocomotionDisplacement(
    void* player, const std::array<float, 3>& displacement) noexcept {
    const auto& contract = GameplayContract();
    auto* const body = Read<void*>(player, contract.character_body_offset);
    if (body == nullptr || !std::isfinite(displacement[0]) ||
        !std::isfinite(displacement[2])) {
        return false;
    }

    std::array<float, 3> bounded{displacement[0], 0.0F, displacement[2]};
    const float length = std::hypot(bounded[0], bounded[2]);
    if (!std::isfinite(length) || length <= 0.000001F) return false;
    if (length > runtime::vr_locomotion_policy::kMaximumPhysicalBodyStep) {
        const float scale =
            runtime::vr_locomotion_policy::kMaximumPhysicalBodyStep / length;
        bounded[0] *= scale;
        bounded[2] *= scale;
    }

    AcquireSRWLockExclusive(&g_pending_lock);
    g_pending.character_body = body;
    g_pending.displacement = bounded;
    g_pending.direct_request = bounded;
    g_pending.physical_request = {};
    g_pending.active = true;
    ReleaseSRWLockExclusive(&g_pending_lock);
    return true;
}

void __cdecl ApplyQueuedLocomotion(
    void* character_body, Vec3* position,
    Vec3* native_step_motion) noexcept {
    if (character_body == nullptr || position == nullptr) return;

    // This exact physical-move owner already exposes the native body position.
    // The Requiem GetSize/SetActiveSize instructions prove +0xC8 is the
    // active capsule height. Only the player's body needs the extra sample.
    if (g_tracked_player_body.load(std::memory_order_acquire) == character_body) {
        const float active_size_y = Read<float>(
            character_body, GameplayContract().character_size_y_offset);
        __try {
            const float body_center_y = position->y;
            if (std::isfinite(body_center_y) &&
                std::isfinite(active_size_y) && active_size_y > 0.0F) {
                AcquireSRWLockExclusive(&g_tracking_state_lock);
                if (g_tracking_state.character_body == character_body) {
                    g_tracking_state.body_center_y = body_center_y;
                    g_tracking_state.active_size_y = active_size_y;
                    g_tracking_state.sampled_at_ms = GetTickCount64();
                    g_tracking_state.body_height_valid = true;
                }
                ReleaseSRWLockExclusive(&g_tracking_state_lock);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    std::array<float, 3> displacement{};
    AcquireSRWLockExclusive(&g_pending_lock);
    if (!g_pending.active || g_pending.character_body != character_body) {
        ReleaseSRWLockExclusive(&g_pending_lock);
        return;
    }
    displacement = g_pending.displacement;
    if (g_physical_tick.body == character_body) {
        g_physical_tick.position_before_injection =
            {position->x, position->y, position->z};
        g_physical_tick.direct_request = g_pending.direct_request;
        g_physical_tick.physical_request = g_pending.physical_request;
        g_physical_tick.injected = true;
    }
    g_pending = {};
    ReleaseSRWLockExclusive(&g_pending_lock);

    __try {
        position->x += displacement[0];
        position->z += displacement[2];
        if (native_step_motion != nullptr) {
            native_step_motion->x += displacement[0];
            native_step_motion->z += displacement[2];
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}

[[nodiscard]] bool FreshHeadTracking(HeadTrackingInput& sample) noexcept {
    AcquireSRWLockShared(&g_head_tracking_lock);
    sample = g_head_tracking;
    ReleaseSRWLockShared(&g_head_tracking_lock);
    const auto now = GetTickCount64();
    return sample.valid && sample.sampled_at_ms != 0 &&
        now >= sample.sampled_at_ms && now - sample.sampled_at_ms <= 250 &&
        sample.pose.pose_valid && sample.pose.device_connected &&
        std::isfinite(sample.world_yaw);
}

void InvalidateRoomScaleSample(void* character_body) noexcept {
    AcquireSRWLockExclusive(&g_tracking_state_lock);
    if (g_tracking_state.character_body == character_body)
        g_tracking_state.room_scale_valid = false;
    ReleaseSRWLockExclusive(&g_tracking_state_lock);
}

void __fastcall HookedCharacterUpdate(
    void* character_body, void*, float dt) noexcept {
    CallbackScope callback;
    const auto original = reinterpret_cast<CharacterUpdate>(
        g_image + GameplayContract().character_update_rva);
    const bool tracked_character_body = character_body != nullptr &&
        g_tracked_player_body.load(std::memory_order_acquire) == character_body;
    if (!tracked_character_body ||
        !std::isfinite(dt) || dt <= 0.0F || dt > 0.25F) {
        original(character_body, dt);
        if (tracked_character_body)
            ServiceGameplayPalmResolver(g_image, character_body);
        return;
    }

    const Vec3 before = Read<Vec3>(
        character_body, GameplayContract().character_position_offset);
    const std::array<float, 3> body_before{before.x, before.y, before.z};
    HeadTrackingInput tracking;
    const auto body_sample = ReadGameplayTrackingSample();
    if (g_recenter_body_requested.exchange(false, std::memory_order_acq_rel))
        g_physical_state = {};
    if (!FreshHeadTracking(tracking) ||
        body_sample.character_body != character_body ||
        !std::isfinite(before.x) || !std::isfinite(before.y) ||
        !std::isfinite(before.z)) {
        g_physical_state = {};
        InvalidateRoomScaleSample(character_body);
        original(character_body, dt);
        ServiceGameplayPalmResolver(g_image, character_body);
        return;
    }

    const auto& pose = tracking.pose.device_to_absolute.values;
    const std::array<float, 3> tracking_position{
        pose[3], pose[7], pose[11]};
    if (!std::isfinite(tracking_position[0]) ||
        !std::isfinite(tracking_position[1]) ||
        !std::isfinite(tracking_position[2])) {
        g_physical_state = {};
        InvalidateRoomScaleSample(character_body);
        original(character_body, dt);
        ServiceGameplayPalmResolver(g_image, character_body);
        return;
    }

    const bool epoch_changed = g_physical_state.initialized &&
        g_physical_state.identity.pose_epoch != 0 &&
        tracking.pose.identity.pose_epoch != 0 &&
        !runtime::SameTrackingEpoch(
            g_physical_state.identity, tracking.pose.identity);
    const bool rebase = !g_physical_state.initialized ||
        g_physical_state.body != character_body ||
        g_physical_state.generation != body_sample.body_generation ||
        epoch_changed ||
        std::hypot(
            tracking_position[0] - g_physical_state.previous_tracking[0],
            tracking_position[2] - g_physical_state.previous_tracking[2]) >
            runtime::vr_locomotion_policy::kMaximumHeadBodySeparation ||
        std::hypot(body_before[0] - g_physical_state.previous_body[0],
            body_before[2] - g_physical_state.previous_body[2]) >
            runtime::vr_locomotion_policy::kMaximumHeadBodySeparation;
    std::array<float, 3> world_delta{};
    if (!rebase) {
        const float dx = tracking_position[0] -
            g_physical_state.previous_tracking[0];
        const float dz = tracking_position[2] -
            g_physical_state.previous_tracking[2];
        const float cosine = std::cos(tracking.world_yaw);
        const float sine = std::sin(tracking.world_yaw);
        world_delta = {cosine * dx + sine * dz, 0.0F,
            -sine * dx + cosine * dz};
    }
    const auto plan = runtime::PlanBodyReconciliation(
        rebase ? body_before : g_physical_state.head_anchor,
        body_before, world_delta);
    if (!plan.valid) {
        g_physical_state = {};
        InvalidateRoomScaleSample(character_body);
        original(character_body, dt);
        ServiceGameplayPalmResolver(g_image, character_body);
        return;
    }

    g_physical_tick = {};
    g_physical_tick.body = character_body;
    AcquireSRWLockExclusive(&g_pending_lock);
    const bool direct_pending = g_pending.active &&
        g_pending.character_body == character_body;
    const std::array<float, 3> direct = direct_pending
        ? g_pending.direct_request : std::array<float, 3>{};
    const auto physical = plan.physical_request;
    if (std::hypot(physical[0], physical[2]) > 0.000001F) {
        // Physical tracking has priority at the native 5 cm step cap. Reduce
        // only stick displacement when both requests share this one solve.
        std::array<float, 3> bounded_direct = direct;
        const float combined_length = std::hypot(
            physical[0] + direct[0], physical[2] + direct[2]);
        const float limit = runtime::vr_locomotion_policy::kMaximumPhysicalBodyStep;
        if (combined_length > limit) {
            const float a = direct[0] * direct[0] + direct[2] * direct[2];
            if (a > 0.000000000001F) {
                const float b = 2.0F *
                    (physical[0] * direct[0] + physical[2] * direct[2]);
                const float c = physical[0] * physical[0] +
                    physical[2] * physical[2] - limit * limit;
                const float scale = std::clamp(
                    (-b + std::sqrt(std::max(0.0F, b*b - 4.0F*a*c))) /
                        (2.0F*a), 0.0F, 1.0F);
                bounded_direct[0] *= scale;
                bounded_direct[2] *= scale;
            } else {
                bounded_direct = {};
            }
        }
        g_pending = {character_body,
            {physical[0] + bounded_direct[0], 0.0F,
             physical[2] + bounded_direct[2]},
            bounded_direct, physical, true};
    }
    ReleaseSRWLockExclusive(&g_pending_lock);

    original(character_body, dt); // Native physics owns exactly one update.
    ServiceGameplayPalmResolver(g_image, character_body);
    const Vec3 after = Read<Vec3>(
        character_body, GameplayContract().character_position_offset);
    const std::array<float, 3> body_after{after.x, after.y, after.z};
    runtime::VrAcceptedBodyMotion native_motion;
    if (!runtime::ObserveAcceptedBodyMotion(
            body_before, body_after, native_motion)) {
        g_physical_state = {};
        InvalidateRoomScaleSample(character_body);
        g_physical_tick = {};
        return;
    }
    std::array<float, 3> physical_accepted{};
    if (g_physical_tick.injected) {
        runtime::VrAcceptedBodyMotion injected_motion;
        if (!runtime::ObserveAcceptedBodyMotion(
                g_physical_tick.position_before_injection,
                body_after, injected_motion)) {
            g_physical_state = {};
            InvalidateRoomScaleSample(character_body);
            g_physical_tick = {};
            return;
        }
        physical_accepted = AttributeRequiemPhysicalAcceptance(
            g_physical_tick.physical_request,
            g_physical_tick.direct_request,
            injected_motion.accepted_displacement);
    }
    runtime::VrAcceptedBodyMotion physical_motion{};
    physical_motion.body_before = body_before;
    physical_motion.accepted_displacement = physical_accepted;
    physical_motion.body_after = {
        body_before[0] + physical_accepted[0], body_before[1],
        body_before[2] + physical_accepted[2]};
    const auto reconciliation = runtime::ReconcilePhysicalBodyMotion(
        plan, physical_motion);
    if (!reconciliation.valid) {
        g_physical_state = {};
        InvalidateRoomScaleSample(character_body);
        g_physical_tick = {};
        return;
    }
    auto locomotion_motion = native_motion;
    locomotion_motion.accepted_displacement[0] -= physical_accepted[0];
    locomotion_motion.accepted_displacement[1] = 0.0F;
    locomotion_motion.accepted_displacement[2] -= physical_accepted[2];
    locomotion_motion.body_after = {
        body_before[0] + locomotion_motion.accepted_displacement[0],
        body_before[1],
        body_before[2] + locomotion_motion.accepted_displacement[2]};
    auto anchor = runtime::CarryHeadAnchorWithLocomotion(
        reconciliation.head_anchor, locomotion_motion);
    const float active_size_y = Read<float>(character_body,
        GameplayContract().character_size_y_offset);
    if (!std::isfinite(active_size_y) || active_size_y <= 0.0F) {
        g_physical_state = {};
        InvalidateRoomScaleSample(character_body);
        g_physical_tick = {};
        return;
    }
    anchor[1] = body_after[1] - active_size_y * 0.5F;
    g_physical_state = {character_body, body_sample.body_generation,
        tracking.pose.identity, tracking_position, body_after, anchor, true};
    AcquireSRWLockExclusive(&g_tracking_state_lock);
    if (g_tracking_state.character_body == character_body &&
        g_tracking_state.body_generation == body_sample.body_generation) {
        g_tracking_state.room_scale_valid = true;
        g_tracking_state.head_anchor = anchor;
        g_tracking_state.body_position = body_after;
        g_tracking_state.observed_tracking_pose =
            tracking.pose.device_to_absolute;
        g_tracking_state.tracking_identity = tracking.pose.identity;
        g_tracking_state.physical_reconciliation = reconciliation;
        g_tracking_state.sampled_at_ms = GetTickCount64();
    }
    ReleaseSRWLockExclusive(&g_tracking_state_lock);
    AcquireSRWLockExclusive(&g_pending_lock);
    if (g_pending.character_body == character_body) g_pending = {};
    ReleaseSRWLockExclusive(&g_pending_lock);
    g_physical_tick = {};
}

__declspec(naked) void PhysicalMoveGateway() noexcept {
    __asm {
        pushfd
        pushad
        call BeginPhysicalMoveCallback
        lea eax, [esp + 44h]
        push eax
        push edi
        push esi
        call ApplyQueuedLocomotion
        add esp, 0Ch
        call EndPhysicalMoveCallback
        popad
        popfd
        fld dword ptr [edi]
        fld dword ptr [esi + 54h]
        jmp dword ptr [g_physical_move_resume]
    }
}

void PublishDirectLocomotion(void* player, float dt) noexcept {
    const auto plan = PlanDirectLocomotionPublication(
        g_native_axis_observed,
        g_direct_move.x,
        g_direct_move.y,
        g_sideways_allowed,
        g_forward_allowed,
        true);
    if (!plan.publish) {
        InvalidatePendingLocomotion();
        return;
    }

    runtime::VrAnalogState move;
    move.active = true;
    move.x = plan.move_x;
    move.y = plan.move_y;
    const auto direction = runtime::HeadRelativeMoveDirection(
        g_direct_head_world_pose, move);
    const bool pushing = Read<std::int32_t>(
        player, GameplayContract().primary_state_index_offset) == 1;
    const auto displacement = RequiemLocomotionDisplacement(
        direction, dt, g_direct_sprinting, pushing);
    const bool published = QueueLocomotionDisplacement(player, displacement);
    if (ShouldMarkDirectLocomotionAccepted(plan, published)) {
        MarkDirectLocomotionAccepted(player);
    } else {
        InvalidatePendingLocomotion();
    }
}

bool __fastcall HookedQuery(
    void* input, void*, LegacyString name) noexcept {
    CallbackScope callback;
    const auto return_rva = reinterpret_cast<std::uintptr_t>(
        _ReturnAddress()) - reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& entry : kGameplayQueries) {
        if (entry.site + 5 != return_rva) continue;
        const bool native = reinterpret_cast<Query>(
            g_image + entry.target)(input, name);
        bool vr = g_intents != nullptr &&
            g_intents->Query(entry.action, entry.query);
        if (vr && entry.action == NativeAction::interact &&
            entry.query == NativeQuery::pressed) {
            g_vr_interact_presses.fetch_add(1, std::memory_order_relaxed);
            RefreshVrSelectionBeforeInteract(g_current_player);
        }
        if (vr && entry.action == NativeAction::inventory)
            g_vr_inventory_presses.fetch_add(1, std::memory_order_relaxed);
        if (vr && entry.action == NativeAction::jump) {
            (entry.query == NativeQuery::held
                ? g_vr_jump_held_queries : g_vr_jump_presses)
                .fetch_add(1, std::memory_order_relaxed);
        }
        return native || vr;
    }
    // Defensive fallback: keep the legacy string lifetime in the native CRT.
    return reinterpret_cast<Query>(g_image + 0xDAFB0)(input, name);
}

bool __fastcall HookedInventoryDoubleQuery(
    void* input, void*, LegacyString name, float window_seconds) noexcept {
    CallbackScope callback;
    // Requiem's native 0.2 s LeftClick double-query owns the same default-use
    // action as BP. Preserve native edge/string lifetime, then let R2 select.
    const bool native = reinterpret_cast<DoubleQuery>(
        g_image + kInventoryDoubleQueryRva)(input, name, window_seconds);
    const bool vr = g_intents && g_intents->Query(
        NativeAction::select, NativeQuery::pressed);
    return native || vr;
}

void __fastcall HookedPointer(
    void* menu, void*, const std::array<float, 2>* physical_delta) noexcept {
    CallbackScope callback;
    using Pointer = void(__thiscall*)(void*, const std::array<float, 2>*);
    const auto return_rva = reinterpret_cast<std::uintptr_t>(
        _ReturnAddress()) - reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& entry : kPointers) {
        if (entry.site + 5 != return_rva) continue;
        if (physical_delta == nullptr) return;
        auto delta = Read<std::array<float, 2>>(physical_delta, 0);
        const auto now = GetTickCount64();
        if (std::abs(delta[0]) > 0.01F || std::abs(delta[1]) > 0.01F)
            g_mouse_override_until = now + 1500;
        if (g_intents && g_pointer_valid && now >= g_mouse_override_until) {
            const float x = Read<float>(menu, entry.cursor);
            const float y = Read<float>(menu, entry.cursor + 4);
            std::array<float, 2> smoothed{};
            if (runtime::SmoothMenuPointerUv(
                    {x / 800.0F, y / 600.0F}, g_pointer_uv,
                    0.40F, smoothed)) {
                delta = {smoothed[0] * 800.0F - x,
                         smoothed[1] * 600.0F - y};
            }
        }
        reinterpret_cast<Pointer>(g_image + entry.target)(menu, &delta);
        return;
    }
}

void __fastcall HookedForward(
    void* player, void*, float amount, float dt) noexcept {
    CallbackScope callback;
    if (g_direct_locomotion && std::abs(amount) > 0.00001F) {
        g_native_axis_observed = true;
    }
    if (g_direct_locomotion && !g_native_axis_observed &&
        std::abs(g_direct_move.y) > 0.00001F) {
        g_forward_allowed = NativeMoveAxisAllowsDirectLocomotion(
            player, g_direct_move.y, dt, false);
        return;
    }
    const auto original = reinterpret_cast<Move>(
        g_image + GameplayContract().move_forward_rva);
    original(player, amount, dt);
}

void __fastcall HookedSideways(
    void* player, void*, float amount, float dt) noexcept {
    CallbackScope callback;
    if (g_direct_locomotion && std::abs(amount) > 0.00001F) {
        g_native_axis_observed = true;
    }
    if (g_direct_locomotion && !g_native_axis_observed &&
        std::abs(g_direct_move.x) > 0.00001F) {
        g_sideways_allowed = NativeMoveAxisAllowsDirectLocomotion(
            player, g_direct_move.x, dt, true);
    } else {
        const auto original = reinterpret_cast<Move>(
            g_image + GameplayContract().move_sideways_rva);
        original(player, amount, dt);
    }

    if (g_direct_locomotion) PublishDirectLocomotion(player, dt);
}

void __fastcall HookedUpdate(void* handler, void*, float dt) noexcept {
    CallbackScope callback;
    InvalidatePendingLocomotion();
    ResetThreadDirectLocomotion();

    const auto& contract = GameplayContract();
    void* const player = Read<void*>(handler, contract.handler_player_offset);
    void* const init = Read<void*>(handler, 0x2C);
    runtime::VrControllerFrame frame;
    bool input_ready = false;
    const bool in_game = Read<std::int32_t>(handler, 0x3C) == 1;
    // These active bytes are written by Requiem's own SetActive methods:
    // inventory 0x6CF20 and notebook 0x96760. The handler accesses init+0x2C.
    const bool inventory_active =
        Read<bool>(Read<void*>(init, 0x164), 0x5C);
    const bool notebook_active =
        Read<bool>(Read<void*>(init, 0x178), 0x44);
    const bool ui = !in_game || inventory_active || notebook_active;
    const auto surface = !in_game ? NativeUiSurface::fullscreen
        : inventory_active ? NativeUiSurface::inventory
        : notebook_active ? NativeUiSurface::notebook
        : NativeUiSurface::none;
    if (ui) g_ui_updates.fetch_add(1, std::memory_order_relaxed);
    g_native_ui_active.store(ui, std::memory_order_release);
    g_native_ui_surface.store(surface, std::memory_order_release);
    AcquireSRWLockExclusive(&g_session_lock);
    if (g_session != nullptr) {
        std::string input_error;
        input_ready = g_session->ReadControllerInput(
            ui ? runtime::VrInputContext::ui
               : runtime::VrInputContext::gameplay,
            runtime::VrHand::right,
            GetTickCount64(), frame, input_error);
    }
    ReleaseSRWLockExclusive(&g_session_lock);
    AcquireSRWLockExclusive(&g_frame_lock);
    g_frame = input_ready ? frame : runtime::VrControllerFrame{};
    ReleaseSRWLockExclusive(&g_frame_lock);

    void* const current_body =
        Read<void*>(player, contract.character_body_offset);
    if (g_crouch_reset_requested.exchange(false, std::memory_order_acq_rel) ||
        g_crouch_player != player || g_crouch_body != current_body) {
        g_crouch_policy.Reset();
        if (g_crouch_player != player || g_crouch_body != current_body) {
            g_vr_crouch_owned = false;
            g_crouch_owner_player = nullptr;
            g_stand_blocked = false;
        }
        g_crouch_player = player;
        g_crouch_body = current_body;
    }
    float head_height = 0.0F;
    const bool body_present = current_body != nullptr;
    const bool gameplay_active = input_ready && frame.focused &&
        !ui && body_present;
    const float turn = g_turn.Update(frame.input.state.turn,
        gameplay_active, runtime::VrTurnMode::snap, dt,
        runtime::vr_setting_limits::kSnapTurnAngle.default_value,
        runtime::vr_setting_limits::kSmoothTurnSpeed.default_value,
        runtime::vr_setting_limits::kTurnDeadZone.default_value);
    if (turn != 0.0F) AddTrackedWorldYaw(-turn);
    const auto pointer = runtime::SelectUiPointerPose(
        frame, frame.interact_source);
    g_pointer_valid = ui && input_ready && frame.focused &&
        pointer.valid && TrackedMenuPointer(pointer.pose, g_pointer_uv);
    if (ui && !g_pointer_valid) {
        frame.input.state.ui_select = {};
        frame.input.state.ui_drag = {};
    }
    if (input_ready && frame.focused &&
        frame.input.state.recenter.just_pressed) {
        RequestTrackedRecenter();
        g_recenter_body_requested.store(true, std::memory_order_release);
    }
    // Inventory/notebook freeze gameplay actions, but a player can stand up
    // physically while either panel is open. Keep the Rework stance policy
    // informed by fresh HMD height and apply its native transition on exit.
    const bool stance_tracking_active = input_ready && frame.focused &&
        in_game && body_present;
    const bool tracking_valid = stance_tracking_active &&
        TrackedHeadTrackingHeight(head_height);
    const auto crouch_button = ui ? runtime::VrButtonState{} :
        frame.input.state.crouch;
    static_cast<void>(g_crouch_policy.Update(
        crouch_button,
        runtime::VrCrouchMode::hybrid,
        runtime::vr_setting_limits::kPhysicalCrouchDepth.default_value,
        stance_tracking_active,
        tracking_valid,
        head_height,
        g_stand_blocked));
    if (!ui) ServiceNativeVrCrouch(
        player, g_crouch_policy.status().desired_crouch);
    thread_local bool previous_ui = false;
    if (ui != previous_ui) {
        probe::WriteLog(
            "Requiem VR UI stance ui=%u tracked=%u head_y=%.3f physical=%u desired=%u native_state=%ld",
            ui ? 1U : 0U, tracking_valid ? 1U : 0U,
            head_height,
            g_crouch_policy.status().physical_crouch ? 1U : 0U,
            g_crouch_policy.status().desired_crouch ? 1U : 0U,
            static_cast<long>(Read<std::int32_t>(player,
                contract.move_state_index_offset)));
    }
    previous_ui = ui;
    AcquireSRWLockExclusive(&g_tracking_state_lock);
    if (g_tracking_state.character_body !=
            (gameplay_active ? current_body : nullptr)) {
        g_tracking_state = {};
        g_tracking_state.character_body = gameplay_active ? current_body : nullptr;
        g_tracking_state.body_generation = ++g_tracking_body_generation;
    }
    g_tracking_state.native_crouched = gameplay_active &&
        Read<std::int32_t>(player, contract.move_state_index_offset) == 4;
    g_tracking_state.physical_crouch =
        gameplay_active && g_crouch_policy.status().physical_crouch;
    g_tracked_player_body.store(
        gameplay_active ? current_body : nullptr, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_tracking_state_lock);

    runtime::VrMatrix44 head_world_pose;
    if (gameplay_active && player != nullptr &&
        frame.input.state.move.active &&
        std::hypot(frame.input.state.move.x, frame.input.state.move.y) > 0.00001F &&
        TrackedHeadWorldPose(head_world_pose)) {
        g_direct_locomotion = true;
        g_direct_move = frame.input.state.move;
        g_direct_head_world_pose = head_world_pose;
        g_direct_sprinting = frame.input.state.sprint.pressed;
    }

    // The hook ledger is cleared during detach. Resolve the exact-build native
    // target from the process-resident image so an update callback already in
    // flight can still finish after its vtable slot has been restored.
    const auto original = reinterpret_cast<Update>(
        g_image + contract.button_handler_update_rva);
    runtime::VrNativeIntents intents;
    intents.Begin(input_ready && frame.focused ? frame.input.state
                                               : runtime::VrInputState{},
        ui ? runtime::VrInputContext::ui
           : runtime::VrInputContext::gameplay);
    auto* const previous_intents = g_intents;
    auto* const previous_player = g_current_player;
    g_intents = &intents;
    g_current_player = player;
    const int interaction_state_before = Read<int>(
        player, contract.primary_state_index_offset);
    LARGE_INTEGER update_started{}, update_finished{};
    QueryPerformanceCounter(&update_started);
    if (original != nullptr) original(handler, dt);
    QueryPerformanceCounter(&update_finished);
    thread_local void* observed_player = nullptr;
    thread_local int observed_state = -1;
    const int current_state = Read<int>(player,
        contract.primary_state_index_offset);
    if (gameplay_active && frame.input.state.interact.just_pressed) {
        probe::WriteLog("Requiem VR interact state=%ld->%ld",
            static_cast<long>(interaction_state_before),
            static_cast<long>(current_state));
    }
    if (player != observed_player) observed_state = -1;
    if (current_state == 2 && observed_state != 2)
        g_native_move_enters.fetch_add(1, std::memory_order_relaxed);
    observed_player = player;
    observed_state = current_state;
    g_intents = previous_intents;
    g_current_player = previous_player;
    static std::uint64_t last_timing_log_ms = 0; // Game update thread only.
    static std::uint64_t update_ticks = 0;
    static std::uint64_t update_count = 0;
    if (update_finished.QuadPart >= update_started.QuadPart) {
        update_ticks += static_cast<std::uint64_t>(
            update_finished.QuadPart - update_started.QuadPart);
        ++update_count;
    }
    const std::uint64_t now_ms = GetTickCount64();
    if (last_timing_log_ms == 0) last_timing_log_ms = now_ms;
    if (now_ms >= last_timing_log_ms + 5000) {
        const auto timing = ConsumePresentationTiming();
        if (timing.world_frames > 0 && timing.ticks_per_second > 0) {
            const auto interaction = ConsumeInteractionCounters();
            const double elapsed_seconds =
                static_cast<double>(now_ms - last_timing_log_ms) / 1000.0;
            const double mean_render_ms =
                static_cast<double>(timing.world_render_ticks) * 1000.0 /
                static_cast<double>(timing.ticks_per_second) /
                static_cast<double>(timing.world_frames);
            const auto phase_ms = [&](std::uint64_t ticks) {
                return static_cast<double>(ticks) * 1000.0 /
                    static_cast<double>(timing.ticks_per_second) /
                    static_cast<double>(timing.world_frames);
            };
            LARGE_INTEGER frequency{};
            QueryPerformanceFrequency(&frequency);
            const double mean_update_ms = update_count > 0 &&
                frequency.QuadPart > 0
                ? static_cast<double>(update_ticks) * 1000.0 /
                    static_cast<double>(frequency.QuadPart) /
                    static_cast<double>(update_count)
                : 0.0;
            probe::WriteLog(
                "Requiem VR timing world_fps=%.1f render_mean_ms=%.1f left_ms=%.1f right_ms=%.1f overlay_ms=%.1f submit_ms=%.1f update_mean_ms=%.1f interact=%llu select_refresh=%llu select_ray=%llu select_hit=%llu select_winner=%llu native_grab_enter=%llu native_move_enter=%llu grab_enter=%llu grab_acquire=%llu grab_release=%llu move_enter=%llu move_acquire=%llu move_release=%llu mechanism_acquire=%llu tool_attached=%llu tool_native=%llu tool_render_aligned=%llu tool_visibility_aligned=%llu tool_palm_resolved=%llu tool_palm_raw=%llu tool_palm_gap_gt_2cm=%llu native_push_enter=%llu push_acquire=%llu push_force_ticks=%llu refract_native=%llu refract_copy=%llu refract_resize=%llu inventory=%llu jump_press=%llu jump_held=%llu ui_updates=%llu",
                static_cast<double>(timing.world_frames) / elapsed_seconds,
                mean_render_ms, phase_ms(timing.left_eye_ticks),
                phase_ms(timing.right_eye_ticks),
                phase_ms(timing.overlay_ticks),
                phase_ms(timing.submit_ticks), mean_update_ms,
                static_cast<unsigned long long>(g_vr_interact_presses.exchange(
                    0, std::memory_order_relaxed)),
                static_cast<unsigned long long>(interaction.selection_refreshes),
                static_cast<unsigned long long>(interaction.redirected_rays),
                static_cast<unsigned long long>(interaction.ray_hits),
                static_cast<unsigned long long>(interaction.ray_winners),
                static_cast<unsigned long long>(interaction.native_grab_enters),
                static_cast<unsigned long long>(interaction.native_move_enters),
                static_cast<unsigned long long>(interaction.grab_enters),
                static_cast<unsigned long long>(interaction.grabs_acquired),
                static_cast<unsigned long long>(interaction.grabs_released),
                static_cast<unsigned long long>(g_native_move_enters.exchange(
                    0, std::memory_order_relaxed)),
                static_cast<unsigned long long>(interaction.moves_acquired),
                static_cast<unsigned long long>(interaction.moves_released),
                static_cast<unsigned long long>(interaction.mechanisms_acquired),
                static_cast<unsigned long long>(interaction.tools_attached),
                static_cast<unsigned long long>(interaction.tools_native),
                static_cast<unsigned long long>(interaction.tools_render_aligned),
                static_cast<unsigned long long>(interaction.tools_visibility_aligned),
                static_cast<unsigned long long>(interaction.tools_visibility_resolved_palm),
                static_cast<unsigned long long>(interaction.tools_visibility_raw_palm),
                static_cast<unsigned long long>(interaction.tools_visibility_palm_gap_over_2cm),
                static_cast<unsigned long long>(interaction.native_push_enters),
                static_cast<unsigned long long>(interaction.pushes_acquired),
                static_cast<unsigned long long>(interaction.push_force_ticks),
                static_cast<unsigned long long>(interaction.refraction_native_calls),
                static_cast<unsigned long long>(interaction.refraction_copy_attempts),
                static_cast<unsigned long long>(interaction.refraction_resize_attempts),
                static_cast<unsigned long long>(g_vr_inventory_presses.exchange(
                    0, std::memory_order_relaxed)),
                static_cast<unsigned long long>(g_vr_jump_presses.exchange(
                    0, std::memory_order_relaxed)),
                static_cast<unsigned long long>(g_vr_jump_held_queries.exchange(
                    0, std::memory_order_relaxed)),
                static_cast<unsigned long long>(g_ui_updates.exchange(
                    0, std::memory_order_relaxed)));
        }
        last_timing_log_ms = now_ms;
        update_ticks = 0;
        update_count = 0;
    }
    ResetThreadDirectLocomotion();
}

[[nodiscard]] bool RemoveHookSet(std::string& error) noexcept {
    bool ok = true;
    auto append_error = [&](const std::string& next) {
        if (next.empty()) return;
        if (!error.empty()) error += "; ";
        error += next;
    };

    std::string next;
    if (g_update_hook.installed() && !hooks::RemoveIatHook(g_update_hook, next)) {
        ok = false;
        append_error(next);
    }
    for (auto it = g_move_hooks.rbegin(); it != g_move_hooks.rend(); ++it) {
        next.clear();
        if (it->installed() && !hooks::RemoveRel32CallHook(*it, next)) {
            ok = false;
            append_error(next);
        }
    }
    for (auto it = g_query_hooks.rbegin(); it != g_query_hooks.rend(); ++it) {
        next.clear();
        if (it->installed() && !hooks::RemoveRel32CallHook(*it, next)) {
            ok = false;
            append_error(next);
        }
    }
    for (auto it = g_pointer_hooks.rbegin(); it != g_pointer_hooks.rend(); ++it) {
        next.clear();
        if (it->installed() && !hooks::RemoveRel32CallHook(*it, next)) {
            ok = false;
            append_error(next);
        }
    }
    next.clear();
    if (g_inventory_double_query_hook.installed() &&
        !hooks::RemoveRel32CallHook(g_inventory_double_query_hook, next)) {
        ok = false;
        append_error(next);
    }
    next.clear();
    if (g_character_update_hook.installed() &&
        !hooks::RemoveRel32CallHook(g_character_update_hook, next)) {
        ok = false;
        append_error(next);
    }
    next.clear();
    if (g_physical_move_hook.installed() &&
        !hooks::RemoveRel32JumpHook(g_physical_move_hook, next)) {
        ok = false;
        append_error(next);
    }
    return ok;
}

} // namespace

bool InstallGameplayBridge(std::string& error) noexcept {
    error.clear();
    if (GameplayBridgeInstalled()) return true;
    bool query_hook_installed = false;
    for (const auto& hook : g_query_hooks)
        query_hook_installed = query_hook_installed || hook.installed();
    bool pointer_hook_installed = false;
    for (const auto& hook : g_pointer_hooks)
        pointer_hook_installed = pointer_hook_installed || hook.installed();
    if (g_update_hook.installed() || g_character_update_hook.installed() ||
        query_hook_installed ||
        pointer_hook_installed || g_inventory_double_query_hook.installed() ||
        g_move_hooks[0].installed() ||
        g_move_hooks[1].installed() || g_physical_move_hook.installed()) {
        error = "Requiem gameplay bridge is partially installed";
        return false;
    }

    auto* const image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "Could not resolve the Requiem module base";
        return false;
    }
    if (g_image == nullptr) {
        // Once callbacks have been published, keep the image base immutable so
        // an in-flight callback from a removed installation cannot race a later
        // installation rewriting the same global pointer.
        g_image = image;
    } else if (g_image != image) {
        error = "The Requiem image base changed after gameplay publication";
        return false;
    }

    const auto& contract = GameplayContract();
    for (const auto& entry : kGameplayQueries) {
        if (!ImageBytesMatch(entry.site,
                CallBytes(entry.site, entry.target))) {
            error = "Requiem native action query does not match the observed build";
            return false;
        }
    }
    for (const auto& entry : kPointers) {
        if (!ImageBytesMatch(entry.site,
                CallBytes(entry.site, entry.target))) {
            error = "Requiem native menu pointer does not match the observed build";
            return false;
        }
    }
    if (!ImageBytesMatch(kInventoryDoubleQuerySite,
            CallBytes(kInventoryDoubleQuerySite, kInventoryDoubleQueryRva))) {
        error = "Requiem inventory double query does not match the observed build";
        return false;
    }
    if (!ImageBytesMatch(contract.character_update_callsite_rva,
            CallBytes(contract.character_update_callsite_rva,
                contract.character_update_rva))) {
        error = "Requiem native body-update callsite does not match the observed build";
        return false;
    }
    if (!ImageBytesMatch(
            contract.change_move_state_rva,
            contract.change_move_state_window) ||
        !ImageBytesMatch(
            contract.crouch_state_proof_rva,
            contract.crouch_state_proof_window) ||
        !ImageBytesMatch(
            contract.get_character_size_y_rva,
            contract.get_character_size_y_window) ||
        !ImageBytesMatch(
            contract.set_active_size_y_rva,
            contract.set_active_size_y_window)) {
        error = "Requiem native crouch/active capsule boundary does not match the observed build";
        return false;
    }
    g_physical_move_resume = reinterpret_cast<std::uintptr_t>(
        g_image + contract.physical_move_owner_rva +
        contract.physical_move_owner_window.size());
    if (!hooks::InstallRel32JumpHook(
            g_image + contract.physical_move_owner_rva,
            contract.physical_move_owner_window,
            reinterpret_cast<void*>(&PhysicalMoveGateway),
            g_physical_move_hook, error)) {
        return false;
    }

    if (!hooks::InstallRel32CallHook(
            g_image + contract.character_update_callsite_rva,
            CallBytes(contract.character_update_callsite_rva,
                contract.character_update_rva),
            reinterpret_cast<void*>(&HookedCharacterUpdate),
            g_character_update_hook, error)) {
        std::string rollback;
        static_cast<void>(RemoveHookSet(rollback));
        return false;
    }

    const std::array<std::uintptr_t, 2> sites{
        contract.forward_callsite_rva,
        contract.sideways_callsite_rva};
    const std::array<std::uintptr_t, 2> targets{
        contract.move_forward_rva,
        contract.move_sideways_rva};
    const std::array<void*, 2> replacements{
        reinterpret_cast<void*>(&HookedForward),
        reinterpret_cast<void*>(&HookedSideways)};
    bool ok = true;
    for (std::size_t index = 0;
        ok && index < std::size(kGameplayQueries); ++index) {
        const auto& entry = kGameplayQueries[index];
        ok = hooks::InstallRel32CallHook(
            g_image + entry.site,
            CallBytes(entry.site, entry.target),
            reinterpret_cast<void*>(&HookedQuery),
            g_query_hooks[index], error);
    }
    for (std::size_t index = 0; ok && index < std::size(kPointers); ++index) {
        const auto& entry = kPointers[index];
        ok = hooks::InstallRel32CallHook(
            g_image + entry.site,
            CallBytes(entry.site, entry.target),
            reinterpret_cast<void*>(&HookedPointer),
            g_pointer_hooks[index], error);
    }
    if (ok) {
        ok = hooks::InstallRel32CallHook(
            g_image + kInventoryDoubleQuerySite,
            CallBytes(kInventoryDoubleQuerySite, kInventoryDoubleQueryRva),
            reinterpret_cast<void*>(&HookedInventoryDoubleQuery),
            g_inventory_double_query_hook, error);
    }
    for (std::size_t index = 0; ok && index < sites.size(); ++index) {
        ok = hooks::InstallRel32CallHook(
            g_image + sites[index],
            CallBytes(sites[index], targets[index]),
            replacements[index], g_move_hooks[index], error);
    }

    if (ok) {
        ok = hooks::InstallPointerHook(
            reinterpret_cast<void**>(
                g_image + contract.button_handler_vtable_entry_rva),
            g_image + contract.button_handler_update_rva,
            reinterpret_cast<void*>(&HookedUpdate),
            g_update_hook, error);
    }
    if (!ok) {
        std::string rollback;
        if (!RemoveHookSet(rollback) && !rollback.empty()) {
            error += "; rollback failed: " + rollback;
        }
        return false;
    }

    g_installed.store(true, std::memory_order_release);
    return true;
}

bool GameplayBridgeInstalled() noexcept {
    bool queries_installed = true;
    for (const auto& hook : g_query_hooks)
        queries_installed = queries_installed && hook.installed();
    bool pointers_installed = true;
    for (const auto& hook : g_pointer_hooks)
        pointers_installed = pointers_installed && hook.installed();
    return g_installed.load(std::memory_order_acquire) && queries_installed &&
        pointers_installed && g_inventory_double_query_hook.installed() &&
        g_update_hook.installed() && g_character_update_hook.installed() &&
        g_move_hooks[0].installed() &&
        g_move_hooks[1].installed() && g_physical_move_hook.installed();
}

bool RemoveGameplayBridge(std::string& error) noexcept {
    error.clear();
    g_installed.store(false, std::memory_order_release);
    ConnectGameplayInput(nullptr);
    InvalidatePendingLocomotion();
    const bool ok = RemoveHookSet(error);
    const auto deadline = GetTickCount64() + 1000;
    while (g_active_callbacks.load(std::memory_order_acquire) != 0 &&
           GetTickCount64() < deadline) {
        Sleep(1);
    }
    if (g_active_callbacks.load(std::memory_order_acquire) != 0) {
        if (!error.empty()) error += "; ";
        error += "Requiem gameplay callbacks are still active";
        return false;
    }
    // Keep process-resident targets published for callbacks that may already
    // have crossed a restored callsite. A later install must reuse the same
    // exact image base.
    return ok;
}

void ConnectGameplayInput(runtime::OpenVrSession* session) noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    g_session = session;
    ReleaseSRWLockExclusive(&g_session_lock);
    if (session == nullptr) {
        g_turn = {};
        g_native_ui_active.store(false, std::memory_order_release);
        g_native_ui_surface.store(NativeUiSurface::none,
            std::memory_order_release);
        AcquireSRWLockExclusive(&g_frame_lock);
        g_frame = {};
        ReleaseSRWLockExclusive(&g_frame_lock);
        InvalidatePendingLocomotion();
        g_crouch_reset_requested.store(true, std::memory_order_release);
        AcquireSRWLockExclusive(&g_tracking_state_lock);
        g_tracking_state = {};
        ++g_tracking_body_generation;
        g_tracked_player_body.store(nullptr, std::memory_order_release);
        ReleaseSRWLockExclusive(&g_tracking_state_lock);
        AcquireSRWLockExclusive(&g_head_tracking_lock);
        g_head_tracking = {};
        ReleaseSRWLockExclusive(&g_head_tracking_lock);
    }
}

bool NativeUiActive() noexcept {
    return g_native_ui_active.load(std::memory_order_acquire);
}

NativeUiSurface CurrentNativeUiSurface() noexcept {
    return g_native_ui_surface.load(std::memory_order_acquire);
}

runtime::VrControllerFrame ReadNativeControllerFrame() noexcept {
    AcquireSRWLockShared(&g_frame_lock);
    const auto frame = g_frame;
    ReleaseSRWLockShared(&g_frame_lock);
    return frame;
}

void PublishGameplayHeadTracking(const runtime::VrHmdPose& pose,
    float world_yaw) noexcept {
    if (!pose.pose_valid || !pose.device_connected ||
        !std::isfinite(world_yaw)) return;
    AcquireSRWLockExclusive(&g_head_tracking_lock);
    g_head_tracking = {pose, world_yaw, GetTickCount64(), true};
    ReleaseSRWLockExclusive(&g_head_tracking_lock);
}

RequiemBodyTrackingSample ReadGameplayTrackingSample() noexcept {
    AcquireSRWLockShared(&g_tracking_state_lock);
    const auto sample = g_tracking_state;
    ReleaseSRWLockShared(&g_tracking_state_lock);
    return sample;
}

} // namespace penumbra_vr::backends::requiem
