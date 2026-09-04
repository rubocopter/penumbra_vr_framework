#include "pe_large_address.hpp"

#include <cstring>
#include <limits>

namespace penumbra_vr::deployment {
namespace {

constexpr std::uint16_t kDosSignature = 0x5A4D;
constexpr std::uint32_t kPeSignature = 0x00004550;
constexpr std::uint16_t kMachineI386 = 0x014C;
constexpr std::uint16_t kPe32Magic = 0x010B;
constexpr std::uint16_t kExecutableImage = 0x0002;
constexpr std::uint16_t kLargeAddressAware = 0x0020;
constexpr std::size_t kPeOffsetField = 0x3C;
constexpr std::size_t kCoffHeaderSize = 20;
constexpr std::size_t kCharacteristicsInCoff = 18;

template <typename Value>
[[nodiscard]] bool Read(
    std::span<const std::uint8_t> image,
    std::size_t offset,
    Value& value) noexcept {
    if (offset > image.size() || image.size() - offset < sizeof(value)) {
        return false;
    }
    std::memcpy(&value, image.data() + offset, sizeof(value));
    return true;
}

[[nodiscard]] bool LocateCharacteristics(
    std::span<const std::uint8_t> image,
    std::size_t& characteristics_offset,
    PeLargeAddressStatus& status,
    std::string& error) noexcept {
    status = {};
    error.clear();

    std::uint16_t dos_signature = 0;
    std::uint32_t pe_offset = 0;
    if (!Read(image, 0, dos_signature) || dos_signature != kDosSignature ||
        !Read(image, kPeOffsetField, pe_offset)) {
        error = "The image has no valid DOS/PE header";
        return false;
    }
    if (pe_offset > std::numeric_limits<std::size_t>::max() - 4U -
            kCoffHeaderSize ||
        static_cast<std::size_t>(pe_offset) + 4U + kCoffHeaderSize > image.size()) {
        error = "The PE header lies outside the image";
        return false;
    }

    const std::size_t pe = static_cast<std::size_t>(pe_offset);
    std::uint32_t pe_signature = 0;
    std::uint16_t optional_header_size = 0;
    std::uint16_t optional_magic = 0;
    characteristics_offset = pe + 4U + kCharacteristicsInCoff;
    if (!Read(image, pe, pe_signature) || pe_signature != kPeSignature ||
        !Read(image, pe + 4U, status.machine) ||
        !Read(image, pe + 4U + 16U, optional_header_size) ||
        !Read(image, characteristics_offset, status.characteristics)) {
        error = "The image has an incomplete COFF header";
        return false;
    }

    const std::size_t optional_header = pe + 4U + kCoffHeaderSize;
    if (optional_header_size < sizeof(optional_magic) ||
        !Read(image, optional_header, optional_magic)) {
        error = "The image has no complete optional header";
        return false;
    }

    status.pe32 = optional_magic == kPe32Magic;
    status.executable = (status.characteristics & kExecutableImage) != 0;
    status.large_address_aware =
        (status.characteristics & kLargeAddressAware) != 0;
    if (status.machine != kMachineI386 || !status.pe32 || !status.executable) {
        error = "Large Address Aware patching is restricted to executable x86 PE32 images";
        return false;
    }
    return true;
}

} // namespace

bool InspectPeLargeAddressStatus(
    std::span<const std::uint8_t> image,
    PeLargeAddressStatus& status,
    std::string& error) noexcept {
    std::size_t ignored_offset = 0;
    return LocateCharacteristics(image, ignored_offset, status, error);
}

bool EnablePeLargeAddressAware(
    std::span<std::uint8_t> image,
    bool& changed,
    std::string& error) noexcept {
    changed = false;
    PeLargeAddressStatus status;
    std::size_t characteristics_offset = 0;
    if (!LocateCharacteristics(
            std::span<const std::uint8_t>(image.data(), image.size()),
            characteristics_offset,
            status,
            error)) {
        return false;
    }
    if (status.large_address_aware) {
        return true;
    }

    const std::uint16_t patched =
        static_cast<std::uint16_t>(status.characteristics | kLargeAddressAware);
    std::memcpy(image.data() + characteristics_offset, &patched, sizeof(patched));

    PeLargeAddressStatus verified;
    if (!InspectPeLargeAddressStatus(
            std::span<const std::uint8_t>(image.data(), image.size()),
            verified,
            error) ||
        !verified.large_address_aware) {
        std::memcpy(
            image.data() + characteristics_offset,
            &status.characteristics,
            sizeof(status.characteristics));
        if (error.empty()) {
            error = "The Large Address Aware bit did not survive verification";
        }
        return false;
    }
    changed = true;
    return true;
}

} // namespace penumbra_vr::deployment
