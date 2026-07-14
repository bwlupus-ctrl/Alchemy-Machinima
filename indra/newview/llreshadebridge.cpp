/**
 * @file llreshadebridge.cpp
 * @brief Viewer side of the ReShade bridge: fills and publishes the
 *        SLReShadeFrame ABI struct consumed by the sl_reshade_bridge add-on.
 *
 * This is the alchemy-machinima port. It is identical to the phoenix-reshade-XL
 * copy except that it also publishes the A5.4 velocity buffer (mVelocityMap)
 * into f.motion -- alchemy has it, so temporal effects (RTGI/motion-blur/T2x)
 * get real motion vectors. The single shared contract llreshadebridgeabi.h and
 * the sl_reshade_bridge.addon binary are byte-for-byte interchangeable between
 * the two viewer forks.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llreshadebridge.h"

#include <atomic>
#include <cstring>              // memcpy

#include "pipeline.h"          // gPipeline, RenderTargetPack, LLRenderTarget, mVelocityMap
#include "llviewercamera.h"    // LLViewerCamera (matrices, near/far/fov, world frame)

extern bool gSnapshot;         // llviewerdisplay.cpp

// -----------------------------------------------------------------------------
LLReShadeBridge& LLReShadeBridge::instance()
{
    static LLReShadeBridge sInstance;
    return sInstance;
}

LLReShadeBridge::LLReShadeBridge()
{
    // Zero, then stamp the immutable header fields once. write_begin/write_end
    // start equal (0) == consistent-but-invalid frame.
    memset(&mFrame, 0, sizeof(mFrame));
    mFrame.magic       = SLRESHADE_ABI_MAGIC;
    mFrame.abi_version = SLRESHADE_ABI_VERSION;
    mFrame.struct_size = (U32)sizeof(SLReShadeFrame);
}

// -----------------------------------------------------------------------------
// The one exported symbol the add-on resolves. Exporting from the executable
// works like exporting from a DLL: the add-on does
//   GetProcAddress(GetModuleHandleW(NULL), SLRESHADE_EXPORT_NAME).
// Returns NULL on ABI major mismatch so an old/new add-on fails soft.
// -----------------------------------------------------------------------------
extern "C"
#if LL_WINDOWS
__declspec(dllexport)
#endif
const SLReShadeFrame* SLReShade_GetFrame(uint32_t requested_abi)
{
    if ((requested_abi >> 16) != SLRESHADE_ABI_MAJOR)
    {
        return nullptr;
    }
    return &LLReShadeBridge::instance().getFrameData();
}

// -----------------------------------------------------------------------------
// helpers
// -----------------------------------------------------------------------------
static void fill_texture(SLReShadeTexture& out, const LLRenderTarget& rt, U32 attachment)
{
    out.gl_name            = rt.getTexture(attachment);
    out.gl_internal_format = rt.getInternalFormat(attachment);
    out.width              = rt.getWidth();
    out.height             = rt.getHeight();
}

// -----------------------------------------------------------------------------
// gatherFrame -- pull this frame's camera + G-buffer state into the published
// struct. Pure CPU reads of already-resident GL handles and camera state; no
// GPU work. Seqlock-wrapped so a reader on another thread can detect tearing.
// -----------------------------------------------------------------------------
void LLReShadeBridge::gatherFrame()
{
    SLReShadeFrame& f = mFrame;

    // --- seqlock: open the write window ---
    f.write_begin++;
    std::atomic_thread_fence(std::memory_order_release);

    // Reset payload (header + seqlock fields are preserved).
    f.flags = 0;
    f.color_hdr = SLReShadeTexture();
    f.depth     = SLReShadeTexture();
    f.albedo    = SLReShadeTexture();
    f.orm       = SLReShadeTexture();
    f.normals   = SLReShadeTexture();
    f.emissive  = SLReShadeTexture();
    f.motion    = SLReShadeTexture();

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    if (cam)
    {
        // Camera scalars
        f.near_clip = cam->getNear();
        f.far_clip  = cam->getFar();
        f.fov_y     = cam->getView();     // vertical FOV, radians
        f.aspect    = cam->getAspect();

        // World-space camera frame (origin + basis) -- lets shaders reconstruct
        // world position from depth without needing inverse matrices marshaled.
        const LLVector3& o    = cam->getOrigin();
        const LLVector3& at   = cam->getAtAxis();
        const LLVector3& left = cam->getLeftAxis();
        const LLVector3& up   = cam->getUpAxis();
        for (U32 i = 0; i < 3; ++i)
        {
            f.origin[i]    = o.mV[i];
            f.at_axis[i]   = at.mV[i];
            f.left_axis[i] = left.mV[i];
            f.up_axis[i]   = up.mV[i];
        }

        // Matrices -- LLMatrix4::mMatrix is a contiguous 4x4 row-major F32 block.
        const LLMatrix4& view = cam->getModelview();
        const LLMatrix4& proj = cam->getProjection();
        memcpy(f.view, &view.mMatrix[0][0], 16 * sizeof(F32));
        memcpy(f.proj, &proj.mMatrix[0][0], 16 * sizeof(F32));
    }

    // G-buffer + scene color handles from the active render target pack.
    if (cam && gPipeline.mRT)
    {
        LLRenderTarget& def = gPipeline.mRT->deferredScreen;
        LLRenderTarget& scr = gPipeline.mRT->screen;

        f.width  = def.getWidth();
        f.height = def.getHeight();

        // Depth is a renderbuffer-style texture allocated as GL_DEPTH_COMPONENT24
        // (llrendertarget.cpp), shared between deferredScreen and screen.
        // Published for diagnostics; effects should take depth from ReShade's
        // generic_depth add-on, not from here.
        f.depth.gl_name            = def.getDepth();
        f.depth.gl_internal_format = 0x81A6; // GL_DEPTH_COMPONENT24
        f.depth.width              = def.getWidth();
        f.depth.height             = def.getHeight();

        // deferredScreen attachment layout (see addDeferredAttachments):
        //   0 = albedo/diffuse, 1 = ORM (occ/rough/metal), 2 = normals, 3 = emissive(opt)
        const U32 n = def.getNumTextures();
        if (n > 0) fill_texture(f.albedo,   def, 0);
        if (n > 1) fill_texture(f.orm,      def, 1);
        if (n > 2) fill_texture(f.normals,  def, 2);
        if (n > 3) fill_texture(f.emissive, def, 3);

        if (scr.getNumTextures() > 0)
        {
            fill_texture(f.color_hdr, scr, 0);
        }

        // [BDMerge A5.4] velocity buffer -> f.motion. Only allocated when
        // BDMergeVelocityBuffer is on; getWidth()==0 => not produced this
        // session, and f.motion stays zero (addon leaves SL_MOTION_NDC unbound).
        // Format GL_RG16F (0x822F), screen-space NDC delta.
        if (gPipeline.mVelocityMap.getWidth() > 0 &&
            gPipeline.mVelocityMap.getNumTextures() > 0)
        {
            fill_texture(f.motion, gPipeline.mVelocityMap, 0);
        }

        // HDR pipeline allocates normals as GL_RGBA16 (0x805B); the non-HDR
        // fallback uses GL_RGB10_A2. Report which one the add-on will see.
        if (f.normals.gl_internal_format == 0x805B /*GL_RGBA16*/)
        {
            f.flags |= SLRESHADE_FLAG_HDR;
        }
        if (gSnapshot)
        {
            f.flags |= SLRESHADE_FLAG_SNAPSHOT;
        }

        // Usable frame == the effect-critical buffers exist.
        if (f.normals.gl_name != 0 && f.depth.gl_name != 0)
        {
            f.flags |= SLRESHADE_FLAG_VALID;
        }
    }

    f.frame_counter++;

    // --- seqlock: close the write window ---
    std::atomic_thread_fence(std::memory_order_release);
    f.write_end = f.write_begin;
}
