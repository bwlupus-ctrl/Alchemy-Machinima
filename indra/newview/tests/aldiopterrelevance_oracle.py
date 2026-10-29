#!/usr/bin/env python3
"""aldiopterrelevance_oracle.py -- consumed-set oracle extractor for the
Ultimate Diopter relevance model (design doc DIOPTER_SMART_UI_DESIGN.md
section 2.3, artifact table row "Oracle extractor").

Emits aldiopterrelevance_oracle.llsd: per-branch setting-consumption sets for
every renderer branch the relevance table models, as LLSD XML.  The TUT test
(tests/aldiopterrelevance_test.cpp) recombines these per-branch sets with the
renderer's gate structure (alOracleConsumedSet) and asserts, for each of the
1580 sweep states, that the recombined consumed-set equals the sRelevance
evaluator's relevant-set.

Extraction strategy (deliberately pragmatic, per section 2.3):

  * Where a fact is soundly machine-extractable, this script EXTRACTS it from
    the tree and DIFFS it against the encoded model below:
      - the LLCachedControl local -> setting-name map in
        LLPipeline::renderUltimateDiopter (pipeline.cpp);
      - the `motion == N` branch chain (pipeline.cpp, renderUltimateDiopter)
        and the per-branch set of consumed diopter settings;
      - the alKaleidoResolveMotion switch (pipeline.cpp) and the per-branch
        set of consumed ALKaleidoLook fields;
      - the warp_active / strength_d single-sourcing: pipeline.cpp must
        derive both THROUGH the shared helper calls (alDiopterWarpArmed /
        alDiopterStrengthD, call-swaps 1 and 3), and the helper bodies in
        aldiopterrelevance.h must keep the exact modeled expressions;
      - the presence of every cited shader gate line (ultimateDiopterGatherF /
        ultimateDiopterF / ultimateKaleidoF .glsl).
    A drift between tree and model is reported and fails the run, so a
    renderer edit invalidates the committed .llsd loudly instead of silently.

  * Where consumption is indirect (uniform packing, helper-function data flow
    such as kal_srcSample / kal_cellScale, per-shape SDF reads), a sound
    automatic extraction would need full GLSL data-flow analysis.  Those sets
    are ENCODED from the verified facts of the companion table
    doc/DIOPTER_RELEVANCE_TABLE.md (revision 6) and design doc sections
    2.1-2.2, with the manual source cited next to each entry.

Regenerate with:
    python indra/newview/tests/aldiopterrelevance_oracle.py \
        > indra/newview/tests/aldiopterrelevance_oracle.llsd
(from anywhere; the tree is located relative to this file).
"""

import os
import re
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
NEWVIEW = os.path.normpath(os.path.join(THIS_DIR, ".."))
PIPELINE = os.path.join(NEWVIEW, "pipeline.cpp")
RELEVANCE_H = os.path.join(NEWVIEW, "aldiopterrelevance.h")
SHADER_DIR = os.path.join(NEWVIEW, "app_settings", "shaders", "class1", "deferred")
GATHER_GLSL = os.path.join(SHADER_DIR, "ultimateDiopterGatherF.glsl")
DIOPTER_GLSL = os.path.join(SHADER_DIR, "ultimateDiopterF.glsl")
KALEIDO_GLSL = os.path.join(SHADER_DIR, "ultimateKaleidoF.glsl")
FLOATER_XML = os.path.join(NEWVIEW, "skins", "default", "xui", "en",
                           "floater_ultimate_diopter.xml")


# =========================================================================
# The encoded per-branch consumption model.
#
# Sets are stated in SETTING names (CineDiopter*); the emitter maps them to
# control names through the floater XML's control_name bindings, because the
# relevance evaluator's relevant-set is keyed by mCtrlName.
# =========================================================================

# ---- diopter: rows relevant whenever Enabled && tool == Diopter ----------
# Source: DIOPTER_RELEVANCE_TABLE.md, every AL_TOOL_DIOPTER row whose single
# clause is ALWAYS (rows 3-7, 10-12, 14, 19, 37, 39, 42-47, 50-52, 65, 67,
# 68, 71, 79, 86, 87, 90, 91, 95, 113, 118).
DIOPTER_ALWAYS = [
    "CineDiopterPreset", "CineDiopterQuality", "CineDiopterBlend",
    "CineDiopterCenterX", "CineDiopterCenterY", "CineDiopterTrackMode",
    "CineDiopterPlacementMode", "CineDiopterFreeze", "CineDiopterDebugView",
    "CineDiopterInvert", "CineDiopterFocusMode", "CineDiopterLensFocusMode",
    "CineDiopterFocusWidthM", "CineDiopterFalloffRate",
    "CineDiopterFalloffCurve", "CineDiopterNearStrength",
    "CineDiopterFarStrength", "CineDiopterMaxBlurPx",
    "CineDiopterBokehHighlight", "CineDiopterDepthEdgeM",
    "CineDiopterGlassProfile", "CineDiopterRingFold", "CineDiopterTwistDeg",
    "CineDiopterLobeAmt", "CineDiopterGhostCount",
    "CineDiopterApertureShape", "CineDiopterCatEye",
    "CineDiopterPatternMode", "CineDiopterPatternZoom",
    "CineDiopterSeamGhostPx", "CineDiopterMotionMode", "CineDiopterHandheld",
    "CineDiopterSpinMode",
]

