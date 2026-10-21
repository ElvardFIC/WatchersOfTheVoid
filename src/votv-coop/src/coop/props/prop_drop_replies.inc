// Private transaction state for prop_drop_intent.cpp. All access is on the game thread.
uint32_t g_nextPlacementRequest = 0; // survives Reset: late replies cannot match a new request
void* g_placementWorld = nullptr;
std::vector<ue_wrap::CachedObjRef> g_refusedPlacements;
bool RetainForRecovery(const PendingPlace& entry, const std::wstring& key, const char* reason) {
    void* actor = entry.actor.Get();
    if (!actor) return true;
    if (!entry.admitted) return false;
    for (const auto& ref : g_refusedPlacements) if (ref.Get() == actor) return true;
    if (g_refusedPlacements.size() >= 256) return false;
    g_refusedPlacements.push_back(entry.actor);
    UE_LOGW("[PROP-DROP] role=client actor=%p cls='%ls' key='%ls' reason=%s world=%u; retained locally for pickup, no fallback author",
        actor, R::ClassNameOf(actor).c_str(), key.c_str(), reason, ue_wrap::world_identity::Generation());
    return true;
}
struct PlacementRecord {
    net::PropDropIntentPayload request{};
    net::ReliableKind kind = net::ReliableKind::PropDropIntent;
    net::PropPlacementCode code = net::PropPlacementCode::Retry;
    ue_wrap::CachedObjRef actor;
    bool processing = false, owed = false;
};
struct PlacementPeer {
    uint32_t newest = 0;
    std::deque<PlacementRecord> records;
};
coop::roster_ledger::PerSlotState<PlacementPeer> g_placementPeers;
uint64_t PlacementNow() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
PlacementRecord* PlacementRow(int slot, uint32_t id) {
    for (auto& r : g_placementPeers[slot].records) if (r.request.requestId == id) return &r;
    return nullptr;
}
bool SamePlacement(const PlacementRecord& r, const net::PropDropIntentPayload& p,
                   net::ReliableKind kind) {
    return r.kind == kind && std::memcmp(&r.request, &p, sizeof(p)) == 0;
}
net::PropPlacementResultPayload PlacementReply(const net::PropDropIntentPayload& p,
                                              net::PropPlacementCode code) {
    return {p.requestId, p.className, p.key, static_cast<uint8_t>(code)};
}
bool ValidPlacement(const net::PropDropIntentPayload& p) {
    if (!p.requestId || !p.className.len || p.className.len > sizeof(p.className.data) ||
        !p.key.len || p.key.len > sizeof(p.key.data) || p.propName.len > sizeof(p.propName.data) ||
        p.hasFoodCondition > 1 || p._pad[0] || p._pad[1]) return false;
    const auto clean = [](const auto& text) {
        return std::find(text.data,text.data + text.len, char{}) == text.data + text.len;
    };
    if (!clean(p.className) || !clean(p.key) || !clean(p.propName)) return false;
    const float values[]{p.locX,p.locY,p.locZ,p.rotPitch,p.rotYaw,p.rotRoll,p.scaleX,p.scaleY,p.scaleZ};
    for (float f : values) if (!std::isfinite(f)) return false;
    return !p.hasFoodCondition || coop::props::prop_food_sync::ValidCondition(
        p.foodTemperature, p.foodRipeness, p.foodUses,p.foodCookProgress);
}
// Admit before native calls. The high-water mark survives cache eviction, so a late retry
// cannot recreate an already collected item. A recycled roster slot clears the whole ledger.
bool BeginPlacement(net::Session& session, const net::PropDropIntentPayload& p,
                    int slot, net::ReliableKind kind) {
    if (session.role() != net::Role::Host || !session.connected() || slot <= 0 ||
        slot >= coop::roster_ledger::kMaxSlots || !session.HasPeerConn(slot) ||
        !coop::roster_ledger::Get(slot).occupied() || !session.IsSlotWorldReady(slot) || !p.requestId) return false;
    auto& peer = g_placementPeers[slot];
    if (auto* old = PlacementRow(slot,p.requestId)) {
        if (!SamePlacement(*old,p,kind)) {
            const auto denied = PlacementReply(p,net::PropPlacementCode::Refused);
            session.SendReliableToSlot(slot,net::ReliableKind::PropPlacementResult,&denied,sizeof(denied));
            return false;
        }
        if (old->processing) return false; // outer transaction will publish its actual outcome
        if (old->code != net::PropPlacementCode::Retry) {
            old->owed = true;
            void* actor = old->actor.Get();
            if (old->code == net::PropPlacementCode::Accepted && actor)
                coop::prop_snapshot::ExpressIncrementalSpawn(actor);
            return false;
        }
        old->processing = true;
        return true;
    }
    if (peer.newest && static_cast<int32_t>(p.requestId - peer.newest) <= 0) {
        const auto denied = PlacementReply(p,net::PropPlacementCode::Refused);
        session.SendReliableToSlot(slot,net::ReliableKind::PropPlacementResult,&denied,sizeof(denied));
        return false;
    }
    // Never evict a transaction whose native call is on the stack or whose reply is unsent.
    if (peer.records.size() >= 256) {
        const auto discard = std::find_if(peer.records.begin(),peer.records.end(),
            [](const PlacementRecord& r) { return !r.processing && !r.owed && r.code != net::PropPlacementCode::Retry; });
        if (discard == peer.records.end()) {
            const auto retry = PlacementReply(p,net::PropPlacementCode::Retry);
            session.SendReliableToSlot(slot,net::ReliableKind::PropPlacementResult,&retry,sizeof(retry));
            return false;
        }
        peer.records.erase(discard);
    }
    peer.newest = p.requestId;
    peer.records.push_back({p,kind,net::PropPlacementCode::Retry,{},true,false});
    return true;
}
void FinishPlacement(int slot, uint32_t id, net::PropPlacementCode code, void* actor = nullptr) {
    if (auto* row = PlacementRow(slot,id)) {
        row->actor.Set(actor);
        row->code = code;
        row->processing = false;
        row->owed = true;
    }
}
void PublishAccepted(net::Session* session) {
    const auto epoch = g_pendingEpoch;
    std::vector<AwaitingHost> accepted;
    for (const auto& row : g_awaitingHost)
        if (row.accepted && !row.recordSubmitted) accepted.push_back(row);
    for (const auto& row : accepted) {
        if (epoch != g_pendingEpoch || !session->connected() || session->role() != net::Role::Client) return;
        void* actor = row.actor.Get();
        if (!actor || coop::hand_item::IsHandAxisActor(actor)) continue;
        if (ue_wrap::prop::GetInteractableKeyString(actor) != row.key || epoch != g_pendingEpoch ||
            row.actor.Get() != actor || !session->connected() || session->role() != net::Role::Client) continue;
        // Publish can reenter Tick. Mark before crossing that boundary.
        for (auto& live : g_awaitingHost) if (live.request.requestId == row.request.requestId) live.recordSubmitted = true;
        // Only accepted placements may transfer the separate container inventory.
        coop::props::container_birth::NoteAuthoredBirth(actor);
        if (epoch != g_pendingEpoch || row.actor.Get() != actor || !session->connected() ||
            session->role() != net::Role::Client) return;
        if (row.parked) UnparkKey(row.key);
        coop::join_membership_sweep::RecordClaimIfTracking(actor);
        coop::prop_save_data::Publish(session,actor,row.key);
        if (epoch != g_pendingEpoch) return;
    }
}
void PlacementTick(net::Session* session) {
    void* world = ue_wrap::world_identity::CurrentWorld();
    if (g_placementWorld && world != g_placementWorld) Reset();
    g_placementWorld = world;
    std::erase_if(g_refusedPlacements,[](const auto& r) {
        void* a = r.Get();
        return !a || PT::GetPropElementIdForActor(a) != coop::element::kInvalidId;
    });
    if (!session || !session->connected()) return;
    const uint64_t epoch = g_pendingEpoch, now = PlacementNow();
    if (session->role() == net::Role::Client) {
        PublishAccepted(session);
        // Copy before transport/native boundaries; a callback can clear or append to the queue.
        std::vector<AwaitingHost> retry;
        for (auto& row : g_awaitingHost) {
            void* a = row.actor.Get();
            if (!a || (row.recordSubmitted && PT::GetPropElementIdForActor(a) != coop::element::kInvalidId) || now < row.next) continue;
            if (retry.size() < 4) {
                row.next = now + 1000;
                retry.push_back(row);
            }
        }
        for (const auto& row : retry) {
            if (epoch != g_pendingEpoch || !session->connected() || session->role() != net::Role::Client) return;
            if (!row.actor.Get()) continue;
            session->SendReliable(row.kind,&row.request,sizeof(row.request));
        }
        return;
    }
    for (int slot = 1; slot < g_placementPeers.size(); ++slot) {
        if (!session->HasPeerConn(slot)) continue;
        const auto occupant = coop::roster_ledger::Get(slot).playerNo;
        const auto generation = coop::roster_ledger::Get(slot).bornGeneration;
        std::vector<net::PropPlacementResultPayload> out;
        for (const auto& row : g_placementPeers[slot].records)
            if (row.owed && !row.processing && out.size() < 4) out.push_back(PlacementReply(row.request,row.code));
        for (const auto& p : out) {
            if (epoch != g_pendingEpoch || !session->connected() || session->role() != net::Role::Host ||
                occupant != coop::roster_ledger::Get(slot).playerNo ||
                generation != coop::roster_ledger::Get(slot).bornGeneration) return;
            if (!session->SendReliableToSlot(slot,net::ReliableKind::PropPlacementResult,&p,sizeof(p))) break;
            if (epoch != g_pendingEpoch || occupant != coop::roster_ledger::Get(slot).playerNo ||
                generation != coop::roster_ledger::Get(slot).bornGeneration) return;
            // A reentrant handler may have changed Retry to a terminal result. Do not erase that debt.
            if (auto* row = PlacementRow(slot,p.requestId); row && static_cast<uint8_t>(row->code) == p.code)
                row->owed = false;
        }
    }
}
