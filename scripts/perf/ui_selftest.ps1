param(
  [string]$Exe = 'I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe',
  [string]$Floaters = 'all',
  [uint32]$Frames = 120,
  [switch]$AB,
  [int]$TimeoutMin = 10,
  [string]$Compare
)

$ErrorActionPreference = 'Stop'

if (Get-Process -Name 'AlchemyTest' -ErrorAction SilentlyContinue) {
  Write-Host 'AlchemyTest is already running; refusing to start a second viewer.' -ForegroundColor Red
  exit 2
}
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
  Write-Host "Viewer executable not found: $Exe" -ForegroundColor Red
  exit 2
}
if ($Floaters.Contains('"')) {
  Write-Host 'Floater list must not contain a double quote.' -ForegroundColor Red
  exit 2
}

# Keep in sync with all_targets in indra/newview/aluiperfselftest.cpp.
$allTargets = @('director', 'lightbox', 'exposure_hub', 'dance_ball',
  'cine_light_rig', 'env_adjust_snapshot', 'actor_gaze',
  'avatar_light_response', 'flycam_recorder', 'phototools',
  'cinematic_camera', 'ghost_studio', 'ultimate_diopter', 'pose_polish')
$targets = if ($Floaters -eq 'all') { $allTargets } else { @($Floaters.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }

$logsDir = Join-Path $env:APPDATA 'AlchemyMachinima\logs'
$csvPath = Join-Path $logsDir 'ui_perf_selftest.csv'
$logPath = Join-Path $logsDir 'Alchemy.log'
$dumpDir = Join-Path $env:LOCALAPPDATA 'CrashDumps'
$dumpTool = Join-Path $PSScriptRoot 'dumpstack.py'
$python = 'C:\Program Files\NVIDIA Corporation\Nsight Systems 2026.1.3\host-windows-x64\python\bin\python.exe'
if (-not (Test-Path -LiteralPath $python)) { $python = 'python' }
$crashes = [System.Collections.Generic.List[object]]::new()

while ($true) {
  $quotedFloaters = '"' + ($targets -join ',') + '"'
  $viewerArgs = @('--set', 'UIPerfSelfTest', $quotedFloaters,
            '--set', 'UIPerfSelfTestFrames', $Frames,
            '--set', 'UIPerfSelfTestAB', $(if ($AB) { 'TRUE' } else { 'FALSE' }))
  # The viewer resolves app_settings/ and skins/ from its working directory: always
  # start it in the exe's own folder, or it loads whatever install the caller is in.
  $process = Start-Process -FilePath $Exe -ArgumentList $viewerArgs -WorkingDirectory (Split-Path -Parent $Exe) -PassThru
  $null = $process.Handle   # keep a handle so ExitCode is filled in after exit
  $startUtc = $process.StartTime.ToUniversalTime()
  $deadline = [DateTime]::UtcNow.AddMinutes($TimeoutMin)
  while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $process.Refresh()
  }
  if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    Write-Host "UI self-test timed out after $TimeoutMin minutes; killed only PID $($process.Id)." -ForegroundColor Red
    exit 2
  }

  $csvFile = Get-Item -LiteralPath $csvPath -ErrorAction SilentlyContinue
  $logFile = Get-Item -LiteralPath $logPath -ErrorAction SilentlyContinue
  $lastBegin = $null
  $summary = $null
  if ($logFile -and $logFile.LastWriteTimeUtc -ge $startUtc) {
    $lastBegin = Select-String -LiteralPath $logFile.FullName -Pattern 'UIPerfSelfTest begin ' |
      Select-Object -Last 1
    $summary = Select-String -LiteralPath $logFile.FullName -Pattern 'UIPerfSelfTest summary complete' |
      Select-Object -Last 1
  }
  $complete = $summary -and (-not $lastBegin -or $summary.LineNumber -gt $lastBegin.LineNumber)
  if ($complete -and $csvFile -and $csvFile.LastWriteTimeUtc -ge $startUtc) {
    if ($process.ExitCode -ne 0) {
      Write-Host "Warning: the self-test completed but the viewer exited with $($process.ExitCode) during shutdown." -ForegroundColor Yellow
    }
    break
  }

  # Crash: name the floater/tab, symbolize the newest dump, drop that floater, rerun the rest.
  if (-not $lastBegin) {
    Write-Host "Viewer exit $($process.ExitCode) before any self-test target began (startup failure)." -ForegroundColor Red
    exit 2
  }
  $site = ($lastBegin.Line -replace '^.*UIPerfSelfTest begin ', '').Trim()
  $crashed = ($site -split '/')[0]
  $stack = @()
  $dump = Get-Item -LiteralPath (Join-Path $dumpDir "AlchemyTest.exe.$($process.Id).dmp") -ErrorAction SilentlyContinue
  if (-not $dump) {
    $dump = Get-ChildItem -LiteralPath $dumpDir -Filter 'AlchemyTest.exe.*.dmp' -ErrorAction SilentlyContinue |
      Where-Object { $_.LastWriteTimeUtc -ge $startUtc } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
  }
  if ($dump -and (Test-Path -LiteralPath $dumpTool)) {
    # Symbolizing is best effort: a python error must never abort the run.
    $savedPref = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
      $stack = & $python -I $dumpTool $dump.FullName $Exe 2>&1 | ForEach-Object { "$_" } |
        Where-Object { $_ -notmatch "``string'|_malloc_base|_free_base" } | Select-Object -First 14
    } catch {
      $stack = @("(stack unavailable: $($_.Exception.Message))")
    } finally {
      $ErrorActionPreference = $savedPref
    }
  }
  $crashes.Add([pscustomobject]@{ Floater = $crashed; Site = $site; ExitCode = $process.ExitCode; Dump = $(if ($dump) { $dump.Name } else { '' }); Stack = ($stack -join "`n") })
  Write-Host "CRASH in $site (exit $($process.ExitCode))" -ForegroundColor Red
  $stack | ForEach-Object { Write-Host "    $_" }
  $before = $targets.Count
  $targets = @($targets | Where-Object { $_ -ne $crashed })
  if (-not $targets.Count -or $targets.Count -eq $before) {
    Write-Host 'No measurable targets left.' -ForegroundColor Red
    exit 2
  }
  Write-Host "Rerunning without $crashed ($($targets.Count) left)..."
}

