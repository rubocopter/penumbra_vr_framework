#include "log.hpp"
#include "penumbra_vr/build_catalog.hpp"
#include "opengl_matrix_telemetry.hpp"
#include "sdl_frame_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace {

volatile LONG g_state = 0;

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "<path conversion failed>";
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void OnFrame(std::uint64_t frame_number) noexcept {
    const penumbra_vr::hooks::OpenGlFrameTelemetry telemetry =
        penumbra_vr::hooks::ConsumeOpenGlFrameTelemetry();
    if (frame_number <= 10 || frame_number % 300 == 0) {
        penumbra_vr::probe::WriteLog(
            "frame=%llu matrix_modes=%lu projection_loads=%lu model_view_loads=%lu "
            "texture_loads=%lu ortho_calls=%lu",
            frame_number,
            static_cast<unsigned long>(telemetry.matrix_mode_calls),
            static_cast<unsigned long>(telemetry.projection_loads),
            static_cast<unsigned long>(telemetry.model_view_loads),
            static_cast<unsigned long>(telemetry.texture_loads),
            static_cast<unsigned long>(telemetry.ortho_calls));
        if (telemetry.has_projection) {
            const auto& m = telemetry.last_projection;
            penumbra_vr::probe::WriteLog(
                "projection=[%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] "
                "[%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f]",
                m[0], m[1], m[2], m[3],
                m[4], m[5], m[6], m[7],
                m[8], m[9], m[10], m[11],
                m[12], m[13], m[14], m[15]);
        }
    }
}

} // namespace

extern "C" DWORD WINAPI PenumbraVR_Initialize(void*) {
    if (InterlockedCompareExchange(&g_state, 1, 0) != 0) {
        return g_state == 2 ? 1UL : 0UL;
    }

    std::wstring log_path;
    std::wstring error;
    if (!penumbra_vr::probe::OpenLog(log_path, error)) {
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    wchar_t executable_path[MAX_PATH]{};
    const DWORD path_length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (path_length == 0 || path_length >= MAX_PATH) {
        penumbra_vr::probe::WriteLog("GetModuleFileNameW failed with Win32 error %lu", GetLastError());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    std::string sha256;
    if (!penumbra_vr::ComputeFileSha256(executable_path, sha256, error)) {
        penumbra_vr::probe::WriteLog("Executable hashing failed: %s", WideToUtf8(error).c_str());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    const penumbra_vr::KnownBuild* build = penumbra_vr::FindKnownBuild(sha256);
    penumbra_vr::probe::WriteLog(
        "Host path=%s sha256=%s",
        WideToUtf8(executable_path).c_str(),
        sha256.c_str());
    if (build == nullptr || build->game != penumbra_vr::GameId::black_plague ||
        !build->black_plague_probe_allowed) {
        penumbra_vr::probe::WriteLog("Refusing to hook an unknown or non-Black-Plague host");
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    std::string hook_error;
    if (!penumbra_vr::hooks::InstallOpenGlMatrixTelemetry(hook_error)) {
        penumbra_vr::probe::WriteLog("OpenGL matrix telemetry failed: %s", hook_error.c_str());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }
    if (!penumbra_vr::hooks::InstallSdlSwapHook(&OnFrame, hook_error)) {
        penumbra_vr::probe::WriteLog("SDL frame hook failed: %s", hook_error.c_str());
        std::string ignored;
        static_cast<void>(penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(ignored));
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    penumbra_vr::probe::WriteLog(
        "Probe initialized build=%.*s log=%s",
        static_cast<int>(build->id.size()),
        build->id.data(),
        WideToUtf8(log_path).c_str());
    InterlockedExchange(&g_state, 2);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_Shutdown(void*) {
    if (InterlockedCompareExchange(&g_state, 3, 2) != 2) {
        return 0;
    }

    std::string error;
    if (!penumbra_vr::hooks::RemoveSdlSwapHook(error)) {
        penumbra_vr::probe::WriteLog("SDL frame hook removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }
    if (!penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(error)) {
        penumbra_vr::probe::WriteLog("OpenGL telemetry removal failed: %s", error.c_str());
        InterlockedExchange(&g_state, 2);
        return 0;
    }

    penumbra_vr::probe::WriteLog(
        "Probe shut down after %llu observed frames",
        penumbra_vr::hooks::ObservedFrameCount());
    penumbra_vr::probe::CloseLog();
    InterlockedExchange(&g_state, 0);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
