#include "native_input_bridge.hpp"
#include "black_plague_body_adapter.hpp"
#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#include "vr_haptics.hpp"
#include "vr_hand_pose.hpp"
#include "vr_native_intents.hpp"
#include "legacy_input_abi.hpp"
#include "render_world_probe.hpp"
#include "spatial_interaction.hpp"
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
runtime::VrUpdateTiming g_update_timing;
runtime::VrUpdateTimingSample g_timing_sample;
runtime::VrSettings g_settings;
using A = runtime::NativeVrAction;
using Q = runtime::NativeVrQuery;
struct Entry { std::uintptr_t site; A action; Q query = Q::pressed; };
// Exact initialized FD316F... image. Each entry is a decoded E8 instruction
// inside cButtonHandler::Update; do not extend by scanning arbitrary byte hits.
constexpr Entry kQueries[] = {
    {0x3DEC,A::pause},{0x3EA0,A::pause},{0x3FC9,A::pause},{0x4065,A::pause},
    {0x40E7,A::pause},{0x42C4,A::pause},{0x449D,A::pause},{0x47C9,A::pause},
    {0x4838,A::pause},{0x4992,A::pause},{0x49FF,A::pause},{0x4B4C,A::pause},
    {0x4CC4,A::pause},{0x500F,A::pause},
    {0x3E1A,A::select},{0x3ECE,A::select},{0x3F57,A::select},{0x3FF7,A::select},
    {0x40A5,A::select},{0x41C6,A::select},{0x43A3,A::select},{0x46B5,A::select},
    {0x48C6,A::select},{0x4A5B,A::select},{0x4B7A,A::select},{0x4CF2,A::select},
    {0x41F6,A::select},{0x43D3,A::select},{0x46DE,A::select},{0x48F6,A::select},
    {0x4A8B,A::select},{0x4BAA,A::select},{0x4D22,A::select},
    {0x4212,A::select,Q::released},{0x43EF,A::select,Q::released},
    {0x46FA,A::select,Q::released},{0x4912,A::select,Q::released},
    {0x4AA7,A::select,Q::released},{0x4D73,A::select,Q::released},
    {0x3E4A,A::back},{0x3EFE,A::back},{0x3F29,A::back},{0x3F87,A::back},
    {0x4027,A::back},{0x4085,A::back},{0x4115,A::back},{0x42F2,A::back},
    {0x4619,A::back},{0x487A,A::back},{0x4A2D,A::back},{0x4DA3,A::back},
    {0x4145,A::back},{0x4322,A::back},{0x4642,A::back},{0x48AA,A::back},{0x4DD3,A::back},
    {0x4161,A::back,Q::released},{0x433E,A::back,Q::released},
    {0x465E,A::back,Q::released},{0x4DEF,A::back,Q::released},
    {0x5096,A::inventory},{0x50BE,A::notebook},{0x5131,A::light},
    {0x5299,A::jump},{0x52C1,A::jump,Q::held},{0x56E9,A::jump,Q::held},
    {0x52EB,A::sprint},{0x5313,A::sprint,Q::released},
    {0x533B,A::crouch},{0x536A,A::crouch,Q::released},{0x537E,A::crouch,Q::held},
    {0x542A,A::interact},{0x546E,A::interact,Q::released},
    {0x5496,A::examine},{0x54BE,A::examine,Q::released},{0x54E6,A::holster}
};
constexpr std::uintptr_t Target(Q query) {
    return query == Q::held ? 0xDA470 : query == Q::released ? 0xDA510 : 0xDA5B0;
}
std::array<std::uint8_t, 5> CallBytes(std::uintptr_t site, std::uintptr_t target) {
    std::array<std::uint8_t, 5> bytes{0xE8};
    const auto displacement = static_cast<std::int32_t>(target - site - 5);
    std::memcpy(bytes.data() + 1, &displacement, 4); return bytes;
}
// MSVC 2003 std::string is passed by value as 28 stack bytes. Never construct
// or destroy it with the framework's modern CRT: the original query owns it.
using LegacyString = adapters::hpl1::LegacyInputString;
using Query = adapters::hpl1::LegacyInputQuery;
using Update = void(__thiscall*)(void*, float);
using Move = void(__thiscall*)(void*, float, float);
using Yaw = void(__thiscall*)(void*, float);
std::uint8_t* g_image = nullptr;
hooks::IatHook g_update_hook;
std::array<hooks::Rel32CallHook, std::size(kQueries)> g_query_hooks;
std::array<hooks::Rel32CallHook, 2> g_move_hooks;
struct PointerEntry { std::uintptr_t site, target, cursor; };
constexpr PointerEntry kPointers[]{{0x4477,0x797B0,0xA0}, {0x4C70,0x945F0,0x84}, {0x4FF2,0x6C7C0,0xAC}};
std::array<hooks::Rel32CallHook, std::size(kPointers)> g_pointer_hooks;
std::atomic<bool> g_ui{true};
std::atomic<bool> g_installed{false};
std::atomic<void*> g_player{nullptr};
SRWLOCK g_session_lock = SRWLOCK_INIT;
runtime::OpenVrSession* g_session = nullptr;
runtime::VrControllerFrame g_frame;
std::array<std::array<float, 5>, 2> g_hand_curls{};
runtime::VrInputState g_disconnect_release;
std::atomic<std::uint64_t> g_release_pending{0};
std::uint64_t g_release_generation = 0;
runtime::VrSnapTurn g_turn;
thread_local runtime::VrNativeIntents* g_intents = nullptr;
thread_local void* g_input_player = nullptr;
thread_local bool g_pointer_valid = false;
thread_local std::array<float, 2> g_pointer_uv{};
thread_local std::uint64_t g_mouse_override_until = 0;

