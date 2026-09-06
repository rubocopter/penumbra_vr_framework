#pragma once

#include "eye_target_probe.hpp"
#include "openvr_session.hpp"
#include "vr_math.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

enum class FramebufferApi : std::uint8_t {
    unavailable,
    core,
    ext,
};

struct RenderWorldFrameTelemetry {
    std::uint32_t calls = 0;
    std::uintptr_t renderer = 0;
    std::uintptr_t world = 0;
    std::uintptr_t camera = 0;
    float frame_time = 0.0F;
    bool has_current_gl_context = false;
    FramebufferApi framebuffer_api = FramebufferApi::unavailable;
    std::array<std::int32_t, 4> viewport{};
    std::array<std::int32_t, 2> max_viewport_dimensions{};
    std::int32_t framebuffer_binding = 0;
    std::int32_t max_texture_size = 0;
    std::int32_t max_renderbuffer_size = 0;
    std::array<char, 64> open_gl_version{};
    std::uint32_t stereo_frames = 0;
    std::uint32_t menu_frames = 0;
    std::uint32_t stereo_eye_passes = 0;
    std::uint32_t eye_scissor_remapped = 0;
    std::uint32_t eye_scissor_bypassed = 0;
    std::uint32_t controller_samples = 0;
    std::uint32_t controller_failures = 0;
    runtime::VrControllerFrame controller_frame;
    std::array<char, 192> controller_error{};
    std::uint64_t stereo_lifetime_frames = 0;
    std::uint32_t compositor_submitted_frames = 0;
    bool compositor_hmd_pose_valid = false;
    std::uint32_t tracked_head_frames = 0;
    bool tracking_anchor_captured = false;
    bool persistent_stereo_active = false;
    bool monitor_mirror_enabled = false;
    std::uint32_t monitor_world_passes = 0;
    std::uint32_t suppressed_monitor_world_passes = 0;
    std::uint32_t eye_owned_frame_time_frames = 0;
    bool stereo_failed = false;
    std::array<char, 192> stereo_error{};
    bool stereo_camera_restored = true;
    std::uint32_t hmd_visibility_updates = 0;
    std::uint32_t hmd_visibility_failures = 0;
    bool hmd_visibility_camera_restored = true;
    std::array<char, 192> hmd_visibility_error{};
    EyeTargetProbeTelemetry eye_targets;
};

[[nodiscard]] bool InstallRenderWorldProbe(std::string& error) noexcept;
[[nodiscard]] bool RemoveRenderWorldProbe(std::string& error) noexcept;
[[nodiscard]] bool ValidateControlledWorldDuplication(
    std::uint32_t frames,
    std::string& error) noexcept;
[[nodiscard]] bool ValidateControlledStereoMatrices(
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::uint32_t frames,
    std::string& error) noexcept;
[[nodiscard]] bool ValidateControlledStereoSubmission(
    runtime::OpenVrSession& session,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::uint32_t frames,
    std::string& error) noexcept;
[[nodiscard]] bool ValidateControlledTrackedStereoSubmission(
    runtime::OpenVrSession& session,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::uint32_t frames,
    std::string& error) noexcept;
[[nodiscard]] bool StartTrackedStereoPresentation(
    runtime::OpenVrSession& session,
    const std::array<runtime::VrEyeConfiguration, 2>& eyes,
    float near_clip,
    std::string& error) noexcept;
[[nodiscard]] bool StopTrackedStereoPresentation(std::string& error) noexcept;
[[nodiscard]] bool TrackedStereoPresentationActive() noexcept;
void PresentTrackedMenuOnRenderThread(bool world_rendered) noexcept;
[[nodiscard]] bool TrackedMenuPointer(const runtime::VrHmdPose& aim, std::array<float, 2>& uv) noexcept;
void RequestTrackedRecenter() noexcept;
[[nodiscard]] bool ControllerWorldPose(const runtime::VrHmdPose& controller,
    runtime::VrMatrix44& pose, std::array<float,3>& velocity, std::array<float,3>& angular) noexcept;
void SetTrackedStereoMonitorMirror(bool enabled) noexcept;
[[nodiscard]] bool TrackedStereoMonitorMirrorEnabled() noexcept;
[[nodiscard]] RenderWorldFrameTelemetry ConsumeRenderWorldFrameTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
