# Spec: Light > Actors (Avatar Light Response) as a generated kit page

Written 10/15/2026, 6:00 AM. Spec only: no code, no build, no commit. Inputs: `WIRING_AUDIT_light_actors.md` (the audit), the legacy panel and store, the kit sources, and `PLAN_DIRECTOR_2026-10-13.md` (process, step 6).

**Line numbers** are from the working tree on 10/15. Other agents have uncommitted edits in `llfloaterdirector.cpp`, `alkitrow.cpp`, `alavatarlightresponse.{h,cpp}`, `panel_avatar_light_response.xml`, `kit_pages.json`, `director_mockup_data.json` and `director_kit_gen.py`. Re-anchor every edit by symbol, not by line.

**Dependency.** `chk_persist` and its store hook (`alavatarlightresponse.cpp:106-119`, `captureFileExtras`) are an uncommitted persistence change in the same tree. That change must land before WP2, because WP2 hosts `chk_persist` from `panel_avatar_light_response.xml`.

## 0. Decisions in one place

1. **Binding: root-class hosting of the existing panel.** The page is generated with `root_class: panel_avatar_light_response` and `v1_sources: [panel_avatar_light_response.xml]`. The legacy `ALPanelAvatarLightResponse` becomes the page root, and its own `postBuild` binds every per-avatar row by name. That code already writes to all selected avatars, reads from the first, saves on mouse-up and refreshes from the store subscription. This is the pattern already used by Light Rig, Rim, Stepped, Shot, Takes, Temporal and VCam. No new binding class is added (Section 2).
2. **The legacy panel stays.** The standalone floater and the v1 Director keep using it unchanged. The kit page replaces only the v2 Director embedding (Section 3).
3. **Two small panel changes** benefit all three hosts:
   - conditional rows reveal through `ALKitRow::setControlVisible`;
   - writes made without a mouse (keyboard, row paste, Quick) get a debounced save.
4. **One Director binder,** `bindLightActorsPage`. It wires the three Director-only rows: Open window, Reset bias, and the Isolation test trigger.
5. **The `who` block and `who_picker` are removed.** The list is the picker.
6. **The preset combo and Apply go in the page bar.** They use the existing `widgets` preset strip. The generator learns an optional `apply` key.
7. **No mixed indicator.** The kit has no mixed state (2.3). The page keeps legacy semantics: it shows the first selected avatar and writes to all selected avatars.

## 1. Page layout

### 1.1 Page attributes (generated `kit_page`)

| attribute | value | source |
|---|---|---|
| root | `<panel name="actors_tab_logic" class="panel_avatar_light_response">` | kit_pages `root_class`/`root_name` |
| page / summary | `light_group/actors_tab` / existing `sum` | mockup |
| master | `chk_enabled`, label "Light response", setting `AvatarLightResponseEnabled` (Boolean, default 1, `settings.xml:30604`) | mockup `master` |
| who | **none** (removed; see defect a) | mockup edit |
| preset | `preset_mode="widgets"`. The `kit_preset_host` holds `combo_preset` then `btn_apply_preset` (hosted from v1). The bar order is combo, then button (`alkitpage.cpp:359-399`). | kit_pages `preset` + generator change |
| sections / chips | Avatars = avatar_list, light_response, light_presets. Mask & silhouette = actor_mask, silhouette. Advanced = actors_diagnostics. Unchanged from the mockup, so the visual match holds. | mockup |
| watch_settings | generated: `AvatarLightResponseDebugTint, AvatarLightResponsePersist, CineLightRigActorMaskBias, CineLightRigActorMaskDebug, CineLightRigActorMaskDisable`. The trigger `CineLightRigActorMaskTest` leaves this list, so undo history no longer watches a command. | `director_kit_gen.py:696-699` |

### 1.2 Cards and rows, in display order (6 cards, 31 kit rows, plus the master and a 2-widget preset strip)

Columns:
- **Bind**:
  - "panel": hosted v1 widget, bound by `ALPanelAvatarLightResponse::postBuild` (`alpanelavatarlightresponse.cpp:87-175`);
  - "setting": kit setting binding;
  - "binder": `LLFloaterDirector::bindLightActorsPage`;
  - "action": a generated `lightbox:`/`floater:` action.
- **Range** is in the UI units shown, and equals the consumer clamp after the stated conversion. No range changes (audit Section 1).
- **H** = the row's own help text (`h`), plain English. Any help text not listed here keeps its current mockup text.

**Card avatar_list** ("Avatar list", section Avatars)

| row | kind | label | bind | range / items | dp | show_if | help |
|---|---|---|---|---|---|---|---|
| target_list | list (hosted scroll_list, multi_select, columns state/name/kind), row_height 140 | Avatars | panel: rows `collectRows` :329-424, selection `selectedKeys` :621-632, commit `onSelectionChanged` :806-813 | n/a | - | - | "Pick one or more avatars to change. Dot: green is showing, amber is changed but not showing, hollow is reset or bypassed, none follows its wearer or source." |
| chk_only_adjusted | toggle | Only adjusted | panel :115-119 (filter :445-457) | bool | - | - | "Lists only the avatars you have changed." |
| btn_refresh | button | Refresh | panel :120-126 | - | - | - | "Rebuilds the list now. It also updates by itself once a second." |
| light_response_popout | button | Open window | binder (moved from `llfloaterdirector.cpp:1970-1974`) | - | - | - | "Opens Avatar Light Response in its own window for the selected cast member." |
| chk_persist | toggle (**new mockup row**) | Remember between sessions | setting `AvatarLightResponsePersist` (Boolean, default 0, `settings.xml:30611`). Turning it on runs `captureFileExtras` + `saveToFile` (`alavatarlightresponse.cpp:106-119`). | bool | - | - | "Keeps each avatar's settings for this account after you log out. Off: changes last for this session only." |

