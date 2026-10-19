## 0.9.2152

- Improve wisp capture animation readiness on host and clients with bounded retries. Check native montage playback before treating the animation as started, and guard against recycled actors or animation instances.
- Bind wisp capture and death to the original victim. Protect hold movement and release against disconnects, pawn replacement and nested cleanup.
- Synchronize full NPC orientation (pitch, yaw and roll), with shortest-arc interpolation and immediate placement on first samples or teleports.
- Extend spawn detection to the Fossil Hound and yellow-wisp spawners. Correct the possessed boar's classification so its physical body uses prop synchronization instead of the Character path.
- Replicate host-driven possessed-boar body movement and route bites against remote players to the correct player. Protect other players' health from duplicate replica damage, including adopted save replicas after movement handoffs.
- Feed replicated boar movement into the native client animation blueprint's velocity input for locomotion and movement-loop audio. Preserve native physics and host behavior.

Includes the 2151 synchronization fixes. Every player needs this exact **0.9.2152** package and **Voices of the Void 0.9.0n**. This package contains **DLL b2153 / network protocol 2153** for the expanded NPC pose format; older protocols are incompatible.

The Windows x64 Release build and nine offline regression runners are documented in RELEASE_2152.md. Two-player gameplay verification remains pending. Entity synchronization is still partial: boar attack/roar/death presentation, late-join action state and other entity-specific behavior remain unfinished.

## 0.9.2151

- Keep the insomnia demon and sleep nightmares personal to the player who meets the native game conditions. Shared sleep acceleration no longer broadcasts another player's nightmare.
- Retry remote ragdoll creation, align the physical body to the transmitted pelvis and improve get-up continuity. Use a selected model's valid native physics rig where available.
- Allow supported NPC synchronization to start before every NPC class has loaded; recognize late-loaded subclasses on their first spawn.
- Retry wisp capture presentation when its replicated actor arrives late. Wisp capture no longer selects the chair sitting pose.
- Synchronize Cremator door poses and lever presentation through the host. Cache actor searches and keep replica lever updates from consuming or burning contents.
- Replicate held physics-gun beam endpoints and gravity-gun cosmetic effects, with stale-effect cleanup.
- Discover custom skins in nested directories. Hide failed model previews and retry loading instead of showing the previous model under a different name.

All players need this exact 0.9.2151 package and Voices of the Void 0.9.0n.
Release build and focused offline checks are documented in RELEASE_2151.md.
Two-player gameplay and the reported door FPS symptom still need in-game confirmation.
Missing model assets and static preview mismatches remain unverified without model names.

## 0.9.2150

- Renamed the fork to Watchers of the Void, authored and maintained by ElvardFIC, with a new supplied cover. Original Multivoid credit remains with Pelmentor.

- Consolidates all accumulated Plus changes since 2146, including development work through 2192, into one release numbered 2150.
- Preserves partially used item records through container extraction and pocket capture; retries failed birth/save delivery without stale retries overwriting newer state. Adds identity and lifecycle checks before removing replicated objects.
- Adds host-mediated anchored-hook retrieval, MRE opening, prop freeing/debris motion, collision-push handling and ordinary physics-gun prop movement. Prevents ATV carry/mount overlap and improves newly created companion ATV placement.
- Extends water, cement, sponge and selected-hand material state; adds supported wall placement/save tracking and client melee damage. Client wall repair remains disabled without spending cement until atomic material accounting is implemented.
- Adds shared console text/history, Madness presentation, Maxwell-family music/squeaks and supported toy audio; adds safe opening, grill state and initial crop/faucet/hose placement persistence.
- Adds common event-schedule authority, black-disk sky presentation, Mimic lifetime handling and event-location routing. Other creature/event paths remain incomplete.
- Improves client socket requests, laptop power animation/retry handling, ZIP-slot bitfield access, canned-food world/hand state and base light phase synchronization.
- Retains selected-model ragdoll, chair, extinguisher, cable-release, flashlight, equipment and container fixes; corrects the ragdoll animation mode so the old animation blueprint cannot overwrite its rig. Includes supplied skin previews and reduces repeated object searches.

All players must install this exact 2150 package. Earlier development snapshots numbered 2150 are not compatible: this release consolidates later protocol changes under the owner's requested number. Previous work-note numbers are historical identifiers, not separate releases.

