#pragma once
#include <atomic>
#include "../third_party/kiln/kiln_native.h"

namespace loader_bootstrap {
enum class State { idle, starting, active, failed };
inline bool validContext(const KilnNativeContext* ctx) noexcept {
    return ctx && ctx->size>=sizeof(KilnNativeContext) &&
        ctx->abi_version==KILN_NATIVE_ABI_VERSION &&
        ctx->phase==KILN_NATIVE_PHASE_BOOT && ctx->side==KILN_SIDE_CLIENT &&
        ctx->hooks_enabled && ctx->owner && ctx->module && ctx->api &&
        ctx->api->size>=offsetof(KilnApi,log)+sizeof(ctx->api->log) &&
        ctx->api->abi_version==KILN_ABI_VERSION && ctx->api->log;
}
// A failed initialization is terminal: do not retry half-initialized global state.
// The separate process-wide lease below coordinates differently named DLL copies.
class Start {
    std::atomic<State> state_{State::idle};
public:
    template<class Claim,class Initialize> int run(Claim claim,Initialize initialize) noexcept {
        auto expected=State::idle;
        if(!state_.compare_exchange_strong(expected,State::starting))return KILN_NATIVE_INIT_FAILED;
        bool ok=false;
        try {ok=claim() && initialize();} catch(...) {}
        state_.store(ok?State::active:State::failed);
        return ok?KILN_NATIVE_INIT_OK:KILN_NATIVE_INIT_FAILED;
    }
};
}
