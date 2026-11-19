#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <filesystem>
#include <cstdio>
#include "../third_party/kiln/kiln_native.h"
static unsigned messages=0;
static void KILN_CALL logger(KilnOwner owner,int32_t level,KilnStr text) {
    assert(owner==7&&level==KILN_LOG_ERROR&&text.len);++messages;
}
int wmain(int argc,wchar_t** argv) {
    assert(argc==4);assert(LoadLibraryW(argv[3]));
    const auto dll=LoadLibraryW(argv[1]);assert(dll);
    const auto init=reinterpret_cast<KilnNativeInitFn>(GetProcAddress(dll,"kiln_native_init"));assert(init);
    assert(GetProcAddress(dll,"DirectInput8Create"));
    wchar_t name[96]{};swprintf_s(name,L"Local\\EnshroudedModLoader.Bootstrap.%lu",GetCurrentProcessId());
    // Loading the DLL alone must never start, take the lease or show a dialog.
    Sleep(100);auto mutex=OpenMutexW(SYNCHRONIZE,FALSE,name);assert(!mutex);
    assert(init(nullptr)==KILN_NATIVE_INIT_UNSUPPORTED);
    KilnApi api{};api.size=sizeof(api);api.abi_version=1;api.log=logger;
    KilnNativeContext ctx{};ctx.size=sizeof(ctx);ctx.abi_version=1;ctx.phase=KILN_NATIVE_PHASE_BOOT;
    ctx.side=KILN_SIDE_CLIENT;ctx.api=&api;ctx.owner=7;ctx.module=dll;
    assert(init(&ctx)==KILN_NATIVE_INIT_UNSUPPORTED); // Safe-idle gate must not claim.
    assert(!OpenMutexW(SYNCHRONIZE,FALSE,name));ctx.hooks_enabled=1;
    // The actual DLL refuses this unsupported test EXE before installing hooks.
    assert(init(&ctx)==KILN_NATIVE_INIT_FAILED&&messages==1);
    mutex=OpenMutexW(SYNCHRONIZE,FALSE,name);assert(mutex);CloseHandle(mutex);
    assert(init(&ctx)==KILN_NATIVE_INIT_FAILED&&messages==2);
    assert(CopyFileW(argv[1],argv[2],FALSE));const auto copy=LoadLibraryW(argv[2]);assert(copy&&copy!=dll);
    auto other=reinterpret_cast<KilnNativeInitFn>(GetProcAddress(copy,"kiln_native_init"));assert(other);
    ctx.module=copy;assert(other(&ctx)==KILN_NATIVE_INIT_FAILED&&messages==3);
    puts("PASS: real native DLL is passive until init, rejects safe idle/unsupported EXE, logs failure, refuses second copy.");
}
