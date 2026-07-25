/**
 * SL_Bridge.fxh — shared convention header for the sl_reshade_bridge add-on.
 *
 * Include this from EVERY ReShade effect that consumes the SL viewer's
 * G-buffer (the debug shader, the iMMERSE provider FX, anything future). It is
 * the single source of truth for:
 *   - the SL_* texture semantics the add-on binds,
 *   - the sl_* camera uniforms the add-on pushes,
 *   - buffer ORIENTATION (the add-on is a verbatim GL pipe; alignment to
 *     ReShade's top-left convention happens HERE, once, for all consumers),
 *   - the viewer's normal encoding.
 *
 * The add-on deliberately does NOT flip: copy_texture_region is a straight
 * blit and can't mirror, and orientation is a sampling convention, not a
 * property of the pixels. Defining it here guarantees the debug view, the
 * provider FX, and iMMERSE all agree — which is what keeps normals aligned
 * with generic_depth's DEPTH.
 */

#ifndef SL_BRIDGE_FXH
#define SL_BRIDGE_FXH

// The SL viewer renders with OpenGL (texture origin bottom-left); ReShade
// samples top-left (D3D convention), so every bridge texture arrives flipped
// in V — the same reason generic_depth exposes RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN.
// Default ON. NOTE (contract v2 section 7.1): do NOT assume this must equal
// RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN — the live config has them UNEQUAL (0 vs 1)
// and the backend copy paths for generic depth and our copied slots may differ.
// Alignment with DEPTH is proven at runtime (gate O1), not by matching defines.
#ifndef SL_INPUT_IS_UPSIDE_DOWN
#define SL_INPUT_IS_UPSIDE_DOWN 1
#endif

// --- textures bound by the add-on via update_texture_bindings(semantic) ------
// When a semantic is INVALID the add-on binds a 4x4 all-zero neutral texture
// (fail closed); when the add-on is absent entirely, ReShade leaves these at
// their zero-initialized default. Either way: sample zero, and — more
// importantly — gate on SL_SemValid() below, never on payload values
// (zero motion / black albedo / (0,0) octahedral are all LEGAL DATA).
texture SLNormalsTex  : SL_NORMALS;      // OCTAHEDRAL .xy, env .z, gbuffer-flags .w
texture SLMotionTex   : SL_MOTION_NDC;   // RG16F screen-space NDC delta (0 if off)
texture SLAlbedoTex   : SL_ALBEDO;
texture SLOrmTex      : SL_ORM;          // occlusion / roughness / metallic
texture SLColorHdrTex : SL_COLOR_HDR;    // pre-tonemap scene color
// v1.1 tail textures (RG8 masks; published by ABI >= 1.1 viewers only)
texture SLMotionMetaTex      : SL_MOTION_META;       // R: per-px motion validity, G: reactivity
texture SLSurfaceCoverageTex : SL_SURFACE_COVERAGE;  // R: G-buffer coverage, G: forward coverage
// v1.2 tail texture (ABI >= 1.2 viewers only, RenderVisibleDiffuseSidecar on).
// RGB: visible-surface diffuse reflectance -- arrives LINEAR (the add-on binds
// the GL_SRGB8_ALPHA8 source through an _srgb view, so hardware decodes on
// sample; do NOT decode again). A: exactness K in [0,1] -- 1 = fully known,
// 0 = unwritten/sky/background; attenuated through semi-transparent forward
// layers. K = 1 with black RGB is a metal: LEGAL DATA, not "missing".
texture SLVisibleDiffuseTex  : SL_VISIBLE_DIFFUSE;   // RGB: linear diffuse, A: exactness K

sampler SL_sNormals  { Texture = SLNormalsTex;  };
sampler SL_sMotion   { Texture = SLMotionTex;   };
sampler SL_sAlbedo   { Texture = SLAlbedoTex;   };
sampler SL_sOrm      { Texture = SLOrmTex;      };
sampler SL_sColorHdr { Texture = SLColorHdrTex; };
// masks: POINT — interpolating a validity/coverage mask across a boundary
// manufactures values that are neither valid nor invalid.
sampler SL_sMotionMeta
{
    Texture = SLMotionMetaTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};
