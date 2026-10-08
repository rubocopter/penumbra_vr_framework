#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace penumbra_vr {
namespace runtime {

struct StereoProjectionTangents {
    float top = 0.0F;
    float left = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

// Overture's logic timer drives camera/player updates independently from the
// renderer. Matching that cadence to the active HMD avoids regular judder at
// refresh rates such as 72 Hz while keeping the proven 90 Hz fallback when
// the runtime cannot provide a plausible display frequency.
[[nodiscard]] inline int SelectVrLogicUpdateRate(float display_hz) noexcept {
    if (!(display_hz >= 50.0F && display_hz <= 240.0F)) return 90;
    return static_cast<int>(display_hz + 0.5F);
}

// Visibility is built once and shared by both eyes. Use the union of the two
// asymmetric eye projections for that shared culling pass so neither eye can
// inherit the other eye's narrower outer edge.
[[nodiscard]] inline StereoProjectionTangents UnionStereoProjectionTangents(
    const StereoProjectionTangents& left_eye,
    const StereoProjectionTangents& right_eye) noexcept {
    StereoProjectionTangents result;
    result.top = left_eye.top < right_eye.top ? left_eye.top : right_eye.top;
    result.left = left_eye.left < right_eye.left ? left_eye.left : right_eye.left;
    result.right = left_eye.right > right_eye.right ? left_eye.right : right_eye.right;
    result.bottom = left_eye.bottom > right_eye.bottom ? left_eye.bottom : right_eye.bottom;
    return result;
}

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

} // namespace runtime
} // namespace penumbra_vr
