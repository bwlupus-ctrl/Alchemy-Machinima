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

#include "alavatarsubjectpicker.h"
#include "aldirectorselect.h"
#include "alghostanimassetindex.h"
#include "alghostanimdrop.h"
#include "alghostclearanim.h"
#include "alghostcloneactions.h"
#include "alghostclonemenulogic.h"
#include "algazenearby.h"
#include "algazenearbyname.h"
#include "alghoststudio.h"
#include "alkithistory.h"
#include "alkitshell.h"
#include "lldirectorcast.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "alpanelghoststudio.h"
#include "altoolghostedit.h"
#include "altoolghostplace.h"
#include "llagent.h"
#include "llactormover.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llinventoryfunctions.h"
#include "llviewerinventory.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
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
#include "llinventorymodel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llviewercamera.h"
#include "llviewermenu.h"           // gMenuHolder, the four avatar menus
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "rlvactions.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
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
    bool mGazeAgree = true;     // every entity target has the same gaze (enabled, target)
    bool mGazeOn = false;
    size_t mGazeBodies = 0;     // entity targets with a spawned body (gaze needs one)
    S32 mGazeMode = -1;         // LLActorMover GAZE_*, -1 when off
    LLUUID mGazeCast;           // the GAZE_CAST target
    bool mSpeedSame = true;     // every entity target has the same animation speed
    F32 mSpeed = 1.f;
    size_t mAtFastest = 0;      // entity targets at the top rung (4)
    size_t mAtSlowest = 0;      // entity targets at or below the bottom rung (0.25)
    size_t mAtNormal = 0;       // entity targets at exactly 1
    size_t mNotLive = 0;        // entity targets not on the live mirror (Back to live acts on them)
    size_t mOverlays = 0;       // [OverlayMenu] overlay targets (Style acts on these only)
    S32 mOverlayStyle = -1;     // the overlay targets' common style, -1 when they differ
    size_t mPaused = 0;
    size_t mReflecting = 0;
    size_t mPosed = 0;
    S32 mLook = -1;             // the entity targets' common EGhostLook, -1 when they differ
    size_t mFadeOutable = 0;    // entity targets Fade out can still hide (not user-hidden)
    size_t mFadeInable = 0;     // entity targets Fade in can still show (user-hidden)
    bool mExpandedMember = false;   // a target is one member of an EXPANDED group (look / turn refuse it)
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

std::vector<LLUUID> liveOverlayTargets()
{
    std::vector<LLUUID> ids;
    for (const LLUUID& id : liveTargets())
    {
        if (ALGhostStudio::instance().getInstance(id)->mKind == ALGhostStudio::BACKING_OVERLAY)
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
    agg.mExpandedMember = Actions::hasExpandedMember(sRows);
    for (const LLUUID& id : liveTargets())
    {
        ++agg.mLive;
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            ++agg.mOverlays;
            if (agg.mOverlays == 1)
            {
                agg.mOverlayStyle = inst->mStyle;
            }
            else if (agg.mOverlayStyle != inst->mStyle)
            {
                agg.mOverlayStyle = -1;
            }
            continue;
        }
        ++agg.mEntities;
        if (inst->mEntityId.notNull())
        {
            ++agg.mGazeBodies;
            LLActorMover& mover = LLActorMover::instance();
            const bool on = mover.isGazeEnabled(inst->mEntityId);
            const S32 mode = on ? mover.getGazeTargetMode(inst->mEntityId) : -1;
            const LLUUID cast = mode == LLActorMover::GAZE_CAST ? mover.getGazeCastTarget(inst->mEntityId) : LLUUID::null;
            if (agg.mEntities == 1)
            {
                agg.mGazeOn = on;
                agg.mGazeMode = mode;
                agg.mGazeCast = cast;
            }
            else if (agg.mGazeOn != on || agg.mGazeMode != mode || agg.mGazeCast != cast)
            {
                agg.mGazeAgree = false;
            }
        }
        {
            const F32 speed = inst->mAnimSpeed;
            if (agg.mEntities == 1)
            {
                agg.mSpeed = speed;
            }
            else if (std::fabs(agg.mSpeed - speed) > Logic::ANIM_SPEED_EPSILON)
            {
                agg.mSpeedSame = false;
            }
            agg.mAtFastest += speed >= Logic::ANIM_SPEED_LADDER[Logic::ANIM_SPEED_RUNGS - 1] - Logic::ANIM_SPEED_EPSILON ? 1 : 0;
            agg.mAtSlowest += speed <= Logic::ANIM_SPEED_LADDER[0] + Logic::ANIM_SPEED_EPSILON ? 1 : 0;
            agg.mAtNormal += std::fabs(speed - 1.f) <= Logic::ANIM_SPEED_EPSILON ? 1 : 0;
        }
        agg.mNotLive += inst->mDriveMode != ALGhostStudio::DRIVE_MIRROR ? 1 : 0;
        agg.mFadeOutable += Logic::fadeEligible(true, inst->mUserHidden) ? 1 : 0;
        agg.mFadeInable += Logic::fadeEligible(false, inst->mUserHidden) ? 1 : 0;
        if (agg.mEntities == 1)
        {
            agg.mLook = (S32)inst->mLook;
        }
        else if (agg.mLook != (S32)inst->mLook)
        {
            agg.mLook = -1;
        }
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

// Back to live (Animation submenu): a paused target is resumed through Play's own path first, then
// every entity target still off the live mirror (cleared, Directed, Reflect, True mirror) is set to
// DRIVE_MIRROR, which also ends the cleared state. Like Pause / Mirror, this records no undo step.
void actBackToLive()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const std::vector<LLUUID> entities = liveEntityTargets();
    if (entities.empty())
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    bool resumed = false;
    for (const LLUUID& id : entities)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        resumed = resumed || (inst && inst->mDriveMode == ALGhostStudio::DRIVE_FROZEN);
    }
    Actions::setPaused(entities, false);
    std::vector<LLUUID> off_mirror;
    for (const LLUUID& id : entities)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && inst->mDriveMode != ALGhostStudio::DRIVE_MIRROR)
        {
            off_mirror.push_back(id);
        }
    }
    if (off_mirror.empty())
    {
        if (!resumed)
        {
            tip(str("GhostCloneMenuAlreadyLive"));      // nothing was paused and nothing was off the mirror
        }
    }
    else
    {
        Actions::setDriveMode(off_mirror, ALGhostStudio::DRIVE_MIRROR, LLUUID::null);
    }
    sAgg = computeAggregate();
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

