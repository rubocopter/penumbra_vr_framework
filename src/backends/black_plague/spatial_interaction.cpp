// Exact-build adapter for the initialized Black Plague FD316F... image.
// Native Enter/Leave retain ownership of HPL's mass/gravity/script transitions.
#include "spatial_interaction.hpp"
#include "hand_contact_probe.hpp"
#include "native_input_bridge.hpp"
#include "render_world_probe.hpp"
#include "vr_grab_pose.hpp"
#include "vr_interaction_policy.hpp"
#include "vr_rework_hand_profile.hpp"
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
constexpr std::array<float,9> kToolModelToHandRotation{
    1,0,0,
    0,0,-1,
    0,1,0};
constexpr runtime::VrAttachmentSocketProfile kFlashlightSocket{
    kToolModelToHandRotation,{0,-0.016669F,0}};
constexpr runtime::VrAttachmentSocketProfile kGlowstickSocket{
    kToolModelToHandRotation,{0,0.059722F,0.00504F}};
std::uint8_t* g_image = nullptr;
std::array<hooks::IatHook,8> g_hooks;
hooks::Rel32CallHook g_tool_hook;
thread_local void* g_updating_hands = nullptr;
std::atomic<std::uint64_t> g_tools_attached{0},g_tools_native{0},g_invalid_tool_pose{0},g_blocked_grabs{0};
std::atomic<std::uint64_t> g_grabs_acquired{0},g_grabs_released{0},g_guarded_releases{0},g_collision_restore_failures{0};
std::atomic<std::uint64_t> g_moves_acquired{0},g_moves_released{0};
std::atomic<std::uint64_t> g_contact_rays{0};
std::atomic<bool> g_enabled{false};
std::atomic<unsigned> g_callbacks{0};
struct CallbackScope {
    CallbackScope() { g_callbacks.fetch_add(1, std::memory_order_acq_rel); }
    ~CallbackScope() { g_callbacks.fetch_sub(1, std::memory_order_acq_rel); }
};
std::atomic<bool> g_held{false};
std::atomic<bool> g_move_held{false};
std::atomic<void*> g_player_identity{nullptr};
std::atomic<std::uint64_t> g_player_generation{0};
thread_local bool g_vr_selection_ready = false;
thread_local void* g_vr_selection_player = nullptr;
// HPL's per-body CollideCharacter flag is consumed by world collision queries,
// character rays and Newton's character/body contact filtering. Never move a
// tracked body until the exact-build field and its consumers have been proved.
std::atomic<bool> g_player_collision_filter_ready{false};
void* g_pending_state = nullptr; // Input thread only; never dereferenced without current-state identity.
void* g_pending_move_state = nullptr;
struct Hold {
    void* state = nullptr;
    void* player = nullptr;
    void* body = nullptr;
    std::uint64_t player_generation = 0;
    runtime::VrHand hand = runtime::VrHand::right;
    runtime::VrGrabPose pose;
    runtime::VrReleaseVelocity release_velocity;
    float max_linear = 0, max_angular = 0;
    Vec previous_palm{};
    bool collide_character = true;
    bool discard_momentum = false;
} g_hold;
struct MoveHold {
    void* state = nullptr;
    void* player = nullptr;
    void* body = nullptr;
    std::uint64_t player_generation = 0;
    runtime::VrHand hand = runtime::VrHand::right;
    Vec local_body_contact{};
    Vec local_hand_contact{};
    Vec previous_palm{};
    float max_linear = 0, max_angular = 0;
} g_move_hold;

