#include "stereo_render_policy.hpp"

#include <iostream>
#include <limits>

namespace {

[[nodiscard]] bool NearlyEqual(float left, float right) noexcept {
    const float difference = left > right ? left - right : right - left;
    return difference <= 0.000001F;
}

} // namespace

int main() {
    using penumbra_vr::runtime::IsFreshPresentationSequence;
    using penumbra_vr::runtime::PlanStereoWorldRendering;
    using penumbra_vr::runtime::SelectVrLogicUpdateRate;
    using penumbra_vr::runtime::ShouldRefreshPresentationSequence;
    using penumbra_vr::runtime::StereoProjectionTangents;
    using penumbra_vr::runtime::UnionStereoProjectionTangents;

    if (SelectVrLogicUpdateRate(72.0F) != 72 ||
        SelectVrLogicUpdateRate(79.98F) != 80 ||
        SelectVrLogicUpdateRate(90.0F) != 90 ||
        SelectVrLogicUpdateRate(120.0F) != 120 ||
        SelectVrLogicUpdateRate(144.0F) != 144 ||
        SelectVrLogicUpdateRate(0.0F) != 90 ||
        SelectVrLogicUpdateRate(1000.0F) != 90 ||
        SelectVrLogicUpdateRate(std::numeric_limits<float>::quiet_NaN()) != 90) {
        std::cerr << "VR logic update-rate selection policy failed\n";
        return 7;
    }

    const StereoProjectionTangents left_eye{-1.00F, -1.20F, 0.85F, 1.10F};
    const StereoProjectionTangents right_eye{-0.90F, -0.80F, 1.30F, 1.00F};
    const auto stereo_union = UnionStereoProjectionTangents(left_eye, right_eye);
    if (!NearlyEqual(stereo_union.top, -1.00F) ||
        !NearlyEqual(stereo_union.left, -1.20F) ||
        !NearlyEqual(stereo_union.right, 1.30F) ||
        !NearlyEqual(stereo_union.bottom, 1.10F)) {
        std::cerr << "Stereo projection union does not cover both eyes\n";
        return 8;
    }

    if (IsFreshPresentationSequence(0, 0) ||
        !IsFreshPresentationSequence(1, 0) ||
        IsFreshPresentationSequence(1, 1) ||
        IsFreshPresentationSequence(4, 5) ||
        !IsFreshPresentationSequence(6, 5)) {
        std::cerr << "Presentation sequence single-consumption policy failed\n";
        return 5;
    }
    if (ShouldRefreshPresentationSequence(0, 0) ||
        ShouldRefreshPresentationSequence(1, 0) ||
        !ShouldRefreshPresentationSequence(1, 1) ||
        !ShouldRefreshPresentationSequence(4, 5) ||
        ShouldRefreshPresentationSequence(6, 5)) {
        std::cerr << "Presentation sequence refresh policy failed\n";
        return 6;
    }

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
