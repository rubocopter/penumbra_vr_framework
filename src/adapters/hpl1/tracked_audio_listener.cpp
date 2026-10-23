#include "tracked_audio_listener.hpp"

#include "tracked_listener.hpp"
#include "iat_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace penumbra_vr::adapters::hpl1 {
namespace {
// OpenAL ABI enums, not HPL layout or exact-build addresses. Both allowlisted
// executables import these cdecl functions by name; no engine RVA is patched.
constexpr int kPosition = 0x1004;
constexpr int kVelocity = 0x1006;
constexpr int kOrientation = 0x100F;
constexpr std::uint64_t kMaximumPoseAgeMs = 250;
using ListenerFv = void (__cdecl*)(int, const float*);
using GetListenerFv = void (__cdecl*)(int, float*);
using GetCurrentContext = void* (__cdecl*)();
using DestroyContext = void (__cdecl*)(void*);
using MakeContextCurrent = char (__cdecl*)(void*);

SRWLOCK g_lock = SRWLOCK_INIT;
hooks::IatHook g_listener_hook;
hooks::IatHook g_destroy_hook;
hooks::IatHook g_make_current_hook;
// Retain originals after unhooking so an already-entered trampoline can finish.
std::atomic<ListenerFv> g_original_listener{nullptr};
std::atomic<DestroyContext> g_original_destroy{nullptr};
std::atomic<MakeContextCurrent> g_original_make_current{nullptr};
GetListenerFv g_get_listener = nullptr;
GetCurrentContext g_current_context = nullptr;
AudioListenerEligible g_eligible = nullptr;
bool g_enabled = false;
void* g_context = nullptr;
std::array<float, 3> g_native_position{};
std::array<float, 3> g_native_velocity{};
std::array<float, 6> g_native_orientation{};
audio::TrackedListenerPose g_pose;
std::uint64_t g_published_at = 0;
bool g_applied = false;

class Lock {
public:
    Lock() noexcept { AcquireSRWLockExclusive(&g_lock); }
    ~Lock() { ReleaseSRWLockExclusive(&g_lock); }
};

void WritePose(const audio::TrackedListenerPose& pose) noexcept {
    const auto original = g_original_listener.load(std::memory_order_acquire);
    const std::array<float, 3> zero_velocity{};
    original(kPosition, pose.position.data());
    original(kVelocity, zero_velocity.data());
    original(kOrientation, pose.orientation.data());
}

void RestoreNative() noexcept {
    if (g_applied && g_current_context && g_context &&
        g_current_context() == g_context) {
        const auto original = g_original_listener.load(std::memory_order_acquire);
        original(kPosition, g_native_position.data());
        original(kVelocity, g_native_velocity.data());
        original(kOrientation, g_native_orientation.data());
        g_applied = false;
    }
    g_published_at = 0;
}

void ObserveContext(void* context) noexcept {
    if (context == g_context) return;
    // A new context gets its own native defaults. A destroyed context is
    // explicitly forgotten by HookedDestroyContext, including pointer reuse.
    g_context = context;
    g_published_at = 0;
    g_applied = false;
    if (!context) return;
    g_get_listener(kPosition, g_native_position.data());
    g_get_listener(kVelocity, g_native_velocity.data());
    g_get_listener(kOrientation, g_native_orientation.data());
}

bool PoseUsable() noexcept {
    const auto now = GetTickCount64();
    return g_enabled && g_eligible && g_eligible() && g_context &&
        g_published_at != 0 && now >= g_published_at &&
        now - g_published_at <= kMaximumPoseAgeMs;
}

void __cdecl HookedListenerFv(int param, const float* values) noexcept {
    Lock lock;
    const auto original = g_original_listener.load(std::memory_order_acquire);
    if (!g_enabled || !g_current_context) { original(param, values); return; }
    void* context = g_current_context();
    ObserveContext(context);
    if (context && values) {
        if (param == kPosition) std::copy_n(values, 3, g_native_position.begin());
        if (param == kVelocity) std::copy_n(values, 3, g_native_velocity.begin());
        if (param == kOrientation) std::copy_n(values, 6, g_native_orientation.begin());
    }
    if (!PoseUsable()) {
        RestoreNative();
        original(param, values);
        return;
    }
    if (values && (param == kPosition || param == kVelocity || param == kOrientation)) {
        // Publish all attributes together, including after native updates. This
        // prevents a mixed native/tracked pose and preserves Rework zero velocity.
        WritePose(g_pose);
        g_applied = true;
    } else {
        original(param, values);
    }
}

void __cdecl HookedDestroyContext(void* context) noexcept {
    Lock lock;
    if (context == g_context) {
        RestoreNative();
        g_context = nullptr;
        g_published_at = 0;
        g_applied = false;
    }
    g_original_destroy.load(std::memory_order_acquire)(context);
}

char __cdecl HookedMakeContextCurrent(void* context) noexcept {
    Lock lock;
    const bool switching = g_current_context && g_current_context() != context;
    // Restore before the native switch; do not leave a tracked pose in an
    // inactive context or mistake it for that context's native defaults later.
    if (switching) RestoreNative();
    const auto result = g_original_make_current.load(std::memory_order_acquire)(context);
    if (result && switching) ObserveContext(nullptr);
    return result;
}

void PublishListenerOriginal(void* original) noexcept {
    g_original_listener.store(reinterpret_cast<ListenerFv>(original), std::memory_order_release);
}
void PublishDestroyOriginal(void* original) noexcept {
    g_original_destroy.store(reinterpret_cast<DestroyContext>(original), std::memory_order_release);
}
void PublishMakeCurrentOriginal(void* original) noexcept {
    g_original_make_current.store(reinterpret_cast<MakeContextCurrent>(original), std::memory_order_release);
}
} // namespace

