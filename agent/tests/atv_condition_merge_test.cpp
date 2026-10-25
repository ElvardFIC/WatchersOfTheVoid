#include "../../src/votv-coop/src/coop/interactables/atv_condition_sync.cpp"
#include "../../src/votv-coop/src/coop/config/config_registry.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
ue_wrap::atv_condition::Snapshot state;
int actor=0,ejected=0,tyreUpdates=0;
bool ejectWorks=true;
void Reset(){state={};state.mask=15;state.hasSpare=true;state.fuel=20;state.health=100;ejected=tyreUpdates=0;ejectWorks=true;}
}
namespace ue_wrap::atv_condition {
bool Resolve(){return true;}bool Resolved(){return true;}
bool Read(void*,Snapshot& out){out=test::state;return true;}
bool WriteAccumulators(void*,const Snapshot& in){auto mask=test::state.mask;auto spare=test::state.hasSpare;test::state=in;test::state.mask=mask;test::state.hasSpare=spare;return true;}
bool WritePresence(void*,uint8_t mask,bool spare){test::state.mask=mask;test::state.hasSpare=spare;return true;}
bool ReadSkipTireUpdate(void*,bool& out){out=false;return true;}
bool CallVerb(void*,Verb verb){if(verb==Verb::UpdTires)++test::tyreUpdates;return true;}
}
namespace ue_wrap::atv {bool EjectWheel(void*,int wheel){++test::ejected;if(test::ejectWorks)test::state.mask&=~(1u<<wheel);return test::ejectWorks;}}
namespace ue_wrap::log {void Write(Level,const char*,...){} std::string Addr(std::string_view value){return std::string(value);}}
namespace coop::config {bool ResolveFlag(const config_registry::FlagRow&){return false;}}
int main(){
    namespace C=coop::atv_condition_sync;
    using E=coop::atv_sync::AtvEntry;
    using P=coop::net::AtvStatePayload;
    test::Reset();E host{};host.actor=&test::actor;host.syncerSlot=1;
    P input{};C::FillPayload(&test::actor,input);input.tiresMask=14;
    C::ApplyPayload(host,input,false);
    CHECK(test::ejected==1&&test::state.mask==14);
    P canonical{};C::FillPayload(&test::actor,canonical);canonical.stateBits=coop::net::kAtvFromSyncer;
    test::Reset();E client{};client.actor=&test::actor;client.syncerSlot=1;
    C::ApplyPayload(client,canonical,true);
    CHECK(test::state.mask==14&&test::ejected==0); // only the host births the ejected wheel
    test::Reset();test::ejectWorks=false;host={};host.actor=&test::actor;host.syncerSlot=1;
    C::ApplyPayload(host,input,false);CHECK(test::state.mask==15&&test::ejected==1);
    C::ApplyPayload(host,input,false);CHECK(test::ejected==1); // no repeated wheel birth attempt
    canonical={};C::FillPayload(&test::actor,canonical);CHECK(canonical.tiresMask==15);
    test::Reset();test::state.mask=14;host={};host.actor=&test::actor;host.syncerSlot=1;
    input={};C::FillPayload(&test::actor,input);input.tiresMask=15;input.syncerSlot=0;
    C::ApplyPayload(host,input,false);CHECK(test::state.mask==15); // host table, never the client's claimed syncer
    const auto old=test::state;input.fuel=std::numeric_limits<float>::quiet_NaN();
    C::ApplyPayload(host,input,false);CHECK(test::state.fuel==old.fuel);
    input.fuel=500;input.health=-100;C::ApplyPayload(host,input,false);
    CHECK(test::state.fuel==100&&test::state.health==-1);
    std::puts("PASS actual merged ATV condition controller: host wheel birth, stamped client presence, failed-eject coalescing, trusted syncer table and finite/domain guards");
}
