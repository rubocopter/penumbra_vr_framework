#pragma once
#include "openvr_session.hpp"
#include <string>
namespace penumbra_vr::backends::black_plague {
[[nodiscard]] bool InstallNativeInputBridge(std::string& error) noexcept;
[[nodiscard]] bool RemoveNativeInputBridge(std::string& error) noexcept;
void ConnectNativeInput(runtime::OpenVrSession* session) noexcept;
[[nodiscard]] bool NativeInputUiActive() noexcept;
[[nodiscard]] runtime::VrControllerFrame ReadNativeControllerFrame() noexcept;
void NativeControllerHaptic(runtime::VrHand hand, bool pickup) noexcept;
}
