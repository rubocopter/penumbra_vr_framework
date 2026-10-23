// Exercise the actual exact-build adapter against a synthetic image. Native
// calls jump to typed fakes; this does not load or modify a game process.
#include "../../src/backends/black_plague/spatial_interaction.cpp"
#include "Newton.h"
#include <iostream>
#include <vector>
namespace penumbra_vr::backends::black_plague {
namespace {
runtime::VrControllerFrame test_frame;
Matrix test_controller_world_basis=runtime::IdentityMatrix();
int test_joints=0, test_leaves=0, test_native_updates=0;
int test_move_leaves=0, test_native_move_updates=0;
int test_push_leaves=0, test_native_push_updates=0;
int test_native_push_release_impulses=0;
void* test_joint=nullptr;
void* test_joint_secondary=nullptr;
void* test_unjointed_companion=nullptr;
void* test_joint_count_fault_body=nullptr;
std::vector<std::pair<void*,std::vector<void*>>> test_joint_graph;
int test_active_calls=0, test_auto_freeze_calls=0;
bool test_active_value=false, test_auto_freeze_value=true;
Vec test_move_force{},test_move_force_position{};
int test_matrix_writes=0, test_contact_impulses=0;
void* test_matrix_body=nullptr;
void* test_impulse_body=nullptr;
Vec test_impulse_delta{},test_impulse_position{};
Vec test_impulse_added_angular{};
Vec test_push_force{};
bool test_ui=false;
std::array<void*,2> test_palm_held{};
bool test_resolved_palm_valid=false;
runtime::VrMatrix44 test_resolved_palm{};
std::uint64_t test_resolved_palm_generation=0;
std::uint64_t test_palm_yaw_epoch=1;
int test_palm_resolver_services=0;
void* test_palm_resolver_character_body=nullptr;
bool test_palm_resolver_saw_held_body=false;
bool test_palm_resolver_publish_on_service=false;
bool test_palm_overlap_available=false;
GameplayPalmOverlapResult test_palm_overlap{};
bool test_head_pose_valid=true;
runtime::VrMatrix44 test_head_pose=runtime::IdentityMatrix();
GameplayInteractionTargetProvider test_interaction_target_provider=nullptr;
bool MatrixNearlyEqual(const Matrix& left,const Matrix& right,float epsilon=0.00001F) {
    for (std::size_t i=0;i<left.values.size();++i) {
        if (std::abs(left.values[i]-right.values[i])>=epsilon) return false;
    }
    return true;
}
bool VecNearlyEqual(const Vec& left,const Vec& right,float epsilon=0.00001F) {
    for (std::size_t i=0;i<left.size();++i) {
        if (std::abs(left[i]-right[i])>=epsilon) return false;
    }
    return true;
}
bool __cdecl NativeEqual(const void* value,const char* expected) {
    return std::strcmp(Read<const char*>(value,0),expected)==0;
}
template<class T> void Put(void* object, std::size_t offset, const T& value) {
    std::memcpy(static_cast<std::uint8_t*>(object)+offset,&value,sizeof(value));
}
void __fastcall NativeEnter(void* state, void*, void*) {
    auto* body=Read<void*>(state,0x20);
    Put(body,0x434,2.0F); Put(body,0x428,false);
}
void __fastcall NativeLeave(void* state, void*, void*) {
    ++test_leaves;
    auto* body=Read<void*>(state,0x20);
    Put(body,0x434,10.0F); Put(body,0x428,true);
}
void __fastcall NativeUpdate(void*, void*, float) { ++test_native_updates; }
void __fastcall NativeStop(void* state, void*) {
    HookedLeave(state,nullptr,nullptr);
    Put(Read<void*>(state,0x10),0x2BC,0);
}
int __fastcall JointCount(void* body,void*) {
    if (body==test_joint_count_fault_body)
        RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    for (const auto& entry:test_joint_graph)
        if (entry.first==body) return static_cast<int>(entry.second.size());
    return body==test_unjointed_companion ? 0 : test_joints;
}
void* __fastcall BodyJoint(void* body,void*,int index) {
    for (const auto& entry:test_joint_graph)
        if (entry.first==body) return index>=0 &&
            static_cast<std::size_t>(index)<entry.second.size()
                ? entry.second[index] : nullptr;
    if (index==0) return test_joint;
    if (index==1) return test_joint_secondary;
    return nullptr;
}
int __fastcall HingeType(void*,void*) { return kHingeJointType; }
int __fastcall SliderType(void*,void*) { return kSliderJointType; }
void __fastcall NativeMoveEnter(void*,void*,void*) {}
void __fastcall NativeMoveLeave(void*,void*,void*) { ++test_move_leaves; }
void __fastcall NativeMoveUpdate(void*,void*,float) { ++test_native_move_updates; }
void __fastcall NativeMoveStop(void* state,void*) {
    HookedMoveLeave(state,nullptr,nullptr);
    Put(Read<void*>(state,0x10),0x2BC,0);
}
void __fastcall NativePushEnter(void*,void*,void*) {}
void __fastcall NativePushLeave(void*,void*,void*) { ++test_push_leaves; }
void __fastcall NativePushUpdate(void*,void*,float) { ++test_native_push_updates; }
void __fastcall NativePushStop(void* state,void*) {
    HookedPushLeave(state,nullptr,nullptr);
    auto* player=Read<void*>(state,0x10);
    Put(player,0x2BC,0);
    // Exact BP AB1F0 adds the camera-forward impulse AFTER ChangeState/Leave
    // whenever player+50 says CanBeThrown. Rework's VR release only changes state.
    if (Read<bool>(player,0x50)) {
        ++test_native_push_release_impulses;
        auto* body=Read<void*>(state,0x50);
        auto velocity=Read<Vec>(body,0x450);
        velocity[2]-=6;
        Put(body,0x450,velocity);
    }
}
void __fastcall NativeChangeActionState(void* player,void*,int next) {
    auto** states=Read<void**>(player,0x2C4);
    const int current=Read<int>(player,0x2BC);
    if (current==1) HookedPushLeave(states[current],nullptr,states[next]);
    Put(player,0x2BC,next);
}
void __fastcall MaxLinear(void* body,void*,float value) { Put(body,0x42C,value); }
void __fastcall MaxAngular(void* body,void*,float value) { Put(body,0x430,value); }
void __fastcall Linear(void* body,void*,const Vec* value) {
    Put(body,0x450,*value);
    if (const auto* native=Read<const NewtonBody*>(body,0x41C))
        NewtonBodySetVelocity(native,value->data());
}
void __fastcall Angular(void* body,void*,const Vec* value) {
    Put(body,0x460,*value);
    if (const auto* native=Read<const NewtonBody*>(body,0x41C))
        NewtonBodySetOmega(native,value->data());
}
// The actual BP getters return a 12-byte Vec by value under x86 __thiscall.
// Model that ABI using class members, including the hidden return pointer.
struct TestVelocityGetters {
    Vec GetLinear() const { return Read<Vec>(this,0x450); }
    Vec GetAngular() const { return Read<Vec>(this,0x460); }
    Vec GetMassCentre() const { return Read<Vec>(this,0x480); }
};
template<class Method> void* NativeMemberEntry(Method method) {
    static_assert(sizeof(method)==sizeof(void*));
    void* entry=nullptr;
    std::memcpy(&entry,&method,sizeof(entry));
    return entry;
}
void __fastcall Gravity(void* body,void*,bool value) { Put(body,0x428,value); }
void __fastcall Active(void*,void*,bool value) {
    ++test_active_calls;
    test_active_value=value;
}
void __fastcall AutoFreeze(void*,void*,bool value) {
    ++test_auto_freeze_calls;
    test_auto_freeze_value=value;
}
void __fastcall BodyMatrix(void* body,void*,const Matrix* value) {
    ++test_matrix_writes;
    test_matrix_body=body;
    Put(body,0x34,*value);
}
void __fastcall BodyImpulseAtPosition(void* body,void*,const Vec* delta,
    const Vec* position) {
    ++test_contact_impulses;
    test_impulse_body=body;
    test_impulse_delta=*delta;
    test_impulse_position=*position;
    const auto angular=Read<Vec>(body,0x460);
    Put(body,0x460,Vec{angular[0]+test_impulse_added_angular[0],
        angular[1]+test_impulse_added_angular[1],angular[2]+test_impulse_added_angular[2]});
}
void __fastcall BodyForceAtPosition(void*,void*,const Vec* force,const Vec* position) {
    test_move_force=*force;
    test_move_force_position=*position;
}
void __fastcall BodyForce(void*,void*,const Vec* force) { test_push_force=*force; }
void* test_pick_rejected_body=nullptr;
struct TestPickCallback {
    struct VTable {
        bool (__thiscall *before)(void*,void*);
        bool (__thiscall *intersect)(void*,void*,void*);
    };
    VTable* vtable=nullptr;
    void* body=nullptr;
    void* area=nullptr;
    float maximum_distance=10.0F;
    float distance=20.0F;
    float area_distance=20.0F;
    int last_entity_type=0x16;
    Vec point{};
    Vec area_point{};
    float minimum_static_distance=20.0F;
    static bool __fastcall Before(void*,void*,void* body) {
        return body!=test_pick_rejected_body;
    }
    static bool __fastcall Intersect(void* self,void*,void* candidate,void* params) {
        auto* pick=static_cast<TestPickCallback*>(self);
        struct Params { float t; float dist; Vec normal; Vec point; } hit{};
        std::memcpy(&hit,params,sizeof(hit));
        if (!std::isfinite(hit.dist) || hit.dist<0 ||
            hit.dist>pick->maximum_distance) return true;
        auto* entity=Read<void*>(candidate,kBodyUserDataOffset);
        const int type=entity ? Read<int>(entity,kEntityTypeOffset) : -1;
        if (type==9) return true;
        if (type==4 || type==0xE || type==0xF || type==0x15) {
            if (hit.dist<pick->area_distance) {
                pick->area=candidate; pick->area_distance=hit.dist;
                pick->area_point=hit.point;
            }
        } else if (hit.dist<pick->distance) {
            pick->body=entity ? candidate : nullptr;
            pick->distance=hit.dist; pick->point=hit.point;
        }
        return true;
    }
};
static_assert(sizeof(TestPickCallback)==0x38);
struct TestHeadHit { void* body; float distance; Vec point; };
std::vector<TestHeadHit> test_head_hits;
int test_head_rays=0;
Vec test_head_ray_from{},test_head_ray_to{};
void __fastcall NativeTestRay(void*,void*,void* callback,const Vec* from,
    const Vec* to,bool,bool,bool,bool) {
    const Vec delta{(*to)[0]-(*from)[0],(*to)[1]-(*from)[1],(*to)[2]-(*from)[2]};
    if (std::hypot(delta[0],delta[1],delta[2])<1.0F) return;
    ++test_head_rays; test_head_ray_from=*from; test_head_ray_to=*to;
    auto** table=*reinterpret_cast<void***>(callback);
    for (const auto& hit : test_head_hits) {
        using Before=bool(__thiscall*)(void*,void*);
        if (!reinterpret_cast<Before>(table[0])(callback,hit.body)) continue;
        struct Params { float t; float dist; Vec normal; Vec point; }
            params{0,hit.distance,{},hit.point};
        using Intersect=bool(__thiscall*)(void*,void*,void*);
        reinterpret_cast<Intersect>(table[1])(callback,hit.body,&params);
    }
}
void __fastcall NativeTestPickClear(void* self,void*) {
    auto* pick=static_cast<TestPickCallback*>(self);
    pick->body=nullptr; pick->area=nullptr;
    pick->distance=pick->area_distance=pick->maximum_distance+10.0F;
}
void __fastcall NativeTestPickResults(void* self,void*) {
    auto* pick=static_cast<TestPickCallback*>(self);
    if (pick->area && pick->area_distance<pick->distance) {
        pick->body=pick->area; pick->distance=pick->area_distance;
        pick->point=pick->area_point;
    }
}
float __fastcall NativeTestPickedDistance(void* player,void*) {
    auto* pick=Read<void*>(player,0x27C);
    const Vec point=Read<Vec>(pick,0x1C);
    const Vec position=Read<Vec>(Read<void*>(player,0x274),0x48);
    return std::hypot(point[0]-position[0],point[1]-position[1],point[2]-position[2]);
}
int __fastcall NativeTestCrosshair(void* entity,void*,void*) {
    auto* player=Read<void*>(Read<void*>(entity,0x10),0x15C);
    const float distance=NativeTestPickedDistance(player,nullptr);
    return distance<=Read<float>(entity,0xB8) ? 4 : 10;
}
float __fastcall NativeTestEntityDistance(void* entity,void*) {
    return NativeTestPickedDistance(Read<void*>(Read<void*>(entity,0x10),0x15C),nullptr);
}
bool __cdecl NativeTestWideNotEmpty(const void* value,const std::uint16_t*) {
    return Read<bool>(value,0);
}
void* test_normal_world=nullptr;
TestPickCallback* test_normal_callback=nullptr;
int test_native_normal_calls=0,test_native_save_calls=0,test_native_lamp_calls=0;
void __fastcall NativeTestNormalUpdate(void* state,void*,float) {
    ++test_native_normal_calls;
    NativeTestPickClear(test_normal_callback,nullptr);
    const Vec from{0,1,0},to{0,1,-10};
    reinterpret_cast<Ray>(g_image+0xAD835)(test_normal_world,test_normal_callback,
        &from,&to,true,false,true,false);
    NativeTestPickResults(test_normal_callback,nullptr);
    const int cue=test_normal_callback->body ? 1 : 10;
    Put(Read<void*>(state,0x10),0x2E4,cue);
}
void __fastcall NativeTestSaveInteract(void* entity,void*) {
    auto* player=Read<void*>(Read<void*>(entity,0x10),0x15C);
    if (static_cast<double>(NativeTestPickedDistance(player,nullptr))<=2.1)
        ++test_native_save_calls;
}
void __fastcall NativeTestLampInteract(void* entity,void*) {
    auto* player=Read<void*>(Read<void*>(entity,0x10),0x15C);
    if (NativeTestPickedDistance(player,nullptr)<=Read<float>(entity,0xBC))
        ++test_native_lamp_calls;
}
void Jump(std::uintptr_t rva,void* target) {
    auto* entry=g_image+rva;
    entry[0]=0xE9;
    const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target)-reinterpret_cast<std::uintptr_t>(entry+5));
    std::memcpy(entry+1,&delta,4);
    FlushInstructionCache(GetCurrentProcess(),entry,5);
}
}
runtime::VrControllerFrame ReadNativeControllerFrame() noexcept { return test_frame; }
void NativeControllerHaptic(runtime::VrHand,runtime::VrHapticEvent,float) noexcept {}
bool NativeInputUiActive() noexcept { return test_ui; }
bool ControllerWorldPose(const runtime::VrHmdPose& hand, Matrix& pose, Vec& velocity, Vec& angular) noexcept {
    if (!test_head_pose_valid || !hand.device_connected || !hand.pose_valid) return false;
    pose=runtime::Multiply(test_controller_world_basis,
        runtime::ExpandMatrix(hand.device_to_absolute));
    velocity=hand.velocity; angular=hand.angular_velocity; return true;
}
bool TrackedHeadWorldPose(runtime::VrMatrix44& pose) noexcept {
    if (!test_head_pose_valid) return false;
    pose=test_head_pose;
    return true;
}
void PublishGameplayPalmHeldBody(std::size_t hand_index,void* body) noexcept {
    if (hand_index<test_palm_held.size()) test_palm_held[hand_index]=body;
}
void SetGameplayInteractionTargetProvider(
    GameplayInteractionTargetProvider provider) noexcept {
    test_interaction_target_provider=provider;
}
bool ReadGameplayPalmPose(std::size_t, runtime::VrMatrix44& pose) noexcept {
    if (!test_resolved_palm_valid) return false;
    pose=test_resolved_palm;
    return true;
}
std::uint64_t GameplayPalmPoseGeneration(std::size_t) noexcept {
    return test_resolved_palm_generation;
}
std::uint64_t GameplayPalmYawEpoch() noexcept {
    return test_palm_yaw_epoch;
}
void ServiceGameplayPalmResolver(std::uint8_t*,void* character_body) noexcept {
    ++test_palm_resolver_services;
    test_palm_resolver_character_body=character_body;
    test_palm_resolver_saw_held_body=
        test_palm_held[0]!=nullptr || test_palm_held[1]!=nullptr;
    if (test_palm_resolver_publish_on_service &&
        test_palm_resolver_saw_held_body && test_resolved_palm_valid)
        ++test_resolved_palm_generation;
}
bool QueryGameplayPalmOverlaps(std::size_t,
    const runtime::VrMatrix44&, GameplayPalmOverlapResult& result) noexcept {
    result={};
    if (!test_palm_overlap_available) return false;
    result=test_palm_overlap;
    return true;
}
#include "native_slider_fixture.hpp"

