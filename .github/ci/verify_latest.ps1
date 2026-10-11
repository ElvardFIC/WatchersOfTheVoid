# verify_latest.ps1 -- the STABLE-ritual closing check (design D3): after every master's COOP_LATEST_GAMES is
# updated, each master's /v1/latest must list every game's newest published stable under `games`, and its
# legacy `proto` must be the 0.9.0n entry's -- the game every client older than `games` targets.
# The masters are the official slots in protocol.h (kOfficialMasterSlots): a player reads the update
# check from the master they chose, so one master left behind tells its players a wrong verdict.
# Fold-aware (R23): "the newest BARE-tag row whose state(N) == PUBLISHED" -- a retracted N has a
# published row too; the terminal closes it. Drilled to FAIL pre-env-step and PASS post-env-step at
# the first real stable ritual (deferred drill, R22).

param(
    [string]$LedgerPath = (Join-Path $PSScriptRoot 'LEDGER.tsv'),
    # Default: every slot of kOfficialMasterSlots in protocol.h.
    [string[]]$MasterLatestUrls = @(),
    # Pass when the masters' COOP_LATEST_* was deliberately pointed at a DEV
    # prerelease. Without it this script asserts the stable contract above and
    # would call a dev-advertising master an "unrecorded release" -- a false
    # accusation, which is worse than no check. See Get-NewestPublished.
    [switch]$AllowDev
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ledger_lib.ps1')

if ($MasterLatestUrls.Count -eq 0) {
    # ONE definition of the masters (protocol.h): comma-separated label=address slots, and a bare
    # host:port means TLS, as the client's own grammar says.
    $repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $hdr = Get-Content (Join-Path $repoRoot 'src/votv-coop/include/coop/net/protocol.h') -Raw
    $m = [regex]::Match($hdr, 'kOfficialMasterSlots\s*=\s*"([^"]+)"')
    if (-not $m.Success) { throw 'kOfficialMasterSlots not found in protocol.h' }
    $MasterLatestUrls = @(foreach ($slot in $m.Groups[1].Value.Split(',')) {
        $address = $slot.Substring($slot.IndexOf('=') + 1).Trim()
        if ($address -match '^[a-z]+://') { "$address/v1/latest" } else { "https://$address/v1/latest" }
    })
}

$kind = if ($AllowDev) { 'published (dev admitted)' } else { 'published stable' }
$rows = (Read-Ledger -Path $LedgerPath).Rows
$perGame = Get-NewestPublishedPerGame -Rows $rows -IncludeDev:$AllowDev
foreach ($g in $perGame.Keys) {
    Write-Host "verify_latest: ledger newest $kind for $g = N=$($perGame[$g].N) tag=$($perGame[$g].TagName)"
}
# The legacy fields answer clients older than `games`, all made for this game (server/src/lobby.rs LEGACY_GAME).
$legacyGame = '0.9.0n'
$expected = if ($perGame.ContainsKey($legacyGame)) { $perGame[$legacyGame] } else { $null }

$failed = 0
foreach ($url in $MasterLatestUrls) {
    Write-Host "verify_latest: querying $url"
    $resp = $null
    try { $resp = Invoke-RestMethod -Uri $url -TimeoutSec 15 } catch {
        Write-Host "verify_latest: FAIL -- $url unreachable: $($_.Exception.Message)"
        $failed++
        continue
    }
    $masterProto = 0
    if ($resp -and ($resp.PSObject.Properties.Name -contains 'proto')) { $masterProto = [int]$resp.proto }

    Write-Host "verify_latest: $url reports proto=$masterProto mod='$($resp.mod)'"
    $hint = if ($AllowDev) { '' } else { ' -- if the masters were deliberately pointed at a DEV prerelease, re-run with -AllowDev' }
    # The legacy pair: the legacy game's newest, or no record when the ledger has none for it.
    if ($null -eq $expected) {
        if ($masterProto -gt 0) {
            Write-Host "verify_latest: FAIL -- $url reports proto=$masterProto but the ledger has NO $kind for $legacyGame (unrecorded release?)$hint"
            $failed++
        }
    } elseif ($masterProto -ne $expected.N) {
        Write-Host "verify_latest: FAIL -- $url legacy proto $masterProto != ledger N $($expected.N), the newest $kind for $legacyGame (COOP_LATEST_GAMES not updated / stale?)"
        $failed++
    }
    # Every game: each one the ledger has, at its newest; none the ledger does not.
    $listed = @{}
    if ($resp.PSObject.Properties.Name -contains 'games') {
        foreach ($e in @($resp.games)) { $listed[[string]$e.game] = [int]$e.proto }
    }
    foreach ($g in $perGame.Keys) {
        if (-not $listed.ContainsKey($g)) {
            Write-Host "verify_latest: FAIL -- $url lists no release for $g under games (ledger N $($perGame[$g].N))"
            $failed++
        } elseif ($listed[$g] -ne $perGame[$g].N) {
            Write-Host "verify_latest: FAIL -- $url lists $g at N=$($listed[$g]), ledger N $($perGame[$g].N)"
            $failed++
        }
    }
    foreach ($g in $listed.Keys) {
        if (-not $perGame.ContainsKey($g)) {
            Write-Host "verify_latest: FAIL -- $url lists $g at N=$($listed[$g]) but the ledger has NO $kind for it (unrecorded release?)$hint"
            $failed++
        }
    }
    if ($perGame.Count -eq 0 -and $listed.Count -eq 0 -and $masterProto -le 0) {
        Write-Host "verify_latest: OK_EMPTY -- no $kind in the ledger; $url has no released record"
    }
}

if ($failed -gt 0) {
    Write-Host "verify_latest: FAIL -- $failed of $($MasterLatestUrls.Count) master(s)"
    exit 1
}
Write-Host "verify_latest: PASS ($($MasterLatestUrls.Count) master(s))"
exit 0
