#include "overture_source_integration.hpp"
#include "Init.h"
#include "Player.h"
#include "input/SteamVRInput.h"
#include <cmath>
#include <cstdio>
using penumbra_vr::adapters::overture_source::OvertureSourceIntegration;
int failures=0;
void Check(bool value,const char* name) {
    if(!value) { ++failures; std::fprintf(stderr,"FAIL: %s\n",name); }
}
bool Near(float actual,float expected) { return std::fabs(actual-expected)<0.0001f; }
hpl::cMatrixf Pose(float yaw,float x=0.0f) {
    auto pose=hpl::cMath::MatrixRotateY(yaw);
    pose.SetTranslation({x,1.7f,0.0f}); return pose;
}
int main() {
    hpl::cGame game; cInit init{&game}; cPlayer player;
    OvertureSourceIntegration integration;
    hpl::cVRInputState input;
    input.recenter.justPressed=true;
    game.vr_tracking.SetHeadTrackingPose(Pose(0.7f));
    std::string error;
    Check(integration.HandleInput(player,init,input,0.01f,false,false,error),"menu recenter before body creation succeeds");
    Check(Near(game.vr_tracking.GetWorldYaw(),-0.7f),"bodyless recenter uses current HMD yaw");
    Check(player.held_writes==0,"bodyless menu cannot write native gameplay");

    hpl::iCharacterBody body; player.body=&body;
    input={};
    Check(integration.UpdatePlayer(player,init,0.01f,true,false,error),"body initializes after menu");
    Check(Near(game.vr_tracking.GetWorldYaw(),-0.7f),"body creation preserves menu recenter");
    // Gameplay's previous pose is deliberately different from the pause pose.
    game.vr_tracking.SetHeadTrackingPose(Pose(-0.4f,0.03f));
    input.recenter.justPressed=true;
    Check(integration.HandleInput(player,init,input,0.01f,false,false,error),"pause recenter succeeds");
    Check(Near(game.vr_tracking.GetWorldYaw(),0.4f),"pause recenter uses current pose instead of last gameplay pose");
    Check(Near(game.vr_tracking.GetHeightCalibration(),0.1f),"recenter preserves configured height");
    Check(body.updates==0,"menu recenter never moves native body");

    // In Default the global + container publication must consume jump/turn once.
    input={}; input.jump.pressed=true; input.jump.justPressed=true;
    input.turnActive=true; input.turnX=1.0f;
    input.moveActive=true; input.moveY=1.0f;
    Check(integration.HandleInput(player,init,input,0.1f,true,true,error),"gameplay publication succeeds");
    const float yaw=game.vr_tracking.GetWorldYaw(); const int held=player.held_writes;
    Check(integration.HandleInput(player,init,input,0.1f,true,true,error),"duplicate container publication consumed");
    Check(player.jumps==1 && player.held_writes==held && Near(game.vr_tracking.GetWorldYaw(),yaw),"duplicate cannot repeat jump hold or turn");
    Check(integration.UpdatePlayer(player,init,0.1f,true,false,error),"player consumes movement after recenter");
    Check(body.updates==1 && Near(std::hypot(body.position.x,body.position.z),0.3f),"pause recenter resets physical baseline and preserves exactly one stick step");
    const auto prior=body.position;
    Check(integration.UpdatePlayer(player,init,0.1f,true,false,error),"repeat player sample valid");
    Check(body.updates==1 && Near(body.position.x,prior.x) && Near(body.position.z,prior.z),"stick movement consumed exactly once");

    // Publishing a fresh input pose must not advance the physical reconciliation
    // baseline. Otherwise ordinary room-scale motion silently disappears.
    input={}; game.vr_tracking.SetWorldYaw(0.0f);
    game.vr_tracking.SetHeadTrackingPose(Pose(0.0f,0.05f));
    Check(integration.HandleInput(player,init,input,0.01f,true,true,error),"fresh ordinary pose published");
    Check(integration.UpdatePlayer(player,init,0.01f,true,false,error),"physical reconciliation after input publication");
    Check(body.updates==2 && Near(body.position.x-prior.x,0.02f),"fresh pose does not swallow room-scale displacement");
    integration.Reset();
    std::printf("Overture source integration: %d failures\n",failures);
    return failures?1:0;
}
