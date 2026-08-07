/**
 * @file llprismlens.cpp
 * @brief Bounded Prism Lens and virtual-camera surface-feed implementation.
 */

#include "llviewerprecompiledheaders.h"

#include "llprismlens.h"

#include "llappviewer.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "llenvironment.h"
#include "llface.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llnotificationsutil.h"
#include "llplane.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llselectmgr.h"
#include "lltimer.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

extern bool gCubeSnapshot;

namespace
{
constexpr S32 DEBUG_RECT_X = 96;
constexpr S32 DEBUG_RECT_Y = 96;
constexpr S32 DEBUG_RECT_WIDTH = 512;
constexpr S32 DEBUG_RECT_HEIGHT = 288;
constexpr S32 MIN_PROJECTED_EXTENT = 8;
constexpr S32 MIN_PROJECTED_AREA = 64;
constexpr S32 MIN_VISIBLE_EXTENT = 2;
constexpr S32 MIN_VISIBLE_AREA = 4;
constexpr F32 LENS_PLANE_EPSILON = 0.002f;
constexpr F32 PLANAR_ABSOLUTE_EPSILON = 0.003f;
constexpr F32 PLANAR_RELATIVE_EPSILON = 0.002f;
constexpr F32 PLANAR_NORMAL_COS = 0.9986295f; // cos(3 degrees)
constexpr F32 CLIP_EPSILON = 1e-5f;
constexpr F32 SURFACE_UV_EPSILON = 1e-5f;
constexpr F32 SURFACE_CORNER_EPSILON = 1e-4f;
constexpr F32 MAX_SURFACE_EDGE_COS = 0.052336f; // sin(3 degrees)
// Determinant floor for inverting a face's relative transform in the geometry
// fallback. Deliberately far below any legal prim scale product (0.01^3 = 1e-6)
// so tiny-but-valid objects are never misclassified as non-invertible; it only
// screens out genuinely collapsed/singular transforms before glm::inverse.
constexpr F32 SURFACE_MATRIX_MIN_DETERMINANT = 1e-12f;
// Geometry-fallback OBB: boundary-edge directions within ~0.08 degrees of an
// already-evaluated candidate axis produce the same oriented rectangle (a
// quad's two parallel sides), so they are skipped as duplicates.
constexpr F32 OBB_PARALLEL_AXIS_COS = 0.999999f;
constexpr F32 MIN_CULL_HALF_ANGLE = 0.0005f;
constexpr F32 MIN_CULL_VERTICAL_HALF_ANGLE = 0.0436332f; // half of LLCamera's 5 degrees
constexpr F32 MAX_CULL_VERTICAL_HALF_ANGLE = 1.5271631f; // half of LLCamera's 175 degrees
constexpr F32 MAX_CULL_HORIZONTAL_HALF_ANGLE = 1.5699231f;
constexpr U32 MIN_TARGET_EXTENT = 64;
constexpr U32 MAX_TARGET_EXTENT = 1024;
constexpr F64 SURFACE_CACHE_REVALIDATE_SECONDS = 4.0;
constexpr F64 SURFACE_CACHE_REVALIDATE_JITTER_SECONDS = 1.0;

struct PrismRect
{
    S32 mX = 0;
    S32 mY = 0;
    U32 mWidth = 0;
    U32 mHeight = 0;
};

struct PrismFrame
{
    U32 mFrame = 0;
    LLFace* mResolvedFace = nullptr; // Call-scoped; RAII-cleared before returning.
    PrismRect mLensRect;
    PrismRect mDebugRect;
    S32 mMainViewport[4] = { 0, 0, 0, 0 };
    LLPlane mFragmentClipPlane;
    LLVector3 mSurfaceOrigin;
    LLVector3 mSurfaceUDual;
    LLVector3 mSurfaceVDual;
    LLVector3 mWorldSurfaceOrigin;
    LLVector3 mWorldSurfaceUEdge;
    LLVector3 mWorldSurfaceVEdge;
    F32 mCompositeUvScale[2] = { 1.f, 1.f };
    F32 mCompositeUvOffset[2] = { 0.f, 0.f };
    U32 mTargetWidth = 0;
    U32 mTargetHeight = 0;
    F32 mZoom = 2.f;
    F32 mResolutionScale = 1.f;
    F32 mEdgeFeather = 0.f;
    bool mPrepared = false;
    bool mProduced = false;
};

bool prismEnabled()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "PrismLensEnabled", false);
    return enabled;
}

bool prismDebugEnabled()
{
    static LLCachedControl<bool> debug(gSavedSettings, "PrismLensDebug", false);
    return debug;
}

F32 prismZoom()
{
    static LLCachedControl<F32> zoom(gSavedSettings, "PrismLensZoom", 2.f);
    return llclamp(zoom(), 1.f, 8.f);
}

F32 prismResolutionScale()
{
    static LLCachedControl<F32> scale(gSavedSettings, "PrismLensResolutionScale", 1.f);
    const F32 value = scale();
    return std::isfinite(value) ? llclamp(value, 0.25f, 2.f) : 1.f;
}

F32 prismEdgeFeather()
{
    static LLCachedControl<F32> feather(gSavedSettings, "PrismLensEdgeFeather", 0.f);
    return llmax(feather(), 0.f);
}

F32 prismProtectedMainFps()
{
    static LLCachedControl<F32> setting(gSavedSettings, "PrismProtectedMainFPS", 30.f);
    const F32 value = setting();
    if (value == 30.f || value == 45.f || value == 60.f) return value;
    static bool warned = false;
    if (!warned)
    {
        LL_WARNS("PrismLens") << "Invalid PrismProtectedMainFPS; using 30" << LL_ENDL;
        warned = true;
    }
    return 30.f;
}

F32 prismCaptureBudgetHz()
{
    static LLCachedControl<F32> setting(
        gSavedSettings, "PrismCaptureRefreshCeilingHz", 30.f);
    const F32 value = setting();
    if (value == 0.f || value == 5.f || value == 10.f || value == 15.f ||
        value == 20.f || value == 30.f)
    {
        return value;
    }
    static bool warned = false;
    if (!warned)
    {
        LL_WARNS("PrismLens")
            << "Invalid PrismCaptureRefreshCeilingHz; using 30" << LL_ENDL;
        warned = true;
    }
    return 30.f;
}

bool prismManualEveryFrameMode()
{
    static LLCachedControl<bool> adaptive(
        gSavedSettings, "PrismAdaptivePerformance", true);
    return !adaptive() && prismCaptureBudgetHz() == 0.f;
}

bool prismAdaptiveEnabled()
{
    static LLCachedControl<bool> adaptive(gSavedSettings, "PrismAdaptivePerformance", true);
    return adaptive;
}

class PrismAdaptiveController
{
public:
    void updateFrame(bool master_enabled, bool has_captures,
                     bool render_context_available)
    {
        const U32 frame = gFrameCount;
        if (mLastUpdateFrame == frame)
        {
            return;
        }

        const F64 now = LLTimer::getTotalSeconds();
        const bool adaptive_enabled = prismAdaptiveEnabled();
        const F32 target_fps = prismProtectedMainFps();
        const F32 capture_budget_hz = prismCaptureBudgetHz();
        const F32 resolution_ceiling = prismResolutionScale();
        const bool transition = !mInitialized ||
            master_enabled != mMasterEnabled ||
            adaptive_enabled != mAdaptiveEnabled ||
            has_captures != mHadCaptures ||
            target_fps != mTargetFps ||
            capture_budget_hz != mCaptureBudgetHz ||
            resolution_ceiling != mResolutionCeiling;
        F64 frame_seconds = 0.0;
        if (transition)
        {
            reset(now, frame);
        }
        else
        {
            frame_seconds = llclamp(now - mLastUpdateTime, 0.0, 0.1);
            if (mMasterEnabled && mAdaptiveEnabled && mHadCaptures &&
                mRenderContextAvailable && render_context_available)
            {
                observePreviousFrame(frame, now);
            }
            else
            {
                invalidateNoAuxReference();
            }
            mLastUpdateFrame = frame;
            mLastUpdateTime = now;
        }

        mInitialized = true;
        mMasterEnabled = master_enabled;
        mAdaptiveEnabled = adaptive_enabled;
        mHadCaptures = has_captures;
        mRenderContextAvailable = render_context_available;
        mTargetFps = target_fps;
        mCaptureBudgetHz = capture_budget_hz;
        mResolutionCeiling = resolution_ceiling;
        mPresentedFpsValid = std::isfinite(gFPSClamped) && gFPSClamped > 0.f;
        mDownshiftPending = false;
        mRecovering = false;
        mRecoveryVetoed = false;

        // Manual mode is deliberately identical to the old path. Master-off,
        // no-capture, and mode/target transitions reset all adaptive history.
        if (!master_enabled || !adaptive_enabled || !has_captures)
        {
            mCadenceFactor = 1.f;
            mResolutionFactor = 1.f;
            resetDwell();
            invalidateNoAuxReference();
            return;
        }
        if (!render_context_available)
        {
            // A cube snapshot or unavailable deferred context is not evidence
            // about Prism cost. Hold the current protection level and require a
            // fresh dwell when ordinary main-view rendering resumes.
            resetDwell();
            invalidateNoAuxReference();
            return;
        }
        if (!mPresentedFpsValid)
        {
            // An unavailable presented-FPS signal may neither relax nor deepen
            // protection. This is a hold, not an inferred healthy baseline.
            resetDwell();
            return;
        }

        const F32 low_band = target_fps - llmax(0.5f, target_fps * 0.02f);
        const F32 recovery_band = target_fps - llmax(0.1f, target_fps * 0.005f);
        const F32 ratio = gFPSClamped / target_fps;
        if (gFPSClamped < low_band)
        {
            mRecoveryEligibleSince = -1.0;
            mRecoveryVetoed = false;
            if (mLowFpsSince < 0.0)
            {
                mLowFpsSince = now;
            }
            const bool severe = ratio <= 0.8f;
            if (severe || now - mLowFpsSince >= 0.2)
            {
                // Clamp down in one step after a short persistence filter. A
                // severe (at/below 80% target) frame suspends cadence immediately.
                const F32 desired_cadence = llclamp((ratio - 0.8f) / 0.2f,
                                                    0.f, 1.f);
                const F32 minimum_resolution_factor = llclamp(
                    0.25f / prismResolutionScale(), 0.f, 1.f);
                const F32 desired_resolution = llclamp(
                    ratio, minimum_resolution_factor, 1.f);
                mCadenceFactor = llmin(mCadenceFactor, desired_cadence);
                mResolutionFactor = llmin(mResolutionFactor, desired_resolution);
            }
            else
            {
                mDownshiftPending = true;
            }
            return;
        }

        mLowFpsSince = -1.0;
        const bool needs_recovery = mCadenceFactor < 0.999f ||
                                    mResolutionFactor < 0.999f;
        if (!needs_recovery)
        {
            mCadenceFactor = 1.f;
            mResolutionFactor = 1.f;
            mRecoveryEligibleSince = -1.0;
            return;
        }

        const bool no_aux_veto = mNoAuxReferenceValid &&
            mNoAuxReferenceFps < recovery_band;
        mRecoveryVetoed = gFPSClamped >= recovery_band && no_aux_veto;
        if (gFPSClamped < recovery_band || no_aux_veto)
        {
            mRecoveryEligibleSince = -1.0;
            return;
        }
        if (mRecoveryEligibleSince < 0.0)
        {
            mRecoveryEligibleSince = now;
            return;
        }
        if (now - mRecoveryEligibleSince < 2.0)
        {
            return;
        }

        // Recovery is intentionally rate-limited and frame-delta-capped. A
        // stall cannot grant catch-up credit or jump directly back to full cost.
        mCadenceFactor = llmin(1.f, mCadenceFactor +
            static_cast<F32>(frame_seconds) * 0.05f);
        mResolutionFactor = llmin(1.f, mResolutionFactor +
            static_cast<F32>(frame_seconds) * 0.025f);
        mRecovering = true;
    }

    void noteAuxiliaryWork()
    {
        if (mMasterEnabled && mAdaptiveEnabled && mHadCaptures)
        {
            mLastAuxiliaryWorkFrame = gFrameCount;
        }
    }

    void forceReset()
    {
        // Resource/scene lifecycle resets are observations too: prevent an
        // old dwell or no-aux reference from crossing a target rebuild. Keep
        // this frame consumed so a second callback cannot evaluate twice.
        reset(LLTimer::getTotalSeconds(), gFrameCount);
        mInitialized = false;
        mMasterEnabled = false;
        mAdaptiveEnabled = prismAdaptiveEnabled();
        mHadCaptures = false;
        mRenderContextAvailable = false;
        mPresentedFpsValid = false;
        mTargetFps = prismProtectedMainFps();
        mCaptureBudgetHz = prismCaptureBudgetHz();
        mResolutionCeiling = prismResolutionScale();
    }

    F32 cadenceFactor() const { return mCadenceFactor; }
    F32 resolutionFactor() const { return mResolutionFactor; }
    bool presentedFpsValid() const { return mPresentedFpsValid; }
    bool downshiftPending() const { return mDownshiftPending; }
    bool recovering() const { return mRecovering; }
    bool recoveryVetoed() const { return mRecoveryVetoed; }
    bool noAuxReferenceValid() const { return mNoAuxReferenceValid; }
    F32 noAuxReferenceFps() const { return mNoAuxReferenceFps; }

private:
    void reset(F64 now, U32 frame)
    {
        mCadenceFactor = 1.f;
        mResolutionFactor = 1.f;
        mLastUpdateFrame = frame;
        mLastUpdateTime = now;
        mLastAuxiliaryWorkFrame = std::numeric_limits<U32>::max();
        resetDwell();
        invalidateNoAuxReference();
    }

    void resetDwell()
    {
        mLowFpsSince = -1.0;
        mRecoveryEligibleSince = -1.0;
        mDownshiftPending = false;
        mRecovering = false;
        mRecoveryVetoed = false;
    }

    void invalidateNoAuxReference()
    {
        mConsecutiveNoAuxFrames = 0;
        mNoAuxWindowStart = -1.0;
        mNoAuxReferenceValid = false;
        mNoAuxReferenceFps = 0.f;
    }

    void observePreviousFrame(U32 frame, F64 now)
    {
        // The presented-FPS EWMA observed at the start of this frame describes
        // the previous presented frame. Only a contiguous, explicitly no-aux
        // run may contribute a conservative recovery veto.
        if (frame != mLastUpdateFrame + 1u ||
            mLastAuxiliaryWorkFrame == mLastUpdateFrame ||
            !std::isfinite(gFPSClamped) || gFPSClamped <= 0.f)
        {
            invalidateNoAuxReference();
            return;
        }
        if (mConsecutiveNoAuxFrames == 0)
        {
            mNoAuxWindowStart = mLastUpdateTime;
        }
        ++mConsecutiveNoAuxFrames;
        if (mConsecutiveNoAuxFrames < 8 || mNoAuxWindowStart < 0.0 ||
            now - mNoAuxWindowStart < 0.25)
        {
            return;
        }
        if (!mNoAuxReferenceValid)
        {
            mNoAuxReferenceFps = gFPSClamped;
            mNoAuxReferenceValid = true;
        }
        else
        {
            // Slow EWMA. It can only block recovery; it is never treated as a
            // cost attribution measurement or as permission to increase work.
            mNoAuxReferenceFps = 0.9f * mNoAuxReferenceFps +
                                 0.1f * gFPSClamped;
        }
    }

    bool mInitialized = false;
    bool mMasterEnabled = false;
    bool mAdaptiveEnabled = true;
    bool mHadCaptures = false;
    bool mRenderContextAvailable = false;
    bool mPresentedFpsValid = false;
    bool mDownshiftPending = false;
    bool mRecovering = false;
    bool mRecoveryVetoed = false;
    F32 mTargetFps = 30.f;
    F32 mCaptureBudgetHz = 30.f;
    F32 mResolutionCeiling = 1.f;
    F32 mCadenceFactor = 1.f;
    F32 mResolutionFactor = 1.f;
    F64 mLowFpsSince = -1.0;
    F64 mRecoveryEligibleSince = -1.0;
    F64 mLastUpdateTime = 0.0;
    U32 mLastUpdateFrame = std::numeric_limits<U32>::max();
    U32 mLastAuxiliaryWorkFrame = std::numeric_limits<U32>::max();
    U32 mConsecutiveNoAuxFrames = 0;
    F64 mNoAuxWindowStart = -1.0;
    bool mNoAuxReferenceValid = false;
    F32 mNoAuxReferenceFps = 0.f;
};

PrismAdaptiveController& prismAdaptiveController()
{
    static PrismAdaptiveController controller;
    return controller;
}

F32 prismAppliedResolutionScale()
{
    const F32 ceiling = prismResolutionScale();
    if (!prismAdaptiveEnabled())
    {
        return ceiling;
    }
    return llclamp(ceiling * prismAdaptiveController().resolutionFactor(),
                   0.25f, ceiling);
}

F32 prismAdaptiveCadenceFactor()
{
    return prismAdaptiveEnabled()
        ? prismAdaptiveController().cadenceFactor() : 1.f;
}

bool makeDebugRect(const S32 viewport[4], PrismRect& rect)
{
    if (viewport[2] < 64 || viewport[3] < 64)
    {
        return false;
    }

    rect.mWidth = static_cast<U32>(llmin(viewport[2], DEBUG_RECT_WIDTH));
    rect.mHeight = static_cast<U32>(llmin(viewport[3], DEBUG_RECT_HEIGHT));
    rect.mX = viewport[0] + llmin(DEBUG_RECT_X, viewport[2] - static_cast<S32>(rect.mWidth));
    rect.mY = viewport[1] + llmin(DEBUG_RECT_Y, viewport[3] - static_cast<S32>(rect.mHeight));
    return true;
}

U32 bucketedTargetExtent(F32 projected_extent, F32 scale)
{
    const U32 requested = llclamp(
        static_cast<U32>(llmax(1, ll_round(projected_extent * scale))),
        MIN_TARGET_EXTENT, MAX_TARGET_EXTENT);
    U32 bucket = MIN_TARGET_EXTENT;
    while (bucket < requested && bucket < MAX_TARGET_EXTENT)
    {
        bucket <<= 1;
    }
    return llmin(bucket, MAX_TARGET_EXTENT);
}

F32 clipDistance(const glm::vec4& p, S32 plane)
{
    switch (plane)
    {
        case 0: return p.x + p.w;
        case 1: return p.w - p.x;
        case 2: return p.y + p.w;
        case 3: return p.w - p.y;
        case 4: return p.z + p.w;
        default: return p.w - p.z;
    }
}

using ClipPolygon = std::vector<glm::vec4>;

void clipPolygonAgainstPlane(ClipPolygon& polygon, ClipPolygon& scratch, S32 plane)
{
    if (polygon.empty())
    {
        return;
    }

    scratch.clear();
    scratch.reserve(polygon.size() + 1);
    glm::vec4 previous = polygon.back();
    F32 previous_distance = clipDistance(previous, plane);
    bool previous_inside = previous_distance >= 0.f;

    for (const glm::vec4& current : polygon)
    {
        const F32 current_distance = clipDistance(current, plane);
        const bool current_inside = current_distance >= 0.f;
        if (current_inside != previous_inside)
        {
            const F32 denominator = previous_distance - current_distance;
            if (fabsf(denominator) > CLIP_EPSILON)
            {
                scratch.push_back(previous + (current - previous) *
                    (previous_distance / denominator));
            }
        }
        if (current_inside)
        {
            scratch.push_back(current);
        }
        previous = current;
        previous_distance = current_distance;
        previous_inside = current_inside;
    }
    polygon.swap(scratch);
}

struct LocalSurfaceBasis
{
    LLVector3 mOrigin;
    LLVector3 mUEdge;
    LLVector3 mVEdge;
};

bool assignRectangularSurfaceFrame(const LLVector3& surface_origin,
                                   const LLVector3& surface_u_edge,
                                   const LLVector3& surface_v_edge,
                                   const LLVector3& world_surface_origin,
                                   const LLVector3& world_surface_u_edge,
                                   const LLVector3& world_surface_v_edge,
                                   PrismFrame& frame,
                                   std::string& reject_reason)
{
    if (!surface_origin.isFinite() || !surface_u_edge.isFinite() ||
        !surface_v_edge.isFinite() || !world_surface_origin.isFinite() ||
        !world_surface_u_edge.isFinite() || !world_surface_v_edge.isFinite())
    {
        reject_reason = "designated face contains non-finite transformed geometry";
        return false;
    }

    const F32 uu = surface_u_edge * surface_u_edge;
    const F32 uv = surface_u_edge * surface_v_edge;
    const F32 vv = surface_v_edge * surface_v_edge;
    const F32 dual_determinant = uu * vv - uv * uv;
    if (!std::isfinite(uu) || !std::isfinite(uv) || !std::isfinite(vv) ||
        uu <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON ||
        vv <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON ||
        dual_determinant <= uu * vv * SURFACE_UV_EPSILON)
    {
        reject_reason = "designated face has a degenerate surface basis";
        return false;
    }

    const F32 world_u_length = world_surface_u_edge.magVec();
    const F32 world_v_length = world_surface_v_edge.magVec();
    if (!std::isfinite(world_u_length) || !std::isfinite(world_v_length) ||
        world_u_length <= F_ALMOST_ZERO || world_v_length <= F_ALMOST_ZERO ||
        fabsf(world_surface_u_edge * world_surface_v_edge) >
            world_u_length * world_v_length * MAX_SURFACE_EDGE_COS)
    {
        reject_reason = "Prism lens surface-fit requires an approximately rectangular face";
        return false;
    }

    frame.mSurfaceOrigin = surface_origin;
    frame.mSurfaceUDual = (surface_u_edge * vv - surface_v_edge * uv) / dual_determinant;
    frame.mSurfaceVDual = (surface_v_edge * uu - surface_u_edge * uv) / dual_determinant;
    frame.mWorldSurfaceOrigin = world_surface_origin;
    frame.mWorldSurfaceUEdge = world_surface_u_edge;
    frame.mWorldSurfaceVEdge = world_surface_v_edge;
    return true;
}

