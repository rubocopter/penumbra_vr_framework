// Included by both x86 consumer tests. Literal callsites belong to each test;
// these fixtures execute the actual patched CALL with the native stack ABI.
namespace {
std::wstring ending_draw_text;
std::array<float,4> ending_draw_color{};
int __fastcall TestEndingWordWrap(void*,void*,TextPosition pos,float width,float line,
    TextSize size,const void* color,int align,const void* text) {
    test_position=pos; test_width=width; test_line=line; test_size=size;
    test_alignment=align;
    CopyMessageText(text,ending_draw_text);
    std::memcpy(ending_draw_color.data(),color,sizeof(ending_draw_color));
    return 13;
}
unsigned native_ending_draws=0;
void __fastcall TestNativeEnding(void*,void*) { ++native_ending_draws; }
using TestRows = void(__thiscall*)(void*,float,float,TextSize,const void*,void*);
using TestGuiFont = void(__thiscall*)(void*,const void*,void*,const TextPosition*,
    const TextSize*,const void*,int,int);
std::array<std::uint8_t,84> narrative_rows{};
float narrative_wrap_width=0,narrative_wrap_line=0;
TextSize narrative_wrap_size{};
const void* narrative_wrap_text=nullptr;
void* narrative_gui=nullptr; void* narrative_font=nullptr;
int narrative_material=0;
void __fastcall TestNativeRows(void*,void*,float width,float line,TextSize size,
    const void* text,void* rows) {
    narrative_wrap_width=width; narrative_wrap_line=line;
    narrative_wrap_size=size; narrative_wrap_text=text;
    const auto begin=reinterpret_cast<std::uintptr_t>(narrative_rows.data());
    const auto end=begin+narrative_rows.size();
    std::memcpy(static_cast<std::uint8_t*>(rows)+4,&begin,4);
    std::memcpy(static_cast<std::uint8_t*>(rows)+8,&end,4);
}
void __fastcall TestNativeGuiFont(void* gui,void*,const void* text,void* font,
    const TextPosition* pos,const TextSize* size,const void* color,int align,int material) {
    narrative_gui=gui; narrative_font=font; narrative_material=material;
    test_position=*pos; test_size=*size; test_color=color;
    test_text=text; test_alignment=align;
}
std::array<std::uint8_t,5> NarrativeCallBytes(std::uintptr_t site,std::uintptr_t target) {
    std::array<std::uint8_t,5> bytes{0xE8};
    const auto delta=static_cast<std::int32_t>(target-site-5);
    std::memcpy(bytes.data()+1,&delta,4); return bytes;
}
// A separate native-target stub prevents a synthetic fixture depending on
// production constants before their independent exact-image verification.
void NarrativeCaller(std::uint8_t* image,std::uintptr_t site,unsigned words,std::uintptr_t target) {
    auto* caller=image+site-3-words*3;
    const std::uint8_t enter[]{0x55,0x8B,0xEC}; std::memcpy(caller,enter,3);
    for (unsigned i=0;i<words;++i) {
        caller[3+i*3]=0xFF; caller[4+i*3]=0x75;
        caller[5+i*3]=static_cast<std::uint8_t>(4+words*4-i*4);
    }
    const auto bytes=NarrativeCallBytes(site,target); std::memcpy(image+site,bytes.data(),5);
    const std::uint8_t leave[]{0x8B,0xE5,0x5D,0xC2,static_cast<std::uint8_t>(words*4),0};
    std::memcpy(image+site+5,leave,6);
}
std::uintptr_t narrative_real_rows=0,narrative_real_gui=0;
void PrepareNarrativeGuiFixture(std::uint8_t* image,std::uintptr_t rows,
    const std::array<std::uintptr_t,2>& gui) {
    // Independent BP/RQ target maps, selected by the independently supplied caller.
    narrative_real_rows=rows==0xB1C03?0xEC0C0:0xEBFB0;
    narrative_real_gui=rows==0xB1C03?0xFA460:0xFA4E0;
    NarrativeCaller(image,rows,6,narrative_real_rows);
    for (const auto site:gui) NarrativeCaller(image,site,7,narrative_real_gui);
    TestJump(image+narrative_real_rows,reinterpret_cast<void*>(&TestNativeRows));
    TestJump(image+narrative_real_gui,reinterpret_cast<void*>(&TestNativeGuiFont));
    const auto ending_site=rows==0xB1C03?0xDA82:0xDB92;
    const auto ending_target=rows==0xB1C03?0xD5B0:0xD6C0;
    NarrativeCaller(image,ending_site,0,ending_target);
    TestJump(image+ending_target,reinterpret_cast<void*>(&TestNativeEnding));
}
int CheckNarrativePresentation(std::uint8_t* image,
    const std::array<std::uintptr_t,4>& radio,std::uintptr_t rows_site,
    const std::array<std::uintptr_t,2>& gui) {
    const wchar_t* text=L"one\ntwo\nthree";
    const std::uint32_t count=13;
    std::array<std::uint8_t,28> legacy{};
    std::memcpy(legacy.data()+4,&text,4);
    std::memcpy(legacy.data()+20,&count,4); std::memcpy(legacy.data()+24,&count,4);
    const auto borrowed=legacy;
    std::array<float,4> color{1,1,1,0.35F};
    // Current/previous text and both shadows: native BP/RQ origin is 25/500,
    // unlike Rework's 40/470. Radio remains eligible during ScreenGui video.
    for (std::size_t i=0;i<radio.size();++i) {
        const auto draw=reinterpret_cast<WordWrap>(image+radio[i]-33);
        const float offset=i%2?2.0F:0.0F;
        const float depth=(rows_site==0xB1C03?47.0F:157.0F)-(i%2?1.0F:0.0F);
        for (const bool ui:{false,true}) {
            test_message_ui=ui;
            if (draw(nullptr,{25+offset,500+offset,depth},750,16,{15,15},
                    color.data(),0,legacy.data())!=13 ||
                test_position!=TextPosition{400+offset,253+offset,depth} ||
                test_size!=TextSize{30,30} || test_line!=32 || test_alignment!=2 ||
                test_color!=color.data() || test_text!=legacy.data() || legacy!=borrowed) {
                std::cerr<<"Narrative Radio current/previous subtitle is not centered at 2x\n";
                return 31;
            }
        }
        test_message_vr=false;
        draw(nullptr,{25,500,depth},750,16,{15,15},color.data(),0,legacy.data());
        if (test_position!=TextPosition{25,500,depth} || test_size!=TextSize{15,15}) return 32;
        test_message_vr=true;
    }
    const auto wrap=reinterpret_cast<TestRows>(image+rows_site-21);
    std::array<std::uint8_t,16> vector{};
    wrap(nullptr,750,16,{15,15},legacy.data(),vector.data());
    if (narrative_wrap_width!=750 || narrative_wrap_line!=32 ||
        narrative_wrap_size!=TextSize{30,30} || narrative_wrap_text!=legacy.data()) return 33;
    // Native owns row allocation and row text. GUI calls borrow those exact
    // rows, font, set, color, blend material and depth, including 1px shadow.
    for (std::size_t line=0;line<3;++line) for (std::size_t shadow=0;shadow<2;++shadow) {
        const auto draw=reinterpret_cast<TestGuiFont>(image+gui[shadow]-24);
        const float offset=static_cast<float>(shadow);
        const TextPosition pos{25+offset,500+16*static_cast<float>(line)+offset,30-offset};
        const TextSize size{15,15};
        const auto* row=narrative_rows.data()+line*28;
        draw(image,row,image+1,&pos,&size,color.data(),0,2);
        if (test_position!=TextPosition{400+offset,253+32*static_cast<float>(line)+offset,30-offset} ||
            test_size!=TextSize{30,30} || test_alignment!=2 || test_text!=row ||
            test_color!=color.data() || narrative_gui!=image || narrative_font!=image+1 ||
            narrative_material!=2 || size!=TextSize{15,15} || legacy!=borrowed) return 34;
    }
    test_message_vr=false;
    wrap(nullptr,750,16,{15,15},legacy.data(),vector.data());
    if (narrative_wrap_size!=TextSize{15,15} || narrative_wrap_line!=16) return 35;
    const TextPosition pos{25,500,30}; const TextSize size{15,15};
    reinterpret_cast<TestGuiFont>(image+gui[0]-24)(image,narrative_rows.data(),image+1,&pos,&size,color.data(),0,2);
    if (test_position!=pos || test_size!=size || test_alignment!=0) return 36;
    test_message_vr=true; test_message_ui=false;
    // Disabled teardown and unrecognized row pointers must pass through even
    // when a previous active Radio draw left a row view on this thread.
    test_message_ui=true;
    wrap(nullptr,750,16,{15,15},legacy.data(),vector.data());
    g_message_enabled=false;
    reinterpret_cast<TestGuiFont>(image+gui[0]-24)(image,narrative_rows.data(),image+1,&pos,&size,color.data(),0,2);
    if (test_position!=pos || test_size!=size || test_alignment!=0) return 37;
    g_message_enabled=true;
    reinterpret_cast<TestGuiFont>(image+gui[0]-24)(image,legacy.data(),image+1,&pos,&size,color.data(),0,2);
    if (test_position!=pos || test_size!=size || test_alignment!=0) return 38;
    // A saved SubtitleScale is applied by panel geometry; incoming native
    // size/spacing are multiplied, never replaced with fixed pixel defaults.
    reinterpret_cast<WordWrap>(image+radio[0]-33)(nullptr,{25,500,47},750,24,
        {22.5F,22.5F},color.data(),0,legacy.data());
    if (test_size!=TextSize{45,45} || test_line!=48 || test_position!=TextPosition{400,229.5F,47}) return 40;
    test_message_ui=false;
    return 0;
}
bool NarrativePreflightRejects(std::uint8_t* image,
    const std::array<std::uintptr_t,4>& radio,std::uintptr_t rows,
    const std::array<std::uintptr_t,2>& gui) {
    std::array<std::uintptr_t,8> sites{radio[0],radio[1],radio[2],radio[3],rows,gui[0],gui[1],
        rows==0xB1C03?0xDA82U:0xDB92U};
    for (const auto site:sites) {
        image[site]=0x90;
        std::string error;
        if (InstallMessagePresentationAt(image,error) || MessagePresentationHooksPresent()) return false;
        image[site]=0xE8;
        for (const auto message:kMessageFontCalls)
            if (std::memcmp(image+message,MessageCallBytes(message).data(),5)) return false;
    }
    return true;
}
bool NarrativeGuiRestored(std::uint8_t* image,std::uintptr_t rows,
    const std::array<std::uintptr_t,2>& gui) {
    if (std::memcmp(image+rows,NarrativeCallBytes(rows,narrative_real_rows).data(),5)) return false;
    for (const auto site:gui)
        if (std::memcmp(image+site,NarrativeCallBytes(site,narrative_real_gui).data(),5)) return false;
    const auto site=rows==0xB1C03?0xDA82:0xDB92;
    if (std::memcmp(image+site,NarrativeCallBytes(site,rows==0xB1C03?0xD5B0:0xD6C0).data(),5)) return false;
    return true;
}
int CheckEndingPresentation(std::uint8_t* image,std::uintptr_t rows_site) {
    const auto site=rows_site==0xB1C03?0xDA82:0xDB92;
    using Draw=void(__thiscall*)(void*);
    const auto draw=reinterpret_cast<Draw>(image+site-3);
    TestJump(image+kWordWrapRva,reinterpret_cast<void*>(&TestEndingWordWrap));
    FlushInstructionCache(GetCurrentProcess(),image,0x300000);
    std::array<std::uint8_t,0x80> ending{};
    std::array<std::uint8_t,56> rows{};
    const wchar_t first[]=L"hello",second[]=L"world";
    const std::uint32_t length=5,capacity=7;
    for (std::size_t i=0;i<2;++i) {
        std::memcpy(rows.data()+28*i+4,i==0?first:second,sizeof(first));
        std::memcpy(rows.data()+28*i+20,&length,4);
        std::memcpy(rows.data()+28*i+24,&capacity,4);
    }
    void* font=image+1;const auto begin=reinterpret_cast<std::uintptr_t>(rows.data()),end=begin+rows.size();
    const float shade=0.7F,alpha=0.35F;
    std::memcpy(ending.data()+0x28,&font,4);ending[0x30]=1;
    std::memcpy(ending.data()+0x38,&begin,4);std::memcpy(ending.data()+0x3C,&end,4);
    std::memcpy(ending.data()+0x54,&shade,4);std::memcpy(ending.data()+0x5C,&alpha,4);
    for (const std::uint32_t revealed:{0U,3U,5U,6U,10U,12U}) {
        std::memcpy(ending.data()+0x48,&revealed,4);
        const auto state=ending;const auto native_rows=rows;
        ending_draw_text=L"not called";
        test_message_ui=true;
        draw(ending.data());
        const std::wstring expected=revealed<=5?std::wstring(first,revealed):
            std::wstring(L"hello\n")+std::wstring(second,std::min(revealed-5,5U));
        if (ending_draw_text!=expected || test_size!=TextSize{30,30} || test_line!=34 ||
            test_alignment!=2 || test_position!=TextPosition{400,268,10} || test_width!=750 ||
            ending_draw_color!=std::array<float,4>{shade,shade,shade,alpha} || ending!=state || rows!=native_rows) {
            std::cerr<<"Native ending typewriter is not centered/enlarged with unchanged reveal state\n";
            return 42;
        }
    }
    // One formerly wrapped native row now needs two rows at 2x. Reflow only
    // presentation, without changing the native row or reveal counter.
    const wchar_t* long_text=L"aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa";
    const auto long_length=static_cast<std::uint32_t>(std::wcslen(long_text));
    std::memcpy(rows.data()+4,&long_text,4);
    std::memcpy(rows.data()+20,&long_length,4);std::memcpy(rows.data()+24,&long_length,4);
    const auto one_end=begin+28;const std::uint32_t eight=8;
    std::memcpy(ending.data()+0x3C,&one_end,4);std::memcpy(ending.data()+0x48,&eight,4);
    const auto before_reflow=ending;const auto before_rows=rows;
    draw(ending.data());
    if (ending_draw_text!=L"aaaa aaa" || test_position!=TextPosition{400,268,10} ||
        ending!=before_reflow || rows!=before_rows) return 44;
    // A complete typewriter page must stay inside the HUD, even when 2x
    // would make twenty explicit lines taller than its 600-pixel canvas.
    std::array<std::uint8_t,28*20> page{};
    for (std::size_t i=0;i<20;++i) {
        std::memcpy(page.data()+28*i+4,first,sizeof(first));
        std::memcpy(page.data()+28*i+20,&length,4);
        std::memcpy(page.data()+28*i+24,&capacity,4);
    }
    const auto page_begin=reinterpret_cast<std::uintptr_t>(page.data()),page_end=page_begin+page.size();
    const std::uint32_t all_chars=100;
    std::memcpy(ending.data()+0x38,&page_begin,4);std::memcpy(ending.data()+0x3C,&page_end,4);
    std::memcpy(ending.data()+0x48,&all_chars,4);
    const auto page_state=ending; const auto page_rows=page;
    draw(ending.data());
    const float bottom=test_position[1]+test_size[1]+19*test_line;
    if (test_position[1]<29.99F || bottom>570.01F || test_size[1]>30 ||
        test_size[1]<15 || ending!=page_state || page!=page_rows) {
        std::cerr<<"Long narrative page clips outside the centered HUD\n";
        return 45;
    }
    native_ending_draws=0;
    test_message_vr=false;draw(ending.data());
    test_message_vr=true;g_message_enabled=false;draw(ending.data());
    g_message_enabled=true;
    // Invalid row boundaries must retain the original native owner.
    const auto invalid=end+1;std::memcpy(ending.data()+0x3C,&invalid,4);draw(ending.data());
    if (native_ending_draws!=3) return 43;
    test_message_ui=false;
    TestJump(image+kWordWrapRva,reinterpret_cast<void*>(&TestNativeWordWrap));
    return 0;
}
}
