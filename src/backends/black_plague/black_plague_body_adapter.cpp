#include "black_plague_body_adapter.hpp"
#include "body_adapter_boundary.hpp"
#include "body_collision_probe.hpp"
#include "native_input_bridge.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
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

std::uint8_t* g_image = nullptr;
bool g_installed = false;
SRWLOCK g_lock = SRWLOCK_INIT;
BlackPlagueBodyMotion g_motion;
bool g_shadow_enabled = false;
BodyReconciliationShadow g_shadow;
BlackPlagueShadowTelemetry g_shadow_telemetry;
runtime::VrMatrix34 g_shadow_pose;
float g_shadow_yaw = 0.0F;
std::uint64_t g_shadow_tracking_time = 0;
std::uint64_t g_shadow_body_time = 0;
void* g_shadow_body_identity = nullptr; // Comparison only; never dereferenced.
std::uint64_t g_shadow_generation = 0;

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

[[nodiscard]] bool MatchesCurrentBody(void* player) noexcept {
    return player != nullptr && Read<void*>(player, kPlayerCharacterBodyOffset) != nullptr;
}

[[nodiscard]] bool ReconciliationShadowRequested() noexcept {
    char shadow_option[2]{};
    if (GetEnvironmentVariableA("PVR_BP_RECONCILIATION_SHADOW",
            shadow_option, 2) == 1 && shadow_option[0] == '1') {
        return true;
    }

    // Steam owns the game process when --launch-vr uses steam://, so a variable
    // set only in the launcher shell is not a reliable child-process signal if
    // Steam was already running. The diagnostic launcher therefore holds this
    // per-session named mutex only until probe initialization completes. The
    // adapter samples it once here, preserving the existing default-off and
    // no-persistent-setting semantics.
    HANDLE request = OpenMutexW(SYNCHRONIZE, FALSE, kShadowRequestMutexName);
    if (request == nullptr) return false;
    CloseHandle(request);
    return true;
}

[[nodiscard]] bool InstallForImage(std::uint8_t* image,
    std::string& error) noexcept {
    error.clear();
    if (g_installed) return true;
    if (image == nullptr) {
        error = "The Black Plague image is unavailable";
        return false;
    }
    g_image = image;
    // Input and physics hooks own their respective instructions. Each has
    // already validated pristine exact-build bytes before patching; bind only
    // to their live replacement and never attempt a competing patch here.
    if (!MovementOwnerReady(error) || !BodyUpdateOwnerReady(error)) {
        g_image = nullptr;
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    g_motion = {};
    g_shadow_enabled = ReconciliationShadowRequested();
    g_shadow = {};
    g_shadow_telemetry = {};
    g_shadow_tracking_time = 0;
    g_shadow_body_time = 0;
    g_shadow_body_identity = nullptr;
    g_shadow_generation = 0;
    ReleaseSRWLockExclusive(&g_lock);
    g_installed = true;
    return true;
}

[[nodiscard]] bool Publish(void* player, float amount, float delta_seconds,
    bool sideways) noexcept {
    if (!g_installed || !MatchesCurrentBody(player) ||
        !ValidIntent(amount, delta_seconds)) return false;
    reinterpret_cast<Move>(g_image + (sideways ? kMoveSideways : kMoveForward))(
        player, amount, delta_seconds);
    AcquireSRWLockExclusive(&g_lock);
    g_motion.intent_published = true;
    g_motion.native_horizontal_intent[sideways ? 1U : 0U] = amount;
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

} // namespace

bool InstallBlackPlagueBodyAdapter(std::string& error) noexcept {
    return InstallForImage(reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr)), error);
}

bool RemoveBlackPlagueBodyAdapter(std::string& error) noexcept {
    error.clear();
    g_installed = false;
    g_image = nullptr;
    AcquireSRWLockExclusive(&g_lock);
    g_motion = {};
    g_shadow_enabled = false;
    g_shadow = {};
    g_shadow_telemetry = {};
    g_shadow_tracking_time = 0;
    g_shadow_body_time = 0;
    g_shadow_body_identity = nullptr;
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

void ObserveBlackPlagueNativeBodyTick(void* player, void* character_body,
    const std::array<float, 3>& body_before,
    const std::array<float, 3>& body_after,
    const std::array<float, 3>& feet_after,
    float delta_seconds) noexcept {
    if (!g_installed || !MatchesCurrentBody(player) ||
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
        std::string owner_error;
        const bool owners_ready = MovementOwnerReady(owner_error) &&
            BodyUpdateOwnerReady(owner_error);
        if (!owners_ready || g_shadow_tracking_time == 0 ||
            now - g_shadow_tracking_time > 250) {
            g_shadow.Reset();
            g_shadow_telemetry.latest = {};
        } else {
            if (g_shadow_body_time == 0 || now - g_shadow_body_time > 250)
                g_shadow.Reset();
            if (g_shadow_body_identity != character_body) {
                g_shadow_body_identity = character_body;
                ++g_shadow_generation;
            }
            g_shadow_telemetry.latest = g_shadow.Observe(g_shadow_pose,
                g_shadow_yaw, g_shadow_generation, accepted, feet_after,
                delta_seconds);
            ++g_shadow_telemetry.observed_ticks;
            if (g_shadow_telemetry.latest.reset) ++g_shadow_telemetry.resets;
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
    float world_yaw, bool recentered) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (g_shadow_enabled) {
        if (recentered) g_shadow.Reset();
        g_shadow_pose = pose;
        g_shadow_yaw = world_yaw;
        g_shadow_tracking_time = GetTickCount64();
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void InvalidateBlackPlagueShadowTracking() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_shadow_tracking_time = 0;
    g_shadow_body_time = 0;
    g_shadow.Reset();
    g_shadow_telemetry.latest = {};
    ReleaseSRWLockExclusive(&g_lock);
}

BlackPlagueShadowTelemetry ConsumeBlackPlagueShadowTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto result = g_shadow_telemetry;
    g_shadow_telemetry = {};
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
