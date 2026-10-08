#include "log.hpp"
#include "penumbra_vr/build_catalog.hpp"
#include "penumbra_vr/requiem_probe_capabilities.hpp"
#include "penumbra_vr/requiem_probe_lifecycle.hpp"
#include "gameplay_bridge.hpp"
#include "native_vr_settings_menu.hpp"
#include "render_world.hpp"
#include "openvr_session.hpp"
#include "opengl_eye_scissor.hpp"
#include "sdl_frame_hook.hpp"
#include "vr_settings_store.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <string>

namespace {

HINSTANCE g_instance = nullptr;
penumbra_vr::runtime::OpenVrSession g_session;
volatile LONG g_state = 0; // 0 clean, 1 initializing, 2 ready, 3 active, 4 partial.
bool g_render_hook_installed = false;
bool g_gameplay_bridge_owned = false;
bool g_scissor_hook_installed = false;
bool g_swap_hook_installed = false;
bool g_native_menu_owned = false;
penumbra_vr::runtime::VrSettings g_vr_settings;

bool CommitNativeVrSettings(const penumbra_vr::runtime::VrSettings& settings,
    std::string& error) noexcept {
    std::wstring settings_error;
    const auto path = penumbra_vr::launcher::DefaultVrSettingsPath(settings_error);
    if (path.empty() || !penumbra_vr::launcher::SaveVrSettings(
            path, settings, settings_error)) {
        error = "Requiem VR settings could not be saved";
        penumbra_vr::probe::WriteLog("%s", error.c_str());
        return false;
    }
    penumbra_vr::backends::requiem::ConfigureGameplaySettings(settings);
    penumbra_vr::backends::requiem::ConfigurePresentationSettings(settings);
    error.clear();
    return true;
}

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
    if (g_native_menu_owned) {
        if (!penumbra_vr::backends::requiem::RemoveNativeVrSettingsMenu(next)) {
            error = "Native VR menu: " + next;
            return false;
        }
        g_native_menu_owned = false;
    }
    penumbra_vr::backends::requiem::ConnectGameplayInput(nullptr);
    if (g_gameplay_bridge_owned) {
        if (!penumbra_vr::backends::requiem::RemoveGameplayBridge(next)) {
            error = "Gameplay bridge: " + next;
            return false;
        }
        g_gameplay_bridge_owned = false;
    }
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
    penumbra_vr::probe::WriteLog("Requiem probe initialization entered");
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

    std::wstring settings_error;
    auto& vr_settings = g_vr_settings;
    const auto settings_path =
        penumbra_vr::launcher::DefaultVrSettingsPath(settings_error);
    if (settings_path.empty() || !penumbra_vr::launcher::LoadVrSettings(
            settings_path, vr_settings, settings_error)) {
        vr_settings = {};
        penumbra_vr::probe::WriteLog(
            "Requiem VR settings unavailable; using shared defaults");
    }
    penumbra_vr::backends::requiem::ConfigureGameplaySettings(vr_settings);
    penumbra_vr::backends::requiem::ConfigurePresentationSettings(vr_settings);
    penumbra_vr::backends::requiem::ConfigureNativeVrSettingsMenu(
        &g_vr_settings, &CommitNativeVrSettings);
    penumbra_vr::probe::WriteLog(
        "Requiem VR gameplay settings turn_mode=%.*s snap_angle=%.1f smooth_speed=%.1f turn_dead_zone=%.3f crouch_mode=%.*s crouch_depth=%.3f",
        static_cast<int>(penumbra_vr::runtime::ToConfigValue(vr_settings.turn_mode).size()),
        penumbra_vr::runtime::ToConfigValue(vr_settings.turn_mode).data(),
        vr_settings.snap_turn_angle, vr_settings.smooth_turn_speed,
        vr_settings.turn_dead_zone,
        static_cast<int>(penumbra_vr::runtime::ToConfigValue(vr_settings.crouch_mode).size()),
        penumbra_vr::runtime::ToConfigValue(vr_settings.crouch_mode).data(),
        vr_settings.physical_crouch_depth);

