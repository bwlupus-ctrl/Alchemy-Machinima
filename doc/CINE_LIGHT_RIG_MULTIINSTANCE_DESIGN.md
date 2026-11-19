# Cine Light Rig — Multi-Instance (per-subject independent rigs) — Deep Design

**Status:** design pass complete. **No source modified.**
**Date:** 2026-08-16.
**Answers:** `doc/CINE_LIGHT_RIG_MULTIINSTANCE_SEED.md` (sections A–E).
**Sits on top of (inputs only, never edited):**
`doc/CINE_LIGHT_RIG_MULTIANCHOR_DESIGN.md` (group-as-one-unit, now a
**per-instance** mode) and `doc/CINE_LIGHT_RIG_MASTER_PRESETS_DESIGN.md`
(scale-aware geometry SA-0..SA-12, the **SA-9 nominal-EV invariant**,
`scaledPoint`). **Feeds:** a Codex `--prompt-file` implementation brief →
Opus adversarial review → build.

Claim labels per CLAUDE.md: **PROVES** = I read the code at the cited line
(all citations re-verified against the working tree on 2026-08-16; the tree
already contains the built multi-anchor group feature, so line numbers here
are current and authoritative — they differ from the seed's older numbers,
which are flagged in §10). **IMPLIES** = doc/comment or an unbroken but
unread chain. **INFERS** = reasoning; could be wrong. Guesses say "guess".

**User decisions (2026-08-16, fixed):** (1) concurrent per-subject rigs, each
independent; (2) the `cine_anchor` combo becomes the **editor selector**;
(3) one instance per Director cast slot, max 5 = {You, A, B, C, D}, anchor
implicit per slot; (4) the camera-focused rig gets shadow priority, never
exceeding `MAX_SPOT_SHADOWS = 6`. **Claude pre-commit (user may veto):** the
multi-anchor group stays a **per-instance mode**; double-lighting a shared
member is allowed in v1 with an optional warning, auto-suppress deferred.

---

## 0. Executive summary

One delivery: a **manager** owning five per-slot `ALCineLightRig` instances,
a per-instance **editing-buffer** state model, a camera-focus shadow policy,
selector UI, and additive scene serialization. **One new settings key**
(`CineLightRigInstances`, an LLSD blob store — the single unavoidable
addition, justified §A.4). **Zero model math changes, zero pipeline/shader/
llprimitive edits, zero scene version bump, `lldirectorcast.*` consumed
read-only.**

1. **Manager & state (§A).** `ALCineLightRigManager` (new singleton) owns
   `ALCineLightRig mInstances[5]` keyed by slot {SELF,A,B,C,D}. `ALCineLightRig`
   loses its `static instance()` (`alcinelightrig.cpp:452-456`) and becomes a
   plain owned object; the five `instance()` call sites (`alcinelightrig.cpp`,
   `alpanelcinelightrig.cpp`, `llfloaterdirector.cpp`, `llviewerdisplay.cpp`,
   `llappviewer.cpp`) route to the manager or to `manager.selected()`. The
   **~49 `CineLightRig*` settings keys stay the live editing buffer for the
   SELECTED instance only**; the panel's `control_name` bindings are
   UNCHANGED. Each instance carries a `ParamBlob` snapshot of those keys;
   the selected instance ticks from the **verbatim** `gSavedSettings` path
   (`readSettings`, `alcinelightrig.cpp:536-623`), non-selected enabled
   instances tick from their blob. Switching the selector **flushes**
   settings→outgoing blob and **loads** incoming blob→settings, then refreshes
   the panel.
2. **Budget & shadow (§B).** The **camera-focused instance** = the enabled
   instance whose slot avatar is nearest `gAgentCamera.getFocusTargetGlobal()`
   (`llagentcamera.h:242`), with hysteresis (margin + dwell) and a stable
   fallback (selected-if-enabled, else lowest enabled slot). The manager marks
   every OTHER enabled instance's projectors NoShadow through the existing
   `LLPipeline::toggleProjectorCastShadows` /`isProjectorNoShadow`
   session set (`pipeline.cpp:15097-15110`, already the lever
   `updateShadowPolicy` uses at `alcinelightrig.cpp:825-827`). Only the focused
   instance's ≤4 projectors then compete for the ≤6 slots — **6 can never be
   exceeded, structurally.** Worst case 5×8 = 40 emitters, all LIT: within the
   `RenderLocalLightCount` default of 256 (`pipeline.cpp:9173`) and covered by
   the existing `isCineRigEmitter()` exemptions (`pipeline.cpp:9137,16652,…`).
   No new soft cap.
3. **UI (§C).** `cine_anchor` becomes the five-slot editor selector; the
   group-mode disable at `alpanelcinelightrig.cpp:386` is dropped (the combo is
   always usable). A per-slot lit/active marker rides the combo rows and a
   status line. Per-instance enable/power edits the SELECTED instance. Group
   controls stay and edit the selected instance. **No panel reflow** — the
   buffer model holds the finalized layout (panel 1364,
   `panel_cine_light_rig.xml:5`).
4. **Scene (§D).** `sceneData`/`applySceneData` (`alcinelightrig.cpp:1820-1981`)
   gain an additive `instances` array + `selected_slot`, read AFTER the frozen
   three-way split (`:1855-1881`); the legacy single block keeps being written
   for the selected instance (old-viewer compat). **No version bump.** All four
   cross-version cases enumerated (§D.4).
5. **Finals preserved (§E).** This is a **multiplication of instances**, not a
   change to per-rig math. Each instance runs `scaledPoint` / exposure / mirror
   / master-temp / gobo / track-mode / group verbatim. **One enabled instance
   ("You") is BITWISE-identical to today via a structural fast path** — the
   selected+sole-enabled instance executes today's exact `tick()` reading
   `gSavedSettings`; no blob arithmetic runs.

---

## 1. Verification — current code, cited and re-checked

### 1.1 The singleton and its blast radius (all PROVES)

| # | Fact | Evidence |
|---|---|---|
| V1 | `static ALCineLightRig& instance()` returns a function-local static | `alcinelightrig.h:54`; `alcinelightrig.cpp:452-456` |
| V2 | One emitter set: `mProjectors[LIGHT_COUNT]`, `mOmnis[LIGHT_COUNT]`, one `mRegion` | `alcinelightrig.h:132-136` |
| V3 | One anchor + one group state: `mAnchor`, `mGroupEnabled`, `mGroupSlots`, `mLastResolvedGroupSlots` | `alcinelightrig.h:138-141` |
| V4 | One FX/transition/smoothing block (`mActiveFX`, `mTransition*`, `mSmoothedCentre`, `mSmoothedScale`, `mShaftEnabled[4]`, `mHeroEnabled[4]`, `mLastFrame`) | `alcinelightrig.h:142-169` |
| V5 | `instance()` call sites are exactly five files | `alcinelightrig.cpp:452`; `alpanelcinelightrig.cpp` (≈40 calls); `llfloaterdirector.cpp:888,1057`; `llviewerdisplay.cpp:1814`; `llappviewer.cpp:5525,6059` |
| V6 | `tick()` driven once per frame | `llappviewer.cpp:5525` (`ALCineLightRig::instance().tick(...)`) |
| V7 | `shutdown()` at app exit; idempotent, safe on partial emitter set | `llappviewer.cpp:6059`; body `alcinelightrig.cpp:1483-1498` |
| V8 | `renderGizmo()` once per frame from display | `llviewerdisplay.cpp:1814` |
| V9 | Scene save/load call the singleton | `llfloaterdirector.cpp:888` (`sceneData`), `:1057` (`applySceneData`) |

### 1.2 The per-tick model (single anchor + built-in group) — all PROVES