// UV-independent FALLBACK used only when a face's geometric UVs cannot supply
// a trustworthy affine rectangle (typical for mesh faces with atlassed, sheared
// or collapsed UVs). Derives a MINIMUM-AREA oriented bounding rectangle from
// the flat face geometry itself, in SURFACE space: every unique BOUNDARY-edge
// direction (edges used by exactly one triangle -- a quad's shared diagonal is
// interior and never considered) seeds a candidate in-plane U axis, the plane
// normal completes each frame, and the candidate whose vertex extents enclose
// the smallest area wins. The minimum-area rectangle of a convex polygon has a
// side collinear with a hull edge, so a flat quad screen yields its true
// side-aligned rectangle rather than a diagonal-anchored, oversized one.
// Planarity was already enforced by validateSurfaceGeometry, so coplanar input
// is a precondition here; residual out-of-plane slack is flattened by the axis
// projection, never amplified.
bool deriveGeometricRectangularSurface(const LLVolumeFace& volume_face,
                                       const std::vector<LLVector3>& surface_positions,
                                       const LLMatrix4& surface_matrix,
                                       const LLMatrix4& model_matrix,
                                       LocalSurfaceBasis& local_basis,
                                       PrismFrame& frame,
                                       std::string& reject_reason)
{
    if (surface_positions.size() < 3 || !volume_face.mIndices ||
        volume_face.mNumIndices < 3)
    {
        reject_reason = "designated face has too little geometry for a display rectangle";
        return false;
    }

    // Centroid of the surface-space vertex set (positions were already checked
    // finite by the caller's gather loop).
    LLVector3 centroid;
    centroid.setZero();
    for (const LLVector3& position : surface_positions)
    {
        centroid += position;
    }
    centroid *= 1.f / static_cast<F32>(surface_positions.size());

    // Single pass over the index buffer: the surface-space plane normal comes
    // from the first non-degenerate triangle (mirrors the world-space
    // plane-finding loop in validateSurfaceGeometry); an undirected edge-use
    // census identifies BOUNDARY edges (used by exactly one triangle) as the
    // minimum-area OBB axis candidates; and the longest edge is still tracked,
    // purely as the last-resort axis for boundary-less topology (for example a
    // doubled two-sided sheet where every edge is shared by two triangles).
    LLVector3 plane_normal;
    bool found_plane = false;
    LLVector3 longest_edge;
    F32 longest_edge_squared = 0.f;
    // Undirected edge (min index in the high half-word, max in the low) -> the
    // number of triangles that use it. Ordered map so candidate iteration -- and
    // therefore equal-area tie-breaking on symmetric screens -- is deterministic.
    std::map<U32, U32> edge_use_counts;
    for (S32 i = 0; i + 2 < volume_face.mNumIndices; i += 3)
    {
        const U16 ia = volume_face.mIndices[i];
        const U16 ib = volume_face.mIndices[i + 1];
        const U16 ic = volume_face.mIndices[i + 2];
        if (ia >= surface_positions.size() || ib >= surface_positions.size() ||
            ic >= surface_positions.size())
        {
            reject_reason = "designated volume face contains an invalid triangle index";
            return false;
        }
        const LLVector3& pa = surface_positions[ia];
        const LLVector3& pb = surface_positions[ib];
        const LLVector3& pc = surface_positions[ic];
        if (!found_plane)
        {
            LLVector3 normal = (pb - pa) % (pc - pa);
            if (normal.normVec() > F_ALMOST_ZERO)
            {
                plane_normal = normal;
                found_plane = true;
            }
        }
        const U16 corners[3] = { ia, ib, ic };
        for (S32 corner = 0; corner < 3; ++corner)
        {
            const U16 ea = corners[corner];
            const U16 eb = corners[(corner + 1) % 3];
            const U32 edge_key = (static_cast<U32>(llmin(ea, eb)) << 16) |
                                 static_cast<U32>(llmax(ea, eb));
            ++edge_use_counts[edge_key];
        }
        const LLVector3 edges[3] = { pb - pa, pc - pb, pa - pc };
        for (const LLVector3& edge : edges)
        {
            const F32 length_squared = edge.magVecSquared();
            if (length_squared > longest_edge_squared)
            {
                longest_edge_squared = length_squared;
                longest_edge = edge;
            }
        }
    }
    if (!found_plane)
    {
        reject_reason = "designated face is degenerate";
        return false;
    }

    // A seed direction projected into the plane gives U; V = n x U; then U is
    // re-orthonormalized as V x n so tiny planarity slack cannot skew the
    // pair. Every normalization is guarded -- no divide by zero, no NaN.
    const auto build_plane_axes = [&plane_normal](LLVector3 seed,
                                                  LLVector3& u_axis,
                                                  LLVector3& v_axis) -> bool
    {
        u_axis = seed - plane_normal * (seed * plane_normal);
        if (u_axis.normVec() <= F_ALMOST_ZERO)
        {
            return false;
        }
        v_axis = plane_normal % u_axis;
        if (v_axis.normVec() <= F_ALMOST_ZERO)
        {
            return false;
        }
        u_axis = v_axis % plane_normal;
        return u_axis.normVec() > F_ALMOST_ZERO;
    };

    // Vertex extents about the centroid along an oriented axis pair.
    const auto project_extents = [&surface_positions, &centroid](
        const LLVector3& u_axis, const LLVector3& v_axis,
        F32& u_min, F32& u_max, F32& v_min, F32& v_max)
    {
        u_min = std::numeric_limits<F32>::max();
        u_max = -std::numeric_limits<F32>::max();
        v_min = std::numeric_limits<F32>::max();
        v_max = -std::numeric_limits<F32>::max();
        for (const LLVector3& position : surface_positions)
        {
            const LLVector3 offset = position - centroid;
            const F32 u = offset * u_axis;
            const F32 v = offset * v_axis;
            u_min = llmin(u_min, u);
            u_max = llmax(u_max, u);
            v_min = llmin(v_min, v);
            v_max = llmax(v_max, v);
        }
    };

    // Minimum-area oriented bounding rectangle, restricted to axes parallel to
    // boundary edges. Interior edges (a quad's shared diagonal is used by two
    // triangles) never become candidates, so the canonical 2-triangle screen
    // quad recovers its true side-aligned rectangle instead of the rotated,
    // oversized one its diagonal used to anchor.
    LLVector3 best_u_axis;
    LLVector3 best_v_axis;
    F32 best_u_min = 0.f;
    F32 best_u_max = 0.f;
    F32 best_v_min = 0.f;
    F32 best_v_max = 0.f;
    F32 best_area = std::numeric_limits<F32>::max();
    bool found_candidate = false;
    std::vector<LLVector3> tried_axes;
    for (const auto& edge_use : edge_use_counts)
    {
        if (edge_use.second != 1)
        {
            continue; // interior or non-manifold edge -- not a boundary side
        }
        const U16 ea = static_cast<U16>(edge_use.first >> 16);
        const U16 eb = static_cast<U16>(edge_use.first & 0xFFFFu);
        const LLVector3 edge_vector = surface_positions[eb] - surface_positions[ea];
        // In-plane direction of this boundary edge (degenerate edges and edges
        // perpendicular to the plane project to nothing and are skipped).
        LLVector3 direction =
            edge_vector - plane_normal * (edge_vector * plane_normal);
        if (direction.normVec() <= F_ALMOST_ZERO)
        {
            continue;
        }
        // Undirected duplicate-direction skip: parallel boundary edges (the
        // opposite sides of a quad) would re-derive the identical rectangle.
        bool duplicate = false;
        for (const LLVector3& tried : tried_axes)
        {
            if (fabsf(tried * direction) >= OBB_PARALLEL_AXIS_COS)
            {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
        {
            continue;
        }
        tried_axes.push_back(direction);

        LLVector3 u_cand;
        LLVector3 v_cand;
        if (!build_plane_axes(direction, u_cand, v_cand))
        {
            continue;
        }
        F32 u_min = 0.f;
        F32 u_max = 0.f;
        F32 v_min = 0.f;
        F32 v_max = 0.f;
        project_extents(u_cand, v_cand, u_min, u_max, v_min, v_max);
        const F32 area = (u_max - u_min) * (v_max - v_min);
        if (!std::isfinite(area))
        {
            continue;
        }
        if (!found_candidate || area < best_area)
        {
            found_candidate = true;
            best_area = area;
            best_u_axis = u_cand;
            best_v_axis = v_cand;
            best_u_min = u_min;
            best_u_max = u_max;
            best_v_min = v_min;
            best_v_max = v_max;
        }
    }

    if (!found_candidate)
    {
        // Degenerate boundary topology (no edge used by exactly one triangle,
        // or every boundary edge projected to nothing): fall back to the
        // previous longest-edge behavior so such faces keep working, with the
        // same guards -- never NaN, never divide-by-zero.
        if (longest_edge_squared <= F_ALMOST_ZERO * F_ALMOST_ZERO)
        {
            reject_reason = "designated face has no usable edge for a display axis";
            return false;
        }
        if (!build_plane_axes(longest_edge, best_u_axis, best_v_axis))
        {
            reject_reason = "designated face has a degenerate in-plane axis frame";
            return false;
        }
        project_extents(best_u_axis, best_v_axis,
                        best_u_min, best_u_max, best_v_min, best_v_max);
    }

    const LLVector3 surface_origin =
        centroid + best_u_axis * best_u_min + best_v_axis * best_v_min;
    const LLVector3 surface_u_edge = best_u_axis * (best_u_max - best_u_min);
    const LLVector3 surface_v_edge = best_v_axis * (best_v_max - best_v_min);

    // WORLD basis: transform the surface-space frame exactly the way the
    // cached replay path does -- corner points through the full transform,
    // edges as differences of transformed points.
    const LLVector3 world_surface_origin = surface_origin * model_matrix;
    const LLVector3 world_surface_u_edge =
        (surface_origin + surface_u_edge) * model_matrix - world_surface_origin;
    const LLVector3 world_surface_v_edge =
        (surface_origin + surface_v_edge) * model_matrix - world_surface_origin;

    // LOCAL basis: map the frame back through inverse(surface_matrix) so the
    // surface-geometry cache can replay it against a fresh relative transform.
    // LLMatrix4::invert() only handles rotation+translation and the relative
    // xform bakes scale in, so use a full glm inverse with a determinant guard.
    const glm::mat4 surface_glm = glm::make_mat4(&surface_matrix.mMatrix[0][0]);
    const F32 surface_determinant = glm::determinant(surface_glm);
    if (!std::isfinite(surface_determinant) ||
        fabsf(surface_determinant) <= SURFACE_MATRIX_MIN_DETERMINANT)
    {
        reject_reason = "designated face's surface transform is not invertible";
        return false;
    }
    const glm::mat4 local_from_surface = glm::inverse(surface_glm);
    bool local_valid = true;
    const auto to_local =
        [&local_from_surface, &local_valid](const LLVector3& point)
    {
        const glm::vec4 local = local_from_surface *
            glm::vec4(point.mV[VX], point.mV[VY], point.mV[VZ], 1.f);
        if (!std::isfinite(local.w) || fabsf(local.w) <= F_ALMOST_ZERO)
        {
            local_valid = false;
            return LLVector3();
        }
        return LLVector3(local.x / local.w, local.y / local.w, local.z / local.w);
    };
    local_basis.mOrigin = to_local(surface_origin);
    local_basis.mUEdge =
        to_local(surface_origin + surface_u_edge) - local_basis.mOrigin;
    local_basis.mVEdge =
        to_local(surface_origin + surface_v_edge) - local_basis.mOrigin;
    if (!local_valid || !local_basis.mOrigin.isFinite() ||
        !local_basis.mUEdge.isFinite() || !local_basis.mVEdge.isFinite())
    {
        reject_reason = "designated face contains a non-finite local surface basis";
        return false;
    }

    // The shared frame checks (finiteness, non-degenerate edges, world-edge
    // orthogonality) still gate the fallback result.
    return assignRectangularSurfaceFrame(
        surface_origin, surface_u_edge, surface_v_edge,
        world_surface_origin, world_surface_u_edge, world_surface_v_edge,
        frame, reject_reason);
}

bool deriveRectangularSurface(const LLVolumeFace& volume_face,
                              const std::vector<LLVector3>& surface_positions,
                              const std::vector<LLVector3>& world_positions,
                              const LLMatrix4& surface_matrix,
                              const LLMatrix4& model_matrix,
                              F32 fit_tolerance,
                              LocalSurfaceBasis& local_basis,
                              PrismFrame& frame,
                              std::string& reject_reason)
{
    // Structural sanity: the caller-built position mirrors must match the
    // volume face's vertex count no matter which derivation path runs below.
    if (surface_positions.size() != static_cast<size_t>(volume_face.mNumVertices) ||
        world_positions.size() != surface_positions.size())
    {
        reject_reason = "designated face vertex mirrors are inconsistent";
        return false;
    }

    // PRIMARY path: use the volume face's raw geometric UVs to derive an affine
    // position basis. The shared VB texcoord0 stream is deliberately not
    // consumed because it may contain TE repeat/offset/rotation or animated
    // texture transforms. Every reject below that stems from UV QUALITY --
    // missing, non-finite, degenerate, non-spanning, or non-affine UVs -- now
    // routes to the UV-independent geometry fallback instead of refusing the
    // face outright (this is what admits typical mesh display faces). Faces
    // whose UVs pass the affine fit -- all prim box faces and cleanly mapped
    // mesh screens -- never reach the fallback and keep the historical
    // UV-derived basis bit-for-bit.
    const auto geometry_fallback = [&]() -> bool
    {
        return deriveGeometricRectangularSurface(
            volume_face, surface_positions, surface_matrix, model_matrix,
            local_basis, frame, reject_reason);
    };
    if (!volume_face.mTexCoords)
    {
        return geometry_fallback();
    }

    F32 uv_min_x = std::numeric_limits<F32>::max();
    F32 uv_min_y = std::numeric_limits<F32>::max();
    F32 uv_max_x = -std::numeric_limits<F32>::max();
    F32 uv_max_y = -std::numeric_limits<F32>::max();
    for (S32 i = 0; i < volume_face.mNumVertices; ++i)
    {
        const LLVector2& uv = volume_face.mTexCoords[i];
        if (!std::isfinite(uv.mV[VX]) || !std::isfinite(uv.mV[VY]))
        {
            return geometry_fallback();
        }
        uv_min_x = llmin(uv_min_x, uv.mV[VX]);
        uv_min_y = llmin(uv_min_y, uv.mV[VY]);
        uv_max_x = llmax(uv_max_x, uv.mV[VX]);
        uv_max_y = llmax(uv_max_y, uv.mV[VY]);
    }

    const F32 uv_width = uv_max_x - uv_min_x;
    const F32 uv_height = uv_max_y - uv_min_y;
    if (uv_width <= SURFACE_UV_EPSILON || uv_height <= SURFACE_UV_EPSILON)
    {
        return geometry_fallback();
    }

    S32 basis_b = -1;
    S32 basis_c = -1;
    const LLVector2 uv_a = volume_face.mTexCoords[0];
    for (S32 i = 1; i < volume_face.mNumVertices && basis_c < 0; ++i)
    {
        const LLVector2 delta_b = volume_face.mTexCoords[i] - uv_a;
        if (delta_b.magVecSquared() <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON)
        {
            continue;
        }
        for (S32 j = i + 1; j < volume_face.mNumVertices; ++j)
        {
            const LLVector2 delta_c = volume_face.mTexCoords[j] - uv_a;
            const F32 determinant = delta_b.mV[VX] * delta_c.mV[VY] -
                                    delta_b.mV[VY] * delta_c.mV[VX];
            if (fabsf(determinant) > SURFACE_UV_EPSILON)
            {
                basis_b = i;
                basis_c = j;
                break;
            }
        }
    }
    if (basis_b < 0 || basis_c < 0)
    {
        return geometry_fallback();
    }

    const LLVector2 delta_b = volume_face.mTexCoords[basis_b] - uv_a;
    const LLVector2 delta_c = volume_face.mTexCoords[basis_c] - uv_a;
    const F32 determinant = delta_b.mV[VX] * delta_c.mV[VY] -
                            delta_b.mV[VY] * delta_c.mV[VX];
    const LLVector3 position_delta_b = surface_positions[basis_b] - surface_positions[0];
    const LLVector3 position_delta_c = surface_positions[basis_c] - surface_positions[0];
    const LLVector3 position_per_u =
        (position_delta_b * delta_c.mV[VY] - position_delta_c * delta_b.mV[VY]) /
        determinant;
    const LLVector3 position_per_v =
        (position_delta_c * delta_b.mV[VX] - position_delta_b * delta_c.mV[VX]) /
        determinant;
    const LLVector3 raw_uv_origin = surface_positions[0] -
        position_per_u * uv_a.mV[VX] - position_per_v * uv_a.mV[VY];
    const LLVector3 surface_origin = raw_uv_origin +
        position_per_u * uv_min_x + position_per_v * uv_min_y;
    const LLVector3 surface_u_edge = position_per_u * uv_width;
    const LLVector3 surface_v_edge = position_per_v * uv_height;
    const LLVector3 world_delta_b = world_positions[basis_b] - world_positions[0];
    const LLVector3 world_delta_c = world_positions[basis_c] - world_positions[0];
    const LLVector3 world_per_u =
        (world_delta_b * delta_c.mV[VY] - world_delta_c * delta_b.mV[VY]) /
        determinant;
    const LLVector3 world_per_v =
        (world_delta_c * delta_b.mV[VX] - world_delta_b * delta_c.mV[VX]) /
        determinant;
    const LLVector3 world_raw_uv_origin = world_positions[0] -
        world_per_u * uv_a.mV[VX] - world_per_v * uv_a.mV[VY];
    const LLVector3 world_surface_origin = world_raw_uv_origin +
        world_per_u * uv_min_x + world_per_v * uv_min_y;
    const LLVector3 world_surface_u_edge = world_per_u * uv_width;
    const LLVector3 world_surface_v_edge = world_per_v * uv_height;

    // Affine-fit DECISION: the surface basis above is derived from only three
    // vertices (0, basis_b, basis_c). Confirm that affine UV->position map
    // predicts EVERY vertex within tolerance; otherwise a non-affine / atlassed /
    // sheared-UV mesh face would silently map the feed skewed onto the surface.
    // A failed fit is no longer a hard reject: such faces fall back to the
    // UV-independent oriented bounding quad instead, so a flat mesh face with
    // arbitrary UVs still yields a valid display rectangle. A passing fit keeps
    // the UV-derived basis exactly as before.
    for (S32 i = 0; i < volume_face.mNumVertices; ++i)
    {
        const LLVector2& uv = volume_face.mTexCoords[i];
        const F32 normalized_u = (uv.mV[VX] - uv_min_x) / uv_width;
        const F32 normalized_v = (uv.mV[VY] - uv_min_y) / uv_height;
        const LLVector3 predicted = surface_origin +
            surface_u_edge * normalized_u + surface_v_edge * normalized_v;
        const LLVector3 predicted_world = world_surface_origin +
            world_surface_u_edge * normalized_u + world_surface_v_edge * normalized_v;
        if ((predicted - surface_positions[i]).magVec() > fit_tolerance ||
            (predicted_world - world_positions[i]).magVec() > fit_tolerance)
        {
            return geometry_fallback();
        }
    }

    const LLVector3 local_a(volume_face.mPositions[0].getF32ptr());
    const LLVector3 local_delta_b =
        LLVector3(volume_face.mPositions[basis_b].getF32ptr()) - local_a;
    const LLVector3 local_delta_c =
        LLVector3(volume_face.mPositions[basis_c].getF32ptr()) - local_a;
    const LLVector3 local_per_u =
        (local_delta_b * delta_c.mV[VY] - local_delta_c * delta_b.mV[VY]) /
        determinant;
    const LLVector3 local_per_v =
        (local_delta_c * delta_b.mV[VX] - local_delta_b * delta_c.mV[VX]) /
        determinant;
    const LLVector3 local_raw_uv_origin = local_a -
        local_per_u * uv_a.mV[VX] - local_per_v * uv_a.mV[VY];
    local_basis.mOrigin = local_raw_uv_origin +
        local_per_u * uv_min_x + local_per_v * uv_min_y;
    local_basis.mUEdge = local_per_u * uv_width;
    local_basis.mVEdge = local_per_v * uv_height;
    if (!local_basis.mOrigin.isFinite() || !local_basis.mUEdge.isFinite() ||
        !local_basis.mVEdge.isFinite())
    {
        reject_reason = "designated face contains a non-finite local surface basis";
        return false;
    }

    return assignRectangularSurfaceFrame(
        surface_origin, surface_u_edge, surface_v_edge,
        world_surface_origin, world_surface_u_edge, world_surface_v_edge,
        frame, reject_reason);
}

bool selectedFaceIdentity(LLUUID& object_id, S32& te,
                          std::string* reject_reason = nullptr,
                          LLVOVolume** selected_volume = nullptr)
{
    if (selected_volume)
    {
        *selected_volume = nullptr;
    }
    const auto reject = [reject_reason](const char* reason)
    {
        if (reject_reason)
        {
            *reject_reason = reason;
        }
        return false;
    };

    auto selection = LLSelectMgr::getInstance()->getSelection();
    if (selection.isNull())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }

    LLViewerObject* object = nullptr;
    LLSelectNode* node = nullptr;
    U32 selected_object_count = 0;
    U32 selected_face_count = 0;
    // Do not use valid_begin(): mValid only means the asynchronous object-
    // properties reply has arrived. Excluding not-yet-valid nodes can turn a
    // real multi-object selection into an apparently valid single-face one.
    for (auto iter = selection->begin(); iter != selection->end(); ++iter)
    {
        LLSelectNode* candidate_node = *iter;
        LLViewerObject* candidate_object = candidate_node ? candidate_node->getObject() : nullptr;
        if (!candidate_object)
        {
            return reject("selection contains an unavailable object; try again when it finishes loading");
        }
        ++selected_object_count;
        if (selected_object_count > 1)
        {
            return reject("select exactly one face; multi-object or multi-face selections are ambiguous");
        }
        for (S32 candidate_te = 0; candidate_te < candidate_object->getNumTEs(); ++candidate_te)
        {
            if (!candidate_node->isTESelected(candidate_te))
            {
                continue;
            }
            ++selected_face_count;
            object = candidate_object;
            node = candidate_node;
            te = candidate_te;
            if (selected_face_count > 1)
            {
                return reject("select exactly one face; multi-object or multi-face selections are ambiguous");
            }
        }
    }

    if (selected_object_count != 1 || selected_face_count != 1 ||
        !object || !node || object->isDead() ||
        object->isHUDAttachment())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }

    LLVOVolume* volume_object = dynamic_cast<LLVOVolume*>(object);
    LLDrawable* drawable = object->mDrawable.get();
    LLVolume* volume = volume_object ? volume_object->getVolume() : nullptr;
    if (!volume_object || te < 0 || te >= object->getNumTEs() ||
        !drawable || te >= drawable->getNumFaces() ||
        !volume || te >= volume->getNumVolumeFaces())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }

    LLFace* face = drawable->getFace(te);
    if (!face)
    {
        return reject("select exactly one valid, non-HUD volume face");
    }
    if (volume_object->isRiggedMesh() || volume_object->isAnimatedObject() ||
        face->isState(LLFace::RIGGED))
    {
        return reject("requires a static, flat face (prim or mesh)");
    }

    object_id = object->getID();
    if (object_id.isNull())
    {
        return reject("select exactly one valid, non-HUD volume face");
    }
    if (selected_volume)
    {
        *selected_volume = volume_object;
    }
    return true;
}

bool selectedObjectIdentity(LLUUID& object_id,
                            std::string* reject_reason = nullptr)
{
    const auto reject = [reject_reason](const char* reason)
    {
        if (reject_reason)
        {
            *reject_reason = reason;
        }
        return false;
    };

    auto selection = LLSelectMgr::getInstance()->getSelection();
    if (selection.isNull())
    {
        return reject("select exactly one valid, non-HUD object");
    }

    LLViewerObject* selected = nullptr;
    U32 object_count = 0;
    for (auto iter = selection->begin(); iter != selection->end(); ++iter)
    {
        LLSelectNode* node = *iter;
        LLViewerObject* object = node ? node->getObject() : nullptr;
        if (!object)
        {
            return reject("selection contains an unavailable object; try again when it finishes loading");
        }
        if (++object_count > 1)
        {
            return reject("select exactly one object for the virtual camera");
        }
        selected = object;
    }

    if (object_count != 1 || !selected || selected->isDead() ||
        selected->isHUDAttachment() || selected->getID().isNull())
    {
        return reject("select exactly one valid, non-HUD object");
    }

    object_id = selected->getID();
    if (reject_reason)
    {
        reject_reason->clear();
    }
    return true;
}

enum class ESurfaceValidation
{
    VALID,
    TRANSIENT,
    INVALID
};

struct SurfaceGeometry
{
    LLFace* mFace = nullptr;
    LLVector3 mCenter;
    LLVector3 mPlaneNormal;
};

struct SurfaceGeometryCache
{
    bool mValid = false;
    const LLVOVolume* mObject = nullptr;
    const LLVolume* mVolume = nullptr;
    const LLVector4a* mPositions = nullptr;
    const LLVector2* mTexCoords = nullptr;
    const U16* mIndices = nullptr;
    S32 mTextureEntry = -1;
    S32 mNumVertices = 0;
    S32 mNumIndices = 0;
    S32 mSculptLevel = -1;
    F32 mDetail = 0.f;
    F64 mValidatedAt = 0.0;
    F64 mRevalidateAt = 0.0;
    LocalSurfaceBasis mLocalBasis;
    F32 mPlaneToUvSign = 1.f;
    std::vector<LLVector3> mSurfacePositions;
    std::vector<LLVector3> mWorldPositions;

    bool matches(const LLVOVolume* object, const LLVolume* volume,
                 const LLVolumeFace& face, S32 te, F64 now) const
    {
        return mValid && mObject == object && mVolume == volume &&
            mPositions == face.mPositions && mTexCoords == face.mTexCoords &&
            mIndices == face.mIndices && mTextureEntry == te &&
            mNumVertices == face.mNumVertices && mNumIndices == face.mNumIndices &&
            mSculptLevel == volume->getSculptLevel() && mDetail == volume->getDetail() &&
            now >= mValidatedAt && now < mRevalidateAt;
    }
};

bool applyCachedSurfaceGeometry(const SurfaceGeometryCache& cache,
                                const LLMatrix4& surface_matrix,
                                const LLMatrix4& model_matrix,
                                LLFace* face,
                                PrismFrame& frame,
                                SurfaceGeometry& geometry,
                                std::string& reject_reason)
{
    const LLVector3 local_origin = cache.mLocalBasis.mOrigin;
    const LLVector3 local_u_end = local_origin + cache.mLocalBasis.mUEdge;
    const LLVector3 local_v_end = local_origin + cache.mLocalBasis.mVEdge;
    const LLVector3 surface_origin = local_origin * surface_matrix;
    const LLVector3 surface_u_edge = local_u_end * surface_matrix - surface_origin;
    const LLVector3 surface_v_edge = local_v_end * surface_matrix - surface_origin;
    const LLVector3 world_origin = surface_origin * model_matrix;
    const LLVector3 world_u_edge =
        (surface_origin + surface_u_edge) * model_matrix - world_origin;
    const LLVector3 world_v_edge =
        (surface_origin + surface_v_edge) * model_matrix - world_origin;

    if (!assignRectangularSurfaceFrame(
            surface_origin, surface_u_edge, surface_v_edge,
            world_origin, world_u_edge, world_v_edge, frame, reject_reason))
    {
        return false;
    }

    LLVector3 plane_normal = world_u_edge % world_v_edge;
    if (plane_normal.normVec() <= F_ALMOST_ZERO)
    {
        reject_reason = "designated face has a degenerate transformed plane";
        return false;
    }
    if (cache.mPlaneToUvSign < 0.f)
    {
        plane_normal = -plane_normal;
    }

    geometry.mFace = face;
    geometry.mCenter = world_origin + (world_u_edge + world_v_edge) * 0.5f;
    geometry.mPlaneNormal = plane_normal;
    return true;
}

// Resolve and validate the camera-independent part of a lens face. Keeping this
// in one path ensures the Add button cannot claim success for geometry that the
// renderer would immediately reject (or retain forever while the master is off).
ESurfaceValidation validateSurfaceGeometry(
    LLVOVolume* object, S32 te, PrismFrame& frame,
    SurfaceGeometryCache& cache,
    SurfaceGeometry& geometry, std::string& reject_reason)
{
    geometry = SurfaceGeometry();
    reject_reason.clear();
    if (!object || object->isDead())
    {
        reject_reason = "selected object is no longer available";
        return ESurfaceValidation::TRANSIENT;
    }

    LLDrawable* drawable = object->mDrawable.get();
    LLVolume* volume = object->getVolume();
    if (!drawable || !volume)
    {
        reject_reason = "selected face geometry is still loading; try again";
        return ESurfaceValidation::TRANSIENT;
    }
    if (te < 0 || te >= object->getNumTEs() ||
        te >= drawable->getNumFaces() || te >= volume->getNumVolumeFaces())
    {
        reject_reason = "invalid texture-entry/face index";
        return ESurfaceValidation::INVALID;
    }

    LLFace* face = drawable->getFace(te);
    if (!face || face->getTEOffset() != te || !face->hasGeometry() ||
        !face->getVertexBuffer() || face->getIndicesCount() < 3)
    {
        reject_reason = "selected face geometry is still loading; try again";
        return ESurfaceValidation::TRANSIENT;
    }
    if (object->isRiggedMesh() || object->isAnimatedObject() ||
        face->isState(LLFace::RIGGED))
    {
        reject_reason = "requires a static, flat face (prim or mesh)";
        return ESurfaceValidation::INVALID;
    }

    const LLVolumeFace& volume_face = volume->getVolumeFace(te);
    if (!volume_face.mPositions || !volume_face.mIndices ||
        volume_face.mNumVertices < 3 || volume_face.mNumIndices < 3)
    {
        reject_reason = "designated volume face has no triangle geometry";
        return ESurfaceValidation::INVALID;
    }

    LLMatrix4 surface_matrix = object->getRelativeXform();
    if (drawable->isState(LLDrawable::ANIMATED_CHILD))
    {
        // Animated-child VBs are rebuilt with force_identity=true: scale is
        // baked into position, while the drawable world matrix is applied at draw.
        surface_matrix.initScale(object->getScale());
    }
    const LLMatrix4* model_matrix = nullptr;
    if (drawable->isState(LLDrawable::ANIMATED_CHILD))
    {
        model_matrix = &drawable->getWorldMatrix();
    }
    else if (drawable->isActive())
    {
        model_matrix = &drawable->getRenderMatrix();
    }
    else if (drawable->getRegion())
    {
        model_matrix = &drawable->getRegion()->mRenderMatrix;
    }
    if (!model_matrix)
    {
        reject_reason = "selected face transform is still loading; try again";
        return ESurfaceValidation::TRANSIENT;
    }

    const F64 now = LLTimer::getTotalSeconds();
    if (cache.matches(object, volume, volume_face, te, now))
    {
        if (applyCachedSurfaceGeometry(cache, surface_matrix, *model_matrix,
                                       face, frame, geometry, reject_reason))
        {
            return ESurfaceValidation::VALID;
        }
        return ESurfaceValidation::INVALID;
    }

    // Pointer/count/sculpt-level checks catch normal LOD and asset replacement.
    // The bounded periodic revalidation also catches rare in-place volume edits
    // without paying an O(vertices + triangles) scan on every display every frame.
    cache.mValid = false;
    std::vector<LLVector3>& surface_positions = cache.mSurfacePositions;
    std::vector<LLVector3>& world_positions = cache.mWorldPositions;
    surface_positions.clear();
    surface_positions.reserve(volume_face.mNumVertices);
    world_positions.clear();
    world_positions.reserve(volume_face.mNumVertices);
    LLVector3 center;
    center.setZero();
    LLVector3 min_corner(std::numeric_limits<F32>::max(),
                         std::numeric_limits<F32>::max(),
                         std::numeric_limits<F32>::max());
    LLVector3 max_corner(-std::numeric_limits<F32>::max(),
                         -std::numeric_limits<F32>::max(),
                         -std::numeric_limits<F32>::max());
    for (S32 i = 0; i < volume_face.mNumVertices; ++i)
    {
        const LLVector3 surface =
            LLVector3(volume_face.mPositions[i].getF32ptr()) * surface_matrix;
        const LLVector3 world = surface * (*model_matrix);
        if (!surface.isFinite() || !world.isFinite())
        {
            reject_reason = "designated face contains non-finite geometry";
            return ESurfaceValidation::INVALID;
        }
        surface_positions.push_back(surface);
        world_positions.push_back(world);
        center += world;
        min_corner.setVec(llmin(min_corner.mV[VX], world.mV[VX]),
                          llmin(min_corner.mV[VY], world.mV[VY]),
                          llmin(min_corner.mV[VZ], world.mV[VZ]));
        max_corner.setVec(llmax(max_corner.mV[VX], world.mV[VX]),
                          llmax(max_corner.mV[VY], world.mV[VY]),
                          llmax(max_corner.mV[VZ], world.mV[VZ]));
    }
    center *= 1.f / static_cast<F32>(world_positions.size());

    LLVector3 plane_normal;
    bool found_plane = false;
    for (S32 i = 0; i + 2 < volume_face.mNumIndices; i += 3)
    {
        const U16 ia = volume_face.mIndices[i];
        const U16 ib = volume_face.mIndices[i + 1];
        const U16 ic = volume_face.mIndices[i + 2];
        if (ia >= world_positions.size() || ib >= world_positions.size() ||
            ic >= world_positions.size())
        {
            reject_reason = "designated volume face contains an invalid triangle index";
            return ESurfaceValidation::INVALID;
        }
        LLVector3 normal = (world_positions[ib] - world_positions[ia]) %
                           (world_positions[ic] - world_positions[ia]);
        if (normal.normVec() > F_ALMOST_ZERO)
        {
            plane_normal = normal;
            found_plane = true;
            break;
        }
    }
    if (!found_plane)
    {
        reject_reason = "designated face is degenerate";
        return ESurfaceValidation::INVALID;
    }

    const F32 diagonal = (max_corner - min_corner).magVec();
    const F32 planar_tolerance = llmax(PLANAR_ABSOLUTE_EPSILON,
                                       diagonal * PLANAR_RELATIVE_EPSILON);
    for (const LLVector3& position : world_positions)
    {
        if (fabsf((position - center) * plane_normal) > planar_tolerance)
        {
            reject_reason = "designated face is materially non-planar";
            return ESurfaceValidation::INVALID;
        }
    }
    for (S32 i = 0; i + 2 < volume_face.mNumIndices; i += 3)
    {
        const U16 ia = volume_face.mIndices[i];
        const U16 ib = volume_face.mIndices[i + 1];
        const U16 ic = volume_face.mIndices[i + 2];
        if (ia >= world_positions.size() || ib >= world_positions.size() ||
            ic >= world_positions.size())
        {
            reject_reason = "designated volume face contains an invalid triangle index";
            return ESurfaceValidation::INVALID;
        }
        LLVector3 normal = (world_positions[ib] - world_positions[ia]) %
                           (world_positions[ic] - world_positions[ia]);
        if (normal.normVec() > F_ALMOST_ZERO &&
            fabsf(normal * plane_normal) < PLANAR_NORMAL_COS)
        {
            reject_reason = "designated face triangle normals are non-planar";
            return ESurfaceValidation::INVALID;
        }
    }

    LocalSurfaceBasis local_basis;
    if (!deriveRectangularSurface(volume_face, surface_positions, world_positions,
                                  surface_matrix, *model_matrix,
                                  planar_tolerance, local_basis, frame, reject_reason))
    {
        return ESurfaceValidation::INVALID;
    }

    LLVector3 uv_plane_normal =
        frame.mWorldSurfaceUEdge % frame.mWorldSurfaceVEdge;
    if (uv_plane_normal.normVec() <= F_ALMOST_ZERO)
    {
        reject_reason = "designated face has a degenerate UV plane";
        return ESurfaceValidation::INVALID;
    }

    cache.mObject = object;
    cache.mVolume = volume;
    cache.mPositions = volume_face.mPositions;
    cache.mTexCoords = volume_face.mTexCoords;
    cache.mIndices = volume_face.mIndices;
    cache.mTextureEntry = te;
    cache.mNumVertices = volume_face.mNumVertices;
    cache.mNumIndices = volume_face.mNumIndices;
    cache.mSculptLevel = volume->getSculptLevel();
    cache.mDetail = volume->getDetail();
    cache.mValidatedAt = now;
    const U32 revalidation_phase =
        (object->getID().getCRC32() ^ static_cast<U32>(te * 2654435761u)) & 1023u;
    cache.mRevalidateAt = now + SURFACE_CACHE_REVALIDATE_SECONDS +
        SURFACE_CACHE_REVALIDATE_JITTER_SECONDS *
            (static_cast<F64>(revalidation_phase) / 1024.0);
    cache.mLocalBasis = local_basis;
    cache.mPlaneToUvSign = plane_normal * uv_plane_normal < 0.f ? -1.f : 1.f;
    cache.mValid = true;

    geometry.mFace = face;
    geometry.mCenter = frame.mWorldSurfaceOrigin +
        (frame.mWorldSurfaceUEdge + frame.mWorldSurfaceVEdge) * 0.5f;
    geometry.mPlaneNormal = plane_normal;
    return ESurfaceValidation::VALID;
}

struct PrismInstance
{
    bool mOccupied = false;
    LLPrismLens::CaptureHandle mHandle;
    LLPrismLens::ECaptureMode mMode = LLPrismLens::ECaptureMode::SURFACE_LENS;
    // Lens: source and destination are mObjectId/mTE. Camera Feed: the source
    // object is mCameraObjectId and destinations live exclusively in bindings.
    LLUUID mObjectId;
    S32 mTE = -1;
    LLUUID mCameraObjectId;
    LLPrismLens::CameraSettings mCamera;
    LLPrismLens::CaptureRateSettings mRate;
    LLPrismLens::CaptureRuntimeState mRuntime;
    PrismFrame mFrame;
    U32 mDisplayCount = 0;
    bool mAnyDisplayVisible = false;
    bool mHasOutput = false;
    U32 mOutputWidth = 0;
    U32 mOutputHeight = 0;
    U32 mLastRenderedFrame = 0;
    F64 mRetryAfterTime = 0.0;
    F64 mLastAttemptTime = 0.0;
    F64 mLastProducedTime = 0.0;
    F64 mNextDueTime = 0.0;
    U32 mPublicationSamples = 0;
    F64 mPublicationWindowStart = 0.0;
    F32 mRequestedHz = 0.f;
    F32 mEntitlementHz = 0.f;
    F32 mOutputUvScale[2] = { 1.f, 1.f };
    F32 mOutputUvOffset[2] = { 0.f, 0.f };
    SurfaceGeometryCache mSurfaceCache;
};

struct PrismDisplay
{
    bool mOccupied = false;
    LLPrismLens::DisplayHandle mHandle;
    U32 mCaptureSlot = LLPrismLens::MAX_CAPTURES;
    U64 mCaptureGeneration = 0;
    LLUUID mObjectId;
    S32 mTE = -1;
    LLPrismLens::DisplaySettings mSettings;
    LLPrismLens::DisplayRuntimeState mRuntime;
    PrismFrame mFrame;
    SurfaceGeometryCache mSurfaceCache;
};

class PrismLensRegistry
{
public:
    static PrismLensRegistry& instance()
    {
        static PrismLensRegistry registry;
        return registry;
    }

    LLPrismLens::ActionStatus cameraSelectionStatus(
        const LLPrismLens::CaptureHandle* replacing = nullptr) const
    {
        LLPrismLens::ActionStatus status;
        if (!replacing && count() >= LLPrismLens::MAX_CAPTURES)
        {
            status.mResult = LLPrismLens::ERegistryResult::AT_CAPACITY;
            status.mReason = "Maximum of 3 Prism captures reached.";
            return status;
        }
        if (replacing && findCapture(*replacing) < 0)
        {
            status.mResult = LLPrismLens::ERegistryResult::STALE_HANDLE;
            status.mReason = "That capture no longer exists.";
            return status;
        }
        if (replacing && mLenses[findCapture(*replacing)].mMode !=
                LLPrismLens::ECaptureMode::CAMERA_FEED)
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
            status.mReason = "Only Camera Feed captures can be rebound to a camera source.";
            return status;
        }

