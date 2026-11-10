/**
 * @file alghostclonemenu.cpp
 * @brief Right-click menus for Studio clones -- see alghostclonemenu.h.
 *
 * Every action goes through alghostcloneactions.h, the same row actions the
 * Clones page buttons call, with the rows the menu opened on.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alghostclonemenu.h"

#include "aldirectorselect.h"
#include "alghostclearanim.h"
#include "alghostcloneactions.h"
#include "alghostclonemenulogic.h"
#include "alghoststudio.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "alpanelghoststudio.h"
#include "altoolghostedit.h"
#include "altoolghostplace.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llfocusmgr.h"
#include "llghostavatar.h"
#include "llmenugl.h"
#include "llmodaldialog.h"
#include "llnotificationsutil.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "lluicolortable.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llviewermenu.h"           // gMenuHolder, the four avatar menus
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ALGhostCloneMenu
{
namespace
{
namespace Logic = ALGhostCloneMenuLogic;
namespace Actions = ALGhostCloneActions;

// The stock "no context menu spawned" value (LLMenuHolderGL's own initial and
// disarmed value): LLMenuItemGL::handleRightMouseUp treats ANY other value as an
// armed spawn and turns a right-release over an item into a left-click.
constexpr S32 SPAWN_DISARMED = S32_MAX;

LLContextMenu*              sMenu = nullptr;            // menu_ghost_clone.xml
Logic::RightClickGesture    sGesture;
LLUUID                      sPendingClone;
LLTool*                     sPressTool = nullptr;       // the tool that saw the press
ECameraMode                 sPressCameraMode = CAMERA_MODE_THIRD_PERSON;

// What the entity menu acts on, fixed when the menu opens.
LLUUID                      sPicked;
std::vector<LLUUID>         sRows;                      // Clones list rows (group headers kept)

// Aggregates over the targets, computed once per open (callbacks read these).
struct Aggregate
{
    size_t mLive = 0;
    size_t mEntities = 0;
    size_t mPaused = 0;
    size_t mReflecting = 0;
    size_t mPosed = 0;
};
Aggregate                   sAgg;
S32                         sStaggerCurrent = 0;        // cached when the menu opens

// What the overlay submenu acts on, fixed when an avatar menu opens.
LLUUID                      sMenuAvatar;                // the avatar the open menu belongs to
bool                        sMenuHud = false;           // the menu belongs to one of your own HUD attachments
std::vector<LLUUID>         sOverlays;                  // oldest first; back() = topmost / most recent
LLUUID                      sChosenOverlay;             // "Which overlay" pick
LLUUID                      sOverlayTarget;             // refreshed on open and on every overlay change
S32                         sOverlayStyle = -1;

LLUUID                      sMenuOwner;                 // edit-tool owner id for menu-started edits

struct BaseText
{
    LLHandle<LLView> mItem;                             // proves the key still names the same widget
    std::string mLabel;
    std::string mTip;
};
std::map<LLView*, BaseText> sBase;                      // item -> text as loaded from XML

void tip(const std::string& text)
{
    if (!text.empty())
    {
        LLNotificationsUtil::add("SystemMessageTip", LLSD().with("MESSAGE", text));
    }
}

std::string str(const char* key)
{
    return LLTrans::getString(key);
}

// A disabled item carries its reason in the label and the tooltip; an enabled
// one shows the XML text again. Writes only when something changed.
void setReason(LLUICtrl* ctrl, const std::string& reason)
{
    LLMenuItemGL* item = dynamic_cast<LLMenuItemGL*>(ctrl);
    if (!item)
    {
        return;
    }
    auto base = sBase.find(ctrl);
    if (base == sBase.end() || base->second.mItem.get() != ctrl)
    {
        // First sight, or the address now belongs to a different (rebuilt) item.
        BaseText text{ ctrl->getHandle(), item->getLabel(), item->getToolTip() };
        base = sBase.insert_or_assign(ctrl, std::move(text)).first;
    }
    const std::string label = reason.empty() ? base->second.mLabel : base->second.mLabel + " (" + reason + ")";
    const std::string text = reason.empty() ? base->second.mTip : reason;
    if (item->getLabel() != label)
    {
        item->setLabel(label);
    }
    if (item->getToolTip() != text)
    {
        item->setToolTip(text);
    }
}

bool state(LLUICtrl* ctrl, bool enabled, const char* reason_key = nullptr)
{
    setReason(ctrl, !enabled && reason_key ? str(reason_key) : std::string());
    return enabled;
}

ALGhostStudio::Instance* live(const LLUUID& id)
{
    ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(id);
    return inst && !inst->mDeleting ? inst : nullptr;
}

std::vector<LLUUID> liveTargets()
{
    std::vector<LLUUID> ids;
    for (const LLUUID& id : Actions::instances(sRows))
    {
        if (live(id))
        {
            ids.push_back(id);
        }
    }
    return ids;
}

std::vector<LLUUID> liveEntityTargets()
{
    std::vector<LLUUID> ids;
    for (const LLUUID& id : liveTargets())
    {
        if (ALGhostStudio::instance().getInstance(id)->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            ids.push_back(id);
        }
    }
    return ids;
}

Aggregate computeAggregate()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    Aggregate agg;
    for (const LLUUID& id : liveTargets())
    {
        ++agg.mLive;
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            continue;
        }
        ++agg.mEntities;
        agg.mPaused += inst->mDriveMode == ALGhostStudio::DRIVE_FROZEN ? 1 : 0;
        agg.mReflecting += inst->mDriveMode == ALGhostStudio::DRIVE_REFLECT ? 1 : 0;
        agg.mPosed += studio.isInstancePosed(id) ? 1 : 0;
    }
    return agg;
}

LLUUID overlayTarget()
{
    std::vector<LLUUID> ids;
    for (const LLUUID& id : sOverlays)
    {
        if (live(id))
        {
            ids.push_back(id);
        }
    }
    return Logic::chooseOverlay(ids, sChosenOverlay, LLUUID::null);
}

// The overlay target and what the menu shows about it. Called when the menu
// opens and again whenever the user changes the overlay or its look.
void refreshOverlayState()
{
    sOverlayTarget = overlayTarget();
    const ALGhostStudio::Instance* target = live(sOverlayTarget);
    sOverlayStyle = target ? target->mStyle : -1;
}

// Live, never cached: a target that despawned while the menu was open refuses.
std::string overlayRefusal()
{
    return live(sOverlayTarget) ? ALDirectorSelect::cloneRefusal(sOverlayTarget) : str("GhostCloneMenuNotReady");
}

// Smaller / Larger / Reset through the page's unit planner and the scale
// spinner's apply path (limits checked there, never silently clamped here).
bool resizeRows(const std::vector<LLUUID>& rows, int direction)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const std::vector<LLUUID> unit_ids = Actions::units(rows);
    bool all_ok = !unit_ids.empty();
    for (const LLUUID& unit : unit_ids)
    {
        LLVector3d foot;
        LLQuaternion rotation;
        F32 scale = 1.f;
        if (!studio.getUnitTransform(unit, foot, rotation, scale))
        {
            all_ok = false;
            continue;
        }
        const F32 next = Logic::steppedScale(scale, direction, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
        all_ok = Actions::applyUnitTransform(unit, foot, rotation, next) && all_ok;
    }
    if (!all_ok)
    {
        tip(str("GhostCloneMenuResizeFail"));
    }
    return all_ok;
}

int sizeDirection(const LLSD& param)
{
    const std::string kind = param.asString();
    return kind == "smaller" ? -1 : kind == "larger" ? 1 : 0;
}

// ---- edit tool (the Clones page "Edit clones" toggle, owner = this menu) ----
bool editActive()
{
    return ALToolGhostEdit::instanceExists() && ALToolGhostEdit::getInstance()->isEditModeActive();
}

const char* editBlockReason()
{
    if (ALGhostStudio::instance().getCrowdPlacementDraft().mActive)
    {
        return "GhostCloneMenuEditBusy";
    }
    if (ALToolGhostPlace::instanceExists() &&
        ALToolGhostPlace::getInstance()->armedOwner().notNull())
    {
        return "GhostCloneMenuEditBusy";
    }
    return nullptr;
}

// keep_multi_selection: only the ENTITY menu's Edit, whose sRows are the rows that menu
// opened on. The overlay submenu's Edit always selects its own overlay target (sRows may
// still hold a clone menu that was dismissed earlier).
void toggleEdit(const LLUUID& primary, bool keep_multi_selection)
{
    ALToolGhostEdit* tool = ALToolGhostEdit::getInstance();
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (editActive())
    {
        // Same teardown as the panel toggle, whichever host started the edit.
        tool->stopEditMode();
        if (tool_mgr->usingTransientTool() && tool_mgr->getCurrentTool() == tool)
        {
            tool_mgr->clearTransientTool();
        }
        return;
    }
    if (const char* reason = editBlockReason())
    {
        tip(str(reason));
        return;
    }
    if (sMenuOwner.isNull())
    {
        sMenuOwner.generate();
    }
    {
        ALGhostStudio& studio = ALGhostStudio::instance();
        const LLUUID shared_row = studio.getSelected().notNull() ? Actions::rowFor(studio.getSelected()) : LLUUID::null;
        if (!keep_multi_selection || shared_row.isNull() || std::find(sRows.begin(), sRows.end(), shared_row) == sRows.end())
        {
            studio.setSelected(Actions::rowFor(primary));   // otherwise the multi-selection stays as it is
        }
    }
    if (!tool->beginEditFor(sMenuOwner))
    {
        tip(str("GhostCloneMenuEditBusy"));
        return;
    }
    tool_mgr->setTransientTool(tool);
    if (!(tool->isEditModeActive() && tool->editOwner() == sMenuOwner))
    {
        tool->stopEditModeFor(sMenuOwner);
    }
}

// ---- entity menu actions ------------------------------------------------------
void actPlayPause()
{
    const Aggregate agg = computeAggregate();
    const bool pause = Logic::pauseAction(agg.mPaused, agg.mEntities);
    if (agg.mEntities == 0)
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    Actions::setPaused(sRows, pause);
}

void actMirror()
{
    const Aggregate agg = computeAggregate();
    if (agg.mEntities == 0 || agg.mPaused > 0)
    {
        tip(str(agg.mEntities == 0 ? "GhostCloneMenuNotReady" : "GhostCloneMenuPausedMirror"));
        return;
    }
    const ALGhostStudio::EDriveMode mode = Logic::allOf(agg.mReflecting, agg.mEntities)
        ? ALGhostStudio::DRIVE_MIRROR : ALGhostStudio::DRIVE_REFLECT;
    Actions::setDriveMode(sRows, mode, LLUUID::null);
}

// Entity clones hold their pose by pausing (the page's Pause button); the
// "Grab pose now" snapshot is the overlay clones' freeze (the page disables it
// for entity clones).
void actFreezePose()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<LLUUID> overlay_rows;
    for (const LLUUID& id : liveTargets())
    {
        if (studio.getInstance(id)->mKind == ALGhostStudio::BACKING_OVERLAY)
        {
            overlay_rows.push_back(id);
        }
    }
    Actions::setPaused(sRows, true);
    if (!overlay_rows.empty() && Actions::freezeOverlays(overlay_rows))
    {
        tip(str("GhostCloneMenuFreezeFail"));
    }
}

// The page refuses Clear while ANY selected clone is posed; recheck at commit.
bool clearAllowed(const Aggregate& agg)
{
    return ALGhostClearAnim::canClear(agg.mEntities > 0, agg.mPosed > 0);
}

void actClearAnimations()
{
    const Aggregate agg = computeAggregate();
    if (!clearAllowed(agg))
    {
        tip(str(agg.mEntities == 0 ? "GhostCloneMenuNotReady" : "GhostCloneMenuPosed"));
        return;
    }
    const S32 cleared = ALPanelGhostStudio::clearAnimationsOnClonesRecorded(liveEntityTargets(), /*director_fallback=*/true);
    tip(cleared > 0 ? ALGhostClearAnim::clearedToast((size_t)cleared) : str("GhostCloneMenuNotReady"));
}

