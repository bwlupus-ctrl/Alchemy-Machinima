# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⛔ ALL CLAUDE-WRITTEN CODE REQUIRES AN ADVERSARIAL CODEX REVIEW (user directive, 2026-07-25)

**Every line of code Claude writes for this project goes through an ADVERSARIAL review by Codex
before it is built, and before it is committed. No exceptions, no "this one is trivial".**

Adversarial means Codex is asked to ATTACK the code, not to bless it:
- Tell it what the change claims to do and ask it to find where that claim is FALSE.
- Ask explicitly: "is this approach wrong?" and invite it to say so.
- State your own reasoning so it can refute the reasoning, not just the syntax.
- Name the specific things you are least sure of and ask it to check those first.
- Never ask "does this look good" — that gets agreement, which is worthless.

Then **re-defer after every fix** and loop until it returns 0 must-fix. A fix is code too; it gets
the same treatment as the original.

**This is not ceremony.** In one session it caught, among others: a `GL_BLEND` change that made
opaque surfaces transparent in-world; a parameter table where 16 of 27 fields were never actually
read, so a whole mode silently did nothing; two positional aggregate rows corrupted to 26 and 28
values that would have compiled fine and shipped scrambled; a readiness latch that would have
published garbage flagged VALID whenever an optional shader failed to compile; and a shader
permutation added before `clearPermutations()`, which erased it and reintroduced the exact hazard
the change existed to prevent.

**Claude's own confidence is not evidence.** Multiple confidently-stated root causes in that same
session were wrong and were overturned either by Codex or by the user. Review is what closes that
gap.

## ⛔ SHIP FEATURES WHOLE, NOT IN SLICES (user directive, 2026-07-25)

**If it is logical to batch work together for a feature to be COMPLETE, and doing so is not a major
shift that risks crashing the client, then it gets coded together. In one go.**

Do not split a feature into "engine now, UI later" or "core now, the parts that make it usable
later". A feature the user cannot reach, select, or see is NOT delivered, however much code exists
behind it. Slicing is for changes that are genuinely risky to land at once -- render-state changes,
anything that can take the client down, anything needing an in-world A/B between steps. It is NOT a
way to book progress.

**What triggered this:** the handheld-operator locomotion feature was shipped as an engine with no
controls, so the only way to select a mode was Debug Settings. Then the vehicle physics that give
Drive its entire character were deferred to a later slice, so Drive selected and ran but felt like
nothing. Three separate commits and three review rounds were spent on what should have been one
delivery, and the user was left unable to use any of it in between. That is the failure mode this
rule exists to stop.

**Applies to the whole feature surface:** simulation + settings + UI + the wiring that registers it
with presets/resets. If part of it genuinely must wait, say so BEFORE starting, not after
committing.

## ⛔ RULES EARNED ON 2026-07-25 (a long, expensive night)

Six failures from one session. Each cost the user real time; each is preventable.

### 1. Shared render state — review the BEAUTY PASS and the FEATURE-OFF path, not just your feature
The only regression that reached the user's screen: `LLGLEnable(GL_BLEND)` was added to
`LLDrawPoolFullbright::renderPostDeferred` to make a sidecar channel deterministic. That pool sets
blend FACTORS but never the enable, and **the beauty pass was relying on inheriting blend-disabled**.
Opaque fullbright surfaces with alpha in their textures went see-through. Four review rounds passed
it, because every round reasoned about the sidecar correctly and nobody asked what else depended on
that state.

**Rule:** any change to global/shared GL state (`gGL.setColorMask`, `blendFunc`, `GL_BLEND`,
draw-buffer state, render-target attachments) must be reviewed explicitly against (a) the beauty
pass, and (b) the path where the feature is switched OFF. Prove the off-path is inert; do not assume
it. Ask the reviewer for that specifically — a per-change review will not find it on its own.

### 2. Do not hand the user a chain of one-click guesses
Over one evening: "enable Motion flip Y" (derived from ABI flag NAMES instead of the algebra — it
was wrong), then "set SL_ALBEDO_TO_LINEAR", then "it's just RTGI gain tuning". Each cost an in-world
trip. The user broke the case themselves by switching off RTGI Diffuse.

**Rule:** build the instrument, then bisect. Hand over ONE discriminating test with its outcomes
stated in advance, not a sequence of hunches. If you are guessing, say the word "guess".

### 3. Do not declare a root cause you cannot support
"H3 confirmed." "The provider write is corrupting it." "Tuning dominates." Three confident calls,
all wrong. The third was overturned by the user pushing a sweep further than suggested.

**Rule:** separate what the code PROVES, what documentation IMPLIES, and what is INFERENCE — and
label them. Proprietary internals (iMMERSE, closed shaders) can never be more than inference.

### 4. Check the instrument before the subject
An entire theory was built on a debug view that was contaminated — it was reading through an active
RTGI result, and it called a proprietary accessor whose behaviour is not in this repo.

**Rule:** before trusting a diagnostic view, establish what it actually samples and what could
corrupt it. A wrong instrument produces confident wrong answers faster than no instrument.

### 5. Verify before adding more
The session ended with the sidecar, forward coverage, water, particle suppression and the whole
camera-operator feature committed, built, and **never once seen working**. Volume of committed code
was being optimised over things the user could check.

**Rule:** unproven work is a liability, not progress. Prefer finishing and verifying one thing over
starting the next. Track explicitly what is BUILT-BUT-UNTESTED and surface that list unprompted.

### 6. Mechanical edits get verified by counting, not by compiling
A textual edit to strip one field from six positional aggregate initialisers left two rows at 26 and
28 values instead of 27 — one pattern matched a row another pattern had already rewritten. It would
have compiled and shipped scrambled parameters.

**Rule:** after any scripted/positional edit, verify by COUNTING and by cross-checking values against
the source of truth. "It compiled" proves nothing about positional data.

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

### ⚠️ PASSING FILES TO CODEX — INLINE or IN-REPO ONLY (recurring time-waster)
Codex can ONLY read files inside its workspace = **the repo root (`I:\alchemy-machinima`)**. It
**cannot** read the Claude scratchpad (`C:\Users\...\AppData\Local\Temp\claude\...`) or any path
outside the repo — a prompt that says "read the file at <scratchpad path>" fails with *"outside the
permitted workspace"* and wastes a full round-trip. This has happened repeatedly. Rules:
- **Inline the CONTENT** of the artifact (diff, plan, brief) directly in the prompt text — do not
  reference a scratchpad path. This is the default.
- For a large diff, tell Codex to run **`git diff`** / **`git show <sha>`** / **`git diff --stat`**
  itself — repo-relative git always works and needs nothing pasted.
- If a file reference is genuinely needed, **write the file INSIDE the repo first** (e.g. `doc/` or a
  repo-local scratch path) and reference that repo-relative path.
- **Never** put a `C:\Users\...\Temp\claude\...` path in a Codex prompt and expect it to be read.

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

The viewer executable lands at `build-<OS>-<preset>/newview/<CONFIG>/`. On Windows this fork builds **`AlchemyTest.exe`**, NOT `SecondLifeViewer.exe`:

    build-Windows-vs2026-os/newview/Release/AlchemyTest.exe

**Always report that full path in a build report.** The user launches from it.

**The running viewer holds the link lock.** `LNK1104: cannot open file ...AlchemyTest.exe` means the client is open, not that the build is broken. Poll and retry rather than failing.

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