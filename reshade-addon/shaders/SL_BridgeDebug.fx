/**
 * SL_BridgeDebug.fx — validation shader for the sl_reshade_bridge add-on.
 *
 * Install into the ReShade effects folder alongside ReShade.fxh. Enable the
 * "SL_BridgeDebug" technique and cycle DebugMode:
 *
 *   Expected results with a working bridge:
 *     Normals (decoded)  -> smooth RGB direction colors that ROTATE WITH THE
 *                           CAMERA (view-space). Flat grey = bridge dead.
 *     Albedo             -> unlit scene colors.
 *     ORM                -> green-ish roughness/metal patterns.
 *     HDR color          -> the scene, brighter than final (pre-tonemap).
 *     Motion             -> black until the viewer ships a velocity buffer.
 *     Validity / tail    -> ABI 1.1 tail state (see below). This is gate L1's
 *                           instrument: resize / minimize / toggle a source off
 *                           and watch the corresponding cell go RED while the
 *                           image goes neutral — if a buffer keeps showing
 *                           stale pixels while its cell is red, L1 FAILS.
 *     Visible diffuse    -> ABI 1.2 sidecar RGB: unlit linear diffuse of the
 *                           VISIBLE surface (RenderVisibleDiffuseSidecar on).
 *                           Metals read BLACK here (correct: no diffuse), sky
 *                           reads black too — Exactness K tells them apart.
 *                           Magenta/black diagonal stripes = semantic INVALID
 *                           (setting off, 1.1 viewer, or bridge dead) — a
 *                           DIFFERENT state from valid-but-black data.
 *     Exactness K        -> the sidecar's per-pixel exactness channel:
 *                           GREEN = K=1 fully known (metals MUST read green
 *                           with black RGB in the view above — K=0 on a metal
 *                           is a seed-classification bug), deep BLUE = K=0
 *                           unwritten/sky/background, ORANGE→YELLOW ramp =
 *                           0<K<1 (attenuated through semi-transparent forward
 *                           layers). Stripes = semantic INVALID, as above.
 *     Uniform heartbeat  -> bottom-left bar sweeps every ~2s if sl_frame_counter
 *                           is being pushed; camera pos values shift as you move.
 *
 *   Validity / tail layout (rows top to bottom):
 *     Row 1 (tall): 10 cells = SL_SEM_VALID_* in bit order (COLOR_HDR, DEPTH,
 *                   ALBEDO, ORM, NORMALS, EMISSIVE, MOTION, MOTION_META,
 *                   SURFACE_COVERAGE, VISIBLE_DIFFUSE). Green = valid, dark
 *                   red = invalid.
 *                   ALL grey = no v1.1 tail (1.0 viewer or bridge dead).
 *     Row 2: 12 cells = SL_RESET_* flags; white = raised this frame.
 *     Row 3: 10 cells = SL_MotionCoverage bits (cyan); then a config strip
 *            showing SL_ConfigFlags bits (yellow).
 */

#include "ReShade.fxh"
#include "SL_Bridge.fxh"   // shared semantics, uniforms, orientation, decode

uniform int DebugMode <
    ui_type = "combo";
    ui_items = "Normals (decoded)\0Normals (raw)\0Albedo\0ORM\0HDR color\0Motion\0Validity / tail\0Visible diffuse\0Exactness K\0";
    ui_label = "Buffer";
> = 0;

float3 sl_bit_cell(uint mask, int bit, float3 on_color, float3 off_color)
{
    return ((mask & (1u << uint(bit))) != 0u) ? on_color : off_color;
}

// Unmistakable "semantic INVALID" pattern: magenta/black diagonal stripes.
// Deliberately nothing the scene could produce — the whole point is that
// INVALID (setting off / 1.1 viewer / bridge dead) must never be confusable
// with VALID-BUT-ZERO data (black metal diffuse, K=0 sky), which renders as
// its actual value against a plain background. Conflating those two states is
// the exact bug class the explicit-validity contract exists to prevent.
float3 sl_invalid_pattern(float2 uv)
{
    float s = frac((uv.x + uv.y) * 24.0);
    return (s < 0.5) ? float3(0.85, 0.0, 0.85) : float3(0.06, 0.0, 0.06);
}

// Exactness-K heat map. Three regimes, deliberately non-adjacent colors so a
// screenshot answers "which regime" at a glance:
//   K = 0        deep blue    unwritten / sky / background ("no answer here")
//   0 < K < 1    orange→yellow  partially exact (accumulated through
//                              semi-transparent forward layers)
//   K = 1        green        fully known (metals belong HERE, black-diffuse)
// 8-bit unorm makes the ==0/==1 compares exact (0/255, 255/255).
float3 sl_k_heat(float k)
{
    if (k <= 0.0) return float3(0.05, 0.10, 0.45);
    if (k >= 1.0) return float3(0.10, 0.85, 0.25);
    return lerp(float3(0.90, 0.25, 0.05), float3(0.95, 0.90, 0.15), k);
}

