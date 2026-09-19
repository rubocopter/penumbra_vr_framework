// Exact-build adapter for the initialized Black Plague FD316F... image.
// Native Enter/Leave retain ownership of HPL's mass/gravity/script transitions.
#include "spatial_interaction.hpp"
#include "hand_contact_probe.hpp"
#include "native_input_bridge.hpp"
#include "render_world_probe.hpp"
#include "vr_grab_pose.hpp"
#include "vr_interaction_policy.hpp"
#include "vr_magnetic_pickup_policy.hpp"
#include "vr_mechanism_policy.hpp"
#include "vr_rework_hand_profile.hpp"
#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <algorithm>
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
using CheckShapeWorldCollision = bool(__thiscall*)(
    void*, Vec*, void*, const Matrix*, void*, bool, bool, void*, bool, bool);
using CreateSphereShape = void*(__thiscall*)(void*, const Vec*, Matrix*);
using DestroyShape = void(__thiscall*)(void*, void*);

struct NudgeCollidePoint {
    Vec point{};
    Vec normal{};
    float depth = 0.0F;
};
struct LegacyNudgeCollideData {
    std::uint32_t allocator_or_proxy = 0;
    const NudgeCollidePoint* first = nullptr;
    const NudgeCollidePoint* last = nullptr;
    const NudgeCollidePoint* capacity_end = nullptr;
    std::int32_t point_count = 0;
};
static_assert(sizeof(NudgeCollidePoint) == 0x1C);
static_assert(sizeof(void*) == 4,
    "The Black Plague exact-build adapter requires an x86 target");
static_assert(sizeof(LegacyNudgeCollideData) == 0x14);

constexpr std::uintptr_t kPhysicsWorldVtable = 0x291B80;
constexpr std::uintptr_t kPhysicsBodyVtable = 0x292C08;
constexpr std::uintptr_t kCollideShapeNewtonVtable = 0x292D40;
constexpr std::uintptr_t kCharacterPhysicsBodyOffset = 0x23C;
constexpr std::uintptr_t kCharacterPhysicsWorldOffset = 0x240;
constexpr std::uintptr_t kCreateSphereShape = 0x18ACB0;
constexpr std::uintptr_t kCreateSphereShapeVtableSlot = 0x34;
constexpr std::uintptr_t kDestroyShape = 0xD4210;
constexpr std::uintptr_t kCheckShapeWorldCollision = 0xD4830;
constexpr std::uintptr_t kGetLinearVelocity = 0x19C6D0;
constexpr std::uintptr_t kGetAngularVelocity = 0x19C720;
constexpr std::uintptr_t kAddImpulseAtPosition = 0x19C3D0;
constexpr std::uintptr_t kGetBodyJoint = 0xCD240;
constexpr std::uintptr_t kPhysicsJointHingeNewtonVtable = 0x292EC8;
constexpr std::uintptr_t kPhysicsJointSliderNewtonVtable = 0x292FB8;
constexpr std::uintptr_t kHingeGetType = 0x156750;
constexpr std::uintptr_t kSliderGetType = 0x1499B0;
constexpr std::uintptr_t kJointTypeVtableSlot = 0x14;
constexpr std::uintptr_t kJointPinDirectionOffset = 0xB8;
constexpr std::uintptr_t kJointPivotPointOffset = 0xC4;
constexpr std::uintptr_t kBodyBoundingVolumeOffset = 0xB4;
constexpr std::uintptr_t kBodyUserDataOffset = 0x414;
constexpr std::uintptr_t kBodyCollideOffset = 0x418;
constexpr std::uintptr_t kBodyActiveOffset = 0x31;
constexpr std::uintptr_t kBodyCharacterOffset = 0x3C7;
constexpr std::uintptr_t kBodyPlayerOffset = 0x3C9;
constexpr std::uintptr_t kWorldBodyListOffset = 0x14;
constexpr std::uintptr_t kBodyListPayloadOffset = 0x08;
constexpr std::uintptr_t kEntityActiveOffset = 0x14;
constexpr std::uintptr_t kEntityTypeOffset = 0xC0;
constexpr std::uintptr_t kItemSubtypeOffset = 0x250;
constexpr int kObjectEntityType = 1;
constexpr int kItemEntityType = 5;
constexpr int kSwingDoorEntityType = 8;
constexpr int kLeverEntityType = 0x12;
constexpr int kHingeJointType = 1;
constexpr int kSliderJointType = 2;
constexpr std::uintptr_t kBoundingVolumeGetMax = 0xD8A10;
constexpr std::uintptr_t kBoundingVolumeGetMin = 0xD8A40;
constexpr std::uintptr_t kBoundingVolumeGetWorldCenter = 0xD8A70;
constexpr std::uintptr_t kBoundingVolumeGetRadius = 0xD8AF0;
constexpr std::size_t kMaximumBodyListNodes = 65536;
constexpr float kNudgeRadius = 0.12F;
constexpr float kNudgeMinimumHandSpeed = 0.03F;
constexpr float kFlashlightGripRadius = 0.020F;
// The primary cylinder position stream in Black Plague's installed glowstick
// is byte-for-byte numerically identical to the Rework source asset. Reuse the
// proven model-side VR grip profile for that geometry; the surrounding DAE
// still differs, so this is deliberately glowstick-specific.
constexpr float kGlowstickGripRadius = 0.0125F;
constexpr float kGlowstickScale = 1.55F;
constexpr Vec kGlowstickGripPoint{0.0F,0.0078F,-0.078F};
constexpr float kGlowstickRotationX = 4.71F;
constexpr std::uint64_t kToolGripMaximumAgeMilliseconds = 100;
constexpr std::uint64_t kInteractionTargetMaximumAgeMilliseconds = 100;
constexpr std::array<float,9> kToolModelToHandRotation{
    1,0,0,
    0,0,-1,
    0,1,0};
constexpr runtime::VrAttachmentSocketProfile kFlashlightSocket{
    kToolModelToHandRotation,{0,-0.016669F,0}};
std::uint8_t* g_image = nullptr;
std::array<hooks::IatHook,8> g_hooks;
hooks::Rel32CallHook g_tool_hook;
thread_local void* g_updating_hands = nullptr;
std::atomic<std::uint64_t> g_tools_attached{0},g_tools_native{0},g_invalid_tool_pose{0},g_blocked_grabs{0};
std::atomic<std::uint64_t> g_grabs_acquired{0},g_grabs_released{0},g_guarded_releases{0},g_collision_restore_failures{0};
std::atomic<std::uint64_t> g_moves_acquired{0},g_moves_released{0};
std::atomic<std::uint64_t> g_contact_rays{0};
std::atomic<std::uint64_t> g_nudge_queries{0},g_nudge_contacts{0},g_nudges_applied{0};
std::atomic<std::uint64_t> g_interact_presses{0},g_selection_refreshes{0};
std::atomic<std::uint64_t> g_selection_ray_batches{0},g_selection_rays{0};
std::atomic<std::uint64_t> g_selection_candidates{0},g_selection_discards{0};
std::atomic<std::uint64_t> g_selection_winner_distance_millimetres{0};
std::atomic<std::uint64_t> g_selection_central_ray{0},g_selection_auxiliary_ray{0};
std::atomic<std::uint64_t> g_selection_palm_queries{0},g_selection_palm_candidates{0};
std::atomic<std::uint64_t> g_selection_palm_winners{0},g_selection_palm_assisted_winners{0};
std::atomic<std::uint64_t> g_grab_enters{0},g_move_enters{0};
std::atomic<std::uint64_t> g_grab_pending{0},g_move_pending{0};
std::atomic<std::uint64_t> g_magnetic_queries{0},g_magnetic_candidates{0};
std::atomic<std::uint64_t> g_magnetic_visibility_rays{0},g_magnetic_winners{0};
std::atomic<std::uint64_t> g_mechanism_acquired{0},g_mechanism_updates{0};
std::atomic<std::uint64_t> g_mechanism_rejected{0};
std::array<std::atomic<float>,2> g_tool_grip_weight{};
std::array<std::atomic<std::uint64_t>,2> g_tool_grip_time{};
struct InteractionTarget {
    std::array<float,3> point{};
    void* body = nullptr;
    std::uint64_t time = 0;
    bool valid = false;
};
SRWLOCK g_interaction_target_lock = SRWLOCK_INIT;
std::array<InteractionTarget,2> g_interaction_targets{};
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
thread_local bool g_vr_selection_refresh_active = false;
// HPL's per-body CollideCharacter flag is consumed by world collision queries,
// character rays and Newton's character/body contact filtering. Never move a
// tracked body until the exact-build field and its consumers have been proved.
std::atomic<bool> g_player_collision_filter_ready{false};
void* g_pending_state = nullptr; // Input thread only; never dereferenced without current-state identity.
void* g_pending_move_state = nullptr;
runtime::VrHand g_pending_hand = runtime::VrHand::right;
runtime::VrHand g_pending_move_hand = runtime::VrHand::right;
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
    enum class Mode : std::uint8_t { free_body, hinge, slider };
    void* state = nullptr;
    void* player = nullptr;
    void* body = nullptr;
    std::uint64_t player_generation = 0;
    runtime::VrHand hand = runtime::VrHand::right;
    Vec local_body_contact{};
    Vec local_hand_contact{};
    Vec previous_palm{};
    float max_linear = 0, max_angular = 0;
    Mode mode = Mode::free_body;
    Vec joint_pin{};
    Vec joint_pivot{};
    float hinge_lightness = 1.0F;
} g_move_hold;