**Card light_response** ("Light response", Avatars). The card's `ew`/`es` equal the master, so the generator drops them (`director_kit_gen.py:748-750`).

| row | kind | label | bind | range (UI ↔ store) | dp | help |
|---|---|---|---|---|---|---|
| sld_diffuse | slider, unit % (help only, `alkithelp.cpp:130`) | Diffuse | panel: `mDiffuse = v/100` :826, read :703 | 0..100 st 1 ↔ 0..1 (`model.h:26-27`, `model.cpp:106`) | 0 | "Softens shine and highlights on the selected avatars. 0 leaves them unchanged." |
| sld_tame | slider, unit % | Reflections | panel: `mTame = v/100` :827, read :704 | 0..100 st 1 ↔ 0..1 (`model.h:28-29`, `.cpp:107`) | 0 | "Dims reflections from the sun, sky, lights and mirrors. Ordinary lighting is unchanged." |
| sld_brightness | slider, unit EV | Brightness | panel: `mExposureEV = v` :828, read :705 | -2..1 st 0.05 ↔ same (`model.h:30-31`, `.cpp:108`) | 2 | "Makes the selected avatars brighter or darker, in stops. 0 is unchanged; highlights and glow keep their level." |
| sld_glow | slider, unit % | Glow | panel: `mGlow = v/100` :829, read :706 | 0..200 st 1 ↔ 0..2, default 1 (`model.h:32-34`, `.cpp:109`) | 0 | "Scales the avatar's own glow. 100 is unchanged, 0 turns it off. Actor FX effects are not affected." |
| chk_bypass | toggle | Bypass | panel `onBypass` :928-941 (saves now), read :707 | bool | - | "Shows the selected avatars unchanged, for a before/after check. Their settings are kept." |
| txt_brightness_ev | readout, **advanced (kept, see defect b)** | Brightness EV | panel `refreshBrightnessValue` :755-761 (`%+.2f EV`) | - | 2 | "The brightness change of the first selected avatar, in stops." |

**Card light_presets** ("Response presets", Avatars)

| row | kind | label | bind | help |
|---|---|---|---|---|
| btn_reset_selected | button | Reset | panel :964-973 (identity entry kept) | "Removes all changes from the selected avatars. They still do not follow a wearer or source." |
| btn_inherit_selected | button | Inherit | panel :975-984 → `store.inherit` (`alavatarlightresponse.cpp:287-308`); enable rule :736-752 | "Deletes the selected avatars' own settings so they follow their wearer or source again. Animesh and clones only." |
| btn_copy | button | Copy | panel :1004-1016 | "Copies the first selected avatar's settings." |
| btn_paste | button | Paste | panel :1018-1030 | "Pastes the copied settings onto every selected avatar." |
| btn_reset_all | button, advanced | Reset all... | panel :986-1002 (confirm `AvatarLightResponseResetAll`) | "Removes the settings of every avatar. Asks first." |

Page-bar preset strip (not rows): `combo_preset` (items Reset 0 .. Silhouette 8, equal to `EPreset`, `model.h:158-170`) and `btn_apply_preset` (`onApplyPreset` :943-962). Both are disabled with no selection (:730-731).

**Card actor_mask** ("Actor only", Mask & silhouette)

| row | kind | label | bind | range / items | dp | help |
|---|---|---|---|---|---|---|
| am_disable_check | toggle | Disable | setting `CineLightRigActorMaskDisable` (Boolean, `settings.xml:2204`) | bool | - | "Turns Actor only lighting off everywhere without losing your settings. Every light then lights the whole scene." |
| am_debug_combo | combo, advanced | Debug view | setting `CineLightRigActorMaskDebug` (U32, not persisted, `settings.xml:2211`) | Off 0, Overlay 1, Mask only 2 (consumer `alactormaskdiagnostics.cpp:1537`, no clamp) | - | unchanged |
| am_bias_slider | slider, advanced | Mask bias | setting `CineLightRigActorMaskBias` (F32, default -8, `settings.xml:2218`) | -64..0 st 0.5 (clamp `pipeline.cpp:25703`, also `:25538`) | 1 | "Depth nudge used to draw the actor mask. If the debug view shows holes, move it lower. Otherwise leave it at -8." |
| am_reset_CineLightRigActorMaskBias | button, advanced | Reset bias | binder: `resetToDefault` on `CineLightRigActorMaskBias` (today UNWIRED in the manifest) | - | - | "Sets Mask bias back to -8." |
| **am_test_trigger** (replaces am_test_combo) | button, advanced, label "Run test" / "Cancel test" | Isolation test | binder: writes 1 when the setting is 0, writes 2 when it is 1. The consumer resets it to 0 itself (`alactormaskdiagnostics.cpp:423-430, 680-705`). | command 1 / 2 | - | "Checks that Actor only lights change nothing outside the actor; results go to the log. Click again to cancel. Needs a still camera and subject." |
| open_lightbox_actormask | button, advanced | Lightbox › | action `lightbox:actor_mask_settings` | - | - | "Opens the full Actor Mask settings, with notes on their limits, in a separate window." (the old text tripped the `Lightbox`/`tab` audit terms, `kit_lint.py:69`) |

**Card silhouette** ("Silhouette", Mask & silhouette)

