# Director kit: user decisions, 10/10/2026 (morning)

These come from the R3 review's questions. They are binding for the rollout spec and the page contracts.

1. **Diopter page:** the everyday controls go on the page: on/off, preset, strength, focus split. An "Open full editor" button opens the existing Diopter editor window for everything else. Don't recreate the 9-tab editor in the Director.
2. **Capture page:** **DROP it.** Remove Capture from the Director v2 page list. The rollout's page count becomes 35, and all gates, matrices and parity counts must match. Snapshot stays in its existing window.
3. **Light Cues and Gaze Cues pages:** the cue list lives in the Director, with add, remove and play. Detailed keyframe editing stays in the existing cue windows, opened by a button.
4. **Light rig Mirror with an object target:** mirror around the object's front direction, the same way it works for avatars. This is the ~6-line engine fix: derive the azimuth from the object's rotation and publish it through the facing accessor. Don't disable Mirror.
