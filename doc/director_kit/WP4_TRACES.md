# Director kit WP4 traces

These traces describe the implemented Rig/Rim paths on `feature/director-kit-p0`. They are code traces, not claims of an in-world run.

## Drag Key in the map -> dial value

1. `indra/newview/allightrigmap.cpp:ALLightRigMap::beginDrag` opens one Rig-page mouse gesture, selects Key, and writes `DirectorV2SelectedLight = 0`.
2. Both selector cards observe that setting in `indra/newview/alkitcard.cpp:ALKitCard::postBuild` and reconcile their active options in `ALKitCard::syncSelector`; Rig Lights and Rim Per-light rim therefore select Key together.
3. The Fixture row's `mirror_setting` observer in `indra/newview/alkitrow.cpp:ALKitRow::postBuild` selects index 0 and calls `cine_fixture_role->onCommit()` inside `ALKitHistory::ScopedSuspend`. `indra/newview/alpanelcinelightrig.cpp:ALPanelCineLightRig::onFixtureRoleCommit` updates the fixture role and repopulates its dependent controls. A direct fixture-role user commit writes the same index back to `DirectorV2SelectedLight`.
4. Pointer motion reaches `indra/newview/allightrigmap.cpp:ALLightRigMap::applyDrag`, which writes `CineLightRigKeyYaw` and `CineLightRigKeyPitch`. The setting-bound `cine_key_yaw`/`cine_key_pitch` dials receive those values through their control variables, so their displayed values follow the map drag.

## Toggle Easy -> cards

1. The visible shell segment created by `indra/newview/alkitshell.cpp:ALKitShell::refreshModeProxy` calls `indra/newview/alkitpage.cpp:ALKitPage::setModeIndex(index, true)`.
2. The page resolves its instance-scoped `cine_rig_easy` adapter, registered in `indra/newview/alpanelcinelightrig.cpp:ALPanelCineLightRig::postBuild`, and calls `ALPanelCineLightRig::applyEasyMode` with `USER` origin. That function owns the preference write, optional explicit normalization, effective-mode decision, and `syncEasyControls` publication.
3. The hidden `cine_easy_mode` switch publishes the effective result to `indra/newview/alkitpage.cpp:ALKitPage::syncEffectiveMode`; `ALKitPage::applyModeAndChip` then shows the four Easy cards or the fifteen Advanced cards.
4. Search reveal enters through `ALKitPage::revealCard` -> `setModeIndex(index, false)`. The adapter call is inside `ALKitHistory::ScopedSuspend`; passive entry requires an already Easy-native rig and publishes its effective result without explicit-user snapping.

## Rim preset -> one undo step -> Ctrl+Z restores

1. `indra/newview/alpanelrigrim.cpp:registerProvider` installs the idempotent `rig_rim` provider before the Rim page's preset strip builds.
2. `indra/newview/alkitpresets.cpp:ALKitPresetStrip::onSelect` opens one `Apply preset` mouse gesture, then `RigRimPresetProvider::apply` calls `indra/newview/alrigrimcontroller.cpp:ALRigRimController::applyPresetByName` for a built-in.
3. `alrigrimcontroller.cpp:applyRigRimPreset` writes the complete built-in row. `indra/newview/alkithistory.cpp:ALKitHistory::captureSetting` and `addEntry` collect/coalesce every watched write into that single open Step.
4. Director Ctrl+Z reaches `ALKitHistory::undo`; `ALKitHistory::replay` restores the Step's setting entries in reverse under suspension. There is no provider-side barrier or second gesture.

## Lights card reset -> active option only

1. The Lights card's reset callback is installed in `indra/newview/alkitcard.cpp:ALKitCard::postBuild` and opens one `Reset Lights` gesture.
2. `ALKitCard::syncSelector` makes exactly the `DirectorV2SelectedLight` option visible. The reset loop requires `rowBranchVisible(row, this)`, so rows under the three hidden `ALKitOption` ancestors are excluded.
3. The active option's visible rows (plus its locally hidden advanced rows) call `ALKitRow::reset`; a right-click `Reset all Lights` is the separate explicit all-option path.

## Page reset asks first -> one undo step

1. The page-bar reset calls `indra/newview/alkitpage.cpp:ALKitPage::resetPage`, which opens `GenericAlertYesCancel` through a page handle and performs no writes on Cancel or after page destruction.
2. On Yes, the callback opens one `Reset page` mouse gesture, then resets every resettable non-preset, non-who row across all cards.
3. Setting and widget records coalesce in `indra/newview/alkithistory.cpp:ALKitHistory::addEntry`; the next gesture or the settle timer closes one Step, so one Ctrl+Z restores the page reset.

## Rail selects Mara (Subject B) -> anchor slot 2 -> undo not polluted

1. A rail commit reaches `indra/newview/alkitshell.h:ALKitShell::setCastSelection`; the built Rig page callback installed by `indra/newview/alkitpage.cpp:ALKitPage::postBuild` resolves `cine_rig_slot` unless its who chip is pinned.
2. The adapter registered by `indra/newview/alpanelcinelightrig.cpp:ALPanelCineLightRig::postBuild` maps the Subject B UUID to combo value 2, then the page sets `cine_anchor` and commits it.
3. `ALPanelCineLightRig::onAnchorSelected` wraps synchronous `ALCineLightRigManager::setSelectedSlot(SUBJECT_B)` in `ALKitHistory::ScopedSuspend`, then calls `history.barrier("rig slot changed")`.
4. The new context is `slot:2`; none of the synchronous slot-restoration writes enter undo, and the barrier leaves no cross-slot undo Step.

## Dynamic visibility and header presets

- Clip, shadow hint and gobo preview visibility is written by `ALPanelCineLightRig::updateDerivedStatus` / `syncGoboLibrary`; the allocation-free row visibility probe in `indra/newview/alkitcard.cpp:ALKitCard::draw` detects the control edge and requests a coalesced layout.
- Rim's avatar-tag edge is written by `indra/newview/alrigrimcontroller.cpp:ALRigRimController::tickFrame`; the same card probe adds/removes the row.
- `indra/newview/alkitcard.cpp:ALKitCard::postBuild` reparents each `card_preset` control into its card, preserving the control object and the callbacks later installed by `ALPanelCineLightRig::postBuild`; `layoutBody` places first-sub presets in the header and later-sub presets beside their sub-heading.
- A passive `BDMergeProjectorVolumetricsFeather` write reaches the Easy feather row's `ALKitSettingsListBinding` watch in `indra/newview/alkitbinding.cpp:ALKitSettingsListBinding::connectChanged`, which recomputes amber immediately. `ALPanelCineLightRig::syncEasyControls` copies the setting to `cine_easy_cone_feather` without committing, and the visible-page 10 Hz `indra/newview/alkitpage.cpp:ALKitPage::draw` call to `ALKitRow::pollLiveValue` refreshes its displayed value.
