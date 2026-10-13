# Vehicles

## Purpose

The ATV, the game's one vehicle: a five-body physics rig with a driver seat. How the peer
driving it and the peers watching them share one rig; why the host mints a second ATV so two
people can drive;
why a watching peer runs the rig natively instead of freezing it; what condition travels; and
what is still open.

## How it works

### The rig, and why a mirror is not frozen

The ATV is not an engine vehicle: its root body simulates physics and its four wheels are
separate simulating bodies held by constraints, so the vehicle's visible output is suspension
travel, and one actor transform is not the pose. The first lane froze a mirror and teleported its
root twenty times a second, dragging four constrained bodies behind it: measured suspension
travel of thirty centimetres against a native two to four, and a release that launched the other
peer's copy at a metre and a half per second. That lane was replaced whole
(`coop/interactables/atv_sync`).

A peer that does not author an ATV runs the rig natively, physics on and tick on, and is
corrected toward the authority on every packet (`coop/interactables/atv_corrector`): the
velocity is written, the position error is closed by a bounded corrective velocity, and past a
speed-scaled threshold, or when the error stops shrinking, the mirror is cut to the authority's
pose through the game's own teleport. This is MTA's vehicle shape, and the one thing a mirror may
not do is author collision damage: the seven hit delegates are intercepted on a peer that does
not own the rig's tick and dispatched with a zeroed impulse, so the notification runs whole and
only the damage dies (`coop/interactables/atv_hit_guard`). Cancelling the wheel delegates
outright cost a mirror its rig shape, a sag of twenty-five to forty centimetres, until the cause was
found.

### Two predicates

Who streams and whose machine runs the brain are different questions. Pose authority, the peer
driving or carrying the ATV, decides who streams; tick ownership, that peer or the host when
nobody authors, decides whose accumulators, wheel torque and collision damage are real. On the
wire the author is one slot and the seat another, because the driver seat is what the occupancy
deny reads, and a peer merely grabbing an ATV must
not deny a seat nobody is in. A simultaneous mount resolves to the lower slot. A peer may name only itself as author, and only the recorded author
may release; the release clears the author and nothing else, since nothing was frozen.

### The stream

While authored, the rig's pose, full rotation, linear and angular velocity stream about twenty
times a second, keyed by the ATV's save key; the host relays a client's stream. An idle ATV is synced by
the host at five hertz gated on change with a two-second keepalive floor, so a parked vehicle
costs one packet every two seconds. An ATV that appears at runtime, from the spawn menu or the
host's companion spawn, has no save twin, so the host mints a synthetic key and announces it.

A save usually places one ATV. Once that one has sat still, and the world has no second one
already, the host mints one `ATV_C` beside it and places it with the game's own vehicle teleport.
It is given its own save key when it has none, and it is included in the save, so a save and quit
writes each scooter's fuel, tires and position into that scooter's own record. A later load that
already has two does not mint a third, and a joining player who already received both in the
world save is not handed another copy.

### The driver

The driver's puppet is snapped onto `sitarrow` after the pose drive, so they sit in the saddle
instead of trailing the rig. A player who sits in a chair sends that seat's point (`SitAnchor`)
and the other machines put their puppet there. There is no second seat on the ATV.

### Condition

The author's condition rides the same payload: per-wheel durability, dirt, fixes and type, the
spare's trio, body dirt, fuel and health (`coop/interactables/atv_condition_sync`). A mirror is
overwritten and re-derives its visuals through the game's own reducers on change edges, against a
baseline seeded from the actor rather than from the last packet: a zero baseline would break all
eight constraints on a settled rig, and a per-packet one would starve the dirt reducer forever.
Accumulators apply from any legitimate author; presence, which tyres are on and whether the
spare is, is consumed only from host-authored packets, because a client-authored eject ships a
mask whose paired wheel prop cannot travel, and applying it would turn a divergence into an item
loss the host persists. The mirror's own accrual is held at zero by the impulse neuter, so an
overwrite never races an irreversible act. The battery is an inserted prop's charge and rides
the prop lane.

### Service by a peer that does not author it

