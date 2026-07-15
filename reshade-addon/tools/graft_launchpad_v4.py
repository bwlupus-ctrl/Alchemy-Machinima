# Graft the SL bridge onto the V4 MartysMods_LAUNCHPAD.fx.
# Every anchor is asserted; any mismatch aborts before writing.
import sys

PATH = r"I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\reshade-shaders\Shaders\iMMERSE\MartysMods_LAUNCHPAD.fx"

with open(PATH, "r", encoding="utf-8", newline="") as f:
    s = f.read()

# File is CRLF throughout; work in LF internally, restore CRLF on write.
assert "\r\n" in s
s = s.replace("\r\n", "\n")

orig = s

def replace_once(s, old, new, tag):
    n = s.count(old)
    assert n == 1, f"[{tag}] anchor found {n} times (expected 1)"
    return s.replace(old, new, 1)

# ── A1: bridge preprocessor block after LAUNCHPAD_DEBUG_OUTPUT ────────────────
i = s.find("#ifndef LAUNCHPAD_DEBUG_OUTPUT")
assert i >= 0, "A1 start"
j = s.find("#endif", i)
assert j >= 0, "A1 endif"
j = s.find("\n", j) + 1
A1 = """
/*=============================================================================
    SL BRIDGE (Second Life viewer G-buffer via the sl_reshade_bridge add-on)
    Grafted for personal use per reshade-addon/LAUNCHPAD_INTEGRATION.md.
    Set SL_ENABLE_BRIDGE to 0 to restore stock Launchpad Optical Flow.
=============================================================================*/

#ifndef SL_ENABLE_BRIDGE
 #define SL_ENABLE_BRIDGE           1
#endif

#if SL_ENABLE_BRIDGE
 // Provide real unlit albedo into Deferred::AlbedoTex for true colored GI bounce.
 #ifndef SL_PROVIDE_ALBEDO
  #define SL_PROVIDE_ALBEDO 1
 #endif
 // iMMERSE mixes albedo with LINEAR GI. If colored bounce looks too dark or
 // saturated the SL gbuffer albedo is sRGB-encoded -> set this to 1.
 #ifndef SL_ALBEDO_TO_LINEAR
  #define SL_ALBEDO_TO_LINEAR 0
 #endif
 // GL eye-space (right-handed, -Z fwd) -> D3D view-space (left-handed, +Z fwd).
 #ifndef SL_NORMAL_FLIP_Z
  #define SL_NORMAL_FLIP_Z 1
 #endif
#endif
"""
s = s[:j] + A1 + s[j:]

# ── A2: OPTICAL_FLOW_Q uniform only exists without the bridge ────────────────
s = replace_once(s,
    "uniform int OPTICAL_FLOW_Q <",
    "#if !SL_ENABLE_BRIDGE\nuniform int OPTICAL_FLOW_Q <",
    "A2a")
s = replace_once(s,
    'ui_category = "Motion Estimation / Optical Flow";\n> = 0;',
    'ui_category = "Motion Estimation / Optical Flow";\n> = 0;\n#endif //!SL_ENABLE_BRIDGE',
    "A2b")