Full Release build and offline regression/package checks are recorded in RELEASE_2150.md. Two-player gameplay has not been verified. Insomnia demon, ragdoll appearance, token/UI synchronization, shared computer display, ZIP ejection and disappearing workbench items remain open reports; related code changes do not prove those symptoms resolved. Full remaining work is in agent/TODO_2150.md in the source workspace.

## 0.9.2146

- Fixed malformed empty equipment slots that could make clients see "Not enough space" when equipping any equippable item, including backpacks and fanny packs. Empty slots from older player profiles are repaired when those profiles load.
- Refresh the open shared-container item list after another player's contents update, including its selection and action buttons.
- Fixed a missing item-state broadcast when the host registered an object before its saved charge/uses were restored. Already registered objects now send their final save data after spawning, without announcing a duplicate spawn. This addresses one cause of batteries and other consumables appearing full on the other peer.

All players need b2146. Includes b2143–2145 changes. Focused offline checks pass; two-player testing is still required, especially all inventory/hand/container routes for consumables. Container extraction into the inventory has a separate native ordering issue still under investigation.

## 0.9.2145

- Fixed remote hand-held fire extinguishers missing their foam and spray audio. The effect follows the other player's aim, stops when empty, and waits for the correct held-item display to exist.
- Fixed unplugged cable ends remaining suspended after another player lets go. Release is delivered reliably, stale movement packets cannot restart a finished grab, and interrupted streams recover automatically.
- Fixed other players' flashlights remaining on after their batteries run out. Automatic shutdown and unequipping now update the remote light, with lightweight state checks and change-only messages.

All players need b2145. Includes the b2144 selected-model ragdoll and chair changes. Focused offline checks pass; two-player in-game verification is still required. General item-charge persistence through inventory remains under investigation.

## 0.9.2144

- Remote ragdolls now keep the selected Kel, Kerfur or compatible custom player model and its materials. Replaced the plushie's physics override with the complete 17-body rig, instead of the model's simplified two-body collision asset.
- Validate the selected model's skeleton and physical bones before starting ragdoll simulation. Removed the incompatible animation setup and added object-lifetime and initialization checks. Unsupported models remain visible instead of turning into a substitute plushie.
- Added synchronization of the chair's rotation while seated and when standing up, with periodic updates for joining players. ATV seats keep their separate synchronization.

All players need b2144. Focused automated checks pass; selected-model ragdolls and chair rotation still need two-player in-game testing. Limb motion is simulated locally rather than streamed bone by bone.

## 0.9.2143

- Integrated the 2142RBE source changes while retaining the earlier food-state and client microstutter fixes.
- Fixed inherited damage and saw events being missed by the host when a client hits a prop. Pending hits are cleared when leaving or changing worlds.
- Added protection against duplicate client props after R + left click: one placement request per actor, no competing spawn announcement, and the host's reply binds to the original local object.
- Synchronized the drone's rotor sound pitch with the host, including both nearby and distant audio loops.
- Restored the compatible native ragdoll body instead of applying a player skin with an incompatible skeleton. Ragdolls currently use that fallback body rather than the selected skin.
- Preserved food condition during inventory placement and corrected held-food join snapshots.

All players need b2143. Targeted automated checks and a Release build pass; these fixes still need testing together in game. Chair rotation and the remaining test-log items are tracked for follow-up.

## 0.9.2142

- Fixed partially eaten food becoming full again after a client takes it into their hand or inventory and places it back in the world. Remaining bites, temperature and ripeness now travel with the placement.
- Ordered food updates with item spawning and removal, and refreshed food state after birth/snapshot messages to prevent stale or missing updates.
- Retains the client microstutter fixes from b2141, now confirmed working by the user.

Everyone needs b2142. Please test the food fix together in game.

## 0.9.2141

- Reduced repeated client-side searches for power cords and sockets. Incoming connection updates are now grouped before applying them, avoiding repeated expensive identity reads for every connection.
- Replaced full game-object scans in multimedia synchronization with indexed MediaPlayer lookups, and fixed temporary string allocations that were not being freed.
- Added timing diagnostics for Plus synchronization features to help investigate remaining frame-time spikes. These fixes target likely sources of the reported client microstutters; the affected player's in-game result still needs confirmation.