    const auto probe_path = ModulePath(g_instance);
    std::string error;
    const auto probe_directory = std::filesystem::path(probe_path).parent_path();
    if (!penumbra_vr::hooks::InstallSdlSwapHook(
            &penumbra_vr::backends::requiem::OnSdlSwap, error)) {
        penumbra_vr::probe::WriteLog(
            "Requiem SDL bootstrap hook install failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    g_swap_hook_installed = true;

    constexpr ULONGLONG kGraphicsBootstrapTimeoutMs = 15'000;
    const ULONGLONG completed_swap_deadline =
        GetTickCount64() + kGraphicsBootstrapTimeoutMs;
    while (!penumbra_vr::RequiemOpenVrBootstrapReady(
               penumbra_vr::hooks::CompletedFrameCount()) &&
           GetTickCount64() < completed_swap_deadline) {
        Sleep(1);
    }
    if (!penumbra_vr::RequiemOpenVrBootstrapReady(
            penumbra_vr::hooks::CompletedFrameCount())) {
        penumbra_vr::probe::WriteLog(
            "Requiem SDL bootstrap timed out before normal-loop readiness; completed=%llu observed=%llu",
            static_cast<unsigned long long>(penumbra_vr::hooks::CompletedFrameCount()),
            static_cast<unsigned long long>(penumbra_vr::hooks::ObservedFrameCount()));
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    penumbra_vr::probe::WriteLog(
        "Requiem SDL bootstrap ready completed=%llu observed=%llu; entering OpenVR initialization",
        static_cast<unsigned long long>(penumbra_vr::hooks::CompletedFrameCount()),
        static_cast<unsigned long long>(penumbra_vr::hooks::ObservedFrameCount()));
    if (probe_path.empty() || !g_session.Initialize(
            (probe_directory / L"openvr_api.dll").wstring(), error)) {
        penumbra_vr::probe::WriteLog("Requiem OpenVR init failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    penumbra_vr::probe::WriteLog("Requiem OpenVR initialization completed");
    std::string input_error;
    const bool input_ready = g_session.InitializeControllerInput(
        (probe_directory / L"vr" / L"actions.json").wstring(), input_error);
    penumbra_vr::probe::WriteLog(
        "Requiem controller actions initialized=%u error=%s",
        input_ready ? 1U : 0U, input_error.c_str());

    g_gameplay_bridge_owned = true;
    if (!penumbra_vr::backends::requiem::InstallGameplayBridge(error)) {
        penumbra_vr::probe::WriteLog(
            "Requiem gameplay bridge install failed: %s", error.c_str());
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
    g_native_menu_owned = true;
    if (!penumbra_vr::backends::requiem::InstallNativeVrSettingsMenu(error)) {
        penumbra_vr::probe::WriteLog("Requiem native VR menu install failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    if (!penumbra_vr::hooks::InstallOpenGlEyeScissor(error)) {
        penumbra_vr::probe::WriteLog("Requiem GL scissor install failed: %s", error.c_str());
        std::string cleanup_error;
        const bool clean = Cleanup(cleanup_error);
        InterlockedExchange(&g_state, clean ? 0 : 4);
        return 0;
    }
    g_scissor_hook_installed = true;
    penumbra_vr::probe::WriteLog(
        "Requiem host accepted; exact gameplay, RenderWorld, GL scissor and SDL swap hooks installed");
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
    penumbra_vr::backends::requiem::ConnectGameplayInput(
        g_session.controller_input_initialized() ? &g_session : nullptr);
    penumbra_vr::probe::WriteLog(
        "Requiem tracked stereo presentation armed; controller locomotion=%u",
        g_session.controller_input_initialized() ? 1U : 0U);
    InterlockedExchange(&g_state, 3);
    return 1;
}

extern "C" DWORD WINAPI PenumbraVR_StopPresentation(void*) noexcept {
    const LONG state = InterlockedCompareExchange(&g_state, 0, 0);
    if (state == 2) return 1;
    if (state != 3 && state != 4) return 0;
    std::string error;
    penumbra_vr::backends::requiem::ConnectGameplayInput(nullptr);
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
