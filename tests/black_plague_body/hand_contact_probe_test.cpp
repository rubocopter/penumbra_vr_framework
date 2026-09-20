#include "hand_contact_probe.hpp"
#include "vr_interaction_policy.hpp"
#include "vr_rework_hand_profile.hpp"

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
namespace runtime = penumbra_vr::runtime;

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
constexpr std::uintptr_t kCreateBoxShape = 0x18AC10;
constexpr std::uintptr_t kDestroyShape = 0xD4210;
constexpr std::uintptr_t kCreateBoxShapeVtableSlot = 0x30;

enum class FakeMode {
    clear,
    collided,
    mutate_shape,
};

std::atomic<FakeMode> g_mode{FakeMode::clear};
std::atomic<std::uint32_t> g_create_count{0};
std::atomic<std::uint32_t> g_destroy_count{0};
std::uint8_t* g_fake_image = nullptr;
std::array<std::uint8_t*, 4> g_fake_palm_shapes{};
std::size_t g_fake_palm_shape_count = 0;
std::array<Vec3, 4> g_created_sizes{};
std::array<Matrix, 4> g_created_transforms{};
Matrix g_last_create_transform{};
bool g_last_create_transform_valid = false;
void* g_last_query_shape = nullptr;
void* g_last_skip_body = nullptr;
void* g_fake_collision_body = nullptr;
bool g_last_skip_static = false;
bool g_last_is_character = false;
bool g_last_collide_character = true;
bool g_last_debug = true;

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
    void* skip_body,
    bool skip_static,
    bool is_character,
    void* callback,
    bool collide_character,
    bool debug) {
    g_last_skip_body = skip_body;
    g_last_skip_static = skip_static;
    g_last_is_character = is_character;
    g_last_collide_character = collide_character;
    g_last_debug = debug;
    g_last_query_shape = shape;
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
    reinterpret_cast<Callback>(vtable[0])(
        callback, g_fake_collision_body, &data);
    resolved_position->x += 0.01F;

    if (mode == FakeMode::mutate_shape) {
        ++*reinterpret_cast<std::int32_t*>(
            static_cast<std::uint8_t*>(shape) + kShapeUserCountOffset);
    }
    return true;
}

void* __fastcall FakeCreateBoxShape(
    void* world,
    void*,
    const Vec3* size,
    Matrix* transform) {
    const auto create_index = g_create_count.load(std::memory_order_relaxed);
    if (world == nullptr || size == nullptr || transform == nullptr ||
        g_fake_image == nullptr || create_index >= g_fake_palm_shape_count) {
        return nullptr;
    }
    auto* const fake_shape = g_fake_palm_shapes[create_index];
    if (fake_shape == nullptr) return nullptr;
    g_created_sizes[create_index] = *size;
    g_created_transforms[create_index] = *transform;
    g_last_create_transform = *transform;
    g_last_create_transform_valid = true;
    ++g_create_count;
    std::memset(fake_shape, 0, 0x100);
    *reinterpret_cast<void**>(fake_shape) =
        g_fake_image + kCollideShapeNewtonVtable;
    std::memcpy(fake_shape + 0x04, &size->x, sizeof(float));
    std::memcpy(fake_shape + 0x08, &size->y, sizeof(float));
    std::memcpy(fake_shape + 0x0C, &size->z, sizeof(float));
    *reinterpret_cast<std::int32_t*>(fake_shape + kShapeTypeOffset) = 1;
    // HPL1 CreateBoxShape returns a standalone shape with no body users.
    *reinterpret_cast<std::int32_t*>(fake_shape + kShapeUserCountOffset) = 0;
    *reinterpret_cast<void**>(fake_shape + kShapeWorldOffset) = world;
    return fake_shape;
}

void __fastcall FakeDestroyShape(void*, void*, void* shape) {
    for (std::size_t index = 0; index < g_fake_palm_shape_count; ++index) {
        if (shape == g_fake_palm_shapes[index]) {
            ++g_destroy_count;
            return;
        }
    }
}

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 1.0e-6F;
}

bool MatchesExpectedPalmTransform(const Matrix& actual) {
    const auto expected = runtime::rework_hand_profile::CollisionLocalPose();
    for (std::size_t index = 0; index < expected.values.size(); ++index) {
        if (!NearlyEqual(actual.values[index], expected.values[index])) {
            return false;
        }
    }
    return true;
}

