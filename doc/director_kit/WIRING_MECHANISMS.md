# Director wiring study: mechanisms

## Scope and evidence baseline

This is a read-only source/XUI study of three configurations: original v1 at `9c25f75491a`, current v1 at checkout `4f6b5f3980fe1859633fc71d27bfef0dd64b9e13` with `DirectorUIV2=false`, and v2 at that checkout with `DirectorUIV2=true`. The requested runtime build commit is `d812416ca65`; the commits after it through the checkout are documentation/user-log commits, so the executable sources studied are the requested build sources. No build or runtime instrumentation was performed. Original references are written as `9c25f75491a:path:line`; unqualified paths refer to the checkout.

Terminology used below:

- **setting-bound** means XUI/`setControlName()` connects a widget directly to an `LLControlVariable`;
- **owner-bound** means a panel/floater instance finds a widget and installs a C++ callback or keeps a member pointer;
- **registry-bound** means XUI names a callback that must exist in a callback registry while the XUI is constructed;
- **floater-owned** means the callback is installed by `LLFloaterDirector`, rather than by the leaf panel class.

## A. Original v1 wiring, by mechanism

### A1. XUI `control_name` -> `LLControlVariable`

This is the shortest working chain. A control constructed from XUI receives a control name; `LLUICtrl`/the compound widget binds it to the corresponding control variable. User commit writes the control and later setting changes feed the widget. It does not require a Director callback. For example, Weather's master checkbox names `AlchemyWeatherEnabled` directly (`9c25f75491a:indra/newview/skins/default/xui/en/panel_weather_settings.xml:18-21`), rain names `AlchemyWeatherRainEnabled` (`:39-42`), and its sliders name their individual settings (`:44-78`). The Cine Light Rig likewise binds Manual directly (`9c25f75491a:indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:109`) and Rig Rim enable directly (`:949`). In the checkbox implementation, `setControlName()` delegates to the internal toggle button (`indra/llui/llcheckboxctrl.cpp:280-283`), and checkbox commit calls `setControlValue(getValue())` before outward callbacks (`indra/llui/llcheckboxctrl.cpp:159-166`).

Consequently, geometry-only changes to `floater_director.xml` or a shared panel XUI do not by themselves disconnect a retained `control_name`. Current v1 still takes the non-v2 branch at `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:294-304`) and constructs the same named owner panels from `floater_director.xml`.

### A2. XUI `commit_callback.function` -> callback registry

The XUI callback name is resolved during child construction, so registration timing and registrar scope matter. Weather's reset buttons name `Weather.ResetControl` (for example `9c25f75491a:indra/newview/skins/default/xui/en/panel_weather_settings.xml:46-49`). `ALPanelWeatherSettings` is selected through `LLPanelInjector` (`9c25f75491a:indra/newview/alpanelweathersettings.cpp:30-31`); its constructor registers `Weather.ResetControl` in the **default/global** registrar before its children are built (`:37-62`). That callback is therefore not tied to the v1 Director instance and remains resolvable wherever the injected panel is instantiated.

By contrast, the Director context-menu callbacks use a local `CommitCallbackRegistry::ScopedRegistrar` inside `LLFloaterDirector::postBuild()` (`9c25f75491a:indra/newview/llfloaterdirector.cpp:381-429`). The lambdas capture that particular `LLFloaterDirector*`, and the menus are constructed while the registrar is alive. This is construction-time lexical scoping, not a registry permanently attached to every later page. A lazily constructed v2 copy cannot acquire those registrations merely because its widget names resemble v1; v2 must construct the relevant menu under its own scoped registrar or install a direct callback. Current v2 does create the Director menus under its own registrar in `bindChrome()` (`indra/newview/llfloaterdirector.cpp:867-903`), so the chrome/context-menu case is explicitly covered.

### A3. Owner `postBuild()`: `getChild`/`findChild` plus direct callbacks