# ---- kaleido: rows relevant whenever Enabled && tool == Kaleidoscope -----
# Source: DIOPTER_RELEVANCE_TABLE.md, AL_TOOL_KALEIDO ALWAYS rows (124, 125,
# 127-129, 131, 136-138, 145, 146, 152, 165, 171, 180).
KALEIDO_ALWAYS = [
    "CineDiopterKalPreset", "CineDiopterKalMode", "CineDiopterKalCenterX",
    "CineDiopterKalCenterY", "CineDiopterKalAngle", "CineDiopterKalEdgeWrap",
    "CineDiopterKalSourceZoom", "CineDiopterKalSourceOffsetX",
    "CineDiopterKalSourceOffsetY", "CineDiopterKalBlend",
    "CineDiopterKalDebugView", "CineDiopterKalMotionMode",
    "CineDiopterKalSpinMode", "CineDiopterKalProtectMode",
    "CineDiopterKalFreezeTime",
]

# Rows relevant for both tools whenever Enabled (table row 2).  Row 1,
# diopter_enabled, is UNCONDITIONAL and is special-cased by the combiner: it
# is the only control relevant with the master enable off (design doc 1.4
# Tier 0, pipeline.cpp:19037-19046).
TOOL_SHARED_ALWAYS = ["CineDiopterToolMode"]

# ---- diopter mask consumption per shape (framed placement only) ----------
# Source: ud_mask / ud_edgeProfile branch chain, ultimateDiopterGatherF.glsl
# :134-325 as tabulated in design doc 2.1 (mask columns only; Size is OR-
# consumed and handled by the combiner, table row 8).  On-Lens replaces this
# entire branch (glsl:200 `if (diopter_glass2.y > 0.5)`); Full Frame pins
# m = 1.0 (glsl:234).
_EDGE = "CineDiopterWobbleAmt"     # wobY/wobA term, glsl:256-259, shapes 1-11
MASK_SHAPE = [
    # 0 Full Frame -- no mask parameters at all
    [],
    # 1 Split: angle, stretch, feather, wobble, split curvature (glsl mask
    # table row 1; feather present, hollow/arc/round absent)
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     _EDGE, "CineDiopterSplitCurvature"],
    # 2 Ramp: no feather (design doc 2.1 row 2)
    ["CineDiopterAngleDeg", "CineDiopterStretch", _EDGE],
    # 3 Bar: + feather + hollow
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", _EDGE],
    # 4 Circle: + arc/broken
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     _EDGE],
    # 5 Squircle: + exponent
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     _EDGE, "CineDiopterSquirclePow"],
    # 6 Polygon: + corner rounding + sides
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     "CineDiopterCornerRound", _EDGE, "CineDiopterPolySides"],
    # 7 Star: + corner rounding + points + inner
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     "CineDiopterCornerRound", _EDGE, "CineDiopterStarPoints",
     "CineDiopterStarInner"],
    # 8 Heart: no rounding
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     _EDGE],
    # 9 Flower: + rounding + petals
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     "CineDiopterCornerRound", _EDGE, "CineDiopterPetalCount",
     "CineDiopterPetalDepth"],
    # 10 Blob: + rounding + seed/irregularity
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     "CineDiopterCornerRound", _EDGE, "CineDiopterBlobSeed",
     "CineDiopterBlobAmt"],
    # 11 Crescent: no rounding + bite/shift
    ["CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
     "CineDiopterHollow", "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
     _EDGE, "CineDiopterCrescentBite", "CineDiopterCrescentShift"],
]

# Mask controls surviving under On-Lens ("full with angular crops",
# ultimateDiopterGatherF.glsl:200-232; design doc 2.1: only Angle, Stretch,
# Feather, Arc Length, Broken survive; Size/Wobble/Rounding/Hollow-as-mask
# and the shape-specific sliders go dark; table rows 9, 17, 18, 21, 22 C2).
ONLENS_MASK = [
    "CineDiopterAngleDeg", "CineDiopterStretch", "CineDiopterFeather",
    "CineDiopterArcLengthDeg", "CineDiopterBrokenCount",
]

# Shape / Contents combos are preset-owned header controls consumed only
# under framed placement (On-Lens replaces the shape branch entirely and
# forces content = 0 at pipeline.cpp:13977-13982; table rows 15, 16).
FRAMED_HEADER = ["CineDiopterShape", "CineDiopterContent"]

# Refraction branch (ultimateDiopterGatherF.glsl:336
# `if (profile <= 0 || radius < 1e-4) return;`): the six profile dependents,
# table rows 53-58.
REFRACTION = [
    "CineDiopterIOR", "CineDiopterThickness", "CineDiopterRimWidth",
    "CineDiopterRimWarp", "CineDiopterRimCaustic", "CineDiopterRimDarken",
]

# Halo-warp branch (ultimateDiopterGatherF.glsl:390
# `if (diopter_halo2.w < 0.5) return uv;`, armed by warp_active at
# pipeline.cpp:14350-14352): the four non-arming dependents, table rows 64,
# 66, 69, 70.  Hollow's annulus-inner path (glsl:399) is combined in code.
WARP_PARAMS = [
    "CineDiopterRingCount", "CineDiopterRingPhase", "CineDiopterLobeCount",
    "CineDiopterLobePhaseDeg",
]