| row | kind | label | bind | range / items | dp | show_if | help |
|---|---|---|---|---|---|---|---|
| sld_sil_amount | slider, unit % | Amount | panel: `mSilAmount = v/100` :830, read :708 | 0..100 ↔ 0..1 (`.cpp:110`, `silQuant` :137-143) | 0 | - | "Fades the avatar from its normal look (0) to a flat, unlit colour (100). For exact key colours use Actor Chroma Key." |
| combo_sil_fill | combo (hosted, items carry values) | Fill | panel `onSilFillPreset` :878-903 (Custom is a no-op), read `matchFillPreset` :712-713 | Black 0, Charcoal 1, White 2, Chroma Green 3, Chroma Blue 4, Custom 5 (= `EFillPreset`, `model.h:105-114`) | - | - | "The flat colour used by Silhouette. Custom uses the colour swatch below." |
| swatch_sil_fill | color | Custom colour | panel `onSilFill`: commit (false,true), select (true,true), cancel (true,false) :146-148, :848-876 | each channel 0..1 (`.cpp:113`) | - | - | "Pick any flat colour for the silhouette. Picking one sets Fill to Custom." |
| chk_sil_haze | toggle | Haze | panel `onSilCheckbox(true)` :905-926, read :714 | bool, default on | - | - | "Lets fog and haze lie over the silhouette so it sits in the scene's air." |
| chk_sil_keep_rim | toggle | Keep rim | panel `onSilCheckbox(false)`, read :715 | bool | - | - | "Keeps rim light on the silhouette edge. Needs Rig Rim turned on in the Light Rig." |
| txt_sil_note | readout (**kit_pages add_row**, hosted, Orange) | Note | panel `refreshSilhouetteNote` :763-804 | - | - | `sil_note` | "Shown only while the silhouette is getting ready, or when Keep rim needs Rig Rim." |

**Card actors_diagnostics** ("Diagnostics", advanced card, section Advanced)

| row | kind | label | bind | show_if | help |
|---|---|---|---|---|---|
| chk_debug_tint | toggle | Debug tint | setting `AvatarLightResponseDebugTint` (Boolean, not persisted, `settings.xml:30625`; consumer `lldrawpool.cpp:541`) | - | "Paints every changed avatar magenta, so you can spot surfaces the change does not reach." |
| txt_status | readout | Status | panel `refreshStatus` :572-600 | - | "How many avatars are changed, how many the renderer applied last frame, and how many are on screen." |
| txt_budget | readout (**kit_pages add_row**, hosted, word_wrap → row_height 36) | Frame rate | panel `refreshStatus` :602-618 | `over_budget` | "Shown when changed avatars exceed your avatar impostor limit. They then draw at full detail, which costs frame rate." |

Coverage: all 28 named legacy widgets are on the page. Two are not rows:
- `chk_enabled` is the master;
- `combo_preset` and `btn_apply_preset` are in the bar.

`txt_sil_header` is dropped as obsolete, because the card title replaces it; it needs a parity_allow entry. The unnamed "Fill" label is not hosted.

### 1.3 Where page edits go (mockup source chain, confirmed)

`director_mockup_data.py` merges two inputs:
- `C:\Users\xianw\cxs\content_curated\*.json` (`CUR`, `director_mockup_data.py:21`);
- `C:\Users\xianw\cxs\director_content_map.json` (`MAP`, `:22`).

By default it writes `C:\Users\xianw\cxs\director_mockup\director_mockup_data.json` (`OUT`, `:23`, written at `:1008`), not the repo file.

That output is dated 10/10 7:49 AM and contains no `stepped_tab`. The repo file (468,523 bytes, 10/15) has been hand-edited since 10/10, for example Stepped (+46 rows, `360c226ec5f`) and the Step particle toggles (`22dd8e78475`). Regenerating from the curated folder would drop that work and trip the row-count pin.

Rules:
- **Edit `scripts/code_tools/director_kit/director_mockup_data.json` directly** (one-line compact JSON; use a small Python load/modify/dump with `separators=(",", ":")`, `ensure_ascii=False`). It is the design authority.
- Generator-only configuration (hosting, overrides, conditional readouts, preset strip) goes in `kit_pages.json`.
- **Do not run `director_mockup_data.py`.** Optionally mirror the same edits into `content_curated/light_group.json` (actors_tab), `inline_pages.json` (actor_mask) and `presets.json` for bookkeeping only.

Exact mockup edits (`light_group` / `actors_tab`):
1. Delete the page `who` block.
2. Delete `who_picker` from the `target_list` row.
3. Append the `chk_persist` row to card `avatar_list` (after `light_response_popout`):
   `{"l":"Remember between sessions","k":"toggle","w":"chk_persist","h":"<1.2>","kw":"persist save keep account remember","s":"AvatarLightResponsePersist","d":false,"ty":"check"}`
4. In card `actor_mask`, replace the `am_test_combo` row in place with:
   `{"w":"am_test_trigger","k":"button","l":"Isolation test","a":1,"btn":"Run test","h":"<1.2>","kw":"actor mask isolation check verify cancel"}`
   No `s`: a `control_name` on a button would bind its toggle state.
5. Replace the `h` of every row whose help is given in 1.2.
6. Leave `txt_brightness_ev` advanced (`a:1`).

Net rows: +1 (`chk_persist`); the trigger replaces the combo 1-for-1. The completeness pin moves +1 (Section 4).

### 1.4 Audit mockup defects: resolution

- **a. who/who_picker:** fixed by edits 1-2. The list is laid out again (`alkitrow.cpp:643-649`), and the inert chip (`alkitpage.cpp:653, 687-691`) is gone.
- **b. Brightness EV readout is advanced:** **not adopted.** The kit slider row already shows the number at all times, to 2 decimals: the hosted slider keeps `show_text`, with `text_width="52"` (`director_kit_gen.py:483-488`). The row label is "Brightness", and the help panel shows the unit `EV` (`alkithelp.cpp:130-131`). A second always-visible row would only add the sign and unit. The readout must still exist because the panel calls `getChild` on it (:109), so it stays as an advanced row, as in the mockup. If the user wants the unit on screen, label the slider "Brightness (EV)"; do not un-hide the readout.
- **c. Presets:** fixed by `preset: widgets` plus the generator `apply` key. The audit's open question is settled: `widgets` mode only moves whatever children the host has into the bar (`alkitpage.cpp:359-399`). Only the generator demands `save`/`delete` (`director_kit_gen.py:803-806`).
- **d. Jargon help:** replaced (1.2). The new texts avoid `kit_lint` audit terms (`kit_lint.py:69`).
- **e. Isolation test:** one self-resetting trigger button (1.2, binder in 2.5).

