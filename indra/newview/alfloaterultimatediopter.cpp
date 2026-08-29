/**
 * @file alfloaterultimatediopter.cpp
 * @brief [Ultimate Diopter] Phase 3 smart-UI floater -- see
 *        alfloaterultimatediopter.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloaterultimatediopter.h"

#include "aldiopterpresetbank.h" // ALScopedPresetMaterializing, AL_EXIT_AUTO_AFTER_EDIT (CR2-2)
#include "altoolfocuspick.h"     // Phase 4 -- the eyedropper
#include "llbutton.h"
#include "llcombobox.h"
#include "llcontrol.h"           // LLControlVariable::isDefault() -- Phase 5 reset tint
#include "llfiltereditor.h"      // Phase 5 -- filter box
#include "llfontgl.h"            // LLFontGL::HCENTER -- setImageOverlay()
#include "llpanel.h"             // overlay band child panels
#include "llsliderctrl.h"        // Phase 4 -- range-combo-governed sliders
#include "lltextbox.h"           // Phase 4 -- readout / range-status strips
#include "lltoolmgr.h"           // LLToolMgr::inBuildMode() -- dof-focus-live predicate
#include "lluictrl.h"
#include "llviewercontrol.h"     // gSavedSettings, LLCachedControl
#include "pipeline.h"            // LLPipeline::sDiopterBaseFocus{Provenance,ResolvedM},
                                 // sLastFocusPoint, RenderDepthOfField, alDiopterResolveBaseFocus()
#include "v4color.h"             // LLColor4 -- Phase 5 reset-button tint

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace
{
// Local copy of the fork's setToolTipIfChanged idiom (also used by the
// Director Console). Keep it here so the lens controls remain self-contained.
void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}

// Same dedupe idiom as setToolTipIfChanged(), for the Phase 4 readout /
// range-status LLTextBox strips -- these are refreshed unconditionally
// every draw() (not signature-guarded), so avoiding a redundant setText()
// call on the overwhelming majority of frames where nothing changed matters
// more here than it does for the once-per-signature-change tooltips above.
void setTextBoxIfChanged(LLTextBox* box, const std::string& text)
{
    if (box && box->getText() != text)
    {
        box->setText(text);
    }
}

// design doc §1.4 Tier 0 -- the master-enable gate. Every non-unconditional
// row gets this SAME text while the master switch is off, rather than its
// usual tier-specific explanation, so 180 controls don't each claim an
// unrelated reason ("not part of this mode") when the real reason is simply
// that the effect is off. No new widget: the existing "diopter_enabled"
// checkbox IS the affordance the text points at.
constexpr char MASTER_DISABLED_TIP[] =
    "Turn on \"Enable Ultimate Diopter\" above to use this control.";

// §6.4 verbatim tooltip copy for the controls the design doc hand-authored
// text for. Keyed by mCtrlName; every other row falls back to the
// tier-differentiated generic text in tierTooltip() below.
struct NamedTip { const char* mCtrlName; const char* mTip; };
constexpr NamedTip NAMED_TIPS[] =
{
    { "diopter_base_m",
      "Overridden -- Base Focus is tracking the camera focus subject" },
    { "diopter_ghost_spacing",
      "Raise Ghost Copies above 0 to use this" },
    { "diopter_ring_count",
      "Needs a warp: raise Ring Fold, Twist, Lobe Amount, or Source Zoom, or turn on Faceted Fold" },
    { "diopter_size",
      "On-Lens glass fills the frame -- Size is not used. Pulse: Size and Stutter Size still breathe the falloff." },
    { "diopter_wave_amp",
      "Wave undulates the glass edge -- Full Frame and On-Lens have no edge to undulate. Pick a framed shape to use it." },
    { "diopter_wobble_freq",
      "Sets how tight the edge waves are. Raise Edge Wobble, or set Motion to Wave, to use it." },
};

// ---------------------------------------------------------------------------
// Phase 3 -- same-rect overlay band tables (design doc §3.3/§3.6).
//
// One entry per (pivot value, panel) pair that floater_ultimate_diopter.xml
// actually authored -- a mode value with zero exclusive members (nothing
// only-that-mode uses) has no panel and no entry at all; every band's
// updateBandPanels() pass simply leaves every OTHER panel hidden for it,
// matching alpanelcinecamparams.cpp:511-524's own null-tolerant iteration.
// Panel names follow the band_<prefix>_<mode> scheme documented in
// scratchpad/wave3_handoff.md, generated mechanically from the XML build
// (never hand-typed against a diverging source of truth).
// ---------------------------------------------------------------------------
constexpr ALDiopterBandEntry BAND_SHAPE[] = {
    { 1, "band_shape_1" }, { 5, "band_shape_5" }, { 6, "band_shape_6" },
    { 7, "band_shape_7" }, { 9, "band_shape_9" }, { 10, "band_shape_10" },
    { 11, "band_shape_11" },
};
constexpr ALDiopterBandEntry BAND_BOKEH_AP[] = {
    { 2, "band_bokeh_ap_2" }, { 4, "band_bokeh_ap_4" },
};
constexpr ALDiopterBandEntry BAND_MOTION[] = {
    { 1, "band_motion_1" }, { 2, "band_motion_2" }, { 3, "band_motion_3" },
    { 4, "band_motion_4" }, { 5, "band_motion_5" },
};
constexpr ALDiopterBandEntry BAND_SPIN[] = {
    { 0, "band_spin_0" }, { 3, "band_spin_3" },
};
constexpr ALDiopterBandEntry BAND_KAL_PATTERN[] = {
    { 0, "band_kal_pattern_0" }, { 7, "band_kal_pattern_7" },
};
constexpr ALDiopterBandEntry BAND_KAL_MOTION[] = {
    { 1, "band_kal_motion_1" }, { 2, "band_kal_motion_2" },
    { 3, "band_kal_motion_3" }, { 4, "band_kal_motion_4" },
};
constexpr ALDiopterBandEntry BAND_KAL_SPIN[] = {
    { 0, "band_kal_spin_0" }, { 3, "band_kal_spin_3" },
};

#define BAND(setting, table) \
    { setting, table, (U32)(sizeof(table) / sizeof(table[0])) }

const std::vector<ALDiopterBandSpec> BAND_SPECS = {
    BAND("CineDiopterShape",          BAND_SHAPE),
    BAND("CineDiopterApertureShape",  BAND_BOKEH_AP),
    BAND("CineDiopterMotionMode",     BAND_MOTION),
    BAND("CineDiopterSpinMode",       BAND_SPIN),
    BAND("CineDiopterKalMode",        BAND_KAL_PATTERN),
    BAND("CineDiopterKalMotionMode",  BAND_KAL_MOTION),
    BAND("CineDiopterKalSpinMode",    BAND_KAL_SPIN),
};

#undef BAND

// The 39 controls physically nested inside one of the panels above -- a
// control used by more than one value of its band's pivot cannot be one of
// these (it would need two XML parents), so these are exactly the rows
// "Active controls only = Off" can NEVER reveal: their panel is hidden
// whenever a different mode is selected, and there is nowhere else on
// screen to show them (design doc §5.2's Star-Points-under-Circle example,
// generalized to every band). Generated from the same XML build as the
// tables above -- see scratchpad/wave3_handoff.md.
const std::unordered_set<std::string> SWAP_NESTED_CTRLS = {
    // Shape-specific (band_shape_*)
    "diopter_split_curve", "diopter_squircle_pow", "diopter_poly_sides",
    "diopter_star_points", "diopter_star_inner", "diopter_petal_count",
    "diopter_petal_depth", "diopter_blob_seed", "diopter_blob_amt",
    "diopter_crescent_bite", "diopter_crescent_shift",
    // Bokeh aperture (band_bokeh_ap_*)
    "diopter_ap_inner", "diopter_anamorph", "diopter_anam_angle",
    // Motion (band_motion_*)
    "diopter_ping_pong", "diopter_pulse_target", "diopter_wave_amp",
    "diopter_path_fx", "diopter_path_fy", "diopter_path_phase",
    "diopter_stutter_rate", "diopter_stutter_pos", "diopter_stutter_angle",
    "diopter_stutter_size", "diopter_stutter_focus", "diopter_stutter_smooth",
    // Diopter Spin (band_spin_*)
    "diopter_spin_speed", "diopter_spin_bounce",
    // Kaleido pattern (band_kal_pattern_*)
    "kal_seam_soften", "kal_star_sharp",
    // Kal Anim motion (band_kal_motion_*)
    "kal_ping_pong", "kal_pulse_target", "kal_wave_amp", "kal_wave_freq",
    "kal_path_fx", "kal_path_fy", "kal_path_phase",
    // Kal Anim Spin (band_kal_spin_*)
    "kal_spin_speed", "kal_spin_bounce",
};

// Codex round-2 CR2-7 -- the 12 named predicates (aldiopterrelevance.h
// ALDiopterPred), folded into one U16 bitset: bit (p - PRED_WARP_ARMED) is
// alDiopterEvalPred(p, st). This is the ONLY place continuous (float)
// ALDiopterState fields feed the relevance-refresh signature -- confirmed
// by reading alDiopterStateValue/alDiopterEvalClause/alDiopterEvalPred
// directly: every mask-based clause term reads a DISCRETE field via
// alDiopterStateValue, never a float, so a float can only move a clause's
// outcome by moving a predicate's boolean result.
U16 computePredicateBits(const ALDiopterState& st)
{
    U16 bits = 0;
    for (S32 p = PRED_WARP_ARMED; p < (S32)AL_PRED_COUNT; ++p)
    {
        if (alDiopterEvalPred((ALDiopterPred)p, st))
        {
            bits |= (U16)(1u << (p - PRED_WARP_ARMED));
        }
    }
    return bits;
}

// ---------------------------------------------------------------------------
// Phase 4 -- per-control range ladders (§5.3e). Each capped at THAT
// control's own renderer-governed ceiling -- never a shared guess (§5.3e's
// own "a shared ladder would offer values the renderer clamps away"
// warning, e.g. Depth Feather's 32 m cap living in pipeline.cpp, not the
// XML).
// ---------------------------------------------------------------------------
constexpr ALDiopterRangeEntry RANGE_BASE_LENS[] = {
    { "Macro", 0.1f, 2.f,   0.01f, 2 },
    { "Room",  0.5f, 15.f,  0.05f, 2 },
    { "Set",   5.f,  60.f,  0.25f, 1 },
    { "Vista", 20.f, 128.f, 0.5f,  1 },
};
constexpr ALDiopterRangeEntry RANGE_KAL_DEPTH_CUT[] = {
    { "Near",  0.1f, 4.f,   0.02f, 2 },
    { "Room",  1.f,  20.f,  0.1f,  1 },
    { "Set",   10.f, 80.f,  0.5f,  1 },
    { "Vista", 50.f, 256.f, 1.0f,  0 },
};
constexpr ALDiopterRangeEntry RANGE_KAL_DEPTH_FEATHER[] = {
    { "Tight", 0.05f, 1.f,  0.01f, 2 },
    { "Soft",  0.5f,  6.f,  0.05f, 2 },
    { "Wide",  2.f,   32.f, 0.25f, 1 },
};

// AL_RANGE_BASE / AL_RANGE_LENS share RANGE_BASE_LENS but remain independent
// slots (independent combo, independent single-level undo shadow).
const ALDiopterRangeSpec RANGE_SPECS[AL_RANGE_SLOT_COUNT] = {
    { "diopter_base_m",    "diopter_base_range",     "CineDiopterBaseFocusM",
      0.1f, 128.f, RANGE_BASE_LENS,        (U32)(sizeof(RANGE_BASE_LENS) / sizeof(RANGE_BASE_LENS[0])) },
    { "diopter_lens_m",    "diopter_lens_range",     "CineDiopterLensFocusM",
      0.1f, 128.f, RANGE_BASE_LENS,        (U32)(sizeof(RANGE_BASE_LENS) / sizeof(RANGE_BASE_LENS[0])) },
    { "kal_depth_cut",     "kal_depth_cut_range",    "CineDiopterKalDepthCut",
      0.1f, 256.f, RANGE_KAL_DEPTH_CUT,    (U32)(sizeof(RANGE_KAL_DEPTH_CUT) / sizeof(RANGE_KAL_DEPTH_CUT[0])) },
    { "kal_depth_feather", "kal_depth_feather_range","CineDiopterKalDepthFeatherM",
      0.05f, 32.f, RANGE_KAL_DEPTH_FEATHER, (U32)(sizeof(RANGE_KAL_DEPTH_FEATHER) / sizeof(RANGE_KAL_DEPTH_FEATHER[0])) },
};

// Phase 5 -- non-default reset-button tint. No new texture asset (this is a
// code-only wiring pass): the SAME "Refresh_Off" icon every reset button
// already authors, tinted, rather than a second "Refresh_On"-style asset
// that could not be added/verified in this pass.
const LLColor4 RESET_NON_DEFAULT_TINT(0.95f, 0.75f, 0.25f, 1.f);

// [Ultimate Diopter] §5.3a -- the dof-focus-live predicate, duplicated a 4th
// time here (renderDoF, renderUltimateDiopter and alKaleidoFocusUV already
// carry the first three, per the design doc's own count). Kept local and
// NOT factored into a shared pipeline.cpp helper in this wave: doing so
// would mean editing renderDoF/renderUltimateDiopter/alKaleidoFocusUV's own
// bodies, and pipeline.cpp/.h are not in this wave's named Phase 4 file
// list. Flagged as deferred cleanup in the wave-4 handoff, the same
// local-helper judgment used for the UI tooltip helpers. Must stay
// byte-for-byte identical to pipeline.cpp's three copies (RenderDepthOfField,
// RenderDepthOfFieldInEditMode, LLToolMgr::inBuildMode(), sLastFocusPoint).
bool alDiopterUiDofFocusLive()
{
    static LLCachedControl<bool> dof_in_edit_mode(
        gSavedSettings, "RenderDepthOfFieldInEditMode", false);
    return LLPipeline::RenderDepthOfField &&
           (dof_in_edit_mode || !LLToolMgr::getInstance()->inBuildMode()) &&
           !LLPipeline::sLastFocusPoint.isExactlyZero();
}

} // namespace

ALFloaterUltimateDiopter::ALFloaterUltimateDiopter(const LLSD& key)
    : LLFloater(key)
{
    // Phase 4 -- a stable per-instance token for ALToolFocusPick's owner
    // check. This floater is single_instance, so there is only ever one
    // possible owner for the process lifetime; a fresh UUID (rather than a
    // hardcoded constant) sidesteps any need to reason about collisions
    // with some other future ALToolFocusPick consumer.
    mPickOwnerId = LLUUID::generateNewID();
}

bool ALFloaterUltimateDiopter::postBuild()
{
    mDiopterPresetCombo = findChild<LLComboBox>("diopter_preset");
    mKalPresetCombo = findChild<LLComboBox>("kal_preset");
    // CR2-9 follow-up -- the concurrent XML fix's two placeholder status
    // texts (§6.3's last paragraph), one per tool.
    mDiopterPresetStatus = findChild<LLTextBox>("diopter_ui_preset_status");
    mKalPresetStatus = findChild<LLTextBox>("kal_ui_preset_status");

    S32 missing_ctrl = 0;
    S32 missing_reset = 0;
    for (S32 i = 0; i < AL_RELEVANCE_COUNT; ++i)
    {
        const ALDiopterRelevance& row = sRelevance[i];
        RowWidgets& w = mRows[i];

        w.mCtrl = findChild<LLUICtrl>(row.mCtrlName);
        if (!w.mCtrl)
        {
            LL_WARNS("UltimateDiopter") << "sRelevance row " << i << " ('"
                << row.mCtrlName << "') has no matching control in "
                << "floater_ultimate_diopter.xml -- gating skipped for this "
                << "row this session" << LL_ENDL;
            ++missing_ctrl;
            continue;
        }
        w.mDefaultCtrlTip = w.mCtrl->getToolTip();
        // Phase 5 -- cached once for updateResetIndicators(); every row's
        // control is control_name-bound, so getControlVariable() always
        // resolves for a successfully-found w.mCtrl.
        w.mControl = w.mCtrl->getControlVariable();

        if (row.mResetName)
        {
            w.mReset = findChild<LLButton>(row.mResetName);
            if (!w.mReset)
            {
                LL_WARNS("UltimateDiopter") << "sRelevance row " << i << " ('"
                    << row.mCtrlName << "') reset button '" << row.mResetName
                    << "' not found -- reset-button gating skipped for this "
                    << "row this session" << LL_ENDL;
                ++missing_reset;
            }
            else
            {
                w.mDefaultResetTip = w.mReset->getToolTip();
            }
        }
    }

    LL_INFOS("UltimateDiopter") << "ALFloaterUltimateDiopter::postBuild: "
        << (AL_RELEVANCE_COUNT - missing_ctrl) << "/" << AL_RELEVANCE_COUNT
        << " relevance rows resolved to live controls, "
        << (180 - missing_reset) << "/180 reset buttons resolved" << LL_ENDL;

    // ---------------------------------------------------------------------
    // Phase 4 -- eyedropper pick buttons. Plain setCommitCallback() wiring
    // (no XUI commit_callback.function registration needed -- these are
    // instance-specific handlers, unlike the app-lifetime
    // "Diopter.ResetControl" registered in llviewerfloaterreg.cpp).
    // ---------------------------------------------------------------------
    if (LLButton* btn = findChild<LLButton>("diopter_pick_base"))
    {
        btn->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { armFocusPick(AL_PICK_BASE); });
    }
    if (LLButton* btn = findChild<LLButton>("diopter_pick_lens"))
    {
        btn->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { armFocusPick(AL_PICK_LENS); });
    }
    if (LLButton* btn = findChild<LLButton>("kal_pick_depth_cut"))
    {
        btn->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { armFocusPick(AL_PICK_KAL_DEPTH_CUT); });
    }
    if (LLButton* btn = findChild<LLButton>("diopter_set_from_camera"))
    {
        btn->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSetFromCameraFocusClicked(); });
    }

    mBaseFocusReadout = findChild<LLTextBox>("diopter_base_readout");
    mLensPickReadout = findChild<LLTextBox>("diopter_lens_readout");
    mKalSubjectReadout = findChild<LLTextBox>("kal_subject_readout");
    mFocusRangeStatus = findChild<LLTextBox>("diopter_range_status");
    mKalRangeStatus = findChild<LLTextBox>("kal_range_status");

    // ---------------------------------------------------------------------
    // Phase 4 -- "Show Depth" toggle (shared CineDiopterDebugView, no
    // control_name binding since 0/3 is not the setting's whole range).
    // ---------------------------------------------------------------------
    mShowDepthFocus = findChild<LLUICtrl>("diopter_show_depth");
    mShowDepthKal = findChild<LLUICtrl>("kal_show_depth");
    if (mShowDepthFocus)
    {
        mShowDepthFocus->setCommitCallback(
            [this](LLUICtrl* ctrl, const LLSD&) { onShowDepthToggled(ctrl); });
    }
    if (mShowDepthKal)
    {
        mShowDepthKal->setCommitCallback(
            [this](LLUICtrl* ctrl, const LLSD&) { onShowDepthToggled(ctrl); });
    }
    updateShowDepthCheckboxes();

    // ---------------------------------------------------------------------
    // Phase 4 -- per-control range combos (§5.3e). initRangeCombos() wires
    // the combos; syncRangesToCurrentValues() (also called from onOpen(),
    // per §5.3e's "auto-select on open") applies the narrowest containing
    // rung for each slider's CURRENT value so the first paint already shows
    // the right bounds rather than the XML's widest-fallback ones.
    // ---------------------------------------------------------------------
    initRangeCombos();
    syncRangesToCurrentValues();

    // ---------------------------------------------------------------------
    // Phase 5 -- filter box. Mirrors LLFloaterSettingsDebug's own
    // filter_input wiring (llfloatersettingsdebug.cpp ~71): fires on every
    // keystroke, not just commit, via LLFilterEditor/LLSearchEditor's own
    // handleKeystroke() -> commit-signal path.
    // ---------------------------------------------------------------------
    if (LLFilterEditor* filter = findChild<LLFilterEditor>("diopter_filter"))
    {
        filter->setCommitCallback(
            [this](LLUICtrl*, const LLSD& value) { onFilterChanged(value); });
    }

    // Force the first paint to be correct rather than waiting for a
    // signature change that will never come if the state matches the
    // struct's factory defaults exactly.
    refreshRelevance(/*force=*/true);
    updateFocusReadouts();
    updateResetIndicators();
    return true;
}

