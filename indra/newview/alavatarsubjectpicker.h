/** Shared Director subject menu, attachment wearer resolver and transient pick lifecycle. */
#ifndef AL_AVATAR_SUBJECT_PICKER_H
#define AL_AVATAR_SUBJECT_PICKER_H
#include "llhandle.h"
#include "lluuid.h"
#include <functional>
class LLVOAvatar;
class LLViewerObject;
class LLView;
class LLUICtrl;
class LLContextMenu;
namespace ALAvatarSubjectPicker
{
bool valid(const LLVOAvatar* avatar, bool ghosts = false, bool animesh = false);
LLVOAvatar* fromObject(LLViewerObject* object);
LLVOAvatar* fromSelection(bool animesh = false);
void choose(LLUICtrl* anchor, LLView* owner, LLHandle<LLContextMenu>& menu,
            bool ghosts, std::function<void(const LLUUID&)> picked, bool animesh = false);
void cancelFor(LLView* owner);
void rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id);
bool handleKey(KEY key, MASK mask);
}
#endif
