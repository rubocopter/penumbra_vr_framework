#include "vr_hand_pose.hpp"
#include <algorithm>
#include <cmath>

namespace penumbra_vr::runtime {
VrHandArticulation ArticulateVrHand(const std::array<float,5>& curls, bool left) noexcept {
    VrHandArticulation result;
    const float mirror=left ? -1.0F : 1.0F;
    constexpr std::array<float,5> open_spread{0,4,0,-3,-7};
    for (std::size_t finger=0;finger<curls.size();++finger) {
        const float curl=std::isfinite(curls[finger]) ? std::clamp(curls[finger],0.0F,1.0F) : 0;
        auto& pose=result.fingers[finger];
        if (finger==0) {
            // Thumb: metacarpal opposition plus two phalange joints; do not
            // treat it as a fourth identical three-phalange finger chain.
            pose.flexion_degrees={20*curl,45*curl,60*curl};
            result.thumb_yaw_degrees=mirror*(48-33*curl);
        } else {
            // Separate proximal/middle/distal flexion. Distal flexion follows
            // the middle joint progressively rather than folding in lockstep.
            pose.flexion_degrees={65*curl,85*curl,50*curl*curl};
            pose.spread_degrees=mirror*open_spread[finger]*(1-curl);
        }
    }
    return result;
}
}
