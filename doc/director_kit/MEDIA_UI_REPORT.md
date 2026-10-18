# VCam media Director UI

Implemented on top of `a42a4695519`, `ffa4842f0ce` and `c6e16716846`, against
`SPEC_media_projector.md` v8.1 and `.tmp/VCamPage_media_mockup.dc.html`.
No build, commit or push was performed. No credentials or secrets were read or
stored. The pre-existing dirty submodule and unrelated untracked files remain.

## Control and value-wiring audit

All source controls address the selected **capture handle, including its
generation**. They do not address a camera-list ordinal. The generated page
uses `bind="none"` for these owner-bound controls; no global setting stands in
for a per-slot descriptor. Source edits call the slice-2b API directly.

The table names manifest probes, not executed runtime results. Native picker,
filesystem, network, VLC/audio/GL and live face interactions remain explicit
runtime acceptance. The new selftest code was syntax/semantic checked.

| Mockup element | XUI ID | Setting / handler and value contract | Refresh read-back | Manifest probe | Search term |
| --- | --- | --- | --- | --- | --- |
| Add: Media from a prim face (video) | `vc_add_camera`, value `media` | `ALPanelDirectorVCam::onControl`: add/select slot, then `setMediaSource` with an unbound face; only converts after successful Add | Selector resets to `+ Add`; selected handle and slot list refresh from registry | `vcam_media_add`, kind `media` | Add media, prim face |
| Add: video-file source | `vc_add_camera`, value `video` | Add/select default `MEDIA_URL`; empty cue, Loop true, Volume 0 | Selector resets; registry selection and list refresh | `vcam_media_add`, kind `video` | Add video, local file |
| Existing Add media slot | `vc_add_media_slot` | `ALVCamPageModel::addMediaSlot`; default private URL slot | Enabled below capacity; hidden when kill switch is off | Existing `vcam_media` count/kind oracle | Add media slot |
| Slot Name | `vc_camera_name` | Existing `setLabel`, normalized scene label | Selected slot `mLabel` | Existing `vcam_camera_name` | Name |
| Source: Video URL / Prim face | `vc_media_kind` | Segmented string data **`url` / `face`**; `setMediaUrlSource` / `setMediaSource`; preserves aspect and never plays on switch | Tagged descriptor determines selected segment and conditional rows | `vcam_media_file` | media, video URL, prim face, VLC |
| Video | `vc_media_video` | Text, max 8192; shared pure `ALMediaFile::classify`, then atomic `setMediaUrlSource`. Enter cues only. Invalid draft remains visible; exact refusal and disabled Play | Selected descriptor on selection/source change; local draft/error retained during refresh | `vcam_media_file`; isolated `syntax` and `scene_restore` | video URL, local file, path |
| Browse… | `vc_media_browse` | `browseMediaFile`; async `FFLOAD_VIDEO`; panel handle, capture generation, selection, configuration revision and edit epoch guard the callback/preflight | Successful current result updates descriptor and Video field; cancel/failure retains committed source; runtime reason shows preflight/failure | `vcam_media_runtime` **DEFER_INWORLD**; isolated `browse_ticket` | Browse, local file, video picker |
| Play | `vc_media_play` in `vc_media_transport` | Commit current valid draft, then `playMediaFile`; only explicit Play can start I/O/playback | Enabled only for valid nonempty syntax; playback/admission/download reason read from model | `vcam_media_runtime` **DEFER_INWORLD**; isolated `admission` | Play, VLC |
| Pause | `vc_media_pause` in `vc_media_transport` | `pauseMediaFile`; keeps desired Pause through pending download/startup | Enabled for committed nonempty private cue; model playback status | `vcam_media_runtime` **DEFER_INWORLD**; admission transport oracle | Pause |
| Stop | `vc_media_stop` in `vc_media_transport` | `stopMediaFile`; also invalidates pending Browse epoch | Private-kind enable and stopped runtime reason | `vcam_media_runtime` **DEFER_INWORLD**; lifecycle teardown oracle | Stop |
| Loop | `vc_media_loop` | Boolean `MediaUrlSource::mLoop`, default **true**; `setMediaUrlSource` | Selected descriptor Boolean | `vcam_media_file`; admission/end policy helpers | Loop |
| Volume | `vc_media_volume` | F32 **0–1**, step **0.05**, precision **2**, default **0**; `setMediaUrlSource` | Descriptor value; read-only native slider text **Muted / N%** | `vcam_media_file` | Volume, muted, audio |
| Prim face | `vc_media_face` | Read-only authored object name (UUID fallback) and TE | Selected face descriptor; `None picked` when unbound | `vcam_media_readout` with face-kind prerequisite | prim face source |
| Pick prim face / replace | `vc_pick_media_face` in `vc_media_face_actions` | Existing transient pick tool; Escape cancels. Handle, selection and configuration revision guard commit | Face descriptor and runtime status after validated pick | `vcam_media_pick` **DEFER_INWORLD / requires_world** | Pick, replace, prim face |
| Locate face | `vc_media_locate` in `vc_media_face_actions` | Existing `handle_zoom_to_object` on the authored source object | Resolves authored source only; never substitutes another face | `vcam_media_runtime` **DEFER_INWORLD** | Locate face |
| Required clear-source action | `vc_media_clear_face` | `setMediaSource` with null object/TE -1, retaining aspect | Face becomes `None picked`; source status refreshes | `vcam_media_runtime` **DEFER_INWORLD** | clear prim face |
| Status | `vc_media_status` | Read-only exact shared policy/runtime strings; draft refusal takes precedence | `CaptureRuntimeState::mReason` or current syntax error | `vcam_media_readout`; syntax/admission helpers | status, download, VLC |
| Source aspect | `vc_source_aspect` | Combo S32 **0 Auto / 1 16:9 / 2 4:3 / 3 2.39**; selected descriptor setter | `mediaAspect()` for either kind | Existing `vcam_media` aspect/revision oracle | source aspect, Auto |
| Source geometry / image detail | `vc_media_geometry` | Read-only runtime snapshot; player, playback, video width/height, SAR, resolved aspect/fallback and accepted-image readiness | `CaptureDefinition::mMediaInfo`; no player creation or I/O | `vcam_media_readout` | geometry, SAR, aspect, fallback, image |
| Freeze / Resume | `vc_media_running` | Button calls `setMediaRunning(!mMediaRunning)`; Freeze requires an accepted frame; Resume does not Play | Button label and value reflect `mMediaRunning`; disabled Freeze without output | Existing `vcam_media` consumption/demand/revision assertions retained; Resume fixture needs no GL frame; isolated `demand_teardown` | Freeze, Resume, image |
| Private / prim note | `vc_media_note` | Read-only, kind-specific spec privacy copy | Selected descriptor kind | `vcam_media_readout` | private video, local file |
| Clear video cache | `vc_media_cache_clear` | `clearMediaFileCache`; owner-only; stops cache-backed slots/cancels HTTP, retains cues/local files | Enabled only for feature-on cache owner | `vcam_media_runtime` **DEFER_INWORLD**; isolated `clear` | Clear video cache, download |
| Media cache usage/status | `vc_media_cache_status` | Read-only `ALMediaFile::cacheStatus()` snapshot, bytes/limit only for owner | Diff-guarded visible-page idle refresh at **2 Hz**; lock and Clear/degraded status shown | `vcam_media_readout`; isolated `cache_policy` | cache usage, limit, download |

