#pragma once

#include "openvr_session.hpp"
#include "vr_settings.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace penumbra_vr::runtime {

struct RenderTargetPolicy {
    float scale = vr_setting_limits::kRenderScale.default_value;
    std::uint32_t minimum_dimension = 512;
    std::uint32_t maximum_attempts = 4;
};

[[nodiscard]] bool BuildRenderTargetCandidates(
    VrRenderTargetSize recommended,
    const RenderTargetPolicy& policy,
    std::vector<VrRenderTargetSize>& candidates,
    std::string& error) noexcept;

} // namespace penumbra_vr::runtime
