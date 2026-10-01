/**
 * @file llreflectionmapmanager.cpp
 * @brief LLReflectionMapManager class implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llreflectionmapmanager.h"

#include <vector>

#include "alcinelightrigmanager.h" // [LiveProbeRefresh]
#include "alenvintensity.h"       // [ProbeOnDemand]
#include "alprobedirty.h"         // [ProbeOnDemand]
#include "lldrawable.h"           // [ProbeOnDemand]
#include "lllightconstants.h"     // [ProbeOnDemand]
#include "lltimer.h"              // [LiveProbeRefresh]
#include "llvoavatar.h"           // [ProbeOnDemand]
#include "llvoavatarself.h"       // [ProbeOnDemand]
#include "llvovolume.h"           // [ProbeOnDemand]
#include "llviewercamera.h"
#include "llspatialpartition.h"
#include "llviewerregion.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llviewercontrol.h"
#include "llenvironment.h"
#include "llstartup.h"
#include "llviewermenufile.h"
#include "llnotificationsutil.h"

#if LL_WINDOWS
#pragma warning (push)
#pragma warning (disable : 4702) // compiler complains unreachable code
#endif
#define TINYEXR_USE_MINIZ 0
#include <zlib.h>
#include <tinyexr.h>
#if LL_WINDOWS
#pragma warning (pop)
#endif

LLPointer<LLImageGL> gEXRImage;

void load_exr(const std::string& filename)
{
    // reset reflection maps when previewing a new HDRI
    gPipeline.mReflectionMapManager.reset();
    gPipeline.mReflectionMapManager.initReflectionMaps();

    float* out; // width * height * RGBA
    int width;
    int height;
    const char* err = NULL; // or nullptr in C++11

    int ret =  LoadEXRWithLayer(&out, &width, &height, filename.c_str(), /* layername */ nullptr, &err);
    if (ret == TINYEXR_SUCCESS)
    {
        U32 texName = 0;
        LLImageGL::generateTextures(1, &texName);

        gEXRImage = new LLImageGL(texName, 4, GL_TEXTURE_2D, GL_RGB16F, GL_RGB16F, GL_FLOAT, LLTexUnit::TAM_CLAMP);
        gEXRImage->setHasMipMaps(true);
        gEXRImage->setUseMipMaps(true);
        gEXRImage->setFilteringOption(LLTexUnit::TFO_TRILINEAR);

        gGL.getTexUnit(0)->bind(gEXRImage);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGBA, GL_FLOAT, out);

        LLImageGLMemory::alloc_tex_image(width, height, GL_RGB16F, 1, /*has_mips=*/true);

        free(out); // release memory of image data

        glGenerateMipmap(GL_TEXTURE_2D);

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    }
    else
    {
        LLSD notif_args;
        notif_args["WHAT"] = filename;
        notif_args["REASON"] = "Unknown";
        if (err)
        {
            notif_args["REASON"] = std::string(err);
            FreeEXRErrorMessage(err); // release memory of error message.
        }
        LLNotificationsUtil::add("CannotLoad", notif_args);
    }
}

void hdri_preview()
{
    LLFilePickerReplyThread::startPicker(
        [](const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter load_filter, LLFilePicker::ESaveFilter save_filter)
        {
            if (LLAppViewer::instance()->quitRequested())
            {
                return;
            }
            if (filenames.size() > 0)
            {
                load_exr(filenames[0]);
            }
        },
        LLFilePicker::FFLOAD_HDRI,
        true);
}

extern bool gCubeSnapshot;
extern bool gTeleportDisplay;

static U32 sUpdateCount = 0;

// [ProbeOnDemand] Who is capturing right now (tiny-group cull context); NONE
// outside LLReflectionMapManager::updateProbeFace.
LLReflectionMapManager::ProbeCaptureKind LLReflectionMapManager::sCaptureKind =
    LLReflectionMapManager::ProbeCaptureKind::NONE;

// [ProbeOnDemand] Capture-context scope around each probe->update(): sets the
// capture kind and, when probe_centric, saves + clears the nearby-light list so the
// capture builds its own probe-centric list (restored on every exit).
LLReflectionMapManager::ProbeCaptureScope::ProbeCaptureScope(
    LLReflectionMapManager& manager, bool active, bool probe_centric, ProbeCaptureKind kind)
    : mManager(manager)
    , mActive(active)
    , mCentric(active && probe_centric)
    , mSavedLights(false)
{
    if (!mActive)
    {
        return; // on-demand OFF and tiny cull OFF: no capture context needed
    }
    sCaptureKind = kind;
    if (mCentric)
    {
        mManager.mProbeCentricLights = true;
        mSavedLights = gPipeline.beginCinematicProbeCapture();
    }
}

LLReflectionMapManager::ProbeCaptureScope::~ProbeCaptureScope()
{
    if (mCentric)
    {
        if (mSavedLights)
        {
            gPipeline.endCinematicProbeCapture();
        }
        mManager.mProbeCentricLights = false;
    }
    if (mActive)
    {
        sCaptureKind = ProbeCaptureKind::NONE;
    }
}

// get the next highest power of two of v (or v if v is already a power of two)
//defined in llvertexbuffer.cpp
extern U32 nhpo2(U32 v);

static void touch_default_probe(LLReflectionMap* probe)
{
    if (LLViewerCamera::getInstance())
    {
        LLVector3 origin = LLViewerCamera::getInstance()->getOrigin();
        origin.mV[2] += 64.f;

        probe->mOrigin.load3(origin.mV);
    }
}

LLReflectionMapManager::LLReflectionMapManager()
{
    mDynamicProbeCount = LL_MAX_REFLECTION_PROBE_COUNT;
    initCubeFree();
}

void LLReflectionMapManager::initCubeFree()
{
    // start at 1 because index 0 is reserved for mDefaultProbe
    for (U32 i = 1; i < mDynamicProbeCount; ++i)
    {
        mCubeFree.push_back(i);
    }
}

struct CompareProbeDistance
{
    LLReflectionMap* mDefaultProbe;

    bool operator()(const LLPointer<LLReflectionMap>& lhs, const LLPointer<LLReflectionMap>& rhs)
    {
        return lhs->mDistance < rhs->mDistance;
    }
};

static F32 update_score(LLReflectionMap* p)
{
    return gFrameTimeSeconds - p->mLastUpdateTime  - p->mDistance*0.1f;
}

// return true if a is higher priority for an update than b
static bool check_priority(LLReflectionMap* a, LLReflectionMap* b)
{
    if (a->mCubeIndex == -1)
    { // not a candidate for updating
        return false;
    }
    else if (b->mCubeIndex == -1)
    { // b is not a candidate for updating, a is higher priority by default
        return true;
    }
    else if (!a->mComplete && !b->mComplete)
    { //neither probe is complete, use distance
        return a->mDistance < b->mDistance;
    }
    else if (a->mComplete && b->mComplete)
    { //both probes are complete, use update_score metric
        return update_score(a) > update_score(b);
    }

    // a or b is not complete,
    if (sUpdateCount % 3 == 0)
    { // every third update, allow complete probes to cut in line in front of non-complete probes to avoid spammy probe generators from deadlocking scheduler (SL-20258))
        return !b->mComplete;
    }

    // prioritize incomplete probe
    return b->mComplete;
}

