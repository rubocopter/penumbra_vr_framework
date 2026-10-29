#pragma once

// Requiem cGui boundary, independently decoded from the offline image
// 989726EE2780AC890486DFCA0AB6B80AF8E97BC4FF64AF5A28CF1F23ACE2CA5B:
// 46DC -> E3120 (absolute/relative); 4626/466B query left down/up;
// 4633 -> E3170 and 4678 -> E31C0 dispatch button 1 to gui+30;
// F905D/F9066 store the focused set's cursor at D8/DC. Native F91F0/F92C0
// retain widget attention/focus. Only these native recipients are fixtures;
// the production pointer, query table, rel32 hooks and input policy run here.
// This protects the existing v1.0.13 desktop route, not a new cursor algorithm.
namespace penumbra_vr::backends::requiem {
namespace {

struct ComputerWidgets final {
    void* set = nullptr;
    bool attention = true;
    bool hovered = false;
    bool pressed = false;
    unsigned moves = 0;
    unsigned accepts = 0;
    unsigned background_clicks = 0;
    std::array<float, 2> delta{};
} computer_widgets;

bool __fastcall ComputerMousePosition(void* gui, void*,
    const std::array<float, 2>* absolute,
    const std::array<float, 2>* relative) {
    auto* const focused = Read<void*>(gui, 0x30);
    if (!focused) return false;
    Put(focused, 0xD8, *absolute);
    ++computer_widgets.moves;
    computer_widgets.delta = *relative;
    // Model native hover/attention without assigning guessed widget layouts.
    computer_widgets.hovered = focused == computer_widgets.set &&
        Near((*absolute)[0], 480) && Near((*absolute)[1], 360);
    return true;
}

bool __fastcall ComputerMouseDown(void* gui, void*, int button) {
    if (button != 1 || Read<void*>(gui, 0x30) != computer_widgets.set)
        return false;
    computer_widgets.pressed = computer_widgets.hovered;
    return computer_widgets.pressed;
}

bool __fastcall ComputerMouseUp(void* gui, void*, int button) {
    if (button != 1 || Read<void*>(gui, 0x30) != computer_widgets.set)
        return false;
    if (computer_widgets.pressed && computer_widgets.hovered) {
        if (computer_widgets.attention) ++computer_widgets.accepts;
        else ++computer_widgets.background_clicks;
    }
    computer_widgets.pressed = false;
    return true;
}

bool ComputerPointerAndAccept(std::uint8_t* image,
    bool install_pointer = true, bool install_release = true) {
    const auto fail = [](const char* reason) {
        std::cerr << "Requiem computer: " << reason << '\n';
        return false;
    };
    std::array<std::uint8_t, 0x80> gui{};
    std::array<std::uint8_t, 0x120> desktop{}, other_set{};
    Put(gui.data(), 0x30, static_cast<void*>(desktop.data()));
    Put(desktop.data(), 0xD8, std::array<float, 2>{400, 300});
    Put(other_set.data(), 0xD8, std::array<float, 2>{400, 300});
    computer_widgets = {};
    computer_widgets.set = desktop.data();

    // Pin the independently decoded RQ route. State 8 / Schmup's relative
    // pointer is deliberately absent; it cannot operate a ScreenGui popup.
    static_assert(kGuiPointerSite == 0x46DC && kGuiSendMousePos == 0xE3120,
        "Requiem desktop boundary drifted");
    Branch(image, 0xE3120, 0xE9, reinterpret_cast<void*>(&ComputerMousePosition));
    Branch(image, 0xE3170, 0xE9, reinterpret_cast<void*>(&ComputerMouseDown));
    Branch(image, 0xE31C0, 0xE9, reinterpret_cast<void*>(&ComputerMouseUp));
    Branch(image, 0xDAFB0, 0xE9, reinterpret_cast<void*>(&NativeQueryStub));
    Branch(image, 0xDAF10, 0xE9, reinterpret_cast<void*>(&NativeQueryStub));

    // Real thiscall argument forwarding through the owned rel32 callsite.
    for (unsigned i = 0; i < 2; ++i) {
        constexpr std::array<std::uint8_t, 4> push{0xFF, 0x74, 0x24, 0x08};
        std::memcpy(image + 0x46DC - 8 + i * 4, push.data(), push.size());
    }
    const auto native_pointer = CallBytes(0x46DC, 0xE3120);
    std::memcpy(image + 0x46DC, native_pointer.data(), native_pointer.size());
    image[0x46E1] = 0xC2; image[0x46E2] = 8; image[0x46E3] = 0;
    std::string error;
    if (install_pointer && !hooks::InstallRel32CallHook(image + 0x46DC,
            native_pointer, reinterpret_cast<void*>(&HookedGuiPointer),
            g_gui_pointer_hook, error)) return fail(error.c_str());

    std::array<hooks::Rel32CallHook, 2> query_hooks{};
    constexpr std::array<std::uintptr_t, 2> sites{0x4626, 0x466B};
    constexpr std::array<std::uintptr_t, 2> targets{0xDAFB0, 0xDAF10};
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const auto site = sites[i];
        image[site - 3] = 0x83; image[site - 2] = 0xEC; image[site - 1] = 28;
        const auto call = CallBytes(site, targets[i]);
        std::memcpy(image + site, call.data(), call.size());
        image[site + 5] = 0xC2; image[site + 6] = 28; image[site + 7] = 0;
        for (const auto& entry : kGameplayQueries) {
            if (entry.site == site && (i == 0 || install_release)) {
                if (entry.target != targets[i] || entry.action != NativeAction::select ||
                    entry.query != (i == 0 ? NativeQuery::pressed : NativeQuery::released))
                    return fail("native left-button mapping drifted");
                if (!hooks::InstallRel32CallHook(image + site, call,
                        reinterpret_cast<void*>(&HookedQuery), query_hooks[i], error))
                    return fail(error.c_str());
            }
        }
    }
    FlushInstructionCache(GetCurrentProcess(), image, 0x300000);
    using Pointer = bool(__thiscall*)(void*, const std::array<float, 2>*,
        const std::array<float, 2>*);
    using Button = bool(__thiscall*)(void*, int);
    const auto pointer = reinterpret_cast<Pointer>(image + 0x46DC - 8);
    const auto query = [&](std::uintptr_t site) {
        return reinterpret_cast<Query>(image + site - 3)(nullptr, LegacyString{});
    };
    const auto dispatch = [&] {
        if (query(0x4626))
            reinterpret_cast<Button>(image + 0xE3170)(gui.data(), 1);
        if (query(0x466B))
            reinterpret_cast<Button>(image + 0xE31C0)(gui.data(), 1);
    };
    runtime::VrNativeIntents intents;
    g_intents = &intents;
    g_pointer_valid = true;
    g_pointer_uv = {0.75F, 0.75F};
    g_mouse_override_until = 0;
    g_keyboard_held = false;
    const std::array<float, 2> physical{20, 30}, zero{};
    if (!pointer(gui.data(), &physical, &zero) || !computer_widgets.hovered ||
        !Near(computer_widgets.delta[0], 80) || !Near(computer_widgets.delta[1], 60))
        return fail("tracked cursor did not reach the attention popup through native cGui");

