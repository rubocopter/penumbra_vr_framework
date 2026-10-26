#pragma once
#include "game/Game.h"
// Config and native body calls are the external boundary. Tracking transforms,
// source adapter and backend/reconciliation are compiled from production.
enum eVRTurnMode { eVRTurnMode_Disabled,eVRTurnMode_Snap,eVRTurnMode_Smooth };
enum eVRPlayMode { eVRPlayMode_Standing,eVRPlayMode_Seated };
struct cVRSettings {
    float GetMoveSpeed() const { return 1.0f; }
    float GetMoveDeadZone() const { return 0.15f; }
    float GetHeightOffset() const { return 0.1f; }
    eVRTurnMode GetTurnMode() const { return eVRTurnMode_Snap; }
    float GetSnapTurnAngle() const { return 45.0f; }
    float GetSmoothTurnSpeed() const { return 90.0f; }
    float GetTurnDeadZone() const { return 0.2f; }
    eVRPlayMode GetPlayMode() const { return eVRPlayMode_Standing; }
    float GetPlayerHeight() const { return 1.7f; }
};
struct cInit { hpl::cGame* mpGame; cVRSettings mVRSettings; };