        LLUUID object_id;
        if (!selectedObjectIdentity(object_id, &status.mReason))
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_SELECTION;
            return status;
        }
        LLViewerObject* object = gObjectList.findObject(object_id);
        LLVOVolume* volume = object ? dynamic_cast<LLVOVolume*>(object) : nullptr;
        if (!volume || volume->isRiggedMesh() || volume->isAnimatedObject() ||
            !object->getRenderPosition().isFinite() ||
            !object->getRenderRotation().isFinite())
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_SELECTION;
            status.mReason = "Camera source must be a finite, non-rigged volume object.";
            return status;
        }
        if (replacing)
        {
            const S32 capture_slot = findCapture(*replacing);
            for (const PrismDisplay& display : mDisplays)
            {
                if (display.mOccupied && display.mCaptureSlot == static_cast<U32>(capture_slot) &&
                    display.mObjectId == object_id)
                {
                    status.mResult = LLPrismLens::ERegistryResult::DUPLICATE;
                    status.mReason = "A camera source cannot also be one of its display objects.";
                    return status;
                }
            }
        }
        status.mResult = LLPrismLens::ERegistryResult::OK;
        status.mReason.clear();
        return status;
    }

    LLPrismLens::ActionStatus lensSelectionStatus() const
    {
        LLPrismLens::ActionStatus status;
        if (count() >= LLPrismLens::MAX_CAPTURES)
        {
            status.mResult = LLPrismLens::ERegistryResult::AT_CAPACITY;
            status.mReason = "Maximum of 3 Prism captures reached.";
            return status;
        }
        if (displayCount() >= LLPrismLens::MAX_DISPLAY_BINDINGS)
        {
            status.mResult = LLPrismLens::ERegistryResult::AT_CAPACITY;
            status.mReason = "Maximum of 16 Prism display faces reached.";
            return status;
        }
        LLUUID object_id;
        S32 te = -1;
        LLVOVolume* volume = nullptr;
        if (!selectedFaceIdentity(object_id, te, &status.mReason, &volume))
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_SELECTION;
            return status;
        }
        if (findDisplayIdentity(object_id, te) >= 0)
        {
            status.mResult = LLPrismLens::ERegistryResult::DUPLICATE;
            status.mReason = "That face is already a Prism display.";
            return status;
        }
        PrismFrame frame;
        SurfaceGeometry geometry;
        std::string validation_reason;
        if (validateSurfaceGeometry(volume, te, frame, mSelectionSurfaceCache, geometry,
                                    validation_reason) != ESurfaceValidation::VALID)
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_SELECTION;
            status.mReason = validation_reason.empty()
                ? "Selected face is not ready for Prism use." : validation_reason;
            return status;
        }
        status.mResult = LLPrismLens::ERegistryResult::OK;
        status.mReason.clear();
        return status;
    }

    LLPrismLens::ActionStatus displaySelectionStatus(
        const LLPrismLens::CaptureHandle& capture) const
    {
        LLPrismLens::ActionStatus status;
        const S32 capture_slot = findCapture(capture);
        if (capture_slot < 0)
        {
            status.mResult = LLPrismLens::ERegistryResult::STALE_HANDLE;
            status.mReason = "That capture no longer exists.";
            return status;
        }
        if (mLenses[capture_slot].mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
            status.mReason = "A surface lens owns exactly one display face.";
            return status;
        }
        if (displayCount() >= LLPrismLens::MAX_DISPLAY_BINDINGS)
        {
            status.mResult = LLPrismLens::ERegistryResult::AT_CAPACITY;
            status.mReason = "Maximum of 16 Prism display faces reached.";
            return status;
        }
        LLUUID object_id;
        S32 te = -1;
        LLVOVolume* volume = nullptr;
        if (!selectedFaceIdentity(object_id, te, &status.mReason, &volume))
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_SELECTION;
            return status;
        }
        if (findDisplayIdentity(object_id, te) >= 0)
        {
            status.mResult = LLPrismLens::ERegistryResult::DUPLICATE;
            status.mReason = "That face is already a Prism display.";
            return status;
        }
        if (object_id == mLenses[capture_slot].mCameraObjectId)
        {
            status.mResult = LLPrismLens::ERegistryResult::DUPLICATE;
            status.mReason = "A camera source cannot display its own feed.";
            return status;
        }
        PrismFrame frame;
        SurfaceGeometry geometry;
        std::string validation_reason;
        if (validateSurfaceGeometry(volume, te, frame, mSelectionSurfaceCache, geometry,
                                    validation_reason) != ESurfaceValidation::VALID)
        {
            status.mResult = LLPrismLens::ERegistryResult::INVALID_SELECTION;
            status.mReason = validation_reason.empty()
                ? "Selected face is not ready for Prism use." : validation_reason;
            return status;
        }
        status.mResult = LLPrismLens::ERegistryResult::OK;
        status.mReason.clear();
        return status;
    }

    LLPrismLens::ERegistryResult addCamera(LLPrismLens::CaptureHandle* handle,
                                           std::string* reason)
    {
        const LLPrismLens::ActionStatus status = cameraSelectionStatus();
        if (!status.allowed())
        {
            if (reason) *reason = status.mReason;
            return status.mResult;
        }
        LLUUID object_id;
        selectedObjectIdentity(object_id);
        const S32 slot = freeCaptureSlot();
        U64 generation = 0;
        if (slot < 0 || !allocateGeneration(generation))
        {
            if (reason) *reason = "Prism capture capacity or handle generation exhausted.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }
        PrismInstance capture;
        capture.mOccupied = true;
        capture.mHandle.mId.generate();
        capture.mHandle.mGeneration = generation;
        capture.mMode = LLPrismLens::ECaptureMode::CAMERA_FEED;
        capture.mCameraObjectId = object_id;
        capture.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
        capture.mNextDueTime = LLTimer::getTotalSeconds();
        mLenses[slot] = capture;
        ++mRevision;
        if (handle) *handle = capture.mHandle;
        if (reason) reason->clear();
        return LLPrismLens::ERegistryResult::OK;
    }

    // Prim-free camera. Mirrors addCamera's slot/handle/generation/revision
    // bookkeeping and runtime init, but skips the object identity and
    // eligibility checks: the source is a stored transform, not a selected
    // in-world object. mCameraObjectId is left null and mCamera.mVirtual is set.
    LLPrismLens::ERegistryResult addVirtualCamera(
        LLPrismLens::CaptureHandle* handle, const LLVector3& pos,
        const LLQuaternion& rot, std::string* reason)
    {
        if (count() >= LLPrismLens::MAX_CAPTURES)
        {
            if (reason) *reason = "Maximum of 3 Prism captures reached.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }
        LLPrismLens::CameraSettings camera; // defaults (FIXED fov, etc.)
        camera.mVirtual = true;
        camera.mVirtualPos = pos;
        camera.mVirtualRot = rot;
        std::string camera_reason;
        if (!validCameraSettings(camera, &camera_reason))
        {
            if (reason) *reason = camera_reason;
            return LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        }
        const S32 slot = freeCaptureSlot();
        U64 generation = 0;
        if (slot < 0 || !allocateGeneration(generation))
        {
            if (reason) *reason = "Prism capture capacity or handle generation exhausted.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }
        PrismInstance capture;
        capture.mOccupied = true;
        capture.mHandle.mId.generate();
        capture.mHandle.mGeneration = generation;
        capture.mMode = LLPrismLens::ECaptureMode::CAMERA_FEED;
        capture.mCameraObjectId.setNull(); // objectless: virtual transform only
        capture.mCamera = camera;
        capture.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
        capture.mNextDueTime = LLTimer::getTotalSeconds();
        mLenses[slot] = capture;
        ++mRevision;
        if (handle) *handle = capture.mHandle;
        if (reason) reason->clear();
        return LLPrismLens::ERegistryResult::OK;
    }

    LLPrismLens::ERegistryResult addLens(LLPrismLens::CaptureHandle* handle,
                                         std::string* reason)
    {
        const LLPrismLens::ActionStatus status = lensSelectionStatus();
        if (!status.allowed())
        {
            if (reason) *reason = status.mReason;
            return status.mResult;
        }
        LLUUID object_id;
        S32 te = -1;
        selectedFaceIdentity(object_id, te);
        const S32 capture_slot = freeCaptureSlot();
        const S32 display_slot = freeDisplaySlot();
        U64 capture_generation = 0;
        U64 display_generation = 0;
        if (capture_slot < 0 || display_slot < 0 ||
            !allocateGeneration(capture_generation) ||
            !allocateGeneration(display_generation))
        {
            if (reason) *reason = "Prism registry capacity or handle generation exhausted.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }

        PrismInstance capture;
        capture.mOccupied = true;
        capture.mHandle.mId.generate();
        capture.mHandle.mGeneration = capture_generation;
        capture.mMode = LLPrismLens::ECaptureMode::SURFACE_LENS;
        capture.mObjectId = object_id;
        capture.mTE = te;
        capture.mDisplayCount = 1;
        capture.mNextDueTime = LLTimer::getTotalSeconds();
        resetFrame(capture, gFrameCount);

        PrismDisplay display;
        display.mOccupied = true;
        display.mHandle.mId.generate();
        display.mHandle.mGeneration = display_generation;
        display.mCaptureSlot = static_cast<U32>(capture_slot);
        display.mCaptureGeneration = capture_generation;
        display.mObjectId = object_id;
        display.mTE = te;
        display.mSettings.mFitMode = LLPrismLens::EFitMode::STRETCH;

        mLenses[capture_slot] = capture;
        mDisplays[display_slot] = display;
        ++mRevision;
        if (handle) *handle = capture.mHandle;
        if (reason) reason->clear();
        return LLPrismLens::ERegistryResult::OK;
    }

    LLPrismLens::ERegistryResult addDisplay(
        const LLPrismLens::CaptureHandle& capture_handle,
        LLPrismLens::EFitMode fit, LLPrismLens::DisplayHandle* handle,
        std::string* reason)
    {
        if (fit != LLPrismLens::EFitMode::FIT &&
            fit != LLPrismLens::EFitMode::FILL &&
            fit != LLPrismLens::EFitMode::STRETCH)
        {
            if (reason) *reason = "Unknown display fit mode.";
            return LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        }
        const LLPrismLens::ActionStatus status = displaySelectionStatus(capture_handle);
        if (!status.allowed())
        {
            if (reason) *reason = status.mReason;
            return status.mResult;
        }
        LLUUID object_id;
        S32 te = -1;
        selectedFaceIdentity(object_id, te);
        const S32 capture_slot = findCapture(capture_handle);
        const S32 display_slot = freeDisplaySlot();
        U64 generation = 0;
        if (capture_slot < 0 || display_slot < 0 || !allocateGeneration(generation))
        {
            if (reason) *reason = "Prism display capacity or handle generation exhausted.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }
        PrismDisplay display;
        display.mOccupied = true;
        display.mHandle.mId.generate();
        display.mHandle.mGeneration = generation;
        display.mCaptureSlot = static_cast<U32>(capture_slot);
        display.mCaptureGeneration = mLenses[capture_slot].mHandle.mGeneration;
        display.mObjectId = object_id;
        display.mTE = te;
        display.mSettings.mFitMode = fit;
        mDisplays[display_slot] = display;
        ++mLenses[capture_slot].mDisplayCount;
        ++mRevision;
        if (handle) *handle = display.mHandle;
        if (reason) reason->clear();
        return LLPrismLens::ERegistryResult::OK;
    }

    // Prim-free screen. Mirrors addDisplay's capacity / slot / handle /
    // generation / revision bookkeeping, but takes a stored world transform +
    // size instead of a selected face: no selection status, no face identity, no
    // surface-geometry validation. mObjectId stays null, mTE stays -1, and
    // mSettings.mVirtual is set. A virtual display counts toward the capture's
    // mDisplayCount exactly like a real binding.
    LLPrismLens::ERegistryResult addVirtualDisplay(
        const LLPrismLens::CaptureHandle& capture_handle,
        const LLVector3& pos, const LLQuaternion& rot, F32 width, F32 height,
        LLPrismLens::DisplayHandle* handle, std::string* reason)
    {
        const S32 capture_slot = findCapture(capture_handle);
        if (capture_slot < 0)
        {
            if (reason) *reason = "That capture no longer exists.";
            return LLPrismLens::ERegistryResult::STALE_HANDLE;
        }
        if (mLenses[capture_slot].mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
        {
            if (reason) *reason = "A surface lens owns exactly one display face.";
            return LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        }
        if (displayCount() >= LLPrismLens::MAX_DISPLAY_BINDINGS)
        {
            if (reason) *reason = "Maximum of 16 Prism display faces reached.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }
        LLPrismLens::DisplaySettings settings; // defaults (FIT, etc.)
        settings.mVirtual = true;
        settings.mPos = pos;
        settings.mRot = rot;
        settings.mWidth = width;
        settings.mHeight = height;
        std::string settings_reason;
        if (!validDisplaySettings(settings, &settings_reason))
        {
            if (reason) *reason = settings_reason;
            return LLPrismLens::ERegistryResult::INVALID_CONFIGURATION;
        }
        const S32 display_slot = freeDisplaySlot();
        U64 generation = 0;
        if (display_slot < 0 || !allocateGeneration(generation))
        {
            if (reason) *reason = "Prism display capacity or handle generation exhausted.";
            return LLPrismLens::ERegistryResult::AT_CAPACITY;
        }
        PrismDisplay display;
        display.mOccupied = true;
        display.mHandle.mId.generate();
        display.mHandle.mGeneration = generation;
        display.mCaptureSlot = static_cast<U32>(capture_slot);
        display.mCaptureGeneration = mLenses[capture_slot].mHandle.mGeneration;
        display.mObjectId.setNull(); // faceless: virtual transform only
        display.mTE = -1;
        display.mSettings = settings;
        mDisplays[display_slot] = display;
        ++mLenses[capture_slot].mDisplayCount;
        ++mRevision;
        if (handle) *handle = display.mHandle;
        if (reason) reason->clear();
        return LLPrismLens::ERegistryResult::OK;
    }

    bool canDesignate() const
    {
        return lensSelectionStatus().allowed();
    }

    LLPrismLens::EDesignationResult selectedStatus(std::string* reason = nullptr) const
    {
        const LLPrismLens::ActionStatus status = lensSelectionStatus();
        if (reason) *reason = status.mReason;
        switch (status.mResult)
        {
            case LLPrismLens::ERegistryResult::OK:
                return LLPrismLens::EDesignationResult::ELIGIBLE;
            case LLPrismLens::ERegistryResult::DUPLICATE:
                return LLPrismLens::EDesignationResult::ALREADY_EXISTS;
            case LLPrismLens::ERegistryResult::AT_CAPACITY:
                return LLPrismLens::EDesignationResult::AT_CAPACITY;
            default:
                return LLPrismLens::EDesignationResult::INVALID_SELECTION;
        }
    }

    LLPrismLens::EDesignationResult designate(std::string* reason)
    {
        LLPrismLens::CaptureHandle handle;
        const LLPrismLens::ERegistryResult result = addLens(&handle, reason);
        switch (result)
        {
            case LLPrismLens::ERegistryResult::OK:
                return LLPrismLens::EDesignationResult::ADDED;
            case LLPrismLens::ERegistryResult::DUPLICATE:
                return LLPrismLens::EDesignationResult::ALREADY_EXISTS;
            case LLPrismLens::ERegistryResult::AT_CAPACITY:
                return LLPrismLens::EDesignationResult::AT_CAPACITY;
            default:
                return LLPrismLens::EDesignationResult::INVALID_SELECTION;
        }
    }

    bool remove(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return false;
        }
        clearSlot(slot);
        return true;
    }

    void clearAll()
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            if (mLenses[slot].mOccupied)
            {
                clearSlot(slot);
            }
        }
        for (PrismDisplay& display : mDisplays)
        {
            display = PrismDisplay();
        }
        gPipeline.releasePrismLensBuffers();
        resetRuntimeHistory();
    }

    void releaseRenderResources()
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            gPipeline.releasePrismLensOutput(slot);
            PrismInstance& lens = mLenses[slot];
            lens.mHasOutput = false;
            lens.mOutputWidth = 0;
            lens.mOutputHeight = 0;
            lens.mLastRenderedFrame = 0;
            lens.mLastProducedTime = 0.0;
            lens.mRetryAfterTime = 0.0;
            lens.mPublicationSamples = 0;
            lens.mPublicationWindowStart = 0.0;
            lens.mFrame.mProduced = false;
            lens.mRuntime.mOutput = LLPrismLens::EOutputState::EMPTY;
            lens.mRuntime.mActivity = LLPrismLens::EActivityState::PAUSED;
            lens.mRuntime.mObservedPublicationHz = 0.f;
            lens.mRuntime.mOutputAgeSeconds = 0.f;
        }
        gPipeline.releasePrismLensBuffers();
        mLastRenderedSlot = -1;
        resetRuntimeHistory();
        ++mRuntimeRevision;
    }

    void onRenderTargetsReleased()
    {
        bool changed = false;
        for (PrismInstance& capture : mLenses)
        {
            if (!capture.mOccupied)
            {
                continue;
            }
            const bool owned_publication = capture.mHasOutput ||
                capture.mOutputWidth != 0 || capture.mOutputHeight != 0 ||
                capture.mRuntime.mOutput == LLPrismLens::EOutputState::CURRENT ||
                capture.mRuntime.mOutput == LLPrismLens::EOutputState::HELD;
            if (!owned_publication)
            {
                continue;
            }
            capture.mHasOutput = false;
            capture.mOutputWidth = 0;
            capture.mOutputHeight = 0;
            capture.mLastRenderedFrame = 0;
            capture.mLastProducedTime = 0.0;
            capture.mRetryAfterTime = 0.0;
            capture.mPublicationSamples = 0;
            capture.mPublicationWindowStart = 0.0;
            capture.mFrame.mProduced = false;
            capture.mRuntime.mOutput = LLPrismLens::EOutputState::EMPTY;
            capture.mRuntime.mActivity = !prismEnabled()
                ? LLPrismLens::EActivityState::PAUSED
                : (capture.mAnyDisplayVisible
                    ? LLPrismLens::EActivityState::WAITING
                    : LLPrismLens::EActivityState::IDLE);
            capture.mRuntime.mObservedPublicationHz = 0.f;
            capture.mRuntime.mOutputAgeSeconds = 0.f;
            capture.mNextDueTime = LLTimer::getTotalSeconds();
            changed = true;
        }
        mLastRenderedSlot = -1;
        if (changed)
        {
            ++mRuntimeRevision;
        }
    }

    bool hasDesignation() const
    {
        return count() != 0;
    }

    U32 count() const
    {
        U32 result = 0;
        for (const PrismInstance& lens : mLenses)
        {
            result += lens.mOccupied ? 1u : 0u;
        }
        return result;
    }

    U32 revision() const { return static_cast<U32>(mRevision); }

    bool getDesignation(U32 slot, LLPrismLens::Designation& designation) const
    {
        designation = LLPrismLens::Designation();
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied ||
            mLenses[slot].mMode != LLPrismLens::ECaptureMode::SURFACE_LENS)
        {
            return false;
        }
        designation.mSlot = slot;
        designation.mObjectId = mLenses[slot].mObjectId;
        designation.mTextureEntry = mLenses[slot].mTE;
        return true;
    }

    void releaseResolvedFace(U32 slot)
    {
        if (slot < LLPrismLens::MAX_LENSES)
        {
            mLenses[slot].mFrame.mResolvedFace = nullptr;
        }
    }

    void setLensDisplayRuntime(U32 slot,
                               LLPrismLens::EDisplayHealth health,
                               LLPrismLens::EDisplayVisibility visibility,
                               const std::string& health_reason,
                               const std::string& visibility_reason)
    {
        if (slot >= LLPrismLens::MAX_CAPTURES || !mLenses[slot].mOccupied)
        {
            return;
        }
        const U64 capture_generation = mLenses[slot].mHandle.mGeneration;
        for (PrismDisplay& display : mDisplays)
        {
            if (!display.mOccupied || display.mCaptureSlot != slot ||
                display.mCaptureGeneration != capture_generation)
            {
                continue;
            }
            display.mRuntime.mHealth = health;
            display.mRuntime.mVisibility = visibility;
            display.mRuntime.mHealthReason = health_reason;
            display.mRuntime.mVisibilityReason = visibility_reason;
        }
    }

    bool prepare(U32 slot, const S32 viewport[4], const glm::mat4& main_projection,
                  const glm::mat4& main_modelview, const LLViewerCamera& main_camera)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return false;
        }

        if (mLenses[slot].mMode == LLPrismLens::ECaptureMode::CAMERA_FEED)
        {
            return prepareCameraCapture(slot, viewport, main_projection,
                                        main_modelview, main_camera);
        }

        PrismInstance& lens = mLenses[slot];
        lens.mAnyDisplayVisible = false;
        lens.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
        for (PrismDisplay& display : mDisplays)
        {
            if (!display.mOccupied || display.mCaptureSlot != slot) continue;
            display.mFrame = PrismFrame();
            display.mFrame.mFrame = gFrameCount;
        }
        resetFrame(lens, gFrameCount);
        PrismFrame& mFrame = lens.mFrame;
        const S32 mTE = lens.mTE;
        const auto reject = [this, slot](const std::string& reject_reason)
        {
            return rejectSlot(slot, reject_reason);
        };
        const auto resolveObject = [this, slot](bool reject_invalid)
        {
            return resolveObjectForSlot(slot, reject_invalid);
        };

        mFrame.mZoom = prismZoom();
        mFrame.mResolutionScale = prismAppliedResolutionScale();
        mFrame.mEdgeFeather = prismEdgeFeather();
        std::memcpy(mFrame.mMainViewport, viewport, sizeof(mFrame.mMainViewport));
        makeDebugRect(viewport, mFrame.mDebugRect);

        LLVOVolume* object = resolveObject(true);
        if (!object)
        {
            return false;
        }

        SurfaceGeometry geometry;
        std::string surface_reject_reason;
        const ESurfaceValidation validation = validateSurfaceGeometry(
            object, mTE, mFrame, lens.mSurfaceCache,
            geometry, surface_reject_reason);
        if (validation == ESurfaceValidation::TRANSIENT)
        {
            // Drawable, transform, and volume rebuilds are temporary. Retain the
            // local slot and its most recent output until the face resolves again.
            lens.mRuntime.mHealth = LLPrismLens::ECaptureHealth::LENS_SURFACE_OFFLINE;
            lens.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
            lens.mRuntime.mReason = surface_reject_reason;
            setLensDisplayRuntime(
                slot, LLPrismLens::EDisplayHealth::OFFLINE,
                LLPrismLens::EDisplayVisibility::UNKNOWN,
                surface_reject_reason,
                "Visibility is unknown while the lens geometry is unavailable.");
            return false;
        }
        if (validation == ESurfaceValidation::INVALID)
        {
            return reject(surface_reject_reason);
        }

        LLFace* face = geometry.mFace;
        const LLVector3 center = geometry.mCenter;
        const LLVector3 plane_normal = geometry.mPlaneNormal;
        lens.mRuntime.mHealth = LLPrismLens::ECaptureHealth::READY;
        lens.mRuntime.mReason.clear();
        lens.mRuntime.mEffectiveResolutionScale = mFrame.mResolutionScale;
        setLensDisplayRuntime(
            slot, LLPrismLens::EDisplayHealth::READY,
            LLPrismLens::EDisplayVisibility::OFFSCREEN, std::string(),
            "The lens face is not visible in the main view.");

        const LLVector3 eye = main_camera.getOrigin();
        const LLVector3 surface_normal =
            mFrame.mWorldSurfaceUEdge % mFrame.mWorldSurfaceVEdge;
        if ((eye - center) * surface_normal < 0.f)
        {
            // Keep the generalized view basis right-handed when the back of the
            // geometric UV surface is viewed, then undo that U reversal at sample time.
            mFrame.mCompositeUvScale[0] = -1.f;
            mFrame.mCompositeUvOffset[0] = 1.f;
        }

        LLVector3 keep_normal = plane_normal;
        if ((eye - center) * keep_normal > 0.f)
        {
            keep_normal = -keep_normal;
        }

        const glm::mat4 main_view_projection = main_projection * main_modelview;

        F32 projected_min_x = std::numeric_limits<F32>::max();
        F32 projected_min_y = std::numeric_limits<F32>::max();
        F32 projected_max_x = -std::numeric_limits<F32>::max();
        F32 projected_max_y = -std::numeric_limits<F32>::max();
        bool projected = false;
        F32 min_x = std::numeric_limits<F32>::max();
        F32 min_y = std::numeric_limits<F32>::max();
        F32 max_x = -std::numeric_limits<F32>::max();
        F32 max_y = -std::numeric_limits<F32>::max();
        bool visible = false;
        const auto accumulate_footprint =
            [viewport](const ClipPolygon& polygon, F32& bounds_min_x,
                       F32& bounds_min_y, F32& bounds_max_x,
                       F32& bounds_max_y, bool& has_footprint)
        {
            for (const glm::vec4& clip : polygon)
            {
                if (clip.w <= CLIP_EPSILON)
                {
                    continue;
                }
                const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
                const F32 x = static_cast<F32>(viewport[0]) +
                    (ndc.x * 0.5f + 0.5f) * static_cast<F32>(viewport[2]);
                const F32 y = static_cast<F32>(viewport[1]) +
                    (ndc.y * 0.5f + 0.5f) * static_cast<F32>(viewport[3]);
                if (!std::isfinite(x) || !std::isfinite(y))
                {
                    continue;
                }
                bounds_min_x = llmin(bounds_min_x, x);
                bounds_min_y = llmin(bounds_min_y, y);
                bounds_max_x = llmax(bounds_max_x, x);
                bounds_max_y = llmax(bounds_max_y, y);
                has_footprint = true;
            }
        };

        // Surface-fit guarantees a rectangular affine aperture. Clip that
        // four-corner hull once instead of allocating and clipping every source
        // triangle; holes/extra tessellation can only make this conservative.
        const LLVector3 world_corners[4] =
        {
            mFrame.mWorldSurfaceOrigin,
            mFrame.mWorldSurfaceOrigin + mFrame.mWorldSurfaceUEdge,
            mFrame.mWorldSurfaceOrigin + mFrame.mWorldSurfaceUEdge +
                mFrame.mWorldSurfaceVEdge,
            mFrame.mWorldSurfaceOrigin + mFrame.mWorldSurfaceVEdge
        };
        ClipPolygon polygon;
        ClipPolygon clip_scratch;
        polygon.reserve(8);
        clip_scratch.reserve(8);
        for (const LLVector3& corner : world_corners)
        {
            polygon.push_back(main_view_projection * glm::vec4(
                corner.mV[VX], corner.mV[VY], corner.mV[VZ], 1.f));
        }
        // Retain a surface that crosses the near plane, and measure its full
        // projected size before clipping the footprint to the viewport.
        for (S32 plane = 4; plane < 6 && !polygon.empty(); ++plane)
        {
            clipPolygonAgainstPlane(polygon, clip_scratch, plane);
        }
        accumulate_footprint(polygon, projected_min_x, projected_min_y,
                             projected_max_x, projected_max_y, projected);
        for (S32 plane = 0; plane < 4 && !polygon.empty(); ++plane)
        {
            clipPolygonAgainstPlane(polygon, clip_scratch, plane);
        }
        accumulate_footprint(polygon, min_x, min_y, max_x, max_y, visible);
        if (!projected)
        {
            // Entirely behind the near plane (or beyond the far plane).
            return false;
        }

        const F32 projected_width = projected_max_x - projected_min_x;
        const F32 projected_height = projected_max_y - projected_min_y;
        if (projected_width < static_cast<F32>(MIN_PROJECTED_EXTENT) ||
            projected_height < static_cast<F32>(MIN_PROJECTED_EXTENT) ||
            projected_width * projected_height < static_cast<F32>(MIN_PROJECTED_AREA))
        {
            // Camera distance and clipping can make an otherwise valid lens tiny.
            // Keep the registry entry and retained output; simply skip this frame.
            return false;
        }
        if (!visible)
        {
            return false;
        }

        const S32 viewport_right = viewport[0] + viewport[2];
        const S32 viewport_top = viewport[1] + viewport[3];
        const S32 left = llclamp(static_cast<S32>(floorf(min_x)), viewport[0], viewport_right);
        const S32 bottom = llclamp(static_cast<S32>(floorf(min_y)), viewport[1], viewport_top);
        const S32 right = llclamp(static_cast<S32>(ceilf(max_x)), viewport[0], viewport_right);
        const S32 top = llclamp(static_cast<S32>(ceilf(max_y)), viewport[1], viewport_top);
        const S32 width = right - left;
        const S32 height = top - bottom;
        if (width < MIN_VISIBLE_EXTENT || height < MIN_VISIBLE_EXTENT ||
            width * height < MIN_VISIBLE_AREA)
        {
            // This is a transient visibility condition, not an invalid designation.
            return false;
        }

        mFrame.mLensRect.mX = left;
        mFrame.mLensRect.mY = bottom;
        mFrame.mLensRect.mWidth = static_cast<U32>(width);
        mFrame.mLensRect.mHeight = static_cast<U32>(height);
        // A nearly clipped footprint still needs a finite crop of at least one pixel.
        mFrame.mZoom = llmin(mFrame.mZoom, static_cast<F32>(llmin(width, height)));
        mFrame.mTargetWidth = bucketedTargetExtent(static_cast<F32>(width), mFrame.mResolutionScale);
        mFrame.mTargetHeight = bucketedTargetExtent(static_cast<F32>(height), mFrame.mResolutionScale);
        mFrame.mFragmentClipPlane = LLPlane(center + keep_normal * LENS_PLANE_EPSILON,
                                            keep_normal);
        mFrame.mResolvedFace = face;
        mFrame.mPrepared = true;
        lens.mAnyDisplayVisible = true;
        lens.mRuntime.mActivity = LLPrismLens::EActivityState::WAITING;
        setLensDisplayRuntime(
            slot, LLPrismLens::EDisplayHealth::READY,
            LLPrismLens::EDisplayVisibility::VISIBLE,
            std::string(), std::string());
        for (PrismDisplay& display : mDisplays)
        {
            if (!display.mOccupied || display.mCaptureSlot != slot ||
                display.mCaptureGeneration != lens.mHandle.mGeneration)
            {
                continue;
            }
            display.mFrame = mFrame;
            display.mFrame.mResolvedFace = nullptr;
            break;
        }
        return true;
    }

    // Synthesize a virtual screen's surface basis + world rectangle into `frame`
    // from a stored transform + size. Returns false for a non-finite or
    // degenerate record. Everything is world/agent space -- the same space the
    // quad positions and the composite modelview use. The dual basis uses the
    // SAME general formula as solveSurfaceBasis (623-625), which for an
    // orthogonal rectangle reduces to UDual = u_edge/|u_edge|^2 and
    // VDual = v_edge/|v_edge|^2, so a virtual screen maps its feed identically to
    // a real rectangular face.
    static bool synthesizeVirtualDisplayFrame(
        const LLPrismLens::DisplaySettings& settings, PrismFrame& frame)
    {
        if (!settings.mPos.isFinite() || !settings.mRot.isFinite() ||
            !std::isfinite(settings.mWidth) || !std::isfinite(settings.mHeight) ||
            settings.mWidth <= F_ALMOST_ZERO || settings.mHeight <= F_ALMOST_ZERO)
        {
            return false;
        }
        // right = local +X, up = local +Y in agent space (the camera-guide
        // convention). The rectangle lies in the right/up plane, centred on mPos.
        const LLVector3 right = LLVector3::x_axis * settings.mRot;
        const LLVector3 up = LLVector3::y_axis * settings.mRot;
        const F32 half_w = settings.mWidth * 0.5f;
        const F32 half_h = settings.mHeight * 0.5f;
        const LLVector3 tl = settings.mPos - right * half_w + up * half_h;
        const LLVector3 tr = settings.mPos + right * half_w + up * half_h;
        const LLVector3 bl = settings.mPos - right * half_w - up * half_h;
        const LLVector3 u_edge = tr - tl; // rightward, |u_edge| == width
        const LLVector3 v_edge = bl - tl; // downward,  |v_edge| == height
        const F32 uu = u_edge * u_edge;
        const F32 vv = v_edge * v_edge;
        const F32 uv = u_edge * v_edge;
        const F32 dual_determinant = uu * vv - uv * uv;
        if (!std::isfinite(uu) || !std::isfinite(vv) || !std::isfinite(uv) ||
            uu <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON ||
            vv <= SURFACE_UV_EPSILON * SURFACE_UV_EPSILON ||
            dual_determinant <= uu * vv * SURFACE_UV_EPSILON)
        {
            return false;
        }
        // A virtual screen's surface space IS its world/agent space, so the
        // surface and world packs share the same origin and edges.
        frame.mSurfaceOrigin = tl;
        frame.mSurfaceUDual = (u_edge * vv - v_edge * uv) / dual_determinant;
        frame.mSurfaceVDual = (v_edge * uu - u_edge * uv) / dual_determinant;
        frame.mWorldSurfaceOrigin = tl;
        frame.mWorldSurfaceUEdge = u_edge;
        frame.mWorldSurfaceVEdge = v_edge;
        return true;
    }

    bool prepareDisplayFrame(PrismDisplay& display, const S32 viewport[4],
                             const glm::mat4& main_projection,
                             const glm::mat4& main_modelview)
    {
        display.mFrame = PrismFrame();
        PrismFrame& frame = display.mFrame;
        frame.mFrame = gFrameCount;
        frame.mResolutionScale = prismAppliedResolutionScale();
        frame.mEdgeFeather = prismEdgeFeather();
        std::memcpy(frame.mMainViewport, viewport, sizeof(frame.mMainViewport));
        makeDebugRect(viewport, frame.mDebugRect);

        // Resolved at composite time. For a real display it is the prim face; a
        // virtual (prim-free) display has none and leaves this null, which is
        // exactly what a virtual CompositeState expects.
        SurfaceGeometry geometry;
        if (display.mSettings.mVirtual)
        {
            // Prim-free screen: synthesize the surface basis + world rectangle
            // from the stored transform/size instead of reading a face. All of
            // the following is in world/agent space -- the SAME space the quad
            // positions and the composite modelview use -- so the shader's
            // prism_uv = (position - surfaceOrigin) . UDual is correct. There is
            // no object lookup and no surface-geometry validation.
            if (!synthesizeVirtualDisplayFrame(display.mSettings, frame))
            {
                display.mRuntime.mHealth = LLPrismLens::EDisplayHealth::INVALID;
                display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::UNKNOWN;
                display.mRuntime.mHealthReason = "Virtual screen transform or size is not usable.";
                display.mRuntime.mVisibilityReason = "Invalid virtual screen is not composited.";
                return false;
            }
        }
        else
        {
            LLViewerObject* object = gObjectList.findObject(display.mObjectId);
            LLVOVolume* volume = object ? dynamic_cast<LLVOVolume*>(object) : nullptr;
            if (!object)
            {
                display.mRuntime.mHealth = LLPrismLens::EDisplayHealth::OFFLINE;
                display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::UNKNOWN;
                display.mRuntime.mHealthReason = "Display object is outside the local object list.";
                display.mRuntime.mVisibilityReason = "Visibility is unknown while the object is offline.";
                return false;
            }
            if (object->isDead() || !volume || object->isHUDAttachment() ||
                volume->isRiggedMesh() || volume->isAnimatedObject())
            {
                display.mRuntime.mHealth = LLPrismLens::EDisplayHealth::INVALID;
                display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::UNKNOWN;
                display.mRuntime.mHealthReason = "Display must remain a live, static, non-HUD volume face.";
                display.mRuntime.mVisibilityReason = "Invalid display geometry is not composited.";
                return false;
            }

            std::string validation_reason;
            const ESurfaceValidation validation = validateSurfaceGeometry(
                volume, display.mTE, frame, display.mSurfaceCache,
                geometry, validation_reason);
            if (validation != ESurfaceValidation::VALID)
            {
                display.mRuntime.mHealth = validation == ESurfaceValidation::TRANSIENT
                    ? LLPrismLens::EDisplayHealth::OFFLINE
                    : LLPrismLens::EDisplayHealth::INVALID;
                display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::UNKNOWN;
                display.mRuntime.mHealthReason = validation_reason;
                display.mRuntime.mVisibilityReason = "Display geometry is not currently compositable.";
                return false;
            }
        }

        // Shared main-view projection for BOTH real and virtual displays: the
        // synthesized world rectangle projects through the identical corner /
        // frustum-clip / lens-rect / target-size machinery, so a virtual screen
        // gets real frustum culling and target sizing for free. (Its composite
        // scissor is still the full target -- see getCompositeStates -- but the
        // lens rect it computes here is only used for output sizing.)
        const glm::mat4 view_projection = main_projection * main_modelview;
        const LLVector3 corners[4] =
        {
            frame.mWorldSurfaceOrigin,
            frame.mWorldSurfaceOrigin + frame.mWorldSurfaceUEdge,
            frame.mWorldSurfaceOrigin + frame.mWorldSurfaceUEdge + frame.mWorldSurfaceVEdge,
            frame.mWorldSurfaceOrigin + frame.mWorldSurfaceVEdge
        };
        ClipPolygon polygon;
        ClipPolygon scratch;
        polygon.reserve(8);
        scratch.reserve(8);
        for (const LLVector3& corner : corners)
        {
            polygon.push_back(view_projection * glm::vec4(
                corner.mV[VX], corner.mV[VY], corner.mV[VZ], 1.f));
        }
        for (S32 plane = 0; plane < 6 && !polygon.empty(); ++plane)
        {
            clipPolygonAgainstPlane(polygon, scratch, plane);
        }
        if (polygon.empty())
        {
            display.mRuntime.mHealth = LLPrismLens::EDisplayHealth::READY;
            display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::OFFSCREEN;
            display.mRuntime.mHealthReason.clear();
            display.mRuntime.mVisibilityReason = "Display face is outside the main-view frustum.";
            return false;
        }

        F32 min_x = std::numeric_limits<F32>::max();
        F32 min_y = std::numeric_limits<F32>::max();
        F32 max_x = -std::numeric_limits<F32>::max();
        F32 max_y = -std::numeric_limits<F32>::max();
        for (const glm::vec4& clip : polygon)
        {
            if (clip.w <= CLIP_EPSILON) continue;
            const F32 x = static_cast<F32>(viewport[0]) +
                (clip.x / clip.w * 0.5f + 0.5f) * static_cast<F32>(viewport[2]);
            const F32 y = static_cast<F32>(viewport[1]) +
                (clip.y / clip.w * 0.5f + 0.5f) * static_cast<F32>(viewport[3]);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            min_x = llmin(min_x, x);
            min_y = llmin(min_y, y);
            max_x = llmax(max_x, x);
            max_y = llmax(max_y, y);
        }
        const S32 viewport_right = viewport[0] + viewport[2];
        const S32 viewport_top = viewport[1] + viewport[3];
        const S32 left = llclamp(static_cast<S32>(floorf(min_x)), viewport[0], viewport_right);
        const S32 bottom = llclamp(static_cast<S32>(floorf(min_y)), viewport[1], viewport_top);
        const S32 right = llclamp(static_cast<S32>(ceilf(max_x)), viewport[0], viewport_right);
        const S32 top = llclamp(static_cast<S32>(ceilf(max_y)), viewport[1], viewport_top);
        const S32 width = right - left;
        const S32 height = top - bottom;
        if (width < MIN_VISIBLE_EXTENT || height < MIN_VISIBLE_EXTENT ||
            width * height < MIN_VISIBLE_AREA)
        {
            display.mRuntime.mHealth = LLPrismLens::EDisplayHealth::READY;
            display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::OFFSCREEN;
            display.mRuntime.mHealthReason.clear();
            display.mRuntime.mVisibilityReason = "Display face is below the visible-pixel threshold.";
            return false;
        }

        frame.mLensRect.mX = left;
        frame.mLensRect.mY = bottom;
        frame.mLensRect.mWidth = static_cast<U32>(width);
        frame.mLensRect.mHeight = static_cast<U32>(height);
        frame.mTargetWidth = bucketedTargetExtent(static_cast<F32>(width), frame.mResolutionScale);
        frame.mTargetHeight = bucketedTargetExtent(static_cast<F32>(height), frame.mResolutionScale);
        frame.mResolvedFace = geometry.mFace;
        frame.mPrepared = true;
        display.mRuntime.mHealth = LLPrismLens::EDisplayHealth::READY;
        display.mRuntime.mVisibility = LLPrismLens::EDisplayVisibility::VISIBLE;
        display.mRuntime.mHealthReason.clear();
        display.mRuntime.mVisibilityReason.clear();
        return true;
    }

    bool prepareCameraCapture(U32 slot, const S32 viewport[4],
                              const glm::mat4& main_projection,
                              const glm::mat4& main_modelview,
                              const LLViewerCamera& main_camera)
    {
        PrismInstance& capture = mLenses[slot];
        resetFrame(capture, gFrameCount);
        capture.mAnyDisplayVisible = false;
        capture.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
        capture.mRuntime.mEffectiveResolutionScale = prismAppliedResolutionScale();

        // Destination geometry is independent of source health. Prepare it
        // first so an offline/invalid camera can continue showing a valid held
        // publication on every currently visible binding.
        U32 required_width = MIN_TARGET_EXTENT;
        U32 required_height = MIN_TARGET_EXTENT;
        for (PrismDisplay& display : mDisplays)
        {
            if (!display.mOccupied || display.mCaptureSlot != slot ||
                display.mCaptureGeneration != capture.mHandle.mGeneration)
            {
                continue;
            }
            if (prepareDisplayFrame(display, viewport, main_projection, main_modelview))
            {
                capture.mAnyDisplayVisible = true;
                required_width = llmax(required_width, display.mFrame.mTargetWidth);
                required_height = llmax(required_height, display.mFrame.mTargetHeight);
            }
            display.mFrame.mResolvedFace = nullptr;
        }
        if (capture.mAnyDisplayVisible)
        {
            capture.mRuntime.mActivity = LLPrismLens::EActivityState::WAITING;
        }

        LLViewerObject* source_object = gObjectList.findObject(capture.mCameraObjectId);
        LLVOVolume* source = source_object
            ? dynamic_cast<LLVOVolume*>(source_object) : nullptr;
        // A virtual (prim-free) camera derives everything from its stored
        // transform, so none of the object-required health returns apply and the
        // effective FOV is always the fixed vertical FOV (FOLLOW_PROJECTOR is
        // forbidden -- there is no spotlight object to read). The non-virtual
        // path below is byte-identical to before.
        F32 vertical_fov = capture.mCamera.mFixedVerticalFovRad;
        if (capture.mCamera.mVirtual)
        {
            if (!capture.mCamera.mVirtualPos.isFinite() ||
                !capture.mCamera.mVirtualRot.isFinite())
            {
                capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::INVALID_SOURCE;
                capture.mRuntime.mReason = "Virtual camera transform is not finite.";
                return false;
            }
        }
        else
        {
            if (capture.mCameraObjectId.isNull())
            {
                capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::UNBOUND_SOURCE;
                capture.mRuntime.mReason = "No camera source is bound.";
                return false;
            }
            if (!source_object)
            {
                capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::SOURCE_OFFLINE;
                capture.mRuntime.mReason = "Camera source is outside the local object list.";
                return false;
            }
            if (source_object->isDead() || !source || source_object->isHUDAttachment() ||
                source->isRiggedMesh() || source->isAnimatedObject() ||
                !source_object->getRenderPosition().isFinite() ||
                !source_object->getRenderRotation().isFinite())
            {
                capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::INVALID_SOURCE;
                capture.mRuntime.mReason = "Camera source is no longer a finite, static, non-HUD volume.";
                return false;
            }
            if (capture.mCamera.mFovMode == LLPrismLens::EFovMode::FOLLOW_PROJECTOR)
            {
                if (!source->isLightSpotlight())
                {
                    capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::INVALID_SOURCE;
                    capture.mRuntime.mReason = "Follow Projector source is not a spotlight projector.";
                    return false;
                }
                vertical_fov = source->getSpotLightParams().mV[VX];
                if (!std::isfinite(vertical_fov) || vertical_fov < 5.f * DEG_TO_RAD ||
                    vertical_fov > 175.f * DEG_TO_RAD)
                {
                    capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::INVALID_SOURCE;
                    capture.mRuntime.mReason = "Projector FOV is outside the supported 5-175 degree range.";
                    return false;
                }
            }
        }
        capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::READY;
        capture.mRuntime.mReason.clear();
        capture.mRuntime.mEffectiveVerticalFovRad = vertical_fov;
        capture.mRuntime.mEffectiveFarClip = llmin(capture.mCamera.mFarClip,
                                                   main_camera.getFar());

        if (!capture.mAnyDisplayVisible)
        {
            return false;
        }

        // One canonical aspect belongs to the producer. Sibling faces never
        // allocate additional targets; they apply Fit/Fill/Stretch at composite.
        const F32 aspect = capture.mCamera.mOutputAspect;
        F32 ideal_width = static_cast<F32>(required_width);
        F32 ideal_height = static_cast<F32>(required_height);
        if (ideal_width / ideal_height < aspect)
        {
            ideal_width = ideal_height * aspect;
        }
        else
        {
            ideal_height = ideal_width / aspect;
        }
        const F32 downscale = llmin(1.f, llmin(
            static_cast<F32>(MAX_TARGET_EXTENT) / ideal_width,
            static_cast<F32>(MAX_TARGET_EXTENT) / ideal_height));
        ideal_width *= downscale;
        ideal_height *= downscale;
        const F32 upscale = llmax(1.f, llmax(
            static_cast<F32>(MIN_TARGET_EXTENT) / ideal_width,
            static_cast<F32>(MIN_TARGET_EXTENT) / ideal_height));
        ideal_width *= upscale;
        ideal_height *= upscale;
        PrismFrame& frame = capture.mFrame;
        frame.mFrame = gFrameCount;
        std::memcpy(frame.mMainViewport, viewport, sizeof(frame.mMainViewport));
        makeDebugRect(viewport, frame.mDebugRect);
        frame.mResolutionScale = prismAppliedResolutionScale();
        frame.mTargetWidth = llclamp(static_cast<U32>(ll_round(ideal_width)),
                                     MIN_TARGET_EXTENT, MAX_TARGET_EXTENT);
        frame.mTargetHeight = llclamp(static_cast<U32>(ll_round(ideal_height)),
                                      MIN_TARGET_EXTENT, MAX_TARGET_EXTENT);
        frame.mPrepared = true;
        return true;
    }

    LLFace* resolveFaceForComposite(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_LENSES)
        {
            return nullptr;
        }
        PrismInstance& lens = mLenses[slot];
        PrismFrame& frame = lens.mFrame;
        if (!lens.mOccupied || !lens.mHasOutput || !frame.mPrepared ||
            frame.mFrame != gFrameCount)
        {
            return nullptr;
        }
        LLVOVolume* object = resolveObjectForSlot(slot, true);
        if (!object || !object->mDrawable || lens.mTE < 0 ||
            lens.mTE >= object->mDrawable->getNumFaces())
        {
            return nullptr;
        }
        LLFace* face = object->mDrawable->getFace(lens.mTE);
        if (!face || face->getTEOffset() != lens.mTE || !face->hasGeometry() ||
            !face->getVertexBuffer() || face->getIndicesCount() < 3)
        {
            return nullptr;
        }
        if (object->isRiggedMesh() || face->isState(LLFace::RIGGED))
        {
            rejectSlot(slot, "requires a static, flat face (prim or mesh)");
            return nullptr;
        }
        frame.mResolvedFace = face;
        return face;
    }

    const PrismDisplay* display(U32 slot) const
    {
        return slot < LLPrismLens::MAX_DISPLAY_BINDINGS && mDisplays[slot].mOccupied
            ? &mDisplays[slot] : nullptr;
    }

    LLFace* resolveDisplayFaceForComposite(U32 display_slot)
    {
        if (display_slot >= LLPrismLens::MAX_DISPLAY_BINDINGS) return nullptr;
        PrismDisplay& display = mDisplays[display_slot];
        if (!display.mOccupied || !display.mFrame.mPrepared ||
            display.mFrame.mFrame != gFrameCount ||
            display.mCaptureSlot >= LLPrismLens::MAX_CAPTURES)
        {
            return nullptr;
        }
        const U32 capture_slot = display.mCaptureSlot;
        PrismInstance& capture = mLenses[capture_slot];
        if (!capture.mOccupied || !capture.mHasOutput ||
            capture.mHandle.mGeneration != display.mCaptureGeneration)
        {
            return nullptr;
        }
        LLViewerObject* object = gObjectList.findObject(display.mObjectId);
        LLVOVolume* volume = object ? dynamic_cast<LLVOVolume*>(object) : nullptr;
        if (!volume || object->isDead() || !object->mDrawable || display.mTE < 0 ||
            display.mTE >= object->mDrawable->getNumFaces())
        {
            return nullptr;
        }
        LLFace* face = object->mDrawable->getFace(display.mTE);
        if (!face || face->getTEOffset() != display.mTE || !face->hasGeometry() ||
            !face->getVertexBuffer() || face->getIndicesCount() < 3 ||
            volume->isRiggedMesh() || volume->isAnimatedObject() ||
            face->isState(LLFace::RIGGED))
        {
            return nullptr;
        }
        display.mFrame.mResolvedFace = face;
        return face;
    }

    void releaseResolvedDisplayFace(U32 display_slot)
    {
        if (display_slot < LLPrismLens::MAX_DISPLAY_BINDINGS)
        {
            mDisplays[display_slot].mFrame.mResolvedFace = nullptr;
        }
    }

    static void closePublicationWindow(PrismInstance& capture, F64 now)
    {
        if (capture.mPublicationWindowStart <= 0.0)
        {
            return;
        }
        const F64 sample_window = now - capture.mPublicationWindowStart;
        if (sample_window < 1.0)
        {
            return;
        }
        capture.mRuntime.mObservedPublicationHz =
            static_cast<F32>(capture.mPublicationSamples / sample_window);
        capture.mPublicationSamples = 0;
        capture.mPublicationWindowStart = now;
    }

    void closeAttemptWindow(F64 now)
    {
        if (mAttemptWindowStart <= 0.0)
        {
            return;
        }
        const F64 sample_window = now - mAttemptWindowStart;
        if (sample_window < 1.0)
        {
            return;
        }
        mObservedAttemptHz = static_cast<F32>(mAttemptSamples / sample_window);
        mAttemptSamples = 0;
        mAttemptWindowStart = now;
    }

    void updateCadenceEntitlements(F64 now)
    {
        static LLCachedControl<bool> adaptive(gSavedSettings, "PrismAdaptivePerformance", true);

        F32 capacity = prismCaptureBudgetHz();
        const F32 frame_opportunities = std::isfinite(gFPSClamped) && gFPSClamped > 0.f
            ? gFPSClamped : 30.f;
        if (!adaptive() && capacity == 0.f)
        {
            capacity = frame_opportunities; // Manual Every Frame sentinel.
        }
        else if (adaptive())
        {
            capacity = capacity > 0.f ? capacity : 30.f;
            // The controller owns both fast protection and slow recovery. Apply
            // its held factor even after presented FPS crosses the target; using
            // the instantaneous signal here would bypass recovery hysteresis.
            capacity *= prismAdaptiveCadenceFactor();
        }
        capacity = llclamp(capacity, 0.f, frame_opportunities);

        bool active[LLPrismLens::MAX_CAPTURES] = { false, false, false };
        F32 demand[LLPrismLens::MAX_CAPTURES] = { 0.f, 0.f, 0.f };
        F32 entitlement[LLPrismLens::MAX_CAPTURES] = { 0.f, 0.f, 0.f };
        U32 active_count = 0;
        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        {
            PrismInstance& capture = mLenses[slot];
            if (capture.mOccupied)
            {
                closePublicationWindow(capture, now);
            }
            if (capture.mOccupied && capture.mHasOutput)
            {
                capture.mRuntime.mOutput = capture.mLastRenderedFrame == gFrameCount
                    ? LLPrismLens::EOutputState::CURRENT
                    : LLPrismLens::EOutputState::HELD;
                capture.mRuntime.mOutputAgeSeconds = capture.mLastProducedTime > 0.0
                    ? static_cast<F32>(llmax(0.0, now - capture.mLastProducedTime)) : 0.f;
            }
            if (!capture.mOccupied || !capture.mAnyDisplayVisible ||
                !capture.mFrame.mPrepared || now < capture.mRetryAfterTime)
            {
                capture.mRequestedHz = 0.f;
                capture.mEntitlementHz = 0.f;
                capture.mRuntime.mCadenceEntitlementHz = 0.f;
                continue;
            }
            active[slot] = true;
            ++active_count;
            demand[slot] = capture.mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS
                ? capture.mRate.mTargetFps : capacity;
            capture.mRequestedHz = demand[slot];
        }

        // Bounded max-min water filling. A low requested rate is satisfied first;
        // the remainder is shared fairly by producers that can still use it.
        F32 remaining = capacity;
        U32 remaining_count = active_count;
        bool assigned[LLPrismLens::MAX_CAPTURES] = { false, false, false };
        while (remaining_count > 0 && remaining > 0.f)
        {
            const F32 share = remaining / static_cast<F32>(remaining_count);
            bool fixed_one = false;
            for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
            {
                if (!active[slot] || assigned[slot] || demand[slot] > share) continue;
                entitlement[slot] = demand[slot];
                remaining = llmax(0.f, remaining - entitlement[slot]);
                assigned[slot] = true;
                --remaining_count;
                fixed_one = true;
            }
            if (!fixed_one)
            {
                for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
                {
                    if (active[slot] && !assigned[slot]) entitlement[slot] = share;
                }
                break;
            }
        }

        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        {
            PrismInstance& capture = mLenses[slot];
            const F32 previous_entitlement = capture.mEntitlementHz;
            capture.mEntitlementHz = entitlement[slot];
            capture.mRuntime.mCadenceEntitlementHz = entitlement[slot];
            if (active[slot])
            {
                if (entitlement[slot] > previous_entitlement + 0.001f &&
                    entitlement[slot] > 0.f && capture.mNextDueTime > now)
                {
                    // A first, tiny recovery entitlement can create a very
                    // distant deadline. Pull only a future deadline toward the
                    // newly admitted period; never make it overdue or grant
                    // catch-up credit.
                    capture.mNextDueTime = llmin(
                        capture.mNextDueTime,
                        now + 1.0 / static_cast<F64>(entitlement[slot]));
                }
                if (adaptive() && capacity <= 0.f)
                {
                    capture.mRuntime.mActivity = LLPrismLens::EActivityState::PAUSED;
                }
                else
                {
                    capture.mRuntime.mActivity = entitlement[slot] + 0.01f < demand[slot]
                        ? LLPrismLens::EActivityState::THROTTLED
                        : LLPrismLens::EActivityState::WAITING;
                }
                if (capture.mNextDueTime <= 0.0) capture.mNextDueTime = now;
            }
        }
        ++mPerformanceRevision;
    }

    S32 chooseRenderSlot()
    {
        const F64 now = LLTimer::getTotalSeconds();
        updateCadenceEntitlements(now);
        closeAttemptWindow(now);
        const bool every_frame = prismManualEveryFrameMode();
        // The zero-Hz manual setting is a frame token, not an estimated rate.
        // Even if this entry point is reached twice in one presented frame it
        // may admit at most one auxiliary attempt.
        if (every_frame && mLastEveryFrameAttemptFrame == gFrameCount)
        {
            return -1;
        }
        S32 best_slot = -1;
        F64 best_overdue = -std::numeric_limits<F64>::max();
        F64 best_attempt_age = -1.0;
        U64 best_area = 0;
        for (U32 offset = 0; offset < LLPrismLens::MAX_CAPTURES; ++offset)
        {
            const U32 slot = (mNextRenderSlot + offset) % LLPrismLens::MAX_CAPTURES;
            const PrismInstance& capture = mLenses[slot];
            if (!capture.mOccupied || !capture.mAnyDisplayVisible ||
                !capture.mFrame.mPrepared || now < capture.mRetryAfterTime)
            {
                continue;
            }
            if (every_frame)
            {
                // Automatic producers are eligible on every global frame token.
                // Target-FPS producers retain an independent monotonic deadline,
                // so the token cannot make a 5-FPS camera publish at 30 FPS.
                if (capture.mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS &&
                    now < capture.mNextDueTime)
                {
                    continue;
                }
                best_slot = static_cast<S32>(slot);
                break; // deterministic round robin among currently due captures
            }
            if (capture.mEntitlementHz <= 0.f || now < capture.mNextDueTime)
            {
                continue;
            }
            const F64 overdue = now - capture.mNextDueTime;
            const F64 attempt_age = capture.mLastAttemptTime > 0.0
                ? now - capture.mLastAttemptTime : std::numeric_limits<F64>::max();
            const U64 area = static_cast<U64>(capture.mFrame.mTargetWidth) *
                             static_cast<U64>(capture.mFrame.mTargetHeight);
            if (best_slot < 0 || overdue > best_overdue ||
                (overdue == best_overdue && attempt_age > best_attempt_age) ||
                (overdue == best_overdue && attempt_age == best_attempt_age &&
                 area > best_area))
            {
                best_slot = static_cast<S32>(slot);
                best_overdue = overdue;
                best_attempt_age = attempt_age;
                best_area = area;
            }
        }
        if (best_slot >= 0)
        {
            PrismInstance& capture = mLenses[best_slot];
            capture.mLastAttemptTime = now;
            if (every_frame)
            {
                mLastEveryFrameAttemptFrame = gFrameCount;
                if (capture.mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS)
                {
                    const F64 period = 1.0 / llmax(1.f, capture.mRate.mTargetFps);
                    const F64 phase_next = capture.mNextDueTime + period;
                    capture.mNextDueTime = phase_next <= now ? now + period : phase_next;
                }
                else
                {
                    capture.mNextDueTime = now;
                }
            }
            else
            {
                const F64 period = 1.0 / llmax(0.01f, capture.mEntitlementHz);
                const F64 phase_next = capture.mNextDueTime + period;
                // Preserve phase across ordinary main-frame quantization. If a full
                // extra period was missed, discard backlog instead of catching up.
                capture.mNextDueTime = phase_next <= now ? now + period : phase_next;
            }
            mNextRenderSlot = (static_cast<U32>(best_slot) + 1) %
                              LLPrismLens::MAX_CAPTURES;
            if (mAttemptWindowStart <= 0.0) mAttemptWindowStart = now;
            ++mAttemptSamples;
        }
        return best_slot;
    }

    void markProduced(U32 slot, U32 output_width, U32 output_height)
    {
        if (slot >= LLPrismLens::MAX_LENSES)
        {
            return;
        }
        PrismInstance& lens = mLenses[slot];
        if (lens.mOccupied && lens.mFrame.mFrame == gFrameCount && lens.mFrame.mPrepared)
        {
            lens.mFrame.mProduced = true;
            lens.mHasOutput = true;
            lens.mOutputWidth = output_width;
            lens.mOutputHeight = output_height;
            lens.mLastRenderedFrame = gFrameCount;
            lens.mRetryAfterTime = 0.0;
            const F64 now = LLTimer::getTotalSeconds();
            lens.mLastProducedTime = now;
            if (lens.mPublicationWindowStart <= 0.0)
            {
                lens.mPublicationWindowStart = now;
            }
            ++lens.mPublicationSamples;
            closePublicationWindow(lens, now);
            lens.mRuntime.mOutput = LLPrismLens::EOutputState::CURRENT;
            const bool cadence_throttled = lens.mRequestedHz > 0.f &&
                lens.mEntitlementHz + 0.01f < lens.mRequestedHz;
            const bool scale_throttled =
                lens.mRuntime.mEffectiveResolutionScale + 0.01f <
                    prismResolutionScale();
            lens.mRuntime.mActivity = cadence_throttled || scale_throttled
                ? LLPrismLens::EActivityState::THROTTLED
                : LLPrismLens::EActivityState::LIVE;
            lens.mRuntime.mOutputAgeSeconds = 0.f;
            std::memcpy(lens.mOutputUvScale, lens.mFrame.mCompositeUvScale,
                        sizeof(lens.mOutputUvScale));
            std::memcpy(lens.mOutputUvOffset, lens.mFrame.mCompositeUvOffset,
                        sizeof(lens.mOutputUvOffset));
            mLastRenderedSlot = static_cast<S32>(slot);
            ++mRuntimeRevision;
        }
    }

    void invalidateOutput(U32 slot, U32 retry_frames = 0)
    {
        if (slot < LLPrismLens::MAX_LENSES)
        {
            PrismInstance& lens = mLenses[slot];
            lens.mHasOutput = false;
            lens.mOutputWidth = 0;
            lens.mOutputHeight = 0;
            lens.mFrame.mProduced = false;
            lens.mRetryAfterTime = LLTimer::getTotalSeconds() +
                static_cast<F64>(retry_frames) / 30.0;
            lens.mRuntime.mOutput = LLPrismLens::EOutputState::EMPTY;
            lens.mRuntime.mActivity = LLPrismLens::EActivityState::WAITING;
            ++mRuntimeRevision;
        }
    }

    void deferRetry(U32 slot, U32 retry_frames)
    {
        if (slot < LLPrismLens::MAX_LENSES && mLenses[slot].mOccupied)
        {
            mLenses[slot].mRetryAfterTime = LLTimer::getTotalSeconds() +
                static_cast<F64>(retry_frames) / 30.0;
        }
    }

    const PrismFrame* frame(U32 slot) const
    {
        return slot < LLPrismLens::MAX_LENSES && mLenses[slot].mOccupied
            ? &mLenses[slot].mFrame : nullptr;
    }

    const PrismInstance* capture(U32 slot) const
    {
        return slot < LLPrismLens::MAX_CAPTURES && mLenses[slot].mOccupied
            ? &mLenses[slot] : nullptr;
    }

    const PrismFrame* activeFrame() const
    {
        return mActiveSlot >= 0 ? frame(static_cast<U32>(mActiveSlot)) : nullptr;
    }

    bool activeCaptureIsSurfaceLens() const
    {
        return mActiveSlot >= 0 &&
            mLenses[mActiveSlot].mOccupied &&
            mLenses[mActiveSlot].mMode == LLPrismLens::ECaptureMode::SURFACE_LENS;
    }

    void setActiveSlot(U32 slot)
    {
        mActiveSlot = slot < LLPrismLens::MAX_LENSES ? static_cast<S32>(slot) : -1;
    }

    void clearActiveSlot() { mActiveSlot = -1; }

    bool getOutputRegion(U32 slot, U32& width, U32& height) const
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mHasOutput)
        {
            return false;
        }
        width = mLenses[slot].mOutputWidth;
        height = mLenses[slot].mOutputHeight;
        return width > 0 && height > 0;
    }

    bool getOutputOrientation(U32 slot, F32 scale[2], F32 offset[2]) const
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mHasOutput)
        {
            return false;
        }
        std::memcpy(scale, mLenses[slot].mOutputUvScale,
                    sizeof(mLenses[slot].mOutputUvScale));
        std::memcpy(offset, mLenses[slot].mOutputUvOffset,
                    sizeof(mLenses[slot].mOutputUvOffset));
        return true;
    }

    S32 lastRenderedSlot() const { return mLastRenderedSlot; }

    bool setCamera(const LLPrismLens::CaptureHandle& handle, std::string* reason)
    {
        const LLPrismLens::ActionStatus status = cameraSelectionStatus(&handle);
        if (!status.allowed())
        {
            if (reason) *reason = status.mReason;
            return false;
        }
        LLUUID object_id;
        selectedObjectIdentity(object_id);
        const S32 slot = findCapture(handle);
        PrismInstance& capture = mLenses[slot];
        if (capture.mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
        {
            if (reason) *reason = "Only Camera Feed captures have a source camera.";
            return false;
        }
        capture.mCameraObjectId = object_id;
        suppressOutput(static_cast<U32>(slot));
        ++mRevision;
        if (reason) reason->clear();
        return true;
    }

    static bool validCameraSettings(const LLPrismLens::CameraSettings& settings,
                                    std::string* reason)
    {
        if (settings.mFovMode != LLPrismLens::EFovMode::FIXED &&
            settings.mFovMode != LLPrismLens::EFovMode::FOLLOW_PROJECTOR)
        {
            if (reason) *reason = "Unknown camera FOV mode.";
            return false;
        }
        const bool finite_offset = settings.mLocalEyeOffset.isFinite();
        const bool valid_fov = std::isfinite(settings.mFixedVerticalFovRad) &&
            settings.mFixedVerticalFovRad >= 5.f * DEG_TO_RAD &&
            settings.mFixedVerticalFovRad <= 175.f * DEG_TO_RAD;
        const bool valid_near = std::isfinite(settings.mNearClip) &&
            settings.mNearClip >= 0.01f && settings.mNearClip <= 10.f;
        const bool valid_far = std::isfinite(settings.mFarClip) &&
            settings.mFarClip >= 0.2f && settings.mFarClip <= 512.f &&
            settings.mFarClip >= settings.mNearClip + 0.1f;
        const bool valid_aspect = std::isfinite(settings.mOutputAspect) &&
            settings.mOutputAspect >= 0.25f && settings.mOutputAspect <= 4.f;
        const bool valid_optics =
            std::isfinite(settings.mOptics.mChromaticAberration) &&
            settings.mOptics.mChromaticAberration >= 0.f && settings.mOptics.mChromaticAberration <= 1.f &&
            std::isfinite(settings.mOptics.mFilmGrain) &&
            settings.mOptics.mFilmGrain >= 0.f && settings.mOptics.mFilmGrain <= 1.f &&
            std::isfinite(settings.mOptics.mCRTScanlines) &&
            settings.mOptics.mCRTScanlines >= 0.f && settings.mOptics.mCRTScanlines <= 1.f &&
            std::isfinite(settings.mOptics.mExposureBias) &&
            settings.mOptics.mExposureBias >= -4.f && settings.mOptics.mExposureBias <= 4.f;
        if (!finite_offset || !valid_fov || !valid_near || !valid_far || !valid_aspect || !valid_optics)
        {
            if (reason)
            {
                *reason = "Camera settings require finite FOV 5-175 degrees, near 0.01-10 m, "
                          "far 0.2-512 m (at least near+0.1), finite offset, aspect 0.25-4, and valid optics.";
            }
            return false;
        }
        // Prim-free virtual camera: the stored transform must be finite and the
        // orientation a (near-)unit quaternion. FOLLOW_PROJECTOR has no spotlight
        // object to read when objectless, so it is rejected here; every mutation
        // site soft-corrects to FIXED before calling this, so a user never hits
        // the rejection. Non-virtual captures skip this block entirely.
        if (settings.mVirtual)
        {
            if (settings.mFovMode == LLPrismLens::EFovMode::FOLLOW_PROJECTOR)
            {
                if (reason) *reason = "A virtual camera cannot use Follow Projector; use a fixed vertical FOV.";
                return false;
            }
            if (!settings.mVirtualPos.isFinite() || !settings.mVirtualRot.isFinite())
            {
                if (reason) *reason = "A virtual camera requires a finite stored position and orientation.";
                return false;
            }
            const F32 q_mag = std::sqrt(
                settings.mVirtualRot.mQ[VX] * settings.mVirtualRot.mQ[VX] +
                settings.mVirtualRot.mQ[VY] * settings.mVirtualRot.mQ[VY] +
                settings.mVirtualRot.mQ[VZ] * settings.mVirtualRot.mQ[VZ] +
                settings.mVirtualRot.mQ[VS] * settings.mVirtualRot.mQ[VS]);
            if (!std::isfinite(q_mag) || std::fabs(q_mag - 1.f) > 1e-3f)
            {
                if (reason) *reason = "A virtual camera orientation must be a unit quaternion.";
                return false;
            }
        }
        return true;
    }

    bool setCameraSettings(const LLPrismLens::CaptureHandle& handle,
                           const LLPrismLens::CameraSettings& settings,
                           std::string* reason)
    {
        const S32 slot = findCapture(handle);
        if (slot < 0)
        {
            if (reason) *reason = "That capture handle is stale.";
            return false;
        }
        PrismInstance& capture = mLenses[slot];
        if (capture.mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
        {
            if (reason) *reason = "Surface Lens captures do not use camera optics.";
            return false;
        }
        // A virtual (prim-free) camera has no spotlight object to follow, so a
        // stale FOLLOW_PROJECTOR selection is soft-corrected to FIXED here rather
        // than rejected. For a non-virtual capture `corrected` == `settings`, so
        // the object-anchored path stays byte-identical.
        LLPrismLens::CameraSettings corrected = settings;
        if (corrected.mVirtual)
        {
            corrected.mFovMode = LLPrismLens::EFovMode::FIXED;
        }
        if (!validCameraSettings(corrected, reason)) return false;
        if (!corrected.mVirtual &&
            corrected.mFovMode == LLPrismLens::EFovMode::FOLLOW_PROJECTOR)
        {
            LLVOVolume* source = dynamic_cast<LLVOVolume*>(
                gObjectList.findObject(capture.mCameraObjectId));
            if (source && !source->isLightSpotlight())
            {
                if (reason) *reason = "Follow Projector requires a spotlight projector source.";
                return false;
            }
        }
        capture.mCamera = corrected;
        suppressOutput(static_cast<U32>(slot));
        ++mRevision;
        if (reason) reason->clear();
        return true;
    }

    bool setRateSettings(const LLPrismLens::CaptureHandle& handle,
                         const LLPrismLens::CaptureRateSettings& settings,
                         std::string* reason)
    {
        const S32 slot = findCapture(handle);
        if (slot < 0)
        {
            if (reason) *reason = "That capture handle is stale.";
            return false;
        }
        if ((settings.mMode != LLPrismLens::EOutputRateMode::AUTOMATIC &&
             settings.mMode != LLPrismLens::EOutputRateMode::TARGET_FPS) ||
            !std::isfinite(settings.mTargetFps) || settings.mTargetFps < 1.f ||
            settings.mTargetFps > 30.f)
        {
            if (reason) *reason = "Picture FPS must be between 1 and 30.";
            return false;
        }
        mLenses[slot].mRate = settings;
        // A cadence edit does not blank a valid retained image or grant backlog.
        const F64 now = LLTimer::getTotalSeconds();
        const F64 requested_period = 1.0 / static_cast<F64>(settings.mTargetFps);
        mLenses[slot].mNextDueTime = llmax(
            now, mLenses[slot].mLastAttemptTime + requested_period);
        ++mRevision;
        if (reason) reason->clear();
        return true;
    }

    static bool validDisplaySettings(const LLPrismLens::DisplaySettings& settings,
                                     std::string* reason)
    {
        if (settings.mFitMode != LLPrismLens::EFitMode::FIT &&
            settings.mFitMode != LLPrismLens::EFitMode::FILL &&
            settings.mFitMode != LLPrismLens::EFitMode::STRETCH)
        {
            if (reason) *reason = "Unknown display fit mode.";
            return false;
        }
        for (F32 value : settings.mAnchor)
        {
            if (!std::isfinite(value) || value < 0.f || value > 1.f)
            {
                if (reason) *reason = "Display anchor components must be between 0 and 1.";
                return false;
            }
        }
        for (F32 value : settings.mBarColorLinear)
        {
            if (!std::isfinite(value) || value < 0.f || value > 1.f)
            {
                if (reason) *reason = "Display bar color components must be between 0 and 1.";
                return false;
            }
        }
        // Prim-free virtual screen: the stored transform must be finite, the
        // orientation a (near-)unit quaternion, and the size positive-finite so a
        // corrupt persisted record is caught here instead of drawing a degenerate
        // quad. A non-virtual display skips this block entirely, so the real-face
        // path is unchanged.
        if (settings.mVirtual)
        {
            if (!settings.mPos.isFinite() || !settings.mRot.isFinite())
            {
                if (reason) *reason = "A virtual screen requires a finite stored position and orientation.";
                return false;
            }
            const F32 q_mag = std::sqrt(
                settings.mRot.mQ[VX] * settings.mRot.mQ[VX] +
                settings.mRot.mQ[VY] * settings.mRot.mQ[VY] +
                settings.mRot.mQ[VZ] * settings.mRot.mQ[VZ] +
                settings.mRot.mQ[VS] * settings.mRot.mQ[VS]);
            if (!std::isfinite(q_mag) || std::fabs(q_mag - 1.f) > 1e-3f)
            {
                if (reason) *reason = "A virtual screen orientation must be a unit quaternion.";
                return false;
            }
            if (!std::isfinite(settings.mWidth) || !std::isfinite(settings.mHeight) ||
                settings.mWidth <= F_ALMOST_ZERO || settings.mHeight <= F_ALMOST_ZERO)
            {
                if (reason) *reason = "A virtual screen requires a positive, finite width and height.";
                return false;
            }
        }
        return true;
    }

    bool setDisplaySettings(const LLPrismLens::DisplayHandle& handle,
                            const LLPrismLens::DisplaySettings& settings,
                            std::string* reason)
    {
        const S32 slot = findDisplay(handle);
        if (slot < 0)
        {
            if (reason) *reason = "That display handle is stale.";
            return false;
        }
        if (!validDisplaySettings(settings, reason)) return false;
        mDisplays[slot].mSettings = settings;
        // Screen effects are creative strengths, not a schedule or mapping:
        // out-of-range values are silently clamped rather than rejected so a
        // stale UI can never wedge a display into an uneditable state.
        mDisplays[slot].mSettings.mEffects.clampAndValidate();
        ++mRevision;
        if (reason) reason->clear();
        return true;
    }

    bool removeDisplay(const LLPrismLens::DisplayHandle& handle, std::string* reason)
    {
        const S32 slot = findDisplay(handle);
        if (slot < 0)
        {
            if (reason) *reason = "That display handle is stale.";
            return false;
        }
        PrismDisplay& display = mDisplays[slot];
        if (display.mCaptureSlot >= LLPrismLens::MAX_CAPTURES ||
            !mLenses[display.mCaptureSlot].mOccupied)
        {
            if (reason) *reason = "The display's capture is no longer valid.";
            return false;
        }
        const U32 capture_slot = display.mCaptureSlot;
        PrismInstance& capture = mLenses[capture_slot];
        if (capture.mMode == LLPrismLens::ECaptureMode::SURFACE_LENS)
        {
            if (reason) *reason = "Remove the Surface Lens capture to remove its required display.";
            return false;
        }
        if (capture.mDisplayCount > 0) --capture.mDisplayCount;
        display = PrismDisplay();
        if (capture.mDisplayCount == 0)
        {
            gPipeline.releasePrismLensOutput(capture_slot);
            // Scratch packs are pooled, not per-slot; releasing frees the whole
            // pool and surviving captures lazily re-acquire on their next render.
            gPipeline.releasePrismLensBuffers();
            capture.mHasOutput = false;
            capture.mOutputWidth = 0;
            capture.mOutputHeight = 0;
            capture.mLastRenderedFrame = 0;
            capture.mLastProducedTime = 0.0;
            capture.mPublicationSamples = 0;
            capture.mPublicationWindowStart = 0.0;
            capture.mRuntime.mOutput = LLPrismLens::EOutputState::EMPTY;
            capture.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
            capture.mRuntime.mObservedPublicationHz = 0.f;
            capture.mRuntime.mOutputAgeSeconds = 0.f;
            if (mLastRenderedSlot == static_cast<S32>(capture_slot))
            {
                mLastRenderedSlot = -1;
            }
            ++mRuntimeRevision;
        }
        ++mRevision;
        if (reason) reason->clear();
        return true;
    }

    bool removeCapture(const LLPrismLens::CaptureHandle& handle)
    {
        const S32 slot = findCapture(handle);
        if (slot < 0) return false;
        clearSlot(static_cast<U32>(slot));
        return true;
    }

    U64 configurationRevision() const { return mRevision; }

    U64 runtimeRevision() const
    {
        refreshRuntimeRevision();
        return mRuntimeRevision;
    }

    LLPrismLens::RegistrySnapshot snapshot() const
    {
        refreshRuntimeRevision();
        LLPrismLens::RegistrySnapshot result;
        result.mConfigurationRevision = mRevision;
        result.mRuntimeRevision = mRuntimeRevision;
        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        {
            const PrismInstance& capture = mLenses[slot];
            if (!capture.mOccupied) continue;
            LLPrismLens::CaptureDefinition& out = result.mCaptures[result.mCaptureCount++];
            out.mSlot = slot;
            out.mHandle = capture.mHandle;
            out.mMode = capture.mMode;
            out.mCameraObjectId = capture.mCameraObjectId;
            out.mCamera = capture.mCamera;
            out.mRate = capture.mRate;
            out.mRuntime = capture.mRuntime;
            out.mDisplayCount = capture.mDisplayCount;
        }
        for (const PrismDisplay& display : mDisplays)
        {
            if (!display.mOccupied || display.mCaptureSlot >= LLPrismLens::MAX_CAPTURES ||
                !mLenses[display.mCaptureSlot].mOccupied)
            {
                continue;
            }
            LLPrismLens::DisplayDefinition& out = result.mDisplays[result.mDisplayCount++];
            out.mHandle = display.mHandle;
            out.mCapture = mLenses[display.mCaptureSlot].mHandle;
            out.mDisplayObjectId = display.mObjectId;
            out.mDisplayTextureEntry = display.mTE;
            out.mSettings = display.mSettings;
            out.mRuntime = display.mRuntime;
        }
        return result;
    }

    LLPrismLens::PerformanceSnapshot performanceSnapshot() const
    {
        const bool adaptive = prismAdaptiveEnabled();
        const PrismAdaptiveController& controller = prismAdaptiveController();
        const F32 cadence_factor = adaptive ? controller.cadenceFactor() : 1.f;
        LLPrismLens::PerformanceSnapshot result;
        result.mRevision = mPerformanceRevision;
        result.mAdaptiveEnabled = adaptive;
        result.mRequestedProtectedMainFps = prismProtectedMainFps();
        result.mEffectiveProtectedMainFps = result.mRequestedProtectedMainFps;
        result.mRequestedTargetAvailable = controller.presentedFpsValid();
        result.mPresentedFps = controller.presentedFpsValid() &&
            std::isfinite(gFPSClamped) ? gFPSClamped : 0.f;
        result.mUserResolutionScaleCeiling = prismResolutionScale();
        result.mRequestedTotalCaptureBudgetHz = prismCaptureBudgetHz();
        result.mEffectiveTotalCaptureBudgetHz = adaptive
            ? (result.mRequestedTotalCaptureBudgetHz > 0.f
                ? result.mRequestedTotalCaptureBudgetHz : 30.f)
            : result.mRequestedTotalCaptureBudgetHz;
        if (adaptive)
        {
            result.mEffectiveTotalCaptureBudgetHz *= cadence_factor;
            if (result.mPresentedFps > 0.f)
            {
                result.mEffectiveTotalCaptureBudgetHz = llmin(
                    result.mEffectiveTotalCaptureBudgetHz,
                    result.mPresentedFps);
            }
        }
        const bool protecting = adaptive &&
            (cadence_factor < 0.999f ||
             controller.resolutionFactor() < 0.999f ||
             controller.downshiftPending() || controller.recovering() ||
             controller.recoveryVetoed());
        if (!prismEnabled())
        {
            result.mState = LLPrismLens::EPerformanceState::DISABLED;
        }
        else if (adaptive && !controller.presentedFpsValid())
        {
            result.mState = LLPrismLens::EPerformanceState::LEARNING;
        }
        else if (adaptive && cadence_factor <= 0.0001f)
        {
            result.mState = LLPrismLens::EPerformanceState::SUSPENDED;
        }
        else
        {
            result.mState = protecting
                ? LLPrismLens::EPerformanceState::PROTECTING
                : LLPrismLens::EPerformanceState::STEADY;
        }
        F32 min_scale = 0.f;
        F32 max_scale = 0.f;
        F32 admitted = 0.f;
        for (const PrismInstance& capture : mLenses)
        {
            if (!capture.mOccupied || !capture.mAnyDisplayVisible) continue;
            const F32 scale = capture.mRuntime.mEffectiveResolutionScale;
            min_scale = min_scale == 0.f ? scale : llmin(min_scale, scale);
            max_scale = llmax(max_scale, scale);
            admitted += capture.mEntitlementHz;
        }
        result.mMinimumAppliedResolutionScale = min_scale;
        result.mMaximumAppliedResolutionScale = max_scale;
        result.mAdmittedGlobalAttemptRateHz = admitted;
        result.mObservedGlobalAttemptRateHz = mObservedAttemptHz;
        result.mGpuTimingReliable = false;
        if (!prismEnabled())
        {
            result.mReason = "Prism rendering is disabled; definitions are retained and GPU outputs released.";
        }
        else if (!hasDesignation())
        {
            result.mReason = "No Prism captures are configured; no auxiliary work is scheduled and adaptive history is reset.";
        }
        else if (adaptive && !controller.presentedFpsValid())
        {
            result.mReason = "Presented-FPS signal is unavailable; adaptive protection is holding without increasing work. GPU timing is unavailable.";
        }
        else if (adaptive && cadence_factor <= 0.0001f)
        {
            result.mReason = "Presented FPS remained below the protection band; auxiliary captures are suspended and retained output is held. This FPS-only guard does not attribute the slowdown to Prism.";
        }
        else if (adaptive && controller.recoveryVetoed() &&
                 controller.noAuxReferenceValid())
        {
            result.mReason = llformat(
                "Recovery is held because the recent %.1f FPS no-aux reference remains below the recovery band. This conservative veto does not attribute cost to Prism; GPU timing is unavailable.",
                controller.noAuxReferenceFps());
        }
        else if (adaptive && controller.recovering())
        {
            result.mReason = "Presented FPS stayed inside the recovery band for two seconds; cadence and resolution are recovering slowly with no catch-up. Protection is FPS-only and does not attribute cost to Prism.";
        }
        else if (adaptive && controller.downshiftPending())
        {
            result.mReason = "Presented FPS is below the protection band; a short persistence dwell is filtering a transient before fast downshift. GPU timing is unavailable.";
        }
        else if (adaptive && protecting)
        {
            result.mReason = "Adaptive protection is holding reduced cadence or resolution until sustained safe presented FPS permits slow recovery. GPU timing is unavailable and no Prism-cost attribution is inferred.";
        }
        else if (adaptive)
        {
            result.mReason = "Presented-FPS protection is steady. GPU timing is unavailable and no per-Prism cost attribution is inferred.";
        }
        else
        {
            result.mReason = "Manual total-capture budget active.";
        }
        return result;
    }

    LLSD sceneData() const
    {
        LLSD result = LLSD::emptyMap();
        result["prism_captures"] = LLSD::emptyArray();
        result["prism_displays"] = LLSD::emptyArray();
        for (const PrismInstance& capture : mLenses)
        {
            if (!capture.mOccupied) continue;
            LLSD item = LLSD::emptyMap();
            item["capture_id"] = capture.mHandle.mId;
            item["mode"] = capture.mMode == LLPrismLens::ECaptureMode::CAMERA_FEED
                ? "camera_feed" : "surface_lens";
            item["output_rate_mode"] =
                capture.mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS
                    ? "target_fps" : "automatic";
            item["target_output_fps"] = capture.mRate.mTargetFps;
            if (capture.mMode == LLPrismLens::ECaptureMode::CAMERA_FEED)
            {
                item["camera_id"] = capture.mCameraObjectId;
                item["fov_mode"] = capture.mCamera.mFovMode == LLPrismLens::EFovMode::FOLLOW_PROJECTOR
                    ? "follow_projector" : "fixed";
                item["fixed_vertical_fov_radians"] = capture.mCamera.mFixedVerticalFovRad;
                item["near_clip"] = capture.mCamera.mNearClip;
                item["far_clip"] = capture.mCamera.mFarClip;
                LLSD offset = LLSD::emptyArray();
                offset.append(capture.mCamera.mLocalEyeOffset.mV[VX]);
                offset.append(capture.mCamera.mLocalEyeOffset.mV[VY]);
                offset.append(capture.mCamera.mLocalEyeOffset.mV[VZ]);
                item["local_eye_offset"] = offset;
                item["output_aspect"] = capture.mCamera.mOutputAspect;
                item["chromatic_aberration"] = capture.mCamera.mOptics.mChromaticAberration;
                item["film_grain"] = capture.mCamera.mOptics.mFilmGrain;
                item["crt_scanlines"] = capture.mCamera.mOptics.mCRTScanlines;
                item["exposure_bias"] = capture.mCamera.mOptics.mExposureBias;
                // Render-only camera guide (frustum gizmo). Optional on read so
                // pre-guide scenes round-trip clean; absent keys keep defaults.
                item["show_guide"]       = capture.mCamera.mShowGuide;
                item["guide_thirds"]     = capture.mCamera.mGuideThirds;
                item["guide_uproll"]     = capture.mCamera.mGuideUpRoll;
                item["guide_crosshair"]  = capture.mCamera.mGuideCrosshair;
                item["guide_clipmarkers"] = capture.mCamera.mGuideClipMarkers;
                // Prim-free virtual camera. Optional on read so pre-virtual
                // scenes round-trip clean. camera_id above is already null for a
                // virtual capture (a legal persisted state), so it round-trips.
                // No quat<->LLSD helper exists here; hand-serialize pos (3-array)
                // and rot (4-array x,y,z,w), mirroring local_eye_offset above.
                item["virtual"] = capture.mCamera.mVirtual;
                LLSD virtual_pos = LLSD::emptyArray();
                virtual_pos.append(capture.mCamera.mVirtualPos.mV[VX]);
                virtual_pos.append(capture.mCamera.mVirtualPos.mV[VY]);
                virtual_pos.append(capture.mCamera.mVirtualPos.mV[VZ]);
                item["virtual_pos"] = virtual_pos;
                LLSD virtual_rot = LLSD::emptyArray();
                virtual_rot.append(capture.mCamera.mVirtualRot.mQ[VX]);
                virtual_rot.append(capture.mCamera.mVirtualRot.mQ[VY]);
                virtual_rot.append(capture.mCamera.mVirtualRot.mQ[VZ]);
                virtual_rot.append(capture.mCamera.mVirtualRot.mQ[VS]);
                item["virtual_rot"] = virtual_rot;
            }
            result["prism_captures"].append(item);
        }
        for (const PrismDisplay& display : mDisplays)
        {
            if (!display.mOccupied || display.mCaptureSlot >= LLPrismLens::MAX_CAPTURES ||
                !mLenses[display.mCaptureSlot].mOccupied)
            {
                continue;
            }
            LLSD item = LLSD::emptyMap();
            item["binding_id"] = display.mHandle.mId;
            item["capture_id"] = mLenses[display.mCaptureSlot].mHandle.mId;
            item["display_id"] = display.mObjectId;
            item["display_te"] = display.mTE;
            switch (display.mSettings.mFitMode)
            {
                case LLPrismLens::EFitMode::FILL: item["fit"] = "fill"; break;
                case LLPrismLens::EFitMode::STRETCH: item["fit"] = "stretch"; break;
                default: item["fit"] = "fit"; break;
            }
            LLSD anchor = LLSD::emptyArray();
            anchor.append(display.mSettings.mAnchor[0]);
            anchor.append(display.mSettings.mAnchor[1]);
            item["anchor"] = anchor;
            LLSD color = LLSD::emptyArray();
            color.append(display.mSettings.mBarColorLinear[0]);
            color.append(display.mSettings.mBarColorLinear[1]);
            color.append(display.mSettings.mBarColorLinear[2]);
            item["bar_color_linear"] = color;
            // Per-display TV screen effects. The block is optional on read:
            // an absent (pre-effects) block loads as all defaults, so older
            // scenes round-trip clean.
            const LLPrismLens::ScreenEffects& fx = display.mSettings.mEffects;
            LLSD effects_sd = LLSD::emptyMap();
            effects_sd["scanlines"]      = fx.mScanlines;
            effects_sd["scanline_count"] = fx.mScanlineCount;
            effects_sd["pixelate"]       = fx.mPixelate;
            effects_sd["grayscale"]      = fx.mGrayscale;
            effects_sd["sepia"]          = fx.mSepia;
            effects_sd["static"]         = fx.mStatic;
            effects_sd["vertical_roll"]  = fx.mVerticalRoll;
            effects_sd["roll_speed"]     = fx.mRollSpeed;
            effects_sd["tracking"]       = fx.mTracking;
            effects_sd["flicker"]        = fx.mFlicker;
            effects_sd["chroma_bleed"]   = fx.mChromaBleed;
            effects_sd["vignette"]       = fx.mVignette;
            effects_sd["interlace"]      = fx.mInterlace;
            effects_sd["dropout"]        = fx.mDropout;
            effects_sd["brightness"]     = fx.mBrightness;
            effects_sd["flip_h"]         = fx.mFlipH;
            effects_sd["flip_v"]         = fx.mFlipV;
            effects_sd["rotate90"]       = fx.mRotate90;
            effects_sd["sheen"]          = fx.mSheen;
            item["screen_effects"] = effects_sd;
            // Prim-free virtual screen. Optional on read so pre-virtual scenes
            // round-trip clean. display_id/display_te above are already written
            // as null / -1 for a virtual screen (a legal persisted state that the
            // reader accepts only when "virtual" is true). No quat<->LLSD helper
            // exists here, so hand-serialize pos (3-array) and rot (4-array
            // x,y,z,w), mirroring the camera's virtual serialization.
            item["virtual"] = display.mSettings.mVirtual;
            if (display.mSettings.mVirtual)
            {
                LLSD virtual_pos = LLSD::emptyArray();
                virtual_pos.append(display.mSettings.mPos.mV[VX]);
                virtual_pos.append(display.mSettings.mPos.mV[VY]);
                virtual_pos.append(display.mSettings.mPos.mV[VZ]);
                item["virtual_pos"] = virtual_pos;
                LLSD virtual_rot = LLSD::emptyArray();
                virtual_rot.append(display.mSettings.mRot.mQ[VX]);
                virtual_rot.append(display.mSettings.mRot.mQ[VY]);
                virtual_rot.append(display.mSettings.mRot.mQ[VZ]);
                virtual_rot.append(display.mSettings.mRot.mQ[VS]);
                item["virtual_rot"] = virtual_rot;
                item["width"] = display.mSettings.mWidth;
                item["height"] = display.mSettings.mHeight;
            }
            result["prism_displays"].append(item);
        }
        return result;
    }

    bool applySceneData(const LLSD& data, std::string* reason)
    {
        struct ParsedCapture
        {
            LLUUID mId;
            LLPrismLens::ECaptureMode mMode = LLPrismLens::ECaptureMode::SURFACE_LENS;
            LLUUID mCameraId;
            LLPrismLens::CameraSettings mCamera;
            LLPrismLens::CaptureRateSettings mRate;
        };
        struct ParsedDisplay
        {
            LLUUID mId;
            LLUUID mCaptureId;
            LLUUID mObjectId;
            S32 mTE = -1;
            LLPrismLens::DisplaySettings mSettings;
        };
        const auto fail = [reason](const std::string& message)
        {
            if (reason) *reason = message;
            return false;
        };
        const auto is_numeric = [](const LLSD& value)
        {
            return value.isInteger() || value.isReal();
        };
        const auto is_numeric_array = [&is_numeric](const LLSD& value, S32 size)
        {
            if (!value.isArray() || value.size() != size)
            {
                return false;
            }
            for (S32 component = 0; component < size; ++component)
            {
                if (!is_numeric(value[component]))
                {
                    return false;
                }
            }
            return true;
        };
        if (!data.isMap() || !data.has("prism_captures") ||
            !data.has("prism_displays") || !data["prism_captures"].isArray() ||
            !data["prism_displays"].isArray())
        {
            return fail("Version-3 Prism scene data requires capture and display arrays.");
        }
        const LLSD& captures_data = data["prism_captures"];
        const LLSD& displays_data = data["prism_displays"];
        if (captures_data.size() > LLPrismLens::MAX_CAPTURES ||
            displays_data.size() > LLPrismLens::MAX_DISPLAY_BINDINGS)
        {
            return fail("Prism scene exceeds the 3-capture or 16-display resource limit.");
        }

        std::vector<ParsedCapture> parsed_captures;
        std::vector<ParsedDisplay> parsed_displays;
        std::set<LLUUID> capture_ids;
        std::set<LLUUID> binding_ids;
        std::set<std::string> display_identities;
        for (LLSD::array_const_iterator it = captures_data.beginArray();
             it != captures_data.endArray(); ++it)
        {
            const LLSD& item = *it;
            if (!item.isMap() || !item.has("capture_id") || !item.has("mode") ||
                !item.has("output_rate_mode") || !item.has("target_output_fps"))
            {
                return fail("A Prism capture is missing required version-3 fields.");
            }
            ParsedCapture parsed;
            parsed.mId = item["capture_id"].asUUID();
            if (parsed.mId.isNull() || !capture_ids.insert(parsed.mId).second)
            {
                return fail("Prism capture IDs must be nonnull and unique.");
            }
            const std::string mode = item["mode"].asString();
            if (mode == "camera_feed") parsed.mMode = LLPrismLens::ECaptureMode::CAMERA_FEED;
            else if (mode == "surface_lens") parsed.mMode = LLPrismLens::ECaptureMode::SURFACE_LENS;
            else return fail("Unknown Prism capture mode.");
            const std::string rate_mode = item["output_rate_mode"].asString();
            if (rate_mode == "automatic")
                parsed.mRate.mMode = LLPrismLens::EOutputRateMode::AUTOMATIC;
            else if (rate_mode == "target_fps")
                parsed.mRate.mMode = LLPrismLens::EOutputRateMode::TARGET_FPS;
            else return fail("Unknown Prism output-rate mode.");
            if (!is_numeric(item["target_output_fps"]))
            {
                return fail("Prism target_output_fps must be an integer or real value.");
            }
            parsed.mRate.mTargetFps = static_cast<F32>(item["target_output_fps"].asReal());
            if (!std::isfinite(parsed.mRate.mTargetFps) ||
                parsed.mRate.mTargetFps < 1.f || parsed.mRate.mTargetFps > 30.f)
            {
                return fail("Prism target_output_fps must be finite and between 1 and 30.");
            }
            if (parsed.mMode == LLPrismLens::ECaptureMode::CAMERA_FEED)
            {
                if (!item.has("camera_id") || !item.has("fov_mode") ||
                    !item.has("fixed_vertical_fov_radians") || !item.has("near_clip") ||
                    !item.has("far_clip") || !item.has("local_eye_offset") ||
                    !item.has("output_aspect") || !item["local_eye_offset"].isArray() ||
                    item["local_eye_offset"].size() != 3)
                {
                    return fail("A Camera Feed capture is missing required optics fields.");
                }
                if (!is_numeric(item["fixed_vertical_fov_radians"]) ||
                    !is_numeric(item["near_clip"]) ||
                    !is_numeric(item["far_clip"]) ||
                    !is_numeric_array(item["local_eye_offset"], 3) ||
                    !is_numeric(item["output_aspect"]))
                {
                    return fail("Camera optics and local_eye_offset components must be integer or real values.");
                }
                const LLSD& camera_id = item["camera_id"];
                if (camera_id.isUUID())
                {
                    parsed.mCameraId = camera_id.asUUID(); // Null UUID is intentionally unbound.
                }
                else if (camera_id.isString() &&
                         LLUUID::validate(camera_id.asString()))
                {
                    parsed.mCameraId.set(camera_id.asString());
                }
                else
                {
                    return fail("Prism camera_id must be a UUID (null is allowed for an unbound feed).");
                }
                const std::string fov_mode = item["fov_mode"].asString();
                if (fov_mode == "fixed") parsed.mCamera.mFovMode = LLPrismLens::EFovMode::FIXED;
                else if (fov_mode == "follow_projector")
                    parsed.mCamera.mFovMode = LLPrismLens::EFovMode::FOLLOW_PROJECTOR;
                else return fail("Unknown Prism camera FOV mode.");
                parsed.mCamera.mFixedVerticalFovRad =
                    static_cast<F32>(item["fixed_vertical_fov_radians"].asReal());
                parsed.mCamera.mNearClip = static_cast<F32>(item["near_clip"].asReal());
                parsed.mCamera.mFarClip = static_cast<F32>(item["far_clip"].asReal());
                parsed.mCamera.mLocalEyeOffset.setVec(
                    static_cast<F32>(item["local_eye_offset"][0].asReal()),
                    static_cast<F32>(item["local_eye_offset"][1].asReal()),
                    static_cast<F32>(item["local_eye_offset"][2].asReal()));
                parsed.mCamera.mOutputAspect = static_cast<F32>(item["output_aspect"].asReal());

                if (item.has("chromatic_aberration") && is_numeric(item["chromatic_aberration"]))
                {
                    parsed.mCamera.mOptics.mChromaticAberration =
                        static_cast<F32>(item["chromatic_aberration"].asReal());
                }
                if (item.has("film_grain") && is_numeric(item["film_grain"]))
                {
                    parsed.mCamera.mOptics.mFilmGrain =
                        static_cast<F32>(item["film_grain"].asReal());
                }
                if (item.has("crt_scanlines") && is_numeric(item["crt_scanlines"]))
                {
                    parsed.mCamera.mOptics.mCRTScanlines =
                        static_cast<F32>(item["crt_scanlines"].asReal());
                }
                if (item.has("exposure_bias") && is_numeric(item["exposure_bias"]))
                {
                    parsed.mCamera.mOptics.mExposureBias =
                        static_cast<F32>(item["exposure_bias"].asReal());
                }

                // Render-only camera guide flags. Optional/tolerant: an absent
                // key keeps the struct default, so old scenes load unchanged and
                // are never rejected by the required-field check above.
                if (item.has("show_guide"))
                    parsed.mCamera.mShowGuide = item["show_guide"].asBoolean();
                if (item.has("guide_thirds"))
                    parsed.mCamera.mGuideThirds = item["guide_thirds"].asBoolean();
                if (item.has("guide_uproll"))
                    parsed.mCamera.mGuideUpRoll = item["guide_uproll"].asBoolean();
                if (item.has("guide_crosshair"))
                    parsed.mCamera.mGuideCrosshair = item["guide_crosshair"].asBoolean();
                if (item.has("guide_clipmarkers"))
                    parsed.mCamera.mGuideClipMarkers = item["guide_clipmarkers"].asBoolean();

                // Prim-free virtual camera. Optional/tolerant: absent keys keep
                // the struct default (non-virtual), so old scenes load unchanged
                // and are never rejected by the required-field check above. The
                // camera_id may legally be null for a virtual capture (see the
                // "Null UUID is intentionally unbound" path above).
                if (item.has("virtual"))
                    parsed.mCamera.mVirtual = item["virtual"].asBoolean();
                if (item.has("virtual_pos") && is_numeric_array(item["virtual_pos"], 3))
                {
                    parsed.mCamera.mVirtualPos.setVec(
                        static_cast<F32>(item["virtual_pos"][0].asReal()),
                        static_cast<F32>(item["virtual_pos"][1].asReal()),
                        static_cast<F32>(item["virtual_pos"][2].asReal()));
                }
                if (item.has("virtual_rot") && is_numeric_array(item["virtual_rot"], 4))
                {
                    // Assign components directly (no re-normalization) so a
                    // corrupt persisted quaternion is caught by the unit-length
                    // check in validCameraSettings below rather than masked.
                    parsed.mCamera.mVirtualRot.mQ[VX] = static_cast<F32>(item["virtual_rot"][0].asReal());
                    parsed.mCamera.mVirtualRot.mQ[VY] = static_cast<F32>(item["virtual_rot"][1].asReal());
                    parsed.mCamera.mVirtualRot.mQ[VZ] = static_cast<F32>(item["virtual_rot"][2].asReal());
                    parsed.mCamera.mVirtualRot.mQ[VS] = static_cast<F32>(item["virtual_rot"][3].asReal());
                }
                // A virtual camera cannot follow a projector; soft-correct a
                // stale mode to FIXED so the scene loads instead of failing.
                if (parsed.mCamera.mVirtual)
                    parsed.mCamera.mFovMode = LLPrismLens::EFovMode::FIXED;

                std::string camera_reason;
                if (!validCameraSettings(parsed.mCamera, &camera_reason)) return fail(camera_reason);
            }
            parsed_captures.push_back(parsed);
        }

        for (LLSD::array_const_iterator it = displays_data.beginArray();
             it != displays_data.endArray(); ++it)
        {
            const LLSD& item = *it;
            // A prim-free virtual screen carries a stored transform + size
            // instead of an object/TE identity, so the object-identity fields are
            // required ONLY for a real (face-bound) display. Everything else is
            // common to both.
            const bool is_virtual = item.isMap() && item.has("virtual") &&
                                    item["virtual"].asBoolean();
            if (!item.isMap() || !item.has("binding_id") || !item.has("capture_id") ||
                !item.has("fit") || !item.has("anchor") ||
                !item.has("bar_color_linear") ||
                !item["anchor"].isArray() || item["anchor"].size() != 2 ||
                !item["bar_color_linear"].isArray() ||
                item["bar_color_linear"].size() != 3)
            {
                return fail("A Prism display is missing required version-3 fields.");
            }
            if (!is_virtual &&
                (!item.has("display_id") || !item.has("display_te") ||
                 !item["display_te"].isInteger()))
            {
                return fail("A Prism display is missing required version-3 fields.");
            }
            if (!is_numeric_array(item["anchor"], 2) ||
                !is_numeric_array(item["bar_color_linear"], 3))
            {
                return fail("Prism anchor and bar_color_linear components must be integer or real values.");
            }
            ParsedDisplay parsed;
            parsed.mId = item["binding_id"].asUUID();
            parsed.mCaptureId = item["capture_id"].asUUID();
            if (parsed.mId.isNull() || !binding_ids.insert(parsed.mId).second)
                return fail("Prism binding IDs must be nonnull and unique.");
            if (parsed.mCaptureId.isNull() || !capture_ids.count(parsed.mCaptureId))
                return fail("A Prism display references an unknown capture.");
            if (is_virtual)
            {
                // Prim-free screen: parse the transform + size and leave the face
                // identity null / -1. No object/TE requirement and no
                // face-uniqueness check (a virtual screen binds no face).
                if (!item.has("virtual_pos") || !is_numeric_array(item["virtual_pos"], 3) ||
                    !item.has("virtual_rot") || !is_numeric_array(item["virtual_rot"], 4) ||
                    !item.has("width") || !is_numeric(item["width"]) ||
                    !item.has("height") || !is_numeric(item["height"]))
                {
                    return fail("A virtual Prism screen is missing its transform or size.");
                }
                parsed.mObjectId.setNull();
                parsed.mTE = -1;
                parsed.mSettings.mVirtual = true;
                parsed.mSettings.mPos.setVec(
                    static_cast<F32>(item["virtual_pos"][0].asReal()),
                    static_cast<F32>(item["virtual_pos"][1].asReal()),
                    static_cast<F32>(item["virtual_pos"][2].asReal()));
                // Assign quaternion components directly (no re-normalization) so a
                // corrupt persisted orientation is caught by the unit-length check
                // in validDisplaySettings below rather than masked.
                parsed.mSettings.mRot.mQ[VX] = static_cast<F32>(item["virtual_rot"][0].asReal());
                parsed.mSettings.mRot.mQ[VY] = static_cast<F32>(item["virtual_rot"][1].asReal());
                parsed.mSettings.mRot.mQ[VZ] = static_cast<F32>(item["virtual_rot"][2].asReal());
                parsed.mSettings.mRot.mQ[VS] = static_cast<F32>(item["virtual_rot"][3].asReal());
                parsed.mSettings.mWidth = static_cast<F32>(item["width"].asReal());
                parsed.mSettings.mHeight = static_cast<F32>(item["height"].asReal());
            }
            else
            {
                parsed.mObjectId = item["display_id"].asUUID();
                parsed.mTE = item["display_te"].asInteger();
                if (parsed.mObjectId.isNull() || parsed.mTE < 0 ||
                    parsed.mTE >= static_cast<S32>(LLTEContents::MAX_TES))
                    return fail("Prism display object/face identity is invalid.");
                const std::string identity = parsed.mObjectId.asString() + ":" +
                                             llformat("%d", parsed.mTE);
                if (!display_identities.insert(identity).second)
                    return fail("A display face may be bound to only one Prism capture.");
            }
            const std::string fit = item["fit"].asString();
            if (fit == "fit") parsed.mSettings.mFitMode = LLPrismLens::EFitMode::FIT;
            else if (fit == "fill") parsed.mSettings.mFitMode = LLPrismLens::EFitMode::FILL;
            else if (fit == "stretch") parsed.mSettings.mFitMode = LLPrismLens::EFitMode::STRETCH;
            else return fail("Unknown Prism display fit mode.");
            parsed.mSettings.mAnchor[0] = static_cast<F32>(item["anchor"][0].asReal());
            parsed.mSettings.mAnchor[1] = static_cast<F32>(item["anchor"][1].asReal());
            for (S32 component = 0; component < 3; ++component)
            {
                parsed.mSettings.mBarColorLinear[component] =
                    static_cast<F32>(item["bar_color_linear"][component].asReal());
            }
            // Optional per-display screen-effects block. It loads clean and
            // never rejects the scene: an absent block, an absent key, or a
            // non-numeric/non-finite value each fall back to the struct
            // default for that knob, and everything is clamped afterwards.
            if (item.has("screen_effects") && item["screen_effects"].isMap())
            {
                const LLSD& effects_sd = item["screen_effects"];
                LLPrismLens::ScreenEffects& fx = parsed.mSettings.mEffects;
                const auto read_effect = [&effects_sd, &is_numeric](
                    const char* key, F32& destination)
                {
                    if (effects_sd.has(key) && is_numeric(effects_sd[key]))
                    {
                        const F32 value =
                            static_cast<F32>(effects_sd[key].asReal());
                        if (std::isfinite(value))
                        {
                            destination = value;
                        }
                    }
                };
                read_effect("scanlines",      fx.mScanlines);
                read_effect("scanline_count", fx.mScanlineCount);
                read_effect("pixelate",       fx.mPixelate);
                read_effect("grayscale",      fx.mGrayscale);
                read_effect("sepia",          fx.mSepia);
                read_effect("static",         fx.mStatic);
                read_effect("vertical_roll",  fx.mVerticalRoll);
                read_effect("roll_speed",     fx.mRollSpeed);
                read_effect("tracking",       fx.mTracking);
                read_effect("flicker",        fx.mFlicker);
                read_effect("chroma_bleed",   fx.mChromaBleed);
                read_effect("vignette",       fx.mVignette);
                read_effect("interlace",      fx.mInterlace);
                read_effect("dropout",        fx.mDropout);
                read_effect("brightness",     fx.mBrightness);
                read_effect("sheen",          fx.mSheen);
                // Orientation flags round-trip as LLSD booleans, but tolerate a
                // numeric encoding too. Absent keys keep the struct default
                // (false), so older scenes load bit-identical.
                const auto read_flag = [&effects_sd, &is_numeric](
                    const char* key, bool& destination)
                {
                    if (!effects_sd.has(key)) return;
                    const LLSD& value = effects_sd[key];
                    if (value.isBoolean())
                    {
                        destination = value.asBoolean();
                    }
                    else if (is_numeric(value))
                    {
                        destination = value.asReal() != 0.0;
                    }
                };
                read_flag("flip_h",   fx.mFlipH);
                read_flag("flip_v",   fx.mFlipV);
                read_flag("rotate90", fx.mRotate90);
                fx.clampAndValidate();
            }
            std::string display_reason;
            if (!validDisplaySettings(parsed.mSettings, &display_reason))
                return fail(display_reason);
            parsed_displays.push_back(parsed);
        }

        for (const ParsedCapture& capture : parsed_captures)
        {
            U32 references = 0;
            for (const ParsedDisplay& display : parsed_displays)
            {
                if (display.mCaptureId != capture.mId) continue;
                ++references;
                if (capture.mMode == LLPrismLens::ECaptureMode::CAMERA_FEED &&
                    capture.mCameraId.notNull() && capture.mCameraId == display.mObjectId)
                {
                    return fail("A camera source cannot also be one of its display objects.");
                }
            }
            if (capture.mMode == LLPrismLens::ECaptureMode::SURFACE_LENS && references != 1)
            {
                return fail("A Surface Lens must have exactly one display binding.");
            }
        }

        const U64 generations_needed = parsed_captures.size() + parsed_displays.size();
        if (mNextGeneration == 0 || generations_needed >
            std::numeric_limits<U64>::max() - mNextGeneration)
        {
            return fail("Prism handle generation space is exhausted.");
        }

        // Commit only after complete parse/cross-reference validation. Outputs
        // and the old registry remain untouched on every failure above.
        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
            gPipeline.releasePrismLensOutput(slot);
        gPipeline.releasePrismLensBuffers();
        for (PrismInstance& capture : mLenses) capture = PrismInstance();
        for (PrismDisplay& display : mDisplays) display = PrismDisplay();

        for (U32 slot = 0; slot < parsed_captures.size(); ++slot)
        {
            const ParsedCapture& parsed = parsed_captures[slot];
            PrismInstance& capture = mLenses[slot];
            capture.mOccupied = true;
            capture.mHandle.mId = parsed.mId;
            allocateGeneration(capture.mHandle.mGeneration);
            capture.mMode = parsed.mMode;
            capture.mCameraObjectId = parsed.mCameraId;
            capture.mCamera = parsed.mCamera;
            capture.mRate = parsed.mRate;
            capture.mNextDueTime = LLTimer::getTotalSeconds();
        }
        U32 display_slot = 0;
        for (const ParsedDisplay& parsed : parsed_displays)
        {
            U32 capture_slot = 0;
            while (capture_slot < parsed_captures.size() &&
                   parsed_captures[capture_slot].mId != parsed.mCaptureId)
                ++capture_slot;
            PrismInstance& capture = mLenses[capture_slot];
            PrismDisplay& display = mDisplays[display_slot++];
            display.mOccupied = true;
            display.mHandle.mId = parsed.mId;
            allocateGeneration(display.mHandle.mGeneration);
            display.mCaptureSlot = capture_slot;
            display.mCaptureGeneration = capture.mHandle.mGeneration;
            display.mObjectId = parsed.mObjectId;
            display.mTE = parsed.mTE;
            display.mSettings = parsed.mSettings;
            ++capture.mDisplayCount;
            if (capture.mMode == LLPrismLens::ECaptureMode::SURFACE_LENS)
            {
                capture.mObjectId = parsed.mObjectId;
                capture.mTE = parsed.mTE;
            }
        }
        mActiveSlot = -1;
        mLastRenderedSlot = -1;
        resetRuntimeHistory();
        ++mRevision;
        ++mRuntimeRevision;
        if (reason) reason->clear();
        return true;
    }

private:
    static void hashRuntimeValue(U64& hash, U64 value)
    {
        // FNV-1a over a fixed-width value keeps this allocation-free and stable
        // for the process lifetime; only equality is relevant.
        for (U32 byte = 0; byte < 8; ++byte)
        {
            hash ^= (value >> (byte * 8)) & 0xffu;
            hash *= 1099511628211ULL;
        }
    }

    static void hashRuntimeString(U64& hash, const std::string& value)
    {
        hashRuntimeValue(hash, value.size());
        for (unsigned char character : value)
        {
            hash ^= character;
            hash *= 1099511628211ULL;
        }
    }

    static U64 quantizedRuntimeFloat(F32 value, F32 quantum)
    {
        if (!std::isfinite(value))
        {
            return std::numeric_limits<U64>::max();
        }
        return static_cast<U64>(static_cast<S64>(ll_round(value / quantum)));
    }

    U64 calculateRuntimeSignature() const
    {
        U64 hash = 14695981039346656037ULL;
        for (const PrismInstance& capture : mLenses)
        {
            hashRuntimeValue(hash, capture.mOccupied ? 1u : 0u);
            if (!capture.mOccupied) continue;
            hashRuntimeValue(hash, capture.mHandle.mGeneration);
            hashRuntimeValue(hash, static_cast<U64>(capture.mRuntime.mHealth));
            hashRuntimeValue(hash, static_cast<U64>(capture.mRuntime.mOutput));
            hashRuntimeValue(hash, static_cast<U64>(capture.mRuntime.mActivity));
            hashRuntimeString(hash, capture.mRuntime.mReason);
            hashRuntimeValue(hash, quantizedRuntimeFloat(
                capture.mRuntime.mEffectiveVerticalFovRad, 0.001f));
            hashRuntimeValue(hash, quantizedRuntimeFloat(
                capture.mRuntime.mEffectiveFarClip, 0.01f));
            hashRuntimeValue(hash, quantizedRuntimeFloat(
                capture.mRuntime.mEffectiveResolutionScale, 0.01f));
            hashRuntimeValue(hash, quantizedRuntimeFloat(
                capture.mRuntime.mCadenceEntitlementHz, 0.1f));
            hashRuntimeValue(hash, quantizedRuntimeFloat(
                capture.mRuntime.mObservedPublicationHz, 0.1f));
        }
        for (const PrismDisplay& display : mDisplays)
        {
            hashRuntimeValue(hash, display.mOccupied ? 1u : 0u);
            if (!display.mOccupied) continue;
            hashRuntimeValue(hash, display.mHandle.mGeneration);
            hashRuntimeValue(hash, static_cast<U64>(display.mRuntime.mHealth));
            hashRuntimeValue(hash, static_cast<U64>(display.mRuntime.mVisibility));
            hashRuntimeString(hash, display.mRuntime.mHealthReason);
            hashRuntimeString(hash, display.mRuntime.mVisibilityReason);
        }
        return hash;
    }

    void refreshRuntimeRevision() const
    {
        const U64 signature = calculateRuntimeSignature();
        if (!mRuntimeSignatureInitialized)
        {
            mLastRuntimeSignature = signature;
            mRuntimeSignatureInitialized = true;
        }
        else if (signature != mLastRuntimeSignature)
        {
            mLastRuntimeSignature = signature;
            ++mRuntimeRevision;
        }
    }

    void resetRuntimeHistory()
    {
        prismAdaptiveController().forceReset();
        mObservedAttemptHz = 0.f;
        mAttemptSamples = 0;
        mAttemptWindowStart = 0.0;
        mNextRenderSlot = 0;
        mLastEveryFrameAttemptFrame = std::numeric_limits<U32>::max();
        ++mPerformanceRevision;
    }

    static void resetFrame(PrismInstance& lens, U32 frame)
    {
        lens.mFrame = PrismFrame();
        lens.mFrame.mFrame = frame;
    }

    S32 findSlot(const LLUUID& object_id, S32 te) const
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_LENSES; ++slot)
        {
            const PrismInstance& lens = mLenses[slot];
            if (lens.mOccupied && lens.mObjectId == object_id && lens.mTE == te)
            {
                return static_cast<S32>(slot);
            }
        }
        return -1;
    }

    S32 findCapture(const LLPrismLens::CaptureHandle& handle) const
    {
        if (handle.mId.isNull() || handle.mGeneration == 0)
        {
            return -1;
        }
        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        {
            const PrismInstance& capture = mLenses[slot];
            if (capture.mOccupied && capture.mHandle.mId == handle.mId &&
                capture.mHandle.mGeneration == handle.mGeneration)
            {
                return static_cast<S32>(slot);
            }
        }
        return -1;
    }

    S32 findDisplay(const LLPrismLens::DisplayHandle& handle) const
    {
        if (handle.mId.isNull() || handle.mGeneration == 0)
        {
            return -1;
        }
        for (U32 slot = 0; slot < LLPrismLens::MAX_DISPLAY_BINDINGS; ++slot)
        {
            const PrismDisplay& display = mDisplays[slot];
            if (display.mOccupied && display.mHandle.mId == handle.mId &&
                display.mHandle.mGeneration == handle.mGeneration)
            {
                return static_cast<S32>(slot);
            }
        }
        return -1;
    }

    S32 findDisplayIdentity(const LLUUID& object_id, S32 te) const
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_DISPLAY_BINDINGS; ++slot)
        {
            const PrismDisplay& display = mDisplays[slot];
            if (display.mOccupied && display.mObjectId == object_id && display.mTE == te)
            {
                return static_cast<S32>(slot);
            }
        }
        return -1;
    }

    S32 freeCaptureSlot() const
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
        {
            if (!mLenses[slot].mOccupied) return static_cast<S32>(slot);
        }
        return -1;
    }

    S32 freeDisplaySlot() const
    {
        for (U32 slot = 0; slot < LLPrismLens::MAX_DISPLAY_BINDINGS; ++slot)
        {
            if (!mDisplays[slot].mOccupied) return static_cast<S32>(slot);
        }
        return -1;
    }

    U32 displayCount() const
    {
        U32 result = 0;
        for (const PrismDisplay& display : mDisplays)
        {
            result += display.mOccupied ? 1u : 0u;
        }
        return result;
    }

    bool allocateGeneration(U64& generation)
    {
        if (mNextGeneration == 0 || mNextGeneration == std::numeric_limits<U64>::max())
        {
            generation = 0;
            return false;
        }
        generation = mNextGeneration++;
        return true;
    }

    void suppressOutput(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_CAPTURES || !mLenses[slot].mOccupied) return;
        gPipeline.releasePrismLensOutput(slot);
        // Scratch packs are pooled, not per-slot; releasing frees the whole
        // pool and surviving captures lazily re-acquire on their next render.
        gPipeline.releasePrismLensBuffers();
        PrismInstance& capture = mLenses[slot];
        capture.mHasOutput = false;
        capture.mOutputWidth = 0;
        capture.mOutputHeight = 0;
        capture.mRuntime.mOutput = LLPrismLens::EOutputState::SUPPRESSED;
        capture.mRuntime.mActivity = capture.mAnyDisplayVisible
            ? LLPrismLens::EActivityState::WAITING
            : LLPrismLens::EActivityState::IDLE;
        capture.mNextDueTime = LLTimer::getTotalSeconds();
        ++mRuntimeRevision;
    }

    void clearSlot(U32 slot)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return;
        }
        if (mActiveSlot == static_cast<S32>(slot))
        {
            mActiveSlot = -1;
        }
        if (mLastRenderedSlot == static_cast<S32>(slot))
        {
            mLastRenderedSlot = -1;
        }
        gPipeline.releasePrismLensOutput(slot);
        // Scratch packs are pooled, not per-slot; releasing frees the whole
        // pool and surviving captures lazily re-acquire on their next render.
        gPipeline.releasePrismLensBuffers();
        for (PrismDisplay& display : mDisplays)
        {
            if (display.mOccupied && display.mCaptureSlot == slot)
            {
                display = PrismDisplay();
            }
        }
        mLenses[slot] = PrismInstance();
        if (count() == 0)
        {
            resetRuntimeHistory();
        }
        ++mRevision;
        ++mRuntimeRevision;
    }

    bool rejectSlot(U32 slot, const std::string& reason)
    {
        if (slot < LLPrismLens::MAX_LENSES && mLenses[slot].mOccupied)
        {
            PrismInstance& capture = mLenses[slot];
            const bool changed = capture.mRuntime.mHealth !=
                    LLPrismLens::ECaptureHealth::INVALID_LENS_SURFACE ||
                capture.mRuntime.mReason != reason;
            capture.mRuntime.mHealth = LLPrismLens::ECaptureHealth::INVALID_LENS_SURFACE;
            capture.mRuntime.mActivity = LLPrismLens::EActivityState::PAUSED;
            capture.mRuntime.mReason = reason;
            capture.mAnyDisplayVisible = false;
            setLensDisplayRuntime(
                slot, LLPrismLens::EDisplayHealth::INVALID,
                LLPrismLens::EDisplayVisibility::UNKNOWN, reason,
                "Visibility is unknown because the lens surface is invalid.");
            if (changed)
            {
                ++mRuntimeRevision;
                LL_WARNS("PrismLens") << "Prism lens is invalid but retained: "
                                       << capture.mObjectId << " TE " << capture.mTE
                                       << ": " << reason << LL_ENDL;
            }
        }
        return false;
    }

    LLVOVolume* resolveObjectForSlot(U32 slot, bool reject_invalid)
    {
        if (slot >= LLPrismLens::MAX_LENSES || !mLenses[slot].mOccupied)
        {
            return nullptr;
        }
        const LLUUID object_id = mLenses[slot].mObjectId;
        if (object_id.isNull())
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "object identifier is invalid");
            }
            return nullptr;
        }
        LLViewerObject* object = gObjectList.findObject(object_id);
        if (!object)
        {
            // Objects can leave the local object list while crossing regions,
            // changing draw distance, or streaming back in. Keep the local
            // designation so it resumes when the UUID resolves again.
            PrismInstance& capture = mLenses[slot];
            if (capture.mRuntime.mHealth !=
                    LLPrismLens::ECaptureHealth::LENS_SURFACE_OFFLINE)
            {
                capture.mRuntime.mHealth =
                    LLPrismLens::ECaptureHealth::LENS_SURFACE_OFFLINE;
                capture.mRuntime.mActivity = LLPrismLens::EActivityState::IDLE;
                capture.mRuntime.mReason = "Lens object is outside the local object list.";
                ++mRuntimeRevision;
            }
            setLensDisplayRuntime(
                slot, LLPrismLens::EDisplayHealth::OFFLINE,
                LLPrismLens::EDisplayVisibility::UNKNOWN,
                "Lens object is outside the local object list.",
                "Visibility is unknown while the lens object is offline.");
            return nullptr;
        }
        if (object->isDead())
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "object is dead");
            }
            return nullptr;
        }
        LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
        if (!volume)
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "object is not a volume");
            }
            return nullptr;
        }
        if (object->isHUDAttachment())
        {
            if (reject_invalid)
            {
                rejectSlot(slot, "HUD attachments cannot be Prism Lenses");
            }
            return nullptr;
        }
        return volume;
    }

    PrismInstance mLenses[LLPrismLens::MAX_LENSES];
    PrismDisplay mDisplays[LLPrismLens::MAX_DISPLAY_BINDINGS];
    mutable SurfaceGeometryCache mSelectionSurfaceCache;
    U64 mRevision = 1;
    mutable U64 mRuntimeRevision = 1;
    mutable U64 mLastRuntimeSignature = 0;
    mutable bool mRuntimeSignatureInitialized = false;
    U64 mPerformanceRevision = 1;
    U64 mNextGeneration = 1;
    F32 mObservedAttemptHz = 0.f;
    U32 mAttemptSamples = 0;
    F64 mAttemptWindowStart = 0.0;
    U32 mNextRenderSlot = 0;
    U32 mLastEveryFrameAttemptFrame = std::numeric_limits<U32>::max();
    S32 mActiveSlot = -1;
    S32 mLastRenderedSlot = -1;
};

