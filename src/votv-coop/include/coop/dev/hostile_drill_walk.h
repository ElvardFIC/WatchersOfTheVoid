// coop/dev/hostile_drill_walk.h -- the receive boundary's A70 drill: an attacker-chosen class name
// must not cost the host a walk of the whole object array. Two arms, both judged by the HOST reading
// its own whole-array walk count (ue_wrap::walk_census) across a sentinel the attack rides behind:
//
//   record_bad_classes -- a client sends ONE PropSaveDataIntent for a real covered prop's key, whose
//     `classes` group carries ~1024 distinct unknown names. On bytes without the fix each name is a
//     FindClass miss and a whole-array walk; with it each is one object_index lookup. The host's walk
//     count rises by about the name count on the red bytes and by zero on the green. A sentinel spawn
//     rides the same Bulk FIFO behind the intent, so its binding means the intent drained and applied.
//   bad_alarm -- a client sends a burst of AlarmState while the alarm trigger is unbound, which
//     re-walks for trigger_alarm_C per message on the red bytes. A fresh world usually has the trigger
//     BOUND, where no walk is possible either way, so this arm FAILs on an observed walk (red) and is
//     INCONCLUSIVE otherwise (the alarm fix is then proven by code; record_bad_classes carries the HIGH).
//
// This is defensive testing of OUR OWN mod, in OUR OWN rig, per coop/dev/hostile_drill.h. Nothing
// here ships enabled. The judge reads nothing of the fix: it counts walks, a counter the tree already
// keeps, so the same judge reads red on the commit before the fix and green on it.

#pragma once

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill::walk {

// The two A70 sub-arms; None when the active drill arm is not one of ours.
enum class Arm { None, RecordBadClasses, BadAlarm };

// Map the hostile_drill enum string to a walk arm (else None), and its name for the markers.
Arm        ParseArm(const char* enumValue);
const char* ArmName(Arm a);

// Client (sender), game thread: once this peer's own join is over, craft and send the attack and its
// sentinel, once. A no-op after it has sent (or reported it could not stage).
void TickClient(coop::net::Session* s, Arm a);

// Host (judge), game thread: baseline the walk count on the first world-ready slot, then read the
// delta when the sentinel lands and print the PASS / FAIL / INCONCLUSIVE marker.
void TickHostJudge(coop::net::Session* s, Arm a);

// Back to the un-armed state (disconnect / re-arm).
void OnDisconnect();

}  // namespace coop::dev::hostile_drill::walk
