#pragma once

namespace coop::net { class Session; }
namespace ue_wrap::script_gate { struct Call; }

// Local recovery for the native world-container extract/pocket sequence. This
// does not arbitrate another peer's debit or persist debt across world changes.
namespace coop::container_extract_recovery {
enum class RestoreResult { Unrelated, Restored, Failed, Invalidated };
void Install(net::Session* session);
void OnDisconnect();
bool InFlight();
bool Pending(void* actor);
RestoreResult RestoreForPickup(const ue_wrap::script_gate::Call& call, void* actor);
void OnPocketPost(const ue_wrap::script_gate::Call& call);
}