Everyone needs b2141. Includes the fire and explosion changes from previous builds.

## 0.9.2140 (test build)

- Expanded explosion synchronization beyond gas cans to gameplay blasts from bombs, grenades, mines, rockets, explosive props and world events, including explosions from items held in the inventory hand.
- Shared blasts preserve their position, rotation, scale, damage, fire and camera effects. The host controls world damage and chain reactions; each player receives their own local effects.
- Added duplicate protection, ordered bomb removal, retries and safeguards for reconnects and changing held items.

Everyone needs b2140. Multiplayer testing is required. Random debris trajectories and client physics impulses are not mirrored. Private laptop-game explosions and the separate spore/story-object spawners are outside this blast channel. Includes b2137/b2138 fire synchronization; fire is still not serialized into stowed inventory records.

## 0.9.2139 (test build)

- Added shared explosions for gas cans placed in the world, including gas-can chain reactions. Client breaks are validated and executed by the host; each peer plays the native blast effects and handles its own player's exposure.
- Kept world damage and fire spread on the host to prevent duplicated chain reactions. Added protection against duplicate blasts, stale requests, item replacement and delayed sends.
- Fixed the ordering between a client's gas-can break request and the source item's destruction. Joining players do not replay past explosions.

Everyone needs b2139. Multiplayer testing is required. This first explosion pass covers world gas cans; other explosives and gas cans exploding while selected in the inventory hand are still pending. Random burning debris and explosion physics impulses are not replayed on clients. Includes the physical-item and selected-hand fire changes from b2137/b2138; fire is still not serialized into stowed inventory records.

## 0.9.2138 (test build)

- Added shared fire effects and native contact ignition for the item currently selected in a player's inventory hand.
- Switching or stowing the item removes its remote fire. Delayed updates cannot ignite the next item, including a replacement of the same type.
- Fixed hand-state send retries, stale mirror identity and fire cleanup when a player leaves or changes items. Remote held items wait for the player's position before appearing.
- Prevented duplicate fire-spread requests from held-item copies and fixed a timing edge case that could prematurely expire a newly received fire update.
- Preserved burning when a client releases a surviving held item back into the shared world.

Everyone needs b2138. Multiplayer testing is required. Explosions are still pending. This update mirrors the currently burning hand actor; it does not store fire in inventory records or restore it when the game recreates a stowed item without fire.

## 0.9.2137 (test build)

- Added shared burning and extinguishing for physical world items with a shared key, including items held with the physics grab. Uses the game's own fire effects and contact ignition.
- The host controls item fire damage and spread; burning fuel and extinguished state are refreshed for other players and late joiners.
- Added request validation and protection against stale updates, repeated ignition, delayed sends and session changes.

Everyone needs b2137. Multiplayer testing is required. This is a partial fire update: explosions and fire on items selected in the inventory hand are still pending. Fire state is not saved into stowed inventory items.

## 0.9.2136

Well, I lied to you guys — I've still got tokens because payday came around. I FIXED THE LIFT, FINALLY, OH MY GOD. God bless it.

Mattresses are shared now, so no separate sleeping spot for each of you. Deal with it.

Fixed bugs here and there, and playing should be more fun now. I still haven't synchronized explosions or item states like whether something is burning, though — that's for later. Enjoy the new version. Peace, people!

## 0.9.2135 (test build)

- Synchronized the delivery drone's "Stay indefinitely / Leave after 5 minutes" setting. Client changes are checked by the host, and waiting drones periodically refresh their shared state.
- Added synchronization of saved text on editable paper notes and notebooks, including text saved when closing the editor. Uses the existing item save data for reconnects and inventory transfers.
- Hardened pending console requests and note updates against failed sends, replaced objects and session changes.

Everyone needs b2135. Multiplayer verification is still required. Reopen a note to see incoming edits; live collaborative editing, drawings and nametags are outside this change. Includes the b2134 garage lift changes.

## 0.9.2134 (test build)

- Added garage cargo lift synchronization: clients send button requests to the host, and everyone receives the physical platform's position, direction and movement state.
- Added smooth platform updates, late-join recovery and protection against repeated requests, stale updates, replaced objects and session teardown.

Everyone needs b2134. Multiplayer verification is still required, especially riding the lift together, carrying objects and obstruction handling.

## 0.9.2133 (test build)

