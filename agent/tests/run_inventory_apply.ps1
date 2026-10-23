param([switch]$FaultControls)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
. (Join-Path $root '../_toolchain/env.ps1')
$output=Join-Path $root 'build/inventory-apply-check'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$reflection=Get-Content (Join-Path $root 'src/votv-coop/src/ue_wrap/core/reflection_props.cpp') -Raw
$offset=[regex]::Match($reflection,'(?ms)^int32_t InstanceOffset::Of\(.*?^\}').Value
if(-not $offset){throw 'Production InstanceOffset seam missing'}
[IO.File]::WriteAllText((Join-Path $output 'instance_offset_production.inc'),("namespace ue_wrap::reflection {`n"+$offset+"`n}"),[Text.UTF8Encoding]::new($false))
& cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$output" "/I$root/src/votv-coop/include" `
    "/Fo$output/test.obj" "/Fe$output/test.exe" "$PSScriptRoot/inventory_apply_test.cpp" /link /OPT:REF
if($LASTEXITCODE){throw 'Inventory apply compile failed'}
& "$output/test.exe"
if($LASTEXITCODE){throw 'Inventory apply regression failed'}
if($FaultControls) {
    $source=(Get-Content (Join-Path $root 'src/votv-coop/src/ue_wrap/actors/inventory.cpp') -Raw).Replace("`r`n","`n")
    foreach($name in @('unchecked-record','stale-target','foreign-slot','corrupt-personal','partial-profile','corrupt-target','unchecked-hand')) {
        $mutant=switch($name) {
            'unchecked-record' {$source.Replace('return SR::WriteSaveRecordChecked(at,record);','SR::WriteSaveRecord(at,record);return true;')}
            'stale-target' {$source.Replace('if(!current() || !write(static_cast<uint8_t*>(data)+i*stride,records[i]) || !current())return false;','if(!write(static_cast<uint8_t*>(data)+i*stride,records[i]))return false;').Replace(' || !current())return false;',' )return false;').Replace('return current();','return true;')}
            'foreign-slot' {$source.Replace('        return -1;' + "`n" + '    }' + "`n`n" + '    const auto stackOff=', '    }' + "`n`n" + '    const auto stackOff=')}
            'corrupt-personal' {$source.Replace('if(!SR::TryReadArr(stack.data+static_cast<size_t>(idx)*SR::kMxStride,0,contents))return -1;','contents=SR::ReadArr(stack.data+static_cast<size_t>(idx)*SR::kMxStride,0);')}
            'partial-profile' {$source.Replace('if(!SR::TryReadArr(save,kOff_equipment,eq) || !SR::TryReadArr(save,kOff_hold,hd))return false;','eq=SR::ReadArr(save,kOff_equipment);hd=SR::ReadArr(save,kOff_hold);')}
            'corrupt-target' {$source.Replace(('if(!SR::TryReadArr(carried,0,oldCarried) || !SR::TryReadArr(saveSlot,kOff_equipment,oldEquipment) ||'+"`n"+'       !SR::TryReadArr(saveSlot,kOff_hold,oldHeld))return false;'),'oldEquipment=SR::ReadArr(saveSlot,kOff_equipment);oldHeld=SR::ReadArr(saveSlot,kOff_hold);')}
            'unchecked-hand' {$source.Replace('if(!WriteEquipRecord(staged.data(),record) || !current())return false;','WriteEquipRecord(staged.data(),record);')}
        }
        if($mutant -eq $source){throw "Missing control site: $name"}
        $control=Join-Path $output $name
        New-Item -ItemType Directory -Force -Path $control | Out-Null
        $inc=(Join-Path $control 'inventory.inc').Replace('\','/')
        [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
        $test=(Join-Path $PSScriptRoot 'inventory_apply_test.cpp').Replace('\','/')
        $fixture=Join-Path $control 'control.cpp'
        [IO.File]::WriteAllText($fixture,(('#define INVENTORY_APPLY_SOURCE "'+$inc+'"')+"`n"+('#include "'+$test+'"')),[Text.UTF8Encoding]::new($false))
        & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$output" "/I$root/src/votv-coop/include" "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
        if($LASTEXITCODE){throw "Control compile failed: $name"}
        & "$control/test.exe"
        if($LASTEXITCODE -ne 1){throw "Removed protection was not detected: $name"}
        Write-Output "PASS expected-failure control: $name"
    }
    $control=Join-Path $output 'stale-layout'
    New-Item -ItemType Directory -Force -Path $control | Out-Null
    $offsetMutant=$offset.Replace('if (cls != cls_ || !IsLiveByIndex(cls_, clsIdx_) || SlotSerial(clsIdx_) != clsSerial_) {','if (!cls_) {')
    if($offsetMutant -eq $offset){throw 'Missing class-incarnation control site'}
    $offsetInc=(Join-Path $control 'offset.inc').Replace('\','/')
    [IO.File]::WriteAllText($offsetInc,("namespace ue_wrap::reflection {`n"+$offsetMutant+"`n}"),[Text.UTF8Encoding]::new($false))
    $test=(Join-Path $PSScriptRoot 'inventory_apply_test.cpp').Replace('\','/')
    $fixture=Join-Path $control 'control.cpp'
    [IO.File]::WriteAllText($fixture,(('#define INVENTORY_OFFSET_SOURCE "'+$offsetInc+'"')+"`n"+('#include "'+$test+'"')),[Text.UTF8Encoding]::new($false))
    & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$output" "/I$root/src/votv-coop/include" "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
    if($LASTEXITCODE){throw 'Class-incarnation control compile failed'}
    & "$control/test.exe"
    if($LASTEXITCODE -ne 1){throw 'Old class-layout reuse was not detected'}
    Write-Output 'PASS expected-failure control: stale-layout'
    $control=Join-Path $output 'stale-derived-state'
    New-Item -ItemType Directory -Force -Path $control | Out-Null
    $container=Get-Content (Join-Path $root 'src/votv-coop/src/ue_wrap/actors/container_inventory.cpp') -Raw
    $mutant=$container.Replace('if (current() && recalc.Get())','if (recalc.Get())')
    if($mutant -eq $container){throw 'Missing derived-state control site'}
    $inc=(Join-Path $control 'container.inc').Replace('\','/')
    [IO.File]::WriteAllText($inc,$mutant,[Text.UTF8Encoding]::new($false))
    $fixture=Join-Path $control 'control.cpp'
    [IO.File]::WriteAllText($fixture,(('#define INVENTORY_CONTAINER_SOURCE "'+$inc+'"')+"`n"+('#include "'+$test+'"')),[Text.UTF8Encoding]::new($false))
    & cl /nologo /std:c++20 /EHsc /utf-8 /Gy "/I$output" "/I$root/src/votv-coop/include" "/Fo$control/test.obj" "/Fe$control/test.exe" $fixture /link /OPT:REF
    if($LASTEXITCODE){throw 'Derived-state control compile failed'}
    & "$control/test.exe"
    if($LASTEXITCODE -ne 1){throw 'Stale native derivation was not detected'}
    Write-Output 'PASS expected-failure control: stale-derived-state'
}
& "$env:USERPROFILE/.local/bin/python3.15.exe" "$PSScriptRoot/inventory_apply_native_test.py"
if($LASTEXITCODE){throw 'Inventory native contract failed'}
foreach($name in @('inventory','container_inventory','save_record')) {
    & cl /nologo /std:c++20 /EHsc /utf-8 /c "/I$root/src/votv-coop/include" "/Fo$output/$name.obj" "$root/src/votv-coop/src/ue_wrap/actors/$name.cpp"
    if($LASTEXITCODE){throw "Production compile failed: $name"}
}
