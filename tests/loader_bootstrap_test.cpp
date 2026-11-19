#include <cassert>
#include <thread>
#include <vector>
#include <stdexcept>
#include "../src/loader_bootstrap.h"
static void KILN_CALL logger(KilnOwner,int32_t,KilnStr){}
int main() {
    KilnApi api{};api.size=sizeof(api);api.abi_version=1;api.log=logger;
    KilnNativeContext ctx{};ctx.size=sizeof(ctx);ctx.abi_version=1;ctx.phase=KILN_NATIVE_PHASE_BOOT;
    ctx.side=KILN_SIDE_CLIENT;ctx.api=&api;ctx.owner=1;ctx.module=&ctx;ctx.hooks_enabled=1;
    assert(loader_bootstrap::validContext(&ctx));assert(!loader_bootstrap::validContext(nullptr));
    for(unsigned i=0;i<10;++i){auto c=ctx;auto a=api;c.api=&a;
        switch(i){case 0:c.size--;break;case 1:c.abi_version=2;break;case 2:c.phase=KILN_NATIVE_PHASE_EARLY;break;
        case 3:c.side=KILN_SIDE_SERVER;break;case 4:c.hooks_enabled=0;break;case 5:c.api=nullptr;break;
        case 6:a.size=0;break;case 7:a.abi_version=2;break;case 8:a.log=nullptr;break;case 9:c.owner=0;break;}
        assert(!loader_bootstrap::validContext(&c));
    }
    std::atomic<unsigned> initialized{0},claimed{0},ok{0};loader_bootstrap::Start start;
    std::vector<std::thread> threads;
    for(unsigned i=0;i<32;++i)threads.emplace_back([&]{
        if(start.run([&]{++claimed;return true;},[&]{++initialized;return true;})==KILN_NATIVE_INIT_OK)++ok;
    });
    for(auto& t:threads)t.join();assert(initialized==1&&claimed==1&&ok==1);
    loader_bootstrap::Start duplicate;
    assert(duplicate.run([]{return false;},[]{assert(false);return true;})==KILN_NATIVE_INIT_FAILED);
    loader_bootstrap::Start failed;
    assert(failed.run([]{return true;},[]()->bool{throw std::runtime_error("failure");})==KILN_NATIVE_INIT_FAILED);
    assert(failed.run([]{assert(false);return true;},[]{return true;})==KILN_NATIVE_INIT_FAILED);
    puts("PASS: native context gates, concurrent single initialization, duplicate lease refusal and terminal failures.");
}
