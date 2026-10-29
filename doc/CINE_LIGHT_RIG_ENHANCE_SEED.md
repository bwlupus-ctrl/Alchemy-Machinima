# Cine Light Rig Enhancements — design seed

**Status:** seed for a deep design pass. No source modified. Captured 2026-08-16.
Feeds `doc/CINE_LIGHT_RIG_ENHANCE_DESIGN.md` -> Codex implement -> Opus review -> build.

The Cinematic Light Rig (base + scale-aware master presets + mirror fix) is BUILT and confirmed
working in-world. Read `doc/CINEMATIC_LIGHT_RIG_DEEP_DESIGN.md`,
`doc/CINE_LIGHT_RIG_MASTER_PRESETS_DESIGN.md`, and `doc/CINE_LIGHT_RIG_STATUS.md` for the current
architecture and its finals (do not re-litigate them).

## What the user asked for (2026-08-16), with decisions fixed

One delivery (ship-whole), **gizmo drag-editing explicitly deferred to a later feature**:

1. **Master Colour Temperature control — RELATIVE shift** (user chose). One global control that
   warms/cools ALL lights together while PRESERVING each light's own gel choice (a warm-key/cool-rim
   contrast setup stays contrasty, just shifted) — a global white-balance trim, NOT an absolute
   per-light override (the 6 Kelvin presets already do absolute).
2. **Per-light gobos.** Each of KEY/FILL/RIM/BG gets its own gobo (projector cookie) texture, instead
   of the single global `CineLightRigCookieUUID`.
3. **Bundle a curated gobo texture set** (user chose) — grayscale gobos shipped WITH the viewer so the
   per-light picker and the gobo presets work out of the box, no SL upload. Candidates: venetian
   blinds, window panes, prison bars, slats, grid, soft dapple/cloud, branch/foliage break-up, plus
   the existing soft-circle default.
4. **Gobo master presets / cinematic looks** — new master setups that use per-light gobos for
   shadow-shaped looks (e.g. Venetian Noir, Window Light, Prison Bars, Dappled Forest).
5. **Shadow-policy fix-it button** — a one-click control near the yellow "N rig projectors request 0
   shadow slots" warning that raises `BDMergeMaxSpotShadows` to the needed count (hard cap 6) so the
   requested shafts/shadows get slots. No pipeline edit — it only writes the setting.
6. **Radius "further away" UX.** NOT a new control — the radius spinner already allows 0.5..512 m.
   The real limits: 0.1 m stepping is tedious, and projector reach hard-caps at 20 m
   (`effective_radius * 2.2`, the llprimitive clamp), so past ~9 m the inverse-square comp stops fully
   compensating and past ~20 m orbit the subject leaves the light's reach. Improve stepping/usability
   and make the reach cap legible.

## Verified facts (read from code — cite file:line, label PROVES/INFERS in the design)

- Radius UI is already `min_val=0.5 max_val=512` (`panel_cine_light_rig.xml`, radius spinner,
  increment 0.1); model clamps `MIN_RADIUS..MAX_RADIUS = 0.5..512` (`alcinelightrigmodel.cpp:24,369`);
  projector reach `= effective_radius * 2.2` (`alcinelightrigmodel.cpp:568`), which the viewer clamps
  to 20 m (llprimitive, a DESIGN INPUT — do not change).
- Colour today is a discrete per-light **profile index** (0..23) -> RGB from the `PROFILES` table
  (`alcinelightrigmodel.cpp:50-81`), gamma-corrected, then used as the light colour. There is NO
  global colour control. A RELATIVE temp shift must therefore be an RGB warm/cool GAIN applied on top
  of each light's base RGB (preserving relative gel), NOT an index change.
- Gobo/cookie today is ONE global UUID `CineLightRigCookieUUID`, applied to every projector emitter
  via `setLightTextureID(rigCookie())` (`alcinelightrig.cpp:516`). Per-projector textures are already
  supported (each emitter can carry its own `setLightTextureID`), so per-light gobos need no shader or
  pipeline change — only a per-light texture id driven onto each emitter.
- Bundled textures ship via a `textures.xml` entry + a PNG under `skins/default/textures/...`
  (e.g. `Command_Lightbox_Icon` -> `toolbar_icons/lightbox.png`, textures.xml:153), packaged by
  `viewer_manifest.py`.
- Shadow slots: `MAX_SPOT_SHADOWS = 6` (compile-time cap, pipeline.h:1030); runtime
  `BDMergeMaxSpotShadows` default 2, clamped [2,6] (`pipeline.cpp:600`), reallocates live. The fix-it
  button writes this setting; it CANNOT exceed 6 without a shader/pipeline change (out of scope, user
  declined).
- Preset schema (design B.2) carries per-light yaw/pitch/profile_idx+name/ev/beam_idx+name/on + radius.
  A gobo field would extend it (needs a version bump decision — see below).

## What the design pass must decide

### A. Master Colour Temperature (relative shift)
- The scale/units: mireds (perceptually linear for CT) vs a normalized warm..cool [-1,+1] vs a Kelvin
  target that becomes a shift. Recommend one; mireds is the physically-correct axis for "shift all CT".