## 2. Per-avatar binding mechanism

### 2.1 What the kit offers today
- A kit row resolves a binding in this order (`ALKitRow::resolveBinding`, `alkitrow.cpp:756-800`):
  1. a setting;
  2. a `reset_settings` list;
  3. a page-scoped `ALKitBindingRegistry` factory (`alkitbinding.cpp:239-282`; it has **no callers** in `indra/`);
  4. a widget default (`default_value`).

  With none of these, the row is unbound and the control's own commit callbacks do the work.
- `ALKitSettingWatch` watches settings only (`alkitbinding.cpp:284-297`).
- The established pattern for state that is not a setting is a hosted **owner panel as the page root**:
  - Shot (`kit_pages.json:12`), Takes (`:65`), Temporal (`:66`) and VCam (`:70`);
  - Light Rig (`:76`, with `v1_sources`), Rim (`:104`) and Stepped (`:186`);
  - Stepped is the per-target precedent, with a hosted list row and per-actor code-bound sliders (`alpanelsteppedanim.cpp:25-90`; binder `llfloaterdirector.cpp:1486-1501`).

### 2.2 Chosen design: the legacy panel is the adapter
The generated root is `<panel class="panel_avatar_light_response">` (`LLPanelInjector`, `alpanelavatarlightresponse.cpp:66`).

With `v1_sources: [panel_avatar_light_response.xml]`, every row whose `w` names a legacy widget hosts a copy of that widget, stripped of geometry (`director_kit_gen.py:359-369, 461-506`). This keeps its items, columns, `multi_select` and `can_apply_immediately`.

Children are built before the root's `postBuild`, so `ALPanelAvatarLightResponse::postBuild` finds every row control by name (`getChild`, :90-111, :120, :155) and wires it. Each requirement maps to existing code:

| requirement | where it is already met |
|---|---|
| write to all selected avatars | every handler loops over `selectedKeys()`: :821-834, :856-861, :895-899, :911-923, :934-939, :954-959, :968-970, :1024-1027 |
| read from the first selected avatar | `refreshEditors` reads `keys.front()` (:698-700); editors are disabled with no selection (:720-734) |
| save on slider mouse-up only | drags call `applyParams(..., false)` (:833); mouse-up calls `saveIfPersist` (:139-143, :842-846) |
| do not reset a slider that is held | `mSliderDragging` (:134-138); `alIsBeingEdited` guard (:278-288) |
| refresh from the store, not `ALKitSettingWatch` | `setChangeCallback` (:160-164; `alavatarlightresponse.h` `setChangeCallback`) plus the revision gate in `draw` (:225-228); rows and status are coalesced to 1 Hz (:259-277) |
| refuse loudly when the store is full | notification `AvatarLightResponseCapReached` (:677-682) |

All 24 names in `postBuild` **must** exist on the generated page with compatible tags. A miss makes `getChild` build a dummy, and the row goes dead without any error. The parity gate (Section 4) and the static check in test T8 enforce this.

### 2.3 Mixed indicator: not supported by the kit
- The kit has no mixed or tentative concept.
- `LLSliderCtrl::setTentative` (`llsliderctrl.cpp:546-553`) only affects an editable value box, and kit sliders use the read-only text (`widgets/slider.xml:3`, `can_edit_text="false"`).
- `ALKitSwitch` draws its own track and has no tentative state (`alkitswitch.h`).
- Combos and swatches have none either.

The page therefore keeps legacy semantics: it shows the first selected avatar and writes to all selected. See open question Q1.

### 2.4 Kit behaviours that must not touch per-avatar state
- **Undo/redo.**
  - Problem: a code-bound row records widget history (`alkitrow.cpp:295-305`), and replay calls `setValue` + `onCommit` (`alkithistory.cpp:263-268`). That would write to whatever is selected at undo time.
  - Fix: set `no_widget_history` on these 12 rows: target_list, chk_only_adjusted, sld_diffuse, sld_tame, sld_brightness, sld_glow, chk_bypass, sld_sil_amount, combo_sil_fill, swatch_sil_fill, chk_sil_haze, chk_sil_keep_rim. Buttons and readouts are already excluded.
  - Result: per-avatar edits are not undoable, the same as the legacy panel and floater (Q3).
- **Page reset, card reset and row reset.**
  - These act only on `resettable()` rows (`alkitrow.h:69`; `alkitpage.cpp:1493-1501`; `alkitcard.cpp:333`).
  - Per-avatar rows have no binding as long as they carry **no `d`/`default_value`, no `reset_settings` and no `s`**. Then they are never reset, never amber, and get no reset icon. T8 asserts this.
  - "Reset page" does reset the setting rows, including `chk_persist` (Q2).
- **Row paste, arrow keys, Quick favourites and search.**
  - These commit through the control (`ALKitRow::setBoundValue`, `alkitrow.cpp:811-827`), so they write to the selection like a click does.
  - They never produce a slider mouse-up, so the panel needs a debounced save (2.5 b).

### 2.5 Code changes (WP1)

**a. Conditional rows** (all hosts). In `postBuild`, after the `getChild` block, add:
```cpp
// Kit host: txt_sil_note / txt_budget are show_if rows. Hide them through the row so a later
// reveal re-lays out the card (alkitrow.cpp:651-679). Outside a kit row this is a plain setVisible.
ALKitRow::setControlVisible(mSilNote, false);
ALKitRow::setControlVisible(mBudget, false);
```
Then:
- in `refreshStatus` (:613, :617), replace `setVisibleIfChanged(mBudget, …)` with `ALKitRow::setControlVisible(mBudget, over > 0)`;
- in `refreshSilhouetteNote` (:803), replace `setVisibleIfChanged(mSilNote, …)` with `ALKitRow::setControlVisible(mSilNote, !note.empty())`;
- add `#include "alkitrow.h"`. Precedent: `alrigrimcontroller.cpp:404, 415`.

