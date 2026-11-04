param([Parameter(Mandatory=$true)][string]$EvidenceDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$evidence=if([IO.Path]::IsPathRooted($EvidenceDirectory)){$EvidenceDirectory}else{Join-Path $root $EvidenceDirectory}
$logs=Join-Path $evidence 'prebuild'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$python=Join-Path $env:USERPROFILE '.local/bin/python3.15.exe'
$results=@()
Push-Location $root
try {
    foreach($name in @('fact_rows_gate','public_prose_gate')) {
        $log=Join-Path $logs ($name+'.log')
        $ErrorActionPreference='Continue'
        $global:LASTEXITCODE=$null
        & $python ".github/ci/$name.py" *> $log
        $code=$LASTEXITCODE
        $ErrorActionPreference='Stop'
        $results+= [pscustomobject]@{Runner=$name;ExitCode=$code;Log=$log}
        $results | ConvertTo-Json -Depth 3 | Set-Content (Join-Path $logs 'results.json') -Encoding UTF8
        if($null -eq $code -or $code -ne 0){throw "Pre-build gate failed: $name; Release build not started; see $log"}
    }
} finally {Pop-Location}
