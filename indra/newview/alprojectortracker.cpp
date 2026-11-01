#include "llviewerprecompiledheaders.h"
#include "alprojectortracker.h"
#include "llagent.h"
#include "lldirectorcast.h"
#include "lldrawable.h"
#include "lljoint.h"
#include "llprismlens.h"
#include "llselectmgr.h"
#include "lltimer.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include "pipeline.h"
using namespace ALProjectorTrackMath;
namespace
{
// Match Path Object's placement write and moved-child traversal. Resolve the
// changed drawables immediately as well: this tracker runs AFTER the normal
// moved list, so parent render transforms are current before child-local math.
void markOne(LLDrawable* drawable)
{
    if (!drawable || drawable->isDead()) return;
    gPipeline.markMoved(drawable, false);
    drawable->clearState(LLDrawable::EARLY_MOVE);
    gPipeline.updateMoveNormalAsync(drawable);
}
void markMoved(LLViewerObject* object)
{
    markOne(object->mDrawable);
    for (LLViewerObject* child : object->getChildren())
        if (child) markOne(child->mDrawable);
}
LLViewerObject* parentOf(LLViewerObject* object)
{
    return static_cast<LLViewerObject*>(object->getParent());
}
void place(LLViewerObject* object, const Pose& world)
{
    if (auto* parent = parentOf(object))
    {
        const Pose local = relative(world, {parent->getRenderPosition(), parent->getRenderRotation()});
        object->setPosition(local.position);
        object->setRotation(local.rotation);
    }
    else
    {
        object->setPositionAgent(world.position);
        object->setRotation(world.rotation);
    }
    markMoved(object);
}
}
ALProjectorTracker& ALProjectorTracker::instance()
{
    static ALProjectorTracker tracker;
    return tracker;
}
const ALProjectorTracker::State& ALProjectorTracker::state(const LLUUID& id) const
{
    static const State empty;
    const auto it = mStates.find(id);
    return it == mStates.end() ? empty : it->second;
}
const char* ALProjectorTracker::jointName(S32 index)
{
    static const char* names[] = {"mHead", "mChest", "mPelvis", "mWristLeft",
        "mWristRight", "mAnkleLeft", "mAnkleRight"};
    return names[llclamp(index, 0, 6)];
}
bool ALProjectorTracker::eligible(LLViewerObject* object)
{
    auto* volume = dynamic_cast<LLVOVolume*>(object);
    return volume && !object->isDead() && !object->isAttachment() &&
        !object->isHUDAttachment() && !object->isLocalOnly() &&
        (volume->isLightSpotlight() || LLPrismLens::hasProjectorBinding(object->getID()));
}
bool ALProjectorTracker::serverEditTransform(LLViewerObject* object, Pose& pose) const
{
    if (!object || (!tracked(object->getID()) && !tracked(object->getRootEdit()->getID()))) return false;
    pose = tracked(object->getID()) ? state(object->getID()).server
                                   : Pose{object->getPosition(), object->getRotation()};
    for (auto* parent = parentOf(object); parent; parent = parentOf(parent))
    {
        const Pose frame = tracked(parent->getID()) ? state(parent->getID()).server
                          : Pose{parent->getPosition(), parent->getRotation()};
        pose = apply(pose, frame);
    }
    return true;
}
void ALProjectorTracker::configure(const LLUUID& id, const LLUUID& subject,
    bool has_subject, S32 joint, Mode mode, F32 smoothing)
{
    auto& s = mStates[id];
    if (s.subject != subject || s.joint != joint || s.mode != mode) s.captured = false;
    s.subject = subject;
    s.hasSubject = has_subject;
    s.joint = llclamp(joint, 0, 6);
    s.mode = mode;
    s.smoothing = llclamp(smoothing, 0.f, 1.f);
    if (!has_subject) stop(id, "Choose a subject");
    ++mRevision;
}
void ALProjectorTracker::restore(LLViewerObject* object, const State& s)
{
    if (!object || object->isDead()) return;
    // A relink changes the coordinate frame. Never apply an old local snapshot
    // to a different parent; processUpdateCore will capture the new frame.
    const auto* parent = parentOf(object);
    if (object->getRegion() != s.region || (parent ? parent->getID() : LLUUID::null) != s.parent) return;
    object->setPosition(s.server.position);
    object->setRotation(s.server.rotation);
    markMoved(object);
}
void ALProjectorTracker::stop(const LLUUID& id, const std::string& status)
{
    auto it = mStates.find(id);
    if (it == mStates.end()) return;
    if (mActive.erase(id)) restore(gObjectList.findObject(id), it->second);
    it->second.enabled = false;
    it->second.status = status;
    ++mRevision;
}
void ALProjectorTracker::enable(const LLUUID& id, bool enabled)
{
    if (!enabled) { stop(id, "Track off"); return; }
    auto* object = gObjectList.findObject(id);
    auto& s = mStates[id];
    if (!eligible(object) || !s.hasSubject) return;
    if (tracked(id)) return;
    s.server = {object->getPosition(), object->getRotation()};
    object->getLastServerTransform(s.server.position, s.server.rotation);
    s.current = {object->getRenderPosition(), object->getRenderRotation()};
    const auto* parent = parentOf(object);
    s.parent = parent ? parent->getID() : LLUUID::null;
    s.region = object->getRegion();
    s.captured = false;
    s.missingSince = 0.;
    s.enabled = true;
    s.status = "Tracking";
    mActive.insert(id);
    mRegion = gAgent.getRegion();
    ++mRevision;
}
void ALProjectorTracker::capture(const LLUUID& id)
{
    auto it = mStates.find(id);
    if (it == mStates.end() || it->second.mode != FOLLOW) return;
    auto& s = it->second;
    auto* object = gObjectList.findObject(id);
    auto* avatar = s.hasSubject ? LLDirectorCast::instance().resolve(s.subject) : nullptr;
    LLJoint* joint = avatar ? avatar->getJoint(jointName(s.joint)) : nullptr;
    if (!object || !joint || !joint->getWorldPosition().isFinite()) return;
    s.current = {object->getRenderPosition(), object->getRenderRotation()};
    s.offset = relative(s.current, {joint->getWorldPosition(), avatar->getRenderRotation()});
    s.captured = true;
}
void ALProjectorTracker::beforeObjectUpdate(LLViewerObject* object)
{
    if (tracked(object->getID())) restore(object, mStates.at(object->getID()));
}
void ALProjectorTracker::receivedServerPose(LLViewerObject* object, const LLVector3& position, const LLQuaternion& rotation)
{
    if (!tracked(object->getID()) || object->isDead() || !position.isFinite() || !rotation.isFinite()) return;
    auto& s = mStates.at(object->getID());
    s.server = {position, rotation};
    const auto* parent = parentOf(object);
    s.parent = parent ? parent->getID() : LLUUID::null;
    s.region = object->getRegion();
}
void ALProjectorTracker::pauseObjectForEdit(LLViewerObject* object)
{
    if (!object) return;
    for (auto it = mActive.begin(); it != mActive.end();)
    {
        const LLUUID id = *it++;
        auto* tracked_object = gObjectList.findObject(id);
        if (tracked_object && tracked_object->getRootEdit() == object->getRootEdit())
            stop(id, "Paused for Build edit; re-enable Track to resume");
    }
}
bool ALProjectorTracker::pauseSelectionForEdit()
{
    bool paused = false;
    auto selection = LLSelectMgr::getInstance()->getSelection();
    for (auto it = mActive.begin(); it != mActive.end();)
    {
        const LLUUID id = *it++;
        auto* object = gObjectList.findObject(id);
        if (!object) continue;
        auto* root = object->getRootEdit();
        bool selected = false;
        for (auto node = selection->begin(); node != selection->end(); ++node)
        {
            auto* candidate = (*node)->getObject();
            if (candidate && candidate->getRootEdit() == root) { selected = true; break; }
        }
        if (selected) { stop(id, "Paused for Build edit; re-enable Track to resume"); paused = true; }
    }
    if (paused)
    {
        // Clear edit override caches before the selection manager can reapply
        // a previous preview. Reset the baseline without triggering another edit.
        for (auto node = selection->begin(); node != selection->end(); ++node)
        {
            (*node)->mLastPositionLocal.clearVec();
            (*node)->mLastRotation = LLQuaternion();
        }
        LLSelectMgr::getInstance()->updateSelectionCenter();
        LLSelectMgr::getInstance()->saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
    }
    return paused;
}
void ALProjectorTracker::drop(const LLUUID& id)
{
    mActive.erase(id);
    if (mStates.erase(id)) ++mRevision;
}
void ALProjectorTracker::clear()
{
    if (mStates.empty() && mActive.empty()) return;
    while (!mActive.empty())
    {
        const LLUUID id = *mActive.begin();
        stop(id, "Track off");
    }
    mStates.clear();
    mRegion = nullptr;
    ++mRevision;
}
void ALProjectorTracker::update(F32 dt)
{
    if (mActive.empty()) return;
    if (mRegion != gAgent.getRegion()) { clear(); return; }
    const F64 now = LLTimer::getTotalSeconds();
    // Linksets are flat. Place roots before tracked children so a child uses
    // this frame's final rendered parent, even when both lights are tracked.
    for (S32 pass = 0; pass != 2; ++pass)
    for (auto it = mActive.begin(); it != mActive.end();)
    {
        const LLUUID id = *it++;
        auto* object = gObjectList.findObject(id);
        if (object && !object->isDead() && (object->isRootEdit() ? 0 : 1) != pass) continue;
        if (!object || object->isDead()) { mActive.erase(id); mStates.erase(id); ++mRevision; continue; }
        if (!eligible(object) || object->getRegion() != mStates.at(id).region)
        { stop(id, "Projector unavailable"); continue; }
        auto& s = mStates.at(id);
        auto* avatar = LLDirectorCast::instance().resolve(s.subject);
        if (avatar && avatar->getRegion() != object->getRegion())
        { stop(id, "Subject left region"); continue; }
        LLJoint* joint = avatar && !avatar->isDead() && avatar->mDrawable.notNull()
            ? avatar->getJoint(jointName(s.joint)) : nullptr;
        if (!avatar || avatar->isDead() || avatar->mDrawable.isNull())
        {
            if (!s.missingSince) s.missingSince = now;
            if (now - s.missingSince > 2.) { stop(id, "Subject unavailable"); continue; }
            place(object, s.current);
            continue;
        }
        s.missingSince = 0.;
        if (!joint || !joint->getWorldPosition().isFinite())
        { place(object, s.current); continue; } // never snap to the root
        const Pose frame{joint->getWorldPosition(), avatar->getRenderRotation()};
        if (!s.captured)
        {
            s.offset = relative(s.current, frame);
            s.captured = true;
        }
        Pose desired = s.mode == FOLLOW ? apply(s.offset, frame) : s.current;
        if (s.mode == AIM) desired.rotation = aim(frame.position - s.current.position, s.current.rotation);
        s.current = smooth(s.current, desired, dt, s.smoothing);
        place(object, s.current);
    }
}
