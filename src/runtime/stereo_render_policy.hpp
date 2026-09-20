#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

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

// A compositor presentation sample is single-use. Reusing the same sequence
// after a successful Submit would submit an eye twice before the next
// WaitGetPoses call and OpenVR reports VRCompositorError_AlreadySubmitted.
[[nodiscard]] bool IsFreshPresentationSequence(
    std::uint64_t sequence,
    std::uint64_t last_submitted_sequence) noexcept;

// UpdateRenderList may run less often than RenderWorld. When the presentation
// sample owned by the last visibility update has already been submitted, the
// render boundary must acquire the next compositor sample instead of falling
// back to a non-VR world pass for that iteration.
[[nodiscard]] bool ShouldRefreshPresentationSequence(
    std::uint64_t sequence,
    std::uint64_t last_submitted_sequence) noexcept;

} // namespace penumbra_vr::runtime
