#pragma once

#include "opengl_enhanced_eye_stage.hpp"
#include "opengl_eye_targets.hpp"

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
    bool enhanced_visuals_requested = false;
    bool enhanced_visuals_available = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t persistent_frames = 0;
    std::array<char, 192> error{};
};

void ResetEyeTargetProbe() noexcept;
void ProcessEyeTargetRequestsOnRenderThread(bool count_frame = true) noexcept;
void ConfigurePersistentEyeEnhancedVisuals(bool enabled) noexcept;

[[nodiscard]] bool RequestTransientEyeTargetValidation(
    std::string& error) noexcept;
[[nodiscard]] bool RequestPersistentEyeTargets(
    std::uint32_t width,
    std::uint32_t height,
    std::string& error) noexcept;
[[nodiscard]] bool DestroyPersistentEyeTargets(std::string& error) noexcept;
[[nodiscard]] bool BeginPersistentEyeTarget(
    graphics::Eye eye,
    graphics::OpenGlEyeBinding& binding,
    std::string& error) noexcept;
[[nodiscard]] bool EndPersistentEyeTarget(
    graphics::OpenGlEyeBinding& binding,
    std::string& error) noexcept;
[[nodiscard]] bool BeginPersistentEnhancedEyeScene(
    graphics::Eye eye,
    graphics::OpenGlEnhancedEyeBinding& binding,
    std::string& error) noexcept;
[[nodiscard]] bool EndPersistentEnhancedEyeScene(
    graphics::OpenGlEnhancedEyeBinding& binding,
    std::string& error) noexcept;
[[nodiscard]] bool PersistentEyeEnhancedVisualsAvailable() noexcept;
[[nodiscard]] bool BeginGameplayOverlayTarget(
    graphics::OpenGlEyeBinding& binding,
    std::string& error) noexcept;
[[nodiscard]] bool EndGameplayOverlayTarget(
    graphics::OpenGlEyeBinding& binding,
    std::string& error) noexcept;
[[nodiscard]] bool GetGameplayOverlayColorTexture(
    std::uint32_t& color_texture,
    std::string& error) noexcept;
[[nodiscard]] bool GetPersistentEyeColorTextures(
    std::array<std::uint32_t, 2>& color_textures,
    std::string& error) noexcept;

[[nodiscard]] bool PersistentEyeTargetsActive() noexcept;
[[nodiscard]] std::uint64_t PersistentEyeTargetLifetimeFrames() noexcept;
[[nodiscard]] EyeTargetProbeTelemetry ConsumeEyeTargetProbeTelemetry() noexcept;

} // namespace penumbra_vr::backends::black_plague
