#pragma once
#include <string>
namespace penumbra_vr::backends::black_plague {
[[nodiscard]] bool InstallSpatialInteraction(std::string& error) noexcept;
[[nodiscard]] bool RemoveSpatialInteraction(std::string& error) noexcept;
// Called only by ButtonHandler on the native game thread, never by IPC.
void RefreshVrSelectionBeforeInteract(void* player) noexcept;
void ServiceSpatialInteraction(void* player, bool ui) noexcept;
}
