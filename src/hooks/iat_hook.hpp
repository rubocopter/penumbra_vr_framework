#pragma once

#include <string>

namespace penumbra_vr::hooks {

struct IatHook {
    void** slot = nullptr;
    void* original = nullptr;
    void* replacement = nullptr;

    [[nodiscard]] bool installed() const noexcept { return slot != nullptr; }
};

[[nodiscard]] bool InstallIatHook(
    const char* imported_module,
    const char* imported_function,
    void* replacement,
    IatHook& hook,
    std::string& error) noexcept;

[[nodiscard]] bool RemoveIatHook(IatHook& hook, std::string& error) noexcept;

} // namespace penumbra_vr::hooks
