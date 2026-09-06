#include "vr_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace penumbra_vr::runtime {
namespace {

constexpr float kMinimumSpan = 1.0e-6F;
constexpr float kRigidTolerance = 2.0e-3F;
constexpr float kMinimumHorizontalForwardLength = 0.1F;
constexpr float kMaximumCullHalfAngle = 1.483529864F; // 85 degrees.

[[nodiscard]] constexpr std::size_t Index(
    std::size_t row,
    std::size_t column) noexcept {
    return row * 4U + column;
}

[[nodiscard]] bool IsFinite(const VrMatrix34& matrix) noexcept {
    for (const float value : matrix.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsFinite(const VrMatrix44& matrix) noexcept {
    for (const float value : matrix.values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] VrMatrix34 CollapseRigidMatrix(const VrMatrix44& matrix) noexcept {
    VrMatrix34 collapsed;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            collapsed.values[row * 4U + column] =
                matrix.values[Index(row, column)];
        }
    }
    return collapsed;
}

[[nodiscard]] float DotRotationRows(
    const VrMatrix34& matrix,
    std::size_t first_row,
    std::size_t second_row) noexcept {
    float result = 0.0F;
    for (std::size_t column = 0; column < 3; ++column) {
        result += matrix.values[first_row * 4U + column] *
            matrix.values[second_row * 4U + column];
    }
    return result;
}

[[nodiscard]] bool HasRigidRotation(const VrMatrix34& matrix) noexcept {
    for (std::size_t row = 0; row < 3; ++row) {
        if (std::fabs(DotRotationRows(matrix, row, row) - 1.0F) >
            kRigidTolerance) {
            return false;
        }
    }

    for (std::size_t first = 0; first < 3; ++first) {
        for (std::size_t second = first + 1; second < 3; ++second) {
            if (std::fabs(DotRotationRows(matrix, first, second)) >
                kRigidTolerance) {
                return false;
            }
        }
    }

    const float determinant =
        matrix.values[0] *
            (matrix.values[5] * matrix.values[10] -
             matrix.values[6] * matrix.values[9]) -
        matrix.values[1] *
            (matrix.values[4] * matrix.values[10] -
             matrix.values[6] * matrix.values[8]) +
        matrix.values[2] *
            (matrix.values[4] * matrix.values[9] -
             matrix.values[5] * matrix.values[8]);
    return std::fabs(determinant - 1.0F) <= kRigidTolerance;
}

} // namespace

VrMatrix44 IdentityMatrix() noexcept {
    VrMatrix44 matrix;
    matrix.values[0] = 1.0F;
    matrix.values[5] = 1.0F;
    matrix.values[10] = 1.0F;
    matrix.values[15] = 1.0F;
    return matrix;
}

VrMatrix44 ExpandMatrix(const VrMatrix34& matrix) noexcept {
    VrMatrix44 expanded = IdentityMatrix();
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            expanded.values[Index(row, column)] =
                matrix.values[row * 4U + column];
        }
    }
    return expanded;
}

VrMatrix44 Multiply(
    const VrMatrix44& left,
    const VrMatrix44& right) noexcept {
    VrMatrix44 product;
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            float value = 0.0F;
            for (std::size_t component = 0; component < 4; ++component) {
                value += left.values[Index(row, component)] *
                    right.values[Index(component, column)];
            }
            product.values[Index(row, column)] = value;
        }
    }
    return product;
}

bool InvertRigidTransform(
    const VrMatrix34& transform,
    VrMatrix44& inverse,
    std::string& error) noexcept {
    error.clear();
    inverse = {};
    if (!IsFinite(transform)) {
        error = "The rigid transform contains a non-finite value";
        return false;
    }
    if (!HasRigidRotation(transform)) {
        error = "The transform rotation is not a right-handed orthonormal basis";
        return false;
    }

    inverse = IdentityMatrix();
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            inverse.values[Index(row, column)] =
                transform.values[column * 4U + row];
        }
    }

    for (std::size_t row = 0; row < 3; ++row) {
        float translated = 0.0F;
        for (std::size_t component = 0; component < 3; ++component) {
            translated += inverse.values[Index(row, component)] *
                transform.values[component * 4U + 3U];
        }
        inverse.values[Index(row, 3)] = -translated;
    }
    return true;
}

