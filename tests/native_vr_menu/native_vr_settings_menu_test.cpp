// Executes the production widget lifecycle against a synthetic x86 native host.
#include <iostream>
#include <cmath>
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
const char* neural_status = "Unavailable";
runtime::VrSettings committed_settings;
std::unordered_map<const void*, std::wstring> strings;
std::unordered_map<void*, int> active_callbacks;
std::unordered_map<void*, backend::NativeVec3> positions;
std::vector<void*> allocations;
int allocation_budget = -1;
int destroyed = 0;
bool Commit(const penumbra_vr::runtime::VrSettings& settings, std::string&) noexcept {
    ++commits;
    if (save_ok) committed_settings = settings;
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
    const backend::NativeVec3* position, const void* text, int state,
    backend::NativeVec2, int) {
    backend::Write<void*>(object, 0, backend::g_image + backend::kButtonVtableRva);
    backend::Write<void*>(object, backend::kWidgetInitOffset, init);
    backend::Write<int>(object, backend::kButtonTargetStateOffset, state);
    strings[static_cast<std::uint8_t*>(object) + backend::kWidgetTextOffset] = strings.at(text);
    positions[object] = *position;
    return object;
}
void* __fastcall DeleteButton(void* object, void*, unsigned int) {
    ++destroyed;
    strings.erase(static_cast<std::uint8_t*>(object) + backend::kWidgetTextOffset);
    return object; // Fixture owns all allocated memory until its final cleanup.
}
void __fastcall ActiveChanged(void* widget, void*) { ++active_callbacks[widget]; }
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
void* StepControl(void* menu, void* row, bool increment) {
    void* result = nullptr;
    const auto position = positions.at(row);
    backend::ForEachStateWidget(menu, backend::kStartState, [&](void* widget) {
        const auto found = positions.find(widget);
        if (found != positions.end() && Active(widget) && Text(widget) == (increment ? L"+" : L"-") &&
            found->second.x == position.x + (increment ? 28.0F : -28.0F) &&
            found->second.y == position.y + 22.0F) result = widget;
    });
    return result;
}
int failures = 0;
void Check(bool result, const char* description) {
    if (!result) { ++failures; std::cerr << description << '\n'; }
}
}