struct NudgePlan {
    Vec velocity{};
    float push_fraction = 0.0F;
    float maximum_push_speed = 0.0F;
    float maximum_delta_velocity = 0.0F;
};

struct OwnedNudgeShape {
    std::uint8_t* image = nullptr;
    void* world = nullptr;
    void* shape = nullptr;
} g_nudge_shape;

struct NudgeHit {
    void* body = nullptr;
    Vec contact_sum{};
    std::uint32_t contact_count = 0;
};

bool Copy(const void* source, void* dest, std::size_t size) noexcept;
[[nodiscard]] float Dot(const Vec& left,const Vec& right) noexcept;
[[nodiscard]] Vec Cross(const Vec& left,const Vec& right) noexcept;

class NudgeContactCallback final {
public:
    virtual void OnCollision(void* body, void* collide_data) {
        LegacyNudgeCollideData data{};
        if (!body || !Copy(collide_data,&data,sizeof(data)) ||
            data.point_count < 0 || data.point_count > 32 ||
            (data.point_count != 0 && (!data.first || !data.last))) {
            valid = false;
            return;
        }
        if (!data.point_count) return;
        const auto first = reinterpret_cast<std::uintptr_t>(data.first);
        const auto last = reinterpret_cast<std::uintptr_t>(data.last);
        const auto required = static_cast<std::uintptr_t>(data.point_count) *
            sizeof(NudgeCollidePoint);
        if (last < first || last-first < required) {
            valid = false;
            return;
        }
        NudgeHit* hit = nullptr;
        for (std::size_t i=0;i<hit_count;++i) {
            if (hits[i].body==body) { hit=&hits[i]; break; }
        }
        if (!hit) {
            if (hit_count>=hits.size()) return;
            hit=&hits[hit_count++];
            hit->body=body;
        }
        for (std::int32_t i=0;i<data.point_count;++i) {
            NudgeCollidePoint point{};
            if (!Copy(reinterpret_cast<const std::uint8_t*>(data.first)+
                    static_cast<std::size_t>(i)*sizeof(point),&point,sizeof(point)) ||
                !std::isfinite(point.point[0]) || !std::isfinite(point.point[1]) ||
                !std::isfinite(point.point[2]) || !std::isfinite(point.depth) ||
                point.depth<0) {
                valid=false;
                return;
            }
            for (std::size_t axis=0;axis<3;++axis)
                hit->contact_sum[axis]+=point.point[axis];
            ++hit->contact_count;
        }
    }

    bool valid=true;
    std::array<NudgeHit,32> hits{};
    std::size_t hit_count=0;
};

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

struct InteractionRaySegment {
    Vec from{};
    Vec to{};
};

// HPL returns ray hits incrementally through the callback. The old VR ray
// path improved the ray geometry but still allowed HPL's first-hit ordering to
// decide the winner. Keep the native callback contract and collect the
// candidates synchronously so the VR interaction layer can choose the same way
// for every generated segment.
struct RankedRayCallback final {
    struct VTable {
        bool (__thiscall *before)(void*, void*);
        bool (__thiscall *intersect)(void*, void*, void*);
    };

    VTable* vtable = nullptr;
    void* original = nullptr;
    void* best_body = nullptr;
    float best_distance = INFINITY;
    float best_score = INFINITY;
    std::size_t ray_index = 0;
    Vec best_point{};
    Vec best_normal{};

    [[nodiscard]] bool Better(float distance, std::size_t candidate_ray) const noexcept {
        // Prefer the actual interaction ray when two candidates are almost
        // identical. Auxiliary rays only widen the palm selection cone.
        const float score = distance + (candidate_ray == 0 ? 0.0F : 0.002F);
        return score < best_score;
    }

    static bool __fastcall Before(void* self, void*, void* body) {
        auto* proxy = static_cast<RankedRayCallback*>(self);
        auto** original_vtable = *reinterpret_cast<void***>(proxy->original);
        using BeforeFn = bool(__thiscall*)(void*, void*);
        return reinterpret_cast<BeforeFn>(original_vtable[0])(proxy->original, body);
    }

    static bool __fastcall Intersect(void* self, void*, void* body, void* params) {
        auto* proxy = static_cast<RankedRayCallback*>(self);
        struct Params { float t; float dist; Vec normal; Vec point; } hit{};
        if (!Copy(params, &hit, sizeof(hit)) || !std::isfinite(hit.dist)) return true;
        ++g_selection_candidates;
        if (proxy->Better(hit.dist, proxy->ray_index)) {
            if (proxy->best_body) ++g_selection_discards;
            proxy->best_distance = hit.dist;
            proxy->best_score = hit.dist + (proxy->ray_index == 0 ? 0.0F : 0.002F);
            proxy->best_body = body;
            proxy->best_point = hit.point;
            proxy->best_normal = hit.normal;
        } else ++g_selection_discards;
        // HPL/Newton interprets false as "stop this ray now". The native
        // gameplay callback returns true so every body along the segment can
        // participate; preserve that contract while ranking VR candidates.
        return true;
    }
};

[[nodiscard]] std::array<InteractionRaySegment,5> InteractionRaySegments(
    const Matrix& pose,
    float native_length) noexcept {
    using namespace runtime::vr_interaction_policy;
    const float forward = ClampPhysicalInteractionReach(native_length);
    const float rear = kCollisionSizeZ * 0.5F;
    const float offset_x = kCollisionSizeX * 0.25F;
    const float offset_y = kCollisionSizeY * 0.35F;
    constexpr std::array<std::array<float,2>,5> normalized_offsets{{
        {0.0F,0.0F},{1.0F,0.0F},{-1.0F,0.0F},{0.0F,1.0F},{0.0F,-1.0F}}};
    std::array<InteractionRaySegment,5> result{};
    for (std::size_t index=0;index<result.size();++index) {
        const Vec lateral{
            normalized_offsets[index][0]*offset_x,
            normalized_offsets[index][1]*offset_y,
            0.0F};
        Vec local_from=lateral;
        Vec local_to=lateral;
        local_from[2]=rear;
        local_to[2]=-forward;
        result[index].from=TransformPoint(pose,local_from);
        result[index].to=TransformPoint(pose,local_to);
    }
    return result;
}

