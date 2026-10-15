# [FrameLimiter] Build and run the standalone cap timing check (no viewer build needed).
# Usage: powershell -ExecutionPolicy Bypass -File scripts/perf/framelimiter_timing.ps1
# Prints mean/min/p50/max frame-start interval at 30, 60 and 144 fps, plus a slow-frame
# case (work longer than the interval: expect no wait and no burst afterwards).
$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$nv   = Join-Path $repo 'indra\newview'
$work = Join-Path $env:TEMP 'framelimiter_timing'
New-Item -ItemType Directory -Force -Path (Join-Path $work 'stub') | Out-Null
# Copy the sources so the stub precompiled header wins over the viewer's real one.
Copy-Item (Join-Path $nv 'alframelimiter.cpp'), (Join-Path $nv 'alframelimiter.h') $work -Force
Copy-Item (Join-Path $PSScriptRoot 'framelimiter_timing.cpp') $work -Force
@'
#pragma once
#define LL_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define LL_PROFILE_ZONE_NAMED_CATEGORY_APP(x)
'@ | Set-Content (Join-Path $work 'stub\llviewerprecompiledheaders.h')
$vcvars = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vcvars) { $vcvars = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools" }
$bat = Join-Path $vcvars 'VC\Auxiliary\Build\vcvars64.bat'
Push-Location $work
try {
    cmd /c "`"$bat`" >nul && cl /nologo /O2 /EHsc /std:c++17 /I stub /I . framelimiter_timing.cpp alframelimiter.cpp /Fe:framelimiter_timing.exe user32.lib"
    if ($LASTEXITCODE -ne 0) { throw 'compile failed' }
    foreach ($a in @(@(30,8000),@(60,5000),@(144,2000),@(30,40000),@(30,8000,300,1500))) {
        & .\framelimiter_timing.exe @a
    }
} finally { Pop-Location }