The initial hide is required. `row->setControlVisible(true)` returns early when `mCodeHidden` is already false (`alkitrow.cpp:653-654`), so without it the first reveal would never request a layout.

**b. Debounced save for writes made without a mouse** (all hosts). The store has no save on quit; the floater saves on close (`alfloateravatarlightresponse.cpp:32`); the Director host has no such hook.
- Add `bool mSavePending = false; LLFrameTimer mSaveDebounce;` to the header.
- In `onSliderCommit` (:815-840) and `onSilFill(false, …)`, set `mSavePending = true; mSaveDebounce.reset();`.
- Clear `mSavePending` wherever `saveIfPersist()` already runs: :845, :864, :901, :924, :960, :971, :981, :1028, and in `onBypass` via `save_now`.
- In `idleUpdate`, after the editors block:
  ```cpp
  if (mSavePending && !mSliderDragging && mSaveDebounce.getElapsedTimeF32() >= 0.5f)
  {
      mSavePending = false;
      ALAvatarLightResponse::instance().saveIfPersist();
  }
  ```
- Flush the same way, without the timer condition, in the offscreen branch (:241-249) before returning.

Effect: drags still save only on mouse-up, never per tick, and keyboard/paste/Quick edits save 0.5 s after the last change.

**c. Director binder** (`llfloaterdirector.cpp` and `.h`).
- Declare `void bindLightActorsPage(LLView* root);` next to `bindSteppedPage` (`llfloaterdirector.h:105`).
- Add the member `boost::signals2::scoped_connection mActorMaskTestWatch;`.
- In the `bindPage` table (`llfloaterdirector.cpp:1381-1408`):
  - replace `{ "actors_tab", &LLFloaterDirector::bindCameraPage }` (:1397) with `{ "actors_tab", &LLFloaterDirector::bindLightActorsPage }`;
  - delete `{ "actors_tab", &LLFloaterDirector::bindLauncherPage }` (:1400).
```cpp
// Light > Actors (generated kit page rooted as panel_avatar_light_response). The panel's own
// postBuild binds the list and every per-avatar row; this binds the Director-only rows.
void LLFloaterDirector::bindLightActorsPage(LLView* root)
{
    if (LLButton* b = root->findChild<LLButton>("light_response_popout", true))
        b->setCommitCallback([this](LLUICtrl*, const LLSD&)
        {
            const LLUUID actor = firstSelectedCastId();
            LLFloaterReg::showInstance("avatar_light_response", LLSD(actor.notNull() ? actor : gAgentID));
        });
    if (LLButton* b = root->findChild<LLButton>("am_reset_CineLightRigActorMaskBias", true))
        b->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            if (LLControlVariable* c = gSavedSettings.getControl("CineLightRigActorMaskBias").get())
                c->resetToDefault(true);
        });
    if (LLButton* b = root->findChild<LLButton>("am_test_trigger", true))
    {
        const LLHandle<LLView> handle = b->getHandle();
        auto sync = [handle]()
        {
            if (LLButton* btn = dynamic_cast<LLButton*>(handle.get()))
                btn->setLabel(gSavedSettings.getU32("CineLightRigActorMaskTest") == 1 ? "Cancel test" : "Run test");
        };
        b->setCommitCallback([](LLUICtrl*, const LLSD&)
        {
            ALKitHistory::ScopedSuspend suspend;   // a command, never an undo step
            const U32 now = gSavedSettings.getU32("CineLightRigActorMaskTest");
            gSavedSettings.setU32("CineLightRigActorMaskTest", now == 1 ? 2 : 1);   // 1 arm, 2 cancel; self-resets to 0
        });
        mActorMaskTestWatch = ALKitSettingWatch::instance().watch("CineLightRigActorMaskTest",
            [sync](const LLSD&, const LLSD&) { sync(); });   // event-driven label, no per-frame read
        sync();
    }
}
```
- Delete the light_response_popout block in `bindCameraPage` (:1970-1974); no other page has that widget.
- Delete the v2-only `card_actor_mask_open` block in `bindLauncherPage` (:2190-2197), which is dead once the page converts. This follows Image/Roto/Tron, whose blocks are gone.
- **Keep** the v1 `addLauncherCard("actor_mask", …)` in `postBuild` (:1069-1075).

### 2.6 Rejected alternatives
- **An `ALKitBindingRegistry` adapter per widget** (read first, write all): this would duplicate the panel's selection, list, enable rules and save logic outside the panel. Risk 6 in the audit (three hosts) makes a duplicate the riskier choice, and the registry has no users to copy from.
- **The audit's Option 2,** one custom card hosting the whole legacy panel: it keeps the legacy look, fails the mockup visual gate, and leaves the per-avatar rows invisible to kit search and help.

## 3. Legacy panel, standalone floater and v1

- **Keep `ALPanelAvatarLightResponse` and `panel_avatar_light_response.xml`.** The standalone floater (`floater_avatar_light_response.xml`, `alfloateravatarlightresponse.cpp:15-34`, opened from `llfloaterdirector.cpp:823, 1317` and the popout) and the v1 Director tab (`floater_director.xml:1413-1428`) keep using them unchanged.
- WP1's panel edits (2.5 a/b) are additive and host-agnostic. `ALKitRow::setControlVisible` falls back to `setVisible` when there is no kit row (`alkitrow.cpp:675-678`).
- The kit page replaces only `panel_dpage_light_actors.xml` (the v2 Director).
- The legacy XML is now also a generator input. Renaming or removing any of its widgets changes the generated page header (`source_bundle_sha`, `director_kit_gen.py:92-103`), and the generator gate then requires a regeneration. Say this in a comment at the top of the XML.
- The static clipboard `mClip` (:68-70) stays shared between instances. That is intended: copy in the floater, paste in the Director.

## 4. Generator and gate changes (exact)