void ALFloaterUltimateDiopter::onOpen(const LLSD& key)
{
    // §4.2 piece 2 -- runs before the relevance refresh so a just-migrated
    // rect is what the rest of open-time work sees. postBuild() runs
    // BEFORE LLFloater::applyRectControl() (llfloater.cpp ~3690-3699), so
    // this cannot live there; onOpen() is the earliest point at which
    // getRect() reflects the actually-restored (and engine-clamped) rect.
    migrateSavedRectIfNeeded();

    // "Refresh triggers: floater open" (design doc §7 Phase 2 row) --
    // gSavedSettings may have changed while this single_instance floater
    // was closed (Debug Settings, a scene restore, another instance), and
    // draw() was not running to pick it up. Force one rebuild on open.
    refreshRelevance(/*force=*/true);

    // §5.3e -- "auto-select the range on open", the other of the two
    // triggers the policy names (the other is every eyedropper pick,
    // handled directly in onFocusPickCommitted()).
    syncRangesToCurrentValues();
    updateFocusReadouts();
    updateResetIndicators();

    LLFloater::onOpen(key);
}

void ALFloaterUltimateDiopter::onClose(bool app_quitting)
{
    ALToolFocusPick::getInstance()->cancelForOwner(mPickOwnerId);
    mActivePick = AL_PICK_NONE;
    LLFloater::onClose(app_quitting);
}