# ── A3: SL uniforms before NORMALS_MODE ───────────────────────────────────────
A3 = """#if SL_ENABLE_BRIDGE
uniform int SL_DEBUG_VIEW <
    ui_type = "combo";
    ui_items = "Off\\0Normals iMMERSE receives\\0Motion iMMERSE receives\\0Albedo iMMERSE receives\\0";
    ui_label = "DEBUG view";
    ui_category = "Second Life Bridge";
> = 0;

// Motion conversion is DERIVED, not guessed. velocityF.glsl writes
// (cur_ndc - last_ndc) in GL NDC (x right, y UP), un-jittered. iMMERSE wants
// (prev_uv - cur_uv) in ReShade UV (y DOWN): prev-cur = -v, x0.5 NDC->UV, and
// the top-left flip negates y once more => motion = float2(-v.x, +v.y) * 0.5.
// The toggles default to exactly that; escape hatch only.
uniform float SL_MOTION_SCALE <
    ui_type = "slider"; ui_min = 0.0; ui_max = 2.0; ui_step = 0.05;
    ui_label = "Motion scale"; ui_category = "Second Life Bridge";
> = 1.0;
uniform bool SL_MOTION_FLIP_X < ui_label = "Motion flip X"; ui_category = "Second Life Bridge"; > = true;
uniform bool SL_MOTION_FLIP_Y < ui_label = "Motion flip Y"; ui_category = "Second Life Bridge"; > = false;

uniform int SL_ALBEDO_SPACE <
    ui_type = "combo";
    ui_items = "Match iMMERSE HDR space (recommended)\\0Raw linear\\0Raw sRGB-encoded\\0";
    ui_label = "Albedo space";
    ui_tooltip = "AlbedoTex is NOT plain colour: Launchpad's own writer stores\\n"
                 "sdr_to_hdr(display-space colour), and RTGI's bounce is calibrated\\n"
                 "against that. The bridge receives LINEAR albedo (hardware sRGB\\n"
                 "decode), so it must be re-encoded + curve-matched or coloured\\n"
                 "bounce oversaturates. Raw modes are for live A/B diagnosis only.";
    ui_category = "Second Life Bridge";
> = 0;

uniform float SL_ALBEDO_MAX <
    ui_type = "slider"; ui_min = 0.5; ui_max = 8.0; ui_step = 0.1;
    ui_label = "Albedo max brightness";
    ui_tooltip = "Luminance cap in multiples of the mid-grey (0.18) target.\\n"
                 "Launchpad's own albedo is log-equalized around that target, which\\n"
                 "implicitly BOUNDS the GI multibounce/temporal feedback gain. SL's\\n"
                 "real albedo is unbounded after the HDR curve (near-white hits ~25x)\\n"
                 "and over-drives the loop: bright/gold surfaces flood the scene and\\n"
                 "wind up over seconds. 2.0 is a good start; raise if bounce feels flat.";
    ui_category = "Second Life Bridge";
> = 2.0;
#endif

"""
s = replace_once(s, "uniform int NORMALS_MODE <", A3 + "uniform int NORMALS_MODE <", "A3")

# ── A4: include the bridge header ─────────────────────────────────────────────
s = replace_once(s,
    '#include ".\\MartysMods\\mmx_sfc.fxh"',
    '#include ".\\MartysMods\\mmx_sfc.fxh"\n\n#if SL_ENABLE_BRIDGE\n#include "SL_Bridge.fxh"\n#endif',
    "A4")

# ── A5: compile out ALL optical-flow textures under the bridge ───────────────
# NOTE: LinearDepthCurr + its sampler stay UNCONDITIONAL (declared above this
# point) — external effects alias LinearDepthCurr by name (e.g. VirtualCinemaV2's
# IMMERSE_LAUNCHPAD LiDAR-depth path), so it must exist on the bridge path too.
# The compile-out therefore starts at LinearDepthPrevLo, not at LinearDepthCurr.
s = replace_once(s,
    "texture LinearDepthPrevLo      { Width = BUFFER_WIDTH>>3;",
    "#if !SL_ENABLE_BRIDGE //bridge mode: no optical flow -> reclaim ALL its textures\n"
    "texture LinearDepthPrevLo      { Width = BUFFER_WIDTH>>3;",
    "A5a")
s = replace_once(s,
    "sampler sFlowFeaturesPrevL7  { Texture = FlowFeaturesPrevL7; AddressU = MIRROR; AddressV = MIRROR; };",
    "sampler sFlowFeaturesPrevL7  { Texture = FlowFeaturesPrevL7; AddressU = MIRROR; AddressV = MIRROR; };\n#endif //!SL_ENABLE_BRIDGE (OF textures)",
    "A5b")

# ── A6: compile out the optical-flow shader code ──────────────────────────────
s = replace_once(s,
    "void WriteCurrFeatureAndDepthPS(in VSOUT i, out float o0 : SV_Target0, out float o1 : SV_Target1)",
    "#if !SL_ENABLE_BRIDGE //bridge mode: no optical flow -> compile out its shaders\nvoid WriteCurrFeatureAndDepthPS(in VSOUT i, out float o0 : SV_Target0, out float o1 : SV_Target1)",
    "A6a")
