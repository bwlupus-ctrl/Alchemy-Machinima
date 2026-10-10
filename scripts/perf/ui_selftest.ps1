param(
  [string]$Exe = 'I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe',
  [string]$Floaters = 'all',
  [uint32]$Frames = 120,
  [uint32]$Inventory = 6000,
  [switch]$AB,
  [switch]$Layout,
  [switch]$DirectorV2,
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
if ($DirectorV2) {
  $Layout = $true
  $Floaters = 'director'
}
if ($DirectorV2 -and $TimeoutMin -eq 10) { $TimeoutMin = 75 }
elseif ($Layout -and $TimeoutMin -eq 10) { $TimeoutMin = 25 }

# Keep in sync with all_targets in indra/newview/aluiperfselftest.cpp.
$allTargets = @('director', 'lightbox', 'exposure_hub', 'dance_ball',
  'cine_light_rig', 'env_adjust_snapshot', 'actor_gaze',
  'avatar_light_response', 'flycam_recorder', 'phototools',
  'cinematic_camera', 'ghost_studio', 'ultimate_diopter', 'pose_polish',
  'inventory_stress')
$targets = if ($Floaters -eq 'all') { $allTargets } else { @($Floaters.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
$inventoryRequested = $Inventory -gt 0 -and $targets -contains 'inventory_stress'

$logsDir = Join-Path $env:APPDATA 'AlchemyMachinima\logs'
$csvPath = Join-Path $logsDir 'ui_perf_selftest.csv'
$layoutCsvPath = Join-Path $logsDir 'ui_layout_audit.csv'
$layoutStatesPath = Join-Path $logsDir 'ui_layout_audit_states.csv'
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
    if ($DirectorV2) { Remove-Item -LiteralPath $layoutStatesPath -Force -ErrorAction SilentlyContinue }
  }
  $quotedFloaters = '"' + ($targets -join ',') + '"'
  $viewerArgs = @('--set', 'UIPerfSelfTest', $quotedFloaters,
            '--set', 'UIPerfSelfTestFrames', $Frames,
            '--set', 'UIPerfInventoryStress', $(if ($targets -contains 'inventory_stress') { $Inventory } else { 0 }),
            '--set', 'UIPerfSelfTestAB', $(if ($AB) { 'TRUE' } else { 'FALSE' }),
            '--set', 'UILayoutAudit', $(if ($Layout) { 'TRUE' } else { 'FALSE' }))
  if ($DirectorV2) {
    $viewerArgs += @('--set', 'DirectorUIV2', 'TRUE',
      '--set', 'UIPerfDirectorV2', 'TRUE',
      '--set', 'UILayoutAuditSizes', '"1160x720,min,1141x720,915x720"')
  }
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
  $layoutStatesFile = Get-Item -LiteralPath $layoutStatesPath -ErrorAction SilentlyContinue
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
    ($layoutCsvFile -and $layoutCsvFile.LastWriteTimeUtc -ge $startUtc -and
      (-not $DirectorV2 -or ($layoutStatesFile -and $layoutStatesFile.LastWriteTimeUtc -ge $startUtc)))
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
$inventoryMissing = @()
if ($inventoryRequested) {
  $inventoryMeasures = @('open', 'scroll_top', 'scroll_middle', 'scroll_bottom',
    'scroll_sweep', 'filter_a', 'hover_sweep')
  $inventoryPasses = if ($AB) {
    @('folder_cull_on', 'folder_cull_off', 'folder_cull_on_2')
  } else {
    @($rows | Where-Object floater -eq 'inventory_stress' | Select-Object -ExpandProperty pass -Unique)
  }
  if (-not $inventoryPasses.Count) { $inventoryPasses = @('(no inventory pass)') }
  foreach ($pass in $inventoryPasses) {
    $emitted = @($rows | Where-Object { $_.pass -eq $pass -and $_.floater -eq 'inventory_stress' } |
      Select-Object -ExpandProperty measure -Unique)
    foreach ($measure in $inventoryMeasures) {
      if ($emitted -notcontains $measure) { $inventoryMissing += "$pass/$measure" }
    }
  }
  if ($inventoryMissing.Count) {
    Write-Host "INVENTORY FIXTURE INCOMPLETE: missing measure row(s): $($inventoryMissing -join ', ')" -ForegroundColor Red
  }
}
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
  if (-not $DirectorV2 -and $row.measure -eq 'open' -and
      ([double]$row.action_ms -gt 100.0 -or [double]$row.first_frame_ui_ms -gt 100.0)) {
    $reasons.Add('open>100ms')
  }
  if (-not $DirectorV2 -and $row.measure -eq 'tab' -and [double]$row.action_ms -gt 16.0) { $reasons.Add('tab>16ms') }
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
  Write-Host "`nFolder-view cull on/off mean delta (positive = off is slower; inventory ON is ON1/ON2 mean):"
  $on1 = @{}
  foreach ($row in ($rows | Where-Object pass -eq 'folder_cull_on')) {
    $on1["$($row.floater)|$($row.tab_path)|$($row.measure)"] = $row
  }
  $on2 = @{}
  foreach ($row in ($rows | Where-Object pass -eq 'folder_cull_on_2')) {
    $on2["$($row.floater)|$($row.tab_path)|$($row.measure)"] = $row
  }
  $deltas = foreach ($row in ($rows | Where-Object pass -eq 'folder_cull_off')) {
    $key = "$($row.floater)|$($row.tab_path)|$($row.measure)"
    if ($on1.ContainsKey($key)) {
      $onMean = [double]$on1[$key].ui_mean_ms
      $onSamples = 1
      if ($on2.ContainsKey($key)) {
        $onMean = ($onMean + [double]$on2[$key].ui_mean_ms) / 2.0
        $onSamples = 2
      }
      [pscustomobject]@{
        Floater = $row.floater
        Tab = $row.tab_path
        Measure = $row.measure
        OnSamples = $onSamples
        OffMinusOnMs = [math]::Round([double]$row.ui_mean_ms - $onMean, 3)
      }
    }
  }
  $deltas | Sort-Object OffMinusOnMs -Descending | Format-Table -AutoSize
  $drift = foreach ($key in $on2.Keys) {
    if ($on1.ContainsKey($key)) {
      $parts = $key.Split('|', 3)
      [pscustomobject]@{
        Floater = $parts[0]
        Tab = $parts[1]
        Measure = $parts[2]
        On2MinusOn1Ms = [math]::Round([double]$on2[$key].ui_mean_ms - [double]$on1[$key].ui_mean_ms, 3)
      }
    }
  }
  if ($drift) {
    Write-Host "`nFolder-view cull ON1 vs ON2 drift (positive = ON2 is slower):"
    $drift | Sort-Object On2MinusOn1Ms -Descending | Format-Table -AutoSize
  }
}

Write-Host "Saved: $dest"
$layoutFindingCount = 0
if ($Layout) {
  $layoutRoot = 'C:\Users\xianw\cxs\ui_audit'
  $layoutDest = Join-Path $layoutRoot $stamp
  New-Item -ItemType Directory -Path $layoutDest -Force | Out-Null
  $layoutCsvDest = Join-Path $layoutDest 'ui_layout_audit.csv'
  Copy-Item -LiteralPath $layoutCsvFile.FullName -Destination $layoutCsvDest
  $layoutStates = @()
  if ($DirectorV2) {
    $layoutStatesDest = Join-Path $layoutDest 'ui_layout_audit_states.csv'
    Copy-Item -LiteralPath $layoutStatesFile.FullName -Destination $layoutStatesDest
    $layoutStates = @(Import-Csv -LiteralPath $layoutStatesDest)
  }
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
    if ($DirectorV2) {
      if ($parts.Count -lt 2 -or $parts[0] -ne 'director' -or $parts[1] -notlike 'audit-*') { continue }
      if ($shot.BaseName -notmatch '^(?<tab>.*)__(?<page>[0-9]{3})__(?<state>.*)__(?<shot>[0-9]+)$') { continue }
      $sizePass = $parts[1].Substring(6)
      $floaterSafe = 'director'
    } else {
      if ($parts.Count -lt 2) { continue }
      if ($shot.BaseName -notmatch '^(?<page>[0-9]{3})_(?<tab>.*)_(?<shot>[0-9]+)$') { continue }
      $sizePass = $parts[0]
      $floaterSafe = $parts[1]
    }
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
$directorBudgetFailed = $false
if ($DirectorV2) {
  function Write-DirectorBudgetLine([string]$Name, [string]$Value, [bool]$Pass) {
    $status = if ($Pass) { 'PASS' } else { 'FAIL' }
    $colour = if ($Pass) { 'Green' } else { 'Red' }
    Write-Host ("  {0}: {1} [{2}]" -f $Name, $Value, $status) -ForegroundColor $colour
  }

  $openRows = @($rows | Where-Object { $_.floater -eq 'director' -and $_.measure -eq 'open' })
  $openMs = if ($openRows.Count) { [double](($openRows | Measure-Object action_ms -Maximum).Maximum) } else { [double]::PositiveInfinity }
  $openPass = $openMs -le 120.0
  Write-DirectorBudgetLine 'director open action ms <= 120' $(if ([double]::IsPositiveInfinity($openMs)) { 'missing' } else { '{0:N3} ms' -f $openMs }) $openPass

  $steadyRows = @($rows | Where-Object { $_.floater -eq 'director' -and $_.measure -in @('steady', 'tab') })
  $steadyDeltas = @($steadyRows | ForEach-Object {
      [double]$_.ui_mean_ms - [double]$baselines[$_.pass].ui_mean_ms
    })
  $steadyMs = if ($steadyDeltas.Count) { [double](($steadyDeltas | Measure-Object -Maximum).Maximum) } else { [double]::PositiveInfinity }
  $steadyPass = $steadyMs -le 0.5
  Write-DirectorBudgetLine 'worst steady page mean <= 0.5 ms (target 0.4)' $(if ([double]::IsPositiveInfinity($steadyMs)) { 'missing' } else { '{0:N3} ms above baseline' -f $steadyMs }) $steadyPass

  $buildRows = @($rows | Where-Object { $_.floater -eq 'director' -and $_.measure -eq 'kit_build' })
  $buildMs = if ($buildRows.Count) { [double](($buildRows | Measure-Object action_ms -Maximum).Maximum) } else { [double]::PositiveInfinity }
  $buildPass = $buildMs -le 50.0
  Write-DirectorBudgetLine 'any kit page first reveal <= 50 ms (target 30)' $(if ([double]::IsPositiveInfinity($buildMs)) { 'missing' } else { '{0:N3} ms' -f $buildMs }) $buildPass

  $searchRows = @($rows | Where-Object measure -eq 'kit_search')
  $searchMs = if ($searchRows.Count) { [double](($searchRows | Measure-Object action_ms -Maximum).Maximum) } else { [double]::PositiveInfinity }
  $searchPass = $searchMs -le 5.0
  Write-DirectorBudgetLine 'kit_search max <= 5 ms (target 2)' $(if ([double]::IsPositiveInfinity($searchMs)) { 'missing' } else { '{0:N3} ms' -f $searchMs }) $searchPass

  $amberRows = @($rows | Where-Object measure -eq 'kit_amber_storm')
  $amberMs = if ($amberRows.Count) { [double](($amberRows | Measure-Object action_ms -Maximum).Maximum) } else { [double]::PositiveInfinity }
  $amberPass = $amberMs -le 2.0
  Write-DirectorBudgetLine 'kit_amber_storm <= 2 ms' $(if ([double]::IsPositiveInfinity($amberMs)) { 'missing' } else { '{0:N3} ms' -f $amberMs }) $amberPass

  $hoverRows = @($rows | Where-Object { $_.floater -eq 'director' -and $_.measure -eq 'hover_sweep' -and $_.tab_path -like '*cine_light_rig_tab*' })
  $hoverDeltas = @($hoverRows | ForEach-Object {
      [math]::Max(0.0, [double]$_.find_calls_mean - [double]$baselines[$_.pass].find_calls_mean)
    })
  $hoverFinds = if ($hoverDeltas.Count) { [double](($hoverDeltas | Measure-Object -Maximum).Maximum) } else { [double]::PositiveInfinity }
  $hoverPass = $hoverFinds -eq 0.0
  Write-DirectorBudgetLine 'Director Rig hover find calls per frame = 0' $(if ([double]::IsPositiveInfinity($hoverFinds)) { 'missing' } else { '{0:N3}' -f $hoverFinds }) $hoverPass

  $kitFindings = @($layoutRows | Where-Object {
    $_.tab_path -match '^(default|.*cine_light_rig_tab|.*light_rim_tab)@'
  })
  $requiredSizes = @('1160x720', 'minimum', '1141x720', '915x720')
  $coverageMissing = [System.Collections.Generic.List[string]]::new()
  foreach ($size in $requiredSizes) {
    $sizeStates = @($layoutStates | Where-Object { $_.size_pass -eq $size -and $_.applied -eq '1' })
    foreach ($pattern in @('default@default', '*cine_light_rig_tab@mode-Advanced',
        '*cine_light_rig_tab@mode-Easy', '*cine_light_rig_tab@chip-*',
        '*cine_light_rig_tab@selector-*-Key', '*cine_light_rig_tab@selector-*-Fill',
        '*cine_light_rig_tab@selector-*-Back', '*cine_light_rig_tab@selector-*-BG',
        '*cine_light_rig_tab@card-*-expanded', '*cine_light_rig_tab@card-*-closed',
        '*cine_light_rig_tab@card-*-more', '*cine_light_rig_tab@master-on',
        '*cine_light_rig_tab@master-off', '*cine_light_rig_tab@map-top',
        '*cine_light_rig_tab@map-side-left', '*cine_light_rig_tab@map-side-right',
        '*cine_light_rig_tab@map-2_5d', '*light_rim_tab@chip-*',
        '*light_rim_tab@selector-*-Key', '*light_rim_tab@selector-*-Fill',
        '*light_rim_tab@selector-*-Back', '*light_rim_tab@selector-*-BG',
        '*light_rim_tab@card-*-expanded', '*light_rim_tab@card-*-closed',
        '*light_rim_tab@card-*-more', '*light_rim_tab@master-on',
        '*light_rim_tab@master-off')) {
      if (-not @($sizeStates | Where-Object tab_path -Like $pattern).Count) {
        $coverageMissing.Add("$size/$pattern")
      }
    }
    foreach ($pageGroup in @($layoutStates | Where-Object size_pass -eq $size |
        Group-Object { $_.tab_path -replace '@.*$','' })) {
      $expected = [int](($pageGroup.Group | ForEach-Object { [int]$_.enumerated } |
          Measure-Object -Maximum).Maximum)
      $applied = @($pageGroup.Group | Where-Object applied -eq '1').Count
      if ($pageGroup.Count -ne $expected -or $applied -ne $expected) {
        $coverageMissing.Add("$size/$($pageGroup.Name): $applied/$expected applied")
      }
    }
  }
  $kitLayoutPass = $kitFindings.Count -eq 0 -and $coverageMissing.Count -eq 0
  $kitLayoutValue = "$($kitFindings.Count) finding(s), $($coverageMissing.Count) missing state capture(s)"
  Write-DirectorBudgetLine 'layout findings shell + Light > Rig + Light > Rim, all states at all 4 sizes = 0' $kitLayoutValue $kitLayoutPass

  $legacyRows = @($layoutRows | Where-Object {
    $_.size_pass -in @('1160x720', 'minimum') -and
    $_.tab_path -notmatch '^(default|.*cine_light_rig_tab|.*light_rim_tab)@'
  })
  $baselineLayoutPath = $null
  if ($Compare) {
    $compareItem = Get-Item -LiteralPath $Compare -ErrorAction SilentlyContinue
    if ($compareItem) {
      $candidate = if ($compareItem.Name -eq 'ui_layout_audit.csv') {
        $compareItem.FullName
      } else {
        Join-Path $compareItem.DirectoryName 'ui_layout_audit.csv'
      }
      if (Test-Path -LiteralPath $candidate -PathType Leaf) { $baselineLayoutPath = $candidate }
      if (-not $baselineLayoutPath -and $compareItem.BaseName -match '^ui_perf_(?<stamp>[0-9]{8}_[0-9]{6})$') {
        $candidate = Join-Path 'C:\Users\xianw\cxs\ui_audit' "$($Matches.stamp)\ui_layout_audit.csv"
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { $baselineLayoutPath = $candidate }
      }
    }
  }
  $legacyNew = @()
  if ($baselineLayoutPath) {
    function Normalize-LegacySize([string]$Size) {
      if ($Size -in @('1160x720', 'default')) { return 'default' }
      if ($Size -in @('850x560', 'minimum', 'min')) { return 'minimum' }
      return $Size
    }
    function Normalize-LegacyWidget([string]$Path) {
      return ($Path -replace '^[^/]+_content/[^/]+_legacy_host/', '')
    }
    $baselineKeys = @{}
    foreach ($finding in @(Import-Csv -LiteralPath $baselineLayoutPath)) {
      $key = "$(Normalize-LegacySize $finding.size_pass)|$(($finding.tab_path -replace '@default$','') -replace ' \[[^\]]*\]', '')|$($finding.finding)|$(Normalize-LegacyWidget $finding.widget_a)|$(Normalize-LegacyWidget $finding.widget_b)"
      $baselineKeys[$key] = $true
    }
    $legacyNew = @($legacyRows | Where-Object {
      $size = Normalize-LegacySize $_.size_pass
      $key = "$size|$(($_.tab_path -replace '@default$','') -replace ' \[[^\]]*\]', '')|$($_.finding)|$(Normalize-LegacyWidget $_.widget_a)|$(Normalize-LegacyWidget $_.widget_b)"
      -not $baselineKeys.ContainsKey($key)
    })
  }
  $legacyPass = [bool]$baselineLayoutPath -and $legacyNew.Count -eq 0
  $legacyValue = if ($baselineLayoutPath) { "$($legacyNew.Count) new finding(s)" } else { 'missing -Compare layout baseline' }
  Write-DirectorBudgetLine 'layout findings legacy pages vs v1 baseline = 0 new' $legacyValue $legacyPass

  $directorBudgetFailed = -not ($openPass -and $steadyPass -and $buildPass -and
    $searchPass -and $amberPass -and $hoverPass -and $kitLayoutPass -and $legacyPass)
  Write-Host ("DIRECTOR V2 BUDGET: " + $(if ($directorBudgetFailed) { 'FAIL' } else { 'PASS' })) -ForegroundColor $(if ($directorBudgetFailed) { 'Red' } else { 'Green' })
}
if ($crashes.Count) {
  Write-Host "`nCRASHED (excluded from the numbers above):" -ForegroundColor Red
  $crashes | ForEach-Object { Write-Host "  $($_.Site)  exit $($_.ExitCode)  $($_.Dump)" -ForegroundColor Red; Write-Host ($_.Stack -replace '(?m)^', '      ') }
}
$layoutFlagged = $Layout -and -not $DirectorV2 -and $layoutFindingCount -gt 0
if ($layoutFlagged) {
  Write-Host "LAYOUT FLAGGED: $layoutFindingCount finding(s)" -ForegroundColor Red
} elseif ($Layout) {
  Write-Host 'LAYOUT: no findings.' -ForegroundColor Green
}
$flagged = @($ranked | Where-Object { $_.Flag })
$exitFlagged = if ($DirectorV2) { @($flagged | Where-Object Floater -ne 'director') } else { $flagged }
if ($flagged.Count) {
  Write-Host "FLAGGED: $($flagged.Count) measurement(s)" -ForegroundColor Red
} elseif ($inventoryMissing.Count) {
  Write-Host 'Performance thresholds were not evaluated as successful because the inventory fixture was incomplete.' -ForegroundColor Red
} else {
  Write-Host 'No performance thresholds flagged.' -ForegroundColor Green
}
if ($crashes.Count) { exit 3 }
if ($inventoryMissing.Count) { exit 5 }
if ($directorBudgetFailed) { exit 6 }
if ($exitFlagged.Count) { exit 1 }
if ($layoutFlagged) { exit 4 }
exit 0
