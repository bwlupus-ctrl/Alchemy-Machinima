# VCam selector mockup match report

Date: 2026-10-18

Reference: `.tmp/VCamPage_media_mockup.dc.html`, selector markup at lines 329-352,
`camButtons` at 1848-1857, and `camRates` at 904-915.

## Mockup versus implementation

| Item | Mockup | Implementation |
| --- | --- | --- |
| Container | Inline wrapping bar, outside cards | `camera_selector` uses kit `kind="inline"`: full grid width, no card background, border, header, badge, reset/menu controls, or card padding. Existing row ownership/search remains. |
| Leading label | `Settings for`, 12px, #9BA3AE | IBM Plex Sans Kit12, `KitDim`; no selected camera name appended. |
| Camera choice | Radiogroup of compact `Cam n` chips | Mutually exclusive native toggle buttons in the selector, 24px high, 4px chip gaps, 8px outer gaps. Arrow/Home/End keys select within the group; only the selected chip is a Tab stop. Monitor clicks retain their existing independent behavior. |
| Chip width/padding | Intrinsic width; 10px horizontal padding; dot/text gap 6px | Text width plus 32px (20px padding + 6px dot + 6px gap), reserved for both regular and selected text metrics. No equal-width slot tiles. |
| Chip corners/border | Radius 4px, 1px border | Existing `Kit_RoundRect4` image, border and inset fill, using live theme references. |
| Chip labels | `Cam n` for every slot | Slot number, including media slots; camera names appear in tooltips rather than tile labels. |
| Tooltips | `Edit Cam n · <name>` | Exact wording, native button tooltip; name follows the current capture label. |
| Status dot | 6px circle; live / preview / idle | Radius 3px. Matches `camRates`: master on + visible bound face = live; otherwise previews on = preview; otherwise idle. Uses `KitOkGreen` / `KitAccent` / `KitFaint`. |
| Selected chip | #E8A33D border, #2E2A22 fill, #F1F2F4 text, weight 600 | `KitAccent` / `KitActiveFill` / `KitTextStrong`. Dedicated `KitSansSemibold` variable-font face requests weight 600; existing kit bold remains 700. |
| Other chips | #343A42 border, #1F2329 fill, #C9CED6 text, weight 400 | `KitInputBorder` / `KitInputBg` / `KitLabel`; regular Kit12 face. |
| Add control | 156px `+ Add camera…` dropdown, 24px high; `8 of 8` at capacity | Exact visible label and disabled capacity state. `Add a camera` is the accessibility label. Native combo styling uses the kit theme. |
| Add choices, in order | From selected object / Virtual cam at my view / OTS pair from Subjects A/B / Surface lens from selected face / Media from a prim face (video) | Exact five labels and existing values `object`, `virtual`, `ots`, `lens`, `media`. Media remains index 5 after the placeholder; disabled when the existing media feature gate is off. |
| Remove control | `Remove capture`, 24px, #343A42 border, transparent, only with a selected capture | Exact label/height, 10px horizontal padding, rounded 4px outline in `KitInputBorder`, transparent interior, hidden with no selected capture. Generic page restyling preserves its transparency. Existing confirmation/removal handler retained. |
| Narrow widths | Outer bar and radio group wrap | Shared width-dependent layout determines preferred height and all child rectangles; the inline kit row grows with it. Drawing and native button/combo hit targets use those same rectangles. |
| Name editing | No Name editor in this bar or elsewhere in the mockup | Preserved as the first row in the selected capture's Source card: existing `vc_camera_name` for camera/lens captures, new `vc_media_name` for media captures. Both call the existing normalized scene-persisted rename handler; focus protects in-progress edits. |

## Theme mapping

The mockup's RGB values map to existing kit segment/chip roles. No page-local RGB
constants or new palette tokens were introduced. Theme changes remain authoritative:

| Mockup role / swatch | Existing kit token |
| --- | --- |
| Label #9BA3AE | `KitDim` |
| Selected border / preview dot #E8A33D | `KitAccent` |
| Selected fill #2E2A22 | `KitActiveFill` (same selected-fill role as kit segments) |
| Selected text #F1F2F4 | `KitTextStrong` |
| Inactive/remove border #343A42 | `KitInputBorder` |
| Inactive fill #1F2329 | `KitInputBg` (same inactive-fill role as kit segments) |
| Inactive text #C9CED6 | `KitLabel` |
| Live dot #4CC38A | `KitOkGreen` |
| Idle dot #5C646F | `KitFaint` |

`KitActiveFill`, `KitInputBg`, and `KitOkGreen` have different Default swatches
from the corresponding raw mockup colors; the existing theme roles are used as
requested instead of hard-coding the mockup palette. Selected typography adds
only a non-user-selectable font face in `fonts.xml`, using the already bundled
IBM Plex variable font at weight 600.

## Behavior and coverage

- Selector/action IDs remain `vc_camera_selector`, `vc_add_camera`, and
  `vc_remove_capture`; `vc_camera_name` is retained. Existing add/select/remove,
  OTS, media gate, selection independence, rename normalization, and scene-save
  handlers remain in place.
- Source Name uses the existing control commit registration. The media Name
  row receives the existing isolated media fixture in `aluiperfselftest.cpp`.
- Search is regenerated with Name at its Source home for both capture types.
  Generator assertions reject a card wrapper, badge, Name in the bar, missing
  Source Name, or fixed-width selector row. Five negative selftest cases cover
  those regressions.
- The exact source row-count pin changes **2123 -> 2124, delta +1** with an
  adjacent explanatory comment: moving camera Name is net zero; adding its
  media Source counterpart is +1. The page count remains 37.
- Compared the old and regenerated functional manifests by version, page,
  widget, source, exercise, probe and action, ignoring generated ordinal IDs
  and card placement: **no prior exercise/action removed or downgraded**;
  only `vc_media_name` RUN is added. Native widget IDs were not renamed.
- All three generators ran: `director_kit_gen.py`,
  `director_page_sources.py` (outputs unchanged), and
  `director_functional_manifest.py`. Other generated page changes are input
  fingerprint stamps. No baseline, gate threshold, or exemption was weakened.

## Validation

Evidence directory: `.tmp/vcam_selector_match/`.

