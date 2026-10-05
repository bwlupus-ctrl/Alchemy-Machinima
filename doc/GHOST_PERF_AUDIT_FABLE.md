# Ghost Studio / entity clone — top-down performance audit (Fable)

Read-only audit per `doc/GHOST_PERF_AUDIT_BRIEF.md`. Capture: `C:\Users\xianw\Desktop\Tracy\captures\alchemy_20261005_103942.tracy` (20 s, 305 frames, mean 66 ms, worst 399 ms).

Written incrementally; sections are appended as the audit proceeds.

## 0. Capture facts (from the aggregate CSV)

| Zone | file:line | count | mean | max | total |
|---|---|---|---|---|---|
| LLAppViewer::doFrame | llappviewer.cpp:1397 | 303 | 66.1 ms | 399.3 ms | 20.02 s |
| render_ui_2d | llviewerdisplay.cpp:1980 | 305 | 35.2 ms | 359.1 ms | 10.74 s |
| LLFloater::draw | llfloater.cpp:2140 | 2745 | 3.8 ms | 357.9 ms | 10.55 s |
| LLTextBase::appendText | lltextbase.cpp:2590 | 4832 | 2.06 ms | 351.8 ms | 9.97 s |
| LLTextBase::appendTextImpl | lltextbase.cpp:2463 | 4783 | 2.09 ms | 351.8 ms | 9.97 s |
| LLPipeline::generateSunShadow | pipeline.cpp:26999 | 442 | 6.7 ms | 23.6 ms | 2.98 s |
| LLReflectionMapManager::update | llreflectionmapmanager.cpp:284 | 305 | 4.0 ms | 23.3 ms | 1.22 s |
| rmmu - cine budget | llreflectionmapmanager.cpp:978 | 38 | 12.6 ms | 21.9 ms | 0.48 s |
| LLAppViewer::idle | llappviewer.cpp:5104 | 305 | 3.3 ms | 16.2 ms | 1.00 s |

Headline numbers:

- `LLTextBase::appendText` alone accounts for **9.97 s of the 20.02 s capture (50 % of all main-thread time)**, i.e. **32.7 ms per frame on average**, not just the one spike. UI-2D (35.2 ms/frame) is ~93 % appendText. The whole 3D render (`display - 2`, shadows, deferred lighting, probes) is ~30 ms/frame; the UI costs more than the world.
- 4,832 appendText calls / 305 frames = **15.8 appendText per frame, every frame**. Only ~49 of those calls were cheap enough not to reach appendTextImpl.
- Max appendText = 351.8 ms in a single call = the 399 ms frame.

### 0.1 Per-call data (tracy-csvexport -u, written to `C:\Users\xianw\AppData\Local\Temp\ghostaudit\`)

The 4,832 appendText calls are **bimodal**, not a broad distribution:

| bucket | calls | total |
|---|---|---|
| < 0.1 ms | 4,801 | 17.5 ms (whole capture) |
| 0.1 - 1 ms | 2 | ~0 |
| 1 - 100 ms | 0 | 0 |
| **> 100 ms** | **29** | **9,954 ms** |

The 29 big calls are each 332-352 ms and arrive on a **0.5 s cadence** (gaps 0.50-0.55 s, with occasional skips to exactly 1.0 s or 1.5 s):

```
t(s)   gap    dur        t(s)   gap    dur        t(s)   gap    dur
0.040  ----   350.7     7.807  0.506  348.5     13.995 1.041  338.3
0.592  0.551  350.2     8.324  0.517  339.1     15.548 1.553  348.8
1.113  0.522  351.8     8.825  0.501  337.5     16.056 0.508  348.8
2.638  1.525  347.3     9.349  0.524  332.2     16.558 0.503  341.6
3.168  0.530  350.3     9.869  0.520  336.0     17.089 0.531  346.5
4.222  1.054  343.6    10.390  0.521  334.6     17.593 0.503  349.9
5.253  1.030  342.9    10.911  0.521  335.0     18.128 0.535  345.4
6.283  1.030  336.6    11.422  0.512  334.8     18.633 0.505  344.9
7.301  1.018  338.1    11.928  0.505  339.8     19.153 0.520  346.4
                       12.953  1.026  346.9     19.665 0.513  347.7
```

Frames without a big call are 31-44 ms with 15-21 cheap appendText calls each (so the ~16/frame "setText every frame" traffic is real but costs ~0.06 ms/frame; it is NOT the hitch). Frames with a big call are 388-399 ms. The remaining 31-44 ms baseline is 3D (shadows ~6.7 ms, deferred lighting 2.4 ms, probe update 4 ms incl. the 12.6 ms "cine budget" steps on 38 frames, Swap 5.9 ms).

Other UI zones in the capture confirm where the cost is NOT: `LLTextBase::reflow` 63,135 calls, max 0.01 ms; `LLTextBase::draw` 21,045 calls, max 0.88 ms; `LLFontGL::getWidthF32` max 0.13 ms; `LLView::findChildView` 163,433 calls (536/frame, 0.4 ms/frame -- getChild-by-name inside draw() paths, see section 3).

Tracy messages in the capture (`-m`) are only ActorMask / CineLightRig log lines; nothing from Ghost Studio, so the UI side was not instrumented beyond the stock appendText zones. The identification below is therefore code-path analysis anchored on the three hard facts: one appendText call per spike, 0.5 s cadence, ~345 ms each.

## 1. The exact spike: `rig_diagnostics` setText every 0.5 s, O(lines x total length) inside appendText

### 1.1 Which call

`ALPanelGhostStudio::refreshRigDiagnostics()` (`indra/newview/alpanelghoststudio.cpp:1612-1645`) is the only Ghost Studio refresher on a 0.5 s timer, and the only one that writes a multi-kilobyte string into a text widget:

```cpp
if (selection == mRigDiagnosticsSelection &&
    mRigDiagnosticsTimer.getElapsedTimeF32() < 0.5f) return;     // 1616-1617
...
text = ghost ? ghost->getRigDiagnosticText() : "...";               // 1636-1638
if (mRigDiagnostics->getText() != text)                             // 1641
    mRigDiagnostics->setText(text);                                 // 1643
```

`mRigDiagnostics` is the `<text_editor name="rig_diagnostics" font="Monospace" max_length="1048576" word_wrap="false">` at `skins/default/xui/en/panel_ghost_studio.xml:303` (declared `LLTextEditor*` at `alpanelghoststudio.h:298`). Every other setText in the panel is a one-line LLTextBox (status strings) -- those are the 4,801 sub-0.1 ms calls.

The text comes from `LLGhostAvatar::getRigDiagnosticText()` (`indra/newview/llghostavatar.cpp:3000-3129`): 2 header lines, then for every cloned linkset root and child prim 2 lines (clone/source), then **one ~110-char line per face of every clone prim**. A mesh-body avatar with N attachment prims yields roughly 2 + N*(2 + faces) lines; with 50-80 prims and up to 8 faces each that is 400-800 lines, 40-90 KB.

### 1.2 Why one setText costs ~345 ms (the quadratic)

