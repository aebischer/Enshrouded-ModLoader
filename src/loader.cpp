#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <unknwn.h>
#include <bcrypt.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <set>
#include <mutex>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <functional>
#include "MinHook.h"
#include "loader_bootstrap.h"

// Prototype for exactly the examined client. Never apply offsets to other builds.
constexpr char ExpectedHash[] = "5D9EF2F140B8ED49E30DBEE655841B89D08BAEC20F799A1817E171A18A850C77";
constexpr uintptr_t ButtonRva = 0xb6ce60;
struct Text { const char* bytes; uint64_t length; uint64_t hash; };
using Button = unsigned char(__fastcall*)(void*, const Text*);
static Button originalButton;
static uintptr_t imageBase;
static std::wstring folder;
static std::mutex logMutex;

static void log(const char* line) noexcept {
    try {
        std::lock_guard<std::mutex> guard(logMutex);
        const auto path=std::filesystem::path(folder)/L"mod-loader.log";
        std::error_code ec;
        if(std::filesystem::exists(path,ec) && std::filesystem::file_size(path,ec)>2*1024*1024 && !ec)
            MoveFileExW(path.c_str(),(path.wstring()+L".previous").c_str(),MOVEFILE_REPLACE_EXISTING);
        FILE* out=nullptr; _wfopen_s(&out,path.c_str(),L"ab");
        if(out) {
            SYSTEMTIME t{}; GetLocalTime(&t);
            fprintf(out,"%04u-%02u-%02u %02u:%02u:%02u %.*s\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,4096,line);
            fclose(out);
        }
    } catch(...) {}
}
static void log(const std::string& line) noexcept {log(line.c_str());}
static Text text(const char* value) {
    Text result{value, strlen(value), 0xcbf29ce484222325ULL};
    for (size_t i=0; i<result.length; ++i) {
        result.hash ^= static_cast<uint64_t>(static_cast<int64_t>(static_cast<signed char>(value[i])));
        result.hash *= 0x100000001b3ULL;
    }
    return result;
}
static bool matchingExe(const wchar_t* path) {
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return false;
    if (BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0) { BCryptCloseAlgorithmProvider(alg,0); return false; }
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    bool ok=file!=INVALID_HANDLE_VALUE;
    std::vector<unsigned char> bytes(65536); unsigned char digest[32]; DWORD count=0;
    while(ok) {
        if(!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&count,nullptr)) {ok=false; break;}
        if(!count) break;
        if(BCryptHashData(hash,bytes.data(),count,0)<0) {ok=false; break;}
    }
    if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
    ok=ok && BCryptFinishHash(hash,digest,sizeof digest,0)>=0;
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg,0);
    if(!ok) return false;
    char actual[65]{};
    for(int i=0;i<32;++i) sprintf_s(actual+i*2,3,"%02X",digest[i]);
    return strcmp(actual,ExpectedHash)==0;
}

// Menu-only bootstrap. RandomWorld is deliberately not linked into this DLL.
template<class T> static T readAt(const void* p,size_t offset) {
    T value; memcpy(&value,static_cast<const char*>(p)+offset,sizeof value); return value;
}
template<class T> static void putAt(void* p,size_t offset,T value) {
    memcpy(static_cast<char*>(p)+offset,&value,sizeof value);
}
static uint32_t gameLanguage() {
    if(!imageBase) return 1;
    const auto context=readAt<uintptr_t>(reinterpret_cast<void*>(imageBase),0x283f4a8);
    if(!context) return 1;
    const auto services=readAt<uintptr_t>(reinterpret_cast<void*>(context),0xd0);
    if(!services) return 1;
    const auto pool=readAt<uintptr_t>(reinterpret_cast<void*>(services),0x33ce8);
    return pool?readAt<uint32_t>(reinterpret_cast<void*>(pool),0xd8):1;
}
// Error dialogs can run before initialization, or while the native UI is failing.
// Do not let a stale native locale pointer hide the original error.
static uint32_t dialogLanguage() noexcept {
    __try {return gameLanguage();}
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION || GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR
             ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {return 1;}
}
#include "loader_localization.h"
static std::filesystem::path packageRoot() {
    return std::filesystem::path(folder).parent_path()/L"Mods"/L"EnshroudedModLoader";
}
static bool hasPackageManifest(const std::filesystem::path& dll) {
    const auto path=dll.wstring()+L".mod";
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) return false;
    FILE* file=nullptr;
    if(_wfopen_s(&file,path.c_str(),L"rb") || !file) return false;
    char header[64]{};
    const auto count=fread(header,1,sizeof(header)-1,file);
    fclose(file);
    const char* first=header;
    if(count>=3 && !memcmp(first,"\xef\xbb\xbf",3)) first+=3;
    constexpr char marker[]="ENSHROUDED_PACKAGE_V1";
    constexpr size_t size=sizeof(marker)-1;
    return !strncmp(first,marker,size) && (first[size]=='\r' || first[size]=='\n' || first[size]=='\0');
}
using WorldLookup=unsigned char*(__fastcall*)(uint16_t);
using EditFrame=uintptr_t(__fastcall*)(void*,void*);
using BrowserScreen=void(__fastcall*)(void*);
static EditFrame originalEdit;
static BrowserScreen originalBrowser;
static thread_local unsigned char* activeWorld=nullptr;
static std::string selectedKey;
static bool modsOpen=false;
static std::vector<std::string> modFiles;
static size_t modPage=0;
static size_t selectedMod=SIZE_MAX;
static std::string catalogError;
static bool pageDrawLogged=false;
static std::string launchError;
#include "mod_profiles.inl"