sampler SL_sSurfaceCoverage
{
    Texture = SLSurfaceCoverageTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};
// POINT for the same reason as the masks: A carries exactness K, and
// interpolating an exactness value across a coverage boundary manufactures
// values that are neither valid nor invalid.
sampler SL_sVisibleDiffuse
{
    Texture = SLVisibleDiffuseTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};

// --- camera uniforms pushed by the add-on (matched by "source" annotation) ---
uniform float4x4 SLView   < source = "sl_view_matrix"; >;   // row-major, v' = v*M
uniform float4x4 SLProj   < source = "sl_proj_matrix"; >;
uniform float3   SLCamPos  < source = "sl_camera_pos"; >;
uniform float3   SLCamAt   < source = "sl_camera_at"; >;
uniform float3   SLCamLeft < source = "sl_camera_left"; >;
uniform float3   SLCamUp   < source = "sl_camera_up"; >;
uniform float2   SLNearFar < source = "sl_near_far"; >;     // (near, far) meters
uniform float    SLFovY    < source = "sl_fov_y"; >;        // radians, vertical
uniform float    SLAspect  < source = "sl_aspect"; >;
uniform float    SLFrame   < source = "sl_frame_counter"; >;

// --- ABI 1.1 tail status (uint, pushed by the add-on each frame) -------------
// Validity is EXPLICIT, never inferred (contract v2 section 4). With an old
// add-on (or none) these sources are never pushed and read 0 -> every
// SL_SemValid() test fails -> consumers fail closed automatically.
uniform uint SL_SemanticValid  < source = "sl_semantic_valid"; >;    // SL_SEM_VALID_*
uniform uint SL_ResetFlags     < source = "sl_reset_flags"; >;       // SL_RESET_*
uniform uint SL_MotionEncoding < source = "sl_motion_encoding"; >;   // SL_MOTION_ENC_*
uniform uint SL_OrientFlags    < source = "sl_orientation_flags"; >;
uniform uint SL_MotionCoverage < source = "sl_motion_coverage"; >;
uniform uint SL_ConfigFlags    < source = "sl_config_flags"; >;      // SL_CONFIG_*
uniform uint SL_TailValid      < source = "sl_tail_valid"; >;        // 1 = v1.1 tail read

// Bit values mirror SLRESHADE_SEM_VALID_* in llreshadebridgeabi.h — the ABI
// header is authoritative; change these only together with it.
#define SL_SEM_VALID_COLOR_HDR        0x001u
#define SL_SEM_VALID_DEPTH            0x002u
#define SL_SEM_VALID_ALBEDO           0x004u
#define SL_SEM_VALID_ORM              0x008u
#define SL_SEM_VALID_NORMALS          0x010u
#define SL_SEM_VALID_EMISSIVE         0x020u
#define SL_SEM_VALID_MOTION           0x040u
#define SL_SEM_VALID_MOTION_META      0x080u
#define SL_SEM_VALID_SURFACE_COVERAGE 0x100u
#define SL_SEM_VALID_VISIBLE_DIFFUSE  0x200u

// Mirrors SLRESHADE_RESET_* — any nonzero SL_ResetFlags means temporal history
// (GI accumulation, TAA-style feedback, motion reprojection) must be dropped.
#define SL_RESET_FIRST_VALID_FRAME    0x001u
#define SL_RESET_FRAME_DISCONTINUITY  0x002u
#define SL_RESET_CAMERA_CUT           0x004u
#define SL_RESET_TELEPORT             0x008u
#define SL_RESET_PROJECTION_CHANGE    0x010u
#define SL_RESET_RESIZE               0x020u
#define SL_RESET_TARGET_RECREATED     0x040u
#define SL_RESET_HDR_MODE_CHANGE      0x080u
#define SL_RESET_SOURCE_LOSS          0x100u
#define SL_RESET_CONTEXT_CHANGE       0x200u
#define SL_RESET_SNAPSHOT_TRANSITION  0x400u
#define SL_RESET_PAUSE_RESUME         0x800u

