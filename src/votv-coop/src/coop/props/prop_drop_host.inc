// Host: spawn the authoritative prop by key at the transform. Mirrors the spawn receiver's
// spawn-by-key (deferred begin, set the key, write the parity identity, finish) but does not
// mark it a mirror, so the host's own finish-spawn watcher catches it and broadcasts the
// authoritative spawn to every peer. Returns the spawned actor, or null.
void* HostSpawnPlacedProp(const coop::net::PropDropIntentPayload& p, const std::wstring& cls,
                          const std::wstring& key, uint8_t authorSlot) {
    const uint64_t epoch = g_pendingEpoch;
    void* world = ue_wrap::world_identity::CurrentWorld();
    const auto occupant = coop::roster_ledger::Get(authorSlot).playerNo;
    const auto generation = coop::roster_ledger::Get(authorSlot).bornGeneration;
    const auto contextCurrent = [&] {
        return epoch == g_pendingEpoch && world == ue_wrap::world_identity::CurrentWorld() &&
            occupant == coop::roster_ledger::Get(authorSlot).playerNo &&
            generation == coop::roster_ledger::Get(authorSlot).bornGeneration;
    };
    // Host resolving a CLIENT's placed-prop class: object_index, so an attacker's unknown class is an
    // O(1) miss, not a whole-array walk. Host world is always up, so the index is current.
    void* clsObj = ue_wrap::object_index::ClassByName(cls.c_str());
    if (!clsObj) {
        UE_LOGW("[PROP-DROP] HOST class '%ls' not found -- cannot spawn placed prop key='%ls'",
                cls.c_str(), key.c_str());
        return nullptr;
    }
    // The lineage gate, on the CLASS, before anything is spawned. This author is the one place a
    // client names a class the host then constructs, and the spawn-menu door widened its firing
    // set from four whitelisted lineages to the whole prop catalog -- so what the wire may name
    // has to be stated here rather than left to the callers. Prop lineage is the whole of it: the
    // parity write below already assumes it, the intent kinds are all prop lanes, and a creature
    // or event class arriving on this lane would run an unrelated begin-play on the host's world.
    // A client cannot reach this with a non-prop class through any built door; refusing is for the
    // wire, which is not a door.
    if (!ue_wrap::prop::IsClassDescendantOfProp(clsObj)) {
        UE_LOGW("[PROP-DROP] HOST refusing spawn intent from slot=%u: class '%ls' is not prop "
                "lineage (key='%ls')", authorSlot, cls.c_str(), key.c_str());
        return nullptr;
    }
    // Prop lineage is necessary and not sufficient. Two prop-lineage families are ones the host's
    // own express refuses to broadcast: a per-player class (the inventory container, which is one
    // actor per peer by design) and a wire-suppressed intermediate variant. Spawned from a wire
    // intent the host would construct them and then never express them -- one silent actor per
    // packet, accumulating in its world with nothing to retire it. A client reaches neither
    // through a built door; this states it for the wire, which is not a door.
    if (coop::prop_lifecycle::IsPerPlayerPropClass(cls) ||
        coop::prop_lifecycle::IsWireSuppressedPropClass(cls)) {
        UE_LOGW("[PROP-DROP] HOST refusing spawn intent from slot=%u: class '%ls' is per-player or "
                "wire-suppressed -- the host would build it and never express it (key='%ls')",
                authorSlot, cls.c_str(), key.c_str());
        return nullptr;
    }
    const ue_wrap::FVector  loc{p.locX, p.locY, p.locZ};
    const ue_wrap::FRotator rot{p.rotPitch, p.rotYaw, p.rotRoll};
    if (!contextCurrent()) return nullptr;
    void* actor = E::BeginDeferredSpawn(clsObj, loc, rot);
    if (!contextCurrent()) return nullptr;
    if (!actor) {
        UE_LOGW("[PROP-DROP] HOST BeginDeferredSpawn('%ls') failed", cls.c_str());
        return nullptr;
    }
    if (p.hasFoodCondition > 1 ||
        (ue_wrap::food::IsFood(actor) != (p.hasFoodCondition == 1)) ||
        (p.hasFoodCondition && !coop::props::prop_food_sync::ValidCondition(
            p.foodTemperature, p.foodRipeness, p.foodUses,p.foodCookProgress))) {
        E::DestroyActor(actor);
        return nullptr; // never author a default portion from missing or malformed food data
    }
    ue_wrap::CachedObjRef identity;
    identity.Set(actor);
    const auto stillCurrent = [&] {
        return contextCurrent() && identity.Get() == actor;
    };
    // Set the key before finishing: the init inside the finish spawn's construction script mints
    // a fresh key unless one is already set, and writing our wire key first keeps the cross-peer
    // identity. The setter is resolved on the prop base class (its declaring class) and cached,
    // as the spawn receiver does: the function lookup is exact-owner with no superclass climb,
    // so a leaf-class resolve missed every subclass that does not redeclare the setter, and the
    // host spawn then auto-minted a key unequal to the client's, an identity split and a
    // host-side duplicate. Every wire class reaching here is prop lineage (the intent author
    // gates on it), so the base's setter is a valid member call on the spawned actor.
    static void* s_setKeyFn = nullptr;
    if (!s_setKeyFn) {
        if (void* propBase = R::FindClass(P::name::PropClass))
            s_setKeyFn = R::FindFunction(propBase, P::name::PropSetKeyFn);
    }
    void* setKeyFn = s_setKeyFn;
    bool keyed = false;
    if (setKeyFn) {
        const R::FName kf = ue_wrap::fname_utils::StringToFName(key);
        if (!stillCurrent()) return nullptr;
        if (kf.ComparisonIndex != 0) {
            ue_wrap::ParamFrame sk(setKeyFn);
            keyed = sk.SetRaw(L"Key", &kf, sizeof(kf)) && stillCurrent() && ue_wrap::Call(actor, sk);
        }
    }
    if (!stillCurrent()) return nullptr;
    if (!keyed) {
        E::DestroyActor(actor);
        return nullptr; // never acknowledge an auto-minted identity different from the client's
    }
    // The single-player parity identity (the props-table name and the static,
    // remove-without-respawn, frozen and sleep flags) before finishing: the init resolves the
    // true mesh, mass and collision from the name (empty gives the default white cube).
    if (ue_wrap::prop::IsDescendantOfProp(actor)) {
        const std::wstring nameW = WireToWide(p.propName.len, p.propName.data, sizeof(p.propName.data));
        R::FName nameRow{0, 0};
        if (!nameW.empty() && nameW != L"None") nameRow = ue_wrap::fname_utils::StringToFName(nameW);
        if (!stillCurrent()) return nullptr;
        if (!nameW.empty() && nameW != L"None" && nameRow.ComparisonIndex == 0) {
            E::DestroyActor(actor);
            return nullptr;
        }
        namespace pf = coop::net::propspawn_flags;
        const bool parity = ue_wrap::prop::WriteSpParityIdentity(
            actor, nameRow,
            (p.physFlags & pf::kStatic) != 0,
            (p.physFlags & pf::kRemoveWOrespawn) != 0,
            (p.physFlags & pf::kFrozen) != 0,
            (p.physFlags & pf::kSleep) != 0);
        if (!stillCurrent()) return nullptr;
        if (!parity) { E::DestroyActor(actor); return nullptr; }
    }
    if (!stillCurrent()) return nullptr;
    // No mirror mark: the host finish-spawn watcher must see this and broadcast it.
    if (!E::FinishDeferredSpawn(actor, loc, rot)) {
        UE_LOGW("[PROP-DROP] HOST FinishDeferredSpawn('%ls') failed", cls.c_str());
        if (stillCurrent()) E::DestroyActor(actor);
        return nullptr;
    }
    if (!stillCurrent()) return nullptr;
    const auto finalKey = ue_wrap::prop::GetInteractableKeyString(actor);
    if (!stillCurrent()) return nullptr;
    if (finalKey != key) {
        E::DestroyActor(actor);
        return nullptr;
    }
    // Construction initializes a full portion. Restore the client's LIVE values afterwards,
    // before the host poll or next-tick birth broadcast can publish those defaults.
    if (p.hasFoodCondition) {
        const bool restored = ue_wrap::food::Write(actor, {p.foodTemperature, p.foodRipeness, p.foodUses,p.foodCookProgress});
        if (!stillCurrent()) return nullptr;
        if (!restored) {
            E::DestroyActor(actor);
            return nullptr;
        }
        coop::props::prop_food_sync::ForgetPublished(key);
    }
    // Scale is a runtime transform (the deferred begin takes location and rotation only); apply
    // it after finishing and before the next-tick drain re-reads the scale for the broadcast.
    if (p.scaleX > 0.001f || p.scaleY > 0.001f || p.scaleZ > 0.001f) {
        // Clamped, because a scale is the one field on this intent a client can make absurd with
        // no other consequence to itself: the lane reads the client's own actor, so an honest
        // value is whatever the game gave it, and the bound only has to be past anything the game
        // does. Client-scoped by the project's rule -- the host's own spawns never come through
        // here.
        constexpr float kMaxScale = 100.0f;
        auto clamp = [](float v) { return v < 0.f ? 0.f : (v > kMaxScale ? kMaxScale : v); };
        const bool scaled = E::SetActorScale3D(actor, ue_wrap::FVector{clamp(p.scaleX), clamp(p.scaleY), clamp(p.scaleZ)});
        if (!stillCurrent()) return nullptr;
        if (!scaled) { E::DestroyActor(actor); return nullptr; }
    }
    if (!stillCurrent()) return nullptr;
    // Container contents live in a separate inventory slice, outside getData.
    // Reserve it before record restoration can reenter the contents publisher.
    coop::props::container_birth::ExpectBirthSlice(actor, authorSlot);
    if (!stillCurrent()) return nullptr;
    // The prop's own save record, if the author's copy is already here. It usually is not -- it
    // rides behind this intent in the same FIFO -- and then it lands on this actor by Key once the
    // actor is indexed, which is what makes the store keyed by identity rather than by actor.
    // Until then this key is AWAITED: the host has just spawned a class-default copy, and
    // publishing that as canonical would overwrite the author's real state on the author's own
    // machine.
    if (!p.hasFoodCondition) {
        const bool applied = coop::prop_save_data::ApplyParked(actor, key);
        if (!stillCurrent()) return nullptr;
        if (!applied) coop::prop_save_data::ExpectRecordFor(actor, key, authorSlot);
    }
    return actor;
}

