#include "body_collision_probe.hpp"

#include "black_plague_body_adapter.hpp"

#include "native_input_bridge.hpp"
#include "rel32_call_hook.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct Matrix {
    std::array<float, 16> values{};
};

using CharacterUpdate = void(__thiscall*)(void*, float);
using CheckShapeWorldCollision = bool(__thiscall*)(
    void*, Vec3*, void*, const Matrix*, void*, bool, bool, void*, bool, bool);

constexpr std::uintptr_t kPlayerCharacterBodyOffset = 0x274;
constexpr std::uintptr_t kPlayerGroundField268Offset = 0x268;
constexpr std::uintptr_t kPlayerGroundField26cOffset = 0x26C;
constexpr std::uintptr_t kPlayerJumpButtonDownOffset = 0x1FC;
constexpr std::uintptr_t kPlayerJumpCountOffset = 0x200;
constexpr std::uintptr_t kPlayerMaxJumpCountOffset = 0x204;
constexpr std::uintptr_t kPlayerMoveStateIndexOffset = 0x2D0;
constexpr std::uintptr_t kPlayerMoveStateVectorOffset = 0x2D8;
constexpr std::uintptr_t kCharacterPositionOffset = 0x48;
constexpr std::uintptr_t kCharacterSizeOffset = 0xC4;
constexpr std::uintptr_t kCharacterPhysicsBodyOffset = 0x23C;
constexpr std::uintptr_t kCharacterPhysicsWorldOffset = 0x240;
constexpr std::uintptr_t kPhysicsWorldCharacterUpdateCall = 0xD460A;
constexpr std::uintptr_t kCharacterUpdate = 0xD6E00;
constexpr std::uintptr_t kCharacterCollisionCall = 0xD7312;
constexpr std::uintptr_t kCheckShapeWorldCollision = 0xD4830;

constexpr std::array<std::uint8_t, 5> kUpdateCall{
    0xE8, 0xF1, 0x27, 0x00, 0x00};
