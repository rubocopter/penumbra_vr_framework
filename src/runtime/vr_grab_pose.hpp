#pragma once
#include "vr_math.hpp"
namespace penumbra_vr::runtime {
// Rework's rigid palm-relative hold, independent of HPL entities and Newton.
class VrGrabPose final {
public:
    [[nodiscard]] bool Begin(const VrMatrix44& palm, const VrMatrix44& body,
        const std::array<float, 3>& local_contact, bool contact_in_palm, std::string& error) noexcept;
    [[nodiscard]] bool Update(const VrMatrix44& palm, VrMatrix44& body, std::string& error) const noexcept;
    void Reset() noexcept { active_ = false; local_ = {}; }
private:
    bool active_ = false;
    VrMatrix44 local_;
};
[[nodiscard]] std::array<float, 3> LimitTrackedVelocity(
    std::array<float, 3> velocity, float scale, float maximum) noexcept;
[[nodiscard]] bool ControllerPoseInGame(const VrMatrix44& game_view, const VrMatrix34& anchor,
    const VrHmdPose& controller, VrMatrix44& pose, std::array<float, 3>& velocity,
    std::array<float, 3>& angular, std::string& error) noexcept;
}