bool ReadBytes(const void* source, void* dest, std::size_t size) noexcept {
    if (!source) return false;
    __try { std::memcpy(dest, source, size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T Read(const void* object, std::uintptr_t offset) noexcept {
    T result{};
    if (object) static_cast<void>(ReadBytes(static_cast<const std::uint8_t*>(object) + offset, &result, sizeof(result)));
    return result;
}
[[nodiscard]] std::uintptr_t DecodeCallTarget(
    const std::array<std::uint8_t, 5>& bytes, std::uintptr_t call) noexcept {
    if (bytes[0] != 0xE8) return 0;
    std::int32_t displacement = 0;
    std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
    return call + bytes.size() + displacement;
}
bool UiContext(void* handler) {
    if (Read<int>(handler, 0x3C) != 1) return true;
    const auto* init = Read<void*>(handler, 0x2C);
    const auto* player = Read<void*>(handler, 0x38);
    if (!init || !player || !Read<bool>(player, 0x1DC)) return true;
    if (Read<bool>(Read<void*>(player, 0x28C), 0)) return true;
    // These are the same overlay checks/ordering as the mapped native handler.
    return Read<int>(Read<void*>(init, 0x1AC), 0x30) != 0 ||
        Read<bool>(Read<void*>(init, 0x180), 0x2D) ||
        Read<bool>(Read<void*>(init, 0x17C), 0x31) ||
        Read<bool>(Read<void*>(init, 0x178), 0x44) ||
        Read<bool>(Read<void*>(init, 0x164), 0x5C);
}
bool __fastcall HookedQuery(void* input, void*, LegacyString name) {
    const auto return_rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) -
                            reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& entry : kQueries) {
        if (entry.site + 5 != return_rva) continue;
        // Always execute native query first, including its string destructor and
        // native edge bookkeeping, even when VR has a matching pressed button.
        const bool native = reinterpret_cast<Query>(g_image + Target(entry.query))(input, name);
        const bool vr = g_intents && g_intents->Query(entry.action, entry.query);
        if (vr && !native && entry.action == A::light && g_input_player) {
            auto* glow = Read<void*>(g_input_player,0x29C);
            auto* flashlight = Read<void*>(g_input_player,0x290);
            if (!glow || !flashlight) return native;
            const auto plan = runtime::PlanQuickLight(Read<bool>(glow,0),Read<bool>(flashlight,4));
            if (plan.toggle_glow)
                reinterpret_cast<void(__thiscall*)(void*)>(g_image+0x9BD80)(g_input_player);
            // The native handler executes StartFlashLightButton on true.
            return plan.toggle_flashlight;
        }
        if (vr && entry.action == A::interact && entry.query == Q::pressed)
            RefreshVrSelectionBeforeInteract(g_input_player);
        return native || vr;
    }
    return reinterpret_cast<Query>(g_image + Target(Q::pressed))(input, name);
}
void __fastcall HookedForward(void* player, void*, float amount, float dt) {
    const float intent = g_intents ? g_intents->Move(amount, false) : amount;
    if (!PublishBlackPlagueForwardIntent(player, intent, dt)) {
        reinterpret_cast<Move>(g_image + 0x9CBC0)(player, intent, dt);
    }
}
void __fastcall HookedSideways(void* player, void*, float amount, float dt) {
    const float intent = g_intents ? g_intents->Move(amount, true) : amount;
    if (!PublishBlackPlagueSidewaysIntent(player, intent, dt)) {
        reinterpret_cast<Move>(g_image + 0x9CC60)(player, intent, dt);
    }
}
void __fastcall HookedPointer(void* menu, void*, const std::array<float, 2>* physical_delta) {
    using Pointer = void(__thiscall*)(void*, const std::array<float, 2>*);
    const auto return_rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - reinterpret_cast<std::uintptr_t>(g_image);
    for (const auto& entry : kPointers) {
        if (entry.site + 5 != return_rva) continue;
        std::array<float, 2> delta{};
        if (!ReadBytes(physical_delta, delta.data(), sizeof(delta))) return;
        const auto now = GetTickCount64();
        if (std::abs(delta[0]) > 0.01F || std::abs(delta[1]) > 0.01F) g_mouse_override_until = now + 1500;
        if (g_intents && g_pointer_valid && now >= g_mouse_override_until) {
            const float x = Read<float>(menu, entry.cursor), y = Read<float>(menu, entry.cursor + 4);
            std::array<float, 2> smoothed{};
            if (runtime::SmoothMenuPointerUv(
                    {x / 800.0F, y / 600.0F}, g_pointer_uv, 0.40F, smoothed)) {
                delta = {smoothed[0] * 800.0F - x, smoothed[1] * 600.0F - y};
            }
        }
        reinterpret_cast<Pointer>(g_image + entry.target)(menu, &delta);
        return;
    }
}
void __fastcall HookedUpdate(void* handler, void*, float dt) {
    const bool ui = UiContext(handler);
    const auto context = ui ? runtime::VrInputContext::ui : runtime::VrInputContext::gameplay;
    g_ui.store(ui, std::memory_order_release);
    runtime::VrControllerFrame frame;
    std::uint64_t consumed_release = 0;
    AcquireSRWLockExclusive(&g_session_lock);
    if (g_release_pending.load(std::memory_order_acquire)) {
        consumed_release = g_release_pending.load(std::memory_order_acquire);
        frame.input.state = g_disconnect_release;
        g_disconnect_release = {};
        g_frame = {};
        g_hand_curls = {};
    } else if (g_session) {
        std::string error;
        g_session->SetControllerMoveDeadZone(g_settings.move_dead_zone);
        const auto handedness = g_settings.handedness == runtime::VrHandedness::left ?
            runtime::VrHand::left : runtime::VrHand::right;
        static_cast<void>(g_session->ReadControllerInput(context, handedness,
                                                        GetTickCount64(), frame, error));
        for (std::size_t hand_index = 0; hand_index < frame.hands.size(); ++hand_index) {
            auto& hand = frame.hands[hand_index];
            if (!hand.skeleton_valid) {
                g_hand_curls[hand_index] = {};
                continue;
            }
            runtime::VrHandCurlInput curl_input;
            curl_input.valid = true;
            curl_input.skeletal = true;
            curl_input.finger_curl = hand.finger_curl;
            // The Framework action layer does not currently expose normalized
            // grip/trigger analogs, so use only the evidenced skeletal portion
            // of the shared policy here. Do not synthesize missing analog data.
            const auto target = runtime::BuildVrHandCurlTargets(curl_input);
            runtime::SmoothVrHandCurls(g_hand_curls[hand_index], target, dt);
            hand.finger_curl = g_hand_curls[hand_index];
        }
        g_frame = frame;
    }
    if (frame.input.state.move.active) {
        frame.input.state.move.x *= g_settings.move_speed;
        frame.input.state.move.y *= g_settings.move_speed;
        g_frame.input.state.move = frame.input.state.move;
    }
    const float turn = g_turn.Update(
        frame.input.state.turn, !ui && frame.focused,
        g_settings.turn_mode, dt, g_settings.snap_turn_angle, g_settings.smooth_turn_speed,
        g_settings.turn_dead_zone);
    const auto timing=g_update_timing.Update(dt,GetTickCount64(),!ui && frame.focused);
    if (timing.ready) g_timing_sample=timing;
    ReleaseSRWLockExclusive(&g_session_lock);
    const auto pointer = runtime::SelectUiPointerPose(frame, frame.interact_source);
    g_pointer_valid = ui && frame.focused &&
        pointer.valid && TrackedMenuPointer(pointer.pose, g_pointer_uv);
    if (frame.input.state.recenter.just_pressed) RequestTrackedRecenter();
    if (ui && !g_pointer_valid) {
        frame.input.state.ui_select.pressed = false;
        frame.input.state.ui_select.just_pressed = false;
        frame.input.state.ui_drag.pressed = false;
        frame.input.state.ui_drag.just_pressed = false;
    }
    runtime::VrNativeIntents intents;
    float movement_yaw = 0;
    if (!ui && !TrackedMovementYaw(movement_yaw)) frame.input.state.move = {};
    intents.Begin(frame.input.state, context, movement_yaw);
    auto* previous = g_intents;
    auto* previous_player = g_input_player;
    g_input_player = Read<void*>(handler, 0x38);
    g_player.store(g_input_player, std::memory_order_release);
    g_intents = &intents;
    // Release spatial ownership even when a UI context filters gameplay edges.
    ServiceSpatialInteraction(g_input_player, ui);
    if (turn != 0) {
        auto* player = Read<void*>(handler, 0x38);
        const float sensitivity = Read<float>(player, 0x1E4);
        if (player && std::isfinite(sensitivity) && sensitivity >= 0.001F && sensitivity <= 100)
            reinterpret_cast<Yaw>(g_image + 0x9CD00)(player, turn / sensitivity);
    }
    reinterpret_cast<Update>(g_image + 0x3BF0)(handler, dt);
    ServiceSpatialInteraction(g_input_player, UiContext(handler));
    g_intents = previous;
    g_input_player = previous_player;
    if (consumed_release != 0) static_cast<void>(g_release_pending.compare_exchange_strong(
        consumed_release, 0, std::memory_order_acq_rel));
    // A button can open an inventory/menu during this update. Publish the new
    // context before rendering so its desktop UI is visible in the headset.
    g_ui.store(UiContext(handler), std::memory_order_release);
}
}

