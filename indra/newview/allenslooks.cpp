/**
 * @file allenslooks.cpp
 * @brief Lens Looks frame resolver, sanitisation and action presets.
 */
#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"
#include "llcombobox.h"

#include "allenslooks.h"

#include "alenvintensity.h"
#include "lldirectorcast.h"
#include "lldrawpool.h"
#include "lljoint.h"
#include "llpresentationtime.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llglslshader.h"
#include "llviewershadermgr.h"
#include "llvoavatar.h"
#include "pipeline.h"

#include <cmath>

extern U32 gFrameCount;
extern bool gSnapshot;

namespace
{
F32 finite_or(F32 v, F32 fallback) { return std::isfinite(v) ? v : fallback; }
F32 sane(F32 v, F32 fallback, F32 lo, F32 hi)
{
    return llclamp(finite_or(v, fallback), lo, hi);
}
LLColor3 sane_color(LLColor3 value, const LLColor3& fallback)
{
    for (S32 i = 0; i < 3; ++i)
    {
        value.mV[i] = sane(value.mV[i], fallback.mV[i], 0.f, 1.f);
    }
    return value;
}

struct IrPreset
{
    const char* name;
    S32 mode;
    F32 strength;
    F32 foliage_ev;
    F32 sky_darken;
    F32 water_darken;
    F32 red_leak;
    F32 halation;
    S32 aero_filter;
    F32 aero_saturation;
    bool grain_hint;
};

const IrPreset IR_PRESETS[] = {
    {.name="Wood 720nm Classic", .mode=0, .strength=1.f,  .foliage_ev=.5f, .sky_darken=2.5f, .water_darken=2.f, .red_leak=.15f, .halation=.3f, .aero_filter=0, .aero_saturation=1.15f, .grain_hint=false},
    {.name="Kodak HIE",          .mode=0, .strength=1.f,  .foliage_ev=1.f,  .sky_darken=3.f,   .water_darken=2.f, .red_leak=.05f, .halation=.6f, .aero_filter=0, .aero_saturation=1.15f, .grain_hint=true},
    {.name="Subtle 665nm",       .mode=0, .strength=.85f, .foliage_ev=0.f,  .sky_darken=1.2f, .water_darken=2.f, .red_leak=.35f, .halation=.1f, .aero_filter=0, .aero_saturation=1.15f, .grain_hint=false},
    {.name="Deep 850nm",         .mode=0, .strength=1.f,  .foliage_ev=1.2f, .sky_darken=4.f,   .water_darken=3.f, .red_leak=0.f,   .halation=.2f, .aero_filter=0, .aero_saturation=1.15f, .grain_hint=false},
    {.name="Aerochrome #12",     .mode=1, .strength=1.f,  .foliage_ev=0.f,  .sky_darken=2.f,   .water_darken=2.f, .red_leak=.1f,  .halation=.25f,.aero_filter=0, .aero_saturation=1.15f, .grain_hint=false},
    {.name="Aerochrome Hot Pink #21",.mode=1,.strength=1.f,.foliage_ev=0.f,.sky_darken=2.f,   .water_darken=2.f, .red_leak=.1f,  .halation=.25f,.aero_filter=1, .aero_saturation=1.3f, .grain_hint=false},
    {.name="Aerochrome Red #25", .mode=1, .strength=1.f,  .foliage_ev=0.f,  .sky_darken=2.f,   .water_darken=2.f, .red_leak=.1f,  .halation=.25f,.aero_filter=2, .aero_saturation=1.1f, .grain_hint=false},
};

struct SoftPreset
{
    const char* name;
    S32 shape;
    F32 size;
    F32 aspect;
    F32 angle;
    F32 feather;
    F32 blur;
    F32 glow;
    F32 desaturate;
    LLColor3 tint;
    F32 tint_amount;
};

const SoftPreset SOFT_PRESETS[] = {
    {.name="Vaseline Oval", .shape=1, .size=.35f, .aspect=1.4f, .angle=0.f, .feather=.5f, .blur=.6f, .glow=.25f, .desaturate=0.f, .tint=LLColor3(1.f,.92f,.8f), .tint_amount=.3f},
    {.name="Lensbaby Sweet", .shape=0, .size=.22f, .aspect=1.f, .angle=0.f, .feather=.35f, .blur=.9f, .glow=.1f, .desaturate=0.f, .tint=LLColor3(1.f,.92f,.8f), .tint_amount=0.f},
    {.name="Dream Sequence", .shape=1, .size=.3f, .aspect=1.4f, .angle=0.f, .feather=.8f, .blur=.5f, .glow=.6f, .desaturate=.3f, .tint=LLColor3(1.f,.92f,.8f), .tint_amount=.5f},
    {.name="Faux Tilt Band", .shape=2, .size=.18f, .aspect=1.f, .angle=0.f, .feather=.4f, .blur=.8f, .glow=0.f, .desaturate=0.f, .tint=LLColor3(1.f,.92f,.8f), .tint_amount=0.f},
    {.name="Vintage Portrait", .shape=1, .size=.4f, .aspect=1.4f, .angle=0.f, .feather=.5f, .blur=.4f, .glow=.15f, .desaturate=.2f, .tint=LLColor3(1.f,.92f,.8f), .tint_amount=.4f},
    {.name="Nightmare Edge", .shape=0, .size=.28f, .aspect=1.f, .angle=0.f, .feather=.25f, .blur=1.f, .glow=.8f, .desaturate=.8f, .tint=LLColor3(.6f,.7f,1.f), .tint_amount=.6f},
};

ALLensLooks::Frame sFrame;
}

