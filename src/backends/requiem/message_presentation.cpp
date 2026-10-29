#include "message_presentation.hpp"
#include "gameplay_bridge.hpp"
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

namespace penumbra_vr::backends::requiem {
namespace {
// Independently decoded from the Requiem initialized Steam image, unpacked
// SHA256 989726EE2780AC890486DFCA0AB6B80AF8E97BC4FF64AF5A28CF1F23ACE2CA5B.
// cGameMessage::Draw 3F500 owns these six callers, not Radio or menu draws.
using TextPosition = std::array<float,3>;
using TextSize = std::array<float,2>;
using WordWrap = int(__thiscall*)(void*,TextPosition,float,float,TextSize,
    const void*,int,const void*);
constexpr std::array<std::uintptr_t,6> kMessageFontCalls{
    0x3F568,0x3F5D2,0x3F61D,0x3F668,0x3F6B3,0x3F6FE};
// Same independently decoded image: cEffect_SubTitle::OnPostSceneDraw D0F0.
// Ordinary transient text/shadows; alternate transient GUI/black-text stays native.
constexpr std::array<std::uintptr_t,3> kSubtitleFontCalls{0xD406,0xD456,0xD4A6};
constexpr TextSize kSubtitleOrigin{25,500};
// Independently decoded Requiem Radio current/previous text, shadows and
// ScreenGui row renderer; no BP-relative RVA inference.
constexpr std::array<std::uintptr_t,4> kRadioFontCalls{0xB22BB,0xB2300,0xB239E,0xB240E};
constexpr std::array<std::uintptr_t,3> kRadioGuiCalls{0xB2003,0xB20B9,0xB2121};
constexpr std::uintptr_t kRadioRowsRva=0xEBFB0,kRadioGuiFontRva=0xFA4E0;
// Independent Requiem cEnding map (level00 StartEnding1; not final Credits).
constexpr std::uintptr_t kEndingTextCall=0xDB92,kEndingTextRva=0xD6C0;
constexpr std::size_t kEndingFont=0x28,kEndingTextActive=0x30,
    kEndingRowsBegin=0x38,kEndingRowsEnd=0x3C,kEndingVisibleChars=0x48,
    kEndingShade=0x54,kEndingAlpha=0x5C;
constexpr std::uintptr_t kWordWrapRva=0xEBB00, kFontLengthRva=0xEB490;
constexpr std::array<std::uint8_t,10> kFontLengthEntry{
    0x8B,0x54,0x24,0x08,0xD9,0x05,0xC0,0x39,0x67,0};
constexpr const char* kMessageCallMismatch="Message font call does not match the exact Requiem build";
bool MessagePresentationEligible() noexcept {
    return TrackedStereoPresentationActive() && !NativeUiActive();
}
} // namespace
#include "../../adapters/hpl1/native_message_presentation.inl"
}