bool InstallTrackedAudioListener(AudioListenerEligible eligible, std::string& error) noexcept {
    error.clear();
    Lock lock;
    if (g_listener_hook.installed() || g_destroy_hook.installed() ||
        g_make_current_hook.installed()) {
        error = "Tracked audio listener is already installed";
        return false;
    }
    const auto module = GetModuleHandleW(L"OpenAL32.dll");
    g_get_listener = module ? reinterpret_cast<GetListenerFv>(
        GetProcAddress(module, "alGetListenerfv")) : nullptr;
    g_current_context = module ? reinterpret_cast<GetCurrentContext>(
        GetProcAddress(module, "alcGetCurrentContext")) : nullptr;
    if (!eligible || !g_get_listener || !g_current_context) {
        error = "Tracked audio listener requires the native OpenAL listener/context API";
        return false;
    }
    if (!hooks::InstallIatHook("OpenAL32.dll", "alcDestroyContext",
            reinterpret_cast<void*>(&HookedDestroyContext), g_destroy_hook,
            error, &PublishDestroyOriginal)) return false;
    if (!hooks::InstallIatHook("OpenAL32.dll", "alcMakeContextCurrent",
            reinterpret_cast<void*>(&HookedMakeContextCurrent), g_make_current_hook,
            error, &PublishMakeCurrentOriginal)) {
        std::string rollback;
        if (!hooks::RemoveIatHook(g_destroy_hook, rollback)) error += "; " + rollback;
        return false;
    }
    if (!hooks::InstallIatHook("OpenAL32.dll", "alListenerfv",
            reinterpret_cast<void*>(&HookedListenerFv), g_listener_hook,
            error, &PublishListenerOriginal)) {
        std::string rollback;
        if (!hooks::RemoveIatHook(g_destroy_hook, rollback)) error += "; " + rollback;
        if (!hooks::RemoveIatHook(g_make_current_hook, rollback)) error += "; " + rollback;
        return false;
    }
    g_context = nullptr;
    g_published_at = 0;
    g_applied = false;
    g_eligible = eligible;
    g_enabled = true;
    return true;
}

void PublishTrackedAudioListener(const runtime::VrMatrix44& head_world) noexcept {
    Lock lock;
    if (!g_enabled) return;
    void* context = g_current_context();
    ObserveContext(context);
    audio::TrackedListenerPose pose;
    if (!context || !g_eligible() || !audio::BuildTrackedListenerPose(head_world, pose)) {
        RestoreNative();
        return;
    }
    g_pose = pose;
    g_published_at = GetTickCount64();
    WritePose(g_pose);
    g_applied = true;
}

void InvalidateTrackedAudioListener() noexcept {
    Lock lock;
    RestoreNative();
}

bool RemoveTrackedAudioListener(std::string& error) noexcept {
    error.clear();
    Lock lock;
    RestoreNative();
    g_enabled = false;
    g_eligible = nullptr;
    // Keep native cache alive if shutdown runs on a thread without the context.
    // Subsequent native writes remain untouched; never steal/make a context current.
    std::string listener_error, destroy_error, current_error;
    const bool listener_removed = hooks::RemoveIatHook(g_listener_hook, listener_error);
    const bool destroy_removed = hooks::RemoveIatHook(g_destroy_hook, destroy_error);
    const bool current_removed = hooks::RemoveIatHook(g_make_current_hook, current_error);
    if (!listener_removed || !destroy_removed || !current_removed) {
        error = listener_error + "; " + destroy_error + "; " + current_error;
        return false;
    }
    return true;
}
} // namespace penumbra_vr::adapters::hpl1
