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
// A raw GL texture name becomes a ReShade resource_view via the OpenGL backend's
// handle encoding: (GLenum target << 40) | object (confirmed against reshade-SL's
// opengl_impl_type_convert). The GL name is STABLE across frames, so binding it
// once lets its contents flow every frame - re-binding per frame is expensive and
// unnecessary. We therefore (re)bind only when the depth texture name changes
// (i.e. the render targets were (re)allocated on a resolution change).
static U32 sBoundDepthName = 0;

static reshade::api::resource_view gl_tex_srv(U32 gl_name)
{
    return reshade::api::resource_view{ (static_cast<uint64_t>(GL_TEXTURE_2D) << 40)
                                        | static_cast<uint64_t>(gl_name) };
}

static void on_reshade_begin_effects(reshade::api::effect_runtime* runtime,
                                     reshade::api::command_list* /*cmd*/,
                                     reshade::api::resource_view /*rtv*/,
                                     reshade::api::resource_view /*rtv_srgb*/)
{
    const LLReShadeFrameData& f = LLReShadeBridge::instance().getFrameData();
    if (!f.mValid || f.mTexDepth == 0 || f.mTexDepth == sBoundDepthName)
    {
        return;
    }
    sBoundDepthName = f.mTexDepth;

    reshade::api::resource_view srv = gl_tex_srv(f.mTexDepth);
    runtime->update_texture_bindings("DEPTH", srv, srv);

    LL_INFOS("ReShade") << "[RTGI Step A] bound viewer depth (GL " << f.mTexDepth
                        << ") to ReShade DEPTH semantic" << LL_ENDL;
}

static void on_destroy_effect_runtime(reshade::api::effect_runtime* /*runtime*/)
{
    // Runtime (re)created (e.g. device reset) -> force a re-bind next frame.
    sBoundDepthName = 0;
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
    sBoundDepthName = 0;
    mEnabled = true;
    LL_INFOS("ReShade") << "[RTGI Step A] add-on registered; will feed real depth to ReShade" << LL_ENDL;
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