s = replace_once(s,
    "void UpscaleFilter2to1PS(in VSOUT i, out float4 o : SV_Target0){o = filter_flow_final(i, sMotionTexUpscale2, 0, 1, 2);}",
    "void UpscaleFilter2to1PS(in VSOUT i, out float4 o : SV_Target0){o = filter_flow_final(i, sMotionTexUpscale2, 0, 1, 2);}\n#endif //!SL_ENABLE_BRIDGE (OF shaders)",
    "A6b")

# ── A7: bridge providers, inserted before the Normals entry points ───────────
A7 = """/*=============================================================================
    SL Bridge providers (V4 targets)
=============================================================================*/
#if SL_ENABLE_BRIDGE

// POINT samplers: linear filtering across a geometry edge blends two ENCODED
// values into a meaningless in-between (edge shimmer). 1:1 pass, so exact.
sampler SL_sNormalsPoint
{
    Texture = SLNormalsTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

sampler SL_sMotionPoint
{
    Texture = SLMotionTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

#if SL_PROVIDE_ALBEDO
sampler SL_sAlbedoPoint
{
    Texture = SLAlbedoTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};
#endif

float3 sl_gl_to_view(float3 n_gl)
{
#if SL_NORMAL_FLIP_Z
    return float3(n_gl.x, n_gl.y, -n_gl.z);
#else
    return n_gl;
#endif
}

// V4: shading normals live alone in NormalsTexV4 (RG16 octahedral). We override
// ONLY that — the SL gbuffer normal already includes normal maps — and leave
// GeoNormalsTexV4 to Launchpad's smoothed geometry normal so RTGI keeps its
// stable coarse structure (same rationale as the V3 build's writemask=3).
// get_normals() returns -octahedral_dec(xy) => store octahedral_enc(-N).
void PS_ProvideNormals(in VSOUT i, out float2 o : SV_Target)
{
    float4 raw = tex2Dlod(SL_sNormalsPoint, float4(SL_UV(i.uv), 0, 0));
    if (raw.x == 0.0 && raw.y == 0.0) discard; // dead bridge -> keep Launchpad's
    float3 n_view = normalize(sl_gl_to_view(SL_DecodeNormal(raw.xy)));
    o = Math::octahedral_enc(-n_view);
}

float2 sl_to_immerse_motion(float2 d_ndc) // d_ndc = cur_ndc - last_ndc (GL, y up)
{
    float2 mv = d_ndc * 0.5 * SL_MOTION_SCALE; // NDC(-2..2) -> UV(-1..1)
    if (SL_MOTION_FLIP_X) mv.x = -mv.x;        // forward -> backward (prev-cur)
    if (SL_MOTION_FLIP_Y) mv.y = -mv.y;        // (defaults: derived, see UI block)
    return mv;
}

void PS_ProvideMotion(in VSOUT i, out float2 o : SV_Target)
{
    // Sole writer of MotionVectorsTex in bridge mode -> must always write; a
    // discard here would freeze stale motion on static pixels (ghosting).
    float2 d = tex2Dlod(SL_sMotionPoint, float4(SL_UV(i.uv), 0, 0)).xy;
    o = sl_to_immerse_motion(d);
}

// Bridge mode has no optical flow, so nothing else writes LinearDepthCurr.
// External consumers (VirtualCinemaV2 LiDAR depth, etc.) alias it, so provide it
// here — matches the standard linearized depth their fallback uses.
void WriteCurrDepthPS(in VSOUT i, out float o : SV_Target0)
{
    o = Depth::get_linear_depth(i.uv);
}

#if SL_PROVIDE_ALBEDO
// Local copies of Launchpad's SDR->"unpacked HDR" curve (defined LATER in this
// file; FX requires declaration before use, hence the sl_ duplicates).
float3 sl_cone_overlap(float3 c)
{
    float k = 0.5 * 0.33;
    float2 f = float2(1 - 2 * k, k);
    float3x3 m = float3x3(f.xyy, f.yxy, f.yyx);
    return mul(c, m);
}

float3 sl_sdr_to_hdr(float3 color)
{
    color = saturate(color);
    color = sl_cone_overlap(color);
    color = color*0.283799*((2.52405+color)*color);
    color = color * rcp(1.04 - saturate(color));
    return color;
}

// V4: AlbedoTex.a carries linear Z (textured normals read it) -> the pass uses
// RenderTargetWriteMask=7 so we only replace RGB. Discard on black keeps
// Launchpad's albedo for sky/unbound and makes a dead bridge a pure no-op.
//
// COLOUR SPACE: the addon binds SL_ALBEDO with an _srgb view, so `a` arrives
// LINEAR. AlbedoTex's native space is sdr_to_hdr(display colour) — Launchpad's
// own AlbedoMainPS writes exactly that from ColorInput. Feeding raw linear
// oversaturates every coloured-GI bounce (first seen 2026-07-14, the day the
// albedo slot first actually worked). Default: encode back to display space,
// then apply the same curve.
void PS_ProvideAlbedo(in VSOUT i, out float3 o : SV_Target)
{
    float3 a = tex2Dlod(SL_sAlbedoPoint, float4(SL_UV(i.uv), 0, 0)).rgb;
    if (a.r == 0.0 && a.g == 0.0 && a.b == 0.0) discard;

    [branch]
    if (SL_ALBEDO_SPACE == 0)      // match iMMERSE: linear -> display -> their curve
    {
        a = pow(a, 1.0 / 2.2);
        a = sl_sdr_to_hdr(a);
        // Bound the magnitude the way Launchpad's log-equalizer implicitly does.
        // Without this, near-white/gold albedo (~25x after the curve) over-drives
        // the GI multibounce + temporal accumulation -> runaway gold wash that
        // rebuilds a few seconds after every reload.
        float target_l = dot(sl_sdr_to_hdr(0.18.xxx), 0.3333);
        float l = dot(a, 0.3333);
        a *= min(1.0, (SL_ALBEDO_MAX * target_l) / max(l, 1e-6));
    }
    else if (SL_ALBEDO_SPACE == 2) // raw sRGB-encoded (diagnosis)
    {
        a = pow(a, 1.0 / 2.2);
    }
    // SL_ALBEDO_SPACE == 1: raw linear (previous behaviour, diagnosis)
    o = a;
}
#endif

// Reads back what iMMERSE will actually consume, via its own accessors.
float3 PS_ShowReceived(in VSOUT i) : SV_Target
{
    float3 o = 0.0;
    if (SL_DEBUG_VIEW == 1)
    {
        o = Deferred::get_normals(i.uv) * 0.5 + 0.5;
    }
    else if (SL_DEBUG_VIEW == 2)
    {
        float2 m = Deferred::get_motion(i.uv);
        float  ang = atan2(m.y, m.x);
        float3 rgb = saturate(3.0 * abs(2.0 * frac(ang / 6.2831853 + float3(0.0, -1.0/3.0, 1.0/3.0)) - 1.0) - 1.0);
        o = lerp(0.5, rgb, saturate(length(m) * 250.0));
    }
    else if (SL_DEBUG_VIEW == 3)
    {
        o = Deferred::get_albedo(i.uv);
    }
    else
    {
        discard;
    }
    return o;
}

#endif //SL_ENABLE_BRIDGE

"""
anchor = "/*=============================================================================\n\tShader Entry Points - Normals"
s = replace_once(s, anchor, A7 + anchor, "A7")