bool ComposeEyeViewFromHeadView(
    const VrMatrix44& head_view,
    const VrMatrix34& eye_to_head,
    VrMatrix44& eye_view,
    std::string& error) noexcept {
    VrMatrix44 head_to_eye;
    if (!InvertRigidTransform(eye_to_head, head_to_eye, error)) {
        eye_view = {};
        return false;
    }
    if (!IsFinite(head_view)) {
        eye_view = {};
        error = "The HPL head-view matrix contains a non-finite value";
        return false;
    }

    eye_view = Multiply(head_to_eye, head_view);
    return true;
}

bool ComposeRelativeTrackedHeadView(
    const VrMatrix44& game_head_view,
    const VrMatrix34& anchor_device_to_absolute,
    const VrMatrix34& current_device_to_absolute,
    float world_units_per_meter,
    VrMatrix44& tracked_head_view,
    std::string& error) noexcept {
    error.clear();
    tracked_head_view = {};
    if (!IsFinite(game_head_view)) {
        error = "The HPL head-view matrix contains a non-finite value";
        return false;
    }
    if (!std::isfinite(world_units_per_meter) || world_units_per_meter < 0.0F) {
        error = "The tracking translation scale must be finite and non-negative";
        return false;
    }

    VrMatrix44 absolute_to_anchor;
    if (!InvertRigidTransform(
            anchor_device_to_absolute, absolute_to_anchor, error)) {
        error = "The tracking anchor is invalid: " + error;
        return false;
    }

    const VrMatrix44 current_to_absolute =
        ExpandMatrix(current_device_to_absolute);
    VrMatrix44 current_to_anchor =
        Multiply(absolute_to_anchor, current_to_absolute);
    current_to_anchor.values[Index(0, 3)] *= world_units_per_meter;
    current_to_anchor.values[Index(1, 3)] *= world_units_per_meter;
    current_to_anchor.values[Index(2, 3)] *= world_units_per_meter;

    VrMatrix44 relative_tracking_view;
    if (!InvertRigidTransform(
            CollapseRigidMatrix(current_to_anchor),
            relative_tracking_view,
            error)) {
        error = "The relative HMD pose is invalid: " + error;
        return false;
    }

    tracked_head_view = Multiply(relative_tracking_view, game_head_view);
    return true;
}

