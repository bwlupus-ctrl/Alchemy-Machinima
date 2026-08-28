/**
 * @file alfloaterultimatediopter.h
 * @brief [Ultimate Diopter] Phase 3 smart-UI floater: the nine-tab regroup,
 *        Tier-A hiding, same-rect overlay band panels, and the
 *        "Active controls only" escape hatch, layered on top of wave 2's
 *        dim-only gating (design doc DIOPTER_SMART_UI_DESIGN.md §7 Phase 3
 *        row). Wave 4 (this pass) adds Phase 4 (depth eyedropper + focus
 *        honesty, §5.2/§5.3) and Phase 5 (polish, §7) on top -- see the
 *        "Phase 4 / Phase 5" comment block below the Phase 3 one.
 *
 * Extends wave 2's class rather than replacing it: postBuild()'s table-
 * driven row resolution, refreshRelevance()'s signature guard, the tier /
 * preset-owned tooltip logic and the master-enable (Tier 0) branch are all
 * still here, unchanged in spirit. Phase 3 adds three things on top:
 *
 *  1. Same-rect overlay band panels (§3.3, the ALPanelCineCamParams idiom)
 *     for the clean mode partitions the XML regroup introduced -- Shape's
 *     shape-specific band, Bokeh's aperture band, Motion's per-mode band,
 *     both Spin groups' per-mode bands, Kaleido's pattern+FX band, and Kal
 *     Anim's motion band. updateBandPanels() walks a small static table
 *     (bandSpecs(), mirroring ALPanelCineCamParams::modeTable()) and shows
 *     exactly one panel per band for the currently selected mode value,
 *     hiding the rest -- exactly alpanelcinecamparams.cpp:511-524's pattern,
 *     called unconditionally from refreshRelevance() (already signature-
 *     guarded; every one of the 7 pivot settings was already part of wave
 *     2's ALDiopterState/signature, so no new driver was needed for this).
 *
 *     NOT every mode-dependent control could go in a same-rect panel: a
 *     control used by MORE than one mode value of the same pivot (e.g.
 *     Motion Speed, live in 11 of 12 modes) can only have one XML parent,
 *     so it cannot be duplicated across sibling panels. Those "standing"
 *     rows stay directly on the tab (not nested in any band panel) and are
 *     gated the ordinary per-row way; see scratchpad/wave3_handoff.md for
 *     the full header/standing/exclusive breakdown per band.
 *
 *  2. TIER_MODE rows now HIDE (setVisible(false)) instead of merely
 *     dimming, now that the regroup gives every hidden control a real home
 *     to disappear into. TIER_OVERRIDDEN / TIER_UNARMED are unchanged from
 *     wave 2 (disable + dim, never hidden) -- §3.2's Tier B/C treatment
 *     does not change in Phase 3, only Tier A does.
 *
 *  3. The "Active controls only" toggle (new persisted setting
 *     CineDiopterUIActiveOnly, default TRUE, a checkbox in the persistent
 *     footer strip). Off does NOT mean "show everything" -- that is
 *     impossible against same-rect overlays without duplicating widgets
 *     (§5.2's own Star-Points-under-Circle example). Off means: a
 *     TIER_MODE row that is (a) a "standing" row, not swap-panel-nested,
 *     and (b) on the CURRENTLY SELECTED tool, is shown disabled+dimmed
 *     instead of hidden (matching Tier B/C's always-shown treatment); a
 *     row that is genuinely nested inside another mode's now-hidden panel,
 *     or that belongs to the other tool entirely, stays hidden regardless
 *     -- there is nowhere on screen to put it. See isSwapNested() and the
 *     handoff for the exact 39-control nested set.
 *
 * Focus transfer before every hide/swap (§3.8a) still targets the floater
 * itself, as wave 2 chose for Phase 2 -- kept deliberately simple rather
 * than threading a per-section "mode combo" target through 7 different
 * band pivots for uncertain UX benefit; see the handoff's ambiguity list.
 *
 * Control discovery is still entirely table-driven: postBuild() walks
 * sRelevance[0..AL_RELEVANCE_COUNT) and resolves each row's mCtrlName /
 * mResetName via findChild(), so there is never a second hardcoded name
 * list to drift from the table (design doc table<->schema parity rule).
 * The XML regroup moved every control to a new tab/panel without this
 * class needing to change at all -- findChild() does not care which tab or
 * panel currently parents a name.
 *
 * ---------------------------------------------------------------------------
 * Phase 4 (depth eyedropper + focus honesty, §5.2/§5.3) and Phase 5
 * (polish, §7's row) -- wave 4, layered on top of everything above without
 * touching any of it:
 *
 *  1. Three ALToolFocusPick eyedroppers (Base Distance, Lens Distance,
 *     Kaleido Depth Cut) plus a "Set From Camera Focus" button beside Base
 *     Distance. A pick writes its metres AND forces the paired mode setting
 *     mandatory per §5.2 (Base -> CineDiopterFocusMode=0; Lens ->
 *     CineDiopterLensFocusMode=1; kaleido -> CineDiopterKalProtectMode |= 2,
 *     which maps exactly onto the combo's Off/Radius/Depth/Radius+Depth
 *     2-bit encoding: 0->2, 1->3, 2/3 unchanged). CineDiopterKalDepthCut
 *     AND CineDiopterKalProtectMode are BOTH in the kaleido preset-owned
 *     array (aldiopterpresetbank.cpp kaleidoOwnedNames()), so while a named
 *     preset is active the kaleido pick batches both writes under
 *     ALScopedPresetMaterializing (aldiopterpresetbank.h) -- materializing
 *     the preset's look into every OTHER owned field first, then stomping
 *     the two picked values on top -- followed by exactly ONE
 *     AL_EXIT_AUTO_AFTER_EDIT flip to Custom, so the transition's own
 *     snapshot (alDiopterHandlePresetTransition) captures both new values
 *     together (Codex round-2 CR2-2 -- writing them one at a time let the
 *     first write's owned-setting listener snapshot the bank BEFORE the
 *     second write ever ran, silently losing the "arm Depth protection"
 *     half of the pick on a later Custom restore). CineDiopterBaseFocusM /
 *     CineDiopterLensFocusM are NOT preset-owned (absent from
 *     diopterOwnedNames()), so the Base/Lens picks have no listener to
 *     interact with either way.
 *  2. A throttled (~10 Hz) hover preview, uniformly across all three
 *     pickers (§5.2: "not worth a behavioural inconsistency between two
 *     buttons that look identical"), shown in a small readout strip beside
 *     each pick button; committing writes nothing until the click.
 *  3. The Base Focus provenance readout (§5.3a) -- reads the ALREADY
 *     PUBLISHED LLPipeline::sDiopterBaseFocusProvenance/ResolvedM statics
 *     (wave 1/2, call-swap 6) verbatim, never re-derived. The Kal Anim
 *     "Subject: N m" readout beside Depth Cut is a distinct, mode-
 *     independent quantity (Depth Cut has no camera-tracking mode of its
 *     own) computed by calling the SAME shared alDiopterResolveBaseFocus()
 *     helper with focus_mode forced to 1, so it can never disagree with the
 *     renderer's own camera-focus ladder either.
 *  4. Per-control range combos (§5.3e) on the four metre sliders that need
 *     one (Base/Lens Distance share one ladder; Depth Cut and Depth Feather
 *     each have their own, per their own renderer-governed ceiling) with
 *     clamp-and-commit-immediately semantics, an inline "say so" message
 *     when a range change actually moves the value, and a bounded single-
 *     level "undo" (switching the range combo again restores the pre-clamp
 *     value if nothing has touched the slider since). Auto-selects the
 *     narrowest containing range on open and after every eyedropper pick.
 *  5. "Show Depth" toggle checkboxes (Focus and Kal Anim tabs) flipping the
 *     shared CineDiopterDebugView between Off(0) and Depth(3); plain XML
 *     checkboxes for RenderFocusPointCrosshair/RenderFocusPointLocked
 *     (§5.3d) -- both pre-existing settings already consumed by
 *     LLPipeline::renderFocusPoint, just never exposed in this floater.
 *  6. Phase 5 polish, exactly §7's two-row list: a non-default tint on the
 *     180 reset buttons' existing icon (isDefault()-driven, no new texture
 *     asset), and an LLFilterEditor filter box (mirroring
 *     LLFloaterSettingsDebug's '+'-separated-AND-tokens, lowercase
 *     substring convention) that can reveal a currently tier-hidden
 *     "standing" row so a director can find where a control went -- it
 *     cannot reveal a swap-nested row (isSwapNested(), same "nowhere to put
 *     it" limit "Active controls only = Off" already has) and is inert
 *     while the master switch is off.
 *
 * Nothing above touches aldiopterrelevance.{h,cpp} (wave 1's exclusive
 * territory, AL_RELEVANCE_COUNT unchanged at 181), pipeline.{h,cpp}, or
 * llviewerfloaterreg.cpp -- every new interactive control is wired with a
 * plain setCommitCallback() in postBuild(), not a new named XUI
 * commit_callback.function registration, so nothing outside this class and
 * the new altoolfocuspick.{h,cpp} needed to change. See the wave-4 handoff
 * for the one deliberate scope trim: the dof-focus-live predicate this
 * wave needs for the "Subject" readout is a 4th local duplicate of the
 * expression already duplicated three times in pipeline.cpp (renderDoF,
 * renderUltimateDiopter, alKaleidoFocusUV) -- factoring all four into one
 * shared pipeline.cpp helper is out of this wave's named file list (same
 * judgment call wave 2 made for setToolTipIfChanged), and is flagged there
 * as deferred cleanup rather than done silently.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALFLOATERULTIMATEDIOPTER_H
#define AL_ALFLOATERULTIMATEDIOPTER_H

#include "llfloater.h"
#include "aldiopterrelevance.h"
#include "lluuid.h"
#include "v3dmath.h"       // LLVector3d -- onFocusPickCommitted()'s point_global param

#include <array>
#include <string>
#include <vector>

class LLButton;
class LLComboBox;
class LLControlVariable;
class LLFilterEditor;
class LLPanel;
class LLSliderCtrl;
class LLTextBox;
class LLUICtrl;

// One same-rect overlay-band child panel: shown when the band's pivot
// setting equals mMode, hidden otherwise (alpanelcinecamparams.cpp:511-524's
// ModeEntry idiom, generalized to 7 independent bands).
struct ALDiopterBandEntry
{
    U32         mMode;
    const char* mPanel;
};

// One band: which setting drives it, and the panels to switch between.
// mEntries is NOT one-per-mode-value -- a mode value with zero exclusive
// members (e.g. Shape = Full Frame, which has no shape-specific slider at
// all) has no entry and no panel; every OTHER band's panel simply goes
// invisible for that mode, matching ALPanelCineCamParams's own null-
// tolerant iteration.
struct ALDiopterBandSpec
{
    const char*              mPivotSetting;
    const ALDiopterBandEntry* mEntries;
    U32                       mEntryCount;
};

// ---------------------------------------------------------------------------
// Phase 4 -- the eyedropper (§5.2).
// ---------------------------------------------------------------------------

// Which depth control (if any) the ALToolFocusPick singleton is currently
// armed for on THIS floater's behalf. AL_PICK_NONE both before arming and
// once a commit/cancel has been processed -- see onFocusPick{Committed,
// Preview}(), which ignore any callback whose target no longer matches.
enum ALDiopterPickTarget
{
    AL_PICK_NONE = 0,
    AL_PICK_BASE,
    AL_PICK_LENS,
    AL_PICK_KAL_DEPTH_CUT,
};

// ---------------------------------------------------------------------------
// Phase 4 -- per-control range combos (§5.3e). "Range tables must be
// per-control, not one shared table" -- each metre slider's ladder is
// capped at THAT control's own renderer-governed ceiling, never a shared
// Macro/Room/Set/Vista guess.
// ---------------------------------------------------------------------------
struct ALDiopterRangeEntry
{
    const char* mLabel;
    F32         mMin;
    F32         mMax;
    F32         mIncrement;
    S32         mDecimalDigits;
};

// One metre slider that gets a range combo: which XUI names to resolve,
// which gSavedSettings key to read/write directly (the slider is also
// control_name-bound to the same key; going through gSavedSettings matches
// how every other write in this class already works), and which ladder
// governs it. mHardMin/mHardMax are the control's OWN absolute ceiling
// (§5.3e's table) -- NOT necessarily the widest range entry's own bounds,
// though today they coincide for all four controls.
struct ALDiopterRangeSpec
{
    const char*                mSliderName;
    const char*                mComboName;
    const char*                mSettingName;
    F32                        mHardMin;
    F32                        mHardMax;
    const ALDiopterRangeEntry* mEntries;
    U32                        mEntryCount;
};

// AL_RANGE_BASE and AL_RANGE_LENS share the identical ladder (§5.3e's
// "Base / Lens Distance (cap 128)" column) but are independent slots --
// each slider gets its own auto-selected range and its own single-level
// undo shadow.
enum ALDiopterRangeSlot
{
    AL_RANGE_BASE = 0,
    AL_RANGE_LENS,
    AL_RANGE_KAL_DEPTH_CUT,
    AL_RANGE_KAL_DEPTH_FEATHER,
    AL_RANGE_SLOT_COUNT,
};

// ---------------------------------------------------------------------------
// Codex round-2 CR2-7 -- a fixed, allocation-free replacement for the old
// std::string-concatenation signature. Every DISCRETE state field that a
// clause's mask lookup can read (alDiopterStateValue, aldiopterrelevance.cpp)
// is compared exactly, as its own native type -- no string round-trip, so no
// rounding-induced aliasing. Every CONTINUOUS (float) ALDiopterState field is
// read ONLY through the 12 named predicates (alDiopterEvalPred) -- confirmed
// by reading alDiopterStateValue/alDiopterEvalClause/alDiopterEvalPred
// directly, not assumed -- so folding each predicate's CURRENT boolean
// outcome into one bit of mPredBits is both sufficient (nothing that can
// move relevance is left out) and necessary: comparing the raw floats
// directly would force a refresh EVERY frame "Camera focus point" is
// selected, since mResolvedBaseFocusM changes continuously while the camera
// moves, even though the predicates it feeds (ABERRATION/FIELD_CURVE/
// LENS_POWERED, via strength_d) only flip at an actual zero-crossing.
struct ALDiopterSigKey
{
    bool mEnabled = false;
    U32  mToolMode = 0;

    U32  mShape = 0;
    U32  mPlacementMode = 0;
    U32  mContent = 0;
    U32  mGlassProfile = 0;
    U32  mApertureShape = 0;
    U32  mPatternMode = 0;
    U32  mMotionMode = 0;
    U32  mPulseTarget = 0;
    U32  mSpinMode = 0;
    U32  mFocusMode = 0;
    U32  mLensFocusMode = 0;
    bool mFreeze = false;

    U32  mKalMode = 0;
    U32  mKalMotionMode = 0;
    U32  mKalPulseTarget = 0;
    U32  mKalSpinMode = 0;
    U32  mKalProtectMode = 0;
    U32  mKalProtectAnchor = 0;
    bool mKalFreeze = false;

    U32  mDiopterPreset = 0;
    U32  mKalPreset = 0;
    bool mActiveOnly = false;

    // Bit (p - PRED_WARP_ARMED) of alDiopterEvalPred(p, st), for every real
    // predicate p in [PRED_WARP_ARMED, AL_PRED_COUNT) -- 12 predicates today,
    // comfortably inside a U16.
    U16  mPredBits = 0;

    bool operator==(const ALDiopterSigKey& o) const
    {
        return mEnabled == o.mEnabled && mToolMode == o.mToolMode &&
               mShape == o.mShape && mPlacementMode == o.mPlacementMode &&
               mContent == o.mContent && mGlassProfile == o.mGlassProfile &&
               mApertureShape == o.mApertureShape &&
               mPatternMode == o.mPatternMode && mMotionMode == o.mMotionMode &&
               mPulseTarget == o.mPulseTarget && mSpinMode == o.mSpinMode &&
               mFocusMode == o.mFocusMode && mLensFocusMode == o.mLensFocusMode &&
               mFreeze == o.mFreeze &&
               mKalMode == o.mKalMode && mKalMotionMode == o.mKalMotionMode &&
               mKalPulseTarget == o.mKalPulseTarget && mKalSpinMode == o.mKalSpinMode &&
               mKalProtectMode == o.mKalProtectMode &&
               mKalProtectAnchor == o.mKalProtectAnchor && mKalFreeze == o.mKalFreeze &&
               mDiopterPreset == o.mDiopterPreset && mKalPreset == o.mKalPreset &&
               mActiveOnly == o.mActiveOnly && mPredBits == o.mPredBits;
    }
    bool operator!=(const ALDiopterSigKey& o) const { return !(*this == o); }
};

class ALFloaterUltimateDiopter final : public LLFloater
{
public:
    ALFloaterUltimateDiopter(const LLSD& key);
    ~ALFloaterUltimateDiopter() override = default;

    bool postBuild() override;
    void draw() override;
    void onOpen(const LLSD& key) override;
    // Phase 4: cancels any in-flight ALToolFocusPick arm belonging to this
    // floater before the base close -- the LLHandle capture in
    // armFocusPick()'s callbacks already makes a stale callback safe (it
    // simply no-ops), so this is cleanliness (hands the camera back
    // immediately) rather than a correctness requirement.
    void onClose(bool app_quitting) override;

private:
    // One entry per sRelevance[] row, resolved once in postBuild(). A null
    // mCtrl means the row's control was not found against the CURRENT xml
    // -- logged once and the row is simply skipped on every later refresh
    // (never crashes, never retries the lookup).
    struct RowWidgets
    {
        LLUICtrl*          mCtrl = nullptr;
        LLButton*          mReset = nullptr;   // stays null for diopter_enabled (no reset)
        std::string        mDefaultCtrlTip;    // authored XML tool_tip, restored when relevant
        std::string        mDefaultResetTip;
        // Phase 5 -- cached once so the per-frame non-default tint pass
        // (updateResetIndicators()) needs no per-row gSavedSettings lookup.
        LLControlVariable* mControl = nullptr;
        bool               mLastIsDefault = true;
    };

    // Phase 4 -- one range-combo-governed slider's live widgets plus its
    // single-level "undo" shadow (§5.3e). See onRangeComboCommit().
    struct RangeSlotState
    {
        LLSliderCtrl* mSlider = nullptr;
        LLComboBox*   mCombo = nullptr;
        bool          mHasShadow = false;    // a prior range change clamped a value away
        F32           mShadowValue = 0.f;    // ...and this was the pre-clamp value
        F32           mLastClampedTo = 0.f;  // what we wrote; invalidates the shadow if the
                                              // slider no longer holds this (a manual edit happened)
    };

    // Reads the 18 term-driving settings + 14 scalar fields + enabled/tool/
    // provenance/resolved-base into a fresh ALDiopterState, applying the
    // Content effective-value coercion the wave-1 handoff recommends
    // (On-Lens forces content to Diopter in the renderer too,
    // pipeline.cpp:13977-13982).
    void populateState(ALDiopterState& st) const;

    // Cheap change guard (design doc §3.5): every value that can move any
    // row's relevance, preset-dimming, band-panel selection or hide/dim
    // treatment, packed into a fixed-size POD key (Codex round-2 CR2-7 --
    // see ALDiopterSigKey's own comment for why the 12-predicate bitset
    // replaces the 15 raw float fields the old std::string signature used
    // to stringify). Broadened beyond wave 2's original driver set by
    // Phase 3's "Active controls only" toggle, since flipping it changes
    // whether a failing TIER_MODE row hides or dims without any OTHER
    // driver moving. The 7 band pivot settings (Shape, ApertureShape,
    // MotionMode, SpinMode, KalMode, KalMotionMode, KalSpinMode) need no
    // separate driver -- all 7 are already discrete fields of the key.
    ALDiopterSigKey buildSigKey(const ALDiopterState& st, U32 diopter_preset,
                                 U32 kal_preset, bool active_only) const;

    // Walks sRelevance and applies TIER visibility/dim + preset-owned dim +
    // tooltips to every resolved row and its coupled reset button, then
    // switches every overlay band to the currently selected mode
    // (updateBandPanels()). No-op (cheap) unless the signature actually
    // moved, unless force is set (floater open).
    void refreshRelevance(bool force = false);

    // setVisible()/setEnabled() with the §3.8a focus-transfer rule: neither
    // call releases keyboard focus (llview.cpp), so a control about to go
    // dark or disappear that currently holds focus (self or a descendant)
    // hands focus to the floater itself first. The floater is always a
    // valid target (Phase 3 never hides the floater itself); see the
    // header comment / handoff for why a per-section target was not built.
    void setRowVisualState(LLUICtrl* ctrl, bool visible, bool enabled);

    // Applies the combined visible/enabled/tooltip state to one row's
    // control and its coupled reset button (§2.3: "reset buttons live in
    // the row" -- visibility, enablement and tooltip all travel together).
    void applyRowState(S32 row_index, bool visible, bool enabled,
                       const std::string& explain_tip);

    // True for a TIER_MODE row whose relevance clauses can fail for MORE
    // THAN ONE mode value of its band's pivot setting (a control physically
    // placed inside one mode's same-rect swap panel cannot ALSO appear in a
    // different mode's panel -- one XML parent only). Such a row stays
    // hidden regardless of "Active controls only"; see applyRowState()'s
    // caller in refreshRelevance() and the handoff's 39-name inventory.
    static bool isSwapNested(const char* ctrl_name);

    // alpanelcinecamparams.cpp:511-524's pattern, generalized over
    // bandSpecs(): for every band, read its pivot setting and show exactly
    // the one child panel whose mMode matches (hide every sibling). Called
    // unconditionally from refreshRelevance() -- cheap, and every pivot
    // setting was already one of wave 2's signature drivers.
    void updateBandPanels();
    static const std::vector<ALDiopterBandSpec>& bandSpecs();

    // §4.2 piece 2: a saved rect from before Phase 3 (min_height 420) would
    // otherwise sit below the new min_height (720) until the user next
    // resizes it by hand -- LLFloater::applyRectControl() already clamps a
    // restored rect up to the CURRENT mMinWidth/mMinHeight via llmax
    // (llfloater.cpp:1007), but that runs before postBuild() ever executes
    // and this is cheap insurance restated explicitly and version-gated, so
    // it runs (and logs) at most once per saved-rect schema bump rather
    // than silently relying on an engine behavior this class does not own.
    void migrateSavedRectIfNeeded();

    // The explanatory tooltip for a row whose clauses currently fail.
    // Differentiates the three tiers per §1.4/§3.2 (mode-excluded /
    // overridden / unarmed get different generic text), and returns the
    // verbatim §6.4 copy for the handful of controls the design doc
    // hand-authored text for.
    std::string tierTooltip(const ALDiopterRelevance& row) const;

    // The explanatory tooltip for a row that IS relevant but whose value is
    // currently owned by a non-Custom preset (§1.5/§6.3) -- an independent
    // dimming path from the tier system, per the table header's mTier note.
    std::string presetOwnedTooltip(U32 tool_mask) const;

    // CR2-9 follow-up -- the one-line preset status text §6.3's last
    // paragraph asks for under each preset combo ("Vintage Swirl -- edit
    // any control to make it yours"), populated into the two placeholder
    // <text> elements the concurrent XML fix added
    // (diopter_ui_preset_status / kal_ui_preset_status). Called from
    // refreshRelevance() -- both preset ids are already signature fields
    // (ALDiopterSigKey::mDiopterPreset/mKalPreset), so this needs no
    // separate driver.
    void updatePresetStatusText(U32 diopter_preset, U32 kal_preset);
    // Empty for Custom (id 0) -- §6.3's status-line copy is authored for
    // the active-preset case only; the doc specifies no Custom-state text,
    // so this deliberately shows nothing rather than inventing copy.
    static std::string formatPresetStatusText(LLComboBox* combo, U32 preset_id);

    // -----------------------------------------------------------------
    // Phase 4 -- the eyedropper (§5.2).
    // -----------------------------------------------------------------

    // Arms ALToolFocusPick for `target`, capturing an LLHandle to this
    // floater (not a raw `this`) in both callbacks so a closed/destroyed
    // floater makes a late callback a safe no-op rather than a dangling
    // pointer. No-ops (silently) if another owner already has the tool
    // armed -- the pick buttons are not disabled while inert because the
    // ONLY other possible owner is this same floater's own prior arm, which
    // arm()'s owner-match check already lets through cleanly.
    void armFocusPick(ALDiopterPickTarget target);
    void onFocusPickCommitted(ALDiopterPickTarget target, bool accepted,
                               F32 depth_m, const LLVector3d& point_global);
    void onFocusPickPreview(ALDiopterPickTarget target, F32 depth_m);

    // "Set From Camera Focus" (§5.3b) -- beside Base Distance only (Lens
    // Focus has no camera-tracking mode to mirror). Reuses the identical
    // alDiopterResolveBaseFocus() ladder the eyedropper's readout already
    // calls; if the ladder cannot resolve a target, the slider is left
    // untouched rather than writing a meaningless fallback value.
    void onSetFromCameraFocusClicked();

    // Base-focus / Subject readouts (§5.3a), refreshed unconditionally every
    // draw() (NOT signature-guarded -- these must react to camera movement
    // every frame, independent of whether any relevance-driving value
    // moved). While a pick is in flight for a given control, its readout
    // shows the live throttled preview instead of the normal text.
    void updateFocusReadouts();
    static std::string formatBaseFocusReadout(ALDiopterFocusProvenance prov, F32 resolved_m);
    static std::string formatSubjectReadout(ALDiopterFocusProvenance prov, F32 resolved_m);
    static std::string formatPickingReadout(F32 preview_m);

    // -----------------------------------------------------------------
    // Phase 4 -- per-control range combos (§5.3e).
    // -----------------------------------------------------------------

    // postBuild(): resolves each slot's slider/combo, fills the combo with
    // its ladder's labels, wires its commit callback. Does NOT apply any
    // bounds yet -- syncRangesToCurrentValues() does that (called right
    // after, and again from onOpen()).
    void initRangeCombos();
    // "auto-select the range on open and after every eyedropper pick" --
    // chooses the narrowest containing rung for EVERY slot from its
    // CURRENT gSavedSettings value and applies it. No clamp is needed here
    // by construction (a fresh eyedropper write and the on-open read are
    // both already within [mHardMin, mHardMax]).
    void syncRangesToCurrentValues();
    // Same as above, but for exactly one slot (called after a pick commits
    // for that control specifically) with the value already known.
    void autoSelectRange(ALDiopterRangeSlot slot, F32 value);
    static S32 narrowestRangeIndex(const ALDiopterRangeEntry* entries, U32 count, F32 value);
    // Sets min/max/increment/precision on the slot's slider from the given
    // ladder rung. Never touches the underlying setting's value.
    void applyRangeBounds(ALDiopterRangeSlot slot, S32 range_index);
    // The range combo's own commit callback: clamp-and-commit-immediately
    // (§5.3e's mandatory policy), the single-level undo shadow, and the
    // inline "say so" status text when a clamp actually moves the value.
    void onRangeComboCommit(ALDiopterRangeSlot slot);
    void setRangeStatusText(ALDiopterRangeSlot slot, const std::string& text);

    // Codex round-2 CR2-3 -- ranges otherwise only resync at open/pick-
    // commit time, so a reset button, a preset materialize, or any other
    // external write to one of the four range-governed settings can leave
    // the runtime-applied bounds stale: the slider then silently clamps its
    // DISPLAY into the old range while the real gSavedSettings value sits
    // outside it (a "clamped lie"). Runs unconditionally every draw() (a
    // cheap 4-setting check): if a raw value has left the bounds currently
    // applied to its own slider, clamps it into the control's absolute
    // hard limits (mHardMin/mHardMax) if needed, auto-selects a containing
    // range, and clears any stale undo shadow / status text for that slot
    // (autoSelectRange() already does the latter two).
    void checkRangeStaleness();

    // -----------------------------------------------------------------
    // Phase 4 -- "Show Depth" toggle + RenderFocusPointCrosshair/Locked
    // (§5.3c/§5.3d). The crosshair/locked checkboxes are plain
    // control_name-bound XML, no C++ needed; only the shared Show-Depth
    // toggle (CineDiopterDebugView has no natural boolean shape) needs a
    // handler.
    // -----------------------------------------------------------------
    void onShowDepthToggled(LLUICtrl* ctrl);
    void updateShowDepthCheckboxes();

    // -----------------------------------------------------------------
    // Phase 5 -- polish, exactly §7's row.
    // -----------------------------------------------------------------

    // Non-default indicator on the 180 reset buttons: tints the SAME
    // "Refresh_Off" icon rather than swapping to a second, unverifiable
    // texture asset (this is a code-only wiring pass -- see the wave-4
    // handoff). Runs unconditionally every draw(), NOT signature-guarded --
    // a plain slider edit does not necessarily move the ALDiopterSigKey
    // relevance key, so gating this on that key would leave the indicator
    // stale after the overwhelming majority of ordinary edits.
    void updateResetIndicators();

    // LLFilterEditor commit callback (fires per-keystroke, matching
    // LLFloaterSettingsDebug's own filter_input precedent) and the per-row
    // match test it drives, both '+'-separated-AND-tokens/lowercase-
    // substring, mirrored from llfloatersettingsdebug.cpp's
    // setSearchFilter()/matchesSearchFilter().
    void onFilterChanged(const LLSD& value);
    bool matchesFilter(const ALDiopterRelevance& row, const RowWidgets& w) const;

    std::array<RowWidgets, AL_RELEVANCE_COUNT> mRows;
    ALDiopterSigKey mRelevanceSig;

    // Cached so presetOwnedTooltip() can name the active preset without a
    // second findChild() per refresh.
    LLComboBox* mDiopterPresetCombo = nullptr;
    LLComboBox* mKalPresetCombo = nullptr;

    // CR2-9 follow-up -- the concurrent XML fix's two placeholder status
    // texts, one per tool's preset combo (Main tab / Kaleido tab).
    LLTextBox* mDiopterPresetStatus = nullptr;
    LLTextBox* mKalPresetStatus = nullptr;

    // Phase 4 -- eyedropper state. mPickOwnerId is generated once
    // (constructor) and never changes; this floater is single_instance, so
    // there is exactly one possible owner for the lifetime of the process.
    LLUUID              mPickOwnerId;
    ALDiopterPickTarget mActivePick = AL_PICK_NONE;
    F32                 mPickPreviewM = 0.f;

    LLTextBox* mBaseFocusReadout = nullptr;   // Focus tab, beside Base Distance
    LLTextBox* mLensPickReadout = nullptr;    // Focus tab, beside Lens Distance (preview-only)
    LLTextBox* mKalSubjectReadout = nullptr;  // Kal Anim tab, beside Depth Cut

    LLUICtrl* mShowDepthFocus = nullptr;      // Focus tab checkbox
    LLUICtrl* mShowDepthKal = nullptr;        // Kal Anim tab checkbox

    std::array<RangeSlotState, AL_RANGE_SLOT_COUNT> mRangeSlots;
    LLTextBox* mFocusRangeStatus = nullptr;   // shared by AL_RANGE_BASE/AL_RANGE_LENS
    LLTextBox* mKalRangeStatus = nullptr;     // shared by the two Kal Anim range slots

    // Phase 5 -- filter box state.
    std::string mFilterText;
    std::vector<std::string> mFilterTokens;
};

// §4.2: bump whenever min_height/min_width grow enough that an old saved
// rect could sit below the new floor. 1 == the Phase 3 regroup (520x420 ->
// 560x720, min_height measured at 720 -- see wave3_handoff.md).
constexpr U32 AL_DIOPTER_UI_RECT_VERSION = 1;

#endif // AL_ALFLOATERULTIMATEDIOPTER_H