int RunSpatialTest() {
    {
        // A stationary wrist can be opposite the lagging bone. The angular
        // controller must still converge, including at the exact half-turn.
        for (int axis=0;axis<3;++axis) {
            const auto current=runtime::IdentityMatrix();
            auto destination=current;
            for (int other=0;other<3;++other)
                if (other!=axis) destination.values[other*4+other]=-1;
            Vec corrected{};
            if (!ArticulatedAngularDrive(current,destination,{},{},0.016F,corrected) ||
                !FiniteVec(corrected) || std::abs(corrected[axis])<0.19F ||
                std::hypot(corrected[0],corrected[1],corrected[2])>0.20001F)
                return 225;
        }
        // Near pi, select the shortest direction on either side. An oblique
        // exact half-turn also exercises recovery beyond a diagonal matrix.
        for (float angle : {3.1410F,-3.1410F}) {
            const auto current=runtime::IdentityMatrix();
            auto destination=current;
            destination.values[0]=destination.values[5]=std::cos(angle);
            destination.values[1]=-std::sin(angle);
            destination.values[4]=std::sin(angle);
            Vec corrected{};
            if (!ArticulatedAngularDrive(current,destination,{},{},0.016F,corrected) ||
                corrected[2]*angle<=0 || std::abs(corrected[2])<0.19F) return 227;
        }
        auto oblique=runtime::IdentityMatrix();
        const Vec axis{0.6F,0.8F,0};
        for (int row=0;row<3;++row)
            for (int col=0;col<3;++col)
                oblique.values[row*4+col]=2*axis[row]*axis[col]-(row==col ? 1.0F : 0.0F);
        Vec corrected{};
        if (!ArticulatedAngularDrive(runtime::IdentityMatrix(),oblique,{},{},0.016F,corrected) ||
            std::abs(Dot(axis,corrected))<0.19F) return 228;
    }
    {
        // Run the articulated contact controller against the shipped Newton
        // solver, with several chain sizes and selected endpoints/interior
        // bodies. Synthetic ABI tests alone cannot detect joint separation.
        for (const unsigned count : {2U,3U,7U,14U,18U,24U}) {
            for (const unsigned selected : {0U,count/2,count-1}) {
                const NewtonWorld* world=NewtonCreate(nullptr,nullptr);
                NewtonCollision* shape=NewtonCreateBox(world,0.08F,0.08F,0.08F,nullptr);
                std::vector<NewtonBody*> bodies;
                for (unsigned index=0;index<count;++index) {
                    auto* body=NewtonCreateBody(world,shape);
                    auto matrix=runtime::IdentityMatrix();
                    matrix.values[12]=static_cast<float>(index)*0.1F;
                    NewtonBodySetMatrix(body,matrix.values.data());
                    const float original_mass=count==2 && index==1 ? 2.2F : 3.0F;
                    const float mass=index==selected ? original_mass/14.0F : original_mass;
                    NewtonBodySetMassMatrix(body,mass,0.01F*mass,0.01F*mass,0.01F*mass);
                    NewtonBodySetAutoFreeze(body,0);
                    if (index!=selected) {
                        // Native Grab disables gravity only on the selected
                        // bone. The rest of the ragdoll must still be lifted.
                        NewtonBodySetForceAndTorqueCallback(body,[](const NewtonBody* falling) {
                            float body_mass=0,ix=0,iy=0,iz=0;
                            NewtonBodyGetMassMatrix(falling,&body_mass,&ix,&iy,&iz);
                            const Vec gravity{0,-9.81F*body_mass,0};
                            NewtonBodySetForce(falling,gravity.data());
                        });
                    }
                    if (index) {
                        const Vec pivot{static_cast<float>(index)*0.1F-0.05F,0,0};
                        if (count==2) {
                            const Vec pin{0,0,1};
                            NewtonConstraintCreateHinge(world,pivot.data(),pin.data(),body,bodies.back());
                        } else NewtonConstraintCreateBall(world,pivot.data(),bodies.back(),body);
                    }
                    bodies.push_back(body);
                }
                bool stable=true;
                const Vec local_contact{0.03F,0,0};
                const auto read_pose=[](const NewtonBody* body) {
                    Matrix native{},pose{};
                    NewtonBodyGetMatrix(body,native.values.data());
                    for (unsigned row=0;row<4;++row)
                        for (unsigned col=0;col<4;++col)
                            pose.values[row*4+col]=native.values[col*4+row];
                    return pose;
                };
                const auto start=read_pose(bodies[selected]);
                for (unsigned tick=0;tick<360;++tick) {
                    constexpr float dt=1.0F/90.0F;
                    auto destination=start;
                    destination.values[7]=static_cast<float>(tick)*dt*0.15F;
                    const auto current=read_pose(bodies[selected]);
                    Vec linear{},angular{},impulse{},point{},driven{};
                    NewtonBodyGetVelocity(bodies[selected],linear.data());
                    NewtonBodyGetOmega(bodies[selected],angular.data());
                    if (!ArticulatedContactImpulse(current,destination,local_contact,
                            {0,0.15F,0},linear,angular,dt,impulse,point) ||
                        !ArticulatedAngularDrive(current,destination,{},angular,dt,driven)) {
                        stable=false; break;
                    }
                    NewtonAddBodyImpulse(bodies[selected],impulse.data(),point.data());
                    for (unsigned index=0;index<count;++index) {
                        if (index==selected) continue;
                        Vec carried{};
                        NewtonBodyGetVelocity(bodies[index],carried.data());
                        for (int axis=0;axis<3;++axis) carried[axis]+=impulse[axis];
                        NewtonBodySetVelocity(bodies[index],carried.data());
                    }
                    NewtonBodyGetOmega(bodies[selected],angular.data());
                    if (!ArticulatedAngularDrive(current,destination,{},angular,dt,driven)) {
                        stable=false; break;
                    }
                    NewtonBodySetOmega(bodies[selected],driven.data());
                    NewtonUpdate(world,dt);
                    for (unsigned index=1;index<count;++index) {
                        const auto a=TransformPoint(read_pose(bodies[index-1]),{0.05F,0,0});
                        const auto b=TransformPoint(read_pose(bodies[index]),{-0.05F,0,0});
                        const Vec error{a[0]-b[0],a[1]-b[1],a[2]-b[2]};
                        if (!FiniteVec(error) || std::hypot(error[0],error[1],error[2])>0.04F)
                            stable=false;
                    }
                }
                const auto final=read_pose(bodies[selected]);
                NewtonReleaseCollision(world,shape);
                NewtonDestroy(world);
                const float target_height=start.values[7]+359.0F/90.0F*0.15F;
                if (!stable || std::abs(final.values[7]-target_height)>0.08F) {
                    std::cerr << "articulated Newton chain failed: bodies=" << count
                        << " selected=" << selected << " y=" << final.values[7] << '\n';
                    return 219;
                }
            }
        }
    }
    {
        // A world-rooted three-hinge arm must rise through the native solver.
        // Exercise world and explicit static endpoints, all selectable bones,
        // native-like selected mass reduction/gravity and live joint limits.
        for (const bool static_root : {false,true}) {
            for (const unsigned selected : {0U,1U,2U}) {
                const auto* world=NewtonCreate(nullptr,nullptr);
                auto* shape=NewtonCreateBox(world,0.18F,0.06F,0.06F,nullptr);
                NewtonBody* fixed=static_root ? NewtonCreateBody(world,shape) : nullptr;
                std::array<NewtonBody*,3> bodies{};
                std::array<NewtonJoint*,3> joints{};
                const auto read_pose=[](const NewtonBody* body) {
                    Matrix native{},pose{};
                    NewtonBodyGetMatrix(body,native.values.data());
                    for (unsigned row=0;row<4;++row)
                        for (unsigned col=0;col<4;++col)
                            pose.values[row*4+col]=native.values[col*4+row];
                    return pose;
                };
                const Vec pin{0,0,1};
                for (unsigned index=0;index<3;++index) {
                    auto* bone=NewtonCreateBody(world,shape);
                    bodies[index]=bone;
                    auto pose=runtime::IdentityMatrix();
                    pose.values[12]=0.1F+0.2F*index;
                    NewtonBodySetMatrix(bone,pose.values.data());
                    const float mass=index==selected ? (4.0F-index)/14.0F : 4.0F-index;
                    NewtonBodySetMassMatrix(bone,mass,0.01F*mass,0.01F*mass,0.01F*mass);
                    NewtonBodySetAutoFreeze(bone,0);
                    if (index!=selected)
                        NewtonBodySetForceAndTorqueCallback(bone,[](const NewtonBody* falling) {
                            float mass,ix,iy,iz;
                            NewtonBodyGetMassMatrix(falling,&mass,&ix,&iy,&iz);
                            const Vec gravity{0,-9.81F*mass,0};
                            NewtonBodySetForce(falling,gravity.data());
                        });
                    const Vec pivot{0.2F*index,0,0};
                    joints[index]=NewtonConstraintCreateHinge(world,pivot.data(),pin.data(),
                        bone,index ? bodies[index-1] : fixed);
                    NewtonHingeSetUserCallback(joints[index],[](const NewtonJoint* hinge,
                        NewtonHingeSliderUpdateDesc* desc)->unsigned {
                        const float angle=NewtonHingeGetJointAngle(hinge);
                        if (angle>=-0.7F && angle<=0.7F) return 0;
                        desc->m_accel=NewtonHingeCalculateStopAlpha(hinge,desc,
                            std::clamp(angle,-0.7F,0.7F));
                        return 1;
                    });
                }
                const Vec contact{0.1F,0,0};
                const auto start=read_pose(bodies[selected]);
                const float reach=0.2F*(selected+1);
                bool stable=true;
                for (unsigned tick=0;tick<360;++tick) {
                    constexpr float dt=1.0F/90.0F;
                    const float angle=std::min(tick*dt*0.3F,0.6F);
                    auto destination=start;
                    destination.values[3]=reach*std::cos(angle)-contact[0];
                    destination.values[7]=reach*std::sin(angle);
                    const auto current=read_pose(bodies[selected]);
                    Vec linear{},angular{},impulse{},point{};
                    NewtonBodyGetVelocity(bodies[selected],linear.data());
                    NewtonBodyGetOmega(bodies[selected],angular.data());
                    if (!ArticulatedContactImpulse(current,destination,contact,{},
                            linear,angular,dt,impulse,point)) { stable=false; break; }
                    NewtonAddBodyImpulse(bodies[selected],impulse.data(),point.data());
                    for (unsigned index=0;index<3;++index) {
                        if (index==selected) continue;
                        Vec carried{};
                        NewtonBodyGetVelocity(bodies[index],carried.data());
                        for (int axis=0;axis<3;++axis) carried[axis]+=impulse[axis];
                        NewtonBodySetVelocity(bodies[index],carried.data());
                    }
                    NewtonUpdate(world,dt);
                    for (unsigned index=0;index<3;++index) {
                        const auto a=TransformPoint(read_pose(bodies[index]),{-0.1F,0,0});
                        const auto b=index ? TransformPoint(read_pose(bodies[index-1]),{0.1F,0,0}) : Vec{};
                        const Vec separation{a[0]-b[0],a[1]-b[1],a[2]-b[2]};
                        if (!FiniteVec(separation) ||
                            std::hypot(separation[0],separation[1],separation[2])>0.04F ||
                            std::abs(NewtonHingeGetJointAngle(joints[index]))>0.75F) stable=false;
                    }
                }
                const auto final=TransformPoint(read_pose(bodies[selected]),contact);
                if (fixed && !MatrixNearlyEqual(read_pose(fixed),runtime::IdentityMatrix())) stable=false;
                NewtonReleaseCollision(world,shape);
                NewtonDestroy(world);
                if (!stable || final[1]<reach*std::sin(0.6F)-0.08F) {
                    std::cerr << "anchored arm failed: static=" << static_root
                        << " selected=" << selected << " y=" << final[1] << '\n';
                    return 246;
                }
            }
        }
    }
    {
        // tech_lever's activation is 110 degrees, with native stops near
        // -1/130 degrees. Offset COM exposes the origin-velocity mismatch;
        // the zero-COM control preserves the prior mathematical behavior.
        for (const Vec local_com : {Vec{},Vec{0.156F,0.416F,0}}) {
            const auto* world=NewtonCreate(nullptr,nullptr);
            auto* shape=NewtonCreateBox(world,0.08F,0.4F,0.06F,nullptr);
            auto* body=NewtonCreateBody(world,shape);
            NewtonBodySetMassMatrix(body,2,0.04F,0.04F,0.04F);
            NewtonBodySetCentreOfMass(body,local_com.data());
            NewtonBodySetAutoFreeze(body,0);
            const Vec pivot{0.093F,0.151F,0},pin{0,0,1};
            auto* hinge=NewtonConstraintCreateHinge(world,pivot.data(),pin.data(),body,nullptr);
            NewtonHingeSetUserCallback(hinge,[](const NewtonJoint* joint,
                NewtonHingeSliderUpdateDesc* desc)->unsigned {
                const float angle=NewtonHingeGetJointAngle(joint);
                constexpr float minimum=-0.0174533F,maximum=2.268928F;
                if (angle>=minimum && angle<=maximum) return 0;
                desc->m_accel=NewtonHingeCalculateStopAlpha(joint,desc,
                    std::clamp(angle,minimum,maximum));
                return 1;
            });
            const auto read_pose=[&] {
                Matrix native{},pose{};
                NewtonBodyGetMatrix(body,native.values.data());
                for (unsigned row=0;row<4;++row)
                    for (unsigned col=0;col<4;++col)
                        pose.values[row*4+col]=native.values[col*4+row];
                return pose;
            };
            const Vec local_contact{0.16F,0.55F,0};
            const Vec radial{local_contact[0]-pivot[0],local_contact[1]-pivot[1],0};
            bool stable=true,activated=false;
            for (unsigned tick=0;tick<540;++tick) {
                constexpr float dt=1.0F/90.0F;
                const float angle=std::min(tick*dt*1.2F,2.1F);
                const Vec target{pivot[0]+radial[0]*std::cos(angle)-radial[1]*std::sin(angle),
                    pivot[1]+radial[0]*std::sin(angle)+radial[1]*std::cos(angle),0};
                const auto pose=read_pose();
                const auto contact=TransformPoint(pose,local_contact);
                const Vec delta{target[0]-contact[0],target[1]-contact[1],target[2]-contact[2]};
                auto plan=runtime::vr_mechanism_policy::PlanHinge(delta,pin,pivot,contact,
                    {pose.values[3],pose.values[7],pose.values[11]},1.35F);
                if (!CorrectHingeMassCentreVelocity(pose,local_com,plan)) { stable=false; break; }
                if (tick==0 && (!VecNearlyEqual(plan.linear_velocity,{}) ||
                        !VecNearlyEqual(plan.angular_velocity,{}))) stable=false;
                NewtonBodySetVelocity(body,plan.linear_velocity.data());
                NewtonBodySetOmega(body,plan.angular_velocity.data());
                NewtonUpdate(world,dt);
                const auto retained_pivot=TransformPoint(read_pose(),pivot);
                const Vec error{retained_pivot[0]-pivot[0],retained_pivot[1]-pivot[1],retained_pivot[2]-pivot[2]};
                const float actual_angle=NewtonHingeGetJointAngle(hinge);
                if (actual_angle>=1.919862F) activated=true;
                if (!FiniteVec(error) || std::hypot(error[0],error[1],error[2])>0.01F ||
                    actual_angle < -0.04F || actual_angle>2.30F) stable=false;
            }
            NewtonReleaseCollision(world,shape);
            NewtonDestroy(world);
            if (!stable || !activated) {
                std::cerr << "offset-COM lever failed to reach native activation: COM="
                    << local_com[0] << ',' << local_com[1] << '\n';
                return 254;
            }
        }
    }
    {
        // BP's 0x19C3D0 wrapper forwards the vector unchanged to
        // NewtonAddBodyImpulse(pointDeltaVeloc, pointPosit). Exercise the real
        // shipped DLL, whose hash matches BP's installed Newton.dll.
        const NewtonWorld* world=NewtonCreate(nullptr,nullptr);
        NewtonCollision* shape=NewtonCreateBox(world,1,1,1,nullptr);
        const Matrix identity=runtime::IdentityMatrix();
        const Vec hand_center{-0.1F,0,0},contact{},stationary{};
        NudgePlan bounded{{1,0,0},0.22F,0.38F,0.10F};
        for (float mass : {1.0F,3.0F,20.0F,100.0F}) {
            NewtonBody* body=NewtonCreateBody(world,shape);
            NewtonBodySetMassMatrix(body,mass,mass,mass,mass);
            Vec request{};
            if (!ComputeNudgeImpulse(hand_center,{1,0,0},bounded.velocity,
                    contact,identity,stationary,stationary,mass,bounded,request)) return 174;
            NewtonAddBodyImpulse(body,request.data(),contact.data());
            Vec actual{};
            NewtonBodyGetVelocity(body,actual.data());
            NewtonDestroyBody(world,body);
            if (std::abs(actual[0]-0.10F)>0.00001F) {
                std::cerr << "BP bounded nudge exceeded delta-v: mass=" << mass
                    << " speed=" << actual[0] << '\n';
                NewtonReleaseCollision(world,shape);
                NewtonDestroy(world);
                return 175;
            }
        }
        NewtonReleaseCollision(world,shape);
        NewtonDestroy(world);
    }
    {
        // Newton can report a negative distance when a widened VR ray starts
        // inside/behind geometry. Such a hit is not a forward acquisition and
        // must never outrank a valid candidate or underflow telemetry.
        RankedRayCallback ranked{};
        struct Params { float t; float dist; Vec normal; Vec point; };
        int behind_body=1,forward_body=2;
        const auto candidates_before=g_selection_candidates.load();
        Params behind{0.0F,-0.25F,{}, {9.0F,0,0}};
        if (!RankedRayCallback::Intersect(&ranked,nullptr,&behind_body,&behind) ||
            ranked.best_body!=nullptr ||
            g_selection_candidates.load()!=candidates_before) return 153;
        Params forward{0.0F,0.15F,{}, {0.15F,0,0}};
        if (!RankedRayCallback::Intersect(&ranked,nullptr,&forward_body,&forward) ||
            ranked.best_body!=&forward_body ||
            std::abs(ranked.best_distance-0.15F)>0.00001F) return 154;
    }
    {
        std::array<std::uint8_t,16> protected_storage{};
        std::array<std::uint8_t,16> other_storage{};
        const auto now=GetTickCount64();
        AcquireSRWLockExclusive(&g_interaction_target_lock);
        g_interaction_targets[0]={{1.0F,2.0F,3.0F},protected_storage.data(),now,true};
        ReleaseSRWLockExclusive(&g_interaction_target_lock);
        if (ProtectedNudgeBody(0,now)!=protected_storage.data() ||
            ProtectedNudgeBody(1,now)!=nullptr ||
            ProtectedNudgeBody(0,now+kInteractionTargetMaximumAgeMilliseconds+1)!=nullptr ||
            protected_storage.data()==other_storage.data()) return 130;
        AcquireSRWLockExclusive(&g_interaction_target_lock);
        g_interaction_targets[0]={};
        ReleaseSRWLockExclusive(&g_interaction_target_lock);
    }
    {
        const Vec hand_center{0,0,0};
        const Vec contact{0.1F,0,0};
        const Vec stationary{};
        const Matrix body_matrix=runtime::IdentityMatrix();
        Vec impulse{};
        float applied_delta=0.0F;
        NudgePlan gentle{{1,0,0},0.34F,0.65F,0.22F};
        if (!ComputeNudgeImpulse(hand_center,{1,0,0},gentle.velocity,contact,
                body_matrix,stationary,stationary,2.0F,gentle,impulse,
                &applied_delta) ||
            !VecNearlyEqual(impulse,{0.22F,0,0}) ||
            std::abs(applied_delta-0.22F)>=0.00001F) return 40;
        NudgePlan large{{1,0,0},0.22F,0.38F,0.10F};
        if (!ComputeNudgeImpulse(hand_center,{1,0,0},large.velocity,contact,
                body_matrix,stationary,stationary,20.0F,large,impulse) ||
            !VecNearlyEqual(impulse,{0.10F,0,0})) return 41;
        NudgePlan door{{1,0,0},0.30F,0.30F,0.10F};
        if (!ComputeNudgeImpulse(hand_center,{1,0,0},door.velocity,contact,
                body_matrix,stationary,stationary,2.0F,door,impulse) ||
            !VecNearlyEqual(impulse,{0.10F,0,0})) return 42;
        if (ComputeNudgeImpulse(hand_center,{-1,0,0},{-1,0,0},contact,
                body_matrix,stationary,stationary,2.0F,gentle,impulse)) return 43;
        if (ComputeNudgeImpulse(hand_center,{1,0,0},gentle.velocity,contact,
                body_matrix,{0.34F,0,0},stationary,2.0F,gentle,impulse)) return 44;
    }
    g_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if (!g_image) return 1;
    {
        using MagneticClass = runtime::vr_magnetic_pickup_policy::VrMagneticPickupClass;
        if (MagneticClassForItemSubtype(3)!=MagneticClass::consumable ||
            MagneticClassForItemSubtype(0)!=MagneticClass::ordinary ||
            MagneticClassForItemSubtype(4)!=MagneticClass::equipment ||
            MagneticClassForItemSubtype(12)!=MagneticClass::unsupported ||
            MagneticClassForItemSubtype(13)!=MagneticClass::unsupported)
            return 45;
        std::array<std::uint8_t,0x500> magnetic_body{};
        std::array<std::uint8_t,0x300> magnetic_entity{};
        Put(magnetic_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(magnetic_body.data(),kBodyActiveOffset,true);
        Put(magnetic_body.data(),kBodyCollideOffset,true);
        Put(magnetic_body.data(),kBodyUserDataOffset,static_cast<void*>(magnetic_entity.data()));
        Put(magnetic_entity.data(),kEntityActiveOffset,true);
        Put(magnetic_entity.data(),kEntityTypeOffset,kItemEntityType);
        Put(magnetic_entity.data(),kItemSubtypeOffset,3);
        runtime::vr_magnetic_pickup_policy::VrMagneticPickupProfile profile{};
        if (!MagneticItemProfile(magnetic_body.data(),profile) || !profile.eligible ||
            profile.range!=runtime::vr_magnetic_pickup_policy::kMaximumRange)
            return 46;
        Put(magnetic_entity.data(),kItemSubtypeOffset,12);
        if (MagneticItemProfile(magnetic_body.data(),profile)) return 47;
        Put(magnetic_entity.data(),kItemSubtypeOffset,3);
        Put(magnetic_body.data(),kBodyCharacterOffset,true);
        if (MagneticItemProfile(magnetic_body.data(),profile)) return 48;
    }
    Jump(0xAC900,reinterpret_cast<void*>(&NativeEnter)); Jump(0xAA4C0,reinterpret_cast<void*>(&NativeLeave));
    Jump(0xABA90,reinterpret_cast<void*>(&NativeUpdate)); Jump(0xA9FD0,reinterpret_cast<void*>(&NativeStop));
    Jump(0xAAC80,reinterpret_cast<void*>(&NativeMoveEnter)); Jump(0xAAED0,reinterpret_cast<void*>(&NativeMoveLeave));
    Jump(0xAA690,reinterpret_cast<void*>(&NativeMoveUpdate)); Jump(0xAA030,reinterpret_cast<void*>(&NativeMoveStop));
    Jump(0xAB370,reinterpret_cast<void*>(&NativePushEnter)); Jump(0xAB6A0,reinterpret_cast<void*>(&NativePushLeave));
    Jump(0xAB010,reinterpret_cast<void*>(&NativePushUpdate)); Jump(0xAB1F0,reinterpret_cast<void*>(&NativePushStop));
    Jump(0x9CA80,reinterpret_cast<void*>(&NativeChangeActionState));
    Jump(0x19C950,NativeMemberEntry(&TestVelocityGetters::GetMassCentre));
    Jump(0xCCF00,reinterpret_cast<void*>(&JointCount)); Jump(0xCA120,reinterpret_cast<void*>(&BodyMatrix));
    Jump(kGetBodyJoint,reinterpret_cast<void*>(&BodyJoint));
    Jump(kHingeGetType,reinterpret_cast<void*>(&HingeType));
    Jump(kSliderGetType,reinterpret_cast<void*>(&SliderType));
    Jump(0x19C360,reinterpret_cast<void*>(&MaxLinear)); Jump(0x19C380,reinterpret_cast<void*>(&MaxAngular));
    Jump(0x19C2A0,reinterpret_cast<void*>(&Linear)); Jump(0x19C2C0,reinterpret_cast<void*>(&Angular));
    Jump(kGetLinearVelocity,NativeMemberEntry(&TestVelocityGetters::GetLinear));
    Jump(kGetAngularVelocity,NativeMemberEntry(&TestVelocityGetters::GetAngular));
    Jump(0x19C590,reinterpret_cast<void*>(&Gravity));
    Jump(0x19C3F0,reinterpret_cast<void*>(&Active));
    Jump(0x19C450,reinterpret_cast<void*>(&AutoFreeze));
    Jump(0x19C9A0,reinterpret_cast<void*>(&BodyForce));
    Jump(0x19C9E0,reinterpret_cast<void*>(&BodyForceAtPosition));
    Jump(kAddImpulseAtPosition,reinterpret_cast<void*>(&BodyImpulseAtPosition));
    {
        std::array<std::uint8_t,0x500> nudge_body{};
        std::array<std::uint8_t,0x300> nudge_entity{};
        Put(nudge_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(nudge_body.data(),kBodyActiveOffset,true);
        Put(nudge_body.data(),kBodyCollideOffset,true);
        Put(nudge_body.data(),kBodyUserDataOffset,
            static_cast<void*>(nudge_entity.data()));
        Put(nudge_entity.data(),kEntityActiveOffset,true);
        Put(nudge_entity.data(),kEntityTypeOffset,kObjectEntityType);
        NudgePlan plan{};
        test_joints=0;
        if (!BuildNudgePlan(nudge_body.data(),{1,0,0},{0.1F,0,0},
                2.0F,0.20F,0,plan) ||
            std::abs(plan.push_fraction-0.60F)>0.00001F ||
            std::abs(plan.maximum_push_speed-0.90F)>0.00001F ||
            std::abs(plan.maximum_delta_velocity-0.28F)>0.00001F)
            return 55;
        if (!BuildNudgePlan(nudge_body.data(),{1,0,0},{0.1F,0,0},
                4.0F,0.80F,0,plan) ||
            std::abs(plan.push_fraction-0.22F)>0.00001F ||
            std::abs(plan.maximum_push_speed-0.38F)>0.00001F ||
            std::abs(plan.maximum_delta_velocity-0.10F)>0.00001F)
            return 56;
        std::array<std::uint8_t,0x180> joint{};
        Put(joint.data(),0,g_image+kPhysicsJointHingeNewtonVtable);
        Put(g_image+kPhysicsJointHingeNewtonVtable,kJointTypeVtableSlot,
            g_image+kHingeGetType);
        Put(joint.data(),kJointPinDirectionOffset,Vec{0,1,0});
        Put(joint.data(),kJointPivotPointOffset,Vec{0,0,0});
        test_joint=joint.data();
        test_joints=1;
        // Rework nudges recognized hinge/slider Objects directly; Move state
        // commitment is only required for taking over the sustained mechanism
        // servo. A locker/drawer must therefore not be rejected merely because
        // its entity family is Object.
        if (!BuildNudgePlan(nudge_body.data(),{0,0,-1},{1,0,0},
                4.0F,0.40F,1,plan) ||
            !VecNearlyEqual(plan.velocity,{0,0,-1}) ||
            std::abs(plan.push_fraction-0.80F)>0.00001F ||
            std::abs(plan.maximum_push_speed-1.35F)>0.00001F ||
            std::abs(plan.maximum_delta_velocity-0.36F)>0.00001F)
            return 57;

        Put(nudge_entity.data(),kEntityTypeOffset,kSwingDoorEntityType);
        if (!BuildNudgePlan(nudge_body.data(),{0,0,-1},{1,0,0},
                4.0F,0.40F,1,plan) ||
            !VecNearlyEqual(plan.velocity,{0,0,-1}) ||
            std::abs(plan.push_fraction-0.30F)>0.00001F ||
            std::abs(plan.maximum_push_speed-0.30F)>0.00001F ||
            std::abs(plan.maximum_delta_velocity-0.10F)>0.00001F)
            return 58;
        test_joint=nullptr;
        test_joints=0;
    }
    std::array<std::uint8_t,0x500> body{},player{},state{};
    std::array<std::uint8_t,0x200> character_body{};
    std::array<void*,10> states{}; states[6]=state.data();
    Put(player.data(),0x2C4,states.data());
    Put(player.data(),0x274,static_cast<void*>(character_body.data()));
    Put(state.data(),0x10,player.data()); Put(state.data(),0x20,body.data());
    Put(body.data(),0,g_image+0x292C08); Put(body.data(),0x34,runtime::IdentityMatrix());
    Put(body.data(),0x42C,3.0F); Put(body.data(),0x430,4.0F);
    Put(body.data(),0x3C8,true);
    test_frame.focused=true;
    auto& hand=test_frame.hands[1].grip;
    hand.pose_valid=true; hand.device_connected=true;
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    hand.velocity={2,0,0}; hand.angular_velocity={0,2,0};
    {
        // Rework UseItem casts from the dominant controller aim along its
        // local -Z axis. Black Plague's native state instead starts at the
        // camera, which makes a syringe selected from inventory impossible to
        // point with the hand. Preserve the native ray length while replacing
        // only that origin/orientation boundary.
        auto& aim=test_frame.hands[1].aim;
        aim.pose_valid=true; aim.device_connected=true;
        aim.device_to_absolute={{1,0,0,1,0,1,0,2,0,0,1,3}};
        Vec use_from{},use_to{};
        if (!BuildUseItemHandRay(test_frame,2.0F,use_from,use_to) ||
            !VecNearlyEqual(use_from,{1,2,3}) ||
            !VecNearlyEqual(use_to,{1,2,1})) return 156;
        test_frame.interact_source=runtime::VrHand::left;
        if (BuildUseItemHandRay(test_frame,2.0F,use_from,use_to)) return 157;
        test_frame.interact_source=runtime::VrHand::right;
        aim.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};

        std::array<std::uint8_t,0x40> use_state{};
        std::array<std::uint8_t,0x200> use_init{};
        std::array<std::uint8_t,0x400> use_effect{};
        std::array<std::uint8_t,0x40> use_pick{};
        void* use_init_pointer=use_init.data();
        void* use_effect_pointer=use_effect.data();
        void* use_pick_pointer=use_pick.data();
        Put(use_state.data(),0x0C,use_init_pointer);
        Put(use_state.data(),0x10,static_cast<void*>(player.data()));
        Put(use_init.data(),0x15C,use_effect_pointer);
        Put(player.data(),0x27C,use_pick_pointer);
        Put(player.data(),0x2BC,4);
        Put(use_pick.data(),0x04,static_cast<void*>(body.data()));
        Put(use_pick.data(),0x1C,Vec{1,2,1.25F});
        Put(use_effect.data(),0x30C,true);
        PublishUseItemLaserFromNativeState(
            use_state.data(),{1,2,3},{1,2,1});
        Vec published_from{},published_to{};
        bool published_usable=false;
        if (!ReadUseItemLaser(published_from,published_to,published_usable) ||
            !published_usable || !VecNearlyEqual(published_from,{1,2,3}) ||
            !VecNearlyEqual(published_to,{1,2,1.25F})) return 158;
        Put(use_effect.data(),0x30C,false);
        Put(use_pick.data(),0x04,static_cast<void*>(nullptr));
        PublishUseItemLaserFromNativeState(
            use_state.data(),{1,2,3},{1,2,1});
        if (!ReadUseItemLaser(published_from,published_to,published_usable) ||
            published_usable || !VecNearlyEqual(published_to,{1,2,1}))
            return 159;
        ClearUseItemLaser();
        if (ReadUseItemLaser(
                published_from,published_to,published_usable)) return 160;
        Put(player.data(),0x2BC,0);
    }
    {
        std::array<std::uint8_t,0x100> world{};
        std::array<std::uint8_t,0x500> touched_body{};
        std::array<std::uint8_t,0x300> touched_entity{};
        Put(world.data(),0,g_image+kPhysicsWorldVtable);
        Put(touched_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(touched_body.data(),kBodyActiveOffset,true);
        Put(touched_body.data(),kBodyCollideOffset,true);
        Put(touched_body.data(),kBodyUserDataOffset,
            static_cast<void*>(touched_entity.data()));
        Put(touched_entity.data(),kEntityActiveOffset,true);
        Put(touched_entity.data(),kEntityTypeOffset,kObjectEntityType);
        // Native SetInteractMode marks a movable Object as interactive.
        Put(touched_entity.data(),0x37,true);
        test_resolved_palm=runtime::IdentityMatrix();
        test_resolved_palm_valid=true;
        test_palm_overlap={};
        test_palm_overlap.valid=true;
        test_palm_overlap.hit_count=1;
        test_palm_overlap.hits[0].body=touched_body.data();
        test_palm_overlap.hits[0].contact_sum={0.04F,0,0};
        test_palm_overlap.hits[0].contact_count=1;
        test_palm_overlap_available=true;
        TestPickCallback::VTable table{
            reinterpret_cast<bool(__thiscall*)(void*,void*)>(TestPickCallback::Before),
            reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(TestPickCallback::Intersect)};
        TestPickCallback callback{&table};
        PalmSelectionCandidate selected{};
        if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
            selected.body!=touched_body.data() || selected.assisted ||
            std::abs(selected.distance-0.04F)>=0.00001F) return 54;
        // An inert table/decoration is a different Object entity, so the
        // assembly-sibling filter cannot protect the nearby usable prop.
        std::array<std::uint8_t,0x500> inert_body{};
        std::array<std::uint8_t,0x300> inert_entity{};
        Put(inert_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(inert_body.data(),kBodyActiveOffset,true);
        Put(inert_body.data(),kBodyCollideOffset,true);
        Put(inert_body.data(),kBodyUserDataOffset,
            static_cast<void*>(inert_entity.data()));
        Put(inert_entity.data(),kEntityActiveOffset,true);
        Put(inert_entity.data(),kEntityTypeOffset,kObjectEntityType);
        test_palm_overlap.hit_count=2;
        test_palm_overlap.hits[1]={};
        test_palm_overlap.hits[1].body=inert_body.data();
        test_palm_overlap.hits[1].contact_sum={0.01F,0,0};
        test_palm_overlap.hits[1].contact_count=1;
        if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
            selected.body!=touched_body.data()) return 178;
        std::swap(test_palm_overlap.hits[0],test_palm_overlap.hits[1]);
        if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
            selected.body!=touched_body.data()) return 179;
        std::swap(test_palm_overlap.hits[0],test_palm_overlap.hits[1]);
        // Native script hooks are still eligible on otherwise static Objects.
        for (const auto script_offset : {0x204U,0x20CU}) {
            Put(inert_entity.data(),script_offset,reinterpret_cast<void*>(1));
            if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
                selected.body!=inert_body.data()) return 180;
            Put(inert_entity.data(),script_offset,static_cast<void*>(nullptr));
        }
        test_palm_overlap.hit_count=1;
        // A locker/vice owns a static frame and a movable child. The closer
        // frame cannot steal the palm pick from its nearby door/handle.
        std::array<std::uint8_t,0x500> static_frame{};
        std::array<void*,2> assembly{static_frame.data(),touched_body.data()};
        Put(static_frame.data(),0,g_image+kPhysicsBodyVtable);
        Put(static_frame.data(),kBodyActiveOffset,true);
        Put(static_frame.data(),kBodyCollideOffset,true);
        Put(static_frame.data(),kBodyUserDataOffset,
            static_cast<void*>(touched_entity.data()));
        Put(touched_body.data(),0x434,5.0F);
        Put(touched_entity.data(),0x14C,assembly.data());
        Put(touched_entity.data(),0x150,assembly.data()+assembly.size());
        test_palm_overlap.hit_count=2;
        test_palm_overlap.hits[1]={};
        test_palm_overlap.hits[1].body=static_frame.data();
        test_palm_overlap.hits[1].contact_sum={0.01F,0,0};
        test_palm_overlap.hits[1].contact_count=1;
        for (const auto type : {kObjectEntityType,kWheelEntityType}) {
            Put(touched_entity.data(),kEntityTypeOffset,type);
            if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
                selected.body!=touched_body.data()) return 176;
        }
        // A native directional ray must apply the same assembly policy.
        RankedRayCallback ranked{};
        struct Params { float t; float dist; Vec normal; Vec point; };
        Params frame_hit{0,0.01F,{}, {0.01F,0,0}};
        Params door_hit{0,0.04F,{}, {0.04F,0,0}};
        RankedRayCallback::Intersect(&ranked,nullptr,static_frame.data(),&frame_hit);
        RankedRayCallback::Intersect(&ranked,nullptr,touched_body.data(),&door_hit);
        if (ranked.best_body!=touched_body.data()) return 177;
        test_palm_overlap_available=false;
        test_palm_overlap={};
    }
    test_resolved_palm=runtime::IdentityMatrix();
    test_resolved_palm.values[3]=5; test_resolved_palm.values[7]=6;
    test_resolved_palm.values[11]=7; test_resolved_palm_valid=true;
    Matrix resolved_check; Vec resolved_velocity{},resolved_angular{};
    const Matrix expected_visible_palm=
        runtime::rework_hand_profile::ApplyVisualLocalPose(test_resolved_palm);
    if (!HandPose(runtime::VrHand::right,false,resolved_check,resolved_velocity,resolved_angular) ||
        resolved_check.values!=expected_visible_palm.values)
        return 30;
    if (!HandPose(runtime::VrHand::right,true,resolved_check,resolved_velocity,resolved_angular) ||
        resolved_check.values[3]!=0 || resolved_check.values[7]!=0 || resolved_check.values[11]!=0)
        return 31;
    // Match Rework 23c890f target acquisition: the visible palm stays at the
    // resolved position while interaction intent follows the raw controller by
    // at most 18 cm.
    test_resolved_palm=runtime::IdentityMatrix();
    test_resolved_palm.values[3]=-0.19F;
    Matrix interaction_check; Vec interaction_velocity{},interaction_angular{};
    Matrix expected_interaction=runtime::IdentityMatrix();
    expected_interaction.values[3]=-0.01F;
    expected_interaction=
        runtime::rework_hand_profile::ApplyVisualLocalPose(expected_interaction);
    const bool interaction_pose_ok=InteractionHandPose(runtime::VrHand::right,
        interaction_check,interaction_velocity,interaction_angular);
    const bool interaction_matrix_ok=interaction_pose_ok &&
        MatrixNearlyEqual(interaction_check,expected_interaction);
    if (!interaction_matrix_ok) {
        std::cerr<<"interaction pose actual xyz="<<interaction_check.values[3]<<','
            <<interaction_check.values[7]<<','<<interaction_check.values[11]
            <<" expected="<<expected_interaction.values[3]<<','
            <<expected_interaction.values[7]<<','<<expected_interaction.values[11]<<'\n';
        return 37;
    }
    test_resolved_palm_valid=false;
    auto begin=[&] {
        g_enabled.store(true); Put(player.data(),0x2BC,0);
        test_frame.input.state.interact.pressed=true; test_frame.input.state.interact.just_pressed=true;
        g_vr_selection_ready=true;
        g_vr_selection_player=player.data();
        HookedEnter(state.data(),nullptr,nullptr);
        Put(player.data(),0x2BC,6); // Native ChangeState commits only after Enter.
        ServiceSpatialInteraction(player.data(),false);
        if (g_prepared_grab.active) {
            if (test_resolved_palm_valid)
                test_resolved_palm=runtime::ExpandMatrix(hand.device_to_absolute);
            ++test_resolved_palm_generation;
            ServiceSpatialInteraction(player.data(),false);
        }
    };
    // A native/fallback transition with no VR grip selection must not be
    // promoted into a VR grab merely because the VR button edge is present.
    g_enabled.store(true); Put(player.data(),0x2BC,0);
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=false; g_vr_selection_player=nullptr;
    HookedEnter(state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,6);
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || g_pending_state) return 24;
    test_leaves=0;
    begin();
    if (g_held.load() || Read<float>(body.data(),0x42C)!=3 ||
        Read<int>(player.data(),0x2BC)!=0 || test_leaves!=1) return 15;
    // Exercise acquisition after the exact-build installation gate has proved
    // the native collision field and all of its required consumers.
    g_player_collision_filter_ready.store(true);
    // ChangeState publishes the committed state after Enter returns. The
    // OpenVR edge may therefore be gone by the next service point; the pending
    // transition must retain its originating hand while the button remains
    // held instead of requiring just_pressed twice. Rework then excludes the
    // accepted body and resolves the grab palm synchronously before anchoring
    // it; acquisition must not depend on a later character-update publication.
    g_enabled.store(true); Put(player.data(),0x2BC,0);
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    test_frame.interact_source=runtime::VrHand::right;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    test_resolved_palm=runtime::ExpandMatrix(hand.device_to_absolute);
    test_resolved_palm_valid=true;
    test_palm_resolver_publish_on_service=true;
    test_palm_resolver_services=0;
    test_palm_resolver_character_body=nullptr;
    test_palm_resolver_saw_held_body=false;
    HookedEnter(state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,6);
    test_frame.input.state.interact.just_pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (!g_held.load() || g_prepared_grab.active ||
        g_hold.hand!=runtime::VrHand::right || test_palm_held[1]!=body.data() ||
        test_palm_resolver_services!=1 ||
        test_palm_resolver_character_body!=character_body.data() ||
        !test_palm_resolver_saw_held_body) return 40;
    test_palm_resolver_publish_on_service=false;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load()) return 41;
    test_resolved_palm_valid=false;
    test_leaves=0;

    begin();
    if (!g_held.load() || Read<float>(body.data(),0x42C)!=20 || Read<bool>(body.data(),0x3C8) ||
        test_palm_held[1]!=body.data()) return 2;
    HookedGrabUpdate(state.data(),nullptr,0);
    if (!g_held.load() || test_leaves) return 3;
    // Kinematic Grab does not integrate dt. A slow frame with a fresh, steady
    // hand must not restore collision or eject the body into the player.
    HookedGrabUpdate(state.data(),nullptr,0.5F);
    if (!g_held.load() || test_leaves || Read<bool>(body.data(),0x3C8)) {
        std::cerr << "long tick ejected a valid held body\n";
        return 170;
    }
    test_active_calls=test_auto_freeze_calls=0;
    // A presentation stall can age the game/world reference before the next
    // render refresh, while the controller is still connected and tracked.
    // Keep ownership/collision exclusion; do not drive from an old world pose.
    const auto stalled_body_pose=Read<Matrix>(body.data(),0x34);
    test_head_pose_valid=false;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_held.load() || test_leaves || Read<bool>(body.data(),0x3C8) ||
        !MatrixNearlyEqual(stalled_body_pose,Read<Matrix>(body.data(),0x34))) {
        std::cerr << "expired presentation reference ejected a tracked held body\n";
        return 171;
    }
    test_head_pose_valid=true;
    for (unsigned loss=0;loss<4;++loss) {
        test_head_pose_valid=false;
        if (loss==0) test_frame.focused=false;
        if (loss==1) hand.pose_valid=false;
        if (loss==2) hand.device_connected=false;
        if (loss==3) test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        if (g_held.load() || !Read<bool>(body.data(),0x3C8)) {
            std::cerr << "world-reference deferral masked a real release\n";
            return 172;
        }
        test_frame.focused=true;
        hand.pose_valid=hand.device_connected=true;
        test_head_pose_valid=true;
        test_leaves=0;
        begin();
        if (!g_held.load()) return 173;
    }
    test_active_calls=test_auto_freeze_calls=0;
    test_palm_resolver_services=0;
    test_palm_resolver_character_body=nullptr;
    test_palm_resolver_saw_held_body=false;
    hand.device_to_absolute.values[3]=0.2F;
    // Rework calls inherited iEntity::SetActive(true) every grab frame. Force
    // the exact BP field false here so the adapter must restore that state in
    // addition to waking the Newton body.
    Put(body.data(),kBodyActiveOffset,false);
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    if (test_palm_resolver_services!=1 ||
        test_palm_resolver_character_body!=character_body.data() ||
        !test_palm_resolver_saw_held_body) return 153;
    if (Read<Matrix>(body.data(),0x34).values[3]!=0.2F ||
        !Read<bool>(body.data(),kBodyActiveOffset) || test_native_updates) return 4;
    if (test_active_calls!=1 || !test_active_value ||
        test_auto_freeze_calls!=1 || test_auto_freeze_value) return 132;
    // Snap yaw changes world-space hand coordinates without a physical hand
    // teleport. A >35 cm palm displacement on a new yaw epoch keeps Grab.
    test_resolved_palm=runtime::IdentityMatrix();
    test_resolved_palm.values[3]=0.62F;
    test_resolved_palm_valid=true;
    ++test_palm_yaw_epoch;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    if (!g_held.load() ||
        std::abs(Read<Matrix>(body.data(),0x34).values[3]-0.62F)>0.001F)
        return 161;
    HookedGrabUpdate(state.data(),nullptr,0.016F); // stable historical samples
    // Rework samples the controller velocity at the release boundary. A quick
    // final throw gesture must not be replaced by the older hold-history median.
    hand.velocity={4,0,0};
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || test_leaves!=1 || Read<float>(body.data(),0x42C)!=3 ||
        Read<float>(body.data(),0x430)!=4 || Read<float>(body.data(),0x434)!=10 ||
        !Read<bool>(body.data(),0x428) || !Read<bool>(body.data(),0x3C8) ||
        Read<Vec>(body.data(),0x450)!=Vec{5.0F,0,0} || test_palm_held[1]!=nullptr) return 5;
    test_resolved_palm_valid=false;
    hand.velocity={2,0,0};
    // Bodies authored not to collide with characters must retain that policy.
    Put(body.data(),0x3C8,false); begin();
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || Read<bool>(body.data(),0x3C8)) return 21;
    Put(body.data(),0x3C8,true);

    // Rework excludes a newly held body before resolving the grab palm. BP's
    // resolver publishes asynchronously, so acquisition must wait for that
    // refreshed result rather than anchor to the collision-stopped old palm.
    const float collision_body_x=Read<Matrix>(body.data(),0x34).values[3];
    hand.device_to_absolute.values[3]=collision_body_x;
    test_resolved_palm=runtime::IdentityMatrix();
    test_resolved_palm.values[3]=collision_body_x-0.19F;
    test_resolved_palm_valid=true;
    begin();
    Matrix refreshed_grab_palm{}; Vec refreshed_velocity{},refreshed_angular{};
    if (!HandPose(runtime::VrHand::right,false,refreshed_grab_palm,
            refreshed_velocity,refreshed_angular) || !g_held.load() ||
        !VecNearlyEqual(g_hold.previous_palm,
            {refreshed_grab_palm.values[3],refreshed_grab_palm.values[7],
                refreshed_grab_palm.values[11]})) return 38;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load()) return 39;
    test_resolved_palm_valid=false;

    begin(); hand.pose_valid=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || Read<Vec>(body.data(),0x450)!=Vec{}) return 6;
    hand.pose_valid=true; begin(); ServiceSpatialInteraction(player.data(),true);
    if (g_held.load() || Read<Vec>(body.data(),0x450)!=Vec{}) return 7;
    test_joints=1; begin();
    if (g_held.load() || Read<float>(body.data(),0x42C)!=3 ||
        Read<int>(player.data(),0x2BC)!=0) return 8;
    if (test_native_updates!=0) return 9;
    // boat_bucket01 is a native Grab Object with two movable bodies linked
    // by an internal hinge. Accept either body, retain native joint ownership,
    // and consume Rework's slower jointed carry caps.
    {
        std::array<std::uint8_t,0x500> handle{};
        std::array<std::uint8_t,0x300> bucket{};
        std::array<std::uint8_t,0x180> bucket_joint{};
        void* parts[]{body.data(),handle.data()};
        const auto old_entity=Read<void*>(body.data(),kBodyUserDataOffset);
        Put(handle.data(),0,g_image+kPhysicsBodyVtable);
        Put(handle.data(),0x434,2.2F);
        Put(handle.data(),kBodyUserDataOffset,static_cast<void*>(bucket.data()));
        Put(body.data(),kBodyUserDataOffset,static_cast<void*>(bucket.data()));
        Put(bucket.data(),kEntityActiveOffset,true);
        Put(bucket.data(),kEntityTypeOffset,kObjectEntityType);
        Put(bucket.data(),kEntityBodiesBeginOffset,static_cast<void**>(parts));
        Put(bucket.data(),kEntityBodiesEndOffset,parts+2);
        Put(bucket_joint.data(),kJointParentBodyOffset,static_cast<void*>(handle.data()));
        Put(bucket_joint.data(),kJointChildBodyOffset,static_cast<void*>(body.data()));
        test_joint=bucket_joint.data();
        begin();
        if (!g_held.load() || Read<float>(body.data(),0x42C)!=4 ||
            Read<float>(body.data(),0x430)!=6) return 190;
        hand.velocity={12,0,0};
        test_matrix_writes=test_contact_impulses=0;
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (!g_hold.articulated || test_matrix_writes || !test_contact_impulses) {
            std::cerr << "two-body bucket bypassed its native hinge solver\n";
            return 191;
        }
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        if (g_held.load() || Read<float>(body.data(),0x42C)!=3 ||
            Read<float>(body.data(),0x430)!=4 ||
            Read<Vec>(body.data(),0x450)!=Vec{3,0,0}) return 192;
        // Selecting the other side of the internal hinge uses the same policy.
        Put(bucket_joint.data(),kJointParentBodyOffset,static_cast<void*>(body.data()));
        Put(bucket_joint.data(),kJointChildBodyOffset,static_cast<void*>(handle.data()));
        begin();
        if (!g_held.load()) return 197;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        Put(bucket_joint.data(),kJointParentBodyOffset,static_cast<void*>(handle.data()));
        Put(bucket_joint.data(),kJointChildBodyOffset,static_cast<void*>(body.data()));
        // Native Grab with a static/world endpoint takes the constrained
        // contact route. Foreign bodies and mechanism entity types stay out.
        Put(handle.data(),0x434,0.0F); begin();
        if (!g_held.load() || !g_hold.constrained) return 193;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        Put(handle.data(),0x434,2.2F);
        // The handle still enumerates this joint, so removing it as an
        // endpoint makes this an inconsistent graph, not a valid world hinge.
        Put(bucket_joint.data(),kJointParentBodyOffset,static_cast<void*>(nullptr)); begin();
        if (g_held.load()) return 194;
        Put(bucket_joint.data(),kJointParentBodyOffset,static_cast<void*>(handle.data()));
        Put(handle.data(),kBodyUserDataOffset,old_entity); begin();
        if (g_held.load()) return 195;
        Put(handle.data(),kBodyUserDataOffset,static_cast<void*>(bucket.data()));
        Put(bucket.data(),kEntityTypeOffset,kSwingDoorEntityType); begin();
        if (g_held.load()) return 196;
        // Skeletal Grab props enumerate inactive mass-zero animation colliders
        // beside their dynamic ragdoll bodies. They are not fixed joint anchors.
        Put(bucket.data(),kEntityTypeOffset,kObjectEntityType);
        std::array<std::uint8_t,0x500> companion{};
        Put(companion.data(),0,g_image+kPhysicsBodyVtable);
        Put(companion.data(),kBodyUserDataOffset,static_cast<void*>(bucket.data()));
        Put(companion.data(),kBodyActiveOffset,false);
        Put(companion.data(),kBodyCollideOffset,true);
        void* ragdoll_parts[]{body.data(),companion.data(),handle.data()};
        Put(bucket.data(),kEntityBodiesBeginOffset,static_cast<void**>(ragdoll_parts));
        Put(bucket.data(),kEntityBodiesEndOffset,ragdoll_parts+3);
        test_unjointed_companion=companion.data();
        begin();
        if (!g_held.load()) return 198;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        if (g_held.load() || Read<bool>(companion.data(),kBodyActiveOffset)) return 199;
        // Entity reactivation can wake all enumerated bodies, including the
        // unjointed animation colliders. Their non-character policy identifies
        // them without changing the native activation lifecycle.
        Put(companion.data(),kBodyActiveOffset,true); begin();
        if (!g_held.load()) return 205;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        if (!Read<bool>(companion.data(),kBodyActiveOffset)) return 206;
        Put(companion.data(),0x3C8,true); begin();
        if (g_held.load()) return 200;
        Put(companion.data(),0x3C8,false);
        test_joint_count_fault_body=companion.data(); begin();
        if (g_held.load()) return 207;
        test_joint_count_fault_body=nullptr;
        Put(companion.data(),kBodyActiveOffset,false);
        // An inactive collider still cannot be a constrained endpoint.
        Put(bucket_joint.data(),kJointParentBodyOffset,static_cast<void*>(companion.data()));
        begin();
        if (g_held.load()) return 201;
        test_unjointed_companion=nullptr;
        Put(body.data(),kBodyUserDataOffset,old_entity);
        test_joint=nullptr;
        hand.velocity={2,0,0};
    }
    // Native Grab can own world-hung or static-rooted Objects too (the mental
    // realm arms, cryo head and cables). Their constraints must survive VR
    // manipulation; they must never use the rigid SetMatrix route.
    for (const bool static_root : {false,true}) {
        std::array<std::uint8_t,0x500> middle{},root{},companion{};
        std::array<std::uint8_t,0x300> entity{};
        std::array<std::uint8_t,0x180> link{},anchor{};
        const auto old_entity=Read<void*>(body.data(),kBodyUserDataOffset);
        const auto old_pose=Read<Matrix>(body.data(),0x34);
        void* parts[]{body.data(),middle.data(),root.data(),companion.data()};
        Put(entity.data(),kEntityActiveOffset,true);
        Put(entity.data(),kEntityTypeOffset,kObjectEntityType);
        Put(entity.data(),kEntityBodiesBeginOffset,static_cast<void**>(parts));
        Put(entity.data(),kEntityBodiesEndOffset,parts+4);
        for (void* part : parts) {
            Put(part,0,g_image+kPhysicsBodyVtable);
            Put(part,kBodyUserDataOffset,static_cast<void*>(entity.data()));
        }
        Put(middle.data(),0x434,3.0F);
        Put(root.data(),0x434,static_root ? 0.0F : 4.0F);
        Put(root.data(),0x3C8,true);
        Put(link.data(),kJointParentBodyOffset,static_cast<void*>(middle.data()));
        Put(link.data(),kJointChildBodyOffset,static_cast<void*>(body.data()));
        Put(anchor.data(),kJointParentBodyOffset,static_root ? root.data() : nullptr);
        Put(anchor.data(),kJointChildBodyOffset,static_cast<void*>(middle.data()));
        test_joint_graph={{body.data(),{link.data()}},
            {middle.data(),{link.data(),anchor.data()}},
            {root.data(),static_root ? std::vector<void*>{anchor.data()} : std::vector<void*>{}},
            {companion.data(),{}}};
        const Vec untouched{7,8,9};
        Put(root.data(),0x450,untouched);
        Put(companion.data(),0x450,untouched);
        Matrix contact_palm{}; Vec contact_velocity{},contact_angular{};
        if (!InteractionHandPose(runtime::VrHand::right,contact_palm,
                contact_velocity,contact_angular)) return 248;
        g_interaction_targets[1]={TransformPoint(contact_palm,{0,0.06F,0}),
            body.data(),GetTickCount64(),true};
        begin();
        if (!g_held.load()) {
            std::cerr << "native anchored Grab rejected (static=" << static_root << ")\n";
            return 240;
        }
        Put(body.data(),0x450,Vec{});
        Put(body.data(),0x460,Vec{});
        hand.velocity={};
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (!VecNearlyEqual(test_impulse_delta,{})) {
            std::cerr << "stationary constrained acquisition pulled the mechanism\n";
            return 247;
        }
        hand.velocity={2,0,0};
        test_matrix_writes=test_contact_impulses=0;
        Put(body.data(),0x460,Vec{0.1F,0.2F,0.3F});
        hand.device_to_absolute.values[7]+=0.05F;
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (test_matrix_writes || !test_contact_impulses ||
            test_impulse_body!=body.data() || test_impulse_delta[1]<=0 ||
            !VecNearlyEqual(Read<Vec>(body.data(),0x460),{0.1F,0.2F,0.3F}) ||
            Read<Vec>(root.data(),0x450)!=untouched ||
            Read<Vec>(companion.data(),0x450)!=untouched) return 241;
        test_frame.input.state.interact.pressed=false;
        hand.velocity={12,0,0};
        const Vec solver_velocity{0.3F,0.2F,0.1F};
        Put(body.data(),0x450,solver_velocity);
        ServiceSpatialInteraction(player.data(),false);
        if (g_held.load() || Read<Vec>(body.data(),0x450)!=solver_velocity ||
            Read<float>(body.data(),0x42C)!=3 ||
            Read<float>(body.data(),0x430)!=4) return 242;
        hand.velocity={2,0,0};
        hand.device_to_absolute.values[7]-=0.05F;
        Put(body.data(),0x34,old_pose);
        Put(body.data(),kBodyUserDataOffset,old_entity);
        test_joint_graph.clear();
        g_interaction_targets={};
    }
    // Asset-backed free Grab families: food/trash bags (3), worm (4),
    // mattress/pillow (6), dog (14), Howard/gas victim (18). Skeletal loaders
    // enumerate one animation companion per physical bone, including 36 total
    // entries for the human corpses. Check every selectable physical member.
    for (const std::size_t count : {2U,3U,4U,6U,14U,18U}) {
        std::vector<std::array<std::uint8_t,0x500>> bones(count),colliders(count);
        std::vector<std::array<std::uint8_t,0x180>> joints(count-1);
        std::array<std::uint8_t,0x300> entity{};
        std::vector<void*> parts;
        Put(entity.data(),kEntityActiveOffset,true);
        Put(entity.data(),kEntityTypeOffset,kObjectEntityType);
        for (std::size_t index=0;index<count;++index) {
            for (auto* part : {bones[index].data(),colliders[index].data()}) {
                Put(part,0,g_image+kPhysicsBodyVtable);
                Put(part,kBodyUserDataOffset,static_cast<void*>(entity.data()));
                Put(part,kBodyActiveOffset,true);
                parts.push_back(part);
                test_joint_graph.push_back({part,{}});
            }
            Put(bones[index].data(),0x434,3.0F);
            if (index) {
                Put(joints[index-1].data(),kJointParentBodyOffset,bones[index-1].data());
                Put(joints[index-1].data(),kJointChildBodyOffset,bones[index].data());
                test_joint_graph[(index-1)*2].second.push_back(joints[index-1].data());
                test_joint_graph[index*2].second.push_back(joints[index-1].data());
            }
        }
        Put(entity.data(),kEntityBodiesBeginOffset,parts.data());
        Put(entity.data(),kEntityBodiesEndOffset,parts.data()+parts.size());
        for (auto& bone:bones) {
            bool articulated=false;
            if (!IsInternalGrabAssembly(bone.data(),&articulated) || !articulated) {
                std::cerr << "Grab assembly rejected: dynamic=" << count << '\n';
                return 220;
            }
        }
        // These are distinct native ownership families even with many bodies:
        // doors/chests/lockers, static-ended cables and world-hung props.
        Put(bones[0].data(),0x434,0.0F);
        bool articulated=false;
        if (IsInternalGrabAssembly(bones.back().data(),&articulated)) return 221;
        Put(bones[0].data(),0x434,3.0F);
        Put(joints[0].data(),kJointParentBodyOffset,static_cast<void*>(nullptr));
        if (IsInternalGrabAssembly(bones.back().data(),&articulated)) return 222;
        Put(joints[0].data(),kJointParentBodyOffset,bones[0].data());
        for (const int type : {kSwingDoorEntityType,kLeverEntityType,kWheelEntityType}) {
            Put(entity.data(),kEntityTypeOffset,type);
            if (IsInternalGrabAssembly(bones.back().data(),&articulated)) return 223;
        }
        test_joint_graph.clear();
    }
    // Exercise real Grab-family sizes through native Enter/Leave and palm
    // acquisition, including the 36-entry human ragdolls, not only the dog.
    for (const std::size_t bone_count : {2U,3U,4U,6U,14U,18U}) {
        std::vector<std::array<std::uint8_t,0x500>> bones(bone_count-1);
        std::vector<std::array<std::uint8_t,0x500>> colliders(bone_count);
        std::vector<std::array<std::uint8_t,0x180>> joints(bone_count-1);
        std::array<std::uint8_t,0x300> corpse{};
        std::vector<void*> parts(bone_count*2);
        const auto old_entity=Read<void*>(body.data(),kBodyUserDataOffset);
        const Matrix original_bone_pose=Read<Matrix>(body.data(),0x34);
        Put(corpse.data(),kEntityActiveOffset,true);
        Put(corpse.data(),kEntityTypeOffset,kObjectEntityType);
        Put(corpse.data(),kEntityHasInteractionOffset,true);
        for (std::size_t index=0;index<bone_count;++index) {
            void* bone=index==0 ? static_cast<void*>(body.data()) : bones[index-1].data();
            Put(bone,0,g_image+kPhysicsBodyVtable);
            Put(bone,0x34,original_bone_pose);
            Put(bone,0x434,3.0F);
            Put(bone,kBodyActiveOffset,true);
            Put(bone,kBodyCollideOffset,true);
            Put(bone,kBodyUserDataOffset,static_cast<void*>(corpse.data()));
            Put(colliders[index].data(),0,g_image+kPhysicsBodyVtable);
            Put(colliders[index].data(),kBodyUserDataOffset,static_cast<void*>(corpse.data()));
            parts[index*2]=bone;
            parts[index*2+1]=colliders[index].data();
            test_joint_graph.push_back({bone,{}});
            test_joint_graph.push_back({colliders[index].data(),{}});
        }
        for (std::size_t index=0;index<bone_count-1;++index) {
            Put(joints[index].data(),kJointParentBodyOffset,parts[index*2]);
            Put(joints[index].data(),kJointChildBodyOffset,parts[(index+1)*2]);
            test_joint_graph[index*2].second.push_back(joints[index].data());
            test_joint_graph[(index+1)*2].second.push_back(joints[index].data());
        }
        Put(corpse.data(),kEntityBodiesBeginOffset,parts.data());
        Put(corpse.data(),kEntityBodiesEndOffset,parts.data()+parts.size());
        begin();
        if (!g_held.load()) return 202;
        // A chain of connected dynamic bones must follow through Newton, not
        // a teleport of one bone that pulls the visible skeleton apart.
        test_matrix_writes=0;
        test_contact_impulses=0;
        const Vec sibling_velocity{0.1F,-0.2F,0.3F};
        const Vec companion_velocity{7,8,9};
        for (std::size_t index=1;index<bone_count;++index)
            Put(parts[index*2],0x450,sibling_velocity);
        for (auto& collider:colliders) Put(collider.data(),0x450,companion_velocity);
        std::array<std::uint8_t,0x500> detached{};
        Put(detached.data(),0,g_image+kPhysicsBodyVtable);
        Put(detached.data(),kBodyUserDataOffset,static_cast<void*>(corpse.data()));
        Put(detached.data(),0x434,3.0F);
        Put(detached.data(),0x450,companion_velocity);
        parts.push_back(detached.data());
        test_joint_graph.push_back({detached.data(),{}});
        Put(corpse.data(),kEntityBodiesBeginOffset,parts.data());
        Put(corpse.data(),kEntityBodiesEndOffset,parts.data()+parts.size());
        hand.device_to_absolute.values[3]+=0.04F;
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (!g_held.load() || test_matrix_writes!=0 ||
            test_contact_impulses==0 || test_impulse_body!=body.data() ||
            !FiniteVec(test_impulse_delta) ||
            !FiniteVec(test_impulse_position)) return 208;
        Vec expected_velocity=sibling_velocity;
        for (int axis=0;axis<3;++axis) expected_velocity[axis]+=test_impulse_delta[axis];
        for (std::size_t index=1;index<bone_count;++index)
            if (!VecNearlyEqual(Read<Vec>(parts[index*2],0x450),expected_velocity)) return 230;
        for (auto& collider:colliders)
            if (!VecNearlyEqual(Read<Vec>(collider.data(),0x450),companion_velocity)) return 231;
        if (!VecNearlyEqual(Read<Vec>(detached.data(),0x450),companion_velocity)) return 232;
        parts.pop_back();
        test_joint_graph.pop_back();
        Put(corpse.data(),kEntityBodiesEndOffset,parts.data()+parts.size());
        hand.device_to_absolute.values[3]-=0.04F;
        // Off-centre contact impulses also generate native angular motion.
        // The wrist servo may correct it only by its bounded angular step,
        // never overwrite the impulse with the pre-impulse velocity.
        Put(body.data(),0x460,Vec{});
        test_impulse_added_angular={0,0,1};
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        test_impulse_added_angular={};
        if (Read<Vec>(body.data(),0x460)[2]<0.79F) return 224;
        // A yaw epoch can rotate the virtual hand through the world while the
        // physical controller stays still. The ragdoll must rebase its contact
        // without receiving an artificial full-distance pull.
        hand.velocity={};
        Put(body.data(),0x450,Vec{});
        Put(body.data(),0x460,Vec{});
        test_resolved_palm=runtime::IdentityMatrix();
        test_resolved_palm.values[3]=0.65F;
        test_resolved_palm_valid=true;
        ++test_palm_yaw_epoch;
        test_contact_impulses=0;
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (!g_held.load() || test_contact_impulses!=1 ||
            std::hypot(std::hypot(test_impulse_delta[0],test_impulse_delta[1]),
                test_impulse_delta[2])>0.05F) return 209;
        // Smooth turning changes yaw epochs on consecutive ticks too. Keep
        // actual tracking-space lift/pull while rejecting the world rebase.
        for (int tick=0;tick<5;++tick) {
            hand.device_to_absolute.values[7]+=0.02F;
            hand.device_to_absolute.values[11]+=0.01F;
            const float yaw=static_cast<float>(tick+1)*0.2F;
            test_controller_world_basis.values[0]=test_controller_world_basis.values[10]=std::cos(yaw);
            test_controller_world_basis.values[2]=std::sin(yaw);
            test_controller_world_basis.values[8]=-std::sin(yaw);
            test_resolved_palm=runtime::Multiply(test_controller_world_basis,
                runtime::ExpandMatrix(hand.device_to_absolute));
            ++test_palm_yaw_epoch;
            HookedGrabUpdate(state.data(),nullptr,0.016F);
            if (!g_held.load() || test_impulse_delta[1]<0.10F ||
                test_impulse_delta[0]*std::sin(yaw)+
                    test_impulse_delta[2]*std::cos(yaw)<0.05F) return 226;
        }
        test_controller_world_basis=runtime::IdentityMatrix();
        Put(body.data(),0x460,Vec{});
        hand.device_to_absolute.values[0]=hand.device_to_absolute.values[5]=std::cos(0.2F);
        hand.device_to_absolute.values[1]=-std::sin(0.2F);
        hand.device_to_absolute.values[4]=std::sin(0.2F);
        test_resolved_palm=runtime::ExpandMatrix(hand.device_to_absolute);
        ++test_palm_yaw_epoch;
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (Read<Vec>(body.data(),0x460)[2]<0.10F) return 229;
        hand.device_to_absolute.values[0]=hand.device_to_absolute.values[5]=1;
        hand.device_to_absolute.values[1]=hand.device_to_absolute.values[4]=0;
        hand.device_to_absolute.values[7]-=0.10F;
        hand.device_to_absolute.values[11]-=0.05F;
        test_resolved_palm_valid=false;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        if (g_held.load()) return 203;
        // A missing presentation reference suspends VR driving without
        // erasing the motion Newton is still solving through the joints.
        begin();
        Put(body.data(),0x450,Vec{0.3F,-0.2F,0.1F});
        Put(body.data(),0x460,Vec{0,0.4F,0});
        test_head_pose_valid=false;
        test_matrix_writes=test_contact_impulses=0;
        HookedGrabUpdate(state.data(),nullptr,0.016F);
        if (!g_held.load() || test_matrix_writes || test_contact_impulses ||
            !VecNearlyEqual(Read<Vec>(body.data(),0x450),{0.3F,-0.2F,0.1F}) ||
            !VecNearlyEqual(Read<Vec>(body.data(),0x460),{0,0.4F,0})) return 214;
        test_head_pose_valid=true;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        // Exercise the palm picker before the native transition: a closer
        // animation collider cannot steal the physical bone's surface contact.
        std::array<std::uint8_t,0x100> world{};
        Put(world.data(),0,g_image+kPhysicsWorldVtable);
        TestPickCallback::VTable table{
            reinterpret_cast<bool(__thiscall*)(void*,void*)>(TestPickCallback::Before),
            reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(TestPickCallback::Intersect)};
        TestPickCallback callback{&table};
        // Repeat at head, middle and tail bones through overlap ranking,
        // native acceptance, prepared acquisition and Newton manipulation.
        for (std::size_t index : {std::size_t{0},bone_count/2,bone_count-1}) {
            void* const alternative=parts[((index+1)%bone_count)*2];
            test_resolved_palm=runtime::ExpandMatrix(hand.device_to_absolute);
            test_resolved_palm_valid=true;
            const Vec centre{test_resolved_palm.values[3],test_resolved_palm.values[7],
                test_resolved_palm.values[11]};
            const Vec contact{centre[0]+0.04F,centre[1],centre[2]};
            test_palm_overlap={};
            test_palm_overlap.valid=true;
            test_palm_overlap.hit_count=3;
            test_palm_overlap.hits[0]={parts[index*2],contact,1};
            test_palm_overlap.hits[1]={alternative,{centre[0]+0.08F,centre[1],centre[2]},1};
            test_palm_overlap.hits[2]={parts[index*2+1],centre,1};
            Put(parts[index*2+1],kBodyActiveOffset,true);
            Put(parts[index*2+1],kBodyCollideOffset,true);
            test_palm_overlap_available=true;
            PalmSelectionCandidate selected{};
            if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
                selected.body!=parts[index*2] || !VecNearlyEqual(selected.point,contact)) return 215;
            // Native veto, inactive and collision-disabled bones remain vetoes;
            // the next eligible dynamic contact wins instead.
            test_pick_rejected_body=parts[index*2];
            if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
                selected.body!=alternative) return 216;
            test_pick_rejected_body=nullptr;
            for (const auto offset : {kBodyActiveOffset,kBodyCollideOffset}) {
                Put(parts[index*2],offset,false);
                if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
                    selected.body!=alternative) return 217;
                Put(parts[index*2],offset,true);
            }
            if (!FindPalmOverlapTarget(world.data(),&callback,runtime::VrHand::right,selected) ||
                selected.body!=parts[index*2]) return 218;
            g_interaction_targets[1]={selected.point,selected.body,GetTickCount64(),true};
            Put(state.data(),0x20,selected.body);
            begin();
            if (!g_held.load() || !g_hold.articulated ||
                g_hold.body!=parts[index*2] ||
                !VecNearlyEqual(g_hold.local_contact,
                    InverseTransformPoint(original_bone_pose,contact))) return 211;
            test_matrix_writes=test_contact_impulses=0;
            hand.device_to_absolute.values[3]+=0.03F;
            HookedGrabUpdate(state.data(),nullptr,0.016F);
            if (!g_held.load() || test_matrix_writes!=0 ||
                test_contact_impulses!=1 || test_impulse_body!=parts[index*2])
                return 212;
            hand.device_to_absolute.values[3]-=0.03F;
            test_frame.input.state.interact.pressed=false;
            ServiceSpatialInteraction(player.data(),false);
            if (g_held.load()) return 213;
            Put(parts[index*2+1],kBodyActiveOffset,false);
        }
        test_palm_overlap_available=false;
        test_resolved_palm_valid=false;
        g_interaction_targets[1]={};
        Put(state.data(),0x20,body.data());
        hand.velocity={2,0,0};
        for (const auto& collider:colliders)
            if (Read<bool>(collider.data(),kBodyActiveOffset)) return 204;
        test_joint_graph.clear();
        Put(body.data(),kBodyUserDataOffset,old_entity);
    }
    test_joints=0; begin();
    hand.device_to_absolute.values[3]+=2;
    ++test_palm_yaw_epoch;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    if (g_held.load() || Read<Vec>(body.data(),0x450)!=Vec{}) return 10;

    // Grab must accept the VR-selected body even if the native screen/camera
    // contact is stale or on the wrong side of the prop.
    Put(body.data(),0x34,runtime::IdentityMatrix());
    hand.device_to_absolute.values[3]=0.0F;
    Put(state.data(),0x14,Vec{10.0F,0,0});
    Put(state.data(),0xE1,true);
    Matrix selected_pose{}; Vec selected_velocity{},selected_angular{};
    if (!InteractionHandPose(runtime::VrHand::right,selected_pose,
            selected_velocity,selected_angular)) return 140;
    const Vec selected_world_contact=TransformPoint(selected_pose,{
        runtime::vr_interaction_policy::kInteractionOffsetX,
        runtime::vr_interaction_policy::kInteractionOffsetY,
        runtime::vr_interaction_policy::kInteractionOffsetZ});
    AcquireSRWLockExclusive(&g_interaction_target_lock);
    g_interaction_targets[1]={selected_world_contact,body.data(),GetTickCount64(),true};
    ReleaseSRWLockExclusive(&g_interaction_target_lock);
    begin();
    if (!g_held.load()) return 140;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load()) return 141;
    AcquireSRWLockExclusive(&g_interaction_target_lock);
    g_interaction_targets[1]={};
    ReleaseSRWLockExclusive(&g_interaction_target_lock);
    Put(state.data(),0x14,Vec{});
    Put(state.data(),0xE1,false);

    // A fresh nearby VR winner is enough to acquire even when its exact
    // surface contact lies outside the palm box.
    Put(body.data(),0x34,runtime::IdentityMatrix());
    hand.device_to_absolute.values[3]=0.0F;
    Matrix broad_selected_pose{}; Vec broad_velocity{},broad_angular{};
    if (!InteractionHandPose(runtime::VrHand::right,broad_selected_pose,
            broad_velocity,broad_angular)) return 162;
    const Vec broad_world_contact=TransformPoint(broad_selected_pose,{0.28F,0,0});
    AcquireSRWLockExclusive(&g_interaction_target_lock);
    g_interaction_targets[1]={broad_world_contact,body.data(),GetTickCount64(),true};
    ReleaseSRWLockExclusive(&g_interaction_target_lock);
    begin();
    if (!g_held.load()) return 163;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    AcquireSRWLockExclusive(&g_interaction_target_lock);
    g_interaction_targets[1]={};
    ReleaseSRWLockExclusive(&g_interaction_target_lock);

    // Rework's acquisition volume is a box around the palm/fingers, not the
    // old 18 cm radial guard. A contact near a valid box corner can be farther
    // than 18 cm from the palm origin and must still enter the kinematic hold.
    const float body_x=Read<Matrix>(body.data(),0x34).values[3];
    hand.device_to_absolute.values[3]=body_x;
    Matrix interaction_volume_pose{}; Vec volume_velocity{},volume_angular{};
    if (!InteractionHandPose(runtime::VrHand::right,interaction_volume_pose,
            volume_velocity,volume_angular)) return 137;
    const Vec interaction_local_contact{
        runtime::vr_interaction_policy::kInteractionOffsetX+
            runtime::vr_interaction_policy::kInteractionSizeX*0.5F-0.001F,
        runtime::vr_interaction_policy::kInteractionOffsetY+
            runtime::vr_interaction_policy::kInteractionSizeY*0.5F-0.001F,
        runtime::vr_interaction_policy::kInteractionOffsetZ+
            runtime::vr_interaction_policy::kInteractionSizeZ*0.5F-0.001F};
    const Vec interaction_world_contact=
        TransformPoint(interaction_volume_pose,interaction_local_contact);
    const Matrix current_body_pose=Read<Matrix>(body.data(),0x34);
    Put(state.data(),0x14,
        InverseTransformPoint(current_body_pose,interaction_world_contact));
    begin();
    if (!g_held.load()) return 138;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    Matrix volume_palm{};
    if (!HandPose(runtime::VrHand::right,false,volume_palm,
            volume_velocity,volume_angular)) return 142;
    const auto held_body_origin=TransformPoint(
        Read<Matrix>(body.data(),0x34),Vec{});
    if (!VecNearlyEqual(held_body_origin,
            {volume_palm.values[3],volume_palm.values[7],volume_palm.values[11]},
            0.001F)) return 143;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load()) return 139;
    Put(state.data(),0x14,Vec{});

    // A fresh selected surface point is the grip contact. Requiem's tested
    // free-body path lets the player regrab a prop from a different point;
    // retaining the BP body-origin socket would erase that hand contact.
    Put(body.data(),0x34,runtime::IdentityMatrix());
    Put(state.data(),0x14,Vec{2.0F,0,0});
    Put(state.data(),0xE1,true);
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    Matrix anchored_selected_pose{};
    Vec anchored_selected_velocity{},anchored_selected_angular{};
    if (!InteractionHandPose(runtime::VrHand::right,anchored_selected_pose,
            anchored_selected_velocity,anchored_selected_angular)) return 148;
    const Vec selected_contact=TransformPoint(anchored_selected_pose,{0.04F,0,0});
    g_interaction_targets[1]={selected_contact,body.data(),GetTickCount64(),true};
    begin();
    if (!g_held.load()) return 149;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    Matrix selected_palm{};
    if (!HandPose(runtime::VrHand::right,false,selected_palm,
            anchored_selected_velocity,anchored_selected_angular)) return 150;
    const auto anchored_contact=TransformPoint(
        Read<Matrix>(body.data(),0x34),selected_contact);
    if (!VecNearlyEqual(anchored_contact,
            {selected_palm.values[3],selected_palm.values[7],
                selected_palm.values[11]},0.001F)) return 151;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load()) return 152;
    g_interaction_targets[1]={};
    Put(state.data(),0x14,Vec{});
    Put(state.data(),0xE1,false);
    Matrix restored_body_pose=runtime::IdentityMatrix();
    restored_body_pose.values[3]=body_x;
    Put(body.data(),0x34,restored_body_pose);

    // Acquisition still rejects a selected body beyond Rework's bounded raw
    // interaction reach even if the native state transition itself commits.
    hand.device_to_absolute.values[3]=body_x+0.25F;
    begin();
    if (g_held.load()) return 25;
    hand.device_to_absolute.values[3]=body_x;

    // If ChangeState advances without the expected Leave callback, the same
    // live player's registered grab-state proves that the held body is still
    // safe to restore once. Ownership must then be dropped so teardown cannot
    // be stranded behind a stale hold.
    begin();
    if (!g_held.load()) return 28;
    Put(player.data(),0x2BC,0);
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || !Read<bool>(body.data(),0x3C8)) return 29;

    // Player replacement invalidates hold ownership before old state/body
    // pointers are dereferenced. The old collision flag is deliberately not
    // restored because its lifetime is no longer demonstrated.
    begin();
    if (!g_held.load()) return 26;
    std::array<std::uint8_t,0x500> replacement_player{};
    std::array<void*,10> replacement_states{};
    Put(replacement_player.data(),0x2C4,replacement_states.data());
    ServiceSpatialInteraction(replacement_player.data(),false);
    if (g_held.load() || Read<bool>(body.data(),0x3C8)) return 27;
    Put(body.data(),0x3C8,true);
    ServiceSpatialInteraction(player.data(),false);

    // Black Plague's action-state 2 is a separate Move interaction. Rework
    // drives free Move bodies from the selected surface point to the tracked
    // palm with force.
    std::array<std::uint8_t,0x200> move_state{};
    states[2]=move_state.data();
    Put(move_state.data(),0x10,player.data()); Put(move_state.data(),0x54,body.data());
    Put(move_state.data(),0x38,Vec{});
    Put(body.data(),0x34,runtime::IdentityMatrix());
    Put(body.data(),0x42C,3.0F); Put(body.data(),0x430,4.0F); Put(body.data(),0x434,2.0F);
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    test_frame.interact_source=runtime::VrHand::right;
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    test_auto_freeze_calls=0;
    test_auto_freeze_value=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    test_palm_resolver_services=0;
    test_palm_resolver_character_body=nullptr;
    test_palm_resolver_saw_held_body=false;
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load() || Read<float>(body.data(),0x42C)!=10 ||
        Read<float>(body.data(),0x430)!=15 || test_palm_held[1]!=body.data() ||
        test_auto_freeze_calls!=1 || test_auto_freeze_value) return 32;
    if (test_palm_resolver_services!=1 ||
        test_palm_resolver_character_body!=character_body.data() ||
        !test_palm_resolver_saw_held_body) return 154;
    hand.device_to_absolute.values[3]=0.1F;
    HookedMoveUpdate(move_state.data(),nullptr,0.016F);
    if (test_palm_resolver_services!=2 ||
        test_palm_resolver_character_body!=character_body.data() ||
        !test_palm_resolver_saw_held_body) return 155;
    if (std::abs(test_move_force[0]-250.0F)>0.001F ||
        test_move_force[1]!=0 || test_move_force[2]!=0 ||
        test_move_force_position!=Vec{} || test_native_move_updates) return 33;
    // A snap turn is a world-yaw rebase, not a physical throw. Free Move
    // should carry the body through that rebase without an impulse or release.
    test_resolved_palm=runtime::IdentityMatrix();
    test_resolved_palm.values[3]=0.52F;
    test_resolved_palm_valid=true;
    ++test_palm_yaw_epoch;
    HookedMoveUpdate(move_state.data(),nullptr,0.016F);
    if (!g_move_held.load() ||
        std::abs(Read<Matrix>(body.data(),0x34).values[3]-0.42F)>0.001F ||
        std::abs(test_move_force[0])>0.001F) return 164;
    test_resolved_palm_valid=false;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load() || test_move_leaves!=1 ||
        Read<float>(body.data(),0x42C)!=3 || Read<float>(body.data(),0x430)!=4 ||
        test_palm_held[1]!=nullptr || test_auto_freeze_calls!=2 ||
        !test_auto_freeze_value) return 34;

    // Push is a distinct native state used by the tutorial's large crate.
    // VR-origin Push keeps the native state lifecycle but replaces its
    // camera/mouse force with the tracked palm, matching Rework's 300 N
    // horizontal hand delta and rebasing contact on snap yaw.
    std::array<std::uint8_t,0x200> push_state{};
    states[1]=push_state.data();
    Put(push_state.data(),0x10,player.data()); Put(push_state.data(),0x50,body.data());
    Put(body.data(),0x34,runtime::IdentityMatrix());
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    test_push_force={}; test_native_push_updates=0; test_push_leaves=0;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedPushEnter(push_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,1);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_push_held.load() || test_palm_held[1]!=body.data()) return 167;
    hand.device_to_absolute.values[3]=0.10F;
    HookedPushUpdate(push_state.data(),nullptr,0.016F);
    if (std::abs(test_push_force[0]-300.0F)>0.001F || test_push_force[1]!=0 ||
        test_push_force[2]!=0 || test_native_push_updates!=0) return 168;
    test_push_force={};
    test_resolved_palm=runtime::IdentityMatrix();
    test_resolved_palm.values[3]=0.52F;
    test_resolved_palm_valid=true;
    ++test_palm_yaw_epoch;
    HookedPushUpdate(push_state.data(),nullptr,0.016F);
    if (!g_push_held.load() || test_push_force!=Vec{}) return 169;
    const Vec push_release_velocity{0.1F,0,0.2F};
    Put(body.data(),0x450,push_release_velocity);
    Put(player.data(),0x50,true);
    test_resolved_palm_valid=false;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (test_native_push_release_impulses ||
        Read<Vec>(body.data(),0x450)!=push_release_velocity) {
        std::cerr << "VR Push release added the native camera launch impulse\n";
        return 249;
    }
    Put(player.data(),0x50,false);
    // Cover the real native virtual release dispatch, tracking-loss cleanup
    // and a VR transition cancelled before acquisition, not only service release.
    for (unsigned release_path=0;release_path<3;++release_path) {
        test_frame.input.state.interact.pressed=true;
        test_frame.input.state.interact.just_pressed=true;
        g_vr_selection_ready=true; g_vr_selection_player=player.data();
        HookedPushEnter(push_state.data(),nullptr,nullptr);
        Put(player.data(),0x2BC,1);
        Put(player.data(),0x50,true);
        Put(body.data(),0x450,push_release_velocity);
        if (release_path==2) ServiceSpatialInteraction(player.data(),true);
        else {
            ServiceSpatialInteraction(player.data(),false);
            if (!g_push_held.load()) return 251;
            if (release_path==0) HookedPushStopInteract(push_state.data(),nullptr);
            else {
                hand.pose_valid=false;
                ServiceSpatialInteraction(player.data(),false);
                hand.pose_valid=true;
            }
        }
        if (g_push_held.load() || test_native_push_release_impulses ||
            Read<int>(player.data(),0x2BC)!=0 ||
            Read<Vec>(body.data(),0x450)!=push_release_velocity) return 252;
    }
    // Native-origin mouse Push retains its original launch semantics.
    Put(player.data(),0x2BC,1);
    HookedPushStopInteract(push_state.data(),nullptr);
    if (test_native_push_release_impulses!=1 ||
        Read<Vec>(body.data(),0x450)[2]!=push_release_velocity[2]-6) return 253;
    Put(player.data(),0x50,false);
    test_frame.input.state.interact.pressed=false;
    if (g_push_held.load() || test_push_leaves!=5 || test_palm_held[1]!=nullptr)
        return 170;

    // Native-origin Push remains native.
    Put(player.data(),0x2BC,1);
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=false;
    test_native_push_updates=0;
    HookedPushUpdate(push_state.data(),nullptr,0.016F);
    if (test_native_push_updates!=1) return 171;
    Put(player.data(),0x2BC,0);

    // Move acquisition uses the same Rework palm/finger interaction box as
    // Grab. A valid contact near a box corner must not be rejected by the old
    // 18 cm radial guard after native Move=2 has already committed.
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    Matrix move_interaction_pose{}; Vec move_volume_velocity{},move_volume_angular{};
    if (!InteractionHandPose(runtime::VrHand::right,move_interaction_pose,
            move_volume_velocity,move_volume_angular)) return 140;
    const Vec move_local_contact{
        runtime::vr_interaction_policy::kInteractionOffsetX+
            runtime::vr_interaction_policy::kInteractionSizeX*0.5F-0.001F,
        runtime::vr_interaction_policy::kInteractionOffsetY+
            runtime::vr_interaction_policy::kInteractionSizeY*0.5F-0.001F,
        runtime::vr_interaction_policy::kInteractionOffsetZ+
            runtime::vr_interaction_policy::kInteractionSizeZ*0.5F-0.001F};
    const Vec move_world_contact=TransformPoint(move_interaction_pose,move_local_contact);
    Put(move_state.data(),0x38,
        InverseTransformPoint(Read<Matrix>(body.data(),0x34),move_world_contact));
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load()) return 141;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load()) return 142;
    Put(move_state.data(),0x38,Vec{});

    // The VR target that won selection is the contact Rework carries into its
    // Move interaction. Native Move=2 remains authoritative for lock/script
    // lifecycle, but a different legacy state contact must not shrink the
    // already-selected palm target back to a tiny activation point.
    g_interaction_targets[1]={move_world_contact,body.data(),GetTickCount64(),true};
    Put(move_state.data(),0x38,Vec{2.0F,0,0});
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load()) return 143;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load()) return 144;
    g_interaction_targets[1]={};
    Put(move_state.data(),0x38,Vec{});

    // A selected VR ray hit can be reachable without falling inside the
    // small palm contact box. Move must accept that same fresh 40 cm winner
    // after the native state has authorized the mechanism.
    g_interaction_targets[1]={Vec{0.30F,0,0},body.data(),GetTickCount64(),true};
    Put(move_state.data(),0x38,Vec{2.0F,0,0});
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load()) return 165;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load()) return 166;
    g_interaction_targets[1]={};
    Put(move_state.data(),0x38,Vec{});

    // Rework constrains no-joint Object drawers by deriving a slide axis from
    // the movable body to a static sibling body owned by the same entity. BP's
    // exact iGameEntity layout exposes mvBodies at +0x14C/+0x150. Once native
    // Move=2 has committed the Object, the Framework should consume that same
    // axis instead of treating the drawer as a free-body Move.
    {
        std::array<std::uint8_t,0x300> drawer_entity{};
        std::array<std::uint8_t,0x500> frame_body{};
        std::array<void*,2> drawer_bodies{body.data(),frame_body.data()};
        Put(drawer_entity.data(),kEntityActiveOffset,true);
        Put(drawer_entity.data(),kEntityTypeOffset,kObjectEntityType);
        Put(drawer_entity.data(),0x14C,drawer_bodies.data());
        Put(drawer_entity.data(),0x150,drawer_bodies.data()+drawer_bodies.size());
        Put(body.data(),kBodyUserDataOffset,static_cast<void*>(drawer_entity.data()));
        Put(frame_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(frame_body.data(),kBodyUserDataOffset,static_cast<void*>(drawer_entity.data()));
        Put(frame_body.data(),0x434,0.0F);
        Matrix drawer_pose=runtime::IdentityMatrix();
        drawer_pose.values[3]=0.40F;
        Matrix frame_pose=runtime::IdentityMatrix();
        Put(body.data(),0x34,drawer_pose);
        Put(frame_body.data(),0x34,frame_pose);
        Put(body.data(),0x42C,3.0F); Put(body.data(),0x430,4.0F);
        Put(body.data(),0x434,2.0F);
        Put(move_state.data(),0x38,Vec{});
        hand.device_to_absolute={{1,0,0,0.40F,0,1,0,0,0,0,1,0}};
        test_joints=0;
        test_frame.input.state.interact.pressed=true;
        test_frame.input.state.interact.just_pressed=true;
        g_vr_selection_ready=true; g_vr_selection_player=player.data();
        HookedMoveEnter(move_state.data(),nullptr,nullptr);
        Put(player.data(),0x2BC,2);
        ServiceSpatialInteraction(player.data(),false);
        if (!g_move_held.load() || g_move_hold.mode!=MoveHold::Mode::slider ||
            !VecNearlyEqual(g_move_hold.joint_pin,{1,0,0}) ||
            Read<float>(body.data(),0x42C)!=runtime::vr_mechanism_policy::kJointedMaximumLinearSpeed ||
            Read<float>(body.data(),0x430)!=runtime::vr_mechanism_policy::kJointedMaximumAngularSpeed)
            return 145;
        hand.device_to_absolute.values[3]=0.45F;
        hand.device_to_absolute.values[7]=0.10F;
        Put(body.data(),0x450,Vec{}); Put(body.data(),0x460,Vec{});
        HookedMoveUpdate(move_state.data(),nullptr,0.016F);
        const auto drawer_velocity=Read<Vec>(body.data(),0x450);
        if (drawer_velocity[0]<=0.0F || std::abs(drawer_velocity[1])>0.00001F ||
            std::abs(drawer_velocity[2])>0.00001F || test_native_move_updates!=0)
            return 146;
        test_frame.input.state.interact.pressed=false;
        ServiceSpatialInteraction(player.data(),false);
        if (g_move_held.load() || Read<float>(body.data(),0x42C)!=3.0F ||
            Read<float>(body.data(),0x430)!=4.0F) return 147;
        Put(body.data(),kBodyUserDataOffset,static_cast<void*>(nullptr));
        Put(body.data(),0x34,runtime::IdentityMatrix());
        Put(player.data(),0x2BC,0);
    }

    if (const int slider_result=RunNativeSliderRegression(
            player.data(),move_state.data(),body.data(),hand)) return slider_result;

    // A representative cGameLever with one recognized hinge consumes the
    // shared Rework servo while native Move Enter/Leave retain lifecycle
    // ownership (controllers, gravity and scripts).
    std::array<std::uint8_t,0x300> mechanism_entity{},joint{};
    Put(mechanism_entity.data(),kEntityActiveOffset,true);
    Put(mechanism_entity.data(),kEntityTypeOffset,kLeverEntityType);
    Put(body.data(),kBodyUserDataOffset,static_cast<void*>(mechanism_entity.data()));
    Put(joint.data(),0,g_image+kPhysicsJointHingeNewtonVtable);
    Put(g_image+kPhysicsJointHingeNewtonVtable,kJointTypeVtableSlot,g_image+kHingeGetType);
    Put(joint.data(),kJointPinDirectionOffset,Vec{0,1,0});
    Put(joint.data(),kJointPivotPointOffset,Vec{});
    test_joint=joint.data();
    test_joints=1;
    hand.device_to_absolute.values[3]=0;
    hand.device_to_absolute.values[11]=0;
    Put(move_state.data(),0x38,Vec{0.1F,0,0});
    Put(body.data(),0x450,Vec{}); Put(body.data(),0x460,Vec{});
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load() || g_move_hold.mode!=MoveHold::Mode::hinge ||
        std::abs(g_move_hold.hinge_lightness-1.35F)>0.00001F ||
        Read<float>(body.data(),0x42C)!=runtime::vr_mechanism_policy::kJointedMaximumLinearSpeed ||
        std::abs(Read<float>(body.data(),0x430)-10.8F)>0.0001F) return 35;
    hand.device_to_absolute.values[11]=0.05F;
    const Vec local_mass_centre{0.13F,0.2F,-0.04F};
    Put(body.data(),0x480,local_mass_centre);
    HookedMoveUpdate(move_state.data(),nullptr,0.016F);
    const auto mechanism_angular=Read<Vec>(body.data(),0x460);
    if (test_native_move_updates!=0 || std::abs(mechanism_angular[1]+6.75F)>0.001F ||
        std::abs(mechanism_angular[0])>0.001F || std::abs(mechanism_angular[2])>0.001F)
        return 36;
    const auto expected_com_velocity=Cross(mechanism_angular,local_mass_centre);
    if (!VecNearlyEqual(Read<Vec>(body.data(),0x450),expected_com_velocity)) {
        std::cerr << "hinge servo applied origin velocity at an offset centre of mass\n";
        return 250;
    }
    Put(body.data(),0x480,Vec{});
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load() || Read<float>(body.data(),0x42C)!=3 ||
        Read<float>(body.data(),0x430)!=4) return 49;

    // BP's Wheel uses the same native Move=2 transition as its lever. The
    // vice must consume the hand hinge servo after native authorization,
    // while bare contact must never activate or bypass the wheel lifecycle.
    Put(mechanism_entity.data(),kEntityTypeOffset,kWheelEntityType);
    MoveHold untouched_wheel{};
    if (BindRecognizedMechanism(body.data(),untouched_wheel)) return 170;
    hand.device_to_absolute.values[11]=0;
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load() || g_move_hold.mode!=MoveHold::Mode::hinge) return 171;
    hand.device_to_absolute.values[11]=0.05F;
    HookedMoveUpdate(move_state.data(),nullptr,0.016F);
    const auto wheel_angular=Read<Vec>(body.data(),0x460);
    if (std::abs(wheel_angular[1])<0.01F ||
        std::abs(wheel_angular[0])>0.001F || std::abs(wheel_angular[2])>0.001F)
        return 172;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load() || Read<float>(body.data(),0x42C)!=3 ||
        Read<float>(body.data(),0x430)!=4) return 173;

    // Once the native game has already committed an Object to Move=2, Rework
    // drives its recognized hinge/slider exactly like other Move mechanisms.
    // The native transition remains the authority for locks/scripts; this is
    // deliberately narrower than allowing arbitrary jointed Objects to nudge.
    Put(mechanism_entity.data(),kEntityTypeOffset,kObjectEntityType);
    Put(joint.data(),0,g_image+kPhysicsJointHingeNewtonVtable);
    hand.device_to_absolute.values[11]=0;
    Put(body.data(),0x450,Vec{}); Put(body.data(),0x460,Vec{});
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load() || g_move_hold.mode!=MoveHold::Mode::hinge)
        return 135;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load()) return 136;

    // Rework routes SwingDoor through the same Move hinge servo while the
    // entity owns controller/gravity lifecycle and hinge limits. Black Plague
    // consumes only the exact one-joint hinge form; a slider-shaped door must
    // fail closed rather than being generalized from Lever behavior.
    Put(mechanism_entity.data(),kEntityTypeOffset,kSwingDoorEntityType);
    hand.device_to_absolute.values[11]=0;
    Put(body.data(),0x450,Vec{}); Put(body.data(),0x460,Vec{});
    test_frame.input.state.interact.pressed=true;
    test_frame.input.state.interact.just_pressed=true;
    g_vr_selection_ready=true; g_vr_selection_player=player.data();
    HookedMoveEnter(move_state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,2);
    ServiceSpatialInteraction(player.data(),false);
    if (!g_move_held.load() || g_move_hold.mode!=MoveHold::Mode::hinge ||
        Read<float>(body.data(),0x42C)!=runtime::vr_mechanism_policy::kJointedMaximumLinearSpeed)
        return 50;
    hand.device_to_absolute.values[11]=0.05F;
    HookedMoveUpdate(move_state.data(),nullptr,0.016F);
    const auto door_angular=Read<Vec>(body.data(),0x460);
    if (test_native_move_updates!=0 || std::abs(door_angular[1]+6.75F)>0.001F ||
        std::abs(door_angular[0])>0.001F || std::abs(door_angular[2])>0.001F)
        return 51;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load() || Read<float>(body.data(),0x42C)!=3 ||
        Read<float>(body.data(),0x430)!=4) return 52;

    Put(joint.data(),0,g_image+kPhysicsJointSliderNewtonVtable);
    Put(g_image+kPhysicsJointSliderNewtonVtable,kJointTypeVtableSlot,g_image+kSliderGetType);
    MoveHold rejected_door{};
    if (BindRecognizedMechanism(body.data(),rejected_door)) return 53;

    // Rework selects the joint that actually constrains the grabbed body.
    // A mechanism body can also parent another joint, so joint 0 is not a
    // reliable drive joint. The exact BP joint layout exposes parent at +2C
    // and child at +30.
    std::array<std::uint8_t,0x180> parent_joint{},drive_joint{};
    std::array<std::uint8_t,0x500> linked_body{};
    Put(linked_body.data(),0,g_image+kPhysicsBodyVtable);
    Put(parent_joint.data(),0,g_image+kPhysicsJointHingeNewtonVtable);
    Put(parent_joint.data(),0x2C,static_cast<void*>(body.data()));
    Put(parent_joint.data(),0x30,static_cast<void*>(linked_body.data()));
    Put(parent_joint.data(),kJointPinDirectionOffset,Vec{1,0,0});
    Put(parent_joint.data(),kJointPivotPointOffset,Vec{});
    Put(drive_joint.data(),0,g_image+kPhysicsJointHingeNewtonVtable);
    Put(drive_joint.data(),0x2C,static_cast<void*>(linked_body.data()));
    Put(drive_joint.data(),0x30,static_cast<void*>(body.data()));
    Put(drive_joint.data(),kJointPinDirectionOffset,Vec{0,1,0});
    Put(drive_joint.data(),kJointPivotPointOffset,Vec{});
    Put(mechanism_entity.data(),kEntityTypeOffset,kLeverEntityType);
    test_joint=parent_joint.data();
    test_joint_secondary=drive_joint.data();
    test_joints=2;
    MoveHold multi_joint{};
    if (!BindRecognizedMechanism(body.data(),multi_joint) ||
        multi_joint.mode!=MoveHold::Mode::hinge ||
        !VecNearlyEqual(multi_joint.joint_pin,{0,1,0})) return 131;

    test_joint=nullptr; test_joint_secondary=nullptr; test_joints=0;
    Put(player.data(),0x2BC,0);

    const auto diagnostics=ConsumeSpatialDiagnostics();
    if (diagnostics.grabs_acquired<5 || diagnostics.grabs_released<5 ||
        diagnostics.moves_acquired<2 || diagnostics.moves_released<2 ||
        diagnostics.mechanism_acquired<2 || diagnostics.mechanism_updates<2 ||
        diagnostics.guarded_releases<3 || diagnostics.collision_restore_failures) return 22;
    // Failed teardown keeps the release path resident; the native tick drains it.
    begin(); std::string error;
    if (RemoveSpatialInteraction(error) || error.empty()) return 11;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || !RemoveSpatialInteraction(error)) return 12;
    // A rejected/uncommitted transition must not acquire a stale body.
    g_enabled.store(true); Put(player.data(),0x2BC,0);
    HookedEnter(state.data(),nullptr,nullptr);
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || g_pending_state) return 13;
    // Leaving during Enter cancels the pending acquisition.
    HookedEnter(state.data(),nullptr,nullptr);
    HookedLeave(state.data(),nullptr,nullptr);
    Put(player.data(),0x2BC,6);
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || g_pending_state) return 14;
    std::array<std::uint8_t,0x150> hands{},model{};
    Put(hands.data(),0x74,2); Put(hands.data(),0x6C,model.data());
    Put(model.data(),0x118,body.data());
    Put(model.data(),4,static_cast<const char*>("Flashlight"));
    Put(g_image,0x272138,reinterpret_cast<void*>(&NativeEqual));
    auto& left=test_frame.hands[0].grip;
    left=hand; left.device_to_absolute={{1,0,0,2,0,1,0,3,0,0,1,4}};
    g_updating_hands=hands.data();
    Matrix native=runtime::IdentityMatrix();
    // Installed emitter lies on model -Y; Rework's grip socket points toward
    // +Z. Check physical nodes independently of the production socket profile,
    // for both hands and a rotated wrist, rather than comparing a formula to itself.
    for (const auto tool_hand : {runtime::VrHand::left,runtime::VrHand::right}) {
        auto& sample=test_frame.hands[tool_hand==runtime::VrHand::left ? 0U : 1U].grip;
        const auto saved=sample;
        for (const float yaw : {0.0F,0.6F}) {
            sample.device_to_absolute={{std::cos(yaw),0,std::sin(yaw),2,
                0,1,0,3,-std::sin(yaw),0,std::cos(yaw),4}};
            test_frame.interact_source=tool_hand==runtime::VrHand::left ?
                runtime::VrHand::right : runtime::VrHand::left;
            HookedToolMatrix(body.data(),nullptr,&native);
            const auto tool=Read<Matrix>(body.data(),0x34);
            Matrix socket{}; Vec ignored_linear{},ignored_angular{};
            if (!ToolHandPose(tool_hand,kFlashlightGripRadius,
                    socket,ignored_linear,ignored_angular)) return 243;
            const auto grip=TransformPoint(tool,{0,-0.016669F,0});
            const auto emitter=TransformPoint(tool,{0,-0.103966F,0});
            const auto expected_emitter=TransformPoint(socket,{0,0,0.087297F});
            if (!VecNearlyEqual(grip,{socket.values[3],socket.values[7],socket.values[11]}) ||
                !VecNearlyEqual(emitter,expected_emitter)) {
                std::cerr << "flashlight emitter points behind the grip socket\n";
                return 244;
            }
            const auto ray=TransformPoint(tool,{0,-0.203767F,0});
            const Vec ray_to_emitter{ray[0]-emitter[0],ray[1]-emitter[1],ray[2]-emitter[2]};
            if (std::abs(std::hypot(ray_to_emitter[0],ray_to_emitter[1],ray_to_emitter[2])-
                    0.099801F)>0.00001F) return 245;
        }
        sample=saved;
    }
    test_frame.interact_source=runtime::VrHand::right;
    HookedToolMatrix(body.data(),nullptr,&native);
    auto attached=Read<Matrix>(body.data(),0x34);
    Matrix left_palm=runtime::IdentityMatrix();
    left_palm.values[3]=2; left_palm.values[7]=3; left_palm.values[11]=4;
    left_palm=runtime::rework_hand_profile::ApplyAttachmentGripLocalPose(
        left_palm,true,
        runtime::vr_interaction_policy::GripOpenCentreOffset(kFlashlightGripRadius));
    const Matrix expected_flashlight=
        runtime::ComposeAttachmentSocketPose(left_palm,kFlashlightSocket);
    if (!MatrixNearlyEqual(attached,expected_flashlight)) return 16;
    Put(model.data(),4,static_cast<const char*>("Glowstick"));
    HookedToolMatrix(body.data(),nullptr,&native);
    attached=Read<Matrix>(body.data(),0x34);
    left_palm=runtime::IdentityMatrix();
    left_palm.values[3]=2; left_palm.values[7]=3; left_palm.values[11]=4;
    left_palm=runtime::rework_hand_profile::ApplyAttachmentGripLocalPose(
        left_palm,true,
        runtime::vr_interaction_policy::GripOpenCentreOffset(kGlowstickGripRadius));
    const Matrix expected_glow=ReworkGlowstickPose(left_palm);
    if (!MatrixNearlyEqual(attached,expected_glow)) return 17;
    test_frame.interact_source=runtime::VrHand::left;
    hand.device_to_absolute={{1,0,0,7,0,1,0,8,0,0,1,9}};
    HookedToolMatrix(body.data(),nullptr,&native);
    attached=Read<Matrix>(body.data(),0x34);
    Matrix right_palm=runtime::IdentityMatrix();
    right_palm.values[3]=7; right_palm.values[7]=8; right_palm.values[11]=9;
    right_palm=runtime::rework_hand_profile::ApplyAttachmentGripLocalPose(
        right_palm,false,
        runtime::vr_interaction_policy::GripOpenCentreOffset(kGlowstickGripRadius));
    const Matrix expected_right_glow=ReworkGlowstickPose(right_palm);
    if (!MatrixNearlyEqual(attached,expected_right_glow)) return 23;
    test_frame.interact_source=runtime::VrHand::right;
    left.pose_valid=false;
    HookedToolMatrix(body.data(),nullptr,&native);
    if (Read<Matrix>(body.data(),0x34).values!=native.values) return 18;
    left.pose_valid=true; test_ui=true;
    HookedToolMatrix(body.data(),nullptr,&native);
    if (Read<Matrix>(body.data(),0x34).values!=native.values) return 19;
    test_ui=false; Put(model.data(),4,static_cast<const char*>("Hammer"));
    HookedToolMatrix(body.data(),nullptr,&native);
    if (Read<Matrix>(body.data(),0x34).values!=native.values) return 20;
    g_updating_hands=nullptr;
    {
        // A synthetic native caller reproduces the real AD852 return address;
        // this exercises HookedRay's production dispatcher without a test-only
        // branch. Forward the seven thiscall stack arguments to its fastcall
        // adapter, preserving the native callee's stack cleanup.
        constexpr std::array<std::uint8_t,35> caller{
            0x55,0x8B,0xEC,
            0xFF,0x75,0x20,0xFF,0x75,0x1C,0xFF,0x75,0x18,
            0xFF,0x75,0x14,0xFF,0x75,0x10,0xFF,0x75,0x0C,
            0xFF,0x75,0x08,0xE8,0,0,0,0,0x8B,0xE5,0x5D,
            0xC2,0x1C,0};
        std::memcpy(g_image+0xAD835,caller.data(),caller.size());
        const auto displacement=static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&HookedRay)-
            reinterpret_cast<std::uintptr_t>(g_image+0xAD852));
        std::memcpy(g_image+0xAD84E,&displacement,sizeof(displacement));
        Jump(0x189E30,reinterpret_cast<void*>(&NativeTestRay));
        Jump(0xA4B60,reinterpret_cast<void*>(&NativeTestPickClear));
        Jump(0xA5910,reinterpret_cast<void*>(&NativeTestPickResults));
        FlushInstructionCache(GetCurrentProcess(),g_image+0xAD835,caller.size());
        std::array<std::uint8_t,0x100> world{};
        std::array<std::uint8_t,0x500> save_body{};
        std::array<std::uint8_t,0x300> save_entity{};
        std::array<std::uint8_t,0x200> gaze_init{};
        std::array<std::uint8_t,0x400> gaze_player{},gaze_character{};
        std::array<std::uint8_t,0x40> gaze_normal{};
        std::array<void*,10> gaze_states{};
        Put(world.data(),0,g_image+kPhysicsWorldVtable);
        Put(save_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(save_body.data(),kBodyActiveOffset,true);
        Put(save_body.data(),kBodyUserDataOffset,
            static_cast<void*>(save_entity.data()));
        Put(save_entity.data(),0,g_image+0x2770B8);
        Put(save_entity.data(),kEntityActiveOffset,true);
        Put(save_entity.data(),kEntityTypeOffset,10);
        // SaveArea's native loader deliberately disables physical collision.
        Put(save_body.data(),kBodyCollideOffset,false);
        test_head_pose=runtime::IdentityMatrix();
        test_head_pose.values[7]=1.6F;
        test_head_pose_valid=true; test_resolved_palm_valid=false;
        test_palm_overlap_available=false;
        test_frame.focused=true;
        test_head_hits={{save_body.data(),1.5F,{0,1.6F,-1.5F}}};
        test_head_rays=0;
        TestPickCallback::VTable table{
            reinterpret_cast<bool(__thiscall*)(void*,void*)>(TestPickCallback::Before),
            reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(TestPickCallback::Intersect)};
        TestPickCallback callback; callback.vtable=&table;
        Put(gaze_init.data(),0x15C,static_cast<void*>(gaze_player.data()));
        Put(gaze_player.data(),0x27C,static_cast<void*>(&callback));
        Put(gaze_player.data(),0x274,static_cast<void*>(gaze_character.data()));
        Put(gaze_player.data(),0x1DC,true);
        Put(gaze_character.data(),0x48,Vec{0,1.6F,0});
        Put(save_entity.data(),0x10,static_cast<void*>(gaze_init.data()));
        Put(g_image+0x2770B8,0x44,reinterpret_cast<void*>(&NativeTestCrosshair));
        Put(g_image+0x2770B8,0x48,reinterpret_cast<void*>(&NativeTestEntityDistance));
        Put(g_image,0x272178,reinterpret_cast<void*>(&NativeTestWideNotEmpty));
        Put(g_image+0x2770B8,0x28,reinterpret_cast<void*>(&NativeTestSaveInteract));
        const Vec original_from{0,1,0},original_to{0,1,-10};
        g_enabled.store(true);
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        if (callback.body!=save_body.data() || test_head_rays!=1 ||
            !VecNearlyEqual(test_head_ray_from,{0,1.6F,0}) ||
            !VecNearlyEqual(test_head_ray_to,{0,1.6F,-10}) ||
            !VecNearlyEqual(callback.point,{0,1.6F,-1.5F})) return 181;
        // Native save range consumes the published point/body position, not
        // a short palm distance or Rework's source GetPickedDist()==0.
        NativeTestSaveInteract(save_entity.data(),nullptr);
        if (test_native_save_calls!=1) return 182;
        NativeTestPickClear(&callback,nullptr);
        test_head_hits={{save_body.data(),2.2F,{0,1.6F,-2.2F}}};
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        NativeTestSaveInteract(save_entity.data(),nullptr);
        if (test_native_save_calls!=1 ||
            !VecNearlyEqual(callback.point,{0,1.6F,-2.2F})) return 183;
        // Every native head hit participates, including unrelated foreground
        // geometry. Rejecting it must not expose a SaveArea behind it.
        std::array<std::uint8_t,0x500> foreground{};
        Put(foreground.data(),0,g_image+kPhysicsBodyVtable);
        Put(foreground.data(),kBodyActiveOffset,true);
        for (bool reversed : {false,true}) {
            NativeTestPickClear(&callback,nullptr);
            test_head_hits={{foreground.data(),0.5F,{0,1.6F,-0.5F}},
                {save_body.data(),1.5F,{0,1.6F,-1.5F}}};
            if (reversed) std::reverse(test_head_hits.begin(),test_head_hits.end());
            reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
                &original_from,&original_to,true,false,true,false);
            NativeTestPickResults(&callback,nullptr);
            if (callback.body || callback.area) return 184;
        }
        // Swanson's PC is a fixed Lamp (exact BP type 12), with a native
        // 2.1 m interaction callback. Interact must reach it across the desk
        // without admitting dynamic props into the distant activation path.
        Put(save_entity.data(),0,g_image+0x2763E8);
        Put(save_entity.data(),kEntityTypeOffset,12);
        Put(save_entity.data(),0xBC,2.1F);
        Put(save_body.data(),kBodyCollideOffset,true);
        Put(save_body.data(),0x434,0.0F);
        Put(g_image+0x2763E8,0x44,reinterpret_cast<void*>(&NativeTestCrosshair));
        Put(g_image+0x2763E8,0x48,reinterpret_cast<void*>(&NativeTestEntityDistance));
        Put(g_image+0x2763E8,0x28,reinterpret_cast<void*>(&NativeTestLampInteract));
        g_vr_selection_refresh_active=true;
        NativeTestPickClear(&callback,nullptr);
        test_head_hits={{save_body.data(),1.5F,{0,1.6F,-1.5F}}};
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        if (callback.body!=save_body.data()) return 260;
        using LampInteract=void(__thiscall*)(void*);
        const auto lamp_interact=reinterpret_cast<LampInteract>(
            Read<void*>(g_image+0x2763E8,0x28));
        lamp_interact(save_entity.data());
        if (test_native_lamp_calls!=1) return 261;
        NativeTestPickClear(&callback,nullptr);
        test_head_hits={{save_body.data(),2.2F,{0,1.6F,-2.2F}}};
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        lamp_interact(save_entity.data());
        if (test_native_lamp_calls!=1 ||
            !VecNearlyEqual(callback.point,{0,1.6F,-2.2F})) return 262;
        NativeTestPickClear(&callback,nullptr);
        test_head_hits={{foreground.data(),0.5F,{0,1.6F,-0.5F}},
            {save_body.data(),1.5F,{0,1.6F,-1.5F}}};
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        if (callback.body || callback.area) return 264;
        for (const bool dynamic_lamp : {true,false}) {
            Put(save_body.data(),0x434,dynamic_lamp ? 1.0F : 0.0F);
            Put(save_entity.data(),0,g_image+(dynamic_lamp ? 0x2763E8 : 0x2770B8));
            NativeTestPickClear(&callback,nullptr);
            test_head_hits={{save_body.data(),1.5F,{0,1.6F,-1.5F}}};
            reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
                &original_from,&original_to,true,false,true,false);
            if (callback.body || callback.area) return 263;
        }
        g_vr_selection_refresh_active=false;
        std::array<std::uint8_t,0x500> described_body{},hand_body{};
        std::array<std::uint8_t,0x300> described_entity{},hand_entity{};
        std::array<void*,20> described_table{};
        described_table[0x44/4]=reinterpret_cast<void*>(&NativeTestCrosshair);
        described_table[0x48/4]=reinterpret_cast<void*>(&NativeTestEntityDistance);
        Put(described_entity.data(),0,described_table.data());
        Put(described_entity.data(),0x10,static_cast<void*>(gaze_init.data()));
        Put(described_entity.data(),0xB8,2.0F);
        Put(described_entity.data(),0x98,true);
        Put(described_entity.data(),kEntityActiveOffset,true);
        Put(described_entity.data(),kEntityTypeOffset,kObjectEntityType);
        Put(described_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(described_body.data(),kBodyActiveOffset,true);
        Put(described_body.data(),kBodyCollideOffset,true);
        Put(described_body.data(),kBodyUserDataOffset,
            static_cast<void*>(described_entity.data()));
        test_head_hits={{described_body.data(),1.5F,{0,1.6F,-1.5F}}};
        NativeTestPickClear(&callback,nullptr);
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        if (callback.body!=described_body.data() ||
            Read<void*>(gaze_player.data(),0x27C)!=&callback) return 185;
        g_vr_selection_refresh_active=true;
        NativeTestPickClear(&callback,nullptr);
        reinterpret_cast<Ray>(g_image+0xAD835)(world.data(),&callback,
            &original_from,&original_to,true,false,true,false);
        g_vr_selection_refresh_active=false;
        if (callback.body || callback.area) return 186;
        // A physical target A must not suppress B's independent gaze cue or
        // redirect Interact to B. Explicit Examine owns only the gaze refresh.
        Put(hand_entity.data(),kEntityActiveOffset,true);
        Put(hand_entity.data(),kEntityTypeOffset,kObjectEntityType);
        Put(hand_entity.data(),kEntityHasInteractionOffset,true);
        Put(hand_body.data(),0,g_image+kPhysicsBodyVtable);
        Put(hand_body.data(),kBodyActiveOffset,true);
        Put(hand_body.data(),kBodyCollideOffset,true);
        Put(hand_body.data(),kBodyUserDataOffset,static_cast<void*>(hand_entity.data()));
        test_resolved_palm=test_head_pose;
        test_resolved_palm_valid=true;
        test_palm_overlap_available=true;
        test_palm_overlap={}; test_palm_overlap.valid=true;
        test_palm_overlap.hit_count=1;
        test_palm_overlap.hits[0]={hand_body.data(),{0.04F,1.6F,0},1};
        Put(gaze_normal.data(),0,g_image+0x27D138);
        Put(gaze_normal.data(),0x10,static_cast<void*>(gaze_player.data()));
        gaze_states[0]=gaze_normal.data();
        Put(gaze_player.data(),0x2C4,gaze_states.data());
        test_normal_world=world.data(); test_normal_callback=&callback;
        Jump(0xAD6C0,reinterpret_cast<void*>(&NativeTestNormalUpdate));
        const int updates_before=test_native_normal_calls;
        HookedNormalUpdate(gaze_normal.data(),nullptr,0.016F);
        if (test_native_normal_calls!=updates_before+1 ||
            callback.body!=hand_body.data() ||
            Read<int>(gaze_player.data(),0x2E4)!=4) return 187;
        RefreshVrSelectionBeforeInteract(gaze_player.data());
        if (callback.body!=hand_body.data() ||
            Read<int>(gaze_player.data(),0x2E4)!=1) return 188;
        // A valid palm contact may be far below the native body centre. BP's
        // body-origin distance must not veto a physically accepted VR prop.
        Jump(0x9BC90,reinterpret_cast<void*>(&NativeTestPickedDistance));
        Put(gaze_character.data(),0x48,Vec{0,3.1F,0});
        const float native_floor_distance=NativeTestPickedDistance(gaze_player.data(),nullptr);
        const float physical_floor_distance=HookedPhysicalPickedDistance(gaze_player.data(),nullptr);
        if (native_floor_distance<1.4F || !std::isfinite(physical_floor_distance) ||
            physical_floor_distance>runtime::vr_interaction_policy::kInteractionTargetDistance)
            return 191;
        const auto expect_native_distance=[&]() {
            return std::abs(HookedPhysicalPickedDistance(gaze_player.data(),nullptr)-
                native_floor_distance)<0.00001F;
        };
        g_gaze_distance_scope=true;
        if (!expect_native_distance()) return 192;
        g_gaze_distance_scope=false;
        g_vr_examine_selection_active=true;
        if (!expect_native_distance()) return 193;
        g_vr_examine_selection_active=false;
        test_frame.focused=false;
        if (!expect_native_distance()) return 194;
        test_frame.focused=true;
        const auto saved_target=g_interaction_targets[1];
        g_interaction_targets[1].time=GetTickCount64()-1000;
        if (!expect_native_distance()) return 195;
        g_interaction_targets[1]=saved_target;
        const auto saved_point=callback.point;
        callback.point[0]+=0.02F;
        if (std::abs(HookedPhysicalPickedDistance(gaze_player.data(),nullptr)-
                NativeTestPickedDistance(gaze_player.data(),nullptr))>0.00001F) return 196;
        callback.point=saved_point;
        Put(gaze_character.data(),0x48,Vec{0,1.6F,0});
        RefreshVrSelectionBeforeExamine(gaze_player.data());
        if (callback.body!=described_body.data() ||
            !VecNearlyEqual(callback.point,{0,1.6F,-1.5F}) ||
            g_interaction_targets[1].body!=hand_body.data()) return 189;
        test_head_pose_valid=false;
        RefreshVrSelectionBeforeExamine(gaze_player.data());
        if (callback.body || callback.area) return 190;
        test_head_pose_valid=true;
        test_palm_overlap_available=false; test_resolved_palm_valid=false;
        g_enabled.store(false);
        test_head_hits.clear();
    }
    {
        // Exercise the actual render transaction without any hands/tool update.
        // The billboard vector includes an already-hidden ray: restoration must
        // preserve that native state rather than turning everything back on.
        std::array<std::uint8_t,0x300> visibility_player{},light_helper{},visibility_init{};
        std::array<std::uint8_t,0x150> visibility_hands{},flash_model{},glow_model{};
        std::array<std::uint8_t,0x520> flash_mesh{},glow_mesh{},ray{},hidden_ray{},unrelated{};
        std::array<void*,2> rays{ray.data(),hidden_ray.data()};
        Put(visibility_player.data(),0x290,light_helper.data());
        Put(light_helper.data(),0x20,visibility_init.data());
        Put(visibility_init.data(),0x18C,visibility_hands.data());
        Put(visibility_hands.data(),0x74,2);
        Put(visibility_hands.data(),0x6C,flash_model.data());
        Put(visibility_hands.data(),0x70,glow_model.data());
        Put(flash_model.data(),4,static_cast<const char*>("Flashlight"));
        Put(glow_model.data(),4,static_cast<const char*>("Glowstick"));
        Put(flash_model.data(),0x118,flash_mesh.data());
        Put(glow_model.data(),0x118,glow_mesh.data());
        Put(flash_model.data(),0xC0,rays.data());
        Put(flash_model.data(),0xC4,rays.data()+rays.size());
        for (auto* mesh : {flash_mesh.data(),glow_mesh.data(),unrelated.data()}) {
            Put(mesh,0,g_image+0x2818F8); Put(mesh,0x3EC,true);
        }
        for (auto* billboard : {ray.data(),hidden_ray.data()})
            Put(billboard,0,g_image+0x282E88);
        Put(ray.data(),0x3EC,true);
        Put(g_image,0x272138,reinterpret_cast<void*>(&NativeEqual));
        const auto original_player=visibility_player;
        const auto original_light=light_helper;
        g_enabled.store(true);
        g_updating_hands=nullptr;
        {
            {
                ScopedEquippedLightMenuVisibility menu(visibility_player.data(),true);
                if (Read<bool>(flash_mesh.data(),0x3EC) || Read<bool>(glow_mesh.data(),0x3EC) ||
                    Read<bool>(ray.data(),0x3EC) || Read<bool>(hidden_ray.data(),0x3EC) ||
                    !Read<bool>(unrelated.data(),0x3EC)) {
                    std::cerr << "UI admits equipped light geometry without a tool callback\n";
                    return 250;
                }
                { ScopedEquippedLightMenuVisibility nested(visibility_player.data(),true); }
                if (Read<bool>(flash_mesh.data(),0x3EC)) return 251;
            }
            ScopedEquippedLightMenuVisibility gameplay(visibility_player.data(),false);
            if (!Read<bool>(flash_mesh.data(),0x3EC) || !Read<bool>(glow_mesh.data(),0x3EC) ||
                !Read<bool>(ray.data(),0x3EC) || Read<bool>(hidden_ray.data(),0x3EC) ||
                visibility_player!=original_player || light_helper!=original_light) return 252;
        }
        Put(glow_model.data(),4,static_cast<const char*>("Hammer"));
        { ScopedEquippedLightMenuVisibility menu(visibility_player.data(),true);
          if (!Read<bool>(glow_mesh.data(),0x3EC)) return 253; }
        g_enabled.store(false);
    }
    VirtualFree(g_image,0,MEM_RELEASE); g_image=nullptr;
    return 0;
}
}
int main() {
    const int result=penumbra_vr::backends::black_plague::RunSpatialTest();
    if (result) std::cerr<<"Spatial adapter regression: "<<result<<'\n';
    return result;
}