Transport and face action groups are generated native button children of a
single kit row, preserving the mockup's horizontal button groups. Their row
keywords include each action, so search reveals the containing action row.
The functional manifest separately inventories every child button; isolated model probes use distinct widget identities with an explicit target widget, preserving the native-action hazard guard. The group
containers are explicitly non-interactive. Add selector actions are declared
in `vcam_page.json` and have separate zero-camera creation/selection oracles.

The existing source-aspect and consumption assertions were retained. The
consumption fixture starts frozen and exercises **Resume**, because the new UI
correctly disables Freeze without an accepted GL frame. URL/face prerequisites
now select the appropriate tagged source instead of assuming every media slot
contains `MediaFaceSource`.

## Scene save/load

The committed slice-2b writer/parser/apply path already carries these fields;
the new controls write those same descriptors:

| Field | Contract |
| --- | --- |
| `capture_id`, `label` | Stable source-slot identity and normalized label |
| `source_kind` | `media_url` or `media_face` |
| `source_location` | Private source: `http_url` or `local_file` |
| `source_url` / `source_path` | Exactly one authored HTTP(S) URL or absolute native local path; no runtime/cache URI |
| `source_loop` | Boolean; missing defaults true |
| `source_volume` | Finite 0–1; missing defaults zero |
| `source_aspect` | 0–3, shared across both media kinds |
| `source_object_id`, `source_te` | Face-kind source identity; missing/offline target is retained |
| `output_rate_mode`, `target_output_fps` | Existing common slot rate configuration |
| Existing display/projector/gate records | Retain source-slot references and common output configuration |

