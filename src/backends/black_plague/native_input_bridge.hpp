#pragma once
#include "body_adapter_boundary.hpp"
#include "openvr_session.hpp"
#include "vr_update_timing.hpp"
#include "vr_settings.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace penumbra_vr::backends::black_plague {
[[nodiscard]] bool InstallNativeInputBridge(std::string& error) noexcept;
void ConfigureNativeInputBridge(runtime::VrSettings settings) noexcept;
[[nodiscard]] bool RemoveNativeInputBridge(std::string& error) noexcept;
void ConnectNativeInput(runtime::OpenVrSession* session) noexcept;
[[nodiscard]] bool NativeInputUiActive() noexcept;
[[nodiscard]] runtime::VrControllerFrame ReadNativeControllerFrame() noexcept;
// Last cPlayer observed at the exact-build ButtonHandler boundary. Consumers
// may compare it with native object links, but must not call game methods from
// a non-game thread.
[[nodiscard]] void* NativePlayerPointer() noexcept;
void NativeControllerHaptic(runtime::VrHand hand, bool pickup) noexcept;
[[nodiscard]] runtime::VrUpdateTimingSample ConsumeNativeUpdateTiming() noexcept;
// The input bridge is the sole owner of MoveForward/MoveSideways callsites.
// Consumers use this status to bind fan-out behavior without re-patching them.
[[nodiscard]] NativeMovementBoundaryStatus
ReadNativeMovementBoundaryStatus() noexcept;
}
