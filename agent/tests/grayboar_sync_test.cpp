#include "../../src/votv-coop/src/coop/creatures/grayboar_sync.cpp"
#include <cassert>
#include <cwchar>
#include <cstdio>
namespace test {int body=1,claims=0,watches=0;uint32_t eid=0;bool watchOk=true;
ue_wrap::script_gate::PreFn pre=nullptr;}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::script_gate {
bool WatchClassName(const wchar_t* cls,const wchar_t* fn,int,PreFn pre,PostFn post){
 assert(!wcscmp(cls,L"grayboar_C")&&!wcscmp(fn,L"ReceiveTick")&&!post);
 ++test::watches;test::pre=pre;return test::watchOk;
}
}
namespace coop::prop_element_tracker {
coop::element::ElementId GetPropElementIdForActor(void* p){assert(p==&test::body);return test::eid;}
}
namespace coop::prop_drive_host {
void Claim(void* p,const char*){assert(p==&test::body);++test::claims;}
}
int main(){
 using namespace test;namespace B=coop::grayboar_sync;
 coop::net::Session session;ue_wrap::script_gate::Call c{};c.object=&body;
 watchOk=false;B::Install(&session);assert(watches==1);
 watchOk=true;B::Install(&session);assert(watches==2);B::Install(&session);assert(watches==2);
 auto tick=[&]{assert(pre(c)==ue_wrap::script_gate::Verdict::Run);};
 tick();assert(claims==0);eid=coop::element::kInvalidId;tick();assert(claims==0);
 eid=coop::element::kHostRangeSize;tick();assert(claims==0);
 eid=42;tick();assert(claims==1); // Late registration recovers without a scan.
 tick();assert(claims==2); // Claim's existing idempotency owns the same generation.
 session.mode=coop::net::Role::Client;tick();assert(claims==2);
 session.mode=coop::net::Role::Host;session.active=false;tick();assert(claims==2);
 session.active=true;B::OnDisconnect();tick();assert(claims==2);
 B::Install(&session);tick();assert(claims==3&&watches==2);
 c.object=nullptr;tick();assert(claims==3);
 puts("PASS actual grayboar body feeder: host identity, delayed registration, client/offline refusal, reconnect and hook retry");
}
