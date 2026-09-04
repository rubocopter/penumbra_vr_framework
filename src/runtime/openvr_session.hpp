#pragma once

#include <cstdint>
#include <string>

namespace penumbra_vr::runtime {

struct VrRenderTargetSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
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

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] VrRenderTargetSize recommended_render_target_size() const noexcept;

private:
    void* library_ = nullptr;
    void* system_ = nullptr;
    void* shutdown_ = nullptr;
    VrRenderTargetSize recommended_size_{};
};

} // namespace penumbra_vr::runtime