// Stagger: the Crowd tab's Start offset math over exactly the picked group's live
// entity members. No setting is written and the selection is untouched, so the
// kit's setting watcher records nothing: the one custom step below is the only
// history record. One step restores every member's start offset (and the
// group's mode) directly. The Director shell is looked up only after a
// successful apply (a refusal never constructs the Director).
void actStagger(S32 mode)
{
    if (const char* refusal = Actions::staggerRefusal(sPicked))
    {
        tip(str(refusal));
        return;
    }
    const Actions::StaggerState before = Actions::snapshotStagger(sPicked);
    if (!Actions::setStagger(sPicked, mode))
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    const Actions::StaggerState after = Actions::snapshotStagger(sPicked);
    if (before.mOffsets == after.mOffsets)
    {
        return;                         // nothing changed: no undo step
    }
    LLFloater* director = LLFloaterReg::getInstance("director");
    ALKitShell* shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
    if (shell)
    {
        shell->history().recordCustom(shell, "clone_stagger", "Stagger clones",
            [before, after](bool redo, std::string& reason)
            {
                if (!Actions::restoreStagger(redo ? after : before))
                {
                    reason = "Those clones are no longer available";
                    return false;
                }
                return true;
            });
    }
}

void actDuplicate()
{
    const Actions::DuplicateResult copies = Actions::duplicateRows(sRows, true);
    if (copies.mFirst.notNull())
    {
        ALGhostStudio::instance().setSelected(copies.mFirst);
    }
}

