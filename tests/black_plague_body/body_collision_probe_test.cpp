// Exercise the exact-build observation adapter against a synthetic image.
// No game process or VR runtime is loaded.
#include "../../src/backends/black_plague/body_collision_probe.cpp"

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>

namespace penumbra_vr::backends::black_plague {
namespace {

std::array<std::uint8_t, 0x300> g_player_storage{};
std::array<std::uint8_t, 0x300> g_body_storage{};
std::array<void*, 16> g_move_states{};
void* g_test_player = g_player_storage.data();

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
        requested->values[3] - 0.15F,
        requested->values[7],
        requested->values[11]};
    return true;
}

void __fastcall FakeUpdate(void* body, void*, float) {
    const Vec3 before = Read<Vec3>(body, kCharacterPositionOffset);
    Matrix requested{};
    requested.values[0] = requested.values[5] =
        requested.values[10] = requested.values[15] = 1.0F;
    requested.values[3] = before.x + 0.20F;
    requested.values[7] = before.y;
    requested.values[11] = before.z + 0.10F;
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
    Jump(image, kCharacterUpdate, reinterpret_cast<void*>(&FakeUpdate));
    Jump(image, kCheckShapeWorldCollision,
        reinterpret_cast<void*>(&FakeCollision));

    std::string error;
    if (!InstallForImage(image, error)) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 2;
    }

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

    HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
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

    RequestBodyJumpBurst();
    for (std::size_t index = 0;
        index < BodyJumpBurstTelemetry::kCapacity; ++index) {
        HookedCharacterUpdate(g_body_storage.data(), nullptr, 0.016F);
    }
    if (!IsBodyJumpBurstComplete()) {
        static_cast<void>(RemoveBodyCollisionProbe(error));
        VirtualFree(image, 0, MEM_RELEASE);
        return 6;
    }
    const BodyJumpBurstTelemetry burst = ConsumeBodyJumpBurstTelemetry();
    if (burst.count != BodyJumpBurstTelemetry::kCapacity ||
        burst.samples.front().body_update_sequence != 2 ||
        burst.samples.back().body_update_sequence !=
            BodyJumpBurstTelemetry::kCapacity + 1 ||
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

    if (!RemoveBodyCollisionProbe(error)) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 4;
    }

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