| # | Fact | Evidence |
|---|---|---|
| V10 | Feature gate: `!CineLightRigEnabled` ⇒ `shutdown()` and return; non-finite time ⇒ `setEmittersDark()` | `alcinelightrig.cpp:1032-1059` |
| V11 | Params pulled from ~40 keys via static `LLCachedControl` into `Setup`/`Globals`/`Transforms`, then sanitized | `readSettings`, `alcinelightrig.cpp:536-623` |
| V12 | Tick-only keys read directly in `tick()`: `CineLightRigEnabled`, `…FX`, `…OffsetZ`, `…Damping`, `…TrackMode`, `…ScaleAware` | `alcinelightrig.cpp:1032-1043` |
| V13 | `!globals.mPower` ⇒ destroy emitters, clear smoothing, return | `alcinelightrig.cpp:1065-1076` |
| V14 | Group branch: `mGroupEnabled` ⇒ `gatherGroupMembers(mGroupSlots,…)`; else resolve `mAnchor`; null avatar ⇒ dark path | `alcinelightrig.cpp:1081-1105` |
| V15 | Group aggregate (≥2 members): per-member `memberTrackPoint` + `groupBoundsCentre` + `groupSubjectScale`; single survivor falls to the shipped single-avatar path | `alcinelightrig.cpp:1107-1196` |
| V16 | Facing = `atan2` of `<1,0,0>*root->getWorldRotation()`, finite-guarded | `alcinelightrig.cpp:1172-1182` |
| V17 | Subject scale = `scale_aware ? avatar->getUniformScale() : 1.f`, then `sanitizeGlobals` | `alcinelightrig.cpp:1184-1196` |
| V18 | Centre via `memberTrackPoint`, OffsetZ `clamp(offset_z,−10,10)*subject_scale`, agent→global | `alcinelightrig.cpp:1272-1301` |
| V19 | Exponential centre+scale smoother, one `CineLightRigDamping`; snap on first frame/damping≤0/non-monotone time | `alcinelightrig.cpp:1303-1326` |
| V20 | FX path snaps live; non-FX path eases via `updateTransition`; then `render()` + `applyFrame()` + `updateShadowPolicy()` + `updateProjectorFlags()` | `alcinelightrig.cpp:1328-1358` |
| V21 | Emitters are client-only `LLVOVolume` tagged `LOCAL_OBJECT_CINE_RIG_EMITTER`; `isCineRigEmitter()` reads that kind | `alcinelightrig.cpp:665-666`; `llviewerobject.h:857-865` |
| V22 | Setters `setAnchor`/`setGroupEnabled`/`setGroupSlots` each reset `mHaveSmoothedCentre=false; mSmoothedScale=1.f` | `alcinelightrig.cpp:458-477` |

### 1.3 The model — SA-9 confirmed byte-frozen (PROVES)

| # | Fact | Evidence |
|---|---|---|
| V23 | `distance_ev = log2(safe_radius / DEFAULT_RADIUS)` reads the **NOMINAL** radius | `alcinelightrigmodel.cpp:702` |
| V24 | `effective_radius` (scaled) derived only when `mSubjectScale != 1.f`, and feeds **only** geometry (offsets/aim/light-radius) | `alcinelightrigmodel.cpp:703-710, 725-727, 755-757, 762, 774-787` |
| V25 | `total_ev = light.mEV + mMasterEV + distance_ev` — **no `mSubjectScale` term** in the intensity chain | `alcinelightrigmodel.cpp:728` |
| V26 | Group model functions are pure and additive: `groupBoundsCentre` (`:484`), `groupSubjectScale` (`:522-548`, `count==1` returns the sanitized member scale) | `alcinelightrigmodel.cpp:484,522-548` |
| V27 | `sanitizeSubjectScale` collapses non-finite/≤0/subnormal→1, clamps `[0.05,150]` | `alcinelightrigmodel.cpp` (`:463` chain); constants `alcinelightrigmodel.h:28-33` |

**The model is not touched by this feature.** Multi-instance runs the exact
same `render()` per instance; §E is the tripwire.

### 1.4 Shadow-slot machinery — the priority lever (all PROVES)

| # | Fact | Evidence |
|---|---|---|
| V28 | `MAX_SPOT_SHADOWS = 6` (compile-time); runtime count `bdmergeMaxSpotShadows() = clamp(BDMergeMaxSpotShadows, 2, 6)` | `pipeline.h:1030`; `pipeline.cpp:599-600` |
| V29 | Shadow slots are awarded by **priority**: `setupSpotLight` keeps the top `bdmergeMaxSpotShadows()` projectors by `getSpotLightPriority()`, displacing lower ones; `mTargetShadowSpotLight[]` is rebuilt from NULL each frame | `pipeline.cpp:17526-17579` |
| V30 | `getSpotLightPriority()` = on-screen `calcPixelArea` (default) or world `r³` when `BDMergeStableSpotShadows` | `llvovolume.cpp:3354-3389` |
| V31 | A projector in `sNoShadowProjectors` still **lights** but is excluded from the slot competition (`isProjectorShadowSuppressed`) | `pipeline.cpp:15266-15276, 17555-17557` |
| V32 | The exclusion set is toggled by public session API: `toggleProjectorCastShadows(id)`, `isProjectorNoShadow(id)`; cleared on session reset | `pipeline.cpp:15097-15110, 15086` |
| V33 | The rig ALREADY drives that set: `updateShadowPolicy()` opts each projector in/out per `CineLightRigShadowMode` (mode 0 = none, 1 = key only, 2 = all) | `alcinelightrig.cpp:812-830` |
| V34 | Rig emitters bypass the world-light toggle via `isCineRigEmitter()` in every nearby-light/deferred-light gather; the local-light cap is `RenderLocalLightCount` (default 256) | `pipeline.cpp:9137,9227,9321,9700,16652,19970`; default `pipeline.cpp:9173` |
| V35 | The panel already surfaces a shadow-budget hint and a "raise Max Spot Shadows" fix-it, and computes requested slots from `ShadowMode` | `alpanelcinelightrig.cpp:707-798` (`computeRequestedShadowSlots`, `onClickShadowFixIt`, `updateDerivedStatus`) |

### 1.5 Camera-focus signal (PROVES / IMPLIES)

| # | Fact | Evidence |
|---|---|---|
| V36 | `gAgentCamera.getFocusTargetGlobal()` returns the current look-at point in global coords | `llagentcamera.h:242` (PROVES) |
| V37 | `gAgentCamera.getFocusObject()` is usually null unless the user alt-zoom-focuses; not a reliable subject signal | `llagentcamera.h:229` (IMPLIES — nullable) |
| V38 | `LLViewerCamera::getInstance()->getOrigin()/getAtAxis()` give the eye and view axis (already used for spot priority) | `llvovolume.cpp:3374` (PROVES) |
| V39 | The VCam (`LLPrismLens`) carries no subject list/facing; not an anchor source | multi-anchor design §1.3 (IMPLIES) |

### 1.6 Panel plumbing (all PROVES)

| # | Fact | Evidence |
|---|---|---|
| V40 | `cine_anchor` combo populated with "You"(null) + **every cast member** (not the 5 slots), value = UUID | `alpanelcinelightrig.cpp:311-320` |
| V41 | `onAnchorSelected → instance().setAnchor(uuid)` | `alpanelcinelightrig.cpp:340-349` |
| V42 | The anchor combo is disabled while **GROUP mode** is enabled (`setEnabled(!enabled)` where `enabled = rig.isGroupEnabled()`), NOT while the rig is enabled | `alpanelcinelightrig.cpp:386` (**contradicts seed — §10**) |
| V43 | Group controls already exist (`cine_group_enable`, 5 `cine_group_*`, `cine_group_status`), synced via `syncGroupControls` with cached displayed-state | `alpanelcinelightrig.cpp:152-161, 372-423`; header `alpanelcinelightrig.h:69-71,85-87` |
| V44 | Panel is one shared class hosted twice (Director tab + floater); both refresh from `draw()`/`onVisibilityChange` | `alpanelcinelightrig.cpp:818-845` |
| V45 | Panel/wrapper geometry today: panel 1364, floater content 1374 / panel 1364, Director scroll-content 1374 / embedded 1364 | `panel_cine_light_rig.xml:5`; `floater_cine_light_rig.xml:24,34`; `floater_director.xml:1536,1546` |
| V46 | 49 `CineLightRig*` keys in `settings.xml`; scenes ALSO persist them via `sceneSettingsList()` | `settings.xml` (grep count 49); `llfloaterdirector.cpp` settings loop |

