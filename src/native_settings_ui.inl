#include "localized_settings.h"
// Exact client SHA-256 pinned in loader.cpp. Facts checked against:
// ADE4C0 option row, B93D49 scrolling difficulty tab, B9323C scrollbar look.
// The native option row owns label/value layout, arrows, highlight, sounds and input.
constexpr size_t NativeOptionBytes=0xc50;
static bool buildNativeOption(unsigned char* config,const Text& label,const std::vector<Text>& choices,bool enabled) {
    if(choices.size()<2 || choices.size()>8) return false;
    memset(config,0,NativeOptionBytes);
    for(size_t i=0;i<choices.size();++i) {
        putAt<uint32_t>(config,i*0x30,static_cast<uint32_t>(i));
        putAt<Text>(config,i*0x30+8,choices[i]);
    }
    config[0xc00]=static_cast<unsigned char>(choices.size());
    putAt<Text>(config,0xc08,label);
    putAt<Text>(config,0xc20,text(""));
    config[0xc3a]=1; // ADE A0D: native arrow/input gate, required even when enabled
    config[0xc38]=1; // native explainer behavior
    config[0xc3c]=enabled?1:0;
    // c3b=0: no wrap; arrows disappear at the endpoints, as in difficulty.
    return true;
}
// B23C10 initializes the native buffer (including its inline-vector pointers).
// C9F4B0 renders it; format 6 calls D3FEA0, which converts ratios to percent.
constexpr size_t NativeSliderBytes=0x140;
static void configureNativePercent(unsigned char* config,const SettingSpec& spec) {
    putAt<float>(config,0x30,spec.minimum/100.0f);
    putAt<float>(config,0x34,spec.maximum/100.0f);
    config[0x38]=1; config[0x39]=0; config[0x3a]=1; config[0x3b]=0;
    putAt<uint32_t>(config,0x4c,6);
    putAt<uint32_t>(config,0x80,1);
    const int count=(spec.maximum-spec.minimum)/spec.step+1;
    putAt<uint64_t>(config,0x90,static_cast<uint64_t>(count));
    for(int i=0;i<count;++i) putAt<float>(config,0xb0+static_cast<size_t>(i)*4,(spec.minimum+i*spec.step)/100.0f);
}
struct NativeSettingsScroll {
    alignas(16) unsigned char bytes[0x600]{};
    bool ready=false;
    explicit NativeSettingsScroll(void* state) {
        alignas(16) unsigned char config[0x110]{},look[0xc0]{},nav[0x58]{};
        ui<void*(__fastcall*)(void*)>(0xa3eeb0)(config);
        ui<void*(__fastcall*)(void*)>(0xa3df20)(look);
        const auto bundle=uiBundle();
        memcpy(look+0x44,reinterpret_cast<const void*>(bundle+0xfa84),32);
        memcpy(look+0x24,reinterpret_cast<const void*>(bundle+0xfac4),32);
        putAt<uint64_t>(look,4,readAt<uint64_t>(reinterpret_cast<void*>(bundle),0xfb04));
        look[0x7c]=1; look[0x90]=1;
        for(const auto offset:{0x80,0x84,0x88}) putAt<float>(look,offset,24.0f);
        putAt<float>(look,0x8c,20.0f);
        memcpy(config+0x44,look,sizeof look);
        config[0x40]|=0x18; config[0x2e]=1; config[0x2f]=1; config[0x32]=1;
        putAt<uint32_t>(nav,0,3); nav[0x20]=1;
        putAt<uint32_t>(nav,0x24,3); putAt<uint16_t>(nav,0x28,0x101);
        putAt<uint32_t>(nav,0x30,2); putAt<uint16_t>(nav,0x34,0x1b); putAt<uint16_t>(nav,0x48,1);
        memcpy(nav+0x38,reinterpret_cast<const void*>(bundle+0x124d0),16);
        ui<void*(__fastcall*)(void*,unsigned char,void*,uintptr_t,const void*)>(0xa3e020)(bytes,1,state,0,config);
        ready=true;
        const auto focus=ui<float(__fastcall*)(void*)>(0xd4e3e0)(bytes);
        ui<void(__fastcall*)(void*,float)>(0xa518a0)(bytes,focus);
        const float stretch[2]={1,1}; uint64_t packed; memcpy(&packed,stretch,8);
        ui<void(__fastcall*)(uintptr_t,uint64_t)>(0x11cf360)(readAt<uintptr_t>(bytes,8),packed);
        ui<void(__fastcall*)(void*,const void*)>(0xa498b0)(bytes,nav);
        uint64_t input[2]{};
        ui<void*(__fastcall*)(void*,const void*,unsigned char)>(0xd4c3c0)(input,nav,0);
        ui<void(__fastcall*)(void*,const void*)>(0xa51690)(bytes,input);
    }
    ~NativeSettingsScroll() {if(ready) ui<void(__fastcall*)(void*)>(0xa40160)(bytes);}
    NativeSettingsScroll(const NativeSettingsScroll&)=delete;
    NativeSettingsScroll& operator=(const NativeSettingsScroll&)=delete;
};
static void renderNativeModSettings(const std::string& id) {
    auto& view=settingsViews.at(id);
    const auto language=gameLanguage();
    if(view.package.settings.empty()) {pageLabel(loaderText("ui.no_settings")); return;}
    if(!view.error.empty()) {pageLabel(view.error.c_str()); pageLabel(loaderText("ui.refresh_settings")); return;}
    // The native scroll borrows this plain 0x18-byte state; it stays alive between frames.
    static std::string scrollIdentity;
    alignas(16) static unsigned char scrollState[0x18]{};
    const auto identity=settingsWorld+":"+id;
    if(scrollIdentity!=identity) {scrollIdentity=identity; memset(scrollState,0,sizeof scrollState);}
    NativeFrame viewport("SettingsViewport"); viewport.stretch(1,1);
    {
        NativeSettingsScroll scroll(scrollState);
        NativeFrame content("SettingsRows",false,0); content.stretch(1,0);
        for(const auto& spec:view.package.settings) {
            const auto& translatedLabel=packageText(view.package,"setting."+spec.key,spec.label,language);
            const auto rowId="Setting_"+spec.key;
            NativeFrame row(rowId.c_str()); row.stretch(1,0);
            if(spec.percent) {
                alignas(16) unsigned char slider[NativeSliderBytes]{};
                const auto label=text(translatedLabel.c_str());
                ui<void*(__fastcall*)(void*,const Text*)>(0xb23c10)(slider,&label);
                configureNativePercent(slider,spec);
                float value=std::stoi(view.values.at(spec.key))/100.0f;
                if(ui<unsigned char(__fastcall*)(void*,float*,int*)>(0xc9f4b0)(slider,&value,nullptr)) {
                    // Snap native float roundoff back to the manifest's integer grid.
                    if(value>=spec.minimum/100.0f && value<=spec.maximum/100.0f) {
                        int index=static_cast<int>((value*100.0f-spec.minimum)/spec.step+0.5f);
                        changeSetting(id,spec.key,std::to_string(spec.minimum+index*spec.step));
                    }
                }
                if(!view.error.empty()) break;
                continue;
            }
            std::vector<Text> labels;
            if(spec.boolean) labels={text(localizedBoolean(false,language)),text(localizedBoolean(true,language))};
            else for(size_t i=0;i<spec.choices.size();++i)
                labels.push_back(text(packageText(view.package,"choice."+spec.key+"."+std::to_string(i),spec.choices[i],language).c_str()));
            const auto current=view.values.at(spec.key);
            unsigned char selected=spec.boolean?static_cast<unsigned char>(current=="true"):
                static_cast<unsigned char>(std::find(spec.choices.begin(),spec.choices.end(),current)-spec.choices.begin());
            alignas(16) unsigned char config[NativeOptionBytes]{};
            if(selected>=labels.size() || !buildNativeOption(config,text(translatedLabel.c_str()),labels,true)) continue;
            if(ui<unsigned char(__fastcall*)(void*,unsigned char*)>(0xade4c0)(config,&selected) && selected<labels.size()) {
                changeSetting(id,spec.key,spec.boolean?(selected?"true":"false"):spec.choices[selected]);
            }
            if(!view.error.empty()) break;
        }
    }
    if(!view.error.empty()) pageLabel(view.error.c_str());
}
