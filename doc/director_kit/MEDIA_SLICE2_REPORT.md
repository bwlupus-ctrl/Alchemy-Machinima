# Media slots: Slice 2 — shared routing and lifetime

Base: `feature/director-kit-p0`, `63da81d57214790e70d582c34c4a0fb9b8b5b498`.
Read `CLAUDE.md`, the complete v4 specification (including section 8), and
`MEDIA_SLICE1_TRIAGE.md`. `VCamMediaSlotsEnabled` still defaults to FALSE.
During this run the spec was independently revised to v5, explicitly assigning
private URL/local-file sources to separate slice 2b work. That concurrent edit
was left untouched; this report covers the requested v4 slice 2 borrowed-face scope.
No full viewer build, commit, push, VCam history, player creation/navigation,
playback command, resizing, volume change, asset upload or object update was added.

## Changes and decisions

- Shared source capability now admits enabled media to display faces, virtual
  screens, OBJECT and RIG_LIGHT projectors, reassignment and restored routes.
  Destination validation, shared caps, stable generations and arbitration remain.
  The rig combo counts/lists media with zero cameras. Slice 1 already migrated
  the texture picker and Build panel to occupied slots/common handles; opening
  registry validation completes those paths without additional edits there.
- Main/aux displays, raw/output monitor previews and projector preparation use
  the retained media signal and accepted aspect. Outputs retain independent
  FIT/FILL/STRETCH, anchors, orientation, effects, looks and no-signal settings.
  Media skips camera exposure/grading/FOV. Cookies encode SDR once; FIT bars
  remain black even with grain. Frozen preview images can feed newly added outputs.
- Media cookie shader/sampler/source/target preflight precedes bind/clear.
  Resolution replacement allocates/draws/flushes one local owned spare, then
  swaps ownership and accepted metadata; failure keeps the old target/publication.
  There is no standing pair or slot staging. RGBA8 + TMG_AUTO builds mips before
  publication. Binding validates accepted source kind, slot/binding generations,
  source epoch and load token, independently of newer image/layout serials.
  Media accepted look/FOV metadata travels with its pixels. Held sources also prepare
  cookies after first bind, rig rebuild, destination-aspect changes and animated
  looks; static cookies avoid redundant draws/mips.
- Both probe identity sites include media kind/source epoch. The structural key
  excludes frame serials. Non-rig live, rig-emitter live and on-demand live
  signatures include accepted-cookie revisions; scheduler budgets are unchanged.
- The existing updateMedia-start reconcile now owns per-slot demand, re-resolving
  live volume/TE/media-entry/impl ownership and current plugin/load identity before
  interest/update work. No impl or face pointer is retained. Demand is the maximum
  individual consumer area, then the maximum across slots sharing an impl;
  calculateInterest takes max(real area, demand). Provisional LibVLC alone can
  bootstrap, with the existing Linux GStreamer10/force-VLC substitution rule.
  Normal media policy, consent, CPU/instance limits and inactive-window throttles
  remain authoritative. Source failure, freeze, removal, consumer loss, disable
  and GL release revoke owned demand; source invalidation releases images/cookies.
- Resume feed / Freeze feed changes slot consumption only. Freeze preserves a
  valid retained image, including after its last output is removed; validation
  continues. Original playback/audio and other slot consumers remain independent.
  Existing cadence/adaptive tiers now include consumed media outputs.
- Volumetric acquisition checks both enables inside the ownership helper before
  its same-target return. Snapshot/status, scene apply and rig rebuild cannot
  bypass it. Media-off releases media ownership; master-off releases all owned
  overrides. User-declined choices remain intact. Media cannot acquire or inherit
  a gate subscription, including through reassignment or scene routes.

The Director generator, page sources and functional manifest were regenerated.
The new segmented control uses the media-only fixture with consumption/runtime
revision and zero-demand-on-freeze oracles. Pick remains DEFER_INWORLD. The exact
source-row pin increases from 2113 to 2114 for the one added row; no assertion,
coverage threshold, deferred-pick classification or required consumer was removed.