runtime::VrUpdateTimingSample ConsumeNativeUpdateTiming() noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    const auto result=g_timing_sample; g_timing_sample={};
    ReleaseSRWLockExclusive(&g_session_lock);
    return result;
}
NativeMovementBoundaryStatus ReadNativeMovementBoundaryStatus() noexcept {
    NativeMovementBoundaryStatus result;
    constexpr std::array<std::uintptr_t, 2> sites{0x51CD, 0x5227};
    constexpr std::array<std::uintptr_t, 2> targets{0x9CBC0, 0x9CC60};
    result.initialized = g_installed.load(std::memory_order_acquire);
    for (std::size_t index = 0; index < sites.size(); ++index) {
        auto& callsite = result.callsites[index];
        callsite.rva = sites[index];
        callsite.expected = CallBytes(sites[index], targets[index]);
        callsite.expected_target = targets[index];
        callsite.owner_installed = g_move_hooks[index].installed();
        if (g_image != nullptr) {
            static_cast<void>(ReadBytes(g_image + sites[index],
                callsite.live.data(), callsite.live.size()));
        }
        callsite.live_target = DecodeCallTarget(callsite.live, sites[index]);
        callsite.owner_matches_live = callsite.owner_installed &&
            callsite.live == g_move_hooks[index].replacement_instruction;
        result.initialized = result.initialized && callsite.owner_matches_live;
    }
    return result;
}
void ConfigureNativeInputBridge(runtime::VrSettings settings) noexcept {
    runtime::NormalizeVrSettings(settings);
    if (!g_installed.load(std::memory_order_acquire)) {
        g_settings = settings;
    }
}
bool InstallNativeInputBridge(std::string& error) noexcept {
    error.clear();
    if (g_update_hook.installed()) return true;
    g_image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    void* update = nullptr;
    if (!ReadBytes(g_image + 0x272A84, &update, sizeof(update)) || update != g_image + 0x3BF0) {
        error = "ButtonHandler Update vtable does not match the exact build"; return false;
    }
    for (const auto& entry : kQueries) {
        const auto expected = CallBytes(entry.site, Target(entry.query));
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + entry.site, actual.data(), actual.size()) || actual != expected) {
            error = "Native input query mismatch at RVA " + std::to_string(entry.site); return false;
        }
    }
    for (const auto& entry : kPointers) {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + entry.site, actual.data(), actual.size()) || actual != CallBytes(entry.site, entry.target)) {
            error = "Native menu pointer call mismatch"; return false;
        }
    }
    constexpr std::array<std::uintptr_t, 2> sites{0x51CD, 0x5227}, targets{0x9CBC0, 0x9CC60};
    for (std::size_t i = 0; i < sites.size(); ++i) {
        std::array<std::uint8_t, 5> actual{};
        if (!ReadBytes(g_image + sites[i], actual.data(), actual.size()) || actual != CallBytes(sites[i], targets[i])) {
            error = "Native movement call mismatch"; return false;
        }
    }
    // Validate direct yaw entry too, before publishing any input hook.
    for (const auto& light : {std::array<std::uintptr_t,2>{0x513D,0x9BCD0},{0x5165,0x9BD80}}) {
        std::array<std::uint8_t,5> actual{};
        if (!ReadBytes(g_image+light[0],actual.data(),actual.size()) || actual!=CallBytes(light[0],light[1])) {
            error="Native light button call mismatch"; return false;
        }
    }
    constexpr std::array<std::uint8_t, 9> yaw{0x56,0x8B,0xF1,0x8B,0x8E,0xC4,0x02,0,0};
    std::array<std::uint8_t, 9> actual_yaw{};
    if (!ReadBytes(g_image + 0x9CD00, actual_yaw.data(), actual_yaw.size()) || actual_yaw != yaw) {
        error = "Native yaw entry mismatch"; return false;
    }
    bool ok = true;
    for (std::size_t i = 0; ok && i < std::size(kQueries); ++i) {
        const auto& entry = kQueries[i];
        ok = hooks::InstallRel32CallHook(g_image + entry.site, CallBytes(entry.site, Target(entry.query)),
                                        reinterpret_cast<void*>(&HookedQuery), g_query_hooks[i], error);
    }
    const std::array<void*, 2> replacements{reinterpret_cast<void*>(&HookedForward), reinterpret_cast<void*>(&HookedSideways)};
    for (std::size_t i = 0; ok && i < sites.size(); ++i)
        ok = hooks::InstallRel32CallHook(g_image + sites[i], CallBytes(sites[i], targets[i]), replacements[i], g_move_hooks[i], error);
    for (std::size_t i = 0; ok && i < std::size(kPointers); ++i) {
        const auto& entry = kPointers[i];
        ok = hooks::InstallRel32CallHook(g_image + entry.site, CallBytes(entry.site, entry.target),
            reinterpret_cast<void*>(&HookedPointer), g_pointer_hooks[i], error);
    }
    if (ok) ok = hooks::InstallPointerHook(reinterpret_cast<void**>(g_image + 0x272A84),
            g_image + 0x3BF0, reinterpret_cast<void*>(&HookedUpdate), g_update_hook, error);
    if (!ok) {
        std::string rollback;
        if (!RemoveNativeInputBridge(rollback)) error += "; rollback failed: " + rollback;
    }
    g_installed.store(ok, std::memory_order_release);
    return ok;
}
bool RemoveNativeInputBridge(std::string& error) noexcept {
    g_installed.store(false, std::memory_order_release);
    g_player.store(nullptr, std::memory_order_release);
    ConnectNativeInput(nullptr);
    // Let a running game consume the pending release in its own update thread.
    // Never invoke player/physics methods from this remote control thread.
    const auto deadline = GetTickCount64() + 1000;
    while (g_release_pending.load(std::memory_order_acquire) && GetTickCount64() < deadline) Sleep(1);
    if (g_release_pending.load(std::memory_order_acquire)) {
        error = "Waiting for the native update thread to release VR input before detaching";
        return false;
    }
    bool ok = hooks::RemoveIatHook(g_update_hook, error);
    for (auto& hook : g_query_hooks) {
        std::string next;
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; error += next; }
    }
    for (auto& hook : g_move_hooks) {
        std::string next;
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; error += next; }
    }
    for (auto& hook : g_pointer_hooks) {
        std::string next;
        if (!hooks::RemoveRel32CallHook(hook, next)) { ok = false; error += next; }
    }
    // DLL and original pointers remain resident for callbacks already in flight.
    return ok;
}
void ConnectNativeInput(runtime::OpenVrSession* session) noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    if (!session && g_session) {
        g_disconnect_release = runtime::MakeReleasedVrInputState(g_frame.input.state);
        g_release_pending.store(++g_release_generation, std::memory_order_release);
    }
    g_session = session; g_frame = {}; g_turn = {};
    ReleaseSRWLockExclusive(&g_session_lock);
}
bool NativeInputUiActive() noexcept {
    return g_installed.load(std::memory_order_acquire) && g_ui.load(std::memory_order_acquire);
}
runtime::VrControllerFrame ReadNativeControllerFrame() noexcept {
    AcquireSRWLockShared(&g_session_lock);
    const auto frame = g_frame;
    ReleaseSRWLockShared(&g_session_lock);
    return frame;
}
void* NativePlayerPointer() noexcept {
    return g_player.load(std::memory_order_acquire);
}
void NativeControllerHaptic(runtime::VrHand hand, bool pickup) noexcept {
    AcquireSRWLockExclusive(&g_session_lock);
    if (g_session) {
        std::string error;
        const auto event = pickup
            ? runtime::VrHapticEvent::object_pickup
            : runtime::VrHapticEvent::object_drop;
        const auto profile = runtime::HapticProfile(event);
        static_cast<void>(g_session->TriggerHaptic(
            hand,
            profile.duration_seconds,
            profile.frequency_hz,
            runtime::ScaleHapticAmplitude(event, 1.0F),
            error));
    }
    ReleaseSRWLockExclusive(&g_session_lock);
}
}