void actDelete()
{
    // A clone that vanished while the menu was open must not turn Delete into
    // "something was removed" and clear the user's selection.
    std::vector<LLUUID> live_rows;
    for (const LLUUID& row : sRows)
    {
        if (ALGhostStudio::instance().groupForMember(row) || live(row))
        {
            live_rows.push_back(row);
        }
    }
    bool skipped = false;
    const bool removed = Actions::deleteRows(live_rows, &skipped);
    if (skipped)
    {
        tip(str("GhostCloneMenuGroupDelete"));
    }
    if (removed)
    {
        ALGhostStudio::instance().setSelected(LLUUID::null);
    }
}

// ---- registration ---------------------------------------------------------------
void registerEntityCallbacks(LLUICtrl::EnableCallbackRegistry::Registrar& enable,
                             LLUICtrl::CommitCallbackRegistry::Registrar& commit)
{
    // Play / Pause: two items, one visible at a time, so the label is XML text.
    enable.add("GhostClone.VisiblePause", [](LLUICtrl*, const LLSD&)
    {
        return !Logic::allOf(sAgg.mPaused, sAgg.mEntities);
    });
    enable.add("GhostClone.VisiblePlay", [](LLUICtrl*, const LLSD&)
    {
        return Logic::allOf(sAgg.mPaused, sAgg.mEntities);
    });
    enable.add("GhostClone.EnablePlayPause", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, sAgg.mEntities > 0, "GhostCloneMenuNotReady");
    });
    commit.add("GhostClone.PlayPause", [](LLUICtrl*, const LLSD&) { actPlayPause(); });

    enable.add("GhostClone.EnableMirror", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, sAgg.mEntities > 0 && sAgg.mPaused == 0,
                     sAgg.mEntities > 0 ? "GhostCloneMenuPausedMirror" : "GhostCloneMenuNotReady");
    });
    enable.add("GhostClone.CheckMirror", [](LLUICtrl*, const LLSD&)
    {
        return Logic::allOf(sAgg.mReflecting, sAgg.mEntities);
    });
    commit.add("GhostClone.Mirror", [](LLUICtrl*, const LLSD&) { actMirror(); });

    enable.add("GhostClone.EnableTargets", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, sAgg.mLive > 0, "GhostCloneMenuNotReady");
    });
    commit.add("GhostClone.Size", [](LLUICtrl*, const LLSD& param)
    {
        resizeRows(sRows, sizeDirection(param));
    });

    enable.add("GhostClone.EnableStagger", [](LLUICtrl* ctrl, const LLSD&)
    {
        const char* refusal = Actions::staggerRefusal(sPicked);
        return state(ctrl, refusal == nullptr, refusal ? refusal : "GhostCloneMenuNoCrowd");
    });
    enable.add("GhostClone.CheckStagger", [](LLUICtrl*, const LLSD& param)
    {
        return sStaggerCurrent == param.asInteger();
    });
    commit.add("GhostClone.Stagger", [](LLUICtrl*, const LLSD& param) { actStagger(param.asInteger()); });

    enable.add("GhostClone.EnableEdit", [](LLUICtrl* ctrl, const LLSD&)
    {
        const char* reason = editActive() ? nullptr : editBlockReason();
        return state(ctrl, sAgg.mLive > 0 && !reason, reason ? reason : "GhostCloneMenuNotReady");
    });
    enable.add("GhostClone.CheckEdit", [](LLUICtrl*, const LLSD&) { return editActive(); });
    commit.add("GhostClone.Edit", [](LLUICtrl*, const LLSD&)
    {
        // The picked clone, else the first target that is still alive.
        LLUUID target = live(sPicked) ? sPicked : LLUUID::null;
        if (target.isNull())
        {
            const std::vector<LLUUID> alive = liveTargets();
            target = alive.empty() ? LLUUID::null : alive.front();
        }
        if (target.notNull())
        {
            toggleEdit(target, /*keep_multi_selection=*/true);
        }
    });

    commit.add("GhostClone.FreezePose", [](LLUICtrl*, const LLSD&) { actFreezePose(); });

    enable.add("GhostClone.EnableClearAnim", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, clearAllowed(sAgg),
                     sAgg.mEntities == 0 ? "GhostCloneMenuNotReady" : "GhostCloneMenuPosed");
    });
    commit.add("GhostClone.ClearAnim", [](LLUICtrl*, const LLSD&) { actClearAnimations(); });

    enable.add("GhostClone.CheckSolo", [](LLUICtrl*, const LLSD&) { return ALGhostStudio::instance().getSolo(); });
    commit.add("GhostClone.Solo", [](LLUICtrl*, const LLSD&) { Actions::toggleSolo(sRows); });
    commit.add("GhostClone.Duplicate", [](LLUICtrl*, const LLSD&) { actDuplicate(); });
    commit.add("GhostClone.Delete", [](LLUICtrl*, const LLSD&) { actDelete(); });

    enable.add("GhostClone.EnableSelectInDirector", [](LLUICtrl* ctrl, const LLSD&)
    {
        // Revalidated on every check: the picked clone may be gone by now.
        const std::string reason = ALDirectorSelect::cloneRefusal(sPicked);
        setReason(ctrl, reason);
        return reason.empty();
    });
    commit.add("GhostClone.SelectInDirector", [](LLUICtrl*, const LLSD&)
    {
        ALDirectorSelect::selectClone(sPicked, false);
    });
    commit.add("GhostClone.OpenInDirector", [](LLUICtrl*, const LLSD&)
    {
        ALDirectorSelect::openCloneInDirector(sPicked);
    });
}