// Mirrors SLRESHADE_CONFIG_*
#define SL_CONFIG_VELOCITY_ENABLED    0x01u
#define SL_CONFIG_HDR_ENABLED         0x02u
#define SL_CONFIG_SNAPSHOT            0x04u
#define SL_CONFIG_FORCE_10BIT         0x08u
#define SL_CONFIG_PROVIDER_OWNED      0x10u

bool SL_SemValid(uint bit)   { return (SL_SemanticValid & bit) != 0u; }
bool SL_HistoryResetAny()    { return SL_ResetFlags != 0u; }
bool SL_HistoryReset(uint bit) { return (SL_ResetFlags & bit) != 0u; }

// --- orientation -------------------------------------------------------------
// Map a ReShade top-left UV to the SL buffer's sampling UV.
float2 SL_UV(float2 uv)
{
#if SL_INPUT_IS_UPSIDE_DOWN
    return float2(uv.x, 1.0 - uv.y);
#else
    return uv;
#endif
}

// --- normal encoding (matches the viewer's globalF.glsl decodeNormal) --------
// OCTAHEDRAL (Knarkowicz), NOT spheremap. Corrected 2026-07-25: this helper
// previously used the Lambert-azimuthal/spheremap decoder, which the viewer has
// not used for some time -- globalF.glsl:46-74 is octahedron encoding. The two
// agree only near the centre (enc=(0.5,0.5) -> (0,0,1)) and diverge completely
// off-axis (enc=(1,0.5): octahedral -> (1,0,0), spheremap -> (0,0,-1)), so every
// consumer was fed wrong normals at grazing angles while still looking plausible
// head-on. Port of the viewer's decodeNormal, expression for expression.
float3 SL_DecodeNormal(float2 enc)
{
    float2 f = enc * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float  t = saturate(-n.z);
    n.xy += float2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
    return normalize(n);                      // view-space, unit length
}

// --- orientation-correct fetches (use these, not raw tex2D) ------------------
float4 SL_NormalRaw(float2 uv)   { return tex2D(SL_sNormals, SL_UV(uv)); }
float3 SL_NormalView(float2 uv)  { return SL_DecodeNormal(SL_NormalRaw(uv).xy); }
float3 SL_Albedo(float2 uv)      { return tex2D(SL_sAlbedo,  SL_UV(uv)).rgb; }
float3 SL_ORM(float2 uv)         { return tex2D(SL_sOrm,     SL_UV(uv)).rgb; }
float3 SL_ColorHDR(float2 uv)    { return tex2D(SL_sColorHdr,SL_UV(uv)).rgb; }
// v1.1 masks. R meaning: MotionMeta = per-pixel motion validity [0,1];
// SurfaceCoverage = deferred/G-buffer visible coverage [0,1]. Gate use on
// SL_SemValid(SL_SEM_VALID_MOTION_META / _SURFACE_COVERAGE) first.
float2 SL_MotionMeta(float2 uv)      { return tex2D(SL_sMotionMeta,      SL_UV(uv)).rg; }
float2 SL_SurfaceCoverage(float2 uv) { return tex2D(SL_sSurfaceCoverage, SL_UV(uv)).rg; }
// v1.2 visible-diffuse sidecar. RGB = LINEAR diffuse (hardware sRGB decode
// already applied -- never decode again), A = exactness K [0,1]. Gate use on
// SL_SemValid(SL_SEM_VALID_VISIBLE_DIFFUSE) first; then gate per pixel on K.
// Do NOT divide RGB by K.
float4 SL_VisibleDiffuse(float2 uv)  { return tex2D(SL_sVisibleDiffuse,  SL_UV(uv)); }

// Motion in screen-space NDC delta. Under a V-flip the Y component's sign also
// inverts, so correct it here — consumers get a ready-to-use motion vector.
float2 SL_Motion(float2 uv)
{
    float2 m = tex2D(SL_sMotion, SL_UV(uv)).xy;
#if SL_INPUT_IS_UPSIDE_DOWN
    m.y = -m.y;
#endif
    return m;
}

#endif // SL_BRIDGE_FXH