- Added synchronization of the coordinate triangle's scan progress, including its outer lines, inner lines and circle. Observers receive the display state without starting another signal calculation.
- Fixed coordinate snapshots being rejected when joining an idle or client-operated desk, and retained updates until the interface is ready.
- Hardened interface reconstruction, world changes, player departure and stale updates. A failed coordinate repaint now refuses a ping verdict instead of using old coordinates.

Everyone needs b2133. Multiplayer verification is still required; see the combined test checklist. Garage lift, shared physical fire/explosions, writable item text, basement walls and the remaining ATV/drone tasks are still pending.

## 0.9.2132 (test build)

- Fixed a missing network identity when unfolding floor mattresses and sleeping bags, which could make them disappear for other players. Fresh floor beds now receive their native save key before synchronization.
- Protected the host's pending spawn queue against deleted or replaced objects, world changes and nested game callbacks.

Everyone needs b2132. Multiplayer testing is still required. The coordinate triangle has been investigated; its presentation fix and the other pending gameplay tasks are not included yet.

## 0.9.2131 (test build)

- Reviving now extinguishes the player's remaining fire state, so its delayed pain sounds and camera movement can stop normally.
- Prevented remote player models from running local-only fire and death logic. This addresses a path that can hide the model after an explosion and affect the observer's camera; the existing remote ragdoll display remains in use.

Everyone needs b2131. These changes require multiplayer testing. Floor mattresses, the garage lift, shared fire/explosions, the coordinate triangle, writable notes and basement walls remain pending.

## 0.9.2130 (test build)

- Fixed item save-data queue errors during nested game callbacks, including repeated application and newer records being removed by an older update.
- Fixed pending item spawns inheriting the identity of a deleted object, and protected queued births during callbacks and world changes.
- Hardened record resend handling when a player leaves or the world disconnects.

Everyone needs b2130. Multiplayer testing is still required; these fixes do not establish that every reported item-sync issue is resolved.

## 0.9.2129 (test build)

- World downloads now compress the save before sending it and verify the restored data before loading. Two tested saves shrank by about 28 times; actual download time still depends on the connection.
- Added the separate synchronization paths for placing a mattress on a bunk bed and rolling it back up.
- Strengthened transfer-header/chunk validation and fixed concurrent CRC table initialization.
- Follow-up audit: reject incomplete save reads and failed final writes, handle receive-buffer allocation failures, and refuse an unavailable save directory.

Everyone needs b2129. Offline checks passed; the new multiplayer behavior still needs testing.

## 0.9.2128 (test build)

- Adjusted world-transfer pacing so normal network latency no longer prevents the download speed from increasing. Congestion limits remain in place.
- Remote light-switch clicks now fade with distance instead of playing across the map.
- Workbench items crafted by a client now enter the existing item synchronization path immediately, without needing to be picked up first.
- Client mattress and sleeping-bag unfolding/folding now use the same item synchronization path.
- World-created and container-extracted items take priority over device-ejection rules, avoiding inappropriate physics sleep or device-denial handling for modules and drives.

Use b2128 on every peer. These fixes passed offline checks; multiplayer verification is still needed.

## 0.9.2127

So, boxes don't lag anymore, music now syncs as long as you all have the **SAME SONGS** in your "radio" folder, and the module compartment covers open for everyone now, yay! The transformer repair cover opens for both players too. Have fun, friends! I'm off to play with my friends too.

## 0.9.2126 (test build)

Everyone in one session needs b2126. Includes b2125's shared local radio music.

**Gravity gun corrections.** Live local gun holds now take priority over old incoming held poses, releases and spawn reconciliation. Continuous force observation no longer evaluates the native force parameters twice; one-shot impulses still wake parked bodies before the shot. Fixed an out-of-bounds read in the native hook when the physics query returns a one-byte boolean. These address concrete defects found while investigating the reported freeze and one-way motion; reproducing those symptoms with two players is still required before calling them resolved.

**Computer covers.** A client can bring a hand-held cover to its socket: the client releases its physics handle and requests insertion of that specific part. The host checks the socket, distance and ownership, closes the old held stream and consumes the cover once. Short overlap/animation races retry; another player's held cover cannot be consumed by the request. Added the missing native top-right cover return path. Remote cover closure follows the shared state; the host runs the native magnet animation where available.