| file | change |
|---|---|
| `scripts/code_tools/director_kit/kit_pages.json` | Add the `p0_pages` entry below. Add `parity_allow` entries (exact names, no globs): `actors_flow_scroll`, `actors_flow_grid`, `actors_light_response_card`, `actors_light_response_card_title`, `director_light_response_panel` (each "v1 layout container"); `card_actor_mask`, `card_actor_mask_title`, `card_actor_mask_summary`, `card_actor_mask_open` (each "retired Director launcher replaced by the inline Actor only card"); `txt_sil_header` ("static heading replaced by the Silhouette card title"). Add any further name only if the parity gate reports it, and only with a specific reason. |
| `scripts/code_tools/director_kit_gen.py` | (1) `validate_data` (:227-228): row pin +1 (2065 → 2066 as of 10/15; apply as +1 to whatever is there), with a comment line "2066: + Actors Remember-between-sessions toggle; Isolation test combo replaced by one trigger button (10/15)". (2) `expected` (:1174-1186): add `"light_group/actors_tab": "kit (P0)"`. (3) The Counter at :1191 and in `selftest` :1258: `{"kit (P0)": 22, "pure kit (new)": 1, "legacy": 13}`, with a pin comment "10/15 Light Actors: kit 21 -> 22, legacy 14 -> 13". (4) `selftest` :1295: "13 serialized legacy pages", `legacy_count == 13`. (5) In `kit_page` widgets preset (:800-808), `combo` is required and `apply`, `save` and `delete` are optional, copied in the order combo, apply, save, delete. The Light Rig output must stay byte-identical (it has no `apply`). |
| `scripts/code_tools/xui_layout_check.py` | `LEGACY_PAGE_COUNT = 13` (:1681), with the comment "Light Actors: 14 -> 13". `gate_policy.json` `xui_legacy_v1_findings` has no `light_actors` key, so nothing to remove. |
| `scripts/code_tools/director_parity.py` | Add `"panel_avatar_light_response": {"ALPanelAvatarLightResponse"}` to `root_owner` (:724-727). **Required**: page owners are ALPanelAvatarLightResponse, ALFloaterLightBox and LLFloaterDirector (`director_page_sources.py:103`). Without the filter, every `getChild` in `llfloaterdirector.cpp` and `alfloaterlightbox.cpp` is checked against this one page (check 3). With it, check 3 proves the panel's 24 lookups resolve to compatible tags. |
| `scripts/code_tools/director_page_sources.py` | `ADAPTERS["actors_tab"]` (:297): "ALActorLightKitAdapter" (no such class) → "ALPanelAvatarLightResponse". `KIT_ACTION_OVERRIDES` (:134) stays. Regenerate `page_sources.json`/`.md` and `pages/actors_tab.md`. |
| `scripts/code_tools/director_functional_manifest.py` | `LAUNCHER_HOSTS["actors_tab"]` (:56) **stays**: v1 still reaches Actor Mask through the launcher, and v2 rows route by tab because `GENERATED_KIT_PAGES` wins in `host_for` (:75-77). The `am_test_combo` SKIP (:929-940) stays for v1; add the same SKIP for `("v2","actors_tab","am_test_trigger")` with the same reason. Add the per-avatar store oracles (WP3). |
| `indra/newview/aluiperfselftest.cpp` | `functionalLauncherName` (:2421) **stays** (v1). Replace the `chk_bypass` fixture (:2570-2590) with the `light_response_fixture` prerequisite (WP3). |
| `scripts/code_tools/director_kit/check_kit_pages.py` | Not a gate (286 errors before this work). Optionally accept `apply` in `widgets` (:206-211). Do not use it as a pass criterion. |
| regenerated outputs | Run `director_kit_gen.py` (XUI pages, `floater_director_v2.xml`, `director_kit_index.xml`, `gen_report.txt`), then `director_page_sources.py`, then `director_wiring_table.py` (`doc/director_kit/wiring/wiring_table.csv`), then `director_functional_manifest.py`. Never hand-edit generated files. |

The `kit_pages.json` entry:
```json
"light_group/actors_tab": {
 "v1_sources": ["panel_avatar_light_response.xml"],
 "root_class": "panel_avatar_light_response", "root_name": "actors_tab_logic",
 "preset": {"mode": "widgets", "combo": "combo_preset", "apply": "btn_apply_preset"},
 "selector_settings": {}, "card_enables": {},
 "row_overrides": {
  "target_list": {"no_widget_history": true, "row_height": 140},
  "chk_only_adjusted": {"no_widget_history": true},
  "sld_diffuse": {"no_widget_history": true}, "sld_tame": {"no_widget_history": true},
  "sld_brightness": {"no_widget_history": true}, "sld_glow": {"no_widget_history": true},
  "chk_bypass": {"no_widget_history": true}, "sld_sil_amount": {"no_widget_history": true},
  "combo_sil_fill": {"no_widget_history": true}, "swatch_sil_fill": {"no_widget_history": true},
  "chk_sil_haze": {"no_widget_history": true}, "chk_sil_keep_rim": {"no_widget_history": true}
 },
 "add_rows": [
  {"card": "silhouette", "after": "chk_sil_keep_rim", "row": {"k": "readout", "w": "txt_sil_note", "l": "Note", "show_if": "sil_note", "h": "Shown only while the silhouette is getting ready, or when Keep rim needs Rig Rim."}},
  {"card": "actors_diagnostics", "after": "txt_status", "row": {"k": "readout", "w": "txt_budget", "l": "Frame rate", "show_if": "over_budget", "h": "Shown when changed avatars exceed your avatar impostor limit. They then draw at full detail, which costs frame rate."}}
 ],
 "label_names": {}
}
```

## 5. Value-wiring validation table (implementer fills the last two columns, per control)