A peer that does not author the ATV still runs the game's own servicing verbs on its copy when its
player uses them: the gas can's refuel, the toolbox's and the wall fixer's repair, the sponge's
wash, a tyre or the spare put on. The author's next block used to overwrite the result, so the
can was spent and the ATV kept its fuel. The copy is now measured against the author's last
write, five times a second and once more before each block lands, and its servicing movement --
fuel, health and durability up, dirt down, a tyre or the spare put on -- travels as an edit
(`AtvConditionEdit`, `coop/interactables/atv_condition_sync`): a client's to the host, which
applies it when it authors the ATV or nobody does and forwards it to the author when a client
does; the host's own to the client that authors it. A tyre put on is applied on the host's copy
whoever authors, since presence is the host's. Only the servicing directions travel and only
while the player is within reach of the ATV, so a mirror can never author damage and a drift on a
far copy is never mistaken for a service. Until the author's stream carries the result, the
servicing peer's copy keeps the serviced values (a three-second floor), so nothing flickers back.

## Who owns what

| State | Owner | Shape |
|---|---|---|
| the pose while driven or carried | the author | a twenty-hertz stream, relayed |
| the pose while idle | the host | five hertz on change, a keepalive floor |
| the driver seat | the occupancy arbiter | the lower slot wins a simultaneous mount |
| collision damage, the accumulators, wheel torque | the tick owner | a mirror's impulses are zeroed |
| the condition values | the author | overwritten on the mirror, re-derived on edges |
| tyre and spare presence | the host | consumed from host-authored packets only |
| a second ATV | the host | minted once beside the save ATV after it has sat still, and announced at once |
| a runtime ATV's identity | the host | a synthetic key, announced |

## Wire messages

| Kind | Direction | Carries |
|---|---|---|
| `AtvState` | the author, relayed; the host when idle | key, pose, rotation, velocities, the driver seat, the author, brake, lights, turbo, honk, radio, the frame modules, the condition block, an adopt flag |
| `AtvRelease` | the author | authority lost, nothing else |
| `AtvSpawn`, `AtvDestroy` | the host to all | a runtime ATV's synthetic key and birth; its teardown |

## Late join

Every indexed ATV is snapshotted to the joiner with its pose, velocity, author
and condition, so an ATV airborne at the join arrives moving and lands, and the joiner's mirror
is cut to the pose on its first packet.

## Known limits

| Limit | Evidence |
|---|---|
| While driving, the mirror trails the author: the correction is a bounded velocity bias, and the cut to the author's pose happens only past a speed-scaled error or when the error stops shrinking | `[V]` `coop/interactables/atv_corrector` |
| At an authority handoff the host becomes the idle syncer on the tick after the release, so its stream and the new author's overlap until the new author's first packet lands | `[V]` `coop/interactables/atv_sync` |
| A tyre put ON by the driver is copied as presence: the other machines show the wheel and do not receive an inventory tyre. A tyre taken off is ejected on the host from the pose stream | `[V]` `coop/interactables/atv_condition_sync` |
| The pose stream rides the reliable lane; a sequenced unreliable stream is the intended shape | `[V]` `coop/interactables/atv_sync` sends it reliable |
| A radio station that is a file on the driver's machine does not play on a machine that does not have that file. The playing flag and a text url do travel | `[V]` `coop/interactables/atv_sync` (`AtvRadio`); `ue_wrap/devices/atv` reads the radio's MediaPlayer |
| A non-author's refuel, repair or wash is measured, not intercepted: it travels as the movement it left on the copy | `[RD]` `coop/interactables/atv_condition_sync`; built, not yet played |

## Code map

| Concept | Files |
|---|---|
| the lane | `coop/interactables/atv_sync`, `coop/interactables/atv_sync_internal.h` |
| the driver in the saddle | `coop/player/sit_pose`, `ue_wrap/actors/sit_pose` |
| the corrector and the hit guard | `coop/interactables/atv_corrector`, `coop/interactables/atv_hit_guard` |
| condition | `coop/interactables/atv_condition_sync`, `ue_wrap/devices/atv_condition` |
| the engine wrapper | `ue_wrap/devices/atv` |
| the driver seat | `coop/interactables/device_occupancy` |
| tests and probes | `coop/dev/atv_probe` (the drive arm), `coop/dev/atv_tire_probe`, `coop/dev/atv_eject_drill`; the smoke run with the ATV scenario enabled |