[[nodiscard]] std::uint64_t ObservePlayerGeneration(void* player) noexcept {
    void* const previous = g_player_identity.exchange(player, std::memory_order_acq_rel);
    if (previous != player) {
        return g_player_generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
    return g_player_generation.load(std::memory_order_acquire);
}

[[nodiscard]] Vec TransformPoint(const Matrix& matrix, const Vec& point) noexcept {
    return {
        matrix.values[0] * point[0] + matrix.values[1] * point[1] +
            matrix.values[2] * point[2] + matrix.values[3],
        matrix.values[4] * point[0] + matrix.values[5] * point[1] +
            matrix.values[6] * point[2] + matrix.values[7],
        matrix.values[8] * point[0] + matrix.values[9] * point[1] +
            matrix.values[10] * point[2] + matrix.values[11]};
}

[[nodiscard]] Vec InverseTransformPoint(const Matrix& matrix, const Vec& point) noexcept {
    const Vec delta{
        point[0]-matrix.values[3], matrix.values[7] == matrix.values[7] ? point[1]-matrix.values[7] : 0.0F, point[2]-matrix.values[11]};
    return {
        matrix.values[0]*delta[0] + matrix.values[4]*delta[1] + matrix.values[8]*delta[2],
        matrix.values[1]*delta[0] + matrix.values[5]*delta[1] + matrix.values[9]*delta[2],
        matrix.values[2]*delta[0] + matrix.values[6]*delta[1] + matrix.values[10]*delta[2]};
}

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
void AddForceAtPosition(void* body, const Vec& force, const Vec& position) {
    reinterpret_cast<void(__thiscall*)(void*,const Vec*,const Vec*)>(g_image+0x19C9E0)(body,&force,&position);
}
bool RawHandPose(runtime::VrHand hand, bool aim, Matrix& pose, Vec& velocity, Vec& angular) {
    const auto frame = ReadNativeControllerFrame();
    if (!frame.focused) return false;
    const auto& sample = frame.hands[hand == runtime::VrHand::left ? 0 : 1];
    const auto& tracking = aim && sample.aim.pose_valid ? sample.aim : sample.grip;
    return ControllerWorldPose(tracking,pose,velocity,angular);
}
bool HandPose(runtime::VrHand hand, bool aim, Matrix& pose, Vec& velocity, Vec& angular) {
    if (!RawHandPose(hand,aim,pose,velocity,angular)) return false;
    if (!aim) {
        runtime::VrMatrix44 resolved{};
        const std::size_t hand_index = hand == runtime::VrHand::left ? 0U : 1U;
        if (ReadGameplayPalmPose(hand_index,resolved)) pose=resolved;
        pose=runtime::rework_hand_profile::ApplyVisualLocalPose(pose);
    }
    return true;
}
bool InteractionHandPose(runtime::VrHand hand, Matrix& pose, Vec& velocity, Vec& angular) {
    Matrix raw{};
    if (!RawHandPose(hand,false,raw,velocity,angular)) return false;
    pose=raw;
    runtime::VrMatrix44 resolved{};
    const std::size_t hand_index = hand == runtime::VrHand::left ? 0U : 1U;
    if (ReadGameplayPalmPose(hand_index,resolved)) {
        const Vec reach{
            raw.values[3]-resolved.values[3],
            raw.values[7]-resolved.values[7],
            raw.values[11]-resolved.values[11]};
        const float distance=std::hypot(reach[0],reach[1],reach[2]);
        if (!std::isfinite(distance)) return false;
        const float scale=distance>runtime::vr_interaction_policy::kMaximumCollisionInteractionReach && distance>0 ?
            runtime::vr_interaction_policy::kMaximumCollisionInteractionReach/distance : 1.0F;

        // Rework 23c890f keeps the visible/physical palm collision-resolved,
        // but lets target acquisition follow the real controller a bounded
        // distance beyond it. Preserve raw orientation and clamp only the
        // translation from the resolved palm toward the raw palm.
        pose.values[3]=resolved.values[3]+reach[0]*scale;
        pose.values[7]=resolved.values[7]+reach[1]*scale;
        pose.values[11]=resolved.values[11]+reach[2]*scale;
    }
    // Rework GetHandPalmPose appends the same imported hand translation and
    // rotation used by the renderer. Keep selection, held props and tools on
    // that visible palm frame rather than the controller origin.
    pose=runtime::rework_hand_profile::ApplyVisualLocalPose(pose);
    return true;
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
            const auto tool_hand=ReadNativeControllerFrame().interact_source==runtime::VrHand::left ?
                runtime::VrHand::right : runtime::VrHand::left;
            if (!HandPose(tool_hand,false,palm,velocity,angular)) { ++g_invalid_tool_pose; break; }
            // Native models point along -Y. The per-game profile rotates +90
            // degrees around X so the flashlight beam points along controller
            // -Z, then aligns the measured model socket with the hand origin.
            // These sockets come from BP's installed nodes, not Rework's DAE.
            const auto& socket=flashlight ? kFlashlightSocket : kGlowstickSocket;
            destination=runtime::ComposeAttachmentSocketPose(palm,socket);
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
            InteractionHandPose(frame.interact_source,pose,velocity,angular)) {
            const float native_length = std::hypot(to[0]-from[0],to[1]-from[1],to[2]-from[2]);
            if (std::isfinite(native_length) && native_length > 0 && native_length <= 20) {
                // Rework uses palm overlap for props and reserves long-range
                // aim assistance for classified inventory items. Until the
                // binary backend maps that entity classifier, keep this ray
                // inside Rework's collision-to-raw-palm reach instead of
                // granting every prop the native 1.9 m camera reach.
                const float length =
                    runtime::vr_interaction_policy::ClampPhysicalInteractionReach(
                        native_length);
                for (std::size_t row=0;row<3;++row) {
                    from[row]=pose.values[row*4+3]; to[row]=from[row]-pose.values[row*4+2]*length;
                }
                g_vr_selection_ready = true;
                ++g_contact_rays;
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
    auto* const player = Read<void*>(state,0x10);
    g_pending_state = g_enabled.load(std::memory_order_acquire) && frame.focused &&
        frame.input.state.interact.just_pressed && g_vr_selection_ready &&
        g_vr_selection_player == player ? state : nullptr;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    reinterpret_cast<Transition>(g_image+0xAC900)(state,previous);
    // ChangeState publishes player+2BC AFTER Enter returns (9CABB).
    // Acquire only when ServiceSpatialInteraction observes the committed state.
}

void __fastcall HookedMoveEnter(void* state, void*, void* previous) {
    CallbackScope scope;
    const auto frame = ReadNativeControllerFrame();
    auto* const player = Read<void*>(state,0x10);
    g_pending_move_state = g_enabled.load(std::memory_order_acquire) && frame.focused &&
        frame.input.state.interact.just_pressed && g_vr_selection_ready &&
        g_vr_selection_player == player ? state : nullptr;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    reinterpret_cast<Transition>(g_image+0xAAC80)(state,previous);
}

void AcquirePendingMove(void* state, std::uint64_t player_generation) {
    const auto frame=ReadNativeControllerFrame();
    auto* const player=Read<void*>(state,0x10);
    auto* const body=Read<void*>(state,0x54);
    if (!g_enabled.load(std::memory_order_acquire) || g_held.load() || g_move_held.load() ||
        !frame.focused || !frame.input.state.interact.just_pressed ||
        Read<int>(player,0x2BC)!=2 || !BodyMatches(body) ||
        Read<void*>(body,0x330)!=nullptr || Read<void*>(body,0x10)!=nullptr ||
        reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(body)!=0) return;
    Matrix palm,interaction_pose,body_pose; Vec velocity{},angular{},interaction_velocity{},interaction_angular{};
    if (!HandPose(frame.interact_source,false,palm,velocity,angular) ||
        !InteractionHandPose(frame.interact_source,interaction_pose,interaction_velocity,interaction_angular) ||
        !Copy(static_cast<std::uint8_t*>(body)+0x34,&body_pose,sizeof(body_pose))) return;
    const float max_linear=Read<float>(body,0x42C),max_angular=Read<float>(body,0x430);
    const float mass=Read<float>(body,0x434);
    if (!std::isfinite(max_linear) || !std::isfinite(max_angular) || max_linear<0 || max_angular<0 ||
        !std::isfinite(mass) || mass<=0) return;
    const auto local_body_contact=Read<Vec>(state,0x38);
    const auto world_contact=TransformPoint(body_pose,local_body_contact);
    const float contact_distance=std::hypot(
        world_contact[0]-interaction_pose.values[3], world_contact[1]-interaction_pose.values[7],
        world_contact[2]-interaction_pose.values[11]);
    if (!std::isfinite(contact_distance) || contact_distance>
        runtime::vr_interaction_policy::kMaximumCollisionInteractionReach+
            runtime::vr_interaction_policy::kInteractionContactTolerance) {
        ++g_blocked_grabs;
        return;
    }
    MoveHold hold;
    hold.state=state; hold.player=player; hold.body=body;
    hold.player_generation=player_generation; hold.hand=frame.interact_source;
    hold.local_body_contact=local_body_contact;
    hold.local_hand_contact=InverseTransformPoint(palm,world_contact);
    hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    hold.max_linear=max_linear; hold.max_angular=max_angular;
    g_move_hold=hold;
    PublishGameplayPalmHeldBody(
        hold.hand == runtime::VrHand::left ? 0U : 1U, hold.body);
    // Rework raises these caps while a free Move body is hand-driven so the
    // Newton force is not strangled by map-authored carrying limits.
    SetFloat(body,0x19C360,10); SetFloat(body,0x19C380,15);
    g_move_held.store(true,std::memory_order_release);
    ++g_moves_acquired;
    NativeControllerHaptic(hold.hand,true);
}

void RestoreMoveBody(const MoveHold& hold) noexcept {
    if (!BodyMatches(hold.body)) return;
    SetFloat(hold.body,0x19C360,hold.max_linear);
    SetFloat(hold.body,0x19C380,hold.max_angular);
}

void __fastcall HookedMoveLeave(void* state, void*, void* next) {
    CallbackScope scope;
    if (g_pending_move_state == state) g_pending_move_state = nullptr;
    const bool owned=g_move_held.load(std::memory_order_acquire) && g_move_hold.state==state;
    MoveHold hold;
    if (owned) {
        hold=g_move_hold;
        g_move_hold={};
        g_move_held.store(false,std::memory_order_release);
        PublishGameplayPalmHeldBody(
            hold.hand == runtime::VrHand::left ? 0U : 1U, nullptr);
    }
    reinterpret_cast<Transition>(g_image+0xAAED0)(state,next);
    if (owned) {
        RestoreMoveBody(hold);
        NativeControllerHaptic(hold.hand,false);
        ++g_moves_released;
    }
}

void __fastcall HookedMoveUpdate(void* state, void*, float dt) {
    CallbackScope scope;
    if (!g_move_held.load(std::memory_order_acquire) || g_move_hold.state!=state) {
        reinterpret_cast<Update>(g_image+0xAA690)(state,dt);
        return;
    }
    if (dt==0) return;
    Matrix palm,body_pose; Vec velocity{},angular{};
    bool valid=g_enabled.load(std::memory_order_acquire) && BodyMatches(g_move_hold.body) &&
        std::isfinite(dt) && dt>0 && dt<=0.25F &&
        HandPose(g_move_hold.hand,false,palm,velocity,angular) &&
        Copy(static_cast<std::uint8_t*>(g_move_hold.body)+0x34,&body_pose,sizeof(body_pose));
    if (valid) {
        const float displacement=std::hypot(
            palm.values[3]-g_move_hold.previous_palm[0],
            palm.values[7]-g_move_hold.previous_palm[1],
            palm.values[11]-g_move_hold.previous_palm[2]);
        valid=displacement<=0.35F;
    }
    if (!valid) {
        ++g_guarded_releases;
        reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xAA030)(state);
        return;
    }
    g_move_hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    const auto current_contact=TransformPoint(body_pose,g_move_hold.local_body_contact);
    const auto target_contact=TransformPoint(palm,g_move_hold.local_hand_contact);
    const float mass=Read<float>(g_move_hold.body,0x434);
    if (!std::isfinite(mass) || mass<=0) {
        ++g_guarded_releases;
        reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xAA030)(state);
        return;
    }
    Vec force{};
    for (std::size_t axis=0;axis<3;++axis)
        force[axis]=(target_contact[axis]-current_contact[axis])*1250.0F*mass;
    // Rework's free Move body applies this force at the selected surface point.
    // Keep the native body/joint route for mechanisms; only zero-joint bodies
    // can reach this owner.
    AddForceAtPosition(g_move_hold.body,force,current_contact);
    static_cast<void>(Store(static_cast<std::uint8_t*>(state)+0x44,
        current_contact.data(),sizeof(current_contact)));
    const int move_count=20;
    static_cast<void>(Store(static_cast<std::uint8_t*>(state)+0x58,&move_count,sizeof(move_count)));
}
void AcquirePendingGrab(void* state, std::uint64_t player_generation) {
    if (!g_player_collision_filter_ready.load(std::memory_order_acquire)) { ++g_blocked_grabs; return; }
    const auto frame = ReadNativeControllerFrame();
    auto* player=Read<void*>(state,0x10);
    auto* body=Read<void*>(state,0x20);
    if (!g_enabled.load(std::memory_order_acquire) || g_held.load() || !frame.focused || !frame.input.state.interact.just_pressed ||
        Read<int>(player,0x2BC)!=6 || !BodyMatches(body) || Read<void*>(body,0x330)!=nullptr ||
        Read<void*>(body,0x10)!=nullptr ||
        reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(body)!=0) return;
    Matrix palm,interaction_pose,body_pose; Vec velocity{},angular{},interaction_velocity{},interaction_angular{};
    if (!HandPose(frame.interact_source,false,palm,velocity,angular) ||
        !InteractionHandPose(frame.interact_source,interaction_pose,interaction_velocity,interaction_angular) ||
        !Copy(static_cast<std::uint8_t*>(body)+0x34,&body_pose,sizeof(body_pose))) return;
    const float max_linear=Read<float>(body,0x42C),max_angular=Read<float>(body,0x430);
    const float mass=Read<float>(body,0x434);
    if (!std::isfinite(max_linear) || !std::isfinite(max_angular) || max_linear<0 || max_angular<0 ||
        !std::isfinite(mass) || mass<=0) return;
    Hold hold; hold.state=state; hold.player=player; hold.body=body;
    hold.player_generation=player_generation; hold.hand=frame.interact_source;
    hold.max_linear=max_linear; hold.max_angular=max_angular;
    hold.collide_character=Read<bool>(body,0x3C8);
    const auto local_contact=Read<Vec>(state,0x14);
    const auto world_contact=TransformPoint(body_pose,local_contact);
    const float contact_distance=std::hypot(
        world_contact[0]-interaction_pose.values[3],
        world_contact[1]-interaction_pose.values[7],
        world_contact[2]-interaction_pose.values[11]);
    if (!std::isfinite(contact_distance) || contact_distance>
        runtime::vr_interaction_policy::kMaximumCollisionInteractionReach+
            runtime::vr_interaction_policy::kInteractionContactTolerance) {
        ++g_blocked_grabs;
        return;
    }
    std::string error;
    if (!hold.pose.Begin(palm,body_pose,local_contact,Read<bool>(state,0xE1),error)) return;
    hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    const bool no_character_collision=false;
    if (!Store(static_cast<std::uint8_t*>(body)+0x3C8,&no_character_collision,sizeof(no_character_collision))) return;
    g_hold=hold;
    PublishGameplayPalmHeldBody(
        hold.hand == runtime::VrHand::left ? 0U : 1U, hold.body);
    SetFloat(body,0x19C360,20); SetFloat(body,0x19C380,30);
    g_held.store(true,std::memory_order_release);
    ++g_grabs_acquired;
    NativeControllerHaptic(hold.hand,true);
}

