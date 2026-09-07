#include "overture_backend.hpp"

#include "vr_locomotion.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::backends::overture {
namespace {

[[nodiscard]] std::array<float, 3> Translation(
    const runtime::VrMatrix34& matrix) noexcept {
    return {matrix.values[3], matrix.values[7], matrix.values[11]};
}

[[nodiscard]] std::array<float, 3> Subtract(
    const std::array<float, 3>& left,
    const std::array<float, 3>& right) noexcept {
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

void Add(std::array<float, 3>& destination,
         const std::array<float, 3>& value) noexcept {
    destination[0] += value[0];
    destination[1] += value[1];
    destination[2] += value[2];
}

[[nodiscard]] float HorizontalLength(
    const std::array<float, 3>& value) noexcept {
    return std::hypot(value[0], value[2]);
}

[[nodiscard]] float HorizontalDistance(
    const std::array<float, 3>& first,
    const std::array<float, 3>& second) noexcept {
    return std::hypot(first[0] - second[0], first[2] - second[2]);
}

[[nodiscard]] bool FiniteDelta(float delta_seconds) noexcept {
    return std::isfinite(delta_seconds) && delta_seconds >= 0.0F &&
        delta_seconds <= 0.25F;
}

[[nodiscard]] bool FinitePlayerFrame(
    const OverturePlayerFrame& frame) noexcept {
    if (!FiniteDelta(frame.delta_seconds)) {
        return false;
    }
    for (const float value : frame.head_tracking_pose.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool FinitePosition(
    const std::array<float, 3>& position) noexcept {
    return std::isfinite(position[0]) && std::isfinite(position[1]) &&
        std::isfinite(position[2]);
}

} // namespace

void OvertureBackend::SetSettings(runtime::VrSettings settings) noexcept {
    runtime::NormalizeVrSettings(settings);
    settings_ = settings;
    tracking_space_.SetHeightCalibration(settings_.height_offset);
    if (settings_.play_mode == runtime::VrPlayMode::standing) {
        seated_baseline_known_ = false;
        seated_baseline_ = 0.0F;
        tracking_space_.SetSeatedOffset(0.0F);
    }
}

const runtime::VrSettings& OvertureBackend::settings() const noexcept {
    return settings_;
}

bool OvertureBackend::Initialize(
    const runtime::VrMatrix34& head_tracking_pose,
    OvertureBodyAdapter& body,
    std::string& error) noexcept {
    Reset();
    tracking_space_.SetHeadTrackingPose(head_tracking_pose);
    runtime::VrMatrix44 head_world;
    if (!tracking_space_.HeadWorldPose(head_world, error)) {
        return false;
    }
    previous_head_position_ = Translation(head_tracking_pose);
    head_anchor_ = body.BodyPosition();
    head_anchor_[1] = body.FeetHeight();
    if (!FinitePosition(head_anchor_)) {
        error = "The Overture body adapter returned a non-finite position";
        return false;
    }
    tracking_space_.SetHeightCalibration(settings_.height_offset);
    tracking_space_.SetPlayerWorldPosition(head_anchor_);
    UpdatePlayMode(previous_head_position_[1]);
    initialized_ = true;
    error.clear();
    return true;
}

bool OvertureBackend::HandleInput(
    const OvertureInputFrame& frame,
    OvertureBodyAdapter& body,
    std::string& error) noexcept {
    if (!initialized_) {
        error = "The Overture backend has not been initialized";
        return false;
    }
    if (!FiniteDelta(frame.delta_seconds)) {
        error = "The Overture input frame contains invalid timing data";
        return false;
    }

    if (frame.input.recenter.just_pressed) {
        if (!tracking_space_.RecenterOrientation(error)) {
            return false;
        }
        previous_head_position_ = Translation(
            tracking_space_.head_tracking_pose());
    }

    const float yaw_delta = turn_.Update(
        frame.input.turn,
        frame.gameplay_active,
        settings_.turn_mode,
        frame.delta_seconds,
        settings_.snap_turn_angle,
        settings_.smooth_turn_speed,
        settings_.turn_dead_zone);
    // Rework's stick convention turns positive X clockwise in world space.
    tracking_space_.AddWorldYaw(-yaw_delta);

    if (frame.gameplay_active) {
        if (frame.input.jump.just_pressed) {
            body.StartJump();
        }
        body.SetJumpHeld(frame.input.jump.pressed);
        pending_input_ = frame.input;
    } else {
        body.SetJumpHeld(false);
        pending_input_ = {};
    }
    error.clear();
    return true;
}

bool OvertureBackend::UpdatePlayer(
    const OverturePlayerFrame& frame,
    OvertureBodyAdapter& body,
    OvertureFrameResult& result,
    std::string& error) noexcept {
    result = {};
    if (!initialized_) {
        error = "The Overture backend has not been initialized";
        return false;
    }
    if (!FinitePlayerFrame(frame)) {
        error = "The Overture frame contains invalid tracking or timing data";
        return false;
    }

    tracking_space_.SetHeadTrackingPose(frame.head_tracking_pose);
    runtime::VrMatrix44 validated_head_world;
    if (!tracking_space_.HeadWorldPose(validated_head_world, error)) {
        return false;
    }
    UpdatePlayMode(frame.head_tracking_pose.values[7]);

    std::array<float, 3> body_position = body.BodyPosition();
    if (!FinitePosition(body_position)) {
        error = "The Overture body adapter returned a non-finite position";
        return false;
    }
    if (HorizontalDistance(body_position, head_anchor_) >
        runtime::vr_locomotion_policy::kMaximumHeadBodySeparation) {
        head_anchor_ = body_position;
        result.head_anchor_rebased = true;
    }

    const std::array<float, 3> current_head =
        Translation(frame.head_tracking_pose);
    std::array<float, 3> tracking_delta =
        Subtract(current_head, previous_head_position_);
    tracking_delta[1] = 0.0F;
    tracking_delta = tracking_space_.TrackingDirectionToWorld(tracking_delta);
    previous_head_position_ = current_head;
    Add(head_anchor_, tracking_delta);

    if (frame.body_motion_enabled) {
        if (HorizontalLength(tracking_delta) > 0.0F) {
            const std::array<float, 3> request =
                runtime::ClampPhysicalBodyStep(head_anchor_, body_position);
            result.requested_room_scale_distance = HorizontalLength(request);
            if (result.requested_room_scale_distance > 0.0F) {
                const std::array<float, 3> body_after = body.MoveBodyBy(
                    request, BodyMoveKind::room_scale_static_only);
                if (!FinitePosition(body_after)) {
                    error = "The Overture body adapter returned a non-finite room-scale result";
                    return false;
                }
                const std::array<float, 3> accepted =
                    Subtract(body_after, body_position);
                const float accepted_distance =
                    runtime::AcceptedDistanceAlongRequest(request, accepted);
                result.rejected_room_scale_distance =
                    result.requested_room_scale_distance - accepted_distance;
                if (result.rejected_room_scale_distance >
                    runtime::vr_locomotion_policy::kRejectedMotionEpsilon) {
                    const float inverse_length =
                        1.0F / result.requested_room_scale_distance;
                    head_anchor_[0] -= request[0] * inverse_length *
                        result.rejected_room_scale_distance;
                    head_anchor_[2] -= request[2] * inverse_length *
                        result.rejected_room_scale_distance;
                }
                body_position = body_after;
            }
        }

        runtime::VrMatrix44 head_world;
        if (!tracking_space_.HeadWorldPose(head_world, error)) {
            return false;
        }
        const std::array<float, 3> move_direction =
            runtime::HeadRelativeMoveDirection(head_world, pending_input_.move);
        const std::array<float, 3> movement = runtime::LocomotionDisplacement(
            move_direction,
            frame.delta_seconds,
            settings_.move_speed,
            frame.constrained_movement,
            pending_input_.sprint.pressed);
        result.locomotion_distance = HorizontalLength(movement);
        if (result.locomotion_distance > 0.0F) {
            const std::array<float, 3> body_before = body_position;
            const std::array<float, 3> body_after = body.MoveBodyBy(
                movement, BodyMoveKind::stick_locomotion);
            if (!FinitePosition(body_after)) {
                error = "The Overture body adapter returned a non-finite locomotion result";
                return false;
            }
            const std::array<float, 3> accepted =
                Subtract(body_after, body_before);
            if (runtime::ShouldCarryHeadAnchorWithLocomotion(
                    head_anchor_, body_before, accepted)) {
                Add(head_anchor_, accepted);
            }
            body_position = body_after;
        }
    }

    const float feet_height = body.FeetHeight();
    if (!std::isfinite(feet_height)) {
        error = "The Overture body adapter returned a non-finite feet height";
        return false;
    }
    head_anchor_[1] = feet_height;
    tracking_space_.SetPlayerWorldPosition(head_anchor_);
    result.head_anchor = head_anchor_;
    result.body_position = body_position;
    result.world_yaw_radians = tracking_space_.world_yaw();
    result.seated_offset = tracking_space_.seated_offset();
    // Rework's cPlayer::Update consumes vr_moveVec once. Preserve that even
    // if a host accidentally invokes the player phase twice before new input.
    pending_input_.move = {};
    error.clear();
    return true;
}

void OvertureBackend::Reset() noexcept {
    tracking_space_ = {};
    turn_ = {};
    pending_input_ = {};
    previous_head_position_ = {};
    head_anchor_ = {};
    seated_baseline_ = 0.0F;
    initialized_ = false;
    seated_baseline_known_ = false;
}

runtime::VrTrackingSpace& OvertureBackend::tracking_space() noexcept {
    return tracking_space_;
}

const runtime::VrTrackingSpace& OvertureBackend::tracking_space() const noexcept {
    return tracking_space_;
}

void OvertureBackend::UpdatePlayMode(float raw_head_height) noexcept {
    if (settings_.play_mode == runtime::VrPlayMode::standing) {
        seated_baseline_known_ = false;
        seated_baseline_ = 0.0F;
        tracking_space_.SetSeatedOffset(0.0F);
        return;
    }

    const bool plausible = raw_head_height > 0.60F && raw_head_height < 1.50F;
    if (!seated_baseline_known_) {
        if (!plausible) {
            tracking_space_.SetSeatedOffset(0.0F);
            return;
        }
        seated_baseline_ = raw_head_height;
        seated_baseline_known_ = true;
    } else {
        if (raw_head_height > seated_baseline_ + 0.45F) {
            seated_baseline_known_ = false;
            seated_baseline_ = 0.0F;
            tracking_space_.SetSeatedOffset(0.0F);
            return;
        }
        if (plausible && raw_head_height < seated_baseline_ - 0.10F) {
            seated_baseline_ = raw_head_height;
        }
    }

    const float mapped_height = (seated_baseline_ - 0.2F) * 1.065F;
    tracking_space_.SetSeatedOffset(std::clamp(
        settings_.player_height - mapped_height, 0.0F, 1.5F));
}

} // namespace penumbra_vr::backends::overture