$destDir = 'C:\Users\xianw\cxs\ui_perf'
New-Item -ItemType Directory -Path $destDir -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$dest = Join-Path $destDir "ui_perf_$stamp.csv"
Copy-Item -LiteralPath $csvFile.FullName -Destination $dest
$rows = Import-Csv -LiteralPath $dest
$baselines = @{}
foreach ($row in ($rows | Where-Object measure -eq 'baseline')) {
  $baselines[$row.pass] = $row
}

$previous = @{}
if ($Compare) {
  foreach ($row in (Import-Csv -LiteralPath $Compare)) {
    $previous["$($row.pass)|$($row.floater)|$($row.tab_path)|$($row.measure)"] = $row
  }
}

$ranked = foreach ($row in ($rows | Where-Object { $_.measure -ne 'baseline' })) {
  $base = $baselines[$row.pass]
  $meanDelta = [double]$row.ui_mean_ms - [double]$base.ui_mean_ms
  $p95Delta = [double]$row.ui_p95_ms - [double]$base.ui_p95_ms
  $reasons = [System.Collections.Generic.List[string]]::new()
  if ($meanDelta -gt 1.0) { $reasons.Add('mean>baseline+1ms') }
  if ($p95Delta -gt 2.0) { $reasons.Add('p95>baseline+2ms') }
  if ($row.measure -eq 'open' -and
      ([double]$row.action_ms -gt 100.0 -or [double]$row.first_frame_ui_ms -gt 100.0)) {
    $reasons.Add('open>100ms')
  }
  if ($row.measure -eq 'tab' -and [double]$row.action_ms -gt 16.0) { $reasons.Add('tab>16ms') }
  $key = "$($row.pass)|$($row.floater)|$($row.tab_path)|$($row.measure)"
  if ($previous.ContainsKey($key)) {
    $old = [double]$previous[$key].ui_mean_ms
    if ($old -gt 0 -and [double]$row.ui_mean_ms -gt $old * 1.2) { $reasons.Add('regression>20%') }
  }
  [pscustomobject]@{
    Pass = $row.pass
    Floater = $row.floater
    Tab = $row.tab_path
    Measure = $row.measure
    MeanDeltaMs = [math]::Round($meanDelta, 3)
    P95DeltaMs = [math]::Round($p95Delta, 3)
    ActionMs = [math]::Round([double]$row.action_ms, 3)
    FirstFrameMs = [math]::Round([double]$row.first_frame_ui_ms, 3)
    Flag = ($reasons -join ';')
  }
}
$ranked | Sort-Object MeanDeltaMs -Descending | Format-Table -AutoSize

if ($AB) {
  Write-Host "`nClip-cull on/off mean delta (positive = off is slower):"
  $on = @{}
  foreach ($row in ($rows | Where-Object pass -eq 'clip_on')) {
    $on["$($row.floater)|$($row.tab_path)|$($row.measure)"] = $row
  }
  $deltas = foreach ($row in ($rows | Where-Object pass -eq 'clip_off')) {
    $key = "$($row.floater)|$($row.tab_path)|$($row.measure)"
    if ($on.ContainsKey($key)) {
      [pscustomobject]@{
        Floater = $row.floater
        Tab = $row.tab_path
        Measure = $row.measure
        OffMinusOnMs = [math]::Round([double]$row.ui_mean_ms - [double]$on[$key].ui_mean_ms, 3)
      }
    }
  }
  $deltas | Sort-Object OffMinusOnMs -Descending | Format-Table -AutoSize
}

Write-Host "Saved: $dest"
if ($crashes.Count) {
  Write-Host "`nCRASHED (excluded from the numbers above):" -ForegroundColor Red
  $crashes | ForEach-Object { Write-Host "  $($_.Site)  exit $($_.ExitCode)  $($_.Dump)" -ForegroundColor Red; Write-Host ($_.Stack -replace '(?m)^', '      ') }
  exit 3
}
$flagged = @($ranked | Where-Object { $_.Flag })
if ($flagged.Count) {
  Write-Host "FLAGGED: $($flagged.Count) measurement(s)" -ForegroundColor Red
  exit 1
}
Write-Host 'No thresholds flagged.' -ForegroundColor Green
exit 0
