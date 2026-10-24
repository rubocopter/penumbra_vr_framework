// Actual Requiem render/interaction consumer with typed native boundaries.
// No game, audio device, GL context or headset is created.
#include "../../src/backends/requiem/render_world.cpp"
#include <iostream>
#include <vector>

namespace penumbra_vr::backends::requiem {
namespace {
runtime::VrControllerFrame test_frame;
bool test_ui=false;
using TestVec=std::array<float,3>;
struct TestPick {
    struct Table {
        bool(__thiscall* before)(void*,void*);
        bool(__thiscall* intersect)(void*,void*,void*);
    };
    Table* table=nullptr;
    void* body=nullptr;
    void* area=nullptr;
    float maximum=10, distance=20, area_distance=20;
    int last_type=0;
    TestVec point{},area_point{};
    float minimum_static=20;
};
static_assert(sizeof(TestPick)==0x38);
struct SceneHit { void* body; float distance; TestVec point; };
std::vector<SceneHit> test_head_hits;
std::vector<SceneHit> test_hand_hits;
unsigned test_head_rays=0, test_native_rays=0;
TestVec test_last_from{},test_last_to{};

template<class T> void Put(void* object,std::size_t offset,const T& value) {
    std::memcpy(static_cast<std::uint8_t*>(object)+offset,&value,sizeof(value));
}
void JumpTo(std::uintptr_t site,void* target) {
    auto* at=g_image+site;
    at[0]=0xE9;
    const auto delta=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(target)-
        reinterpret_cast<std::intptr_t>(at+5));
    std::memcpy(at+1,&delta,sizeof(delta));
}
void __fastcall NativeClear(void* value,void*) {
    auto& pick=*static_cast<TestPick*>(value);
    pick.body=nullptr; pick.area=nullptr;
    pick.distance=pick.area_distance=pick.minimum_static=pick.maximum+10;
}
void __fastcall NativeResolve(void* value,void*) {
    auto& pick=*static_cast<TestPick*>(value);
    if(pick.area && pick.area_distance<pick.distance) {
        pick.body=pick.area; pick.distance=pick.area_distance; pick.point=pick.area_point;
    }
}
bool __fastcall NativeBefore(void*,void*,void* body) { return body!=nullptr; }
bool __fastcall NativeIntersect(void* value,void*,void* body,void* raw) {
    auto& pick=*static_cast<TestPick*>(value);
    const auto& hit=*static_cast<RankedRayCallback::Hit*>(raw);
    void* entity=ReadNative<void*>(body,0x414);
    // Independently mapped native solid/area arbitration, not VR ranking.
    if(entity && ReadNative<int>(entity,0xC0)==13) {
        if(hit.distance<pick.area_distance) {
            pick.area=body; pick.area_distance=hit.distance; pick.area_point=hit.point;
        }
    } else if(hit.distance<pick.distance) {
        pick.body=entity ? body : nullptr; pick.distance=hit.distance; pick.point=hit.point;
    }
    return true;
}
void __fastcall NativeRay(void*,void*,void* callback,const TestVec* from,
    const TestVec* to,bool,bool,bool,bool) {
    ++test_native_rays;
    const bool head=(*from)[1]>=1.5F;
    if(head) { ++test_head_rays; test_last_from=*from; test_last_to=*to; }
    auto* table=ReadNative<TestPick::Table*>(callback,0);
    for(const auto& hit:head ? test_head_hits : test_hand_hits) {
        if(!table->before(callback,hit.body)) continue;
        RankedRayCallback::Hit native_hit{};
        native_hit.distance=hit.distance; native_hit.point=hit.point;
        if(!table->intersect(callback,hit.body,&native_hit)) break;
    }
}
float __fastcall NativeEntityDistance(void* entity,void*) {
    auto* player=ReadNative<void*>(ReadNative<void*>(entity,0x10),0x15C);
    auto* pick=ReadNative<TestPick*>(player,0x280);
    const auto body=ReadNative<TestVec>(ReadNative<void*>(player,0x278),0x48);
    return std::hypot(pick->point[0]-body[0],pick->point[1]-body[1],pick->point[2]-body[2]);
}
bool __cdecl NativeNotEmpty(const void* value,const std::uint16_t*) {
    return ReadNative<bool>(value,0);
}
void __fastcall NativeLadderPick(void* entity,void*) {
    auto* player=ReadNative<void*>(ReadNative<void*>(entity,0x10),0x15C);
    auto* pick=ReadNative<TestPick*>(player,0x280);
    const auto body=ReadNative<TestVec>(ReadNative<void*>(player,0x278),0x48);
    // Native 38520 uses the first entity body origin for horizontal distance;
    // only the vertical guard uses the picked point (387AA..387BF).
    auto* first_body=ReadNative<void*>(ReadNative<void*>(entity,0x14C),0);
    const auto origin=ReadNative<runtime::VrMatrix44>(first_body,0x34);
    if(std::hypot(origin.values[3]-body[0],origin.values[11]-body[2])<=1.4F &&
        std::abs(pick->point[1]-body[1])<1.1F) Put(player,0x2E8,9);
}
constexpr std::uintptr_t test_ray_caller=kNormalRayCallRva+3-5-28;
void __fastcall NativeNormalUpdate(void* state,void*,float) {
    auto* player=ReadNative<void*>(state,0x10);
    auto* pick=ReadNative<TestPick*>(player,0x280);
    NativeClear(pick,nullptr);
    const TestVec from{0,1,0},to{0,1,-10};
    reinterpret_cast<Ray>(g_image+test_ray_caller)(nullptr,pick,&from,&to,true,false,true,false);
    NativeResolve(pick,nullptr);
    Put(player,0x2E8,10);
    auto* entity=ReadNative<void*>(pick->body,0x414);
    if(entity) {
        const float distance=NativeEntityDistance(entity,nullptr);
        const bool interact=ReadNative<bool>(entity,0x37) && distance<=ReadNative<float>(entity,0xBC);
        const bool examine=ReadNative<bool>(entity,0x98) && distance<=ReadNative<float>(entity,0xB8) &&
            !(ReadNative<bool>(entity,0x38) && ReadNative<bool>(entity,0xB4));
        Put(player,0x2E8,interact ? 1 : examine ? 4 : 10);
        if(ReadNative<int>(entity,0xC0)==13) NativeLadderPick(entity,nullptr);
    }
}
bool Near(float a,float b) { return std::abs(a-b)<0.0001F; }
bool Failed(int line) { std::cerr<<"Requiem selection boundary failed at line "<<line<<"\n"; return false; }
bool SelectionCases() {
    std::array<std::uint8_t,0x500> body{};
    std::array<std::uint8_t,0x300> entity{};
    std::array<std::uint8_t,0x400> player{},character{};
    std::array<std::uint8_t,0x200> init{};
    TestPick::Table table{
        reinterpret_cast<bool(__thiscall*)(void*,void*)>(NativeBefore),
        reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(NativeIntersect)};
    TestPick pick; pick.table=&table;
    Put(body.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(body.data(),0x414,static_cast<void*>(entity.data()));
    Put(entity.data(),0,g_image+0x270100);
    Put(entity.data(),0x14,true); Put(entity.data(),0xC0,1);
    Put(entity.data(),0x98,true); Put(entity.data(),0xB8,2.1F);
    Put(entity.data(),0x10,static_cast<void*>(init.data()));
    Put(init.data(),0x15C,static_cast<void*>(player.data()));
    Put(player.data(),0x280,static_cast<void*>(&pick));
    Put(player.data(),0x278,static_cast<void*>(character.data()));
    Put(character.data(),0x48,TestVec{0,1.0F,0});
    Put(g_image+0x270100,0x48,reinterpret_cast<void*>(&NativeEntityDistance));
    Put(g_image,0x27312C,reinterpret_cast<void*>(&NativeNotEmpty));
    JumpTo(0xA5000,reinterpret_cast<void*>(&NativeClear));
    JumpTo(0xA5D50,reinterpret_cast<void*>(&NativeResolve));
    JumpTo(kCastRayRva,reinterpret_cast<void*>(&NativeRay));
    // Seven forwarded arguments; the inner call returns at the exact native
    // ADCD2 boundary checked by HookedRay. This preserves thiscall stack ABI.
    constexpr std::uintptr_t caller=kNormalRayCallRva+3-5-28;
    for(unsigned n=0;n<7;++n) {
        const std::array<std::uint8_t,4> push{0xFF,0x74,0x24,0x1C};
        std::memcpy(g_image+caller+n*4,push.data(),push.size());
    }
    auto* call=g_image+kNormalRayCallRva+3-5;
    call[0]=0xE8;
    const auto delta=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(&HookedRay)-
        reinterpret_cast<std::intptr_t>(call+5));
    std::memcpy(call+1,&delta,sizeof(delta));
    const std::array<std::uint8_t,3> ret{0xC2,0x1C,0};
    std::memcpy(call+5,ret.data(),ret.size());
    FlushInstructionCache(GetCurrentProcess(),g_image,0x300000);
    test_frame.focused=true; test_frame.interact_source=runtime::VrHand::right;
    g_presenting.store(true);
    g_head_world_pose=runtime::IdentityMatrix(); g_head_world_pose.values[7]=1.7F;
    g_head_world_pose_valid=true; g_head_world_pose_sampled_at_ms=GetTickCount64();
    const TestVec native_from{0,1,0},native_to{0,1,-10};
    test_head_hits={{body.data(),1.8F,{0,1.7F,-1.8F}}};
    NativeClear(&pick,nullptr);
    reinterpret_cast<Ray>(g_image+caller)(nullptr,&pick,&native_from,&native_to,true,false,true,false);
    if(pick.body!=body.data() || test_head_rays!=1 || !Near(test_last_from[1],1.7F)) {
        std::cerr<<"Head-only Examine target lost when hand/native camera is away\n";
        return Failed(__LINE__);
    }
    if(ReadNative<void*>(player.data(),0x280)!=&pick) return Failed(__LINE__);
    // The actual Normal update owner keeps physical hand selection while
    // presenting the independently examined head target's unexamined cue.
    std::array<std::uint8_t,0x40> normal{};
    std::array<void*,10> states{};
    Put(normal.data(),0,g_image+kNormalVtableRva);
    Put(normal.data(),0x10,static_cast<void*>(player.data()));
    states[0]=normal.data(); Put(player.data(),0x2C8,static_cast<void*>(states.data()));
    Put(player.data(),0x2C0,0);
    JumpTo(kNormalUpdateRva,reinterpret_cast<void*>(&NativeNormalUpdate));
    Put(g_image+kNormalVtableRva,4,g_image+kNormalUpdateRva);
    Put(g_image,kRaySlotRva,g_image+kCastRayRva);
    std::string error;
    if(!hooks::InstallPointerHook(reinterpret_cast<void**>(g_image+kNormalVtableRva+4),
            g_image+kNormalUpdateRva,reinterpret_cast<void*>(&HookedNormalUpdate),g_normal_update_hook,error) ||
       !hooks::InstallPointerHook(reinterpret_cast<void**>(g_image+kRaySlotRva),
            g_image+kCastRayRva,reinterpret_cast<void*>(&HookedRay),g_ray_hook,error)) return Failed(__LINE__);
    const auto update=[&] {
        g_head_world_pose_sampled_at_ms=GetTickCount64();
        reinterpret_cast<HandsUpdate>(ReadNative<void*>(g_image+kNormalVtableRva,4))(normal.data(),0);
    };
    std::array<std::uint8_t,0x500> hand_body{},foreground{};
    std::array<std::uint8_t,0x300> hand_entity{};
    Put(hand_body.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(hand_body.data(),0x414,static_cast<void*>(hand_entity.data()));
    Put(hand_entity.data(),0x10,static_cast<void*>(init.data()));
    Put(hand_entity.data(),0x14,true); Put(hand_entity.data(),0xC0,1);
    Put(hand_entity.data(),0x37,true); Put(hand_entity.data(),0xBC,2.1F);
    auto& grip=test_frame.hands[1].grip;
    grip.device_connected=true; grip.pose_valid=true;
    grip.device_to_absolute.values={1,0,0,0, 0,1,0,0.8F, 0,0,1,0};
    g_world_from_tracking=runtime::IdentityMatrix();
    g_world_from_tracking_time=GetTickCount64();
    test_hand_hits={{hand_body.data(),0.2F,{0,0.8F,-0.2F}}};
    update();
    if(pick.body!=hand_body.data() || ReadNative<int>(player.data(),0x2E8)!=4) {
        std::cerr<<"Independent gaze cue displaced physical hand target\n"; return Failed(__LINE__);
    }
    Put(entity.data(),0x38,true); Put(entity.data(),0xB4,false); update();
    if(pick.body!=hand_body.data() || ReadNative<int>(player.data(),0x2E8)!=4) {
        std::cerr<<"Repeatable Examine cue lost against physical hand target\n"; return Failed(__LINE__);
    }
    Put(entity.data(),0x38,false);
    RefreshVrSelectionBeforeExamine(player.data());
    if(pick.body!=body.data() || g_vr_selection_player || ReadNative<void*>(player.data(),0x280)!=&pick) {
        std::cerr<<"Examine action did not refresh its own head target\n"; return Failed(__LINE__);
    }
    // A described loose prop cannot become a distant VR grab on Interact.
    test_hand_hits.clear();
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body || g_vr_selection_player) return Failed(__LINE__);
    // Native Examine range and HasBeenExamined semantics remain authoritative.
    // RQ ADD10..ADD24 hides an examined cue only for descriptions marked once.
    Put(entity.data(),0x38,true); Put(entity.data(),0xB4,true); update();
    if(ReadNative<int>(player.data(),0x2E8)!=10) return Failed(__LINE__);
    Put(entity.data(),0xB4,false); update();
    if(ReadNative<int>(player.data(),0x2E8)!=4) return Failed(__LINE__);
    Put(entity.data(),0x38,false);
    test_head_hits={{body.data(),2.2F,{0,1.7F,-2.2F}}}; update();
    if(pick.body || ReadNative<int>(player.data(),0x2E8)!=10) return Failed(__LINE__);
    // Native solid/area arbitration must keep foreground geometry opaque,
    // irrespective of native callback hit order or absent entity user data.
    test_head_hits={{body.data(),1.0F,{0,1.7F,-1}}, {foreground.data(),0.4F,{0,1.7F,-0.4F}}};
    for(bool reversed:{false,true}) {
        if(reversed) std::reverse(test_head_hits.begin(),test_head_hits.end());
        update(); if(pick.body || ReadNative<int>(player.data(),0x2E8)!=10) return Failed(__LINE__);
    }
    // Physical HMD yaw changes the head ray; no actor/body yaw is used.
    test_head_hits={{body.data(),1.0F,{-1,1.7F,0}}};
    g_head_world_pose.values={0,0,1,0, 0,1,0,1.7F, -1,0,0,0, 0,0,0,1};
    update();
    if(!Near(test_last_to[0],-10) || !Near(test_last_to[2],0)) return Failed(__LINE__);
    g_head_world_pose=runtime::IdentityMatrix(); g_head_world_pose.values[7]=1.7F;
    // Ladder's own callback retains its 1.4 m horizontal / 1.1 m vertical
    // bounds. The native area winner can be activated without a palm hit.
    Put(entity.data(),0,g_image+kLadderVtableRva); Put(entity.data(),0xC0,13);
    void* ladder_body=body.data(); Put(entity.data(),0x14C,static_cast<void*>(&ladder_body));
    JumpTo(kLadderPickRva,reinterpret_cast<void*>(&NativeLadderPick));
    for(const auto& limits: {TestVec{1.3F,1.7F,-2.0F},TestVec{1.41F,1.7F,-0.2F},TestVec{1.0F,2.2F,-0.2F}}) {
        const TestVec point{0,limits[1],limits[2]};
        auto origin=runtime::IdentityMatrix(); origin.values[11]=-limits[0];
        Put(body.data(),0x34,origin);
        test_head_hits={{body.data(),1.3F,point}};
        RefreshVrSelectionBeforeInteract(player.data());
        const bool allowed=Near(limits[0],1.3F);
        if(pick.body!=body.data() || (ReadNative<int>(player.data(),0x2E8)==9)!=allowed) {
            std::cerr<<"Ladder native cue/range changed\n"; return Failed(__LINE__);
        }
    }
    // Nonphysical head selection cannot see a ladder through a solid wall.
    test_head_hits={{body.data(),1.0F,{0,1.7F,-1}}, {foreground.data(),0.4F,{0,1.7F,-0.4F}}};
    update(); if(pick.body || ReadNative<int>(player.data(),0x2E8)!=10) return Failed(__LINE__);
    // Focus/UI and stale head data leave the native callback route intact.
    test_head_hits={{body.data(),1.0F,{0,1.7F,-1}}};
    for(int gate=0;gate<3;++gate) {
        test_frame.focused=gate!=0; test_ui=gate==1; g_head_world_pose_valid=gate!=2;
        update(); if(pick.body) return Failed(__LINE__);
    }
    test_frame.focused=true; test_ui=false; g_head_world_pose_valid=true;
    if(!hooks::RemoveIatHook(g_normal_update_hook,error) ||
        !hooks::RemoveIatHook(g_ray_hook,error) ||
        ReadNative<void*>(g_image+kNormalVtableRva,4)!=g_image+kNormalUpdateRva) return Failed(__LINE__);
    return true;
}
}
runtime::VrControllerFrame ReadNativeControllerFrame() noexcept { return test_frame; }
bool NativeUiActive() noexcept { return test_ui; }
NativeUiSurface CurrentNativeUiSurface() noexcept { return NativeUiSurface::none; }
RequiemBodyTrackingSample ReadGameplayTrackingSample() noexcept { return {}; }
void PublishGameplayHeadTracking(const runtime::VrHmdPose&,float) noexcept {}
}
#include "native_throw_fixture.hpp"
int main(int argc, char** argv) {
    using namespace penumbra_vr::backends::requiem;
    g_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!g_image) return 2;
    bool passed=false;
    if(argc>1 && std::string(argv[1])=="throw") {
        native_throw_fixture::Fixture fixture(test_frame,PVR_TEST_NEWTON_DLL);
        passed=fixture.Ready() && native_throw_fixture::CharacterizedNativeCases(fixture) &&
            native_throw_fixture::AssistedThrowCases(fixture,&HookedGrabExamine,&HookedThrowDirection) &&
            native_throw_fixture::GestureReleaseCases(fixture);
        if(passed) {
            fixture.Reset(); test_ui=true;
            HookedGrabLeave(fixture.state_image.data(),nullptr,fixture.next_image.data());
            passed=native_throw_fixture::Near(fixture.Velocity(),{}) &&
                native_throw_fixture::Near(fixture.Omega(),{});
            test_ui=false;
        }
        if(passed) {
            std::string error;
            passed=RemoveRenderWorld(error) && !RenderHooksInstalled() &&
                ReadNative<void*>(g_image,kGrabExamineSlotRva)==g_image+kGrabExamineRva &&
                std::equal(kThrowDirectionCall.begin(),kThrowDirectionCall.end(),g_image+kThrowDirectionCallRva);
            fixture.Reset(); fixture.InvokeExamine();
            passed=passed && native_throw_fixture::Near(fixture.impulse_delta,{13,0,0});
        }
    } else passed=SelectionCases();
    VirtualFree(g_image,0,MEM_RELEASE);
    return passed ? 0 : 1;
}