---

## Section A — the instance manager and state model (the core)

### A.1 Class shape

**MI-1.** New singleton `ALCineLightRigManager` (files
`indra/newview/alcinelightrigmanager.{h,cpp}`). `ALCineLightRig` **loses**
`static instance()` and its constructor stays public; the manager owns the
array.

```cpp
// alcinelightrigmanager.h
class ALCineLightRigManager
{
public:
    enum Slot : S32 { SLOT_SELF = 0, SLOT_A, SLOT_B, SLOT_C, SLOT_D, SLOT_COUNT };

    static ALCineLightRigManager& instance();     // replaces ALCineLightRig::instance()

    void tick(F64 presentation_time);              // llappviewer.cpp:5525 target
    void shutdown();                               // llappviewer.cpp:6059 target
    void renderGizmo() const;                      // llviewerdisplay.cpp:1814 target
    LLSD sceneData() const;                         // llfloaterdirector.cpp:888 target
    void applySceneData(const LLSD& data);          // llfloaterdirector.cpp:1057 target

    // Panel-facing: the SELECTED instance is the one bound to the live keys.
    ALCineLightRig&       selected();
    const ALCineLightRig& selected() const;
    Slot selectedSlot() const { return mSelected; }
    void setSelectedSlot(Slot slot);               // flush + load + panel refresh trigger

    ALCineLightRig&       at(Slot slot) { return mInstances[slot]; }
    bool  isSlotEnabled(Slot slot) const;          // reads that slot's ParamBlob.mEnabled
    U32   enabledMask() const;                       // bit per enabled slot
    S32   enabledCount() const;
    Slot  cameraFocusSlot() const { return mFocusSlot; }   // §B

private:
    struct ParamBlob;                               // §A.2
    ALCineLightRig mInstances[SLOT_COUNT];
    ParamBlob      mBlobs[SLOT_COUNT];              // per-instance settings snapshot
    Slot mSelected = SLOT_SELF;
    Slot mFocusSlot = SLOT_SELF;                    // §B hysteresis state
    // focus hysteresis bookkeeping (§B.3)
    Slot mFocusChallenger = SLOT_SELF; S32 mFocusChallengeTicks = 0;
};
```

**MI-2 (slot ⇒ anchor is implicit, per decision 3).** An instance's anchor is
NOT a stored free UUID for normal operation; it is derived from its slot each
tick exactly as `gatherGroupMembers` already resolves per slot
(`alcinelightrig.cpp:110-160`):

| Slot | Resolve call | Evidence |
|---|---|---|
| SELF | `LLDirectorCast::instance().resolve(LLUUID::null)` | `lldirectorcast.h:85`; `alcinelightrig.cpp:117` |
| A | `resolveSubjectA()` | `lldirectorcast.h:100`; `:119` |
| B | `resolveSubjectB()` | `:101`; `:121` |
| C | `resolveSubjectC()` | `:102`; `:123` |
| D | `resolveSubjectD()` | `:103`; `:125` |

`ALCineLightRig` gains a `Slot mSlot` field (or the manager passes the slot
into `tick`). `ALCineLightRig::tick` replaces the `mGroupEnabled ? gather :
resolve(mAnchor)` decision (`:1092-1094`) with:
**per-instance group mode ON ⇒ today's gather** (the instance's own
`mGroupSlots`); **group OFF ⇒ resolve THIS SLOT** (not `mAnchor`). A resolved
null avatar ⇒ the existing dark path (`:1095-1105`), so an instance whose slot
avatar is unloaded/left goes dark **without touching the other four** (each
instance owns its own emitters/state, V2/V4).

> `mAnchor` (`alcinelightrig.h:138`) is retained ONLY as the migration/compat
> carrier for old scenes (§D) and is otherwise unused by the tick. This keeps
> the field for the legacy scene block without reviving free-UUID anchoring.

### A.2 State-vs-settings: the editing-buffer model (RECOMMENDED)

**MI-3.** Keep the 49 `CineLightRig*` settings keys as the **live editing
buffer for the SELECTED instance only**. The panel's `control_name` bindings
(`panel_cine_light_rig.xml`) and `readSettings` (`alcinelightrig.cpp:536-623`)
are **UNCHANGED for the selected instance**. Each instance additionally holds a
`ParamBlob` — a plain struct snapshot of every settings-backed value the tick
reads (the 40 `readSettings` keys of V11 **plus** the tick-only keys of V12:
`mEnabled`, `mFX`, `mOffsetZ`, `mDamping`, `mTrackMode`, `mScaleAware`) **plus**
the session-only state that is not settings-backed but must persist per
instance (`mGroupEnabled`, `mGroupSlots`, `mShaftEnabled[4]`, `mHeroEnabled[4]`,
FX phase `mFXStart`/pending). Runtime-derived state (`mSmoothedCentre`,
`mSmoothedScale`, `mLastFrame`, emitter pointers, retry counters) stays as live
per-instance fields on `ALCineLightRig` — it is regenerated each tick and is
NOT part of the blob.

**MI-3a (blob completeness is a correctness requirement, not illustrative).** The
`ParamBlob` MUST capture EVERY settings key that ANY per-instance code path reads
— not only `readSettings`' 40 (V11) and the tick-only six (V12), but also keys
read by side paths: `CineLightRigShadowMode` (`updateShadowPolicy`, `:814`),
`CineLightRigGizmo` (`renderGizmo`). Any key a non-selected instance's path reads
from `gSavedSettings` instead of its blob is a **cross-instance leak** (it would
read the SELECTED instance's value — the exact class of bug MI-10a fixes for
ShadowMode). The Codex brief must enumerate all ~49 keys against the blob and
prove each per-instance read is sourced from the blob (non-selected) or
`gSavedSettings` (selected), never mixed. TUT test 3 (settings↔blob round-trip)
must exercise every field.

```cpp
struct ParamBlob                       // POD; F32 copied as F32 (bit-exact)
{
    bool mEnabled = false; bool mPower = true;
    F32  mRadius; /* … the 40 readSettings values … */
    F32  mOffsetZ; F32 mDamping; S32 mTrackMode; bool mScaleAware; S32 mFX;
    S32  mShadowMode = 0;                  // CineLightRigShadowMode — read by updateShadowPolicy (MI-10a)
    bool mGizmo = false;                    // CineLightRigGizmo — read by renderGizmo
    bool mGroupEnabled = false; U32 mGroupSlots = 0;
    bool mShaft[4] = {}; bool mHero[4] = {};
    // helpers:
    static ParamBlob fromSettings();       // read gSavedSettings + selected session state
    void  toSettings() const;               // write gSavedSettings + session state
    LLSD  toLLSD() const;  static ParamBlob fromLLSD(const LLSD&);   // §D / §A.4
};
```

**MI-4 (tick routing — where flush/load and per-instance ticking hook in).**
`ALCineLightRigManager::tick(t)` (called from `llappviewer.cpp:5525`):

```
1. mBlobs[mSelected] = ParamBlob::fromSettings();   // live-mirror the selected buffer
2. resolveCameraFocus();                            // §B, updates mFocusSlot with hysteresis
3. for slot in {SELF,A,B,C,D}:
      owns_shadows = (slot == mFocusSlot);          // §B.2a: only the focus rig arbitrates shadows
      if slot == mSelected:
          mInstances[slot].tickSelected(t, owns_shadows);   // VERBATIM today's tick() when owns_shadows
      else if mBlobs[slot].mEnabled:
          mInstances[slot].tickFromBlob(mBlobs[slot], t, owns_shadows);  // reads the blob
      else:
          mInstances[slot].shutdown();              // dark, releases its own emitters (V7)
4. applyShadowSuppression();                        // §B.2a: force-suppress every enabled NON-focus
                                                     //   projector AFTER all ticks (order-independent)
```

