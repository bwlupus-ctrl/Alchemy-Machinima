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
#include "alactorchromakey.h"
#include "llnotificationsutil.h"
#include "../../reshade-addon/src/sl_module_watch.hpp"
#if __has_include("sl_bridge_build_info.h")
#include "sl_bridge_build_info.h"
#endif
#ifndef SL_EXPECTED_ADDON_SOURCE_SHA256
#define SL_EXPECTED_ADDON_SOURCE_SHA256 "" // unconfigured binaries fail closed
#define SL_ADDON_COMMIT ""
#endif
#include <cstring>              // memcpy

#include "alcameradispatch.h"  // [ReShadeReset] presented camera owner
#include "aldirectorswitcher.h" // [ReShadeReset] explicit Director cuts
#include "llagent.h"           // [ReShadeReset] agent-to-global camera position
#include "llappviewer.h"       // [ReShadeReset] gFrameIntervalSeconds
#include "pipeline.h"          // gPipeline, RenderTargetPack, LLRenderTarget, mVelocityMap
#include "llviewercamera.h"    // LLViewerCamera (matrices, near/far/fov, world frame)
#include "llviewercontrol.h"   // gSavedSettings / LLCachedControl (sidecar gate)
#include "llviewerdisplay.h"   // [ReShadeReset] gDisplaySwapBuffers
#include "llviewershadermgr.h" // [TronA0] gbufferAvatarTagActive (normals .w flag encoding)

extern bool gSnapshot;         // llviewerdisplay.cpp

#if LL_WINDOWS
static SLKey::ModuleWatch& keyModuleWatch()
{
    static SLKey::ModuleWatch watch;
    static const bool started = [&]() { watch.start(); return true; }();
    (void)started;
    return watch;
}
#endif

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
    mPendingSurfaceCoverageSeeded(false),
    mPendingSurfaceCoverageResolved(false),
    mPendingResetFlags(0),
    mResetEventCounter(0),
    mEverHadValidFrame(false),
    mLastFrameValid(false),
    mLastHDR(false),
    mLastSnapshot(false),
    // [ReShadeReset] Camera history begins only after an add-on consumes the ABI.
    mConsumerActive(false),
    mHaveLastCamera(false),
    mLastCameraGlobal(),
    mLastCameraAt(),
    mLastCutSerial(0),
    mLastCutSlot(-1),
    mLastCutMode(-1),
    mLastCineDriving(false),
    mPendingDirectorCut(false)
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

void LLReShadeBridge::noteTeleport(const char* reason)
{
    // [ReShadeReset] Teleport/region callbacks are rare, but stay entirely inert
    // until the external add-on has requested the bridge ABI at least once.
    static LLCachedControl<bool> detect_cut(gSavedSettings,
                                             "BDMergeReShadeCutDetect", true);
    if (!isActive() || !detect_cut())
    {
        return;
    }

    noteResetEvent(SLRESHADE_RESET_TELEPORT);
    LL_INFOS("ReShadeBridge") << "[ReShadeReset] teleport: " << reason << LL_ENDL;
}

