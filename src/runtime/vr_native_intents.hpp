#pragma once
#include "vr_input_state.hpp"
#include <array>
#include <cstdint>

namespace penumbra_vr::runtime {
enum class NativeVrAction : std::uint8_t {
    pause, select, back, inventory, notebook, light, jump, sprint, crouch,
    interact, examine, holster, count
};
enum class NativeVrQuery : std::uint8_t { held, pressed, released };
struct VrQuickLightPlan { bool toggle_glow; bool toggle_flashlight; };
// Rework order: off -> glowstick -> flashlight -> off.
[[nodiscard]] constexpr VrQuickLightPlan PlanQuickLight(bool glow, bool flashlight) noexcept {
    return {glow || !flashlight, glow || flashlight};
}

// Consumes an edge once even when the native handler checks several aliases
// (e.g. LeftClick and Interact). Held queries are deliberately repeatable.
class VrNativeIntents final {
public:
    void Begin(const VrInputState& state, VrInputContext context, float head_yaw = 0) noexcept;
    [[nodiscard]] bool Query(NativeVrAction action, NativeVrQuery query) noexcept;
    [[nodiscard]] float Move(float keyboard, bool sideways) const noexcept;
private:
    std::array<VrButtonState, static_cast<std::size_t>(NativeVrAction::count)> buttons_{};
    VrAnalogState move_;
};

class VrSnapTurn final {
public:
    // Returns native yaw-input radians. Snap requires a neutral return; smooth
    // integrates configured angular speed once per native update.
    [[nodiscard]] float Update(const VrAnalogState& axis, bool gameplay,
        VrTurnMode mode, float dt, float snap_angle_degrees,
        float smooth_speed_degrees, float dead_zone) noexcept;
private:
    bool armed_ = false;
};
}
