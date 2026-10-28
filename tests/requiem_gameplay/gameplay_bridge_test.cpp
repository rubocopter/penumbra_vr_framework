// Real Requiem queue, body-phase and action-query consumers; no game or HMD.
#include "../../src/backends/requiem/gameplay_bridge.cpp"

#include <iostream>
#include <cstdlib>
#include <thread>

namespace penumbra_vr::backends::requiem {
namespace {
unsigned g_native_updates = 0;
unsigned g_native_queries = 0;
bool g_keyboard_held = false;
float g_collision_fraction = 1.0F;
Vec3 g_scripted_native_travel{};

template<class T> void Put(void* object, std::size_t offset, const T& value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}
void Branch(std::uint8_t* image, std::uintptr_t site, std::uint8_t opcode, void* target) {
    image[site] = opcode;
    const auto relative = static_cast<std::int32_t>(
        reinterpret_cast<std::intptr_t>(target) -
        reinterpret_cast<std::intptr_t>(image + site + 5));
    std::memcpy(image + site + 1, &relative, sizeof(relative));
}
bool __fastcall NativeQueryStub(void*, void*, LegacyString) {
    ++g_native_queries;
    return g_keyboard_held;
}
void __fastcall NativeUpdateStub(void* body, void*, float) {
    ++g_native_updates;
    auto position = Read<Vec3>(body, GameplayContract().character_position_offset);
    const auto before = position;
    Vec3 native_motion{0.0F, 0.125F, 0.0F};
    ApplyQueuedLocomotion(body, &position, &native_motion);
    if (std::hypot(g_physical_tick.physical_request[0], g_physical_tick.physical_request[2]) > 0.050001F)
        std::abort();
    // Stand in for the native horizontal collision solve after injection.
    position.x = before.x + (position.x - before.x) * g_collision_fraction;
    position.z = before.z + (position.z - before.z) * g_collision_fraction;
    if (native_motion.y != 0.125F || position.y != before.y) std::abort();
    position.x+=g_scripted_native_travel.x;
    position.y+=g_scripted_native_travel.y;
    position.z+=g_scripted_native_travel.z;
    Put(body, GameplayContract().character_position_offset, position);
}
bool Near(float actual, float expected) { return std::fabs(actual - expected) < 0.0002F; }

void __fastcall HandlerUpdateStub(void*, void*, float) {}
unsigned g_crouch_transitions=0;
void __fastcall CrouchTransitionStub(void* player,void*,int next,bool) {
    ++g_crouch_transitions;
    Put(player,GameplayContract().move_state_index_offset,next);
}

bool ScriptedPlayerOwnership(std::uint8_t* image) {
    Branch(image,GameplayContract().button_handler_update_rva,0xE9,
        reinterpret_cast<void*>(&HandlerUpdateStub));
    Branch(image,GameplayContract().change_move_state_rva,0xE9,
        reinterpret_cast<void*>(&CrouchTransitionStub));
    std::array<std::uint8_t,0x500> player{},body{};
    std::array<std::uint8_t,0x80> handler{};
    Put(handler.data(),GameplayContract().handler_player_offset,static_cast<void*>(player.data()));
    Put(handler.data(),0x3C,1);
    Put(player.data(),GameplayContract().character_body_offset,static_cast<void*>(body.data()));
    Put(player.data(),GameplayContract().move_state_index_offset,4);
    Put(player.data(),kPlayerActiveOffset,false);
    g_crouch_player=player.data(); g_crouch_body=body.data();
    g_vr_crouch_owned=true; g_crouch_owner_player=player.data();
    g_crouch_transitions=0;
    HookedUpdate(handler.data(),nullptr,1.0F/90.0F);
    if(g_crouch_transitions || Read<int>(player.data(),GameplayContract().move_state_index_offset)!=4) {
        std::cerr<<"VR crouch overwrote script-owned inactive player state\n";
        return false;
    }
    // Script reenables gameplay: release the previously VR-owned stance once.
    Put(player.data(),kPlayerActiveOffset,true);
    HookedUpdate(handler.data(),nullptr,1.0F/90.0F);
    const bool ok=g_crouch_transitions==1 &&
        Read<int>(player.data(),GameplayContract().move_state_index_offset)==0;
    g_vr_crouch_owned=false; g_crouch_owner_player=nullptr;
    return ok;
}

bool KeypadContext(std::uint8_t* image) {
    Branch(image,GameplayContract().button_handler_update_rva,0xE9,
        reinterpret_cast<void*>(&HandlerUpdateStub));
    std::array<std::uint8_t,0x80> handler{},screen{};
    std::array<std::uint8_t,0x200> init{};
    std::array<std::uint8_t,0x100> set{};
    Put(handler.data(),0x2C,static_cast<void*>(init.data()));
    Put(handler.data(),0x3C,1);
    Put(init.data(),0x1AC,static_cast<void*>(screen.data()));
    Put(screen.data(),0x30,static_cast<void*>(set.data()));
    HookedUpdate(handler.data(),nullptr,1.0F/60.0F);
    if(!NativeUiActive() || CurrentNativeUiSurface()!=NativeUiSurface::fullscreen) {
        std::cerr<<"Active native ScreenGui still selects gameplay action context\n";
        return false;
    }
    Put(screen.data(),0x30,static_cast<void*>(nullptr));
    HookedUpdate(handler.data(),nullptr,1.0F/60.0F);
    if(NativeUiActive() || CurrentNativeUiSurface()!=NativeUiSurface::none) return false;
    for(unsigned repeat=0;repeat<3;++repeat) {
        Put(screen.data(),0x30,static_cast<void*>(set.data()));
        HookedUpdate(handler.data(),nullptr,1.0F/60.0F);
        if(!NativeUiActive()) return false;
        // Native Escape close clears screen+30. A subsequent pause/main-menu
        // frame keeps UI context; resuming with no ScreenGui restores gameplay.
        Put(screen.data(),0x30,static_cast<void*>(nullptr));
        Put(handler.data(),0x3C,6);
        HookedUpdate(handler.data(),nullptr,1.0F/60.0F);
        if(!NativeUiActive()) return false;
        Put(handler.data(),0x3C,1);
        HookedUpdate(handler.data(),nullptr,1.0F/60.0F);
        if(NativeUiActive()) return false;
    }
    return true;
}

bool TurnDisconnect(std::uint8_t* image) {
    Branch(image, GameplayContract().button_handler_update_rva, 0xE9,
        reinterpret_cast<void*>(&HandlerUpdateStub));
    const runtime::VrAnalogState right{true, 1.0F, 0.0F};
    const auto update = [&] {
        return g_turn.Update(right, true, runtime::VrTurnMode::snap,
            1.0F / 90.0F, 45.0F, 90.0F, 0.2F);
    };
    g_turn = {};
    static_cast<void>(g_turn.Update({true, 0.0F, 0.0F}, true,
        runtime::VrTurnMode::snap, 1.0F / 90.0F, 45.0F, 90.0F, 0.2F));
    std::thread control([] { ConnectGameplayInput(nullptr); });
    control.join();
    // Disconnect may request a reset, but cannot touch the native owner's
    // armed turn state. This deterministic assertion also catches a reset
    // moved back onto the control thread without relying on a race detector.
    if (!Near(update(), 0.78539816F)) {
        std::cerr << "control-thread disconnect mutated native turn state\n";
        return false;
    }
    HookedUpdate(nullptr, nullptr, 1.0F / 90.0F);
    if (!Near(update(), 0.0F)) {
        std::cerr << "native update failed to consume disconnected turn reset\n";
        return false;
    }
    std::thread repeated([] {
        for (int i = 0; i < 2000; ++i) ConnectGameplayInput(nullptr);
    });
    for (int i = 0; i < 2000; ++i)
        HookedUpdate(nullptr, nullptr, 1.0F / 90.0F);
    repeated.join();
    HookedUpdate(nullptr, nullptr, 1.0F / 90.0F);
    return true;
}

std::array<std::array<std::uint8_t, 0x100>, 2> g_palm_shapes{};
unsigned g_palm_creates = 0;
unsigned g_palm_destroys = 0;
DWORD g_palm_owner = 0;
bool g_palm_destroy_before_detach = true;
bool __fastcall PalmQueryStub(void*, void*, Vec3*, void*,
    const runtime::VrMatrix44*, void*, bool, bool, void*, bool, bool) { return false; }
void* __fastcall PalmCreateStub(void* world, void*, const Vec3* size,
    runtime::VrMatrix44*) {
    if (g_palm_creates >= g_palm_shapes.size()) return nullptr;
    auto* shape = g_palm_shapes[g_palm_creates++].data();
    Put(shape, 0, g_image + 0x293F10);
    Put(shape, 4, *size);
    Put(shape, 0x10, std::int32_t{1});
    Put(shape, 0x58, world);
    return shape;
}
void __fastcall PalmDestroyStub(void*, void*, void*) {
    ++g_palm_destroys;
    g_palm_destroy_before_detach = g_palm_destroy_before_detach &&
        g_update_hook.installed() && GetCurrentThreadId() == g_palm_owner;
}

bool BridgeShutdown(std::uint8_t* image, bool timeout, bool body_tick) {
    Branch(image, 0x18B3B0, 0xE9, reinterpret_cast<void*>(&PalmCreateStub));
    Branch(image, 0xD4B70, 0xE9, reinterpret_cast<void*>(&PalmDestroyStub));
    Branch(image, 0xD5190, 0xE9, reinterpret_cast<void*>(&PalmQueryStub));
    Branch(image, GameplayContract().button_handler_update_rva, 0xE9,
        reinterpret_cast<void*>(&HandlerUpdateStub));
    Branch(image, GameplayContract().character_update_rva, 0xE9,
        reinterpret_cast<void*>(&HandlerUpdateStub));
    std::array<std::uint8_t, 0x300> character{};
    std::array<std::uint8_t, 0x400> physics{};
    std::array<std::uint8_t, 0x100> world{};
    Put(world.data(), 0, image + 0x292D50);
    Put(image, 0x292D50 + 0x30, image + 0x18B3B0);
    Put(physics.data(), 0, image + 0x293DD8);
    Put(character.data(), 0x23C, static_cast<void*>(physics.data()));
    Put(character.data(), 0x240, static_cast<void*>(world.data()));
    const auto pose = runtime::IdentityMatrix();
    PublishGameplayPalmTracking({pose, pose}, {true, true}, pose, true, 1);
    g_palm_owner = GetCurrentThreadId();
    ServiceGameplayPalmResolver(image, character.data());
    if (g_palm_creates != 2) return false;
    auto** slot = reinterpret_cast<void**>(
        image + GameplayContract().button_handler_vtable_entry_rva);
    *slot = image + GameplayContract().button_handler_update_rva;
    std::string error;
    if (!hooks::InstallPointerHook(slot, *slot, reinterpret_cast<void*>(&HookedUpdate),
            g_update_hook, error)) return false;
    // Only one owner installed: partial setup must be removable too.
    std::atomic<bool> done{false};
    bool removed = true;
    std::thread control([&] {
        removed = RemoveGameplayBridge(error);
        done.store(true, std::memory_order_release);
    });
    if (!timeout) {
        while (!done.load(std::memory_order_acquire)) {
            if (body_tick) HookedCharacterUpdate(character.data(), nullptr, 0.01F);
            else HookedUpdate(nullptr, nullptr, 0.01F); // No game/player loaded.
            Sleep(1);
        }
    }
    control.join();
    if (timeout) {
        if (removed || error.empty() || !g_update_hook.installed() ||
            g_palm_destroys != 0) {
            std::cerr << "timed out bridge shutdown detached its native owner\n";
            return false;
        }
        HookedUpdate(nullptr, nullptr, 0.01F);
        removed = RemoveGameplayBridge(error);
    }
    if (!removed || !error.empty() || g_update_hook.installed() ||
        g_palm_destroys != 2 || !g_palm_destroy_before_detach) {
        std::cerr << "bridge detached before native palm destruction: " << error << '\n';
        return false;
    }
    PublishGameplayPalmTracking({pose, pose}, {true, true}, pose, true, 2);
    ServiceGameplayPalmResolver(image, character.data());
    return RemoveGameplayBridge(error) && g_palm_creates == 2 && g_palm_destroys == 2;
}

bool JumpQueries(std::uint8_t* image) {
    Branch(image, 0xDAE70, 0xE9, reinterpret_cast<void*>(&NativeQueryStub));
    Branch(image, 0xDAFB0, 0xE9, reinterpret_cast<void*>(&NativeQueryStub));
    // These independently evidenced native sites must both be owned by the
    // production query table. Leave an omitted site on its native target.
    for (const auto site : {0x5297U, 0x56BFU}) {
        image[site - 3] = 0x83; image[site - 2] = 0xEC; image[site - 1] = 28;
        Branch(image, site, 0xE8, image + 0xDAE70);
        image[site + 5] = 0xC2; image[site + 6] = 28; image[site + 7] = 0;
        for (const auto& entry : kGameplayQueries) {
            if (entry.site == site) Branch(image, site, 0xE8, reinterpret_cast<void*>(&HookedQuery));
        }
    }
    FlushInstructionCache(GetCurrentProcess(), image, 0x300000);
    runtime::VrNativeIntents intents;
    runtime::VrInputState state;
    std::array<std::uint8_t,0x300> player{};
    Put(player.data(),0x1DC,true);
    g_current_player=player.data();
    state.jump = {true, true, true, false};
    g_intents = &intents;
    for (const auto context : {runtime::VrInputContext::gameplay, runtime::VrInputContext::ui}) {
        for (const bool held : {true, false}) {
            state.jump.pressed = held;
            intents.Begin(state, context);
            for (int repeat = 0; repeat < 4; ++repeat) {
                for (const auto site : {0x5297U, 0x56BFU}) {
                    const auto query = reinterpret_cast<Query>(image + site - 3);
                    const bool actual = query(nullptr, LegacyString{});
                    const bool expected = held && context == runtime::VrInputContext::gameplay;
                    if (actual != expected) {
                        std::cerr << "Jump held query RVA " << std::hex << site << " lost VR-only hold\n";
                        return false;
                    }
                }
            }
        }
    }
    state.jump.pressed=true;
    intents.Begin(state,runtime::VrInputContext::gameplay);
    for (const bool player_missing : {false,true}) {
        Put(player.data(),0x1DC,false);
        g_current_player=player_missing?nullptr:player.data();
        for (const auto site : {0x5297U,0x56BFU}) {
            const auto query=reinterpret_cast<Query>(image+site-3);
            if(query(nullptr,LegacyString{})) {
                std::cerr<<"Inactive/missing Requiem player retained VR jump hold\n";
                return false;
            }
        }
    }
    // Focus/input loss publishes an empty input state; neither site may keep
    // the previous hold. After callback teardown there are no VR intents.
    Put(player.data(),0x1DC,true);
    g_current_player=player.data();
    intents.Begin({}, runtime::VrInputContext::gameplay);
    for (const bool native : {false, true}) {
        g_keyboard_held = native;
        for (const auto site : {0x5297U, 0x56BFU}) {
            const auto query = reinterpret_cast<Query>(image + site - 3);
            if (query(nullptr, LegacyString{}) != native) return false;
        }
    }
    g_intents = nullptr;
    g_keyboard_held = false;
    for (const auto site : {0x5297U, 0x56BFU}) {
        const auto query = reinterpret_cast<Query>(image + site - 3);
        if (query(nullptr, LegacyString{})) return false;
    }
    return g_native_queries == 42;
}

bool CreditsQueries(std::uint8_t* image) {
    Branch(image, GameplayContract().button_handler_update_rva, 0xE9,
        reinterpret_cast<void*>(&HandlerUpdateStub));
    std::array<std::uint8_t, 0x80> handler{};
    Put(handler.data(), 0x3C, 5); // Native credits/story handler state.
    HookedUpdate(handler.data(), nullptr, 1.0F / 60.0F);
    if (!NativeUiActive() || CurrentNativeUiSurface() != NativeUiSurface::fullscreen)
        return false;

    Branch(image, 0xDAFB0, 0xE9, reinterpret_cast<void*>(&NativeQueryStub));
    constexpr std::array<unsigned, 3> sites{0x3F69, 0x3F97, 0x3FC7};
    for (const auto site : sites) {
        image[site - 3] = 0x83; image[site - 2] = 0xEC; image[site - 1] = 28;
        Branch(image, site, 0xE8, image + 0xDAFB0);
        image[site + 5] = 0xC2; image[site + 6] = 28; image[site + 7] = 0;
        // Keep an omitted production callsite native, so this fixture detects
        // a missing mapping rather than installing the expected hook itself.
        for (const auto& entry : kGameplayQueries) {
            if (entry.site == site)
                Branch(image, site, 0xE8, reinterpret_cast<void*>(&HookedQuery));
        }
    }
    FlushInstructionCache(GetCurrentProcess(), image, 0x300000);
    runtime::VrNativeIntents intents;
    g_intents = &intents;
    const auto query = [&](unsigned site, bool expected) {
        const auto before = g_native_queries;
        const bool actual = reinterpret_cast<Query>(image + site - 3)(nullptr, LegacyString{});
        if (actual != expected || g_native_queries != before + 1) {
            std::cerr << "Credits query RVA " << std::hex << site
                << " got=" << actual << " expected=" << expected
                << " native_queries=" << std::dec << g_native_queries - before << '\n';
            return false;
        }
        return true;
    };
    // Independent native routes: Escape -> Pause/UI-close, LeftClick ->
    // Select, RightClick -> Back. None requires an active gameplay player.
    using Button = runtime::VrButtonState runtime::VrInputState::*;
    constexpr std::array<Button, 4> buttons{
        &runtime::VrInputState::ui_close, &runtime::VrInputState::pause,
        &runtime::VrInputState::ui_select, &runtime::VrInputState::ui_back};
    constexpr std::array<unsigned, 4> winners{0x3F69, 0x3F69, 0x3F97, 0x3FC7};
    g_keyboard_held = false;
    for (std::size_t index = 0; index < buttons.size(); ++index) {
        runtime::VrInputState input{};
        input.*buttons[index] = {true, true, true, false};
        intents.Begin(input, runtime::VrInputContext::ui);
        for (const auto site : sites) {
            if (!query(site, site == winners[index]) || !query(site, false)) return false;
        }
        input.*buttons[index] = {true, true, false, false}; // Still held next frame.
        intents.Begin(input, runtime::VrInputContext::ui);
        for (const auto site : sites) if (!query(site, false)) return false;
        input.*buttons[index] = {true, false, false, true};
        intents.Begin(input, runtime::VrInputContext::ui);
        for (const auto site : sites) if (!query(site, false)) return false;
    }
    // Closing must not consume the independent right-click/back edge.
    runtime::VrInputState input{};
    input.ui_close = input.ui_back = {true, true, true, false};
    intents.Begin(input, runtime::VrInputContext::ui);
    if (!query(0x3F69, true) || !query(0x3FC7, true) ||
        !query(0x3F69, false) || !query(0x3FC7, false)) return false;
    intents.Begin({}, runtime::VrInputContext::ui); // Focus/input loss.
    for (const auto site : sites) if (!query(site, false)) return false;
    for (const bool attached : {true, false}) {
        g_intents = attached ? &intents : nullptr;
        for (const bool native : {true, false}) {
            g_keyboard_held = native;
            for (const auto site : sites) if (!query(site, native)) return false;
        }
    }
    g_intents = nullptr;
    g_keyboard_held = false;
    return true;
}

bool NativeCarry(std::uint8_t* image) {
    // Exercise the real post-native body observer with travel independent of
    // VR intent. This checks ownership/carry, not an actual platform contact.
    Branch(image,GameplayContract().character_update_rva,0xE9,
        reinterpret_cast<void*>(&NativeUpdateStub));
    std::array<std::uint8_t,0x300> body{};
    Put(body.data(),GameplayContract().character_size_y_offset,1.8F);
    Put(body.data(),GameplayContract().character_position_offset,Vec3{0,2,0});
    g_tracked_player_body.store(body.data());
    g_tracking_state={}; g_tracking_state.character_body=body.data();
    g_tracking_state.body_generation=1; g_physical_state={};
    g_head_tracking={}; g_head_tracking.valid=true;
    g_head_tracking.pose.pose_valid=g_head_tracking.pose.device_connected=true;
    g_head_tracking.pose.device_to_absolute.values={1,0,0,0,0,1,0,1.7F,0,0,1,0};
    g_collision_fraction=1;
    for(int phase=0;phase<3;++phase) {
        g_scripted_native_travel=phase==0 ? Vec3{} :
            phase==1 ? Vec3{0.01F,0.02F,-0.015F} : Vec3{-0.01F,-0.02F,0.015F};
        for(int tick=0;tick<30;++tick) {
            g_head_tracking.sampled_at_ms=GetTickCount64();
            const auto updates=g_native_updates;
            HookedCharacterUpdate(body.data(),nullptr,1.0F/90.0F);
            const auto p=Read<Vec3>(body.data(),GameplayContract().character_position_offset);
            if(g_native_updates!=updates+1 || !g_tracking_state.room_scale_valid ||
                !Near(g_tracking_state.head_anchor[0],p.x) ||
                !Near(g_tracking_state.head_anchor[1],p.y-0.9F) ||
                !Near(g_tracking_state.head_anchor[2],p.z)) {
                std::cerr<<"Native scripted travel lost/doubled in VR head carry\n";
                return false;
            }
        }
    }
    g_scripted_native_travel={};
    return true;
}

bool Movement(std::uint8_t* image) {
    Branch(image, GameplayContract().character_update_rva, 0xE9,
        reinterpret_cast<void*>(&NativeUpdateStub));
    std::array<std::uint8_t, 0x300> player{}, body{};
    Put(player.data(), GameplayContract().character_body_offset, static_cast<void*>(body.data()));
    Put(body.data(), GameplayContract().character_size_y_offset, 1.8F);
    g_tracked_player_body.store(body.data());
    for (const int hz : {30, 60, 72, 90, 120}) {
        for (const bool sprint : {false, true}) {
            for (const bool mixed : {false, true}) {
                for (const float fraction : {1.0F, 0.5F}) {
                    g_collision_fraction = fraction;
                    Put(body.data(), GameplayContract().character_position_offset, Vec3{0, 2, 0});
                    g_physical_state = {};
                    g_tracking_state = {};
                    g_tracking_state.character_body = body.data();
                    g_tracking_state.body_generation = 1;
                    g_head_tracking = {};
                    g_head_tracking.valid = true;
                    g_head_tracking.pose.pose_valid = true;
                    g_head_tracking.pose.device_connected = true;
                    g_head_tracking.pose.device_to_absolute.values = {1,0,0,0, 0,1,0,1.7F, 0,0,1,0};
                    g_head_tracking.sampled_at_ms = GetTickCount64();
                    HookedCharacterUpdate(body.data(), nullptr, 1.0F / static_cast<float>(hz)); // Establish tracking origin.
                    const auto updates_before = g_native_updates;
                    g_direct_head_world_pose = runtime::IdentityMatrix();
                    g_direct_move = {true, 0, 1};
                    g_native_axis_observed = false;
                    g_forward_allowed = true;
                    g_sideways_allowed = true;
                    g_direct_sprinting = sprint;
                    for (int tick = 0; tick < hz; ++tick) {
                        g_head_tracking.sampled_at_ms = GetTickCount64();
                        // Move tracking along an independent horizontal axis;
                        // the room-scale request stays bounded at 5 cm.
                        if (mixed) g_head_tracking.pose.device_to_absolute.values[3] += 0.01F;
                        PublishDirectLocomotion(player.data(), 1.0F / static_cast<float>(hz));
                        PublishDirectLocomotion(player.data(), 1.0F / static_cast<float>(hz)); // Latest request, never double travel.
                        HookedCharacterUpdate(body.data(), nullptr, 1.0F / static_cast<float>(hz));
                        const auto position = Read<Vec3>(body.data(), GameplayContract().character_position_offset);
                        auto duplicate = position;
                        ApplyQueuedLocomotion(body.data(), &duplicate, nullptr);
                        if (!Near(position.x, duplicate.x) || !Near(position.z, duplicate.z) ||
                            !Near(position.y, 2.0F)) return false;
                    }
                    const auto position = Read<Vec3>(body.data(), GameplayContract().character_position_offset);
                    const float expected = (sprint ? -4.5F : -3.0F) * fraction;
                    if (!Near(position.z, expected) || g_native_updates != updates_before + hz) {
                        std::cerr << "Requiem queue/consumer hz=" << hz << " sprint=" << sprint
                            << " mixed=" << mixed << " got=" << position.z << " expected=" << expected << '\n';
                        return false;
                    }
                }
            }
        }
    }
    return true;
}
} // namespace
} // namespace penumbra_vr::backends::requiem

#include "keypad_fixture.hpp"
int main(int argc, char** argv) {
    using namespace penumbra_vr::backends::requiem;
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x300000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!image) return 2;
    g_image = image;
    const bool result = argc > 1 && std::strcmp(argv[1], "keypad") == 0
        ? KeypadContext(image) && KeypadPointerAndQueries(image)
        : argc > 1 && std::strcmp(argv[1], "turn") == 0
        ? TurnDisconnect(image)
        : argc > 1 && std::strncmp(argv[1], "shutdown", 8) == 0
            ? BridgeShutdown(image, std::strcmp(argv[1], "shutdown-timeout") == 0,
                std::strcmp(argv[1], "shutdown-body") == 0)
        : argc > 1 && std::strcmp(argv[1], "credits") == 0
            ? CreditsQueries(image)
        : argc > 1 && std::strcmp(argv[1], "jump") == 0
            ? JumpQueries(image) : argc>1 && std::strcmp(argv[1],"native-carry")==0
            ? NativeCarry(image) : argc>1 && std::strcmp(argv[1],"scripted-player")==0
            ? ScriptedPlayerOwnership(image) : Movement(image);
    VirtualFree(image, 0, MEM_RELEASE);
    return result ? 0 : 1;
}
