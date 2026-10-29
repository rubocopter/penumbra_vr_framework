// Actual Requiem render/interaction consumer with typed native boundaries.
// No game, audio device, GL context or headset is created.
#include "../../src/backends/requiem/render_world.cpp"
#include <iostream>
#include <vector>
#include <thread>

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
std::vector<SceneHit> test_offset_hand_hits;
bool test_separate_hand_rays=false;
bool test_separate_area_ray=false;
bool test_geometric_area_ray=false;
bool test_filter_segments=false;
void* test_character_only=nullptr;
std::vector<SceneHit> test_area_hits;
void* test_script_area_body=nullptr;
void* test_joint_body=nullptr;
void* test_joint_ptr=nullptr;
bool test_move_leave_saw_restored=false;
unsigned test_head_rays=0, test_native_rays=0;
TestVec test_last_from{},test_last_to{};
TestVec test_area_from{},test_area_to{};

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
int __fastcall NativeJointCountStub(void* body,void*) {
    return body==test_joint_body && test_joint_ptr ? 1 : 0;
}
void __fastcall NativeMoveLeaveStub(void*,void*,void*) {
    test_move_leave_saw_restored=test_joint_ptr!=nullptr &&
        ReadNative<float>(test_joint_ptr,0xE8)==0.07F &&
        ReadNative<float>(test_joint_ptr,0xEC)==0.12F;
    test_joint_ptr=nullptr; // Simulate native Move::Leave changing the links.
}
void __fastcall NativeBodyFloatStub(void*,void*,float) {}
void __fastcall NativeBodyAwakeStub(void*,void*,bool) {}
void* __fastcall NativeJointStub(void* body,void*,int index) {
    return body==test_joint_body && index==0 ? test_joint_ptr : nullptr;
}
int __fastcall NativeSliderTypeStub(void*,void*) { return 2; }
bool __fastcall NativeBefore(void*,void*,void* body) { return body!=nullptr; }
bool __fastcall NativeIntersect(void* value,void*,void* body,void* raw) {
    auto& pick=*static_cast<TestPick*>(value);
    const auto& hit=*static_cast<RankedRayCallback::Hit*>(raw);
    void* entity=ReadNative<void*>(body,0x414);
    // Native A5C5F ignores entityless character-only colliders (Collide=false).
    if(body==test_character_only) return true;
    // Model native callback area routing explicitly: 13 is a verified ladder
    // type, while a level05 ScriptArea numeric type is not mapped yet.
    if(body==test_script_area_body ||
        (entity && ReadNative<int>(entity,0xC0)==13)) {
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
    const bool head_to_hand=head && std::abs((*to)[1]-(*from)[1])>0.1F;
    if(head_to_hand) { test_area_from=*from; test_area_to=*to; }
    else if(head) { ++test_head_rays; test_last_from=*from; test_last_to=*to; }
    auto* table=ReadNative<TestPick::Table*>(callback,0);
    const bool offset=std::abs((*from)[0])>0.001F ||
        std::abs((*from)[1]-0.8F)>0.001F;
    const auto& hits=head_to_hand && test_separate_area_ray ? test_area_hits : head ? test_head_hits :
        test_separate_hand_rays && offset ? test_offset_hand_hits : test_hand_hits;
    for(const auto& hit:hits) {
        if(test_filter_segments) {
            const TestVec delta{(*to)[0]-(*from)[0],(*to)[1]-(*from)[1],(*to)[2]-(*from)[2]};
            const TestVec rel{hit.point[0]-(*from)[0],hit.point[1]-(*from)[1],hit.point[2]-(*from)[2]};
            float dot=0,length2=0;
            for(unsigned i=0;i<3;++i){dot+=delta[i]*rel[i];length2+=delta[i]*delta[i];}
            const float t=dot/length2;
            if(t<0 || t>1 || std::hypot(rel[0]-t*delta[0],rel[1]-t*delta[1],rel[2]-t*delta[2])>0.015F)
                continue;
        }
        if(head_to_hand && test_geometric_area_ray) {
            // A 5 mm target must lie on this finite ray, not merely occur in
            // the injected hit list. Pin visible-palm versus controller origin.
            const TestVec delta{(*to)[0]-(*from)[0],(*to)[1]-(*from)[1],(*to)[2]-(*from)[2]};
            const TestVec rel{hit.point[0]-(*from)[0],hit.point[1]-(*from)[1],hit.point[2]-(*from)[2]};
            float dot=0,length2=0;
            for(unsigned i=0;i<3;++i) { dot+=delta[i]*rel[i]; length2+=delta[i]*delta[i]; }
            const float t=dot/length2;
            if(t<0 || t>1 || std::hypot(rel[0]-t*delta[0],rel[1]-t*delta[1],rel[2]-t*delta[2])>0.005F)
                continue;
        }
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
float __fastcall NativePlayerPickDistance(void* player,void*) {
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
unsigned test_lamp_picks=0;
void __fastcall NativeLampPick(void* entity,void*) {
    ++test_lamp_picks;
    auto* player=ReadNative<void*>(ReadNative<void*>(entity,0x10),0x15C);
    // Characterized 38BF0: native range and authored on/off admission own cue 1.
    if(NativeEntityDistance(entity,nullptr)<=ReadNative<float>(entity,0xBC) &&
        ReadNative<bool>(entity,0x37)) Put(player,0x2E8,1);
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
    // Native normal inventory item behind a character-only barrier. Physical
    // palm reach cannot reach it, but original native pickup range admits it.
    std::array<std::uint8_t,0x500> orb_body{},orb_entity{},character_only{};
    Put(orb_body.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(orb_body.data(),0x414,static_cast<void*>(orb_entity.data()));
    Put(orb_body.data(),0x31,true); Put(orb_body.data(),0x418,true);
    Put(orb_entity.data(),0,g_image+0x276ED0);
    Put(orb_entity.data(),0x10,static_cast<void*>(init.data()));
    Put(orb_entity.data(),0x14,true); Put(orb_entity.data(),0x37,true);
    Put(orb_entity.data(),0xC0,5); Put(orb_entity.data(),0x250,0);
    Put(orb_entity.data(),0xBC,2.0F);
    Put(g_image+0x276ED0,0x48,reinterpret_cast<void*>(&NativeEntityDistance));
    JumpTo(0x9C0E0,reinterpret_cast<void*>(&NativePlayerPickDistance));
    Put(character_only.data(),0x3C8,true); Put(character_only.data(),0x418,false);
    test_character_only=character_only.data(); test_filter_segments=true;
    const auto saved_head=test_head_hits;
    const auto saved_hand=test_hand_hits;
    const TestVec orb_point{0,0.8F,-1.2F};
    test_hand_hits={{orb_body.data(),1.2F,orb_point},{character_only.data(),0.4F,{0,0.8F,-0.4F}}};
    test_head_hits={{orb_body.data(),1.5F,orb_point}};
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=orb_body.data() || g_vr_selection_player!=player.data()) {
        std::cerr<<"Native inventory pickup lost beyond character-only hatch barrier\n";
        return Failed(__LINE__);
    }
    const auto check_orb=[&](bool expected) {
        g_world_from_tracking_time=GetTickCount64();
        update(); RefreshVrSelectionBeforeInteract(player.data());
        return (pick.body==orb_body.data())==expected;
    };
    // Native character-to-hit distance, not ray length, owns admission.
    Put(character.data(),0x48,TestVec{0,0.8F,0.81F});
    if(!check_orb(false)) return Failed(__LINE__);
    for(const float height:{0.5F,1.0F}) { // crouched and standing
        Put(character.data(),0x48,TestVec{0,height,0});
        if(!check_orb(true)) return Failed(__LINE__);
    }
    // A true solid/closed hatch blocks the controller ray in either hit order.
    test_hand_hits.push_back({foreground.data(),0.6F,{0,0.8F,-0.6F}});
    for(const bool reverse:{false,true}) {
        if(reverse) std::reverse(test_hand_hits.begin(),test_hand_hits.end());
        if(!check_orb(false)) return Failed(__LINE__);
    }
    test_hand_hits={{orb_body.data(),1.2F,orb_point}};
    // Hand is already through the obstruction, but head sight is still blocked.
    test_head_hits.push_back({foreground.data(),0.75F,{0,1.25F,-0.6F}});
    if(!check_orb(false)) return Failed(__LINE__);
    test_head_hits={{orb_body.data(),1.5F,orb_point}};
    for(const std::size_t field:{std::size_t{0x14},std::size_t{0x37}}) {
        Put(orb_entity.data(),field,false);
        if(!check_orb(false)) return Failed(__LINE__);
        Put(orb_entity.data(),field,true);
    }
    Put(orb_entity.data(),0x35,true);
    if(!check_orb(false)) return Failed(__LINE__);
    Put(orb_entity.data(),0x35,false);
    Put(orb_entity.data(),0xC0,1); // loose props must remain physical-only
    if(!check_orb(false)) return Failed(__LINE__);
    Put(orb_entity.data(),0xC0,5);
    Put(orb_entity.data(),0x250,13); // uncharacterized subtype fails closed
    if(!check_orb(false)) return Failed(__LINE__);
    Put(orb_entity.data(),0x250,0);
    Put(orb_entity.data(),0xBC,0.5F); // authored shorter native distance
    if(!check_orb(false)) return Failed(__LINE__);
    Put(orb_entity.data(),0xBC,2.0F);
    // Shared Normal pickup assistance remains bounded at 1.9 m from the hand.
    test_hand_hits={{orb_body.data(),1.91F,{0,0.8F,-1.91F}}};
    test_head_hits={{orb_body.data(),2.11F,{0,0.8F,-1.91F}}};
    if(!check_orb(false)) return Failed(__LINE__);
    // Controller and world orientation change; no camera-yaw inversion.
    grip.device_to_absolute.values={0,0,1,0, 0,1,0,0.8F, -1,0,0,0};
    test_hand_hits={{orb_body.data(),1.2F,{-1.2F,0.8F,0}}};
    test_head_hits={{orb_body.data(),1.5F,{-1.2F,0.8F,0}}};
    if(!check_orb(true)) return Failed(__LINE__);
    // The published pick is native inventory admission, never VR Grab/Move.
    if(g_grab_hold.state || g_move_hold.state || g_push_hold.state ||
        !Near(NativePlayerPickDistance(player.data(),nullptr),std::sqrt(1.48F)))
        return Failed(__LINE__);
    for(const unsigned guard:{0U,1U,2U}) {
        test_ui=guard==0; test_frame.focused=guard!=1;
        if(guard==2) g_world_from_tracking_time=GetTickCount64()-101;
        update(); RefreshVrSelectionBeforeInteract(player.data());
        if(g_vr_selection_player) return Failed(__LINE__);
    }
    test_ui=false; test_frame.focused=true; g_world_from_tracking_time=GetTickCount64();
    grip.device_to_absolute.values={1,0,0,0, 0,1,0,0.8F, 0,0,1,0};
    test_filter_segments=false; test_character_only=nullptr;
    test_head_hits=saved_head; test_hand_hits=saved_hand;
    // A native wall on the same hand ray must occlude a physical candidate,
    // regardless of hit order. It must not mask another unobstructed ray.
    test_head_hits.clear();
    Put(foreground.data(),0,g_image+kPhysicsBodyVtableRva);
    test_hand_hits={{hand_body.data(),0.20F,{0,0.8F,-0.20F}},
                    {foreground.data(),0.10F,{0,0.8F,-0.10F}}};
    for(const bool reversed:{false,true}) {
        if(reversed) std::reverse(test_hand_hits.begin(),test_hand_hits.end());
        RefreshVrSelectionBeforeInteract(player.data());
        if(pick.body || g_vr_selection_player) {
            std::cerr<<"Hand selected a physical body through foreground geometry\n";
            return Failed(__LINE__);
        }
    }
    test_separate_hand_rays=true;
    Put(entity.data(),0x37,true); // This independent finger-ray target is usable.
    test_offset_hand_hits={{body.data(),0.30F,{0,0.8F,-0.30F}}};
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=body.data()) {
        std::cerr<<"Occluded central ray masked an unobstructed finger ray\n";
        return Failed(__LINE__);
    }
    test_separate_hand_rays=false; test_offset_hand_hits.clear();
    Put(entity.data(),0x37,false); // Restore the head-only described Object.
    test_hand_hits={{hand_body.data(),0.2F,{0,0.8F,-0.2F}}};
    test_head_hits={{body.data(),1.8F,{0,1.7F,-1.8F}}};
    RefreshVrSelectionBeforeExamine(player.data());
    if(pick.body!=body.data() || g_vr_selection_player || ReadNative<void*>(player.data(),0x280)!=&pick) {
        std::cerr<<"Examine action did not refresh its own head target\n"; return Failed(__LINE__);
    }
    // A described loose prop cannot become a distant VR grab on Interact.
    test_hand_hits.clear();
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body || g_vr_selection_player) return Failed(__LINE__);
    // BP's fixed-Lamp native activation fallback is required by buttonfurn.
    // It must not turn dynamic lights or described props into remote grabs.
    Put(entity.data(),0,g_image+0x2773C0); Put(entity.data(),0xC0,12);
    Put(entity.data(),0x37,true); Put(entity.data(),0xBC,2.1F);
    Put(body.data(),0x434,0.0F);
    Put(g_image+0x2773C0,0x48,reinterpret_cast<void*>(&NativeEntityDistance));
    JumpTo(0x38BF0,reinterpret_cast<void*>(&NativeLampPick));
    test_head_hits={{body.data(),1.5F,{0,1.7F,-1.5F}}}; test_hand_hits.clear();
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=body.data() || g_vr_selection_player!=player.data() || !test_lamp_picks) {
        std::cerr<<"Fixed native Lamp lost without palm contact: body="<<pick.body
            <<" picks="<<test_lamp_picks<<" fixed="<<g_normal_gaze.fixed_lamp
            <<" cue="<<g_normal_gaze.cue<<" valid="<<g_normal_gaze.valid<<'\n'; return Failed(__LINE__);
    }
    // Physical props keep priority; the original native ray still owns walls.
    test_hand_hits={{hand_body.data(),0.1F,{0,0.8F,-0.1F}}};
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=hand_body.data()) return Failed(__LINE__);
    test_hand_hits.clear(); test_head_hits.push_back({foreground.data(),0.4F,{0,1.7F,-0.4F}});
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) return Failed(__LINE__);
    test_head_hits={{body.data(),2.5F,{0,1.7F,-2.5F}}};
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) return Failed(__LINE__);
    test_head_hits={{body.data(),1.5F,{0,1.7F,-1.5F}}}; Put(body.data(),0x434,1.0F);
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) return Failed(__LINE__);
    Put(body.data(),0x434,0.0F); Put(entity.data(),0,g_image+0x270100);
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) return Failed(__LINE__);
    Put(entity.data(),0xC0,1); Put(entity.data(),0x37,false);
    // Inert Object housing must not win VR contact ranking. Static script
    // buttons and Pick callbacks remain eligible without a manipulation flag.
    test_head_hits.clear(); test_hand_hits={{body.data(),0.1F,{0,0.8F,-0.1F}}};
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) { std::cerr<<"Inert Object stole physical VR selection\n"; return Failed(__LINE__); }
    for(auto offset:{0x204U,0x20CU}) {
        Put(entity.data(),offset,reinterpret_cast<void*>(1));
        update(); RefreshVrSelectionBeforeInteract(player.data());
        if(pick.body!=body.data()) return Failed(__LINE__);
        Put(entity.data(),offset,static_cast<void*>(nullptr));
    }
    test_hand_hits.clear();
    // Level05 generator/hatch/door buttons are authored script areas. Model
    // their native callback route without inventing a Requiem entity type.
    std::array<std::uint8_t,0x500> script_area{}, script_entity{};
    Put(script_area.data(),0,g_image+0x271000);
    Put(script_area.data(),0x414,static_cast<void*>(script_entity.data()));
    Put(script_entity.data(),0x14,true);
    Put(script_entity.data(),0xC0,0);
    Put(script_entity.data(),0x37,true);
    Put(script_entity.data(),0xBC,2.1F);
    Put(script_entity.data(),0x10,static_cast<void*>(init.data()));
    test_script_area_body=script_area.data();
    test_head_hits.clear();
    // Rework 23c890f uses camera -> hand-centre LOS for native areas before
    // physical contact. A button beyond the 18 cm physical ray must retain
    // native area range, with the same selection before and during Interact.
    test_separate_area_ray=true;
    test_geometric_area_ray=true;
    test_hand_hits.clear();
    grip.device_to_absolute.values[11]=-0.5F;
    test_area_hits={{script_area.data(),1.51F,{0,0.35F,-0.675F}}};
    update();
    if(pick.body!=script_area.data()) {
        std::cerr<<"Native button area missing from passive head-to-hand selection\n";
        return Failed(__LINE__);
    }
    // head=(0,1.7,0), visible palm=(0,0.8,-0.45), native length=10.
    if(!Near(test_area_from[1],1.7F) ||
        !Near(test_area_to[1],1.7F-9.0F/std::sqrt(1.0125F)) ||
        !Near(test_area_to[2],-4.5F/std::sqrt(1.0125F))) return Failed(__LINE__);
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=script_area.data() || g_vr_selection_player!=player.data())
        return Failed(__LINE__);
    // Rotated controller +Z shifts the visible palm 5 cm along world +X.
    // The query must follow that palm, still using the native area range.
    grip.device_to_absolute.values={0,0,1,0, 0,1,0,0.8F, -1,0,0,-0.5F};
    test_area_hits={{script_area.data(),1.55F,{0.075F,0.35F,-0.75F}}};
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=script_area.data()) return Failed(__LINE__);
    test_geometric_area_ray=false;
    test_area_hits.push_back({foreground.data(),0.4F,{0,1.3F,-0.1F}});
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) return Failed(__LINE__);
    test_area_hits={{script_area.data(),11.0F,{0,0.8F,-11.0F}}};
    update(); RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body) return Failed(__LINE__);
    test_separate_area_ray=false; test_area_hits.clear();
    grip.device_to_absolute.values={1,0,0,0, 0,1,0,0.8F, 0,0,1,0};
    test_hand_hits={{script_area.data(),0.18F,{0,0.8F,-0.18F}}};
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=script_area.data() || g_vr_selection_player!=player.data()) {
        std::cerr<<"Native script-area button lost during VR hand selection\n";
        return Failed(__LINE__);
    }
    test_hand_hits={{script_area.data(),0.18F,{0,0.8F,-0.18F}},
                    {foreground.data(),0.10F,{0,0.8F,-0.10F}}};
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body || g_vr_selection_player) {
        std::cerr<<"Script-area button selected through nearer solid\n";
        return Failed(__LINE__);
    }
    // An area in front of a regular pickable physical body must win through
    // the native area arbitration even when the VR body ranker sees the body.
    test_hand_hits={{hand_body.data(),0.28F,{0,0.8F,-0.28F}},
                    {script_area.data(),0.18F,{0,0.8F,-0.18F}}};
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=script_area.data()) return Failed(__LINE__);
    test_hand_hits={{hand_body.data(),0.10F,{0,0.8F,-0.10F}},
                    {script_area.data(),0.18F,{0,0.8F,-0.18F}}};
    RefreshVrSelectionBeforeInteract(player.data());
    if(pick.body!=hand_body.data()) return Failed(__LINE__);
    // The production overlap fast path also uses native area arbitration.
    // Compare the same native area result against nearby palm contact; a
    // physical contact in front of the area must retain priority.
    test_hand_hits={{script_area.data(),0.18F,{0,0.8F,-0.18F}},
                    {hand_body.data(),0.28F,{0,0.8F,-0.28F}}};
    const auto rays=InteractionRaySegments(runtime::IdentityMatrix(),2.1F);
    NativePickResult candidate{};
    if(!FindNativeHandArea(reinterpret_cast<Ray>(&NativeRay),nullptr,&pick,
            rays,1U,true,false,true,false,2.1F,candidate) ||
        !NativeAreaPrecedesPhysicalTarget(candidate,{0,0.8F,-0.28F},{0,0.8F,0}) ||
        NativeAreaPrecedesPhysicalTarget(candidate,{0,0.8F,-0.10F},{0,0.8F,0})) return Failed(__LINE__);
    test_hand_hits={{hand_body.data(),0.10F,{0,0.8F,-0.10F}},
                    {script_area.data(),0.18F,{0,0.8F,-0.18F}}};
    if(FindNativeHandArea(reinterpret_cast<Ray>(&NativeRay),nullptr,&pick,
            rays,1U,true,false,true,false,2.1F,candidate)) return Failed(__LINE__);
    NativePickResult invalid{};
    invalid.area=script_area.data(); invalid.body=hand_body.data();
    invalid.area_distance=0.05F;
    if(NativeAreaPrecedesPhysicalTarget(invalid,{0,0,-0.28F},{})) return Failed(__LINE__);
    // Area starts 6.25 cm behind the palm. Its native ray distance must not
    // be compared directly with a contact distance measured from the palm.
    candidate.body=candidate.area=script_area.data();
    candidate.point={0,0,-0.18F}; candidate.area_distance=0.2425F;
    if(!NativeAreaPrecedesPhysicalTarget(candidate,{0,0,-0.20F},{})) return Failed(__LINE__);
    if(NativeAreaPrecedesPhysicalTarget(candidate,{0,0,-0.10F},{})) return Failed(__LINE__);
    test_script_area_body=nullptr;
    test_hand_hits.clear();
    test_head_hits={{body.data(),1.8F,{0,1.7F,-1.8F}}};
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
bool SliderStickyCases() {
    std::array<std::uint8_t,0x500> body{},parent{},joint{},replacement{};
    Put(body.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(parent.data(),0,g_image+kPhysicsBodyVtableRva);
    Put(parent.data(),0x434,0.0F);
    Put(joint.data(),0,g_image+kSliderVtableRva);
    Put(joint.data(),0x30,static_cast<void*>(body.data()));
    Put(joint.data(),0x2C,static_cast<void*>(parent.data()));
    Put(joint.data(),0xB8,TestVec{1,0,0});
    Put(joint.data(),0xC4,TestVec{0,0,0});
    Put(joint.data(),0xE8,0.07F);
    Put(joint.data(),0xEC,0.12F);
    Put(g_image+kSliderVtableRva,0x14,g_image+kSliderGetTypeRva);
    JumpTo(kSliderGetTypeRva,reinterpret_cast<void*>(&NativeSliderTypeStub));
    JumpTo(kGetJointCountRva,reinterpret_cast<void*>(&NativeJointCountStub));
    JumpTo(kGetBodyJointRva,reinterpret_cast<void*>(&NativeJointStub));
    test_joint_body=body.data(); test_joint_ptr=joint.data();
    MoveHold hold{}; hold.body=body.data();
    if(!BindNativeJoint(hold.body,hold) || hold.mode!=MoveMode::slider ||
        hold.slider_joint!=joint.data()) return Failed(__LINE__);
    if(!SuspendRequiemMoveSliderStickiness(hold) ||
        !hold.slider_sticky_suspended ||
        !Near(ReadNative<float>(joint.data(),0xE8),0.0F) ||
        !Near(ReadNative<float>(joint.data(),0xEC),0.0F)) return Failed(__LINE__);
    // Scripts that rewrite a distance during Move keep their new setting.
    Put(joint.data(),0xEC,0.03F);
    RestoreRequiemMoveSliderStickiness(hold);
    if(!Near(ReadNative<float>(joint.data(),0xE8),0.07F) ||
        !Near(ReadNative<float>(joint.data(),0xEC),0.03F)) return Failed(__LINE__);
    // Recycled/unlinked joint pointers must not receive stale restoration.
    Put(joint.data(),0xE8,0.0F);
    test_joint_ptr=replacement.data();
    RestoreRequiemMoveSliderStickiness(hold);
    if(!Near(ReadNative<float>(joint.data(),0xE8),0.0F)) return Failed(__LINE__);
    test_joint_ptr=joint.data();
    Put(parent.data(),0x434,3.0F);
    Put(joint.data(),0xE8,0.07F);
    MoveHold moving_parent{}; moving_parent.body=body.data();
    if(!BindNativeJoint(moving_parent.body,moving_parent) ||
        !SuspendRequiemMoveSliderStickiness(moving_parent) ||
        moving_parent.slider_sticky_suspended ||
        !Near(ReadNative<float>(joint.data(),0xE8),0.07F)) return Failed(__LINE__);
    Put(parent.data(),0x434,0.0F);
    Put(joint.data(),0x30,static_cast<void*>(parent.data()));
    MoveHold other_child{}; other_child.body=body.data();
    other_child.mode=MoveMode::slider; other_child.slider_joint=joint.data();
    if(!SuspendRequiemMoveSliderStickiness(other_child) ||
        other_child.slider_sticky_suspended ||
        !Near(ReadNative<float>(joint.data(),0xE8),0.07F)) return Failed(__LINE__);
    // Release must restore the original sticky ranges before the native
    // transition removes or changes the joint association.
    Put(joint.data(),0x30,static_cast<void*>(body.data()));
    Put(joint.data(),0xEC,0.12F);
    test_joint_ptr=joint.data();
    std::array<std::uint8_t,0x100> state{};
    MoveHold releasing{};
    releasing.body=body.data(); releasing.mode=MoveMode::slider;
    releasing.slider_joint=joint.data(); releasing.state=state.data();
    if(!SuspendRequiemMoveSliderStickiness(releasing) ||
        !releasing.slider_sticky_suspended) return Failed(__LINE__);
    {
        ActiveCall game_callback;
        g_move_hold=releasing;
    }
    JumpTo(kMoveTargets[2],reinterpret_cast<void*>(&NativeMoveLeaveStub));
    JumpTo(0x19CAC0,reinterpret_cast<void*>(&NativeBodyFloatStub));
    JumpTo(0x19CAE0,reinterpret_cast<void*>(&NativeBodyFloatStub));
    JumpTo(0x19CBB0,reinterpret_cast<void*>(&NativeBodyAwakeStub));
    Put(g_image,kMoveSlots[2],g_image+kMoveTargets[2]);
    std::string removal_error;
    if(!hooks::InstallPointerHook(
            reinterpret_cast<void**>(g_image+kMoveSlots[2]),
            g_image+kMoveTargets[2],reinterpret_cast<void*>(&HookedMoveLeave),
            g_move_state_hooks[2],removal_error)) return Failed(__LINE__);
    g_presenting.store(false); g_targets_destroyed.store(true);
    bool removed=false;
    std::thread control([&] {removed=RemoveRenderWorld(removal_error);});
    control.join();
    if(removed || removal_error.empty() || !g_move_state_hooks[2].installed() ||
        !Near(ReadNative<float>(joint.data(),0xE8),0.0F)) {
        std::cerr<<"Control-thread removal abandoned the game-thread slider hold\n";
        return Failed(__LINE__);
    }
    HookedMoveLeave(state.data(),nullptr,nullptr);
    if(!test_move_leave_saw_restored || g_move_hold.state!=nullptr ||
        !Near(ReadNative<float>(joint.data(),0xE8),0.07F) ||
        !Near(ReadNative<float>(joint.data(),0xEC),0.12F)) return Failed(__LINE__);
    // An Enter may have published its native transition before its first
    // Update commits a hold. All three pending families must retain Leave.
    for(void** pending:{&g_pending_grab_state,&g_pending_move_state,&g_pending_push_state}) {
        { ActiveCall game_callback; *pending=state.data(); }
        std::thread pending_control([&] {removed=RemoveRenderWorld(removal_error);});
        pending_control.join();
        if(removed || !g_move_state_hooks[2].installed()) return Failed(__LINE__);
        { ActiveCall game_callback; *pending=nullptr; }
    }
    {
        ActiveCall in_flight;
        std::thread callback_control([&] {removed=RemoveRenderWorld(removal_error);});
        callback_control.join();
        if(removed || !g_move_state_hooks[2].installed()) return Failed(__LINE__);
    }
    if(!RemoveRenderWorld(removal_error) || RenderHooksInstalled()) return Failed(__LINE__);
    test_joint_ptr=nullptr; test_joint_body=nullptr;
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
#include "native_mechanism_fixture.hpp"
#include "articulated_grab_fixture.hpp"
namespace penumbra_vr::backends::requiem {
bool PresentationSettingsCases() {
    runtime::VrSettings settings;
    settings.ui_scale=1.5F;
    settings.ui_distance=2.25F;
    settings.subtitle_scale=1.8F;
    ConfigurePresentationSettings(settings);
    const auto inventory=PanelGeometry(NativeUiSurface::inventory);
    const auto notebook=PanelGeometry(NativeUiSurface::notebook);
    if (!Near(inventory.width,800.0F/750.0F*1.5F) ||
        !Near(inventory.center_y,-100.0F/750.0F*1.5F) ||
        !Near(notebook.width,800.0F/1450.0F*1.5F)) {
        std::cerr<<"Requiem render/pointer panel geometry ignored live UiScale\n";
        return false;
    }
    const auto overlay=GameplayOverlayGeometry();
    const auto fullscreen=FullscreenPanelGeometry();
    if (!Near(overlay.right,400.0F/750.0F*1.8F) ||
        !Near(overlay.left,-overlay.right) || !Near(overlay.top,0.4F*1.8F) ||
        !Near(overlay.bottom,-overlay.top) || !Near(overlay.distance,2.25F) ||
        !Near(fullscreen.width,3.6F) || !Near(fullscreen.distance,2.25F))
        return Failed(__LINE__);
    auto anchor=runtime::VrMatrix34{};
    const auto identity=runtime::IdentityMatrix();
    std::copy_n(identity.values.begin(),12,anchor.values.begin());
    PublishFullscreenMenuPointer(anchor,4.0F/3.0F,fullscreen);
    runtime::VrHmdPose aim;
    aim.device_connected=true; aim.pose_valid=true;
    aim.device_to_absolute=anchor;
    aim.device_to_absolute.values[3]=0.6F;
    std::array<float,2> uv{};
    if (!TrackedMenuPointer(aim,uv) || !Near(uv[0],2.0F/3.0F) ||
        !Near(uv[1],0.5F)) return Failed(__LINE__);
    // Live edits use normalized settings, and HUD scaling never changes the
    // inventory/notebook or the matching fullscreen pointer plane.
    settings.ui_scale=0.9F; settings.subtitle_scale=0.8F;
    settings.ui_distance=1.25F;
    ConfigurePresentationSettings(settings);
    if (!Near(PanelGeometry(NativeUiSurface::inventory).width,0.96F) ||
        !Near(FullscreenPanelGeometry().width,2.16F) ||
        !Near(GameplayOverlayGeometry().right,400.0F/750.0F*0.8F) ||
        !Near(GameplayOverlayGeometry().distance,1.25F)) return Failed(__LINE__);
    settings.ui_scale=10; settings.ui_distance=10; settings.subtitle_scale=0;
    ConfigurePresentationSettings(settings);
    if (!Near(FullscreenPanelGeometry().width,4.8F) ||
        !Near(GameplayOverlayGeometry().distance,3) ||
        !Near(GameplayOverlayGeometry().right,0.4F)) return Failed(__LINE__);
    ConfigurePresentationSettings(runtime::VrSettings{});
    return true;
}
}
int main(int argc, char** argv) {
    using namespace penumbra_vr::backends::requiem;
    g_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!g_image) return 2;
    bool passed=false;
    if(argc>1 && std::string(argv[1])=="ui-geometry") {
        passed=PresentationSettingsCases();
    } else if(argc>1 && std::string(argv[1])=="articulated") {
        passed=articulated_grab_fixture::Cases();
    } else if(argc>1 && std::string(argv[1])=="mechanism") {
        passed=native_mechanism_fixture::Cases();
    } else if(argc>1 && std::string(argv[1])=="throw") {
        native_throw_fixture::Fixture fixture(test_frame,PVR_TEST_NEWTON_DLL);
        passed=fixture.Ready() && native_throw_fixture::CharacterizedNativeCases(fixture) &&
            native_throw_fixture::AssistedThrowCases(fixture,&HookedGrabExamine,&HookedThrowDirection) &&
            native_throw_fixture::GestureReleaseCases(fixture) &&
            native_throw_fixture::PushThrowCases(fixture) &&
            native_throw_fixture::MoveThrowCases(fixture) &&
            native_throw_fixture::OrientationAndFreshnessCases(fixture) &&
            native_throw_fixture::GestureMassAndOrientationCases(fixture) &&
            native_throw_fixture::PendingThrowCases(fixture);
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
                std::equal(kThrowDirectionCall.begin(),kThrowDirectionCall.end(),g_image+kThrowDirectionCallRva) &&
                std::equal(kMoveThrowDirectionCall.begin(),kMoveThrowDirectionCall.end(),g_image+kMoveThrowDirectionCallRva) &&
                ReadNative<void*>(g_image,kMoveSlots[3])==g_image+kMoveTargets[3] &&
                ReadNative<void*>(g_image,kPushSlots[3])==g_image+kPushTargets[3];
            fixture.Reset(); fixture.InvokeExamine();
            passed=passed && native_throw_fixture::Near(fixture.impulse_delta,{13,0,0});
        }
    } else passed=SelectionCases() && SliderStickyCases();
    VirtualFree(g_image,0,MEM_RELEASE);
    return passed ? 0 : 1;
}
