// Executes the production widget lifecycle against a synthetic x86 native host.
#include <iostream>
#include <unordered_map>
#include <vector>
#ifdef PVR_TEST_BLACK_PLAGUE_MENU
#include "../../src/backends/black_plague/native_vr_settings_menu.cpp"
namespace backend = penumbra_vr::backends::black_plague;
#else
#include "../../src/backends/requiem/native_vr_settings_menu.cpp"
namespace backend = penumbra_vr::backends::requiem;
#endif
namespace runtime = penumbra_vr::runtime;

namespace {
float tracked_height = 1.75F;
bool pose_available = true;
bool save_ok = true;
int commits = 0;
int native_clicks = 0;
std::unordered_map<const void*, std::wstring> strings;
std::vector<void*> allocations;
int allocation_budget = -1;
int destroyed = 0;
bool Commit(const penumbra_vr::runtime::VrSettings&, std::string&) noexcept {
    ++commits;
    return save_ok;
}
void* __cdecl Allocate(std::size_t size) {
    if (allocation_budget == 0) return nullptr;
    if (allocation_budget > 0) --allocation_budget;
    void* result = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
    allocations.push_back(result);
    return result;
}
void __fastcall ConstructString(void* object, void*, const wchar_t* text) {
    strings[object] = text;
}
void __fastcall DestroyString(void* object, void*) { strings.erase(object); }
void __fastcall AssignString(void* object, void*, const wchar_t* text) {
    strings[object] = text;
}
void* __fastcall ConstructButton(void* object, void*, void* init,
    const backend::NativeVec3*, const void* text, int state,
    backend::NativeVec2, int) {
    backend::Write<void*>(object, 0, backend::g_image + backend::kButtonVtableRva);
    backend::Write<void*>(object, backend::kWidgetInitOffset, init);
    backend::Write<int>(object, backend::kButtonTargetStateOffset, state);
    strings[static_cast<std::uint8_t*>(object) + backend::kWidgetTextOffset] = strings.at(text);
    return object;
}
void* __fastcall DeleteButton(void* object, void*, unsigned int) {
    ++destroyed;
    strings.erase(static_cast<std::uint8_t*>(object) + backend::kWidgetTextOffset);
    return object; // Fixture owns all allocated memory until its final cleanup.
}
void __fastcall ActiveChanged(void*, void*) {}
void __fastcall MouseDown(void*, void*, int) { ++native_clicks; }
void __fastcall AddWidget(void* menu, void*, int state, void* widget) {
    using namespace backend;
    auto* lists = Read<std::uint8_t*>(menu, kMenuStateListsOffset);
    void* sentinel = Read<void*>(lists + state * 12, 4);
    void* node = Allocate(12);
    Write<void*>(node, 0, Read<void*>(sentinel));
    Write<void*>(node, 4, sentinel);
    Write<void*>(node, 8, widget);
    Write<void*>(sentinel, 0, node);
}
void __fastcall SetState(void* menu, void*, int state) {
    using namespace backend;
    const int previous = Read<int>(menu, kMenuCurrentStateOffset);
    ForEachStateWidget(menu, previous, [](void* widget) {
        Write<std::uint8_t>(widget, kWidgetActiveOffset, 0);
    });
    Write<int>(menu, kMenuPreviousStateOffset, previous);
    Write<int>(menu, kMenuCurrentStateOffset, state);
    ForEachStateWidget(menu, state, [](void* widget) {
        Write<std::uint8_t>(widget, kWidgetActiveOffset, 1);
        HookedButtonActiveChanged(widget, nullptr);
    });
}
void Redirect(std::uintptr_t rva, void* function) {
    auto* site = backend::g_image + rva;
    site[0] = 0xE9;
    const auto displacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(function) - reinterpret_cast<std::uintptr_t>(site) - 5);
    std::memcpy(site + 1, &displacement, 4);
}
bool Active(void* widget) {
    return backend::Read<std::uint8_t>(widget, backend::kWidgetActiveOffset) != 0;
}
std::wstring Text(void* widget) {
    return strings.at(static_cast<std::uint8_t*>(widget) + backend::kWidgetTextOffset);
}
void Click(void* widget, int button = 0) {
    backend::HookedButtonMouseDown(widget, nullptr, button);
}
int failures = 0;
void Check(bool result, const char* description) {
    if (!result) { ++failures; std::cerr << description << '\n'; }
}
}

#ifdef PVR_TEST_BLACK_PLAGUE_MENU
namespace penumbra_vr::backends::black_plague {
bool TrackedHeadTrackingHeightForCalibration(float& height) noexcept {
    height = tracked_height; return pose_available;
}
bool OpenNativeControllerBindings(std::string&) noexcept { return false; }
}
#else
namespace penumbra_vr::backends::requiem {
bool TrackedHeadTrackingHeightForMenu(float& height) noexcept {
    height = tracked_height; return pose_available;
}
bool OpenNativeControllerBindings(std::string&) noexcept { return false; }
}
#endif

