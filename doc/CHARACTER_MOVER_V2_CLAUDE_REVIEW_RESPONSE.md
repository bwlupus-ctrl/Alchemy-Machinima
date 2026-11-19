# Character Mover V2 — Claude Adversarial Review Response

**Commit reviewed:** `35cd2ef0d13` (`feat: implement Character Mover V2 runtime`)
**Range:** `ab62137f1f8..35cd2ef0d13`, committed diff only (dirty working tree excluded)
**Method:** three parallel adversarial passes (shared motion engine, actor-mover core, director/scene/UI+avatar), each proving/refuting the section-6 invariants from code; every BLOCKER and MAJOR re-verified by hand against the committed blobs. No build was run (matches the handoff gate).

## Verdict: READY TO BUILD — NOT YET READY FOR RUNTIME ACCEPTANCE

- **Viewer compile/link gate: PASS.** No compile or link blocker exists in any shipped viewer/library file. The one proven safety property that matters most — *default presentation weight 1.0 is bit-identical to stock* (invariant 11) — holds, verified by enumerating every pose-weight write site in `llcharacter`. A normal avatar animates exactly as before; the whole system is inert unless the Actor Mover drives it.
- **Test gate: FAIL (test-only).** The new integration test has never compiled (B1, B2). `BUILD_TESTING` is off by default, so this does **not** block the viewer build — but the handoff's "regression tests pass" claim is false and must not be relied on.
- **Runtime acceptance gate: hold for fixes.** Three runtime MAJORs (AM-M1/M2/M3) gate specific configurations — one of them (AM-M1) fires in the **default self-avatar + AO** flow — and the UI honesty MAJOR (DIR-1) will actively mislead authors. Fix these before signing off the runtime matrix.

Ghost-actor and custom-clip flows — the dominant machinima use — are lifecycle-clean with careful ownership discipline. The damage is concentrated in (a) the never-built test, (b) the self+AO+stock-anim path, (c) a bad-asset hang, (d) recorder-seek determinism, and (e) UI that promises unimplemented roles.

---

## BLOCKERS — compile/link (test target only; viewer build NOT blocked)

### B1 — Integration test uses a nonexistent assertion (`ensure_approximately`)
- **File:** `indra/llcharacter/tests/llmotionpresentation_test.cpp` (9 call sites: lines 70,72,92,95,106,108,125,140,142)
- **Failure:** `ensure_approximately(msg, actual, expected, tol)` exists nowhere in `indra/` (verified: 0 definitions). TUT provides `ensure_distance(msg, actual, expected, distance)` — identical signature. Nine "identifier not found" errors.
- **Fix:** `s/ensure_approximately/ensure_distance/` (tolerance arg already matches).
- **Blocks:** COMPILE of the test target only.

### B2 — Integration test cannot link (LLPose symbols)
- **File:** `indra/llcharacter/CMakeLists.txt:79` — `LL_ADD_INTEGRATION_TEST(llmotionpresentation "llmotion.cpp" ...)`
- **Failure:** source list is `llmotion.cpp` only; `test_libs` = `llcommon llmath`. `llmotion.cpp` (via `deactivate()` → `mPose.setWeight`, `getFirstJointState`, and `~LLMotion`) needs `LLPose::*` from `llpose.cpp`, which is neither compiled in nor linked. Unresolved externals on MSVC/GCC.
- **Fix:** add `llpose.cpp;lljoint.cpp` to the test's source list (or link `llcharacter`).
- **Blocks:** LINK of the test target only.

*Net: fix B1+B2 before any `BUILD_TESTING=ON` build; neither blocks the viewer.*

---

## MAJOR — runtime (config-gated; none block compile/link)

### AM-M1 — `AOEngine::override()` cleanup cross-kills the AO stand and the outgoing gait on self
- **File:** `llactormover.cpp:190-194` (`startLocalMotion`) → `aoengine.cpp::override()` (line 499; cleanup sweeps issue `sendAnimationRequest(…STOP)` + `LLCharacter::stopMotion(…)`).
- **Invariant:** breaks **inv 5** in effect (loading incoming gait must not reduce outgoing weight) and undermines **inv 3** ("balanced ≠ safe").
- **Failure scenarios (self avatar + `AlchemyAOEnable`):** (a) default fallback walk resolves to `ANIM_AGENT_WALK` with `allow_ao=true` (llactormover.cpp:427-431) → gait start's cleanup server-stops and locally kills the user's AO **Standing** state. (b) Walk→run crossfade: `override(ANIM_AGENT_RUN,true)` cleans the **Walking** state → `LLCharacter::stopMotion` kills the weight-1 *outgoing* clip at fade start; avatar pops to the weight-0 incoming clip, and the mover's primary handle silently fails its per-frame sample writes against a dead motion. Custom-clip UUIDs and ghosts escape (state lookup returns null before cleanup).
- **Fix:** add a read-only resolve accessor to `AOEngine` (`getAnimationForState` without cleanup/timers/state writes), **or** refuse `allow_ao` when the source is a stock remap ID.
- **Blocks:** RUNTIME (driving the self avatar with AO — the default config).