struct CatalogInfo {bool present=false,valid=false,library=false; std::string version,error;};
static std::map<std::string,CatalogInfo> catalog;
static std::set<std::string> requiredLibraries;
static std::string planError;
static void updatePlanPreview() {
    requiredLibraries.clear(); planError.clear();
    LoadPlan plan;
    if(!makeLoadPlan(enabledMods,plan,planError,true)) return;
    for(const auto& item:plan.ordered) if(!item.mod) requiredLibraries.insert(item.id);
}
static void refreshMods() {
    modFiles.clear(); catalog.clear(); catalogError.clear(); modPage=0; selectedMod=SIZE_MAX; pageDrawLogged=false;
    const auto root=packageRoot();
    std::error_code ec;
    if(std::filesystem::exists(root,ec)) {
        std::filesystem::recursive_directory_iterator it(root,ec),end;
        size_t visited=0;
        const auto deadline=GetTickCount64()+1500;
        while(!ec && it!=end) {
            if(++visited>8192 || GetTickCount64()>deadline) {catalogError=loaderText("err.scan_limit"); break;}
            if(it.depth()>=16) it.disable_recursion_pending();
            auto ext=it->path().extension().wstring();
            std::transform(ext.begin(),ext.end(),ext.begin(),[](wchar_t c){return towlower(c);});
            if(it->is_symlink(ec) || (GetFileAttributesW(it->path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)) {
                it.disable_recursion_pending();
            } else if(it->is_regular_file(ec) && ext==L".dll" &&
                      hasPackageManifest(it->path())) {
                const auto id=modId(it->path().lexically_relative(root).u8string());
                if(!id.empty()) modFiles.push_back(id);
            }
            if(!ec) it.increment(ec);
        }
    }
    if(ec) catalogError=loaderText("err.scan_read");
    for(const auto& id:enabledMods) if(std::find(modFiles.begin(),modFiles.end(),id)==modFiles.end()) modFiles.push_back(id);
    std::sort(modFiles.begin(),modFiles.end());
    modFiles.erase(std::unique(modFiles.begin(),modFiles.end()),modFiles.end());
    for(const auto& id:modFiles) {
        auto& info=catalog[id]; Package item;
        info.present=modPath(id,item.path);
        info.valid=readPackage(id,item,info.error); info.library=info.valid && !item.mod; info.version=item.version;
    }
    updatePlanPreview(); refreshSettingsViews();
    if(!modFiles.empty()) selectedMod=0;
    log("Mods inventory: "+std::to_string(modFiles.size())+" entries");
}
static void changeModPage(size_t page) {
    modPage=page; selectedMod=modPage*6<modFiles.size()?modPage*6:SIZE_MAX;
}
static void uiFailure() noexcept {
    modsOpen=false;
    try {log("Mods UI exception; page closed. See profile/catalog before retry.");} catch(...) {}
    loaderErrorDialog("dialog.ui.body","dialog.ui.title",MB_OK|MB_ICONERROR);
}

// Functions/layouts verified against the SHA-256-pinned client's world-edit UI.
template<class Fn> static Fn ui(uintptr_t rva) { return reinterpret_cast<Fn>(imageBase+rva); }
static void pageLabel(const char* caption,bool heading=false) {
    // b1da10 writes through +0x143 on this exact client (not a simple Text).
    alignas(16) unsigned char label[0x150]{};
    const auto value=text(caption);
    const auto context=*reinterpret_cast<uintptr_t*>(imageBase+0x283f4a8);
    const auto bundle=readAt<uintptr_t>(reinterpret_cast<void*>(context),0xc8);
    ui<void*(__fastcall*)(void*,const Text*,uintptr_t,unsigned char)>(0xb1da10)(label,&value,bundle+(heading?0x11d44:0x10c6c),1);
    label[0x3d]=1;
    if(heading) {
        putAt<float>(label,0x7c,readAt<float>(reinterpret_cast<void*>(bundle),0x10bb8));
        putAt<uint16_t>(label,0x90,readAt<uint16_t>(label,0x90)|8);
    }
    putAt<uint16_t>(label,0x38,0x201);
    const uint16_t alignment=0x201;
    ui<void(__fastcall*)(uintptr_t,const uint16_t*)>(0x11ced70)(readAt<uintptr_t>(label,8),&alignment);
    ui<void(__fastcall*)(void*)>(0xb25160)(label);
}

static std::string worldKey(const unsigned char* world) {
    if(!world || !world[0xe0]) return {};
    char key[48]{};
    sprintf_s(key,"%04x-",readAt<uint16_t>(world,0x10));
    for(size_t i=0;i<16;++i) sprintf_s(key+5+i*2,3,"%02x",world[0x14+i]);
    return key;
}
static bool ownButton(const char* caption,bool enabled=true) {
    alignas(16) unsigned char config[0x78]{};
    putAt<uint16_t>(config,4,0x202); config[6]=1; config[9]=enabled;
    putAt<uint32_t>(config,0x10,3);
    putAt<float>(config,0x4c,10); putAt<float>(config,0x50,5); putAt<float>(config,0x54,1);
    const auto label=text(caption);
    return originalButton(config,&label)!=0 && enabled;
}
// Intercept only the native Return control while MODS owns navigation.
using PointerEvent=void*(__fastcall*)(void*,uint32_t*,unsigned char,unsigned char);
using MenuAction=unsigned char(__fastcall*)(void*,void*,uint16_t);
static PointerEvent originalPointerEvent;
static MenuAction originalMenuAction;
static thread_local bool drawingModBack=false;
static bool modBackClicked=false,modBackRequested=false;
static unsigned backDrainFrames=0;
static bool restoringWorldPage=false;
static void* __fastcall pointerEvent(void* node,uint32_t* event,unsigned char button,unsigned char consume) {
    void* result=originalPointerEvent(node,event,button,consume);
    if(drawingModBack && event && (*event&0xff)==4) {
        modBackClicked=true;
        *event&=0xffffff00u; // do not inject another Back into the game's queue
    }
    return result;
}
static unsigned char __fastcall menuAction(void* action,void* output,uint16_t mask) {
    const auto pressed=originalMenuAction(action,output,mask);
    if(action && readAt<unsigned char>(action,0)==1 && (modsOpen || backDrainFrames || restoringWorldPage)) {
        if(modsOpen && pressed) modBackRequested=true;
        return 0; // consume, but never navigate the menus underneath MODS
    }
    return pressed;
}
struct WorldPageReturn {
    bool pending=false;
    uint16_t slot=0;
    unsigned char editVisible=0;
    uint32_t page=0;
    uint64_t selection=0,tab=0;
    void capture(const void* state,const void* menu=nullptr) {
        if(menu) {tab=readAt<uint64_t>(menu,0x98);
            if(tab>1) throw std::runtime_error("Unsupported world source");
            selection=readAt<uint64_t>(menu,0xa8+tab*24);
        }
        slot=readAt<uint16_t>(state,0x41);
        editVisible=readAt<unsigned char>(state,0x39);
        page=readAt<uint32_t>(state,0x3c);
        pending=true;
    }
    void apply(void* state,void* menu=nullptr) const {
        if(menu) {
            putAt<uint64_t>(menu,0x98,tab);
            putAt<uint64_t>(menu,0x230,tab);
            putAt<uint64_t>(menu,0xa8+tab*24,selection);
        }
        putAt<uint16_t>(state,0x41,slot);
        putAt<unsigned char>(state,0x39,editVisible);
        putAt<uint32_t>(state,0x3c,page);
    }
};
static WorldPageReturn worldPageReturn, lastNativeReturn;
static std::string lastNativeWorld;
static void prepareRefusedReturn(const std::string& key) {
    worldPageReturn.pending=false;
    if(lastNativeReturn.pending && lastNativeWorld==key) worldPageReturn=lastNativeReturn;
    modBackRequested=false; backDrainFrames=0;
}
static void* activeEditState=nullptr;
static void* activeEditMenu=nullptr;
using WorldDetails=void(__fastcall*)(void*,void*,void*);
static WorldDetails originalWorldDetails;
static void __fastcall worldDetails(void* menu,void* state,void* result) {
    try {
    if(restoringWorldPage && worldPageReturn.pending) {
        worldPageReturn.pending=false;
        const auto world=reinterpret_cast<WorldLookup>(imageBase+0xd436b0)(worldPageReturn.slot);
        if(worldKey(world)==selectedKey) {
            worldPageReturn.apply(state,menu);
            log("Mods Back: restored selected world's edit page");
        } else log("Mods Back: selected world unavailable; edit restoration skipped");
        worldPageReturn.pending=false;
    }
    } catch(...) {
        worldPageReturn.pending=false;
        try {log("Mods Back: restoration failed; native page retained");} catch(...) {}
    }
    try {
        lastNativeReturn.pending=false;
        if(!state || !menu) throw std::runtime_error("Missing native page");
        lastNativeWorld=worldKey(reinterpret_cast<WorldLookup>(imageBase+0xd436b0)(readAt<uint16_t>(state,0x41)));
        if(!lastNativeWorld.empty()) lastNativeReturn.capture(state,menu);
    } catch(...) {lastNativeReturn.pending=false;}
    originalWorldDetails(menu,state,result);
}
#include "mods_browser.inl"
__declspec(noinline) static unsigned char __fastcall drawButton(void* config,const Text* label) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-imageBase;
    const auto pressed=originalButton(config,label);
    // Same native column and insertion point as the previous Seed button.
    if(activeWorld && caller==0xc8b2c0) {
        if(pressed) modsOpen=false;
        if(ownButton(loaderText("ui.title"),!selectedKey.empty()) && !pressed) {
            try {openProfile(selectedKey); launchError.clear(); refreshMods();
                if(activeEditState) worldPageReturn.capture(activeEditState,activeEditMenu);
                modBackRequested=false; modsOpen=true;
                log("Mods page opened world="+selectedKey);
            } catch(...) {uiFailure();}
        }
    }
    return pressed;
}
static uintptr_t __fastcall editFrame(void* menu,void* state) {
    auto previous=activeWorld;
    auto previousMenu=activeEditMenu; activeEditMenu=menu;
    auto previousEdit=activeEditState; activeEditState=state;
    activeWorld=reinterpret_cast<WorldLookup>(imageBase+0xd436b0)(readAt<uint16_t>(state,0x41));
    try {
        const auto key=worldKey(activeWorld);
        if(key!=selectedKey) { selectedKey=key; modsOpen=false; }
    } catch(...) {uiFailure();}
    struct Restore {unsigned char* value; void* edit; void* menu; ~Restore(){activeWorld=value; activeEditState=edit; activeEditMenu=menu;}} restore{previous,previousEdit,previousMenu};
    return originalEdit(menu,state);
}
struct BackKeyLatch {
    bool held=true;
    bool update(bool focused,bool down) {
        const bool pressed=focused && down && !held;
        held=!focused || down;
        return pressed;
    }
};
static void __fastcall browserScreen(void* menu) {
    static BackKeyLatch escape;
    DWORD foregroundProcess=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foregroundProcess);
    const bool escapePressed=escape.update(foregroundProcess==GetCurrentProcessId(),(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0);
    if(modsOpen && escapePressed) modBackRequested=true;
    if(modsOpen && modBackRequested) {
        // Keep owning Back until the physical key is released. Then suppress
        // the queued native release for two full browser frames as well.
        if((GetAsyncKeyState(VK_ESCAPE)&0x8000)==0) {
            modsOpen=false; modBackRequested=false; backDrainFrames=2;
            log("Mods Back: one-level close after input release");
        }
    }
    if(!modsOpen && backDrainFrames) {
        --backDrainFrames;
        return;
    }
    // Replace the whole native browser, not a popup over its world controls.
    // The pre-game scenery is drawn by the caller and remains untouched.
    if(modsOpen) {try {renderModsPage();} catch(...) {uiFailure();} return;}
    restoringWorldPage=worldPageReturn.pending;
    struct RestoreNavigation {~RestoreNavigation(){restoringWorldPage=false;}} restoreNavigation;
    originalBrowser(menu);
}
using StartLocal=void(__fastcall*)(unsigned char*);
using StartSession=void(__fastcall*)(void*);
static StartLocal originalStartLocal;
static StartSession originalStartSession;
static void __fastcall startLocal(unsigned char* world) {
    bool allowed=false;
    try {
        const auto key=worldKey(world);
        allowed=prepareWorldMods(key,launchError);
        if(!allowed) {prepareRefusedReturn(key); selectedKey=key; openProfile(key); refreshMods(); modsOpen=true; log("World start refused: "+launchError);}
    } catch(const std::exception& e) {sessionTainted=true; log(e.what()); uiFailure();}
      catch(...) {sessionTainted=true; uiFailure();}
    if(allowed) {launchError.clear(); originalStartLocal(world);}
}
static void __fastcall startSession(void* request) {
    // Local requests flow through d8ed80 and use their actual world pointer.
    // A remote server must never inherit native DLLs from a local world.
    if(request && !readAt<uintptr_t>(request,0) && (!sessionMods.empty() || sessionTainted)) {
        try {worldPageReturn.pending=false; launchError=loaderText("err.restart_server"); modsOpen=true;} catch(...) {uiFailure();}
        log("Remote join refused with resident local mods"); return;
    }
    originalStartSession(request);
}
static bool unchangedHookTargets(const wchar_t* exe) {
    FileHandle file(CreateFileW(exe,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
    if(file.value==INVALID_HANDLE_VALUE) return false;
    FileHandle mapping(CreateFileMappingW(file.value,nullptr,PAGE_READONLY|SEC_IMAGE_NO_EXECUTE,0,0,nullptr));
    if(!mapping.value) return false;
    auto view=static_cast<const unsigned char*>(MapViewOfFile(mapping.value,FILE_MAP_READ,0,0,0));
    if(!view) return false;
    bool ok=true;
    for(const auto rva:{ButtonRva,uintptr_t(0xc8a7d0),uintptr_t(0xc5ef50),uintptr_t(0xd8ed80),uintptr_t(0xd8efb0),uintptr_t(0xa471b0),uintptr_t(0xda6830),uintptr_t(0xc87950)})
        if(memcmp(view+rva,reinterpret_cast<void*>(imageBase+rva),32)!=0) ok=false;
    UnmapViewOfFile(view); return ok;
}
static DWORD initializeImpl() {
    std::vector<wchar_t> exeBuffer(32768); auto* exe=exeBuffer.data();
    const auto length=GetModuleFileNameW(nullptr,exe,32768);
    if(!length || length>=32768) throw std::runtime_error("Executable path unavailable");
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&initializeImpl),&pinned))
        throw std::runtime_error("Cannot pin loader");
    folder=exe; folder.resize(folder.find_last_of(L"\\/"));
    folder+=L"\\ModLoader"; CreateDirectoryW(folder.c_str(),nullptr);
    log("ModLoader 0.6.14: automatic standalone / Kiln bootstrap");
    if(!matchingExe(exe)) throw std::runtime_error("Unsupported executable");
    imageBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!unchangedHookTargets(exe)) throw std::runtime_error("Hook target modified in memory");
    auto target=reinterpret_cast<void*>(imageBase+ButtonRva);
    const unsigned char prologue[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55};
    if(memcmp(target,prologue,sizeof prologue)!=0) throw std::runtime_error("Button signature mismatch");
    if(MH_Initialize()!=MH_OK) throw std::runtime_error("Hook initialization failed");
    if(MH_CreateHook(target,reinterpret_cast<void*>(&drawButton),reinterpret_cast<void**>(&originalButton))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xc8a7d0),reinterpret_cast<void*>(&editFrame),reinterpret_cast<void**>(&originalEdit))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xc5ef50),reinterpret_cast<void*>(&browserScreen),reinterpret_cast<void**>(&originalBrowser))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xd8ed80),reinterpret_cast<void*>(&startLocal),reinterpret_cast<void**>(&originalStartLocal))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xd8efb0),reinterpret_cast<void*>(&startSession),reinterpret_cast<void**>(&originalStartSession))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xa471b0),reinterpret_cast<void*>(&pointerEvent),reinterpret_cast<void**>(&originalPointerEvent))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xda6830),reinterpret_cast<void*>(&menuAction),reinterpret_cast<void**>(&originalMenuAction))!=MH_OK ||
       MH_CreateHook(reinterpret_cast<void*>(imageBase+0xc87950),reinterpret_cast<void*>(&worldDetails),reinterpret_cast<void**>(&originalWorldDetails))!=MH_OK ||
       MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) {
        MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
        throw std::runtime_error("Hook installation failed; hooks removed");
    }
    log("MODS button active in world edit menu");
    return 0;
}
static loader_bootstrap::Start bootstrap;
static HANDLE bootstrapLease=nullptr;
static bool claimBootstrap() noexcept {
    wchar_t name[96]{};
    swprintf_s(name,L"Local\\EnshroudedModLoader.Bootstrap.%lu",GetCurrentProcessId());
    const auto lease=CreateMutexW(nullptr,FALSE,name);
    if(!lease)return false;
    if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(lease);return false;}
    bootstrapLease=lease; // Keep until process exit, including after a failed start.
    return true;
}
static bool initializeCore() noexcept {
    try {return initializeImpl()==0;}
    catch(const std::exception& e) {log(e.what());}
    catch(...) {log("Loader initialization failed");}
    return false;
}
static DWORD WINAPI initialize(void*) noexcept {
    // A chained legacy proxy must stand aside before taking the process lease.
    // Presence is deliberately enough: unknown Kiln ABI must not trigger a
    // fallback which bypasses its profile/disabled-mod policy.
    const auto proxy=GetModuleHandleW(L"dinput8.dll");
    if(proxy&&GetProcAddress(proxy,"KilnProxyInfo"))return 0;
    const auto status=bootstrap.run(claimBootstrap,initializeCore);
    if(status!=KILN_NATIVE_INIT_OK)
        loaderErrorDialog("dialog.start.body","dialog.start.title",MB_OK|MB_ICONERROR|MB_SETFOREGROUND);
    return status;
}
extern "C" __declspec(dllexport) int KILN_CALL kiln_native_init(const KilnNativeContext* ctx) noexcept {
    if(!loader_bootstrap::validContext(ctx))return KILN_NATIVE_INIT_UNSUPPORTED;
    const auto status=bootstrap.run(claimBootstrap,initializeCore);
    const char* message=status==KILN_NATIVE_INIT_OK?
        "Enshrouded ModLoader started; world profiles and package paths unchanged":
        "Enshrouded ModLoader failed or another copy already started; see ModLoader/mod-loader.log";
    ctx->api->log(ctx->owner,status==KILN_NATIVE_INIT_OK?KILN_LOG_INFO:KILN_LOG_ERROR,{message,strlen(message)});
    return status;
}
static HMODULE systemInput() {
    static HMODULE module=[] {
        wchar_t path[MAX_PATH]{}; const auto n=GetSystemDirectoryW(path,MAX_PATH);
        if(!n || n>=MAX_PATH-12 || wcscat_s(path,L"\\dinput8.dll")!=0) return static_cast<HMODULE>(nullptr);
        return LoadLibraryW(path);
    }();
    return module;
}
extern "C" HRESULT WINAPI ProxyDirectInput8Create(HINSTANCE a,DWORD b,REFIID c,LPVOID* d,LPUNKNOWN e) {
    using Fn=HRESULT(WINAPI*)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
    auto fn=reinterpret_cast<Fn>(GetProcAddress(systemInput(),"DirectInput8Create"));
    return fn?fn(a,b,c,d,e):E_FAIL;
}
extern "C" HRESULT WINAPI ProxyDllCanUnloadNow() {return S_FALSE;}
extern "C" HRESULT WINAPI ProxyDllGetClassObject(REFCLSID a,REFIID b,LPVOID* c) { using Fn=HRESULT(WINAPI*)(REFCLSID,REFIID,LPVOID*); auto fn=reinterpret_cast<Fn>(GetProcAddress(systemInput(),"DllGetClassObject")); return fn?fn(a,b,c):CLASS_E_CLASSNOTAVAILABLE; }
extern "C" HRESULT WINAPI ProxyDllRegisterServer() { return E_NOTIMPL; }
extern "C" HRESULT WINAPI ProxyDllUnregisterServer() { return E_NOTIMPL; }
extern "C" const void* WINAPI ProxyGetdfDIJoystick() { using Fn=const void*(WINAPI*)(); auto fn=reinterpret_cast<Fn>(GetProcAddress(systemInput(),"GetdfDIJoystick")); return fn?fn():nullptr; }
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        (void)self; // Static CRT and thread_local require thread notifications.
        HANDLE worker=CreateThread(nullptr,0,initialize,nullptr,0,nullptr);
        if(!worker) return FALSE;
        CloseHandle(worker);
    }
    return TRUE;
}


