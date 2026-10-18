# VCam media UI â€” live-test fix round 2

Built on the uncommitted round-1 tree (base `88c254041fc`), after reading its diff,
`MEDIA_UI_LIVE1_REPORT.md`, `CLAUDE.md`, media spec v8.1, both loop/flare diagnostics,
and `.tmp/VCamPage_media_mockup.dc.html`. Existing round-1 fixes are retained.
No commit, push, viewer build, plugin build, or live/hardware-capture test was performed.
Only the three authorized integration targets and their library dependencies were built.

## Same-input loops and rebuild containment

- `indra/media_plugins/libvlc/media_plugin_libvlc.cpp:219` calls production
  `ALMediaFile::restartLoop()` (`indra/newview/alvcammediafilepolicy.h:344`).
  Admitted EOF + Loop + desired Playing restarts the existing player. VLC 3's
  Ended state needs transport stop before play; this stop does **not** call
  `stopPlayer()`, release media/context/input, or replace the player. It clears
  quiesced stale EOF/frame-ready flags, sets time zero, and plays the same media.
  Duration is absent from the decision, including unknown-duration files.
- The successful branch publishes Playing, never transient Done/Pending. There
  is no player/load-token renewal, admission reset, layout/source epoch change,
  gain drop, or ownership release. Existing accepted slot/cookie pixels remain
  until the next completed upload/copy replaces them through the existing path.
- POSIX `unchangedFile()` runs immediately before restart (`:215`); mismatch
  refuses and tears down. The existing viewer lease/deny-write handle remains
  owned across transport restart and fallback. No file/container/track gate was
  relaxed. POSIX fallback still reopens against the pinned identity (`:573`).
- A failed same-player start alone permits `playMedia(true)` (`:233`).
  `Admission::rebuild()` (`alvcammediafilepolicy.h:321`) records hold provenance
  only when replacing an already admitted loop. The replacement gets a new
  player token, a fresh bounded verification window, zero gain, and no frame
  publication until its own video-track admission.
- The tokened admission message carries `loop_reverify` (`media_plugin_libvlc.cpp:171`).
  The parent accepts that hold only from a previously admitted load
  (`indra/llplugin/llpluginclassmedia.cpp:1197`). Pending still invalidates dirty
  pixels and upload/layout provenance (`:1219`); it never authorizes new pixels.
  `privateFileRetaining()` (`llpluginclassmedia.h:174`) also checks desired
  transport and the existing verification deadline.
- `indra/newview/llprismlens.cpp:6626` retains volumetric ownership for this
  narrow hold as well as normal admitted playback. Stop, refusal/timeout,
  budget/policy loss, disable, and source replacement keep the existing full
  teardown. A new URI resets hold/admission (`llpluginclassmedia.cpp:836`).

## Measured flare energy

The previous shader already had a soft knee: `smoothstep(threshold,
threshold + max(threshold * .5, .1), luma)`. Its sampled energy lacked temporal
history. The fix preserves that transfer and its bright-energy cap.

- `indra/newview/app_settings/shaders/class1/alchemy/cineFlareEnergyF.glsl:12`
  computes the same nine isolated-shaft samples once per stable source, applies
  the existing soft knee, and stores an EMA. Two 20Ã—1 R16F targets ping-pong;
  history follows rig/light index plus emitter UUID, never packed flare order.
- `indra/newview/pipeline.cpp:13612` runs the tiny GPU pass, with production
  `ALFlareEnergy::alpha()` (`indra/newview/alflareenergy.h:9`) supplying
  `1 - exp(-dt/tau)`: 60 ms attack, 250 ms release, actual elapsed dt.
  Absent sources decay their last measured energy toward zero, including when
  the shaft target disappears. The placeholder scene sampler is never sampled
  for an absent source. Release does not switch to modeled energy.
