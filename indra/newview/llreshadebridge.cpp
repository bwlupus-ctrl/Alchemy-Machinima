/**
 * @file llreshadebridge.cpp
 * @brief In-process bridge exposing the viewer G-buffer + camera to ReShade 6.x.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llreshadebridge.h"

#include <cstring>              // memcpy

#include "pipeline.h"          // gPipeline, RenderTargetPack, LLRenderTarget
#include "llviewercamera.h"    // LLViewerCamera (matrices, near/far/fov, world frame)
#include "llviewercontrol.h"   // gSavedSettings (BDMergeReShadeOverrideDepth)

#if LL_RESHADE_ADDON
// Vendored ReShade add-on SDK header (Apache-2.0, indra/newview/reshade). ReShade
// is injected into this process as opengl32.dll; these inline helpers resolve its
// exports at runtime, so nothing is linked and a normal (no-ReShade) launch is
// unaffected - register_addon simply returns false.
// RESHADE_ADDON must be defined before the header to enable the add-on event API
// (reshade_events.hpp gates the event traits behind `#if RESHADE_ADDON`).
#define RESHADE_ADDON 1
#include "reshade.hpp"
#include "llgl.h"              // GL_TEXTURE_2D for the resource-view handle encoding
#endif

// -----------------------------------------------------------------------------
LLReShadeBridge& LLReShadeBridge::instance()
{
    static LLReShadeBridge sInstance;
    return sInstance;
}

// -----------------------------------------------------------------------------
// gatherFrame -- pull this frame's camera + G-buffer state into mFrame.
// Pure CPU reads of already-resident GL handles and camera state; no GPU work.
// -----------------------------------------------------------------------------
void LLReShadeBridge::gatherFrame()
{
#if LL_RESHADE_ADDON
    // Register with ReShade lazily on the first frame: by now the GL context and
    // the injected ReShade runtime both exist. One-shot; stays inert if ReShade
    // isn't present (register_addon returns false).
    static bool sInitAttempted = false;
    if (!sInitAttempted)
    {
        sInitAttempted = true;
        init();
    }
#endif

    LLReShadeFrameData& f = mFrame;
    f = LLReShadeFrameData();   // reset; mValid defaults false

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    if (!cam)
    {
        return;
    }

    // Camera scalars
    f.mNear   = cam->getNear();
    f.mFar    = cam->getFar();
    f.mFovY   = cam->getView();     // vertical FOV, radians
    f.mAspect = cam->getAspect();

    // World-space camera frame (origin + basis) -- lets shaders reconstruct
    // world position from depth without needing inverse matrices marshaled.
    const LLVector3& o    = cam->getOrigin();
    const LLVector3& at   = cam->getAtAxis();
    const LLVector3& left = cam->getLeftAxis();
    const LLVector3& up   = cam->getUpAxis();
    for (U32 i = 0; i < 3; ++i)
    {
        f.mOrigin[i]   = o.mV[i];
        f.mAtAxis[i]   = at.mV[i];
        f.mLeftAxis[i] = left.mV[i];
        f.mUpAxis[i]   = up.mV[i];
    }

    // Matrices -- LLMatrix4::mMatrix is a contiguous 4x4 row-major F32 block.
    const LLMatrix4& view = cam->getModelview();
    const LLMatrix4& proj = cam->getProjection();
    memcpy(f.mView, &view.mMatrix[0][0], 16 * sizeof(F32));
    memcpy(f.mProj, &proj.mMatrix[0][0], 16 * sizeof(F32));

    // G-buffer + scene color handles from the active render target pack.
    if (gPipeline.mRT)
    {
        LLRenderTarget& def = gPipeline.mRT->deferredScreen;
        LLRenderTarget& scr = gPipeline.mRT->screen;

        f.mWidth    = def.getWidth();
        f.mHeight   = def.getHeight();
        // Raw depth handle: used for the mValid check below. When the DEPTH override
        // is on we hand ReShade the R32F COLOR copy instead (set further down), since
        // ReShade can't sample the raw depth-format texture.
        f.mTexDepth = def.getDepth();

        // deferredScreen attachment layout (see addDeferredAttachments):
        //   0 = albedo/diffuse, 1 = ORM (occ/rough/metal), 2 = normals, 3 = emissive(opt)
        const U32 n = def.getNumTextures();
        if (n > 0) f.mTexAlbedo   = def.getTexture(0);
        if (n > 1) f.mTexORM      = def.getTexture(1);
        if (n > 2) f.mTexNormals  = def.getTexture(2);
        if (n > 3) f.mTexEmissive = def.getTexture(3);

        if (scr.getNumTextures() > 0)
        {
            f.mTexColorHDR = scr.getTexture(0);
        }

        // [RTGI Step B] A5.4 velocity buffer (RG16F, NDC-delta). Only allocated when
        // BDMergeVelocityBuffer is on; getWidth()==0 => not produced this session.
        if (gPipeline.mVelocityMap.getWidth() > 0 && gPipeline.mVelocityMap.getNumTextures() > 0)
        {
            f.mTexVelocity = gPipeline.mVelocityMap.getTexture(0);
        }

        // Opt-in DEPTH override (default off) - read here on the main thread and
        // stashed for the ReShade-thread callback. Off => generic_depth owns DEPTH.
        static LLCachedControl<bool> bind_depth(gSavedSettings, "BDMergeReShadeOverrideDepth", false);
        mBindDepth = bind_depth;

        // When overriding, bind the R32F depth COPY (sampleable) instead of the raw
        // depth. copyReShadeDepth() (called just before gatherFrame) fills it.
        if (mBindDepth &&
            gPipeline.mReShadeDepthCopy.getWidth() > 0 &&
            gPipeline.mReShadeDepthCopy.getNumTextures() > 0)
        {
            f.mTexDepth = gPipeline.mReShadeDepthCopy.getTexture(0);
        }

        // Consider the frame usable only if the two effect-critical buffers exist.
        f.mValid = (f.mTexNormals != 0) && (f.mTexDepth != 0);
    }

    // [RTGI Step A] The actual push (binding the real depth to ReShade's DEPTH
    // semantic) happens in on_reshade_begin_effects, which reads this mFrame - so
    // the binding is done on ReShade's own thread at the right point in its frame.
}

// -----------------------------------------------------------------------------
// Add-on lifecycle. No-ops unless LL_RESHADE_ADDON is enabled.
// -----------------------------------------------------------------------------
#if LL_RESHADE_ADDON
// [RTGI Step A] Feed the viewer's REAL depth buffer to ReShade's built-in DEPTH
// semantic, so depth-dependent effects (RTGI/MXAO/DOF/...) stop reconstructing /
// heuristically detecting depth and use the engine's exact buffer.
//
// Bindings MUST be created through ReShade's device via create_resource_view - NOT
// by hand-encoding a handle. This is exactly how the reference generic_depth add-on
// (examples/09-depth) does it, and it is what makes ReShade set up the view's FORMAT
// + shader-resource usage so the effect sampler reads REAL values. A hand-encoded
// handle is not a registered view: ReShade doesn't know its format and the sampler
// reads flat/zero (our earlier depth-flat / motion-grey symptoms). We wrap the raw
// GL texture name as a resource ((target<<40)|object = make_resource_handle), create
// a proper shader-resource view of it, and cache per-semantic so the previous view
// is destroyed and we rebind only when the GL texture name changes (RT realloc).
struct BoundSem { U32 gl_name = 0; reshade::api::resource_view srv = { 0 }; };
static BoundSem sDepthBound;
static BoundSem sVelocityBound;

static void bind_gl_texture(reshade::api::effect_runtime* runtime, const char* semantic,
                            U32 gl_name, reshade::api::format fmt, BoundSem& state)
{
    if (gl_name == state.gl_name)
    {
        return; // same GL texture already bound; its contents flow every frame
    }

    reshade::api::device* dev = runtime->get_device();
    if (dev == nullptr)
    {
        return;
    }

    if (state.srv != 0)
    {
        dev->destroy_resource_view(state.srv);
        state.srv = reshade::api::resource_view{ 0 };
    }

    // ---- Texture-completeness fix: THE root cause of the flat-depth / grey-motion
    // symptom. ReShade samples effect textures through GL SAMPLER OBJECTS whose min
    // filter is ALWAYS a mipmapped mode (reshade-SL create_sampler maps every
    // api::filter_mode to GL_*_MIPMAP_*). When a sampler object is bound, ITS
    // filter - not the texture's - determines completeness (GL 4.6 8.17/8.23.1).
    // Viewer RT textures are mutable (glTexImage2D), level-0-only, with
    // GL_TEXTURE_MAX_LEVEL at the default 1000 => mipmap-INCOMPLETE under ReShade's
    // samplers, and every sample returns constant (0,0,0,1). generic_depth never
    // trips this because it binds textures ReShade itself created via glTexStorage
    // (immutable => always complete). Clamping MAX_LEVEL to the one defined level
    // makes ours complete; the viewer never mip-samples these RTs, so this is
    // side-effect free. We are in-process on the render thread (ReShade fires
    // begin_effects inside SwapBuffers), so direct GL here is safe; restore the
    // binding we disturb.
    GLint prev_tex2d = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex2d);
    glBindTexture(GL_TEXTURE_2D, gl_name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex2d));

    // Raw GL texture object -> ReShade resource, then a proper shader-resource view.
    reshade::api::resource res =
        reshade::api::resource{ (static_cast<uint64_t>(GL_TEXTURE_2D) << 40) | static_cast<uint64_t>(gl_name) };
    reshade::api::resource_view srv = reshade::api::resource_view{ 0 };
    if (dev->create_resource_view(res, reshade::api::resource_usage::shader_resource,
                                  reshade::api::resource_view_desc(fmt), &srv))
    {
        runtime->update_texture_bindings(semantic, srv, srv);
        state.srv     = srv;
        state.gl_name = gl_name;
        LL_INFOS("ReShade") << "[RTGI] bound GL " << gl_name << " (" << (int)fmt
                            << ") -> ReShade '" << semantic << "'" << LL_ENDL;
    }
    else
    {
        LL_WARNS("ReShade") << "[RTGI] create_resource_view failed for GL " << gl_name
                            << " -> '" << semantic << "'" << LL_ENDL;
    }
}

static void on_reshade_begin_effects(reshade::api::effect_runtime* runtime,
                                     reshade::api::command_list* /*cmd*/,
                                     reshade::api::resource_view /*rtv*/,
                                     reshade::api::resource_view /*rtv_srgb*/)
{
    const LLReShadeFrameData& f = LLReShadeBridge::instance().getFrameData();
    if (!f.mValid)
    {
        return;
    }

    // DEPTH override (opt-in): when on, f.mTexDepth is the R32F COLOR copy that
    // copyReShadeDepth() produced (ReShade can't sample a raw depth-format texture).
    // The copy is R32F -> format::r32_float.
    if (LLReShadeBridge::instance().bindDepth() && f.mTexDepth != 0)
    {
        bind_gl_texture(runtime, "DEPTH", f.mTexDepth, reshade::api::format::r32_float, sDepthBound);
    }

    // [RTGI Step B] A5.4 velocity buffer (RG16F) -> custom SL_MOTION_NDC semantic,
    // which SL_GBufferProvider.fx transcodes into iMMERSE's Deferred::MotionVectorsTex.
    if (f.mTexVelocity != 0)
    {
        bind_gl_texture(runtime, "SL_MOTION_NDC", f.mTexVelocity, reshade::api::format::r16g16_float, sVelocityBound);
    }
}