// ---- Look / turn, Animation, Appearance -------------------------------------------
// Every item acts on exactly sRows (the picked clone, or the whole multi-selection when
// the picked clone belongs to it) through the shared actions in alghostcloneactions.cpp.
// The lists below (Nearby people, Recent animations, Swap source) are built when the menu
// opens and show names only: an item carries its id in its callback, never in its label.
LLContextMenu* subMenu(LLContextMenu* parent, const char* name)
{
    LLMenuItemGL* item = parent ? parent->getItem(std::string(name)) : nullptr;
    LLContextMenuBranch* branch = dynamic_cast<LLContextMenuBranch*>(item);
    return branch ? branch->getBranch() : nullptr;
}

void addPlaceholder(LLContextMenu* menu, const std::string& label)
{
    LLMenuItemCallGL::Params item;
    item.name = "Nobody";
    item.label = label;
    item.enabled = false;
    menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
}

void addListItem(LLContextMenu* menu, const std::string& label, std::function<bool()> enabled,
                 std::function<void()> click)
{
    LLMenuItemCallGL::Params item;
    item.name = "list_item";
    item.label = label;
    item.on_click.function = [click](LLUICtrl*, const LLSD&) { click(); };
    item.on_enable.function = [enabled](LLUICtrl*, const LLSD&) { return enabled(); };
    menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item));
}

// A person the clone can look at / turn toward: a real avatar still in the world.
bool personNearby(const LLUUID& id)
{
    LLVOAvatar* avatar = LLDirectorCast::instance().resolve(id, false);
    return avatar && !avatar->isSelf() && ALAvatarSubjectPicker::valid(avatar);
}

// Look at / Turn to face: camera, me, or a nearby avatar (LOOK_TARGET_ACTOR with the
// avatar's agent id: the one resolver finds any avatar, cast member or not).
void actFace(bool turn, ALGhostStudio::ELookTarget target, const LLUUID& target_id)
{
    if (liveTargets().empty())
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    if (Actions::hasExpandedMember(sRows))
    {
        tip(str("GhostCloneMenuWholeGroup"));
        return;
    }
    if (target == ALGhostStudio::LOOK_TARGET_ACTOR && !personNearby(target_id))
    {
        tip(str("GhostCloneMenuPersonGone"));
        return;
    }
    const bool any = turn ? Actions::turnTo(sRows, target, target_id)
                          : Actions::lookAt(sRows, target, target_id);
    if (!any)
    {
        tip(str("GhostCloneMenuNotReady"));
    }
}

void actFaceParam(bool turn, const LLSD& param)
{
    const std::string kind = param.asString();
    if (kind == "stop")
    {
        if (liveTargets().empty())
        {
            tip(str("GhostCloneMenuNotReady"));
            return;
        }
        if (Actions::hasExpandedMember(sRows))
        {
            tip(str("GhostCloneMenuWholeGroup"));
            return;
        }
        if (turn)
        {
            Actions::stopTurning(sRows);
        }
        else
        {
            Actions::stopLooking(sRows);
        }
        return;
    }
    if (kind != "camera" && kind != "me")
    {
        return;                         // an unknown parameter is never a quiet "camera"
    }
    actFace(turn, kind == "me" ? ALGhostStudio::LOOK_TARGET_ME : ALGhostStudio::LOOK_TARGET_CAMERA,
            LLUUID::null);
}

// RLVa @shownames: a restricted name is never listed (you are never restricted).
bool nameHiddenByRlva(const LLUUID& id)
{
    return RlvActions::isRlvEnabled() && id != gAgentID && !RlvActions::canShowName(RlvActions::SNC_DEFAULT, id);
}

struct NearbyEntry
{
    F64 mDistance = 0.;
    std::string mLabel;
    LLUUID mId;
};

