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
// Independently decoded initialized image SHA256 2881741D...35A8295:
// cEffect_SubTitle::OnPostSceneDraw CF90, ordinary text/shadows only.
// Alternate transient GUI/black-text paths remain native.
constexpr std::array<std::uintptr_t,3> kSubtitleFontCalls{0xD2A6,0xD2F6,0xD346};
constexpr TextSize kSubtitleOrigin{25,500};
// Radio owns video-chat/slideshow narration too. Mapped independently from
// the initialized BP image, including its separate ScreenGui row renderer.
constexpr std::array<std::uintptr_t,4> kRadioFontCalls{0xB1EBB,0xB1F00,0xB1F9E,0xB200E};
constexpr std::array<std::uintptr_t,3> kRadioGuiCalls{0xB1C03,0xB1CB9,0xB1D21};
constexpr std::uintptr_t kRadioRowsRva=0xEC0C0,kRadioGuiFontRva=0xFA460;
// level22 StartEnding1 -> cEnding: independently decoded BP text owner.
constexpr std::uintptr_t kEndingTextCall=0xDA82,kEndingTextRva=0xD5B0;
constexpr std::size_t kEndingFont=0x28,kEndingTextActive=0x30,
    kEndingRowsBegin=0x38,kEndingRowsEnd=0x3C,kEndingVisibleChars=0x48,
    kEndingShade=0x54,kEndingAlpha=0x5C;
constexpr std::uintptr_t kWordWrapRva=0xEBC10, kFontLengthRva=0xEB5A0;
constexpr std::array<std::uint8_t,10> kFontLengthEntry{
    0x8B,0x54,0x24,0x08,0xD9,0x05,0xC0,0x29,0x67,0};
constexpr const char* kMessageCallMismatch="Message font call does not match the exact BP build";
bool MessagePresentationEligible() noexcept {
    return TrackedStereoPresentationActive() && !NativeInputUiActive();
}
} // namespace
#include "../../adapters/hpl1/native_message_presentation.inl"
}
