// Action/context behavior adapted from Penumbra VR Rework 23c890f.
// HPL1/Overture: Copyright (C) 2006-2010 Frictional Games, GPLv3 or later.
#include "vr_action_input.hpp"

#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {
constexpr std::array<const char*, 10> kGameNames{
    "sprint", "interact", "examine", "holster", "inventory", "notebook",
    "quick_light", "jump", "crouch", "pause"};
constexpr std::array<VrButtonState VrInputState::*, 10> kGameMembers{
    &VrInputState::sprint, &VrInputState::interact, &VrInputState::examine,
    &VrInputState::holster, &VrInputState::inventory, &VrInputState::notebook,
    &VrInputState::quick_light, &VrInputState::jump, &VrInputState::crouch, &VrInputState::pause};
constexpr std::array<const char*, 4> kUiNames{"select", "drag", "back", "close"};
constexpr std::array<VrButtonState VrInputState::*, 4> kUiMembers{
    &VrInputState::ui_select, &VrInputState::ui_drag, &VrInputState::ui_back, &VrInputState::ui_close};

void ReleaseButtons(const VrInputState& previous, VrInputState& next) {
    next = MakeReleasedVrInputState(previous);
}
bool ValidHand(VrHand hand) { return hand == VrHand::left || hand == VrHand::right; }
bool ValidPose(const VrHmdPose& pose) {
    return pose.device_connected && pose.pose_valid &&
        std::all_of(pose.device_to_absolute.values.begin(), pose.device_to_absolute.values.end(),
            [](float value) { return std::isfinite(value); }) &&
        std::all_of(pose.velocity.begin(), pose.velocity.end(), [](float v) { return std::isfinite(v); }) &&
        std::all_of(pose.angular_velocity.begin(), pose.angular_velocity.end(), [](float v) { return std::isfinite(v); });
}
} // namespace

VrUiPointerPose SelectUiPointerPose(
    const VrControllerFrame& frame,
    VrHand preferred_hand) noexcept {
    VrUiPointerPose result;
    const auto select = [&](VrHand hand) -> bool {
        const auto index = hand == VrHand::left ? 0U : 1U;
        const auto& sample = frame.hands[index];
        if (!sample.grip.device_connected || !sample.grip.pose_valid) return false;
        result.pose = sample.aim.device_connected && sample.aim.pose_valid
            ? sample.aim : sample.grip;
        result.hand = hand;
        result.valid = true;
        return true;
    };
    if (!select(preferred_hand)) {
        static_cast<void>(select(OppositeHand(preferred_hand)));
    }
    return result;
}

bool VrActionInput::Initialize(VrActionBackend& backend, const char* manifest, std::string& error) {
    error.clear();
    if (initialized_) { error = "Controller actions are already initialized"; return false; }
    Reset();
    if (manifest == nullptr || manifest[0] == '\0') { error = "Empty action manifest path"; return false; }
    if (!backend.Manifest(manifest, error)) return false;
    VrActionInput candidate;
    const auto resolve = [&](const std::string& path, VrActionHandle& handle, VrHandleKind kind = VrHandleKind::action) {
        if (!backend.Resolve(path.c_str(), kind, handle, error)) return false;
        if (handle == 0) { error = "Invalid action handle: " + path; return false; }
        return true;
    };
    if (!resolve("/actions/global", candidate.global_, VrHandleKind::action_set) ||
        !resolve("/actions/offhand", candidate.offhand_, VrHandleKind::action_set) ||
        !resolve("/actions/offhand/in/interact", candidate.offhand_interact_) ||
        !resolve("/actions/global/in/recenter", candidate.recenter_)) return false;
    for (std::size_t i = 0; i < 2; ++i) {
        const std::string hand = i == 0 ? "left" : "right";
        if (!resolve("/user/hand/" + hand, candidate.sources_[i], VrHandleKind::source) ||
            !resolve("/actions/global/in/" + hand + "_pose", candidate.grips_[i]) ||
            !resolve("/actions/global/in/" + hand + "_aim", candidate.aims_[i]) ||
            !resolve("/actions/global/in/" + hand + "_skeleton", candidate.skeletons_[i]) ||
            !resolve("/actions/global/out/" + hand + "_haptic", candidate.haptics_[i])) return false;
        auto& context = candidate.contexts_[i];
        const std::string game = i == 0 ? "/actions/gameplay_left" : "/actions/gameplay";
        const std::string ui = i == 0 ? "/actions/ui_left" : "/actions/ui";
        if (!resolve(game, context.set, VrHandleKind::action_set) ||
            !resolve(ui, context.ui_set, VrHandleKind::action_set) ||
            !resolve(game + "/in/move", context.move) || !resolve(game + "/in/turn", context.turn)) return false;
        for (std::size_t j = 0; j < kGameNames.size(); ++j)
            if (!resolve(game + "/in/" + kGameNames[j], context.gameplay[j])) return false;
        for (std::size_t j = 0; j < kUiNames.size(); ++j)
            if (!resolve(ui + "/in/" + kUiNames[j], context.ui[j])) return false;
    }
    candidate.initialized_ = true;
    *this = candidate;
    return true;
}

