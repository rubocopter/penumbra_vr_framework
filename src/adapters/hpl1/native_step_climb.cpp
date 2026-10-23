#include "native_step_climb.hpp"
#include "rel32_call_hook.hpp"
#include "vr_step_climb.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace penumbra_vr::adapters::hpl1 {
namespace {
hooks::Rel32JumpHook g_hook;
NativeStepClimbLayout g_layout;
NativeStepClimbEligibility g_eligible{};
void* g_resume{};
std::uint8_t* g_published_image{};
std::array<std::uint8_t, 5> g_expected_jump{};
std::atomic<bool> g_installed{false};

bool SameLayout(const NativeStepClimbLayout& layout) noexcept {
    return layout.success_rva == g_layout.success_rva &&
        layout.success_signature == g_layout.success_signature &&
        layout.timestep_stack_offset == g_layout.timestep_stack_offset &&
        layout.ray_reference_height_stack_offset == g_layout.ray_reference_height_stack_offset &&
        layout.ray_distances_stack_offset == g_layout.ray_distances_stack_offset &&
        layout.position_y_offset == g_layout.position_y_offset &&
        layout.climb_speed_offset == g_layout.climb_speed_offset &&
        layout.climbing_flag_offset == g_layout.climbing_flag_offset;
}

bool ReadBytes(const void* source, void* target, std::size_t length) noexcept {
    __try {
        std::memcpy(target, source, length);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

float ReadFloat(const std::uint8_t* source, std::size_t offset) noexcept {
    float value{};
    std::memcpy(&value, source + offset, sizeof(value));
    return value;
}

void __cdecl ApplyNativeStepClimb(void* character_body,
    const std::uint8_t* native_frame, std::uint32_t selected_ray) noexcept {
    auto* body = static_cast<std::uint8_t*>(character_body);
    const float dt = ReadFloat(native_frame, g_layout.timestep_stack_offset);
    const float speed = ReadFloat(body, g_layout.climb_speed_offset);
    const float native_advance = dt * speed;
    float advance = native_advance;
    std::uint8_t climbing = 1;
    if (g_eligible != nullptr && g_eligible(character_body)) {
        // Native success retains EBX, the accepted ray index. The former
        // height local is already reused by shape construction at this point.
        const float surface_height =
            ReadFloat(native_frame, g_layout.ray_reference_height_stack_offset) -
            ReadFloat(native_frame, g_layout.ray_distances_stack_offset +
                selected_ray * sizeof(float));
        const auto plan = runtime::ResolveVrStepClimbAdvance(
            native_advance, surface_height);
        advance = plan.distance;
        climbing = plan.climbing ? 1U : 0U;
    }
    const float y = ReadFloat(body, g_layout.position_y_offset) + advance;
    std::memcpy(body + g_layout.position_y_offset, &y, sizeof(y));
    body[g_layout.climbing_flag_offset] = climbing;
}

__declspec(naked) void StepClimbGateway() noexcept {
    __asm {
        pushfd
        pushad
        // The original frame starts before pushfd/pushad (36 bytes).
        lea eax, [esp + 24h]
        push ebx
        push eax
        push esi
        call ApplyNativeStepClimb
        add esp, 12
        popad
        popfd
        jmp dword ptr [g_resume]
    }
}
}

bool InstallNativeStepClimbAdapter(std::uint8_t* image,
    const NativeStepClimbLayout& layout, NativeStepClimbEligibility eligible,
    std::string& error) noexcept {
    error.clear();
    if (image == nullptr || layout.success_rva == 0 || eligible == nullptr ||
        layout.timestep_stack_offset == 0 || layout.ray_reference_height_stack_offset == 0 ||
        layout.ray_distances_stack_offset == 0 ||
        layout.position_y_offset == 0 || layout.climb_speed_offset == 0 ||
        layout.climbing_flag_offset == 0) {
        error = "The native step-climb contract is incomplete";
        return false;
    }
    if (g_hook.installed()) {
        if (g_hook.instruction == image + layout.success_rva &&
            g_eligible == eligible && SameLayout(layout) &&
            NativeStepClimbAdapterMatchesLive()) return true;
        error = "The native step-climb instruction already has another owner";
        return false;
    }
    if (g_published_image != nullptr &&
        (g_published_image != image || !SameLayout(layout) || g_eligible != eligible)) {
        error = "The published native step-climb contract cannot change during process lifetime";
        return false;
    }
    std::array<std::uint8_t, 26> live{};
    if (!ReadBytes(image + layout.success_rva, live.data(), live.size()) ||
        live != layout.success_signature) {
        error = "The complete native step-climb block does not match the exact build";
        return false;
    }
    if (g_published_image == nullptr) {
        g_layout = layout;
        g_eligible = eligible;
        g_resume = image + layout.success_rva + live.size();
        g_expected_jump[0] = 0xE9;
        const auto displacement = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(&StepClimbGateway) -
            reinterpret_cast<std::uintptr_t>(image + layout.success_rva + 5));
        std::memcpy(g_expected_jump.data() + 1, &displacement, sizeof(displacement));
        g_published_image = image;
    }
    std::array<std::uint8_t, 5> window{};
    std::copy_n(live.begin(), window.size(), window.begin());
    const bool installed = hooks::InstallRel32JumpHook(image + layout.success_rva,
        window, reinterpret_cast<void*>(&StepClimbGateway), g_hook, error);
    g_installed.store(installed, std::memory_order_release);
    return installed;
}

bool RemoveNativeStepClimbAdapter(std::string& error) noexcept {
    // Keep callback data/resume resident for a dispatched gateway; the owning
    // backend provides its existing callback-drain/process-lifetime contract.
    const bool removed = hooks::RemoveRel32JumpHook(g_hook, error);
    if (removed) g_installed.store(false, std::memory_order_release);
    return removed;
}

bool NativeStepClimbAdapterInstalled() noexcept {
    return g_installed.load(std::memory_order_acquire);
}

bool NativeStepClimbAdapterMatchesLive() noexcept {
    if (!NativeStepClimbAdapterInstalled()) return false;
    std::array<std::uint8_t, 26> live{};
    if (!ReadBytes(g_published_image + g_layout.success_rva,
            live.data(), live.size())) return false;
    return std::equal(g_expected_jump.begin(), g_expected_jump.end(), live.begin()) &&
        std::equal(g_layout.success_signature.begin() + 5,
            g_layout.success_signature.end(), live.begin() + 5);
}

} // namespace penumbra_vr::adapters::hpl1
