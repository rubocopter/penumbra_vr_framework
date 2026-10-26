#pragma once
#include "openvr.h"
#include "input/SteamVRInput.h"
namespace hpl {
struct cGame {
    vr::IVRSystem* vr_hmd;
    TrackedController vr_left_hand, vr_right_hand;
    eSteamVRHand vr_dominant_hand=eSteamVRHand_Right;
    struct Tracking { cMatrixf TrackingToWorld(const cMatrixf& pose) { return pose; } } vr_tracking;
    cVector3f pointerDrawStart, pointerDrawEnd;
    bool pointerDrawActive=false;
};
}
