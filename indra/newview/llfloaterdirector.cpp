/**
 * @file llfloaterdirector.cpp
 * @brief Director Console floater -- see llfloaterdirector.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterdirector.h"

#include "alcompassdial.h"
#include "llactormover.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llclipboard.h"
#include "lldirectorcast.h"
#include "llfloaterreg.h"
#include "llflycamrecorder.h"
#include "llmenugl.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewermenu.h"           // gMenuHolder, LLViewerMenuHolderGL
#include "llviewerobjectlist.h"     // gObjectList
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

namespace
{
// cast-row status glyphs (existing skin icons, poser iconography rule)
constexpr char ICON_MOVING[]   = "Move_Walk_Off";
constexpr char ICON_IDLE[]     = "Profile_Friend_Online";
constexpr char ICON_GONE[]     = "Profile_Friend_Offline";
constexpr char ICON_MARK[]     = "Flag";

// CinematicCamMode value -> display name (matches the mode combo labels)
const char* cinecam_mode_name(S32 mode)
{
    static const char* names[] = {
        "off", "Bone Lock", "Orbit", "Fly Hover", "Sweep", "Crane",
        "Dolly Zoom", "Push-In", "Low Hero", "Overhead", "Over-the-Shoulder",
        "Crash Zoom", "Slow Zoom", "Whip Arc", "Arc Move", "Reveal Rise",
        "Pull-Back", "Two-Shot", "Lead Follow", "ECU Eyes", "Long Lens",
        "Spiral", "Pedestal Rise",
    };
    constexpr S32 count = (S32)(sizeof(names) / sizeof(names[0]));
    return (mode >= 0 && mode < count) ? names[mode] : "?";
}
} // anonymous namespace

LLFloaterDirector::LLFloaterDirector(const LLSD& key)
:   LLFloater(key)
{
}

bool LLFloaterDirector::postBuild()
{
    // ---- transport ----
    mActionBtn = getChild<LLButton>("btn_action");
    mCutBtn = getChild<LLButton>("btn_cut");
    mSetMarksBtn = getChild<LLButton>("btn_set_marks");
    mResetMarksBtn = getChild<LLButton>("btn_reset_marks");
    mActionBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAction(); });
    mCutBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAction(); });
    mSetMarksBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetMarks(); });
    mResetMarksBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickResetMarks(); });

    // ---- cast column ----
    mCastList = getChild<LLScrollListCtrl>("cast_list");
    mCastHint = getChild<LLTextBox>("cast_hint");
    mRemoveBtn = getChild<LLButton>("btn_remove");
    getChild<LLButton>("btn_add_you")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddYou(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastRemove(); });
    mCastList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onCastRightClick(ctrl, x, y); });
    {
        // same LLContextMenu idiom as the Animation Explorer's list menu
        LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
        registrar.add("Director.SetSubjectA", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(true); });
        registrar.add("Director.SetSubjectB", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(false); });
        registrar.add("Director.SetMarkHere", [this](LLUICtrl*, const LLSD&) { onCastSetMarkHere(); });
        registrar.add("Director.ResetToMark", [this](LLUICtrl*, const LLSD&) { onCastResetToMark(); });
        registrar.add("Director.ClearLocoAnim", [this](LLUICtrl*, const LLSD&) { onCastClearLocoAnim(); });
        registrar.add("Director.CopyUUID", [this](LLUICtrl*, const LLSD&) { onCastCopyUUID(); });
        registrar.add("Director.RemoveFromCast", [this](LLUICtrl*, const LLSD&) { onCastRemove(); });
        if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
                "menu_director_cast.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mCastMenuHandle = menu->getHandle();
        }
    }

    // ---- Move tab ----
    mScopeRadio = getChild<LLRadioGroup>("scope_radio");
    mScopeRadio->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScopeCommit(); });
    // initial selection (draw()'s diff-sync only reacts to changes)
    mScopeRadio->setValue(gSavedSettings.getBOOL("ActorMoverSync") ? 1 : 0);
    mHeadingDial = getChild<ALCompassDial>("heading_dial");
    mHeadingDial->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDialCommit(); });
    mWalkBtn = getChild<LLButton>("btn_walk");
    mStopBtn = getChild<LLButton>("btn_stop");
    mWalkBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickWalk(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickStop(); });

    // ---- Animate tab ----
    mAnimateHeader = getChild<LLTextBox>("animate_header");
    getChild<LLButton>("btn_open_animexp")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("animation_explorer"); });

    // ---- Camera tab ----
    mSubjectAText = getChild<LLTextBox>("subject_a_text");
    mSubjectBText = getChild<LLTextBox>("subject_b_text");
    mSetABtn = getChild<LLButton>("btn_set_a");
    mSetBBtn = getChild<LLButton>("btn_set_b");
    mClearABtn = getChild<LLButton>("btn_clear_a");
    mClearBBtn = getChild<LLButton>("btn_clear_b");
    mSetABtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(true); });
    mSetBBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(false); });
    mClearABtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(true); });
    mClearBBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(false); });

    // ---- Takes tab ----
    mTakeRecordBtn = getChild<LLButton>("btn_take_record");
    mTakePlayBtn = getChild<LLButton>("btn_take_play");
    mTakeStopBtn = getChild<LLButton>("btn_take_stop");
    mTakeScrub = getChild<LLSliderCtrl>("take_scrub");
    mTakeTimeText = getChild<LLTextBox>("take_time_text");
    mTakeStatusText = getChild<LLTextBox>("take_status_text");
    mTakeRecordBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTakeRecord(); });
    mTakePlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTakePlayPause(); });
    mTakeStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTakeStop(); });
    mTakeScrub->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTakeScrub(); });

    // ---- status strip ----
    mStatusStrip = getChild<LLTextBox>("status_strip");

    return true;
}

void LLFloaterDirector::draw()
{
    refreshCastList();
    refreshTransport();
    refreshMoveTab();
    refreshCameraTab();
    refreshTakesTab();
    refreshStatusStrip();
    LLFloater::draw();
}

//static
void LLFloaterDirector::setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}

// ---------------------------------------------------------------------------
// transport
// ---------------------------------------------------------------------------
void LLFloaterDirector::onClickAction()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (cast.isRunning() || cast.isCountingDown())
    {
        cast.cut();
    }
    else
    {
        cast.action();
    }
}

void LLFloaterDirector::onClickSetMarks()
{
    LLDirectorCast::instance().setMarks();
}

void LLFloaterDirector::onClickResetMarks()
{
    LLDirectorCast::instance().resetToMarks();
}

void LLFloaterDirector::refreshTransport()
{
    static LLCachedControl<bool> arm_moves(gSavedSettings, "DirectorArmMoves", true);
    static LLCachedControl<bool> arm_camera(gSavedSettings, "DirectorArmCamera", true);
    static LLCachedControl<bool> arm_play(gSavedSettings, "DirectorArmRecorderPlay", false);
    static LLCachedControl<bool> arm_capture(gSavedSettings, "DirectorArmRecorderCapture", false);

    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool counting = cast.isCountingDown();
    const bool running = cast.isRunning();

    // ACTION morphs to CUT (danger styling from XML) while counting/running;
    // the countdown shows on the button itself
    mActionBtn->setVisible(!counting && !running);
    mCutBtn->setVisible(counting || running);
    if (counting)
    {
        mCutBtn->setLabel(LLStringExplicit(
            llformat("%d\xE2\x80\xA6", (S32)ceilf(cast.countdownRemaining()))));
        setToolTipIfChanged(mCutBtn, "Counting down. Click to cancel");
    }
    else if (running)
    {
        mCutBtn->setLabel(LLStringExplicit("CUT"));
        setToolTipIfChanged(mCutBtn, "Stop everything ACTION started");
    }

    // the ACTION tooltip lists exactly what will fire (usability rule 3)
    std::string fires;
    if (arm_moves)   fires += "actor moves, ";
    if (arm_camera)  fires += "cinematic camera, ";
    if (arm_capture) fires += "recorder capture, ";
    else if (arm_play) fires += "recorder playback, ";
    if (fires.empty())
    {
        fires = "Nothing armed: check at least one 'Action fires' box";
        mActionBtn->setEnabled(false);
    }
    else
    {
        fires = "Fires: " + fires.substr(0, fires.size() - 2);
        static LLCachedControl<F32> delay(gSavedSettings, "DirectorActionDelay", 0.f);
        if ((F32)delay > 0.01f)
        {
            fires += llformat(" after a %.1f s countdown", (F32)delay);
        }
        mActionBtn->setEnabled(true);
    }
    setToolTipIfChanged(mActionBtn, fires);

    // marks
    const bool have_cast = !cast.getCast().empty();
    bool any_marked = false;
    for (const auto& m : cast.getCast())
    {
        if (m.mHasMark) { any_marked = true; break; }
    }
    mSetMarksBtn->setEnabled(have_cast);
    setToolTipIfChanged(mSetMarksBtn, have_cast
        ? std::string("Snapshot every cast member's current position as their mark")
        : std::string("Add cast members first"));
    mResetMarksBtn->setEnabled(any_marked);
    setToolTipIfChanged(mResetMarksBtn, any_marked
        ? std::string("Snap every marked cast member back to their mark (local only)")
        : std::string("Set marks first"));
}

// ---------------------------------------------------------------------------
// cast column
// ---------------------------------------------------------------------------
//static
std::string LLFloaterDirector::castMemberName(const LLUUID& id)
{
    if (const LLDirectorCast::CastMember* m = LLDirectorCast::instance().getMember(id);
        m && !m->mLastName.empty())
    {
        return m->mLastName;
    }
    if (LLAvatarName av_name; LLAvatarNameCache::get(id, &av_name))
    {
        return av_name.getCompleteName();
    }
    LLViewerObject* obj = gObjectList.findObject(id);
    LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
    return (av && av->isControlAvatar())
        ? "Animesh " + id.asString().substr(0, 8)
        : id.asString().substr(0, 8);
}

void LLFloaterDirector::refreshCastList()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const uuid_vec_t& ids = cast.getIds();

    mCastHint->setVisible(ids.empty());

    // membership change -> rebuild rows (rare); otherwise only re-set cells
    std::vector<LLScrollListItem*> items = mCastList->getAllData();
    bool rebuild = items.size() != ids.size();
    if (!rebuild)
    {
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (items[i]->getValue().asUUID() != ids[i])
            {
                rebuild = true;
                break;
            }
        }
    }
    if (rebuild)
    {
        const LLUUID prev_sel = firstSelectedCastId();
        mCastList->deleteAllItems();
        mRowStates.clear();
        mRowStates.resize(ids.size());
        for (const LLUUID& id : ids)
        {
            LLSD row;
            row["value"] = id;
            row["columns"][0]["column"] = "status";
            row["columns"][0]["type"] = "icon";
            row["columns"][0]["value"] = "";
            row["columns"][1]["column"] = "name";
            row["columns"][1]["value"] = "";
            row["columns"][2]["column"] = "ab";
            row["columns"][2]["value"] = "";
            row["columns"][3]["column"] = "mark";
            row["columns"][3]["type"] = "icon";
            row["columns"][3]["value"] = "";
            mCastList->addElement(row, ADD_BOTTOM);
        }
        if (prev_sel.notNull())
        {
            mCastList->selectByID(prev_sel);
        }
        items = mCastList->getAllData();
    }

    const S32 status_col = mCastList->getColumn("status")->mIndex;
    const S32 name_col = mCastList->getColumn("name")->mIndex;
    const S32 ab_col = mCastList->getColumn("ab")->mIndex;
    const S32 mark_col = mCastList->getColumn("mark")->mIndex;

    LLActorMover& mover = LLActorMover::instance();
    for (size_t i = 0; i < items.size(); ++i)
    {
        LLScrollListItem* item = items[i];
        CastRowState& state = mRowStates[i];
        const LLUUID id = item->getValue().asUUID();

        LLVOAvatar* av = cast.resolve(id);      // also refreshes cached name
        const bool in_world = av != nullptr;
        const bool moving = in_world && mover.isMoving(id);

        const std::string icon = !in_world ? ICON_GONE
                               : moving    ? ICON_MOVING
                                           : ICON_IDLE;
        std::string name = castMemberName(id);
        if (!in_world)
        {
            name += " (away)";
        }
        std::string ab;
        if (cast.getSubjectA() == id) ab = "A";
        else if (cast.getSubjectB() == id) ab = "B";
        const LLDirectorCast::CastMember* m = cast.getMember(id);
        const std::string mark = (m && m->mHasMark) ? ICON_MARK : "";

        if (state.mIcon != icon)
        {
            if (LLScrollListCell* cell = item->getColumn(status_col))
            {
                cell->setValue(icon);
            }
            state.mIcon = icon;
        }
        if (state.mName != name || state.mInWorld != in_world)
        {
            if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(name_col)))
            {
                cell->setText(name);
                // out-of-world members stay in the cast, grayed
                cell->setColor(in_world ? LLColor4::white : LLColor4::grey);
            }
            state.mName = name;
            state.mInWorld = in_world;
        }
        if (state.mAB != ab)
        {
            if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(ab_col)))
            {
                cell->setText(ab);
            }
            state.mAB = ab;
        }
        if (state.mMark != mark)
        {
            if (LLScrollListCell* cell = item->getColumn(mark_col))
            {
                cell->setValue(mark);
            }
            state.mMark = mark;
        }
    }

    // remove needs a selection (tooltip says so, usability rule 1)
    const bool have_sel = mCastList->getFirstSelected() != nullptr;
    mRemoveBtn->setEnabled(have_sel);
    setToolTipIfChanged(mRemoveBtn, have_sel
        ? std::string("Remove the selected member(s) from the cast")
        : std::string("Select a cast member first"));
}

uuid_vec_t LLFloaterDirector::selectedCastIds() const
{
    uuid_vec_t ids;
    for (LLScrollListItem* item : mCastList->getAllSelected())
    {
        ids.push_back(item->getValue().asUUID());
    }
    return ids;
}

LLUUID LLFloaterDirector::firstSelectedCastId() const
{
    LLScrollListItem* item = mCastList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

void LLFloaterDirector::onCastRightClick(LLUICtrl* ctrl, S32 x, S32 y)
{
    LLScrollListItem* item = mCastList->hitItem(x, y);
    LLContextMenu* menu = mCastMenuHandle.get();
    if (item && menu)
    {
        if (!item->getSelected())
        {
            const S32 index = mCastList->getItemIndex(item);
            if (index >= 0)
            {
                mCastList->selectNthItem(index);
            }
        }
        menu->buildDrawLabels();
        menu->updateParent(LLMenuGL::sMenuContainer);
        menu->show(x, y);
        LLMenuGL::showPopup(ctrl, menu, x, y);
    }
}

void LLFloaterDirector::onClickAddYou()
{
    if (isAgentAvatarValid())
    {
        LLDirectorCast::instance().add(gAgentAvatarp->getID());
    }
}

void LLFloaterDirector::onCastSetSubject(bool subject_a)
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (subject_a)
    {
        // a body can only be one subject; taking A drops a stale B
        if (cast.getSubjectB() == id) cast.setSubjectB(LLUUID::null);
        cast.setSubjectA(id);
    }
    else
    {
        if (cast.getSubjectA() == id) cast.setSubjectA(LLUUID::null);
        cast.setSubjectB(id);
    }
}

void LLFloaterDirector::onCastSetMarkHere()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        LLVOAvatar* av = cast.resolve(id);
        LLDirectorCast::CastMember* m = cast.getMember(id);
        if (av && av->getRootJoint() && m)
        {
            // current RENDERED root, same convention as setMarks()
            m->mMark = av->getRootJoint()->getWorldPosition();
            m->mHasMark = true;
        }
    }
}

void LLFloaterDirector::onCastResetToMark()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        if (const LLDirectorCast::CastMember* m = cast.getMember(id); m && m->mHasMark)
        {
            LLActorMover::instance().placeAt(id, m->mMark);
        }
    }
}

void LLFloaterDirector::onCastClearLocoAnim()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        if (LLDirectorCast::CastMember* m = cast.getMember(id))
        {
            m->mLocoAnim.setNull();
        }
    }
}

void LLFloaterDirector::onCastCopyUUID()
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    // same LLClipboard idiom as the Animation Explorer's Copy UUID
    LLWString idwstr = utf8string_to_wstring(id.asString());
    LLClipboard::instance().copyToClipboard(idwstr, 0, narrow(idwstr.size()));
}

void LLFloaterDirector::onCastRemove()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        cast.remove(id);
    }
}

// ---------------------------------------------------------------------------
// Move tab
// ---------------------------------------------------------------------------
void LLFloaterDirector::onScopeCommit()
{
    // segmented Selected|Everyone drives the same setting as the standalone
    // mover's "Sync all" checkbox (1 = Everyone = sync on)
    gSavedSettings.setBOOL("ActorMoverSync", mScopeRadio->getValue().asInteger() == 1);
}

void LLFloaterDirector::onDialCommit()
{
    // live while dragging, so the in-world heading ray tracks the needle
    gSavedSettings.setF32("ActorMoverHeading", (F32)mHeadingDial->getValue().asReal());
}

void LLFloaterDirector::onClickWalk()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    if (sync)
    {
        LLActorMover::instance().startAll();
    }
    else
    {
        for (const LLUUID& id : selectedCastIds())
        {
            LLActorMover::instance().start(id);
        }
    }
}

void LLFloaterDirector::onClickStop()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    if (sync)
    {
        LLActorMover::instance().stopAll();
    }
    else
    {
        for (const LLUUID& id : selectedCastIds())
        {
            LLActorMover::instance().stop(id);
        }
    }
}

void LLFloaterDirector::refreshMoveTab()
{
    static LLCachedControl<bool> sync(gSavedSettings, "ActorMoverSync", true);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);

    // two-way: reflect external changes (slider in the standalone floater,
    // debug settings) without fighting the dial's own live commits
    const S32 want = sync ? 1 : 0;
    if (mScopeRadio->getValue().asInteger() != want)
    {
        mScopeRadio->setValue(want);
    }
    if (fabsf((F32)mHeadingDial->getValue().asReal() - (F32)heading) > 0.01f)
    {
        mHeadingDial->setValue((F32)heading);
    }

    const bool have_sel = mCastList->getFirstSelected() != nullptr;
    const bool enabled = sync || have_sel;
    mWalkBtn->setEnabled(enabled);
    mStopBtn->setEnabled(enabled);
    const std::string walk_tip = enabled
        ? std::string(sync ? "Start every cast member (you, if the cast is empty) with these parameters"
                           : "Start the selected cast member(s) with these parameters")
        : std::string("Select a cast member first (or switch to Everyone)");
    const std::string stop_tip = enabled
        ? std::string(sync ? "Stop every cast member and release their rendered bodies"
                           : "Stop the selected cast member(s)")
        : std::string("Select a cast member first (or switch to Everyone)");
    setToolTipIfChanged(mWalkBtn, walk_tip);
    setToolTipIfChanged(mStopBtn, stop_tip);

    // Animate tab header rides the same selection
    mAnimateHeader->setText(have_sel ? castMemberName(firstSelectedCastId())
                                     : std::string("No cast member selected"));
}

// ---------------------------------------------------------------------------
// Camera tab
// ---------------------------------------------------------------------------
//static
LLUUID LLFloaterDirector::avatarFromSelection()
{
    LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (obj)
    {
        LLVOAvatar* av = obj->getAvatar();  // avatar itself or attachment parent
        if (av && !av->isDead())
        {
            return av->getID();
        }
    }
    return LLUUID::null;
}

void LLFloaterDirector::onClickSetSubjectFromSelection(bool subject_a)
{
    const LLUUID id = avatarFromSelection();
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    cast.add(id);       // subjects are ids into the cast; no-op if present
    if (subject_a)
    {
        if (cast.getSubjectB() == id) cast.setSubjectB(LLUUID::null);
        cast.setSubjectA(id);
    }
    else
    {
        if (cast.getSubjectA() == id) cast.setSubjectA(LLUUID::null);
        cast.setSubjectB(id);
    }
}

void LLFloaterDirector::onClickClearSubject(bool subject_a)
{
    if (subject_a)
    {
        LLDirectorCast::instance().setSubjectA(LLUUID::null);
    }
    else
    {
        LLDirectorCast::instance().setSubjectB(LLUUID::null);
    }
}

void LLFloaterDirector::refreshCameraTab()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID a = cast.getSubjectA();
    const LLUUID b = cast.getSubjectB();

    mSubjectAText->setText("A: " + (a.notNull() ? castMemberName(a)
                                                : std::string("\xE2\x80\x94")));
    mSubjectBText->setText("B: " + (b.notNull() ? castMemberName(b)
                                                : std::string("\xE2\x80\x94")));

    const bool have_sel_av = avatarFromSelection().notNull();
    mSetABtn->setEnabled(have_sel_av);
    mSetBBtn->setEnabled(have_sel_av);
    const std::string set_tip = have_sel_av
        ? std::string("Make the selected avatar this subject (adds it to the cast)")
        : std::string("Select an avatar or animesh in world first");
    setToolTipIfChanged(mSetABtn, set_tip);
    setToolTipIfChanged(mSetBBtn, set_tip);

    mClearABtn->setEnabled(a.notNull());
    setToolTipIfChanged(mClearABtn, a.notNull()
        ? std::string("Clear Subject A; the camera falls back to follow-target / selection / you")
        : std::string("Subject A is not set"));
    mClearBBtn->setEnabled(b.notNull());
    setToolTipIfChanged(mClearBBtn, b.notNull()
        ? std::string("Clear Subject B; Two-Shot/OTS fall back to you + target")
        : std::string("Subject B is not set"));
}

// ---------------------------------------------------------------------------
// Takes tab (mirrors llfloaterflycamrecorder.cpp against the same API)
// ---------------------------------------------------------------------------
void LLFloaterDirector::onTakeRecord()
{
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    if (rec.getState() == LLFlycamRecorder::STATE_RECORDING)
    {
        rec.stopRecording();
    }
    else
    {
        rec.startRecording();
    }
}

void LLFloaterDirector::onTakePlayPause()
{
    LLFlycamRecorder::instance().togglePlayback();
}

void LLFloaterDirector::onTakeStop()
{
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    if (rec.getState() == LLFlycamRecorder::STATE_RECORDING)
    {
        rec.stopRecording();
    }
    else
    {
        rec.stopPlayback();
    }
}

void LLFloaterDirector::onTakeScrub()
{
    // commit only fires on user interaction (refresh's setValue doesn't),
    // so this is always a deliberate scrub; from idle it previews the pose
    LLFlycamRecorder::instance().seek(mTakeScrub->getValueF32());
}

void LLFloaterDirector::refreshTakesTab()
{
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    const LLFlycamRecorder::EState state = rec.getState();
    const F32 duration = rec.getDuration();
    const bool recording = (state == LLFlycamRecorder::STATE_RECORDING);
    const bool have_take = rec.getNumKeyframes() > 0;

    mTakeRecordBtn->setLabel(recording ? LLStringExplicit("Stop rec")
                                       : LLStringExplicit("Record"));
    mTakePlayBtn->setLabel(state == LLFlycamRecorder::STATE_PLAYING
                               ? LLStringExplicit("Pause")
                               : LLStringExplicit("Play"));
    mTakePlayBtn->setEnabled(have_take && !recording);
    setToolTipIfChanged(mTakePlayBtn,
        !have_take ? std::string("Record or load a take first")
        : recording ? std::string("Stop recording first")
                    : std::string("Play or pause the recorded take (takes over the camera)"));
    mTakeScrub->setEnabled(have_take && !recording);
    setToolTipIfChanged(mTakeScrub,
        !have_take ? std::string("Record or load a take first")
        : recording ? std::string("Stop recording first")
                    : std::string("Scrub through the take (s). Dragging while stopped previews that moment"));

    mTakeScrub->setMaxValue(llmax(duration, 0.01f));
    mTakeScrub->setValue(rec.getPlayhead());

    const F32 shown = recording ? duration : rec.getPlayhead();
    mTakeTimeText->setText(llformat("%.1f / %.1f s   %d keys",
                                    shown, duration, rec.getNumKeyframes()));
    mTakeStatusText->setText(rec.getStatus());
}

// ---------------------------------------------------------------------------
// status strip
// ---------------------------------------------------------------------------
void LLFloaterDirector::refreshStatusStrip()
{
    static LLCachedControl<bool> cam_enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  cam_mode(gSavedSettings, "CinematicCamMode", 0);
    static LLCachedControl<bool> orbit_enabled(gSavedSettings, "FlycamOrbitEnabled", false);

    LLDirectorCast& cast = LLDirectorCast::instance();
    LLActorMover& mover = LLActorMover::instance();

    S32 moving = 0;
    for (const LLUUID& id : cast.getIds())
    {
        if (mover.isMoving(id))
        {
            ++moving;
        }
    }

    std::string cam;
    if (cam_enabled && (S32)cam_mode > 0)
    {
        // name the framing target: Subject A wins (mirrors resolveTarget()),
        // then the selection when that targeting mode is on, else you
        static LLCachedControl<bool> use_selected(gSavedSettings, "CinematicCamUseSelected", false);
        std::string target = "you";
        if (cast.getSubjectA().notNull())
        {
            target = castMemberName(cast.getSubjectA());
        }
        else if (use_selected)
        {
            const LLUUID sel = avatarFromSelection();
            target = sel.notNull() ? castMemberName(sel) : std::string("selection");
        }
        cam = llformat("CineCam: %s \xE2\x86\x92 %s", cinecam_mode_name(cam_mode), target.c_str());
    }
    else
    {
        cam = "CineCam: off";
    }

    const bool rec = LLFlycamRecorder::instance().getState() == LLFlycamRecorder::STATE_RECORDING;
    mStatusStrip->setText(llformat("%d moving \xC2\xB7 %s \xC2\xB7 Orbit %s \xC2\xB7 REC %s",
                                   moving, cam.c_str(),
                                   orbit_enabled ? "on" : "off",
                                   rec ? "\xE2\x97\x8F" : "\xE2\x97\x8B"));
}
