#pragma once

#include "vr_settings.hpp"

#include <cstdint>

namespace penumbra_vr::runtime {

enum class VrInputContext : std::uint8_t {
    gameplay,
    ui,
};

enum class VrHand : std::uint8_t {
    left,
    right,
};

struct VrButtonState {
    bool active = false;
    bool pressed = false;
    bool just_pressed = false;
    bool just_released = false;
};

struct VrAnalogState {
    bool active = false;
    float x = 0.0F;
    float y = 0.0F;
};

struct VrInputState {
    VrAnalogState move;
    VrAnalogState turn;

    VrButtonState sprint;
    VrButtonState interact;
    VrButtonState examine;
    VrButtonState holster;
    VrButtonState inventory;
    VrButtonState notebook;
    VrButtonState quick_light;
    VrButtonState jump;
    VrButtonState crouch;
    VrButtonState pause;
    VrButtonState recenter;

    VrButtonState ui_select;
    VrButtonState ui_drag;
    VrButtonState ui_back;
    VrButtonState ui_close;
};

enum class VrInputUpdateStatus : std::uint8_t {
    actions_active,
    action_idle_grace,
    fallback_required,
};

struct VrInputUpdateResult {
    VrInputUpdateStatus status = VrInputUpdateStatus::fallback_required;
    VrInputState state;
};

[[nodiscard]] VrButtonState MakeVrButtonState(
    bool active,
    bool pressed,
    bool changed) noexcept;
[[nodiscard]] float ClampStickDeadZone(float dead_zone) noexcept;
void ApplyStickDeadZone(VrAnalogState& state, float dead_zone) noexcept;
void SuppressAllButtonEdges(VrInputState& state) noexcept;
[[nodiscard]] VrInputState MakeReleasedVrInputState(const VrInputState& previous) noexcept;
[[nodiscard]] bool AnyActionActive(const VrInputState& state) noexcept;
[[nodiscard]] VrHand OppositeHand(VrHand hand) noexcept;

// Device-independent action-state behavior adapted from Overture Rework. An
// OpenVR reader supplies raw actions; a game backend consumes the filtered
// intents without depending on OpenVR handles or HPL types.
class VrInputRouter final {
public:
    [[nodiscard]] VrInputUpdateResult Update(
        VrInputState sample,
        VrInputContext context,
        VrHand handedness,
        bool left_pose_valid,
        bool right_pose_valid,
        std::uint64_t now_milliseconds) noexcept;

    void Reset() noexcept;
    void SetMoveDeadZone(float dead_zone) noexcept;
    void SetInteractSourceHand(VrHand hand) noexcept;

    [[nodiscard]] const VrInputState& state() const noexcept;
    [[nodiscard]] float move_dead_zone() const noexcept;
    [[nodiscard]] VrHand interact_source_hand() const noexcept;
    [[nodiscard]] bool using_actions() const noexcept;

private:
    static constexpr std::uint64_t kActionIdleGraceMilliseconds = 500;

    VrInputState state_;
    float move_dead_zone_ = vr_setting_limits::kMoveDeadZone.default_value;
    VrHand interact_source_hand_ = VrHand::right;
    VrInputContext active_context_ = VrInputContext::gameplay;
    VrHand active_handedness_ = VrHand::right;
    std::uint64_t idle_since_milliseconds_ = 0;
    bool using_actions_ = false;
    bool has_active_context_ = false;
    bool idle_timer_started_ = false;
};

} // namespace penumbra_vr::runtime
