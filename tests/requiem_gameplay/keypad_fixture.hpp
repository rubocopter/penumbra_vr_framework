#pragma once
// Actual bridge context/pointer/query hooks with characterized cGui boundary.
// No fabricated puzzle callbacks or hardcoded code are executed here.
namespace penumbra_vr::backends::requiem {
namespace {
std::array<float,2> keypad_point{},keypad_delta{};
unsigned keypad_down=0,keypad_up=0;
bool __fastcall KeypadDown(void*,void*,int button) {
    if(button!=1) return false;
    ++keypad_down; return true;
}
bool __fastcall KeypadUp(void*,void*,int button) {
    if(button!=1) return false;
    ++keypad_up; return true;
}
bool __fastcall KeypadPointer(void* gui,void*,const std::array<float,2>* point,
    const std::array<float,2>* delta) {
    keypad_point=*point; keypad_delta=*delta;
    auto* set=Read<void*>(gui,0x30);
    if(!set) return false;
    Put(set,0xD8,*point); return true;
}
bool KeypadPointerAndQueries(std::uint8_t* image) {
    std::array<std::uint8_t,0x80> gui{};
    std::array<std::uint8_t,0x120> set{},popup{};
    Put(gui.data(),0x30,static_cast<void*>(set.data()));
    Put(set.data(),0xD8,std::array<float,2>{400,300});
    Put(popup.data(),0xD8,std::array<float,2>{400,300});
    Branch(image,kGuiSendMousePos,0xE9,reinterpret_cast<void*>(&KeypadPointer));
    // Execute the actual mapped rel32 pointer owner with native thiscall ABI.
    for(unsigned i=0;i<2;++i) {
        const std::array<std::uint8_t,4> push{0xFF,0x74,0x24,0x08};
        std::memcpy(image+kGuiPointerSite-8+i*4,push.data(),push.size());
    }
    const auto native_call=CallBytes(kGuiPointerSite,kGuiSendMousePos);
    std::memcpy(image+kGuiPointerSite,native_call.data(),native_call.size());
    image[kGuiPointerSite+5]=0xC2; image[kGuiPointerSite+6]=8; image[kGuiPointerSite+7]=0;
    std::string error;
    if(!hooks::InstallRel32CallHook(image+kGuiPointerSite,native_call,
        reinterpret_cast<void*>(&HookedGuiPointer),g_gui_pointer_hook,error)) return false;
    using GuiPointer=bool(__thiscall*)(void*,const std::array<float,2>*,const std::array<float,2>*);
    const auto pointer=reinterpret_cast<GuiPointer>(image+kGuiPointerSite-8);
    Branch(image,0xE3170,0xE9,reinterpret_cast<void*>(&KeypadDown));
    Branch(image,0xE31C0,0xE9,reinterpret_cast<void*>(&KeypadUp));
    Branch(image,0xDAFB0,0xE9,reinterpret_cast<void*>(&NativeQueryStub));
    Branch(image,0xDAF10,0xE9,reinterpret_cast<void*>(&NativeQueryStub));
    // Native query call return addresses, real thiscall by-value string ABI.
    for(const unsigned site:{0x458AU,0x45CFU,0x4626U,0x466BU,0x473AU}) {
        image[site-3]=0x83; image[site-2]=0xEC; image[site-1]=28;
        Branch(image,site,0xE8,reinterpret_cast<void*>(&HookedQuery));
        image[site+5]=0xC2; image[site+6]=28; image[site+7]=0;
    }
    runtime::VrInputState input{};
    runtime::VrNativeIntents intents;
    g_intents=&intents; g_pointer_valid=true; g_pointer_uv={0.75F,0.75F};
    g_mouse_override_until=0;
    const std::array<float,2> physical{20,30},zero{};
    if(!pointer(gui.data(),&physical,&zero) ||
        !Near(keypad_point[0],480) || !Near(keypad_point[1],360) ||
        !Near(keypad_delta[0],80) || !Near(keypad_delta[1],60)) return false;
    // Focused popup receives its own cursor; do not write the root's cursor.
    Put(gui.data(),0x30,static_cast<void*>(popup.data()));
    if(!pointer(gui.data(),&physical,&zero) ||
        !Near(Read<float>(set.data(),0xD8),480) ||
        !Near(Read<float>(popup.data(),0xD8),480)) return false;
    // Physical mouse has priority, preserving original events unchanged.
    const std::array<float,2> moved{2,3};
    if(!pointer(gui.data(),&physical,&moved) ||
        keypad_point!=physical || keypad_delta!=moved) return false;
    g_mouse_override_until=0; g_pointer_valid=false;
    if(!pointer(gui.data(),&physical,&zero) || keypad_point!=physical) return false;
    g_pointer_valid=true;
    const auto query=[&](unsigned site) {return reinterpret_cast<Query>(image+site-3)(nullptr,LegacyString{});};
    using MouseButton=bool(__thiscall*)(void*,int);
    for(unsigned repeat=0;repeat<3;++repeat) {
        input={}; input.ui_select={true,true,true,false};
        intents.Begin(input,runtime::VrInputContext::ui);
        if(!query(0x4626) ||
            !reinterpret_cast<MouseButton>(image+0xE3170)(gui.data(),1) ||
            query(0x4626) || query(0x466B)) return false;
        input.ui_select={true,false,false,true};
        intents.Begin(input,runtime::VrInputContext::ui);
        if(query(0x4626) || !query(0x466B) ||
            !reinterpret_cast<MouseButton>(image+0xE31C0)(gui.data(),1) || query(0x466B)) return false;
    }
    // Losing focus while a native GUI button is down must release that button
    // once without inventing a new click when tracking/focus returns.
    runtime::VrControllerFrame frame{};
    frame.focused=true; frame.input.state.ui_select={true,true,true,false};
    frame.input.state.ui_back={true,true,true,false};
    intents.Begin(ScreenGuiInput(set.data(),frame,true,true),runtime::VrInputContext::ui);
    if(!query(0x4626) || !query(0x458A)) return false;
    frame.focused=false;
    intents.Begin(ScreenGuiInput(set.data(),frame,true,false),runtime::VrInputContext::ui);
    if(query(0x4626) || !query(0x466B) || query(0x466B) ||
        query(0x458A) || !query(0x45CF) || query(0x45CF)) return false;
    intents.Begin(ScreenGuiInput(set.data(),frame,true,false),runtime::VrInputContext::ui);
    if(query(0x466B) || query(0x45CF)) return false;
    frame.focused=true; frame.input.state.ui_select={true,false,false,false};
    frame.input.state.ui_back={true,false,false,false};
    intents.Begin(ScreenGuiInput(set.data(),frame,true,true),runtime::VrInputContext::ui);
    if(query(0x4626) || query(0x466B) || query(0x458A) || query(0x45CF)) return false;
    // Replacing/closing a set must not leak a stale release into the next GUI.
    frame.input.state.ui_select={true,true,true,false};
    frame.input.state.ui_back={true,true,true,false};
    static_cast<void>(ScreenGuiInput(set.data(),frame,true,true));
    frame.focused=false;
    const auto replaced=ScreenGuiInput(popup.data(),frame,false,false);
    if(replaced.ui_select.just_released || replaced.ui_back.just_released) return false;
    // Cancel/close has its own native Escape query, not a gameplay reset.
    input={}; input.ui_close={true,true,true,false};
    intents.Begin(input,runtime::VrInputContext::ui);
    if(!query(0x473A) || query(0x473A)) return false;
    intents.Begin({},runtime::VrInputContext::ui); // focus loss
    if(query(0x4626) || query(0x466B) || query(0x473A)) return false;
    input={}; input.interact={true,true,true,false};
    intents.Begin(input,runtime::VrInputContext::gameplay);
    if(query(0x4626) || query(0x466B)) return false;
    g_intents=nullptr; g_pointer_valid=false;
    if(!RemoveHookSet(error) || g_gui_pointer_hook.installed() ||
        !std::equal(native_call.begin(),native_call.end(),image+kGuiPointerSite)) return false;
    return keypad_down==3 && keypad_up==3;
}
} // namespace
} // namespace penumbra_vr::backends::requiem