struct ScopedActivePrismSlot
{
    ScopedActivePrismSlot(PrismLensRegistry& registry, U32 slot)
        : mRegistry(registry)
    {
        mRegistry.setActiveSlot(slot);
    }

    ~ScopedActivePrismSlot()
    {
        mRegistry.clearActiveSlot();
    }

    PrismLensRegistry& mRegistry;
};

struct PrismShaderDirtyState
{
    LLGLSLShader* mShader = nullptr;
    bool mSavedDirty = false;
};

struct PrismEnvironmentScratch
{
    LLEnvironment::ShaderUniformState mUniforms;
    // Capacity is retained between captures. Pointers are cleared at scope exit
    // and are never retained across a shader reload.
    std::vector<PrismShaderDirtyState> mShaderDirtyStates;
    bool mActive = false;
};

PrismEnvironmentScratch& prismEnvironmentScratch()
{
    static PrismEnvironmentScratch scratch;
    return scratch;
}

void collectEnvironmentShaders(
    std::vector<PrismShaderDirtyState>& shader_states)
{
    shader_states.clear();
    for (auto shader = LLViewerShaderMgr::instance()->beginShaders();
         shader != LLViewerShaderMgr::instance()->endShaders(); ++shader)
    {
        shader_states.push_back({ &*shader, false });
        if (shader->mRiggedVariant)
        {
            shader_states.push_back({ shader->mRiggedVariant, false });
        }
        for (LLGLSLShader& variant : shader->mGLTFVariants)
        {
            shader_states.push_back({ &variant, false });
        }
    }

    // The base list is already unique by shader-manager invariant. Sorting also
    // protects against a future base/rigged/GLTF alias without per-capture node
    // allocations from a set.
    std::sort(shader_states.begin(), shader_states.end(),
        [](const PrismShaderDirtyState& lhs,
           const PrismShaderDirtyState& rhs)
        {
            return std::less<LLGLSLShader*>()(lhs.mShader, rhs.mShader);
        });
    shader_states.erase(
        std::unique(shader_states.begin(), shader_states.end(),
            [](const PrismShaderDirtyState& lhs,
               const PrismShaderDirtyState& rhs)
            {
                return lhs.mShader == rhs.mShader;
            }),
        shader_states.end());
}

