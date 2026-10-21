bool TryApply(void* actor, const std::wstring& key, const SR::SaveRecord& rec, uint64_t epoch,bool preserveGrillRuntime,
              const std::function<bool()>& current) {
    if (!actor) {
        UE_LOGW("prop_save_data: TryApply failed -- actor is null for key '%ls'", key.c_str());
        return false;
    }
    if (!Covers(actor)) {
        UE_LOGW("prop_save_data: TryApply failed -- actor %p does not cover save state for key '%ls'", actor, key.c_str());
        coop::dev::food_clock_probe::NoteRefused(actor);
        return false;
    }
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    // Client inventory records can arrive after the host has unlocked this safe.
    // Preserve the irreversible lock transition, including parked-before-birth records.
    SR::SaveRecord adjusted;
    const SR::SaveRecord* applied=&rec;
    if(preserveGrillRuntime && PT::SessionIsHost() && ue_wrap::grill_saved_state::Supported(actor)) {
        adjusted=rec;
        if(!ue_wrap::grill_saved_state::PreserveHostRecord(actor,adjusted) || !current() || epoch!=g_worldEpoch || identity.Get()!=actor)return false;
        if((adjusted.floats!=rec.floats || adjusted.ints!=rec.ints || adjusted.bools!=rec.bools) && g_session) {
            SendBody(g_session,-1,key,adjusted,nullptr,current);
            if(!current() || epoch!=g_worldEpoch || identity.Get()!=actor)return false;
        }
        applied=&adjusted;
    }
    if(PT::SessionIsHost()&&ue_wrap::safe::IsSafe(actor)) {
        adjusted=rec;
        const bool changed=ue_wrap::safe::PreserveUnlocked(actor,adjusted);
        if(!current())return false;
        if(changed&&g_session) {
            // A claim parked before the safe existed may already have been
            // relayed. Correct that earlier canonical as soon as it can resolve.
            SendBody(g_session,-1,key,adjusted,nullptr,current);
            if(!current() || epoch!=g_worldEpoch||identity.Get()!=actor) {
                UE_LOGW("prop_save_data: TryApply failed -- identity changed after PreserveUnlocked for key '%ls'", key.c_str());
                return false;
            }
        }
        applied=&adjusted;
    }
    if (!current() || !SR::ApplyRecord(actor, *applied)) {
        UE_LOGW("prop_save_data: TryApply failed -- SR::ApplyRecord returned false for key '%ls'", key.c_str());
        return false;
    }
    if (!current() || epoch != g_worldEpoch || identity.Get() != actor) {
        UE_LOGW("prop_save_data: TryApply failed -- identity changed after ApplyRecord for key '%ls'", key.c_str());
        return false;
    }
    ++g_appliedTotal;
    UE_LOGI("prop_save_data: TryApply success for key '%ls' on actor %p", key.c_str(), actor);
    coop::dev::food_clock_probe::NoteApply(actor, key, *applied);
    return epoch == g_worldEpoch;
}

// Own both values across native dispatch. The queue can be cleared or this key replaced by a
// callback; only the exact row we started with may be retired or charged a failure afterward.
bool ApplyQueued(void* actor, const std::wstring& key, bool budgeted) {
    if (!actor || key.empty() || g_applying.count(key)) return false;
    if (budgeted && g_applyBudget <= 0) return false;
    const auto it = g_parked.find(key);
    if (it == g_parked.end()) return false;
    if (budgeted && it->second.failures >= kMaxApplyFailures) return false;
    const std::wstring applyKey = key;
    const Parked snapshot = it->second;
    const uint64_t epoch = g_worldEpoch;
    const auto world=ue_wrap::world_identity::Generation();
    KeyScope scope(g_applying, applyKey);
    RecordScope order(applyKey);const auto revision=order.Revision();
    const auto current=[&]{const auto it=g_parked.find(applyKey);return epoch==g_worldEpoch && world==ue_wrap::world_identity::Generation() &&
        order.Revision()==revision && it!=g_parked.end() && it->second.seq==snapshot.seq;};
    if (budgeted) --g_applyBudget; // failed dispatches cost a slot too; nested skips do not
    const bool ok = TryApply(actor, applyKey, snapshot.rec, epoch,snapshot.preserveGrillRuntime,current);
    if (epoch != g_worldEpoch) return false;
    const auto landed = g_parked.find(applyKey);
    if (landed != g_parked.end() && landed->second.seq == snapshot.seq) {
        if (ok) Unpark(landed);
        else if (++landed->second.failures == kMaxApplyFailures) {
            UE_LOGW("prop_save_data: the record for key '%ls' failed to apply %d times -- it "
                    "stays parked but is no longer retried; that prop keeps its own state",
                    applyKey.c_str(), kMaxApplyFailures);
        }
    }
    return ok;
}

// Queue first so a newer arrival during native dispatch cannot be overwritten by this caller.
void LandRecord(const std::wstring& key, SR::SaveRecord&& rec, uint8_t senderSlot, size_t bytes,
                bool budgeted = true,bool preserveGrillRuntime=false) {
    Park(key, std::move(rec), senderSlot, bytes);
    if(auto it=g_parked.find(key);it!=g_parked.end())it->second.preserveGrillRuntime=preserveGrillRuntime;
    // The index only, never ResolveLiveActorByKey: that one falls back to a cold GUObjectArray
    // scan on a miss, and a join hands this lane hundreds of arrivals at once. A prop the index
    // does not know yet is parked and retried at O(1) on the next frame instead.
    void* actor = (!budgeted || g_applyBudget > 0) ? PT::FindLiveActorByKey(key) : nullptr;
    ApplyQueued(actor, key, budgeted);
}

