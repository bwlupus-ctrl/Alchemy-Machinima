# Ghost Studio / entity clone — top-down performance audit brief

**This is a READ-ONLY audit.** Do not edit code, build, commit or git add. Write only your own report file.

## Symptom
The user adds ONE entity clone (Ghost Studio) and cams around it. The frame hitches every few seconds. Removing the clone stops it.

The build has Tracy enabled. A capture taken while hitching covered 20 s and 305 frames, with mean frame time 66 ms:
- Trace: `C:\Users\xianw\Desktop\Tracy\captures\alchemy_20261005_103942.tracy`, with a CSV next to it.
- The worst frame was 399 ms. Its chain was `render_ui_2d → LLFloater::draw → LLTextBase::appendText`, with 352 ms inside a single appendText call.
- `LLFloater::draw` appended text 4,832 times over the capture, about 16 per frame. `setText` goes through appendText.
- UI-2D averaged 35 ms per frame.
- Other notable costs: "rmmu - cine budget" at 12.6 ms mean, and reflection probe updates.

The first suspect is `ALPanelGhostStudio::draw()` (`indra/newview/alpanelghoststudio.cpp:834`). Every frame it calls:
- refreshSourceCombo, refreshList, refreshLookTargetCombo, refreshAnimationLibrary;
- the selection mirror;
- refreshDetail (dozens of unconditional setText/setValue calls);
- refreshRigDiagnostics (on a 0.5 s timer), refreshStatus, refreshCrowdPlacement.

Ghost Studio has TWO panel hosts; one of them is in the Director console.

## Scope
Audit top-down, the way the Lightbox was redone.

1. **Ghost Studio UI.** Cover every panel host (Ghost Studio floater, Director console tab, any others). For each refresher, report what runs per frame, what allocates, and what rebuilds lists or combos or sets large text. Identify the exact ~350 ms spike and why it recurs every few seconds.
2. **Non-UI per-frame work that exists only while a clone exists.** Include:
   - AutoAnimate (alghostautoanimmodel);
   - nameplates (alghostnameplates);
   - the spawn engine and material resolver;
   - the attachment enumerator and animation asset index;
   - clone animation sync and true-mirror;
   - pose polish;
   - idle callbacks, signals and timers.

   Also check whether any of these trigger the UI rebuilds above.
3. **Other always-open Alchemy panels with the same pattern:** draw() doing unconditional refresh work (Director console, Cine Light Rig, Lightbox, Vcam/Prism, Ultimate Diopter). A short list with file:line is enough. The user wants a PERMANENT, systemic fix, not one field patched.

## Deliverable
- Ranked root causes, with file:line evidence.
- A fix design:
  - shared helpers or a pattern (diff-guarded writes, revision-driven rebuilds, throttled live readouts, no work when hidden);
  - exactly what changes in which files, plus a rule that future panels follow automatically where possible.
- Risks, and how to verify, including which Tracy zones to add if evidence is missing.
