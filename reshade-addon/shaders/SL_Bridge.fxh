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
// Default ON. IMPORTANT: keep this MATCHED to your RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN
// so our normals/motion land in the same orientation as the shared DEPTH buffer.
#ifndef SL_INPUT_IS_UPSIDE_DOWN
#define SL_INPUT_IS_UPSIDE_DOWN 1
#endif

// --- textures bound by the add-on via update_texture_bindings(semantic) ------
texture SLNormalsTex  : SL_NORMALS;      // spheremap .xy, env .z, gbuffer-flags .w
texture SLMotionTex   : SL_MOTION_NDC;   // RG16F screen-space NDC delta (0 if off)
texture SLAlbedoTex   : SL_ALBEDO;
texture SLOrmTex      : SL_ORM;          // occlusion / roughness / metallic
texture SLColorHdrTex : SL_COLOR_HDR;    // pre-tonemap scene color

sampler SL_sNormals  { Texture = SLNormalsTex;  };
sampler SL_sMotion   { Texture = SLMotionTex;   };
sampler SL_sAlbedo   { Texture = SLAlbedoTex;   };
sampler SL_sOrm      { Texture = SLOrmTex;      };
sampler SL_sColorHdr { Texture = SLColorHdrTex; };

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
float3 SL_DecodeNormal(float2 enc)
{
    float2 fenc = enc * 4.0 - 2.0;
    float  f    = dot(fenc, fenc);
    float  g    = sqrt(saturate(1.0 - f / 4.0));
    return float3(fenc * g, 1.0 - f / 2.0);   // view-space
}

// --- orientation-correct fetches (use these, not raw tex2D) ------------------
float4 SL_NormalRaw(float2 uv)   { return tex2D(SL_sNormals, SL_UV(uv)); }
float3 SL_NormalView(float2 uv)  { return SL_DecodeNormal(SL_NormalRaw(uv).xy); }
float3 SL_Albedo(float2 uv)      { return tex2D(SL_sAlbedo,  SL_UV(uv)).rgb; }
float3 SL_ORM(float2 uv)         { return tex2D(SL_sOrm,     SL_UV(uv)).rgb; }
float3 SL_ColorHDR(float2 uv)    { return tex2D(SL_sColorHdr,SL_UV(uv)).rgb; }

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
