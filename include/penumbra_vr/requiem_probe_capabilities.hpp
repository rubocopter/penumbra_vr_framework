#pragma once

#include <cstdint>

namespace penumbra_vr {

inline constexpr std::uint32_t kRequiemProbeRenderWorld = 1U << 0;
inline constexpr std::uint32_t kRequiemProbeOpenVr = 1U << 1;
inline constexpr std::uint32_t kRequiemProbeRequiredCapabilities =
    kRequiemProbeRenderWorld | kRequiemProbeOpenVr;

} // namespace penumbra_vr
