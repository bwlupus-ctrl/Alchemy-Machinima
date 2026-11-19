# Cinematic Gobo Engine & Localized Volumetric Fog Specification

**Target System:** `Alchemy-Machinima` (Windows C++ / OpenGL Deferred Renderer).  
**Authors:** AI Research & Engineering.  
**Scope:** Architectural specification and code design for (1) Built-in & Animated Projector Gobos and (2) Localized Volumetric Fog/Mist Volumes within the hybrid Froxel and Projector Volumetrics pipelines.  
**Target Delivery File:** `I:\alchemy-machinima\doc\CINEMATIC_GOBO_AND_LOCAL_FOG_SPEC.md`  
**Status:** Design Specification for Adversarial Review (Zero Source Code Modified).

---

## 0. Executive Summary & Design Principles

Cinematic lighting differs fundamentally from gameplay lighting: it requires precise shaping (shadow breakup, optical framing, atmospheric scattering) and localized staging (ground mist, smoke pockets, beam texturing) without imposing server-side asset or prim limits.

This specification details two decoupled, zero-regression client-side systems:
1. **Cinematic Gobo / Cookie Engine**:
   - Built-in library of high-bit-depth optical textures.
   - Client-side projector texture overrides on arbitrary in-world or virtual set lights.
   - Real-time procedural UV animation: continuous rotation, wind sway domain-warping, synthesized caustics, and optical chromatic dispersion.
   - Unified consumption across standard surface lighting (`spotLightF.glsl`), per-cone volumetric raymarching (`projectorVolumetricF.glsl`), and froxel light injection (`froxelInjectF.glsl`).
2. **Localized Volumetric Fog & Ground Mist Volumes**:
   - Client-placed 3D bounding shapes (Oriented Bounding Boxes, Ellipsoids, Cylinders) with Signed Distance Field (SDF) smooth edge feathering.
   - Density, ground-hugging height falloff, internal billow turbulence, and custom scattering albedos.
   - Direct integration into the Hybrid Froxel media pass (`froxelMediaF.glsl`), receiving full shadow occlusion and multi-light in-scattering with $O(1)$ draw cost.
   - In-world 3D manipulator gizmo and visual wireframe cage.

---

## 1. System 1: Cinematic Gobo & Animated Cookie Engine

