/**
 * @file llkeyframewalkmotion.cpp
 * @brief Implementation of LLKeyframeWalkMotion class.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "linden_common.h"

#include "llkeyframewalkmotion.h"
#include "llcharacter.h"
#include "llmath.h"
#include "m3math.h"
#include "llcriticaldamp.h"
#include "llframetimer.h"

#include <vector>

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------
const F32 MAX_WALK_PLAYBACK_SPEED = 8.f;        // max m/s for which we adjust walk cycle speed

const F32 MIN_WALK_SPEED = 0.1f;                // minimum speed at which we use velocity for down foot detection
const F32 TIME_EPSILON = 0.001f;                // minumum frame time
const F32 MAX_TIME_DELTA = 2.f;                 // max two seconds a frame for calculating interpolation
F32 SPEED_ADJUST_MAX_SEC = 2.f;                 // maximum adjustment to walk animation playback speed for a second
F32 ANIM_SPEED_MAX = 1.5f;                      // absolute upper limit on animation speed
const F32 MAX_ROLL = 0.6f;
const F32 SPEED_ADJUST_TIME_CONSTANT = 0.1f;    // time constant for speed adjustment interpolation

//-----------------------------------------------------------------------------
// [PosePolish M3] Phase-aware locomotion constants.
// docs/pose_polish_integration_plan.md "Milestone 3 -- Phase-aware locomotion".
// All of this is only ever touched when sPhaseAwareLocomotionEnabled is true.
//-----------------------------------------------------------------------------
const S32 PHASE_SAMPLE_COUNT = 24;                  // samples taken around one cycle to find plant windows
const F32 PHASE_MIN_CYCLE_LENGTH = 0.15f;           // seconds; shorter loops aren't trusted for stride analysis
const F32 PHASE_LOW_ACTIVITY_FRACTION = 0.35f;      // sample counts as "planted" below this fraction of that ankle's peak activity
const F32 PHASE_MIN_WINDOW_FRACTION = 0.12f;        // a plant (or swing) window must span at least this much of the cycle
const F32 PHASE_MAX_WINDOW_FRACTION = 0.60f;        // ...and no more than this much (else it's not really alternating)
const F32 PHASE_MIN_RIGHT_OFFSET = 0.20f;           // valid range, as a fraction of cycle, for the R-vs-L plant offset
const F32 PHASE_MAX_RIGHT_OFFSET = 0.80f;
const F64 PHASE_HANDOFF_FRESHNESS_SEC = 0.5;        // a handoff record older than this is presumed unrelated/stale
const F64 PHASE_HANDOFF_PRUNE_AGE_SEC = 30.0;       // opportunistic cleanup horizon for the per-character map
const F32 PHASE_MAX_RIGHT_OFFSET_MISMATCH = 0.15f;  // max tolerated cross-clip difference in R-vs-L plant offset

//-----------------------------------------------------------------------------
// [PosePolish M3] wrapUnit()
// Wraps an arbitrary float into [0, 1) -- used throughout for cyclic phase math.
//-----------------------------------------------------------------------------
static F32 wrapUnit(F32 x)
{
    F32 w = x - std::floor(x);
    // guard the (rare) edge case where floating point puts us at exactly 1.0
    if (w >= 1.f) w = 0.f;
    if (w < 0.f) w = 0.f;
    return w;
}

//-----------------------------------------------------------------------------
// [PosePolish M3] circularDistanceUnit()
// Shortest distance between two phases in [0,1), itself in [0, 0.5].
//-----------------------------------------------------------------------------
static F32 circularDistanceUnit(F32 a, F32 b)
{
    F32 d = wrapUnit(a - b);
    return llmin(d, 1.f - d);
}

//-----------------------------------------------------------------------------
// [PosePolish M3] static member definitions
//-----------------------------------------------------------------------------
bool LLKeyframeWalkMotion::sPhaseAwareLocomotionEnabled = false;
std::map<LLUUID, LLKeyframeWalkMotion::LLLocomotionPhaseInfo> LLKeyframeWalkMotion::sPhaseInfoCache;
std::map<const LLCharacter*, LLKeyframeWalkMotion::LLLocomotionHandoff> LLKeyframeWalkMotion::sLocomotionHandoff;

//-----------------------------------------------------------------------------
// LLKeyframeWalkMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLKeyframeWalkMotion::LLKeyframeWalkMotion(const LLUUID &id)
:   LLKeyframeMotion(id),
    mCharacter(NULL),
    mCyclePhase(0.0f),
    mRealTimeLast(0.0f),
    mAdjTimeLast(0.0f),
    mDownFoot(0)
{}


//-----------------------------------------------------------------------------
// ~LLKeyframeWalkMotion()
// Class Destructor
//-----------------------------------------------------------------------------
LLKeyframeWalkMotion::~LLKeyframeWalkMotion()
{}


//-----------------------------------------------------------------------------
// LLKeyframeWalkMotion::onInitialize()
//-----------------------------------------------------------------------------
LLMotion::LLMotionInitStatus LLKeyframeWalkMotion::onInitialize(LLCharacter *character)
{
    mCharacter = character;

    return LLKeyframeMotion::onInitialize(character);
}

//-----------------------------------------------------------------------------
// LLKeyframeWalkMotion::onActivate()
//-----------------------------------------------------------------------------
bool LLKeyframeWalkMotion::onActivate()
{
    mRealTimeLast = 0.0f;
    mAdjTimeLast = 0.0f;

    // [PosePolish M3] Phase-aware locomotion start. See docs/pose_polish_integration_plan.md
    // "Milestone 3". No-op unless sPhaseAwareLocomotionEnabled is true (pushed from newview;
    // see the setter's doc comment in llkeyframewalkmotion.h) -- when false this whole block
    // is skipped and mAdjTimeLast stays exactly 0.0f as today.
    if (sPhaseAwareLocomotionEnabled)
    {
        F32 phase_start_time = 0.f;
        if (computeLocomotionStartOffset(phase_start_time))
        {
            mAdjTimeLast = phase_start_time;
        }
    }

    return LLKeyframeMotion::onActivate();
}

//-----------------------------------------------------------------------------
// LLKeyframeWalkMotion::onDeactivate()
//-----------------------------------------------------------------------------
void LLKeyframeWalkMotion::onDeactivate()
{
    mCharacter->removeAnimationData("Down Foot");

    // [PosePolish M3] Only erase the shared per-character handoff record if we are
    // still its most recent writer. Freshness (see computeLocomotionStartOffset)
    // alone cannot tell a genuine clip-to-clip handoff (e.g. walk -> run, where the
    // new clip is already active and writing its own fresh record well before this
    // deactivation finally happens, once ease-out completes) from a real stop
    // (walk -> idle/sit/teleport) followed by an unrelated later activation within
    // the freshness window. If nobody has taken over since our last write, this
    // really is the end of a locomotion sequence, so the record must not survive
    // to wrongly seed some later, disconnected walk/run activation. If a sibling
    // motion HAS taken over, its own record must be left alone.
    if (sPhaseAwareLocomotionEnabled && mCharacter)
    {
        std::map<const LLCharacter*, LLLocomotionHandoff>::iterator found = sLocomotionHandoff.find(mCharacter);
        if (found != sLocomotionHandoff.end() && found->second.mLastWriter == this)
        {
            sLocomotionHandoff.erase(found);
        }
    }

    LLKeyframeMotion::onDeactivate();
}

//-----------------------------------------------------------------------------
// LLKeyframeWalkMotion::onUpdate()
//-----------------------------------------------------------------------------
bool LLKeyframeWalkMotion::onUpdate(F32 time, U8* joint_mask)
{
    LL_PROFILE_ZONE_SCOPED;
    // compute time since last update
    F32 deltaTime = time - mRealTimeLast;

    void* speed_ptr = mCharacter->getAnimationData("Walk Speed");
    F32 speed = (speed_ptr) ? *((F32 *)speed_ptr) : 1.f;

    // adjust the passage of time accordingly
    F32 adjusted_time = mAdjTimeLast + (deltaTime * speed);

    // save time for next update
    mRealTimeLast = time;
    mAdjTimeLast = adjusted_time;

    // handle wrap around
    if (adjusted_time < 0.0f)
    {
        adjusted_time = getDuration() + fmod(adjusted_time, getDuration());
    }

    // [PosePolish M3] record this clip's current gait phase so a sibling walk/run
    // motion activating on this same character can start near a compatible plant
    // phase. No-op unless sPhaseAwareLocomotionEnabled is true.
    if (sPhaseAwareLocomotionEnabled)
    {
        updateLocomotionHandoff(mAdjTimeLast);
    }

    // let the base class update the cycle
    return LLKeyframeMotion::onUpdate( adjusted_time, joint_mask );
}

// End

//-----------------------------------------------------------------------------
// [PosePolish M3] anonymous-namespace helper
//-----------------------------------------------------------------------------
namespace
{
    // Finds the longest contiguous *circular* run of samples at or below
    // low_threshold (a run may wrap past the last sample back to the first).
    // Returns true and fills out_center_phase (0..1) / out_length_frac (0..1,
    // fraction of the full cycle the run spans) only when a run was found.
    bool findPlantWindow(const std::vector<F32>& activity, F32 low_threshold,
                          F32& out_center_phase, F32& out_length_frac)
    {
        const S32 n = static_cast<S32>(activity.size());
        if (n < 4)
        {
            return false;
        }

        S32 best_start = -1;
        S32 best_len = 0;
        S32 run_start = -1;
        S32 run_len = 0;

        // walk two full laps so a run that wraps past index n-1 back to 0 is found,
        // but never count more than one full lap as a single run
        for (S32 i = 0; i < n * 2; ++i)
        {
            S32 idx = i % n;
            if (activity[idx] <= low_threshold)
            {
                if (run_len == 0)
                {
                    run_start = i;
                }
                ++run_len;
                if (run_len > best_len && run_len <= n)
                {
                    best_len = run_len;
                    best_start = run_start;
                }
            }
            else
            {
                run_len = 0;
            }
        }

        if (best_start < 0 || best_len <= 0)
        {
            return false;
        }

        out_length_frac = static_cast<F32>(best_len) / static_cast<F32>(n);
        F32 center_index = static_cast<F32>(best_start) + static_cast<F32>(best_len - 1) * 0.5f;
        out_center_phase = wrapUnit(center_index / static_cast<F32>(n));
        return true;
    }
}

//-----------------------------------------------------------------------------
// [PosePolish M3] LLKeyframeWalkMotion::getLocomotionPhaseInfo()
// Lazily computes and caches, per animation asset, the alternating L/R foot
// plant-window phases for this clip. Reads only already-loaded keyframe curve
// data (mJointMotionList, inherited protected from LLKeyframeMotion) -- no
// character/skeleton stepping is needed, so this is cheap and safe to compute
// once on first use. Only ever called when sPhaseAwareLocomotionEnabled is
// true (see callers).
//-----------------------------------------------------------------------------
const LLKeyframeWalkMotion::LLLocomotionPhaseInfo& LLKeyframeWalkMotion::getLocomotionPhaseInfo()
{
    const LLUUID& asset_id = getID();
    std::map<LLUUID, LLLocomotionPhaseInfo>::iterator cached = sPhaseInfoCache.find(asset_id);
    if (cached != sPhaseInfoCache.end())
    {
        return cached->second;
    }

    LLLocomotionPhaseInfo info;

    if (mJointMotionList
        && mJointMotionList->mLoop
        && (mJointMotionList->mLoopOutPoint - mJointMotionList->mLoopInPoint) >= PHASE_MIN_CYCLE_LENGTH
        && mJointMotionList->mDuration > TIME_EPSILON)
    {
        // find the ankle joint tracks by name -- the same joints LLWalkAdjustMotion
        // already samples (at runtime, in world space) for foot-slip correction.
        JointMotion* left_ankle = NULL;
        JointMotion* right_ankle = NULL;
        for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); ++i)
        {
            JointMotion* jm = mJointMotionList->getJointMotion(i);
            if (!jm) continue;
            if (jm->mJointName == "mAnkleLeft") left_ankle = jm;
            else if (jm->mJointName == "mAnkleRight") right_ankle = jm;
        }

        if (left_ankle && right_ankle)
        {
            F32 cycle_start = mJointMotionList->mLoopInPoint;
            F32 cycle_length = mJointMotionList->mLoopOutPoint - mJointMotionList->mLoopInPoint;
            F32 duration = mJointMotionList->mDuration;

            // per-sample "activity" for each ankle: angular change of the ankle's
            // own rotation curve (present on essentially every keyframe walk clip)
            // plus, when present, its local position-curve displacement. A low,
            // sustained activity window is our proxy for "foot roughly still ==
            // planted"; a swing window is the opposite. This never needs a bound
            // character or world-space foot velocity, matching the task's fallback:
            // "infer from the ankle joint state track."
            std::vector<F32> left_activity(PHASE_SAMPLE_COUNT, 0.f);
            std::vector<F32> right_activity(PHASE_SAMPLE_COUNT, 0.f);

            F32 dt = cycle_length / static_cast<F32>(PHASE_SAMPLE_COUNT);
            for (S32 i = 0; i < PHASE_SAMPLE_COUNT; ++i)
            {
                F32 t0 = cycle_start + dt * static_cast<F32>(i);
                F32 t1 = (i + 1 == PHASE_SAMPLE_COUNT) ? (cycle_start + cycle_length)
                                                        : (cycle_start + dt * static_cast<F32>(i + 1));

                LLQuaternion lr0 = left_ankle->mRotationCurve.getValue(t0, duration);
                LLQuaternion lr1 = left_ankle->mRotationCurve.getValue(t1, duration);
                F32 l_activity = 2.f * acosf(llclamp(fabsf(dot(lr0, lr1)), 0.f, 1.f));

                LLQuaternion rr0 = right_ankle->mRotationCurve.getValue(t0, duration);
                LLQuaternion rr1 = right_ankle->mRotationCurve.getValue(t1, duration);
                F32 r_activity = 2.f * acosf(llclamp(fabsf(dot(rr0, rr1)), 0.f, 1.f));

                if (left_ankle->mPositionCurve.mNumKeys > 0)
                {
                    LLVector3 lp0 = left_ankle->mPositionCurve.getValue(t0, duration);
                    LLVector3 lp1 = left_ankle->mPositionCurve.getValue(t1, duration);
                    l_activity += (lp1 - lp0).magVec() * 2.f;
                }
                if (right_ankle->mPositionCurve.mNumKeys > 0)
                {
                    LLVector3 rp0 = right_ankle->mPositionCurve.getValue(t0, duration);
                    LLVector3 rp1 = right_ankle->mPositionCurve.getValue(t1, duration);
                    r_activity += (rp1 - rp0).magVec() * 2.f;
                }

                left_activity[i] = l_activity;
                right_activity[i] = r_activity;
            }

            F32 left_max = 0.f, right_max = 0.f;
            for (S32 i = 0; i < PHASE_SAMPLE_COUNT; ++i)
            {
                left_max = llmax(left_max, left_activity[i]);
                right_max = llmax(right_max, right_activity[i]);
            }

            F32 left_center = 0.f, left_len = 0.f;
            F32 right_center = 0.f, right_len = 0.f;
            bool left_ok = (left_max > 0.f)
                && findPlantWindow(left_activity, left_max * PHASE_LOW_ACTIVITY_FRACTION, left_center, left_len);
            bool right_ok = (right_max > 0.f)
                && findPlantWindow(right_activity, right_max * PHASE_LOW_ACTIVITY_FRACTION, right_center, right_len);

            // reliable only if BOTH feet show a genuine, alternating plant window:
            // neither a sliver nor almost the whole cycle, and the two plants
            // roughly opposite each other in phase (not both planting at once).
            if (left_ok && right_ok
                && left_len >= PHASE_MIN_WINDOW_FRACTION && left_len <= PHASE_MAX_WINDOW_FRACTION
                && right_len >= PHASE_MIN_WINDOW_FRACTION && right_len <= PHASE_MAX_WINDOW_FRACTION)
            {
                F32 right_offset = wrapUnit(right_center - left_center);
                if (right_offset >= PHASE_MIN_RIGHT_OFFSET && right_offset <= PHASE_MAX_RIGHT_OFFSET)
                {
                    info.mValid = true;
                    info.mCycleStart = cycle_start;
                    info.mCycleLength = cycle_length;
                    info.mLeftPlantPhase = left_center;
                    info.mRightOffset = right_offset;
                }
            }
        }
    }
    // else: not a looping clip, too short, or missing ankle tracks -- info stays
    // invalid and every caller below falls back to stock (time-0) behavior.

    std::pair<std::map<LLUUID, LLLocomotionPhaseInfo>::iterator, bool> inserted =
        sPhaseInfoCache.insert(std::make_pair(asset_id, info));
    return inserted.first->second;
}

//-----------------------------------------------------------------------------
// [PosePolish M3] LLKeyframeWalkMotion::updateLocomotionHandoff()
// Publishes this clip's current gait phase, in a clip-independent "canonical"
// form (0 == a left-foot plant), so a sibling walk/run motion activating on
// this same character moments later can pick it up. Only ever called when
// sPhaseAwareLocomotionEnabled is true.
//-----------------------------------------------------------------------------
void LLKeyframeWalkMotion::updateLocomotionHandoff(F32 adjusted_time)
{
    if (!mCharacter)
    {
        return;
    }

    const LLLocomotionPhaseInfo& info = getLocomotionPhaseInfo();
    if (!info.mValid)
    {
        // this clip's own phase isn't reliable -- don't pollute the shared
        // handoff record with a guess.
        return;
    }

    F32 local_phase = wrapUnit((adjusted_time - info.mCycleStart) / info.mCycleLength);
    F32 canonical_phase = wrapUnit(local_phase - info.mLeftPlantPhase);

    F64 now = LLFrameTimer::getElapsedSeconds();

    // opportunistic cleanup so this map can never grow unbounded over a long session
    for (std::map<const LLCharacter*, LLLocomotionHandoff>::iterator it = sLocomotionHandoff.begin();
         it != sLocomotionHandoff.end(); )
    {
        if (it->first != mCharacter && (now - it->second.mUpdateTime) > PHASE_HANDOFF_PRUNE_AGE_SEC)
        {
            it = sLocomotionHandoff.erase(it);
        }
        else
        {
            ++it;
        }
    }

    LLLocomotionHandoff& record = sLocomotionHandoff[mCharacter];
    record.mCanonicalPhase = canonical_phase;
    record.mRightOffset = info.mRightOffset;
    record.mUpdateTime = now;
    record.mLastWriter = this;
}

//-----------------------------------------------------------------------------
// [PosePolish M3] LLKeyframeWalkMotion::computeLocomotionStartOffset()
// If this clip has a reliable plant phase (getLocomotionPhaseInfo), a *fresh*
// handoff record exists for this character -- meaning some other walk/run
// motion was actively updating on it moments ago, i.e. winding down, not a
// cold start from standing/sitting/teleport/scrub (see onDeactivate(), which
// erases the record once nothing has taken over) -- AND that record's own
// right-foot offset is close enough to this clip's own (so aligning the left
// foot doesn't leave the right foot far out of phase), returns a start time
// within this clip's own loop that lands near the same canonical plant phase.
// The result is always inside [mCycleStart, mCycleStart+mCycleLength), i.e.
// bounded to a single stride by construction; this seeds mAdjTimeLast once at
// onActivate() and never touches the per-frame playback rate, so it cannot
// look sped up (keeps the SPEED_ADJUST_MAX_SEC clamp above completely
// untouched). Returns false (fall back to the stock time-0 start) otherwise.
//-----------------------------------------------------------------------------
bool LLKeyframeWalkMotion::computeLocomotionStartOffset(F32& start_time_out)
{
    if (!mCharacter)
    {
        return false;
    }

    const LLLocomotionPhaseInfo& info = getLocomotionPhaseInfo();
    if (!info.mValid)
    {
        return false;
    }

    std::map<const LLCharacter*, LLLocomotionHandoff>::iterator found = sLocomotionHandoff.find(mCharacter);
    if (found == sLocomotionHandoff.end() || found->second.mUpdateTime < 0.0)
    {
        return false;
    }

    F64 now = LLFrameTimer::getElapsedSeconds();
    if ((now - found->second.mUpdateTime) > PHASE_HANDOFF_FRESHNESS_SEC)
    {
        // stale: whatever last recorded a phase on this character wasn't
        // actively winding down into this activation -- don't guess.
        return false;
    }

    // Left-anchor alignment alone can put the right foot far out of phase if
    // the two clips' own R-vs-L plant offsets differ (e.g. an outgoing 0.8 and
    // an incoming 0.2 both pass the per-clip alternation check individually,
    // yet would leave the right foot ~0.6 of a cycle apart). Only accept the
    // seed when both clips' right-offsets are close, so the single left-anchor
    // seed keeps *both* feet reasonably aligned rather than just one.
    if (circularDistanceUnit(found->second.mRightOffset, info.mRightOffset) > PHASE_MAX_RIGHT_OFFSET_MISMATCH)
    {
        return false;
    }

    F32 desired_local_phase = wrapUnit(found->second.mCanonicalPhase + info.mLeftPlantPhase);
    start_time_out = info.mCycleStart + desired_local_phase * info.mCycleLength;
    return true;
}

//-----------------------------------------------------------------------------
// LLWalkAdjustMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLWalkAdjustMotion::LLWalkAdjustMotion(const LLUUID &id) :
    LLMotion(id),
    mLastTime(0.f),
    mAnimSpeed(0.f),
    mAdjustedSpeed(0.f),
    mRelativeDir(0.f),
    mAnkleOffset(0.f)
{
    mName = "walk_adjust";
    mPelvisState = new LLJointState;
}

//-----------------------------------------------------------------------------
// LLWalkAdjustMotion::onInitialize()
//-----------------------------------------------------------------------------
LLMotion::LLMotionInitStatus LLWalkAdjustMotion::onInitialize(LLCharacter *character)
{
    mCharacter = character;
    mLeftAnkleJoint = mCharacter->getJoint("mAnkleLeft");
    mRightAnkleJoint = mCharacter->getJoint("mAnkleRight");

    mPelvisJoint = mCharacter->getJoint("mPelvis");
    mPelvisState->setJoint( mPelvisJoint );
    if ( !mPelvisJoint )
    {
        LL_WARNS() << getName() << ": Can't get pelvis joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mPelvisState->setUsage(LLJointState::POS);
    addJointState( mPelvisState );

    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// LLWalkAdjustMotion::onActivate()
//-----------------------------------------------------------------------------
bool LLWalkAdjustMotion::onActivate()
{
    mAnimSpeed = 0.f;
    mAdjustedSpeed = 0.f;
    mRelativeDir = 1.f;
    mPelvisState->setPosition(LLVector3::zero);
    // store ankle positions for next frame
    mLastLeftFootGlobalPos = mCharacter->getPosGlobalFromAgent(mLeftAnkleJoint->getWorldPosition());
    mLastLeftFootGlobalPos.mdV[VZ] = 0.0;

    mLastRightFootGlobalPos = mCharacter->getPosGlobalFromAgent(mRightAnkleJoint->getWorldPosition());
    mLastRightFootGlobalPos.mdV[VZ] = 0.0;

    F32 leftAnkleOffset = (mLeftAnkleJoint->getWorldPosition() - mCharacter->getCharacterPosition()).magVec();
    F32 rightAnkleOffset = (mRightAnkleJoint->getWorldPosition() - mCharacter->getCharacterPosition()).magVec();
    mAnkleOffset = llmax(leftAnkleOffset, rightAnkleOffset);

    return true;
}

//-----------------------------------------------------------------------------
// LLWalkAdjustMotion::onUpdate()
//-----------------------------------------------------------------------------
bool LLWalkAdjustMotion::onUpdate(F32 time, U8* joint_mask)
{
    LL_PROFILE_ZONE_SCOPED;
    // delta_time is guaranteed to be non zero
    F32 delta_time = llclamp(time - mLastTime, TIME_EPSILON, MAX_TIME_DELTA);
    mLastTime = time;

    // find the avatar motion vector in the XY plane
    LLVector3 avatar_velocity = mCharacter->getCharacterVelocity() * mCharacter->getTimeDilation();
    avatar_velocity.mV[VZ] = 0.f;

    F32 speed = llclamp(avatar_velocity.magVec(), 0.f, MAX_WALK_PLAYBACK_SPEED);

    // grab avatar->world transforms
    LLQuaternion avatar_to_world_rot = mCharacter->getRootJoint()->getWorldRotation();

    LLQuaternion world_to_avatar_rot(avatar_to_world_rot);
    world_to_avatar_rot.conjugate();

    LLVector3 foot_slip_vector;

    // find foot drift along velocity vector
    if (speed > MIN_WALK_SPEED)
    {   // walking/running

        // calculate world-space foot drift
        // use global coordinates to seamlessly handle region crossings
        LLVector3d leftFootGlobalPosition = mCharacter->getPosGlobalFromAgent(mLeftAnkleJoint->getWorldPosition());
        leftFootGlobalPosition.mdV[VZ] = 0.0;
        LLVector3 leftFootDelta(leftFootGlobalPosition - mLastLeftFootGlobalPos);
        mLastLeftFootGlobalPos = leftFootGlobalPosition;

        LLVector3d rightFootGlobalPosition = mCharacter->getPosGlobalFromAgent(mRightAnkleJoint->getWorldPosition());
        rightFootGlobalPosition.mdV[VZ] = 0.0;
        LLVector3 rightFootDelta(rightFootGlobalPosition - mLastRightFootGlobalPos);
        mLastRightFootGlobalPos = rightFootGlobalPosition;

        // get foot drift along avatar direction of motion
        F32 left_foot_slip_amt = leftFootDelta * avatar_velocity;
        F32 right_foot_slip_amt = rightFootDelta * avatar_velocity;

        // if right foot is pushing back faster than left foot...
        if (right_foot_slip_amt < left_foot_slip_amt)
        {   //...use it to calculate optimal animation speed
            foot_slip_vector = rightFootDelta;
        }
        else
        {   // otherwise use the left foot
            foot_slip_vector = leftFootDelta;
        }

        // calculate ideal pelvis offset so that foot is glued to ground and damp towards it
        // this will soak up transient slippage
        //
        // FIXME: this interacts poorly with speed adjustment
        // mPelvisOffset compensates for foot drift by moving the avatar pelvis in the opposite
        // direction of the drift, up to a certain limited distance
        // but this will cause the animation playback rate calculation below to
        // kick in too slowly and sometimes start playing the animation in reverse.

        //mPelvisOffset -= PELVIS_COMPENSATION_WIEGHT * (foot_slip_vector * world_to_avatar_rot);//lerp(LLVector3::zero, -1.f * (foot_slip_vector * world_to_avatar_rot), LLSmoothInterpolation::getInterpolant(0.1f));

        ////F32 drift_comp_max = DRIFT_COMP_MAX_TOTAL * (llclamp(speed, 0.f, DRIFT_COMP_MAX_SPEED) / DRIFT_COMP_MAX_SPEED);
        //F32 drift_comp_max = DRIFT_COMP_MAX_TOTAL;

        //// clamp pelvis offset to a 90 degree arc behind the nominal position
        //// NB: this is an ADDITIVE amount that is accumulated every frame, so clamping it alone won't do the trick
        //// must clamp with absolute position of pelvis in mind
        //LLVector3 currentPelvisPos = mPelvisState->getJoint()->getPosition();
        //mPelvisOffset.mV[VX] = llclamp( mPelvisOffset.mV[VX], -drift_comp_max, drift_comp_max );
        //mPelvisOffset.mV[VY] = llclamp( mPelvisOffset.mV[VY], -drift_comp_max, drift_comp_max );
        //mPelvisOffset.mV[VZ] = 0.f;
        //
        //mLastRightFootGlobalPos += LLVector3d(mPelvisOffset * avatar_to_world_rot);
        //mLastLeftFootGlobalPos += LLVector3d(mPelvisOffset * avatar_to_world_rot);

        //foot_slip_vector -= mPelvisOffset;

        LLVector3 avatar_movement_dir = avatar_velocity;
        avatar_movement_dir.normalize();

        // planted foot speed is avatar velocity - foot slip amount along avatar movement direction
        F32 foot_speed = speed - ((foot_slip_vector * avatar_movement_dir) / delta_time);

        // multiply animation playback rate so that foot speed matches avatar speed
        F32 min_speed_multiplier = clamp_rescale(speed, 0.f, 1.f, 0.f, 0.1f);
        F32 desired_speed_multiplier = llclamp(speed / foot_speed, min_speed_multiplier, ANIM_SPEED_MAX);

        // blend towards new speed adjustment value
        F32 new_speed_adjust = LLSmoothInterpolation::lerp(mAdjustedSpeed, desired_speed_multiplier, SPEED_ADJUST_TIME_CONSTANT);

        // limit that rate at which the speed adjustment changes
        F32 speedDelta = llclamp(new_speed_adjust - mAdjustedSpeed, -SPEED_ADJUST_MAX_SEC * delta_time, SPEED_ADJUST_MAX_SEC * delta_time);
        mAdjustedSpeed += speedDelta;

        // modulate speed by dot products of facing and velocity
        // so that if we are moving sideways, we slow down the animation
        // and if we're moving backward, we walk backward
        // do this at the end to be more responsive to direction changes instead of in the above speed calculations
        F32 directional_factor = (avatar_movement_dir * world_to_avatar_rot).mV[VX];

        mAnimSpeed = mAdjustedSpeed * directional_factor;
    }
    else
    {   // standing/turning

        // damp out speed adjustment to 0
        mAnimSpeed = LLSmoothInterpolation::lerp(mAnimSpeed, 1.f, 0.2f);
        //mPelvisOffset = lerp(mPelvisOffset, LLVector3::zero, LLSmoothInterpolation::getInterpolant(0.2f));
    }

    // broadcast walk speed change
    mCharacter->setAnimationData("Walk Speed", &mAnimSpeed);

    // set position
    // need to update *some* joint to keep this animation active
    mPelvisState->setPosition(mPelvisOffset);

    return true;
}

//-----------------------------------------------------------------------------
// LLWalkAdjustMotion::onDeactivate()
//-----------------------------------------------------------------------------
void LLWalkAdjustMotion::onDeactivate()
{
    mCharacter->removeAnimationData("Walk Speed");
}

//-----------------------------------------------------------------------------
// LLFlyAdjustMotion::LLFlyAdjustMotion()
//-----------------------------------------------------------------------------
LLFlyAdjustMotion::LLFlyAdjustMotion(const LLUUID &id)
    : LLMotion(id),
      mRoll(0.f)
{
    mName = "fly_adjust";

    mPelvisState = new LLJointState;
}

//-----------------------------------------------------------------------------
// LLFlyAdjustMotion::onInitialize()
//-----------------------------------------------------------------------------
LLMotion::LLMotionInitStatus LLFlyAdjustMotion::onInitialize(LLCharacter *character)
{
    mCharacter = character;

    LLJoint* pelvisJoint = mCharacter->getJoint("mPelvis");
    mPelvisState->setJoint( pelvisJoint );
    if ( !pelvisJoint )
    {
        LL_WARNS() << getName() << ": Can't get pelvis joint." << LL_ENDL;
        return STATUS_FAILURE;
    }

    mPelvisState->setUsage(LLJointState::POS | LLJointState::ROT);
    addJointState( mPelvisState );

    return STATUS_SUCCESS;
}

//-----------------------------------------------------------------------------
// LLFlyAdjustMotion::onActivate()
//-----------------------------------------------------------------------------
bool LLFlyAdjustMotion::onActivate()
{
    mPelvisState->setPosition(LLVector3::zero);
    mPelvisState->setRotation(LLQuaternion::DEFAULT);
    mRoll = 0.f;
    return true;
}

//-----------------------------------------------------------------------------
// LLFlyAdjustMotion::onUpdate()
//-----------------------------------------------------------------------------
bool LLFlyAdjustMotion::onUpdate(F32 time, U8* joint_mask)
{
    LL_PROFILE_ZONE_SCOPED;
    LLVector3 ang_vel = mCharacter->getCharacterAngularVelocity() * mCharacter->getTimeDilation();
    F32 speed = mCharacter->getCharacterVelocity().magVec();

    F32 roll_factor = clamp_rescale(speed, 7.f, 15.f, 0.f, -MAX_ROLL);
    F32 target_roll = llclamp(ang_vel.mV[VZ], -4.f, 4.f) * roll_factor;

    // roll is critically damped interpolation between current roll and angular velocity-derived target roll
    mRoll = LLSmoothInterpolation::lerp(mRoll, target_roll, F32Milliseconds(100.f));

    LLQuaternion roll(mRoll, LLVector3(0.f, 0.f, 1.f));
    mPelvisState->setRotation(roll);

    return true;
}

