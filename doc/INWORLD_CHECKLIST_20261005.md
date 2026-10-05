# In-world checklist — builds of 2026-10-05

Current exe: `Release\AlchemyTest.exe` (built 4:35 PM). It contains everything below.

Backups:
- `AlchemyTest.pre-dirfix.exe`: before the Director fix.
- `AlchemyTest.pre-uiperf.exe`: before the UI performance batch.
- `AlchemyTest.pre-throttlerot.exe`: before the throttle rotation change.

## A. Clone hitch + UI performance (commits 30af60abc28, bd6ef320f69, d591170ed68)
1. **Clone hitch.** Add an entity clone, keep Ghost Studio open with it selected, and cam around it.
   - Expect no hitch every few seconds.
   - Tell Claude when you're ready, so it can take a Tracy capture to compare with the 10:39 AM baseline (worst frame 399 ms).
2. **Rig diagnostics.** The box shows structure only and re-samples every 2 s while visible. Press **Refresh** for the live per-face state.
3. **Two Ghost Studio copies.** Open the standalone Ghost Studio and the Director's Ghosts tab together. Rename or restyle a clone in one; the other follows within about 1 s.
4. **First open.** The first time you open Ghost Studio in a session there may be a one-time brief hitch while the animation library scans.
5. **Chat.** Select text in chat history, then receive a multi-line message. The selection stays, and chat still follows new messages.

## B. Lightbox (commits 9f31a1f2058, 299fef32c10)
6. Every tab scrolls when its content is taller than the window. Proj Shafts is no longer cut off.
7. The wheel over a slider adjusts it. For about 0.5 s after you scroll the panel, the wheel keeps scrolling instead of adjusting sliders.

## C. Director console (commits f1ec6ecc313 … d90aec8ea16)
8. **Props tab** has a scrollbar, and the Shuttle row and status line are reachable. Its content is about 15 px narrower.
9. **Takes and Time tabs** show a scrollbar only when content is taller than the tab. Time is about 15 px narrower.
10. **Ghosts tab** shows the full-height Ghost Studio with an outer scroll. The inner Pose and Crowd areas are about 12 px longer. The standalone Ghost Studio is unchanged.
11. **Focus reveal.** Tab-key through Path, Lights, Weather, Volumes, Camera and the Ghost sub-tabs; the focused control scrolls into view.
12. **Wheel.** Sliders accept the wheel, with the same 0.5 s scroll-through rule as the Lightbox.
13. **Move tab** may show a scrollbar at the default height.
14. **Camera cards.** The Cinematic card is about 117 px shorter and Shot Builders about 2 px shorter. Cards are repacked with no overlap.
15. **Two Path editors.** Open the Director's Path tab and the Actor Mover's Path tab, each on a different actor.
    - The highlight and editing follow the window you last clicked or revealed.
    - Hiding one keeps the other's preview and edit session.
    - Changing the selection in the window you are NOT using doesn't move the highlight until you click into it.
16. **Path tool handoff.**
    - Arm Walk Here in one window, then click the other window: the walk must not fire for the other actor.
    - Alt-tab away mid-edit and come back: the tool still works, and Esc always exits it.
    - Minimise and restore a window: it reclaims.
17. **Tab restore.** The Director reopens on the same tab, remembered by name. The old setting migrates silently.
18. **Lights tab and standalone panels** (Actor Mover, Prop Mover, Cine Light Rig and others) look unchanged.
19. **Actor FX.** While you're dragging or typing in a control, outside changes don't overwrite it; it catches up after you let go.
20. **Actor Gaze** follows its source window and reopens without a stale selection. If the source window was closed, it opens empty.

## D. Pad (earlier builds)
21. **Rumble:** hello on connect, two pulses for fly cam on, one soft pulse for off, a tick on D-pad Down. The Joystick window has a toggle and Strength.
22. **Throttle mode:** LT/RT now also scale pan and tilt.
