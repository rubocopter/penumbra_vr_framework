// Exercise the exact-build observation adapter against a synthetic image.
// No game process or VR runtime is loaded.
#include "black_plague_body_adapter.hpp"
#include "../../src/backends/black_plague/body_collision_probe.cpp"

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>

namespace penumbra_vr::backends::black_plague {
extern bool g_test_movement_owner_ready;
namespace {

std::array<std::uint8_t, 0x300> g_player_storage{};
std::array<std::uint8_t, 0x300> g_body_storage{};
std::array<void*, 16> g_move_states{};
void* g_test_player = g_player_storage.data();
unsigned int g_native_update_calls = 0;
bool g_stationary_native_update = false;
float g_collision_x_adjustment = -0.15F;

template<class T>
void Put(void* object, std::size_t offset, const T& value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}

void Jump(std::uint8_t* image, std::uintptr_t rva, void* target) {
    auto* instruction = image + rva;
    instruction[0] = 0xE9;
    const auto displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::intptr_t>(target) -
        reinterpret_cast<std::intptr_t>(instruction + 5));
    std::memcpy(instruction + 1, &displacement, sizeof(displacement));
}

bool __fastcall FakeCollision(
    void*, void*, Vec3* resolved, void*, const Matrix* requested,
    void*, bool, bool, void*, bool, bool) {
    *resolved = {
        requested->values[3] + g_collision_x_adjustment,
        requested->values[7],
        requested->values[11]};
    return true;
}

void __fastcall FakeUpdate(void* body, void*, float) {
    ++g_native_update_calls;
    const Vec3 before = Read<Vec3>(body, kCharacterPositionOffset);
    Vec3 native_position = before;
    if (!g_stationary_native_update) {
        native_position.x += 0.20F;
        native_position.z += 0.10F;
    }
    Put(body, kCharacterPositionOffset, native_position);
    auto* const current_position = reinterpret_cast<Vec3*>(
        static_cast<std::uint8_t*>(body) + kCharacterPositionOffset);
    ApplyQueuedPhysicalDisplacement(body, current_position);
    const Vec3 requested_position = Read<Vec3>(body, kCharacterPositionOffset);
    if (std::abs(requested_position.x - before.x) < 0.00001F &&
        std::abs(requested_position.z - before.z) < 0.00001F) {
        return;
    }
    Matrix requested{};
    requested.values[0] = requested.values[5] =
        requested.values[10] = requested.values[15] = 1.0F;
    requested.values[3] = requested_position.x;
    requested.values[7] = requested_position.y;
    requested.values[11] = requested_position.z;
    Vec3 resolved{};
    static_cast<void>(HookedCheckShapeWorldCollision(
        reinterpret_cast<void*>(0x2222), nullptr, &resolved,
        reinterpret_cast<void*>(0x3333), &requested,
        reinterpret_cast<void*>(0x4444), false, true,
        reinterpret_cast<void*>(0x5555), true, false));
    // A later native phase adds gravity after the horizontal collision result.
    resolved.y -= 0.02F;
    Put(body, kCharacterPositionOffset, resolved);
}

[[nodiscard]] bool Near(float left, float right) {
    return std::abs(left - right) < 0.00001F;
}

} // namespace

void* NativePlayerPointer() noexcept {
    return g_test_player;
}

std::uint64_t NativePlayerGeneration() noexcept {
    return 1;
}

void ServiceSpatialHandNudge(void*) noexcept {}

