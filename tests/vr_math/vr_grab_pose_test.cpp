#include "vr_grab_pose.hpp"
#include <cmath>
#include <limits>
#include <iostream>
using namespace penumbra_vr::runtime;
int main() {
    auto palm = IdentityMatrix(), body = IdentityMatrix();
    palm.values[3] = 1; body.values[3] = 3;
    VrGrabPose grab;
    VrMatrix44 result;
    std::string error;
    if (grab.Update(palm,result,error) || !grab.Begin(palm,body,{},false,error)) return 1;
    palm.values[3] = 2;
    if (!grab.Update(palm,result,error) || result.values[3] != 4) return 2;
    if (!grab.Begin(palm,body,{0.2F,0,0},true,error) || !grab.Update(palm,result,error) ||
        std::abs(result.values[3] - 1.8F) > 0.00001F) return 3;
    palm.values[0] = 2;
    if (grab.Update(palm,result,error) || grab.Begin(palm,body,{},false,error)) return 4;
    if (LimitTrackedVelocity({100,0,0},1.25F,9) != std::array<float,3>{9,0,0} ||
        LimitTrackedVelocity({0,2,0},0.5F,12) != std::array<float,3>{0,1,0} ||
        LimitTrackedVelocity({std::numeric_limits<float>::infinity(),0,0},1,9) != std::array<float,3>{}) return 5;
    auto anchor = VrMatrix34{{1,0,0,0, 0,1,0,1.6F, 0,0,1,0}};
    VrHmdPose hand; hand.device_connected = true; hand.pose_valid = true;
    hand.device_to_absolute = anchor;
    hand.device_to_absolute.values[3] = 0.3F;
    hand.device_to_absolute.values[7] = 1.2F;
    hand.velocity = {1,2,3}; hand.angular_velocity = {0,1,0};
    std::array<float,3> velocity{}, angular{};
    if (!ControllerPoseInGame(IdentityMatrix(),anchor,hand,result,velocity,angular,error) ||
        std::abs(result.values[3] - 0.3F) > 0.00001F || std::abs(result.values[7] + 0.4F) > 0.00001F ||
        velocity != hand.velocity || angular != hand.angular_velocity) return 6;
    hand.pose_valid = false;
    if (ControllerPoseInGame(IdentityMatrix(),anchor,hand,result,velocity,angular,error)) return 7;
    std::cout << "Palm-relative grab, bounded release velocity and game-space poses passed\n";
}
