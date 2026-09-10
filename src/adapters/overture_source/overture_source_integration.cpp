#include "overture_source_integration.hpp"

#include "Init.h"
#include "Player.h"
#include "game/Game.h"
#include "input/SteamVRInput.h"
#include "physics/CharacterBody.h"

#include <array>

namespace penumbra_vr::adapters::overture_source {
namespace {

using backends::overture::BodyMoveKind;
using backends::overture::OvertureBodyAdapter;
using runtime::VrButtonState;
using runtime::VrInputState;
using runtime::VrMatrix34;
using runtime::VrSettings;

[[nodiscard]] std::array<float, 3> ToArray(
    const hpl::cVector3f& value) noexcept {
    return {value.x, value.y, value.z};
}

[[nodiscard]] hpl::cVector3f ToHpl(
    const std::array<float, 3>& value) noexcept {
    return {value[0], value[1], value[2]};
}

[[nodiscard]] VrMatrix34 ToRuntimePose(const hpl::cMatrixf& pose) noexcept {
    return {{
        pose.m[0][0], pose.m[0][1], pose.m[0][2], pose.m[0][3],
        pose.m[1][0], pose.m[1][1], pose.m[1][2], pose.m[1][3],
        pose.m[2][0], pose.m[2][1], pose.m[2][2], pose.m[2][3],
    }};
}

[[nodiscard]] VrButtonState ToRuntimeButton(
    const hpl::cVRButtonState& button) noexcept {
    return {
        button.active,
        button.pressed,
        button.justPressed,
        button.justReleased,
    };
}

[[nodiscard]] VrInputState ToRuntimeInput(
    const hpl::cVRInputState& input) noexcept {
    VrInputState result;
    result.move = {input.moveActive, input.moveX, input.moveY};
    result.turn = {input.turnActive, input.turnX, 0.0F};
    result.sprint = ToRuntimeButton(input.sprint);
    result.interact = ToRuntimeButton(input.interact);
    result.examine = ToRuntimeButton(input.examine);
    result.holster = ToRuntimeButton(input.holster);
    result.inventory = ToRuntimeButton(input.inventory);
    result.notebook = ToRuntimeButton(input.notebook);
    result.quick_light = ToRuntimeButton(input.quickLight);
    result.jump = ToRuntimeButton(input.jump);
    result.crouch = ToRuntimeButton(input.crouch);
    result.pause = ToRuntimeButton(input.pause);
    result.recenter = ToRuntimeButton(input.recenter);
    result.ui_select = ToRuntimeButton(input.uiSelect);
    result.ui_drag = ToRuntimeButton(input.uiDrag);
    result.ui_back = ToRuntimeButton(input.uiBack);
    result.ui_close = ToRuntimeButton(input.uiClose);
    return result;
}

[[nodiscard]] runtime::VrTurnMode ToRuntimeTurnMode(
    eVRTurnMode mode) noexcept {
    switch (mode) {
    case eVRTurnMode_Disabled:
        return runtime::VrTurnMode::disabled;
    case eVRTurnMode_Smooth:
        return runtime::VrTurnMode::smooth;
    case eVRTurnMode_Snap:
    default:
        return runtime::VrTurnMode::snap;
    }
}

[[nodiscard]] runtime::VrPlayMode ToRuntimePlayMode(
    eVRPlayMode mode) noexcept {
    return mode == eVRPlayMode_Seated
        ? runtime::VrPlayMode::seated
        : runtime::VrPlayMode::standing;
}

[[nodiscard]] VrSettings ToRuntimeSettings(const cVRSettings& settings) noexcept {
    VrSettings result;
    result.move_speed = settings.GetMoveSpeed();
    result.move_dead_zone = settings.GetMoveDeadZone();
    result.height_offset = settings.GetHeightOffset();
    result.turn_mode = ToRuntimeTurnMode(settings.GetTurnMode());
    result.snap_turn_angle = settings.GetSnapTurnAngle();
    result.smooth_turn_speed = settings.GetSmoothTurnSpeed();
    result.turn_dead_zone = settings.GetTurnDeadZone();
    result.play_mode = ToRuntimePlayMode(settings.GetPlayMode());
    result.player_height = settings.GetPlayerHeight();
    return result;
}

class HplOvertureBodyAdapter final : public OvertureBodyAdapter {
public:
    explicit HplOvertureBodyAdapter(cPlayer& player) noexcept
        : player_(player) {}

    [[nodiscard]] std::array<float, 3> BodyPosition() const noexcept override {
        return ToArray(player_.GetCharacterBody()->GetPosition());
    }

    [[nodiscard]] float FeetHeight() const noexcept override {
        return player_.GetCharacterBody()->GetFeetPosition().y;
    }

    [[nodiscard]] std::array<float, 3> MoveBodyBy(
        const std::array<float, 3>& displacement,
        float delta_seconds,
        BodyMoveKind kind) noexcept override {
        hpl::iCharacterBody* const body = player_.GetCharacterBody();
        body->vr_velocity = ToHpl(displacement);
        body->vr_stepstaticonly =
            kind == BodyMoveKind::room_scale_static_only;
        body->Update(delta_seconds);
        body->vr_stepstaticonly = false;
        return ToArray(body->GetPosition());
    }