int main() {
    using namespace backend;
    g_image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x300000,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!g_image) return 1;
    auto* image = g_image;
    Redirect(kButtonConstructorRva, reinterpret_cast<void*>(&ConstructButton));
    Redirect(kButtonDeletingDestructorRva, reinterpret_cast<void*>(&DeleteButton));
    Redirect(kAddWidgetToStateRva, reinterpret_cast<void*>(&AddWidget));
    Redirect(kMenuSetStateRva, reinterpret_cast<void*>(&SetState));
    Redirect(kGameAllocateRva, reinterpret_cast<void*>(&Allocate));
    Write<void*>(image, kWideStringConstructorIatRva, reinterpret_cast<void*>(&ConstructString));
    Write<void*>(image, kWideStringDestructorIatRva, reinterpret_cast<void*>(&DestroyString));
    Write<void*>(image, kWideStringAssignIatRva, reinterpret_cast<void*>(&AssignString));
    g_original_active_changed = reinterpret_cast<ButtonActiveChanged>(&ActiveChanged);
    g_original_mouse_down = reinterpret_cast<ButtonMouseDown>(&MouseDown);
    runtime::VrSettings settings;
    ConfigureNativeVrSettingsMenu(&settings, &Commit);
    alignas(void*) std::array<std::uint8_t, 0x300> menu{}, init{};
    alignas(void*) std::array<std::uint8_t, 17 * 12> lists{};
    std::array<std::array<std::uint8_t, 12>, 17> sentinels{};
    for (std::size_t i = 0; i < sentinels.size(); ++i) {
        Write<void*>(sentinels[i].data(), 0, sentinels[i].data());
        Write<void*>(lists.data() + i * 12, 4, sentinels[i].data());
    }
    Write<void*>(menu.data(), 0x20, init.data());
    Write<void*>(menu.data(), kMenuStateListsOffset, lists.data());
    Write<void*>(init.data(), kInitMainMenuOffset, menu.data());
    // Failed injection must destroy every unowned widget and leave no binding.
    allocation_budget = 3;
    Check(!InjectMenu(menu.data()) && g_root == nullptr && destroyed == 3,
        "partial allocation rollback");
    allocation_budget = -1;
    g_installed.store(true);
    ObserveNativeVrSettingsMenu(menu.data());
    Check(BoundMenuContainsRoot() && Active(g_root) && !Active(g_back),
        "startup Start page receives root without another SetState");
    Check(g_rows.size() == NativeVrMenuSettings().size(), "backend capability rows");
    const auto caps = NativeVrSettingCapabilities();
    for (const auto id : NativeVrMenuSettings())
        Check(caps.supported[static_cast<std::size_t>(id)], "only consumed rows");
    auto* native = Allocate(kButtonObjectSize);
    Write<void*>(native, kWidgetInitOffset, init.data());
    AddWidget(menu.data(), nullptr, kStartState, native);
    SetWidgetActive(native, true);
    Click(g_root);
    Check(g_page_active && !Active(g_root) && Active(g_back) && !Active(native),
        "VR page hides native root widgets");
    g_language = MenuLanguage::spanish;
    RefreshLocalizedText(MenuLanguage::spanish);
    Check(Text(g_root) == L"Ajustes de VR" && Text(g_back) == L"Volver",
        "Spanish page labels");
    for (auto* row : g_rows) Check(!Text(row).empty(), "Spanish row labels");
    g_language = MenuLanguage::english;
    RefreshLocalizedText(MenuLanguage::english);
    Check(Text(g_root) == L"VR Settings" && Text(g_back) == L"Back",
        "English page labels");
    const auto before = settings.play_mode;
    std::size_t play_row = 0;
    for (; NativeVrMenuSettings()[play_row] != runtime::VrSettingId::play_mode; ++play_row) {}
    save_ok = false;
    Click(g_rows[play_row]);
    Check(settings.play_mode == before && commits == 1, "failed save rolls back settings");
    save_ok = true;
    Click(g_rows[play_row]);
    Check(settings.play_mode != before && commits == 2, "successful save applies settings");
    pose_available = false;
    Click(g_calibrate_height);
    Check(Text(g_calibrate_height) == L"No fresh HMD pose", "calibration rejects stale pose");
    pose_available = true;
    tracked_height = 0.5F;
    Click(g_calibrate_height);
    Check(Text(g_calibrate_height) == L"Invalid HMD height", "calibration rejects invalid height");
    tracked_height = 1.75F;
    Click(g_calibrate_height);
    Check(settings.player_height == tracked_height && commits == 3, "calibration saves fresh height");
    Click(g_bindings);
    Check(Text(g_bindings) == L"Open bindings from SteamVR", "binding failure leaves usable page");
    Click(g_back);
    Check(!g_page_active && Active(g_root) && !Active(g_back) && Active(native), "Back restores root");
    Click(native);
    Check(native_clicks == 1, "native buttons still delegate");
    Click(g_root);
    HookedMenuSetState(menu.data(), nullptr, 1);
    Check(!g_page_active && !Active(g_root), "native transition hides custom page");
    HookedMenuSetState(menu.data(), nullptr, 0);
    Check(Active(g_root) && !Active(g_back), "pause return starts at root");
    // CreateWidgets may reuse the same cMainMenu and even a root button address.
    void* old_root = g_root;
    Write<int>(old_root, kButtonTargetStateOffset, 1);
    Check(!BoundMenuContainsRoot(), "recycled native address is not our root");
    Write<void*>(sentinels[0].data(), 0, sentinels[0].data());
    HookedMenuSetState(menu.data(), nullptr, 0);
    Check(g_root != old_root && BoundMenuContainsRoot() && Active(g_root),
        "recreated widgets rebind on Start");
    std::string error;
    Check(RemoveNativeVrSettingsMenu(error), "hook removal");
    Check(g_root == nullptr && !g_installed.load(), "removal drops binding");
    for (void* allocation : allocations) HeapFree(GetProcessHeap(), 0, allocation);
    VirtualFree(image, 0, MEM_RELEASE);
    std::cout << "native VR menu failures=" << failures << '\n';
    return failures ? 1 : 0;
}
