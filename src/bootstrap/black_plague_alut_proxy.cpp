#include "penumbra_vr/black_plague_probe_capabilities.hpp"
#include "penumbra_vr/build_catalog.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#pragma comment(linker, "/export:alutCreateBufferFromFile=PenumbraVR_alut_original.alutCreateBufferFromFile,@1")
#pragma comment(linker, "/export:alutCreateBufferFromFileImage=PenumbraVR_alut_original.alutCreateBufferFromFileImage,@2")
#pragma comment(linker, "/export:alutCreateBufferHelloWorld=PenumbraVR_alut_original.alutCreateBufferHelloWorld,@3")
#pragma comment(linker, "/export:alutCreateBufferWaveform=PenumbraVR_alut_original.alutCreateBufferWaveform,@4")
#pragma comment(linker, "/export:alutExit=PenumbraVR_alut_original.alutExit,@5")
#pragma comment(linker, "/export:alutGetError=PenumbraVR_alut_original.alutGetError,@6")
#pragma comment(linker, "/export:alutGetErrorString=PenumbraVR_alut_original.alutGetErrorString,@7")
#pragma comment(linker, "/export:alutGetMIMETypes=PenumbraVR_alut_original.alutGetMIMETypes,@8")
#pragma comment(linker, "/export:alutGetMajorVersion=PenumbraVR_alut_original.alutGetMajorVersion,@9")
#pragma comment(linker, "/export:alutGetMinorVersion=PenumbraVR_alut_original.alutGetMinorVersion,@10")
#pragma comment(linker, "/export:alutInit=PenumbraVR_alut_original.alutInit,@11")
#pragma comment(linker, "/export:alutInitWithoutContext=PenumbraVR_alut_original.alutInitWithoutContext,@12")
#pragma comment(linker, "/export:alutLoadMemoryFromFile=PenumbraVR_alut_original.alutLoadMemoryFromFile,@13")
#pragma comment(linker, "/export:alutLoadMemoryFromFileImage=PenumbraVR_alut_original.alutLoadMemoryFromFileImage,@14")
#pragma comment(linker, "/export:alutLoadMemoryHelloWorld=PenumbraVR_alut_original.alutLoadMemoryHelloWorld,@15")
#pragma comment(linker, "/export:alutLoadMemoryWaveform=PenumbraVR_alut_original.alutLoadMemoryWaveform,@16")
#pragma comment(linker, "/export:alutLoadWAVFile=PenumbraVR_alut_original.alutLoadWAVFile,@17")
#pragma comment(linker, "/export:alutLoadWAVMemory=PenumbraVR_alut_original.alutLoadWAVMemory,@18")
#pragma comment(linker, "/export:alutSleep=PenumbraVR_alut_original.alutSleep,@19")
#pragma comment(linker, "/export:alutUnloadWAV=PenumbraVR_alut_original.alutUnloadWAV,@20")

namespace {

HINSTANCE g_instance = nullptr;

constexpr std::uintptr_t kRenderWorldCallRva = 0x000EE010;
constexpr std::array<std::uint8_t, 5> kRenderWorldCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00};
constexpr DWORD kStartupTimeoutMs = 15'000;
constexpr std::uint32_t kRequiredStableWindowSamples = 20;

struct GameWindowReadiness {
    DWORD process_id = 0;
    HWND window = nullptr;
    LONG width = 0;
    LONG height = 0;
};

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "<wide conversion failed>";
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::filesystem::path ModulePath(HMODULE module) {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return path;
}

std::filesystem::path BootstrapLogPath() {
    std::wstring local_app_data(32768, L'\0');
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", local_app_data.data(),
        static_cast<DWORD>(local_app_data.size()));
    if (length == 0 || length >= local_app_data.size()) return {};
    local_app_data.resize(length);
    const std::filesystem::path directory =
        std::filesystem::path(local_app_data) / L"PenumbraVR";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return {};
    return directory / L"black_plague_bootstrap.log";
}

