# User-reported bugs (in-world testing)

## 10/10/2026, 9:13 PM build (d812416ca65), Director v2 on

1. **The button colour state "sticks" after clicking.** After clicking, a button/switch can keep the pressed/selected colours.
   - Screenshot: mockup_ref/user_bug_bounce_card_20261010.png (the Rig "Bounce" card). The **Bounce** switch is drawn in an in-between state: the knob in the middle, the off-grey knob over the amber "on" outline, both colour schemes at once, while the value reads "Off".
   - The **Actor only** switch shows a smeared/textured fill.
   - Suspects: the kit switch / LLButton state tints added for v2 (image_color_selected/hover/pressed and the kit restyle): the hover/pressed tint isn't reset on mouse-up/leave, or the switch's animated knob position and colour state get out of sync with the value.
2. **Ratio field is empty** in the same Bounce card: a spinner with no visible value (the same class as the EV empty box on the Lights card).
3. **The Eye catchlight doesn't work** (user report; feature-level. Check the binding/setting path of the Rig catchlight row in v2 vs v1, and whether the engine applies it).

The user is using the client: log only, don't act until the next session.
