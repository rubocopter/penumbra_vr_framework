#include "message_presentation.hpp"
#include "render_world_probe.hpp"
#include "native_input_bridge.hpp"
#include "rel32_call_hook.hpp"
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>

namespace penumbra_vr::backends::black_plague {
namespace {
using TextPosition = std::array<float,3>;
using TextSize = std::array<float,2>;
using WordWrap = int(__thiscall*)(void*,TextPosition,float,float,TextSize,
    const void*,int,const void*);
constexpr std::array<std::uintptr_t,6> kMessageFontCalls{
    0x3F3E8,0x3F452,0x3F49D,0x3F4E8,0x3F533,0x3F57E};
constexpr std::uintptr_t kWordWrapRva=0xEBC10, kFontLengthRva=0xEB5A0;
constexpr std::array<std::uint8_t,10> kFontLengthEntry{
    0x8B,0x54,0x24,0x08,0xD9,0x05,0xC0,0x29,0x67,0};
constexpr const char* kMessageCallMismatch="GameMessage font call does not match the exact BP build";
bool MessagePresentationEligible() noexcept {
    return TrackedStereoPresentationActive() && !NativeInputUiActive();
}
} // namespace
#include "../../adapters/hpl1/native_message_presentation.inl"
}
