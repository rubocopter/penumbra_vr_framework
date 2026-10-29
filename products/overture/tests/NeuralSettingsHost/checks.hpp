#include <cassert>
#include <climits>

using penumbra_vr::runtime::NeuralPostSettings;
static int Values(const NeuralPostSettings& s, int index) {
    const int values[] = {s.enabled, s.internal_height, s.scale_percent,
        s.intensity_percent, s.passes, s.style, s.fovea_percent, s.outer_enabled,
        s.outer_scale_percent};
    return values[index];
}

static cMainMenuWidget* StepControl(cMainMenuWidget* row, bool increment) {
    for(auto* widget : pageWidgets) {
        auto* button = dynamic_cast<cMainMenuWidget_Button*>(widget);
        if(button && button->msText == (increment ? L"+" : L"-") &&
            widget->mvPositon.x == row->mvPositon.x + (increment ? 28.0f : -28.0f) &&
            widget->mvPositon.y == row->mvPositon.y + 22.0f) return widget;
    }
    return nullptr;
}

static void CheckStepGeometry(cMainMenuWidget_Button* row, cMainMenuWidget* step) {
    auto* button = dynamic_cast<cMainMenuWidget_Button*>(step);
    assert(button);
    // Production Button hit rectangles start at y+3 and have font-height+3.
    assert(button->mvPositon.y + 3 > row->mvPositon.y + row->mvFontSize.y + 6);
    assert(button->mvPositon.y + button->mvFontSize.y + 6 < row->mvPositon.y + 43);
    assert(button->mvPositon.y + button->mvFontSize.y + 6 < 600);
}

