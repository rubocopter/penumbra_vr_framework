// Include inside the owning backend's namespace, after its verified native map.
// Shared VC7 font boundary and BP's accepted GameMessage-only readability path.
// RVAs, native signatures and VR/UI eligibility remain backend-owned.
namespace {
std::uint8_t* g_message_image=nullptr;
std::array<hooks::Rel32CallHook,kMessageFontCalls.size()> g_message_hooks;
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
std::array<std::uint8_t,5> MessageCallBytes(std::uintptr_t site) noexcept {
    std::array<std::uint8_t,5> bytes{0xE8};
    const auto delta=static_cast<std::int32_t>(kWordWrapRva-site-5);
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
int __fastcall HookedMessageWordWrap(void* font,void*,TextPosition position,
    float width,float line_height,TextSize size,const void* color,int alignment,
    const void* text) {
    ++g_message_callbacks;
    struct Release { ~Release() { --g_message_callbacks; } } release;
    if (g_message_enabled.load() && MessagePresentationEligible() &&
        std::isfinite(width) && width>0 && std::isfinite(line_height) && line_height>0 &&
        std::isfinite(size[0]) && size[0]>0 && std::isfinite(size[1]) && size[1]>0) {
        try {
            std::wstring value;
            if (CopyMessageText(text,value)) {
                const TextSize enlarged{size[0]*2.0F,size[1]*2.0F};
                const float spacing=line_height*2.0F;
                const auto rows=MessageRows(font,value,width,enlarged);
                const float height=enlarged[1]+static_cast<float>(rows-1)*spacing;
                // Native message origin is (30,300). Keep each shadow's own
                // +/-pixel offset and Z layer while centering all text lines.
                position[0]+=400.0F-30.0F;
                position[1]-=height*0.5F;
                size=enlarged; line_height=spacing; alignment=2;
            }
        } catch (...) {
            // Allocation/measurement failure retains the native draw payload.
        }
    }
    return reinterpret_cast<WordWrap>(g_message_image+kWordWrapRva)(
        font,position,width,line_height,size,color,alignment,text);
}
bool InstallMessagePresentationAt(std::uint8_t* image,std::string& error) noexcept {
    error.clear();
    for (const auto site:kMessageFontCalls) {
        std::array<std::uint8_t,5> bytes{};
        if (!ReadMessageBytes(image+site,bytes.data(),bytes.size()) || bytes!=MessageCallBytes(site)) {
            error=kMessageCallMismatch; return false;
        }
    }
    std::array<std::uint8_t,10> actual{};
    if (!ReadMessageBytes(image+kFontLengthRva,actual.data(),actual.size()) || actual!=kFontLengthEntry) {
        error="GameMessage native font measurement ABI mismatch"; return false;
    }
    g_message_image=image;
    for (std::size_t i=0;i<g_message_hooks.size();++i) {
        if (!hooks::InstallRel32CallHook(image+kMessageFontCalls[i],MessageCallBytes(kMessageFontCalls[i]),
                reinterpret_cast<void*>(&HookedMessageWordWrap),g_message_hooks[i],error)) {
            std::string rollback;
            if (!RemoveMessagePresentation(rollback)) error+="; "+rollback;
            return false;
        }
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
