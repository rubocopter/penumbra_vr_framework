#pragma once

#include "vr_action_input.hpp"
#include "vr_tracking_types.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::runtime {

struct VrRenderTargetSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct VrEyeConfiguration {
    float left_tangent = 0.0F;
    float right_tangent = 0.0F;
    float top_tangent = 0.0F;
    float bottom_tangent = 0.0F;
    VrMatrix34 eye_to_head;
};

class OpenVrSession final {
public:
    OpenVrSession() noexcept = default;
    ~OpenVrSession() noexcept;
    OpenVrSession(const OpenVrSession&) = delete;
    OpenVrSession& operator=(const OpenVrSession&) = delete;

    [[nodiscard]] bool Initialize(
        const std::wstring& loader_path,
        std::string& error) noexcept;
    [[nodiscard]] bool Shutdown(std::string& error) noexcept;

    [[nodiscard]] bool InitializeControllerInput(
        const std::wstring& manifest_path, std::string& error) noexcept;
    [[nodiscard]] bool ReadControllerInput(
        VrInputContext context, VrHand handedness, std::uint64_t now_ms,
        VrControllerFrame& frame, std::string& error) noexcept;
    [[nodiscard]] bool TriggerHaptic(
        VrHand hand, float duration, float frequency, float amplitude,
        std::string& error) noexcept;
    void SetControllerMoveDeadZone(float dead_zone) noexcept;
    [[nodiscard]] bool controller_input_initialized() const noexcept;

    [[nodiscard]] bool ReadEyeConfiguration(
        std::array<VrEyeConfiguration, 2>& eyes,
        std::string& error) const noexcept;
    [[nodiscard]] bool WaitForHmdPose(
        VrHmdPose& pose,
        std::string& error) const noexcept;
    [[nodiscard]] bool SubmitOpenGlEyeTextures(
        const std::array<std::uint32_t, 2>& color_textures,
        std::string& error) const noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] VrRenderTargetSize recommended_render_target_size() const noexcept;

private:
    void* library_ = nullptr;
    void* system_ = nullptr;
    void* compositor_ = nullptr;
    void* shutdown_ = nullptr;
    void* input_ = nullptr;
    VrActionInput actions_;
    VrRenderTargetSize recommended_size_{};
};

} // namespace penumbra_vr::runtime
