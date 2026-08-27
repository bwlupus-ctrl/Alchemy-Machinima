# Ultimate Diopter — complete relevance table (revision 6, all 181 rows)

Companion artifact to diopter_smart_ui_design.md §2.3. This file is the implementation source of truth for sRelevance.

Generated from floater_ultimate_diopter.xml, the renderer consumption paths in the current source tree, and the revision-6 schema in design-doc §2.3.

Revision 6 · 2026-08-27

---

## Schema and representation

The data columns have full ALDiopterRelevance schema parity: mCtrlName, mResetName, mTool, mSection, mPresetOwned, mUnconditional, mTier, mClauseCount, and mClauses. The # column is a presentation-only audit index. Setting (DERIVED) is the sole non-schema data column: it is the control→setting binding read from floater_ultimate_diopter.xml.

Clauses are ORed; terms inside a clause are ANDed. Each clause below states its explicit mTermCount. C1{mTermCount=0}: ALWAYS is a real, vacuously passing clause; the row still depends on the implicit Enabled term and mTool. Every non-unconditional row has mClauseCount ≥ 1. The sole unconditional row, diopter_enabled, has mClauseCount=0 and no clauses.

Every non-unconditional row carries the implicit CineDiopterEnabled == on term, applied once by the evaluator. mTool independently gates the selected tool. Neither implicit gate is included in the displayed per-clause mTermCount.

mTier describes treatment when the row's relevance clauses fail: TIER_MODE hides mode-excluded controls; TIER_OVERRIDDEN disables/dims controls whose value is overridden; TIER_UNARMED disables/dims controls awaiting an arming value. Independently, mPresetOwned=true adds the preset-override dimming path while a non-Custom preset owns the value; it does not replace the row's clause-failure tier.

mPresetOwned is true iff the Setting appears in the 44-field diopter or 49-field kaleido owned list at llviewerfloaterreg.cpp:571-588 or llviewerfloaterreg.cpp:627-645, cross-checked against materializeDiopterPreset at pipeline.cpp:13745-13809 and materializeKaleidoPreset at pipeline.cpp:14858-14931.

## Asserted invariants

| Invariant | Value |
|---|---|
| Total rows | 181, numbered consecutively 1–181 |
| Unique controls | 181; no duplicate mCtrlName values |
| Unique derived settings | 181; no duplicate Setting values |
| CineDiopter settings coverage | Every one of the 181 CineDiopter* settings in settings.xml appears exactly once |
| mUnconditional=true | Exactly 1: row 1, diopter_enabled |
| mResetName=nullptr | Exactly 1: row 1, diopter_enabled |
| mClauseCount=0 | Exactly 1 and iff mUnconditional=true: row 1 |
| Total explicit clauses | 194 |
| mPresetOwned=true | 93: 44 diopter + 49 kaleido |
| Current XML rows per tab | Main 14 · Shape 22 · Focus 15 · Glass 27 · Bokeh 16 · Motion 29 · Kaleido 28 · Kal Anim 30 |
| Proposed §3.6 rows per tab/group | Main 6 · Shape 28 · Focus 15 · Glass 12 · Halo & Ghosts 19 · Bokeh 12 · Motion 29 · Kaleido 28 · Kal Anim 28 · persistent Footer 4 |

## mSection inventory and counts

The exact section IDs already used by the §2.3 illustrative rows are retained. Additional IDs are direct uppercase identifiers for the named §3.6 bands/groups. Section counts count controls, not visual rows after §3.7 pairs X/Y controls.

| mSection | Proposed §3.6 location/group | Rows |
|---|---|---:|
| SEC_HEADER | Main header | 6 |
| SEC_FRAMING | Shape header/framing | 9 |
| SEC_FOOTER | Persistent capture footer | 4 |
| SEC_SHAPE_EDGE | Shape Band 1, edge/bitmask-gated | 8 |
| SEC_SHAPE_SPECIFIC | Shape Band 2, shape-specific | 11 |
| SEC_FOCUS_BASE | Focus base-focus header | 2 |
| SEC_FOCUS_LENS | Focus lens-focus pair/group | 3 |
| SEC_FOCUS_FALLOFF | Focus falloff group | 8 |
| SEC_FOCUS_WINDOW | Focus Window Surround | 2 |
| SEC_GLASS_PROFILE | Glass profile header/band | 7 |
| SEC_GLASS_CHARACTER | Glass Character group | 5 |
| SEC_HALO_ARMING | Halo & Ghosts warp-arming row | 5 |
| SEC_HALO_PARAMS | Halo & Ghosts warp dependents | 6 |
| SEC_GHOSTS | Halo & Ghosts, Ghost Copies group | 8 |
| SEC_BOKEH_APERTURE | Bokeh aperture band + Cat's-Eye | 8 |
| SEC_GLASS_OPTICS | Bokeh magnify pair; exact illustrative §2.3 ID | 2 |
| SEC_BOKEH_SEAM | Bokeh Seam Double pair | 2 |
| SEC_MOTION_PARAMS | Motion header + 12-mode band | 18 |
| SEC_HANDHELD | Motion Handheld group | 5 |
| SEC_SPIN | Motion Spin group | 6 |
| SEC_KAL_HEADER | Kaleido header | 8 |
| SEC_KAL_PATTERN | Kaleido pattern band, geometric controls | 6 |
| SEC_KAL_FX | Kaleido pattern-band FX controls | 4 |
| SEC_KAL_SOURCE | Kaleido standing Source group | 5 |
| SEC_KAL_CELL | Kaleido standing Cell group | 5 |
| SEC_KAL_MOTION | Kal Anim motion header/band | 13 |
| SEC_KAL_SPIN | Kal Anim Spin group | 6 |
| SEC_KAL_PROTECT | Kal Anim Protect group | 9 |
| **Total** |  | **181** |

Grounded ambiguities are explicit rather than silent:

- §3.6 calls Shape Band 2 “the 10 shape-specific sliders,” but the complete inventory has 11 such settings (rows 26–36). All 11 are assigned SEC_SHAPE_SPECIFIC.
- §2.2 says Edge Wrap is live in every kaleido mode but omits it from the Header member list. Row 131 is assigned SEC_KAL_HEADER because that is the only always-visible kaleido region.
- §3.6 omits Kaleido Freeze/Freeze At from its Kal Anim list. Rows 180–181 are assigned SEC_FOOTER by the same persistent capture-control rule used for rows 12–13.

