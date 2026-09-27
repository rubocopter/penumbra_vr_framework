#pragma once

#include "openvr_session.hpp"
#include "vr_math.hpp"

#include <cstdint>
#include <string>

namespace penumbra_vr::backends::requiem {

struct RequiemPresentationTiming final {
    std::uint64_t world_frames = 0;
    std::uint64_t world_render_ticks = 0;
    std::uint64_t left_eye_ticks = 0;
    std::uint64_t right_eye_ticks = 0;
    std::uint64_t overlay_ticks = 0;
    std::uint64_t submit_ticks = 0;
    std::uint64_t ticks_per_second = 0;
};

struct RequiemInteractionCounters final {
    std::uint64_t selection_refreshes = 0;
    std::uint64_t redirected_rays = 0;
    std::uint64_t ray_hits = 0;
    std::uint64_t ray_winners = 0;
    std::uint64_t native_grab_enters = 0;
    std::uint64_t native_move_enters = 0;
    std::uint64_t grab_enters = 0;
    std::uint64_t grabs_acquired = 0;
    std::uint64_t grabs_released = 0;
    std::uint64_t moves_acquired = 0;
    std::uint64_t moves_released = 0;
    std::uint64_t mechanisms_acquired = 0;
    std::uint64_t tools_attached = 0;
    std::uint64_t tools_native = 0;
    std::uint64_t tools_render_aligned = 0;
};

[[nodiscard]] RequiemInteractionCounters ConsumeInteractionCounters() noexcept;

[[nodiscard]] RequiemPresentationTiming ConsumePresentationTiming() noexcept;

[[nodiscard]] bool InstallRenderWorld(std::string& error) noexcept;
[[nodiscard]] bool RenderHooksInstalled() noexcept;
[[nodiscard]] bool RemoveRenderWorld(std::string& error) noexcept;
[[nodiscard]] bool StartPresentation(runtime::OpenVrSession& session,
    std::string& error) noexcept;
[[nodiscard]] bool StopPresentation(std::string& error) noexcept;
[[nodiscard]] bool TrackedHeadWorldPose(runtime::VrMatrix44& pose) noexcept;
[[nodiscard]] bool TrackedHeadTrackingHeight(float& height) noexcept;
void OnSdlSwap(std::uint64_t frame_number) noexcept;
void RequestTrackedRecenter() noexcept;
void AddTrackedWorldYaw(float radians) noexcept;
void RefreshVrSelectionBeforeInteract(void* player) noexcept;
[[nodiscard]] bool TrackedMenuPointer(const runtime::VrHmdPose& pointer_pose,
    std::array<float, 2>& uv) noexcept;

} // namespace penumbra_vr::backends::requiem
