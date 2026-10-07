# Handoff: night of 2026-10-06

## What's in the latest build (started ~10:50 PM)

**Weighted Classic flycam (new third flight model).** Your Classic flycam, unchanged, with weight added on top: momentum on movement and rotation, and it coasts to a stop when you let go.
- **Where:** Joystick window → **Flight model** tab → Weighted section. You get a preset menu (Light / Steadicam / Heavy / Crane), a Weight % slider (0 % = exactly Classic) and Reset Weighted.
- **Selecting it:** Flight model menu: Classic / Weighted / Drone. Also available in the Director flycam panel.
- **D-pad Up** toggles between the two models chosen in **Xbox Pad tab → D-pad Flight model toggles A ↔ B** (default Classic ↔ Weighted).
- **Reset Drone** now resets only the Drone tuning; it no longer switches your flight model.
- **Tests:** Weighted 50/50, Classic 4/4 (unchanged from before), Drone 49/49.

**Also from tonight (built at 10:31 PM):**
- **Joystick window:** rebuilt into tabs: General / Buttons / Xbox Pad / Drone (now "Flight model"). Nothing overlaps, and every tab scrolls with the mouse wheel.
- **Drone:** "Invert look up/down" checkbox.
- **Set Hover Height wheel:** 0.025 m per notch. Shift = 0.001, Ctrl = 0.25.
- **Right-click Force Mask:** now converts the whole avatar, including faces with slight transparency, glow, worn animesh and nested parts.
- **UI mandate:** the layout checker blocks the build if any changed UI file has overlaps, clipping or a missing scroll area. The viewer also logs `UIFit` warnings when a window opens with content that doesn't fit.

## Backups
- `AlchemyTest.pre-weighted.exe`: the build from just before Weighted Classic.
- `AlchemyTest.pre-uimandate.exe`: the build from before the joystick rebuild.

## Open / next
- **Feel test for Weighted Classic.** If the presets feel off, they can be tuned without a rebuild through hidden debug settings: `FlycamWeightedTauMax` (1.0), `FlycamWeightedCurve` (20), `FlycamWeightedRotationShare` (0.6).
- **Shader cache.** The grey hair fixed itself after a full shader recompile, so a stale cached shader was the likely cause. The Actor FX shaders also recompile on every login, which is why logins are slow. Fix that next.
- **Sidecar side effect.** Turning the ReShade sidecar (`RenderVisibleDiffuseSidecar`) off makes some alpha surfaces transparent. That's a separate bug.
- **Lightbox and Director panels.** The layout checker flags real overlaps and clipping there (Lightbox about 281 findings). Triage those next.
- **Nothing pushed to GitHub.**
