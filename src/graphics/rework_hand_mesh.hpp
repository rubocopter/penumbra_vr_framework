#pragma once

#include "vr_hand_pose.hpp"

#include <cstddef>

#ifdef _WIN32
// rework_hand_mesh.cpp decodes the original Rework JPEG through GDI+ while
// keeping the Windows surface lean. GDI+ still requires the COM stream/property
// declarations omitted by WIN32_LEAN_AND_MEAN, so include them explicitly.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#endif

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
