#include "log.hpp"
#include "penumbra_vr/build_catalog.hpp"
#include "penumbra_vr/requiem_probe_capabilities.hpp"
#include "render_world.hpp"
#include "openvr_session.hpp"
#include "opengl_eye_scissor.hpp"
#include "sdl_frame_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <string>

namespace {

HINSTANCE g_instance = nullptr;
penumbra_vr::runtime::OpenVrSession g_session;
volatile LONG g_state = 0; // 0 clean, 1 initializing, 2 ready, 3 active, 4 partial.
bool g_render_hook_installed = false;
bool g_scissor_hook_installed = false;
bool g_swap_hook_installed = false;

[[nodiscard]] std::wstring ModulePath(HMODULE module) {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return path;
}

[[nodiscard]] bool Cleanup(std::string& error) noexcept {
    error.clear();
    std::string next;
    if (g_render_hook_installed) {
        if (!penumbra_vr::backends::requiem::StopPresentation(next)) {
            error = "Presentation: " + next;
            return false;
        }
        if (!penumbra_vr::backends::requiem::RemoveRenderWorld(next)) {
            error = "RenderWorld hook: " + next;
            return false;
        }
        g_render_hook_installed = false;
    }
    if (g_scissor_hook_installed) {
        if (!penumbra_vr::hooks::RemoveOpenGlEyeScissor(next)) {
            error = "GL scissor hook: " + next;
            return false;
        }
        g_scissor_hook_installed = false;
    }
    if (g_swap_hook_installed) {
        if (!penumbra_vr::hooks::RemoveSdlSwapHook(next)) {
            error = "SDL swap hook: " + next;
            return false;
        }
        g_swap_hook_installed = false;
    }
    if (g_session.initialized() && !g_session.Shutdown(next)) {
        error = "OpenVR session: " + next;
        return false;
    }
    penumbra_vr::probe::CloseLog();
    return true;
}

} // namespace

extern "C" DWORD WINAPI PenumbraVR_Initialize(void*) noexcept {
    if (InterlockedCompareExchange(&g_state, 1, 0) != 0) return 0;

    std::wstring log_path, log_error;
    if (!penumbra_vr::probe::OpenLog(log_path, log_error, L"requiem")) {
        InterlockedExchange(&g_state, 0);
        return 0;
    }
    const auto host_path = ModulePath(nullptr);
    std::string sha256;
    std::wstring hash_error;
    if (host_path.empty() || !penumbra_vr::ComputeFileSha256(
            host_path, sha256, hash_error)) {
        penumbra_vr::probe::WriteLog("Requiem host hashing failed");
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }
    const auto* build = penumbra_vr::FindKnownBuild(sha256);
    if (build == nullptr || build->game != penumbra_vr::GameId::requiem ||
        build->id != "requiem-steam-observed") {
        penumbra_vr::probe::WriteLog(
            "Requiem probe refused unsupported host sha256=%s", sha256.c_str());
        penumbra_vr::probe::CloseLog();
        InterlockedExchange(&g_state, 0);
        return 0;
    }

    const auto probe_path = ModulePath(g_instance);
    std::string error;
    if (probe_path.empty() || !g_session.Initialize(
            std::filesystem::path(probe_path).parent_path().append(
                L"openvr_api.dll").wstring(), error)) {
        penumbra_vr::probe::WriteLog("Requiem OpenVR init failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    if (!penumbra_vr::backends::requiem::InstallRenderWorld(error)) {
        g_render_hook_installed =
            penumbra_vr::backends::requiem::RenderHooksInstalled();
        penumbra_vr::probe::WriteLog("Requiem renderer install failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    g_render_hook_installed = true;
    if (!penumbra_vr::hooks::InstallOpenGlEyeScissor(error)) {
        penumbra_vr::probe::WriteLog("Requiem GL scissor install failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    g_scissor_hook_installed = true;
    if (!penumbra_vr::hooks::InstallSdlSwapHook(
            &penumbra_vr::backends::requiem::OnSdlSwap, error)) {
        penumbra_vr::probe::WriteLog("Requiem SDL swap install failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    g_swap_hook_installed = true;
    penumbra_vr::probe::WriteLog(
        "Requiem host accepted; exact RenderWorld, GL scissor and SDL swap hooks installed");
    InterlockedExchange(&g_state, 2);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_QueryCapabilities(void*) noexcept {
    const LONG state = InterlockedCompareExchange(&g_state, 0, 0);
    if (state != 2 && state != 3) return 0;
    return penumbra_vr::kRequiemProbeRequiredCapabilities;
}

extern "C" DWORD WINAPI PenumbraVR_StartPresentation(void*) noexcept {
    if (InterlockedCompareExchange(&g_state, 0, 0) != 2) return 0;
    std::string error;
    if (!penumbra_vr::backends::requiem::StartPresentation(g_session, error)) {
        penumbra_vr::probe::WriteLog("Requiem presentation start failed: %s", error.c_str());
        return 0;
    }
    penumbra_vr::probe::WriteLog("Requiem rotational stereo presentation armed");
    InterlockedExchange(&g_state, 3);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_StopPresentation(void*) noexcept {
    const LONG state = InterlockedCompareExchange(&g_state, 0, 0);
    if (state == 2) return 1;
    if (state != 3 && state != 4) return 0;
    std::string error;
    if (!penumbra_vr::backends::requiem::StopPresentation(error)) {
        penumbra_vr::probe::WriteLog("Requiem presentation stop partial: %s", error.c_str());
        InterlockedExchange(&g_state, 4);
        return 0;
    }
    // A partial cleanup may already have removed another dependency. Only a
    // full re-initialization can restore the ready capability state.
    if (state == 3) InterlockedExchange(&g_state, 2);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_Shutdown(void*) noexcept {
    const LONG state = InterlockedCompareExchange(&g_state, 0, 0);
    if (state == 0) return 1;
    if (state == 1) return 0;
    std::string error;
    if (!Cleanup(error)) {
        penumbra_vr::probe::WriteLog("Requiem cleanup partial: %s", error.c_str());
        InterlockedExchange(&g_state, 4);
        return 0;
    }
    InterlockedExchange(&g_state, 0);
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
