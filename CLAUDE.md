# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⛔ MANDATORY WORKING AGREEMENT — CONSULT CODEX (user directive, 2026-07-21)

**For this project, from 2026-07-21 onward: every code change must be reviewed with Codex, and
any non-trivial judgement call needs a second opinion. This is not optional and does not expire.**

### The rule
1. **Consult Codex before writing code.** Design/feasibility questions go to Codex first.
2. **Re-defer after every fix.** When Codex reports a problem and you fix it, send the FIX BACK to
   Codex for verification. Loop until it reports 0 must-fix. Never ship a fix that has not been
   through the loop.

### ⚡ ORDER OF OPERATIONS (user directive — do NOT build between review rounds)
**BATCH the fixes. Apply ALL of Codex's findings, defer ONCE with the complete set, iterate on
review until 0 must-fix, and only THEN build.** Do not build between review rounds.

    fix all findings  ->  defer to Codex  ->  (repeat until 0 must-fix)  ->  BUILD ONCE  ->  user tests

Rationale: a build is ~6 minutes and only catches syntax; a review round is cheaper and catches the
semantics that actually break things. Building code that the next review round will change anyway
is pure waste. Converge the expensive-to-fix thing (correctness) before paying for the build.

Two exceptions where a build is still warranted mid-loop:
- Codex's finding is specifically about a compile/link concern and only the compiler can settle it.
- The review has converged and you need the compiler as the final independent check — it has TWICE
  contradicted Codex on access specifiers/types. A compile error after convergence starts a NEW
  fix round (fix -> re-defer -> build), it does not bypass the loop.

Never hand the user a build to test until Codex has returned 0 must-fix on the code IN that build.
3. **Share raw findings and results — never a summary.** Paste actual log output, actual compiler
   errors, actual diffs. Your interpretation of a symptom is the single biggest source of wasted
   rounds (see below).
4. **Get a second opinion on anything material** — architecture, root-cause diagnoses, whether an
   approach is viable at all. Ask Codex explicitly "is this approach wrong?" and invite it to say so.

### How to invoke
```
node "C:/Users/xianw/.claude/plugins/cache/openai-codex/codex/1.0.6/scripts/codex-companion.mjs" task "<prompt>"
```
Run from the repo root. Foreground by default (add `--background` + poll with `status`/`result`).
The `codex:codex-rescue` subagent is a fire-and-forget FORWARDER — it cannot poll and will only
return a job handle, so call the helper directly from the main loop instead.

### Why this rule exists — earned the hard way on the ghost-clone work
- Codex caught `LLVOAvatar::slamPosition()` opening with `gAgent.setPositionAgent()`
  (llvoavatar.cpp:4038) — copied from LLUIAvatar, it would have **teleported the real user** onto
  every spawned clone.
- Codex caught that a palette-isolation test compared ghosts at DIFFERENT world positions, so it
  would have reported PASS even with pose isolation completely broken.
- Codex caught that `LLViewerJointAttachment::addObject()` **sends ObjectDetach to the simulator**
  for non-local objects — i.e. would have stripped the user's real worn attachments.
- **The one round where the rule was skipped** (an "obvious" ordering fix, shipped without
  re-deferring) was wrong, and cost the user a wasted in-world session.
- **Describing symptoms instead of pasting logs** sent Codex down a wrong path twice. The brief
  that included raw log lines let it refute a bad hypothesis immediately.

### Division of labour that actually works
- **Codex** — semantics, ordering, architecture, in-tree precedent. Catches what compiles fine but
  is wrong.
- **The compiler** — facts. It has twice contradicted Codex on access specifiers/types. Trust it
  over any review claim about whether a member is public/protected/private.
- **Claude** — cross-check the load-bearing claim independently; reconcile when reviews disagree
  (they have — one report cleared a construction order the other flagged, because each was asked a
  different question). Never pick one blindly; work out which one actually examined the thing.

### In-world testing
Claude cannot run the viewer or log in (credentials are off-limits). The user runs it. Therefore
instrument so the LOG STATES A VERDICT — distinguish PASS / FAIL / INCONCLUSIVE explicitly, and
make "the measurement itself was untrustworthy" a separate outcome from "the feature is broken".
Never let a diagnostic silently no-op into a confident wrong answer.

## Project Overview

Alchemy Viewer is a third-party client for Second Life, forked from the official Linden Lab viewer. It is a large C++ desktop application (~750 source files in the main viewer module alone) using OpenGL for rendering, with builds targeting Windows, macOS, and Linux.

## Build System

CMake with vcpkg for dependency management. The source root for CMake is `indra/` (not the repo root). All CMake presets are defined in `indra/CMakePresets.json`.

### Prerequisites

- CMake 3.27+, Python 3.13+, Rust (for Velopack), .NET SDK, Visual Studio 2022/2026 (Windows) or Xcode (macOS) or GCC/Clang+Ninja (Linux)
- Python venv: `python3 -m venv .venv && .venv/Scripts/Activate.ps1 && pip install -r requirements.txt` (Windows) or `source .venv/bin/activate` (Unix)
- Dotnet tooling: `dotnet tool restore`

### Configure (first time or after CMake changes)

```
# Windows (Visual Studio 2026)
cmake -S indra --preset vs2026-os

# Linux (Ninja)
cmake -S indra --preset ninja-os

# macOS (Xcode)
cmake -S indra --preset xcode-os
```

Append `-os` presets for open-source builds, omit `-os` for proprietary builds (adds `-DINSTALL_PROPRIETARY=ON`). List available presets: `cmake -S indra --list-presets`

### Build

```
# Windows
cmake --build build-Windows-vs2026-os --config Release

# Linux
cmake --build --preset ninja-os-release

# macOS
cmake --build build-Darwin-xcode-os --config Release
```

Configuration types: `Debug`, `OptDebug` (debug build, release libs), `RelWithDebInfo` (default), `Release`.

The viewer executable lands at `build-<OS>-<preset>/newview/<CONFIG>/` (e.g., `SecondLifeViewer.exe` on Windows, `SecondLife.app` on macOS).

### Tests

Tests are off by default. To enable: add `-DBUILD_TESTING=ON` to the configure command. Then run via CTest:

```
ctest --test-dir build-<OS>-<preset> --output-on-failure
```

Unit tests live alongside the library they test in `indra/<library>/tests/` directories. The test framework is TUT (Template Unit Test). Test targets are defined by the `LL_ADD_PROJECT_UNIT_TESTS` macro in `indra/cmake/LLAddBuildTest.cmake`. Integration tests are in `indra/integration_tests/`.

## Architecture

### Source Tree (`indra/`)

All source code lives under `indra/`. The codebase is organized as a set of libraries that the main viewer application (`newview`) links against. Dependency flows downward — libraries only depend on libraries listed above them:


### Architecture breakdown
- @doc/ARCHITECTURE.md