[[nodiscard]] Vec InverseTransformPoint(const Matrix& matrix, const Vec& point) noexcept {
    const Vec delta{
        point[0]-matrix.values[3], point[1]-matrix.values[7], point[2]-matrix.values[11]};
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
[[nodiscard]] bool FiniteVec(const Vec& value) noexcept {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}
bool BodyMatches(void* body) { return body && Read<void*>(body,0) == g_image + kPhysicsBodyVtable; }
[[nodiscard]] float ReworkHingeLightness(float mass) noexcept {
    if (!std::isfinite(mass) || mass<=0.0F) return 0.0F;
    if (mass>10.0F) return 2.25F;
    if (mass>=5.0F) return 1.75F;
    return 1.35F;
}
[[nodiscard]] bool BindRecognizedMechanism(void* body, MoveHold& hold) noexcept {
    if (!BodyMatches(body) ||
        reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(body)!=1)
        return false;
    void* const entity=Read<void*>(body,kBodyUserDataOffset);
    if (!entity || !Read<bool>(entity,kEntityActiveOffset)) return false;
    const int entity_type=Read<int>(entity,kEntityTypeOffset);
    const bool is_lever=entity_type==kLeverEntityType;
    const bool is_swing_door=entity_type==kSwingDoorEntityType;
    if (!is_lever && !is_swing_door) return false;
    void* joint=nullptr;
    __try {
        joint=reinterpret_cast<void*(__thiscall*)(void*,int)>(g_image+kGetBodyJoint)(body,0);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!joint) return false;
    void* const vtable=Read<void*>(joint,0);
    int expected_type=0;
    std::uintptr_t expected_get_type=0;
    if (vtable==g_image+kPhysicsJointHingeNewtonVtable) {
        expected_type=kHingeJointType;
        expected_get_type=kHingeGetType;
        hold.mode=MoveHold::Mode::hinge;
    } else if (vtable==g_image+kPhysicsJointSliderNewtonVtable) {
        // Rework's SwingDoor family is authored and handled as hinges. Keep
        // Black Plague doors fail-closed if target evidence ever disagrees.
        if (is_swing_door) return false;
        expected_type=kSliderJointType;
        expected_get_type=kSliderGetType;
        hold.mode=MoveHold::Mode::slider;
    } else {
        return false;
    }
    if (Read<void*>(vtable,kJointTypeVtableSlot)!=g_image+expected_get_type) return false;
    int type=0;
    __try {
        type=reinterpret_cast<int(__thiscall*)(void*)>(g_image+expected_get_type)(joint);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (type!=expected_type) return false;
    hold.joint_pin=Read<Vec>(joint,kJointPinDirectionOffset);
    hold.joint_pivot=Read<Vec>(joint,kJointPivotPointOffset);
    if (!FiniteVec(hold.joint_pin) || !FiniteVec(hold.joint_pivot) ||
        runtime::vr_mechanism_policy::Length(hold.joint_pin)<=1.0e-6F)
        return false;
    return true;
}
[[nodiscard]] bool BuildNudgePlan(void* body, const Vec& hand_velocity,
    const Vec& contact, float mass, float radius, int joint_count,
    NudgePlan& plan) noexcept {
    plan={};
    if (!BodyMatches(body) || !FiniteVec(hand_velocity) || !FiniteVec(contact) ||
        !std::isfinite(mass) || mass<=0.0F || !std::isfinite(radius) || radius<0.0F ||
        joint_count<0 || !Read<bool>(body,kBodyActiveOffset) ||
        !Read<bool>(body,kBodyCollideOffset) || Read<bool>(body,kBodyCharacterOffset) ||
        Read<bool>(body,kBodyPlayerOffset)) return false;
    void* const entity=Read<void*>(body,kBodyUserDataOffset);
    if (!entity || !Read<bool>(entity,kEntityActiveOffset)) return false;
    const int entity_type=Read<int>(entity,kEntityTypeOffset);
    if (entity_type!=kObjectEntityType && entity_type!=kItemEntityType &&
        entity_type!=kSwingDoorEntityType) return false;

    plan.velocity=hand_velocity;
    if (joint_count>0) {
        // The exact Black Plague evidence currently proves only the same
        // one-joint hinge/slider families already consumed by Move. Unknown
        // jointed Objects fail closed instead of receiving an unconstrained
        // palm impulse.
        MoveHold mechanism{};
        if (!BindRecognizedMechanism(body,mechanism)) return false;
        const float pin_length=runtime::vr_mechanism_policy::Length(mechanism.joint_pin);
        if (!std::isfinite(pin_length) || pin_length<=1.0e-6F) return false;
        const Vec pin{
            mechanism.joint_pin[0]/pin_length,
            mechanism.joint_pin[1]/pin_length,
            mechanism.joint_pin[2]/pin_length};
        if (mechanism.mode==MoveHold::Mode::slider) {
            const float along=Dot(hand_velocity,pin);
            plan.velocity={pin[0]*along,pin[1]*along,pin[2]*along};
        } else if (mechanism.mode==MoveHold::Mode::hinge) {
            Vec radial{
                contact[0]-mechanism.joint_pivot[0],
                contact[1]-mechanism.joint_pivot[1],
                contact[2]-mechanism.joint_pivot[2]};
            const float along_pin=Dot(radial,pin);
            for (std::size_t axis=0;axis<3;++axis) radial[axis]-=pin[axis]*along_pin;
            const float radial_length=runtime::vr_mechanism_policy::Length(radial);
            if (!std::isfinite(radial_length) || radial_length<=0.02F) return false;
            const Vec tangent_raw=Cross(pin,radial);
            const float tangent_length=runtime::vr_mechanism_policy::Length(tangent_raw);
            if (!std::isfinite(tangent_length) || tangent_length<=1.0e-6F) return false;
            const Vec tangent{
                tangent_raw[0]/tangent_length,
                tangent_raw[1]/tangent_length,
                tangent_raw[2]/tangent_length};
            const float tangent_speed=Dot(hand_velocity,tangent);
            plan.velocity={
                tangent[0]*tangent_speed,
                tangent[1]*tangent_speed,
                tangent[2]*tangent_speed};
        } else return false;

        // Nudge is only a proximity fallback. Keep SwingDoor contact at the
        // conservative locked-door cap; the native Move transition remains the
        // authoritative path for actually opening an unlocked door.
        plan.push_fraction=0.30F;
        plan.maximum_push_speed=0.30F;
        plan.maximum_delta_velocity=0.10F;
        return runtime::vr_mechanism_policy::Length(plan.velocity)>=0.02F;
    }

    if (entity_type==kSwingDoorEntityType) return false;
    const bool small_light=mass<=3.0F && radius<=0.30F;
    const bool large_heavy=mass>=12.0F || radius>=0.75F;
    if (small_light) {
        plan.push_fraction=0.60F;
        plan.maximum_push_speed=0.90F;
        plan.maximum_delta_velocity=0.28F;
    } else if (large_heavy) {
        plan.push_fraction=0.22F;
        plan.maximum_push_speed=0.38F;
        plan.maximum_delta_velocity=0.10F;
    } else if (entity_type==kObjectEntityType) {
        // BP does not yet have a verifier-pinned Object interact-mode/breakable
        // field. Treat ordinary Objects as Rework's gentle grab/move class;
        // this is conservative for breakables and avoids proximity punches.
        plan.push_fraction=0.34F;
        plan.maximum_push_speed=0.65F;
        plan.maximum_delta_velocity=0.22F;
    } else {
        plan.push_fraction=0.50F;
        plan.maximum_push_speed=1.00F;
        plan.maximum_delta_velocity=0.40F;
    }
    return true;
}
[[nodiscard]] bool SafeNativeRayBefore(void* callback,void* body) noexcept {
    if (!callback || !body) return false;
    __try {
        auto** vtable=*reinterpret_cast<void***>(callback);
        using BeforeFn = bool(__thiscall*)(void*,void*);
        return vtable && reinterpret_cast<BeforeFn>(vtable[0])(callback,body);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
[[nodiscard]] float Dot(const Vec& left,const Vec& right) noexcept {
    return left[0]*right[0]+left[1]*right[1]+left[2]*right[2];
}
[[nodiscard]] Vec Cross(const Vec& left,const Vec& right) noexcept {
    return {left[1]*right[2]-left[2]*right[1],
        left[2]*right[0]-left[0]*right[2],
        left[0]*right[1]-left[1]*right[0]};
}
[[nodiscard]] bool ValidPhysicsWorld(void* world) noexcept {
    return g_image && world && Read<void*>(world,0)==g_image+kPhysicsWorldVtable;
}
[[nodiscard]] bool ValidNudgeWorld(void* world) noexcept {
    if (!ValidPhysicsWorld(world)) return false;
    auto* const vtable=Read<void**>(world,0);
    return vtable && Read<void*>(vtable,kCreateSphereShapeVtableSlot)==g_image+kCreateSphereShape;
}
void DestroyNudgeShape() noexcept {
    const auto owned=g_nudge_shape;
    g_nudge_shape={};
    if (!owned.shape || !owned.image || !owned.world || !ValidNudgeWorld(owned.world)) return;
    __try {
        reinterpret_cast<DestroyShape>(owned.image+kDestroyShape)(owned.world,owned.shape);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
[[nodiscard]] bool EnsureNudgeShape(void* world) noexcept {
    if (!ValidNudgeWorld(world)) return false;
    if (g_nudge_shape.shape && g_nudge_shape.image==g_image && g_nudge_shape.world==world) return true;
    if (g_nudge_shape.shape) DestroyNudgeShape();
    const Vec radius{kNudgeRadius,kNudgeRadius,kNudgeRadius};
    void* shape=nullptr;
    __try {
        shape=reinterpret_cast<CreateSphereShape>(g_image+kCreateSphereShape)(world,&radius,nullptr);
    } __except(EXCEPTION_EXECUTE_HANDLER) { shape=nullptr; }
    if (!shape || Read<void*>(shape,0)!=g_image+kCollideShapeNewtonVtable ||
        Read<int>(shape,0x10)!=2 || Read<int>(shape,0x54)!=0 ||
        Read<void*>(shape,0x58)!=world) {
        if (shape) {
            g_nudge_shape={g_image,world,shape};
            DestroyNudgeShape();
        }
        return false;
    }
    g_nudge_shape={g_image,world,shape};
    return true;
}
[[nodiscard]] bool BodyVelocity(void* body,std::uintptr_t target,Vec& value) noexcept {
    value={};
    __try {
        value=reinterpret_cast<Vec(__thiscall*)(void*)>(g_image+target)(body);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return FiniteVec(value);
}
[[nodiscard]] bool SafeNudgeQuery(void* world,Vec* corrected,void* shape,
    const Matrix* transform,void* skip_body,NudgeContactCallback* callback,
    bool& collided) noexcept {
    collided=false;
    __try {
        collided=reinterpret_cast<CheckShapeWorldCollision>(
            g_image+kCheckShapeWorldCollision)(world,corrected,shape,transform,
                skip_body,true,false,callback,false,false);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
[[nodiscard]] bool SafeAddImpulseAtPosition(void* body,const Vec* impulse,
    const Vec* position) noexcept {
    __try {
        reinterpret_cast<void(__thiscall*)(void*,const Vec*,const Vec*)>(
            g_image+kAddImpulseAtPosition)(body,impulse,position);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
[[nodiscard]] bool ComputeNudgeImpulse(const Vec& hand_center,const Vec& hand_velocity,
    const Vec& push_velocity,const Vec& contact,const Matrix& body_matrix,
    const Vec& linear_velocity,const Vec& angular_velocity,float mass,
    const NudgePlan& plan,Vec& impulse,
    float* applied_delta=nullptr) noexcept {
    impulse={};
    if (applied_delta) *applied_delta=0.0F;
    if (!FiniteVec(hand_center) || !FiniteVec(hand_velocity) || !FiniteVec(push_velocity) ||
        !FiniteVec(contact) ||
        !FiniteVec(linear_velocity) || !FiniteVec(angular_velocity) ||
        !std::isfinite(mass) || mass<=0 || !std::isfinite(plan.push_fraction) ||
        !std::isfinite(plan.maximum_push_speed) ||
        !std::isfinite(plan.maximum_delta_velocity) || plan.push_fraction<=0.0F ||
        plan.maximum_push_speed<=0.0F || plan.maximum_delta_velocity<=0.0F) return false;
    const float speed=std::hypot(std::hypot(push_velocity[0],push_velocity[1]),
        push_velocity[2]);
    if (!std::isfinite(speed) || speed<kNudgeMinimumHandSpeed) return false;
    const Vec to_object{contact[0]-hand_center[0],contact[1]-hand_center[1],
        contact[2]-hand_center[2]};
    if (Dot(hand_velocity,to_object)<=0) return false;
    const Vec push_direction{push_velocity[0]/speed,push_velocity[1]/speed,
        push_velocity[2]/speed};
    const float desired_speed=std::clamp(speed*plan.push_fraction,0.0F,
        plan.maximum_push_speed);
    const Vec body_position{body_matrix.values[3],body_matrix.values[7],
        body_matrix.values[11]};
    if (!FiniteVec(body_position)) return false;
    const Vec radius{contact[0]-body_position[0],contact[1]-body_position[1],
        contact[2]-body_position[2]};
    const Vec spin=Cross(angular_velocity,radius);
    const Vec contact_velocity{linear_velocity[0]+spin[0],linear_velocity[1]+spin[1],
        linear_velocity[2]+spin[2]};
    const float current_speed=Dot(contact_velocity,push_direction);
    if (!std::isfinite(current_speed)) return false;
    const float delta=std::clamp(desired_speed-current_speed,0.0F,
        plan.maximum_delta_velocity);
    if (delta<=0.005F) return false;
    impulse={push_direction[0]*delta*mass,push_direction[1]*delta*mass,
        push_direction[2]*delta*mass};
    if (applied_delta) *applied_delta=delta;
    return FiniteVec(impulse);
}
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

struct MagneticRayHit {
    float t = 0.0F;
    float distance = INFINITY;
    Vec normal{};
    Vec point{};
};

struct MagneticCandidate {
    void* body = nullptr;
    Vec visible_sample{};
    Vec centre{};
    float score = INFINITY;
};

[[nodiscard]] runtime::vr_magnetic_pickup_policy::VrMagneticPickupClass
MagneticClassForItemSubtype(int subtype) noexcept {
    using Class = runtime::vr_magnetic_pickup_policy::VrMagneticPickupClass;
    switch (subtype) {
    case 3:  // battery
    case 5:  // food
    case 7:  // glowstick
    case 8:  // flare
    case 9:  // painkillers
        return Class::consumable;
    case 0:  // normal
    case 2:  // note
    case 6:  // map
        return Class::ordinary;
    case 1:  // notebook
    case 4:  // flashlight
    case 10: // weaponmelee
    case 11: // throw
        return Class::equipment;
    default:
        // Black Plague gasmask=12 and collectable=13 have no demonstrated
        // Rework magnetic-pickup equivalent and remain deliberately excluded.
        return Class::unsupported;
    }
}

[[nodiscard]] bool ReadBodyBoundingVolume(void* body, Vec& centre, Vec& minimum,
    Vec& maximum, float& radius) noexcept {
    centre={}; minimum={}; maximum={}; radius=0.0F;
    if (!BodyMatches(body)) return false;
    auto* const bv=static_cast<std::uint8_t*>(body)+kBodyBoundingVolumeOffset;
    using VectorGetter = Vec*(__thiscall*)(void*,Vec*);
    __try {
        reinterpret_cast<VectorGetter>(g_image+kBoundingVolumeGetWorldCenter)(bv,&centre);
        reinterpret_cast<VectorGetter>(g_image+kBoundingVolumeGetMin)(bv,&minimum);
        reinterpret_cast<VectorGetter>(g_image+kBoundingVolumeGetMax)(bv,&maximum);
        radius=reinterpret_cast<float(__thiscall*)(void*)>(
            g_image+kBoundingVolumeGetRadius)(bv);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!FiniteVec(centre) || !FiniteVec(minimum) || !FiniteVec(maximum) ||
        !std::isfinite(radius) || radius<0.0F) return false;
    for (std::size_t axis=0;axis<3;++axis)
        if (minimum[axis]>maximum[axis]) return false;
    return true;
}

[[nodiscard]] bool MagneticItemProfile(void* body,
    runtime::vr_magnetic_pickup_policy::VrMagneticPickupProfile& profile) noexcept {
    profile={};
    if (!BodyMatches(body) || !Read<bool>(body,kBodyActiveOffset) ||
        !Read<bool>(body,kBodyCollideOffset) || Read<bool>(body,kBodyCharacterOffset) ||
        Read<bool>(body,kBodyPlayerOffset)) return false;
    void* const entity=Read<void*>(body,kBodyUserDataOffset);
    if (!entity || !Read<bool>(entity,kEntityActiveOffset) ||
        Read<int>(entity,kEntityTypeOffset)!=kItemEntityType) return false;
    profile=runtime::vr_magnetic_pickup_policy::Profile(
        MagneticClassForItemSubtype(Read<int>(entity,kItemSubtypeOffset)));
    return profile.eligible;
}

void InsertMagneticCandidate(
    std::array<MagneticCandidate,
        runtime::vr_magnetic_pickup_policy::kRankedCandidateCount>& candidates,
    std::size_t& count, const MagneticCandidate& candidate) noexcept {
    std::size_t insert=count;
    if (insert>candidates.size()) insert=candidates.size();
    while (insert>0 && candidates[insert-1].score>candidate.score) --insert;
    if (insert>=candidates.size()) return;
    const std::size_t last=count<candidates.size() ? count : candidates.size()-1;
    for (std::size_t index=last;index>insert;--index)
        candidates[index]=candidates[index-1];
    candidates[insert]=candidate;
    if (count<candidates.size()) ++count;
}

struct MagneticSightCallback final {
    struct VTable {
        bool (__thiscall *before)(void*,void*);
        bool (__thiscall *intersect)(void*,void*,void*);
    };
    VTable* vtable=nullptr;
    void* candidate=nullptr;
    void* nearest_body=nullptr;
    MagneticRayHit nearest{};

    static bool __fastcall Before(void* self,void*,void* body) {
        auto* callback=static_cast<MagneticSightCallback*>(self);
        if (!BodyMatches(body) || !Read<bool>(body,kBodyActiveOffset) ||
            Read<bool>(body,kBodyCharacterOffset) || Read<bool>(body,kBodyPlayerOffset))
            return false;
        return body==callback->candidate || Read<bool>(body,kBodyCollideOffset);
    }
    static bool __fastcall Intersect(void* self,void*,void* body,void* params) {
        auto* callback=static_cast<MagneticSightCallback*>(self);
        struct Params { float t; float dist; Vec normal; Vec point; } hit{};
        if (!Before(self,nullptr,body) || !Copy(params,&hit,sizeof(hit)) ||
            !std::isfinite(hit.dist) || hit.dist<0.0F ||
            hit.dist>=callback->nearest.distance) return true;
        callback->nearest_body=body;
        callback->nearest={hit.t,hit.dist,hit.normal,hit.point};
        return true;
    }
};

[[nodiscard]] bool CastSolidSight(void* world, const Vec& origin, void* candidate,
    const Vec& sample, MagneticRayHit& hit) noexcept {
    hit={};
    const Vec ray{sample[0]-origin[0],sample[1]-origin[1],sample[2]-origin[2]};
    const float length=std::hypot(std::hypot(ray[0],ray[1]),ray[2]);
    if (!ValidPhysicsWorld(world) || !BodyMatches(candidate) ||
        !FiniteVec(origin) || !FiniteVec(sample) || !std::isfinite(length) || length<=0.001F)
        return false;
    const float scale=runtime::vr_magnetic_pickup_policy::kSightOvershoot/length;
    const Vec end{sample[0]+ray[0]*scale,sample[1]+ray[1]*scale,sample[2]+ray[2]*scale};
    MagneticSightCallback::VTable table{
        reinterpret_cast<bool(__thiscall*)(void*,void*)>(MagneticSightCallback::Before),
        reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(MagneticSightCallback::Intersect)};
    MagneticSightCallback callback{&table,candidate};
    __try {
        reinterpret_cast<Ray>(g_image+0x189E30)(
            world,&callback,&origin,&end,true,false,true,true);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (callback.nearest_body!=candidate || !FiniteVec(callback.nearest.point)) return false;
    hit=callback.nearest;
    return true;
}

[[nodiscard]] bool CastMagneticSight(void* world, const Vec& origin, void* candidate,
    const Vec& sample, MagneticRayHit& hit) noexcept {
    ++g_magnetic_visibility_rays;
    return CastSolidSight(world,origin,candidate,sample,hit);
}

[[nodiscard]] bool FindMagneticTarget(void* world, runtime::VrHand hand,
    void*& body, MagneticRayHit& hand_hit) noexcept {
    body=nullptr; hand_hit={};
    if (!ValidPhysicsWorld(world)) return false;
    Matrix aim_pose{}; Vec velocity{},angular{};
    if (!RawHandPose(hand,true,aim_pose,velocity,angular)) return false;
    Matrix head_pose{};
    if (!TrackedHeadWorldPose(head_pose)) return false;
    const Vec origin{aim_pose.values[3],aim_pose.values[7],aim_pose.values[11]};
    Vec direction{-aim_pose.values[2],-aim_pose.values[6],-aim_pose.values[10]};
    const float direction_length=std::hypot(
        std::hypot(direction[0],direction[1]),direction[2]);
    if (!FiniteVec(origin) || !std::isfinite(direction_length) || direction_length<=0.001F)
        return false;
    for (float& axis:direction) axis/=direction_length;
    const Vec head{head_pose.values[3],head_pose.values[7],head_pose.values[11]};
    if (!FiniteVec(head)) return false;

    ++g_magnetic_queries;
    std::array<MagneticCandidate,
        runtime::vr_magnetic_pickup_policy::kRankedCandidateCount> candidates{};
    std::size_t candidate_count=0;
    void* const list=Read<void*>(world,kWorldBodyListOffset);
    if (!list) return false;
    void* node=Read<void*>(list,0);
    std::size_t visited=0;
    while (node && node!=list && visited++<kMaximumBodyListNodes) {
        void* const current=node;
        node=Read<void*>(current,0);
        void* const candidate_body=Read<void*>(current,kBodyListPayloadOffset);
        if (!candidate_body ||
            (g_held.load(std::memory_order_acquire) && candidate_body==g_hold.body) ||
            (g_move_held.load(std::memory_order_acquire) && candidate_body==g_move_hold.body))
            continue;
        runtime::vr_magnetic_pickup_policy::VrMagneticPickupProfile profile{};
        if (!MagneticItemProfile(candidate_body,profile)) continue;
        Vec centre{},minimum{},maximum{}; float radius=0.0F;
        if (!ReadBodyBoundingVolume(candidate_body,centre,minimum,maximum,radius)) continue;
        const Vec to_target{centre[0]-origin[0],centre[1]-origin[1],centre[2]-origin[2]};
        const float forward=Dot(to_target,direction);
        if (!runtime::vr_magnetic_pickup_policy::ForwardDistanceEligible(
                forward,profile.range)) continue;
        const Vec perpendicular{
            to_target[0]-direction[0]*forward,
            to_target[1]-direction[1]*forward,
            to_target[2]-direction[2]*forward};
        const float perpendicular_sq=Dot(perpendicular,perpendicular);
        const float cone_radius=runtime::vr_magnetic_pickup_policy::ConeRadius(forward,radius);
        if (!runtime::vr_magnetic_pickup_policy::InsideAimCone(
                perpendicular_sq,cone_radius)) continue;
        const Vec aim_point{
            origin[0]+direction[0]*forward,
            origin[1]+direction[1]*forward,
            origin[2]+direction[2]*forward};
        const auto sample=runtime::vr_magnetic_pickup_policy::VisibleSample(
            aim_point,minimum,maximum,centre);
        const float score=runtime::vr_magnetic_pickup_policy::CandidateScore(
            perpendicular_sq,cone_radius,forward,profile.priority_bias);
        InsertMagneticCandidate(candidates,candidate_count,
            {candidate_body,sample,centre,score});
        ++g_magnetic_candidates;
    }
    if (node!=list) return false;

    for (std::size_t index=0;index<candidate_count;++index) {
        MagneticRayHit sample_hand{},sample_head{};
        bool visible=CastMagneticSight(world,origin,candidates[index].body,
                candidates[index].visible_sample,sample_hand) &&
            CastMagneticSight(world,head,candidates[index].body,
                candidates[index].visible_sample,sample_head);
        if (!visible) {
            visible=CastMagneticSight(world,origin,candidates[index].body,
                    candidates[index].centre,sample_hand) &&
                CastMagneticSight(world,head,candidates[index].body,
                    candidates[index].centre,sample_head);
        }
        if (!visible) continue;
        body=candidates[index].body;
        hand_hit=sample_hand;
        return true;
    }
    return false;
}

bool ResolvedControllerPose(runtime::VrHand hand, Matrix& pose,
    Vec& velocity, Vec& angular) {
    if (!RawHandPose(hand,false,pose,velocity,angular)) return false;
    runtime::VrMatrix44 resolved{};
    const std::size_t hand_index = hand == runtime::VrHand::left ? 0U : 1U;
    if (ReadGameplayPalmPose(hand_index,resolved)) pose=resolved;
    return true;
}
bool HandPose(runtime::VrHand hand, bool aim, Matrix& pose, Vec& velocity, Vec& angular) {
    if (aim) return RawHandPose(hand,true,pose,velocity,angular);
    if (!ResolvedControllerPose(hand,pose,velocity,angular)) return false;
    if (!aim) {
        pose=runtime::rework_hand_profile::ApplyVisualLocalPose(pose);
    }
    return true;
}
bool ToolHandPose(runtime::VrHand hand, float grip_radius, Matrix& pose,
    Vec& velocity, Vec& angular) {
    if (!ResolvedControllerPose(hand,pose,velocity,angular)) return false;
    // Rework 23c890f attaches tools through the cylinder formed by the four
    // long fingers, not the palm/controller origin. Keep Black Plague's own
    // measured model socket below, but reuse that proven hand-side grip frame.
    pose=runtime::rework_hand_profile::ApplyAttachmentGripLocalPose(
        pose,
        hand==runtime::VrHand::left,
        runtime::vr_interaction_policy::GripOpenCentreOffset(grip_radius));
    return true;
}
Matrix ReworkGlowstickPose(const Matrix& grip_pose) noexcept {
    // Rework 23c890f composes the attachment as
    // hand/grip-socket * T(-VrGripPoint) * R(VrRotOffset) * S(VrScale).
    // The previous BP candidate rotated a measured geometry centre first and
    // omitted VrScale entirely, which put the small model through the palm.
    Matrix grip = runtime::IdentityMatrix();
    grip.values[3] = -kGlowstickGripPoint[0];
    grip.values[7] = -kGlowstickGripPoint[1];
    grip.values[11] = -kGlowstickGripPoint[2];
    const float c = std::cos(kGlowstickRotationX);
    const float s = std::sin(kGlowstickRotationX);
    Matrix rotation = runtime::IdentityMatrix();
    rotation.values[5] = c;
    rotation.values[6] = -s;
    rotation.values[9] = s;
    rotation.values[10] = c;
    Matrix scale = runtime::IdentityMatrix();
    scale.values[0] = kGlowstickScale;
    scale.values[5] = kGlowstickScale;
    scale.values[10] = kGlowstickScale;
    return runtime::Multiply(runtime::Multiply(runtime::Multiply(
        grip_pose, grip), rotation), scale);
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

        // Rework 23c890f keeps the visible/physical palm collision-resolved, but
        // lets target acquisition follow the real controller a bounded distance
        // beyond it. Preserve raw orientation and clamp only the translation from
        // the resolved palm toward the raw palm.
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

struct PalmSelectionCandidate {
    void* body = nullptr;
    Vec point{};
    float distance = INFINITY;
    bool assisted = false;
};

[[nodiscard]] bool PalmAcquisitionEntity(void* body) noexcept {
    if (!BodyMatches(body) || !Read<bool>(body,kBodyActiveOffset) ||
        !Read<bool>(body,kBodyCollideOffset) || Read<bool>(body,kBodyCharacterOffset) ||
        Read<bool>(body,kBodyPlayerOffset)) return false;
    void* const entity=Read<void*>(body,kBodyUserDataOffset);
    if (!entity || !Read<bool>(entity,kEntityActiveOffset)) return false;
    const int type=Read<int>(entity,kEntityTypeOffset);
    return type==kObjectEntityType || type==kItemEntityType ||
        type==kSwingDoorEntityType || type==kLeverEntityType;
}

[[nodiscard]] bool FindPalmOverlapTarget(void* world,void* native_callback,
    runtime::VrHand hand,PalmSelectionCandidate& selected) noexcept {
    selected={};
    if (!ValidPhysicsWorld(world) || !native_callback) return false;
    const std::size_t hand_index=hand==runtime::VrHand::left ? 0U : 1U;
    runtime::VrMatrix44 resolved{};
    if (!ReadGameplayPalmPose(hand_index,resolved)) return false;
    const Vec resolved_center{
        resolved.values[3],resolved.values[7],resolved.values[11]};
    if (!FiniteVec(resolved_center)) return false;

    Matrix raw{}; Vec velocity{},angular{};
    if (!RawHandPose(hand,false,raw,velocity,angular)) return false;
    Matrix assisted_pose=raw;
    Vec reach{
        raw.values[3]-resolved.values[3],
        raw.values[7]-resolved.values[7],
        raw.values[11]-resolved.values[11]};
    const float raw_reach=runtime::vr_mechanism_policy::Length(reach);
    if (!std::isfinite(raw_reach)) return false;
    const float reach_scale=raw_reach>
            runtime::vr_interaction_policy::kMaximumCollisionInteractionReach && raw_reach>0.0F
        ? runtime::vr_interaction_policy::kMaximumCollisionInteractionReach/raw_reach
        : 1.0F;
    assisted_pose.values[3]=resolved.values[3]+reach[0]*reach_scale;
    assisted_pose.values[7]=resolved.values[7]+reach[1]*reach_scale;
    assisted_pose.values[11]=resolved.values[11]+reach[2]*reach_scale;

    auto choose_from_overlap=[&](const Matrix& pose,bool assisted,
            PalmSelectionCandidate& winner) noexcept -> bool {
        GameplayPalmOverlapResult overlap{};
        ++g_selection_palm_queries;
        if (!QueryGameplayPalmOverlaps(hand_index,pose,overlap) || !overlap.valid)
            return false;
        PalmSelectionCandidate best_item{},best_other{};
        for (std::size_t hit_index=0;hit_index<overlap.hit_count;++hit_index) {
            const auto& hit=overlap.hits[hit_index];
            if (!hit.body || hit.contact_count==0 || !PalmAcquisitionEntity(hit.body) ||
                !SafeNativeRayBefore(native_callback,hit.body)) continue;
            Vec point{};
            for (std::size_t axis=0;axis<3;++axis)
                point[axis]=hit.contact_sum[axis]/static_cast<float>(hit.contact_count);
            if (!FiniteVec(point)) continue;
            const Vec delta{
                point[0]-resolved_center[0],
                point[1]-resolved_center[1],
                point[2]-resolved_center[2]};
            const float distance=runtime::vr_mechanism_policy::Length(delta);
            if (!std::isfinite(distance)) continue;
            if (assisted && distance>0.001F) {
                MagneticRayHit sight{};
                if (!CastSolidSight(world,resolved_center,hit.body,point,sight)) continue;
            }
            ++g_selection_palm_candidates;
            ++g_selection_candidates;
            void* const entity=Read<void*>(hit.body,kBodyUserDataOffset);
            const bool item=entity && Read<int>(entity,kEntityTypeOffset)==kItemEntityType;
            auto& slot=item ? best_item : best_other;
            if (distance<slot.distance) {
                if (slot.body) ++g_selection_discards;
                slot={hit.body,point,distance,assisted};
            } else ++g_selection_discards;
        }
        winner=best_item.body ? best_item : best_other;
        return winner.body!=nullptr;
    };

    // Collision-resolved palm contact is authoritative. Only if it has no
    // valid target do we move the same proven palm box toward the real
    // controller by the bounded 18 cm Rework reach, with solid LOS.
    if (choose_from_overlap(resolved,false,selected)) return true;
    const float bounded_reach=raw_reach*reach_scale;
    if (bounded_reach<=0.001F) return false;
    if (!choose_from_overlap(assisted_pose,true,selected)) return false;
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
            const float grip_radius=flashlight ? kFlashlightGripRadius : kGlowstickGripRadius;
            if (!ToolHandPose(tool_hand,grip_radius,palm,velocity,angular)) {
                ++g_invalid_tool_pose; break;
            }
            // Native models point along -Y. The per-game profile rotates +90
            // degrees around X so the flashlight beam points along controller
            // -Z, then aligns the measured model socket with the hand origin.
            // These sockets come from BP's installed nodes, not Rework's DAE.
            destination=flashlight
                ? runtime::ComposeAttachmentSocketPose(palm,kFlashlightSocket)
                : ReworkGlowstickPose(palm);
            selected=&destination;
            const std::size_t hand_index=tool_hand==runtime::VrHand::left ? 0U : 1U;
            g_tool_grip_weight[hand_index].store(
                runtime::vr_interaction_policy::GripPoseWeight(grip_radius),
                std::memory_order_release);
            g_tool_grip_time[hand_index].store(GetTickCount64(),std::memory_order_release);
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
        Vec from{},to{};
        if (Copy(origin,from.data(),sizeof(from)) && Copy(end,to.data(),sizeof(to))) {
            const float native_length = std::hypot(to[0]-from[0],to[1]-from[1],to[2]-from[2]);
            if (std::isfinite(native_length) && native_length > 0 && native_length <= 20) {
                g_vr_selection_ready = true;
                RankedRayCallback::VTable table{
                    reinterpret_cast<bool(__thiscall*)(void*,void*)>(RankedRayCallback::Before),
                    reinterpret_cast<bool(__thiscall*)(void*,void*,void*)>(RankedRayCallback::Intersect)};
                RankedRayCallback ranked{&table,callback};
                PalmSelectionCandidate palm_target{};
                const bool palm_winner=FindPalmOverlapTarget(
                    world,callback,frame.interact_source,palm_target);
                if (palm_winner) {
                    ranked.best_body=palm_target.body;
                    ranked.best_distance=palm_target.distance;
                    ranked.best_score=palm_target.distance;
                    ranked.best_point=palm_target.point;
                    ranked.best_normal={};
                    ++g_selection_palm_winners;
                    if (palm_target.assisted) ++g_selection_palm_assisted_winners;
                } else {
                    // Rework falls back to directional picking only when the
                    // physical palm volume has no valid nearby target.
                    Matrix pose{}; Vec velocity{},angular{};
                    if (InteractionHandPose(frame.interact_source,pose,velocity,angular)) {
                        ++g_selection_ray_batches;
                        const auto rays=InteractionRaySegments(pose,native_length);
                        // The centre ray owns the initial candidate. Auxiliary
                        // rays widen the selection only during explicit refresh.
                        const std::size_t ray_count=
                            g_vr_selection_refresh_active ? rays.size() : 1U;
                        for (std::size_t ray_index=0;ray_index<ray_count;++ray_index) {
                            ranked.ray_index = ray_index;
                            ++g_contact_rays;
                            ++g_selection_rays;
                            if (ray_index==0) ++g_selection_central_ray;
                            else ++g_selection_auxiliary_ray;
                            reinterpret_cast<Ray>(g_image+0x189E30)(world,&ranked,
                                &rays[ray_index].from,&rays[ray_index].to,
                                distance,normal,point,prefilter);
                        }
                    }
                }
                bool magnetic_winner=false;
                MagneticRayHit magnetic_hit{};
                if (!ranked.best_body) {
                    void* magnetic_body=nullptr;
                    if (FindMagneticTarget(world,frame.interact_source,
                            magnetic_body,magnetic_hit)) {
                        if (SafeNativeRayBefore(callback,magnetic_body)) {
                            ranked.best_body=magnetic_body;
                            ranked.best_distance=magnetic_hit.distance;
                            ranked.best_point=magnetic_hit.point;
                            ranked.best_normal=magnetic_hit.normal;
                            magnetic_winner=true;
                            ++g_magnetic_winners;
                        }
                    }
                }
                if (ranked.best_body) {
                    struct Params { float t; float dist; Vec normal; Vec point; } selected{
                        magnetic_winner ? magnetic_hit.t : 0.0F,
                        ranked.best_distance, ranked.best_normal, ranked.best_point};
                    using IntersectFn = bool(__thiscall*)(void*, void*, void*);
                    auto** native_vtable = *reinterpret_cast<void***>(callback);
                    reinterpret_cast<IntersectFn>(native_vtable[1])(callback,
                        ranked.best_body,&selected);
                    g_selection_winner_distance_millimetres.store(
                        static_cast<std::uint64_t>(ranked.best_distance*1000.0F),
                        std::memory_order_release);
                }
                const std::size_t target_hand =
                    frame.interact_source == runtime::VrHand::left ? 0U : 1U;
                AcquireSRWLockExclusive(&g_interaction_target_lock);
                auto& target = g_interaction_targets[target_hand];
                // Magnetic inventory selection is intentionally separate from
                // Rework's nearby physical-contact assistance.
                target.valid = ranked.best_body != nullptr && !magnetic_winner;
                target.time = target.valid ? GetTickCount64() : 0;
                if (target.valid) {
                    target.point = ranked.best_point;
                    target.body = ranked.best_body;
                } else {
                    target.point = {};
                    target.body = nullptr;
                }
                ReleaseSRWLockExclusive(&g_interaction_target_lock);
                return;
            }
        }
    }
    reinterpret_cast<Ray>(g_image+0x189E30)(world,callback,origin,end,distance,normal,point,prefilter);
}
void __fastcall HookedEnter(void* state, void*, void* previous) {
    CallbackScope scope;
    ++g_grab_enters;
    const auto frame = ReadNativeControllerFrame();
    auto* const player = Read<void*>(state,0x10);
    g_pending_state = g_enabled.load(std::memory_order_acquire) && frame.focused &&
        frame.input.state.interact.just_pressed && g_vr_selection_ready &&
        g_vr_selection_player == player ? state : nullptr;
    if (g_pending_state) g_pending_hand = frame.interact_source;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    if (g_pending_state) ++g_grab_pending;
    reinterpret_cast<Transition>(g_image+0xAC900)(state,previous);
    // ChangeState publishes player+2BC AFTER Enter returns (9CABB).
    // Acquire only when ServiceSpatialInteraction observes the committed state.
}

void __fastcall HookedMoveEnter(void* state, void*, void* previous) {
    CallbackScope scope;
    ++g_move_enters;
    const auto frame = ReadNativeControllerFrame();
    auto* const player = Read<void*>(state,0x10);
    g_pending_move_state = g_enabled.load(std::memory_order_acquire) && frame.focused &&
        frame.input.state.interact.just_pressed && g_vr_selection_ready &&
        g_vr_selection_player == player ? state : nullptr;
    if (g_pending_move_state) g_pending_move_hand = frame.interact_source;
    g_vr_selection_ready = false;
    g_vr_selection_player = nullptr;
    if (g_pending_move_state) ++g_move_pending;
    reinterpret_cast<Transition>(g_image+0xAAC80)(state,previous);
}

void AcquirePendingMove(void* state, std::uint64_t player_generation,
    runtime::VrHand pending_hand) {
    const auto frame=ReadNativeControllerFrame();
    auto* const player=Read<void*>(state,0x10);
    auto* const body=Read<void*>(state,0x54);
    if (!g_enabled.load(std::memory_order_acquire) || g_held.load() || g_move_held.load() ||
        !frame.focused || !frame.input.state.interact.pressed ||
        frame.interact_source != pending_hand ||
        Read<int>(player,0x2BC)!=2 || !BodyMatches(body)) return;
    const int joint_count=reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(body);
    if (joint_count<0) return;
    MoveHold hold;
    if (joint_count==0) {
        if (Read<void*>(body,0x330)!=nullptr || Read<void*>(body,0x10)!=nullptr) return;
    } else if (!BindRecognizedMechanism(body,hold)) {
        ++g_mechanism_rejected;
        return;
    }
    Matrix palm,interaction_pose,body_pose; Vec velocity{},angular{},interaction_velocity{},interaction_angular{};
    if (!HandPose(pending_hand,false,palm,velocity,angular) ||
        !InteractionHandPose(pending_hand,interaction_pose,interaction_velocity,interaction_angular) ||
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
    hold.state=state; hold.player=player; hold.body=body;
    hold.player_generation=player_generation; hold.hand=pending_hand;
    hold.local_body_contact=local_body_contact;
    hold.local_hand_contact=InverseTransformPoint(palm,world_contact);
    hold.previous_palm={palm.values[3],palm.values[7],palm.values[11]};
    hold.max_linear=max_linear; hold.max_angular=max_angular;
    if (hold.mode==MoveHold::Mode::hinge) {
        hold.hinge_lightness=ReworkHingeLightness(mass);
        if (hold.hinge_lightness<=0.0F) {
            ++g_mechanism_rejected;
            return;
        }
    }
    g_move_hold=hold;
    PublishGameplayPalmHeldBody(
        hold.hand == runtime::VrHand::left ? 0U : 1U, hold.body);
    // Rework raises these caps while a Move body is hand-driven so the force
    // or joint servo is not strangled by map-authored carrying limits.
    if (hold.mode==MoveHold::Mode::free_body) {
        SetFloat(body,0x19C360,runtime::vr_mechanism_policy::kFreeMoveMaximumLinearSpeed);
        SetFloat(body,0x19C380,runtime::vr_mechanism_policy::kFreeMoveMaximumAngularSpeed);
    } else {
        SetFloat(body,0x19C360,runtime::vr_mechanism_policy::kJointedMaximumLinearSpeed);
        SetFloat(body,0x19C380,runtime::vr_mechanism_policy::kJointedMaximumAngularSpeed*
            (hold.mode==MoveHold::Mode::hinge ? hold.hinge_lightness : 1.0F));
        ++g_mechanism_acquired;
    }
    g_move_held.store(true,std::memory_order_release);
    ++g_moves_acquired;
    NativeControllerHaptic(hold.hand,runtime::VrHapticEvent::object_pickup);
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
        NativeControllerHaptic(hold.hand,runtime::VrHapticEvent::object_drop);
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
    const Vec desired_delta{
        target_contact[0]-current_contact[0],
        target_contact[1]-current_contact[1],
        target_contact[2]-current_contact[2]};
    if (g_move_hold.mode==MoveHold::Mode::free_body) {
        Vec force{};
        for (std::size_t axis=0;axis<3;++axis)
            force[axis]=desired_delta[axis]*1250.0F*mass;
        // Rework's free Move body applies this force at the selected surface point.
        AddForceAtPosition(g_move_hold.body,force,current_contact);
    } else {
        runtime::vr_mechanism_policy::VrMechanismMotionPlan plan{};
        if (g_move_hold.mode==MoveHold::Mode::slider) {
            plan=runtime::vr_mechanism_policy::PlanSlider(
                desired_delta,g_move_hold.joint_pin);
        } else {
            const Vec body_position{
                body_pose.values[3],body_pose.values[7],body_pose.values[11]};
            plan=runtime::vr_mechanism_policy::PlanHinge(
                desired_delta,g_move_hold.joint_pin,g_move_hold.joint_pivot,
                current_contact,body_position,g_move_hold.hinge_lightness);
        }
        if (!plan.valid) {
            ++g_mechanism_rejected;
            ++g_guarded_releases;
            reinterpret_cast<void(__thiscall*)(void*)>(g_image+0xAA030)(state);
            return;
        }
        SetVelocity(g_move_hold.body,0x19C2A0,plan.linear_velocity);
        SetVelocity(g_move_hold.body,0x19C2C0,plan.angular_velocity);
        ++g_mechanism_updates;
    }
    static_cast<void>(Store(static_cast<std::uint8_t*>(state)+0x44,
        current_contact.data(),sizeof(current_contact)));
    const int move_count=20;
    static_cast<void>(Store(static_cast<std::uint8_t*>(state)+0x58,&move_count,sizeof(move_count)));
}
void AcquirePendingGrab(void* state, std::uint64_t player_generation,
    runtime::VrHand pending_hand) {
    if (!g_player_collision_filter_ready.load(std::memory_order_acquire)) { ++g_blocked_grabs; return; }
    const auto frame = ReadNativeControllerFrame();
    auto* player=Read<void*>(state,0x10);
    auto* body=Read<void*>(state,0x20);
    if (!g_enabled.load(std::memory_order_acquire) || g_held.load() || !frame.focused ||
        !frame.input.state.interact.pressed || frame.interact_source != pending_hand ||
        Read<int>(player,0x2BC)!=6 || !BodyMatches(body) || Read<void*>(body,0x330)!=nullptr ||
        Read<void*>(body,0x10)!=nullptr ||
        reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(body)!=0) return;
    Matrix palm,interaction_pose,body_pose; Vec velocity{},angular{},interaction_velocity{},interaction_angular{};
    if (!HandPose(pending_hand,false,palm,velocity,angular) ||
        !InteractionHandPose(pending_hand,interaction_pose,interaction_velocity,interaction_angular) ||
        !Copy(static_cast<std::uint8_t*>(body)+0x34,&body_pose,sizeof(body_pose))) return;
    const float max_linear=Read<float>(body,0x42C),max_angular=Read<float>(body,0x430);
    const float mass=Read<float>(body,0x434);
    if (!std::isfinite(max_linear) || !std::isfinite(max_angular) || max_linear<0 || max_angular<0 ||
        !std::isfinite(mass) || mass<=0) return;
    Hold hold; hold.state=state; hold.player=player; hold.body=body;
    hold.player_generation=player_generation; hold.hand=pending_hand;
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
    NativeControllerHaptic(hold.hand,runtime::VrHapticEvent::object_pickup);
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
        NativeControllerHaptic(hold.hand,runtime::VrHapticEvent::object_drop);
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
        g_contact_rays.exchange(0),g_nudge_queries.exchange(0),g_nudge_contacts.exchange(0),
        g_nudges_applied.exchange(0),g_interact_presses.exchange(0),
        g_selection_refreshes.exchange(0),g_selection_ray_batches.exchange(0),
        g_selection_rays.exchange(0),g_selection_candidates.exchange(0),
        g_selection_discards.exchange(0),g_selection_winner_distance_millimetres.exchange(0),
        g_selection_central_ray.exchange(0),g_selection_auxiliary_ray.exchange(0),
        g_selection_palm_queries.exchange(0),g_selection_palm_candidates.exchange(0),
        g_selection_palm_winners.exchange(0),g_selection_palm_assisted_winners.exchange(0),
        g_grab_enters.exchange(0),g_move_enters.exchange(0),
        g_grab_pending.exchange(0),g_move_pending.exchange(0),
        g_magnetic_queries.exchange(0),g_magnetic_candidates.exchange(0),
        g_magnetic_visibility_rays.exchange(0),g_magnetic_winners.exchange(0),
        g_mechanism_acquired.exchange(0),g_mechanism_updates.exchange(0),
        g_mechanism_rejected.exchange(0)};
}

bool ReadAttachedToolGrip(std::size_t hand_index,float& pose_weight) noexcept {
    pose_weight=0.0F;
    if (hand_index>=g_tool_grip_time.size()) return false;
    const std::uint64_t time=g_tool_grip_time[hand_index].load(std::memory_order_acquire);
    if (!time || GetTickCount64()-time>kToolGripMaximumAgeMilliseconds) return false;
    const float weight=g_tool_grip_weight[hand_index].load(std::memory_order_acquire);
    if (!std::isfinite(weight) || weight<0.0F || weight>1.0F) return false;
    pose_weight=weight;
    return true;
}

bool ReadSpatialInteractionTarget(
    std::size_t hand_index,
    std::array<float,3>& world_point) noexcept {
    world_point = {};
    if (hand_index >= g_interaction_targets.size()) return false;
    InteractionTarget target{};
    AcquireSRWLockShared(&g_interaction_target_lock);
    target = g_interaction_targets[hand_index];
    ReleaseSRWLockShared(&g_interaction_target_lock);
    if (!target.valid || target.time == 0 ||
        GetTickCount64() - target.time > kInteractionTargetMaximumAgeMilliseconds)
        return false;
    world_point = target.point;
    return std::all_of(world_point.begin(),world_point.end(),
        [](float value) { return std::isfinite(value); });
}

[[nodiscard]] void* ProtectedNudgeBody(
    std::size_t hand_index,
    std::uint64_t now) noexcept {
    if (hand_index >= g_interaction_targets.size()) return nullptr;
    InteractionTarget target{};
    AcquireSRWLockShared(&g_interaction_target_lock);
    target = g_interaction_targets[hand_index];
    ReleaseSRWLockShared(&g_interaction_target_lock);
    if (!target.valid || target.body == nullptr || target.time == 0 ||
        now < target.time ||
        now - target.time > kInteractionTargetMaximumAgeMilliseconds) {
        return nullptr;
    }
    return target.body;
}
bool InstallSpatialInteraction(std::string& error) noexcept {
    error.clear();
    if (AllSpatialHooksInstalled()) {
        g_player_collision_filter_ready.store(true,std::memory_order_release);
        SetGameplayInteractionTargetProvider(&ReadSpatialInteractionTarget);
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
    for (const auto& pair : {std::array<std::uintptr_t,2>{0x34,0x19C2A0}, {0x38,kGetLinearVelocity},
        {0x3C,0x19C2C0},{0x40,kGetAngularVelocity},{0x54,0x19C360},{0x5C,0x19C380},
        {0x7C,0x19C9E0},{0x88,kAddImpulseAtPosition},{0xBC,0x19C590}})
        if (Read<void*>(g_image+kPhysicsBodyVtable,pair[0])!=g_image+pair[1]) { error="Physics body method mismatch"; return false; }
    if (Read<void*>(g_image+kPhysicsJointHingeNewtonVtable,kJointTypeVtableSlot)!=
            g_image+kHingeGetType ||
        Read<void*>(g_image+kPhysicsJointSliderNewtonVtable,kJointTypeVtableSlot)!=
            g_image+kSliderGetType) {
        error="Physics joint type boundary mismatch";
        return false;
    }
    constexpr std::array<std::uint8_t,15> get_joint_entry{
        0x8B,0x81,0x54,0x03,0,0,0x8B,0x4C,0x24,0x04,0x8D,0x04,0x88,0x8B,0x00};
    std::array<std::uint8_t,15> actual_get_joint{};
    if (!Copy(g_image+kGetBodyJoint,actual_get_joint.data(),actual_get_joint.size()) ||
        actual_get_joint!=get_joint_entry) {
        error="Physics body joint lookup mismatch";
        return false;
    }
    constexpr std::array<std::uint8_t,6> hinge_pin_store{0x89,0x86,0xB8,0,0,0};
    constexpr std::array<std::uint8_t,6> hinge_pivot_store{0x89,0x86,0xC4,0,0,0};
    std::array<std::uint8_t,6> actual_joint_store{};
    if (!Copy(g_image+0x19F1B7,actual_joint_store.data(),actual_joint_store.size()) ||
        actual_joint_store!=hinge_pin_store ||
        !Copy(g_image+0x19F1CB,actual_joint_store.data(),actual_joint_store.size()) ||
        actual_joint_store!=hinge_pivot_store ||
        !Copy(g_image+0x19F6DC,actual_joint_store.data(),actual_joint_store.size()) ||
        actual_joint_store!=hinge_pin_store ||
        !Copy(g_image+0x19F6F0,actual_joint_store.data(),actual_joint_store.size()) ||
        actual_joint_store!=hinge_pivot_store) {
        error="Physics joint pin/pivot layout mismatch";
        return false;
    }
    if (Read<void*>(g_image+kPhysicsWorldVtable,kCreateSphereShapeVtableSlot)!=
            g_image+kCreateSphereShape) {
        error="Physics world sphere-shape method mismatch";
        return false;
    }
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
    SetGameplayInteractionTargetProvider(&ReadSpatialInteractionTarget);
    g_enabled.store(true,std::memory_order_release);
    return true;
}
bool RemoveSpatialInteraction(std::string& error) noexcept {
    error.clear();
    g_enabled.store(false,std::memory_order_release);
    g_player_collision_filter_ready.store(false,std::memory_order_release);
    SetGameplayInteractionTargetProvider(nullptr);
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
    } else {
        DestroyNudgeShape();
    }
    return success;
}

void ServiceSpatialHandNudge(void* character_body) noexcept {
    CallbackScope scope;
    if (!g_enabled.load(std::memory_order_acquire) || !g_image || !character_body ||
        NativeInputUiActive()) return;
    const auto frame=ReadNativeControllerFrame();
    if (!frame.focused) return;
    void* const world=Read<void*>(character_body,kCharacterPhysicsWorldOffset);
    void* const player_body=Read<void*>(character_body,kCharacterPhysicsBodyOffset);
    if (!world || !player_body || !BodyMatches(player_body) || !EnsureNudgeShape(world)) return;

    for (std::size_t hand_index=0;hand_index<2;++hand_index) {
        const auto hand=hand_index==0 ? runtime::VrHand::left : runtime::VrHand::right;
        if ((g_held.load(std::memory_order_acquire) && g_hold.hand==hand) ||
            (g_move_held.load(std::memory_order_acquire) && g_move_hold.hand==hand) ||
            (g_pending_state && g_pending_hand==hand) ||
            (g_pending_move_state && g_pending_move_hand==hand) ||
            (frame.input.state.interact.pressed && frame.interact_source==hand)) continue;

        // Rework keeps acquisition and physical pushing as separate routes.
        // Protect only the fresh native selection winner while the hand is
        // approaching it, so the nudge cannot repel the object before Enter
        // establishes ownership. Other contacted props remain pushable.
        void* const protected_body = ProtectedNudgeBody(
            hand_index, GetTickCount64());

        Matrix raw{}; Vec velocity{},angular{};
        if (!RawHandPose(hand,false,raw,velocity,angular) || !FiniteVec(velocity)) continue;
        const float speed=std::hypot(std::hypot(velocity[0],velocity[1]),velocity[2]);
        if (!std::isfinite(speed) || speed<kNudgeMinimumHandSpeed) continue;
        const auto visible=runtime::rework_hand_profile::ApplyVisualLocalPose(raw);
        const Vec center{visible.values[3],visible.values[7],visible.values[11]};
        Matrix nudge=runtime::IdentityMatrix();
        nudge.values[3]=center[0]; nudge.values[7]=center[1]; nudge.values[11]=center[2];
        Vec corrected=center;
        NudgeContactCallback callback;
        bool collided=false;
        if (!SafeNudgeQuery(world,&corrected,g_nudge_shape.shape,&nudge,
                player_body,&callback,collided)) continue;
        ++g_nudge_queries;
        if (!collided || !callback.valid) continue;

        for (std::size_t hit_index=0;hit_index<callback.hit_count;++hit_index) {
            const auto& hit=callback.hits[hit_index];
            if (!hit.body || !hit.contact_count || !BodyMatches(hit.body) ||
                hit.body == protected_body ||
                (g_held.load(std::memory_order_acquire) && g_hold.body==hit.body) ||
                (g_move_held.load(std::memory_order_acquire) && g_move_hold.body==hit.body)) continue;
            g_nudge_contacts.fetch_add(hit.contact_count,std::memory_order_relaxed);
            const float mass=Read<float>(hit.body,0x434);
            if (!std::isfinite(mass) || mass<=0) continue;
            Vec contact{};
            for (std::size_t axis=0;axis<3;++axis)
                contact[axis]=hit.contact_sum[axis]/static_cast<float>(hit.contact_count);
            if (!FiniteVec(contact)) continue;
            const int joint_count=reinterpret_cast<int(__thiscall*)(void*)>(g_image+0xCCF00)(hit.body);
            Vec body_centre{},body_minimum{},body_maximum{};
            float body_radius=0.0F;
            if (!ReadBodyBoundingVolume(hit.body,body_centre,body_minimum,
                    body_maximum,body_radius)) continue;
            NudgePlan plan{};
            if (!BuildNudgePlan(hit.body,velocity,contact,mass,body_radius,
                    joint_count,plan)) continue;
            Vec linear{},body_angular{};
            if (!BodyVelocity(hit.body,kGetLinearVelocity,linear) ||
                !BodyVelocity(hit.body,kGetAngularVelocity,body_angular)) continue;
            const Matrix body_matrix=Read<Matrix>(hit.body,0x34);
            Vec impulse{};
            float nudge_strength=0.0F;
            if (!ComputeNudgeImpulse(center,velocity,plan.velocity,contact,
                    body_matrix,linear,body_angular,mass,plan,impulse,
                    &nudge_strength)) continue;
            if (SafeAddImpulseAtPosition(hit.body,&impulse,&contact)) {
                ++g_nudges_applied;
                NativeControllerHaptic(
                    hand,
                    runtime::VrHapticEvent::interaction,
                    std::clamp(nudge_strength,0.05F,0.5F));
            }
        }
    }
}
void RefreshVrSelectionBeforeInteract(void* player) noexcept {
    CallbackScope scope;
    ++g_interact_presses;
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
    g_vr_selection_refresh_active=true;
    ++g_selection_refreshes;
    reinterpret_cast<Update>(g_image+0xAD6C0)(normal,0);
    g_vr_selection_refresh_active=false;
    if (!g_vr_selection_ready) g_vr_selection_player=nullptr;
}
void ServiceSpatialInteraction(void* player, bool ui) noexcept {
    CallbackScope scope;
    const std::uint64_t player_generation=ObservePlayerGeneration(player);
    if (g_pending_state) {
        auto* pending = g_pending_state;
        const auto pending_hand = g_pending_hand;
        g_pending_state = nullptr;
        if (!ui && player && Read<int>(player,0x2BC)==6 &&
            Read<void*>(Read<void*>(player,0x2C4),6*sizeof(void*))==pending &&
            Read<void*>(pending,0x10)==player)
            AcquirePendingGrab(pending,player_generation,pending_hand);
    }
    if (g_pending_move_state) {
        auto* pending = g_pending_move_state;
        const auto pending_hand = g_pending_move_hand;
        g_pending_move_state = nullptr;
        if (!ui && player && Read<int>(player,0x2BC)==2 &&
            Read<void*>(Read<void*>(player,0x2C4),2*sizeof(void*))==pending &&
            Read<void*>(pending,0x10)==player)
            AcquirePendingMove(pending,player_generation,pending_hand);
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
            NativeControllerHaptic(hand,runtime::VrHapticEvent::object_drop);
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
            NativeControllerHaptic(hold.hand,runtime::VrHapticEvent::object_drop);
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
        NativeControllerHaptic(hand,runtime::VrHapticEvent::object_drop);
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
        NativeControllerHaptic(hold.hand,runtime::VrHapticEvent::object_drop);
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