void ALFloaterUltimateDiopter::migrateSavedRectIfNeeded()
{
    if (gSavedSettings.getU32("CineDiopterUIRectVersion") >= AL_DIOPTER_UI_RECT_VERSION)
    {
        return;
    }

    // LLFloater::applyRectControl() already clamps a restored rect up to
    // the CURRENT mMinWidth/mMinHeight via llmax (llfloater.cpp:1007), so
    // in practice this is a no-op the very first time it runs after this
    // version bump -- it exists so the migration is explicit, logged and
    // version-gated (matching the design doc's own request) rather than
    // relying silently on an engine behavior this class does not own, and
    // so a FUTURE min_height increase has an obvious place to add another
    // clamp without re-deriving the ordering constraint above.
    const S32 min_w = getMinWidth();
    const S32 min_h = getMinHeight();
    const LLRect& r = getRect();
    const S32 w = llmax(r.getWidth(), min_w);
    const S32 h = llmax(r.getHeight(), min_h);
    if (w != r.getWidth() || h != r.getHeight())
    {
        LL_INFOS("UltimateDiopter") << "ALFloaterUltimateDiopter: migrating a "
            "pre-Phase-3 saved rect (" << r.getWidth() << "x" << r.getHeight()
            << ") up to the new minimum (" << w << "x" << h << ")" << LL_ENDL;
        reshape(w, h);
    }
    gSavedSettings.setU32("CineDiopterUIRectVersion", AL_DIOPTER_UI_RECT_VERSION);
}

void ALFloaterUltimateDiopter::draw()
{
    // [Ultimate Diopter] refresh mechanism choice (design doc §7 Phase 2
    // row: "a single control-group signal or per-frame draw() check; pick
    // the house-idiomatic cheap option and justify"): per-frame draw()
    // check, following LLFloaterDirector::draw() (llfloaterdirector.cpp
    // ~552-565) and ALPanelCineCamParams's own §3.5-cited idiom -- refresh
    // functions run unconditionally every frame the floater is visible, and
    // are individually cheap because each guards on a signature string.
    //
    // Rejected: ~36 LLControlVariable::getSignal() listeners (one per
    // driving setting, the design doc's OTHER suggested option). It would
    // need connection bookkeeping (a std::vector<connection> plus a
    // destructor to disconnect them) for zero behavioural gain here: this
    // floater is a bool "single_instance" LLFloater whose draw() already
    // runs every frame it is visible, and staleness while genuinely
    // invisible (closed or minimized) does not matter -- onOpen() above
    // forces a rebuild the moment it becomes visible again. Per-frame +
    // signature-guard is simpler, has no dangling-connection risk across
    // repeated open/close of a single_instance floater, and is exactly the
    // mechanism the design doc's own §3.5 code sample shows for
    // refreshRelevance() specifically. The guard itself is a fixed POD key
    // (ALDiopterSigKey), not a string, as of Codex round-2 CR2-7 -- see
    // that struct's own comment.
    refreshRelevance();

    // Phase 4/5 -- unconditional per-frame passes, deliberately NOT folded
    // into refreshRelevance()'s signature guard: each of these must react
    // to something the relevance signature does not track -- camera
    // movement (focus readouts), any of 181 controls' raw value moving
    // without necessarily moving a relevance driver (non-default tint), a
    // live throttled pick preview (both readouts and, via
    // updateShowDepthCheckboxes(), the Show-Depth checkboxes' sync to
    // whatever CineDiopterDebugView holds right now), or a range-governed
    // setting drifting outside its currently-applied bounds via a reset or
    // external write (checkRangeStaleness(), Codex round-2 CR2-3). All five
    // are cheap (a handful of cached-pointer reads/compares plus a
    // dedupe-guarded setText/setValue), matching the signature-guarded
    // pass's own per-frame cost profile.
    updateFocusReadouts();
    updateShowDepthCheckboxes();
    updateResetIndicators();
    checkRangeStaleness();

    LLFloater::draw();
}

