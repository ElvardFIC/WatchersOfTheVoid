// coop/config/config_registry_rows_dev_port.inc -- the [dev] rows of the version-port tooling (the fingerprint
// capture and census red arm, the dependency dump, the ***REMOVED***), included by
// config_registry_rows_dev.inc where they stood, so each keeps its place in every expansion of the list.
// The entry forms are config_registry_rows.inc's.

CFG_FLAG(fingerprint_capture, "fingerprint_capture", "dev", false, "VOTVCOOP_FINGERPRINT_CAPTURE",
    "Port tool: once gameplay is loaded, write every function of every loaded Blueprint class with its "
    "canonical bytecode text and fingerprint to multivoid-fingerprints-capture.txt beside the exe: the "
    "mod's half of the comparison with the text rendered from the cooked assets, and the native half of a "
    "build's reference table (docs/bytecode-fingerprint.md). About a second of work at world load; off by default.")
CFG_FLAG(dependency_dump, "dependency_dump", "dev", false, "VOTVCOOP_DEPENDENCY_DUMP",
    "Refactor proof: at the readiness milestones 'joined' (client) and 'peer-world-ready' (host), write every "
    "script_gate watch, ufunction_hook patch and game_thread observer and interceptor, sorted and counted, to "
    "multivoid-dependency-dump-<milestone>.txt beside the exe. Two builds' dumps diffed equal prove a change left "
    "the registered set as it was. Off by default.")
CFG_STRING(fingerprint_census_red, "fingerprint_census_red", "dev", "", "VOTVCOOP_FINGERPRINT_CENSUS_RED", false,
    "The fingerprint census's red arm: a row key ('class.function', lowercase) whose reference is falsified, so the "
    "census must report it 'body changed' and print 'fingerprint-census: RED ARM <row> reported body changed', or "
    "'RED ARM FAILED'. Empty: no red arm.")
***REMOVED***
    ***REMOVED***
    "mainGamemode BeginPlay scans Binaries for *.dll and Content/Paks for *.pak, then pops 'Modded "
    "client detected!' and quits). Empties that one scan's file list through our Func-patch substrate; "
    ***REMOVED***
    "public build is expected to carry no such gate, so this is off by default and never ships.")
