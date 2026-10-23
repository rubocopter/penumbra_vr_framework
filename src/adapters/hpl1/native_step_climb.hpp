#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace penumbra_vr::adapters::hpl1 {

// Every location and signature is supplied by the exact-build backend. The
// adapter knows only the native success block's stack/register calling contract.
struct NativeStepClimbLayout {
    std::uintptr_t success_rva{};
    std::size_t timestep_stack_offset{};
    std::size_t ray_reference_height_stack_offset{};
    std::size_t ray_distances_stack_offset{};
    std::size_t position_y_offset{};
    std::size_t climb_speed_offset{};
    std::size_t climbing_flag_offset{};
    std::array<std::uint8_t, 26> success_signature{};
};

using NativeStepClimbEligibility = bool (*)(void* character_body) noexcept;

[[nodiscard]] bool InstallNativeStepClimbAdapter(
    std::uint8_t* image, const NativeStepClimbLayout& layout,
    NativeStepClimbEligibility eligible, std::string& error) noexcept;
[[nodiscard]] bool RemoveNativeStepClimbAdapter(std::string& error) noexcept;
[[nodiscard]] bool NativeStepClimbAdapterInstalled() noexcept;
[[nodiscard]] bool NativeStepClimbAdapterMatchesLive() noexcept;

} // namespace penumbra_vr::adapters::hpl1
