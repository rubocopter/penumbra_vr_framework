#include "openvr_session.hpp"

#include <iostream>
#include <string>

int main() {
    penumbra_vr::runtime::OpenVrSession session;
    std::string error;
    if (session.Initialize(
            L"Z:\\path-that-must-not-exist\\openvr_api.dll", error) ||
        error.empty() || session.initialized() ||
        session.recommended_render_target_size().width != 0 ||
        session.recommended_render_target_size().height != 0) {
        std::cerr << "OpenVR session did not reject a missing loader cleanly\n";
        return 1;
    }
    if (!session.Shutdown(error) || !error.empty()) {
        std::cerr << "Empty OpenVR session shutdown failed: " << error << '\n';
        return 2;
    }
    std::cout << "OpenVR session missing-loader failure path passed\n";
    return 0;
}
