param(
    [ValidateSet("User", "System")]
    [string]$Scope = "User",
    [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$localBundledVst3 = Join-Path $PSScriptRoot "HarmonizerV.vst3"
$repoBuiltVst3 = Join-Path $PSScriptRoot "..\$BuildDir\HarmonizerV_artefacts\Release\VST3\HarmonizerV.vst3"

if (Test-Path $localBundledVst3) {
    $sourceVst3 = $localBundledVst3
} elseif (Test-Path $repoBuiltVst3) {
    $sourceVst3 = $repoBuiltVst3
} else {
    Write-Error "Compiled VST3 not found at $localBundledVst3 or $repoBuiltVst3. Build Release first."
    exit 1
}

if ($Scope -eq "System") {
    $targetDir = "C:\Program Files\Common Files\VST3"
} else {
    $targetDir = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
}

if (-not (Test-Path $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
}

$destVst3 = Join-Path $targetDir "HarmonizerV.vst3"
try {
    if (Test-Path $destVst3) {
        Remove-Item -Path $destVst3 -Recurse -Force
    }
    Copy-Item -Path $sourceVst3 -Destination $destVst3 -Recurse -Force
} catch {
    Write-Error "Could not install VST3 to $destVst3. If your DAW is open, close it first and retry. Details: $($_.Exception.Message)"
    exit 1
}

Write-Host "Installed HarmonizerV.vst3 ($Scope) to $destVst3"
