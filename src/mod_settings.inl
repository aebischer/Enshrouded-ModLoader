// Settings are independent of activation: disabling a mod preserves its world values.
using SettingValues=std::map<std::string,std::string>;
static std::map<std::string,SettingValues> sessionSettings;
static std::map<std::string,std::string> sessionManifests;
static std::filesystem::path legacySettingsPath(const std::string& world,const std::string& id) {
    return worldProfilesRoot()/(worldId(world)+".settings")/std::filesystem::u8path(id+".ini");
}
static std::filesystem::path settingsPath(const std::string& world,const std::string& id) {
    unsigned char digest[32]{}; BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)
        throw std::runtime_error("Settings identity unavailable");
    const auto status=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(id.data())),static_cast<ULONG>(id.size()),digest,sizeof digest);
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(status<0) throw std::runtime_error("Settings identity unavailable");
    char name[65]{};
    for(size_t i=0;i<32;++i) sprintf_s(name+i*2,3,"%02x",digest[i]);
    return worldProfilesRoot()/(worldId(world)+".settings-v2")/(std::string(name)+".ini");
}
// Move once, without replacement: deleting a migrated INI must restore defaults,
// not resurrect an old copy. Corrupt legacy files retain their strict errors.
static bool migrateSettings(const std::string& world,const std::string& id,std::string& error) {
    const auto path=settingsPath(world,id),legacy=legacySettingsPath(world,id);
    std::error_code ec;
    if(std::filesystem::exists(path,ec)) return true;
    if(ec) {error=loaderText("err.settings_read"); return false;}
    if(!std::filesystem::is_regular_file(legacy,ec)) {
        if(ec && ec!=std::errc::no_such_file_or_directory && ec!=std::errc::not_a_directory) {error=loaderText("err.settings_old_read"); return false;}
        return true;
    }
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) {error=loaderText("err.settings_migrate"); return false;}
    FileHandle lock(CreateFileW((path.wstring()+L".lock").c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    FileHandle oldLock(CreateFileW((legacy.wstring()+L".lock").c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(lock.value==INVALID_HANDLE_VALUE || oldLock.value==INVALID_HANDLE_VALUE) {error=loaderText("err.settings_locked"); return false;}
    if(std::filesystem::exists(path,ec)) return true;
    if(ec || !MoveFileExW(legacy.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) {error=loaderText("err.settings_migrate"); return false;}
    return true;
}
static SettingValues defaultSettings(const Package& package) {
    SettingValues values; for(const auto& spec:package.settings) values.emplace(spec.key,spec.defaultValue); return values;
}
static bool validSettingValue(const SettingSpec& spec,const std::string& value) {
    if(spec.percent) {
        int number;
        return settingInteger(value,number) && number>=spec.minimum && number<=spec.maximum && (number-spec.minimum)%spec.step==0;
    }
    if(spec.boolean) return value=="true" || value=="false";
    return std::find(spec.choices.begin(),spec.choices.end(),value)!=spec.choices.end();
}
static bool readModSettings(const std::string& world,const Package& package,SettingValues& values,std::string& error) {
    error.clear(); values.clear();
    if(worldId(world).empty() || modId(package.id)!=package.id) {error=loaderText("err.settings_id"); return false;}
    auto parsed=defaultSettings(package);
    FileHandle manifestLock(CreateFileW((package.path.wstring()+L".mod").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    Package fresh;
    if(manifestLock.value==INVALID_HANDLE_VALUE || !readPackage(package.id,fresh,error) || fresh.path!=package.path || fresh.manifest!=package.manifest) {error=loaderText("err.manifest_changed"); return false;}
    if(!migrateSettings(world,package.id,error)) return false;
    const auto path=settingsPath(world,package.id); std::error_code ec;
    if(!std::filesystem::exists(path,ec) && !ec) {values=std::move(parsed); return true;}
    std::string body;
    if(!boundedRead(path,16384,body)) {error=loaderText("err.settings_unreadable")+package.id; return false;}
    if(body.compare(0,3,"\xef\xbb\xbf")==0) body.erase(0,3);
    std::istringstream in(body); std::string line; std::set<std::string> seen;
    if(!std::getline(in,line)) {error=loaderText("err.settings_empty"); return false;}
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line!="ENSHROUDED_SETTINGS_V1") {error=loaderText("err.settings_format"); return false;}
    while(std::getline(in,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.empty()) continue;
        const auto equal=line.find('=');
        const auto key=line.substr(0,equal);
        const auto spec=std::find_if(package.settings.begin(),package.settings.end(),[&](const SettingSpec& s){return s.key==key;});
        if(equal==std::string::npos || spec==package.settings.end() || !seen.insert(key).second || !validSettingValue(*spec,line.substr(equal+1))) {
            error=(std::string(loaderText("err.settings_value"))+": ")+package.id; return false;
        }
        parsed[key]=line.substr(equal+1);
    }
    values=std::move(parsed); return true;
}
static std::string settingsText(const SettingValues& values) {
    std::string text; for(const auto& pair:values) text+=pair.first+"="+pair.second+"\n"; return text;
}
static bool saveModSettings(const std::string& world,const Package& package,const SettingValues& values,const SettingValues& expected,std::string& error) {
    error.clear();
    if(worldId(world).empty() || modId(package.id)!=package.id || values.size()!=package.settings.size()) {error=loaderText("err.settings_invalid"); return false;}
    for(const auto& spec:package.settings) {
        const auto value=values.find(spec.key);
        if(value==values.end() || !validSettingValue(spec,value->second)) {error=loaderText("err.settings_value"); return false;}
    }
    FileHandle manifestLock(CreateFileW((package.path.wstring()+L".mod").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    Package fresh;
    if(manifestLock.value==INVALID_HANDLE_VALUE || !readPackage(package.id,fresh,error) || fresh.path!=package.path || fresh.manifest!=package.manifest) {error=loaderText("err.manifest_changed"); return false;}
    if(!migrateSettings(world,package.id,error)) return false;
    const auto path=settingsPath(world,package.id); std::error_code ec;
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) {error=loaderText("err.folder_create"); return false;}
    FileHandle lock(CreateFileW((path.wstring()+L".lock").c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(lock.value==INVALID_HANDLE_VALUE) {error=loaderText("err.settings_locked"); return false;}
    SettingValues current;
    if(!readModSettings(world,package,current,error)) return false;
    if(current!=expected) {error=loaderText("err.settings_changed"); return false;}
    const auto body="ENSHROUDED_SETTINGS_V1\n"+settingsText(values);
    const auto temp=path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64())+L".tmp";
    FileHandle file(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE) {error=loaderText("err.settings_write"); return false;}
    DWORD count=0; bool ok=WriteFile(file.value,body.data(),static_cast<DWORD>(body.size()),&count,nullptr) && count==body.size();
    if(ok) ok=FlushFileBuffers(file.value)!=0;
    CloseHandle(file.value); file.value=INVALID_HANDLE_VALUE;
    if(ok) ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) {DeleteFileW(temp.c_str()); error=loaderText("err.settings_write");}
    return ok;
}
static bool matchesSessionSettings(const std::string& world,std::string& error) {
    for(const auto& loaded:sessionManifests) {
        Package package;
        if(!readPackage(loaded.first,package,error) || package.manifest!=loaded.second) {error=loaderText("err.resident_manifest")+loaded.first; return false;}
    }
    for(const auto& loaded:sessionSettings) {
        Package package; SettingValues values;
        if(!readPackage(loaded.first,package,error) || !readModSettings(world,package,values,error)) return false;
        if(values!=loaded.second) {error=loaderText("ui.restart_world"); return false;}
    }
    return true;
}
struct SettingsView {Package package; SettingValues values; std::string error;};
static std::map<std::string,SettingsView> settingsViews;
static std::string settingsWorld;
static void refreshSettingsViews() {
    settingsWorld=worldId(selectedKey); settingsViews.clear();
    for(const auto& file:modFiles) {
        auto& view=settingsViews[file];
        if(readPackage(file,view.package,view.error) && !view.package.settings.empty()) readModSettings(selectedKey,view.package,view.values,view.error);
    }
    std::string error;
    settingsRestartPending=sessionCommitted && sessionWorld==settingsWorld && !matchesSessionSettings(selectedKey,error);
}
static bool changeSetting(const std::string& id,const std::string& key,const std::string& value) {
    if(settingsWorld!=worldId(selectedKey)) return false;
    auto& view=settingsViews.at(id);
    if(!view.error.empty()) return false;
    auto next=view.values; next[key]=value;
    if(!saveModSettings(selectedKey,view.package,next,view.values,view.error)) return false;
    view.values=std::move(next); launchError.clear();
    std::string error;
    settingsRestartPending=sessionCommitted && sessionWorld==settingsWorld && !matchesSessionSettings(selectedKey,error);
    return true;
}