// helper class to seed octree with probes
void LLReflectionMapManager::update()
{
    // [ProbeOnDemand] Latch the setting for this frame and (un)arm the dirty
    // recorder. With the setting OFF nothing below changes any behavior.
    static LLCachedControl<bool> on_demand(gSavedSettings, "RenderProbeOnDemand", false);
    mOnDemandActive = on_demand && LLPipeline::sReflectionProbesEnabled;
    ALProbeDirty::setRecording(mOnDemandActive);
    ALProbeDirty::setProbeResolution(mProbeResolution);
    // OFF costs one cached-control read: the window / frame bookkeeping only runs
    // while on-demand is active or the schedule log is enabled.
    static LLCachedControl<bool> sched_log_setting(gSavedSettings, "RenderProbeSchedLog", false);
    mSchedStats = mOnDemandActive || sched_log_setting;
    if (mSchedStats)
    {
        mSchedFrame = ALProbeSched::SchedFrame();
    }

    if (!LLPipeline::sReflectionProbesEnabled || gTeleportDisplay || LLStartUp::getStartupState() < STATE_PRECACHE)
    {
        resetCinematicRefresh(); // [LiveProbeRefresh]
        noteScheduleEarlyReturn(); // [ProbeOnDemand]
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("reflection manager update");
    llassert(!gCubeSnapshot); // assert a snapshot is not in progress
    if (LLAppViewer::instance()->logoutRequestSent())
    {
        resetCinematicRefresh(); // [LiveProbeRefresh]
        noteScheduleEarlyReturn(); // [ProbeOnDemand]
        return;
    }

    if (mPaused && gFrameTimeSeconds > mResumeTime)
    {
        resume();
    }

    mResetFade = llmin((F32)(mResetFade + gFrameIntervalSeconds * 2.f), 1.f);

    {
        U32 probe_count_temp = mDynamicProbeCount;
        if (mRenderReflectionProbeDynamicAllocation > -1)
        {
            if (mRenderReflectionProbeLevel == 0)
            {
                mDynamicProbeCount = 1;
            }
            else if (mRenderReflectionProbeLevel == 1)
            {
                mDynamicProbeCount = (U32)mProbes.size();
            }
            else if (mRenderReflectionProbeLevel == 2)
            {
                mDynamicProbeCount = llmax((U32)mProbes.size(), 128);
            }
            else
            {
                mDynamicProbeCount = 256;
            }

            if (mRenderReflectionProbeDynamicAllocation > 1)
            {
                // Round mDynamicProbeCount to the nearest increment of 16
                mDynamicProbeCount = ((mDynamicProbeCount + mRenderReflectionProbeDynamicAllocation / 2) / mRenderReflectionProbeDynamicAllocation) * 16;
                mDynamicProbeCount = llclamp(mDynamicProbeCount, 1, mRenderReflectionProbeCount);
            }
            else
            {
                mDynamicProbeCount = llclamp(mDynamicProbeCount + mRenderReflectionProbeDynamicAllocation, 1, mRenderReflectionProbeCount);
            }
        }
        else
        {
            mDynamicProbeCount = mRenderReflectionProbeCount;
        }

        // The default probe owns slot zero. While the cinematic Live Probe is
        // designated, reserve one additional effective slot even when manual
        // dynamic-allocation rounding would otherwise collapse a two-probe
        // scene back to a single cube.
        if (mCinematicLiveProbe.notNull() &&
            mRenderReflectionProbeLevel > 0 &&
            mRenderReflectionProbeCount >= 2)
        {
            mDynamicProbeCount = llmax(mDynamicProbeCount, 2u);
        }

        mDynamicProbeCount = llmin(mDynamicProbeCount, LL_MAX_REFLECTION_PROBE_COUNT);

        if (mDynamicProbeCount != probe_count_temp)
            mResetFade = 1.f;
    }

    initReflectionMaps();

    static LLCachedControl<bool> render_hdr(gSavedSettings, "RenderHDREnabled", true);

    if (!mRenderTarget.isComplete())
    {
        U32 color_fmt = render_hdr ? GL_R11F_G11F_B10F : GL_RGB8;
        U32 targetRes = mProbeResolution * 4; // super sample
        mRenderTarget.allocate(targetRes, targetRes, color_fmt, true);
    }

    if (mMipChain.empty())
    {
        U32 res = mProbeResolution;
        U32 count = (U32)(log2((F32)res) + 0.5f);

        mMipChain.resize(count);
        for (U32 i = 0; i < count; ++i)
        {
            mMipChain[i].allocate(res, res, render_hdr ? GL_R11F_G11F_B10F : GL_RGB8);
            res /= 2;
        }
    }

    llassert(mProbes[0] == mDefaultProbe);

    LLVector4a camera_pos;
    camera_pos.load3(LLViewerCamera::instance().getOrigin().mV);

    // process kill list
    for (auto& probe : mKillList)
    {
        auto const & iter = std::find(mProbes.begin(), mProbes.end(), probe);
        if (iter != mProbes.end())
        {
            deleteProbe((U32)(iter - mProbes.begin()));
        }
    }

    mKillList.clear();

    // process create list
    for (auto& probe : mCreateList)
    {
        mProbes.push_back(probe);
    }

    mCreateList.clear();

    if (mProbes.empty())
    {
        resetCinematicRefresh(); // [LiveProbeRefresh]
        noteScheduleEarlyReturn(); // [ProbeOnDemand]
        return;
    }

    // [ProbeOnDemand] Collect the frame's dirty events, hit the probes they reach,
    // run the environment / light diff and the refresh-all barrier. OFF: a no-op.
    flushProbeSchedule();

    bool did_update = false;

    bool realtime = mRenderReflectionProbeDetail >= (S32)LLReflectionMapManager::DetailLevel::REALTIME;

    LLReflectionMap* closestDynamic = nullptr;
    LLReflectionMap* cinematicLive = nullptr;

    LLReflectionMap* oldestProbe = nullptr;
    LLReflectionMap* oldestOccluded = nullptr;

    // [ProbeManualRate] Ordinary-probe face throttle. N = 1 (default) is today's
    // code path exactly: every frame is a tick. On a skipped frame NOTHING ordinary
    // runs: no continuation face, no new probe start (so no txn start, no ack, no
    // op bump, no barrier event). did_update still marks "a probe pass is in
    // flight" so the selection loop below can never start a second probe over it.
    // The realtime slot, the Live probe, the hero probe and the allocation / cull
    // bookkeeping above and below are not gated.
    static LLCachedControl<S32> ord_every_n(gSavedSettings, "RenderProbeUpdateEveryNFrames", 1);
    const S32 ord_n = llclamp(static_cast<S32>(ord_every_n), 1, 120);
    const bool ord_tick = ord_n <= 1 ||
        (mOrdThrottleCounter++ % static_cast<U32>(ord_n)) == 0;

    if (mUpdatingProbe != nullptr)
    {
        did_update = true;
        if (ord_tick)
        {
            doProbeUpdate();
        }
    }

    // update distance to camera for all probes
    std::sort(mProbes.begin()+1, mProbes.end(), CompareProbeDistance());
    llassert(mProbes[0] == mDefaultProbe);
    llassert(mProbes[0]->mCubeArray == mTexture);
    llassert(mProbes[0]->mCubeIndex == 0);

    // Keep the explicitly selected cinematic probe inside the finite cubemap
    // allocation even in probe-dense scenes. Index zero remains the default.
    if (mCinematicLiveProbe.notNull())
    {
        auto iter = std::find(
            mProbes.begin() + 1, mProbes.end(), mCinematicLiveProbe);
        if (iter != mProbes.end() && iter != mProbes.begin() + 1)
        {
            std::rotate(mProbes.begin() + 1, iter, iter + 1);
        }
    }

    // make sure we're assigning cube slots to the closest probes

    // first free any cube indices for distant probes
    for (U32 i = mReflectionProbeCount; i < mProbes.size(); ++i)
    {
        LLReflectionMap* probe = mProbes[i];
        llassert(probe != nullptr);

        if (probe && probe->mCubeIndex != -1 && mUpdatingProbe != probe)
        { // free this index
            mCubeFree.push_back(probe->mCubeIndex);

            probe->mCubeArray = nullptr;
            probe->mCubeIndex = -1;
            probe->mComplete = false;
            probe->mFadeIn = 0;
        }
    }

    // next distribute the free indices
    U32 count = llmin(mReflectionProbeCount, (U32)mProbes.size());

    for (U32 i = 1; i < count && !mCubeFree.empty(); ++i)
    {
        // find the closest probe that needs a cube index
        LLReflectionMap* probe = mProbes[i];

        if (probe->mCubeIndex == -1)
        {
            S32 idx = allocateCubeIndex();
            llassert(idx > 0); //if we're still in this loop, mCubeFree should not be empty and allocateCubeIndex should be returning good indices
            probe->mCubeArray = mTexture;
            probe->mCubeIndex = idx;
        }
    }

    mResetFade = llmin((F32)(mResetFade + gFrameIntervalSeconds * 2.f), 1.f);

    for (unsigned int i = 0; i < mProbes.size(); ++i)
    {
        LLReflectionMap* probe = mProbes[i];
        if (probe->getNumRefs() == 1)
        { // no references held outside manager, delete this probe
            deleteProbe(i);
            --i;
            continue;
        }

        if (probe != mDefaultProbe &&
            (!probe->isRelevant() || mPaused))
        { // skip irrelevant probes (or all non-default probes if paused)
            continue;
        }

        LLVector4a d;

        if (probe != mDefaultProbe)
        {
            if (probe->mViewerObject) //make sure probes track the viewer objects they are attached to
            {
                probe->mOrigin.load3(probe->mViewerObject->getPositionAgent().mV);
            }
            d.setSub(camera_pos, probe->mOrigin);
            probe->mDistance = d.getLength3().getF32() - probe->mRadius;
        }
        else if (probe->mComplete)
        {
            // make default probe have a distance of 64m for the purposes of prioritization (if it's already been generated once)
            probe->mDistance = 64.f;
        }
        else
        {
            probe->mDistance = -4096.f; //boost priority of default probe when it's not complete
        }

        if (probe->mComplete)
        {
            probe->autoAdjustOrigin();
            probe->mFadeIn = llmin((F32) (probe->mFadeIn + gFrameIntervalSeconds), 1.f);
        }
        // [ProbeOnDemand] Barrier members are captured regardless of occlusion.
        const bool probe_barrier_member = mOnDemandActive &&
            ALProbeSched::barrierIsMember(mBarrier, probe->mSched.mId);
        if (probe->mOccluded && probe->mComplete && !probe_barrier_member)
        {
            if (oldestOccluded == nullptr)
            {
                oldestOccluded = probe;
            }
            else if (probe->mLastUpdateTime < oldestOccluded->mLastUpdateTime)
            {
                oldestOccluded = probe;
            }
        }
        else
        {
            if (probe != mCinematicLiveProbe && !did_update &&
                i < mReflectionProbeCount &&
                // [ProbeOnDemand] only ORDINARY records with something to do are queued
                (!mOnDemandActive ||
                    ALProbeSched::selectable(probe->mSched,
                        ALProbeSched::View{ probe->mComplete, probe->mOccluded,
                                            probe->getIsDynamic(), probe->mCubeIndex != -1,
                                            probe_barrier_member },
                        mSchedPolicy, static_cast<F64>(gFrameTimeSeconds))) &&
                (oldestProbe == nullptr ||
                    check_priority(probe, oldestProbe)))
            {
               oldestProbe = probe;
            }
        }

        if (probe == mCinematicLiveProbe && probe->mCubeIndex != -1)
        {
            cinematicLive = probe;
        }
        else if (realtime &&
            closestDynamic == nullptr &&
            probe->mCubeIndex != -1 &&
            probe->getIsDynamic())
        {
            closestDynamic = probe;
        }

        if (mRenderReflectionProbeLevel == 0)
        {
            // only update default probe when coverage is set to none
            llassert(probe == mDefaultProbe);
            break;
        }
    }

    LLReflectionMap* realtime_probe = cinematicLive
        ? cinematicLive : (realtime ? closestDynamic : nullptr);
    // [LiveProbeRefresh] Refresh-mode dispatch. Every frame mode and the
    // closest-dynamic fallback run the shipped capture unchanged (its body now
    // lives in updateRealtimeProbeAllFaces); the other modes go through the
    // face-budget scheduler, sampled here so extra display() calls (snapshots,
    // 360) can never advance it.
    static LLCachedControl<S32> cine_refresh_mode(gSavedSettings, "CineLightRigLiveProbeRefresh", 3);
    static LLCachedControl<S32> cine_change_faces(gSavedSettings, "CineLightRigLiveProbeChangeFaces", 2);
    static LLCachedControl<F32> cine_watchdog_sec(gSavedSettings, "CineLightRigLiveProbeWatchdogSec", 5.f);
    static LLCachedControl<F32> cine_settle_sec(gSavedSettings, "CineLightRigLiveProbeSettleSec", 0.5f);
    static LLCachedControl<S32> cine_manual_faces(gSavedSettings, "CineLightRigLiveProbeManualFaces", 2); // [ProbeManualRate]
    static LLCachedControl<S32> cine_manual_frames(gSavedSettings, "CineLightRigLiveProbeManualFrames", 4); // [ProbeManualRate]
    const ALCineLiveProbeRefresh::Mode cine_mode =
        ALCineLiveProbeRefresh::sanitizeMode(cine_refresh_mode);
    if (realtime_probe != nullptr && realtime_probe == cinematicLive &&
        cine_mode != ALCineLiveProbeRefresh::Mode::EVERY_FRAME)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - cine step");
        if (mOnDemandActive)
        {
            // [ProbeOnDemand] Live's FULL / budget passes write the SAME secondary
            // scratch the sliced path uses: a partial sliced pass must never resume
            // on top of them (it would publish a mixed cube).
            ALProbeSched::sliceInvalidate(mRtSlice);
            mRtSliceRanThisFrame = 0;
        }
        realtime_probe->autoAdjustOrigin();
        ALCineLiveProbeRefresh::Params cine_params;
        cine_params.mMode = cine_mode;
        cine_params.mChangeFaces = llclamp((S32)cine_change_faces, 1, 6);
        cine_params.mWatchdogSec = llclamp((F32)cine_watchdog_sec, 0.f, 60.f);
        cine_params.mSettleSec = llclamp((F32)cine_settle_sec, 0.f, 5.f);
        cine_params.mManualFaces = llclamp((S32)cine_manual_faces, 1, 6); // [ProbeManualRate]
        cine_params.mManualEvery = llclamp((S32)cine_manual_frames, 1, 120); // [ProbeManualRate]
        // [ProbeManualRate] on_change is false in Manual, so Manual samples no hash
        // (H = 0, on the ON path too: sampleCinematicHOnDemand is never reached)
        // and never takes the animated-look FULL fallback.
        const bool on_change = cine_mode == ALCineLiveProbeRefresh::Mode::ON_CHANGE;
        // [ProbeOnDemand] ON: the H takes its lights from the debounced light diff
        // (published values) and the animated-light FULL path is off, so flicker /
        // moving facelights / rig transitions settle once instead of cycling Live.
        const U64 cine_h = on_change
            ? (mOnDemandActive ? sampleCinematicHOnDemand(realtime_probe)
                               : sampleCinematicH(realtime_probe))
            : 0;
        // sampleCinematicH above refreshed the pipeline's animated-gobo flag.
        const bool cine_animating = on_change && !mOnDemandActive &&
            (ALCineLightRigManager::instance().liveProbeAnimating() ||
             gPipeline.isCinematicProbeLightAnimating());
        const bool cine_ready = mCinematicIrradianceReady &&
            mCinematicRadianceReady && realtime_probe->mComplete;
        // decide() resets its own state on a probe / cube / mode change; count
        // those here so the [LiveProbe] resets counter covers every reset.
        if (mCineRefresh.mProbe != nullptr &&
            (mCineRefresh.mProbe != realtime_probe ||
             mCineRefresh.mCubeIndex != realtime_probe->mCubeIndex ||
             mCineRefresh.mMode != cine_mode))
        {
            ++mCineStats.mResets;
        }
        const ALCineLiveProbeRefresh::Decision decision =
            ALCineLiveProbeRefresh::decide(mCineRefresh, cine_params, cine_h,
                cine_animating, cine_ready, (F64)gFrameTimeSeconds,
                realtime_probe, realtime_probe->mCubeIndex);
        ++mCineStats.mSteps;
        mCineLastPath = decision.mPath;
        if (decision.mPath == ALCineLiveProbeRefresh::Path::FULL)
        {
            // Budget passes never advance mRealtimeRadiancePass, so resync the
            // FULL cursor with the scheduler: the next pass is radiance iff the
            // last completed pass was irradiance (strict alternation across
            // budget <-> animation handoffs; an abandoned partial pass does not
            // count as completed).
            mRealtimeRadiancePass = mCineRefresh.mLastPassIrr;
            const bool full_radiance = mRealtimeRadiancePass; // the pass the moved body will run
            // [ProbeOnDemand] a FULL frame is a single-frame pass: its start op
            const U64 full_start_op = mOnDemandActive ? ALProbeDirty::nextOp() : 0;
            updateRealtimeProbeAllFaces(realtime_probe, cinematicLive);
            ALCineLiveProbeRefresh::onFullPass(mCineRefresh, full_radiance,
                cine_h, (F64)gFrameTimeSeconds);
            ++(full_radiance ? mCineStats.mRadPub : mCineStats.mIrrPub);
            ++mCineStats.mCleanPasses; // a FULL frame is one complete, clean pass
            ++mCineStats.mFullFrames;
            mCineStats.mFaces += 6;
            if (mSchedStats)
            {
                mSchedFrame.mRtFaces += 6; // [ProbeOnDemand]
            }
            if (mOnDemandActive)
            {
                mLiveLastFaceOp = ALProbeDirty::nextOp();
                noteLivePassForBarrier(full_radiance, full_start_op);
            }
        }
        else if (decision.mPath == ALCineLiveProbeRefresh::Path::BUDGET)
        {
            ALCineLiveProbeRefresh::noteFrameH(mCineRefresh, cine_h);
            if (realtime_probe == mUpdatingProbe)
            {
                // would share the primary scratch: skip this frame, keep the cursor
                ++mCineStats.mBlocked;
            }
            else
            {
                // [ProbeOnDemand] face / pass bookkeeping for the barrier and the
                // exposure ordering (call-site counters only)
                const U32 probe_faces_before = mCineStats.mFaces;
                const U32 probe_irr_before = mCineStats.mIrrPub;
                const U32 probe_rad_before = mCineStats.mRadPub;
                if (mOnDemandActive && mCineRefresh.mActive && mCineRefresh.mFace == 0)
                {
                    mBarrier.mLivePassStartOp = ALProbeDirty::nextOp();
                }
                updateCinematicBudget(realtime_probe, decision.mFaces);
                ++mCineStats.mBudgetFrames;
                if (mSchedStats)
                {
                    mSchedFrame.mRtFaces += mCineStats.mFaces - probe_faces_before;
                }
                if (mOnDemandActive)
                {
                    if (mCineStats.mFaces != probe_faces_before)
                    {
                        mLiveLastFaceOp = ALProbeDirty::nextOp();
                    }
                    if (mCineStats.mIrrPub != probe_irr_before)
                    {
                        noteLivePassForBarrier(false, mBarrier.mLivePassStartOp);
                    }
                    if (mCineStats.mRadPub != probe_rad_before)
                    {
                        noteLivePassForBarrier(true, mBarrier.mLivePassStartOp);
                    }
                    if (mCineContStartOp != 0)
                    {
                        // [ProbeManualRate] a Manual burst began a new pass after
                        // the one it just ended
                        mBarrier.mLivePassStartOp = mCineContStartOp;
                        mCineContStartOp = 0;
                    }
                }
            }
        }
        else
        {
            ++mCineStats.mIdleFrames;
        }
        logCinematicRefresh((F64)gFrameTimeSeconds);
    }
    else
    {
        // Every frame / fallback / paused / no probe: the scheduler is not driving.
        if (mCineRefresh.mProbe != nullptr)
        {
            resetCinematicRefresh();
        }
        if (realtime_probe != nullptr)
        {
            if (!mOnDemandActive)
            {
                updateRealtimeProbeAllFaces(realtime_probe, cinematicLive);
                if (mSchedStats)
                {
                    mSchedFrame.mRtFaces += 6; // [ProbeOnDemand] call-site counter
                }
            }
            else if (realtime_probe == cinematicLive)
            {
                // [ProbeOnDemand] Every frame Live: a single-frame pass, tracked
                // here for the refresh-all barrier (this branch does not bump the
                // publication counters). The kind is read BEFORE the call.
                const bool every_frame_radiance = mRealtimeRadiancePass;
                const U64 every_frame_start_op = ALProbeDirty::nextOp();
                updateRealtimeProbeAllFaces(realtime_probe, cinematicLive);
                mSchedFrame.mRtFaces += 6;
                mLiveLastFaceOp = ALProbeDirty::nextOp();
                noteLivePassForBarrier(every_frame_radiance, every_frame_start_op);
                mRtSlice.reset();
            }
            else if (realtime_probe->mComplete)
            {
                // [ProbeOnDemand] a complete closest-dynamic probe is captured in
                // N-face slices (N = 1..6) through the secondary scratch.
                static LLCachedControl<S32> slice_faces(gSavedSettings, "RenderProbeRealtimeFacesPerFrame", 2);
                updateRealtimeSliced(realtime_probe, llclamp(static_cast<S32>(slice_faces), 1, 6));
            }
            else
            {
                // [ProbeOnDemand] incomplete: no realtime capture; it stays ORDINARY
                // and warms up through the queue.
                mRtSlice.reset();
            }
        }
        else if (mOnDemandActive)
        {
            mRtSlice.reset();
        }
    }

    static LLCachedControl<F32> sUpdatePeriod(gSavedSettings, "RenderDefaultProbeUpdatePeriod", 2.f);
    if (!mOnDemandActive)
    {
        if ((gFrameTimeSeconds - mDefaultProbe->mLastUpdateTime) < sUpdatePeriod)
        {
            if (mRenderReflectionProbeLevel == 0)
            { // when probes are disabled don't update the default probe more often than the prescribed update period
                oldestProbe = nullptr;
            }
        }
        else if (mRenderReflectionProbeLevel > 0)
        { // when probes are enabled don't update the default probe less often than the prescribed update period
          oldestProbe = mDefaultProbe;
        }
    }
    else
    {
        // [ProbeOnDemand] The default probe refreshes when it has something to do
        // (warm-up, its camera moved 16 m, clouds moved, safety), never more often
        // than the update period.
        const F64 now_on_demand = static_cast<F64>(gFrameTimeSeconds);
        const ALProbeSched::Record& default_rec = mDefaultProbe->mSched;
        const bool default_eligible = ALProbeSched::selectable(default_rec,
            ALProbeSched::View{ mDefaultProbe->mComplete, false, false,
                                mDefaultProbe->mCubeIndex != -1,
                                ALProbeSched::barrierIsMember(mBarrier, default_rec.mId) },
            mSchedPolicy, now_on_demand);
        if (mRenderReflectionProbeLevel == 0)
        {
            oldestProbe = (default_eligible &&
                           (gFrameTimeSeconds - mDefaultProbe->mLastUpdateTime) >= sUpdatePeriod)
                ? mDefaultProbe.get() : nullptr;
        }
        else if (default_eligible &&
                 now_on_demand - default_rec.mLastStart >=
                     static_cast<F64>(static_cast<F32>(sUpdatePeriod)))
        {
            oldestProbe = mDefaultProbe;
        }
        else if (!default_eligible && oldestProbe == mDefaultProbe)
        {
            oldestProbe = nullptr;
        }
    }

    // switch to updating the next oldest probe
    if (ord_tick && !did_update && oldestProbe != nullptr) // [ProbeManualRate] tick-gated
    {
        LLReflectionMap* probe = oldestProbe;
        llassert(probe->mCubeIndex != -1);

        probe->autoAdjustOrigin();

        sUpdateCount++;
        if (mOnDemandActive)
        {
            // [ProbeOnDemand] Transaction start: a capture always begins with the
            // irradiance pass (neutralises a stale radiance flag left behind by a
            // deleted probe), and records what it is about to capture.
            mRadiancePass = false;
            const F64 start_now = static_cast<F64>(gFrameTimeSeconds);
            ALProbeSched::Record& rec = probe->mSched;
            const ALProbeSched::Why why = ALProbeSched::evaluate(rec,
                ALProbeSched::View{ probe->mComplete, probe->mOccluded, probe->getIsDynamic(),
                                    probe->mCubeIndex != -1,
                                    ALProbeSched::barrierIsMember(mBarrier, rec.mId) },
                mSchedPolicy, start_now);
            const U16 reasons = static_cast<U16>(rec.mPending | ALProbeSched::whyReason(why));
            ALProbeSched::CaptureState cap;
            for (S32 c = 0; c < 3; ++c)
            {
                cap.mOrigin[c] = probe->mOrigin.getF32ptr()[c];
                cap.mCam[c] = LLViewerCamera::instance().getOrigin().mV[c];
            }
            cap.mRadius = probe->mRadius;
            cap.mAmbiance = probe->getAmbiance();
            cap.mNear = probe->getNearClip();
            cap.mDynamic = probe->getIsDynamic();
            cap.mBox = probe->mViewerObject.notNull() && probe->mViewerObject->getReflectionProbeIsBox();
            const LLVector2 cloud_scroll = LLEnvironment::instance().getCloudScrollDelta();
            cap.mCloud[0] = cloud_scroll.mV[0];
            cap.mCloud[1] = cloud_scroll.mV[1];
            ALProbeSched::onTxnStart(rec, mSchedSerial, mSchedEpoch, start_now, reasons,
                                     probe->mCubeIndex, &cap);
            ++mSchedWindow.mStarts;
            for (S32 bit = 0; bit < 12; ++bit)
            {
                if (reasons & static_cast<U16>(1u << bit))
                {
                    ++mSchedWindow.mStartReason[bit];
                }
            }
            if (ALProbeSched::noteStartWindow(rec, mSchedWindowIndex, reasons) &&
                !mSchedWindow.mUnsettled)
            {
                mSchedWindow.mUnsettled = true;
                mSchedWindow.mUnsettledId = rec.mId;
                mSchedWindow.mUnsettledReasons = reasons;
            }
        }
        mUpdatingProbe = probe;
        doProbeUpdate();
    }

    if (oldestOccluded)
    {
        // as far as this occluded probe is concerned, an origin/radius update is as good as a full update
        oldestOccluded->autoAdjustOrigin();
        oldestOccluded->mLastUpdateTime = gFrameTimeSeconds;
    }

    // [ProbeOnDemand] Fold this frame into the [ProbeSched] window and log it.
    if (mOnDemandActive)
    {
        mRtSliceLastRanId = mRtSliceRanThisFrame;
        mRtSliceRanThisFrame = 0;
    }
    if (mSchedStats)
    {
        logProbeSchedule(static_cast<F64>(gFrameTimeSeconds));
    }
}

// [LiveProbeRefresh] The shipped realtime capture: all six faces of
// realtime_probe every call, alternating irradiance / radiance. This body was
// moved verbatim out of update() (only re-indented) so the Every frame mode and
// the closest-dynamic fallback behave exactly as before.
void LLReflectionMapManager::updateRealtimeProbeAllFaces(
    LLReflectionMap* realtime_probe, LLReflectionMap* cinematicLive)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - realtime");
    // The designated cinematic probe owns the one realtime slot. Without
    // one, preserve the shipped closest-dynamic behavior.
    // should do a full irradiance pass on "odd" frames and a radiance pass on "even" frames
    realtime_probe->autoAdjustOrigin();

    // store and override the value of "isRadiancePass" -- parts of the render pipe rely on "isRadiancePass" to set
    // lighting values etc
    bool radiance_pass = isRadiancePass();
    mRadiancePass = mRealtimeRadiancePass;
    mCinematicLiveProbeCapture = realtime_probe == cinematicLive;
    const bool saved_nearby_lights = mCinematicLiveProbeCapture &&
        gPipeline.beginCinematicProbeCapture();
    for (U32 i = 0; i < 6; ++i)
    {
        updateProbeFace(realtime_probe, i);
    }
    if (mCinematicLiveProbeCapture)
    {
        if (mRealtimeRadiancePass)
        {
            mCinematicRadianceReady = true;
        }
        else
        {
            mCinematicIrradianceReady = true;
        }
        if (mCinematicIrradianceReady && mCinematicRadianceReady)
        {
            realtime_probe->mComplete = true;
            updateNeighbors(realtime_probe);
        }
    }
    if (saved_nearby_lights)
    {
        gPipeline.endCinematicProbeCapture();
    }
    mCinematicLiveProbeCapture = false;
    mRealtimeRadiancePass = !mRealtimeRadiancePass;

    // restore "isRadiancePass"
    mRadiancePass = radiance_pass;
}