# ── A8: technique — swap the OF pass block for the bridge motion pass ─────────
s = replace_once(s,
    "\t//OF\n\tpass {VertexShader = OpticalFlowVS;PixelShader = WriteCurrFeatureAndDepthPS;",
    "#if !SL_ENABLE_BRIDGE\n\t//OF\n\tpass {VertexShader = OpticalFlowVS;PixelShader = WriteCurrFeatureAndDepthPS;",
    "A8a")
s = replace_once(s,
    "\tpass {VertexShader = OpticalFlowVS;PixelShader = WritePrevDepthMipPS;RenderTarget0 = LinearDepthPrevLo;}\n",
    "\tpass {VertexShader = OpticalFlowVS;PixelShader = WritePrevDepthMipPS;RenderTarget0 = LinearDepthPrevLo;}\n"
    "#else\n"
    "\t// SL bridge: viewer velocity replaces ALL optical-flow passes. Gated by\n"
    "\t// OpticalFlowVS so it only runs when a downstream effect requested motion.\n"
    "\tpass SLDepth  {VertexShader = MainVS;PixelShader = WriteCurrDepthPS;RenderTarget = LinearDepthCurr;} //provide LinearDepthCurr for external consumers (VirtualCinemaV2 LiDAR depth etc.)\n"
    "\tpass SLMotion {VertexShader = MainVS;PixelShader = PS_ProvideMotion;RenderTarget = Deferred::MotionVectorsTex;} //ALWAYS run: sole writer of MotionVectorsTex; IPC-gating freezes it (ghost) when downstream effects don't request OPTICALFLOW\n"
    "#endif //SL_ENABLE_BRIDGE (technique OF block)\n",
    "A8b")

