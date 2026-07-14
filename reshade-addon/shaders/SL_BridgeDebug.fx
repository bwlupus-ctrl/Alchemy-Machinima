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
 *     Uniform heartbeat  -> bottom-left bar sweeps every ~2s if sl_frame_counter
 *                           is being pushed; camera pos values shift as you move.
 */

#include "ReShade.fxh"
#include "SL_Bridge.fxh"   // shared semantics, uniforms, orientation, decode

uniform int DebugMode <
    ui_type = "combo";
    ui_items = "Normals (decoded)\0Normals (raw)\0Albedo\0ORM\0HDR color\0Motion\0";
    ui_label = "Buffer";
> = 0;

float3 PS_Debug(float4 vpos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    // All fetches below are orientation-corrected inside SL_Bridge.fxh.
    float3 c;
    if      (DebugMode == 0) c = SL_NormalView(uv) * 0.5 + 0.5;
    else if (DebugMode == 1) c = SL_NormalRaw(uv).xyz;
    else if (DebugMode == 2) c = SL_Albedo(uv);
    else if (DebugMode == 3) c = SL_ORM(uv);
    else if (DebugMode == 4) c = SL_ColorHDR(uv);            // pre-tonemap; may exceed 1
    else                     c = float3(abs(SL_Motion(uv)) * 20.0, 0.0);

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

technique SL_BridgeDebug < ui_tooltip = "Visualize SL G-buffer bridge data. Flat grey/black everywhere = bridge not feeding."; >
{
    pass
    {
        VertexShader = PostProcessVS;
        PixelShader  = PS_Debug;
    }
}
