#include "refraction_copy_gate.hpp"

#include <iostream>

int main() {
    using penumbra_vr::backends::requiem::RefractionCopyRequest;
    using penumbra_vr::backends::requiem::ShouldCaptureRefractionEye;

    constexpr std::uintptr_t image = 0x400000;
    RefractionCopyRequest request{
        .image_base = image,
        .return_address = image + 0x12C27F,
        .inside_eye = true,
        .expected_framebuffer = 9,
        .current_framebuffer = 9,
        .viewport = {0, 0, 3400, 3468},
        .eye_width = 3400,
        .eye_height = 3468,
        .native_width = 2560,
        .native_height = 1440,
        .rectangle_texture = 27,
        .max_rectangle_size = 16384,
    };
    if (!ShouldCaptureRefractionEye(request)) {
        std::cerr << "The mapped clipped refraction call was rejected\n";
        return 1;
    }
    request.return_address = image + 0x12C2CE;
    if (!ShouldCaptureRefractionEye(request)) {
        std::cerr << "The mapped full-screen refraction call was rejected\n";
        return 2;
    }
    request.return_address = image + 0x12C2D4;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "An unrelated screen copy was accepted\n";
        return 3;
    }
    request.return_address = image + 0x12C27F;
    request.current_framebuffer = 11;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "A different framebuffer was accepted\n";
        return 4;
    }
    request.current_framebuffer = 9;
    request.viewport[3] = 1440;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "A stale viewport was accepted\n";
        return 5;
    }
    request.viewport[3] = 3468;
    request.rectangle_texture = 0;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "An unbound texture was accepted\n";
        return 6;
    }
    request.rectangle_texture = 27;
    request.max_rectangle_size = 2048;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "An oversized eye texture was accepted\n";
        return 7;
    }
    request.max_rectangle_size = 16384;
    request.native_width = 3400;
    request.native_height = 3468;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "An already matched native target was accepted\n";
        return 8;
    }
    request.native_width = 2560;
    request.native_height = 1440;
    request.inside_eye = false;
    if (ShouldCaptureRefractionEye(request)) {
        std::cerr << "A non-eye render was accepted\n";
        return 9;
    }
    std::cout << "Requiem refraction copy gate passed\n";
    return 0;
}
