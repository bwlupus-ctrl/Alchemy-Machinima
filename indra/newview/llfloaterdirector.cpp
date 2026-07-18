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

#include "indra_constants.h"        // KEY_ESCAPE / MASK_NONE

#include "alcompassdial.h"
#include "alpanelcinecamparams.h"   // embedded shared panel (scene preset hooks)
#include "alpanelpatheditor.h"      // embedded shared Actor Pathing editor
#include "llactormover.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "lldir.h"                  // gDirUtilp (scene files)
#include "lldirectorcast.h"
#include "lldiriterator.h"          // scene file listing
#include "llfile.h"                 // LLFile::mkdir/remove, ll*fstream
#include "llfloaterreg.h"
#include "llflycamrecorder.h"
#include "llkeyframemotion.h"       // priority readout (AnimationExplorer idiom)
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"    // scene delete confirm
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"          // scene LLSD XML files
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "lluri.h"                  // LLURI::escape scene filenames
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewermenu.h"           // gMenuHolder, LLViewerMenuHolderGL
#include "llviewerobjectlist.h"     // gObjectList
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()

#include <algorithm>

namespace
{
// cast-row status glyphs (existing skin icons, poser iconography rule)
constexpr char ICON_MOVING[]   = "Move_Walk_Off";
constexpr char ICON_IDLE[]     = "Profile_Friend_Online";
constexpr char ICON_GONE[]     = "Profile_Friend_Offline";
constexpr char ICON_MARK[]     = "Flag";

// left-rail tab icons (existing toolbar-icon skin assets, one family)
constexpr char TAB_ICON_MOVE[]    = "Command_Move_Icon";
constexpr char TAB_ICON_PATH[]    = "Command_Places_Icon";
constexpr char TAB_ICON_ANIMATE[] = "Command_Poser_Icon";
constexpr char TAB_ICON_CAMERA[]  = "Command_View_Icon";
constexpr char TAB_ICON_TAKES[]   = "Command_Snapshot_Icon";

// scene files live beside the cinematic presets, same idiom
constexpr char SCENE_SUBDIR[]  = "director_scenes";
constexpr S32  SCENE_VERSION   = 1;
// playing-marker glyph for the Animate tab's list
constexpr char GLYPH_PLAYING[] = "\xE2\x96\xB6";    // BLACK RIGHT-POINTING TRIANGLE

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

LLFloaterDirector::~LLFloaterDirector()
{
    // AnimationExplorer idiom: menus were parented to gMenuHolder, not us
    if (LLContextMenu* menu = mCastMenuHandle.get())
    {
        menu->die();
        mCastMenuHandle.markDead();
    }
    if (LLContextMenu* menu = mAnimMenuHandle.get())
    {
        menu->die();
        mAnimMenuHandle.markDead();
    }
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
    // Enter anywhere sensible fires ACTION (the default button); while it is
    // hidden (running/counting) Enter does nothing -- Esc is CUT (handleKeyHere)
    setDefaultBtn(mActionBtn);

    // ---- scene files (transport bar) ----
    mSceneCombo = getChild<LLComboBox>("scene_combo");
    mSceneNameEditor = getChild<LLLineEditor>("scene_name_editor");
    mSceneSaveBtn = getChild<LLButton>("btn_scene_save");
    mSceneDeleteBtn = getChild<LLButton>("btn_scene_delete");
    mSceneCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSceneSelected(); });
    mSceneNameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { commitSceneName(); });
    mSceneSaveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneSave(); });
    mSceneDeleteBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSceneDelete(); });
    refreshSceneList();

    // ---- left-rail tabs (remember the active tab, icon each rail button) ----
    mTabContainer = getChild<LLTabContainer>("director_tabs");
    mTabContainer->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTabChanged(); });
    // per-tab icons: image overlay left of the label (existing skin assets)
    struct { const char* panel; const char* icon; } tab_icons[] = {
        { "move_tab",    TAB_ICON_MOVE },
        { "path_tab",    TAB_ICON_PATH },
        { "animate_tab", TAB_ICON_ANIMATE },
        { "camera_tab",  TAB_ICON_CAMERA },
        { "takes_tab",   TAB_ICON_TAKES },
    };
    for (const auto& t : tab_icons)
    {
        if (LLPanel* panel = mTabContainer->getPanelByName(t.panel))
        {
            mTabContainer->setTabImage(panel, t.icon);
        }
    }

    // ---- legend popover ----
    mHelpBtn = getChild<LLButton>("btn_help");
    mLegendPanel = getChild<LLPanel>("legend_panel");
    mHelpBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleLegend(); });
    mLegendPanel->getChild<LLButton>("btn_legend_close")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { mLegendPanel->setVisible(false); });

    // ---- cast column ----
    mCastList = getChild<LLScrollListCtrl>("cast_list");
    mCastHint = getChild<LLTextBox>("cast_hint");
    mRemoveBtn = getChild<LLButton>("btn_remove");
    mFocusBtn = getChild<LLButton>("btn_focus");
    getChild<LLButton>("btn_add_you")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddYou(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastRemove(); });
    mFocusBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickFocusActor(); });
    mCastList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onCastRightClick(ctrl, x, y); });
    // double-click a cast row = Set Subject A (mirrors the right-click path)
    mCastList->setDoubleClickCallback([this]() { onCastSetSubject(true); });
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
        // Animate tab's list menu shares the registrar scope
        registrar.add("Director.AnimCopyUUID", [this](LLUICtrl*, const LLSD&) { onAnimCopyUUID(); });
        registrar.add("Director.AnimSetLoco", [this](LLUICtrl*, const LLSD&) { onAnimSetLoco(); });
        registrar.add("Director.AnimPlayLocal", [this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(true); });
        registrar.add("Director.AnimStopLocal", [this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(false); });
        if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
                "menu_director_cast.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mCastMenuHandle = menu->getHandle();
        }
        if (LLContextMenu* menu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>(
                "menu_director_anims.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mAnimMenuHandle = menu->getHandle();
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

    // ---- Path tab ----
    // the waypoint editor lives in its own tab now; always visible, its target
    // actor re-pointed each draw to the console's cast selection
    mPathPanel = findChild<ALPanelPathEditor>("path_editor");

    // ---- Animate tab ----
    mAnimateHeader = getChild<LLTextBox>("animate_header");
    mAnimList = getChild<LLScrollListCtrl>("anim_list");
    mAnimHint = getChild<LLTextBox>("anim_hint");
    mAnimCopyBtn = getChild<LLButton>("btn_anim_copy");
    mAnimSetLocoBtn = getChild<LLButton>("btn_anim_setloco");
    mAnimPlayBtn = getChild<LLButton>("btn_anim_play");
    mAnimStopBtn = getChild<LLButton>("btn_anim_stop");
    mPasteEditor = getChild<LLLineEditor>("paste_uuid_editor");
    mPastePlayBtn = getChild<LLButton>("btn_paste_play");
    mPasteStopBtn = getChild<LLButton>("btn_paste_stop");
    mPasteSetLocoBtn = getChild<LLButton>("btn_paste_setloco");
    mLocoText = getChild<LLTextBox>("loco_text");
    mClearLocoBtn = getChild<LLButton>("btn_clear_loco");
    mAnimCopyBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimCopyUUID(); });
    mAnimSetLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimSetLoco(); });
    mAnimPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(true); });
    mAnimStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAnimPlayLocal(false); });
    mPastePlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPastePlayLocal(true); });
    mPasteStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPastePlayLocal(false); });
    mPasteSetLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPasteSetLoco(); });
    mClearLocoBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearLoco(); });
    mAnimList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onAnimRightClick(ctrl, x, y); });
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
    // embedded shared params panel: scene files read its selected preset and
    // apply presets through it on load
    mCineCamPanel = findChild<ALPanelCineCamParams>("cinecam_params_embedded");

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
    mRecIndicator = getChild<LLTextBox>("rec_indicator");

    return true;
}

