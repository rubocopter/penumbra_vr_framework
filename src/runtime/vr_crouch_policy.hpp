#pragma once

#include "vr_input_state.hpp"
#include "vr_settings.hpp"

#include <cstdint>

namespace penumbra_vr::runtime {

namespace vr_crouch_policy {
inline constexpr float kMinimumPlausibleHeadHeight = 0.90F;
inline constexpr float kMaximumPlausibleHeadHeight = 2.20F;
inline constexpr float kExitHysteresis = 0.08F;
} // namespace vr_crouch_policy

struct VrPhysicalCrouchStatus {
    bool physical_enabled = false;
    bool tracking_valid = false;
    float head_height = 0.0F;
    bool standing_height_known = false;
    float standing_height = 0.0F;
    float enter_height = 0.0F;
    float exit_height = 0.0F;
    bool physical_crouch = false;
    bool button_latched = false;
    bool desired_crouch = false;
    bool stand_blocked = false;
    bool stand_release_pending = false;
    bool effective_crouch = false;
    std::uint64_t physical_entries = 0;
    std::uint64_t physical_exits = 0;
};

// Game-neutral port of Rework 23c890f's tracked-height crouch policy. The
// target backend remains responsible for turning the resulting logical crouch
// button into its own native move state/body shape.
class VrPhysicalCrouchPolicy final {
public:
    [[nodiscard]] VrButtonState Update(
        const VrButtonState& button,
        VrCrouchMode mode,
        float physical_crouch_depth,
        bool gameplay_active,
        bool tracking_valid,
        float head_height,
        bool stand_blocked = false) noexcept;

    void Reset() noexcept;
    [[nodiscard]] const VrPhysicalCrouchStatus& status() const noexcept;

private:
    bool standing_height_known_ = false;
    float standing_height_ = 0.0F;
    bool physical_crouch_ = false;
    bool button_latched_ = false;
    bool stand_release_pending_ = false;
    bool effective_crouch_ = false;
    std::uint64_t physical_entries_ = 0;
    std::uint64_t physical_exits_ = 0;
    VrPhysicalCrouchStatus status_{};
};

} // namespace penumbra_vr::runtime
