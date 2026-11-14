param([switch]$CompileOnly,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/entity-adapters-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
if(-not $CompileOnly) {
    foreach($test in @('roach_sync','piramid_sync')) {
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/entity_adapter_stubs" "/I$checkout/src/votv-coop/include" `
            "/Fo$output/$test.obj" "/Fe$output/$test.exe" "$PSScriptRoot/${test}_test.cpp" /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Compile failed: $test"}
        & "$output/$test.exe"
        if($LASTEXITCODE -ne 0){throw "Test failed: $test"}
    }
    & python3.15.exe (Join-Path $PSScriptRoot 'entity_adapters_native_test.py')
    if($LASTEXITCODE -ne 0){throw 'Native entity adapter contract failed'}
}
if($FaultControls) {
    $mutations=@(
        @('roach_sync','serial','!slot.comp.Get()','!slot.comp.Raw()'),
        @('roach_sync','stale-page','if(g_asmSeq&&static_cast<int32_t>(p.seq-g_asmSeq)<=0)return;',''),
        @('roach_sync','unchecked-location','f.Set<bool>(L"bSweep",false)&&','true&&'),
        @('roach_sync','replay-uncertain','g_pending.uncertain=true;','g_pending.uncertain=false;'),
        @('piramid_sync','repeat-gather','g_pending.active=!admitted;','g_pending.active=true;'),
        @('piramid_sync','foreign-pass','if(!BrainSuppress_Interceptor(self,params))return false;','return false;'),
        @('piramid_sync','join-generation','session->peerGenerationForSlot(slot)!=generation||',''),
        @('piramid_sync','callback-heading','||!owner.Get()||!component.Get()||','||!component.Get()||')
    )
    foreach($mutation in $mutations) {
        $unit,$label,$needle,$replacement=$mutation
        $source=Get-Content -LiteralPath "$checkout/src/votv-coop/src/coop/creatures/$unit.cpp" -Raw
        if(-not $source.Contains($needle)){throw "Fault anchor missing: $label"}
        $test=Get-Content -LiteralPath "$PSScriptRoot/${unit}_test.cpp" -Raw
        $test=$test.Substring($test.IndexOf("`n")+1)
        $path=Join-Path $output "$unit-fault-$label.cpp"
        [IO.File]::WriteAllText($path,$source.Replace($needle,$replacement)+"`n"+$test,[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/entity_adapter_stubs" "/I$checkout/src/votv-coop/include" `
            "/Fo$output/$label.obj" "/Fe$output/$label.exe" $path /link /OPT:REF
        if($LASTEXITCODE -ne 0){throw "Fault control did not compile: $label"}
        & "$output/$label.exe"
        if($LASTEXITCODE -eq 0){throw "Fault was not detected: $label"}
        Write-Output "PASS rejected independent fault: $label"
    }
}
foreach($unit in @('roach_sync','piramid_sync')) {
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
        "/I$checkout/src/votv-coop/third_party/GameNetworkingSockets/include" `
        "/Fo$output/$unit-production.obj" "$checkout/src/votv-coop/src/coop/creatures/$unit.cpp"
    if($LASTEXITCODE -ne 0){throw "Production compile failed: $unit"}
}