Restore remains **syntax-only**: the private descriptor parser classifies text
and validates fields before mutation. It does not call Browse, Play, stat,
drive/reparse checks, cache lookup, downloader or navigation. Owners are
retired before replacement. Private slots return **cued/stopped**, without a
player or demand, even with saved nonzero Volume or Loop on. A later explicit
Play performs all local/network/content gates. Runtime player, cache, draft,
image and transport state are not serialized.

The new `scene_restore` oracle passes two media-only descriptors through the
production writer/parser/apply path: an unresolvable HTTP host and a nonexistent
local path, with Loop false, Volume .35, aspect 4:3 and 12 fps. It checks the
round trip, zero cameras, no accepted output and zero media demand. The ordinary
source-control oracles also round-trip their resulting scene. These probes
have been compiled, **not executed** under the no-build constraint; the no-I/O
claim additionally follows the unchanged syntax-only production call path.

## Enable and kill switch

`VCamMediaSlotsEnabled` now defaults **true**. Its existing disable callbacks,
private-owner retirement, token/subscription cancellation, static-face/cookie
fallback, owned-volumetric release, process-lifetime cache lock and bounded quit
behavior are unchanged. Authored descriptors remain; re-enable requires fresh
Play for private sources. The Add menu removes media choices while disabled.
`PrismLensEnabled` remains the overall master.

## Mockup differences

- Added the requested small **Clear video cache** control and owner-only cache
  usage/status in Source, after the privacy note; the mockup has no cache UI.
- Added **Clear face** for the spec's clear-source flow, below the mockup's
  Pick/Locate row. Escape retains the existing cancel flow.
- Added a separate geometry/image readout so long exact refusal/download status
  cannot hide player, SAR, fallback or image readiness information.
- The privacy notes use the required v8.1 wording. Required exact refusal and
  cache status strings supersede illustrative mockup status copy.
- **Locate face** uses the viewer's existing object-framing operation, rather
  than the illustrative mockup's flash animation.
- Retained the prior **Add media slot** shortcut as well as adding both media
  choices to the existing Add selector. The default shortcut creates Video URL.
- Existing slot/media badges and the kit's scrollable card system remain; media
  monitor titles now say **MEDIA** instead of CAM. Camera-only cards remain
  hidden for media, while common output/rate controls use the existing slot model.

The generator's exact inventory tally is **2,125 rows**. The user explicitly
approved updating the 2,114 pin to the exact new count. Independent comparison
against HEAD confirms **VCam 179 -> 190 (+11)**, **all pages 2114 -> 2125 (+11)**,
and unchanged other-page source data. The added rows are:

- `vc_media_kind`
- `vc_media_video`
- `vc_media_browse`
- `vc_media_transport`
- `vc_media_loop`
- `vc_media_volume`
- `vc_media_clear_face`
- `vc_media_geometry`
- `vc_media_note`
- `vc_media_cache_clear`
- `vc_media_cache_status`