// [LiveProbeRefresh] Budgeted capture of the pass the scheduler started: up to
// `faces` faces of one irradiance or radiance pass (never more than the pass
// has left). Publication happens only when the sixth face lands, exactly as in
// the six-face path; mComplete / mFadeIn are never touched, so the previous
// cube stays displayed while the next one builds.
void LLReflectionMapManager::updateCinematicBudget(
    LLReflectionMap* probe, S32 faces)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - cine budget");
    LL_PROFILE_ZONE_NUM(faces);
    if (!mCineRefresh.mActive)
    {
        return;
    }
    if (mCineRefresh.mFace == 0)
    {
        // FREEZE the origin for this whole pass so its six faces agree.
        mCineFrozenOrigin = probe->mOrigin;
    }
    const LLVector4a live_origin = probe->mOrigin;
    probe->mOrigin = mCineFrozenOrigin;
    const bool radiance_pass = isRadiancePass();
    mCinematicLiveProbeCapture = true;
    const bool saved_nearby_lights = gPipeline.beginCinematicProbeCapture();
    // Stops at the pass end: a pass never starts mid-frame. [ProbeManualRate]
    // Manual alone continues into the next pass so a burst is exactly `faces`.
    bool pass_ended = false;
    for (S32 n = 0; n < faces; ++n)
    {
        if (!mCineRefresh.mActive)
        {
            if (!ALCineLiveProbeRefresh::continueManualPass(
                    mCineRefresh, static_cast<F64>(gFrameTimeSeconds)))
            {
                break;
            }
            // [ProbeManualRate] new pass: re-freeze the origin, and remember its
            // start op for the barrier (update() applies it after noting the pass
            // that just ended).
            mCineFrozenOrigin = live_origin;
            probe->mOrigin = mCineFrozenOrigin;
            if (mOnDemandActive)
            {
                mCineContStartOp = ALProbeDirty::nextOp();
            }
        }
        mRadiancePass = mCineRefresh.mRadiance;
        updateProbeFace(probe, static_cast<U32>(mCineRefresh.mFace));
        ++mCineStats.mFaces;
        const ALCineLiveProbeRefresh::PassEnd pass_end =
            ALCineLiveProbeRefresh::advanceFace(
                mCineRefresh, static_cast<F64>(gFrameTimeSeconds));
        if (pass_end != ALCineLiveProbeRefresh::PassEnd::NONE)
        {
            pass_ended = true;
        }
        // clean / dirty describe the pass itself (H stayed stable across it),
        // for the same pass population as irr_pub + rad_pub.
        if (pass_end == ALCineLiveProbeRefresh::PassEnd::IRRADIANCE)
        {
            ++mCineStats.mIrrPub;
            ++(mCineRefresh.mPassClean ? mCineStats.mCleanPasses
                                       : mCineStats.mDirtyPasses);
        }
        else if (pass_end == ALCineLiveProbeRefresh::PassEnd::RADIANCE)
        {
            ++mCineStats.mRadPub;
            ++(mCineRefresh.mPassClean ? mCineStats.mCleanPasses
                                       : mCineStats.mDirtyPasses);
            mCinematicIrradianceReady = true;
            mCinematicRadianceReady = true;
            probe->mComplete = true;
        }
    }
    if (saved_nearby_lights)
    {
        gPipeline.endCinematicProbeCapture();
    }
    mCinematicLiveProbeCapture = false;
    mRadiancePass = radiance_pass;
    // The influence volume keeps following the subject.
    probe->mOrigin = live_origin;
    if (pass_ended) // == !mCineRefresh.mActive in modes 0-3 (they stop at the end)
    {
        updateNeighbors(probe); // after any pass end
    }
}

// [LiveProbeRefresh] Quantised hash H of everything that changes what the live
// probe would capture. Runs inside the manager step (after display() and the
// environment update), so extra display() calls can never advance it. Field
// order is fixed; see doc/LIVE_PROBE_REFRESH_DESIGN.md section 4.4.
U64 LLReflectionMapManager::sampleCinematicH(LLReflectionMap* probe)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("cine probe H");
    LLTimer hash_timer;
    static LLCachedControl<F32> move_tolerance(gSavedSettings, "CineLightRigLiveProbeMoveTolerance", 0.05f);
    const F32 move_m = llclamp((F32)move_tolerance, 0.001f, 1.f);

    ALCineLiveProbeRefresh::Signature& sig = mCineSig;
    sig.clear();

    // 1. Probe
    sig.addAbs3(probe->mOrigin.getF32ptr(), move_m);
    sig.addAbs(probe->mRadius, 0.02f);
    sig.addAbs(probe->getAmbiance(), 0.01f);
    sig.addExact(static_cast<U64>(static_cast<U32>(probe->mCubeIndex)));

    // 2. Rigs (all enabled slots light the capture)
    ALCineLightRigManager::instance().appendLiveProbeSignature(sig, move_m);

    // 2b. Non-rig local lights the capture could use (world prims + attachment
    // lights), AlchemyGlobalLightScale and the attached/bdmerge light gates.
    // Every light passing the capture's filters (no nearest-N / frustum
    // truncation); see LLPipeline::appendCinematicProbeLightSignature.
    gPipeline.appendCinematicProbeLightSignature(sig,
        LLVector3(probe->mOrigin.getF32ptr()), probe->mRadius, move_m);

    // 3-5. Sky, water and the capture-relevant settings. [ProbeOnDemand] These
    // sections were moved verbatim into gatherEnvSample() (the getters, in order)
    // and ALProbeSched::appendEnvironmentFields (the field layout, in order), so the
    // ordinary probes' environment generation can share them. This call keeps the
    // original 0.1 degree sun / moon direction tolerance and the verbatim blend
    // factors (unit-tested: S13).
    ALProbeSched::appendEnvironmentFields(sig, gatherEnvSample(), 0.1f, false);

    // 6. Sticky-quantised hash
    const U64 h = mCineSticky.update(sig);
    mCineLastH = h;

    const F64 elapsed_us = static_cast<F64>(hash_timer.getElapsedTimeF64()) * 1.0e6;
    mCineStats.mHashUsSum += elapsed_us;
    mCineStats.mHashUsMax = std::max(mCineStats.mHashUsMax, elapsed_us);
    ++mCineStats.mHashSamples;
    return h;
}

// [LiveProbeRefresh] Drop all scheduler state. Called before every early
// return of update(), on the non-scheduler branch, and at probe / map
// lifecycle changes, so no pass or convergence survives a discontinuity.
void LLReflectionMapManager::resetCinematicRefresh()
{
    if (mCineRefresh.mProbe == nullptr && mCineSticky.mAcc.empty() &&
        mCineStickyOn.mAcc.empty())
    {
        return; // nothing live; keep the per-frame early-return cost trivial
    }
    ALCineLiveProbeRefresh::reset(mCineRefresh);
    mCineSticky.mAcc.clear();
    mCineSticky.mLayout.clear();
    mCineStickyOn.mAcc.clear(); // [ProbeOnDemand] the ON-path H history
    mCineStickyOn.mLayout.clear();
    mCineLastH = 0;
    mCineLastPath = ALCineLiveProbeRefresh::Path::IDLE;
    ++mCineStats.mResets;
}

void LLReflectionMapManager::requestCinematicLiveProbeRefresh()
{
    // Only On change and Manual ([ProbeManualRate]) consume a manual request;
    // latching it in the other modes would leave it pending until an unrelated
    // reset wiped it.
    const ALCineLiveProbeRefresh::Mode request_mode =
        ALCineLiveProbeRefresh::sanitizeMode(
            gSavedSettings.getS32("CineLightRigLiveProbeRefresh"));
    if (request_mode == ALCineLiveProbeRefresh::Mode::ON_CHANGE ||
        request_mode == ALCineLiveProbeRefresh::Mode::MANUAL)
    {
        mCineRefresh.mManual = true;
    }
}

LLReflectionMapManager::CinematicRefreshStatus
LLReflectionMapManager::getCinematicRefreshStatus() const
{
    CinematicRefreshStatus status;
    const ALCineLiveProbeRefresh::Mode configured =
        ALCineLiveProbeRefresh::sanitizeMode(
            gSavedSettings.getS32("CineLightRigLiveProbeRefresh"));
    status.mMode = configured;
    status.mStepping = configured != ALCineLiveProbeRefresh::Mode::EVERY_FRAME &&
        mCineRefresh.mProbe != nullptr && mCineRefresh.mMode == configured;
    if (!status.mStepping)
    {
        return status;
    }
    status.mPath = mCineLastPath;
    status.mReason = mCineRefresh.mReason;
    if (mCineRefresh.mReasonTime >= 0.0)
    {
        status.mSecondsSinceReason = static_cast<F32>(std::max(
            0.0, static_cast<F64>(gFrameTimeSeconds) - mCineRefresh.mReasonTime));
    }
    status.mPassFace = mCineRefresh.mActive
        ? static_cast<S32>(mCineRefresh.mFace) : -1;
    status.mPassIsRadiance = mCineRefresh.mRadiance;
    status.mConverged = ALCineLiveProbeRefresh::converged(mCineRefresh, mCineLastH);
    return status;
}

// [LiveProbeRefresh] Once-per-second one-line summary (setting-gated).
void LLReflectionMapManager::logCinematicRefresh(F64 now)
{
    static LLCachedControl<bool> refresh_log(gSavedSettings, "CineLightRigLiveProbeRefreshLog", false);
    if (!refresh_log)
    {
        mCineStats.mWindowStart = -1.0;
        return;
    }
    // A gap since the last scheduler step (Every frame mode, paused, early
    // returns, log toggled) must not let the "per-second" window span many
    // seconds of raw counts: restart it. Resets seen during the gap are kept.
    const bool gap = mCineStats.mLastStep >= 0.0 && now - mCineStats.mLastStep > 0.5;
    mCineStats.mLastStep = now;
    if (mCineStats.mWindowStart < 0.0 || gap)
    {
        const U32 kept_resets = mCineStats.mResets;
        mCineStats = CineRefreshStats();
        mCineStats.mResets = kept_resets;
        mCineStats.mWindowStart = now;
        mCineStats.mLastStep = now;
        return;
    }
    if (now - mCineStats.mWindowStart < 1.0)
    {
        return;
    }
    const CineRefreshStats& s = mCineStats;
    const F64 idle_pct = s.mSteps > 0
        ? 100.0 * static_cast<F64>(s.mIdleFrames) / static_cast<F64>(s.mSteps) : 0.0;
    const F64 hash_avg = s.mHashSamples > 0
        ? s.mHashUsSum / static_cast<F64>(s.mHashSamples) : 0.0;
    const bool is_converged = ALCineLiveProbeRefresh::converged(mCineRefresh, mCineLastH);
    const F64 reason_age = mCineRefresh.mReasonTime >= 0.0
        ? std::max(0.0, now - mCineRefresh.mReasonTime) : -1.0;
    static const char* const mode_names[] = { "every-frame", "balanced", "economy", "on-change", "manual" };
    LL_INFOS("LiveProbe") << llformat(
        "[LiveProbe] mode=%s steps=%u faces=%u full=%u budget=%u idle=%u(%.0f%%) "
        "irr_pub=%u rad_pub=%u clean=%u dirty=%u blocked=%u resets=%u "
        "hash_us=%.1f/%.1f converged=%s last=%s %.1fs",
        mode_names[static_cast<S32>(mCineRefresh.mMode)], s.mSteps, s.mFaces,
        s.mFullFrames, s.mBudgetFrames, s.mIdleFrames, idle_pct,
        s.mIrrPub, s.mRadPub, s.mCleanPasses, s.mDirtyPasses, s.mBlocked,
        s.mResets, hash_avg, s.mHashUsMax, is_converged ? "y" : "n",
        ALCineLiveProbeRefresh::reasonName(mCineRefresh.mReason), reason_age)
        << LL_ENDL;
    mCineStats = CineRefreshStats();
    mCineStats.mWindowStart = now;
    mCineStats.mLastStep = now;
}

// ===========================================================================
// [ProbeOnDemand] On-demand probe scheduling.
// Spec: doc/PROBE_UPDATE_ON_DEMAND_BRIEF_V6.md. The pure model (events, records,
// debounce, light diff, recount, barrier, signatures) lives in alprobeschedule.h;
// this block is the viewer-facing glue. Everything here is a no-op with
// RenderProbeOnDemand OFF.
// ===========================================================================
namespace
{
void probe_copy3(F32* dst, const F32* src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

bool probe_box_overlap(const F32 amn[3], const F32 amx[3], const F32 bmn[3], const F32 bmx[3])
{
    for (S32 i = 0; i < 3; ++i)
    {
        if (amx[i] < bmn[i] || bmx[i] < amn[i])
        {
            return false;
        }
    }
    return true;
}

// Everything the capture could use from one light, plus whether the capture's
// filters would accept it (a mirror of the transient branch of
// LLPipeline::calcNearbyLights minus the Live ignored / pinned lists and the
// probe-dependent frustum / distance tests; isTooSlow is not evaluated under
// on-demand).
void probe_gather_light(LLDrawable* drawable, LLVOVolume* light, F32 light_scale,
                        ALProbeSched::LightSample& s)
{
    s.mId = light->getID();
    const LLVector3 pos = drawable->getPositionAgent();
    probe_copy3(s.mPos, pos.mV);
    const LLQuaternion rotation = light->getRenderRotation();
    const LLVector3 forward = LLVector3(0.f, 0.f, -1.f) * rotation;
    const LLVector3 up = LLVector3(0.f, 1.f, 0.f) * rotation;
    probe_copy3(s.mFwd, forward.mV);
    probe_copy3(s.mUp, up.mV);
    s.mRadius = light->getLightRadius();
    s.mFalloff = light->getLightFalloff();
    probe_copy3(s.mScale, light->getScale().mV);
    s.mSpot = light->isLightSpotlight();
    probe_copy3(s.mSpotParams, light->getSpotLightParams().mV);
    s.mTexId = light->getLightTextureID();
    s.mNoShadow = LLPipeline::isProjectorNoShadow(light->getID());
    const LLPipeline::GoboOverride gobo = LLPipeline::getGoboOverride(light->getID());
    s.mGobo.mPattern = gobo.mPattern;
    s.mGobo.mAnimMode = gobo.mAnimMode;
    s.mGobo.mSpeed = gobo.mSpeed;
    s.mGobo.mZoom = gobo.mZoom;
    s.mGobo.mDispersion = gobo.mDispersion;
    probe_copy3(s.mGobo.mTint, gobo.mTint.mV);
    for (S32 i = 0; i < 4; ++i)
    {
        s.mGobo.mParams[i] = gobo.mPatternParams.mV[i];
    }

    const F32 ls = light_scale * ALEnvIntensity::localLightEVScale(light);
    const LLColor3 light_color = light->getLightLinearColor() * ls;
    probe_copy3(s.mColor, light_color.mV);

    bool eligible = drawable->isState(LLDrawable::LIGHT) && !light->isHUDAttachment();
    if (eligible)
    {
        if (light->isAttachment())
        {
            if (!LLPipeline::sRenderAttachedLights)
            {
                eligible = false;
            }
            else
            {
                LLVOAvatar* avatar = light->getAvatar();
                if (!LLPipeline::probeShouldRenderLight(true, avatar == gAgentAvatarp) ||
                    (avatar && (avatar->isTooComplex() || avatar->isInMuteList())))
                {
                    eligible = false;
                }
            }
        }
        else if (!light->isCineRigEmitter() && !LLPipeline::probeShouldRenderLight(false, false))
        {
            eligible = false;
        }
    }
    if (s.mRadius * 1.5f <= 0.001f || light_color.magVecSquared() < 0.001f)
    {
        eligible = false;
    }
    s.mEligible = eligible;
}

} // namespace

// The Live probe H's environment sections (sky, water, capture-relevant
// settings): exactly the getters the Live probe H reads, in order, into a POD.
// Static: no manager state is touched.
ALProbeSched::EnvSample LLReflectionMapManager::gatherEnvSample()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("probe sched env");
    static LLCachedControl<bool> auto_adjust_legacy(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    ALProbeSched::EnvSample e;

    // 3. Sky
    LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (sky)
    {
        ALProbeSched::SkySample& s = e.mSky;
        s.mValid = true;
        probe_copy3(s.mSunDir.mV, sky->getSunDirection().mV);
        probe_copy3(s.mMoonDir.mV, sky->getMoonDirection().mV);
        probe_copy3(s.mSunlight.mV, sky->getSunlightColor().mV);
        probe_copy3(s.mMoonlight.mV, sky->getMoonlightColor().mV);
        probe_copy3(s.mCloudColor.mV, sky->getCloudColor().mV);
        probe_copy3(s.mAmbient.mV, sky->getAmbientColor().mV);
        probe_copy3(s.mBlueDensity.mV, sky->getBlueDensity().mV);
        probe_copy3(s.mBlueHorizon.mV, sky->getBlueHorizon().mV);
        probe_copy3(s.mGlow.mV, sky->getGlow().mV);
        probe_copy3(s.mCloudPosDensity1.mV, sky->getCloudPosDensity1().mV);
        probe_copy3(s.mCloudPosDensity2.mV, sky->getCloudPosDensity2().mV);
        s.mHazeDensity = sky->getHazeDensity();
        s.mDensityMultiplier = sky->getDensityMultiplier();
        s.mDistanceMultiplier = sky->getDistanceMultiplier();
        s.mCloudScale = sky->getCloudScale();
        s.mStarBrightness = sky->getStarBrightness();
        s.mDropletRadius = sky->getSkyDropletRadius();
        s.mMaxY = sky->getMaxY();
        s.mHazeHorizon = sky->getHazeHorizon();
        s.mCloudShadow = sky->getCloudShadow();
        s.mCloudVariance = sky->getCloudVariance();
        s.mMoonBrightness = sky->getMoonBrightness();
        s.mMoistureLevel = sky->getSkyMoistureLevel();
        s.mIceLevel = sky->getSkyIceLevel();
        s.mReflectionAmbiance = sky->getReflectionProbeAmbiance(auto_adjust_legacy);
        s.mGamma = sky->getGamma();
        s.mSunMoonGlowFactor = sky->getSunMoonGlowFactor();
        // Sun / moon disc size: drawn into the cube faces (llvosky) and edited
        // live by Personal Lighting without a manager reset.
        s.mSunScale = sky->getSunScale();
        s.mMoonScale = sky->getMoonScale();
        s.mBlendFactor = static_cast<F32>(sky->getBlendFactor());
        s.mIsSunUp = sky->getIsSunUp();
        s.mSunTex = sky->getSunTextureId();
        s.mMoonTex = sky->getMoonTextureId();
        s.mCloudNoiseTex = sky->getCloudNoiseTextureId();
        s.mBloomTex = sky->getBloomTextureId();
        s.mRainbowTex = sky->getRainbowTextureId();
        s.mHaloTex = sky->getHaloTextureId();
        s.mNextSunTex = sky->getNextSunTextureId();
        s.mNextMoonTex = sky->getNextMoonTextureId();
        s.mNextCloudNoiseTex = sky->getNextCloudNoiseTextureId();
    }

    // 4. Water
    LLSettingsWater::ptr_t water = LLEnvironment::instance().getCurrentWater();
    if (water)
    {
        ALProbeSched::WaterSample& w = e.mWater;
        w.mValid = true;
        probe_copy3(w.mFogColor.mV, water->getWaterFogColor().mV);
        w.mFogDensity = water->getModifiedWaterFogDensity(false);
        w.mFogMod = water->getFogMod();
        w.mFresnelScale = water->getFresnelScale();
        w.mFresnelOffset = water->getFresnelOffset();
        w.mScaleAbove = water->getScaleAbove();
        w.mScaleBelow = water->getScaleBelow();
        w.mBlendFactor = static_cast<F32>(water->getBlendFactor());
        w.mBlurMultiplier = water->getBlurMultiplier();
        const LLVector2 wave1 = water->getWave1Dir();
        w.mWave1[0] = wave1.mV[0];
        w.mWave1[1] = wave1.mV[1];
        const LLVector2 wave2 = water->getWave2Dir();
        w.mWave2[0] = wave2.mV[0];
        w.mWave2[1] = wave2.mV[1];
        probe_copy3(w.mNormalScale.mV, water->getNormalScale().mV);
        w.mRenderWaterHeight = gPipeline.getRenderWaterHeight();
        w.mNormalMap = water->getNormalMapID();
        w.mNextNormalMap = water->getNextNormalMapID();
        w.mTransparent = water->getTransparentTextureID();
        w.mNextTransparent = water->getNextTransparentTextureID();
    }

    // 5. Capture-relevant settings. GI / ambient sampling EVs are identity
    // during captures (alenvintensity.cpp) and are deliberately excluded.
    static LLCachedControl<F32> sun_ev(gSavedSettings, "AlchemyEnvSunEV", 0.f);
    static LLCachedControl<F32> moon_ev(gSavedSettings, "AlchemyEnvMoonEV", 0.f);
    static LLCachedControl<F32> local_light_ev(gSavedSettings, "AlchemyEnvLocalLightEV", 0.f);
    static LLCachedControl<F32> shadow_lift_ev(gSavedSettings, "AlchemyEnvShadowLiftEV", 0.f);
    static LLCachedControl<F32> sun_kelvin(gSavedSettings, "AlchemyEnvSunKelvin", 6500.f);
    static LLCachedControl<LLColor4> sun_tint_color(gSavedSettings, "AlchemyEnvSunTintColor", LLColor4::white);
    static LLCachedControl<LLColor4> moon_tint_color(gSavedSettings, "AlchemyEnvMoonTintColor", LLColor4::white);
    static LLCachedControl<F32> sun_tint_strength(gSavedSettings, "AlchemyEnvSunTintStrength", 1.f);
    static LLCachedControl<F32> moon_tint_strength(gSavedSettings, "AlchemyEnvMoonTintStrength", 1.f);
    static LLCachedControl<bool> moon_linked(gSavedSettings, "AlchemyEnvMoonLinked", true);
    static LLCachedControl<bool> local_light_include_rig(gSavedSettings, "AlchemyEnvLocalLightIncludeRig", false);
    static LLCachedControl<S32> shadow_detail(gSavedSettings, "RenderShadowDetail", 2);
    static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);
    ALProbeSched::EnvSettings& st = e.mSet;
    st.mSunEV = (F32)sun_ev;
    st.mMoonEV = (F32)moon_ev;
    st.mLocalLightEV = (F32)local_light_ev;
    st.mShadowLiftEV = (F32)shadow_lift_ev;
    st.mSunKelvin = (F32)sun_kelvin;
    const LLColor4 sun_tint = sun_tint_color;
    const LLColor4 moon_tint = moon_tint_color;
    probe_copy3(st.mSunTint, sun_tint.mV);
    probe_copy3(st.mMoonTint, moon_tint.mV);
    st.mSunTintStrength = (F32)sun_tint_strength;
    st.mMoonTintStrength = (F32)moon_tint_strength;
    st.mMoonLinked = (bool)moon_linked;
    st.mLocalLightIncludeRig = (bool)local_light_include_rig;
    st.mShadowDetail = (S32)shadow_detail;
    st.mLocalLightCount = (S32)local_light_count;
    st.mAutoAdjustLegacy = (bool)auto_adjust_legacy;
    return e;
}

