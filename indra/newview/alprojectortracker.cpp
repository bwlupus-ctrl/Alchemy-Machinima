#include "llviewerprecompiledheaders.h"
#include "alprojectortracker.h"
#include "llagent.h"
#include "lldirectorcast.h"
#include "alghoststudio.h"
#include "lldrawable.h"
#include "llfloatertools.h"
#include "lljoint.h"
#include "llprismlens.h"
#include "llselectmgr.h"
#include "llspatialpartition.h"
#include "lltimer.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvovolume.h"
#include "pipeline.h"
using namespace ALProjectorTrackMath;
namespace
{
const LLViewerObject* placingRoot = nullptr;
void markOne(LLDrawable* drawable)
{
    if (!drawable || drawable->isDead()) return;
    gPipeline.markMoved(drawable, false);
    drawable->clearState(LLDrawable::EARLY_MOVE);
    gPipeline.updateMoveNormalAsync(drawable);
    // Child bounds live inside the bridge. Refit them before early cube culls.
    if (auto* volume = drawable->getVOVolume())
    {
        if (volume->getVolume())
        {
            volume->updateRelativeXform();
            volume->genBBoxes(false);
            // Ordinary children bake their local transform into vertices. A
            // same-cell move does not dirty the mesh via the octree. Finish
            // that targeted work now, before hero/mirror/cube probe passes.
            if (!drawable->isRoot() && drawable->isState(LLDrawable::REBUILD_POSITION))
            {
                drawable->updateGeometry();
                if (auto* group = drawable->getSpatialGroup())
                {
                    group->dirtyMesh();
                    group->rebuildGeom();
                    group->rebuildMesh();
                }
            }
        }
    }
}
void markMoved(LLViewerObject* object)
{
    markOne(object->mDrawable);
    for (LLViewerObject* child : object->getChildren())
        if (child) markOne(child->mDrawable);
    // Hero/mirror probes run before updateGeom drains the bridge move queue.
    // Refit this linkset now without reordering the global render pipeline.
    auto* root = object->getRootEdit();
    if (root->getRegion() && root->mDrawable.notNull())
        if (auto* bridge = root->mDrawable->getSpatialBridge()) bridge->updateMove();
}
LLViewerObject* parentOf(LLViewerObject* object)
{
    return static_cast<LLViewerObject*>(object->getParent());
}
void place(LLViewerObject* object, const Pose& world)
{
    const bool silhouette = object->isChanged(LLXform::SILHOUETTE);
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
    // Cached selection contours are carried by the current rigid pose. Keep
    // real camera/geometry invalidations, not drive-only silhouette dirtiness.
    if (!silhouette) object->clearChanged(LLXform::SILHOUETTE);
    const auto* previous = placingRoot;
    placingRoot = object->getRootEdit();
    markMoved(object);
    placingRoot = previous;
}
bool subjectFrame(LLVOAvatar* avatar, LLJoint* joint, Pose& frame)
{
    if (!avatar || !joint || !avatar->getRootJoint()) return false;
    LLVector3 position = joint->getWorldPosition();
    if (avatar->hasClientOuterTransform())
        position = renderedJoint(position, avatar->getClientOuterTransformMatrix());
    // Joint world positions use the common agent origin, including neighbors;
    // global rebasing avoids any dependence on the subject's region origin.
    position = gAgent.getPosAgentFromGlobal(gAgent.getPosGlobalFromAgent(position));
    frame = {position, bodyYaw(avatar->getRootJoint()->getWorldRotation())};
    return frame.position.isFinite() && frame.rotation.isFinite();
}
}
bool ALProjectorTracker::subjectFrame(LLVOAvatar* avatar, LLJoint* joint, ALProjectorTrackMath::Pose& frame)
{
    return ::subjectFrame(avatar, joint, frame);
}
bool ALProjectorTracker::isTrackingMove(const LLViewerObject* object)
{
    return placingRoot && object && object->getRootEdit() == placingRoot;
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
    return volume && object->getRegion() && !object->isDead() && !object->isAttachment() &&
        !object->isHUDAttachment() && !object->isLocalOnly() &&
        (volume->isLightSpotlight() || LLPrismLens::hasProjectorBinding(object->getID()));
}
bool ALProjectorTracker::canUseTransform(LLViewerObject* object) const
{
    if (mActive.empty() || !object) return true;
    for (const auto& id : mActive)
    {
        auto* candidate = gObjectList.findObject(id);
        if (!candidate || candidate->getRootEdit() != object->getRootEdit()) continue;
        Pose checked;
        if (mStates.at(id).restorePending || (candidate->hasLastServerTransform() &&
            !candidate->getLastServerTransform(checked.position, checked.rotation))) return false;
    }
    return true;
}
bool ALProjectorTracker::serverEditTransform(LLViewerObject* object, Pose& pose) const
{
    if (mActive.empty() || !object || !canUseTransform(object) ||
        (!tracked(object->getID()) && !tracked(object->getRootEdit()->getID()))) return false;
    pose = {object->getPosition(), object->getRotation()};
    if (needsServerPose(state(object->getID()).overridden, state(object->getID()).restorePending))
    {
        pose = state(object->getID()).server;
        object->getLastServerTransform(pose.position, pose.rotation);
    }
    for (auto* parent = parentOf(object); parent; parent = parentOf(parent))
    {
        Pose frame{parent->getPosition(), parent->getRotation()};
        if (needsServerPose(state(parent->getID()).overridden, state(parent->getID()).restorePending))
        {
            frame = state(parent->getID()).server;
            parent->getLastServerTransform(frame.position, frame.rotation);
        }
        pose = apply(pose, frame);
    }
    return true;
}
void ALProjectorTracker::rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id)
{
    for (auto& item : mStates)
    {
        State& s = item.second;
        if (s.subject != old_id) continue;
        s.subject = new_id;
        s.subjectPending = new_id.notNull() && ALGhostStudio::instance().getInstance(new_id) != nullptr;
        s.missingSince = 0.;
        if (new_id.isNull())
        {
            // Null subjects otherwise mean You. Use the existing clear/restore
            // path, including its pending server-pose protection.
            s.hasSubject = false;
            s.captured = false;
            s.capturePending = false;
            stop(item.first, "Choose a subject");
        }
        ++mRevision;
    }
}

