// coop/items/inventory_pickup_sync.cpp -- see coop/items/inventory_pickup_sync.h.

#include "coop/items/inventory_pickup_sync.h"
#include "container_extract_restore.h"

#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/props/prop_save_data.h"
#include "coop/props/prop_sound.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace coop::inventory_pickup_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;

UE_FACT_FN(kRowPlaySound2D, "inventory_pickup_sync", L"GameplayStatics", L"PlaySound2D", Declared);

std::atomic<coop::net::Session*> g_session{nullptr};

// Resolved once by Install on the game thread, before the observer registers; read-only after.
ue_wrap::facts::Resolved g_playSound2DFn;
// The cue is judged by its name, which lives for the process, so no pointer to the asset is held.
R::FName g_cueName{};
int32_t g_offSound = -1;
int32_t g_offPitch = -1;
int32_t g_offWco   = -1;
bool    g_observerRegistered = false;
bool    g_pocketWatchRegistered = false;

// A successful putObjectInventory2 captures getData into the player's personal store and then
// destroys the actor. Publish the live keyed record before that capture/destruction edge so the
// host does not retain an older copy and later restore the item's data on another interaction.
constexpr const wchar_t* kPocketVerb = L"putObjectInventory2";
constexpr int kPocketTag = 0x504F434B;  // 'POCK'
ue_wrap::script_gate::Verdict OnPocketPre(const ue_wrap::script_gate::Call& call) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected() || !call.locals || !call.function) return ue_wrap::script_gate::Verdict::Run;
    void* local = coop::players::Registry::Get().Local();
    if (!local || call.object != local) return ue_wrap::script_gate::Verdict::Run;

    static ue_wrap::CachedObjRef s_fn;
    static int32_t s_inputOff = -1;
    static int32_t s_returnOff = -1;
    if (!s_fn.Is(call.function)) {
        s_fn.Set(call.function);
        s_inputOff = R::FindParamOffset(call.function, L"InputPin");
        s_returnOff = R::FindParamOffset(call.function, L"return");
        const int32_t frame = R::FunctionFrameSize(call.function);
        if (s_inputOff < 0 || s_inputOff > frame || sizeof(void*) > frame - s_inputOff)
            s_inputOff = -1;
        if (s_returnOff < 0 || s_returnOff >= frame) s_returnOff = -1;
        if (s_inputOff < 0)
            UE_LOGW("inventory_pickup: %ls InputPin did not resolve -- mutable pocket state cannot be published",
                    kPocketVerb);
    }
    if (s_inputOff < 0) return ue_wrap::script_gate::Verdict::Run;
    const auto cancelPickup = [&] {
        // Cancel preserves out parameters. A reused caller frame may still hold
        // true from its previous pickup: explicitly report that no transfer took place.
        if (s_returnOff >= 0) {
            call.locals[s_returnOff] = 0;
            if (uint8_t* out = ue_wrap::script_gate::OutParamPtr(call, s_returnOff)) *out = 0;
        }
        return ue_wrap::script_gate::Verdict::Cancel;
    };
    void* actor = nullptr;
    std::memcpy(&actor, call.locals + s_inputOff, sizeof(actor));
    if (!actor || !R::IsLive(actor) || !ue_wrap::prop::IsKeyedInteractable(actor))
        return ue_wrap::script_gate::Verdict::Run;
    ue_wrap::CachedObjRef carrier;
    carrier.Set(actor);
    const auto extracted = detail::RestoreContainerCarrier(call, actor);
    if (extracted != detail::ExtractRestore::Unrelated) {
        if (extracted == detail::ExtractRestore::Failed)
            UE_LOGW("inventory_pickup: container carrier restore failed; native extraction continues");
        // The container lane owns this transfer. A temporary carrier's pre-pocket record must
        // not become a stale parked world update. If pocketing fails, the normal spawn drain
        // publishes the surviving, now restored world actor instead.
        return carrier.Get() == actor ? ue_wrap::script_gate::Verdict::Run
                                      : cancelPickup();
    }
    const std::wstring key = ue_wrap::prop::GetInteractableKeyString(actor);
    if (carrier.Get() != actor) return cancelPickup();
    if (key.empty() || key == L"None") return ue_wrap::script_gate::Verdict::Run;
    if (!coop::prop_save_data::PrepareForCapture(actor, key))
        return cancelPickup();
    const std::wstring className = R::ClassNameOf(actor);
    const bool carriesRecord = coop::prop_save_data::Covers(actor);
    const bool published = coop::prop_save_data::Publish(s, actor, key);
    // The native pickup destroys this actor. A retry that only retains its live
    // pointer cannot recover the charge/contents afterward; retain the world item
    // until its supported mutable record has entered the reliable queue.
    if (carriesRecord && !published) return cancelPickup();
    if (published)
        UE_LOGI("inventory_pickup: published mutable state before pocketing key='%ls' cls='%ls'",
                key.c_str(), className.c_str());
    return carrier.Get() == actor ? ue_wrap::script_gate::Verdict::Run
                                  : cancelPickup();
}