// Environment generation of the ordinary probes: the shared environment fields
// (sun / moon direction tolerance RenderProbeDirtySunDeg; blend factors zeroed
// while their textures are not mixing) plus a fixed-layout tail of everything
// else the capture depends on.
U64 LLReflectionMapManager::sampleOrdinaryEnvH()
{
    static LLCachedControl<F32> sun_deg(gSavedSettings, "RenderProbeDirtySunDeg", 0.5f);
    static LLCachedControl<F32> draw_distance(gSavedSettings, "RenderReflectionProbeDrawDistance", 64.f);
    static LLCachedControl<S32> probe_detail(gSavedSettings, "RenderReflectionProbeDetail", 1);
    static LLCachedControl<S32> probe_level(gSavedSettings, "RenderReflectionProbeLevel", 3);
    static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);
    static LLCachedControl<F32> max_local_light_ambiance(gSavedSettings, "RenderReflectionProbeMaxLocalLightAmbiance", 8.f);
    static LLCachedControl<bool> attached_lights(gSavedSettings, "RenderAttachedLights", true);
    static LLCachedControl<bool> bd_toggles(gSavedSettings, "BDMergeLightToggles", false);
    static LLCachedControl<bool> bd_own(gSavedSettings, "BDMergeRenderOwnAttachedLights", true);
    static LLCachedControl<bool> bd_others(gSavedSettings, "BDMergeRenderOthersAttachedLights", true);
    static LLCachedControl<bool> bd_world(gSavedSettings, "BDMergeRenderWorldLights", true);
    static LLCachedControl<bool> bd_projectors(gSavedSettings, "BDMergeRenderProjectors", true);
    static LLCachedControl<F32> global_light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);
    static LLCachedControl<bool> rim_include_probes(gSavedSettings, "CineRigRimIncludeProbes", false);
    static LLCachedControl<bool> rim_enabled(gSavedSettings, "CineRigRimEnabled", false);
    static LLCachedControl<F32> rim_master_gain(gSavedSettings, "CineRigRimMasterGain", 1.f);
    static LLCachedControl<F32> rim_back_softness(gSavedSettings, "CineRigRimBackSoftness", 0.5f);
    static LLCachedControl<F32> rim_roughness_soften(gSavedSettings, "CineRigRimRoughnessSoften", 0.5f);
    static LLCachedControl<F32> rim_shadow(gSavedSettings, "CineRigRimShadow", 1.f);
    static LLCachedControl<F32> rim_tron_mix(gSavedSettings, "CineRigRimTronMix", 1.f);
    static LLCachedControl<F32> rim_tint(gSavedSettings, "CineRigRimTint", 0.25f);
    static LLCachedControl<bool> rim_debug_only(gSavedSettings, "CineRigRimDebugRimOnly", false);
    static LLCachedControl<bool> rim_include_alpha(gSavedSettings, "CineRigRimIncludeAlpha", true);
    static LLCachedControl<S32> rim_tron_mode(gSavedSettings, "CineRigRimTronMode", 0);
    static LLCachedControl<S32> rim_tron_color_source(gSavedSettings, "CineRigRimTronColorSource", 0);

    mOrdEnvSig.clear();
    ALProbeSched::appendEnvironmentFields(mOrdEnvSig, gatherEnvSample(),
        llclamp(static_cast<F32>(sun_deg), 0.1f, 5.f), true);

    ALProbeSched::OrdTailSample tail;
    tail.mDrawDistance = static_cast<F32>(draw_distance);
    tail.mListRange = llmin(LLPipeline::RenderFarClip, static_cast<F32>(draw_distance));
    tail.mProbeDetail = static_cast<S32>(probe_detail);
    tail.mProbeLevel = static_cast<S32>(probe_level);
    tail.mLocalLightCount = static_cast<S32>(local_light_count);
    tail.mMaxLocalLightAmbiance = static_cast<F32>(max_local_light_ambiance);
    tail.mAttachedLights = static_cast<bool>(attached_lights);
    tail.mBdToggles = static_cast<bool>(bd_toggles);
    tail.mBdOwn = static_cast<bool>(bd_own);
    tail.mBdOthers = static_cast<bool>(bd_others);
    tail.mBdWorld = static_cast<bool>(bd_world);
    tail.mBdProjectors = static_cast<bool>(bd_projectors);
    tail.mGlobalLightScale = static_cast<F32>(global_light_scale);
    tail.mRimIncludeProbes = static_cast<bool>(rim_include_probes);
    tail.mRimEnabled = static_cast<bool>(rim_enabled);
    tail.mRimMasterGain = static_cast<F32>(rim_master_gain);
    tail.mRimBackSoftness = static_cast<F32>(rim_back_softness);
    tail.mRimRoughnessSoften = static_cast<F32>(rim_roughness_soften);
    tail.mRimShadow = static_cast<F32>(rim_shadow);
    tail.mRimTronMix = static_cast<F32>(rim_tron_mix);
    tail.mRimTint = static_cast<F32>(rim_tint);
    tail.mRimDebugOnly = static_cast<bool>(rim_debug_only);
    tail.mRimIncludeAlpha = static_cast<bool>(rim_include_alpha);
    tail.mRimTronMode = static_cast<S32>(rim_tron_mode);
    tail.mRimTronColorSource = static_cast<S32>(rim_tron_color_source);
    tail.mSlotCount = ALCineLightRigManager::SLOT_COUNT;
    tail.mLightCount = ALCineLightRigModel::LIGHT_COUNT;
    if (tail.mRimIncludeProbes && tail.mRimEnabled)
    {
        // Only read the per-slot rim parameters while they can matter.
        for (S32 i = 0; i < ALCineLightRigManager::SLOT_COUNT && i < ALProbeSched::kMaxRimSlots; ++i)
        {
            const auto slot = static_cast<ALCineLightRigManager::Slot>(i);
            const bool enabled = ALCineLightRigManager::instance().isSlotEnabled(slot);
            tail.mSlotEnabled[i] = enabled;
            if (!enabled)
            {
                continue;
            }
            for (S32 j = 0; j < ALCineLightRigModel::LIGHT_COUNT && j < ALProbeSched::kMaxRimLights; ++j)
            {
                const LLVector4& rim = ALCineLightRigManager::instance().at(slot).rigRimParams(j);
                for (S32 c = 0; c < 4; ++c)
                {
                    tail.mRimParams[i][j][c] = rim.mV[c];
                }
            }
        }
    }
    ALProbeSched::appendOrdinaryTail(mOrdEnvSig, tail);
    return mOrdEnvSticky.update(mOrdEnvSig);
}

// The Live Probe's H on the on-demand path. The probe section is raw (the Live
// probe follows its subject); lights come from the debounced light diff
// (PUBLISHED positions / hashes), so light flicker, moving facelights and rig
// transitions neither force the FULL path nor change H while they run.
U64 LLReflectionMapManager::sampleCinematicHOnDemand(LLReflectionMap* probe)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("cine probe H on");
    LLTimer hash_timer;
    static LLCachedControl<F32> move_tolerance(gSavedSettings, "CineLightRigLiveProbeMoveTolerance", 0.05f);
    static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);
    static LLCachedControl<F32> light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);
    static LLCachedControl<bool> render_attached(gSavedSettings, "RenderAttachedLights", true);
    static LLCachedControl<bool> bd_toggles(gSavedSettings, "BDMergeLightToggles", false);
    static LLCachedControl<bool> bd_own(gSavedSettings, "BDMergeRenderOwnAttachedLights", true);
    static LLCachedControl<bool> bd_others(gSavedSettings, "BDMergeRenderOthersAttachedLights", true);
    static LLCachedControl<bool> bd_world(gSavedSettings, "BDMergeRenderWorldLights", true);
    static LLCachedControl<bool> bd_projectors(gSavedSettings, "BDMergeRenderProjectors", true);

    ALProbeSched::LiveHInput in;
    probe_copy3(in.mOrigin, probe->mOrigin.getF32ptr());
    in.mRadius = probe->mRadius;
    in.mAmbiance = probe->getAmbiance();
    in.mCube = probe->mCubeIndex;
    in.mMoveM = llclamp(static_cast<F32>(move_tolerance), 0.001f, 1.f);
    in.mGlobals.mGlobalScale = static_cast<F32>(light_scale);
    in.mGlobals.mGoboAniso = LLPipeline::BDMergeGoboAnisotropic;
    in.mGlobals.mAttached = static_cast<bool>(render_attached);
    in.mGlobals.mBdToggles = static_cast<bool>(bd_toggles);
    in.mGlobals.mBdOwn = static_cast<bool>(bd_own);
    in.mGlobals.mBdOthers = static_cast<bool>(bd_others);
    in.mGlobals.mBdWorld = static_cast<bool>(bd_world);
    in.mGlobals.mBdProjectors = static_cast<bool>(bd_projectors);
    in.mEffectiveCount = std::max(static_cast<S32>(local_light_count),
                                  static_cast<S32>(mCinematicPinnedLightIds.size()));
    in.mSceneSerial = mLiveSceneSerial;

    static std::vector<ALProbeSched::LiveToken> tokens;
    tokens.clear();
    const F32 camera_far = LLViewerCamera::instance().getFar();
    const F32 max_dist = LLPipeline::sRenderDeferred
        ? llmin(LLPipeline::RenderFarClip, camera_far)
        : llmin(llmin(LLPipeline::RenderFarClip, camera_far), LIGHT_MAX_RADIUS * 4.f);
    for (const ALProbeSched::LightEntry* entry : mLightsBySeq)
    {
        // The ignored / pinned predicates are capture-gated, so read the lists directly.
        const bool ignored = std::find(mCinematicIgnoredLightIds.begin(),
            mCinematicIgnoredLightIds.end(), entry->mId) != mCinematicIgnoredLightIds.end();
        const bool pinned = std::find(mCinematicPinnedLightIds.begin(),
            mCinematicPinnedLightIds.end(), entry->mId) != mCinematicPinnedLightIds.end();
        if (ALProbeSched::liveSelects(*entry, in.mOrigin, max_dist, pinned, ignored))
        {
            tokens.push_back(ALProbeSched::liveTokenOf(*entry));
        }
    }

    mCineSigOn.clear();
    ALProbeSched::buildLiveOnSignature(mCineSigOn, in, tokens, gatherEnvSample());
    const U64 h = mCineStickyOn.update(mCineSigOn);
    mCineLastH = h;

    const F64 elapsed_us = static_cast<F64>(hash_timer.getElapsedTimeF64()) * 1.0e6;
    mCineStats.mHashUsSum += elapsed_us;
    mCineStats.mHashUsMax = std::max(mCineStats.mHashUsMax, elapsed_us);
    ++mCineStats.mHashSamples;
    return h;
}

U32 LLReflectionMapManager::schedId(LLReflectionMap* probe)
{
    if (probe->mSched.mId == 0)
    {
        probe->mSched.mId = mSchedNextId++;
    }
    return probe->mSched.mId;
}

LLReflectionMap* LLReflectionMapManager::findProbeById(U32 id) const
{
    for (const auto& probe : mProbes)
    {
        if (probe->mSched.mId == id)
        {
            return probe.get();
        }
    }
    return nullptr;
}

// A finished Live pass (FULL, budget or Every-frame single frame) for the
// refresh-all barrier. The barrier binds to the Live probe's record id.
void LLReflectionMapManager::noteLivePassForBarrier(bool radiance, U64 start_op)
{
    if (mCinematicLiveProbe.isNull())
    {
        return;
    }
    ALProbeSched::barrierLivePass(mBarrier, mCinematicLiveProbe->mSched.mId, radiance, start_op);
}

void LLReflectionMapManager::requestRefreshAllProbes()
{
    if (!mOnDemandActive)
    {
        return;
    }
    mRefreshAllRequested = true;
    // One full irradiance + radiance pair of the Live Probe (On change / Manual).
    requestCinematicLiveProbeRefresh();
}

std::string LLReflectionMapManager::getRefreshAllStatus() const
{
    if (mRefreshAllRequested)
    {
        return "Refresh all: requested";
    }
    if (mBarrier.mActive)
    {
        return ALProbeSched::barrierReadout(mBarrier);
    }
    if (mBarrier.mTerminal != ALProbeSched::Barrier::Terminal::NONE &&
        static_cast<F64>(gFrameTimeSeconds) - mBarrier.mTerminalTime < 10.0)
    {
        return ALProbeSched::barrierReadout(mBarrier);
    }
    return std::string();
}

// A manager early return (teleport, logout, no probes): the closest-dynamic slice
// state and an armed barrier do not survive it (cancelled).
void LLReflectionMapManager::noteScheduleEarlyReturn()
{
    mRtSlice.reset();
    mRtSliceLastRanId = 0;
    mRtSliceRanThisFrame = 0;
    if (mBarrier.mActive)
    {
        ALProbeSched::barrierCancel(mBarrier, false, static_cast<F64>(gFrameTimeSeconds));
    }
    if (mSchedStats)
    {
        ++mSchedWindow.mFrames;
        ++mSchedWindow.mEarlyFrames;
    }
}

// Drop every piece of schedule state: reflection map reset, teleport / EEP reset,
// cleanup, and the ON -> OFF edge. The next ON frame is an ON edge (resync).
void LLReflectionMapManager::resetProbeSchedule(bool disabled)
{
    ++mSchedEpoch;
    for (auto& probe : mProbes)
    {
        ALProbeSched::clearTxn(probe->mSched);
    }
    mLightSnap.clear();
    mLightsBySeq.clear();
    mRecount.reset();
    ALProbeDirty::reset();
    mRtSlice.reset();
    mRtSliceLastRanId = 0;
    mRtSliceRanThisFrame = 0;
    mOrdEnvValid = false;
    mOrdEnvH = 0;
    mOrdEnvSticky.mAcc.clear();
    mOrdEnvSticky.mLayout.clear();
    mCineStickyOn.mAcc.clear();
    mCineStickyOn.mLayout.clear();
    mRefreshAllRequested = false;
    if (mBarrier.mActive)
    {
        ALProbeSched::barrierCancel(mBarrier, !disabled, static_cast<F64>(gFrameTimeSeconds));
    }
    mSchedWasOn = false;
}