bool ComposeYawRecenteredTrackedHeadView(
    const VrMatrix44& game_head_view,
    const VrMatrix34& anchor_device_to_absolute,
    const VrMatrix34& current_device_to_absolute,
    float world_units_per_meter,
    VrMatrix44& tracked_head_view,
    std::string& error) noexcept {
    error.clear();
    tracked_head_view = {};
    if (!IsFinite(game_head_view)) {
        error = "The HPL head-view matrix contains a non-finite value";
        return false;
    }
    if (!std::isfinite(world_units_per_meter) || world_units_per_meter < 0.0F) {
        error = "The tracking translation scale must be finite and non-negative";
        return false;
    }

    VrMatrix44 game_head_pose;
    if (!InvertRigidTransform(
            CollapseRigidMatrix(game_head_view), game_head_pose, error)) {
        error = "The HPL head-view matrix is not rigid: " + error;
        return false;
    }

    VrMatrix44 ignored_inverse;
    if (!InvertRigidTransform(
            anchor_device_to_absolute, ignored_inverse, error)) {
        error = "The tracking anchor is invalid: " + error;
        return false;
    }
    if (!InvertRigidTransform(
            current_device_to_absolute, ignored_inverse, error)) {
        error = "The current HMD pose is invalid: " + error;
        return false;
    }

    float anchor_forward_x = -anchor_device_to_absolute.values[2];
    float anchor_forward_z = -anchor_device_to_absolute.values[10];
    float game_forward_x = -game_head_pose.values[Index(0, 2)];
    float game_forward_z = -game_head_pose.values[Index(2, 2)];
    const float anchor_length = std::sqrt(
        anchor_forward_x * anchor_forward_x +
        anchor_forward_z * anchor_forward_z);
    const float game_length = std::sqrt(
        game_forward_x * game_forward_x + game_forward_z * game_forward_z);
    if (anchor_length < kMinimumHorizontalForwardLength) {
        error = "The HMD is too close to vertical to establish a yaw centre";
        return false;
    }
    if (game_length < kMinimumHorizontalForwardLength) {
        error = "The game camera is too close to vertical to establish a yaw centre";
        return false;
    }

    anchor_forward_x /= anchor_length;
    anchor_forward_z /= anchor_length;
    game_forward_x /= game_length;
    game_forward_z /= game_length;
    const float yaw_cosine =
        anchor_forward_x * game_forward_x +
        anchor_forward_z * game_forward_z;
    const float yaw_sine =
        anchor_forward_z * game_forward_x -
        anchor_forward_x * game_forward_z;

    VrMatrix44 tracking_to_game_yaw = IdentityMatrix();
    tracking_to_game_yaw.values[Index(0, 0)] = yaw_cosine;
    tracking_to_game_yaw.values[Index(0, 2)] = yaw_sine;
    tracking_to_game_yaw.values[Index(2, 0)] = -yaw_sine;
    tracking_to_game_yaw.values[Index(2, 2)] = yaw_cosine;

    const VrMatrix44 mapped_current_pose = Multiply(
        tracking_to_game_yaw, ExpandMatrix(current_device_to_absolute));
    VrMatrix44 tracked_head_pose = game_head_pose;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            tracked_head_pose.values[Index(row, column)] =
                mapped_current_pose.values[Index(row, column)];
        }
    }

    const std::array<float, 3> tracking_delta{
        current_device_to_absolute.values[3] -
            anchor_device_to_absolute.values[3],
        current_device_to_absolute.values[7] -
            anchor_device_to_absolute.values[7],
        current_device_to_absolute.values[11] -
            anchor_device_to_absolute.values[11],
    };
    for (std::size_t row = 0; row < 3; ++row) {
        float mapped_delta = 0.0F;
        for (std::size_t component = 0; component < 3; ++component) {
            mapped_delta +=
                tracking_to_game_yaw.values[Index(row, component)] *
                tracking_delta[component];
        }
        tracked_head_pose.values[Index(row, 3)] =
            game_head_pose.values[Index(row, 3)] +
            mapped_delta * world_units_per_meter;
    }

    if (!InvertRigidTransform(
            CollapseRigidMatrix(tracked_head_pose), tracked_head_view, error)) {
        error = "The yaw-recentered HMD pose is invalid: " + error;
        return false;
    }
    return true;
}

bool BuildHplInfiniteProjection(
    const VrEyeConfiguration& eye,
    float near_clip,
    VrMatrix44& projection,
    std::string& error) noexcept {
    error.clear();
    projection = {};
    if (!std::isfinite(eye.left_tangent) ||
        !std::isfinite(eye.right_tangent) ||
        !std::isfinite(eye.top_tangent) ||
        !std::isfinite(eye.bottom_tangent) ||
        !std::isfinite(near_clip)) {
        error = "The projection inputs contain a non-finite value";
        return false;
    }
    if (near_clip <= 0.0F) {
        error = "The near clip distance must be positive";
        return false;
    }

    const float horizontal_span = eye.right_tangent - eye.left_tangent;
    const float vertical_span = eye.bottom_tangent - eye.top_tangent;
    if (horizontal_span <= kMinimumSpan || vertical_span <= kMinimumSpan) {
        error = "The OpenVR projection tangents do not form a positive frustum";
        return false;
    }

    projection.values[0] = 2.0F / horizontal_span;
    projection.values[2] =
        (eye.right_tangent + eye.left_tangent) / horizontal_span;
    projection.values[5] = 2.0F / vertical_span;
    projection.values[6] =
        (eye.bottom_tangent + eye.top_tangent) / vertical_span;
    projection.values[10] = -1.0F;
    projection.values[11] = -2.0F * near_clip;
    projection.values[14] = -1.0F;
    return true;
}