namespace penumbra_vr::neural_post {
const char* StatusText() noexcept { return neural_status; }
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
    // Fail during the separate page, after every existing main-page row.
    const int neural_allocation_budget = static_cast<int>(6 + kNativeVrMenuSettingCount * 3 + 4);
    allocation_budget = neural_allocation_budget;
    const int before_neural_cleanup = destroyed;
    Check(!InjectMenu(menu.data()) && g_neural == nullptr &&
        destroyed == before_neural_cleanup + neural_allocation_budget,
        "partial Neural allocation rolls back every unowned widget");
    allocation_budget = -1;
    g_installed.store(true);
    ObserveNativeVrSettingsMenu(menu.data());
    Check(BoundMenuContainsRoot() && Active(g_root) && !Active(g_back),
        "startup Start page receives root without another SetState");
    Check(g_rows.size() == NativeVrMenuSettings().size(), "backend capability rows");
    for (void* row : g_neural_rows)
        Check(row != nullptr && !Active(row), "Neural rows stay hidden on root page");
    const auto caps = NativeVrSettingCapabilities();
    for (const auto id : NativeVrMenuSettings())
        Check(caps.supported[static_cast<std::size_t>(id)], "only consumed rows");
#ifndef PVR_TEST_BLACK_PLAGUE_MENU
    for (const auto id:{runtime::VrSettingId::render_scale,runtime::VrSettingId::subtitle_scale,
            runtime::VrSettingId::ui_scale,runtime::VrSettingId::ui_distance})
        Check(caps.supported[static_cast<std::size_t>(id)],
            "Requiem exposes its consumed subtitle/UI setting");
#endif
    auto* native = Allocate(kButtonObjectSize);
    Write<void*>(native, kWidgetInitOffset, init.data());
    AddWidget(menu.data(), nullptr, kStartState, native);
    SetWidgetActive(native, true);
    Click(g_root);
    Check(g_page_active && !Active(g_root) && Active(g_back) && !Active(native),
        "VR page hides native root widgets");
    for (void* row : g_rows) {
        auto* minus = StepControl(menu.data(), row, false);
        auto* plus = StepControl(menu.data(), row, true);
        Check(minus && plus && Active(minus) && Active(plus),
            "every VR setting has active trigger-clickable minus/plus controls");
    }
    std::size_t scale_row = 0;
    while (NativeVrMenuSettings()[scale_row] != runtime::VrSettingId::render_scale) ++scale_row;
    {
        const auto previous = settings;
        settings.render_scale = 1.0F;
        save_ok = false;
        Click(StepControl(menu.data(), g_rows[scale_row], false));
        Check(settings.render_scale == 1.0F, "RenderScale failed save rolls back");
        save_ok = true;
        Click(StepControl(menu.data(), g_rows[scale_row], false));
        Check(std::abs(settings.render_scale - 0.95F) < 0.0001F &&
            committed_settings.render_scale == settings.render_scale,
            "RenderScale controller decrement commits the actual multiplier");
        Click(StepControl(menu.data(), g_rows[scale_row], true));
        Check(std::abs(settings.render_scale - 1.0F) < 0.0001F,
            "RenderScale controller increment restores the multiplier");
        Check((std::wstring(SettingName(runtime::VrSettingId::render_scale)).find(L"restart") !=
            std::wstring::npos) == kNativeRenderScaleRequiresRestart,
            "RenderScale label matches backend application lifetime");
        settings = previous;
        commits = 0;
    }
    std::size_t height_row = 0;
    while (NativeVrMenuSettings()[height_row] != runtime::VrSettingId::player_height) ++height_row;
    if (void* minus = StepControl(menu.data(), g_rows[height_row], false)) {
        const auto previous = settings;
        settings.player_height = 1.8F;
        save_ok = false;
        Click(minus); // Controller select always delivers left mouse.
        Check(settings.player_height == 1.8F, "controller decrement failed save rolls back");
        save_ok = true;
        Click(minus);
        Check(settings.player_height < 1.8F && committed_settings.player_height == settings.player_height,
            "controller select decrements the actual VR setting and persists");
        Click(StepControl(menu.data(), g_rows[height_row], true));
        Check(std::abs(settings.player_height - 1.8F) < 0.0001F,
            "controller plus reverses the decrement");
        settings = previous;
        commits = 0;
    }
    void* neural_entry = nullptr;
    ForEachStateWidget(menu.data(), kStartState, [&](void* widget) {
        if (widget != native && Text(widget) == L"Neural") neural_entry = widget;
    });
    Check(neural_entry != nullptr && Active(neural_entry),
        "VR page exposes a separate Neural submenu");
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
#ifndef PVR_TEST_BLACK_PLAGUE_MENU
    struct UiSettingCase {
        runtime::VrSettingId id;
        float runtime::VrSettings::* field;
        const wchar_t* english;
        const wchar_t* spanish;
    };
    const UiSettingCase ui_settings[]{
        {runtime::VrSettingId::subtitle_scale,&runtime::VrSettings::subtitle_scale,
            L"Subtitle size",L"Tama\u00f1o de subt\u00edtulos"},
        {runtime::VrSettingId::ui_scale,&runtime::VrSettings::ui_scale,
            L"UI scale",L"Escala de interfaz"},
        {runtime::VrSettingId::ui_distance,&runtime::VrSettings::ui_distance,
            L"UI distance",L"Distancia de interfaz"},
    };
    settings.subtitle_scale=1.55F;
    for (const auto& test:ui_settings) {
        std::size_t row=0;
        while (row<NativeVrMenuSettings().size() && NativeVrMenuSettings()[row]!=test.id)
            ++row;
        Check(row<g_rows.size(), "Requiem consumed subtitle/UI widget exists");
        if (row>=g_rows.size()) continue;
        RefreshLocalizedText(MenuLanguage::spanish);
        Check(Text(g_rows[row]).find(test.spanish)!=std::wstring::npos,
            "Spanish subtitle/UI setting label");
        RefreshLocalizedText(MenuLanguage::english);
        Check(Text(g_rows[row]).find(test.english)!=std::wstring::npos,
            "English subtitle/UI setting label");
        const auto previous_value=settings.*test.field;
        const auto previous_commits=commits;
        save_ok=false;
        Click(g_rows[row]);
        Check(settings.*test.field==previous_value && commits==previous_commits+1,
            "subtitle/UI setting failed save preserves previous value");
        save_ok=true;
        Click(g_rows[row]);
        Check(settings.*test.field>previous_value && commits==previous_commits+2,
            "subtitle/UI setting edit persists through native widget callback");
    }
#endif
    Click(g_bindings);
    Check(Text(g_bindings) == L"Open bindings from SteamVR", "binding failure leaves usable page");
    // Execute all nine actual production widgets through the existing callback.
    const auto before_neural = settings;
    settings.neural.internal_height = 896;
    settings.neural.scale_percent = 50;
    settings.neural.intensity_percent = 60;
    settings.neural.passes = 2;
    settings.neural.style = 1;
    settings.neural.fovea_percent = 60;
    settings.neural.outer_scale_percent = 50;
    const int before_navigation_commits = commits;
    Click(g_neural, kRightMouse);
    Check(!g_neural_page_active, "right click does not enter Neural page");
    Click(g_neural);
    Check(g_page_active && g_neural_page_active && Active(g_back) &&
        Active(g_neural_status) && !Active(g_neural) && !Active(native) &&
        !Active(g_bindings) && !Active(g_calibrate_height),
        "Neural page retains Back and hides main-page/native controls");
    for (void* row : g_rows) Check(!Active(row), "main rows hidden on Neural page");
    Check(Text(g_neural_status) == L"Status: Unavailable",
        "status comes from bridge independently of saved enabled preference");
    struct NeuralCase {
        runtime::VrSettingId id;
        const wchar_t* english;
        const wchar_t* spanish;
    };
    const NeuralCase neural_cases[]{
        {runtime::VrSettingId::neural_enabled, L"Neural enabled", L"Neural activado"},
        {runtime::VrSettingId::neural_internal_height, L"Internal resolution (height)", L"Resoluci\u00f3n interna (alto)"},
        {runtime::VrSettingId::neural_scale, L"Processing scale", L"Escala de procesamiento"},
        {runtime::VrSettingId::neural_intensity, L"Intensity", L"Intensidad"},
        {runtime::VrSettingId::neural_passes, L"Passes", L"Pasadas"},
        {runtime::VrSettingId::neural_style, L"Style", L"Estilo"},
        {runtime::VrSettingId::neural_fovea, L"Central region", L"Zona central"},
        {runtime::VrSettingId::neural_outer_enabled, L"Peripheral treatment", L"Tratamiento perif\u00e9rico"},
        {runtime::VrSettingId::neural_outer_scale, L"Peripheral scale", L"Escala perif\u00e9rica"},
    };
    Check(g_neural_rows.size() == std::size(neural_cases), "all nine separate Neural rows exist");
    const auto neural_caps = NeuralSettingCapabilities();
    Check(std::count(neural_caps.supported.begin(), neural_caps.supported.end(), true) == 9,
        "dedicated Neural capability has exactly nine supported settings");
    for (std::size_t i = 0; i < std::size(neural_cases); ++i) {
        const auto& test = neural_cases[i];
        void* const row = g_neural_rows[i];
        Check(row != nullptr && Active(row), "Neural widget is active on its own page");
        void* minus = StepControl(menu.data(), row, false);
        void* plus = StepControl(menu.data(), row, true);
        Check(minus && plus && Active(minus) && Active(plus),
            "every Neural setting has active trigger-clickable minus/plus controls");
        Check(neural_caps.supported[static_cast<std::size_t>(test.id)],
            "each Neural setting is supported by both native consumers");
        Check(std::find(NativeVrMenuSettings().begin(), NativeVrMenuSettings().end(),
            test.id) == NativeVrMenuSettings().end(), "Neural does not extend main-page row list");
        RefreshLocalizedText(MenuLanguage::spanish);
        Check(Text(row).find(test.spanish) == 0, "Spanish Neural row label");
        RefreshLocalizedText(MenuLanguage::english);
        Check(Text(row).find(test.english) == 0, "English Neural row label");
        const auto previous_settings = settings;
        const auto previous_value = runtime::FormatVrSettingValue(test.id, settings);
        const int previous_commits = commits;
        Click(row, 1);
        Check(commits == previous_commits && settings.neural == previous_settings.neural,
            "unhandled mouse button does not edit or commit");
        save_ok = false;
        Click(row);
        Check(settings.neural == previous_settings.neural && commits == previous_commits + 1 &&
            Text(row) == RowText(test.id, neural_caps),
            "Neural failed save restores snapshot and displayed value");
        save_ok = true;
        Click(row);
        Check(runtime::FormatVrSettingValue(test.id, settings) != previous_value &&
            committed_settings.neural == settings.neural && commits == previous_commits + 2,
            "Neural left click commits the actual edited snapshot");
        Click(row, kRightMouse);
        Check(settings.neural == previous_settings.neural &&
            committed_settings.neural == settings.neural && commits == previous_commits + 3,
            "Neural right click reverses edit through existing save owner");
        if (minus && plus) {
            Click(plus);
            Click(minus);
            Check(settings.neural == previous_settings.neural &&
                committed_settings.neural == settings.neural,
                "controller-select plus/minus edits actual Neural consumer bidirectionally");
        }
        for (const auto& main_setting : runtime::VrSettingDescriptors()) {
            if (static_cast<std::size_t>(main_setting.id) >=
                static_cast<std::size_t>(runtime::VrSettingId::neural_enabled)) break;
            Check(runtime::FormatVrSettingValue(main_setting.id, settings) ==
                runtime::FormatVrSettingValue(main_setting.id, previous_settings),
                "Neural edits preserve existing main-page preferences");
        }
    }
    {
        const auto previous = settings.neural;
        void* minus = StepControl(menu.data(), g_neural_rows[2], false);
        if (minus) {
            settings.neural.scale_percent = 100;
            save_ok = false;
            Click(minus);
            Check(settings.neural.scale_percent == 100,
                "controller decrement at upper bound rolls back failed save");
            save_ok = true;
            Click(minus);
            Check(settings.neural.scale_percent == 95 && committed_settings.neural.scale_percent == 95,
                "controller decrement leaves saturated upper bound and persists");
            settings.neural.scale_percent = 25;
            const int before_lower = commits;
            Click(minus);
            Check(settings.neural.scale_percent == 25 && commits == before_lower + 1,
                "controller decrement preserves lower bound through existing save owner");
        }
        settings.neural = previous;
    }
    const int before_enabled_transitions = commits;
    settings.neural.enabled = false;
    Click(g_neural_rows[0]);
    Check(settings.neural.enabled && committed_settings.neural.enabled &&
        Text(g_neural_rows[0]) == L"Neural enabled: On",
        "actual Enabled widget commits disabled-to-enabled transition");
    save_ok = false;
    Click(g_neural_rows[0], kRightMouse);
    Check(settings.neural.enabled && committed_settings.neural.enabled &&
        Text(g_neural_rows[0]) == L"Neural enabled: On",
        "failed disable preserves enabled preference and row");
    save_ok = true;
    Click(g_neural_rows[0], kRightMouse);
    Check(!settings.neural.enabled && !committed_settings.neural.enabled &&
        Text(g_neural_rows[0]) == L"Neural enabled: Off" &&
        commits == before_enabled_transitions + 3,
        "actual Enabled widget commits enabled-to-disabled transition");
    const int before_status_click = commits;
    settings.neural.enabled = true;
    neural_status = "Starting";
    ObserveNativeVrSettingsMenu(menu.data());
    Check(Text(g_neural_status) == L"Status: Starting",
        "existing observer refreshes actual runtime status while page is visible");
    neural_status = "Active";
    RefreshNativeVrSettingsMenuStatus();
    Check(Text(g_neural_status) == L"Status: Active" && commits == before_status_click,
        "existing frame callback fan-out refreshes status without saving or clicking");
    neural_status = "Bypassed";
    RefreshLocalizedText(MenuLanguage::spanish);
    Check(Text(g_neural_status) == L"Estado: Omitido", "Spanish runtime status text");
    neural_status = "Unavailable";
    Click(g_neural_status);
    Check(Text(g_neural_status) == L"Estado: No disponible" && commits == before_status_click,
        "status control refreshes without saving or native delegation");
    RefreshLocalizedText(MenuLanguage::english);
    const int neural_deactivations = active_callbacks[g_neural_rows[0]];
    Click(g_back, kRightMouse);
    Check(g_neural_page_active, "right click does not navigate Back");
    Click(g_back);
    Check(g_page_active && !g_neural_page_active && Active(g_neural) && Active(g_back) &&
        Active(g_rows[0]) && !Active(g_neural_rows[0]) && !Active(g_neural_status) &&
        active_callbacks[g_neural_rows[0]] == neural_deactivations + 1,
        "Neural Back restores VR page through native activation callbacks");
    Check(commits == before_navigation_commits + 51,
        "page navigation and status never create settings transactions");
    settings = before_neural;
    Click(g_back);
    Check(!g_page_active && Active(g_root) && !Active(g_back) && Active(native), "Back restores root");
    Click(native);
    Check(native_clicks == 1, "native buttons still delegate");
    Click(g_root);
    Click(g_neural);
    HookedMenuSetState(menu.data(), nullptr, 1);
    Check(!g_page_active && !g_neural_page_active && !Active(g_root) &&
        !Active(g_neural_rows[0]), "native transition hides both custom pages");
    HookedMenuSetState(menu.data(), nullptr, 0);
    Check(Active(g_root) && !Active(g_back) && !Active(g_neural) &&
        !Active(g_neural_rows[0]), "pause return starts at root");
    // CreateWidgets may reuse the same cMainMenu and even a root button address.
    void* old_root = g_root;
    Write<int>(old_root, kButtonTargetStateOffset, 1);
    Check(!BoundMenuContainsRoot(), "recycled native address is not our root");
    Write<void*>(sentinels[0].data(), 0, sentinels[0].data());
    HookedMenuSetState(menu.data(), nullptr, 0);
    Check(g_root != old_root && BoundMenuContainsRoot() && Active(g_root),
        "recreated widgets rebind on Start");
    std::string error;
    Click(g_root);
    Click(g_neural);
    void* const removed_neural = g_neural;
    const auto removed_neural_rows = g_neural_rows;
    const auto removed_main_steps = g_row_steps;
    const auto removed_neural_steps = g_neural_row_steps;
    Check(RemoveNativeVrSettingsMenu(error), "hook removal");
    Check(g_root == nullptr && g_neural == nullptr && !g_neural_page_active &&
        !g_installed.load(), "removal drops both page bindings");
    Check(!Active(removed_neural) &&
        Read<int>(removed_neural, kButtonTargetStateOffset) == kSafeRemovedTargetState,
        "removed Neural entry retains a safe native target");
    for (void* row : removed_neural_rows)
        Check(!Active(row) && Read<int>(row, kButtonTargetStateOffset) == kSafeRemovedTargetState,
            "removed Neural widgets are hidden and have safe native targets");
    for (void* step : removed_main_steps)
        Check(!Active(step) && Read<int>(step, kButtonTargetStateOffset) == kSafeRemovedTargetState,
            "removed main row steps are hidden and have safe native targets");
    for (void* step : removed_neural_steps)
        Check(!Active(step) && Read<int>(step, kButtonTargetStateOffset) == kSafeRemovedTargetState,
            "removed Neural row steps are hidden and have safe native targets");
    for (void* allocation : allocations) HeapFree(GetProcessHeap(), 0, allocation);
    VirtualFree(image, 0, MEM_RELEASE);
    std::cout << "native VR menu failures=" << failures << '\n';
    return failures ? 1 : 0;
}
