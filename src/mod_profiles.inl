// Bounded profile storage. All writers use a per-world lock and atomic replacement.
#include <fstream>
#include <sstream>
#include <map>
#include <memory>
#include "mod_api.h"
constexpr size_t MaxMods=1024, MaxModPath=1024, MaxProfileBytes=MaxMods*(MaxModPath+1)+64;
struct FileHandle {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit FileHandle(HANDLE h=INVALID_HANDLE_VALUE):value(h){}
    ~FileHandle(){if(value!=INVALID_HANDLE_VALUE && value) CloseHandle(value);}
    FileHandle(const FileHandle&)=delete;
    FileHandle& operator=(const FileHandle&)=delete;
};
static std::set<std::string> enabledMods;
static std::string profileError,profileNotice,profileWorld;
static bool sessionCommitted=false,sessionTainted=false,settingsRestartPending=false;
static std::string sessionWorld;
static std::set<std::string> sessionMods;
static std::vector<HMODULE> residentMods;
static EnshroudedModContext modContext{};
static std::string worldId(const std::string& key) {
    const auto id=key.size()==37 && key[4]=='-'?key.substr(5):key;
    if(id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=std::string::npos || id==std::string(32,'0')) return {};
    return id;
}
static std::string modId(const std::string& name) {
    if(name.empty() || name.size()>MaxModPath || name.find_first_of("\r\n:\t")!=std::string::npos || name.find('\0')!=std::string::npos) return {};
    try {
        auto path=std::filesystem::u8path(name);
        if(path.is_absolute() || path.has_root_path()) return {};
        for(const auto& part:path) {
            auto s=part.wstring();
            if(s.empty() || s==L".." || s==L"." || s.back()==L'.' || s.back()==L' ' || s.find_first_of(L"<>\"|?*")!=std::wstring::npos) return {};
        }
        auto normalized=path.generic_wstring();
        if(!normalized.empty()) CharLowerBuffW(normalized.data(),static_cast<DWORD>(normalized.size()));
        const auto result=std::filesystem::path(normalized).u8string();
        if(result.size()<5 || result.substr(result.size()-4)!=".dll") return {};
        return result;
    } catch(const std::exception&) {return {};}
}
static bool boundedRead(const std::filesystem::path& path,size_t limit,std::string& out) {
    out.clear();
    FileHandle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file.value,&size) || size.QuadPart<0 || static_cast<uint64_t>(size.QuadPart)>limit) return false;
    out.resize(static_cast<size_t>(size.QuadPart)); DWORD count=0;
    if(!ReadFile(file.value,out.data(),static_cast<DWORD>(out.size()),&count,nullptr) || count!=out.size()) {out.clear(); return false;}
    return true;
}
static std::filesystem::path profilePath(const std::string& key) {return std::filesystem::path(folder)/L"worlds"/(worldId(key)+".mods");}
static bool profileExists(const std::string& key) {
    std::error_code ec;
    return std::filesystem::exists(profilePath(key),ec) || bool(ec);
}
static bool readProfile(const std::string& key,std::set<std::string>& out,std::string& error) {
    out.clear(); error.clear();
    if(worldId(key).empty()) {error="Identifiant du monde invalide"; return false;}
    if(!profileExists(key)) return true; // Absence explicitly means an empty configuration.
    std::string body;
    if(!boundedRead(profilePath(key),MaxProfileBytes,body)) {error="Profil illisible ou trop volumineux"; return false;}
    if(body.compare(0,3,"\xef\xbb\xbf")==0) body.erase(0,3);
    std::istringstream in(body); std::string line;
    auto lineRead=[&] {if(!std::getline(in,line)) return false; if(!line.empty() && line.back()=='\r') line.pop_back(); return true;};
    if(!lineRead() || line!="ENSHROUDED_MODS_V1") {error="Fichier de configuration des mods invalide"; return false;}
    std::set<std::string> parsed;
    while(lineRead()) {
        if(line.empty()) continue;
        auto name=modId(line);
        if(name.empty() || parsed.size()>=MaxMods || !parsed.insert(name).second) {error="Liste de mods invalide (limite ou doublon)"; return false;}
    }
    out=std::move(parsed); return true;
}
// expected != nullptr implements optimistic conflict detection while holding the writer lock.
static bool saveProfile(const std::string& key,const std::set<std::string>& entries,std::string& error,const std::set<std::string>* expected=nullptr) {
    error.clear();
    if(worldId(key).empty()) {error="Identifiant du monde invalide"; return false;}
    if(entries.size()>MaxMods) {error="Trop de mods (1024 maximum)"; return false;}
    std::string body="ENSHROUDED_MODS_V1\n";
    for(const auto& entry:entries) {
        if(modId(entry)!=entry) {error="Chemin de mod invalide"; return false;}
        body+=entry+"\n";
    }
    const auto path=profilePath(key),dir=path.parent_path();
    std::error_code ec; std::filesystem::create_directories(dir,ec);
    if(ec) {error="Creation du dossier de configuration impossible"; return false;}
    FileHandle lock(CreateFileW((path.wstring()+L".lock").c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(lock.value==INVALID_HANDLE_VALUE) {error="Profil utilise par un autre programme. Actualise puis reessaie."; return false;}
    if(expected) {
        std::set<std::string> current;
        if(!readProfile(key,current,error)) return false;
        if(current!=*expected) {error="Le profil a change. Actualise avant de modifier."; return false;}
    }
    const auto temp=path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp";
    FileHandle file(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE) {error="Enregistrement des choix impossible"; return false;}
    DWORD written=0;
    bool ok=WriteFile(file.value,body.data(),static_cast<DWORD>(body.size()),&written,nullptr) && written==body.size();
    if(ok) ok=FlushFileBuffers(file.value)!=0;
    CloseHandle(file.value); file.value=INVALID_HANDLE_VALUE;
    if(ok) ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) {DeleteFileW(temp.c_str()); error="Enregistrement des choix impossible";}
    return ok;
}
static void openProfile(const std::string& key) {
    profileWorld=worldId(key); profileNotice.clear();
    readProfile(key,enabledMods,profileError);
}
static bool isEnabled(const std::string& file) {return enabledMods.count(modId(file))!=0;}
static bool needsRestart(const std::string& key,const std::set<std::string>& desired) {
    return sessionTainted || settingsRestartPending || (sessionCommitted && (sessionMods!=desired || (!sessionMods.empty() && sessionWorld!=worldId(key))));
}
static bool modPath(const std::string& id,std::filesystem::path& out) {
    if(modId(id)!=id) return false;
    std::error_code ec;
    const auto root=std::filesystem::canonical(std::filesystem::path(folder).parent_path()/L"Mods",ec);
    if(ec) return false;
    out=std::filesystem::canonical(root/std::filesystem::u8path(id),ec);
    if(ec || !std::filesystem::is_regular_file(out,ec) || ec) return false;
    const auto relative=out.lexically_relative(root);
    if(relative.empty() || relative.is_absolute()) return false;
    for(const auto& part:relative) if(part==L"..") return false;
    return true;
}
#include "mod_packages.inl"
#include "mod_settings.inl"
static bool toggleMod(const std::string& file) {
    if(profileWorld!=worldId(selectedKey) || !profileError.empty()) return false;
    const auto id=modId(file); if(id.empty()) return false;
    if(!enabledMods.count(id)) {
        Package package;
        if(!readPackage(id,package,profileError)) return false;
        if(!package.mod) {profileError="Cette bibliotheque est geree par ses mods dependants."; return false;}
    }
    auto next=enabledMods;
    if(next.count(id)) next.erase(id); else next.insert(id);
    if(!saveProfile(selectedKey,next,profileError,&enabledMods)) return false;
    enabledMods=std::move(next); launchError.clear();
    profileNotice="Choix enregistr\xc3\xa9s pour ce monde."; return true;
}

static void pluginLog(const char* message) noexcept {try {if(message) log(std::string("[mod] ")+std::string(message,strnlen_s(message,4096)));} catch(...) {}}
using LoadMod=HMODULE(*)(const wchar_t*);
static HMODULE loadNativeMod(const wchar_t* path) {return LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);}
static LoadMod loadMod=loadNativeMod;
static bool prepareWorldMods(const std::string& key,std::string& error) {
    std::set<std::string> desired;
    if(!readProfile(key,desired,error)) return false;
    settingsRestartPending=false;
    if(sessionCommitted && sessionWorld==worldId(key) && !matchesSessionSettings(key,error)) {settingsRestartPending=true; return false;}
    if(needsRestart(key,desired)) {error="Redemarre le jeu pour appliquer les mods de ce monde."; return false;}
    if(sessionCommitted) return matchesSessionSettings(key,error);
    LoadPlan plan;
    if(!makeLoadPlan(desired,plan,error)) return false;
    std::map<std::string,SettingValues> preparedSettings;
    std::map<std::string,std::string> preparedManifests;
    for(const auto& item:plan.ordered) preparedManifests.emplace(item.id,item.manifest);
    for(const auto& item:plan.ordered) if(!item.settings.empty()) {
        if(!readModSettings(key,item,preparedSettings[item.id],error)) return false;
    }
    sessionWorld=worldId(key); sessionMods=desired; sessionSettings=std::move(preparedSettings); sessionManifests=std::move(preparedManifests);
    modContext={sizeof(EnshroudedModContext),1,sessionWorld.c_str(),pluginLog};
    for(const auto& item:plan.ordered) {
        sessionTainted=true;
        const auto module=loadMod(item.path.c_str());
        if(!module) {error="Chargement impossible : "+item.id+" ("+std::to_string(GetLastError())+"). Redemarre le jeu."; log(error); return false;}
        if(std::find(residentMods.begin(),residentMods.end(),module)!=residentMods.end()) {error="Module deja resident : "+item.id+". Redemarre le jeu."; return false;}
        residentMods.push_back(module);
        if(item.mod) {
            if(!item.settings.empty()) {
                const auto configure=reinterpret_cast<int(*)(const char*,const char*)>(GetProcAddress(module,"EnshroudedModConfigure"));
                const auto values=settingsText(sessionSettings.at(item.id));
                if(!configure || configure(sessionWorld.c_str(),values.c_str())!=1) {error="Configuration refusee : "+item.id+". Redemarre le jeu."; return false;}
            }
            const auto init=reinterpret_cast<int(*)(const EnshroudedModContext*)>(GetProcAddress(module,"EnshroudedModInitialize"));
            if(!init || init(&modContext)!=1) {error="Initialisation refusee : "+item.id+". Redemarre le jeu."; log(error); return false;}
        }
        log("Loaded "+item.id+" version="+item.version+" world="+sessionWorld);
    }
    sessionCommitted=true; sessionTainted=false; return true;
}