// Real avatars in draw distance of the camera, nearest to the picked clone first, up to
// NEARBY_MAX. Display names (LLAvatarNameCache), cast members marked; never an id.
std::vector<NearbyEntry> nearbyPeople()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const ALGhostStudio::Instance* clone = ALGhostStudio::instance().getInstance(sPicked);
    const LLVector3d camera = gAgent.getPosGlobalFromAgent(LLViewerCamera::getInstance()->getOrigin());
    const LLVector3d origin = clone ? clone->mFootGlobal : camera;
    const F32 draw_distance = LLViewerCamera::getInstance()->getFar();
    const std::string suffix = str("GhostCloneMenuCastSuffix");
    std::vector<NearbyEntry> found;
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        if (!avatar || avatar->isSelf() || !ALAvatarSubjectPicker::valid(avatar))
        {
            continue;               // you are "Me"; clones, animesh and dead avatars are not people
        }
        const LLVector3d position = avatar->getPositionGlobal();
        if (dist_vec(position, camera) > draw_distance)
        {
            continue;
        }
        if (nameHiddenByRlva(avatar->getID()))
        {
            continue;               // RLVa hides this name: the person is omitted
        }
        std::string name;
        LLAvatarName av_name;
        if (LLAvatarNameCache::get(avatar->getID(), &av_name))
        {
            name = av_name.getDisplayName();
        }
        NearbyEntry entry;
        entry.mLabel = Logic::nearbyLabel(name, cast.getMember(avatar->getID()) != nullptr, suffix);
        if (entry.mLabel.empty())
        {
            continue;               // no name yet: left out, never listed by id
        }
        entry.mDistance = dist_vec(position, origin);
        entry.mId = avatar->getID();
        found.push_back(entry);
    }
    return Logic::nearbySelect(found);
}

void populateNearbyMenu(LLContextMenu* menu, bool turn, const std::vector<NearbyEntry>& people)
{
    if (!menu)
    {
        return;
    }
    menu->empty();
    for (const NearbyEntry& person : people)
    {
        const LLUUID id = person.mId;
        addListItem(menu, person.mLabel, []() { return sAgg.mLive > 0 && !sAgg.mExpandedMember; },
                    [turn, id]() { actFace(turn, ALGhostStudio::LOOK_TARGET_ACTOR, id); });
    }
    if (people.empty())
    {
        addPlaceholder(menu, str("GhostCloneMenuNobodyNearby"));
    }
}

// Animation > Restart animation: the animation restarts on the entity clones (the call
// the Sync path makes after zeroing offsets).
void actRestartAnimation()
{
    if (!Actions::restartRows(sRows))
    {
        tip(str("GhostCloneMenuNotReady"));
    }
}

