# VCam media UI - live-test fix 1

Base: `88c254041fcacecaed30d9021d81188e6a5cc203`. The in-world build was
`ffbd7be775b`. Read `CLAUDE.md`, `MEDIA_UI_REPORT.md`, spec v8.1 section 7, and
`.tmp/VCamPage_media_mockup.dc.html`; the mockup controls layout.

No commit, push, viewer build, plugin build, or in-world test was performed.
Only the authorized `INTEGRATION_TEST_alvcammediafilepolicy` target and its
library dependencies were built. Unrelated untracked files and the dirty
`vcpkg` submodule were retained.

## Diagnosis and behavior

The screenshot alone cannot identify the user's file-policy refusal. The
source proves that failed Browse preflight retained the old committed cue
(which could be empty), did not return the rejected path to the editor, and
put its reason through `BrowseOverlay`'s five-second expiry. The empty editor
then showed its ordinary empty-cue hint. This explains how a refusal could
become invisible before player creation; it does not prove which gate refused
the unavailable user file.

Browse failures now return the picked spelling and exact public refusal to
the panel without committing the descriptor. The editor retains that draft,
colors it as an error, and disables Play. Invalid typed commits likewise keep
the draft and leave the committed source/player untouched. Refusals are kept
per capture ID/generation/cue when switching selections. They have no timer:
new Browse, typed commit, Play, Stop, or a kind switch clears the prior refusal.
Loop/Volume refreshes and underlying playback/health changes cannot erase it.
A current successful Browse clears the error, including a same-path repick.

An empty Video field now consistently reports **Enter a direct video URL**,
including the registry status after clearing a cue. The existing selftest's
replacement assertions were retained with the corrected empty/nonempty status
expectation. The spec's conflicting clear-field sentence was updated to match
the requested mockup contract.

Loop, Volume, and Source aspect use field-only registry setters. Those setters
read the current descriptor inside the registry, rather than copying the
page's deferred snapshot. Browse also merges its picked path with the latest
Loop/Volume/Aspect at completion. Neither ordering can restore an obsolete cue.
Commit/restore remain syntax-only; the new diagnostics do not add stat/fetch or
player creation to typing or commit. No file/container/track gate was relaxed.

## Diagnostics

All new viewer diagnostics use `LL_INFOS("VCamMedia")`. Source text is omitted
entirely, so URL queries, fragments, credentials, proxy settings, and rejected
input cannot leak through these diagnostics. Only allowlisted extension tokens
are retained. Native `#` filenames, URI decoding, and uppercase extensions are
handled correctly. Arbitrary exception text is never echoed to the log.

| Transition | Evidence emitted |
| --- | --- |
| Every picker return | picked, cancelled, or stale-dropped; slot/generation, extension, current-ticket/policy reason |
| Typed commit | valid/invalid, slot, syntax/descriptor policy reason, extension |
| Local preflight | request and container start/finish, admitted/refused, rule/stage, exact byte size and extension; size explicitly unknown if open was refused |
| Container rejection | specific structural rule, while preserving the exact public `This file isn't a video` status |
| Download | start, validated completion, failure, HTTP rejection status, byte count, admission/MIME/encoding/length/capacity/redirect stage |
| Cancellation/deadline | worker terminal reason; main subscription reports a terminal refusal even if its worker is still blocked |
| Play | request or refusal, including empty input, invalid slot, disabled policy, and I/O budget admission |
| Player | create/launch, track count/video presence and admission verdict/reason; launch timeout, video-track timeout, decoder/early-end/identity refusal, budget loss |
| Slot image | first successfully published frame after invalidation, completed serial and dimensions |
| Output routing | binding/slot generation, bound/unbound, quarantine/offline/not-demanding/disabled/waiting/available transitions; explicit no-display/no-projector summary |

Polling does not log progress percentages or frames. Routing keeps numeric
state per binding and logs only changes; first-frame logging is guarded by the
prior accepted-output state. Private plugin admission logs verdict/player
transitions. Worker cancellation and main subscription refusal each have a
one-shot latch. No logging was inserted in ordinary-media frame paths.

## Layout and the checker blind spot

All changes are in `vcam_page.json`, `director_kit_gen.py`, and `ALKitRow`.
Generated XUI was regenerated, never hand-edited. Other authored VCam cards
are unchanged as JSON data. The 37-page/2,122-row equality pin
and every existing checker exemption, threshold, and assertion remain intact.

| Mockup row | Final layout |
| --- | --- |
| Source / Video | Existing 100 px label track and shared control track |
| Browse / Play-Pause-Stop | Unlabelled actions start at the left grid edge; nested buttons have explicit row-owned coordinates and 24 px height inside 28 px lines |
| Loop / Volume | Normal labelled rows; transport cannot escape its measured row |
| Status | Label and first value line share their top/baseline origin; wrapping determines height instead of a fixed 60 px minimum |
| Source aspect / Freeze | Aspect retains the label/control grid; unlabelled Freeze uses the mockup's full-span action alignment |
| Privacy note | Full-width, measured wrapping; removed the fixed 76 px floor and its empty gaps |
| Media cache | Label/value share the first-line origin; measured status is immediately followed by the left-aligned Clear action |

