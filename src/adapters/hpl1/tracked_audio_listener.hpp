#pragma once

#include "vr_math.hpp"

#include <string>

namespace penumbra_vr::adapters::hpl1 {

// Each binary backend owns its eligibility predicate (presentation, focus, UI).
using AudioListenerEligible = bool (*)() noexcept;
[[nodiscard]] bool InstallTrackedAudioListener(
    AudioListenerEligible eligible, std::string& error) noexcept;
// Called before eye-camera overrides with the reconciled current head world pose.
// Also writes the native OpenAL consumer immediately; native updates thereafter
// retain this pose until invalidation or freshness expiry.
void PublishTrackedAudioListener(const runtime::VrMatrix44& head_world) noexcept;
void InvalidateTrackedAudioListener() noexcept;
[[nodiscard]] bool RemoveTrackedAudioListener(std::string& error) noexcept;

} // namespace penumbra_vr::adapters::hpl1
