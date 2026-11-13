#include <windows.h>
static int attached=0;
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID) {if(reason==DLL_PROCESS_ATTACH) ++attached; return TRUE;}
extern "C" __declspec(dllexport) int AuditDependencyAttached() {return attached;}