The old code centered a 20 px label in a tall row while its value text began
at the top of a much taller text box. Fixed 60/76 px row floors created empty
space even for short content. Nested action coordinates depended on initial
XUI parent rectangles/follows behavior rather than an explicit row layout.
There was also a concrete sibling hazard: `pollLiveValue()` resized a wrapping
row immediately, then queued its card's sibling reflow for a later idle pass.
It now requests the card reflow without independently growing the row first.
These source defects are fixed; their exact native pixel presentation still
requires the next viewer build and an in-world visual check.

**Why zero findings was possible:** `calculate()` returns at kit-owned nodes;
kit children therefore have no calculated runtime rectangles. The zero-size
page root also returns from `check_file()` after only kit geometry/label-width
checks. Those checks neither constructed runtime labels nor modeled nested
actions or the later live-text resize. Zero findings did not measure the
reported layout.

The added `kit_runtime_findings()` runs before that early return and checks
conditional media rows, including XML-hidden rows. It validates nested
button containment and sibling stacking at minimum and wide card widths,
first-line label/value alignment under multiline stress, and atomic card
reflow. It fails closed if ALKitRow/ALKitCard no longer satisfy the recognized
geometry contracts or an action group lacks row ownership. Existing generic
layout checks are unchanged; this is an additional conservative check, not a
claim that static analysis executes the viewer.

Regression evidence:

- Saved HEAD row implementation plus the permanent media fixture: **7 findings**
  (`kit-runtime-overlap`, `kit-runtime-baseline`); current implementation: **0**.
- Missing action ownership with the current implementation: **1 finding**.
- Actual HEAD VCam page plus HEAD runtime source: **10 findings**; fixed page
  plus current source: **0** (`.tmp/media_live1/layout_old_vs_fixed.json`).
- A source comparison removes only diagnostic rule arguments from the
  container walker and matches HEAD exactly: predicates and limits unchanged.

## Offline probe and verification

`alvcammediafilepolicy_test.cpp`, test 35, reads `VCAM_MEDIA_PROBE_FILE`.
Unset/empty means an explicit **SKIPPED** probe; the ordinary suite still runs.
Windows reads the wide environment value so Unicode paths survive. The probe
opens a real file, supplies descriptor snapshots/offset reads to production
`validateFile()`, which calls production `checkContainer()`, and prints the
verdict, exact rule/reason, size, and extension. It does not initialize the
viewer, network, cache, or VLC. A reported probe refusal is an expected result,
not a failed test-runner assertion. Container admission alone does not assert
that VLC will find a video track or that the viewer's local-drive policy passes.

Run from the executable directory:

```powershell
Set-Location I:\alchemy-machinima\build-Windows-vs2026-os\sharedlibs\Release
$env:VCAM_MEDIA_PROBE_FILE = 'D:\Footage\the-user-file.mov'
.\INTEGRATION_TEST_alvcammediafilepolicy.exe
```

Executed probe output on on-disk container fixtures (not decoded video clips):

```text
VCAM_MEDIA_PROBE_FILE: SKIPPED (not set)
VCAM_MEDIA_PROBE_FILE: ADMITTED rule=validateFile/checkContainer reason=passed size=32 ext=mp4
VCAM_MEDIA_PROBE_FILE: REJECTED rule=dref entry is not a self-contained url/alias reason=This file isn't a video size=125 ext=mp4
```

| Check | Result / artifact |
| --- | --- |
| Authorized Release test target build | PASS; `.tmp/media_live1/test_build_final.log` |
| Policy test executable, default and admitted/refused/Unicode-path probes | **36/36 PASS in each run**; `tests_default.log`, `tests_probe_valid.log`, `tests_probe_refused.log`, `tests_probe_unicode.log` in `.tmp/media_live1` |
| `zs_check.ps1 -Files`, all touched C++ plus header consumers | **12/12 distinct TUs PASS**; `.tmp/media_live1/zs_final.log`, `zs_plugin.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **All gates PASS**, exits before purge; `.tmp/media_live1/prebuild_final.log` |
| `xui_layout_check.py --changed HEAD --json` | **40 files, zero findings**; `.tmp/media_live1/layout_final.json` |
| Layout selftests including old/current media regression | PASS; `.tmp/media_live1/layout_selftest.log` |
| Generator, page sources, functional manifest checks | PASS through guard plus explicit manifest check/selftest |
| `git diff --check` | PASS |

Header consumers checked include `alkitcard.cpp` for `alkitrow.h`,
`aluiperfselftest.cpp` for `alpaneldirectorvcam.h`, both I/O and policy-test TUs
for the container/policy/lifecycle headers, `llprismlens.cpp` and
`alpaneldirectorvcam.cpp` for `alvcammediafileio.h`, and `llviewermedia.cpp` /
`llprismprojector_test.cpp` for the prism/plugin headers.

Verification used the existing workspace syntax cache and process-local
Python/Git wrappers; no gate flags or rules were relaxed. The external syntax
script has no nested `media_plugins/libvlc` project mapping, so its separate
`-Files` invocation used `-BuildDir` pointing to an exact copy of the real
LibVLC vcxproj for flag extraction only. That TU passed with its actual Release
flags. MSBuild needed TEMP/TMP redirected into the writable workspace. The
initial deprecated `u8path` warning was fixed in code, without suppressing it.

The user's actual file was not supplied. Its refusal verdict, native picker
behavior, visible runtime geometry, playback/audio, and projector/display pixels
remain for the next viewer/plugin build and in-world run. No live PASS is claimed.
