#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace penumbra_vr::runtime {

struct VrRenderTargetSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct VrMatrix34 {
    std::array<float, 12> values{};
};

struct VrEyeConfiguration {
    float left_tangent = 0.0F;
    float right_tangent = 0.0F;
    float top_tangent = 0.0F;
    float bottom_tangent = 0.0F;
    VrMatrix34 eye_to_head;
};

struct VrHmdPose {
    VrMatrix34 device_to_absolute;
    std::array<float, 3> velocity{};
    std::array<float, 3> angular_velocity{};
    std::uint32_t tracking_result = 0;
    bool pose_valid = false;
    bool device_connected = false;
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

    [[nodiscard]] bool ReadEyeConfiguration(
        std::array<VrEyeConfiguration, 2>& eyes,
        std::string& error) const noexcept;
    [[nodiscard]] bool WaitForHmdPose(
        VrHmdPose& pose,
        std::string& error) const noexcept;

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] VrRenderTargetSize recommended_render_target_size() const noexcept;

private:
    void* library_ = nullptr;
    void* system_ = nullptr;
    void* compositor_ = nullptr;
    void* shutdown_ = nullptr;
    VrRenderTargetSize recommended_size_{};
};

} // namespace penumbra_vr::runtime