# Ghost branch (ultimateDiopterF.glsl:107 `if (ghostCount > 0 && m > 0.001)`;
# the m term is per-pixel, not UI state): seven dependents, table rows 72-78.
GHOSTS = [
    "CineDiopterGhostSpacing", "CineDiopterTangentSmear",
    "CineDiopterRadialSmear", "CineDiopterGhostThreshold",
    "CineDiopterGhostKnee", "CineDiopterGhostGain", "CineDiopterDispersion",
]

# Faceted Fold pattern branch (pattern_mode 1..3; table rows 88, 89.
# Source Zoom is itself a warp-arming control and stays ALWAYS, row 90).
FACETED = ["CineDiopterPatternSegments", "CineDiopterPatternFeedDeg"]

# Seam double-image branch (ultimateDiopterF.glsl:90
# `if (diopter_comp.x > 1e-4)`; table row 92).
SEAM = ["CineDiopterSeamGhostAmt"]

# Aberration group (pipeline.cpp:14253-14256, all strength_d * character
# products; table rows 60, 62, 63; row 61 Axial additionally needs
# Content = Diopter because axial_ca_m is zeroed at :14265).
ABERRATION = [
    "CineDiopterCAScale", "CineDiopterFieldCurveScale",
    "CineDiopterEdgeVignetteScale",
]
ABERRATION_AXIAL = "CineDiopterAxialCAScale"

# Aperture branch sets (ud_apReach, ultimateDiopterGatherF.glsl:457
# `if (shape <= 0) return 1.0;`; membership per design doc 2.1 "Aperture
# Shape (5) x Bokeh" -- Rotation applies to {1,2,3} (Heart subtracts rot,
# glsl:479-486), Anamorphic has its own angle uniform; table rows 80-85).
APERTURE = [
    [],                                                        # 0 Round
    ["CineDiopterBlades", "CineDiopterBladeRotDeg",
     "CineDiopterBladeCurve"],                                 # 1 Polygon
    ["CineDiopterBlades", "CineDiopterBladeRotDeg",
     "CineDiopterBladeCurve", "CineDiopterApertureInner"],     # 2 Star
    ["CineDiopterBladeRotDeg", "CineDiopterBladeCurve"],       # 3 Heart
    ["CineDiopterAnamorph", "CineDiopterAnamorphAngleDeg"],    # 4 Anamorphic
]

# Diopter motion branch sets (pipeline.cpp `motion == N` chain; table rows
# 96-112).  Speed rides the shared timeline tm for every non-Static mode
# EXCEPT Stutter (table row 96 rev 6.1, Motion{1,2,3,4,6,7,8,9,10,11}):
# Stutter deliberately clocks itself from t_now * StutterRate
# (pipeline.cpp:14128-14131) and never reads tm -- Codex-adjudicated, the
# rev-6 table row was wrong, not the renderer.
_SPD = "CineDiopterMotionSpeed"
MOTION = [
    [],                                                        # 0 Static
    [_SPD, "CineDiopterMotionAngleDeg", "CineDiopterSweepRange",
     "CineDiopterSweepPingPong"],                              # 1 Sweep
    [_SPD, "CineDiopterPulseTarget"],                          # 2 Pulse (+amt)
    [_SPD],                                                    # 3 Wave (+amp)
    [_SPD, "CineDiopterPathFreqX", "CineDiopterPathFreqY",
     "CineDiopterPathPhase", "CineDiopterPathAmp"],            # 4 Path
    ["CineDiopterStutterRate", "CineDiopterStutterPos",
     "CineDiopterStutterAngleDeg", "CineDiopterStutterSize",
     "CineDiopterStutterSmooth"],                              # 5 Stutter (+focus;
                                                               #   own clock, no Speed)
    [_SPD, "CineDiopterMotionAngleDeg", "CineDiopterPathAmp"], # 6 Orbit
    [_SPD, "CineDiopterPathAmp"],                              # 7 Wander
    [_SPD, "CineDiopterPathAmp"],                              # 8 Handheld
    [_SPD, "CineDiopterSweepRange"],                           # 9 Pendulum
    [_SPD],                                                    # 10 Heartbeat (+amt)
    [_SPD, "CineDiopterSweepRange", "CineDiopterPathAmp"],     # 11 Strobe Jump
]
# Compound-guarded motion rows combined in code (design doc 2.1):
#   PulseAmt   : Motion 2 and (Target=Size or (Target=Focus and Content=Diopter))
#                or Motion 10                       (rows 101; pipeline:14259/14261)
#   WaveAmp    : Motion 3 and SHAPE_HAS_EDGE        (row 102; glsl:234-259)
#   StutterFocus: Motion 5 and Content=Diopter      (row 111; pipeline:14092)

# Handheld group (pipeline.cpp:14154 `if (handheld > 0.f)`; rows 114-116;
# row 117 Focus Breath additionally guarded `if (content != 1)` at :14165).
HANDHELD = [
    "CineDiopterHandheldSpeed", "CineDiopterHandheldGait",
    "CineDiopterHandheldRotDeg",
]
HANDHELD_FOCUS = "CineDiopterHandheldFocus"