void ALFloaterUltimateDiopter::populateState(ALDiopterState& st) const
{
    // Every field is overwritten every refresh (wave-1 handoff: "the
    // struct's defaults are settings.xml factory defaults, not 'current'").
    st.mEnabled = gSavedSettings.getBOOL("CineDiopterEnabled");
    st.mToolMode = gSavedSettings.getU32("CineDiopterToolMode");

    st.mShape = gSavedSettings.getU32("CineDiopterShape");
    st.mPlacementMode = gSavedSettings.getU32("CineDiopterPlacementMode");
    st.mContent = gSavedSettings.getU32("CineDiopterContent");
    // [Ultimate Diopter] wave-1 handoff "Content coercion decision":
    // populate the EFFECTIVE value, matching the renderer's own coercion
    // (pipeline.cpp:13977-13982, "On-Lens + Sharp Window would flip the
    // full-frame mask to zero coverage -- on-lens glass is always Diopter
    // contents"). Rows 15/16 (Shape/Content combos) hide under On-Lens
    // regardless via their own Place=Framed clause, so this only affects
    // rows that test Content=Diopter directly while On-Lens is selected.
    if (st.mPlacementMode == 1U)
    {
        st.mContent = 0U;
    }
    st.mGlassProfile = gSavedSettings.getU32("CineDiopterGlassProfile");
    st.mApertureShape = gSavedSettings.getU32("CineDiopterApertureShape");
    st.mPatternMode = gSavedSettings.getU32("CineDiopterPatternMode");
    st.mMotionMode = gSavedSettings.getU32("CineDiopterMotionMode");
    st.mPulseTarget = gSavedSettings.getU32("CineDiopterPulseTarget");
    st.mSpinMode = gSavedSettings.getU32("CineDiopterSpinMode");
    st.mFocusMode = gSavedSettings.getU32("CineDiopterFocusMode");
    st.mLensFocusMode = gSavedSettings.getU32("CineDiopterLensFocusMode");
    st.mFreeze = gSavedSettings.getBOOL("CineDiopterFreeze");

    st.mKalMode = gSavedSettings.getU32("CineDiopterKalMode");
    st.mKalMotionMode = gSavedSettings.getU32("CineDiopterKalMotionMode");
    st.mKalPulseTarget = gSavedSettings.getU32("CineDiopterKalPulseTarget");
    st.mKalSpinMode = gSavedSettings.getU32("CineDiopterKalSpinMode");
    st.mKalProtectMode = gSavedSettings.getU32("CineDiopterKalProtectMode");
    st.mKalProtectAnchor = gSavedSettings.getU32("CineDiopterKalProtectAnchor");
    st.mKalFreeze = gSavedSettings.getBOOL("CineDiopterKalFreezeTime");

    st.mRingFold = gSavedSettings.getF32("CineDiopterRingFold");
    st.mTwistDeg = gSavedSettings.getF32("CineDiopterTwistDeg");
    st.mLobeAmt = gSavedSettings.getF32("CineDiopterLobeAmt");
    st.mPatternZoom = gSavedSettings.getF32("CineDiopterPatternZoom");
    st.mHandheld = gSavedSettings.getF32("CineDiopterHandheld");
    // handoff: "mGhostCount: cast the integer setting to F32 (predicate is > 0.f)"
    st.mGhostCount = (F32)gSavedSettings.getU32("CineDiopterGhostCount");
    st.mSeamPx = gSavedSettings.getF32("CineDiopterSeamGhostPx");
    st.mCharacter = gSavedSettings.getF32("CineDiopterCharacter");
    st.mFieldScale = gSavedSettings.getF32("CineDiopterFieldCurveScale");
    st.mWobbleAmt = gSavedSettings.getF32("CineDiopterWobbleAmt");
    st.mCellBreathe = gSavedSettings.getF32("CineDiopterKalCellBreathe");

    st.mPower = gSavedSettings.getF32("CineDiopterPower");
    st.mBaseFocusM = gSavedSettings.getF32("CineDiopterBaseFocusM");
    st.mLensFocusM = gSavedSettings.getF32("CineDiopterLensFocusM");

    // handoff: "mBaseFocusProvenance + mResolvedBaseFocusM: from the shared
    // resolver of swap-site 6 -- never re-derive in the floater."
    st.mBaseFocusProvenance = LLPipeline::sDiopterBaseFocusProvenance;
    st.mResolvedBaseFocusM = LLPipeline::sDiopterBaseFocusResolvedM;
}

ALDiopterSigKey ALFloaterUltimateDiopter::buildSigKey(const ALDiopterState& st,
                                                        U32 diopter_preset,
                                                        U32 kal_preset,
                                                        bool active_only) const
{
    // Codex round-2 CR2-7 -- fixed POD key, zero heap allocation (replaces
    // the old std::string built from ~40 std::to_string() calls every
    // frame). See ALDiopterSigKey's own comment (the header) for why the
    // 15 raw float ALDiopterState fields are represented ONLY through
    // computePredicateBits()'s 12-bit outcome rather than compared directly:
    // comparing them directly would force a refresh every single frame
    // "Camera focus point" tracking is live (mResolvedBaseFocusM changes
    // continuously with camera movement), even on frames where nothing
    // that could move relevance actually changed.
    ALDiopterSigKey key;

    key.mEnabled = st.mEnabled;
    key.mToolMode = st.mToolMode;

    key.mShape = st.mShape;
    key.mPlacementMode = st.mPlacementMode;
    key.mContent = st.mContent;
    key.mGlassProfile = st.mGlassProfile;
    key.mApertureShape = st.mApertureShape;
    key.mPatternMode = st.mPatternMode;
    key.mMotionMode = st.mMotionMode;
    key.mPulseTarget = st.mPulseTarget;
    key.mSpinMode = st.mSpinMode;
    key.mFocusMode = st.mFocusMode;
    key.mLensFocusMode = st.mLensFocusMode;
    key.mFreeze = st.mFreeze;

    key.mKalMode = st.mKalMode;
    key.mKalMotionMode = st.mKalMotionMode;
    key.mKalPulseTarget = st.mKalPulseTarget;
    key.mKalSpinMode = st.mKalSpinMode;
    key.mKalProtectMode = st.mKalProtectMode;
    key.mKalProtectAnchor = st.mKalProtectAnchor;
    key.mKalFreeze = st.mKalFreeze;

    // Broadened beyond the wave-1 handoff's original relevance-only
    // signature: the preset-ownership dimming path this floater also
    // drives is deliberately independent of sRelevance (table header's
    // mTier note), so its own two drivers must gate this refresh too.
    key.mDiopterPreset = diopter_preset;
    key.mKalPreset = kal_preset;

    // Phase 3: the one new driver -- flips whether a failing TIER_MODE row
    // hides or dims (§5.2/§6.2), independent of everything else above.
    key.mActiveOnly = active_only;

    key.mPredBits = computePredicateBits(st);

    return key;
}

void ALFloaterUltimateDiopter::setRowVisualState(LLUICtrl* ctrl, bool visible, bool enabled)
{
    if (!ctrl)
    {
        return;
    }
    // [Ultimate Diopter] §3.8a -- neither LLView::setEnabled(false) nor
    // setVisible(false) touches gFocusMgr (llview.cpp ~477-485, ~634-648),
    // so a director tabbed into a control that is about to dim OR disappear
    // would keep typing into a now-dark/invisible widget. Move focus to the
    // floater itself first -- always a valid target, since Phase 3 never
    // hides the floater itself, only child rows/panels within it. Only
    // fires on an actual losing-state transition while focus is inside;
    // "the floater itself" rather than a per-band mode combo is a
    // deliberate simplification over the doc's "target the control that
    // caused the change" recommendation -- see the class header comment
    // and scratchpad/wave3_handoff.md for why.
    const bool was_live = ctrl->getVisible() && ctrl->getEnabled();
    const bool losing_focus = was_live && (!visible || !enabled) && ctrl->hasFocus();
    if (losing_focus)
    {
        setFocus(true);
    }
    ctrl->setVisible(visible);
    ctrl->setEnabled(enabled);
}

void ALFloaterUltimateDiopter::applyRowState(S32 row_index, bool visible, bool enabled,
                                             const std::string& explain_tip)
{
    RowWidgets& w = mRows[row_index];
    if (!w.mCtrl)
    {
        return;
    }
    const bool live = visible && enabled;

    setRowVisualState(w.mCtrl, visible, enabled);
    setToolTipIfChanged(w.mCtrl, live ? w.mDefaultCtrlTip : explain_tip);

    // §2.3 / risk list: "every relevance row owns its sibling button's
    // visibility, enablement and tooltip" -- both travel with the row now
    // that Phase 3 hides.
    if (w.mReset)
    {
        setRowVisualState(w.mReset, visible, enabled);
        setToolTipIfChanged(w.mReset, live ? w.mDefaultResetTip : explain_tip);
    }
}