// Full-screen ABI 1.1 tail visualization (DebugMode 6).
float3 PS_TailStatus(float2 uv)
{
    // background: dark, tinted faintly blue when a tail is present
    float3 c = (SL_TailValid != 0u) ? float3(0.03, 0.04, 0.08) : float3(0.06, 0.06, 0.06);

    // Row 1: semantic validity — 10 cells across the top third
    if (uv.y > 0.05 && uv.y < 0.30 && uv.x > 0.05 && uv.x < 0.95)
    {
        int cell = int((uv.x - 0.05) / 0.09);
        if (cell < 10)
        {
            c = (SL_TailValid == 0u)
                ? float3(0.25, 0.25, 0.25)                       // no tail: grey
                : sl_bit_cell(SL_SemanticValid, cell,
                              float3(0.10, 0.85, 0.25),          // valid
                              float3(0.45, 0.05, 0.05));         // invalid
        }
    }
    // Row 2: reset flags — 12 cells
    else if (uv.y > 0.35 && uv.y < 0.50 && uv.x > 0.05 && uv.x < 0.95)
    {
        int cell = int((uv.x - 0.05) / 0.075);
        if (cell < 12)
        {
            c = sl_bit_cell(SL_ResetFlags, cell,
                            float3(1.0, 1.0, 1.0), float3(0.10, 0.10, 0.14));
        }
    }
    // Row 3a: motion coverage — 10 cells
    else if (uv.y > 0.55 && uv.y < 0.68 && uv.x > 0.05 && uv.x < 0.95)
    {
        int cell = int((uv.x - 0.05) / 0.09);
        if (cell < 10)
        {
            c = sl_bit_cell(SL_MotionCoverage, cell,
                            float3(0.1, 0.8, 0.9), float3(0.08, 0.10, 0.12));
        }
    }
    // Row 3b: config flags — 5 cells (VELOCITY, HDR, SNAPSHOT, 10BIT, OWNED)
    else if (uv.y > 0.73 && uv.y < 0.86 && uv.x > 0.05 && uv.x < 0.55)
    {
        int cell = int((uv.x - 0.05) / 0.10);
        if (cell < 5)
        {
            c = sl_bit_cell(SL_ConfigFlags, cell,
                            float3(0.95, 0.85, 0.15), float3(0.12, 0.11, 0.06));
        }
    }
    // Bottom edge: motion-encoding id as 1-4 tick marks
    else if (uv.y > 0.90 && uv.y < 0.95 && uv.x > 0.05 && uv.x < 0.45)
    {
        int cell = int((uv.x - 0.05) / 0.10);
        c = (uint(cell) < SL_MotionEncoding) ? float3(0.8, 0.5, 1.0) : float3(0.1, 0.08, 0.12);
    }
    return c;
}

float3 PS_Debug(float4 vpos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    // All fetches below are orientation-corrected inside SL_Bridge.fxh.
    float3 c;
    if      (DebugMode == 0) c = SL_NormalView(uv) * 0.5 + 0.5;
    else if (DebugMode == 1) c = SL_NormalRaw(uv).xyz;
    else if (DebugMode == 2) c = SL_Albedo(uv);
    else if (DebugMode == 3) c = SL_ORM(uv);
    else if (DebugMode == 4) c = SL_ColorHDR(uv);            // pre-tonemap; may exceed 1
    // Motion: SIGNED, matching the viewer's own velocityDebugF.glsl exactly
    // (clamp(v*gain,-1,1)*0.5+0.5, blue 0.5) so the two views are directly
    // comparable. It previously used abs(), which made left/right and up/down
    // indistinguishable -- and a SIGN error is the most likely motion defect and
    // the one gate M1 exists to catch, so a direction-blind instrument could
    // never have found it. Gain 32 matches BDMergeMotionBlurStrength's default.
    //
    // EXPECTED DIFFERENCE vs the viewer view: the X channel should agree, but Y
    // may read INVERTED, because SL_Motion() already negates Y to compensate for
    // the GL/ReShade V-flip (SL_Bridge.fxh). That inversion is the flip handling
    // working, not a fault -- if Y matches the viewer exactly, the flip
    // compensation is MISSING.
    else if (DebugMode == 5) c = float3(clamp(SL_Motion(uv) * 32.0, -1.0, 1.0) * 0.5 + 0.5, 0.5);
    else if (DebugMode == 6) c = PS_TailStatus(uv);
    // Visible diffuse (ABI 1.2 sidecar): gate on the EXPLICIT bit first —
    // stripes mean INVALID (a different state from valid-but-black; see
    // sl_invalid_pattern). RGB shown raw: it arrives linear via the _srgb
    // view, same convention as the Albedo view above, so the two are directly
    // comparable. Metals correctly read black HERE and green in the K view.
    else if (DebugMode == 7)
        c = SL_SemValid(SL_SEM_VALID_VISIBLE_DIFFUSE)
              ? SL_VisibleDiffuse(uv).rgb
              : sl_invalid_pattern(uv);
    else if (DebugMode == 8)
        c = SL_SemValid(SL_SEM_VALID_VISIBLE_DIFFUSE)
              ? sl_k_heat(SL_VisibleDiffuse(uv).a)
              : sl_invalid_pattern(uv);
    else                     c = PS_TailStatus(uv);

    // --- uniform heartbeat HUD (bottom-left) -------------------------------
    // bar: sweeps with frame counter => uniforms alive
    if (uv.y > 0.97 && uv.x < 0.25)
    {
        float sweep = frac(SLFrame / 120.0);
        c = (uv.x / 0.25 < sweep) ? float3(0.1, 1.0, 0.2) : float3(0.1, 0.1, 0.1);
    }
    // three thin rows above it encode camera pos fractional part: should
    // visibly flicker/shift when the camera moves
    else if (uv.y > 0.94 && uv.x < 0.25)
    {
        int row = int((uv.y - 0.94) / 0.01);
        float v = frac(abs(row == 0 ? SLCamPos.x : row == 1 ? SLCamPos.y : SLCamPos.z));
        c = (uv.x / 0.25 < v) ? float3(0.2, 0.6, 1.0) : float3(0.05, 0.05, 0.1);
    }

    return c;
}

technique SL_BridgeDebug < ui_tooltip = "Visualize SL G-buffer bridge data. Flat grey/black everywhere = bridge not feeding.\n'Validity / tail' shows the ABI 1.1 semantic-valid bits, reset flags, motion coverage and config -- the L1 fail-closed instrument."; >
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader  = PS_Debug;
    }
}
