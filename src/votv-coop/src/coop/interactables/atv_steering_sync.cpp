#include "coop/interactables/atv_steering_sync.h"
#include "coop/interactables/atv_sync.h"
#include "coop/net/session.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/devices/atv.h"
#include "ue_wrap/devices/atv_steering.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/script_gate.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace coop::atv_steering_sync {
namespace {
namespace FX=ue_wrap::atv_steering;
UE_FACT_FN(kTick,"atv_steering_sync",L"ATV_C",L"ReceiveTick",Declared);
struct Target {ue_wrap::CachedObjRef actor;int16_t angle=0;uint8_t author=0;uint16_t person=0;uint64_t revision=0;};
std::unordered_map<void*,Target> g_targets;
net::Session* g_session=nullptr;
uint32_t g_world=0,g_serial=0;
uint64_t g_epoch=0,g_revision=0;
bool g_applying=false;
void Clear(){++g_epoch;g_targets.clear();g_world=ue_wrap::world_identity::Generation();g_serial=net::session_serial::Current();}
bool Current(uint64_t epoch){return g_session && g_session->running() && g_session->connected() && epoch==g_epoch &&
    g_world==ue_wrap::world_identity::Generation() && g_serial==net::session_serial::Current();}
bool Local(void* actor){return atv_sync::OwnsTick(actor) ||
    (ue_wrap::atv::IsDriven(actor) && ue_wrap::atv::GetOccupantPlayer(actor)==players::Registry::Get().Local());}
void Apply(void* actor){
    if (g_applying || !Current(g_epoch)) return;
    auto it=g_targets.find(actor);if(it==g_targets.end())return;
    const auto row=it->second;const auto epoch=g_epoch;
    if (!row.actor.Get() || roster_ledger::Get(row.author).playerNo!=row.person || Local(actor)) {
        g_targets.erase(it);return;
    }
    const auto valid=[&]{auto live=g_targets.find(actor);return Current(epoch) && row.actor.Get() &&
        live!=g_targets.end() && live->second.revision==row.revision &&
        roster_ledger::Get(row.author).playerNo==row.person && !Local(actor);};
    g_applying=true;struct Scope{~Scope(){g_applying=false;}}scope;
    FX::Apply(actor,float(row.angle)/100.f,valid);
}
void AfterTick(const ue_wrap::script_gate::Call& call){Apply(call.object);}
}
int16_t Capture(void* actor){float yaw=0;return FX::Read(actor,yaw)?int16_t(std::lround(std::clamp(yaw,-45.f,45.f)*100.f)):net::kAtvSteeringUnavailable;}
bool Valid(int16_t yaw){return yaw==net::kAtvSteeringUnavailable || (yaw>=-4500 && yaw<=4500);}
void Install(net::Session* session){
    if(g_session!=session){g_session=session;Clear();}
    if(g_world!=ue_wrap::world_identity::Generation() || g_serial!=net::session_serial::Current())Clear();
    ue_wrap::script_gate::WatchName(kTick,0x41545653,nullptr,&AfterTick);
    std::erase_if(g_targets,[](const auto& row){return !row.second.actor.Get();});
}
void Receive(void* actor,int16_t yaw,uint8_t author){
    if(!Current(g_epoch) || !actor || !Valid(yaw))return;
    if(yaw==net::kAtvSteeringUnavailable){g_targets.erase(actor);return;}
    if(author==0xff)author=0;
    if(author>=net::kMaxPeers || !roster_ledger::Get(author).playerNo || Local(actor))return;
    if(!g_targets.contains(actor) && g_targets.size()>=64)return;
    Target row{};row.actor.Set(actor);row.angle=yaw;row.author=author;
    row.person=roster_ledger::Get(author).playerNo;row.revision=++g_revision;
    g_targets[actor]=row;
}
void OnPeerLeft(int slot){std::erase_if(g_targets,[=](const auto& row){return row.second.author==slot;});}
void OnDisconnect(){g_session=nullptr;Clear();}
}
