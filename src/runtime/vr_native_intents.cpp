#include "vr_native_intents.hpp"
#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
namespace {
VrButtonState Merge(VrButtonState a, VrButtonState b) {
    return {a.active || b.active, a.pressed || b.pressed,
            a.just_pressed || b.just_pressed, (a.just_released || b.just_released) && !a.pressed && !b.pressed};
}
}
void VrNativeIntents::Begin(const VrInputState& s, VrInputContext context, float head_yaw) noexcept {
    buttons_ = {}; move_ = {};
    const auto set = [&](NativeVrAction action, VrButtonState value) {
        buttons_[static_cast<std::size_t>(action)] = value;
    };
    set(NativeVrAction::pause, Merge(s.pause, s.ui_close));
    if (context == VrInputContext::ui) {
        set(NativeVrAction::select, Merge(s.ui_select, s.ui_drag));
        set(NativeVrAction::back, s.ui_back);
        // Inventory/notebook close is routed through the native Escape path.
    } else {
        move_ = s.move;
        if (!std::isfinite(head_yaw)) move_ = {};
        else if (move_.active && std::isfinite(move_.x) && std::isfinite(move_.y)) {
            const float cosine = std::cos(head_yaw), sine = std::sin(head_yaw);
            move_.x = s.move.x * cosine + s.move.y * sine;
            move_.y = s.move.y * cosine - s.move.x * sine;
            // A square/raw axis pair must not exceed full-speed circular input.
            // Normalize after yaw rotation to preserve direction at every heading.
            const float magnitude = std::hypot(move_.x, move_.y);
            if (magnitude > 1) { move_.x /= magnitude; move_.y /= magnitude; }
        }
        set(NativeVrAction::inventory, s.inventory); set(NativeVrAction::notebook, s.notebook);
        set(NativeVrAction::light, s.quick_light); set(NativeVrAction::jump, s.jump);
        set(NativeVrAction::sprint, s.sprint); set(NativeVrAction::crouch, s.crouch);
        set(NativeVrAction::interact, s.interact); set(NativeVrAction::examine, s.examine);
        set(NativeVrAction::holster, s.holster);
    }
}
bool VrNativeIntents::Query(NativeVrAction action, NativeVrQuery query) noexcept {
    const auto index = static_cast<std::size_t>(action);
    if (index >= buttons_.size()) return false;
    auto& button = buttons_[index];
    if (query == NativeVrQuery::held) return button.pressed;
    bool& edge = query == NativeVrQuery::pressed ? button.just_pressed : button.just_released;
    const bool result = edge; edge = false; return result;
}
float VrNativeIntents::Move(float keyboard, bool sideways) const noexcept {
    if (!std::isfinite(keyboard)) return keyboard; // Do not repair unrelated native data.
    const float amount = sideways ? move_.x : move_.y;
    if (!move_.active || !std::isfinite(amount)) return keyboard;
    return std::clamp(keyboard + amount, -1.0F, 1.0F);
}
float VrSnapTurn::Update(const VrAnalogState& axis, bool gameplay, VrTurnMode mode,
    float dt, float snap_angle_degrees, float smooth_speed_degrees,
    float dead_zone) noexcept {
    constexpr float radians_per_degree = 0.01745329252F;
    if (!gameplay || mode == VrTurnMode::disabled || !axis.active ||
        !std::isfinite(axis.x) || !std::isfinite(dt) || dt < 0 || dt > 0.25F ||
        !std::isfinite(snap_angle_degrees) ||
        !std::isfinite(smooth_speed_degrees) || !std::isfinite(dead_zone)) {
        armed_ = false;
        return 0;
    }
    dead_zone = std::clamp(dead_zone, 0.0F, 0.9F);
    if (mode == VrTurnMode::smooth) {
        armed_ = false;
        const float magnitude = std::abs(axis.x);
        if (magnitude <= dead_zone) {
            return 0;
        }
        const float scaled = (magnitude - dead_zone) / (1 - dead_zone);
        return std::copysign(scaled, axis.x) *
            smooth_speed_degrees * radians_per_degree * dt;
    }
    if (std::abs(axis.x) <= dead_zone) {
        armed_ = true;
        return 0;
    }
    const float activation = std::min(1.0F, std::max(0.65F, dead_zone + 0.25F));
    if (!armed_ || std::abs(axis.x) < activation) {
        return 0;
    }
    armed_ = false;
    return std::copysign(snap_angle_degrees * radians_per_degree, axis.x);
}
}