void LLFloaterDirector::onOpen(const LLSD& key)
{
    // pick up scenes saved by an earlier session without losing the current
    // combo selection
    if (mSceneCombo)
    {
        refreshSceneList(mSceneCombo->getSelectedItemLabel());
    }

    // return to the tab left showing last time (clamped: tab set can shrink)
    if (mTabContainer)
    {
        const S32 last = llclamp(gSavedSettings.getS32("DirectorLastTab"),
                                 0, mTabContainer->getTabCount() - 1);
        mTabContainer->selectTab(last);
    }

    // a reopened console never resurfaces the legend
    if (mLegendPanel)
    {
        mLegendPanel->setVisible(false);
    }
    LLFloater::onOpen(key);
}

// ---------------------------------------------------------------------------
// tabs + legend
// ---------------------------------------------------------------------------
void LLFloaterDirector::onTabChanged()
{
    if (mTabContainer)
    {
        gSavedSettings.setS32("DirectorLastTab", mTabContainer->getCurrentPanelIndex());
    }
}

void LLFloaterDirector::onToggleLegend()
{
    if (mLegendPanel)
    {
        mLegendPanel->setVisible(!mLegendPanel->getVisible());
    }
}

void LLFloaterDirector::draw()
{
    refreshCastList();
    refreshTransport();
    refreshMoveTab();
    refreshPathTab();
    refreshAnimateTab();
    refreshCameraTab();
    refreshTakesTab();
    refreshStatusStrip();
    LLFloater::draw();
}

