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

$quotedFloaters = '"' + $Floaters + '"'
$args = @('--set', 'UIPerfSelfTest', $quotedFloaters,
          '--set', 'UIPerfSelfTestFrames', $Frames,
          '--set', 'UIPerfSelfTestAB', $(if ($AB) { 'TRUE' } else { 'FALSE' }))
$process = Start-Process -FilePath $Exe -ArgumentList $args -PassThru
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

$logsDir = Join-Path $env:APPDATA 'AlchemyMachinima\logs'
$csvPath = Join-Path $logsDir 'ui_perf_selftest.csv'
$logPath = Join-Path $logsDir 'Alchemy.log'
$csvFile = Get-Item -LiteralPath $csvPath -ErrorAction SilentlyContinue
$logFile = Get-Item -LiteralPath $logPath -ErrorAction SilentlyContinue
$lastBegin = $null
$summary = $null
if ($logFile -and $logFile.LastWriteTimeUtc -ge $process.StartTime.ToUniversalTime()) {
  $lastBegin = Select-String -LiteralPath $logFile.FullName -Pattern 'UIPerfSelfTest begin ' |
    Select-Object -Last 1
  $summary = Select-String -LiteralPath $logFile.FullName -Pattern 'UIPerfSelfTest summary complete' |
    Select-Object -Last 1
}
$complete = $summary -and (-not $lastBegin -or $summary.LineNumber -gt $lastBegin.LineNumber)
if ($process.ExitCode -ne 0 -or -not $complete -or -not $csvFile -or
    $csvFile.LastWriteTimeUtc -lt $process.StartTime.ToUniversalTime()) {
  if ($lastBegin) {
    Write-Host "Viewer exit $($process.ExitCode) without a completed self-test summary. Last crash-site marker: $($lastBegin.Line.Trim())" -ForegroundColor Red
  } else {
    Write-Host "Viewer exit $($process.ExitCode) without producing a completed self-test summary and fresh CSV." -ForegroundColor Red
  }
  exit 2
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
$flagged = @($ranked | Where-Object { $_.Flag })
if ($flagged.Count) {
  Write-Host "FLAGGED: $($flagged.Count) measurement(s)" -ForegroundColor Red
  exit 1
}
Write-Host 'No thresholds flagged.' -ForegroundColor Green
exit 0
