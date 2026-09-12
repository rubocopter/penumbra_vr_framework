#include "rel32_call_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace penumbra_vr::hooks {
namespace {

class SuspendedThreads final {
public:
    SuspendedThreads() = default;
    SuspendedThreads(const SuspendedThreads&) = delete;
    SuspendedThreads& operator=(const SuspendedThreads&) = delete;

    ~SuspendedThreads() {
        for (auto iterator = handles_.rbegin(); iterator != handles_.rend(); ++iterator) {
            ResumeThread(*iterator);
            CloseHandle(*iterator);
        }
    }

    [[nodiscard]] bool SuspendOthers(
        const std::uint8_t* protected_begin,
        std::size_t protected_size,
        std::string& error) {
        const DWORD process_id = GetCurrentProcessId();
        const DWORD current_thread_id = GetCurrentThreadId();
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            error = "CreateToolhelp32Snapshot failed with Win32 error " +
                std::to_string(GetLastError());
            return false;
        }

        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        if (!Thread32First(snapshot, &entry)) {
            const DWORD win32_error = GetLastError();
            CloseHandle(snapshot);
            error = "Thread32First failed with Win32 error " + std::to_string(win32_error);
            return false;
        }

        bool success = true;
        do {
            if (entry.th32OwnerProcessID != process_id ||
                entry.th32ThreadID == current_thread_id) {
                continue;
            }

            HANDLE thread = OpenThread(
                THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                FALSE,
                entry.th32ThreadID);
            if (thread == nullptr) {
                if (GetLastError() == ERROR_INVALID_PARAMETER) {
                    continue;
                }
                error = "OpenThread failed with Win32 error " +
                    std::to_string(GetLastError());
                success = false;
                break;
            }
            if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
                error = "SuspendThread failed with Win32 error " +
                    std::to_string(GetLastError());
                CloseHandle(thread);
                success = false;
                break;
            }

            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(thread, &context)) {
                error = "GetThreadContext failed with Win32 error " +
                    std::to_string(GetLastError());
                ResumeThread(thread);
                CloseHandle(thread);
                success = false;
                break;
            }

            const std::uintptr_t instruction_pointer = context.Eip;
            const std::uintptr_t protected_address =
                reinterpret_cast<std::uintptr_t>(protected_begin);
            if (instruction_pointer >= protected_address &&
                instruction_pointer < protected_address + protected_size) {
                error = "A thread was executing the call instruction; retry the operation";
                ResumeThread(thread);
                CloseHandle(thread);
                success = false;
                break;
            }
            handles_.push_back(thread);
        } while (Thread32Next(snapshot, &entry));

        CloseHandle(snapshot);
        return success;
    }

private:
    std::vector<HANDLE> handles_;
};

[[nodiscard]] bool BuildRel32Instruction(
    const std::uint8_t* instruction,
    const void* target,
    std::uint8_t opcode,
    std::array<std::uint8_t, 5>& bytes,
    std::string& error) noexcept {
    const auto next_instruction = reinterpret_cast<std::intptr_t>(instruction + bytes.size());
    const auto target_address = reinterpret_cast<std::intptr_t>(target);
    const std::int64_t displacement =
        static_cast<std::int64_t>(target_address) - static_cast<std::int64_t>(next_instruction);
    if (displacement < std::numeric_limits<std::int32_t>::min() ||
        displacement > std::numeric_limits<std::int32_t>::max()) {
        error = "The replacement target is outside rel32 range";
        return false;
    }

    bytes[0] = opcode;
    const std::int32_t encoded_displacement = static_cast<std::int32_t>(displacement);
    std::memcpy(bytes.data() + 1, &encoded_displacement, sizeof(encoded_displacement));
    return true;
}

[[nodiscard]] void* DecodeCallTarget(
    const std::uint8_t* instruction) noexcept {
    std::int32_t displacement = 0;
    std::memcpy(&displacement, instruction + 1, sizeof(displacement));
    const std::intptr_t next_instruction =
        reinterpret_cast<std::intptr_t>(instruction + 5);
    return reinterpret_cast<void*>(next_instruction + displacement);
}