void ALProjectorTracker::configure(const LLUUID& id, const LLUUID& subject,
    bool has_subject, S32 joint, Mode mode, F32 smoothing)
{
    auto& s = mStates[id];
    if (s.subject != subject || s.joint != joint || s.mode != mode)
    { s.captured = false; s.capturePending = false; }
    // Follow previously owned position. Relinquish it once when switching to
    // Aim; subsequent Aim ticks preserve the root's live interpolation.
    if (s.mode == FOLLOW && mode == AIM)
        if (auto* object = gObjectList.findObject(id))
            if (restore(object, s))
                s.current = {object->getRenderPosition(), object->getRenderRotation()};
    s.subject = subject;
    s.subjectPending = subject.notNull() && ALGhostStudio::instance().getInstance(subject) != nullptr;
    s.hasSubject = has_subject;
    s.joint = llclamp(joint, 0, 6);
    s.mode = mode;
    s.smoothing = llclamp(smoothing, 0.f, 1.f);
    if (!has_subject) stop(id, "Choose a subject");
    ++mRevision;
}
bool ALProjectorTracker::restore(LLViewerObject* object, State& s)
{
    if (!object || object->isDead()) return true;
    if (!s.overridden && !s.restorePending) return true;
    Pose server;
    const auto* parent = parentOf(object);
    if (!object->getLastServerTransform(server.position, server.rotation))
    {
        // Never reinterpret stale packet locals in a different frame. A cache
        // with unresolved metadata cannot fall back to the driven object pose.
        if (object->hasLastServerTransform() || object->getRegion() != s.region ||
            (parent ? parent->getID() : LLUUID::null) != s.parent)
        {
            if (!s.restorePending || !s.paused) ++mRevision;
            s.restorePending = true;
            s.paused = true;
            s.status = "Restore pending: waiting for server pose";
            return false;
        }
        server = s.server; // uncached initial pose, still in the identical frame
    }
    object->setPosition(server.position);
    object->setRotation(server.rotation);
    s.server = server;
    s.overridden = false;
    s.restorePending = false;
    markMoved(object);
    return true;
}
void ALProjectorTracker::stop(const LLUUID& id, const std::string& status)
{
    auto it = mStates.find(id);
    if (it == mStates.end()) return;
    it->second.enabled = false;
    // Track off / clear must retain protection until restoration succeeds.
    if (restore(gObjectList.findObject(id), it->second))
    {
        mActive.erase(id);
        it->second.status = status;
    }
    ++mRevision;
}
bool ALProjectorTracker::selected(LLViewerObject* object) const
{
    auto selection = LLSelectMgr::getInstance()->getSelection();
    for (auto node = selection->begin(); node != selection->end(); ++node)
    {
        auto* candidate = (*node)->getObject();
        if (candidate && candidate->getRootEdit() == object->getRootEdit()) return true;
    }
    return false;
}
bool ALProjectorTracker::grabbed(LLViewerObject* object) const
{
    return mGrabRoot.notNull() && object->getRootEdit()->getID() == mGrabRoot;
}
bool ALProjectorTracker::pause(LLViewerObject* object, State& s, const char* status)
{
    const bool changed = s.overridden || s.restorePending;
    const bool was_paused = s.paused;
    const std::string old_status = s.status;
    s.paused = true;
    if (restore(object, s)) s.status = status;
    if (!was_paused || changed || old_status != s.status) ++mRevision;
    return changed;
}
void ALProjectorTracker::refreshPause(LLViewerObject* object, State& s)
{
    if (grabbed(object)) pause(object, s, "Paused while grabbed");
    else if (gFloaterTools && gFloaterTools->getVisible() && selected(object))
        pause(object, s, "Paused while selected in Build tools");
    else if (!s.restorePending && s.paused)
    {
        s.paused = false;
        s.current = {object->getRenderPosition(), object->getRenderRotation()};
        s.status = "Tracking";
        ++mRevision;
    }
}
void ALProjectorTracker::selectionChanged()
{
    if (mActive.empty()) return;
    bool restored = false;
    for (const auto& id : mActive)
        if (auto* object = gObjectList.findObject(id))
        {
            auto& s = mStates.at(id);
            const bool overridden = s.overridden;
            refreshPause(object, s);
            restored |= overridden && !s.overridden;
        }
    if (restored)
    {
        auto selection = LLSelectMgr::getInstance()->getSelection();
        for (auto node = selection->begin(); node != selection->end(); ++node)
        {
            (*node)->mLastPositionLocal.clearVec();
            (*node)->mLastRotation = LLQuaternion();
        }
    }
}
void ALProjectorTracker::enable(const LLUUID& id, bool enabled)
{
    if (!enabled) { stop(id, "Track off"); return; }
    auto* object = gObjectList.findObject(id);
    auto& s = mStates[id];
    if (!eligible(object) || !s.hasSubject) return;
    if (tracked(id)) { s.enabled = true; refreshPause(object, s); return; }
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
    s.overridden = false;
    mActive.insert(id);
    s.restorePending = object->hasLastServerTransform() &&
        !object->getLastServerTransform(s.server.position, s.server.rotation);
    if (s.restorePending) pause(object, s, "Restore pending: waiting for server pose");
    refreshPause(object, s);
    mRegion = gAgent.getRegion();
    ++mRevision;
}
void ALProjectorTracker::capture(const LLUUID& id)
{
    auto it = mStates.find(id);
    if (it == mStates.end() || it->second.mode != FOLLOW) return;
    // UI callbacks can run before applyHeld. Consume at the final-pose tick.
    it->second.capturePending = true;
}
void ALProjectorTracker::beforeObjectUpdate(LLViewerObject* object)
{
    if (mActive.empty()) return;
    if (tracked(object->getID())) restore(object, mStates.at(object->getID()));
}
void ALProjectorTracker::receivedServerPose(LLViewerObject* object, const LLVector3& position, const LLQuaternion& rotation)
{
    if (mActive.empty()) return;
    if (!tracked(object->getID()) || object->isDead() || !position.isFinite() || !rotation.isFinite()) return;
    auto& s = mStates.at(object->getID());
    s.server = {position, rotation};
    const auto* parent = parentOf(object);
    s.parent = parent ? parent->getID() : LLUUID::null;
    s.region = object->getRegion();
    // The decoder installs this accepted pose immediately after this hook.
    // LLViewerObject retains the packet parent ID even for unresolved orphans.
    s.overridden = false;
    Pose checked;
    s.restorePending = !object->getLastServerTransform(checked.position, checked.rotation);
    if (!s.enabled && !s.restorePending)
    { s.status = "Track off"; mActive.erase(object->getID()); ++mRevision; }
}
bool ALProjectorTracker::pauseObjectForEdit(LLViewerObject* object)
{
    if (mActive.empty() || !object) return false;
    bool changed = false;
    for (const auto& id : mActive)
    {
        auto* candidate = gObjectList.findObject(id);
        if (candidate && candidate->getRootEdit() == object->getRootEdit())
            changed |= pause(candidate, mStates.at(id),
                gFloaterTools && gFloaterTools->getVisible() && selected(candidate)
                ? "Paused while selected in Build tools" : "Paused for edit");
    }
    return changed;
}
bool ALProjectorTracker::pauseSelectionForEdit()
{
    if (mActive.empty()) return false;
    bool changed = false;
    auto selection = LLSelectMgr::getInstance()->getSelection();
    for (auto node = selection->begin(); node != selection->end(); ++node)
        changed |= pauseObjectForEdit((*node)->getObject());
    // Never recapture a mixed selection after callers may have mutated it.
    if (changed)
        for (auto node = selection->begin(); node != selection->end(); ++node)
        {
            (*node)->mLastPositionLocal.clearVec();
            (*node)->mLastRotation = LLQuaternion();
        }
    return changed;
}
bool ALProjectorTracker::selectionSendBarrier()
{
    if (mActive.empty()) return false;
    bool cancel = pauseSelectionForEdit();
    auto selection = LLSelectMgr::getInstance()->getSelection();
    for (const auto& id : mActive)
        if (auto* object = gObjectList.findObject(id))
            if (selected(object) && !canUseTransform(object)) cancel = true;
    if (!cancel) return false;
    // Backstop cancellation rolls back EVERY selected node, including ordinary
    // prims already mutated by an unhooked align/snap/scale operation.
    for (auto node = selection->begin(); node != selection->end(); ++node)
    {
        auto* object = (*node)->getObject();
        if (!object) continue;
        Pose server;
        if (object->getLastServerTransform(server.position, server.rotation))
        {
            object->setPosition(server.position);
            object->setRotation(server.rotation);
        }
        else if (!tracked(object->getID()) || canUseTransform(object))
        {
            object->setPosition((*node)->mSavedPositionLocal);
            LLQuaternion rotation = (*node)->mSavedRotation;
            if (auto* parent = parentOf(object)) rotation = rotation * ~parent->getRotationRegion();
            object->setRotation(rotation);
        }
        LLVector3 scale = (*node)->mSavedScale;
        object->getLastServerScale(scale);
        object->setScale(scale);
        (*node)->mSavedScale = scale;
        (*node)->mLastPositionLocal.clearVec();
        (*node)->mLastRotation = LLQuaternion();
        (*node)->mLastScale.clearVec();
        markMoved(object);
    }
    LLSelectMgr::getInstance()->updateSelectionCenter();
    return true;
}
void ALProjectorTracker::beginGrab(LLViewerObject* object)
{
    if (!object) return;
    mGrabRoot = object->getRootEdit()->getID(); // even when Track starts mid-grab
    pauseObjectForEdit(object);
    selectionChanged();
}
void ALProjectorTracker::endGrab()
{
    mGrabRoot.setNull();
    selectionChanged();
}
void ALProjectorTracker::drop(const LLUUID& id)
{
    if (mStates.empty()) return;
    mActive.erase(id);
    if (mStates.erase(id)) ++mRevision;
}
void ALProjectorTracker::clear()
{
    if (mStates.empty() && mActive.empty()) return;
    const auto active = mActive;
    for (const auto& id : active) stop(id, "Track off");
    for (auto it = mStates.begin(); it != mStates.end();)
        if (!tracked(it->first)) it = mStates.erase(it); else ++it;
    mRegion = nullptr;
    ++mRevision;
}
bool ALProjectorTracker::holdMissingSubject(const LLUUID& id, State& s, F64 now)
{
    // A stable Studio handle is a known, temporarily unresolved subject. Keep
    // the captured pose/offset and Track armed until recovery or final deletion.
    if (s.subjectPending) { s.missingSince = 0.; return true; }
    if (!s.missingSince) s.missingSince = now;
    if (now - s.missingSince > 2.) { stop(id, "Subject unavailable"); return false; }
    return true;
}

