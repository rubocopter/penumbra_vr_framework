#pragma once

#include <string>

namespace penumbra_vr::backends::black_plague {

// Synthetic exact-build harness. Available only in BUILD_TESTING builds and
// never attaches to or modifies a running game process.
[[nodiscard]] bool RunNativeInputBridgeContractHarness(
    std::string& error) noexcept;

} // namespace penumbra_vr::backends::black_plague
