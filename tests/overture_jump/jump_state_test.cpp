#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
// The product includes Windows headers without NOMINMAX. Exercise that include
// environment as well as the generated state's actual force consumer.
#define min(left, right) (((left) < (right)) ? (left) : (right))
#include "../../src/backends/overture/native_jump_envelope.hpp"
#undef min

// Boundary objects record the real production Jump state's calls. Physics
// integration below characterizes the independently verified native equation;
// this is host evidence, not a replacement collision solver or headset trial.
struct cVector3f {
    float x{}, y{}, z{};
    cVector3f(float value = 0) : x(value), y(value), z(value) {}
    cVector3f(float a, float b, float c) : x(a), y(b), z(c) {}
    cVector3f operator*(float s) const { return {x*s,y*s,z*s}; }
    cVector3f& operator+=(cVector3f v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    float Length() const { return std::sqrt(x*x+y*y+z*z); }
};
enum eCharDir { eCharDir_Forward, eCharDir_Right };
enum ePlayerMoveState { ePlayerMoveState_Still, ePlayerMoveState_Walk,
    ePlayerMoveState_Run, ePlayerMoveState_Jump, ePlayerMoveState_Crouch };
struct Matrix {
    cVector3f GetForward() const { return {0,0,1}; }
    cVector3f GetRight() const { return {1,0,0}; }
};
struct iCharacterBody {
    cVector3f force{}, velocity{}, gravity{0,-10,0};
    std::array<float,2> speed{};
    bool grounded = false;
    bool custom_gravity_active = false;
    bool gravity_active = true;
    float mass = 80;
    float max_gravity_speed = 0;
    Matrix GetMoveMatrix() const { return {}; }
    float GetMoveSpeed(eCharDir dir) const { return speed[dir]; }
    void SetMoveSpeed(eCharDir dir,float v) { speed[dir]=v; }
    void AddForce(cVector3f f) { force+=f; }
    void AddForceVelocity(cVector3f v) { velocity+=v; }
    cVector3f GetForceVelocity() const { return velocity; }
    cVector3f GetCustomGravity() const { return gravity; }
    cVector3f GetForce() const { return force; }
    float GetMass() const { return mass; }
    bool GravityIsActive() const { return gravity_active; }
    bool GetCustomGravityActive() const { return custom_gravity_active; }
    bool IsOnGround() const { return grounded; }
    void SetMaxGravitySpeed(float v) { max_gravity_speed=v; }
    void SetCustomGravityActive(bool active) { custom_gravity_active=active; }
    void SetCustomGravity(cVector3f v) { gravity=v; }
};
struct Config {
    float jump_force=350;
    float GetFloat(const char* section, const char* name,float) const {
        if (std::string(section)=="Player" && std::string(name)=="JumpStartForce") return jump_force;
        return 3;
    }
};
struct Game { float dt = 1.0F/60.0F; float GetStepSize() const { return dt; } };
struct cInit { Config config; Game game; Config* mpGameConfig=&config; Game* mpGame=&game; };
struct cPlayer;
struct iPlayerMoveState {
    iPlayerMoveState(cPlayer* player,cInit* init) : mpPlayer(player),mpInit(init),mpGameConfig(init->mpGameConfig) {}
    cPlayer* mpPlayer;
    cInit* mpInit;
    Config* mpGameConfig;
    ePlayerMoveState mType=ePlayerMoveState_Walk;
    ePlayerMoveState mPrevMoveState=ePlayerMoveState_Walk;
    float mfForwardSpeed=3,mfBackwardSpeed=3,mfSidewaySpeed=3;
    float mfForwardAcc{},mfForwardDeacc{},mfSidewayAcc{},mfSidewayDeacc{};
    float mfMaxHeadMove{},mfMinHeadMove{},mfHeadMoveSpeed{},mfHeadMoveBackSpeed{},mfFootStepMul{};
    std::string msStepType;
};
struct cPlayer {
    explicit cPlayer(cInit* init = null_init()) : mpInit(init) {}
    cInit* mpInit;
    iCharacterBody body;
    iPlayerMoveState walk{this,null_init()};
    bool mbJumpButtonDown=true,landed=false;
    float mfJumpCount=0;
    float mfMaxJumpCount=0.3F;
    struct PrimaryState { bool OnJump() const { return true; } } primary;
    std::array<PrimaryState*,1> mvStates{&primary};
    std::array<iPlayerMoveState*,5> mvMoveStates{&walk,&walk,&walk,&walk,&walk};
    int mState=0,mlGroundCount=0;
    ePlayerMoveState mMoveState=ePlayerMoveState_Jump;
    ePlayerMoveState state=ePlayerMoveState_Jump;
    static cInit* null_init() { static cInit init; return &init; }
    iCharacterBody* GetCharacterBody() { return &body; }
    float GetDefaultMass() const { return 80; }
    iPlayerMoveState* GetMoveStateData(ePlayerMoveState) { return &walk; }
    bool GetJumpButtonDown() const { return mbJumpButtonDown; }
    float GetJumpCount() const { return mfJumpCount; }
    float GetMaxJumpCount() const { return mfMaxJumpCount; }
    void Jump();
    void SetJumpButtonDown(bool abX);
    void ChangeMoveState(ePlayerMoveState next) { state=next; }
    bool GetLandedFromJump() const { return landed; }
    void SetLandedFromJump(bool v) { landed=v; }
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4100 4189) // Unused locals/parameters in the unmodified game class.
#endif
#include "overture_jump_state_under_test.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

bool Near(float a,float b,float tolerance=0.001F) { return std::fabs(a-b)<tolerance; }
void InitializeProductionBodyGravity(iCharacterBody* mpCharBody) {
#include "overture_body_gravity_under_test.inl"
}
struct JumpEnvelope { float height{}, airtime{}; };
JumpEnvelope MeasureHostEnvelope(int hz,float hold_seconds,bool crouched=false,
    bool repress=false,bool reset_on_repress=true) {
    cInit init; init.game.dt=1.0F/static_cast<float>(hz);
    cPlayer player(&init);
    InitializeProductionBodyGravity(&player.body);
    cPlayerMoveState_Jump jump(&player,&init);
    player.mvMoveStates[ePlayerMoveState_Jump]=&jump;
    iPlayerMoveState previous(&player,&init);
    previous.mType=crouched?ePlayerMoveState_Crouch:ePlayerMoveState_Walk;
    jump.EnterState(&previous);
    player.Jump();
    player.SetJumpButtonDown(hold_seconds>0);
    bool previous_held=hold_seconds>0;
    // Scene physics runs before the global/default input and player updates.
    // EnterState and its skipped first OnUpdate precede the next physics tick.
    jump.OnUpdate(init.game.dt);
    float y=0,peak=0;
    for (int tick=1; tick<hz*3; ++tick) {
        // CharacterBody.cpp's characterized native force/gravity integration.
        player.body.velocity += player.body.force*(init.game.dt/player.body.GetMass());
        player.body.velocity += player.body.gravity*init.game.dt;
        player.body.force={};
        y+=player.body.velocity.y*init.game.dt;
        peak=std::fmax(peak,y);
        if (y<=0 && player.body.velocity.y<0) return {peak,tick*init.game.dt};
        bool held=tick*init.game.dt<hold_seconds;
        if(repress && tick*init.game.dt>=0.2F) held=true;
        if(held && !previous_held && reset_on_repress) player.Jump();
        player.SetJumpButtonDown(held);
        previous_held=held;
        jump.OnUpdate(init.game.dt);
    }
    return {};
}
int main() {
    for (const int hz : {30,60,72,90,120}) {
        const auto released=MeasureHostEnvelope(hz,0.1F);
        const auto repressed=MeasureHostEnvelope(hz,0.1F,false,true);
        const auto no_reset=MeasureHostEnvelope(hz,0.1F,false,true,false);
        if(repressed.height<=released.height+0.02F ||
            repressed.airtime<=released.airtime+0.05F) {
            std::cerr<<"Real Jump() edge did not renew native held ascent: hz="<<hz
                <<" released="<<released.height<<" repressed="<<repressed.height<<'\n';
            return 12;
        }
        if(!Near(released.height,no_reset.height) ||
            !Near(released.airtime,no_reset.airtime)) {
            std::cerr<<"Held setter renewed an exhausted counter without Jump()\n";
            return 15;
        }
        // Independently integrated native 60 Hz equations: hold to 0.1 s,
        // release, then Jump() at 0.2 s resets count but does not relaunch.
        // These literals distinguish counter renewal from a second launch.
        if(hz==60 && (!Near(repressed.height,1.0391663F,0.002F) ||
            !Near(repressed.airtime,0.8F,0.002F) ||
            !Near(no_reset.height,0.8544443F,0.002F) ||
            !Near(no_reset.airtime,0.5333333F,0.002F))) {
            std::cerr<<"Release/repress diverged from native 60 Hz trajectory\n";
            return 16;
        }
    }
    // Profile/solver changes must return vertical ownership immediately,
    // including changes between EnterState and the first physics tick.
    for (int stage=0; stage<3; ++stage) {
        for (int change=0; change<7; ++change) {
            cInit init;
            cPlayer player(&init);
            InitializeProductionBodyGravity(&player.body);
            const auto override_native = [&] {
                if(change==0) player.body.custom_gravity_active=false;
                if(change==1) player.body.gravity_active=false;
                if(change==2) player.mfMaxJumpCount=0.6F;
                if(change==3) player.body.mass=40;
                if(change==4) player.body.gravity.x=1;
                if(change==5) player.body.velocity.y=-2; // Low ceiling/external impulse.
                if(change==6) player.body.force.y=25; // Pending scripted force.
            };
            if(stage==0) override_native();
            cPlayerMoveState_Jump jump(&player,&init);
            jump.EnterState(&player.walk);
            if(stage==2) {
                jump.OnUpdate(init.game.dt);
                player.body.velocity += player.body.force*(init.game.dt/player.body.GetMass());
                player.body.velocity += player.body.gravity*init.game.dt;
                player.body.force={};
            }
            if(stage!=0) override_native();
            jump.OnUpdate(init.game.dt);
            if(jump.mbNativeJumpEnvelope) {
                std::cerr<<"Jump retained envelope after native override "<<change<<'\n';
                return 11;
            }
            const float expected_force=stage==2?18.0F*player.body.mass*0.9F:47600.0F;
            if(!Near(player.body.force.y,expected_force+(change==6?25.0F:0.0F),0.01F)) {
                std::cerr<<"Native override did not consume original force branch\n";
                return 13;
            }
        }
    }
    {
        cInit init; init.config.jump_force=700;
        cPlayer player(&init);
        InitializeProductionBodyGravity(&player.body);
        cPlayerMoveState_Jump jump(&player,&init);
        jump.EnterState(&player.walk);
        jump.OnUpdate(init.game.dt);
        if(!Near(player.body.force.y,95200.0F,0.01F)) return 14;
    }
    // Characterized free-space envelopes expose timestep quantization instead
    // of equating source constants with live height/airtime parity. Both binary
    // builds use the 60 Hz reference. HMD-rate Overture keeps its native solver.
    for (const bool crouched : {false,true}) {
      for (const float hold : {0.0F,0.3F}) {
        const auto reference=MeasureHostEnvelope(60,hold,crouched);
        // Hand-checked native 60 Hz integration, independent of the policy.
        const float expected_height=crouched?(hold==0.0F?0.1125F:0.5451111F):(hold==0.0F?0.45F:1.5247778F);
        const float expected_airtime=crouched?(hold==0.0F?0.1833333F:0.5333333F):(hold==0.0F?0.3833333F:0.8F);
        if (!Near(reference.height,expected_height,0.002F) ||
            !Near(reference.airtime,expected_airtime,0.002F)) return 10;
        for (const int hz : {30,60,72,90,120}) {
            const auto envelope=MeasureHostEnvelope(hz,hold,crouched);
            std::cout<<"host_jump hz="<<hz<<" crouched="<<crouched<<" hold="<<hold
                <<" height="<<envelope.height<<" airtime="<<envelope.airtime<<'\n';
            if (!(envelope.height>0 && envelope.airtime>0) ||
                std::fabs(envelope.height-reference.height)>0.025F ||
                std::fabs(envelope.airtime-reference.airtime)>1.0F/static_cast<float>(hz)+0.001F) {
                std::cerr<<"Actual Jump consumer diverges from native 60 Hz tap/hold envelope\n";
                return 9;
            }
        }
      }
    }
    // Native normal launch at the original 60 Hz logic rate: 350/60 =
    // 5.833333 m/s before gravity. It must stay the same at HMD logic rates.
    for (const int hz : {30,60,72,90,120}) {
        for (const bool crouched : {false,true}) {
            cInit init; init.game.dt=1.0F/static_cast<float>(hz);
            cPlayer player(&init);
            InitializeProductionBodyGravity(&player.body);
            if (!Near(player.body.gravity.y,-18) || !player.body.custom_gravity_active ||
                !Near(player.body.max_gravity_speed,40)) {
                std::cerr<<"Overture's native gravity initialization differs from the normal BP/Requiem profile\n";
                return 8;
            }
            iPlayerMoveState previous(&player,&init);
            previous.mType=crouched?ePlayerMoveState_Crouch:ePlayerMoveState_Walk;
            player.body.speed={3,0};
            cPlayerMoveState_Jump jump(&player,&init);
            jump.EnterState(&previous);
            jump.OnUpdate(init.game.dt); // Prepare the first native physics tick.
            const float impulse=player.body.force.y*init.game.dt/80;
            // First native interval v=350/60-18/60; a 30 Hz frame spans two
            // 60 Hz native intervals. Held second interval adds -2.8 m/s^2.
            const float initial_velocity=crouched?2.6166667F:5.5333333F;
            const float expected=(hz==30?initial_velocity-0.023333333F:initial_velocity)+18*init.game.dt;
            if (!Near(impulse,expected)) {
                std::cerr<<"Overture launch hz="<<hz<<" crouched="<<crouched
                    <<" velocity="<<impulse<<" expected="<<expected<<'\n';
                return 1;
            }
            if (!Near(player.body.velocity.z,-3) || !Near(player.body.speed[0],0)) return 2;
            player.body.force={};
            // A ceiling/external vertical impulse overrides sampling. The
            // original native held/release branches must still be consumed.
            player.body.velocity.y=1;
            for (const auto sample : {std::array<float,2>{0,1296}, {0.15F,936}, {0.3F,-1600}}) {
                player.body.force={}; player.mfJumpCount=sample[0];
                jump.OnUpdate(init.game.dt);
                if (!Near(player.body.force.y,sample[1])) return 4;
            }
            player.mfJumpCount=0; player.SetJumpButtonDown(false); player.body.force={};
            jump.OnUpdate(init.game.dt);
            if (!Near(player.body.force.y,-1600)) return 5;
            player.body.velocity.y=-1; player.body.force={};
            jump.OnUpdate(init.game.dt);
            if (!Near(player.body.force.y,0)) return 6;
            player.body.grounded=true; player.body.velocity={};
            jump.OnUpdate(init.game.dt);
            if (player.state!=previous.mType) return 7;
        }
    }
    std::cout<<"Overture native jump launch/hold/release boundary passed\n";
    return 0;
}