### AM-M2 — Asset-controlled infinite loop / render-thread hang
- **File:** `llactormover.cpp:247-252` (`startLocalMotion`): `while (activation_offset <= getEaseInDuration()+0.001f) activation_offset += cycle;`
- **Failure:** `cycle` guarded only `> 0.001f` (can be ~0.002); `getEaseInDuration()` is validated finiteness-only at deserialize (`llkeyframemotion.cpp:2044`, no range clamp). A large-ease-in clip stalls F32 accumulation once `activation_offset`'s ULP exceeds `cycle` (~16k) → infinite loop → hard hang. Even non-stalling cases spin ~1e9 iterations. Reachable via any crafted/corrupt clip assigned to a locomotion role.
- **Fix:** closed form in F64 — `activation_offset = loop_in + cycle * ceil((ease_in + eps - loop_in)/cycle)` — or an iteration cap.
- **Blocks:** RUNTIME (hang).

### AM-M3 — Follower gait non-deterministic across a recorder seek
- **File:** `llactormover.cpp:4458-4468` (`advanceFollower`): odometer integrates `|fArc - old_d|`.
- **Failure:** a sync leader's scrub-seek (or leader restart at arc 0) is absorbed as "travel" → follower gait phase shifts permanently and path-dependently (scrubbing back does not restore it), a one-frame speed spike can trigger a spurious walk→run, and no discontinuity is propagated (`updateLocomotionGait(...,false)`, `mContactDiscontinuity` never set) so Pose Polish foot-locks drag across the jump. The leader path is clean (absolute odometer, llactormover.cpp:4187 / legacy 3464-3466).
- **Fix:** detect `|fArc - old_d| > SYNC_SEEK_SNAP_M`, snap the odometer to `fArc`, pass `discontinuity=true`, set `mv.mContactDiscontinuity`.
- **Blocks:** RUNTIME (determinism/visual under recorder seeks).

### DIR-1 — UI exposes 7 locomotion roles the runtime never selects
- **File:** combo `floater_director.xml:809-820` + `llfloaterdirector.cpp:refreshLocomotionRoleEditor:2323-2377`; runtime gate `llactormover.cpp:desiredLocomotionRole:439-471`.
- **Invariant:** violates the review objective "schema/UI promises the runtime does not implement."
- **Failure:** `desiredLocomotionRole()` only ever returns `NONE/HOVER/FLY/WALK_FORWARD/RUN_FORWARD` (both mover reviews confirm; `LOCO_TURN_LEFT/RIGHT` appear nowhere else, backward/strafe/takeoff/land are unreachable dead switch cases). But the combo offers all 12 and returns confident, confidence-scored status (`"Turn left: auto phase 92%"`). A director configures Turn/Backward/Strafe/Takeoff/Land, saves, ships — the avatar never uses them and nothing says so. Doc §4 admits these are deferred; the UI does not.
- **Fix:** disable/grey/annotate the 7 non-functional combo items (or restrict the combo to the 5 reachable roles) until selection lands.
- **Blocks:** runtime acceptance / authoring honesty (not compile/build).

---

## MINOR (polish; none block build)

**Motion engine (owned-mode / runtime only):**
- ME-m1 `applyExternalLocomotionSeed` (llkeyframemotion.cpp:1047-1077) bypasses the non-negative clamp; negative `base_time` snaps to clip start. Fix: positive-mod.
- ME-m2 presentation weight double-applies through `mResidualWeight` capture during ease transitions (llmotioncontroller.cpp:713/764/988) — exact at weight 1.0, 0.5→0.25 otherwise.
- ME-m3 `claimPresentationControl` is not type-gated (procedural motions can be weight-claimed; only external *sample* is keyframe-gated). Document or gate.
- ME-m4 owned-motion server restart swallowed during ease-out (deprecation guard). Owner must drive restarts.
- ME-m5 one-frame clock-domain mix at claim/release while stopped (cosmetic).
- ME-m6 owner must re-find motion by ID every frame — never cache `LLMotion*` across `purgeExcessMotions`.
- ME-m7 "large-time mapping" test claim overstated (only native passthrough tested; raw `setExternalSampleTime` is unmapped — caller must pre-bound).