# Spin envelope branches (pipeline.cpp:14172-14198; rows 119-123).
SPIN = [
    ["CineDiopterSpinSpeed"],                                  # 0 Constant
    ["CineDiopterSpinTravelDeg", "CineDiopterSpinDurationS",
     "CineDiopterSpinDelayS"],                                 # 1 Ease
    ["CineDiopterSpinTravelDeg", "CineDiopterSpinDurationS",
     "CineDiopterSpinDelayS"],                                 # 2 Loop
    ["CineDiopterSpinTravelDeg", "CineDiopterSpinDurationS",
     "CineDiopterSpinBounce", "CineDiopterSpinDelayS"],        # 3 Flick
]

# Focus rows resolved in code (design doc 2.1 "Focus mode pairs"):
#   Power       : LensFocusMode == 0                (row 40)
#   LensFocusM  : LensFocusMode == 1                (row 41)
#   BaseFocusM  : resolver provenance == MANUAL     (row 38; pipeline:14203-14232)
#   FloorMaxBlur: FIELD_CURVE_ARMED or Content=SharpWindow
#                                                   (row 48; glsl:592-595)
#   SpotBlur    : Content == SharpWindow            (row 49; glsl:593-594)
#   MagnifyScale: Profile=Off and Content=Diopter and LENS_POWERED
#                                                   (row 93; pipeline:14251-14252,:14264)
#   MagnifyTrim : Profile=Off and Content=Diopter   (row 94; additive at :14251)
#   Character   : LENS_POWERED                      (row 59; :14253-14256)
#   FreezeAt    : Freeze == on                      (row 13)

# ---- kaleido per-mode pattern-branch sets --------------------------------
# Source: kmode branch chain ultimateKaleidoF.glsl:273-673 as tabulated in
# design doc 2.2 / table rows 126-151.  Stated as per-CONTROL mode sets and
# inverted into per-mode lists at emit time (the branch chain is dispatched
# per mode; the sets below are the audited column form of the same data).
KAL_MODE_SETS = {
    "CineDiopterKalSegments":    [0,1,2,3,4,5,6,7,8,10,13,15,16,18,19,20,22,23],
    "CineDiopterKalTwist":       [0,1,6,7,9,11,13,14,17,21,23],
    "CineDiopterKalRingCount":   [6,9,22],
    "CineDiopterKalStarSharp":   [7],
    "CineDiopterKalShapeBias":   [16,17,19,23],
    # feed = SourceAngle + SourceSpin folded on the CPU
    # (pipeline.cpp:15013 feed_rad); modes 2 and 8 never read feed
    # (Codex 1.7 + rev-2 self-audit; table rows 135/139)
    "CineDiopterKalSourceAngle": [m for m in range(24) if m not in (2, 8)],
    "CineDiopterKalSourceSpin":  [m for m in range(24) if m not in (2, 8)],
    "CineDiopterKalFXBand":      [9,10,12,13,14,15,17,18,21,23],
    "CineDiopterKalFXAmount":    [11,12,14,15,17,20,21,22],
    # direct-ft consumers only (mask 0xFFFA00u, Codex D2); the Tier-C
    # cellScale path is combined in code with CELL_BREATHE_ARMED
    "CineDiopterKalFXFlow":      [9] + list(range(11, 24)),
    "CineDiopterKalFXFreq":      [12,19],
    "CineDiopterKalSeamSoften":  [0],           # glsl:694, Radial Mirror only
    "CineDiopterKalCellSizeVar": [3,4,5,8,10,20,22],
    "CineDiopterKalCellBreathe": [3,4,5,8,10,20,22],
    "CineDiopterKalCellSubdiv":  [4,8],
    "CineDiopterKalCellMerge":   [20],
    "CineDiopterKalCellTint":    [2,3,4,5,8,10,15,20,22,23],
}
# FX Flow's Tier-C path: in these modes ft reaches output only through
# kal_cellScale's ftime term, a no-op unless Cell Breathe > 0
# (ultimateKaleidoF.glsl:110-118; Codex 1.8; table row 142 C2).
KAL_FLOW_CELL_MODES = [3, 4, 5, 8, 10]
# Wave Amplitude / Wave Frequency (Kal Anim rows 159/160): consumed only when
# Motion = Wave AND the pattern mode evaluates the wave terms (design doc 2.2
# Wv column; excluded modes 2,3,4,5,8,19,20).
KAL_WAVE_MODES = [m for m in range(24) if m not in (2, 3, 4, 5, 8, 19, 20)]

# Kaleido motion branch sets (alKaleidoResolveMotion switch,
# pipeline.cpp:14704-14800; rows 153-164.  Mode 5 = Track consumes no
# sliders -- it follows alKaleidoFocusUV).
_KSPD = "CineDiopterKalSpeed"
KAL_MOTION = [
    [],                                                        # 0 Static
    [_KSPD, "CineDiopterKalMotionAngle", "CineDiopterKalSweepRange",
     "CineDiopterKalPingPong"],                                # 1 Sweep
    [_KSPD, "CineDiopterKalPulseTarget",
     "CineDiopterKalPulseAmt"],                                # 2 Pulse (+angle @Offset)
    [_KSPD],                                                   # 3 Wave (+amp/freq)
    [_KSPD, "CineDiopterKalPathFreqX", "CineDiopterKalPathFreqY",
     "CineDiopterKalPathPhase", "CineDiopterKalPathAmp"],      # 4 Path
    [],                                                        # 5 Track
    [_KSPD, "CineDiopterKalMotionAngle",
     "CineDiopterKalPathAmp"],                                 # 6 Orbit
    [_KSPD, "CineDiopterKalPathAmp"],                          # 7 Wander
    [_KSPD, "CineDiopterKalPathAmp"],                          # 8 Handheld
    [_KSPD, "CineDiopterKalSweepRange"],                       # 9 Pendulum
    [_KSPD, "CineDiopterKalPulseAmt"],                         # 10 Heartbeat
    [_KSPD, "CineDiopterKalSweepRange",
     "CineDiopterKalPathAmp"],                                 # 11 Strobe Jump
]
# Pulse target Offset (== 2) additionally consumes the direction angle
# (pipeline.cpp:14738-14741 dir_x/dir_y in the else branch; row 154 C2).
KAL_PULSE_OFFSET_EXTRA = ["CineDiopterKalMotionAngle"]

