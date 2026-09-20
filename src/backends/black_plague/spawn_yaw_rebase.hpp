#pragma once

#include <cmath>

namespace penumbra_vr::backends::black_plague {

struct SpawnYawRebase {
    bool valid = false;
    float native_delta = 0.0F;
    float world_yaw = 0.0F;
};

[[nodiscard]] inline float WrapSpawnYaw(float radians) noexcept {
    constexpr float kPi = 3.14159265358979323846F;
    constexpr float kTau = 2.0F * kPi;
    float wrapped = std::fmod(radians + kPi, kTau);
    if (wrapped < 0.0F) wrapped += kTau;
    return wrapped - kPi;
}

[[nodiscard]] inline SpawnYawRebase ComputeSpawnYawRebase(
    float current_world_yaw,
    float old_native_yaw,
    float new_native_yaw) noexcept {
    if (!std::isfinite(current_world_yaw) || !std::isfinite(old_native_yaw) ||
        !std::isfinite(new_native_yaw)) {
        return {};
    }
    const float native_delta = WrapSpawnYaw(new_native_yaw - old_native_yaw);
    return {
        true,
        native_delta,
        WrapSpawnYaw(current_world_yaw - native_delta),
    };
}

} // namespace penumbra_vr::backends::black_plague
