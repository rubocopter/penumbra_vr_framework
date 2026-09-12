#include "opengl_matrix_telemetry.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <iostream>
#include <string>

#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:__imp__glMatrixMode@4=__imp_glMatrixMode")
#pragma comment(linker, "/alternatename:__imp__glLoadMatrixf@4=__imp_glLoadMatrixf")
#pragma comment(linker, "/alternatename:__imp__glOrtho@48=__imp_glOrtho")
#endif

extern "C" __declspec(dllimport) void APIENTRY glMatrixMode(unsigned int mode);
extern "C" __declspec(dllimport) void APIENTRY glLoadMatrixf(const float* matrix);
extern "C" __declspec(dllimport) void APIENTRY glOrtho(
    double left, double right, double bottom, double top, double near_value, double far_value);
extern "C" __declspec(dllimport) unsigned long __cdecl OpenGlStub_MatrixModeCalls();
extern "C" __declspec(dllimport) unsigned long __cdecl OpenGlStub_LoadMatrixCalls();
extern "C" __declspec(dllimport) unsigned long __cdecl OpenGlStub_OrthoCalls();

namespace {

constexpr unsigned int kGlModelView = 0x1700;
constexpr unsigned int kGlProjection = 0x1701;
constexpr unsigned int kGlTexture = 0x1702;

__declspec(noinline) void CallOpenGlAfterUnhook(const std::array<float, 16>& matrix) {
    glMatrixMode(kGlProjection);
    glLoadMatrixf(matrix.data());
}

} // namespace

int main() {
    std::string error;
    if (!penumbra_vr::hooks::InstallOpenGlMatrixTelemetry(error)) {
        std::cerr << "InstallOpenGlMatrixTelemetry failed: " << error << '\n';
        return 1;
    }
    if (!penumbra_vr::hooks::InstallOpenGlMatrixTelemetry(error)) {
        std::cerr << "Idempotent telemetry installation failed: " << error << '\n';
        return 6;
    }

    const std::array<float, 16> projection{
        1.1F, 0.0F, 0.0F, 0.0F,
        0.0F, 2.2F, 0.0F, 0.0F,
        0.0F, 0.0F, -1.0F, -1.0F,
        0.0F, 0.0F, -0.2F, 0.0F,
    };
    const std::array<float, 16> model_view{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        3.0F, 4.0F, 5.0F, 1.0F,
    };
    const std::array<float, 16> second_model_view{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        6.0F, 7.0F, 8.0F, 1.0F,
    };

    glMatrixMode(kGlProjection);
    glLoadMatrixf(projection.data());
    glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 1.0);
    glMatrixMode(kGlModelView);
    glLoadMatrixf(model_view.data());
    glLoadMatrixf(model_view.data());
    glLoadMatrixf(second_model_view.data());
    glMatrixMode(kGlTexture);
    glLoadMatrixf(model_view.data());

    const penumbra_vr::hooks::OpenGlFrameTelemetry telemetry =
        penumbra_vr::hooks::ConsumeOpenGlFrameTelemetry();
    if (telemetry.matrix_mode_calls != 3 || telemetry.projection_loads != 1 ||
        telemetry.model_view_loads != 3 || telemetry.texture_loads != 1 ||
        telemetry.ortho_calls != 1 || !telemetry.has_projection ||
        telemetry.last_projection != projection ||
        telemetry.projection_call_stack_depth == 0 ||
        telemetry.unique_model_view_matrices != 2 ||
        telemetry.dropped_model_view_matrices != 0 ||
        telemetry.dominant_model_view_loads != 2 ||
        !telemetry.has_dominant_model_view ||
        telemetry.dominant_model_view != model_view) {
        std::cerr << "Unexpected OpenGL telemetry counters or projection matrix\n";
        return 2;
    }

    const penumbra_vr::hooks::OpenGlFrameTelemetry consumed =
        penumbra_vr::hooks::ConsumeOpenGlFrameTelemetry();
    if (consumed.matrix_mode_calls != 0 || consumed.projection_loads != 0 ||
        consumed.model_view_loads != 0 || consumed.texture_loads != 0 ||
        consumed.ortho_calls != 0 || consumed.has_projection ||
        consumed.unique_model_view_matrices != 0 ||
        consumed.dropped_model_view_matrices != 0 ||
        consumed.dominant_model_view_loads != 0 ||
        consumed.has_dominant_model_view) {
        std::cerr << "Telemetry was not reset after consumption\n";
        return 3;
    }

    if (!penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(error)) {
        std::cerr << "RemoveOpenGlMatrixTelemetry failed: " << error << '\n';
        return 4;
    }
    if (!penumbra_vr::hooks::RemoveOpenGlMatrixTelemetry(error)) {
        std::cerr << "Idempotent telemetry removal failed: " << error << '\n';
        return 7;
    }

    CallOpenGlAfterUnhook(projection);
    if (OpenGlStub_MatrixModeCalls() != 4 || OpenGlStub_LoadMatrixCalls() != 6 ||
        OpenGlStub_OrthoCalls() != 1) {
        std::cerr << "OpenGL calls were not forwarded or restored correctly\n";
        return 5;
    }

    std::cout << "OpenGL matrix telemetry and restoration passed\n";
    return 0;
}