# ── A9: albedo overlay after the pyramid result ───────────────────────────────
s = replace_once(s,
    "    pass UpscaleAlbedoPyramid{VertexShader = AlbedoVS; PixelShader = AlbedoMainPS; RenderTarget = Deferred::AlbedoTex;}\n",
    "    pass UpscaleAlbedoPyramid{VertexShader = AlbedoVS; PixelShader = AlbedoMainPS; RenderTarget = Deferred::AlbedoTex;}\n"
    "#if SL_ENABLE_BRIDGE && SL_PROVIDE_ALBEDO\n"
    "    pass SLAlbedo {VertexShader = MainVS; PixelShader = PS_ProvideAlbedo; RenderTarget = Deferred::AlbedoTex; RenderTargetWriteMask = 7;} //ALWAYS run (ungated); keep .a = linear Z\n"
    "#endif\n",
    "A9")

# ── A10: normals overlay after CopyNormals ────────────────────────────────────
i = s.find("PixelShader = CopyNormalsPS; RenderTarget = Deferred::NormalsTexV4; }")
assert i >= 0, "A10 anchor"
j = s.find("\n", i) + 1
A10 = ("#if SL_ENABLE_BRIDGE\n"
       "\tpass SLNormals {VertexShader = MainVS; PixelShader = PS_ProvideNormals; RenderTarget = Deferred::NormalsTexV4;} //ALWAYS run (ungated)\n"
       "#endif\n")
s = s[:j] + A10 + s[j:]

# ── A11: mark the technique label so the UI shows which build loaded ──────────
s = replace_once(s,
    'ui_label = "iMMERSE: Launchpad (enable and move to the top!)";',
    'ui_label = "iMMERSE: Launchpad [SL BRIDGE] (enable and move to the top!)";',
    "A11")

# ── A12: debug technique at EOF ───────────────────────────────────────────────
A12 = """
#if SL_ENABLE_BRIDGE
technique SL_Bridge_Debug
<
    ui_label = "SL Bridge: Debug View (move to BOTTOM of list)";
    ui_tooltip = "Shows what iMMERSE received from the SL bridge after all writes.\\n"
                 "Enable, drag to the very bottom, pick a buffer in Launchpad's DEBUG view.";
>
{
    pass { VertexShader = MainVS; PixelShader = PS_ShowReceived; }
}
#endif
"""
s = s.rstrip() + "\n" + A12

assert s != orig
with open(PATH, "w", encoding="utf-8", newline="") as f:
    f.write(s.replace("\n", "\r\n"))

print("OK — all anchors matched.")
print("lines:", s.count("\n") + 1)
# sanity: preprocessor balance
opens  = sum(s.count(t) for t in ("#if ", "#ifdef", "#ifndef"))
closes = s.count("#endif")
print("#if-family:", opens, " #endif:", closes)
