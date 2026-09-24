#pragma once

namespace penumbra_vr::backends::black_plague {

struct BlackPlaguePanelGeometry {
    float distance;
    float width;
    float center_y;
};

struct BlackPlagueOverlayGeometry {
    float left;
    float right;
    float bottom;
    float top;
    float distance;
};

// HPL can leave the closing inventory/notebook sprites in DrawAll's queue
// for the first world frame. Capture that frame at the previous panel pose.
[[nodiscard]] constexpr bool BlackPlaguePreserveWorldPanelOnExit(
    bool world_ui, bool any_ui, bool previous_panel_valid) noexcept {
    return !world_ui && !any_ui && previous_panel_valid;
}

// Rework Inventory::SetActive places the 800-pixel surface 1.1 m from the
// player at 1/750 m per pixel. Keep the pointer and rendered panel in sync.
[[nodiscard]] constexpr BlackPlaguePanelGeometry BlackPlagueInventoryPanel(
    float ui_scale) noexcept {
    return {1.1F, 800.0F / 750.0F * ui_scale,
        -100.0F / 750.0F * ui_scale};
}

// Rework carries its 800x600 notebook surface on the off hand at 1/1450 m
// per pixel. The visible 350x460 book is centered on that surface.
[[nodiscard]] constexpr BlackPlaguePanelGeometry BlackPlagueNotebookPanel(
    float ui_scale) noexcept {
    return {0.02F, 800.0F / 1450.0F * ui_scale, 0.0F};
}

[[nodiscard]] constexpr BlackPlaguePanelGeometry BlackPlagueFullscreenPanel(
    float ui_scale, float ui_distance) noexcept {
    return {ui_distance, 2.4F * ui_scale, 0.0F};
}

// Rework anchors the 800x600 message plane at UI distance with y=300 centered
// and a 1/750 m pixel pitch. Black Plague captures native HUD and messages in
// one queue, so scale that surface while preserving Rework's visible text area.
[[nodiscard]] constexpr BlackPlagueOverlayGeometry BlackPlagueGameplayOverlay(
    float subtitle_scale, float ui_distance) noexcept {
    constexpr float half_height = 300.0F / 750.0F;
    return {
        -400.0F / 750.0F * subtitle_scale,
         400.0F / 750.0F * subtitle_scale,
         -half_height * subtitle_scale,
          half_height * subtitle_scale,
         ui_distance,
    };
}

} // namespace penumbra_vr::backends::black_plague
