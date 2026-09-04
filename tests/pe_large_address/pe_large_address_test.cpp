#include "pe_large_address.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

template <typename Value>
void Write(std::vector<std::uint8_t>& image, std::size_t offset, Value value) {
    std::memcpy(image.data() + offset, &value, sizeof(value));
}

[[nodiscard]] std::vector<std::uint8_t> MakePe32(std::uint16_t characteristics) {
    std::vector<std::uint8_t> image(512, 0);
    constexpr std::uint32_t pe_offset = 0x80;
    Write<std::uint16_t>(image, 0, 0x5A4D);
    Write<std::uint32_t>(image, 0x3C, pe_offset);
    Write<std::uint32_t>(image, pe_offset, 0x00004550);
    Write<std::uint16_t>(image, pe_offset + 4, 0x014C);
    Write<std::uint16_t>(image, pe_offset + 4 + 16, 0x00E0);
    Write<std::uint16_t>(image, pe_offset + 4 + 18, characteristics);
    Write<std::uint16_t>(image, pe_offset + 24, 0x010B);
    return image;
}

} // namespace

int main() {
    std::string error;
    auto image = MakePe32(0x010F);
    penumbra_vr::deployment::PeLargeAddressStatus status;
    if (!penumbra_vr::deployment::InspectPeLargeAddressStatus(
            image, status, error) ||
        status.large_address_aware || !status.pe32 || !status.executable ||
        status.machine != 0x014C) {
        std::cerr << "Known x86 PE32 image was not inspected correctly: " << error << '\n';
        return 1;
    }

    const auto original = image;
    bool changed = false;
    if (!penumbra_vr::deployment::EnablePeLargeAddressAware(
            image, changed, error) ||
        !changed || image.size() != original.size()) {
        std::cerr << "Could not enable Large Address Aware: " << error << '\n';
        return 2;
    }
    std::size_t changed_bytes = 0;
    for (std::size_t index = 0; index < image.size(); ++index) {
        if (image[index] != original[index]) {
            ++changed_bytes;
        }
    }
    if (changed_bytes != 1 ||
        !penumbra_vr::deployment::InspectPeLargeAddressStatus(
            image, status, error) ||
        !status.large_address_aware) {
        std::cerr << "LAA mutation changed more than its one required bit\n";
        return 3;
    }

    changed = true;
    const auto already_patched = image;
    if (!penumbra_vr::deployment::EnablePeLargeAddressAware(
            image, changed, error) ||
        changed || image != already_patched) {
        std::cerr << "LAA mutation is not idempotent\n";
        return 4;
    }

    std::vector<std::uint8_t> invalid(64, 0);
    if (penumbra_vr::deployment::EnablePeLargeAddressAware(
            invalid, changed, error) || error.empty() || invalid !=
            std::vector<std::uint8_t>(64, 0)) {
        std::cerr << "Invalid input was mutated\n";
        return 5;
    }

    auto x64 = MakePe32(0x010F);
    Write<std::uint16_t>(x64, 0x84, 0x8664);
    if (penumbra_vr::deployment::EnablePeLargeAddressAware(
            x64, changed, error) || error.empty()) {
        std::cerr << "An unsupported machine type was patched\n";
        return 6;
    }

    std::cout << "Transactional Large Address Aware PE transformation passed\n";
    return 0;
}