bool BuildConservativeStereoCullFrustum(
    const std::array<VrEyeConfiguration, 2>& eyes,
    float angular_guard_radians,
    VrCullFrustum& frustum,
    std::string& error) noexcept {
    error.clear();
    frustum = {};
    if (!std::isfinite(angular_guard_radians) || angular_guard_radians < 0.0F) {
        error = "The stereo cull angular guard must be finite and non-negative";
        return false;
    }

    float horizontal_tangent = 0.0F;
    float vertical_tangent = 0.0F;
    for (const VrEyeConfiguration& eye : eyes) {
        if (!std::isfinite(eye.left_tangent) ||
            !std::isfinite(eye.right_tangent) ||
            !std::isfinite(eye.top_tangent) ||
            !std::isfinite(eye.bottom_tangent)) {
            error = "A stereo cull projection tangent is non-finite";
            return false;
        }
        if (eye.right_tangent - eye.left_tangent <= kMinimumSpan ||
            eye.bottom_tangent - eye.top_tangent <= kMinimumSpan) {
            error = "A stereo cull eye does not form a positive frustum";
            return false;
        }
        horizontal_tangent = std::max(
            horizontal_tangent,
            std::max(std::fabs(eye.left_tangent), std::fabs(eye.right_tangent)));
        vertical_tangent = std::max(
            vertical_tangent,
            std::max(std::fabs(eye.top_tangent), std::fabs(eye.bottom_tangent)));
    }
    if (horizontal_tangent <= kMinimumSpan || vertical_tangent <= kMinimumSpan) {
        error = "The stereo cull frustum has no positive angular extent";
        return false;
    }

    const float horizontal_half_angle =
        std::atan(horizontal_tangent) + angular_guard_radians;
    const float vertical_half_angle =
        std::atan(vertical_tangent) + angular_guard_radians;
    if (horizontal_half_angle >= kMaximumCullHalfAngle ||
        vertical_half_angle >= kMaximumCullHalfAngle) {
        error = "The guarded stereo cull frustum is too wide";
        return false;
    }

    const float guarded_horizontal_tangent = std::tan(horizontal_half_angle);
    const float guarded_vertical_tangent = std::tan(vertical_half_angle);
    frustum.vertical_fov_radians = vertical_half_angle * 2.0F;
    frustum.aspect = guarded_horizontal_tangent / guarded_vertical_tangent;
    if (!std::isfinite(frustum.vertical_fov_radians) ||
        !std::isfinite(frustum.aspect) || frustum.aspect <= 0.0F) {
        frustum = {};
        error = "The guarded stereo cull frustum is invalid";
        return false;
    }
    return true;
}

bool ProjectAimOnMenu(const VrMatrix34& anchor, const VrMatrix34& aim,
                      float aspect, float distance, float width,
                      std::array<float, 2>& uv) noexcept {
    uv = {};
    if (!std::isfinite(aspect) || aspect <= 0 ||
        !std::isfinite(distance) || distance <= 0 ||
        !std::isfinite(width) || width <= 0) return false;
    VrMatrix44 view, pose;
    std::string error;
    if (!ComposeYawRecenteredTrackedHeadView(IdentityMatrix(), anchor, aim, 1, view, error) ||
        !InvertRigidTransform(CollapseRigidMatrix(view), pose, error)) return false;
    const auto& m = pose.values;
    const float dz = -m[10];
    if (dz >= -0.0001F) return false;
    const float t = (-distance - m[11]) / dz;
    if (t < 0 || t > 20) return false;
    const float x = m[3] - t * m[2], y = m[7] - t * m[6];
    const std::array<float, 2> candidate{x / width + 0.5F, 0.5F - y * aspect / width};
    if (candidate[0] < 0 || candidate[0] > 1 || candidate[1] < 0 || candidate[1] > 1) return false;
    uv = candidate; return true;
}

} // namespace penumbra_vr::runtime