| Check | Result / evidence |
| --- | --- |
| Explicit `zs_check.ps1 -Files` | PASS, 8/8 distinct TUs: six touched .cpp files, plus `alkitpage.cpp` for `alkitcard.h` and `lltexturectrl.cpp` for `alvcammonitorstrip.h`; `zs_final.log`. Last action-label/gating follow-up rechecked, 2/2 PASS in `zs_actions_final.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | PASS, all UI/draw/compile/maths/generator/lint/parity/layout/page-source/theme gates; `prebuild_final.log`. Exits before object purge. |
| Generator `--selftest` | PASS, including five new selector regression cases; `gen_selftest.log`. |
| Functional manifest `--self-test` and `--check` | PASS; `manifest_selftest.log`, `manifest_check.log`. |
| Manifest coverage comparison | PASS, all previous exercises/actions retained; `manifest_coverage.log`. |
| XUI layout `--changed HEAD` | 41 files, **zero findings**; `xui_final.log`. |
| UI draw lint `--changed HEAD` | 0 errors, 0 new, 0 baseline additions; 3 advisory warnings at unchanged selftest lines; `draw_final.log`. |
| VCam runtime rect `--selftest` | PASS: valid fixture accepted; displaced action/value, baseline mismatch, overlapping rows and missing dump rejected; `runtime_rect_selftest.log`. |
| VCam runtime rect fixture `--require-media-modes` | PASS, zero findings; `runtime_rect_fixture.log`. |
| `git diff --check` | PASS. |

The guard initially hit PowerShell native-command errors caused by an inaccessible
per-user Git ignore path and CRLF warnings. Final checks use an empty workspace
`XDG_CONFIG_HOME` for that process, and touched source files use LF. No checker or
repository Git configuration was changed.

No commit, push, viewer build, or object purge was performed. The runtime rect
results are fixture checks; this change has not been rendered in a rebuilt viewer
or verified with a new in-world screenshot.

## Fix round 1

Date: 2026-10-18. Reference remains `.tmp/VCamPage_media_mockup.dc.html`.
This section supersedes the earlier rename, visible Add-label, and search claims.

- Both Name IDs now reach `commitCamera()` before the `vc_media_` router can
  consume them. Ordinary commits and the context menu's Paste value setter
  use that same route. Both manifest entries now select the existing
  `vcam_camera_name` oracle, with whitespace and overlength input. The oracle
  asserts the normalized selected registry label, editor read-back, and
  serialized scene label, then exercises `ALKitRow::setBoundValue()` (the native
  Paste value sink) and verifies persistence again.
- Add shows **`+ Add camera…`**, or **`8 of 8`** with the combo disabled at eight
  captures. The five choices and stable media choice index remain intact.
- The full-width inline selector row suppresses its favorite star during
  `refreshFavorite()` as well as layout. It also skips the `KitRowHover` band.
- Chips and Remove use transparent native hover/pressed images and colors
  with no glow. Their selector-owned border/fill remains visible in every
  pointer state, including after general kit restyling.
- An existing media capture retains its Source card and Name when
  `VCamMediaSlotsEnabled` is off; its other media rows remain gated. Camera,
  Surface Lens, media URL, and prim-face captures retain Name editing. The
  media Name oracle additionally changes names for both media kinds under
  all four feature/master gate combinations and checks reachability and
  registry/scene persistence, restoring both settings afterward.
- Search emits one canonical **Source › Name** result. The media row is an
  explicitly validated search alias, while both native controls and functional
  entries remain present. Local search and navigation resolve the result to
  the selected capture's editor, including navigation from another page.
- Removed `cue` from `vc_media_video` keywords. Generator tests assert that
  its synonym field contains none of `cue`, `keyframe`, or `mark`.
- `auditVCamSourceRects()` now includes `camera_selector`, its native label
  textbox and measured baseline, every chip, Add, and visible Remove. Selector
  records include item-count/selection metadata. The checker requires selector
  coverage per size, exact chip counts and Remove visibility, row containment,
  disjoint controls, and label/chip/Remove baseline alignment within the
  existing 3px tolerance. Empty and wrapped selector fixtures are included;
  missing chips/actions/labels, overlap, baseline drift, and absent selector
  coverage are rejected.

All three generators ran again. The row pin stays **2124 -> 2124, exact delta
0**, with an adjacent comment: both native Name rows remain; the search alias
does not add or remove a kit row. The manifest stays **4024 -> 4024 entries**.
Every incoming widget/source/exercise/action tuple is retained, and every HEAD
widget/exercise/action tuple remains. No baseline or acceptance threshold was
changed. New draw-lint annotations cover only bounded test-fixture setting
changes and their restoration.

Evidence: `.tmp/vcam_fix_round1/`.

| Check | Result / evidence |
| --- | --- |
| Explicit `zs_check.ps1 -Files ... -IncludeTests` | **14/14 PASS**, including all uncommitted touched .cpp files and additional header consumers; `zs_final.log`. |
| `prebuild_guard.ps1 -ChecksOnly` | **All gates PASS**, exits before object purge; `prebuild_final.log`. |
| XUI layout `--changed HEAD` | **41 files, zero findings**; `xui_final.log`. |
| Kit generator `--selftest` | PASS, seven selector/search regressions rejected, one Name hit and no cue synonyms asserted; `gen_selftest_final.log`. |
| Functional manifest `--self-test` / `--check` | PASS, both Names use the registry oracle; `manifest_selftest_final.log`, `manifest_check.log`. |
| Functional coverage comparison | PASS against incoming tree and HEAD; `manifest_coverage.log`, `verify_coverage.py`. |
| Runtime rect checker `--selftest` / fixture `--require-media-modes` | PASS, zero fixture findings; `runtime_rect_selftest.log`, `runtime_rect_fixture.log`. |
| `git diff --check` | PASS; `diff_check.log`. |

The new panel/manifest probes were syntax-checked but **not executed in a rebuilt
viewer**. The rect CSV used here is a checker fixture, not a new viewer dump.
A real post-layout dump and live hover/pressed/rename verification require a
viewer build. No viewer build, integration-target build, commit, push, or object
purge was performed in this fix round.
