#include "render_target_policy.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using penumbra_vr::runtime::RenderTargetPolicy;
using penumbra_vr::runtime::VrRenderTargetSize;

[[nodiscard]] bool Same(
    const VrRenderTargetSize& value,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    return value.width == width && value.height == height;
}

} // namespace

int main() {
    std::vector<VrRenderTargetSize> candidates;
    std::string error;
    if (!penumbra_vr::runtime::BuildRenderTargetCandidates(
            {3400, 3468}, {}, candidates, error) ||
        candidates.size() != 4 ||
        !Same(candidates[0], 3400, 3468) ||
        !Same(candidates[1], 1700, 1734) ||
        !Same(candidates[2], 850, 867) ||
        !Same(candidates[3], 512, 522)) {
        std::cerr << "Default render-target fallback chain is incorrect: " << error << '\n';
        return 1;
    }

    RenderTargetPolicy half_scale;
    half_scale.scale = 0.5F;
    if (!penumbra_vr::runtime::BuildRenderTargetCandidates(
            {3400, 3468}, half_scale, candidates, error) ||
        candidates.empty() || !Same(candidates.front(), 1700, 1734)) {
        std::cerr << "Half render scale was not applied correctly\n";
        return 2;
    }

    RenderTargetPolicy clamped_scale;
    clamped_scale.scale = 4.0F;
    clamped_scale.maximum_attempts = 1;
    if (!penumbra_vr::runtime::BuildRenderTargetCandidates(
            {1000, 1200}, clamped_scale, candidates, error) ||
        candidates.size() != 1 || !Same(candidates.front(), 2000, 2400)) {
        std::cerr << "Render scale was not clamped to the Rework range\n";
        return 3;
    }

    RenderTargetPolicy invalid;
    invalid.scale = std::numeric_limits<float>::quiet_NaN();
    if (penumbra_vr::runtime::BuildRenderTargetCandidates(
            {1000, 1000}, invalid, candidates, error) || error.empty() ||
        !candidates.empty()) {
        std::cerr << "A non-finite render scale was accepted\n";
        return 4;
    }
    if (penumbra_vr::runtime::BuildRenderTargetCandidates(
            {}, {}, candidates, error) || error.empty() || !candidates.empty()) {
        std::cerr << "An empty recommended size was accepted\n";
        return 5;
    }

    std::cout << "VR render scale and fallback policy passed\n";
    return 0;
}
