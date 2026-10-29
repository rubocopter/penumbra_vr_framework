// Include inside the owning backend's namespace, after its verified native map.
// Shared VC7 font boundary and BP's established message readability behavior.
// RVAs, native signatures and VR/UI eligibility remain backend-owned.
namespace {
std::uint8_t* g_message_image=nullptr;
constexpr auto kPresentationFontCalls=[] {
    std::array<std::uintptr_t,kMessageFontCalls.size()+kSubtitleFontCalls.size()+kRadioFontCalls.size()> calls{};
    std::copy(kMessageFontCalls.begin(),kMessageFontCalls.end(),calls.begin());
    std::copy(kSubtitleFontCalls.begin(),kSubtitleFontCalls.end(),calls.begin()+kMessageFontCalls.size());
    std::copy(kRadioFontCalls.begin(),kRadioFontCalls.end(),calls.begin()+kMessageFontCalls.size()+kSubtitleFontCalls.size());
    return calls;
}();
std::array<hooks::Rel32CallHook,kPresentationFontCalls.size()+kRadioGuiCalls.size()+1> g_message_hooks;
std::atomic<bool> g_message_enabled{false};
std::atomic<unsigned> g_message_callbacks{0};
bool ReadMessageBytes(const void* source,void* destination,std::size_t size) noexcept {
    if (!source || !destination) return false;
    __try { std::memcpy(destination,source,size); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> T ReadMessageField(const void* object,std::size_t offset) noexcept {
    T value{};
    if (object) ReadMessageBytes(static_cast<const std::uint8_t*>(object)+offset,&value,sizeof(value));
    return value;
}
std::array<std::uint8_t,5> MessageCallBytes(std::uintptr_t site,std::uintptr_t target=kWordWrapRva) noexcept {
    std::array<std::uint8_t,5> bytes{0xE8};
    const auto delta=static_cast<std::int32_t>(target-site-5);
    std::memcpy(bytes.data()+1,&delta,sizeof(delta)); return bytes;
}
bool CopyMessageText(const void* legacy,std::wstring& text) {
    // Borrow VC7's initialized wstring layout; never construct/destroy it with
    // the modern CRT. Each mapped native DrawWordWrap verifies length +14,
    // capacity +18 and SSO data +4.
    const auto length=ReadMessageField<std::uint32_t>(legacy,0x14);
    const auto capacity=ReadMessageField<std::uint32_t>(legacy,0x18);
    if (!legacy || length>16384 || capacity<length) return false;
    const auto* characters=capacity<8
        ? reinterpret_cast<const wchar_t*>(static_cast<const std::uint8_t*>(legacy)+4)
        : ReadMessageField<const wchar_t*>(legacy,4);
    text.resize(length);
    return length==0 || ReadMessageBytes(characters,text.data(),length*sizeof(wchar_t));
}
std::size_t MessageRows(void* font,const std::wstring& text,float width,TextSize size) {
    using GetLength=float(__thiscall*)(void*,const TextSize*,const wchar_t*);
    const auto measure=reinterpret_cast<GetLength>(g_message_image+kFontLengthRva);
    // Rework 23c890f FontData::DrawWordWrap, counting its native line breaks
    // with the owning game's glyph measurement. No second invisible draw or foreign
    // vector/string allocation is needed to center the complete block.
    std::size_t first=0,last_space=0,rows=1;
    for (std::size_t pos=0;pos<text.size();++pos) {
        if (text[pos]!=L' ' && text[pos]!=L'\n') continue;
        bool unchanged=true;
        const auto token=text.substr(first,pos-first);
        if (measure(font,&size,token.c_str())>width) {
            ++rows; first=last_space+1; last_space=pos; unchanged=false;
        }
        if (text[pos]==L'\n') {
            last_space=pos; first=last_space+1; ++rows; unchanged=false;
        }
        if (unchanged) last_space=pos;
    }
    const auto tail=text.substr(first);
    if (measure(font,&size,tail.c_str())>width) ++rows;
    return rows;
}
int PresentMessageWordWrap(void* font,TextPosition position,
    float width,float line_height,TextSize size,const void* color,int alignment,
    const void* text,TextSize native_origin,bool narrative=false) {
    ++g_message_callbacks;
    struct Release { ~Release() { --g_message_callbacks; } } release;
    if (g_message_enabled.load() &&
        (narrative?TrackedStereoPresentationActive():MessagePresentationEligible()) &&
        std::isfinite(width) && width>0 && std::isfinite(line_height) && line_height>0 &&
        std::isfinite(size[0]) && size[0]>0 && std::isfinite(size[1]) && size[1]>0) {
        try {
            std::wstring value;
            if (CopyMessageText(text,value)) {
                const TextSize enlarged{size[0]*2.0F,size[1]*2.0F};
                const float spacing=line_height*2.0F;
                const auto rows=MessageRows(font,value,width,enlarged);
                const float height=enlarged[1]+static_cast<float>(rows-1)*spacing;
                // Each mapped channel has its own native origin. Keep every
                // shadow's pixel offset and Z layer around the same HUD center.
                position[0]+=400.0F-native_origin[0];
                position[1]+=300.0F-native_origin[1]-height*0.5F;
                size=enlarged; line_height=spacing; alignment=2;
            }
        } catch (...) {
            // Allocation/measurement failure retains the native draw payload.
        }
    }
    return reinterpret_cast<WordWrap>(g_message_image+kWordWrapRva)(
        font,position,width,line_height,size,color,alignment,text);
}
int __fastcall HookedMessageWordWrap(void* font,void*,TextPosition position,
    float width,float line_height,TextSize size,const void* color,int alignment,
    const void* text) {
    return PresentMessageWordWrap(font,position,width,line_height,size,color,alignment,text,{30,300});
}
int __fastcall HookedSubtitleWordWrap(void* font,void*,TextPosition position,
    float width,float line_height,TextSize size,const void* color,int alignment,
    const void* text) {
    return PresentMessageWordWrap(font,position,width,line_height,size,color,alignment,text,kSubtitleOrigin);
}
int __fastcall HookedRadioWordWrap(void* font,void*,TextPosition position,
    float width,float line_height,TextSize size,const void* color,int alignment,const void* text) {
    // A Radio draw is narrative even when a ScreenGui/video owns the panel.
    return PresentMessageWordWrap(font,position,width,line_height,size,color,alignment,text,{25,500},true);
}
using RadioRows=void(__thiscall*)(void*,float,float,TextSize,const void*,void*);
using RadioGuiFont=void(__thiscall*)(void*,const void*,void*,const TextPosition*,
    const TextSize*,const void*,int,int);
struct RadioRowPresentation {
    std::uintptr_t begin=0,end=0;
    float line=0,height=0;
};
thread_local RadioRowPresentation g_radio_rows;
void __fastcall HookedRadioRows(void* font,void*,float width,float line,TextSize size,
    const void* text,void* rows) {
    ++g_message_callbacks;
    struct Release { ~Release() { --g_message_callbacks; } } release;
    g_radio_rows={};
    const bool adapt=g_message_enabled.load() && TrackedStereoPresentationActive() &&
        std::isfinite(width) && width>0 && std::isfinite(line) && line>0 &&
        std::isfinite(size[0]) && size[0]>0 && std::isfinite(size[1]) && size[1]>0;
    // Native GetWordWrapRows owns every VC7 vector/string allocation and
    // cleanup. Reflow at the same 2x font as the later GUI draws.
    reinterpret_cast<RadioRows>(g_message_image+kRadioRowsRva)(font,width,
        adapt?line*2:line,adapt?TextSize{size[0]*2,size[1]*2}:size,text,rows);
    if (adapt) {
        const auto begin=ReadMessageField<std::uintptr_t>(rows,4);
        const auto end=ReadMessageField<std::uintptr_t>(rows,8);
        // Independently decoded native vector: begin+4, end+8, 28-byte wstring.
        if (begin && end>begin && (end-begin)%28==0 && (end-begin)/28<=16384) {
            const auto count=(end-begin)/28;
            g_radio_rows={begin,end,line,size[1]*2+static_cast<float>(count-1)*line*2};
        }
    }
}
void __fastcall HookedRadioGuiFont(void* gui,void*,const void* text,void* font,
    const TextPosition* position,const TextSize* size,const void* color,int alignment,int material) {
    ++g_message_callbacks;
    struct Release { ~Release() { --g_message_callbacks; } } release;
    TextPosition centered{}; TextSize enlarged{};
    const auto row=reinterpret_cast<std::uintptr_t>(text);
    if (g_message_enabled.load() && TrackedStereoPresentationActive() &&
        row>=g_radio_rows.begin && row<g_radio_rows.end && (row-g_radio_rows.begin)%28==0 &&
        ReadMessageBytes(position,&centered,sizeof(centered)) && ReadMessageBytes(size,&enlarged,sizeof(enlarged))) {
        const auto index=static_cast<float>((row-g_radio_rows.begin)/28);
        centered[0]+=375;
        centered[1]+=300-500-g_radio_rows.height*0.5F+index*g_radio_rows.line;
        enlarged[0]*=2; enlarged[1]*=2;
        position=&centered; size=&enlarged; alignment=2;
    }
    reinterpret_cast<RadioGuiFont>(g_message_image+kRadioGuiFontRva)(
        gui,text,font,position,size,color,alignment,material);
}
using EndingText=void(__thiscall*)(void*);
void __fastcall HookedEndingText(void* ending,void*) {
    ++g_message_callbacks;
    struct Release { ~Release() { --g_message_callbacks; } } release;
    if (g_message_enabled.load() && TrackedStereoPresentationActive() &&
        ReadMessageField<bool>(ending,kEndingTextActive)) {
        try {
            const auto begin=ReadMessageField<std::uintptr_t>(ending,kEndingRowsBegin);
            const auto end=ReadMessageField<std::uintptr_t>(ending,kEndingRowsEnd);
            auto* font=ReadMessageField<void*>(ending,kEndingFont);
            auto remaining=ReadMessageField<std::uint32_t>(ending,kEndingVisibleChars);
            if (font && begin && end>begin && (end-begin)%28==0 && (end-begin)/28<=16384) {
                std::wstring full,visible;
                bool prefix_complete=false,valid=true;
                for (auto row=begin;row<end;row+=28) {
                    std::wstring value;
                    if (!CopyMessageText(reinterpret_cast<const void*>(row),value) ||
                        full.size()+value.size()+1>16384) { valid=false; break; }
                    if (row!=begin) full+=L'\n';
                    full+=value;
                    if (!prefix_complete) {
                        if (row!=begin) visible+=L'\n';
                        visible+=value.substr(0,remaining);
                        if (remaining<=value.size()) prefix_complete=true;
                        else remaining-=static_cast<std::uint32_t>(value.size());
                    }
                }
                if (valid) {
                    // Keep the native character counter/rows entirely intact.
                    // Reflow only the displayed prefix; reserve the full block's
                    // height so revealing more characters never recenters it.
                    // The native ending is a whole page, unlike a short radio
                    // subtitle. Fit its complete reflow into the HUD before
                    // revealing characters, keeping the baseline stable.
                    TextSize size{30,30};
                    float line=34;
                    auto rows=MessageRows(font,full,750,size);
                    float height=size[1]+static_cast<float>(rows-1)*line;
                    if (height>540) {
                        float low=0,high=2;
                        for (unsigned step=0;step!=14;++step) {
                            const float scale=(low+high)*0.5F;
                            const TextSize candidate{15*scale,15*scale};
                            const auto count=MessageRows(font,full,750,candidate);
                            const float extent=candidate[1]+static_cast<float>(count-1)*17*scale;
                            if (extent<=540) low=scale; else high=scale;
                        }
                        size={15*low,15*low}; line=17*low;
                        rows=MessageRows(font,full,750,size);
                        height=size[1]+static_cast<float>(rows-1)*line;
                    }
                    const TextPosition position{400,300-height*0.5F,10};
                    const auto shade=ReadMessageField<float>(ending,kEndingShade);
                    const auto alpha=ReadMessageField<float>(ending,kEndingAlpha);
                    const std::array<float,4> color{shade,shade,shade,alpha};
                    // Read-only VC7 string view over scratch-owned characters.
                    // DrawWordWrap borrows it synchronously; neither CRT owns or
                    // destroys the other's allocation or constructs its objects.
                    std::array<std::uint8_t,28> view{};
                    const auto length=static_cast<std::uint32_t>(visible.size());
                    const auto capacity=std::max(7U,length);
                    if (length<8) std::memcpy(view.data()+4,visible.c_str(),(length+1)*sizeof(wchar_t));
                    else {
                        const auto* characters=visible.c_str();
                        std::memcpy(view.data()+4,&characters,sizeof(characters));
                    }
                    std::memcpy(view.data()+20,&length,4); std::memcpy(view.data()+24,&capacity,4);
                    reinterpret_cast<WordWrap>(g_message_image+kWordWrapRva)(
                        font,position,750,line,size,color.data(),2,view.data());
                    return;
                }
            }
        } catch (...) {
            // Invalid/unavailable native data or allocation keeps native owner.
        }
    }
    reinterpret_cast<EndingText>(g_message_image+kEndingTextRva)(ending);
}
bool InstallMessagePresentationAt(std::uint8_t* image,std::string& error) noexcept {
    error.clear();
    for (const auto site:kPresentationFontCalls) {
        std::array<std::uint8_t,5> bytes{};
        if (!ReadMessageBytes(image+site,bytes.data(),bytes.size()) || bytes!=MessageCallBytes(site)) {
            error=kMessageCallMismatch; return false;
        }
    }
    for (std::size_t i=0;i<kRadioGuiCalls.size();++i) {
        const auto site=kRadioGuiCalls[i],target=i==0?kRadioRowsRva:kRadioGuiFontRva;
        std::array<std::uint8_t,5> bytes{};
        if (!ReadMessageBytes(image+site,bytes.data(),bytes.size()) || bytes!=MessageCallBytes(site,target)) {
            error=kMessageCallMismatch; return false;
        }
    }
    std::array<std::uint8_t,5> ending_call{};
    if (!ReadMessageBytes(image+kEndingTextCall,ending_call.data(),ending_call.size()) ||
        ending_call!=MessageCallBytes(kEndingTextCall,kEndingTextRva)) {
        error=kMessageCallMismatch; return false;
    }
    std::array<std::uint8_t,10> actual{};
    if (!ReadMessageBytes(image+kFontLengthRva,actual.data(),actual.size()) || actual!=kFontLengthEntry) {
        error="GameMessage native font measurement ABI mismatch"; return false;
    }
    g_message_image=image;
    for (std::size_t i=0;i<kPresentationFontCalls.size();++i) {
        auto* callback=i<kMessageFontCalls.size()?&HookedMessageWordWrap:
            i<kMessageFontCalls.size()+kSubtitleFontCalls.size()?&HookedSubtitleWordWrap:&HookedRadioWordWrap;
        const auto site=kPresentationFontCalls[i];
        if (!hooks::InstallRel32CallHook(image+site,MessageCallBytes(site),
                reinterpret_cast<void*>(callback),g_message_hooks[i],error)) {
            std::string rollback;
            if (!RemoveMessagePresentation(rollback)) error+="; "+rollback;
            return false;
        }
    }
    for (std::size_t i=0;i<kRadioGuiCalls.size();++i) {
        const auto site=kRadioGuiCalls[i],target=i==0?kRadioRowsRva:kRadioGuiFontRva;
        void* callback=i==0?reinterpret_cast<void*>(&HookedRadioRows):reinterpret_cast<void*>(&HookedRadioGuiFont);
        if (!hooks::InstallRel32CallHook(image+site,MessageCallBytes(site,target),callback,
                g_message_hooks[kPresentationFontCalls.size()+i],error)) {
            std::string rollback;
            if (!RemoveMessagePresentation(rollback)) error+="; "+rollback;
            return false;
        }
    }
    if (!hooks::InstallRel32CallHook(image+kEndingTextCall,MessageCallBytes(kEndingTextCall,kEndingTextRva),
            reinterpret_cast<void*>(&HookedEndingText),g_message_hooks.back(),error)) {
        std::string rollback;
        if (!RemoveMessagePresentation(rollback)) error+="; "+rollback;
        return false;
    }
    g_message_enabled=true; return true;
}
}
bool MessagePresentationReady() noexcept {
    return g_message_enabled.load() &&
        std::all_of(g_message_hooks.begin(),g_message_hooks.end(),
            [](const auto& hook) { return hook.installed(); });
}
bool MessagePresentationHooksPresent() noexcept {
    return std::any_of(g_message_hooks.begin(),g_message_hooks.end(),
        [](const auto& hook) { return hook.installed(); });
}
bool InstallMessagePresentation(std::string& error) noexcept {
    if (MessagePresentationHooksPresent()) { error="GameMessage presentation is already installed"; return false; }
    return InstallMessagePresentationAt(reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr)),error);
}
bool RemoveMessagePresentation(std::string& error) noexcept {
    g_message_enabled=false; error.clear(); bool ok=true;
    for (auto& hook:g_message_hooks) {
        std::string detail;
        if (!hooks::RemoveRel32CallHook(hook,detail)) { ok=false; error+=detail+"; "; }
    }
    const auto deadline=GetTickCount64()+1000;
    while (g_message_callbacks.load() && GetTickCount64()<deadline) Sleep(1);
    if (g_message_callbacks.load()) { error+="GameMessage callbacks are still active"; ok=false; }
    return ok;
}
