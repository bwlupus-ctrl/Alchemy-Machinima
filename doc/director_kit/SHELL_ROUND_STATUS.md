# Director Shell + Leftovers Round Status

Completed on 2026-10-10 on `feature/director-kit-p0` at the working-tree HEAD. No commit was created and no build was run.

## Implemented

- Ghost Studio is an unwrapped Director page in both shells. Its four named scrollers now use `ALScrollFit::Controller`, its tabs follow the available page height, and the standalone floater remains supported. The Filters and Props legacy hosts were inspected; neither has the same nested-scroll ownership problem.
- Exposure keeps undo/redo at the page top while its tabs consume the remaining fitted height.
- Roster hint clipping/layout, transport spacing, Cast reachability, and kit chrome sizing were corrected for the audited shell sizes.
- Generated Bloom & Glow and Light Actors layout fixes live in the generator inputs/source XUI. Kit pages and page-source reports were regenerated rather than hand-edited.
- Switcher slot labels elide to the measured button width while retaining the full label and mode in the tooltip.
- The style-combo caret probe ignores controls outside containing scroll viewports and the floater viewport.
- Path Editor accordion changes now recompute the scrolled document height, and the hidden suspend banner no longer reserves its 52 px gap.
- Prism and Ultimate Diopter received the same required named scroll-fit/focus wiring because their already-changed XUI was included in the zero-finding gate scope.

## Verification

`prebuild_guard.ps1 -ChecksOnly` passed all nine prebuild gates against `7c94b7f1a80`: changed-XUI layout, UI draw selftest/changed lint, `/Zs` compile (18/18 translation units), light-map maths, generator check, kit lint, light parity, XUI checker selftest, and Director page-source check. The changed-XUI gate reports zero findings, including `panel_ghost_studio.xml`.

Per the round constraint, the executable build and the runtime all-pages self-test (including the new TEXT-ON-TEXT check) were not run; those remain the orchestrator's next steps.