`LLTextEditor::setText` (`indra/llui/lltexteditor.cpp:332-355`) -> `LLTextBase::setText` (`lltextbase.cpp:2415-2438`) -> `appendText` -> `appendTextImpl` (2461). `parse_urls` is false for this widget (widget default `widgets/text_editor.xml`, `simple_text_editor.xml`; `LLTextBase::Params` default `lltextbase.cpp:182`), `parse_highlights` is false (184), so the URL/highlight scanners are not involved. The path is:

- `appendAndHighlightText` (2823-2847) splits the string on `\n` and, **per line**, calls `appendAndHighlightTextImpl` (2718) followed by `appendLineBreakSegment` (2687).
- `appendAndHighlightTextImpl` calls `setCursorPos(old_length)` at 2730 and then `setCursorPos(cursor_pos)` / `setCursorPos(getLength())` at 2811-2819. Because `appendLineBreakSegment` inserts without moving the cursor, the cursor is always one behind the end when the next line starts, so **both** setCursorPos calls take the `new_cursor_pos != mCursorPos` branch.
- `LLTextBase::setCursorPos` (`lltextbase.cpp:3477-3502`) calls `wstring_grapheme_align_forward(text, pos)` (3491-3493) on the **whole document** (`getWText()`).
- `wstring_grapheme_align_forward(wstr, pos)` (`indra/llcommon/llstring.cpp:939-942`) is the convenience overload that calls `wstring_find_emoji_clusters(wstr)` (829-858), which **walks every codepoint of the whole string** (`is_shaping_starter` per char, 758-779) and allocates a vector, then throws it away.

So each appended line costs two full scans of the text accumulated so far: for L lines of average length c the total is about `L^2 * c` codepoint visits. For 600 lines x 100 chars that is ~36 M `is_shaping_starter` calls plus 1,200 vector allocations -- a few hundred milliseconds, i.e. exactly the observed 332-352 ms and exactly why it scales with how much the clone wears. `startOfDoc()` (3141) at the end of setText is one more full scan. None of this is visible as a child zone because none of `setCursorPos`, `insertStringNoUndo`, `appendAndHighlightTextImpl` or `wstring_find_emoji_clusters` has a profiler zone.

This is an Alchemy-side regression in llui: stock `LLTextBase::setCursorPos` is O(log n). The grapheme-cluster snap was added so mouse clicks never park a caret inside a ZWJ sequence; it is correct, but it runs on programmatic appends where the position is already a line boundary, and it recomputes the cluster list from scratch instead of using the cached-cluster overload (`llstring.cpp:924`). The second align site in lltextbase.cpp is `3218` (mouse-driven `setCursorAtLocalPos`), which is per click and harmless. `LLLineEditor` has the same pattern (`lllineeditor.cpp:512, 587`) but its text is one line.

Secondary, smaller per-line costs in the same loop (linear, not quadratic): `getEditableIndex` (two `std::multiset` lookups), `updateCursorXPos` -> `getLocalRectFromDocIndex` -> `getDocRectFromDocIndex` (walks the segments of the stale last line and measures them with the font), `LLStyle::Params` copy + `new LLStyle` per segment, `utf8str_to_wstring` per line, two `insertStringNoUndo` per line (each: multiset lookup, segment shift loop over the 1-2 trailing segments, `u32string::insert` at the end, `truncate()` check which is cheap here because `getLength() < max_length/4`).

The cheap path also matters for a second reason: `mLineInfoList` is only rebuilt by `reflow()` from `draw()`. The capture shows `reflow` max 0.01 ms, i.e. the rig_diagnostics editor itself was **never drawn** during the capture (it is at top=570 in a 534-px-high host and `LLView::drawChildren` skips children whose screen rect does not overlap the root, `llview.cpp:1265-1268`). The 345 ms was paid for a widget nobody could see. If the user scrolls it into view, a 600-line reflow + draw lands on top.

### 1.3 Why it recurs every 0.5 s while camming (the diff guard that cannot hold)

The guard `mRigDiagnostics->getText() != text` (1641) only helps when the string is byte-identical. `getRigDiagnosticText()` embeds per-face render state that changes with the camera:

- `in_draw_pass = face->mDrawInfo != nullptr` (3074) and `draw_ready` (3075-3078) flip whenever a face is culled / un-culled, changes LOD, or its draw pool rebuilds -- i.e. constantly while camming around a clone;
- `binding` / `source_binding` / `same=` (`getTEImage`, 3053-3058, 3103) change as textures resolve or swap;
- `drawable_invisible` (3070-3073).

So whenever anything on the clone or its source changed visibility state in the last 0.5 s the whole text differs and the full setText runs. The 1.0 s / 1.5 s gaps in the spike list are the checks where nothing happened to change -- the guard works, it just rarely gets the chance. The selection check (`selection == mRigDiagnosticsSelection`) does not help either: the selection is stable, so the 0.5 s timer is the only gate.

Also note the guard itself: when the text is unchanged, `getText()` is cheap (view model caches the UTF-8 string, `llviewmodel.cpp:116-140`), but `getRigDiagnosticText()` still runs every 0.5 s (hundreds of `llformat` + `LLUUID::asString()` + `gObjectList.findObject` + `dynamic_cast` per prim).

### 1.4 Multiplier: two hosts

`panel_ghost_studio` is instantiated by `floater_ghost_studio.xml:18` and by the Director console Ghosts tab (`floater_director.xml:566-575`, hosted by `LLFloaterDirector`, `llviewerfloaterreg.cpp:773`). `LLTabContainer` only draws the selected tab, so the Director copy costs nothing unless the Ghosts tab is the active tab; with the standalone floater open at the same time both copies run `draw()`, both have their own 0.5 s timer, and the spike doubles (or lands twice per second, offset). The capture shows one 0.5 s train, i.e. one host visible.

### 1.5 Verdict

**Root cause of the ~350 ms hitch:** `refreshRigDiagnostics` rewrites a 400-800-line diagnostic dump into an `LLTextEditor` every 0.5 s whenever any face's cull/draw state changed, and `LLTextBase::appendText` is quadratic in line count because `setCursorPos` runs a whole-document emoji-cluster scan twice per appended line. Either half alone would be a bug; together they are the 10 s of the 20 s capture.

## 2. Ghost Studio UI audit, refresher by refresher (`ALPanelGhostStudio::draw`, `alpanelghoststudio.cpp:834-920`)

Everything below runs **every frame the panel is drawn**, for each visible host, regardless of whether the viewer window has focus, whether the user is interacting, or whether the ghost model changed. Allocation notes count heap allocations per frame in the steady state (one entity clone, one cast member, nothing changing).

