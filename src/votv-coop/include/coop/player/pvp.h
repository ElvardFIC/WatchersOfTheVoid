// coop/player/pvp.h -- optional hits between players.
//
// A swing or a throw hurts only when BOTH peers have PVP on. An explosion (a pipe bomb, a gas
// can, anything whose class is an explosion) still hurts everyone. The victim's machine applies
// the damage, the same way every other hit does. Game thread for the wire; the impact filter is
// safe from the worker that dispatches the hit.

#pragma once

#include <cstdint>

namespace coop::net { class Session; struct PvpHitPayload; }

namespace coop::pvp {

void SetInitialLocal(bool on);
bool LocalOn();
void RequestLocal(bool on);

void StoreForSlot(int slot, bool on);
bool OnForSlot(int slot);

void Install(coop::net::Session* session);
// Retry the fire-button watch until mainPlayer_C has loaded. Game thread.
void Tick();
void OnSlotDisconnected(int slot);

// A prop just left a player's hand fast enough to be a throw. `nonce` is the same number on every
// machine that sees that let-go, so the hit lands once. `explosive` is a bomb or a gas can: those
// still hurt a player who has PVP off.
uint16_t MintThrowNonce();
void NoteThrow(void* actor, uint8_t slot, uint16_t nonce);

// The local pawn and the item in its hand, published so a swing's impact can name them.
void NoteBodies(void* localPawn, void* held);

// True if this impact on the LOCAL player should be swallowed. Explosions pass. A recent throw
// passes only when both peers have PVP on.
bool CancelLocalImpact(void* params, void* function);

// The body on a puppet is always cancelled by the caller (it would write this machine's health).
// A swing of the local player's held item, with both PVP flags on, is sent to that puppet's owner.
void OnPuppetImpact(void* puppet, void* params, void* function);

// Host: a swing from `senderSlot`. Victim: apply it.
void OnHit(const coop::net::PvpHitPayload& payload, uint8_t senderSlot);

}  // namespace coop::pvp