LLRender::eBlendFactor blendFactorFromGL(GLint factor)
{
    switch (factor)
    {
        case GL_ONE:                     return LLRender::BF_ONE;
        case GL_ZERO:                    return LLRender::BF_ZERO;
        case GL_DST_COLOR:               return LLRender::BF_DEST_COLOR;
        case GL_SRC_COLOR:               return LLRender::BF_SOURCE_COLOR;
        case GL_ONE_MINUS_DST_COLOR:     return LLRender::BF_ONE_MINUS_DEST_COLOR;
        case GL_ONE_MINUS_SRC_COLOR:     return LLRender::BF_ONE_MINUS_SOURCE_COLOR;
        case GL_DST_ALPHA:               return LLRender::BF_DEST_ALPHA;
        case GL_SRC_ALPHA:               return LLRender::BF_SOURCE_ALPHA;
        case GL_ONE_MINUS_DST_ALPHA:     return LLRender::BF_ONE_MINUS_DEST_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA:     return LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
        default:                         return LLRender::BF_UNDEF;
    }
}

struct ScopedPrismRenderState
{
    ScopedPrismRenderState()
        : mSavedCamera(LLViewerCamera::instance()),
          mSavedProjection(get_current_projection()),
          mSavedModelview(get_current_modelview()),
          mSavedLastProjection(get_last_projection()),
          mSavedLastModelview(get_last_modelview()),
          mSavedGLProjection(gGL.getMatrix(LLRender::MM_PROJECTION)),
          mSavedGLModelview(gGL.getMatrix(LLRender::MM_MODELVIEW)),
          mSavedDeltaModelview(gGLDeltaModelView),
          mSavedInverseDeltaModelview(gGLInverseDeltaModelView),
          mSavedMatrixMode(gGL.getMatrixMode()),
          mSavedRT(gPipeline.mRT),
          mSavedCameraID(LLViewerCamera::sCurCameraID),
          mSavedOcclusion(LLPipeline::sUseOcclusion),
          mSavedUnderWater(LLPipeline::sUnderWaterRender),
          mSavedPrismRender(LLPipeline::sPrismLensRender),
          mSavedVisibleLightCount(LLPipeline::sVisibleLightCount),
          mSavedVisibleFaces(gPipeline.mNumVisibleFaces),
          mSavedVisibleNodes(gPipeline.mNumVisibleNodes),
          mSavedGLLastMatrix(gGLLastMatrix),
          mSavedBoundTarget(LLRenderTarget::getCurrentBoundTarget()),
          mSavedCurResX(LLRenderTarget::sCurResX),
          mSavedCurResY(LLRenderTarget::sCurResY),
          mSavedShader(LLGLSLShader::sCurBoundShaderPtr),
          mBlendState(GL_BLEND, LLGLState::DISABLED_STATE),
          mDepthState(GL_TRUE, GL_TRUE, GL_LEQUAL)
    {
        std::memcpy(mSavedGlobalViewport, gGLViewport, sizeof(mSavedGlobalViewport));
        glGetIntegerv(GL_VIEWPORT, mSavedGLViewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, mSavedColorMask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, mSavedClearColor);
        glGetIntegerv(GL_BLEND_SRC_RGB, &mSavedBlend[0]);
        glGetIntegerv(GL_BLEND_DST_RGB, &mSavedBlend[1]);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSavedBlend[2]);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &mSavedBlend[3]);
        mSavedScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX, mSavedScissorBox);

        mPipelineStateCaptured = gPipeline.beginPrismAuxiliaryState();
        gPipeline.pushRenderTypeMask();
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true, true);
    }

    ~ScopedPrismRenderState()
    {
        LLRenderTarget* bound_target = LLRenderTarget::getCurrentBoundTarget();
        if (bound_target != mSavedBoundTarget && bound_target)
        {
            bound_target->flush();
        }

        gPipeline.popRenderTypeMask();
        gPipeline.mRT = mSavedRT;
        LLPipeline::sPrismLensRender = mSavedPrismRender;
        LLPipeline::sUseOcclusion = mSavedOcclusion;
        LLPipeline::sUnderWaterRender = mSavedUnderWater;
        LLPipeline::sVisibleLightCount = mSavedVisibleLightCount;
        gPipeline.mNumVisibleFaces = mSavedVisibleFaces;
        gPipeline.mNumVisibleNodes = mSavedVisibleNodes;
        gGLLastMatrix = mSavedGLLastMatrix;
        LLViewerCamera::sCurCameraID = mSavedCameraID;

        LLViewerCamera::instance() = mSavedCamera;
        set_current_projection(mSavedProjection);
        set_current_modelview(mSavedModelview);
        set_last_projection(mSavedLastProjection);
        set_last_modelview(mSavedLastModelview);
        gGLDeltaModelView = mSavedDeltaModelview;
        gGLInverseDeltaModelView = mSavedInverseDeltaModelview;

        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadMatrix(glm::value_ptr(mSavedGLProjection));
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(glm::value_ptr(mSavedGLModelview));
        gGL.matrixMode(mSavedMatrixMode);
        gGL.syncMatrices();

        if (mPipelineStateCaptured)
        {
            // Camera/modelview must be main-eye state before the pipeline
            // restores its hardware-light snapshot and main probe UBO.
            gPipeline.endPrismAuxiliaryState();
        }

        std::memcpy(gGLViewport, mSavedGlobalViewport, sizeof(mSavedGlobalViewport));
        LLRenderTarget::sCurResX = mSavedCurResX;
        LLRenderTarget::sCurResY = mSavedCurResY;
        glViewport(mSavedGLViewport[0], mSavedGLViewport[1],
                   mSavedGLViewport[2], mSavedGLViewport[3]);
        glClearColor(mSavedClearColor[0], mSavedClearColor[1],
                     mSavedClearColor[2], mSavedClearColor[3]);
        glScissor(mSavedScissorBox[0], mSavedScissorBox[1],
                  mSavedScissorBox[2], mSavedScissorBox[3]);
        if (mSavedScissorEnabled)
        {
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }

        gGL.setColorMask(mSavedColorMask[0] != GL_FALSE,
                         mSavedColorMask[1] != GL_FALSE,
                         mSavedColorMask[2] != GL_FALSE,
                         mSavedColorMask[3] != GL_FALSE);

        const LLRender::eBlendFactor src_rgb = blendFactorFromGL(mSavedBlend[0]);
        const LLRender::eBlendFactor dst_rgb = blendFactorFromGL(mSavedBlend[1]);
        const LLRender::eBlendFactor src_alpha = blendFactorFromGL(mSavedBlend[2]);
        const LLRender::eBlendFactor dst_alpha = blendFactorFromGL(mSavedBlend[3]);
        if (src_rgb != LLRender::BF_UNDEF && dst_rgb != LLRender::BF_UNDEF &&
            src_alpha != LLRender::BF_UNDEF && dst_alpha != LLRender::BF_UNDEF)
        {
            gGL.blendFunc(src_rgb, dst_rgb, src_alpha, dst_alpha);
        }
        else
        {
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
        }

        if (mEnvironmentStateCaptured)
        {
            restoreEnvironmentUniforms();
        }

        if (mSavedShader && mSavedShader->isComplete())
        {
            mSavedShader->bind();
            // This shader may be the exact program last used by the auxiliary
            // pass. Reset the explicit SSR/hero guard even though no pool bind
            // occurs on this direct restoration path. Missing uniforms (probe-
            // disabled permutations) are a normal no-op.
            static const LLStaticHashedString sPrismAuxiliary(
                "prism_auxiliary");
            mSavedShader->uniform1i(
                sPrismAuxiliary, mSavedPrismRender ? 1 : 0);
        }
        else
        {
            LLGLSLShader::unbind();
        }

        llassert(LLRenderTarget::getCurrentBoundTarget() == mSavedBoundTarget);
    }

    bool activateClipUniforms()
    {
        PrismEnvironmentScratch& scratch = prismEnvironmentScratch();
        llassert(!mEnvironmentStateCaptured);
        llassert(!scratch.mActive);
        if (mEnvironmentStateCaptured || scratch.mActive)
        {
            LL_WARNS("PrismLens")
                << "Rejected nested Prism environment-uniform transaction"
                << LL_ENDL;
            return false;
        }

        // The source camera and matrices are now installed; stage the
        // default-probe-only UBO in that camera space before any pool binds.
        gPipeline.activatePrismAuxiliaryProbeState();

        scratch.mActive = true;
        mSavedWaterPlane = LLDrawPoolAlpha::sWaterPlane;
        mSavedClassicMode = LLRender::sClassicMode;
        LLEnvironment& environment = LLEnvironment::instance();
        environment.swapShaderUniformState(scratch.mUniforms);
        mEnvironmentStateCaptured = true;

        // Build the source camera/water/clip maps exactly once. The persistent
        // scratch maps retain their vector capacity after they are swapped back.
        environment.updateSettingsUniforms();

        collectEnvironmentShaders(scratch.mShaderDirtyStates);
        for (PrismShaderDirtyState& state : scratch.mShaderDirtyStates)
        {
            state.mSavedDirty = state.mShader->mUniformsDirty;
            state.mShader->mUniformsDirty = true;
        }
        mShaderDirtyStateCaptured = true;
        return true;
    }

    void restoreEnvironmentUniforms()
    {
        PrismEnvironmentScratch& scratch = prismEnvironmentScratch();
        llassert(scratch.mActive);

        // Camera and matrices have already returned to the main view. Restore
        // its exact maps and camera-dependent globals before any shader bind.
        LLEnvironment::instance().swapShaderUniformState(scratch.mUniforms);
        LLDrawPoolAlpha::sWaterPlane = mSavedWaterPlane;
        LLRender::sClassicMode = mSavedClassicMode;

        if (mShaderDirtyStateCaptured)
        {
            for (PrismShaderDirtyState& state : scratch.mShaderDirtyStates)
            {
                // During this synchronous scope, LLEnvironment::update() is not
                // allowed to run: bind() is therefore the only operation that
                // can clear the forced bit. A clear bit proves this shader
                // received source maps and needs one main-view reapply. Shaders
                // not used by the auxiliary pass recover their exact prior bit.
                const bool auxiliary_maps_applied =
                    !state.mShader->mUniformsDirty;
                state.mShader->mUniformsDirty =
                    state.mSavedDirty || auxiliary_maps_applied;
            }
        }

        scratch.mShaderDirtyStates.clear();
        scratch.mActive = false;
        mShaderDirtyStateCaptured = false;
        mEnvironmentStateCaptured = false;
    }

    bool isValid() const
    {
        return mPipelineStateCaptured;
    }

    LLViewerCamera mSavedCamera;
    glm::mat4 mSavedProjection;
    glm::mat4 mSavedModelview;
    glm::mat4 mSavedLastProjection;
    glm::mat4 mSavedLastModelview;
    glm::mat4 mSavedGLProjection;
    glm::mat4 mSavedGLModelview;
    glm::mat4 mSavedDeltaModelview;
    glm::mat4 mSavedInverseDeltaModelview;
    LLRender::eMatrixMode mSavedMatrixMode;
    S32 mSavedGlobalViewport[4];
    GLint mSavedGLViewport[4];
    GLboolean mSavedColorMask[4];
    GLfloat mSavedClearColor[4];
    GLint mSavedBlend[4];
    GLboolean mSavedScissorEnabled = GL_FALSE;
    GLint mSavedScissorBox[4] = { 0, 0, 0, 0 };
    LLPipeline::RenderTargetPack* mSavedRT;
    LLViewerCamera::eCameraID mSavedCameraID;
    S32 mSavedOcclusion;
    bool mSavedUnderWater;
    bool mSavedPrismRender;
    S32 mSavedVisibleLightCount;
    S32 mSavedVisibleFaces;
    S32 mSavedVisibleNodes;
    const LLMatrix4* mSavedGLLastMatrix;
    LLRenderTarget* mSavedBoundTarget;
    U32 mSavedCurResX;
    U32 mSavedCurResY;
    LLGLSLShader* mSavedShader;
    LLVector4 mSavedWaterPlane;
    bool mSavedClassicMode = false;
    bool mEnvironmentStateCaptured = false;
    bool mShaderDirtyStateCaptured = false;
    bool mPipelineStateCaptured = false;
    LLGLState mBlendState;
    LLGLDepthTest mDepthState;
};
} // anonymous namespace