Rule (memory: value-wiring validation): for every control, verify the field, type, range against the clamp, combo values against the enum, handler target and read-back in the code. Then prove it at runtime. "Expected" below is from code reading. Use "panel" for `alpanelavatarlightresponse.cpp`, "model" for `alavatarlightresponsemodel.{h,cpp}` and "store" for `alavatarlightresponse.cpp`.

| # | widget | writes | type | UI range ↔ store, clamp | combo values | handler | read-back | runtime oracle | code-verified | runtime result |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | chk_enabled (master) | `AvatarLightResponseEnabled` | Boolean | - | - | kit master; hook store :94-104 | setting | toggle → setting false/true | | |
| 2 | chk_persist | `AvatarLightResponsePersist` | Boolean | - | - | setting; hook store :106-119 | setting | toggle → setting; in-world: file written on ON | | |
| 3 | target_list | panel selection | multi list | - | - | :113-114, :806-813 | `selectedKeys` :621 | select fixture → sliders enabled, show the fixture's values | | |
| 4 | chk_only_adjusted | panel filter | bool | - | - | :115-119 | :445-457 | ON hides unchanged rows | | |
| 5 | btn_refresh | idle flags | - | - | - | :120-126 | :259-266 | commit; list rebuilt next idle | | |
| 6 | light_response_popout | opens floater | - | - | - | binder | - | floater visible, key = cast id or `gAgentID` | | |
| 7 | sld_diffuse | `Params::mDiffuse` | F32 | 0..100 ↔ 0..1, model.cpp:106 | - | :826 | :703 | 50 → 0.5 ±1e-4, other fields unchanged | | |
| 8 | sld_tame | `mTame` | F32 | 0..100 ↔ 0..1, :107 | - | :827 | :704 | 25 → 0.25 | | |
| 9 | sld_brightness | `mExposureEV` | F32 | -2..1 ↔ same, :108 | - | :828 | :705 | -1.0 → -1.0 | | |
| 10 | txt_brightness_ev | display | - | - | - | :755-761 | - | after #9: "-1.00 EV" | | |
| 11 | sld_glow | `mGlow` | F32 | 0..200 ↔ 0..2, :109 | - | :829 | :706 | 150 → 1.5 | | |
| 12 | chk_bypass | `mBypass` | bool | - | - | :928-941 | :707 | ON → true | | |
| 13 | sld_sil_amount | `mSilAmount` | F32 | 0..100 ↔ 0..1, :110 | - | :830 | :708 | 40 → 0.4 | | |
| 14 | combo_sil_fill | `mSilFillSrgb` | vec3 | each 0..1, :113 | 0-5 = `EFillPreset` model.h:105-114 | :878-903 | :712-713 | item 2 → `fillPresetSrgb(FILL_WHITE)`; item 5 → no write | | |
| 15 | swatch_sil_fill | `mSilFillSrgb` | vec3 | each 0..1, :113 | - | :848-876 | :709-711 | (0.2,0.4,0.6) stored; combo reads 5 | | |
| 16 | chk_sil_haze | `mSilHaze` | bool | default true | - | :905-926 | :714 | OFF → false | | |
| 17 | chk_sil_keep_rim | `mSilKeepRim` | bool | - | - | :905-926 | :715 | ON → true | | |
| 18 | txt_sil_note | visibility | - | - | - | :763-804 | - | shown iff (any silhouette and no emissive buffer) or (Keep rim and Rig Rim off) | | |
| 19 | combo_preset (bar) | index only | S32 | 0..8 | = `EPreset` model.h:158-170 | read by :943-962 | none | - | | |
| 20 | btn_apply_preset (bar) | all fields | - | - | - | :943-962 → model.cpp:612-652 | :692-753 | preset 1 → params == `applyPreset(Params(),1)` | | |
| 21 | btn_reset_selected | identity entry | - | - | - | :964-973 | - | entry present and `isIdentity` | | |
| 22 | btn_inherit_selected | removes entry | - | - | - | :975-984 → store :287-308 | enable rule :736-752 | resident fixture → disabled (not run, reason); in-world: clone → entry removed | | |
| 23 | btn_copy | `mClip` | - | - | - | :1004-1016 | - | Paste enabled after | | |
| 24 | btn_paste | all fields | - | - | - | :1018-1030 | - | fixture B params == fixture A params | | |
| 25 | btn_reset_all | `clearAll` | - | - | - | :986-1002 (modal) | - | hazard: not run at login (reason: modal + clears all) | | |
| 26 | am_disable_check | `CineLightRigActorMaskDisable` | Boolean | - | - | setting | setting | toggle → setting | | |
| 27 | am_debug_combo | `CineLightRigActorMaskDebug` | U32 | 0..2, no clamp | Off 0, Overlay 1, Mask only 2 | setting | setting | item 1 → 1 | | |
| 28 | am_bias_slider | `CineLightRigActorMaskBias` | F32 | -64..0, pipeline.cpp:25703 | - | setting | setting | -7.5 → -7.5 | | |
| 29 | am_reset_CineLightRigActorMaskBias | reset to -8 | F32 | - | - | binder | setting | -20 then click → -8 | | |
| 30 | am_test_trigger | `CineLightRigActorMaskTest` | U32 | command 1/2 | - | binder | label via setting watch | SKIP at login (reason as `am_test_combo`); static: label follows setting | | |
| 31 | open_lightbox_actormask | action | - | - | - | `lightbox:actor_mask_settings` | - | Lightbox open on that panel | | |
| 32 | chk_debug_tint | `AvatarLightResponseDebugTint` | Boolean | - | - | setting | setting | toggle → setting | | |
| 33 | txt_status | display | - | - | - | :572-600 | - | "adjusted" count +1 after the fixture write | | |
| 34 | txt_budget | visibility | - | - | - | :602-618 | - | hidden at login (no live avatars); visible state only | | |

## 6. Work packages and tests

