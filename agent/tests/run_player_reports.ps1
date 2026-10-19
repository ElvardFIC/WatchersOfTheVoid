param([string[]]$Only=@())
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/player-reports-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach($name in @('tool_fx','npc_late_class','skin_previews_2151','kitchen_2124','skin_preview_retry','cremator_paint','wisp_spawn_race')) {
 if($Only.Count -and $name -notin $Only){continue}
 $includes=@("/I$checkout/src/votv-coop/include")
 if($name -ne 'npc_late_class'){$includes=@("/I$PSScriptRoot/text_drone_stubs","/I$PSScriptRoot/stubs")+$includes}
 if($name -eq 'kitchen_2124'){$includes=@("/I$PSScriptRoot/stubs","/I$checkout/src/votv-coop/include")}
 if($name -eq 'skin_preview_retry'){$includes=@("/I$PSScriptRoot/stubs","/I$checkout/src/votv-coop/include")}
 if($name -eq 'cremator_paint'){$includes=@("/I$checkout/src/votv-coop/include")}
 if($name -eq 'wisp_spawn_race'){$includes=@("/I$PSScriptRoot/wisp_stubs","/I$checkout/src/votv-coop/include")}
 & cl /nologo /std:c++20 /EHsc /Gy /utf-8 @includes `
  "/Fo$output/$name.obj" "/Fe$output/$name.exe" "$PSScriptRoot/${name}_test.cpp" /link /OPT:REF
 if($LASTEXITCODE -ne 0){throw "Regression compile failed: $name"}
 if($name -eq 'skin_previews_2151'){& "$output/$name.exe" (Join-Path $output ([guid]::NewGuid().ToString()))}
 else {& "$output/$name.exe"}
 if($LASTEXITCODE -ne 0){throw "Regression failed: $name"}
}