// Animation > Recent: the page's own "play on the selected clones" apply (Directed clones
// of the targets, else every target switched to Directed), one undo step on the Director
// shell. The toast carries the name only.
void actPlayRecent(const LLUUID& asset, const std::string& name)
{
    // The drop path plays only an animation the agent owns. An asset-id matcher over the inventory finds
    // it; an entry is dropped only when the inventory is fully fetched and the asset is not in it (a
    // Library animation or a half-fetched inventory never loses its entry).
    struct AssetMatcher : public LLInventoryCollectFunctor
    {
        LLUUID mAsset;
        bool operator()(LLInventoryCategory*, LLInventoryItem* item) override
        {
            return item && item->getType() == LLAssetType::AT_ANIMATION && item->getAssetUUID() == mAsset &&
                   item->getPermissions().getOwner() == gAgentID;
        }
    } matcher;
    matcher.mAsset = asset;
    LLInventoryModel::cat_array_t cats;
    LLInventoryModel::item_array_t items;
    gInventory.collectDescendentsIf(gInventory.getRootFolderID(), cats, items, LLInventoryModel::EXCLUDE_TRASH, matcher);
    if (items.empty())
    {
        if (LLInventoryModelBackgroundFetch::instance().isEverythingFetched())
        {
            Actions::removeRecentAnimation(asset);
            LLStringUtil::format_map_t args;
            args["[NAME]"] = name;
            tip(LLTrans::getString("GhostCloneMenuAnimGone", args));
        }
        else
        {
            tip(str("GhostCloneMenuInventoryLoading"));
        }
        return;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<ALGhostAnimDrop::SelectedClone> selected;
    for (const LLUUID& id : liveEntityTargets())
    {
        selected.emplace_back(id, studio.getInstance(id)->mDriveMode == ALGhostStudio::DRIVE_DIRECTED);
    }
    if (selected.empty())
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    ALPanelGhostStudio::applyInventoryAnimationTo(selected, asset, name, /*director_fallback=*/true);
}

void populateRecentMenu(LLContextMenu* animation)
{
    LLContextMenu* recent = subMenu(animation, "Recent Animations");
    LLMenuItemGL* branch = animation ? animation->getItem(std::string("Recent Animations")) : nullptr;
    if (!recent || !branch)
    {
        return;
    }
    recent->empty();
    const std::vector<Actions::RecentAnimation>& list = Actions::recentAnimations();
    const std::string fallback = str("GhostCloneMenuAnimUnnamed");
    for (const Actions::RecentAnimation& entry : list)
    {
        const LLUUID asset = entry.mAsset;
        const std::string label = Logic::recentLabel(entry.mName, fallback);
        addListItem(recent, label, []() { return sAgg.mEntities > 0; },
                    [asset, label]() { actPlayRecent(asset, label); });
    }
    branch->setVisible(Logic::recentSubmenuVisible(list.size()));      // hidden when empty
    animation->arrange();
}

// A cast member's display name, never its id: the Cast's cached name, else the name
// cache, else the live avatar's name. Empty when none is known (the member is left out).
std::string castMemberName(const LLUUID& id)
{
    if (nameHiddenByRlva(id))
    {
        return std::string();       // RLVa hides this name: the member is left out
    }
    std::string name;
    if (const LLDirectorCast::CastMember* member = LLDirectorCast::instance().getMember(id))
    {
        name = member->mLastName;
    }
    if (name.empty())
    {
        LLAvatarName av_name;
        if (LLAvatarNameCache::get(id, &av_name))
        {
            name = av_name.getDisplayName();
        }
    }
    if (name.empty())
    {
        if (LLVOAvatar* avatar = LLDirectorCast::instance().resolve(id, false))
        {
            name = avatar->getFullname();
        }
    }
    return Logic::containsUuidText(name) ? std::string() : name;
}

// Appearance > Swap source: the Clones page source combo's "You" and cast members.
void actSwapSource(const LLUUID& source, const std::string& label)
{
    // The Source column label comes from the page's own helper, so both entry points agree.
    (void)label;
    const std::string status = Actions::swapSource(sRows, source, ALPanelGhostStudio::sourceName(source));
    if (!status.empty())
    {
        tip(status);
    }
}

void populateSwapMenu(LLContextMenu* menu)
{
    if (!menu)
    {
        return;
    }
    menu->empty();
    bool any = false;
    for (const LLDirectorCast::ActorChoice& choice : LLDirectorCast::instance().getActorChoices())
    {
        const bool self = LLDirectorCast::isSelf(choice.mId);
        if (!self && LLGhostAvatar::isGhostId(choice.mId))
        {
            continue;               // cast members only, not Studio clones
        }
        const std::string name = self ? str("GhostCloneMenuSwapYou") : castMemberName(choice.mId);
        if (name.empty())
        {
            continue;
        }
        const LLUUID source = self ? LLUUID::null : choice.mId;      // null = You, as on the page
        addListItem(menu, name, []() { return sAgg.mLive > 0; },
                    [source, name]() { actSwapSource(source, name); });
        any = true;
    }
    if (!any)
    {
        addPlaceholder(menu, str("GhostCloneMenuNoCast"));
    }
}

void actEntityLook(S32 look)
{
    if (look < ALGhostStudio::LOOK_NORMAL || look > ALGhostStudio::LOOK_SILHOUETTE ||
        !Actions::setEntityLook(sRows, (ALGhostStudio::EGhostLook)look))
    {
        tip(str("GhostCloneMenuNotReady"));
    }
    sAgg = computeAggregate();
}

// Appearance > Fade out / Fade in: hidden-only (the clone is never deleted), session only. The
// Crowd tab Fade out / Fade in duration drives the ramp (ALGhostStudio::fadeInstance). One undo
// step on the Director shell: undo jumps back to the previous coverage, redo fades again. A
// refusal (nothing eligible) never constructs the Director.
void actFade(bool fade_out)
{
    const std::vector<Actions::FadeBefore> before = Actions::fadeRows(sRows, fade_out);
    if (before.empty())
    {
        tip(str(fade_out ? "GhostCloneMenuAlreadyFaded" : "GhostCloneMenuNotFaded"));
        return;
    }
    LLFloater* director = LLFloaterReg::getInstance("director");
    ALKitShell* shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
    if (shell)
    {
        shell->history().recordCustom(shell, "clone_fade", fade_out ? "Fade out clones" : "Fade in clones",
            [before, fade_out](bool redo, std::string& reason)
            {
                if (!Actions::restoreFade(before, redo, fade_out))
                {
                    reason = "Those clones are no longer available";
                    return false;
                }
                return true;
            });
    }
    sAgg = computeAggregate();
}

// ---- Animation > Speed: the Clones page Anim speed spinner, stepped on a ladder -----------------
void actSpeed(const std::string& kind)
{
    if (kind != "faster" && kind != "slower" && kind != "reset")
    {
        return;                         // an unknown parameter never produces a tip or a change
    }
    if (liveEntityTargets().empty())
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    if (Actions::hasExpandedMember(sRows))
    {
        tip(str("GhostCloneMenuWholeGroup"));
        return;
    }
    const std::vector<Actions::SpeedBefore> before = Actions::snapshotAnimSpeed(sRows);
    if (kind == "reset")
    {
        Actions::setAnimSpeed(sRows, 1.f);
    }
    else
    {
        Actions::stepAnimSpeed(sRows, kind == "faster" ? 1 : -1);
    }
    const std::vector<Actions::SpeedBefore> after = Actions::snapshotAnimSpeed(sRows);
    bool changed = before.size() != after.size();
    for (size_t i = 0; !changed && i < before.size(); ++i)
    {
        changed = before[i].mId != after[i].mId || before[i].mSpeed != after[i].mSpeed;
    }
    if (changed)
    {
        LLFloater* director = LLFloaterReg::getInstance("director");
        ALKitShell* shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
        if (shell)
        {
            shell->history().recordCustom(shell, "clone_anim_speed", "Set clone animation speed",
                [before, after](bool redo, std::string& reason)
                {
                    if (!Actions::restoreAnimSpeed(redo ? after : before))
                    {
                        reason = "Those clones are no longer available";
                        return false;
                    }
                    return true;
                });
        }
    }
    sAgg = computeAggregate();
}

void refreshSpeedInfo()
{
    LLContextMenu* speed = subMenu(subMenu(sMenu, "Animation"), "Anim Speed");
    LLMenuItemGL* info = speed ? speed->getItem(std::string("Speed Info")) : nullptr;
    if (!info)
    {
        return;
    }
    if (sAgg.mEntities > 0 && sAgg.mSpeedSame)
    {
        LLStringUtil::format_map_t args;
        args["[SPEED]"] = llformat("%.2f", sAgg.mSpeed);
        info->setLabel(LLTrans::getString("GhostCloneMenuSpeedInfo", args));
    }
    else
    {
        info->setLabel(str("GhostCloneMenuSpeedMixed"));
    }
}

void addCheckItem(LLContextMenu* menu, const std::string& label, std::function<bool()> enabled,
                  std::function<bool()> checked, std::function<void()> click)
{
    LLMenuItemCheckGL::Params item;
    item.name = "list_item";
    item.label = label;
    item.on_click.function = [click](LLUICtrl*, const LLSD&) { click(); };
    item.on_enable.function = [enabled](LLUICtrl*, const LLSD&) { return enabled(); };
    item.on_check.function = [checked](LLUICtrl*, const LLSD&) { return checked(); };
    menu->addChild(LLUICtrlFactory::create<LLMenuItemCheckGL>(item));
}

// ---- Gaze: the Clones page Motion > Head gaze target + toggle, on the same units as Look at -----
bool gazeUnchanged(const std::vector<Actions::GazeBefore>& a, const std::vector<Actions::GazeBefore>& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].mId != b[i].mId || a[i].mEnabled != b[i].mEnabled || a[i].mMode != b[i].mMode ||
            (a[i].mMode == LLActorMover::GAZE_CAST && a[i].mCast != b[i].mCast))
        {
            return false;
        }
    }
    return true;
}