std::string ALFloaterUltimateDiopter::tierTooltip(const ALDiopterRelevance& row) const
{
    for (const NamedTip& nt : NAMED_TIPS)
    {
        if (strcmp(row.mCtrlName, nt.mCtrlName) == 0)
        {
            return nt.mTip;
        }
    }

    // design doc §3.2 -- tiers get different generic text when no
    // hand-authored copy exists for this specific control.
    switch (row.mTier)
    {
        case TIER_OVERRIDDEN:
            return "Overridden right now by the current mode or settings -- "
                   "your value is kept and takes effect again the moment "
                   "the override lifts.";
        case TIER_UNARMED:
            return "Not yet active -- turn on or raise whatever arms this "
                   "control first.";
        case TIER_MODE:
        default:
            return "Not part of the current mode, shape or placement. It "
                   "comes back if you switch back.";
    }
}

std::string ALFloaterUltimateDiopter::presetOwnedTooltip(U32 tool_mask) const
{
    LLComboBox* combo = (tool_mask & AL_TOOL_DIOPTER) ? mDiopterPresetCombo
                                                       : mKalPresetCombo;
    const std::string preset_name = combo ? combo->getSelectedItemLabel() : std::string();
    if (preset_name.empty())
    {
        return "Owned by the current preset -- edit any preset-owned control "
               "to make this Custom.";
    }
    return "Owned by preset '" + preset_name + "' -- edit any preset-owned "
           "control to make this Custom.";
}

// static
std::string ALFloaterUltimateDiopter::formatPresetStatusText(LLComboBox* combo, U32 preset_id)
{
    if (preset_id == 0)
    {
        // §6.3's last paragraph authors the status line for the ACTIVE-
        // preset case only ("Vintage Swirl -- edit any control to make it
        // yours"); it specifies no Custom-state copy, so this stays empty
        // rather than inventing text the spec doesn't call for.
        return std::string();
    }
    const std::string name = combo ? combo->getSelectedItemLabel() : std::string();
    if (name.empty())
    {
        return std::string();
    }
    // §6.3 template (DIOPTER_SMART_UI_DESIGN.md:1586): "<PresetName> [dash]
    // edit any control to make it yours". The doc writes the separator as a
    // U+2014 em dash; this file uses a double-hyphen "--" everywhere instead
    // (NAMED_TIPS above, every tier/preset-owned tooltip). A grep of the
    // entire floater_ultimate_diopter.xml plus every string in this .cpp/.h
    // finds NO U+2014 anywhere, so matching the doc here would introduce the
    // file's only em dash for a single string -- the real inconsistency.
    // Deliberate spec-vs-house-style deviation, flagged for the next reviewer.
    return name + " -- edit any control to make it yours";
}

void ALFloaterUltimateDiopter::updatePresetStatusText(U32 diopter_preset, U32 kal_preset)
{
    setTextBoxIfChanged(mDiopterPresetStatus,
                         formatPresetStatusText(mDiopterPresetCombo, diopter_preset));
    setTextBoxIfChanged(mKalPresetStatus,
                         formatPresetStatusText(mKalPresetCombo, kal_preset));
}

const std::vector<ALDiopterBandSpec>& ALFloaterUltimateDiopter::bandSpecs()
{
    return BAND_SPECS;
}

bool ALFloaterUltimateDiopter::isSwapNested(const char* ctrl_name)
{
    return ctrl_name && SWAP_NESTED_CTRLS.count(ctrl_name) != 0;
}

void ALFloaterUltimateDiopter::updateBandPanels()
{
    // alpanelcinecamparams.cpp:511-524's exact pattern, generalized over
    // bandSpecs(): findChild() every call rather than caching the LLPanel*
    // pointers, matching the house precedent -- this only runs inside
    // refreshRelevance()'s own signature guard, so the extra lookups cost
    // nothing on the (overwhelming majority of) frames nothing moved.
    for (const ALDiopterBandSpec& band : bandSpecs())
    {
        const U32 value = gSavedSettings.getU32(band.mPivotSetting);
        for (U32 i = 0; i < band.mEntryCount; ++i)
        {
            const ALDiopterBandEntry& entry = band.mEntries[i];
            if (LLPanel* panel = findChild<LLPanel>(entry.mPanel))
            {
                const bool visible = (entry.mMode == value);
                // Codex round-2 CR2-6 / §3.8a -- neither setVisible(false)
                // nor setEnabled(false) releases keyboard focus (llview.cpp),
                // so a panel about to go invisible that currently holds
                // focus (itself or a descendant -- hasFocus() covers both,
                // matching setRowVisualState()'s own use of it below) would
                // leave a director typing into a now-invisible control.
                // Check and transfer BEFORE setVisible(false), never after.
                // Targets the floater itself -- the same deliberately-simple
                // choice wave 2/3 already made for every per-row transfer
                // (setRowVisualState) rather than threading a per-band
                // "pivot control" target through 7 different band pivots.
                if (!visible && panel->hasFocus())
                {
                    setFocus(true);
                }
                panel->setVisible(visible);
            }
        }
    }
}

void ALFloaterUltimateDiopter::refreshRelevance(bool force)
{
    ALDiopterState st;
    populateState(st);
    const U32 diopter_preset = gSavedSettings.getU32("CineDiopterPreset");
    const U32 kal_preset = gSavedSettings.getU32("CineDiopterKalPreset");
    const bool active_only = gSavedSettings.getBOOL("CineDiopterUIActiveOnly");

    const ALDiopterSigKey key = buildSigKey(st, diopter_preset, kal_preset, active_only);
    if (!force && key == mRelevanceSig)
    {
        return;
    }
    mRelevanceSig = key;

    // design doc §3.3 -- switch every same-rect overlay band to the
    // currently selected mode BEFORE the row pass below, so that a row
    // whose panel just became visible/hidden and a row's own setVisible()
    // call agree on the same frame.
    updateBandPanels();

    // CR2-9 follow-up -- both preset ids are already ALDiopterSigKey fields
    // (mDiopterPreset/mKalPreset), so this needs no separate driver; it
    // only needs to run when the key actually changed, exactly like the
    // row loop below.
    updatePresetStatusText(diopter_preset, kal_preset);

    for (S32 i = 0; i < AL_RELEVANCE_COUNT; ++i)
    {
        const ALDiopterRelevance& row = sRelevance[i];
        if (!mRows[i].mCtrl)
        {
            continue;
        }

        // Codex round-3 CR3-1 -- tool-mismatch must dominate BOTH the
        // enabled and the disabled path, so it is checked FIRST, before
        // Tier 0 (the master-disabled branch just below) ever runs. CR2-1
        // made a mismatch an unconditional hide inside the enabled path,
        // but the master-disabled branch used to run BEFORE that check and
        // `continue` past it -- so whenever the master switch was off,
        // BOTH tools' footer rows (diopter_freeze_at AND kal_freeze_at,
        // same rect) stayed visible+dimmed together regardless of the
        // selected tool, the identical overlap CR2-1 fixed for the enabled
        // state, just reachable from the OTHER branch. A row belonging to
        // the other tool has no home on screen in the CURRENT tool's tabs
        // at all, whether the effect is on or off -- so this check now
        // runs unconditionally, ahead of everything else, and its hide is
        // final for this row (never re-reached by Tier 0, the tier logic,
        // Active-only, or the filter override below, all of which only run
        // once tool_matches is already known true).
        if (!alDiopterToolMatches(row.mTool, st.mToolMode))
        {
            applyRowState(i, /*visible=*/false, /*enabled=*/false, tierTooltip(row));
            continue;
        }

        // design doc §1.4 Tier 0 -- the master enable. UNCONDITIONAL rows
        // (only diopter_enabled) are exempt; every other row goes uniformly
        // dark (never hidden -- a floater full of invisible controls has no
        // affordance at all) with the SAME text while the master switch is
        // off, rather than each claiming an unrelated tier-specific reason.
        if (!st.mEnabled && !row.mUnconditional)
        {
            applyRowState(i, /*visible=*/true, /*enabled=*/false, MASTER_DISABLED_TIP);
            continue;
        }

        const bool relevant = alDiopterEvaluate(row, st);

        bool visible = true;
        bool enabled = true;
        std::string tip;

        if (!relevant)
        {
            enabled = false;
            tip = tierTooltip(row);

            // design doc §3.2 Tier A -> hide; §5.2/§6.2 "Active controls
            // only = Off" escape hatch. Off can reveal a failing TIER_MODE
            // row (dimmed, same as Tier B/C always are) ONLY when doing so
            // is physically possible: the row must not be nested inside
            // some OTHER mode's now-hidden same-rect panel (isSwapNested).
            // Tool-mismatch was excluded unconditionally above, so
            // `relevant == false` here can only be because a clause failed
            // for an in-tool reason. TIER_OVERRIDDEN / TIER_UNARMED rows
            // are never hidden in the first place (unchanged from wave 2),
            // so Off has no effect on them -- matching the design doc's
            // "Tier-B stay visible as they always do".
            if (row.mTier == TIER_MODE)
            {
                const bool can_reveal = !active_only && !isSwapNested(row.mCtrlName);
                visible = can_reveal;
            }
        }
        else
        {
            // Preset-owned dimming: an INDEPENDENT path from the clause
            // tiers (table header's mTier note) -- a preset-owned control
            // can be fully "relevant" by clause (its mode is selected, its
            // arming control is on) while still not being authoritative,
            // because a non-Custom preset materializes over it every frame
            // (§1.5/§6.3).
            bool preset_dimmed = false;
            if (row.mPresetOwned)
            {
                preset_dimmed = (row.mTool & AL_TOOL_DIOPTER) ? (diopter_preset != 0U)
                                                               : (kal_preset != 0U);
            }
            if (preset_dimmed)
            {
                enabled = false;
                tip = presetOwnedTooltip(row.mTool);
            }
        }

        // Phase 5 -- the filter box (§7's row, mirroring
        // LLFloaterSettingsDebug's own live filter). Applied AFTER the tier/
        // preset logic above, as a final override: a match can reveal a row
        // the tier pass just hid (unless it is swap-nested -- isSwapNested,
        // the identical "nowhere to put it" limit "Active controls only =
        // Off" already has), and a non-match hides an otherwise-visible row
        // so only what the director searched for remains on screen. Never
        // touches the master checkbox (row.mUnconditional) or applies while
        // the master switch is off (that state already has its own uniform
        // explanation). No separate tool-mismatch guard needed here any
        // more -- a mismatched row already `continue`d past this point
        // entirely, above.
        if (!mFilterTokens.empty() && st.mEnabled && !row.mUnconditional)
        {
            if (matchesFilter(row, mRows[i]))
            {
                if (!isSwapNested(row.mCtrlName))
                {
                    visible = true;
                }
            }
            else
            {
                visible = false;
            }
        }

        applyRowState(i, visible, enabled, tip);
    }
}

