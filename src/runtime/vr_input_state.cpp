#include "vr_input_state.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {

void SuppressButtonEdges(VrButtonState& state) noexcept {
    state.just_pressed = false;
    state.just_released = false;
}

void SuppressGameplayEdges(VrInputState& state) noexcept {
    SuppressButtonEdges(state.sprint);
    SuppressButtonEdges(state.interact);
    SuppressButtonEdges(state.examine);
    SuppressButtonEdges(state.holster);
    SuppressButtonEdges(state.inventory);
    SuppressButtonEdges(state.notebook);
    SuppressButtonEdges(state.quick_light);
    SuppressButtonEdges(state.jump);
    SuppressButtonEdges(state.crouch);
    SuppressButtonEdges(state.pause);
}

void SuppressUiEdges(VrInputState& state) noexcept {
    SuppressButtonEdges(state.ui_select);
    SuppressButtonEdges(state.ui_drag);
    SuppressButtonEdges(state.ui_back);
    SuppressButtonEdges(state.ui_close);
}

[[nodiscard]] bool PoseIsValid(
    VrHand hand,
    bool left_pose_valid,
    bool right_pose_valid) noexcept {
    return hand == VrHand::left ? left_pose_valid : right_pose_valid;
}

void ReleaseForLostPose(
    VrButtonState& sample,
    const VrButtonState& previous) noexcept {
    sample.pressed = false;
    sample.just_pressed = false;
    sample.just_released = previous.pressed;
}

} // namespace

VrButtonState MakeVrButtonState(
    bool active,
    bool pressed,
    bool changed) noexcept {
    VrButtonState state;
    state.active = active;
    state.pressed = active && pressed;
    state.just_pressed = active && changed && pressed;
    state.just_released = active && changed && !pressed;
    return state;
}

float ClampStickDeadZone(float dead_zone) noexcept {
    return std::clamp(
        dead_zone,
        vr_setting_limits::kMoveDeadZone.minimum,
        vr_setting_limits::kMoveDeadZone.maximum);
}

void ApplyStickDeadZone(VrAnalogState& state, float dead_zone) noexcept {
    if (!state.active) {
        state.x = 0.0F;
        state.y = 0.0F;
        return;
    }

    dead_zone = ClampStickDeadZone(dead_zone);
    const float magnitude = std::sqrt(state.x * state.x + state.y * state.y);
    if (magnitude <= dead_zone || magnitude <= 0.0001F) {
        state.x = 0.0F;
        state.y = 0.0F;
        return;
    }

    const float scaled_magnitude = std::min(
        (magnitude - dead_zone) / (1.0F - dead_zone), 1.0F);
    state.x = (state.x / magnitude) * scaled_magnitude;
    state.y = (state.y / magnitude) * scaled_magnitude;
}

void SuppressAllButtonEdges(VrInputState& state) noexcept {
    SuppressGameplayEdges(state);
    SuppressButtonEdges(state.recenter);
    SuppressUiEdges(state);
}

VrInputState MakeReleasedVrInputState(const VrInputState& previous) noexcept {
    VrInputState released;
    for (auto member : {&VrInputState::sprint, &VrInputState::interact, &VrInputState::examine,
        &VrInputState::holster, &VrInputState::inventory, &VrInputState::notebook,
        &VrInputState::quick_light, &VrInputState::jump, &VrInputState::crouch,
        &VrInputState::pause, &VrInputState::recenter, &VrInputState::ui_select,
        &VrInputState::ui_drag, &VrInputState::ui_back, &VrInputState::ui_close}) {
        (released.*member).just_released = (previous.*member).pressed;
    }
    return released;
}

bool AnyActionActive(const VrInputState& state) noexcept {
    return state.move.active || state.turn.active || state.sprint.active ||
        state.interact.active || state.examine.active || state.holster.active ||
        state.inventory.active || state.notebook.active ||
        state.quick_light.active || state.jump.active || state.crouch.active ||
        state.pause.active || state.recenter.active || state.ui_select.active ||
        state.ui_drag.active || state.ui_back.active || state.ui_close.active;
}

