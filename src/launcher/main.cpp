#include "penumbra_vr/build_catalog.hpp"
#include "pe_memory_inspector.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

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

std::uintptr_t FindRemoteModuleBase(DWORD process_id, const wchar_t* module_name) {
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

bool WaitForThread(HANDLE thread, DWORD& exit_code, std::wstring& error) {
    const DWORD wait = WaitForSingleObject(thread, 15'000);
    if (wait != WAIT_OBJECT_0) {
        error = wait == WAIT_TIMEOUT ? L"Remote call timed out" : LastErrorText(L"WaitForSingleObject");
        return false;
    }
    if (!GetExitCodeThread(thread, &exit_code)) {
        error = LastErrorText(L"GetExitCodeThread");
        return false;
    }
    return true;
}

bool ResolveRemoteExport(
    const std::filesystem::path& local_module_path,
    std::uintptr_t remote_module_base,
    const char* export_name,
    LPTHREAD_START_ROUTINE& remote_export,
    std::wstring& error) {
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
    DWORD process_id,
    const char* procedure_name,
    LPTHREAD_START_ROUTINE& remote_procedure,
    std::wstring& error) {
    const auto local_kernel = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"kernel32.dll"));
    const auto local_procedure = reinterpret_cast<std::uintptr_t>(
        GetProcAddress(reinterpret_cast<HMODULE>(local_kernel), procedure_name));
    const std::uintptr_t remote_kernel = FindRemoteModuleBase(process_id, L"kernel32.dll");
    if (local_kernel == 0 || local_procedure == 0 || remote_kernel == 0) {
        error = L"Could not resolve a remote kernel32 procedure";
        return false;
    }

    remote_procedure = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        remote_kernel + (local_procedure - local_kernel));
    return true;
}

bool CallRemote(
    HANDLE process,
    LPTHREAD_START_ROUTINE procedure,
    void* parameter,
    DWORD& result,
    std::wstring& error) {
    Handle thread(CreateRemoteThread(process, nullptr, 0, procedure, parameter, 0, nullptr));
    if (!thread.valid()) {
        error = LastErrorText(L"CreateRemoteThread");
        return false;
    }
    return WaitForThread(thread.get(), result, error);
}

bool InjectAndInitialize(
    HANDLE process,
    DWORD process_id,
    const std::filesystem::path& probe_path,
    std::wstring& error) {
    if (FindRemoteModuleBase(process_id, L"PenumbraVR.BlackPlague.Probe.dll") != 0) {
        error = L"The Black Plague probe is already loaded in this process";
        return false;
    }

    const std::wstring probe = probe_path.wstring();
    const SIZE_T byte_count = (probe.size() + 1) * sizeof(wchar_t);
    void* remote_path = VirtualAllocEx(process, nullptr, byte_count, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_path == nullptr) {
        error = LastErrorText(L"VirtualAllocEx");
        return false;
    }

    auto release_remote_path = [&]() { VirtualFreeEx(process, remote_path, 0, MEM_RELEASE); };
    if (!WriteProcessMemory(process, remote_path, probe.c_str(), byte_count, nullptr)) {
        error = LastErrorText(L"WriteProcessMemory");
        release_remote_path();
        return false;
    }

    LPTHREAD_START_ROUTINE remote_load_library = nullptr;
    if (!ResolveRemoteKernelProcedure(process_id, "LoadLibraryW", remote_load_library, error)) {
        release_remote_path();
        return false;
    }

    DWORD remote_probe_base = 0;
    const bool loaded = CallRemote(
        process, remote_load_library, remote_path, remote_probe_base, error);
    release_remote_path();
    if (!loaded || remote_probe_base == 0) {
        if (loaded) {
            error = L"The remote LoadLibraryW call returned NULL";
        }
        return false;
    }

    LPTHREAD_START_ROUTINE remote_initialize = nullptr;
    if (!ResolveRemoteExport(
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
    return true;
}

bool ShutdownAndEject(
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

    LPTHREAD_START_ROUTINE remote_free_library = nullptr;
    if (!ResolveRemoteKernelProcedure(process_id, "FreeLibrary", remote_free_library, error)) {
        return false;
    }

    DWORD free_result = 0;
    if (!CallRemote(
            process,
            remote_free_library,
            reinterpret_cast<void*>(remote_probe),
            free_result,
            error)) {
        return false;
    }
    if (free_result == 0) {
        error = L"The remote FreeLibrary call failed";
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

} // namespace

int wmain(int argc, wchar_t** argv) {
    const bool attach = argc == 3 && _wcsicmp(argv[1], L"--attach") == 0;
    const bool detach = argc == 3 && _wcsicmp(argv[1], L"--detach") == 0;
    const bool inspect = argc == 3 && _wcsicmp(argv[1], L"--inspect") == 0;
    const bool validate_eye_targets =
        argc == 3 && _wcsicmp(argv[1], L"--validate-eye-targets") == 0;
    const bool capture_image = argc == 4 && _wcsicmp(argv[1], L"--capture-image") == 0;
    if ((!attach && !detach && !inspect && !validate_eye_targets &&
         !capture_image && argc != 2) ||
        ((attach || detach || inspect || validate_eye_targets) && argc != 3) ||
        (capture_image && argc != 4)) {
        std::wcerr << L"Usage:\n"
                   << L"  PenumbraVR.ProbeLauncher.exe <path-to-Black-Plague-Penumbra.exe>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --attach <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --detach <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --validate-eye-targets <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --inspect <process-id>\n"
                   << L"  PenumbraVR.ProbeLauncher.exe --capture-image <process-id> <output-path>\n";
        return 2;
    }

    const std::filesystem::path probe_path =
        CurrentExecutableDirectory() / L"PenumbraVR.BlackPlague.Probe.dll";
    if (!inspect && !capture_image && !std::filesystem::is_regular_file(probe_path)) {
        std::wcerr << L"Probe DLL not found beside the launcher: " << probe_path << L'\n';
        return 5;
    }

    if (attach || detach || inspect || validate_eye_targets || capture_image) {
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
            (inspect || capture_image) ? kInspectionAccess : kInjectionAccess,
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
                game_path, !(inspect || capture_image), build, error)) {
            std::wcerr << error << L'\n';
            return 4;
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
        if (detach) {
            if (!ShutdownAndEject(process.get(), parsed_pid, probe_path, error)) {
                std::wcerr << L"Probe removal failed: " << error << L'\n';
                return 8;
            }
            std::wcout << L"Removed the frame probe from "
                       << penumbra_vr::GameDisplayName(build->game) << L" (PID "
                       << parsed_pid << L").\n";
            return 0;
        }

        if (!InjectAndInitialize(process.get(), parsed_pid, probe_path, error)) {
            std::wcerr << L"Probe initialization failed: " << error << L'\n';
            return 8;
        }

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

    if (!InjectAndInitialize(process.get(), process_info.dwProcessId, probe_path, error)) {
        std::wcerr << L"Probe initialization failed: " << error << L'\n';
        TerminateProcess(process.get(), 1);
        WaitForSingleObject(process.get(), 5'000);
        return 9;
    }

    std::wcout << L"Started " << penumbra_vr::GameDisplayName(build->game)
               << L" with the frame probe (PID " << process_info.dwProcessId << L").\n"
               << L"Logs: %LOCALAPPDATA%\\PenumbraVR\\logs\n";
    return 0;
}
