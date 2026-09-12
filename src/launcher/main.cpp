#include "penumbra_vr/build_catalog.hpp"
#include "pe_memory_inspector.hpp"
#include "vr_settings_capabilities.hpp"
#include "penumbra_vr/black_plague_probe_capabilities.hpp"
#include "vr_settings_editor.hpp"
#include "vr_settings_store.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uintptr_t kBlackPlagueRenderWorldCallRva = 0x000EE010;
constexpr std::array<std::uint8_t, 5> kBlackPlagueRenderWorldCall{
    0xE8, 0xFB, 0xEA, 0x03, 0x00,
};

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE value) noexcept : value_(value) {}
    ~Handle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }

private:
    HANDLE value_ = nullptr;
};

std::wstring LastErrorText(const wchar_t* operation) {
    return std::wstring(operation) + L" failed with Win32 error " + std::to_wstring(GetLastError());
}

std::uintptr_t FindRemoteModuleBase(DWORD process_id, const wchar_t* module_name,
    std::filesystem::path* resolved_path = nullptr) {
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id));
    if (!snapshot.valid()) {
        return 0;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snapshot.get(), &entry)) {
        return 0;
    }

    do {
        if (_wcsicmp(entry.szModule, module_name) == 0) {
            if (resolved_path) *resolved_path = entry.szExePath;
            return reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
        }
    } while (Module32NextW(snapshot.get(), &entry));
    return 0;
}

bool WaitForRemoteModule(
    HANDLE process,
    DWORD process_id,
    const wchar_t* module_name,
    DWORD timeout_ms,
    std::wstring& error) {
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    do {
        if (FindRemoteModuleBase(process_id, module_name) != 0) {
            return true;
        }

        DWORD process_exit_code = 0;
        if (!GetExitCodeProcess(process, &process_exit_code)) {
            error = LastErrorText(L"GetExitCodeProcess");
            return false;
        }
        if (process_exit_code != STILL_ACTIVE) {
            error = L"The game exited before its runtime dependencies were loaded";
            return false;
        }
        Sleep(25);
    } while (GetTickCount64() < deadline);

    error = std::wstring(L"Timed out waiting for ") + module_name + L" in the game process";
    return false;
}

bool WaitForBlackPlagueInitializedCode(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& executable_path,
    DWORD timeout_ms,
    std::wstring& error) {
    const ULONGLONG deadline = GetTickCount64() + timeout_ms;
    do {
        const std::uintptr_t module_base = FindRemoteModuleBase(
            process_id, executable_path.filename().c_str());
        if (module_base != 0) {
            std::array<std::uint8_t, kBlackPlagueRenderWorldCall.size()> bytes{};
            SIZE_T bytes_read = 0;
            if (ReadProcessMemory(
                    process,
                    reinterpret_cast<const void*>(
                        module_base + kBlackPlagueRenderWorldCallRva),
                    bytes.data(),
                    bytes.size(),
                    &bytes_read) &&
                bytes_read == bytes.size() &&
                bytes == kBlackPlagueRenderWorldCall) {
                return true;
            }
        }

        DWORD process_exit_code = 0;
        if (!GetExitCodeProcess(process, &process_exit_code)) {
            error = LastErrorText(L"GetExitCodeProcess");
            return false;
        }
        if (process_exit_code != STILL_ACTIVE) {
            error = L"The game exited before its protected code finished initialization";
            return false;
        }
        Sleep(25);
    } while (GetTickCount64() < deadline);

    error = L"Timed out waiting for the exact initialized RenderWorld call bytes";
    return false;
}

bool WaitForThread(HANDLE thread, DWORD& exit_code, std::wstring& error, bool* finished = nullptr) {
    if (finished) *finished = false;
    const DWORD wait = WaitForSingleObject(thread, 15'000);
    if (wait != WAIT_OBJECT_0) {
        error = wait == WAIT_TIMEOUT ? L"Remote call timed out" : LastErrorText(L"WaitForSingleObject");
        return false;
    }
    if (finished) *finished = true;
    if (!GetExitCodeThread(thread, &exit_code)) {
        error = LastErrorText(L"GetExitCodeThread");
        return false;
    }
    return true;
}

bool ResolveRemoteExport(
    DWORD process_id,
    const std::filesystem::path& local_module_path,
    std::uintptr_t remote_module_base,
    const char* export_name,
    LPTHREAD_START_ROUTINE& remote_export,
    std::wstring& error) {
    std::filesystem::path loaded_path;
    const auto loaded_base = FindRemoteModuleBase(process_id, local_module_path.filename().c_str(), &loaded_path);
    std::error_code path_error;
    if (loaded_base != remote_module_base || loaded_path.empty() ||
        !std::filesystem::equivalent(loaded_path, local_module_path, path_error) || path_error) {
        error = L"The loaded probe is from another path/build; restart the game before switching builds";
        return false;
    }
    HMODULE local_module = LoadLibraryExW(
        local_module_path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (local_module == nullptr) {
        error = LastErrorText(L"LoadLibraryExW(module metadata)");
        return false;
    }

    const auto local_export = reinterpret_cast<std::uintptr_t>(
        GetProcAddress(local_module, export_name));
    if (local_export == 0) {
        error = LastErrorText(L"GetProcAddress(remote export)");
        FreeLibrary(local_module);
        return false;
    }

    const std::uintptr_t export_rva =
        local_export - reinterpret_cast<std::uintptr_t>(local_module);
    FreeLibrary(local_module);
    remote_export = reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_module_base + export_rva);
    return true;
}

bool ResolveRemoteKernelProcedure(
    HANDLE process,
    DWORD process_id,
    const char* procedure_name,
    LPTHREAD_START_ROUTINE& remote_procedure,
    std::wstring& error) {
    const auto local_kernel = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"kernel32.dll"));
    const auto local_procedure = reinterpret_cast<std::uintptr_t>(
        GetProcAddress(reinterpret_cast<HMODULE>(local_kernel), procedure_name));
    HMODULE owner = nullptr;
    if (!local_procedure || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(local_procedure), &owner)) {
        error = L"Could not resolve a remote kernel32 procedure";
        return false;
    }
    std::array<wchar_t,32768> owner_path{};
    const DWORD owner_length = GetModuleFileNameW(owner,owner_path.data(),static_cast<DWORD>(owner_path.size()));
    if (!owner_length || owner_length >= owner_path.size()) { error = L"Could not resolve forwarded export owner"; return false; }
    const auto owner_filename = std::filesystem::path(owner_path.data()).filename();
    if (!WaitForRemoteModule(process, process_id, owner_filename.c_str(), 15'000, error)) {
        return false;
    }
    const auto remote_owner = FindRemoteModuleBase(process_id, owner_filename.c_str());
    if (!remote_owner) { error = L"The target process export owner disappeared after initialization"; return false; }
    // Kernel32 exports can resolve into KernelBase. Relocate relative to the
    // actual owner, not to whichever DLL was queried with GetProcAddress.
    remote_procedure = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        remote_owner + (local_procedure - reinterpret_cast<std::uintptr_t>(owner)));
    return true;
}

