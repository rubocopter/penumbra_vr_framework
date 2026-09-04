#include "openvr_session.hpp"

namespace penumbra_vr::runtime {

OpenVrSession::~OpenVrSession() noexcept = default;

bool OpenVrSession::Initialize(
    const std::wstring&,
    std::string& error) noexcept {
    error = "This build was compiled without an OpenVR SDK";
    return false;
}

bool OpenVrSession::Shutdown(std::string& error) noexcept {
    error.clear();
    return true;
}

bool OpenVrSession::initialized() const noexcept {
    return false;
}

VrRenderTargetSize OpenVrSession::recommended_render_target_size() const noexcept {
    return {};
}

} // namespace penumbra_vr::runtime
