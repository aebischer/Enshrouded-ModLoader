#include "../sdk/include/enshrouded_mod.h"
#include <stddef.h>
/* C consumers must have exactly the same ABI as C++ consumers. */
typedef char context_size_is_24[(sizeof(EnshroudedModContext)==24)?1:-1];
typedef char world_offset_is_8[(offsetof(EnshroudedModContext,world_id)==8)?1:-1];
typedef char logger_offset_is_16[(offsetof(EnshroudedModContext,write_log)==16)?1:-1];
ENSHROUDED_MOD_EXPORT int ENSHROUDED_MOD_CALL EnshroudedModInitialize(const EnshroudedModContext* context) {
    return context && context->size>=sizeof(*context) && context->abi_version==ENSHROUDED_MOD_ABI_VERSION
        ? ENSHROUDED_MOD_SUCCESS : ENSHROUDED_MOD_FAILURE;
}