## Validation

Python: `C:\Users\xianw\AppData\Local\Python\pythoncore-3.14-64\python.exe`.
Logs/runners are under `.tmp/media_slice2/`. The unchanged guard uses its supported
workspace CachePath redirect and process-local Git ignore override from slice 1.

| Check | Result |
| --- | --- |
| zs_check -Files, every changed .cpp including tests | PASS: 11/11 mapped TUs; the final lens resource-release follow-up also passed explicitly (1/1). |
| Projector test TU with its own integration-target flags | PASS, cl /Zs. Cases 27–29 cover accepted identity, demand/fan-out/revocation and both-enable eligibility; case 14 additionally rejects media gate inheritance. |
| prebuild_guard.ps1 -ChecksOnly | PASS: final post-audit run, all gates, exit 0 before object purge. |
| Kit generation/lint selftests; functional manifest check/selftests; functional_report selftests | PASS. |
| New-control enrichment with all 47 required consumers | PASS: Add/aspect/consumption remain RUN with media-only oracles; Pick remains DEFER_INWORLD; existing in-world denials remain fail-closed. |
| OFF source comparison | PASS: legacy upload/update/helpers, loadURI and volume bodies are byte-identical to HEAD; interest/admission differ only by gated demand additions; plugin/VLC files are unchanged. |
| git diff --check | PASS. |

Final checks-only output (`.tmp/media_slice2/prebuild_post_audit.log`):

```text
UI gate: OK (XUI changed since 7050a38cc4e pass the layout checker)
UI draw gate: OK (no new per-frame UI cost since 7050a38cc4e)
Compile gate: OK (zs_check -Base 7050a38cc4e -IncludeTests)
Light-map maths gate: OK (alkitmath_harness -Repo I:\alchemy-machinima)
Director kit gen gate: OK (--check)
Kit lint gate: OK (--all)
Director parity gate: OK (--groups light_group)
XUI layout selftest gate: OK (--selftest)
Director page sources gate: OK (--check)
Director theme gate: OK (--check)
Director theme selftest: OK (--selftest)
ChecksOnly: all gates ran; exiting before the obj purge (no files touched).
```

Cookie preflight, accepted retention and safe resize now apply only to media.
Camera cookie preparation, generation matching, current look/FOV sampling and
resize invalidation retain HEAD behavior. Source comparison and syntax gates do
not prove GL output, upload lifetime, measured performance or audio acceptance.

## Reviewer fix round — 2026-10-17

- P1-1: `bindProjectorCookie` has separate media and camera selection arms.
  Cameras require `feed.mGeneration != 0` and
  `candidate.mSourceGeneration == feed.mGeneration`, then use current feed
  settings. A suppressed/missing camera output therefore selects its current
  OWN_COOKIE/BLACK/WHITE no-signal path. Accepted identity and accepted settings
  remain exclusive to MEDIA_FACE.
- P1-2: restored the HEAD camera preparation body, including look-program
  fallback, shader-bind order, exposure/sampler handling, dirty/publication
  handling and destructive resize semantics. Media preparation is a separate
  pass with strict preflight and retained-target resize. All media skips occur
  before shader binding; the one successful bind is followed by draw flush,
  SDR-uniform reset and unbind. Sampler preflight uses the const channel getter
  without binding. Each media projector uses one `getProjectorFeed` lookup;
  synchronous preparation does not re-query unchanged registry identity.
- Both inexpensive ON-path fixes: `mediaSlotPermitted` uses a static
  `LLCachedControl<bool>` for AudioStreamingMedia. One media pass covers all
  media slots with one registry snapshot, reused by the composite-state overload
  instead of copying another snapshot per slot/projector. Camera projectors are
  filtered out before the media pass's feed lookup. Frozen-source preparation,
  animation cadence, accepted publication, mip generation and probe signatures
  remain in place.
- This round edits pipeline/lens/media implementation, their necessary
  declarations, this report, and generated `page_sources.json` line references.
  `SPEC_media_projector.md` was not edited.

