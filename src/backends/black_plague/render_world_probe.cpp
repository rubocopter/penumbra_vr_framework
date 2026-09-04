#include "render_world_probe.hpp"

#include "rel32_call_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::uintptr_t kRenderWorldRva = 0x0012CB10;
constexpr std::uintptr_t kRenderWorldCallSiteRva = 0x000EE010;
constexpr std::array<std::uint8_t, 5> kExpectedCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};

using RenderWorld = void(__thiscall*)(void* renderer, void* world, void* camera, float frame_time);

hooks::Rel32CallHook g_hook;
std::atomic<void*> g_original_target{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
SRWLOCK g_telemetry_lock = SRWLOCK_INIT;
RenderWorldFrameTelemetry g_telemetry;

class ActiveCall final {
public:
    ActiveCall() noexcept {
        g_active_calls.fetch_add(1, std::memory_order_acq_rel);
    }
    ActiveCall(const ActiveCall&) = delete;
    ActiveCall& operator=(const ActiveCall&) = delete;
    ~ActiveCall() {
        g_active_calls.fetch_sub(1, std::memory_order_acq_rel);
    }
};

void __fastcall HookedRenderWorld(
    void* renderer,
    void*,
    void* world,
    void* camera,
    float frame_time) noexcept {
    ActiveCall active_call;

    AcquireSRWLockExclusive(&g_telemetry_lock);
    ++g_telemetry.calls;
    g_telemetry.renderer = reinterpret_cast<std::uintptr_t>(renderer);
    g_telemetry.world = reinterpret_cast<std::uintptr_t>(world);
    g_telemetry.camera = reinterpret_cast<std::uintptr_t>(camera);
    g_telemetry.frame_time = frame_time;
    ReleaseSRWLockExclusive(&g_telemetry_lock);

    const auto original = reinterpret_cast<RenderWorld>(
        g_original_target.load(std::memory_order_acquire));
    if (original != nullptr) {
        original(renderer, world, camera, frame_time);
    }
}

[[nodiscard]] void* DecodeExpectedTarget(std::uint8_t* instruction) noexcept {
    std::int32_t displacement = 0;
    std::memcpy(&displacement, kExpectedCall.data() + 1, sizeof(displacement));
    const std::intptr_t next_instruction =
        reinterpret_cast<std::intptr_t>(instruction + kExpectedCall.size());
    return reinterpret_cast<void*>(next_instruction + displacement);
}

} // namespace

bool InstallRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (g_hook.installed()) {
        error = "The Black Plague RenderWorld probe is already installed";
        return false;
    }

    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "GetModuleHandleW(NULL) failed";
        return false;
    }
    std::uint8_t* call_site = image + kRenderWorldCallSiteRva;
    void* expected_target = image + kRenderWorldRva;
    if (DecodeExpectedTarget(call_site) != expected_target) {
        error = "The manifest call displacement does not target RenderWorld";
        return false;
    }

    AcquireSRWLockExclusive(&g_telemetry_lock);
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    g_active_calls.store(0, std::memory_order_release);
    g_original_target.store(expected_target, std::memory_order_release);

    if (!hooks::InstallRel32CallHook(
            call_site,
            kExpectedCall,
            reinterpret_cast<void*>(&HookedRenderWorld),
            g_hook,
            error)) {
        g_original_target.store(nullptr, std::memory_order_release);
        return false;
    }
    return true;
}

bool RemoveRenderWorldProbe(std::string& error) noexcept {
    error.clear();
    if (!hooks::RemoveRel32CallHook(g_hook, error)) {
        return false;
    }

    constexpr DWORD kQuiescenceTimeoutMilliseconds = 2000;
    for (DWORD elapsed = 0; elapsed < kQuiescenceTimeoutMilliseconds; ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            g_original_target.store(nullptr, std::memory_order_release);
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for an active RenderWorld probe call to finish";
    return false;
}

RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept {
    AcquireSRWLockExclusive(&g_telemetry_lock);
    const RenderWorldFrameTelemetry result = g_telemetry;
    g_telemetry = {};
    ReleaseSRWLockExclusive(&g_telemetry_lock);
    return result;
}

} // namespace penumbra_vr::backends::black_plague
