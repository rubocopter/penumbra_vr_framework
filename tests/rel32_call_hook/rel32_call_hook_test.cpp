#include "rel32_call_hook.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {

struct FakeRenderer {
    int original_calls = 0;
    void* last_world = nullptr;
    void* last_camera = nullptr;
    float last_frame_time = 0.0F;

    __declspec(noinline) void RenderWorld(void* world, void* camera, float frame_time) {
        ++original_calls;
        last_world = world;
        last_camera = camera;
        last_frame_time = frame_time;
    }
};

penumbra_vr::hooks::Rel32CallHook g_hook;
int g_hook_calls = 0;

using RenderWorldFunction = void(__thiscall*)(FakeRenderer*, void*, void*, float);

void __fastcall HookedRenderWorld(
    FakeRenderer* renderer,
    void*,
    void* world,
    void* camera,
    float frame_time) {
    ++g_hook_calls;
    reinterpret_cast<RenderWorldFunction>(g_hook.original_target)(
        renderer, world, camera, frame_time);
}

__declspec(noinline) void CallRenderWorld(
    FakeRenderer* renderer,
    void* world,
    void* camera,
    float frame_time) {
    renderer->RenderWorld(world, camera, frame_time);
}

std::uint8_t* FindDirectCall(std::uint8_t* function, std::size_t search_size) {
    for (std::size_t offset = 0; offset + 5 <= search_size; ++offset) {
        if (function[offset] == 0xE8) {
            return function + offset;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    FakeRenderer renderer;
    auto* world = reinterpret_cast<void*>(0x1234);
    auto* camera = reinterpret_cast<void*>(0x5678);
    CallRenderWorld(&renderer, world, camera, 0.25F);
    if (renderer.original_calls != 1) {
        std::cerr << "The unhooked test call did not reach the original target\n";
        return 1;
    }

    auto* call = FindDirectCall(
        reinterpret_cast<std::uint8_t*>(&CallRenderWorld),
        64);
    if (call == nullptr) {
        std::cerr << "Could not find the test call instruction\n";
        return 2;
    }

    std::array<std::uint8_t, 5> expected{};
    std::memcpy(expected.data(), call, expected.size());
    std::array<std::uint8_t, 5> wrong_expected = expected;
    ++wrong_expected[4];

    std::string error;
    if (penumbra_vr::hooks::InstallRel32CallHook(
            call,
            wrong_expected,
            reinterpret_cast<void*>(&HookedRenderWorld),
            g_hook,
            error)) {
        std::cerr << "The hook accepted mismatched version bytes\n";
        return 3;
    }
    if (!penumbra_vr::hooks::InstallRel32CallHook(
            call,
            expected,
            reinterpret_cast<void*>(&HookedRenderWorld),
            g_hook,
            error)) {
        std::cerr << "InstallRel32CallHook failed: " << error << '\n';
        return 4;
    }

    CallRenderWorld(&renderer, world, camera, 0.5F);
    CallRenderWorld(&renderer, world, camera, 0.75F);
    if (g_hook_calls != 2 || renderer.original_calls != 3 ||
        renderer.last_world != world || renderer.last_camera != camera ||
        renderer.last_frame_time != 0.75F) {
        std::cerr << "The hooked calls were not intercepted and forwarded correctly\n";
        return 5;
    }

    if (!penumbra_vr::hooks::RemoveRel32CallHook(g_hook, error)) {
        std::cerr << "RemoveRel32CallHook failed: " << error << '\n';
        return 6;
    }
    if (!std::equal(expected.begin(), expected.end(), call)) {
        std::cerr << "The original call instruction was not restored\n";
        return 7;
    }

    CallRenderWorld(&renderer, world, camera, 1.0F);
    if (g_hook_calls != 2 || renderer.original_calls != 4 ||
        renderer.last_frame_time != 1.0F) {
        std::cerr << "The restored call did not bypass the hook\n";
        return 8;
    }

    std::cout << "rel32 call interception, forwarding and restoration passed\n";
    return 0;
}