Process (memory and PLAN step 6):
- Codex implements and Opus reviews each WP.
- Run `cxs\zs_check.ps1` after every Codex round.
- A Codex review must find no major issues before the build.
- Checkpoint commit and exe backup, then **one** `cxs\fresh_build.ps1` at the end, only when the viewer is closed.
- Never loosen a gate: every pin changes by exactly the amount stated here.

**WP1: C++ (panel and binder).** Files: `alpanelavatarlightresponse.{h,cpp}` (2.5 a, b) and `llfloaterdirector.{h,cpp}` (2.5 c). About 80 lines. Gates: zs_check, `ui_draw_lint --changed` (no new per-frame lookups; the binder label is event-driven).

**WP2: page data and generator.** Files:
- mockup JSON edits (1.3);
- the `kit_pages.json` entry and parity_allow;
- `director_kit_gen.py` (pins and `apply`);
- `director_page_sources.py` ADAPTERS;
- regenerate (Section 4 order).

WP2 does not depend on WP1 to generate. The page still works without WP1, except that the 3 binder rows are unwired and the 2 conditional notes do not re-lay out.

**WP3: gates, manifest and harness.** Files: `xui_layout_check.py` pin, `director_parity.py` root_owner, `director_functional_manifest.py`, `aluiperfselftest.cpp`, plus a regenerated wiring table and manifest. Harness changes:
- **New `light_response_fixture` prerequisite** for every v2 `actors_tab` per-avatar entry (#3-#24):
  - make one or two fixture UUIDs;
  - find the panel as `dynamic_cast<ALPanelAvatarLightResponse*>(mFunctionalPageRoot)` (v2 root) or `findChild<ALPanelAvatarLightResponse>` (v1);
  - call the public `focusTarget(id)` (:177-191). It pins the row so the 1 Hz rebuild cannot drop it, which the current hand-added row (:2570-2590) does not guarantee;
  - for two selected rows, add `setSelectedByValue(a, true)` + `onCommit`;
  - cleanup: `ALAvatarLightResponse::instance().inherit(id, false)` for each fixture.
- **New `light_response_store` probe.** After the exercise, read `findEntry(fixture)->mParams`. Compare the named field to `expect.value`, and compare every other field to the pre-state.
- **Negative control:** one synthetic entry naming the wrong field (diffuse → `mTame`) must report FAIL. The run fails if it passes. This addresses "Codex relaxes gates".
- **In-world:** run store oracles only when `AvatarLightResponsePersist` is OFF; otherwise SKIP with the reason "would write the fixture into the account file". At the login screen, saves are already refused (`saveToFile` with a null `gAgentID`).

**Tests (all must pass; report PASS/FAIL/not run with reasons per PLAN step 6):**

Generator and gates:
- **T1.** `director_kit_gen.py --selftest` and `--check`. The Light Rig page must be unchanged apart from the header hash.
- **T2.** `kit_lint.py --all`: 0 errors and no new `help-audit-term` warnings on this page.
- **T3.** `director_parity.py --groups light_group`: 0 findings. The allow list must be exactly Section 4's.
- **T4.** `xui_layout_check.py --selftest` (pin 13) and `--changed`: 0 findings.
- **T5.** `director_page_sources.py --check`.
- **T6.** `ui_draw_lint --changed` and zs_check. Together, T1-T6 are the prebuild guard gates.

Static checks:
- **T7.** `git diff` touches no generated file by hand.
- **T8.** A static script over `panel_dpage_light_actors.xml` must show all of the following:
  - each of the 24 panel lookups appears exactly once, with tag `scroll_list` / `kit_switch` / `slider` / `color_swatch` / `combo_box` / `text` / `button` as the `getChild` type requires;
  - the 12 per-avatar rows have `widget_history="false"`, and none has `default_value`, `reset_settings` or `setting`;
  - no `who_*` attributes and no `who_picker`;
  - `preset_mode="widgets"`, with a `kit_preset_host` holding `combo_preset` then `btn_apply_preset`;
  - `txt_sil_note` and `txt_budget` rows have `visible="false"`;
  - no `am_test_combo`; `am_test_trigger` has no `control_name`.

Runtime:
- **T9 (login run).** Every oracle in Section 5, plus:
  - multi-select: two fixtures at 0.2 and 0.7 show 20; setting 50 writes 0.5 to both;
  - Ctrl+Z after a slider edit changes nothing in the store;
  - "Reset page" leaves every store entry unchanged.
- **T10 (in-world as 5kvideoeye, auto-login only, server actions blocked).**
  - With Persist ON: the file changes on mouse-up, not during a drag, and 0.5 s after an arrow-key edit.
  - Real multi-avatar select works.
  - Inherit works on an animesh or clone.
  - The budget note appears past the impostor limit.
  - Run test then Cancel test changes the label and then returns it to "Run test".
- **T11 (regression).** The standalone floater and the v1 Director Actors tab each go through a click-every-control pass with values checked.
- **T12 (visual hard gate).** Compare a screenshot of the Actors page with the mockup (`doc/director_kit/mockup/Director.dc.html`), at narrow and wide widths, with More closed and open.

## 7. Open questions (each has a recommended default; implement the default unless the user says otherwise)

- **Q1. Mixed indicator.** The kit cannot show "values differ" (2.3).
  - Default: **ship without it**, keeping the legacy show-first/write-all behaviour.
  - Option: one extra readout row, e.g. "3 selected; showing You; some differ", updated by the panel. This would be +1 row and +1 pin.
- **Q2. "Reset page" and the card reset also turn "Remember between sessions" off,** because it is a setting row with default Off. The saved file is kept.
  - Default: **accept.** No kit change; the reset already asks first.
  - Option: a generic row flag to exclude a row from page and card reset (kit change in `alkitpage.cpp:1499` and `alkitcard.cpp:333`).
- **Q3. Undo.** Per-avatar edits on this page are not undoable, the same as the legacy floater.
  - Default: **accept.**
  - Option (later): record `ALKitHistory::recordCustom` entries holding the before/after `Params` of each edited key.
