// Exact-build adapter for the initialized Black Plague FD316F... image.
// Native Enter/Leave retain ownership of HPL's mass/gravity/script transitions.
#include "spatial_interaction.hpp"
#include "native_input_bridge.hpp"
#include "render_world_probe.hpp"
#include "vr_grab_pose.hpp"
#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace penumbra_vr::backends::black_plague {
namespace {
using Vec = std::array<float,3>;
using Matrix = runtime::VrMatrix44;
using Update = void(__thiscall*)(void*,float);
using Transition = void(__thiscall*)(void*,void*);
using Ray = void(__thiscall*)(void*,void*,const Vec*,const Vec*,bool,bool,bool,bool);
std::uint8_t* g_image = nullptr;
std::array<hooks::IatHook,5> g_hooks;
hooks::Rel32CallHook g_tool_hook;
thread_local void* g_updating_hands = nullptr;
std::atomic<std::uint64_t> g_tools_attached{0},g_tools_native{0},g_invalid_tool_pose{0},g_blocked_grabs{0};
std::atomic<bool> g_enabled{false};
std::atomic<unsigned> g_callbacks{0};
struct CallbackScope {
    CallbackScope() { g_callbacks.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackScope() { g_callbacks.fetch_sub(1, std::memory_order_acq_rel); }
};
std::atomic<bool> g_held{false};
// HPL's per-body CollideCharacter flag is consumed by world collision queries,
// character rays and Newton's character/body contact filtering. Never move a
// tracked body until the exact-build field and its consumers have been proved.
std::atomic<bool> g_player_collision_filter_ready{false};
void* g_pending_state = nullptr; // Input thread only; never dereferenced without current-state identity.
struct Hold {
    void* state = nullptr;
    void* body = nullptr;
    runtime::VrHand hand = runtime::VrHand::right;
    runtime::VrGrabPose pose;
    float max_linear = 0, max_angular = 0;
    Vec previous_palm{};
    bool collide_character = true;
    bool discard_momentum = false;
} g_hold;

bool Copy(const void* source, void* dest, std::size_t size) noexcept {
    if (!source) return false;
    __try { std::memcpy(dest,source,size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool Store(void* destination, const void* source, std::size_t size) noexcept {
    if (!destination || !source) return false;
    __try { std::memcpy(destination,source,size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T Read(const void* object, std::uintptr_t offset) {
    T result{};
    if (object) static_cast<void>(Copy(static_cast<const std::uint8_t*>(object)+offset,&result,sizeof(result)));
    return result;
}
bool BodyMatches(void* body) { return body && Read<void*>(body,0) == g_image + 0x292C08; }
void SetFloat(void* body, std::uintptr_t target, float value) {
    reinterpret_cast<void(__thiscall*)(void*,float)>(g_image+target)(body,value);
}
void SetVelocity(void* body, std::uintptr_t target, const Vec& value) {
    reinterpret_cast<void(__thiscall*)(void*,const Vec*)>(g_image+target)(body,&value);
}
bool HandPose(runtime::VrHand hand, bool aim, Matrix& pose, Vec& velocity, Vec& angular) {
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused) return false;
    const auto& sample = frame.hands[hand == runtime::VrHand::left ? 0 : 1];
    const auto& tracking = aim && sample.aim.pose_valid ? sample.aim : sample.grip;
    return ControllerWorldPose(tracking,pose,velocity,angular);
}
void __fastcall HookedHandsUpdate(void* hands, void*, float dt) {
    CallbackScope scope;
    auto* previous = g_updating_hands;
    g_updating_hands = hands;
    reinterpret_cast<Update>(g_image+0xA3DE0)(hands,dt);
    g_updating_hands = previous;
}
void __fastcall HookedToolMatrix(void* entity, void*, const Matrix* native_matrix) {
    CallbackScope scope;
    Matrix destination;
    const Matrix* selected = native_matrix;
    if (g_enabled.load(std::memory_order_acquire) && g_updating_hands && !NativeInputUiActive() &&
        Read<int>(g_updating_hands,0x74)==2) {
        for (const auto slot : {0x6CU,0x70U}) {
            auto* model=Read<void*>(g_updating_hands,slot);
            if (!model || Read<void*>(model,0x118)!=entity) continue;
            // Borrow the legacy string through its own CRT comparison operator.
            // IAT 272138 is verified against MSVCP71 before installing hooks.
            using Equal = bool(__cdecl*)(const void*,const char*);
            const auto equal=reinterpret_cast<Equal>(Read<void*>(g_image,0x272138));
            const auto* name=static_cast<const std::uint8_t*>(model)+4;
            const bool flashlight=equal(name,"Flashlight");
            const bool glow=equal(name,"Glowstick");
            Matrix palm; Vec velocity{},angular{};
            if (!flashlight && !glow) break;
            if (!HandPose(runtime::VrHand::left,false,palm,velocity,angular)) { ++g_invalid_tool_pose; break; }
            // Native models point along -Y. Rotate +90 degrees around X so the
            // flashlight beam points along the controller's -Z. These sockets
            // use the installed model nodes, not Rework's different DAE files.
            Matrix local=runtime::IdentityMatrix();
            local.values[5]=0; local.values[6]=-1; local.values[9]=1; local.values[10]=0;
            const Vec socket=flashlight ? Vec{0,-0.016669F,0} : Vec{0,0.059722F,0.00504F};
            for (std::size_t row=0;row<3;++row)
                for (std::size_t col=0;col<3;++col) local.values[row*4+3]-=local.values[row*4+col]*socket[col];
            destination=runtime::Multiply(palm,local);
            selected=&destination;
            break;
        }
    }
    // Native transform propagation also moves the model's attached lights.
    if (selected==native_matrix) ++g_tools_native; else ++g_tools_attached;
    reinterpret_cast<void(__thiscall*)(void*,const Matrix*)>(g_image+0xCA120)(entity,selected);
}
void __fastcall HookedRay(void* world, void*, void* callback, const Vec* origin, const Vec* end,
    bool distance, bool normal, bool point, bool prefilter) {
    CallbackScope scope;
    // Only the normal-player picking call is redirected. Sound, AI, collision,
    // shadows and all other raycasts keep their original origins and endpoints.
    if (g_enabled.load(std::memory_order_acquire) &&
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) == reinterpret_cast<std::uintptr_t>(g_image)+0xAD852) {
        const auto frame = ReadNativeControllerFrame();
        Matrix pose; Vec velocity{},angular{},from{},to{};
        if (Copy(origin,from.data(),sizeof(from)) && Copy(end,to.data(),sizeof(to)) &&
            HandPose(frame.interact_source,true,pose,velocity,angular)) {
            const float length = std::hypot(to[0]-from[0],to[1]-from[1],to[2]-from[2]);
            if (std::isfinite(length) && length > 0 && length <= 20) {
                for (std::size_t row=0;row<3;++row) {
                    from[row]=pose.values[row*4+3]; to[row]=from[row]-pose.values[row*4+2]*length;
                }
                reinterpret_cast<Ray>(g_image+0x189E30)(world,callback,&from,&to,distance,normal,point,prefilter);
                return;
            }
        }
    }
    reinterpret_cast<Ray>(g_image+0x189E30)(world,callback,origin,end,distance,normal,point,prefilter);
}
void __fastcall HookedEnter(void* state, void*, void* previous) {
    CallbackScope scope;
    const auto frame = ReadNativeControllerFrame();
    g_pending_state = g_enabled.load(std::memory_order_acquire) && frame.focused &&
        frame.input.state.interact.just_pressed ? state : nullptr;
    reinterpret_cast<Transition>(g_image+0xAC900)(state,previous);
    // ChangeState publishes player+2BC AFTER Enter returns (9CABB).
    // Acquire only when ServiceSpatialInteraction observes the committed state.
}
void AcquirePendingGrab(void* state) {
    if (!g_player_collision_filter_ready.load(std::memory_order_acquire)) { ++g_blocked_grabs; return; }
    const auto frame = ReadNativeControllerFrame();
    auto* player=Read<void*>(state,0x10);
    auto* body=Read<void*>(state,0x20);
    if (!g_enabled.load(std::memory_order_acquire) || g_held.load() || !frame.focused || !frame.input.state.interact.just_pressed ||
        Read<int>(player,0x2BC)!=6 || !BodyMatches(body) || Read<void*>(body,0x330)!=nullptr ||
        Read<void*>(body,0x10)!=nullptr ||
        reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(body)!=0) return;
    Matrix palm,body_pose; Vec velocity{},angular{};
    if (!HandPose(frame.interact_source,false,palm,velocity,angular) || !Copy(static_cast<std::uint8_t*>(body)+0x34,&body_pose,sizeof(body_pose))) return;
    const float max_linear=Read<float>(body,0x42C),max_angular=Read<float>(body,0x430);
    const float mass=Read<float>(body,0x434);
    if (!std::isfinite(max_linear) || !std::isfinite(max_angular) || max_linear<0 || max_angular<0 ||
        !std::isfinite(mass) || mass<=0) return;
    Hold hold; hold.state=state; hold.body=body; hold.hand=frame.interact_source;
    hold.max_linear=max_linear; hold.max_angular=max_angular;
    hold.collide_character=Read<bool>(body,0x3C8);
    const auto local_contact=Read<Vec>(state,0x14);
    std::string error;
    if (!hold.pose.Begin(palm,body_pose,local_contact,Read<bool>(state,0xE1),error)) return;
    hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    const bool no_character_collision=false;
    if (!Store(static_cast<std::uint8_t*>(body)+0x3C8,&no_character_collision,sizeof(no_character_collision))) return;
    g_hold=hold;
    SetFloat(body,0x19C360,20); SetFloat(body,0x19C380,30);
    g_held.store(true,std::memory_order_release);
    NativeControllerHaptic(hold.hand,true);
}
void __fastcall HookedLeave(void* state, void*, void* next) {
    CallbackScope scope;
    if (g_pending_state == state) g_pending_state = nullptr;
    const bool owned=g_held.load(std::memory_order_acquire) && g_hold.state==state;
    Hold hold;
    Matrix palm; Vec velocity{},angular{};
    if (owned) {
        hold=g_hold;
        if (!hold.discard_momentum) static_cast<void>(HandPose(hold.hand,false,palm,velocity,angular));
        g_hold={}; g_held.store(false,std::memory_order_release);
    }
    reinterpret_cast<Transition>(g_image+0xAA4C0)(state,next);
    if (owned && BodyMatches(hold.body)) {
        static_cast<void>(Store(static_cast<std::uint8_t*>(hold.body)+0x3C8,
            &hold.collide_character,sizeof(hold.collide_character)));
        SetFloat(hold.body,0x19C360,hold.max_linear); SetFloat(hold.body,0x19C380,hold.max_angular);
        SetVelocity(hold.body,0x19C2A0,runtime::LimitTrackedVelocity(velocity,1.25F,9));
        SetVelocity(hold.body,0x19C2C0,runtime::LimitTrackedVelocity(angular,0.5F,6));
        NativeControllerHaptic(hold.hand,false);
    }
}
void __fastcall HookedGrabUpdate(void* state, void*, float dt) {
    CallbackScope scope;
    if (!g_held.load(std::memory_order_acquire) || g_hold.state!=state) {
        reinterpret_cast<Update>(g_image+0xABA90)(state,dt); return;
    }
    Matrix palm,destination; Vec velocity{},angular{};
    std::string error;
    if (dt == 0) return; // Paused/zero-time ticks must not eject a held object.
    bool valid=g_enabled.load(std::memory_order_acquire) && BodyMatches(g_hold.body) && std::isfinite(dt) && dt>0 && dt<=0.25F &&
        HandPose(g_hold.hand,false,palm,velocity,angular) && g_hold.pose.Update(palm,destination,error);
    if (valid) {
        const float displacement=std::hypot(palm.values[3]-g_hold.previous_palm[0],
            palm.values[7]-g_hold.previous_palm[1],palm.values[11]-g_hold.previous_palm[2]);
        valid=displacement<=1.0F;
    }
    if (!valid) {
        g_hold.discard_momentum=true;
        reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xA9FD0)(state);
        return;
    }
    g_hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    reinterpret_cast<void(__thiscall*)(void*,bool)>(g_image+0x19C590)(g_hold.body,false);
    // SetMatrix not a raw write: native transform callbacks update Newton and
    // the linked scene node. Parented/jointed bodies never enter this path.
    reinterpret_cast<void(__thiscall*)(void*,const Matrix*)>(g_image+0xCA120)(g_hold.body,&destination);
    SetVelocity(g_hold.body,0x19C2A0,runtime::LimitTrackedVelocity(velocity,1,20));
    SetVelocity(g_hold.body,0x19C2C0,runtime::LimitTrackedVelocity(angular,0.5F,6));
}
}

SpatialDiagnostics ConsumeSpatialDiagnostics() noexcept {
    return {g_tools_attached.exchange(0),g_tools_native.exchange(0),g_invalid_tool_pose.exchange(0),g_blocked_grabs.exchange(0)};
}
bool InstallSpatialInteraction(std::string& error) noexcept {
    error.clear();
    if (g_enabled.load(std::memory_order_acquire)) return true;
    g_player_collision_filter_ready.store(false,std::memory_order_release);
    g_image=reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    constexpr std::array<std::uintptr_t,5> slots{0x291BE8,0x27D0D4,0x27D12C,0x27D130,0x27CB70};
    constexpr std::array<std::uintptr_t,5> targets{0x189E30,0xABA90,0xAC900,0xAA4C0,0xA3DE0};
    const auto crt=GetModuleHandleW(L"MSVCP71.dll");
    const auto compare=crt ? GetProcAddress(crt,"??$?8DU?$char_traits@D@std@@V?$allocator@D@1@@std@@YA_NABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@0@PBD@Z") : nullptr;
    if (!compare || Read<void*>(g_image,0x272138)!=reinterpret_cast<void*>(compare)) {
        error="HUD legacy string comparison mismatch"; return false;
    }
    constexpr std::array<std::uint8_t,5> tool_call{0xE8,0x08,0x5E,0x02,0x00};
    std::array<std::uint8_t,5> actual_tool{};
    if (!Copy(g_image+0xA4313,actual_tool.data(),actual_tool.size()) || actual_tool!=tool_call) {
        error="HUD matrix call mismatch"; return false;
    }
    // Also validate each body-method slot used by direct native calls.
    for (const auto& pair : {std::array<std::uintptr_t,2>{0x34,0x19C2A0}, {0x3C,0x19C2C0},
        {0x54,0x19C360},{0x5C,0x19C380},{0xBC,0x19C590}})
        if (Read<void*>(g_image+0x292C08,pair[0])!=g_image+pair[1]) { error="Physics body method mismatch"; return false; }
    for (std::size_t i=0;i<slots.size();++i)
        if (Read<void*>(g_image,slots[i])!=g_image+targets[i]) { error="Spatial vtable mismatch"; return false; }
    constexpr std::array<std::uint8_t,8> matrix_entry{0x56,0x8B,0x74,0x24,0x08,0x57,0x8B,0xC1};
    std::array<std::uint8_t,8> actual{};
    if (!Copy(g_image+0xCA120,actual.data(),actual.size()) || actual!=matrix_entry) { error="Entity SetMatrix entry mismatch"; return false; }
    constexpr std::array<std::uint8_t,8> joints_entry{0x8B,0x91,0x54,0x03,0,0,0x85,0xD2};
    if (!Copy(g_image+0xCCF00,actual.data(),actual.size()) || actual!=joints_entry ||
        Read<void*>(g_image,0x27D0E4)!=g_image+0xA9FD0 ||
        Read<void*>(g_image,0x27D13C)!=g_image+0xAD6C0) { error="Spatial state helpers mismatch"; return false; }
    constexpr std::array<std::uint8_t,3> ray_call{0xFF,0x57,0x68};
    std::array<std::uint8_t,3> actual_ray{};
    if (!Copy(g_image+0xAD84F,actual_ray.data(),actual_ray.size()) || actual_ray!=ray_call) {
        error="Normal-state ray call mismatch"; return false;
    }
    // iPhysicsBody defaults CollideCharacter (+3C8) to true. The same byte is
    // consulted by the character-aware world query and character-body ray.
    constexpr std::array<std::uint8_t,7> collision_default{0xC6,0x85,0xC8,0x03,0,0,1};
    constexpr std::array<std::uint8_t,6> collision_world{0x8A,0x86,0xC8,0x03,0,0};
    constexpr std::array<std::uint8_t,6> collision_ray{0x8A,0x90,0xC8,0x03,0,0};
    constexpr std::array<std::uint8_t,6> collision_contact_a{0x8A,0x90,0xC8,0x03,0,0};
    constexpr std::array<std::uint8_t,6> collision_contact_b{0x8A,0x91,0xC8,0x03,0,0};
    std::array<std::uint8_t,7> actual_collision{};
    if (!Copy(g_image+0xCD877,actual_collision.data(),collision_default.size()) ||
        std::memcmp(actual_collision.data(),collision_default.data(),collision_default.size())!=0 ||
        !Copy(g_image+0xD4952,actual_collision.data(),collision_world.size()) ||
        std::memcmp(actual_collision.data(),collision_world.data(),collision_world.size())!=0 ||
        !Copy(g_image+0xD4E0E,actual_collision.data(),collision_ray.size()) ||
        std::memcmp(actual_collision.data(),collision_ray.data(),collision_ray.size())!=0 ||
        !Copy(g_image+0x19D2D0,actual_collision.data(),collision_contact_a.size()) ||
        std::memcmp(actual_collision.data(),collision_contact_a.data(),collision_contact_a.size())!=0 ||
        !Copy(g_image+0x19D2E4,actual_collision.data(),collision_contact_b.size()) ||
        std::memcmp(actual_collision.data(),collision_contact_b.data(),collision_contact_b.size())!=0) {
        error="CollideCharacter field mismatch"; return false;
    }
    const std::array<void*,5> replacements{reinterpret_cast<void*>(&HookedRay),reinterpret_cast<void*>(&HookedGrabUpdate),
        reinterpret_cast<void*>(&HookedEnter),reinterpret_cast<void*>(&HookedLeave),reinterpret_cast<void*>(&HookedHandsUpdate)};
    for (std::size_t i=0;i<slots.size();++i) {
        if (!hooks::InstallPointerHook(reinterpret_cast<void**>(g_image+slots[i]),g_image+targets[i],replacements[i],g_hooks[i],error)) {
            std::string rollback; static_cast<void>(RemoveSpatialInteraction(rollback));
            if (!rollback.empty()) error+="; "+rollback;
            return false;
        }
    }
    if (!hooks::InstallRel32CallHook(g_image+0xA4313,tool_call,reinterpret_cast<void*>(&HookedToolMatrix),g_tool_hook,error)) {
        std::string rollback; static_cast<void>(RemoveSpatialInteraction(rollback));
        if (!rollback.empty()) error+="; "+rollback;
        return false;
    }
    g_player_collision_filter_ready.store(true,std::memory_order_release);
    g_enabled.store(true,std::memory_order_release);
    return true;
}
bool RemoveSpatialInteraction(std::string& error) noexcept {
    error.clear();
    g_enabled.store(false,std::memory_order_release);
    g_player_collision_filter_ready.store(false,std::memory_order_release);
    const auto deadline=GetTickCount64()+1000;
    while (g_callbacks.load(std::memory_order_acquire) && GetTickCount64()<deadline) Sleep(1);
    if (g_callbacks.load(std::memory_order_acquire)) { error="Spatial callbacks are still active"; return false; }
    if (g_held.load(std::memory_order_acquire)) { error="Release the tracked body before removing spatial hooks"; return false; }
    bool success=true;
    if (!hooks::RemoveRel32CallHook(g_tool_hook,error)) success=false;
    for (auto& hook:g_hooks) { std::string next; if (!hooks::RemoveIatHook(hook,next)) { success=false; error+=next; } }
    return success;
}
void RefreshVrSelectionBeforeInteract(void* player) noexcept {
    CallbackScope scope;
    if (!g_enabled.load(std::memory_order_acquire) || !player || NativeInputUiActive() ||
        Read<int>(player,0x2BC)!=0) return;
    // Resolve on each input tick: never retain a state/player pointer across maps.
    auto* states=Read<void*>(player,0x2C4);
    auto* normal=Read<void*>(states,0);
    if (!normal || Read<void*>(normal,0)!=g_image+0x27D138 || Read<void*>(normal,0x10)!=player) return;
    // This mapped update has no time integration. It clears/recasts the native
    // pick callback and computes the crosshair, including script eligibility.
    reinterpret_cast<Update>(g_image+0xAD6C0)(normal,0);
}
void ServiceSpatialInteraction(void* player, bool ui) noexcept {
    CallbackScope scope;
    if (g_pending_state) {
        auto* pending = g_pending_state;
        g_pending_state = nullptr;
        if (!ui && player && Read<int>(player,0x2BC)==6 &&
            Read<void*>(Read<void*>(player,0x2C4),6*sizeof(void*))==pending &&
            Read<void*>(pending,0x10)==player) AcquirePendingGrab(pending);
    }
    if (!g_held.load(std::memory_order_acquire)) return;
    // A different player means a lifecycle mismatch. Never dereference the old
    // state from an unrelated map; retain hooks and report refusal at detach.
    if (!player || Read<int>(player,0x2BC)!=6 ||
        Read<void*>(Read<void*>(player,0x2C4),6*sizeof(void*))!=g_hold.state ||
        Read<void*>(g_hold.state,0x10)!=player) return;
    const auto frame=ReadNativeControllerFrame();
    Matrix palm; Vec velocity{},angular{};
    const bool valid_pose=HandPose(g_hold.hand,false,palm,velocity,angular);
    if (!g_enabled.load(std::memory_order_acquire) || ui || !frame.focused ||
        !frame.input.state.interact.pressed || !valid_pose) {
        g_hold.discard_momentum=!g_enabled.load(std::memory_order_acquire) || ui || !frame.focused || !valid_pose;
        reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xA9FD0)(g_hold.state);
    }
}
}