ALLensLooks::InfraredParams ALLensLooks::infrared()
{
    InfraredParams p;
    static LLCachedControl<bool> enabled(gSavedSettings, "RenderInfraredEnabled", false);
    p.enabled = enabled() && !gOnLensNoIR;
    if (!p.enabled)
    {
        return p;
    }
    static LLCachedControl<U32> mode(gSavedSettings, "RenderInfraredMode", 0);
    static LLCachedControl<F32> strength(gSavedSettings, "RenderInfraredStrength", 1.f);
    static LLCachedControl<F32> exposure_ev(gSavedSettings, "RenderInfraredExposureEV", 0.f);
    static LLCachedControl<F32> foliage_ev(gSavedSettings, "RenderInfraredFoliageEV", 0.f);
    static LLCachedControl<F32> sensitivity(gSavedSettings, "RenderInfraredFoliageSensitivity", .5f);
    static LLCachedControl<F32> dry(gSavedSettings, "RenderInfraredDryVegetation", .4f);
    static LLCachedControl<F32> sky_darken(gSavedSettings, "RenderInfraredSkyDarken", 2.f);
    static LLCachedControl<F32> water_darken(gSavedSettings, "RenderInfraredWaterDarken", 2.f);
    static LLCachedControl<F32> skin_ev(gSavedSettings, "RenderInfraredSkinEV", .5f);
    static LLCachedControl<F32> shadow_depth(gSavedSettings, "RenderInfraredShadowDepth", .4f);
    static LLCachedControl<F32> red_leak(gSavedSettings, "RenderInfraredRedLeak", .1f);
    static LLCachedControl<F32> halation(gSavedSettings, "RenderInfraredHalation", .25f);
    static LLCachedControl<U32> aero_filter(gSavedSettings, "RenderInfraredAeroFilter", 0);
    static LLCachedControl<F32> aero_saturation(gSavedSettings, "RenderInfraredAeroSaturation", 1.15f);
    static LLCachedControl<U32> debug_view(gSavedSettings, "RenderInfraredDebugView", 0);
    p.mode = (S32)llmin((U32)mode, 1u);
    p.strength = sane(strength, 1.f, 0.f, 1.f);
    p.exposure_gain = exp2f(sane(exposure_ev, 0.f, -3.f, 3.f));
    p.foliage_gain = exp2f(sane(foliage_ev, 0.f, -2.f, 3.f));
    p.sensitivity = sane(sensitivity, .5f, 0.f, 1.f);
    p.dry = sane(dry, .4f, 0.f, 1.f);
    p.sky_darken = sane(sky_darken, 2.f, 0.f, 6.f);
    p.water_darken = sane(water_darken, 2.f, 0.f, 6.f);
    p.skin_gain = exp2f(sane(skin_ev, .5f, -2.f, 2.f));
    p.shadow_depth = sane(shadow_depth, .4f, 0.f, 1.f);
    p.red_leak = sane(red_leak, .1f, 0.f, 1.f);
    p.halation = sane(halation, .25f, 0.f, 1.f);
    static const F32 FILTERS[4][2] = {{.03f,0.f},{.03f,.35f},{.03f,.8f},{.6f,0.f}};
    const U32 filter = llmin((U32)aero_filter, 3u);
    p.aero_b = FILTERS[filter][0]; p.aero_g = FILTERS[filter][1];
    p.aero_saturation = sane(aero_saturation, 1.15f, 0.f, 2.f);
    p.debug_view = (S32)llmin((U32)debug_view, 3u);
    return p;
}

