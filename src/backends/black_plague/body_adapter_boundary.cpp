#include "body_adapter_boundary.hpp"

#include <cstdio>

namespace penumbra_vr::backends::black_plague {
namespace {

template<std::size_t Size>
void Hex(char (&destination)[Size], const std::array<std::uint8_t, 5>& bytes)
    noexcept {
    std::snprintf(destination, Size, "%02X %02X %02X %02X %02X", bytes[0],
        bytes[1], bytes[2], bytes[3], bytes[4]);
}

} // namespace

bool ValidateBodyAdapterBoundary(const BodyAdapterCallsiteBoundary& boundary,
    const char* owner_name, std::string& error) noexcept {
    error.clear();
    if (boundary.owner_installed && boundary.owner_matches_live) return true;
    char expected[16]{};
    char live[16]{};
    Hex(expected, boundary.expected);
    Hex(live, boundary.live);
    char detail[512]{};
    std::snprintf(detail, sizeof(detail),
        "Black Plague body adapter %s owner mismatch: %s @ RVA 0x%05lX "
        "expected pristine [%s] -> 0x%05lX; live [%s] -> 0x%08lX; "
        "%s owner-installed=%u owner-matches-live=%u",
        owner_name, boundary.concern,
        static_cast<unsigned long>(boundary.rva), expected,
        static_cast<unsigned long>(boundary.expected_target), live,
        static_cast<unsigned long>(boundary.live_target), owner_name,
        boundary.owner_installed ? 1U : 0U,
        boundary.owner_matches_live ? 1U : 0U);
    error = detail;
    return false;
}

} // namespace penumbra_vr::backends::black_plague
