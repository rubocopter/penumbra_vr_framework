#pragma once

#include "openvr_session.hpp"
#include "vr_math.hpp"

#include <cstdint>
#include <string>

namespace penumbra_vr::backends::requiem {

struct RequiemPresentationTiming final {
    std::uint64_t world_frames = 0;
    std::uint64_t world_render_ticks = 0;
    std::uint64_t ticks_per_second = 0;
};

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
[[nodiscard]] bool TrackedMenuPointer(const runtime::VrHmdPose& pointer_pose,
    std::array<float, 2>& uv) noexcept;

} // namespace penumbra_vr::backends::requiem
