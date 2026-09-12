#include "vr_grab_pose.hpp"
#include <cmath>
#include <limits>
#include <iostream>
using namespace penumbra_vr::runtime;
int main() {
    auto palm = IdentityMatrix(), body = IdentityMatrix();
    palm.values[3] = 1; body.values[3] = 3;
    const VrAttachmentSocketProfile socket{
        {1,0,0, 0,0,-1, 0,1,0},
        {1,2,3}};
    auto attachment_hand = IdentityMatrix();
    attachment_hand.values[3] = 10;
    attachment_hand.values[7] = 20;
    attachment_hand.values[11] = 30;
    const auto attachment = ComposeAttachmentSocketPose(attachment_hand,socket);
    const std::array<float,3> mapped_socket{
        attachment.values[0]*socket.model_grip_point[0] +
            attachment.values[1]*socket.model_grip_point[1] +
            attachment.values[2]*socket.model_grip_point[2] + attachment.values[3],
        attachment.values[4]*socket.model_grip_point[0] +
            attachment.values[5]*socket.model_grip_point[1] +
            attachment.values[6]*socket.model_grip_point[2] + attachment.values[7],
        attachment.values[8]*socket.model_grip_point[0] +
            attachment.values[9]*socket.model_grip_point[1] +
            attachment.values[10]*socket.model_grip_point[2] + attachment.values[11]};
    if (mapped_socket != std::array<float,3>{10,20,30} ||
        attachment.values[5] != 0 || attachment.values[6] != -1 ||
        attachment.values[9] != 1 || attachment.values[10] != 0) return 10;
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
    VrReleaseVelocity release;
    std::array<float,3> release_linear{},release_angular{};
    release.Add({1,2,3},{0,1,0});
    release.Estimate(release_linear,release_angular);
    if (release_linear!=std::array<float,3>{}) return 8;
    release.Add({1,2,3},{0,1,0});
    release.Add({100,-100,50},{20,20,20});
    release.Add({1.2F,2.2F,3.2F},{0,1.2F,0});
    release.Add({0.8F,1.8F,2.8F},{0,0.8F,0});
    release.Estimate(release_linear,release_angular);
    if (release.sample_count()!=5 || release_linear!=std::array<float,3>{1,2,3} ||
        release_angular!=std::array<float,3>{0,1,0}) return 9;
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
