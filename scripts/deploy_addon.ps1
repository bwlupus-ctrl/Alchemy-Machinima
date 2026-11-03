param(
    [string]$Built = "$PSScriptRoot\..\reshade-addon\build\Release\sl_reshade_bridge.addon",
    [Parameter(Mandatory=$true)][string]$Destination,
    [string]$ViewerBuildDirectory = "$PSScriptRoot\..\build-Windows-vs2026-os\newview"
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Built).Path
$target = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $target -PathType Container) { $target = Join-Path $target 'sl_reshade_bridge.addon' }
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$before = if (Test-Path -LiteralPath $target) { (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash } else { '(absent)' }
Write-Host "Built binary SHA256: $sourceHash"
Write-Host "Deployed before SHA256: $before"
$header = Join-Path (Split-Path -Parent (Split-Path -Parent $source)) 'sl_bridge_build_info.h'
$viewerHeader = Join-Path $ViewerBuildDirectory 'sl_bridge_build_info.h'
foreach ($entry in @(@('Add-on configured source', $header), @('Viewer expected source', $viewerHeader))) {
    $value = if (Test-Path -LiteralPath $entry[1]) {
        (Get-Content -LiteralPath $entry[1] | Select-String '^#define SL_EXPECTED_ADDON_SOURCE_SHA256').Line
    } else { '(unavailable; configure this component)' }
    Write-Host "$($entry[0]): $value"
}
if ($sourceHash -ne $before) { Copy-Item -LiteralPath $source -Destination $target -Force }
$after = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
Write-Host "Deployed after SHA256: $after"
if ($after -ne $sourceHash) { throw 'Add-on deploy hash mismatch (a running viewer may hold the file locked).' }
