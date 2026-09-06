#pragma once

#include <array>
#include <cstddef>

namespace penumbra_vr::runtime {

struct StereoRenderPlan {
    std::array<float, 2> eye_frame_times{};
    float monitor_frame_time = 0.0F;
    bool render_monitor_world = true;
    bool frame_time_owned_by_first_eye = false;

    [[nodiscard]] std::size_t world_pass_count() const noexcept;
};

// Overture Rework advances renderer time on the first eye when its optional
// monitor mirror is disabled. Keeping that decision independent of a game
// backend makes the frame-time ownership explicit and prevents an accidental
// third full world render in continuous VR.
[[nodiscard]] StereoRenderPlan PlanStereoWorldRendering(
    float frame_time,
    bool continuous_stereo,
    bool monitor_mirror_enabled) noexcept;

} // namespace penumbra_vr::runtime
