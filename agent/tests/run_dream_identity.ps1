param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$checkout=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $checkout '../_toolchain/env.ps1')
$output=Join-Path $checkout 'build/dream-identity-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Compile-And-Run([string]$name,[string]$source,[bool]$expectFailure=$false) {
    & cl /nologo /std:c++20 /EHsc /utf-8 "/I$PSScriptRoot/dream_identity_stubs" `
        "/I$checkout/src/votv-coop/include" "/Fo$output/$name.obj" "/Fe$output/$name.exe" $source
    if($LASTEXITCODE -ne 0){throw "Dream identity compile failed: $name"}
    & "$output/$name.exe"
    if($expectFailure){if($LASTEXITCODE -ne 1){throw "Fault control did not reach its assertion: $name (exit $LASTEXITCODE)"}}
    elseif($LASTEXITCODE -ne 0){throw "Dream identity regression failed: $name"}
}
Compile-And-Run 'test' "$PSScriptRoot/dream_identity_test.cpp"
& cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$checkout/src/votv-coop/include" `
    "/Fo$output/puppet_identity.obj" "$checkout/src/votv-coop/src/coop/player/puppet_identity.cpp"
if($LASTEXITCODE -ne 0){throw 'Production puppet_identity compilation failed'}
if($FaultControls){
    $source=Get-Content -LiteralPath "$checkout/src/votv-coop/src/coop/player/puppet_identity.cpp" -Raw
    $controls=@{
        'missing-watch'=$source.Replace('{&kRowPuppetDreamBurger, kDreamBurger, &OnWholePre, nullptr}',
                                      '{&kRowPuppetDreamBurger, kDreamBurger, nullptr, nullptr}')
        'stale-offset'=$source.Replace('if (!g_actorFn[index].Is(call.function)) {',
                                      'if (g_actorFn[index].Raw() != call.function) {')
        'unbounded-frame'=$source.Replace('off > bytes - static_cast<int32_t>(sizeof(void*))) return nullptr;',
                                          'false) return nullptr;')
    }
    foreach($name in $controls.Keys){
        if($controls[$name] -eq $source){throw "Fault substitution missed: $name"}
        $mutation=Join-Path $output "$name.cpp"
        # The source-local shared header stays the production one.
        $controls[$name].Replace('"puppet_identity_internal.h"',
            ('"'+($checkout.Replace('\','/')+'/src/votv-coop/src/coop/player/puppet_identity_internal.h')+'"')) |
            Set-Content -LiteralPath $mutation -Encoding utf8
        $runner=Join-Path $output "$name-test.cpp"
        ('#define PUPPET_IDENTITY_SOURCE "'+$mutation.Replace('\','/')+'"'+"`n"+
         '#include "'+($PSScriptRoot.Replace('\','/')+'/dream_identity_test.cpp')+'"') |
            Set-Content -LiteralPath $runner -Encoding utf8
        Compile-And-Run $name $runner $true
    }
}
