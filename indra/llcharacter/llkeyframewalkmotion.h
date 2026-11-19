/**
 * @file llkeyframewalkmotion.h
 * @brief Implementation of LLKeframeWalkMotion class.
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

#ifndef LL_LLKEYFRAMEWALKMOTION_H
#define LL_LLKEYFRAMEWALKMOTION_H

//-----------------------------------------------------------------------------
// Header files
//-----------------------------------------------------------------------------
#include "llkeyframemotion.h"
#include "llcharacter.h"
#include "v3dmath.h"
#include <map>

#define MIN_REQUIRED_PIXEL_AREA_WALK_ADJUST (20.f)
#define MIN_REQUIRED_PIXEL_AREA_FLY_ADJUST (20.f)

//-----------------------------------------------------------------------------
// class LLKeyframeWalkMotion
//-----------------------------------------------------------------------------
class LLKeyframeWalkMotion :
    public LLKeyframeMotion
{
    friend class LLWalkAdjustMotion;
public:
    // Constructor
    LLKeyframeWalkMotion(const LLUUID &id);

    // Destructor
    virtual ~LLKeyframeWalkMotion();

public:
    //-------------------------------------------------------------------------
    // functions to support MotionController and MotionRegistry
    //-------------------------------------------------------------------------

    // static constructor
    // all subclasses must implement such a function and register it
    static LLMotion *create(const LLUUID &id) { return new LLKeyframeWalkMotion(id); }

public:
    //-------------------------------------------------------------------------
    // animation callbacks to be implemented by subclasses
    //-------------------------------------------------------------------------
    virtual LLMotionInitStatus onInitialize(LLCharacter *character);
    virtual bool onActivate();
    virtual void onDeactivate();
    virtual bool onUpdate(F32 time, U8* joint_mask);

public:
    // [PosePolish M3] Phase-aware locomotion master+feature gate.
    // docs/pose_polish_integration_plan.md "Milestone 3 -- Phase-aware locomotion".
    //
    // llcharacter has no access to gSavedSettings/LLControlGroup (verified: no such
    // usage anywhere under indra/llcharacter). The one precedent for feeding a viewer
    // setting into this library is LLHeadRotMotion/LLEyeMotion's static-setter "push"
    // pattern (indra/llcharacter/llheadrotmotion.h:104-108,198-202, wired per-frame
    // from indra/newview/llappviewer.cpp:5130-5136). This mirrors that pattern:
    // defaults to false, so unless/until a newview caller pushes
    // (ALPolishEnabled && ALPolishPhaseMatchEnabled) in every frame, this stays off
    // and onActivate()/onUpdate() below take the exact stock code path.
    static void setPhaseAwareLocomotionEnabled(bool enabled) { sPhaseAwareLocomotionEnabled = enabled; }
    static void resetPhaseAwareLocomotionEnabled() { sPhaseAwareLocomotionEnabled = false; }

public:
    //-------------------------------------------------------------------------
    // Member Data
    //-------------------------------------------------------------------------
    LLCharacter *mCharacter;
    F32         mCyclePhase;
    F32         mRealTimeLast;
    F32         mAdjTimeLast;
    S32         mDownFoot;

private:
    //-------------------------------------------------------------------------
    // [PosePolish M3] Phase-aware locomotion start offset.
    //
    // Generalizes the cadence adjuster above: rather than replacing the per-frame
    // time warp (mAdjTimeLast / SPEED_ADJUST_TIME_CONSTANT / SPEED_ADJUST_MAX_SEC),
    // this only chooses a better *seed* for mAdjTimeLast at onActivate() so a fresh
    // walk/run clip begins near a compatible foot-plant phase instead of always at
    // time 0. It never touches the per-frame rate, so it cannot look sped up; the
    // seed is bounded to within a single stride by construction (see .cpp).
    //-------------------------------------------------------------------------

    // Per-clip (per-asset) cached plant-phase data. Computed once, lazily, the
    // first time it's needed for a given animation asset; reused after that.
    struct LLLocomotionPhaseInfo
    {
        bool    mValid;             // reliable alternating L/R plant windows were found
        F32     mCycleStart;        // seconds; = mLoopInPoint
        F32     mCycleLength;       // seconds; = mLoopOutPoint - mLoopInPoint, > 0 when valid
        F32     mLeftPlantPhase;    // 0..1, fraction of cycle at left-foot plant-window center
        F32     mRightOffset;       // 0..1, (right plant phase - left plant phase), wrapped

        LLLocomotionPhaseInfo()
        :   mValid(false), mCycleStart(0.f), mCycleLength(0.f),
            mLeftPlantPhase(0.f), mRightOffset(0.5f)
        {}
    };

    // Per-character handoff: the last gait phase written by *any* active,
    // phase-aware walk/run motion on this character, expressed in a clip-
    // independent "canonical" phase (0 == a left-foot plant), plus a
    // timestamp so a stale or unrelated record is never reused, the writer's
    // own right-foot offset (so a reader can check both feet are compatible,
    // not just the left), and an identity of the writing motion instance (so
    // onDeactivate() can tell whether it is still the record's owner -- see
    // onDeactivate()/updateLocomotionHandoff() in the .cpp for why this
    // matters: a stale record must not outlive the clip that wrote it, but a
    // genuine clip-to-clip handoff must not be erased out from under the
    // motion that has since taken over).
    struct LLLocomotionHandoff
    {
        F32     mCanonicalPhase;   // 0..1, left-plant-anchored
        F32     mRightOffset;      // 0..1, writer's own mRightOffset at time of write
        F64     mUpdateTime;       // LLFrameTimer::getElapsedSeconds() at last write
        const LLKeyframeWalkMotion* mLastWriter; // identity of the motion that wrote this record

        LLLocomotionHandoff() : mCanonicalPhase(0.f), mRightOffset(0.5f), mUpdateTime(-1.0), mLastWriter(NULL) {}
    };

    const LLLocomotionPhaseInfo& getLocomotionPhaseInfo();
    void updateLocomotionHandoff(F32 adjusted_time);
    bool computeLocomotionStartOffset(F32& start_time_out);

    static bool sPhaseAwareLocomotionEnabled;
    static std::map<LLUUID, LLLocomotionPhaseInfo> sPhaseInfoCache;
    static std::map<const LLCharacter*, LLLocomotionHandoff> sLocomotionHandoff;
};

class LLWalkAdjustMotion : public LLMotion
{
public:
    // Constructor
    LLWalkAdjustMotion(const LLUUID &id);

public:
    //-------------------------------------------------------------------------
    // functions to support MotionController and MotionRegistry
    //-------------------------------------------------------------------------

    // static constructor
    // all subclasses must implement such a function and register it
    static LLMotion *create(const LLUUID &id) { return new LLWalkAdjustMotion(id); }

public:
    //-------------------------------------------------------------------------
    // animation callbacks to be implemented by subclasses
    //-------------------------------------------------------------------------
    virtual LLMotionInitStatus onInitialize(LLCharacter *character);
    virtual bool onActivate();
    virtual void onDeactivate();
    virtual bool onUpdate(F32 time, U8* joint_mask);
    virtual LLJoint::JointPriority getPriority(){return LLJoint::HIGH_PRIORITY;}
    virtual bool getLoop() { return true; }
    virtual F32 getDuration() { return 0.f; }
    virtual F32 getEaseInDuration() { return 0.f; }
    virtual F32 getEaseOutDuration() { return 0.f; }
    virtual F32 getMinPixelArea() { return MIN_REQUIRED_PIXEL_AREA_WALK_ADJUST; }
    virtual LLMotionBlendType getBlendType() { return ADDITIVE_BLEND; }

public:
    //-------------------------------------------------------------------------
    // Member Data
    //-------------------------------------------------------------------------
    LLCharacter     *mCharacter;
    LLJoint*        mLeftAnkleJoint;
    LLJoint*        mRightAnkleJoint;
    LLPointer<LLJointState> mPelvisState;
    LLJoint*        mPelvisJoint;
    LLVector3d      mLastLeftFootGlobalPos;
    LLVector3d      mLastRightFootGlobalPos;
    F32             mLastTime;
    F32             mAdjustedSpeed;
    F32             mAnimSpeed;
    F32             mRelativeDir;
    LLVector3       mPelvisOffset;
    F32             mAnkleOffset;
};

class LLFlyAdjustMotion : public LLMotion
{
public:
    // Constructor
    LLFlyAdjustMotion(const LLUUID &id);

public:
    //-------------------------------------------------------------------------
    // functions to support MotionController and MotionRegistry
    //-------------------------------------------------------------------------

    // static constructor
    // all subclasses must implement such a function and register it
    static LLMotion *create(const LLUUID &id) { return new LLFlyAdjustMotion(id); }

public:
    //-------------------------------------------------------------------------
    // animation callbacks to be implemented by subclasses
    //-------------------------------------------------------------------------
    virtual LLMotionInitStatus onInitialize(LLCharacter *character);
    virtual bool onActivate();
    virtual void onDeactivate() {};
    virtual bool onUpdate(F32 time, U8* joint_mask);
    virtual LLJoint::JointPriority getPriority(){return LLJoint::HIGHER_PRIORITY;}
    virtual bool getLoop() { return true; }
    virtual F32 getDuration() { return 0.f; }
    virtual F32 getEaseInDuration() { return 0.f; }
    virtual F32 getEaseOutDuration() { return 0.f; }
    virtual F32 getMinPixelArea() { return MIN_REQUIRED_PIXEL_AREA_FLY_ADJUST; }
    virtual LLMotionBlendType getBlendType() { return ADDITIVE_BLEND; }

protected:
    //-------------------------------------------------------------------------
    // Member Data
    //-------------------------------------------------------------------------
    LLCharacter     *mCharacter;
    LLPointer<LLJointState> mPelvisState;
    F32             mRoll;
};

#endif // LL_LLKeyframeWalkMotion_H

