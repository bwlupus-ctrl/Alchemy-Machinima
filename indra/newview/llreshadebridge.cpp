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
#include "llviewercontrol.h"   // gSavedSettings / LLCachedControl (sidecar gate)

extern bool gSnapshot;         // llviewerdisplay.cpp

// -----------------------------------------------------------------------------
LLReShadeBridge& LLReShadeBridge::instance()
{
    static LLReShadeBridge sInstance;
    return sInstance;
}

LLReShadeBridge::LLReShadeBridge()
:   mRenderTargetGeneration(0),
    mPendingMotionCoverage(0),
    mPendingVisibleDiffuseSeeded(false),
    mPendingVisibleDiffuseResolved(false),
    mPendingResetFlags(0),
    mResetEventCounter(0),
    mEverHadValidFrame(false),
    mLastFrameValid(false),
    mLastHDR(false),
    mLastSnapshot(false)
{
    // Zero, then stamp the immutable header fields once. write_begin/write_end
    // start equal (0) == consistent-but-invalid frame.
    memset(&mFrame, 0, sizeof(mFrame));
    mFrame.magic       = SLRESHADE_ABI_MAGIC;
    mFrame.abi_version = SLRESHADE_ABI_VERSION;
    mFrame.struct_size = (U32)sizeof(SLReShadeFrame);
    memset(mLastTextures, 0, sizeof(mLastTextures));
}

void LLReShadeBridge::noteMotionCoverage(U32 bits)
{
    mPendingMotionCoverage |= bits;
}

void LLReShadeBridge::noteProjectionChange()
{
    noteResetEvent(SLRESHADE_RESET_PROJECTION_CHANGE);
}

void LLReShadeBridge::noteVisibleDiffuseSeeded()
{
    mPendingVisibleDiffuseSeeded = true;
}

void LLReShadeBridge::noteVisibleDiffuseResolved()
{
    // Completion of the forward loop cannot manufacture readiness on its own:
    // if the seed pass did not run, deferred-opaque pixels were never
    // classified and the exactness channel is meaningless there.
    mPendingVisibleDiffuseResolved = mPendingVisibleDiffuseSeeded;
}

