/*=============================================================================

    SL_GBufferProvider.fx  -  [RTGI Step B]

    Publishes the Second Life viewer's REAL engine motion vectors into the shared
    iMMERSE motion texture, so every effect that reads Deferred::get_motion()
    (RTGI, motion blur, TAA, ...) uses engine-true motion instead of the Launchpad's
    reconstructed optical flow. The viewer's in-process ReShade add-on
    (llreshadebridge) binds the A5.4 velocity buffer to the SL_MOTION_NDC semantic;
    this effect transcodes it (NDC-delta -> UV-delta) and writes the shared
    Deferred::MotionVectorsTex.

    USAGE: enable AFTER "MartysMods Launchpad" and BEFORE any motion consumer
    (RTGI etc.) in the ReShade technique order. Requires the viewer add-on active
    (log: "[RTGI Step B] bound viewer velocity ...") and BDMergeVelocityBuffer ON
    in the viewer so the velocity buffer is actually produced.

    -------------------------------------------------------------------------
    IP NOTE: This file is 100% original. It only (a) READS a texture the viewer
    add-on binds to a custom semantic, and (b) WRITES a texture whose namespace,
    name, size and format it declares IDENTICALLY so ReShade shares the same GPU
    resource across effects (the documented cross-effect texture-sharing behavior).
    No iMMERSE / MartysMods shader code is included, copied, derived, or modified.
    A texture name is not the proprietary work; Marty's producer passes are his.
=============================================================================*/

#include "ReShade.fxh"

//------------------------------------------------------------------ controls --

uniform float SL_MOTION_SCALE <
    ui_label   = "SL Motion: Scale";
    ui_type    = "slider"; ui_min = 0.0; ui_max = 4.0; ui_step = 0.01;
    ui_tooltip = "Gain on the engine motion vectors. 1.0 = physical.";
> = 1.0;

uniform int SL_MOTION_SIGN <
    ui_label   = "SL Motion: Direction";
    ui_type    = "combo";
    ui_items   = "current -> previous (reproject)\0previous -> current (forward)\0";
    ui_tooltip = "iMMERSE reprojects with current->previous. SL stores previous->current,\n"
                 "so the default negates it. If ghosting gets WORSE not better, flip this.";
> = 0;

uniform bool SL_MOTION_DEBUG <
    ui_label   = "SL Motion: Debug tint (writes to backbuffer)";
    ui_tooltip = "Visualise the published motion (R=+x, G=+y) to confirm it is real\n"
                 "engine motion and not the Launchpad's estimate.";
> = false;

//----------------------------------------------------------------- textures ---

// The viewer add-on binds the real velocity buffer (RG16F, NDC-space delta
// cur_ndc - last_ndc, Y up) to this custom semantic.
texture  SLMotionNDCTex : SL_MOTION_NDC;
sampler  sSLMotionNDC { Texture = SLMotionNDCTex; };

// iMMERSE Launchpad's shared motion texture. SAME namespace + name + size +
// format => ReShade binds this effect to the very same resource, so overwriting it
// (after the Launchpad has run) feeds every downstream Deferred::get_motion() reader.
namespace Deferred
{
    texture MotionVectorsTex { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RG16F; };
}

//------------------------------------------------------------------ shaders ---

float2 transcode(float2 uv)
{
    // SL: NDC-space delta (cur_ndc - last_ndc), range ~[-2,2], Y up.
    float2 ndc = tex2D(sSLMotionNDC, uv).xy;

    // NDC delta -> UV delta: the +0.5 remap offset cancels in a delta (x0.5).
    // Flip Y: GL NDC is Y-up, ReShade texture/UV space is Y-down.
    float2 mv = float2(ndc.x, -ndc.y) * 0.5;

    // SL delta is previous->current; iMMERSE reprojects current->previous.
    if (SL_MOTION_SIGN == 0) mv = -mv;

    return mv * SL_MOTION_SCALE;
}

float2 PS_PublishMotion(float4 vpos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    return transcode(uv);
}

float3 PS_DebugTint(float4 vpos : SV_Position, float2 uv : TEXCOORD) : SV_Target
{
    if (!SL_MOTION_DEBUG)
        discard;
    float2 mv = transcode(uv);
    // Encode motion around grey: right/up = warm, left/down = cool. Scaled up so
    // small per-frame motion is visible.
    return float3(0.5 + mv * 40.0, 0.5);
}

//---------------------------------------------------------------- technique ---

technique SL_GBufferProvider <
    ui_label   = "SL G-Buffer Provider (real motion -> iMMERSE)";
    ui_tooltip = "Order AFTER MartysMods Launchpad and BEFORE motion consumers (RTGI).\n"
                 "Overwrites the shared motion vectors with the SL viewer's engine-true\n"
                 "motion. Needs the viewer add-on + BDMergeVelocityBuffer enabled.";
>
{
    pass PublishMotion
    {
        VertexShader = PostProcessVS;
        PixelShader  = PS_PublishMotion;
        RenderTarget = Deferred::MotionVectorsTex;
    }
    pass DebugTint
    {
        VertexShader = PostProcessVS;
        PixelShader  = PS_DebugTint;
        // writes to the backbuffer (default RT) only when SL_MOTION_DEBUG is on
    }
}
