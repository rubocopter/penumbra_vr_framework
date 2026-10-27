// Production Move update with characterized native body calls and real Newton
// joints. No game process, renderer, headset or installed files are touched.
#pragma once
#include "../../products/overture/dependencies/include/Newton.h"

namespace penumbra_vr::backends::requiem::native_mechanism_fixture {
using Vec = std::array<float,3>;
using Matrix = runtime::VrMatrix44;
struct NewtonApi {
    HMODULE module = LoadLibraryExW(PVR_TEST_NEWTON_DLL,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
#define PVR_MECHANISM_API(X) \
    X(NewtonCreate) X(NewtonDestroy) X(NewtonCreateBox) X(NewtonReleaseCollision) \
    X(NewtonCreateBody) X(NewtonBodySetMassMatrix) X(NewtonBodySetCentreOfMass) \
    X(NewtonBodyGetCentreOfMass) X(NewtonBodySetMatrix) X(NewtonBodyGetMatrix) \
    X(NewtonBodySetVelocity) X(NewtonBodySetOmega) X(NewtonBodyGetVelocity) \
    X(NewtonBodyGetOmega) X(NewtonBodySetAutoFreeze) X(NewtonUpdate) \
    X(NewtonConstraintCreateHinge) X(NewtonHingeGetJointAngle) \
    X(NewtonHingeSetUserCallback) X(NewtonHingeCalculateStopAlpha)
#define PVR_LOAD(name) decltype(&::name) name = module ? reinterpret_cast<decltype(&::name)>(GetProcAddress(module,#name)) : nullptr;
    PVR_MECHANISM_API(PVR_LOAD)
#undef PVR_LOAD
    bool Ready() const {
#define PVR_READY(name) && name != nullptr
        return module != nullptr PVR_MECHANISM_API(PVR_READY);
#undef PVR_READY
    }
    ~NewtonApi() { if(module) FreeLibrary(module); }
#undef PVR_MECHANISM_API
};
inline NewtonApi* api=nullptr;
inline NewtonBody* native_body=nullptr;
inline Vec commanded_linear{},commanded_angular{};
inline unsigned matrix_writes=0,leaves=0;
inline Vec __fastcall Centre(void*,void*) {
    Vec value{}; api->NewtonBodyGetCentreOfMass(native_body,value.data()); return value;
}
inline void __fastcall Linear(void*,void*,const Vec* value) {
    commanded_linear=*value; api->NewtonBodySetVelocity(native_body,value->data());
}
inline void __fastcall Angular(void*,void*,const Vec* value) {
    commanded_angular=*value; api->NewtonBodySetOmega(native_body,value->data());
}
inline void __fastcall UnexpectedMatrix(void*,void*,const Matrix*) { ++matrix_writes; }
inline void __fastcall Leave(void*,void*,void*) { ++leaves; }
inline void __fastcall SetLimit(void*,void*,float) {}
inline void __fastcall Awake(void*,void*,bool) {}
inline Matrix Pose() {
    Matrix column{},row{}; api->NewtonBodyGetMatrix(native_body,column.values.data());
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c)
        row.values[r*4+c]=column.values[c*4+r];
    return row;
}
inline void SetPose(const Matrix& row) {
    Matrix column{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c)
        column.values[r*4+c]=row.values[c*4+r];
    api->NewtonBodySetMatrix(native_body,column.values.data());
}
inline bool Cases() {
    NewtonApi newton; if(!newton.Ready()) return false; api=&newton;
    JumpTo(0x19D0B0,reinterpret_cast<void*>(&Centre));
    JumpTo(0x19CA00,reinterpret_cast<void*>(&Linear));
    JumpTo(0x19CA20,reinterpret_cast<void*>(&Angular));
    JumpTo(kEntitySetMatrixRva,reinterpret_cast<void*>(&UnexpectedMatrix));
    JumpTo(kMoveTargets[2],reinterpret_cast<void*>(&Leave));
    JumpTo(0x19CAC0,reinterpret_cast<void*>(&SetLimit));
    JumpTo(0x19CAE0,reinterpret_cast<void*>(&SetLimit));
    JumpTo(0x19CBB0,reinterpret_cast<void*>(&Awake));
    std::array<std::uint8_t,0x500> body{},player{},character{};
    std::array<std::uint8_t,0x100> state{};
    Put(body.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(body.data(),0x434,2.0F);
    Put(player.data(),GameplayContract().character_body_offset,static_cast<void*>(character.data()));
    Put(player.data(),0x2C0,2);
    Put(state.data(),0x10,static_cast<void*>(player.data()));
    Put(state.data(),0x54,static_cast<void*>(body.data()));
    test_frame={}; test_frame.focused=true;
    test_frame.interact_source=runtime::VrHand::right;
    test_frame.input.state.interact.pressed=true;
    auto& grip=test_frame.hands[1].grip;
    grip.pose_valid=grip.device_connected=true;
    g_presenting.store(true); g_world_from_tracking=runtime::IdentityMatrix();
    g_world_from_tracking_identity={};
    const auto hand=[&](const Vec& contact,float yaw) {
        auto raw=runtime::IdentityMatrix();
        raw.values[0]=raw.values[10]=std::cos(yaw);
        raw.values[2]=std::sin(yaw); raw.values[8]=-std::sin(yaw);
        // Place the physical palm origin at the literal world-space contact.
        const auto visual=runtime::rework_hand_profile::ApplyVisualLocalPose(raw);
        for(unsigned i=0;i<3;++i) raw.values[i*4+3]=contact[i]-visual.values[i*4+3];
        std::copy_n(raw.values.begin(),12,grip.device_to_absolute.values.begin());
        g_world_from_tracking_time=GetTickCount64();
    };
    const auto seed=[&](const Matrix& pose,const Vec& contact,const Vec& pin,const Vec& pivot) {
        Put(body.data(),0x34,pose);
        g_move_hold={}; g_move_hold.state=state.data(); g_move_hold.player=player.data();
        g_move_hold.body=body.data(); g_move_hold.hand=runtime::VrHand::right;
        g_move_hold.mode=MoveMode::hinge; g_move_hold.joint_pin=pin; g_move_hold.joint_pivot=pivot;
        g_move_hold.local_body_contact=InverseTransformPoint(pose,contact);
        g_move_hold.local_hand_contact={}; g_move_hold.hinge_lightness=1.0F;
        runtime::VrMatrix44 palm{};
        if(!InteractionPalm(runtime::VrHand::right,palm)) return false;
        g_move_hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
        if(!TrackingGripPosition(runtime::VrHand::right,g_move_hold.previous_tracking)) return false;
        g_move_hold.previous_palm_pose=palm; g_move_hold.yaw_epoch=GameplayPalmYawEpoch();
        return true;
    };
    bool passed=true;
    // A rigid hinge requires zero velocity AT ITS PIVOT. These expectations
    // use v(point)=v(COM)+omega x (point-COM), independently of servo policy.
    for(const Vec com:{Vec{},Vec{0.2F,0.4F,0.1F}})
    for(const float body_yaw:{0.0F,1.57079632679F})
    for(const float hand_yaw:{0.0F,1.57079632679F,3.14159265359F})
    for(const float sign:{-1.0F,1.0F}) {
        auto* world=newton.NewtonCreate(nullptr,nullptr);
        auto* shape=newton.NewtonCreateBox(world,0.1F,0.4F,0.1F,nullptr);
        native_body=newton.NewtonCreateBody(world,shape);
        newton.NewtonBodySetMassMatrix(native_body,2,0.04F,0.04F,0.04F);
        newton.NewtonBodySetCentreOfMass(native_body,com.data());
        auto pose=runtime::IdentityMatrix();
        pose.values[0]=pose.values[10]=std::cos(body_yaw);
        pose.values[2]=std::sin(body_yaw); pose.values[8]=-std::sin(body_yaw);
        pose.values[3]=2; pose.values[7]=3; pose.values[11]=4;
        SetPose(pose);
        const Vec pivot{2,3,4},contact{2.4F,3,4};
        hand(contact,hand_yaw); passed=seed(pose,contact,{0,0,sign},pivot) && passed;
        HookedMoveUpdate(state.data(),nullptr,1.0F/90.0F);
        passed=passed && runtime::vr_mechanism_policy::Length(commanded_linear)<0.0001F &&
            runtime::vr_mechanism_policy::Length(commanded_angular)<0.0001F;
        hand({2.4F,2.98F,4},hand_yaw);
        HookedMoveUpdate(state.data(),nullptr,1.0F/90.0F);
        const Vec world_com=TransformPoint(pose,com);
        const auto pivot_velocity=runtime::vr_mechanism_policy::Add(commanded_linear,
            runtime::vr_mechanism_policy::Cross(commanded_angular,
                runtime::vr_mechanism_policy::Subtract(pivot,world_com)));
        if(runtime::vr_mechanism_policy::Length(pivot_velocity)>0.0001F ||
            commanded_angular[2]>=-0.01F || matrix_writes!=0) {
            std::cerr<<"Move hinge drives its fixed pivot: COM="<<com[0]<<','<<com[1]
                <<" body yaw="<<body_yaw<<" hand yaw="<<hand_yaw
                <<" residual="<<pivot_velocity[0]<<','<<pivot_velocity[1]<<','<<pivot_velocity[2]<<'\n';
            passed=false;
        }
        newton.NewtonReleaseCollision(world,shape); newton.NewtonDestroy(world);
    }
    // Repeated real solver/tracking updates with the offset collider profile
    // which exposed BP's regression. Preserve pivot and physical stops while
    // following the hand arc past the native 110 degree activation threshold.
    for(const Vec com:{Vec{},Vec{0.156F,0.416F,0}}) {
        auto* world=newton.NewtonCreate(nullptr,nullptr);
        auto* shape=newton.NewtonCreateBox(world,0.08F,0.4F,0.06F,nullptr);
        native_body=newton.NewtonCreateBody(world,shape);
        newton.NewtonBodySetMassMatrix(native_body,2,0.04F,0.04F,0.04F);
        newton.NewtonBodySetCentreOfMass(native_body,com.data());
        newton.NewtonBodySetAutoFreeze(native_body,0);
        const Vec pivot{0.093F,0.151F,0},pin{0,0,1},contact{0.16F,0.55F,0};
        auto* hinge=newton.NewtonConstraintCreateHinge(world,pivot.data(),pin.data(),native_body,nullptr);
        newton.NewtonHingeSetUserCallback(hinge,[](const NewtonJoint* joint,NewtonHingeSliderUpdateDesc* desc)->unsigned {
            const auto angle=api->NewtonHingeGetJointAngle(joint);
            if(angle>=-0.0174533F && angle<=2.268928F) return 0;
            desc->m_accel=api->NewtonHingeCalculateStopAlpha(joint,desc,std::clamp(angle,-0.0174533F,2.268928F));
            return 1;
        });
        hand(contact,0); passed=seed(Pose(),contact,pin,pivot) && passed;
        bool activated=false; float max_error=0;
        for(unsigned tick=0;tick<540;++tick) {
            const float angle=std::min(tick/90.0F*1.2F,2.1F);
            const float x=contact[0]-pivot[0],y=contact[1]-pivot[1];
            hand({pivot[0]+x*std::cos(angle)-y*std::sin(angle),
                  pivot[1]+x*std::sin(angle)+y*std::cos(angle),0},0);
            Put(body.data(),0x34,Pose()); HookedMoveUpdate(state.data(),nullptr,1.0F/90.0F);
            newton.NewtonUpdate(world,1.0F/90.0F);
            const auto retained=TransformPoint(Pose(),pivot);
            const float error=std::hypot(retained[0]-pivot[0],retained[1]-pivot[1],retained[2]-pivot[2]);
            max_error=std::max(max_error,error);
            const auto actual=newton.NewtonHingeGetJointAngle(hinge);
            activated=activated || actual>=1.919862F;
            passed=passed && std::isfinite(error) && actual>=-0.04F && actual<=2.30F;
        }
        if(max_error>0.01F || !activated) {
            std::cerr<<"Move/Newton offset-COM regression: max pivot error="<<max_error
                <<" activated="<<activated<<'\n'; passed=false;
        }
        HookedMoveLeave(state.data(),nullptr,nullptr);
        passed=passed && g_move_hold.state==nullptr;
        newton.NewtonReleaseCollision(world,shape); newton.NewtonDestroy(world);
    }
    passed=passed && matrix_writes==0 && leaves==2;
    g_move_hold={}; g_presenting.store(false); native_body=nullptr; api=nullptr;
    return passed;
}
} // namespace penumbra_vr::backends::requiem::native_mechanism_fixture
