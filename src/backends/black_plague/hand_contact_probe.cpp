#include "hand_contact_probe.hpp"
#include "vr_hand_contact.hpp"
#include "vr_interaction_policy.hpp"

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

OwnedPalmShape g_owned_palm_shape;
runtime::VrHandResolveState g_resolver_state;
bool g_resolver_first_tick_complete = false;

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
    PalmResolverValidationTelemetry* telemetry) noexcept {
    if (g_owned_palm_shape.shape == nullptr) return true;
    const auto owner = g_owned_palm_shape;
    g_owned_palm_shape = {};
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

[[nodiscard]] bool EnsureOwnedPalmShape(
    std::uint8_t* image,
    void* world,
    PalmResolverValidationTelemetry& telemetry) noexcept {
    if (!ValidWorld(image, world)) return false;
    if (g_owned_palm_shape.shape != nullptr &&
        g_owned_palm_shape.image == image &&
        g_owned_palm_shape.world == world) {
        telemetry.shape_reused = true;
        return true;
    }
    if (g_owned_palm_shape.shape != nullptr) {
        telemetry.world_replaced = true;
        if (!SafeDestroyOwnedPalmShape(&telemetry)) return false;
    }

    using namespace runtime::vr_interaction_policy;
    const Vec3 size{kCollisionSizeX, kCollisionSizeY, kCollisionSizeZ};
    void* shape = nullptr;
    __try {
        const auto create = reinterpret_cast<CreateBoxShape>(image + kCreateBoxShape);
        shape = create(world, &size, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        shape = nullptr;
    }
    if (shape == nullptr ||
        Read<void*>(shape, 0) != image + kCollideShapeNewtonVtable ||
        Read<void*>(shape, kShapeWorldOffset) != world ||
        Read<std::int32_t>(shape, kShapeUserCountOffset) <= 0) {
        if (shape != nullptr) {
            g_owned_palm_shape = {image, world, shape};
            static_cast<void>(SafeDestroyOwnedPalmShape(&telemetry));
        }
        return false;
    }
    g_owned_palm_shape = {image, world, shape};
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
        if (!SafeDestroyOwnedPalmShape(&g_resolver_working) &&
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

    const bool had_shape = g_owned_palm_shape.shape != nullptr;
    const void* previous_world = g_owned_palm_shape.world;
    if (!EnsureOwnedPalmShape(image, world, g_resolver_working)) {
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
        image, world, g_owned_palm_shape.shape, physics_body,
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
    if (!SafeDestroyOwnedPalmShape(&g_resolver_working)) {
        fail(PalmResolverValidationResult::shape_destroy_failed);
        return;
    }
    g_resolver_working.result = PalmResolverValidationResult::passed;
    runtime::ResetVrHandResolveState(g_resolver_state);
    g_resolver_first_tick_complete = false;
    PublishResolverResult(g_resolver_working, RequestState::passed);
}

} // namespace penumbra_vr::backends::black_plague
