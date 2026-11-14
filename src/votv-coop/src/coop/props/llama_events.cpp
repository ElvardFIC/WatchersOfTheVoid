#include "coop/props/llama_events.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/hand_item.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/actors/llama_events.h"
#include <algorithm>
#include <array>
#include <memory>
#include <unordered_map>
namespace coop::llama_events {
namespace {
namespace M=ue_wrap::llama_events;namespace SG=ue_wrap::script_gate;
struct Scope {net::Session* session=nullptr;net::Role role{};uint32_t serial=0,generation=0;void* world=nullptr;bool active=false;bool operator==(const Scope&)const=default;};
struct Row {M::Owner owner;bool guarded=false,busy=false;};
using Ptr=std::shared_ptr<Row>;
net::Session* g_session=nullptr;Scope g_scope;uint64_t g_epoch=0,g_checked=0;bool g_hold=false,g_busy=false,g_polled=false;
std::unordered_map<void*,Ptr> g_rows;
constexpr int kTag=0x4C4C4556;constexpr size_t kCapacity=256;
Scope Capture(net::Session* s){Scope scope;scope.session=s;scope.serial=net::session_serial::Current();scope.world=ue_wrap::world_identity::CurrentWorld();scope.generation=ue_wrap::world_identity::Generation();
    if(s){scope.role=s->role();scope.active=s->running()&&s->connected();}return scope;}
bool Client(){return g_scope.session&&g_scope.active&&g_scope.world&&g_scope.role==net::Role::Client&&g_scope==Capture(g_session);}
void Hold(){if(!g_hold){SG::Acquire("llama legacy event guard");g_hold=true;}}
void Prune(){std::erase_if(g_rows,[](const auto& pair){return M::Retired(pair.second->owner)||(!Client()&&!pair.second->guarded);});
    if(g_rows.empty()&&!Client()&&g_hold){SG::Release("llama legacy event guard");g_hold=false;}}
bool Ready(){SG::WatchClassName(L"lib_C",L"setEvent",kTag,&BeforeSetEvent,nullptr);SG::ResolvePendingNames();return SG::ClassNameWatchLive(L"lib_C",L"setEvent",kTag);}
void ScopeTo(net::Session* s){const auto scope=Capture(s);if(g_session!=s||!(scope==g_scope)){++g_epoch;g_session=s;g_scope=scope;g_polled=false;}Prune();}
}
void Install(net::Session* session){ScopeTo(session);if(Client())Hold();Ready();}
bool Prepare(void* actor,const std::function<bool()>& current){
    if(g_busy||!Client()||!current||!actor||hand_item::IsHandAxisActor(actor))return false;
    struct Guard{Guard(){g_busy=true;}~Guard(){g_busy=false;}}guard;const auto epoch=g_epoch;
    Hold();if(!Ready()||epoch!=g_epoch||!Client()||!current())return false;Prune();
    auto found=g_rows.find(actor);Ptr row;if(found!=g_rows.end())row=found->second;
    else {if(g_rows.size()>=kCapacity)return false;M::Owner owner;if(!M::Identify(actor,owner))return false;row=std::make_shared<Row>();row->owner=owner;g_rows[actor]=row;}
    if(row->busy)return false;row->busy=true;
    const auto valid=[&]{return epoch==g_epoch&&Client()&&current()&&epoch==g_epoch&&Client()&&!M::Retired(row->owner)&&!hand_item::IsHandAxisActor(actor);};
    const auto result=M::Cleanup(row->owner,row->guarded,valid);row->busy=false;
    if(!row->guarded&&result!=M::Result::Retry){auto it=g_rows.find(actor);if(it!=g_rows.end()&&it->second==row)g_rows.erase(it);}
    Prune();return result==M::Result::Done&&valid();
}
void Tick(){if(g_busy)return;ScopeTo(g_session);if(!Client())return;Hold();if(!Ready())return;
    const auto now=active_drive::NowMs();if(g_polled&&now>=g_checked&&now-g_checked<100)return;g_polled=true;g_checked=now;
    const auto epoch=g_epoch;std::array<ue_wrap::CachedObjRef,kCapacity> actors{};const auto count=ue_wrap::llama::Collect(actors.data(),actors.size());
    if(epoch!=g_epoch||!Client())return;for(size_t i=0;i<std::min(count,actors.size());++i){const auto actor=actors[i];if(actor.Get())Prepare(actor.Get(),[actor,epoch]{return epoch==g_epoch&&Client()&&actor.Get();});if(epoch!=g_epoch||!Client())return;}
}
void OnDisconnect(){++g_epoch;g_session=nullptr;g_scope={};g_polled=false;Prune();}
SG::Verdict BeforeSetEvent(const SG::Call& call){
    Prune();M::Event event;if(!M::ReadEvent(call.function,call.locals,event)||event.active||event.ambient)return SG::Verdict::Run;
    const auto found=g_rows.find(event.actor.Get());if(found==g_rows.end()||!found->second->guarded)return SG::Verdict::Run;const auto row=found->second;
    if(!row->owner.actor.Is(event.actor.Get())||!row->owner.cls.Is(event.cls.Get())||M::Retired(row->owner))return SG::Verdict::Run;
    bool absent=false;return M::Absent(row->owner,absent,[row]{return !M::Retired(row->owner);})&&absent?SG::Verdict::Cancel:SG::Verdict::Run;
}
}