# Kaleido spin branches (alKaleidoSpinDeg; rows 166-170; same envelope
# structure as the diopter's).
KAL_SPIN = [
    ["CineDiopterKalSpinSpeed"],                               # 0 Constant
    ["CineDiopterKalSpinTravel", "CineDiopterKalSpinDuration",
     "CineDiopterKalSpinDelay"],                               # 1 Ease
    ["CineDiopterKalSpinTravel", "CineDiopterKalSpinDuration",
     "CineDiopterKalSpinDelay"],                               # 2 Loop
    ["CineDiopterKalSpinTravel", "CineDiopterKalSpinDuration",
     "CineDiopterKalSpinBounce", "CineDiopterKalSpinDelay"],   # 3 Flick
]

# Protect branches (kal_holdMask, ultimateKaleidoF.glsl:710 `if (pmode > 0)`,
# :145-153; rows 172-179).  ProtectCenterX/Y additionally need
# Anchor == Fixed (== 1; pipeline.cpp:14997-15008; rows 175/176).
KAL_PROTECT = [
    [],                                                        # 0 Off
    ["CineDiopterKalProtectRadius", "CineDiopterKalProtectFeather",
     "CineDiopterKalProtectAnchor"],                           # 1 Disc
    ["CineDiopterKalDepthCut", "CineDiopterKalDepthFeatherM",
     "CineDiopterKalDepthInvert"],                             # 2 Depth
    ["CineDiopterKalProtectRadius", "CineDiopterKalProtectFeather",
     "CineDiopterKalProtectAnchor", "CineDiopterKalDepthCut",
     "CineDiopterKalDepthFeatherM", "CineDiopterKalDepthInvert"],  # 3 Disc x Depth
]
KAL_ANCHOR_FIXED_PAIR = ["CineDiopterKalProtectCenterX",
                         "CineDiopterKalProtectCenterY"]
KAL_FREEZE_DEPENDENT = ["CineDiopterKalFreezeAt"]
DIOPTER_FREEZE_DEPENDENT = ["CineDiopterFreezeAt"]

# =========================================================================
# Targeted tree extraction / drift checks.  No deviation waivers: the model
# must match the tree exactly, or the run fails.
# =========================================================================


