#pragma once
#include <string>

namespace penumbra_vr::backends::requiem {
// The render owner supplies its presentation state; native UI stays native.
[[nodiscard]] bool TrackedStereoPresentationActive() noexcept;
[[nodiscard]] bool MessagePresentationReady() noexcept;
[[nodiscard]] bool MessagePresentationHooksPresent() noexcept;
[[nodiscard]] bool InstallMessagePresentation(std::string& error) noexcept;
[[nodiscard]] bool RemoveMessagePresentation(std::string& error) noexcept;
}
