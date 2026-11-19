// Audit-only reproduction cases. Does not touch the game or installed DLL.
#include "../src/loader.cpp"
#include <iostream>
int main(int argc,char** argv) {
    if(argc==2) {
        folder=(std::filesystem::u8path(argv[1])/L"ModLoader").wstring();
        std::string error;
        const std::string key(32,'a');
        saveProfile(key,{"real/probe.dll","alias/probe.dll"},error);
        std::cout<<"Aliased profile starts: "<<prepareWorldMods(key,error)<<" error="<<error<<"\n";
        if(residentMods.size()==2) {
            auto count=reinterpret_cast<int(*)()>(GetProcAddress(residentMods[0],"ModTestCount"));
            std::cout<<"Both aliases return same module: "<<(residentMods[0]==residentMods[1])<<"\n";
            std::cout<<"Same plugin initialization count: "<<(count?count():-1)<<"\n";
        }
        return 0;
    }
    const auto root=std::filesystem::current_path()/("audit-profiles-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root/L"ModLoader"/L"worlds");
    folder=(root/L"ModLoader").wstring();
    const std::string key="0101-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const auto path=root/L"ModLoader"/L"worlds"/(worldId(key)+".mods");
    std::string error; std::set<std::string> actual;
    auto write=[&](const std::string& value) {std::ofstream out(path,std::ios::binary); out<<value;};
    write("ENSHROUDED_MODS_V1\r\na.dll\r\n");
    std::cout<<"CRLF header accepted: "<<readProfile(key,actual,error)<<" error="<<error<<"\n";
    write(std::string("ENSHROUDED_MODS_V1\n")+char(0xff)+".dll\n");
    bool threw=false;
    try {openProfile(key);} catch(const std::exception& e) {threw=true; std::cout<<"Invalid UTF-8 exception: "<<e.what()<<"\n";}
    std::cout<<"openProfile throws for invalid UTF-8: "<<threw<<"\n";
    std::set<std::string> huge;
    for(int i=0;i<1025;++i) huge.insert("mod"+std::to_string(i)+".dll");
    std::cout<<"Save 1025 entries: "<<saveProfile(key,huge,error)<<"\n";
    std::cout<<"Read same saved profile: "<<readProfile(key,actual,error)<<" error="<<error<<"\n";
    std::string longName(33000,'a'); longName+=".dll";
    std::cout<<"Save overlong entry: "<<saveProfile(key,{longName},error)<<"\n";
    std::cout<<"Read same long profile: "<<readProfile(key,actual,error)<<" error="<<error<<"\n";
    write("ENSHROUDED_MODS_V1\na.dll\n");
    selectedKey=key; openProfile(key);
    // Simulate an external installer updating the profile while the UI is open.
    saveProfile(key,{"a.dll","external.dll"},error);
    toggleMod("other.dll"); readProfile(key,actual,error);
    std::cout<<"Concurrent external selection preserved: "<<actual.count("external.dll")<<"\n";
    std::cout<<"Zero world GUID accepted: "<<!worldId(std::string(32,'0')).empty()<<"\n";
    std::filesystem::create_directories(root/L"Mods"/L"EnshroudedModLoader");
    for(const auto* name:{L"audit_parent.dll",L"audit_dependency.dll"})
        std::filesystem::copy_file(std::filesystem::current_path()/name,root/L"Mods"/L"EnshroudedModLoader"/name);
    saveProfile(key,{"audit_parent.dll"},error);
    std::cout<<"Parent only profile starts: "<<prepareWorldMods(key,error)<<" error="<<error<<"\n";
    auto dependency=GetModuleHandleW(L"audit_dependency.dll");
    auto attached=reinterpret_cast<int(*)()>(GetProcAddress(dependency,"AuditDependencyAttached"));
    std::cout<<"Disabled dependency DllMain ran: "<<(attached?attached():0)<<"\n";
    std::cout<<"Dependency tracked in sessionMods: "<<sessionMods.count("audit_dependency.dll")<<"\n";
}