namespace LLPrismLens
{
ActionStatus addCameraSelectionStatus()
{
    return PrismLensRegistry::instance().cameraSelectionStatus();
}

ActionStatus addLensSelectionStatus()
{
    return PrismLensRegistry::instance().lensSelectionStatus();
}

ActionStatus addDisplaySelectionStatus(const CaptureHandle& capture)
{
    return PrismLensRegistry::instance().displaySelectionStatus(capture);
}

ActionStatus setCameraSelectionStatus(const CaptureHandle& capture)
{
    return PrismLensRegistry::instance().cameraSelectionStatus(&capture);
}

ERegistryResult addCameraCaptureFromSelectedObject(CaptureHandle* capture,
                                                    std::string* reason)
{
    return PrismLensRegistry::instance().addCamera(capture, reason);
}

ERegistryResult addVirtualCamera(CaptureHandle* capture, const LLVector3& pos,
                                 const LLQuaternion& rot, std::string* reason)
{
    return PrismLensRegistry::instance().addVirtualCamera(capture, pos, rot, reason);
}

ERegistryResult addSurfaceLensFromSelectedFace(CaptureHandle* capture,
                                                std::string* reason)
{
    return PrismLensRegistry::instance().addLens(capture, reason);
}

ERegistryResult addSelectedDisplay(const CaptureHandle& capture, EFitMode fit,
                                   DisplayHandle* binding, std::string* reason)
{
    return PrismLensRegistry::instance().addDisplay(capture, fit, binding, reason);
}

ERegistryResult addVirtualDisplay(const CaptureHandle& capture,
                                  const LLVector3& pos, const LLQuaternion& rot,
                                  F32 width, F32 height, DisplayHandle* binding,
                                  std::string* reason)
{
    return PrismLensRegistry::instance().addVirtualDisplay(
        capture, pos, rot, width, height, binding, reason);
}

bool setSelectedCamera(const CaptureHandle& capture, std::string* reason)
{
    return PrismLensRegistry::instance().setCamera(capture, reason);
}

bool setCameraSettings(const CaptureHandle& capture,
                       const CameraSettings& settings, std::string* reason)
{
    return PrismLensRegistry::instance().setCameraSettings(capture, settings, reason);
}

bool setCaptureRateSettings(const CaptureHandle& capture,
                            const CaptureRateSettings& settings,
                            std::string* reason)
{
    return PrismLensRegistry::instance().setRateSettings(capture, settings, reason);
}

bool setDisplaySettings(const DisplayHandle& binding,
                        const DisplaySettings& settings, std::string* reason)
{
    return PrismLensRegistry::instance().setDisplaySettings(binding, settings, reason);
}

bool removeDisplay(const DisplayHandle& binding, std::string* reason)
{
    return PrismLensRegistry::instance().removeDisplay(binding, reason);
}

bool removeCapture(const CaptureHandle& capture)
{
    return PrismLensRegistry::instance().removeCapture(capture);
}

U64 configurationRevision()
{
    return PrismLensRegistry::instance().configurationRevision();
}

U64 runtimeRevision()
{
    return PrismLensRegistry::instance().runtimeRevision();
}

RegistrySnapshot registrySnapshot()
{
    return PrismLensRegistry::instance().snapshot();
}

// Render-only camera guides. A Blender-style wireframe "frustum gizmo" drawn
// client-side each frame at every CAMERA_FEED capture whose per-camera guide is
// enabled: an apex camera body, four edge lines out to a framing gate sized by
// the camera's vertical FOV and output aspect, plus four independently
// toggleable aids (rule-of-thirds, up/roll nub, center crosshair, near-clip
// marker). No rezzed prim, no render target, no shader, no allocation. This
// mirrors the client-side overlay discipline of LLActorMover's camera gizmo:
// UI shader, no texture, no depth test, vertices fed straight to gGL in
// world/agent coordinates (render_ui_3d has already set the world modelview).
// The whole thing early-outs cheaply when the global kill-switch is off or no
// camera has its guide on, so the default-off path adds no measurable cost.
void renderCameraGuides()
{
    // Global kill-switch (default ON). When off this is the only work done.
    static LLCachedControl<bool> guide_enable(gSavedSettings, "PrismCameraGuideEnable", true);
    if (!guide_enable)
    {
        return;
    }

    const RegistrySnapshot snapshot = registrySnapshot();

    // Cheap early-out: nothing to draw unless at least one CAMERA_FEED capture
    // has its per-camera guide enabled. No GL state is touched until then.
    bool any_guide = false;
    for (U32 i = 0; i < snapshot.mCaptureCount; ++i)
    {
        const CaptureDefinition& def = snapshot.mCaptures[i];
        if (def.mMode == ECaptureMode::CAMERA_FEED && def.mCamera.mShowGuide)
        {
            any_guide = true;
            break;
        }
    }
    if (!any_guide)
    {
        return;
    }

    const LLColor4 col(0.96f, 0.65f, 0.14f, 1.f); // amber

    // Same beacon-style local overlay as LLActorMover::renderHeadingPreview():
    // UI shader, no texture, no depth writes -- strictly a client-side overlay.
    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    for (U32 i = 0; i < snapshot.mCaptureCount; ++i)
    {
        const CaptureDefinition& def = snapshot.mCaptures[i];
        if (def.mMode != ECaptureMode::CAMERA_FEED || !def.mCamera.mShowGuide)
        {
            continue;
        }

        // Resolve the world transform. A virtual (prim-free) camera draws at its
        // stored transform with no object lookup; an object-anchored camera reads
        // its live render transform. Both then feed the identical frustum draw.
        LLQuaternion rotation;
        LLVector3 eye;
        if (def.mCamera.mVirtual)
        {
            rotation = def.mCamera.mVirtualRot;
            eye = def.mCamera.mVirtualPos; // no mLocalEyeOffset for virtual cams
        }
        else
        {
            LLViewerObject* obj = gObjectList.findObject(def.mCameraObjectId);
            if (!obj || obj->isDead() || obj->isHUDAttachment())
            {
                continue;
            }
            // Mirror the CAMERA_FEED render-path transform derivation (the SL
            // camera marker looks down local -Z, local +Y is up).
            rotation = obj->getRenderRotation();
            eye = obj->getRenderPosition() +
                def.mCamera.mLocalEyeOffset * rotation;
        }
        LLVector3 forward = LLVector3(0.f, 0.f, -1.f) * rotation;
        LLVector3 up = LLVector3::y_axis * rotation;
        LLVector3 right = forward % up;
        if (!eye.isFinite() || !forward.isFinite() || !up.isFinite() ||
            forward.normVec() <= F_ALMOST_ZERO || up.normVec() <= F_ALMOST_ZERO ||
            right.normVec() <= F_ALMOST_ZERO)
        {
            continue;
        }
        up = right % forward; // re-orthonormalize
        up.normVec();

        // FOV: prefer the effective (last-run) vertical FOV, but a freshly-added
        // capture has not run yet (effective == 0), so fall back to the authored
        // fixed FOV; the guide must still draw for a new camera.
        F32 vfov = def.mRuntime.mEffectiveVerticalFovRad;
        if (!std::isfinite(vfov) || vfov < 5.f * DEG_TO_RAD)
        {
            vfov = def.mCamera.mFixedVerticalFovRad;
        }
        const F32 aspect = def.mCamera.mOutputAspect;
        const F32 near_clip = def.mCamera.mNearClip;
        const F32 far_clip = def.mCamera.mFarClip; // user's configured range; NOT
                                                   // clamped to the main camera.
        if (!std::isfinite(vfov) || !std::isfinite(aspect) ||
            !std::isfinite(near_clip) || !std::isfinite(far_clip) ||
            vfov < 5.f * DEG_TO_RAD || vfov > 175.f * DEG_TO_RAD ||
            aspect <= 0.f || near_clip <= 0.f || far_clip <= near_clip)
        {
            continue;
        }

        const F32 tan_half = tanf(vfov * 0.5f);
        // Draw the framing gate at a readable distance, not the raw far clip:
        // the far clip can be hundreds of metres (capture depth), which would
        // make the gizmo an unreadable region-sized box. Framing (aspect /
        // thirds / crosshair) is distance-independent, so clamp the gate to a
        // sensible reference distance; a nearer far clip is honoured as-is.
        const F32 gate_dist = llmin(far_clip, 24.f);
        const F32 hh_far = gate_dist * tan_half;  // gate half-height
        const F32 hw_far = hh_far * aspect;       // gate half-width (real aspect)
        const LLVector3 gate_c = eye + forward * gate_dist;
        const LLVector3 tl = gate_c - right * hw_far + up * hh_far;
        const LLVector3 tr = gate_c + right * hw_far + up * hh_far;
        const LLVector3 bl = gate_c - right * hw_far - up * hh_far;
        const LLVector3 br = gate_c + right * hw_far - up * hh_far;

        gGL.setLineWidth(2.f);
        gGL.begin(LLRender::LINES);
        gGL.color4fv(col.mV);

        // Apex -> far gate corners (four frustum edges).
        gGL.vertex3fv(eye.mV); gGL.vertex3fv(tl.mV);
        gGL.vertex3fv(eye.mV); gGL.vertex3fv(tr.mV);
        gGL.vertex3fv(eye.mV); gGL.vertex3fv(bl.mV);
        gGL.vertex3fv(eye.mV); gGL.vertex3fv(br.mV);
        // Far gate rectangle.
        gGL.vertex3fv(tl.mV); gGL.vertex3fv(tr.mV);
        gGL.vertex3fv(tr.mV); gGL.vertex3fv(br.mV);
        gGL.vertex3fv(br.mV); gGL.vertex3fv(bl.mV);
        gGL.vertex3fv(bl.mV); gGL.vertex3fv(tl.mV);

        // Rule-of-thirds grid: two verticals + two horizontals subdividing the
        // gate into 3x3, from lerped gate corners at 1/3 and 2/3.
        if (def.mCamera.mGuideThirds)
        {
            const LLVector3 t13 = lerp(tl, tr, 1.f / 3.f);
            const LLVector3 t23 = lerp(tl, tr, 2.f / 3.f);
            const LLVector3 b13 = lerp(bl, br, 1.f / 3.f);
            const LLVector3 b23 = lerp(bl, br, 2.f / 3.f);
            gGL.vertex3fv(t13.mV); gGL.vertex3fv(b13.mV);
            gGL.vertex3fv(t23.mV); gGL.vertex3fv(b23.mV);
            const LLVector3 l13 = lerp(tl, bl, 1.f / 3.f);
            const LLVector3 l23 = lerp(tl, bl, 2.f / 3.f);
            const LLVector3 r13 = lerp(tr, br, 1.f / 3.f);
            const LLVector3 r23 = lerp(tr, br, 2.f / 3.f);
            gGL.vertex3fv(l13.mV); gGL.vertex3fv(r13.mV);
            gGL.vertex3fv(l23.mV); gGL.vertex3fv(r23.mV);
        }

        // Center crosshair: two short lines through the gate centre.
        if (def.mCamera.mGuideCrosshair)
        {
            const F32 cx = hw_far * 0.12f;
            const F32 cy = hh_far * 0.12f;
            gGL.vertex3fv((gate_c - right * cx).mV);
            gGL.vertex3fv((gate_c + right * cx).mV);
            gGL.vertex3fv((gate_c - up * cy).mV);
            gGL.vertex3fv((gate_c + up * cy).mV);
        }

        // Near-clip rectangle marker so the depth range reads (the far rect is
        // the gate already).
        if (def.mCamera.mGuideClipMarkers)
        {
            const F32 hh_near = near_clip * tan_half;
            const F32 hw_near = hh_near * aspect;
            const LLVector3 nc = eye + forward * near_clip;
            const LLVector3 ntl = nc - right * hw_near + up * hh_near;
            const LLVector3 ntr = nc + right * hw_near + up * hh_near;
            const LLVector3 nbl = nc - right * hw_near - up * hh_near;
            const LLVector3 nbr = nc + right * hw_near - up * hh_near;
            gGL.vertex3fv(ntl.mV); gGL.vertex3fv(ntr.mV);
            gGL.vertex3fv(ntr.mV); gGL.vertex3fv(nbr.mV);
            gGL.vertex3fv(nbr.mV); gGL.vertex3fv(nbl.mV);
            gGL.vertex3fv(nbl.mV); gGL.vertex3fv(ntl.mV);
        }
        gGL.end();

        // Filled triangles: apex camera-body diamond, plus the optional up/roll
        // nub on the gate's top edge.
        gGL.begin(LLRender::TRIANGLES);
        gGL.color4fv(col.mV);
        const F32 body = llmax(0.06f, hh_far * 0.05f); // camera-body half-size
        const LLVector3 a0 = eye + right * body;
        const LLVector3 a1 = eye + up * body;
        const LLVector3 a2 = eye - right * body;
        const LLVector3 a3 = eye - up * body;
        gGL.vertex3fv(a0.mV); gGL.vertex3fv(a1.mV); gGL.vertex3fv(a2.mV);
        gGL.vertex3fv(a0.mV); gGL.vertex3fv(a2.mV); gGL.vertex3fv(a3.mV);

        if (def.mCamera.mGuideUpRoll)
        {
            const LLVector3 tc = (tl + tr) * 0.5f; // gate top-edge centre
            const F32 nub_w = hw_far * 0.06f;
            const F32 nub_h = hh_far * 0.12f;
            const LLVector3 nub_apex = tc + up * nub_h;
            const LLVector3 nub_l = tc - right * nub_w;
            const LLVector3 nub_r = tc + right * nub_w;
            gGL.vertex3fv(nub_l.mV); gGL.vertex3fv(nub_r.mV); gGL.vertex3fv(nub_apex.mV);
        }
        gGL.end();
    }

    gGL.flush();
    gGL.setLineWidth(1.f);
}

PerformanceSnapshot performanceSnapshot()
{
    return PrismLensRegistry::instance().performanceSnapshot();
}

LLSD sceneData()
{
    return PrismLensRegistry::instance().sceneData();
}

bool applySceneData(const LLSD& data, std::string* reason)
{
    return PrismLensRegistry::instance().applySceneData(data, reason);
}

EDesignationResult selectedFaceStatus(std::string* reason)
{
    return PrismLensRegistry::instance().selectedStatus(reason);
}

bool canDesignateSelectedFace()
{
    return PrismLensRegistry::instance().canDesignate();
}

EDesignationResult designateSelectedFace(std::string* reason)
{
    return PrismLensRegistry::instance().designate(reason);
}

bool removeDesignation(U32 slot)
{
    return PrismLensRegistry::instance().remove(slot);
}

void clearDesignations()
{
    PrismLensRegistry::instance().clearAll();
}

bool hasDesignation()
{
    return PrismLensRegistry::instance().hasDesignation();
}

U32 designationCount()
{
    return PrismLensRegistry::instance().count();
}

U32 designationRevision()
{
    return PrismLensRegistry::instance().revision();
}

bool getDesignation(U32 slot, Designation& designation)
{
    return PrismLensRegistry::instance().getDesignation(slot, designation);
}

void onRenderTargetsReleased()
{
    prismAdaptiveController().forceReset();
    PrismLensRegistry::instance().onRenderTargetsReleased();
}

void renderAuxiliaryView()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    PrismLensRegistry& registry = PrismLensRegistry::instance();
    static bool was_enabled = false;
    const bool enabled = prismEnabled();
    const bool has_captures = registry.hasDesignation();
    const bool render_context_available =
        !LLPipeline::sPrismLensRender && LLPipeline::sRenderDeferred && !gCubeSnapshot;