void registerOverlayCallbacks(LLUICtrl::EnableCallbackRegistry::Registrar& enable,
                              LLUICtrl::CommitCallbackRegistry::Registrar& commit)
{
    enable.add("GhostOverlay.Enable", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, live(sOverlayTarget) != nullptr, "GhostCloneMenuNotReady");
    });
    commit.add("GhostOverlay.Style", [](LLUICtrl*, const LLSD& param)
    {
        const LLUUID target = overlayTarget();
        if (live(target))
        {
            Actions::setOverlayStyle(std::vector<LLUUID>{ target }, param.asInteger());
        }
        refreshOverlayState();
    });
    enable.add("GhostOverlay.CheckStyle", [](LLUICtrl*, const LLSD& param)
    {
        return sOverlayStyle == param.asInteger();
    });
    // Size acts on a whole collapsed group (the page's unit): never from an avatar's
    // overlay submenu, which neither shows nor selects that group.
    enable.add("GhostOverlay.EnableSize", [](LLUICtrl* ctrl, const LLSD&)
    {
        if (live(sOverlayTarget) && Actions::collapsedGroup(sOverlayTarget))
        {
            return state(ctrl, false, "GhostCloneMenuOverlayInGroup");
        }
        return state(ctrl, live(sOverlayTarget) != nullptr, "GhostCloneMenuNotReady");
    });
    commit.add("GhostOverlay.Size", [](LLUICtrl*, const LLSD& param)
    {
        const LLUUID target = overlayTarget();
        if (live(target) && !Actions::collapsedGroup(target))
        {
            resizeRows(std::vector<LLUUID>{ target }, sizeDirection(param));
        }
    });
    // Mirror (the reflect drive) is an entity-clone drive: an overlay has no
    // drive mode, so the item stays visible and disabled with the reason.
    enable.add("GhostOverlay.EnableMirror", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, false, "GhostCloneMenuMirrorEntityOnly");
    });
    commit.add("GhostOverlay.Mirror", [](LLUICtrl*, const LLSD&) {});
    enable.add("GhostOverlay.EnableEdit", [](LLUICtrl* ctrl, const LLSD&)
    {
        const char* reason = editActive() ? nullptr : editBlockReason();
        return state(ctrl, live(sOverlayTarget) != nullptr && !reason, reason ? reason : "GhostCloneMenuNotReady");
    });
    enable.add("GhostOverlay.CheckEdit", [](LLUICtrl*, const LLSD&) { return editActive(); });
    commit.add("GhostOverlay.Edit", [](LLUICtrl*, const LLSD&)
    {
        const LLUUID target = overlayTarget();
        if (live(target))
        {
            toggleEdit(target, /*keep_multi_selection=*/false);
        }
    });
    commit.add("GhostOverlay.Remove", [](LLUICtrl*, const LLSD&)
    {
        const LLUUID target = overlayTarget();
        if (live(target))
        {
            bool skipped = false;
            Actions::deleteRows(std::vector<LLUUID>{ target }, &skipped);
            if (skipped)
            {
                tip(str(Actions::collapsedGroup(target) ? "GhostCloneMenuOverlayInGroup" : "GhostCloneMenuGroupDelete"));
            }
            sChosenOverlay.setNull();
            refreshOverlayState();
        }
    });
    enable.add("GhostOverlay.EnableSelectInDirector", [](LLUICtrl* ctrl, const LLSD&)
    {
        const std::string reason = overlayRefusal();
        setReason(ctrl, reason);
        return reason.empty();
    });
    commit.add("GhostOverlay.SelectInDirector", [](LLUICtrl*, const LLSD&)
    {
        ALDirectorSelect::selectClone(overlayTarget(), false);
    });
    commit.add("GhostOverlay.OpenInDirector", [](LLUICtrl*, const LLSD&)
    {
        ALDirectorSelect::openCloneInDirector(overlayTarget());
    });
}

