$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$agentRoot = Join-Path $root 'agent'
$archive = Join-Path $agentRoot 'archive/pre-release-2150'
if (Test-Path -LiteralPath $archive) { throw 'Consolidation already started; do not overwrite history.' }
New-Item -ItemType Directory -Path $archive | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)
$manifest = @()
foreach ($file in @(Get-ChildItem $agentRoot -Filter 'TODO_*.md' -File)) {
    $destination = Join-Path $archive $file.Name
    Copy-Item -LiteralPath $file.FullName -Destination $destination
    $hash = (Get-FileHash -LiteralPath $file.FullName).Hash
    if ((Get-FileHash -LiteralPath $destination).Hash -ne $hash) { throw 'Archive hash mismatch' }
    $manifest += [pscustomobject]@{ File=$file.Name; SHA256=$hash }
    if ($file.Name -notin @('TODO_2150.md','TODO_CONTINUE.md')) {
        Remove-Item -LiteralPath $file.FullName
    }
}
$manifest | ConvertTo-Json | Set-Content (Join-Path $archive 'manifest.json') -Encoding UTF8
$tracker = Get-Content (Join-Path $agentRoot 'TESTLOG_STATUS.md')
$rows = @($tracker | Where-Object { $_ -match '^\|(\d+)\|' -and [int]$Matches[1] -notin @(6,10,41) })
if ($rows.Count -ne 55) { throw "Unexpected backlog row count: $($rows.Count)" }
$intro = @'
# TODO — release 2150

Consolidated 18 October 2026 at the owner's request. Release 2150 includes all
accumulated source work through WORK_2192, including the later development numbers.
Those numbers are historical work IDs, not newer packaged releases.
Every row below is still open for implementation or actual two-player verification.
"Code ready" and focused offline checks do not close a gameplay report.

## First checks on both peers using the new 2150 package

- [ ] Insomnia demon spawn, visibility, client targeting and attacks (49).
- [ ] Selected-model ragdoll pose/entry/exit on each peer (5/50); do not restore the plushie fallback.
- [ ] Client appliance plug/unplug, short/long cables, extensions and unpowered outlets (51).
- [ ] Identify which token counter/UI is desynchronized; do not assume money (52).
- [ ] Shared base flicker, late join and load recovery (53).
- [ ] Computer shared screen/operator/activity (54), ON/OFF during animation, power loss and reconnect (55).
- [ ] ZIP insertion/ejection: actual returned item, class, model and scale on both peers (56).
- [ ] Cans: open/eat/final portion, held/world, stow/re-equip, simultaneous use and reconnect (57).
- [ ] Workbench: simple placement and disappearance after 1–2 seconds or later (58).
  Record the exact item, authoring peer and logs; crafting is not required by the report.

## Complete open tracker carried into 2150

The statuses retain their original work IDs and evidence. Duplicate reports stay linked
rather than being silently dropped. Issues 6 (mining) and 10 (nightmares) were reported
working; issue 41 (old basement wall) remains explicitly excluded by the owner.

| ID | Problem | Remaining work / evidence |
|---|---|---|
'@
$extra = @'

## Additional unfinished details retained from earlier TODOs

- [ ] Food/package final-bite destruction and subclass eaten() output; preserve local poison/player effects.
  Other package families and client-local loot need explicit provenance, not global replaceProp admission.
- [ ] Rejected client placement may remain awaiting binding: establish acknowledgement/refusal behavior.
- [ ] Coin pickup by E/overlap: one host credit, disappearance only after acceptance; map-placed coins remain local.
- [ ] Console event bodies/AI/damage for Rufus/thiccfus, llama, Madness grunts and variants;
  simultaneous typists' canonical order, periodic/local output and exact music phase remain.
- [ ] Wall repair requires an atomic request/result and exact personal cement stock across
  hand/slot/drop, retry/rejection/disconnect; keep the current no-spend refusal until complete.
  Creative construction, wet-wall timer persistence and exact impact/effects remain.
- [ ] Water/cement/coal concurrent writers and stale in-flight records; dryTimer ownership;
  sponge soapAmount/foam/surfaces, grill lid/final cooked food and personal-hand transitions.
- [ ] Enroll pre-session plants/faucets/hoses; later watering/fertilizing/harvest, pot transitions,
  accepted hose rerouting and concurrent use. Missing historical save objects are not migrated.
- [ ] Sky-event audio, delayed email/bookkeeping and pre-session latent chains; other custom
  event effects/creatures need host phase/cues rather than replaying whole commands on every peer.
- [ ] Stow frame-time capture if residual stutters remain; preserve pending item-state restoration.
- [ ] Skin preview gaps, custom skins and real animation/physics; do not remove compatibility exclusions.
- [ ] Existing saved second-ATV positions are unchanged; test near-simultaneous carry/mount and stale menus.
- [ ] Whole-package multiplayer smoke: host/client role reversal, two clients contesting objects,
  late join/rejoin, disconnect, save/reload, partial/empty consumables, full inventory,
  equipped items, container rows, loose cables, flashlight crank/low battery and model previews.

Exact prior TODO text is preserved in [archive/pre-release-2150](archive/pre-release-2150/).
WORK notes, tests, native evidence and the issue tracker remain in place. Old TODO paths
inside historical notes refer to that archive. Update this backlog and TODO_CONTINUE;
do not create another parallel per-turn TODO queue.
'@
[IO.File]::WriteAllText((Join-Path $agentRoot 'TODO_2150.md'), $intro + "`n" + ($rows -join "`n") + "`n" + $extra + "`n", $utf8)
Write-Output "Consolidated $($rows.Count) open tracker rows; preserved $($manifest.Count) original TODOs."
