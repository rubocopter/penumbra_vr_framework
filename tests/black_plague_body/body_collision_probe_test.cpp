// Exercise the exact-build observation adapter against a synthetic image.
// No game process or VR runtime is loaded.
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

    const auto saved_replacement = g_physical_request_hook.replacement_instruction;
    ++g_physical_request_hook.replacement_instruction[4];
    if (QueuePhysicalBodyDisplacement({0.01F, 0.0F, 0.0F})) return 46;
    g_physical_request_hook.replacement_instruction = saved_replacement;
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

    // Physical validation is a separate, default-off mode. It implies shadow
    // planning, queues tick N's plan for tick N+1, and only reconciles a
    // request that the exact physical gateway proves was injected.
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
    auto validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    auto queued_physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (!validation.pending || validation.queued_plans != 1 ||
        validation.latest_result != BlackPlaguePhysicalValidationResult::queued ||
        !Near(validation.requested_displacement[0], 0.03F) ||
        !queued_physical.pending || queued_physical.queued_requests != 1)
        return 54;

    // Add unrelated native movement before the injection. The physical
    // acceptance baseline must start after that native movement, so the
    // reconciler sees 0.03 m rather than the whole 0.23 m tick delta.
    g_stationary_native_update = false;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    const auto matched_sample = ConsumeBodyCollisionTelemetry();
    const auto matched_shadow = ConsumeBlackPlagueShadowTelemetry();
    validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    queued_physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (!matched_sample.physical_request_injected ||
        !Near(matched_sample.accepted_displacement[0], 0.23F) ||
        !Near(matched_sample.physical_position_before_injection[0],
            start.x + 0.20F) ||
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

    // Recenter invalidates a plan queued for the next tick before it can be
    // mistaken for evidence from a different tracking generation.
    g_stationary_native_update = true;
    head.values[3] = 0.06F;
    PublishBlackPlagueShadowTracking(head, 0.0F, false);
    HookedCharacterUpdate(replacement.data(), nullptr, 0.016F);
    validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    if (!validation.pending || validation.queued_plans != 1) return 56;
    PublishBlackPlagueShadowTracking(head, 0.0F, true);
    validation = ConsumeBlackPlaguePhysicalValidationTelemetry();
    queued_physical = ConsumePhysicalBodyDisplacementTelemetry();
    if (validation.pending || validation.invalidated_plans != 1 ||
        validation.latest_result !=
            BlackPlaguePhysicalValidationResult::invalidated ||
        queued_physical.pending) return 57;

    if (!RemoveBlackPlagueBodyAdapter(error)) {
        CloseHandle(physical_mutex);
        return 58;
    }
    CloseHandle(physical_mutex);
    if (ReadBlackPlaguePhysicalValidationStatus().enabled) return 59;
    g_stationary_native_update = false;
    g_collision_x_adjustment = -0.15F;

    if (!RemoveBodyCollisionProbe(error)) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 4;
    }
    if (!std::equal(kPhysicalRequestWindow.begin(),
            kPhysicalRequestWindow.end(),
            image + kPhysicalRequestInjection)) return 48;

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
