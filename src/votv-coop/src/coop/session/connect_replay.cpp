#include "ue_wrap/core/gc_pin.h"
#include "coop/props/trash_mirror.h"
#include "coop/element/object_scan_hub.h"  // the shared pass over the object index
#include "coop/world/credit_intent.h"
#include "coop/interactables/desk_visual_sync.h"
#include "coop/interactables/desk_cursor_sync.h"
#include "coop/interactables/desk_ping_sync.h"
#include "coop/interactables/deck_play_sync.h"  // deck playback lane
#include "coop/interactables/desk_sim_sync.h"
#include "coop/interactables/tape_caddy_sync.h"
#include "coop/interactables/notebook_sync.h"
#include "coop/interactables/laptop_ui_sync.h"
#include "coop/interactables/laptop_buffer_sync.h"  // the PC buffer quad lane
#include "coop/interactables/sleepingbag_sync.h"  // client fold/unfold verbs run on the host's source prop
#include "coop/interactables/floppy_slot_entry.h"
#include "coop/interactables/mirror_slot_entry.h"
#include "coop/props/container_birth.h"
#include "coop/props/prop_key_index.h"  // Begin/End at the gate hold
#include "coop/props/container_view_close.h"  // a view into a container ends where the player's reach does
#include "coop/props/prop_food_state.h"  // the food family's live state and who authors it
#include "coop/props/prop_food_sync.h"
#include "coop/props/notebook_refresh.h"
#include "coop/props/bucket_sync.h"
#include "coop/props/dingus_music_sync.h"
#include "coop/props/llama_sync.h"
#include "coop/props/watoy_sync.h"
#include "coop/props/sponge_effects_sync.h"
#include "coop/props/water_pour_sync.h"
#include "coop/props/prop_record_refresh.h"  // a drive box or reel case republishes its record in place
#include "coop/player/movement_ledger.h"
#include "coop/player/run_end_travel.h"
#include "coop/player/local_body.h"  // skins: local first-person body owner
#include "coop/player/sit_pose.h"
#include "coop/player/nameplate.h"  // plate-pref session wiring (Install)
#include "coop/player/pvp.h"
#include "coop/player/nick_color.h"  // nick-color session wiring (Install)
#include "coop/player/puppet_identity.h"  // a puppet's touch never runs this machine's player's effects
#include "coop/player/sleep_rest.h"
#include "coop/interactables/shared_screen_sync.h"
#include "coop/creatures/wisp_attack_sync.h"  // Killer Wisp coop: host detect + neutralize + relay
#include "coop/creatures/wisp_grab_hold.h"  // Killer Wisp: grab-window body placement (per-slot/full teardown)
#include "coop/creatures/wisp_tear_mirror.h"  // Killer Wisp coop: victim kill + tear mirror
#include "coop/session/pause_guard.h"  // coop no-pause invariant (ESC pause froze clients)
#include "coop/items/player_inventory_sync.h"  // per-player inventory (host file scaffold)
#include "coop/player/stat_orders.h"  // an admin's orders to a player's stats: the kept session, the tokens answered Left
#include "coop/element/death_seam.h"  // an element's end of play, handed to the lanes that subscribed
#include "coop/props/host_spawn_watcher.h"  // HOST mirror of those ambient spawner outputs (the pinecone scare)
#include "coop/props/prop_drop_intent.h"  // client-place -> host-auth keyed-prop DROP INTENT
#include "coop/props/remover_request.h"  // host-arbitrated hand remover requests
#include "coop/interactables/laptop_tab.h"  // per-machine laptop tab reports
#include "coop/props/prop_action_sync.h" // host-authoritative prop damage and hacksaw actions
#include "coop/props/remover_request.h"  // the tool gun's remover as a request the host performs
#include "coop/creatures/creature_hit_intent.h"  // a client's hit on a creature as an intent the host judges
#include "coop/props/prop_act_host.h"  // a client's take or use of a shared prop, judged and committed on the host
#include "coop/props/prop_act_request.h"  // a client's take or use of a shared prop, asked first and replayed on a grant
#include "coop/props/prop_spawn_authoring.h"  // a PLAYER's spawn verb vs the world's own spawns
#include "coop/creatures/kerfur_convert.h"  // host-authoritative kerfur on/off conversion (the dupe fix)
#include "coop/creatures/kerfur_command.h"  // host-authoritative kerfur menu command relay, serving the requester
#include "coop/creatures/kerfur_convert_host.h"  // the host half of the kerfur conversion: its queue, served each tick
#include "coop/creatures/kerfur_menu_input.h"  // client radial-menu verb detection (InpActEvt_use PRE -> kerfur_command relay)
#include "coop/creatures/kerfur_entity.h"  // the stable-KerfurId authority table
#include "coop/creatures/kerfur_form_assembler.h"  // script-body gate consumer (observe-only + containment counter)
#include "coop/props/prop_stick_sync.h"  // wall-attachable stick mirror (camera-on-wall)
#include "coop/dev/dev_lanes.h"  // the developer drills, probes and selftests, wired in one place
#include "coop/dev/perf_probe.h"
#include "coop/save/save_transfer.h"
#include "coop/session/join_beacon.h"
#include "coop/props/drink_spill_sync.h"
#include "coop/player/blood_sync.h"
#include "coop/interactables/atv_steering_sync.h"
#include "coop/items/coingun_sync.h"
#include "coop/items/order_sync.h"
#include "coop/world/world_sound_sync.h"
#include "coop/world/explosion_sync.h"
#include "coop/world/explosion_parking.h"
#include "coop/world/event_fire_sync.h"
#include "coop/world/event_rate.h"
#include "coop/world/event_location_sync.h"
#include "coop/world/media_sync.h"
#include "coop/interactables/kitchen_sync.h"
#include "coop/interactables/cord_sync.h"
#include "coop/props/prop_support_sync.h"
#include "coop/items/gravity_motion.h"
#include "coop/world/service_panel_sync.h"
#include "coop/world/desk_short_circuit_sync.h"
#include "coop/world/madness_sync.h"
#include "coop/world/radio_sync.h"
#include "coop/world/cargo_lift_sync.h"
#include "coop/props/prop_fire_sync.h"
#include "coop/props/explosion_sync.h"
#include "coop/props/explosion_prop_motion.h"
#include "coop/props/explosion_events.h"
#include "coop/player/hand_fire_sync.h"
#include "coop/player/hand_extinguisher_sync.h"
#include "coop/player/hand_toy_audio.h"
#include "coop/world/firefly_sync.h"
#include "coop/items/inventory_pickup_sync.h"
#include "coop/commands/command_sync.h"
#include "coop/world/ariral_ship_sync.h"
#include "coop/world/time_sync.h"
#include "coop/interactables/window_stroke_sync.h"
#include "coop/session/join_progress.h"
#include "coop/session/world_load_episode.h"
#include "coop/interactables/garbage_sync.h"
#include "coop/props/pile_look.h"
#include "coop/props/trash_channel.h"
#include "coop/player/local_streams.h"  // LastHeldActor
#include "coop/player/puppet_carry_drive.h"
#include "coop/props/trash_clump_pose_stream.h"
#include "coop/props/trash_sweep.h"
#include "coop/props/prop_drive_stream.h"  // CLIENT: park and drive those props
#include "coop/props/prop_park.h"  // the parks' held Characters, given back at session end
#include "coop/props/rider_hold.h"  // the holds on welded Characters' movement, released last
#include "coop/props/trash_collect_sync.h"
#include "coop/items/broom_stroke.h"
#include "coop/props/pack_trash_intent.h"
#include "coop/creatures/served_player.h"  // a robot the host runs for a client reads that client as its player
#include "coop/items/broom_push.h"
#include "coop/save/save_block.h"
#include "coop/save/save_button_disable.h"
#include "coop/props/grab_observer.h"
#include "coop/player/player_damage.h"
#include "coop/player/player_hit.h"
#include "coop/session/player_handshake.h"  // TickSkinConverge
#include "coop/player/skin_preview.h"
#include "coop/net/session.h"
#include "coop/creatures/npc_adoption.h"
#include "coop/creatures/kerfur_prop_adoption.h"
#include "coop/creatures/npc_mirror.h"
#include "coop/creatures/grayboar_sync.h"
#include "coop/creatures/grayboar_actions.h"
#include "coop/creatures/grayboar_turn.h"
#include "coop/creatures/kavotia_sync.h"
#include "coop/creatures/ariral_sync.h"
#include "coop/creatures/deer_sync.h"
#include "coop/creatures/centipede_sync.h"
#include "coop/creatures/mannequin_sync.h"
#include "coop/props/prop_lifecycle.h"
#include "coop/props/pile_spawn_bind.h"       // OnDisconnect: a client's own parked clumps go back to the game
#include "coop/props/prop_element_tracker.h"  // reseed hub consumer install + drain
#include "coop/props/prop_save_data.h"
#include "coop/props/remote_prop.h"
#include "coop/props/remote_prop_spawn.h"
#include "coop/props/join_membership_sweep.h"  // the join claim and sweep
#include "coop/creatures/owner_entity_sync.h"
#include "coop/creatures/npc_mirror_brain.h"  // a parked family's mirror runs none of its Blueprint graph
#include "coop/items/hook_sync.h"  // the hook lane: owner-phase stream, mirrors, the tick park
#include "coop/world/placed_save_sync.h"
#include "coop/world/garden_actions.h"
#include "ue_wrap/core/script_gate.h"  // the session's hold on the script-body gate
#include "ue_wrap/core/walk_timer.h"  // per-sync [WALK-TIME] attribution (diagnostic)
#include "coop/items/hotbar_icon_edge.h"
// coop/session/connect_replay.cpp -- the host's connect replay for one slot; declared in
// coop/session/subsystems.h, whose fan-out lists call it from the world-ready answer.

