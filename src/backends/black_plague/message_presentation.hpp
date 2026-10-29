#pragma once
#include <string>

namespace penumbra_vr::backends::black_plague {
[[nodiscard]] bool MessagePresentationReady() noexcept;
[[nodiscard]] bool MessagePresentationHooksPresent() noexcept;
[[nodiscard]] bool InstallMessagePresentation(std::string& error) noexcept;
[[nodiscard]] bool RemoveMessagePresentation(std::string& error) noexcept;
}
