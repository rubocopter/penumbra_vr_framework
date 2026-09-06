#pragma once

#include "openvr_session.hpp"

#include <array>
#include <string>

namespace penumbra_vr::runtime {

// Row-major storage using the same column-vector convention as HPL1 and OpenVR.
struct VrMatrix44 {
    std::array<float, 16> values{};
};

struct VrCullFrustum {
    float vertical_fov_radians = 0.0F;
    float aspect = 0.0F;
};

[[nodiscard]] VrMatrix44 IdentityMatrix() noexcept;
// Same 2.4 m wide, 2 m distant panel as OpenGlMenuFrame; UV origin is top-left.
[[nodiscard]] bool ProjectAimOnMenu(const VrMatrix34& anchor, const VrMatrix34& aim,
    float aspect, std::array<float, 2>& uv) noexcept;
[[nodiscard]] VrMatrix44 ExpandMatrix(const VrMatrix34& matrix) noexcept;
[[nodiscard]] VrMatrix44 Multiply(
    const VrMatrix44& left,
    const VrMatrix44& right) noexcept;

// OpenVR eye-to-head transforms are rigid. The inverse maps head space to eye
// space and can therefore be prepended to an HPL world-to-head view matrix.
[[nodiscard]] bool InvertRigidTransform(
    const VrMatrix34& transform,
    VrMatrix44& inverse,
    std::string& error) noexcept;
[[nodiscard]] bool ComposeEyeViewFromHeadView(
    const VrMatrix44& head_view,
    const VrMatrix34& eye_to_head,
    VrMatrix44& eye_view,
    std::string& error) noexcept;

// Re-centers tracking at anchor_device_to_absolute, converts current HMD
// motion to a view-space offset, and prepends it to the game's head view.
// Set world_units_per_meter to zero for rotation-only validation.
[[nodiscard]] bool ComposeRelativeTrackedHeadView(
    const VrMatrix44& game_head_view,
    const VrMatrix34& anchor_device_to_absolute,
    const VrMatrix34& current_device_to_absolute,
    float world_units_per_meter,
    VrMatrix44& tracked_head_view,
    std::string& error) noexcept;

// Preserves the raw OpenVR pitch and roll while aligning only the anchor yaw
// with the game's current camera heading. This follows the tracking-space
// boundary proven by Penumbra Overture VR Rework, adapted to runtime-neutral
// matrices and an exact-build camera adapter.
[[nodiscard]] bool ComposeYawRecenteredTrackedHeadView(
    const VrMatrix44& game_head_view,
    const VrMatrix34& anchor_device_to_absolute,
    const VrMatrix34& current_device_to_absolute,
    float world_units_per_meter,
    VrMatrix44& tracked_head_view,
    std::string& error) noexcept;

// Builds the infinite-far OpenGL projection used by the mapped HPL1 camera
// path from the raw per-eye tangents returned by OpenVR.
[[nodiscard]] bool BuildHplInfiniteProjection(
    const VrEyeConfiguration& eye,
    float near_clip,
    VrMatrix44& projection,
    std::string& error) noexcept;

// Produces one symmetric frustum containing both asymmetric eye frusta. A
// small angular guard can cover the pose age between visibility preparation
// and rendering without changing either submitted eye projection.
[[nodiscard]] bool BuildConservativeStereoCullFrustum(
    const std::array<VrEyeConfiguration, 2>& eyes,
    float angular_guard_radians,
    VrCullFrustum& frustum,
    std::string& error) noexcept;

} // namespace penumbra_vr::runtime
