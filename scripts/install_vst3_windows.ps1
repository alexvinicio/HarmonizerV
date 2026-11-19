param(
    [string]$BuildDir = "build"
)

$sourceVst3 = Join-Path $PSScriptRoot "..\$BuildDir\HarmonizerV_artefacts\Release\VST3\HarmonizerV.vst3"
if (-not (Test-Path $sourceVst3)) {
    Write-Error "Compiled VST3 not found at $sourceVst3. Build Release first."
    exit 1
}

$targetDir = "C:\Program Files\Common Files\VST3"
if (-not (Test-Path $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
}

$destVst3 = Join-Path $targetDir "HarmonizerV.vst3"
if (Test-Path $destVst3) {
    Remove-Item -Path $destVst3 -Recurse -Force
}

Copy-Item -Path $sourceVst3 -Destination $destVst3 -Recurse -Force
Write-Host "Installed HarmonizerV.vst3 to $destVst3"