// Camera-independent light diff (U1): lights near any ordinary probe (and every
// eligible spot, at any distance) are tracked by their own state; their changes
// become events. XFORM / PHOTO motion is debounced (published at the settle).
void LLReflectionMapManager::flushLightDiff(F64 now, U64 serial, F32 dt, F32 rcap,
                                            std::vector<ALProbeSched::Event>& events)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("probe sched lights");
    static LLCachedControl<F32> light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);

    // Union of the capture spheres of the ORDINARY probes (events are emitted only
    // for lights that reach these) and, separately, of the Live probe (lights only
    // Live sees are tracked SILENTLY for its tokens). The default probe sits at the
    // camera + 64 m, so it is deliberately excluded: a watch set that follows the
    // camera would turn camera motion into light add / remove events.
    F32 umn[3], umx[3];
    F32 lmn_live[3], lmx_live[3];
    ALProbeSched::boxEmpty(umn, umx);
    ALProbeSched::boxEmpty(lmn_live, lmx_live);
    for (const auto& probe : mProbes)
    {
        if (probe->mCubeIndex == -1)
        {
            continue;
        }
        F32 bmn[3], bmx[3];
        if (probe == mCinematicLiveProbe)
        {
            ALProbeSched::boxSphere(bmn, bmx, probe->mOrigin.getF32ptr(), rcap);
            ALProbeSched::boxUnion(lmn_live, lmx_live, bmn, bmx);
        }
        else if (probe->mSched.mOwner == ALProbeSched::Owner::ORDINARY && probe != mDefaultProbe)
        {
            ALProbeSched::boxSphere(bmn, bmx, probe->mOrigin.getF32ptr(), rcap);
            ALProbeSched::boxUnion(umn, umx, bmn, bmx);
        }
    }

    for (auto& kv : mLightSnap)
    {
        kv.second.mSeen = false;
    }
    static std::vector<ALProbeSched::Event> silent_events; // discarded: Live-only lights
    const size_t events_before = events.size();
    U32 processed = 0;
    U32 total = 0;
    for (const LLPointer<LLDrawable>& light_drawable : gPipeline.getLights())
    {
        ++total;
        LLDrawable* drawable = light_drawable.get();
        LLVOVolume* light = drawable ? drawable->getVOVolume() : nullptr;
        if (!light)
        {
            continue;
        }
        // Cheap prefilter: inside the union of capture spheres, or an eligible spot.
        const LLVector3 pos = drawable->getPositionAgent();
        const F32 pc[3] = { pos.mV[0], pos.mV[1], pos.mV[2] };
        F32 lmn[3], lmx[3];
        ALProbeSched::boxSphere(lmn, lmx, pc, 1.5f * light->getLightRadius());
        const bool in_union = !ALProbeSched::boxIsEmpty(umn, umx) &&
                              probe_box_overlap(lmn, lmx, umn, umx);
        const bool in_live = !ALProbeSched::boxIsEmpty(lmn_live, lmx_live) &&
                             probe_box_overlap(lmn, lmx, lmn_live, lmx_live);
        if (!in_union && !in_live && !light->isLightSpotlight())
        {
            continue;
        }
        ALProbeSched::LightSample sample;
        probe_gather_light(drawable, light, static_cast<F32>(light_scale), sample);
        // Events only for lights an ordinary probe can see (or an eligible spot,
        // at any distance); everything else here is Live-only and silent.
        const bool loud = in_union || (sample.mSpot && sample.mEligible);
        if (!loud && !in_live)
        {
            continue;
        }
        ++processed;

        auto it = mLightSnap.find(drawable);
        bool is_new = false;
        if (it != mLightSnap.end() && it->second.mId != sample.mId)
        {
            // The pointer was reused by a different light object: deliver the old
            // one's removal before the new one starts.
            if (!it->second.mSilent)
            {
                ALProbeSched::lightLeave(it->second, serial, events);
            }
            const U64 old_seq = it->second.mSeq;
            auto pos_it = std::lower_bound(mLightsBySeq.begin(), mLightsBySeq.end(), old_seq,
                [](const ALProbeSched::LightEntry* l, U64 seq) { return l->mSeq < seq; });
            if (pos_it != mLightsBySeq.end() && (*pos_it)->mSeq == old_seq)
            {
                mLightsBySeq.erase(pos_it);
            }
            mLightSnap.erase(it);
            it = mLightSnap.end();
        }
        if (it == mLightSnap.end())
        {
            it = mLightSnap.try_emplace(drawable).first;
            it->second.mSeq = mLightNextSeq++;
            it->second.mSilent = !loud;
            mLightsBySeq.push_back(&it->second);
            is_new = true;
        }
        ALProbeSched::LightEntry& entry = it->second;
        entry.mSeen = true;
        if (!is_new && entry.mSilent && loud)
        {
            // A Live-only light became visible to an ordinary probe: update its
            // state quietly, then announce it as an addition.
            silent_events.clear();
            ALProbeSched::lightStep(entry, sample, false, serial, now, dt,
                                    mLightDebounceConfig, silent_events);
            entry.mSilent = false;
            if (sample.mEligible)
            {
                F32 amn[3], amx[3];
                ALProbeSched::boxSphere(amn, amx, sample.mPos, 1.5f * sample.mRadius);
                ALProbeSched::Event add = ALProbeSched::makeEvent(amn, amx,
                    ALProbeSched::R_LIGHT, ALProbeSched::C_LIGHT, serial);
                add.mSpot = sample.mSpot;
                events.push_back(add);
            }
        }
        else if (!is_new && !entry.mSilent && !loud)
        {
            // Left every ordinary probe's reach: a removal for them, silent from now on.
            ALProbeSched::lightLeave(entry, serial, events);
            silent_events.clear();
            ALProbeSched::lightStep(entry, sample, false, serial, now, dt,
                                    mLightDebounceConfig, silent_events);
            entry.mSilent = true;
        }
        else
        {
            // A light re-created from the object cache after a cull is not an addition.
            const bool quiet = entry.mSilent || (is_new && ALProbeDirty::isCacheBorn(sample.mId));
            std::vector<ALProbeSched::Event>& out = quiet ? silent_events : events;
            if (quiet)
            {
                silent_events.clear();
            }
            ALProbeSched::lightStep(entry, sample, is_new, serial, now, dt,
                                    mLightDebounceConfig, out);
        }
    }

    // Sweep: lights that were removed or left the prefilter; deliver due settles.
    for (auto it = mLightSnap.begin(); it != mLightSnap.end();)
    {
        if (!it->second.mSeen)
        {
            // Silent entries, and lights the object cache culled, leave quietly.
            if (!it->second.mSilent && !ALProbeDirty::wasCulled(it->second.mId))
            {
                ALProbeSched::lightLeave(it->second, serial, events);
            }
            const U64 seq = it->second.mSeq;
            auto pos_it = std::lower_bound(mLightsBySeq.begin(), mLightsBySeq.end(), seq,
                [](const ALProbeSched::LightEntry* l, U64 s) { return l->mSeq < s; });
            if (pos_it != mLightsBySeq.end() && (*pos_it)->mSeq == seq)
            {
                mLightsBySeq.erase(pos_it);
            }
            it = mLightSnap.erase(it);
        }
        else
        {
            if (it->second.mSilent)
            {
                silent_events.clear();
                ALProbeSched::lightSettleDue(it->second, now, silent_events);
            }
            else
            {
                ALProbeSched::lightSettleDue(it->second, now, events);
            }
            ++it;
        }
    }

    // A discrete light event (add / remove / eligibility / STRUCT) marks the
    // probes it reaches (every probe, for a spot) for a cap-rule recount.
    for (size_t i = events_before; i < events.size(); ++i)
    {
        const ALProbeSched::Event& ev = events[i];
        if (!(ev.mClass & ALProbeSched::C_LIGHT) || (ev.mReason & ALProbeSched::R_SETTLE))
        {
            continue;
        }
        F32 c[3];
        F32 r = 0.f;
        ALProbeSched::boxBoundingSphere(ev.mMin, ev.mMax, c, r);
        for (auto& probe : mProbes)
        {
            if (probe->mSched.mOwner != ALProbeSched::Owner::ORDINARY || probe->mCubeIndex == -1)
            {
                continue;
            }
            const F32* o = probe->mOrigin.getF32ptr();
            const F32 dx = c[0] - o[0];
            const F32 dy = c[1] - o[1];
            const F32 dz = c[2] - o[2];
            const F32 reach = r + rcap;
            if (ev.mSpot || dx * dx + dy * dy + dz * dz <= reach * reach)
            {
                ALProbeSched::markRecountDue(probe->mSched);
            }
        }
    }
    mSchedWindow.mLightsProcessed = processed;
    mSchedWindow.mLightsTotal = total;
}

// The per-frame schedule flush (spec 4.6 step 2). OFF: nothing is recorded; a
// leftover ON period is discarded. ON: collect, hit, diff, barrier.
void LLReflectionMapManager::flushProbeSchedule()
{
    if (!mOnDemandActive)
    {
        if (mSchedWasOn || mBarrier.mActive || !mLightSnap.empty() || mOrdEnvValid)
        {
            resetProbeSchedule(true);
        }
        return;
    }
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("probe sched flush");
    const F64 now = static_cast<F64>(gFrameTimeSeconds);

    LLTimer flush_timer;
    const U64 serial = ++mSchedSerial;
    const F32 dt = static_cast<F32>(gFrameIntervalSeconds);
    ALProbeDirty::setFrame(serial, now, dt);

    static LLCachedControl<F32> min_interval_setting(gSavedSettings, "RenderProbeMinInterval", 1.f);
    static LLCachedControl<F32> max_age_setting(gSavedSettings, "RenderProbeMaxAge", 60.f);
    static LLCachedControl<F32> draw_distance_setting(gSavedSettings, "RenderReflectionProbeDrawDistance", 64.f);
    static LLCachedControl<S32> local_light_count_setting(gSavedSettings, "RenderLocalLightCount", 256);
    static LLCachedControl<S32> shadow_detail_setting(gSavedSettings, "RenderShadowDetail", 2);
    static LLCachedControl<F32> default_period_setting(gSavedSettings, "RenderDefaultProbeUpdatePeriod", 2.f);
    mSchedPolicy = ALProbeSched::policyFromSettings(static_cast<F32>(min_interval_setting),
                                                    static_cast<F32>(max_age_setting));
    const F32 rcap = static_cast<F32>(draw_distance_setting) * 1.7320508f + 20.f;
    const S32 light_cap = static_cast<S32>(local_light_count_setting);
    LLReflectionMap* live_probe = mCinematicLiveProbe.get();

    // a. Housekeeping: ids, owners, the ON edge.
    const bool on_edge = !mSchedWasOn;
    for (auto& probe_ptr : mProbes)
    {
        LLReflectionMap* probe = probe_ptr.get();
        ALProbeSched::Record& rec = probe->mSched;
        if (rec.mId == 0)
        {
            rec.mId = mSchedNextId++;
        }
        const ALProbeSched::Owner previous = rec.mOwner;
        ALProbeSched::Owner owner = ALProbeSched::Owner::ORDINARY;
        if (probe == live_probe)
        {
            owner = ALProbeSched::Owner::LIVE;
        }
        else if (mRtSliceLastRanId != 0 && rec.mId == mRtSliceLastRanId && !rec.mInTxn)
        {
            // (a probe with an active ordinary transaction stays ORDINARY, so its
            // events keep being attributed to that transaction -- first-frame
            // slicing may have coincided with its selection)
            owner = ALProbeSched::Owner::REALTIME;
        }
        rec.mOwner = owner;
        if (on_edge)
        {
            ALProbeSched::clearTxn(rec);
        }
        else if (previous == ALProbeSched::Owner::LIVE && owner == ALProbeSched::Owner::ORDINARY)
        {
            ALProbeSched::hit(rec, ALProbeSched::R_RESYNC, serial, now);
        }
        ALProbeSched::graceInit(rec, probe->mComplete, now);
    }
    if (on_edge)
    {
        for (auto& probe_ptr : mProbes)
        {
            if (probe_ptr->mSched.mOwner == ALProbeSched::Owner::ORDINARY && probe_ptr->mCubeIndex != -1)
            {
                ALProbeSched::hit(probe_ptr->mSched, ALProbeSched::R_RESYNC, serial, now);
            }
            // counts that went stale while OFF must not be trusted
            ALProbeSched::markRecountDue(probe_ptr->mSched);
        }
        ++mLiveSceneSerial;
        mOrdEnvValid = false;
        mRecount.reset();
    }

    // Arm a pending refresh-all: every relevant allocated ordinary / realtime probe.
    if (mRefreshAllRequested)
    {
        mRefreshAllRequested = false;
        std::vector<U32> ids;
        for (auto& probe_ptr : mProbes)
        {
            ALProbeSched::Record& rec = probe_ptr->mSched;
            if (rec.mOwner != ALProbeSched::Owner::LIVE && probe_ptr->mCubeIndex != -1 &&
                (probe_ptr == mDefaultProbe || probe_ptr->isRelevant()))
            {
                ids.push_back(rec.mId);
                ALProbeSched::hit(rec,
                    static_cast<U16>(ALProbeSched::R_RESYNC | ALProbeSched::R_BARRIER),
                    serial, now);
            }
        }
        ALProbeSched::Barrier::LiveState live_state = ALProbeSched::Barrier::LiveState::NONE;
        U32 live_id = 0;
        if (live_probe)
        {
            live_id = live_probe->mSched.mId;
            live_state = (live_probe->mCubeIndex != -1 && live_probe->isRelevant())
                ? ALProbeSched::Barrier::LiveState::PENDING
                : ALProbeSched::Barrier::LiveState::UNAVAILABLE;
        }
        ALProbeSched::barrierArm(mBarrier, serial, now, ids, live_state, live_id,
                                 ALProbeDirty::nextOp());
        ++mLiveSceneSerial;
    }

    // b. Collect: recorder events (incl. due motion settles), the light diff, the
    // environment generation.
    ALProbeDirty::drain(mDirtyDrain, now);
    mSchedEvents.assign(mDirtyDrain.mEvents.begin(), mDirtyDrain.mEvents.end());
    flushLightDiff(now, serial, dt, rcap, mSchedEvents);

    {
        static std::vector<ALProbeSched::RecountProbe> recount_probes;
        recount_probes.clear();
        const F32 max_dist = llmin(LLPipeline::RenderFarClip, static_cast<F32>(draw_distance_setting));
        for (auto& probe_ptr : mProbes)
        {
            if (probe_ptr->mSched.mOwner != ALProbeSched::Owner::ORDINARY || probe_ptr->mCubeIndex == -1)
            {
                continue;
            }
            ALProbeSched::RecountProbe rp;
            rp.mRec = &probe_ptr->mSched;
            probe_copy3(rp.mOrigin, probe_ptr->mOrigin.getF32ptr());
            rp.mMaxDist = max_dist;
            recount_probes.push_back(rp);
        }
        mSchedWindow.mRecountPairs += mRecount.run(recount_probes, mLightsBySeq,
            ALProbeSched::kRecountPairBudget, serial, light_cap);
    }

    {
        const U64 env_h = sampleOrdinaryEnvH();
        if (mOrdEnvValid && env_h != mOrdEnvH)
        {
            const F32 zero[3] = { 0.f, 0.f, 0.f };
            mSchedEvents.push_back(ALProbeSched::makeEvent(zero, zero, ALProbeSched::R_ENV,
                static_cast<U8>(ALProbeSched::C_GLOBAL), serial));
            ++mSchedWindow.mEnvChanges;
        }
        mOrdEnvH = env_h;
        mOrdEnvValid = true;
    }

    // E x P after filtering: only events that need a per-probe spatial test count.
    U32 spatial_events = 0;
    for (const ALProbeSched::Event& ev : mSchedEvents)
    {
        if (!(ev.mClass & (ALProbeSched::C_GLOBAL | ALProbeSched::C_TERRAIN_WATER)))
        {
            ++spatial_events;
        }
    }
    U32 ordinary_probes = 0;
    for (auto& probe_ptr : mProbes)
    {
        if (probe_ptr->mSched.mOwner == ALProbeSched::Owner::ORDINARY && probe_ptr->mCubeIndex != -1)
        {
            ++ordinary_probes;
        }
    }
    const bool overflow = mDirtyDrain.mOverflow || spatial_events * ordinary_probes > 16384u;

    // c. Hit test.
    if (overflow)
    {
        // Too much to attribute: everything is dirty. Never clears a transaction.
        for (auto& probe_ptr : mProbes)
        {
            if (probe_ptr->mSched.mOwner == ALProbeSched::Owner::ORDINARY && probe_ptr->mCubeIndex != -1)
            {
                ALProbeSched::hit(probe_ptr->mSched, ALProbeSched::R_RESYNC, serial, now);
            }
        }
        ++mLiveSceneSerial;
        ++mSchedWindow.mOverflow;
    }
    else if (!mSchedEvents.empty())
    {
        ALProbeSched::ShadowDirs shadows;
        shadows.mEnabled = static_cast<S32>(shadow_detail_setting) > 0;
        LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
        if (sky && shadows.mEnabled)
        {
            const LLVector3 sun = sky->getSunDirection();
            const LLVector3 moon = sky->getMoonDirection();
            shadows.mSunValid = sun.magVec() > 1e-4f;
            shadows.mMoonValid = moon.magVec() > 1e-4f;
            probe_copy3(shadows.mSun, sun.mV);
            probe_copy3(shadows.mMoon, moon.mV);
        }
        const U8 ordinary_mask = static_cast<U8>(ALProbeSched::C_STATIC | ALProbeSched::C_TERRAIN_WATER |
                                                 ALProbeSched::C_LIGHT | ALProbeSched::C_GLOBAL);
        const U8 default_mask = static_cast<U8>(ALProbeSched::C_TERRAIN_WATER | ALProbeSched::C_LIGHT |
                                                ALProbeSched::C_GLOBAL);
        for (auto& probe_ptr : mProbes)
        {
            ALProbeSched::Record& rec = probe_ptr->mSched;
            if (rec.mOwner != ALProbeSched::Owner::ORDINARY || probe_ptr->mCubeIndex == -1)
            {
                continue;
            }
            const U8 mask = (probe_ptr == mDefaultProbe) ? default_mask : ordinary_mask;
            const F32* origin = probe_ptr->mOrigin.getF32ptr();
            for (const ALProbeSched::Event& ev : mSchedEvents)
            {
                if (ALProbeSched::hitsCapture(ev, rec, origin, rcap, mask, light_cap, shadows))
                {
                    ALProbeSched::hit(rec, ev.mReason, serial, now);
                }
            }
        }

        // Live scene serial: static / terrain / global events and gobo arrivals
        // inside the Live footprint (pure light events are excluded: the light
        // tokens carry them). A blur-restricted event only counts when Live
        // rendered a face after the blur.
        if (live_probe && live_probe->mCubeIndex != -1)
        {
            ALProbeSched::Record live_rec;
            live_rec.mLastFaceOp = mLiveLastFaceOp;
            live_rec.mRecountDue = false;
            const F32* live_origin = live_probe->mOrigin.getF32ptr();
            const U8 live_scene_mask = static_cast<U8>(ALProbeSched::C_STATIC |
                ALProbeSched::C_TERRAIN_WATER | ALProbeSched::C_GLOBAL);
            const U8 live_light_mask = static_cast<U8>(ALProbeSched::C_LIGHT);
            for (const ALProbeSched::Event& ev : mSchedEvents)
            {
                bool bump = ALProbeSched::hitsCapture(ev, live_rec, live_origin, rcap,
                                                      live_scene_mask, light_cap, shadows);
                if (!bump && (ev.mReason & ALProbeSched::R_TEX))
                {
                    bump = ALProbeSched::hitsCapture(ev, live_rec, live_origin, rcap,
                                                     live_light_mask, light_cap, shadows);
                }
                if (bump)
                {
                    ++mLiveSceneSerial;
                    break; // at most once per frame
                }
            }
        }
    }

    // d. Ordinary probes: their own capture inputs; e. the default probe.
    LLVector4a camera_origin;
    camera_origin.load3(LLViewerCamera::instance().getOrigin().mV);
    const LLVector2 cloud_scroll = LLEnvironment::instance().getCloudScrollDelta();
    const F32 cloud[2] = { cloud_scroll.mV[0], cloud_scroll.mV[1] };
    for (auto& probe_ptr : mProbes)
    {
        LLReflectionMap* probe = probe_ptr.get();
        ALProbeSched::Record& rec = probe->mSched;
        if (rec.mOwner != ALProbeSched::Owner::ORDINARY || probe->mCubeIndex == -1 || !probe->mComplete)
        {
            continue;
        }
        if (probe == mDefaultProbe)
        {
            const U16 reasons = ALProbeSched::defaultProbeReasons(rec,
                camera_origin.getF32ptr(), cloud, now, static_cast<F32>(default_period_setting));
            if (reasons)
            {
                ALProbeSched::hit(rec, reasons, serial, now);
            }
            continue;
        }
        ALProbeSched::CaptureState cur;
        probe_copy3(cur.mOrigin, probe->mOrigin.getF32ptr());
        cur.mRadius = probe->mRadius;
        cur.mAmbiance = probe->getAmbiance();
        cur.mNear = probe->getNearClip();
        cur.mDynamic = probe->getIsDynamic();
        cur.mBox = probe->mViewerObject.notNull() && probe->mViewerObject->getReflectionProbeIsBox();
        if (ALProbeSched::probeChanged(rec, cur, probe->mCubeIndex))
        {
            ALProbeSched::hit(rec, ALProbeSched::R_PROBE, serial, now);
        }
    }

    // Barrier progress: members that vanished, lost their slot or became
    // irrelevant are dropped; the Live designation is tracked by identity.
    if (mBarrier.mActive)
    {
        for (const ALProbeSched::Barrier::Member& member : mBarrier.mMembers)
        {
            if (member.mState != ALProbeSched::Barrier::MState::PENDING)
            {
                continue;
            }
            LLReflectionMap* probe = findProbeById(member.mId);
            if (!probe || probe->mCubeIndex == -1 || !(probe == mDefaultProbe || probe->isRelevant()))
            {
                ALProbeSched::barrierMemberDropped(mBarrier, member.mId);
            }
        }
        ALProbeSched::barrierLiveDesignation(mBarrier, live_probe != nullptr,
            live_probe && live_probe->mCubeIndex != -1 && live_probe->isRelevant(),
            live_probe ? live_probe->mSched.mId : 0u, ALProbeDirty::nextOp());
        ALProbeSched::barrierTick(mBarrier, now, mPaused);
    }

    // Window statistics.
    mSchedWindow.mEvRaw += mDirtyDrain.mRaw;
    mSchedWindow.mEvMotion += mDirtyDrain.mMotionNotes;
    mSchedWindow.mEvSettles += mDirtyDrain.mSettles;
    mSchedWindow.mEvEarly += mDirtyDrain.mEarlySettles;
    mSchedWindow.mEvBulk += mDirtyDrain.mBulk;
    mSchedWindow.mEvBulkDeadline += mDirtyDrain.mBulkDeadlines;
    mSchedWindow.mDeb = mDirtyDrain.mPendingMotion;
    for (S32 i = 0; i < ALProbeDirty::TAG_COUNT; ++i)
    {
        mSchedWindow.mDropped[i] += mDirtyDrain.mDropped[i];
    }
    mSchedWindow.mDropDyn += mDirtyDrain.mDroppedDyn;
    mSchedWindow.mNoopMove += mDirtyDrain.mNoopMove;
    mSchedWindow.mRebal += mDirtyDrain.mRebal;
    for (const ALProbeSched::Event& ev : mSchedEvents)
    {
        if ((ev.mClass & ALProbeSched::C_LIGHT) && !(ev.mReason & ALProbeSched::R_SETTLE))
        {
            ++mSchedWindow.mEvLights;
        }
    }

    mSchedWasOn = true;
    const F64 flush_us = static_cast<F64>(flush_timer.getElapsedTimeF64()) * 1.0e6;
    mSchedWindow.mFlushUsSum += flush_us;
    mSchedWindow.mFlushUsMax = std::max(mSchedWindow.mFlushUsMax, flush_us);
    ++mSchedWindow.mFlushSamples;
}