The original Director floater owns the controls that are literal descendants of `floater_director.xml`: transport (`9c25f75491a:indra/newview/llfloaterdirector.cpp:284-309`), tabs (`:312-333`), legend/cast (`:345-375`), group controls (`:430-449`), actor-style controls (`:485-507`), animation controls (`:532-548`), subject/gaze/launcher buttons (`:575-610`), and launcher cards (`:870-874`). These callbacks exist only after that floater instance's `postBuild()` finds those exact descendants.

Injected leaf panels own their own descendants. The v1 Director XUI instantiates 22 registered panel classes, including Actor Mover, animation preview, Cine Cam Params, Cine Light Rig, DoF, Ghost Studio, Path Editor, Temporal Capture and Weather. Representative chains are:

- `ALPanelCineLightRig` is registered with `LLPanelInjector` (`9c25f75491a:indra/newview/alpanelcinelightrig.cpp:48-49`); its `postBuild()` starts at `:713`, caches/fills the rig widgets, and installs callbacks for the anchor, setup/preset controls, aim/reset buttons, Easy controls, fixture controls, manual cone/shaft/hero controls, and actor-only master (`:833-1007`). This same owner is what makes the standalone Cine Light Rig floater work.
- `ALPanelWeatherSettings` owns group switching, strike/reset, preset save/delete and preset apply (`9c25f75491a:indra/newview/alpanelweathersettings.cpp:130-150`). Its ordinary setting widgets remain `control_name`-bound; the owner callback is required only for the higher-level actions/group visibility.
- Standalone floaters bind their own XUI instances: Lightbox presets in `ALFloaterLightBox::postBuild()` (`9c25f75491a:indra/newview/alfloaterlightbox.cpp:151-194`), Cine Light Cues controls in `ALFloaterCineLightCues::postBuild()` (`9c25f75491a:indra/newview/alfloatercinelightcues.cpp:56-117`), and Gaze Cues in `ALFloaterGazeCues::postBuild()` (`9c25f75491a:indra/newview/alfloatergazecues.cpp:107-184`). Their callbacks do not migrate to copied controls in another floater; a v2 launcher that opens the original floater is safe, while copied floater-owned controls need a v2 binder.

### A4. Per-frame/idle refresh through cached member pointers

Commit wiring is only half of v1. Owners cache widget pointers in `postBuild()` and later mirror model/runtime state back into them. Original `LLFloaterDirector::draw()` calls the cast, group, transport, move/path, actor-style, animation, gaze, subject, prism, launcher and status refreshers every frame (usually internally gated) (`9c25f75491a:indra/newview/llfloaterdirector.cpp:886-903`). Cine Light Cues refreshes list names, rows and status from `draw()` (`9c25f75491a:indra/newview/alfloatercinelightcues.cpp:387-393`); Gaze Cues does the equivalent (`9c25f75491a:indra/newview/alfloatergazecues.cpp:187-193`); Cine Light Rig's large refresh loop begins at `9c25f75491a:indra/newview/alpanelcinelightrig.cpp:2844`; Weather's draw refreshes deferred availability (`9c25f75491a:indra/newview/alpanelweathersettings.cpp:153-158`). If a copied widget never becomes the pointer cached by the running owner, it may accept a click yet be immediately stale, or a refresher may continue updating the v1 copy instead.

### A5. Injector-owned panels versus floater-owned leaf markup

`LLPanelInjector<T>`/`LLRegisterPanelClass<T>` changes what `class="..."` constructs. The subclass constructor runs before its XML children are created and its virtual `postBuild()` runs after them; this holds wherever the panel XML is instantiated, including inside a v2 lazy page. Weather proves the pattern: injector (`9c25f75491a:indra/newview/alpanelweathersettings.cpp:30-31`), constructor-time registry registration (`:58-62`), then owner `postBuild()` (`:130-150`). Cine Light Rig follows the same pattern (`9c25f75491a:indra/newview/alpanelcinelightrig.cpp:48-49,713-1007`).

