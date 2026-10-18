$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$utf8=New-Object Text.UTF8Encoding($false)
$handoff=@'
# Current handoff — release 2150

Updated 18 October 2026. The owner requested a consolidated 2150 package,
one backlog and cleanup. This supersedes the former packaging deferral.

- Release/protocol identity: 2150; includes all source work through WORK_2192.
  Development IDs 2151–2192 are historical work labels, not rollback instructions.
  Both peers must use the new package, not an old development 2150 binary.
- Full backlog: [TODO_2150.md](TODO_2150.md). All 55 unclosed tracker rows are retained,
  including duplicate reports and fixes awaiting gameplay. 6/10 were reported working;
  old basement wall41 is excluded. Exact old TODOs are in archive/pre-release-2150/.
- Issue evidence: [TESTLOG_STATUS.md](TESTLOG_STATUS.md), then the relevant WORK note.
  Reports49–58 were observed by the owner on2168; do not close them from offline checks.
- Release evidence: [RELEASE_2150.md](../package/RELEASE_2150.md).
  Runtime multiplayer verification is still pending.

## Next work

1. Run the ten priority host/client checks in TODO_2150 on the new package.
2. Bench58: simple placement disappears after1–2 seconds or later. Exact item and
   peer remain unknown; birth/save retry fixes are not a proven symptom resolution.
3. Identify token52's exact counter/UI before changing balance code.
4. Wall repair21/22 requires atomic cement request/result accounting across
   hand/slot/drop/retry/disconnect; preserve the existing no-spend refusal.
5. Continue remaining code fixes from TODO_2150; preserve historical native evidence.

The user leads the project. No other agent workflow. Commit, push, game installation
and publication still need an explicit request. Preserve all uncommitted work.
Edit canonical checkout agent/, then run sync_mirrors.ps1 and -Check.
Keep this handoff short; update the single 2150 backlog rather than creating duplicates.
'@
[IO.File]::WriteAllText((Join-Path $root 'agent/TODO_CONTINUE.md'),$handoff+"`n",$utf8)
$workspace=Split-Path -Parent $root
$readme=@'
# Watchers of the Void

Поточна версія: **2150**, VOTV **0.9.0n**. Включає накопичені зміни до WORK_2192.
Збірка: [dist/WatchersOfTheVoid-0.9.2150.zip](dist/WatchersOfTheVoid-0.9.2150.zip).
Усі гравці мають установити цей самий пакет. Перевірка в грі вдвох ще потрібна.

| Шлях | Призначення |
|---|---|
| src/ | Єдиний робочий Git-репозиторій |
| src/src/votv-coop/ | Вихідний код мода |
| src/agent/TODO_2150.md | Єдиний зведений список незавершених завдань |
| src/agent/TODO_CONTINUE.md | Короткий поточний контекст |
| src/package/RELEASE_2150.md | Перевірки й відомості про збірку |
| src/package/build_2150.ps1 | Release-збірка поточного коду |
| src/package/pack.ps1 | Пакування перевіреної DLL 2150 |
| agent/ | Автоматично синхронізована копія робочих нотаток і тестів |
| dist/ | Поточний ZIP; попередні пакети в dist/archive/ |
| incoming/, archive/, _research/ | Збережені сторонні вихідники, історія й матеріали досліджень |
| _toolchain/ | Локальні інструменти та залежності збірки |

Старі TODO збережені в src/agent/archive/pre-release-2150/.
WORK-нотатки й native-матеріали залишені для продовження роботи.
Номери старих робочих етапів не визначають номер нового пакета.
Журнал очищення: archive/cleanup-2150.json. Встановлення в гру та публікація не виконувалися.
'@
[IO.File]::WriteAllText((Join-Path $workspace 'README.md'),$readme+"`n",$utf8)
Write-Output 'Updated current handoff and workspace README.'