- The math: convert the shift to an RGB gain (white-balance trim) applied to each light's LINEAR RGB
  AFTER the profile lookup and BEFORE clamp — so relative gel differences survive. Define it as a pure
  function; keep it in the model (renderer-independent). Work how it composes with gamma
  (setLightSRGBColor vs Linear), the headroom re-base, and the [0,1] clamp. Default = 0 shift = today's
  colour BITWISE (fast-path, like the scale s==1 no-op).
- Persistence: a global settings key (`CineLightRigMasterTemp` or similar). Decide whether presets
  store it (probably yes, as a global alongside MasterEV) — and whether that needs the scene/preset
  version bump (both are v1 today; adding a field is the forward-compat question).
- TUT tests: shift=0 bitwise no-op; a nonzero shift warms/cools by the expected gain; relative gel
  order preserved (a cool light stays cooler than a warm light after shift); clamp/gamma composition.

### B. Per-light gobos
- Model vs controller: the gobo is a TEXTURE (render input), not geometry/exposure. Decide if it lives
  as a per-light field in `Setup`/`LightBase` (like profile/beam — a small gobo INDEX into the bundled
  library) that the controller maps to a texture id and pushes via `setLightTextureID` per emitter, OR
  a controller-only per-light setting. The index-in-Setup route lets presets carry it and keeps the
  model pure (it stores an index, never a texture).
- Settings + schema: 4 per-light gobo keys (KeyGobo/FillGobo/RimGobo/BgGobo) as indices; the preset
  schema gains a per-light `gobo_idx`+`gobo_name`. Index 0 = the default soft-circle cookie (today's
  behaviour), so an absent/զero field = current look (back-compat).
- A "custom UUID" escape: keep the global `CineLightRigCookieUUID` as one selectable library entry, or
  allow a per-light custom UUID? Decide (probably: library indices + index 0 = current cookie; a custom
  UUID is a later add).
- The omni emitters carry NULL texture (plain point light) — gobos apply to PROJECTORS only. Confirm.

### C. Bundled gobo library (the textures)
- Which gobos (a tight, useful set — recommend the count) and how they are produced. They are grayscale
  projector cookies (luminance modulates the projected light). PROPOSE producing them as procedurally
  generated grayscale PNGs (venetian = horizontal bars, window = grid/panes, prison = vertical bars,
  slats, grid, soft dapple = blurred noise blobs, branch break-up = organic noise). State the
  generation approach (a small script that writes the PNGs) so they are reproducible and reviewable, the
  file locations (`skins/default/textures/gobos/*.png`), the `textures.xml` entries, and the
  viewer_manifest packaging. Each gobo needs a stable NAME the index maps to.
- Forward-facing: a corrupt/missing gobo texture must fall back to the default cookie, never a black or
  missing projection.

### D. Gobo master presets (cinematic looks)
- Propose a small slate of gobo-driven master setups (reimagined cinematic looks): e.g. Venetian Noir
  (hard side key through blinds), Window Light (soft key through panes + ambient), Prison Bars, Dappled
  Forest (branch break-up + cool bg), Skylight Grid. For EACH: name, intent, exact per-light values
  INCLUDING per-light gobo index, radius, and how it reads on a scaled clone (inherits SA guarantees).
  These append to `cine_light_rig_presets.xml` (currently 26 entries; Classic compiled #0).

### E. Shadow-policy fix-it button
- Placement (near the shadow-policy hint text), label, and behaviour: compute the requested slot count
  (rig projectors wanting shadows/shafts, capped 6) and set `BDMergeMaxSpotShadows` to it (clamped
  [2,6]). Be honest in the UI when the request exceeds 6 (the hint already says "use Key only"). No
  pipeline edit — settings write only. Confirm the existing hint logic (`alpanelcinelightrig.cpp`
  shadow hint) so the button and the warning agree.

### F. Radius UX
- Improve the control: a coarser increment (or a second coarse step, or a slider) so pushing the light
  to 5-20 m is not 100+ clicks; and make the 20 m reach cap legible (the tooltip already mentions it —
  consider a soft visual cue when radius exceeds the useful range). No model/clamp change (512 stays;
  reach cap is a design input).

## Constraints
- Label PROVES/IMPLIES/INFERS; cite file:line. No source modified by the design.
- Ship-whole: master temp + per-light gobos + gobo textures + gobo presets + shadow fix-it + radius UX
  in ONE delivery. Gizmo drag-editing is explicitly NOT in this delivery.
- OFF-LIMITS unchanged: pipeline.cpp, all GLSL/shaders (per-light gobos use the EXISTING projector
  texture path — NO shader change), indra/llprimitive/* (the 20 m / [0,1] clamps are design inputs),
  the avatar/ghost/actormover precedents (read-only), llselectmgr, the applySceneData three-way split,
  the nine gate exemptions. The scale-aware and mirror code are finals — touch only where a new field
  legitimately threads through (e.g. a gobo index in Setup, a master-temp global).
- Forward-facing robustness: every new texture, index, and setting must degrade gracefully (missing
  gobo -> default cookie; out-of-range index -> clamp; shift NaN -> 0).
- Concrete enough to become a Codex brief: named files, functions, settings keys, the gobo texture
  list + generation approach, the preset/settings schema additions, and an OFF-LIMITS list. Decide and
  STATE any scene/preset version bump.