Plain panels and controls whose behavior is installed only by `LLFloaterDirector::postBuild()` are different. Copying their XUI into another file creates the widgets but does not run the v1 floater's original `getChild()` statements against the copy. Those controls work in v2 only if (a) they are independently `control_name`-bound, (b) their XUI callback resolves globally, or (c) `LLFloaterDirector::bindPage()` explicitly binds the lazy page instance.

## B. How v2 instantiates the mechanisms

### B1. Lazy construction and binding order

`floater_director_v2.xml` contains `kit_lazy_page` placeholders, including Rig, Rim and Weather (`indra/newview/skins/default/xui/en/floater_director_v2.xml:72-76,91`). `LLFloaterDirector::postBuild()` detects `director_shell` and returns through `postBuildV2()` instead of running the v1 body (`indra/newview/llfloaterdirector.cpp:294-303`). `postBuildV2()` first binds chrome, then subscribes to the shell's page-built signal, and only then marks the shell ready (`indra/newview/llfloaterdirector.cpp:752-770`). This ordering is intentional: the first visible page is not built until `ALKitShell::setReady()` (`indra/newview/alkitshell.cpp:81-96`).

On reveal, `ALKitLazyPage::ensureBuilt()` calls `createFromFile<LLPanel>(page_file, this, LLDefaultChildRegistry)` (`indra/newview/alkitlazypage.cpp:28-48`). Normal XUI construction therefore runs constructors and `postBuild()` bottom-up for every registered class in the generated page. Only after the complete page exists does the shell emit `pageBuilt`, which calls `LLFloaterDirector::bindPage(page, panel)` (`indra/newview/llfloaterdirector.cpp:756-763`; `indra/newview/alkitlazypage.cpp:44-50`).

### B2. Kit rows: direct setting path, code path, signals, mirrors and guards

A generated setting row contains two independent pieces: row metadata such as `bind="setting" setting="CineLightRigKeyEV"`, and an actual child widget with `control_name="CineLightRigKeyEV"` (`indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:241-243`). The **user write** follows the ordinary widget `control_name` chain. `ALKitRow`'s `ALKitSettingBinding` is the reset/default/change-observation companion: it resolves the setting from `gSavedSettings` (`indra/newview/alkitbinding.cpp:49-66`) and watches its signal (`:68-71`). `ALKitRow::resolveBinding()` chooses setting, settings-list, vector, owner-registered code binding, or widget-default binding (`indra/newview/alkitrow.cpp:520-545`); the resulting change callback updates amber/reset/value state (`:503-517`).

For `bind="code"`, the owner panel still supplies behavior through its normal named-widget callbacks. Because the generated Rig page root is `class="panel_cine_light_rig"`, `ALPanelCineLightRig::postBuild()` finds the generated children (`indra/newview/alpanelcinelightrig.cpp:640-690`) and installs the same callbacks (`:747-920`). The kit row adds history observation for code-bound controls only when the row is neither a button/readout nor a mirror (`indra/newview/alkitrow.cpp:163-180`); it does not replace the owner's handler.

`mirror_setting` is a two-way index bridge rather than a normal `control_name`. User combo commit writes its selected index unless `mSyncingMirror` is true (`indra/newview/alkitrow.cpp:197-212`). Setting changes select the combo index and deliberately call `onCommit()` so the owner repopulates dependent controls, but do so under `ALKitHistory::ScopedSuspend` and the same guard (`:214-239`). Thus passive setting sync invokes the owner's logic without writing the mirror back or adding an undo step. The Rig Easy-mode adapter has the analogous explicit origin split: user/search/passive origins are mapped in the panel (`indra/newview/alpanelcinelightrig.cpp:921-958`), while `ALKitPage` suppresses recursive mode-switch updates with `mUpdatingModeSwitch` (`indra/newview/alkitpage.cpp:476-503,761-791`).

