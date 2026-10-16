// Execute the actual six native font-call hooks, including x86 by-value vector
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
int RunMessageTest() {
    auto* image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if (!image) return 1;
    for (const auto site:kMessageFontCalls) {
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
    std::string error;
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
    // Other font callers (radio subtitles, menus) still reach native DrawWordWrap.
    reinterpret_cast<WordWrap>(image+kWordWrapRva)(nullptr,{40,470,47},720,16,
        {14,14},color.data(),0,legacy.data());
    if (test_position!=TextPosition{40,470,47} || test_size!=TextSize{14,14} || test_alignment!=0) return 10;
    if (!RemoveMessagePresentation(error)) return 7;
    for (const auto site:kMessageFontCalls) {
        if (std::memcmp(image+site,MessageCallBytes(site).data(),5)!=0) return 8;
    }
    VirtualFree(image,0,MEM_RELEASE); return 0;
}
}
int main() { return penumbra_vr::backends::black_plague::RunMessageTest(); }