- `tickSelected(t)` **is today's `ALCineLightRig::tick` body verbatim** — it
  reads `gSavedSettings` through the existing static `LLCachedControl`s
  (V11/V12) and resolves its slot (MI-2). No blob is consulted.
- `tickFromBlob(blob, t)` is the SAME body with the ONLY difference being the
  value source: a new private `readSettings(const ParamBlob&, …)` overload
  that assigns the identical `Setup`/`Globals`/`Transforms` fields from blob
  members instead of `LLCachedControl`s, and the six tick-only reads (V12)
  taken from blob members. **Every downstream statement (facing, scale,
  centre, smoothing, FX/transition, `render`, `applyFrame`, shadow/projector
  flags) is byte-identical** — the reorder discipline of the multi-anchor
  design §F.2.3 applies: same statements, not equivalent ones.

**MI-5 (selector switch — flush/load).** `setSelectedSlot(newSlot)`
(called from the repurposed `onAnchorSelected`, §C):

```
if (newSlot == mSelected) return;
mBlobs[mSelected] = ParamBlob::fromSettings();   // FLUSH outgoing edits
mSelected = newSlot;
mBlobs[mSelected].toSettings();                   // LOAD incoming blob into live keys
// panel picks up the change on its next draw() via the existing sync paths (V44)
```

`ParamBlob::toSettings()` writes each key with `gSavedSettings.setF32/…`; the
existing settings-change signals repaint every `control_name`-bound widget in
BOTH hosts (V44), so no manual widget push is needed. The panel's cached
displayed-state (`mDisplayedGroupSlots`, `mDisplayedAnchor`, seed cache) is
invalidated so `syncGroupControls`/`syncSeedEditor` re-read (§C.3).

**MI-6 (why the buffer model over rebinding the panel to the manager).** The
finalized panel is large (≈49 bound controls + reset buttons + presets + gobos
+ FX + group + shadow hint) and was signed off in the master-presets and
multi-anchor passes. Rebinding every `control_name` to a manager-backed model
would rewrite the entire XUI binding layer and `readSettings`/`writeSetupToSettings`
for zero behavioural gain, and would break the per-control reset buttons that
call `gSavedSettings.getControl(param)->resetToDefault` (master-presets §D.1).
The buffer model localizes the change to (a) a `ParamBlob` snapshot/restore,
(b) one `readSettings(blob,…)` overload, (c) the manager's tick loop — the
finalized panel and its settings bindings are untouched. **This is the seed's
recommended model; adopted.**

### A.3 Which instances are active

**MI-7.** `ParamBlob.mEnabled` is the per-instance enable (the old global
`CineLightRigEnabled` becomes the SELECTED instance's enable, mirrored). An
instance ticks live **iff** its blob's `mEnabled` (and `mPower`, V13) is true;
otherwise `shutdown()` releases its own emitters. **Zero enabled ⇒ no emitters
anywhere** (each instance hits today's dark/shutdown path — V7/V13). Default on
first run: only SELF enabled, SELF selected; A/B/C/D disabled. An enabled
instance whose slot avatar is unresolved goes dark via the existing null-avatar
path (V14) **without disturbing the others** (per-instance emitters/state).

### A.4 Cross-session persistence — one new LLSD key

**MI-8.** The selected instance's buffer already persists (the 49 keys ride
`settings.xml` and `sceneSettingsList()`, V46). The **four non-selected blobs**
persist in **one new hidden settings key** `CineLightRigInstances` (LLSD, no
UI): an array of the five `ParamBlob::toLLSD()` maps plus the selected slot.

- **Write:** on `shutdown()` and whenever settings are saved, the manager
  flushes `mBlobs[mSelected] = fromSettings()` and writes all five blobs +
  `selected_slot` into `CineLightRigInstances`.
- **Read:** at startup the manager loads `CineLightRigInstances`, restores the
  five blobs, sets `mSelected`, and calls `mBlobs[mSelected].toSettings()` so
  the live keys reflect the selected instance. Absent/corrupt key ⇒ all five
  blobs default (SELF from current settings, A–D off) — today's single-rig
  startup.

**Why one LLSD key (option b), not per-key or a file:** option (b) adds **one**
key vs ~245 flat keys (5×49) or a new file with its own load/save/migration
lifecycle. It rides `gSavedSettings` persistence, is forward-compatible (old
binaries ignore an unknown key), and is the minimal schema touch consistent
with the seed's "prefer no schema churn." Option (c) scene-only is rejected:
user decision 1 (independent concurrent rigs) implies the four extra rigs
should survive a restart like every other rig parameter. **This is the ONLY
new settings key in the delivery.**

---

## Section B — budget and shadow priority (the hard constraint)

### B.1 "Camera-focused instance," precisely

**MI-9.** Per tick, over the ENABLED instances only, resolve each slot's avatar
(MI-2) and compute its world position `p_slot` (the avatar's `getRenderPosition`
in global space). The camera-focus target is `f = gAgentCamera.getFocusTargetGlobal()`
(V36). The **raw** focus slot is `argmin_slot ||p_slot − f||`. Tie-break by the
smaller angle to the view axis `LLViewerCamera::getInstance()->getAtAxis()` (V38),
then by slot order (SELF<A<B<C<D) for full determinism.

**Fallback chain (stable):**
1. If `getFocusTargetGlobal()` is non-finite or no enabled instance resolves an
   avatar → the **selected** instance if it is enabled;
2. else the lowest-numbered enabled slot;
3. else (nothing enabled) `mFocusSlot` is irrelevant (no projectors exist).

Rationale for focus-target over `getFocusObject()` (V37): the focus object is
null in the common free-camera/machinima case, whereas the look-at target is
always defined; distance-to-look-at is exactly "which subject the camera is
on."

### B.2 Claiming the ≤6 slots — structural, no pipeline edit

**MI-10.** After `resolveCameraFocus()`, before ticking, the manager runs
`applyShadowSuppression`:

- For every ENABLED, non-focus instance: add all four of its projector UUIDs to
  `sNoShadowProjectors` via `LLPipeline::toggleProjectorCastShadows` guarded by
  `isProjectorNoShadow` (V32) — exactly the idempotent pattern
  `updateShadowPolicy` already uses (`alcinelightrig.cpp:825-827`).
- For the FOCUS instance: leave its projectors free to compete, then let the
  instance's own `updateShadowPolicy()` (V33) apply its `CineLightRigShadowMode`
  (0/1/2) inside that free set.

Because only the focus instance's ≤4 projectors are ever eligible, the pipeline's
priority competition (V29) draws from ≤4 candidates against ≤6 slots — **the 6
cap is provably never exceeded, regardless of how many instances are lit.** All
non-focus instances remain **LIT but shadowless** (V31: suppressed projectors
still light). Deterministic fill order within the focus instance is the existing
`getSpotLightPriority()` ordering (V29/V30); with `BDMergeStableSpotShadows` on,
that is world-size (`r³`) and fully stable.

> Non-`ALCineLightRig` world projectors continue to compete on their own
> priority as today; the manager only ever adds/removes ITS OWN emitter UUIDs
> from the set, so world content is unaffected. On `shutdown()` the manager
> removes all rig UUIDs it added (mirroring the session-reset clear at
> `pipeline.cpp:15086`).

### B.2a The `updateShadowPolicy` interlock (design amendment — REQUIRED)

**MI-10a (a real hazard the naive loop misses).** `updateShadowPolicy`
(`alcinelightrig.cpp:812-830`) is called as the LAST step of every `tick`
(V20/V33) and **mutates the shared `sNoShadowProjectors` set** for its own four
projectors, reading the **global** `CineLightRigShadowMode` via a static
`LLCachedControl`. Two consequences if the per-instance tick keeps calling it
unchanged:

1. A non-focus instance's own `updateShadowPolicy` would **un-suppress its
   projectors mid-tick** (whenever its `ShadowMode ≥ 1`), defeating the
   manager's suppression — the 6-slot guarantee (MI-10) collapses.
2. A non-selected instance reads the **selected** instance's `ShadowMode` from
   `gSavedSettings` (not its own), so even its intended policy is wrong.

**Resolution — shadow arbitration is centralized on the FOCUS instance; every
other rig's shadow side effects are silenced:**

- `tick`/`tickSelected`/`tickFromBlob` take a `bool owns_shadows` parameter. The
  final `updateShadowPolicy()` call inside the shared body runs **only when
  `owns_shadows` is true** (i.e. the focus instance). When false, the tick does
  NOT touch `sNoShadowProjectors` at all.
- `updateShadowPolicy` is parameterized by the mode it should apply —
  `updateShadowPolicy(S32 shadow_mode)` — so the focus instance sources its OWN
  `ShadowMode` (from `gSavedSettings` when it is also the selected instance, else
  from its `ParamBlob.mShadowMode`). No path reads another instance's mode.
- After all ticks, `applyShadowSuppression()` (MI-4 step 4) force-adds all four
  projectors of every ENABLED, NON-focus instance to `sNoShadowProjectors`
  (idempotent via `isProjectorNoShadow`, V32). Running it AFTER the ticks makes
  it order-independent: non-focus ticks never touched the set, and the focus
  tick only touched its OWN projectors.
- **Focus hand-off is self-healing:** when a rig goes focus→non-focus,
  `applyShadowSuppression` adds its projectors (suppressed); when non-focus→focus,
  its next tick's `updateShadowPolicy(its mode)` sets the correct subset (which
  may itself suppress all four if its mode is 0). On `shutdown()` the manager
  removes every rig UUID it added.