ALLensLooks::SoftEdgeParams ALLensLooks::softEdge()
{
    SoftEdgeParams p;
    static LLCachedControl<bool> enabled(gSavedSettings, "RenderSoftEdgeEnabled", false);
    p.enabled = enabled();
    if (!p.enabled)
    {
        return p;
    }
    static LLCachedControl<U32> blur_type(gSavedSettings, "RenderSoftEdgeBlurType", 0);
    static LLCachedControl<U32> shape(gSavedSettings, "RenderSoftEdgeShape", 1);
    static LLCachedControl<U32> anchor(gSavedSettings, "RenderSoftEdgeAnchor", 0);
    static LLCachedControl<F32> center_x(gSavedSettings, "RenderSoftEdgeCenterX", .5f);
    static LLCachedControl<F32> center_y(gSavedSettings, "RenderSoftEdgeCenterY", .5f);
    static LLCachedControl<F32> size(gSavedSettings, "RenderSoftEdgeSize", .35f);
    static LLCachedControl<F32> aspect(gSavedSettings, "RenderSoftEdgeAspect", 1.4f);
    static LLCachedControl<F32> angle(gSavedSettings, "RenderSoftEdgeAngle", 0.f);
    static LLCachedControl<F32> feather(gSavedSettings, "RenderSoftEdgeFeather", .4f);
    static LLCachedControl<F32> blur(gSavedSettings, "RenderSoftEdgeBlur", .5f);
    static LLCachedControl<F32> glow(gSavedSettings, "RenderSoftEdgeGlow", .2f);
    static LLCachedControl<F32> glow_threshold(gSavedSettings, "RenderSoftEdgeGlowThreshold", .8f);
    static LLCachedControl<F32> desaturate(gSavedSettings, "RenderSoftEdgeDesaturate", 0.f);
    static LLCachedControl<LLColor3> tint(gSavedSettings, "RenderSoftEdgeTint", LLColor3(1.f,.92f,.8f));
    static LLCachedControl<F32> tint_amount(gSavedSettings, "RenderSoftEdgeTintAmount", 0.f);
    static LLCachedControl<bool> show_mask(gSavedSettings, "RenderSoftEdgeShowMask", false);
    p.blur_type = (S32)llmin((U32)blur_type, 1u);
    p.shape = (S32)llmin((U32)shape, 2u);
    p.anchor = (S32)llmin((U32)anchor, 2u);
    p.center.set(sane(center_x, .5f, 0.f, 1.f), sane(center_y, .5f, 0.f, 1.f));
    p.size=sane(size,.35f,.05f,1.5f); p.aspect=sane(aspect,1.4f,.3f,3.f);
    p.angle_rad=sane(angle,0.f,-90.f,90.f)*DEG_TO_RAD;
    p.feather=sane(feather,.4f,.02f,2.f); p.blur=sane(blur,.5f,0.f,1.f);
    p.glow=sane(glow,.2f,0.f,2.f); p.glow_threshold=sane(glow_threshold,.8f,.1f,4.f);
    p.desaturate=sane(desaturate,0.f,0.f,1.f); p.tint=sane_color(tint,LLColor3(1.f,.92f,.8f));
    p.tint_amount=sane(tint_amount,0.f,0.f,1.f); p.show_mask=show_mask();
    return p;
}