void registerAvatarCallbacks(LLUICtrl::EnableCallbackRegistry::Registrar& enable,
                             LLUICtrl::CommitCallbackRegistry::Registrar& commit)
{
    // "Select in Director" on self / other avatar and attachment menus.
    enable.add("Avatar.EnableSelectInDirector", [](LLUICtrl* ctrl, const LLSD&)
    {
        const std::string reason = sMenuHud ? str("DirectorSelectHud") : ALDirectorSelect::avatarRefusal(sMenuAvatar);
        setReason(ctrl, reason);
        return reason.empty();
    });
    commit.add("Avatar.SelectInDirector", [](LLUICtrl*, const LLSD&)
    {
        ALDirectorSelect::selectAvatar(sMenuAvatar, false);
    });
}

// The avatar an overlay sits on. An overlay whose source was removed belongs
// to no avatar (a null source means "you" only while it is not removed).
LLUUID overlayOwner(const ALGhostStudio::Instance& inst)
{
    return inst.mSource.isNull() ? gAgentID : inst.mSource;
}

// "Which overlay": every overlay on the avatar, newest first.
void populateWhichMenu(LLContextMenu* which)
{
    which->empty();
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (size_t i = 0; i < sOverlays.size(); ++i)
    {
        const LLUUID id = sOverlays[sOverlays.size() - 1 - i];
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        LLStringUtil::format_map_t args;
        args["[N]"] = std::to_string(i + 1);
        LLMenuItemCheckGL::Params item;
        item.name = "ov_pick";
        item.label = inst && !inst->mName.empty() ? inst->mName : LLTrans::getString("GhostCloneMenuOverlayN", args);
        item.on_click.function = [id](LLUICtrl*, const LLSD&) { sChosenOverlay = id; refreshOverlayState(); };
        item.on_enable.function = [id](LLUICtrl*, const LLSD&) { return live(id) != nullptr; };
        item.on_check.function = [id](LLUICtrl*, const LLSD&) { return id == sOverlayTarget; };
        which->addChild(LLUICtrlFactory::create<LLMenuItemCheckGL>(item));
    }
}