**R1 impact (why this stays bitwise for the single rig).** For the lone-rig
user, SELF is the only enabled instance, so `mFocusSlot == SELF == mSelected` and
`owns_shadows == true` every tick. `tickSelected(t, /*owns_shadows=*/true)` then
calls `updateShadowPolicy(gSavedSettings ShadowMode)` — **exactly today's final
statement** — and `applyShadowSuppression()` finds no enabled non-focus instance,
so it is a no-op. The resulting `sNoShadowProjectors` membership is byte-identical
to today. The `owns_shadows` gate only ever removes shadow side effects from rigs
that are NOT the camera's subject, which by definition do not exist in the
single-rig case.

### B.3 Hysteresis (no shadow thrash when the camera moves)

**MI-11.** A moving camera must not re-award shadows every frame. The manager
holds `mFocusSlot`, a `mFocusChallenger`, and `mFocusChallengeTicks`:

```
raw = rawFocusSlot();                       // §B.1
if (raw == mFocusSlot) { mFocusChallengeTicks = 0; }
else if (raw == mFocusChallenger &&
         dist(raw) < dist(mFocusSlot) - FOCUS_MARGIN_M) {   // must be clearly closer
    if (++mFocusChallengeTicks >= FOCUS_DWELL_TICKS)        // and stay closer a while
        { mFocusSlot = raw; mFocusChallengeTicks = 0; }
} else { mFocusChallenger = raw; mFocusChallengeTicks = 0; }
```

Constants (named, tunable in-world): `FOCUS_MARGIN_M ≈ 0.75 m`,
`FOCUS_DWELL_TICKS ≈ 20` (≈⅓ s at 60 fps — guess, flagged for the in-world
pass). Result: the focus hand-off happens only when another subject is
decisively closer to the look-at for a sustained interval, so a pan across two
actors does not flip shadow ownership mid-frame. INFERS (elementary): a single
margin+dwell filter cannot oscillate faster than `FOCUS_DWELL_TICKS`.

### B.4 Total light budget

**MI-12.** Worst case: 5 instances × (4 projectors + 4 omnis) = **40 local
lights**, all lit. `RenderLocalLightCount` defaults to 256 (V34), and rig
emitters carry the `isCineRigEmitter()` exemption from the world-light toggle
in every gather path (V34) — so all 40 render regardless of the user's world/
attachment/projector toggles, and 40 ≪ 256. Bounce omnis are OFF by default
(`CineLightRigBounceEnabled` gates `destroyOmnis`, `alcinelightrig.cpp:1233-1236`),
so the typical concurrent load is 5×4 = **20 projectors**. **No new soft cap on
instances or emitters-per-instance:** the count is bounded to 5 by construction
(decision 3), the shadow cap is enforced structurally (MI-10), and the local-
light cap already exists. R4 (§7) documents the raw GPU cost so the in-world
pass can judge whether an *optional* future "max concurrent lit instances"
slider is worth it — deferred, not needed for correctness.

---

## Section C — UI: the anchor combo as editor selector

### C.1 Repurpose `cine_anchor`

**MI-13.** `cine_anchor` changes from "who the one rig lights" to "which slot's
rig I'm editing," listing the **five fixed slots** (not the full cast roster,
V40). `updateAnchorList` (`alpanelcinelightrig.cpp:284-324`) is rewritten to
populate five rows in slot order:

```
You   [lit] | Subject A — <name or "unset"> [lit] | … | Subject D — <name> [lit]
```

- Row value = the slot index (LLSD int 0..4), not a UUID.
- **Active/lit indicator:** each row's label carries a trailing marker when the
  slot is enabled AND currently resolving/lighting — derive "lit" from
  `manager.isSlotEnabled(slot)` && (that instance `lastResolvedGroupSlots()!=0`
  or its slot avatar resolves). Use a text marker (e.g. a leading "● " for
  lit, "○ " for enabled-but-dark, none for disabled) since `LLComboBox` has no
  per-row icon in this codebase (multi-anchor design §B.5 established the
  text-decoration idiom). A `cine_group_status`-style caption is unnecessary;
  the row markers plus the existing group status line cover it.
- Selecting a row calls `manager.setSelectedSlot((Slot)value)` (MI-5), which
  flushes/loads the buffer and lets the panel re-sync on next `draw()`.

**MI-14 (drop the group-mode lock).** Remove
`mAnchorCombo->setEnabled(!enabled)` at `alpanelcinelightrig.cpp:386`. The combo
is the instance selector and must ALWAYS be usable, independent of any
instance's group mode (each instance now has its own group state). `onAnchorSelected`
(`:340-349`) becomes `setSelectedSlot`, no `setAnchor`.

### C.2 Per-instance enable / power, and lockstep hosts

**MI-15.** The existing `CineLightRigEnabled` / `CineLightRigPower`
`control_name` widgets now edit the SELECTED instance's blob (they are part of
the buffer, MI-3) — no new widget needed. Because both hosts share one panel
class and one `gSavedSettings` buffer (V44), the Director tab and the floater
**edit the selected instance in lockstep** by construction: whichever is
visible writes the same keys, which the other reflects on its next draw. The
manager is the single source of `mSelected`, so opening the other host shows
the same selected slot.

### C.3 Sync plumbing

**MI-16.** `syncGroupControls` (`:372-423`) already caches displayed state and
edits the SELECTED instance once its calls route through `manager.selected()`
instead of `ALCineLightRig::instance()`. On a selector switch (MI-5) the panel
must re-read everything: invalidate `mDisplayedGroupEnabled/Slots/ResolvedSlots`
(`alpanelcinelightrig.h:85-87`), `mDisplayedAnchor` (repurposed to the slot),
and `mSeedInitialized`, so `draw()`'s existing `syncGroupControls`/
`syncSeedEditor`/`updateDerivedStatus` calls (`:818-832`) rebind the widgets.
The per-control reset buttons keep working unchanged (they hit `gSavedSettings`,
which is the selected buffer).