void ALLensLooks::resolveFrame()
{
    if (gPipeline.mRT != &gPipeline.mMainRT) return;
    static U32 last = ~0u; if (last == gFrameCount) return; last = gFrameCount;
    const InfraredParams ir = infrared(); const SoftEdgeParams se = softEdge();
    sFrame.mInfraredActive = ir.enabled && ir.strength > 0.f;
    sFrame.mSoftEdgeActive = se.enabled;
    sFrame.mTagUsable = LLViewerShaderMgr::gbufferAvatarTagActive();
    LLVector2 target = se.center;
    const auto project = [&target](const LLVector3& position)
    {
        const glm::vec4 view = glm::make_mat4(gGLLastModelView) *
            glm::vec4(position.mV[VX], position.mV[VY], position.mV[VZ], 1.f);
        const glm::vec4 clip = glm::make_mat4(gGLLastProjection) * view;
        if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
            !std::isfinite(clip.w) || clip.w <= 1.e-4f) return;
        const F32 x = clip.x / clip.w;
        const F32 y = clip.y / clip.w;
        LLVector2 uv(x * .5f + .5f, y * .5f + .5f);
        // gGLLastProjection is tile-local during a high-resolution snapshot,
        // while lens_softedge5 evaluates the mask in full-frame UV.  Promote
        // the projected point to that same space so every tile shares one
        // anchor (the pyramid itself intentionally remains tile-local).
        const LLVector2 frame = LLRenderPass::actorFxScreenSize();
        const LLVector2 offset = LLRenderPass::actorFxFragOffset();
        if (gViewerWindow && (offset.mV[VX] != 0.f || offset.mV[VY] != 0.f ||
                              frame.mV[VX] != gViewerWindow->getWorldViewWidthRaw() ||
                              frame.mV[VY] != gViewerWindow->getWorldViewHeightRaw()))
        {
            const LLVector2 tile((F32)gViewerWindow->getWorldViewWidthRaw(),
                                 (F32)gViewerWindow->getWorldViewHeightRaw());
            uv.set((offset.mV[VX] + uv.mV[VX] * tile.mV[VX]) / frame.mV[VX],
                   (offset.mV[VY] + uv.mV[VY] * tile.mV[VY]) / frame.mV[VY]);
        }
        if (uv.mV[VX] < 0.f || uv.mV[VX] > 1.f ||
            uv.mV[VY] < 0.f || uv.mV[VY] > 1.f) return;
        target = uv;
    };
    if (se.anchor == 1 && LLPipeline::sDoFEnabled && !LLPipeline::sLastFocusPoint.isExactlyZero())
        project(LLPipeline::sLastFocusPoint);
    else if (se.anchor == 2)
        if (LLVOAvatar* av = LLDirectorCast::instance().resolveSubjectA()) if (LLJoint* head = av->getJoint("mHead")) project(head->getWorldPosition());
    const F64 now=LLPresentationTime::currentFrame().presentation_time; static F64 then=-1.;
    const F64 dt=(then<0.||!std::isfinite(then)||!std::isfinite(now)||now<then)?-1.:(now-then);
    const F32 a=(gSnapshot || dt<0.)?1.f:1.f-expf(-(F32)dt/.15f); then=now;
    sFrame.mCenter += (target-sFrame.mCenter)*a;
    const F32 goal=(sFrame.mInfraredActive || (se.enabled && se.glow>0.f))?0.f:1.f;
    const F32 b=dt<0.?1.f:1.f-expf(-(F32)dt/.4f);
    sFrame.mBloomScale += (goal-sFrame.mBloomScale)*b;
    if (fabsf(goal-sFrame.mBloomScale)<1.e-5f) sFrame.mBloomScale=goal;
}

const ALLensLooks::Frame& ALLensLooks::frame() { return sFrame; }

LLColor3 ALLensLooks::gradTintLinear()
{
    static LLCachedControl<LLColor3> tint(gSavedSettings, "RenderGradNDTintColor", LLColor3(1.f,1.f,1.f));
    LLColor3 c = linearColor3(sane_color(tint, LLColor3(1.f,1.f,1.f)));
    const F32 l = ALEnvIntensity::linearLuminance(c); return l > 1.e-5f ? c * (1.f / l) : LLColor3(1.f,1.f,1.f);
}

LLColor3 ALLensLooks::frameFilterGain()
{
    static const F32 MIRED[] = {0.f,112.f,131.f,81.f,18.f,-131.f,-112.f,-81.f,-18.f};
    static LLCachedControl<U32> preset_setting(gSavedSettings, "RenderLensFilterPreset", 0);
    static LLCachedControl<F32> mired_setting(gSavedSettings, "RenderLensFilterMired", 0.f);
    const U32 preset=llmin((U32)preset_setting,8u);
    const F32 shift=preset?MIRED[preset]:sane(mired_setting,0.f,-200.f,200.f);
    const F32 kelvin=1.e6f/(1.e6f/5500.f+shift); LLColor3 c=ALEnvIntensity::kelvinRatio(kelvin);
    const F32 l=ALEnvIntensity::linearLuminance(c); return l>1.e-5f? c * (1.f / l) : LLColor3(1.f,1.f,1.f);
}