bool LLFloaterDirector::handleKeyHere(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE && mask == MASK_NONE)
    {
        // Esc = CUT, but ONLY while the transport is live; an idle floater
        // keeps the stock Esc behavior (defocus/close handling upstream)
        LLDirectorCast& cast = LLDirectorCast::instance();
        if (cast.isRunning() || cast.isCountingDown())
        {
            cast.cut();
            return true;
        }
    }
    return LLFloater::handleKeyHere(key, mask);
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

    // scene controls
    const bool naming = mSceneNameEditor->getVisible();
    const std::string scene_sel = mSceneCombo->getSelectedItemLabel();
    setToolTipIfChanged(mSceneSaveBtn, naming
        ? std::string("Write the scene file (Enter in the name box saves too; an empty name cancels)")
        : std::string("Save the whole setup as a scene: cast, marks, loco anims, subjects, arming, camera and move parameters. Opens an inline name box; re-saving the selected scene keeps its name"));
    setToolTipIfChanged(mSceneCombo, mSceneCombo->getItemCount() > 0
        ? std::string("Load a scene: restores cast, marks, loco anims, subjects and parameters. Nothing is moved; press Reset to marks to place actors")
        : std::string("No saved scenes yet. Save one first"));
    mSceneDeleteBtn->setEnabled(!scene_sel.empty() && !naming);
    setToolTipIfChanged(mSceneDeleteBtn, scene_sel.empty()
        ? std::string("Select a scene first")
        : naming ? std::string("Finish naming first")
                 : std::string("Delete the selected scene file (asks to confirm)"));
}

// ---------------------------------------------------------------------------
// scene files: one LLSD XML per scene in user_settings/director_scenes/,
// same folder/escape/list idiom as the cinematic camera presets
// ---------------------------------------------------------------------------
//static
std::string LLFloaterDirector::scenesDir()
{
    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, SCENE_SUBDIR);
    if (!gDirUtilp->fileExists(dir))
    {
        LLFile::mkdir(dir);
    }
    return dir;
}

//static
std::string LLFloaterDirector::scenePath(const std::string& name)
{
    // same reversible sanitization the graphics/cinematic presets use
    return gDirUtilp->add(scenesDir(), LLURI::escape(name) + ".xml");
}

