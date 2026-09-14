#pragma once

#include "vr_settings.hpp"

namespace penumbra_vr::runtime {

struct VrPlayModeStatus {
    bool baseline_known = false;
    float baseline_height = 0.0F;
    float seated_offset = 0.0F;
};

// Rework-derived seated calibration. This owns only tracking-height policy;
// backends retain their native body/posture application boundaries.
class VrPlayModePolicy final {
public:
    [[nodiscard]] VrPlayModeStatus Update(
        VrPlayMode mode,
        float raw_head_height,
        float player_height) noexcept;
    void Reset() noexcept;
    [[nodiscard]] VrPlayModeStatus status() const noexcept;

private:
    float baseline_height_ = 0.0F;
    bool baseline_known_ = false;
    float seated_offset_ = 0.0F;
};

} // namespace penumbra_vr::runtime
