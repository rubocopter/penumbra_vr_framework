#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

volatile LONG g_matrix_mode_calls = 0;
volatile LONG g_load_matrix_calls = 0;
volatile LONG g_ortho_calls = 0;

} // namespace

extern "C" void APIENTRY glMatrixMode(unsigned int) {
    InterlockedIncrement(&g_matrix_mode_calls);
}

extern "C" void APIENTRY glLoadMatrixf(const float*) {
    InterlockedIncrement(&g_load_matrix_calls);
}

extern "C" void APIENTRY glOrtho(double, double, double, double, double, double) {
    InterlockedIncrement(&g_ortho_calls);
}

extern "C" unsigned long __cdecl OpenGlStub_MatrixModeCalls() {
    return static_cast<unsigned long>(g_matrix_mode_calls);
}

extern "C" unsigned long __cdecl OpenGlStub_LoadMatrixCalls() {
    return static_cast<unsigned long>(g_load_matrix_calls);
}

extern "C" unsigned long __cdecl OpenGlStub_OrthoCalls() {
    return static_cast<unsigned long>(g_ortho_calls);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, void*) {
    return TRUE;
}
