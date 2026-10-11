# User-reported bugs (in-world testing)

## 10/10/2026, 9:13 PM build (d812416ca65), Director v2 on

1. **The button colour state "sticks" after clicking.** After clicking, a button/switch can keep the pressed/selected colours.
   - Screenshot: mockup_ref/user_bug_bounce_card_20261010.png (the Rig "Bounce" card). The **Bounce** switch is drawn in an in-between state: the knob in the middle, the off-grey knob over the amber "on" outline, both colour schemes at once, while the value reads "Off".
   - The **Actor only** switch shows a smeared/textured fill.
   - Suspects: the kit switch / LLButton state tints added for v2 (image_color_selected/hover/pressed and the kit restyle): the hover/pressed tint isn't reset on mouse-up/leave, or the switch's animated knob position and colour state get out of sync with the value.
2. **Ratio field is empty** in the same Bounce card: a spinner with no visible value (the same class as the EV empty box on the Lights card).
3. **The Eye catchlight doesn't work** (user report; feature-level. Check the binding/setting path of the Rig catchlight row in v2 vs v1, and whether the engine applies it).

The user is using the client: log only, don't act until the next session.

4. **Lights card Key / Fill / Back / BG tabs don't switch the panel** (screenshot mockup_ref/user_bug_lights_tabs_20261010.png): clicking Back leaves Key highlighted and the content unchanged (Key's values shown); Back only gets a white focus outline (likely the same sticky-state bug as #1, or the option tab's click isn't routed to ALKitCard option selection / DirectorV2SelectedLight). Core function broken - HIGH priority.
5. Same screenshot: EV spinner still empty (see #2); the Pitch arc dial draws only half an arc with its -85/85 labels crowding the dial edge; the arc dial does not match the mockup's half-dial.

6. **Rim page: choosing a preset does nothing** (v2). Selecting a Rig Rim preset from the Rim page preset menu doesn't apply it. Check the rig_rim preset provider wiring (WP4: ALRigRimController / applyRigRimPresetByName, the kit preset strip commit -> provider apply), and whether it's the same click/commit routing failure as #4. HIGH priority.

7. **Rim on/off toggle does nothing** (v2): the Rim page master switch (CineRigRimEnabled, page-master home per WP3) doesn't turn rim lighting on or off. Same suspected root cause as #1/#4/#6 (v2 control commit / binding not reaching the setting). HIGH priority.

8. **The VCam page redesign was NEVER BUILT** (user 10/10). v2 Camera › VCam is still the legacy launcher card + the old Virtual Cam Manager window (mockup_ref/user_vcam_current_20261010.webp). The approved design (mockup_ref/vcam_mockup.png; doc/VCAM_MONITORS_DESIGN.md, work packages VC1-VC7) is NOT implemented: monitors strip with 1 fps previews, ONE master Previews toggle + shared speed, Large/Compact, drag-to-surface, per-camera chips (Cam 1..8, + Add camera, Remove), the signal-chain steps Source › Lens › Look › Output › Rate & performance, and the status line. The user expected it. Schedule it explicitly with the user (it was planned after the kit foundation).

9. **CRITICAL: Weather can't be turned on; user: 'This entire UI has the look but nothing actually works.'** (v2, 9:13 PM build). Legacy pages hosted in v2 (e.g. World › Weather, panel_weather_settings.xml) appear not to be wired to their C++ owners (the v2 lazy build may instantiate legacy panels without the v1 binders/postBuild wiring), and kit pages don't commit (#4/#6/#7). Unknown yet: whether v1 (DirectorUIV2 off) is still fine. The 10/10 Haiku sweep changed only geometry (names/bindings verified identical) but MUST be confirmed functionally. ROOT PROCESS GAP: no functional test. NEXT SESSION FIRST: build a functional self-test (for every page in v1 and v2: toggle each checkbox/switch, pick a combo item, nudge each slider/spinner; assert the bound setting changed or the commit callback fired, then restore) and make it a build gate before any further styling.


KNOWN-GOOD (user 10/10, 9:13 PM build): the standalone Cine Light Rig FLOATER works. So the v1 Light Rig panel/engine are fine, and the failures are in how v2 hosts/binds controls (kit pages + legacy pages inside v2). Use the standalone floater as the reference when diagnosing.

10. **Weather rain: switched on but NO visible effect** (9:13 PM build). This is likely RENDER-side, not UI. PRIME SUSPECT: the 10/10 custom water level change (02422bd48fd + minors: LLViewerRegion/LLSurface water-height plumbing, getWaterHeight no-water-object guard, once-per-frame water updates). The weather/rain code may read the water height (underwater camera suppression, rain floor/splash level) and now get a different value even with the override OFF. Compare with AlchemyTest.pre-kitp0.exe (4:02 PM build, which already had the water feature) and the pre-invstress build (no water feature). Also check whether it happens in v1 (DirectorUIV2 off). Asked the user: v1 or v2? Custom water level off?

   - USER: rain is broken in BOTH directors (v1 and v2), so it's render-side and not the Director UI. The custom water level change is the prime suspect.

11. **Projector volumetrics won't show after light masking is turned OFF** (9:13 PM build). Earlier this week a patch stopped volumetric projectors from showing when light masking (Actor Mask / actor-only lighting) was on. Now, with masking off, the projector volumetrics still don't render. Suspect: that gate keys on a stale/sticky state or a different setting than the user's masking toggle, or never re-enables. Read-only diag: doc/PROJECTOR_VOLUMETRIC_DIAG.md.

   - RESOLVED (user): masking was still on; not a bug. UX FOLLOW-UP: make hidden suppressing states visible where the affected controls are, e.g. show 'Light masking is ON: projector volumetrics hidden' on the projector/volumetric controls, and 'Weather master is OFF' next to Rain (see #10). Both cases tonight were an invisible upstream switch.