// POST observer on UGameplayStatics::PlaySound2D, which dispatches at human-event rate game-wide
// (UI clicks, 2D cues); the body is three cached-offset reads and compares, exiting on the first
// mismatch. The predicate:
//   Sound is inventory_Cue    its name against the resolved one, two integer compares
//   1.05 < pitch < 1.2        the collect plays 1.1; the same cue on a climb plays 0.9
//   WorldContext == LOCAL     the collector. A puppet has no input stack and can never dispatch
//                             this; and the gamemode's own collect helper plays the same cue at
//                             the same pitch with ITSELF as the context, so this test is what
//                             makes the cue-and-pitch pair unambiguous.
// putObjectInventory2 plays the cue once, on its single success path, so one dispatch that
// passes all three tests is one collect.
void OnPlaySound2DPost(void* /*self*/, void* /*function*/, void* params) {
    if (!GT::IsGameThread() || !params) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;

    const auto* p = static_cast<const uint8_t*>(params);
    void* sound = *reinterpret_cast<void* const*>(p + g_offSound);
    if (!sound) return;
    const R::FName& soundName = R::NameOf(sound);
    if (soundName.ComparisonIndex != g_cueName.ComparisonIndex || soundName.Number != g_cueName.Number) return;
    const float pitch = *reinterpret_cast<const float*>(p + g_offPitch);
    if (pitch <= 1.05f || pitch >= 1.2f) return;  // rejects the climb play at 0.9
    void* wco = *reinterpret_cast<void* const*>(p + g_offWco);
    void* local = coop::players::Registry::Get().Local();
    if (!local || wco != local) return;

    ue_wrap::FVector loc{};
    if (!ue_wrap::engine::TryGetActorLocation(local, loc)) return;   // no blip without a position
    if (!std::isfinite(loc.X) || !std::isfinite(loc.Y) || !std::isfinite(loc.Z)) return;
    coop::net::InventoryPickupPayload payload{loc.X, loc.Y, loc.Z};
    s->SendReliable(coop::net::ReliableKind::InventoryPickup, &payload, sizeof(payload));
    UE_LOGI("inventory_pickup: broadcast collect blip at (%.0f, %.0f, %.0f)",
            loc.X, loc.Y, loc.Z);
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_pocketWatchRegistered &&
        ue_wrap::script_gate::WatchName(kPocketVerb, kPocketTag, &OnPocketPre, nullptr)) {
        g_pocketWatchRegistered = true;
        UE_LOGI("inventory_pickup: watching %ls -- keyed save state publishes before pocket capture",
                kPocketVerb);
    }
    ue_wrap::script_gate::ResolvePendingNames();
    if (g_observerRegistered) return;

    if (!g_playSound2DFn) {
        // A native class, in the index from boot; a missing function would be an engine change.
        void* cls = ue_wrap::object_index::ClassByName(L"GameplayStatics");
        if (!cls) return;
        g_playSound2DFn = ue_wrap::facts::ResolveOn(kRowPlaySound2D, cls);
        if (!g_playSound2DFn) {
            UE_LOGE("inventory_pickup: GameplayStatics has no PlaySound2D -- blip sync disabled");
            g_observerRegistered = true;
            return;
        }
        g_offSound = R::FindParamOffset(g_playSound2DFn.Fn(), L"Sound");
        g_offPitch = R::FindParamOffset(g_playSound2DFn.Fn(), L"PitchMultiplier");
        g_offWco   = R::FindParamOffset(g_playSound2DFn.Fn(), L"WorldContextObject");
        if (g_offSound < 0 || g_offPitch < 0 || g_offWco < 0) {
            UE_LOGW("inventory_pickup: PlaySound2D param offsets unresolved "
                    "(Sound=%d Pitch=%d WCO=%d) -- blip sync disabled",
                    g_offSound, g_offPitch, g_offWco);
            // Permanently DISABLED (no retry): a recook changed the signature;
            // re-walking it would yield the same miss. The observer is never
            // registered in this state, so the -1 offsets are unreachable.
            g_playSound2DFn = {};
            g_observerRegistered = true;
            return;
        }
    }
    // Neither failure below clears by retrying, so each disables the lane with one line, as the
    // offsets' does above.
    g_cueName = ue_wrap::fname_utils::StringToFName(L"inventory_Cue");
    if (g_cueName.ComparisonIndex == 0) {
        UE_LOGE("inventory_pickup: the cue's name did not convert -- blip sync disabled");
        g_observerRegistered = true;
        return;
    }
    if (!GT::RegisterPostObserver(g_playSound2DFn, &OnPlaySound2DPost)) {
        UE_LOGE("inventory_pickup: POST observer registration FAILED (the observer table is full) -- "
                "blip sync disabled");
        g_observerRegistered = true;
        return;
    }
    g_observerRegistered = true;
    UE_LOGI("inventory_pickup: observer installed on GameplayStatics::PlaySound2D @ %p "
            "(offs Sound=%d Pitch=%d WCO=%d, cue name index=%d)",
            g_playSound2DFn.Fn(), g_offSound, g_offPitch, g_offWco, g_cueName.ComparisonIndex);
}

void OnReliable(const coop::net::InventoryPickupPayload& payload) {
    if (!GT::IsGameThread()) { UE_LOGW("inventory_pickup: OnReliable off-game-thread -- dropping"); return; }
    if (!std::isfinite(payload.x) || !std::isfinite(payload.y) || !std::isfinite(payload.z)) return;
    void* worldCtx = coop::players::Registry::Get().Local();
    if (!worldCtx) return;  // no local pawn yet -> no world to play in
    coop::prop_sound::PlayInventoryBlipAt(worldCtx, ue_wrap::FVector{payload.x, payload.y, payload.z});
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    // The observer stays registered; it self-gates on a connected session.
}

}  // namespace coop::inventory_pickup_sync