bool CallRemote(
    HANDLE process,
    LPTHREAD_START_ROUTINE procedure,
    void* parameter,
    DWORD& result,
    std::wstring& error,
    bool* finished = nullptr) {
    if (finished) *finished = true;
    Handle thread(CreateRemoteThread(process, nullptr, 0, procedure, parameter, 0, nullptr));
    if (!thread.valid()) {
        error = LastErrorText(L"CreateRemoteThread");
        return false;
    }
    return WaitForThread(thread.get(), result, error, finished);
}

bool ShutdownAndDeactivate(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error);

bool InjectAndInitialize(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::uint32_t& capabilities,
    std::wstring& error) {
    capabilities = 0;
    std::uintptr_t remote_probe_base = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe_base == 0) {
        const std::wstring probe = probe_path.wstring();
        const SIZE_T byte_count = (probe.size() + 1) * sizeof(wchar_t);
        void* remote_path = VirtualAllocEx(
            process, nullptr, byte_count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remote_path == nullptr) {
            error = LastErrorText(L"VirtualAllocEx");
            return false;
        }

        auto release_remote_path = [&]() {
            VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        };
        if (!WriteProcessMemory(process, remote_path, probe.c_str(), byte_count, nullptr)) {
            error = LastErrorText(L"WriteProcessMemory");
            release_remote_path();
            return false;
        }

        LPTHREAD_START_ROUTINE remote_load_library = nullptr;
        if (!ResolveRemoteKernelProcedure(
                process, process_id, "LoadLibraryW", remote_load_library, error)) {
            release_remote_path();
            return false;
        }

        DWORD loaded_base = 0;
        bool load_finished = false;
        const bool loaded = CallRemote(
            process, remote_load_library, remote_path, loaded_base, error, &load_finished);
        if (load_finished) release_remote_path();
        // A timed-out LoadLibrary thread can still read its argument. Keep the
        // small allocation until process exit instead of causing a remote UAF.
        else error += L"; the loader argument is retained until game exit";
        if (!loaded || loaded_base == 0) {
            if (loaded) {
                error = L"The remote LoadLibraryW call returned NULL";
            }
            return false;
        }
        remote_probe_base = loaded_base;
    }

    LPTHREAD_START_ROUTINE remote_initialize = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path, remote_probe_base, "PenumbraVR_Initialize", remote_initialize, error)) {
        return false;
    }

    DWORD initialize_result = 0;
    if (!CallRemote(process, remote_initialize, nullptr, initialize_result, error)) {
        return false;
    }
    if (initialize_result != 1) {
        error = L"PenumbraVR_Initialize rejected the host or could not install the hook";
        return false;
    }

    const auto fail_after_initialize = [&](std::wstring primary_error) {
        std::wstring shutdown_error;
        if (!ShutdownAndDeactivate(process, process_id, probe_path, shutdown_error)) {
            primary_error += L"; cleanup failed: " + shutdown_error;
        }
        capabilities = 0;
        error = std::move(primary_error);
        return false;
    };

    LPTHREAD_START_ROUTINE remote_query = nullptr;
    if (!ResolveRemoteExport(process_id, probe_path, remote_probe_base,
            "PenumbraVR_QueryCapabilities", remote_query, error)) {
        return fail_after_initialize(error);
    }
    DWORD query_result = 0;
    if (!CallRemote(process, remote_query, nullptr, query_result, error)) {
        return fail_after_initialize(error);
    }
    capabilities = query_result;
    if ((capabilities & penumbra_vr::kBlackPlagueProbeRequiredCapabilities) !=
        penumbra_vr::kBlackPlagueProbeRequiredCapabilities) {
        return fail_after_initialize(
            L"The probe initialized without its required render/frame capabilities");
    }
    return true;
}

void ReportProbeCapabilities(std::uint32_t capabilities) {
    using Capability = penumbra_vr::BlackPlagueProbeCapability;
    const auto state = [capabilities](Capability capability) noexcept {
        return penumbra_vr::HasBlackPlagueProbeCapability(
                   capabilities, capability)
            ? L"ready" : L"unavailable";
    };
    std::wcout << L"Probe capabilities: input=" << state(Capability::native_input)
               << L", body=" << state(Capability::body_collision)
               << L", adapter=" << state(Capability::body_adapter)
               << L", ownership=" << state(Capability::movement_ownership)
               << L", interaction=" << state(Capability::spatial_interaction)
               << L".\n";
}

bool ShutdownAndDeactivate(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"The Black Plague probe is not loaded in this process";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_shutdown = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path, remote_probe, "PenumbraVR_Shutdown", remote_shutdown, error)) {
        return false;
    }

    DWORD shutdown_result = 0;
    if (!CallRemote(process, remote_shutdown, nullptr, shutdown_result, error)) {
        return false;
    }
    if (shutdown_result != 1) {
        error = L"PenumbraVR_Shutdown could not remove the frame hook";
        return false;
    }

    return true;
}

bool InvokeRemotePresentationAction(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    const char* export_name,
    const wchar_t* failure_message,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before changing VR presentation state";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_action = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            export_name,
            remote_action,
            error)) {
        return false;
    }

    DWORD action_result = 0;
    if (!CallRemote(process, remote_action, nullptr, action_result, error)) {
        return false;
    }
    if (action_result != 1) {
        error = failure_message;
        return false;
    }
    return true;
}

bool ValidateRemoteEyeTargets(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before validating eye targets";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_validate = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_ValidateEyeTargets",
            remote_validate,
            error)) {
        return false;
    }

    DWORD validation_result = 0;
    if (!CallRemote(process, remote_validate, nullptr, validation_result, error)) {
        return false;
    }
    if (validation_result != 1) {
        error = L"The in-game eye-target validation failed; inspect the probe log";
        return false;
    }
    return true;
}

