# Run 5 in-world functional triage

## PRODUCT BUGS

- Animation Explorer left `mCurrentAnimationID`/`mCurrentObject` unchanged when its list had no selection, allowing Dance “From Explorer” to consume a stale, no-longer-selected UUID. `AnimationExplorer::onSelectAnimation` now clears both owner fields, its UUID readout, and preview on empty selection (`animationexplorer.cpp:233-243`).
- The category-A callbacks themselves are live: Path list selection writes the mover edit node (`alpanelpatheditor.cpp:1169`); Dance writes `ALDanceBall` roster/rules/timing/wave/playlist state (`alpaneldanceball.cpp:1239`, `1318`, `1330`, `1709`, `1724`, `1848`, `1907`, `1931`); Prop Drive writes `ALObjectPathMover` driving state (`alpanelpropmover.cpp:417`); gaze Reset commits `LLDirectorCast::GazeTarget` (`alpanellensgaze.cpp:532`); Ghost visibility/edit controls write `ALGhostStudio`/`ALToolGhostEdit` (`alpanelghoststudio.cpp:2862`, `4155`); Takes Stop writes recorder transport state (`alpanelflycamrecorder.cpp:224`). Their run-5 failures were harness oracle/prerequisite defects.

## HARNESS BUGS

- Path fixture selected only the panel's transient target. Director draw immediately replaced it from an empty cast selection. The fixture now adds/selects a viewer-local cast actor through `cast_list::onCommit`, calls `ALPanelPathEditor::revealFor`, creates two nodes, and establishes the per-row redo/camera/primitive/sync/copy/follow prerequisite (`aluiperfselftest.cpp:2363`). It restores the cast selection, membership, path, walk, follow and edit state (`aluiperfselftest.cpp:3210-3265`).
- `DEFER_INWORLD` rows were tagged `no_state_oracle` even when the runner had an owner oracle, and the unconditional failure at `aluiperfselftest.cpp:6227` overrode list results. Manifest enrichment now assigns explicit Path, Prop, Dance, Ghost, gaze, fog-list and recorder probes and exempts those probes from the no-oracle classifier (`functional_report.py:1161-1187`, `1290-1294`).
- Owner assertions now read the state each handler actually owns: Path edit node; `ALDanceBall` session/runtime fields and revision; Ghost show/edit state; `LLDirectorCast` look-at and gaze target; `ALObjectPathMover` roster/node/drive counts; and `LLFlycamRecorder` transport (`aluiperfselftest.cpp:5793-5923`). List rows retain their existing fog/playlist/prop/node owner assertions immediately above that block.
- Prop actions use a temporary `mIsLocalOnly` volume and the production panel callbacks (no selection). CORRECTION (Opus safety review): an earlier claim that selection traffic is suppressed was FALSE: any LLSelectMgr selection broadcasts PointAt (ViewerEffect, llhudeffectpointat.cpp:254) and AGENT_STATE_EDITING (llagent.cpp:2312-2315). `btn_enroll` and `db_from_explorer` (Animation Explorer sends ObjectSelect for animation sources, animationexplorer.cpp:540-556) are therefore excluded in-world.
- Gaze Reset previously observed `LLActorMover` although the handler writes `LLDirectorCast`; gaze fixtures now select You, enable only viewer-local gaze state, create a temporary cast target, assert the cast-owned mode/reference, and restore both owners (`aluiperfselftest.cpp:2729-2767`, `5871-5892`).
- Combo popup lists are detached from their floater by design. Pre-dispatch now accepts the explicitly tracked popup captor inside its own rect, fixing the Rig/Roto Ink “outside Director/redirect floater” rows without weakening foreign-popup checks (`aluiperfselftest.cpp:3793-3796`).
- Diopter Blend/Power were correctly disabled because the fixture set tool/focus modes but omitted `CineDiopterEnabled`; the reveal now drives the master relevance gate too (`functional_report.py:839-848`).

## Safety and allow-list

The in-world allow-list is unchanged, so `functional/INWORLD_NEW_ROWS.md` still authorizes no new rows. All new fixtures are session/viewer-local and restored. Avatar motion remains qualified through existing `LLActorMover`/`ALDanceBall` local paths; no `gAgent` movement/animation send was added.

## Changed files

- `indra/newview/aluiperfselftest.cpp`
- `indra/newview/animationexplorer.cpp`
- `scripts/perf/functional_report.py`
- `doc/director_kit/RUN_INWORLD5_TRIAGE.md`
