// Native scorchMark traces six surfaces. Its host decals enter the same identity,
// cleaning, saved adoption and retirement lane as liquid deposits, without sound.
void PruneScorchCalls(){std::erase_if(g_scorchCalls,[](const ScorchCall& row){
    return !Current(row.epoch) || !row.source.Get() || !SG::IsInvocationActive(row.invocation);
});
    const auto world=ue_wrap::world_identity::Generation();
    if(g_scorchWorld!=world){g_scorchOrigins.clear();g_scorchWorld=world;}
    std::erase_if(g_scorchOrigins,[](const auto& row){return !row.second.actor.Get();});
}
bool SharedScorch(void* actor){const auto row=g_scorchOrigins.find(actor);return g_scorchWorld==ue_wrap::world_identity::Generation() &&
    row!=g_scorchOrigins.end() && row->second.shared && row->second.actor.Is(actor);}
bool PrivateScorch(void* actor){const auto row=g_scorchOrigins.find(actor);return g_scorchWorld==ue_wrap::world_identity::Generation() &&
    row!=g_scorchOrigins.end() && !row->second.shared && row->second.actor.Is(actor);}
bool RememberScorch(void* actor,bool shared){
    if(g_scorchWorld!=ue_wrap::world_identity::Generation())return false;
    if(!g_scorchOrigins.contains(actor) && g_scorchOrigins.size()>=8192)return false;
    ScorchOrigin row{};row.actor.Set(actor);row.shared=shared;if(!row.actor.Get())return false;
    g_scorchOrigins[actor]=row;return true;
}
const ScorchCall* ActiveScorch(void* source=nullptr){
    const auto call=std::find_if(g_scorchCalls.rbegin(),g_scorchCalls.rend(),[&](const ScorchCall& row){
        return Current(row.epoch) && row.source.Get() && (!source || row.source.Is(source)) && SG::IsInvocationActive(row.invocation);
    });return call==g_scorchCalls.rend()?nullptr:&*call;
}
void ObserveScorchOrigin(void* actor){if(const auto* call=ActiveScorch())RememberScorch(actor,call->shared);}
SG::Verdict BeforeScorch(const SG::Call& call){
    World();if(!Current())return SG::Verdict::Run;const auto epoch=g_epoch;
    CachedObjRef source;source.Set(call.object);
    const bool blast=ue_wrap::prop_explosion::IsGameplayBlast(call.object);
    const bool pc=!blast && ue_wrap::prop_explosion::IsPrivatePcBlast(call.object);
    if(!Current(epoch) || !source.Get())return SG::Verdict::Cancel;
    if(!blast && !pc)return SG::Verdict::Run;
    // Replicated and original client blasts must not trace independent surfaces.
    if(blast && !Host())return SG::Verdict::Cancel;
    PruneScorchCalls();
    if(!g_hook || !g_finishFunction.Get() || !call.invocation || g_scorchCalls.size()>=32)return SG::Verdict::Run;
    ScorchCall row{};row.source=source;row.invocation=call.invocation;row.epoch=epoch;row.shared=blast;
    if(row.source.Get() && SG::IsInvocationActive(row.invocation))g_scorchCalls.push_back(row);
    return SG::Verdict::Run;
}
bool ObserveScorchFinish(void* source,void* actor){
    const auto epoch=g_epoch;const auto* call=ActiveScorch(source);if(!call)return false;
    const bool shared=call->shared;
    uint8_t kind=0;if(!N::DecalKind(actor,kind) || kind!=N::kExplosionScorch)return false;
    if(!Current(epoch))return true;
    if(!RememberScorch(actor,shared))return true;
    if(!shared || !Host())return true;
    const auto assigned=g_actorIds.find(actor);
    if(assigned!=g_actorIds.end() && assigned->second.actor.Is(actor))return true;
    Packet owner{};owner.kind=N::kExplosionScorch;
    for(auto& pending:g_candidates)if(pending.actor.Is(actor)){
        if(pending.baseline){pending.owner=owner;pending.baseline=false;pending.expires=Now()+1500;}return true;
    }
    if(g_candidates.size()>=256)return true;
    Candidate row{};row.actor.Set(actor);row.owner=owner;row.expires=Now()+1500;
    g_candidates.push_back(row);g_unassigned[actor]=row.actor;return true;
}