## Predicate glossary (12; exact renderer expressions)

Each entry quotes source text exactly. The UI truth rule names the state-level interpretation used by the relevance evaluator; per-pixel conditions such as the current mask sample do not become UI state.

| Predicate | UI truth rule | Exact renderer expression and verified source |
|---|---|---|
| PRED_WARP_ARMED | The renderer's warp_active value. | <code>bool warp_active = (pattern_mode &gt; 0) \|\| (ring_fold &gt; 1e-3f) \|\|</code><br><code>                   (fabsf(twist_deg) &gt; 1e-2f) \|\| (lobe_amt &gt; 1e-3f) \|\|</code><br><code>                   (fabsf(pattern_zoom - 1.f) &gt; 1e-3f);</code> — pipeline.cpp:14347-14352 |
| PRED_REFRACTION_ARMED | profile &gt; 0; radius is uploaded with a positive minimum. | <code>if (profile &lt;= 0 \|\| radius &lt; 1e-4) return;</code> — ultimateDiopterGatherF.glsl:336; <code>const F32 eff_size = llclamp((F32)s_size * size_scale, 0.01f, 1.5f);</code> — pipeline.cpp:14279 |
| PRED_LENS_POWERED | The exact computed strength_d is greater than zero. | <code>F32 strength_d = llclamp(fabsf(1.f / lens_focus_m - 1.f / base_focus_m), 0.f, 10.f);</code> — pipeline.cpp:14246-14249 |
| PRED_ABERRATION_ARMED | strength_d &gt; 0 and character &gt; 0; the controlled scale itself is deliberately not part of the arming predicate. | <code>F32 axial_ca_m = strength_d * 0.004f * character * (F32)g_axial_scale * lens_focus_m;</code><br><code>F32 ca_mag = strength_d * 0.15f * character * (F32)g_ca_scale * 0.01f;</code><br><code>F32 edge_vig = strength_d * 0.02f * character * (F32)g_vig_scale;</code> — pipeline.cpp:14254-14256 |
| PRED_FIELD_CURVE_ARMED | The renderer's computed field_curve is greater than zero. | <code>F32 field_curve = llclamp(strength_d * 0.15f * character * (F32)g_field_scale, 0.f, 1.f);</code> — pipeline.cpp:14253 |
| PRED_GHOSTS_ARMED | ghostCount &gt; 0; m &gt; 0.001 is per-pixel and is not UI state. | <code>if (ghostCount &gt; 0 &amp;&amp; m &gt; 0.001)</code> — ultimateDiopterF.glsl:107 |
| PRED_SEAM_ARMED | The uploaded seam distance passes the shader's exact epsilon. | <code>if (diopter_comp.x &gt; 1e-4)</code> — ultimateDiopterF.glsl:90; <code>gUltimateDiopterProgram.uniform4f(LLShaderMgr::DIOPTER_COMP, seam_px,</code> — pipeline.cpp:14378 |
| PRED_HANDHELD_ARMED | handheld &gt; 0.f. | <code>if (handheld &gt; 0.f)</code> — pipeline.cpp:14154 |
| PRED_EDGE_WOBBLE_ARMED | The static edge-wobble coefficient diopter_shape4.x is greater than zero. | <code>llclamp((F32)s_wobble_amt, 0.f, 0.3f),</code> — pipeline.cpp:14311; <code>float wobY = diopter_shape4.x * sin(q.y * diopter_shape4.y)</code><br><code>           + diopter_shape4.w * sin(q.y * diopter_shape4.y * 1.31 + diopter_shape4.z);</code><br><code>float wobA = diopter_shape4.x * sin(ang * diopter_shape4.y)</code><br><code>           + diopter_shape4.w * sin(ang * diopter_shape4.y * 1.31 + diopter_shape4.z);</code> — ultimateDiopterGatherF.glsl:256-259 |
| PRED_SHAPE_HAS_EDGE | Neither On-Lens nor Full Frame: placement is framed and shape != 0. | <code>if (diopter_glass2.y &gt; 0.5)                      // On-Lens (edge to edge)</code> — ultimateDiopterGatherF.glsl:200; <code>else if (shape == 0)                             // Full Frame</code> — ultimateDiopterGatherF.glsl:234; the edge SDF is the following <code>else</code> branch at :238-259. |
| PRED_BASE_FOCUS_MANUAL | The initial slider value remains the provenance after the camera-focus branches fail to replace it. | <code>F32 base_focus_m = llclamp((F32)f_base_m, 0.1f, 4096.f);</code> — pipeline.cpp:14203; exact override branches are <code>if ((U32)f_focus_mode == 1U &amp;&amp; dof_focus_live)</code> at :14204 and <code>else if ((U32)f_focus_mode == 1U)</code> at :14213, with replacement guarded by <code>if (d &gt; 0.05f)</code> at :14208 and :14226. |
| PRED_CELL_BREATHE_ARMED | kal_cell.y &gt; 0.0, the Cell Breathe coefficient. | <code>s *= 1.0 + kal_cell.y * 0.35 * sin(ftime * KAL_TAU + hz * KAL_TAU);</code> — ultimateKaleidoF.glsl:117; upload mapping is <code>prog.uniform4f(LLShaderMgr::KAL_CELL, look.mCellSizeVar, look.mCellBreathe,</code> — pipeline.cpp:15050. |

PRED_NONE is not a truth-valued predicate. It marks a term that uses (mSetting, mMask), is excluded from the predicate-flip rule, and is not one of the 12 glossary predicates.

---

## The table

| # | mCtrlName | Setting (DERIVED) | mResetName | mTool | mSection | mPresetOwned | mUnconditional | mTier | mClauseCount | mClauses (OR of clauses; explicit mTermCount) |
|---:|---|---|---|---|---|---:|---:|---|---:|---|
| 1 | diopter_enabled | CineDiopterEnabled | nullptr | AL_TOOL_BOTH | SEC_HEADER | false | true | TIER_MODE | 0 | — |
| 2 | diopter_tool_mode | CineDiopterToolMode | rst_CineDiopterToolMode | AL_TOOL_BOTH | SEC_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 3 | diopter_preset | CineDiopterPreset | rst_CineDiopterPreset | AL_TOOL_DIOPTER | SEC_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 4 | diopter_quality | CineDiopterQuality | rst_CineDiopterQuality | AL_TOOL_DIOPTER | SEC_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 5 | diopter_blend | CineDiopterBlend | rst_CineDiopterBlend | AL_TOOL_DIOPTER | SEC_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 6 | diopter_center_x | CineDiopterCenterX | rst_CineDiopterCenterX | AL_TOOL_DIOPTER | SEC_FRAMING | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 7 | diopter_center_y | CineDiopterCenterY | rst_CineDiopterCenterY | AL_TOOL_DIOPTER | SEC_FRAMING | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 8 | diopter_size | CineDiopterSize | rst_CineDiopterSize | AL_TOOL_DIOPTER | SEC_FRAMING | false | false | TIER_MODE | 3 | C1{mTermCount=2}: Shape{2,3,4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=2}: PRED_REFRACTION_ARMED AND Place=Framed OR C3{mTermCount=2}: PRED_WARP_ARMED AND Place=Framed |
| 9 | diopter_angle | CineDiopterAngleDeg | rst_CineDiopterAngleDeg | AL_TOOL_DIOPTER | SEC_FRAMING | false | false | TIER_MODE | 2 | C1{mTermCount=2}: Shape{1,2,3,4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=1}: Place=OnLens |
| 10 | diopter_track | CineDiopterTrackMode | rst_CineDiopterTrackMode | AL_TOOL_DIOPTER | SEC_FRAMING | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 11 | diopter_placement | CineDiopterPlacementMode | rst_CineDiopterPlacementMode | AL_TOOL_DIOPTER | SEC_FRAMING | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 12 | diopter_freeze | CineDiopterFreeze | rst_CineDiopterFreeze | AL_TOOL_DIOPTER | SEC_FOOTER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 13 | diopter_freeze_at | CineDiopterFreezeAt | rst_CineDiopterFreezeAt | AL_TOOL_DIOPTER | SEC_FOOTER | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: Freeze=on |
| 14 | diopter_debug | CineDiopterDebugView | rst_CineDiopterDebugView | AL_TOOL_DIOPTER | SEC_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 15 | diopter_shape | CineDiopterShape | rst_CineDiopterShape | AL_TOOL_DIOPTER | SEC_FRAMING | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Place=Framed |
| 16 | diopter_content | CineDiopterContent | rst_CineDiopterContent | AL_TOOL_DIOPTER | SEC_FRAMING | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Place=Framed |
| 17 | diopter_stretch | CineDiopterStretch | rst_CineDiopterStretch | AL_TOOL_DIOPTER | SEC_FRAMING | false | false | TIER_MODE | 2 | C1{mTermCount=2}: Shape{1,2,3,4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=1}: Place=OnLens |
| 18 | diopter_feather | CineDiopterFeather | rst_CineDiopterFeather | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | false | false | TIER_MODE | 2 | C1{mTermCount=2}: Shape{1,3,4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=1}: Place=OnLens |
| 19 | diopter_invert | CineDiopterInvert | rst_CineDiopterInvert | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 20 | diopter_hollow | CineDiopterHollow | rst_CineDiopterHollow | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | true | false | TIER_MODE | 2 | C1{mTermCount=2}: Shape{3,4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=1}: PRED_WARP_ARMED |
| 21 | diopter_arc_len | CineDiopterArcLengthDeg | rst_CineDiopterArcLengthDeg | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | true | false | TIER_MODE | 2 | C1{mTermCount=2}: Shape{4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=1}: Place=OnLens |
| 22 | diopter_broken | CineDiopterBrokenCount | rst_CineDiopterBrokenCount | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | true | false | TIER_MODE | 2 | C1{mTermCount=2}: Shape{4,5,6,7,8,9,10,11} AND Place=Framed OR C2{mTermCount=1}: Place=OnLens |
| 23 | diopter_round | CineDiopterCornerRound | rst_CineDiopterCornerRound | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{6,7,9,10} AND Place=Framed |
| 24 | diopter_wobble_amt | CineDiopterWobbleAmt | rst_CineDiopterWobbleAmt | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | false | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_SHAPE_HAS_EDGE |
| 25 | diopter_wobble_freq | CineDiopterWobbleFreq | rst_CineDiopterWobbleFreq | AL_TOOL_DIOPTER | SEC_SHAPE_EDGE | false | false | TIER_UNARMED | 2 | C1{mTermCount=2}: PRED_EDGE_WOBBLE_ARMED AND PRED_SHAPE_HAS_EDGE OR C2{mTermCount=2}: Motion=Wave AND PRED_SHAPE_HAS_EDGE |
| 26 | diopter_poly_sides | CineDiopterPolySides | rst_CineDiopterPolySides | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{6} AND Place=Framed |
| 27 | diopter_star_points | CineDiopterStarPoints | rst_CineDiopterStarPoints | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{7} AND Place=Framed |
| 28 | diopter_star_inner | CineDiopterStarInner | rst_CineDiopterStarInner | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{7} AND Place=Framed |
| 29 | diopter_petal_count | CineDiopterPetalCount | rst_CineDiopterPetalCount | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{9} AND Place=Framed |
| 30 | diopter_petal_depth | CineDiopterPetalDepth | rst_CineDiopterPetalDepth | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{9} AND Place=Framed |
| 31 | diopter_blob_seed | CineDiopterBlobSeed | rst_CineDiopterBlobSeed | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{10} AND Place=Framed |
| 32 | diopter_blob_amt | CineDiopterBlobAmt | rst_CineDiopterBlobAmt | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{10} AND Place=Framed |
| 33 | diopter_crescent_bite | CineDiopterCrescentBite | rst_CineDiopterCrescentBite | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{11} AND Place=Framed |
| 34 | diopter_crescent_shift | CineDiopterCrescentShift | rst_CineDiopterCrescentShift | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{11} AND Place=Framed |
| 35 | diopter_squircle_pow | CineDiopterSquirclePow | rst_CineDiopterSquirclePow | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{5} AND Place=Framed |
| 36 | diopter_split_curve | CineDiopterSplitCurvature | rst_CineDiopterSplitCurvature | AL_TOOL_DIOPTER | SEC_SHAPE_SPECIFIC | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Shape{1} AND Place=Framed |
| 37 | diopter_focus_mode | CineDiopterFocusMode | rst_CineDiopterFocusMode | AL_TOOL_DIOPTER | SEC_FOCUS_BASE | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 38 | diopter_base_m | CineDiopterBaseFocusM | rst_CineDiopterBaseFocusM | AL_TOOL_DIOPTER | SEC_FOCUS_BASE | false | false | TIER_OVERRIDDEN | 1 | C1{mTermCount=1}: PRED_BASE_FOCUS_MANUAL |
| 39 | diopter_lens_mode | CineDiopterLensFocusMode | rst_CineDiopterLensFocusMode | AL_TOOL_DIOPTER | SEC_FOCUS_LENS | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 40 | diopter_power | CineDiopterPower | rst_CineDiopterPower | AL_TOOL_DIOPTER | SEC_FOCUS_LENS | false | false | TIER_OVERRIDDEN | 1 | C1{mTermCount=1}: LensMode=Power |
| 41 | diopter_lens_m | CineDiopterLensFocusM | rst_CineDiopterLensFocusM | AL_TOOL_DIOPTER | SEC_FOCUS_LENS | false | false | TIER_OVERRIDDEN | 1 | C1{mTermCount=1}: LensMode=Manual |
| 42 | diopter_focus_width | CineDiopterFocusWidthM | rst_CineDiopterFocusWidthM | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 43 | diopter_falloff | CineDiopterFalloffRate | rst_CineDiopterFalloffRate | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 44 | diopter_curve | CineDiopterFalloffCurve | rst_CineDiopterFalloffCurve | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 45 | diopter_near_str | CineDiopterNearStrength | rst_CineDiopterNearStrength | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 46 | diopter_far_str | CineDiopterFarStrength | rst_CineDiopterFarStrength | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 47 | diopter_max_blur | CineDiopterMaxBlurPx | rst_CineDiopterMaxBlurPx | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 48 | diopter_floor_max | CineDiopterFloorMaxBlurPx | rst_CineDiopterFloorMaxBlurPx | AL_TOOL_DIOPTER | SEC_FOCUS_WINDOW | false | false | TIER_UNARMED | 2 | C1{mTermCount=1}: PRED_FIELD_CURVE_ARMED OR C2{mTermCount=1}: Content=SharpWindow |
| 49 | diopter_spot_blur | CineDiopterSpotBlur | rst_CineDiopterSpotBlur | AL_TOOL_DIOPTER | SEC_FOCUS_WINDOW | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Content=SharpWindow |
| 50 | diopter_bokeh_hi | CineDiopterBokehHighlight | rst_CineDiopterBokehHighlight | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 51 | diopter_depth_edge | CineDiopterDepthEdgeM | rst_CineDiopterDepthEdgeM | AL_TOOL_DIOPTER | SEC_FOCUS_FALLOFF | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 52 | diopter_profile | CineDiopterGlassProfile | rst_CineDiopterGlassProfile | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 53 | diopter_ior | CineDiopterIOR | rst_CineDiopterIOR | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_REFRACTION_ARMED |
| 54 | diopter_thick | CineDiopterThickness | rst_CineDiopterThickness | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_REFRACTION_ARMED |
| 55 | diopter_rim_width | CineDiopterRimWidth | rst_CineDiopterRimWidth | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_REFRACTION_ARMED |
| 56 | diopter_rim_warp | CineDiopterRimWarp | rst_CineDiopterRimWarp | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_REFRACTION_ARMED |
| 57 | diopter_rim_caustic | CineDiopterRimCaustic | rst_CineDiopterRimCaustic | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_REFRACTION_ARMED |
| 58 | diopter_rim_darken | CineDiopterRimDarken | rst_CineDiopterRimDarken | AL_TOOL_DIOPTER | SEC_GLASS_PROFILE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: PRED_REFRACTION_ARMED |
| 59 | diopter_character | CineDiopterCharacter | rst_CineDiopterCharacter | AL_TOOL_DIOPTER | SEC_GLASS_CHARACTER | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_LENS_POWERED |
| 60 | diopter_ca_scale | CineDiopterCAScale | rst_CineDiopterCAScale | AL_TOOL_DIOPTER | SEC_GLASS_CHARACTER | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_ABERRATION_ARMED |
| 61 | diopter_axial_scale | CineDiopterAxialCAScale | rst_CineDiopterAxialCAScale | AL_TOOL_DIOPTER | SEC_GLASS_CHARACTER | false | false | TIER_UNARMED | 1 | C1{mTermCount=2}: PRED_ABERRATION_ARMED AND Content=Diopter |
| 62 | diopter_field_scale | CineDiopterFieldCurveScale | rst_CineDiopterFieldCurveScale | AL_TOOL_DIOPTER | SEC_GLASS_CHARACTER | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_ABERRATION_ARMED |
| 63 | diopter_vig_scale | CineDiopterEdgeVignetteScale | rst_CineDiopterEdgeVignetteScale | AL_TOOL_DIOPTER | SEC_GLASS_CHARACTER | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_ABERRATION_ARMED |
| 64 | diopter_ring_count | CineDiopterRingCount | rst_CineDiopterRingCount | AL_TOOL_DIOPTER | SEC_HALO_PARAMS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_WARP_ARMED |
| 65 | diopter_ring_fold | CineDiopterRingFold | rst_CineDiopterRingFold | AL_TOOL_DIOPTER | SEC_HALO_ARMING | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 66 | diopter_ring_phase | CineDiopterRingPhase | rst_CineDiopterRingPhase | AL_TOOL_DIOPTER | SEC_HALO_PARAMS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_WARP_ARMED |
| 67 | diopter_twist | CineDiopterTwistDeg | rst_CineDiopterTwistDeg | AL_TOOL_DIOPTER | SEC_HALO_ARMING | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 68 | diopter_lobe_amt | CineDiopterLobeAmt | rst_CineDiopterLobeAmt | AL_TOOL_DIOPTER | SEC_HALO_ARMING | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 69 | diopter_lobe_count | CineDiopterLobeCount | rst_CineDiopterLobeCount | AL_TOOL_DIOPTER | SEC_HALO_PARAMS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_WARP_ARMED |
| 70 | diopter_lobe_phase | CineDiopterLobePhaseDeg | rst_CineDiopterLobePhaseDeg | AL_TOOL_DIOPTER | SEC_HALO_PARAMS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_WARP_ARMED |
| 71 | diopter_ghost_count | CineDiopterGhostCount | rst_CineDiopterGhostCount | AL_TOOL_DIOPTER | SEC_GHOSTS | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 72 | diopter_ghost_spacing | CineDiopterGhostSpacing | rst_CineDiopterGhostSpacing | AL_TOOL_DIOPTER | SEC_GHOSTS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 73 | diopter_tangent_smear | CineDiopterTangentSmear | rst_CineDiopterTangentSmear | AL_TOOL_DIOPTER | SEC_GHOSTS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 74 | diopter_radial_smear | CineDiopterRadialSmear | rst_CineDiopterRadialSmear | AL_TOOL_DIOPTER | SEC_GHOSTS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 75 | diopter_ghost_threshold | CineDiopterGhostThreshold | rst_CineDiopterGhostThreshold | AL_TOOL_DIOPTER | SEC_GHOSTS | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 76 | diopter_ghost_knee | CineDiopterGhostKnee | rst_CineDiopterGhostKnee | AL_TOOL_DIOPTER | SEC_GHOSTS | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 77 | diopter_ghost_gain | CineDiopterGhostGain | rst_CineDiopterGhostGain | AL_TOOL_DIOPTER | SEC_GHOSTS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 78 | diopter_dispersion | CineDiopterDispersion | rst_CineDiopterDispersion | AL_TOOL_DIOPTER | SEC_GHOSTS | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_GHOSTS_ARMED |
| 79 | diopter_ap_shape | CineDiopterApertureShape | rst_CineDiopterApertureShape | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 80 | diopter_blades | CineDiopterBlades | rst_CineDiopterBlades | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Aperture{1,2} |
| 81 | diopter_blade_rot | CineDiopterBladeRotDeg | rst_CineDiopterBladeRotDeg | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Aperture{1,2,3} |
| 82 | diopter_blade_curve | CineDiopterBladeCurve | rst_CineDiopterBladeCurve | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Aperture{1,2,3} |
| 83 | diopter_ap_inner | CineDiopterApertureInner | rst_CineDiopterApertureInner | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Aperture{2} |
| 84 | diopter_anamorph | CineDiopterAnamorph | rst_CineDiopterAnamorph | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Aperture{4} |
| 85 | diopter_anam_angle | CineDiopterAnamorphAngleDeg | rst_CineDiopterAnamorphAngleDeg | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Aperture{4} |
| 86 | diopter_cat_eye | CineDiopterCatEye | rst_CineDiopterCatEye | AL_TOOL_DIOPTER | SEC_BOKEH_APERTURE | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 87 | diopter_pattern_mode | CineDiopterPatternMode | rst_CineDiopterPatternMode | AL_TOOL_DIOPTER | SEC_HALO_ARMING | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 88 | diopter_pattern_seg | CineDiopterPatternSegments | rst_CineDiopterPatternSegments | AL_TOOL_DIOPTER | SEC_HALO_PARAMS | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Faceted{1,2,3} |
| 89 | diopter_pattern_feed | CineDiopterPatternFeedDeg | rst_CineDiopterPatternFeedDeg | AL_TOOL_DIOPTER | SEC_HALO_PARAMS | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Faceted{1,2,3} |
| 90 | diopter_pattern_zoom | CineDiopterPatternZoom | rst_CineDiopterPatternZoom | AL_TOOL_DIOPTER | SEC_HALO_ARMING | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 91 | diopter_seam_px | CineDiopterSeamGhostPx | rst_CineDiopterSeamGhostPx | AL_TOOL_DIOPTER | SEC_BOKEH_SEAM | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 92 | diopter_seam_amt | CineDiopterSeamGhostAmt | rst_CineDiopterSeamGhostAmt | AL_TOOL_DIOPTER | SEC_BOKEH_SEAM | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_SEAM_ARMED |
| 93 | diopter_mag_scale | CineDiopterMagnifyScale | rst_CineDiopterMagnifyScale | AL_TOOL_DIOPTER | SEC_GLASS_OPTICS | false | false | TIER_UNARMED | 1 | C1{mTermCount=3}: Profile=Off AND Content=Diopter AND PRED_LENS_POWERED |
| 94 | diopter_mag_trim | CineDiopterMagnifyTrim | rst_CineDiopterMagnifyTrim | AL_TOOL_DIOPTER | SEC_GLASS_OPTICS | false | false | TIER_OVERRIDDEN | 1 | C1{mTermCount=2}: Profile=Off AND Content=Diopter |
| 95 | diopter_motion_mode | CineDiopterMotionMode | rst_CineDiopterMotionMode | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 96 | diopter_motion_speed | CineDiopterMotionSpeed | rst_CineDiopterMotionSpeed | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{1,2,3,4,5,6,7,8,9,10,11} |
| 97 | diopter_motion_angle | CineDiopterMotionAngleDeg | rst_CineDiopterMotionAngleDeg | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{1,6} |
| 98 | diopter_sweep_range | CineDiopterSweepRange | rst_CineDiopterSweepRange | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{1,9,11} |
| 99 | diopter_ping_pong | CineDiopterSweepPingPong | rst_CineDiopterSweepPingPong | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{1} |
| 100 | diopter_pulse_target | CineDiopterPulseTarget | rst_CineDiopterPulseTarget | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{2} |
| 101 | diopter_pulse_amt | CineDiopterPulseAmt | rst_CineDiopterPulseAmt | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 3 | C1{mTermCount=2}: Motion{2} AND PulseTarget=Size OR C2{mTermCount=3}: Motion{2} AND PulseTarget=Focus AND Content=Diopter OR C3{mTermCount=1}: Motion{10} |
| 102 | diopter_wave_amp | CineDiopterWaveAmp | rst_CineDiopterWaveAmp | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Motion{3} AND PRED_SHAPE_HAS_EDGE |
| 103 | diopter_path_fx | CineDiopterPathFreqX | rst_CineDiopterPathFreqX | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{4} |
| 104 | diopter_path_fy | CineDiopterPathFreqY | rst_CineDiopterPathFreqY | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{4} |
| 105 | diopter_path_phase | CineDiopterPathPhase | rst_CineDiopterPathPhase | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{4} |
| 106 | diopter_path_amp | CineDiopterPathAmp | rst_CineDiopterPathAmp | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{4,6,7,8,11} |
| 107 | diopter_stutter_rate | CineDiopterStutterRate | rst_CineDiopterStutterRate | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{5} |
| 108 | diopter_stutter_pos | CineDiopterStutterPos | rst_CineDiopterStutterPos | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{5} |
| 109 | diopter_stutter_angle | CineDiopterStutterAngleDeg | rst_CineDiopterStutterAngleDeg | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{5} |
| 110 | diopter_stutter_size | CineDiopterStutterSize | rst_CineDiopterStutterSize | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{5} |
| 111 | diopter_stutter_focus | CineDiopterStutterFocus | rst_CineDiopterStutterFocus | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Motion{5} AND Content=Diopter |
| 112 | diopter_stutter_smooth | CineDiopterStutterSmooth | rst_CineDiopterStutterSmooth | AL_TOOL_DIOPTER | SEC_MOTION_PARAMS | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Motion{5} |
| 113 | diopter_handheld | CineDiopterHandheld | rst_CineDiopterHandheld | AL_TOOL_DIOPTER | SEC_HANDHELD | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 114 | diopter_handheld_speed | CineDiopterHandheldSpeed | rst_CineDiopterHandheldSpeed | AL_TOOL_DIOPTER | SEC_HANDHELD | true | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_HANDHELD_ARMED |
| 115 | diopter_handheld_gait | CineDiopterHandheldGait | rst_CineDiopterHandheldGait | AL_TOOL_DIOPTER | SEC_HANDHELD | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_HANDHELD_ARMED |
| 116 | diopter_handheld_rot | CineDiopterHandheldRotDeg | rst_CineDiopterHandheldRotDeg | AL_TOOL_DIOPTER | SEC_HANDHELD | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: PRED_HANDHELD_ARMED |
| 117 | diopter_handheld_focus | CineDiopterHandheldFocus | rst_CineDiopterHandheldFocus | AL_TOOL_DIOPTER | SEC_HANDHELD | false | false | TIER_UNARMED | 1 | C1{mTermCount=2}: PRED_HANDHELD_ARMED AND Content=Diopter |
| 118 | diopter_spin_mode | CineDiopterSpinMode | rst_CineDiopterSpinMode | AL_TOOL_DIOPTER | SEC_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 119 | diopter_spin_speed | CineDiopterSpinSpeed | rst_CineDiopterSpinSpeed | AL_TOOL_DIOPTER | SEC_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Spin=Constant |
| 120 | diopter_spin_travel | CineDiopterSpinTravelDeg | rst_CineDiopterSpinTravelDeg | AL_TOOL_DIOPTER | SEC_SPIN | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Spin{1,2,3} |
| 121 | diopter_spin_duration | CineDiopterSpinDurationS | rst_CineDiopterSpinDurationS | AL_TOOL_DIOPTER | SEC_SPIN | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Spin{1,2,3} |
| 122 | diopter_spin_bounce | CineDiopterSpinBounce | rst_CineDiopterSpinBounce | AL_TOOL_DIOPTER | SEC_SPIN | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Spin{3} |
| 123 | diopter_spin_delay | CineDiopterSpinDelayS | rst_CineDiopterSpinDelayS | AL_TOOL_DIOPTER | SEC_SPIN | false | false | TIER_MODE | 1 | C1{mTermCount=1}: Spin{1,2,3} |
| 124 | kal_preset | CineDiopterKalPreset | rst_CineDiopterKalPreset | AL_TOOL_KALEIDO | SEC_KAL_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 125 | kal_mode | CineDiopterKalMode | rst_CineDiopterKalMode | AL_TOOL_KALEIDO | SEC_KAL_HEADER | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 126 | kal_segments | CineDiopterKalSegments | rst_CineDiopterKalSegments | AL_TOOL_KALEIDO | SEC_KAL_PATTERN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{0,1,2,3,4,5,6,7,8,10,13,15,16,18,19,20,22,23} |
| 127 | kal_center_x | CineDiopterKalCenterX | rst_CineDiopterKalCenterX | AL_TOOL_KALEIDO | SEC_KAL_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 128 | kal_center_y | CineDiopterKalCenterY | rst_CineDiopterKalCenterY | AL_TOOL_KALEIDO | SEC_KAL_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 129 | kal_angle | CineDiopterKalAngle | rst_CineDiopterKalAngle | AL_TOOL_KALEIDO | SEC_KAL_HEADER | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 130 | kal_twist | CineDiopterKalTwist | rst_CineDiopterKalTwist | AL_TOOL_KALEIDO | SEC_KAL_PATTERN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{0,1,6,7,9,11,13,14,17,21,23} |
| 131 | kal_edge_wrap | CineDiopterKalEdgeWrap | rst_CineDiopterKalEdgeWrap | AL_TOOL_KALEIDO | SEC_KAL_HEADER | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 132 | kal_ring_count | CineDiopterKalRingCount | rst_CineDiopterKalRingCount | AL_TOOL_KALEIDO | SEC_KAL_PATTERN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{6,9,22} |
| 133 | kal_star_sharp | CineDiopterKalStarSharp | rst_CineDiopterKalStarSharp | AL_TOOL_KALEIDO | SEC_KAL_PATTERN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{7} |
| 134 | kal_shape_bias | CineDiopterKalShapeBias | rst_CineDiopterKalShapeBias | AL_TOOL_KALEIDO | SEC_KAL_PATTERN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{16,17,19,23} |
| 135 | kal_source_angle | CineDiopterKalSourceAngle | rst_CineDiopterKalSourceAngle | AL_TOOL_KALEIDO | SEC_KAL_SOURCE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{0,1,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23} |
| 136 | kal_source_zoom | CineDiopterKalSourceZoom | rst_CineDiopterKalSourceZoom | AL_TOOL_KALEIDO | SEC_KAL_SOURCE | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 137 | kal_source_offset_x | CineDiopterKalSourceOffsetX | rst_CineDiopterKalSourceOffsetX | AL_TOOL_KALEIDO | SEC_KAL_SOURCE | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 138 | kal_source_offset_y | CineDiopterKalSourceOffsetY | rst_CineDiopterKalSourceOffsetY | AL_TOOL_KALEIDO | SEC_KAL_SOURCE | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 139 | kal_source_spin | CineDiopterKalSourceSpin | rst_CineDiopterKalSourceSpin | AL_TOOL_KALEIDO | SEC_KAL_SOURCE | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{0,1,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23} |
| 140 | kal_fx_band | CineDiopterKalFXBand | rst_CineDiopterKalFXBand | AL_TOOL_KALEIDO | SEC_KAL_FX | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{9,10,12,13,14,15,17,18,21,23} |
| 141 | kal_fx_amount | CineDiopterKalFXAmount | rst_CineDiopterKalFXAmount | AL_TOOL_KALEIDO | SEC_KAL_FX | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{11,12,14,15,17,20,21,22} |
| 142 | kal_fx_flow | CineDiopterKalFXFlow | rst_CineDiopterKalFXFlow | AL_TOOL_KALEIDO | SEC_KAL_FX | true | false | TIER_MODE | 2 | C1{mTermCount=1}: KalMode{9,11,12,13,14,15,16,17,18,19,20,21,22,23} OR C2{mTermCount=2}: KalMode{3,4,5,8,10} AND PRED_CELL_BREATHE_ARMED |
| 143 | kal_fx_freq | CineDiopterKalFXFreq | rst_CineDiopterKalFXFreq | AL_TOOL_KALEIDO | SEC_KAL_FX | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{12,19} |
| 144 | kal_seam_soften | CineDiopterKalSeamSoften | rst_CineDiopterKalSeamSoften | AL_TOOL_KALEIDO | SEC_KAL_PATTERN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{0} |
| 145 | kal_blend | CineDiopterKalBlend | rst_CineDiopterKalBlend | AL_TOOL_KALEIDO | SEC_KAL_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 146 | kal_debug | CineDiopterKalDebugView | rst_CineDiopterKalDebugView | AL_TOOL_KALEIDO | SEC_KAL_HEADER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 147 | kal_cell_size_var | CineDiopterKalCellSizeVar | rst_CineDiopterKalCellSizeVar | AL_TOOL_KALEIDO | SEC_KAL_CELL | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{3,4,5,8,10,20,22} |
| 148 | kal_cell_breathe | CineDiopterKalCellBreathe | rst_CineDiopterKalCellBreathe | AL_TOOL_KALEIDO | SEC_KAL_CELL | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{3,4,5,8,10,20,22} |
| 149 | kal_cell_subdiv | CineDiopterKalCellSubdiv | rst_CineDiopterKalCellSubdiv | AL_TOOL_KALEIDO | SEC_KAL_CELL | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{4,8} |
| 150 | kal_cell_merge | CineDiopterKalCellMerge | rst_CineDiopterKalCellMerge | AL_TOOL_KALEIDO | SEC_KAL_CELL | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{20} |
| 151 | kal_cell_tint | CineDiopterKalCellTint | rst_CineDiopterKalCellTint | AL_TOOL_KALEIDO | SEC_KAL_CELL | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMode{2,3,4,5,8,10,15,20,22,23} |
| 152 | kal_motion_mode | CineDiopterKalMotionMode | rst_CineDiopterKalMotionMode | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 153 | kal_speed | CineDiopterKalSpeed | rst_CineDiopterKalSpeed | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{1,2,3,4,6,7,8,9,10,11} |
| 154 | kal_motion_angle | CineDiopterKalMotionAngle | rst_CineDiopterKalMotionAngle | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 2 | C1{mTermCount=1}: KalMotion{1,6} OR C2{mTermCount=2}: KalMotion{2} AND KalPulseTarget=Offset |
| 155 | kal_sweep_range | CineDiopterKalSweepRange | rst_CineDiopterKalSweepRange | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{1,9,11} |
| 156 | kal_ping_pong | CineDiopterKalPingPong | rst_CineDiopterKalPingPong | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{1} |
| 157 | kal_pulse_target | CineDiopterKalPulseTarget | rst_CineDiopterKalPulseTarget | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{2} |
| 158 | kal_pulse_amt | CineDiopterKalPulseAmt | rst_CineDiopterKalPulseAmt | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{2,10} |
| 159 | kal_wave_amp | CineDiopterKalWaveAmp | rst_CineDiopterKalWaveAmp | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=2}: KalMotion{3} AND KalMode{0,1,6,7,9,10,11,12,13,14,15,16,17,18,21,22,23} |
| 160 | kal_wave_freq | CineDiopterKalWaveFreq | rst_CineDiopterKalWaveFreq | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=2}: KalMotion{3} AND KalMode{0,1,6,7,9,10,11,12,13,14,15,16,17,18,21,22,23} |
| 161 | kal_path_fx | CineDiopterKalPathFreqX | rst_CineDiopterKalPathFreqX | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{4} |
| 162 | kal_path_fy | CineDiopterKalPathFreqY | rst_CineDiopterKalPathFreqY | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{4} |
| 163 | kal_path_phase | CineDiopterKalPathPhase | rst_CineDiopterKalPathPhase | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{4} |
| 164 | kal_path_amp | CineDiopterKalPathAmp | rst_CineDiopterKalPathAmp | AL_TOOL_KALEIDO | SEC_KAL_MOTION | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalMotion{4,6,7,8,11} |
| 165 | kal_spin_mode | CineDiopterKalSpinMode | rst_CineDiopterKalSpinMode | AL_TOOL_KALEIDO | SEC_KAL_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 166 | kal_spin_speed | CineDiopterKalSpinSpeed | rst_CineDiopterKalSpinSpeed | AL_TOOL_KALEIDO | SEC_KAL_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalSpin=Constant |
| 167 | kal_spin_travel | CineDiopterKalSpinTravel | rst_CineDiopterKalSpinTravel | AL_TOOL_KALEIDO | SEC_KAL_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalSpin{1,2,3} |
| 168 | kal_spin_duration | CineDiopterKalSpinDuration | rst_CineDiopterKalSpinDuration | AL_TOOL_KALEIDO | SEC_KAL_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalSpin{1,2,3} |
| 169 | kal_spin_bounce | CineDiopterKalSpinBounce | rst_CineDiopterKalSpinBounce | AL_TOOL_KALEIDO | SEC_KAL_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalSpin{3} |
| 170 | kal_spin_delay | CineDiopterKalSpinDelay | rst_CineDiopterKalSpinDelay | AL_TOOL_KALEIDO | SEC_KAL_SPIN | true | false | TIER_MODE | 1 | C1{mTermCount=1}: KalSpin{1,2,3} |
| 171 | kal_protect_mode | CineDiopterKalProtectMode | rst_CineDiopterKalProtectMode | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 172 | kal_protect_radius | CineDiopterKalProtectRadius | rst_CineDiopterKalProtectRadius | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Protect{1,3} |
| 173 | kal_protect_feather | CineDiopterKalProtectFeather | rst_CineDiopterKalProtectFeather | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Protect{1,3} |
| 174 | kal_protect_anchor | CineDiopterKalProtectAnchor | rst_CineDiopterKalProtectAnchor | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Protect{1,3} |
| 175 | kal_protect_center_x | CineDiopterKalProtectCenterX | rst_CineDiopterKalProtectCenterX | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Protect{1,3} AND Anchor=Fixed |
| 176 | kal_protect_center_y | CineDiopterKalProtectCenterY | rst_CineDiopterKalProtectCenterY | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | false | false | TIER_MODE | 1 | C1{mTermCount=2}: Protect{1,3} AND Anchor=Fixed |
| 177 | kal_depth_cut | CineDiopterKalDepthCut | rst_CineDiopterKalDepthCut | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Protect{2,3} |
| 178 | kal_depth_feather | CineDiopterKalDepthFeatherM | rst_CineDiopterKalDepthFeatherM | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Protect{2,3} |
| 179 | kal_depth_invert | CineDiopterKalDepthInvert | rst_CineDiopterKalDepthInvert | AL_TOOL_KALEIDO | SEC_KAL_PROTECT | true | false | TIER_MODE | 1 | C1{mTermCount=1}: Protect{2,3} |
| 180 | kal_freeze | CineDiopterKalFreezeTime | rst_CineDiopterKalFreezeTime | AL_TOOL_KALEIDO | SEC_FOOTER | false | false | TIER_MODE | 1 | C1{mTermCount=0}: ALWAYS |
| 181 | kal_freeze_at | CineDiopterKalFreezeAt | rst_CineDiopterKalFreezeAt | AL_TOOL_KALEIDO | SEC_FOOTER | false | false | TIER_UNARMED | 1 | C1{mTermCount=1}: KalFreeze=on |

