#include "../src/loader.cpp"
#include <cassert>
int main() {
    const auto root=std::filesystem::current_path()/("empty-mods-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root/L"Mods"/L"EnshroudedModLoader");
    folder=(root/L"ModLoader").wstring();
    const auto source=std::filesystem::current_path()/L"test-mods";
    for(const auto* name:{L"Test_A.dll",L"Test_B.dll",L"Test_A.dll.mod",L"Test_B.dll.mod"})
        std::filesystem::copy_file(source/name,root/L"Mods"/L"EnshroudedModLoader"/name);
    const std::string world(32,'a'),other(32,'b');
    std::string error;
    assert(saveProfile(world,{"test_a.dll","test_b.dll"},error));
    assert(prepareWorldMods(world,error));
    assert(residentMods.size()==2 && sessionCommitted && !sessionTainted);
    std::string before,after;
    assert(boundedRead(std::filesystem::path(folder)/L"mod-loader.log",16384,before));
    assert(before.find("Test_A : mod de test initialise, monde="+world)!=std::string::npos);
    assert(before.find("Test_B : mod de test initialise, monde="+world)!=std::string::npos);
    assert(prepareWorldMods(world,error));
    assert(boundedRead(std::filesystem::path(folder)/L"mod-loader.log",16384,after) && before==after);
    assert(saveProfile(other,{},error) && !prepareWorldMods(other,error));
    puts("PASS: both empty mods load, log the world, initialize once and require restart for another world.");
}