    // This is the controller's sole per-frame update site. It runs before any
    // prepare/scheduling work so all three captures share one coherent decision.
    prismAdaptiveController().updateFrame(
        enabled, has_captures, render_context_available);
    if (!enabled)
    {
        // Reclaim bounded Prism VRAM exactly once on the transition while
        // preserving local registry identities for a later re-enable.
        if (was_enabled)
        {
            registry.releaseRenderResources();
        }
        was_enabled = false;
        return;
    }
    was_enabled = true;

    // Enabled but empty remains a complete no-op.
    if (!has_captures)
    {
        return;
    }
    if (!render_context_available)
    {
        return;
    }

    S32 main_viewport[4];
    std::memcpy(main_viewport, gGLViewport, sizeof(main_viewport));
    const glm::mat4 main_projection = get_current_projection();
    const glm::mat4 main_modelview = get_current_modelview();
    const LLViewerCamera main_camera = LLViewerCamera::instance();

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism prepare visible lenses");
        for (U32 slot = 0; slot < MAX_LENSES; ++slot)
        {
            registry.prepare(slot, main_viewport, main_projection,
                             main_modelview, main_camera);
            // Prepared surface data is copied by value. Never retain a drawable-owned
            // LLFace beyond this preparation call.
            registry.releaseResolvedFace(slot);
        }
    }

    const S32 render_slot = registry.chooseRenderSlot();
    if (render_slot < 0)
    {
        return;
    }

    const U32 slot = static_cast<U32>(render_slot);
    const PrismFrame* frame_ptr = registry.frame(slot);
    if (!frame_ptr || frame_ptr->mTargetWidth == 0 || frame_ptr->mTargetHeight == 0)
    {
        return;
    }
    const PrismFrame& frame = *frame_ptr;
    const U32 output_width = frame.mTargetWidth;
    const U32 output_height = frame.mTargetHeight;
    const U32 render_width = output_width;
    const U32 render_height = output_height;
    bool produced = false;
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Prism one scheduled auxiliary update");
        ScopedActivePrismSlot active_slot(registry, slot);
        ScopedPrismRenderState scoped_state;
        if (!scoped_state.isValid())
        {
            registry.deferRetry(slot, 1);
            return;
        }
        // Acquire an EXACT-SIZE scratch pack (physical == render) from the
        // size-keyed pool; the deferred fullscreen shaders sample the G-buffer
        // with normalized [0,1] UVs, so no sub-rect/logical-extent is allowed.
        if (!gPipeline.acquirePrismLensScratch(render_width, render_height))
        {
            registry.deferRetry(slot, 30);
            return;
        }
        LLPipeline::sPrismLensRender = true;
        LLPipeline::sUseOcclusion = 0;
        LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_PRISM_LENS;
        LLPipeline::RenderTargetPack* scratch_rt = gPipeline.getActivePrismLensScratchPack();
        if (!scratch_rt)
        {
            registry.deferRetry(slot, 5);
            return;
        }
        gPipeline.mRT = scratch_rt;

        const PrismInstance* capture = registry.capture(slot);
        if (!capture)
        {
            return;
        }
        LLViewerCamera lens_camera = main_camera;
        glm::mat4 prism_projection;
        glm::mat4 prism_modelview;
        if (capture->mMode == ECaptureMode::CAMERA_FEED)
        {
            LLQuaternion rotation;
            LLVector3 camera_eye;
            if (capture->mCamera.mVirtual)
            {
                // Prim-free: the eye/orientation come from the stored transform,
                // not from an in-world object. mVirtualPos is in AGENT space --
                // the SAME space source_object->getRenderPosition() returns (it
                // resolves to getPositionAgent()), so it drops straight into the
                // agent-space lens camera below with no conversion. No object
                // lookup, no early-out, and no mLocalEyeOffset add (offset is an
                // object-rig convenience only).
                rotation = capture->mCamera.mVirtualRot;
                camera_eye = capture->mCamera.mVirtualPos;
            }
            else
            {
                LLViewerObject* source_object = gObjectList.findObject(capture->mCameraObjectId);
                LLVOVolume* source = source_object
                    ? dynamic_cast<LLVOVolume*>(source_object) : nullptr;
                if (!source_object || !source || source_object->isDead() ||
                    source_object->isHUDAttachment() || source->isRiggedMesh() ||
                    source->isAnimatedObject())
                {
                    registry.deferRetry(slot, 5);
                    return;
                }
                rotation = source_object->getRenderRotation();
                camera_eye = source_object->getRenderPosition() +
                    capture->mCamera.mLocalEyeOffset * rotation;
            }
            LLVector3 forward = LLVector3(0.f, 0.f, -1.f) * rotation;
            LLVector3 up = LLVector3::y_axis * rotation;
            LLVector3 right = forward % up;
            if (!camera_eye.isFinite() || !forward.isFinite() || !up.isFinite() ||
                forward.normVec() <= F_ALMOST_ZERO || up.normVec() <= F_ALMOST_ZERO ||
                right.normVec() <= F_ALMOST_ZERO)
            {
                registry.deferRetry(slot, 5);
                return;
            }
            up = right % forward;
            up.normVec();
            const LLVector3 left_axis = -right;
            const F32 fov = capture->mRuntime.mEffectiveVerticalFovRad;
            const F32 aspect = capture->mCamera.mOutputAspect;
            const F32 near_clip = capture->mCamera.mNearClip;
            const F32 far_clip = llmin(capture->mCamera.mFarClip, main_camera.getFar());
            if (!std::isfinite(fov) || !std::isfinite(aspect) ||
                !std::isfinite(near_clip) || !std::isfinite(far_clip) ||
                fov < 5.f * DEG_TO_RAD || fov > 175.f * DEG_TO_RAD ||
                aspect < 0.25f || aspect > 4.f || near_clip <= 0.f ||
                far_clip < near_clip + 0.1f)
            {
                registry.deferRetry(slot, 5);
                return;
            }
            lens_camera.setOrigin(camera_eye);
            lens_camera.setNear(near_clip);
            lens_camera.setFar(far_clip);
            lens_camera.setAxes(forward, left_axis, up);
            lens_camera.setAspect(aspect);
            lens_camera.setViewHeightInPixels(static_cast<S32>(render_height));
            // Value-only camera mutation: never broadcasts viewer camera state.
            lens_camera.setViewNoBroadcast(fov);
            const glm::vec3 eye(camera_eye.mV[VX], camera_eye.mV[VY], camera_eye.mV[VZ]);
            const glm::vec3 at(forward.mV[VX], forward.mV[VY], forward.mV[VZ]);
            const glm::vec3 camera_up(up.mV[VX], up.mV[VY], up.mV[VZ]);
            prism_projection = glm::perspective(fov, aspect, near_clip, far_clip);
            prism_modelview = glm::lookAt(eye, eye + at, camera_up);
        }
        else
        {
            const glm::vec3 eye(main_camera.getOrigin().mV[VX],
                                main_camera.getOrigin().mV[VY],
                                main_camera.getOrigin().mV[VZ]);
            const glm::vec3 surface_origin(frame.mWorldSurfaceOrigin.mV[VX],
                                           frame.mWorldSurfaceOrigin.mV[VY],
                                           frame.mWorldSurfaceOrigin.mV[VZ]);
            glm::vec3 surface_u(frame.mWorldSurfaceUEdge.mV[VX],
                                frame.mWorldSurfaceUEdge.mV[VY],
                                frame.mWorldSurfaceUEdge.mV[VZ]);
            const glm::vec3 surface_v(frame.mWorldSurfaceVEdge.mV[VX],
                                      frame.mWorldSurfaceVEdge.mV[VY],
                                      frame.mWorldSurfaceVEdge.mV[VZ]);
            if (frame.mCompositeUvScale[0] < 0.f) surface_u = -surface_u;
            const glm::vec3 surface_center = surface_origin +
                0.5f * (glm::vec3(frame.mWorldSurfaceUEdge.mV[VX],
                                  frame.mWorldSurfaceUEdge.mV[VY],
                                  frame.mWorldSurfaceUEdge.mV[VZ]) + surface_v);
            const glm::vec3 sub_u = surface_u / frame.mZoom;
            const glm::vec3 sub_v = surface_v / frame.mZoom;
            const glm::vec3 pa = surface_center - 0.5f * (sub_u + sub_v);
            const glm::vec3 pb = pa + sub_u;
            const glm::vec3 pc = pa + sub_v;
            const glm::vec3 vr = glm::normalize(pb - pa);
            const glm::vec3 vu = glm::normalize(pc - pa);
            const glm::vec3 vn = glm::normalize(glm::cross(vr, vu));
            const glm::vec3 va = pa - eye;
            const glm::vec3 vb = pb - eye;
            const glm::vec3 vc = pc - eye;
            const F32 eye_distance = -glm::dot(vn, va);
            const F32 near_clip = main_camera.getNear();
            const F32 far_clip = main_camera.getFar();
            if (!std::isfinite(eye_distance) || eye_distance <= CLIP_EPSILON ||
                near_clip <= 0.f || far_clip <= near_clip)
            {
                registry.deferRetry(slot, 5);
                return;
            }
            const F32 left = glm::dot(vr, va) * near_clip / eye_distance;
            const F32 right = glm::dot(vr, vb) * near_clip / eye_distance;
            const F32 bottom = glm::dot(vu, va) * near_clip / eye_distance;
            const F32 top = glm::dot(vu, vc) * near_clip / eye_distance;
            if (!std::isfinite(left) || !std::isfinite(right) ||
                !std::isfinite(bottom) || !std::isfinite(top) ||
                right <= left || top <= bottom)
            {
                registry.deferRetry(slot, 5);
                return;
            }
            F32 h_half = atanf(llmax(fabsf(left), fabsf(right)) / near_clip);
            F32 v_half = atanf(llmax(fabsf(bottom), fabsf(top)) / near_clip);
            if (!std::isfinite(h_half) || !std::isfinite(v_half))
            {
                h_half = MAX_CULL_HORIZONTAL_HALF_ANGLE;
                v_half = MAX_CULL_VERTICAL_HALF_ANGLE;
            }
            else
            {
                h_half = llclamp(h_half, MIN_CULL_HALF_ANGLE,
                                 MAX_CULL_HORIZONTAL_HALF_ANGLE);
                v_half = llclamp(v_half, MIN_CULL_VERTICAL_HALF_ANGLE,
                                 MAX_CULL_VERTICAL_HALF_ANGLE);
                v_half = llmax(v_half, atanf(tanf(h_half) / MAX_ASPECT_RATIO));
            }
            F32 cull_aspect = tanf(h_half) / tanf(v_half);
            if (!std::isfinite(cull_aspect) || cull_aspect <= 0.f)
            {
                cull_aspect = MAX_ASPECT_RATIO;
            }
            lens_camera.setAxes(LLVector3(-vn.x, -vn.y, -vn.z),
                                LLVector3(-vr.x, -vr.y, -vr.z),
                                LLVector3(vu.x, vu.y, vu.z));
            lens_camera.setAspect(cull_aspect);
            lens_camera.setViewHeightInPixels(static_cast<S32>(render_height));
            lens_camera.setViewNoBroadcast(2.f * v_half);
            LLVector3 keep_normal;
            frame.mFragmentClipPlane.getVector3(keep_normal);
            const LLVector3 keep_point = keep_normal * -frame.mFragmentClipPlane[3];
            LLPlane lens_cull_plane(keep_point, -keep_normal);
            lens_camera.setUserClipPlane(lens_cull_plane);
            prism_projection = glm::frustum(left, right, bottom, top,
                                            near_clip, far_clip);
            prism_modelview = glm::lookAt(eye, eye - vn, vu);
        }

        // Resolve one source-eye region water height for every auxiliary
        // consumer. This also sets sUnderWaterRender from eye Z versus that
        // exact height, keeping cull, pool ordering, water, and fog coherent.
        gPipeline.setPrismAuxiliaryWaterHeight(lens_camera.getOrigin());

        // Delay any growth that replaces a retained output until all projection
        // validation has succeeded. An early degenerate-view return must leave
        // the previous cached beauty intact.
        if (!gPipeline.allocatePrismLensOutput(slot, output_width, output_height))
        {
            // Avoid starving the other retained lenses if VRAM allocation keeps
            // failing for this slot.
            registry.invalidateOutput(slot, 30);
            return;
        }

        LLViewerCamera::instance() = lens_camera;
        set_current_projection(prism_projection);
        set_current_modelview(prism_modelview);
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadMatrix(glm::value_ptr(prism_projection));
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(glm::value_ptr(prism_modelview));

        gGLViewport[0] = 0;
        gGLViewport[1] = 0;
        gGLViewport[2] = static_cast<S32>(render_width);
        gGLViewport[3] = static_cast<S32>(render_height);
        glViewport(0, 0, static_cast<GLsizei>(render_width),
                   static_cast<GLsizei>(render_height));
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, static_cast<GLsizei>(render_width),
                  static_cast<GLsizei>(render_height));
        LLViewerCamera::updateFrustumPlanes(LLViewerCamera::instance(), false, false, true);

        // Build one source-eye uniform cache after the Prism flag, camera,
        // matrices, water height, and clip plane are installed. The scope swaps
        // the exact main cache back without rebuilding it.
        if (!scoped_state.activateClipUniforms())
        {
            registry.deferRetry(slot, 1);
            return;
        }

        // Mark actual auxiliary work at the last responsible moment: camera,
        // target, and projection validation alone must not contaminate the
        // conservative no-aux reference used only to veto recovery.
        prismAdaptiveController().noteAuxiliaryWork();

        // [Prism spot shadows] Projector/spot shadows for this capture. Gated
        // exactly like the main view's spot shadows: nothing here runs when
        // spot shadows are globally off. This block must stay BEFORE the aux
        // updateCull/stateSort below: shadow generation runs its own stateSort
        // per map, so the aux scene stateSort has to come after it to rebuild
        // the aux draw maps. All shared shadow state mutated in this block was
        // saved by beginPrismAuxiliaryState (via ScopedPrismRenderState) and is
        // restored by endPrismAuxiliaryState before the frame's main stateSort,
        // keeping the main view byte-identical.
        if (LLPipeline::sRenderDeferred && LLPipeline::RenderShadowDetail > 1)
        {
            // --- [Prism spot shadows Stage 1] re-express resident MAIN-view
            // maps in aux view space. A spot shadow map's content is rendered
            // from the projector's own frustum and is therefore camera-
            // independent; the only camera-dependent term in the sampling
            // matrix is the eye's inverse view. Rebuild mSunShadowMatrix[i+4]
            // from the persisted per-slot light view/projection with the AUX
            // camera's inverse view so a projector holding a main-view slot
            // shadows correctly in the feed at zero extra render cost.
            {
                const glm::mat4 inv_view_aux =
                    glm::inverse(get_current_modelview());
                // same [-1,1] -> [0,1] bias matrix generateSunShadow builds
                const glm::mat4 shadow_bias(0.5f, 0.0f, 0.0f, 0.0f,
                                            0.0f, 0.5f, 0.0f, 0.0f,
                                            0.0f, 0.0f, 0.5f, 0.0f,
                                            0.5f, 0.5f, 0.5f, 1.0f);
                // runtime slot count (same clamp as pipeline.cpp's
                // bdmergeMaxSpotShadows, which is file-local there)
                static LLCachedControl<U32> max_spot_shadows(
                    gSavedSettings, "BDMergeMaxSpotShadows", 2);
                const U32 num_spots = llclamp(
                    (U32)max_spot_shadows, 2u, LLPipeline::MAX_SPOT_SHADOWS);
                for (U32 i = 0; i < num_spots; ++i)
                {
                    if (gPipeline.mShadowSpotLight[i].notNull() &&
                        gPipeline.mSpotShadow[i].getWidth() > 0)
                    {
                        gPipeline.mSunShadowMatrix[i + 4] =
                            shadow_bias * gPipeline.mShadowProjection[i + 4] *
                            gPipeline.mShadowModelview[i + 4] * inv_view_aux;
                    }
                }
            }

            // --- [Prism spot shadows Stage 2] generate dedicated aux shadow
            // maps (gPipeline.mPrismSpotShadow[]) for statelessly selected feed
            // projectors, so projectors WITHOUT a main-view slot cast shadows
            // too. This subsumes Stage 1's slots: it reassigns every slot it
            // fills (fresh map + aux-correct matrices) and nulls the rest so
            // absent projectors resolve unshadowed. The call restores the aux
            // matrices, viewport, and camera id before returning.
            gPipeline.generatePrismSpotShadows(LLViewerCamera::instance());
        }

        static LLCullResult prism_cull;
        prism_cull.clear();
        gPipeline.updateCull(LLViewerCamera::instance(), prism_cull);
        gPipeline.stateSort(LLViewerCamera::instance(), prism_cull);

        // Rebuild the source-eye local-light set for this aux frame. The aux
        // path otherwise never (re)populates mNearbyLights before the deferred
        // local-light loop in renderDeferredLighting() consumes it:
        // beginPrismAuxiliaryState() cleared it, and calcNearbyLights' only
        // other caller (renderGeomPostDeferred) runs at the TAIL of
        // renderDeferredLighting(), i.e. AFTER the loop. Without this rebuild,
        // no point/spot/projector light renders in the feed at all (the
        // sPrismLensRender branch of calcNearbyLights was effectively dead for
        // lighting). sPrismLensRender is true here, so this takes that branch
        // (source-eye set; no NEARBY_LIGHT-bit or fade-clock mutation);
        // endPrismAuxiliaryState() restores the main-view set, so the main view
        // stays byte-identical.
        gPipeline.calcNearbyLights(LLViewerCamera::instance());

        // scratch_rt was validated non-null right after the acquire above and
        // the pool cannot be released while this scoped render is running.
        LLPipeline::RenderTargetPack& rt = *scratch_rt;
        gPipeline.bindPrismLensTarget(rt.deferredScreen);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        rt.deferredScreen.clear();
        gPipeline.renderGeomDeferred(LLViewerCamera::instance(), false);
        rt.deferredScreen.flush();
        gPipeline.renderDeferredLighting();

        // [Prism camera feed - projector volumetric rays] Inject the projector
        // volumetric shafts into THIS aux capture. renderProjectorVolumetric only
        // runs inside the main renderFinalize(), which the aux never calls, so the
        // feed otherwise shows lit projector cones with no visible airborne shaft.
        // All prerequisites are already in place here: the aux view matrices
        // (get_current_modelview/projection), the aux G-buffer depth/normals in
        // mRT->deferredScreen (mRT == this scratch pack), the aux projector list
        // (mShadowSpotLight[]) + shadow maps (generatePrismSpotShadows, via
        // getSpotShadowTarget's sPrismLensRender redirect), and the source-eye
        // nearby-light set (calcNearbyLights). aux_direct=true forces the direct
        // fullscreen march straight onto rt.screen and snapshots+restores the
        // shared main-view volumetric state, so this aux pass - which precedes the
        // main view - cannot disturb the main view's temporal accumulation. No
        // feedProjectorVolumetricBloom() here: the aux builds no HDR bloom pyramid.
        gPipeline.renderProjectorVolumetric(&rt.screen, /*aux_direct=*/true);

        // renderDeferredLighting() flushes the scratch HDR screen. Copy the
        // finished beauty while all copy-related GL mutations remain covered by
        // the complete state scope; publish only after that scope closes.
        gPipeline.mPrismLensOutput[slot].copyContents(
            rt.screen,
            0, 0, render_width, render_height,
            0, 0, output_width, output_height,
            GL_COLOR_BUFFER_BIT,
            render_width == output_width && render_height == output_height
                ? GL_NEAREST : GL_LINEAR);
        produced = true;
    }

    // Publish only after the scoped camera/matrix/viewport/target/uniform
    // restoration has completed successfully.
    if (produced)
    {
        registry.markProduced(slot, output_width, output_height);
    }
}

