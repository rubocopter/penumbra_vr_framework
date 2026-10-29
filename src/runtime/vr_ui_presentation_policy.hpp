#pragma once

namespace penumbra_vr::runtime::vr_ui_presentation_policy {
struct PanelGeometry {
    float distance;
    float width;
    float center_y;
};
// Rework's native surfaces as consumed by BP. A panel's render and pointer
// owners must use the same geometry. Native queue/pose ownership stays local.
[[nodiscard]] constexpr PanelGeometry InventoryPanel(float ui_scale) noexcept {
    return {1.1F,800.0F/750.0F*ui_scale,-100.0F/750.0F*ui_scale};
}
[[nodiscard]] constexpr PanelGeometry NotebookPanel(float ui_scale) noexcept {
    return {0.02F,800.0F/1450.0F*ui_scale,0.0F};
}
[[nodiscard]] constexpr PanelGeometry FullscreenPanel(
    float ui_scale,float ui_distance) noexcept {
    return {ui_distance,2.4F*ui_scale,0.0F};
}
} // namespace penumbra_vr::runtime::vr_ui_presentation_policy
