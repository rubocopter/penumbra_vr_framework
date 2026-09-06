#include "opengl_eye_scissor.hpp"

#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using penumbra_vr::hooks::MapEyeScissor;
    using penumbra_vr::hooks::ScissorRect;
    ScissorRect result{};
    const ScissorRect eye{0, 0, 3400, 3468};
    if (!MapEyeScissor({0, -1, 2560, 1440}, {2560, 1440}, eye, result) ||
        result != eye ||
        !MapEyeScissor({640, 360, 320, 180}, {1280, 720}, {0, 0, 320, 240}, result) ||
        result != ScissorRect{159, 119, 82, 62}) {
        std::cerr << "Full-screen/legacy Y or asymmetric scale regression\n";
        return 1;
    }
    if (!MapEyeScissor({30, 40, 0, 0}, {100, 100}, eye, result) ||
        result[2] != 0 || result[3] != 0 ||
        !MapEyeScissor({-1000, -1000, 10, 10}, {100, 100}, eye, result) ||
        result[2] != 0 || result[3] != 0 ||
        !MapEyeScissor({1000, 1000, 10, 10}, {100, 100}, eye, result) ||
        result[2] != 0 || result[3] != 0) {
        std::cerr << "Empty/offscreen rectangles acquired visible area\n";
        return 2;
    }
    const ScissorRect sentinel{1, 2, 3, 4};
    result = sentinel;
    constexpr auto max = std::numeric_limits<std::int32_t>::max();
    if (MapEyeScissor({0, 0, -1, 10}, {100, 100}, eye, result) ||
        MapEyeScissor({0, 0, 10, 10}, {0, 100}, eye, result) ||
        MapEyeScissor({0, 0, 10, 10}, {100, 100}, {0, 0, 0, 10}, result) ||
        MapEyeScissor({0, 0, 10, 10}, {100, 100}, {max, 0, 10, 10}, result) ||
        result != sentinel) {
        std::cerr << "Invalid dimensions were accepted or modified output\n";
        return 3;
    }
    if (!MapEyeScissor({max - 4, max - 4, max, max}, {max, max},
            {0, 0, max, max}, result) || result[2] != 5 || result[3] != 5) {
        std::cerr << "Integer boundary handling failed\n";
        return 4;
    }
    // Vary head-projected bounds across and beyond all four edges, at sizes
    // both above and below desktop resolution, including an offset viewport.
    for (const ScissorRect viewport : {eye, ScissorRect{7, 9, 512, 512},
            ScissorRect{0, 0, 1, 1}, ScissorRect{0, 0, 2560, 1440}}) {
        for (int x = -300; x < 2800; x += 31) {
            for (int y = -200; y < 1650; y += 37) {
                if (!MapEyeScissor({x, y, 257, 193}, {2560, 1440}, viewport, result)) {
                    return 5;
                }
                for (std::size_t axis = 0; axis < 2; ++axis) {
                    const double start = static_cast<double>(axis == 0 ? x : y);
                    const double end = start + (axis == 0 ? 257 : 193);
                    const double size = axis == 0 ? 2560.0 : 1440.0;
                    if (result[axis] < viewport[axis] || result[axis + 2] < 0 ||
                        result[axis] + result[axis + 2] > viewport[axis] + viewport[axis + 2]) {
                        return 6;
                    }
                    if (start >= 0.0 && end <= size &&
                        (result[axis] > viewport[axis] + start * viewport[axis + 2] / size ||
                         result[axis] + result[axis + 2] <
                            viewport[axis] + end * viewport[axis + 2] / size)) {
                        std::cerr << "Conservative coverage was lost\n";
                        return 7;
                    }
                }
            }
        }
    }
    std::cout << "Eye scissor bounds, rounding, scaling and overflow checks passed\n";
    return 0;
}