**Transformer repair door.** The yellow button's opening/closing is now shared through the existing power-grid channel. Client prediction, busy-animation retries, duplicate protection and late-join state are included. Failed state sends and join replies stay pending.

Production-controller tests, native Blueprint checks and a guard-page test passed, with deliberate fault controls. Full multiplayer gameplay verification remains pending.

## 0.9.2125 (test build)

Everyone in one session needs b2125. Added shared local-file music for the radio's track-selection interface: any player can select a song, play, pause or skip it. Playback mode and approximate position follow the host. Everyone supplies the same filenames and subfolders in their own `radio` folder; installation paths may differ. Music files are not transferred. Cyrillic filenames are supported.

Requests retry during loading, duplicate clicks are fenced, and stale host updates cannot erase a pending local command. Late joiners receive the current track and pause state. Local radio players are separated from the generic TV/media channel. Volume remains local. Internet URLs retain their older behavior; the ATV and legacy random-music radio are outside this change.

Offline controller, cooked Blueprint and regression checks passed. Two-player radio testing is still required, especially track endings and multiple radios. The latest gameplay report confirms boxes and coordinate tower minigames work, and compartment covers open. Gravity-gun freezes/one-way motion, client cover return and the transformer's yellow-button door remain pending; this build does not claim those fixes.

## 0.9.2124 (test build)

Everyone in one session needs b2124. The previous gameplay test confirmed outlet connections, coffee maker operation, microwave startup and PVP. This build addresses the reported failures; its multiplayer gameplay test is still pending.

**Gravity gun.** Added the native attraction and impulse paths to the existing continuous held/release/coast stream, including shots without a held target. A brief shot survives a slow network tick; parked network props can be grabbed again. Switching targets releases the previous prop.

**Microwave and mini-oven lids.** The kitchen channel now owns child lids consistently. Clients send actual mouse movement to the host, which runs the native hinge torque and sends the door's relative pose. This addresses the sound/light-only opening, conflicting lid streams and collision with a door that remained closed remotely. Automatic client closes do not echo back, and simultaneous drags have one owner. Engine physics and FPS still need in-game verification.

**Standing on boxes.** Remote player bodies retain query collision for traces while no longer physically pushing boxes through their delayed walking/jumping pose.

**Main computer covers.** Added shared screw and cover state for the four module compartments. Removal and return run on the host. A cover removed by a client drops beside that player for pickup; it is not automatically placed in their hand. Repeated requests cannot spawn extra covers, and a held cover is not consumed before release.

**Radio tower.** Added the separate radio tower's six fuses, 2x2 through 5x5 light puzzle, reset/confirm levers and repair state. Transformer puzzles and the three coordinate radar towers already had synchronization in upstream MultiVoid and keep their existing systems. The new channel includes late-join state, ordered retries and refunds for conflicting fuse inserts.

Several code-review passes, cooked Blueprint checks and offline regression tests passed, including ten deliberate fault controls. These checks do not replace a two-player test.

## 0.9.2123 (test build)

Everyone in one session needs b2123. Includes b2121/b2122; multiplayer gameplay verification is still pending.

**Cord fixes.** A client connection request no longer runs local power callbacks or turns into an unintended disconnect when the local socket appears occupied. Switching sockets guards the actual delayed Blueprint reset instead of relying on a wall-clock delay. The state cache keeps one occupant per socket and expires obsolete rows. Save reconstruction remains allowed; user disconnects are recognized without a pre/post nesting counter. Component/socket names compare with Unreal's case-insensitive name semantics.

**Physics fixes.** Live local hand/gravity-gun holds now take priority over support/coast streams, with physics restored for the local grab. Old or duplicate end messages and obsolete pending ends cannot rewind the prop. A quick grab/drop opens a new generation. Recycled actor slots are checked by serial on the receiver too. Stationary heartbeats no longer prevent a coast from ending at low frame rates. Movement-base field lookup accepts different FName display casing.

Offline regression tests exercise the production host and receiver, including deliberately broken variants. This does not replace the combined two-player test.

## 0.9.2122 (test build)

Everyone in one session needs b2122. This includes the b2121 kitchen/gravity-gun changes. Two-player gameplay verification is still pending.