//static
const std::vector<std::string>& LLFloaterDirector::sceneSettingsList()
{
    // CinematicCamMode is NOT here: it is stored separately so a scene's
    // mode can be applied AFTER its named preset (the preset apply path
    // writes the mode too, and the scene's own mode must win)
    static const std::vector<std::string> settings = {
        // transport arming + countdown
        "DirectorArmMoves",
        "DirectorArmCamera",
        "DirectorArmRecorderPlay",
        "DirectorArmRecorderCapture",
        "DirectorActionDelay",
        // Flycam Orbit rig
        "FlycamOrbitEnabled",
        "FlycamOrbitLevel",
        "FlycamOrbitMinRadius",
        "FlycamOrbitMaxRadius",
        "FlycamOrbitOffsetUp",
        "FlycamOrbitOffsetLeft",
        "FlycamOrbitZoom",
        "FlycamOrbitSmoothing",
        // Move-tab (Actor Mover) parameters
        "ActorMoverSpeed",
        "ActorMoverDistance",
        "ActorMoverHeading",
        "ActorMoverWalkNominal",
        "ActorMoverEndMode",
        "ActorMoverSync",
        "ActorMoverUseCustomAnim",
        "ActorMoverCustomAnim",
    };
    return settings;
}

void LLFloaterDirector::refreshSceneList(const std::string& select_name)
{
    if (!mSceneCombo)
    {
        return;
    }
    mSceneCombo->clearRows();

    std::vector<std::string> names;
    {
        LLDirIterator dir_iter(scenesDir(), "*.xml");
        std::string file;
        while (dir_iter.next(file))
        {
            names.emplace_back(LLURI::unescape(
                gDirUtilp->getBaseFileName(file, /*strip_exten=*/true)));
        }
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        mSceneCombo->add(name);
    }

    if (!select_name.empty() && mSceneCombo->setSelectedByValue(select_name, true))
    {
        return;
    }
    mSceneCombo->setLabel(LLStringExplicit("Scenes"));
}

void LLFloaterDirector::onSceneSelected()
{
    const std::string name = mSceneCombo->getSelectedItemLabel();
    if (!name.empty())
    {
        loadScene(name);
    }
}

void LLFloaterDirector::onClickSceneSave()
{
    if (!mSceneNameEditor->getVisible())
    {
        // reveal the inline name box in the combo's spot (usability rule 2:
        // no naming popups), preloaded so "tweak and re-save" is Save-Enter
        mSceneNameEditor->setText(mSceneCombo->getSelectedItemLabel());
        mSceneCombo->setVisible(false);
        mSceneNameEditor->setVisible(true);
        mSceneNameEditor->setFocus(true);
        mSceneNameEditor->selectAll();
    }
    else
    {
        commitSceneName();
    }
}

void LLFloaterDirector::commitSceneName()
{
    std::string name = mSceneNameEditor->getText();
    LLStringUtil::trim(name);
    mSceneNameEditor->setVisible(false);
    mSceneCombo->setVisible(true);
    if (name.empty())
    {
        return;     // empty name = cancel, nothing written
    }
    saveScene(name);
    refreshSceneList(name);
}

void LLFloaterDirector::onClickSceneDelete()
{
    const std::string name = mSceneCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLSD args;
    args["NAME"] = name;
    LLHandle<LLFloater> handle = getHandle();
    LLNotificationsUtil::add("DirectorConfirmDeleteScene", args, LLSD(),
        [handle, name](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
            {
                return;
            }
            const std::string path = scenePath(name);
            if (LLFile::remove(path) != 0)
            {
                LL_WARNS("Director") << "Cannot delete scene file " << path << LL_ENDL;
            }
            if (LLFloaterDirector* self = static_cast<LLFloaterDirector*>(handle.get()))
            {
                self->refreshSceneList();
            }
        });
}