void Log(const char* format, ...) noexcept {
    std::array<char, 1536> message{};
    va_list args;
    va_start(args, format);
    _vsnprintf_s(message.data(), message.size(), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::array<char, 1792> line{};
    _snprintf_s(
        line.data(), line.size(), _TRUNCATE,
        "%04u-%02u-%02u %02u:%02u:%02u.%03u %s\r\n",
        static_cast<unsigned>(time.wYear),
        static_cast<unsigned>(time.wMonth),
        static_cast<unsigned>(time.wDay),
        static_cast<unsigned>(time.wHour),
        static_cast<unsigned>(time.wMinute),
        static_cast<unsigned>(time.wSecond),
        static_cast<unsigned>(time.wMilliseconds),
        message.data());
    OutputDebugStringA(line.data());

    const auto path = BootstrapLogPath();
    if (path.empty()) return;
    HANDLE file = CreateFileW(
        path.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(
        file, line.data(), static_cast<DWORD>(std::strlen(line.data())),
        &written, nullptr);
    CloseHandle(file);
}

BOOL CALLBACK FindGameWindow(HWND window, LPARAM parameter) {
    auto* readiness = reinterpret_cast<GameWindowReadiness*>(parameter);
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != readiness->process_id || !IsWindowVisible(window)) {
        return TRUE;
    }

    wchar_t class_name[64]{};
    if (GetClassNameW(window, class_name, static_cast<int>(std::size(class_name))) <= 0 ||
        std::wcscmp(class_name, L"SDL_app") != 0) {
        return TRUE;
    }

    RECT client{};
    if (!GetClientRect(window, &client) ||
        client.right <= client.left || client.bottom <= client.top) {
        return TRUE;
    }

    readiness->window = window;
    readiness->width = client.right - client.left;
    readiness->height = client.bottom - client.top;
    return FALSE;
}

GameWindowReadiness ReadGameWindow() {
    GameWindowReadiness readiness{};
    readiness.process_id = GetCurrentProcessId();
    EnumWindows(&FindGameWindow, reinterpret_cast<LPARAM>(&readiness));
    return readiness;
}

bool InitializedRenderWorldCallPresent() noexcept {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (base == 0) return false;
    std::array<std::uint8_t, kRenderWorldCall.size()> bytes{};
    SIZE_T bytes_read = 0;
    return ReadProcessMemory(
               GetCurrentProcess(),
               reinterpret_cast<const void*>(base + kRenderWorldCallRva),
               bytes.data(), bytes.size(), &bytes_read) != FALSE &&
        bytes_read == bytes.size() && bytes == kRenderWorldCall;
}

bool WaitForSafeInitialization() noexcept {
    const ULONGLONG deadline = GetTickCount64() + kStartupTimeoutMs;
    bool initialized_code = false;
    GameWindowReadiness previous{};
    std::uint32_t stable_samples = 0;

    do {
        initialized_code = initialized_code || InitializedRenderWorldCallPresent();
        const auto current = ReadGameWindow();
        if (current.window != nullptr) {
            if (current.window == previous.window &&
                current.width == previous.width &&
                current.height == previous.height) {
                ++stable_samples;
            } else {
                previous = current;
                stable_samples = 1;
            }
        } else {
            previous = {};
            stable_samples = 0;
        }

        if (initialized_code && stable_samples >= kRequiredStableWindowSamples) {
            return true;
        }
        Sleep(25);
    } while (GetTickCount64() < deadline);

    return false;
}

using ProbeEntry = DWORD(WINAPI*)(void*);

ProbeEntry ResolveEntry(HMODULE probe, const char* name) noexcept {
    return reinterpret_cast<ProbeEntry>(GetProcAddress(probe, name));
}

DWORD WINAPI BootstrapThread(void*) noexcept {
    const auto host_path = ModulePath(nullptr);
    if (host_path.empty()) {
        Log("Black Plague bootstrap could not resolve the host executable path");
        return 0;
    }

    std::string sha256;
    std::wstring hash_error;
    if (!penumbra_vr::ComputeFileSha256(host_path, sha256, hash_error)) {
        Log("Black Plague bootstrap could not hash host: %s",
            WideToUtf8(hash_error).c_str());
        return 0;
    }
    const auto* build = penumbra_vr::FindKnownBuild(sha256);
    if (build == nullptr || build->game != penumbra_vr::GameId::black_plague ||
        !build->black_plague_probe_allowed) {
        Log("Black Plague bootstrap ignored unsupported host sha256=%s", sha256.c_str());
        return 0;
    }

    Log("Black Plague bootstrap accepted host build=%.*s sha256=%s",
        static_cast<int>(build->id.size()), build->id.data(), sha256.c_str());
    if (!WaitForSafeInitialization()) {
        Log("Black Plague bootstrap timed out waiting for initialized code and stable SDL_app window");
        return 0;
    }

    const auto bootstrap_path = ModulePath(g_instance);
    if (bootstrap_path.empty()) {
        Log("Black Plague bootstrap could not resolve its module directory");
        return 0;
    }
    const auto probe_path =
        bootstrap_path.parent_path() / L"PenumbraVR.BlackPlague.Probe.dll";

    HMODULE probe = GetModuleHandleW(L"PenumbraVR.BlackPlague.Probe.dll");
    const bool owns_probe_reference = probe == nullptr;
    if (probe == nullptr) probe = LoadLibraryW(probe_path.c_str());
    if (probe == nullptr) {
        Log("Black Plague bootstrap failed to load probe path=%s error=%lu",
            WideToUtf8(probe_path.wstring()).c_str(),
            static_cast<unsigned long>(GetLastError()));
        return 0;
    }

    const auto initialize = ResolveEntry(probe, "PenumbraVR_Initialize");
    const auto query = ResolveEntry(probe, "PenumbraVR_QueryCapabilities");
    const auto start = ResolveEntry(probe, "PenumbraVR_StartPresentation");
    const auto shutdown = ResolveEntry(probe, "PenumbraVR_Shutdown");
    if (initialize == nullptr || query == nullptr || start == nullptr || shutdown == nullptr) {
        Log("Black Plague bootstrap found an incomplete probe export surface");
        if (owns_probe_reference) FreeLibrary(probe);
        return 0;
    }

    if (initialize(nullptr) != 1) {
        Log("Black Plague bootstrap: probe initialization failed");
        if (owns_probe_reference) FreeLibrary(probe);
        return 0;
    }

    const std::uint32_t capabilities = query(nullptr);
    if ((capabilities & penumbra_vr::kBlackPlagueProbeRequiredCapabilities) !=
        penumbra_vr::kBlackPlagueProbeRequiredCapabilities) {
        Log("Black Plague bootstrap: required capabilities missing mask=0x%08lX",
            static_cast<unsigned long>(capabilities));
        shutdown(nullptr);
        if (owns_probe_reference) FreeLibrary(probe);
        return 0;
    }

    if (start(nullptr) != 1) {
        Log("Black Plague bootstrap: persistent VR presentation failed to start");
        shutdown(nullptr);
        if (owns_probe_reference) FreeLibrary(probe);
        return 0;
    }

    Log("Black Plague bootstrap started persistent VR presentation capabilities=0x%08lX",
        static_cast<unsigned long>(capabilities));
    return 1;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, &BootstrapThread, nullptr, 0, nullptr);
        if (thread != nullptr) CloseHandle(thread);
    }
    return TRUE;
}
