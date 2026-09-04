#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

enum class EyeTargetProbeEvent : std::uint8_t {
    none,
    transient_validation,
    persistent_created,
    persistent_destroyed,
};

struct EyeTargetProbeTelemetry {
    EyeTargetProbeEvent event = EyeTargetProbeEvent::none;
    bool event_passed = false;
    bool state_restored = false;
    bool persistent_active = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t persistent_frames = 0;
    std::array<char, 192> error{};
};

void ResetEyeTargetProbe() noexcept;
void ProcessEyeTargetRequestsOnRenderThread() noexcept;

[[nodiscard]] bool RequestTransientEyeTargetValidation(
    std::string& error) noexcept;
[[nodiscard]] bool RequestPersistentEyeTargets(
    std::uint32_t width,
    std::uint32_t height,
    std::string& error) noexcept;
[[nodiscard]] bool DestroyPersistentEyeTargets(std::string& error) noexcept;

[[nodiscard]] bool PersistentEyeTargetsActive() noexcept;
[[nodiscard]] std::uint64_t PersistentEyeTargetLifetimeFrames() noexcept;
[[nodiscard]] EyeTargetProbeTelemetry ConsumeEyeTargetProbeTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
