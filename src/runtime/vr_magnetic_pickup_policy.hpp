#pragma once

#include <array>
#include <cstddef>

namespace penumbra_vr::runtime::vr_magnetic_pickup_policy {

// Pure targeting policy extracted from Overture Rework 23c890f. Backends own
// entity classification, world enumeration and visibility raycasts; this file
// owns only the demonstrated item profiles and geometric ranking math.
inline constexpr float kMaximumRange = 2.35F;
inline constexpr std::size_t kRankedCandidateCount = 5;
inline constexpr float kSearchPadding = 0.50F;
inline constexpr float kMinimumForwardDistance = 0.08F;
inline constexpr float kMaximumBodyAllowance = 0.12F;
inline constexpr float kConeBaseRadius = 0.07F;
inline constexpr float kConeGrowthPerMetre = 0.12F;
inline constexpr float kForwardScoreWeight = 0.02F;
inline constexpr float kVisibleSampleWeight = 0.90F;
inline constexpr float kSightOvershoot = 0.03F;

enum class VrMagneticPickupClass {
    consumable,
    ordinary,
    equipment,
    unsupported,
};

struct VrMagneticPickupProfile {
    float range = 0.0F;
    float priority_bias = 0.0F;
    bool eligible = false;
};

[[nodiscard]] constexpr VrMagneticPickupProfile Profile(
    VrMagneticPickupClass pickup_class) noexcept {
    switch (pickup_class) {
    case VrMagneticPickupClass::consumable:
        return {kMaximumRange, -0.20F, true};
    case VrMagneticPickupClass::ordinary:
        return {1.90F, -0.10F, true};
    case VrMagneticPickupClass::equipment:
        return {1.45F, 0.0F, true};
    default:
        return {};
    }
}

[[nodiscard]] constexpr bool ForwardDistanceEligible(
    float forward,
    float range) noexcept {
    return forward > kMinimumForwardDistance && forward <= range;
}

[[nodiscard]] constexpr float BodyAllowance(float body_radius) noexcept {
    if (body_radius <= 0.0F) return 0.0F;
    return body_radius > kMaximumBodyAllowance ? kMaximumBodyAllowance : body_radius;
}

[[nodiscard]] constexpr float ConeRadius(
    float forward,
    float body_radius) noexcept {
    return kConeBaseRadius + forward * kConeGrowthPerMetre + BodyAllowance(body_radius);
}

[[nodiscard]] constexpr bool InsideAimCone(
    float perpendicular_distance_squared,
    float cone_radius) noexcept {
    return cone_radius > 0.0F &&
        perpendicular_distance_squared <= cone_radius * cone_radius;
}

[[nodiscard]] constexpr float CandidateScore(
    float perpendicular_distance_squared,
    float cone_radius,
    float forward,
    float priority_bias) noexcept {
    return perpendicular_distance_squared / (cone_radius * cone_radius) +
        forward * kForwardScoreWeight + priority_bias;
}

[[nodiscard]] constexpr float Clamp(float value, float minimum, float maximum) noexcept {
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

[[nodiscard]] constexpr std::array<float, 3> VisibleSample(
    const std::array<float, 3>& aim_point,
    const std::array<float, 3>& bounds_minimum,
    const std::array<float, 3>& bounds_maximum,
    const std::array<float, 3>& centre) noexcept {
    std::array<float, 3> result{};
    for (std::size_t axis = 0; axis < result.size(); ++axis) {
        const float closest = Clamp(
            aim_point[axis], bounds_minimum[axis], bounds_maximum[axis]);
        result[axis] = closest * kVisibleSampleWeight +
            centre[axis] * (1.0F - kVisibleSampleWeight);
    }
    return result;
}

} // namespace penumbra_vr::runtime::vr_magnetic_pickup_policy
