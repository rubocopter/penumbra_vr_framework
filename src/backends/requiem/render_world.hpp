#pragma once

#include "openvr_session.hpp"

#include <cstdint>
#include <string>

namespace penumbra_vr::backends::requiem {

[[nodiscard]] bool InstallRenderWorld(std::string& error) noexcept;
[[nodiscard]] bool RenderHooksInstalled() noexcept;
[[nodiscard]] bool RemoveRenderWorld(std::string& error) noexcept;
[[nodiscard]] bool StartPresentation(runtime::OpenVrSession& session,
    std::string& error) noexcept;
[[nodiscard]] bool StopPresentation(std::string& error) noexcept;
void OnSdlSwap(std::uint64_t frame_number) noexcept;

} // namespace penumbra_vr::backends::requiem
