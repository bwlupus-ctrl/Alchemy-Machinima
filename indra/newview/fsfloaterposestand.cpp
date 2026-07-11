/*
 * @file fsfloaterposestand.cpp
 * @brief It's a pose stand!
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder.roxley@phoenixviewer.com>
 * ----------------------------------------------------------------------------
 *
 * [BDMerge F6] Ported from Firestorm fork (I:\enve\indra\newview\fsfloaterposestand.cpp),
 * BD/FS -> Alchemy merge campaign, item F6 (Pose Stand + Undeform, portable half).
 * Movelock excluded per board ruling (FS's LSL-bridge dependent, non-portable).
 *
 * Adaptations vs donor:
 *  - AO pause/resume remapped from FS's gSavedPerAccountSettings "UseAO" toggle
 *    onto Alchemy's AOEngine: gSavedPerAccountSettings "PauseAO" (a dedicated
 *    pause switch AOEngine::onPauseAO() already listens for), gated on
 *    "AlchemyAOEnable" (Alchemy's AO master-enable key). This pauses the AO
 *    without disturbing the user's AO-enabled preference, unlike the donor's
 *    approach of flipping the enable flag itself.
 *  - gAgent.stopCurrentAnimations() takes no arguments in Alchemy's LLAgent
 *    (donor calls stopCurrentAnimations(true) - FS-only overload); the
 *    force_keep_script_perms argument was dropped accordingly.
 */

#include "llviewerprecompiledheaders.h"
#include "fsfloaterposestand.h"

#include "fspose.h"
#include "llagent.h"
#include "llvoavatarself.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "rlvhandler.h"

FSFloaterPoseStand::FSFloaterPoseStand(const LLSD& key)
:   LLFloater(key),
    mComboPose(nullptr),
    mPoseStandLock(false),
    mAOPaused(false)
{
}

bool FSFloaterPoseStand::postBuild()
{
    mComboPose = getChild<LLComboBox>("pose_combo");
    mComboPose->setCommitCallback(boost::bind(&FSFloaterPoseStand::onCommitCombo, this));
    loadPoses();

    return true;
}

// virtual
void FSFloaterPoseStand::onOpen(const LLSD& key)
{
    if (!isAgentAvatarValid())
    {
        return;
    }

    // [BDMerge F6] AO pause: Alchemy's AOEngine listens for "PauseAO" (see
    // aoengine.cpp AOEngine::onPauseAO()) and pauses/resumes without touching
    // the master "AlchemyAOEnable" preference. Only pause if the AO is
    // actually enabled and not already paused.
    if (gSavedPerAccountSettings.getBOOL("AlchemyAOEnable")
        && !gSavedPerAccountSettings.getBOOL("PauseAO"))
    {
        gSavedPerAccountSettings.setBOOL("PauseAO", true);
        mAOPaused = true;
    }

    if (gSavedSettings.getBOOL("FSPoseStandLock")
        && !gAgentAvatarp->isSitting()
        && !gRlvHandler.hasBehaviour(RLV_BHVR_SIT))
    {
        setLock(true);
    }
    gAgent.stopCurrentAnimations();
    gAgent.setCustomAnim(true);
    gFocusMgr.setKeyboardFocus(nullptr);
    gFocusMgr.setMouseCapture(nullptr);
    std::string last_pose = gSavedSettings.getString("FSPoseStandLastSelectedPose");
    if (!last_pose.empty())
    {
        mComboPose->setSelectedByValue(last_pose, true);
    }
    onCommitCombo();
}

// virtual
void FSFloaterPoseStand::onClose(bool app_quitting)
{
    if (!isAgentAvatarValid())
    {
        return;
    }

    if (mPoseStandLock && gAgentAvatarp->isSitting())
    {
        setLock(false);
        gAgent.standUp();
    }
    gAgent.setCustomAnim(false);
    FSPose::getInstance()->stopPose();
    gAgent.stopCurrentAnimations();
    // [BDMerge F6] AO resume: only clear the pause we set (mirrors donor's
    // "only restore what we paused" guard).
    if (mAOPaused && gSavedPerAccountSettings.getBOOL("PauseAO"))
    {
        gSavedPerAccountSettings.setBOOL("PauseAO", false);
        mAOPaused = false;
    }
}

void FSFloaterPoseStand::loadPoses()
{
    const std::string pose_filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "posestand.xml");
    llifstream pose_file(pose_filename.c_str());
    LLSD poses;
    if (pose_file.is_open())
    {
        if (LLSDSerialize::fromXML(poses, pose_file) >= 1)
        {
            for (LLSD::map_iterator p_itr = poses.beginMap(); p_itr != poses.endMap(); ++p_itr)
            {
                LLUUID anim_id(p_itr->first);
                if (anim_id.notNull())
                {
                    mComboPose->add(LLTrans::getString(p_itr->second["name"].asStringRef()), anim_id);
                }
            }
        }
        pose_file.close();
    }
    mComboPose->sortByName();
}

void FSFloaterPoseStand::onCommitCombo()
{
    std::string selected_pose = mComboPose->getValue();
    gSavedSettings.setString("FSPoseStandLastSelectedPose", selected_pose);
    FSPose::getInstance()->setPose(selected_pose);
}

void FSFloaterPoseStand::setLock(bool enabled)
{
    if (enabled)
    {
        gAgent.sitDown();
    }
    else
    {
        gAgent.standUp();
    }
    mPoseStandLock = enabled;
}
