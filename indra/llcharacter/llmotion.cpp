/**
 * @file llmotion.cpp
 * @brief Implementation of LLMotion class.
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

#include "llmotion.h"
#include "llcriticaldamp.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLMotion class
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// LLMotion()
// Class Constructor
//-----------------------------------------------------------------------------
LLMotion::LLMotion( const LLUUID &id ) :
    mStopped(true),
    mActive(false),
    mID(id),
    mActivationTimestamp(0.f),
    mStopTimestamp(0.f),
    mSendStopTimestamp(F32_MAX),
    mResidualWeight(0.f),
    mFadeWeight(1.f),
    mPriorityOverride(-1),
    mDeactivateCallback(nullptr),
    mDeactivateCallbackUserData(nullptr)
{
    for (S32 i=0; i<3; ++i)
        memset(&mJointSignature[i][0], 0, sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS);
}

//-----------------------------------------------------------------------------
// ~LLMotion()
// Class Destructor
//-----------------------------------------------------------------------------
LLMotion::~LLMotion()
{
}

//-----------------------------------------------------------------------------
// fadeOut()
//-----------------------------------------------------------------------------
void LLMotion::fadeOut()
{
    if (mFadeWeight > 0.01f)
    {
        mFadeWeight = lerp(mFadeWeight, 0.f, LLSmoothInterpolation::getInterpolant(0.15f));
    }
    else
    {
        mFadeWeight = 0.f;
    }
}

//-----------------------------------------------------------------------------
// fadeIn()
//-----------------------------------------------------------------------------
void LLMotion::fadeIn()
{
    if (mFadeWeight < 0.99f)
    {
        mFadeWeight = lerp(mFadeWeight, 1.f, LLSmoothInterpolation::getInterpolant(0.15f));
    }
    else
    {
        mFadeWeight = 1.f;
    }
}

//-----------------------------------------------------------------------------
// addJointState()
//-----------------------------------------------------------------------------
void LLMotion::addJointState(const LLPointer<LLJointState>& jointState)
{
    mPose.addJointState(jointState);
    // Honor any per-instance priority override here too, so the joint signature
    // this builds agrees with what the pose blender will use. With no override
    // this is the stock rule (USE_MOTION_PRIORITY -> motion base priority).
    S32 priority = getJointPriority(jointState);

    U32 usage = jointState->getUsage();

    // for now, usage is everything
    S32 joint_num = jointState->getJoint()->getJointNum();
    if ((joint_num >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS) || (joint_num < 0))
    {
        LL_WARNS() << "joint_num " << joint_num << " is outside of legal range [0-" << LL_CHARACTER_MAX_ANIMATED_JOINTS << ") for joint " << jointState->getJoint()->getName() << LL_ENDL;
        return;
    }
    mJointSignature[0][joint_num] = (usage & LLJointState::POS) ? (0xff >> (7 - priority)) : 0;
    mJointSignature[1][joint_num] = (usage & LLJointState::ROT) ? (0xff >> (7 - priority)) : 0;
    mJointSignature[2][joint_num] = (usage & LLJointState::SCALE) ? (0xff >> (7 - priority)) : 0;
}

//-----------------------------------------------------------------------------
// setPriorityOverride()
//-----------------------------------------------------------------------------
void LLMotion::setPriorityOverride(S32 priority)
{
    if (priority == mPriorityOverride)
    {
        return;
    }
    mPriorityOverride = priority;

    // Rebuild THIS instance's joint signature so the motion controller's
    // per-joint update/mask logic agrees with the overridden blend priority.
    // Both mPose and mJointSignature are per-instance; the shared, per-asset
    // JointMotionList (cached in LLKeyframeDataCache) is never touched here, so
    // other avatars playing the same asset are unaffected. If the asset has not
    // finished loading yet mPose is empty and this loop is a no-op -- the
    // signature is then (re)built with the override honored when the joint
    // states are added in setupPose()/addJointState().
    for (LLJointState* jsp = mPose.getFirstJointState(); jsp; jsp = mPose.getNextJointState())
    {
        LLJoint* joint = jsp->getJoint();
        if (!joint)
        {
            continue;
        }
        S32 joint_num = joint->getJointNum();
        if ((joint_num < 0) || (joint_num >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS))
        {
            continue;
        }
        U32 usage = jsp->getUsage();
        // keep the signature shift well-defined for any override value
        S32 pri = llclamp((S32)getJointPriority(jsp), (S32)LLJoint::LOW_PRIORITY, (S32)LL_CHARACTER_MAX_PRIORITY);
        mJointSignature[0][joint_num] = (usage & LLJointState::POS)   ? (U8)(0xff >> (7 - pri)) : 0;
        mJointSignature[1][joint_num] = (usage & LLJointState::ROT)   ? (U8)(0xff >> (7 - pri)) : 0;
        mJointSignature[2][joint_num] = (usage & LLJointState::SCALE) ? (U8)(0xff >> (7 - pri)) : 0;
    }
}

void LLMotion::setDeactivateCallback( void (*cb)(void *), void* userdata )
{
    mDeactivateCallback = cb;
    mDeactivateCallbackUserData = userdata;
}

//virtual
void LLMotion::setStopTime(F32 time)
{
    mStopTimestamp = time;
    mStopped = true;
}

bool LLMotion::isBlending()
{
    return mPose.getWeight() < 1.f;
}

//-----------------------------------------------------------------------------
// activate()
//-----------------------------------------------------------------------------
void LLMotion::activate(F32 time)
{
    mActivationTimestamp = time;
    mStopped = false;
    mActive = true;
    onActivate();
}

//-----------------------------------------------------------------------------
// deactivate()
//-----------------------------------------------------------------------------
void LLMotion::deactivate()
{
    mActive = false;
    mPose.setWeight(0.f);

    // A client-side priority override does not survive a stop: clear it and
    // restore the baked joint signature so a later replay of this instance
    // starts from the asset's real priority unless the caller re-applies one.
    if (mPriorityOverride >= 0)
    {
        setPriorityOverride(-1);
    }

    if (mDeactivateCallback)
    {
        (*mDeactivateCallback)(mDeactivateCallbackUserData);
        mDeactivateCallback = NULL; // only call callback once
        mDeactivateCallbackUserData = NULL;
    }

    onDeactivate();
}

bool LLMotion::canDeprecate()
{
    return true;
}

// End

