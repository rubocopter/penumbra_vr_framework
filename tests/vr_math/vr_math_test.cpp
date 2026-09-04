#include "vr_math.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

using penumbra_vr::runtime::VrEyeConfiguration;
using penumbra_vr::runtime::VrMatrix34;
using penumbra_vr::runtime::VrMatrix44;

[[nodiscard]] bool ApproximatelyEqual(
    float actual,
    float expected,
    float tolerance = 1.0e-5F) {
    return std::fabs(actual - expected) <= tolerance;
}

[[nodiscard]] bool ExpectMatrixValue(
    const VrMatrix44& matrix,
    std::size_t row,
    std::size_t column,
    float expected,
    const char* label) {
    const float actual = matrix.values[row * 4U + column];
    if (ApproximatelyEqual(actual, expected)) {
        return true;
    }
    std::cerr << label << " expected " << expected << " but got " << actual
              << '\n';
    return false;
}

[[nodiscard]] VrMatrix34 Translation(float x, float y, float z) {
    VrMatrix34 matrix;
    matrix.values = {
        1.0F, 0.0F, 0.0F, x,
        0.0F, 1.0F, 0.0F, y,
        0.0F, 0.0F, 1.0F, z,
    };
    return matrix;
}

[[nodiscard]] bool TestRigidInverse() {
    VrMatrix34 transform;
    transform.values = {
         0.0F, 0.0F, 1.0F, 1.0F,
         0.0F, 1.0F, 0.0F, 2.0F,
        -1.0F, 0.0F, 0.0F, 3.0F,
    };

    VrMatrix44 inverse;
    std::string error;
    if (!penumbra_vr::runtime::InvertRigidTransform(
            transform, inverse, error)) {
        std::cerr << "Rigid inverse failed: " << error << '\n';
        return false;
    }

    const VrMatrix44 identity = penumbra_vr::runtime::Multiply(
        penumbra_vr::runtime::ExpandMatrix(transform), inverse);
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            const float expected = row == column ? 1.0F : 0.0F;
            if (!ExpectMatrixValue(
                    identity, row, column, expected, "Rigid inverse product")) {
                return false;
            }
        }
    }
    return ExpectMatrixValue(inverse, 0, 3, 3.0F, "Inverse X") &&
        ExpectMatrixValue(inverse, 1, 3, -2.0F, "Inverse Y") &&
        ExpectMatrixValue(inverse, 2, 3, -1.0F, "Inverse Z");
}

[[nodiscard]] bool TestProjection() {
    VrEyeConfiguration symmetric;
    symmetric.left_tangent = -1.0F;
    symmetric.right_tangent = 1.0F;
    symmetric.top_tangent = -1.0F;
    symmetric.bottom_tangent = 1.0F;

    VrMatrix44 projection;
    std::string error;
    if (!penumbra_vr::runtime::BuildHplInfiniteProjection(
            symmetric, 0.05F, projection, error)) {
        std::cerr << "Symmetric projection failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(projection, 0, 0, 1.0F, "Symmetric X scale") ||
        !ExpectMatrixValue(projection, 1, 1, 1.0F, "Symmetric Y scale") ||
        !ExpectMatrixValue(projection, 0, 2, 0.0F, "Symmetric X offset") ||
        !ExpectMatrixValue(projection, 1, 2, 0.0F, "Symmetric Y offset") ||
        !ExpectMatrixValue(projection, 2, 2, -1.0F, "Infinite Z scale") ||
        !ExpectMatrixValue(projection, 2, 3, -0.1F, "Near clip") ||
        !ExpectMatrixValue(projection, 3, 2, -1.0F, "Perspective divide")) {
        return false;
    }

    VrEyeConfiguration left_eye;
    left_eye.left_tangent = -1.84177F;
    left_eye.right_tangent = 0.947191F;
    left_eye.top_tangent = -1.32898F;
    left_eye.bottom_tangent = 1.32898F;
    if (!penumbra_vr::runtime::BuildHplInfiniteProjection(
            left_eye, 0.05F, projection, error)) {
        std::cerr << "Recorded left-eye projection failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(projection, 0, 0, 0.717111F, "Left-eye X scale") ||
        !ExpectMatrixValue(projection, 0, 2, -0.320756F, "Left-eye X offset") ||
        !ExpectMatrixValue(projection, 1, 1, 0.752457F, "Left-eye Y scale")) {
        return false;
    }

    VrEyeConfiguration right_eye = left_eye;
    right_eye.left_tangent = -0.947191F;
    right_eye.right_tangent = 1.84177F;
    if (!penumbra_vr::runtime::BuildHplInfiniteProjection(
            right_eye, 0.05F, projection, error)) {
        std::cerr << "Recorded right-eye projection failed: " << error << '\n';
        return false;
    }
    return ExpectMatrixValue(
        projection, 0, 2, 0.320756F, "Right-eye X offset");
}

[[nodiscard]] bool TestEyeViews() {
    const VrMatrix44 head_view = penumbra_vr::runtime::IdentityMatrix();
    VrMatrix44 eye_view;
    std::string error;
    if (!penumbra_vr::runtime::ComposeEyeViewFromHeadView(
            head_view, Translation(-0.032F, 0.0F, 0.0F), eye_view, error)) {
        std::cerr << "Left-eye view failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(eye_view, 0, 3, 0.032F, "Left head-to-eye X")) {
        return false;
    }

    if (!penumbra_vr::runtime::ComposeEyeViewFromHeadView(
            head_view, Translation(0.032F, 0.0F, 0.0F), eye_view, error)) {
        std::cerr << "Right-eye view failed: " << error << '\n';
        return false;
    }
    return ExpectMatrixValue(eye_view, 0, 3, -0.032F, "Right head-to-eye X");
}

[[nodiscard]] bool TestInvalidInputs() {
    VrMatrix34 scaled = Translation(0.0F, 0.0F, 0.0F);
    scaled.values[0] = 2.0F;
    VrMatrix44 result;
    std::string error;
    if (penumbra_vr::runtime::InvertRigidTransform(scaled, result, error) ||
        error.empty()) {
        std::cerr << "A scaled transform was accepted as rigid\n";
        return false;
    }

    VrEyeConfiguration invalid_projection;
    invalid_projection.left_tangent = -1.0F;
    invalid_projection.right_tangent = -1.0F;
    invalid_projection.top_tangent = -1.0F;
    invalid_projection.bottom_tangent = 1.0F;
    if (penumbra_vr::runtime::BuildHplInfiniteProjection(
            invalid_projection, 0.05F, result, error) || error.empty()) {
        std::cerr << "A zero-width frustum was accepted\n";
        return false;
    }

    invalid_projection.right_tangent =
        std::numeric_limits<float>::quiet_NaN();
    if (penumbra_vr::runtime::BuildHplInfiniteProjection(
            invalid_projection, 0.05F, result, error) || error.empty()) {
        std::cerr << "A non-finite frustum was accepted\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!TestRigidInverse() || !TestProjection() || !TestEyeViews() ||
        !TestInvalidInputs()) {
        return 1;
    }
    std::cout << "VR matrix tests passed\n";
    return 0;
}