VrHand OppositeHand(VrHand hand) noexcept {
    return hand == VrHand::left ? VrHand::right : VrHand::left;
}

VrInputUpdateResult VrInputRouter::Update(
    VrInputState sample,
    VrInputContext context,
    VrHand handedness,
    bool left_pose_valid,
    bool right_pose_valid,
    std::uint64_t now_milliseconds) noexcept {
    ApplyStickDeadZone(sample.move, move_dead_zone_);

    if (context == VrInputContext::gameplay) {
        if ((sample.interact.pressed || state_.interact.pressed) &&
            !PoseIsValid(
                interact_source_hand_, left_pose_valid, right_pose_valid)) {
            ReleaseForLostPose(sample.interact, state_.interact);
        }
    } else if (!PoseIsValid(
                   handedness, left_pose_valid, right_pose_valid)) {
        ReleaseForLostPose(sample.ui_select, state_.ui_select);
        ReleaseForLostPose(sample.ui_drag, state_.ui_drag);
    }

    const bool handedness_changed = handedness != active_handedness_;
    const bool suppress_context_edges = !using_actions_ ||
        !has_active_context_ || context != active_context_ ||
        handedness_changed;
    if (handedness_changed) {
        SuppressButtonEdges(sample.recenter);
    }
    if (suppress_context_edges) {
        if (context == VrInputContext::gameplay) {
            SuppressGameplayEdges(sample);
        } else {
            SuppressUiEdges(sample);
        }
    }

    if (AnyActionActive(sample)) {
        using_actions_ = true;
        has_active_context_ = true;
        active_context_ = context;
        active_handedness_ = handedness;
        idle_since_milliseconds_ = 0;
        idle_timer_started_ = false;
        state_ = sample;
        return {VrInputUpdateStatus::actions_active, state_};
    }

    if (using_actions_) {
        if (!idle_timer_started_) {
            idle_since_milliseconds_ = now_milliseconds;
            idle_timer_started_ = true;
        }
        const std::uint64_t idle_elapsed =
            now_milliseconds >= idle_since_milliseconds_
            ? now_milliseconds - idle_since_milliseconds_
            : 0;
        if (idle_elapsed < kActionIdleGraceMilliseconds) {
            VrInputState held_state = state_;
            SuppressAllButtonEdges(held_state);
            state_ = held_state;
            return {VrInputUpdateStatus::action_idle_grace, state_};
        }
    }

    using_actions_ = false;
    idle_since_milliseconds_ = 0;
    idle_timer_started_ = false;
    state_ = {};
    return {VrInputUpdateStatus::fallback_required, state_};
}

void VrInputRouter::Reset() noexcept {
    state_ = {};
    move_dead_zone_ = vr_setting_limits::kMoveDeadZone.default_value;
    interact_source_hand_ = VrHand::right;
    active_context_ = VrInputContext::gameplay;
    active_handedness_ = VrHand::right;
    idle_since_milliseconds_ = 0;
    using_actions_ = false;
    has_active_context_ = false;
    idle_timer_started_ = false;
}

void VrInputRouter::SetMoveDeadZone(float dead_zone) noexcept {
    move_dead_zone_ = ClampStickDeadZone(dead_zone);
}

void VrInputRouter::SetInteractSourceHand(VrHand hand) noexcept {
    interact_source_hand_ = hand;
}

const VrInputState& VrInputRouter::state() const noexcept {
    return state_;
}

float VrInputRouter::move_dead_zone() const noexcept {
    return move_dead_zone_;
}

VrHand VrInputRouter::interact_source_hand() const noexcept {
    return interact_source_hand_;
}

bool VrInputRouter::using_actions() const noexcept {
    return using_actions_;
}

} // namespace penumbra_vr::runtime