// Closest dynamic probe, on-demand ON, no Live Probe: N faces per frame of one
// irradiance or radiance pass through the secondary scratch (the probe is a
// REALTIME-owned record; the ordinary queue never touches it). The pass kind
// alternates; a pass starts at face 0 with the origin frozen for its six faces.
void LLReflectionMapManager::updateRealtimeSliced(LLReflectionMap* probe, S32 faces)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - rt sliced");
    LL_PROFILE_ZONE_NUM(faces);
    if (probe == mUpdatingProbe)
    {
        // The ordinary capture owns the primary scratch for this probe: skip the
        // frame, keep the cursor.
        ++mSchedWindow.mRtBlocked;
        return;
    }
    const U32 id = schedId(probe);
    ALProbeSched::sliceBind(mRtSlice, id, probe->mCubeIndex);
    mRtSliceRanThisFrame = id;
    probe->autoAdjustOrigin();
    const bool saved_radiance_pass = isRadiancePass();
    for (S32 n = 0; n < faces; ++n)
    {
        ALProbeSched::sliceBegin(mRtSlice, mSchedSerial);
        if (mRtSlice.mFace == 0)
        {
            mRtSliceFrozenOrigin = probe->mOrigin; // freeze for the pass
        }
        const LLVector4a live_origin = probe->mOrigin;
        probe->mOrigin = mRtSliceFrozenOrigin;
        mRadiancePass = mRtSlice.mRadiance;
        updateProbeFace(probe, static_cast<U32>(mRtSlice.mFace));
        probe->mOrigin = live_origin; // the influence volume keeps following
        probe->mSched.mLastFaceOp = ALProbeDirty::nextOp();
        ++mSchedFrame.mRtFaces;
        ++mSchedFrame.mSlicedFaces;
        const ALCineLiveProbeRefresh::PassEnd end = ALProbeSched::sliceAdvance(mRtSlice);
        if (end != ALCineLiveProbeRefresh::PassEnd::NONE)
        {
            updateNeighbors(probe); // after any pass end
            ALProbeSched::barrierRealtimePass(mBarrier, id,
                end == ALCineLiveProbeRefresh::PassEnd::RADIANCE, mRtSlice.mPassStartSerial);
            break; // a pass never spans two frames' budgets
        }
    }
    mRadiancePass = saved_radiance_pass;
}

// [ProbeSched] one line per 5 s of manager frames (setting-gated; on and off paths).
void LLReflectionMapManager::logProbeSchedule(F64 now)
{
    static LLCachedControl<bool> sched_log(gSavedSettings, "RenderProbeSchedLog", false);
    SchedWindow& w = mSchedWindow;
    if (!sched_log)
    {
        if (w.mStart >= 0.0 || w.mFrames != 0 || w.mStarts != 0)
        {
            w = SchedWindow();
        }
        return;
    }
    // fold this frame
    ++w.mFrames;
    if (mPaused)
    {
        ++w.mPausedFrames;
    }
    w.mOrdFaces += mSchedFrame.mOrdFaces;
    w.mRtFaces += mSchedFrame.mRtFaces;
    static LLCachedControl<S32> slice_faces(gSavedSettings, "RenderProbeRealtimeFacesPerFrame", 2);
    if (ALProbeSched::frameOverBudget(mSchedFrame, llclamp(static_cast<S32>(slice_faces), 1, 6)))
    {
        w.mOverBudget = true;
    }
    // A gap since the last logged frame restarts the window.
    const bool gap = w.mLastFrame >= 0.0 && now - w.mLastFrame > 0.5;
    w.mLastFrame = now;
    if (w.mStart < 0.0 || gap)
    {
        w = SchedWindow();
        w.mStart = now;
        w.mLastFrame = now;
        ++mSchedWindowIndex;
        return;
    }
    if (now - w.mStart < 5.0)
    {
        return;
    }

    // End of the window: the probe-state snapshot.
    U32 dirty_count = 0;
    U32 deferred = 0;
    U32 waiting = 0;
    U32 recount_due = 0;
    F64 max_lag = 0.0;
    U32 starved = 0;
    F64 starved_worst = 0.0;
    S32 probes = 0;
    const F64 frame_now = static_cast<F64>(gFrameTimeSeconds);
    for (const auto& probe : mProbes)
    {
        const ALProbeSched::Record& rec = probe->mSched;
        if (!mOnDemandActive || rec.mOwner != ALProbeSched::Owner::ORDINARY || probe->mCubeIndex == -1)
        {
            continue;
        }
        ++probes;
        if (rec.mRecountDue)
        {
            ++recount_due;
        }
        const ALProbeSched::Why why = ALProbeSched::evaluate(rec,
            ALProbeSched::View{ probe->mComplete, probe->mOccluded, probe->getIsDynamic(), true,
                                ALProbeSched::barrierIsMember(mBarrier, rec.mId) },
            mSchedPolicy, frame_now);
        if (ALProbeSched::dirty(rec))
        {
            ++dirty_count;
            const F64 lag = rec.mFirstDirty >= 0.0 ? frame_now - rec.mFirstDirty : 0.0;
            max_lag = std::max(max_lag, lag);
            if (lag > ALProbeSched::kStarveSec)
            {
                ++starved;
                starved_worst = std::max(starved_worst, lag);
            }
        }
        if (why == ALProbeSched::Why::DEFERRED)
        {
            ++deferred;
        }
        else if (why == ALProbeSched::Why::WAIT_INTERVAL)
        {
            ++waiting;
        }
    }
    ALProbeSched::VerdictInput verdict_in;
    verdict_in.mOn = mOnDemandActive;
    verdict_in.mOverBudget = w.mOverBudget;
    verdict_in.mPaused = w.mFrames > 0 && 2 * (w.mEarlyFrames + w.mPausedFrames) > w.mFrames;
    verdict_in.mUnsettled = w.mUnsettled;
    verdict_in.mUnsettledId = w.mUnsettledId;
    verdict_in.mUnsettledReasons = w.mUnsettledReasons;
    verdict_in.mStarvedCount = starved;
    verdict_in.mStarvedWorst = starved_worst;

    const F64 elapsed = std::max(0.001, now - w.mStart);
    const F64 flush_avg = w.mFlushSamples > 0 ? w.mFlushUsSum / static_cast<F64>(w.mFlushSamples) : 0.0;
    std::string barrier_text = "idle";
    if (mBarrier.mActive)
    {
        barrier_text = "armed " +
            std::to_string(ALProbeSched::barrierCount(mBarrier, ALProbeSched::Barrier::MState::DONE)) + "/" +
            std::to_string(ALProbeSched::barrierTotal(mBarrier));
    }
    LL_INFOS("ProbeSched") << llformat(
        "[ProbeSched] mode=%s probes=%d faces=%.1f/s rt=%.1f/s starts=%u done=%u nack=%u "
        "g=%u t=%u l=%u e=%u p=%u s=%u c=%u r=%u w=%u d=%u z=%u b=%u "
        "events raw=%u nl=%u motion=%u settles=%u early_settles=%u bulk=%u bulk_deadline=%u "
        "deb=%u recount_pairs=%u/%u recount_due=%u "
        "drop_lodvol=%u drop_lodmesh=%u drop_lodterr=%u drop_lodtree=%u drop_lodgrass=%u "
        "drop_texanim=%u drop_shift=%u drop_vocache=%u drop_dyn=%u noop_move=%u rebal=%u overflow=%u "
        "dirty=%u deferred=%u wait=%u maxlag=%.1f rt_blocked=%u env_changes=%u "
        "flush_us=%.1f/%.1f lights=%u/%u barrier=%s verdict=%s",
        mOnDemandActive ? "on" : "off", probes,
        static_cast<F64>(w.mOrdFaces) / elapsed, static_cast<F64>(w.mRtFaces) / elapsed,
        w.mStarts, w.mDone, w.mNack,
        w.mStartReason[0], w.mStartReason[1], w.mStartReason[2], w.mStartReason[3],
        w.mStartReason[4], w.mStartReason[5], w.mStartReason[6], w.mStartReason[7],
        w.mStartReason[8], w.mStartReason[9], w.mStartReason[10], w.mStartReason[11],
        w.mEvRaw, w.mEvLights, w.mEvMotion, w.mEvSettles, w.mEvEarly, w.mEvBulk, w.mEvBulkDeadline,
        w.mDeb, w.mRecountPairs, ALProbeSched::kRecountPairBudget, recount_due,
        w.mDropped[ALProbeDirty::TAG_LOD_VOLUME], w.mDropped[ALProbeDirty::TAG_LOD_MESH],
        w.mDropped[ALProbeDirty::TAG_LOD_TERRAIN], w.mDropped[ALProbeDirty::TAG_LOD_TREE],
        w.mDropped[ALProbeDirty::TAG_LOD_GRASS], w.mDropped[ALProbeDirty::TAG_TEXANIM_TOGGLE],
        w.mDropped[ALProbeDirty::TAG_REGION_SHIFT], w.mDropped[ALProbeDirty::TAG_VOCACHE],
        w.mDropDyn, w.mNoopMove, w.mRebal, w.mOverflow,
        dirty_count, deferred, waiting, max_lag, w.mRtBlocked, w.mEnvChanges,
        flush_avg, w.mFlushUsMax, w.mLightsProcessed, w.mLightsTotal,
        barrier_text.c_str(), ALProbeSched::verdictText(verdict_in).c_str())
        << LL_ENDL;
    w = SchedWindow();
    w.mStart = now;
    w.mLastFrame = now;
    ++mSchedWindowIndex;
}


void LLReflectionMapManager::refreshSettings()
{
    mRenderReflectionProbeDetail = gSavedSettings.getS32("RenderReflectionProbeDetail");
    mRenderReflectionProbeLevel = gSavedSettings.getS32("RenderReflectionProbeLevel");
    mRenderReflectionProbeCount = gSavedSettings.getU32("RenderReflectionProbeCount");
    mRenderReflectionProbeDynamicAllocation = gSavedSettings.getS32("RenderReflectionProbeDynamicAllocation");
}

LLReflectionMap* LLReflectionMapManager::addProbe(LLSpatialGroup* group)
{
    if (gGLManager.mGLVersion < 4.05f || !LLPipeline::sReflectionProbesEnabled)
    {
        return nullptr;
    }

    LLReflectionMap* probe = new LLReflectionMap();
    probe->mGroup = group;

    if (mDefaultProbe.isNull())
    {  //safety check to make sure default probe is always first probe added
        mDefaultProbe = new LLReflectionMap();
        mProbes.push_back(mDefaultProbe);
    }

    llassert(mProbes[0] == mDefaultProbe);

    if (group)
    {
        probe->mOrigin = group->getOctreeNode()->getCenter();
    }

    if (gCubeSnapshot)
    { //snapshot is in progress, mProbes is being iterated over, defer insertion until next update
        mCreateList.push_back(probe);
    }
    else
    {
        mProbes.push_back(probe);
    }

    return probe;
}

U32 LLReflectionMapManager::probeCount()
{
    return mDynamicProbeCount;
}

U32 LLReflectionMapManager::probeMemory()
{
    return (mDynamicProbeCount * 6 * (mProbeResolution * mProbeResolution) * 4) / 1024 / 1024 + (mDynamicProbeCount * 6 * (mIrradianceMapResolution * mIrradianceMapResolution) * 4) / 1024 / 1024;
}

struct CompareProbeDepth
{
    bool operator()(const LLReflectionMap* lhs, const LLReflectionMap* rhs)
    {
        return lhs->mMinDepth < rhs->mMinDepth;
    }
};

void LLReflectionMapManager::getReflectionMaps(std::vector<LLReflectionMap*>& maps)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    LLMatrix4a modelview;
    modelview.loadu(gGLModelView);
    LLVector4a oa; // scratch space for transformed origin

    U32 count = 0;
    U32 lastIdx = 0;
    for (U32 i = 0; count < maps.size() && i < mProbes.size(); ++i)
    {
        mProbes[i]->mLastBindTime = gFrameTimeSeconds; // something wants to use this probe, indicate it's been requested
        if (mProbes[i]->mCubeIndex != -1)
        {
            if (!mProbes[i]->mOccluded && mProbes[i]->mComplete)
            {
                maps[count++] = mProbes[i];
                modelview.affineTransform(mProbes[i]->mOrigin, oa);
                mProbes[i]->mMinDepth = -oa.getF32ptr()[2] - mProbes[i]->mRadius;
                mProbes[i]->mMaxDepth = -oa.getF32ptr()[2] + mProbes[i]->mRadius;
            }
        }
        else
        {
            mProbes[i]->mProbeIndex = -1;
        }
        lastIdx = i;
    }

    // set remaining probe indices to -1
    for (U32 i = lastIdx+1; i < mProbes.size(); ++i)
    {
        mProbes[i]->mProbeIndex = -1;
    }

    if (count > 1)
    {
        std::sort(maps.begin(), maps.begin() + count, CompareProbeDepth());
    }

    for (U32 i = 0; i < count; ++i)
    {
        maps[i]->mProbeIndex = i;
    }

    // null terminate list
    if (count < maps.size())
    {
        maps[count] = nullptr;
    }
}

LLReflectionMap* LLReflectionMapManager::registerSpatialGroup(LLSpatialGroup* group)
{
    if (!group)
    {
        return nullptr;
    }
    LLSpatialPartition* part = group->getSpatialPartition();
    if (!part || part->mPartitionType != LLViewerRegion::PARTITION_VOLUME)
    {
        return nullptr;
    }
    OctreeNode* node = group->getOctreeNode();
    F32 size = node->getSize().getF32ptr()[0];
    if (size < 15.f || size > 17.f)
    {
        return nullptr;
    }
    return addProbe(group);
}

LLReflectionMap* LLReflectionMapManager::registerViewerObject(LLViewerObject* vobj)
{
    if (!LLPipeline::sReflectionProbesEnabled)
    {
        return nullptr;
    }

    llassert(vobj != nullptr);

    LLReflectionMap* probe = new LLReflectionMap();
    probe->mViewerObject = vobj;
    probe->mOrigin.load3(vobj->getPositionAgent().mV);

    if (gCubeSnapshot)
    { //snapshot is in progress, mProbes is being iterated over, defer insertion until next update
        mCreateList.push_back(probe);
    }
    else
    {
        mProbes.push_back(probe);
    }

    return probe;
}

