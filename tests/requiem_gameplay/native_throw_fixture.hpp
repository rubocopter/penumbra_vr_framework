#pragma once

// Include AFTER render_world.cpp, at global scope. The caller supplies the same
// frame returned by ReadNativeControllerFrame and an executable 0x300000-byte
// g_image, as interaction_consumer_test.cpp does. No game is loaded.
//
// Fixture f(test_frame, absolute_path_to_shipped_Newton_dll);
// if (!f.Ready() || !CharacterizedNativeCases(f)) return false;
// Future integration: f.InstallHooks(&HookedGrabExamine, &HookedThrowDirection);
// Reset(); InvokeExamine(); inspect Events(), Velocity(), Omega(), direction.
// Or run AssistedThrowCases(f, &HookedGrabExamine, &HookedThrowDirection).
// There is intentionally no forward declaration of those future hook names.
#include "../../products/overture/dependencies/include/Newton.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace penumbra_vr::backends::requiem::native_throw_fixture {
using Vec = std::array<float, 3>;
using Cursor = std::array<float, 2>;
using ExamineHook = void(__fastcall*)(void* state, void* unused_edx);
// 113B20: ECX=camera; stack +4=out, +8=cursor, +C=graphics; EAX=out;
// RET 0C. The explicit out pointer is NOT a second hidden structure result.
using Direction = Vec*(__thiscall*)(void* camera, Vec* out,
    const Cursor* cursor, void* graphics);
using DirectionHook = Vec*(__fastcall*)(void* camera, void* unused_edx,
    Vec* out, const Cursor* cursor, void* graphics);
static_assert(sizeof(void*) == 4 && sizeof(dFloat) == 4,
    "This fixture models the characterized Requiem x86/float ABI");

struct NewtonApi final {
    HMODULE module = nullptr;
#define PVR_THROW_NEWTON_EXPORTS(X) \
    X(NewtonCreate, create) X(NewtonDestroy, destroy) \
    X(NewtonCreateBox, box) X(NewtonReleaseCollision, release_shape) \
    X(NewtonCreateBody, body) X(NewtonDestroyBody, destroy_body) \
    X(NewtonBodySetMassMatrix, mass) X(NewtonBodySetVelocity, linear) \
    X(NewtonBodySetOmega, angular) X(NewtonBodyGetVelocity, velocity) \
    X(NewtonBodyGetOmega, omega) X(NewtonAddBodyImpulse, impulse)
#define PVR_THROW_DECLARE(name, member) decltype(&::name) member = nullptr;
    PVR_THROW_NEWTON_EXPORTS(PVR_THROW_DECLARE)
#undef PVR_THROW_DECLARE
    explicit NewtonApi(const wchar_t* absolute_path) {
        module = LoadLibraryExW(absolute_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) return;
#define PVR_THROW_RESOLVE(name, member) \
        { const auto proc = GetProcAddress(module, #name); \
          static_assert(sizeof(proc) == sizeof(member)); \
          std::memcpy(&member, &proc, sizeof(member)); }
        PVR_THROW_NEWTON_EXPORTS(PVR_THROW_RESOLVE)
#undef PVR_THROW_RESOLVE
    }
    bool Ready() const {
#define PVR_THROW_REQUIRE(name, member) && member != nullptr
        return module != nullptr PVR_THROW_NEWTON_EXPORTS(PVR_THROW_REQUIRE);
#undef PVR_THROW_REQUIRE
    }
    ~NewtonApi() { if (module) FreeLibrary(module); }
    NewtonApi(const NewtonApi&) = delete;
    NewtonApi& operator=(const NewtonApi&) = delete;
#undef PVR_THROW_NEWTON_EXPORTS
};

enum class Event {
    change, native_leave, leave_upward_impulse, restore_linear, restore_angular,
    native_enter, commit, zero_linear, zero_angular, direction,
    throw_central, throw_at_point
};

class Fixture final {
public:
    runtime::VrControllerFrame& frame;
    NewtonApi newton;
    std::array<std::uint8_t, 0x500> body_image{};
    std::array<std::uint8_t, 0x400> player_image{};
    std::array<std::uint8_t, 0x180> state_image{}, next_image{};
    std::array<void*, 7> states{};
    std::array<std::uint8_t, 0x100> camera{}, graphics{};
    Vec native_direction{1, 0, 0}, impulse_delta{}, impulse_point{};
    Vec restored_linear{}, restored_angular{};
    void* direction_return_address = nullptr;
    bool direction_arguments_match = false, direction_result_matches = false;
    float mass_at_throw = 0;
    unsigned leaves = 0, enters = 0, changes = 0, throw_impulses = 0;

