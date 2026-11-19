/**
 * @file alcinehaze.cpp
 * @brief Cinematic Depth Atmosphere ("cine haze"): settings -> shader uniforms.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alcinehaze.h"

#include "llglslshader.h"
#include "llshadermgr.h"
#include "llrender.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "v3color.h"

extern bool gCubeSnapshot;

namespace
{
// Sun lobe pow(cos(theta), e) with e chosen so the lobe is at half strength at the
// requested angular half-width:  e = ln(0.5) / ln(cos(width)).
F32 lobe_exponent_from_width_deg(F32 width_deg)
{
    const F32 w = llclamp(width_deg, 1.f, 89.f) * DEG_TO_RAD;
    return logf(0.5f) / logf(cosf(w));
}

void set_setting(const char* name, const LLSD& value)
{
    if (LLControlVariable* control = gSavedSettings.getControl(name))
    {
        control->setValue(value);
    }
}

void reset_setting(const char* name)
{
    if (LLControlVariable* control = gSavedSettings.getControl(name))
    {
        control->resetToDefault(true);
    }
}

const char* const kCineHazeSettings[] = {
    "CineHazeEnable",
    "CineHazeStrength",
    "CineHazeDensity",
    "CineHazeStartDistance",
    "CineHazeMaxOpacity",
    "CineHazeColorMode",
    "CineHazeColor",
    "CineHazeColorIntensity",
    "CineHazeHeightEnable",
    "CineHazeReferenceHeight",
    "CineHazeHeightFalloff",
    "CineHazeSunTint",
    "CineHazeSunStrength",
    "CineHazeSunLobeWidth",
    "CineHazeDebugView",
};
} // anonymous namespace

bool ALCineHaze::isEnabled()
{
    static LLCachedControl<bool> enable(gSavedSettings, "CineHazeEnable", false);
    static LLCachedControl<F32>  strength(gSavedSettings, "CineHazeStrength", 1.f);
    static LLCachedControl<F32>  density(gSavedSettings, "CineHazeDensity", 0.004f);
    // zero density / zero strength == disabled: no work, bit-exact off path
    return enable() && strength() > 0.f && density() > 0.f;
}

bool ALCineHaze::isSuppressedByFroxel()
{
    // The fork's froxel volumetrics (BDMergeFroxelVolumetrics, default off) are a
    // second distance + optional height medium (froxelMediaF.glsl height fog,
    // composited in renderFinalize via renderFroxelVolumetrics()).  Running both
    // would fog the same view ray twice, so the two are mutually exclusive: the
    // froxel master wins and this layer no-ops (bit-exact off path) while it is on.
    static LLCachedControl<bool> froxel(gSavedSettings, "BDMergeFroxelVolumetrics", false);
    return froxel();
}

bool ALCineHaze::isActiveForCurrentPass()
{
    if (!isEnabled() || isSuppressedByFroxel())
    {
        return false;
    }
    // Reflection / irradiance probe faces: keep the layer out of the probes so it
    // never feeds back into ambient lighting and is never applied a second time
    // through a reflection.  HUDs and avatar impostors never see world atmosphere.
    if (gCubeSnapshot || LLPipeline::sRenderingHUDs || LLPipeline::sImpostorRender)
    {
        return false;
    }
    return true;
}

void ALCineHaze::bind(LLGLSLShader& shader)
{
    if (shader.getUniformLocation(LLShaderMgr::CINE_HAZE_ACTIVE) < 0)
    {
        return; // this program does not link cineHazeF.glsl
    }

    const bool active = isActiveForCurrentPass();
    shader.uniform1i(LLShaderMgr::CINE_HAZE_ACTIVE, active ? 1 : 0);
    if (!active)
    {
        return; // the shader never reads the remaining uniforms when inactive
    }

    static LLCachedControl<F32>      strength(gSavedSettings, "CineHazeStrength", 1.f);
    static LLCachedControl<F32>      density(gSavedSettings, "CineHazeDensity", 0.004f);
    static LLCachedControl<F32>      start_distance(gSavedSettings, "CineHazeStartDistance", 20.f);
    static LLCachedControl<F32>      max_opacity(gSavedSettings, "CineHazeMaxOpacity", 0.85f);
    static LLCachedControl<S32>      color_mode(gSavedSettings, "CineHazeColorMode", 1);
    static LLCachedControl<LLColor3> color(gSavedSettings, "CineHazeColor", LLColor3(0.62f, 0.70f, 0.80f));
    static LLCachedControl<F32>      color_intensity(gSavedSettings, "CineHazeColorIntensity", 1.f);
    static LLCachedControl<bool>     height_enable(gSavedSettings, "CineHazeHeightEnable", false);
    static LLCachedControl<F32>      reference_height(gSavedSettings, "CineHazeReferenceHeight", 20.f);
    static LLCachedControl<F32>      height_falloff(gSavedSettings, "CineHazeHeightFalloff", 0.05f);
    static LLCachedControl<LLColor3> sun_tint(gSavedSettings, "CineHazeSunTint", LLColor3(1.f, 0.86f, 0.68f));
    static LLCachedControl<F32>      sun_strength(gSavedSettings, "CineHazeSunStrength", 0.f);
    static LLCachedControl<F32>      sun_lobe_width(gSavedSettings, "CineHazeSunLobeWidth", 30.f);
    static LLCachedControl<S32>      debug_view(gSavedSettings, "CineHazeDebugView", 0);

    // Units: SL world metres.  Eye-space positions in the shaders are metres too
    // (the modelview carries no scale), so density is 1/m and distances are m.
    shader.uniform4f(LLShaderMgr::CINE_HAZE_PARAMS,
                     llmax(density(), 0.f),
                     llmax(start_distance(), 0.f),
                     llclamp(max_opacity(), 0.f, 1.f),
                     llclamp(strength(), 0.f, 1.f));

    // World-up in eye space and the camera's world height, both from the CURRENT
    // modelview so every camera (main view, prism/VCam, mirror, probe) gets its
    // own frame.  Agent-space Z is absolute SL altitude and is untouched by
    // region-crossing origin rebasing (only X/Y shift), so the layer never jumps.
    // glm is column-major: mv[col][row].  R = mv[0..2][0..2], t = mv[3].
    //   world +Z in eye space = R * (0,0,1) = column 2 of R
    //   camera world position = -R^T t  ->  z = -sum_i R[i][2] * t[i]
    const glm::mat4 mv = get_current_modelview();
    const F32 up_x = mv[2][0];
    const F32 up_y = mv[2][1];
    const F32 up_z = mv[2][2];
    const F32 cam_h = -(mv[2][0] * mv[3][0] + mv[2][1] * mv[3][1] + mv[2][2] * mv[3][2]);
    shader.uniform3f(LLShaderMgr::CINE_HAZE_UP, up_x, up_y, up_z);
    shader.uniform4f(LLShaderMgr::CINE_HAZE_HEIGHT,
                     height_enable() ? 1.f : 0.f,
                     reference_height(),
                     llmax(height_falloff(), 0.f),   // k >= 0: denser low, thinner high
                     cam_h);

    // Manual colour stays an sRGB picker value; the shader converts it and scales
    // by sky_hdr_scale so white == a white WindLight sky pixel (pre-exposure).
    const LLColor3& c = color();
    shader.uniform4f(LLShaderMgr::CINE_HAZE_COLOR, c.mV[0], c.mV[1], c.mV[2], llmax(color_intensity(), 0.f));
    shader.uniform1i(LLShaderMgr::CINE_HAZE_COLOR_MODE, (color_mode() == 1) ? 1 : 0);

    // Sun tint is pre-converted to linear here (strength multiplies linear light).
    const F32 sun_str = llmax(sun_strength(), 0.f);
    if (sun_str > 0.f)
    {
        const LLColor3 tint_linear = linearColor3(sun_tint());
        shader.uniform4f(LLShaderMgr::CINE_HAZE_SUN,
                         tint_linear.mV[0] * sun_str,
                         tint_linear.mV[1] * sun_str,
                         tint_linear.mV[2] * sun_str,
                         lobe_exponent_from_width_deg(sun_lobe_width()));
    }
    else
    {
        shader.uniform4f(LLShaderMgr::CINE_HAZE_SUN, 0.f, 0.f, 0.f, 0.f);   // w <= 0 disables the lobe
    }

    shader.uniform1i(LLShaderMgr::CINE_HAZE_DEBUG, llclamp(debug_view(), 0, 6));
    // Per-draw hint; the alpha pool raises it around additive-blended draws.
    shader.uniform1i(LLShaderMgr::CINE_HAZE_ADDITIVE, 0);
}

void ALCineHaze::applyPreset(const std::string& name)
{
    // One preset = one fully-specified look.  Every field is written so switching
    // presets never inherits a previous preset's manual colour, height layer or
    // sun lobe.  density is extinction per metre: half-contrast distance past the
    // start is ~ln(2)/density (0.0035 ~= 198 m, 0.014 ~= 50 m).  colormode 1 =
    // sky-linked (matches the horizon, seamless); 0 = manual colour.  Height
    // presets anchor the layer at the current camera altitude (refFromCam).
    struct Preset
    {
        F32      strength   = 0.7f;
        F32      density    = 0.004f;
        F32      start      = 20.f;
        F32      maxop      = 0.85f;
        S32      colormode  = 1;
        LLColor3 color      = LLColor3(0.62f, 0.70f, 0.80f);
        F32      colorint   = 1.f;
        bool     height     = false;
        F32      falloff    = 0.05f;
        bool     refFromCam = false;
        LLColor3 suntint    = LLColor3(1.f, 0.86f, 0.68f);
        F32      sunstr     = 0.f;
        F32      lobew      = 35.f;
    };

    Preset p;
    if (name == "subtle")
    {   // Restrained distance haze fading toward the sky, faint warm sun.
        p.strength = 0.65f; p.density = 0.0035f; p.start = 15.f; p.maxop = 0.75f;
        p.sunstr = 0.2f;    p.lobew = 35.f;
    }
    else if (name == "vista")
    {   // Aerial vista: light, long-range depth cueing for landscapes.
        p.strength = 0.80f; p.density = 0.0016f; p.start = 50.f; p.maxop = 0.70f;
        p.suntint = LLColor3(1.f, 0.88f, 0.72f); p.sunstr = 0.25f; p.lobew = 45.f;
    }
    else if (name == "golden")
    {   // Golden hour: warm haze with a strong sun-facing glow.
        p.strength = 0.75f; p.density = 0.005f; p.start = 18.f; p.maxop = 0.82f;
        p.colorint = 1.1f;  p.suntint = LLColor3(1.f, 0.80f, 0.55f);
        p.sunstr = 0.9f;    p.lobew = 42.f;
    }
    else if (name == "bluehour")
    {   // Blue hour: cool, manual blue distance, no sun glow.
        p.strength = 0.80f; p.density = 0.006f; p.start = 14.f; p.maxop = 0.85f;
        p.colormode = 0;    p.color = LLColor3(0.52f, 0.62f, 0.85f); p.sunstr = 0.f;
    }
    else if (name == "mist")
    {   // Morning mist: low-lying height fog anchored at the camera altitude.
        p.strength = 0.90f; p.density = 0.009f; p.start = 6.f; p.maxop = 0.92f;
        p.height = true;    p.falloff = 0.08f;  p.refFromCam = true;
        p.sunstr = 0.25f;   p.lobew = 40.f;
    }
    else if (name == "valley")
    {   // Valley fog: heavy height-based atmosphere, dense low.
        p.strength = 1.00f; p.density = 0.014f; p.start = 8.f; p.maxop = 0.95f;
        p.height = true;    p.falloff = 0.05f;  p.refFromCam = true;
        p.sunstr = 0.15f;   p.lobew = 40.f;
    }
    else if (name == "dream")
    {   // Dream / ethereal: soft, bright, wide gentle sun bloom.
        p.strength = 0.80f; p.density = 0.004f; p.start = 10.f; p.maxop = 0.95f;
        p.colorint = 1.3f;  p.suntint = LLColor3(1.f, 0.90f, 0.78f);
        p.sunstr = 0.30f;   p.lobew = 60.f;
    }
    else if (name == "noir")
    {   // Noir / cold: desaturated cool distance, no sun.
        p.strength = 0.85f; p.density = 0.007f; p.start = 12.f; p.maxop = 0.85f;
        p.colormode = 0;    p.color = LLColor3(0.60f, 0.63f, 0.68f); p.colorint = 0.9f;
        p.sunstr = 0.f;
    }
    else if (name == "smoke")
    {   // Smoke / interior: short-start neutral haze for enclosed sets.
        p.strength = 0.80f; p.density = 0.011f; p.start = 3.f; p.maxop = 0.82f;
        p.colormode = 0;    p.color = LLColor3(0.52f, 0.52f, 0.55f); p.sunstr = 0.f;
    }
    else
    {
        return; // unknown preset name: leave settings untouched
    }

    set_setting("CineHazeEnable", true);
    set_setting("CineHazeStrength", p.strength);
    set_setting("CineHazeDensity", p.density);
    set_setting("CineHazeStartDistance", p.start);
    set_setting("CineHazeMaxOpacity", p.maxop);
    set_setting("CineHazeColorMode", p.colormode);
    set_setting("CineHazeColor", p.color.getValue());
    set_setting("CineHazeColorIntensity", p.colorint);
    set_setting("CineHazeHeightEnable", p.height);
    set_setting("CineHazeHeightFalloff", p.falloff);
    set_setting("CineHazeSunTint", p.suntint.getValue());
    set_setting("CineHazeSunStrength", p.sunstr);
    set_setting("CineHazeSunLobeWidth", p.lobew);
    set_setting("CineHazeDebugView", 0);
    if (p.refFromCam)
    {
        setReferenceHeightFromCamera();
    }
}

void ALCineHaze::resetToDefaults()
{
    for (const char* name : kCineHazeSettings)
    {
        reset_setting(name);
    }
}

void ALCineHaze::setReferenceHeightFromCamera()
{
    // Agent-space Z == absolute SL altitude in metres (same frame as the shader).
    set_setting("CineHazeReferenceHeight", LLViewerCamera::getInstance()->getOrigin().mV[VZ]);
}
