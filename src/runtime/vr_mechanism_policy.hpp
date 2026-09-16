#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace penumbra_vr::runtime::vr_mechanism_policy {

using Vec3 = std::array<float, 3>;

// Proven Overture Rework 23c890f mechanism servo constants. Native joint
// discovery, limits, controller pausing and velocity application stay in each
// game backend/product.
inline constexpr float kServoGain = 10.0F;
inline constexpr float kMaximumServoSpeed = 3.5F;
inline constexpr float kMinimumHingeRadius = 0.03F;
inline constexpr float kFallbackHingeRadius = 0.30F;
inline constexpr float kJointedMaximumLinearSpeed = 5.0F;
inline constexpr float kFreeMoveMaximumLinearSpeed = 10.0F;
inline constexpr float kJointedMaximumAngularSpeed = 8.0F;
inline constexpr float kFreeMoveMaximumAngularSpeed = 15.0F;

struct VrMechanismMotionPlan {
    Vec3 linear_velocity{};
    Vec3 angular_velocity{};
    float hinge_radius = 0.0F;
    bool valid = false;
};

[[nodiscard]] inline float Dot(const Vec3& left, const Vec3& right) noexcept {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

[[nodiscard]] inline Vec3 Add(const Vec3& left, const Vec3& right) noexcept {
    return {left[0] + right[0], left[1] + right[1], left[2] + right[2]};
}

[[nodiscard]] inline Vec3 Subtract(const Vec3& left, const Vec3& right) noexcept {
    return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
}

[[nodiscard]] inline Vec3 Scale(const Vec3& value, float scale) noexcept {
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

[[nodiscard]] inline Vec3 Cross(const Vec3& left, const Vec3& right) noexcept {
    return {
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0]};
}

[[nodiscard]] inline float Length(const Vec3& value) noexcept {
    return std::sqrt(Dot(value, value));
}

[[nodiscard]] inline bool Finite(const Vec3& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) &&
        std::isfinite(value[2]);
}

[[nodiscard]] inline Vec3 Normalize(const Vec3& value) noexcept {
    const float length = Length(value);
    if (!std::isfinite(length) || length <= 1.0e-6F) return {};
    return Scale(value, 1.0F / length);
}

[[nodiscard]] inline Vec3 ClampLength(Vec3 value, float maximum) noexcept {
    const float length = Length(value);
    if (length > maximum && length > 0.0F) {
        value = Scale(value, maximum / length);
    }
    return value;
}

[[nodiscard]] inline VrMechanismMotionPlan PlanUnconstrainedJointDrag(
    const Vec3& desired_delta) noexcept {
    VrMechanismMotionPlan result;
    if (!Finite(desired_delta)) return result;
    result.linear_velocity = ClampLength(Scale(desired_delta, kServoGain), kMaximumServoSpeed);
    result.valid = true;
    return result;
}

[[nodiscard]] inline VrMechanismMotionPlan PlanSlider(
    const Vec3& desired_delta,
    const Vec3& pin_direction) noexcept {
    VrMechanismMotionPlan result;
    if (!Finite(desired_delta) || !Finite(pin_direction)) return result;
    const Vec3 pin = Normalize(pin_direction);
    if (Length(pin) <= 0.0F) return result;
    const Vec3 drag_velocity = Scale(desired_delta, kServoGain);
    result.linear_velocity = ClampLength(
        Scale(pin, Dot(drag_velocity, pin)), kMaximumServoSpeed);
    result.valid = true;
    return result;
}

[[nodiscard]] inline VrMechanismMotionPlan PlanHinge(
    const Vec3& desired_delta,
    const Vec3& pin_direction,
    const Vec3& pivot,
    const Vec3& picked_point,
    const Vec3& body_position,
    float lightness) noexcept {
    VrMechanismMotionPlan result;
    if (!Finite(desired_delta) || !Finite(pin_direction) || !Finite(pivot) ||
        !Finite(picked_point) || !Finite(body_position) ||
        !std::isfinite(lightness) || lightness <= 0.0F) {
        return result;
    }

    const Vec3 pin = Normalize(pin_direction);
    if (Length(pin) <= 0.0F) return result;

    auto radial = Subtract(picked_point, pivot);
    radial = Subtract(radial, Scale(pin, Dot(radial, pin)));
    float radius = Length(radial);
    if (radius < kMinimumHingeRadius) {
        radial = Subtract(body_position, pivot);
        radial = Subtract(radial, Scale(pin, Dot(radial, pin)));
        radius = Length(radial);
    }
    if (radius < kMinimumHingeRadius) radius = kFallbackHingeRadius;

    const Vec3 tangent = Normalize(Cross(pin, radial));
    const Vec3 drag_velocity = Scale(desired_delta, kServoGain);
    const float tangent_speed = Dot(drag_velocity, tangent);
    float allowed_speed = std::fabs(tangent_speed) * lightness;
    const float maximum_speed = kMaximumServoSpeed * lightness;
    if (allowed_speed > maximum_speed) allowed_speed = maximum_speed;
    const float angular_speed = allowed_speed / radius;
    result.angular_velocity = Scale(
        pin, angular_speed * (tangent_speed >= 0.0F ? 1.0F : -1.0F));

    auto body_radial = Subtract(body_position, pivot);
    body_radial = Subtract(body_radial, Scale(pin, Dot(body_radial, pin)));
    result.linear_velocity = Cross(result.angular_velocity, body_radial);
    result.hinge_radius = radius;
    result.valid = true;
    return result;
}

} // namespace penumbra_vr::runtime::vr_mechanism_policy