void LLReflectionMapManager::setCinematicLiveProbe(
    LLReflectionMap* probe, const std::vector<LLUUID>& ignored_light_ids,
    const std::vector<LLUUID>& pinned_light_ids)
{
    if (mCinematicLiveProbe.get() != probe)
    {
        mCinematicLiveProbe = probe;
        mCinematicIrradianceReady = false;
        mCinematicRadianceReady = false;
        mRealtimeRadiancePass = false;
        resetCinematicRefresh(); // [LiveProbeRefresh]
        if (probe)
        {
            probe->mComplete = false;
            probe->mFadeIn = 0.f;
        }
    }
    mCinematicIgnoredLightIds = ignored_light_ids;
    mCinematicPinnedLightIds = pinned_light_ids;
    if (!probe)
    {
        mCinematicLiveProbeCapture = false;
    }
}

bool LLReflectionMapManager::isCinematicLiveProbeIgnoredLight(
    const LLUUID& id) const
{
    return mCinematicLiveProbeCapture &&
        std::find(mCinematicIgnoredLightIds.begin(),
                  mCinematicIgnoredLightIds.end(), id) !=
            mCinematicIgnoredLightIds.end();
}

bool LLReflectionMapManager::isCinematicLiveProbePinnedLight(
    const LLUUID& id) const
{
    return mCinematicLiveProbeCapture &&
        std::find(mCinematicPinnedLightIds.begin(),
                  mCinematicPinnedLightIds.end(), id) !=
            mCinematicPinnedLightIds.end();
}

S32 LLReflectionMapManager::allocateCubeIndex()
{
    if (!mCubeFree.empty())
    {
        S32 ret = mCubeFree.front();
        mCubeFree.pop_front();
        return ret;
    }

    return -1;
}

void LLReflectionMapManager::deleteProbe(U32 i)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LLReflectionMap* probe = mProbes[i];

    llassert(probe != mDefaultProbe);

    if (probe->mCubeIndex != -1)
    { // mark the cube index used by this probe as being free
        mCubeFree.push_back(probe->mCubeIndex);
    }
    if (mUpdatingProbe == probe)
    {
        mUpdatingProbe = nullptr;
        mUpdatingFace = 0;
    }

    // remove from any Neighbors lists
    for (auto& other : probe->mNeighbors)
    {
        auto const & iter = std::find(other->mNeighbors.begin(), other->mNeighbors.end(), probe);
        llassert(iter != other->mNeighbors.end());
        other->mNeighbors.erase(iter);
    }

    mProbes.erase(mProbes.begin() + i);
}


void LLReflectionMapManager::doProbeUpdate()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(mUpdatingProbe != nullptr);

    updateProbeFace(mUpdatingProbe, mUpdatingFace);
    if (mSchedStats)
    {
        ++mSchedFrame.mOrdFaces; // [ProbeOnDemand] call-site counter
    }
    if (mOnDemandActive)
    {
        // The face has been rendered: a later texture downscale gets a larger op.
        mUpdatingProbe->mSched.mLastFaceOp = ALProbeDirty::nextOp();
    }

    bool debug_updates = gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PROBE_UPDATES) && mUpdatingProbe->mViewerObject;

    if (++mUpdatingFace == 6)
    {
        if (debug_updates)
        {
            if (mOnDemandActive)
            {
                // [ProbeOnDemand] time + the reasons of this refresh (G T L E P S C R W D Z B)
                const ALProbeSched::Record& dbg_rec = mUpdatingProbe->mSched;
                mUpdatingProbe->mViewerObject->setDebugText(llformat("%.1f %s", (F32)gFrameTimeSeconds,
                    ALProbeSched::reasonLetters(dbg_rec.mInTxn ? dbg_rec.mTxnReasons : dbg_rec.mLastReasons).c_str()),
                    LLColor4(1, 1, 1, 1));
            }
            else
            {
                mUpdatingProbe->mViewerObject->setDebugText(llformat("%.1f", (F32)gFrameTimeSeconds), LLColor4(1, 1, 1, 1));
            }
        }
        updateNeighbors(mUpdatingProbe);
        mUpdatingFace = 0;
        if (isRadiancePass())
        {
            if (mOnDemandActive)
            {
                // [ProbeOnDemand] Radiance end: ack the transaction only when its
                // irradiance pass completed in the same epoch / cube; a nack keeps
                // the probe dirty (R_RESYNC). A barrier member is done on an ack of
                // a transaction that started after arming.
                ALProbeSched::Record& rec = mUpdatingProbe->mSched;
                const U64 txn_serial = rec.mTxnSerial;
                if (ALProbeSched::onTxnComplete(rec, mSchedEpoch, mUpdatingProbe->mCubeIndex,
                        static_cast<F64>(gFrameTimeSeconds), mSchedSerial))
                {
                    ++mSchedWindow.mDone;
                    ALProbeSched::barrierMemberAck(mBarrier, rec.mId, txn_serial);
                }
                else
                {
                    ++mSchedWindow.mNack;
                }
            }
            mUpdatingProbe->mComplete = true;
            mUpdatingProbe = nullptr;
            mRadiancePass = false;
        }
        else
        {
            if (mOnDemandActive)
            {
                ALProbeSched::onTxnIrradianceDone(mUpdatingProbe->mSched, mSchedEpoch); // [ProbeOnDemand]
            }
            mRadiancePass = true;
        }
    }
    else if (debug_updates)
    {
        mUpdatingProbe->mViewerObject->setDebugText(llformat("%.1f", (F32)gFrameTimeSeconds), LLColor4(1, 1, 0, 1));
    }
}

// Do the reflection map update render passes.
// For every 12 calls of this function, one complete reflection probe radiance map and irradiance map is generated
// First six passes render the scene with direct lighting only into a scratch space cube map at the end of the cube map array and generate
// a simple mip chain (not convolution filter).
// At the end of these passes, an irradiance map is generated for this probe and placed into the irradiance cube map array at the index for this probe
// The next six passes render the scene with both radiance and irradiance into the same scratch space cube map and generate a simple mip chain.
// At the end of these passes, a radiance map is generated for this probe and placed into the radiance cube map array at the index for this probe.
// In effect this simulates single-bounce lighting.
void LLReflectionMapManager::updateProbeFace(
    LLReflectionMap* probe, U32 face, bool force_dynamic)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("probe update");
    // [ProbeOnDemand] the capture-context scopes below only matter while on-demand is
    // active or the tiny-group cull is enabled
    static LLCachedControl<F32> tiny_cull_px(gSavedSettings, "RenderProbeTinyCullPixels", 0.f);
    const bool capture_context = mOnDemandActive || static_cast<F32>(tiny_cull_px) > 0.f;
    // hacky hot-swap of camera specific render targets
    gPipeline.mRT = &gPipeline.mAuxillaryRT;

    mLightScale = 1.f;
    static LLCachedControl<F32> max_local_light_ambiance(gSavedSettings, "RenderReflectionProbeMaxLocalLightAmbiance", 8.f);
    if (!isRadiancePass() && probe->getAmbiance() > max_local_light_ambiance)
    {
        mLightScale = max_local_light_ambiance / probe->getAmbiance();
    }

    if (probe == mDefaultProbe)
    {
        touch_default_probe(probe);

        gPipeline.pushRenderTypeMask();

        //only render sky, water, terrain, and clouds
        gPipeline.andRenderTypeMask(LLPipeline::RENDER_TYPE_SKY, LLPipeline::RENDER_TYPE_WL_SKY,
            LLPipeline::RENDER_TYPE_WATER, LLPipeline::RENDER_TYPE_VOIDWATER, LLPipeline::RENDER_TYPE_CLOUDS, LLPipeline::RENDER_TYPE_TERRAIN, LLPipeline::END_RENDER_TYPES);

        {
            // [ProbeOnDemand] capture context (tiny cull kind + probe-centric lights)
            ProbeCaptureScope capture_scope(*this, capture_context, mOnDemandActive && !mCinematicLiveProbeCapture,
                mCinematicLiveProbeCapture ? ProbeCaptureKind::LIVE
                    : (probe != mUpdatingProbe ? ProbeCaptureKind::REALTIME : ProbeCaptureKind::ORDINARY));
            probe->update(mRenderTarget.getWidth(), face, force_dynamic);
        }

        gPipeline.popRenderTypeMask();
    }
    else
    {
        llassert(mRenderReflectionProbeLevel > 0); // should never update a probe that's not the default probe if reflection coverage is none
        // [ProbeOnDemand] capture context (tiny cull kind + probe-centric lights)
        ProbeCaptureScope capture_scope(*this, capture_context, mOnDemandActive && !mCinematicLiveProbeCapture,
            mCinematicLiveProbeCapture ? ProbeCaptureKind::LIVE
                : (probe != mUpdatingProbe ? ProbeCaptureKind::REALTIME : ProbeCaptureKind::ORDINARY));
        probe->update(mRenderTarget.getWidth(), face, force_dynamic);
    }

    gPipeline.mRT = &gPipeline.mMainRT;

    S32 sourceIdx = mReflectionProbeCount;

    if (probe != mUpdatingProbe)
    { // this is the "realtime" probe that's updating every frame, use the secondary scratch space channel
        sourceIdx += 1;
    }

    gGL.setColorMask(true, true);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    LLGLDisable cull(GL_CULL_FACE);
    LLGLDisable blend(GL_BLEND);

    // downsample to placeholder map
    {
        gGL.matrixMode(gGL.MM_MODELVIEW);
        gGL.pushMatrix();
        gGL.loadIdentity();

        gGL.matrixMode(gGL.MM_PROJECTION);
        gGL.pushMatrix();
        gGL.loadIdentity();

        gGL.flush();
        U32 res = mProbeResolution * 2;

        static LLStaticHashedString resScale("resScale");
        static LLStaticHashedString direction("direction");
        static LLStaticHashedString znear("znear");
        static LLStaticHashedString zfar("zfar");

        LLRenderTarget* screen_rt = &gPipeline.mAuxillaryRT.screen;

        // perform a gaussian blur on the super sampled render before downsampling
        {
            gGaussianProgram.bind();
            gGaussianProgram.uniform1f(resScale, 1.f / (mProbeResolution * 2));
            S32 diffuseChannel = gGaussianProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, LLTexUnit::TT_TEXTURE);

            // horizontal
            gGaussianProgram.uniform2f(direction, 1.f, 0.f);
            gGL.getTexUnit(diffuseChannel)->bind(screen_rt);
            mRenderTarget.bindTarget();
            gPipeline.mScreenTriangleVB->setBuffer();
            gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            mRenderTarget.flush();

            // vertical
            gGaussianProgram.uniform2f(direction, 0.f, 1.f);
            gGL.getTexUnit(diffuseChannel)->bind(&mRenderTarget);
            screen_rt->bindTarget();
            gPipeline.mScreenTriangleVB->setBuffer();
            gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            screen_rt->flush();
        }


        S32 mips = (S32)(log2((F32)mProbeResolution) + 0.5f);

        gReflectionMipProgram.bind();
        S32 diffuseChannel = gReflectionMipProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, LLTexUnit::TT_TEXTURE);

        for (int i = 0; i < mMipChain.size(); ++i)
        {
            LL_PROFILE_GPU_ZONE("probe mip");
            mMipChain[i].bindTarget();
            if (i == 0)
            {
                gGL.getTexUnit(diffuseChannel)->bind(screen_rt);
            }
            else
            {
                gGL.getTexUnit(diffuseChannel)->bind(&(mMipChain[i - 1]));
            }


            gReflectionMipProgram.uniform1f(resScale, 1.f/(mProbeResolution*2));

            gPipeline.mScreenTriangleVB->setBuffer();
            gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            res /= 2;

            GLint mip = i - (static_cast<GLint>(mMipChain.size()) - mips);

            if (mip >= 0)
            {
                LL_PROFILE_GPU_ZONE("probe mip copy");
                mTexture->bind(0);
                //glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, mip, 0, 0, probe->mCubeIndex * 6 + face, 0, 0, res, res);
                glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, mip, 0, 0, sourceIdx * 6 + face, 0, 0, res, res);
                //if (i == 0)
                //{
                    //glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, mip, 0, 0, probe->mCubeIndex * 6 + face, 0, 0, res, res);
                //}
                mTexture->unbind();
            }
            mMipChain[i].flush();
        }

        gGL.popMatrix();
        gGL.matrixMode(gGL.MM_MODELVIEW);
        gGL.popMatrix();

        gGL.getTexUnit(diffuseChannel)->unbind(LLTexUnit::TT_TEXTURE);
        gReflectionMipProgram.unbind();
    }

    if (face == 5)
    {
        mMipChain[0].bindTarget();
        static LLStaticHashedString sSourceIdx("sourceIdx");

        if (isRadiancePass())
        {
            //generate radiance map (even if this is not the irradiance map, we need the mip chain for the irradiance map)
            gRadianceGenProgram.bind();
            mVertexBuffer->setBuffer();

            S32 channel = gRadianceGenProgram.enableTexture(LLShaderMgr::REFLECTION_PROBES, LLTexUnit::TT_CUBE_MAP_ARRAY);
            mTexture->bind(channel);
            gRadianceGenProgram.uniform1i(sSourceIdx, sourceIdx);
            gRadianceGenProgram.uniform1f(LLShaderMgr::REFLECTION_PROBE_MAX_LOD, mMaxProbeLOD);
            gRadianceGenProgram.uniform1f(LLShaderMgr::REFLECTION_PROBE_STRENGTH, 1.f);

            U32 res = mMipChain[0].getWidth();

            for (int i = 0; i < mMipChain.size(); ++i)
            {
                LL_PROFILE_GPU_ZONE("probe radiance gen");
                static LLStaticHashedString sMipLevel("mipLevel");
                static LLStaticHashedString sRoughness("roughness");
                static LLStaticHashedString sWidth("u_width");

                gRadianceGenProgram.uniform1f(sRoughness, (F32)i / (F32)(mMipChain.size() - 1));
                gRadianceGenProgram.uniform1f(sMipLevel, (GLfloat)i);
                gRadianceGenProgram.uniform1i(sWidth, mProbeResolution);

                for (int cf = 0; cf < 6; ++cf)
                { // for each cube face
                    LLCoordFrame frame;
                    frame.lookAt(LLVector3(0, 0, 0), LLCubeMapArray::sClipToCubeLookVecs[cf], LLCubeMapArray::sClipToCubeUpVecs[cf]);

                    F32 mat[16];
                    frame.getOpenGLRotation(mat);
                    gGL.loadMatrix(mat);

                    mVertexBuffer->drawArrays(gGL.TRIANGLE_STRIP, 0, 4);

                    glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, i, 0, 0, probe->mCubeIndex * 6 + cf, 0, 0, res, res);
                }

                if (i != mMipChain.size() - 1)
                {
                    res /= 2;
                    glViewport(0, 0, res, res);
                }
            }

            gRadianceGenProgram.unbind();
        }
        else
        {
            //generate irradiance map
            gIrradianceGenProgram.bind();
            S32 channel = gIrradianceGenProgram.enableTexture(LLShaderMgr::REFLECTION_PROBES, LLTexUnit::TT_CUBE_MAP_ARRAY);
            mTexture->bind(channel);

            gIrradianceGenProgram.uniform1i(sSourceIdx, sourceIdx);
            gIrradianceGenProgram.uniform1f(LLShaderMgr::REFLECTION_PROBE_MAX_LOD, mMaxProbeLOD);

            mVertexBuffer->setBuffer();
            int start_mip = 0;
            // find the mip target to start with based on irradiance map resolution
            for (start_mip = 0; start_mip < mMipChain.size(); ++start_mip)
            {
                if (mMipChain[start_mip].getWidth() == mIrradianceMapResolution)
                {
                    break;
                }
            }

            //for (int i = start_mip; i < mMipChain.size(); ++i)
            {
                int i = start_mip;
                LL_PROFILE_GPU_ZONE("probe irradiance gen");
                glViewport(0, 0, mMipChain[i].getWidth(), mMipChain[i].getHeight());
                for (int cf = 0; cf < 6; ++cf)
                { // for each cube face
                    LLCoordFrame frame;
                    frame.lookAt(LLVector3(0, 0, 0), LLCubeMapArray::sClipToCubeLookVecs[cf], LLCubeMapArray::sClipToCubeUpVecs[cf]);

                    F32 mat[16];
                    frame.getOpenGLRotation(mat);
                    gGL.loadMatrix(mat);

                    mVertexBuffer->drawArrays(gGL.TRIANGLE_STRIP, 0, 4);

                    S32 res = mMipChain[i].getWidth();
                    mIrradianceMaps->bind(channel);
                    glCopyTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, i - start_mip, 0, 0, probe->mCubeIndex * 6 + cf, 0, 0, res, res);
                    mTexture->bind(channel);
                }
            }

            gIrradianceGenProgram.unbind();
        }

        mMipChain[0].flush();
    }
}

void LLReflectionMapManager::reset()
{
    mReset = true;
}

void LLReflectionMapManager::pause(F32 duration)
{
    mPaused = true;
    mResumeTime = gFrameTimeSeconds + duration;
}

void LLReflectionMapManager::resume()
{
    mPaused = false;
}

void LLReflectionMapManager::shift(const LLVector4a& offset)
{
    for (auto& probe : mProbes)
    {
        probe->mOrigin.add(offset);
    }
    // [LiveProbeRefresh] A budget pass in flight captures about the frozen
    // origin; keep it in the same coordinate frame as the shifted probe.
    mCineFrozenOrigin.add(offset);

    // [ProbeOnDemand] SH: a region crossing moves the world, not the scene. Queued
    // events, debounce bounds, every light entry (position, accumulators and the
    // stored hashes, so a crossing is not a light event), the capture-start state
    // and the sliced probe's frozen origin move with it. No overflow, no resync.
    if (mOnDemandActive || mSchedWasOn)
    {
        const F32* o = offset.getF32ptr();
        const F32 off[3] = { o[0], o[1], o[2] };
        ALProbeDirty::shiftQueued(offset);
        for (auto& kv : mLightSnap)
        {
            ALProbeSched::lightShift(kv.second, off);
        }
        for (auto& probe : mProbes)
        {
            ALProbeSched::Record& rec = probe->mSched;
            for (S32 i = 0; i < 3; ++i)
            {
                rec.mStartOrigin[i] += off[i];
                rec.mStartCam[i] += off[i];
            }
        }
        mRtSliceFrozenOrigin.add(offset);
    }
}

