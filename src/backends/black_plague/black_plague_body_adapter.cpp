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

std::uint8_t* g_image = nullptr;
bool g_installed = false;
SRWLOCK g_lock = SRWLOCK_INIT;
BlackPlagueBodyMotion g_motion;

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
    const std::array<float, 3>& feet_after) noexcept {
    if (!g_installed || !MatchesCurrentBody(player) ||
        Read<void*>(player, kPlayerCharacterBodyOffset) != character_body) return;
    runtime::VrAcceptedBodyMotion accepted;
    if (!runtime::ObserveAcceptedBodyMotion(body_before, body_after,
            accepted)) return;
    AcquireSRWLockExclusive(&g_lock);
    g_motion.valid = true;
    ++g_motion.native_tick_sequence;
    g_motion.feet_after = feet_after;
    g_motion.accepted = accepted;
    ReleaseSRWLockExclusive(&g_lock);
}

BlackPlagueBodyMotion ConsumeBlackPlagueBodyMotion() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const BlackPlagueBodyMotion result = g_motion;
    g_motion = {};
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