U32 getCompositeStates(LLRenderTarget* screen_target, CompositeState* states,
                       U32 capacity)
{
    if (!states || capacity == 0 || !prismEnabled() || LLPipeline::sPrismLensRender ||
        screen_target != &gPipeline.mMainRT.screen ||
        LLRenderTarget::getCurrentBoundTarget() != screen_target)
    {
        return 0;
    }

    PrismLensRegistry& registry = PrismLensRegistry::instance();
    U32 state_count = 0;
    // Capture-first traversal groups sibling faces so the pipeline can retain
    // one texture binding while changing only per-display basis/mapping uniforms.
    for (U32 capture_slot = 0;
         capture_slot < MAX_CAPTURES && state_count < capacity; ++capture_slot)
    {
        const PrismInstance* capture = registry.capture(capture_slot);
        if (!capture || !capture->mHasOutput) continue;
        LLRenderTarget& output = gPipeline.mPrismLensOutput[capture_slot];
        U32 output_width = 0;
        U32 output_height = 0;
        F32 output_uv_scale[2];
        F32 output_uv_offset[2];
        if (!output.isComplete() ||
            !registry.getOutputRegion(capture_slot, output_width, output_height) ||
            !registry.getOutputOrientation(capture_slot, output_uv_scale, output_uv_offset) ||
            output_width > output.getWidth() || output_height > output.getHeight())
        {
            continue;
        }

        for (U32 display_slot = 0;
             display_slot < MAX_DISPLAY_BINDINGS && state_count < capacity;
             ++display_slot)
        {
            const PrismDisplay* display = registry.display(display_slot);
            if (!display || display->mCaptureSlot != capture_slot ||
                display->mCaptureGeneration != capture->mHandle.mGeneration)
            {
                continue;
            }
            const PrismFrame& frame = display->mFrame;
            if (!frame.mPrepared || frame.mFrame != gFrameCount ||
                frame.mMainViewport[2] <= 0 || frame.mMainViewport[3] <= 0)
            {
                continue;
            }
            // A prim-free virtual screen has no face to resolve; the pipeline
            // draws its stored world quad instead. A real display resolves and
            // requires a live face exactly as before.
            const bool is_virtual = display->mSettings.mVirtual;
            LLFace* face = nullptr;
            if (!is_virtual)
            {
                face = registry.resolveDisplayFaceForComposite(display_slot);
                if (!face) continue;
            }

            CompositeState& state = states[state_count];
            state = CompositeState();
            state.mCaptureSlot = capture_slot;
            state.mFace = face;
            state.mVirtual = is_virtual;
            if (is_virtual)
            {
                // World/agent-space quad corners TL, TR, BR, BL from the frame's
                // synthesized rectangle. Drawn under the base world modelview
                // (see pipeline.cpp), matching the surface uniforms below.
                const LLVector3 tl = frame.mWorldSurfaceOrigin;
                const LLVector3 tr = frame.mWorldSurfaceOrigin + frame.mWorldSurfaceUEdge;
                const LLVector3 br = tr + frame.mWorldSurfaceVEdge;
                const LLVector3 bl = frame.mWorldSurfaceOrigin + frame.mWorldSurfaceVEdge;
                std::memcpy(state.mVirtualCorners[0], tl.mV, sizeof(state.mVirtualCorners[0]));
                std::memcpy(state.mVirtualCorners[1], tr.mV, sizeof(state.mVirtualCorners[1]));
                std::memcpy(state.mVirtualCorners[2], br.mV, sizeof(state.mVirtualCorners[2]));
                std::memcpy(state.mVirtualCorners[3], bl.mV, sizeof(state.mVirtualCorners[3]));
            }
            const F32 inv_width = 1.f / static_cast<F32>(frame.mMainViewport[2]);
            const F32 inv_height = 1.f / static_cast<F32>(frame.mMainViewport[3]);
            std::memcpy(state.mSurfaceOrigin, frame.mSurfaceOrigin.mV,
                        sizeof(state.mSurfaceOrigin));
            std::memcpy(state.mSurfaceUDual, frame.mSurfaceUDual.mV,
                        sizeof(state.mSurfaceUDual));
            std::memcpy(state.mSurfaceVDual, frame.mSurfaceVDual.mV,
                        sizeof(state.mSurfaceVDual));

            state.mOpticsParams[0] = capture->mCamera.mOptics.mChromaticAberration;
            state.mOpticsParams[1] = capture->mCamera.mOptics.mFilmGrain;
            state.mOpticsParams[2] = capture->mCamera.mOptics.mCRTScanlines;
            state.mOpticsParams[3] = capture->mCamera.mOptics.mExposureBias;

            // Per-display TV screen effects, packed as four vec4 uniforms for
            // the composite shader. A default ScreenEffects packs all zeros
            // and every shader block gates on > 0.001, so an untouched
            // display composites bit-identical to a build without effects.
            const ScreenEffects& effects = display->mSettings.mEffects;
            state.mScreenEffect0[0] = effects.mScanlines;
            state.mScreenEffect0[1] = effects.mScanlineCount;
            state.mScreenEffect0[2] = effects.mPixelate;
            state.mScreenEffect0[3] = effects.mGrayscale;
            state.mScreenEffect1[0] = effects.mSepia;
            state.mScreenEffect1[1] = effects.mStatic;
            state.mScreenEffect1[2] = effects.mVerticalRoll;
            state.mScreenEffect1[3] = effects.mRollSpeed;
            state.mScreenEffect2[0] = effects.mTracking;
            state.mScreenEffect2[1] = effects.mFlicker;
            state.mScreenEffect2[2] = effects.mChromaBleed;
            state.mScreenEffect2[3] = effects.mVignette;
            state.mScreenEffect3[0] = effects.mInterlace;
            state.mScreenEffect3[1] = effects.mDropout;
            state.mScreenEffect3[2] = effects.mBrightness;
            state.mScreenEffect3[3] = 0.f; // Reserved.
            // Feature A (orientation) + Feature B (sheen). Flip/rotate are
            // booleans encoded as 0/1; sheen is the 0..1 reflectivity. All zero
            // (the default) is a strict shader no-op, so an untouched display
            // stays bit-identical.
            state.mScreenEffect4[0] = effects.mFlipH ? 1.f : 0.f;
            state.mScreenEffect4[1] = effects.mFlipV ? 1.f : 0.f;
            state.mScreenEffect4[2] = effects.mRotate90 ? 1.f : 0.f;
            state.mScreenEffect4[3] = effects.mSheen;

            std::memcpy(state.mRetainedOrientationScale, output_uv_scale,
                        sizeof(state.mRetainedOrientationScale));
            std::memcpy(state.mRetainedOrientationOffset, output_uv_offset,
                        sizeof(state.mRetainedOrientationOffset));
            state.mTextureRegionScale[0] = static_cast<F32>(output_width - 1u) /
                                           static_cast<F32>(output.getWidth());
            state.mTextureRegionScale[1] = static_cast<F32>(output_height - 1u) /
                                           static_cast<F32>(output.getHeight());
            state.mTextureRegionOffset[0] = 0.5f / static_cast<F32>(output.getWidth());
            state.mTextureRegionOffset[1] = 0.5f / static_cast<F32>(output.getHeight());

            const F32 display_u = frame.mWorldSurfaceUEdge.magVec();
            const F32 display_v = frame.mWorldSurfaceVEdge.magVec();
            const F32 display_aspect = display_v > F_ALMOST_ZERO
                ? display_u / display_v : 1.f;
            const F32 capture_aspect = capture->mMode == ECaptureMode::CAMERA_FEED
                ? capture->mCamera.mOutputAspect
                : static_cast<F32>(output_width) / static_cast<F32>(output_height);
            const F32 anchor_x = display->mSettings.mAnchor[0];
            const F32 anchor_y = display->mSettings.mAnchor[1];
            if (capture->mMode == ECaptureMode::CAMERA_FEED &&
                std::isfinite(display_aspect) && std::isfinite(capture_aspect) &&
                display_aspect > F_ALMOST_ZERO && capture_aspect > F_ALMOST_ZERO)
            {
                if (display->mSettings.mFitMode == EFitMode::FIT)
                {
                    state.mLetterbox = 1;
                    if (display_aspect > capture_aspect)
                    {
                        const F32 fraction = capture_aspect / display_aspect;
                        state.mDisplayToCaptureScale[0] = 1.f / fraction;
                        state.mDisplayToCaptureOffset[0] =
                            -anchor_x * (1.f - fraction) / fraction;
                    }
                    else
                    {
                        const F32 fraction = display_aspect / capture_aspect;
                        state.mDisplayToCaptureScale[1] = 1.f / fraction;
                        state.mDisplayToCaptureOffset[1] =
                            -anchor_y * (1.f - fraction) / fraction;
                    }
                }
                else if (display->mSettings.mFitMode == EFitMode::FILL)
                {
                    if (display_aspect > capture_aspect)
                    {
                        const F32 crop = capture_aspect / display_aspect;
                        state.mDisplayToCaptureScale[1] = crop;
                        state.mDisplayToCaptureOffset[1] = anchor_y * (1.f - crop);
                    }
                    else
                    {
                        const F32 crop = display_aspect / capture_aspect;
                        state.mDisplayToCaptureScale[0] = crop;
                        state.mDisplayToCaptureOffset[0] = anchor_x * (1.f - crop);
                    }
                }
            }
            std::memcpy(state.mBarColorLinear, display->mSettings.mBarColorLinear,
                        sizeof(state.mBarColorLinear));

            if (is_virtual)
            {
                // A virtual screen bounds its own pixels with the world quad
                // geometry (plus the depth test), so it uses the full-target
                // scissor as an outer clip -- the same approach the aux path uses
                // for real faces. The main-view lens-rect projection is only used
                // above for output sizing, not as the scissor here.
                state.mScissor[0] = 0;
                state.mScissor[1] = 0;
                state.mScissor[2] = static_cast<S32>(screen_target->getWidth());
                state.mScissor[3] = static_cast<S32>(screen_target->getHeight());
            }
            else
            {
                const F32 scale_x = static_cast<F32>(screen_target->getWidth()) * inv_width;
                const F32 scale_y = static_cast<F32>(screen_target->getHeight()) * inv_height;
                const S32 scissor_left = llclamp(ll_round(
                    static_cast<F32>(frame.mLensRect.mX - frame.mMainViewport[0]) * scale_x),
                    0, static_cast<S32>(screen_target->getWidth()) - 1);
                const S32 scissor_bottom = llclamp(ll_round(
                    static_cast<F32>(frame.mLensRect.mY - frame.mMainViewport[1]) * scale_y),
                    0, static_cast<S32>(screen_target->getHeight()) - 1);
                const S32 scissor_right = llclamp(ll_round(
                    static_cast<F32>(frame.mLensRect.mX - frame.mMainViewport[0] +
                                     static_cast<S32>(frame.mLensRect.mWidth)) * scale_x),
                    scissor_left + 1, static_cast<S32>(screen_target->getWidth()));
                const S32 scissor_top = llclamp(ll_round(
                    static_cast<F32>(frame.mLensRect.mY - frame.mMainViewport[1] +
                                     static_cast<S32>(frame.mLensRect.mHeight)) * scale_y),
                    scissor_bottom + 1, static_cast<S32>(screen_target->getHeight()));
                state.mScissor[0] = scissor_left;
                state.mScissor[1] = scissor_bottom;
                state.mScissor[2] = scissor_right - scissor_left;
                state.mScissor[3] = scissor_top - scissor_bottom;
            }
            state.mEdgeFeather = frame.mEdgeFeather;
            // Keep all resolved LLFace pointers in their display frames until
            // next preparation; pipeline consumes the whole returned batch now.
            ++state_count;
        }
    }
    return state_count;
}

// [Prism camera feed - recursive mirror] Auxiliary-capture twin of
// getCompositeStates. It runs ONLY inside the Prism aux (VCam) render
// (sPrismLensRender == true), letting the display faces composite the PREVIOUS
// frame's retained feed (mPrismLensOutput[slot]) while the aux is drawing the
// current frame - so a camera aimed at its own monitor produces a stable,
// 1-frame-lagged infinite tunnel.
//
// Kept as a SEPARATE function (not a flag on getCompositeStates) so the main-view
// composite path stays byte-identical: the pipeline calls this one instead only
// when sPrismLensRender is set. Differences from getCompositeStates:
//   * Gate requires sPrismLensRender TRUE and the bound target to be the ACTIVE
//     AUX pack screen (gPipeline.mRT->screen, which during the aux is the scratch
//     pack, never mMainRT.screen). It can therefore never touch the main view.
//   * Scissor is the FULL aux target extent, not the main-view lens rect
//     (frame.mLensRect / mMainViewport are main-view screen coords, meaningless in
//     the aux camera's viewport). The real face geometry (renderIndexed) plus the
//     aux depth test bound the touched pixels; the scissor is only an outer clip.
//   * The retained texture the shader samples (mPrismLensOutput[capture_slot]) is
//     the PREVIOUS publication - copyContents into it happens AFTER this aux render
//     - so this is never a read-after-write against the in-progress rt.screen, and
//     the recursion is a fixed, bounded, 1-frame-lagged source (no runaway/flare).
// Everything else (surface basis, orientation, texture region, optics, effects,
// aspect fit) is identical to getCompositeStates, so the feed's monitor matches
// the main view's monitor exactly, one frame delayed.
//
// Known constraint (surfaced for review): the display frames are prepared against
// the MAIN camera frustum (renderAuxiliaryView prepares them before the aux
// render; re-preparing them here against the aux frustum would overwrite the
// main-view frame data the later main composite consumes, breaking byte-identical
// main). A monitor visible in the aux but NOT in the main view is therefore not
// prepared this frame and does not recurse until it is also main-visible.
U32 getAuxCompositeStates(LLRenderTarget* screen_target, CompositeState* states,
                          U32 capacity)
{
    if (!states || capacity == 0 || !prismEnabled() ||
        !LLPipeline::sPrismLensRender ||
        screen_target != &gPipeline.mRT->screen ||
        LLRenderTarget::getCurrentBoundTarget() != screen_target)
    {
        return 0;
    }

    PrismLensRegistry& registry = PrismLensRegistry::instance();
    U32 state_count = 0;
    for (U32 capture_slot = 0;
         capture_slot < MAX_CAPTURES && state_count < capacity; ++capture_slot)
    {
        const PrismInstance* capture = registry.capture(capture_slot);
        if (!capture || !capture->mHasOutput) continue;
        LLRenderTarget& output = gPipeline.mPrismLensOutput[capture_slot];
        U32 output_width = 0;
        U32 output_height = 0;
        F32 output_uv_scale[2];
        F32 output_uv_offset[2];
        if (!output.isComplete() ||
            !registry.getOutputRegion(capture_slot, output_width, output_height) ||
            !registry.getOutputOrientation(capture_slot, output_uv_scale, output_uv_offset) ||
            output_width > output.getWidth() || output_height > output.getHeight())
        {
            continue;
        }

        for (U32 display_slot = 0;
             display_slot < MAX_DISPLAY_BINDINGS && state_count < capacity;
             ++display_slot)
        {
            const PrismDisplay* display = registry.display(display_slot);
            if (!display || display->mCaptureSlot != capture_slot ||
                display->mCaptureGeneration != capture->mHandle.mGeneration)
            {
                continue;
            }
            const PrismFrame& frame = display->mFrame;
            if (!frame.mPrepared || frame.mFrame != gFrameCount ||
                frame.mMainViewport[2] <= 0 || frame.mMainViewport[3] <= 0)
            {
                continue;
            }
            // Prim-free virtual screen: no face to resolve; draw the stored world
            // quad. Mirror of the main builder so a virtual screen also works as
            // a recursive-mirror surface. A real display resolves as before.
            const bool is_virtual = display->mSettings.mVirtual;
            LLFace* face = nullptr;
            if (!is_virtual)
            {
                face = registry.resolveDisplayFaceForComposite(display_slot);
                if (!face) continue;
            }

            CompositeState& state = states[state_count];
            state = CompositeState();
            state.mCaptureSlot = capture_slot;
            state.mFace = face;
            state.mVirtual = is_virtual;
            if (is_virtual)
            {
                const LLVector3 tl = frame.mWorldSurfaceOrigin;
                const LLVector3 tr = frame.mWorldSurfaceOrigin + frame.mWorldSurfaceUEdge;
                const LLVector3 br = tr + frame.mWorldSurfaceVEdge;
                const LLVector3 bl = frame.mWorldSurfaceOrigin + frame.mWorldSurfaceVEdge;
                std::memcpy(state.mVirtualCorners[0], tl.mV, sizeof(state.mVirtualCorners[0]));
                std::memcpy(state.mVirtualCorners[1], tr.mV, sizeof(state.mVirtualCorners[1]));
                std::memcpy(state.mVirtualCorners[2], br.mV, sizeof(state.mVirtualCorners[2]));
                std::memcpy(state.mVirtualCorners[3], bl.mV, sizeof(state.mVirtualCorners[3]));
            }
            std::memcpy(state.mSurfaceOrigin, frame.mSurfaceOrigin.mV,
                        sizeof(state.mSurfaceOrigin));
            std::memcpy(state.mSurfaceUDual, frame.mSurfaceUDual.mV,
                        sizeof(state.mSurfaceUDual));
            std::memcpy(state.mSurfaceVDual, frame.mSurfaceVDual.mV,
                        sizeof(state.mSurfaceVDual));

            state.mOpticsParams[0] = capture->mCamera.mOptics.mChromaticAberration;
            state.mOpticsParams[1] = capture->mCamera.mOptics.mFilmGrain;
            state.mOpticsParams[2] = capture->mCamera.mOptics.mCRTScanlines;
            state.mOpticsParams[3] = capture->mCamera.mOptics.mExposureBias;

            const ScreenEffects& effects = display->mSettings.mEffects;
            state.mScreenEffect0[0] = effects.mScanlines;
            state.mScreenEffect0[1] = effects.mScanlineCount;
            state.mScreenEffect0[2] = effects.mPixelate;
            state.mScreenEffect0[3] = effects.mGrayscale;
            state.mScreenEffect1[0] = effects.mSepia;
            state.mScreenEffect1[1] = effects.mStatic;
            state.mScreenEffect1[2] = effects.mVerticalRoll;
            state.mScreenEffect1[3] = effects.mRollSpeed;
            state.mScreenEffect2[0] = effects.mTracking;
            state.mScreenEffect2[1] = effects.mFlicker;
            state.mScreenEffect2[2] = effects.mChromaBleed;
            state.mScreenEffect2[3] = effects.mVignette;
            state.mScreenEffect3[0] = effects.mInterlace;
            state.mScreenEffect3[1] = effects.mDropout;
            state.mScreenEffect3[2] = effects.mBrightness;
            state.mScreenEffect3[3] = 0.f; // Reserved.
            // Feature A (orientation) + Feature B (sheen); twin of the main
            // builder above so the recursive feed matches the main composite.
            state.mScreenEffect4[0] = effects.mFlipH ? 1.f : 0.f;
            state.mScreenEffect4[1] = effects.mFlipV ? 1.f : 0.f;
            state.mScreenEffect4[2] = effects.mRotate90 ? 1.f : 0.f;
            state.mScreenEffect4[3] = effects.mSheen;

            std::memcpy(state.mRetainedOrientationScale, output_uv_scale,
                        sizeof(state.mRetainedOrientationScale));
            std::memcpy(state.mRetainedOrientationOffset, output_uv_offset,
                        sizeof(state.mRetainedOrientationOffset));
            state.mTextureRegionScale[0] = static_cast<F32>(output_width - 1u) /
                                           static_cast<F32>(output.getWidth());
            state.mTextureRegionScale[1] = static_cast<F32>(output_height - 1u) /
                                           static_cast<F32>(output.getHeight());
            state.mTextureRegionOffset[0] = 0.5f / static_cast<F32>(output.getWidth());
            state.mTextureRegionOffset[1] = 0.5f / static_cast<F32>(output.getHeight());

            const F32 display_u = frame.mWorldSurfaceUEdge.magVec();
            const F32 display_v = frame.mWorldSurfaceVEdge.magVec();
            const F32 display_aspect = display_v > F_ALMOST_ZERO
                ? display_u / display_v : 1.f;
            const F32 capture_aspect = capture->mMode == ECaptureMode::CAMERA_FEED
                ? capture->mCamera.mOutputAspect
                : static_cast<F32>(output_width) / static_cast<F32>(output_height);
            const F32 anchor_x = display->mSettings.mAnchor[0];
            const F32 anchor_y = display->mSettings.mAnchor[1];
            if (capture->mMode == ECaptureMode::CAMERA_FEED &&
                std::isfinite(display_aspect) && std::isfinite(capture_aspect) &&
                display_aspect > F_ALMOST_ZERO && capture_aspect > F_ALMOST_ZERO)
            {
                if (display->mSettings.mFitMode == EFitMode::FIT)
                {
                    state.mLetterbox = 1;
                    if (display_aspect > capture_aspect)
                    {
                        const F32 fraction = capture_aspect / display_aspect;
                        state.mDisplayToCaptureScale[0] = 1.f / fraction;
                        state.mDisplayToCaptureOffset[0] =
                            -anchor_x * (1.f - fraction) / fraction;
                    }
                    else
                    {
                        const F32 fraction = display_aspect / capture_aspect;
                        state.mDisplayToCaptureScale[1] = 1.f / fraction;
                        state.mDisplayToCaptureOffset[1] =
                            -anchor_y * (1.f - fraction) / fraction;
                    }
                }
                else if (display->mSettings.mFitMode == EFitMode::FILL)
                {
                    if (display_aspect > capture_aspect)
                    {
                        const F32 crop = capture_aspect / display_aspect;
                        state.mDisplayToCaptureScale[1] = crop;
                        state.mDisplayToCaptureOffset[1] = anchor_y * (1.f - crop);
                    }
                    else
                    {
                        const F32 crop = display_aspect / capture_aspect;
                        state.mDisplayToCaptureScale[0] = crop;
                        state.mDisplayToCaptureOffset[0] = anchor_x * (1.f - crop);
                    }
                }
            }
            std::memcpy(state.mBarColorLinear, display->mSettings.mBarColorLinear,
                        sizeof(state.mBarColorLinear));

            // Full aux-target scissor (see header note): the main-view lens-rect
            // scissor math is invalid in the aux camera's viewport.
            state.mScissor[0] = 0;
            state.mScissor[1] = 0;
            state.mScissor[2] = static_cast<S32>(screen_target->getWidth());
            state.mScissor[3] = static_cast<S32>(screen_target->getHeight());
            state.mEdgeFeather = frame.mEdgeFeather;
            ++state_count;
        }
    }
    return state_count;
}

bool getActiveClipPlane(LLPlane& plane)
{
    PrismLensRegistry& registry = PrismLensRegistry::instance();
    const PrismFrame* frame = registry.activeFrame();
    if (!LLPipeline::sPrismLensRender || !frame || !frame->mPrepared ||
        frame->mFrame != gFrameCount || !registry.activeCaptureIsSurfaceLens())
    {
        return false;
    }
    plane = frame->mFragmentClipPlane;
    return true;
}

void compositeDebug()
{
    PrismLensRegistry& registry = PrismLensRegistry::instance();
    const S32 debug_slot = registry.lastRenderedSlot();
    const PrismFrame* frame = debug_slot >= 0
        ? registry.frame(static_cast<U32>(debug_slot)) : nullptr;
    U32 source_width = 0;
    U32 source_height = 0;
    if (!prismEnabled() || !prismDebugEnabled() || LLPipeline::sPrismLensRender ||
        !frame || frame->mFrame != gFrameCount ||
        !registry.getOutputRegion(static_cast<U32>(debug_slot), source_width, source_height) ||
        !gPipeline.mPrismLensOutput[debug_slot].isComplete() ||
        frame->mDebugRect.mWidth == 0 || frame->mDebugRect.mHeight == 0)
    {
        return;
    }

    LLRenderTarget& source = gPipeline.mPrismLensOutput[debug_slot];
    LLRenderTarget& destination = gPipeline.mMainRT.screen;
    const F32 scale_x = static_cast<F32>(destination.getWidth()) /
                        static_cast<F32>(frame->mMainViewport[2]);
    const F32 scale_y = static_cast<F32>(destination.getHeight()) /
                        static_cast<F32>(frame->mMainViewport[3]);
    const S32 destination_x = ll_round(
        static_cast<F32>(frame->mDebugRect.mX - frame->mMainViewport[0]) * scale_x);
    const S32 destination_y = ll_round(
        static_cast<F32>(frame->mDebugRect.mY - frame->mMainViewport[1]) * scale_y);
    const S32 destination_width = ll_round(
        static_cast<F32>(frame->mDebugRect.mWidth) * scale_x);
    const S32 destination_height = ll_round(
        static_cast<F32>(frame->mDebugRect.mHeight) * scale_y);
    destination.copyContents(
        source,
        0, 0, source_width, source_height,
        destination_x, destination_y,
        destination_x + destination_width,
        destination_y + destination_height,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
}
} // namespace LLPrismLens