// ---------------------------------------------------------------------------
// Phase 4 -- the eyedropper (§5.2).
// ---------------------------------------------------------------------------

void ALFloaterUltimateDiopter::armFocusPick(ALDiopterPickTarget target)
{
    ALToolFocusPick* tool = ALToolFocusPick::getInstance();
    const LLHandle<ALFloaterUltimateDiopter> handle = getDerivedHandle<ALFloaterUltimateDiopter>();

    const bool armed = tool->arm(
        mPickOwnerId,
        [handle, target](bool accepted, F32 depth_m, const LLVector3d& point_global)
        {
            if (ALFloaterUltimateDiopter* self = handle.get())
            {
                self->onFocusPickCommitted(target, accepted, depth_m, point_global);
            }
        },
        [handle, target](F32 depth_m)
        {
            if (ALFloaterUltimateDiopter* self = handle.get())
            {
                self->onFocusPickPreview(target, depth_m);
            }
        });
    if (!armed)
    {
        // Another owner already has the tool armed. The only OTHER possible
        // owner is this same floater's own prior arm (single_instance), and
        // arm()'s owner-match check lets that case through cleanly, so this
        // branch is effectively unreachable in practice; still handled
        // rather than assumed, per ALToolGhostPlace's own precedent.
        return;
    }

    mActivePick = target;
    mPickPreviewM = 0.f;
    LLToolMgr::getInstance()->setTransientTool(tool);
}

void ALFloaterUltimateDiopter::onFocusPickCommitted(ALDiopterPickTarget target, bool accepted,
                                                      F32 depth_m, const LLVector3d& /*point_global*/)
{
    if (mActivePick != target)
    {
        // A stale callback from an arm this floater already superseded (or
        // that a later arm() call's owner-match refused) -- ignore rather
        // than act on a target that is no longer the active one.
        return;
    }
    mActivePick = AL_PICK_NONE;

    if (!accepted)
    {
        return;     // cancelled (Esc / right-click / tool replacement) -- settings untouched
    }

    switch (target)
    {
        case AL_PICK_BASE:
        {
            // §5.2's mandatory pairing: write the metres AND force Manual
            // mode, or the value is silently overwritten from the camera
            // focus point every frame (pipeline.cpp's base-focus resolver).
            const F32 clamped = llclamp(depth_m, 0.1f, 128.f);
            gSavedSettings.setF32("CineDiopterBaseFocusM", clamped);
            gSavedSettings.setU32("CineDiopterFocusMode", 0U);   // Manual distance
            autoSelectRange(AL_RANGE_BASE, clamped);
            break;
        }
        case AL_PICK_LENS:
        {
            const F32 clamped = llclamp(depth_m, 0.1f, 128.f);
            gSavedSettings.setF32("CineDiopterLensFocusM", clamped);
            gSavedSettings.setU32("CineDiopterLensFocusMode", 1U);   // Manual distance (m)
            autoSelectRange(AL_RANGE_LENS, clamped);
            break;
        }
        case AL_PICK_KAL_DEPTH_CUT:
        {
            const F32 clamped = llclamp(depth_m, 0.1f, 256.f);
            // ProtectMode's 4 values are a 2-bit Radius|Depth field (Off=0,
            // Radius=1, Depth=2, Radius+Depth=3), so "arm Depth protection,
            // leave Radius exactly as it was" is precisely `|= 2` --
            // 0->2, 1->3, 2/3 unchanged (§5.2: "ProtectMode: 0->2, 1->3").
            const U32 old_mode = gSavedSettings.getU32("CineDiopterKalProtectMode");
            const U32 new_mode = old_mode | 2u;

            // Codex round-2 CR2-2 -- both CineDiopterKalDepthCut AND
            // CineDiopterKalProtectMode are kaleido preset-owned
            // (aldiopterpresetbank.cpp kaleidoOwnedNames()). Writing them
            // one at a time here would let the FIRST write's owned-setting
            // listener (llviewerfloaterreg.cpp) synchronously materialize-
            // then-flip to Custom and snapshot the bank BEFORE the second
            // write ever ran -- capturing the new depth cut but the OLD
            // protect mode, so a later Custom restore would silently lose
            // the "arm Depth protection" half of this pick.
            const U32 active_preset = gSavedSettings.getU32("CineDiopterKalPreset");
            if (active_preset != 0)
            {
                // Batch: under the guard (so NEITHER write below can trip
                // the owned-setting listener individually -- it early-
                // returns whenever alDiopterIsPresetMaterializing()), first
                // materialize the active preset's look into every OTHER
                // owned field (mirroring what the listener would have done
                // for a single edit), then stomp our two new values on top.
                {
                    ALScopedPresetMaterializing guard;
                    LLPipeline::materializeKaleidoPreset(active_preset, std::string());
                    gSavedSettings.setF32("CineDiopterKalDepthCut", clamped);
                    gSavedSettings.setU32("CineDiopterKalProtectMode", new_mode);
                }
                // Exactly ONE Custom transition, now that gSavedSettings
                // holds the complete, consistent post-pick look -- the
                // transition's own snapshot (alDiopterHandlePresetTransition,
                // aldiopterpresetbank.cpp) captures BOTH new values
                // together in the same bank write.
                alDiopterSetPresetExitReason(AL_EXIT_AUTO_AFTER_EDIT);
                gSavedSettings.setU32("CineDiopterKalPreset", 0);
            }
            else
            {
                // Already Custom: CineDiopterKalPreset == 0 means the
                // owned-setting listener's own `if (active != 0)` is false
                // for either write, so plain writes are exactly as safe as
                // they are for the non-owned Base/Lens picks above.
                gSavedSettings.setF32("CineDiopterKalDepthCut", clamped);
                gSavedSettings.setU32("CineDiopterKalProtectMode", new_mode);
            }

            autoSelectRange(AL_RANGE_KAL_DEPTH_CUT, clamped);
            break;
        }
        default:
            break;
    }

    refreshRelevance(/*force=*/true);
}

void ALFloaterUltimateDiopter::onFocusPickPreview(ALDiopterPickTarget target, F32 depth_m)
{
    if (mActivePick != target)
    {
        return;     // stale preview from a since-superseded/cancelled arm
    }
    mPickPreviewM = depth_m;
}

