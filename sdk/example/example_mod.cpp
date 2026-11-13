#include "enshrouded_mod.h"
#include <cstring>

// Minimal no-op example. Keep configuration separate from game initialization.
// A real mod parses these validated key=value lines into its own typed settings.
static char configuredWorld[33]{};
static char configuration[4096]{};

ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModConfigure(
    const char* world_id, const char* settings_utf8) {
    if(!world_id || !settings_utf8 || strnlen_s(world_id,33)!=32 ||
       strnlen_s(settings_utf8,sizeof configuration)>=sizeof configuration)
        return ENSHROUDED_MOD_FAILURE;
    if(strcpy_s(configuredWorld,world_id) || strcpy_s(configuration,settings_utf8))
        return ENSHROUDED_MOD_FAILURE;
    return ENSHROUDED_MOD_SUCCESS;
}

ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModInitialize(
    const EnshroudedModContext* context) {
    if(!context || context->size<sizeof(*context) ||
       context->abi_version!=ENSHROUDED_MOD_ABI_VERSION ||
       !context->world_id || !context->write_log ||
       strcmp(context->world_id,configuredWorld)!=0)
        return ENSHROUDED_MOD_FAILURE;
    context->write_log("SDK example initialized without gameplay changes");
    context->write_log(configuration);
    return ENSHROUDED_MOD_SUCCESS;
}
