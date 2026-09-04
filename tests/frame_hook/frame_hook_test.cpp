#include "sdl_frame_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

extern "C" __declspec(dllimport) void __cdecl SDL_GL_SwapBuffers();
extern "C" __declspec(dllimport) unsigned long __cdecl SDLStub_GetSwapCount();

namespace {

std::uint64_t g_callback_count = 0;
std::atomic<bool> g_block_callback{false};
std::atomic<bool> g_callback_entered{false};
std::atomic<bool> g_release_callback{false};

void OnFrame(std::uint64_t frame_number) noexcept {
    if (frame_number == g_callback_count + 1) {
        ++g_callback_count;
    }
    if (g_block_callback.load(std::memory_order_acquire)) {
        g_callback_entered.store(true, std::memory_order_release);
        while (!g_release_callback.load(std::memory_order_acquire)) {
            Sleep(1);
        }
    }
}

__declspec(noinline) void CallImportedSwapAfterUnhook() {
    SDL_GL_SwapBuffers();
}

} // namespace

int main() {
    std::string error;
    if (!penumbra_vr::hooks::InstallSdlSwapHook(&OnFrame, error)) {
        std::cerr << "InstallSdlSwapHook failed: " << error << '\n';
        return 1;
    }

    SDL_GL_SwapBuffers();
    SDL_GL_SwapBuffers();
    SDL_GL_SwapBuffers();
    if (g_callback_count != 3 || penumbra_vr::hooks::ObservedFrameCount() != 3 ||
        SDLStub_GetSwapCount() != 3) {
        std::cerr << "The hook did not observe and forward exactly three swaps\n";
        return 2;
    }

    g_block_callback.store(true, std::memory_order_release);
    std::thread in_flight_swap([] { SDL_GL_SwapBuffers(); });
    for (int wait = 0;
         wait < 2000 && !g_callback_entered.load(std::memory_order_acquire);
         ++wait) {
        Sleep(1);
    }
    if (!g_callback_entered.load(std::memory_order_acquire)) {
        std::cerr << "The in-flight swap did not enter its callback\n";
        g_release_callback.store(true, std::memory_order_release);
        in_flight_swap.join();
        return 3;
    }

    std::atomic<bool> removal_finished{false};
    bool removal_succeeded = false;
    std::string removal_error;
    std::thread removal([&] {
        removal_succeeded =
            penumbra_vr::hooks::RemoveSdlSwapHook(removal_error);
        removal_finished.store(true, std::memory_order_release);
    });
    Sleep(20);
    if (removal_finished.load(std::memory_order_acquire)) {
        std::cerr << "Hook removal did not wait for its in-flight callback\n";
        g_release_callback.store(true, std::memory_order_release);
        removal.join();
        in_flight_swap.join();
        return 4;
    }
    g_release_callback.store(true, std::memory_order_release);
    removal.join();
    in_flight_swap.join();
    if (!removal_succeeded) {
        std::cerr << "RemoveSdlSwapHook failed: " << removal_error << '\n';
        return 5;
    }

    CallImportedSwapAfterUnhook();
    if (g_callback_count != 4 || penumbra_vr::hooks::ObservedFrameCount() != 4 ||
        SDLStub_GetSwapCount() != 5) {
        std::cerr << "The original import was not restored correctly: callbacks="
                  << g_callback_count << " observed=" << penumbra_vr::hooks::ObservedFrameCount()
                  << " forwarded=" << SDLStub_GetSwapCount() << '\n';
        return 6;
    }

    std::cout << "SDL frame hook install, forwarding and removal passed\n";
    return 0;
}