// mode < 0 with off = true: Off (the page toggle unchecked); otherwise the target is set and gaze
// turned on. One undo step (kit history custom record) on the Director shell.
void actGaze(S32 mode, const LLUUID& cast_target, bool off)
{
    if (liveEntityTargets().empty())
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    if (Actions::hasExpandedMember(sRows))
    {
        tip(str("GhostCloneMenuWholeGroup"));
        return;
    }
    const std::vector<LLUUID> entities = Actions::gazeEntities(sRows);
    if (entities.empty())
    {
        tip(str("GhostCloneMenuNotReady"));
        return;
    }
    const std::vector<Actions::GazeBefore> before = Actions::snapshotGaze(sRows);
    Actions::applyGazeTarget(entities, mode, cast_target, !off);
    const std::vector<Actions::GazeBefore> after = Actions::snapshotGaze(sRows);
    if (!gazeUnchanged(before, after))
    {
        LLFloater* director = LLFloaterReg::getInstance("director");
        ALKitShell* shell = director ? director->findChild<ALKitShell>("director_shell") : nullptr;
        if (shell)
        {
            // The snapshots hold cast target ids: they follow a respawned target clone and fall back to
            // the travel-direction mode when it is deleted (the mover's own rewriteGazeTarget rule).
            struct History
            {
                std::vector<Actions::GazeBefore> mBefore, mAfter;
            };
            auto history = std::make_shared<History>();
            history->mBefore = before;
            history->mAfter = after;
            shell->history().recordCustom(shell, "clone_gaze", "Set clone gaze",
                [history](bool redo, std::string& reason)
                {
                    if (!Actions::restoreGaze(redo ? history->mAfter : history->mBefore))
                    {
                        reason = "Those clones are no longer available";
                        return false;
                    }
                    return true;
                },
                false, {},
                [history](const LLUUID& old_id, const LLUUID& new_id)
                {
                    Actions::rewriteGazeReferences(history->mBefore, old_id, new_id);
                    Actions::rewriteGazeReferences(history->mAfter, old_id, new_id);
                });
        }
    }
    sAgg = computeAggregate();
}

void actGazeParam(const LLSD& param)
{
    const std::string kind = param.asString();
    if (kind == "off")
    {
        actGaze(-1, LLUUID::null, true);
    }
    else if (kind == "camera")
    {
        actGaze(LLActorMover::GAZE_CAMERA, LLUUID::null, false);
    }
    else if (kind == "travel")
    {
        actGaze(LLActorMover::GAZE_TANGENT, LLUUID::null, false);
    }
    else if (kind == "me")
    {
        actGaze(LLActorMover::GAZE_CAST, gAgentID, false);
    }
}

bool gazeChecked(const std::string& kind)
{
    if (!sAgg.mGazeAgree || sAgg.mEntities == 0 || sAgg.mGazeBodies == 0)
    {
        return false;      // no spawned body: nothing is set, so nothing is checked (not even Off)
    }
    if (kind == "off")
    {
        return !sAgg.mGazeOn;
    }
    if (!sAgg.mGazeOn)
    {
        return false;
    }
    return kind == "camera" ? sAgg.mGazeMode == LLActorMover::GAZE_CAMERA
         : kind == "travel" ? sAgg.mGazeMode == LLActorMover::GAZE_TANGENT
         : kind == "me" ? (sAgg.mGazeMode == LLActorMover::GAZE_CAST && sAgg.mGazeCast == gAgentID)
         : false;
}