- `pipeline.cpp:14009` explicitly zeroes reset flags; emitter changes reset only
  their stable history cell (`:14086`). Source-off/kill-switch and resource-loss
  paths invalidate history. The pass balances render-target bind/flush and
  restores blend, depth, scissor, sRGB, color mask, active texture unit, sampler
  texture bindings (including allocation's unit 0), and the color-correct shader.
  It adds no pixel readback, GPU fence wait, or GPU-to-CPU synchronization.
- `postEffectUtilsF.glsl:782` samples measured history for the optical stack.
  `RenderCineLensFlareSmoothing` is Boolean, default TRUE
  (`indra/newview/app_settings/settings_alchemy.xml:4380`). FALSE uses the
  original live-probe/modeled-tail path and resets history. Shader/allocation
  failure also uses the established path. The optional history program is
  registered at `indra/newview/llviewershadermgr.cpp:5550`.
- The source audit confirms the history probe and soft-knee code exactly match
  the established shader (`.tmp/media_live2/audit.py`). No standalone shader
  compile gate was found in the repository/tool scripts; shader compilation and
  GPU presentation remain unverified without the prohibited viewer build/run.

## Off-state cueing and private media preference

`llprismlens.cpp:2749` reports **Turn on Virtual Cam to play video** for Browse
with the master off. The picker callback no longer rejects solely because the
master is off. A syntax-valid local pick is committed as a cue, without
preflight/I/O/player creation (`:2778`). Reconciliation preserves that off-state
picker ticket (`:3107`). Play while off reports the same exact status (`:2667`).
The Source status and cache readout also explain the off state
(`alpaneldirectorvcam.cpp:968`, `:1040`). Enable alone does not start playback;
explicit Play performs preflight and admission.

Private-file transport/reconciliation no longer consults AudioStreamingMedia.
`indra/newview/llviewermedia.cpp:874`, `:3152`, and `:4827` exempt explicit private
playback from that preference's unloading, slot-permission, and class-visibility
checks. Borrowed MEDIA_FACE continues through ordinary preference/policy checks.
Global volume/mute, media-slot/master switches, budgets and admission still apply.

## Monitor text and camera-image diagnosis

The old label was provably wrong: HEAD's
`indra/newview/alvcammonitorstrip.cpp:345` unconditionally selected
**Shows: camera output (raw)** whenever no mapped display was selected, including
MEDIA slots. It did not describe the actual producer.

A separate stale-image path was present at HEAD's
`indra/newview/llprismpreview.cpp:1259`: failed preview signal construction set
OFFLINE but left the previous atlas handle/cell visible. A previously published
scene cell could therefore remain drawable. This is a concrete stale-image
mechanism; without the user's runtime trace, it is not proof that this exact
sequence produced their observed thumbnail. Source inspection found media
already excluded from camera scheduling, rather than an intentional camera
render fallback.

The fixes enforce the media signal boundary at three points:

- `llprismpreview.cpp:1261` clears the atlas handle when signal construction fails.
- `llprismlens.cpp:11619` rejects MEDIA compositing without an accepted completed
  media serial, even if other output bookkeeping were stale.
- `alpaneldirectorvcam.cpp:534` sets no-signal whenever MEDIA has no accepted
  output; `alvcammonitorstrip.cpp:360` refuses atlas sampling in that state and
  paints BLACK/WHITE (`:375`). WHITE follows the slot's bound projector choice;
  raw no-signal otherwise uses black, never a scene/camera sampler.

`alvcammonitorstrip.cpp:251` now caches only name and one short state/reason:
**Preview**, **Error: â€¦**, or **Disabled**. The status assignment is guarded by
value equality (`:259`). It is visible in compact mode and with Info off (`:409`).
Removed the FPS badge, editing/live header, timestamp overlay and Shows line.
No timestamp/FPS/publication-serial field participates in text invalidation.
The atlas still refreshes at its existing throttle; drag handle, menu and
surface-drop behavior remain. Director's volatile performance/rate/admitted
strings are also stable states (`alpaneldirectorvcam.cpp:508`, `:993`, `:1007`).
Actual refusal reasons survive; checking/downloading/waiting without accepted
pixels consistently says **Error: No source**, avoiding progress-text churn.
All text-control updates still use the existing diff-guarded writer.

## Verification

Independent adversarial review converged to **0 must-fix findings**. It caught
and corrected uninitialized history reset flags, a redundant render-target bind,
allocation's unit-0 binding restoration, remaining volatile Director strings,
and no-source/status visibility edge cases before the authorized target builds.

Built from `I:/alchemy-machinima/build-Windows-vs2026-os`, Release, using each
of the three requested `cmake --build . --target <T> --config Release -- /m /v:minimal`
commands. All builds exited 0. Ran the exes from `sharedlibs/Release`:

```text
alvcammediafilepolicy: Total Tests: 37; Passed Tests: 37
LLPrismProjector:     Total Tests: 39; Passed Tests: 39
alposepolish:         Total Tests: 23; Passed Tests: 23
VCAM_MEDIA_PROBE_FILE: SKIPPED (not set)
```

Logs: `.tmp/media_live2/tests_policy.log`, `tests_projector.log`, `tests_pose.log`.
New regressions exercise production Admission/restart decisions
(`alvcammediafilepolicy_test.cpp:956`), real plugin admission/update handlers
(`llprismprojector_test.cpp:864`), and pure smoothing/knee math (`:911`). They cover
known/unknown duration, same token/epoch/admission/gain, retained eligibility,
identity refusal, Loop off/Pause, forced rebuild hold with muted/unpublished new
pixels, stale-player rejection, fresh admission, deadline loss, and new URI reset.
EMA tests cover 24/30/60/144 FPS, the two time constants, zero dt, steady state,
and knee continuity.

| Check | Result |
| --- | --- |
| Explicit `zs_check.ps1 -Files` | 19 distinct TUs PASS; `zs_final.log`, `zs_plugin.log`, `zs_shader_consumer.log` |
| `prebuild_guard.ps1 -ChecksOnly` | All gates PASS; `prebuild_final.log`; exits before purge |
| `xui_layout_check.py --changed HEAD --json` | Zero findings, zero failed files; `layout.json` |
| `git diff --check` | PASS |

The explicit syntax check includes every changed C++ file from rounds 1 and 2,
plus direct header consumers: `alkitcard.cpp`, `aluiperfselftest.cpp`,
`alvcammediafilepolicy.cpp`, `llviewerdisplay.cpp`, `llviewermedia.cpp`,
`alpaneldirectorvcam.cpp`, `llprismlens.cpp`, both media/projector test TUs, and
`llglslshader.cpp`. These cover every touched header, including `alflareenergy.h`,
`pipeline.h`, and `llshadermgr.h` (inventory: `header_consumers.py`). The plugin TU
uses the round-1 exact vcxproj copy solely for the syntax script's nested-project
mapping. TEMP/TMP and the supported syntax cache option point inside the writable
workspace; gate rules/flags/assertions were not changed.

The page-source evidence index was regenerated to update 16 pipeline line
references; row counts and gate pins are unchanged. Shader runtime, actual
file decoding/audio, external recording continuity at EOF, and native visual
layout still require the next viewer/plugin build and in-world test. No live
PASS is claimed. An authored brightness discontinuity in the video itself also
remains source content; retaining missing-frame pixels cannot remove it.
