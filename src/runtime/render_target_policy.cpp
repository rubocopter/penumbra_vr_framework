#include "render_target_policy.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace penumbra_vr::runtime {
namespace {

[[nodiscard]] bool ScaleDimension(
    std::uint32_t value,
    double scale,
    std::uint32_t& result) noexcept {
    const double scaled = static_cast<double>(value) * scale;
    if (scaled < 1.0 ||
        scaled > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    result = static_cast<std::uint32_t>(scaled + 0.5);
    return result != 0;
}

} // namespace

bool BuildRenderTargetCandidates(
    VrRenderTargetSize recommended,
    const RenderTargetPolicy& policy,
    std::vector<VrRenderTargetSize>& candidates,
    std::string& error) noexcept {
    candidates.clear();
    error.clear();
    if (recommended.width == 0 || recommended.height == 0) {
        error = "The recommended OpenVR render-target size is empty";
        return false;
    }
    if (!std::isfinite(policy.scale)) {
        error = "The VR render scale must be finite";
        return false;
    }
    if (policy.minimum_dimension == 0 || policy.maximum_attempts == 0) {
        error = "The render-target fallback policy must allow a positive size and attempt count";
        return false;
    }

    // Match the proven Rework range. Values outside it are normalized here so
    // malformed configuration cannot request a zero-sized or absurd target.
    const double scale = std::clamp(
        static_cast<double>(policy.scale), 0.5, 2.0);
    VrRenderTargetSize current;
    if (!ScaleDimension(recommended.width, scale, current.width) ||
        !ScaleDimension(recommended.height, scale, current.height)) {
        error = "The scaled OpenVR render-target size is outside the supported range";
        return false;
    }
    candidates.push_back(current);

    while (candidates.size() < policy.maximum_attempts) {
        const std::uint32_t smaller = std::min(current.width, current.height);
        if (smaller <= policy.minimum_dimension) {
            break;
        }

        double fallback_scale = 0.5;
        if (static_cast<double>(smaller) * fallback_scale <
            static_cast<double>(policy.minimum_dimension)) {
            fallback_scale = static_cast<double>(policy.minimum_dimension) /
                static_cast<double>(smaller);
        }

        VrRenderTargetSize next;
        if (!ScaleDimension(current.width, fallback_scale, next.width) ||
            !ScaleDimension(current.height, fallback_scale, next.height) ||
            (next.width == current.width && next.height == current.height)) {
            break;
        }
        candidates.push_back(next);
        current = next;
    }
    return true;
}

} // namespace penumbra_vr::runtime
