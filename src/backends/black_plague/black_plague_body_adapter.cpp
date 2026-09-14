#include "black_plague_body_adapter.hpp"
#include "body_adapter_boundary.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

using Move = void(__thiscall*)(void*, float, float);

constexpr std::uintptr_t kPlayerCharacterBodyOffset = 0x274;
constexpr std::uintptr_t kMoveForward = 0x9CBC0;
constexpr std::uintptr_t kMoveSideways = 0x9CC60;
constexpr wchar_t kShadowRequestMutexName[] =
    L"Local\\PenumbraVR.BlackPlague.ReconciliationShadow";
constexpr wchar_t kPhysicalValidationRequestMutexName[] =
    L"Local\\PenumbraVR.BlackPlague.PhysicalDisplacementValidation";
constexpr wchar_t kRoomScaleRequestMutexName[] =
    L"Local\\PenumbraVR.BlackPlague.RoomScaleValidation";
constexpr std::uint64_t kRoomScaleSampleMaximumAgeMilliseconds = 250;
constexpr std::uint64_t kDirectLocomotionIntentMaximumAgeMilliseconds = 250;

std::atomic<std::uint8_t*> g_image{nullptr};
std::atomic<bool> g_installed{false};
SRWLOCK g_lock = SRWLOCK_INIT;
BlackPlagueBodyMotion g_motion;
bool g_shadow_enabled = false;
BlackPlagueShadowRequestSource g_shadow_source =
    BlackPlagueShadowRequestSource::disabled;
BodyReconciliationShadow g_shadow;
BlackPlagueShadowTelemetry g_shadow_telemetry;
runtime::VrMatrix34 g_shadow_pose;
runtime::VrTrackingSampleIdentity g_shadow_tracking_identity;
float g_shadow_yaw = 0.0F;
std::uint64_t g_shadow_tracking_time = 0;
std::uint64_t g_shadow_body_time = 0;
void* g_shadow_body_identity = nullptr; // Comparison only; never dereferenced.
std::uint64_t g_shadow_generation = 0;
bool g_physical_validation_enabled = false;
BlackPlaguePhysicalValidationRequestSource g_physical_validation_source =
    BlackPlaguePhysicalValidationRequestSource::disabled;
BlackPlaguePhysicalValidationTelemetry g_physical_validation_telemetry;
bool g_room_scale_enabled = false;
BlackPlagueRoomScaleRequestSource g_room_scale_source =
    BlackPlagueRoomScaleRequestSource::disabled;
BlackPlagueRoomScaleCameraSample g_room_scale_camera_sample;
std::uint64_t g_room_scale_camera_sample_time = 0;

struct PendingPhysicalValidation {
    bool active = false;
    void* character_body = nullptr;
    std::uint64_t generation = 0;
    std::uint64_t tick_sequence = 0;
    bool physical_request_queued = false;
    BodyReconciliationTickPlan tick{};
};

PendingPhysicalValidation g_pending_physical_validation;

struct DirectLocomotionIntentState {
    bool valid = false;
    void* player = nullptr; // Identity only; never dereferenced through this field.
    BlackPlagueDirectLocomotionIntent intent{};
    std::uint64_t published_time = 0;
};

DirectLocomotionIntentState g_direct_locomotion_intent;

