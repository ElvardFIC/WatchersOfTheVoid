// coop/dev/hostile_drill_explosion.h -- the explosion_* drill arms, proving the host refuses what a client's shot and
// blast requests may not do, and counts each refusal under its own reason. Defensive testing of OUR OWN mod, per
// coop/dev/hostile_drill.h; nothing here ships enabled.
//
// A joined client forges ProjectileFired and ExplosionRequest messages through the session's own send, bypassing its
// own request lane, and the HOST judges by accounting: every message is exactly one accept or one refusal, so the sum
// of its counter deltas since its baseline equals the messages sent, and each reason's delta matches the arm's table.
//   explosion_malformed  explosion_duplicate    explosion_unknown_shot  explosion_consumed
//   explosion_no_launcher  explosion_origin      explosion_reach         explosion_not_seated
//   explosion_no_explosive explosion_hand_bound explosion_story         explosion_wait_behind_shot
//   explosion_not_ready  explosion_evicted      explosion_flood         explosion_rejoin
//   explosion_self_clamp
// The family acts on slot 1 only; another slot idles. explosion_rejoin runs under a third peer that stays (the rig's
// observer plus rejoin), so the session survives the first client's leave.

#pragma once

#include <string>

namespace coop::net { class Session; }

namespace coop::dev::hostile_drill::explosion {

// The sub-arm; None when the active drill arm is not ours.
enum class Arm { None, Malformed, Duplicate, UnknownShot, Consumed, NoLauncher, Origin, Reach, NotSeated,
                 NoExplosive, HandBound, Story, WaitBehindShot, NotReady, Evicted, Flood, Rejoin, SelfClamp };

// The arm a config value names; None for a value that is not one of ours.
Arm ArmOf(const std::string& value);

// The arm's name, for the verdict markers.
const char* ArmName(Arm a);

// Client: the not_ready arm holds this client's world-ready announce, once per session.
void Install(coop::net::Session* s, Arm a);

// Client (sender), game thread: once the host says it is ready, send the arm's forged messages.
void TickClient(coop::net::Session* s, Arm a);

// Host (judge), game thread: prepare, say ready, then judge by accounting and print PASS / FAIL / INCONCLUSIVE.
void TickHostJudge(coop::net::Session* s, Arm a);

// Back to the un-armed state, putting back what the arm wrote (disconnect / re-arm).
void OnDisconnect();

}  // namespace coop::dev::hostile_drill::explosion