constexpr std::array<std::uint8_t, 5> kCollisionCall{
    0xE8, 0x19, 0xD5, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 24> kUpdateSignature{
    0x81, 0xEC, 0xD4, 0x05, 0x00, 0x00, 0x53, 0x55,
    0x56, 0x8B, 0xF1, 0x8A, 0x46, 0x30, 0x33, 0xDB,
    0x84, 0xC0, 0x57, 0x0F, 0x84, 0x95, 0x12, 0x00};
constexpr std::array<std::uint8_t, 24> kCollisionSignature{
    0x81, 0xEC, 0xD0, 0x02, 0x00, 0x00, 0x53, 0x55,
    0x33, 0xDB, 0x56, 0x57, 0x8B, 0xF9, 0x89, 0x5C,
    0x24, 0x34, 0x89, 0x5C, 0x24, 0x38, 0x89, 0x5C};

std::uint8_t* g_image = nullptr;
hooks::Rel32CallHook g_update_hook;
hooks::Rel32CallHook g_collision_hook;
CharacterUpdate g_original_update = nullptr;
CheckShapeWorldCollision g_original_collision = nullptr;
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
BodyCollisionTelemetry g_telemetry;
BodyJumpBurstTelemetry g_jump_burst;
std::atomic<std::uint32_t> g_jump_burst_remaining{0};
std::atomic<std::uint64_t> g_body_update_sequence{0};

struct TickContext {
    void* character_body = nullptr;
    Vec3 position_before{};
    Vec3 requested_position{};
    Vec3 collision_position{};
    bool collision_sample_valid = false;
};
thread_local TickContext g_tick;

bool ReadBytes(const void* source, void* destination, std::size_t size) noexcept {
    if (source == nullptr || destination == nullptr) {
        return false;
    }
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<class T>
T Read(const void* object, std::uintptr_t offset) noexcept {
    T value{};
    if (object != nullptr) {
        static_cast<void>(ReadBytes(
            static_cast<const std::uint8_t*>(object) + offset,
            &value,
            sizeof(value)));
    }
    return value;
}

[[nodiscard]] bool Finite(const Vec3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
        std::isfinite(value.z);
}

[[nodiscard]] Vec3 Subtract(const Vec3& value, const Vec3& origin) noexcept {
    return {value.x - origin.x, value.y - origin.y, value.z - origin.z};
}

[[nodiscard]] std::array<float, 3> ToArray(const Vec3& value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] Vec3 FeetPosition(const Vec3& body, const Vec3& size) noexcept {
    return {body.x, body.y - size.y * 0.5F, body.z};
}

[[nodiscard]] bool IsCurrentPlayerBody(void* character_body, void*& player) noexcept {
    player = NativePlayerPointer();
    return player != nullptr &&
        Read<void*>(player, kPlayerCharacterBodyOffset) == character_body;
}

bool __fastcall HookedCheckShapeWorldCollision(
    void* world,
    void*,
    Vec3* resolved_position,
    void* shape,
    const Matrix* requested_transform,
    void* skip_body,
    bool skip_static,
    bool is_character,
    void* callback,
    bool collide_character,
    bool debug) {
    Vec3 requested{};
    Matrix requested_matrix{};
    bool observe = false;
    if (g_tick.character_body != nullptr &&
        ReadBytes(requested_transform, &requested_matrix,
            sizeof(requested_matrix))) {
        requested = {
            requested_matrix.values[3],
            requested_matrix.values[7],
            requested_matrix.values[11]};
        observe = Finite(requested);
    }

    const bool collided = g_original_collision(
        world,
        resolved_position,
        shape,
        requested_transform,
        skip_body,
        skip_static,
        is_character,
        callback,
        collide_character,
        debug);

    Vec3 resolved{};
    if (observe && ReadBytes(resolved_position, &resolved, sizeof(resolved)) &&
        Finite(resolved)) {
        g_tick.requested_position = requested;
        g_tick.collision_position = resolved;
        g_tick.collision_sample_valid = true;
    }
    return collided;
}

void __fastcall HookedCharacterUpdate(void* character_body, void*, float delta_seconds) {
    void* player = nullptr;
    const Vec3 position_before = Read<Vec3>(
        character_body, kCharacterPositionOffset);
    const Vec3 size = Read<Vec3>(character_body, kCharacterSizeOffset);
    const bool observe = IsCurrentPlayerBody(character_body, player) &&
        Finite(position_before) && Finite(size) && size.x > 0.0F &&
        size.y > 0.0F && size.z > 0.0F && std::isfinite(delta_seconds);

    const TickContext previous = g_tick;
    if (observe) {
        g_tick = {character_body, position_before, {}, {}, false};
    }
    g_original_update(character_body, delta_seconds);

    if (observe) {
        const Vec3 position_after = Read<Vec3>(
            character_body, kCharacterPositionOffset);
        if (Finite(position_after)) {
            BodyCollisionTelemetry sample{};
            sample.character_updates = 1;
            sample.horizontal_collision_requests =
                g_tick.collision_sample_valid ? 1 : 0;
            sample.valid = true;
            sample.collision_sample_valid = g_tick.collision_sample_valid;
            sample.player = reinterpret_cast<std::uintptr_t>(player);
            sample.character_body =
                reinterpret_cast<std::uintptr_t>(character_body);
            sample.physics_body = reinterpret_cast<std::uintptr_t>(
                Read<void*>(character_body, kCharacterPhysicsBodyOffset));
            sample.physics_world = reinterpret_cast<std::uintptr_t>(
                Read<void*>(character_body, kCharacterPhysicsWorldOffset));
            sample.delta_seconds = delta_seconds;
            sample.character_size = ToArray(size);
            sample.shape_radius = std::max(size.x, size.z) * 0.5F;
            sample.native_shape_is_cylinder =
                std::abs(sample.shape_radius * 2.0F - size.y) >= 0.01F;
            sample.body_position_before = ToArray(position_before);
            sample.body_position_after = ToArray(position_after);
            sample.feet_position_before = ToArray(
                FeetPosition(position_before, size));
            sample.feet_position_after = ToArray(
                FeetPosition(position_after, size));
            sample.accepted_displacement = ToArray(
                Subtract(position_after, position_before));
            if (g_tick.collision_sample_valid) {
                sample.requested_displacement = ToArray(
                    Subtract(g_tick.requested_position, position_before));
                sample.collision_resolved_displacement = ToArray(
                    Subtract(g_tick.collision_position, position_before));
            }

            ObserveBlackPlagueNativeBodyTick(
                player, character_body, sample.body_position_before,
                sample.body_position_after, sample.feet_position_after);

            AcquireSRWLockExclusive(&g_telemetry_lock);
            sample.character_updates += g_telemetry.character_updates;
            sample.horizontal_collision_requests +=
                g_telemetry.horizontal_collision_requests;
            g_telemetry = sample;
            const auto sequence = g_body_update_sequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            auto remaining = g_jump_burst_remaining.load(
                std::memory_order_relaxed);
            if (remaining != 0 &&
                g_jump_burst_remaining.compare_exchange_strong(
                    remaining, remaining - 1, std::memory_order_relaxed) &&
                g_jump_burst.count < BodyJumpBurstTelemetry::kCapacity) {
                auto& burst = g_jump_burst.samples[g_jump_burst.count++];
                burst.body_update_sequence = sequence;
                burst.body = sample;
                burst.player_268 = Read<std::int32_t>(
                    player, kPlayerGroundField268Offset);
                burst.player_26c = Read<std::uint8_t>(
                    player, kPlayerGroundField26cOffset);
                burst.jump_button_down_1fc = Read<std::uint8_t>(
                    player, kPlayerJumpButtonDownOffset);
                burst.jump_count_200 = Read<float>(player, kPlayerJumpCountOffset);
                burst.max_jump_count_204 = Read<float>(
                    player, kPlayerMaxJumpCountOffset);
                burst.move_state_index_2d0 = Read<std::int32_t>(
                    player, kPlayerMoveStateIndexOffset);
                void* const states = Read<void*>(player,
                    kPlayerMoveStateVectorOffset);
                if (states != nullptr && burst.move_state_index_2d0 >= 0 &&
                    burst.move_state_index_2d0 < 16) {
                    burst.move_state = reinterpret_cast<std::uintptr_t>(
                        Read<void*>(states, static_cast<std::uintptr_t>(
                            burst.move_state_index_2d0) * sizeof(void*)));
                }
            }
            ReleaseSRWLockExclusive(&g_telemetry_lock);
        }
    }
    g_tick = previous;
}

template<std::size_t Size>
[[nodiscard]] bool Matches(
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& expected) noexcept {
    std::array<std::uint8_t, Size> actual{};
    return ReadBytes(g_image + rva, actual.data(), actual.size()) &&
        actual == expected;
}

[[nodiscard]] std::uintptr_t DecodeCallTarget(
    const std::array<std::uint8_t, 5>& bytes, std::uintptr_t call) noexcept {
    if (bytes[0] != 0xE8) return 0;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
    return call + bytes.size() + displacement;
}

[[nodiscard]] bool InstallForImage(
    std::uint8_t* image,
    std::string& error) noexcept {
    error.clear();
    if (g_update_hook.installed() || g_collision_hook.installed()) {
        return true;
    }
    if (image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }
    g_image = image;
    if (!Matches(kCharacterUpdate + 0x15, kUpdateSignature) ||
        !Matches(kCheckShapeWorldCollision + 0x15, kCollisionSignature) ||
        !Matches(kPhysicsWorldCharacterUpdateCall, kUpdateCall) ||
        !Matches(kCharacterCollisionCall, kCollisionCall)) {
        error = "Body/collision boundary does not match the exact initialized build";
        g_image = nullptr;
        return false;
    }

    // Publish the verified native targets before either live callsite can
    // dispatch to its wrapper on another game thread.
    g_original_update = reinterpret_cast<CharacterUpdate>(
        g_image + kCharacterUpdate);
    g_original_collision = reinterpret_cast<CheckShapeWorldCollision>(
        g_image + kCheckShapeWorldCollision);
    if (!hooks::InstallRel32CallHook(
            g_image + kPhysicsWorldCharacterUpdateCall,
            kUpdateCall,
            reinterpret_cast<void*>(&HookedCharacterUpdate),
            g_update_hook,
            error)) {
        g_original_update = nullptr;
        g_original_collision = nullptr;
        g_image = nullptr;
        return false;
    }
    if (!hooks::InstallRel32CallHook(
            g_image + kCharacterCollisionCall,
            kCollisionCall,
            reinterpret_cast<void*>(&HookedCheckShapeWorldCollision),
            g_collision_hook,
            error)) {
        std::string rollback;
        static_cast<void>(hooks::RemoveRel32CallHook(g_update_hook, rollback));
        g_original_update = nullptr;
        g_original_collision = nullptr;
        g_image = nullptr;
        if (!rollback.empty()) {
            error += "; rollback failed: " + rollback;
        }
        return false;
    }
    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    g_jump_burst = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_jump_burst_remaining.store(0, std::memory_order_relaxed);
    g_body_update_sequence.store(0, std::memory_order_relaxed);
    return true;
}

} // namespace

bool InstallBodyCollisionProbe(std::string& error) noexcept {
    return InstallForImage(
        reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr)), error);
}

