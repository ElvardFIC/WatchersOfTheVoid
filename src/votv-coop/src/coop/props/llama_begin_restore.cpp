#include "coop/props/llama_begin_restore.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/hand_item.h"
#include "coop/props/active_drive.h"
#include "ue_wrap/actors/llama_begin_restore.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>
namespace coop::llama_begin_restore {
namespace {
namespace M=ue_wrap::llama_begin_restore;
struct Scope {net::Session* session=nullptr;net::Role role{};uint32_t serial=0,generation=0;void* world=nullptr;bool active=false;bool operator==(const Scope&)const=default;};
struct Row {M::Snapshot source;bool confirmed=false,unknown=false,cleanup=false,attempted=false,busy=false,canceled=false;uint64_t last=0;bool polled=false;};
struct Frame {std::shared_ptr<Row> row;uint64_t invocation=0;bool committed=false;};
std::unordered_map<void*,std::shared_ptr<Row>> g_rows;std::unordered_map<uint64_t,Frame> g_frames;
net::Session* g_session=nullptr;Scope g_scope;uint64_t g_epoch=0,g_ticket=0;bool g_busy=false;
constexpr size_t kCapacity=256,kFrames=32;
Scope Capture(net::Session* s){Scope scope;scope.session=s;scope.serial=net::session_serial::Current();scope.world=ue_wrap::world_identity::CurrentWorld();scope.generation=ue_wrap::world_identity::Generation();
    if(s){scope.role=s->role();scope.active=s->running()&&s->connected();}return scope;}
bool LiveClient(){return g_session&&g_session->running()&&g_session->connected()&&g_session->role()==net::Role::Client;}
bool Client(){return LiveClient()&&g_scope==Capture(g_session)&&g_scope.active&&g_scope.role==net::Role::Client;}
void ScopeTo(net::Session* s){const auto scope=Capture(s);if(!(scope==g_scope)||g_session!=s){++g_epoch;for(auto& [actor,row]:g_rows)row->cleanup=true;g_session=s;g_scope=scope;}}
void Sweep(){
    std::erase_if(g_frames,[](const auto& pair){const auto& frame=pair.second;if(M::Retired(frame.row->source))return true;if(ue_wrap::script_gate::IsInvocationActive(frame.invocation))return false;
        if(frame.committed)frame.row->unknown=true;return true;});
    std::erase_if(g_rows,[](const auto& pair){return M::Retired(pair.second->source);});
}
void Drain(){
    Sweep();if(LiveClient())return;std::vector<std::shared_ptr<Row>> rows;for(const auto& [actor,row]:g_rows)rows.push_back(row);
    for(const auto& row:rows){if(!row->cleanup||row->busy||std::any_of(g_frames.begin(),g_frames.end(),[&](const auto& pair){return pair.second.row==row;}))continue;
        const auto now=active_drive::NowMs();if(row->polled&&now>=row->last&&now-row->last<100)continue;row->polled=true;row->last=now;row->busy=true;
        const auto result=M::Restore(row->source,row->confirmed,row->attempted,[&]{return row->cleanup&&!row->canceled&&!LiveClient()&&M::Current(row->source);});row->busy=false;
        if(result!=M::Result::Retry){const auto it=g_rows.find(row->source.actor.Raw());if(it!=g_rows.end()&&it->second==row)g_rows.erase(it);}
    }
}
struct Busy {Busy(){g_busy=true;}~Busy(){g_busy=false;}};
void DropUnused(const std::shared_ptr<Row>& row){
    if(row->confirmed||row->unknown||std::any_of(g_frames.begin(),g_frames.end(),[&](const auto& pair){return pair.second.row==row;}))return;
    std::erase_if(g_rows,[&](const auto& pair){return pair.second==row;});
}
}
bool Prepare(void* actor,bool original,uint64_t invocation,const std::function<bool()>& current,uint64_t& ticket){
    ticket=0;if(!original)return true;if(g_busy||!invocation||!current||!Client()||!actor||hand_item::IsHandAxisActor(actor))return false;
    Busy guard;Sweep();if(g_frames.size()>=kFrames)return false;const auto epoch=g_epoch;const auto scope=g_scope;
    const auto valid=[&]{return epoch==g_epoch&&scope==g_scope&&Client()&&current()&&epoch==g_epoch&&scope==g_scope&&Client()&&!hand_item::IsHandAxisActor(actor);};
    if(!valid())return false;auto it=g_rows.find(actor);std::shared_ptr<Row> row;
    if(it!=g_rows.end())row=it->second;
    else {if(g_rows.size()>=kCapacity)return false;row=std::make_shared<Row>();g_rows[actor]=row;}
    uint64_t id=++g_ticket;if(!id)id=++g_ticket;g_frames[id]={row,invocation,false};
    if(!row->source.actor.Raw()){M::Snapshot source;if(!M::Capture(actor,source,valid)||!valid()){
            g_frames.erase(id);auto found=g_rows.find(actor);if(found!=g_rows.end()&&found->second==row&&!row->source.actor.Raw())g_rows.erase(found);return false;}row->source=source;
    }else {M::Snapshot currentSource;if(!M::Capture(actor,currentSource,valid)||!valid()||!row->source.actor.Is(currentSource.actor.Get())||!row->source.cls.Is(currentSource.cls.Get())||
        !row->source.sphere.Is(currentSource.sphere.Get())||!row->source.sphereClass.Is(currentSource.sphereClass.Get())||!row->source.timerType.Is(currentSource.timerType.Get())){g_frames.erase(id);return false;}}
    if(!valid()||!g_frames.contains(id)){g_frames.erase(id);DropUnused(row);return false;}ticket=id;return true;
}
void Commit(uint64_t ticket){auto it=g_frames.find(ticket);if(it!=g_frames.end())it->second.committed=true;}
void Cancel(uint64_t ticket){const auto it=g_frames.find(ticket);if(it==g_frames.end())return;const auto row=it->second.row;g_frames.erase(it);
    DropUnused(row);
}
void Finished(uint64_t invocation){
    std::erase_if(g_frames,[&](const auto& pair){if(pair.second.invocation!=invocation)return false;if(pair.second.committed)pair.second.row->confirmed=true;return true;});
    if(!g_busy){Busy guard;Drain();}
}
void Consumed(void* actor){
    const auto it=g_rows.find(actor);if(it==g_rows.end())return;const auto row=it->second;const auto& source=row->source;
    if(!source.world||source.world!=ue_wrap::world_identity::CurrentWorld()||source.generation!=ue_wrap::world_identity::Generation()||
        !source.actor.Is(actor)||!source.cls.Get()||ue_wrap::reflection::ClassOf(actor)!=source.cls.Get()||ue_wrap::world_identity::WorldOf(actor)!=source.world)return;
    row->canceled=true;std::erase_if(g_frames,[&](const auto& pair){return pair.second.row==row;});g_rows.erase(it);
}
void Install(net::Session* session){ScopeTo(session);if(!g_busy){Busy guard;Drain();}}
void Tick(){if(g_busy)return;Busy guard;ScopeTo(g_session);Drain();}
void OnDisconnect(){++g_epoch;for(auto& [actor,row]:g_rows)row->cleanup=true;g_session=nullptr;g_scope={};if(!g_busy){Busy guard;Drain();}}
}
