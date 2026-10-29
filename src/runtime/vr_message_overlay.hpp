#pragma once

namespace penumbra_vr::runtime {

struct VrMessageOverlayGeometry {
    float left;
    float right;
    float bottom;
    float top;
    float distance;
};

// BP's proven captured 800x600 HUD/message plane: y=300 is head-centered,
// with Rework's 1/750 m pixel pitch. The backend owns settings normalization,
// panel selection and distance. Scale the queue once, independently of the
// GameMessage-only native font adaptation.
[[nodiscard]] constexpr VrMessageOverlayGeometry GameplayMessageOverlay(
    float subtitle_scale, float distance) noexcept {
    return {-400.0F / 750.0F * subtitle_scale,
             400.0F / 750.0F * subtitle_scale,
            -300.0F / 750.0F * subtitle_scale,
             300.0F / 750.0F * subtitle_scale,
             distance};
}

} // namespace penumbra_vr::runtime
