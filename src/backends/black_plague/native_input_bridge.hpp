#pragma once
#include "openvr_session.hpp"
#include "vr_update_timing.hpp"
#include "vr_settings.hpp"
#include <string>
namespace penumbra_vr::backends::black_plague {
[[nodiscard]] bool InstallNativeInputBridge(std::string& error) noexcept;
void ConfigureNativeInputBridge(runtime::VrSettings settings) noexcept;
[[nodiscard]] bool RemoveNativeInputBridge(std::string& error) noexcept;
void ConnectNativeInput(runtime::OpenVrSession* session) noexcept;
[[nodiscard]] bool NativeInputUiActive() noexcept;
[[nodiscard]] runtime::VrControllerFrame ReadNativeControllerFrame() noexcept;
void NativeControllerHaptic(runtime::VrHand hand, bool pickup) noexcept;
[[nodiscard]] runtime::VrUpdateTimingSample ConsumeNativeUpdateTiming() noexcept;
}