void LLReShadeBridge::noteResetEvent(U32 flags)
{
    mPendingResetFlags |= flags;
    ++mResetEventCounter;
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

static bool texture_present(const SLReShadeTexture& texture)
{
    return texture.gl_name != 0 && texture.width != 0 && texture.height != 0;
}

static bool texture_equal(const SLReShadeTexture& lhs, const SLReShadeTexture& rhs)
{
    return lhs.gl_name == rhs.gl_name &&
           lhs.gl_internal_format == rhs.gl_internal_format &&
           lhs.width == rhs.width &&
           lhs.height == rhs.height;
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
    SLReShadeFrameV11Tail tail = {};

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
        // LLCachedControl, not getBOOL: gatherFrame() runs EVERY frame, and a
        // getBOOL is a string-keyed map lookup. Matches the hot-path convention
        // used throughout this file's callers.
        static LLCachedControl<bool> sidecar_on(gSavedSettings,
                                                "RenderVisibleDiffuseSidecar", false);
        if (sidecar_on && scr.getNumTextures() > 1)
        {
            fill_texture(tail.visible_diffuse, scr, 1);
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

    const SLReShadeTexture textures[] =
    {
        f.color_hdr, f.depth, f.albedo, f.orm, f.normals, f.emissive, f.motion,
        tail.visible_diffuse
    };
    bool dimensions_changed = false;
    bool target_changed = false;
    for (U32 i = 0; i < 8; ++i)
    {
        if (!texture_equal(textures[i], mLastTextures[i]))
        {
            dimensions_changed |= textures[i].width != mLastTextures[i].width ||
                                  textures[i].height != mLastTextures[i].height;
            target_changed |= textures[i].gl_name != mLastTextures[i].gl_name ||
                              textures[i].gl_internal_format != mLastTextures[i].gl_internal_format;
            mLastTextures[i] = textures[i];
        }
    }
    if (dimensions_changed || target_changed)
    {
        ++mRenderTargetGeneration;
        if (dimensions_changed)
        {
            noteResetEvent(SLRESHADE_RESET_RESIZE);
        }
        if (target_changed)
        {
            noteResetEvent(SLRESHADE_RESET_TARGET_RECREATED);
        }
    }

    const bool valid = (f.flags & SLRESHADE_FLAG_VALID) != 0;
    const bool hdr = (f.flags & SLRESHADE_FLAG_HDR) != 0;
    const bool snapshot = (f.flags & SLRESHADE_FLAG_SNAPSHOT) != 0;
    if (valid && !mEverHadValidFrame)
    {
        noteResetEvent(SLRESHADE_RESET_FIRST_VALID_FRAME);
    }
    else if (!valid && mLastFrameValid)
    {
        noteResetEvent(SLRESHADE_RESET_SOURCE_LOSS);
    }
    if (valid && mEverHadValidFrame && hdr != mLastHDR)
    {
        noteResetEvent(SLRESHADE_RESET_HDR_MODE_CHANGE);
    }
    if (f.frame_counter != 0 && snapshot != mLastSnapshot)
    {
        noteResetEvent(SLRESHADE_RESET_SNAPSHOT_TRANSITION);
    }

    if (texture_present(f.color_hdr)) tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_COLOR_HDR;
    if (texture_present(f.depth))     tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_DEPTH;
    if (texture_present(f.albedo))    tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_ALBEDO;
    if (texture_present(f.orm))       tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_ORM;
    if (texture_present(f.normals))   tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_NORMALS;
    if (texture_present(f.emissive))  tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_EMISSIVE;
    if (texture_present(f.motion) && mPendingMotionCoverage != 0)
    {
        tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_MOTION;
    }
    // Same shape as MOTION above: the texture existing is necessary but NEVER
    // sufficient. mPendingVisibleDiffuseResolved is set by the pipeline only
    // when the seed pass ran and the forward pool loop completed on the main
    // view this frame. Without it, toggling the setting (or a resize, or a
    // frame where the sidecar pass was skipped) publishes stale or cleared
    // content flagged VALID. (M4)
    if (texture_present(tail.visible_diffuse) && mPendingVisibleDiffuseResolved)
    {
        tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_VISIBLE_DIFFUSE;
    }
    tail.history_reset_flags = mPendingResetFlags;
    SLReShadeV11_SetGeneration(&tail, mRenderTargetGeneration);
    tail.motion_encoding =
        SLRESHADE_MOTION_ENCODING_CUR_NDC_MINUS_PREV_NDC_GL_UNJITTERED;
    tail.source_orientation_flags =
        SLRESHADE_ORIENT_SOURCE_UV_ORIGIN_BOTTOM_LEFT |
        SLRESHADE_ORIENT_SOURCE_REQUIRES_V_FLIP |
        SLRESHADE_ORIENT_MOTION_NDC_Y_UP |
        SLRESHADE_ORIENT_DESTINATION_UV_Y_DOWN;
    tail.motion_coverage_bits = mPendingMotionCoverage;
    if (texture_present(f.motion)) tail.effective_config_flags |= SLRESHADE_CONFIG_VELOCITY_ENABLED;
    if (hdr)                       tail.effective_config_flags |= SLRESHADE_CONFIG_HDR_ENABLED;
    if (snapshot)                  tail.effective_config_flags |= SLRESHADE_CONFIG_SNAPSHOT;
    if (LLRender::s10bitBackBuffer) tail.effective_config_flags |= SLRESHADE_CONFIG_FORCE_10BIT;
    // PROVIDER_OWNED is an add-on policy and is not observable by the viewer.
    // Tail word 16 is a modulo-2^32 reset-event counter. Unlike the transient
    // flags, a sampling reader detects any event by comparing counter values.
    tail.reserved[0] = mResetEventCounter;
    memcpy(f.reserved, &tail, sizeof(tail));

    mEverHadValidFrame |= valid;
    mLastFrameValid = valid;
    if (valid)
    {
        mLastHDR = hdr;
    }
    mLastSnapshot = snapshot;
    mPendingMotionCoverage = 0;
    mPendingVisibleDiffuseSeeded = false;
    mPendingVisibleDiffuseResolved = false;
    mPendingResetFlags = 0;

    f.frame_counter++;

    // --- seqlock: close the write window ---
    std::atomic_thread_fence(std::memory_order_release);
    f.write_end = f.write_begin;
}
