using namespace hpl;
#define _W(x) L##x
#define hplNew(Type, Args) new Type Args
inline tWString Translate(const char*, const char* key) { return cString::To16Char(key); }
#define kTranslate(section, key) Translate(section, key)

namespace penumbra_vr { namespace neural_post {
static const char* status = "Unavailable";
inline const char* StatusText() noexcept { return status; }
} }

struct cVector2f { float x, y; cVector2f(float v) : x(v), y(v) {} };
struct cVector3f { float x, y, z; cVector3f(float a = 0, float b = 0, float c = 0) : x(a), y(b), z(c) {} };
struct cColor { template<class... T> cColor(T...) {} };
enum eFontAlign { eFontAlign_Left, eFontAlign_Right, eFontAlign_Center };
enum eMButton { eMButton_Left, eMButton_Right, eMButton_Middle };
struct FontStub { template<class... T> void Draw(T...) {} };

class cInit {
public:
    cVRSettings mVRSettings;
    cConfigFile config;
    int commits = 0;
    void ApplyVRSettings(bool save) { if(save) { ++commits; mVRSettings.Save(&config); } }
};

class cMainMenuWidget {
public:
    cInit* mpInit;
    cVector3f mvPositon;
    bool mbOver = false;
    bool active = true;
    cMainMenuWidget(cInit* init, cVector3f pos) : mpInit(init), mvPositon(pos) {}
    virtual ~cMainMenuWidget() {}
    virtual void OnMouseDown(eMButton) {}
    virtual void OnUpdate(float) {}
    void SetActive(bool value) { active = value; }
    bool IsActive() { return active; }
};
class cMainMenuWidget_Button : public cMainMenuWidget {
public:
    tWString msText, msTip;
    FontStub font;
    FontStub* mpFont = &font;
    cVector2f mvFontSize;
    eFontAlign mAlignment;
    int next;
    cMainMenuWidget_Button(cInit* init, cVector3f pos, tWString text, int target, cVector2f size, eFontAlign align)
        : cMainMenuWidget(init, pos), msText(text), mvFontSize(size), mAlignment(align), next(target) {}
    void OnMouseOver(bool) {}
    void OnDraw() {}
};
class cMainMenuWidget_Text : public cMainMenuWidget {
public:
    tWString msText;
    cMainMenuWidget* mpExtra;
    cMainMenuWidget_Text(cInit* init, cVector3f pos, tWString text, cVector2f, eFontAlign, cMainMenuWidget* extra = nullptr)
        : cMainMenuWidget(init, pos), msText(text), mpExtra(extra) {}
    void OnMouseDown(eMButton button);
    void SetTextColor(cColor) {}
};
// Unrelated engine-facing one-shot controls are layout-only in this host.
class cMainMenuWidget_RecenterButton : public cMainMenuWidget_Button {
public:
    cMainMenuWidget_RecenterButton(cInit* init, cVector3f pos, tWString text, cVector2f size, eFontAlign align)
        : cMainMenuWidget_Button(init, pos, text, eMainMenuState_LastEnum, size, align) {}
};
using cMainMenuWidget_CalibrateHeight = cMainMenuWidget_RecenterButton;
static std::vector<cMainMenuWidget*> pageWidgets;
static void AddWidgetToState(int, cMainMenuWidget* widget) { pageWidgets.push_back(widget); }
using tMainMenuWidgetList = std::list<cMainMenuWidget*>;
using tMainMenuWidgetListIt = tMainMenuWidgetList::iterator;
class cMainMenu {
public:
    eMainMenuState mState = eMainMenuState_Start;
    eMainMenuState mLastState = eMainMenuState_Start;
    tMainMenuWidgetList mlstWidgets;
    std::vector<tMainMenuWidgetList> mvState{eMainMenuState_LastEnum};
    void SetState(eMainMenuState);
};
static bool gbMustRestart = false;