bool gestureContextValid()
{
    return LLToolMgr::getInstance()->getCurrentTool() == sPressTool &&
           gAgentCamera.getCameraMode() == sPressCameraMode &&
           gAgentCamera.getCameraMode() != CAMERA_MODE_MOUSELOOK &&
           gFocusMgr.getAppHasFocus() && !gFocusMgr.getMouseCapture() && !gFocusMgr.getTopCtrl() &&
           LLModalDialog::activeCount() == 0 && !gDisconnected;
}

void showEntityMenu(const LLUUID& clone, S32 x, S32 y)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (!sMenu || !live(clone))
    {
        return;
    }
    const std::vector<LLUUID> host_rows = ALDirectorSelect::hostRows();
    const std::vector<LLUUID> selection = Actions::instances(host_rows);
    const Logic::TargetPlan<LLUUID> plan = Logic::planTargets(clone, selection);
    if (plan.mSelectPickedFirst)
    {
        // Select the picked clone's unit first; a collapsed group is one unit.
        sRows = std::vector<LLUUID>{ Actions::rowFor(clone) };
        studio.setSelected(sRows.front());
    }
    else
    {
        sRows = host_rows;      // the page's rows, group headers kept
    }
    sPicked = clone;
    sAgg = computeAggregate();
    // Stagger exists only when the picked clone's unit is a collapsed group.
    const bool unit_is_group = Actions::collapsedGroup(clone) != nullptr;
    if (LLMenuItemGL* stagger = sMenu->getItem(std::string("Stagger")))
    {
        stagger->setVisible(Logic::staggerVisible(unit_is_group));
    }
    sStaggerCurrent = unit_is_group ? Actions::staggerCurrent(clone) : 0;
    sMenu->show(x, y);
    // The menu was shown after the release, so its spawn point must not stay
    // armed: the holder would swallow the release of the next right-click here.
    LLMenuHolderGL::sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);
}
} // namespace

