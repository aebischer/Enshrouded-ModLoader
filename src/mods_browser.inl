// Native browser building blocks, verified on client SHA-256 ExpectedHash.
// Sources of layout facts: c5ef50 (screen), cf70d0 (server list), c87950
// (details). No background bitmap or opaque fullscreen rectangle is added.
struct NativeFrame {
    alignas(16) unsigned char bytes[0x30]{}; // native extent 0x23
    NativeFrame(const char* id,bool horizontal=false,float gap=0) {
        const auto name=text(id);
        ui<void*(__fastcall*)(void*,const Text*,float)>(horizontal?0xa3cf70:0xa3c740)(bytes,&name,gap);
    }
    NativeFrame(const NativeFrame&)=delete;
    NativeFrame& operator=(const NativeFrame&)=delete;
    ~NativeFrame() { ui<void(__fastcall*)(void*)>(0xa40080)(bytes); }
    uintptr_t node() const { return readAt<uintptr_t>(bytes,8); }
    void width(float value) {ui<void(__fastcall*)(uintptr_t,float)>(0x11ceea0)(node(),value);}
    void height(float value) {ui<void(__fastcall*)(uintptr_t,float)>(0x11cee20)(node(),value);}
    void stretch(float x,float y) {
        const float values[2]={x,y}; uint64_t packed; memcpy(&packed,values,8);
        ui<void(__fastcall*)(uintptr_t,uint64_t)>(0x11cf360)(node(),packed);
    }
    void align(uint16_t value) {ui<void(__fastcall*)(uintptr_t,const uint16_t*)>(0x11ced70)(node(),&value);}
    void padding(float x,float y) {
        const float values[4]={x,y,x,y};
        ui<void(__fastcall*)(uintptr_t,const float*)>(0x11cf210)(node(),values);
    }
    void panel() {
        // Same type 1 / alpha 1 as the server detail pane at c87c41.
        // Its textures and their translucency come from the game's look bundle.
        alignas(16) unsigned char config[0xc0]{};
        putAt<uint32_t>(config,0,1); putAt<float>(config,4,1.0f);
        ui<void(__fastcall*)(void*,const void*)>(0xd027e0)(bytes,config);
    }
};

static uintptr_t uiContext() {return *reinterpret_cast<uintptr_t*>(imageBase+0x283f4a8);}
static uintptr_t uiBundle() {return readAt<uintptr_t>(reinterpret_cast<void*>(uiContext()),0xc8);}

static bool browserAction(const char* id,const char* caption,bool enabled=true) {
    NativeFrame cell(id); cell.width(230);
    return ownButton(caption,enabled);
}

static bool nativeBackAction(NativeFrame& frame) {
    // C5FB99..C5FC8A: action 1 is the native browser's Return shortcut.
    // BDD180 is a server-provider advertisement, NOT a navigation control.
    alignas(16) unsigned char action[0x48]{};
    ui<void*(__fastcall*)(void*,unsigned char,unsigned char)>(0xb24190)(action,1,1);
    putAt<uint64_t>(action,0x40,ui<uint64_t(__fastcall*)(uintptr_t)>(0x11b6600)(frame.node()));
    putAt<uint16_t>(action,0x17,1);
    // CE8D40 supplies the game's standard hint layout to CE85B0, including
    // the bound key, localized action name, mouse hover and click dispatch.
    modBackClicked=false;
    struct DrawBackScope {
        bool previous=drawingModBack;
        DrawBackScope() {drawingModBack=true;}
        ~DrawBackScope() {drawingModBack=previous;}
    } drawBack;
    ui<unsigned char(__fastcall*)(void*,unsigned char,void*)>(0xce8d40)(action,0,nullptr);
    ui<unsigned char(__fastcall*)(void*,void*,uint16_t)>(0xda6830)(action,nullptr,0x10);
    if(modBackClicked) log("Mods Back: native control click captured");
    return modBackClicked;
}

