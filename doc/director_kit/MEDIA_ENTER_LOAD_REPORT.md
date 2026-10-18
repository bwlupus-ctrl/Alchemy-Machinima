# Media Enter / Load report

Date: 2026-10-18. Implements the user-approved v8.4 interaction on top of the
uncommitted VCam selector changes. No commit, push, viewer build or object purge.

## Mockup first

Updated `.tmp/gate_mockup/project/VCamPage.dc.html` before changing the kit or
C++. Copied its exact bytes to `.tmp/VCamPage_media_mockup.dc.html`.
Both files have SHA-256
`b4478805325702c6bcd1448fc61b49e0b30e0686f0cd77889f8aa769cd43f6ac`.

Browse and Load share one action row and button style. Load's tooltip is
`Check and play this link (Enter does the same)`.
Video help now says
`Enter or Load checks and plays the link; clicking away only saves it.`
The mockup preserves the committed playing source when a draft is invalid and
keeps the same playing source unchanged on repeated Enter.

## Implementation

- Enter uses an optional explicit Return callback on `LLLineEditor`, installed
  only on VCam's Video editor. It consumes Return before ancestor panels can
  dispatch a default button. Editors without this callback retain their behavior.
- Enter, Load and Play reach `commitMediaVideo(true)` and the shared pure
  `ALMediaFile::commitVideoAction` rule. Source syntax and the commit must succeed
  before the existing Play pipeline is requested. Empty input never requests Play.
- Ordinary editor commits, including focus loss and right-click Paste value,
  retain `commitMediaVideo()` with its cue-only default. Their cue status is
  `Set. Press Enter, Load or Play to start it.` Runtime, policy-reenable and
  functional-test expectations use the same updated string.
- Invalid syntax never reaches source replacement or transport. Its text/error
  remains a draft; current playback is preserved. Load, like Play, is disabled
  for an invalid or empty draft.
- An identical committed descriptor skips source replacement. The existing
  transport coalescing decision is extracted into the pure `beginVideoPlay`
  helper: a playing/pending owner returns before resume, I/O or a new load.
  Source identity reconciliation still precedes that decision. Paused sources
  can resume normally.
- Existing asynchronous check/download, tokened admission, status precedence,
  cancellation, feature/master gates and initially muted Volume remain in force.
  The existing `checking file` / `Downloading N%` progress comes from that path.
  Restore remains syntax-only and never requests Play or I/O.
- The selector changes, Source Name rows, theme roles and existing actions are
  preserved. No acceptance threshold, baseline or exemption was relaxed.

## Generated contracts and exact deltas

The kit JSON replaces the single Browse row with one compound Browse/Load row.
Its row-count delta is **0**, so the completeness pin remains **2124**, with an
adjacent explanatory comment. There is **one additional native button**, not an
additional kit row. The page-count pin remains 37.

Search keywords include Enter, Load and play. Fix round 1 removes `cue` from
`vc_media_video` so cue/keyframe/mark synonyms do not pollute its search hit. The functional inventory
adds one deferred Load runtime action and one runnable `commit_action` lifecycle
probe. Browse keeps its previous deferred runtime action. The compound action
container has its own inventory entry. Relative to the incoming selector tree,
the manifest gains three entries; all HEAD widget/exercise/action tuples remain.
The previous selector's additional runnable `vc_media_name` entry also remains.

All three generators ran after the final C++ edits:
`director_kit_gen.py`, `director_page_sources.py`, and
`director_functional_manifest.py`. The spec now records v8.4 and replaces the
cue-only Enter rule. Generated page documentation reflects current source lines.

## Validation

Evidence: `.tmp/media_enter_load/`.

| Check | Result / evidence |
| --- | --- |
| Explicit `zs_check.ps1 -Files ... -IncludeTests` | 13/13 TUs pass, 0 failures; `zs_final.log`. Includes all currently touched C++ files and additional header consumers. |
| `prebuild_guard.ps1 -ChecksOnly` | All gates pass; exits before object purge; `prebuild_final.log`. |
| XUI layout `--changed HEAD` | 41 changed files, **zero findings**; `xui_final.log`. |
| Kit generator `--selftest` | Pass; `gen_selftest.log`. |
| Functional manifest `--self-test` / `--check` | Pass; `manifest_selftest.log`, `manifest_check.log`. |
| Functional coverage comparison | Every HEAD widget/exercise/action retained; `manifest_coverage.log`. |
| `INTEGRATION_TEST_alvcammediafilepolicy` | **66/66 pass**; `policy_test_final.log`. Group capacity increases 65 to 66 with a comment so the new case actually runs. |
| `INTEGRATION_TEST_llprismprojector` | **41/41 pass**; `projector_test_final.log`. |
| Actual mockup handler tests / JavaScript syntax | Pass; `mockup_test.log`, `test_mockup.cjs`; `node --check` accepts the extracted script. Byte identity checked as part of these tests. |
| `git diff --check` | Pass; `diff_check.log`. |

Additional header coverage: `lllineeditor.h` through `alkitrow.cpp`,
`alpaneldirectorvcam.h` through `aluiperfselftest.cpp`, `alkitcard.h` through
`alkitpage.cpp`, `alvcammonitorstrip.h` through `lltexturectrl.cpp`,
`alvcammediafilepolicy.h` through `alvcammediafilepolicy.cpp`, and
`alvcammediafilelifecycle.h` through `alvcammediafileio.cpp`.

The new C++ case and functional probe cover Enter/Load commit-before-Play,
focus-loss/paste cue-only dispatch, invalid Enter reaching neither replacement
nor Play, and same-source playing/pending transport coalescing. The C++ case
also checks failed source commits, empty input and paused-source resume. The
mockup tests exercise its actual handlers for the requested interactions and
confirm muted Volume.

Only the two permitted integration targets and their prerequisites were built.
No rebuilt viewer or live VLC/network/audio fixture was exercised. The optional
`VCAM_MEDIA_PROBE_FILE` fixture was unset and reports SKIPPED. Real download
progress, admission and audible playback remain runtime acceptance work;
the manifest explicitly retains that classification.
