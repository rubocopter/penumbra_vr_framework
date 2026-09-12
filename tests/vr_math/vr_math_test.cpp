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

[[nodiscard]] VrMatrix34 YawNinetyDegrees(float x, float y, float z) {
    VrMatrix34 matrix;
    matrix.values = {
         0.0F, 0.0F, 1.0F, x,
         0.0F, 1.0F, 0.0F, y,
        -1.0F, 0.0F, 0.0F, z,
    };
    return matrix;
}

[[nodiscard]] VrMatrix34 PitchDegrees(float degrees) {
    const float radians = degrees * 3.14159265358979323846F / 180.0F;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    VrMatrix34 matrix;
    matrix.values = {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, cosine, -sine, 0.0F,
        0.0F, sine, cosine, 0.0F,
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

[[nodiscard]] bool TestConservativeStereoCullFrustum() {
    std::array<VrEyeConfiguration, 2> eyes{};
    eyes[0].left_tangent = -1.84177F;
    eyes[0].right_tangent = 0.947191F;
    eyes[0].top_tangent = -1.32898F;
    eyes[0].bottom_tangent = 1.32898F;
    eyes[1] = eyes[0];
    eyes[1].left_tangent = -0.947191F;
    eyes[1].right_tangent = 1.84177F;

    penumbra_vr::runtime::VrCullFrustum frustum;
    std::string error;
    if (!penumbra_vr::runtime::BuildConservativeStereoCullFrustum(
            eyes, 0.0F, frustum, error)) {
        std::cerr << "Stereo cull frustum failed: " << error << '\n';
        return false;
    }
    const float expected_vertical_fov = 2.0F * std::atan(1.32898F);
    const float expected_aspect = 1.84177F / 1.32898F;
    if (!ApproximatelyEqual(frustum.vertical_fov_radians, expected_vertical_fov) ||
        !ApproximatelyEqual(frustum.aspect, expected_aspect)) {
        std::cerr << "Stereo cull frustum did not contain both asymmetric eyes\n";
        return false;
    }

    penumbra_vr::runtime::VrCullFrustum guarded;
    constexpr float kFiveDegrees = 0.087266463F;
    if (!penumbra_vr::runtime::BuildConservativeStereoCullFrustum(
            eyes, kFiveDegrees, guarded, error) ||
        guarded.vertical_fov_radians <= frustum.vertical_fov_radians) {
        std::cerr << "Stereo cull angular guard was not applied\n";
        return false;
    }

    eyes[1].right_tangent = std::numeric_limits<float>::quiet_NaN();
    if (penumbra_vr::runtime::BuildConservativeStereoCullFrustum(
            eyes, 0.0F, guarded, error) || error.empty()) {
        std::cerr << "A non-finite stereo cull frustum was accepted\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestRelativeHeadTracking() {
    const VrMatrix44 game_view = penumbra_vr::runtime::IdentityMatrix();
    const VrMatrix34 origin = Translation(0.0F, 0.0F, 0.0F);
    VrMatrix44 tracked_view;
    std::string error;

    if (!penumbra_vr::runtime::ComposeRelativeTrackedHeadView(
            game_view,
            origin,
            Translation(0.25F, -0.1F, 0.5F),
            1.0F,
            tracked_view,
            error)) {
        std::cerr << "Relative translation failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(tracked_view, 0, 3, -0.25F, "Tracked X") ||
        !ExpectMatrixValue(tracked_view, 1, 3, 0.1F, "Tracked Y") ||
        !ExpectMatrixValue(tracked_view, 2, 3, -0.5F, "Tracked Z")) {
        return false;
    }

    if (!penumbra_vr::runtime::ComposeRelativeTrackedHeadView(
            game_view,
            Translation(4.0F, 2.0F, -3.0F),
            Translation(4.25F, 1.9F, -2.5F),
            0.0F,
            tracked_view,
            error)) {
        std::cerr << "Rotation-only recenter failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(tracked_view, 0, 3, 0.0F, "Rotation-only X") ||
        !ExpectMatrixValue(tracked_view, 1, 3, 0.0F, "Rotation-only Y") ||
        !ExpectMatrixValue(tracked_view, 2, 3, 0.0F, "Rotation-only Z")) {
        return false;
    }

    if (!penumbra_vr::runtime::ComposeRelativeTrackedHeadView(
            game_view,
            origin,
            YawNinetyDegrees(0.0F, 0.0F, 0.0F),
            0.0F,
            tracked_view,
            error)) {
        std::cerr << "Relative yaw failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(tracked_view, 0, 0, 0.0F, "Yaw inverse 00") ||
        !ExpectMatrixValue(tracked_view, 0, 2, -1.0F, "Yaw inverse 02") ||
        !ExpectMatrixValue(tracked_view, 2, 0, 1.0F, "Yaw inverse 20") ||
        !ExpectMatrixValue(tracked_view, 2, 2, 0.0F, "Yaw inverse 22")) {
        return false;
    }

    const VrMatrix34 same_pose = YawNinetyDegrees(3.0F, 2.0F, 1.0F);
    if (!penumbra_vr::runtime::ComposeRelativeTrackedHeadView(
            game_view,
            same_pose,
            same_pose,
            1.0F,
            tracked_view,
            error)) {
        std::cerr << "Identical-pose recenter failed: " << error << '\n';
        return false;
    }
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            const float expected = row == column ? 1.0F : 0.0F;
            if (!ExpectMatrixValue(
                    tracked_view,
                    row,
                    column,
                    expected,
                    "Identical-pose tracked view")) {
                return false;
            }
        }
    }

    if (penumbra_vr::runtime::ComposeRelativeTrackedHeadView(
            game_view,
            origin,
            origin,
            -1.0F,
            tracked_view,
            error) || error.empty()) {
        std::cerr << "A negative tracking scale was accepted\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestYawRecenteredHeadTracking() {
    const VrMatrix44 game_view = penumbra_vr::runtime::IdentityMatrix();
    VrMatrix44 tracked_view;
    std::string error;

    const VrMatrix34 yawed_anchor =
        YawNinetyDegrees(3.0F, 1.5F, -2.0F);
    if (!penumbra_vr::runtime::ComposeYawRecenteredTrackedHeadView(
            game_view,
            yawed_anchor,
            yawed_anchor,
            0.0F,
            tracked_view,
            error)) {
        std::cerr << "Yaw-only recenter failed: " << error << '\n';
        return false;
    }
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            const float expected = row == column ? 1.0F : 0.0F;
            if (!ExpectMatrixValue(
                    tracked_view,
                    row,
                    column,
                    expected,
                    "Yaw-only centre aligns with game heading")) {
                return false;
            }
        }
    }

    const VrMatrix34 level_anchor = Translation(0.0F, 1.6F, 0.0F);
    const VrMatrix34 pitched_current = PitchDegrees(30.0F);
    if (!penumbra_vr::runtime::ComposeYawRecenteredTrackedHeadView(
            game_view,
            level_anchor,
            pitched_current,
            0.0F,
            tracked_view,
            error)) {
        std::cerr << "Tracked pitch preservation failed: " << error << '\n';
        return false;
    }
    const float cosine = std::cos(30.0F * 3.14159265358979323846F / 180.0F);
    const float sine = std::sin(30.0F * 3.14159265358979323846F / 180.0F);
    if (!ExpectMatrixValue(tracked_view, 1, 1, cosine, "Pitch view 11") ||
        !ExpectMatrixValue(tracked_view, 1, 2, sine, "Pitch view 12") ||
        !ExpectMatrixValue(tracked_view, 2, 1, -sine, "Pitch view 21") ||
        !ExpectMatrixValue(tracked_view, 2, 2, cosine, "Pitch view 22")) {
        return false;
    }

    if (penumbra_vr::runtime::ComposeYawRecenteredTrackedHeadView(
            game_view,
            PitchDegrees(90.0F),
            pitched_current,
            0.0F,
            tracked_view,
            error) || error.empty()) {
        std::cerr << "A vertical tracking anchor was accepted\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool TestWorldTranslationToView() {
    VrMatrix44 translated;
    std::string error;
    if (!penumbra_vr::runtime::ApplyWorldTranslationToView(
            penumbra_vr::runtime::IdentityMatrix(),
            {1.0F, 2.0F, -3.0F}, translated, error)) {
        std::cerr << "World translation failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(translated, 0, 3, -1.0F, "Translated view X") ||
        !ExpectMatrixValue(translated, 1, 3, -2.0F, "Translated view Y") ||
        !ExpectMatrixValue(translated, 2, 3, 3.0F, "Translated view Z")) {
        return false;
    }

    VrMatrix44 yawed_view;
    if (!penumbra_vr::runtime::InvertRigidTransform(
            YawNinetyDegrees(4.0F, 2.0F, -3.0F), yawed_view, error) ||
        !penumbra_vr::runtime::ApplyWorldTranslationToView(
            yawed_view, {1.0F, 0.0F, 0.0F}, translated, error)) {
        std::cerr << "Yawed world translation failed: " << error << '\n';
        return false;
    }
    if (!ExpectMatrixValue(translated, 0, 3, -3.0F,
            "Yawed translated view X") ||
        !ExpectMatrixValue(translated, 1, 3, -2.0F,
            "Yawed translated view Y") ||
        !ExpectMatrixValue(translated, 2, 3, -5.0F,
            "Yawed translated view Z")) {
        std::cerr << "World translation was applied in view-local space\n";
        return false;
    }

    if (penumbra_vr::runtime::ApplyWorldTranslationToView(
            penumbra_vr::runtime::IdentityMatrix(),
            {std::numeric_limits<float>::quiet_NaN(), 0.0F, 0.0F},
            translated, error) || error.empty()) {
        std::cerr << "A non-finite world translation was accepted\n";
        return false;
    }
    return true;
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
    std::array<float, 2> uv{};
    const auto menu_anchor = Translation(0, 1.6F, 0);
    if (!penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, menu_anchor, 4.0F / 3.0F, 2.0F, 2.4F, uv) ||
        std::abs(uv[0] - 0.5F) > 0.00001F || std::abs(uv[1] - 0.5F) > 0.00001F ||
        !penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, Translation(0.6F,1.9F,0), 4.0F / 3.0F,
            2.0F, 2.4F, uv) ||
        std::abs(uv[0] - 0.75F) > 0.00001F || std::abs(uv[1] - (1.0F / 3.0F)) > 0.00001F ||
        !penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, Translation(5,1.6F,0), 1, 2, 2.4F, uv) ||
        std::abs(uv[0] - 1.0F) > 0.00001F ||
        penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, menu_anchor, 0, 2, 2.4F, uv) ||
        penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, menu_anchor, 1, 0, 2.4F, uv) ||
        penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, PitchDegrees(180), 1, 2, 2.4F, uv) ||
        !penumbra_vr::runtime::ProjectAimOnMenu(
            menu_anchor, Translation(0.6F,1.9F,0), 4.0F / 3.0F,
            1.5F, 2.4F, uv)) {
        std::cerr << "Menu controller-ray intersection failed\n"; return 2;
    }
    std::array<float, 2> smoothed{};
    if (!penumbra_vr::runtime::SmoothMenuPointerUv(
            {0.25F, 0.75F}, {0.75F, 0.25F}, 0.40F, smoothed) ||
        std::abs(smoothed[0] - 0.45F) > 0.00001F ||
        std::abs(smoothed[1] - 0.55F) > 0.00001F ||
        penumbra_vr::runtime::SmoothMenuPointerUv(
            {0.25F, 0.75F}, {0.75F, 0.25F}, 1.1F, smoothed)) {
        std::cerr << "Menu pointer smoothing failed\n"; return 3;
    }
    if (!TestRigidInverse() || !TestProjection() ||
        !TestConservativeStereoCullFrustum() || !TestEyeViews() ||
        !TestRelativeHeadTracking() || !TestYawRecenteredHeadTracking() ||
        !TestWorldTranslationToView() || !TestInvalidInputs()) {
        return 1;
    }
    std::cout << "VR matrix tests passed\n";
    return 0;
}