void ALFloaterUltimateDiopter::onSetFromCameraFocusClicked()
{
    // §5.3b -- "Same helper, writes the metres and forces FocusMode = 0."
    // Reuses the identical resolver the renderer and the readout both
    // trust; if the camera-focus ladder cannot resolve a target right now
    // (no region, a zero focus point, or a forward distance <= 0.05 m),
    // leave the slider untouched rather than writing a meaningless value.
    const bool dof_live = alDiopterUiDofFocusLive();
    F32 out_m;
    ALDiopterFocusProvenance out_prov;
    alDiopterResolveBaseFocus(gSavedSettings.getF32("CineDiopterBaseFocusM"),
                               /*focus_mode=*/1U, dof_live, out_m, out_prov);
    if (out_prov == AL_DIOPTER_FOCUS_MANUAL)
    {
        return;
    }

    const F32 clamped = llclamp(out_m, 0.1f, 128.f);
    gSavedSettings.setF32("CineDiopterBaseFocusM", clamped);
    gSavedSettings.setU32("CineDiopterFocusMode", 0U);
    autoSelectRange(AL_RANGE_BASE, clamped);
    refreshRelevance(/*force=*/true);
}

// static
std::string ALFloaterUltimateDiopter::formatBaseFocusReadout(ALDiopterFocusProvenance prov,
                                                                F32 resolved_m)
{
    // §5.3a's three verbatim strings, driven by the SAME provenance the
    // renderer resolved this frame (LLPipeline::sDiopterBaseFocusProvenance)
    // -- never re-derived, so this can never disagree with what is actually
    // on screen.
    switch (prov)
    {
        case AL_DIOPTER_FOCUS_DOF_LIVE:
            return llformat("Base focus: %.1f m -- tracking subject", resolved_m);
        case AL_DIOPTER_FOCUS_ALT_ZOOM:
            return llformat(
                "Base focus: %.1f m -- camera focus unavailable, tracking camera target",
                resolved_m);
        case AL_DIOPTER_FOCUS_MANUAL:
        default:
            return llformat(
                "Base focus: %.1f m -- no focus target; the slider below is in control",
                resolved_m);
    }
}

// static
std::string ALFloaterUltimateDiopter::formatSubjectReadout(ALDiopterFocusProvenance prov,
                                                             F32 resolved_m)
{
    // The Kal Anim "Subject" readout (§5.3a's own mockup, "Subject: 12.4 m
    // <- next to Base Distance and Depth Cut") is mode-INDEPENDENT -- Depth
    // Cut has no camera-tracking combo of its own, so this reports "what is
    // the camera looking at right now" rather than "what is this control
    // currently using", and MANUAL here means the camera-focus ladder found
    // nothing at all (not "manual mode was selected", since focus_mode is
    // forced to 1 by the caller) -- so no metre value is shown in that case.
    switch (prov)
    {
        case AL_DIOPTER_FOCUS_DOF_LIVE:
            return llformat("Subject: %.1f m", resolved_m);
        case AL_DIOPTER_FOCUS_ALT_ZOOM:
            return llformat("Subject: %.1f m (camera target)", resolved_m);
        case AL_DIOPTER_FOCUS_MANUAL:
        default:
            return "Subject: no focus target available";
    }
}

// static
std::string ALFloaterUltimateDiopter::formatPickingReadout(F32 preview_m)
{
    return llformat("Picking: %.1f m -- click to set, Esc to cancel", preview_m);
}

void ALFloaterUltimateDiopter::updateFocusReadouts()
{
    if (mActivePick == AL_PICK_BASE)
    {
        setTextBoxIfChanged(mBaseFocusReadout, formatPickingReadout(mPickPreviewM));
    }
    else
    {
        setTextBoxIfChanged(mBaseFocusReadout,
            formatBaseFocusReadout(LLPipeline::sDiopterBaseFocusProvenance,
                                    LLPipeline::sDiopterBaseFocusResolvedM));
    }

    if (mActivePick == AL_PICK_LENS)
    {
        setTextBoxIfChanged(mLensPickReadout, formatPickingReadout(mPickPreviewM));
    }
    else
    {
        // Lens Distance has no permanent provenance concept (no camera-
        // tracking mode to report on) -- this readout is preview-only,
        // blank whenever a pick is not in flight.
        setTextBoxIfChanged(mLensPickReadout, std::string());
    }

    if (mActivePick == AL_PICK_KAL_DEPTH_CUT)
    {
        setTextBoxIfChanged(mKalSubjectReadout, formatPickingReadout(mPickPreviewM));
    }
    else
    {
        const bool dof_live = alDiopterUiDofFocusLive();
        F32 subject_m;
        ALDiopterFocusProvenance subject_prov;
        // focus_mode forced to 1 -- this readout asks "what would the
        // camera-focus ladder resolve to right now", independent of
        // whatever CineDiopterFocusMode (a DIFFERENT control's mode) is
        // currently set to.
        alDiopterResolveBaseFocus(0.f, /*focus_mode=*/1U, dof_live, subject_m, subject_prov);
        setTextBoxIfChanged(mKalSubjectReadout, formatSubjectReadout(subject_prov, subject_m));
    }
}

// ---------------------------------------------------------------------------
// Phase 4 -- per-control range combos (§5.3e).
// ---------------------------------------------------------------------------

void ALFloaterUltimateDiopter::initRangeCombos()
{
    for (S32 slot = 0; slot < (S32)AL_RANGE_SLOT_COUNT; ++slot)
    {
        const ALDiopterRangeSpec& spec = RANGE_SPECS[slot];
        RangeSlotState& state = mRangeSlots[slot];

        state.mSlider = findChild<LLSliderCtrl>(spec.mSliderName);
        state.mCombo = findChild<LLComboBox>(spec.mComboName);
        if (!state.mCombo)
        {
            continue;
        }

        state.mCombo->removeall();
        for (U32 i = 0; i < spec.mEntryCount; ++i)
        {
            state.mCombo->add(std::string(spec.mEntries[i].mLabel));
        }
        state.mCombo->setCommitCallback(
            [this, slot](LLUICtrl*, const LLSD&)
            {
                onRangeComboCommit((ALDiopterRangeSlot)slot);
            });
    }
}

void ALFloaterUltimateDiopter::syncRangesToCurrentValues()
{
    for (S32 slot = 0; slot < (S32)AL_RANGE_SLOT_COUNT; ++slot)
    {
        const F32 current = gSavedSettings.getF32(RANGE_SPECS[slot].mSettingName);
        autoSelectRange((ALDiopterRangeSlot)slot, current);
    }
}

// static
S32 ALFloaterUltimateDiopter::narrowestRangeIndex(const ALDiopterRangeEntry* entries,
                                                    U32 count, F32 value)
{
    S32 best = (S32)count - 1;     // fall back to the widest (last) rung
    F32 best_span = std::numeric_limits<F32>::max();
    for (U32 i = 0; i < count; ++i)
    {
        if (value >= entries[i].mMin && value <= entries[i].mMax)
        {
            const F32 span = entries[i].mMax - entries[i].mMin;
            if (span < best_span)
            {
                best_span = span;
                best = (S32)i;
            }
        }
    }
    return best;
}

void ALFloaterUltimateDiopter::autoSelectRange(ALDiopterRangeSlot slot, F32 value)
{
    RangeSlotState& state = mRangeSlots[slot];
    const ALDiopterRangeSpec& spec = RANGE_SPECS[slot];
    const S32 index = narrowestRangeIndex(spec.mEntries, spec.mEntryCount, value);

    if (state.mCombo)
    {
        state.mCombo->selectNthItem(index);
    }
    applyRangeBounds(slot, index);

    // A fresh, deliberate placement (open, or a just-committed eyedropper
    // pick) supersedes any pending single-level undo from an earlier manual
    // range change.
    state.mHasShadow = false;
    setRangeStatusText(slot, std::string());
}

void ALFloaterUltimateDiopter::applyRangeBounds(ALDiopterRangeSlot slot, S32 range_index)
{
    RangeSlotState& state = mRangeSlots[slot];
    const ALDiopterRangeSpec& spec = RANGE_SPECS[slot];
    if (!state.mSlider || range_index < 0 || (U32)range_index >= spec.mEntryCount)
    {
        return;
    }
    const ALDiopterRangeEntry& e = spec.mEntries[range_index];
    // §5.3e -- setMinValue/setMaxValue do NOT re-clamp the underlying value
    // (llslider.cpp), so the caller (onRangeComboCommit / autoSelectRange)
    // is always the one responsible for the clamp-and-commit policy; this
    // method only ever touches the slider's bounds/step/precision.
    state.mSlider->setMinValue(e.mMin);
    state.mSlider->setMaxValue(e.mMax);
    state.mSlider->setIncrement(e.mIncrement);
    state.mSlider->setPrecision(e.mDecimalDigits);
}

