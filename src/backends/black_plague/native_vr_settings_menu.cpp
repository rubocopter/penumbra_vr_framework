#include "native_vr_settings_menu.hpp"
#include "native_input_bridge.hpp"
#include "render_world_probe.hpp"

#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#include "vr_settings_capabilities.hpp"
#include "vr_settings_editor.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace penumbra_vr::backends::black_plague {
namespace {
constexpr std::uintptr_t kButtonVtableRva = 0x27AD44;
constexpr std::uintptr_t kButtonMouseDownSlotRva = kButtonVtableRva + 0x0C;
constexpr std::uintptr_t kButtonActiveChangedSlotRva = kButtonVtableRva + 0x20;
constexpr std::uintptr_t kButtonMouseDownRva = 0x7AC30;
constexpr std::uintptr_t kButtonActiveChangedRva = 0x73BC0;
constexpr std::uintptr_t kButtonConstructorRva = 0x74110;
constexpr std::uintptr_t kButtonDeletingDestructorRva = 0x742B0;
constexpr std::uintptr_t kAddWidgetToStateRva = 0x7B040;
constexpr std::uintptr_t kMenuSetStateRva = 0x7A680;
constexpr std::array<std::uintptr_t, 10> kMenuSetStateCallsites{
    0x7ABA8, 0x7AC58, 0x7AD78, 0x7ADA1, 0x7AE9F,
    0x8A424, 0x8A8DD, 0x8A941, 0x8ACD9, 0x8AD68};
constexpr std::uintptr_t kGameAllocateRva = 0x1EECC8;
constexpr std::uintptr_t kWideStringConstructorIatRva = 0x2721FC;
constexpr std::uintptr_t kWideStringDestructorIatRva = 0x2721F0;
constexpr std::uintptr_t kWideStringAssignIatRva = 0x272180;

constexpr std::ptrdiff_t kWidgetInitOffset = 0x08;
constexpr std::ptrdiff_t kWidgetActiveOffset = 0x30;
constexpr std::ptrdiff_t kWidgetTextOffset = 0x34;
constexpr std::ptrdiff_t kButtonTargetStateOffset = 0x5C;
constexpr std::ptrdiff_t kInitMainMenuOffset = 0x1B8;
constexpr std::ptrdiff_t kMenuPreviousStateOffset = 0x98;
constexpr std::ptrdiff_t kMenuCurrentStateOffset = 0xB4;
constexpr std::ptrdiff_t kMenuStateListsOffset = 0xBC;
constexpr int kStartState = 0;
constexpr std::size_t kNativeWideStringSize = 0x1C;
constexpr std::size_t kButtonObjectSize = 0x84;
constexpr int kFontAlignCenter = 2;
constexpr int kLeftMouse = 0;
constexpr int kRightMouse = 2;
constexpr int kSafeRemovedTargetState = kStartState;
constexpr int kRootSentinel = kBlackPlagueVrMenuRootSentinel;
constexpr int kRowSentinelBase = 0x70000100;
constexpr int kBackSentinel = 0x70000200;
constexpr int kBindingsSentinel = 0x70000300;
constexpr int kCalibrateHeightSentinel = 0x70000400;

constexpr std::size_t kNativeVrMenuSettingCount = kBlackPlagueVrMenuSettingCount;
constexpr std::size_t kNativeVrMenuRowsPerColumn = 9;
constexpr const wchar_t* kNativeSettingsDirectory = L"Black Plague";
constexpr const char* kNativeSpanishLanguageSetting = "languagefile=\"espanol.lang\"";
const auto& NativeVrMenuSettings() noexcept { return BlackPlagueVrMenuSettings(); }
auto NativeVrSettingCapabilities() noexcept { return BlackPlagueVrSettingCapabilities(); }
bool NativeVrMenuRootBindingMatches(const void* root, const void* widget,
    const void* vtable, const void* expected, int state) noexcept {
    return BlackPlagueVrMenuRootBindingMatches(root, widget, vtable, expected, state);
}
bool NativeVrMenuNeedsRebindOnStateTransition(int next, bool root, bool owner) noexcept {
    return BlackPlagueVrMenuNeedsRebindOnStateTransition(next, root, owner);
}
bool ValidateNativeVrMenuContract(const std::uint8_t*, std::string&) noexcept { return true; }
} // namespace

#include "native_vr_settings_menu.inl"

} // namespace penumbra_vr::backends::black_plague
