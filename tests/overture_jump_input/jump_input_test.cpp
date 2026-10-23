#include "overture_backend.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

// Extract the production input types without pulling in graphics/OpenVR.
#define PVR_INPUT_TYPES
#include "overture_jump_input_under_test.hpp"
#undef PVR_INPUT_TYPES
namespace hpl { using ::cVRButtonState; using ::cVRInputState; }
using namespace penumbra_vr;
using namespace penumbra_vr::backends::overture;
using runtime::VrButtonState;
using runtime::VrInputState;

struct Input {
    bool held{}, edge{};
    bool IsTriggerd(const char*) const { return held; }
    bool BecameTriggerd(const char*) { const bool result=edge; edge=false; return result; }
};
struct Updater { std::string GetCurrentContainerName() const { return "Default"; } };
struct Game {
    struct VrInput { cVRInputState state; const cVRInputState& GetState() const { return state; } } vr_input;
    Updater updater;
    float dt=1.0F/60.0F;
    float GetStepSize() const { return dt; }
    Updater* GetUpdater() { return &updater; }
};
struct Init { Game game; Game* mpGame=&game; };
struct LookAt { bool active{}; bool IsActive() const { return active; } };
class cPlayer final : public OvertureBodyAdapter {
public:
    explicit cPlayer(Init& init) : mpInit(&init) {
        std::string error;
        runtime::VrMatrix34 pose{{1,0,0,0, 0,1,0,1.7F, 0,0,1,0}};
        if(!backend.Initialize(pose,*this,error)) std::abort();
    }
    Init* mpInit;
    LookAt look;
    OvertureBackend backend;
    bool active=true, mbJumpButtonDown=false;
    float mfJumpCount=0, mfMaxJumpCount=0.3F;
    int jumps=0;
    bool IsActive() const { return active; }
    LookAt* GetLookAt() { return &look; }
    void Jump() { ++jumps; mfJumpCount=0; }
    void SetJumpButtonDown(bool);
    void HandleFrameworkVRInput(const cVRInputState&,float,bool,bool);
    std::array<float,3> BodyPosition() const noexcept override { return {}; }
    float FeetHeight() const noexcept override { return 0; }
    std::array<float,3> MoveBodyBy(const std::array<float,3>&,float,BodyMoveKind) noexcept override { return {}; }
    void StartJump() noexcept override { Jump(); }
    void SetJumpHeld(bool held) noexcept override { SetJumpButtonDown(held); }
};
class cButtonHandler {
public:
    Init* mpInit;
    cPlayer* mpPlayer;
    Input* mpInput;
    void UpdateRelevantJumpInput(float,bool,bool);
};

// Real ButtonHandler publication and later native Jump/cancel blocks, real
// source conversion, real Player setter and real Framework input consumer.
#include "overture_jump_input_under_test.hpp"
void cPlayer::HandleFrameworkVRInput(const cVRInputState& input,float dt,bool active_game,bool) {
    OvertureInputFrame frame;
    frame.input=ToRuntimeInput(input);
    frame.delta_seconds=dt;
    frame.gameplay_active=active_game;
    std::string error;
    if(!backend.HandleInput(frame,*this,error)) std::abort();
}

int main() {
    for(int source=0;source<3;++source) {
        Init init; Input keyboard; cPlayer player(init);
        cButtonHandler button{&init,&player,&keyboard};
        auto& jump=init.game.vr_input.state.jump;
        for(int tick=0;tick<8;++tick) {
            jump.active=true;
            jump.pressed=source!=1;
            jump.justPressed=tick==0 && source!=1;
            keyboard.held=source!=0;
            keyboard.edge=tick==0 && source!=0;
            button.UpdateRelevantJumpInput(init.game.dt,false,true);
            const float expected=static_cast<float>(tick+1)*init.game.dt;
            if(!player.mbJumpButtonDown || std::fabs(player.mfJumpCount-expected)>0.00001F || player.jumps!=1) {
                std::cerr<<"ButtonHandler overwrote/recounted held Jump: source="<<source
                    <<" tick="<<tick<<" held="<<player.mbJumpButtonDown<<" count="<<player.mfJumpCount<<'\n';
                return 1;
            }
        }
        // One input source releasing must preserve the other's ongoing hold.
        jump.justPressed=false; keyboard.edge=false;
        jump.pressed=true; keyboard.held=false;
        button.UpdateRelevantJumpInput(init.game.dt,false,true);
        if(!player.mbJumpButtonDown || player.mfJumpCount>=player.mfMaxJumpCount) return 2;
        jump.pressed=false; keyboard.held=true;
        button.UpdateRelevantJumpInput(init.game.dt,false,true);
        if(!player.mbJumpButtonDown || player.mfJumpCount>=player.mfMaxJumpCount) return 3;
        keyboard.held=false;
        button.UpdateRelevantJumpInput(init.game.dt,false,true);
        if(player.mbJumpButtonDown || player.mfJumpCount!=player.mfMaxJumpCount) return 4;
        for(int gate=0;gate<4;++gate) {
            jump.pressed=true;
            player.active=gate!=0;
            player.look.active=gate==1;
            button.UpdateRelevantJumpInput(init.game.dt,gate==2,gate!=3);
            if(player.mbJumpButtonDown) return 5;
        }
    }
    std::cout<<"ButtonHandler -> Framework -> Player Jump input ownership passed\n";
}