bool MatchesExpectedPalmTransform() {
    return g_last_create_transform_valid &&
        MatchesExpectedPalmTransform(g_last_create_transform);
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

        auto install_gateway = [&](std::uintptr_t rva, void* target) {
            auto* const entry = image + rva;
            entry[0] = 0xE9;
            const auto relative = static_cast<std::int32_t>(
                reinterpret_cast<std::intptr_t>(target) -
                reinterpret_cast<std::intptr_t>(entry + 5));
            std::memcpy(entry + 1, &relative, sizeof(relative));
        };
        install_gateway(kCreateBoxShape,
            reinterpret_cast<void*>(&FakeCreateBoxShape));
        install_gateway(kDestroyShape,
            reinterpret_cast<void*>(&FakeDestroyShape));

        void* const create_slot = image + kCreateBoxShape;
        std::memcpy(image + kPhysicsWorldVtable + kCreateBoxShapeVtableSlot,
            &create_slot, sizeof(create_slot));

        Write(world, 0, image + kPhysicsWorldVtable);
        Write(world2, 0, image + kPhysicsWorldVtable);
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

        g_fake_image = image;
        g_fake_palm_shape_count = palm_shapes.size();
        for (std::size_t index = 0; index < palm_shapes.size(); ++index) {
            g_fake_palm_shapes[index] = palm_shapes[index].data();
        }
        g_created_sizes = {};
        g_created_transforms = {};
        g_create_count.store(0, std::memory_order_relaxed);
        g_destroy_count.store(0, std::memory_order_relaxed);
        g_last_create_transform = {};
        g_last_create_transform_valid = false;
        g_last_query_shape = nullptr;
        g_last_skip_body = nullptr;
        g_last_skip_static = true;
        g_last_is_character = true;
        g_last_collide_character = true;
        g_last_debug = true;
    }

    ~Fixture() {
        g_fake_palm_shapes = {};
        g_fake_palm_shape_count = 0;
        g_fake_image = nullptr;
        if (image != nullptr) VirtualFree(image, 0, MEM_RELEASE);
    }

    std::uint8_t* image = nullptr;
    std::vector<std::uint8_t> world = std::vector<std::uint8_t>(0x100);
    std::vector<std::uint8_t> world2 = std::vector<std::uint8_t>(0x100);
    std::vector<std::uint8_t> physics_body = std::vector<std::uint8_t>(0x400);
    std::vector<std::uint8_t> shape = std::vector<std::uint8_t>(0x100);
    std::array<std::array<std::uint8_t, 0x100>, 4> palm_shapes{};
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