#include "coop/session/subsystems.h"

#include "coop/world/balance_sync.h"
#include "coop/interactables/comp_sync.h"
#include "coop/interactables/console_state_sync.h"
#include "coop/interactables/desk_input_sync.h"
#include "coop/interactables/physmods_sync.h"  // desk physical-modules lane
#include "coop/items/order_queue_sync.h"  // the delivery order queue, the host's, mirrored
#include "coop/interactables/drive_payload_sync.h"  // a drive's row, the host's
#include "coop/interactables/drive_sync.h"  // the drive slots
#include "coop/interactables/drive_rack_sync.h"  // rack storage
#include "coop/interactables/meadow_db_sync.h"  // meadow signal-DB mirror (multiset shadow + join seed)
#include "coop/interactables/desk_snd_fx.h"
#include "coop/interactables/dish_calib_sync.h"
#include "coop/interactables/dish_hashcode_sync.h"
#include "coop/interactables/dish_sync.h"
#include "coop/interactables/download_arm_sync.h"
#include "coop/interactables/sat_console_sync.h"  // the SAT console's shared commands run on the host
#include "coop/world/daily_task_sync.h"
#include "coop/interactables/device_occupancy.h"
#include "coop/world/email_sync.h"
#include "coop/interactables/laptop_sync.h"  // the stationary PC lane
#include "coop/interactables/laptop_tab.h"  // the tab the PC's user is on
#include "coop/interactables/portable_pc_lid.h"  // the portable PC's lid lane
#include "coop/interactables/floppy_slot_sync.h"
#include "coop/interactables/floppybox_sync.h"  // the disc crate LIFO lane
#include "coop/props/container_contents_sync.h"  // the world-container GObjStack slice
#include "coop/interactables/signal_catch_sync.h"
#include "coop/interactables/signal_sync.h"
#include "coop/player/hand_item.h"  // hotbar hand-item display axis (connect replay)
#include "coop/player/sleep_sync.h"
#include "coop/session/rig_ready.h"
#include "coop/voice/voice_chat.h"
#include "coop/save/join_window_baseline.h"  // the connect edge's removes and position corrections
#include "coop/interactables/grime_sync.h"
#include "coop/interactables/interactable_sync.h"
#include "coop/interactables/atv_sync.h"
#include "coop/interactables/drone_sync.h"
#include "coop/world/event_cue_sync.h"
#include "coop/comms/chat_sync.h"
#include "coop/interactables/upgrade_sync.h"
#include "coop/interactables/turbine_sync.h"
#include "coop/interactables/keypad_sync.h"
#include "coop/world/fuse_tower_rows.h"
#include "coop/world/power_grid.h"
#include "coop/world/power_panel.h"
#include "coop/world/sky_sync.h"
#include "coop/interactables/window_sync.h"
#include "coop/props/prop_drive_host.h"    // HOST: the props a hook drags, streamed while they move
#include "coop/interactables/verb_lanes.h"  // the interactable lanes on a device's own verbs, at the script gate
#include "coop/creatures/kerfus_lanes.h"  // the plain kerfur: its brain on the host, its state and drive, its verbs
#include "coop/props/trash_pile_sync.h"
#include "coop/player/item_activate.h"
#include "coop/net/snapshot_send_scope.h"  // the divergence deletes pass the slot's join gate
#include "coop/creatures/npc_sync.h"
#include "coop/creatures/npc_world_enum.h"  // RegisterExistingWorldNpcs
#include "coop/world/world_actor_sync.h"  // non-Character event-actor transform mirror (sibling of npc_sync)
#include "coop/creatures/piramid_sync.h"  // piramid event choreography lane (mirror brain suppression + PyramidGather)
#include "coop/player/players_registry.h"
#include "coop/props/prop_snapshot.h"
#include "coop/world/alarm_sync.h"  // base radar alarm shared-world toggle
#include "coop/interactables/serverbox_sync.h"  // host-authoritative signal-server sim state
#include "coop/interactables/server_upgrade_sync.h"  // the servers' physical upgrades
#include "coop/creatures/roach_sync.h"  // host-authoritative roach-infestation mirror
#include "coop/items/hook_anchor.h"
#include "coop/world/event_active_sync.h"  // the native activeEvents registry probe
#include "coop/world/creature_event_relay.h"  // the host's creature events, carried to every client's count
#include "coop/world/weather_sync.h"
#include "ue_wrap/core/log.h"