bool RemoveBodyCollisionProbe(std::string& error) noexcept {
    error.clear();
    bool success = hooks::RemoveRel32CallHook(g_update_hook, error);
    std::string collision_error;
    if (!hooks::RemoveRel32CallHook(g_collision_hook, collision_error)) {
        success = false;
        if (!error.empty()) {
            error += "; ";
        }
        error += collision_error;
    }
    if (success) {
        g_original_update = nullptr;
        g_original_collision = nullptr;
        g_image = nullptr;
    }
    return success;
}

BodyCollisionTelemetry ConsumeBodyCollisionTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    const BodyCollisionTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    return result;
}

void RequestBodyJumpBurst() noexcept {
    g_jump_burst_remaining.store(
        static_cast<std::uint32_t>(BodyJumpBurstTelemetry::kCapacity),
        std::memory_order_relaxed);
}

bool IsBodyJumpBurstComplete() noexcept {
    if (g_jump_burst_remaining.load(std::memory_order_relaxed) != 0) {
        return false;
    }
    AcquireSRWLockShared(&g_telemetry_lock);
    const bool complete = g_jump_burst.count != 0;
    ReleaseSRWLockShared(&g_telemetry_lock);
    return complete;
}

BodyJumpBurstTelemetry ConsumeBodyJumpBurstTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    const BodyJumpBurstTelemetry result = g_jump_burst;
    g_jump_burst = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    return result;
}

NativeBodyUpdateBoundaryStatus ReadNativeBodyUpdateBoundaryStatus() noexcept {
    NativeBodyUpdateBoundaryStatus result;
    result.expected = kUpdateCall;
    result.expected_target = kCharacterUpdate;
    result.owner_installed = g_update_hook.installed();
    if (g_image != nullptr) {
        static_cast<void>(ReadBytes(g_image + kPhysicsWorldCharacterUpdateCall,
            result.live.data(), result.live.size()));
    }
    result.live_target = DecodeCallTarget(result.live,
        kPhysicsWorldCharacterUpdateCall);
    result.owner_matches_live = result.owner_installed &&
        result.live == g_update_hook.replacement_instruction;
    result.initialized = result.owner_matches_live;
    return result;
}

} // namespace penumbra_vr::backends::black_plague