**Actor mover:**
- AM-m4 `migrateActor` loses dwell-anim carry-over (`stopLocomotion` nulls `mDwellAnim` before the migrate test) → bare stand on mid-dwell replacement.
- AM-m5 `placeAt` hold triggers one-frame gait start/stop churn (invokes AM-M1's AO cleanup).
- AM-m6 "role set, nothing playing" never self-recovers (sync-take mid-dwell; dwell-entry stop guard can skip a valid secondary).
- AM-m7 **inv 4 refuted for self** — a sim-signaled motion *is* stopped on self (`isSelf()` bypass, llactormover.cpp:307-310). Looks deliberate (self AO replacements are server-signaled) but breaks the invariant as stated — confirm intent + document.
- AM-m8 hover/fly boundary (0.15) has no hysteresis (walk/run does) → air churn.
- AM-m9 `Path::rebuild()` caps are per-segment (4096); `applyPathSceneData` admits 4096 nodes → up to ~16.7M samples (~200 MB) + multi-second hang on a hostile scene. Add a whole-path sample budget.
- AM-m10 double gait+sample update per frame (harmless; drop the second `updateLocomotionSample`).

**Director/scene/UI:**
- DIR-m1 Pitch checkbox stays enabled while Air path forces flight-tangent pitch regardless (Ground Follow correctly greys out) — mirror it or fix the tooltip.
- DIR-m2 `mMinNaturalRate`/`mMaxNaturalRate` round-trip into every scene but are never read (dead weight; not UI-promised).
- DIR-m3 typing speed/phase for an unset role before "Set role" is silently discarded on refresh (paper cut).
- DIR-m4 `std::swap` (lldirectorcast.cpp:274) without explicit `<utility>` — works on this MSVC STL transitively; add the include.

---

## Invariant scorecard (section 6)

| # | Invariant | Result |
|---|---|---|
| 1 | No `setAnimTimeFactor` / restore unowned clock | **PROVEN** |
| 2 | No `LLVOAvatar::startMotion/stopMotion` for locomotion | **PROVEN** |
| 3 | Every AO `override(true)` has reachable matching `override(false)` | **PROVEN (count)** — but *balanced ≠ safe*, see AM-M1 |
| 4 | Sim-signaled motion never stopped by mover | **PROVEN non-self / REFUTED self** (AM-m7) |
| 5 | Presentation ownership never silently moves between tokens | **PROVEN** |
| 6 | Non-keyframe cannot enter external-sample mode | **PROVEN (sample)** — claim not gated (ME-m3) |
| 7 | Loading incoming gait cannot reduce outgoing weight | **PROVEN structurally** — undermined at runtime by AM-M1(b) |
| 8 | First loaded pose uses latest distance + phase | **PROVEN** — negative-base edge ME-m1 |
| 9 | Same-UUID transition never creates two controlled sides | **PROVEN** |
| 10 | Stop/arrival/dwell/suspend/cancel/actor-loss/replacement can't leak a gait side | **PROVEN** — one edge AM-m6 |
| 11 | Default weight 1.0 preserves stock arithmetic | **PROVEN (bit-identical)** |
| 12 | Path rebuild: deterministic termination + hard cap | **PROVEN per-segment** — whole-path scale AM-m9 |
| 13 | Scene load never commits partial invalid data | **PROVEN** |
| 14 | Airborne cannot ground-follow / retain foot locks | **PROVEN** |
| 15 | Stale contact hints preserve prior Pose Polish behavior | **PROVEN** |

## Commit hygiene (section 11) — clean
Confirmed the commit stages only Mover V2 work: none of the unrelated Cine Light / tonemapper / ReShade files are present; the committed `settings.xml` hunk is only `ActorMoverGaitTransitionTime` (not the per-light `CineLightRig*OffsetZ`), and the committed `llfloaterdirector.cpp` hunk carries none of the per-light-Z edits. The unrelated work remains intact and unstaged.

## Recommended sequencing
1. **Before the viewer build:** nothing required — it compiles/links as-is.
2. **Before running the integration test:** B1 + B2.
3. **Before runtime acceptance / driving self+AO:** AM-M1 (must), AM-M2 (must — hang), AM-M3, DIR-1. ME-m1/m2 before exercising the owned path.
4. **Polish pass:** remaining minors.
