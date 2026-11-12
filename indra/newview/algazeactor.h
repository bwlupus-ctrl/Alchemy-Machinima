/** Shared local, per-actor writes for the Gaze card and avatar context menu. */
#ifndef AL_GAZE_ACTOR_H
#define AL_GAZE_ACTOR_H

#include "alavatarsubjectpicker.h"
#include "lldirectorcast.h"
#include "llselectmgr.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

namespace ALGazeActor
{
inline void setEnabled(const LLUUID& actor, bool enabled)
{
    LLActorMover::instance().setGazeEnabled(actor, enabled);
}

inline void setTarget(const LLUUID& actor, const LLActorMover::GazeTarget& target, bool enable = false)
{
    LLDirectorCast::instance().setGazeTarget(actor, target);
    if (enable) setEnabled(actor, true);
}

// Menu picks exclude dead/HUD objects and the actor body/attachments.
inline LLUUID menuObjectTarget(const LLUUID& actor, LLViewerObject* object)
{
    if (!object || object->isDead() || object->isHUDAttachment()) return LLUUID::null;
    LLVOAvatar* wearer = ALAvatarSubjectPicker::fromObject(object);
    if (object->getID() == actor || (wearer && wearer->getID() == actor)) return LLUUID::null;
    LLViewerObject* root = object->getRootEdit();
    return root ? root->getID() : object->getID();
}

// Preserve the card: any selected object, resolved to its root-edit UUID.
inline LLUUID cardSelectedObjectTarget()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (!object) object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
    if (!object) return LLUUID::null;
    return object->getRootEdit() ? object->getRootEdit()->getID() : object->getID();
}
}
#endif
