# Director console: top-down audit brief (performance + structural "booby traps")

**This is a READ-ONLY audit.** Do not edit code, build, commit or git add. Write only your own report file.

The user cannot test in-world for a while, so evidence must come from the code, the git history and the existing Tracy capture.

## Why
Since the Director console was reconfigured, the user keeps hitting "booby traps": hidden assumptions that surface as bugs one at a time. The same pattern was just fixed elsewhere:
- **Lightbox, commit 9f31a1f2058:** hand-typed scroll content heights clipped tabs and killed the wheel. The fix was to auto-wrap and auto-fit every tab.
- **Ghost Studio, commits 30af60abc28 and bd6ef320f69:** draw() did per-frame work, including a 40-90 KB setText every 0.5 s, which caused a 345 ms hitch. The panel is hosted in TWO places (a standalone floater and the Director's "Ghosts" tab).
- **Part C, commit d591170ed68:** only the Director's read-only status/header texts were diff-guarded. The cast list, switchers and child-panel pushes were deliberately NOT changed (see `doc/UI_DRAW_PERF_PART_C_BRIEF.md`, "Dropped", for the reasons).

The rules now live in CLAUDE.md under "UI PANELS MUST NOT DO WORK IN draw()". Helpers are in `indra/newview/aluirefresh.h`. The prior audit, section 4, is in `doc/GHOST_PERF_AUDIT_FABLE.md`.

Code: `indra/newview/llfloaterdirector.cpp/.h`, `skins/default/xui/en/floater_director.xml`, plus every panel class the Director hosts:
- switchers: alpaneldirectorswitcher, alpaneldirectoranimswitcher;
- Ghost Studio, path editor, actor mover, lens gaze, flycam recorder, cine light rig/cues, prop mover;
- anything else embedded via `class=` / `filename=`;
- ALScrollFocus.

Find the reconfigure in git history (`git log -- indra/newview/llfloaterdirector.cpp indra/newview/skins/default/xui/en/floater_director.xml`) and study what changed.

Tracy evidence from 2026-10-05 10:39, taken before the fixes, with the Director open:
- `C:\Users\xianw\Desktop\Tracy\captures\alchemy_20261005_103942.tracy` and its CSV;
- per-call exports in `C:\Users\xianw\AppData\Local\Temp\ghostaudit\`.

## Scope
1. **Performance:** everything the Director does per frame, on every tab, visible or not:
   - the 9 refreshers in `LLFloaterDirector::draw`, `refreshCastList` cell re-sets, the status-strip cast walk, Move/Path selection pushes, Animate-tab snapshots, Camera-tab name lookups;
   - the switcher bank re-parse;
   - idle callbacks, signals and timers registered by the Director or its child panels;
   - work done for hidden tabs;
   - double work when a panel is also open standalone.

   Estimate the cost per item.
2. **Structural traps from the reconfigure:**
   - hand-typed heights / scroll content sizes (the Lightbox class);
   - tabs or panels without scroll containers;
   - embedded panels with fixed heights that clip;
   - shared panels with two hosts (state divergence, double refresh, focus/selection mirrors);
   - refreshers that assume visibility;
   - getChild-by-name in hot paths (dummy-widget creation);
   - stale state between tabs;
   - callbacks or signals that outlive the panel (lifetime);
   - save_rect/min sizes;
   - anything that "works only because" of an assumption.

   For each, show file:line evidence and a realistic failure scenario.
3. **Already-known items:** for each, state whether it is still present after commits d591170ed68 and bd6ef320f69.

## Deliverable
- Ranked findings: performance and traps, each with file:line, the failure or cost, and a confidence level.
- A fix plan in the house style:
  - shared helper or rule first;
  - then per-area changes;
  - mark each item **safe-mechanical**, **needs-revision/plan-review** or **leave-alone**, with reasons;
  - learn from `UI_DRAW_PERF_PART_C_BRIEF.md`'s dropped list: don't propose revisions on models that are publicly mutable or settings-backed.
- An in-world test checklist for when the user is back.
