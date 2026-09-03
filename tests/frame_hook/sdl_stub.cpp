#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

volatile LONG g_swap_count = 0;

} // namespace

extern "C" void __cdecl SDL_GL_SwapBuffers() {
    InterlockedIncrement(&g_swap_count);
}

extern "C" unsigned long __cdecl SDLStub_GetSwapCount() {
    return static_cast<unsigned long>(g_swap_count);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, void*) {
    return TRUE;
}