bool CreateRemotePersistentEyeTargets(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before creating persistent eye targets";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_create = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_CreatePersistentEyeTargets",
            remote_create,
            error)) {
        return false;
    }

    DWORD creation_result = 0;
    if (!CallRemote(process, remote_create, nullptr, creation_result, error)) {
        return false;
    }
    if (creation_result != 1) {
        error = L"Persistent eye-target creation failed; inspect the probe log";
        return false;
    }
    return true;
}

bool CreateRemoteOpenVrEyeTargets(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before initializing OpenVR";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_create = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_CreateOpenVrEyeTargets",
            remote_create,
            error)) {
        return false;
    }

    DWORD creation_result = 0;
    if (!CallRemote(process, remote_create, nullptr, creation_result, error)) {
        return false;
    }
    if (creation_result != 1) {
        error = L"OpenVR initialization or eye-target creation failed; inspect the probe log";
        return false;
    }
    return true;
}

bool ValidateRemoteWorldDuplication(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before validating world duplication";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_validate = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_ValidateWorldDuplication",
            remote_validate,
            error)) {
        return false;
    }

    DWORD validation_result = 0;
    if (!CallRemote(process, remote_validate, nullptr, validation_result, error)) {
        return false;
    }
    if (validation_result != 1) {
        error = L"Controlled world duplication failed; inspect the probe log";
        return false;
    }
    return true;
}

bool ValidateRemoteStereoMatrices(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before validating stereo matrices";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_validate = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_ValidateStereoMatrices",
            remote_validate,
            error)) {
        return false;
    }

    DWORD validation_result = 0;
    if (!CallRemote(process, remote_validate, nullptr, validation_result, error)) {
        return false;
    }
    if (validation_result != 1) {
        error = L"Controlled stereo-matrix validation failed; inspect the probe log";
        return false;
    }
    return true;
}

bool ValidateRemoteStereoSubmission(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before submitting stereo frames";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_validate = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_ValidateStereoSubmission",
            remote_validate,
            error)) {
        return false;
    }

    DWORD validation_result = 0;
    if (!CallRemote(process, remote_validate, nullptr, validation_result, error)) {
        return false;
    }
    if (validation_result != 1) {
        error = L"Controlled OpenVR stereo submission failed; inspect the probe log";
        return false;
    }
    return true;
}

bool ValidateRemoteTrackedStereoSubmission(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    const std::uintptr_t remote_probe = FindRemoteModuleBase(
        process_id, L"PenumbraVR.BlackPlague.Probe.dll");
    if (remote_probe == 0) {
        error = L"Attach the Black Plague probe before submitting tracked stereo frames";
        return false;
    }

    LPTHREAD_START_ROUTINE remote_validate = nullptr;
    if (!ResolveRemoteExport(
            process_id,
            probe_path,
            remote_probe,
            "PenumbraVR_ValidateTrackedStereoSubmission",
            remote_validate,
            error)) {
        return false;
    }

    DWORD validation_result = 0;
    if (!CallRemote(process, remote_validate, nullptr, validation_result, error)) {
        return false;
    }
    if (validation_result != 1) {
        error = L"Controlled tracked OpenVR stereo submission failed; inspect the probe log";
        return false;
    }
    return true;
}

std::filesystem::path CurrentExecutableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

bool GetProcessExecutablePath(
    HANDLE process,
    DWORD process_id,
    std::filesystem::path& path,
    std::wstring& error) {
    std::wstring buffer(32768, L'\0');
    DWORD length = static_cast<DWORD>(buffer.size());
    if (QueryFullProcessImageNameW(process, 0, buffer.data(), &length)) {
        buffer.resize(length);
        path = buffer;
        return true;
    }

    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id));
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (snapshot.valid() && Module32FirstW(snapshot.get(), &entry)) {
        path = entry.szExePath;
        return true;
    }

    error = LastErrorText(L"QueryFullProcessImageNameW/Module32FirstW");
    return false;
}

bool ReadCameraSnapshot(
    HANDLE process,
    std::uintptr_t camera,
    std::array<float, 16>& view,
    std::array<float, 16>& projection,
    std::array<std::uint8_t, 3>& flags,
    std::wstring& error) {
    SIZE_T bytes_read = 0;
    if (!ReadProcessMemory(
            process,
            reinterpret_cast<const void*>(camera + 0x44),
            view.data(),
            sizeof(view),
            &bytes_read) ||
        bytes_read != sizeof(view)) {
        error = LastErrorText(L"ReadProcessMemory(camera view)");
        return false;
    }
    if (!ReadProcessMemory(
            process,
            reinterpret_cast<const void*>(camera + 0x84),
            projection.data(),
            sizeof(projection),
            &bytes_read) ||
        bytes_read != sizeof(projection)) {
        error = LastErrorText(L"ReadProcessMemory(camera projection)");
        return false;
    }
    if (!ReadProcessMemory(
            process,
            reinterpret_cast<const void*>(camera + 0x8D0),
            flags.data(),
            sizeof(flags),
            &bytes_read) ||
        bytes_read != sizeof(flags)) {
        error = LastErrorText(L"ReadProcessMemory(camera flags)");
        return false;
    }
    return true;
}

void PrintMatrix(const wchar_t* name, const std::array<float, 16>& matrix) {
    std::wcout << name << L":\n";
    for (std::size_t row = 0; row < 4; ++row) {
        std::wcout << L"  [";
        for (std::size_t column = 0; column < 4; ++column) {
            if (column != 0) {
                std::wcout << L", ";
            }
            std::wcout << matrix[row * 4 + column];
        }
        std::wcout << L"]\n";
    }
}

bool ValidateKnownTarget(
    const std::filesystem::path& game_path,
    bool require_black_plague_probe,
    const penumbra_vr::KnownBuild*& build,
    std::wstring& error) {
    std::string sha256;
    if (!penumbra_vr::ComputeFileSha256(game_path.wstring(), sha256, error)) {
        error = L"Could not fingerprint executable: " + error;
        return false;
    }

    build = penumbra_vr::FindKnownBuild(sha256);
    if (build == nullptr) {
        error = L"Refusing to inspect or inject an unknown executable. SHA-256: " +
            std::wstring(sha256.begin(), sha256.end());
        return false;
    }
    if (require_black_plague_probe &&
        (build->game != penumbra_vr::GameId::black_plague ||
         !build->black_plague_probe_allowed)) {
        error = L"This operation is only enabled for the whitelisted Black Plague probe build";
        return false;
    }
    return true;
}

