#include "sdl_frame_hook.hpp"

#include "iat_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>

namespace penumbra_vr::hooks {
namespace {

using SdlSwapBuffers = void(__cdecl*)();

std::atomic<std::uint64_t> g_frame_count{0};
std::atomic<FrameCallback> g_callback{nullptr};
std::atomic<void*> g_original_swap{nullptr};
std::atomic<std::uint32_t> g_active_calls{0};
IatHook g_swap_hook;

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

void PublishOriginalSwap(void* original) noexcept {
    g_original_swap.store(original, std::memory_order_release);
}

void __cdecl HookedSdlSwapBuffers() noexcept {
    ActiveCall active_call;
    const std::uint64_t frame = g_frame_count.fetch_add(1, std::memory_order_relaxed) + 1;
    const FrameCallback callback = g_callback.load(std::memory_order_acquire);
    if (callback != nullptr) {
        callback(frame);
    }
    const auto original = reinterpret_cast<SdlSwapBuffers>(
        g_original_swap.load(std::memory_order_acquire));
    if (original != nullptr) {
        original();
    }
}

} // namespace

bool InstallSdlSwapHook(FrameCallback callback, std::string& error) noexcept {
    if (g_swap_hook.installed()) {
        error = "The SDL swap hook is already installed";
        return false;
    }

    g_callback.store(callback, std::memory_order_release);
    g_frame_count.store(0, std::memory_order_relaxed);
    if (!InstallIatHook(
            "SDL.dll",
            "SDL_GL_SwapBuffers",
            reinterpret_cast<void*>(&HookedSdlSwapBuffers),
            g_swap_hook,
            error,
            &PublishOriginalSwap)) {
        g_callback.store(nullptr, std::memory_order_release);
        return false;
    }
    return true;
}

bool RemoveSdlSwapHook(std::string& error) noexcept {
    if (!RemoveIatHook(g_swap_hook, error)) {
        return false;
    }
    g_callback.store(nullptr, std::memory_order_release);

    constexpr std::uint32_t kQuiescenceTimeoutMilliseconds = 2000;
    for (std::uint32_t elapsed = 0;
         elapsed < kQuiescenceTimeoutMilliseconds;
         ++elapsed) {
        if (g_active_calls.load(std::memory_order_acquire) == 0) {
            return true;
        }
        Sleep(1);
    }
    error = "Timed out waiting for an active SDL swap callback to finish";
    return false;
}

std::uint64_t ObservedFrameCount() noexcept {
    return g_frame_count.load(std::memory_order_relaxed);
}

} // namespace penumbra_vr::hooks
