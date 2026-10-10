# Director kit WP4 status

Implementation status: **complete, awaiting orchestrator-only compile/gate execution**.

- Completed the four-way `DirectorV2SelectedLight` link: Rig map, Rig Lights selector, Fixture role (two-way committed `mirror_setting`), and Rim per-light selector.
- Kept the existing who adapter and slot history barrier, and registered the named `cine_rig_slot` context alongside the page context used by history Steps.
- Refactored Easy mode through an event-driven writer plus a read-only state synchronizer. `CineLightRigEasyMode` is cached with `LLCachedControl`; setting-signal/draw/reveal reconciliation never writes it, while mode-adapter commits write only on an actual cached-value change. User proxy changes remain recordable; passive/search/restore reconciliation is suspended and publishes effective mode without explicit-entry snapping.
- Confirmed the constructor-time `rig_rim` provider registration, one-gesture preset application, card-header preset reparenting, active-option card reset, and dynamic visibility probe paths.
- Added confirmation and handle safety to page reset while retaining one history gesture for the accepted reset.
- Removed the expired `cine_rimq_status` and `cine_rimq_enable_tag` `pending_cpp_targets`; both are owned and wired by `ALPanelRigRim` -> `ALRigRimController`.
- Generator page config and generated XUI/index files were not changed. The only tooling-policy change was deleting those two stale WP4 pending records from `scripts/code_tools/director_kit/gate_policy.json`.
- No build, `/Zs`, or commit was performed. `ui_draw_lint.py --changed HEAD --json` now reports `new_errors: 0`; the three Easy-mode findings at the former 2110/2115/2116 sites are gone. Required orchestrator checks remain `director_parity.py --groups light_group` and `kit_lint.py --all` (plus its stated compile/lint harnesses).

See `doc/director_kit/WP4_TRACES.md` for the six required reviewer-walkable flows and code paths.
