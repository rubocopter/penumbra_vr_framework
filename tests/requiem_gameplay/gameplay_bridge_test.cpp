// Real Requiem queue, body-phase and action-query consumers; no game or HMD.
#include "../../src/backends/requiem/gameplay_bridge.cpp"

#include <iostream>
#include <cstdlib>

namespace penumbra_vr::backends::requiem {
namespace {
unsigned g_native_updates = 0;
unsigned g_native_queries = 0;
bool g_keyboard_held = false;
float g_collision_fraction = 1.0F;

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
    Put(body, GameplayContract().character_position_offset, position);
}
bool Near(float actual, float expected) { return std::fabs(actual - expected) < 0.0002F; }

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

int main(int argc, char** argv) {
    using namespace penumbra_vr::backends::requiem;
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x300000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!image) return 2;
    g_image = image;
    const bool result = argc > 1 && std::strcmp(argv[1], "jump") == 0
        ? JumpQueries(image) : Movement(image);
    VirtualFree(image, 0, MEM_RELEASE);
    return result ? 0 : 1;
}
