#include "native_vr_settings_menu.hpp"

#include "iat_hook.hpp"
#include "vr_settings_capabilities.hpp"
#include "vr_settings_editor.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
constexpr std::uintptr_t kGameAllocateRva = 0x1EECC8;
constexpr std::uintptr_t kWideStringConstructorIatRva = 0x2721FC;
constexpr std::uintptr_t kWideStringDestructorIatRva = 0x2721F0;
constexpr std::uintptr_t kWideStringAssignIatRva = 0x272200;

constexpr std::ptrdiff_t kWidgetInitOffset = 0x08;
constexpr std::ptrdiff_t kWidgetYPositionOffset = 0x14;
constexpr std::ptrdiff_t kWidgetDrawYOffset = 0x20;
constexpr std::ptrdiff_t kWidgetActiveOffset = 0x30;
constexpr std::ptrdiff_t kWidgetTextOffset = 0x34;
constexpr std::ptrdiff_t kButtonTargetStateOffset = 0x5C;
constexpr std::ptrdiff_t kInitMainMenuOffset = 0x1B8;
constexpr std::ptrdiff_t kMenuPreviousStateOffset = 0x98;
constexpr std::ptrdiff_t kMenuCurrentStateOffset = 0xB4;
constexpr std::ptrdiff_t kMenuStateListsOffset = 0xBC;
constexpr int kOptionsState = 8;
constexpr std::size_t kNativeWideStringSize = 0x1C;
constexpr std::size_t kButtonObjectSize = 0x84;
constexpr int kFontAlignCenter = 2;
constexpr int kLeftMouse = 0;
constexpr int kRightMouse = 2;
constexpr int kSafeRemovedTargetState = kOptionsState;
constexpr int kRootSentinel = 0x70000000;
constexpr int kRowSentinelBase = 0x70000100;
constexpr int kBackSentinel = 0x70000200;

struct NativeVec2 { float x; float y; };
struct NativeVec3 { float x; float y; float z; };

using ButtonMouseDown = void (__thiscall*)(void*, int);
using ButtonActiveChanged = void (__thiscall*)(void*);
using ButtonConstructor = void* (__thiscall*)(
    void*, void*, const NativeVec3*, const void*, int, NativeVec2, int);
using ButtonDeletingDestructor = void* (__thiscall*)(void*, unsigned int);
using AddWidgetToState = void (__thiscall*)(void*, int, void*);
using GameAllocate = void* (__cdecl*)(std::size_t);
using WideStringConstructor = void (__thiscall*)(void*, const wchar_t*);
using WideStringDestructor = void (__thiscall*)(void*);
using WideStringAssign = void (__thiscall*)(void*, const wchar_t*);

std::uint8_t* g_image = nullptr;
hooks::IatHook g_mouse_down_hook;
hooks::IatHook g_active_changed_hook;
ButtonMouseDown g_original_mouse_down = nullptr;
ButtonActiveChanged g_original_active_changed = nullptr;
runtime::VrSettings* g_settings = nullptr;
CommitNativeVrSettings g_commit = nullptr;
void* g_menu = nullptr;
void* g_root = nullptr;
void* g_back = nullptr;
void* g_native_back = nullptr;
float g_native_back_y = 0.0F;
float g_native_back_draw_y = 0.0F;
std::array<void*, kBlackPlagueVrMenuSettingCount> g_rows{};
std::atomic<bool> g_installed{false};
bool g_page_active = false;
bool g_injecting = false;

template <typename T>
[[nodiscard]] T Read(const void* base, std::ptrdiff_t offset = 0) noexcept {
    T value{};
    std::memcpy(&value,
        static_cast<const std::uint8_t*>(base) + offset, sizeof(T));
    return value;
}

template <typename T>
void Write(void* base, std::ptrdiff_t offset, const T& value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(T));
}

[[nodiscard]] void* MenuFromWidget(void* widget) noexcept {
    if (widget == nullptr) return nullptr;
    void* const init = Read<void*>(widget, kWidgetInitOffset);
    return init == nullptr ? nullptr : Read<void*>(init, kInitMainMenuOffset);
}