### C.4 Group-mode presentation and double-lighting

**MI-17.** Group controls stay and edit the SELECTED instance's `mGroupEnabled`/
`mGroupSlots` (per-instance, decision from Claude pre-commit). A member covered
by one instance's group AND by its own enabled instance is **double-lit** —
allowed in v1 (user's choice). Optional **warning** (deferred-nicety, may ship
if cheap): when `updateDerivedStatus` detects that any slot bit set in an
enabled instance's `mGroupSlots` is also an independently enabled slot, show a
one-line advisory in the existing `cine_shadow_hint`-style text area. **No
auto-suppress in v1** (explicitly deferred, §8).

### C.5 No panel reflow

**MI-18.** The buffer model (§A) adds **no controls** to the panel — the combo
is repurposed, enable/power already exist, group controls already exist. So the
finalized geometry (panel 1364; wrappers 1374/1364, V45) is **unchanged**. If a
future iteration adds a visible per-instance affordance and a reflow is needed,
apply the M1 lesson verbatim (multi-anchor §E.3): move the panel height AND
**all four** wrapper heights (`panel_cine_light_rig.xml:5`,
`floater_cine_light_rig.xml:24,34`, `floater_director.xml:1536,1546`) by the
same delta. Not required for this delivery.

---

## Section D — scene serialization (backward-compat)

### D.1 Save — additive `instances` + legacy block

**MI-19.** `ALCineLightRigManager::sceneData()` first flushes
`mBlobs[mSelected] = fromSettings()`, then:

```
LLSD data = mInstances[mSelected].sceneData();   // the LEGACY v1 block, verbatim
                                                  //   (version, anchor, anchor_group,
                                                  //    anchor_group_slots, base, fx, seed,
                                                  //    shafts, heroes, mirror, orbit_*)
data["selected_slot"] = (S32)mSelected;           // NEW
data["instances"]     = LLSD::emptyArray();       // NEW: 5 entries
for slot in {SELF..D}:
    LLSD e = perInstanceLLSD(slot);               // {slot, enabled, base, group, group_slots,
                                                  //  shafts, heroes, fx, seed, offset_z,
                                                  //  damping, track_mode, scale_aware, …}
    data["instances"].append(e);
```

- `data["version"]` stays **1** (the block shape is unchanged; the new keys are
  additive — same argument as the multi-anchor `anchor_group` additive keys,
  which an old binary ignores because the loader reads named keys only, V-loader
  `alcinelightrig.cpp:1855-1981`).
- The legacy block continues to carry the SELECTED instance's full state, so an
  old viewer loads a sane single rig (D.4 case 2).

### D.2 Load — after the frozen three-way split

**MI-20.** `applySceneData(data)`:

1. Run the EXISTING three-way split unchanged (`alcinelightrig.cpp:1855-1881`):
   no block / unknown version / v1. This is a **final** — untouched.
2. **If `data["instances"]` is a well-formed array (new scene):** for each
   entry, restore `mBlobs[slot]` from its LLSD (`ParamBlob::fromLLSD`), set
   `mSelected = clamp(data["selected_slot"], 0, 4)`, then
   `mBlobs[mSelected].toSettings()` so the live keys and the legacy-applied
   selected state agree. The legacy block already applied by step 1 to the
   settings is thus consistent with (and superseded by) the selected blob.
3. **Else (old scene, no `instances`):** MIGRATE — the legacy block that step 1
   applied to `gSavedSettings` becomes the blob for the slot matching the old
   `data["anchor"]`:
   - `anchor == null` → SLOT_SELF.
   - `anchor == cast.getSubjectA()` (`lldirectorcast.h:96`) → SLOT_A; likewise
     B/C/D via `getSubjectB/C/D()` (`:97-99`).
   - any other UUID (a legacy arbitrary cast anchor) → SLOT_SELF, and the
     migrated blob keeps `mAnchor = data["anchor"]` so the SELF instance
     resolves that avatar (the one place `mAnchor` still drives a tick — a
     documented compat allowance for pre-multi-instance scenes; INFERS this is
     acceptable because such scenes predate the 5-slot model and the operator
     re-picks if wrong).
   - All OTHER slots default OFF. `mSelected =` the migrated slot.

### D.3 Reset / no-schema

**MI-21.** No scene version bump (justified: additive keys, both cross-version
directions degrade sanely — the exact condition the multi-anchor design used to
avoid a bump). `SCENE_VERSION` (`llfloaterdirector.cpp`) is untouched. Reset All
(`alpanelcinelightrig.cpp:643-645`) additionally disables A–D instances and
selects SELF (its existing `setGroupEnabled(false)/setGroupSlots(0)` now target
`manager.selected()`).

### D.4 The four cross-version cases

| Case | Result | Mechanism |
|---|---|---|
| **Old viewer + old scene** | today's single rig | unchanged baseline |
| **Old viewer + NEW scene** | reads legacy block only (the selected instance), ignores `instances`/`selected_slot` — a sane single rig | v1 loader reads named keys only (`:1855-1981`) |
| **New viewer + OLD scene** | migrate legacy block into the anchor's slot (MI-20 step 3), others OFF | no `instances` key ⇒ migration path |
| **New viewer + NEW scene** | restore all five blobs + selected slot; legacy block consistent | MI-20 step 2 |

---

## Section E — preserving the finals (the ship-risk)

**MI-22.** This feature is a **multiplication of instances**, never a change to
per-rig math. Each instance runs the finalized model verbatim: `scaledPoint`
foot-pivot (`alcinelightrig.cpp:84-108`), SA-9 nominal-EV invariance
(`alcinelightrigmodel.cpp:702,728` — V23/V25), mirror reflection, master
colour-temp gain, gobos, track-mode, and the group multi-anchor mode — all
consumed, none edited. The manager only chooses WHICH blob feeds an instance and
WHICH instances suppress shadows.

**MI-23 (R1 — single-instance bitwise, STRUCTURAL).** The invariant "one
enabled instance ('You') == today, bitwise" is enforced by code structure, not
by float algebra:

- The **selected** instance always ticks via `tickSelected(t)` = today's exact
  `ALCineLightRig::tick` body reading `gSavedSettings` (MI-4). No blob is read;
  no aggregation runs.
- The shipped default (fresh install AND every migrated old scene) is SELF
  enabled, SELF selected, A–D disabled. The manager loop (MI-4) then executes
  **exactly** `mInstances[SELF].tickSelected(t)` and four `shutdown()`s on
  already-dark instances (no-ops on empty emitter sets, V7). The statements that
  run for SELF are byte-identical to today's `tick`.
- **Two legs, stated honestly (correction — the coupling is NOT total).** A user
  can enable SELF, then move the selector to A (still disabled) to peek at it —
  now `popcount(enabledMask)==1` (only SELF) yet `mSelected==A`, so SELF ticks
  via `tickFromBlob`, NOT `tickSelected`. So "one enabled ⇒ selected ⇒ verbatim"
  is **false in general**. The bitwise-today guarantee therefore rests on TWO
  legs, both required:
  1. **The pure single-rig user never leaves SELF selected** (they only ever use
     one rig and never move the selector), so SELF ticks via `tickSelected`
     verbatim — this is the population that must equal today, and it does.
  2. **`tickFromBlob` is byte-identical in OUTPUT to `tickSelected`** for the
     same parameter values (MI-24; the only difference is value source, blob ↔
     `LLCachedControl`, and the blob was populated from those same values). So
     even the "enabled-but-not-selected lone rig" renders identically.
  On scene load the manager still **normalizes `mSelected` to the sole enabled
  slot when exactly one is enabled**, which maximizes how often leg 1's literal
  path runs. TUT test 7 pins the routing predicate as a CONDITIONAL (if
  sole-enabled AND selected ⇒ verbatim), not a claim that usage always satisfies
  it. The reviewer must verify leg 2 by diffing `tickFromBlob` against
  `tickSelected` statement-for-statement.

