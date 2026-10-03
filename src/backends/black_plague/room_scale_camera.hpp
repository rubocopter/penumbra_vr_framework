#pragma once

#include "black_plague_body_adapter.hpp"
#include "vr_play_mode_policy.hpp"

namespace penumbra_vr::backends::black_plague {

struct BlackPlagueRoomScaleViewSettings {
    float height_offset = 0.0F;
    runtime::VrPlayMode play_mode = runtime::VrPlayMode::standing;
    float player_height = 1.70F;
    float posture_offset = 0.0F;
};

// Native feet/reconciliation data stay backend-owned. This composition has no
// game calls so camera continuity can be checked without launching Black Plague.
[[nodiscard]] bool ResolveBlackPlagueRoomScalePlacement(
    const runtime::VrMatrix44& game_head_view,
    const runtime::VrMatrix34& tracking_anchor,
    const runtime::VrMatrix34& current_tracking_pose,
    const runtime::VrTrackingSampleIdentity& current_identity,
    const BlackPlagueRoomScaleCameraSample& room_scale,
    const BlackPlagueRoomScaleViewSettings& settings,
    runtime::VrPlayModePolicy& play_mode_policy,
    bool& placement_available,
    std::array<float, 3>& world_translation,
    std::array<float, 3>& render_prediction,
    std::array<float, 3>& render_head_anchor,
    std::string& error) noexcept;

} // namespace penumbra_vr::backends::black_plague
