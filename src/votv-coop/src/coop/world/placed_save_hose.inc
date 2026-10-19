// Hose transaction implementation, included inside placed_save_sync's private namespace.
struct HoseEdit {
    ue_wrap::CachedObjRef ref;
    void* function = nullptr;
    uint64_t baseline = 0, next = 0;
    uint32_t seq = 0;
    std::array<float, 10> root{};
    bool resumed = false, finished = false, captured = false;
    Record proposal;
};
std::map<std::wstring, HoseEdit> g_hoseEdits;
struct HoseAnswer { std::wstring key; uint32_t seq = 0, playerNo = 0; };
std::array<std::deque<HoseAnswer>, players::kMaxPeers> g_hoseAnswers;
std::array<Used, players::kMaxPeers> g_hoseSeen{};
std::array<uint64_t, players::kMaxPeers> g_hosePeerEpoch{};
size_t g_hoseAnswerSlot = 1;
void ResetHoseState() {
    g_hoseEdits.clear(); g_hoseSeen = {}; g_hoseAnswerSlot = 1;
    for (auto& answers : g_hoseAnswers) answers.clear();
}
uint64_t HoseFingerprint(const Record& record) {
    std::vector<uint8_t> bytes; W::SerSave(bytes, record);
    return blob_chunks::Fnv64(bytes);
}
bool SameHoseRoot(const Record& a, const Record& b) {
    return a.key == b.key && a.className == b.className && a.xform == b.xform &&
        a.names.size() == 1 && b.names.size() == 1 && a.names[0].size() == 2 && b.names[0].size() == 2 &&
        a.names[0][0] == b.names[0][0] && a.vectors.size() == 1 && b.vectors.size() == 1 &&
        a.vectors[0].size() >= 2 && b.vectors[0].size() >= 2 && a.vectors[0].front() == b.vectors[0].front();
}
void HoseFinished(const SG::Call& c) {
    if (!Active() || g_applying || c.fromOurCode) return;
    World();
    for (auto& [key, row] : g_rows) if (row.ref.Is(c.object)) {
        if (row.local) { row.settled = true; row.next = 0; }
        if (Host() && row.accepted) { row.dirty = true; row.pollAt = row.next = 0; }
        if (!Host()) {
            auto edit = g_hoseEdits.find(key);
            if (edit != g_hoseEdits.end() && edit->second.ref.Is(c.object) && edit->second.resumed) edit->second.finished = true;
        }
        break;
    }
}
SG::Verdict BeforeHoseResume(const SG::Call& c) {
    if (!Active() || g_applying || c.fromOurCode) return SG::Verdict::Run;
    World();
    for (const auto& [key, row] : g_rows) if (row.accepted && !row.dead && row.ref.Is(c.object)) {
        if (Host()) return row.link ? SG::Verdict::Cancel : SG::Verdict::Run;
        if (row.incoming || row.link || !SG::ClassNameWatchLive(L"groundHose_C", L"finish", kTag)) return SG::Verdict::Cancel;
        auto edit = g_hoseEdits.find(key);
        if (edit != g_hoseEdits.end()) return edit->second.finished ? SG::Verdict::Cancel : SG::Verdict::Run;
        if (g_hoseEdits.size() >= 32) return SG::Verdict::Cancel;
        HoseEdit pending; pending.ref = row.ref; pending.function = c.function;
        pending.root = row.record.xform;
        pending.baseline = HoseFingerprint(row.record); pending.seq = ++g_useSeq;
        if (!pending.seq) pending.seq = ++g_useSeq;
        g_hoseEdits.emplace(key, std::move(pending));
        break;
    }
    return SG::Verdict::Run;
}
void HoseResumed(const SG::Call& c) {
    if (!Active() || Host() || g_applying || c.fromOurCode) return;
    World();
    for (auto& [key, edit] : g_hoseEdits) if (edit.ref.Is(c.object)) { edit.resumed = true; break; }
}
void ReleaseHoseEdit(const std::wstring& key) {
    g_hoseEdits.erase(key);
    auto row = g_rows.find(key);
    if (row != g_rows.end() && !row->second.dead && row->second.accepted && row->second.ref.Get()) {
        row->second.incoming = true; row->second.next = 0;
    }
}
void FlushHoseEdits(uint64_t now) {
    const auto epoch = g_epoch;
    std::vector<std::wstring> keys;
    for (const auto& [key, edit] : g_hoseEdits) keys.push_back(key);
    int work = 0;
    for (const auto& key : keys) {
        auto it = g_hoseEdits.find(key); if (it == g_hoseEdits.end()) continue;
        void* actor = it->second.ref.Get();
        if (!actor || (!it->second.resumed && !SG::IsObjectBodyActive(it->second.function, actor))) {
            ReleaseHoseEdit(key); continue;
        }
        if (!it->second.finished || now < it->second.next) continue;
        if (++work > 4) break;
        const auto ref = it->second.ref;
        if (!it->second.captured) {
            Applying applying;
            Record raw, proposal;
            const bool ok = P::Ready(actor) && P::Capture(actor, raw) && P::Normalize(raw, proposal);
            if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation() || !Tracking() || Host()) return;
            if (!ref.Get()) { ReleaseHoseEdit(key); continue; }
            it = g_hoseEdits.find(key); if (it == g_hoseEdits.end()) continue;
            if (!ok) { it->second.next = now + kRetryMs; continue; }
            // Native transform load/get can round-trip a quaternion by a few
            // bits. A route edits world points only; retain its canonical root.
            proposal.xform = it->second.root;
            auto row = g_rows.find(key);
            if (row == g_rows.end() || !row->second.ref.Is(actor) || !SameHoseRoot(row->second.record, proposal)) {
                ReleaseHoseEdit(key); continue;
            }
            it->second.proposal = std::move(proposal); it->second.captured = true;
        }
        std::vector<uint8_t> bytes; W::AppU8(bytes, HoseRoute); W::SerSave(bytes, it->second.proposal);
        W::AppU32(bytes, it->second.seq); W::AppU32(bytes, uint32_t(it->second.baseline));
        W::AppU32(bytes, uint32_t(it->second.baseline >> 32));
        it->second.next = now + kRetryMs;
        blob_chunks::SendBlob(g_session, net::ReliableKind::PlacedSave, ++g_blobSeq, bytes);
        if (epoch != g_epoch || !Tracking() || Host()) return;
        // Keep exactly this proposal/nonce until the host's explicit answer.
        // A normal State can have been queued before this route was submitted.
    }
}
void AcceptHoseAnswer(const std::vector<uint8_t>& bytes, size_t offset, uint32_t blobSeq) {
    uint32_t seq = 0; uint8_t status = 0; std::wstring key; Record record;
    if (!W::RdU32(bytes, offset, seq) || !seq || !W::RdU8(bytes, offset, status) || status > 2 ||
        !W::RdWStr(bytes, offset, key) || !ValidKey(key)) return;
    if (status == 1) {
        Record raw;
        if (!W::DeSave(bytes, offset, raw) || !P::Normalize(raw, record) || record.key != key ||
            P::ClassKind(record.className) != P::Kind::Hose) return;
    }
    if (offset != bytes.size()) return;
    auto edit = g_hoseEdits.find(key); auto row = g_rows.find(key);
    if (edit == g_hoseEdits.end() || edit->second.seq != seq || !edit->second.captured ||
        row == g_rows.end() || !row->second.ref.Is(edit->second.ref.Get())) return;
    g_hoseEdits.erase(edit);
    if (status == 2) Dead(row->second);
    else if (!row->second.dead) {
        if (status == 1 && (!row->second.hoseStateSeen || int32_t(blobSeq - row->second.hoseStateSeq) > 0)) {
            row->second.record = std::move(record); row->second.hoseStateSeq = blobSeq; row->second.hoseStateSeen = true;
        }
        row->second.incoming = true; row->second.next = 0;
    }
}
void ReceiveHoseRoute(const Record& proposal, uint32_t seq, uint64_t baseline, uint8_t slot) {
    const auto epoch = g_epoch;
    const auto peerEpoch = g_hosePeerEpoch[slot];
    const auto playerNo = uint32_t(roster_ledger::Get(slot).playerNo);
    if (!playerNo || P::ClassKind(proposal.className) != P::Kind::Hose) return;
    auto& queue = g_hoseAnswers[slot];
    std::erase_if(queue, [playerNo](const HoseAnswer& answer) { return answer.playerNo != playerNo; });
    for (const auto& answer : queue) if (answer.seq == seq) return;
    if (queue.size() >= 32) return; // no mutation without space for its acknowledgement
    queue.push_back({proposal.key, seq, playerNo});
    const auto seen = g_hoseSeen[slot];
    if (seen.playerNo == playerNo && int32_t(seq - seen.seq) <= 0) return;
    // Rejected requests are terminal too: retrying a conflict later must not
    // suddenly apply an edit after the player already received a refusal.
    g_hoseSeen[slot] = {seq, playerNo, Now()};
    auto it = g_rows.find(proposal.key);
    if (it == g_rows.end() || !it->second.accepted || it->second.dead || it->second.incoming || it->second.link ||
        P::ClassKind(it->second.record.className) != P::Kind::Hose) return;
    const auto ref = it->second.ref; void* actor = ref.Get();
    if (!actor) return;
    const auto worldCurrent = [&] { return epoch == g_epoch && g_world == ue_wrap::world_identity::Generation() &&
        Tracking() && Host() && ref.Get(); };
    const auto current = [&] { return worldCurrent() && peerEpoch == g_hosePeerEpoch[slot] &&
        uint32_t(roster_ledger::Get(slot).playerNo) == playerNo; };
    Applying applying;
    Record raw, canonical;
    if (!P::Ready(actor) || !P::Capture(actor, raw) || !P::Normalize(raw, canonical) || !current() ||
        HoseFingerprint(canonical) != baseline || !SameHoseRoot(canonical, proposal) || !Reach(slot, proposal) || !current() ||
        !P::HoseLinksAvailable(actor, proposal) || !current()) return;
    // A previous route can be waiting for native Link in this tick. Reserve its
    // endpoints immediately, before the reverse pointers have been installed.
    for (const auto& [key, row] : g_rows) if (key != proposal.key && !row.dead && row.link &&
        P::ClassKind(row.record.className) == P::Kind::Hose && row.record.names.size() == 1 && row.record.names[0].size() == 2) {
        if (row.record.names[0][0] == proposal.names[0][0] ||
            (!proposal.names[0][1].empty() && proposal.names[0][1] != L"None" && row.record.names[0][1] == proposal.names[0][1])) return;
    }
    it = g_rows.find(proposal.key);
    if (it == g_rows.end() || !it->second.ref.Is(actor) || it->second.dead || it->second.link) return;
    it->second.record = proposal; it->second.link = it->second.dirty = true;
    it->second.linkAt = it->second.next = 0;
    const bool applied = P::Apply(actor, proposal);
    if (!worldCurrent()) return;
    // Complete geometry and reverse links in Tick before acknowledging. A
    // failed load is restored through the same native path, never broadcast as
    // an accepted proposal. No iterator crosses a reflected engine callback.
    // Once native loading has begun this accepted world mutation must finish
    // even if its requester leaves inside the call. The targeted reply is
    // separately invalidated by peer departure; no personal resources are used.
    if (!applied) { P::Apply(actor, canonical); if (!worldCurrent()) return; }
    it = g_rows.find(proposal.key);
    if (it == g_rows.end() || !it->second.ref.Is(actor) || it->second.dead) return;
    it->second.record = applied ? proposal : canonical;
    it->second.link = it->second.dirty = true; it->second.linkAt = it->second.next = 0;
}
void FlushHoseAnswers() {
    const auto epoch = g_epoch;
    // Rotate peers, and blocked hose heads within each peer, so an unfinished
    // host gesture cannot starve other completed routes or other recipients.
    for (size_t visited = 0, work = 0; visited < size_t(players::kMaxPeers - 1) && work < 4; ++visited) {
        const auto slot = uint8_t(g_hoseAnswerSlot);
        g_hoseAnswerSlot = g_hoseAnswerSlot + 1 < players::kMaxPeers ? g_hoseAnswerSlot + 1 : 1;
        auto& queue = g_hoseAnswers[slot];
        if (queue.empty()) continue;
        ++work;
        const auto answer = queue.front(); queue.pop_front();
        const auto peerEpoch = g_hosePeerEpoch[slot];
        if (uint32_t(roster_ledger::Get(slot).playerNo) != answer.playerNo) continue;
        auto it = g_rows.find(answer.key); Record record; uint8_t status = 0;
        if (it != g_rows.end() && it->second.accepted && P::ClassKind(it->second.record.className) == P::Kind::Hose) {
            if (it->second.dead) status = 2;
            else {
                const auto ref = it->second.ref; void* actor = ref.Get();
                Applying applying; Record raw;
                const bool captured = actor && !it->second.link && P::Ready(actor) && P::Capture(actor, raw) && P::Normalize(raw, record);
                if (epoch != g_epoch || g_world != ue_wrap::world_identity::Generation() || !Tracking() || !Host()) return;
                if (peerEpoch != g_hosePeerEpoch[slot] || uint32_t(roster_ledger::Get(slot).playerNo) != answer.playerNo) continue;
                if (!captured || !ref.Get() || record.key != answer.key || P::ClassKind(record.className) != P::Kind::Hose) {
                    queue.push_back(answer); continue;
                }
                status = 1;
            }
        }
        std::vector<uint8_t> bytes; W::AppU8(bytes, HoseReply); W::AppU32(bytes, answer.seq);
        W::AppU8(bytes, status); W::AppWStr(bytes, answer.key);
        if (status == 1) W::SerSave(bytes, record);
        const bool sent = blob_chunks::SendBlobToSlot(g_session, slot, net::ReliableKind::PlacedSave, ++g_blobSeq, bytes);
        if (epoch != g_epoch || !Tracking() || !Host()) return;
        if (!sent && peerEpoch == g_hosePeerEpoch[slot] && uint32_t(roster_ledger::Get(slot).playerNo) == answer.playerNo) queue.push_back(answer);
    }
}