    Fixture(runtime::VrControllerFrame& controller_frame, const wchar_t* dll)
        : frame(controller_frame), newton(dll), saved_frame_(controller_frame) {
        if (!newton.Ready() || !g_image) return;
        world_ = newton.create(nullptr, nullptr);
        if (!world_) return;
        shape_ = newton.box(world_, 1, 1, 1, nullptr);
        if (!shape_) return;
        current_ = this;
        InstallHooks(nullptr, nullptr);
    }
    ~Fixture() {
        std::string error;
        if(!RemoveRenderWorld(error)) std::cerr<<error<<'\n';
        if (current_ == this) {
            g_grab_hold = {}; g_pending_grab_state = nullptr;
            frame = saved_frame_; current_ = nullptr;
        }
        if (native_body_) newton.destroy_body(world_, native_body_);
        if (shape_) newton.release_shape(world_, shape_);
        if (world_) newton.destroy(world_);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
    bool Ready() const { return hooks_ready_ && shape_ != nullptr && current_ == this; }
    const std::vector<Event>& Events() const { return events_; }
    Vec Velocity() const { Vec v{}; newton.velocity(native_body_, v.data()); return v; }
    Vec Omega() const { Vec v{}; newton.omega(native_body_, v.data()); return v; }

    // Direction calls execute a real CALL whose return address is AA5CC. Both
    // thunks forward all three stack arguments and return EAX with RET 0C.
    void InstallHooks(ExamineHook examine, DirectionHook direction_hook) {
        std::string error;
        hooks_ready_=RemoveRenderWorld(error);
        if(!hooks_ready_) return;
        Jump(0xAA520, reinterpret_cast<void*>(&NativeThrowStub));
        Jump(0xAA920, reinterpret_cast<void*>(&NativeLeaveStub));
        Jump(0x9CEE0, reinterpret_cast<void*>(&NativeChangeStateStub));
        Jump(0x113B20, reinterpret_cast<void*>(&NativeUnprojectStub));
        Jump(0x19CA00, reinterpret_cast<void*>(&LinearStub));
        Jump(0x19CA20, reinterpret_cast<void*>(&AngularStub));
        Jump(0x19CAC0, reinterpret_cast<void*>(&MaxLinearStub));
        Jump(0x19CAE0, reinterpret_cast<void*>(&MaxAngularStub));
        Jump(0x19CB00, reinterpret_cast<void*>(&MassStub));
        Jump(0x19CFE0, reinterpret_cast<void*>(&SetMassStub));
        Jump(0x19D2F0, reinterpret_cast<void*>(&CentralStub));
        Jump(0x19CB30, reinterpret_cast<void*>(&PointStub));
        Store(g_image, 0x27E258, g_image+0xAA520);
        Store(g_image, 0x27E2A0, reinterpret_cast<void*>(&HookedGrabLeave));
        Store(g_image, kPhysicsBodyVtableRva + 0x34, g_image + 0x19CA00);
        Store(g_image, kPhysicsBodyVtableRva + 0x3C, g_image + 0x19CA20);
        Store(g_image, kPhysicsBodyVtableRva + 0x68, g_image + 0x19CFE0);
        Store(g_image, kPhysicsBodyVtableRva + 0x6C, g_image + 0x19CB00);
        Store(g_image, kPhysicsBodyVtableRva + 0x84, g_image + 0x19D2F0);
        Store(g_image, kPhysicsBodyVtableRva + 0x88, g_image + 0x19CB30);
        Store(g_image, 0x270800 + 0x5C, reinterpret_cast<void*>(&NativeEnterStub));
        constexpr std::array<std::uint8_t, 4> push{0xFF, 0x74, 0x24, 0x0C};
        for (unsigned i = 0; i != 3; ++i)
            std::memcpy(g_image + 0xAA5BB + 4 * i, push.data(), push.size());
        auto* call = g_image + 0xAA5C7;
        call[0] = 0xE8;
        const auto target = static_cast<void*>(g_image + 0x113B20);
        const auto delta = static_cast<std::int32_t>(
            reinterpret_cast<std::intptr_t>(target) -
            reinterpret_cast<std::intptr_t>(call + 5));
        std::memcpy(call + 1, &delta, sizeof(delta));
        const std::array<std::uint8_t, 3> ret{0xC2, 0x0C, 0};
        std::memcpy(call + 5, ret.data(), ret.size());
        FlushInstructionCache(GetCurrentProcess(), g_image, 0x300000);
        if(examine) hooks_ready_=hooks::InstallPointerHook(
            reinterpret_cast<void**>(g_image+kGrabExamineSlotRva),
            g_image+kGrabExamineRva,reinterpret_cast<void*>(examine),g_grab_hooks[3],error);
        if(hooks_ready_ && direction_hook) hooks_ready_=hooks::InstallRel32CallHook(
            g_image+kThrowDirectionCallRva,kThrowDirectionCall,
            reinterpret_cast<void*>(direction_hook),g_throw_direction_hook,error);
        if(!hooks_ready_) std::cerr<<error<<'\n';
    }

    void Reset(float original_mass = 1.1F, bool owned = true) {
        if (native_body_) newton.destroy_body(world_, native_body_);
        native_body_ = newton.body(world_, shape_);
        body_image.fill(0); player_image.fill(0);
        state_image.fill(0); next_image.fill(0);
        events_.clear(); zero_phase_ = false; in_leave_ = false;
        leaves = enters = changes = throw_impulses = 0;
        direction_arguments_match = direction_result_matches = false;
        direction_return_address = nullptr;
        mass_at_throw = 0; impulse_delta = impulse_point = {};
        restored_linear = restored_angular = {};
        native_direction = {1, 0, 0};
        Store(body_image.data(), 0, g_image + kPhysicsBodyVtableRva);
        Store(body_image.data(), 0x34, runtime::IdentityMatrix());
        Store(body_image.data(), 0x41C, native_body_);
        SetMassStub(body_image.data(), nullptr, original_mass / 5.0F);
        Store(body_image.data(), 0x42C, 20.0F);
        Store(body_image.data(), 0x430, 30.0F);
        Store(state_image.data(), 0, g_image + 0x27E240);
        Store(state_image.data(), 4, 6);
        Store(state_image.data(), 0x10, static_cast<void*>(player_image.data()));
        Store(state_image.data(), 0x20, static_cast<void*>(body_image.data()));
        Store(state_image.data(), 0x28, 0);
        Store(state_image.data(), 0xDC, original_mass);
        Store(state_image.data(), 0x138, 8.0F);
        Store(state_image.data(), 0x13C, 30.0F);
        Store(state_image.data(), 0x140, 2.0F);
        Store(state_image.data(), 0x144, 13.0F);
        Store(next_image.data(), 0, g_image + 0x270800);
        states[0] = next_image.data(); states[6] = state_image.data();
        Store(player_image.data(), 0x2C0, 6);
        Store(player_image.data(), 0x2C8, states.data());
        Store(player_image.data(), 0x50, true);
        Store(player_image.data(), 0x1E0, static_cast<void*>(camera.data()));
        Store(player_image.data(), 0x2EC, Cursor{320, 240});
        frame = {}; frame.focused = true;
        frame.input.state.interact = {true, true, false, false};
        frame.input.state.examine = {true, true, true, false};
        frame.interact_source = runtime::VrHand::left;
        for (auto& hand : frame.hands) {
            hand.grip.device_connected = hand.grip.pose_valid = true;
            hand.aim.device_connected = hand.aim.pose_valid = true;
            hand.grip.device_to_absolute = {};
            hand.aim.device_to_absolute = {};
            for (unsigned i = 0; i != 3; ++i) {
                hand.grip.device_to_absolute.values[i * 4 + i] = 1;
                hand.aim.device_to_absolute.values[i * 4 + i] = 1;
            }
        }
        frame.hands[0].grip.velocity = {0, 4, 0};
        frame.hands[1].grip.velocity = {7, 0, 0};
        g_world_from_tracking = runtime::IdentityMatrix();
        RefreshReference();
        g_pending_grab_state = nullptr; g_grab_hold = {};
        if (owned) {
            g_grab_hold.state = state_image.data();
            g_grab_hold.player = player_image.data();
            g_grab_hold.body = body_image.data();
            g_grab_hold.hand = runtime::VrHand::left;
            g_grab_hold.max_linear = g_grab_hold.max_angular = 100;
            g_grab_hold.collide_character = false;
        }
    }
    void RefreshReference() {
        g_world_from_tracking_time = GetTickCount64();
        g_world_from_tracking_yaw_epoch = g_world_yaw_epoch.load();
    }
    void InvokeExamine(bool refresh_reference=true) {
        if(refresh_reference) RefreshReference();
        reinterpret_cast<void(__thiscall*)(void*)>(
            ReadNative<void*>(g_image, 0x27E258))(state_image.data());
    }
    Vec CallDirectionOutsideExamine() {
        Vec out{};
        reinterpret_cast<Direction>(g_image + 0xAA5BB)(camera.data(), &out,
            reinterpret_cast<const Cursor*>(player_image.data() + 0x2EC), graphics.data());
        return out;
    }
    template<class T> static void Store(void* object, std::size_t offset, const T& value) {
        std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
    }

    // Characterized AA520, NOT the proposed production policy. The real native
    // direction/impulse slots and the actual production Leave/Restore run here.
    static void __fastcall NativeThrowStub(void* state, void*) {
        auto& f = *current_;
        auto* player = ReadNative<void*>(state, 0x10);
        NativeChangeStateStub(player, nullptr, ReadNative<int>(state, 0x28));
        if (!ReadNative<bool>(player, 0x50)) return;
        auto* body = ReadNative<void*>(state, 0x20);
        f.zero_phase_ = true;
        const Vec zero{};
        SetBodyVelocity(body, 0x19CA00, zero);
        SetBodyVelocity(body, 0x19CA20, zero);
        f.events_.push_back(Event::direction);
        Vec direction{};
        const auto* cursor = reinterpret_cast<const Cursor*>(
            static_cast<const std::uint8_t*>(player) + 0x2EC);
        auto* returned = reinterpret_cast<Direction>(g_image + 0xAA5BB)(
            ReadNative<void*>(player, 0x1E0), &direction, cursor, f.graphics.data());
        f.direction_result_matches = returned == &direction;
        const auto point = TransformPoint(ReadNative<runtime::VrMatrix44>(body, 0x34),
            ReadNative<Vec>(state, 0x14));
        f.mass_at_throw = ReadNative<float>(body, 0x434);
        const float low = ReadNative<float>(state, 0x138);
        const float high = ReadNative<float>(state, 0x13C);
        const float weight = f.mass_at_throw < low ? 1.0F :
            f.mass_at_throw >= high ? 0.0F : 1 - (f.mass_at_throw - low) / (high - low);
        const float magnitude = (1 - weight) * ReadNative<float>(state, 0x140) +
            weight * ReadNative<float>(state, 0x144);
        for (auto& value : direction) value *= magnitude;
        auto* table = ReadNative<void*>(body, 0);
        if (ReadNative<bool>(state, 0xE1))
            reinterpret_cast<void(__thiscall*)(void*, const Vec*, const Vec*)>(
                ReadNative<void*>(table, 0x88))(body, &direction, &point);
        else reinterpret_cast<void(__thiscall*)(void*, const Vec*)>(
            ReadNative<void*>(table, 0x84))(body, &direction);
        f.zero_phase_ = false;
    }

private:
    bool hooks_ready_=true;
    inline static thread_local Fixture* current_ = nullptr;
    runtime::VrControllerFrame saved_frame_;
    const NewtonWorld* world_ = nullptr;
    NewtonCollision* shape_ = nullptr;
    NewtonBody* native_body_ = nullptr;
    std::vector<Event> events_;
    bool zero_phase_ = false, in_leave_ = false;
    static void Jump(std::uintptr_t rva, void* destination) {
        g_image[rva] = 0xE9;
        const auto delta = static_cast<std::int32_t>(
            reinterpret_cast<std::intptr_t>(destination) -
            reinterpret_cast<std::intptr_t>(g_image + rva + 5));
        std::memcpy(g_image + rva + 1, &delta, sizeof(delta));
    }
    static void __fastcall NativeChangeStateStub(void* player, void*, int next) {
        auto& f = *current_; ++f.changes; f.events_.push_back(Event::change);
        auto** states = ReadNative<void**>(player, 0x2C8);
        const int old = ReadNative<int>(player, 0x2C0);
        if (old == next) return;
        auto* old_table = ReadNative<void*>(states[old], 0);
        reinterpret_cast<Transition>(ReadNative<void*>(old_table, 0x60))(
            states[old], states[next]);
        auto* next_table = ReadNative<void*>(states[next], 0);
        reinterpret_cast<Transition>(ReadNative<void*>(next_table, 0x5C))(
            states[next], states[old]);
        Store(states[next], 8, ReadNative<int>(states[old], 4));
        Store(player, 0x2C0, next); f.events_.push_back(Event::commit);
    }
    static void __fastcall NativeLeaveStub(void* state, void*, void*) {
        auto& f = *current_; ++f.leaves; f.events_.push_back(Event::native_leave);
        auto* body = ReadNative<void*>(state, 0x20);
        Store(body, 0x3C4, ReadNative<bool>(state, 0xE0));
        Store(body, 0x428, ReadNative<bool>(state, 0xD4));
        f.in_leave_ = true;
        const Vec upward{0, ReadNative<float>(body, 0x434), 0};
        CentralStub(body, nullptr, &upward); f.in_leave_ = false;
        if (!ReadNative<bool>(ReadNative<void*>(state, 0x10), 0x4A))
            SetMassStub(body, nullptr, ReadNative<float>(state, 0xDC));
    }
    static void __fastcall NativeEnterStub(void*, void*, void*) {
        auto& f = *current_; ++f.enters; f.events_.push_back(Event::native_enter);
    }
    static Vec* __fastcall NativeUnprojectStub(void* camera, void*, Vec* out,
        const Cursor* cursor, void* graphics) {
        auto& f = *current_;
        f.direction_return_address = _ReturnAddress();
        f.direction_arguments_match = camera == f.camera.data() &&
            cursor == reinterpret_cast<const Cursor*>(f.player_image.data() + 0x2EC) &&
            graphics == f.graphics.data();
        *out = f.native_direction; return out;
    }
    static void __fastcall LinearStub(void* body, void*, const Vec* value) {
        auto& f = *current_;
        f.events_.push_back(f.zero_phase_ ? Event::zero_linear : Event::restore_linear);
        if (!f.zero_phase_) f.restored_linear = *value;
        f.newton.linear(ReadNative<const NewtonBody*>(body, 0x41C), value->data());
    }
    static void __fastcall AngularStub(void* body, void*, const Vec* value) {
        auto& f = *current_;
        f.events_.push_back(f.zero_phase_ ? Event::zero_angular : Event::restore_angular);
        if (!f.zero_phase_) f.restored_angular = *value;
        f.newton.angular(ReadNative<const NewtonBody*>(body, 0x41C), value->data());
    }
    static void __fastcall MaxLinearStub(void* body, void*, float value) { Store(body, 0x42C, value); }
    static void __fastcall MaxAngularStub(void* body, void*, float value) { Store(body, 0x430, value); }
    static float __fastcall MassStub(void* body, void*) { return ReadNative<float>(body, 0x434); }
    static void __fastcall SetMassStub(void* body, void*, float value) {
        Store(body, 0x434, value);
        current_->newton.mass(ReadNative<const NewtonBody*>(body, 0x41C),
            value, value, value, value);
    }
    static void __fastcall CentralStub(void* body, void*, const Vec* delta) {
        auto& f = *current_; const Vec centre{};
        if (f.in_leave_) f.events_.push_back(Event::leave_upward_impulse);
        else { ++f.throw_impulses; f.events_.push_back(Event::throw_central);
            f.impulse_delta = *delta; f.impulse_point = centre; }
        f.newton.impulse(ReadNative<const NewtonBody*>(body, 0x41C), delta->data(), centre.data());
    }
    static void __fastcall PointStub(void* body, void*, const Vec* delta, const Vec* point) {
        auto& f = *current_; ++f.throw_impulses; f.events_.push_back(Event::throw_at_point);
        f.impulse_delta = *delta; f.impulse_point = *point;
        f.newton.impulse(ReadNative<const NewtonBody*>(body, 0x41C), delta->data(), point->data());
    }
};

inline bool Near(const Vec& a, const Vec& b, float epsilon = 0.0001F) {
    for (unsigned i = 0; i != 3; ++i) if (std::abs(a[i] - b[i]) > epsilon) return false;
    return true;
}

// Positive characterization uses real HookedGrabLeave/RestoreGrab and Newton.
// Calling with future hooks installed is deliberately NOT this baseline test.
inline bool CharacterizedNativeCases(Fixture& f) {
    f.InstallHooks(nullptr, nullptr);
    for (const auto profile : std::array<std::array<float, 2>, 4>{{
            {1.1F, 13}, {8, 13}, {19, 7.5F}, {30, 2}}}) {
        f.Reset(profile[0]); f.InvokeExamine();
        const std::vector<Event> expected{Event::change, Event::native_leave,
            Event::leave_upward_impulse, Event::restore_linear, Event::restore_angular,
            Event::native_enter, Event::commit, Event::zero_linear,
            Event::zero_angular, Event::direction, Event::throw_central};
        if (f.Events() != expected || f.leaves != 1 || f.enters != 1 || f.changes != 1 ||
            f.throw_impulses != 1 || g_grab_hold.state != nullptr ||
            !Near(f.restored_linear, {0, 5, 0}) ||
            !Near(f.Velocity(), {profile[1], 0, 0}) || !Near(f.Omega(), {}) ||
            !f.direction_arguments_match || !f.direction_result_matches ||
            f.direction_return_address != g_image + 0xAA5CC ||
            std::abs(f.mass_at_throw - profile[0]) > 0.0001F ||
            ReadNative<int>(f.player_image.data(), 0x2C0) != 0) {
            std::cerr << "RQ native throw order/profile/ABI failed, mass=" << profile[0] << '\n';
            return false;
        }
    }
    f.Reset(); Fixture::Store(f.player_image.data(), 0x50, false); f.InvokeExamine();
    if (f.throw_impulses || !Near(f.Velocity(), {0, 5, 0}) || f.leaves != 1) return false;
    f.Reset(); Fixture::Store(f.state_image.data(), 0xE1, true);
    Fixture::Store(f.state_image.data(), 0x14, Vec{0, 0.5F, 0}); f.InvokeExamine();
    if (f.throw_impulses != 1 || !Near(f.impulse_point, {0, 0.5F, 0}) ||
            !Near(f.impulse_delta, {13, 0, 0}) || Near(f.Omega(), {})) return false;
    // Ordinary grip release goes through the actual Leave, without AA520's
    // subsequent reset/impulse. Fresh release sample and Rework caps must win.
    f.Reset(); f.frame.hands[0].grip.velocity = {20, 0, 0};
    f.frame.hands[0].grip.angular_velocity = {20, 0, 0};
    f.frame.input.state.interact = {true, false, false, true};
    reinterpret_cast<void(__thiscall*)(void*, int)>(g_image + 0x9CEE0)(
        f.player_image.data(), 0);
    if (f.throw_impulses || !Near(f.Velocity(), {9, 0, 0}) ||
            !Near(f.Omega(), {6, 0, 0})) return false;
    return true;
}

// Intentionally RED with the characterized native callbacks: native B points
// +X while the grabbing LEFT hand points -Z. No replacement throw algorithm is
// supplied by this header. Only production hook callbacks can make this green.
inline bool AssistedThrowCases(Fixture& f, ExamineHook examine, DirectionHook direction) {
    f.InstallHooks(examine, direction);
    if(!f.Ready() || !RenderHooksInstalled()) return false;
    const auto launch = [&](const Vec& expected, const char* label) {
        f.InvokeExamine();
        if (f.changes != 1 || f.leaves != 1 || f.enters != 1 || f.throw_impulses != 1 ||
                !Near(f.impulse_delta, expected) || !Near(f.Velocity(), expected) ||
                !Near(f.Omega(), {}) || !f.direction_result_matches ||
                ReadNative<int>(f.player_image.data(), 0x2C0) != 0) {
            std::cerr << "RQ assisted throw failed: " << label << '\n'; return false;
        }
        // Same genuine AA5C7 CALL outside Examine must not retain hand context.
        if (!Near(f.CallDirectionOutsideExamine(), f.native_direction)) return false;
        return true;
    };
    f.Reset();
    f.frame.hands[1].aim.device_to_absolute.values[0] = -1;
    f.frame.hands[1].aim.device_to_absolute.values[10] = -1;
    if (!launch({0, 0, -13}, "left owner / right B / native camera +X")) return false;
    f.Reset();
    auto& aim = f.frame.hands[0].aim.device_to_absolute.values;
    aim[0] = aim[10] = 0; aim[2] = 1; aim[8] = -1;
    if (!launch({-13, 0, 0}, "aim must beat differently oriented grip")) return false;
    f.Reset(); f.frame.hands[0].aim.pose_valid = false;
    if (!launch({0, 0, -13}, "missing aim uses owner grip")) return false;
    f.Reset(); f.frame.hands[0].aim.device_connected = false;
    if (!launch({0, 0, -13}, "disconnected aim uses owner grip")) return false;
    f.Reset();
    auto& world = g_world_from_tracking.values;
    world[0] = world[10] = 0; world[2] = 1; world[8] = -1;
    if (!launch({-13, 0, 0}, "nonidentity world rotation / local -Z")) return false;
    f.Reset(1.1F, false);
    if (!launch({13, 0, 0}, "native-origin Examine remains native")) return false;
    f.Reset(); f.frame.hands[0].aim.pose_valid = f.frame.hands[0].grip.pose_valid = false;
    if (!launch({13, 0, 0}, "missing owner pose retains native fallback")) return false;
    f.Reset();
    g_world_from_tracking_identity={1,1,2,3};
    f.frame.hands[0].aim.identity={2,2,4,3};
    f.frame.hands[0].grip.identity={2,2,4,3};
    if (!launch({13, 0, 0}, "mismatched tracking epoch retains native fallback")) return false;
    g_world_from_tracking_identity={};
    f.Reset();
    AddTrackedWorldYaw(1.57079632679F);
    f.InvokeExamine(false);
    if(f.throw_impulses!=1 || !Near(f.impulse_delta,{13,0,0})) {
        std::cerr<<"RQ assisted throw reused a reference from before turn\n"; return false;
    }
    f.Reset(); Fixture::Store(f.player_image.data(), 0x50, false); f.InvokeExamine();
    if (f.throw_impulses || f.changes != 1 || f.leaves != 1 ||
            !Near(f.Velocity(), {0, 5, 0})) return false;
    return true;
}

inline bool GestureReleaseCases(Fixture& f) {
    for (unsigned gate=0;gate<8;++gate) {
        f.Reset();
        g_world_from_tracking_identity={1,1,2,3};
        f.frame.hands[0].grip.identity={2,2,2,3};
        f.frame.hands[0].grip.velocity={4,0,0};
        f.frame.hands[0].grip.angular_velocity={0,4,0};
        // Translate the reference far away: velocities rotate, never translate.
        auto& world=g_world_from_tracking.values;
        world[0]=world[10]=0; world[2]=1; world[8]=-1; world[3]=100;
        if(gate==1) f.frame.focused=false;
        if(gate==2) f.frame.hands[0].grip.device_connected=false;
        if(gate==3) f.frame.hands[0].grip.pose_valid=false;
        if(gate==4) g_world_from_tracking_time=GetTickCount64()-101;
        if(gate==5) f.frame.hands[0].grip.identity.pose_epoch=4;
        if(gate==6) f.frame.hands[0].grip.identity.yaw_epoch=4;
        if(gate==7) AddTrackedWorldYaw(1.57079632679F);
        reinterpret_cast<void(__thiscall*)(void*,int)>(g_image+0x9CEE0)(
            f.player_image.data(),0);
        const Vec linear=gate==0 ? Vec{0,0,-5} : Vec{};
        const Vec angular=gate==0 ? Vec{0,2,0} : Vec{};
        if(f.throw_impulses || f.leaves!=1 || !Near(f.Velocity(),linear) ||
            !Near(f.Omega(),angular)) {
            std::cerr<<"RQ gesture release guard/rotation failed: "<<gate<<'\n'; return false;
        }
    }
    g_world_from_tracking_identity={};
    return true;
}
} // namespace penumbra_vr::backends::requiem::native_throw_fixture