void LLReflectionMapManager::updateNeighbors(LLReflectionMap* probe)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    if (mDefaultProbe == probe)
    {
        return;
    }

    //remove from existing neighbors
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmun - clear");

        for (auto& other : probe->mNeighbors)
        {
            auto const & iter = std::find(other->mNeighbors.begin(), other->mNeighbors.end(), probe);
            llassert(iter != other->mNeighbors.end()); // <--- bug davep if this ever happens, something broke badly
            other->mNeighbors.erase(iter);
        }

        probe->mNeighbors.clear();
    }

    // search for new neighbors
    if (probe->isRelevant())
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmun - search");
        for (auto& other : mProbes)
        {
            if (other != mDefaultProbe && other != probe)
            {
                if (other->isRelevant() && probe->intersects(other))
                {
                    probe->mNeighbors.push_back(other);
                    other->mNeighbors.push_back(probe);
                }
            }
        }
    }
}

void LLReflectionMapManager::updateUniforms()
{
    if (!LLPipeline::sReflectionProbesEnabled)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("rmmu - uniforms")


    mReflectionMaps.resize(mReflectionProbeCount);
    getReflectionMaps(mReflectionMaps);

    F32 minDepth[256];

    for (int i = 0; i < 256; ++i)
    {
        mProbeData.refBucket[i][0] = mReflectionProbeCount;
        mProbeData.refBucket[i][1] = mReflectionProbeCount;
        mProbeData.refBucket[i][2] = mReflectionProbeCount;
        mProbeData.refBucket[i][3] = mReflectionProbeCount;
        minDepth[i] = FLT_MAX;
    }

    // load modelview matrix into matrix 4a
    LLMatrix4a modelview;
    modelview.loadu(gGLModelView);
    LLVector4a oa; // scratch space for transformed origin

    S32 count = 0;
    U32 nc = 0; // neighbor "cursor" - index into refNeighbor to start writing the next probe's list of neighbors

    LLEnvironment& environment = LLEnvironment::instance();
    LLSettingsSky::ptr_t psky = environment.getCurrentSky();

    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    F32 minimum_ambiance = psky->getReflectionProbeAmbiance(should_auto_adjust);

    bool is_ambiance_pass = gCubeSnapshot && !isRadiancePass();
    F32 ambscale = is_ambiance_pass ? 0.f : 1.f;
    ambscale *= mResetFade;
    ambscale = llmax(0, ambscale);
    F32 radscale = is_ambiance_pass ? 0.5f : 1.f;
    radscale *= mResetFade;
    radscale = llmax(0, radscale);

    for (auto* refmap : mReflectionMaps)
    {
        if (refmap == nullptr)
        {
            break;
        }

        if (refmap != mDefaultProbe)
        {
            // bucket search data
            // theory of operation:
            //      1. Determine minimum and maximum depth of each influence volume and store in mDepth (done in getReflectionMaps)
            //      2. Sort by minimum depth
            //      3. Prepare a bucket for each 1m of depth out to 256m
            //      4. For each bucket, store the index of the nearest probe that might influence pixels in that bucket
            //      5. In the shader, lookup the bucket for the pixel depth to get the index of the first probe that could possibly influence
            //          the current pixel.
            unsigned int depth_min = llclamp(llfloor(refmap->mMinDepth), 0, 255);
            unsigned int depth_max = llclamp(llfloor(refmap->mMaxDepth), 0, 255);
            for (U32 i = depth_min; i <= depth_max; ++i)
            {
                if (refmap->mMinDepth < minDepth[i])
                {
                    minDepth[i] = refmap->mMinDepth;
                    mProbeData.refBucket[i][0] = refmap->mProbeIndex;
                }
            }
        }

        llassert(refmap->mProbeIndex == count);
        llassert(mReflectionMaps[refmap->mProbeIndex] == refmap);

        llassert(refmap->mCubeIndex >= 0); // should always be  true, if not, getReflectionMaps is bugged

        {
            if (refmap->mViewerObject && refmap->mViewerObject->getVolume())
            { // have active manual probes live-track the object they're associated with
                LLVOVolume* vobj = (LLVOVolume*)refmap->mViewerObject.get();

                refmap->mOrigin.load3(vobj->getPositionAgent().mV);

                if (vobj->getReflectionProbeIsBox())
                {
                    LLVector3 s = vobj->getScale().scaledVec(LLVector3(0.5f, 0.5f, 0.5f));
                    refmap->mRadius = s.magVec();
                }
                else
                {
                    refmap->mRadius = refmap->mViewerObject->getScale().mV[0] * 0.5f;
                }
            }
            modelview.affineTransform(refmap->mOrigin, oa);
            mProbeData.refSphere[count].set(oa.getF32ptr());
            mProbeData.refSphere[count].mV[3] = refmap->mRadius;
        }

        mProbeData.refIndex[count][0] = refmap->mCubeIndex;
        llassert(nc % 4 == 0);
        mProbeData.refIndex[count][1] = nc / 4;
        mProbeData.refIndex[count][3] = refmap->mPriority;

        // for objects that are reflection probes, use the volume as the influence volume of the probe
        // only possibile influence volumes are boxes and spheres, so detect boxes and treat everything else as spheres
        if (refmap->getBox(mProbeData.refBox[count]))
        { // negate priority to indicate this probe has a box influence volume
            mProbeData.refIndex[count][3] = -mProbeData.refIndex[count][3];
        }

        mProbeData.refParams[count].set(
            llmax(minimum_ambiance, refmap->getAmbiance())*ambscale, // ambiance scale
            radscale, // radiance scale
            refmap->mFadeIn, // fade in weight
            oa.getF32ptr()[2] - refmap->mRadius); // z near

        S32 ni = nc; // neighbor ("index") - index into refNeighbor to write indices for current reflection probe's neighbors
        {
            //LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmsu - refNeighbors");
            //pack neghbor list
            const U32 max_neighbors = 64;
            U32 neighbor_count = 0;

            for (auto& neighbor : refmap->mNeighbors)
            {
                if (ni >= 4096)
                { // out of space
                    break;
                }

                GLint idx = neighbor->mProbeIndex;
                if (idx == -1 || neighbor->mOccluded || neighbor->mCubeIndex == -1)
                {
                    continue;
                }

                // this neighbor may be sampled
                mProbeData.refNeighbor[ni++] = idx;

                neighbor_count++;
                if (neighbor_count >= max_neighbors)
                {
                    break;
                }
            }
        }

        if (nc == ni)
        {
            //no neighbors, tag as empty
            mProbeData.refIndex[count][1] = -1;
        }
        else
        {
            mProbeData.refIndex[count][2] = ni - nc;

            // move the cursor forward
            nc = ni;
            if (nc % 4 != 0)
            { // jump to next power of 4 for compatibility with ivec4
                nc += 4 - (nc % 4);
            }
        }


        count++;
    }

#if 0
    {
        // fill in gaps in refBucket
        S32 probe_idx = mReflectionProbeCount;

        for (int i = 0; i < 256; ++i)
        {
            if (i < count)
            { // for debugging, store depth of mReflectionsMaps[i]
                rpd.refBucket[i][1] = (S32) (mReflectionMaps[i]->mDepth * 10);
            }

            if (rpd.refBucket[i][0] == mReflectionProbeCount)
            {
                rpd.refBucket[i][0] = probe_idx;
            }
            else
            {
                probe_idx = rpd.refBucket[i][0];
            }
        }
    }
#endif

    mProbeData.refmapCount = count;

    gPipeline.mHeroProbeManager.updateUniforms();

    // Get the hero data.

    mProbeData.heroBox = gPipeline.mHeroProbeManager.mHeroData.heroBox;
    mProbeData.heroSphere = gPipeline.mHeroProbeManager.mHeroData.heroSphere;
    mProbeData.heroShape  = gPipeline.mHeroProbeManager.mHeroData.heroShape;
    mProbeData.heroMipCount   = gPipeline.mHeroProbeManager.mHeroData.heroMipCount;
    mProbeData.heroProbeCount = gPipeline.mHeroProbeManager.mHeroData.heroProbeCount;

    //copy mProbeData into uniform buffer object
    if (mUBO == 0)
    {
        glGenBuffers(1, &mUBO);
    }

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmsu - update buffer");
        glBindBuffer(GL_UNIFORM_BUFFER, mUBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(ReflectionProbeData), &mProbeData, GL_STREAM_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

#if 0
    if (!gCubeSnapshot)
    {
        for (auto& probe : mProbes)
        {
            LLViewerObject* vobj = probe->mViewerObject;
            if (vobj)
            {
                F32 time = (F32)gFrameTimeSeconds - probe->mLastUpdateTime;
                vobj->setDebugText(llformat("%d/%d/%d/%.1f - %.1f/%.1f", probe->mCubeIndex, probe->mProbeIndex, (U32) probe->mNeighbors.size(), probe->mMinDepth, probe->mMaxDepth, time), time > 1.f ? LLColor4::white : LLColor4::green);
            }
        }
    }
#endif
}

void LLReflectionMapManager::setUniforms()
{
    if (!LLPipeline::sReflectionProbesEnabled)
    {
        return;
    }

    if (mUBO == 0)
    {
        updateUniforms();
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, LLGLSLShader::UB_REFLECTION_PROBES, mUBO);
}


void renderReflectionProbe(LLReflectionMap* probe)
{
    if (probe->isRelevant())
    {
        F32* po = probe->mOrigin.getF32ptr();

        //draw orange line from probe to neighbors
        gGL.flush();
        gGL.diffuseColor4f(1, 0.5f, 0, 1);
        gGL.begin(gGL.LINES);
        for (auto& neighbor : probe->mNeighbors)
        {
            if (probe->mViewerObject && neighbor->mViewerObject)
            {
                continue;
            }

            gGL.vertex3fv(po);
            gGL.vertex3fv(neighbor->mOrigin.getF32ptr());
        }
        gGL.end();
        gGL.flush();

        gGL.diffuseColor4f(1, 1, 0, 1);
        gGL.begin(gGL.LINES);
        for (auto& neighbor : probe->mNeighbors)
        {
            if (probe->mViewerObject && neighbor->mViewerObject)
            {
                gGL.vertex3fv(po);
                gGL.vertex3fv(neighbor->mOrigin.getF32ptr());
            }
        }
        gGL.end();
        gGL.flush();
    }

#if 0
    LLSpatialGroup* group = probe->mGroup;
    if (group)
    { // draw lines from corners of object aabb to reflection probe

        const LLVector4a* bounds = group->getBounds();
        LLVector4a o = bounds[0];

        gGL.flush();
        gGL.diffuseColor4f(0, 0, 1, 1);
        F32* c = o.getF32ptr();

        const F32* bc = bounds[0].getF32ptr();
        const F32* bs = bounds[1].getF32ptr();

        // daaw blue lines from corners to center of node
        gGL.begin(gGL.LINES);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] + bs[0], bc[1] + bs[1], bc[2] + bs[2]);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] - bs[0], bc[1] + bs[1], bc[2] + bs[2]);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] + bs[0], bc[1] - bs[1], bc[2] + bs[2]);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] - bs[0], bc[1] - bs[1], bc[2] + bs[2]);

        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] + bs[0], bc[1] + bs[1], bc[2] - bs[2]);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] - bs[0], bc[1] + bs[1], bc[2] - bs[2]);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] + bs[0], bc[1] - bs[1], bc[2] - bs[2]);
        gGL.vertex3fv(c);
        gGL.vertex3f(bc[0] - bs[0], bc[1] - bs[1], bc[2] - bs[2]);
        gGL.end();

        //draw yellow line from center of node to reflection probe origin
        gGL.flush();
        gGL.diffuseColor4f(1, 1, 0, 1);
        gGL.begin(gGL.LINES);
        gGL.vertex3fv(c);
        gGL.vertex3fv(po);
        gGL.end();
        gGL.flush();
    }
#endif
}

void LLReflectionMapManager::renderDebug()
{
    gDebugProgram.bind();

    for (auto& probe : mProbes)
    {
        renderReflectionProbe(probe);
    }

    gDebugProgram.unbind();
}

void LLReflectionMapManager::initReflectionMaps()
{
    static LLCachedControl<U32> ref_probe_res(gSavedSettings, "RenderReflectionProbeResolution", 128U);
    static LLCachedControl<U32> ref_probe_irradiance_res(gSavedSettings, "RenderReflectionProbeIrradianceResolution", 16U);
    U32 probe_resolution = nhpo2(llclamp(ref_probe_res(), (U32)64, (U32)512));
    U32 irradiance_resolution = llmin(nhpo2(llclamp(ref_probe_irradiance_res(), (U32)16, (U32)256)), probe_resolution); // Must be equal or smaller then probe resolution
    if (mTexture.isNull() || mReflectionProbeCount != mDynamicProbeCount || mProbeResolution != probe_resolution ||
        mIrradianceMapResolution != irradiance_resolution || mReset)
    {
        if(mProbeResolution != probe_resolution)
        {
            mRenderTarget.release();
            mMipChain.clear();
        }

        gEXRImage = nullptr;
        mReset = false;
        mReflectionProbeCount = mDynamicProbeCount;
        mProbeResolution = probe_resolution;
        mIrradianceMapResolution = irradiance_resolution;
        mMaxProbeLOD = log2f((F32)mProbeResolution) - 1.f; // number of mips - 1

        if (mTexture.isNull() ||
            mTexture->getWidth() != mProbeResolution ||
            mReflectionProbeCount + 2 != mTexture->getCount())
        {
#if 0 // Cubemap copy critically flawed and overflows
            if (mTexture)
            {
                mTexture = new LLCubeMapArray(*mTexture, mProbeResolution, mReflectionProbeCount + 2);

                mIrradianceMaps = new LLCubeMapArray(*mIrradianceMaps, mIrradianceMapResolution, mReflectionProbeCount);
            }
            else
#endif
            {
                mTexture = new LLCubeMapArray();

                static LLCachedControl<bool> render_hdr(gSavedSettings, "RenderHDREnabled", true);

                // store mReflectionProbeCount+2 cube maps, final two cube maps are used for render target and radiance map generation
                // source)
                mTexture->allocate(mProbeResolution, 3, mReflectionProbeCount + 2, true, render_hdr);

                mIrradianceMaps = new LLCubeMapArray();
                mIrradianceMaps->allocate(mIrradianceMapResolution, 3, mReflectionProbeCount, false, render_hdr);
            }
        }

        // reset probe state
        mUpdatingFace = 0;
        mUpdatingProbe = nullptr;
        mRadiancePass = false;
        mRealtimeRadiancePass = false;
        mCinematicIrradianceReady = false;
        mCinematicRadianceReady = false;
        resetCinematicRefresh(); // [LiveProbeRefresh]
        resetProbeSchedule();    // [ProbeOnDemand]

        // if default probe already exists, remember whether or not it's complete (SL-20498)
        bool default_complete = mDefaultProbe.isNull() ? false : mDefaultProbe->mComplete;

        for (auto& probe : mProbes)
        {
            probe->mLastUpdateTime = 0.f;
            probe->mComplete = false;
            probe->mProbeIndex = -1;
            probe->mCubeArray = nullptr;
            probe->mCubeIndex = -1;
            probe->mNeighbors.clear();
            probe->mFadeIn = 0;
        }

        mCubeFree.clear();
        initCubeFree();

        if (mDefaultProbe.isNull())
        {
            llassert(mProbes.empty()); // default probe MUST be the first probe created
            mDefaultProbe = new LLReflectionMap();
            mProbes.push_back(mDefaultProbe);
        }

        llassert(mProbes[0] == mDefaultProbe);

        mDefaultProbe->mCubeIndex = 0;
        mDefaultProbe->mCubeArray = mTexture;
        mDefaultProbe->mDistance = 64.f;
        mDefaultProbe->mRadius = 4096.f;
        mDefaultProbe->mProbeIndex = 0;
        mDefaultProbe->mComplete = default_complete;

        touch_default_probe(mDefaultProbe);
    }

    if (mVertexBuffer.isNull())
    {
        U32 mask = LLVertexBuffer::MAP_VERTEX;
        LLPointer<LLVertexBuffer> buff = new LLVertexBuffer(mask);
        buff->allocateBuffer(4, 0);

        LLStrider<LLVector3> v;

        buff->getVertexStrider(v);

        v[0] = LLVector3(-1, -1, -1);
        v[1] = LLVector3(1, -1, -1);
        v[2] = LLVector3(-1, 1, -1);
        v[3] = LLVector3(1, 1, -1);

        buff->unmapBuffer();

        mVertexBuffer = buff;
    }
}

void LLReflectionMapManager::cleanup()
{
    mCinematicLiveProbe = nullptr;
    mCinematicIgnoredLightIds.clear();
    mCinematicPinnedLightIds.clear();
    mCinematicLiveProbeCapture = false;
    mCinematicIrradianceReady = false;
    mCinematicRadianceReady = false;
    resetCinematicRefresh(); // [LiveProbeRefresh]
    resetProbeSchedule();    // [ProbeOnDemand]
    mVertexBuffer = nullptr;
    mRenderTarget.release();

    mMipChain.clear();

    mTexture = nullptr;
    mIrradianceMaps = nullptr;

    mProbes.clear();
    mKillList.clear();
    mCreateList.clear();

    mReflectionMaps.clear();
    mUpdatingFace = 0;

    mDefaultProbe = nullptr;
    mUpdatingProbe = nullptr;

    glDeleteBuffers(1, &mUBO);
    mUBO = 0;

    // note: also called on teleport (not just shutdown), so make sure we're in a good "starting" state
    initCubeFree();
}

void LLReflectionMapManager::doOcclusion()
{
    LLVector4a eye;
    eye.load3(LLViewerCamera::instance().getOrigin().mV);

    for (auto& probe : mProbes)
    {
        if (probe != nullptr && probe != mDefaultProbe)
        {
            probe->doOcclusion(eye);
        }
    }
}

void LLReflectionMapManager::forceDefaultProbeAndUpdateUniforms(bool force)
{
    static std::vector<bool> mProbeWasOccluded;

    if (force)
    {
        llassert(mProbeWasOccluded.empty());

        for (size_t i = 0; i < mProbes.size(); ++i)
        {
            auto& probe = mProbes[i];
            mProbeWasOccluded.push_back(probe->mOccluded);
            if (probe != nullptr && probe != mDefaultProbe)
            {
                probe->mOccluded = true;
            }
        }

        updateUniforms();
    }
    else
    {
        llassert(mProbes.size() == mProbeWasOccluded.size());

        const size_t n = llmin(mProbes.size(), mProbeWasOccluded.size());
        for (size_t i = 0; i < n; ++i)
        {
            auto& probe = mProbes[i];
            llassert(probe->mOccluded == (probe != mDefaultProbe));
            probe->mOccluded = mProbeWasOccluded[i];
        }
        mProbeWasOccluded.clear();
        mProbeWasOccluded.shrink_to_fit();
    }
}
