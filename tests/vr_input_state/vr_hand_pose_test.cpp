#include "vr_hand_pose.hpp"
#include <cmath>
#include <limits>
#include <iostream>
using namespace penumbra_vr::runtime;
int main() {
    const auto near=[](float a,float b) { return std::abs(a-b)<0.00001F; };
    if (RemapVrFingerCurl(0.15F,0.15F,0.98F)!=0 ||
        RemapVrFingerCurl(0.98F,0.15F,0.98F)!=1 ||
        !near(RemapVrFingerCurl(0.565F,0.15F,0.98F),0.5F)) return 1;
    if (ApplyVrFingerCurlDeadzone(0.08F)!=0 ||
        !near(ApplyVrFingerCurlDeadzone(0.54F),0.5F) ||
        ApplyVrFingerCurlDeadzone(2.0F)!=1) return 2;

    VrHandCurlInput input;
    input.valid=true; input.skeletal=true;
    input.finger_curl={0.04F,0.08F,0.54F,1.0F,2.0F};
    auto target=BuildVrHandCurlTargets(input);
    if (target[0]!=0 || target[1]!=0 || !near(target[2],0.5F) ||
        target[3]!=1 || target[4]!=1) return 3;
    input.grip=0.565F;
    target=BuildVrHandCurlTargets(input);
    if (!near(target[0],0.5F)) return 4;

    input={}; input.valid=true; input.grip=1; input.trigger=1;
    target=BuildVrHandCurlTargets(input);
    for (float curl : target) if (curl!=1) return 5;
    input={};
    if (BuildVrHandCurlTargets(input)!=std::array<float,5>{}) return 6;

    const float blend70=VrHandCurlSmoothingBlend(0.07F);
    if (!near(blend70,1.0F-std::exp(-1.0F)) ||
        !near(VrHandCurlSmoothingBlend(0.2F),1.0F-std::exp(-0.1F/0.07F)) ||
        VrHandCurlSmoothingBlend(std::numeric_limits<float>::quiet_NaN())!=0) return 7;
    std::array<float,5> smoothed{};
    SmoothVrHandCurls(smoothed,{1,1,1,1,1},0.07F);
    for (float curl : smoothed) if (!near(curl,blend70)) return 8;

    std::array<float,5> curls{};
    auto previous=ArticulateVrHand(curls,false);
    for (int step=0;step<=100;++step) {
        curls.fill(static_cast<float>(step)/100);
        const auto right=ArticulateVrHand(curls,false),left=ArticulateVrHand(curls,true);
        if (right.thumb_yaw_degrees!=0 || left.thumb_yaw_degrees!=0) return 9;
        for (std::size_t f=0;f<5;++f) {
            if (right.fingers[f].spread_degrees!=0 ||
                left.fingers[f].spread_degrees!=0) return 10;
            for (std::size_t j=0;j<3;++j) {
                const float angle=right.fingers[f].flexion_degrees[j];
                if (!std::isfinite(angle) || angle<0 || angle>66 ||
                    angle<previous.fingers[f].flexion_degrees[j] ||
                    angle!=left.fingers[f].flexion_degrees[j]) return 11;
            }
        }
        previous=right;
    }
    curls={0,0.5F,0,0,0};
    const auto isolated=ArticulateVrHand(curls,false);
    if (isolated.fingers[1].flexion_degrees!=std::array<float,3>{24.0F,31.0F,18.0F} ||
        isolated.fingers[2].flexion_degrees!=std::array<float,3>{}) return 12;
    curls={std::numeric_limits<float>::quiet_NaN(),-1,2,std::numeric_limits<float>::infinity(),0};
    const auto invalid=ArticulateVrHand(curls,false);
    if (invalid.thumb_yaw_degrees!=0 ||
        invalid.fingers[0].flexion_degrees!=std::array<float,3>{} ||
        invalid.fingers[1].flexion_degrees!=std::array<float,3>{} ||
        invalid.fingers[2].flexion_degrees!=std::array<float,3>{52,66,38} ||
        invalid.fingers[3].flexion_degrees!=std::array<float,3>{}) return 13;
    curls={1,1,1,1,1};
    const auto closed=ArticulateVrHand(curls,false);
    if (closed.fingers[0].flexion_degrees!=std::array<float,3>{22,28,16} ||
        closed.fingers[1].flexion_degrees!=std::array<float,3>{48,62,36} ||
        closed.fingers[2].flexion_degrees!=std::array<float,3>{52,66,38} ||
        closed.fingers[3].flexion_degrees!=std::array<float,3>{52,66,38} ||
        closed.fingers[4].flexion_degrees!=std::array<float,3>{52,66,38}) return 14;
    std::cout<<"Hand conditioning and articulation: Rework mapping, smoothing and rigid-skin pose limits passed\n";
}
