#include "openvr_session.hpp"

#include <array>
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
        const std::array<std::uint32_t, 2> zero_textures{};
        if (session.SubmitOpenGlEyeTextures(zero_textures, error) || error.empty()) {
            std::cerr << "OpenVR accepted zero OpenGL texture names\n";
            return 14;
        }
        std::array<penumbra_vr::runtime::VrEyeConfiguration, 2> eyes{};
        if (!session.ReadEyeConfiguration(eyes, error)) {
            std::cerr << error << '\n';
            return 11;
        }
        penumbra_vr::runtime::VrHmdPose pose;
        if (!session.WaitForHmdPose(pose, error)) {
            std::cerr << error << '\n';
            return 12;
        }
        std::cout << "OpenVR initialized; recommended per-eye render target: "
                  << size.width << 'x' << size.height << '\n'
                  << "left projection tangents: "
                  << eyes[0].left_tangent << ", "
                  << eyes[0].right_tangent << ", "
                  << eyes[0].top_tangent << ", "
                  << eyes[0].bottom_tangent << '\n'
                  << "right projection tangents: "
                  << eyes[1].left_tangent << ", "
                  << eyes[1].right_tangent << ", "
                  << eyes[1].top_tangent << ", "
                  << eyes[1].bottom_tangent << '\n'
                  << "eye-to-head X translations: "
                  << eyes[0].eye_to_head.values[3] << ", "
                  << eyes[1].eye_to_head.values[3] << '\n'
                  << "HMD connected=" << pose.device_connected
                  << " pose_valid=" << pose.pose_valid
                  << " tracking_result=" << pose.tracking_result << '\n';
        if (!session.Shutdown(error)) {
            std::cerr << error << '\n';
            return 13;
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
    std::array<penumbra_vr::runtime::VrEyeConfiguration, 2> eyes{};
    if (session.ReadEyeConfiguration(eyes, error) || error.empty()) {
        std::cerr << "Uninitialized OpenVR eye query did not fail closed\n";
        return 3;
    }
    penumbra_vr::runtime::VrHmdPose pose;
    if (session.WaitForHmdPose(pose, error) || error.empty()) {
        std::cerr << "Uninitialized OpenVR pose query did not fail closed\n";
        return 4;
    }
    const std::array<std::uint32_t, 2> textures{1, 2};
    if (session.SubmitOpenGlEyeTextures(textures, error) || error.empty()) {
        std::cerr << "Uninitialized OpenVR submission did not fail closed\n";
        return 5;
    }
    std::cout << "OpenVR session missing-loader failure path passed\n";
    penumbra_vr::runtime::VrControllerFrame controller_frame;
    controller_frame.focused = true;
    if (session.InitializeControllerInput(L"C:/missing/actions.json", error) || error.empty() ||
        session.controller_input_initialized() ||
        session.ReadControllerInput(penumbra_vr::runtime::VrInputContext::gameplay,
            penumbra_vr::runtime::VrHand::right, 0, controller_frame, error) ||
        controller_frame.focused || error.empty() ||
        session.TriggerHaptic(penumbra_vr::runtime::VrHand::left, 0.02F, 100, 0.5F, error)) {
        std::cerr << "Controller input did not reject an uninitialized session\n";
        return 6;
    }
    return 0;
}