bool VrActionInput::Update(VrActionBackend& backend, VrInputContext context,
    VrHand handedness, bool focused, std::uint64_t now_ms,
    VrControllerFrame& frame, std::string& error) {
    error.clear();
    frame = {};
    const auto release = [&] {
        ReleaseButtons(router_.state(), frame.input.state);
        router_.Reset();
        focused_ = false;
    };
    if (!initialized_ || !ValidHand(handedness) ||
        (context != VrInputContext::gameplay && context != VrInputContext::ui)) {
        error = "Controller input is unavailable or context/hand is invalid";
        release(); return false;
    }
    if (!focused) { release(); return true; }
    const std::size_t dominant = handedness == VrHand::left ? 0U : 1U;
    const auto& handles = contexts_[dominant];
    // One interaction owner for the selected handedness. Activating the shared
    // offhand set also binds the dominant trigger on some controller profiles.
    // Keep both tracked hands/skeletons, but do not activate mixed ownership.
    const std::array<VrActiveSet, 2> sets{{{global_, 0},
        {context == VrInputContext::gameplay ? handles.set : handles.ui_set, 0}}};
    if (!backend.Activate(sets, error)) {
        release(); return false;
    }
    for (std::size_t i = 0; i < 2; ++i) {
        auto& hand = frame.hands[i];
        if (!backend.Pose(grips_[i], hand.grip) || !ValidPose(hand.grip)) hand.grip = {};
        if (!backend.Pose(aims_[i], hand.aim) || !ValidPose(hand.aim)) hand.aim = {};
        hand.skeleton_valid = backend.Skeleton(skeletons_[i], hand.finger_curl) &&
            std::all_of(hand.finger_curl.begin(), hand.finger_curl.end(), [](float v) { return std::isfinite(v); });
        if (hand.skeleton_valid) {
            for (float& curl : hand.finger_curl) curl = std::clamp(curl, 0.0F, 1.0F);
        } else hand.finger_curl = {};
    }
    VrInputState raw;
    router_.SetInteractSourceHand(handedness);
    bool ok = backend.Digital(recenter_, raw.recenter, error);
    if (ok && context == VrInputContext::gameplay) {
        ok = backend.Analog(handles.move, raw.move, error) && backend.Analog(handles.turn, raw.turn, error);
        for (std::size_t j = 0; ok && j < kGameMembers.size(); ++j)
            ok = backend.Digital(handles.gameplay[j], raw.*kGameMembers[j], error);
        if (ok) {
            const bool held = router_.state().interact.pressed;
            raw.interact.just_pressed = raw.interact.just_pressed && !held;
            raw.interact.just_released = held && !raw.interact.pressed;
        }
    } else if (ok) {
        for (std::size_t j = 0; ok && j < kUiMembers.size(); ++j)
            ok = backend.Digital(handles.ui[j], raw.*kUiMembers[j], error);
    }
    if (!ok) { frame = {}; release(); return false; }
    if (!std::isfinite(raw.move.x) || !std::isfinite(raw.move.y) ||
        !std::isfinite(raw.turn.x) || !std::isfinite(raw.turn.y)) {
        error = "Non-finite controller axis"; frame = {}; release(); return false;
    }
    raw.move.x = std::clamp(raw.move.x, -1.0F, 1.0F);
    raw.move.y = std::clamp(raw.move.y, -1.0F, 1.0F);
    raw.turn.x = std::clamp(raw.turn.x, -1.0F, 1.0F);
    raw.turn.y = std::clamp(raw.turn.y, -1.0F, 1.0F);
    if (!raw.turn.active) raw.turn = {};
    // Inactive bindings must not retain a walking/held command for the legacy
    // router's 500 ms grace period. There is no raw-poll fallback in this port.
    if (!AnyActionActive(raw)) { release(); return true; }
    frame.input = router_.Update(raw, context, handedness,
        frame.hands[0].grip.pose_valid, frame.hands[1].grip.pose_valid, now_ms);
    frame.interact_source = router_.interact_source_hand();
    frame.focused = true;
    focused_ = true;
    return true;
}

bool VrActionInput::TriggerHaptic(VrActionBackend& backend, VrHand hand,
    float duration, float frequency, float amplitude, std::string& error) {
    error.clear();
    if (!initialized_ || !focused_ || !ValidHand(hand) || !std::isfinite(duration) ||
        !std::isfinite(frequency) || !std::isfinite(amplitude) || duration <= 0.0F ||
        duration > 1.0F || frequency < 0.0F || frequency > 320.0F || amplitude < 0.0F || amplitude > 1.0F) {
        error = "Haptic request is unavailable or outside safe limits"; return false;
    }
    return backend.Haptic(haptics_[hand == VrHand::left ? 0 : 1], duration, frequency, amplitude, error);
}

void VrActionInput::Reset() noexcept { *this = VrActionInput{}; }
} // namespace penumbra_vr::runtime
