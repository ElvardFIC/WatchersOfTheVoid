$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$workspace=Split-Path -Parent $root
$backup=Join-Path $workspace 'archive/branding-before-watchers-2150'
if (Test-Path -LiteralPath $backup) { throw 'Branding backup already exists; inspect before rerunning.' }
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($relative in @('assets/branding/icon.png','assets/branding/README.md','package/README.md','package/pack.ps1','dist/MultiVoidKindaPlus-0.9.2150.zip')) {
    $destination=Join-Path $backup $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
    if ((Get-FileHash -LiteralPath $destination).Hash -ne (Get-FileHash -LiteralPath (Join-Path $root $relative)).Hash) { throw 'Backup hash mismatch' }
}
$original='C:/Users/Elvar/Downloads/Gemini_Generated_Image_8cwfpi8cwfpi8cwf.jpg'
$cover=Join-Path $root 'assets/branding/cover.jpg'
Copy-Item -LiteralPath $original -Destination $cover
# Mechanical package-format conversion only; retain the supplied artwork and composition.
Add-Type -AssemblyName System.Drawing
$source=[Drawing.Image]::FromFile($cover)
$icon=New-Object Drawing.Bitmap(256,256)
$graphics=[Drawing.Graphics]::FromImage($icon)
try {
    $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.DrawImage($source,0,0,256,256)
    $icon.Save((Join-Path $root 'assets/branding/icon.png'),[Drawing.Imaging.ImageFormat]::Png)
} finally { $graphics.Dispose(); $icon.Dispose(); $source.Dispose() }
$utf8=New-Object Text.UTF8Encoding($false)
foreach ($file in @(Get-ChildItem $PSScriptRoot -File | Where-Object { $_.Extension -in '.ps1','.md' -and $_.Name -notin @('rebrand_2150.ps1','CHANGELOG_PLUS.md') })) {
    $text=[IO.File]::ReadAllText($file.FullName)
    $text=$text.Replace('MultiVoidKindaPlus','WatchersOfTheVoid').Replace('MultiVoid Kinda Plus','Watchers of the Void')
    [IO.File]::WriteAllText($file.FullName,$text,$utf8)
}
foreach ($file in @((Join-Path $workspace 'README.md'),(Join-Path $root 'README.md'))) {
    $text=[IO.File]::ReadAllText($file).Replace('MultiVoidKindaPlus','WatchersOfTheVoid').Replace('MultiVoid Kinda Plus','Watchers of the Void')
    [IO.File]::WriteAllText($file,$text,$utf8)
}
Write-Output 'Preserved prior branding/package; converted supplied cover to 256x256 PNG; renamed current package helpers.'