**MI-24 (statements that MUST execute unchanged in the single-instance path).**
The reviewer diffs `tickSelected` against today's `tick` and requires identity
of: the enable/power gates (`:1049-1076`), the slot resolve replacing
`resolve(mAnchor)` **only** in the branch selector — for SELF the resolve is
`resolve(LLUUID::null)`, which is exactly `resolve(mAnchor)` when `mAnchor` is
null (the default) — the facing block (`:1172-1182`), the scale read
(`:1184-1196`), `memberTrackPoint` + OffsetZ (`:1272-1301`), the smoother
(`:1303-1326`), FX/transition (`:1328-1350`), `render` + `applyFrame`
(`:1352-1355`), and `updateShadowPolicy`/`updateProjectorFlags` (`:1356-1357`).
`tickFromBlob` must be the SAME statements with the value source swapped
(ParamBlob member ↔ LLCachedControl) — reviewed as a diff, per multi-anchor
§F.2.3.

**MI-25 (SA-9 untouched).** `mSubjectScale` still enters only geometry (V24);
`distance_ev` still reads nominal (V23); the intensity chain has no scale term
(V25). Multi-instance changes none of this — it just calls `render()` five
times with five `Globals`. The existing exposure-invariance TUT test
(`alcinelightrigmodel_test.cpp:880-904`) continues to pin it.

---

## 5. Testability — TUT additions

