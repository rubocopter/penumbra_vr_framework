// Palm-relative anchoring and throw limits adapted from Overture VR Rework
// 23c890f, PlayerState_Interact_VR.cpp. Frictional Games / Rework contributors,
// GPL-3.0-or-later; see THIRD_PARTY.md.
#include "vr_grab_pose.hpp"
#include <algorithm>
#include <cmath>
namespace penumbra_vr::runtime {
namespace {
VrMatrix34 Rigid(const VrMatrix44& matrix) {
    VrMatrix34 result;
    std::copy_n(matrix.values.begin(), result.values.size(), result.values.begin()); return result;
}
bool Inverse(const VrMatrix44& matrix, VrMatrix44& inverse, std::string& error) {
    if (matrix.values[12] != 0 || matrix.values[13] != 0 || matrix.values[14] != 0 || matrix.values[15] != 1) {
        error = "Grab transform is not affine"; return false;
    }
    return InvertRigidTransform(Rigid(matrix), inverse, error);
}
}
bool VrGrabPose::Begin(const VrMatrix44& palm, const VrMatrix44& body,
    const std::array<float, 3>& contact, bool contact_in_palm, std::string& error) noexcept {
    Reset(); error.clear();
    VrMatrix44 inverse, ignored;
    if (!Inverse(palm, inverse, error) || !Inverse(body, ignored, error)) return false;
    if (!std::all_of(contact.begin(), contact.end(), [](float v) { return std::isfinite(v); })) {
        error = "Non-finite grab contact"; return false;
    }
    local_ = Multiply(inverse, body);
    if (contact_in_palm) {
        for (std::size_t row = 0; row < 3; ++row)
            local_.values[row * 4 + 3] = -(local_.values[row * 4] * contact[0] +
                local_.values[row * 4 + 1] * contact[1] + local_.values[row * 4 + 2] * contact[2]);
    }
    active_ = true; return true;
}
bool VrGrabPose::Update(const VrMatrix44& palm, VrMatrix44& body, std::string& error) const noexcept {
    body = {}; error.clear();
    if (!active_) { error = "No active VR grab"; return false; }
    VrMatrix44 ignored;
    if (!Inverse(palm, ignored, error)) return false;
    body = Multiply(palm, local_); return true;
}
std::array<float, 3> LimitTrackedVelocity(std::array<float, 3> v, float scale, float maximum) noexcept {
    if (!std::isfinite(scale) || !std::isfinite(maximum) || scale < 0 || maximum < 0 ||
        !std::all_of(v.begin(), v.end(), [](float x) { return std::isfinite(x); })) return {};
    const double length = std::hypot(static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2]));
    const double factor = length > 0 ? std::min(static_cast<double>(scale), maximum / length) : 0;
    for (float& x : v) x = static_cast<float>(x * factor);
    return v;
}
bool ControllerPoseInGame(const VrMatrix44& game_view, const VrMatrix34& anchor,
    const VrHmdPose& controller, VrMatrix44& pose, std::array<float, 3>& velocity,
    std::array<float, 3>& angular, std::string& error) noexcept {
    pose = {}; velocity = {}; angular = {}; error.clear();
    if (!controller.device_connected || !controller.pose_valid) { error = "Controller tracking is invalid"; return false; }
    VrMatrix44 view, tracking_inverse;
    if (!ComposeYawRecenteredTrackedHeadView(game_view, anchor, controller.device_to_absolute, 1, view, error) ||
        !Inverse(view, pose, error) || !InvertRigidTransform(controller.device_to_absolute, tracking_inverse, error)) return false;
    const auto transform = Multiply(pose, tracking_inverse);
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t c = 0; c < 3; ++c) {
            velocity[row] += transform.values[row * 4 + c] * controller.velocity[c];
            angular[row] += transform.values[row * 4 + c] * controller.angular_velocity[c];
        }
    velocity = LimitTrackedVelocity(velocity, 1, 20);
    angular = LimitTrackedVelocity(angular, 1, 12);
    return true;
}
}
