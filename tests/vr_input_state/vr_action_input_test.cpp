#include "vr_action_input.hpp"

#include <iostream>
#include <limits>
#include <map>
#include <vector>

using namespace penumbra_vr::runtime;
namespace {
class FakeBackend final : public VrActionBackend {
public:
    std::map<std::string, VrActionHandle> handles;
    std::map<VrActionHandle, VrButtonState> buttons;
    std::map<VrActionHandle, VrAnalogState> axes;
    std::vector<VrActiveSet> active;
    std::string fail_resolve;
    bool fail_update = false, inactive = false, pose_valid = true, skeleton = false;
    bool fail_read = false;
    VrActionHandle last_haptic = 0;
    bool Manifest(const char*, std::string&) override { return true; }
    bool Resolve(const char* path, VrHandleKind, VrActionHandle& handle, std::string& error) override {
        if (path == fail_resolve) { error = "fake resolution failure"; return false; }
        handle = handles[path] = handles.size() + 1;
        return true;
    }
    bool Activate(std::span<const VrActiveSet> sets, std::string& error) override {
        if (fail_update) { error = "fake update failure"; return false; }
        active.assign(sets.begin(), sets.end()); return true;
    }
    bool Digital(VrActionHandle handle, VrButtonState& result, std::string& error) override {
        if (fail_read) { error = "fake read failure"; return false; }
        result = inactive ? VrButtonState{} : buttons.contains(handle) ? buttons[handle] : VrButtonState{true};
        return true;
    }
    bool Analog(VrActionHandle handle, VrAnalogState& result, std::string&) override {
        result = inactive ? VrAnalogState{} : axes[handle]; return true;
    }
    bool Pose(VrActionHandle, VrHmdPose& result) override {
        result.device_connected = result.pose_valid = pose_valid;
        result.device_to_absolute.values = {1,0,0,0, 0,1,0,1, 0,0,1,0}; return true;
    }
    bool Skeleton(VrActionHandle, std::array<float, 5>& curl) override {
        curl = {-1,0,0.5F,1,2}; return skeleton;
    }
    bool Haptic(VrActionHandle handle, float, float, float, std::string&) override {
        last_haptic = handle; return true;
    }
    void Press(const char* name, bool changed = true) {
        buttons[handles.at(name)] = MakeVrButtonState(true, true, changed);
    }
};
}
int main() {
    VrActionInput reader;
    FakeBackend fake;
    std::string error;
    VrControllerFrame frame;
    auto update = [&](VrInputContext context = VrInputContext::gameplay, VrHand hand = VrHand::right, bool focus = true) {
        return reader.Update(fake, context, hand, focus, 100, frame, error);
    };
    if (update() || error.empty() || reader.initialized()) return 1;
    fake.fail_resolve = "/actions/gameplay/in/jump";
    if (reader.Initialize(fake, "C:/vr/actions.json", error) || reader.initialized()) return 2;
    fake = {};
    if (!reader.Initialize(fake, "C:/vr/actions.json", error) || fake.handles.size() != 50 ||
        reader.Initialize(fake, "C:/vr/actions.json", error) || error.empty()) return 3;
    if (!update() || fake.active.size() != 3 ||
        fake.active[1].set != fake.handles.at("/actions/gameplay") ||
        fake.active[2].restricted_source != fake.handles.at("/user/hand/left")) return 4;
    fake.Press("/actions/offhand/in/interact");
    fake.Press("/actions/gameplay/in/inventory");
    fake.axes[fake.handles.at("/actions/gameplay/in/move")] = {true, 1, 0};
    if (!update() || !frame.input.state.interact.just_pressed || frame.interact_source != VrHand::left ||
        !frame.input.state.inventory.just_pressed || frame.input.state.move.x != 1) return 5;
    fake.Press("/actions/offhand/in/interact", false);
    fake.Press("/actions/gameplay/in/interact");
    if (!update() || frame.interact_source != VrHand::left ||
        frame.input.state.interact.just_pressed || !frame.input.state.interact.pressed) return 18;
    fake.buttons[fake.handles.at("/actions/offhand/in/interact")] = MakeVrButtonState(true,false,true);
    if (!update() || frame.input.state.interact.pressed || !frame.input.state.interact.just_released) return 19;
    fake.buttons.erase(fake.handles.at("/actions/gameplay/in/interact"));
    fake.Press("/actions/offhand/in/interact");
    if (!update() || !frame.input.state.interact.just_pressed) return 20;
    fake.pose_valid = false;
    if (!update() || frame.input.state.interact.pressed || !frame.input.state.interact.just_released ||
        !frame.input.state.inventory.pressed) return 6;
    fake.pose_valid = true;
    fake.skeleton = true;
    fake.Press("/actions/ui_left/in/select");
    if (!update(VrInputContext::ui, VrHand::left) || fake.active.size() != 2 ||
        fake.active[1].set != fake.handles.at("/actions/ui_left") ||
        frame.input.state.ui_select.just_pressed || !frame.input.state.ui_select.pressed ||
        frame.input.state.move.active || !frame.hands[0].skeleton_valid ||
        frame.hands[0].finger_curl != std::array<float, 5>{0,0,0.5F,1,1}) return 7;
    if (!reader.TriggerHaptic(fake, VrHand::left, 0.02F, 100, 0.3F, error) ||
        fake.last_haptic != fake.handles.at("/actions/global/out/left_haptic") ||
        reader.TriggerHaptic(fake, VrHand::left, 2, 100, 0.3F, error)) return 8;
    if (!update(VrInputContext::ui, VrHand::left, false) || frame.focused ||
        frame.input.state.ui_select.pressed || !frame.input.state.ui_select.just_released ||
        frame.hands[0].grip.pose_valid || reader.TriggerHaptic(fake, VrHand::left, 0.02F, 100, 0.3F, error)) return 9;
    if (!update() || !frame.input.state.inventory.pressed) return 10;
    fake.fail_update = true;
    if (update() || error.empty() || frame.focused || !frame.input.state.inventory.just_released) return 11;
    fake.fail_update = false;
    if (!update()) return 12;
    fake.inactive = true;
    if (!update() || frame.input.state.move.x != 0 || frame.input.state.inventory.pressed ||
        !frame.input.state.inventory.just_released) return 13;
    fake.inactive = false;
    fake.axes[fake.handles.at("/actions/gameplay/in/move")] = {true, std::numeric_limits<float>::quiet_NaN(), 0};
    if (update() || error.empty() || frame.input.state.move.active) return 14;
    fake.axes.clear();
    if (!update()) return 15;
    fake.fail_read = true;
    if (update() || frame.hands[0].grip.pose_valid || frame.input.state.interact.pressed) return 16;
    reader.Reset();
    if (reader.initialized() || update()) return 17;
    std::cout << "Action registration, contexts, offhand, poses, skeleton, focus, failure and haptics passed\n";
    return 0;
}
