#include "coop/props/llama_timers.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/hand_item.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/llama.h"
#include "ue_wrap/actors/llama_timer.h"
#include <algorithm>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
namespace coop::llama_timers {
namespace {
namespace M=ue_wrap::llama_timer;
struct Scope {net::Session* session=nullptr;net::Role role{};uint32_t serial=0,generation=0;void* world=nullptr;bool running=false,connected=false;bool operator==(const Scope&)const=default;};
struct Row {M::Lease lease;bool dispatched=false,busy=false,cleanup=false;};
using Ptr=std::shared_ptr<Row>;
net::Session* g_session=nullptr;Scope g_scope;uint64_t g_epoch=0,g_poll=0,g_now=0;bool g_busy=false;
std::unordered_map<void*,Ptr> g_rows;std::vector<Ptr> g_debt;
constexpr size_t kCapacity=256;
Scope Capture(net::Session* session){Scope scope;scope.session=session;scope.serial=net::session_serial::Current();scope.world=ue_wrap::world_identity::CurrentWorld();scope.generation=ue_wrap::world_identity::Generation();
    if(session){scope.role=session->role();scope.running=session->running();scope.connected=session->connected();}return scope;}
bool Client(){return g_scope.session&&g_scope.world&&g_scope.running&&g_scope.connected&&g_scope.role==net::Role::Client&&g_scope==Capture(g_session);}
void Retire(){++g_epoch;for(auto& [actor,row]:g_rows){row->cleanup=true;if(row->dispatched&&std::find(g_debt.begin(),g_debt.end(),row)==g_debt.end())g_debt.push_back(row);}g_rows.clear();g_poll=g_now=0;}
void ScopeTo(net::Session* session){const auto scope=Capture(session);if(g_session!=session||!(g_scope==scope)){Retire();g_session=session;g_scope=scope;}}
void Drain(){
    // Own each row across callbacks; a reentrant scope change may append different debt.
    const auto rows=g_debt;
    for(const auto& row:rows){if(row->busy)continue;if(!row->dispatched||M::Retired(row->lease)){std::erase(g_debt,row);continue;}
        row->busy=true;const auto result=M::Resume(row->lease,[&]{return row->cleanup&&M::Current(row->lease);});row->busy=false;
        if(result!=M::Result::Retry)std::erase(g_debt,row);
    }
}
struct Busy {Busy(){g_busy=true;}~Busy(){g_busy=false;}};
bool Pending(void* actor){return std::any_of(g_debt.begin(),g_debt.end(),[&](const auto& row){return row->lease.actor.Is(actor);});}
}
void Install(net::Session* session){ScopeTo(session);if(g_busy)return;Busy guard;Drain();}
void OnDisconnect(){Retire();g_session=nullptr;g_scope={};if(g_busy)return;Busy guard;Drain();}
void Tick(){
    if(g_busy)return;Busy guard;ScopeTo(g_session);Drain();if(!Client())return;
    const auto epoch=g_epoch,now=active_drive::NowMs();if(now<g_now)g_poll=0;g_now=now;if(now<g_poll)return;g_poll=now+100;
    std::erase_if(g_rows,[](const auto& pair){return M::Retired(pair.second->lease);});
    std::array<ue_wrap::CachedObjRef,kCapacity> actors{};const auto count=ue_wrap::llama::Collect(actors.data(),actors.size());
    if(epoch!=g_epoch||!Client())return;
    for(size_t i=0;i<std::min(count,actors.size());++i){void* actor=actors[i].Get();
        if(!actor||hand_item::IsHandAxisActor(actor)||ue_wrap::world_identity::WorldOf(actor)!=g_scope.world||Pending(actor))continue;
        const auto current=[&]{return epoch==g_epoch&&Client()&&actors[i].Get()&&actors[i].Get()==actor&&!hand_item::IsHandAxisActor(actor)&&
            ue_wrap::world_identity::WorldOf(actor)==g_scope.world&&epoch==g_epoch&&Client();};
        auto found=g_rows.find(actor);Ptr row;
        if(found!=g_rows.end())row=found->second;
        else {if(g_rows.size()+g_debt.size()>=kCapacity)continue;M::State state;
            if(!M::Inspect(actor,state,current)){if(epoch!=g_epoch||!Client())return;continue;}
            if(!state.exists||!state.active||state.paused||!current())continue;
            row=std::make_shared<Row>();row->lease=state.lease;g_rows[actor]=row;
        }
        if(!current()||row->busy)continue;row->busy=true;const auto result=M::Pause(row->lease,row->dispatched,current);row->busy=false;
        // A disconnect during native Pause has already moved the sticky dispatched owner to debt.
        if(epoch!=g_epoch||!Client()){
            if(row->dispatched&&row->cleanup&&std::find(g_debt.begin(),g_debt.end(),row)==g_debt.end())g_debt.push_back(row);
            // The native pause unwound: finish same-world cleanup now, even if no later SP tick runs.
            Drain();return;
        }
        if(!row->dispatched||result==M::Result::Retired){auto it=g_rows.find(actor);if(it!=g_rows.end()&&it->second==row)g_rows.erase(it);}
    }
}
}