// Match the full executable path, never just "penumbra.exe": the three games
// use that same filename. Refuse ambiguous matches instead of picking a PID.
bool FindRunningGame(const std::filesystem::path& path, DWORD& pid, std::wstring& error) {
    pid = 0;
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!snapshot.valid() || !Process32FirstW(snapshot.get(), &entry)) {
        error = LastErrorText(L"Enumerate game processes");
        return false;
    }
    do {
        if (_wcsicmp(entry.szExeFile, path.filename().c_str()) != 0) continue;
        Handle candidate(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (!candidate.valid()) continue;
        std::wstring image(32768, L'\0');
        DWORD length = static_cast<DWORD>(image.size());
        if (!QueryFullProcessImageNameW(candidate.get(), 0, image.data(), &length)) continue;
        image.resize(length);
        std::error_code ec;
        if (!std::filesystem::equivalent(path, image, ec) || ec) continue;
        if (pid != 0) {
            error = L"More than one matching game is running; close duplicate instances";
            return false;
        }
        pid = entry.th32ProcessID;
    } while (Process32NextW(snapshot.get(), &entry));
    return true;
}

bool StartRemoteVr(HANDLE process, DWORD pid, const std::filesystem::path& probe,
                   std::wstring& error) {
    const auto settings = penumbra_vr::launcher::DefaultVrSettingsPath(error);
    penumbra_vr::runtime::VrSettings profile;
    if (settings.empty() || !penumbra_vr::launcher::LoadVrSettings(settings,profile,error))
        return false;
    const bool mirror=profile.monitor_mirror;
    std::wcout << L"VR settings: " << settings << L"; monitor mirror "
               << (mirror ? L"on" : L"off") << L"; hand "
               << (profile.handedness==penumbra_vr::runtime::VrHandedness::left ? L"left" : L"right")
               << L"; move " << profile.move_speed
               << L"; render " << profile.render_scale
               << L"; UI " << profile.ui_distance << L"m x" << profile.ui_scale
               << L'\n' << std::flush;
    if (!InvokeRemotePresentationAction(process, pid, probe,
        mirror ? "PenumbraVR_EnableMonitorMirror" : "PenumbraVR_DisableMonitorMirror",
        L"Could not apply the saved mirror setting", error) ||
        !InvokeRemotePresentationAction(process, pid, probe, "PenumbraVR_StartPresentation",
        L"Could not start VR; inspect %LOCALAPPDATA%\\PenumbraVR\\logs", error)) return false;
    LPTHREAD_START_ROUTINE query=nullptr;
    const auto module=FindRemoteModuleBase(pid,L"PenumbraVR.BlackPlague.Probe.dll");
    DWORD actual=0;
    if (!module || !ResolveRemoteExport(pid,probe,module,"PenumbraVR_QueryMonitorMirror",query,error) ||
        !CallRemote(process,query,nullptr,actual,error)) return false;
    if (actual!=(mirror ? 1U : 2U)) {
        error=L"VR started, but monitor mirror readback differs from saved settings; inspect the log";
        return false;
    }
    std::wcout << L"Monitor mirror verified in game: " << (mirror ? L"on" : L"off") << L'\n';
    return true;
}