void ALProjectorTracker::update(F32 dt)
{
    if (mActive.empty()) return;
    if (mRegion != gAgent.getRegion()) { clear(); mRegion = gAgent.getRegion(); }
    const F64 now = LLTimer::getTotalSeconds();
    // Place roots before children, using this frame's final parent transform.
    for (S32 pass = 0; pass != 2; ++pass)
    for (auto it = mActive.begin(); it != mActive.end();)
    {
        const LLUUID id = *it++;
        auto* object = gObjectList.findObject(id);
        if (object && !object->isDead() && (object->isRootEdit() ? 0 : 1) != pass) continue;
        if (!object || object->isDead()) { mActive.erase(id); mStates.erase(id); ++mRevision; continue; }
        auto& s = mStates.at(id);
        if (!s.enabled)
        {
            if (restore(object, s)) { s.status = "Track off"; mActive.erase(id); ++mRevision; }
            continue;
        }
        if (!eligible(object)) { stop(id, "Projector unavailable"); continue; }
        if (s.restorePending && !restore(object, s)) continue;
        refreshPause(object, s);
        // A requested capture is evaluated even while Build pauses placement.
        if (s.capturePending)
        {
            auto* avatar = s.hasSubject ? LLDirectorCast::instance().resolve(s.subject) : nullptr;
            LLJoint* joint = avatar ? avatar->getJoint(jointName(s.joint)) : nullptr;
            Pose frame;
            if (canUseTransform(object) && subjectFrame(avatar, joint, frame))
            {
                s.current = {object->getRenderPosition(), object->getRenderRotation()};
                s.offset = relative(s.current, frame);
                s.captured = true;
                s.capturePending = false;
            }
        }
        if (s.paused) continue;
        Pose server = s.server;
        if (object->hasLastServerTransform() && !object->getLastServerTransform(server.position, server.rotation))
        { s.restorePending = true; pause(object, s, "Restore pending: waiting for server pose"); continue; }
        const Pose true_world = parentOf(object) ? apply(server,
            {parentOf(object)->getRenderPosition(), parentOf(object)->getRenderRotation()}) :
            Pose{object->getPositionAgent(), server.rotation};
        // Aim overrides rotation only, even with a missing joint or subject.
        if (s.mode == AIM) s.current.position = true_world.position;
        auto* avatar = LLDirectorCast::instance().resolve(s.subject);
        LLJoint* joint = avatar && !avatar->isDead() && avatar->mDrawable.notNull()
            ? avatar->getJoint(jointName(s.joint)) : nullptr;
        if (!avatar || avatar->isDead() || avatar->mDrawable.isNull())
        {
            if (!holdMissingSubject(id, s, now)) continue;
            place(object, s.current);
            s.overridden = true;
            continue;
        }
        s.missingSince = 0.;
        Pose frame;
        if (!subjectFrame(avatar, joint, frame))
        { place(object, s.current); s.overridden = true; continue; }
        if (!s.captured)
        {
            s.offset = relative(s.current, frame);
            s.captured = true;
        }
        Pose desired = s.mode == FOLLOW ? apply(s.offset, frame) : s.current;
        if (s.mode == AIM) desired.rotation = aim(frame.position - true_world.position, s.current.rotation);
        s.current = smooth(s.current, desired, dt, s.smoothing);
        if (s.mode == AIM) s.current.position = true_world.position;
        place(object, s.current);
        s.overridden = true;
    }
}
