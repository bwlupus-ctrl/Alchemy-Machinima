/**
 * @file llpresentationtime.cpp
 * @brief Dual-time presentation clock for Temporal Capture (World Time Scale).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llpresentationtime.h"

#include "llcontrol.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llmotioncontroller.h"     // sole writer of sGlobalTimeFactor

// Hot-path frame pointer: updated to point at the singleton's frozen frame each
// tick(), cleared on destruction. Consumers read it through currentFrame()
// without a singleton lookup. Main-thread only (written in idle(), read in the
// same idle->display pass).
static const LLTemporalFrameContext* sCurrentFrame = nullptr;
static const LLTemporalFrameContext  sLiveDefault{};   // LIVE, drive_mask 0

// Clamp bounds for the world scale slider (0 = freeze .. 8x fast). Kept in sync
// with the panel's slider detents.
static constexpr F32 TEMPORAL_MIN_SCALE = 0.f;
static constexpr F32 TEMPORAL_MAX_SCALE = 8.f;

LLPresentationTime::LLPresentationTime()
{
    mMonotonic.reset();
}

LLPresentationTime::~LLPresentationTime()
{
    restoreStockClocks();
    sCurrentFrame = nullptr;
}

// static
const LLTemporalFrameContext& LLPresentationTime::currentFrame()
{
    return sCurrentFrame ? *sCurrentFrame : sLiveDefault;
}

U32 LLPresentationTime::readDriveMask() const
{
    static LLCachedControl<bool> drive_anim(gSavedSettings,    "TemporalDriveAnimation",   true);
    static LLCachedControl<bool> drive_objects(gSavedSettings, "TemporalDriveObjects",     true);
    static LLCachedControl<bool> drive_texanim(gSavedSettings, "TemporalDriveTextureAnim", true);
    static LLCachedControl<bool> drive_parts(gSavedSettings,   "TemporalDriveParticles",   true);
    static LLCachedControl<bool> drive_camera(gSavedSettings,  "TemporalDriveCamera",      false);

    U32 mask = 0;
    if (drive_anim)    mask |= static_cast<U32>(LLTemporalFeature::ANIMATION);
    if (drive_objects) mask |= static_cast<U32>(LLTemporalFeature::OBJECTS);
    if (drive_texanim) mask |= static_cast<U32>(LLTemporalFeature::TEXTURE_ANIM);
    if (drive_parts)   mask |= static_cast<U32>(LLTemporalFeature::PARTICLES);
    if (drive_camera)  mask |= static_cast<U32>(LLTemporalFeature::CAMERA);
    return mask;
}

void LLPresentationTime::tick()
{
    // 0 = Live (stock), 1 = Manual scale. Other modes are reserved and not
    // settings-selectable in Landing 1.
    static LLCachedControl<S32> mode_setting(gSavedSettings, "TemporalMode",       0);
    static LLCachedControl<F32> world_scale(gSavedSettings,  "TemporalWorldScale", 1.f);

    // Independent monotonic wall delta -- never gFrameIntervalSeconds (which is
    // recomputed later in idle() and is distrusted above 200fps).
    const F64 now = mMonotonic.getElapsedTimeF64();
    if (!mInitialized)
    {
        mInitialized = true;
        mPrevWall = now;
    }
    // Wall delta for THIS iteration. The presentation CLOCK is advanced by the full
    // (lower-bounded) delta so presentation_time stays accurate. The first tick
    // yields 0 via the init branch above, so there is no pathological startup step.
    //
    // Consumers split by what they can safely integrate: skeletal animation (the
    // controller's own uncapped delta * sGlobalTimeFactor, llmotioncontroller.cpp:
    // 858-871) and texture animation consume the FULL presentation delta. The
    // position/emission consumers (actor traversal, gaze, ghost turns, particles,
    // object spin, object paths) apply a uniform 0.25s subsystem hitch cap at their
    // own call sites -- they are not built to integrate one huge step (ping-pong
    // reflection, rotation, emission), as removing the cap demonstrably breaks the
    // ping-pong/stepTurn evaluators. Gait<->traversal coupling is therefore EXACT
    // while the per-frame presentation delta stays under 0.25s -- normal operation at
    // ANY scale. It degrades ONLY on a genuine stall (a wall frame > 0.25/scale),
    // where the capped consumers behave exactly as the stock viewer already does
    // (stock caps traversal at 0.25 with uncapped gait too) -- in fact no worse than
    // stock, since the scale shrinks gait's step as well. Exact stall coupling would
    // need per-consumer sub-stepping (deferred, see the brief).
    const F64 wall_delta = llmax(0.0, now - mPrevWall);
    mPrevWall = now;

    // Resolve mode / effective scale / paused from settings.
    LLTemporalMode mode = LLTemporalMode::LIVE;
    F32  effective = 1.f;
    bool paused = false;

    if (mode_setting() == 1)   // MANUAL_SCALE
    {
        effective = llclamp((F32)world_scale, TEMPORAL_MIN_SCALE, TEMPORAL_MAX_SCALE);
        if (effective <= 0.f)
        {
            mode = LLTemporalMode::PAUSED;
            paused = true;
            effective = 0.f;
        }
        else
        {
            mode = LLTemporalMode::MANUAL_SCALE;
        }
    }

    // Advance the presentation clock. Wall time is untouched.
    mPrevPresentationTime = mPresentationTime;
    if (mode == LLTemporalMode::LIVE)
    {
        mPresentationTime += wall_delta;                       // 1:1
    }
    else if (!paused)
    {
        mPresentationTime += wall_delta * (F64)effective;
    }
    // paused: hold presentation_time

    // Freeze exactly one immutable context for this iteration.
    mFrame.generation        = ++mGeneration;
    mFrame.mode              = mode;
    mFrame.wall_time         = now;
    mFrame.wall_delta        = wall_delta;
    mFrame.presentation_time = mPresentationTime;
    mFrame.presentation_delta= llmax(0.0, mPresentationTime - mPrevPresentationTime);
    mFrame.effective_scale   = effective;
    mFrame.paused            = paused;
    mFrame.drive_mask        = (mode == LLTemporalMode::LIVE) ? 0u : readDriveMask();

    sCurrentFrame = &mFrame;

    // Apply the animation global -- the SOLE writer of sGlobalTimeFactor. When
    // the ANIMATION drive is active we set it to the scale (0 at freeze, which
    // halts every skeleton); otherwise we restore 1.0 exactly once.
    if (mFrame.active() && mFrame.drives(LLTemporalFeature::ANIMATION))
    {
        if (!mAnimGlobalApplied)
        {
            // Take over: remember whatever the global was (default 1.0, or a value
            // set by the Advanced animation-speed menu) so release restores it
            // exactly instead of a blind 1.0. These two writes are adjacent and
            // cannot fail between, so ownership + restore stay deterministic.
            mSavedGlobalTimeFactor = LLMotionController::getGlobalTimeFactor();
            mAnimGlobalApplied = true;
        }
        // While active, World Time Scale is authoritative and re-asserts the scale
        // every tick (so the Advanced menu can't fight it mid-shot).
        LLMotionController::setGlobalTimeFactor(effective);
    }
    else
    {
        restoreStockClocks();
    }
}

void LLPresentationTime::restoreStockClocks()
{
    if (mAnimGlobalApplied)
    {
        // restore the value that was in effect before World Time Scale engaged
        // (not a blind 1.0), so an Advanced-menu slow-motion survives
        LLMotionController::setGlobalTimeFactor(mSavedGlobalTimeFactor);
        mAnimGlobalApplied = false;
    }
}