### Diff-level camera proof against 63da81d5721

Runner: `.tmp/media_slice2/check_camera_head.py`; output:
`.tmp/media_slice2/camera_head_proof.log`. It compares the full function source,
not selected predicates. The only folds are the MEDIA_FACE-only arm/guard,
the `!media` allocation arm, its enclosing indentation, and the existing
unique-pointer target spelling (`mTarget->` back to HEAD's `mTarget.`).
No shader, settings, generation, draw or publication statement is removed.
The normalized HEAD/current source pairs and zero-byte unified diffs are saved
under `.tmp/media_slice2/camera_head_proof/`.

| Camera path | Diff vs HEAD after those explicit folds |
| --- | --- |
| `preparePrismProjectorFeeds` (9,260 bytes) | Empty; SHA256 `c19ff6ed84039b984fe5d745f55cee174e70d5c046b00ac4d7db8d1733dd5e18` |
| `bindProjectorCookie` (6,188 bytes) | Empty; SHA256 `425af3344bf323453e87042f252c81abc97471ded2d462573d29aec045b7adbd` |
| `ensurePrismProjectorFeed` (1,622 bytes) | Empty; HEAD allocation/invalidation sequence restored |
| Dirty preparation and dirty marking | Empty without media folds or storage normalization |

This establishes source-path equivalence with the declared storage adaptation;
it is not a claim that viewer/GL runtime comparison was executed.

### Fix-round gates

The same full-path Python and existing checks-only runner are used. No gate,
threshold, baseline or coverage requirement was changed. The supported CachePath
redirect and process-local Git ignore override remain as documented above.

| Check | Result |
| --- | --- |
| `zs_check -Files` for every changed .cpp, including tests | PASS: 11/11 TUs, 0 failures; `.tmp/media_slice2/zs_review_fix.log` |
| Camera source proof and media preflight/snapshot/policy checks | PASS; `.tmp/media_slice2/camera_head_proof.log` |
| Legacy media source/upload/load/volume comparison | PASS; `.tmp/media_slice2/off_review_fix.log` |
| `prebuild_guard.ps1 -ChecksOnly` | PASS: every gate, exit 0; `.tmp/media_slice2/prebuild_review_fix_final.log` |
| `git diff --check` | PASS, exit 0; `.tmp/media_slice2/diff_check_review_fix.log` |

The initial guard run passed every gate except page sources: separating media
preparation shifted pipeline source line numbers. Standard page-source generation
updated only 16 numeric line references in `page_sources.json`, each by +209;
no rows, bindings or assertions changed. The fix-only generated diff is saved in
`.tmp/media_slice2/page_sources_review.diff`. The full unchanged guard rerun
passed UI, draw, compile (tests included), maths, kit generation/lint/parity,
XUI selftest, page sources and both theme checks. Its final line was:
`ChecksOnly: all gates ran; exiting before the obj purge (no files touched).`

No build, commit or push was performed. In-world acceptance remains outstanding.

## Remaining for Slice 3

Execute media-only/mixed and legacy scene round-trip/malformed-input cases;
complete UI/search and broader functional oracles; prove picker commit/cancel;
exercise source/TE/impl/plugin/load replacement, policy/consumer loss, freeze/resume,
GL reset and shutdown with main/worker uploads. Validate SDR/aspect/orientation,
all output/no-signal paths, paused-source rig rebuild and resize/preflight failure
retention. Run OBJECT/RIG_LIGHT live-probe acceptance with on-demand on/off and
watchdog disabled. Test all four master/media enable combinations with repeated
snapshots/scene apply, user declines, static beauty/cookie restoration and audio.
Complete diagnostics/failure injection and measured copy/mip/memory/A-V checks.

Independent adversarial re-review, test-executable execution and viewer/GL/in-world
acceptance were not performed; this report does not claim runtime acceptance.
The pre-existing vcpkg modification and unrelated untracked files were left alone.
