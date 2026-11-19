/**
 * @file aldiopterrelevance.cpp
 * @brief The 181-row Ultimate Diopter relevance table and its evaluator.
 *
 * sRelevance[] below is a row-for-row conversion of
 * doc/DIOPTER_RELEVANCE_TABLE.md (revision 6) -- the implementation source
 * of truth (design doc DIOPTER_SMART_UI_DESIGN.md section 2.3).  Row order,
 * clause order, and term order match the table exactly; each row is tagged
 * with its table row number and derived setting.  Masks follow the schema
 * semantics "bit i set == the term passes when the setting's value == i";
 * the trailing comment on each clause restates the table's clause text.
 *
 * Table invariants (asserted by tests/aldiopterrelevance_test.cpp):
 *   181 rows; exactly 1 UNCONDITIONAL and exactly 1 null reset (both
 *   diopter_enabled); no duplicate controls or settings; 194 explicit
 *   clauses; 93 preset-owned rows (44 diopter + 49 kaleido, matching the
 *   canonical owned lists in aldiopterpresetbank.cpp).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "aldiopterrelevance.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Table row shorthand (design doc 2.3)
// ---------------------------------------------------------------------------

#define S(name, mask) {name, mask, PRED_NONE, false}
#define P(pred)       {nullptr, 0u, pred, false}

const ALDiopterRelevance sRelevance[AL_RELEVANCE_COUNT] =
{
    // row 1 -- CineDiopterEnabled
    { "diopter_enabled", nullptr, AL_TOOL_BOTH, SEC_HEADER,
      false, true, TIER_MODE, 0, {} },
    // row 2 -- CineDiopterToolMode
    { "diopter_tool_mode", "rst_CineDiopterToolMode", AL_TOOL_BOTH, SEC_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 3 -- CineDiopterPreset
    { "diopter_preset", "rst_CineDiopterPreset", AL_TOOL_DIOPTER, SEC_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 4 -- CineDiopterQuality
    { "diopter_quality", "rst_CineDiopterQuality", AL_TOOL_DIOPTER, SEC_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 5 -- CineDiopterBlend
    { "diopter_blend", "rst_CineDiopterBlend", AL_TOOL_DIOPTER, SEC_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 6 -- CineDiopterCenterX
    { "diopter_center_x", "rst_CineDiopterCenterX", AL_TOOL_DIOPTER, SEC_FRAMING,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 7 -- CineDiopterCenterY
    { "diopter_center_y", "rst_CineDiopterCenterY", AL_TOOL_DIOPTER, SEC_FRAMING,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 8 -- CineDiopterSize
    { "diopter_size", "rst_CineDiopterSize", AL_TOOL_DIOPTER, SEC_FRAMING,
      false, false, TIER_MODE, 3,
      {
        { 2, { S("CineDiopterShape", 0xFFCu), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{2,3,4,5,6,7,8,9,10,11} AND Place=Framed
        { 2, { P(PRED_REFRACTION_ARMED), S("CineDiopterPlacementMode", 0x1u) } }, // PRED_REFRACTION_ARMED AND Place=Framed
        { 2, { P(PRED_WARP_ARMED), S("CineDiopterPlacementMode", 0x1u) } }, // PRED_WARP_ARMED AND Place=Framed
      } },
    // row 9 -- CineDiopterAngleDeg
    { "diopter_angle", "rst_CineDiopterAngleDeg", AL_TOOL_DIOPTER, SEC_FRAMING,
      false, false, TIER_MODE, 2,
      {
        { 2, { S("CineDiopterShape", 0xFFEu), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{1,2,3,4,5,6,7,8,9,10,11} AND Place=Framed
        { 1, { S("CineDiopterPlacementMode", 0x2u) } }, // Place=OnLens
      } },
    // row 10 -- CineDiopterTrackMode
    { "diopter_track", "rst_CineDiopterTrackMode", AL_TOOL_DIOPTER, SEC_FRAMING,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 11 -- CineDiopterPlacementMode
    { "diopter_placement", "rst_CineDiopterPlacementMode", AL_TOOL_DIOPTER, SEC_FRAMING,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 12 -- CineDiopterFreeze
    { "diopter_freeze", "rst_CineDiopterFreeze", AL_TOOL_DIOPTER, SEC_FOOTER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 13 -- CineDiopterFreezeAt
    { "diopter_freeze_at", "rst_CineDiopterFreezeAt", AL_TOOL_DIOPTER, SEC_FOOTER,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { S("CineDiopterFreeze", 0x2u) } }, // Freeze=on
      } },
    // row 14 -- CineDiopterDebugView
    { "diopter_debug", "rst_CineDiopterDebugView", AL_TOOL_DIOPTER, SEC_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 15 -- CineDiopterShape
    { "diopter_shape", "rst_CineDiopterShape", AL_TOOL_DIOPTER, SEC_FRAMING,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterPlacementMode", 0x1u) } }, // Place=Framed
      } },
    // row 16 -- CineDiopterContent
    { "diopter_content", "rst_CineDiopterContent", AL_TOOL_DIOPTER, SEC_FRAMING,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterPlacementMode", 0x1u) } }, // Place=Framed
      } },
    // row 17 -- CineDiopterStretch
    { "diopter_stretch", "rst_CineDiopterStretch", AL_TOOL_DIOPTER, SEC_FRAMING,
      false, false, TIER_MODE, 2,
      {
        { 2, { S("CineDiopterShape", 0xFFEu), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{1,2,3,4,5,6,7,8,9,10,11} AND Place=Framed
        { 1, { S("CineDiopterPlacementMode", 0x2u) } }, // Place=OnLens
      } },
    // row 18 -- CineDiopterFeather
    { "diopter_feather", "rst_CineDiopterFeather", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      false, false, TIER_MODE, 2,
      {
        { 2, { S("CineDiopterShape", 0xFFAu), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{1,3,4,5,6,7,8,9,10,11} AND Place=Framed
        { 1, { S("CineDiopterPlacementMode", 0x2u) } }, // Place=OnLens
      } },
    // row 19 -- CineDiopterInvert
    { "diopter_invert", "rst_CineDiopterInvert", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 20 -- CineDiopterHollow
    { "diopter_hollow", "rst_CineDiopterHollow", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      true, false, TIER_MODE, 2,
      {
        { 2, { S("CineDiopterShape", 0xFF8u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{3,4,5,6,7,8,9,10,11} AND Place=Framed
        { 1, { P(PRED_WARP_ARMED) } }, // PRED_WARP_ARMED
      } },
    // row 21 -- CineDiopterArcLengthDeg
    { "diopter_arc_len", "rst_CineDiopterArcLengthDeg", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      true, false, TIER_MODE, 2,
      {
        { 2, { S("CineDiopterShape", 0xFF0u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{4,5,6,7,8,9,10,11} AND Place=Framed
        { 1, { S("CineDiopterPlacementMode", 0x2u) } }, // Place=OnLens
      } },
    // row 22 -- CineDiopterBrokenCount
    { "diopter_broken", "rst_CineDiopterBrokenCount", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      true, false, TIER_MODE, 2,
      {
        { 2, { S("CineDiopterShape", 0xFF0u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{4,5,6,7,8,9,10,11} AND Place=Framed
        { 1, { S("CineDiopterPlacementMode", 0x2u) } }, // Place=OnLens
      } },
    // row 23 -- CineDiopterCornerRound
    { "diopter_round", "rst_CineDiopterCornerRound", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x6C0u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{6,7,9,10} AND Place=Framed
      } },
    // row 24 -- CineDiopterWobbleAmt
    { "diopter_wobble_amt", "rst_CineDiopterWobbleAmt", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      false, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_SHAPE_HAS_EDGE) } }, // PRED_SHAPE_HAS_EDGE
      } },
    // row 25 -- CineDiopterWobbleFreq
    { "diopter_wobble_freq", "rst_CineDiopterWobbleFreq", AL_TOOL_DIOPTER, SEC_SHAPE_EDGE,
      false, false, TIER_UNARMED, 2,
      {
        { 2, { P(PRED_EDGE_WOBBLE_ARMED), P(PRED_SHAPE_HAS_EDGE) } }, // PRED_EDGE_WOBBLE_ARMED AND PRED_SHAPE_HAS_EDGE
        { 2, { S("CineDiopterMotionMode", 0x8u), P(PRED_SHAPE_HAS_EDGE) } }, // Motion=Wave AND PRED_SHAPE_HAS_EDGE
      } },
    // row 26 -- CineDiopterPolySides
    { "diopter_poly_sides", "rst_CineDiopterPolySides", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x40u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{6} AND Place=Framed
      } },
    // row 27 -- CineDiopterStarPoints
    { "diopter_star_points", "rst_CineDiopterStarPoints", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x80u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{7} AND Place=Framed
      } },
    // row 28 -- CineDiopterStarInner
    { "diopter_star_inner", "rst_CineDiopterStarInner", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x80u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{7} AND Place=Framed
      } },
    // row 29 -- CineDiopterPetalCount
    { "diopter_petal_count", "rst_CineDiopterPetalCount", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x200u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{9} AND Place=Framed
      } },
    // row 30 -- CineDiopterPetalDepth
    { "diopter_petal_depth", "rst_CineDiopterPetalDepth", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x200u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{9} AND Place=Framed
      } },
    // row 31 -- CineDiopterBlobSeed
    { "diopter_blob_seed", "rst_CineDiopterBlobSeed", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x400u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{10} AND Place=Framed
      } },
    // row 32 -- CineDiopterBlobAmt
    { "diopter_blob_amt", "rst_CineDiopterBlobAmt", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x400u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{10} AND Place=Framed
      } },
    // row 33 -- CineDiopterCrescentBite
    { "diopter_crescent_bite", "rst_CineDiopterCrescentBite", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x800u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{11} AND Place=Framed
      } },
    // row 34 -- CineDiopterCrescentShift
    { "diopter_crescent_shift", "rst_CineDiopterCrescentShift", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x800u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{11} AND Place=Framed
      } },
    // row 35 -- CineDiopterSquirclePow
    { "diopter_squircle_pow", "rst_CineDiopterSquirclePow", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x20u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{5} AND Place=Framed
      } },
    // row 36 -- CineDiopterSplitCurvature
    { "diopter_split_curve", "rst_CineDiopterSplitCurvature", AL_TOOL_DIOPTER, SEC_SHAPE_SPECIFIC,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterShape", 0x2u), S("CineDiopterPlacementMode", 0x1u) } }, // Shape{1} AND Place=Framed
      } },
    // row 37 -- CineDiopterFocusMode
    { "diopter_focus_mode", "rst_CineDiopterFocusMode", AL_TOOL_DIOPTER, SEC_FOCUS_BASE,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 38 -- CineDiopterBaseFocusM
    { "diopter_base_m", "rst_CineDiopterBaseFocusM", AL_TOOL_DIOPTER, SEC_FOCUS_BASE,
      false, false, TIER_OVERRIDDEN, 1,
      {
        { 1, { P(PRED_BASE_FOCUS_MANUAL) } }, // PRED_BASE_FOCUS_MANUAL
      } },
    // row 39 -- CineDiopterLensFocusMode
    { "diopter_lens_mode", "rst_CineDiopterLensFocusMode", AL_TOOL_DIOPTER, SEC_FOCUS_LENS,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 40 -- CineDiopterPower
    { "diopter_power", "rst_CineDiopterPower", AL_TOOL_DIOPTER, SEC_FOCUS_LENS,
      false, false, TIER_OVERRIDDEN, 1,
      {
        { 1, { S("CineDiopterLensFocusMode", 0x1u) } }, // LensMode=Power
      } },
    // row 41 -- CineDiopterLensFocusM
    { "diopter_lens_m", "rst_CineDiopterLensFocusM", AL_TOOL_DIOPTER, SEC_FOCUS_LENS,
      false, false, TIER_OVERRIDDEN, 1,
      {
        { 1, { S("CineDiopterLensFocusMode", 0x2u) } }, // LensMode=Manual
      } },
    // row 42 -- CineDiopterFocusWidthM
    { "diopter_focus_width", "rst_CineDiopterFocusWidthM", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 43 -- CineDiopterFalloffRate
    { "diopter_falloff", "rst_CineDiopterFalloffRate", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 44 -- CineDiopterFalloffCurve
    { "diopter_curve", "rst_CineDiopterFalloffCurve", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 45 -- CineDiopterNearStrength
    { "diopter_near_str", "rst_CineDiopterNearStrength", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 46 -- CineDiopterFarStrength
    { "diopter_far_str", "rst_CineDiopterFarStrength", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 47 -- CineDiopterMaxBlurPx
    { "diopter_max_blur", "rst_CineDiopterMaxBlurPx", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 48 -- CineDiopterFloorMaxBlurPx
    { "diopter_floor_max", "rst_CineDiopterFloorMaxBlurPx", AL_TOOL_DIOPTER, SEC_FOCUS_WINDOW,
      false, false, TIER_UNARMED, 2,
      {
        { 1, { P(PRED_FIELD_CURVE_ARMED) } }, // PRED_FIELD_CURVE_ARMED
        { 1, { S("CineDiopterContent", 0x2u) } }, // Content=SharpWindow
      } },
    // row 49 -- CineDiopterSpotBlur
    { "diopter_spot_blur", "rst_CineDiopterSpotBlur", AL_TOOL_DIOPTER, SEC_FOCUS_WINDOW,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterContent", 0x2u) } }, // Content=SharpWindow
      } },
    // row 50 -- CineDiopterBokehHighlight
    { "diopter_bokeh_hi", "rst_CineDiopterBokehHighlight", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 51 -- CineDiopterDepthEdgeM
    { "diopter_depth_edge", "rst_CineDiopterDepthEdgeM", AL_TOOL_DIOPTER, SEC_FOCUS_FALLOFF,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 52 -- CineDiopterGlassProfile
    { "diopter_profile", "rst_CineDiopterGlassProfile", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 53 -- CineDiopterIOR
    { "diopter_ior", "rst_CineDiopterIOR", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_REFRACTION_ARMED) } }, // PRED_REFRACTION_ARMED
      } },
    // row 54 -- CineDiopterThickness
    { "diopter_thick", "rst_CineDiopterThickness", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_REFRACTION_ARMED) } }, // PRED_REFRACTION_ARMED
      } },
    // row 55 -- CineDiopterRimWidth
    { "diopter_rim_width", "rst_CineDiopterRimWidth", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_REFRACTION_ARMED) } }, // PRED_REFRACTION_ARMED
      } },
    // row 56 -- CineDiopterRimWarp
    { "diopter_rim_warp", "rst_CineDiopterRimWarp", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_REFRACTION_ARMED) } }, // PRED_REFRACTION_ARMED
      } },
    // row 57 -- CineDiopterRimCaustic
    { "diopter_rim_caustic", "rst_CineDiopterRimCaustic", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_REFRACTION_ARMED) } }, // PRED_REFRACTION_ARMED
      } },
    // row 58 -- CineDiopterRimDarken
    { "diopter_rim_darken", "rst_CineDiopterRimDarken", AL_TOOL_DIOPTER, SEC_GLASS_PROFILE,
      true, false, TIER_MODE, 1,
      {
        { 1, { P(PRED_REFRACTION_ARMED) } }, // PRED_REFRACTION_ARMED
      } },
    // row 59 -- CineDiopterCharacter
    { "diopter_character", "rst_CineDiopterCharacter", AL_TOOL_DIOPTER, SEC_GLASS_CHARACTER,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_LENS_POWERED) } }, // PRED_LENS_POWERED
      } },
    // row 60 -- CineDiopterCAScale
    { "diopter_ca_scale", "rst_CineDiopterCAScale", AL_TOOL_DIOPTER, SEC_GLASS_CHARACTER,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_ABERRATION_ARMED) } }, // PRED_ABERRATION_ARMED
      } },
    // row 61 -- CineDiopterAxialCAScale
    { "diopter_axial_scale", "rst_CineDiopterAxialCAScale", AL_TOOL_DIOPTER, SEC_GLASS_CHARACTER,
      false, false, TIER_UNARMED, 1,
      {
        { 2, { P(PRED_ABERRATION_ARMED), S("CineDiopterContent", 0x1u) } }, // PRED_ABERRATION_ARMED AND Content=Diopter
      } },
    // row 62 -- CineDiopterFieldCurveScale
    { "diopter_field_scale", "rst_CineDiopterFieldCurveScale", AL_TOOL_DIOPTER, SEC_GLASS_CHARACTER,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_ABERRATION_ARMED) } }, // PRED_ABERRATION_ARMED
      } },
    // row 63 -- CineDiopterEdgeVignetteScale
    { "diopter_vig_scale", "rst_CineDiopterEdgeVignetteScale", AL_TOOL_DIOPTER, SEC_GLASS_CHARACTER,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_ABERRATION_ARMED) } }, // PRED_ABERRATION_ARMED
      } },
    // row 64 -- CineDiopterRingCount
    { "diopter_ring_count", "rst_CineDiopterRingCount", AL_TOOL_DIOPTER, SEC_HALO_PARAMS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_WARP_ARMED) } }, // PRED_WARP_ARMED
      } },
    // row 65 -- CineDiopterRingFold
    { "diopter_ring_fold", "rst_CineDiopterRingFold", AL_TOOL_DIOPTER, SEC_HALO_ARMING,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 66 -- CineDiopterRingPhase
    { "diopter_ring_phase", "rst_CineDiopterRingPhase", AL_TOOL_DIOPTER, SEC_HALO_PARAMS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_WARP_ARMED) } }, // PRED_WARP_ARMED
      } },
    // row 67 -- CineDiopterTwistDeg
    { "diopter_twist", "rst_CineDiopterTwistDeg", AL_TOOL_DIOPTER, SEC_HALO_ARMING,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 68 -- CineDiopterLobeAmt
    { "diopter_lobe_amt", "rst_CineDiopterLobeAmt", AL_TOOL_DIOPTER, SEC_HALO_ARMING,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 69 -- CineDiopterLobeCount
    { "diopter_lobe_count", "rst_CineDiopterLobeCount", AL_TOOL_DIOPTER, SEC_HALO_PARAMS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_WARP_ARMED) } }, // PRED_WARP_ARMED
      } },
    // row 70 -- CineDiopterLobePhaseDeg
    { "diopter_lobe_phase", "rst_CineDiopterLobePhaseDeg", AL_TOOL_DIOPTER, SEC_HALO_PARAMS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_WARP_ARMED) } }, // PRED_WARP_ARMED
      } },
    // row 71 -- CineDiopterGhostCount
    { "diopter_ghost_count", "rst_CineDiopterGhostCount", AL_TOOL_DIOPTER, SEC_GHOSTS,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 72 -- CineDiopterGhostSpacing
    { "diopter_ghost_spacing", "rst_CineDiopterGhostSpacing", AL_TOOL_DIOPTER, SEC_GHOSTS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 73 -- CineDiopterTangentSmear
    { "diopter_tangent_smear", "rst_CineDiopterTangentSmear", AL_TOOL_DIOPTER, SEC_GHOSTS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 74 -- CineDiopterRadialSmear
    { "diopter_radial_smear", "rst_CineDiopterRadialSmear", AL_TOOL_DIOPTER, SEC_GHOSTS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 75 -- CineDiopterGhostThreshold
    { "diopter_ghost_threshold", "rst_CineDiopterGhostThreshold", AL_TOOL_DIOPTER, SEC_GHOSTS,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 76 -- CineDiopterGhostKnee
    { "diopter_ghost_knee", "rst_CineDiopterGhostKnee", AL_TOOL_DIOPTER, SEC_GHOSTS,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 77 -- CineDiopterGhostGain
    { "diopter_ghost_gain", "rst_CineDiopterGhostGain", AL_TOOL_DIOPTER, SEC_GHOSTS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 78 -- CineDiopterDispersion
    { "diopter_dispersion", "rst_CineDiopterDispersion", AL_TOOL_DIOPTER, SEC_GHOSTS,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_GHOSTS_ARMED) } }, // PRED_GHOSTS_ARMED
      } },
    // row 79 -- CineDiopterApertureShape
    { "diopter_ap_shape", "rst_CineDiopterApertureShape", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 80 -- CineDiopterBlades
    { "diopter_blades", "rst_CineDiopterBlades", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterApertureShape", 0x6u) } }, // Aperture{1,2}
      } },
    // row 81 -- CineDiopterBladeRotDeg
    { "diopter_blade_rot", "rst_CineDiopterBladeRotDeg", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterApertureShape", 0xEu) } }, // Aperture{1,2,3}
      } },
    // row 82 -- CineDiopterBladeCurve
    { "diopter_blade_curve", "rst_CineDiopterBladeCurve", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterApertureShape", 0xEu) } }, // Aperture{1,2,3}
      } },
    // row 83 -- CineDiopterApertureInner
    { "diopter_ap_inner", "rst_CineDiopterApertureInner", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterApertureShape", 0x4u) } }, // Aperture{2}
      } },
    // row 84 -- CineDiopterAnamorph
    { "diopter_anamorph", "rst_CineDiopterAnamorph", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterApertureShape", 0x10u) } }, // Aperture{4}
      } },
    // row 85 -- CineDiopterAnamorphAngleDeg
    { "diopter_anam_angle", "rst_CineDiopterAnamorphAngleDeg", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterApertureShape", 0x10u) } }, // Aperture{4}
      } },
    // row 86 -- CineDiopterCatEye
    { "diopter_cat_eye", "rst_CineDiopterCatEye", AL_TOOL_DIOPTER, SEC_BOKEH_APERTURE,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 87 -- CineDiopterPatternMode
    { "diopter_pattern_mode", "rst_CineDiopterPatternMode", AL_TOOL_DIOPTER, SEC_HALO_ARMING,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 88 -- CineDiopterPatternSegments
    { "diopter_pattern_seg", "rst_CineDiopterPatternSegments", AL_TOOL_DIOPTER, SEC_HALO_PARAMS,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterPatternMode", 0xEu) } }, // Faceted{1,2,3}
      } },
    // row 89 -- CineDiopterPatternFeedDeg
    { "diopter_pattern_feed", "rst_CineDiopterPatternFeedDeg", AL_TOOL_DIOPTER, SEC_HALO_PARAMS,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterPatternMode", 0xEu) } }, // Faceted{1,2,3}
      } },
    // row 90 -- CineDiopterPatternZoom
    { "diopter_pattern_zoom", "rst_CineDiopterPatternZoom", AL_TOOL_DIOPTER, SEC_HALO_ARMING,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 91 -- CineDiopterSeamGhostPx
    { "diopter_seam_px", "rst_CineDiopterSeamGhostPx", AL_TOOL_DIOPTER, SEC_BOKEH_SEAM,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 92 -- CineDiopterSeamGhostAmt
    { "diopter_seam_amt", "rst_CineDiopterSeamGhostAmt", AL_TOOL_DIOPTER, SEC_BOKEH_SEAM,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_SEAM_ARMED) } }, // PRED_SEAM_ARMED
      } },
    // row 93 -- CineDiopterMagnifyScale
    { "diopter_mag_scale", "rst_CineDiopterMagnifyScale", AL_TOOL_DIOPTER, SEC_GLASS_OPTICS,
      false, false, TIER_UNARMED, 1,
      {
        { 3, { S("CineDiopterGlassProfile", 0x1u), S("CineDiopterContent", 0x1u), P(PRED_LENS_POWERED) } }, // Profile=Off AND Content=Diopter AND PRED_LENS_POWERED
      } },
    // row 94 -- CineDiopterMagnifyTrim
    { "diopter_mag_trim", "rst_CineDiopterMagnifyTrim", AL_TOOL_DIOPTER, SEC_GLASS_OPTICS,
      false, false, TIER_OVERRIDDEN, 1,
      {
        { 2, { S("CineDiopterGlassProfile", 0x1u), S("CineDiopterContent", 0x1u) } }, // Profile=Off AND Content=Diopter
      } },
    // row 95 -- CineDiopterMotionMode
    { "diopter_motion_mode", "rst_CineDiopterMotionMode", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 96 -- CineDiopterMotionSpeed
    // rev-6.1 correction (Codex-adjudicated): Stutter (Motion 5) clocks
    // itself from t_now * StutterRate (pipeline.cpp:14128-14131) and never
    // reads the shared tm = t_now * MotionSpeed timeline, so Speed is NOT
    // consumed there. bits 1-4 + 6-11 = 0xFDE.
    { "diopter_motion_speed", "rst_CineDiopterMotionSpeed", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0xFDEu) } }, // Motion{1,2,3,4,6,7,8,9,10,11}
      } },
    // row 97 -- CineDiopterMotionAngleDeg
    { "diopter_motion_angle", "rst_CineDiopterMotionAngleDeg", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x42u) } }, // Motion{1,6}
      } },
    // row 98 -- CineDiopterSweepRange
    { "diopter_sweep_range", "rst_CineDiopterSweepRange", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0xA02u) } }, // Motion{1,9,11}
      } },
    // row 99 -- CineDiopterSweepPingPong
    { "diopter_ping_pong", "rst_CineDiopterSweepPingPong", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x2u) } }, // Motion{1}
      } },
    // row 100 -- CineDiopterPulseTarget
    { "diopter_pulse_target", "rst_CineDiopterPulseTarget", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x4u) } }, // Motion{2}
      } },
    // row 101 -- CineDiopterPulseAmt
    { "diopter_pulse_amt", "rst_CineDiopterPulseAmt", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 3,
      {
        { 2, { S("CineDiopterMotionMode", 0x4u), S("CineDiopterPulseTarget", 0x1u) } }, // Motion{2} AND PulseTarget=Size
        { 3, { S("CineDiopterMotionMode", 0x4u), S("CineDiopterPulseTarget", 0x2u), S("CineDiopterContent", 0x1u) } }, // Motion{2} AND PulseTarget=Focus AND Content=Diopter
        { 1, { S("CineDiopterMotionMode", 0x400u) } }, // Motion{10}
      } },
    // row 102 -- CineDiopterWaveAmp
    { "diopter_wave_amp", "rst_CineDiopterWaveAmp", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterMotionMode", 0x8u), P(PRED_SHAPE_HAS_EDGE) } }, // Motion{3} AND PRED_SHAPE_HAS_EDGE
      } },
    // row 103 -- CineDiopterPathFreqX
    { "diopter_path_fx", "rst_CineDiopterPathFreqX", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x10u) } }, // Motion{4}
      } },
    // row 104 -- CineDiopterPathFreqY
    { "diopter_path_fy", "rst_CineDiopterPathFreqY", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x10u) } }, // Motion{4}
      } },
    // row 105 -- CineDiopterPathPhase
    { "diopter_path_phase", "rst_CineDiopterPathPhase", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x10u) } }, // Motion{4}
      } },
    // row 106 -- CineDiopterPathAmp
    { "diopter_path_amp", "rst_CineDiopterPathAmp", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x9D0u) } }, // Motion{4,6,7,8,11}
      } },
    // row 107 -- CineDiopterStutterRate
    { "diopter_stutter_rate", "rst_CineDiopterStutterRate", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x20u) } }, // Motion{5}
      } },
    // row 108 -- CineDiopterStutterPos
    { "diopter_stutter_pos", "rst_CineDiopterStutterPos", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x20u) } }, // Motion{5}
      } },
    // row 109 -- CineDiopterStutterAngleDeg
    { "diopter_stutter_angle", "rst_CineDiopterStutterAngleDeg", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x20u) } }, // Motion{5}
      } },
    // row 110 -- CineDiopterStutterSize
    { "diopter_stutter_size", "rst_CineDiopterStutterSize", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x20u) } }, // Motion{5}
      } },
    // row 111 -- CineDiopterStutterFocus
    { "diopter_stutter_focus", "rst_CineDiopterStutterFocus", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterMotionMode", 0x20u), S("CineDiopterContent", 0x1u) } }, // Motion{5} AND Content=Diopter
      } },
    // row 112 -- CineDiopterStutterSmooth
    { "diopter_stutter_smooth", "rst_CineDiopterStutterSmooth", AL_TOOL_DIOPTER, SEC_MOTION_PARAMS,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterMotionMode", 0x20u) } }, // Motion{5}
      } },
    // row 113 -- CineDiopterHandheld
    { "diopter_handheld", "rst_CineDiopterHandheld", AL_TOOL_DIOPTER, SEC_HANDHELD,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 114 -- CineDiopterHandheldSpeed
    { "diopter_handheld_speed", "rst_CineDiopterHandheldSpeed", AL_TOOL_DIOPTER, SEC_HANDHELD,
      true, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_HANDHELD_ARMED) } }, // PRED_HANDHELD_ARMED
      } },
    // row 115 -- CineDiopterHandheldGait
    { "diopter_handheld_gait", "rst_CineDiopterHandheldGait", AL_TOOL_DIOPTER, SEC_HANDHELD,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_HANDHELD_ARMED) } }, // PRED_HANDHELD_ARMED
      } },
    // row 116 -- CineDiopterHandheldRotDeg
    { "diopter_handheld_rot", "rst_CineDiopterHandheldRotDeg", AL_TOOL_DIOPTER, SEC_HANDHELD,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { P(PRED_HANDHELD_ARMED) } }, // PRED_HANDHELD_ARMED
      } },
    // row 117 -- CineDiopterHandheldFocus
    { "diopter_handheld_focus", "rst_CineDiopterHandheldFocus", AL_TOOL_DIOPTER, SEC_HANDHELD,
      false, false, TIER_UNARMED, 1,
      {
        { 2, { P(PRED_HANDHELD_ARMED), S("CineDiopterContent", 0x1u) } }, // PRED_HANDHELD_ARMED AND Content=Diopter
      } },
    // row 118 -- CineDiopterSpinMode
    { "diopter_spin_mode", "rst_CineDiopterSpinMode", AL_TOOL_DIOPTER, SEC_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 119 -- CineDiopterSpinSpeed
    { "diopter_spin_speed", "rst_CineDiopterSpinSpeed", AL_TOOL_DIOPTER, SEC_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterSpinMode", 0x1u) } }, // Spin=Constant
      } },
    // row 120 -- CineDiopterSpinTravelDeg
    { "diopter_spin_travel", "rst_CineDiopterSpinTravelDeg", AL_TOOL_DIOPTER, SEC_SPIN,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterSpinMode", 0xEu) } }, // Spin{1,2,3}
      } },
    // row 121 -- CineDiopterSpinDurationS
    { "diopter_spin_duration", "rst_CineDiopterSpinDurationS", AL_TOOL_DIOPTER, SEC_SPIN,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterSpinMode", 0xEu) } }, // Spin{1,2,3}
      } },
    // row 122 -- CineDiopterSpinBounce
    { "diopter_spin_bounce", "rst_CineDiopterSpinBounce", AL_TOOL_DIOPTER, SEC_SPIN,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterSpinMode", 0x8u) } }, // Spin{3}
      } },
    // row 123 -- CineDiopterSpinDelayS
    { "diopter_spin_delay", "rst_CineDiopterSpinDelayS", AL_TOOL_DIOPTER, SEC_SPIN,
      false, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterSpinMode", 0xEu) } }, // Spin{1,2,3}
      } },
    // row 124 -- CineDiopterKalPreset
    { "kal_preset", "rst_CineDiopterKalPreset", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 125 -- CineDiopterKalMode
    { "kal_mode", "rst_CineDiopterKalMode", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 126 -- CineDiopterKalSegments
    { "kal_segments", "rst_CineDiopterKalSegments", AL_TOOL_KALEIDO, SEC_KAL_PATTERN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0xDDA5FFu) } }, // KalMode{0,1,2,3,4,5,6,7,8,10,13,15,16,18,19,20,22,23}
      } },
    // row 127 -- CineDiopterKalCenterX
    { "kal_center_x", "rst_CineDiopterKalCenterX", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 128 -- CineDiopterKalCenterY
    { "kal_center_y", "rst_CineDiopterKalCenterY", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 129 -- CineDiopterKalAngle
    { "kal_angle", "rst_CineDiopterKalAngle", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 130 -- CineDiopterKalTwist
    { "kal_twist", "rst_CineDiopterKalTwist", AL_TOOL_KALEIDO, SEC_KAL_PATTERN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0xA26AC3u) } }, // KalMode{0,1,6,7,9,11,13,14,17,21,23}
      } },
    // row 131 -- CineDiopterKalEdgeWrap
    { "kal_edge_wrap", "rst_CineDiopterKalEdgeWrap", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 132 -- CineDiopterKalRingCount
    { "kal_ring_count", "rst_CineDiopterKalRingCount", AL_TOOL_KALEIDO, SEC_KAL_PATTERN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x400240u) } }, // KalMode{6,9,22}
      } },
    // row 133 -- CineDiopterKalStarSharp
    { "kal_star_sharp", "rst_CineDiopterKalStarSharp", AL_TOOL_KALEIDO, SEC_KAL_PATTERN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x80u) } }, // KalMode{7}
      } },
    // row 134 -- CineDiopterKalShapeBias
    { "kal_shape_bias", "rst_CineDiopterKalShapeBias", AL_TOOL_KALEIDO, SEC_KAL_PATTERN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x8B0000u) } }, // KalMode{16,17,19,23}
      } },
    // row 135 -- CineDiopterKalSourceAngle
    { "kal_source_angle", "rst_CineDiopterKalSourceAngle", AL_TOOL_KALEIDO, SEC_KAL_SOURCE,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0xFFFEFBu) } }, // KalMode{0,1,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23}
      } },
    // row 136 -- CineDiopterKalSourceZoom
    { "kal_source_zoom", "rst_CineDiopterKalSourceZoom", AL_TOOL_KALEIDO, SEC_KAL_SOURCE,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 137 -- CineDiopterKalSourceOffsetX
    { "kal_source_offset_x", "rst_CineDiopterKalSourceOffsetX", AL_TOOL_KALEIDO, SEC_KAL_SOURCE,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 138 -- CineDiopterKalSourceOffsetY
    { "kal_source_offset_y", "rst_CineDiopterKalSourceOffsetY", AL_TOOL_KALEIDO, SEC_KAL_SOURCE,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 139 -- CineDiopterKalSourceSpin
    { "kal_source_spin", "rst_CineDiopterKalSourceSpin", AL_TOOL_KALEIDO, SEC_KAL_SOURCE,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0xFFFEFBu) } }, // KalMode{0,1,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23}
      } },
    // row 140 -- CineDiopterKalFXBand
    { "kal_fx_band", "rst_CineDiopterKalFXBand", AL_TOOL_KALEIDO, SEC_KAL_FX,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0xA6F600u) } }, // KalMode{9,10,12,13,14,15,17,18,21,23}
      } },
    // row 141 -- CineDiopterKalFXAmount
    { "kal_fx_amount", "rst_CineDiopterKalFXAmount", AL_TOOL_KALEIDO, SEC_KAL_FX,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x72D800u) } }, // KalMode{11,12,14,15,17,20,21,22}
      } },
    // row 142 -- CineDiopterKalFXFlow
    { "kal_fx_flow", "rst_CineDiopterKalFXFlow", AL_TOOL_KALEIDO, SEC_KAL_FX,
      true, false, TIER_MODE, 2,
      {
        { 1, { S("CineDiopterKalMode", 0xFFFA00u) } }, // KalMode{9,11,12,13,14,15,16,17,18,19,20,21,22,23}
        { 2, { S("CineDiopterKalMode", 0x538u), P(PRED_CELL_BREATHE_ARMED) } }, // KalMode{3,4,5,8,10} AND PRED_CELL_BREATHE_ARMED
      } },
    // row 143 -- CineDiopterKalFXFreq
    { "kal_fx_freq", "rst_CineDiopterKalFXFreq", AL_TOOL_KALEIDO, SEC_KAL_FX,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x81000u) } }, // KalMode{12,19}
      } },
    // row 144 -- CineDiopterKalSeamSoften
    { "kal_seam_soften", "rst_CineDiopterKalSeamSoften", AL_TOOL_KALEIDO, SEC_KAL_PATTERN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x1u) } }, // KalMode{0}
      } },
    // row 145 -- CineDiopterKalBlend
    { "kal_blend", "rst_CineDiopterKalBlend", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 146 -- CineDiopterKalDebugView
    { "kal_debug", "rst_CineDiopterKalDebugView", AL_TOOL_KALEIDO, SEC_KAL_HEADER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 147 -- CineDiopterKalCellSizeVar
    { "kal_cell_size_var", "rst_CineDiopterKalCellSizeVar", AL_TOOL_KALEIDO, SEC_KAL_CELL,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x500538u) } }, // KalMode{3,4,5,8,10,20,22}
      } },
    // row 148 -- CineDiopterKalCellBreathe
    { "kal_cell_breathe", "rst_CineDiopterKalCellBreathe", AL_TOOL_KALEIDO, SEC_KAL_CELL,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x500538u) } }, // KalMode{3,4,5,8,10,20,22}
      } },
    // row 149 -- CineDiopterKalCellSubdiv
    { "kal_cell_subdiv", "rst_CineDiopterKalCellSubdiv", AL_TOOL_KALEIDO, SEC_KAL_CELL,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x110u) } }, // KalMode{4,8}
      } },
    // row 150 -- CineDiopterKalCellMerge
    { "kal_cell_merge", "rst_CineDiopterKalCellMerge", AL_TOOL_KALEIDO, SEC_KAL_CELL,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0x100000u) } }, // KalMode{20}
      } },
    // row 151 -- CineDiopterKalCellTint
    { "kal_cell_tint", "rst_CineDiopterKalCellTint", AL_TOOL_KALEIDO, SEC_KAL_CELL,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMode", 0xD0853Cu) } }, // KalMode{2,3,4,5,8,10,15,20,22,23}
      } },
    // row 152 -- CineDiopterKalMotionMode
    { "kal_motion_mode", "rst_CineDiopterKalMotionMode", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 153 -- CineDiopterKalSpeed
    { "kal_speed", "rst_CineDiopterKalSpeed", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0xFDEu) } }, // KalMotion{1,2,3,4,6,7,8,9,10,11}
      } },
    // row 154 -- CineDiopterKalMotionAngle
    { "kal_motion_angle", "rst_CineDiopterKalMotionAngle", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 2,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x42u) } }, // KalMotion{1,6}
        { 2, { S("CineDiopterKalMotionMode", 0x4u), S("CineDiopterKalPulseTarget", 0x4u) } }, // KalMotion{2} AND KalPulseTarget=Offset
      } },
    // row 155 -- CineDiopterKalSweepRange
    { "kal_sweep_range", "rst_CineDiopterKalSweepRange", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0xA02u) } }, // KalMotion{1,9,11}
      } },
    // row 156 -- CineDiopterKalPingPong
    { "kal_ping_pong", "rst_CineDiopterKalPingPong", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x2u) } }, // KalMotion{1}
      } },
    // row 157 -- CineDiopterKalPulseTarget
    { "kal_pulse_target", "rst_CineDiopterKalPulseTarget", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x4u) } }, // KalMotion{2}
      } },
    // row 158 -- CineDiopterKalPulseAmt
    { "kal_pulse_amt", "rst_CineDiopterKalPulseAmt", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x404u) } }, // KalMotion{2,10}
      } },
    // row 159 -- CineDiopterKalWaveAmp
    { "kal_wave_amp", "rst_CineDiopterKalWaveAmp", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterKalMotionMode", 0x8u), S("CineDiopterKalMode", 0xE7FEC3u) } }, // KalMotion{3} AND KalMode{0,1,6,7,9,10,11,12,13,14,15,16,17,18,21,22,23}
      } },
    // row 160 -- CineDiopterKalWaveFreq
    { "kal_wave_freq", "rst_CineDiopterKalWaveFreq", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterKalMotionMode", 0x8u), S("CineDiopterKalMode", 0xE7FEC3u) } }, // KalMotion{3} AND KalMode{0,1,6,7,9,10,11,12,13,14,15,16,17,18,21,22,23}
      } },
    // row 161 -- CineDiopterKalPathFreqX
    { "kal_path_fx", "rst_CineDiopterKalPathFreqX", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x10u) } }, // KalMotion{4}
      } },
    // row 162 -- CineDiopterKalPathFreqY
    { "kal_path_fy", "rst_CineDiopterKalPathFreqY", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x10u) } }, // KalMotion{4}
      } },
    // row 163 -- CineDiopterKalPathPhase
    { "kal_path_phase", "rst_CineDiopterKalPathPhase", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x10u) } }, // KalMotion{4}
      } },
    // row 164 -- CineDiopterKalPathAmp
    { "kal_path_amp", "rst_CineDiopterKalPathAmp", AL_TOOL_KALEIDO, SEC_KAL_MOTION,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalMotionMode", 0x9D0u) } }, // KalMotion{4,6,7,8,11}
      } },
    // row 165 -- CineDiopterKalSpinMode
    { "kal_spin_mode", "rst_CineDiopterKalSpinMode", AL_TOOL_KALEIDO, SEC_KAL_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 166 -- CineDiopterKalSpinSpeed
    { "kal_spin_speed", "rst_CineDiopterKalSpinSpeed", AL_TOOL_KALEIDO, SEC_KAL_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalSpinMode", 0x1u) } }, // KalSpin=Constant
      } },
    // row 167 -- CineDiopterKalSpinTravel
    { "kal_spin_travel", "rst_CineDiopterKalSpinTravel", AL_TOOL_KALEIDO, SEC_KAL_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalSpinMode", 0xEu) } }, // KalSpin{1,2,3}
      } },
    // row 168 -- CineDiopterKalSpinDuration
    { "kal_spin_duration", "rst_CineDiopterKalSpinDuration", AL_TOOL_KALEIDO, SEC_KAL_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalSpinMode", 0xEu) } }, // KalSpin{1,2,3}
      } },
    // row 169 -- CineDiopterKalSpinBounce
    { "kal_spin_bounce", "rst_CineDiopterKalSpinBounce", AL_TOOL_KALEIDO, SEC_KAL_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalSpinMode", 0x8u) } }, // KalSpin{3}
      } },
    // row 170 -- CineDiopterKalSpinDelay
    { "kal_spin_delay", "rst_CineDiopterKalSpinDelay", AL_TOOL_KALEIDO, SEC_KAL_SPIN,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalSpinMode", 0xEu) } }, // KalSpin{1,2,3}
      } },
    // row 171 -- CineDiopterKalProtectMode
    { "kal_protect_mode", "rst_CineDiopterKalProtectMode", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 172 -- CineDiopterKalProtectRadius
    { "kal_protect_radius", "rst_CineDiopterKalProtectRadius", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalProtectMode", 0xAu) } }, // Protect{1,3}
      } },
    // row 173 -- CineDiopterKalProtectFeather
    { "kal_protect_feather", "rst_CineDiopterKalProtectFeather", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalProtectMode", 0xAu) } }, // Protect{1,3}
      } },
    // row 174 -- CineDiopterKalProtectAnchor
    { "kal_protect_anchor", "rst_CineDiopterKalProtectAnchor", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalProtectMode", 0xAu) } }, // Protect{1,3}
      } },
    // row 175 -- CineDiopterKalProtectCenterX
    { "kal_protect_center_x", "rst_CineDiopterKalProtectCenterX", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterKalProtectMode", 0xAu), S("CineDiopterKalProtectAnchor", 0x2u) } }, // Protect{1,3} AND Anchor=Fixed
      } },
    // row 176 -- CineDiopterKalProtectCenterY
    { "kal_protect_center_y", "rst_CineDiopterKalProtectCenterY", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      false, false, TIER_MODE, 1,
      {
        { 2, { S("CineDiopterKalProtectMode", 0xAu), S("CineDiopterKalProtectAnchor", 0x2u) } }, // Protect{1,3} AND Anchor=Fixed
      } },
    // row 177 -- CineDiopterKalDepthCut
    { "kal_depth_cut", "rst_CineDiopterKalDepthCut", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalProtectMode", 0xCu) } }, // Protect{2,3}
      } },
    // row 178 -- CineDiopterKalDepthFeatherM
    { "kal_depth_feather", "rst_CineDiopterKalDepthFeatherM", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalProtectMode", 0xCu) } }, // Protect{2,3}
      } },
    // row 179 -- CineDiopterKalDepthInvert
    { "kal_depth_invert", "rst_CineDiopterKalDepthInvert", AL_TOOL_KALEIDO, SEC_KAL_PROTECT,
      true, false, TIER_MODE, 1,
      {
        { 1, { S("CineDiopterKalProtectMode", 0xCu) } }, // Protect{2,3}
      } },
    // row 180 -- CineDiopterKalFreezeTime
    { "kal_freeze", "rst_CineDiopterKalFreezeTime", AL_TOOL_KALEIDO, SEC_FOOTER,
      false, false, TIER_MODE, 1,
      {
        { 0, {} }, // ALWAYS
      } },
    // row 181 -- CineDiopterKalFreezeAt
    { "kal_freeze_at", "rst_CineDiopterKalFreezeAt", AL_TOOL_KALEIDO, SEC_FOOTER,
      false, false, TIER_UNARMED, 1,
      {
        { 1, { S("CineDiopterKalFreezeTime", 0x2u) } }, // KalFreeze=on
      } },
};

#undef S
#undef P

// ---------------------------------------------------------------------------
// Derived focus quantities
// ---------------------------------------------------------------------------

F32 alDiopterStateLensFocusM(const ALDiopterState& st)
{
    // The renderer solves the lens plane against the RESOLVED base plane
    // (pipeline.cpp:14203-14245): clamp first, exactly as the resolver does.
    const F32 base = llclamp(st.mResolvedBaseFocusM, 0.1f, 4096.f);
    const F32 power = llclamp(st.mPower, -10.f, 10.f);
    return alDiopterLensFocusM(st.mLensFocusMode, st.mLensFocusM, base, power);
}

F32 alDiopterStateStrengthD(const ALDiopterState& st)
{
    const F32 base = llclamp(st.mResolvedBaseFocusM, 0.1f, 4096.f);
    return alDiopterStrengthD(alDiopterStateLensFocusM(st), base);
}

// ---------------------------------------------------------------------------
// Predicate dispatch
// ---------------------------------------------------------------------------

bool alDiopterEvalPred(ALDiopterPred pred, const ALDiopterState& st)
{
    switch (pred)
    {
        case PRED_WARP_ARMED:
            return alDiopterWarpArmed((S32)st.mPatternMode, st.mRingFold,
                                      st.mTwistDeg, st.mLobeAmt,
                                      st.mPatternZoom);
        case PRED_HANDHELD_ARMED:
            return alDiopterHandheldArmed(st.mHandheld);
        case PRED_GHOSTS_ARMED:
            return alDiopterGhostsArmed(st.mGhostCount);
        case PRED_SEAM_ARMED:
            return alDiopterSeamArmed(st.mSeamPx);
        case PRED_REFRACTION_ARMED:
            return alDiopterRefractionArmed((S32)st.mGlassProfile);
        case PRED_ABERRATION_ARMED:
            return alDiopterAberrationArmed(alDiopterStateStrengthD(st),
                                            st.mCharacter);
        case PRED_FIELD_CURVE_ARMED:
            return alDiopterFieldCurveArmed(alDiopterStateStrengthD(st),
                                            st.mCharacter, st.mFieldScale);
        case PRED_CELL_BREATHE_ARMED:
            return alDiopterCellBreatheArmed(st.mCellBreathe);
        case PRED_BASE_FOCUS_MANUAL:
            return alDiopterBaseFocusManual(st.mBaseFocusProvenance);
        case PRED_EDGE_WOBBLE_ARMED:
            return alDiopterEdgeWobbleArmed(st.mWobbleAmt);
        case PRED_SHAPE_HAS_EDGE:
            return alDiopterShapeHasEdge(st.mShape, st.mPlacementMode);
        case PRED_LENS_POWERED:
            return alDiopterLensPowered(alDiopterStateStrengthD(st));
        case PRED_NONE:
        case AL_PRED_COUNT:
            break;
    }
    return false;   // PRED_NONE has no truth value; callers must not ask
}

const char* alDiopterPredName(ALDiopterPred pred)
{
    switch (pred)
    {
        case PRED_NONE:              return "PRED_NONE";
        case PRED_WARP_ARMED:        return "PRED_WARP_ARMED";
        case PRED_HANDHELD_ARMED:    return "PRED_HANDHELD_ARMED";
        case PRED_GHOSTS_ARMED:      return "PRED_GHOSTS_ARMED";
        case PRED_SEAM_ARMED:        return "PRED_SEAM_ARMED";
        case PRED_REFRACTION_ARMED:  return "PRED_REFRACTION_ARMED";
        case PRED_ABERRATION_ARMED:  return "PRED_ABERRATION_ARMED";
        case PRED_FIELD_CURVE_ARMED: return "PRED_FIELD_CURVE_ARMED";
        case PRED_CELL_BREATHE_ARMED:return "PRED_CELL_BREATHE_ARMED";
        case PRED_BASE_FOCUS_MANUAL: return "PRED_BASE_FOCUS_MANUAL";
        case PRED_EDGE_WOBBLE_ARMED: return "PRED_EDGE_WOBBLE_ARMED";
        case PRED_SHAPE_HAS_EDGE:    return "PRED_SHAPE_HAS_EDGE";
        case PRED_LENS_POWERED:      return "PRED_LENS_POWERED";
        case AL_PRED_COUNT:          break;
    }
    return "PRED_<invalid>";
}

// ---------------------------------------------------------------------------
// Term-driving setting lookup
// ---------------------------------------------------------------------------

S32 alDiopterStateValue(const ALDiopterState& st, const char* setting)
{
    struct Entry { const char* mName; U32 mValue; };
    const Entry entries[] =
    {
        { "CineDiopterShape",              st.mShape },
        { "CineDiopterPlacementMode",      st.mPlacementMode },
        { "CineDiopterContent",            st.mContent },
        { "CineDiopterGlassProfile",       st.mGlassProfile },
        { "CineDiopterApertureShape",      st.mApertureShape },
        { "CineDiopterPatternMode",        st.mPatternMode },
        { "CineDiopterMotionMode",         st.mMotionMode },
        { "CineDiopterPulseTarget",        st.mPulseTarget },
        { "CineDiopterSpinMode",           st.mSpinMode },
        { "CineDiopterLensFocusMode",      st.mLensFocusMode },
        { "CineDiopterFreeze",             st.mFreeze ? 1u : 0u },
        { "CineDiopterKalMode",            st.mKalMode },
        { "CineDiopterKalMotionMode",      st.mKalMotionMode },
        { "CineDiopterKalPulseTarget",     st.mKalPulseTarget },
        { "CineDiopterKalSpinMode",        st.mKalSpinMode },
        { "CineDiopterKalProtectMode",     st.mKalProtectMode },
        { "CineDiopterKalProtectAnchor",   st.mKalProtectAnchor },
        { "CineDiopterKalFreezeTime",      st.mKalFreeze ? 1u : 0u },
    };
    if (setting)
    {
        for (const Entry& e : entries)
        {
            if (strcmp(e.mName, setting) == 0)
            {
                return (S32)e.mValue;
            }
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Clause / row evaluation
// ---------------------------------------------------------------------------

bool alDiopterToolMatches(U32 row_tool, U32 tool_mode)
{
    return ((row_tool >> (tool_mode & 1u)) & 1u) != 0u;
}

bool alDiopterEvalClause(const ALDiopterRelevance& row, S32 clause_index,
                         const ALDiopterState& st)
{
    // The implicit terms (design doc 2.3): every non-UNCONDITIONAL row's
    // clause is ANDed with CineDiopterEnabled == on and the tool match.
    // (UNCONDITIONAL rows have no clauses, so they never reach here.)
    if (!st.mEnabled)
    {
        return false;
    }
    if (!alDiopterToolMatches(row.mTool, st.mToolMode))
    {
        return false;
    }
    const ALDiopterClause& clause = row.mClauses[clause_index];
    for (S32 t = 0; t < (S32)clause.mTermCount; ++t)
    {
        const ALDiopterTerm& term = clause.mTerms[t];
        bool pass;
        if (term.mPred != PRED_NONE)
        {
            pass = alDiopterEvalPred(term.mPred, st);
        }
        else
        {
            const S32 value = alDiopterStateValue(st, term.mSetting);
            pass = (value >= 0) && (value < 32) &&
                   (((term.mMask >> value) & 1u) != 0u);
        }
        if (term.mNegate)
        {
            pass = !pass;
        }
        if (!pass)
        {
            return false;
        }
    }
    return true;    // ALWAYS clauses (mTermCount == 0) land here directly
}

bool alDiopterEvaluate(const ALDiopterRelevance& row, const ALDiopterState& st)
{
    if (row.mUnconditional)
    {
        // diopter_enabled: sits outside the gate it controls; the only
        // control relevant while the master enable is off (Tier 0).
        return true;
    }
    for (S32 c = 0; c < (S32)row.mClauseCount; ++c)
    {
        if (alDiopterEvalClause(row, c, st))
        {
            return true;
        }
    }
    return false;
}

std::set<std::string> alDiopterRelevantSet(const ALDiopterState& st)
{
    std::set<std::string> out;
    for (S32 r = 0; r < AL_RELEVANCE_COUNT; ++r)
    {
        if (alDiopterEvaluate(sRelevance[r], st))
        {
            out.insert(sRelevance[r].mCtrlName);
        }
    }
    return out;
}
