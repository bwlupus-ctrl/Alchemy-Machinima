# Director kit WP4 status

Implementation status: **complete, awaiting orchestrator-only compile/gate execution**.

- Completed the four-way `DirectorV2SelectedLight` link: Rig map, Rig Lights selector, Fixture role (two-way committed `mirror_setting`), and Rim per-light selector.
- Kept the existing who adapter and the page-key slot context used by history Steps; removed the dead `cine_rig_slot` context registration. A same-slot anchor commit now returns before the history barrier, while a real slot change retains the barrier.
- Easy mode has one guarded writer: explicit v1/v2 user commits write `CineLightRigEasyMode` only when its `LLCachedControl` differs. Search reveal uses a session-only display override, cleared by a user mode click, preference change, or slot change; it writes neither settings nor history. Undo/redo replays the recorded user preference write under history suspension.
- Restored the pre-WP4 v1 entry contract from `1af3670dc8b`: passive entry calls `normalizeSelectedForEasy(false)` (including Key EV fold/zero and Key-on) under history suspension, while explicit v1/v2 entry calls `normalizeSelectedForEasy(true)`. A non-Easy-ready look may display Advanced without clearing the saved preference, and v1 retains its deferred 10 Hz Easy-control refresh.
- Confirmed the constructor-time `rig_rim` provider registration, one-gesture preset application, card-header preset reparenting, active-option card reset, and dynamic visibility probe paths.
- Added an anchored page-reset confirmation popover while retaining one history gesture for the accepted reset.
- Persisted selector-card More state per option value, routed the Rim avatar-tag edge through `ALKitRow::setControlVisible`, removed the kit binding include from the v1 panel header, and reject no-op history entries before recording.
- Removed the expired `cine_rimq_status` and `cine_rimq_enable_tag` `pending_cpp_targets`; both are owned and wired by `ALPanelRigRim` -> `ALRigRimController`.
- Generator page config and generated XUI/index files were not changed. The only tooling-policy change was deleting those two stale WP4 pending records from `scripts/code_tools/director_kit/gate_policy.json`.
- No build, `/Zs`, draw lint, Python gate, or commit was performed in this fix round; those checks remain orchestrator-owned.

See `doc/director_kit/WP4_TRACES.md` for the six required reviewer-walkable flows and code paths.