### B3. Selector cards / option tabs

The Rig Lights selector declares `sel_setting="DirectorV2SelectedLight"` and four options with values 0-3 (`indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:236-303`; subsequent options continue in the same card). `DirectorV2SelectedLight` is an S32 setting (`indra/newview/app_settings/settings.xml:8747-8748`). During card build, option values are normalized to the selector setting's type (`indra/newview/alkitcard.cpp:38-57,158-164`). For up to six options the card creates segment buttons; their commit lambda writes the selected option value to `gSavedSettings` (`indra/newview/alkitcard.cpp:426-447`). The setting signal calls `syncSelector()` and requests layout (`:475-484`). `syncSelector()` shows exactly the matching `ALKitOption`, selects its option-level enable switch, and updates the segment state/colours (`:523-553`). The Rim per-light selector uses the same shared setting (`indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:68`). The map also observes the setting (`indra/newview/allightrigmap.cpp:224-229`) and writes it from map selection/drag (`:592-594,633-643`). The Fixture combo mirrors the same index (`panel_dpage_light_cine_light_rig.xml:577-581`; `alkitrow.cpp:197-239`).

There is no function named `selectOption()` in this checkout. The intended click chain is exactly: programmatic `LLButton` -> lambda at `alkitcard.cpp:433-444` -> `DirectorV2SelectedLight` write -> control signal at `:477-482` -> `syncSelector()` at `:523-553`.

### B4. Provider preset strip -> Rig Rim provider

Rim declares `preset_mode="provider" preset_provider="rig_rim"` (`indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:4`). `ALPanelRigRim` is an injected owner; its constructor registers the provider before children are constructed (`indra/newview/alpanelrigrim.cpp:68-89`). `ALKitPage::postBuild()` creates a provider strip in the page bar (`indra/newview/alkitpage.cpp:300-310`). The strip resolves its provider and attaches its combo commit in `postBuild()` (`indra/newview/alkitpresets.cpp:213-232`). Selection calls `mProvider->apply(id)` under one history gesture (`:308-340`). `RigRimPresetProvider::apply()` dispatches built-ins to `ALRigRimController::applyPresetByName()` or user snapshots to the store (`indra/newview/alpanelrigrim.cpp:32-61`); the controller writes the preset's settings (`indra/newview/alrigrimcontroller.cpp:283-312,420-430`).

The older embedded/standalone Cine Rig panel has a separate combo path: `ALRigRimController::attach()` finds `cine_rig_rim_preset`, fills it, and installs a direct callback (`indra/newview/alrigrimcontroller.cpp:328-390`). The v2 Rim provider strip does not use that combo; its correctness depends on the provider chain above.

### B5. Page master switches

Rig and Rim declare `master_widget` plus `master_setting` (`panel_dpage_light_cine_light_rig.xml:4`; `panel_dpage_light_rim.xml:4`). `ALKitPage::postBuild()` creates an `ALKitSwitch`, calls `setControlName(master_setting, this)`, adds it to the bar, and observes flips for relayout (`indra/newview/alkitpage.cpp:452-474`). The direct click/write semantics are inherited from `LLCheckBoxCtrl`: its internal button toggles before commit (`indra/llui/llbutton.cpp:530-568`), then checkbox commit writes the bound control (`indra/llui/llcheckboxctrl.cpp:159-166`). The off banner's “Turn on” path is more explicit: `writeBoundValue(true)`, `setValue(true)`, then `onCommit()` (`indra/newview/alkitpage.cpp:523-533`; `indra/newview/alkitswitch.cpp:97-113`). `updateMaster()` only controls the banner/dimming/state text and does not write the setting (`indra/newview/alkitpage.cpp:912-921`).

### B6. Hosted legacy pages: which owners actually run

