#include "stereo_render_policy.hpp"

namespace penumbra_vr::runtime {

std::size_t StereoRenderPlan::world_pass_count() const noexcept {
    return eye_frame_times.size() + (render_monitor_world ? 1U : 0U);
}

StereoRenderPlan PlanStereoWorldRendering(
    float frame_time,
    bool continuous_stereo,
    bool monitor_mirror_enabled) noexcept {
    StereoRenderPlan plan;
    if (continuous_stereo && !monitor_mirror_enabled) {
        plan.eye_frame_times[0] = frame_time;
        plan.render_monitor_world = false;
        plan.frame_time_owned_by_first_eye = true;
        return plan;
    }

    plan.monitor_frame_time = frame_time;
    return plan;
}

} // namespace penumbra_vr::runtime