void LLFloaterDirector::saveScene(const std::string& name)
{
    // engine data (cast, marks, loco anims, subjects) ...
    LLSD scene = LLDirectorCast::instance().sceneData();
    scene["version"] = SCENE_VERSION;

    // ... plus everything settings-backed
    LLSD settings = LLSD::emptyMap();
    for (const std::string& setting : sceneSettingsList())
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
        {
            settings[setting] = ctrl->getValue();
        }
    }
    scene["settings"] = settings;

    // CineCam: active mode, and the ACTIVE named preset when one is selected
    // in the shared params panel (stored by name; applied via the panel on load)
    scene["cinecam_mode"] = gSavedSettings.getS32("CinematicCamMode");
    if (mCineCamPanel)
    {
        const std::string preset = mCineCamPanel->getSelectedPresetName();
        if (!preset.empty())
        {
            scene["cinecam_preset"] = preset;
        }
    }

    const std::string path = scenePath(name);
    llofstream out(path.c_str());
    if (!out.is_open())
    {
        LL_WARNS("Director") << "Cannot write scene file " << path << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(scene, out);
    out.close();
    LL_INFOS("Director") << "Saved scene '" << name << "'" << LL_ENDL;
}

void LLFloaterDirector::loadScene(const std::string& name)
{
    const std::string path = scenePath(name);
    llifstream in(path.c_str());
    if (!in.is_open())
    {
        LL_WARNS("Director") << "Cannot open scene file " << path << LL_ENDL;
        return;
    }
    LLSD scene;
    LLSDSerialize::fromXML(scene, in);
    in.close();
    if (!scene.isMap())
    {
        LL_WARNS("Director") << "Malformed scene file " << path << LL_ENDL;
        return;
    }

    // a loaded scene starts from a clean transport
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (cast.isRunning() || cast.isCountingDown())
    {
        cast.cut();
    }

    // cast, marks, loco anims, subjects -- data only, nothing moves; members
    // not in world stay in the cast grayed "(away)"
    cast.applySceneData(scene);

    // only apply keys this floater owns: a scene file is data, not commands
    if (scene["settings"].isMap())
    {
        for (const std::string& setting : sceneSettingsList())
        {
            if (scene["settings"].has(setting))
            {
                if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
                {
                    ctrl->setValue(scene["settings"][setting]);
                }
            }
        }
    }

    // named preset first (its apply path writes CinematicCam* including the
    // mode), THEN the scene's explicit mode wins
    if (scene["cinecam_preset"].isString() && mCineCamPanel)
    {
        const std::string preset = scene["cinecam_preset"].asString();
        if (!mCineCamPanel->applyPresetByName(preset))
        {
            LL_WARNS("Director") << "Scene '" << name
                                 << "' references missing cinematic preset '"
                                 << preset << "'" << LL_ENDL;
        }
    }
    if (scene["cinecam_mode"].isInteger())
    {
        gSavedSettings.setS32("CinematicCamMode", scene["cinecam_mode"].asInteger());
    }
    LL_INFOS("Director") << "Loaded scene '" << name << "'" << LL_ENDL;
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

    // focus needs a selection that is actually in world (reason-tooltip otherwise)
    const LLUUID focus_id = firstSelectedCastId();
    const bool   focus_in_world = focus_id.notNull() && cast.resolve(focus_id) != nullptr;
    mFocusBtn->setEnabled(focus_in_world);
    setToolTipIfChanged(mFocusBtn, focus_id.isNull()
        ? std::string("Select a cast member first")
        : focus_in_world ? "Frame " + castMemberName(focus_id) + " in the camera"
                         : castMemberName(focus_id) + " is not in world");
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

void LLFloaterDirector::onClickFocusActor()
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    // reuse the viewer's "Zoom In" focus path; a no-op (returns false) when the
    // actor isn't a reachable in-world object
    if (LLDirectorCast::instance().resolve(id))
    {
        handle_zoom_to_object(id);
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
}

// ---------------------------------------------------------------------------
// Path tab: the shared waypoint editor, targeting the cast selection
// ---------------------------------------------------------------------------
void LLFloaterDirector::refreshPathTab()
{
    // the waypoint editor now owns its own tab (always visible); keep it pointed
    // at the console's cast selection every draw (cheap; the panel diffs its
    // own state internally)
    if (mPathPanel)
    {
        mPathPanel->setTargetActor(firstSelectedCastId());
    }
}

// ---------------------------------------------------------------------------
// Animate tab: live signaled-animation list for the selected cast member
// ---------------------------------------------------------------------------
LLUUID LLFloaterDirector::selectedAnimId() const
{
    LLScrollListItem* item = mAnimList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

LLUUID LLFloaterDirector::pasteAnimId() const
{
    std::string text = mPasteEditor->getText();
    LLStringUtil::trim(text);
    if (!LLUUID::validate(text))
    {
        return LLUUID::null;
    }
    return LLUUID(text);    // an all-zero uuid also comes back null-equivalent
}

void LLFloaterDirector::onAnimRightClick(LLUICtrl* ctrl, S32 x, S32 y)
{
    LLScrollListItem* item = mAnimList->hitItem(x, y);
    LLContextMenu* menu = mAnimMenuHandle.get();
    if (item && menu)
    {
        if (!item->getSelected())
        {
            const S32 index = mAnimList->getItemIndex(item);
            if (index >= 0)
            {
                mAnimList->selectNthItem(index);
            }
        }
        menu->buildDrawLabels();
        menu->updateParent(LLMenuGL::sMenuContainer);
        menu->show(x, y);
        LLMenuGL::showPopup(ctrl, menu, x, y);
    }
}

void LLFloaterDirector::onAnimCopyUUID()
{
    const LLUUID anim = selectedAnimId();
    if (anim.isNull())
    {
        return;
    }
    // same LLClipboard idiom as the Animation Explorer's Copy UUID
    LLWString idwstr = utf8string_to_wstring(anim.asString());
    LLClipboard::instance().copyToClipboard(idwstr, 0, narrow(idwstr.size()));
}

void LLFloaterDirector::onAnimSetLoco()
{
    const LLUUID anim = selectedAnimId();
    LLDirectorCast::CastMember* m =
        LLDirectorCast::instance().getMember(firstSelectedCastId());
    if (anim.notNull() && m)
    {
        // the loco line under the paste row is the visible confirmation
        m->mLocoAnim = anim;
    }
}

void LLFloaterDirector::onAnimPlayLocal(bool play)
{
    const LLUUID anim = selectedAnimId();
    LLVOAvatar* av = LLDirectorCast::instance().resolve(firstSelectedCastId());
    if (anim.isNull() || !av)
    {
        return;
    }
    // client-side only: nothing is sent to the sim
    play ? (void)av->startMotion(anim) : (void)av->stopMotion(anim);
}

void LLFloaterDirector::onPastePlayLocal(bool play)
{
    const LLUUID anim = pasteAnimId();
    LLVOAvatar* av = LLDirectorCast::instance().resolve(firstSelectedCastId());
    if (anim.isNull() || !av)
    {
        return;
    }
    play ? (void)av->startMotion(anim) : (void)av->stopMotion(anim);
}

void LLFloaterDirector::onPasteSetLoco()
{
    const LLUUID anim = pasteAnimId();
    LLDirectorCast::CastMember* m =
        LLDirectorCast::instance().getMember(firstSelectedCastId());
    if (anim.notNull() && m)
    {
        m->mLocoAnim = anim;
    }
}

void LLFloaterDirector::onClickClearLoco()
{
    if (LLDirectorCast::CastMember* m =
            LLDirectorCast::instance().getMember(firstSelectedCastId()))
    {
        m->mLocoAnim.setNull();
    }
}

void LLFloaterDirector::refreshAnimateTab()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID sel = firstSelectedCastId();
    LLVOAvatar* av = sel.notNull() ? cast.resolve(sel) : nullptr;
    const std::string member_name = sel.notNull() ? castMemberName(sel)
                                                  : std::string();

    // header: who the tab is showing
    mAnimateHeader->setText(sel.isNull()
        ? std::string("No cast member selected")
        : av ? member_name
             : member_name + " (away)");

    // snapshot the signaled set (std::map iteration = stable id order)
    std::vector<std::pair<LLUUID, S32>> snap;
    if (av)
    {
        snap.assign(av->mSignaledAnimations.begin(), av->mSignaledAnimations.end());
    }

    // membership change -> rebuild rows (rare); otherwise only re-set cells
    if (sel != mAnimAvatarId || snap != mAnimSnapshot)
    {
        const LLUUID prev_sel = selectedAnimId();
        mAnimAvatarId = sel;
        mAnimSnapshot = snap;
        mAnimList->deleteAllItems();
        mAnimRowStates.clear();
        mAnimRowStates.resize(snap.size());
        for (const auto& entry : snap)
        {
            const LLUUID& anim_id = entry.first;
            LLSD row;
            row["value"] = anim_id;
            row["columns"][0]["column"] = "anim_id";
            row["columns"][0]["value"] = anim_id.asString();
            row["columns"][0]["font"] = "Monospace";
            row["columns"][1]["column"] = "prio";
            row["columns"][1]["value"] = "?";
            row["columns"][2]["column"] = "playing";
            row["columns"][2]["value"] = "";
            mAnimList->addElement(row, ADD_BOTTOM);
        }
        if (prev_sel.notNull())
        {
            mAnimList->selectByID(prev_sel);
        }
    }

    // per-row priority / playing marker, diffed (draw()-rate friendly)
    if (av)
    {
        const S32 prio_col = mAnimList->getColumn("prio")->mIndex;
        const S32 playing_col = mAnimList->getColumn("playing")->mIndex;
        std::vector<LLScrollListItem*> items = mAnimList->getAllData();
        for (size_t i = 0; i < items.size() && i < mAnimRowStates.size(); ++i)
        {
            LLScrollListItem* item = items[i];
            AnimRowState& state = mAnimRowStates[i];
            const LLUUID anim_id = item->getValue().asUUID();

            // priority when resolvable via findMotion (AnimationExplorer idiom)
            std::string prio = "?";
            if (auto* motion = dynamic_cast<LLKeyframeMotion*>(av->findMotion(anim_id)))
            {
                prio = llformat("%d", (S32)motion->getPriority());
            }
            const std::string playing =
                av->mPlayingAnimations.find(anim_id) != av->mPlayingAnimations.end()
                    ? GLYPH_PLAYING : "";

            if (state.mPrio != prio)
            {
                if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(prio_col)))
                {
                    cell->setText(prio);
                }
                state.mPrio = prio;
            }
            if (state.mPlaying != playing)
            {
                if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(playing_col)))
                {
                    cell->setText(playing);
                }
                state.mPlaying = playing;
            }
        }
    }

    // empty-state hint over the list
    std::string hint;
    if (sel.isNull())
    {
        hint = "Select a cast member to see the animations playing on them";
    }
    else if (!av)
    {
        hint = member_name + " is not in world right now";
    }
    else if (snap.empty())
    {
        hint = "No signaled animations on " + member_name;
    }
    mAnimHint->setVisible(!hint.empty());
    if (!hint.empty())
    {
        mAnimHint->setText(hint);
    }

    // row-action buttons (reason-tooltips when disabled, usability rule 1)
    const bool have_row = mAnimList->getFirstSelected() != nullptr;
    const std::string no_row_tip = "Select an animation row first";

    mAnimCopyBtn->setEnabled(have_row);
    setToolTipIfChanged(mAnimCopyBtn, have_row
        ? std::string("Copy the animation asset UUID to the clipboard")
        : no_row_tip);
    mAnimSetLocoBtn->setEnabled(have_row && sel.notNull());
    setToolTipIfChanged(mAnimSetLocoBtn, !have_row ? no_row_tip
        : sel.isNull() ? std::string("Select a cast member first")
        : "Use this animation as " + member_name
          + "'s walk while the mover drives them (overrides the shared custom anim)");
    const std::string not_in_world_tip = sel.isNull()
        ? std::string("Select a cast member first")
        : member_name + " is not in world";
    mAnimPlayBtn->setEnabled(have_row && av);
    setToolTipIfChanged(mAnimPlayBtn, !have_row ? no_row_tip
        : !av ? not_in_world_tip
        : "Play this animation on " + member_name + " now (client-side only)");
    mAnimStopBtn->setEnabled(have_row && av);
    setToolTipIfChanged(mAnimStopBtn, !have_row ? no_row_tip
        : !av ? not_in_world_tip
        : "Stop this animation on " + member_name + " (client-side only)");

    // paste row
    const LLUUID pasted = pasteAnimId();
    std::string paste_text = mPasteEditor->getText();
    LLStringUtil::trim(paste_text);
    const std::string bad_uuid_tip = paste_text.empty()
        ? std::string("Enter an animation asset UUID first")
        : std::string("That is not a valid UUID");
    mPastePlayBtn->setEnabled(pasted.notNull() && av);
    setToolTipIfChanged(mPastePlayBtn, pasted.isNull() ? bad_uuid_tip
        : !av ? not_in_world_tip
        : "Play the pasted animation on " + member_name + " now (client-side only)");
    mPasteStopBtn->setEnabled(pasted.notNull() && av);
    setToolTipIfChanged(mPasteStopBtn, pasted.isNull() ? bad_uuid_tip
        : !av ? not_in_world_tip
        : "Stop the pasted animation on " + member_name + " (client-side only)");
    mPasteSetLocoBtn->setEnabled(pasted.notNull() && sel.notNull());
    setToolTipIfChanged(mPasteSetLocoBtn, pasted.isNull() ? bad_uuid_tip
        : sel.isNull() ? std::string("Select a cast member first")
        : "Use the pasted animation as " + member_name + "'s walk while the mover drives them");

    // loco-anim affordance: current override + Clear
    const LLDirectorCast::CastMember* m = cast.getMember(sel);
    std::string loco = "Loco anim: \xE2\x80\x94";
    if (m)
    {
        loco = m->mLocoAnim.notNull() ? "Loco anim: " + m->mLocoAnim.asString()
                                      : std::string("Loco anim: default walk");
    }
    if (mLocoText->getText() != loco)
    {
        mLocoText->setText(loco);
    }
    const bool have_loco = m && m->mLocoAnim.notNull();
    mClearLocoBtn->setEnabled(have_loco);
    setToolTipIfChanged(mClearLocoBtn, !m
        ? std::string("Select a cast member first")
        : have_loco ? member_name + " returns to the default walk (or the shared custom anim when enabled)"
                    : std::string("No loco anim override set"));
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

    mStatusStrip->setText(llformat("%d moving \xC2\xB7 %s \xC2\xB7 Orbit %s",
                                   moving, cam.c_str(),
                                   orbit_enabled ? "on" : "off"));

    // REC indicator: red danger dot while capturing, muted circle when idle
    // (diffed so the color/text are only re-set on a state change)
    const bool rec = LLFlycamRecorder::instance().getState() == LLFlycamRecorder::STATE_RECORDING;
    const S32 rec_state = rec ? 1 : 0;
    if (mRecState != rec_state)
    {
        mRecState = rec_state;
        const LLColor4 rec_color = rec ? LLColor4(0.85f, 0.25f, 0.25f, 1.f)
                                       : LLColor4(0.5f, 0.5f, 0.5f, 1.f);
        mRecIndicator->setText(rec ? std::string("REC \xE2\x97\x8F")
                                   : std::string("REC \xE2\x97\x8B"));
        mRecIndicator->setColor(rec_color);
        mRecIndicator->setReadOnlyColor(rec_color);
    }
}
