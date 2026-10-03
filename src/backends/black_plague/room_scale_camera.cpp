#include "room_scale_camera.hpp"
#include "vr_math.hpp"
#include "vr_tracking_space.hpp"

#include <cmath>

namespace penumbra_vr::backends::black_plague {
namespace {
[[nodiscard]] runtime::VrMatrix34 CollapseMatrix(
    const runtime::VrMatrix44& matrix) noexcept {
    runtime::VrMatrix34 result;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            result.values[row * 4U + column] =
                matrix.values[row * 4U + column];
        }
    }
    return result;
}

[[nodiscard]] float TrackingWorldYaw(
    const runtime::VrMatrix44& game_head_view,
    const runtime::VrMatrix34& tracking_anchor) noexcept {
    // For a rigid view, these entries are the world-pose forward X/Z after
    // transposition by the inverse.  This is the same yaw alignment used by
    // ComposeYawRecenteredTrackedHeadView.
    const float game_forward_x = -game_head_view.values[8];
    const float game_forward_z = -game_head_view.values[10];
    const float anchor_forward_x = -tracking_anchor.values[2];
    const float anchor_forward_z = -tracking_anchor.values[10];
    return std::atan2(
        anchor_forward_z * game_forward_x -
            anchor_forward_x * game_forward_z,
        anchor_forward_x * game_forward_x +
            anchor_forward_z * game_forward_z);
}

} // namespace

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
    std::string& error) noexcept {
    world_translation = {};
    render_prediction = {};
    render_head_anchor = {};
    placement_available = false;
    if (!room_scale.enabled || !room_scale.valid) return true;
    const bool identified = room_scale.tracking_identity.sequence != 0;
    if (identified && (room_scale.tracking_identity.pose_epoch == 0 ||
            room_scale.tracking_identity.pose_epoch != current_identity.pose_epoch ||
            room_scale.tracking_identity.yaw_epoch == 0 || current_identity.yaw_epoch == 0)) {
        return true;
    }
    const bool predict_horizontal = !identified || runtime::SameTrackingEpoch(
        room_scale.tracking_identity, current_identity);

    const float tracking_delta_x = current_tracking_pose.values[3] -
        room_scale.observed_tracking_pose.values[3];
    const float tracking_delta_z = current_tracking_pose.values[11] -
        room_scale.observed_tracking_pose.values[11];
    const float tracking_delta_length =
        std::hypot(tracking_delta_x, tracking_delta_z);
    if (!std::isfinite(tracking_delta_length) || tracking_delta_length >
            runtime::vr_locomotion_policy::kMaximumHeadBodySeparation) {
        // A recenter/tracking discontinuity must not be extrapolated into the
        // world before the body owner has rebased the reconciliation sample.
        return true;
    }

    // The native character body advances slower than HMD presentation. Keep
    // between-tick continuation and remove only the component that continues
    // into the last rejected physical direction.
    // Tangential slide and retreat remain render-rate responsive.
    const float world_yaw = TrackingWorldYaw(game_head_view, tracking_anchor);
    const float cosine = std::cos(world_yaw);
    const float sine = std::sin(world_yaw);
    // Turning invalidates the old horizontal prediction basis, not the native
    // feet/world anchor or calibrated height. Falling back to the native camera
    // here made continuous turning repeatedly switch the user's eye height.
    if (predict_horizontal) {
        render_prediction = {
            cosine * tracking_delta_x + sine * tracking_delta_z,
            0.0F,
            -sine * tracking_delta_x + cosine * tracking_delta_z,
        };
        render_prediction = runtime::FilterPhysicalRenderPrediction(
            render_prediction, room_scale.physical_reconciliation);
    }
    render_head_anchor = room_scale.predicted_head_anchor;
    render_head_anchor[0] += render_prediction[0];
    render_head_anchor[2] += render_prediction[2];

    // Rework's player-world pose is the reconciled feet anchor. Its shared
    // tracking transform supplies the physical HMD height continuously and
    // zeroes raw horizontal translation because X/Z is already integrated in
    // render_head_anchor. This also prevents Black Plague's full native
    // crouch-camera drop from being added to a real physical crouch.
    runtime::VrTrackingSpace tracking_space;
    tracking_space.SetHeadTrackingPose(current_tracking_pose);
    tracking_space.SetPlayerWorldPosition(render_head_anchor);
    tracking_space.SetHeightCalibration(
        settings.height_offset);
    const auto play_mode = play_mode_policy.Update(
        settings.play_mode,
        current_tracking_pose.values[7],
        settings.player_height);
    tracking_space.SetSeatedOffset(play_mode.seated_offset);
    tracking_space.SetPostureOffset(settings.posture_offset);
    runtime::VrMatrix44 tracked_head_world;
    if (!tracking_space.HeadWorldPose(tracked_head_world, error)) {
        error = "Could not compose Rework tracking height for Black Plague: " +
            error;
        return false;
    }
    render_head_anchor[1] = tracked_head_world.values[7];

    runtime::VrMatrix44 game_head_pose;
    if (!runtime::InvertRigidTransform(
            CollapseMatrix(game_head_view), game_head_pose, error)) {
        error = "The native Black Plague camera view is not rigid: " + error;
        return false;
    }
    world_translation = {
        render_head_anchor[0] - game_head_pose.values[3],
        render_head_anchor[1] - game_head_pose.values[7],
        render_head_anchor[2] - game_head_pose.values[11],
    };
    if (!std::isfinite(world_translation[0]) ||
        !std::isfinite(world_translation[1]) ||
        !std::isfinite(world_translation[2])) {
        error = "The predicted Black Plague room-scale placement is non-finite";
        return false;
    }
    placement_available = true;
    return true;
}

} // namespace penumbra_vr::backends::black_plague
