#pragma once

#include <cstdint>
#include <string>

namespace penumbra_vr::hooks {

using FrameCallback = void (*)(std::uint64_t frame_number) noexcept;

[[nodiscard]] bool InstallSdlSwapHook(FrameCallback callback, std::string& error) noexcept;
[[nodiscard]] bool RemoveSdlSwapHook(std::string& error) noexcept;
[[nodiscard]] std::uint64_t ObservedFrameCount() noexcept;

} // namespace penumbra_vr::hooks
