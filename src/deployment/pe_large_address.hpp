#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace penumbra_vr::deployment {

struct PeLargeAddressStatus {
    std::uint16_t machine = 0;
    std::uint16_t characteristics = 0;
    bool pe32 = false;
    bool executable = false;
    bool large_address_aware = false;
};

[[nodiscard]] bool InspectPeLargeAddressStatus(
    std::span<const std::uint8_t> image,
    PeLargeAddressStatus& status,
    std::string& error) noexcept;

// Mutates only IMAGE_FILE_LARGE_ADDRESS_AWARE in an already validated PE32
// image. File backup, hash gating and atomic replacement belong to the
// deployment transaction that calls this pure byte transformation.
[[nodiscard]] bool EnablePeLargeAddressAware(
    std::span<std::uint8_t> image,
    bool& changed,
    std::string& error) noexcept;

} // namespace penumbra_vr::deployment