The generated Weather page is an `ALKitPage legacy="true"` containing a child `class="panel_weather_settings" filename="panel_weather_settings.xml"` (`indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:3-7`). XUI construction uses the registered `ALPanelWeatherSettings`, so its constructor and `postBuild()` do run; direct settings, group selection, strike/reset and preset handlers are therefore attached (`indra/newview/alpanelweathersettings.cpp:30-62,130-150`). `ALKitPage` later reparents the already-built child into `mLegacyHost` (`indra/newview/alkitpage.cpp:628-652`); reparenting does not reconstruct it or remove its callbacks. The same rule applies to every generated `class=` owner panel.

Floater-owned markup is handled separately and only where `bindPage()` lists a binder. The current table covers Move, Path, Actor Style, Animate, the camera-family pages, and launcher pages (`indra/newview/llfloaterdirector.cpp:941-983`); those binders search the emitted page root and attach callbacks to that copy (for example Move at `:985-1007`). A copied floater-owned control on an omitted page gets no v1 floater callback. The original v1 `postBuild()` never runs a second time against v2 copies. Scoped callback registries are likewise not “scoped to v1 pages”: global/default registrations are available everywhere, whereas a scoped registrar exists only during the construction performed inside its lexical scope. V2 explicitly recreates the Director context-menu scope in `bindChrome()` (`:867-903`).

## C. Reported failures: exact chain comparison

The conclusions below distinguish a proven source break from a runtime report whose complete static chain is present. “No static break” is not a claim that the user did not see the failure; it means this read-only study must not invent a missing callback that the source demonstrably installs. Those cases need one event/setting assertion in a runtime self-test to locate the boundary.

### C1. Lights Key / Fill / Back / BG option tabs — proven write-type break

Working intent: segment commit (`alkitcard.cpp:433-444`) should write `DirectorV2SelectedLight`, whose signal invokes `syncSelector()` (`:475-482,523-553`). The screenshot's white rectangle on Back is focus state, so it proves mouse-down/focus reached that button; Key remaining active proves the selector setting did not change.

The exact break is `gSavedSettings.setLLSD(mSelectorSetting, optionValue)` at `indra/newview/alkitcard.cpp:436-437` (and the combo variant at `:461-462`). `DirectorV2SelectedLight` is declared **S32** (`indra/newview/app_settings/settings.xml:8747-8748`). `LLControlGroup::setLLSD()` dispatches through `set<LLSD>` (`indra/llxml/llcontrol.cpp:735-738`), `get_control_type<LLSD>()` is `TYPE_LLSD` (`:1318-1321`), and the generic setter writes only if that type equals the declared control type; otherwise it logs “Invalid control” and does not write (`indra/llxml/llcontrol.h:267-278`). Normalizing the option's LLSD payload to an integer (`alkitcard.cpp:38-57,158-164`) does not alter the C++ template type used by `setLLSD`, so it does not cure the rejection.

The working alternate paths use the typed setter: the light map calls `gSavedSettings.setS32("DirectorV2SelectedLight", ...)` (`indra/newview/allightrigmap.cpp:592-594,640-642`). Current/original v1 has no generated Lights selector; its rig controls are directly owned by `ALPanelCineLightRig`, so this new defect is v2-only. Minimal correction: obtain the `LLControlVariable` and call `control->set(typedOptionValue(...))`, or dispatch to the correctly typed group setter; then add a click assertion for both selector cards. This affects both selector cards (Rig Lights and Rim per-light), eight segment buttons total.

### C2. Rim preset selection — chain present; observed failure is not explained by missing provider wiring

The provider is registered in the injected root constructor before the child `ALKitPage` builds (`alpanelrigrim.cpp:68-89`). The strip resolves `rig_rim` and attaches its combo callback (`alkitpresets.cpp:213-232`); `onSelect()` applies the selected id (`:308-340`); built-ins call `ALRigRimController::applyPresetByName()` (`alpanelrigrim.cpp:48-55`), which writes the actual settings (`alrigrimcontroller.cpp:283-312,420-430`). The applying guard correctly ignores synchronous tracked-setting notifications while the provider is writing (`alkitpresets.cpp:422-433`). No `setLLSD` type mismatch exists in this chain.

