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
#include "llsdutil_math.h"          // ll_sd_from_vector3 (scene marks)
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
// groups
// ---------------------------------------------------------------------------
void LLDirectorCast::setGroup(const LLUUID& id, const std::string& name)
{
    if (CastMember* m = getMember(id))
    {
        m->mGroup = name;   // "" = ungroup; no registry to keep in sync
    }
}

std::string LLDirectorCast::getGroup(const LLUUID& id) const
{
    const CastMember* m = getMember(id);
    return m ? m->mGroup : std::string();
}

std::vector<std::string> LLDirectorCast::getGroupNames() const
{
    // first-appearance cast order (NOT sorted): a combo rebuilt from this list
    // keeps its rows where the operator last saw them
    std::vector<std::string> names;
    for (const CastMember& m : mCast)
    {
        if (!m.mGroup.empty()
            && std::find(names.begin(), names.end(), m.mGroup) == names.end())
        {
            names.push_back(m.mGroup);
        }
    }
    return names;
}

uuid_vec_t LLDirectorCast::membersInGroup(const std::string& name) const
{
    uuid_vec_t ids;
    if (name.empty())
    {
        return ids;     // "" means ungrouped, never a startable group
    }
    for (const CastMember& m : mCast)
    {
        if (m.mGroup == name)
        {
            ids.push_back(m.mId);
        }
    }
    return ids;
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

bool LLDirectorCast::setMark(const LLUUID& id)
{
    CastMember* m = getMember(id);
    if (!m)
    {
        return false;   // only cast members carry marks
    }
    LLVOAvatar* av = resolve(id);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    // same capture as setMarks(): the current RENDERED root
    m->mMark = av->getRootJoint()->getWorldPosition();
    m->mHasMark = true;
    return true;
}

bool LLDirectorCast::resetToMark(const LLUUID& id)
{
    const CastMember* m = getMember(id);
    if (!m || !m->mHasMark)
    {
        return false;
    }
    LLActorMover::instance().placeAt(id, m->mMark);
    return true;
}

// ---------------------------------------------------------------------------
// scene serialization
// ---------------------------------------------------------------------------
LLSD LLDirectorCast::sceneData() const
{
    LLSD data = LLSD::emptyMap();
    LLSD cast_arr = LLSD::emptyArray();
    for (const CastMember& m : mCast)
    {
        LLSD e = LLSD::emptyMap();
        e["id"] = m.mId;
        e["name"] = m.mLastName;
        e["has_mark"] = m.mHasMark;
        if (m.mHasMark)
        {
            e["mark"] = ll_sd_from_vector3(m.mMark);
        }
        e["loco_anim"] = m.mLocoAnim;
        e["group"] = m.mGroup;
        cast_arr.append(e);
    }
    data["cast"] = cast_arr;
    data["subject_a"] = mSubjectA;
    data["subject_b"] = mSubjectB;
    return data;
}

void LLDirectorCast::applySceneData(const LLSD& data)
{
    // replace membership wholesale; a running transport is the caller's
    // problem (the console cuts before loading)
    mCast.clear();
    mIds.clear();
    mSubjectA.setNull();
    mSubjectB.setNull();

    const LLSD& cast_arr = data["cast"];
    for (LLSD::array_const_iterator it = cast_arr.beginArray();
         it != cast_arr.endArray(); ++it)
    {
        const LLSD& e = *it;
        CastMember m;
        m.mId = e["id"].asUUID();
        if (m.mId.isNull() || contains(m.mId))
        {
            continue;
        }
        m.mLastName = e["name"].asString();
        m.mHasMark = e["has_mark"].asBoolean();
        if (m.mHasMark && e.has("mark"))
        {
            m.mMark = ll_vector3_from_sd(e["mark"]);
        }
        m.mLocoAnim = e["loco_anim"].asUUID();
        m.mGroup = e["group"].asString();   // absent in pre-group scenes -> ""
        mCast.push_back(m);
        mIds.push_back(m.mId);
        // refresh the cached name when the actor is in world; a resolve
        // failure just leaves the member "(away)" -- never dropped
        resolve(m.mId);
    }

    // subjects only survive when they point into the loaded cast
    const LLUUID a = data["subject_a"].asUUID();
    const LLUUID b = data["subject_b"].asUUID();
    if (contains(a))
    {
        mSubjectA = a;
    }
    if (contains(b))
    {
        mSubjectB = b;
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
