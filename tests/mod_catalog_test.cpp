#include "../src/loader.cpp"
#include <fstream>
#include <cassert>

int main() {
    const auto root=std::filesystem::current_path()/("catalog-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root/L"ModLoader");
    folder=(root/L"ModLoader").wstring();
    refreshMods();
    assert(modFiles.empty() && catalogError.empty());
    std::filesystem::create_directories(root/L"Mods"/L"EnshroudedModLoader"/L"CarryChest");
    // Deliberately not executable: listing must never attempt LoadLibrary.
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"alpha.dll") << "not an executable";
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"CarryChest"/L"CarryChest.dll") << "not an executable";
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"readme.txt") << "ignored";
    std::filesystem::create_directory(root/L"Mods"/L"EnshroudedModLoader"/L"directory.dll");
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"alpha.dll.mod") << "ENSHROUDED_PACKAGE_V1\ninvalid";
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"CarryChest"/L"CarryChest.dll.mod") << "ENSHROUDED_PACKAGE_V1\ninvalid";
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"foreign.dll") << "foreign";
    std::ofstream(root/L"Mods"/L"EnshroudedModLoader"/L"foreign.dll.mod") << "OTHER_PACKAGE_FORMAT";
    // A large shared tree must not consume this loader's scan budget.
    std::filesystem::create_directory(root/L"Mods"/L"OtherFramework");
    for(int i=0;i<8300;++i) std::ofstream(root/L"Mods"/L"OtherFramework"/std::to_wstring(i)) << "ignored";
    modPage=8; refreshMods();
    assert(catalogError.empty() && modPage==0 && modFiles.size()==2);
    assert(modFiles[0]=="alpha.dll" && !catalog["alpha.dll"].valid);
    assert(modFiles[1]=="carrychest/carrychest.dll");
    std::filesystem::remove(root/L"Mods"/L"EnshroudedModLoader"/L"alpha.dll");
    refreshMods();
    assert(modFiles.size()==1);
    enabledMods={"foreign.dll","missing.dll"};
    refreshMods();
    assert(modFiles.size()==3 && !catalog["foreign.dll"].valid && !catalog["missing.dll"].present);
    puts("Catalog: missing folder, nested DLLs, case, directories, non-DLL files and refresh passed.");
}
