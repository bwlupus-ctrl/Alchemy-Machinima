# Character Mover V2 — Build Validation

**Validation range:** `077060046da..HEAD`
**Claude gate:** `doc/CHARACTER_MOVER_V2_CLAUDE_FIX2_REVIEW_RESPONSE.md` returned **READY TO BUILD**.

## Outcome

The Release viewer and the dedicated presentation-control integration test now build successfully. The integration test passes **7/7**.

The first real build exposed a compile blocker that both pre-build static reviews missed. This document records that correction and the executed evidence.

## Build-only correction

`LLActorMover` member definitions added near the top of `llactormover.cpp` were inside the file's anonymous namespace. MSVC correctly rejected them with C2888: a global class member cannot be defined inside an unrelated anonymous namespace.

The namespace is now closed immediately after the internal helper functions and reopened before the later free path-evaluator helpers. This preserves:

- internal linkage for constants and free helper functions;
- global namespace placement for every `LLActorMover::...` definition;
- the original anonymous-namespace close before public path wrappers.

No runtime logic changed in this correction.

## Test correction

The large-time identity assertion used `ensure_distance(..., 0.0001f)`. At `123456.f`, adding or subtracting that tolerance rounds back to the same F32 value, and TUT's distance helper excludes its bounds; the assertion therefore fails even when actual equals expected exactly.

The assertion now uses `ensure_equals`, matching the property under test: the weight-1/native-time path must be bit-exact.

## Commands and results

### Release viewer

```powershell
cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release --target alchemy-bin --parallel 1 -- /nologo /v:minimal
```

Result: exit 0.

Artifact:

- Path: `I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe`
- Size: 82,297,344 bytes
- Timestamp: 2026-08-29 20:29:38 local
- SHA-256: `5C26E68D5EBB58ABB3DCA2B349B23437135907B81D93C203E6FD91176C827468`

The initial parallel attempt also produced C1041 shared-PDB contention. After terminating only the orphaned workers from that attempt, the target was rebuilt serially so compiler/linker results were unambiguous.

### Presentation integration test

```powershell
cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release --target INTEGRATION_TEST_llmotionpresentation --parallel 1 -- /nologo /v:minimal
I:\alchemy-machinima\build-Windows-vs2026-os\sharedlibs\Release\INTEGRATION_TEST_llmotionpresentation.exe
```

Result:

- Build exit: 0
- Test exit: 0
- Total: 7
- Passed: 7
- Failed: 0

## Post-build adversarial review

- Verified anonymous helper scopes close and reopen at the intended boundaries.
- Verified no `LLActorMover::` member definition remains in the first anonymous namespace.
- Verified internal path helpers remain internal and public wrappers retain access.
- Verified exact equality is appropriate for the identity branch and does not weaken approximate numeric tests elsewhere.
- Verified the correction diff is limited to `llactormover.cpp`, the one test assertion, Claude's returned response, and this validation record.
- Unrelated Cine Light, renderer, ReShade, menu, and log work remains unstaged and untouched.

## Runtime acceptance still required

Compile, link, and unit gates are clear. In-client validation must still cover:

1. stop-mode arrival remains a stable stand without per-frame gait churn;
2. an interior dwell holds its dwell animation and resumes exactly once;
3. recorder and follower scrubs without an owned clip select WALK/HOVER, never RUN/FLY from scrub displacement;
4. externally stopped or purged local gait motions reacquire without cross-stopping simulator-signaled animation.