The standalone/v1 comparison uses another valid route: `ALRigRimController::attach()` installs a direct callback on `cine_rig_rim_preset` and calls the same `applyPresetByName()` (`alrigrimcontroller.cpp:362-388`). Thus both routes converge on the same proven provider implementation. The static v2 break is **not** “provider never registered” or “panel postBuild never ran.” A preset also deliberately does not write `CineRigRimEnabled` (the preset writes begin at `alrigrimcontroller.cpp:285` and omit the enable), so a disabled Rim master can make a successful preset visually inert. Minimal next diagnostic/fix: make the functional test choose a built-in and assert one sentinel such as `CineRigRimMasterGain` changed; if it does, surface “Rim light is off” rather than changing preset semantics. If it does not, instrument the combo commit and provider return. Scope: the one provider strip / 20 built-ins plus user presets.

### C3. Rim page master switch — chain present; no static write guard blocks it

The Rim page creates `cine_rig_rim_enable`, binds it to `CineRigRimEnabled`, and listens for flips (`alkitpage.cpp:452-474`). User mouse-up toggles the internal button before emitting commit (`llbutton.cpp:530-568`); virtual `ALKitSwitch::onCommit()` calls `LLCheckBoxCtrl::onCommit()` (`alkitswitch.cpp:97-101`), which writes the control (`llcheckboxctrl.cpp:159-166`). `ALKitSwitch::setControlName()` also connects the setting signal to its state notifier (`alkitswitch.cpp:83-95`). There is no passive/active guard in this path that suppresses a user write. The renderer reads the same setting (`indra/newview/alcinerigrim.cpp:112`).

The v1/standalone check box binds the identical setting in XUI (`panel_cine_light_rig.xml:949`). Therefore no missing owner callback is required in either path. Minimal next diagnostic/fix: assert `CineRigRimEnabled` changes on a real master-switch click, then separately assert the renderer gate observes it. If the first assertion fails, inspect the dynamically created switch's bound `LLControlVariable` and mouse-up delivery; if it passes, the defect is downstream, not kit binding. Scope: two programmatic page masters exist (Rig and Rim), but only Rim is reported.

### C4. “Sticky” switch/button colours — proven focus-state design, not a stuck setting

`ALKitSwitch::draw()` derives track and knob entirely from `isOn()` (`alkitswitch.cpp:38-55`), then draws a permanent amber outline whenever either the wrapper or its internal button has keyboard focus (`:56-64`). A clicked button normally retains focus; mouse leave clears hover in `LLButton`, but it does not clear focus (`indra/llui/llbutton.cpp:632-645`). The Bounce screenshot is consistent with exactly this code: value text says Off, the white off knob is on the left, and the amber two-circle/parallel-line focus outline is superimposed around it. There is no animated middle position in `ALKitSwitch`; knob x is exactly 9 or 25 (`alkitswitch.cpp:53-54`). The Back tab's white outline is likewise focus, independent of selector state.

This is a v2-only presentation defect because v1 uses stock check boxes. Minimal correction: use a distinct thin keyboard-focus treatment only for keyboard modality (or clear/suppress the internal button focus ring for pointer clicks), and draw one coherent switch state. Do not treat selected/hover/pressed/focus as interchangeable palette states. The generated pages contain 41 `kit_switch` instances plus two dynamic page masters; all can show the misleading retained-focus treatment.

### C5. World > Weather — injected owner is wired; visible rain failure is downstream

