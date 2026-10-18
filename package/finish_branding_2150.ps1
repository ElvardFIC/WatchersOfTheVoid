$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent $root
$backup=Join-Path $workspace 'archive/branding-before-watchers-2150'
$utf8=New-Object Text.UTF8Encoding($false)
$text=@'
# Watchers of the Void branding

Author and maintainer: ElvardFIC. Original Multivoid: Pelmentor and contributors.

- cover.jpg: exact original JPEG supplied by ElvardFIC on 18 October 2026.
- icon.png: 256x256 PNG conversion for Thunderstore, preserving the full composition.

Previous icons and branding notes are preserved in the outer workspace at
archive/branding-before-watchers-2150/. No AI regeneration or content editing was
applied to this supplied cover. Future releases may need updated version text in the artwork.
'@
[IO.File]::WriteAllText((Join-Path $root 'assets/branding/README.md'),$text+"`n",$utf8)
foreach ($name in @('icon-512.png','icon.psd')) {
    $path=Join-Path $root ('assets/branding/'+$name)
    if (Test-Path -LiteralPath $path) {
        $dest=Join-Path $backup ('assets/branding/'+$name)
        Copy-Item -LiteralPath $path -Destination $dest
        if ((Get-FileHash -LiteralPath $path).Hash -ne (Get-FileHash -LiteralPath $dest).Hash) { throw 'Old artwork archive mismatch' }
        Remove-Item -LiteralPath $path
    }
}
foreach ($dir in @((Join-Path $root 'dist'),(Join-Path $workspace 'dist'))) {
    $old=Join-Path $dir 'MultiVoidKindaPlus-0.9.2150.zip'
    if (Test-Path -LiteralPath $old) {
        $preserved=Join-Path $backup 'dist/MultiVoidKindaPlus-0.9.2150.zip'
        if ((Get-FileHash -LiteralPath $old).Hash -ne (Get-FileHash -LiteralPath $preserved).Hash) { throw 'Old package archive mismatch' }
        Remove-Item -LiteralPath $old
    }
}
Write-Output 'Archived previous artwork and pre-branding package; current branding documentation updated.'
