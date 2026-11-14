// Remote flashlight clicks. Local use remains the game's native sound.
#include "coop/player/flashlight_click_sound.h"
#include "coop/net/session_serial.h"
#include "coop/player/players_registry.h"
#include "coop/player/roster_ledger.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/engine/engine.h"
#include <cmath>

namespace coop::flashlight_click_sound {
namespace {
namespace R=ue_wrap::reflection;
struct Resource {
    ue_wrap::CachedObjRef object,cls;
    void Set(void* value){object.Set(value);cls.Set(object.Get()?R::ClassOf(object.Get()):nullptr);}
    bool Current()const{return object.Get() && cls.Get() && R::ClassOf(object.Get())==cls.Get();}
};
struct LastApplied {
    int state=-1,pending=-1;bool busy=false;
    uint32_t serial=0,world=0;uint16_t person=0;uint64_t revision=0;
    ue_wrap::CachedObjRef puppet;
};
roster_ledger::PerSlotState<LastApplied> g_lastAppliedStateByPeer;
Resource g_sound,g_attenuation;
uint64_t g_revision=0;
bool g_constructingAttenuation=false;
uint64_t NextRevision(){if(!++g_revision)++g_revision;return g_revision;}
}

void PlayIfStateChanged(void* actor,uint8_t slot,bool enabled){
    if(slot>=players::kMaxPeers || !actor || !R::IsLive(actor))return;
    auto& registry=players::Registry::Get();
    if(registry.IsLocal(actor) || registry.PeerIdOfActor(actor)!=slot)return;
    Resource puppet;puppet.Set(actor);
    const auto serial=net::session_serial::Current(),world=ue_wrap::world_identity::Generation();
    const auto person=roster_ledger::Get(slot).playerNo;
    auto& row=g_lastAppliedStateByPeer[slot];
    if(row.serial!=serial || row.world!=world || row.person!=person || !row.puppet.Is(actor)){
        row={};row.serial=serial;row.world=world;row.person=person;row.puppet=puppet.object;
    }
    const int desired=enabled?1:0;
    if(row.busy && row.pending==desired)return;
    // A nested different state supersedes preparation even when it equals the
    // last admitted state. Give it a new revision before considering dedup.
    if(row.state==desired){if(row.busy){row.revision=NextRevision();row.busy=false;row.pending=-1;}return;}
    const auto revision=row.revision=NextRevision();row.busy=true;row.pending=desired;
    struct Finish {
        LastApplied& row;uint64_t revision;
        ~Finish(){if(row.revision==revision){row.busy=false;row.pending=-1;}}
    } finish{row,revision};
    const auto current=[&]{return row.revision==revision && row.busy && puppet.Current() &&
        serial==net::session_serial::Current() && world==ue_wrap::world_identity::Generation() &&
        person==roster_ledger::Get(slot).playerNo && !registry.IsLocal(actor) && registry.PeerIdOfActor(actor)==slot &&
        ue_wrap::world_identity::CurrentWorld() && ue_wrap::world_identity::WorldOf(actor)==ue_wrap::world_identity::CurrentWorld();};
    if(!current())return;
    if(!g_sound.Current()){
        Resource candidate;candidate.Set(R::FindObject(ue_wrap::profile::name::FlashlightClickSoundName,ue_wrap::profile::name::SoundWaveClass));
        if(!g_sound.Current() && candidate.Current())g_sound=candidate;
    }
    if(!current() || !g_sound.Current())return;
    const auto sound=g_sound;
    if(!g_attenuation.Current()){
        if(g_constructingAttenuation)return;
        g_constructingAttenuation=true;
        struct Construction {~Construction(){g_constructingAttenuation=false;}} construction;
        ue_wrap::engine::SoundAttenuationConfig cfg{};
        cfg.extents[0]/=3.f;cfg.falloffDistance/=3.f;
        Resource candidate;candidate.Set(ue_wrap::engine::SpawnSoundAttenuation(cfg));
        // This constant shared resource is independent of a particular puppet.
        if(!g_attenuation.Current() && candidate.Current())g_attenuation=candidate;
    }
    if(!current() || !sound.Current() || !g_attenuation.Current())return;
    const auto attenuation=g_attenuation;
    ue_wrap::FVector position{};
    if(!ue_wrap::engine::TryGetActorLocation(actor,position) || !std::isfinite(position.X) ||
       !std::isfinite(position.Y) || !std::isfinite(position.Z) || !current() || !sound.Current() || !attenuation.Current())return;
    // Commit immediately before the one native attempt. A reentrant mode packet
    // cannot replay this click; unknown native dispatch completion is not retried.
    row.state=desired;row.busy=false;row.pending=-1;
    ue_wrap::engine::PlaySoundAtLocation(actor,sound.object.Get(),position,attenuation.object.Get());
}
}
