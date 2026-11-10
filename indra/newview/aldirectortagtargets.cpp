/**
 * @file aldirectortagtargets.cpp
 * @brief Name-tag highlight and Director group labels (see the header).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectortagtargets.h"

#include "alghoststudio.h"
#include "altron.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llapp.h"          // gDisconnected, LLApp::isExiting
#include "llappviewer.h"
#include "lldirectorcast.h"
#include "llframetimer.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "pipeline.h"

// static
ALDirectorTagTargets& ALDirectorTagTargets::instance()
{
    static ALDirectorTagTargets targets;
    return targets;
}

// static
LLUUID ALDirectorTagTargets::canonicalId(const LLUUID& id)
{
    // The cast stores You under the agent id; null means You wherever a page defaults to self.
    if (id.isNull())
    {
        return gAgentID;
    }
    return LLDirectorCast::isSelf(id) ? gAgentID : LLDirectorCast::canonicalActorId(id);
}

// static
bool ALDirectorTagTargets::sessionAllowsHighlight()
{
    return LLStartUp::getStartupState() == STATE_STARTED && !gDisconnected && !LLApp::isExiting() &&
        gAgent.getTeleportState() == LLAgent::TELEPORT_NONE &&
        gViewerWindow && gViewerWindow->getUIVisibility() && !gAgentCamera.cameraMouselook();
}

bool ALDirectorTagTargets::highlightEnabled() const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "DirectorTagHighlight", true);
    return enabled;
}

void ALDirectorTagTargets::publish(const void* owner, std::vector<LLUUID>& ids, const LLColor4& accent)
{
    if (!sessionAllowsHighlight())
    {
        withdraw(owner);    // logged out, disconnected or teleporting: show nothing, keep nothing
        return;
    }
    // Sticky owner: another Director cannot take over while the owner is still beating.
    if (mOwner && mOwner != owner &&
        ALTagHighlight::heartbeatFresh(LLFrameTimer::getElapsedSeconds(), mBeat,
                                       LLFrameTimer::getFrameCount(), mBeatFrame))
    {
        return;
    }
    mOwner = owner;
    mBeat = LLFrameTimer::getElapsedSeconds();
    mBeatFrame = LLFrameTimer::getFrameCount();
    mIds.assign(ids);
    const ALTagHighlight::TagStyle next = ALTagHighlight::tagStyleFor(accent);
    if (next.background != mStyle.background || next.text != mStyle.text)
    {
        mStyle = next;
        ++mStyleStamp;
    }
}

void ALDirectorTagTargets::withdraw(const void* owner)
{
    if (owner != mOwner)
    {
        return;     // only the publishing Director may withdraw its own highlight
    }
    mBeat = 0.0;
    mIds.clear();
    mOwner = nullptr;
}

void ALDirectorTagTargets::touch(const void* owner)
{
    if (owner == mOwner && mBeat > 0.0)
    {
        mBeat = LLFrameTimer::getElapsedSeconds();
        mBeatFrame = LLFrameTimer::getFrameCount();
    }
}

bool ALDirectorTagTargets::overrideActive(U64 stamp) const
{
    return stamp == mStyleStamp && !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&
        ALTagHighlight::heartbeatFresh(LLFrameTimer::getElapsedSeconds(), mBeat,
                                       LLFrameTimer::getFrameCount(), mBeatFrame);
}

bool ALDirectorTagTargets::isHighlighted(const LLUUID& id)
{
    return !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&
        ALTagHighlight::heartbeatFresh(LLFrameTimer::getElapsedSeconds(), mBeat,
                                       LLFrameTimer::getFrameCount(), mBeatFrame) &&
        mIds.contains(id);
}

bool ALDirectorTagTargets::isHighlighted(const LLUUID& first, const LLUUID& second)
{
    return !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&
        ALTagHighlight::heartbeatFresh(LLFrameTimer::getElapsedSeconds(), mBeat,
                                       LLFrameTimer::getFrameCount(), mBeatFrame) &&
        (mIds.contains(first) || mIds.contains(second));
}

void ALDirectorTagTargets::refreshGroups()
{
    // At most one revision comparison per frame, however many tags ask.
    const U32 frame = LLFrameTimer::getFrameCount();
    if (mGroupFrameSet && frame == mGroupFrame)
    {
        return;
    }
    mGroupFrame = frame;
    mGroupFrameSet = true;
    if (!LLDirectorCast::instanceExists())
    {
        mGroups.invalidate();
        return;
    }
    const LLDirectorCast& cast = LLDirectorCast::instance();
    const ALGhostStudio& studio = ALGhostStudio::instance();
    mGroups.refresh(cast.getCastRevision(), cast.getGroupRevision(), [&cast, &studio](auto add)
    {
        for (const LLDirectorCast::CastMember& member : cast.getCast())
        {
            const LLUUID id = ALDirectorTagTargets::canonicalId(member.mId);
            add(id, member.mGroup);
            // A clone is also known by its Studio id and runtime id (its plate is keyed by them).
            const ALGhostStudio::Instance* clone = studio.getInstance(id);
            if (!clone) clone = studio.findInstanceByRuntime(id);
            if (clone)
            {
                add(clone->mId, member.mGroup);
                add(clone->mEntityId, member.mGroup);
            }
        }
    });
}

const std::string& ALDirectorTagTargets::groupLabel(const LLUUID& id)
{
    refreshGroups();
    return mGroups.label(id);
}

const std::string& ALDirectorTagTargets::groupLabel(const LLUUID& first, const LLUUID& second)
{
    refreshGroups();
    const std::string& label = mGroups.label(first);
    return label.empty() ? mGroups.label(second) : label;
}

namespace
{
// Subject slot as the look pages store it: 0 Self, 1-4 Director Subject A-D. Resolved WITHOUT
// the name refresh the render-time resolvers do, so a UI draw can never dirty the cast or scene.
void addSubjectSlot(std::vector<LLUUID>& out, S32 slot)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLUUID subject;
    switch (slot)
    {
        case 1: subject = cast.getSubjectA(); break;
        case 2: subject = cast.getSubjectB(); break;
        case 3: subject = cast.getSubjectC(); break;
        case 4: subject = cast.getSubjectD(); break;
        case 0: default: break;     // null resolves to You
    }
    if (slot >= 1 && slot <= 4 && subject.isNull())
    {
        return;     // an unassigned slot is nobody (not You)
    }
    LLVOAvatar* avatar = cast.resolve(subject, false);
    if (avatar && !avatar->isDead())
    {
        out.push_back(avatar->getID());
    }
}
}

// static
void ALDirectorTagTargets::appendSubjectTargets(ALTagHighlight::PageTarget kind, std::vector<LLUUID>& out)
{
    switch (kind)
    {
    case ALTagHighlight::PageTarget::ROTO:
    {
        // What the Roto Ink render pass really isolated (subject or capped target set).
        const LLPipeline::DirectorTagRecord& record = gPipeline.getRotoInkTagRecord();
        if (ALTagHighlight::recordFresh(record.mRecorded, LLFrameTimer::getFrameCount(), record.mFrame))
        {
            out.insert(out.end(), record.mIds.begin(), record.mIds.end());
        }
        break;
    }
    case ALTagHighlight::PageTarget::TRON:
    {
        // What Tron's subject mask and trail sampler really selected on their last render.
        const U32 frame = LLFrameTimer::getFrameCount();
        const LLPipeline::DirectorTagRecord& mask = gPipeline.getTronSubjectTagRecord();
        if (ALTagHighlight::recordFresh(mask.mRecorded, frame, mask.mFrame))
        {
            out.insert(out.end(), mask.mIds.begin(), mask.mIds.end());
        }
        const std::vector<LLUUID>* trails = nullptr;
        U32 trail_frame = 0;
        const bool trail_recorded = ALTron::recordedTrailActors(trails, trail_frame);
        if (trails && ALTagHighlight::recordFresh(trail_recorded, frame, trail_frame))
        {
            out.insert(out.end(), trails->begin(), trails->end());
        }
        break;
    }
    case ALTagHighlight::PageTarget::SHADOWS:
    {
        static LLCachedControl<bool> follow(gSavedSettings, "RenderShadowSubjectCascade", false);
        static LLCachedControl<S32> target(gSavedSettings, "RenderShadowSubjectTarget", 0);
        static LLCachedControl<S32> roto_target(gSavedSettings, "CineOutlineSubjectTarget", 0);
        if (follow)
        {
            // 5 = same as Roto Ink's single subject (clamped, as the renderer does).
            addSubjectSlot(out, target == 5 ? llclamp(static_cast<S32>(roto_target), 0, 4)
                                            : llclamp(static_cast<S32>(target), 0, 4));
        }
        break;
    }
    default:
        break;
    }
}
