#include "sdl_frame_hook.hpp"

#include "iat_hook.hpp"

#include <atomic>

namespace penumbra_vr::hooks {
namespace {

using SdlSwapBuffers = void(__cdecl*)();

std::atomic<std::uint64_t> g_frame_count{0};
FrameCallback g_callback = nullptr;
IatHook g_swap_hook;

void __cdecl HookedSdlSwapBuffers() noexcept {
    const std::uint64_t frame = g_frame_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (g_callback != nullptr) {
        g_callback(frame);
    }
    reinterpret_cast<SdlSwapBuffers>(g_swap_hook.original)();
}

} // namespace

bool InstallSdlSwapHook(FrameCallback callback, std::string& error) noexcept {
    if (g_swap_hook.installed()) {
        error = "The SDL swap hook is already installed";
        return false;
    }

    g_callback = callback;
    g_frame_count.store(0, std::memory_order_relaxed);
    if (!InstallIatHook(
            "SDL.dll",
            "SDL_GL_SwapBuffers",
            reinterpret_cast<void*>(&HookedSdlSwapBuffers),
            g_swap_hook,
            error)) {
        g_callback = nullptr;
        return false;
    }
    return true;
}

bool RemoveSdlSwapHook(std::string& error) noexcept {
    if (!RemoveIatHook(g_swap_hook, error)) {
        return false;
    }
    g_callback = nullptr;
    return true;
}

std::uint64_t ObservedFrameCount() noexcept {
    return g_frame_count.load(std::memory_order_relaxed);
}

} // namespace penumbra_vr::hooks