    void StartJump() noexcept override {
        player_.Jump();
    }

    void SetJumpHeld(bool held) noexcept override {
        player_.SetJumpButtonDown(held);
    }

private:
    cPlayer& player_;
};

void SyncTrackingBeforeUpdate(
    backends::overture::OvertureBackend& backend,
    cInit& init) noexcept {
    backend.tracking_space().SetWorldYaw(
        init.mpGame->vr_tracking.GetWorldYaw());
    backend.tracking_space().SetPostureOffset(
        init.mpGame->vr_tracking.GetPostureOffset());
}

void SyncTrackingToHpl(
    const backends::overture::OvertureBackend& backend,
    cInit& init) noexcept {
    const runtime::VrTrackingSpace& tracking = backend.tracking_space();
    init.mpGame->vr_tracking.SetHeightCalibration(
        tracking.height_calibration());
    init.mpGame->vr_tracking.SetSeatedOffset(tracking.seated_offset());
    init.mpGame->vr_tracking.SetWorldYaw(tracking.world_yaw());
}

} // namespace

void OvertureSourceIntegration::Reset() noexcept {
    backend_.Reset();
    initialized_ = false;
    input_pending_for_player_ = false;
}

bool OvertureSourceIntegration::EnsureInitialized(
    cPlayer& player,
    cInit& init,
    std::string& error) noexcept {
    if (initialized_) {
        return true;
    }
    if (player.GetCharacterBody() == nullptr) {
        error = "The Overture character body is not available";
        return false;
    }

    backend_.SetSettings(ToRuntimeSettings(init.mVRSettings));
    HplOvertureBodyAdapter body(player);
    initialized_ = backend_.Initialize(
        ToRuntimePose(init.mpGame->vr_tracking.GetHeadTrackingPose()),
        body,
        error);
    if (initialized_) {
        // Preserve yaw across map/player resets and accept recentering from the
        // existing settings-menu button, which still owns its HPL UI action.
        SyncTrackingBeforeUpdate(backend_, init);
        SyncTrackingToHpl(backend_, init);
    }
    return initialized_;
}

bool OvertureSourceIntegration::HandleInput(
    cPlayer& player,
    cInit& init,
    const hpl::cVRInputState& input,
    float delta_seconds,
    bool gameplay_active,
    bool player_update_expected,
    std::string& error) noexcept {
    // cButtonHandler is registered globally and in Default, but cPlayer only
    // runs in Default. Suppress that container's duplicate input call; outside
    // Default every call is a new frame and must remain usable for recentering.
    if (player_update_expected && input_pending_for_player_) {
        error.clear();
        return true;
    }
    if (!EnsureInitialized(player, init, error)) {
        return false;
    }

    backend_.SetSettings(ToRuntimeSettings(init.mVRSettings));
    SyncTrackingBeforeUpdate(backend_, init);
    backends::overture::OvertureInputFrame frame;
    frame.input = ToRuntimeInput(input);
    frame.delta_seconds = delta_seconds;
    frame.gameplay_active = gameplay_active;
    HplOvertureBodyAdapter body(player);
    if (!backend_.HandleInput(frame, body, error)) {
        return false;
    }
    input_pending_for_player_ = player_update_expected;
    SyncTrackingToHpl(backend_, init);
    return true;
}

bool OvertureSourceIntegration::UpdatePlayer(
    cPlayer& player,
    cInit& init,
    float delta_seconds,
    bool body_motion_enabled,
    bool constrained_movement,
    std::string& error) noexcept {
    if (!EnsureInitialized(player, init, error)) {
        input_pending_for_player_ = false;
        return false;
    }

    backend_.SetSettings(ToRuntimeSettings(init.mVRSettings));
    SyncTrackingBeforeUpdate(backend_, init);
    backends::overture::OverturePlayerFrame frame;
    frame.head_tracking_pose = ToRuntimePose(
        init.mpGame->vr_tracking.GetHeadTrackingPose());
    frame.delta_seconds = delta_seconds;
    frame.body_motion_enabled = body_motion_enabled;
    frame.constrained_movement = constrained_movement;

    HplOvertureBodyAdapter body(player);
    backends::overture::OvertureFrameResult result;
    const bool updated = backend_.UpdatePlayer(frame, body, result, error);
    input_pending_for_player_ = false;
    if (!updated) {
        return false;
    }

    player.vr_headPos = ToHpl(result.head_anchor);
    player.vr_moveVec = hpl::cVector3f(0.0F, 0.0F, 0.0F);
    init.mpGame->vr_tracking.SetPlayerWorldPose(
        hpl::cMath::MatrixTranslate(player.vr_headPos));
    SyncTrackingToHpl(backend_, init);
    player.GetCharacterBody()->vr_velocity = hpl::cVector3f(0.0F, 0.0F, 0.0F);
    return true;
}

} // namespace penumbra_vr::adapters::overture_source