void RestoreHeldBody(const Hold& hold, const Vec& velocity,
    const Vec& angular) noexcept {
    if (!BodyMatches(hold.body)) return;
    if (!Store(static_cast<std::uint8_t*>(hold.body)+0x3C8,
            &hold.collide_character,sizeof(hold.collide_character))) {
        ++g_collision_restore_failures;
    }
    SetFloat(hold.body,0x19C360,hold.max_linear);
    SetFloat(hold.body,0x19C380,hold.max_angular);
    SetVelocity(hold.body,0x19C2A0,
        runtime::LimitTrackedVelocity(velocity,1.25F,9));
    SetVelocity(hold.body,0x19C2C0,
        runtime::LimitTrackedVelocity(angular,0.5F,6));
}

void __fastcall HookedLeave(void* state, void*, void* next) {
    CallbackScope scope;
    if (g_pending_state == state) g_pending_state = nullptr;
    const bool owned=g_held.load(std::memory_order_acquire) && g_hold.state==state;
    Hold hold;
    Vec velocity{},angular{};
    if (owned) {
        hold=g_hold;
        if (!hold.discard_momentum) hold.release_velocity.Estimate(velocity,angular);
        g_hold={}; g_held.store(false,std::memory_order_release);
        PublishGameplayPalmHeldBody(
            hold.hand == runtime::VrHand::left ? 0U : 1U, nullptr);
    }
    reinterpret_cast<Transition>(g_image+0xAA4C0)(state,next);
    if (owned) {
        RestoreHeldBody(hold,velocity,angular);
        NativeControllerHaptic(hold.hand,false);
        ++g_grabs_released;
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
        valid=displacement<=0.35F;
    }
    if (!valid) {
        g_hold.discard_momentum=true;
        ++g_guarded_releases;
        reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xA9FD0)(state);
        return;
    }
    g_hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    g_hold.release_velocity.Add(velocity,angular);
    reinterpret_cast<void(__thiscall*)(void*,bool)>(g_image+0x19C590)(g_hold.body,false);
    // SetMatrix not a raw write: native transform callbacks update Newton and
    // the linked scene node. Parented/jointed bodies never enter this path.
    reinterpret_cast<void(__thiscall*)(void*,const Matrix*)>(g_image+0xCA120)(g_hold.body,&destination);
    SetVelocity(g_hold.body,0x19C2A0,runtime::LimitTrackedVelocity(velocity,1,9));
    SetVelocity(g_hold.body,0x19C2C0,runtime::LimitTrackedVelocity(angular,0.5F,6));
}

