#include "body_adapter_boundary.hpp"

#include <iostream>

namespace penumbra_vr::backends::black_plague {
namespace {

constexpr std::array<std::uint8_t, 5> kForwardPristine{
    0xE8, 0xEE, 0x79, 0x09, 0x00};
constexpr std::array<std::uint8_t, 5> kForwardBridgeHook{
    0xE8, 0x11, 0x22, 0x33, 0x44};

[[nodiscard]] bool RunTest() {
    std::string error;
    // A pristine exact image is not an adapter installation state: the input
    // bridge must first validate and own the original E8 instruction.
    BodyAdapterCallsiteBoundary pristine{"MoveForward", 0x51CD,
        kForwardPristine, kForwardPristine, 0x9CBC0, 0x9CBC0, false, false};
    if (ValidateBodyAdapterBoundary(pristine, "native-input", error) ||
        error.find("RVA 0x051CD") == std::string::npos ||
        error.find("E8 EE 79 09 00") == std::string::npos) return false;

    // The initialized image legitimately contains the bridge's rel32 target.
    BodyAdapterCallsiteBoundary initialized{"MoveForward", 0x51CD,
        kForwardPristine, kForwardBridgeHook, 0x9CBC0, 0x44738234, true, true};
    if (!ValidateBodyAdapterBoundary(initialized, "native-input", error) ||
        !error.empty()) return false;

    // A third-party rewrite remains fail-closed. Adapter fan-out never writes
    // this callsite, so it cannot create a second hook.
    BodyAdapterCallsiteBoundary mismatch = initialized;
    mismatch.live = {0xE9, 0x00, 0x00, 0x00, 0x00};
    mismatch.live_target = 0;
    mismatch.owner_matches_live = false;
    if (ValidateBodyAdapterBoundary(mismatch, "native-input", error) ||
        error.find("owner-matches-live=0") == std::string::npos) return false;

    // Revalidation is idempotent and performs no image write.
    return ValidateBodyAdapterBoundary(initialized, "native-input", error) &&
        ValidateBodyAdapterBoundary(initialized, "native-input", error);
}

} // namespace
} // namespace penumbra_vr::backends::black_plague

int main() {
    if (!penumbra_vr::backends::black_plague::RunTest()) {
        std::cerr << "Black Plague body adapter regression\n";
        return 1;
    }
    return 0;
}
