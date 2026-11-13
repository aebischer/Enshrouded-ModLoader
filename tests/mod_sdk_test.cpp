#include "../src/loader.cpp"
#include <cassert>
int main() {
    const auto source=std::filesystem::current_path()/L"sdk-package"/L"Mods"/L"example";
    const auto root=std::filesystem::current_path()/("sdk-test-"+std::to_string(GetCurrentProcessId()));
    const auto dest=root/L"Mods"/L"example";
    std::filesystem::create_directories(dest);
    for(const auto* file:{L"example_mod.dll",L"example_mod.dll.mod"}) std::filesystem::copy_file(source/file,dest/file);
    folder=(root/L"ModLoader").wstring();
    const std::string id="example/example_mod.dll",world(32,'a'); std::string error;
    {LoadPlan plan; assert(makeLoadPlan({id},plan,error)); assert(!GetModuleHandleW(L"example_mod.dll"));}
    Package package; assert(readPackage(id,package,error));
    auto initial=defaultSettings(package),custom=initial; custom["mode"]="Rapide";custom["messages"]="false";custom["intensity"]="125";
    assert(saveModSettings(world,package,custom,initial,error));
    assert(saveProfile(world,{id},error) && prepareWorldMods(world,error));
    std::string logText; assert(boundedRead(std::filesystem::path(folder)/L"mod-loader.log",16384,logText));
    assert(logText.find("intensity=125")!=std::string::npos && logText.find("mode=Rapide")!=std::string::npos && logText.find("messages=false")!=std::string::npos);
    assert(prepareWorldMods(world,error) && residentMods.size()==1);
    puts("PASS: public SDK example validates without execution, receives world settings and initializes once.");
}
