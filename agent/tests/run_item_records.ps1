param([switch]$CompileSources,[switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=Join-Path $root 'build/item-record-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $root
try {
    if(-not(Get-Command cl -ErrorAction SilentlyContinue)){. (Join-Path $root '../_toolchain/env.ps1')}
    & cl /nologo /std:c++20 /EHsc /Gy /utf-8 /I agent/tests/item_record_stubs /I src/votv-coop/include "/Fo$output/controller.obj" "/Fe$output/controller.exe" agent/tests/item_record_reentry_test.cpp /link /OPT:REF
    if($LASTEXITCODE -ne 0){throw 'Item record controller compile failed'}
    & "$output/controller.exe"
    if($LASTEXITCODE -ne 0){throw 'Item record controller failed'}
    if($FaultControls){
        $source=Get-Content 'src/votv-coop/src/coop/props/prop_save_data.cpp' -Raw
        $apply=Get-Content 'src/votv-coop/src/coop/props/prop_save_apply.inc' -Raw
        $source=$source.Replace('#include "prop_save_apply.inc"',$apply)
        foreach($name in @('ignores-revision','unguarded-capture-retry','unguarded-send-retry',
            'foreign-supersedes-author','receipt-clears-wait','old-apply-clears-new-wait','birth-keeps-old-capture','wrong-initial-author',
            'capture-skips-wait','capture-skips-post-apply-wait','unguarded-native-apply')){
            $control=Join-Path $output $name
            New-Item -ItemType Directory -Force -Path $control | Out-Null
            $mutant=switch($name){
                'ignores-revision' {$source.Replace(' && (!current || current());',';')}
                'unguarded-capture-retry' {$source.Replace('if (!current && stillCurrent()','if (stillCurrent()')}
                'unguarded-send-retry' {$source.Replace('rec, &identity,stillCurrent,!current);','rec, &identity,stillCurrent,true);')}
                'foreign-supersedes-author' {$source.Replace('if(wait.senderSlot!=senderSlot)return;','')}
                'receipt-clears-wait' {$source.Replace('const bool initialRecord=awaited!=0;','const bool initialRecord=awaited!=0;g_awaiting.erase(key);')}
                'old-apply-clears-new-wait' {$source.Replace('wait->second.seq==snapshot.awaited &&','')}
                'birth-keeps-old-capture' {$source.Replace('RecordChanged(key); // New birth supersedes an active record scope.','')}
                'wrong-initial-author' {$source.Replace(' || parked->second.senderSlot!=authorSlot','')}
                'capture-skips-wait' {$source.Replace('if (!g_parked.count(key)) return !StillAwaited(key);','if (!g_parked.count(key)) return true;')}
                'capture-skips-post-apply-wait' {$source.Replace('&& !g_parked.count(key) && !StillAwaited(key);','&& !g_parked.count(key);')}
                'unguarded-native-apply' {$source.Replace('SR::ApplyRecordWhile(actor, *applied, current)','SR::ApplyRecord(actor, *applied)')}
            }
            if($mutant -eq $source){throw "Missing mutation site: $name"}
            # The publisher uses sibling includes; resolve them from its production directory.
            $mutantPath=(Join-Path $control 'publisher.inc').Replace('\','/')
            [IO.File]::WriteAllText($mutantPath,$mutant,[Text.UTF8Encoding]::new($false))
            $testPath=(Join-Path $PSScriptRoot 'item_record_reentry_test.cpp').Replace('\','/')
            $fixture=Join-Path $control 'control.cpp'
            $lines=@(('#define ITEM_RECORD_SOURCE "'+$mutantPath+'"'),('#include "'+$testPath+'"'))
            [IO.File]::WriteAllText($fixture,($lines -join "`n"),[Text.UTF8Encoding]::new($false))
            & cl /nologo /std:c++20 /EHsc /Gy /utf-8 "/I$PSScriptRoot/item_record_stubs" /I src/votv-coop/include /I src/votv-coop/src/coop/props "/Fo$control/control.obj" "/Fe$control/control.exe" $fixture /link /OPT:REF
            if($LASTEXITCODE -ne 0){throw "Control compile failed: $name"}
            & "$control/control.exe"
            if($LASTEXITCODE -ne 1){throw "Control did not fire its assertion: $name"}
            Write-Output "PASS expected-failure control: $name"
        }
    }
    if($CompileSources){
        & cl /nologo /std:c++20 /EHsc /utf-8 /c /I src/votv-coop/include "/Fo$output/prop_save_data.obj" src/votv-coop/src/coop/props/prop_save_data.cpp
        if($LASTEXITCODE -ne 0){throw 'Item record source compile failed'}
    }
} finally {Pop-Location}