**Cords and outlets.** Added host-authored plug/unplug state, connected socket power, client requests and late-join replay. Child plugs/sockets use their parent and component name; level sockets use their placed identity. The native connection verbs update the plug attachment, cable constraint and appliance callbacks. The host checks reach, cable length and socket occupancy. Switching sockets waits for the game's delayed detach to finish. The temporary hand-drag animation remains local.

**Standing on props.** A player's current physics prop underfoot now keeps the existing host physics stream alive, including a client stepping on a box that nobody grabbed. Leaving, disconnecting or taking the object into a hand ends this support lease. ATV, static/frozen props, trash and held items are excluded. This does not add general synchronization for every loose physics object in the map.

**Physics stream fixes.** Stationary driven props periodically resend their pose, so a lost packet or late join can recover. A failed reliable end send is retried. A recycled actor address cannot reuse another object's drive entry.

## 0.9.2121 (test build)

Everyone in one session needs b2121. Compilation and wire-routing checks passed; a two-player gameplay test is still required.

**Gravity gun.** The held-prop stream now reads the gun component's actual `obj` field. Beam-held props use the existing pose/release/coast path, with the ATV exclusion preserved.

**Kitchen machines.** Added host-authored state and client button intents for the microwave, mini-oven, coffee maker and coffee vending machine. Microwave timer/run state, oven mode/heat, cup/coffee state and vending busy state have dedicated fields. Cooking, ingredient consumption, cup creation and credit charges run on the host; clients paint the result. Unkeyed child lids use the parent's identity; keyed lids keep ContainerState. Late join and actor-loading retries are included.

Power comes from the host's cord state. Outlet/plug synchronization remains a separate unfinished task. Confirm ingredient insertion, lids, completion, one cup/one charge, gravity-gun throws and late join with two players before calling this build verified.

## 0.9.2120

Everyone in one session needs this build. The menu reads Multivoid+ 0.9.0n b2120. An older build cannot join.

**Base.** F1, Player, Debug: "Teleport to base". It presses the game's own key, numpad decimal (the "."), the action named debugtp. Numpad 0 is the cheat menu, not this. Only you move.

**PVP.** After a hit from another player you cannot take another for 3 seconds, including while you are on the ground. One swing cannot be spammed into a kill. An explosion is not part of this pause.

**Events.** A world event that one player starts, including the vent break in the control room, now runs on every machine. The others are moved into that room first, then the event runs. Walking into the same spot after that does not start it a second time. A ship or a pyramid still brings people without starting a second copy. A personal scare still stays on the player who walked into it.

**PVP.** The nickname flag was already shared. A swing hits a player standing in front of where you look, within about three metres. Both of you still need PVP on. An explosion still hits everyone.

**Scooters.** A save has one quadbike. The host places a second one beside it, and both are visible to everyone, so two people can drive. The driver sends where the scooter is. The others follow that. A parked scooter is sent by the host. Gas, a repair tool, a sponge, and a tyre reach the other machines, but only as an improvement, and only from someone standing at the scooter. Sitting on it plays the Omega sit pose. The model rises one and a half heads and moves three heads forward. The player stays where the game put them, so standing up does not start inside the floor. A Kerfur Omega on a scooter is shown sitting, not standing on the seat.

**Chairs.** The same sit pose. The model drops by half the kerfur's height, then another two and a half heads, and moves four heads forward. The other players see you on that seat. Standing up still starts from the capsule.

**Models.** F1, Cosmetics, Skins. The folder is `LogicMods\multimodel`. A pak left in `%LOCALAPPDATA%\MultiModel\Models` is copied there on the next start. Only the model name is sent. A player who does not have that file sees Kel. On the same page, "Always see playermodel" (on by default) keeps your own body visible when you look down or use spectator, including during an event. Only your screen changes. A faint or a death still hides the standing body, because the ragdoll is the body then.

**Events.** The host runs them. A client does not roll its own list, so the same event does not start twice. If the event only makes sense in one place (the locked complex, with the garage as the way out, the obelisk, the pyramid, the wisps, and a few others of that kind), everyone is brought to the player who triggered it. A personal scare stays on that player. Opening the fridge, and a ship that only flies over the map, do not pull anyone. Creatures and ships the event spawns show up for clients too. The F1 Events tab is the catalog, not a "playing now" line.

The scooter's map screen, the drone's bag, and the desk's rear panels still do not sync.
