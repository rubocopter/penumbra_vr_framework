// Shared native-widget lifecycle for the two verified binary backends.
// Included inside each backend namespace; RVAs/layout/callbacks stay there.
namespace {

struct NativeVec2 { float x; float y; };
struct NativeVec3 { float x; float y; float z; };

using ButtonMouseDown = void (__thiscall*)(void*, int);
using ButtonActiveChanged = void (__thiscall*)(void*);
using ButtonConstructor = void* (__thiscall*)(
    void*, void*, const NativeVec3*, const void*, int, NativeVec2, int);
using ButtonDeletingDestructor = void* (__thiscall*)(void*, unsigned int);
using AddWidgetToState = void (__thiscall*)(void*, int, void*);
using MenuSetState = void (__thiscall*)(void*, int);
using GameAllocate = void* (__cdecl*)(std::size_t);
using WideStringConstructor = void (__thiscall*)(void*, const wchar_t*);
using WideStringDestructor = void (__thiscall*)(void*);
using WideStringAssign = void (__thiscall*)(void*, const wchar_t*);

std::uint8_t* g_image = nullptr;
hooks::IatHook g_mouse_down_hook;
hooks::IatHook g_active_changed_hook;
std::array<hooks::Rel32CallHook, kMenuSetStateCallsites.size()> g_menu_set_state_hooks{};
ButtonMouseDown g_original_mouse_down = nullptr;
ButtonActiveChanged g_original_active_changed = nullptr;
runtime::VrSettings* g_settings = nullptr;
CommitNativeVrSettings g_commit = nullptr;
void* g_menu = nullptr;
void* g_root = nullptr;
void* g_back = nullptr;
void* g_bindings = nullptr;
void* g_calibrate_height = nullptr;
std::array<void*, kNativeVrMenuSettingCount> g_rows{};
std::atomic<bool> g_installed{false};
bool g_page_active = false;
bool g_injecting = false;

[[nodiscard]] std::array<std::uint8_t, 5> CallBytes(
    std::uintptr_t site,
    std::uintptr_t target) noexcept {
    std::array<std::uint8_t, 5> bytes{0xE8};
    const auto displacement = static_cast<std::int32_t>(target - site - 5);
    std::memcpy(bytes.data() + 1, &displacement, sizeof(displacement));
    return bytes;
}

enum class MenuLanguage : std::uint8_t {
    english,
    spanish,
};

MenuLanguage g_language = MenuLanguage::english;

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
    if (widget == g_root || widget == g_back || widget == g_bindings ||
        widget == g_calibrate_height) return widget != nullptr;
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

[[nodiscard]] MenuLanguage ReadMenuLanguage() noexcept {
    wchar_t documents[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(
            nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents))) {
        return MenuLanguage::english;
    }
    const std::filesystem::path settings_path =
        std::filesystem::path(documents) / L"Penumbra" / kNativeSettingsDirectory /
        L"settings.cfg";
    std::ifstream input(settings_path, std::ios::binary);
    if (!input) return MenuLanguage::english;
    std::string text{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return text.find(kNativeSpanishLanguageSetting) != std::string::npos
        ? MenuLanguage::spanish
        : MenuLanguage::english;
}

[[nodiscard]] const wchar_t* RootText() noexcept {
    return g_language == MenuLanguage::spanish ? L"Ajustes de VR" : L"VR Settings";
}

[[nodiscard]] const wchar_t* BackText() noexcept {
    return g_language == MenuLanguage::spanish ? L"Volver" : L"Back";
}

[[nodiscard]] const wchar_t* BindingsText() noexcept {
    return g_language == MenuLanguage::spanish
        ? L"Bindings del mando (SteamVR)" : L"Controller bindings (SteamVR)";
}

[[nodiscard]] const wchar_t* CalibrateHeightText() noexcept {
    return g_language == MenuLanguage::spanish
        ? L"Calibrar altura" : L"Calibrate height";
}

[[nodiscard]] const wchar_t* CalibrationNoPoseText() noexcept {
    return g_language == MenuLanguage::spanish
        ? L"Sin pose reciente del visor" : L"No fresh HMD pose";
}

[[nodiscard]] const wchar_t* CalibrationInvalidText() noexcept {
    return g_language == MenuLanguage::spanish
        ? L"Altura del visor no válida" : L"Invalid HMD height";
}

[[nodiscard]] const wchar_t* CalibrationSaveFailedText() noexcept {
    return g_language == MenuLanguage::spanish
        ? L"No se pudo guardar la altura" : L"Could not save height";
}

[[nodiscard]] const wchar_t* SettingName(runtime::VrSettingId id) noexcept {
    using I = runtime::VrSettingId;
    if (g_language == MenuLanguage::spanish) {
        switch (id) {
        case I::handedness: return L"Mano dominante";
        case I::play_mode: return L"Modo de juego";
        case I::player_height: return L"Altura del jugador";
        case I::turn_mode: return L"Modo de giro";
        case I::snap_turn_angle: return L"Ángulo de giro";
        case I::smooth_turn_speed: return L"Velocidad de giro";
        case I::turn_dead_zone: return L"Zona muerta de giro";
        case I::move_speed: return L"Velocidad de movimiento";
        case I::move_dead_zone: return L"Zona muerta de movimiento";
        case I::crouch_mode: return L"Modo de agachado";
        case I::physical_crouch_depth: return L"Profundidad de agachado";
        case I::height_offset: return L"Ajuste de altura";
        case I::ui_distance: return L"Distancia de interfaz";
        case I::ui_scale: return L"Escala de interfaz";
        case I::subtitle_scale: return L"Tamaño de subtítulos";
        case I::render_scale: return L"Escala de renderizado (reinicio)";
        case I::enhanced_visuals: return L"Mejoras visuales";
        case I::hrtf: return L"HRTF (reinicio)";
        default: return L"Ajuste de VR";
        }
    }
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
    case I::subtitle_scale: return L"Subtitle size";
    case I::render_scale: return L"Render scale (restart)";
    case I::enhanced_visuals: return L"Enhanced visuals";
    case I::hrtf: return L"HRTF (restart)";
    default: return L"VR setting";
    }
}

