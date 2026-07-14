/**
 * SL_GBufferProvider.fx
 *
 * Feeds the SL viewer's REAL G-buffer (via the sl_reshade_bridge add-on) into
 * MartysMods iMMERSE, replacing Launchpad's depth-derived normals with true
 * gbuffer normals. This is the bridge from "debug view works" to "RTGI/MXAO use
 * real SL normals."
 *
 * ── INSTALL ──────────────────────────────────────────────────────────────────
 *   Place this file in the iMMERSE shader folder, next to
 *   MartysMods_LAUNCHPAD.fx (so the ".\MartysMods\" includes resolve). Keep
 *   SL_Bridge.fxh (ships with the add-on) on a ReShade effect search path.
 *
 * ── EFFECT ORDER (ReShade list, top → bottom) ────────────────────────────────
 *   iMMERSE: Launchpad            (must stay enabled + first; sets up depth etc.)
 *   SL G-Buffer Provider          ← THIS: overrides normals AFTER Launchpad
 *   iMMERSE Pro: RTGI / MXAO ...  (read the normals we just wrote)
 *   If placed above Launchpad, Launchpad overwrites us and nothing changes.
 *
 * ── SAFETY ───────────────────────────────────────────────────────────────────
 *   If the add-on isn't feeding (texture unbound → zeros), every pixel discards
 *   and this effect is a pure no-op: RTGI simply falls back to Launchpad's
 *   normals. It can never make things worse than baseline iMMERSE.
 *
 * ── IP ───────────────────────────────────────────────────────────────────────
 *   Uses the shared Deferred:: interface by #including Marty's PUBLIC headers,
 *   exactly as RTGI does. Contains none of his shader source.
 */

// NOTE: do NOT include ReShade.fxh here. Modern ReShade.fxh defines
// BUFFER_PIXEL_SIZE / BUFFER_SCREEN_SIZE / BUFFER_ASPECT_RATIO as MACROS, while
// Marty's mmx_global.fxh declares them as `static const` — including ReShade.fxh
// first makes the preprocessor rewrite those declarations into
// `static const float2 float2(...)` (X3000 syntax errors). Launchpad's headers
// are self-sufficient; we mirror that and provide our own fullscreen VS below.
#include "MartysMods/mmx_global.fxh"
#include "MartysMods/mmx_math.fxh"
#include "MartysMods/mmx_deferred.fxh"
#include "SL_Bridge.fxh"

