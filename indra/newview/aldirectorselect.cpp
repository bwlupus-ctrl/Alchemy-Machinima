/**
 * @file aldirectorselect.cpp
 * @brief Select an avatar or clone in the Director -- see aldirectorselect.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "aldirectorselect.h"

#include "alavatarsubjectpicker.h"
#include "alghostanimdrop.h"
#include "alghostcloneactions.h"
#include "alghostclonemenulogic.h"
#include "alghoststudio.h"
#include "alkitlazypage.h"
#include "alkitshell.h"
#include "alpanelghoststudio.h"
#include "lldirectorcast.h"
#include "llagent.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llghostavatar.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"

#include <algorithm>

// Reaches the Clones panel's private multi-selection so a world click can add
// to it exactly the way a list click does (defined at the end of this file).
struct ALGhostCloneMenuAccess
{
    static void selectInHosts(const LLUUID& instance_id, bool additive);
    static std::vector<LLUUID> hostRows();
};

namespace ALDirectorSelect
{
const char* const CLONES_PAGE = "cast_group/ghosts_tab";

namespace
{
void tip(const std::string& text)
{
    if (!text.empty())
    {
        LLNotificationsUtil::add("SystemMessageTip", LLSD().with("MESSAGE", text));
    }
}

ALKitShell* findShell()
{
    LLFloater* floater = LLFloaterReg::findInstance("director");
    return floater ? floater->findChild<ALKitShell>("director_shell") : nullptr;
}

// An explicit Select opens the Director when it is closed or minimized and
// brings it in front of other floaters when it is already open (no keyboard
// focus is taken, so Esc and the camera keys keep working in the world).
void raiseDirector()
{
    LLFloater* floater = LLFloaterReg::findInstance("director");
    if (!floater || !floater->getVisible())
    {
        LLFloaterReg::showInstance("director");
        return;
    }
    if (floater->isMinimized())
    {
        floater->setMinimized(false);
    }
    floater->setFrontmost(false);
}
} // namespace

std::vector<LLUUID> hostRows()
{
    return ALGhostCloneMenuAccess::hostRows();
}

bool directorVisible()
{
    LLFloater* floater = LLFloaterReg::findInstance("director");
    return floater && floater->getVisible() && !floater->isMinimized();
}

LLUUID cloneAtPick(const LLPickInfo& pick)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    LLViewerObject* hit = pick.getObject();
    LLUUID clone = studio.entityCloneForObject(hit);
    if (clone.notNull())
    {
        return clone;
    }
    // Same rule as the 3D inventory drop: the screen-space probe may claim the
    // click only when the clone is nearer than the real hit.
    LLVector3 probe_agent;
    LLUUID nearby = studio.pickInstanceAtScreen(
        pick.mMousePt.mX, pick.mMousePt.mY, ALGhostStudio::SCREEN_PICK_PX, true, &probe_agent);
    if (nearby.isNull())
    {
        // [OverlayPick] No entity clone here: a free-standing overlay clone may
        // claim the click too. An overlay drawn on its own source avatar stays
        // with that avatar's menu (its "Clone overlay" branch), so a click that
        // really hit the source never becomes an overlay click.
        nearby = studio.pickInstanceAtScreen(
            pick.mMousePt.mX, pick.mMousePt.mY, ALGhostStudio::SCREEN_PICK_PX, false, &probe_agent);
        const ALGhostStudio::Instance* inst = nearby.notNull() ? studio.getInstance(nearby) : nullptr;
        if (!inst || inst->mKind != ALGhostStudio::BACKING_OVERLAY)
        {
            return LLUUID::null;
        }
        if (!inst->mSourceRemoved)
        {
            const LLUUID owner = inst->mSource.isNull() ? gAgentID : inst->mSource;
            const LLVOAvatar* under = avatarAtPick(pick);
            const LLViewerObject* root = hit ? hit->getRootEdit() : nullptr;
            if ((under && under->getID() == owner) || (root && root->getID() == owner))
            {
                return LLUUID::null;
            }
        }
    }
    const bool real_pick = hit != nullptr || pick.mPickType == LLPickInfo::PICK_LAND;
    const bool hud_pick = (hit != nullptr && hit->isHUDAttachment()) ||
        pick.mPickType == LLPickInfo::PICK_ICON;
    const LLVector3 eye = LLViewerCamera::getInstance()->getOrigin();
    const LLVector3 hit_agent = gAgent.getPosAgentFromGlobal(pick.mPosGlobal);
    return ALGhostAnimDrop::cloneInFront(real_pick, hud_pick, dist_vec(probe_agent, eye),
                                         dist_vec(hit_agent, eye))
        ? nearby : LLUUID::null;
}

LLVOAvatar* avatarAtPick(const LLPickInfo& pick)
{
    LLViewerObject* object = pick.getObject();
    // A click on your own HUD is not a click on your avatar.
    if (object && object->isHUDAttachment())
    {
        return nullptr;
    }
    LLVOAvatar* avatar = ALAvatarSubjectPicker::fromObject(object);
    if (!avatar && object && !object->isAttachment())
    {
        avatar = LLDirectorCast::instance().resolve(object->getID());   // animesh root
    }
    return avatar && !avatar->isGhostAvatar() && ALAvatarSubjectPicker::valid(avatar, false, true)
        ? avatar : nullptr;
}

std::string avatarRefusal(const LLUUID& avatar_id)
{
    LLViewerObject* object = avatar_id.notNull() ? gObjectList.findObject(avatar_id) : nullptr;
    LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
    if (!avatar || avatar->isGhostAvatar() || !ALAvatarSubjectPicker::valid(avatar, false, true))
    {
        return LLTrans::getString("DirectorSelectNoAvatar");
    }
    return std::string();
}

std::string cloneRefusal(const LLUUID& instance_id)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::Instance* inst = studio.getInstance(instance_id);
    if (!inst || inst->mDeleting)
    {
        return LLTrans::getString("DirectorSelectNoClone");
    }
    if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE && !studio.resolveEntityClone(instance_id))
    {
        return LLTrans::getString("DirectorSelectCloneNotReady");
    }
    return std::string();
}

bool selectAvatar(const LLUUID& avatar_id, bool additive)
{
    const std::string refusal = avatarRefusal(avatar_id);
    if (!refusal.empty())
    {
        tip(refusal);
        return false;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    ALGhostCloneMenuLogic::SelectInput in;
    in.mIsClone = false;
    in.mAdditive = additive;
    in.mInCast = cast.contains(avatar_id);
    in.mCastRefuses = false;
    const ALGhostCloneMenuLogic::SelectAction action = ALGhostCloneMenuLogic::planSelect(in);
    if (action == ALGhostCloneMenuLogic::SelectAction::AddToCastThenSetActor)
    {
        cast.add(avatar_id);      // the Cast's own add path and refusals
        if (!cast.contains(avatar_id))
        {
            tip(LLTrans::getString("DirectorSelectCastRefused"));
            return false;
        }
    }
    raiseDirector();
    cast.setActiveActorId(avatar_id);
    return true;
}

bool selectClone(const LLUUID& instance_id, bool additive)
{
    const std::string refusal = cloneRefusal(instance_id);
    if (!refusal.empty())
    {
        tip(refusal);
        return false;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::Instance* inst = studio.getInstance(instance_id);
    const bool entity = inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
    const LLUUID entity_id = inst->mEntityId;
    raiseDirector();
    ALKitShell* shell = findShell();
    ALGhostCloneMenuLogic::SelectInput in;
    in.mIsClone = true;
    in.mAdditive = additive;
    in.mOnClonesPage = !shell || shell->currentPageKey() == CLONES_PAGE;
    // An overlay is a Clones page object only: it can never be the Actor bar actor.
    if (!entity)
    {
        in.mOnClonesPage = true;
    }
    switch (ALGhostCloneMenuLogic::planSelect(in))
    {
    case ALGhostCloneMenuLogic::SelectAction::CloneListAdd:
        ALGhostCloneMenuAccess::selectInHosts(instance_id, true);
        break;
    case ALGhostCloneMenuLogic::SelectAction::CloneListReplace:
        ALGhostCloneMenuAccess::selectInHosts(instance_id, false);
        break;
    case ALGhostCloneMenuLogic::SelectAction::CloneActor:
        ALGhostCloneMenuAccess::selectInHosts(instance_id, false);
        LLDirectorCast::instance().setActiveActorId(entity_id);
        break;
    default:
        return false;
    }
    return true;
}

bool openCloneInDirector(const LLUUID& instance_id)
{
    if (!cloneRefusal(instance_id).empty())
    {
        return false;
    }
    ALGhostCloneMenuAccess::selectInHosts(instance_id, false);
    raiseDirector();
    if (ALKitShell* shell = findShell())
    {
        return shell->navigateTo(std::string(CLONES_PAGE));
    }
    return false;
}
} // namespace ALDirectorSelect

// ---------------------------------------------------------------------------
namespace
{
ALPanelGhostStudio* findHostBelow(LLView* view)
{
    if (!view)
    {
        return nullptr;
    }
    if (ALPanelGhostStudio* host = dynamic_cast<ALPanelGhostStudio*>(view))
    {
        return host;
    }
    for (LLView* child : *view->getChildList())
    {
        if (ALPanelGhostStudio* host = findHostBelow(child))
        {
            return host;
        }
    }
    return nullptr;
}

// The Clones host that holds the multi-selection: the one the user last worked
// in while it is on screen, else the Director's own (built on demand, without
// selecting its page, so a Ctrl-click accumulates from any Director page).
ALPanelGhostStudio* selectionHost()
{
    if (ALPanelGhostStudio* host = ALPanelGhostStudio::activeHost())
    {
        return host;
    }
    LLFloater* floater = LLFloaterReg::findInstance("director");
    ALKitShell* shell = floater ? floater->findChild<ALKitShell>("director_shell") : nullptr;
    if (!shell)
    {
        return nullptr;
    }
    if (ALKitLazyPage* lazy = shell->findChild<ALKitLazyPage>("ghosts_tab"))
    {
        lazy->ensureBuilt();
        return findHostBelow(lazy);
    }
    return nullptr;
}
} // namespace

void ALGhostCloneMenuAccess::selectInHosts(const LLUUID& instance_id, bool additive)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID row = ALGhostCloneActions::rowFor(instance_id);
    ALPanelGhostStudio* host = additive ? selectionHost() : nullptr;
    if (!host)
    {
        // Replace: the shared primary selection (the unit's list row) is
        // mirrored into every host list.
        studio.setSelected(row);
        return;
    }
    // Ctrl-click: add the row to the list's multi-selection the same way a
    // list Ctrl-click does, then make it the primary. A hidden host (another
    // Director page is current) does not draw, so it has not mirrored the
    // shared selection: bring its rows up to date, then reconcile with the
    // shared selection exactly as the panel's mirror would (the list is
    // replaced by the shared primary row when the revision moved), without
    // running its draw / detail refresh. A freshly built host starts from the
    // shared selection the same way.
    if (!host->isInVisibleChain())
    {
        // Rows only: the whole-inventory animation scan is not worth a hidden
        // selection (the page requests it when it is shown and rebuilds).
        struct ScanGuard
        {
            bool& mFlag;
            const bool mPrevious;
            explicit ScanGuard(bool& flag) : mFlag(flag), mPrevious(flag) { mFlag = true; }
            ~ScanGuard() { mFlag = mPrevious; }
        } scan_guard(host->mSuppressLibraryScan);
        host->refreshList(true);
    }
    const bool shared_moved = host->mObservedSelectionRevision != studio.getSelectionRevision();
    const LLUUID& shared = studio.getSelected();
    const LLUUID shared_row = shared.notNull() ? ALGhostCloneActions::rowFor(shared) : LLUUID::null;
    const std::vector<LLUUID> rows = ALGhostCloneMenuLogic::additiveRows(
        host->selectedRowValues(), shared_moved, shared_row, row, LLUUID::null);
    if (shared_moved)
    {
        host->mList->deselectAllItems(true);
        host->mList->selectMultiple(rows);
    }
    else if (std::find(rows.begin(), rows.end(), row) != rows.end() && host->selectedRowValues() != rows)
    {
        host->mList->selectMultiple(std::vector<LLUUID>{ row });
    }
    const std::vector<LLUUID> now_selected = host->selectedRowValues();
    if (std::find(now_selected.begin(), now_selected.end(), row) == now_selected.end())
    {
        // The row is not in the list (filtered, or not rebuilt yet): nothing was
        // added, so the host must not believe it mirrored anything.
        studio.setSelected(row);
        return;
    }
    host->mPrimaryRow = row;
    host->noteListSelectionChanged();
    if (host->isInVisibleChain())
    {
        host->noteActiveHost();     // a hidden host must not displace the host the user is working in
    }
    studio.setSelected(row);
    host->mObservedSelectionRevision = studio.getSelectionRevision();
}

std::vector<LLUUID> ALGhostCloneMenuAccess::hostRows()
{
    if (ALPanelGhostStudio* host = ALPanelGhostStudio::activeHost())
    {
        return host->selectedRowValues();
    }
    // No visible host: a multi-selection built while the Clones page was hidden
    // lives in the Director's already-built host (never built here). It counts
    // only while it has mirrored the current shared selection.
    LLFloater* floater = LLFloaterReg::findInstance("director");
    ALKitShell* shell = floater ? floater->findChild<ALKitShell>("director_shell") : nullptr;
    ALKitLazyPage* lazy = shell ? shell->findChild<ALKitLazyPage>("ghosts_tab") : nullptr;
    ALPanelGhostStudio* hidden = findHostBelow(lazy);
    if (hidden && hidden->mObservedSelectionRevision == ALGhostStudio::instance().getSelectionRevision())
    {
        return hidden->selectedRowValues();
    }
    return std::vector<LLUUID>();
}
