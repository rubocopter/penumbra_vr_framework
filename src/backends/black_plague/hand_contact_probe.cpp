#include "hand_contact_probe.hpp"
#include "vr_hand_contact.hpp"
#include "vr_interaction_policy.hpp"
#include "vr_rework_hand_profile.hpp"

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

struct CollidePoint {
    Vec3 point{};
    Vec3 normal{};
    float depth = 0.0F;
};

// VC7 std::vector stores an allocator/proxy word before its three pointers.
// D49E3 passes this object to callback slot 0; D49FC reads mlNumOfPoints at
// +0x10 and D4A10 walks 0x1C-byte points from the pointer at +0x04.
struct LegacyCollideData {
    std::uint32_t allocator_or_proxy = 0;
    const CollidePoint* first = nullptr;
    const CollidePoint* last = nullptr;
    const CollidePoint* capacity_end = nullptr;
    std::int32_t point_count = 0;
};

static_assert(sizeof(CollidePoint) == 0x1C);
static_assert(sizeof(void*) == 4,
    "The Black Plague exact-build adapter requires an x86 target");
static_assert(sizeof(LegacyCollideData) == 0x14);

using CheckShapeWorldCollision = bool(__thiscall*)(
    void*, Vec3*, void*, const Matrix*, void*, bool, bool, void*, bool, bool);
using CreateBoxShape = void*(__thiscall*)(void*, const Vec3*, Matrix*);
using DestroyShape = void(__thiscall*)(void*, void*);

constexpr std::uintptr_t kCharacterPositionOffset = 0x48;
constexpr std::uintptr_t kCharacterPhysicsBodyOffset = 0x23C;
constexpr std::uintptr_t kCharacterPhysicsWorldOffset = 0x240;
constexpr std::uintptr_t kPhysicsBodyMatrixOffset = 0x34;
constexpr std::uintptr_t kPhysicsBodyShapeOffset = 0x340;
constexpr std::uintptr_t kPhysicsWorldShapeListOffset = 0x04;
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
constexpr wchar_t kGameplayPalmRequestMutexName[] =
    L"Local\\PenumbraVR.BlackPlague.PalmCollisionValidation";
constexpr std::uint64_t kGameplayPalmSampleMaximumAgeMilliseconds = 250;

enum class RequestState : std::uint32_t {
    idle,
    pending,
    processing,
    passed,
    failed,
};

std::atomic<RequestState> g_request_state{RequestState::idle};
SRWLOCK g_result_lock = SRWLOCK_INIT;
HandContactQueryTelemetry g_result;

std::atomic<RequestState> g_resolver_request_state{RequestState::idle};
SRWLOCK g_resolver_result_lock = SRWLOCK_INIT;
PalmResolverValidationTelemetry g_resolver_result;
PalmResolverValidationTelemetry g_resolver_working;

struct OwnedPalmShape {
    std::uint8_t* image = nullptr;
    void* world = nullptr;
    void* shape = nullptr;
};

OwnedPalmShape g_validation_palm_shape;
runtime::VrHandResolveState g_resolver_state;
bool g_resolver_first_tick_complete = false;

OwnedPalmShape g_gameplay_palm_shape;
OwnedPalmShape g_gameplay_interaction_shape;
std::atomic<bool> g_gameplay_shape_active{false};
std::array<runtime::VrHandResolveState, 2> g_gameplay_resolver_state{};
std::array<std::atomic<void*>, 2> g_gameplay_held_body{};
std::atomic<GameplayInteractionTargetProvider>
    g_gameplay_interaction_target_provider{nullptr};
SRWLOCK g_gameplay_tracking_lock = SRWLOCK_INIT;
std::array<runtime::VrMatrix44, 2> g_gameplay_raw_poses{};
std::array<bool, 2> g_gameplay_raw_valid{};
runtime::VrMatrix44 g_gameplay_head_pose{};
bool g_gameplay_head_valid = false;
std::uint64_t g_gameplay_tracking_time = 0;
std::uint64_t g_gameplay_tracking_yaw_epoch = 0;
std::uint64_t g_gameplay_tracking_generation = 0;
std::uint64_t g_gameplay_resolver_yaw_epoch = 0;
std::array<std::uint64_t, 2> g_gameplay_resolved_tracking_generation{};
std::array<void*, 2> g_gameplay_resolved_skip_body{};
SRWLOCK g_gameplay_pose_lock = SRWLOCK_INIT;
std::array<runtime::VrMatrix44, 2> g_gameplay_resolved_poses{};
std::array<bool, 2> g_gameplay_resolved_valid{};
std::array<std::uint64_t, 2> g_gameplay_resolved_generation{};
std::uint64_t g_gameplay_resolved_time = 0;
SRWLOCK g_gameplay_telemetry_lock = SRWLOCK_INIT;
GameplayPalmResolverTelemetry g_gameplay_telemetry{};
bool g_gameplay_request_sampled = false;
std::atomic<GameplayPalmResolverRequestSource> g_gameplay_request_source{
    GameplayPalmResolverRequestSource::disabled};
std::atomic<bool> g_gameplay_shutdown_requested{false};
std::atomic<bool> g_gameplay_shutdown_complete{true};
std::atomic<bool> g_gameplay_shutdown_succeeded{true};

