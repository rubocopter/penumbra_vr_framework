#include "vr_native_intents.hpp"
#include "legacy_input_abi.hpp"
#include "iat_hook.hpp"
#include <cmath>
#include <iostream>
#include <limits>

using namespace penumbra_vr::runtime;
using A = NativeVrAction;
using Q = NativeVrQuery;
using Legacy = penumbra_vr::adapters::hpl1::LegacyInputString;
using LegacyQuery = penumbra_vr::adapters::hpl1::LegacyInputQuery;
namespace {
int queries = 0;
struct Fixture {
    unsigned int value = 0x1234;
    __declspec(noinline) bool Query(Legacy name) {
        ++queries;
        return value == name.storage[0] && name.storage[6] == 0xABCDEF;
    }
};
__declspec(noinline) bool __fastcall Bridge(void* input, void*, Legacy name) {
    return static_cast<Fixture*>(input)->Query(name);
}
}
int main() {
    VrNativeIntents intents;
    VrInputState state;
    state.interact = {true, true, true, false};
    state.move = {true, 0.5F, 0.8F};
    const auto released = MakeReleasedVrInputState(state);
    if (!released.interact.just_released || released.interact.pressed || released.interact.just_pressed ||
        released.move.active || released.move.x != 0) return 11;
    intents.Begin(state, VrInputContext::gameplay);
    if (!intents.Query(A::interact,Q::pressed) || intents.Query(A::interact,Q::pressed) ||
        !intents.Query(A::interact,Q::held) || !intents.Query(A::interact,Q::held) ||
        intents.Move(0.5F,false) != 1.0F || intents.Move(-0.5F,true) != 0) return 1;
    state.interact = {false,false,false,true};
    intents.Begin(state, VrInputContext::gameplay);
    if (!intents.Query(A::interact,Q::released) || intents.Query(A::interact,Q::released) ||
        intents.Query(A::interact,Q::held)) return 2;
    state.ui_select = {true,true,true,false}; state.ui_close = {true,true,true,false};
    intents.Begin(state, VrInputContext::ui);
    if (intents.Move(0,false) != 0 || intents.Query(A::interact,Q::released) ||
        !intents.Query(A::select,Q::pressed) || !intents.Query(A::pause,Q::pressed) ||
        intents.Query(A::select,Q::pressed) || intents.Query(A::count,Q::pressed)) return 3;
    state.move.x = std::numeric_limits<float>::quiet_NaN();
    intents.Begin(state, VrInputContext::gameplay);
    if (intents.Move(0.25F,true) != 0.25F) return 4;
    VrSnapTurn turn;
    if (turn.Update({true,1,0},true) != 0 || turn.Update({true,0,0},true) != 0 ||
        std::abs(turn.Update({true,1,0},true) - 0.785398163F) > 0.00001F ||
        turn.Update({true,1,0},true) != 0 || turn.Update({true,0,0},false) != 0 ||
        turn.Update({true,-1,0},true) != 0) return 5;
    static_cast<void>(turn.Update({true,0,0},true));
    if (turn.Update({true,-1,0},true) >= 0) return 6;
    // Exercise the exact x86 thiscall -> fastcall aggregate forwarding ABI.
    // Repetition also catches incorrect callee stack cleanup in Debug and /O2.
    Fixture fixture;
    Legacy name{}; name.storage[0] = 0x1234; name.storage[6] = 0xABCDEF;
    auto bridge = reinterpret_cast<LegacyQuery>(&Bridge);
    for (int i = 0; i < 10000; ++i) if (!bridge(&fixture,name)) return 7;
    if (queries != 10000 || fixture.value != 0x1234) return 8;
    void* slot = reinterpret_cast<void*>(&Bridge);
    penumbra_vr::hooks::IatHook hook;
    std::string error;
    auto* other = reinterpret_cast<void*>(&main);
    if (penumbra_vr::hooks::InstallPointerHook(&slot, other, other, hook, error) ||
        hook.installed() || slot != reinterpret_cast<void*>(&Bridge)) return 9;
    if (!penumbra_vr::hooks::InstallPointerHook(&slot, slot, other, hook, error) || slot != other ||
        !penumbra_vr::hooks::RemoveIatHook(hook,error) || slot != reinterpret_cast<void*>(&Bridge)) return 10;
    std::cout << "Native intents, neutral snap-turn, pointer guard and legacy x86 ABI passed\n";
}
