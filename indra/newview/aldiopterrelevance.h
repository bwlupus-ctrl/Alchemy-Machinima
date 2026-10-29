/**
 * @file aldiopterrelevance.h
 * @brief Ultimate Diopter smart-UI relevance model (design doc
 *        DIOPTER_SMART_UI_DESIGN.md section 2.3).
 *
 * LLUI-free, gSavedSettings-free model unit, following the house precedent
 * of alcinelightrigmodel / aldirectorswitchermodel: state arrives as a plain
 * struct, so the evaluator is pure and headlessly sweepable.
 *
 * Three things live here:
 *
 *  1. The 12 armed-state predicate helpers.  Each mirrors the EXACT renderer
 *     expression it models (file:line cited per function).  pipeline.cpp is
 *     expected to call the scalar helpers with its own resolver locals (the
 *     call-swap sites are listed in the wave-1 handoff note), so the UI can
 *     never disagree with the renderer about what is live.
 *
 *  2. The relevance schema (ALDiopterTerm / ALDiopterClause /
 *     ALDiopterRelevance) and the 181-row table sRelevance[], converted
 *     row-for-row from doc/DIOPTER_RELEVANCE_TABLE.md (revision 6), which is
 *     the implementation source of truth.  Rules are OR-of-AND: a control is
 *     relevant when ANY clause passes; a clause passes when ALL its terms
 *     pass.  Counts are explicit -- no sentinel termination (Codex delta-5
 *     finding 3): an ALWAYS clause is {0 terms}, passes vacuously given the
 *     implicit gates, and participates in clause coverage like any other.
 *
 *  3. The evaluator: alDiopterEvalPred / alDiopterEvalClause /
 *     alDiopterEvaluate / alDiopterRelevantSet.  Every clause of every
 *     non-UNCONDITIONAL row carries an implicit "CineDiopterEnabled == on"
 *     term and an implicit tool-match term, applied once by
 *     alDiopterEvalClause rather than repeated 180 times in the table.
 *     Exactly one row (diopter_enabled) is UNCONDITIONAL: it sits outside
 *     the gate it controls and is the only control relevant while the
 *     master enable is off (design doc 1.4, Tier 0; pipeline.cpp's
 *     master-enable gate in renderFinalize).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALDIOPTERRELEVANCE_H
#define AL_ALDIOPTERRELEVANCE_H

#include "stdtypes.h"
#include "lldefs.h"                 // llclamp / llmax

#include <cmath>
#include <set>
#include <string>

// ---------------------------------------------------------------------------
// Predicates -- named armed-state conditions (design doc 2.3)
// ---------------------------------------------------------------------------

enum ALDiopterPred                 // named predicates for threshold-armed state
{
    PRED_NONE = 0,                 // "this term uses (mSetting, mMask)" -- a
                                   //   sentinel with NO truth value; excluded
                                   //   from the predicate-flip coverage rule
    PRED_WARP_ARMED,               // pipeline.cpp warp_active, shared helper
    PRED_HANDHELD_ARMED,           // CineDiopterHandheld > 0
    PRED_GHOSTS_ARMED,             // CineDiopterGhostCount > 0
    PRED_SEAM_ARMED,               // CineDiopterSeamGhostPx > 1e-4
    PRED_REFRACTION_ARMED,         // CineDiopterGlassProfile > 0
    PRED_ABERRATION_ARMED,         // character > 0 && strength_d > 0
    PRED_FIELD_CURVE_ARMED,        // field_curve > 0
    PRED_CELL_BREATHE_ARMED,       // CineDiopterKalCellBreathe > 0   [Codex 1.8]
    PRED_BASE_FOCUS_MANUAL,        // resolver reports MANUAL provenance (5.3)
    PRED_EDGE_WOBBLE_ARMED,        // CineDiopterWobbleAmt > 0        [Codex C2]
    PRED_SHAPE_HAS_EDGE,           // shape != FullFrame && placement == framed
                                   //   -- the mask evaluates wobY/wobA  [Codex C1]
    PRED_LENS_POWERED,             // strength_d > 0 -- the exact renderer
                                   //   expression from pipeline.cpp
                                   //   (|1/lens - 1/base| after clamps); distinct
                                   //   from ABERRATION_ARMED, which also needs
                                   //   character > 0. Multiplicative consumers
                                   //   (Magnify Scale, Character) die here even
                                   //   when their own slider is nonzero.
    AL_PRED_COUNT                  // terminator -- bounds the coverage loops;
                                   //   never a term value
};

// Base-focus provenance (design doc 5.3): what actually drove base_focus_m
// after the camera-focus branches ran (pipeline.cpp renderUltimateDiopter,
// the base_focus_m resolution block).  MANUAL == "the slider value survived",
// which is the only state in which the Base Distance slider is live
// (PRED_BASE_FOCUS_MANUAL, table row 38).
enum ALDiopterFocusProvenance
{
    AL_DIOPTER_FOCUS_MANUAL = 0,   // slider value used (manual mode, or both
                                   //   camera branches failed to resolve)
    AL_DIOPTER_FOCUS_DOF_LIVE,     // live viewer-DoF focus point won
    AL_DIOPTER_FOCUS_ALT_ZOOM,     // gAgentCamera alt-zoom fallback won
};

// ---------------------------------------------------------------------------
// Schema (design doc 2.3; table<->schema parity rule: the companion table
// carries a column iff this struct has the field -- its Setting column is the
// one deliberate exception, header-marked DERIVED from the floater XML)
// ---------------------------------------------------------------------------

// mTool bitmask
constexpr U32 AL_TOOL_DIOPTER = 1u << 0;
constexpr U32 AL_TOOL_KALEIDO = 1u << 1;
constexpr U32 AL_TOOL_BOTH    = AL_TOOL_DIOPTER | AL_TOOL_KALEIDO;

// mTier: treatment when the row's relevance clauses fail (design doc 3.2).
enum ALDiopterTier
{
    TIER_MODE = 0,                 // hide -- not part of this mode
    TIER_OVERRIDDEN,               // dim -- the value is overridden right now
    TIER_UNARMED,                  // dim -- awaiting an arming value
    AL_TIER_COUNT
};

// mSection: one id per 3.6 band/group, exactly the mSection inventory of
// DIOPTER_RELEVANCE_TABLE.md (28 ids, in inventory order).
enum ALDiopterSection
{
    SEC_HEADER = 0,                // Main header (6)
    SEC_FRAMING,                   // Shape header/framing (9)
    SEC_FOOTER,                    // persistent capture footer (4)
    SEC_SHAPE_EDGE,                // Shape band 1, edge/bitmask-gated (8)
    SEC_SHAPE_SPECIFIC,            // Shape band 2, shape-specific (11)
    SEC_FOCUS_BASE,                // Focus base-focus header (2)
    SEC_FOCUS_LENS,                // Focus lens-focus pair/group (3)
    SEC_FOCUS_FALLOFF,             // Focus falloff group (8)
    SEC_FOCUS_WINDOW,              // Focus Window Surround (2)
    SEC_GLASS_PROFILE,             // Glass profile header/band (7)
    SEC_GLASS_CHARACTER,           // Glass Character group (5)
    SEC_HALO_ARMING,               // Halo & Ghosts warp-arming row (5)
    SEC_HALO_PARAMS,               // Halo & Ghosts warp dependents (6)
    SEC_GHOSTS,                    // Halo & Ghosts, Ghost Copies group (8)
    SEC_BOKEH_APERTURE,            // Bokeh aperture band + Cat's-Eye (8)
    SEC_GLASS_OPTICS,              // Bokeh magnify pair (2)
    SEC_BOKEH_SEAM,                // Bokeh Seam Double pair (2)
    SEC_MOTION_PARAMS,             // Motion header + 12-mode band (18)
    SEC_HANDHELD,                  // Motion Handheld group (5)
    SEC_SPIN,                      // Motion Spin group (6)
    SEC_KAL_HEADER,                // Kaleido header (8)
    SEC_KAL_PATTERN,               // Kaleido pattern band, geometric (6)
    SEC_KAL_FX,                    // Kaleido pattern-band FX controls (4)
    SEC_KAL_SOURCE,                // Kaleido standing Source group (5)
    SEC_KAL_CELL,                  // Kaleido standing Cell group (5)
    SEC_KAL_MOTION,                // Kal Anim motion header/band (13)
    SEC_KAL_SPIN,                  // Kal Anim Spin group (6)
    SEC_KAL_PROTECT,               // Kal Anim Protect group (9)
    AL_SEC_COUNT
};

constexpr S32 AL_RELEVANCE_COUNT = 181;
constexpr S32 AL_MAX_CLAUSES = 3;
constexpr S32 AL_MAX_TERMS = 4;

struct ALDiopterTerm               // one ANDed term
{
    const char*     mSetting;      // driving setting, or nullptr if mPred is used
    U32             mMask;         // bit i set == term passes when value == i
    ALDiopterPred   mPred;         // alternative to (mSetting, mMask)
    bool            mNegate;       // term passes when the above does NOT
};

// Counts are EXPLICIT -- no sentinel termination [Codex delta-5 finding 3]:
// an ALWAYS clause is {0 terms}, participates in evaluation (vacuously
// passes given the implicit Enabled + tool gates) and in coverage (it fails
// exactly when an implicit gate fails), and nothing is skipped.
struct ALDiopterClause
{
    U8              mTermCount;    // 0 == ALWAYS (passes given Enabled + tool)
    ALDiopterTerm   mTerms[AL_MAX_TERMS];  // ANDed; entries beyond mTermCount
                                           //   must be {}
};

struct ALDiopterRelevance
{
    const char*      mCtrlName;      // XUI name, e.g. "diopter_star_points"
    const char*      mResetName;     // sibling reset button; nullptr ONLY for
                                     //   diopter_enabled [Codex A10]
    U32              mTool;          // AL_TOOL_DIOPTER | AL_TOOL_KALEIDO | AL_TOOL_BOTH
    U32              mSection;       // ALDiopterSection, for the panel/heading pass
    bool             mPresetOwned;   // in ALDiopterLook / ALKaleidoLook
                                     //   (aldiopterpresetbank.cpp owned lists)
    bool             mUnconditional; // exempt from the implicit Enabled term AND
                                     //   from the clause-coverage rule. Exactly
                                     //   one row sets this: diopter_enabled.
                                     //   Such a row has mClauseCount == 0.
    U32              mTier;          // ALDiopterTier
    U8               mClauseCount;   // 0 only when mUnconditional
    ALDiopterClause  mClauses[AL_MAX_CLAUSES];  // ORed; entries beyond
                                                //   mClauseCount must be {}
};

// The 181-row table, in DIOPTER_RELEVANCE_TABLE.md row order (rows 1..181).
extern const ALDiopterRelevance sRelevance[AL_RELEVANCE_COUNT];

// ---------------------------------------------------------------------------
// ALDiopterState -- every input the predicates and terms read, as a plain
// struct.  The floater glue (wave 2) populates it from gSavedSettings plus
// the renderer's shared focus resolver; tests drive it directly.  Member
// defaults are the settings.xml factory defaults, so a default-constructed
// state is "factory settings, master enable off".
// ---------------------------------------------------------------------------

struct ALDiopterState
{
    // master gates
    bool mEnabled = false;             // CineDiopterEnabled (settings.xml: 0)
    U32  mToolMode = 0;                // CineDiopterToolMode (0 diopter, 1 kaleido)

    // diopter mode enums (term-driving settings)
    U32  mShape = 1;                   // CineDiopterShape (0..11)
    U32  mPlacementMode = 0;           // CineDiopterPlacementMode (0 framed, 1 on-lens)
    U32  mContent = 0;                 // CineDiopterContent (0 diopter, 1 sharp window)
    U32  mGlassProfile = 0;            // CineDiopterGlassProfile (0 off .. 4)
    U32  mApertureShape = 0;           // CineDiopterApertureShape (0..4)
    U32  mPatternMode = 0;             // CineDiopterPatternMode (0 off .. 3)
    U32  mMotionMode = 0;              // CineDiopterMotionMode (0..11)
    U32  mPulseTarget = 0;             // CineDiopterPulseTarget (0 size, 1 focus)
    U32  mSpinMode = 0;                // CineDiopterSpinMode (0..3)
    U32  mFocusMode = 1;               // CineDiopterFocusMode (0 manual, 1 camera);
                                       //   drives provenance coherence only --
                                       //   no term or predicate reads it directly
    U32  mLensFocusMode = 0;           // CineDiopterLensFocusMode (0 power, 1 manual)
    bool mFreeze = false;              // CineDiopterFreeze

    // kaleido mode enums
    U32  mKalMode = 0;                 // CineDiopterKalMode (0..23)
    U32  mKalMotionMode = 0;           // CineDiopterKalMotionMode (0..11; 5 = Track)
    U32  mKalPulseTarget = 0;          // CineDiopterKalPulseTarget (0 zoom, 1 twist, 2 offset)
    U32  mKalSpinMode = 0;             // CineDiopterKalSpinMode (0..3)
    U32  mKalProtectMode = 0;          // CineDiopterKalProtectMode (0..3)
    U32  mKalProtectAnchor = 1;        // CineDiopterKalProtectAnchor
                                       //   (0 pattern center, 1 fixed, 2 focus)
    bool mKalFreeze = false;           // CineDiopterKalFreezeTime

    // predicate scalar inputs (raw setting values)
    F32  mRingFold = 0.f;              // CineDiopterRingFold
    F32  mTwistDeg = 0.f;              // CineDiopterTwistDeg
    F32  mLobeAmt = 0.f;               // CineDiopterLobeAmt
    F32  mPatternZoom = 1.f;           // CineDiopterPatternZoom
    F32  mHandheld = 0.f;              // CineDiopterHandheld
    F32  mGhostCount = 0.f;            // CineDiopterGhostCount
    F32  mSeamPx = 0.f;                // CineDiopterSeamGhostPx
    F32  mCharacter = 1.f;             // CineDiopterCharacter
    F32  mFieldScale = 1.f;            // CineDiopterFieldCurveScale
    F32  mWobbleAmt = 0.f;             // CineDiopterWobbleAmt
    F32  mCellBreathe = 0.f;           // CineDiopterKalCellBreathe

    // focus-plane inputs for strength_d (PRED_LENS_POWERED and friends)
    F32  mPower = 2.f;                 // CineDiopterPower (diopters)
    F32  mBaseFocusM = 8.f;            // CineDiopterBaseFocusM (the slider)
    F32  mLensFocusM = 1.5f;           // CineDiopterLensFocusM (the slider)
    // What the renderer's base-focus resolver actually produced this frame:
    // the resolved distance and where it came from.  Under MANUAL provenance
    // mResolvedBaseFocusM equals the slider value.
    ALDiopterFocusProvenance mBaseFocusProvenance = AL_DIOPTER_FOCUS_DOF_LIVE;
    F32  mResolvedBaseFocusM = 8.f;
};

// ---------------------------------------------------------------------------
// Shared predicate helpers -- pure functions of the minimal scalar args, so
// pipeline.cpp can call them with its resolver locals (see the wave-1
// handoff note for the call-swap sites).  Each mirrors the renderer
// expression cited above it, verbatim.
// ---------------------------------------------------------------------------

// pipeline.cpp renderUltimateDiopter, the warp_active computation feeding
// DIOPTER_HALO2.w (pipeline.cpp:14350-14352):
//   bool warp_active = (pattern_mode > 0) || (ring_fold > 1e-3f) ||
//                      (fabsf(twist_deg) > 1e-2f) || (lobe_amt > 1e-3f) ||
//                      (fabsf(pattern_zoom - 1.f) > 1e-3f);
inline bool alDiopterWarpArmed(S32 pattern_mode, F32 ring_fold, F32 twist_deg,
                               F32 lobe_amt, F32 pattern_zoom)
{
    return (pattern_mode > 0) || (ring_fold > 1e-3f) ||
           (fabsf(twist_deg) > 1e-2f) || (lobe_amt > 1e-3f) ||
           (fabsf(pattern_zoom - 1.f) > 1e-3f);
}

// ultimateDiopterGatherF.glsl:336 -- ud_glassRefract early-out:
//   if (profile <= 0 || radius < 1e-4) return;
// The radius term is not UI state: the uploaded radius has a positive
// minimum (eff_size clamps to [0.01, 1.5], pipeline.cpp:14279), so at the
// state level the branch is armed iff profile > 0.
inline bool alDiopterRefractionArmed(S32 profile)
{
    return profile > 0;
}

// pipeline.cpp:14235-14245 -- the lens plane in view meters:
//   manual mode uses the Lens Distance slider; physical mode solves the
//   thin-lens close-up  s' = 1 / (1/s + D)  against the resolved base plane.
inline F32 alDiopterLensFocusM(U32 lens_mode, F32 lens_m, F32 base_focus_m,
                               F32 power)
{
    if (lens_mode == 1U)
    {
        return llclamp(lens_m, 0.1f, 4096.f);
    }
    F32 denom = 1.f / base_focus_m + power;
    return (denom > 1e-4f) ? llclamp(1.f / denom, 0.1f, 4096.f) : 4096.f;
}

// pipeline.cpp:14249 -- the aberration driver, optical strength in diopters:
//   F32 strength_d = llclamp(fabsf(1.f / lens_focus_m - 1.f / base_focus_m),
//                            0.f, 10.f);
inline F32 alDiopterStrengthD(F32 lens_focus_m, F32 base_focus_m)
{
    return llclamp(fabsf(1.f / lens_focus_m - 1.f / base_focus_m), 0.f, 10.f);
}

// PRED_LENS_POWERED: strength_d > 0.  No epsilon -- the renderer deliberately
// has no minimum clamp ("equal planes / zero power must be optically
// neutral", pipeline.cpp:14246-14249), and every multiplicative consumer
// (Magnify Scale at :14251, Character's products at :14253-14256) dies at
// exactly 0.
inline bool alDiopterLensPowered(F32 strength_d)
{
    return strength_d > 0.f;
}

// PRED_ABERRATION_ARMED: pipeline.cpp:14254-14256 -- all three are
// strength_d * character products; the controlled scale itself is
// deliberately NOT part of the arming predicate:
//   F32 axial_ca_m = strength_d * 0.004f * character * g_axial_scale * lens_focus_m;
//   F32 ca_mag     = strength_d * 0.15f  * character * g_ca_scale * 0.01f;
//   F32 edge_vig   = strength_d * 0.02f  * character * g_vig_scale;
inline bool alDiopterAberrationArmed(F32 strength_d, F32 character)
{
    return strength_d > 0.f && character > 0.f;
}

// PRED_FIELD_CURVE_ARMED: pipeline.cpp:14253 --
//   F32 field_curve = llclamp(strength_d * 0.15f * character * g_field_scale,
//                             0.f, 1.f);
// armed iff the computed field_curve is greater than zero.
inline bool alDiopterFieldCurveArmed(F32 strength_d, F32 character,
                                     F32 field_scale)
{
    return llclamp(strength_d * 0.15f * character * field_scale, 0.f, 1.f) > 0.f;
}

// PRED_GHOSTS_ARMED: ultimateDiopterF.glsl:107 --
//   if (ghostCount > 0 && m > 0.001)
// The m > 0.001 term is per-pixel (the mask sample) and is not UI state.
inline bool alDiopterGhostsArmed(F32 ghost_count)
{
    return ghost_count > 0.f;
}

// PRED_SEAM_ARMED: ultimateDiopterF.glsl:90 --
//   if (diopter_comp.x > 1e-4)
// diopter_comp.x is the uploaded seam distance in pixels
// (pipeline.cpp:14378, DIOPTER_COMP.x = seam_px).
inline bool alDiopterSeamArmed(F32 seam_px)
{
    return seam_px > 1e-4f;
}

// PRED_HANDHELD_ARMED: pipeline.cpp:14154 --
//   if (handheld > 0.f)
inline bool alDiopterHandheldArmed(F32 handheld)
{
    return handheld > 0.f;
}

// PRED_EDGE_WOBBLE_ARMED: the static edge-wobble coefficient
// diopter_shape4.x is greater than zero.  Upload clamp at
// pipeline.cpp:14311 (llclamp(s_wobble_amt, 0.f, 0.3f)); consumption at
// ultimateDiopterGatherF.glsl:256-259 (the wobY/wobA static term).
inline bool alDiopterEdgeWobbleArmed(F32 wobble_amt)
{
    return llclamp(wobble_amt, 0.f, 0.3f) > 0.f;
}

// PRED_SHAPE_HAS_EDGE: neither On-Lens nor Full Frame -- the mask evaluates
// the wobY/wobA edge terms only in the shape-SDF else-branch.
// ultimateDiopterGatherF.glsl:200 (On-Lens replaces the mask), :234 (Full
// Frame pins m = 1.0), :238-259 (the edge SDF else branch).  [Codex C1]
inline bool alDiopterShapeHasEdge(U32 shape, U32 placement)
{
    return placement == 0U && shape != 0U;
}

// PRED_BASE_FOCUS_MANUAL: the slider value remained the provenance after the
// camera-focus branches failed to replace it (pipeline.cpp:14203-14232 --
// the dof_focus_live branch at :14204 and the alt-zoom fallback at :14213,
// each guarded by `if (d > 0.05f)`).
inline bool alDiopterBaseFocusManual(ALDiopterFocusProvenance prov)
{
    return prov == AL_DIOPTER_FOCUS_MANUAL;
}

// PRED_CELL_BREATHE_ARMED: kal_cell.y > 0 -- the Cell Breathe coefficient.
// ultimateKaleidoF.glsl:117 (the ftime term inside kal_cellScale); upload at
// pipeline.cpp:15050 (KAL_CELL.y = look.mCellBreathe).  [Codex 1.8]
inline bool alDiopterCellBreatheArmed(F32 cell_breathe)
{
    return cell_breathe > 0.f;
}

// ---------------------------------------------------------------------------
// State-level evaluation
// ---------------------------------------------------------------------------

// Derived: the renderer's lens plane / strength_d for this state (the base
// plane is the RESOLVED value, not the raw slider -- provenance decides).
F32 alDiopterStateLensFocusM(const ALDiopterState& st);
F32 alDiopterStateStrengthD(const ALDiopterState& st);

// Evaluate one predicate against a state.  pred must not be PRED_NONE.
bool alDiopterEvalPred(ALDiopterPred pred, const ALDiopterState& st);

// The value of a term-driving setting in this state, or -1 if the name is
// not one of the term-driving settings (a table bug the tests assert on).
S32 alDiopterStateValue(const ALDiopterState& st, const char* setting);

// True when the row's tool mask admits the state's tool mode.
bool alDiopterToolMatches(U32 row_tool, U32 tool_mode);

// Evaluate clause clause_index of row against st, INCLUDING the implicit
// terms: st.mEnabled AND tool-match AND all explicit terms.  The row operand
// is in the signature precisely because tool-match reads row.mTool, which
// lives on the row, not the clause.  This is what makes an ALWAYS clause a
// real, coverable clause: it passes whenever Enabled + tool pass and fails
// when they do not.
bool alDiopterEvalClause(const ALDiopterRelevance& row, S32 clause_index,
                         const ALDiopterState& st);

// OR over the row's clauses; UNCONDITIONAL rows are always relevant.
bool alDiopterEvaluate(const ALDiopterRelevance& row, const ALDiopterState& st);

// The full relevant-set for a state, keyed by mCtrlName.
std::set<std::string> alDiopterRelevantSet(const ALDiopterState& st);

// Predicate name for coverage diagnostics.
const char* alDiopterPredName(ALDiopterPred pred);

#endif // AL_ALDIOPTERRELEVANCE_H
