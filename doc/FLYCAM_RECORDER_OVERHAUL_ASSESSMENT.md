# Flycam Recorder Overhaul — Assessment & Implementation Verdict

Annotation layer over `FLYCAM_RECORDER_FULL_OVERHAUL_SOURCE_AUDIT_AND_CODE.md` (the
ChatGPT deep-research paper). Vetted adversarially by Codex against the ACTUAL current
code (`llflycamrecorder.{h,cpp}`, `llviewercamera.h`, `llappviewer.cpp` idle dispatch)
on 2026-07-26. Purpose: separate what to build from what to skip, and define a
de-risked first landing.

## Verdict

The paper is **strong and code-accurate** — better than a typical LLM dump. Its audit
is real, its phasing is sound, and its proposed APIs almost all fit the actual
codebase. Treat it as a **9-phase menu, not one project**; only a slice is worth doing.

- **20/20 findings verified** — 18 CONFIRMED, 2 PARTIALLY-TRUE (the paper overstates):
  - #6 recording density: gaps are not *entirely* unmarked (nonuniform `mTime` preserves
    durations); there's just no explicit dropped-sample metadata.
  - #11 anchor loss: there IS an implicit *hold-last* policy today (failed resolve keeps
    the previous latch, `llflycamrecorder.cpp:445-458`) — it's just not selectable.
- **The crux is real (#1/#2/#20):** "relative playback" subtracts one fixed record
  anchor (`:363-376`, `:476-501`), so the recorded subject's motion stays baked into the
  world keys. It is NOT true per-sample avatar-relative recording. This is the headline
  feature gap.
- **APIs the paper assumes mostly exist:** `LLQuaternion::isFinite/getAngleAxis/normalize/
  DEFAULT`, `LLVOAvatar::getJoint/getPelvisToFoot`, `findObject()`+`asAvatar()` — all real.
- **One invented API:** `replaceFileAtomically()` does not exist — the real primitive is
  `LLFile::rename()` (`llfile.cpp:894-899`). True atomic replace-on-Windows needs a
  tested helper → defer.
- **Over-engineered for this fork:** the full class explosion (take/anchor/evaluator/io/
  edit-model/timeline/tool split), the camera coordinator, 2M-key / 64 MiB caps, and the
  whole editor + interchange product layer. Real, but not correctness prerequisites.

## Slice 1 — Hardening (do first; no file-format change, low risk)

Ships correctness/safety wins on the existing v2 take, independently. Codex-vetted seams:

1. **Stop FOV broadcast** — `setView()` → `setViewNoBroadcast()` at
   `llflycamrecorder.cpp:644-650` (`llviewercamera.cpp:830`). Real sim side-effect, one line.
2. **Transactional v2 load** — validate finite/nonneg time, finite pos, finite non-zero
   quat, bounded FOV, realistic key-count/file-size caps BEFORE the swap (`:721-770`);
   never clear the current take until the new one fully parses.
3. **F64 internal time** — `Keyframe::mTime`, playhead, duration, seek/sample/eval args
   (`llflycamrecorder.h:65-70,97,105-115,135`). LLSD already stores `Real`, so the v2
   shape is unchanged.
4. **Binary segment lookup** — replace the O(N) scan (`:520-526`) with `lower_bound`
   (no monotonic cache yet — seek/reverse complicate invalidation).
5. **Stable selection latch** — capture one object/avatar UUID at `startPlayback()`
   (`:215-235`); resolve THAT during follow instead of `getPrimaryObject()` every frame
   (`:395-425`); on loss, hold + show status — **no silent fall back to self** (`:426-430`).
6. **Complete operator resets** — extend the existing start-reset (`:230-234`) to seek,
   repeat, ping-pong reversal, stop, and anchor rebind (`:259-297`, `:575-592`).
7. **Save flush-check** — verify serialize/flush success on the existing direct save
   (`:688-699`). (Full atomic temp-file+rename deferred — see invented-API note.)

## Slice 2 — True avatar-relative recording (the real feature)

Needs the v3 take model. Store camera pose **in the anchor frame each sample**
(`camera_local(t) = inverse(anchor_world(t)) × camera_world(t)`), an explicit stable
anchor binding (avatar/object/Ghost-Studio instance, no silent fallback), and playback
placement modes (original-world / at-play-start / live-follow / recorded-track), plus
bake-to-world. Optional recorded-anchor track to reconstruct/convert. This is the
"record the camera's relationship to a performer" win. Separate landing from Slice 1.

## Slice 3 — Clock modes / Temporal Capture synergy (optional, after F64)

Ties into the `LLPresentationTime` clock already built. Codex guidance:
- Do NOT reuse `sGlobalTimeFactor` for the camera (that's skeletal timing).
- **Absolute sampling, not scaled dt:** snapshot `LLPresentationTime::currentFrame()`
  and derive take time from `presentation_time`; do NOT multiply by `effective_scale`
  again. Read F64 from the frozen context (the `presentationDelta()` accessor narrows to
  F32).
- Keep **Wall** and **Presentation** as per-take choices; Presentation-mode maps frozen
  `presentation_time` to a take-start epoch so 0× holds naturally.
- Do NOT globally force recorder playback under `TemporalDriveCamera` (still disabled,
  `panel_temporal_capture.xml:240-250`); enable it only with an explicit per-take
  Presentation choice. Director / fixed-frame clocks aren't implemented — defer.
- Preserve the existing camera-dispatch priority (`llappviewer.cpp:5463-5471`); clock
  choice changes how `mPlayhead` is evaluated, not who owns the camera.

## Deferred (real, but not now)

Full subsystem split · timeline/dope-sheet · in-world path/frustum editor · undo/redo ·
named take library · Director shot timeline · camera coordinator · interchange
(Blender/.chan/glTF/AE/FBX) · true atomic save · 2M-key caps.

## Recommended order

Slice 1 (safe, ships value now) → Slice 2 (the feature) → Slice 3 (Temporal synergy).
Each goes through the same design→code adversarial-Codex loop used for Temporal Capture.
