#include "hand_contact_probe.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace bp = penumbra_vr::backends::black_plague;

namespace {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Matrix {
    std::array<float, 16> values{};
};

struct CollidePoint {
    Vec3 point{};
    Vec3 normal{};
    float depth = 0.0F;
};

struct LegacyCollideData {
    std::uint32_t allocator_or_proxy = 0;
    const CollidePoint* first = nullptr;
    const CollidePoint* last = nullptr;
    const CollidePoint* capacity_end = nullptr;
    std::int32_t point_count = 0;
};

static_assert(sizeof(void*) == 4);
static_assert(sizeof(CollidePoint) == 0x1C);
static_assert(sizeof(LegacyCollideData) == 0x14);

constexpr std::uintptr_t kCharacterPositionOffset = 0x48;
constexpr std::uintptr_t kCharacterPhysicsBodyOffset = 0x23C;
constexpr std::uintptr_t kCharacterPhysicsWorldOffset = 0x240;
constexpr std::uintptr_t kPhysicsBodyMatrixOffset = 0x34;
constexpr std::uintptr_t kPhysicsBodyShapeOffset = 0x340;
constexpr std::uintptr_t kShapeTypeOffset = 0x10;
constexpr std::uintptr_t kShapeUserCountOffset = 0x54;
constexpr std::uintptr_t kShapeWorldOffset = 0x58;
constexpr std::uintptr_t kPhysicsWorldVtable = 0x291B80;
constexpr std::uintptr_t kPhysicsBodyVtable = 0x292C08;
constexpr std::uintptr_t kCollideShapeNewtonVtable = 0x292D40;
constexpr std::uintptr_t kCheckShapeWorldCollision = 0xD4830;

enum class FakeMode {
    clear,
    collided,
    mutate_shape,
};

std::atomic<FakeMode> g_mode{FakeMode::clear};

template<class T>
void Write(std::vector<std::uint8_t>& bytes, std::size_t offset, T value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

bool __fastcall FakeCheckShapeWorldCollision(
    void*,
    void*,
    Vec3* resolved_position,
    void* shape,
    const Matrix*,
    void*,
    bool,
    bool,
    void* callback,
    bool,
    bool) {
    const FakeMode mode = g_mode.load(std::memory_order_relaxed);
    if (mode == FakeMode::clear) return false;

    std::array<CollidePoint, 2> points{{
        {{1.0F, 2.0F, 3.0F}, {0.0F, 1.0F, 0.0F}, 0.025F},
        {{4.0F, 5.0F, 6.0F}, {1.0F, 0.0F, 0.0F}, 0.075F},
    }};
    LegacyCollideData data;
    data.first = points.data();
    data.last = points.data() + points.size();
    data.capacity_end = data.last;
    data.point_count = static_cast<std::int32_t>(points.size());

    using Callback = void(__thiscall*)(void*, void*, void*);
    auto** const vtable = *reinterpret_cast<void***>(callback);
    reinterpret_cast<Callback>(vtable[0])(callback, nullptr, &data);
    resolved_position->x += 0.01F;

    if (mode == FakeMode::mutate_shape) {
        ++*reinterpret_cast<std::int32_t*>(
            static_cast<std::uint8_t*>(shape) + kShapeUserCountOffset);
    }
    return true;
}

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 1.0e-6F;
}

struct Fixture {
    Fixture() {
        image = static_cast<std::uint8_t*>(VirtualAlloc(
            nullptr, 0x2A0000, MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (image == nullptr) return;

        auto* const gateway = image + kCheckShapeWorldCollision;
        gateway[0] = 0xE9;
        const auto displacement = static_cast<std::int32_t>(
            reinterpret_cast<std::intptr_t>(
                reinterpret_cast<void*>(&FakeCheckShapeWorldCollision)) -
            reinterpret_cast<std::intptr_t>(gateway + 5));
        std::memcpy(gateway + 1, &displacement, sizeof(displacement));

        Write(world, 0, image + kPhysicsWorldVtable);
        Write(physics_body, 0, image + kPhysicsBodyVtable);
        Write(shape, 0, image + kCollideShapeNewtonVtable);
        Write(shape, 0x04, 0.6F);
        Write(shape, 0x08, 1.65F);
        Write(shape, 0x0C, 0.6F);
        Write(shape, kShapeTypeOffset, std::int32_t{1});
        Write(shape, kShapeUserCountOffset, std::int32_t{1});
        Write(shape, kShapeWorldOffset, world.data());

        Matrix transform;
        transform.values[0] = 1.0F;
        transform.values[5] = 1.0F;
        transform.values[10] = 1.0F;
        transform.values[15] = 1.0F;
        transform.values[3] = 10.0F;
        transform.values[7] = 20.0F;
        transform.values[11] = 30.0F;
        std::memcpy(physics_body.data() + kPhysicsBodyMatrixOffset,
            &transform, sizeof(transform));
        Write(physics_body, kPhysicsBodyShapeOffset, shape.data());

        Write(character, kCharacterPhysicsBodyOffset, physics_body.data());
        Write(character, kCharacterPhysicsWorldOffset, world.data());
        const Vec3 position{10.0F, 20.0F, 30.0F};
        std::memcpy(character.data() + kCharacterPositionOffset,
            &position, sizeof(position));
    }

    ~Fixture() {
        if (image != nullptr) VirtualFree(image, 0, MEM_RELEASE);
    }

    std::uint8_t* image = nullptr;
    std::vector<std::uint8_t> world = std::vector<std::uint8_t>(0x100);
    std::vector<std::uint8_t> physics_body = std::vector<std::uint8_t>(0x400);
    std::vector<std::uint8_t> shape = std::vector<std::uint8_t>(0x100);
    std::vector<std::uint8_t> character = std::vector<std::uint8_t>(0x300);
};

bool RunQuery(
    Fixture& fixture,
    FakeMode mode,
    bool expected_success,
    bp::HandContactQueryTelemetry& telemetry,
    std::string& error) {
    g_mode.store(mode, std::memory_order_relaxed);
    bool success = false;
    std::thread requester([&] {
        success = bp::RequestNoWriteHandContactQuery(telemetry, error);
    });
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(1);
    while (!bp::NoWriteHandContactQueryPending() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    if (bp::NoWriteHandContactQueryPending()) {
        bp::ServiceNoWriteHandContactQuery(
            fixture.image, fixture.character.data());
    }
    requester.join();
    return success == expected_success;
}

} // namespace

int main() {
    std::array<CollidePoint, 2> points{{
        {{}, {0.0F, 1.0F, 0.0F}, 0.01F},
        {{}, {1.0F, 0.0F, 0.0F}, 0.04F},
    }};
    LegacyCollideData data;
    data.first = points.data();
    data.last = points.data() + points.size();
    data.capacity_end = data.last;
    data.point_count = 2;
    std::uint32_t count = 0;
    float depth = 0.0F;
    if (!bp::AccumulateLegacyHandContacts(&data, count, depth) ||
        count != 2 || !NearlyEqual(depth, 0.04F)) {
        std::cerr << "legacy contact parsing failed\n";
        return 1;
    }
    data.last = data.first + 1;
    if (bp::AccumulateLegacyHandContacts(&data, count, depth)) {
        std::cerr << "truncated legacy contact data was accepted\n";
        return 1;
    }

    Fixture fixture;
    if (fixture.image == nullptr) {
        std::cerr << "synthetic image allocation failed\n";
        return 1;
    }

    bp::HandContactQueryTelemetry telemetry;
    std::string error;
    if (!RunQuery(fixture, FakeMode::clear, true, telemetry, error) ||
        telemetry.result != bp::HandContactQueryResult::clear ||
        telemetry.native_collided || telemetry.callback_invoked ||
        telemetry.native_memory_changed) {
        std::cerr << "clear no-write query failed: " << error << '\n';
        return 1;
    }

    if (!RunQuery(fixture, FakeMode::collided, true, telemetry, error) ||
        telemetry.result != bp::HandContactQueryResult::collided ||
        !telemetry.native_collided || !telemetry.callback_invoked ||
        telemetry.callback_count != 1 || telemetry.contact_count != 2 ||
        !NearlyEqual(telemetry.maximum_contact_depth, 0.075F) ||
        !NearlyEqual(telemetry.corrected_position[0], 10.01F) ||
        telemetry.native_memory_changed) {
        std::cerr << "collided no-write query failed: " << error << '\n';
        return 1;
    }

    if (!RunQuery(fixture, FakeMode::mutate_shape, false, telemetry, error) ||
        telemetry.result != bp::HandContactQueryResult::native_memory_changed ||
        !telemetry.native_memory_changed) {
        std::cerr << "native mutation was not rejected\n";
        return 1;
    }

    std::cout << "Black Plague no-write hand-contact query tests passed\n";
    return 0;
}