static const char* catalogState(const std::string& id) {
    const auto& info=catalog.at(id);
    if(info.library) return requiredLibraries.count(id)?loaderText("state.library_required"):loaderText("state.library_unused");
    if(isEnabled(id)) return loaderText("state.on");
    return info.valid?loaderText("state.off"):loaderText("state.invalid");
}
struct NativeScreenLifetime {
    void* window; void* backdrop; bool windowReady=false,backdropReady=false;
    ~NativeScreenLifetime() {
        if(backdropReady) ui<void(__fastcall*)(void*)>(0xa40080)(backdrop);
        if(windowReady) ui<void(__fastcall*)(void*)>(0xa3fa90)(window);
    }
};
static void modListEntry(size_t index) {
    const auto identity="ModEntry_"+std::to_string(index);
    NativeFrame row(identity.c_str(),true);
    row.height(76); row.stretch(1,0); row.padding(20,0);
    const uint16_t align=0x202;
    ui<void(__fastcall*)(uintptr_t,const uint16_t*)>(0x11ced60)(row.node(),&align);
    // The same hover sound, focus registration, highlight and click event
    // used by native server rows at cf80c7..cf82a8.
    ui<void(__fastcall*)(void*,const void*)>(0xa56600)(row.bytes,reinterpret_cast<void*>(uiBundle()+0x12620));
    ui<void(__fastcall*)(uintptr_t,uintptr_t)>(0xa41fb0)(uiContext(),row.node());
    const bool hover=ui<unsigned char(__fastcall*)(void*)>(0xa4dbe0)(row.bytes)!=0;
    alignas(16) unsigned char style[0x60]{};
    style[0]=hover || selectedMod==index; style[1]=1;
    ui<void(__fastcall*)(void*,const void*)>(0xd06ef0)(row.bytes,style);
    uint32_t event=0;
    ui<void*(__fastcall*)(void*,uint32_t*,unsigned char,uint32_t,unsigned char)>(0xa47170)(row.bytes,&event,1,0,0);
    if((event&0xff)==4) {selectedMod=index; log("Selected mod: "+modFiles[index]);}
    {
        NativeFrame label("ModEntryText",false,3); label.stretch(1,0);
        const auto shortName=std::filesystem::u8path(modFiles[index]).filename().u8string();
        pageLabel(shortName.c_str());
        pageLabel(catalogState(modFiles[index]));
    }
}

#include "native_settings_ui.inl"