int RunBodyCollisionProbeTest() {
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x300000, MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    if (image == nullptr) {
        return 1;
    }
    std::memcpy(image + kCharacterUpdate + 0x15,
        kUpdateSignature.data(), kUpdateSignature.size());
    std::memcpy(image + kCheckShapeWorldCollision + 0x15,
        kCollisionSignature.data(), kCollisionSignature.size());
    std::memcpy(image + kPhysicsWorldCharacterUpdateCall,
        kUpdateCall.data(), kUpdateCall.size());
    std::memcpy(image + kCharacterCollisionCall,
        kCollisionCall.data(), kCollisionCall.size());
    std::memcpy(image + kPhysicalRequestInjection,
        kPhysicalRequestWindow.data(), kPhysicalRequestWindow.size());
    std::memcpy(image + kPhysicalStepDecision,
        kPhysicalStepWindow.data(), kPhysicalStepWindow.size());
    Jump(image, kCharacterUpdate, reinterpret_cast<void*>(&FakeUpdate));
    Jump(image, kCheckShapeWorldCollision,
        reinterpret_cast<void*>(&FakeCollision));

    std::string error;
    if (!InstallForImage(image, error)) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 2;
    }
    g_test_movement_owner_ready = false;
    if (InstallBlackPlagueBodyAdapter(error)) return 20;
    g_test_movement_owner_ready = true;
    SetEnvironmentVariableA("PVR_BP_RECONCILIATION_SHADOW", "1");
    if (!InstallBlackPlagueBodyAdapter(error)) return 21;
    runtime::VrMatrix34 head{{1,0,0,0, 0,1,0,1.7F, 0,0,1,0}};
    PublishBlackPlagueShadowTracking(head, 0.0F, true);

    const Vec3 start{10.0F, 2.0F, -3.0F};
    const Vec3 size{0.8F, 1.8F, 0.6F};
    Put(g_body_storage.data(), kCharacterPositionOffset, start);
    Put(g_body_storage.data(), kCharacterSizeOffset, size);
    Put(g_body_storage.data(), kCharacterPhysicsBodyOffset,
        reinterpret_cast<void*>(0x1234));
    Put(g_body_storage.data(), kCharacterPhysicsWorldOffset,
        reinterpret_cast<void*>(0x5678));
    Put(g_player_storage.data(), kPlayerCharacterBodyOffset,
        static_cast<void*>(g_body_storage.data()));
    Put(g_player_storage.data(), kPlayerGroundField268Offset,
        static_cast<std::int32_t>(7));
    Put(g_player_storage.data(), kPlayerGroundField26cOffset,
        static_cast<std::uint8_t>(1));
    Put(g_player_storage.data(), kPlayerJumpButtonDownOffset,
        static_cast<std::uint8_t>(1));
    Put(g_player_storage.data(), kPlayerJumpCountOffset, 0.25F);
    Put(g_player_storage.data(), kPlayerMaxJumpCountOffset, 0.50F);
    Put(g_player_storage.data(), kPlayerMoveStateIndexOffset,
        static_cast<std::int32_t>(3));
    g_move_states[3] = reinterpret_cast<void*>(0x9876);
    Put(g_player_storage.data(), kPlayerMoveStateVectorOffset,
        static_cast<void*>(g_move_states.data()));

    const auto physical_boundary = ReadPhysicalBodyDisplacementBoundaryStatus();
    if (!physical_boundary.initialized || physical_boundary.live[0] != 0xE9)
        return 38;

    // A physical-HMD-only tick bypasses Black Plague's native step-climb
    // search after horizontal collision. Stick locomotion keeps that path so
    // ordinary stair/ledge traversal remains native.
    g_tick = {};
    g_tick.character_body = g_body_storage.data();
    g_tick.physical_request_injected = true;
    if (!ShouldSuppressPhysicalStepClimb() ||
        !g_tick.physical_step_climb_suppressed) return 87;
    g_tick.physical_step_climb_suppressed = false;
    g_tick.locomotion_request_injected = true;
    if (ShouldSuppressPhysicalStepClimb() ||
        g_tick.physical_step_climb_suppressed) return 88;
    g_tick.locomotion_request_injected = false;
    g_tick.position_before = {};
    g_tick.physical_position_before = {0.01F, 0.0F, 0.0F};
    if (ShouldSuppressPhysicalStepClimb() ||
        g_tick.physical_step_climb_suppressed) return 90;
    g_tick = {};

    if (QueuePhysicalBodyDisplacement(
            {NAN, 123.0F, 0.0F})) return 39;
    auto physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (physical.rejected_requests != 1 || physical.pending ||
        physical.latest_result !=
            PhysicalBodyDisplacementResult::invalid_request) return 40;

    if (!QueuePhysicalBodyDisplacement({0.10F, 123.0F, 0.10F})) return 41;
    physical = ConsumePhysicalBodyDisplacementTelemetry();
    const float bounded_axis =
        runtime::vr_locomotion_policy::kMaximumPhysicalBodyStep /
        std::sqrt(2.0F);
    if (!physical.pending || physical.queued_requests != 1 ||
        !Near(physical.requested_displacement[1], 0.0F) ||
        !Near(physical.bounded_displacement[0], bounded_axis) ||
        !Near(physical.bounded_displacement[1], 0.0F) ||
        !Near(physical.bounded_displacement[2], bounded_axis)) return 42;

    Vec3 injected_position = start;
    g_tick.character_body = g_body_storage.data();
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    const Vec3 injected_once = injected_position;
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    g_tick = {};
    physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (physical.pending || physical.consumed_requests != 1 ||
        physical.injected_requests != 1 || physical.rejected_requests != 0 ||
        physical.latest_result != PhysicalBodyDisplacementResult::injected ||
        !Near(injected_once.x, start.x + bounded_axis) ||
        !Near(injected_once.y, start.y) ||
        !Near(injected_once.z, start.z + bounded_axis) ||
        !Near(injected_position.x, injected_once.x) ||
        !Near(injected_position.z, injected_once.z)) return 43;

    // Rework locomotion shares the one exact-build injection point. The
    // physical component keeps priority and the combined request never
    // exceeds the proven 0.05 m step.
    if (!QueuePhysicalBodyDisplacement({0.02F, 0.0F, 0.0F}) ||
        !QueueLocomotionBodyDisplacement({0.04F, 0.0F, 0.0F})) return 71;
    g_tick = {};
    g_tick.character_body = g_body_storage.data();
    injected_position = start;
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    if (!g_tick.physical_request_injected ||
        !g_tick.locomotion_request_injected ||
        !Near(g_tick.physical_injected.x, 0.02F) ||
        !Near(g_tick.locomotion_injected.x, 0.03F) ||
        !Near(g_tick.combined_injected.x, 0.05F) ||
        !Near(injected_position.x, start.x + 0.05F)) return 72;
    const Vec3 accepted_partition = PhysicalAcceptedFromCombined(
        g_tick.physical_injected, g_tick.locomotion_injected,
        {0.035F, 0.0F, 0.0F});
    if (!Near(accepted_partition.x, 0.02F)) return 73;
    g_tick = {};
    static_cast<void>(ConsumePhysicalBodyDisplacementTelemetry());

    // With opposite free-space requests the combined body delta can be zero.
    // Rework still accepts the physical phase before locomotion moves the body
    // back, so the partition must retain the physical component.
    const Vec3 opposite_partition = PhysicalAcceptedFromCombined(
        {0.02F, 0.0F, 0.0F}, {-0.02F, 0.0F, 0.0F}, {});
    if (!Near(opposite_partition.x, 0.02F)) return 78;

    // Queue order is not an ownership contract. A physical plan published
    // after input must retain and re-bound the already queued locomotion.
    if (!QueueLocomotionBodyDisplacement({0.04F, 0.0F, 0.0F}) ||
        !QueuePhysicalBodyDisplacement({0.02F, 0.0F, 0.0F})) return 79;
    g_tick.character_body = g_body_storage.data();
    injected_position = start;
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    if (!g_tick.physical_request_injected ||
        !g_tick.locomotion_request_injected ||
        !Near(g_tick.physical_injected.x, 0.02F) ||
        !Near(g_tick.locomotion_injected.x, 0.03F) ||
        !Near(g_tick.combined_injected.x, 0.05F)) return 80;
    g_tick = {};
    static_cast<void>(ConsumePhysicalBodyDisplacementTelemetry());

    if (!QueueLocomotionBodyDisplacement({0.0F, 0.0F, -0.025F})) return 74;
    g_tick.character_body = g_body_storage.data();
    injected_position = start;
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    if (g_tick.physical_request_injected ||
        !g_tick.locomotion_request_injected ||
        !Near(g_tick.combined_injected.z, -0.025F) ||
        !Near(injected_position.z, start.z - 0.025F)) return 75;
    g_tick = {};

    // cButtonHandler can publish more than once before the single native body
    // tick. The pending metric displacement is a latest-state request, not an
    // accumulator, so duplicate input callbacks must not double locomotion.
    if (!QueueLocomotionBodyDisplacement({0.0F, 0.0F, 0.025F}) ||
        !QueueLocomotionBodyDisplacement({0.0F, 0.0F, 0.025F})) return 81;
    g_tick.character_body = g_body_storage.data();
    injected_position = start;
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    if (!g_tick.locomotion_request_injected ||
        !Near(g_tick.locomotion_injected.z, 0.025F) ||
        !Near(g_tick.combined_injected.z, 0.025F) ||
        !Near(injected_position.z, start.z + 0.025F)) return 82;
    g_tick = {};

    if (!QueuePhysicalBodyDisplacement({0.01F, 0.0F, 0.0F})) return 44;
    std::array<std::uint8_t, 0x300> mismatch_body{};
    Put(g_player_storage.data(), kPlayerCharacterBodyOffset,
        static_cast<void*>(mismatch_body.data()));
    g_tick.character_body = g_body_storage.data();
    injected_position = start;
    ApplyQueuedPhysicalDisplacement(g_body_storage.data(), &injected_position);
    g_tick = {};
    physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (physical.pending || physical.consumed_requests != 1 ||
        physical.injected_requests != 0 || physical.rejected_requests != 1 ||
        physical.latest_result != PhysicalBodyDisplacementResult::body_mismatch ||
        !Near(injected_position.x, start.x)) return 45;
    Put(g_player_storage.data(), kPlayerCharacterBodyOffset,
        static_cast<void*>(g_body_storage.data()));

    std::array<std::uint8_t, 5> saved_replacement{};
    std::memcpy(saved_replacement.data(), image + kPhysicalRequestInjection,
        saved_replacement.size());
    ++image[kPhysicalRequestInjection + 4];
    if (QueuePhysicalBodyDisplacement({0.01F, 0.0F, 0.0F})) return 46;
    std::memcpy(image + kPhysicalRequestInjection, saved_replacement.data(),
        saved_replacement.size());
    physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (physical.rejected_requests != 1 ||
        physical.latest_result !=
            PhysicalBodyDisplacementResult::owner_mismatch) return 47;

    HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
    auto motion = ConsumeBlackPlagueBodyMotion();
    auto shadow = ConsumeBlackPlagueShadowTelemetry();
    if (g_native_update_calls != 1 || !motion.valid ||
        !shadow.latest.valid || !shadow.latest.reset ||
        !Near(motion.accepted.accepted_displacement[0], 0.05F)) return 22;
    const BodyCollisionTelemetry sample = ConsumeBodyCollisionTelemetry();
    if (!sample.valid || !sample.collision_sample_valid ||
        sample.character_updates != 1 ||
        sample.horizontal_collision_requests != 1 ||
        sample.character_body !=
            reinterpret_cast<std::uintptr_t>(g_body_storage.data()) ||
        sample.physics_body != 0x1234 || sample.physics_world != 0x5678 ||
        !Near(sample.character_size[1], 1.8F) ||
        !sample.native_shape_is_cylinder ||
        !Near(sample.shape_radius, 0.4F) ||
        !Near(sample.feet_position_before[1], 1.1F) ||
        !Near(sample.requested_displacement[0], 0.2F) ||
        !Near(sample.requested_displacement[2], 0.1F) ||
        !Near(sample.collision_resolved_displacement[0], 0.05F) ||
        !Near(sample.accepted_displacement[0], 0.05F) ||
        !Near(sample.accepted_displacement[1], -0.02F)) {
        static_cast<void>(RemoveBodyCollisionProbe(error));
        VirtualFree(image, 0, MEM_RELEASE);
        return 3;
    }

    g_stationary_native_update = true;
    g_collision_x_adjustment = 0.0F;
    if (!QueuePhysicalBodyDisplacement({0.03F, 99.0F, 0.04F})) return 49;
    HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
    const auto physical_sample = ConsumeBodyCollisionTelemetry();
    physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (!physical_sample.valid || !physical_sample.collision_sample_valid ||
        !physical_sample.physical_request_consumed ||
        !physical_sample.physical_request_injected ||
        !Near(physical_sample.physical_requested_displacement[0], 0.03F) ||
        !Near(physical_sample.physical_requested_displacement[1], 0.0F) ||
        !Near(physical_sample.physical_requested_displacement[2], 0.04F) ||
        !Near(physical_sample.requested_displacement[0], 0.03F) ||
        !Near(physical_sample.requested_displacement[2], 0.04F) ||
        !Near(physical_sample.accepted_displacement[0], 0.03F) ||
        !Near(physical_sample.accepted_displacement[2], 0.04F) ||
        physical.consumed_requests != 1 || physical.injected_requests != 1)
        return 50;
    g_stationary_native_update = false;
    g_collision_x_adjustment = -0.15F;

    RequestBodyJumpBurst();
    for (std::size_t index = 0;
        index < BodyJumpBurstTelemetry::kCapacity; ++index) {
        PublishBlackPlagueShadowTracking(head, 0.0F, false);
        HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
    }
    if (!IsBodyJumpBurstComplete()) {
        static_cast<void>(RemoveBodyCollisionProbe(error));
        VirtualFree(image, 0, MEM_RELEASE);
        return 6;
    }
    const BodyJumpBurstTelemetry burst = ConsumeBodyJumpBurstTelemetry();
    if (burst.count != BodyJumpBurstTelemetry::kCapacity ||
        burst.samples.front().body_update_sequence != 3 ||
        burst.samples.back().body_update_sequence !=
            BodyJumpBurstTelemetry::kCapacity + 2 ||
        burst.samples.front().player_268 != 7 ||
        burst.samples.front().player_26c != 1 ||
        burst.samples.front().jump_button_down_1fc != 1 ||
        !Near(burst.samples.front().jump_count_200, 0.25F) ||
        !Near(burst.samples.front().max_jump_count_204, 0.50F) ||
        burst.samples.front().move_state_index_2d0 != 3 ||
        burst.samples.front().move_state != 0x9876) {
        static_cast<void>(RemoveBodyCollisionProbe(error));
        VirtualFree(image, 0, MEM_RELEASE);
        return 7;
    }

    // Exercise the existing hook -> adapter -> shared shadow fan-out again
    // with physical tracking. It must still call the native update only once.
    head.values[3] = 0.03F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    const auto calls_before = g_native_update_calls;
    HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
    shadow = ConsumeBlackPlagueShadowTelemetry();
    if (g_native_update_calls != calls_before + 1 || !shadow.latest.valid ||
        !Near(shadow.latest.physical_delta[0], 0.03F) ||
        shadow.latest.physical_observation_available) return 23;

    std::array<std::uint8_t, 0x300> replacement = g_body_storage;
    Put(g_player_storage.data(), kPlayerCharacterBodyOffset,
        static_cast<void*>(replacement.data()));
    static_cast<void>(ConsumeBlackPlagueBodyMotion());
    HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
    if (ConsumeBlackPlagueBodyMotion().valid) return 24;
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    shadow = ConsumeBlackPlagueShadowTelemetry();
    if (!ConsumeBlackPlagueBodyMotion().valid || !shadow.latest.reset ||
        shadow.latest.plan.physical_request != std::array<float, 3>{}) return 25;
    g_test_movement_owner_ready = false;
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    if (ConsumeBlackPlagueShadowTelemetry().latest.valid) return 26;
    g_test_movement_owner_ready = true;
    InvalidateBlackPlagueShadowTracking();
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    if (ConsumeBlackPlagueShadowTelemetry().latest.valid) return 27;
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    if (!ConsumeBlackPlagueShadowTelemetry().latest.reset) return 28;
    if (!RemoveBlackPlagueBodyAdapter(error)) return 29;
    SetEnvironmentVariableA("PVR_BP_RECONCILIATION_SHADOW", nullptr);
    if (!InstallBlackPlagueBodyAdapter(error)) return 30;
    if (ReadBlackPlagueShadowStatus().enabled) return 33;
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    if (ConsumeBlackPlagueShadowTelemetry().observed_ticks != 0 ||
        g_native_update_calls != calls_before + 7) return 31;
    if (!RemoveBlackPlagueBodyAdapter(error)) return 32;
    HANDLE shadow_mutex = CreateMutexW(nullptr, FALSE,
        L"Local\\PenumbraVR.BlackPlague.ReconciliationShadow");
    if (shadow_mutex == nullptr) return 34;
    if (!InstallBlackPlagueBodyAdapter(error)) {
        CloseHandle(shadow_mutex);
        return 35;
    }
    const auto shadow_status = ReadBlackPlagueShadowStatus();
    if (!shadow_status.enabled || shadow_status.source !=
        BlackPlagueShadowRequestSource::mutex) {
        CloseHandle(shadow_mutex);
        return 36;
    }
    if (!RemoveBlackPlagueBodyAdapter(error)) {
        CloseHandle(shadow_mutex);
        return 37;
    }
    CloseHandle(shadow_mutex);

    // Physical validation is a separate, default-off mode. The body-update
    // owner prepares the request from B0 before the one native update, the
    // exact physical gateway consumes it in that same tick, and reconciliation
    // runs once from the resulting B1 observation.
    HANDLE physical_mutex = CreateMutexW(nullptr, FALSE,
        L"Local\\PenumbraVR.BlackPlague.PhysicalDisplacementValidation");
    if (physical_mutex == nullptr) return 51;
    if (!InstallBlackPlagueBodyAdapter(error)) {
        CloseHandle(physical_mutex);
        return 52;
    }
    const auto physical_status = ReadBlackPlaguePhysicalValidationStatus();
    const auto implied_shadow_status = ReadBlackPlagueShadowStatus();
    if (!physical_status.enabled || physical_status.source !=
            BlackPlaguePhysicalValidationRequestSource::mutex ||
        !implied_shadow_status.enabled || implied_shadow_status.source !=
            BlackPlagueShadowRequestSource::physical_validation) {
        CloseHandle(physical_mutex);
        return 53;
    }

    Put(replacement.data(), kCharacterPositionOffset, start);
    Put(g_player_storage.data(), kPlayerCharacterBodyOffset,
        static_cast<void*>(replacement.data()));
    g_stationary_native_update = true;
    g_collision_x_adjustment = 0.0F;
    head.values[3] = 0.0F;
    head.values[11] = 0.0F;
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    static_cast<void>(ConsumeBlackPlagueShadowTelemetry());
    static_cast<void>(ConsumeBlackPlaguePhysicalValidationTelemetry());
    static_cast<void>(ConsumePhysicalBodyDisplacementTelemetry());

    head.values[3] = 0.03F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    auto same_tick_sample = ConsumeBodyCollisionTelemetry();
    auto same_tick_shadow = ConsumeBlackPlagueShadowTelemetry();
    auto validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    auto queued_physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (!same_tick_sample.physical_request_injected ||
        !Near(same_tick_sample.physical_requested_displacement[0], 0.03F) ||
        !Near(same_tick_sample.physical_accepted_displacement[0], 0.03F) ||
        !same_tick_shadow.latest.physical_observation_available ||
        !same_tick_shadow.latest.physical_reconciliation.valid ||
        validation.pending || validation.queued_plans != 1 ||
        validation.matched_observations != 1 ||
        validation.latest_result != BlackPlaguePhysicalValidationResult::reconciled ||
        !Near(validation.requested_displacement[0], 0.03F) ||
        queued_physical.pending || queued_physical.queued_requests != 1 ||
        queued_physical.injected_requests != 1)
        return 54;

    // Add unrelated native movement before a fresh same-tick injection. The physical
    // acceptance baseline must start after that native movement, so the
    // reconciler sees 0.03 m rather than the whole 0.23 m tick delta.
    g_stationary_native_update = false;
    head.values[3] = 0.06F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    const auto matched_sample = ConsumeBodyCollisionTelemetry();
    const auto matched_shadow = ConsumeBlackPlagueShadowTelemetry();
    validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    queued_physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (!matched_sample.physical_request_injected ||
        !Near(matched_sample.accepted_displacement[0], 0.23F) ||
        !Near(matched_sample.physical_position_before_injection[0],
            start.x + 0.23F) ||
        !Near(matched_sample.physical_accepted_displacement[0], 0.03F) ||
        !matched_shadow.latest.physical_observation_available ||
        !Near(matched_shadow.latest.physical_motion.accepted_displacement[0],
            0.03F) ||
        !matched_shadow.latest.physical_reconciliation.valid ||
        !Near(matched_shadow.latest.physical_reconciliation.rejected_distance,
            0.0F) ||
        validation.pending || validation.matched_observations != 1 ||
        validation.latest_result !=
            BlackPlaguePhysicalValidationResult::reconciled ||
        queued_physical.injected_requests != 1) return 55;

    // Recenter happens before planning for the next body tick. No request from
    // the pre-recenter tracking epoch may survive into that tick.
    g_stationary_native_update = true;
    head.values[3] = 0.09F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    const auto recentered_body = ConsumeBodyCollisionTelemetry();
    const auto recentered_shadow = ConsumeBlackPlagueShadowTelemetry();
    validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    queued_physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (recentered_body.physical_request_injected ||
        !recentered_shadow.latest.reset || validation.pending ||
        validation.queued_plans != 0 || validation.matched_observations != 0 ||
        queued_physical.pending || queued_physical.injected_requests != 0)
        return 56;

    head.values[3] = 0.12F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    if (validation.pending || validation.queued_plans != 1 ||
        validation.matched_observations != 1 ||
        validation.latest_result != BlackPlaguePhysicalValidationResult::reconciled)
        return 57;

    if (!RemoveBlackPlagueBodyAdapter(error)) {
        CloseHandle(physical_mutex);
        return 58;
    }
    CloseHandle(physical_mutex);
    if (ReadBlackPlaguePhysicalValidationStatus().enabled) return 59;

    // Active room-scale remains a separate opt-in. It must fail closed unless
    // the already-proven physical request owner is requested at the same time,
    // then publish only a fresh reconciled horizontal camera offset.
    HANDLE room_scale_only_mutex = CreateMutexW(nullptr, FALSE,
        L"Local\\PenumbraVR.BlackPlague.RoomScaleValidation");
    if (room_scale_only_mutex == nullptr) return 60;
    if (!InstallBlackPlagueBodyAdapter(error)) {
        CloseHandle(room_scale_only_mutex);
        return 61;
    }
    if (ReadBlackPlagueRoomScaleStatus().enabled) return 62;
    if (!RemoveBlackPlagueBodyAdapter(error)) return 63;
    CloseHandle(room_scale_only_mutex);

    physical_mutex = CreateMutexW(nullptr, FALSE,
        L"Local\\PenumbraVR.BlackPlague.PhysicalDisplacementValidation");
    HANDLE room_scale_mutex = CreateMutexW(nullptr, FALSE,
        L"Local\\PenumbraVR.BlackPlague.RoomScaleValidation");
    if (physical_mutex == nullptr || room_scale_mutex == nullptr) return 64;
    if (!InstallBlackPlagueBodyAdapter(error)) return 65;
    const auto room_scale_status = ReadBlackPlagueRoomScaleStatus();
    if (!room_scale_status.enabled || room_scale_status.source !=
            BlackPlagueRoomScaleRequestSource::mutex) return 66;

    Put(replacement.data(), kCharacterPositionOffset, start);
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    auto room_scale_camera = ReadBlackPlagueRoomScaleCameraSample();
    if (!room_scale_camera.enabled || !room_scale_camera.valid ||
        !Near(room_scale_camera.horizontal_world_offset[0], 0.0F) ||
        !Near(room_scale_camera.horizontal_world_offset[1], 0.0F) ||
        !Near(room_scale_camera.horizontal_world_offset[2], 0.0F)) return 67;

    // Input publishes a logical latest-state intent. The body owner converts
    // it with the native physics dt, so 20 ms at Rework's 1.5 m/s produces a
    // 0.03 m step regardless of the input callback cadence.
    BlackPlagueDirectLocomotionIntent direct_intent;
    direct_intent.move = {0.0F, 1.0F};
    direct_intent.head_world_pose = runtime::IdentityMatrix().values;
    direct_intent.player_generation = NativePlayerGeneration();
    if (!PublishBlackPlagueDirectLocomotionIntent(
            g_test_player, direct_intent)) return 83;
    HookedCharacterUpdate(replacement.data(), nullptr, 0.020F);
    const auto direct_body = ConsumeBodyCollisionTelemetry();
    if (!direct_body.locomotion_request_injected ||
        !Near(direct_body.locomotion_requested_displacement[2], -0.03F) ||
        !Near(direct_body.locomotion_accepted_displacement[2], -0.03F))
        return 84;

    direct_intent.player_generation = NativePlayerGeneration() + 1;
    if (!PublishBlackPlagueDirectLocomotionIntent(
            g_test_player, direct_intent)) return 85;
    HookedCharacterUpdate(replacement.data(), nullptr, 0.020F);
    if (ConsumeBodyCollisionTelemetry().locomotion_request_injected) return 86;
    InvalidateBlackPlagueDirectLocomotionIntent();

    head.values[3] += 0.03F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    room_scale_camera = ReadBlackPlagueRoomScaleCameraSample();
    if (!room_scale_camera.valid ||
        !Near(room_scale_camera.horizontal_world_offset[0], 0.0F) ||
        !Near(room_scale_camera.horizontal_world_offset[1], 0.0F)) return 68;
    if (!QueueLocomotionBodyDisplacement({0.0F, 0.0F, 0.02F})) return 76;
    head.values[3] += 0.03F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    const auto combined_body = ConsumeBodyCollisionTelemetry();
    const auto combined_shadow = ConsumeBlackPlagueShadowTelemetry();
    if (!combined_body.physical_request_injected ||
        !combined_body.locomotion_request_injected ||
        !Near(combined_body.physical_requested_displacement[0], 0.03F) ||
        !Near(combined_body.locomotion_requested_displacement[2], 0.02F) ||
        !Near(combined_body.physical_accepted_displacement[0], 0.03F) ||
        !Near(combined_body.locomotion_accepted_displacement[2], 0.02F) ||
        !combined_shadow.latest.physical_reconciliation.valid ||
        !Near(combined_shadow.latest.locomotion_carry_displacement[2], 0.02F))
        return 77;
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    if (ReadBlackPlagueRoomScaleCameraSample().valid) return 69;
    if (!RemoveBlackPlagueBodyAdapter(error)) return 70;
    CloseHandle(room_scale_mutex);
    CloseHandle(physical_mutex);
    g_stationary_native_update = false;
    g_collision_x_adjustment = -0.15F;

    if (!RemoveBodyCollisionProbe(error)) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 4;
    }
    if (!std::equal(kPhysicalRequestWindow.begin(),
            kPhysicalRequestWindow.end(),
            image + kPhysicalRequestInjection)) return 48;
    if (!std::equal(kPhysicalStepWindow.begin(),
            kPhysicalStepWindow.end(),
            image + kPhysicalStepDecision)) return 89;

    image[kPhysicsWorldCharacterUpdateCall] = 0x90;
    if (InstallForImage(image, error) || error.empty()) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 5;
    }
    VirtualFree(image, 0, MEM_RELEASE);
    return 0;
}

} // namespace penumbra_vr::backends::black_plague

int main() {
    const int result =
        penumbra_vr::backends::black_plague::RunBodyCollisionProbeTest();
    if (result != 0) {
        std::cerr << "Body/collision probe regression: " << result << '\n';
    }
    return result;
}
