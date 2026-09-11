#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

struct BodyAdapterCallsiteBoundary {
    const char* concern = "";
    std::uintptr_t rva = 0;
    std::array<std::uint8_t, 5> expected{};
    std::array<std::uint8_t, 5> live{};
    std::uintptr_t expected_target = 0;
    std::uintptr_t live_target = 0;
    bool owner_installed = false;
    bool owner_matches_live = false;
};

// A callsite owner reports pristine manifest data and current patched state.
// The adapter consumes this report; it never owns or writes the call itself.
[[nodiscard]] bool ValidateBodyAdapterBoundary(
    const BodyAdapterCallsiteBoundary& boundary,
    const char* owner_name,
    std::string& error) noexcept;

} // namespace penumbra_vr::backends::black_plague
