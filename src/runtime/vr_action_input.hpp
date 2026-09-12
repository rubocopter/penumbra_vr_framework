#pragma once

#include "vr_input_state.hpp"
#include "vr_tracking_types.hpp"

#include <array>
#include <span>
#include <string>

namespace penumbra_vr::runtime {

using VrActionHandle = std::uint64_t;
enum class VrHandleKind { action, action_set, source };
struct VrActiveSet { VrActionHandle set = 0; VrActionHandle restricted_source = 0; };
struct VrHandSample {
    VrHmdPose grip;
    VrHmdPose aim;
    std::array<float, 5> finger_curl{};
    bool skeleton_valid = false;
};
struct VrControllerFrame {
    VrInputUpdateResult input;
    std::array<VrHandSample, 2> hands{};
    VrHand interact_source = VrHand::right;
    bool focused = false;
};

struct VrUiPointerPose {
    VrHmdPose pose;
    VrHand hand = VrHand::right;
    bool valid = false;
};

// Rework pointer ownership: prefer the configured dominant hand, fall back to
// the other tracked hand, and use the grip pose when an aim pose is missing.
[[nodiscard]] VrUiPointerPose SelectUiPointerPose(
    const VrControllerFrame& frame,
    VrHand preferred_hand) noexcept;

// SDK-neutral seam: production calls IVRInput; deterministic tests supply a
// fake device. Handles never escape into a game's entity/physics layer.
class VrActionBackend {
public:
    virtual ~VrActionBackend() = default;
    virtual bool Manifest(const char* absolute_utf8, std::string& error) = 0;
    virtual bool Resolve(const char* path, VrHandleKind kind, VrActionHandle& handle, std::string& error) = 0;
    virtual bool Activate(std::span<const VrActiveSet> sets, std::string& error) = 0;
    virtual bool Digital(VrActionHandle handle, VrButtonState& result, std::string& error) = 0;
    virtual bool Analog(VrActionHandle handle, VrAnalogState& result, std::string& error) = 0;
    virtual bool Pose(VrActionHandle handle, VrHmdPose& result) = 0;
    virtual bool Skeleton(VrActionHandle handle, std::array<float, 5>& curl) = 0;
    virtual bool Haptic(VrActionHandle handle, float duration, float frequency, float amplitude, std::string& error) = 0;
};

class VrActionInput final {
public:
    [[nodiscard]] bool Initialize(VrActionBackend& backend, const char* manifest, std::string& error);
    [[nodiscard]] bool Update(VrActionBackend& backend, VrInputContext context,
        VrHand handedness, bool focused, std::uint64_t now_ms,
        VrControllerFrame& frame, std::string& error);
    [[nodiscard]] bool TriggerHaptic(VrActionBackend& backend, VrHand hand,
        float duration, float frequency, float amplitude, std::string& error);
    void SetMoveDeadZone(float dead_zone) noexcept { router_.SetMoveDeadZone(dead_zone); }
    void Reset() noexcept;
    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
private:
    struct ContextHandles {
        VrActionHandle set = 0, move = 0, turn = 0;
        std::array<VrActionHandle, 10> gameplay{};
        VrActionHandle ui_set = 0;
        std::array<VrActionHandle, 4> ui{};
    };
    std::array<ContextHandles, 2> contexts_{}; // Physical left/right dominant hand.
    std::array<VrActionHandle, 2> sources_{}, grips_{}, aims_{}, skeletons_{}, haptics_{};
    VrActionHandle global_ = 0, offhand_ = 0, offhand_interact_ = 0, recenter_ = 0;
    VrInputRouter router_;
    bool initialized_ = false;
    bool focused_ = false;
};
} // namespace penumbra_vr::runtime
