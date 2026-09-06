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

// Consumes an edge once even when the native handler checks several aliases
// (e.g. LeftClick and Interact). Held queries are deliberately repeatable.
class VrNativeIntents final {
public:
    void Begin(const VrInputState& state, VrInputContext context) noexcept;
    [[nodiscard]] bool Query(NativeVrAction action, NativeVrQuery query) noexcept;
    [[nodiscard]] float Move(float keyboard, bool sideways) const noexcept;
private:
    std::array<VrButtonState, static_cast<std::size_t>(NativeVrAction::count)> buttons_{};
    VrAnalogState move_;
};

class VrSnapTurn final {
public:
    // Returns native yaw-input radians (positive stick = turn right). Returning
    // to the neutral zone is required before another 45-degree step.
    [[nodiscard]] float Update(const VrAnalogState& axis, bool gameplay) noexcept;
private:
    bool armed_ = false;
};
}