bool ReadBytes(const void* source, void* destination, std::size_t size) noexcept {
    if (source == nullptr || destination == nullptr) return false;
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

struct NativeSnapshot {
    std::array<std::uint8_t, 12> world_shape_list{};
    std::array<std::uint8_t, 12> character_position{};
    std::array<std::uint8_t, 64> body_matrix{};
    std::array<std::uint8_t, 4> body_shape_pointer{};
    std::array<std::uint8_t, 0x60> shape_header{};
};

[[nodiscard]] bool CaptureSnapshot(
    void* world,
    void* character_body,
    void* physics_body,
    void* shape,
    NativeSnapshot& snapshot) noexcept {
    return ReadBytes(static_cast<std::uint8_t*>(world) +
            kPhysicsWorldShapeListOffset,
        snapshot.world_shape_list.data(), snapshot.world_shape_list.size()) &&
        ReadBytes(static_cast<std::uint8_t*>(character_body) +
            kCharacterPositionOffset,
        snapshot.character_position.data(), snapshot.character_position.size()) &&
        ReadBytes(static_cast<std::uint8_t*>(physics_body) +
            kPhysicsBodyMatrixOffset,
        snapshot.body_matrix.data(), snapshot.body_matrix.size()) &&
        ReadBytes(static_cast<std::uint8_t*>(physics_body) +
            kPhysicsBodyShapeOffset,
        snapshot.body_shape_pointer.data(), snapshot.body_shape_pointer.size()) &&
        ReadBytes(shape, snapshot.shape_header.data(), snapshot.shape_header.size());
}

class NativeContactCallback final {
public:
    virtual void OnCollision(void*, void* collide_data) {
        ++callback_count;
        std::uint32_t next_count = 0;
        float next_depth = 0.0F;
        if (!AccumulateLegacyHandContacts(
                collide_data, next_count, next_depth)) {
            valid = false;
            return;
        }
        contact_count += next_count;
        maximum_depth = std::max(maximum_depth, next_depth);
    }

    bool valid = true;
    std::uint32_t callback_count = 0;
    std::uint32_t contact_count = 0;
    float maximum_depth = 0.0F;
};

struct GameplaySnapshot {
    std::array<std::uint8_t, 12> character_position{};
    std::array<std::uint8_t, 64> body_matrix{};
    std::array<std::uint8_t, 4> body_shape_pointer{};
};

[[nodiscard]] bool CaptureGameplaySnapshot(
    void* character_body,
    void* physics_body,
    GameplaySnapshot& snapshot) noexcept {
    return ReadBytes(static_cast<std::uint8_t*>(character_body) +
            kCharacterPositionOffset,
        snapshot.character_position.data(), snapshot.character_position.size()) &&
        ReadBytes(static_cast<std::uint8_t*>(physics_body) +
            kPhysicsBodyMatrixOffset,
        snapshot.body_matrix.data(), snapshot.body_matrix.size()) &&
        ReadBytes(static_cast<std::uint8_t*>(physics_body) +
            kPhysicsBodyShapeOffset,
        snapshot.body_shape_pointer.data(), snapshot.body_shape_pointer.size());
}

[[nodiscard]] bool ValidWorld(std::uint8_t* image, void* world) noexcept {
    if (image == nullptr || world == nullptr ||
        Read<void*>(world, 0) != image + kPhysicsWorldVtable) {
        return false;
    }
    void** const vtable = Read<void**>(world, 0);
    if (vtable == nullptr) return false;
    void* create_slot = nullptr;
    return ReadBytes(reinterpret_cast<std::uint8_t*>(vtable) +
            kCreateBoxShapeVtableSlot, &create_slot, sizeof(create_slot)) &&
        create_slot == image + kCreateBoxShape;
}

[[nodiscard]] bool SafeDestroyOwnedPalmShape(
    OwnedPalmShape& owned_shape,
    PalmResolverValidationTelemetry* telemetry) noexcept {
    if (owned_shape.shape == nullptr) return true;
    const auto owner = owned_shape;
    owned_shape = {};
    if (!ValidWorld(owner.image, owner.world)) {
        // A destroyed world owns and tears down its shape list. Forget a stale
        // handle instead of writing through a dead world pointer.
        return true;
    }
    __try {
        const auto destroy = reinterpret_cast<DestroyShape>(
            owner.image + kDestroyShape);
        destroy(owner.world, owner.shape);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (telemetry != nullptr) {
        telemetry->shape_destroyed = true;
        ++telemetry->destroy_count;
    }
    return true;
}

[[nodiscard]] bool EnsureOwnedBoxShape(
    std::uint8_t* image,
    void* world,
    OwnedPalmShape& owned_shape,
    const Vec3& size,
    const Matrix& local_transform,
    PalmResolverValidationTelemetry& telemetry) noexcept {
    if (!ValidWorld(image, world)) return false;
    if (owned_shape.shape != nullptr &&
        owned_shape.image == image &&
        owned_shape.world == world) {
        telemetry.shape_reused = true;
        return true;
    }
    if (owned_shape.shape != nullptr) {
        telemetry.world_replaced = true;
        if (!SafeDestroyOwnedPalmShape(owned_shape, &telemetry)) return false;
    }

    void* shape = nullptr;
    __try {
        const auto create = reinterpret_cast<CreateBoxShape>(image + kCreateBoxShape);
        auto transform = local_transform;
        shape = create(world, &size, &transform);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        shape = nullptr;
    }
    // Rework/HPL1 creates standalone collision shapes with zero users. A body
    // increments the count when it adopts a shape; this diagnostic palm never
    // belongs to a body, so requiring a positive count rejects a valid freshly
    // created box.
    if (shape == nullptr ||
        Read<void*>(shape, 0) != image + kCollideShapeNewtonVtable ||
        Read<void*>(shape, kShapeWorldOffset) != world ||
        Read<std::int32_t>(shape, kShapeUserCountOffset) != 0 ||
        Read<std::int32_t>(shape, kShapeTypeOffset) != 1) {
        if (shape != nullptr) {
            owned_shape = {image, world, shape};
            static_cast<void>(SafeDestroyOwnedPalmShape(owned_shape, &telemetry));
        }
        return false;
    }
    owned_shape = {image, world, shape};
    telemetry.shape_created = true;
    ++telemetry.create_count;
    telemetry.shape_type = Read<std::int32_t>(shape, kShapeTypeOffset);
    telemetry.shape_user_count = Read<std::int32_t>(shape, kShapeUserCountOffset);
    telemetry.shape_size = {
        Read<float>(shape, 0x04), Read<float>(shape, 0x08), Read<float>(shape, 0x0C)};
    return std::isfinite(telemetry.shape_size[0]) &&
        std::isfinite(telemetry.shape_size[1]) &&
        std::isfinite(telemetry.shape_size[2]);
}

[[nodiscard]] bool EnsureOwnedPalmShape(
    std::uint8_t* image,
    void* world,
    OwnedPalmShape& owned_shape,
    PalmResolverValidationTelemetry& telemetry) noexcept {
    using namespace runtime::vr_interaction_policy;
    const Vec3 size{kCollisionSizeX, kCollisionSizeY, kCollisionSizeZ};
    Matrix local_transform{};
    local_transform.values = runtime::rework_hand_profile::CollisionLocalPose().values;
    return EnsureOwnedBoxShape(
        image, world, owned_shape, size, local_transform, telemetry);
}

[[nodiscard]] bool EnsureOwnedInteractionShape(
    std::uint8_t* image,
    void* world,
    OwnedPalmShape& owned_shape,
    PalmResolverValidationTelemetry& telemetry) noexcept {
    using namespace runtime::vr_interaction_policy;
    const Vec3 size{kInteractionSizeX, kInteractionSizeY, kInteractionSizeZ};
    Matrix local_transform{};
    local_transform.values[0] = 1.0F;
    local_transform.values[5] = 1.0F;
    local_transform.values[10] = 1.0F;
    local_transform.values[15] = 1.0F;
    local_transform.values[3] = kInteractionOffsetX;
    local_transform.values[7] = kInteractionOffsetY;
    local_transform.values[11] = kInteractionOffsetZ;
    return EnsureOwnedBoxShape(
        image, world, owned_shape, size, local_transform, telemetry);
}

class ResolverContactCallback final {
public:
    ResolverContactCallback(
        const runtime::VrHandContactVector& motion,
        float tolerance) noexcept
        : accumulator(motion, tolerance) {}

    virtual void OnCollision(void*, void* collide_data) {
        ++callback_count;
        LegacyCollideData data{};
        if (!ReadBytes(collide_data, &data, sizeof(data)) ||
            data.point_count < 0 || data.point_count > 32) {
            valid = false;
            return;
        }
        if (data.point_count == 0) return;
        if (data.first == nullptr || data.last == nullptr) {
            valid = false;
            return;
        }
        const auto first = reinterpret_cast<std::uintptr_t>(data.first);
        const auto last = reinterpret_cast<std::uintptr_t>(data.last);
        const auto required = static_cast<std::uintptr_t>(data.point_count) *
            sizeof(CollidePoint);
        if (last < first || last - first < required) {
            valid = false;
            return;
        }
        for (std::int32_t index = 0; index < data.point_count; ++index) {
            CollidePoint point{};
            const auto* source = reinterpret_cast<const std::uint8_t*>(data.first) +
                static_cast<std::size_t>(index) * sizeof(CollidePoint);
            if (!ReadBytes(source, &point, sizeof(point)) ||
                !Finite(point.point) || !Finite(point.normal) ||
                !std::isfinite(point.depth) || point.depth < 0.0F) {
                valid = false;
                return;
            }
            accumulator.Add(point.depth,
                {point.normal.x, point.normal.y, point.normal.z});
            ++contact_count;
        }
    }

    runtime::VrHandContactAccumulator accumulator;
    bool valid = true;
    std::uint32_t callback_count = 0;
    std::uint32_t contact_count = 0;
};

class GameplayPalmOverlapCallback final {
public:
    virtual void OnCollision(void* body, void* collide_data) {
        if (body == nullptr) {
            valid = false;
            return;
        }
        LegacyCollideData data{};
        if (!ReadBytes(collide_data, &data, sizeof(data)) ||
            data.point_count < 0 || data.point_count > 32) {
            valid = false;
            return;
        }
        if (data.point_count == 0) return;
        if (data.first == nullptr || data.last == nullptr) {
            valid = false;
            return;
        }
        const auto first = reinterpret_cast<std::uintptr_t>(data.first);
        const auto last = reinterpret_cast<std::uintptr_t>(data.last);
        const auto required = static_cast<std::uintptr_t>(data.point_count) *
            sizeof(CollidePoint);
        if (last < first || last - first < required) {
            valid = false;
            return;
        }

        GameplayPalmOverlapHit* hit = nullptr;
        for (std::size_t index = 0; index < result.hit_count; ++index) {
            if (result.hits[index].body == body) {
                hit = &result.hits[index];
                break;
            }
        }
        if (hit == nullptr) {
            if (result.hit_count >= result.hits.size()) return;
            hit = &result.hits[result.hit_count++];
            hit->body = body;
        }
        for (std::int32_t index = 0; index < data.point_count; ++index) {
            CollidePoint point{};
            const auto* source = reinterpret_cast<const std::uint8_t*>(data.first) +
                static_cast<std::size_t>(index) * sizeof(CollidePoint);
            if (!ReadBytes(source, &point, sizeof(point)) ||
                !Finite(point.point) || !Finite(point.normal) ||
                !std::isfinite(point.depth) || point.depth < 0.0F) {
                valid = false;
                return;
            }
            hit->contact_sum[0] += point.point.x;
            hit->contact_sum[1] += point.point.y;
            hit->contact_sum[2] += point.point.z;
            ++hit->contact_count;
        }
    }

    GameplayPalmOverlapResult result{};
    bool valid = true;
};

struct NativePalmQueryContext {
    std::uint8_t* image = nullptr;
    void* world = nullptr;
    void* shape = nullptr;
    void* skip_body = nullptr;
    PalmResolverValidationTelemetry* telemetry = nullptr;
    bool failed = false;
};

[[nodiscard]] bool QueryOwnedPalmShape(
    void* opaque,
    const runtime::VrMatrix44& pose,
    const runtime::VrHandContactVector& motion,
    float tolerance,
    runtime::VrHandCollisionDecision& decision) noexcept {
    auto* context = static_cast<NativePalmQueryContext*>(opaque);
    if (context == nullptr || context->image == nullptr ||
        context->world == nullptr || context->shape == nullptr ||
        context->telemetry == nullptr) {
        if (context != nullptr) context->failed = true;
        return false;
    }
    Matrix transform{};
    transform.values = pose.values;
    const runtime::VrHandContactVector requested{
        transform.values[3], transform.values[7], transform.values[11]};
    Vec3 corrected{requested[0], requested[1], requested[2]};
    ResolverContactCallback callback(motion, tolerance);
    bool collided = false;
    __try {
        const auto query = reinterpret_cast<CheckShapeWorldCollision>(
            context->image + kCheckShapeWorldCollision);
        // Exact-image evidence at D48FB/D4903 proves that the eighth stack
        // argument rejects every body marked IsCharacter when false. D4919
        // independently rejects the exact fourth-argument body. Rework uses
        // those two filters together so the player never blocks a palm and a
        // held body can later be supplied as this per-hand skip body.
        collided = query(context->world, &corrected, context->shape, &transform,
            context->skip_body, false, false, &callback, false, false);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        context->failed = true;
        return false;
    }
    ++context->telemetry->query_count;
    context->telemetry->contact_count += callback.contact_count;
    if (!callback.valid || !Finite(corrected) ||
        collided != (callback.callback_count != 0)) {
        context->failed = true;
        return false;
    }
    decision = runtime::ResolveVrHandCollision(collided, requested,
        {corrected.x, corrected.y, corrected.z}, callback.accumulator.summary());
    return true;
}

void PublishResolverResult(
    const PalmResolverValidationTelemetry& result,
    RequestState state) noexcept {
    AcquireSRWLockExclusive(&g_resolver_result_lock);
    g_resolver_result = result;
    ReleaseSRWLockExclusive(&g_resolver_result_lock);
    g_resolver_request_state.store(state, std::memory_order_release);
}

void PublishResult(
    const HandContactQueryTelemetry& result,
    RequestState state) noexcept {
    AcquireSRWLockExclusive(&g_result_lock);
    g_result = result;
    ReleaseSRWLockExclusive(&g_result_lock);
    g_request_state.store(state, std::memory_order_release);
}

[[nodiscard]] GameplayPalmResolverRequestSource
GameplayPalmResolverRequested() noexcept {
    char option[2]{};
    if (GetEnvironmentVariableA("PVR_BP_PALM_COLLISION_VALIDATION",
            option, 2) == 1 && option[0] == '1') {
        return GameplayPalmResolverRequestSource::environment;
    }
    HANDLE request = OpenMutexW(
        SYNCHRONIZE, FALSE, kGameplayPalmRequestMutexName);
    if (request == nullptr)
        return GameplayPalmResolverRequestSource::production;
    CloseHandle(request);
    return GameplayPalmResolverRequestSource::mutex;
}

void InvalidateGameplayPalmPoses() noexcept {
    AcquireSRWLockExclusive(&g_gameplay_pose_lock);
    g_gameplay_resolved_valid = {};
    g_gameplay_resolved_time = 0;
    ReleaseSRWLockExclusive(&g_gameplay_pose_lock);
}

void ResetGameplayPalmStates() noexcept {
    for (auto& state : g_gameplay_resolver_state) {
        runtime::ResetVrHandResolveState(state);
    }
    g_gameplay_resolved_tracking_generation = {};
    g_gameplay_resolved_skip_body = {};
    InvalidateGameplayPalmPoses();
}

[[nodiscard]] runtime::VrHandResolverFrame GameplayResolverFrame(
    const runtime::VrMatrix44& head,
    bool head_valid,
    bool left_hand,
    std::size_t hand_index) noexcept {
    runtime::VrHandResolverFrame frame;
    frame.left_hand = left_hand;
    frame.head_basis_valid = head_valid;
    if (!head_valid) return frame;
    frame.head = {head.values[3], head.values[7], head.values[11]};
    frame.right = {head.values[0], head.values[4], head.values[8]};
    frame.up = {head.values[1], head.values[5], head.values[9]};
    // Framework world poses use OpenVR's -Z forward convention.
    frame.forward = {-head.values[2], -head.values[6], -head.values[10]};
    std::array<float,3> target{};
    const auto provider =
        g_gameplay_interaction_target_provider.load(std::memory_order_acquire);
    if (provider && provider(hand_index,target)) {
        frame.interaction_target = {target[0],target[1],target[2]};
        frame.interaction_target_valid = true;
    }
    return frame;
}

} // namespace

bool AccumulateLegacyHandContacts(
    const void* collide_data,
    std::uint32_t& contact_count,
    float& maximum_depth) noexcept {
    contact_count = 0;
    maximum_depth = 0.0F;
    LegacyCollideData data{};
    if (!ReadBytes(collide_data, &data, sizeof(data)) ||
        data.point_count < 0 || data.point_count > 32) {
        return false;
    }
    if (data.point_count == 0) return true;
    if (data.first == nullptr || data.last == nullptr) {
        return false;
    }
    const auto first = reinterpret_cast<std::uintptr_t>(data.first);
    const auto last = reinterpret_cast<std::uintptr_t>(data.last);
    const auto required = static_cast<std::uintptr_t>(data.point_count) *
        sizeof(CollidePoint);
    if (last < first || last - first < required) return false;
    for (std::int32_t index = 0; index < data.point_count; ++index) {
        CollidePoint point{};
        const auto* source = reinterpret_cast<const std::uint8_t*>(data.first) +
            static_cast<std::size_t>(index) * sizeof(CollidePoint);
        if (!ReadBytes(source, &point, sizeof(point)) ||
            !Finite(point.point) || !Finite(point.normal) ||
            !std::isfinite(point.depth) || point.depth < 0.0F) {
            return false;
        }
        maximum_depth = std::max(maximum_depth, point.depth);
        ++contact_count;
    }
    return true;
}

bool RequestNoWriteHandContactQuery(
    HandContactQueryTelemetry& telemetry,
    std::string& error) noexcept {
    error.clear();
    telemetry = {};

    RequestState state = g_request_state.load(std::memory_order_acquire);
    if (state == RequestState::passed || state == RequestState::failed) {
        static_cast<void>(g_request_state.compare_exchange_strong(
            state, RequestState::idle, std::memory_order_acq_rel));
    }
    RequestState expected = RequestState::idle;
    if (!g_request_state.compare_exchange_strong(
            expected, RequestState::pending, std::memory_order_acq_rel)) {
        error = "A Black Plague no-write hand-contact query is already active";
        return false;
    }

    const ULONGLONG deadline = GetTickCount64() + 5'000;
    do {
        state = g_request_state.load(std::memory_order_acquire);
        if (state == RequestState::passed || state == RequestState::failed) {
            AcquireSRWLockShared(&g_result_lock);
            telemetry = g_result;
            ReleaseSRWLockShared(&g_result_lock);
            g_request_state.store(RequestState::idle, std::memory_order_release);
            if (state == RequestState::passed) return true;
            error = telemetry.result == HandContactQueryResult::native_memory_changed
                ? "The no-write query changed native world/body/shape bytes"
                : telemetry.result == HandContactQueryResult::invalid_contact_data
                    ? "The native collision callback supplied invalid contact data"
                    : "The exact Black Plague hand-contact query boundary was unavailable";
            return false;
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);

    expected = RequestState::pending;
    if (g_request_state.compare_exchange_strong(
            expected, RequestState::idle, std::memory_order_acq_rel)) {
        error = "No current player-body tick serviced the no-write query";
    } else {
        error = "The no-write query timed out while its game-thread state was indeterminate";
    }
    return false;
}

bool NoWriteHandContactQueryPending() noexcept {
    return g_request_state.load(std::memory_order_acquire) ==
        RequestState::pending;
}

void ServiceNoWriteHandContactQuery(
    std::uint8_t* image,
    void* character_body) noexcept {
    RequestState expected = RequestState::pending;
    if (!g_request_state.compare_exchange_strong(
            expected, RequestState::processing, std::memory_order_acq_rel)) {
        return;
    }

    HandContactQueryTelemetry result;
    result.result = HandContactQueryResult::invalid_boundary;
    if (image == nullptr || character_body == nullptr) {
        PublishResult(result, RequestState::failed);
        return;
    }

    void* const physics_body = Read<void*>(
        character_body, kCharacterPhysicsBodyOffset);
    void* const world = Read<void*>(
        character_body, kCharacterPhysicsWorldOffset);
    if (world == nullptr || physics_body == nullptr ||
        Read<void*>(world, 0) != image + kPhysicsWorldVtable ||
        Read<void*>(physics_body, 0) != image + kPhysicsBodyVtable) {
        PublishResult(result, RequestState::failed);
        return;
    }

    void* const shape = Read<void*>(physics_body, kPhysicsBodyShapeOffset);
    Matrix transform{};
    if (shape == nullptr || Read<void*>(shape, 0) !=
            image + kCollideShapeNewtonVtable ||
        Read<void*>(shape, kShapeWorldOffset) != world ||
        !ReadBytes(static_cast<std::uint8_t*>(physics_body) +
            kPhysicsBodyMatrixOffset, &transform, sizeof(transform))) {
        PublishResult(result, RequestState::failed);
        return;
    }

    result.shape_size = {
        Read<float>(shape, 0x04), Read<float>(shape, 0x08),
        Read<float>(shape, 0x0C)};
    result.shape_type = Read<std::int32_t>(shape, kShapeTypeOffset);
    result.shape_user_count = Read<std::int32_t>(
        shape, kShapeUserCountOffset);
    result.requested_position = {
        transform.values[3], transform.values[7], transform.values[11]};
    const Vec3 requested{
        result.requested_position[0], result.requested_position[1],
        result.requested_position[2]};
    if (!Finite(requested) || !std::isfinite(result.shape_size[0]) ||
        !std::isfinite(result.shape_size[1]) ||
        !std::isfinite(result.shape_size[2]) || result.shape_type < 0 ||
        result.shape_type > 7 || result.shape_user_count <= 0) {
        PublishResult(result, RequestState::failed);
        return;
    }

    NativeSnapshot before{};
    NativeSnapshot after{};
    if (!CaptureSnapshot(
            world, character_body, physics_body, shape, before)) {
        PublishResult(result, RequestState::failed);
        return;
    }

    NativeContactCallback callback;
    Vec3 corrected = requested;
    const auto query = reinterpret_cast<CheckShapeWorldCollision>(
        image + kCheckShapeWorldCollision);
    result.native_collided = query(
        world, &corrected, shape, &transform, physics_body,
        false, false, &callback, false, false);
    result.corrected_position = {corrected.x, corrected.y, corrected.z};
    result.callback_invoked = callback.callback_count != 0;
    result.callback_count = callback.callback_count;
    result.contact_count = callback.contact_count;
    result.maximum_contact_depth = callback.maximum_depth;

    if (!CaptureSnapshot(world, character_body, physics_body, shape, after)) {
        PublishResult(result, RequestState::failed);
        return;
    }
    result.native_memory_changed =
        before.world_shape_list != after.world_shape_list ||
        before.character_position != after.character_position ||
        before.body_matrix != after.body_matrix ||
        before.body_shape_pointer != after.body_shape_pointer ||
        before.shape_header != after.shape_header;
    if (result.native_memory_changed) {
        result.result = HandContactQueryResult::native_memory_changed;
        PublishResult(result, RequestState::failed);
        return;
    }
    if (!callback.valid || !Finite(corrected) ||
        result.native_collided != result.callback_invoked) {
        result.result = HandContactQueryResult::invalid_contact_data;
        PublishResult(result, RequestState::failed);
        return;
    }

    result.result = result.native_collided
        ? HandContactQueryResult::collided
        : HandContactQueryResult::clear;
    PublishResult(result, RequestState::passed);
}

bool RequestPalmResolverValidation(
    PalmResolverValidationTelemetry& telemetry,
    std::string& error) noexcept {
    error.clear();
    telemetry = {};

    RequestState state = g_resolver_request_state.load(std::memory_order_acquire);
    if (state == RequestState::passed || state == RequestState::failed) {
        static_cast<void>(g_resolver_request_state.compare_exchange_strong(
            state, RequestState::idle, std::memory_order_acq_rel));
    }
    RequestState expected = RequestState::idle;
    if (!g_resolver_request_state.compare_exchange_strong(
            expected, RequestState::pending, std::memory_order_acq_rel)) {
        error = "A Black Plague palm-resolver validation is already active";
        return false;
    }

    const ULONGLONG deadline = GetTickCount64() + 5'000;
    do {
        state = g_resolver_request_state.load(std::memory_order_acquire);
        if (state == RequestState::passed || state == RequestState::failed) {
            AcquireSRWLockShared(&g_resolver_result_lock);
            telemetry = g_resolver_result;
            ReleaseSRWLockShared(&g_resolver_result_lock);
            g_resolver_request_state.store(RequestState::idle, std::memory_order_release);
            if (state == RequestState::passed) return true;
            switch (telemetry.result) {
            case PalmResolverValidationResult::shape_create_failed:
                error = "Creating the owned Black Plague palm shape failed";
                break;
            case PalmResolverValidationResult::shape_query_failed:
                error = "The owned palm shape query supplied invalid native data";
                break;
            case PalmResolverValidationResult::resolver_failed:
                error = "The Rework-derived palm resolver rejected the controlled sample";
                break;
            case PalmResolverValidationResult::shape_destroy_failed:
                error = "Destroying the owned Black Plague palm shape failed";
                break;
            default:
                error = "The exact Black Plague palm-resolver boundary was unavailable";
                break;
            }
            return false;
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);

    expected = RequestState::pending;
    if (!g_resolver_request_state.compare_exchange_strong(
            expected, RequestState::idle, std::memory_order_acq_rel)) {
        expected = RequestState::processing;
        static_cast<void>(g_resolver_request_state.compare_exchange_strong(
            expected, RequestState::idle, std::memory_order_acq_rel));
    }
    error = "No two current player-body ticks completed the palm-resolver validation";
    return false;
}

bool PalmResolverValidationPending() noexcept {
    const RequestState state =
        g_resolver_request_state.load(std::memory_order_acquire);
    return state == RequestState::pending || state == RequestState::processing;
}

void ServicePalmResolverValidation(
    std::uint8_t* image,
    void* character_body) noexcept {
    RequestState state = g_resolver_request_state.load(std::memory_order_acquire);
    if (state == RequestState::pending) {
        RequestState expected = RequestState::pending;
        if (!g_resolver_request_state.compare_exchange_strong(
                expected, RequestState::processing, std::memory_order_acq_rel)) {
            return;
        }
        g_resolver_working = {};
        g_resolver_working.result = PalmResolverValidationResult::invalid_boundary;
        runtime::ResetVrHandResolveState(g_resolver_state);
        g_resolver_first_tick_complete = false;
    } else if (state != RequestState::processing) {
        return;
    }

    auto fail = [&](PalmResolverValidationResult result) noexcept {
        g_resolver_working.result = result;
        if (!SafeDestroyOwnedPalmShape(
                g_validation_palm_shape, &g_resolver_working) &&
            result != PalmResolverValidationResult::shape_destroy_failed) {
            g_resolver_working.result =
                PalmResolverValidationResult::shape_destroy_failed;
        }
        runtime::ResetVrHandResolveState(g_resolver_state);
        g_resolver_first_tick_complete = false;
        PublishResolverResult(g_resolver_working, RequestState::failed);
    };

    if (image == nullptr || character_body == nullptr) {
        fail(PalmResolverValidationResult::invalid_boundary);
        return;
    }
    void* const physics_body = Read<void*>(
        character_body, kCharacterPhysicsBodyOffset);
    void* const world = Read<void*>(
        character_body, kCharacterPhysicsWorldOffset);
    if (physics_body == nullptr || !ValidWorld(image, world) ||
        Read<void*>(physics_body, 0) != image + kPhysicsBodyVtable) {
        fail(PalmResolverValidationResult::invalid_boundary);
        return;
    }

    const bool had_shape = g_validation_palm_shape.shape != nullptr;
    const void* previous_world = g_validation_palm_shape.world;
    if (!EnsureOwnedPalmShape(
            image, world, g_validation_palm_shape, g_resolver_working)) {
        fail(PalmResolverValidationResult::shape_create_failed);
        return;
    }
    if (had_shape && previous_world == world) {
        g_resolver_working.shape_reused = true;
    }
    if (had_shape && previous_world != nullptr && previous_world != world) {
        runtime::ResetVrHandResolveState(g_resolver_state);
    }

    Matrix body_transform{};
    if (!ReadBytes(static_cast<std::uint8_t*>(physics_body) +
            kPhysicsBodyMatrixOffset, &body_transform, sizeof(body_transform))) {
        fail(PalmResolverValidationResult::invalid_boundary);
        return;
    }
    runtime::VrMatrix44 raw_pose{};
    raw_pose.values = body_transform.values;
    if (g_resolver_first_tick_complete) {
        // Controlled 1 cm translation exercises the sweep path while remaining
        // far below the palm's blocking span. It is diagnostic storage only.
        raw_pose.values[3] += 0.01F;
    }

    GameplaySnapshot before{};
    GameplaySnapshot after{};
    if (!CaptureGameplaySnapshot(character_body, physics_body, before)) {
        fail(PalmResolverValidationResult::invalid_boundary);
        return;
    }

    NativePalmQueryContext query_context{
        image, world, g_validation_palm_shape.shape, physics_body,
        &g_resolver_working, false};
    runtime::VrHandResolverFrame frame;
    runtime::VrMatrix44 resolved{};
    const bool resolved_ok = runtime::ResolveVrHandPose(g_resolver_state,
        raw_pose, frame, &query_context, QueryOwnedPalmShape, resolved);
    if (!CaptureGameplaySnapshot(character_body, physics_body, after)) {
        fail(PalmResolverValidationResult::invalid_boundary);
        return;
    }
    if (before.character_position != after.character_position ||
        before.body_matrix != after.body_matrix ||
        before.body_shape_pointer != after.body_shape_pointer) {
        g_resolver_working.gameplay_memory_changed = true;
        fail(PalmResolverValidationResult::resolver_failed);
        return;
    }
    if (!resolved_ok) {
        fail(query_context.failed
            ? PalmResolverValidationResult::shape_query_failed
            : PalmResolverValidationResult::resolver_failed);
        return;
    }

    const std::array<float, 3> raw_position{
        raw_pose.values[3], raw_pose.values[7], raw_pose.values[11]};
    const std::array<float, 3> resolved_position{
        resolved.values[3], resolved.values[7], resolved.values[11]};
    if (!g_resolver_first_tick_complete) {
        g_resolver_working.first_raw_position = raw_position;
        g_resolver_working.first_resolved_position = resolved_position;
        g_resolver_first_tick_complete = true;
        return;
    }

    g_resolver_working.second_raw_position = raw_position;
    g_resolver_working.second_resolved_position = resolved_position;
    if (!SafeDestroyOwnedPalmShape(
            g_validation_palm_shape, &g_resolver_working)) {
        fail(PalmResolverValidationResult::shape_destroy_failed);
        return;
    }
    g_resolver_working.result = PalmResolverValidationResult::passed;
    runtime::ResetVrHandResolveState(g_resolver_state);
    g_resolver_first_tick_complete = false;
    PublishResolverResult(g_resolver_working, RequestState::passed);
}

void PublishGameplayPalmTracking(
    const std::array<runtime::VrMatrix44, 2>& raw_poses,
    const std::array<bool, 2>& raw_valid,
    const runtime::VrMatrix44& head_pose,
    bool head_valid,
    std::uint64_t yaw_epoch) noexcept {
    bool yaw_epoch_changed = false;
    AcquireSRWLockExclusive(&g_gameplay_tracking_lock);
    yaw_epoch_changed = g_gameplay_tracking_yaw_epoch != 0 &&
        yaw_epoch != 0 && yaw_epoch != g_gameplay_tracking_yaw_epoch;
    g_gameplay_raw_poses = raw_poses;
    g_gameplay_raw_valid = raw_valid;
    g_gameplay_head_pose = head_pose;
    g_gameplay_head_valid = head_valid;
    g_gameplay_tracking_yaw_epoch = yaw_epoch;
    g_gameplay_tracking_time = GetTickCount64();
    ++g_gameplay_tracking_generation;
    ReleaseSRWLockExclusive(&g_gameplay_tracking_lock);
    // A world-yaw turn rotates the raw palms discontinuously in world space.
    // Do not render a still-valid resolved pose from the previous yaw epoch
    // while the game-thread resolver is waiting for its next service tick.
    if (yaw_epoch_changed) InvalidateGameplayPalmPoses();
}

void PublishGameplayPalmHeldBody(
    std::size_t hand_index,
    void* body) noexcept {
    if (hand_index >= g_gameplay_held_body.size()) return;
    g_gameplay_held_body[hand_index].store(body, std::memory_order_release);
}

void SetGameplayInteractionTargetProvider(
    GameplayInteractionTargetProvider provider) noexcept {
    g_gameplay_interaction_target_provider.store(provider, std::memory_order_release);
}

void ServiceGameplayPalmResolver(
    std::uint8_t* image,
    void* character_body) noexcept {
    if (g_gameplay_shutdown_requested.exchange(
            false, std::memory_order_acq_rel)) {
        PalmResolverValidationTelemetry lifecycle{};
        const bool interaction_destroyed = SafeDestroyOwnedPalmShape(
            g_gameplay_interaction_shape, &lifecycle);
        const bool collision_destroyed = SafeDestroyOwnedPalmShape(
            g_gameplay_palm_shape, &lifecycle);
        const bool destroyed = interaction_destroyed && collision_destroyed;
        g_gameplay_shape_active.store(false, std::memory_order_release);
        ResetGameplayPalmStates();
        g_gameplay_resolver_yaw_epoch = 0;
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        g_gameplay_telemetry.enabled = false;
        g_gameplay_telemetry.shape_destroys += lifecycle.destroy_count;
        if (!destroyed) ++g_gameplay_telemetry.query_failures;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
        g_gameplay_request_source.store(
            GameplayPalmResolverRequestSource::disabled,
            std::memory_order_release);
        g_gameplay_request_sampled = true;
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        g_gameplay_telemetry.source = GameplayPalmResolverRequestSource::disabled;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
        g_gameplay_shutdown_succeeded.store(destroyed, std::memory_order_release);
        g_gameplay_shutdown_complete.store(true, std::memory_order_release);
        return;
    }

    if (!g_gameplay_request_sampled) {
        g_gameplay_request_source.store(
            GameplayPalmResolverRequested(), std::memory_order_release);
        g_gameplay_request_sampled = true;
        const auto source =
            g_gameplay_request_source.load(std::memory_order_acquire);
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        g_gameplay_telemetry.enabled =
            source != GameplayPalmResolverRequestSource::disabled;
        g_gameplay_telemetry.source = source;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
    }
    if (g_gameplay_request_source.load(std::memory_order_acquire) ==
        GameplayPalmResolverRequestSource::disabled) {
        return;
    }
    if (image == nullptr || character_body == nullptr) return;

    std::array<runtime::VrMatrix44, 2> raw_poses{};
    std::array<bool, 2> raw_valid{};
    runtime::VrMatrix44 head_pose{};
    bool head_valid = false;
    std::uint64_t tracking_time = 0;
    std::uint64_t tracking_yaw_epoch = 0;
    std::uint64_t tracking_generation = 0;
    AcquireSRWLockShared(&g_gameplay_tracking_lock);
    raw_poses = g_gameplay_raw_poses;
    raw_valid = g_gameplay_raw_valid;
    head_pose = g_gameplay_head_pose;
    head_valid = g_gameplay_head_valid;
    tracking_time = g_gameplay_tracking_time;
    tracking_yaw_epoch = g_gameplay_tracking_yaw_epoch;
    tracking_generation = g_gameplay_tracking_generation;
    ReleaseSRWLockShared(&g_gameplay_tracking_lock);

    const std::uint64_t now = GetTickCount64();
    if (tracking_time == 0 || now - tracking_time >
            kGameplayPalmSampleMaximumAgeMilliseconds ||
        (!raw_valid[0] && !raw_valid[1])) {
        InvalidateGameplayPalmPoses();
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        ++g_gameplay_telemetry.stale_tracking_samples;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
        return;
    }

    void* const physics_body = Read<void*>(
        character_body, kCharacterPhysicsBodyOffset);
    void* const world = Read<void*>(
        character_body, kCharacterPhysicsWorldOffset);
    if (physics_body == nullptr || !ValidWorld(image, world) ||
        Read<void*>(physics_body, 0) != image + kPhysicsBodyVtable) {
        InvalidateGameplayPalmPoses();
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        ++g_gameplay_telemetry.query_failures;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
        return;
    }

    const void* previous_world = g_gameplay_palm_shape.world;
    PalmResolverValidationTelemetry lifecycle{};
    if (!EnsureOwnedPalmShape(
            image, world, g_gameplay_palm_shape, lifecycle)) {
        g_gameplay_shape_active.store(false, std::memory_order_release);
        InvalidateGameplayPalmPoses();
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        ++g_gameplay_telemetry.query_failures;
        g_gameplay_telemetry.shape_destroys += lifecycle.destroy_count;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
        return;
    }
    g_gameplay_shape_active.store(true, std::memory_order_release);
    const bool interaction_shape_ready = EnsureOwnedInteractionShape(
        image, world, g_gameplay_interaction_shape, lifecycle);
    if (previous_world != nullptr && previous_world != world) {
        ResetGameplayPalmStates();
    }
    const bool yaw_epoch_changed = g_gameplay_resolver_yaw_epoch != 0 &&
        tracking_yaw_epoch != 0 &&
        tracking_yaw_epoch != g_gameplay_resolver_yaw_epoch;
    if (yaw_epoch_changed) {
        // The controller did not teleport; the presentation world rotated.
        // Reset world-space history so ResolveVrHandPose does not classify the
        // turn as a tracking reanchor/recovery event.
        ResetGameplayPalmStates();
        AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
        ++g_gameplay_telemetry.yaw_epoch_resets;
        ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
    }
    if (tracking_yaw_epoch != 0) {
        g_gameplay_resolver_yaw_epoch = tracking_yaw_epoch;
    }

    std::array<runtime::VrMatrix44, 2> resolved_poses{};
    std::array<bool, 2> resolved_valid{};
    std::array<bool, 2> resolved_changed{};
    std::array<runtime::VrMatrix44, 2> previous_resolved_poses{};
    std::array<bool, 2> previous_resolved_valid{};
    std::uint64_t previous_resolved_time = 0;
    AcquireSRWLockShared(&g_gameplay_pose_lock);
    previous_resolved_poses = g_gameplay_resolved_poses;
    previous_resolved_valid = g_gameplay_resolved_valid;
    previous_resolved_time = g_gameplay_resolved_time;
    ReleaseSRWLockShared(&g_gameplay_pose_lock);
    std::uint64_t published = 0;
    std::uint64_t queries = 0;
    std::uint64_t contacts = 0;
    std::uint64_t constrained = 0;
    std::uint64_t tracking_reanchors = 0;
    std::uint64_t recovery_anchors = 0;
    std::uint64_t pullback_recoveries = 0;
    std::uint64_t interaction_assist_samples = 0;
    std::uint64_t held_skips = 0;
    std::uint64_t failures = interaction_shape_ready ? 0U : 1U;

    for (std::size_t hand = 0; hand < raw_poses.size(); ++hand) {
        if (!raw_valid[hand]) continue;
        void* const skip_body =
            g_gameplay_held_body[hand].load(std::memory_order_acquire);
        if (tracking_generation != 0 &&
            g_gameplay_resolved_tracking_generation[hand] == tracking_generation &&
            g_gameplay_resolved_skip_body[hand] == skip_body &&
            previous_resolved_valid[hand] && previous_resolved_time != 0 &&
            now >= previous_resolved_time &&
            now - previous_resolved_time <= kGameplayPalmSampleMaximumAgeMilliseconds) {
            resolved_poses[hand] = previous_resolved_poses[hand];
            resolved_valid[hand] = true;
            continue;
        }
        if (g_gameplay_resolved_tracking_generation[hand] != 0 &&
            g_gameplay_resolved_skip_body[hand] != skip_body) {
            // The collision domain changed even if the controller pose did
            // not. ResolveVrHandPose intentionally reuses an identical raw
            // pose, so reset this hand's history to force a new overlap solve
            // against the updated held-body exclusion.
            runtime::ResetVrHandResolveState(g_gameplay_resolver_state[hand]);
        }
        PalmResolverValidationTelemetry query_telemetry{};
        NativePalmQueryContext query_context{
            image, world, g_gameplay_palm_shape.shape, skip_body,
            &query_telemetry, false};
        auto frame = GameplayResolverFrame(head_pose, head_valid, hand == 0, hand);
        runtime::VrMatrix44 resolved{};
        const bool ok = runtime::ResolveVrHandPose(
            g_gameplay_resolver_state[hand], raw_poses[hand], frame,
            &query_context, QueryOwnedPalmShape, resolved);
        if (g_gameplay_resolver_state[hand].last_interaction_assist) {
            ++interaction_assist_samples;
        }
        queries += query_telemetry.query_count;
        contacts += query_telemetry.contact_count;
        if (skip_body != nullptr) ++held_skips;
        if (!ok) {
            ++failures;
            if (!g_gameplay_resolver_state[hand].valid) continue;
        }
        resolved_poses[hand] = resolved;
        resolved_valid[hand] = true;
        resolved_changed[hand] = true;
        g_gameplay_resolved_tracking_generation[hand] = tracking_generation;
        g_gameplay_resolved_skip_body[hand] = skip_body;
        ++published;
        const float dx = raw_poses[hand].values[3] - resolved.values[3];
        const float dy = raw_poses[hand].values[7] - resolved.values[7];
        const float dz = raw_poses[hand].values[11] - resolved.values[11];
        if (g_gameplay_resolver_state[hand].constrained_frames > 0 ||
            std::hypot(std::hypot(dx, dy), dz) > 0.001F) {
            ++constrained;
        }
        if (g_gameplay_resolver_state[hand].last_tracking_reanchor) {
            ++tracking_reanchors;
        }
        if (g_gameplay_resolver_state[hand].last_recovery_anchor) {
            ++recovery_anchors;
        }
        if (g_gameplay_resolver_state[hand].last_pullback_recovery) {
            ++pullback_recoveries;
        }
    }

    AcquireSRWLockExclusive(&g_gameplay_pose_lock);
    g_gameplay_resolved_poses = resolved_poses;
    g_gameplay_resolved_valid = resolved_valid;
    for (std::size_t hand = 0; hand < resolved_valid.size(); ++hand) {
        if (resolved_changed[hand]) ++g_gameplay_resolved_generation[hand];
    }
    const bool any_resolved = resolved_valid[0] || resolved_valid[1];
    g_gameplay_resolved_time = !any_resolved ? 0 :
        (published != 0 ? now : previous_resolved_time);
    ReleaseSRWLockExclusive(&g_gameplay_pose_lock);

    AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
    ++g_gameplay_telemetry.samples;
    g_gameplay_telemetry.published_poses += published;
    g_gameplay_telemetry.queries += queries;
    g_gameplay_telemetry.contacts += contacts;
    g_gameplay_telemetry.constrained_samples += constrained;
    g_gameplay_telemetry.tracking_reanchors += tracking_reanchors;
    g_gameplay_telemetry.recovery_anchors += recovery_anchors;
    g_gameplay_telemetry.pullback_recoveries += pullback_recoveries;
    g_gameplay_telemetry.interaction_assist_samples += interaction_assist_samples;
    g_gameplay_telemetry.held_body_skips += held_skips;
    g_gameplay_telemetry.query_failures += failures;
    g_gameplay_telemetry.shape_creates += lifecycle.create_count;
    g_gameplay_telemetry.shape_destroys += lifecycle.destroy_count;
    g_gameplay_telemetry.world_replacements += lifecycle.world_replaced ? 1U : 0U;
    ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
}

bool ReadGameplayPalmPose(
    std::size_t hand_index,
    runtime::VrMatrix44& pose) noexcept {
    if (hand_index >= g_gameplay_resolved_poses.size()) return false;
    AcquireSRWLockShared(&g_gameplay_pose_lock);
    pose = g_gameplay_resolved_poses[hand_index];
    const bool valid = g_gameplay_resolved_valid[hand_index];
    const std::uint64_t time = g_gameplay_resolved_time;
    ReleaseSRWLockShared(&g_gameplay_pose_lock);
    return valid && time != 0 &&
        GetTickCount64() - time <= kGameplayPalmSampleMaximumAgeMilliseconds;
}

std::uint64_t GameplayPalmPoseGeneration(std::size_t hand_index) noexcept {
    if (hand_index >= g_gameplay_resolved_generation.size()) return 0;
    AcquireSRWLockShared(&g_gameplay_pose_lock);
    const std::uint64_t generation =
        g_gameplay_resolved_generation[hand_index];
    ReleaseSRWLockShared(&g_gameplay_pose_lock);
    return generation;
}

bool QueryGameplayPalmOverlaps(
    std::size_t hand_index,
    const runtime::VrMatrix44& pose,
    GameplayPalmOverlapResult& result) noexcept {
    result = {};
    if (hand_index >= g_gameplay_held_body.size() ||
        !g_gameplay_shape_active.load(std::memory_order_acquire)) return false;
    const OwnedPalmShape owned = g_gameplay_interaction_shape;
    if (owned.image == nullptr || owned.world == nullptr || owned.shape == nullptr ||
        !ValidWorld(owned.image, owned.world) ||
        Read<void*>(owned.shape, 0) != owned.image + kCollideShapeNewtonVtable ||
        Read<void*>(owned.shape, kShapeWorldOffset) != owned.world) return false;

    Matrix transform{};
    transform.values = pose.values;
    const Vec3 requested{
        transform.values[3], transform.values[7], transform.values[11]};
    if (!Finite(requested)) return false;
    Vec3 corrected = requested;
    GameplayPalmOverlapCallback callback;
    bool collided = false;
    void* const skip_body =
        g_gameplay_held_body[hand_index].load(std::memory_order_acquire);
    __try {
        const auto query = reinterpret_cast<CheckShapeWorldCollision>(
            owned.image + kCheckShapeWorldCollision);
        collided = query(owned.world, &corrected, owned.shape, &transform,
            skip_body, false, false, &callback, false, false);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!callback.valid || !Finite(corrected) ||
        collided != (callback.result.hit_count != 0)) return false;
    callback.result.valid = true;
    result = callback.result;
    return true;
}

GameplayPalmResolverTelemetry ConsumeGameplayPalmResolverTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_gameplay_telemetry_lock);
    GameplayPalmResolverTelemetry result = g_gameplay_telemetry;
    g_gameplay_telemetry = {};
    const auto source =
        g_gameplay_request_source.load(std::memory_order_acquire);
    g_gameplay_telemetry.enabled =
        source != GameplayPalmResolverRequestSource::disabled;
    g_gameplay_telemetry.source = source;
    ReleaseSRWLockExclusive(&g_gameplay_telemetry_lock);
    return result;
}

bool ShutdownGameplayPalmResolver(std::string& error) noexcept {
    error.clear();
    InvalidateGameplayPalmPoses();
    if (!g_gameplay_shape_active.load(std::memory_order_acquire)) {
        return true;
    }
    g_gameplay_shutdown_complete.store(false, std::memory_order_release);
    g_gameplay_shutdown_succeeded.store(false, std::memory_order_release);
    g_gameplay_shutdown_requested.store(true, std::memory_order_release);
    const std::uint64_t deadline = GetTickCount64() + 1000;
    do {
        if (g_gameplay_shutdown_complete.load(std::memory_order_acquire)) {
            if (g_gameplay_shutdown_succeeded.load(std::memory_order_acquire)) {
                return true;
            }
            error = "The gameplay palm shape could not be destroyed on the game thread";
            return false;
        }
        Sleep(1);
    } while (GetTickCount64() < deadline);
    error = "The gameplay palm shape did not reach a current player-body tick for safe destruction";
    return false;
}

} // namespace penumbra_vr::backends::black_plague