The hypothesis “the v2 copy never runs `ALPanelWeatherSettings::postBuild()`” is false for the checked-in page. The generated wrapper explicitly instantiates `class="panel_weather_settings"` (`panel_dpage_world_weather.xml:3-7`), the class is injected (`alpanelweathersettings.cpp:30-31`), constructor-time reset registration runs (`:37-62`), and its direct callbacks are installed (`:130-150`) before the built page is reparented into the legacy host (`alkitpage.cpp:628-652`). Its master checkbox is directly bound to `AlchemyWeatherEnabled` (`panel_weather_settings.xml:18-21`). This is the same shared panel and owner used by current/original v1 (`9c25f75491a:indra/newview/skins/default/xui/en/floater_director.xml:1661-1687`).

There are two evidence-based qualifications. First, the owner deliberately disables the whole controls panel when deferred rendering is unavailable (`alpanelweathersettings.cpp:193-220`); in that state the checkbox cannot be clicked in either Director. Second, `USER_BUGS.md` records the follow-up result that rain is broken in **both** v1 and v2 after the UI settings are on, which moves the no-visible-effect failure to the render path. The pipeline consumes `AlchemyWeatherEnabled` separately (`indra/newview/pipeline.cpp:18978`). Minimal correction is therefore not a v2 legacy binder: retain the shared panel, add a setting-commit assertion for its 54 `control_name` bindings, and diagnose the already-separated render regression. If a future runtime assertion shows the v2 checkbox alone does not change the setting while deferred rendering is on, the failure boundary is event delivery/reparenting, not absent `postBuild()`.

## D. Root-cause classes, scope and minimal correct fixes

### D1. Wrong typed settings API in kit selector commits — confirmed

**Class:** `ALKitCard` writes a declared S32 through `LLControlGroup::setLLSD`, which rejects it (`alkitcard.cpp:436-437,461-462`; `llcontrol.h:267-278`; `llcontrol.cpp:735-738,1318-1321`).

**Affected scope:** both generated selector cards, Rig Lights and Rim per-light; four segments each, eight visible option controls. It also affects any future selector card whose `sel_setting` is not declared as `TYPE_LLSD`.

**Minimal correct fix:** write through the resolved `LLControlVariable::set(typedOptionValue(...))`, or type-dispatch to `setS32`/`setBOOL`/etc. Keep the existing signal-driven `syncSelector()` as the single UI refresh path. Add functional assertions that clicking all eight segments changes the setting and visible option.

### D2. Pointer-focus is rendered as an amber “active” switch state — confirmed visual defect

**Class:** `ALKitSwitch::draw()` always paints the amber double-circle/line focus outline while focus remains (`alkitswitch.cpp:56-64`), even when the off-state track/knob is correctly painted (`:42-55`). Standard button focus similarly explains the white Back outline; it is not evidence of selection.

**Affected scope:** 41 generated `kit_switch` nodes plus two dynamic page masters (approximately 43 switches); the same semantic confusion affects programmatic segment buttons styled with strong selected/focus colours.

**Minimal correct fix:** make focus modality-aware and visually distinct from On/selected/pressed. Pointer mouse-up must leave only value-derived switch colour; keyboard focus may retain a thin neutral ring. Test Off+focused, On+focused, hover, pressed and disabled independently.

### D3. Invisible upstream gates produce “nothing happened” even when downstream values change — confirmed design gap, not a common commit bug

**Class:** presets intentionally do not enable Rim (`alrigrimcontroller.cpp:283-312`), and Weather disables its complete controls panel when deferred rendering is unavailable (`alpanelweathersettings.cpp:193-220`). The current UI does not make those suppressors sufficiently local/obvious. `USER_BUGS.md` already records the same UX class for projector masking.

**Affected scope:** one Rim preset strip (20 built-ins plus user presets) and the Weather panel (54 directly setting-bound controls, 42 XUI reset callbacks, plus owner actions). The underlying rain regression affects both directors and is outside this wiring study.

