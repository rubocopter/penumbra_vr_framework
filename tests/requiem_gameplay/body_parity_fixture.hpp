#pragma once

namespace penumbra_vr::backends::requiem {
namespace {
struct BodyParityScene final {
    Vec3 before{}, correction{}, after{};
    bool observe = true;
    bool step = false;
    unsigned updates = 0;
    void* handler = nullptr;
    void* init = nullptr;
    void* player = nullptr;
    void* replacement = nullptr;
    unsigned transition = 0;
    void* death = nullptr;
} body_parity;

bool __fastcall BodyParityCollision(void*, void*, Vec3* resolved, void*,
    const runtime::VrMatrix44* matrix, void*, bool, bool, void*, bool, bool) {
    *resolved = {matrix->values[3] + body_parity.correction.x,
        matrix->values[7] + body_parity.correction.y,
        matrix->values[11] + body_parity.correction.z};
    return true;
}

void __fastcall BodyParityNativeUpdate(void* body, void*, float) {
    ++body_parity.updates;
    const auto& c = GameplayContract();
    auto position = Read<Vec3>(body, c.character_position_offset);
    position.x += body_parity.before.x;
    position.y += body_parity.before.y;
    position.z += body_parity.before.z;
    Vec3 native_step{};
    ApplyQueuedLocomotion(body, &position, &native_step);
    if (body_parity.observe) {
        auto matrix = runtime::IdentityMatrix();
        matrix.values[3] = position.x; matrix.values[7] = position.y;
        matrix.values[11] = position.z;
        using Collision = bool(__thiscall*)(void*, Vec3*, void*,
            const runtime::VrMatrix44*, void*, bool, bool, void*, bool, bool);
        reinterpret_cast<Collision>(g_image + c.collision_callsite_rva - 36)(
            nullptr, &position, nullptr, &matrix, nullptr, false, true, nullptr, true, false);
    } else {
        position.x += body_parity.correction.x;
        position.y += body_parity.correction.y;
        position.z += body_parity.correction.z;
    }
    position.x += body_parity.after.x;
    position.y += body_parity.after.y;
    position.z += body_parity.after.z;
    Put(body, c.character_position_offset, position);
    Put(body, 0x238, body_parity.step); // RQ D826F native step-success flag.
    switch (body_parity.transition) {
    case 1: Put(body_parity.player, c.character_body_offset, body_parity.replacement); break;
    case 2: Put(body_parity.init, 0x15C, body_parity.replacement); break;
    case 3: Put(body_parity.player, kPlayerActiveOffset, false); break;
    case 4: Put(body_parity.handler, 0x3C, 2); break;
    case 5: ConnectGameplayInput(nullptr); break;
    case 6: ++g_tracking_state.body_generation; break;
    case 7: Put(body_parity.death, 0, true); break;
    default: break;
    }
}

struct BodyParityOwner final {
    std::array<std::uint8_t, 0x80> handler{};
    std::array<std::uint8_t, 0x200> init{};
    std::array<std::uint8_t, 0x500> player{}, replacement{};
    std::array<std::uint8_t, 0x300> body{};
    std::array<std::uint8_t, 0x40> death{};
    void Reset() {
        body_parity = {};
        body_parity.handler = handler.data(); body_parity.init = init.data();
        body_parity.player = player.data(); body_parity.replacement = replacement.data();
        body_parity.death = death.data();
        Put(handler.data(), 0x2C, static_cast<void*>(init.data()));
        Put(handler.data(), 0x38, static_cast<void*>(player.data()));
        Put(handler.data(), 0x3C, 1);
        Put(init.data(), 0x15C, static_cast<void*>(player.data()));
        Put(player.data(), kPlayerActiveOffset, true);
        Put(player.data(), 0x290, static_cast<void*>(death.data()));
        Put(death.data(), 0, false);
        Put(player.data(), GameplayContract().character_body_offset, static_cast<void*>(body.data()));
        HookedUpdate(handler.data(), nullptr, 1.0F / 90.0F);
        Put(body.data(), GameplayContract().character_position_offset, Vec3{0, 2, 0});
        Put(body.data(), GameplayContract().character_size_y_offset, 1.8F);
        Put(body.data(), 0x238, false);
        g_tracked_player_body.store(body.data());
        g_tracking_state = {}; g_tracking_state.character_body = body.data();
        g_tracking_state.body_generation = 1;
        g_physical_state = {}; g_physical_tick = {}; g_pending = {};
        g_head_tracking = {}; g_head_tracking.valid = true;
        g_head_tracking.pose.pose_valid = g_head_tracking.pose.device_connected = true;
        g_head_tracking.pose.device_to_absolute.values = {1,0,0,0,0,1,0,1.7F,0,0,1,0};
        g_head_tracking.sampled_at_ms = GetTickCount64();
        HookedCharacterUpdate(body.data(), nullptr, 1.0F / 90.0F);
    }
    void Tick() {
        g_head_tracking.sampled_at_ms = GetTickCount64();
        HookedCharacterUpdate(body.data(), nullptr, 1.0F / 90.0F);
    }
};

bool BodyParityBoundary(std::uint8_t* image) {
    const auto& c = GameplayContract();
    Branch(image, c.button_handler_update_rva, 0xE9, reinterpret_cast<void*>(&HandlerUpdateStub));
    Branch(image, c.character_update_rva, 0xE9, reinterpret_cast<void*>(&BodyParityNativeUpdate));
    Branch(image, c.collision_rva, 0xE9, reinterpret_cast<void*>(&BodyParityCollision));
    // Forward all nine native thiscall stack arguments through the exact
    // horizontal solver callsite; no other collision call is intercepted.
    for (unsigned i = 0; i < 9; ++i) {
        constexpr std::array<std::uint8_t, 4> push{0xFF,0x74,0x24,0x24};
        std::memcpy(image + c.collision_callsite_rva - 36 + i * 4, push.data(), push.size());
    }
    const auto call = CallBytes(c.collision_callsite_rva, c.collision_rva);
    std::memcpy(image + c.collision_callsite_rva, call.data(), call.size());
    image[c.collision_callsite_rva + 5] = 0xC2;
    image[c.collision_callsite_rva + 6] = 36;
    image[c.collision_callsite_rva + 7] = 0;
    std::string error;
    if (!InstallHorizontalCollisionObserver(error)) return false;
    FlushInstructionCache(GetCurrentProcess(), image, 0x300000);
    return true;
}

bool BodyCollisionCarry(std::uint8_t* image) {
    if (!BodyParityBoundary(image)) return false;
    struct Case { const char* name; Vec3 physical, direct, before, correction, after, anchor;
        bool observe = true; bool step = false; };
    // BP's proven attribution cases, exercised through RQ's real observer.
    const Case cases[]{
        {"lateral rejection", {.03F,0,0}, {}, {}, {-.03F,0,.04F}, {}, {}},
        {"backward correction", {.03F,0,0}, {}, {}, {-.05F,0,.01F}, {}, {}},
        {"overshoot", {.03F,0,0}, {}, {}, {.02F,0,.01F}, {}, {.03F,0,0}},
        {"partial slide", {.03F,0,0}, {}, {}, {-.01F,0,.03F}, {}, {.02F,0,0}},
        {"negative Z rejection", {0,0,-.03F}, {}, {}, {.04F,0,.03F}, {}, {}},
        {"unobserved solve", {.03F,0,0}, {}, {}, {-.03F,0,.04F}, {}, {0,0,.04F}, false},
        {"completed native step", {.03F,0,0}, {}, {}, {-.03F,0,0}, {.03F,.02F,.01F},
            {.03F,0,.01F}, true, true},
        {"native before injection", {.03F,0,0}, {}, {.2F,.03F,.1F}, {-.01F,0,.03F}, {},
            {.22F,0,.13F}},
        {"native after rejection", {.03F,0,0}, {}, {}, {-.03F,0,.04F}, {.015F,.02F,.02F},
            {.015F,0,.02F}},
        {"native without intent", {}, {}, {.2F,.03F,.1F}, {}, {.015F,.02F,.02F}, {.215F,0,.12F}},
        {"mixed opposite", {.03F,0,0}, {-.03F,0,0}, {}, {}, {}, {}},
        {"mixed orthogonal", {.03F,0,0}, {0,0,.02F}, {}, {}, {}, {.03F,0,.02F}},
        {"mixed aligned", {.02F,0,0}, {.02F,0,0}, {}, {}, {}, {.04F,0,0}},
    };
    BodyParityOwner owner;
    bool ok = true;
    for (const auto& c : cases) {
        owner.Reset();
        body_parity.before = c.before; body_parity.correction = c.correction;
        body_parity.after = c.after; body_parity.observe = c.observe; body_parity.step = c.step;
        g_head_tracking.pose.device_to_absolute.values[3] = c.physical.x;
        g_head_tracking.pose.device_to_absolute.values[11] = c.physical.z;
        if (std::hypot(c.direct.x, c.direct.z) > 0 &&
            !QueueLocomotionDisplacement(owner.player.data(), {c.direct.x,0,c.direct.z})) return false;
        const auto updates = body_parity.updates;
        owner.Tick();
        const auto sample = ReadGameplayTrackingSample();
        const auto p = Read<Vec3>(owner.body.data(), GameplayContract().character_position_offset);
        if (body_parity.updates != updates + 1 || !sample.room_scale_valid ||
            !Near(sample.head_anchor[0], c.anchor.x) || !Near(sample.head_anchor[2], c.anchor.z) ||
            !Near(sample.head_anchor[1], p.y - .9F) || !Near(sample.body_center_y, p.y)) {
            std::cerr << "Requiem body carry " << c.name << " anchor=" << sample.head_anchor[0]
                << ',' << sample.head_anchor[1] << ',' << sample.head_anchor[2]
                << " expected=" << c.anchor.x << ',' << p.y - .9F << ',' << c.anchor.z << '\n';
            ok = false;
        }
    }
    std::string error;
    return hooks::RemoveRel32CallHook(g_horizontal_collision_hook, error) && ok;
}

bool BodyPostUpdateOwnership(std::uint8_t* image) {
    if (!BodyParityBoundary(image)) return false;
    BodyParityOwner owner;
    bool ok = true;
    for (unsigned transition = 1; transition <= 7; ++transition) {
        owner.Reset();
        if (!ReadGameplayTrackingSample().room_scale_valid) return false;
        body_parity.transition = transition;
        g_head_tracking.pose.device_to_absolute.values[3] = .03F;
        const auto updates = body_parity.updates;
        owner.Tick();
        if (body_parity.updates != updates + 1 || ReadGameplayTrackingSample().room_scale_valid ||
            g_physical_state.initialized || g_physical_tick.body != nullptr || g_pending.active) {
            std::cerr << "Requiem post-update ownership accepted stale transition " << transition << '\n';
            ok = false;
        }
    }
    // Ownership can end between the input owner and entry to native physics,
    // before any post-update guard could run. Cancel the matching queued move
    // before invoking native Update, and do not retain the previous head sample.
    for (unsigned transition = 1; transition <= 7; ++transition) {
        owner.Reset();
        if (!QueueLocomotionDisplacement(owner.player.data(), {.04F,0,0})) return false;
        switch (transition) {
        case 1: Put(owner.player.data(), GameplayContract().character_body_offset,
                    static_cast<void*>(owner.replacement.data())); break;
        case 2: Put(owner.init.data(), 0x15C, static_cast<void*>(owner.replacement.data())); break;
        case 3: Put(owner.player.data(), kPlayerActiveOffset, false); break;
        case 4: Put(owner.handler.data(), 0x3C, 2); break;
        case 5: ConnectGameplayInput(nullptr); break;
        case 6: ++g_tracking_state.body_generation; break;
        case 7: Put(owner.death.data(), 0, true); break;
        }
        const auto updates=body_parity.updates;
        owner.Tick();
        if (body_parity.updates!=updates+1 || ReadGameplayTrackingSample().room_scale_valid ||
            g_physical_state.initialized || g_physical_tick.body || g_pending.active ||
            !Near(Read<Vec3>(owner.body.data(),GameplayContract().character_position_offset).x,0)) {
            std::cerr<<"Requiem pre-update ownership retained stale transition "<<transition<<'\n';
            ok=false;
        }
    }
    owner.Reset();
    if (!QueueLocomotionDisplacement(owner.player.data(), {.04F,0,0})) return false;
    std::array<std::uint8_t,0x300> unrelated{};
    const auto unrelated_updates=body_parity.updates;
    HookedCharacterUpdate(unrelated.data(),nullptr,1.0F/90.0F);
    if (body_parity.updates!=unrelated_updates+1 || !g_pending.active ||
        g_pending.character_body!=owner.body.data() || !g_physical_state.initialized ||
        g_physical_state.body!=owner.body.data() || !ReadGameplayTrackingSample().room_scale_valid) {
        std::cerr<<"Unrelated native character cancelled the player's pending services\n";
        ok=false;
    }
    owner.Tick();
    if (g_pending.active || !Near(Read<Vec3>(owner.body.data(),
            GameplayContract().character_position_offset).x,.04F)) return false;
    std::string error;
    return hooks::RemoveRel32CallHook(g_horizontal_collision_hook, error) && ok;
}
} // namespace
} // namespace penumbra_vr::backends::requiem
