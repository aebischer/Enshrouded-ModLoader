#include "../src/loader.cpp"
#include <cassert>
static const std::string A="0101-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static const std::string B="0102-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
static std::set<std::string> active={"nested/probe.dll"};

static void child(const std::wstring& mode,const std::filesystem::path& root) {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr,exe,MAX_PATH);
    std::wstring command=L"\""+std::wstring(exe)+L"\" "+mode+L" \""+root.wstring()+L"\"";
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
    assert(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
    assert(WaitForSingleObject(process.hProcess,15000)==WAIT_OBJECT_0);
    DWORD code; assert(GetExitCodeProcess(process.hProcess,&code) && code==0);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
}
int wmain(int argc,wchar_t** argv) {
    const auto root=argc>2?std::filesystem::path(argv[2]):std::filesystem::current_path()/("profiles-test-"+std::to_string(GetCurrentProcessId()));
    folder=(root/L"ModLoader").wstring();
    std::string error;
    if(argc>2) {
        if(std::wstring(argv[1])==L"--restart") {
            assert(prepareWorldMods(B,error));
            assert(sessionWorld==worldId(B) && residentMods.size()==1 && !sessionTainted);
        } else {
            assert(saveProfile(A,{"nested/probe.dll","zreject.dll"},error));
            assert(!prepareWorldMods(A,error));
            assert(sessionTainted && residentMods.size()==2);
            assert(!prepareWorldMods(A,error)); // no second initialization after partial failure
            assert(residentMods.size()==2);
            assert(saveProfile(A,{},error));
            assert(!prepareWorldMods(A,error)); // even disabling everything needs a clean process
        }
        return 0;
    }
    std::filesystem::create_directories(root/L"Mods"/L"EnshroudedModLoader"/L"nested");
    std::filesystem::copy_file("mod_test_plugin.dll",root/L"Mods"/L"EnshroudedModLoader"/L"nested"/L"Probe.DLL");
    std::filesystem::copy_file("mod_test_reject.dll",root/L"Mods"/L"EnshroudedModLoader"/L"ZReject.dll");
    for(const auto& path:{root/L"Mods"/L"EnshroudedModLoader"/L"nested"/L"Probe.DLL",root/L"Mods"/L"EnshroudedModLoader"/L"ZReject.dll"})
        std::ofstream(path.wstring()+L".mod")<<"ENSHROUDED_PACKAGE_V1\nkind=mod\nversion=1.0\nabi=1\n";
    assert(worldId(A)==worldId("0204-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    assert(worldId("../invalid").empty());
    assert(modId("Nested\\Probe.DLL")=="nested/probe.dll");
    assert(modId("../escape.dll").empty() && modId("D:\\escape.dll").empty());
    assert(modId("x.dll:stream").empty() && modId("/escape.dll").empty());
    std::set<std::string> restored;
    assert(readProfile(A,restored,error) && restored.empty());
    assert(saveProfile(A,active,error));
    assert(saveProfile(B,{},error));
    assert(readProfile(A,restored,error) && restored==active);
    assert(readProfile(B,restored,error) && restored.empty());
    // An interrupted write cannot replace the previous complete profile.
    std::ofstream(root/L"ModLoader"/L"worlds"/(worldId(A)+".mods.tmp"))<<"partial";
    assert(readProfile(A,restored,error) && restored==active);
    selectedKey=A; openProfile(A); refreshMods();
    assert(isEnabled("nested/Probe.DLL"));
    assert(toggleMod("nested/Probe.DLL") && !isEnabled("nested/probe.dll"));
    assert(toggleMod("nested/Probe.DLL") && isEnabled("nested/probe.dll"));
    assert(!needsRestart(A,enabledMods));
    assert(prepareWorldMods(A,error));
    assert(residentMods.size()==1 && sessionCommitted && !sessionTainted);
    const auto count=reinterpret_cast<int(*)()>(GetProcAddress(residentMods[0],"ModTestCount"));
    const auto world=reinterpret_cast<const char*(*)()>(GetProcAddress(residentMods[0],"ModTestWorld"));
    assert(count && count()==1 && world && world()==worldId(A));
    assert(prepareWorldMods(A,error) && count()==1);
    assert(needsRestart(B,active) && !prepareWorldMods(B,error));
    assert(saveProfile(A,{},error));
    assert(!prepareWorldMods(A,error) && count()==1);
    assert(saveProfile(A,active,error));
    assert(prepareWorldMods(A,error)); // reverting to the resident set needs no restart
    assert(saveProfile(B,active,error));
    child(L"--restart",root);
    child(L"--failure",root);
    assert(saveProfile(B,{"missing.dll"},error));
    selectedKey=B; openProfile(B); refreshMods();
    assert(std::find(modFiles.begin(),modFiles.end(),"missing.dll")!=modFiles.end());
    assert(toggleMod("missing.dll"));
    std::ofstream(root/L"ModLoader"/L"worlds"/(worldId(B)+".mods"))<<"broken";
    assert(!readProfile(B,restored,error));
    puts("Profiles: isolated worlds, atomic persistence, toggles, real DLL init, restart, partial failure and missing mods passed.");
}
