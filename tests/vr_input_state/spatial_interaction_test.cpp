// Exercise the actual exact-build adapter against a synthetic image. Native
// calls jump to typed fakes; this does not load or modify a game process.
#include "../../src/backends/black_plague/spatial_interaction.cpp"
#include <iostream>
#include <vector>
namespace penumbra_vr::backends::black_plague {
namespace {
runtime::VrControllerFrame test_frame;
int test_joints=0, test_leaves=0, test_native_updates=0;
bool test_ui=false;
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
void __fastcall MaxLinear(void* body,void*,float value) { Put(body,0x42C,value); }
void __fastcall MaxAngular(void* body,void*,float value) { Put(body,0x430,value); }
void __fastcall Linear(void* body,void*,const Vec* value) { Put(body,0x450,*value); }
void __fastcall Angular(void* body,void*,const Vec* value) { Put(body,0x460,*value); }
void __fastcall Gravity(void* body,void*,bool value) { Put(body,0x428,value); }
void __fastcall BodyMatrix(void* body,void*,const Matrix* value) { Put(body,0x34,*value); }
void Jump(std::uintptr_t rva,void* target) {
    auto* entry=g_image+rva;
    entry[0]=0xE9;
    const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target)-reinterpret_cast<std::uintptr_t>(entry+5));
    std::memcpy(entry+1,&delta,4);
    FlushInstructionCache(GetCurrentProcess(),entry,5);
}
}
runtime::VrControllerFrame ReadNativeControllerFrame() noexcept { return test_frame; }
void NativeControllerHaptic(runtime::VrHand,bool) noexcept {}
bool NativeInputUiActive() noexcept { return test_ui; }
bool ControllerWorldPose(const runtime::VrHmdPose& hand, Matrix& pose, Vec& velocity, Vec& angular) noexcept {
    if (!hand.device_connected || !hand.pose_valid) return false;
    pose=runtime::ExpandMatrix(hand.device_to_absolute); velocity=hand.velocity; angular=hand.angular_velocity; return true;
}
int RunSpatialTest() {
    g_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if (!g_image) return 1;
    Jump(0xAC900,reinterpret_cast<void*>(&NativeEnter)); Jump(0xAA4C0,reinterpret_cast<void*>(&NativeLeave));
    Jump(0xABA90,reinterpret_cast<void*>(&NativeUpdate)); Jump(0xA9FD0,reinterpret_cast<void*>(&NativeStop));
    Jump(0xCCF00,reinterpret_cast<void*>(&JointCount)); Jump(0xCA120,reinterpret_cast<void*>(&BodyMatrix));
    Jump(0x19C360,reinterpret_cast<void*>(&MaxLinear)); Jump(0x19C380,reinterpret_cast<void*>(&MaxAngular));
    Jump(0x19C2A0,reinterpret_cast<void*>(&Linear)); Jump(0x19C2C0,reinterpret_cast<void*>(&Angular));
    Jump(0x19C590,reinterpret_cast<void*>(&Gravity));
    std::array<std::uint8_t,0x500> body{},player{},state{};
    std::array<void*,10> states{}; states[6]=state.data();
    Put(player.data(),0x2C4,states.data());
    Put(state.data(),0x10,player.data()); Put(state.data(),0x20,body.data());
    Put(body.data(),0,g_image+0x292C08); Put(body.data(),0x34,runtime::IdentityMatrix());
    Put(body.data(),0x42C,3.0F); Put(body.data(),0x430,4.0F);
    Put(body.data(),0x3C8,true);
    test_frame.focused=true;
    auto& hand=test_frame.hands[1].grip;
    hand.pose_valid=true; hand.device_connected=true;
    hand.device_to_absolute={{1,0,0,0,0,1,0,0,0,0,1,0}};
    hand.velocity={2,0,0}; hand.angular_velocity={0,2,0};
    auto begin=[&] {
        g_enabled.store(true); Put(player.data(),0x2BC,0);
        test_frame.input.state.interact.pressed=true; test_frame.input.state.interact.just_pressed=true;
        HookedEnter(state.data(),nullptr,nullptr);
        Put(player.data(),0x2BC,6); // Native ChangeState commits only after Enter.
        ServiceSpatialInteraction(player.data(),false);
    };
    begin();
    if (g_held.load() || Read<float>(body.data(),0x42C)!=3) return 15;
    // Exercise acquisition after the exact-build installation gate has proved
    // the native collision field and all of its required consumers.
    g_player_collision_filter_ready.store(true);
    begin();
    if (!g_held.load() || Read<float>(body.data(),0x42C)!=20 || Read<bool>(body.data(),0x3C8)) return 2;
    HookedGrabUpdate(state.data(),nullptr,0);
    if (!g_held.load() || test_leaves) return 3;
    hand.device_to_absolute.values[3]=0.2F;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    if (Read<Matrix>(body.data(),0x34).values[3]!=0.2F || test_native_updates) return 4;
    HookedGrabUpdate(state.data(),nullptr,0.016F); // stable second release sample
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || test_leaves!=1 || Read<float>(body.data(),0x42C)!=3 ||
        Read<float>(body.data(),0x430)!=4 || Read<float>(body.data(),0x434)!=10 ||
        !Read<bool>(body.data(),0x428) || !Read<bool>(body.data(),0x3C8) ||
        Read<Vec>(body.data(),0x450)!=Vec{2.5F,0,0}) return 5;
    // Bodies authored not to collide with characters must retain that policy.
    Put(body.data(),0x3C8,false); begin();
    test_frame.input.state.interact.pressed=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || Read<bool>(body.data(),0x3C8)) return 21;
    Put(body.data(),0x3C8,true);
    begin(); hand.pose_valid=false;
    ServiceSpatialInteraction(player.data(),false);
    if (g_held.load() || Read<Vec>(body.data(),0x450)!=Vec{}) return 6;
    hand.pose_valid=true; begin(); ServiceSpatialInteraction(player.data(),true);
    if (g_held.load() || Read<Vec>(body.data(),0x450)!=Vec{}) return 7;
    test_joints=1; begin();
    if (g_held.load() || Read<float>(body.data(),0x42C)!=3) return 8;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    if (test_native_updates!=1) return 9;
    test_joints=0; begin();
    hand.device_to_absolute.values[3]+=2;
    HookedGrabUpdate(state.data(),nullptr,0.016F);
    if (g_held.load() || Read<Vec>(body.data(),0x450)!=Vec{}) return 10;
    const auto diagnostics=ConsumeSpatialDiagnostics();
    if (diagnostics.grabs_acquired<5 || diagnostics.grabs_released<5 ||
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
    if (attached.values[3]!=2 || attached.values[7]!=3 ||
        std::abs(attached.values[11]-4.016669F)>0.00001F || attached.values[9]!=1) return 16;
    Put(model.data(),4,static_cast<const char*>("Glowstick"));
    HookedToolMatrix(body.data(),nullptr,&native);
    attached=Read<Matrix>(body.data(),0x34);
    if (std::abs(attached.values[7]-3.00504F)>0.00001F ||
        std::abs(attached.values[11]-3.940278F)>0.00001F) return 17;
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
