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

struct NativeMovementCallsiteStatus {
    std::uintptr_t rva = 0;
    std::array<std::uint8_t, 5> expected{};
    std::array<std::uint8_t, 5> live{};
    std::uintptr_t expected_target = 0;
    std::uintptr_t live_target = 0;
    bool owner_installed = false;
    bool owner_matches_live = false;
};

struct NativeMovementBoundaryStatus {
    bool initialized = false;
    std::array<NativeMovementCallsiteStatus, 2> callsites{};
};

struct NativeBodyUpdateBoundaryStatus {
    bool initialized = false;
    std::array<std::uint8_t, 5> expected{};
    std::array<std::uint8_t, 5> live{};
    std::uintptr_t expected_target = 0;
    std::uintptr_t live_target = 0;
    bool owner_installed = false;
    bool owner_matches_live = false;
};

struct PhysicalBodyDisplacementBoundaryStatus {
    bool initialized = false;
    std::array<std::uint8_t, 5> expected{};
    std::array<std::uint8_t, 5> live{};
    bool owner_installed = false;
    bool owner_matches_live = false;
};

// A callsite owner reports pristine manifest data and current patched state.
// The adapter consumes this report; it never owns or writes the call itself.
[[nodiscard]] bool ValidateBodyAdapterBoundary(
    const BodyAdapterCallsiteBoundary& boundary,
    const char* owner_name,
    std::string& error) noexcept;

// Status and request boundaries implemented by the existing callsite owners.
// Keeping them here lets the body adapter bind without depending on either
// owner's complete public API.
[[nodiscard]] NativeMovementBoundaryStatus
ReadNativeMovementBoundaryStatus() noexcept;
[[nodiscard]] NativeBodyUpdateBoundaryStatus
ReadNativeBodyUpdateBoundaryStatus() noexcept;
// Queue one bounded room-scale X/Z request for the current player body. Y is
// always discarded so the native jump/gravity pipeline retains ownership.
[[nodiscard]] bool QueuePhysicalBodyDisplacement(
    const std::array<float, 3>& displacement) noexcept;
void InvalidatePhysicalBodyDisplacement() noexcept;
[[nodiscard]] PhysicalBodyDisplacementBoundaryStatus
ReadPhysicalBodyDisplacementBoundaryStatus() noexcept;

} // namespace penumbra_vr::backends::black_plague