bool gazeCastChecked(const LLUUID& id)
{
    return sAgg.mGazeBodies > 0 && sAgg.mGazeAgree && sAgg.mGazeOn && sAgg.mGazeMode == LLActorMover::GAZE_CAST && sAgg.mGazeCast == id;
}

bool gazeItemEnabled()
{
    return sAgg.mGazeBodies > 0 && !sAgg.mExpandedMember;
}

// A cast member's label for the Gaze list: the cast label, never a UUID (getActorChoices falls back to
// the member id for an unnamed entry) and never a name RLVa hides; both show the page's "Unknown".
std::string gazeCastLabel(const LLDirectorCast::ActorChoice& choice)
{
    if (nameHiddenByRlva(choice.mId) || choice.mLabel.empty() || Logic::containsUuidText(choice.mLabel))
    {
        const std::string name = castMemberName(choice.mId);     // RLVa-gated, never an id
        return name.empty() ? std::string("Unknown") : name;
    }
    return choice.mLabel;
}

// An open menu may outlive its cast list: the id is checked against the CURRENT cast at click time
// (a retired id of a respawned or deleted clone is refused with a tip, never written).
void actGazeCast(const LLUUID& id)
{
    bool current = false;
    for (const LLDirectorCast::ActorChoice& choice : LLDirectorCast::instance().getActorChoices())
    {
        current = current || (choice.mId == id && !LLDirectorCast::isSelf(choice.mId));
    }
    const std::vector<LLUUID> own = Actions::gazeEntities(sRows);
    if (!current || std::find(own.begin(), own.end(), id) != own.end())
    {
        tip(str("GhostCloneMenuCastGone"));
        return;
    }
    actGaze(LLActorMover::GAZE_CAST, id, false);
}

// Gaze > Cast member / Nearby people. Cast members by their cast label (never the clone itself or
// its own unit members, never You: that is "Me"); Nearby people are the real avatars the Look at list
// offers who are not cast members, and none at all under RLV @shownearby. A pick is stored as
// GAZE_CAST + the agent id, like the Gaze page Nearby section.
void populateGazeMenus(const std::vector<NearbyEntry>& people)
{
    LLContextMenu* gaze = subMenu(sMenu, "Gaze");
    LLContextMenu* cast_menu = subMenu(gaze, "Gaze Cast");
    LLContextMenu* near_menu = subMenu(gaze, "Gaze Nearby");
    const std::vector<LLUUID> own = Actions::gazeEntities(sRows);
    std::set<LLUUID> offered;
    if (cast_menu)
    {
        cast_menu->empty();
        bool any = false;
        for (const LLDirectorCast::ActorChoice& choice : LLDirectorCast::instance().getActorChoices())
        {
            offered.insert(choice.mId);
            if (LLDirectorCast::isSelf(choice.mId) ||
                std::find(own.begin(), own.end(), choice.mId) != own.end())
            {
                continue;
            }
            const LLUUID id = choice.mId;
            addCheckItem(cast_menu, gazeCastLabel(choice), []() { return gazeItemEnabled(); },
                         [id]() { return gazeCastChecked(id); },
                         [id]() { actGazeCast(id); });
            any = true;
        }
        if (!any)
        {
            addPlaceholder(cast_menu, str("GhostCloneMenuNoCast"));
        }
    }
    if (near_menu)
    {
        near_menu->empty();
        bool any = false;
        if (ALGazeNearby::nearbyAllowed())
        {
            for (const NearbyEntry& person : people)
            {
                if (offered.count(person.mId) || LLDirectorCast::instance().getMember(person.mId))
                {
                    continue;       // cast members are listed under Cast member
                }
                const LLUUID id = person.mId;
                addCheckItem(near_menu, person.mLabel, []() { return gazeItemEnabled(); },
                             [id]() { return gazeCastChecked(id); },
                             [id]()
                             {
                                 if (!personNearby(id))
                                 {
                                     tip(str("GhostCloneMenuPersonGone"));
                                     return;
                                 }
                                 actGaze(LLActorMover::GAZE_CAST, id, false);
                             });
                any = true;
            }
        }
        if (!any)
        {
            addPlaceholder(near_menu, str("GhostCloneMenuNobodyNearby"));
        }
    }
    if (gaze)
    {
        gaze->arrange();
    }
}

// Rebuilds every dynamic list for the clone the menu is opening on.
void populateDynamicMenus()
{
    LLContextMenu* look_turn = subMenu(sMenu, "Look Turn");
    const std::vector<NearbyEntry> people = nearbyPeople();       // one scan per open, shared by both lists
    populateNearbyMenu(subMenu(subMenu(look_turn, "Look At"), "Look Nearby"), false, people);
    populateNearbyMenu(subMenu(subMenu(look_turn, "Turn To Face"), "Turn Nearby"), true, people);
    populateGazeMenus(people);
    refreshSpeedInfo();
    populateRecentMenu(subMenu(sMenu, "Animation"));
    populateSwapMenu(subMenu(subMenu(sMenu, "Appearance"), "Swap Source"));
}

