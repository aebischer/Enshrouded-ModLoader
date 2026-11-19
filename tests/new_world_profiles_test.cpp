#include "../src/loader.cpp"
#include <cassert>
int main() {
    const auto root=std::filesystem::current_path()/("profile-reset-test-"+std::to_string(GetCurrentProcessId()));
    folder=(root/L"ModLoader").wstring();
    std::filesystem::create_directories(root/L"Mods"/L"EnshroudedModLoader");
    const auto source=std::filesystem::current_path()/L"test-mods";
    for(const auto* name:{L"Test_A.dll",L"Test_A.dll.mod"}) std::filesystem::copy_file(source/name,root/L"Mods"/L"EnshroudedModLoader"/name);
    const std::string newWorld(32,'a'),oldWorld(32,'b');
    std::string error; std::set<std::string> entries;
    // Missing files have the same meaning for all worlds; browsing does not write.
    for(const auto& world:{newWorld,oldWorld}) {
        assert(readProfile(world,entries,error) && entries.empty() && error.empty());
        selectedKey=world; openProfile(world); refreshMods();
        assert(profileError.empty() && enabledMods.empty() && !profileExists(world));
        assert(toggleMod("test_a.dll"));
        assert(readProfile(world,entries,error) && entries.count("test_a.dll"));
    }
    // Deleting a damaged file resets the profile and immediately restores activation.
    std::ofstream(profilePath(oldWorld),std::ios::binary)<<"invalid";
    openProfile(oldWorld); assert(!profileError.empty());
    std::filesystem::remove(profilePath(oldWorld));
    openProfile(oldWorld); assert(profileError.empty() && enabledMods.empty());
    assert(toggleMod("test_a.dll"));
    // A stale page must not silently restore choices deleted externally.
    std::filesystem::remove(profilePath(oldWorld));
    assert(!toggleMod("test_a.dll"));
    assert(!profileExists(oldWorld));
    openProfile(oldWorld); assert(profileError.empty() && enabledMods.empty());
    // Missing means vanilla on startup too, but never unloads resident native code.
    assert(prepareWorldMods(oldWorld,error) && sessionMods.empty());
    assert(needsRestart(newWorld,{"test_a.dll"}));
    sessionCommitted=false; // model a fresh process before loading the real test DLL
    assert(prepareWorldMods(newWorld,error) && residentMods.size()==1);
    std::filesystem::remove(profilePath(newWorld));
    assert(readProfile(newWorld,entries,error) && entries.empty());
    assert(needsRestart(newWorld,entries) && !prepareWorldMods(newWorld,error));
    assert(residentMods.size()==1);
    puts("PASS: missing profiles reset all worlds; direct activation; corrupt-file deletion; stale-page conflict; vanilla startup; resident mods still require restart.");
}
