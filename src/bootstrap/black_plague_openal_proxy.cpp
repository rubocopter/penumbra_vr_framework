#include "black_plague_openal_compat.hpp"

#define AL_LIBTYPE_STATIC
#include <AL/alc.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <filesystem>
#include <string>

#include "black_plague_openal_forwarders.inc"

#pragma comment(linker, "/export:alcGetProcAddress=_alcGetProcAddress,@162")
#pragma comment(linker, "/export:alcOpenDevice=_alcOpenDevice,@170")

namespace {

constexpr wchar_t kImplementationName[] = L"PenumbraVR_OpenALSoft.dll";
HMODULE g_instance = nullptr;
HMODULE g_openal_soft = nullptr;

std::filesystem::path ModulePath(HMODULE module) {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return path;
}

HMODULE OpenAlSoft() noexcept {
    if (g_openal_soft != nullptr) return g_openal_soft;
    const auto proxy_path = ModulePath(g_instance);
    if (proxy_path.empty()) return nullptr;
    const auto implementation = proxy_path.parent_path() / kImplementationName;
    g_openal_soft = LoadLibraryW(implementation.c_str());
    return g_openal_soft;
}

FARPROC Resolve(const char* name) noexcept {
    const HMODULE module = OpenAlSoft();
    return module == nullptr ? nullptr : GetProcAddress(module, name);
}

using OpenDeviceFn = ALCdevice*(ALC_APIENTRY*)(const ALCchar*);
using GetProcAddressFn = void*(ALC_APIENTRY*)(ALCdevice*, const ALCchar*);

} // namespace

extern "C" ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar* device_name) {
    const auto open_device = reinterpret_cast<OpenDeviceFn>(Resolve("alcOpenDevice"));
    if (open_device == nullptr) return nullptr;
    return open_device(penumbra_vr::black_plague::NormalizeOpenAlPlaybackDevice(device_name));
}

extern "C" void* ALC_APIENTRY alcGetProcAddress(ALCdevice* device, const ALCchar* function_name) {
    if (function_name != nullptr && std::strcmp(function_name, "alcOpenDevice") == 0) {
        return reinterpret_cast<void*>(&alcOpenDevice);
    }
    const auto get_proc_address =
        reinterpret_cast<GetProcAddressFn>(Resolve("alcGetProcAddress"));
    return get_proc_address == nullptr ? nullptr : get_proc_address(device, function_name);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