### 1.1 Architectural Anchors in Current Codebase
* **Surface Deferred Spotlights**: [`pipeline.cpp:16648-16685`](file:///I:/alchemy-machinima/indra/newview/pipeline.cpp#L16648) $\rightarrow$ [`spotLightF.glsl`](file:///I:/alchemy-machinima/indra/newview/app_settings/shaders/class3/deferred/spotLightF.glsl) via `gDeferredSpotLightProgram`.
* **Per-Cone Hero Volumetrics**: [`pipeline.cpp:renderProjectorVolumetric`](file:///I:/alchemy-machinima/indra/newview/pipeline.cpp) $\rightarrow$ [`projectorVolumetricF.glsl`](file:///I:/alchemy-machinima/indra/newview/app_settings/shaders/class3/deferred/projectorVolumetricF.glsl).
* **Froxel Light Injection**: `LLPipeline::renderFroxelInject()` $\rightarrow$ [`froxelInjectF.glsl`](file:///I:/alchemy-machinima/indra/newview/app_settings/shaders/class1/deferred/froxelInjectF.glsl).
* **Cookie Sampling Utility**: [`deferredUtil.glsl:goboLod()`](file:///I:/alchemy-machinima/indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl#L84), `getProjectedLightDiffuseColor()`.

### 1.2 C++ Data Model & Manager (`algobomanager.h` / `algobomanager.cpp`)

```cpp
#pragma once
#include "llsingleton.h"
#include "lluuid.h"
#include "llvector2.h"
#include "llvector3.h"
#include "llrender.h"
#include <string>
#include <vector>
#include <map>

enum class EGoboAnimType : S32
{
    NONE = 0,
    ROTATION = 1,       // Continuous rotation (ceiling fans, rotating beacons)
    WIND_SWAY = 2,      // Low-frequency organic domain warping (foliage, branches)
    WATER_CAUSTICS = 3, // Procedural dual-layer Voronoi ripple
    PULSE_ZOOM = 4      // Subtle breathing / expansion
};

struct ALProjectorGoboParams
{
    bool            mEnabled{false};
    std::string     mBuiltinPresetName{""};
    LLUUID          mCustomTextureId{LLUUID::null};
    
    // Transform
    LLVector2       mScale{1.0f, 1.0f};
    LLVector2       mOffset{0.0f, 0.0f};
    F32             mRotationAngleRad{0.0f};
    
    // Animation
    EGoboAnimType   mAnimType{EGoboAnimType::NONE};
    F32             mAnimSpeed{1.0f};
    F32             mAnimIntensity{0.1f};
    F32             mAnimFrequency{2.0f};
    
    // Optical dispersion / chromatic fringe
    F32             mChromaticDispersion{0.0f}; // [0.0, 0.05]
    
    // Focus / Blur
    F32             mBlurOffset{0.0f};
};

class ALGoboManager : public LLSingleton<ALGoboManager>
{
    LLSINGLETON(ALGoboManager);
    ~ALGoboManager() = default;

public:
    struct GoboPresetEntry
    {
        std::string mId;
        std::string mDisplayName;
        std::string mRelativeAssetPath;
        LLPointer<LLViewerFetchedTexture> mTextureRef;
    };

    void init();
    const std::vector<GoboPresetEntry>& getPresetList() const { return mPresets; }
    
    // Per-drawable / per-projector runtime overrides
    void setOverride(const LLUUID& lightId, const ALProjectorGoboParams& params);
    bool getOverride(const LLUUID& lightId, ALProjectorGoboParams& outParams) const;
    void clearOverride(const LLUUID& lightId);
    void clearAllOverrides();

    // Shader upload helper for setupSpotLight / setupSpotLightVolumetric
    void uploadGoboUniforms(LLGLSLShader* shader, const LLUUID& lightId, F32 simTime);

private:
    std::vector<GoboPresetEntry> mPresets;
    std::map<LLUUID, ALProjectorGoboParams> mOverrides;
};
```

### 1.3 Shader Uniform Additions (`llshadermgr.h` / `llshadermgr.cpp`)

Add the following uniforms to `LLShaderMgr`:
* `GOBO_ANIM_PARAMS` (`"gobo_anim_params"`, `vec4`: `(anim_type, anim_speed, anim_intensity, anim_freq)`)
* `GOBO_TRANSFORM` (`"gobo_transform"`, `vec4`: `(scale_x, scale_y, offset_x, offset_y)`)
* `GOBO_OPTICS` (`"gobo_optics"`, `vec4`: `(rotation_rad, chromatic_dispersion, blur_offset, sim_time)`)

### 1.4 GLSL Implementation in `deferredUtil.glsl`

```glsl
// --- Gobo / Cookie Optical Engine Uniforms ---
uniform vec4 gobo_anim_params; // (anim_type, speed, intensity, freq)
uniform vec4 gobo_transform;   // (scale.x, scale.y, offset.x, offset.y)
uniform vec4 gobo_optics;      // (rot_rad, dispersion, blur_offset, time)

// Procedural 2D Hash & Value Noise for Wind Sway
float goboHash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float goboNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(goboHash(i + vec2(0.0, 0.0)),
                   goboHash(i + vec2(1.0, 0.0)), f.x),
               mix(goboHash(i + vec2(0.0, 1.0)),
                   goboHash(i + vec2(1.0, 1.0)), f.x), f.y);
}

// Procedural Voronoi Caustics Generator
float goboVoronoiCaustics(vec2 uv, float time)
{
    vec2 p = uv * 8.0;
    vec2 p1 = p + vec2(time * 0.4, time * 0.3);
    vec2 p2 = p - vec2(time * 0.3, time * 0.5);

    // Layer 1
    vec2 i1 = floor(p1);
    vec2 f1 = fract(p1);
    float d1 = 1.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 g = vec2(float(x), float(y));
            vec2 o = vec2(goboHash(i1 + g), goboHash(i1 + g + 57.0));
            o = 0.5 + 0.4 * sin(time + 6.2831 * o);
            d1 = min(d1, length(g + o - f1));
        }
    }

    // Layer 2
    vec2 i2 = floor(p2);
    vec2 f2 = fract(p2);
    float d2 = 1.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 g = vec2(float(x), float(y));
            vec2 o = vec2(goboHash(i2 + g + 11.0), goboHash(i2 + g + 73.0));
            o = 0.5 + 0.4 * cos(time * 0.8 + 6.2831 * o);
            d2 = min(d2, length(g + o - f2));
        }
    }

    float c = min(d1, d2);
    return pow(c, 2.5) * 3.5;
}

// Master Gobo UV Modulation
vec2 transformGoboCoords(vec2 in_tc)
{
    int   anim_type = int(gobo_anim_params.x);
    float speed     = gobo_anim_params.y;
    float intensity = gobo_anim_params.z;
    float freq      = gobo_anim_params.w;

    float base_rot  = gobo_optics.x;
    float time      = gobo_optics.w;

    // 1. Center pivot
    vec2 uv = in_tc - vec2(0.5);

    // 2. Scale & Offset
    uv = uv / max(gobo_transform.xy, vec2(0.001)) - gobo_transform.zw;

    // 3. Rotation Evaluation
    float theta = base_rot;
    if (anim_type == 1) // ROTATION
    {
        theta += time * speed;
    }
    
    float cos_t = cos(theta);
    float sin_t = sin(theta);
    uv = vec2(uv.x * cos_t - uv.y * sin_t, uv.x * sin_t + uv.y * cos_t);

    // 4. Domain Warping (Wind Sway)
    if (anim_type == 2) // WIND_SWAY
    {
        vec2 noise_coord = uv * freq + vec2(time * speed * 0.5);
        vec2 warp = vec2(goboNoise(noise_coord), goboNoise(noise_coord + vec2(17.3))) * 2.0 - 1.0;
        uv += warp * intensity;
    }

    // 5. Uncenter
    return uv + vec2(0.5);
}

// Optical Sampling with Chromatic Dispersion & Focus Blur
vec4 sampleGoboColor(float l_dist, vec2 projected_uv)
{
    int anim_type = int(gobo_anim_params.x);
    float time    = gobo_optics.w;

    if (anim_type == 3) // WATER_CAUSTICS procedural synthesis
    {
        vec2 uv = transformGoboCoords(projected_uv);
        float caust = goboVoronoiCaustics(uv, time * gobo_anim_params.y);
        return vec4(vec3(caust), 1.0);
    }

    vec2 uv = transformGoboCoords(projected_uv);
    
    // Bounds check
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        return vec4(0.0);
    }

    float lod = gobo_optics.z; // blur offset
    float dispersion = gobo_optics.y;

    if (dispersion > 0.0001)
    {
        vec2 center_dir = uv - vec2(0.5);
        vec2 uv_r = uv + center_dir * dispersion;
        vec2 uv_g = uv;
        vec2 uv_b = uv - center_dir * dispersion;

        float r = textureLod(projectionMap, uv_r, goboLod(uv_r, lod)).r;
        float g = textureLod(projectionMap, uv_g, goboLod(uv_g, lod)).g;
        float b = textureLod(projectionMap, uv_b, goboLod(uv_b, lod)).b;
        float a = textureLod(projectionMap, uv,   goboLod(uv,   lod)).a;
        return vec4(r, g, b, a);
    }
    else
    {
        return textureLod(projectionMap, uv, goboLod(uv, lod));
    }
}
```

---

## 2. System 2: Localized Volumetric Fog & Ground Mist Volumes

### 2.1 Architectural Anchors
* **Froxel Media Pass**: [`pipeline.cpp:renderFroxelMedia()`](file:///I:/alchemy-machinima/indra/newview/pipeline.cpp) $\rightarrow$ [`froxelMediaF.glsl`](file:///I:/alchemy-machinima/indra/newview/app_settings/shaders/class1/deferred/froxelMediaF.glsl).
* **Froxel Coordinates & Atlas Decoding**: [`froxelUtil.glsl`](file:///I:/alchemy-machinima/indra/newview/app_settings/shaders/class1/deferred/froxelUtil.glsl).
* **Gizmo / Manipulator Integration**: [`ALGhostManipProxy`](file:///I:/alchemy-machinima/indra/newview/alghostmanipproxy.h) and [`ALWorldOverlayViz`](file:///I:/alchemy-machinima/indra/newview/alworldoverlayviz.h).

### 2.2 C++ Data Model (`allocalfogmanager.h` / `allocalfogmanager.cpp`)

```cpp
#pragma once
#include "llsingleton.h"
#include "lluuid.h"
#include "llvector3.h"
#include "llquaternion.h"
#include "llcolor3.h"
#include "llsd.h"
#include <vector>
#include <string>

enum class EFogShapeType : S32
{
    OBB_BOX = 0,
    ELLIPSOID = 1,
    CYLINDER = 2
};

struct ALLocalFogVolume
{
    LLUUID          mId;
    std::string     mName{"Mist Volume"};
    bool            mEnabled{true};
    EFogShapeType   mShape{EFogShapeType::OBB_BOX};

    // Transform (Agent / World Space, Z-Up)
    LLVector3       mCenterAgent{0.0f, 0.0f, 0.0f};
    LLVector3       mExtents{5.0f, 5.0f, 2.0f}; // Half-widths in meters
    LLQuaternion    mRotation{LLQuaternion::DEFAULT};

    // Participating Media Parameters
    F32             mDensity{0.15f};          // Extinction sigma_t per meter [0.0, 5.0]
    F32             mEdgeFeather{0.6f};       // SDF softness fraction [0.01, 1.0]
    F32             mGroundFalloff{0.5f};     // Exponential height falloff inside volume
    F32             mGroundBaseZ{0.0f};       // Altitude floor offset
    LLColor3        mScatteringColor{1.f, 1.f, 1.f}; // Albedo tint

    // Internal Turbulence
    F32             mNoiseStrength{0.5f};
    F32             mNoiseScale{0.25f};       // Cycles per meter
    LLVector3       mTurbulenceWind{0.1f, 0.0f, 0.0f}; // m/s internal drift
};

class ALLocalFogManager : public LLSingleton<ALLocalFogManager>
{
    LLSINGLETON(ALLocalFogManager);
    ~ALLocalFogManager() = default;

public:
    static constexpr U32 MAX_LOCAL_FOG_VOLUMES = 8;

    LLUUID createVolume(const LLVector3& center, EFogShapeType shape = EFogShapeType::OBB_BOX);
    void   deleteVolume(const LLUUID& id);
    void   clearAllVolumes();

    ALLocalFogVolume* getVolume(const LLUUID& id);
    const std::vector<ALLocalFogVolume>& getVolumes() const { return mVolumes; }

    // Serialization for take/scene persistence
    LLSD   serializeLLSD() const;
    void   deserializeLLSD(const LLSD& data);

    // Shader Uniform Upload (invoked per-frame in renderFroxelMedia)
    void   uploadUniforms(LLGLSLShader* mediaShader, F32 simTime);

    // Selection & Gizmo
    void   setSelectedVolume(const LLUUID& id) { mSelectedId = id; }
    LLUUID getSelectedVolume() const { return mSelectedId; }

private:
    std::vector<ALLocalFogVolume> mVolumes;
    LLUUID mSelectedId{LLUUID::null};
};
```

### 2.3 Local Fog Media Evaluation in `froxelMediaF.glsl`

Add uniform arrays matching `MAX_LOCAL_FOG_VOLUMES = 8`:
```glsl
// --- Local Volumetric Fog Uniforms ---
const int MAX_LOCAL_FOG_VOLUMES = 8;

uniform int   local_fog_count;
uniform vec4  local_fog_center_shape[MAX_LOCAL_FOG_VOLUMES]; // (center.xyz, shape_type)
uniform vec4  local_fog_extents_feather[MAX_LOCAL_FOG_VOLUMES]; // (extents.xyz, feather)
uniform vec4  local_fog_rot_col0[MAX_LOCAL_FOG_VOLUMES]; // mat3 inverse rotation col 0
uniform vec4  local_fog_rot_col1[MAX_LOCAL_FOG_VOLUMES]; // mat3 inverse rotation col 1
uniform vec4  local_fog_rot_col2[MAX_LOCAL_FOG_VOLUMES]; // mat3 inverse rotation col 2
uniform vec4  local_fog_params[MAX_LOCAL_FOG_VOLUMES]; // (density, ground_falloff, ground_base, noise_str)
uniform vec4  local_fog_albedo[MAX_LOCAL_FOG_VOLUMES]; // (albedo.rgb, noise_scale)
uniform vec4  local_fog_wind[MAX_LOCAL_FOG_VOLUMES];   // (wind.xyz, time)

// SDF Mask Evaluator for Local Volumes
float evaluateLocalFogMask(int i, vec3 wpos, out vec3 local_p)
{
    vec3 center   = local_fog_center_shape[i].xyz;
    int  shape    = int(local_fog_center_shape[i].w);
    vec3 extents  = local_fog_extents_feather[i].xyz;
    float feather = local_fog_extents_feather[i].w;

    mat3 inv_rot = mat3(local_fog_rot_col0[i].xyz,
                        local_fog_rot_col1[i].xyz,
                        local_fog_rot_col2[i].xyz);

    // Transform world position to volume local space
    local_p = inv_rot * (wpos - center);

    float mask = 0.0;
    float f_dist = max(feather * min(extents.x, min(extents.y, extents.z)), 0.01);

    if (shape == 0) // OBB Box
    {
        vec3 d = abs(local_p) - extents;
        float outside_dist = length(max(d, vec3(0.0)));
        float inside_dist  = min(max(d.x, max(d.y, d.z)), 0.0);
        float dist = outside_dist + inside_dist;
        mask = smoothstep(0.0, -f_dist, dist);
    }
    else if (shape == 1) // Ellipsoid
    {
        vec3 norm_p = local_p / max(extents, vec3(0.001));
        float q = dot(norm_p, norm_p);
        mask = smoothstep(1.0, 1.0 - feather, q);
    }
    else if (shape == 2) // Vertical Cylinder
    {
        float r_norm = length(local_p.xy) / max(extents.x, 0.001);
        float h_norm = abs(local_p.z) / max(extents.z, 0.001);
        float mask_r = smoothstep(1.0, 1.0 - feather, r_norm);
        float mask_h = smoothstep(1.0, 1.0 - feather, h_norm);
        mask = mask_r * mask_h;
    }

    return mask;
}
```

#### Media Pass Integration (`froxelMediaF.glsl:main`):
```glsl
    // --- Evaluate Base Global Medium ---
    float sigma_t = max(froxel_density, 0.0);
    vec3  sigma_s = vec3(sigma_t); // Global albedo = 1.0

    // Apply global height fog & global wind noise
    if (froxel_fog_strength > 0.0) { /* existing height fog */ }
    if (froxel_noise_strength > 0.0) { /* existing global noise */ }

    // --- Evaluate Localized Fog Volumes ---
    for (int i = 0; i < local_fog_count; ++i)
    {
        vec3 local_p;
        float mask = evaluateLocalFogMask(i, wpos, local_p);
        if (mask <= 0.0001) continue;

        float vol_density   = local_fog_params[i].x;
        float g_falloff     = local_fog_params[i].y;
        float g_base        = local_fog_params[i].z;
        float noise_str     = local_fog_params[i].w;
        vec3  vol_albedo    = local_fog_albedo[i].rgb;
        float noise_scale   = local_fog_albedo[i].w;
        vec3  wind          = local_fog_wind[i].xyz;
        float time          = local_fog_wind[i].w;

        // Height falloff relative to volume bottom
        float height_factor = 1.0;
        if (g_falloff > 0.0)
        {
            float rel_z = local_p.z - g_base;
            height_factor = exp(-max(rel_z, 0.0) * g_falloff);
        }

        // Localized turbulence
        float noise_factor = 1.0;
        if (noise_str > 0.0)
        {
            vec3 noise_pos = (wpos - wind * time) * noise_scale;
            noise_factor = mix(1.0, froxelFbm(noise_pos), noise_str);
        }

        float local_sigma_t = vol_density * mask * height_factor * noise_factor;
        sigma_t += local_sigma_t;
        sigma_s += local_sigma_t * vol_albedo;
    }

    frag_color = vec4(sigma_s, sigma_t);
```

---

## 3. In-World Visualization & Manipulator Gizmo

When a fog volume is selected in the UI:
1. **Bounding Cage Drawing** ([`alworldoverlayviz.cpp`](file:///I:/alchemy-machinima/indra/newview/alworldoverlayviz.cpp)):
   - Evaluated during `render_ui_3d` with `LLGLSUIDefault` and depth-write off.
   - For **OBB**: 12 wireframe edge lines transforming local $(\pm e_x, \pm e_y, \pm e_z)$ by $T(\vec{c}) \cdot R(\mathbf{q})$.
   - For **Ellipsoid**: 3 orthogonal circle rings (XY, XZ, YZ).
   - For **Cylinder**: Top/bottom circle rings + 4 connecting vertical struts.
2. **Transform Gizmo** ([`alghostmanipproxy.cpp`](file:///I:/alchemy-machinima/indra/newview/alghostmanipproxy.h)):
   - Repurpose the existing 3-axis translation/rotation/scale gizmo.
   - Dragging arrow axes alters `mCenterAgent`.
   - Dragging cube handles alters non-uniform `mExtents`.
   - Dragging rotation rings alters `mRotation`.

---

## 4. UI Specification (`floater_lightbox_settings.xml`)

### 4.1 Gobo Controls in Selected Light Panel
```xml
<!-- Gobo / Cookie Section under Selected Projector -->
<text font="SansSerifBold" label="Cinematic Gobo / Cookie" />
<check_box name="sl_gobo_enable" label="Enable Custom Gobo Override" />
<combo_box name="sl_gobo_preset" label="Preset">
    <combo_item name="custom" value="0">Custom UUID / In-World</combo_item>
    <combo_item name="venetian_fine" value="1">Venetian Blinds (Fine)</combo_item>
    <combo_item name="venetian_wide" value="2">Venetian Blinds (Wide)</combo_item>
    <combo_item name="foliage_dappled" value="3">Dappled Tree Canopy</combo_item>
    <combo_item name="ceiling_fan" value="4">Ceiling Fan</combo_item>
    <combo_item name="prison_bars" value="5">Prison / Window Grate</combo_item>
    <combo_item name="french_window" value="6">French Multi-Pane</combo_item>
    <combo_item name="water_caustics" value="7">Procedural Water Caustics</combo_item>
</combo_box>
<combo_box name="sl_gobo_anim_mode" label="Animation Mode">
    <combo_item name="none" value="0">Static</combo_item>
    <combo_item name="rotate" value="1">Continuous Rotation</combo_item>
    <combo_item name="wind" value="2">Wind Sway (Organic)</combo_item>
    <combo_item name="caustics" value="3">Caustic Ripple</combo_item>
</combo_box>
<slider name="sl_gobo_speed" label="Anim Speed" min_val="0.0" max_val="10.0" />
<slider name="sl_gobo_intensity" label="Anim Intensity" min_val="0.0" max_val="1.0" />
<slider name="sl_gobo_dispersion" label="Chromatic Fringe" min_val="0.0" max_val="0.05" />
<slider name="sl_gobo_scale" label="Gobo Scale" min_val="0.1" max_val="5.0" />
<slider name="sl_gobo_rotation" label="Manual Rotation" min_val="0.0" max_val="360.0" />
```

### 4.2 Localized Fog Panel ("Local Mist" Tab)
```xml
<panel label="Local Mist" name="local_mist_panel">
    <scroll_list name="fog_volume_list" height="120" />
    <button name="btn_add_fog_box" label="+ Add Fog Box" />
    <button name="btn_add_fog_sphere" label="+ Add Fog Sphere" />
    <button name="btn_delete_fog" label="Delete Selected" />
    
    <text font="SansSerifBold" label="Selected Volume Properties" />
    <slider name="fog_density" label="Extinction Density" min_val="0.0" max_val="2.0" />
    <slider name="fog_feather" label="Edge Softness" min_val="0.01" max_val="1.0" />
    <slider name="fog_ground_falloff" label="Ground Cling Falloff" min_val="0.0" max_val="5.0" />
    <slider name="fog_noise_str" label="Turbulence Strength" min_val="0.0" max_val="1.0" />
    <slider name="fog_noise_scale" label="Turbulence Scale" min_val="0.01" max_val="1.0" />
    <color_swatch name="fog_albedo_color" label="Scattering Color" />
</panel>
```

---

## 5. Adversarial Verification & Failure Mode Analysis

| Failure Mode / Edge Case | Risk | Engineered Mitigation |
|---|---|---|
| **Matrix Inversion Singularities** | Zero-sized volume extents cause division by zero in GLSL SDF calculation. | Enforce `max(extents, vec3(0.001))` in C++ model and clamp divisor in `evaluateLocalFogMask`. |
| **Gobo UV Wrap Bleed** | Repeating textures tile across the spotlight outer cone. | Explicit clamp to `[0.0, 1.0]` with border color zero (`vec4(0.0)`) outside unit bounds in `sampleGoboColor`. |
| **Froxel Atlas Tile Seams** | High local densities spilling across Z-slice boundaries. | Froxel sampling clamps coordinates to half-texel inside each atlas tile boundary (handled by `froxelUtil.glsl`). |
| **Coordinate Space Mismatches** | Mixing `LLVector3d` (global coords) and `LLVector3` (agent coords) causing distant floating volumes. | `ALLocalFogVolume` stores strictly Agent coordinates (`mCenterAgent`); conversions to View space use current frame `inv_modelview`. |
| **Zero Overhead When Inactive** | Running 8 volume evaluations per froxel when no local fog is created. | `if (local_fog_count == 0)` short-circuit branch in `froxelMediaF.glsl` leaves baseline performance byte-identical. |
| **Quaternion Non-Unit Degeneration** | Interactive gizmo rotations accumulating float error and skewing bounding boxes. | Normalize `mRotation.normalize()` on every gizmo commit and C++ update. |

---

## 6. Verification & Validation Protocol

1. **Gobo Optical Isolation**:
   - Aim a projector at a flat white wall.
   - Cycle through `Venetian Blinds`, `Ceiling Fan`, `Tree Canopy`.
   - Verify crisp shadow borders; verify continuous rotation at $\omega = 1.0\,\text{rad/s}$ has zero angular stutter.
   - Increase `Chromatic Fringe` to $0.03$; verify red/blue fringes disperse radially outward from beam center.
2. **Volumetric Cone Gobo Slicing**:
   - Turn on `BDMergeProjectorVolumetrics` and inspect the light shaft.
   - Confirm the 3D air shaft splits into visible volumetric light sheets matching the Venetian blind slats.
3. **Local Fog Placement & Feathering**:
   - Spawn a $3\text{m} \times 3\text{m} \times 2\text{m}$ Fog Box on an uneven terrain surface.
   - Set `Edge Softness = 0.8`.
   - Verify there is zero hard geometry intersection line where the fog meets the ground plane.
   - Shine a spotlight through the box: verify intense in-scattering occurs *inside* the box, while the air *outside* remains crystal clear.