static void renderModsPage() {
    if(!pageDrawLogged) log("Mods browser: native screen begin");
    alignas(16) unsigned char window[0x20]{},backdrop[0x30]{};
    NativeScreenLifetime lifetime{window,backdrop};
    static const char windowId[]="WorldModsBrowser";
    const uint64_t name[]={reinterpret_cast<uintptr_t>(windowId),sizeof(windowId)-1};
    // Layer 9 and the same fullscreen/input flags as doPlaySessionStarterV2.
    ui<void*(__fastcall*)(void*,const uint64_t*,unsigned char,uint32_t,uint32_t,uint32_t)>(0xa3f530)(window,name,1,9,0xffffffff,1);
    lifetime.windowReady=true;
    const auto node=ui<uintptr_t(__fastcall*)(void*)>(0xa4b6b0)(window);
    ui<void*(__fastcall*)(void*,uintptr_t)>(0xa3cb90)(backdrop,node);
    lifetime.backdropReady=true;
    ui<void(__fastcall*)(void*)>(0xa49b10)(backdrop);
    alignas(16) float safeArea[4]{};
    ui<void*(__fastcall*)(float*)>(0xd41360)(safeArea);
    ui<void(__fastcall*)(uintptr_t,const float*)>(0x11cf210)(readAt<uintptr_t>(backdrop,8),safeArea);
    {
        NativeFrame screen("ModsBrowserContent",false,20);
        // Native browserPanes width 1550; native server-list width 600.
        const float browserWidth=*reinterpret_cast<float*>(imageBase+0x1d920b0);
        const float listWidth=*reinterpret_cast<float*>(imageBase+0x13d64e4);
        screen.width(browserWidth); screen.stretch(0,1); screen.align(0x202);
        {
            NativeFrame header("ModsBrowserHeader",true,20); header.stretch(1,0);
            pageLabel(loaderText("ui.title"),true);
            {NativeFrame spacer("HeaderSpacer"); spacer.stretch(1,0);}
            const auto count=std::string(loaderText("ui.files"))+": "+std::to_string(modFiles.size());
            pageLabel(count.c_str());
        }
        {
            NativeFrame panes("ModsBrowserPanes",true,20); panes.stretch(1,1);
            {
                NativeFrame list("ModsBrowserList",false,8);
                list.width(listWidth); list.stretch(0,1);
                if(modFiles.empty()) {
                    NativeFrame empty("EmptyModList",false,16); empty.stretch(1,1);
                    empty.padding(24,30); empty.panel();
                    pageLabel(loaderText("ui.empty"));
                    pageLabel(loaderText("ui.available"));
                } else {
                    for(size_t i=modPage*6;i<modFiles.size() && i<(modPage+1)*6;++i) modListEntry(i);
                }
            }
            {
                NativeFrame detail("ModsBrowserDetails",false,20);
                detail.width(browserWidth-listWidth-20); detail.stretch(0,1);
                detail.padding(40,30); detail.panel();
                if(selectedMod<modFiles.size()) {
                    const auto& file=modFiles[selectedMod];
                    const auto shortName=std::filesystem::u8path(file).stem().u8string();
                    const auto& info=catalog.at(file);
                    const bool enabled=isEnabled(file);
                    {
                        NativeFrame header("ModDetailHeader",true,20); header.stretch(1,0);
                        {NativeFrame title("ModDetailTitle"); title.stretch(1,0); pageLabel(shortName.c_str(),true);}
                        if(browserAction("ToggleMod",enabled?loaderText("ui.disable"):loaderText("ui.enable"),profileError.empty() && (enabled || (info.valid && !info.library)))) {
                            toggleMod(file); updatePlanPreview();
                        }
                    }
                    const auto metadata=file+(info.version.empty()?std::string{}:std::string("    |    ")+loaderText("ui.version")+" "+info.version);
                    pageLabel(metadata.c_str());
                    {
                        // Declarative controls; browsing never executes the mod DLL.
                        NativeFrame settings("ModWorldSettings",false,0); settings.stretch(1,1);
                        renderNativeModSettings(file);
                    }
                    if(!info.present) pageLabel(loaderText("ui.missing"));
                    if(!info.error.empty()) pageLabel(info.error.c_str());
                } else {
                    pageLabel(loaderText("ui.details"),true);
                    pageLabel(loaderText("ui.select"));
                }
                if(!planError.empty()) pageLabel(planError.c_str());
                if(!catalogError.empty()) pageLabel(catalogError.c_str());
                if(!profileError.empty()) pageLabel(profileError.c_str());
                if(!launchError.empty()) pageLabel(launchError.c_str());
                if(needsRestart(selectedKey,enabledMods)) {
                    pageLabel(loaderText("ui.restart"));
                    pageLabel(loaderText("ui.restart_world"));
                }
            }
        }
        {
            NativeFrame footer("ModsBrowserActions",true,16); footer.stretch(1,0);
            if(browserAction("RefreshMods",loaderText("ui.refresh"))) {openProfile(selectedKey); launchError.clear(); refreshMods();}
            const size_t pages=std::max(size_t(1),(modFiles.size()+5)/6);
            if(pages>1) {
                if(browserAction("PreviousPage",loaderText("ui.previous"),modPage>0)) changeModPage(modPage-1);
                if(browserAction("NextPage",loaderText("ui.next"),modPage+1<pages)) changeModPage(modPage+1);
                const auto status=std::to_string(modPage+1)+" / "+std::to_string(pages); pageLabel(status.c_str());
            }
            {NativeFrame spacer("FooterSpacer"); spacer.stretch(1,0);}
            {
                NativeFrame back("BackToWorld"); back.align(0x303);
                if(nativeBackAction(back)) {modBackRequested=true;}
            }
        }
    }
    if(!pageDrawLogged) {log("Mods browser: native frame completed"); pageDrawLogged=true;}
}
