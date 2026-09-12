#include "iat_hook.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstring>

namespace penumbra_vr::hooks {
namespace {

bool ReplacePointer(void** slot, void* expected, void* replacement, std::string& error) noexcept {
    DWORD old_protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protection)) {
        error = "VirtualProtect failed with Win32 error " + std::to_string(GetLastError());
        return false;
    }

    void* previous = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(slot), replacement, expected);

    if (previous != expected) {
        DWORD ignored = 0;
        if (!VirtualProtect(slot, sizeof(*slot), old_protection, &ignored)) {
            error = "The import slot changed while installing or removing a hook, and "
                "restoring its page protection failed with Win32 error " +
                std::to_string(GetLastError());
        } else {
            error = "The import slot changed while installing or removing a hook";
        }
        return false;
    }

    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    DWORD ignored = 0;
    if (!VirtualProtect(slot, sizeof(*slot), old_protection, &ignored)) {
        const DWORD restore_error = GetLastError();

        // The page is still writable after a failed restoration. Put the slot
        // back in its expected state so a failed install/remove never reports
        // a transaction that actually remained applied.
        void* rollback_previous = InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(slot), expected, replacement);
        FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));

        DWORD rollback_ignored = 0;
        const bool rollback_protection_restored = VirtualProtect(
            slot, sizeof(*slot), old_protection, &rollback_ignored) != FALSE;
        const DWORD rollback_restore_error = rollback_protection_restored
            ? ERROR_SUCCESS : GetLastError();
        error = "Restoring the import slot page protection failed with Win32 error " +
            std::to_string(restore_error);
        if (rollback_previous != replacement) {
            error += "; the slot also changed before the pointer update could be rolled back";
        } else {
            error += "; the pointer update was rolled back";
        }
        if (!rollback_protection_restored) {
            error += "; restoring protection after rollback failed with Win32 error " +
                std::to_string(rollback_restore_error);
        }
        return false;
    }
    return true;
}

void** FindImport(
    const char* imported_module,
    const char* imported_function,
    std::string& error) noexcept {
    auto* image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        error = "GetModuleHandleW(NULL) failed";
        return nullptr;
    }

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        error = "The host image has no valid DOS header";
        return nullptr;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        error = "The host image is not a valid PE32 image";
        return nullptr;
    }

    const IMAGE_DATA_DIRECTORY& directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (directory.VirtualAddress == 0 || directory.Size < sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        error = "The host image has no import directory";
        return nullptr;
    }

    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image + directory.VirtualAddress);
    for (; descriptor->Name != 0; ++descriptor) {
        const char* module_name = reinterpret_cast<const char*>(image + descriptor->Name);
        if (_stricmp(module_name, imported_module) != 0) {
            continue;
        }

        if (descriptor->OriginalFirstThunk == 0 || descriptor->FirstThunk == 0) {
            error = std::string(imported_module) + " has no named import table suitable for hooking";
            return nullptr;
        }

        auto* names = reinterpret_cast<IMAGE_THUNK_DATA32*>(image + descriptor->OriginalFirstThunk);
        auto* addresses = reinterpret_cast<IMAGE_THUNK_DATA32*>(image + descriptor->FirstThunk);
        for (; names->u1.AddressOfData != 0; ++names, ++addresses) {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal)) {
                continue;
            }

            const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(
                image + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), imported_function) == 0) {
                return reinterpret_cast<void**>(&addresses->u1.Function);
            }
        }

        error = std::string(imported_module) + " is imported, but " + imported_function + " is not";
        return nullptr;
    }

    error = std::string("The host executable does not import ") + imported_module;
    return nullptr;
}

} // namespace

bool InstallPointerHook(void** slot, void* expected, void* replacement,
                        IatHook& hook, std::string& error) noexcept {
    error.clear();
    if (!slot || !expected || !replacement || hook.installed() ||
        reinterpret_cast<std::uintptr_t>(slot) % alignof(void*) != 0) {
        error = "Invalid or already installed pointer hook"; return false;
    }
    if (!ReplacePointer(slot, expected, replacement, error)) return false;
    hook = {slot, expected, replacement};
    return true;
}

bool InstallIatHook(
    const char* imported_module,
    const char* imported_function,
    void* replacement,
    IatHook& hook,
    std::string& error,
    PublishIatOriginal publish_original) noexcept {
    error.clear();
    if (hook.installed()) {
        error = std::string(imported_function) + " is already hooked";
        return false;
    }

    void** slot = FindImport(imported_module, imported_function, error);
    if (slot == nullptr) {
        return false;
    }
    if (*slot == nullptr) {
        error = std::string(imported_function) + " resolved to a null address";
        return false;
    }

    hook.slot = slot;
    hook.original = *slot;
    hook.replacement = replacement;
    if (publish_original != nullptr) {
        publish_original(hook.original);
    }
    if (!ReplacePointer(slot, hook.original, hook.replacement, error)) {
        hook = {};
        return false;
    }
    return true;
}

bool RemoveIatHook(IatHook& hook, std::string& error) noexcept {
    error.clear();
    if (!hook.installed()) {
        return true;
    }
    if (!ReplacePointer(hook.slot, hook.replacement, hook.original, error)) {
        return false;
    }
    hook = {};
    return true;
}

} // namespace penumbra_vr::hooks
