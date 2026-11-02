#include "llviewerprecompiledheaders.h"
#include "alavatarsubjectpicker.h"
#include "alghoststudio.h"
#include "alkitmenu.h"
#include "lldirectorcast.h"
#include "llfocusmgr.h"
#include "llmenugl.h"
#include "lluictrlfactory.h"
#include "llselectmgr.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include <set>
#include <memory>
#include <algorithm>
#include <vector>
#include "llviewermenu.h"
#include "llagent.h"

namespace ALAvatarSubjectPicker
{
namespace
{
std::vector<std::weak_ptr<LLUUID>> pendingSubjects;
void prunePendingSubjects()
{
    pendingSubjects.erase(std::remove_if(pendingSubjects.begin(), pendingSubjects.end(),
        [](const auto& subject) { return subject.expired(); }), pendingSubjects.end());
}
}

void rewriteActorReferences(const LLUUID& old_id, const LLUUID& new_id)
{
    prunePendingSubjects();
    for (auto& pending : pendingSubjects)
        if (auto subject = pending.lock(); subject && *subject == old_id) *subject = new_id;
}

enum { SUBJECT_A, SUBJECT_B, SUBJECT_C, SUBJECT_D };
bool valid(const LLVOAvatar* avatar, bool ghosts, bool animesh)
{
    return avatar && !avatar->isDead() && !ALGhostStudio::instance().isDeleting(avatar->getID()) &&
        !ALGhostStudio::instance().isPrivateOverlayRuntime(avatar->getID()) && (animesh || !avatar->isControlAvatar()) &&
        (!avatar->mIsDummy || (ghosts && avatar->isGhostAvatar()) ||
         (animesh && avatar->isControlAvatar())) &&
        (ghosts || !avatar->isGhostAvatar());
}

LLVOAvatar* fromObject(LLViewerObject* object)
{
    if (!object) return nullptr;
    if (auto* avatar = object->asAvatar()) return avatar;
    // Walk attachments to the wearer even for animated attachments, whose
    // getAvatar() can otherwise return their own animesh control avatar.
    if (object->isAttachment())
    {
        LLViewerObject* parent = (LLViewerObject*)object->getParent();
        while (parent && !parent->asAvatar()) parent = (LLViewerObject*)parent->getParent();
        return parent ? parent->asAvatar() : nullptr;
    }
    return object->getAvatar();
}

// Wearer resolution belongs only to the new avatar assignment controls.
// Existing selection consumers must keep CineCam's getAvatar() semantics.
LLVOAvatar* fromSelection(bool animesh)
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (animesh && object && !object->isAttachment())
    {
        if (LLVOAvatar* avatar = LLDirectorCast::instance().resolve(object->getID()))
            return avatar;
    }
    return fromObject(object);
}

