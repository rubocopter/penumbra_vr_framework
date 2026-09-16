#pragma once

#include "vr_hand_pose.hpp"

#include <cstddef>

namespace penumbra_vr::graphics {

struct ReworkHandMeshStats {
    std::size_t positions = 0;
    std::size_t triangles = 0;
    std::size_t joints = 0;
};

[[nodiscard]] ReworkHandMeshStats GetReworkHandMeshStats(bool left) noexcept;

// Draws the proven Rework hand geometry/rig at the current OpenGL model-view
// transform. Finger intent remains Framework-owned through VrHandArticulation;
// this adapter only maps that intent onto the imported bind hierarchy.
[[nodiscard]] bool DrawReworkHandMesh(
    const runtime::VrHandArticulation& articulation,
    bool left) noexcept;

} // namespace penumbra_vr::graphics