void registerCallbacks()
{
    LLUICtrl::EnableCallbackRegistry::Registrar& enable =
        LLUICtrl::EnableCallbackRegistry::currentRegistrar();
    LLUICtrl::CommitCallbackRegistry::Registrar& commit =
        LLUICtrl::CommitCallbackRegistry::currentRegistrar();
    registerEntityCallbacks(enable, commit);
    registerOverlayCallbacks(enable, commit);
    registerAvatarCallbacks(enable, commit);
}

void createMenu()
{
    if (sMenu || !gMenuHolder)
    {
        return;
    }
    const auto& registry = LLViewerMenuHolderGL::child_registry_t::instance();
    sMenu = LLUICtrlFactory::createFromFile<LLContextMenu>("menu_ghost_clone.xml", gMenuHolder, registry);
    if (sMenu)
    {
        sMenu->setBackgroundColor(LLUIColorTable::instance().getColor("MenuPopupBgColor"));
    }
    // The Clone overlay branch starts hidden in every avatar menu; it is shown
    // only by prepareAvatarMenu for an avatar that carries an overlay.
    for (LLContextMenu* menu : { gMenuAvatarSelf, gMenuAvatarOther, gMenuAttachmentSelf, gMenuAttachmentOther })
    {
        if (menu)
        {
            prepareAvatarMenu(menu, nullptr);
        }
    }
}