static void on_destroy_effect_runtime(reshade::api::effect_runtime* /*runtime*/)
{
    // Runtime (re)created (device reset) -> our views are gone with it. Reset state
    // (no destroy - the device/views are already torn down) so we recreate next frame.
    sDepthBound    = BoundSem{};
    sVelocityBound = BoundSem{};
}
#endif // LL_RESHADE_ADDON

void LLReShadeBridge::init()
{
#if LL_RESHADE_ADDON
    // ReShade is injected as opengl32.dll into THIS process, so register the
    // viewer's own module as an in-process add-on. Returns false (harmlessly) when
    // no ReShade runtime is present - a normal launch is entirely unaffected.
    if (!reshade::register_addon(GetModuleHandle(nullptr)))
    {
        mEnabled = false;
        LL_INFOS("ReShade") << "[RTGI] no ReShade runtime present; bridge inert" << LL_ENDL;
        return;
    }

    reshade::register_event<reshade::addon_event::reshade_begin_effects>(&on_reshade_begin_effects);
    reshade::register_event<reshade::addon_event::destroy_effect_runtime>(&on_destroy_effect_runtime);
    sDepthBound    = BoundSem{};
    sVelocityBound = BoundSem{};
    mEnabled = true;
    LL_INFOS("ReShade") << "[RTGI] add-on registered (create_resource_view bindings)" << LL_ENDL;
#else
    mEnabled = false;
#endif
}

void LLReShadeBridge::shutdown()
{
#if LL_RESHADE_ADDON
    if (mEnabled)
    {
        reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(&on_reshade_begin_effects);
        reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(&on_destroy_effect_runtime);
        reshade::unregister_addon(GetModuleHandle(nullptr));
    }
#endif
    mEnabled = false;
}
