#include "../src/mod_api.h"
#include <cstring>
static int count=0;
static char world[33]{};
extern "C" __declspec(dllexport) int EnshroudedModInitialize(const EnshroudedModContext* context) {
    ++count;
    if(context->abi_version!=1 || context->size!=sizeof(*context)) return 0;
    strncpy_s(world,context->world_id,32);
#ifdef MOD_TEST_REJECT
    return 0;
#else
    context->write_log("Test plugin initialized"); return 1;
#endif
}
extern "C" __declspec(dllexport) int ModTestCount() {return count;}
extern "C" __declspec(dllexport) const char* ModTestWorld() {return world;}
