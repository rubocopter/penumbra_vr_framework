#include "vr_hand_pose.hpp"
#include <cmath>
#include <limits>
#include <iostream>
using namespace penumbra_vr::runtime;
int main() {
    std::array<float,5> curls{};
    auto previous=ArticulateVrHand(curls,false);
    for (int step=0;step<=100;++step) {
        curls.fill(static_cast<float>(step)/100);
        const auto right=ArticulateVrHand(curls,false),left=ArticulateVrHand(curls,true);
        if (right.thumb_yaw_degrees!=-left.thumb_yaw_degrees ||
            right.thumb_yaw_degrees<15 || right.thumb_yaw_degrees>48) return 1;
        for (std::size_t f=0;f<5;++f) {
            if (right.fingers[f].spread_degrees!=-left.fingers[f].spread_degrees) return 2;
            for (std::size_t j=0;j<3;++j) {
                const float angle=right.fingers[f].flexion_degrees[j];
                if (!std::isfinite(angle) || angle<0 || angle>85 ||
                    angle<previous.fingers[f].flexion_degrees[j] || angle!=left.fingers[f].flexion_degrees[j]) return 3;
            }
        }
        previous=right;
    }
    curls={0,0.5F,0,0,0};
    const auto isolated=ArticulateVrHand(curls,false);
    if (isolated.fingers[1].flexion_degrees!=std::array<float,3>{32.5F,42.5F,12.5F} ||
        isolated.fingers[2].flexion_degrees!=std::array<float,3>{}) return 4;
    curls={std::numeric_limits<float>::quiet_NaN(),-1,2,std::numeric_limits<float>::infinity(),0};
    const auto invalid=ArticulateVrHand(curls,false);
    if (invalid.thumb_yaw_degrees!=48 || invalid.fingers[1].flexion_degrees!=std::array<float,3>{} ||
        invalid.fingers[2].flexion_degrees!=std::array<float,3>{65,85,50} ||
        invalid.fingers[3].flexion_degrees!=std::array<float,3>{}) return 5;
    std::cout<<"Independent hand articulation: mirroring, limits, per-finger response and invalid input passed\n";
}