[[nodiscard]] bool ReadBytes(const void* source, void* destination,
    std::size_t size) noexcept {
    if (source == nullptr || destination == nullptr) return false;
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<class T>
[[nodiscard]] T Read(const void* object, std::uintptr_t offset) noexcept {
    T value{};
    if (object != nullptr) static_cast<void>(ReadBytes(
        static_cast<const std::uint8_t*>(object) + offset, &value, sizeof(value)));
    return value;
}

[[nodiscard]] bool MovementOwnerReady(std::string& error) noexcept {
    const auto status = ReadNativeMovementBoundaryStatus();
    if (status.initialized) return true;
    const auto& failed = !status.callsites[0].owner_matches_live
        ? status.callsites[0] : status.callsites[1];
    const BodyAdapterCallsiteBoundary boundary{
        failed.rva == 0x51CD ? "MoveForward" : "MoveSideways", failed.rva,
        failed.expected, failed.live, failed.expected_target, failed.live_target,
        failed.owner_installed, failed.owner_matches_live};
    return ValidateBodyAdapterBoundary(boundary, "native-input", error);
}

[[nodiscard]] bool BodyUpdateOwnerReady(std::string& error) noexcept {
    const auto status = ReadNativeBodyUpdateBoundaryStatus();
    if (status.initialized) return true;
    const BodyAdapterCallsiteBoundary boundary{"D460A", 0xD460A,
        status.expected, status.live, status.expected_target, status.live_target,
        status.owner_installed, status.owner_matches_live};
    return ValidateBodyAdapterBoundary(boundary, "body-probe", error);
}

[[nodiscard]] bool ValidIntent(float amount, float delta_seconds) noexcept {
    return std::isfinite(amount) && std::isfinite(delta_seconds) &&
        delta_seconds >= 0.0F && delta_seconds <= 0.25F;
}

[[nodiscard]] bool FiniteVector(const std::array<float, 3>& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

[[nodiscard]] bool FiniteMatrix(const std::array<float, 16>& value) noexcept {
    for (const float component : value) {
        if (!std::isfinite(component)) return false;
    }
    return true;
}

void InvalidateDirectLocomotionIntentLocked() noexcept {
    g_direct_locomotion_intent = {};
}

[[nodiscard]] bool SameVector(const std::array<float, 3>& left,
    const std::array<float, 3>& right) noexcept {
    constexpr float epsilon = 0.00001F;
    return std::abs(left[0] - right[0]) <= epsilon &&
        std::abs(left[1] - right[1]) <= epsilon &&
        std::abs(left[2] - right[2]) <= epsilon;
}

[[nodiscard]] bool HasHorizontalRequest(
    const std::array<float, 3>& request) noexcept {
    return std::hypot(request[0], request[2]) > 0.0F;
}

[[nodiscard]] bool MatchesCurrentBody(void* player) noexcept {
    return player != nullptr && Read<void*>(player, kPlayerCharacterBodyOffset) != nullptr;
}

[[nodiscard]] BlackPlagueShadowRequestSource ReconciliationShadowRequested() noexcept {
    char shadow_option[2]{};
    if (GetEnvironmentVariableA("PVR_BP_RECONCILIATION_SHADOW",
            shadow_option, 2) == 1 && shadow_option[0] == '1') {
        return BlackPlagueShadowRequestSource::environment;
    }

    // Steam owns the game process when --launch-vr uses steam://, so a variable
    // set only in the launcher shell is not a reliable child-process signal if
    // Steam was already running. The diagnostic launcher therefore holds this
    // per-session named mutex only until probe initialization completes. The
    // adapter samples it once here, preserving the existing default-off and
    // no-persistent-setting semantics.
    HANDLE request = OpenMutexW(SYNCHRONIZE, FALSE, kShadowRequestMutexName);
    if (request == nullptr) return BlackPlagueShadowRequestSource::disabled;
    CloseHandle(request);
    return BlackPlagueShadowRequestSource::mutex;
}

[[nodiscard]] BlackPlaguePhysicalValidationRequestSource
PhysicalValidationRequested() noexcept {
    char option[2]{};
    if (GetEnvironmentVariableA("PVR_BP_PHYSICAL_DISPLACEMENT_VALIDATION",
            option, 2) == 1 && option[0] == '1') {
        return BlackPlaguePhysicalValidationRequestSource::environment;
    }
    HANDLE request = OpenMutexW(
        SYNCHRONIZE, FALSE, kPhysicalValidationRequestMutexName);
    if (request == nullptr)
        return BlackPlaguePhysicalValidationRequestSource::disabled;
    CloseHandle(request);
    return BlackPlaguePhysicalValidationRequestSource::mutex;
}

[[nodiscard]] BlackPlagueRoomScaleRequestSource RoomScaleRequested() noexcept {
    char option[2]{};
    if (GetEnvironmentVariableA("PVR_BP_ROOM_SCALE_VALIDATION",
            option, 2) == 1 && option[0] == '1') {
        return BlackPlagueRoomScaleRequestSource::environment;
    }
    HANDLE request = OpenMutexW(SYNCHRONIZE, FALSE, kRoomScaleRequestMutexName);
    if (request == nullptr) return BlackPlagueRoomScaleRequestSource::disabled;
    CloseHandle(request);
    return BlackPlagueRoomScaleRequestSource::mutex;
}

void InvalidateRoomScaleCameraSampleLocked() noexcept {
    g_room_scale_camera_sample = {};
    g_room_scale_camera_sample.enabled = g_room_scale_enabled;
    g_room_scale_camera_sample_time = 0;
}

void PublishRoomScaleCameraSampleLocked(
    const BodyReconciliationShadowSample& shadow_sample,
    const BodyReconciliationTickPlan& tick,
    std::uint64_t now) noexcept {
    InvalidateRoomScaleCameraSampleLocked();
    if (!g_room_scale_enabled || !shadow_sample.valid) return;
    const std::array<float, 3> offset{
        shadow_sample.predicted_anchor[0] -
            shadow_sample.native_motion.body_after[0],
        0.0F,
        shadow_sample.predicted_anchor[2] -
            shadow_sample.native_motion.body_after[2],
    };
    if (!FiniteVector(offset) || std::hypot(offset[0], offset[2]) >
            runtime::vr_locomotion_policy::kMaximumHeadBodySeparation) {
        return;
    }
    g_room_scale_camera_sample.valid = true;
    g_room_scale_camera_sample.body_generation = tick.body_generation;
    g_room_scale_camera_sample.horizontal_world_offset = offset;
    g_room_scale_camera_sample.predicted_head_anchor =
        shadow_sample.predicted_anchor;
    g_room_scale_camera_sample.body_position =
        shadow_sample.native_motion.body_after;
    g_room_scale_camera_sample.observed_tracking_pose = tick.tracking_pose;
    g_room_scale_camera_sample.tracking_identity = tick.tracking_identity;
    g_room_scale_camera_sample_time = now;
}

void InvalidatePendingPhysicalValidationLocked() noexcept {
    if (g_pending_physical_validation.active) {
        ++g_physical_validation_telemetry.invalidated_plans;
        g_physical_validation_telemetry.latest_result =
            BlackPlaguePhysicalValidationResult::invalidated;
    }
    g_pending_physical_validation = {};
    g_physical_validation_telemetry.pending = false;
    g_physical_validation_telemetry.expected_character_body = 0;
    g_physical_validation_telemetry.expected_generation = 0;
    g_physical_validation_telemetry.requested_displacement = {};
    InvalidatePhysicalBodyDisplacement();
}

[[nodiscard]] bool InstallForImage(std::uint8_t* image,
    std::string& error) noexcept {
    error.clear();
    if (g_installed.load(std::memory_order_acquire)) return true;
    if (image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }
    g_image.store(image, std::memory_order_release);
    // Input and physics hooks own their respective instructions. Each has
    // already validated pristine exact-build bytes before patching; bind only
    // to their live replacement and never attempt a competing patch here.
    if (!MovementOwnerReady(error) || !BodyUpdateOwnerReady(error)) {
        g_image.store(nullptr, std::memory_order_release);
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    g_motion = {};
    g_physical_validation_source = PhysicalValidationRequested();
    g_physical_validation_enabled = g_physical_validation_source !=
        BlackPlaguePhysicalValidationRequestSource::disabled;
    g_room_scale_source = RoomScaleRequested();
    g_room_scale_enabled =
        g_room_scale_source != BlackPlagueRoomScaleRequestSource::disabled &&
        g_physical_validation_enabled;
    if (!g_room_scale_enabled) {
        g_room_scale_source = BlackPlagueRoomScaleRequestSource::disabled;
    }
    g_shadow_source = ReconciliationShadowRequested();
    g_shadow_enabled = g_shadow_source != BlackPlagueShadowRequestSource::disabled ||
        g_physical_validation_enabled;
    if (g_shadow_source == BlackPlagueShadowRequestSource::disabled &&
        g_physical_validation_enabled) {
        g_shadow_source = BlackPlagueShadowRequestSource::physical_validation;
    }
    g_shadow = {};
    g_shadow_telemetry = {};
    g_shadow_tracking_time = 0;
    g_shadow_tracking_identity = {};
    g_shadow_body_time = 0;
    g_shadow_body_identity = nullptr;
    g_shadow_generation = 0;
    g_physical_validation_telemetry = {};
    g_pending_physical_validation = {};
    InvalidateDirectLocomotionIntentLocked();
    InvalidateRoomScaleCameraSampleLocked();
    ReleaseSRWLockExclusive(&g_lock);
    g_installed.store(true, std::memory_order_release);
    return true;
}

[[nodiscard]] bool Publish(void* player, float amount, float delta_seconds,
    bool sideways) noexcept {
    if (!g_installed.load(std::memory_order_acquire) ||
        !MatchesCurrentBody(player) ||
        !ValidIntent(amount, delta_seconds)) return false;
    auto* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr) return false;
    reinterpret_cast<Move>(image + (sideways ? kMoveSideways : kMoveForward))(
        player, amount, delta_seconds);
    AcquireSRWLockExclusive(&g_lock);
    if (g_installed.load(std::memory_order_acquire)) {
        g_motion.intent_published = true;
        g_motion.native_horizontal_intent[sideways ? 1U : 0U] = amount;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

} // namespace

bool InstallBlackPlagueBodyAdapter(std::string& error) noexcept {
    return InstallForImage(reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr)), error);
}

bool RemoveBlackPlagueBodyAdapter(std::string& error) noexcept {
    error.clear();
    g_installed.store(false, std::memory_order_release);
    g_image.store(nullptr, std::memory_order_release);
    AcquireSRWLockExclusive(&g_lock);
    g_motion = {};
    g_shadow_enabled = false;
    g_shadow_source = BlackPlagueShadowRequestSource::disabled;
    g_shadow = {};
    g_shadow_telemetry = {};
    g_shadow_tracking_time = 0;
    g_shadow_tracking_identity = {};
    g_shadow_body_time = 0;
    g_shadow_body_identity = nullptr;
    g_shadow_generation = 0;
    g_physical_validation_enabled = false;
    g_physical_validation_source =
        BlackPlaguePhysicalValidationRequestSource::disabled;
    g_room_scale_enabled = false;
    g_room_scale_source = BlackPlagueRoomScaleRequestSource::disabled;
    InvalidateRoomScaleCameraSampleLocked();
    InvalidatePendingPhysicalValidationLocked();
    InvalidateDirectLocomotionIntentLocked();
    g_physical_validation_telemetry = {};
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

bool PublishBlackPlagueForwardIntent(void* player, float amount,
    float delta_seconds) noexcept {
    return Publish(player, amount, delta_seconds, false);
}

bool PublishBlackPlagueSidewaysIntent(void* player, float amount,
    float delta_seconds) noexcept {
    return Publish(player, amount, delta_seconds, true);
}

bool BlackPlagueDirectLocomotionAvailable(void* player) noexcept {
    if (!g_installed.load(std::memory_order_acquire) ||
        !MatchesCurrentBody(player)) return false;
    AcquireSRWLockShared(&g_lock);
    const bool enabled = g_room_scale_enabled;
    ReleaseSRWLockShared(&g_lock);
    return enabled &&
        ReadPhysicalBodyDisplacementBoundaryStatus().initialized;
}

bool PublishBlackPlagueDirectLocomotionIntent(void* player,
    const BlackPlagueDirectLocomotionIntent& intent) noexcept {
    if (!BlackPlagueDirectLocomotionAvailable(player) ||
        intent.player_generation == 0 ||
        !std::isfinite(intent.move[0]) || !std::isfinite(intent.move[1]) ||
        !std::isfinite(intent.move_scale) || intent.move_scale < 0.0F ||
        !FiniteMatrix(intent.head_world_pose)) return false;
    AcquireSRWLockExclusive(&g_lock);
    g_direct_locomotion_intent.valid = true;
    g_direct_locomotion_intent.player = player;
    g_direct_locomotion_intent.intent = intent;
    g_direct_locomotion_intent.published_time = GetTickCount64();
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

void InvalidateBlackPlagueDirectLocomotionIntent() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    InvalidateDirectLocomotionIntentLocked();
    ReleaseSRWLockExclusive(&g_lock);
}

void PrepareBlackPlagueNativeBodyTick(void* player, void* character_body,
    const std::array<float, 3>& body_before, float delta_seconds,
    std::uint64_t tick_sequence, std::uint64_t player_generation) noexcept {
    if (!g_installed.load(std::memory_order_acquire) || tick_sequence == 0 ||
        !FiniteVector(body_before) || !MatchesCurrentBody(player) ||
        Read<void*>(player, kPlayerCharacterBodyOffset) != character_body) {
        return;
    }

    AcquireSRWLockExclusive(&g_lock);
    if (!g_shadow_enabled) {
        ReleaseSRWLockExclusive(&g_lock);
        return;
    }

    const auto now = GetTickCount64();
    std::string owner_error;
    const bool owners_ready = MovementOwnerReady(owner_error) &&
        BodyUpdateOwnerReady(owner_error);
    const bool physical_owner_ready = !g_physical_validation_enabled ||
        ReadPhysicalBodyDisplacementBoundaryStatus().initialized;
    if (!owners_ready || !physical_owner_ready || g_shadow_tracking_time == 0 ||
        now - g_shadow_tracking_time > 250) {
        g_shadow.Reset();
        g_shadow_telemetry.latest = {};
        InvalidatePendingPhysicalValidationLocked();
        InvalidateRoomScaleCameraSampleLocked();
        ReleaseSRWLockExclusive(&g_lock);
        return;
    }

    if (g_shadow_body_time != 0 && now - g_shadow_body_time > 250) {
        g_shadow.Reset();
        InvalidatePendingPhysicalValidationLocked();
        InvalidateRoomScaleCameraSampleLocked();
    }
    if (g_shadow_body_identity != character_body) {
        if (g_shadow_body_identity != nullptr) {
            InvalidatePendingPhysicalValidationLocked();
        }
        g_shadow.Reset();
        InvalidateRoomScaleCameraSampleLocked();
        g_shadow_body_identity = character_body;
        ++g_shadow_generation;
    }
    if (g_pending_physical_validation.active) {
        // A Begin without its matching End is an epoch break. Never integrate
        // a second pose or request on top of an unfinished body tick.
        InvalidatePendingPhysicalValidationLocked();
        g_shadow.Reset();
    }

    const auto tick = g_shadow.PrepareTick(
        g_shadow_pose, g_shadow_yaw, g_shadow_generation,
        body_before, delta_seconds, g_shadow_tracking_identity);
    if (!tick.valid) {
        InvalidateRoomScaleCameraSampleLocked();
        ReleaseSRWLockExclusive(&g_lock);
        return;
    }

    g_pending_physical_validation.active = true;
    g_pending_physical_validation.character_body = character_body;
    g_pending_physical_validation.generation = g_shadow_generation;
    g_pending_physical_validation.tick_sequence = tick_sequence;
    g_pending_physical_validation.tick = tick;

    if (g_room_scale_enabled && g_direct_locomotion_intent.valid) {
        const bool fresh = now >= g_direct_locomotion_intent.published_time &&
            now - g_direct_locomotion_intent.published_time <=
                kDirectLocomotionIntentMaximumAgeMilliseconds;
        if (!fresh || g_direct_locomotion_intent.player != player ||
            g_direct_locomotion_intent.intent.player_generation !=
                player_generation) {
            InvalidateDirectLocomotionIntentLocked();
        } else if (std::isfinite(delta_seconds) && delta_seconds > 0.0F &&
                   delta_seconds <= 0.25F) {
            runtime::VrAnalogState move;
            move.active = true;
            move.x = g_direct_locomotion_intent.intent.move[0];
            move.y = g_direct_locomotion_intent.intent.move[1];
            runtime::VrMatrix44 head_world_pose;
            head_world_pose.values = g_direct_locomotion_intent.intent.head_world_pose;
            const auto direction = runtime::HeadRelativeMoveDirection(
                head_world_pose, move);
            const auto displacement = runtime::LocomotionDisplacement(
                direction, delta_seconds,
                g_direct_locomotion_intent.intent.move_scale,
                false,
                g_direct_locomotion_intent.intent.sprinting);
            if (HasHorizontalRequest(displacement)) {
                static_cast<void>(
                    QueueLocomotionBodyDisplacement(displacement));
            }
        }
    }

    if (g_physical_validation_enabled && !tick.reset &&
        HasHorizontalRequest(tick.reconciliation.physical_request)) {
        if (QueuePhysicalBodyDisplacement(
                tick.reconciliation.physical_request)) {
            g_pending_physical_validation.physical_request_queued = true;
            ++g_physical_validation_telemetry.queued_plans;
            g_physical_validation_telemetry.pending = true;
            g_physical_validation_telemetry.latest_result =
                BlackPlaguePhysicalValidationResult::queued;
            g_physical_validation_telemetry.expected_character_body =
                reinterpret_cast<std::uintptr_t>(character_body);
            g_physical_validation_telemetry.expected_generation =
                g_shadow_generation;
            g_physical_validation_telemetry.requested_displacement =
                tick.reconciliation.physical_request;
        } else {
            ++g_physical_validation_telemetry.queue_failures;
            g_physical_validation_telemetry.latest_result =
                BlackPlaguePhysicalValidationResult::queue_failed;
            g_pending_physical_validation = {};
            g_shadow.Reset();
            InvalidateRoomScaleCameraSampleLocked();
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void ObserveBlackPlagueNativeBodyTick(void* player, void* character_body,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    const std::array<float, 3>& feet_after,
    std::uint64_t tick_sequence,
    const BlackPlaguePhysicalTickObservation& physical_tick) noexcept {
    if (!g_installed.load(std::memory_order_acquire) ||
        !MatchesCurrentBody(player) ||
        Read<void*>(player, kPlayerCharacterBodyOffset) != character_body) {
        InvalidateBlackPlagueShadowTracking();
        return;
    }
    runtime::VrAcceptedBodyMotion accepted;
    if (!runtime::ObserveAcceptedBodyMotion(body_before, body_after,
            accepted)) {
        InvalidateBlackPlagueShadowTracking();
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    g_motion.valid = true;
    ++g_motion.native_tick_sequence;
    g_motion.feet_after = feet_after;
    g_motion.accepted = accepted;
    if (g_shadow_enabled) {
        const auto now = GetTickCount64();
        const bool transaction_matches =
            g_pending_physical_validation.active &&
            g_pending_physical_validation.character_body == character_body &&
            g_pending_physical_validation.generation == g_shadow_generation &&
            g_pending_physical_validation.tick_sequence == tick_sequence;
        if (!transaction_matches) {
            g_shadow.Reset();
            g_shadow_telemetry.latest = {};
            InvalidatePendingPhysicalValidationLocked();
            InvalidateRoomScaleCameraSampleLocked();
        } else {
            runtime::VrAcceptedBodyMotion physical_motion{};
            bool matched_physical_observation = false;
            bool room_scale_sample_safe = true;
            if (g_pending_physical_validation.physical_request_queued) {
                const bool request_matches = physical_tick.request_injected &&
                    SameVector(physical_tick.physical_requested_displacement,
                        g_pending_physical_validation.tick.reconciliation.
                            physical_request) &&
                    FiniteVector(physical_tick.position_before_injection) &&
                    FiniteVector(physical_tick.position_after_injection) &&
                    FiniteVector(physical_tick.physical_accepted_displacement);
                if (request_matches) {
                    physical_motion.body_before =
                        physical_tick.position_before_injection;
                    physical_motion.accepted_displacement =
                        physical_tick.physical_accepted_displacement;
                    physical_motion.body_after = physical_motion.body_before;
                    for (const auto axis : {0U, 1U, 2U}) {
                        physical_motion.body_after[axis] +=
                            physical_motion.accepted_displacement[axis];
                    }
                }
                matched_physical_observation = request_matches;
                if (!request_matches) {
                    room_scale_sample_safe = false;
                    ++g_physical_validation_telemetry.invalidated_plans;
                    g_physical_validation_telemetry.latest_result =
                        BlackPlaguePhysicalValidationResult::invalidated;
                    InvalidatePhysicalBodyDisplacement();
                }
            }
            const auto completed_tick = g_pending_physical_validation.tick;
            auto shadow_sample = room_scale_sample_safe
                ? g_shadow.CompleteTick(
                    completed_tick,
                    accepted,
                    feet_after,
                    matched_physical_observation,
                    physical_motion)
                : BodyReconciliationShadowSample{};
            if (matched_physical_observation && shadow_sample.valid &&
                !shadow_sample.reset) {
                ++g_physical_validation_telemetry.matched_observations;
                g_physical_validation_telemetry.latest_result =
                    BlackPlaguePhysicalValidationResult::reconciled;
                g_physical_validation_telemetry.physical_motion = physical_motion;
                g_physical_validation_telemetry.reconciliation =
                    shadow_sample.physical_reconciliation;
            } else if (matched_physical_observation && shadow_sample.reset) {
                room_scale_sample_safe = false;
                ++g_physical_validation_telemetry.invalidated_plans;
                g_physical_validation_telemetry.latest_result =
                    BlackPlaguePhysicalValidationResult::invalidated;
            }
            if (!shadow_sample.valid) room_scale_sample_safe = false;
            g_pending_physical_validation = {};
            g_physical_validation_telemetry.pending = false;
            g_physical_validation_telemetry.expected_character_body = 0;
            g_physical_validation_telemetry.expected_generation = 0;
            g_shadow_telemetry.latest = shadow_sample;
            ++g_shadow_telemetry.observed_ticks;
            if (g_shadow_telemetry.latest.reset) ++g_shadow_telemetry.resets;
            if (room_scale_sample_safe) {
                PublishRoomScaleCameraSampleLocked(
                    shadow_sample,
                    completed_tick,
                    now);
            } else {
                InvalidateRoomScaleCameraSampleLocked();
            }
        }
        g_shadow_body_time = now;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

BlackPlagueBodyMotion ConsumeBlackPlagueBodyMotion() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const BlackPlagueBodyMotion result = g_motion;
    g_motion = {};
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

void PublishBlackPlagueShadowTracking(const runtime::VrMatrix34& pose,
    float world_yaw, bool recentered,
    runtime::VrTrackingSampleIdentity identity) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_shadow_enabled) {
        if (recentered) {
            g_shadow.Reset();
            InvalidatePendingPhysicalValidationLocked();
            InvalidateRoomScaleCameraSampleLocked();
        }
        g_shadow_pose = pose;
        g_shadow_tracking_identity = identity;
        g_shadow_yaw = world_yaw;
        g_shadow_tracking_time = GetTickCount64();
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void InvalidateBlackPlagueShadowTracking() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_shadow_tracking_time = 0;
    g_shadow_tracking_identity = {};
    g_shadow_body_time = 0;
    g_shadow.Reset();
    g_shadow_telemetry.latest = {};
    InvalidatePendingPhysicalValidationLocked();
    InvalidateRoomScaleCameraSampleLocked();
    ReleaseSRWLockExclusive(&g_lock);
}

BlackPlagueShadowTelemetry ConsumeBlackPlagueShadowTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto result = g_shadow_telemetry;
    g_shadow_telemetry = {};
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

BlackPlagueShadowStatus ReadBlackPlagueShadowStatus() noexcept {
    AcquireSRWLockShared(&g_lock);
    const BlackPlagueShadowStatus status{g_shadow_enabled, g_shadow_source};
    ReleaseSRWLockShared(&g_lock);
    return status;
}

BlackPlaguePhysicalValidationStatus
ReadBlackPlaguePhysicalValidationStatus() noexcept {
    AcquireSRWLockShared(&g_lock);
    const BlackPlaguePhysicalValidationStatus status{
        g_physical_validation_enabled, g_physical_validation_source};
    ReleaseSRWLockShared(&g_lock);
    return status;
}

BlackPlaguePhysicalValidationTelemetry
ConsumeBlackPlaguePhysicalValidationTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto result = g_physical_validation_telemetry;
    const bool pending = g_physical_validation_telemetry.pending;
    const auto expected_body =
        g_physical_validation_telemetry.expected_character_body;
    const auto expected_generation =
        g_physical_validation_telemetry.expected_generation;
    const auto requested =
        g_physical_validation_telemetry.requested_displacement;
    g_physical_validation_telemetry = {};
    if (pending) {
        g_physical_validation_telemetry.pending = true;
        g_physical_validation_telemetry.expected_character_body = expected_body;
        g_physical_validation_telemetry.expected_generation = expected_generation;
        g_physical_validation_telemetry.requested_displacement = requested;
    }
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

BlackPlagueRoomScaleStatus ReadBlackPlagueRoomScaleStatus() noexcept {
    AcquireSRWLockShared(&g_lock);
    const BlackPlagueRoomScaleStatus status{
        g_room_scale_enabled, g_room_scale_source};
    ReleaseSRWLockShared(&g_lock);
    return status;
}

BlackPlagueRoomScaleCameraSample
ReadBlackPlagueRoomScaleCameraSample() noexcept {
    AcquireSRWLockShared(&g_lock);
    auto sample = g_room_scale_camera_sample;
    const auto sample_time = g_room_scale_camera_sample_time;
    ReleaseSRWLockShared(&g_lock);
    if (!sample.enabled || !sample.valid || sample_time == 0 ||
        GetTickCount64() - sample_time >
            kRoomScaleSampleMaximumAgeMilliseconds) {
        sample.valid = false;
        sample.horizontal_world_offset = {};
    }
    return sample;
}

} // namespace penumbra_vr::backends::black_plague
