#pragma once
#include <stdint.h>

// Enshrouded ModLoader SDK, ABI 1. Not an official game SDK.
// This layout and these calling conventions are frozen for ABI 1.
#define ENSHROUDED_MOD_ABI_VERSION 1u
#define ENSHROUDED_MOD_SUCCESS 1
#define ENSHROUDED_MOD_FAILURE 0
#if defined(_WIN32)
#define ENSHROUDED_MOD_CALL __cdecl
#ifdef __cplusplus
#define ENSHROUDED_MOD_EXPORT extern "C" __declspec(dllexport)
#else
#define ENSHROUDED_MOD_EXPORT __declspec(dllexport)
#endif
#else
#error This SDK targets Windows x64.
#endif

typedef void (ENSHROUDED_MOD_CALL *EnshroudedModLogFn)(const char* message);
typedef struct EnshroudedModContext {
    uint32_t size;
    uint32_t abi_version;
    const char* world_id;
    EnshroudedModLogFn write_log;
} EnshroudedModContext;

typedef int (ENSHROUDED_MOD_CALL *EnshroudedModInitializeFn)(const EnshroudedModContext* context);
typedef int (ENSHROUDED_MOD_CALL *EnshroudedModConfigureFn)(const char* world_id, const char* settings_utf8);

// Export Initialize for every kind=mod package. Export Configure when settings
// are declared. Use the exact names; no C++ name mangling. See STANDARD_V1.txt.
// Configure runs before Initialize; both run outside DllMain. Return 1 on
// success, 0 on failure. Never propagate an exception across this C boundary.
// Configure's pointers are borrowed for the call only. Context and its world_id
// stay valid until process exit. Do not free or mutate loader-owned memory.
// No hot unload, world lifecycle callbacks, network API, or game SDK in ABI 1.
#ifdef __cplusplus
static_assert(sizeof(void*)==8, "Windows x64 is required");
static_assert(sizeof(EnshroudedModContext)==24, "ABI 1 layout mismatch");
#endif
