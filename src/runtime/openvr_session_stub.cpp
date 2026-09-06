#include "openvr_session.hpp"

namespace penumbra_vr::runtime {

OpenVrSession::~OpenVrSession() noexcept = default;

bool OpenVrSession::InitializeControllerInput(const std::wstring&, std::string& error) noexcept {
    error = "This build was compiled without an OpenVR SDK";
    return false;
}
bool OpenVrSession::ReadControllerInput(VrInputContext, VrHand, std::uint64_t,
    VrControllerFrame& frame, std::string& error) noexcept {
    frame = {};
    error = "This build was compiled without an OpenVR SDK";
    return false;
}
void OpenVrSession::SetControllerMoveDeadZone(float) noexcept {}
bool OpenVrSession::TriggerHaptic(VrHand, float, float, float, std::string& error) noexcept {
    error = "This build was compiled without an OpenVR SDK";
    return false;
}
bool OpenVrSession::controller_input_initialized() const noexcept { return false; }

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

bool OpenVrSession::ReadEyeConfiguration(
    std::array<VrEyeConfiguration, 2>& eyes,
    std::string& error) const noexcept {
    eyes = {};
    error = "This build was compiled without an OpenVR SDK";
    return false;
}

bool OpenVrSession::WaitForHmdPose(
    VrHmdPose& pose,
    std::string& error) const noexcept {
    pose = {};
    error = "This build was compiled without an OpenVR SDK";
    return false;
}

bool OpenVrSession::SubmitOpenGlEyeTextures(
    const std::array<std::uint32_t, 2>&,
    std::string& error) const noexcept {
    error = "This build was compiled without an OpenVR SDK";
    return false;
}

bool OpenVrSession::initialized() const noexcept {
    return false;
}

VrRenderTargetSize OpenVrSession::recommended_render_target_size() const noexcept {
    return {};
}

} // namespace penumbra_vr::runtime