    runtime::VrInputState input{};
    input.ui_select = {true, true, true, false};
    intents.Begin(input, runtime::VrInputContext::ui);
    dispatch(); dispatch();
    if (!computer_widgets.pressed || computer_widgets.accepts != 0)
        return fail("Accept must remain native mouse-up owned");
    input.ui_select = {true, false, false, true};
    intents.Begin(input, runtime::VrInputContext::ui);
    dispatch(); dispatch();
    if (computer_widgets.pressed || computer_widgets.accepts != 1 ||
        computer_widgets.background_clicks != 0 || !computer_widgets.attention)
        return fail("native popup Accept did not complete exactly once on VR release");

    // Focused GuiSet, not ScreenGui's original desktop, owns cursor updates.
    Put(gui.data(), 0x30, static_cast<void*>(other_set.data()));
    if (!pointer(gui.data(), &physical, &zero) ||
        !Near(Read<float>(other_set.data(), 0xD8), 480) ||
        !Near(Read<float>(other_set.data(), 0xDC), 360) ||
        !Near(Read<float>(desktop.data(), 0xD8), 480) ||
        !Near(Read<float>(desktop.data(), 0xDC), 360))
        return fail("focused GuiSet was bypassed");
    Put(gui.data(), 0x30, static_cast<void*>(desktop.data()));
    const std::array<float, 2> moved{2, 3};
    if (!pointer(gui.data(), &physical, &moved) || computer_widgets.hovered ||
        computer_widgets.delta != moved) return fail("physical mouse priority was lost");
    if (!pointer(gui.data(), &physical, &zero) || computer_widgets.hovered)
        return fail("physical mouse override expired before its native interval");
    g_mouse_override_until = 0;
    g_pointer_valid = false;
    if (!pointer(gui.data(), &physical, &zero) || computer_widgets.hovered)
        return fail("pointer injected without tracking");
    Put(gui.data(), 0x30, static_cast<void*>(nullptr));
    g_pointer_valid = true;
    if (pointer(gui.data(), &physical, &zero)) return fail("missing focus was bypassed");

    // No gameplay trigger may become a computer click.
    input = {}; input.interact = {true, true, true, false};
    intents.Begin(input, runtime::VrInputContext::gameplay);
    if (query(0x4626) || query(0x466B)) return fail("gameplay input leaked into the desktop");
    g_intents = nullptr;
    g_pointer_valid = false;
    for (auto& hook : query_hooks) {
        if (hook.installed() && !hooks::RemoveRel32CallHook(hook, error))
            return fail(error.c_str());
    }
    if (!RemoveHookSet(error) || g_gui_pointer_hook.installed() ||
        !std::equal(native_pointer.begin(), native_pointer.end(), image + 0x46DC))
        return fail("desktop hook teardown did not restore the native call");
    return true;
}
} // namespace
} // namespace penumbra_vr::backends::requiem
