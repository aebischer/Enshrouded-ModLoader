#include "../src/mod_api.h"
extern "C" __declspec(dllimport) int AuditDependencyAttached();
extern "C" __declspec(dllexport) int EnshroudedModInitialize(const EnshroudedModContext*) {return AuditDependencyAttached()==1;}
