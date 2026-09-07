#include "overture_backend.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace {

using penumbra_vr::backends::overture::BodyMoveKind;
using penumbra_vr::backends::overture::OvertureBackend;
using penumbra_vr::backends::overture::OvertureBodyAdapter;
using penumbra_vr::backends::overture::OvertureFrameResult;
using penumbra_vr::backends::overture::OvertureInputFrame;
using penumbra_vr::backends::overture::OverturePlayerFrame;
using penumbra_vr::runtime::VrMatrix34;

[[nodiscard]] VrMatrix34 Pose(float x, float y, float z) {
    return {{
        1.0F, 0.0F, 0.0F, x,
        0.0F, 1.0F, 0.0F, y,
        0.0F, 0.0F, 1.0F, z,
    }};
}

class FakeBody final : public OvertureBodyAdapter {
public:
    [[nodiscard]] std::array<float, 3> BodyPosition() const noexcept override {
        return position;
    }
    [[nodiscard]] float FeetHeight() const noexcept override {
        return feet;
    }
    [[nodiscard]] std::array<float, 3> MoveBodyBy(
        const std::array<float, 3>& displacement,
        BodyMoveKind kind) noexcept override {
        if (kind == BodyMoveKind::room_scale_static_only) {
            ++physical_moves;
            if (block_physical) {
                return position;
            }
        } else {
            ++stick_moves;
        }
        for (std::size_t index = 0; index < position.size(); ++index) {
            position[index] += displacement[index];
        }
        return position;
    }
    void StartJump() noexcept override { ++jumps; }
    void SetJumpHeld(bool held) noexcept override { jump_held = held; }

    std::array<float, 3> position{};
    float feet = 0.0F;
    int physical_moves = 0;
    int stick_moves = 0;
    int jumps = 0;
    bool block_physical = false;
    bool jump_held = false;
};

[[nodiscard]] bool Near(float actual, float expected, float tolerance = 1.0e-4F) {
    if (std::fabs(actual - expected) <= tolerance) {
        return true;
    }
    std::cerr << "Expected " << expected << " but got " << actual << '\n';
    return false;
}

} // namespace

int main() {
    OvertureBackend backend;
    FakeBody body;
    std::string error;
    if (!backend.Initialize(Pose(0.0F, 1.7F, 0.0F), body, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    OvertureInputFrame input;
    input.delta_seconds = 0.10F;
    input.gameplay_active = true;
    input.input.move = {true, 0.0F, 1.0F};
    input.input.sprint = {true, false, false, false};
    input.input.jump = {true, true, true, false};
    OverturePlayerFrame frame;
    frame.head_tracking_pose = Pose(0.10F, 1.7F, 0.0F);
    frame.delta_seconds = 0.10F;
    frame.body_motion_enabled = true;
    body.block_physical = true;

    OvertureFrameResult result;
    if (!backend.HandleInput(input, body, error) ||
        !backend.UpdatePlayer(frame, body, result, error)) {
        std::cerr << error << '\n';
        return 2;
    }
    if (body.physical_moves != 1 || body.stick_moves != 1 || body.jumps != 1 ||
        !body.jump_held || !Near(result.requested_room_scale_distance, 0.05F) ||
        !Near(result.rejected_room_scale_distance, 0.05F) ||
        !Near(body.position[2], -0.15F) ||
        !Near(result.head_anchor[0], 0.05F) ||
        !Near(result.head_anchor[2], -0.15F)) {
        std::cerr << "Overture collision/locomotion equivalence failed\n";
        return 3;
    }
    if (!backend.UpdatePlayer(frame, body, result, error) ||
        body.stick_moves != 1 || result.locomotion_distance != 0.0F) {
        std::cerr << "Overture move intent was not consumed exactly once\n";
        return 6;
    }

    auto settings = backend.settings();
    settings.play_mode = penumbra_vr::runtime::VrPlayMode::seated;
    settings.player_height = 1.70F;
    backend.SetSettings(settings);
    frame.head_tracking_pose = Pose(0.10F, 1.0F, 0.0F);
    input.input = {};
    if (!backend.HandleInput(input, body, error) ||
        !backend.UpdatePlayer(frame, body, result, error) ||
        !Near(result.seated_offset, 0.848F, 1.0e-3F)) {
        std::cerr << "Seated calibration failed: " << error << '\n';
        return 4;
    }

    frame.delta_seconds = -1.0F;
    if (backend.UpdatePlayer(frame, body, result, error) || error.empty()) {
        std::cerr << "Invalid frame timing was accepted\n";
        return 5;
    }

    std::cout << "Functional Overture backend behavior passed\n";
    return 0;
}
