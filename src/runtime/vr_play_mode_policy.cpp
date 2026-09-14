#include "vr_play_mode_policy.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {

VrPlayModeStatus VrPlayModePolicy::Update(
    VrPlayMode mode,
    float raw_head_height,
    float player_height) noexcept {
    if (mode == VrPlayMode::standing) {
        Reset();
        return status();
    }

    const bool plausible = std::isfinite(raw_head_height) &&
        raw_head_height > 0.60F && raw_head_height < 1.50F;
    if (!baseline_known_) {
        if (!plausible) {
            seated_offset_ = 0.0F;
            return status();
        }
        baseline_height_ = raw_head_height;
        baseline_known_ = true;
    } else {
        if (!std::isfinite(raw_head_height) ||
            raw_head_height > baseline_height_ + 0.45F) {
            Reset();
            return status();
        }
        if (plausible && raw_head_height < baseline_height_ - 0.10F) {
            baseline_height_ = raw_head_height;
        }
    }

    if (!std::isfinite(player_height)) player_height = 0.0F;
    const float mapped_height = (baseline_height_ - 0.2F) * 1.065F;
    seated_offset_ = std::clamp(
        player_height - mapped_height, 0.0F, 1.5F);
    return status();
}

void VrPlayModePolicy::Reset() noexcept {
    baseline_height_ = 0.0F;
    baseline_known_ = false;
    seated_offset_ = 0.0F;
}

VrPlayModeStatus VrPlayModePolicy::status() const noexcept {
    return {baseline_known_, baseline_height_, seated_offset_};
}

} // namespace penumbra_vr::runtime
