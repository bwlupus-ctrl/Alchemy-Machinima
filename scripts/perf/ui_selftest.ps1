param(
  [string]$Exe = 'I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe',
  [string]$Floaters = 'all',
  [uint32]$Frames = 120,
  [uint32]$Inventory = 6000,
  [switch]$AB,
  [switch]$Layout,
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
if ($Layout -and $TimeoutMin -eq 10) { $TimeoutMin = 25 }

# Keep in sync with all_targets in indra/newview/aluiperfselftest.cpp.
$allTargets = @('director', 'lightbox', 'exposure_hub', 'dance_ball',
  'cine_light_rig', 'env_adjust_snapshot', 'actor_gaze',
  'avatar_light_response', 'flycam_recorder', 'phototools',
  'cinematic_camera', 'ghost_studio', 'ultimate_diopter', 'pose_polish',
  'inventory_stress')
$targets = if ($Floaters -eq 'all') { $allTargets } else { @($Floaters.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }

$logsDir = Join-Path $env:APPDATA 'AlchemyMachinima\logs'
$csvPath = Join-Path $logsDir 'ui_perf_selftest.csv'
$layoutCsvPath = Join-Path $logsDir 'ui_layout_audit.csv'
$layoutShotsPath = Join-Path $logsDir 'ui_audit'
$logPath = Join-Path $logsDir 'Alchemy.log'
$dumpDir = Join-Path $env:LOCALAPPDATA 'CrashDumps'
$dumpTool = Join-Path $PSScriptRoot 'dumpstack.py'
$python = 'C:\Program Files\NVIDIA Corporation\Nsight Systems 2026.1.3\host-windows-x64\python\bin\python.exe'
if (-not (Test-Path -LiteralPath $python)) { $python = 'python' }
$crashes = [System.Collections.Generic.List[object]]::new()

while ($true) {
  if ($Layout) {
    $resolvedLogs = [System.IO.Path]::GetFullPath($logsDir).TrimEnd('\') + '\'
    $resolvedShots = [System.IO.Path]::GetFullPath($layoutShotsPath)
    if (-not $resolvedShots.StartsWith($resolvedLogs, [System.StringComparison]::OrdinalIgnoreCase)) {
      throw "Refusing to clear screenshot path outside logs directory: $resolvedShots"
    }
    if (Test-Path -LiteralPath $layoutShotsPath) {
      Remove-Item -LiteralPath $layoutShotsPath -Recurse -Force
    }
    Remove-Item -LiteralPath $layoutCsvPath -Force -ErrorAction SilentlyContinue
  }
  $quotedFloaters = '"' + ($targets -join ',') + '"'
  $viewerArgs = @('--set', 'UIPerfSelfTest', $quotedFloaters,
            '--set', 'UIPerfSelfTestFrames', $Frames,
            '--set', 'UIPerfInventoryStress', $(if ($targets -contains 'inventory_stress') { $Inventory } else { 0 }),
            '--set', 'UIPerfSelfTestAB', $(if ($AB) { 'TRUE' } else { 'FALSE' }),
            '--set', 'UILayoutAudit', $(if ($Layout) { 'TRUE' } else { 'FALSE' }))
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
  $layoutCsvFile = Get-Item -LiteralPath $layoutCsvPath -ErrorAction SilentlyContinue
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
  $layoutComplete = (-not $Layout) -or
    ($layoutCsvFile -and $layoutCsvFile.LastWriteTimeUtc -ge $startUtc)
  if ($complete -and $layoutComplete -and $csvFile -and $csvFile.LastWriteTimeUtc -ge $startUtc) {
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
    # Sub-millisecond pages jitter by ~0.05 ms run to run: require a real absolute change too.
    if ($old -gt 0 -and [double]$row.ui_mean_ms -gt $old * 1.2 -and
        ([double]$row.ui_mean_ms - $old) -gt 0.15) { $reasons.Add('regression>20%') }
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
  Write-Host "`nFolder-view cull on/off mean delta (positive = off is slower):"
  $on = @{}
  foreach ($row in ($rows | Where-Object pass -eq 'folder_cull_on')) {
    $on["$($row.floater)|$($row.tab_path)|$($row.measure)"] = $row
  }
  $deltas = foreach ($row in ($rows | Where-Object pass -eq 'folder_cull_off')) {
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
$layoutFindingCount = 0
if ($Layout) {
  $layoutRoot = 'C:\Users\xianw\cxs\ui_audit'
  $layoutDest = Join-Path $layoutRoot $stamp
  New-Item -ItemType Directory -Path $layoutDest -Force | Out-Null
  $layoutCsvDest = Join-Path $layoutDest 'ui_layout_audit.csv'
  Copy-Item -LiteralPath $layoutCsvFile.FullName -Destination $layoutCsvDest
  $shotsDest = Join-Path $layoutDest 'ui_audit'
  if (Test-Path -LiteralPath $layoutShotsPath -PathType Container) {
    Copy-Item -LiteralPath $layoutShotsPath -Destination $shotsDest -Recurse
  } else {
    New-Item -ItemType Directory -Path $shotsDest -Force | Out-Null
  }

  $layoutRows = @(Import-Csv -LiteralPath $layoutCsvDest)
  $layoutFindingCount = $layoutRows.Count
  Write-Host "`nLayout findings by floater/tab:"
  if ($layoutFindingCount) {
    $layoutRows | Group-Object size_pass, floater, tab_path |
      Sort-Object Count -Descending | ForEach-Object {
        [pscustomobject]@{ Count = $_.Count; Page = $_.Name }
      } | Format-Table -AutoSize
    Write-Host 'Layout findings by type:'
    $layoutRows | Group-Object finding | Sort-Object Count -Descending |
      Select-Object Count, Name | Format-Table -AutoSize
  } else {
    Write-Host '  none' -ForegroundColor Green
  }

  function ConvertTo-AuditSafeName([string]$Value) {
    $safe = [regex]::Replace($Value, '[^A-Za-z0-9_-]', '_')
    while ($safe.Contains('__')) { $safe = $safe.Replace('__', '_') }
    if (-not $safe) { $safe = 'default' }
    if ($safe.Length -gt 100) { $safe = $safe.Substring(0, 100) }
    return $safe
  }

  $html = [System.Text.StringBuilder]::new()
  [void]$html.AppendLine('<!doctype html><html><head><meta charset="utf-8"><title>UI layout audit</title>')
  [void]$html.AppendLine('<style>body{font:14px system-ui;background:#181a1b;color:#eee;margin:20px}h1,h2,h3{margin:.7em 0}.page{border-top:1px solid #555;margin-top:24px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:12px}.grid img{width:100%;height:auto;background:#000;border:1px solid #666}li{margin:.3em 0}.ok{color:#8fd18f}.finding{color:#ffb0a8}code{color:#d7d7ff}</style></head><body>')
  [void]$html.AppendLine("<h1>UI layout audit $([System.Net.WebUtility]::HtmlEncode($stamp))</h1>")
  $pageMap = @{}
  foreach ($finding in $layoutRows) {
    $floaterSafe = ConvertTo-AuditSafeName $finding.floater
    $pageIndex = [int]$finding.page_index
    $key = "$($finding.size_pass)|$floaterSafe|$pageIndex"
    if (-not $pageMap.ContainsKey($key)) {
      $pageMap[$key] = [pscustomobject]@{
        SizePass = $finding.size_pass; Floater = $finding.floater
        FloaterSafe = $floaterSafe; PageIndex = $pageIndex; TabPath = $finding.tab_path
        Shots = [System.Collections.Generic.List[object]]::new()
        Findings = [System.Collections.Generic.List[object]]::new()
      }
    }
    $pageMap[$key].Findings.Add($finding)
  }
  $shotFiles = @(Get-ChildItem -LiteralPath $shotsDest -Filter '*.png' -Recurse -File -ErrorAction SilentlyContinue)
  foreach ($shot in $shotFiles) {
    $relativeDir = $shot.DirectoryName.Substring($shotsDest.Length).TrimStart('\')
    $parts = @($relativeDir -split '\\')
    if ($parts.Count -lt 2 -or $shot.BaseName -notmatch '^(?<page>[0-9]{3})_(?<tab>.*)_(?<shot>[0-9]+)$') { continue }
    $sizePass = $parts[0]
    $floaterSafe = $parts[1]
    $pageIndex = [int]$Matches.page
    $key = "$sizePass|$floaterSafe|$pageIndex"
    if (-not $pageMap.ContainsKey($key)) {
      $pageMap[$key] = [pscustomobject]@{
        SizePass = $sizePass; Floater = $floaterSafe
        FloaterSafe = $floaterSafe; PageIndex = $pageIndex; TabPath = $Matches.tab
        Shots = [System.Collections.Generic.List[object]]::new()
        Findings = [System.Collections.Generic.List[object]]::new()
      }
    }
    $pageMap[$key].Shots.Add($shot)
  }
  foreach ($page in @($pageMap.Values | Sort-Object SizePass, Floater, PageIndex)) {
    [void]$html.AppendLine('<section class="page">')
    $heading = [System.Net.WebUtility]::HtmlEncode("$($page.SizePass) / $($page.Floater) / $($page.TabPath)")
    [void]$html.AppendLine(('<h2>{0}</h2><div class="grid">' -f $heading))
    foreach ($shot in @($page.Shots | Sort-Object Name)) {
      $relativeImage = $shot.FullName.Substring($layoutDest.Length).TrimStart('\').Replace('\', '/')
      $imageHref = [System.Net.WebUtility]::HtmlEncode($relativeImage)
      $imageAlt = [System.Net.WebUtility]::HtmlEncode($shot.Name)
      [void]$html.AppendLine(('<a href="{0}"><img loading="lazy" src="{0}" alt="{1}"></a>' -f $imageHref, $imageAlt))
    }
    [void]$html.AppendLine('</div>')
    if (-not $page.Shots.Count) {
      [void]$html.AppendLine('<p class="finding">No screenshot was captured.</p>')
    }
    if ($page.Findings.Count) {
      [void]$html.AppendLine('<ul class="finding">')
      foreach ($finding in $page.Findings) {
        $description = "$($finding.finding): $($finding.widget_a) $($finding.widget_b) - $($finding.detail)"
        [void]$html.AppendLine("<li>$([System.Net.WebUtility]::HtmlEncode($description))</li>")
      }
      [void]$html.AppendLine('</ul>')
    } else {
      [void]$html.AppendLine('<p class="ok">No findings.</p>')
    }
    [void]$html.AppendLine('</section>')
  }
  [void]$html.AppendLine('</body></html>')
  $indexPath = Join-Path $layoutDest 'index.html'
  [System.IO.File]::WriteAllText($indexPath, $html.ToString(), [System.Text.UTF8Encoding]::new($false))
  Write-Host "Layout audit saved: $layoutDest"
}
if ($crashes.Count) {
  Write-Host "`nCRASHED (excluded from the numbers above):" -ForegroundColor Red
  $crashes | ForEach-Object { Write-Host "  $($_.Site)  exit $($_.ExitCode)  $($_.Dump)" -ForegroundColor Red; Write-Host ($_.Stack -replace '(?m)^', '      ') }
}
$layoutFlagged = $Layout -and $layoutFindingCount -gt 0
if ($layoutFlagged) {
  Write-Host "LAYOUT FLAGGED: $layoutFindingCount finding(s)" -ForegroundColor Red
} elseif ($Layout) {
  Write-Host 'LAYOUT: no findings.' -ForegroundColor Green
}
$flagged = @($ranked | Where-Object { $_.Flag })
if ($flagged.Count) {
  Write-Host "FLAGGED: $($flagged.Count) measurement(s)" -ForegroundColor Red
} else {
  Write-Host 'No performance thresholds flagged.' -ForegroundColor Green
}
if ($crashes.Count) { exit 3 }
if ($flagged.Count) { exit 1 }
if ($layoutFlagged) { exit 4 }
exit 0
