#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::hooks {

struct Rel32CallHook {
    std::uint8_t* instruction = nullptr;
    std::array<std::uint8_t, 5> original_instruction{};
    std::array<std::uint8_t, 5> replacement_instruction{};
    void* original_target = nullptr;
    void* replacement_target = nullptr;

    [[nodiscard]] bool installed() const noexcept { return instruction != nullptr; }
};

struct Rel32JumpHook {
    std::uint8_t* instruction = nullptr;
    std::array<std::uint8_t, 5> original_instruction{};
    std::array<std::uint8_t, 5> replacement_instruction{};
    void* replacement_target = nullptr;

    [[nodiscard]] bool installed() const noexcept { return instruction != nullptr; }
};

[[nodiscard]] bool InstallRel32CallHook(
    std::uint8_t* instruction,
    const std::array<std::uint8_t, 5>& expected_instruction,
    void* replacement_target,
    Rel32CallHook& hook,
    std::string& error) noexcept;

[[nodiscard]] bool RemoveRel32CallHook(
    Rel32CallHook& hook,
    std::string& error) noexcept;

// Replace one exact five-byte instruction window with a direct rel32 JMP.
// The caller owns any trampoline semantics and the resume address.
[[nodiscard]] bool InstallRel32JumpHook(
    std::uint8_t* instruction,
    const std::array<std::uint8_t, 5>& expected_instruction,
    void* replacement_target,
    Rel32JumpHook& hook,
    std::string& error) noexcept;

[[nodiscard]] bool RemoveRel32JumpHook(
    Rel32JumpHook& hook,
    std::string& error) noexcept;

} // namespace penumbra_vr::hooks