int LaunchVr(const std::filesystem::path& requested, const std::filesystem::path& probe,
             bool check_only) {
    std::error_code ec;
    const auto game = std::filesystem::canonical(requested, ec);
    std::wstring error;
    const penumbra_vr::KnownBuild* build = nullptr;
    if (ec || !ValidateKnownTarget(game, true, build, error)) {
        std::wcerr << L"VR launch preflight failed: " << (ec ? ec.message().c_str() : "")
                   << error << L'\n';
        return 4;
    }
    for (const wchar_t* relative : {L"openvr_api.dll", L"vr/actions.json"}) {
        const auto dependency = probe.parent_path() / relative;
        if (!std::filesystem::is_regular_file(dependency, ec) || ec) {
            std::wcerr << L"Missing VR launch dependency: " << dependency << L'\n';
            return 5;
        }
    }
    // Reading preferences is part of preflight; do not open Steam if corrupt.
    const auto settings = penumbra_vr::launcher::DefaultVrSettingsPath(error);
    penumbra_vr::runtime::VrSettings profile;
    if (settings.empty() || !penumbra_vr::launcher::LoadVrSettings(settings,profile,error)) {
        std::wcerr << error << L'\n';
        return 5;
    }
    if (check_only) {
        std::wcout << L"VR settings: " << settings << L"; monitor mirror "
                   << (profile.monitor_mirror ? L"on" : L"off") << L"; hand "
                   << (profile.handedness==penumbra_vr::runtime::VrHandedness::left ? L"left" : L"right")
                   << L"; move " << profile.move_speed
                   << L"; render " << profile.render_scale
                   << L"; UI " << profile.ui_distance << L"m x" << profile.ui_scale
                   << L'\n';
        std::wcout << L"VR launch preflight passed for " << game
                   << L". No game or SteamVR process was started.\n";
        return 0;
    }
    // Prevent two shortcuts from racing to inject/start the same runtime.
    Handle launch_mutex(CreateMutexW(nullptr, TRUE, L"Local\\PenumbraVR.BlackPlague.Launch"));
    if (!launch_mutex.valid() || GetLastError() == ERROR_ALREADY_EXISTS) {
        std::wcerr << L"Another VR launch is already in progress.\n";
        return 6;
    }
    DWORD pid = 0;
    if (!FindRunningGame(game, pid, error)) {
        std::wcerr << error << L'\n';
        return 6;
    }
    if (pid == 0) {
        // The protected Steam build must be started by Steam, not CreateProcess.
        std::wcout << L"Starting Black Plague through Steam; waiting for the game...\n" << std::flush;
        const auto opened = reinterpret_cast<INT_PTR>(ShellExecuteW(
            nullptr, L"open", L"steam://rungameid/22120", nullptr, nullptr, SW_SHOWNORMAL));
        if (opened <= 32) {
            std::wcerr << L"Steam launch failed (ShellExecute " << opened << L").\n";
            return 6;
        }
        const auto deadline = GetTickCount64() + 60'000;
        do {
            if (!FindRunningGame(game, pid, error)) break;
            if (pid != 0) break;
            Sleep(100);
        } while (GetTickCount64() < deadline);
        if (pid == 0 || !error.empty()) {
            std::wcerr << L"Could not find the requested game process. " << error
                       << L" Check Steam or its game launcher dialog.\n";
            return 6;
        }
    }
    constexpr DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION |
        PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | SYNCHRONIZE;
    Handle process(OpenProcess(access, FALSE, pid));
    std::filesystem::path running_path;
    if (!process.valid() || !GetProcessExecutablePath(process.get(), pid, running_path, error) ||
        !std::filesystem::equivalent(game, running_path, ec) || ec ||
        !ValidateKnownTarget(running_path, true, build, error)) {
        std::wcerr << L"Game process verification failed. " << error << L'\n';
        return 6;
    }
    std::wcout << L"Waiting for runtime initialization, then enabling VR...\n" << std::flush;
    std::uint32_t capabilities = 0;
    if (!WaitForRemoteModule(process.get(), pid, L"SDL.dll", 15'000, error) ||
        !WaitForBlackPlagueInitializedCode(process.get(), pid, game, 15'000, error) ||
        !InjectAndInitialize(process.get(), pid, probe, capabilities, error) ||
        !StartRemoteVr(process.get(), pid, probe, error)) {
        // Never kill an existing game or a user's unsaved session on failure.
        std::wcerr << L"VR startup failed; the game has been left running. " << error << L'\n';
        return 8;
    }
    ReportProbeCapabilities(capabilities);
    std::wcout << L"VR enabled (PID " << pid << L"). Logs: %LOCALAPPDATA%\\PenumbraVR\\logs\n";
    return 0;
}

const wchar_t* VrSettingDisplayName(penumbra_vr::runtime::VrSettingId id) noexcept {
    using penumbra_vr::runtime::VrSettingId;
    switch (id) {
    case VrSettingId::handedness: return L"Handedness";
    case VrSettingId::play_mode: return L"Play mode";
    case VrSettingId::player_height: return L"Player height";
    case VrSettingId::turn_mode: return L"Turn mode";
    case VrSettingId::snap_turn_angle: return L"Snap turn angle";
    case VrSettingId::smooth_turn_speed: return L"Smooth turn speed";
    case VrSettingId::turn_dead_zone: return L"Turn dead zone";
    case VrSettingId::move_speed: return L"Move speed";
    case VrSettingId::move_dead_zone: return L"Move dead zone";
    case VrSettingId::crouch_mode: return L"Crouch mode";
    case VrSettingId::physical_crouch_depth: return L"Physical crouch depth";
    case VrSettingId::height_offset: return L"Height offset";
    case VrSettingId::ui_distance: return L"UI distance";
    case VrSettingId::ui_scale: return L"UI scale";
    case VrSettingId::render_scale: return L"Render scale";
    case VrSettingId::enhanced_visuals: return L"Enhanced visuals";
    case VrSettingId::hrtf: return L"HRTF";
    case VrSettingId::subtitle_scale: return L"Subtitle scale";
    case VrSettingId::count: return L"";
    }
    return L"";
}

std::wstring WidenAscii(std::string_view text) {
    return std::wstring(text.begin(), text.end());
}

int ConfigureBlackPlagueVrSettings() {
    std::wstring error;
    const auto path = penumbra_vr::launcher::DefaultVrSettingsPath(error);
    if (path.empty()) {
        std::wcerr << L"VR settings could not be located: " << error << L'\n';
        return 8;
    }

    penumbra_vr::runtime::VrSettings settings;
    if (!penumbra_vr::launcher::LoadVrSettings(path, settings, error)) {
        std::wcerr << L"VR settings could not be loaded: " << error << L'\n';
        return 8;
    }
    const auto capabilities =
        penumbra_vr::backends::black_plague::BlackPlagueVrSettingCapabilities();

    for (;;) {
        std::vector<penumbra_vr::runtime::VrSettingId> rows;
        std::wcout << L"\nBlack Plague VR settings\n"
                   << L"File: " << path << L"\n"
                   << L"Only settings currently consumed by the backend are shown.\n\n";
        for (const auto& descriptor : penumbra_vr::runtime::VrSettingDescriptors()) {
            if (!penumbra_vr::runtime::IsVrSettingAvailable(
                    descriptor.id, settings, capabilities)) {
                continue;
            }
            rows.push_back(descriptor.id);
            std::wcout << L"  " << rows.size() << L") "
                       << VrSettingDisplayName(descriptor.id) << L": "
                       << WidenAscii(penumbra_vr::runtime::FormatVrSettingValue(
                              descriptor.id, settings))
                       << L'\n';
        }
        const std::size_t mirror_row = rows.size() + 1;
        std::wcout << L"  " << mirror_row << L") Monitor mirror: "
                   << (settings.monitor_mirror ? L"On" : L"Off") << L"\n\n"
                   << L"Select a setting number, R to reset defaults, S to save and exit, "
                   << L"or Q to discard: " << std::flush;

        std::wstring choice;
        if (!std::getline(std::wcin, choice)) {
            std::wcout << L"\nNo changes saved.\n";
            return 0;
        }
        if (_wcsicmp(choice.c_str(), L"q") == 0) {
            std::wcout << L"No changes saved.\n";
            return 0;
        }
        if (_wcsicmp(choice.c_str(), L"r") == 0) {
            penumbra_vr::runtime::ResetVrSettings(settings, capabilities);
            settings.monitor_mirror = false;
            continue;
        }
        if (_wcsicmp(choice.c_str(), L"s") == 0) {
            if (!penumbra_vr::launcher::SaveVrSettings(path, settings, error)) {
                std::wcerr << L"VR settings could not be saved: " << error << L'\n';
                return 9;
            }
            std::wcout << L"Saved VR settings to " << path
                       << L". They will be applied on the next VR start.\n";
            return 0;
        }

        wchar_t* end = nullptr;
        const unsigned long selected = wcstoul(choice.c_str(), &end, 10);
        if (selected == 0 || end == choice.c_str() || *end != L'\0' ||
            selected > mirror_row) {
            std::wcout << L"Invalid selection.\n";
            continue;
        }
        if (selected == mirror_row) {
            settings.monitor_mirror = !settings.monitor_mirror;
            continue;
        }

        const auto id = rows[selected - 1];
        std::wcout << L"Change " << VrSettingDisplayName(id)
                   << L" (- previous / + next, Enter cancels): " << std::flush;
        std::wstring direction;
        if (!std::getline(std::wcin, direction)) {
            std::wcout << L"\nNo changes saved.\n";
            return 0;
        }
        if (direction.empty()) continue;
        const int step = direction.front() == L'-' ? -1 : direction.front() == L'+' ? 1 : 0;
        if (step == 0 || !penumbra_vr::runtime::AdjustVrSetting(settings, id, step)) {
            std::wcout << L"Use - or + for one Rework-compatible menu step.\n";
        }
    }
}

int SetSavedMonitorMirror(bool enabled) {
    std::wstring error;
    const auto path = penumbra_vr::launcher::DefaultVrSettingsPath(error);
    if (path.empty() || !penumbra_vr::launcher::SaveMonitorMirrorSetting(
            path, enabled, error)) {
        std::wcerr << L"VR monitor-mirror setting could not be saved: "
                   << error << L'\n';
        return 9;
    }
    std::wcout << L"Saved VR monitor mirror "
               << (enabled ? L"on" : L"off") << L" to " << path
               << L". It will be applied on the next VR start.\n";
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    const bool configure_vr = argc == 3 &&
        _wcsicmp(argv[1], L"--configure-vr") == 0 &&
        _wcsicmp(argv[2], L"black-plague") == 0;
    const bool set_vr_mirror = argc == 3 &&
        _wcsicmp(argv[1], L"--set-vr-mirror") == 0 &&
        (_wcsicmp(argv[2], L"on") == 0 ||
         _wcsicmp(argv[2], L"off") == 0);
    const bool launch_vr = argc == 3 && _wcsicmp(argv[1], L"--launch-vr") == 0;
    const bool check_vr = argc == 3 && _wcsicmp(argv[1], L"--check-vr") == 0;
    const bool attach = argc == 3 && _wcsicmp(argv[1], L"--attach") == 0;
    const bool detach = argc == 3 && _wcsicmp(argv[1], L"--detach") == 0;
    const bool inspect = argc == 3 && _wcsicmp(argv[1], L"--inspect") == 0;
    const bool validate_eye_targets =
        argc == 3 && _wcsicmp(argv[1], L"--validate-eye-targets") == 0;
    const bool hold_eye_targets =
        argc == 3 && _wcsicmp(argv[1], L"--hold-eye-targets") == 0;
    const bool hold_openvr_eye_targets =
        argc == 3 && _wcsicmp(argv[1], L"--hold-openvr-eye-targets") == 0;
    const bool validate_world_duplication =
        argc == 3 && _wcsicmp(argv[1], L"--validate-world-duplication") == 0;
    const bool validate_stereo_matrices =
        argc == 3 && _wcsicmp(argv[1], L"--validate-stereo-matrices") == 0;
    const bool validate_stereo_submission =
        argc == 3 && _wcsicmp(argv[1], L"--validate-stereo-submission") == 0;
    const bool validate_tracked_stereo_submission =
        argc == 3 &&
        _wcsicmp(argv[1], L"--validate-tracked-stereo-submission") == 0;
    const bool start_vr = argc == 3 && _wcsicmp(argv[1], L"--start-vr") == 0;
    const bool stop_vr = argc == 3 && _wcsicmp(argv[1], L"--stop-vr") == 0;
    const bool vr_mirror_on =
        argc == 3 && _wcsicmp(argv[1], L"--vr-mirror-on") == 0;
    const bool vr_mirror_off =
        argc == 3 && _wcsicmp(argv[1], L"--vr-mirror-off") == 0;
    const bool capture_image = argc == 4 && _wcsicmp(argv[1], L"--capture-image") == 0;
    const bool inspect_camera = argc == 4 && _wcsicmp(argv[1], L"--inspect-camera") == 0;
    if ((!configure_vr && !set_vr_mirror && !attach && !detach && !inspect && !validate_eye_targets && !hold_eye_targets &&
         !hold_openvr_eye_targets && !validate_world_duplication &&
         !validate_stereo_matrices && !validate_stereo_submission &&
         !validate_tracked_stereo_submission && !start_vr && !stop_vr &&
         !vr_mirror_on && !vr_mirror_off && !capture_image && !inspect_camera && !launch_vr && !check_vr &&
         argc != 2) ||
        ((attach || detach || inspect || validate_eye_targets || hold_eye_targets ||
          hold_openvr_eye_targets || validate_world_duplication ||
          validate_stereo_matrices || validate_stereo_submission ||
          validate_tracked_stereo_submission || start_vr || stop_vr ||
          vr_mirror_on || vr_mirror_off) &&
         argc != 3) ||
        ((capture_image || inspect_camera) && argc != 4)) {
        std::wcerr << L"Usage:\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --configure-vr black-plague\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --set-vr-mirror <on|off>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --launch-vr <path-to-Black-Plague-Penumbra.exe>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --check-vr <path-to-Black-Plague-Penumbra.exe>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe <path-to-Black-Plague-Penumbra.exe>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --attach <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --detach <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --validate-eye-targets <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --hold-eye-targets <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --hold-openvr-eye-targets <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --validate-world-duplication <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --validate-stereo-matrices <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --validate-stereo-submission <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --validate-tracked-stereo-submission <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --start-vr <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --stop-vr <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --vr-mirror-on <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --vr-mirror-off <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --inspect <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --inspect-camera <process-id> <address>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --capture-image <process-id> <output-path>\n";
        return 2;
    }

    const std::filesystem::path probe_path =
        CurrentExecutableDirectory() / L"PenumbraVR.BlackPlague.Probe.dll";
    if (!configure_vr && !set_vr_mirror && !inspect && !inspect_camera && !capture_image &&
        !std::filesystem::is_regular_file(probe_path)) {
        std::wcerr << L"Probe DLL not found beside the launcher: " << probe_path << L'\n';
        return 5;
    }

    if (configure_vr) return ConfigureBlackPlagueVrSettings();
    if (set_vr_mirror) return SetSavedMonitorMirror(
        _wcsicmp(argv[2], L"on") == 0);
    if (launch_vr || check_vr) return LaunchVr(argv[2], probe_path, check_vr);

    if (attach || detach || inspect || validate_eye_targets || hold_eye_targets ||
        hold_openvr_eye_targets || validate_world_duplication ||
        validate_stereo_matrices || validate_stereo_submission ||
        validate_tracked_stereo_submission || start_vr || stop_vr ||
        vr_mirror_on || vr_mirror_off ||
        inspect_camera || capture_image) {
        wchar_t* parse_end = nullptr;
        const unsigned long parsed_pid = wcstoul(argv[2], &parse_end, 10);
        if (parsed_pid == 0 || parse_end == argv[2] || *parse_end != L'\0') {
            std::wcerr << L"Invalid process id: " << argv[2] << L'\n';
            return 2;
        }

        constexpr DWORD kInspectionAccess = PROCESS_QUERY_INFORMATION |
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE;
        constexpr DWORD kInjectionAccess = kInspectionAccess | PROCESS_CREATE_THREAD |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
        Handle process(OpenProcess(
            (inspect || inspect_camera || capture_image) ? kInspectionAccess : kInjectionAccess,
            FALSE,
            parsed_pid));
        if (!process.valid()) {
            std::wcerr << LastErrorText(L"OpenProcess") << L'\n';
            return 6;
        }

        std::filesystem::path game_path;
        std::wstring error;
        if (!GetProcessExecutablePath(process.get(), parsed_pid, game_path, error)) {
            std::wcerr << error << L'\n';
            return 6;
        }

        const penumbra_vr::KnownBuild* build = nullptr;
        if (!ValidateKnownTarget(
                game_path, !(inspect || inspect_camera || capture_image), build, error)) {
            std::wcerr << error << L'\n';
            return 4;
        }
        if (inspect_camera) {
            wchar_t* address_end = nullptr;
            const unsigned long parsed_address = wcstoul(argv[3], &address_end, 0);
            if (parsed_address == 0 || address_end == argv[3] || *address_end != L'\0') {
                std::wcerr << L"Invalid camera address: " << argv[3] << L'\n';
                return 2;
            }

            std::array<float, 16> view{};
            std::array<float, 16> projection{};
            std::array<std::uint8_t, 3> flags{};
            if (!ReadCameraSnapshot(
                    process.get(),
                    static_cast<std::uintptr_t>(parsed_address),
                    view,
                    projection,
                    flags,
                    error)) {
                std::wcerr << L"Camera inspection failed: " << error << L'\n';
                return 7;
            }
            std::wcout << L"Read camera "
                       << reinterpret_cast<void*>(static_cast<std::uintptr_t>(parsed_address))
                       << L" from " << penumbra_vr::GameDisplayName(build->game) << L"\n";
            PrintMatrix(L"view (camera+0x44)", view);
            PrintMatrix(L"projection (camera+0x84)", projection);
            std::wcout << L"flags: infinite_far=" << static_cast<unsigned int>(flags[0])
                       << L" view_updated=" << static_cast<unsigned int>(flags[1])
                       << L" projection_updated=" << static_cast<unsigned int>(flags[2])
                       << L'\n';
            return 0;
        }
        if (inspect || capture_image) {
            const std::uintptr_t module_base = FindRemoteModuleBase(
                parsed_pid, game_path.filename().c_str());
            if (module_base == 0) {
                std::wcerr << L"Could not find the main module in the target process.\n";
                return 7;
            }

            penumbra_vr::launcher::TextSectionInspection result;
            std::error_code capture_path_error;
            const std::filesystem::path capture_path = capture_image
                ? std::filesystem::absolute(argv[3], capture_path_error).lexically_normal()
                : std::filesystem::path{};
            if (capture_path_error) {
                std::wcerr << L"Could not resolve the requested capture path.\n";
                return 7;
            }
            if (capture_image && std::filesystem::exists(capture_path)) {
                std::wcerr << L"Refusing to overwrite an existing analysis image: "
                           << capture_path << L'\n';
                return 7;
            }
            if (!penumbra_vr::launcher::InspectRemoteTextSection(
                    process.get(),
                    module_base,
                    game_path,
                    capture_image ? &capture_path : nullptr,
                    result,
                    error)) {
                std::wcerr << L"Memory inspection failed: " << error << L'\n';
                return 7;
            }

            const double changed_percent = result.compared_bytes == 0
                ? 0.0
                : 100.0 * static_cast<double>(result.different_bytes) /
                    static_cast<double>(result.compared_bytes);
            std::wcout << L"Inspected " << penumbra_vr::GameDisplayName(build->game)
                       << L" (PID " << parsed_pid << L")\n"
                       << L".text RVA: 0x" << std::hex << result.rva << std::dec << L'\n'
                       << L"Compared bytes: " << result.compared_bytes << L'\n'
                       << L"Different bytes: " << result.different_bytes << L" ("
                       << changed_percent << L"%)\n"
                       << L"First difference RVA: 0x" << std::hex
                       << result.first_difference_rva << std::dec << L'\n'
                       << L"Disk entropy: " << result.disk_entropy << L" bits/byte\n"
                       << L"Memory entropy: " << result.memory_entropy << L" bits/byte\n";
            if (capture_image) {
                std::wcout << L"Analysis image: " << capture_path << L'\n';
            }
            return 0;
        }
        if (attach && !WaitForRemoteModule(
                process.get(), parsed_pid, L"SDL.dll", 5'000, error)) {
            std::wcerr << L"Attach failed: " << error << L'\n';
            return 7;
        }
        if (attach && !WaitForBlackPlagueInitializedCode(
                process.get(), parsed_pid, game_path, 15'000, error)) {
            std::wcerr << L"Attach failed: " << error << L'\n';
            return 7;
        }
        if (validate_eye_targets) {
            if (!ValidateRemoteEyeTargets(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Eye-target validation failed: " << error << L'\n';
                return 8;
            }
            std::wcout << L"Validated transient OpenGL eye targets in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }
        if (hold_eye_targets) {
            if (!CreateRemotePersistentEyeTargets(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Persistent eye-target creation failed: "
                           << error << L'\n';
                return 8;
            }
            std::wcout << L"Holding persistent diagnostic eye targets in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L"); detach the probe to destroy them.\n";
            return 0;
        }
        if (hold_openvr_eye_targets) {
            if (!CreateRemoteOpenVrEyeTargets(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"OpenVR eye-target creation failed: "
                           << error << L'\n';
                return 8;
            }
            std::wcout << L"Holding OpenVR-sized eye targets in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L"); detach the probe to destroy them and shut down OpenVR.\n";
            return 0;
        }
        if (validate_world_duplication) {
            if (!ValidateRemoteWorldDuplication(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"World-duplication validation failed: "
                           << error << L'\n';
                return 8;
            }
            std::wcout << L"Validated 120 controlled duplicate world passes in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }
        if (validate_stereo_matrices) {
            if (!ValidateRemoteStereoMatrices(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Stereo-matrix validation failed: "
                           << error << L'\n';
                return 8;
            }
            std::wcout << L"Validated 60 controlled stereo-matrix frames in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }
        if (validate_stereo_submission) {
            if (!ValidateRemoteStereoSubmission(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Stereo-submission validation failed: "
                           << error << L'\n';
                return 8;
            }
            std::wcout << L"Submitted 300 controlled static stereo frames in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }
        if (validate_tracked_stereo_submission) {
            if (!ValidateRemoteTrackedStereoSubmission(
                    process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Tracked stereo-submission validation failed: "
                           << error << L'\n';
                return 8;
            }
            std::wcout << L"Submitted 300 controlled stereo frames with recentered "
                       << L"rotation-only head tracking in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }
        if (start_vr) {
            const std::filesystem::path settings_path =
                penumbra_vr::launcher::DefaultVrSettingsPath(error);
            if (settings_path.empty()) {
                std::wcerr << L"VR settings could not be located: " << error << L'\n';
                return 8;
            }
            bool monitor_mirror = false;
            if (!penumbra_vr::launcher::LoadMonitorMirrorSetting(
                    settings_path, monitor_mirror, error)) {
                std::wcerr << L"VR settings could not be loaded: " << error << L'\n';
                return 8;
            }
            if (!InvokeRemotePresentationAction(
                    process.get(),
                    parsed_pid,
                    probe_path,
                    monitor_mirror
                        ? "PenumbraVR_EnableMonitorMirror"
                        : "PenumbraVR_DisableMonitorMirror",
                    L"The persisted monitor-mirror mode could not be applied",
                    error)) {
                std::wcerr << L"VR presentation startup failed: " << error << L'\n';
                return 8;
            }
            if (!InvokeRemotePresentationAction(
                    process.get(),
                    parsed_pid,
                    probe_path,
                    "PenumbraVR_StartPresentation",
                    L"Persistent VR presentation could not start; inspect the probe log",
                    error)) {
                std::wcerr << L"VR presentation startup failed: " << error << L'\n';
                return 8;
            }
            std::wcout << L"Started continuous tracked VR presentation in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L", monitor mirror "
                       << (monitor_mirror ? L"on" : L"off") << L").\n";
            return 0;
        }
        if (stop_vr) {
            if (!InvokeRemotePresentationAction(
                    process.get(),
                    parsed_pid,
                    probe_path,
                    "PenumbraVR_StopPresentation",
                    L"Persistent VR presentation could not stop; inspect the probe log",
                    error)) {
                std::wcerr << L"VR presentation stop failed: " << error << L'\n';
                return 8;
            }
            std::wcout << L"Stopped continuous VR presentation in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }
        if (vr_mirror_on || vr_mirror_off) {
            const std::filesystem::path settings_path =
                penumbra_vr::launcher::DefaultVrSettingsPath(error);
            if (settings_path.empty()) {
                std::wcerr << L"VR monitor-mirror setting could not be located: "
                           << error << L'\n';
                return 8;
            }
            if (!InvokeRemotePresentationAction(
                    process.get(),
                    parsed_pid,
                    probe_path,
                    vr_mirror_on
                        ? "PenumbraVR_EnableMonitorMirror"
                        : "PenumbraVR_DisableMonitorMirror",
                    L"VR monitor-mirror mode could not change; inspect the probe log",
                    error)) {
                std::wcerr << L"VR monitor-mirror update failed: " << error << L'\n';
                return 8;
            }
            if (!penumbra_vr::launcher::SaveMonitorMirrorSetting(
                    settings_path, vr_mirror_on, error)) {
                std::wcerr << L"VR monitor mirror changed in the running game, but "
                           << L"the setting could not be saved: " << error << L'\n';
                return 9;
            }
            std::wcout << L"VR monitor mirror "
                       << (vr_mirror_on ? L"enabled" : L"disabled") << L" in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L") and saved to "
                       << settings_path << L".\n";
            return 0;
        }
        if (detach) {
            if (!ShutdownAndDeactivate(process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Probe removal failed: " << error << L'\n';
                return 8;
            }
            std::wcout << L"Deactivated the frame probe in "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid
                       << L"); its DLL remains resident until the game exits.\n";
            return 0;
        }

        std::uint32_t capabilities = 0;
        if (!InjectAndInitialize(
                process.get(), parsed_pid, probe_path, capabilities, error)) {
            std::wcerr << L"Probe initialization failed: " << error << L'\n';
            return 8;
        }

        ReportProbeCapabilities(capabilities);
        std::wcout << L"Attached the frame probe to " << penumbra_vr::GameDisplayName(build->game)
                   << L" (PID " << parsed_pid << L").\n"
                   << L"Logs: %LOCALAPPDATA%\\PenumbraVR\\logs\n";
        return 0;
    }

    std::error_code filesystem_error;
    const std::filesystem::path game_path = std::filesystem::weakly_canonical(argv[1], filesystem_error);
    if (filesystem_error || !std::filesystem::is_regular_file(game_path)) {
        std::wcerr << L"Executable not found: " << argv[1] << L'\n';
        return 2;
    }

    std::wstring error;
    const penumbra_vr::KnownBuild* build = nullptr;
    if (!ValidateKnownTarget(game_path, true, build, error)) {
        std::wcerr << error << L'\n';
        return 4;
    }

    std::wstring command_line = L"\"" + game_path.wstring() + L"\"";
    std::vector<wchar_t> mutable_command_line(command_line.begin(), command_line.end());
    mutable_command_line.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process_info{};
    if (!CreateProcessW(
            game_path.c_str(),
            mutable_command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED,
            nullptr,
            game_path.parent_path().c_str(),
            &startup,
            &process_info)) {
        std::wcerr << LastErrorText(L"CreateProcessW") << L'\n';
        return 6;
    }

    Handle process(process_info.hProcess);
    Handle primary_thread(process_info.hThread);
    if (ResumeThread(primary_thread.get()) == static_cast<DWORD>(-1)) {
        std::wcerr << LastErrorText(L"ResumeThread") << L'\n';
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), 5'000);
        return 7;
    }

    if (!WaitForRemoteModule(
            process.get(), process_info.dwProcessId, L"SDL.dll", 15'000, error)) {
        std::wcerr << L"Game startup failed: " << error << L'\n';
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), 5'000);
        return 8;
    }
    if (!WaitForBlackPlagueInitializedCode(
            process.get(),
            process_info.dwProcessId,
            game_path,
            15'000,
            error)) {
        std::wcerr << L"Game startup failed: " << error << L'\n';
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), 5'000);
        return 8;
    }

    std::uint32_t capabilities = 0;
    if (!InjectAndInitialize(process.get(), process_info.dwProcessId,
            probe_path, capabilities, error)) {
        std::wcerr << L"Probe initialization failed: " << error << L'\n';
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), 5'000);
        return 9;
    }

    ReportProbeCapabilities(capabilities);
    std::wcout << L"Started " << penumbra_vr::GameDisplayName(build->game)
               << L" with the frame probe (PID " << process_info.dwProcessId << L").\n"
               << L"Logs: %LOCALAPPDATA%\\PenumbraVR\\logs\n";
    return 0;
}
