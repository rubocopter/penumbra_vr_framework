#include "sdl_frame_hook.hpp"

#include <cstdint>
#include <iostream>
#include <string>

extern "C" __declspec(dllimport) void __cdecl SDL_GL_SwapBuffers();
extern "C" __declspec(dllimport) unsigned long __cdecl SDLStub_GetSwapCount();

namespace {

std::uint64_t g_callback_count = 0;

void OnFrame(std::uint64_t frame_number) noexcept {
    if (frame_number == g_callback_count + 1) {
        ++g_callback_count;
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

    if (!penumbra_vr::hooks::RemoveSdlSwapHook(error)) {
        std::cerr << "RemoveSdlSwapHook failed: " << error << '\n';
        return 3;
    }

    CallImportedSwapAfterUnhook();
    if (g_callback_count != 3 || penumbra_vr::hooks::ObservedFrameCount() != 3 ||
        SDLStub_GetSwapCount() != 4) {
        std::cerr << "The original import was not restored correctly: callbacks="
                  << g_callback_count << " observed=" << penumbra_vr::hooks::ObservedFrameCount()
                  << " forwarded=" << SDLStub_GetSwapCount() << '\n';
        return 4;
    }

    std::cout << "SDL frame hook install, forwarding and removal passed\n";
    return 0;
}
