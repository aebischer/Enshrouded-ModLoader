#include "../src/loader.cpp"
#include <cassert>
#include <random>
static const std::string World="0101-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static void manifest(const std::filesystem::path& path,const std::string& kind="mod",const std::string& extra="",const std::string& version="1.0") {
    std::ofstream(path.wstring()+L".mod",std::ios::binary)<<"ENSHROUDED_PACKAGE_V1\nkind="<<kind<<"\nversion="<<version<<"\nabi=1\n"<<extra;
}
static void child(const std::wstring& mode,const std::filesystem::path& root) {
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,32768);
    std::wstring command=L"\""+std::wstring(exe)+L"\" "+mode+L" \""+root.wstring()+L"\"";
    STARTUPINFOW startup{}; startup.cb=sizeof startup; PROCESS_INFORMATION process{};
    assert(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
    assert(WaitForSingleObject(process.hProcess,20000)==WAIT_OBJECT_0);
    DWORD code; assert(GetExitCodeProcess(process.hProcess,&code) && code==0);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
}
int wmain(int argc,wchar_t** argv) {
    const auto root=argc>2?std::filesystem::path(argv[2]):std::filesystem::current_path()/("regression-"+std::to_string(GetCurrentProcessId()));
    folder=(root/L"ModLoader").wstring();
    const auto mods=root/L"Mods"/L"EnshroudedModLoader";
    std::string error;
    if(argc>2) {
        if(std::wstring(argv[1])==L"--load") {
            assert(prepareWorldMods(World,error));
            assert(residentMods.size()==2 && sessionCommitted && !sessionTainted);
            auto dep=GetModuleHandleW(L"audit_dependency.dll"); assert(dep);
            auto attached=reinterpret_cast<int(*)()>(GetProcAddress(dep,"AuditDependencyAttached")); assert(attached && attached()==1);
            assert(prepareWorldMods(World,error) && residentMods.size()==2);
        } else if(std::wstring(argv[1])==L"--throw") {
            bool caught=false; try {prepareWorldMods(World,error);} catch(...) {caught=true;}
            assert(caught && sessionTainted && residentMods.size()==1);
            assert(!prepareWorldMods(World,error));
        }
        return 0;
    }
    std::filesystem::create_directories(mods);
    std::filesystem::create_directories(std::filesystem::path(folder)/L"worlds");
    for(const auto* name:{L"audit_parent.dll",L"audit_dependency.dll",L"mod_test_plugin.dll",L"mod_test_throw.dll"})
        std::filesystem::copy_file(std::filesystem::current_path()/name,mods/name);
    manifest(mods/L"mod_test_plugin.dll"); manifest(mods/L"mod_test_throw.dll");
    auto write=[&](const std::string& body){std::ofstream(profilePath(World),std::ios::binary)<<body;};
    std::set<std::string> actual;
    assert(readProfile(World,actual,error) && actual.empty() && error.empty());
    selectedKey=World; openProfile(World); assert(enabledMods.empty() && profileError.empty());
    assert(toggleMod("mod_test_plugin.dll"));
    write("ENSHROUDED_MODS_V1\r\nmod_test_plugin.dll\r\n");
    assert(readProfile(World,actual,error) && actual.count("mod_test_plugin.dll"));
    write("\xef\xbb\xbf" "ENSHROUDED_MODS_V1\r\n"); assert(readProfile(World,actual,error));
    write(std::string("ENSHROUDED_MODS_V1\n")+static_cast<char>(-1)+".dll\n");
    openProfile(World); assert(!profileError.empty() && enabledMods.empty());
    write("ENSHROUDED_MODS_V1\nmod_test_plugin.dll\n../invalid.dll\n");
    assert(!readProfile(World,actual,error) && actual.empty());
    write("ENSHROUDED_MODS_V1\nA.dll\na.dll\n"); assert(!readProfile(World,actual,error));
    std::set<std::string> many; for(size_t i=0;i<MaxMods;++i) many.insert("mod"+std::to_string(i)+".dll");
    assert(saveProfile(World,many,error) && readProfile(World,actual,error) && actual==many);
    many.insert("one_more.dll"); assert(!saveProfile(World,many,error));
    assert(readProfile(World,actual,error) && actual.size()==MaxMods);
    assert(!saveProfile(World,{std::string(MaxModPath,'a')+".dll"},error));
    write(std::string(MaxProfileBytes+1,'a')); assert(!readProfile(World,actual,error));
    assert(modId("../escape.dll").empty() && modId("x.dll:ads").empty());
    assert(modId("folder. /x.dll").empty() && worldId(std::string(32,'0')).empty());
    assert(saveProfile(World,{},error)); openProfile(World);
    assert(saveProfile(World,{"external.dll"},error));
    assert(!toggleMod("mod_test_plugin.dll"));
    assert(readProfile(World,actual,error) && actual==std::set<std::string>{"external.dll"});
    { FileHandle lock(CreateFileW((profilePath(World).wstring()+L".lock").c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,0,nullptr));
      assert(lock.value!=INVALID_HANDLE_VALUE && !saveProfile(World,{},error)); }
    assert(saveProfile(World,{},error)); openProfile(World);
    assert(toggleMod("mod_test_plugin.dll") && toggleMod("mod_test_plugin.dll"));
    // Alias and hard-link identities are rejected before any executable is loaded.
    std::filesystem::create_hard_link(mods/L"mod_test_plugin.dll",mods/L"alias.dll"); manifest(mods/L"alias.dll");
    LoadPlan plan;
    assert(!makeLoadPlan({"alias.dll","mod_test_plugin.dll"},plan,error));
    assert(!GetModuleHandleW(L"mod_test_plugin.dll"));
    plan=LoadPlan{};
    // Import table cannot silently activate a disabled/undeclared DLL.
    manifest(mods/L"audit_parent.dll"); manifest(mods/L"audit_dependency.dll","library");
    assert(!makeLoadPlan({"audit_parent.dll"},plan,error) && error.find(loaderText("err.undeclared"))!=std::string::npos);
    assert(!GetModuleHandleW(L"audit_dependency.dll")); plan=LoadPlan{};
    manifest(mods/L"audit_parent.dll","mod","require=audit_dependency.dll@2.0\n");
    assert(!makeLoadPlan({"audit_parent.dll"},plan,error)); plan=LoadPlan{};
    manifest(mods/L"audit_parent.dll","mod","require=audit_dependency.dll@1.0\n");
    manifest(mods/L"audit_dependency.dll","mod");
    assert(!makeLoadPlan({"audit_parent.dll"},plan,error) && error.find(loaderText("err.required_disabled"))!=std::string::npos); plan=LoadPlan{};
    manifest(mods/L"audit_dependency.dll","library","require=audit_parent.dll@1.0\n");
    assert(!makeLoadPlan({"audit_parent.dll"},plan,error)); plan=LoadPlan{};
    manifest(mods/L"audit_dependency.dll","library");
    assert(makeLoadPlan({"audit_parent.dll"},plan,error) && plan.ordered.size()==2 && !plan.ordered.front().mod);
    // Preflight retains read-only handles: replacing/changing a validated DLL is refused.
    FileHandle writer(CreateFileW((mods/L"audit_dependency.dll").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
    assert(writer.value==INVALID_HANDLE_VALUE); plan=LoadPlan{};
    assert(saveProfile(World,{"audit_parent.dll"},error)); openProfile(World); refreshMods();
    assert(requiredLibraries.count("audit_dependency.dll") && catalog.at("audit_dependency.dll").library);
    assert(!toggleMod("audit_dependency.dll"));
    child(L"--load",root);
    assert(saveProfile(World,{"mod_test_throw.dll"},error)); child(L"--throw",root);
    // Mandatory export and exact manifest ABI, prior to DllMain.
    manifest(mods/L"audit_dependency.dll","mod");
    assert(!makeLoadPlan({"audit_dependency.dll"},plan,error)); plan=LoadPlan{};
    std::ofstream((mods/L"mod_test_plugin.dll").wstring()+L".mod")<<"ENSHROUDED_PACKAGE_V1\nkind=mod\nversion=1.0\nabi=99\n";
    assert(!makeLoadPlan({"mod_test_plugin.dll"},plan,error)); plan=LoadPlan{};
    std::ofstream((mods/L"mod_test_plugin.dll").wstring()+L".mod")<<"ENSHROUDED_PACKAGE_V1\nkind=mod\nversion=1.0\nabi=1\nunknown=value";
    Package invalidPackage; assert(!readPackage("mod_test_plugin.dll",invalidPackage,error));
    // Signature validation covers the five exact hook targets without executing the game.
    const wchar_t* gameExe=L"D:\\SteamLibrary\\steamapps\\common\\Enshrouded\\enshrouded.exe";
    assert(matchingExe(gameExe));
    {
        FileHandle exe(CreateFileW(gameExe,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
        FileHandle mapping(CreateFileMappingW(exe.value,nullptr,PAGE_READONLY|SEC_IMAGE_NO_EXECUTE,0,0,nullptr));
        auto mapped=MapViewOfFile(mapping.value,FILE_MAP_READ,0,0,0); assert(mapped);
        imageBase=reinterpret_cast<uintptr_t>(mapped); assert(unchangedHookTargets(gameExe));
        UnmapViewOfFile(mapped); imageBase=0;
    }
    // Malformed PE fuzzing: truncated headers, mutated real DLL, arbitrary bytes.
    std::string real; assert(boundedRead(mods/L"mod_test_plugin.dll",128*1024*1024,real));
    PeFacts facts; assert(inspectPe(real,facts) && facts.initialize);
    for(size_t n=0;n<4096;++n) {auto cut=real.substr(0,n); inspectPe(cut,facts);}
    std::mt19937 rng(42);
    for(int i=0;i<2000;++i) {
        auto bad=real;
        for(int j=0;j<8;++j) bad[rng()%std::min(size_t(4096),bad.size())]=static_cast<char>(rng());
        inspectPe(bad,facts);
    }
    // Pagination always selects an entry actually displayed on the new page.
    modFiles.assign(13,"a.dll"); selectedMod=0; changeModPage(1); assert(selectedMod==6);
    changeModPage(2); assert(selectedMod==12); changeModPage(0); assert(selectedMod==0);
    assert(ProxyDllCanUnloadNow()==S_FALSE);
    puts("PASS: encoding, bounds, missing profile, conflict, locks, aliases, manifests, imports, versions, cycles, dependency loading, throwing init, PE fuzz, pagination.");
}