int main() {
    cVRSettings settings;
    NeuralPostSettings defaults;
    assert(settings.GetNeuralSettings() == defaults);
    settings.Load(nullptr);
    settings.Save(nullptr);
    settings.SetMoveSpeed(2.0f);
    NeuralPostSettings requested;
    requested.enabled = true;
    requested.internal_height = 960;
    requested.scale_percent = 45;
    requested.intensity_percent = 75;
    requested.passes = 3;
    requested.style = 2;
    requested.fovea_percent = 50;
    requested.outer_enabled = true;
    requested.outer_scale_percent = 35;
    settings.SetNeuralSettings(requested);
    cConfigFile config;
    config.SetString("VR", "FutureSetting", "preserved");
    config.SetString("Other", "Key", "also preserved");
    settings.Save(&config);
    cVRSettings loaded;
    loaded.Load(&config);
    assert(loaded.GetNeuralSettings() == requested);
    assert(loaded.GetMoveSpeed() == 2.0f);
    assert(config.GetString("VR", "FutureSetting", "") == "preserved");
    assert(config.GetString("Other", "Key", "") == "also preserved");

    // Old profiles get default Neural settings and preserve deliberate old values.
    cConfigFile old;
    old.SetFloat("VR", "MoveSpeed", 1.75f);
    old.SetFloat("VR", "SmoothTurnSpeed", 180.0f);
    loaded.Load(&old);
    assert(loaded.GetNeuralSettings() == defaults);
    assert(loaded.GetMoveSpeed() == 1.75f && loaded.GetSmoothTurnSpeed() == 180.0f);

    const char* keys[] = {"NeuralInternalHeight", "NeuralScalePercent", "NeuralIntensityPercent",
        "NeuralPasses", "NeuralStyle", "NeuralFoveaPercent", "NeuralOuterScalePercent"};
    for(const char* malformed : {"junk", "", "2junk", "1.5", "999999999999999999999999999", "--4"}) {
        cConfigFile broken;
        for(const char* key : keys) broken.SetString("VR", key, malformed);
        loaded.Load(&broken);
        assert(loaded.GetNeuralSettings() == defaults);
    }
    const int minimum[] = {0, 64, 25, 0, 1, 0, 20, 0, 10};
    const int maximum[] = {1, 1280, 100, 100, 4, 2, 100, 1, 100};
    for(int boundary : {INT_MIN, INT_MAX}) {
        cConfigFile bounded;
        for(const char* key : keys) bounded.SetInt("VR", key, boundary);
        loaded.Load(&bounded);
        for(int i : {1, 2, 3, 4, 5, 6, 8})
            assert(Values(loaded.GetNeuralSettings(), i) == (boundary == INT_MIN ? minimum[i] : maximum[i]));
    }
    requested.internal_height = INT_MAX;
    requested.scale_percent = INT_MIN;
    settings.SetNeuralSettings(requested);
    assert(settings.GetNeuralSettings().internal_height == 1280);
    assert(settings.GetNeuralSettings().scale_percent == 25);

    assert(sizeof(gvMenuBackStates) / sizeof(gvMenuBackStates[0]) == eMainMenuState_LastEnum);
    assert(gvMenuBackStates[eMainMenuState_OptionsVRNeural] == eMainMenuState_OptionsVRSettings);
    assert(gvMenuBackStates[eMainMenuState_OptionsVRSettings] == eMainMenuState_Start);
    cInit init;
    CreateNeuralPage(&init);
    for(auto* widget : pageWidgets) assert(widget->mvPositon.y >= 0 && widget->mvPositon.y < 580);
    auto* back = dynamic_cast<cMainMenuWidget_Button*>(pageWidgets.back());
    assert(back && back->next == eMainMenuState_OptionsVRSettings);

    // Execute the actual native state switch: only current-page widgets are
    // active, and switching pages does not save, reset, or claim input twice.
    cMainMenu menu;
    cMainMenuWidget_Button original(&init, cVector3f(), L"Original VR control", eMainMenuState_Start, 16, eFontAlign_Left);
    menu.mlstWidgets.push_back(&original);
    menu.mvState[eMainMenuState_OptionsVRSettings].push_back(&original);
    for(auto* widget : pageWidgets) {
        menu.mlstWidgets.push_back(widget);
        menu.mvState[eMainMenuState_OptionsVRNeural].push_back(widget);
    }
    menu.SetState(eMainMenuState_OptionsVRNeural);
    assert(menu.mLastState == eMainMenuState_Start && !original.IsActive());
    for(auto* widget : pageWidgets) assert(widget->IsActive());
    init.mVRSettings.SetNeuralSettings(requested);
    const auto beforeSwitch = init.mVRSettings.GetNeuralSettings();
    menu.SetState(gvMenuBackStates[menu.mState]);
    assert(original.IsActive() && menu.mState == eMainMenuState_OptionsVRSettings);
    for(auto* widget : pageWidgets) assert(!widget->IsActive());
    menu.SetState(eMainMenuState_OptionsVRNeural);
    assert(init.commits == 0 && init.mVRSettings.GetNeuralSettings() == beforeSwitch);

    const int incremented[] = {1, 960, 30, 65, 2, 1, 100, 1, 30};
    for(int i = 0; i < 9; ++i) {
        const auto id = static_cast<eVRMenuSetting>(eVRMenuSetting_NeuralEnabled + i);
        init.mVRSettings.SetNeuralSettings(defaults);
        auto* button = gpVRSettingButtons[id];
        auto* minus = StepControl(button, false);
        auto* plus = StepControl(button, true);
        assert(minus && plus && "Neural rows need trigger-clickable minus/plus controls");
        CheckStepGeometry(button, minus);
        CheckStepGeometry(button, plus);
        const int before = init.commits;
        button->OnMouseDown(eMButton_Middle);
        assert(init.commits == before);
        button->SetEnabled(false);
        button->OnMouseDown(eMButton_Left);
        minus->OnMouseDown(eMButton_Left);
        plus->OnMouseDown(eMButton_Left);
        assert(init.commits == before);
        button->SetEnabled(true);
        minus->OnMouseDown(eMButton_Right);
        plus->OnMouseDown(eMButton_Middle);
        assert(init.commits == before);
        // Exercise the real Text::OnMouseDown forwarding to its native label widget.
        gpVRSettingTexts[id]->OnMouseDown(eMButton_Left);
        assert(init.commits == before + 1);
        assert(!gbMustRestart); // Neural edits retain live application semantics.
        assert(Values(init.mVRSettings.GetNeuralSettings(), i) == incremented[i]);
        loaded.Load(&init.config);
        assert(loaded.GetNeuralSettings() == init.mVRSettings.GetNeuralSettings());
        assert(gpVRSettingTexts[id]->msText == GetVRSettingText(&init, id));
        gpVRSettingTexts[id]->OnMouseDown(eMButton_Right);
        const int expected = i == 6 ? 95 : Values(defaults, i);
        assert(Values(init.mVRSettings.GetNeuralSettings(), i) == expected);
        const int beforeController = init.commits;
        plus->OnMouseDown(eMButton_Left);
        minus->OnMouseDown(eMButton_Left);
        assert(init.commits == beforeController + 2);
        assert(Values(init.mVRSettings.GetNeuralSettings(), i) == expected);
        for(int n = 0; n < 50; ++n) plus->OnMouseDown(eMButton_Left);
        int value = Values(init.mVRSettings.GetNeuralSettings(), i);
        assert(value >= minimum[i] && value <= maximum[i]);
        for(int n = 0; n < 50; ++n) minus->OnMouseDown(eMButton_Left);
        value = Values(init.mVRSettings.GetNeuralSettings(), i);
        assert(value >= minimum[i] && value <= maximum[i]);
        if(i != 0 && i != 5 && i != 7) assert(value == minimum[i]);
    }
    init.mVRSettings.SetNeuralSettings(defaults);
    gpVRSettingButtons[eVRMenuSetting_NeuralStyle]->OnMouseDown(eMButton_Right);
    assert(init.mVRSettings.GetNeuralSettings().style == 2);
    gpVRSettingButtons[eVRMenuSetting_NeuralStyle]->OnMouseDown(eMButton_Left);
    assert(init.mVRSettings.GetNeuralSettings().style == 0);
    gbMustRestart = true; // An unrelated graphics change can still require restart.
    gpVRSettingButtons[eVRMenuSetting_NeuralEnabled]->OnMouseDown(eMButton_Left);
    assert(gbMustRestart);
    cMainMenuWidget_NeuralStatus* status = nullptr;
    for(auto* widget : pageWidgets) {
        if(auto* candidate = dynamic_cast<cMainMenuWidget_NeuralStatus*>(widget)) status = candidate;
    }
    assert(status && status->msText.find(L"Unavailable") != tWString::npos);
    penumbra_vr::neural_post::status = "Initializing";
    status->OnUpdate(0.1f);
    assert(status->msText.find(L"Initializing") != tWString::npos);
    penumbra_vr::neural_post::status = "Active";
    status->OnUpdate(0.1f);
    assert(status->msText.find(L"Active") != tWString::npos);
    for(auto* widget : pageWidgets) delete widget;
    pageWidgets.clear();
    CreateMainPage(&init);
    for(auto* widget : pageWidgets) assert(widget->mvPositon.y >= 0 && widget->mvPositon.y < 550);
    init.mVRSettings = cVRSettings();
    for(int i = 0; i < eVRMenuSetting_NeuralEnabled; ++i) {
        const auto id = static_cast<eVRMenuSetting>(i);
        auto* button = gpVRSettingButtons[id];
        auto* minus = StepControl(button, false);
        auto* plus = StepControl(button, true);
        assert(minus && plus && "main VR rows need trigger-clickable minus/plus controls");
        CheckStepGeometry(button, minus);
        CheckStepGeometry(button, plus);
        const auto previous = GetVRSettingText(&init, id);
        const int before = init.commits;
        minus->OnMouseDown(eMButton_Left);
        assert(GetVRSettingText(&init, id) != previous);
        plus->OnMouseDown(eMButton_Left);
        assert(GetVRSettingText(&init, id) == previous && init.commits == before + 2);
        button->SetEnabled(false);
        minus->OnMouseDown(eMButton_Left);
        assert(init.commits == before + 2);
    }
    for(auto* widget : pageWidgets) delete widget;
    std::puts("Neural product persistence, normalization, native page and all nine bidirectional edits pass.");
}
