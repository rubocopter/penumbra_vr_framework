#pragma once

#include "vr_input_state.hpp"
#include "vr_native_intents.hpp"
#include "vr_settings.hpp"
#include "vr_tracking_space.hpp"

#include <array>
#include <string>

namespace penumbra_vr::backends::overture {

enum class BodyMoveKind {
    room_scale_static_only,
    stick_locomotion,
};

// Narrow HPL boundary. The source-built Overture adapter owns cPlayer,
// iCharacterBody and collision calls; the backend owns proven VR behavior.
class OvertureBodyAdapter {
public:
    virtual ~OvertureBodyAdapter() = default;
    [[nodiscard]] virtual std::array<float, 3> BodyPosition() const noexcept = 0;
    [[nodiscard]] virtual float FeetHeight() const noexcept = 0;
    // For room_scale_static_only, mirror Rework by setting vr_velocity,
    // enabling vr_stepstaticonly around iCharacterBody::Update, and returning
    // the collision-resolved position. Stick locomotion uses the same native
    // update without static-only mode. displacement already includes dt.
    [[nodiscard]] virtual std::array<float, 3> MoveBodyBy(
        const std::array<float, 3>& displacement,
        float delta_seconds,
        BodyMoveKind kind) noexcept = 0;
    virtual void StartJump() noexcept = 0;
    virtual void SetJumpHeld(bool held) noexcept = 0;
};

struct OvertureInputFrame {
    runtime::VrInputState input;
    float delta_seconds = 0.0F;
    bool gameplay_active = false;
};

struct OverturePlayerFrame {
    runtime::VrMatrix34 head_tracking_pose;
    float delta_seconds = 0.0F;
    bool body_motion_enabled = false;
    bool constrained_movement = false;
};

struct OvertureFrameResult {
    std::array<float, 3> head_anchor{};
    std::array<float, 3> body_position{};
    float requested_room_scale_distance = 0.0F;
    float rejected_room_scale_distance = 0.0F;
    float locomotion_distance = 0.0F;
    float world_yaw_radians = 0.0F;
    float seated_offset = 0.0F;
    bool head_anchor_rebased = false;
};

// Initial functional Overture backend. HandleInput is called at the single
// game-input boundary; UpdatePlayer is called once from cPlayer::Update where
// Rework previously owned room-scale/stick motion. HPL stays in the adapter.
class OvertureBackend final {
public:
    void SetSettings(runtime::VrSettings settings) noexcept;
    [[nodiscard]] const runtime::VrSettings& settings() const noexcept;

    [[nodiscard]] bool Initialize(
        const runtime::VrMatrix34& head_tracking_pose,
        OvertureBodyAdapter& body,
        std::string& error) noexcept;
    [[nodiscard]] bool HandleInput(
        const OvertureInputFrame& frame,
        OvertureBodyAdapter& body,
        std::string& error) noexcept;
    [[nodiscard]] bool UpdatePlayer(
        const OverturePlayerFrame& frame,
        OvertureBodyAdapter& body,
        OvertureFrameResult& result,
        std::string& error) noexcept;
    void Reset() noexcept;

    [[nodiscard]] runtime::VrTrackingSpace& tracking_space() noexcept;
    [[nodiscard]] const runtime::VrTrackingSpace& tracking_space() const noexcept;

private:
    void UpdatePlayMode(float raw_head_height) noexcept;

    runtime::VrSettings settings_;
    runtime::VrTrackingSpace tracking_space_;
    runtime::VrSnapTurn turn_;
    runtime::VrInputState pending_input_;
    std::array<float, 3> previous_head_position_{};
    std::array<float, 3> head_anchor_{};
    float seated_baseline_ = 0.0F;
    bool initialized_ = false;
    bool seated_baseline_known_ = false;
};

} // namespace penumbra_vr::backends::overture
