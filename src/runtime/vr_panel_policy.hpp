#pragma once

namespace penumbra_vr::runtime {

struct VrStablePanelAnchorPlan {
    bool capture_current_pose = false;
    bool anchor_valid_after = false;
};

// A stable panel captures the current HMD pose when it first becomes active or
// after recentering, then retains that room-space anchor until the panel closes.
[[nodiscard]] constexpr VrStablePanelAnchorPlan PlanStablePanelAnchor(
    bool panel_active,
    bool recenter_requested,
    bool anchor_valid) noexcept {
    if (!panel_active) {
        return {};
    }
    if (recenter_requested || !anchor_valid) {
        return {true, true};
    }
    return {false, true};
}

enum class VrTransientOverlayAction : unsigned char {
    none,
    anchor_world_position,
    restore_face_locked,
};

struct VrTransientOverlayPlan {
    VrTransientOverlayAction action = VrTransientOverlayAction::none;
    bool anchored_after = false;
};

// Rework lets transient text own the overlay only while no stronger panel is
// active. If another panel takes over, release ownership without restoring
// facelock so the new owner is not clobbered.
[[nodiscard]] constexpr VrTransientOverlayPlan PlanTransientOverlayHandoff(
    bool overlay_active,
    bool blocking_panel_active,
    bool overlay_was_anchored) noexcept {
    if (overlay_active && !blocking_panel_active) {
        return {VrTransientOverlayAction::anchor_world_position, true};
    }
    if (overlay_was_anchored && !overlay_active && !blocking_panel_active) {
        return {VrTransientOverlayAction::restore_face_locked, false};
    }
    return {};
}

} // namespace penumbra_vr::runtime
