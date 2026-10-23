#include "../../src/votv-coop/src/coop/world/balance_sync.cpp"
#include <cassert>
#include <cstdio>
#include <functional>
#include <limits>
#include <vector>
namespace test {
uint32_t world=1,serial=1;
int32_t points=100;
int writes=0,repaints=0,credits=0;
bool readable=true,writable=true,hud=true;
std::function<void()> read,write,paint;
std::vector<ue_wrap::game_thread::Task> tasks;
void Fire(std::function<void()>& source){auto f=std::move(source);source={};if(f)f();}
void Reset(coop::net::Session& s,coop::net::Role role=coop::net::Role::Host){
    coop::balance_sync::OnDisconnect();s={};s.mode=role;
    points=100;writes=repaints=credits=0;readable=writable=hud=true;
    read=write=paint={};tasks.clear();++serial;
    coop::balance_sync::SetSession(&s);
}
void Drain(){auto copy=std::move(tasks);tasks.clear();for(auto& task:copy)task();}
}
namespace ue_wrap::world_identity { uint32_t Generation(){return test::world;} }
namespace coop::net::session_serial { uint32_t Current(){return test::serial;} }
namespace ue_wrap::game_thread { void Post(Task task){test::tasks.push_back(std::move(task));} }
namespace ue_wrap::log { void Write(Level,const char*,...){} }
namespace ue_wrap::economy {
bool ReadPoints(int32_t* p){*p=test::points;test::Fire(test::read);return test::readable;}
bool WritePoints(int32_t p){if(!test::writable)return false;++test::writes;test::points=p;test::Fire(test::write);return true;}
bool RefreshPointsHud(){++test::repaints;test::Fire(test::paint);return test::hud;}
bool AddPoints(int32_t p){++test::credits;test::points+=p;return true;}
}
int main(){
    namespace B=coop::balance_sync;using namespace test;using coop::net::Role;
    coop::net::Session s;Reset(s);s.allowed[1]=false;
    B::Tick();assert(s.sent.size()==1&&s.sent[0].slot==2);
    B::Tick();assert(s.sent.size()==1);s.allowed[1]=true;B::Tick();
    assert(s.sent.size()==2&&s.sent.back().slot==1);B::Tick();assert(s.sent.size()==2);
    // A static total still reaches a late peer and a replacement in the same slot.
    s.ready[3]=true;s.generation[3]=3;B::Tick();assert(s.sent.size()==3);
    ++s.generation[1];B::Tick();assert(s.sent.size()==4&&s.sent.back().slot==1);
    ++world;B::Tick();assert(s.sent.size()==7);
    // Refused order correction retains debt through unresolved save and send refusal.
    readable=false;B::SendCurrentToSlot(2);B::Tick();assert(s.sent.size()==7);
    readable=true;s.allowed[2]=false;B::Tick();assert(s.sent.size()==7);
    s.allowed[2]=true;B::Tick();assert(s.sent.size()==8&&s.sent.back().slot==2);
    B::SendCurrentToSlot(-1);B::SendCurrentToSlot(4);B::SendCurrentToSlot(0);assert(s.sent.size()==8);
    points=std::numeric_limits<int32_t>::min();B::Tick();assert(s.sent.size()==11&&s.sent.back().value==points);
    points=0;B::Tick();assert(s.sent.size()==14&&s.sent.back().value==0);
    Reset(s);s.callback=[]{B::OnDisconnect();};B::Tick();assert(s.sent.size()==1);B::Tick();assert(s.sent.size()==1);
    Reset(s);s.callback=[]{++world;};B::Tick();assert(s.sent.size()==1);B::Tick();assert(s.sent.size()==3);
    Reset(s);s.callback=[&]{++s.generation[1];};B::Tick();assert(s.sent.size()==2);B::Tick();assert(s.sent.size()==3);
    Reset(s);s.callback=[]{B::SendCurrentToSlot(1);B::Tick();};B::Tick();B::Tick();assert(s.sent.size()==3);
    Reset(s);read=[]{B::OnDisconnect();};B::Tick();assert(s.sent.empty());
    Reset(s);++serial;B::Tick();assert(s.sent.empty());

    Reset(s,Role::Client);writable=false;B::ApplyFromHost(250);B::Tick();assert(writes==0);
    writable=true;hud=false;B::Tick();assert(points==250&&repaints==1);
    hud=true;B::Tick();assert(repaints==2);B::Tick();assert(repaints==2);
    B::ApplyFromHost(300);paint=[]{B::ApplyFromHost(400);B::Tick();};B::Tick();
    assert(points==300);B::Tick();assert(points==400);B::Tick();assert(repaints==4);
    B::ApplyFromHost(-1);B::Tick();assert(points==-1);B::ApplyFromHost(0);B::Tick();assert(points==0);
    B::ApplyFromHost(500);write=[]{B::OnDisconnect();};auto before=repaints;B::Tick();assert(repaints==before);
    B::ApplyFromHost(600);B::Tick();assert(points==500);
    Reset(s,Role::Client);B::ApplyFromHost(700);paint=[&]{B::SetSession(&s);B::ApplyFromHost(800);};
    B::Tick();B::Tick();assert(points==800);
    Reset(s,Role::Client);B::ApplyFromHost(900);++world;B::Tick();assert(points==100&&writes==0);
    ++world;B::ApplyFromHost(950);B::Tick();assert(points==950&&writes==1);
    Reset(s,Role::Client);B::CreditLocal(10);Drain();assert(credits==0);
    Reset(s);B::CreditLocal(10);B::OnDisconnect();Drain();assert(credits==0);
    Reset(s);B::CreditLocal(10);++serial;Drain();assert(credits==0);
    Reset(s);B::CreditLocal(10);++world;Drain();assert(credits==0&&points==100);
    Reset(s);B::CreditLocal(10);Drain();assert(credits==1&&points==110);
    B::OnDisconnect();B::CreditLocal(5);Drain();assert(points==115);
    puts("PASS balance per-peer delivery/retry, static joins/refusals, signed totals, latest HUD update, reentry, world/session/occupant replacement and deferred credit cancellation across world/session changes");
}