void destroyMenu()
{
    cancelRight();
    sBase.clear();
    delete sMenu;
    sMenu = nullptr;
}

bool pressRight(S32 x, S32 y, const LLPickInfo& pick)
{
    cancelRight();
    if (!sMenu)
    {
        return false;
    }
    const LLUUID clone = ALDirectorSelect::cloneAtPick(pick);
    if (clone.isNull())
    {
        return false;
    }
    // Read the tool first: getCurrentTool() may itself complete a pending tool
    // change, and a tool change cancels the gesture (lltoolmgr.cpp).
    LLTool* press_tool = LLToolMgr::getInstance()->getCurrentTool();
    // An earlier menu's spawn point must not swallow this click's release.
    LLMenuHolderGL::sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);
    sGesture.press(x, y);
    sPendingClone = clone;
    sPressTool = press_tool;
    sPressCameraMode = gAgentCamera.getCameraMode();
    return true;
}

void cancelRight()
{
    sGesture.cancel();
    sPendingClone.setNull();
    sPressTool = nullptr;
}

void noteMouseMove(S32 x, S32 y)
{
    if (!sGesture.active())
    {
        return;
    }
    // A tool that grabbed the mouse (a camera drag) took the gesture over.
    if (gFocusMgr.getMouseCapture())
    {
        cancelRight();
        return;
    }
    sGesture.move(x, y);
}

void releaseRight(S32 x, S32 y)
{
    if (!sGesture.active())
    {
        return;
    }
    const S32 down_x = sGesture.pressX();
    const S32 down_y = sGesture.pressY();
    const bool open = sGesture.release(x, y) && gestureContextValid();
    const LLUUID clone = sPendingClone;
    cancelRight();
    if (open)
    {
        showEntityMenu(clone, down_x, down_y);
    }
}

void prepareAvatarMenu(LLContextMenu* menu, LLVOAvatar* avatar, bool hud_attachment)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    sMenuAvatar = avatar ? avatar->getID() : LLUUID::null;
    sMenuHud = hud_attachment;
    sOverlays.clear();
    if (avatar)
    {
        for (const ALGhostStudio::Instance& inst : studio.getInstances())
        {
            // An orphaned overlay (its source was removed) sits on no avatar.
            if (inst.mKind == ALGhostStudio::BACKING_OVERLAY && !inst.mDeleting && !inst.mSourceRemoved &&
                overlayOwner(inst) == sMenuAvatar)
            {
                sOverlays.push_back(inst.mId);
            }
        }
    }
    if (std::find(sOverlays.begin(), sOverlays.end(), sChosenOverlay) == sOverlays.end())
    {
        sChosenOverlay.setNull();
    }
    refreshOverlayState();
    if (!menu)
    {
        return;
    }
    const bool visible = Logic::overlaySubmenuVisible(sOverlays.size());
    LLContextMenuBranch* branch = dynamic_cast<LLContextMenuBranch*>(menu->getItem(std::string("Clone Overlay")));
    if (!branch)
    {
        return;
    }
    branch->setVisible(visible);
    LLContextMenu* submenu = branch->getBranch();
    LLMenuItemGL* which_item = submenu ? submenu->getItem(std::string("Which Overlay")) : nullptr;
    if (which_item)
    {
        which_item->setVisible(sOverlays.size() > 1);
        submenu->arrange();
        if (sOverlays.size() > 1)
        {
            if (LLContextMenuBranch* which_branch = dynamic_cast<LLContextMenuBranch*>(which_item))
            {
                if (LLContextMenu* which = which_branch->getBranch())
                {
                    populateWhichMenu(which);
                }
            }
        }
    }
}
} // namespace ALGhostCloneMenu
