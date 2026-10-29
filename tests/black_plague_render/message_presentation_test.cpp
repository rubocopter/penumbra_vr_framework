// Execute the mapped native message/subtitle hooks, including x86 by-value vector
// arguments. The fake font measures deterministic glyph widths; it never owns
// the borrowed legacy wstring or changes native fade/color/shadow state.
#include "../../src/backends/black_plague/message_presentation.cpp"
#include <iostream>
namespace penumbra_vr::backends::black_plague {
bool test_message_vr=true,test_message_ui=false;
bool TrackedStereoPresentationActive() noexcept { return test_message_vr; }
bool NativeInputUiActive() noexcept { return test_message_ui; }
namespace {
TextPosition test_position{}; TextSize test_size{};
float test_width=0,test_line=0; int test_alignment=0;
const void* test_color=nullptr; const void* test_text=nullptr;
int __fastcall TestNativeWordWrap(void*,void*,TextPosition pos,float width,float line,
    TextSize size,const void* color,int alignment,const void* text) {
    test_position=pos; test_width=width; test_line=line; test_size=size;
    test_color=color; test_alignment=alignment; test_text=text; return 13;
}
float __fastcall TestNativeLength(void*,void*,const TextSize* size,const wchar_t* text) {
    return static_cast<float>(std::wcslen(text))*(*size)[0]*0.5F;
}
void TestJump(std::uint8_t* source,void* target) {
    source[0]=0xE9;
    const auto delta=static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(target)-reinterpret_cast<std::uintptr_t>(source+5));
    std::memcpy(source+1,&delta,4);
}
}
#include "../native_narrative_presentation_test.inl"
int RunMessageTest() {
    auto* image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if (!image) return 1;
    constexpr std::array<std::uintptr_t,14> fixture_calls{
        0x3F3E8,0x3F452,0x3F49D,0x3F4E8,0x3F533,0x3F57E,
        0xD2A6,0xD2F6,0xD346,0x7AF9A,0xB1EBB,0xB1F00,0xB1F9E,0xB200E};
    for (const auto site:fixture_calls) {
        auto* caller=image+site-33;
        const std::array<std::uint8_t,3> enter{0x55,0x8B,0xEC};
        std::memcpy(caller,enter.data(),3);
        for (unsigned i=0;i<10;++i) {
            caller[3+i*3]=0xFF; caller[4+i*3]=0x75;
            caller[5+i*3]=static_cast<std::uint8_t>(44-i*4);
        }
        const auto bytes=MessageCallBytes(site); std::memcpy(image+site,bytes.data(),5);
        const std::array<std::uint8_t,6> leave{0x8B,0xE5,0x5D,0xC2,0x28,0};
        std::memcpy(image+site+5,leave.data(),leave.size());
    }
    constexpr std::array<std::uint8_t,10> signature{0x8B,0x54,0x24,0x08,0xD9,0x05,0xC0,0x29,0x67,0};
    std::memcpy(image+kFontLengthRva,signature.data(),signature.size());
    constexpr std::array<std::uintptr_t,4> radio_calls{0xB1EBB,0xB1F00,0xB1F9E,0xB200E};
    constexpr std::uintptr_t radio_rows=0xB1C03;
    constexpr std::array<std::uintptr_t,2> radio_gui{0xB1CB9,0xB1D21};
    PrepareNarrativeGuiFixture(image,radio_rows,radio_gui);
    std::string error;
    for (const auto site:{0xD2A6U,0xD2F6U,0xD346U}) {
        image[site]=0x90;
        if (InstallMessagePresentationAt(image,error) || MessagePresentationHooksPresent()) return 23;
        for (const auto message:kMessageFontCalls) {
            if (std::memcmp(image+message,MessageCallBytes(message).data(),5)!=0) return 24;
        }
        image[site]=0xE8;
    }
    if (!NarrativePreflightRejects(image,radio_calls,radio_rows,radio_gui)) return 41;
    if (!InstallMessagePresentationAt(image,error)) { std::cerr<<error; return 2; }
    TestJump(image+kWordWrapRva,reinterpret_cast<void*>(&TestNativeWordWrap));
    TestJump(image+kFontLengthRva,reinterpret_cast<void*>(&TestNativeLength));
    FlushInstructionCache(GetCurrentProcess(),image,0x300000);
    std::array<std::uint8_t,28> legacy{};
    const wchar_t* text=L"one\ntwo\nthree";
    const auto count=static_cast<std::uint32_t>(std::wcslen(text));
    std::memcpy(legacy.data()+4,&text,sizeof(text));
    std::memcpy(legacy.data()+20,&count,4); std::memcpy(legacy.data()+24,&count,4);
    std::array<float,4> color{1,1,1,0.35F};
    for (const auto site:kMessageFontCalls) {
        auto call=reinterpret_cast<WordWrap>(image+site-33);
        if (call(nullptr,{30,300,152},740,16,{14,14},color.data(),0,legacy.data())!=13) return 3;
        if (test_size!=TextSize{28,28} || test_line!=32 || test_alignment!=2 ||
            test_position!=TextPosition{400,254,152} || test_width!=740 ||
            test_color!=color.data() || test_text!=legacy.data()) {
            std::cerr<<"Examine text is not centered at 2x with preserved native payload\n"; return 4;
        }
        call(nullptr,{31,301,150},740,16,{14,14},color.data(),0,legacy.data());
        if (test_position!=TextPosition{401,255,150}) return 5;
        for (bool ui:{false,true}) {
            test_message_ui=ui; test_message_vr=ui;
            call(nullptr,{30,300,152},740,16,{14,14},color.data(),0,legacy.data());
            if (test_size!=TextSize{14,14} || test_position!=TextPosition{30,300,152} || test_alignment!=0) return 6;
        }
        test_message_ui=false; test_message_vr=true;
    }
    // Native word wrapping must be measured again at the doubled font size,
    // including VC7's short-string representation and ordinary space breaks.
    legacy={};
    const wchar_t small[]=L"aa aa";
    std::memcpy(legacy.data()+4,small,sizeof(small));
    const std::uint32_t small_length=5,small_capacity=7;
    std::memcpy(legacy.data()+20,&small_length,4);
    std::memcpy(legacy.data()+24,&small_capacity,4);
    const auto original_legacy=legacy;
    const auto call=reinterpret_cast<WordWrap>(image+kMessageFontCalls[0]-33);
    call(nullptr,{30,300,152},60,16,{14,14},color.data(),0,legacy.data());
    if (test_position!=TextPosition{400,270,152} || legacy!=original_legacy) return 9;
    // These independently mapped cEffect_SubTitle callers own transient script
    // messages, separate from Examine/GameMessage and Radio/menu rendering.
    constexpr std::array<std::uintptr_t,3> subtitle_calls{0xD2A6,0xD2F6,0xD346};
    const wchar_t* dead=L"It's dead.";
    const std::uint32_t dead_length=10;
    std::array<std::uint8_t,28> subtitle{};
    std::memcpy(subtitle.data()+4,&dead,sizeof(dead));
    std::memcpy(subtitle.data()+20,&dead_length,4);
    std::memcpy(subtitle.data()+24,&dead_length,4);
    const auto borrowed_subtitle=subtitle;
    for (std::size_t i=0;i<subtitle_calls.size();++i) {
        const auto draw=reinterpret_cast<WordWrap>(image+subtitle_calls[i]-33);
        const float offset=i==1?1.0F:i==2?-1.0F:0.0F;
        const float depth=i==0?47.0F:46.0F;
        if (draw(nullptr,{25+offset,500+offset,depth},750,16,{15,15},
                color.data(),0,subtitle.data())!=13) return 19;
        if (test_position!=TextPosition{400+offset,285+offset,depth} ||
            test_size!=TextSize{30,30} || test_line!=32 || test_alignment!=2 ||
            test_width!=750 || test_color!=color.data() ||
            test_text!=subtitle.data() || subtitle!=borrowed_subtitle) {
            std::cerr<<"Transient subtitle is not centered/readable with native payload preserved\n";
            return 20;
        }
        draw(nullptr,{25+offset,500+offset,depth},60,16,{15,15},color.data(),0,legacy.data());
        if (test_position!=TextPosition{400+offset,269+offset,depth} ||
            legacy!=original_legacy) return 21;
        auto multiline=subtitle;
        std::memcpy(multiline.data()+4,&text,sizeof(text));
        std::memcpy(multiline.data()+20,&count,4);
        std::memcpy(multiline.data()+24,&count,4);
        draw(nullptr,{25+offset,500+offset,depth},750,16,{15,15},color.data(),0,multiline.data());
        if (test_position!=TextPosition{400+offset,253+offset,depth}) return 25;
        const std::uint32_t invalid_length=16385;
        std::memcpy(multiline.data()+20,&invalid_length,4);
        draw(nullptr,{25,500,depth},750,16,{15,15},color.data(),0,multiline.data());
        if (test_position!=TextPosition{25,500,depth} || test_size!=TextSize{15,15} ||
            test_line!=16 || test_alignment!=0 || test_text!=multiline.data()) return 26;
        for (unsigned mode=0;mode<3;++mode) {
            test_message_vr=mode!=0; test_message_ui=mode==1;
            g_message_enabled=mode!=2;
            draw(nullptr,{25,500,47},750,16,{15,15},color.data(),0,subtitle.data());
            if (test_position!=TextPosition{25,500,47} || test_size!=TextSize{15,15} ||
                test_alignment!=0 || test_line!=16) return 22;
        }
        g_message_enabled=true; test_message_vr=true; test_message_ui=false;
    }
    // Menu callers still reach native DrawWordWrap.
    for (const auto site:{0x7AF9AU}) {
        reinterpret_cast<WordWrap>(image+site-33)(nullptr,{40,470,47},720,16,
            {14,14},color.data(),0,legacy.data());
        if (test_position!=TextPosition{40,470,47} || test_size!=TextSize{14,14} ||
            test_alignment!=0 || std::memcmp(image+site,MessageCallBytes(site).data(),5)!=0) return 10;
    }
    if (const int narrative=CheckNarrativePresentation(image,radio_calls,radio_rows,radio_gui)) return narrative;
    if (const int ending=CheckEndingPresentation(image,radio_rows)) return ending;
    if (!RemoveMessagePresentation(error)) return 7;
    if (!NarrativeGuiRestored(image,radio_rows,radio_gui)) return 39;
    for (const auto site:fixture_calls) {
        if (std::memcmp(image+site,MessageCallBytes(site).data(),5)!=0) return 8;
    }
    VirtualFree(image,0,MEM_RELEASE); return 0;
}
}
int main() { return penumbra_vr::backends::black_plague::RunMessageTest(); }
