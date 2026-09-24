// Exercise the actual exact-build adapter against a synthetic image. Native
// calls jump to typed fakes; this does not load or modify a game process.
#include "../../src/backends/black_plague/spatial_interaction.cpp"
#include <iostream>
#include <vector>
namespace penumbra_vr::backends::black_plague {
namespace {
runtime::VrControllerFrame test_frame;
int test_joints=0, test_leaves=0, test_native_updates=0;
int test_move_leaves=0, test_native_move_updates=0;
void* test_joint=nullptr;
void* test_joint_secondary=nullptr;
int test_active_calls=0, test_auto_freeze_calls=0;
bool test_active_value=false, test_auto_freeze_value=true;
Vec test_move_force{},test_move_force_position{};
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
int __fastcall JointCount(void*,void*) { return test_joints; }
void* __fastcall BodyJoint(void*,void*,int index) {
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
void __fastcall MaxLinear(void* body,void*,float value) { Put(body,0x42C,value); }
void __fastcall MaxAngular(void* body,void*,float value) { Put(body,0x430,value); }
void __fastcall Linear(void* body,void*,const Vec* value) { Put(body,0x450,*value); }
void __fastcall Angular(void* body,void*,const Vec* value) { Put(body,0x460,*value); }
void __fastcall Gravity(void* body,void*,bool value) { Put(body,0x428,value); }
void __fastcall Active(void*,void*,bool value) {
    ++test_active_calls;
    test_active_value=value;
}
void __fastcall AutoFreeze(void*,void*,bool value) {
    ++test_auto_freeze_calls;
    test_auto_freeze_value=value;
}
void __fastcall BodyMatrix(void* body,void*,const Matrix* value) { Put(body,0x34,*value); }
void __fastcall BodyForceAtPosition(void*,void*,const Vec* force,const Vec* position) {
    test_move_force=*force;
    test_move_force_position=*position;
}
struct TestPickCallback {
    struct VTable {
        bool (__thiscall *before)(void*,void*);
        bool (__thiscall *intersect)(void*,void*,void*);
    };
    VTable* vtable=nullptr;
    static bool __fastcall Before(void*,void*,void*) { return true; }
    static bool __fastcall Intersect(void*,void*,void*,void*) { return true; }
};
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
    if (!hand.device_connected || !hand.pose_valid) return false;
    pose=runtime::ExpandMatrix(hand.device_to_absolute); velocity=hand.velocity; angular=hand.angular_velocity; return true;
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
int RunSpatialTest() {
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
            !VecNearlyEqual(impulse,{0.44F,0,0}) ||
            std::abs(applied_delta-0.22F)>=0.00001F) return 40;
        NudgePlan large{{1,0,0},0.22F,0.38F,0.10F};
        if (!ComputeNudgeImpulse(hand_center,{1,0,0},large.velocity,contact,
                body_matrix,stationary,stationary,20.0F,large,impulse) ||
            !VecNearlyEqual(impulse,{2.0F,0,0})) return 41;
        NudgePlan door{{1,0,0},0.30F,0.30F,0.10F};
        if (!ComputeNudgeImpulse(hand_center,{1,0,0},door.velocity,contact,
                body_matrix,stationary,stationary,2.0F,door,impulse) ||
            !VecNearlyEqual(impulse,{0.20F,0,0})) return 42;
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
    Jump(0xCCF00,reinterpret_cast<void*>(&JointCount)); Jump(0xCA120,reinterpret_cast<void*>(&BodyMatrix));
    Jump(kGetBodyJoint,reinterpret_cast<void*>(&BodyJoint));
    Jump(kHingeGetType,reinterpret_cast<void*>(&HingeType));
    Jump(kSliderGetType,reinterpret_cast<void*>(&SliderType));
    Jump(0x19C360,reinterpret_cast<void*>(&MaxLinear)); Jump(0x19C380,reinterpret_cast<void*>(&MaxAngular));
    Jump(0x19C2A0,reinterpret_cast<void*>(&Linear)); Jump(0x19C2C0,reinterpret_cast<void*>(&Angular));
    Jump(0x19C590,reinterpret_cast<void*>(&Gravity));
    Jump(0x19C3F0,reinterpret_cast<void*>(&Active));
    Jump(0x19C450,reinterpret_cast<void*>(&AutoFreeze));
    Jump(0x19C9E0,reinterpret_cast<void*>(&BodyForceAtPosition));
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

    // Grab=6 must consume the VR winner for eligibility, while the final grip
    // pose uses the same deterministic body origin for either entry angle.
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
    const auto anchored_origin=TransformPoint(
        Read<Matrix>(body.data(),0x34),Vec{});
    if (!VecNearlyEqual(anchored_origin,
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
    HookedMoveUpdate(move_state.data(),nullptr,0.016F);
    const auto mechanism_angular=Read<Vec>(body.data(),0x460);
    if (test_native_move_updates!=0 || std::abs(mechanism_angular[1]+6.75F)>0.001F ||
        std::abs(mechanism_angular[0])>0.001F || std::abs(mechanism_angular[2])>0.001F)
        return 36;
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_move_held.load() || Read<float>(body.data(),0x42C)!=3 ||
        Read<float>(body.data(),0x430)!=4) return 49;

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
    VirtualFree(g_image,0,MEM_RELEASE); g_image=nullptr;
    return 0;
}
}
int main() {
    const int result=penumbra_vr::backends::black_plague::RunSpatialTest();
    if (result) std::cerr<<"Spatial adapter regression: "<<result<<'\n';
    return result;
}