The previous `vc_pick_media_face` row is replaced by `vc_media_face_actions`
(one row for one row, net zero). Its existing Pick button ID is retained inside
that action group alongside Locate face. Play/Pause/Stop occupy one transport
row. The comment reads `2125: VCam media source card adds 11 rows; exact coverage
stays pinned.` The exact-equality check and 37-page check remain intact. No other
gate, checker, threshold, legacy/shrink-only pin or hazard allow-list was changed.
Intermediate generated manifests were regenerated against the committed
inventory, so promotion review used the actual baseline.

## Verification

All required checks passed; final outputs and artifact references follow.
No checker, baseline, threshold or test assertion was weakened. The unrelated
existing `ALKitPage::setWhoChipLabel` raw-write lint finding was fixed by using
the equivalent shared `setLabelIfChanged` helper, retaining its conditional
relayout and both-label comparison.

| Required check | Final result / evidence |
| --- | --- |
| `zs_check.ps1 -Files` on every touched `.cpp` | **PASS: 6/6 TUs**, exit 0; `.tmp/media_ui/zs_final.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **PASS: every gate**, exit 0; `.tmp/media_ui/prebuild_final.log` |
| `xui_layout_check.py --changed HEAD --json` | **40 touched XUI files, ZERO findings**, zero failed files; `.tmp/media_ui/layout_final.json` |
| `director_functional_manifest.py --check` | **PASS**, current manifest; `.tmp/media_ui/manifest_check.log` |
| `director_functional_manifest.py --self-test` | **PASS: 12 rule fixtures plus generation selftest**; `.tmp/media_ui/manifest_selftest_final.log` |
| `git diff --check` | **PASS**, exit 0; `.tmp/media_ui/diff_check_final.log` |

Required per-file syntax output:

```text
PASS  indra\llui\llsliderctrl.cpp  [llui.vcxproj]
PASS  indra\newview\alkitpage.cpp  [alchemy-bin.vcxproj]
PASS  indra\newview\alpaneldirectorvcam.cpp  [alchemy-bin.vcxproj]
PASS  indra\newview\aluiperfselftest.cpp  [alchemy-bin.vcxproj]
PASS  indra\newview\alvcammonitorstrip.cpp  [alchemy-bin.vcxproj]
PASS  indra\newview\llprismlens.cpp  [alchemy-bin.vcxproj]
6 TU(s), 6 PASS, 0 FAIL, 16.0s
```

Full checks-only guard output:

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

The supported `CachePath` option directs the syntax flag cache into the writable
workspace. Process-local wrappers select the existing Python 3.14 executable
and retain all inherited Git configuration while substituting a readable empty
`core.excludesFile`. No gate flags, selection, baseline, test assertion or limit
was relaxed. `ChecksOnly` exited before object purging.

All generators ran from their authoritative inputs. Other generated kit XUI
files have source-bundle header updates; the VCam page contains the functional
UI change. The final artifact hash inventory is
`.tmp/media_ui/final_source_hashes.json`.

An independent final verification also passed: six `-Files` syntax checks
(`.tmp/media_ui/root_zs_verified.log`), the unchanged full ChecksOnly guard
(`.tmp/media_ui/root_prebuild_final.log`), and all 40 changed XUI files with zero
findings (`.tmp/media_ui/root_layout_all.log`). The exact 11-row delta and
unchanged other-page data are recorded in `.tmp/media_ui/root_inventory.log`.
`.tmp/media_ui/root_control_audit.log` verifies every media row and nested action
has a manifest entry, every media row is indexed for search, the Source and
aspect data values are exact, and Volume retains its 0–1 / 0.05 contract.


Native Browse/cancel/stale callbacks, real HTTP/local VLC playback, audible
Volume, live face picking, GL output routing, cache fault injection and kill
switch runtime acceptance still require the resulting viewer/plugin build and
an in-world run. No runtime PASS is claimed by these source and gate results.

## Fix round 1

This section supersedes the earlier Add choices, MEDIA badge, extra rows,
Resume-only fixture, global tracking, and validation/coverage claims above.
The mockup takes precedence. No build, commit or push was performed.

The table distinguishes executed source/XUI/manifest audits from added C++
regressions. The latter passed syntax/semantic checking, but were **not run**:
doing so against these changes requires a new test/viewer binary, which this
task forbids building. Real playback, frame pixels, audio and native picker
acceptance remain unverified at runtime.

| Item | Change, file:line | Test / evidence |
| --- | --- | --- |
| 1 | indra/newview/llviewermedia.cpp:1888 and :1998 query per-impl consumption. indra/newview/llprismlens.cpp:2905 resolves only occupied slot consumers; :2913 synchronizes those plugins with both enables, resolving each slot once. Bind/unbind, slot deletion, scene replacement, reconciliation, private-player creation, and both setting transitions resynchronize tracking (:10223). Ordinary plugins receive a no-op disabled configuration. | Added indra/newview/tests/llprismprojector_test.cpp:703 (test 34) and :755 (test 36). The latter exercises the real LLPluginClassMedia updated handler, comparing serialized dirty/timing/layout/token/geometry state bytes for feature ON/no consumer versus OFF, then binding and unbinding CEF/VLC tracking. Source audit 1 PASS. GPU upload/pixel equality still needs runtime acceptance. |
| 2 | indra/newview/alpaneldirectorvcam.cpp:1042 returns on an invalid draft without any Stop/source setter. The draft/error remain and Play stays disabled. indra/newview/llprismlens.cpp:2547 keeps a current owner independent of the newly authored cue; :2613 cues without retiring that owner, and :2649 replaces it only on explicit Play. Active startup/end/transport uses the owner's source. Cache Clear also checks the active owner's location. | Added actual editor invalid-commit checks at indra/newview/aluiperfselftest.cpp:7663: committed cue and configuration/runtime revisions stay unchanged, draft/error remain, Play is disabled. Source audit 2 PASS. This isolated fixture is unloaded; repeat invalid focus-loss and valid cue replacement while a real video plays to verify continuing video/audio. No live-player PASS is claimed. |
| 3 | scripts/code_tools/director_kit/vcam_page.json:22 has exactly one media Add choice, “Media from a prim face (video)”. indra/newview/alpaneldirectorvcam.cpp:1321 uses default URL-slot creation without converting to face. The extra file choice and duplicate Add button are removed. | Add-selector oracle now requires an empty URL cue, Loop ON, Volume 0 and zero camera/output/demand; source audit 3 and generated inventory PASS. |
| 4 | scripts/code_tools/director_kit/vcam_page.json:25 uses the camera scope template; indra/newview/alpaneldirectorvcam.cpp:910 renders “Cam n”. | Source/XUI audit 4 PASS. |
| 5 | scripts/code_tools/director_kit/vcam_page.json:38 and :39 declare cache rows only for URL mode; indra/newview/alpaneldirectorvcam.cpp:919 enforces this. Clear face and geometry/image detail were absent in HEAD and added during the UI round, so both are removed. | HEAD comparison, source audit 5 and generated inventory PASS. |
| 6 | indra/newview/alpaneldirectorvcam.cpp:950 and :1038 display “Enter a direct video URL” for empty text and “Pick a prim face” for an unbound face, distinct from Stopped and invalid text. Registry creation/binding statuses agree. | Source audit 6 PASS; invalid-draft functional assertion requires the exact refusal. |
| 7 | indra/newview/llprismlens.h:795 retains each inactive descriptor on the slot. indra/newview/llprismlens.cpp:2927 switches kind while preserving both descriptors/aspect, stops former private ownership, and never starts another player. An offline previously picked face can be restored without a new pick. | Added llprismprojector_test.cpp:727 (test 35): repeated toggles preserve typed path, picked object/TE, Loop, Volume and aspect, including descriptor copying. Source audit 7 PASS. Scene format still writes the active kind, as specified. |
| 8 | scripts/code_tools/director_kit/vcam_page.json:33: Locate says “Move your camera to the source prim face.” | Source/generated tooltip audit 8 PASS. |
| 9 | scripts/code_tools/director_kit/vcam_page.json:31 retains Volume help and native volume_text; the refresh-time tooltip replacement is removed. | Source audit 9 PASS; existing slider Muted/N% formatting and 0–1 / .05 contract retained. |
| 10 | indra/newview/alpaneldirectorvcam.cpp:1051 commits the authored draft spelling. indra/newview/llprismlens.cpp:2630 and :7716 retain it through commit/scene restore; classification and file URI conversion normalize only the internal I/O descriptor. Browse also retains the picker's spelling. | Test 35 checks backslashes survive while the internal URI becomes file:///D:/Footage/clip.mov; source audit 10 PASS. |
| 11 | indra/newview/alpaneldirectorvcam.cpp:941 enables Pause only for active/pending transport, not cue presence. :1147 handles Play refusal. indra/newview/llprismlens.cpp:2654 reports master/media policy refusal. indra/newview/alvcammediafileio.h:48 defaults to “Cache idle”; active cache startup reports checking only after service creation. | Source audit 11 PASS. Pending-startup Pause remains supported; real policy/transport acceptance is deferred explicitly. |
| 12 | indra/newview/aluiperfselftest.cpp:6640 starts the consumption fixture unfrozen; :7628 verifies native Freeze, released demand and revision change, then clicks native Resume and checks its revision. scripts/code_tools/director_functional_manifest.py:98 gives exact runtime reasons; :679 classifies existing native widgets explicitly without bypassing missing/no-exercise rows. The broad prerequisite exception is removed. vc_monitor_header returns to its HEAD classification (NO_EXERCISE for generated custom control). | Original first-click consumption assertions retained and strengthened with Resume. Source audit 12 and manifest rule/generation selftests PASS. The new setting-write allowance is removed; fixture enable/restore setup moved into existing beginFunctionalEntry setup (aluiperfselftest.cpp:6928), whose pre-existing allowance is unchanged. No allowance file was changed. Promotion checks ran against the actual HEAD inventory, not an intermediate generated file. |
| 13 | indra/newview/llprismlens.cpp:7015 no longer resolves plugins, queries completed images, or formats the removed UI-only geometry summary in snapshot(). The removed row needs no replacement UI query. Only a cheap transport Boolean is copied at :7041. | Source audit 13 PASS; syntax and UI draw gate PASS. |
| 14 | indra/newview/alpaneldirectorvcam.cpp:1157 guards Browse by panel lifetime, selected slot handle/generation and edit epoch. Native/preflight callbacks also validate the registry handle. Global configuration revision is no longer a condition, so unrelated registry edits do not discard picks. | Source audit 14 PASS; existing picker-ticket stale/cancel/preflight lifecycle probes retained. Native unrelated-edit/stale-handle acceptance remains deferred. |
| 15 | indra/newview/alpaneldirectorvcam.cpp:969 refreshes rate values/readout and Target FPS/preset visibility in one common capture branch for both camera and media. | Source audit 15 PASS; syntax PASS. |
| 16 | indra/newview/llprismlens.cpp:2718 calls a dedicated success callback only after accepted preflight/commit. indra/newview/alpaneldirectorvcam.cpp:1171 invalidates the editor read-back handle, resetting draft/error/Play even if the path is identical. Failure/cancel cannot run this reset. | Source audit 16 PASS; existing Browse success/failure/ticket probes retained. Same-path native picker acceptance still requires the viewer. |
| 17 | The kit has no source-kind view override. indra/newview/alpaneldirectorvcam.cpp:283 supplies a reveal guard with a source-switch hint. indra/newview/alkitpage.cpp:1405 returns false, and indra/newview/alkitshell.cpp:1254 / :1273 preserve that failure instead of falling back to a successful legacy reveal. Authored kind is unchanged. | Source audit 17 PASS; syntax and UI draw gates PASS. |

The exact pin is now **2,122 rows across 37 pages**, with **187 VCam rows**.
Compared with this UI round's prior 2,125/190, removing duplicate Add,
Clear face and geometry removes three rows. Compared with HEAD, the net change
is **2,114 → 2,122 (+8)** and **179 → 187 (+8)**. Other-page authored data is
unchanged. Exact equality, page count, legacy/shrink pins, hazard guard,
thresholds and checker assertions remain intact. Independent tally:
.tmp/media_ui/round1_inventory.json.

All three generators ran: director_kit_gen.py, director_page_sources.py
and director_functional_manifest.py. Generated XUI was never edited by hand.
Final verification:

| Check | Result / artifact |
| --- | --- |
| zs_check.ps1 -Files on every touched .cpp, including tests | **10/10 PASS**; .tmp/media_ui/round1_zs_final.log |
| prebuild_guard.ps1 -ChecksOnly | **All gates PASS**, exits before object purge; .tmp/media_ui/round1_prebuild_final.log |
| xui_layout_check.py --changed HEAD --json | **40 touched XUI files, zero findings, zero failed files**; .tmp/media_ui/round1_layout_final.json |
| Functional manifest --check / --self-test | **PASS**, including all 12 rule fixtures; .tmp/media_ui/round1_manifest_check.log, .tmp/media_ui/round1_manifest_selftest.log |
| Per-item source/XUI/manifest audit | **PASS**; .tmp/media_ui/round1_audit.json |

Verification uses the existing workspace cache option and process-local Python/
Git wrappers from the earlier report, retaining inherited Git configuration.
Edited sources use the repository's LF line endings. No gate was changed to
work around execution-policy, line-ending, or temporary-script import issues.

## Fix round 2

This section supersedes round 1's keep-old-player-running behavior and its
frame-free Freeze fixture. Spec v8.1 and the mockup require a validated source
replacement to stop the old player and cue the replacement. No build, commit
or push was performed; no secrets were stored. Gates, inventory pins, checker
rules and existing ordinary-media assertions remain intact.

| Item | Change, file:line | Tests / evidence |
| --- | --- | --- |
| 1: replacement teardown | `indra/newview/llprismlens.cpp:2547` matches generation **and location/value**. `:2628` calls `stopPrivate` for every source replacement, including URL-to-URL and clearing Video, before descriptor mutation. `:2570` advances the token, sets desired Stopped, cancels the old download/preflight subscription, removes the current owner and retires its player. The late-completion path at `:2749` requires a current matching owner. `:2633` publishes **Set. Press Play to start it.**; this string is added to `doc/director_kit/SPEC_media_projector.md:320`. The invalid-draft early return remains at `indra/newview/alpaneldirectorvcam.cpp:1065`. | Added test 37 at `indra/newview/tests/llprismprojector_test.cpp:836`: B/empty replacements cannot match A in the same generation, acquire demand or retain a current owner for late completion. `indra/newview/aluiperfselftest.cpp:7665` retains invalid-editor descriptor/revision/error assertions and adds valid B/clear stopped-cue/read-back assertions at `:7680`. Source audit verifies validation-before-teardown and cancellation-before-replacement. C++ regressions syntax/semantic checked, not executed. |
| 2: Freeze gate | `indra/newview/alpaneldirectorvcam.cpp:950` restores `enabled && (!running || mHasOutput)`. `indra/newview/aluiperfselftest.cpp:6671` asserts that a fresh, stopped slot with no accepted frame disables Freeze, then prepares the frozen fixture for a native Resume click. `:7637` requires that assertion, resumed running state, the original consumption/demand checks and a runtime revision change. | The harness has no accepted GL frame. `scripts/code_tools/director_functional_manifest.py:1722` explicitly records Freeze as **DEFER_INWORLD**, with the true accepted-frame prerequisite, in the existing row's `deferred_coverage`; that row's executable coverage is fresh-slot disable plus native Resume. No extra inventory row or count-pin change. Real Freeze/held pixels/playback continuity remain deferred. |
| 3: ordinary media | `indra/newview/llviewermedia.cpp:1811` and `:2120` gate epoch/reset changes through the same per-impl `mediaFrameTrackingEnabled(this)` / `consumesMedia` predicate. `indra/llplugin/llmediaframestate.h:93` shares that source-change operation with the regression. Ordinary GL handoff at `indra/newview/llviewermedia.cpp:3589` uses the equivalent extracted `syncLegacyUpload` at `indra/llplugin/llmediaframestate.h:32`. | Test 36 at `indra/newview/tests/llprismprojector_test.cpp:755` retains real CEF/VLC updated-message byte comparisons and adds feature-ON/no-consumer versus OFF epoch equality across both source-change events, pending-upload completion, worker `syncToMainThread` and main-thread `syncTexName` handoffs. Consumed obsolete uploads still fail. Source audit proves the remaining legacy upload body equals HEAD after expanding that two-branch helper. No GPU/pixel runtime PASS claimed. |
| 4: Browse/status | `indra/newview/alpaneldirectorvcam.cpp:1196` only marks the model dirty after successful Browse; `indra/newview/alvcampagemodel.h:86` exposes the existing idle-queued dirty mechanism. No synchronous refresh or registry snapshot is called from that reconciliation callback. `indra/newview/alpaneldirectorvcam.cpp:957` separates refused-Play presentation from syntax errors, expires it after five seconds or a relevant policy change, and immediately accepts newer live status. The visible-page timer queues its expiry refresh. | Source audit checks the callback for dirty-only refresh, preserves the invalid-draft guard, and checks the expiry/policy path. A retained stopped-registry refusal also expires from presentation, while new live status supersedes it. Native Browse/reentrancy and timed policy recovery require runtime acceptance. |
| 5: search scope | `indra/newview/alkitshell.cpp:1253` and `:1273` reject only an explicit row-kind guard denial. Normal failed row reveals again fall back to card or legacy-widget reveal. `indra/newview/alkitpage.h:121` allows rows by default; the sole guard installation is the VCam panel. | Executed source regression removes only those two guard-rejection lines and compares the entire `navigateTo` function byte-for-byte with HEAD: **identical**. Therefore other pages retain the prior row/card/legacy fallback behavior. |

All three generators ran from their authoritative inputs: `director_kit_gen.py`,
`director_page_sources.py` and `director_functional_manifest.py`. Generated
artifacts were not hand-edited. The initial guard identified stale page-source
output after owner edits; regenerating it resolved that failure without changing
the gate. The final exact inventory pins and all assertions/checkers are retained.

| Final check | Result / artifact |
| --- | --- |
| `zs_check.ps1 -Files` on **every working-tree-touched .cpp**, including tests | **10/10 PASS**, `.tmp/media_ui/round2_zs_final.log` |
| `prebuild_guard.ps1 -ChecksOnly` | **All gates PASS**, exited before object purge; `.tmp/media_ui/round2_prebuild_final.log` |
| `xui_layout_check.py --changed HEAD --json` | **40 files, zero findings, zero failed files**; `.tmp/media_ui/round2_layout.json` |
| Functional manifest `--check` / `--self-test` | **PASS**, including 12 rule fixtures and generation selftest; `.tmp/media_ui/round2_manifest_check.log`, `.tmp/media_ui/round2_manifest_selftest.log` |
| Source/navigation/ordinary-upload audits | **PASS**; `.tmp/media_ui/round2_audit.log` (script `.tmp/media_ui/round2_verify.py`) |

The added/updated C++ tests passed `/Zs` syntax and semantic checks; they were
**not executed**, because doing so requires building a changed test/viewer binary.
Required live acceptance remains: Play A then commit B/clear (A stops, its
subscription cancels, B stays cued/stopped); finish A's old download after that
replacement (no player/playback); enter an invalid draft during A playback
(A continues); accepted-frame Freeze; native Browse callback and policy/expiry
refresh; real GL upload/pixel equality. These are not reported as runtime PASS.

## Open P2s carried to the next batch (Opus fix-round-2 review)

1. Clearing the Video field shows "Set. Press Play to start it." while Play is disabled (llprismlens.cpp:2633, alpaneldirectorvcam.cpp:967); the mockup shows "Enter a direct video URL" for an empty field.
2. The Loop/Volume/Aspect handlers copy the whole descriptor from the model snapshot (alpaneldirectorvcam.cpp:1199-1205, aspect :1489+). In the frame right after a deferred Browse commit, they can revert the pick and stop playback, a one-frame race. Fix: field-only setters.