bool RunResolverValidation(
    Fixture& fixture,
    bp::PalmResolverValidationTelemetry& telemetry,
    std::string& error,
    bool replace_world = false) {
    g_mode.store(FakeMode::clear, std::memory_order_relaxed);
    bool success = false;
    std::thread requester([&] {
        success = bp::RequestPalmResolverValidation(telemetry, error);
    });
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(1);
    while (!bp::PalmResolverValidationPending() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    if (bp::PalmResolverValidationPending()) {
        bp::ServicePalmResolverValidation(
            fixture.image, fixture.character.data());
        if (replace_world) {
            Write(fixture.character, kCharacterPhysicsWorldOffset,
                fixture.world2.data());
        }
        bp::ServicePalmResolverValidation(
            fixture.image, fixture.character.data());
    }
    requester.join();
    return success;
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

    bp::PalmResolverValidationTelemetry resolver;
    if (!RunResolverValidation(fixture, resolver, error) ||
        resolver.result != bp::PalmResolverValidationResult::passed ||
        !resolver.shape_created || !resolver.shape_reused ||
        !resolver.shape_destroyed || resolver.world_replaced ||
        resolver.gameplay_memory_changed || resolver.create_count != 1 ||
        resolver.destroy_count != 1 || resolver.query_count < 2 ||
        resolver.shape_user_count != 0 || resolver.shape_type != 1 ||
        g_create_count.load(std::memory_order_relaxed) != 1 ||
        g_destroy_count.load(std::memory_order_relaxed) != 1 ||
        !MatchesExpectedPalmTransform() ||
        !NearlyEqual(resolver.shape_size[0],
            penumbra_vr::runtime::vr_interaction_policy::kCollisionSizeX) ||
        !NearlyEqual(resolver.shape_size[1],
            penumbra_vr::runtime::vr_interaction_policy::kCollisionSizeY) ||
        !NearlyEqual(resolver.shape_size[2],
            penumbra_vr::runtime::vr_interaction_policy::kCollisionSizeZ) ||
        g_last_skip_body != fixture.physics_body.data() ||
        g_last_skip_static || g_last_is_character ||
        g_last_collide_character || g_last_debug ||
        !NearlyEqual(resolver.second_raw_position[0],
            resolver.second_resolved_position[0])) {
        std::cerr << "owned palm resolver lifecycle/profile failed: " << error << '\n';
        return 1;
    }

    Fixture replacement_fixture;
    bp::PalmResolverValidationTelemetry replacement;
    if (!RunResolverValidation(
        replacement_fixture, replacement, error, true) ||
        replacement.result != bp::PalmResolverValidationResult::passed ||
        !replacement.shape_created ||
        !replacement.shape_destroyed || !replacement.world_replaced ||
        replacement.gameplay_memory_changed || replacement.create_count != 2 ||
        replacement.destroy_count != 2 ||
        !MatchesExpectedPalmTransform() ||
        g_create_count.load(std::memory_order_relaxed) != 2 ||
        g_destroy_count.load(std::memory_order_relaxed) != 2) {
        std::cerr << "owned palm world replacement failed: " << error << '\n';
        return 1;
    }

    SetEnvironmentVariableA("PVR_BP_PALM_COLLISION_VALIDATION", nullptr);
    g_mode.store(FakeMode::clear, std::memory_order_relaxed);
    runtime::VrMatrix44 left_raw{};
    left_raw.values[0] = left_raw.values[5] =
        left_raw.values[10] = left_raw.values[15] = 1.0F;
    left_raw.values[3] = 10.15F;
    left_raw.values[7] = 20.0F;
    left_raw.values[11] = 30.0F;
    runtime::VrMatrix44 head{};
    head.values[0] = head.values[5] = head.values[10] = head.values[15] = 1.0F;
    head.values[3] = 10.0F;
    head.values[7] = 21.4F;
    head.values[11] = 30.0F;
    std::array<runtime::VrMatrix44, 2> raw_poses{};
    raw_poses[0] = left_raw;
    const std::array<bool, 2> raw_valid{true, false};
    void* const held_body = replacement_fixture.shape.data();
    bp::PublishGameplayPalmHeldBody(0, held_body);
    bp::PublishGameplayPalmTracking(raw_poses, raw_valid, head, true, 1);
    bp::ServiceGameplayPalmResolver(
        replacement_fixture.image, replacement_fixture.character.data());
    runtime::VrMatrix44 gameplay_resolved{};
    auto gameplay = bp::ConsumeGameplayPalmResolverTelemetry();
    if (!bp::ReadGameplayPalmPose(0, gameplay_resolved) ||
        bp::GameplayPalmPoseGeneration(0) != 1 ||
        gameplay.enabled == false ||
        gameplay.source != bp::GameplayPalmResolverRequestSource::production ||
        gameplay.samples != 1 || gameplay.published_poses != 1 ||
        gameplay.queries < 2 || gameplay.contacts != 0 ||
        gameplay.held_body_skips != 1 || gameplay.query_failures != 0 ||
        gameplay.shape_creates != 2 || gameplay.shape_destroys != 0 ||
        !MatchesExpectedPalmTransform(g_created_transforms[2]) ||
        g_last_skip_body != held_body || g_last_skip_static ||
        g_last_is_character || g_last_collide_character || g_last_debug ||
        !NearlyEqual(gameplay_resolved.values[3], left_raw.values[3])) {
        std::cerr << "gameplay palm publication/exclusion contract failed\n";
        return 1;
    }

    g_mode.store(FakeMode::collided, std::memory_order_relaxed);
    g_fake_collision_body = replacement_fixture.physics_body.data();
    bp::GameplayPalmOverlapResult overlap{};
    if (!bp::QueryGameplayPalmOverlaps(0, gameplay_resolved, overlap) ||
        !overlap.valid || overlap.hit_count != 1 ||
        overlap.hits[0].body != g_fake_collision_body ||
        overlap.hits[0].contact_count != 2 ||
        !NearlyEqual(overlap.hits[0].contact_sum[0], 5.0F) ||
        !NearlyEqual(overlap.hits[0].contact_sum[1], 7.0F) ||
        !NearlyEqual(overlap.hits[0].contact_sum[2], 9.0F) ||
        g_last_skip_body != held_body || g_last_skip_static ||
        g_last_is_character || g_last_collide_character || g_last_debug ||
        g_create_count.load(std::memory_order_relaxed) != 4 ||
        g_last_query_shape != replacement_fixture.palm_shapes[3].data() ||
        !NearlyEqual(g_created_sizes[3].x, 0.34F) ||
        !NearlyEqual(g_created_sizes[3].y, 0.17F) ||
        !NearlyEqual(g_created_sizes[3].z, 0.24F) ||
        !NearlyEqual(g_created_transforms[3].values[0], 1.0F) ||
        !NearlyEqual(g_created_transforms[3].values[5], 1.0F) ||
        !NearlyEqual(g_created_transforms[3].values[10], 1.0F) ||
        !NearlyEqual(g_created_transforms[3].values[15], 1.0F) ||
        !NearlyEqual(g_created_transforms[3].values[3], 0.011F) ||
        !NearlyEqual(g_created_transforms[3].values[7], 0.002F) ||
        !NearlyEqual(g_created_transforms[3].values[11], -0.011F)) {
        std::cerr << "gameplay palm overlap query contract failed\n";
        return 37;
    }
    g_fake_collision_body = nullptr;
    g_mode.store(FakeMode::clear, std::memory_order_relaxed);

    // A snap/smooth-turn world-yaw epoch is a coordinate-space rebase, not a
    // tracking teleport. The old resolved pose is invalidated immediately and
    // resolver history is reset before the next collision solve.
    runtime::VrMatrix44 turned = left_raw;
    turned.values[3] += 0.75F;
    raw_poses[0] = turned;
    bp::PublishGameplayPalmTracking(raw_poses, raw_valid, head, true, 2);
    if (bp::ReadGameplayPalmPose(0, gameplay_resolved)) {
        std::cerr << "yaw epoch retained stale resolved palm pose\n";
        return 35;
    }
    bp::ServiceGameplayPalmResolver(
        replacement_fixture.image, replacement_fixture.character.data());
    gameplay = bp::ConsumeGameplayPalmResolverTelemetry();
    if (!bp::ReadGameplayPalmPose(0, gameplay_resolved) ||
        bp::GameplayPalmPoseGeneration(0) != 2 ||
        gameplay.yaw_epoch_resets != 1 || gameplay.tracking_reanchors != 0 ||
        gameplay.recovery_anchors != 0) {
        std::cerr << "yaw epoch palm-history rebase failed\n";
        return 36;
    }

    std::atomic<bool> shutdown_done{false};
    bool shutdown_ok = false;
    std::string shutdown_error;
    std::thread shutdown_thread([&] {
        shutdown_ok = bp::ShutdownGameplayPalmResolver(shutdown_error);
        shutdown_done.store(true, std::memory_order_release);
    });
    const auto shutdown_deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(1);
    while (!shutdown_done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < shutdown_deadline) {
        bp::ServiceGameplayPalmResolver(
            replacement_fixture.image, replacement_fixture.character.data());
        Sleep(1);
    }
    shutdown_thread.join();
    gameplay = bp::ConsumeGameplayPalmResolverTelemetry();
    if (!shutdown_ok || !shutdown_error.empty() ||
        bp::ReadGameplayPalmPose(0, gameplay_resolved) ||
        gameplay.enabled || gameplay.shape_destroys != 2 ||
        g_destroy_count.load(std::memory_order_relaxed) != 4) {
        std::cerr << "gameplay palm game-thread teardown failed: "
                  << shutdown_error << '\n';
        return 1;
    }
    bp::PublishGameplayPalmHeldBody(0, nullptr);
    SetEnvironmentVariableA("PVR_BP_PALM_COLLISION_VALIDATION", nullptr);

    std::cout << "Black Plague hand-contact query/lifecycle/profile tests passed\n";
    return 0;
}
