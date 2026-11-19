// Isolated probes for audit 0.6.8. No production files are changed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static SHORT WINAPI auditKeyState(int) {return 0;}
#define GetAsyncKeyState auditKeyState
#include "../src/loader.cpp"
#include <cassert>
#include <new>
static bool failAllocations=false;
void* operator new(size_t n) {if(failAllocations) throw std::bad_alloc(); if(auto p=malloc(n?n:1)) return p; throw std::bad_alloc();}
void operator delete(void* p) noexcept {free(p);}
void operator delete(void* p,size_t) noexcept {free(p);}
static void manifest(const std::filesystem::path& path,const std::string& extra="") {
 std::ofstream(path.wstring()+L".mod",std::ios::binary)<<"ENSHROUDED_PACKAGE_V1\nkind=mod\nversion=1.0\nabi=1\n"<<extra;
}
int main() {
 const auto root=std::filesystem::current_path()/("audit-068-"+std::to_string(GetCurrentProcessId()));
 folder=(root/L"ModLoader").wstring(); const auto mods=root/L"Mods"/L"EnshroudedModLoader";
 std::filesystem::create_directories(mods/L"a.dll.ini");
 const std::string world(32,'a'); std::string error;
 for(const auto* id:{L"a.dll",L"a.dll.ini/b.dll"}) {
  auto path=mods/id;
  std::filesystem::copy_file(std::filesystem::current_path()/L"mod_test_plugin.dll",path);
  manifest(path,"setting=flag|bool|Flag|true\n");
 }
 Package a,b; assert(readPackage("a.dll",a,error) && readPackage("a.dll.ini/b.dll",b,error));
 auto av=defaultSettings(a),bv=defaultSettings(b);
 assert(saveModSettings(world,a,av,av,error));
 assert(readModSettings(world,b,bv,error));
 assert(saveModSettings(world,b,defaultSettings(b),defaultSettings(b),error));
 assert(settingsPath(world,a.id)!=settingsPath(world,b.id));
 const std::string migratedWorld(32,'b');
 auto legacy=legacySettingsPath(migratedWorld,a.id);
 std::filesystem::create_directories(legacy.parent_path());
 std::ofstream(legacy)<<"ENSHROUDED_SETTINGS_V1\nflag=false\n";
 SettingValues migrated;
 assert(readModSettings(migratedWorld,a,migrated,error) && migrated.at("flag")=="false");
 assert(!std::filesystem::exists(legacy));
 std::filesystem::remove(settingsPath(migratedWorld,a.id));
 assert(readModSettings(migratedWorld,a,migrated,error) && migrated.at("flag")=="true");
 const std::string lockedWorld(32,'c');
 auto lockedLegacy=legacySettingsPath(lockedWorld,a.id);
 std::filesystem::create_directories(lockedLegacy.parent_path());
 std::ofstream(lockedLegacy)<<"ENSHROUDED_SETTINGS_V1\nflag=false\n";
 {FileHandle held(CreateFileW((lockedLegacy.wstring()+L".lock").c_str(),GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,0,nullptr));
  assert(held.value!=INVALID_HANDLE_VALUE);
  assert(!readModSettings(lockedWorld,a,migrated,error) && std::filesystem::exists(lockedLegacy));}
 assert(readModSettings(lockedWorld,a,migrated,error) && migrated.at("flag")=="false");
 const std::string badWorld(32,'d'); auto badLegacy=legacySettingsPath(badWorld,a.id);
 std::filesystem::create_directories(badLegacy.parent_path()); std::ofstream(badLegacy,std::ios::binary)<<"INVALID\n";
 assert(!readModSettings(badWorld,a,migrated,error));
 std::string badBody; assert(boundedRead(settingsPath(badWorld,a.id),16384,badBody) && badBody=="INVALID\n");
 puts("PASS: colliding paths, migration, reset, locked migration retry and corrupt-file preservation.");
 // Literal logging must not allocate before entering the noexcept logger.
 unsigned char fakeWorld[0xf8]{}; fakeWorld[0xe0]=1; fakeWorld[0x14]=1;
 modsOpen=true; modBackRequested=true;
 bool escaped=false; failAllocations=true;
 try {browserScreen(nullptr);} catch(const std::bad_alloc&) {escaped=true;}
 failAllocations=false; assert(!escaped && !modsOpen && backDrainFrames==1);
 puts("PASS: Back logging survives allocation failure.");
 // A refused launch can open MODS without ever going through the MODS button.
 sessionTainted=false; backDrainFrames=0; selectedKey=worldKey(fakeWorld);
 std::filesystem::create_directories(profilePath(selectedKey).parent_path());
 std::ofstream(profilePath(selectedKey))<<"INVALID\n";
 unsigned char state[0x80]{},menu[0x248]{};
 putAt<uint16_t>(state,0x41,0x101); putAt<unsigned char>(state,0x39,1);
 putAt<uint64_t>(menu,0x98,1); putAt<uint64_t>(menu,0xc0,1);
 lastNativeReturn.capture(state,menu); lastNativeWorld=selectedKey;
 worldPageReturn.pending=false; modsOpen=false;
 startLocal(fakeWorld);
 assert(modsOpen && !launchError.empty() && worldPageReturn.pending && worldPageReturn.slot==0x101 && worldPageReturn.selection==1);
 prepareRefusedReturn(std::string(32,'f')); assert(!worldPageReturn.pending);
 lastNativeReturn.editVisible=0; lastNativeReturn.page=2;
 prepareRefusedReturn(selectedKey);
 assert(worldPageReturn.pending && !worldPageReturn.editVisible && worldPageReturn.page==2);
 puts("PASS: refused launch preserves its native origin and rejects stale world snapshots.");
 // Declarative inspection must not leave writable manifests while the executable plan is locked.
 manifest(mods/L"a.dll");
 LoadPlan plan; assert(makeLoadPlan({"a.dll"},plan,error));
 FileHandle writer(CreateFileW((mods/L"a.dll.mod").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr));
 assert(writer.value!=INVALID_HANDLE_VALUE);
 puts("OBSERVATION: preflight locks DLL bytes but not their manifests (external edits require game closure per docs).");
 puts("Audit fixtures completed; installed DLL unchanged.");
}