[[nodiscard]] bool ReplaceInstruction(
    std::uint8_t* instruction,
    const std::array<std::uint8_t, 5>& expected,
    const std::array<std::uint8_t, 5>& replacement,
    std::string& error) {
    SuspendedThreads suspended;
    if (!suspended.SuspendOthers(instruction, expected.size(), error)) {
        return false;
    }
    if (!std::equal(expected.begin(), expected.end(), instruction)) {
        error = "The call instruction changed before it could be patched";
        return false;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(
            instruction,
            replacement.size(),
            PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        error = "VirtualProtect failed with Win32 error " + std::to_string(GetLastError());
        return false;
    }

    std::memcpy(instruction, replacement.data(), replacement.size());
    FlushInstructionCache(GetCurrentProcess(), instruction, replacement.size());

    DWORD ignored = 0;
    if (!VirtualProtect(instruction, replacement.size(), old_protection, &ignored)) {
        const DWORD restore_error = GetLastError();
        std::memcpy(instruction, expected.data(), expected.size());
        FlushInstructionCache(GetCurrentProcess(), instruction, expected.size());
        VirtualProtect(instruction, expected.size(), old_protection, &ignored);
        error = "Could not restore page protection after patching; Win32 error " +
            std::to_string(restore_error);
        return false;
    }
    return true;
}

} // namespace

bool InstallRel32CallHook(
    std::uint8_t* instruction,
    const std::array<std::uint8_t, 5>& expected_instruction,
    void* replacement_target,
    Rel32CallHook& hook,
    std::string& error) noexcept {
    error.clear();
    if (hook.installed()) {
        error = "The rel32 call is already hooked";
        return false;
    }
    if (instruction == nullptr || replacement_target == nullptr) {
        error = "The call instruction and replacement target must be non-null";
        return false;
    }
    if (expected_instruction[0] != 0xE8) {
        error = "The expected instruction is not a direct rel32 call";
        return false;
    }
    if (!std::equal(expected_instruction.begin(), expected_instruction.end(), instruction)) {
        error = "The live call instruction does not match the version manifest";
        return false;
    }

    std::array<std::uint8_t, 5> replacement_instruction{};
    if (!BuildRel32Instruction(
            instruction,
            replacement_target,
            0xE8,
            replacement_instruction,
            error)) {
        return false;
    }

    Rel32CallHook pending{};
    pending.instruction = instruction;
    pending.original_instruction = expected_instruction;
    pending.replacement_instruction = replacement_instruction;
    pending.original_target = DecodeCallTarget(instruction);
    pending.replacement_target = replacement_target;
    hook = pending;
    if (!ReplaceInstruction(
            instruction,
            pending.original_instruction,
            pending.replacement_instruction,
            error)) {
        hook = {};
        return false;
    }
    return true;
}

bool InstallRel32JumpHook(
    std::uint8_t* instruction,
    const std::array<std::uint8_t, 5>& expected_instruction,
    void* replacement_target,
    Rel32JumpHook& hook,
    std::string& error) noexcept {
    error.clear();
    if (hook.installed()) {
        error = "The rel32 jump is already hooked";
        return false;
    }
    if (instruction == nullptr || replacement_target == nullptr) {
        error = "The jump instruction and replacement target must be non-null";
        return false;
    }
    if (!std::equal(expected_instruction.begin(), expected_instruction.end(),
            instruction)) {
        error = "The live instruction window does not match the version manifest";
        return false;
    }

    std::array<std::uint8_t, 5> replacement_instruction{};
    if (!BuildRel32Instruction(
            instruction,
            replacement_target,
            0xE9,
            replacement_instruction,
            error)) {
        return false;
    }

    Rel32JumpHook pending{};
    pending.instruction = instruction;
    pending.original_instruction = expected_instruction;
    pending.replacement_instruction = replacement_instruction;
    pending.replacement_target = replacement_target;
    hook = pending;
    if (!ReplaceInstruction(
            instruction,
            pending.original_instruction,
            pending.replacement_instruction,
            error)) {
        hook = {};
        return false;
    }
    return true;
}

bool RemoveRel32JumpHook(
    Rel32JumpHook& hook,
    std::string& error) noexcept {
    error.clear();
    if (!hook.installed()) {
        return true;
    }
    if (!ReplaceInstruction(
            hook.instruction,
            hook.replacement_instruction,
            hook.original_instruction,
            error)) {
        return false;
    }
    hook = {};
    return true;
}

bool RemoveRel32CallHook(
    Rel32CallHook& hook,
    std::string& error) noexcept {
    error.clear();
    if (!hook.installed()) {
        return true;
    }
    if (!ReplaceInstruction(
            hook.instruction,
            hook.replacement_instruction,
            hook.original_instruction,
            error)) {
        return false;
    }
    hook = {};
    return true;
}

} // namespace penumbra_vr::hooks
