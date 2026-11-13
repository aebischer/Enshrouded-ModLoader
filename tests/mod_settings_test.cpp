#include "../src/loader.cpp"
#include <cassert>
static const std::string A(32,'a'),B(32,'b');
static void child(const std::wstring& mode,const std::filesystem::path& root) {
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,32768);
    std::wstring command=L"\""+std::wstring(exe)+L"\" "+mode+L" \""+root.wstring()+L"\"";
    STARTUPINFOW startup{}; startup.cb=sizeof startup; PROCESS_INFORMATION process{};
    assert(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
    assert(WaitForSingleObject(process.hProcess,20000)==WAIT_OBJECT_0);
    DWORD code; assert(GetExitCodeProcess(process.hProcess,&code) && code==0);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
}
static void* __fastcall fakePointer(void*,uint32_t* result,unsigned char,unsigned char) {*result=0x1204; return result;}
static unsigned char __fastcall fakeMenu(void*,void* output,uint16_t) {if(output) *static_cast<int*>(output)=42; return 1;}
static void verifyBackRouting() {
    originalPointerEvent=fakePointer;
    uint32_t event=0;
    drawingModBack=false; modBackClicked=false;
    assert(pointerEvent(nullptr,&event,1,1)==&event && event==0x1204 && !modBackClicked);
    drawingModBack=true;
    assert(pointerEvent(nullptr,&event,1,1)==&event && event==0x1200 && modBackClicked);
    drawingModBack=false; modBackClicked=false;
    originalMenuAction=fakeMenu;
    unsigned char action[0x48]{}; action[0]=1;
    int output=0;
    modsOpen=true; modBackRequested=false;
    assert(menuAction(action,&output,0x10)==0 && output==42 && modBackRequested);
    action[0]=2; assert(menuAction(action,nullptr,0x10)==1); // other actions unaffected
    modsOpen=false; modBackRequested=false; action[0]=1; backDrainFrames=2;
    assert(menuAction(action,nullptr,0x10)==0 && !modBackRequested);
    backDrainFrames=0; restoringWorldPage=true;
    assert(menuAction(action,nullptr,0x10)==0);
    restoringWorldPage=false; assert(menuAction(action,nullptr,0x10)==1);
    originalMenuAction=nullptr; originalPointerEvent=nullptr;
}
int wmain(int argc,wchar_t** argv) {
    verifyBackRouting();
    unsigned char beforePage[0x220]{},afterPage[0x220]{};
    beforePage[0x39]=1;
    putAt<uint16_t>(beforePage,0x41,0x102);
    putAt<uint32_t>(beforePage,0x3c,2);
    WorldPageReturn returnPage; returnPage.capture(beforePage);
    memset(afterPage,0x5a,sizeof afterPage); returnPage.apply(afterPage);
    assert(returnPage.pending && afterPage[0x39]==1 && readAt<uint16_t>(afterPage,0x41)==0x102 && readAt<uint32_t>(afterPage,0x3c)==2);
    for(size_t i=0;i<sizeof afterPage;++i) if(i!=0x39 && !(i>=0x3c && i<0x40) && !(i>=0x41 && i<0x43)) assert(afterPage[i]==0x5a);
    BackKeyLatch backKey;
    assert(!backKey.update(true,true)); // key already held on first observation
    assert(!backKey.update(true,false));
    assert(backKey.update(true,true));
    assert(!backKey.update(true,true)); // no repeat while held
    assert(!backKey.update(false,false));
    assert(!backKey.update(true,true)); // held while switching back to game
    assert(!backKey.update(true,false));
    assert(backKey.update(true,true));
    // Verify the exact native row contract without calling game code.
    struct RowGuard {unsigned char before[16],config[NativeOptionBytes],after[16];} guard;
    memset(&guard,0xa5,sizeof guard);
    const auto caption=text("Mode de test");
    std::vector<Text> options={text("Calme"),text("Normal"),text("Rapide")};
    assert(buildNativeOption(guard.config,caption,options,true));
    for(auto byte:guard.before) assert(byte==0xa5);
    for(auto byte:guard.after) assert(byte==0xa5);
    assert(guard.config[0xc3a]==1 && guard.config[0xc00]==3 && guard.config[0xc3c]==1 && guard.config[0xc3b]==0);
    assert(readAt<Text>(guard.config,0xc08).hash==caption.hash);
    for(size_t i=0;i<options.size();++i) {
        assert(readAt<uint32_t>(guard.config,i*0x30)==i);
        assert(readAt<Text>(guard.config,i*0x30+8).bytes==options[i].bytes);
    }
    assert(!buildNativeOption(guard.config,caption,{},true));
    assert(!buildNativeOption(guard.config,caption,std::vector<Text>(9,text("X")),true));
    assert(buildNativeOption(guard.config,caption,{text("Off"),text("On")},false));
    assert(guard.config[0xc3c]==0);
    const auto root=argc>2?std::filesystem::path(argv[2]):std::filesystem::current_path()/("mod-settings-test-"+std::to_string(GetCurrentProcessId()));
    folder=(root/L"ModLoader").wstring();
    std::string error;
    if(argc>2) {
        if(std::wstring(argv[1])==L"--bad") {assert(!prepareWorldMods(B,error) && residentMods.empty() && !sessionTainted); return 0;}
        const std::string world=std::wstring(argv[1])==L"--a"?A:B;
        assert(prepareWorldMods(world,error));
        assert(residentMods.size()==2 && sessionSettings.size()==2);
        std::string logText; assert(boundedRead(std::filesystem::path(folder)/L"mod-loader.log",65536,logText));
        const auto expectedA=world==A?"effects=true\nintensity=125\nmessages=false\nmode=Rapide\n":"effects=false\nintensity=100\nmessages=true\nmode=Calme\n";
        const auto expectedB=world==A?"effects=false\nintensity=50\nmessages=true\nmode=Rouge\n":"effects=true\nintensity=100\nmessages=false\nmode=Bleu\n";
        assert(logText.find("Test_A configuration monde="+world+" :\n"+expectedA)!=std::string::npos);
        assert(logText.find("Test_B configuration monde="+world+" :\n"+expectedB)!=std::string::npos);
        return 0;
    }
    std::filesystem::create_directories(root/L"Mods");
    const auto source=std::filesystem::current_path().parent_path()/L"dist"/L"test-mods";
    for(const auto* name:{L"Test_A.dll",L"Test_A.dll.mod",L"Test_B.dll",L"Test_B.dll.mod"}) std::filesystem::copy_file(source/name,root/L"Mods"/name);
    Package pa,pb; assert(readPackage("test_a.dll",pa,error) && readPackage("test_b.dll",pb,error));
    assert(pa.settings.size()==4 && pb.settings.size()==4);
    SettingValues va,vb;
    assert(readModSettings(A,pa,va,error) && va==defaultSettings(pa));
    assert(readModSettings(B,pa,vb,error) && va==vb);
    assert(va.at("intensity")=="100");
    const auto& percent=pa.settings.back();
    for(const auto* bad:{"-1","201","26","100%","nan","0100","100.0","999999999999"}) assert(!validSettingValue(percent,bad));
    struct SliderGuard {unsigned char before[16],config[NativeSliderBytes],after[16];} sg;
    memset(&sg,0xa5,sizeof sg); configureNativePercent(sg.config,percent);
    for(auto b:sg.before) assert(b==0xa5);
    for(auto b:sg.after) assert(b==0xa5);
    assert(readAt<uint64_t>(sg.config,0x90)==9 && readAt<uint32_t>(sg.config,0x4c)==6);
    assert(readAt<float>(sg.config,0xb0+4*4)==1.0f && sg.config[0x3a]==1 && sg.config[0x39]==0);
    // Existing settings files without the new key keep their values and gain 100%.
    std::filesystem::create_directories(settingsPath(B,pa.id).parent_path());
    std::ofstream(settingsPath(B,pa.id),std::ios::binary)<<"ENSHROUDED_SETTINGS_V1\nmode=Calme\n";
    SettingValues legacy; assert(readModSettings(B,pa,legacy,error) && legacy.at("intensity")=="100" && legacy.at("mode")=="Calme");
    std::filesystem::remove(settingsPath(B,pa.id));
    auto changed=va; changed["messages"]="false"; changed["effects"]="true"; changed["mode"]="Rapide"; changed["intensity"]="125";
    assert(saveModSettings(A,pa,changed,va,error));
    assert(readModSettings(B,pa,vb,error) && vb==defaultSettings(pa));
    auto invalid=changed; invalid["effects"]="yes";
    assert(!saveModSettings(A,pa,invalid,changed,error));
    invalid=changed; invalid["mode"]="Inconnu";
    assert(!saveModSettings(A,pa,invalid,changed,error));
    assert(!saveModSettings(A,pa,va,va,error)); // stale writer must not overwrite
    assert(readModSettings(A,pa,va,error) && va==changed);
    auto defaultsB=defaultSettings(pb),changedB=defaultsB;
    changedB["messages"]="true"; changedB["effects"]="false"; changedB["mode"]="Rouge"; changedB["intensity"]="50";
    assert(saveModSettings(A,pb,changedB,defaultsB,error));
    auto changedWorldB=vb; changedWorldB["mode"]="Calme";
    assert(saveModSettings(B,pa,changedWorldB,vb,error));
    assert(saveProfile(A,{"test_a.dll","test_b.dll"},error) && saveProfile(B,{"test_a.dll","test_b.dll"},error));
    std::ofstream(settingsPath(B,pb.id),std::ios::binary)<<"ENSHROUDED_SETTINGS_V1\nmode=invalid\n";
    child(L"--bad",root);
    std::filesystem::remove(settingsPath(B,pb.id));
    child(L"--a",root); child(L"--b",root); // real fresh processes receive distinct persisted values
    selectedKey=A; openProfile(A); refreshMods();
    assert(settingsViews.at("test_a.dll").values==changed);
    assert(toggleMod("test_a.dll")); // activation independent from settings
    assert(readModSettings(A,pa,va,error) && va==changed);
    assert(changeSetting("test_a.dll","mode","Normal"));
    assert(changeSetting("test_a.dll","mode","Rapide"));
    assert(toggleMod("test_a.dll"));
    assert(prepareWorldMods(A,error));
    assert(changeSetting("test_a.dll","mode","Calme"));
    assert(needsRestart(A,enabledMods) && !prepareWorldMods(A,error));
    assert(changeSetting("test_a.dll","mode","Rapide"));
    assert(!needsRestart(A,enabledMods) && prepareWorldMods(A,error));
    std::ofstream(settingsPath(B,pb.id),std::ios::binary)<<"ENSHROUDED_SETTINGS_V1\nmode=invalid\n";
    assert(!readModSettings(B,pb,vb,error));
    std::filesystem::remove(settingsPath(B,pb.id));
    assert(readModSettings(B,pb,vb,error) && vb==defaultSettings(pb));
    // Malformed schema rejected before execution.
    std::ofstream(pa.path.wstring()+L".mod",std::ios::app)<<"setting=effects|bool|Duplicate|true\n";
    Package bad; assert(!readPackage(pa.id,bad,error));
    puts("PASS: native interaction flags, percent slider bounds, legacy defaults, bool/choice/percent validation, per-world/per-mod isolation, real DLL configuration across fresh processes, disabled-mod persistence, immediate save, conflicts and restart/revert.");
}