// Standard fullscreen-triangle VS (same body as ReShade's PostProcessVS).
void SL_FullscreenVS(in uint id : SV_VertexID,
                     out float4 pos : SV_Position, out float2 uv : TEXCOORD)
{
    uv.x = (id == 2) ? 2.0 : 0.0;
    uv.y = (id == 1) ? 2.0 : 0.0;
    pos  = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Provide real motion vectors from the viewer's velocity buffer
// (BDMergeVelocityBuffer / A5.4). Safe to leave on: the motion pass DISCARDS
// where the velocity delta is exactly zero, so if the viewer's velocity buffer
// is off (texture unbound → all zero) this is a pure no-op and Launchpad's
// estimated motion is preserved. Only enable BDMergeVelocityBuffer in the
// viewer to actually feed it.
#ifndef SL_PROVIDE_MOTION
#define SL_PROVIDE_MOTION 1
#endif

// GL eye-space (SL, right-handed, camera looks down -Z) → iMMERSE/D3D view-space
// (left-handed, camera looks down +Z) is a Z negate. This is the one piece that
// warrants in-world confirmation: if RTGI lighting/AO looks inverted (light from
// the wrong side, occlusion in the wrong places), set SL_NORMAL_FLIP_Z to 0.
#ifndef SL_NORMAL_FLIP_Z
#define SL_NORMAL_FLIP_Z 1
#endif

// Diagnostic: paint the screen with what iMMERSE will actually read back AFTER
// our write — ground truth for what RTGI consumes.
//   Normals: compare vs SL_BridgeDebug "Normals (decoded)" — same R/G with BLUE
//            inverted = correct (Z-flip); blocky depth-normals = wrong ORDER.
//   Motion:  hue = direction, brightness = speed. Pan the camera and compare to
//            the motion when this effect is OFF (Launchpad's own estimate) — they
//            should point the SAME way. Opposite hue => flip a Motion axis below.
uniform int SL_DEBUG_VIEW <
    ui_type = "combo";
    ui_items = "Off\0Normals iMMERSE receives\0Motion iMMERSE receives\0";
    ui_label = "DEBUG view";
> = 0;

// Motion conversion (live-tunable so you can dial signs without a reload).
// SL velocity = (cur_ndc - last_ndc), NDC space, forward. iMMERSE wants
// (prev_uv - cur_uv), UV space (it reprojects with uv + motion). Default maps
// that: flip X for direction, keep Y, x0.5 for NDC->UV. If RTGI ghosts/smears
// when panning, flip an axis; if it under/over-corrects, adjust scale.
uniform float SL_MOTION_SCALE <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.05;
    ui_label = "Motion scale"; ui_category = "Motion";
> = 1.0;
uniform bool SL_MOTION_FLIP_X < ui_label = "Motion flip X"; ui_category = "Motion"; > = true;
uniform bool SL_MOTION_FLIP_Y < ui_label = "Motion flip Y"; ui_category = "Motion"; > = false;

float3 sl_gl_to_view(float3 n_gl)
{
#if SL_NORMAL_FLIP_Z
    return float3(n_gl.x, n_gl.y, -n_gl.z);
#else
    return n_gl;
#endif
}

// POINT sampler for the read — matches iMMERSE's sNormalsTexV3. Linear across a
// geometry edge blends two ENCODED normals into a meaningless in-between value
// (edge shimmer); point sampling avoids it. 1:1 pass so this is also exact.
sampler SL_sNormalsPoint
{
    Texture = SLNormalsTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

// ── Normals ──────────────────────────────────────────────────────────────────
// RenderTarget is Deferred::NormalsTexV3 (RGBA16, DLSS-size): XY = gbuffer
// normal (octahedral), ZW = geometry normal (octahedral). iMMERSE's
// get_normals() returns -octahedral_dec(xy), so to hand it view-space normal N
// we store octahedral_enc(-N). We write ONLY XY (RenderTargetWriteMask=3 on the
// pass) — Launchpad's smooth ZW geometry normal is left intact so RTGI keeps its
// stable coarse structure; overwriting ZW with our high-frequency normal caused
// fine-detail shimmer.
void PS_ProvideNormals(in float4 vpos : SV_Position, in float2 uv : TEXCOORD,
                       out float4 o : SV_Target)
{
    float4 raw = tex2Dlod(SL_sNormalsPoint, float4(SL_UV(uv), 0, 0));

    // Dead/unbound bridge → texture reads all-zero → keep Launchpad's normals.
    if (raw.x == 0.0 && raw.y == 0.0)
        discard;

    float3 n_view = normalize(sl_gl_to_view(SL_DecodeNormal(raw.xy)));
    float2 enc = Math::octahedral_enc(-n_view);
    o = float4(enc, enc);   // only .xy is written (see RenderTargetWriteMask)
}

#if SL_PROVIDE_MOTION
sampler SL_sMotionPoint
{
    Texture = SLMotionTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

// Convert the SL velocity delta to iMMERSE's motion convention.
float2 sl_to_immerse_motion(float2 d_ndc)   // d_ndc = cur_ndc - last_ndc
{
    float2 mv = d_ndc * 0.5 * SL_MOTION_SCALE;   // NDC(-2..2) -> UV(-1..1)
    if (SL_MOTION_FLIP_X) mv.x = -mv.x;          // forward -> backward (prev-cur)
    if (SL_MOTION_FLIP_Y) mv.y = -mv.y;          // GL NDC +Y up -> UV +Y down
    return mv;
}

// RenderTarget is Deferred::MotionVectorsTex (RG16F, full-res). DISCARD where the
// SL delta is exactly zero: that both preserves Launchpad's motion on truly
// static pixels and makes the whole pass a no-op when the velocity buffer is off.
void PS_ProvideMotion(in float4 vpos : SV_Position, in float2 uv : TEXCOORD,
                      out float2 o : SV_Target)
{
    float2 d = tex2Dlod(SL_sMotionPoint, float4(SL_UV(uv), 0, 0)).xy;
    if (d.x == 0.0 && d.y == 0.0)
        discard;
    o = sl_to_immerse_motion(d);
}
#endif

// Diagnostic pass: reads back what we wrote (via iMMERSE's own accessors) and
// paints it to the backbuffer. Discards when Off (no-op).
float3 PS_ShowReceived(in float4 vpos : SV_Position, in float2 uv : TEXCOORD) : SV_Target
{
    float3 o = 0.0;
    if (SL_DEBUG_VIEW == 1)          // normals iMMERSE receives
    {
        o = Deferred::get_normals(uv) * 0.5 + 0.5;
    }
    else if (SL_DEBUG_VIEW == 2)     // motion iMMERSE receives (hue=dir, bright=speed)
    {
        float2 m = Deferred::get_motion(uv);
        float  ang = atan2(m.y, m.x);
        float3 rgb = saturate(3.0 * abs(2.0 * frac(ang / 6.2831853 + float3(0.0, -1.0/3.0, 1.0/3.0)) - 1.0) - 1.0);
        o = lerp(0.5, rgb, saturate(length(m) * 250.0));
    }
    else                             // Off → don't touch the backbuffer
    {
        discard;
    }
    return o;
}

technique SL_GBufferProvider <
    ui_tooltip = "Overrides iMMERSE normals with real SL G-buffer normals from "
                 "the sl_reshade_bridge add-on. Place directly BELOW Launchpad "
                 "and ABOVE RTGI/MXAO. No-op if the add-on isn't feeding.";
>
{
    pass ProvideNormals
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ProvideNormals;
        RenderTarget = Deferred::NormalsTexV3;
        RenderTargetWriteMask = 3;   // write .xy only; preserve Launchpad's .zw geometry normals
    }
#if SL_PROVIDE_MOTION
    pass ProvideMotion
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ProvideMotion;
        RenderTarget = Deferred::MotionVectorsTex;
    }
#endif
    // must be LAST so it reads the final NormalsTexV3; writes backbuffer only
    // when SL_DEBUG_VIEW is not Off.
    pass ShowReceived
    {
        VertexShader = SL_FullscreenVS;
        PixelShader  = PS_ShowReceived;
    }
}