namespace coop::subsystems {

// ConnectReplayForSlot is the host's response to ClientWorldReady, which a client re-fires on
// every world-change re-seed (the join double-load and a mid-session level travel), so the host
// re-asserts world state into the client's reloaded world; its keyless chipPiles, eid-only,
// re-acquire their host eid that way. The replay is idempotent (adopt snapshots, deduplicated
// mirror registration), so re-running it is correct.

void ConnectReplayForSlot(int slot) {
    if (slot < 1 || slot >= static_cast<int>(coop::players::kMaxPeers)) return;
    UE_LOGI("net: slot %d world-ready -- replaying snapshot + flashlight + weather + peer states", slot);
    coop::rig_ready::Say("peer-world-ready", slot);
    // Before the snapshot, explicit per-key destroys for props this joiner's blob had that the
    // host's live world no longer has (grabbed or destroyed during the download and load), so the
    // client drops exactly those rather than the divergence sweep inferring the deletes. On the
    // bulk lane ahead of the snapshot, so the removes land before the adds. A no-op for a joiner
    // with no blob baseline, which the sweep still owns.
    coop::join_window_baseline::SendDivergenceDeletes(slot);
    // Every dish code, sent here on the bracket's lane ahead of its Begin, so a set that goes whole lands
    // before its Complete and a joined client holds the host's codes, a midnight since the capture
    // included; a set not taken whole is retried from the lane's tick and can land after it.
    coop::dish_hashcode_sync::QueueConnectBroadcastForSlot(slot);
    coop::prop_snapshot::TriggerForSlot(slot);
    coop::prop_drive_host::OnPeerWorldReady();  // every driven prop's pose again, so the joiner parks the resting ones the delta gate would never send it
    coop::kerfus_lanes::OnPeerWorldReady(slot);  // and every Kerfus's on, charging and energy
    coop::sat_console_sync::OnPeerWorldReady(slot);  // a SAT console typist back at a terminal still running finds it
    // Deliver the current position of any save-authoritative chipPile the host moved in this
    // joiner's connect window (the move's convert was dropped pre-world, and chipPiles carry no
    // position in the snapshot). After the snapshot, so it rides the bulk lane behind it; the
    // client snaps the bound native at quiescence.
    coop::join_window_baseline::FlushDivergedPositions(slot);
    coop::item_activate::QueueConnectBroadcastForSlot(slot);
    coop::weather_sync::QueueConnectBroadcastForSlot(slot);
    coop::interactable_sync::QueueConnectBroadcastForSlot(slot);  // door/light/container states
    coop::keypad_sync::QueueConnectBroadcastForSlot(slot);  // keypad states
    coop::sky_sync::QueueConnectBroadcastForSlot(slot);  // night-sky orientation, moon phase, eye
    coop::ariral_ship_sync::QueueConnectBroadcastForSlot(slot);
    coop::power_grid::QueueConnectBroadcastForSlot(slot);  // the generators' rows, before the panel they gate
    coop::power_panel::QueueConnectBroadcastForSlot(slot);  // the panel's canonical
    coop::fuse_tower_rows::QueueConnectBroadcastForSlot(slot);  // every tower's row
    coop::atv_sync::QueueConnectBroadcastForSlot(slot);  // ATV body pose, stamped with the host's seat and syncer (adopt=1)
    coop::drone_sync::QueueConnectBroadcastForSlot(slot);  // delivery drone pose (adopt=1)
    coop::order_queue_sync::QueueConnectBroadcastForSlot(slot);  // the delivery order queue: a reset + every queued order
    coop::turbine_sync::QueueConnectBroadcastForSlot(slot);  // wind-turbine facing/spin snap
    coop::device_occupancy::QueueConnectBroadcastForSlot(slot);  // live device claims (busy table)
    coop::download_arm_sync::QueueConnectResetForSlot(slot);  // a reset since the capture, ahead of the desk's seed: the game's order is reset, catch, arm
    coop::console_state_sync::QueueConnectBroadcastForSlot(slot);  // sky-signal snapshot + desk adopt
    coop::desk_input_sync::SeedPingAttributionFromMachine();  // a SOLO host's ping edge is absorbed unwired (PollOnce gated on connected) -- re-derive from ground truth so a mid-ping joiner gets the FSM-hold
    coop::desk_snd_fx::QueueConnectBroadcastForSlot(slot);  // desk loop-sound ground truth (a mid-loop joiner gets the ON)
    coop::physmods_sync::QueueConnectBroadcastForSlot(slot);  // canonical module array (ground truth over save drift)
    coop::drive_sync::QueueConnectBroadcastForSlot(slot);  // slot lines
    coop::drive_payload_sync::QueueConnectBroadcastForSlot(slot);  // every drive row that differs from its class default
    coop::drive_rack_sync::QueueConnectBroadcastForSlot(slot);  // rack canonicals (AFTER the payloads -- the shipped seed order on the one pinned lane)
    coop::meadow_db_sync::QueueConnectBroadcastForSlot(slot);  // the seedDelta(h) join seed (blob-instant snapshot vs live)
    coop::signal_sync::QueueConnectBroadcastForSlot(slot);  // the join-window saved-signal seed, both signs
    coop::email_sync::QueueConnectBroadcastForSlot(slot);  // the join-window email seed, both signs
    coop::signal_catch_sync::QueueConnectBroadcastForSlot(slot);  // the in-flight catch replay (the identity half) and the kind-2 state seed
    coop::laptop_sync::QueueConnectBroadcastForSlot(slot);  // the PC's power (its disc slot is floppy_slot_sync's)
    coop::laptop_ui_sync::QueueConnectBroadcastForSlot(slot);
    coop::portable_pc_lid::QueueConnectBroadcastForSlot(slot);  // every open portable PC lid
    coop::floppybox_sync::QueueConnectBroadcastForSlot(slot);  // one canonical per live box
    coop::props::container_contents_sync::QueueConnectBroadcastForSlot(slot);  // one slice per live world container (principle 8 anchor over the join snapshot)
    coop::dish_sync::QueueConnectBroadcastForSlot(slot);  // the dish snapshot -- AFTER the desk rows + the catch seed (same ordered lane)
    coop::download_arm_sync::QueueConnectArmForSlot(slot);  // an armed machine's ARM row, after the catch's seed and the dish snapshot
    coop::dish_calib_sync::QueueConnectBroadcastForSlot(slot);  // every dish's precision, the joiner's seed
    coop::shared_screen_sync::QueueConnectBroadcastForSlot(slot);
    coop::sleep_sync::QueueConnectBroadcastForSlot(slot);  // a joiner arrives awake -- end a running accelerate + re-tally
    coop::comp_sync::QueueConnectBroadcastForSlot(slot);  // the refiner's seed (CompState + CompData)
    coop::voice_chat::ReplayPeerStatesToSlot(slot);  // voice mute/disabled states -> joiner
    coop::window_sync::QueueConnectBroadcastForSlot(slot);  // base-window clean (adopt=1)
    coop::drink_spill_sync::SendAllToSlot(slot);
    coop::grime_sync::QueueConnectBroadcastForSlot(slot);  // surface grime process (adopt=1)
    coop::trash_pile_sync::QueueConnectBroadcastForSlot(slot);  // pile counters (adopt=1) + depleted-key replay
    coop::npc_world_enum::RegisterExistingWorldNpcs(slot);  // pre-existing/level-load NPCs (the save's kerfur) -> the peers already in; the joiner (slot) is skipped, its connect broadcast below sends it and it adopts its twin
    coop::npc_sync::QueueConnectBroadcastForSlot(slot);  // existing NPCs -> joiner
    coop::world_actor_sync::QueueConnectBroadcastForSlot(slot);  // existing event WorldActors -> joiner
    coop::balance_sync::SendCurrentToSlot(slot);  // host's current balance
    coop::credit_intent::SendChestsToSlot(slot);  // every treasure chest's key, lid and gold
    coop::props::prop_food_sync::SendAllToSlot(slot);  // every world food's temperature, ripeness and uses
    coop::kitchen_sync::SendAllToSlot(slot);
    coop::cargo_lift_sync::QueueConnect(slot);
    coop::prop_fire_sync::QueueConnect(slot);
    coop::hand_fire_sync::QueueConnect(slot);
    coop::hand_extinguisher_sync::QueueConnect(slot);
    coop::hand_toy_audio::QueueConnect(slot);
    coop::service_panel_sync::SendAllToSlot(slot);
    coop::cord_sync::SendAllToSlot(slot);
    coop::verb_lanes::ConnectReplayForSlot(slot);
    coop::upgrade_sync::SendCurrentToSlot(slot);  // and the upgrade levels: the joiner's save was taken at the handshake
    coop::daily_task_sync::SendCurrentToSlot(slot);  // and the daily task: a change in its load window was hashed but never sent to it
    // The host-to-client apply-blob push is not here: this fires at ClientWorldReady, after the
    // joiner loaded its world, so the blob would miss the pre-materialise hook; the host tick's
    // connect-edge detector drives it pre-world instead. No join teleport here either: the
    // joiner places itself at its world appearance, and a host teleport at world-ready
    // overwrote it, since world-ready arrives after the client has placed itself. The teleport
    // helper stays for the admin bring-to-host action. Then catch the new client up to the
    // existing peers' item state.
    coop::item_activate::ReplayPeerStatesToSlot(slot);
    // Existing peers' hand items to the joiner, as display mirrors.
    coop::hand_item::ReplayPeerStatesToSlot(slot);
    // One EventSnapshot per in-flight registry entry; the joiner replays the replay-safe rows with
    // the active override.
    coop::event_active_sync::SendJoinSnapshotForSlot(slot);
    // Each creature's count, after its NPC spawn (the queue above, same lane): the joiner applies a count at the bind.
    coop::creature_event_relay::SendJoinSnapshotForSlot(slot);
    // The alarm lane's late-join answer: the current alarm state, unconditionally, so a mid-alarm
    // joiner starts its klaxon on arrival.
    coop::alarm_sync::QueueConnectBroadcastForSlot(slot);
    coop::serverbox_sync::QueueConnectBroadcastForSlot(slot);  // current server state to the joiner
    coop::server_upgrade_sync::QueueConnectBroadcastForSlot(slot);  // every box's upgrade level
    coop::floppy_slot_sync::QueueConnectBroadcastForSlot(slot);  // every device slot, the laptop's file quad behind the laptop's: the joiner's world came from a save frozen before the first insert
    coop::hook_anchor::QueueConnectBroadcastForSlot(slot);  // anchored hooks the joiner's save capture missed; it dedups by key
    coop::placed_save_sync::QueueConnectBroadcastForSlot(slot);
    coop::roach_sync::QueueConnectBroadcastForSlot(slot);  // current roach population to the joiner
    // The piramid lane's late-join answer: re-send an in-flight gather commit to the slot, after
    // the world-actor and NPC snapshots above, since the replay's eid lookups need the joiner's
    // mirrors; its retry window absorbs drain skew.
    coop::piramid_sync::QueueConnectBroadcastForSlot(slot);
    // The cue lane's late-join answer: re-send the live cosmetic cue emitters, so a mid-shower
    // joiner replays the emitter.
    coop::event_cue_sync::QueueConnectBroadcastForSlot(slot);
    coop::madness_sync::QueueConnect(slot);
    coop::dingus_music_sync::QueueConnect(slot);
    coop::llama_sync::QueueConnect(slot);
    coop::watoy_sync::QueueConnect(slot);
    coop::grayboar_actions::QueueConnect(slot);
    coop::kavotia_sync::QueueConnect(slot);
    // The chat lane's late-join answer: the lobby's chat record, oldest first, one message per
    // line. It lands retained on the joiner: arriving in a lobby must not replay a conversation
    // you were not in across your screen; it is there when you open the chat.
    coop::chat_sync::QueueConnectBroadcastForSlot(slot);
}

}  // namespace coop::subsystems
