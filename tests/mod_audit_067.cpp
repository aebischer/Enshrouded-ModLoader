#include "../src/loader.cpp"
#include <cassert>
#include <new>
static bool failNextAllocation=false;
void* operator new(size_t n) {if(failNextAllocation){failNextAllocation=false;throw std::bad_alloc();} if(auto p=malloc(n?n:1)) return p; throw std::bad_alloc();}
void operator delete(void* p) noexcept {free(p);}
void operator delete(void* p,size_t) noexcept {free(p);}
static unsigned char fakeWorld[0xf8]{};
static unsigned char* __fastcall lookup(uint16_t) {return fakeWorld;}
static bool detailsCalled=false;
static void __fastcall details(void*,void*,void*) {detailsCalled=true;}
static void manifest(const std::filesystem::path& path,const std::string& extra) {
    std::ofstream(path.wstring()+L".mod",std::ios::binary)<<"ENSHROUDED_PACKAGE_V1\nkind=mod\nversion=1.0\nabi=1\n"<<extra;
}
int main() {
    const auto root=std::filesystem::current_path()/("audit-067-"+std::to_string(GetCurrentProcessId()));
    folder=(root/L"ModLoader").wstring();
    const auto mods=root/L"Mods"; std::filesystem::create_directories(mods);
    const auto dll=mods/L"mod_test_plugin.dll";
    std::filesystem::copy_file(std::filesystem::current_path()/L"mod_test_plugin.dll",dll);
    const std::string world(32,'a'); std::string error;
    const auto dep=mods/L"audit_dependency.dll";
    std::filesystem::copy_file(std::filesystem::current_path()/L"audit_dependency.dll",dep);
    std::ofstream(dep.wstring()+L".mod")<<"ENSHROUDED_PACKAGE_V1\nkind=library\nversion=1.0\nabi=1\n";
    manifest(dll,"require=audit_dependency.dll@2.0\n");
    enabledMods={"mod_test_plugin.dll"}; updatePlanPreview();
    assert(planError.find(loaderText("err.version"))!=std::string::npos);
    {LoadPlan plan; assert(!makeLoadPlan(enabledMods,plan,error) && error.find(loaderText("err.version"))!=std::string::npos);}
    puts("REGRESSION FIX VERIFIED: preview and launch both reject incompatible dependency versions.");
    manifest(dll,"require=audit_dependency.dll@1.0\n");
    updatePlanPreview(); assert(planError.empty() && requiredLibraries.count("audit_dependency.dll"));
    std::ofstream(dep.wstring()+L".mod")<<"ENSHROUDED_PACKAGE_V1\nkind=library\nversion=1.0\nabi=1\nrequire=mod_test_plugin.dll@1.0\n";
    updatePlanPreview(); assert(planError.find(loaderText("err.dependencies"))!=std::string::npos);
    manifest(dep,""); updatePlanPreview(); assert(planError.find(loaderText("err.required_disabled"))!=std::string::npos);
    manifest(dll,"setting=mode|choice|Mode|A|A|B\n");
    Package cached; assert(readPackage("mod_test_plugin.dll",cached,error));
    auto oldValues=defaultSettings(cached),newValues=oldValues; newValues["mode"]="B";
    manifest(dll,"setting=mode|choice|Mode|A|A|C\n");
    assert(!saveModSettings(world,cached,newValues,oldValues,error));
    Package fresh; SettingValues actual;
    assert(readPackage(cached.id,fresh,error) && readModSettings(world,fresh,actual,error) && actual.at("mode")=="A");
    puts("REGRESSION FIX VERIFIED: changed manifest rejects stale save and preserves defaults.");
    manifest(dll,"");
    assert(saveProfile(world,{cached.id},error) && prepareWorldMods(world,error));
    assert(sessionSettings.empty() && residentMods.size()==1);
    manifest(dll,"setting=flag|bool|Flag|true\n");
    assert(!prepareWorldMods(world,error) && error.find(loaderText("err.resident_manifest"))!=std::string::npos);
    assert(sessionSettings.empty() && !GetProcAddress(residentMods[0],"EnshroudedModConfigure"));
    puts("REGRESSION FIX VERIFIED: schema added to resident mod forces restart.");
    manifest(dll,""); assert(prepareWorldMods(world,error));
    unsigned char state[0x80]{}, menu[0x248]{};
    putAt<uint64_t>(menu,0xa8,7); putAt<uint16_t>(state,0x41,2);
    WorldPageReturn snapshot; snapshot.capture(state,menu);
    putAt<uint64_t>(menu,0xa8,0); putAt<uint16_t>(state,0x41,1);
    snapshot.apply(state,menu);
    assert(readAt<uint64_t>(menu,0xa8)==7 && readAt<uint16_t>(state,0x41)==2);
    fakeWorld[0xe0]=1; fakeWorld[0x14]=1;
    worldPageReturn.pending=true; worldPageReturn.slot=1; restoringWorldPage=true;
    selectedKey=worldKey(fakeWorld); originalWorldDetails=details;
    imageBase=reinterpret_cast<uintptr_t>(&lookup)-0xd436b0;
    bool escaped=false;
    failNextAllocation=true;
    try {worldDetails(nullptr,nullptr,nullptr);} catch(const std::bad_alloc&) {escaped=true;}
    failNextAllocation=false;
    assert(!escaped && detailsCalled && !worldPageReturn.pending);
    puts("REGRESSION FIX VERIFIED: allocation failure is contained; native handler runs and pending clears.");
    puts("Audit probes completed in isolated workspace fixtures. No game files changed.");
}
