#include "stereo_render_policy.hpp"

#include <iostream>

namespace {

[[nodiscard]] bool NearlyEqual(float left, float right) noexcept {
    const float difference = left > right ? left - right : right - left;
    return difference <= 0.000001F;
}

} // namespace

int main() {
    using penumbra_vr::runtime::PlanStereoWorldRendering;

    constexpr float kFrameTime = 1.0F / 60.0F;
    const auto headset_only =
        PlanStereoWorldRendering(kFrameTime, true, false);
    if (!NearlyEqual(headset_only.eye_frame_times[0], kFrameTime) ||
        !NearlyEqual(headset_only.eye_frame_times[1], 0.0F) ||
        !NearlyEqual(headset_only.monitor_frame_time, 0.0F) ||
        headset_only.render_monitor_world ||
        !headset_only.frame_time_owned_by_first_eye ||
        headset_only.world_pass_count() != 2U) {
        std::cerr << "Headset-only continuous stereo did not produce two timed eye passes\n";
        return 1;
    }

    const auto mirrored =
        PlanStereoWorldRendering(kFrameTime, true, true);
    if (!NearlyEqual(mirrored.eye_frame_times[0], 0.0F) ||
        !NearlyEqual(mirrored.eye_frame_times[1], 0.0F) ||
        !NearlyEqual(mirrored.monitor_frame_time, kFrameTime) ||
        !mirrored.render_monitor_world ||
        mirrored.frame_time_owned_by_first_eye ||
        mirrored.world_pass_count() != 3U) {
        std::cerr << "Mirrored continuous stereo did not preserve the monitor pass\n";
        return 2;
    }

    const auto bounded =
        PlanStereoWorldRendering(kFrameTime, false, false);
    if (!NearlyEqual(bounded.eye_frame_times[0], 0.0F) ||
        !NearlyEqual(bounded.eye_frame_times[1], 0.0F) ||
        !NearlyEqual(bounded.monitor_frame_time, kFrameTime) ||
        !bounded.render_monitor_world ||
        bounded.frame_time_owned_by_first_eye ||
        bounded.world_pass_count() != 3U) {
        std::cerr << "Bounded diagnostics no longer preserve their original world pass\n";
        return 3;
    }

    const auto paused =
        PlanStereoWorldRendering(0.0F, true, false);
    if (paused.world_pass_count() != 2U ||
        !paused.frame_time_owned_by_first_eye ||
        !NearlyEqual(paused.eye_frame_times[0], 0.0F)) {
        std::cerr << "A paused headset-only frame changed render scheduling\n";
        return 4;
    }

    std::cout << "Stereo render scheduling and monitor-mirror policy passed\n";
    return 0;
}