The pure model is unchanged, so the model suite passes untouched. New coverage
is split: **pure-model manager helpers** (fully TUT-able) and **controller-side
determinism** (near-pure helpers that avoid `LLDirectorCast`/avatars). NO
tautological / self-comparison tests — each asserts against an independent
expected value or the shipped baseline, and FAILS on regression (explicitly
avoiding the prior feature's tautological-test finding).

Put manager-pure helpers behind free functions so they need no viewer singletons:

1. **Slot↔bit mapping pin.** `slotToGroupBit(Slot)` and the reverse over all 5
   slots equal the frozen `GROUP_SLOT_*` values (`alcinelightrig.h:30-35`);
   fails if the enum drifts.
2. **Blob round-trip is bitwise.** `ParamBlob b` with a hand-built distinctive
   value in every field → `ParamBlob::fromLLSD(b.toLLSD())` equals `b`
   field-by-field, F32 compared with `ensure_equals` (exact). Fails if any field
   is dropped/reordered/re-quantized in serialization. (NOT self-comparison: the
   expected values are the independently-constructed literals, and the assertion
   crosses the LLSD boundary.)
3. **Blob↔settings round-trip preserves values.** Seed `gSavedSettings` with a
   distinctive vector, `ParamBlob::fromSettings()`, mutate settings, then
   `toSettings()` and re-read: every key equals the captured vector, bitwise.
   Fails if flush/load loses or transposes a field (R5). Uses a settings
   fixture, not the live tree.
4. **Camera-focus selection determinism.** Pure `rawFocusSlot(points[5],
   enabledMask, focus_point, at_axis)`: for a fixed geometry the chosen slot
   equals the hand-computed nearest; tie between two equal-distance slots
   resolves to the lower slot; disabled slots are never chosen. Fails if the
   argmin/tie-break changes.
5. **Hysteresis anti-thrash.** Drive `focusFilter(state, raw, dist)` with a
   scripted sequence where `raw` oscillates every tick but never beats the
   incumbent by `FOCUS_MARGIN_M` for `FOCUS_DWELL_TICKS`: assert `mFocusSlot`
   NEVER changes. Then a sustained clear challenger for the full dwell: assert
   it flips exactly once, on the dwell-th tick. Fails on thrash or on a missed
   hand-off (R2).
6. **Shadow-eligibility count ≤ 6.** Pure `eligibleProjectorCount(enabledMask,
   focusSlot)` = 4 (focus has 4 projectors) regardless of how many instances are
   enabled; assert over all `enabledMask ∈ [1,31]` that it is ≤ 4 ≤ 6. Fails if
   a future edit lets a non-focus instance keep shadow eligibility (R2 overflow).
7. **Single-instance == legacy (structural pin).** Assert the routing predicate:
   `pathFor(enabledMask, selected)` returns `TICK_SELECTED_VERBATIM` whenever
   `popcount(enabledMask)==1 && enabledBit==selected`, and the normalization
   `normalizeSelected(enabledMask, selected)` returns the sole enabled slot when
   `popcount==1`. Fails if a refactor lets a lone-enabled instance take the blob
   path (R1). (This tests the *decision*, not float output — the float identity
   is guaranteed by the shared statement body, reviewed as a diff per §5-note.)
8. **Migration slot mapping.** Pure `migrateAnchorToSlot(anchor, subjectIds[4])`:
   null→SELF; each subject id→its slot; an unknown id→SELF-with-mAnchor-set.
   Fails if the anchor→slot table changes (R3).

**Not TUT-testable (adversarial-review items):** the `tickSelected` vs today
statement-identity (git diff), `tickFromBlob` statement-identity, the live
`NoShadow` set integration, panel selector re-sync, and the full scene
cross-version matrix (§D.4). Review these the way the mirror/multi-anchor
reviews did: an in-world single-instance A/B against the current binary first
(R1), then a camera pan across a two-subject two-rig setup (R2), then all four
scene combinations (R3).

---

## 6. File / function checklist

| File | Change |
|---|---|
| `indra/newview/alcinelightrigmanager.h` **(new)** | `ALCineLightRigManager` (MI-1), `Slot` enum, `ParamBlob` struct (MI-3), public tick/shutdown/renderGizmo/scene/selected/at/enabled API, focus-hysteresis fields |
| `indra/newview/alcinelightrigmanager.cpp` **(new)** | `instance()`; the tick loop (MI-4); `setSelectedSlot` flush/load (MI-5); `resolveCameraFocus` + hysteresis (§B.1-3); `applyShadowSuppression` (MI-10); `ParamBlob::from/toSettings`, `from/toLLSD` (§A/§D); `sceneData`/`applySceneData` orchestration + migration (§D); `CineLightRigInstances` persist/restore (§A.4) |
| `indra/newview/alcinelightrig.h` | remove `static instance()`; add `Slot mSlot`; add `tickSelected(F64)` and `tickFromBlob(const ParamBlob&, F64)`; make ctor usable as an array member |
| `indra/newview/alcinelightrig.cpp` | delete `instance()` (`:452-456`); split `tick` (`:1030-1359`) into `tickSelected` (verbatim, reads `gSavedSettings`) + a shared body parameterized by a `readSettings` source; add `readSettings(const ParamBlob&, …)` overload; replace the `mGroupEnabled ? gather : resolve(mAnchor)` selector (`:1092-1094`) with per-instance group-mode / slot-resolve (MI-2); the legacy single-block `sceneData`/`applySceneData` bodies stay for reuse |
| `indra/newview/alpanelcinelightrig.h` | route `ALCineLightRig::instance()` uses to `ALCineLightRigManager`; add nothing structural (existing group/anchor fields repurposed) |
| `indra/newview/alpanelcinelightrig.cpp` | `updateAnchorList` → five-slot selector with lit markers (MI-13); `onAnchorSelected` → `setSelectedSlot` (MI-14); DROP `setEnabled(!enabled)` (`:386`, MI-14); route `setGroupEnabled/Slots`, `setShaft/HeroEnabled`, `stopFX`, `isClipped`, `isShaftEnabled`, Reset All to `manager.selected()`; invalidate displayed-state caches on slot switch (MI-16); optional double-light advisory (MI-17) |
| `indra/newview/llfloaterdirector.cpp` | `:888` `ALCineLightRig::instance().sceneData()` → `ALCineLightRigManager::instance().sceneData()`; `:1057` likewise `applySceneData` |
| `indra/newview/llviewerdisplay.cpp` | `:1814` `renderGizmo()` → manager (draws the SELECTED instance's gizmo, respecting `CineLightRigGizmo`) |
| `indra/newview/llappviewer.cpp` | `:5525` `tick` → manager; `:6059` `shutdown` → manager (which shuts all five + removes its NoShadow UUIDs) |
| `indra/newview/app_settings/settings.xml` | **one** new hidden key `CineLightRigInstances` (LLSD, default empty) — §A.4 |
| `indra/newview/CMakeLists.txt` | add the two new `alcinelightrigmanager.*` sources |
| `indra/newview/tests/alcinelightrigmodel_test.cpp` (or a new `alcinelightrigmanager_test.cpp`) | §5 cluster |

**NOT touched:** `alcinelightrigmodel.{h,cpp}` (no model changes — §E);
`pipeline.cpp`/`pipeline.h`/all GLSL/`indra/llprimitive/*` (6-slot cap + 20 m
reach are inputs — the shadow policy uses only the existing public
`toggleProjectorCastShadows`/`isProjectorNoShadow`); `lldirectorcast.{h,cpp}`
(read-only roster/subject API); `llvovolume.*`/`llviewerobject.*`
(`isCineRigEmitter` consumed as-is); scene `SCENE_VERSION` and preset versions
(stay); `panel_cine_light_rig.xml` and both floater XML (no reflow — MI-18);
the FX/gobo/master-temp/preset/reset-button code (unchanged consumers).

---

## 7. Risks, ranked

- **R1 ⚠ (highest) — single-instance bitwise.** The `tick` split touches the
  every-shot path, including plain single-rig shots. Mitigation: structural
  fast path (`tickSelected` = today's body verbatim; sole-enabled ⇒ selected ⇒
  that path — MI-23/24); review demands statement-identity, not equivalence;
  first in-world test is a single-instance A/B against the current binary.
- **R2 ⚠ — shadow-slot overflow / thrash.** Overflow is closed structurally
  (only the focus instance's ≤4 projectors are eligible — MI-10; TUT test 6).
  Thrash is closed by margin+dwell hysteresis (MI-11; TUT test 5). In-world
  probe: pan slowly across two lit subjects and watch for shadow pops.
- **R3 — scene cross-version.** Four cases enumerated (§D.4); additive keys,
  legacy block always written, migration table pinned (TUT test 8). Load-test
  all four, including a new scene with 3 lit instances into an old binary.
- **R4 — emitter / light budget.** 40 emitters worst case, within
  `RenderLocalLightCount` 256 and the `isCineRigEmitter` exemptions (MI-12);
  raw GPU cost documented, no correctness issue. Optional future concurrency
  slider deferred.
- **R5 — editing-buffer flush/load losing edits or leaking state.** The switch
  flush (MI-5) must capture the outgoing edits BEFORE loading the incoming blob;
  the mirror step (MI-4 step 1) must run BEFORE any tick. TUT tests 2/3 pin the
  round-trips bitwise; review the ordering. Leak risk: a stale
  `mDisplayedGroupSlots` cache showing the old instance's group — closed by the
  cache invalidation in MI-16.
- **R6 — `instance()` retirement blast radius.** Five files (V5); a missed call
  site would compile-fail (the static is deleted), so the compiler enforces
  completeness — low residual risk.
- **R7 — new settings key.** One key, hidden, LLSD, ignored by old binaries; the
  only schema touch. Corrupt-value path defaults to today's single-rig startup
  (§A.4).
- **R8 — double-lighting.** Allowed by decision; only cost is intensity
  stacking on a shared member. Advisory optional (MI-17); auto-suppress
  deferred.

## 8. Deferred (decided now, with reasons)

1. **Group double-light auto-suppress** — allowed in v1 (user choice); a
   member lit by a group instance AND its own instance is not de-duplicated.
   Needs a cross-instance member-resolution pass; low value, revisit on demand.
2. **Per-instance track-mode / damping already free** (they are per-blob) — no
   work; noted so the reviewer knows it is intentional, not missing.
3. **"Max concurrent lit instances" performance slider** — not needed for
   correctness (MI-12); add only if the in-world pass finds 40 emitters heavy.
4. **Focus by VCam frustum** (light exactly what the lens frames) — the VCam has
   no subject list (V39); the look-at-target proxy (§B.1) is the available
   signal. Revisit if a real lens-driven selection is wanted.
5. **Per-row combo icons for lit state** — using text markers instead
   (`LLComboBox` has no per-row icon here); cosmetic upgrade later.

## 9. OFF-LIMITS for the implementation brief

Everything not named in §6 is off-limits. Explicitly, even where adjacent:

- **The finalized per-rig model math** — `scaledPoint`'s body
  (`alcinelightrig.cpp:84-108`), the exposure/EV chain and `distance_ev`
  (`alcinelightrigmodel.cpp:702,728` — SA-9, byte-frozen; §5 exposure test is
  the tripwire), the mirror reflection line, master-temp gain, the gobo path,
  track-mode, and the group centre/scale/facing math (`groupBoundsCentre`,
  `groupSubjectScale`). Multi-instance CALLS these per instance; it changes none
  of them. **No `alcinelightrigmodel.*` changes.**
- **`pipeline.cpp`, `pipeline.h`, all GLSL, `indra/llprimitive/*`** — the
  `MAX_SPOT_SHADOWS = 6` cap and 20 m projector reach are design INPUTS. The
  shadow policy uses ONLY the existing public session API
  (`toggleProjectorCastShadows`/`isProjectorNoShadow`, V32) exactly as
  `updateShadowPolicy` already does; do not touch the priority sort, the slot
  arrays, or the light-gather loops.
- **`lldirectorcast.{h,cpp}`** — consumed read-only (`resolve`,
  `resolveSubjectA..D`, `getSubjectA..D`, `getCast`); do not add rig state to it.
- **`llvovolume.*` / `llviewerobject.*`** — `isCineRigEmitter` /
  `LOCAL_OBJECT_CINE_RIG_EMITTER` consumed as-is.
- **Scene `SCENE_VERSION` and preset versions** — stay; additive keys only, no
  bump (§D.3). The `applySceneData` three-way split
  (`alcinelightrig.cpp:1855-1881`) is frozen; new reads are additive, AFTER it.
- **Settings schema** — exactly ONE new key (`CineLightRigInstances`); no other
  new keys, no renames, no per-key explosion.
- **The finalized panel XML layout** — no reflow (MI-18); if ever needed, the M1
  five-height rule applies.

---

## 10. Where the seed looks wrong (flag, don't silently follow)

1. **The anchor-combo lock is mis-described.** The seed says the combo "is
   DISABLED while the rig is enabled (`:386` `setEnabled(!enabled)`) — you
   cannot re-anchor mid-run." In the current tree, `alpanelcinelightrig.cpp:386`
   is `mAnchorCombo->setEnabled(!enabled)` where **`enabled = rig.isGroupEnabled()`**
   (V42) — the combo is disabled while **GROUP mode** is on, not while the rig
   is enabled. Re-anchoring mid-run is already allowed today. The design still
   drops this `setEnabled` (MI-14) because the combo is being repurposed to the
   instance selector, but the RATIONALE differs from the seed's.

2. **The anchor combo lists the whole cast, not five slots.** The seed frames
   the selector change as re-labelling; in fact `updateAnchorList` populates
   "You" + **every** cast member (V40), which can exceed five. Turning it into a
   fixed five-slot selector (MI-13) is a real content rewrite of
   `updateAnchorList`/`syncAnchorSelection`, not a relabel. Called out so the
   Codex brief scopes that function fully.

3. **Line numbers in the seed's "verified current model" are stale.** The seed
   cites the pre-group tree (e.g. the three-way split at `:1671-1697`); in the
   current tree it is `:1855-1881`, and the emitter/group/setter lines have all
   moved. This document's citations are authoritative.

None of these overturn a *decision* — the four fixed user decisions and the
group-as-per-instance-mode pre-commit all stand. They are code-accuracy
corrections the implementer must have.