---

## Notes on individual rows

- Row 8 diopter_size carries Place=Framed in all three clauses. On-Lens discards eff_size and recomputes the radius (pipeline.cpp:14289-14296), so the slider reaches neither the mask, nor refraction, nor the warp annulus. Design-doc §2.1, Codex D1.
- Row 20 diopter_hollow deliberately does not carry Place=Framed in its second clause: ud_haloWarp reads hollow as the annulus inner radius regardless of placement, so it stays live under On-Lens whenever the warp is armed.
- Row 25 diopter_wobble_freq is the shared spatial-density control (Codex C2, adjudicated INTENDED): relevant under static Edge Wobble or Motion = Wave, both requiring an edge.
- Row 102 diopter_wave_amp carries PRED_SHAPE_HAS_EDGE because Wave is edge undulation and Full Frame / On-Lens have no edge (Codex C1, adjudicated INTENDED).
- Row 142 kal_fx_flow is the two-path row: direct ft consumers, or the five cell modes where ft only reaches output through kal_cellScale and therefore needs Cell Breathe armed (Codex 1.8, D2 — the direct set is modes {9, 11-23}, mask 0xFFFA00u).
- Rows 135 / 139 kal_source_angle / kal_source_spin exclude modes 2 (Mirror Tile) and 8 (Pinwheel Lattice), neither of which reads feed (Codex 1.7 plus the rev-2 self-audit).
- Rows 93–94 diopter_mag_scale / diopter_mag_trim need Profile = Off and Content = Diopter because magnify is forced to 1.f at pipeline.cpp:14252 and pipeline.cpp:14261-14264. Row 93 additionally needs PRED_LENS_POWERED: Magnify Scale is multiplied by strength_d, while Magnify Trim is additive in the exact expression at pipeline.cpp:14249-14251.
- Rows 27 and 93 mPresetOwned are false: CineDiopterStarPoints and CineDiopterMagnifyScale are absent from both the 44-entry owned array at llviewerfloaterreg.cpp:571-588 and the 44 materializer writes at pipeline.cpp:13764-13807. (This derivation caught the design doc's illustrative rows marking both true; the doc was corrected to false in the same revision, so doc and table now agree.)

## Rev-5 row disagreement audit

Exactly one row disagrees with revision 5 after re-derivation from the current source tree:

| Row | Control | Revision 5 | Current tree |
|---:|---|---|---|
| 93 | diopter_mag_scale | One 2-term clause: Profile=Off AND Content=Diopter | One 3-term clause: Profile=Off AND Content=Diopter AND PRED_LENS_POWERED; strength_d is computed at pipeline.cpp:14249 and multiplies CineDiopterMagnifyScale at pipeline.cpp:14251 |

All 181 control→setting bindings, 180 non-null reset names plus the one null reset, and all current XML tab assignments match revision 5. Row 94 diopter_mag_trim was rechecked and does not disagree: its trim term is additive at pipeline.cpp:14251.
