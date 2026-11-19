#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
int wmain(int argc,wchar_t** argv) {
    assert(argc==3);assert(LoadLibraryW(argv[1]));assert(LoadLibraryW(argv[2]));
    Sleep(500);wchar_t name[96]{};
    swprintf_s(name,L"Local\\EnshroudedModLoader.Bootstrap.%lu",GetCurrentProcessId());
    assert(!OpenMutexW(SYNCHRONIZE,FALSE,name));
    puts("PASS: standalone proxy chained behind Kiln stands aside without claiming or initializing.");
}