def _read(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def _fail(msg):
    sys.stderr.write("aldiopterrelevance_oracle: FAIL: %s\n" % msg)
    sys.exit(1)


def check_shader_gates():
    """Assert every shader gate line the model encodes still exists."""
    gates = [
        (GATHER_GLSL, "if (diopter_glass2.y > 0.5)"),      # On-Lens, :200
        (GATHER_GLSL, "else if (shape == 0)"),             # Full Frame, :234
        (GATHER_GLSL, "float wobY = diopter_shape4.x"),    # edge wobble, :256
        (GATHER_GLSL, "if (profile <= 0 || radius < 1e-4) return;"),  # :336
        (GATHER_GLSL, "if (diopter_halo2.w < 0.5) return uv;"),       # :390
        (GATHER_GLSL, "if (shape <= 0) return 1.0;"),      # aperture, :457
        (GATHER_GLSL, "floorCoC = max(floorCoC, diopter_focus3.y"),   # :595
        (DIOPTER_GLSL, "if (diopter_comp.x > 1e-4)"),      # seam, :90
        (DIOPTER_GLSL, "if (ghostCount > 0 && m > 0.001)"),  # ghosts, :107
        (KALEIDO_GLSL, "kal_cell.y * 0.35 * sin(ftime"),   # cell breathe, :117
        (KALEIDO_GLSL, "if (kal_look.x > 1e-4 && kmode == 0)"),  # seam soften, :694
        (KALEIDO_GLSL, "if (pmode > 0)"),                  # protect, :710
    ]
    for path, needle in gates:
        if needle not in _read(path):
            _fail("shader gate %r no longer found in %s -- the encoded "
                  "model is stale; re-derive it" % (needle, path))


def extract_cached_controls(pipeline_src):
    """local name -> setting name, from LLCachedControl declarations."""
    out = {}
    for m in re.finditer(
            r'LLCachedControl<\w+>\s+(\w+)\(\s*gSavedSettings,\s*"(\w+)"',
            pipeline_src):
        out[m.group(1)] = m.group(2)
    return out


def _branch_bodies(chain_src, opener_re):
    """Split an if/else-if chain into {index: body} by brace matching."""
    bodies = {}
    for m in re.finditer(opener_re, chain_src):
        idx = int(m.group(1))
        i = chain_src.find("{", m.end())
        if i < 0:
            continue
        depth, j = 0, i
        while j < len(chain_src):
            if chain_src[j] == "{":
                depth += 1
            elif chain_src[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        bodies[idx] = chain_src[i:j + 1]
    return bodies


def check_diopter_motion(pipeline_src):
    """Extract per-branch setting consumption from the `motion == N` chain
    and diff against MOTION."""
    start = pipeline_src.find("if (motion == 1)             // Sweep")
    if start < 0:
        start = re.search(r"if \(motion == 1\)", pipeline_src)
        start = start.start() if start else -1
    if start < 0:
        _fail("diopter motion chain not found in pipeline.cpp")
    end = pipeline_src.find("// handheld life", start)
    chain = pipeline_src[start:end]
    cached = extract_cached_controls(pipeline_src)
    bodies = _branch_bodies(chain, r"if \(motion == (\d+)\)")
    # [F9] Completeness before per-branch comparison: a DELETED mode branch
    # would otherwise simply not be iterated and could never mismatch.  The
    # chain must dispatch exactly modes 1..11 (0 = Static is the implicit
    # fall-through with no branch).
    if set(bodies.keys()) != set(range(1, 12)):
        _fail("diopter motion chain dispatches modes %s; expected 1..11 -- "
              "a branch was added or deleted, re-derive the model"
              % sorted(bodies.keys()))
    for mode, body in bodies.items():
        got = set()
        for ident in re.findall(r"\b([a-z]\w*)\b", body):
            if ident in cached and cached[ident].startswith("CineDiopter"):
                got.add(cached[ident])
        if re.search(r"\btm\b", body):
            got.add("CineDiopterMotionSpeed")   # tm = t_now * m_speed
        if re.search(r"\bdir_[xy]\b|\bma\b", body):
            # ma / dir_x / dir_y alias m_angle, hoisted above the chain
            got.add("CineDiopterMotionAngleDeg")
        want = set(MOTION[mode])
        # Rows the branch consumes but the model carries separately under
        # compound guards (PulseAmt rows 101 C1-C3, StutterFocus row 111;
        # design doc 2.1 "Compound guards inside the motion branches").
        compound_handled = {
            2: {"CineDiopterPulseAmt"},
            5: {"CineDiopterStutterFocus"},
            10: {"CineDiopterPulseAmt"},
        }.get(mode, set())
        for s in want - got:
            _fail("motion %d: model lists %s but the branch does not "
                  "consume it" % (mode, s))
        extra = got - want - compound_handled
        if extra:
            _fail("motion %d: branch consumes %s missing from the model"
                  % (mode, sorted(extra)))


def check_kal_motion(pipeline_src):
    """Extract per-case ALKaleidoLook field consumption from
    alKaleidoResolveMotion and diff against KAL_MOTION."""
    start = pipeline_src.find("static void alKaleidoResolveMotion")
    if start < 0:
        _fail("alKaleidoResolveMotion not found")
    end = pipeline_src.find("\n}", pipeline_src.find("switch (look.mMotion)", start))
    src = pipeline_src[start:end]
    # look.mField -> setting; the resolver copies k_* cached controls into
    # the look 1:1, so the field name maps by concatenation.
    field_to_setting = {
        "mSpeed": "CineDiopterKalSpeed",
        "mMotionAngleDeg": "CineDiopterKalMotionAngle",
        "mSweepRange": "CineDiopterKalSweepRange",
        "mPingPong": "CineDiopterKalPingPong",
        "mPulseTarget": "CineDiopterKalPulseTarget",
        "mPulseAmt": "CineDiopterKalPulseAmt",
        "mPathFreqX": "CineDiopterKalPathFreqX",
        "mPathFreqY": "CineDiopterKalPathFreqY",
        "mPathPhase": "CineDiopterKalPathPhase",
        "mPathAmp": "CineDiopterKalPathAmp",
    }
    cases = {}
    for m in re.finditer(r"case (\d+):", src):
        idx = int(m.group(1))
        nxt = re.search(r"case \d+:|default:", src[m.end():])
        cases[idx] = src[m.end(): m.end() + (nxt.start() if nxt else len(src))]
    # [F9] Completeness before per-branch comparison: the switch must carry
    # exactly cases 1..11 (0 = Static is `default: break`).  A deleted case
    # must fail here, not silently drop out of the loop below.
    if set(cases.keys()) != set(range(1, 12)):
        _fail("alKaleidoResolveMotion dispatches cases %s; expected 1..11 -- "
              "a case was added or deleted, re-derive the model"
              % sorted(cases.keys()))
    for mode, body in cases.items():
        got = set()
        for field, setting in field_to_setting.items():
            if "look.%s" % field in body:
                got.add(setting)
        if re.search(r"\btm\b", body):
            got.add("CineDiopterKalSpeed")      # tm = t_now * look.mSpeed
        # dir_x/dir_y come from look.mMotionAngleDeg via ma
        if re.search(r"\bdir_[xy]\b|\bma\b", body):
            got.add("CineDiopterKalMotionAngle")
        want = set(KAL_MOTION[mode])
        if mode == 2:
            # the Offset-target angle use is modeled separately
            want = want | set(KAL_PULSE_OFFSET_EXTRA)
        if got != want:
            _fail("kal motion %d: extracted %s vs model %s"
                  % (mode, sorted(got), sorted(want)))


def check_warp_expression(pipeline_src, relevance_h_src):
    """[F4] Wave 2 swapped the renderer to the shared helper (call-swap 1),
    so single-sourcing is now enforced in two halves: (a) pipeline.cpp must
    derive warp_active THROUGH alDiopterWarpArmed -- never through a
    re-inlined expression -- and (b) the helper body in aldiopterrelevance.h
    must keep the exact five-term expression PRED_WARP_ARMED models."""
    call = (r"bool warp_active = alDiopterWarpArmed\(pattern_mode, ring_fold,"
            r" twist_deg,\s*lobe_amt, pattern_zoom\);")
    if not re.search(call, pipeline_src):
        _fail("pipeline.cpp no longer derives warp_active via "
              "alDiopterWarpArmed(pattern_mode, ring_fold, twist_deg, "
              "lobe_amt, pattern_zoom) -- restore the shared-helper call "
              "(call-swap 1) or re-derive this oracle")
    body = (r"return \(pattern_mode > 0\) \|\| \(ring_fold > 1e-3f\) \|\|\s*"
            r"\(fabsf\(twist_deg\) > 1e-2f\) \|\| \(lobe_amt > 1e-3f\) \|\|\s*"
            r"\(fabsf\(pattern_zoom - 1\.f\) > 1e-3f\);")
    if not re.search(body, relevance_h_src):
        _fail("alDiopterWarpArmed's body in aldiopterrelevance.h no longer "
              "matches the modeled five-term arming expression -- update "
              "PRED_WARP_ARMED's model and this oracle together")


def check_strength_expression(pipeline_src, relevance_h_src):
    """[F4] Same two-half contract for strength_d (call-swap 3)."""
    call = r"F32 strength_d = alDiopterStrengthD\(lens_focus_m, base_focus_m\);"
    if not re.search(call, pipeline_src):
        _fail("pipeline.cpp no longer derives strength_d via "
              "alDiopterStrengthD(lens_focus_m, base_focus_m) -- restore the "
              "shared-helper call (call-swap 3) or re-derive this oracle")
    body = (r"return llclamp\(fabsf\(1\.f / lens_focus_m - "
            r"1\.f / base_focus_m\), 0\.f, 10\.f\);")
    if not re.search(body, relevance_h_src):
        _fail("alDiopterStrengthD's body in aldiopterrelevance.h no longer "
              "matches the modeled expression -- update PRED_LENS_POWERED's "
              "model and this oracle together")


def control_bindings():
    """control name -> setting name from the floater XML (the DERIVED
    Setting column of the relevance table)."""
    src = _read(FLOATER_XML)
    out = {}
    for m in re.finditer(
            r"<(?:slider|combo_box|check_box)\b[^>]*?>", src, re.S):
        tag = m.group(0)
        name = re.search(r'\bname="([^"]+)"', tag)
        ctl = re.search(r'\bcontrol_name="([^"]+)"', tag)
        if name and ctl and _is_modeled_setting(ctl.group(1)):
            out[name.group(1)] = ctl.group(1)
    if len(out) != 181:
        _fail("expected 181 bound relevance-model controls in the floater "
              "XML, found %d" % len(out))
    return out


def _is_modeled_setting(setting):
    """The relevance model's scope is exactly the 181 CineDiopter* RENDERER
    inputs.  Floater chrome sits outside it by namespace: CineDiopterUI*
    (e.g. CineDiopterUIActiveOnly, the section 5.3 display toggle, and
    CineDiopterUIRectVersion) is UI state, and the focus-QoL checkboxes bind
    non-CineDiopter Render* settings (RenderFocusPointCrosshair /
    RenderFocusPointLocked)."""
    return (setting.startswith("CineDiopter") and
            not setting.startswith("CineDiopterUI"))


# =========================================================================
# LLSD emission
# =========================================================================

def _esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


class LLSDWriter(object):
    def __init__(self):
        self.lines = []
        self.depth = 0

    def _w(self, text):
        self.lines.append("  " * self.depth + text)

    def key(self, k):
        self._w("<key>%s</key>" % _esc(k))

    def integer(self, v):
        self._w("<integer>%d</integer>" % v)

    def open(self, tag):
        self._w("<%s>" % tag)
        self.depth += 1

    def close(self, tag):
        self.depth -= 1
        self._w("</%s>" % tag)

    def string_array(self, values):
        if not values:
            self._w("<array />")
            return
        self.open("array")
        for v in values:
            self._w("<string>%s</string>" % _esc(v))
        self.close("array")

    def int_array(self, values):
        if not values:
            self._w("<array />")
            return
        self.open("array")
        for v in values:
            self.integer(v)
        self.close("array")

    def array_of_string_arrays(self, arrays):
        self.open("array")
        for a in arrays:
            self.string_array(a)
        self.close("array")

    def text(self):
        return "\n".join(self.lines) + "\n"


def emit(binding):
    to_ctrl = {}
    for ctrl, setting in binding.items():
        if setting in to_ctrl:
            _fail("setting %s bound twice in the floater XML" % setting)
        to_ctrl[setting] = ctrl

    def C(names):
        out = []
        for n in names:
            if n not in to_ctrl:
                _fail("setting %s has no bound control in the floater XML" % n)
            out.append(to_ctrl[n])
        return out

    kal_mode_lists = []
    for mode in range(24):
        row = [s for s, modes in KAL_MODE_SETS.items()
               if mode in modes and s != "CineDiopterKalFXFlow"]
        # FX Flow's direct path is a per-mode fact too; its cellScale path is
        # gated on CELL_BREATHE_ARMED and stays out of the mode list.
        if mode in KAL_MODE_SETS["CineDiopterKalFXFlow"]:
            row.append("CineDiopterKalFXFlow")
        kal_mode_lists.append(C(sorted(row)))

    w = LLSDWriter()
    w._w('<?xml version="1.0" encoding="UTF-8"?>')
    w.open("llsd")
    w.open("map")
    w.key("version"); w.integer(1)
    w.key("master_enable"); w.string_array(C(["CineDiopterEnabled"]))
    w.key("tool_shared_always"); w.string_array(C(TOOL_SHARED_ALWAYS))

    w.key("diopter"); w.open("map")
    w.key("always"); w.string_array(C(DIOPTER_ALWAYS))
    w.key("freeze_dependent"); w.string_array(C(DIOPTER_FREEZE_DEPENDENT))
    w.key("framed_header"); w.string_array(C(FRAMED_HEADER))
    w.key("mask_shape"); w.array_of_string_arrays([C(x) for x in MASK_SHAPE])
    w.key("onlens_mask"); w.string_array(C(ONLENS_MASK))
    w.key("size"); w.string_array(C(["CineDiopterSize"]))
    w.key("hollow"); w.string_array(C(["CineDiopterHollow"]))
    w.key("wobble_freq"); w.string_array(C(["CineDiopterWobbleFreq"]))
    w.key("refraction"); w.string_array(C(REFRACTION))
    w.key("warp_params"); w.string_array(C(WARP_PARAMS))
    w.key("ghosts"); w.string_array(C(GHOSTS))
    w.key("faceted"); w.string_array(C(FACETED))
    w.key("seam"); w.string_array(C(SEAM))
    w.key("aberration"); w.string_array(C(ABERRATION))
    w.key("aberration_axial"); w.string_array(C([ABERRATION_AXIAL]))
    w.key("character"); w.string_array(C(["CineDiopterCharacter"]))
    w.key("power"); w.string_array(C(["CineDiopterPower"]))
    w.key("lens_m"); w.string_array(C(["CineDiopterLensFocusM"]))
    w.key("base_m"); w.string_array(C(["CineDiopterBaseFocusM"]))
    w.key("floor_max"); w.string_array(C(["CineDiopterFloorMaxBlurPx"]))
    w.key("spot_blur"); w.string_array(C(["CineDiopterSpotBlur"]))
    w.key("mag_scale"); w.string_array(C(["CineDiopterMagnifyScale"]))
    w.key("mag_trim"); w.string_array(C(["CineDiopterMagnifyTrim"]))
    w.key("motion"); w.array_of_string_arrays([C(x) for x in MOTION])
    w.key("pulse_amt"); w.string_array(C(["CineDiopterPulseAmt"]))
    w.key("wave_amp"); w.string_array(C(["CineDiopterWaveAmp"]))
    w.key("stutter_focus"); w.string_array(C(["CineDiopterStutterFocus"]))
    w.key("handheld"); w.string_array(C(HANDHELD))
    w.key("handheld_focus"); w.string_array(C([HANDHELD_FOCUS]))
    w.key("spin"); w.array_of_string_arrays([C(x) for x in SPIN])
    w.key("aperture"); w.array_of_string_arrays([C(x) for x in APERTURE])
    w.close("map")

    w.key("kaleido"); w.open("map")
    w.key("always"); w.string_array(C(KALEIDO_ALWAYS))
    w.key("freeze_dependent"); w.string_array(C(KAL_FREEZE_DEPENDENT))
    w.key("mode"); w.array_of_string_arrays(kal_mode_lists)
    w.key("flow_cell_modes"); w.int_array(KAL_FLOW_CELL_MODES)
    w.key("fx_flow"); w.string_array(C(["CineDiopterKalFXFlow"]))
    w.key("wave_modes"); w.int_array(KAL_WAVE_MODES)
    w.key("wave"); w.string_array(C(["CineDiopterKalWaveAmp",
                                     "CineDiopterKalWaveFreq"]))
    w.key("motion"); w.array_of_string_arrays([C(x) for x in KAL_MOTION])
    w.key("pulse_offset_extra"); w.string_array(C(KAL_PULSE_OFFSET_EXTRA))
    w.key("spin"); w.array_of_string_arrays([C(x) for x in KAL_SPIN])
    w.key("protect"); w.array_of_string_arrays([C(x) for x in KAL_PROTECT])
    w.key("anchor_fixed"); w.string_array(C(KAL_ANCHOR_FIXED_PAIR))
    w.close("map")

    w.close("map")
    w.close("llsd")
    return w.text()


def main():
    pipeline_src = _read(PIPELINE)
    relevance_h_src = _read(RELEVANCE_H)
    check_shader_gates()
    check_warp_expression(pipeline_src, relevance_h_src)
    check_strength_expression(pipeline_src, relevance_h_src)
    check_diopter_motion(pipeline_src)
    check_kal_motion(pipeline_src)
    binding = control_bindings()
    sys.stdout.write(emit(binding))
    return 0


if __name__ == "__main__":
    sys.exit(main())
