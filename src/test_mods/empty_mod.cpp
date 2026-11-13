#include "../mod_api.h"
#include <cstdio>
#include <cstring>

#ifdef TEST_MOD_B
static constexpr char Name[]="Test_B";
#else
static constexpr char Name[]="Test_A";
#endif

static char ConfigWorld[33]{},ConfigValues[1024]{};
ENSHROUDED_MOD_EXPORT
int ENSHROUDED_MOD_CALL EnshroudedModConfigure(const char* world,const char* values) {
    if(!world || strnlen_s(world,33)!=32 || !values || strnlen_s(values,1024)>=1024) return 0;
    strcpy_s(ConfigWorld,world); strcpy_s(ConfigValues,values); return 1;
}
// No hooks, game memory access, threads, or save changes.
ENSHROUDED_MOD_EXPORT
int ENSHROUDED_MOD_CALL EnshroudedModInitialize(const EnshroudedModContext* context) {
    if(!context || context->size<sizeof(EnshroudedModContext) ||
       context->abi_version!=ENSHROUDED_MOD_ABI_VERSION || !context->world_id || !context->write_log) return 0;
    if(strnlen_s(context->world_id,33)!=32) return 0;
    char message[160]{};
    sprintf_s(message,"%s : mod de test initialise, monde=%s ; aucun effet sur le jeu.",Name,context->world_id);
    context->write_log(message);
    if(strcmp(ConfigWorld,context->world_id)!=0) return 0;
    char settingsLog[1200]{};
    sprintf_s(settingsLog,"%s configuration monde=%s :\n%s",Name,ConfigWorld,ConfigValues);
    context->write_log(settingsLog);
    return 1;
}
