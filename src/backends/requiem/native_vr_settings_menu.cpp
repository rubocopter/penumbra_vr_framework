#include "native_vr_settings_menu.hpp"
#include "gameplay_bridge.hpp"
#include "render_world.hpp"

#include "iat_hook.hpp"
#include "rel32_call_hook.hpp"
#include "vr_settings_capabilities.hpp"
#include "native_vr_menu_policy.hpp"
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
#include <vector>
#include <cmath>

namespace penumbra_vr::backends::requiem {
namespace {
constexpr std::uintptr_t kButtonVtableRva = 0x27BF1C;
constexpr std::uintptr_t kButtonMouseDownSlotRva = kButtonVtableRva + 0x0C;
constexpr std::uintptr_t kButtonActiveChangedSlotRva = kButtonVtableRva + 0x20;
constexpr std::uintptr_t kButtonMouseDownRva = 0x7B1A0;
constexpr std::uintptr_t kButtonActiveChangedRva = 0x74430;
constexpr std::uintptr_t kButtonConstructorRva = 0x74980;
constexpr std::uintptr_t kButtonDeletingDestructorRva = 0x74B20;
constexpr std::uintptr_t kAddWidgetToStateRva = 0x7B5B0;
constexpr std::uintptr_t kMenuSetStateRva = 0x7ABF0;
constexpr std::array<std::uintptr_t, 10> kMenuSetStateCallsites{
    0x7B118, 0x7B1C8, 0x7B2E8, 0x7B311, 0x7B40F,
    0x8A544, 0x8A9FD, 0x8AA61, 0x8ADF9, 0x8AE88};
constexpr std::uintptr_t kGameAllocateRva = 0x1EF408;
constexpr std::uintptr_t kWideStringConstructorIatRva = 0x2731F8;
constexpr std::uintptr_t kWideStringDestructorIatRva = 0x2731EC;
constexpr std::uintptr_t kWideStringAssignIatRva = 0x273128;

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
constexpr int kRootSentinel = 0x70000000;
constexpr int kRowSentinelBase = 0x70000100;
constexpr int kBackSentinel = 0x70000200;
constexpr int kBindingsSentinel = 0x70000300;
constexpr int kCalibrateHeightSentinel = 0x70000400;

constexpr std::size_t kNativeVrMenuSettingCount = kRequiemVrMenuSettingCount;
constexpr std::size_t kNativeVrMenuRowsPerColumn = 5;
constexpr const wchar_t* kNativeSettingsDirectory = L"Requiem";
constexpr const char* kNativeSpanishLanguageSetting = "languagefile=\"espanol_exp.lang\"";
const auto& NativeVrMenuSettings() noexcept { return RequiemVrMenuSettings(); }
auto NativeVrSettingCapabilities() noexcept { return RequiemVrSettingCapabilities(); }
bool NativeVrMenuRootBindingMatches(const void* root, const void* widget,
    const void* vtable, const void* expected, int state) noexcept {
    return adapters::hpl1::NativeVrMenuRootBindingMatches(root, widget, vtable, expected, state);
}
bool NativeVrMenuNeedsRebindOnStateTransition(int next, bool root, bool owner) noexcept {
    return adapters::hpl1::NativeVrMenuNeedsRebindOnStateTransition(next, root, owner);
}
bool TryCalibratePlayerHeight(runtime::VrSettings& settings, float height) noexcept {
    if (!std::isfinite(height) || height <= 0.90F || height >= 2.20F) return false;
    settings.player_height = height;
    runtime::NormalizeVrSettings(settings);
    return true;
}
bool TrackedHeadTrackingHeightForCalibration(float& height) noexcept {
    return TrackedHeadTrackingHeightForMenu(height);
}
bool ValidateNativeVrMenuContract(const std::uint8_t* image, std::string& error) noexcept {
    // Initialized-image signatures, not encrypted disk bytes. The constructor
    // and mouse-handler slices start after their relocation-bearing EH push.
    const std::array<std::pair<std::uintptr_t, std::vector<std::uint8_t>>, 7> entries{{
        {kMenuSetStateRva, {0x8B,0x54,0x24,0x04,0x53,0x55,0x56,0x57,0x8B,0xF9}},
        {kButtonConstructorRva + 7, {0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89,0x25,0x00,0x00,0x00,0x00,0x83,0xEC,0x28,0x8B,0x54,0x24,0x38}},
        {kButtonDeletingDestructorRva, {0x56,0x8B,0xF1,0xE8,0x48,0xF5,0xFF,0xFF}},
        {kAddWidgetToStateRva, {0x53,0x55,0x56,0x8B,0xE9,0x57}},
        {kButtonMouseDownRva + 7, {0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64,0x89,0x25,0x00,0x00,0x00,0x00,0x83,0xEC,0x28,0x56,0x8B,0xF1,0x8B,0x46,0x5C,0x8B,0x4E,0x08,0x8B,0x89,0xB8,0x01,0x00,0x00,0x50}},
        {kButtonActiveChangedRva, {0x33,0xC0,0x89,0x41,0x2C,0x89,0x81,0x80,0x00,0x00,0x00,0x88,0x41,0x04,0xC3}},
        {kGameAllocateRva, {0x56,0x8B,0x74,0x24,0x08,0xEB,0x11,0x56}},
    }};
    for (const auto& entry : entries) {
        if (std::memcmp(image + entry.first, entry.second.data(), entry.second.size()) != 0) {
            error = "Requiem initialized native-menu signature mismatch";
            return false;
        }
    }
    return true;
}
} // namespace

#include "native_vr_settings_menu.inl"

} // namespace penumbra_vr::backends::requiem
