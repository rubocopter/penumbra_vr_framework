#include "openvr_session.hpp"

#include <iostream>
#include <string>

int wmain(int argc, wchar_t** argv) {
    penumbra_vr::runtime::OpenVrSession session;
    std::string error;

    if (argc == 2) {
        if (!session.Initialize(argv[1], error)) {
            std::cerr << error << '\n';
            return 10;
        }
        const penumbra_vr::runtime::VrRenderTargetSize size =
            session.recommended_render_target_size();
        std::cout << "OpenVR initialized; recommended per-eye render target: "
                  << size.width << 'x' << size.height << '\n';
        if (!session.Shutdown(error)) {
            std::cerr << error << '\n';
            return 11;
        }
        return 0;
    }
    if (argc != 1) {
        std::cerr << "Usage: pvr_openvr_session_test.exe [path-to-openvr_api.dll]\n";
        return 2;
    }

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
