# Ghost Studio — Smart Lock Modes + Per-clone Effect FPS (Codex brief)

Two features in I:\alchemy-machinima. Adversarial design first, then implement as one
`-Wswitch`-clean change. Client-only; Director Console superset for every new control;
per-instance + persisted. Render/placement correctness is IN-WORLD only. ⚠️ After building,
the human copies changed shaders + skins + settings to build/Release (incremental build
copies neither).

## Part 1 — SMART LOCK MODES (replace the "Lock as unit" boolean with a mode + fix the vanish)
User wants "all the above, give an option, make it smart." Today `mLockAsUnit` is a bool
(alghoststudio.h; UI `lock_as_unit_check` panel_ghost_studio.xml:343; read alpanelghoststudio.cpp:1792).
Locked groups (`mGroupId`) transform via `transformUnit` (alghoststudio.cpp:1455).

Replace the bool with a **lock MODE** (enum + combo), selectable at spawn AND changeable on an
existing group:
- **OFF** — ungrouped (N independent instances).
- **RIGID UNIT** — position + rotation + scale locked; the group reads as ONE row in the
  Instances list (`ghost_list`), expandable to reveal members; you direct it as a single object;
  scaling scales the whole unit UNIFORMLY.
- **MOVE + FACE** (loose) — position + rotation locked (march/face together) but SCALE is
  PER-CLONE (scaling one scales only that one); stays N rows, visually marked as grouped.

**FIX THE VANISH (do this regardless of mode):** `transformUnit:1479` scales the formation
offsets by `scale_ratio`, so shrinking a locked unit collapses every member onto the anchor AND
to min scale -> they stack into a dot and read as "gone." For RIGID scale offsets but FLOOR the
collapse (clamp so the unit never degenerates to a point / sub-min); for MOVE+FACE do NOT scale
offsets at all (scale only individual clone scale). Keep the finite/NaN guards.

**Route ALL transform mutations through the mode** (not just the four already-hardened
onClickToActor/ToMe/Drop/AlignFeet): also **onClickUpright** and **onClickPasteTransform**
(alpanelghoststudio.cpp ~1537/1563 — flagged by review as still per-instance) and the in-world
manipulators, so no action can split a RIGID unit. Keep the `std::set<mGroupId>` dedup pattern.

**"Smart":** sensible default mode; selecting a single member of a RIGID unit and scaling still
scales the unit (or offer a clearly-labelled "edit member" affordance); MOVE+FACE lets per-member
scale; never vanish; list reflects the mode (1 row vs N). Preserve `groupMembers`/`lockGroup`/
`ungroup`; migrate the old bool cleanly.

## Part 2 — PER-CLONE EFFECT FPS (Style tab)
User: "the Per clone FPS for the effects in style, too." Add a per-clone **"Effect fps"** control
(spinner, 0 = smooth/every-frame, 1..30) in the Style/Look section, SEPARATE from the existing
"Pose fps" (`pose_rate_spinner`, which throttles the POSE/geometry). Effect fps stop-motions the
SHADER-ANIMATED looks + distortions (scanlines, flicker/shimmer, VHS roll, glitch, dissolve, wave,
sonar, hologram interference, etc.) by QUANTIZING the effect time each clone feeds the shader.
- Implementation: the shader animates off `ghostTime` (uploaded per clone at llactormover.cpp
  apply_program ~3913: `sh->uniform1f(sGhostTime, ghost_now)`). Quantize before upload per clone:
  `t = fps > 0 ? floor(real_t * fps) / fps : real_t`. Add `mEffectFps` (alghoststudio.h) +
  `GhostDrawParams` field + setter; drive the quantized time into the existing sGhostTime upload.
  Do NOT add a new shader uniform if quantizing the fed time is enough (it is).
- Per-instance + persisted (like Pose fps); Style panel control + reset button; mirror to Director
  + Path panels.

## Anchors
- alghoststudio.h/.cpp: `mLockAsUnit`, `mGroupId`, `transformUnit` (:1455), `lockGroup`/`ungroup`/
  `groupMembers` (:1433-1452), `mAnimSpeed`/`mPoseRateHz`, spawn with mLockAsUnit (~2053/2087).
- alpanelghoststudio.cpp: `lock_as_unit_check` bind (:203) + read (:1792); onClickUpright (~1537),
  onClickPasteTransform (~1563); pose_rate (:217), anim_speed (:139); the ghost_list scroll list.
- panel_ghost_studio.xml (Crowd tab lock control + Instances list; Style tab for Effect fps),
  floater_director.xml, panel_path_editor.xml (superset), settings.xml.
- llactormover.cpp apply_program sGhostTime upload (~3913) for Effect fps.

## Acceptance
Lock mode is selectable (Off/Rigid/Move+Face) at spawn and changeable after; RIGID shows one list
row + scales as a unit + can't be split by ANY action + never vanishes; MOVE+FACE keeps per-clone
scale + N rows + rigid position/rotation; Effect fps visibly stop-motions a clone's looks/distortions
at the chosen rate and is smooth at 0; both features per-instance + persisted + mirrored to Director;
builds clean.