[[nodiscard]] std::wstring WidenAscii(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

[[nodiscard]] std::wstring SettingValue(runtime::VrSettingId id) {
    if (g_language != MenuLanguage::spanish) {
        return WidenAscii(runtime::FormatVrSettingValue(id, *g_settings));
    }
    using I = runtime::VrSettingId;
    switch (id) {
    case I::handedness:
        return g_settings->handedness == runtime::VrHandedness::left
            ? L"Izquierda" : L"Derecha";
    case I::play_mode:
        return g_settings->play_mode == runtime::VrPlayMode::seated
            ? L"Sentado" : L"De pie";
    case I::turn_mode:
        switch (g_settings->turn_mode) {
        case runtime::VrTurnMode::disabled: return L"Desactivado";
        case runtime::VrTurnMode::snap: return L"Por pasos";
        case runtime::VrTurnMode::smooth: return L"Suave";
        }
        break;
    case I::crouch_mode:
        switch (g_settings->crouch_mode) {
        case runtime::VrCrouchMode::physical: return L"Físico";
        case runtime::VrCrouchMode::button: return L"Botón";
        case runtime::VrCrouchMode::hybrid: return L"Híbrido";
        }
        break;
    case I::enhanced_visuals:
        return g_settings->enhanced_visuals ? L"Activadas" : L"Desactivadas";
    case I::hrtf:
        switch (g_settings->hrtf_mode) {
        case runtime::VrHrtfMode::automatic: return L"Automático";
        case runtime::VrHrtfMode::on: return L"Activado";
        case runtime::VrHrtfMode::off: return L"Desactivado";
        }
        break;
    default:
        break;
    }
    return WidenAscii(runtime::FormatVrSettingValue(id, *g_settings));
}

[[nodiscard]] std::wstring RowText(runtime::VrSettingId id) {
    std::wstring result = SettingName(id);
    result += L": ";
    result += SettingValue(id);
    const auto capabilities = NativeVrSettingCapabilities();
    if (!runtime::IsVrSettingAvailable(id, *g_settings, capabilities)) {
        result += g_language == MenuLanguage::spanish
            ? L" [inactivo]" : L" [inactive]";
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
    const auto& settings = NativeVrMenuSettings();
    for (std::size_t index = 0; index < settings.size(); ++index) {
        if (g_rows[index] != nullptr) {
            AssignButtonText(g_rows[index], RowText(settings[index]));
        }
    }
}

void RefreshLocalizedText(MenuLanguage next_language = ReadMenuLanguage()) noexcept {
    if (next_language != g_language) g_language = next_language;
    if (g_root != nullptr) AssignButtonText(g_root, RootText());
    if (g_back != nullptr) AssignButtonText(g_back, BackText());
    if (g_bindings != nullptr) AssignButtonText(g_bindings, BindingsText());
    if (g_calibrate_height != nullptr) {
        AssignButtonText(g_calibrate_height, CalibrateHeightText());
    }
    RefreshRows();
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

void ResetBoundMenu() noexcept {
    g_menu = nullptr;
    g_root = nullptr;
    g_back = nullptr;
    g_bindings = nullptr;
    g_calibrate_height = nullptr;
    g_rows.fill(nullptr);
    g_page_active = false;
}

[[nodiscard]] bool BoundMenuContainsRoot() noexcept {
    if (g_menu == nullptr || g_root == nullptr) return false;
    bool present = false;
    // CreateWidgets can replace all widgets without replacing cMainMenu.
    // Inspect only a widget found in the live list. An allocator may recycle
    // the old address for a normal button, so membership alone is insufficient.
    ForEachStateWidget(g_menu, kStartState, [&present](void* widget) noexcept {
        if (widget == g_root) {
            present = NativeVrMenuRootBindingMatches(
                g_root, widget, Read<void*>(widget), g_image + kButtonVtableRva,
                Read<int>(widget, kButtonTargetStateOffset));
        }
    });
    return present;
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
    std::array<void*, kNativeVrMenuSettingCount> rows{};
    g_language = ReadMenuLanguage();
    void* root = CreateButton(init, NativeVec3{600.0F, 500.0F, 40.0F},
        RootText(), kRootSentinel, 25.0F);
    void* back = CreateButton(init, NativeVec3{400.0F, 500.0F, 40.0F},
        BackText(), kBackSentinel, 23.0F);
    void* bindings = CreateButton(init, NativeVec3{600.0F, 449.0F, 40.0F},
        BindingsText(), kBindingsSentinel, 17.0F);
    void* calibrate_height = CreateButton(init, NativeVec3{600.0F, 475.0F, 40.0F},
        CalibrateHeightText(), kCalibrateHeightSentinel, 17.0F);
    bool complete = root != nullptr && back != nullptr && bindings != nullptr &&
        calibrate_height != nullptr;
    const auto& settings = NativeVrMenuSettings();
    for (std::size_t index = 0; complete && index < settings.size(); ++index) {
        const bool right = index >= kNativeVrMenuRowsPerColumn;
        const std::size_t row = right ? index - kNativeVrMenuRowsPerColumn : index;
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
        DestroyUnownedButton(bindings);
        DestroyUnownedButton(calibrate_height);
        for (void* row : rows) DestroyUnownedButton(row);
        g_injecting = false;
        return false;
    }

    g_menu = menu;
    g_root = root;
    g_back = back;
    g_bindings = bindings;
    g_calibrate_height = calibrate_height;
    g_rows = rows;
    g_page_active = false;

    auto* const add = reinterpret_cast<AddWidgetToState>(g_image + kAddWidgetToStateRva);
    Write<std::uint8_t>(g_root, kWidgetActiveOffset, 0);
    add(menu, kStartState, g_root);
    for (void* row : g_rows) {
        Write<std::uint8_t>(row, kWidgetActiveOffset, 0);
        add(menu, kStartState, row);
    }
    Write<std::uint8_t>(g_back, kWidgetActiveOffset, 0);
    add(menu, kStartState, g_back);
    Write<std::uint8_t>(g_bindings, kWidgetActiveOffset, 0);
    add(menu, kStartState, g_bindings);
    Write<std::uint8_t>(g_calibrate_height, kWidgetActiveOffset, 0);
    add(menu, kStartState, g_calibrate_height);
    SetWidgetActive(g_root, true);
    g_injecting = false;
    return true;
}

void ApplyPageState(bool page_active) noexcept {
    if (g_menu == nullptr) return;
    RefreshLocalizedText();
    g_page_active = page_active;
    ForEachStateWidget(g_menu, kStartState, [page_active](void* widget) noexcept {
        if (widget == nullptr) return;
        bool desired = !page_active;
        if (widget == g_root) desired = !page_active;
        else if (widget == g_back || widget == g_bindings ||
            widget == g_calibrate_height || RowIndex(widget) >= 0) desired = page_active;
        SetWidgetActive(widget, desired);
    });
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
    if (widget == g_bindings) {
        if (button == kLeftMouse) {
            std::string error;
            if (!OpenNativeControllerBindings(error)) {
                AssignButtonText(widget, g_language == MenuLanguage::spanish
                    ? L"Abre los bindings desde SteamVR" : L"Open bindings from SteamVR");
            }
        }
        return;
    }
    if (widget == g_calibrate_height) {
        if (button != kLeftMouse || g_settings == nullptr || g_commit == nullptr) return;

        float tracked_height = 0.0F;
        if (!TrackedHeadTrackingHeightForCalibration(tracked_height)) {
            AssignButtonText(widget, CalibrationNoPoseText());
            return;
        }

        const runtime::VrSettings previous = *g_settings;
        if (!TryCalibratePlayerHeight(*g_settings, tracked_height)) {
            AssignButtonText(widget, CalibrationInvalidText());
            return;
        }

        std::string error;
        if (!g_commit(*g_settings, error)) {
            *g_settings = previous;
            AssignButtonText(widget, CalibrationSaveFailedText());
            RefreshRows();
            return;
        }
        RefreshRows();
        AssignButtonText(widget, CalibrateHeightText());
        return;
    }
    const int index = RowIndex(widget);
    if (index < 0 || g_settings == nullptr || g_commit == nullptr) return;
    const int direction = button == kLeftMouse ? 1 :
        (button == kRightMouse ? -1 : 0);
    if (direction == 0) return;
    const auto id = NativeVrMenuSettings()[static_cast<std::size_t>(index)];
    const auto capabilities = NativeVrSettingCapabilities();
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
    if (menu == g_menu) {
        if (state == kStartState &&
            Read<int>(menu, kMenuPreviousStateOffset) != kStartState) {
            // Native SetState deactivates the previous state's widgets by
            // writing +0x30 directly, so there is no virtual callback on exit.
            // A fresh transition back into Start therefore owns resetting
            // the Framework sub-page to its root entry.
            g_page_active = false;
        }
        if (state != kStartState) {
            g_page_active = false;
            if (IsCustomWidget(widget)) {
                Write<std::uint8_t>(widget, kWidgetActiveOffset, 0);
            }
        } else if (widget == g_root) {
            Write<std::uint8_t>(widget, kWidgetActiveOffset,
                g_page_active ? 0 : 1);
        } else if (widget == g_back || widget == g_bindings ||
            widget == g_calibrate_height || RowIndex(widget) >= 0) {
            Write<std::uint8_t>(widget, kWidgetActiveOffset,
                g_page_active ? 1 : 0);
        } else if (g_page_active) {
            Write<std::uint8_t>(widget, kWidgetActiveOffset, 0);
        }
    }
    g_original_active_changed(widget);
}

void __fastcall HookedMenuSetState(void* menu, void*, int next_state) noexcept {
    if (g_image == nullptr) return;
    const auto original = reinterpret_cast<MenuSetState>(g_image + kMenuSetStateRva);
    if (!g_installed.load(std::memory_order_acquire) || menu == nullptr) {
        original(menu, next_state);
        return;
    }

    // SetState is the common lifecycle boundary used by both front-end and
    // in-game menus. Reset the Framework sub-page before native activation so
    // ActiveChanged observes the root-page policy on a returning Start page.
    if (menu == g_menu && !BoundMenuContainsRoot()) ResetBoundMenu();
    if (menu == g_menu) g_page_active = false;

    const bool needs_rebind = NativeVrMenuNeedsRebindOnStateTransition(
        next_state, g_root != nullptr, menu == g_menu);
    if (needs_rebind && g_menu != nullptr && menu != g_menu) {
        ResetBoundMenu();
    }

    original(menu, next_state);

    // Inject after native SetState has finished iterating its state lists. The
    // root button is explicitly activated by InjectMenu, so the new entry is
    // immediately usable even when the pause route had no button callback.
    if (needs_rebind) static_cast<void>(InjectMenu(menu));
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

void ObserveNativeVrSettingsMenu(void* menu) noexcept {
    if (!g_installed.load(std::memory_order_acquire) || menu == nullptr ||
        Read<int>(menu, kMenuCurrentStateOffset) != kStartState) return;
    if (g_menu == menu && BoundMenuContainsRoot()) return;
    ResetBoundMenu();
    static_cast<void>(InjectMenu(menu));
}

bool InstallNativeVrSettingsMenu(std::string& error) noexcept {
    error.clear();
    const auto installed_state_hooks = static_cast<std::size_t>(std::count_if(
        g_menu_set_state_hooks.begin(), g_menu_set_state_hooks.end(),
        [](const hooks::Rel32CallHook& hook) noexcept { return hook.installed(); }));
    if (g_mouse_down_hook.installed() && g_active_changed_hook.installed() &&
        installed_state_hooks == g_menu_set_state_hooks.size()) {
        g_installed.store(true, std::memory_order_release);
        return true;
    }
    if (g_mouse_down_hook.installed() || g_active_changed_hook.installed() ||
        installed_state_hooks != 0) {
        error = "Native HPL VR menu hooks are partially installed";
        return false;
    }
    if (g_settings == nullptr || g_commit == nullptr) {
        error = "Native HPL VR menu settings owner is not configured";
        return false;
    }
    g_image = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    if (g_image == nullptr) {
        error = "Native HPL module base is unavailable";
        return false;
    }
    if (!ValidateNativeVrMenuContract(g_image, error)) return false;
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
    for (std::size_t index = 0; index < kMenuSetStateCallsites.size(); ++index) {
        const auto site = kMenuSetStateCallsites[index];
        if (hooks::InstallRel32CallHook(
                g_image + site, CallBytes(site, kMenuSetStateRva),
                reinterpret_cast<void*>(&HookedMenuSetState),
                g_menu_set_state_hooks[index], error)) {
            continue;
        }

        const std::string install_error = error;
        std::string rollback;
        for (auto& hook : g_menu_set_state_hooks) {
            std::string next;
            if (!hooks::RemoveRel32CallHook(hook, next)) AppendRemovalError(rollback, next);
        }
        {
            std::string next;
            if (!hooks::RemoveIatHook(g_mouse_down_hook, next)) AppendRemovalError(rollback, next);
        }
        {
            std::string next;
            if (!hooks::RemoveIatHook(g_active_changed_hook, next)) AppendRemovalError(rollback, next);
        }
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
    if (BoundMenuContainsRoot()) {
        if (Read<int>(g_menu, kMenuCurrentStateOffset) == kStartState)
            ApplyPageState(false);
        if (g_root != nullptr) Write<int>(g_root, kButtonTargetStateOffset, kSafeRemovedTargetState);
        if (g_back != nullptr) Write<int>(g_back, kButtonTargetStateOffset, kSafeRemovedTargetState);
        if (g_bindings != nullptr) Write<int>(g_bindings, kButtonTargetStateOffset, kSafeRemovedTargetState);
        if (g_calibrate_height != nullptr) {
            Write<int>(g_calibrate_height, kButtonTargetStateOffset, kSafeRemovedTargetState);
        }
        for (void* row : g_rows) {
            if (row != nullptr) Write<int>(row, kButtonTargetStateOffset, kSafeRemovedTargetState);
        }
    }

    bool success = true;
    std::string next;
    for (auto& hook : g_menu_set_state_hooks) {
        next.clear();
        if (!hooks::RemoveRel32CallHook(hook, next)) {
            success = false;
            AppendRemovalError(error, next);
        }
    }
    next.clear();
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
        ResetBoundMenu();
        g_injecting = false;
        g_image = nullptr;
        g_original_mouse_down = nullptr;
        g_original_active_changed = nullptr;
    }
    return success;
}

