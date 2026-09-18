#pragma once

#include <cstdint>
#include <string>

namespace penumbra_vr::backends::black_plague {

struct AudioEnvironmentTelemetry {
    bool valid = false;
    bool late_object_found = false;
    bool late_native_default = false;
    bool bus_trim_applied = false;
    std::uint64_t init_attach_calls = 0;
    std::uint64_t preset_applications = 0;
    std::uint64_t initial_bus_trim_substitutions = 0;
    std::uint64_t late_bootstrap_applications = 0;
    std::uint64_t late_existing_environment_preserved = 0;
};

[[nodiscard]] bool InstallAudioEnvironmentProbe(std::string& error) noexcept;
[[nodiscard]] bool RemoveAudioEnvironmentProbe(std::string& error) noexcept;
[[nodiscard]] AudioEnvironmentTelemetry ReadAudioEnvironmentTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
