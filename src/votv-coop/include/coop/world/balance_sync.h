// coop/world/balance_sync.h -- shared host-authoritative credit balance (saveSlot.Points).
//
// The host polls Points on the game thread and sends its absolute value to each peer on
// change, join and order refusal. Only an accepted send clears that recipient's debt.
// Clients retry both the save write and the native zero-credit HUD repaint; a newer value
// arriving during repaint remains pending. World/session/occupant changes invalidate delivery.
// The wire is host-to-client only. Purchases use OrderRequest: the client supplies catalog
// rows and the host prices and charges them through coop/items/order_sync.

#pragma once

#include <cstdint>

namespace coop::net { class Session; }

namespace coop::balance_sync {

void SetSession(coop::net::Session* session);

// Per-tick (net_pump game thread): on the HOST, poll Points + broadcast BalanceSync on
// change; on a CLIENT, retry applying the latest pending host balance until the saveSlot
// resolves (a connect-edge sync can arrive before the gamemode loads). No-op when not
// connected.
void Tick();

// HOST: send the current balance to ONE slot, or retain it for retry. Two callers, one reason -- the
// change-polled broadcast in Tick() fires only when the host's Points MOVES, so anything that needs
// a specific peer to converge while the value is STATIC has to say so directly:
//   * the connect edge, since a joiner's balance would otherwise stay wrong until the host next
//     spent or earned something;
//   * an order REFUSAL, because the host's balance does not move when it refuses, yet the client
//     has already debited itself: the laptop's order button calls `lib_C::addPoints(-price)` before
//     the order is ever forwarded, and that call is EX_LocalVirtualFunction, so it cannot be
//     suppressed. Without this send the phantom debit persists until the host's balance happens to
//     change.
// The name says the ACTION rather than one trigger, because naming a trigger is how the refusal
// path came to be designed around a second function that did the same thing.
void SendCurrentToSlot(int slot);

// RECEIVER (client): take the host's ABSOLUTE balance for saveSlot.Points; the host refuses a
// client's copy at its receive edge. The write itself is Tick's, on the game thread: this stashes
// the value, and Tick retries the write until the saveSlot resolves.
void ApplyFromHost(int32_t total);

// Apply a LOCAL credit (the +1000 dev button) on the host or solo, through AddPoints. On a
// connected CLIENT this REFUSES and logs: there is no client-to-host economy write at all. Safe
// from the render thread (the menu).
void CreditLocal(int32_t amount);

// Reset the broadcast dedup on session teardown (so a reconnect re-broadcasts).
void OnDisconnect();

}  // namespace coop::balance_sync
