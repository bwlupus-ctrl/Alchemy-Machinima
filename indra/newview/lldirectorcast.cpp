/**
 * @file lldirectorcast.cpp
 * @brief Director Console engine -- see lldirectorcast.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldirectorcast.h"

#include "llactormover.h"           // startAll/stopAll/placeAt (ACTION, marks)
#include "lleventtimer.h"           // one-shot countdown timer
#include "llflycamrecorder.h"       // armed recorder playback/capture
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewerobjectlist.h"     // gObjectList
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

#include <algorithm>

// ---------------------------------------------------------------------------
LLDirectorCast& LLDirectorCast::instance()
{
    static LLDirectorCast sInstance;
    return sInstance;
}

// ---------------------------------------------------------------------------
// membership
// ---------------------------------------------------------------------------
void LLDirectorCast::add(const LLUUID& id)
{
    if (id.isNull() || contains(id))
    {
        return;
    }
    CastMember member;
    member.mId = id;
    mCast.push_back(member);
    mIds.push_back(id);
    // seed the cached name right away when the actor is in world
    resolve(id);
}

void LLDirectorCast::remove(const LLUUID& id)
{
    auto it = std::find_if(mCast.begin(), mCast.end(),
                           [&id](const CastMember& m) { return m.mId == id; });
    if (it == mCast.end())
    {
        return;
    }
    mCast.erase(it);
    mIds.erase(std::find(mIds.begin(), mIds.end(), id));
    // a subject that leaves the cast stops being a subject
    if (mSubjectA == id)
    {
        mSubjectA.setNull();
    }
    if (mSubjectB == id)
    {
        mSubjectB.setNull();
    }
}

void LLDirectorCast::toggle(const LLUUID& id)
{
    if (id.isNull())
    {
        return;
    }
    contains(id) ? remove(id) : add(id);
}

bool LLDirectorCast::contains(const LLUUID& id) const
{
    return id.notNull()
        && std::find(mIds.begin(), mIds.end(), id) != mIds.end();
}

LLDirectorCast::CastMember* LLDirectorCast::getMember(const LLUUID& id)
{
    for (CastMember& m : mCast)
    {
        if (m.mId == id)
        {
            return &m;
        }
    }
    return nullptr;
}

const LLDirectorCast::CastMember* LLDirectorCast::getMember(const LLUUID& id) const
{
    return const_cast<LLDirectorCast*>(this)->getMember(id);
}

// ---------------------------------------------------------------------------
// resolution
// ---------------------------------------------------------------------------
LLVOAvatar* LLDirectorCast::resolve(const LLUUID& id)
{
    if (id.isNull())
    {
        return isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : nullptr;
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
    if (!av || av->isDead())
    {
        return nullptr;
    }
    // refresh the cached display name whenever one is available, so the
    // cast list can keep labelling an actor who later leaves the region
    if (CastMember* m = getMember(id))
    {
        std::string name = av->getFullname();
        if (!name.empty() && name != m->mLastName)
        {
            m->mLastName = name;
        }
    }
    return av;
}

LLVOAvatar* LLDirectorCast::resolveSubjectA()
{
    return mSubjectA.notNull() ? resolve(mSubjectA) : nullptr;
}

LLVOAvatar* LLDirectorCast::resolveSubjectB()
{
    return mSubjectB.notNull() ? resolve(mSubjectB) : nullptr;
}

// ---------------------------------------------------------------------------
// marks
// ---------------------------------------------------------------------------
void LLDirectorCast::setMarks()
{
    for (CastMember& m : mCast)
    {
        LLVOAvatar* av = resolve(m.mId);
        if (av && av->getRootJoint())
        {
            // current RENDERED root, so a mark taken mid ghost-move (or on
            // a posed actor) captures what the camera actually sees
            m.mMark = av->getRootJoint()->getWorldPosition();
            m.mHasMark = true;
        }
    }
}

void LLDirectorCast::resetToMarks()
{
    for (const CastMember& m : mCast)
    {
        if (m.mHasMark)
        {
            // ActorMover-style local root placement: stops any active move,
            // then pins the rendered root at the mark until the next
            // walk/stop (nothing is sent to the sim)
            LLActorMover::instance().placeAt(m.mId, m.mMark);
        }
    }
}

// ---------------------------------------------------------------------------
// ACTION / CUT
// ---------------------------------------------------------------------------
bool LLDirectorCast::isCountingDown() const
{
    // non-null exactly while the one-shot is pending: the fire lambda nulls
    // it before running, and cancelCountdown() nulls it after deleting
    return mCountdownTimer != nullptr;
}

F32 LLDirectorCast::countdownRemaining() const
{
    if (!isCountingDown())
    {
        return 0.f;
    }
    return llmax(0.f, (F32)(mCountdownEndsAt - LLTimer::getElapsedSeconds().value()));
}

void LLDirectorCast::cancelCountdown()
{
    // safe: the pointer is only non-null while the timer is alive and
    // unfired (see isCountingDown()), so this delete never dangles
    delete mCountdownTimer;
    mCountdownTimer = nullptr;
}

void LLDirectorCast::action()
{
    if (mRunning || isCountingDown())
    {
        return;     // ACTION morphs to CUT in the UI; a second press is CUT
    }

    static LLCachedControl<F32> delay(gSavedSettings, "DirectorActionDelay", 0.f);
    const F32 d = llmax((F32)delay, 0.f);
    if (d > 0.01f)
    {
        // countdown so a solo operator can get into frame; the timer is a
        // self-deleting one-shot (run_after contract), so the lambda MUST
        // null our pointer before anything else -- after tick() returns,
        // updateClass() deletes the timer object
        mCountdownEndsAt = LLTimer::getElapsedSeconds().value() + d;
        mCountdownTimer = LLEventTimer::run_after(d, [this]()
        {
            mCountdownTimer = nullptr;
            fireAction();
        });
        LL_INFOS("DirectorCast") << "ACTION armed, firing in " << d << " s" << LL_ENDL;
    }
    else
    {
        fireAction();
    }
}

void LLDirectorCast::fireAction()
{
    static LLCachedControl<bool> arm_moves(gSavedSettings, "DirectorArmMoves", true);
    static LLCachedControl<bool> arm_camera(gSavedSettings, "DirectorArmCamera", true);
    static LLCachedControl<bool> arm_play(gSavedSettings, "DirectorArmRecorderPlay", false);
    static LLCachedControl<bool> arm_capture(gSavedSettings, "DirectorArmRecorderCapture", false);

    mFiredMoves = false;
    mFiredCamera = false;
    mFiredPlay = false;
    mFiredCapture = false;

    if (arm_moves)
    {
        // start-all-cast (self when the cast is empty), each move capturing
        // the shared parameters at start -- same call the mover floater makes
        LLActorMover::instance().startAll();
        mFiredMoves = true;
    }
    if (arm_camera)
    {
        // enable the cinematic camera; orbit is untouched (it rides its own
        // enable + resolveAnchor()). Remember the prior state so CUT
        // restores it instead of hammering it off.
        mCameraWasEnabled = gSavedSettings.getBOOL("CinematicCamEnabled");
        if (!mCameraWasEnabled)
        {
            gSavedSettings.setBOOL("CinematicCamEnabled", true);
        }
        mFiredCamera = true;
    }
    if (arm_capture)
    {
        // capture wins when both recorder arms are set: playback would be
        // recording its own output over the take it is playing
        if (arm_play)
        {
            LL_WARNS("DirectorCast") << "recorder play AND capture armed; capturing"
                                     << LL_ENDL;
        }
        LLFlycamRecorder::instance().startRecording();
        mFiredCapture = true;
    }
    else if (arm_play)
    {
        LLFlycamRecorder::instance().startPlayback();
        mFiredPlay = true;
    }

    mRunning = true;
    LL_INFOS("DirectorCast") << "ACTION: moves " << mFiredMoves
                             << " camera " << mFiredCamera
                             << " play " << mFiredPlay
                             << " capture " << mFiredCapture << LL_ENDL;
}

void LLDirectorCast::cut()
{
    cancelCountdown();
    if (!mRunning)
    {
        return;
    }
    if (mFiredMoves)
    {
        LLActorMover::instance().stopAll();
    }
    if (mFiredCamera && !mCameraWasEnabled)
    {
        // only un-enable what ACTION enabled: a camera the operator had
        // running before ACTION keeps running after CUT
        gSavedSettings.setBOOL("CinematicCamEnabled", false);
    }
    if (mFiredPlay)
    {
        LLFlycamRecorder::instance().stopPlayback();
    }
    if (mFiredCapture)
    {
        LLFlycamRecorder::instance().stopRecording();
    }
    mRunning = false;
    LL_INFOS("DirectorCast") << "CUT" << LL_ENDL;
}