void ALFloaterUltimateDiopter::onRangeComboCommit(ALDiopterRangeSlot slot)
{
    RangeSlotState& state = mRangeSlots[slot];
    const ALDiopterRangeSpec& spec = RANGE_SPECS[slot];
    if (!state.mCombo || !state.mSlider)
    {
        return;
    }

    const S32 index = llclamp(state.mCombo->getCurrentIndex(), 0, (S32)spec.mEntryCount - 1);
    const ALDiopterRangeEntry& e = spec.mEntries[index];

    const F32 raw = gSavedSettings.getF32(spec.mSettingName);
    // §5.3e "the range combo reverting on a single undo": if the PREVIOUS
    // range change on this slot clamped a value away and nothing has
    // touched the slider since (raw still equals what we wrote), try the
    // ORIGINAL pre-clamp value against the newly chosen bounds first,
    // rather than compounding clamps across successive range switches.
    const F32 candidate = (state.mHasShadow && raw == state.mLastClampedTo)
                          ? state.mShadowValue : raw;

    applyRangeBounds(slot, index);

    const F32 clamped = llclamp(candidate, e.mMin, e.mMax);
    if (clamped != candidate)
    {
        state.mHasShadow = true;
        state.mShadowValue = candidate;
        setRangeStatusText(slot, llformat("%.*f m is outside %s -- clamped to %.*f m",
                                           e.mDecimalDigits, candidate, e.mLabel,
                                           e.mDecimalDigits, clamped));
    }
    else
    {
        state.mHasShadow = false;
        setRangeStatusText(slot, std::string());
    }
    state.mLastClampedTo = clamped;

    if (clamped != raw)
    {
        gSavedSettings.setF32(spec.mSettingName, clamped);
    }
}

void ALFloaterUltimateDiopter::setRangeStatusText(ALDiopterRangeSlot slot, const std::string& text)
{
    LLTextBox* box = (slot == AL_RANGE_BASE || slot == AL_RANGE_LENS)
                     ? mFocusRangeStatus : mKalRangeStatus;
    setTextBoxIfChanged(box, text);
}

void ALFloaterUltimateDiopter::checkRangeStaleness()
{
    // Codex round-2 CR2-3 -- ranges only actively resync at postBuild()/
    // onOpen()/after an eyedropper commit. A reset button, a preset
    // materialize, or any other external write to one of these four
    // settings can leave the runtime-applied bounds stale: e.g. Base
    // Distance sitting on the Macro rung (0.1-2 m) when a reset snaps the
    // raw setting back to its 8 m default -- the slider would silently
    // clamp its DISPLAY to 2 m while gSavedSettings actually holds 8 m, a
    // "clamped lie" the director has no way to see. Cheap (4 settings, 4
    // slider getMin/Max calls) and safe to run unconditionally every
    // draw(): once a slot's range is re-picked to contain the current
    // value, this is a no-op for that slot on every subsequent frame until
    // something moves the raw value outside the newly-applied bounds again.
    for (S32 slot = 0; slot < (S32)AL_RANGE_SLOT_COUNT; ++slot)
    {
        RangeSlotState& state = mRangeSlots[slot];
        if (!state.mSlider)
        {
            continue;
        }
        const ALDiopterRangeSpec& spec = RANGE_SPECS[slot];
        const F32 raw = gSavedSettings.getF32(spec.mSettingName);

        // Defense in depth: spec.mHardMin/mHardMax (the §5.3e absolute
        // ceiling column) were carried on every ALDiopterRangeSpec but
        // never actually consulted anywhere until now -- every ladder's
        // own widest rung happens to already cover [hardMin, hardMax], so
        // this is normally a no-op, but a value that somehow drifted
        // outside the control's own absolute limits gets clamped back onto
        // gSavedSettings before the containing-range search below runs.
        const F32 hard_clamped = llclamp(raw, spec.mHardMin, spec.mHardMax);
        if (hard_clamped != raw)
        {
            gSavedSettings.setF32(spec.mSettingName, hard_clamped);
        }

        // Codex round-3 CR3-3 -- the staleness TEST must compare the RAW
        // value (read above, before the hard-clamp), never hard_clamped.
        // The widest rung of every one of the four ladders today has its
        // OWN max/min exactly equal to that control's hard limit (e.g.
        // Vista's 128 m == Base Distance's hardMax), so a raw value beyond
        // the hard ceiling (e.g. 300 m) would hard-clamp to precisely that
        // boundary -- and comparing the CLAMPED result against currently-
        // applied Vista bounds would then read "128 <= 128, still in
        // range" and skip both the auto-range reselect and the stale
        // undo/status clear, even though the real raw value (300) was
        // genuinely out of bounds and the director's prior range choice
        // may no longer be the intended one. Comparing raw instead catches
        // this AND the ordinary case identically: e.g. Macro's 0.1-2 m
        // bound with an 8 m raw default -- 8 m needs no hard-clamp at all
        // (hard_clamped == raw == 8), so raw vs. applied bounds already
        // reads "8 > 2, outside Macro" and triggers exactly as before.
        // Once triggered, autoSelectRange() re-picks a range containing the
        // CORRECTED (hard-clamped) value -- gSavedSettings already holds
        // that value as of the write-back above, not the stale raw one.
        const F32 applied_min = state.mSlider->getMinValue();
        const F32 applied_max = state.mSlider->getMaxValue();
        if (raw < applied_min || raw > applied_max)
        {
            // Re-pick the narrowest containing range; autoSelectRange()
            // also clears any stale single-level undo shadow and status
            // text for this slot, since both refer to a range choice that
            // no longer applies.
            autoSelectRange((ALDiopterRangeSlot)slot, hard_clamped);
        }
    }
}

// ---------------------------------------------------------------------------
// Phase 4 -- "Show Depth" toggle (§5.3c).
// ---------------------------------------------------------------------------

void ALFloaterUltimateDiopter::onShowDepthToggled(LLUICtrl* ctrl)
{
    const bool checked = ctrl && ctrl->getValue().asBoolean();
    // A plain binary toggle (Off <-> Depth), not "remember whatever Setup
    // View was showing before" -- keeps the affordance honest about what it
    // does without an extra persisted/session field for a rarely-visited
    // debug combo. See the wave-4 handoff for this simplification.
    gSavedSettings.setU32("CineDiopterDebugView", checked ? 3U : 0U);
}

void ALFloaterUltimateDiopter::updateShowDepthCheckboxes()
{
    const bool showing_depth = gSavedSettings.getU32("CineDiopterDebugView") == 3U;
    if (mShowDepthFocus && mShowDepthFocus->getValue().asBoolean() != showing_depth)
    {
        mShowDepthFocus->setValue(showing_depth);
    }
    if (mShowDepthKal && mShowDepthKal->getValue().asBoolean() != showing_depth)
    {
        mShowDepthKal->setValue(showing_depth);
    }
}

// ---------------------------------------------------------------------------
// Phase 5 -- polish, exactly §7's row.
// ---------------------------------------------------------------------------

void ALFloaterUltimateDiopter::updateResetIndicators()
{
    for (S32 i = 0; i < AL_RELEVANCE_COUNT; ++i)
    {
        RowWidgets& w = mRows[i];
        if (!w.mReset || !w.mControl)
        {
            continue;
        }
        const bool is_default = w.mControl->isDefault();
        if (is_default != w.mLastIsDefault)
        {
            w.mLastIsDefault = is_default;
            w.mReset->setImageOverlay("Refresh_Off", LLFontGL::HCENTER,
                                       is_default ? LLColor4::white : RESET_NON_DEFAULT_TINT);
        }
    }
}

void ALFloaterUltimateDiopter::onFilterChanged(const LLSD& value)
{
    std::string filter = value.asString();
    LLStringUtil::toLower(filter);
    if (filter == mFilterText)
    {
        return;
    }
    mFilterText = filter;

    // '+'-separated AND tokens, lowercase substring match -- mirrors
    // llfloatersettingsdebug.cpp's setSearchFilter() exactly.
    mFilterTokens.clear();
    size_t start = 0;
    while (true)
    {
        const size_t plus = mFilterText.find('+', start);
        std::string token = mFilterText.substr(
            start, plus == std::string::npos ? std::string::npos : plus - start);
        LLStringUtil::trim(token);
        if (!token.empty())
        {
            mFilterTokens.push_back(token);
        }
        if (plus == std::string::npos)
        {
            break;
        }
        start = plus + 1;
    }

    refreshRelevance(/*force=*/true);
}

bool ALFloaterUltimateDiopter::matchesFilter(const ALDiopterRelevance& row,
                                              const RowWidgets& w) const
{
    if (mFilterTokens.empty())
    {
        return true;
    }
    // Match against the technical control name (e.g. "diopter_ring_count",
    // itself already close to the setting's purpose) plus its authored
    // tooltip text -- avoids maintaining a second, driftable 181-entry
    // human-readable label table just for this polish feature.
    std::string haystack = row.mCtrlName;
    haystack += ' ';
    haystack += w.mDefaultCtrlTip;
    LLStringUtil::toLower(haystack);

    for (const std::string& token : mFilterTokens)
    {
        if (haystack.find(token) == std::string::npos)
        {
            return false;
        }
    }
    return true;
}
