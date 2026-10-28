#pragma once
// Actual Grab update: a seven-body native Object graph, with typed native ABI.
namespace penumbra_vr::backends::requiem::articulated_grab_fixture {
using Vec=std::array<float,3>;
using Matrix=runtime::VrMatrix44;
inline std::array<std::array<std::uint8_t,0x500>,16> bodies{};
inline std::array<std::array<std::uint8_t,0x100>,16> joints{};
inline std::array<Vec,16> velocities{}, omegas{};
inline Vec point_torque{};
inline int edge_count=6;
inline unsigned writes=0, impulses=0, exits=0;
struct PhysicsApi : native_mechanism_fixture::NewtonApi {
#define PVR_GRAB_EXPORTS(X) X(NewtonAddBodyImpulse) X(NewtonConstraintCreateBall) \
    X(NewtonBodySetForce) X(NewtonBodyGetMassMatrix) X(NewtonBodySetForceAndTorqueCallback)
#define PVR_GRAB_LOAD(name) decltype(&::name) name=module ? reinterpret_cast<decltype(&::name)>(GetProcAddress(module,#name)) : nullptr;
    PVR_GRAB_EXPORTS(PVR_GRAB_LOAD)
#undef PVR_GRAB_LOAD
    bool Complete() const {
#define PVR_GRAB_READY(name) && name!=nullptr
        return Ready() PVR_GRAB_EXPORTS(PVR_GRAB_READY);
#undef PVR_GRAB_READY
    }
#undef PVR_GRAB_EXPORTS
};
inline PhysicsApi* physics=nullptr;
inline std::array<NewtonBody*,16> physical{};
inline int Index(void* b) { for(int i=0;i<16;++i) if(b==bodies[i].data()) return i; return -1; }
inline int __fastcall Count(void* b,void*) {
    int count=0;
    for(int i=0;i<edge_count;++i)
        if(ReadNative<void*>(joints[i].data(),0x2C)==b || ReadNative<void*>(joints[i].data(),0x30)==b) ++count;
    return count;
}
inline void* __fastcall Joint(void* b,void*,int n) {
    for(int i=0;i<edge_count;++i)
        if((ReadNative<void*>(joints[i].data(),0x2C)==b || ReadNative<void*>(joints[i].data(),0x30)==b) && n--==0)
            return joints[i].data();
    return nullptr;
}
struct Getters {
    Vec Linear() {
        const auto i=Index(this);
        if(physical[i]) physics->NewtonBodyGetVelocity(physical[i],velocities[i].data());
        return velocities[i];
    }
    Vec Angular() {
        const auto i=Index(this);
        if(physical[i]) physics->NewtonBodyGetOmega(physical[i],omegas[i].data());
        return omegas[i];
    }
};
template<class T> void* Entry(T p) { void* out=nullptr; static_assert(sizeof(p)==sizeof(out)); std::memcpy(&out,&p,sizeof(out)); return out; }
inline void __fastcall Linear(void* b,void*,const Vec* v) {
    const auto i=Index(b); velocities[i]=*v;
    if(physical[i]) physics->NewtonBodySetVelocity(physical[i],v->data());
}
inline void __fastcall Angular(void* b,void*,const Vec* v) {
    const auto i=Index(b); omegas[i]=*v;
    if(physical[i]) physics->NewtonBodySetOmega(physical[i],v->data());
}
inline void __fastcall Impulse(void* b,void*,const Vec* v,const Vec* point) {
    ++impulses; const auto i=Index(b);
    if(physical[i]) physics->NewtonAddBodyImpulse(physical[i],v->data(),point->data());
    else for(int a=0;a<3;++a) { velocities[i][a]+=(*v)[a]; omegas[i][a]+=point_torque[a]; }
}
inline void __fastcall MatrixWrite(void*,void*,const Matrix*) { ++writes; }
inline void __fastcall Awake(void*,void*,bool) {}
inline void __fastcall Limit(void*,void*,float) {}
inline void __fastcall Exit(void*,void*) { ++exits; }
inline bool SeededCases() {
    JumpTo(kGetJointCountRva,reinterpret_cast<void*>(&Count));
    JumpTo(kGetBodyJointRva,reinterpret_cast<void*>(&Joint));
    JumpTo(0x19CE30,Entry(&Getters::Linear)); JumpTo(0x19CE80,Entry(&Getters::Angular));
    JumpTo(0x19CA00,reinterpret_cast<void*>(&Linear)); JumpTo(0x19CA20,reinterpret_cast<void*>(&Angular));
    JumpTo(0x19CB30,reinterpret_cast<void*>(&Impulse));
    JumpTo(kEntitySetMatrixRva,reinterpret_cast<void*>(&MatrixWrite));
    for(auto rva:{0x19CCF0,0x19CB50,0x19CBB0}) JumpTo(rva,reinterpret_cast<void*>(&Awake));
    for(auto rva:{0x19CAC0,0x19CAE0}) JumpTo(rva,reinterpret_cast<void*>(&Limit));
    JumpTo(kGrabExitRva,reinterpret_cast<void*>(&Exit));
    std::array<std::uint8_t,0x300> entity{};
    std::array<std::uint8_t,0x500> player{},character{};
    std::array<std::uint8_t,0x200> state{};
    std::array<void*,8> parts{}; std::array<void*,7> states{};
    Put(entity.data(),0x14,true); Put(entity.data(),0xC0,1);
    for(int i=0;i<8;++i) {
        parts[i]=bodies[i].data(); Put(parts[i],0,g_image+kPhysicsBodyVtableRva);
        Put(parts[i],0x414,static_cast<void*>(entity.data())); Put(parts[i],0x434,0.3F);
        Put(parts[i],0x34,runtime::IdentityMatrix());
    }
    Put(entity.data(),0x14C,parts.data()); Put(entity.data(),0x150,parts.data()+8);
    for(int i=0;i<6;++i) { Put(joints[i].data(),0x2C,parts[i]); Put(joints[i].data(),0x30,parts[i+1]); }
    Put(player.data(),GameplayContract().character_body_offset,static_cast<void*>(character.data()));
    Put(player.data(),0x2C0,6); states[6]=state.data(); Put(player.data(),0x2C8,states.data());
    Put(state.data(),0x10,static_cast<void*>(player.data())); Put(state.data(),0x20,parts[0]);
    test_frame={}; test_frame.focused=true; test_frame.input.state.interact.pressed=true;
    test_frame.interact_source=runtime::VrHand::right;
    auto& grip=test_frame.hands[1].grip; grip.pose_valid=grip.device_connected=true;
    std::copy_n(runtime::IdentityMatrix().values.begin(),12,grip.device_to_absolute.values.begin());
    g_presenting.store(true); g_world_from_tracking=runtime::IdentityMatrix();
    g_world_from_tracking_time=GetTickCount64(); g_world_from_tracking_yaw_epoch=g_world_yaw_epoch.load();
    Matrix palm{}; if(!InteractionPalm(runtime::VrHand::right,palm)) return false;
    g_grab_hold={}; auto& h=g_grab_hold; h.state=state.data(); h.player=player.data(); h.body=parts[0];
    h.articulated=true; h.hand=runtime::VrHand::right; h.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    h.previous_tracking={}; h.yaw_epoch=GameplayPalmYawEpoch();
    std::string error; if(!h.pose.Begin(palm,runtime::IdentityMatrix(),{},false,error)) return false;
    // Acquisition must preserve separation; a moving palm must drive the graph
    // without SetMatrix, while a disconnected dynamic sibling stays untouched.
    bool ok=true;
    for(int selected:{0,3,6}) {
        GrabComponent graph{};
        if(!InspectGrabAssembly(parts[selected],true,nullptr,&graph) || graph.count!=7) return false;
        h.body=parts[selected]; Put(state.data(),0x20,parts[selected]);
        velocities={}; writes=impulses=exits=0;
        grip.device_to_absolute.values[7]=0;
        if(!InteractionPalm(runtime::VrHand::right,palm)) return false;
        h.previous_tracking={}; h.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
        if(!h.pose.Begin(palm,runtime::IdentityMatrix(),{},false,error)) return false;
        h.previous_tracking_pose=grip.device_to_absolute;
        HookedGrabUpdate(state.data(),nullptr,1.0F/90.0F);
        ok=ok && velocities[0]==Vec{} && velocities[6]==Vec{} && writes==0;
        impulses=0;
        grip.device_to_absolute.values[7]+=0.1F;
        // Simultaneous smooth-turn epoch change must retain physical lifting.
        g_world_from_tracking_yaw_epoch=g_world_yaw_epoch.fetch_add(1)+1;
        HookedGrabUpdate(state.data(),nullptr,1.0F/90.0F);
        ok=ok && writes==0 && impulses==1 && velocities[0][1]>0.1F &&
            velocities[6][1]>0.1F && velocities[7]==Vec{} && exits==0;
        HookedGrabUpdate(state.data(),nullptr,0.2F);
        ok=ok && exits==0 && impulses==1;
        const auto before=velocities;
        RestoreGrab(h);
        ok=ok && before==velocities;
    }
    // A native attached endpoint becomes static; drive only its dynamic chain.
    Put(parts[6],0x434,0.0F);
    GrabComponent anchored{};
    ok=ok && InspectGrabAssembly(parts[0],true,nullptr,&anchored) && anchored.count==6 && anchored.anchored;
    ok=ok && !InspectGrabAssembly(parts[6],true,nullptr,&anchored);
    for(int fixed:{0,6}) {
        Put(parts[0],0x434,fixed==0 ? 0.0F : 1.0F);
        Put(parts[6],0x434,fixed==6 ? 0.0F : 1.0F);
        h.body=parts[fixed==0 ? 6 : 0]; h.local_contact={}; velocities={};
        auto destination=runtime::IdentityMatrix(); destination.values[7]=0.1F;
        const auto before_writes=writes;
        ok=ok && DriveGrabAssembly(h,destination,1.0F/90.0F) &&
            velocities[fixed]==Vec{} && velocities[3][1]>0.1F && writes==before_writes;
    }
    // Foreign entities and inconsistent links must never receive writes.
    Put(joints[0].data(),0x2C,static_cast<void*>(character.data()));
    ok=ok && !InspectGrabAssembly(parts[0],true,nullptr,&anchored);
    if(!ok) std::cerr<<"Jointed Grab: matrices="<<writes<<" impulses="<<impulses<<" endY="<<velocities[6][1]<<'\n';
    g_grab_hold={}; g_presenting.store(false); return ok;
}

// Exercise the production Enter -> pending -> fresh resolver -> Update -> Leave
// path. Native calls below model the independently decoded ACE40/AA920 ABI;
// acquisition, graph inspection, palm generations and drive are production code.
inline std::array<std::uint8_t,0x300> object{}, native_state{};
inline std::array<std::uint8_t,0x30> normal_state{};
inline std::array<std::uint8_t,0x500> player{}, character{}, character_physics{};
inline std::array<std::uint8_t,0x100> world{};
inline std::array<std::array<std::uint8_t,0x100>,2> shapes{};
inline std::array<void*,16> parts{};
inline std::array<void*,7> states{};
inline unsigned enters=0, leaves=0, detaches=0, shape_count=0, queries=0;
inline bool stuck=false, can_detach=true, leave_saw_cleared=false;
inline void* selected=nullptr;
inline void* last_skip=nullptr;
inline float detached_mass=1;
inline void __fastcall Enter(void* state,void*,void*) {
    ++enters;
    if(stuck && !can_detach) { Put(player.data(),0x2C0,1); return; }
    if(stuck) { ++detaches; stuck=false; Put(selected,0x434,detached_mass); }
    Put(state,0x20,selected);
    Put(state,0xD4,ReadNative<bool>(selected,0x428));
    Put(state,0xE0,ReadNative<bool>(selected,0x3C4));
    Put(state,0xDC,ReadNative<float>(selected,0x434));
    Put(selected,0x434,ReadNative<float>(selected,0x434)/5.0F);
}
inline void __fastcall Leave(void* state,void*,void*) {
    ++leaves; leave_saw_cleared=g_grab_hold.state==nullptr;
    void* body=ReadNative<void*>(state,0x20);
    Put(body,0x428,ReadNative<bool>(state,0xD4));
    Put(body,0x3C4,ReadNative<bool>(state,0xE0));
    Put(body,0x434,ReadNative<float>(state,0xDC));
}
inline void __fastcall LifecycleExit(void* state,void*) {
    ++exits; HookedGrabLeave(state,nullptr,nullptr); Put(player.data(),0x2C0,1);
}
inline void __fastcall LinearLimit(void* body,void*,float value) { Put(body,0x42C,value); }
inline void __fastcall AngularLimit(void* body,void*,float value) { Put(body,0x430,value); }
inline void* __fastcall CreateShape(void* owner,void*,const Vec*,Matrix*) {
    if(shape_count>=shapes.size()) return nullptr;
    auto* shape=shapes[shape_count++].data();
    Put(shape,0,g_image+0x293F10); Put(shape,0x10,1); Put(shape,0x58,owner);
    return shape;
}
inline void __fastcall DestroyShape(void*,void*,void*) {}
inline void __fastcall NormalTransition(void*,void*,void*) {}
inline void ChangeState(int state) {
    reinterpret_cast<void(__thiscall*)(void*,int)>(g_image+0x9CEE0)(player.data(),state);
}
inline bool __fastcall ClearQuery(void*,void*,Vec*,void*,const Matrix*,
    void* skip,bool,bool,void*,bool,bool) {
    ++queries; last_skip=skip; return false;
}
inline void Tracking(float height=0,float yaw=0,float x=0) {
    auto raw=runtime::IdentityMatrix();
    raw.values[0]=raw.values[10]=std::cos(yaw);
    raw.values[2]=std::sin(yaw); raw.values[8]=-std::sin(yaw); raw.values[7]=height; raw.values[3]=x;
    std::copy_n(raw.values.begin(),12,test_frame.hands[1].grip.device_to_absolute.values.begin());
    g_world_from_tracking_time=GetTickCount64();
    PublishGameplayPalmTracking({raw,raw},{false,true},runtime::IdentityMatrix(),true,
        g_world_from_tracking_yaw_epoch);
}
inline void Configure(int count,bool companions=false,bool rooted=false) {
    bodies={}; joints={}; velocities={}; omegas={}; point_torque={}; object={}; native_state={};
    parts={}; states={}; enters=leaves=detaches=writes=impulses=exits=0;
    stuck=false; can_detach=true; leave_saw_cleared=false;
    edge_count=count-1;
    Put(object.data(),0x14,true); Put(object.data(),0xC0,1);
    const int total=count+(companions ? count : 0)+1;
    for(int i=0;i<total;++i) {
        parts[i]=bodies[i].data(); Put(parts[i],0,g_image+kPhysicsBodyVtableRva);
        Put(parts[i],0x414,static_cast<void*>(object.data()));
        Put(parts[i],0x434,companions && i>=count && i<2*count ? 0.0F : 1.0F);
        Put(parts[i],0x3C8,i<count || i==total-1);
        Put(parts[i],0x42C,8.0F); Put(parts[i],0x430,10.0F); Put(parts[i],0x428,true);
        Put(parts[i],0x34,runtime::IdentityMatrix());
    }
    Put(object.data(),0x14C,parts.data()); Put(object.data(),0x150,parts.data()+total);
    for(int i=0;i<count-1;++i) {
        Put(joints[i].data(),0x2C,parts[i]); Put(joints[i].data(),0x30,parts[i+1]);
    }
    if(rooted) {
        Put(joints[edge_count].data(),0x2C,static_cast<void*>(nullptr));
        Put(joints[edge_count++].data(),0x30,parts[0]);
    }
    Put(player.data(),GameplayContract().character_body_offset,static_cast<void*>(character.data()));
    Put(player.data(),0x2C0,0); states[0]=normal_state.data(); states[6]=native_state.data();
    Put(normal_state.data(),0,g_image+0x272000); Put(native_state.data(),0,g_image+0x272100);
    Put(native_state.data(),4,6); Put(player.data(),0x2C8,states.data());
    Put(native_state.data(),0x10,static_cast<void*>(player.data()));
    g_grab_hold={}; g_pending_grab_state=nullptr; g_vr_selection_contact={};
    PublishGameplayPalmHeldBody(1,nullptr);
    test_frame={}; test_frame.focused=true;
    test_frame.input.state.interact.pressed=test_frame.input.state.interact.just_pressed=true;
    test_frame.interact_source=runtime::VrHand::right;
    test_frame.hands[1].grip.pose_valid=test_frame.hands[1].grip.device_connected=true;
    g_presenting.store(true); g_world_from_tracking=runtime::IdentityMatrix();
    g_world_from_tracking_yaw_epoch=g_world_yaw_epoch.load();
    Tracking();
}
inline bool Acquire(int index) {
    selected=parts[index]; detached_mass=ReadNative<float>(selected,0x434);
    Put(player.data(),0x240,selected);
    ServiceGameplayPalmResolver(g_image,character.data());
    const auto before=GameplayPalmPoseGeneration(1);
    ChangeState(6);
    if(g_pending_grab_state!=native_state.data() || enters!=1) return false;
    test_frame.input.state.interact.just_pressed=false;
    HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    const bool ok=g_grab_hold.state==native_state.data() &&
        g_pending_grab_state==nullptr && GameplayPalmPoseGeneration(1)>before &&
        last_skip==selected && queries>0 && exits==0;
    if(!ok) std::cerr<<"Fresh native Grab acquisition failed: before="<<before
        <<" after="<<GameplayPalmPoseGeneration(1)<<" exits="<<exits<<'\n';
    return ok;
}
inline bool NewtonCases() {
    PhysicsApi api;
    if(!api.Complete()) { std::cerr<<"Articulated Newton exports unavailable\n"; return false; }
    physics=&api;
    bool ok=true;
    // Real gravity-bearing solver components using native-like selected mass
    // reduction. This checks the adapter route, not a second copy of its servo.
    for(int count:{2,6,7,1}) for(int index=0;index<count;++index) {
        const bool rooted=count==1;
        Configure(count,count==6,rooted);
        for(int i=0;i<count;++i) {
            auto pose=runtime::IdentityMatrix(); pose.values[3]=i*0.12F;
            Put(parts[i],0x34,pose);
            if(count==2) Put(parts[i],0x434,i==0 ? 3.0F : 2.2F);
        }
        if(!Acquire(index)) { physics=nullptr; return false; }
        auto* scene=api.NewtonCreate(nullptr,nullptr);
        auto* shape=api.NewtonCreateBox(scene,0.06F,0.06F,0.06F,nullptr);
        for(int i=0;i<count;++i) {
            physical[i]=api.NewtonCreateBody(scene,shape);
            const float mass=ReadNative<float>(parts[i],0x434);
            api.NewtonBodySetMassMatrix(physical[i],mass,mass*0.01F,mass*0.01F,mass*0.01F);
            const auto pose=ReadNative<Matrix>(parts[i],0x34);
            std::array<float,16> native{};
            for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                native[col*4+row]=pose.values[row*4+col];
            api.NewtonBodySetMatrix(physical[i],native.data());
            api.NewtonBodySetAutoFreeze(physical[i],0);
            if(i!=index) api.NewtonBodySetForceAndTorqueCallback(physical[i],[](const NewtonBody* body) {
                float mass=0,ix=0,iy=0,iz=0;
                physics->NewtonBodyGetMassMatrix(body,&mass,&ix,&iy,&iz);
                const Vec gravity{0,-9.81F*mass,0}; physics->NewtonBodySetForce(body,gravity.data());
            });
        }
        for(int i=1;i<count;++i) {
            const Vec pivot{(i-0.5F)*0.12F,0,0};
            if(count==2) {
                const Vec pin{0,0,1};
                api.NewtonConstraintCreateHinge(scene,pivot.data(),pin.data(),physical[i],physical[i-1]);
            } else api.NewtonConstraintCreateBall(scene,pivot.data(),physical[i],physical[i-1]);
        }
        if(rooted) {
            const Vec pivot{0.12F,0,0};
            api.NewtonConstraintCreateBall(scene,pivot.data(),physical[0],nullptr);
        }
        float maximum_separation=0,maximum_root_error=0;
        for(int tick=0;tick<180;++tick) {
            if(rooted) {
                const float angle=std::min(0.6F,tick*0.004F);
                Tracking(0.12F*std::sin(angle),0,0.12F*(1-std::cos(angle)));
            } else Tracking(std::min(0.2F,tick*0.002F),std::min(0.4F,tick*0.004F));
            HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
            api.NewtonUpdate(scene,1.0F/90.0F);
            for(int i=0;i<count;++i) {
                std::array<float,16> native{}; Matrix pose{};
                api.NewtonBodyGetMatrix(physical[i],native.data());
                for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                    pose.values[row*4+col]=native[col*4+row];
                Put(parts[i],0x34,pose);
                ok=ok && ValidGrabMatrix(pose);
            }
            for(int i=1;i<count;++i) {
                const auto a=TransformPoint(ReadNative<Matrix>(parts[i-1],0x34),{0.06F,0,0});
                const auto b=TransformPoint(ReadNative<Matrix>(parts[i],0x34),{-0.06F,0,0});
                maximum_separation=std::max(maximum_separation,std::hypot(a[0]-b[0],a[1]-b[1],a[2]-b[2]));
            }
            if(rooted) {
                const auto pivot=TransformPoint(ReadNative<Matrix>(parts[0],0x34),{0.12F,0,0});
                maximum_root_error=std::max(maximum_root_error,std::hypot(pivot[0]-0.12F,pivot[1],pivot[2]));
            }
        }
        const float selected_height=ReadNative<Matrix>(parts[index],0x34).values[7];
        const bool passed=exits==0 && writes==0 && impulses>=180 && maximum_separation<0.02F &&
            (rooted ? maximum_root_error<0.01F && selected_height>0.03F : std::abs(selected_height-0.2F)<0.08F);
        if(!passed) std::cerr<<"Actual Grab/Newton: count="<<count<<" selected="<<index
            <<" selectedY="<<selected_height<<" joint error="<<maximum_separation
            <<" root error="<<maximum_root_error<<" exits="<<exits<<'\n';
        ok=ok && passed;
        HookedGrabLeave(native_state.data(),nullptr,nullptr);
        physical={}; api.NewtonReleaseCollision(scene,shape); api.NewtonDestroy(scene);
    }
    physics=nullptr; return ok;
}
inline bool LifecycleCases() {
    // Exact Requiem 9CEE0..9CF26: Leave -> Enter -> state commit. Only virtual
    // calls and local branches; safe to execute in the fixture image.
    constexpr unsigned char change_state[]={
        0x53,0x8b,0xd9,0x8b,0x8b,0xc0,0x02,0,0,0x55,0x8b,0x6c,0x24,0x0c,
        0x3b,0xe9,0x74,0x30,0x8b,0x83,0xc8,0x02,0,0,0x56,0x8b,0x34,0x88,
        0x85,0xf6,0x57,0x8b,0x3c,0xa8,0x74,0x08,0x8b,0x06,0x57,0x8b,0xce,
        0xff,0x50,0x60,0x8b,0x17,0x56,0x8b,0xcf,0xff,0x52,0x5c,0x8b,0x46,
        0x04,0x89,0x47,0x08,0x5f,0x89,0xab,0xc0,0x02,0,0,0x5e,0x5d,0x5b,0xc2,0x04,0};
    std::memcpy(g_image+0x9CEE0,change_state,sizeof(change_state));
    constexpr unsigned char set_active[]={
        0x8a,0x44,0x24,0x04,0x84,0xc0,0x53,0x8b,0xd9,0x88,0x83,0xdc,0x01,0,0,
        0x75,0x3d,0x8b,0x8b,0xc0,0x02,0,0,0x85,0xc9,0x74,0x33,0x8b,0x83,0xc8,0x02,0,0,
        0x56,0x8b,0x34,0x88,0x85,0xf6,0x57,0x8b,0x38,0x74,0x08,0x8b,0x06,0x57,
        0x8b,0xce,0xff,0x50,0x60,0x8b,0x17,0x56,0x8b,0xcf,0xff,0x52,0x5c,0x8b,0x46,
        0x04,0x89,0x47,0x08,0x5f,0xc7,0x83,0xc0,0x02,0,0,0,0,0,0,0x5e,0x5b,0xc2,0x04,0};
    std::memcpy(g_image+0x9E3D0,set_active,sizeof(set_active));
    Put(g_image+0x272000,0x5C,reinterpret_cast<void*>(&NormalTransition));
    Put(g_image+0x272000,0x60,reinterpret_cast<void*>(&NormalTransition));
    Put(g_image+0x272100,0x5C,reinterpret_cast<void*>(&HookedGrabEnter));
    Put(g_image+0x272100,0x60,reinterpret_cast<void*>(&HookedGrabLeave));
    JumpTo(kGrabEnterRva,reinterpret_cast<void*>(&Enter));
    JumpTo(kGrabLeaveRva,reinterpret_cast<void*>(&Leave));
    JumpTo(kGrabExitRva,reinterpret_cast<void*>(&LifecycleExit));
    JumpTo(0x19CAC0,reinterpret_cast<void*>(&LinearLimit));
    JumpTo(0x19CAE0,reinterpret_cast<void*>(&AngularLimit));
    JumpTo(0x18B3B0,reinterpret_cast<void*>(&CreateShape));
    JumpTo(0xD4B70,reinterpret_cast<void*>(&DestroyShape));
    JumpTo(0xD5190,reinterpret_cast<void*>(&ClearQuery));
    Put(world.data(),0,g_image+0x292D50); Put(g_image+0x292D50,0x30,g_image+0x18B3B0);
    Put(character_physics.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(character.data(),0x23C,static_cast<void*>(character_physics.data()));
    Put(character.data(),0x240,static_cast<void*>(world.data()));
    bool ok=true;
    const auto check=[&](bool condition,const char* name) {
        if(!condition) { std::cerr<<"Grab lifecycle: "<<name<<'\n'; ok=false; }
    };
    // Seven-body cable/rope, two-body bucket and six-body mattress/pillow.
    // Each selected member must acquire through a fresh held-body exclusion.
    for(int count:{7,2,6}) for(int index=0;index<count;++index) {
        Configure(count,count==6);
        if(!Acquire(index)) return false;
        check(writes==0 && velocities[index]==Vec{},"stationary acquisition snaps/teleports");
        Tracking(0.1F);
        HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
        for(int i=0;i<count;++i) check(velocities[i][1]>0.1F,"connected member did not lift");
        check(velocities[count+(count==6 ? count : 0)]==Vec{},"disconnected body was driven");
        if(count==6) for(int i=6;i<12;++i) check(velocities[i]==Vec{},"animation companion was driven");
        const auto before=velocities;
        HookedGrabLeave(native_state.data(),nullptr,nullptr);
        check(leaves==1 && leave_saw_cleared && g_grab_hold.state==nullptr &&
            before==velocities && ReadNative<float>(selected,0x434)==1.0F &&
            ReadNative<float>(selected,0x42C)==8.0F && ReadNative<float>(selected,0x430)==10.0F &&
            ReadNative<bool>(selected,0x3C8),"native Leave/restore changed graph momentum or settings");
    }
    // A native stick endpoint can become static while the opposite end is held.
    for(int fixed:{0,6}) {
        Configure(7); if(!Acquire(fixed==0 ? 6 : 0)) return false;
        Put(parts[fixed],0x434,0.0F); stuck=true;
        Tracking(0.1F); HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
        check(velocities[fixed]==Vec{} && velocities[3][1]>0.1F && writes==0 && exits==0,
            "native attached endpoint received a correction");
        Put(parts[fixed],0x434,1.0F); stuck=false;
        Tracking(0.2F); HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
        check(velocities[fixed][1]>0.1F && writes==0,"detached endpoint not returned to solver component");
        HookedGrabLeave(native_state.data(),nullptr,nullptr);
    }
    // World-rooted lamp: a single dynamic body must still retain its constraint.
    Configure(1,false,true); if(!Acquire(0)) return false;
    Tracking(0.1F,0.5F); HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    check(writes==0 && velocities[0][1]>0.1F && omegas[0]==Vec{},"rooted lamp wrist/matrix override");
    HookedGrabLeave(native_state.data(),nullptr,nullptr);
    // Native detach refusal must not leave a phantom pending VR acquisition.
    Configure(7); selected=parts[0]; stuck=true; can_detach=false;
    Put(player.data(),0x240,selected); ChangeState(6);
    check(g_pending_grab_state==nullptr && g_grab_hold.state==nullptr && detaches==0,
        "native refusal left a pending hold");
    // An allowed detach remains native-owned and then acquires normally.
    Configure(7); stuck=true; if(!Acquire(0)) return false;
    check(detaches==1 && !stuck,"native detach callback was bypassed");
    HookedGrabLeave(native_state.data(),nullptr,nullptr);
    // Revalidate live topology before the rigid branch can call SetMatrix.
    Configure(1); if(!Acquire(0)) return false;
    writes=0; edge_count=1;
    Put(joints[0].data(),0x2C,static_cast<void*>(nullptr)); Put(joints[0].data(),0x30,parts[0]);
    Tracking(0.1F); HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    check(writes==0 && exits==1,"new world joint reached rigid SetMatrix");
    if(g_grab_hold.state) HookedGrabLeave(native_state.data(),nullptr,nullptr);
    Configure(1); if(!Acquire(0)) return false;
    writes=0; Put(parts[0],0x434,0.0F); test_frame.hands[1].grip.velocity={0,2,0};
    HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    check(writes==0 && exits==1 && velocities[0]==Vec{},"static selected body received rigid drive/throw");
    if(g_grab_hold.state) HookedGrabLeave(native_state.data(),nullptr,nullptr);
    // Every matrix in the connected solve must be valid before any impulse.
    Configure(7); if(!Acquire(0)) return false;
    const auto previous_impulses=impulses;
    auto invalid=runtime::IdentityMatrix(); invalid.values[0]=std::numeric_limits<float>::quiet_NaN();
    Put(parts[3],0x34,invalid); Tracking(0.1F);
    HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    check(impulses==previous_impulses && writes==0 && exits==1,"invalid sibling matrix reached Newton drive");
    if(g_grab_hold.state) HookedGrabLeave(native_state.data(),nullptr,nullptr);
    // Closed graphs consume BP's wrist correction and tracked translation;
    // roots intentionally omit angular correction.
    Configure(2); if(!Acquire(0)) return false;
    Tracking(0,0.5F); HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    check(omegas[0][1]>0.01F && writes==0,"closed graph omitted BP wrist drive");
    HookedGrabLeave(native_state.data(),nullptr,nullptr);
    for(bool rooted:{false,true}) {
        Configure(2,false,rooted); if(!Acquire(0)) return false;
        point_torque={0,0,0.6F};
        HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
        check(omegas[0][2]>0.4F && (rooted ? omegas[0][2]==0.6F : omegas[0][2]<0.6F),
            "wrist correction erased native point torque or overrode rooted solve");
        HookedGrabLeave(native_state.data(),nullptr,nullptr);
    }
    Configure(7); if(!Acquire(0)) return false;
    test_frame.hands[1].grip.velocity={0,1,0}; Tracking();
    HookedGrabUpdate(native_state.data(),nullptr,1.0F/90.0F);
    check(velocities[0][1]>0.5F && velocities[6][1]>0.5F,"tracked translation feedforward omitted");
    HookedGrabLeave(native_state.data(),nullptr,nullptr);
    // Native SetActive(false) owns cancellation, even with focused/pressed VR.
    for(bool pending:{false,true}) {
        Configure(7);
        if(pending) {
            selected=parts[0]; Put(player.data(),0x240,selected); ChangeState(6);
        } else if(!Acquire(0)) return false;
        reinterpret_cast<void(__thiscall*)(void*,bool)>(g_image+0x9E3D0)(player.data(),false);
        check(leaves==1 && g_pending_grab_state==nullptr && g_grab_hold.state==nullptr &&
            ReadNative<int>(player.data(),0x2C0)==0 && !ReadNative<bool>(player.data(),0x1DC),
            "native script deactivation failed to cancel pending/held Grab");
    }
    check(NewtonCases(),"real Newton component regression");
    std::string error; check(ShutdownGameplayPalmResolver(error),"palm fixture teardown failed");
    g_presenting.store(false); return ok;
}
inline bool Cases() { return SeededCases() && LifecycleCases(); }
}