**Minimal correct fix:** do not silently change preset semantics. Show “Rim light is off” adjacent to the preset strip with a Turn on action; show the deferred/weather/rain master gate beside affected Weather controls. Functional tests must separately assert setting mutation and visible engine effect.

### D4. Floater-owned handlers require an explicit lazy-page binder — real architectural hazard, not the cause of Weather

**Class:** plain copied controls do not inherit `LLFloaterDirector::postBuild()` callbacks; only `bindPage()` supplies them (`llfloaterdirector.cpp:941-983`). Injected `class=` panels run their own `postBuild()` and must not be diagnosed as dead merely because they are legacy-hosted (`alkitlazypage.cpp:38-48`; `alkitpage.cpp:628-652`). Default registrars are global; scoped registrars exist only during their construction scope.

**Affected scope:** 31 legacy pages total. Nineteen generated legacy wrappers contain one or more injected owner classes; the remaining 12 classless wrappers contain about 113 directly declared interactive nodes. The current binder table covers the classless floater-owned pages except Scene Options, whose one interactive control is direct-setting-bound. Therefore the current static estimate of controls actually broken by a missing v2 floater binder among the five reported failures is **zero**; future additions remain exposed to this hazard.

**Minimal correct fix:** keep a generated/audited ownership manifest per page: each interactive control must be classified as direct setting, global registry, injected owner, Director binder, or launcher. Fail generation/tests for an unclassified floater-owned action. Do not add a redundant Weather binder.

### D5. No functional wiring gate — confirmed process root cause

**Class:** existing layout/parity checks establish names, geometry and static mappings but did not exercise user commit -> setting/callback -> model refresh. That allowed the typed selector writer to ship despite a visually correct page.

**Affected scope:** 35 generated pages (31 legacy, four kit-native), approximately 614 directly emitted interactive XML nodes, including 242 kit rows. Exact unique runtime controls are somewhat higher because kit pages create bars, segments, reset buttons, switches and preset controls programmatically.

**Minimal correct fix:** add the restore-after-test functional gate requested in `USER_BUGS.md`: for each enabled control, perform a real commit, assert its setting changed or a named owner callback probe fired, assert any linked UI mirrored the result, then restore state. Include explicit probes for both selector cards, both page masters, `rig_rim` preset application, one injected legacy owner (Weather), one floater-bound legacy page, and all programmatic kit-bar controls.

### D6. Reported Rim preset/master failures remain runtime-localization items

The source contains complete, type-correct chains for both (`alkitpresets.cpp:213-340`; `alpanelrigrim.cpp:32-89`; `alkitpage.cpp:452-474`; `llcheckboxctrl.cpp:159-166`). It would be unsupported to label them as missing-provider or missing-switch-write root causes. The first functional probe should capture, for one click each: widget value before/after, commit count, bound control pointer/name, setting before/after, provider return, and one renderer consumer value. That will separate input delivery, binding, engine gating and misleading focus colour without speculative edits.

## REPORT (root-cause classes; <=10 lines)

- **Confirmed:** `ALKitCard` uses `setLLSD` on S32 `DirectorV2SelectedLight`; 2 selector cards / 8 tabs cannot write. Fix with `LLControlVariable::set` or a typed setter.
- **Confirmed visual:** `ALKitSwitch` paints retained pointer focus like an amber active state; about 43 switches are misleading. Make focus modality-aware and value-independent.
- **Confirmed UX:** hidden Rim/deferred/Weather masters make successful subordinate changes look inert; 1 preset strip and about 106 Weather interactions need local gate status/actions.
- **Architectural hazard, not Weather's cause:** only floater-owned copied controls need `bindPage`; injected owners do run. Current reported failures attributable to a missing legacy binder: 0.
- **Runtime localization required:** Rim preset and Rim master have complete static chains; assert commit, setting/provider result and renderer consumer before changing code.
- **Process root:** no functional wiring gate covered about 614 emitted controls across 35 pages; add commit/callback/mirror assertions with state restoration.
