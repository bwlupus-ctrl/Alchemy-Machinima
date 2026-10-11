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