// ---- registration ---------------------------------------------------------------
void registerEntityCallbacks(LLUICtrl::EnableCallbackRegistry::Registrar& enable,
                             LLUICtrl::CommitCallbackRegistry::Registrar& commit)
{
    // Play / Pause: two items, one visible at a time, so the label is XML text.
    enable.add("GhostClone.VisiblePause", [](LLUICtrl*, const LLSD&)
    {
        return Logic::entityItemsVisible(sAgg.mEntities) && !Logic::allOf(sAgg.mPaused, sAgg.mEntities);
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

    // Look / turn, Animation, Appearance: sRows through the shared actions.
    // Look / turn act on a whole unit: a single member of an expanded group is refused with a reason.
    enable.add("GhostClone.EnableFace", [](LLUICtrl* ctrl, const LLSD&)
    {
        if (sAgg.mLive > 0 && sAgg.mExpandedMember)
        {
            return state(ctrl, false, "GhostCloneMenuWholeGroup");
        }
        return state(ctrl, sAgg.mLive > 0, "GhostCloneMenuNotReady");
    });
    enable.add("GhostClone.EnableGaze", [](LLUICtrl* ctrl, const LLSD&)
    {
        if (sAgg.mEntities > 0 && sAgg.mExpandedMember)
        {
            return state(ctrl, false, "GhostCloneMenuWholeGroup");
        }
        return state(ctrl, sAgg.mGazeBodies > 0, "GhostCloneMenuNotReady");
    });
    enable.add("GhostClone.EnableSpeed", [](LLUICtrl* ctrl, const LLSD& param)
    {
        const std::string kind = param.asString();
        if (sAgg.mEntities == 0)
        {
            return state(ctrl, false, "GhostCloneMenuNotReady");
        }
        if (sAgg.mExpandedMember)
        {
            return state(ctrl, false, "GhostCloneMenuWholeGroup");
        }
        if (kind == "faster")
        {
            return state(ctrl, sAgg.mAtFastest < sAgg.mEntities, "GhostCloneMenuSpeedAtMax");
        }
        if (kind == "slower")
        {
            return state(ctrl, sAgg.mAtSlowest < sAgg.mEntities, "GhostCloneMenuSpeedAtMin");
        }
        return state(ctrl, sAgg.mAtNormal < sAgg.mEntities, "GhostCloneMenuSpeedAtOne");
    });
    commit.add("GhostClone.Speed", [](LLUICtrl*, const LLSD& param) { actSpeed(param.asString()); });
    enable.add("GhostClone.CheckGaze", [](LLUICtrl*, const LLSD& param) { return gazeChecked(param.asString()); });
    commit.add("GhostClone.Gaze", [](LLUICtrl*, const LLSD& param) { actGazeParam(param); });
    commit.add("GhostClone.Look", [](LLUICtrl*, const LLSD& param) { actFaceParam(false, param); });
    commit.add("GhostClone.Turn", [](LLUICtrl*, const LLSD& param) { actFaceParam(true, param); });
    enable.add("GhostClone.EnableEntities", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, sAgg.mEntities > 0, "GhostCloneMenuNotReady");
    });
    commit.add("GhostClone.RestartAnim", [](LLUICtrl*, const LLSD&) { actRestartAnimation(); });
    enable.add("GhostClone.CheckEntityLook", [](LLUICtrl*, const LLSD& param)
    {
        return sAgg.mLook == param.asInteger();
    });
    commit.add("GhostClone.EntityLook", [](LLUICtrl*, const LLSD& param) { actEntityLook(param.asInteger()); });
    enable.add("GhostClone.EnableFade", [](LLUICtrl* ctrl, const LLSD& param)
    {
        const bool fade_out = param.asString() == "out";
        if (sAgg.mLive == 0)
        {
            return state(ctrl, false, "GhostCloneMenuNotReady");
        }
        if (sAgg.mEntities == 0)
        {
            return state(ctrl, false, "GhostCloneMenuMirrorEntityOnly");       // an overlay is never faded
        }
        return state(ctrl, (fade_out ? sAgg.mFadeOutable : sAgg.mFadeInable) > 0,
                     fade_out ? "GhostCloneMenuAlreadyFaded" : "GhostCloneMenuNotFaded");
    });
    commit.add("GhostClone.Fade", [](LLUICtrl*, const LLSD& param) { actFade(param.asString() == "out"); });

    // [OverlayMenu] Style: the overlay clones among the targets (the same setter and path as the
    // avatar menus' GhostOverlay.Style); entity clones keep their look.
    enable.add("GhostClone.EnableStyle", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, sAgg.mOverlays > 0, "GhostCloneMenuNotReady");
    });
    enable.add("GhostClone.CheckStyle", [](LLUICtrl*, const LLSD& param)
    {
        return sAgg.mOverlayStyle == param.asInteger();
    });
    commit.add("GhostClone.Style", [](LLUICtrl*, const LLSD& param)
    {
        const std::vector<LLUUID> overlays = liveOverlayTargets();
        if (overlays.empty())
        {
            tip(str("GhostCloneMenuNotReady"));
        }
        else
        {
            Actions::setOverlayStyle(overlays, param.asInteger());
        }
        sAgg = computeAggregate();
    });

    // [OverlayMenu] Back to live: every entity target not on the live mirror goes back to it.
    enable.add("GhostClone.EnableBackToLive", [](LLUICtrl* ctrl, const LLSD&)
    {
        return state(ctrl, sAgg.mNotLive > 0,
                     sAgg.mEntities > 0 ? "GhostCloneMenuAlreadyLive" : "GhostCloneMenuNotReady");
    });
    commit.add("GhostClone.BackToLive", [](LLUICtrl*, const LLSD&) { actBackToLive(); });

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