[[nodiscard]] bool IsCustomWidget(void* widget) noexcept {
    if (widget == g_root || widget == g_back) return widget != nullptr;
    for (void* row : g_rows) {
        if (widget == row && row != nullptr) return true;
    }
    return false;
}

[[nodiscard]] int RowIndex(void* widget) noexcept {
    for (std::size_t index = 0; index < g_rows.size(); ++index) {
        if (g_rows[index] == widget && widget != nullptr) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void SetWidgetActive(void* widget, bool active) noexcept {
    if (widget == nullptr || g_original_active_changed == nullptr) return;
    const bool current = Read<std::uint8_t>(widget, kWidgetActiveOffset) != 0;
    if (current == active) return;
    Write<std::uint8_t>(widget, kWidgetActiveOffset,
        active ? std::uint8_t{1} : std::uint8_t{0});
    g_original_active_changed(widget);
}

template <typename Callback>
void ForEachStateWidget(void* menu, int state, Callback&& callback) noexcept {
    if (menu == nullptr) return;
    auto* const state_lists = Read<std::uint8_t*>(menu, kMenuStateListsOffset);
    if (state_lists == nullptr) return;
    void* const sentinel = Read<void*>(state_lists + state * 12, 4);
    if (sentinel == nullptr) return;
    void* node = Read<void*>(sentinel);
    std::size_t guard = 0;
    while (node != nullptr && node != sentinel && guard++ < 256) {
        void* const next = Read<void*>(node);
        callback(Read<void*>(node, 8));
        node = next;
    }
}

[[nodiscard]] const wchar_t* SettingName(runtime::VrSettingId id) noexcept {
    using I = runtime::VrSettingId;
    switch (id) {
    case I::handedness: return L"Handedness";
    case I::play_mode: return L"Play mode";
    case I::player_height: return L"Player height";
    case I::turn_mode: return L"Turn mode";
    case I::snap_turn_angle: return L"Snap angle";
    case I::smooth_turn_speed: return L"Smooth turn";
    case I::turn_dead_zone: return L"Turn dead zone";
    case I::move_speed: return L"Move speed";
    case I::move_dead_zone: return L"Move dead zone";
    case I::crouch_mode: return L"Crouch mode";
    case I::physical_crouch_depth: return L"Crouch depth";
    case I::height_offset: return L"Height offset";
    case I::ui_distance: return L"UI distance";
    case I::ui_scale: return L"UI scale";
    case I::render_scale: return L"Render scale (restart)";
    case I::enhanced_visuals: return L"Enhanced visuals";
    case I::hrtf: return L"HRTF (restart)";
    default: return L"VR setting";
    }
}

[[nodiscard]] std::wstring WidenAscii(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

[[nodiscard]] std::wstring RowText(runtime::VrSettingId id) {
    std::wstring result = SettingName(id);
    result += L": ";
    result += WidenAscii(runtime::FormatVrSettingValue(id, *g_settings));
    const auto capabilities = BlackPlagueVrSettingCapabilities();
    if (!runtime::IsVrSettingAvailable(id, *g_settings, capabilities)) {
        result += L" [inactive]";
    }
    return result;
}

void AssignButtonText(void* widget, const std::wstring& value) noexcept {
    if (widget == nullptr || g_image == nullptr) return;
    auto* const assign = reinterpret_cast<WideStringAssign>(
        Read<void*>(g_image + kWideStringAssignIatRva));
    if (assign != nullptr) {
        assign(static_cast<std::uint8_t*>(widget) + kWidgetTextOffset,
            value.c_str());
    }
}

void RefreshRows() noexcept {
    if (g_settings == nullptr) return;
    const auto& settings = BlackPlagueVrMenuSettings();
    for (std::size_t index = 0; index < settings.size(); ++index) {
        if (g_rows[index] != nullptr) {
            AssignButtonText(g_rows[index], RowText(settings[index]));
        }
    }
}

[[nodiscard]] void* CreateButton(
    void* init,
    const NativeVec3& position,
    const std::wstring& text,
    int target,
    float font_size) noexcept {
    auto* const allocate = reinterpret_cast<GameAllocate>(g_image + kGameAllocateRva);
    auto* const construct = reinterpret_cast<ButtonConstructor>(
        g_image + kButtonConstructorRva);
    auto* const wide_construct = reinterpret_cast<WideStringConstructor>(
        Read<void*>(g_image + kWideStringConstructorIatRva));
    auto* const wide_destroy = reinterpret_cast<WideStringDestructor>(
        Read<void*>(g_image + kWideStringDestructorIatRva));
    if (allocate == nullptr || construct == nullptr || wide_construct == nullptr ||
        wide_destroy == nullptr) return nullptr;

    void* const object = allocate(kButtonObjectSize);
    if (object == nullptr) return nullptr;
    alignas(void*) std::array<std::byte, kNativeWideStringSize> native_text{};
    wide_construct(native_text.data(), text.c_str());
    const NativeVec2 font{font_size, font_size};
    void* const result = construct(object, init, &position, native_text.data(),
        target, font, kFontAlignCenter);
    wide_destroy(native_text.data());
    return result;
}

void DestroyUnownedButton(void* widget) noexcept {
    if (widget == nullptr || g_image == nullptr) return;
    reinterpret_cast<ButtonDeletingDestructor>(
        g_image + kButtonDeletingDestructorRva)(widget, 1U);
}

void RestoreNativeBackPosition() noexcept {
    if (g_native_back == nullptr) return;
    Write<float>(g_native_back, kWidgetYPositionOffset, g_native_back_y);
    Write<float>(g_native_back, kWidgetDrawYOffset, g_native_back_draw_y);
}

void MoveNativeBackForVrEntry(void* menu) noexcept {
    ForEachStateWidget(menu, kOptionsState, [](void* widget) noexcept {
        if (g_native_back != nullptr || widget == nullptr || IsCustomWidget(widget)) return;
        if (Read<void*>(widget) != g_image + kButtonVtableRva) return;
        if (Read<int>(widget, kButtonTargetStateOffset) != 0) return;
        const float y = Read<float>(widget, kWidgetYPositionOffset);
        if (y < 360.0F || y > 395.0F) return;
        g_native_back = widget;
        g_native_back_y = y;
        g_native_back_draw_y = Read<float>(widget, kWidgetDrawYOffset);
        Write<float>(widget, kWidgetYPositionOffset, 415.0F);
        Write<float>(widget, kWidgetDrawYOffset,
            g_native_back_draw_y + (415.0F - y));
    });
}

[[nodiscard]] bool InjectMenu(void* menu) noexcept {
    if (menu == nullptr || g_settings == nullptr || g_commit == nullptr ||
        g_injecting) return false;
    if (g_menu == menu && g_root != nullptr) return true;
    g_injecting = true;

    void* const init = Read<void*>(menu, 0x20);
    if (init == nullptr) {
        g_injecting = false;
        return false;
    }
    std::array<void*, kBlackPlagueVrMenuSettingCount> rows{};
    void* root = CreateButton(init, NativeVec3{220.0F, 378.0F, 40.0F},
        L"VR Settings", kRootSentinel, 25.0F);
    void* back = CreateButton(init, NativeVec3{400.0F, 500.0F, 40.0F},
        L"Back", kBackSentinel, 23.0F);
    bool complete = root != nullptr && back != nullptr;
    const auto& settings = BlackPlagueVrMenuSettings();
    for (std::size_t index = 0; complete && index < settings.size(); ++index) {
        const bool right = index >= 9;
        const std::size_t row = right ? index - 9 : index;
        const NativeVec3 position{
            right ? 600.0F : 200.0F,
            145.0F + static_cast<float>(row) * 38.0F,
            40.0F};
        rows[index] = CreateButton(init, position, RowText(settings[index]),
            kRowSentinelBase + static_cast<int>(index), 17.0F);
        complete = rows[index] != nullptr;
    }
    if (!complete) {
        DestroyUnownedButton(root);
        DestroyUnownedButton(back);
        for (void* row : rows) DestroyUnownedButton(row);
        g_injecting = false;
        return false;
    }

    g_menu = menu;
    g_root = root;
    g_back = back;
    g_rows = rows;
    g_page_active = false;
    MoveNativeBackForVrEntry(menu);

    auto* const add = reinterpret_cast<AddWidgetToState>(g_image + kAddWidgetToStateRva);
    Write<std::uint8_t>(g_root, kWidgetActiveOffset, 0);
    add(menu, kOptionsState, g_root);
    for (void* row : g_rows) {
        Write<std::uint8_t>(row, kWidgetActiveOffset, 0);
        add(menu, kOptionsState, row);
    }
    Write<std::uint8_t>(g_back, kWidgetActiveOffset, 0);
    add(menu, kOptionsState, g_back);
    SetWidgetActive(g_root, true);
    g_injecting = false;
    return true;
}

void ApplyPageState(bool page_active) noexcept {
    if (g_menu == nullptr) return;
    g_page_active = page_active;
    ForEachStateWidget(g_menu, kOptionsState, [page_active](void* widget) noexcept {
        if (widget == nullptr) return;
        bool desired = !page_active;
        if (widget == g_root) desired = !page_active;
        else if (widget == g_back || RowIndex(widget) >= 0) desired = page_active;
        SetWidgetActive(widget, desired);
    });
    if (page_active) RefreshRows();
}

void __fastcall HookedButtonMouseDown(void* widget, void*, int button) noexcept {
    if (!g_installed.load(std::memory_order_acquire) || !IsCustomWidget(widget)) {
        if (g_original_mouse_down != nullptr) g_original_mouse_down(widget, button);
        return;
    }
    if (widget == g_root) {
        if (button == kLeftMouse) ApplyPageState(true);
        return;
    }
    if (widget == g_back) {
        if (button == kLeftMouse) ApplyPageState(false);
        return;
    }
    const int index = RowIndex(widget);
    if (index < 0 || g_settings == nullptr || g_commit == nullptr) return;
    const int direction = button == kLeftMouse ? 1 :
        (button == kRightMouse ? -1 : 0);
    if (direction == 0) return;
    const auto id = BlackPlagueVrMenuSettings()[static_cast<std::size_t>(index)];
    const auto capabilities = BlackPlagueVrSettingCapabilities();
    if (!runtime::IsVrSettingAvailable(id, *g_settings, capabilities)) return;

    const runtime::VrSettings previous = *g_settings;
    if (!runtime::AdjustVrSetting(*g_settings, id, direction)) return;
    std::string error;
    if (!g_commit(*g_settings, error)) {
        *g_settings = previous;
    }
    RefreshRows();
}

void __fastcall HookedButtonActiveChanged(void* widget, void*) noexcept {
    if (g_original_active_changed == nullptr) return;
    if (!g_installed.load(std::memory_order_acquire)) {
        g_original_active_changed(widget);
        return;
    }
    void* const menu = MenuFromWidget(widget);
    const int state = menu == nullptr ? -1 :
        Read<int>(menu, kMenuCurrentStateOffset);
    if (state == kOptionsState && g_root == nullptr && !g_injecting) {
        static_cast<void>(InjectMenu(menu));
    }
    if (menu == g_menu) {
        if (state == kOptionsState &&
            Read<int>(menu, kMenuPreviousStateOffset) != kOptionsState) {
            // Native SetState deactivates the previous state's widgets by
            // writing +0x30 directly, so there is no virtual callback on exit.
            // A fresh transition back into Options therefore owns resetting
            // the Framework sub-page to its root entry.
            g_page_active = false;
        }
        if (state != kOptionsState) {
            g_page_active = false;
            if (IsCustomWidget(widget)) {
                Write<std::uint8_t>(widget, kWidgetActiveOffset, 0);
            }
        } else if (widget == g_root) {
            Write<std::uint8_t>(widget, kWidgetActiveOffset,
                g_page_active ? 0 : 1);
        } else if (widget == g_back || RowIndex(widget) >= 0) {
            Write<std::uint8_t>(widget, kWidgetActiveOffset,
                g_page_active ? 1 : 0);
        } else if (g_page_active) {
            Write<std::uint8_t>(widget, kWidgetActiveOffset, 0);
        }
    }
    g_original_active_changed(widget);
}

void AppendRemovalError(std::string& aggregate, const std::string& next) {
    if (next.empty()) return;
    if (!aggregate.empty()) aggregate += "; ";
    aggregate += next;
}

} // namespace

void ConfigureNativeVrSettingsMenu(
    runtime::VrSettings* settings,
    CommitNativeVrSettings commit) noexcept {
    g_settings = settings;
    g_commit = commit;
}

bool InstallNativeVrSettingsMenu(std::string& error) noexcept {
    error.clear();
    if (g_mouse_down_hook.installed() && g_active_changed_hook.installed()) {
        g_installed.store(true, std::memory_order_release);
        return true;
    }
    if (g_mouse_down_hook.installed() || g_active_changed_hook.installed()) {
        error = "Black Plague VR menu hooks are partially installed";
        return false;
    }
    if (g_settings == nullptr || g_commit == nullptr) {
        error = "Black Plague VR menu settings owner is not configured";
        return false;
    }
    g_image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (g_image == nullptr) {
        error = "Black Plague module base is unavailable";
        return false;
    }
    void** const mouse_slot = reinterpret_cast<void**>(g_image + kButtonMouseDownSlotRva);
    void** const active_slot = reinterpret_cast<void**>(g_image + kButtonActiveChangedSlotRva);
    void* const expected_mouse = g_image + kButtonMouseDownRva;
    void* const expected_active = g_image + kButtonActiveChangedRva;
    g_original_mouse_down = reinterpret_cast<ButtonMouseDown>(expected_mouse);
    g_original_active_changed = reinterpret_cast<ButtonActiveChanged>(expected_active);

    if (!hooks::InstallPointerHook(active_slot, expected_active,
            reinterpret_cast<void*>(&HookedButtonActiveChanged),
            g_active_changed_hook, error)) {
        return false;
    }
    if (!hooks::InstallPointerHook(mouse_slot, expected_mouse,
            reinterpret_cast<void*>(&HookedButtonMouseDown),
            g_mouse_down_hook, error)) {
        const std::string install_error = error;
        std::string rollback;
        static_cast<void>(hooks::RemoveIatHook(g_active_changed_hook, rollback));
        error = install_error;
        if (!rollback.empty()) error += "; rollback failed: " + rollback;
        return false;
    }
    g_installed.store(true, std::memory_order_release);
    return true;
}

bool RemoveNativeVrSettingsMenu(std::string& error) noexcept {
    error.clear();
    g_installed.store(false, std::memory_order_release);
    if (g_menu != nullptr) {
        ApplyPageState(false);
        RestoreNativeBackPosition();
        if (g_root != nullptr) Write<int>(g_root, kButtonTargetStateOffset, kSafeRemovedTargetState);
        if (g_back != nullptr) Write<int>(g_back, kButtonTargetStateOffset, kSafeRemovedTargetState);
        for (void* row : g_rows) {
            if (row != nullptr) Write<int>(row, kButtonTargetStateOffset, kSafeRemovedTargetState);
        }
    }

    bool success = true;
    std::string next;
    if (!hooks::RemoveIatHook(g_mouse_down_hook, next)) {
        success = false;
        AppendRemovalError(error, next);
    }
    next.clear();
    if (!hooks::RemoveIatHook(g_active_changed_hook, next)) {
        success = false;
        AppendRemovalError(error, next);
    }
    if (success) {
        g_menu = nullptr;
        g_root = nullptr;
        g_back = nullptr;
        g_native_back = nullptr;
        g_rows.fill(nullptr);
        g_page_active = false;
        g_injecting = false;
        g_image = nullptr;
        g_original_mouse_down = nullptr;
        g_original_active_changed = nullptr;
    }
    return success;
}

} // namespace penumbra_vr::backends::black_plague