void LLReShadeBridge::noteCameraCut(const char*)
{
    // [ReShadeReset] The cinematic camera calls this only for an uneased
    // switcher cut that owns the presented camera. Defer the movement test
    // until gatherFrame() sees the final camera pose.
    static LLCachedControl<bool> detect_cut(gSavedSettings,
                                             "BDMergeReShadeCutDetect", true);
    if (isActive() && detect_cut())
    {
        mPendingDirectorCut = true;
    }
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

void LLReShadeBridge::noteSurfaceCoverageSeeded()
{
    mPendingSurfaceCoverageSeeded = true;
}

void LLReShadeBridge::noteSurfaceCoverageResolved()
{
    // Mirrors visible diffuse: completing the forward loop cannot manufacture
    // readiness if the seed never ran to define the deferred half of the mask.
    mPendingSurfaceCoverageResolved = mPendingSurfaceCoverageSeeded;
}

void LLReShadeBridge::noteResetEvent(U32 flags)
{
    mPendingResetFlags |= flags;
    ++mResetEventCounter;
}

void LLReShadeBridge::detectCameraCut(const LLViewerCamera& camera)
{
    // [ReShadeReset] LLCachedControl avoids string lookups in this per-frame path.
    static LLCachedControl<bool> detect_jump(gSavedSettings,
                                              "BDMergeReShadeCutDetect", true);
    static LLCachedControl<F32> cut_distance(gSavedSettings,
                                              "BDMergeReShadeCutDistance", 8.f);
    static LLCachedControl<F32> cut_angle(gSavedSettings,
                                           "BDMergeReShadeCutAngle", 60.f);

    const LLVector3d origin_global =
        gAgent.getPosGlobalFromAgent(camera.getOrigin());
    const LLVector3 at = camera.getAtAxis();
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const U64 cut_serial = switcher.cutSerial();
    const S32 cut_slot = switcher.activeSlot();
    const bool cine_driving =
        gCameraDispatchOwner == ECameraOwner::CINE &&
        switcher.isDrivingCamera();

    // [ReShadeReset] Establish history without treating the first published
    // main-view frame as a discontinuity.
    if (!mHaveLastCamera)
    {
        mHaveLastCamera = true;
        mLastCameraGlobal = origin_global;
        mLastCameraAt = at;
        mLastCutSerial = cut_serial;
        mLastCutSlot = cut_slot;
        mLastCutMode = switcher.activeMode();
        mLastCineDriving = cine_driving;
        mPendingDirectorCut = false;
        return;
    }

    const F64 distance = dist_vec(origin_global, mLastCameraGlobal);
    const F32 angle_degrees = angle_between(at, mLastCameraAt) * RAD_TO_DEG;
    const F32 frame_dt = llmax(gFrameIntervalSeconds.value(), 0.000001f);
    const F64 implied_speed = distance / frame_dt;
    const F32 angular_speed = angle_degrees / frame_dt;
    const bool camera_moved = distance > 0.01 || angle_degrees > 0.25f;
    const bool teleport_pending =
        (mPendingResetFlags & SLRESHADE_RESET_TELEPORT) != 0;
    const bool serial_changed = cut_serial != mLastCutSerial;
    // [ReShadeReset] The switcher deliberately advances its serial at every
    // elapsed auto boundary, even when a one-slot bank selects the same live
    // source. That is continuous program video, not a cut.
    // Same slot AND same mode only: a same-index re-take, rewind, ACTION
    // re-take or live mode edit is a real on-screen change and must not be
    // absorbed here (it still has to pass camera_moved below).
    const S32 cut_mode = switcher.activeMode();
    const bool single_slot_auto = serial_changed && mLastCineDriving &&
        cine_driving && cut_slot == mLastCutSlot && cut_mode == mLastCutMode;
    // An authored ease is continuous program video. Suppress the generic
    // discontinuity backstop only for the ease window plus its catch-up frame,
    // on the same unscaled timer CineCam ends the ease on -- not for the whole
    // switcher program.
    const F64 ease_s = switcher.cutEaseSeconds();
    const bool eased_director_shot = cine_driving && ease_s > 0.0 &&
        switcher.cutEaseElapsedSeconds() < ease_s + llmax((F64)frame_dt, 0.1);
    // [ReShadeReset] Recheck presentation ownership at publication time. The
    // ease decision is CineCam's own (it only arms on its non-eased branch,
    // including rejected eases). mLastCutSerial is still refreshed every
    // frame, absorbing serial bumps while PATH, RECORDER, or another owner
    // presents the camera.
    const bool director_cut = detect_jump() && mPendingDirectorCut &&
        cine_driving && !single_slot_auto && camera_moved;
    // The pose backstop never needs the single-slot exemption: a static slot
    // cannot reach 8 m at 400 m/s or 60 deg at 720 deg/s.
    const bool position_jump = detect_jump() && !teleport_pending &&
        !eased_director_shot &&
        distance > llmax((F32)cut_distance, 0.f) && implied_speed > 400.f;
    const bool rotation_jump = detect_jump() && !teleport_pending &&
        !eased_director_shot &&
        angle_degrees > llclamp((F32)cut_angle, 0.f, 180.f) &&
        angular_speed > 720.f;

    // [ReShadeReset] Coalesce coincident serial/pose evidence into one reset and
    // one log line for this rendered frame.
    if (director_cut || position_jump || rotation_jump)
    {
        noteResetEvent(SLRESHADE_RESET_CAMERA_CUT);
        LL_INFOS("ReShadeBridge")
            << "[ReShadeReset] " << (director_cut ? "cut" : "jump")
            << ": distance=" << distance << "m"
            << " angle=" << angle_degrees << "deg"
            << " speed=" << implied_speed << "m/s"
            << " angular_speed=" << angular_speed << "deg/s"
            << " serial=" << mLastCutSerial << "->" << cut_serial
            << LL_ENDL;
    }

    mLastCameraGlobal = origin_global;
    mLastCameraAt = at;
    mLastCutSerial = cut_serial;
    mLastCutSlot = cut_slot;
    mLastCutMode = cut_mode;
    mLastCineDriving = cine_driving;
    mPendingDirectorCut = false;
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
    // [ReShadeReset] Do no cut-detection work in sessions with no ABI consumer.
    LLReShadeBridge& bridge = LLReShadeBridge::instance();
    bridge.markConsumerActive();
    bridge.noteReader(requested_abi, gFrameCount);
    return &bridge.getFrameData();
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

    // [ReShadeReset] gatherFrame() has exactly one display-loop call site after
    // scene rendering. The explicit snapshot gate plus that call-site placement
    // keep snapshot, probe, hero, and Prism aux poses out of camera history.
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    if (cam && isActive() && !gSnapshot)
    {
        detectCameraCut(*cam);
    }

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
        //
        // Albedo and normals are NOT read from def directly: gatherFrame() runs
        // long after renderFinalize()'s whole post chain (see the call site in
        // llviewerdisplay.cpp), while def's albedo/normals attachments are only
        // known-good between the deferred geometry pass and the point
        // renderDeferredLighting() finishes consuming them for lighting.
        // preserveReShadeGBuffer() (pipeline.cpp) blits them into
        // mReShadeAlbedoRaw/mReShadeNormalsRaw at that earlier, known-good
        // moment; publish those when available. mReShadeGBufferRawValid is
        // false whenever this isn't the main view (aux/probe/HUD/impostor/Prism
        // passes) or RenderReShadeGBufferPublish is off, in which case this
        // falls back to reading def live, exactly as before the fix.
        const U32 n = def.getNumTextures();
        const bool gbuffer_raw_valid = gPipeline.mReShadeGBufferRawValid &&
                                        gPipeline.mReShadeAlbedoRaw.getWidth() > 0 &&
                                        gPipeline.mReShadeNormalsRaw.getWidth() > 0;
        if (gbuffer_raw_valid)
        {
            fill_texture(f.albedo,  gPipeline.mReShadeAlbedoRaw, 0);
            fill_texture(f.normals, gPipeline.mReShadeNormalsRaw, 0);
        }
        else
        {
            if (n > 0) fill_texture(f.albedo,  def, 0);
            if (n > 2) fill_texture(f.normals, def, 2);
        }
        if (n > 1) fill_texture(f.orm,      def, 1);
        static LLCachedControl<bool> emissive_enabled(gSavedSettings, "RenderEnableEmissiveBuffer", false);
        static LLCachedControl<bool> silhouette_force(gSavedSettings, "AvatarSilhouetteForceEmissive", false);
        const bool silhouette_only_emissive = !emissive_enabled() && silhouette_force();
        if (n > 3 && !silhouette_only_emissive) fill_texture(f.emissive, def, 3);

        // On-lens decouple: when applyOnLensFilters captured a RAW pre-filter
        // snapshot this frame (RenderReShadeDecoupleOnLens on + filters active),
        // publish THAT as color_hdr so RTGI-style effects see the unfiltered
        // scene — a lens filter must not change surface-to-surface bounce.
        // Otherwise (filters off, decouple off, non-HDR path, or target not
        // allocated) mReShadeRawSceneValid is false and we publish mRT->screen
        // exactly as before, byte-identical to the legacy/coupled behavior.
        LLRenderTarget& colorSrc = (gPipeline.mReShadeRawSceneValid &&
                                    gPipeline.mReShadeSceneRaw.getWidth() > 0)
                                       ? gPipeline.mReShadeSceneRaw
                                       : scr;
        if (colorSrc.getNumTextures() > 0)
        {
            fill_texture(f.color_hdr, colorSrc, 0);
        }
        // LLCachedControl, not getBOOL: gatherFrame() runs EVERY frame, and a
        // getBOOL is a string-keyed map lookup. Matches the hot-path convention
        // used throughout this file's callers.
        static LLCachedControl<bool> sidecar_on(gSavedSettings,
                                                "RenderVisibleDiffuseSidecar", false);
        if (sidecar_on && scr.getNumTextures() > SL_COVERAGE_ATTACHMENT)
        {
            fill_texture(tail.visible_diffuse,   scr, SL_SIDECAR_ATTACHMENT);
            fill_texture(tail.surface_coverage,  scr, SL_COVERAGE_ATTACHMENT);
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
        // [TronA0] Normals .w carries flag ENCODING 2 (odd 1/6 codes = avatar
        // pixels) exactly when the loaded shaders were built with the tag.
        // Keyed on the compiled define, never the raw setting, so the bit and
        // the bytes in the texture can never disagree across a shader reload.
        if (LLViewerShaderMgr::gbufferAvatarTagActive())
        {
            f.flags |= SLRESHADE_FLAG_AVATAR_TAG;
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

    // Every published texture must appear here, or a resize/recreate of the
    // missing one will not bump the generation counter and the add-on will keep
    // copying from stale storage. The bound is derived from the array rather
    // than hardcoded for exactly that reason -- it was literally 8 while the
    // array grew to 8 entries, and the next addition would have been skipped
    // silently. SL_RESHADE_TRACKED_TEXTURES sizes mLastTextures to match.
    const SLReShadeTexture textures[] =
    {
        f.color_hdr, f.depth, f.albedo, f.orm, f.normals, f.emissive, f.motion,
        tail.visible_diffuse, tail.surface_coverage
    };
    static_assert(LL_ARRAY_SIZE(textures) == SL_RESHADE_TRACKED_TEXTURES,
                  "mLastTextures must have one slot per published texture");
    bool dimensions_changed = false;
    bool target_changed = false;
    for (U32 i = 0; i < LL_ARRAY_SIZE(textures); ++i)
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
    if (texture_present(tail.surface_coverage) && mPendingSurfaceCoverageResolved)
    {
        tail.semantic_valid_bits |= SLRESHADE_SEM_VALID_SURFACE_COVERAGE;
    }

    // [ReShade G-buffer diag] Cheap, debug-gated verification for the
    // albedo/normals preserve-copy fix (preserveReShadeGBuffer(), pipeline.cpp).
    // Off by default; reads only already-computed struct fields plus one
    // getNumTextures() call, throttled to once per ~2s so it is safe to leave
    // on for an extended in-world check without spamming the log.
    static LLCachedControl<bool> gbuffer_diag(gSavedSettings, "RenderReShadeGBufferDiag", false);
    if (gbuffer_diag())
    {
        static F64 last_log_time = 0.0;
        const F64 now = LLTimer::getTotalSeconds();
        if (now - last_log_time >= 2.0)
        {
            last_log_time = now;
            const U32 num_textures = gPipeline.mRT ? gPipeline.mRT->deferredScreen.getNumTextures() : 0;
            LL_INFOS("ReShadeBridge")
                << "gbuffer diag: deferredScreen.numTextures=" << num_textures
                << " published albedo(gl=" << f.albedo.gl_name
                << " fmt=0x" << std::hex << f.albedo.gl_internal_format << std::dec
                << " " << f.albedo.width << "x" << f.albedo.height << ")"
                << " published normals(gl=" << f.normals.gl_name
                << " fmt=0x" << std::hex << f.normals.gl_internal_format << std::dec
                << " " << f.normals.width << "x" << f.normals.height << ")"
                << " preserveCopyRan=" << (gPipeline.mReShadeGBufferRawValid ? "true" : "false")
                << " semanticValidBits=0x" << std::hex << tail.semantic_valid_bits << std::dec
                << LL_ENDL;
        }
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
    // [TronA0] Mirror the avatar-tag frame flag into the tail so the .fx sees
    // it through the EXISTING sl_config_flags uniform (no add-on C++ change).
    if ((f.flags & SLRESHADE_FLAG_AVATAR_TAG) != 0) tail.effective_config_flags |= SLRESHADE_CONFIG_AVATAR_TAG;
    // PROVIDER_OWNED is an add-on policy and is not observable by the viewer.
    // Tail word 16 is a modulo-2^32 reset-event counter. Unlike the transient
    // flags, a sampling reader detects any event by comparing counter values.
    // Named in the ABI header as of S6 -- it was previously written through
    // reserved[0] while the header still called that block "future growth".
    tail.reset_event_counter = mResetEventCounter;
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
    mPendingSurfaceCoverageSeeded = false;
    mPendingSurfaceCoverageResolved = false;
    // [ReShadeReset] Snapshot rendering suppresses the next main-frame swap.
    // Retain flags until a later gather whose frame will actually be presented.
    if (gDisplaySwapBuffers)
    {
        mPendingResetFlags = 0;
    }

    f.frame_counter++;
    if (!gSnapshot && ALActorChromaKey::sActive)
    {
        const bool keyed = ALActorChromaKey::keyedThisFrame();
        const bool arm = mGuardEvalFrame == gFrameCount && mKeyGuard.arm;
        if (keyed) f.flags |= SLRESHADE_FLAG_ACTOR_KEY;
        else if (arm) f.flags |= SLRESHADE_FLAG_ACTOR_KEY_ARM;
        if (keyed || arm)
        {
#if LL_WINDOWS
            mKeyGuard.publish(f.frame_counter, keyModuleWatch().read(), keyed);
#else
            mKeyGuard.publish(f.frame_counter, 0, keyed);
#endif
        }
    }

    // --- seqlock: close the write window ---
    std::atomic_thread_fence(std::memory_order_release);
    f.write_end = f.write_begin;
}

// ABI reports copy their payload immediately. No caller-owned pointer survives.
void LLReShadeBridge::noteReader(U32 abi, U64 frame)
{
    mReaderAbi.store(abi, std::memory_order_relaxed);
    mReaderSeenFrame.store(frame, std::memory_order_release);
}
void LLReShadeBridge::reportReader(const SLReShadeReaderInfo* info)
{
    SLReShadeReaderInfo copy{};
    if (!SLReShade_CopyReader(&copy, info))
    {
        LL_WARNS_ONCE("ActorKey") << "Ignoring invalid SLReShadeReaderInfo" << LL_ENDL;
        return;
    }
    mReaderInfo.store(copy, std::memory_order_release);
    if (SLKey::sourceMatches(copy, SL_EXPECTED_ADDON_SOURCE_SHA256) && strcmp(copy.commit, SL_ADDON_COMMIT))
        LL_INFOS("ActorKey") << "ReShade reader commit differs (informational; source hash matches): " << copy.commit << LL_ENDL;
}
void LLReShadeBridge::reportKeyGuard(const SLReShadeKeyGuard* report)
{
    SLReShadeKeyGuard copy{};
    if (!SLReShade_CopyGuard(&copy, report)) return;
    // Epoch is checked by the main-thread model. Atomically copy the whole
    // fixed report; concurrent callbacks can never pair CLEAN with another id.
    auto previous = mGuardReport.load(std::memory_order_acquire);
    while (copy.viewer_frame >= previous.viewer_frame &&
        !mGuardReport.compare_exchange_weak(previous, copy, std::memory_order_release, std::memory_order_acquire)) {}
}
extern "C"
#if LL_WINDOWS
__declspec(dllexport)
#endif
void SLReShade_ReportReader(const SLReShadeReaderInfo* info)
{
    LLReShadeBridge::instance().reportReader(info);
}
extern "C"
#if LL_WINDOWS
__declspec(dllexport)
#endif
void SLReShade_ReportKeyGuard(const SLReShadeKeyGuard* report)
{
    LLReShadeBridge::instance().reportKeyGuard(report);
}
void LLReShadeBridge::retryKeyGuard()
{
    mKeyGuard.reset();
    mGuardEvalFrame = ~U64(0);
    mGuardAllowed = false;
    mNotifyOldReader = ALActorChromaKey::sActive;
    ++mKeyStatus.revision;
    mKeyStatus.state = SLKey::State::Off;
    mKeyStatus.reason = SLKey::Reason::Off;
    mKeyStatus.red = mKeyStatus.amber = false;
    mKeyStatus.text = "Actor key off";
    mGuardReport.store(SLReShadeKeyGuard{}, std::memory_order_release);
}
bool LLReShadeBridge::keyGuardAllows(U64 frame)
{
    LL_PROFILE_ZONE_NAMED("ActorKey::moduleGate");
    if (mGuardEvalFrame == frame) return mGuardAllowed;
    mGuardEvalFrame = frame;
    SLReShadeReaderInfo reader{};
    SLReShadeKeyGuard report{};
    reader = mReaderInfo.load(std::memory_order_acquire);
    report = mGuardReport.load(std::memory_order_acquire);
    int gate = 0;
    U64 module_state = 0;
#if LL_WINDOWS
    auto& watch = keyModuleWatch();
    static SLKey::SettledScan<SLKey::ModuleList> scans;
    SLKey::ModuleList result;
    gate = scans.run(watch.notified(), GetTickCount(), [&]() { return watch.read(); }, SLKey::scanReShade, result);
    module_state = watch.read();
    mModuleScans = scans.scans;
    if (!watch.notified()) LL_WARNS_ONCE("ActorKey") << "DLL notification unavailable; scanning twice every eligible frame" << LL_ENDL;
#endif
    const U32 abi = mReaderAbi.load(std::memory_order_relaxed);
    const U64 seen = mReaderSeenFrame.load(std::memory_order_acquire);
    const bool recent = abi && frame >= seen && frame-seen <= 120;
    const bool capable = SLKey::readerCapable(abi, reader);
    const bool hash = SLKey::sourceMatches(reader, SL_EXPECTED_ADDON_SOURCE_SHA256);
    // Reports use the ABI frame sequence. The next gather publishes +1.
    mGuardAllowed = mKeyGuard.evaluate(mFrame.frame_counter+1, gate, recent, capable, hash, module_state, report);
    updateKeyStatus(gate != 0, reader, report);
    if (mNotifyOldReader && mKeyGuard.reason == SLKey::Reason::OldReader)
    {
        mNotifyOldReader = false;
        LLSD args; args["MESSAGE"] = mKeyStatus.text;
        LLNotificationsUtil::add("GenericAlert", args);
    }
    return mGuardAllowed;
}
void LLReShadeBridge::updateKeyStatus(bool loaded, const SLReShadeReaderInfo& reader, const SLReShadeKeyGuard& report)
{
    KeyStatus next;
    next.state = mKeyGuard.state; next.reason = mKeyGuard.reason; next.reshadeLoaded = loaded;
    next.red = next.state == SLKey::State::Refused;
    next.amber = loaded || next.state == SLKey::State::Arming;
    next.readerAbi = mReaderAbi.load(std::memory_order_relaxed); next.readerCaps = reader.caps;
    next.passes = report.passes; next.calls = report.effect_calls; next.gui = report.gui_flags; next.foreign = report.foreign_addons;
    next.guarantee = loaded ? SLKey::guarantee : "Key colour is exact.";
    using R = SLKey::Reason;
    switch (next.reason)
    {
    case R::NoReShade: next.text = "ReShade: not loaded"; break;
    case R::Settling: next.text = "ReShade: waiting for DLL loading to settle"; break;
    case R::NoReader: next.text = "ReShade is loaded but the SL bridge add-on is not running - key refused (deploy sl_reshade_bridge.addon)"; break;
    case R::OldReader: next.text = "ReShade add-on is OLDER than this viewer (no key guard) - key refused (run deploy_addon.ps1)"; break;
    case R::StaleSource: next.text = "ReShade add-on binary is stale (built from different bridge sources): run deploy_addon.ps1 - key refused"; break;
    case R::Activity: next.text = "ReShade effect activity with Effects off: " + std::to_string(report.passes) +
        " pass(es), " + std::to_string(report.effect_calls) + " begin/finish call(s) (.addonfx / add-on effects) - key refused. Disable them in the ReShade overlay, then Retry"; break;
    case R::Foreign: next.text = "Other ReShade add-ons are loaded (" + std::to_string(report.foreign_addons) + ", listed in ReShade.log) - key refused. Remove them from the add-on folder for keying"; break;
    case R::Host: next.text = "Unverified ReShade build - key refused (expected 6.8.0.2155)"; break;
    case R::CheckFailed: next.text = "ReShade check failed - key refused (see ReShade.log)"; break;
    case R::Heartbeat: next.text = "ReShade bridge stopped confirming frames - key refused"; break;
    default: next.text = next.state == SLKey::State::Keying ? "ReShade add-on 1.3, key bypass active" : "ReShade: checking..."; break;
    }
    if (loaded)
    {
        next.overlay = "ReShade overlay not hidden";
        if (report.gui_flags & SLRESHADE_GUI_OVERLAY) next.overlay += "; overlay open";
        if (report.gui_flags & SLRESHADE_GUI_OSD) next.overlay += "; FPS/clock OSD always on (ReShade.ini)";
        if (report.gui_flags & SLRESHADE_GUI_TUTORIAL) next.overlay += "; splash shown until the ReShade tutorial is dismissed";
        if (report.gui_flags & SLRESHADE_GUI_RELOAD_SPLASH) next.overlay += "; splash after reload (<= 5 s)";
    }
    const bool changed = next.text != mKeyStatus.text || next.overlay != mKeyStatus.overlay || next.state != mKeyStatus.state || next.reshadeLoaded != mKeyStatus.reshadeLoaded ||
        next.reason != mKeyStatus.reason || next.readerAbi != mKeyStatus.readerAbi || next.readerCaps != mKeyStatus.readerCaps ||
        next.passes != mKeyStatus.passes || next.calls != mKeyStatus.calls || next.gui != mKeyStatus.gui || next.foreign != mKeyStatus.foreign;
    next.revision = mKeyStatus.revision + changed;
    mKeyStatus = next;
    if (changed)
    {
        LL_INFOS("ActorKey") << next.text << " reader_abi=" << next.readerAbi << " reader_caps=" << next.readerCaps
            << " guard=" << static_cast<int>(next.state) << " guard_passes=" << next.passes << " guard_calls=" << next.calls
            << " guard_gui=" << next.gui << " foreign=" << next.foreign << " heartbeat_miss=" << mKeyGuard.heartbeatMiss
            << " arm_presents=" << mKeyGuard.armPresents
#if LL_WINDOWS
            << " module_scans=" << mModuleScans
#endif
            << " " << next.overlay << " " << next.guarantee << LL_ENDL;
    }
}