// [OverlayMenu] The menu adapts to the kinds of its targets every time it is presented. Items that
// only entity clones support are HIDDEN (not just disabled) when every target is an overlay clone;
// Style (overlay looks) shows whenever an overlay is a target. Entity-only targets: today's layout,
// Style hidden. Mixed: the entity layout plus Style. Separators never double up or lead / trail.
const char* const ENTITY_ONLY_ITEMS[] = { "Pause", "Play", "Mirror", "Look Turn", "Gaze", "Animation",
                                          "Appearance", "Clear animations" };

void tidySeparators(LLContextMenu* menu)
{
    bool real_since_separator = false;
    LLMenuItemGL* last_shown_separator = nullptr;
    const U32 count = menu->getItemCount();
    for (U32 i = 0; i < count; ++i)
    {
        LLMenuItemGL* item = menu->getItem((S32)i);
        if (!item)
        {
            continue;
        }
        if (dynamic_cast<LLMenuItemSeparatorGL*>(item))
        {
            item->setVisible(real_since_separator);
            last_shown_separator = real_since_separator ? item : last_shown_separator;
            real_since_separator = false;
        }
        else if (item->getVisible())
        {
            real_since_separator = true;
            last_shown_separator = nullptr;
        }
    }
    if (last_shown_separator)
    {
        last_shown_separator->setVisible(false);    // nothing visible after it
    }
}

void applyKindLayout(const Aggregate& agg)
{
    const bool entity_items = Logic::entityItemsVisible(agg.mEntities);
    for (const char* name : ENTITY_ONLY_ITEMS)
    {
        if (LLMenuItemGL* item = sMenu->getItem(std::string(name)))
        {
            item->setVisible(entity_items);
        }
    }
    if (!entity_items)
    {
        if (LLMenuItemGL* stagger = sMenu->getItem(std::string("Stagger")))
        {
            stagger->setVisible(false);     // an overlay group has no start offsets to stagger
        }
    }
    // Appearance > Look is hidden for everyone: the render looks are not built yet, so the item
    // would move a check mark and change nothing (the Clones page combo is untouched).
    if (LLContextMenu* appearance = subMenu(sMenu, "Appearance"))
    {
        if (LLMenuItemGL* look = appearance->getItem(std::string("Entity Look")))
        {
            look->setVisible(false);
        }
    }
    if (LLMenuItemGL* style = sMenu->getItem(std::string("Clone Style")))
    {
        style->setVisible(Logic::styleVisible(agg.mOverlays));
    }
    tidySeparators(sMenu);
    sMenu->arrange();
}

// Fixes what the menu shows about sRows / sPicked and pops it. A world click opens it AFTER the
// release (its spawn point is then disarmed); a Clones list row opens it on the press, where the
// holder's armed spawn point is what keeps the release from clicking an item.
void presentEntityMenu(S32 x, S32 y, bool opened_on_press)
{
    sAgg = computeAggregate();
    // Stagger exists only when the picked clone's unit is a collapsed group.
    const bool unit_is_group = Actions::collapsedGroup(sPicked) != nullptr;
    if (LLMenuItemGL* stagger = sMenu->getItem(std::string("Stagger")))
    {
        stagger->setVisible(Logic::staggerVisible(unit_is_group));
    }
    sStaggerCurrent = unit_is_group ? Actions::staggerCurrent(sPicked) : 0;
    applyKindLayout(sAgg);
    populateDynamicMenus();
    sMenu->show(x, y);
    if (!opened_on_press)
    {
        // The menu was shown after the release, so its spawn point must not stay
        // armed: the holder would swallow the release of the next right-click here.
        LLMenuHolderGL::sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);
    }
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
    presentEntityMenu(x, y, /*opened_on_press=*/false);
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

// The instance a Clones list row stands for: the row itself, or a group header's first live member.
// Null when the row cannot open the menu (not a live clone).
static LLUUID listRowInstance(const LLUUID& row)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (row.isNull())
    {
        return LLUUID::null;
    }
    if (const ALGhostGroupModel::Group* group = studio.groupForMember(row); group && row == group->mId)
    {
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            if (live(member.mInstanceId))
            {
                return member.mInstanceId;
            }
        }
        return LLUUID::null;
    }
    return live(row) ? row : LLUUID::null;
}

bool canOpenForListRow(const LLUUID& row)
{
    return sMenu && listRowInstance(row).notNull();
}

bool openForListRow(const LLUUID& row, S32 x, S32 y)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID picked = listRowInstance(row);
    if (!sMenu || picked.isNull())
    {
        return false;
    }
    // The same targeting rule as a world click: the whole Clones list selection when the row belongs to
    // it, otherwise only that row (selected first).
    const std::vector<LLUUID> host_rows = ALDirectorSelect::hostRows();
    if (std::find(host_rows.begin(), host_rows.end(), row) != host_rows.end())
    {
        sRows = host_rows;
    }
    else
    {
        sRows = std::vector<LLUUID>{ row };
        studio.setSelected(row);
    }
    sPicked = picked;
    presentEntityMenu(x, y, /*opened_on_press=*/true);
    return true;
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
