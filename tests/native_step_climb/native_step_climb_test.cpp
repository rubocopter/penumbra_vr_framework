#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "native_step_climb.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {
// Exact original success block, independently observed at BP D78CF and
// Requiem D826F. Height is ray reference height (+40) minus selected ray distance
// (+5C + EBX*4). Native D781C/D81BC reuses the old +58 local as a pointer.
constexpr std::array<std::uint8_t, 26> kSuccess{
    0xD9,0x84,0x24,0xF4,0x05,0x00,0x00,
    0xC6,0x86,0x38,0x02,0x00,0x00,0x01,
    0xD8,0x8E,0x2C,0x02,0x00,0x00,
    0xD8,0x46,0x4C,0xD9,0x5E,0x4C};
bool g_vr_intent = true;
void* g_body{};
bool Eligible(void* body) noexcept { return g_vr_intent && body == g_body; }

// Execute the shipped instructions with their real stack/register contract.
// CALL adds four bytes: place native-frame locals relative to the resulting ESP.
__declspec(naked) void Invoke(void*, void*, float, float, std::uint32_t = 1) {
    __asm {
        push ebp
        mov ebp, esp
        push esi
        push ebx
        sub esp, 600h
        mov esi, [ebp + 0Ch]
        mov eax, [ebp + 10h]
        mov [esp + 5F0h], eax
        mov ebx, [ebp + 18h]
        mov dword ptr [esp + 3Ch], 3F800000h
        mov dword ptr [esp + 44h], 2
        mov dword ptr [esp + 58h], 3F000000h
        mov dword ptr [esp + 5Ch], 3F000000h
        fld1
        fsub dword ptr [ebp + 14h]
        fstp dword ptr [esp + ebx*4 + 58h]
        // The real native shape-construction path overwrites this local.
        mov [esp + 54h], esp
        mov eax, [ebp + 8]
        call eax
        add esp, 600h
        pop ebx
        pop esi
        pop ebp
        ret
    }
}

void Put(std::array<std::uint8_t, 0x240>& body, std::size_t offset, float value) {
    std::memcpy(body.data() + offset, &value, sizeof(value));
}
float ReadY(const std::array<std::uint8_t, 0x240>& body) {
    float value{};
    std::memcpy(&value, body.data() + 0x4C, sizeof(value));
    return value;
}
}

int main() {
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (image == nullptr) return 1;
    constexpr std::size_t site = 0x80;
    std::memcpy(image + site, kSuccess.data(), kSuccess.size());
    image[site + kSuccess.size()] = 0xC3;
    std::array<std::uint8_t, 0x240> body{};
    g_body = body.data();
    Put(body, 0x22C, 3.1F);
    Put(body, 0x4C, 10.0F);

    using namespace penumbra_vr::adapters::hpl1;
    NativeStepClimbLayout layout{site, 0x5F4, 0x40, 0x5C, 0x4C, 0x22C, 0x238, kSuccess};
    std::string error;
    // Validate every byte, including the part beyond the five-byte JMP.
    image[site + 25] ^= 1;
    if (InstallNativeStepClimbAdapter(image, layout, Eligible, error) ||
        error.empty() || NativeStepClimbAdapterInstalled() || image[site] != 0xD9)
        return 3;
    image[site + 25] ^= 1;
    if (!InstallNativeStepClimbAdapter(image, layout, Eligible, error) ||
        !NativeStepClimbAdapterMatchesLive() ||
        !InstallNativeStepClimbAdapter(image, layout, Eligible, error)) return 4;

    // A one-centimetre slope rise must not receive a 5.17 cm climb at 60 Hz.
    Invoke(image + site, body.data(), 1.0F/60.0F, 0.01F);
    const float rise = ReadY(body) - 10.0F;
    if (std::abs(rise - 0.01F) > 0.00001F) {
        std::cerr << "native step overshot a 1 cm surface: " << rise << " m\n";
        VirtualFree(image, 0, MEM_RELEASE);
        return 2;
    }
    // EBX can select a different successful ray; another ray's distance and
    // the overwritten pointer local must not influence the accepted rise.
    Put(body, 0x4C, 10.0F);
    Invoke(image + site, body.data(), 1.0F/60.0F, 0.02F, 0);
    if (std::abs(ReadY(body) - 10.02F) > 0.00001F) return 10;
    struct Case { float dt; float height; float expected; bool climbing; };
    const Case cases[]{
        {1.0F/30.0F, 0.01F, 0.01F, true},
        {1.0F/90.0F, 0.01F, 0.01F, true},
        {1.0F/120.0F, 0.01F, 0.01F, true},
        {1.0F/60.0F, 0.35F, 3.1F/60.0F, true},
        {1.0F/60.0F, 0.0F, 0.0F, false},
        {1.0F/60.0F, -0.01F, 0.0F, false},
        {1.0F/60.0F, NAN, 0.0F, false},
    };
    for (const auto& c : cases) {
        Put(body, 0x4C, 10.0F);
        body[0x238] = 1;
        Invoke(image + site, body.data(), c.dt, c.height);
        if (std::abs(ReadY(body) - 10.0F - c.expected) > 0.00001F ||
            (body[0x238] != 0) != c.climbing) return 5;
    }
    // Consecutive shallow ascent ticks follow the measured surface without
    // accumulating the native overshoot that later falls back under gravity.
    Put(body, 0x4C, 10.0F);
    for (int tick = 0; tick < 100; ++tick)
        Invoke(image + site, body.data(), 1.0F/60.0F, 0.01F);
    if (std::abs(ReadY(body) - 11.0F) > 0.0001F) return 6;

    // Non-VR/native updates retain exactly the original increment and flag.
    g_vr_intent = false;
    Put(body, 0x4C, 10.0F);
    Invoke(image + site, body.data(), 1.0F/60.0F, 0.01F);
    if (std::abs(ReadY(body) - 10.0F - 3.1F/60.0F) > 0.00001F ||
        body[0x238] != 1) return 7;
    if (!RemoveNativeStepClimbAdapter(error) || NativeStepClimbAdapterInstalled() ||
        std::memcmp(image + site, kSuccess.data(), kSuccess.size()) != 0) return 8;
    Put(body, 0x4C, 10.0F);
    Invoke(image + site, body.data(), 1.0F/60.0F, 0.01F);
    if (std::abs(ReadY(body) - 10.0F - 3.1F/60.0F) > 0.00001F) return 9;
    VirtualFree(image, 0, MEM_RELEASE);
    return 0;
}