| # | Refresher (line) | Guard | Per-frame work in the steady state | Allocates per frame | Rebuilds / large writes when the guard fails |
|---|---|---|---|---|---|
| a | `mShowAllCheck` / `mEditModeCheck` / `mHint` sync (838-865) | diffed | 2 compares, `ALToolGhostEdit::getInstance()` | 0 | - |
| b | `refreshSourceCombo` (925-954) | signature string | builds `sig` from every cast member: `mId.asString()` + `mLastName` + `'|'` | ~3 strings per cast member | `clearRows` + `add` per member, `sourceName()` (name-cache lookups), then `populateLensGazeCastTargets` (another clearRows/add loop) |
| c | `refreshList` (1042-1179) | signature string | per instance: `asString()` x2, `sourceName()`, `llformat`, `state_name`/`style_name`, `std::set<LLUUID> signed_groups`, `groupForMember` x1-2 | ~6-8 strings + 1 set node per instance | `deleteAllItems` + `addElement(LLSD row)` per instance (7 LLSD columns each) + group header rows; `selectByID` |
| d | `refreshLookTargetCombo` (972-1008) | signature string | `asString()` per cast member and per instance (string concatenation, temporaries) | ~2-4 strings per member/instance | `clearRows`, `add` x (2 + cast + instances), `setSelectedByValue` |
| e | `refreshAnimationLibrary` (1015-1022) | one-shot flag + `isInVisibleChain` | 1 bool + visibility walk | 0 | one inventory walk (`ALGhostAnimAssetIndex::refreshInventory`, `collectDescendentsIf` from root + sort) on first visibility |
| f | selection mirror (874-907) | revision + compare | `getSelected`, `groupForMember`, `selectedListValue()` (LLSD->UUID) | 0-1 | `deselectAllItems` + `selectByID` |
| g | `syncCrowdFacingPointPrototype` (3691) | - | (not read in detail; draft-gated) | - | - |
| h | `refreshDetail` (1181-1573) | **none** on the enable/visible block; partial on values | ~70 `setEnabled`/`setVisible` calls unconditionally; `selectedInstances()` **twice** (1195, 1265: vector + `std::set` each); `selectedListValue`/`selectedInstance` (LLSD->UUID); `mScaleSpin->setToolTip(llformat(...))` builds a string every frame (1383-1389); position/scale/yaw/`setValue` on ~15 spinners/sliders/combos every frame when not focused (1394-1465; each LLSpinCtrl::setValue formats text into its line editor); `refreshAutoAnimate` (status struct copy + ~12 setEnabled; text diffed); for an entity clone: `mLensGazeStatus->setText(gaze_status)` **unconditionally** (1466-1468, this is one of the ~16 appendText/frame); `mDirectedAnimEdit->setText(asString())` every frame (1432-1435, LLLineEditor); for Directed mode `mAnimMetadataText->setText(metadata)` unconditionally (1515) and `index.requestMetadata()` every frame (1477, dedup'd inside); overlay ghosts: `LLDirectorCast::resolve` + `ghostBatchesFor` per frame (1564-1565); `mPoseStatus`/`mTypeText` diffed | ~10-15 strings, 2 vectors, 2 sets, several LLSD temporaries | the `row_value != mShownFor` block is correctly selection-gated |
| i | `refreshRigDiagnostics` (1612-1645) | 0.5 s timer + selection + string compare | see section 1 | 1 x 40-90 KB string per 0.5 s + hundreds of `llformat`/`asString` temporaries | **full `LLTextEditor::setText` of 400-800 lines, O(L^2) -- the spike** |
| j | `refreshStatus` (1575-1610) | string compare | `llformat` status string, `freezeStripStatus()` (returns std::string), `formationSupportsFreezeStrip`, `llformat("Strip %u:")` when a strip is active | 2-3 strings | setText on 2 text boxes |
| k | `refreshCrowdPlacement` (3225-3424) | draft revision for the import block; **none** for the rest | ~30 `setEnabled`/`setVisible` unconditionally; `std::string source_reason`; `crowdPlacementSourceUnchanged` when owner; `selectedInstance()`; `ALToolCrowdPlace::getInstance()->lastMessage()` copies; builds `status` string every frame (3354-3419) then diffs | 2-4 strings | setText diffed |
| l | `mNameplateMode->setValue(gSavedSettings.getS32(...))` (914-918) | focus only | a settings-map lookup by name + combo setValue every frame | 0-1 | - |

Observations on the pattern:

- The signature guards (b, c, d) are string-building guards: they avoid the rebuild but still allocate and concatenate dozens of strings per frame to decide that nothing changed. A revision counter on `ALGhostStudio` / `LLDirectorCast` would make the same decision for free; `ALGhostStudio` already has `mSelectionRevision` and per-instance `mTransformRevision`, and the crowd draft has `mRevision`, so the pattern exists but is not used for the lists/combos.
- `refreshDetail` is the only refresher with no top-level guard at all, and it is the biggest (390 lines). Its ~70 enable/visible writes are cheap individually (`LLView::setEnabled` is a store), but `LLSpinCtrl::setValue`, `LLSliderCtrl::setValue`, `LLComboBox::setValue` and `setToolTip` are not: each formats/compares strings or walks the combo list. Two `selectedInstances()` calls per frame allocate a vector and an `std::set`.
- The only widget that is **large** is the rig diagnostics editor; every other text widget is a one-liner. So the "16 appendText per frame" are all small (`mLensGazeStatus`, `mAnimMetadataText`, LLSpinCtrl internals do not go through appendText; other visible panels contribute the rest, see section 3) and total 0.06 ms/frame. They are a hygiene problem (reflow + segment churn every frame on ~15 widgets), not a frame-time problem.
- Nothing in `draw()` checks whether the panel is actually visible on screen beyond what `LLView::drawChildren` does for the panel itself; a child scrolled out of view (the diagnostics editor) still receives its setText.
- `onVisibilityChange(false)` (664-674) releases tools but does not reset any signature, so the first frame after re-show is a normal frame (fine).

### 2.1 Host-specific notes

- Standalone floater: `floater_ghost_studio.xml` wraps the panel with `follows`/scroll; the diagnostics editor sits at top=570 of a 780-px-tall document, so whether it is on screen depends on floater height.
- Director console Ghosts tab: `floater_director.xml:557-576`, panel height 534 -> the diagnostics editor (top 570, height 210) is **below the visible area** in that host unless scrolled, yet its 0.5 s setText still runs whenever the Ghosts tab is the active tab. In the capture `reflow` never exceeded 0.01 ms, consistent with the editor being off-screen in whichever host was visible.
- Both hosts also pay `LLFloaterDirector::draw()` (section 3) when the console is open.

## 3. Non-UI per-frame work that exists only while a clone exists

Entry points: `ALGhostStudio::updatePerFrame()` from `LLAppViewer::idle` (`llappviewer.cpp:5430`, before `gObjectList.update`), `ALGhostStudio::updatePostObjectList()` (`llappviewer.cpp:5440`, after it), `LLGhostAvatar::idleUpdate` (as an active object inside `gObjectList.update`), `ALGhostStudio::renderFormationPreview()` (`llviewerdisplay.cpp:1966`), `LLActorMover::renderStudioGhosts()` (overlay ghosts only). There are **no** idle callbacks, event timers or signal connections in any `alghost*.cpp` / `llghostavatar.cpp` / `alpanelghoststudio.cpp` (grep for `gIdleCallbacks|doOnIdle|LLEventTimer|connect(` is empty), so everything is pull-based from these hooks.

In the capture `LLAppViewer::idle` averaged 3.3 ms (max 16.2 ms) for the whole idle, so none of the items below is the hitch; they are listed for completeness and because several are the *source of the UI churn* (3.9).

| # | Work | Where | Per-frame cost / notes |
|---|---|---|---|
| 3.1 | `refreshLifecycleStates` | `alghoststudio.cpp:2403-2431` | per entity instance: `gObjectList.findObject` + `dynamic_cast<LLGhostAvatar*>` + `LLDirectorCast::resolve(source)` (`lldirectorcast.cpp:1665`: canonicalise + findObject + ghost/registry fallbacks). Writes `inst.mState` every frame; `state_name(inst)` is part of the list signature, so a READY <-> SOURCE_MISSING flap (source imposter / de-rez) rebuilds the list and the type text. Cheap otherwise. |
| 3.2 | `finishPendingRuntimeFreezes` | 660-679 | no-op unless a freeze is pending. |
| 3.3 | **AutoAnimate** `updateAutoAnimate` | 639-658 | only for instances with `mAutoAnimate`; `resolveEntityClone` (linear scan of instances + findObject + dynamic_cast) per such instance, `takeAutoAnimPublishedPool` (shared_ptr swap), `countAutoAnimActivity`; `logAutoAnimCost` is 60 s-throttled. The learner itself runs inside `LLGhostAvatar::idleUpdate` -> `autoAnimRecordTick` (`llghostavatar.cpp:542-695`): per frame it walks `mClonedLinksets`, and per linkset diffs every source prim's `object_anims` map against `channel.primLast` (map copies on change only), with `std::set changedIds`; the body channel folds `source->mSignaledAnimations` through `ChannelRecorder` every frame. Instrumented by `AutoAnimCostScope` -> the 60 s AUTOANIM-COST log only when `GhostAutoAnimateCostLog` is on. Off by default per instance; no UI coupling except the status struct the panel reads (3.9). |
| 3.4 | **Nameplates** `updateNameplates` -> `ALGhostNameplates::update` | `alghoststudio.cpp:1837-1937`, `alghostnameplates.cpp:64-125` | every frame, for every enabled instance: `nameplateAnchor` (entity: `resolveEntityClone` + `getJoint("mHead")` (string lookup) + world position; overlay: `LLDirectorCast::resolve` + two joint lookups + quaternion math), builds `std::vector<Label>` with a `std::string` per label, `std::set grouped_members`; then per label `projectPosAgentToScreen` and **`tag->setString(label.mText)` unconditionally** -- `LLHUDNameTag::setString` (`llhudnametag.cpp:384-388`) clears the segment vector and re-tokenises + re-measures the text with `font->maxDrawableChars` every frame (`addLine`, 396-465). `setColor` + `setPositionAgent` likewise every frame. Harmless at 1-10 plates, but a per-frame rebuild of something that changes only on rename/selection. Default mode 1 shows plates; gated off entirely when `GhostStudioNameplateMode <= 0`, `!mShowAll`, or UI hidden. |
| 3.5 | Facing / look-at / turns / motion | `updatePerFrame` 1946-2040, `updateLookAt` 1693, `stepAllTurns` 2083 | loops over instances; for a static single clone with no keep-facing/turn/motion it is a few compares plus two empty `std::set`/`std::vector` constructions. `applyEntityTransform` only on facing changes. |
| 3.6 | **True mirror** `updatePostObjectList` -> `applyTrueMirrorStamp` | `alghoststudio.cpp:2051-2081`, `llghostavatar.cpp:1568-1755` | only for `DRIVE_TRUE_MIRROR` instances: frame-guarded (`mTrueMirrorLastStampFrame`), one `findObject`, copies rotation+position for every skeleton joint (`stampTrueMirrorSkeleton`, by index with a name check per joint -- `getName()` string compare per joint per frame), ~30 face/physics visual params (`stampTrueMirrorFace`, `getVisualParam(id)` map lookups), `updateVisualParams()` if any changed, `updateWorldMatrixChildren()`. Roughly the cost of one extra `updateCharacter` pose pass; expected. |
| 3.7 | **Clone animation sync** (MIRROR) | `LLGhostAvatar::idleUpdate` 3351-3610 | per frame: `findObject(source)`; copies `source->mSignaledAnimations` into a temporary `signaled_animation_map_t` (map copy, allocation per entry) **every frame** just to erase two keys and compare (3390-3405) -- only then `autoAnimSyncBody`; per linkset: `findObject(root)`, optional `findObject(source root)`, per prim `object_anims.find` x2 and map compare, `std::vector latched_prims`; `updateControlAvatar()` only on change; `setAnimTimeFactor` re-assert per linkset when speed != 1 (findObject + dynamic_cast each); `repeatHeldAnimesh` is default-off (`GhostMirrorRepeatHeldAnimesh` false). Then `LLVOAvatar::idleUpdate` = the full cost of one more visible avatar (updateCharacter, motions, attachments, LOD, impostor logic). After it: the `log_healed_once` loop (3616-3644) does `dynamic_cast<LLVOVolume*>(gObjectList.findObject(clone_id))` for **every cloned prim every frame forever** before checking `mRigHealedLogged` -- cheap individually (hash lookup + dynamic_cast), but it is O(prims) wasted work that should check the set first and stop once all prims are logged. Then `applyDesiredGhostFootPosition` / `syncGhostObjectToMovingRoot` / `updateEntityOuterTransform` are all change-guarded (1109-1115, 1148-1152, 1199-1204). |
| 3.8 | **Pose polish** | `llvoavatar.cpp:5571-5627, 5790-5798` | runs inside every avatar's `updateCharacter`, including ghost clones; master gate `ALPolishEnabled` defaults **false** -> no-op. When enabled it costs one more polish pass per clone. `LLActorMover::applyGaze` (5775) and `applyOverride` (4388) are map lookups and return immediately when the clone has no gaze/move entry, and are explicitly skipped for TRUE_MIRROR clones. |
| 3.9 | **Spawn engine / material resolver / attachment enumerator** | `alghostspawnengine.cpp:456-575`, `alghostmaterialresolver.cpp`, `alghostattachmentenumerator.cpp` | spawn-time only: `ALGhostSpawnEngine::spawn/update/lock/despawn` are called from the explicit Studio operations; `ALGhostMaterialResolver::capture/apply` from `cloneAttachmentsFrom` (`llghostavatar.cpp:2107`); `verifyBindings` only from the chat-command test harness (`verifyClonedAttachments`, `alchatcommand.cpp:625`); `enumerateWorldRoots` from `cloneAttachmentsFrom` (2639) and `auditSource` (spawn). `visitWorldRoots` at `llactormover.cpp:13905` is inside `walkGhostSourceGeometry`, which is the ActorMask / overlay-ghost source walk -- per frame **only** for overlay ghosts or ActorMask recipients, not for an entity clone. No per-frame work for an idle entity clone. |
| 3.10 | **Animation asset index** | `alghostanimassetindex.cpp:238-320` | `refreshInventory` is one-shot (panel flag) or on the Refresh button; `requestMetadata` is called **every frame** from `refreshDetail` (1477) in Directed mode but dedups on `mMetadata` (one map lookup + `find()`); fine. |
| 3.11 | `renderFormationPreview` | `alghoststudio.cpp:4804-4819` | early-outs unless a crowd draft is active. |
| 3.12 | `ALAvatarLightResponse::tick` | `alavatarlightresponse.cpp:179-188, 322-351` | frame-guarded `ensureTable` (`mTableFrame == gFrameCount`); rebuilds the packed table only when dirty. Not clone-specific except KIND_CLONE rows. |

### 3.9 Does any non-UI work trigger the UI rebuilds?

Yes, three couplings, all through *state that changes every frame or every few frames while the camera moves*:

1. **Render/cull state -> rig diagnostics text** (the spike). `getRigDiagnosticText()` reads `face->mDrawInfo`, vertex-buffer/index counts, drawable INVISIBLE flags and current TE image bindings. Camming changes these (culling, LOD swaps, alpha pass assignment, texture resolution), so the 0.5 s poll sees a different string and runs the quadratic setText. This is the recurrence.
2. **Lifecycle flaps -> list rebuild.** `refreshLifecycleStates` writes `inst.mState` each frame; `state_name(inst)` is in `refreshList`'s signature and in `mTypeText`. A source that flips between resolved/unresolved (imposter distance, out-of-draw-distance, re-rez) rebuilds the whole scroll list and selection.
3. **Gaze / AutoAnimate status -> per-frame setText.** `mLensGazeStatus->setText(gaze_status)` (1466-1468) is written unconditionally every frame with whatever `LLActorMover::getGazeStatus` formats (likely containing live angles), and `refreshAutoAnimate` re-formats a `Learning m:ss / m:ss` string once per second by design (guarded on integer seconds, fine).

Transform revisions (`mTransformRevision`) do **not** reach the UI; the position/yaw/scale spinners are simply re-set every frame instead (1394-1426), which is the "diffless live readout" pattern.

## 4. Other always-open Alchemy panels with the same draw()-does-refresh pattern

Short list (file:line of `draw()` and what it runs unconditionally). Classification: **G** = guarded (signature/revision/timer), **U** = unguarded per-frame writes.

| Panel | draw() | Pattern |
|---|---|---|
| **Director console** `LLFloaterDirector::draw` | `llfloaterdirector.cpp:651-664` | 9 refreshers every frame. `refreshCastList` (1438) re-sets every cell every frame when membership is unchanged; `refreshAnimateTab` (2616) `mAnimateHeader->setText` **U** every frame (2625) + snapshots `mSignaledAnimations` into a vector every frame; `refreshCameraTab` (2929) four `setText` **U** every frame (2941-2947) + `castMemberName` lookups; `refreshStatusStrip` (3106) walks the cast with `isMoving` and formats a string every frame; `refreshGroupControls` **G** (names vector compare); `refreshMoveTab`/`refreshPathTab` push selection into child panels every frame (`selectedCastIds()` allocates). 57 `setText/setValue/clearRows/add` call sites in the file. This console alone contributes ~5 of the ~16 appendText/frame when open. |
| **Ghost Studio** `ALPanelGhostStudio::draw` | `alpanelghoststudio.cpp:834` | section 2. |
| **Cine Light Rig** `ALPanelCineLightRig::draw` | `alpanelcinelightrig.cpp:2665-2681` | `updateAnchorList` (1279) rebuilds label strings for 5 slots every frame (**U** on the string work; the combo write is diffed further down); `syncGroupControls` **G** (1496); `syncSeedEditor` **G**; `syncFixtureControls` (1131) `setValue` on 5+ controls **U** every frame unless focused, each a `gSavedSettings.getXxx(prefix + "...")` name-concatenation + settings-map lookup; `syncGoboLibrary` (1233) setValue + `LLUI::getUIImage(llformat(...))` **U** every frame, softness text **G**; `updateDerivedStatus` (2393) -> `syncObjectTargetControls`, `syncEasyModeForSelected`, `syncEasyControls`, `applyManualLock`, then `setValue` on 3 x LIGHT_COUNT controls **U**, several `gSavedSettings.getXxx("...")` by name per frame; status texts mostly **G** (`[UIPerf]` guards at 1262, 2556, 2596) but `mObjectTargetStatus`/`mGroupStatus`/`mShadowHint` setText are **U** on their branches (1414-1444, 1514-1539, 2454-2470). |
| **Lightbox** `ALFloaterLightBox::draw` | `alfloaterlightbox.cpp:607-...` | timer-throttled (0.25 s / 1 s) **G**, but inside the 1 s tick it does `getChild<LLTextBox>("...")` / `findChild` by name (630, 645-646, 651, 662) -- name lookups each tick, and `setValue` without diff (the Lightbox redo kept the throttles but not diff-guards). |
| **Ultimate Diopter** `ALFloaterUltimateDiopter::draw` | `alfloaterultimatediopter.cpp:472-517` | `refreshRelevance` **G** (POD signature key); `updateFocusReadouts`, `updateShowDepthCheckboxes`, `updateResetIndicators`, `checkRangeStaleness` run every frame by design, documented as "dedupe-guarded setText/setValue". The comment at 474-495 explicitly chose per-frame polling over signals; it is the house idiom being audited here. |
| **Prism manager** `LLFloaterPrismManager::draw` | `llfloaterprismmanager.cpp:715-775` | `pollSnapshots` is timer **and** revision gated (`mConfigurationRevision`, `mRuntimeRevision`, `mPerformanceRevision`, `mGateRevision`) -- the best-behaved panel in this list and the model for the fix. `refreshSelectionActions` + `refreshBoneJointChoices(false)` still run every poll. |
| **Director switcher / anim switcher** | `alpaneldirectorswitcher.cpp:206`, `alpaneldirectoranimswitcher.cpp:114` | `refreshBank(false)` loads + normalises the LLSD bank **every frame** (`loadNormalizedBank`: LLSD parse + 12 `llformat`s) and compares with `banksEqual` **G**; `refreshProgramState` every frame. Guard is right, the comparison input is expensive. |
| **Prop Mover** `ALPanelPropMover::draw` | `alpanelpropmover.cpp:129-137` | 0.5 s timer **G**, then `refresh()` does `clearRows` + full rebuild of both lists every 0.5 s with no change check (160-...). Same shape as the rig diagnostics, smaller payload. |
| **Path editor** `ALPanelPathEditor::draw` | `alpanelpatheditor.cpp:305-332` | 11 refreshers; `refreshList` **G** (snapshot diff), `refreshCopyCombo` **G**, `refreshHeader` **G**; `refreshInspector`/`refreshEditButtons`/`refreshChoreography` set ~20 enables and **tooltips with std::string temporaries every frame** (617, 1368-1372, 1488-1490) -- `setToolTip` is unguarded; `refreshPathControls` may `rebuild()` a dirty path from draw. |
| **Lens gaze** `ALPanelLensGaze::draw` | `alpanellensgaze.cpp:836-845` | `refreshEditActorCombo` **G**, `refreshCastCombo` **G** (signature built from `asString()` of every cast id + every selected id, every frame), `refreshControls` (1034) large unguarded value mirror. |
| **Gaze cues** `ALFloaterGazeCues::draw` | `alfloatergazecues.cpp:187-194` | `refreshCastCombo` **G** (string signature of ids per frame), `refreshCueList` / `refreshInfluenceList` **G** on `getGazeCueRevision()` -- revision-driven, good; `refreshEditorVisibility` cheap. |
| **Cine light cues** `ALFloaterCineLightCues::draw` | `alfloatercinelightcues.cpp:373-385` | `refreshListNames` **G** (string signature from `cueListNames()` vector copy every frame), `refreshCueRows` **G** (revision), `refreshStatus` **U** `mStatus->setText(llformat(...))` every frame (203-225). |
| **Flycam recorder** `ALPanelFlycamRecorder::draw` | `alpanelflycamrecorder.cpp:81-...` | `mTimeText->setText(llformat(...))` and `mStatusText->setText(rec.getStatus())` **U** every frame (110-112), `setLabel` x2, `setMaxValue`/`setValue` on the scrub slider every frame. Two of the ~16 appendText/frame when the Takes tab is visible. |
| **Actor mover panel** `ALPanelActorMover::draw` | `alpanelactormover.cpp:53-87` | diffed radio/dial; builds two tooltip strings every frame then `setToolTipIfChanged` **G**. |
| **Temporal capture** `ALPanelTemporalCapture::draw` | `alpaneltemporalcapture.cpp:75-...` | builds 3 strings per frame, all **G** via `mLast*` compare -- the correct minimal pattern. |
| **Avatar light response** `ALFloaterAvatarLightResponse::draw` | `alfloateravatarlightresponse.cpp:161-176` | 1 s timer + dirty flag **G**. |
| **Scene explorer** `ALFloaterSceneExplorer::draw` | `alfloatersceneexplorer.cpp:553-558` | all work moved to an idle callback with a visibility early-out -- another good model. |
| **Weather / Atmospheric volumes** | `alpanelweathersettings.cpp:151`, `alpanelatmosphericvolumes.cpp:101` | `updateDeferredAvailability` every frame (findChild by name x4 in `updateControlGroup`); `refreshBank(false)` every frame. |

Cross-cutting evidence from the capture: `LLView::findChildView` ran 163,433 times (536 per frame, 0.4 ms/frame). Those are `getChild<>()`/`findChild<>()`-by-name calls inside draw()-driven refreshers (Lightbox, Cine Light Rig `gSavedSettings.getXxx(prefix + ...)` is the settings-map analogue, Weather, Director). Not a hitch, but it is the same disease: per-frame work whose result never changes.

## 5. Ranked root causes

1. **`LLTextBase::setCursorPos` is O(document) and appendText calls it twice per line -> multi-line `setText` is O(lines x length).** `lltextbase.cpp:3477-3502` (`wstring_grapheme_align_forward` on `getWText()`), called from `appendAndHighlightTextImpl` 2730/2811-2819 for every `\n`-split line (2823-2847), plus `startOfDoc()` 2434. `llstring.cpp:939-942, 829-858`. This is the 332-352 ms per call. It affects **every** LLTextEditor/LLTextBox `setText`/`appendText` with many lines in the viewer (chat history, notecards, script editor, About, debug dumps), not just Ghost Studio -- the clone is simply what put an 800-line setText on a 0.5 s timer.
2. **`refreshRigDiagnostics` rewrites a 40-90 KB render-state dump every 0.5 s into an off-screen `LLTextEditor`.** `alpanelghoststudio.cpp:1612-1645`, text from `llghostavatar.cpp:3000-3129`. The string compare guard cannot hold because the text embeds `mDrawInfo`/cull/binding state that changes with the camera (3053-3078, 3103). No visibility check, no revision, no size cap, and the widget is below the visible area in the Director host (`floater_director.xml:566-575` vs `panel_ghost_studio.xml:303`).
3. **`ALPanelGhostStudio::refreshDetail` (1181-1573) + `refreshCrowdPlacement` (3225-3424) have no top-level guard**: ~100 `setEnabled/setVisible/setValue/setToolTip` writes, two `selectedInstances()` vector+set builds, several formatted strings, `mLensGazeStatus->setText` unconditionally, and ~15 spinner/slider/combo `setValue`s every frame, per host. Cost today ~1-2 ms/frame per host (not isolated in the capture because the panel has no zones); it is the systemic version of #2.
4. **String-signature guards that allocate to decide "no change"** (`refreshList` 1047-1079, `refreshLookTargetCombo` 975-987, `refreshSourceCombo` 930-940; same idiom in Lens Gaze, Gaze Cues, Cine Light Cues, Path editor copy combo). Dozens of `LLUUID::asString()` + concatenations per frame per list, while `ALGhostStudio`/`LLDirectorCast` already own revision counters (`mSelectionRevision`, `CrowdPlacementDraft::mRevision`, `getGazeCueRevision`) that would make the same decision in O(1).
5. **Per-frame unguarded text/tooltip/value writes across the other always-open panels** (section 4): Director console `refreshAnimateTab`/`refreshCameraTab`/`refreshStatusStrip` (5 setText/frame), Flycam recorder (2 setText/frame), Cine Light Cues `refreshStatus`, Cine Light Rig `syncFixtureControls`/`syncGoboLibrary`/`updateDerivedStatus` setValue storms with `gSavedSettings.getXxx(prefix + name)` lookups, Path editor tooltips, `findChildView` 536/frame. Each is ~µs; together they are the 15-21 appendText per frame and the ~2.6 ms/frame of UI-2D in non-spike frames, and they all re-reflow their widgets every frame.

Honourable mentions (non-UI, no fix required for the hitch): nameplate `setString` every frame (3.4), the `log_healed_once` findObject loop over every cloned prim every frame (3.7), the per-frame `mSignaledAnimations` map copy in MIRROR mode (3.7), `getRigDiagnosticText()` itself being regenerated every 0.5 s even when the guard then holds (1.3).

## 6. Fix design

The user asked for a permanent, systemic fix. Three layers, each independently valuable; layer A alone removes the 350 ms spike for every widget in the viewer, layer B makes Ghost Studio (and the next panel) stop doing per-frame work by construction, layer C is the rule.

### A. llui: make `appendText`/`setText` linear again (the engine fix)

Files: `indra/llui/lltextbase.cpp`, `indra/llui/lltextbase.h`, `indra/llcommon/llstring.cpp/.h`.

1. **Bounded grapheme alignment.** Replace the `wstring_grapheme_align_forward/backward(wstr, pos)` convenience overloads (`llstring.cpp:939, ~910`) -- or add `wstring_grapheme_align_*_local(wstr, pos)` and switch `lltextbase.cpp:3218, 3491-3493` and `lllineeditor.cpp:512, 587` to it -- with an implementation that scans only a bounded window around `pos` (emoji ZWJ sequences are short; walk back from `pos` while the previous codepoint is a ZWJ / `isEmojiClusterExtender` / pictograph base joined by ZWJ, capped at e.g. 32 codepoints, then forward the same way). Same semantics for real clusters, O(1) per call. Keep the full-document `wstring_find_emoji_clusters` for the font shaper (`llfontshaping.cpp:509`), which already works on a slice.
2. **Fast path in `setCursorPos`** (`lltextbase.cpp:3480`): if `new_cursor_pos == 0 || new_cursor_pos >= getLength()`, or the codepoints at `pos-1`/`pos` are plain ASCII, skip alignment entirely. Appends always land on the end-of-document boundary.
3. **Batch cursor handling in `appendAndHighlightText`** (2823-2847): the per-line `setCursorPos` pair inside `appendAndHighlightTextImpl` exists to preserve a user selection/cursor across a chat append. Save selection/cursor once at the top of `appendTextImpl`, run the line loop with a flag that suppresses the per-segment save/restore, restore once at the end. This also removes 2 x L `updateCursorXPos` -> `getDocRectFromDocIndex` font measurements per setText.
4. **Add Tracy zones** so this can never hide again: `LL_PROFILE_ZONE_SCOPED_CATEGORY_UI` in `LLTextBase::setText`, `appendAndHighlightTextImpl`, `insertStringNoUndo`, `setCursorPos`, `LLTextBase::reflow` already has one; add `wstring_find_emoji_clusters` (category string). Cheap zones; appendText is already zoned.
5. **Optional but cheap:** `LLTextBase::setText` early-out when `utf8str == getText()` (after `removeCRLF`), so every caller gets the diff guard for free and the per-panel `if (getText() != s) setText(s)` boilerplate becomes unnecessary. Watch the one semantic: setText also resets selection/cursor/scroll (`startOfDoc`), so only early-out when there is no selection and the text is identical.

Expected result for the rig diagnostics write: from ~345 ms to low single-digit ms (L lines x per-line linear cost), before any Ghost Studio change.

### B. Ghost Studio: no work when nothing changed, no work when hidden

Files: `indra/newview/alpanelghoststudio.cpp/.h`, `alghoststudio.cpp/.h`, `llghostavatar.cpp/.h`, `skins/default/xui/en/panel_ghost_studio.xml`.

1. **Rig diagnostics becomes on-demand.** Replace the 0.5 s poll (1612-1645) with: (a) only run when `mRigDiagnostics->isInVisibleChain()` **and** its screen rect overlaps the host's visible rect (the same test `LLView::drawChildren` applies, `llview.cpp:1265-1268`; expose a small `LLView::isOnScreen()` helper or use `calcScreenRect().overlaps(root rect)`); (b) rebuild on a **revision**, not a timer: `LLGhostAvatar` keeps `mRigDiagRevision` bumped in `cloneAttachmentsFrom`/`releaseClonedAttachments`/texture-binding changes, and the volatile per-face render fields (`in_draw_pass`, `draw_ready`, `drawable_invisible`) move to a separate one-line "live" summary or are dropped from the dump (they are debug data; a "Refresh" button or a 2 s throttle for that section is enough); (c) cap the text (e.g. first N prims + "... and M more") -- a 1 MB `max_length` on a diagnostics widget is a footgun; (d) keep the `getText() != text` compare as a last-line defence but compare a cheap hash/size first.
2. **Model revision, not string signatures.** Add `U64 mInstanceRevision` to `ALGhostStudio`, bumped by every mutator that changes anything row-visible (`addInstance`, `removeInstance`, `renameInstance`, `setInstanceEnabled`, `setInstanceDriveMode`, `setInstanceLook`, freeze/unfreeze, group changes, lifecycle state changes in `refreshLifecycleStates` **only when the state actually changes**); `LLDirectorCast` gets `mCastRevision` (names, membership). `refreshList`, `refreshSourceCombo`, `refreshLookTargetCombo` compare `(studio_rev, cast_rev)` against the last-built pair and rebuild only then. Delete `mListSig`/`mSourceSig`/`mLookTargetSig`. Zero allocations per frame.
3. **Split `refreshDetail` into "selection changed" and "live values".** The enable/visible block (1205-1345) depends only on (selected row, instance kind/drive mode, group state, crowd draft active, clipboard flag): compute a small POD key (as the Diopter does with `ALDiopterSigKey`) and run the block only when the key changes. The live block (position/yaw/scale/gaze values, 1394-1468) is throttled to ~10 Hz by an `LLFrameTimer` (a readout nobody can read faster) and each write goes through a diff helper (below) so an unchanged value never touches the widget. `selectedInstances()` is computed once per draw and cached in a member (invalidate on selection revision).
4. **`refreshCrowdPlacement`**: same split -- the draft revision already exists (`draft.mRevision`); extend the gate to cover the enable block and the status string (key = `{draft.mActive, owner, blocked, revision, formation, guide shape, have_prototype, mActiveStrip}`).
5. **`refreshStatus`, `refreshAutoAnimate`, `mLensGazeStatus`**: route through the diff helper; `getGazeStatus` text at 4 Hz.
6. **Skip everything when not drawn**: `ALPanelGhostStudio::draw()` already only runs when visible; add the on-screen test for the heavy children (rig diagnostics, the list when scrolled out) and make `onVisibilityChange(true)` force a full refresh (set all cached revisions to 0) so the first frame after re-show is correct without per-frame polling.
7. **Non-UI hygiene (small):** `ALGhostNameplates::update` -- only `setString`/`setColor` when the label text/colour changed (cache per tag); `LLGhostAvatar::idleUpdate` `log_healed_once` -- check `mRigHealedLogged` before `findObject`, and skip the loop once every prim is logged; MIRROR path -- compare `mCloneDesiredAnimations` to the source map in place instead of copying the map every frame; `getRigDiagnosticText()` -- only build when the revision changed.

### C. The shared pattern (so the next panel gets it for free)

New header `indra/newview/aluirefresh.h` (or `indra/llui/llrefreshguard.h` so llui panels can use it too), ~80 lines, header-only:

```cpp
// Diff-guarded widget writes: no-op when the widget already shows the value.
void setTextIfChanged(LLTextBase* w, const std::string& s);        // compares getText()
void setValueIfChanged(LLUICtrl* w, const LLSD& v);                 // compares getValue() (LLSD ==)
void setToolTipIfChanged(LLUICtrl* w, const std::string& tip);      // replaces the 4 private copies
void setEnabledIfChanged(LLView* w, bool b); void setVisibleIfChanged(LLView* w, bool b);

// Revision-driven rebuild: rebuild only when any watched revision moved.
struct ALRevisionGate { std::array<U64,4> seen{}; bool changed(std::initializer_list<U64> now); };

// Throttled live readout: true at most every `hz` while the panel is drawn.
struct ALLiveReadoutClock { F32 period; LLFrameTimer t; bool due(); };

// On-screen test for a child whose refresh is expensive.
bool isWidgetOnScreen(const LLView* v);   // visible chain + screen-rect overlap with root
```

Rules (add to `CLAUDE.md` / the UI section of the repo guide so reviews catch violations):

1. `draw()` may **only** (a) check revisions/timers, (b) call diff-guarded writes. Never `setText`, `clearRows`, `deleteAllItems`, `removeall`, `add`, `setToolTip`, `getChild/findChild`-by-name, `gSavedSettings.getXxx("name")`, or `LLUUID::asString()` directly in a draw path.
2. Model objects that UI mirrors expose a `U64 revision()`; UI compares revisions, never rebuilds signature strings.
3. Any widget that can hold more than ~1 KB of text (text_editor, scroll list with >50 rows) is refreshed on demand or at <= 2 Hz, only while on screen, and never from a string compare of its own contents.
4. Live numeric readouts are throttled to <= 10 Hz and diff-guarded.
5. Every panel `draw()` gets a Tracy zone (`LL_PROFILE_ZONE_SCOPED_CATEGORY_UI` named after the panel) so the next regression shows up as a named zone instead of inside `LLFloater::draw`.

Where it changes, concretely:

| File | Change |
|---|---|
| `indra/llcommon/llstring.cpp/.h` | bounded `wstring_grapheme_align_*` (A1) |
| `indra/llui/lltextbase.cpp` | `setCursorPos` fast path (A2), single save/restore in `appendTextImpl` (A3), zones (A4), optional identical-text early-out in `setText` (A5) |
| `indra/llui/lllineeditor.cpp:512,587` | use the bounded align |
| `indra/newview/aluirefresh.h` (new) | helpers (C) |
| `indra/newview/alghoststudio.h/.cpp` | `mInstanceRevision` + bumps; `refreshLifecycleStates` bumps only on change |
| `indra/newview/lldirectorcast.h/.cpp` | `mCastRevision` + bumps (membership, name resolution) |
| `indra/newview/llghostavatar.h/.cpp` | `mRigDiagRevision`; `getRigDiagnosticText(bool include_live_render_state, S32 max_prims)`; `log_healed_once` + MIRROR map-copy hygiene |
| `indra/newview/alpanelghoststudio.h/.cpp` | revision gates for list/combos; `refreshDetail` split + 10 Hz live clock; on-screen + on-demand rig diagnostics; cached `selectedInstances`; diff helpers everywhere; Tracy zone on `draw()` |
| `indra/newview/alghostnameplates.cpp` | cache text/colour per tag |
| `skins/default/xui/en/panel_ghost_studio.xml:303` | `max_length` down to ~32768; consider a "Refresh diagnostics" button |
| `llfloaterdirector.cpp` 2625, 2941-2947, 3106; `alfloatercinelightcues.cpp:203-225`; `alpanelflycamrecorder.cpp:110-112`; `alpanelcinelightrig.cpp` sync*/updateDerivedStatus; `alpanelpatheditor.cpp` tooltips; `alfloaterlightbox.cpp` getChild-by-name in the 1 s tick | mechanical swap to the diff helpers / cached widget pointers (follow-up batch, each a few lines) |
| `CLAUDE.md` | the five rules above |

Order of work: A1-A4 first (one small llui change, kills the 350 ms everywhere, fully testable with a unit test that appends 2,000 lines and asserts time < 50 ms), then B1 (rig diagnostics on demand), then B2-B6 + C, then the section-4 follow-ups.

## 7. Risks and verification

Risks:

- **A1/A2 (grapheme alignment):** the only behavioural contract is "caret never parks inside a ZWJ/VS/keycap/flag cluster". A bounded window must still see the whole cluster; flags are 2 codepoints, keycaps 3, ZWJ families up to ~10-12, tag sequences (subdivision flags) up to 14. A 32-codepoint window is safe; add the existing emoji test strings to the llcommon unit test (`llstring` tests) for both overloads and assert identical results to the full-scan version.
- **A3 (single cursor save/restore):** chat/IM transcripts rely on "cursor at end stays at end, selection survives append". Keep the restore logic identical, only hoist it; test: select text in a chat history, receive a message, selection unchanged; `track_end` editors still follow.
- **A5 (identical-text early-out in setText):** callers may use `setText(same)` to reset cursor/scroll/selection. Only early-out when there is no selection and `mCursorPos == 0` or `mTrackEnd`; otherwise keep the full path. Low risk, optional.
- **B1 (revision-driven diagnostics):** the dump loses its "live" cull columns unless the user presses Refresh; that is acceptable for a debug widget but should be stated in the label. The on-screen test must use the same rect logic as `drawChildren` or the panel will show stale text for one frame when scrolled into view -- force a refresh in the first on-screen frame.
- **B2 (revisions):** a missed bump = stale list. Mitigation: bump from the few central mutators (`ALGhostStudio::Instance` is only mutated through `ALGhostStudio` methods), add a debug setting `GhostStudioRefreshAlways` that forces the old every-frame compare for one session when chasing a stale-UI report, and keep `onVisibilityChange(true)` as a full refresh.
- **B3 (10 Hz live values):** spinner readouts lag by <= 100 ms behind a gizmo drag; the in-world gizmo is the primary feedback so this is fine, but keep the heading dial unthrottled if it feels laggy.
- **Two hosts:** both copies still refresh when both are visible; with revisions the second copy costs nothing extra beyond the compare.

Verification (Tracy, same capture recipe as `alchemy_20261005_103942`):

1. Before/after A: capture 20 s camming around one entity clone with the Ghost Studio panel open. Expect `LLTextBase::appendText` max < 5 ms (was 352), count unchanged (~16/frame) until B lands; `doFrame` max near the 3D baseline (~45 ms). Add the new zones (`LLTextBase::setText`, `appendAndHighlightTextImpl`, `insertStringNoUndo`, `setCursorPos`, `wstring_find_emoji_clusters`) and confirm `setCursorPos` total < 1 % of `appendText`.
2. After B: `ALPanelGhostStudio::draw` zone mean < 0.2 ms with a selected entity clone and no interaction; `LLTextBase::appendText` count per frame from the Ghost panel = 0 in the steady state (verify with the per-call export: `tracy-csvexport -u -f appendText`, bucket by frame as in section 0.1); `LLView::findChildView` per frame drops when the section-4 panels are converted.
3. Behavioural: select a clone, drag the gizmo -- position/yaw spinners follow at >= 10 Hz; rename/enable/group a clone -- list updates within one frame; scroll the rig diagnostics into view -- it fills within one frame and does not change while the camera moves unless Refresh is pressed (or the 2 s live section ticks, if kept); open the Director Ghosts tab and the standalone floater together -- one spike train must not reappear.
4. Regression guard: a `lltextbase` unit/integration test that `setText`s 2,000 lines x 100 chars into an `LLTextEditor` with `parse_urls=false` and asserts < 50 ms; a unit test for the bounded grapheme align against the full-scan version over the emoji test corpus.
5. Missing-evidence zones to add permanently (all UI-category, negligible cost): every `al*` panel/floater `draw()`; `ALGhostStudio::updatePerFrame`, `updateNameplates`, `updatePostObjectList`; `LLGhostAvatar::idleUpdate` (with sub-zones `autoAnimRecordTick`, `mirror ledger`, `applyTrueMirrorStamp`); `LLGhostAvatar::getRigDiagnosticText`. With those, the next capture answers "which panel" directly instead of via `LLFloater::draw`.