void ALLensLooks::applyInfraredPreset(S32 index)
{
    if (ALPresetLook::replaying()) return;
    if (index<1 || index>(S32)LL_ARRAY_SIZE(IR_PRESETS)) return; const IrPreset& p=IR_PRESETS[index-1];
    ALPresetLook::Load load;
    ALPresetLook::reset(ALPresetLook::owned(ALPresetLook::INFRARED));
    if (gSavedSettings.getBOOL("RenderInfraredPresetSetsGrain"))
        ALPresetLook::reset(ALPresetLook::infraredGrainSettings());
    gSavedSettings.setU32("RenderInfraredMode",(U32)p.mode); gSavedSettings.setF32("RenderInfraredStrength",p.strength); gSavedSettings.setF32("RenderInfraredFoliageEV",p.foliage_ev);
    gSavedSettings.setF32("RenderInfraredSkyDarken",p.sky_darken); gSavedSettings.setF32("RenderInfraredWaterDarken",p.water_darken); gSavedSettings.setF32("RenderInfraredRedLeak",p.red_leak);
    gSavedSettings.setF32("RenderInfraredHalation",p.halation); gSavedSettings.setU32("RenderInfraredAeroFilter",(U32)p.aero_filter); gSavedSettings.setF32("RenderInfraredAeroSaturation",p.aero_saturation);
    if (p.grain_hint && gSavedSettings.getBOOL("RenderInfraredPresetSetsGrain"))
    {
        gSavedSettings.setF32("RenderFilmGrainAmount", .2f);
    }
}

void ALLensLooks::applySoftEdgePreset(S32 index)
{
    if (ALPresetLook::replaying()) return;
    if(index<1 || index>(S32)LL_ARRAY_SIZE(SOFT_PRESETS)) return; const SoftPreset& p=SOFT_PRESETS[index-1];
    ALPresetLook::Load load;
    ALPresetLook::reset(ALPresetLook::owned(ALPresetLook::SOFT_EDGE));
    gSavedSettings.setU32("RenderSoftEdgeShape",(U32)p.shape); gSavedSettings.setF32("RenderSoftEdgeSize",p.size); gSavedSettings.setF32("RenderSoftEdgeAspect",p.aspect); gSavedSettings.setF32("RenderSoftEdgeAngle",p.angle);
    gSavedSettings.setF32("RenderSoftEdgeFeather",p.feather); gSavedSettings.setF32("RenderSoftEdgeBlur",p.blur); gSavedSettings.setF32("RenderSoftEdgeGlow",p.glow); gSavedSettings.setF32("RenderSoftEdgeDesaturate",p.desaturate);
    gSavedSettings.setUntypedValue("RenderSoftEdgeTint",p.tint.getValue()); gSavedSettings.setF32("RenderSoftEdgeTintAmount",p.tint_amount);
}

void ALLensLooks::bindFilterPresets(LLView* root)
{
    struct Binding { const char* widget; const char* setting; S32 count; };
    static const Binding bindings[] = {
        {"gradnd_preset_combo", "RenderGradNDPreset", 6},
        {"polarizer_preset_combo", "RenderPolarizerPreset", 4},
        {"lens_filter_preset", "RenderLensFilterPreset", 8}
    };
    for (const Binding& binding : bindings)
        if (LLComboBox* combo = root->findChild<LLComboBox>(binding.widget))
            combo->setCommitCallback([binding](LLUICtrl* control, const LLSD&)
            {
                const S32 index = control->getValue().asInteger();
                if (index < 1 || index > binding.count || ALPresetLook::replaying()) return;
                ALPresetLook::Load load;
                // These presets resolve their artistic values in the renderer.
                // Preserve filter amount and every Custom-mode slider.
                gSavedSettings.setU32(binding.setting, index);
            });
}

const std::vector<std::string>& ALLensLooks::settings()
{
    static const std::vector<std::string> keys={"RenderInfraredEnabled","RenderInfraredMode","RenderInfraredStrength","RenderInfraredExposureEV","RenderInfraredFoliageEV","RenderInfraredFoliageSensitivity","RenderInfraredDryVegetation","RenderInfraredSkyDarken","RenderInfraredWaterDarken","RenderInfraredSkinEV","RenderInfraredShadowDepth","RenderInfraredRedLeak","RenderInfraredHalation","RenderInfraredAeroFilter","RenderInfraredAeroSaturation","RenderInfraredDebugView","RenderInfraredPresetSetsGrain","RenderSoftEdgeEnabled","RenderSoftEdgeBlurType","RenderSoftEdgeShape","RenderSoftEdgeAnchor","RenderSoftEdgeCenterX","RenderSoftEdgeCenterY","RenderSoftEdgeSize","RenderSoftEdgeAspect","RenderSoftEdgeAngle","RenderSoftEdgeFeather","RenderSoftEdgeBlur","RenderSoftEdgeGlow","RenderSoftEdgeGlowThreshold","RenderSoftEdgeDesaturate","RenderSoftEdgeTint","RenderSoftEdgeTintAmount","RenderSoftEdgeShowMask","RenderGradNDTintColor","RenderGradNDTintAmount","RenderLensFilterPreset","RenderLensFilterMired","RenderLensFilterAmount"};
    return keys;
}
