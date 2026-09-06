#pragma once
#include <string>
#include <cstdint>
namespace penumbra_vr::backends::black_plague {
struct SpatialDiagnostics {
    std::uint64_t tools_attached=0, tools_native=0, invalid_tool_pose=0, blocked_grabs=0;
    std::uint64_t grabs_acquired=0, grabs_released=0;
    std::uint64_t guarded_releases=0, collision_restore_failures=0;
};
[[nodiscard]] SpatialDiagnostics ConsumeSpatialDiagnostics() noexcept;
[[nodiscard]] bool InstallSpatialInteraction(std::string& error) noexcept;
[[nodiscard]] bool RemoveSpatialInteraction(std::string& error) noexcept;
// Called only by ButtonHandler on the native game thread, never by IPC.
void RefreshVrSelectionBeforeInteract(void* player) noexcept;
void ServiceSpatialInteraction(void* player, bool ui) noexcept;
}
