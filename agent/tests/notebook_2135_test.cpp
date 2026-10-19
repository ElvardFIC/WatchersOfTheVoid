#include "support_2135.h"
#include "../../src/votv-coop/src/coop/props/notebook_refresh.cpp"
namespace N=coop::props::notebook_refresh;
namespace test {inline std::wstring key=L"paper1",text=L"",sent;inline bool covered=true,quiet=true,publishOk=true;inline int calls=0;}
namespace ue_wrap::prop {std::wstring GetInteractableKeyString(void*){return test::key;}}
namespace coop::join_membership_sweep {bool HasLoadTailQuiesced(){return test::quiet;}}
namespace coop::prop_save_data {
bool Covers(void*){return test::covered;}
bool Publish(net::Session*,void*,const std::wstring&){++test::calls;test::sent=test::text;test::Callback();return test::publishOk;}
}
coop::net::Session session;
void Reset(){N::OnDisconnect();session={};test::actor={};test::ui={2};test::ui.clipboard=&test::actor;test::key=L"paper1";test::calls=0;test::covered=test::quiet=test::publishOk=test::fields=true;test::callback={};test::text=L"old";N::Install(&session);}
int main(){using namespace test;ue_wrap::script_gate::Call c{};c.object=&ui;
 Reset();N::BeforeQuit(c);text=L"Новый текст\nUnicode 中文";ui.clipboard=nullptr;N::Tick();CHECK(calls==1&&sent==text);N::Tick();CHECK(calls==1);
 Reset();N::AfterUI(c);N::AfterUI(c);N::Tick();CHECK(calls==1);
 Reset();c.fromOurCode=true;N::BeforeQuit(c);N::AfterUI(c);N::Tick();CHECK(calls==0);c.fromOurCode=false;
 Reset();actor.flags=1;N::AfterUI(c);actor.flags=0;N::Tick();CHECK(calls==0);
 Reset();actor.flags=2;N::AfterUI(c);N::Tick();CHECK(calls==0);
 Reset();session.mode=coop::net::Role::Client;quiet=false;N::AfterUI(c);quiet=true;N::Tick();CHECK(calls==0);
 Reset();N::AfterUI(c);session.ready=false;N::Tick();CHECK(calls==0);session.ready=true;N::Tick();CHECK(calls==1);
 Reset();N::AfterUI(c);publishOk=false;N::Tick();CHECK(calls==1);N::Tick();CHECK(calls==1);now+=1000;publishOk=true;N::Tick();CHECK(calls==2);
 Reset();N::AfterUI(c);callback=[&]{text=L"newer";N::AfterUI(c);};N::Tick();CHECK(calls==1);N::Tick();CHECK(calls==2&&sent==L"newer");
 Reset();N::AfterUI(c);callback=[] {N::OnDisconnect();};N::Tick();CHECK(calls==1);N::Tick();CHECK(calls==1);
 Reset();N::AfterUI(c);callback=[] {N::Tick();};N::Tick();CHECK(calls==1);
 Reset();N::AfterUI(c);++actor.serial;N::Tick();CHECK(calls==0);
 Reset();N::AfterUI(c);++world;N::Tick();CHECK(calls==0);
 Reset();N::AfterUI(c);key=L"other";N::Tick();CHECK(calls==0);
 Reset();N::AfterAppend({.object=&actor});N::Tick();CHECK(calls==1);
 puts("PASS actual notebook publisher: native close timing, Unicode, coalescing, readiness, retry, echo and lifetime guards");
}