[[nodiscard]] bool AllSpatialHooksInstalled() noexcept {
    if (!g_tool_hook.installed()) return false;
    for (const auto& hook:g_hooks) if (!hook.installed()) return false;
    return true;
}

[[nodiscard]] bool AnySpatialHookInstalled() noexcept {
    if (g_tool_hook.installed()) return true;
    for (const auto& hook:g_hooks) if (hook.installed()) return true;
    return false;
}

void AppendRemovalError(std::string& error, const std::string& next) {
    if (next.empty()) return;
    if (!error.empty()) error += "; ";
    error += next;
}
}

SpatialDiagnostics ConsumeSpatialDiagnostics() noexcept {
    return {g_tools_attached.exchange(0),g_tools_native.exchange(0),g_invalid_tool_pose.exchange(0),
        g_blocked_grabs.exchange(0),g_grabs_acquired.exchange(0),g_grabs_released.exchange(0),
        g_moves_acquired.exchange(0),g_moves_released.exchange(0),
        g_guarded_releases.exchange(0),g_collision_restore_failures.exchange(0),
        g_contact_rays.exchange(0)};
}
bool InstallSpatialInteraction(std::string& error) noexcept {
    error.clear();
    if (AllSpatialHooksInstalled()) {
        g_player_collision_filter_ready.store(true,std::memory_order_release);
        g_enabled.store(true,std::memory_order_release);
        return true;
    }
    if (AnySpatialHookInstalled()) {
        error="Spatial interaction is only partially installed";
        return false;
    }
    g_player_collision_filter_ready.store(false,std::memory_order_release);
    auto* const image=reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (!image) { error="The Black Plague image is unavailable"; return false; }
    if (!g_image) {
        // Keep the module base immutable after first publication. A wrapper can
        // already have branched through a restored hook while removal observes
        // zero active callbacks, so a later install must not rewrite g_image.
        g_image=image;
    } else if (g_image!=image) {
        error="The Black Plague image base changed after spatial hook publication";
        return false;
    }
    constexpr std::array<std::uintptr_t,8> slots{
        0x291BE8,0x27D0D4,0x27D12C,0x27D130,0x27CB70,
        0x27CF74,0x27CFCC,0x27CFD0};
    constexpr std::array<std::uintptr_t,8> targets{
        0x189E30,0xABA90,0xAC900,0xAA4C0,0xA3DE0,
        0xAA690,0xAAC80,0xAAED0};
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
        {0x54,0x19C360},{0x5C,0x19C380},{0x7C,0x19C9E0},{0xBC,0x19C590}})
        if (Read<void*>(g_image+0x292C08,pair[0])!=g_image+pair[1]) { error="Physics body method mismatch"; return false; }
    for (std::size_t i=0;i<slots.size();++i)
        if (Read<void*>(g_image,slots[i])!=g_image+targets[i]) { error="Spatial vtable mismatch"; return false; }
    constexpr std::array<std::uint8_t,8> matrix_entry{0x56,0x8B,0x74,0x24,0x08,0x57,0x8B,0xC1};
    std::array<std::uint8_t,8> actual{};
    if (!Copy(g_image+0xCA120,actual.data(),actual.size()) || actual!=matrix_entry) { error="Entity SetMatrix entry mismatch"; return false; }
    constexpr std::array<std::uint8_t,8> joints_entry{0x8B,0x91,0x54,0x03,0,0,0x85,0xD2};
    if (!Copy(g_image+0xCCF00,actual.data(),actual.size()) || actual!=joints_entry ||
        Read<void*>(g_image,0x27D0E4)!=g_image+0xA9FD0 ||
        Read<void*>(g_image,0x27CF84)!=g_image+0xAA030 ||
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
    const std::array<void*,8> replacements{
        reinterpret_cast<void*>(&HookedRay),reinterpret_cast<void*>(&HookedGrabUpdate),
        reinterpret_cast<void*>(&HookedEnter),reinterpret_cast<void*>(&HookedLeave),
        reinterpret_cast<void*>(&HookedHandsUpdate),reinterpret_cast<void*>(&HookedMoveUpdate),
        reinterpret_cast<void*>(&HookedMoveEnter),reinterpret_cast<void*>(&HookedMoveLeave)};
    for (std::size_t i=0;i<slots.size();++i) {
        if (!hooks::InstallPointerHook(reinterpret_cast<void**>(g_image+slots[i]),g_image+targets[i],replacements[i],g_hooks[i],error)) {
            const std::string install_error=error;
            std::string rollback; static_cast<void>(RemoveSpatialInteraction(rollback));
            error=install_error;
            if (!rollback.empty()) error+="; rollback failed: "+rollback;
            return false;
        }
    }
    if (!hooks::InstallRel32CallHook(g_image+0xA4313,tool_call,reinterpret_cast<void*>(&HookedToolMatrix),g_tool_hook,error)) {
        const std::string install_error=error;
        std::string rollback; static_cast<void>(RemoveSpatialInteraction(rollback));
        error=install_error;
        if (!rollback.empty()) error+="; rollback failed: "+rollback;
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
    if (g_held.load(std::memory_order_acquire) || g_move_held.load(std::memory_order_acquire)) {
        error="Release the tracked body before removing spatial hooks";
        return false;
    }
    bool success=true;
    std::string next;
    if (!hooks::RemoveRel32CallHook(g_tool_hook,next)) {
        success=false;
        AppendRemovalError(error,next);
    }
    for (auto iterator=g_hooks.rbegin();iterator!=g_hooks.rend();++iterator) {
        next.clear();
        if (!hooks::RemoveIatHook(*iterator,next)) {
            success=false;
            AppendRemovalError(error,next);
        }
    }
    if (AnySpatialHookInstalled()) {
        success=false;
        if (error.empty()) error="Spatial interaction remains partially installed";
        return false;
    }
    // A wrapper may already have branched into our code when its callsite is
    // restored. Stop new dispatches first, then wait for published callbacks.
    const auto deadline=GetTickCount64()+1000;
    while (g_callbacks.load(std::memory_order_acquire) && GetTickCount64()<deadline) Sleep(1);
    if (g_callbacks.load(std::memory_order_acquire)) {
        success=false;
        AppendRemovalError(error,"Spatial callbacks are still active");
    }
    return success;
}
void RefreshVrSelectionBeforeInteract(void* player) noexcept {
    CallbackScope scope;
    g_vr_selection_ready=false;
    g_vr_selection_player=nullptr;
    if (!g_enabled.load(std::memory_order_acquire) || !player || NativeInputUiActive() ||
        Read<int>(player,0x2BC)!=0) return;
    // Resolve on each input tick: never retain a state/player pointer across maps.
    auto* states=Read<void*>(player,0x2C4);
    auto* normal=Read<void*>(states,0);
    if (!normal || Read<void*>(normal,0)!=g_image+0x27D138 || Read<void*>(normal,0x10)!=player) return;
    // This mapped update has no time integration. It clears/recasts the native
    // pick callback and computes the crosshair, including script eligibility.
    g_vr_selection_player=player;
    reinterpret_cast<Update>(g_image+0xAD6C0)(normal,0);
    if (!g_vr_selection_ready) g_vr_selection_player=nullptr;
}
void ServiceSpatialInteraction(void* player, bool ui) noexcept {
    CallbackScope scope;
    const std::uint64_t player_generation=ObservePlayerGeneration(player);
    if (g_pending_state) {
        auto* pending = g_pending_state;
        g_pending_state = nullptr;
        if (!ui && player && Read<int>(player,0x2BC)==6 &&
            Read<void*>(Read<void*>(player,0x2C4),6*sizeof(void*))==pending &&
            Read<void*>(pending,0x10)==player)
            AcquirePendingGrab(pending,player_generation);
    }
    if (g_pending_move_state) {
        auto* pending = g_pending_move_state;
        g_pending_move_state = nullptr;
        if (!ui && player && Read<int>(player,0x2BC)==2 &&
            Read<void*>(Read<void*>(player,0x2C4),2*sizeof(void*))==pending &&
            Read<void*>(pending,0x10)==player)
            AcquirePendingMove(pending,player_generation);
    }
    if (g_move_held.load(std::memory_order_acquire)) {
        if (!player || g_move_hold.player!=player ||
            g_move_hold.player_generation!=player_generation) {
            const auto hand=g_move_hold.hand;
            g_move_hold={};
            g_move_held.store(false,std::memory_order_release);
            PublishGameplayPalmHeldBody(
                hand == runtime::VrHand::left ? 0U : 1U, nullptr);
            ++g_guarded_releases;
            NativeControllerHaptic(hand,false);
            return;
        }
        auto* const states=Read<void*>(player,0x2C4);
        auto* const registered_move_state=Read<void*>(states,2*sizeof(void*));
        const bool state_registered=registered_move_state==g_move_hold.state;
        const bool state_owned=state_registered && Read<void*>(g_move_hold.state,0x10)==player;
        if (Read<int>(player,0x2BC)!=2 || !state_owned) {
            const MoveHold hold=g_move_hold;
            g_move_hold={};
            g_move_held.store(false,std::memory_order_release);
            PublishGameplayPalmHeldBody(
                hold.hand == runtime::VrHand::left ? 0U : 1U, nullptr);
            ++g_guarded_releases;
            if (state_owned) RestoreMoveBody(hold);
            NativeControllerHaptic(hold.hand,false);
            ++g_moves_released;
            return;
        }
        const auto frame=ReadNativeControllerFrame();
        Matrix palm; Vec velocity{},angular{};
        const bool valid_pose=HandPose(g_move_hold.hand,false,palm,velocity,angular);
        if (!g_enabled.load(std::memory_order_acquire) || ui || !frame.focused ||
            !frame.input.state.interact.pressed || !valid_pose) {
            if (!g_enabled.load(std::memory_order_acquire) || ui || !frame.focused || !valid_pose)
                ++g_guarded_releases;
            reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xAA030)(g_move_hold.state);
        }
        return;
    }
    if (!g_held.load(std::memory_order_acquire)) return;
    // A different generation makes the old state/body lifetime uncertain.
    // Drop ownership without dereferencing or restoring stale native memory.
    if (!player || g_hold.player!=player ||
        g_hold.player_generation!=player_generation) {
        const auto hand=g_hold.hand;
        g_hold={};
        g_held.store(false,std::memory_order_release);
        PublishGameplayPalmHeldBody(
            hand == runtime::VrHand::left ? 0U : 1U, nullptr);
        ++g_guarded_releases;
        NativeControllerHaptic(hand,false);
        return;
    }
    auto* const states=Read<void*>(player,0x2C4);
    auto* const registered_grab_state=
        Read<void*>(states,6*sizeof(void*));
    const bool state_registered=registered_grab_state==g_hold.state;
    const bool state_owned=state_registered &&
        Read<void*>(g_hold.state,0x10)==player;
    if (Read<int>(player,0x2BC)!=6 || !state_owned) {
        // HookedLeave normally restores a grab before ChangeState publishes the
        // next index. Reaching this branch means that lifecycle notification
        // was missed. Clear ownership first. Restore only while the same live
        // player still registers the exact grab-state object; otherwise the
        // old state/body lifetime is unproven and must not be dereferenced.
        const Hold hold=g_hold;
        g_hold={};
        g_held.store(false,std::memory_order_release);
        PublishGameplayPalmHeldBody(
            hold.hand == runtime::VrHand::left ? 0U : 1U, nullptr);
        ++g_guarded_releases;
        if (state_owned) RestoreHeldBody(hold,{},{});
        NativeControllerHaptic(hold.hand,false);
        ++g_grabs_released;
        return;
    }
    const auto frame=ReadNativeControllerFrame();
    Matrix palm; Vec velocity{},angular{};
    const bool valid_pose=HandPose(g_hold.hand,false,palm,velocity,angular);
    if (!g_enabled.load(std::memory_order_acquire) || ui || !frame.focused ||
        !frame.input.state.interact.pressed || !valid_pose) {
        g_hold.discard_momentum=!g_enabled.load(std::memory_order_acquire) || ui || !frame.focused || !valid_pose;
        if (g_hold.discard_momentum) ++g_guarded_releases;
        reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xA9FD0)(g_hold.state);
    }
}
}
