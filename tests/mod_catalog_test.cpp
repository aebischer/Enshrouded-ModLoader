#include "../src/loader.cpp"
#include <fstream>
#include <cassert>

int main() {
    const auto root=std::filesystem::current_path()/("catalog-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root/L"ModLoader");
    folder=(root/L"ModLoader").wstring();
    refreshMods();
    assert(modFiles.empty() && catalogError.empty());
    std::filesystem::create_directories(root/L"Mods"/L"CarryChest");
    // Deliberately not executable: listing must never attempt LoadLibrary.
    std::ofstream(root/L"Mods"/L"alpha.dll") << "not an executable";
    std::ofstream(root/L"Mods"/L"CarryChest"/L"CarryChest.dll") << "not an executable";
    std::ofstream(root/L"Mods"/L"readme.txt") << "ignored";
    std::filesystem::create_directory(root/L"Mods"/L"directory.dll");
    modPage=8; refreshMods();
    assert(catalogError.empty() && modPage==0 && modFiles.size()==2);
    assert(modFiles[0]=="alpha.dll");
    assert(modFiles[1]=="carrychest/carrychest.dll");
    std::filesystem::remove(root/L"Mods"/L"alpha.dll");
    refreshMods();
    assert(modFiles.size()==1);
    puts("Catalog: missing folder, nested DLLs, case, directories, non-DLL files and refresh passed.");
}