// Same asynchronous lifecycle as VCam's MediaFacePickTool: one outstanding
// pick, generation invalidation on deselect, and a weak owner on completion.
class CameraSubjectPickTool final : public LLTool
{
public:
    CameraSubjectPickTool() : LLTool("Director avatar target") {}
    static CameraSubjectPickTool& instance() { static CameraSubjectPickTool tool; return tool; }
    void begin(LLView* owner, bool ghosts, bool animesh, std::function<void(const LLUUID&)> picked)
    {
        ++mGeneration;
        mGhosts = ghosts;
        mAnimesh = animesh;
        mOwner = owner->getHandle();
        mPicked = std::move(picked);
        gFocusMgr.setKeyboardFocus(nullptr);
    }
    void cancelFor(LLView* owner)
    {
        if (mOwner.get() == owner && LLToolMgr::getInstance()->getCurrentTool() == this)
            LLToolMgr::getInstance()->clearTransientTool();
    }
    bool handleMouseDown(S32 x, S32 y, MASK mask) override
    {
        if (mPickPending) return true;
        mPickPending = true;
        mPendingGeneration = mGeneration;
        gViewerWindow->pickAsync(x, y, mask, [](const LLPickInfo& pick)
        {
            auto& tool = instance();
            tool.mPickPending = false;
            if (tool.mPendingGeneration != tool.mGeneration ||
                LLToolMgr::getInstance()->getCurrentTool() != &tool) return;
            LLViewerObject* object = pick.getObject();
            LLVOAvatar* avatar = tool.mAnimesh && object && !object->isAttachment()
                ? LLDirectorCast::instance().resolve(object->getID()) : fromObject(object);
            if (!avatar) avatar = fromObject(object);
            const LLUUID id = valid(avatar, tool.mGhosts, tool.mAnimesh) ? avatar->getID() : LLUUID::null;
            const auto owner = tool.mOwner;
            const auto callback = std::move(tool.mPicked);
            LLToolMgr::getInstance()->clearTransientTool();
            if (owner.get() && owner.get()->isInVisibleChain() && callback && id.notNull()) callback(id);
        }, false, true); // include rigged avatar mesh and worn attachments
        return true;
    }
    bool handleHover(S32, S32, MASK) override
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLPICKOBJECT3);
        return true;
    }
    bool handleKey(KEY key, MASK mask) override
    {
        if (key != KEY_ESCAPE || mask != MASK_NONE) return false;
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    bool handleRightMouseDown(S32, S32, MASK) override
    {
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    void handleDeselect() override
    {
        ++mGeneration;
        // App focus loss deselects without removing the transient tool. Keep
        // its callback/owner armed, but discard any pick started before blur.
        // A real tool change updates the selected tool before calling us.
        if (LLToolMgr::getInstance()->getCurrentTool() == this) return;
        mPicked = {};
        mOwner.markDead();
    }
private:
    LLHandle<LLView> mOwner;
    std::function<void(const LLUUID&)> mPicked;
    U64 mGeneration = 1;
    U64 mPendingGeneration = 0;
    bool mPickPending = false;
    bool mGhosts = false;
    bool mAnimesh = false;
};

void choose(LLUICtrl* anchor, LLView* owner, LLHandle<LLContextMenu>& menu_handle,
            bool ghosts, std::function<void(const LLUUID&)> picked, bool animesh)
{
    if (!anchor || !gMenuHolder) return;
    prunePendingSubjects();
    if (LLContextMenu* old = menu_handle.get())
    {
        old->hide();
        delete old;
    }
    LLContextMenu::Params params;
    params.name = "director_camera_subject_menu";
    params.visible = false;
    auto* menu = LLUICtrlFactory::create<LLContextMenu>(params);
    menu_handle = menu->getHandle();
    gMenuHolder->addChild(menu);
    auto add = [menu](const std::string& name, const std::string& label,
                      bool enabled, std::function<void()> action)
    {
        LLMenuItemCallGL::Params item;
        item.name = name;
        item.label = label;
        item.enabled = enabled;
        item.on_click.function = [action](LLUICtrl*, const LLSD&) { if (action) action(); };
        menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
    };
    const auto handle = owner->getHandle();
    add("pick_world", "Click avatar in world", true, [handle, ghosts, animesh, picked]()
    {
        auto* self = handle.get();
        if (!self || !self->isInVisibleChain()) return;
        auto& tool = CameraSubjectPickTool::instance();
        LLToolMgr::getInstance()->setTransientTool(&tool);
        tool.begin(self, ghosts, animesh, [handle, ghosts, animesh, picked](const LLUUID& id)
        {
            if (handle.get()) picked(id);
        });
    });
    auto& cast = LLDirectorCast::instance();
    std::set<LLUUID> emitted;
    auto add_avatar = [&](const LLUUID& id, const std::string& name, bool in_cast)
    {
        LLVOAvatar* avatar = cast.resolve(id);
        if (avatar && !valid(avatar, ghosts, animesh)) return;
        if (!avatar)
        {
            // Preserve away cast entries as read-only choices. A live avatar is
            // required to assign here; known props and Studio clones are omitted.
            const auto& studio = ALGhostStudio::instance();
            if (!in_cast || gObjectList.findObject(id) || studio.getInstance(id) ||
                studio.findInstanceByRuntime(id)) return;
        }
        if (!emitted.insert(LLDirectorCast::canonicalActorId(id)).second) return;
        std::string label = id == gAgentID ? "You" :
            !name.empty() ? name : avatar ? avatar->getFullname() : id.asString();
        const S32 mark = cast.getSubjectA() == id ? SUBJECT_A :
            cast.getSubjectB() == id ? SUBJECT_B : cast.getSubjectC() == id ? SUBJECT_C :
            cast.getSubjectD() == id ? SUBJECT_D : -1;
        label += mark >= SUBJECT_A ? llformat(" - Subject %c", 'A' + mark - SUBJECT_A) :
            (in_cast ? " - cast" : " - nearby");
        if (!avatar) label += " (away)";
        auto subject = std::make_shared<LLUUID>(id);
        pendingSubjects.push_back(subject);
        add("avatar_" + id.asString(), label, avatar != nullptr, [handle, picked, subject]()
        {
            // Rekey the operative payload while a menu is open. Final deletion
            // invalidates it rather than allowing null to select You.
            if (handle.get() && subject->notNull()) picked(*subject);
        });
    };
    for (const auto& member : cast.getCast()) add_avatar(member.mId, member.mLastName, true);
    if (animesh)
        for (const auto& clone : ALGhostStudio::instance().getInstances())
            if (clone.mKind == ALGhostStudio::BACKING_ENTITY_CLONE && clone.mEntityId.notNull())
                add_avatar(clone.mEntityId, clone.mName, false);
    // Same live-avatar source and filters as Light rig > Add avatar. Only on open.
    for (LLCharacter* character : LLCharacter::sInstances)
        if (auto* avatar = dynamic_cast<LLVOAvatar*>(character); valid(avatar, ghosts, animesh))
            add_avatar(avatar->getID(), avatar->getFullname(), false);
    if (emitted.empty()) add("no_avatars", "No live avatars nearby", false, {});
    menu->buildDrawLabels();
    ALKitMenu::showContextMenu(anchor, menu, 0, 0);
}

void cancelFor(LLView* owner) { CameraSubjectPickTool::instance().cancelFor(owner); }
bool handleKey(KEY key, MASK mask)
{
    auto& tool = CameraSubjectPickTool::instance();
    return LLToolMgr::getInstance()->getCurrentTool() == &tool && tool.handleKey(key, mask);
}
}
