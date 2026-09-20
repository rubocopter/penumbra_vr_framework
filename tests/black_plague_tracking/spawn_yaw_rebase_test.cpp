#include "spawn_yaw_rebase.hpp"

#include <cmath>
#include <iostream>

namespace {

constexpr float kPi = 3.14159265358979323846F;

bool Near(float left, float right, float epsilon = 0.00001F) {
    return std::abs(left - right) < epsilon;
}

float Degrees(float value) {
    return value * kPi / 180.0F;
}

} // namespace

int main() {
    using penumbra_vr::backends::black_plague::ComputeSpawnYawRebase;

    struct Case {
        float world_yaw;
        float old_native;
        float new_native;
        float expected_delta;
        float expected_world;
    };
    const Case cases[] = {
        {Degrees(20.0F), Degrees(10.0F), Degrees(80.0F),
            Degrees(70.0F), Degrees(-50.0F)},
        {Degrees(35.0F), Degrees(120.0F), Degrees(10.0F),
            Degrees(-110.0F), Degrees(145.0F)},
        {Degrees(15.0F), Degrees(0.0F), kPi,
            -kPi, Degrees(-165.0F)},
        {Degrees(25.0F), Degrees(170.0F), Degrees(-170.0F),
            Degrees(20.0F), Degrees(5.0F)},
        {Degrees(-40.0F), Degrees(-175.0F), Degrees(175.0F),
            Degrees(-10.0F), Degrees(-30.0F)},
    };

    for (const auto& test : cases) {
        const auto result = ComputeSpawnYawRebase(
            test.world_yaw, test.old_native, test.new_native);
        if (!result.valid || !Near(result.native_delta, test.expected_delta) ||
            !Near(result.world_yaw, test.expected_world)) {
            std::cerr << "spawn yaw rebase mismatch\n";
            return 1;
        }
    }

    const auto invalid = ComputeSpawnYawRebase(
        0.0F, 0.0F, INFINITY);
    if (invalid.valid) {
        std::cerr << "non-finite spawn yaw was accepted\n";
        return 2;
    }
    return 0;
}
