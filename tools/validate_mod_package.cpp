// Use the loader's exact non-executing preflight; do not duplicate its rules.
#include "../src/loader.cpp"
int wmain(int argc,wchar_t** argv) {
    if(argc<3) {
        fputs("Usage: validate_mod_package.exe <root containing Mods/EnshroudedModLoader> <mod/id.dll> [other enabled mod ids]\n",stderr);
        return 2;
    }
    try {
        folder=(std::filesystem::absolute(argv[1])/L"ModLoader").wstring();
        std::set<std::string> desired;
        for(int i=2;i<argc;++i) {
            auto id=modId(std::filesystem::path(argv[i]).u8string());
            if(id.empty() || !desired.insert(id).second) {fputs("Invalid or duplicate mod id\n",stderr);return 2;}
        }
        LoadPlan plan; std::string error;
        if(!makeLoadPlan(desired,plan,error)) {fprintf(stderr,"INVALID: %s\n",error.c_str());return 1;}
        for(const auto& item:plan.ordered) printf("OK %s %s version=%s ABI=1 settings=%zu\n",item.mod?"mod":"library",item.id.c_str(),item.version.c_str(),item.settings.size());
        puts("Static package validation passed. No mod DLL executed; gameplay compatibility is not certified.");
        return 0;
    } catch(const std::exception& e) {fprintf(stderr,"Validation failed: %s\n",e.what());return 1;}
      catch(...) {fputs("Validation failed\n",stderr);return 1;}
}
