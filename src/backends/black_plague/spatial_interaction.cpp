// Exact-build adapter for the initialized Black Plague FD316F... image.
// Native Enter/Leave retain ownership of HPL's mass/gravity/script transitions.
#include "spatial_interaction.hpp"
#include "native_input_bridge.hpp"
#include "render_world_probe.hpp"
#include "vr_grab_pose.hpp"
#include "iat_hook.hpp"
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
std::array<hooks::IatHook,4> g_hooks;
std::atomic<bool> g_enabled{false};
std::atomic<unsigned> g_callbacks{0};
struct CallbackScope {
    CallbackScope() { g_callbacks.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackScope() { g_callbacks.fetch_sub(1, std::memory_order_acq_rel); }
};
std::atomic<bool> g_held{false};
struct Hold {
    void* state = nullptr;
    void* body = nullptr;
    runtime::VrHand hand = runtime::VrHand::right;
    runtime::VrGrabPose pose;
    float max_linear = 0, max_angular = 0;
    Vec previous_palm{};
    bool discard_momentum = false;
} g_hold;

bool Copy(const void* source, void* dest, std::size_t size) noexcept {
    if (!source) return false;
    __try { std::memcpy(dest,source,size); return true; }
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
    reinterpret_cast<Transition>(g_image+0xAC900)(state,previous);
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
    const auto local_contact=Read<Vec>(state,0x14);
    std::string error;
    if (!hold.pose.Begin(palm,body_pose,local_contact,Read<bool>(state,0xE1),error)) return;
    hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    g_hold=hold;
    SetFloat(body,0x19C360,20); SetFloat(body,0x19C380,30);
    g_held.store(true,std::memory_order_release);
    NativeControllerHaptic(hold.hand,true);
}
void __fastcall HookedLeave(void* state, void*, void* next) {
    CallbackScope scope;
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

bool InstallSpatialInteraction(std::string& error) noexcept {
    error.clear();
    if (g_enabled.load(std::memory_order_acquire)) return true;
    g_image=reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    constexpr std::array<std::uintptr_t,4> slots{0x291BE8,0x27D0D4,0x27D12C,0x27D130};
    constexpr std::array<std::uintptr_t,4> targets{0x189E30,0xABA90,0xAC900,0xAA4C0};
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
    const std::array<void*,4> replacements{reinterpret_cast<void*>(&HookedRay),reinterpret_cast<void*>(&HookedGrabUpdate),
        reinterpret_cast<void*>(&HookedEnter),reinterpret_cast<void*>(&HookedLeave)};
    for (std::size_t i=0;i<slots.size();++i) {
        if (!hooks::InstallPointerHook(reinterpret_cast<void**>(g_image+slots[i]),g_image+targets[i],replacements[i],g_hooks[i],error)) {
            std::string rollback; static_cast<void>(RemoveSpatialInteraction(rollback));
            if (!rollback.empty()) error+="; "+rollback;
            return false;
        }
    }
    g_enabled.store(true,std::memory_order_release);
    return true;
}
bool RemoveSpatialInteraction(std::string& error) noexcept {
    error.clear();
    g_enabled.store(false,std::memory_order_release);
    const auto deadline=GetTickCount64()+1000;
    while (g_callbacks.load(std::memory_order_acquire) && GetTickCount64()<deadline) Sleep(1);
    if (g_callbacks.load(std::memory_order_acquire)) { error="Spatial callbacks are still active"; return false; }
    if (g_held.load(std::memory_order_acquire)) { error="Release the tracked body before removing spatial hooks"; return false; }
    bool success=true;
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
