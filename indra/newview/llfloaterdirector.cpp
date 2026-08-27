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
#include "alscrollfocus.h"

#include "indra_constants.h"        // KEY_ESCAPE / MASK_NONE, MASK_ALT

#include "alfloateractorgaze.h"
#include "alpanelactormover.h"      // embedded shared Actor Mover transport
#include "alpanelanimpreview.h"     // embedded shared preview pane + own-avatar controls
#include "alpanelcinecamparams.h"   // embedded shared panel (scene preset hooks)
#include "alpanelpatheditor.h"      // embedded shared Actor Pathing editor
#include "alprojectorshaftpresets.h"
#include "alcinelightrigmanager.h"
#include "aldirectorswitcher.h"
#include "llactormover.h"
#include "llagent.h"                   // stable self id while avatar rebuilds
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llcontrolavatar.h"        // animesh control-avatar resolution (Add Animesh to Cast)
#include "lldir.h"                  // gDirUtilp (scene files)
#include "lldirectorcast.h"
#include "lldiriterator.h"          // scene file listing
#include "llcinematiccamera.h"
#include "llfile.h"                 // LLFile::mkdir/remove, ll*fstream
#include "llfloaterreg.h"
#include "llflycamrecorder.h"
#include "llghostavatar.h"
#include "llkeyframemotion.h"       // priority readout (signaled-anim list)
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"    // scene delete confirm
#include "llpresentationtime.h"
#include "llprismlens.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"          // scene LLSD XML files
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"             // group start-delay spinner (Groups section)
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "lluri.h"                  // LLURI::escape scene filenames
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewermenu.h"           // gMenuHolder, LLViewerMenuHolderGL
#include "llviewerobject.h"         // getRootEdit/isAnimatedObject/getControlAvatar
#include "llviewerobjectlist.h"     // gObjectList
#include "llmotion.h"               // LLMotion::setPriorityOverride (play-local priority)
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
constexpr char TAB_ICON_GHOSTS[]  = "Command_Appearance_Icon";
constexpr char TAB_ICON_PROPS[]   = "Command_Build_Icon";
constexpr char TAB_ICON_ANIMATE[] = "Command_Poser_Icon";
constexpr char TAB_ICON_CAMERA[]  = "Command_View_Icon";
constexpr char TAB_ICON_TAKES[]   = "Command_Snapshot_Icon";
constexpr char TAB_ICON_SHAFTS[]  = "Command_PersonalLighting_Icon";
constexpr char TAB_ICON_LIGHTS[]  = "Command_Lightbox_Icon";
constexpr char TAB_ICON_TEMPORAL[] = "Command_Environments_Icon"; // day-cycle/time metaphor (no clock asset ships)
constexpr char TAB_ICON_WEATHER[]  = "Command_Water_Icon"; // rain/precipitation metaphor (distinct from Time's sky icon)

// scene files live beside the cinematic presets, same idiom
constexpr char SCENE_SUBDIR[]  = "director_scenes";
constexpr S32  SCENE_VERSION   = 4;

// the assign combo's explicit "ungroup" row: discoverable equivalent of
// committing an empty name (which still works)
constexpr char GROUP_NONE_LABEL[] = "(none)";

enum SubjectMark : S32
{
    SUBJECT_A,
    SUBJECT_B,
    SUBJECT_C,
    SUBJECT_D,
};

LLUUID subject_mark_id(const LLDirectorCast& cast, S32 subject)
{
    switch (subject)
    {
        case ALDirectorSwitcher::SUBJECT_A: return cast.getSubjectA();
        case ALDirectorSwitcher::SUBJECT_B: return cast.getSubjectB();
        case ALDirectorSwitcher::SUBJECT_C: return cast.getSubjectC();
        case ALDirectorSwitcher::SUBJECT_D: return cast.getSubjectD();
        default: return LLUUID::null;
    }
}

void clear_subject(LLDirectorCast& cast, S32 subject)
{
    switch (subject)
    {
        case SUBJECT_A: cast.setSubjectA(LLUUID::null); break;
        case SUBJECT_B: cast.setSubjectB(LLUUID::null); break;
        case SUBJECT_C: cast.setSubjectC(LLUUID::null); break;
        case SUBJECT_D: cast.setSubjectD(LLUUID::null); break;
        default: break;
    }
}

void assign_subject(LLDirectorCast& cast, S32 subject, const LLUUID& id)
{
    if (subject < SUBJECT_A || subject > SUBJECT_D)
    {
        return;
    }
    // One cast member carries at most one subject mark. With C/D empty this
    // produces exactly the existing A/B reassignment behavior.
    if (cast.getSubjectA() == id) cast.setSubjectA(LLUUID::null);
    if (cast.getSubjectB() == id) cast.setSubjectB(LLUUID::null);
    if (cast.getSubjectC() == id) cast.setSubjectC(LLUUID::null);
    if (cast.getSubjectD() == id) cast.setSubjectD(LLUUID::null);
    switch (subject)
    {
        case SUBJECT_A: cast.setSubjectA(id); break;
        case SUBJECT_B: cast.setSubjectB(id); break;
        case SUBJECT_C: cast.setSubjectC(id); break;
        case SUBJECT_D: cast.setSubjectD(id); break;
        default: break;
    }
}

// Stable per-group tint for cast rows: hash the name to a hue, keep it a
// readable pastel (low saturation, full value) so the text stays legible on
// the dark list while two groups a row apart still read as different colors.
// Same spirit as LLActorMover::actorPathColor, keyed by name instead of id.
LLColor4 group_tint(const std::string& name)
{
    U32 h = 2166136261u;            // FNV-1a over the name bytes
    for (unsigned char c : name)
    {
        h = (h ^ c) * 16777619u;
    }
    const F32 hue = (h % 360u) / 360.f;
    LLColor4 col;
    col.setHSL(hue, 0.55f, 0.78f);  // pastel: light, moderately saturated
    col.mV[VW] = 1.f;
    return col;
}
// playing-marker glyph for the Animate tab's list
constexpr char GLYPH_PLAYING[] = "\xE2\x96\xB6";    // BLACK RIGHT-POINTING TRIANGLE

// CinematicCamMode value -> display name (matches the mode combo labels)
const char* cinecam_mode_name(S32 mode)
{
    return LLCinematicCamera::modeName(mode);
}

} // anonymous namespace

LLFloaterDirector::LLFloaterDirector(const LLSD& key)
:   LLFloater(key)
{
    mCommitCallbackRegistrar.add(
        "Director.ResetControlDefault",
        [](LLUICtrl*, const LLSD& data)
        {
            if (LLControlVariable* control =
                    gSavedSettings.getControl(data.asString()))
            {
                control->resetToDefault(true);
            }
        });
}

LLFloaterDirector::~LLFloaterDirector()
{
    // the embedded ALPanelAnimPreview owns and releases its own dummy

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

void LLFloaterDirector::onClose(bool app_quitting)
{
    // Closing can merely hide this reusable floater, so its child panel may
    // remain alive. Explicitly release a live bullet-time cut here as well.
    ALDirectorSwitcher::instance().cancelEaseWorldTime();
}

bool LLFloaterDirector::postBuild()
{
    getChild<LLComboBox>("director_ps_preset")->setCommitCallback(
        [](LLUICtrl* control, const LLSD&)
        {
            ALProjectorShaftPresets::apply(control->getValue().asInteger());
        });

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
        { "ghosts_tab",  TAB_ICON_GHOSTS },
        { "actor_style_tab", TAB_ICON_GHOSTS },
        { "props_tab",   TAB_ICON_PROPS },
        { "animate_tab", TAB_ICON_ANIMATE },
        { "camera_tab",  TAB_ICON_CAMERA },
        { "takes_tab",   TAB_ICON_TAKES },
        { "projector_volumetrics_tab", TAB_ICON_SHAFTS },
        { "cine_light_rig_tab", TAB_ICON_LIGHTS },
        { "weather_tab", TAB_ICON_WEATHER },
        { "temporal_tab", TAB_ICON_TEMPORAL },
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
    getChild<LLButton>("btn_add_animesh")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickAddAnimesh(); });
    mRemoveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastRemove(); });
    mFocusBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickFocusActor(); });
    mCastList->setRightMouseDownCallback(
        [this](LLUICtrl* ctrl, S32 x, S32 y, MASK) { onCastRightClick(ctrl, x, y); });
    // double-click a cast row = Set Subject A (mirrors the right-click path)
    mCastList->setDoubleClickCallback([this]() { onCastSetSubject(SUBJECT_A); });
    {
        // same LLContextMenu idiom as the Animation Explorer's list menu
        LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
        registrar.add("Director.SetSubjectA", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_A); });
        registrar.add("Director.SetSubjectB", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_B); });
        registrar.add("Director.SetSubjectC", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_C); });
        registrar.add("Director.SetSubjectD", [this](LLUICtrl*, const LLSD&) { onCastSetSubject(SUBJECT_D); });
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

    // ---- groups ----
    // assign combo (cast column): tags every selected member; free-typed names
    // create groups, picking an existing name reuses it, an empty name ungroups.
    // The embedded editor commits on Enter / item pick only (never focus-lost),
    // so a half-typed name can never tag a freshly clicked row by accident.
    mGroupAssignCombo = getChild<LLComboBox>("group_assign_combo");
    mGroupAssignCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitGroupAssign(); });
    // group transport (Move tab): pick a group, Start/Stop every member in it
    mGroupRunCombo = getChild<LLComboBox>("group_run_combo");
    mGroupStartBtn = getChild<LLButton>("btn_group_start");
    mGroupStopBtn = getChild<LLButton>("btn_group_stop");
    mGroupStartBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(true); });
    mGroupStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroup(false); });
    // Groups management section (Move tab): list every group with member count
    // + start delay; select a row to highlight its members in the cast list
    // and edit its delay / name. Dissolve only untags -- nobody leaves the cast.
    mGroupsList = getChild<LLScrollListCtrl>("groups_list");
    mGroupDelaySpinner = getChild<LLSpinCtrl>("group_delay_spinner");
    mGroupRenameEditor = getChild<LLLineEditor>("group_rename_editor");
    mGroupRenameBtn = getChild<LLButton>("btn_group_rename");
    mGroupDissolveBtn = getChild<LLButton>("btn_group_dissolve");
    mGroupsList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGroupsListSelect(); });
    mGroupDelaySpinner->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitGroupDelay(); });
    mGroupRenameEditor->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupRename(); });
    mGroupRenameBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupRename(); });
    mGroupDissolveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGroupDissolve(); });

    // ---- Move tab ----
    // the transport (scope, heading dial, params, Walk/Stop) is the shared
    // ALPanelActorMover the standalone Actor Mover floater also embeds; the
    // console feeds it the cast selection each draw (refreshMoveTab)
    mMoverPanel = findChild<ALPanelActorMover>("actor_mover_panel");

    // ---- Path tab ----
    // the waypoint editor lives in its own tab now; always visible, its target
    // actor re-pointed each draw to the console's cast selection
    mPathPanel = findChild<ALPanelPathEditor>("path_editor");

    // ---- Actor FX tab ----
    // This is a view over LLDirectorCast's per-actor model. It deliberately
    // does not share Ghost Studio's selected instance: actor styles attach to
    // stable cast ids, with a separate null-id record for You.
    mActorStyleTarget = getChild<LLComboBox>("actor_style_target");
    mActorStyleEnabled = getChild<LLCheckBoxCtrl>("actor_style_enabled");
    mActorStyleMode = getChild<LLComboBox>("actor_style_mode");
    mActorStyleLook = getChild<LLComboBox>("actor_style_look");
    mActorStyleUseActorHue = getChild<LLCheckBoxCtrl>("actor_style_use_actor_hue");
    mActorStyleHue = getChild<LLSliderCtrl>("actor_style_hue");
    mActorStyleAlpha = getChild<LLSliderCtrl>("actor_style_alpha");
    mActorStyleDissolveProgress = getChild<LLSliderCtrl>("actor_style_dissolve_progress");
    mActorStylePixel = getChild<LLSliderCtrl>("actor_style_pixel");
    mActorStyleShimmerSpeed = getChild<LLSliderCtrl>("actor_style_shimmer_speed");
    mActorStyleShimmerAmount = getChild<LLSliderCtrl>("actor_style_shimmer_amount");
    mActorStyleGlitch = getChild<LLSliderCtrl>("actor_style_glitch");
    mActorStyleDistortion = getChild<LLComboBox>("actor_style_distortion");
    mActorStyleDistortionAmount = getChild<LLSliderCtrl>("actor_style_distortion_amount");
    mActorStyleBrightness = getChild<LLSliderCtrl>("actor_style_brightness");
    mActorStyleEffectFps = getChild<LLSpinCtrl>("actor_style_effect_fps");
    mActorStyleReset = getChild<LLButton>("actor_style_reset");
    mActorStyleStatus = getChild<LLTextBox>("actor_style_status");

    mActorStyleTarget->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onActorStyleTargetChanged(); });
    for (LLUICtrl* control : {
            static_cast<LLUICtrl*>(mActorStyleEnabled),
            static_cast<LLUICtrl*>(mActorStyleMode),
            static_cast<LLUICtrl*>(mActorStyleLook),
            static_cast<LLUICtrl*>(mActorStyleUseActorHue),
            static_cast<LLUICtrl*>(mActorStyleHue),
            static_cast<LLUICtrl*>(mActorStyleAlpha),
            static_cast<LLUICtrl*>(mActorStyleDissolveProgress),
            static_cast<LLUICtrl*>(mActorStylePixel),
            static_cast<LLUICtrl*>(mActorStyleShimmerSpeed),
            static_cast<LLUICtrl*>(mActorStyleShimmerAmount),
            static_cast<LLUICtrl*>(mActorStyleGlitch),
            static_cast<LLUICtrl*>(mActorStyleDistortion),
            static_cast<LLUICtrl*>(mActorStyleDistortionAmount),
            static_cast<LLUICtrl*>(mActorStyleBrightness),
            static_cast<LLUICtrl*>(mActorStyleEffectFps) })
    {
        control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onActorStyleControlChanged(); });
    }
    mActorStyleReset->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onActorStyleReset(); });

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

    // embedded shared preview pane + own-avatar controls (the same
    // ALPanelAnimPreview the standalone Animation Explorer floater embeds). It
    // owns its dummy, mouse-drag-to-rotate, auto-loop, and the Stop / Stop-and-
    // Revoke / Blacklist / Capture-all controls; refreshAnimateTab feeds it the
    // selected/pasted anim + its source object each draw.
    mAnimPreviewPanel = findChild<ALPanelAnimPreview>("anim_preview_panel");

    // Move and Animate are fixed-height documents inside resizable tabs. Keep
    // keyboard-focused controls visible at the Director's saved/minimum size.
    LLScrollContainer* move_scroll = getChild<LLScrollContainer>("move_scroll");
    LLView* move_document = getChildView("move_scroll_content");
    ALScrollFocus::install(move_scroll, move_document, move_document);
    LLScrollContainer* animate_scroll = getChild<LLScrollContainer>("animate_scroll");
    LLView* animate_document = getChildView("animate_scroll_content");
    ALScrollFocus::install(animate_scroll, animate_document, animate_document);
    LLScrollContainer* actor_style_scroll =
        getChild<LLScrollContainer>("actor_style_scroll");
    LLView* actor_style_document = getChildView("actor_style_scroll_content");
    ALScrollFocus::install(actor_style_scroll, actor_style_document,
                           actor_style_document);
    LLScrollContainer* shafts_scroll =
        getChild<LLScrollContainer>("projector_volumetrics_scroll");
    LLView* shafts_document =
        getChildView("projector_volumetrics_scroll_content");
    ALScrollFocus::install(shafts_scroll, shafts_document, shafts_document);

    // ---- Camera tab ----
    mSubjectAText = getChild<LLTextBox>("subject_a_text");
    mSubjectBText = getChild<LLTextBox>("subject_b_text");
    mSubjectCText = getChild<LLTextBox>("subject_c_text");
    mSubjectDText = getChild<LLTextBox>("subject_d_text");
    mSetABtn = getChild<LLButton>("btn_set_a");
    mSetBBtn = getChild<LLButton>("btn_set_b");
    mSetCBtn = getChild<LLButton>("btn_set_c");
    mSetDBtn = getChild<LLButton>("btn_set_d");
    mClearABtn = getChild<LLButton>("btn_clear_a");
    mClearBBtn = getChild<LLButton>("btn_clear_b");
    mClearCBtn = getChild<LLButton>("btn_clear_c");
    mClearDBtn = getChild<LLButton>("btn_clear_d");
    mSetABtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_A); });
    mSetBBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_B); });
    mSetCBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_C); });
    mSetDBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickSetSubjectFromSelection(SUBJECT_D); });
    mClearABtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_A); });
    mClearBBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_B); });
    mClearCBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_C); });
    mClearDBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickClearSubject(SUBJECT_D); });
    getChild<LLButton>("btn_actor_gaze")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onOpenActorGaze(); });
    getChild<LLButton>("btn_virtual_cam")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onOpenVirtualCam(); });
    getChild<LLButton>("btn_ultimate_diopter")->setCommitCallback(
        [](LLUICtrl*, const LLSD&) { LLFloaterReg::showInstance("ultimate_diopter"); });
    // embedded shared params panel: scene files read its selected preset and
    // apply presets through it on load
    mCineCamPanel = findChild<ALPanelCineCamParams>("cinecam_params_embedded");

    // ---- Takes tab ----
    // the transport + controls are the shared ALPanelFlycamRecorder embedded in
    // the Takes tab (panel_flycam_recorder.xml); it owns its own wiring and
    // self-refreshes, so there is nothing to hook up here. Same panel the
    // standalone Flycam Recorder floater uses -> one singleton, no fork.

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
    refreshGroupControls();
    refreshTransport();
    refreshMoveTab();
    refreshPathTab();
    refreshActorStyleTab();
    refreshAnimateTab();
    refreshCameraTab();
    refreshStatusStrip();
    LLFloater::draw();
    // the embedded ALPanelAnimPreview blits + spins its own dummy in its draw()
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
        // Render-only real-avatar gaze (per-member selection lives in cast data)
        "DirectorLookAtCameraEnabled",
        "DirectorLookAtCameraMode",
        "DirectorLookAtCameraStrength",
        "DirectorLookAtCameraHeadEye",
        "DirectorLookAtCameraTorso",
        "DirectorLookAtCameraTorsoAmount",
        "DirectorLookAtCameraSmoothing",
        "DirectorLookAtCameraEaseTime",
        // Switcher program data. Armed/live slot stay transient: loading a
        // scene is data application and must never seize the live camera.
        "DirectorSwitcherAuto",
        "DirectorSwitcherBank",
        "DirectorSwitcherEaseCuts",
        "DirectorSwitcherEaseSec",
        "DirectorSwitcherIntervalSec",
        "DirectorSwitcherJitterSec",
        "DirectorSwitcherSeed",
        "DirectorSwitcherSequence",
        // Flycam Orbit rig
        "FlycamOrbitEnabled",
        "FlycamOrbitLevel",
        "FlycamOrbitMinRadius",
        "FlycamOrbitMaxRadius",
        "FlycamOrbitOffsetUp",
        "FlycamOrbitOffsetLeft",
        "FlycamOrbitZoom",
        "FlycamOrbitSmoothing",
        // Cinematic target format, lens, and non-destructive screen guide
        "CinematicAutoFrameEnabled",
        "CinematicAutoFrameFill",
        "CinematicAutoFrameComposeLine",
        "CinematicAutoFrameDistanceTrim",
        "CinematicCamFrameOffsetUp",
        "CinematicFrameAspectRatio",
        "CinematicFrameCustomRatio",
        "CinematicFrameFocalLengthMM",
        "CinematicFrameGuideEnabled",
        "CinematicFrameGuideOpacity",
        "CinematicFrameGuideStyle",
        "CinematicFrameLensEnabled",
        // Move-tab (Actor Mover) parameters
        "ActorMoverSpeed",
        "ActorMoverDistance",
        "ActorMoverHeading",
        "ActorMoverWalkNominal",
        "ActorMoverEndMode",
        "ActorMoverSync",
        "ActorMoverUseCustomAnim",
        "ActorMoverCustomAnim",
        // Temporal Capture (World Time Scale): requested mode / scale / drive
        // gates only -- NOT effective scale, paused state, or Freeze World, which
        // is runtime-only and independently owned (see the brief, scene-save note)
        "TemporalMode",
        "TemporalWorldScale",
        "TemporalOutputFPS",
        "TemporalDriveAnimation",
        "TemporalDriveObjects",
        "TemporalDriveTextureAnim",
        "TemporalDriveParticles",
        "TemporalDriveCamera",
        // Surface Lens appearance is creative scene intent. Adaptive controls,
        // aggregate budget, master enable and debug remain global preferences.
        "PrismLensZoom",
        // Client-only cinematic light rig. Anchor and FX phase are stored in
        // the structured light_rig block; every persisted control is included
        // here so a scene remains a complete settings snapshot.
        "CineLightRigEnabled",
        "CineLightRigScaleAware",
        "CineLightRigPower",
        "CineLightRigRadius",
        "CineLightRigMasterEV",
        "CineLightRigMasterTempMired",
        "CineLightRigOffsetZ",
        "CineLightRigHeadroomStops",
        "CineLightRigBounceEnabled",
        "CineLightRigBounceRatio",
        "CineLightRigTransitionSec",
        "CineLightRigDamping",
        "CineLightRigTrackMode",
        "CineLightRigCookieUUID",
        "CineLightRigSeed",
        "CineLightRigMirror",
        "CineLightRigOrbitYaw",
        "CineLightRigOrbitPitch",
        "CineLightRigFX",
        "CineLightRigShadowMode",
        "CineLightRigGizmo",
        "CineLightRigKeyYaw",
        "CineLightRigKeyPitch",
        "CineLightRigKeyProfile",
        "CineLightRigKeyEV",
        "CineLightRigKeyBeam",
        "CineLightRigKeyGobo",
        "CineLightRigKeyOn",
        "CineLightRigFillYaw",
        "CineLightRigFillPitch",
        "CineLightRigFillProfile",
        "CineLightRigFillEV",
        "CineLightRigFillBeam",
        "CineLightRigFillGobo",
        "CineLightRigFillOn",
        "CineLightRigRimYaw",
        "CineLightRigRimPitch",
        "CineLightRigRimProfile",
        "CineLightRigRimEV",
        "CineLightRigRimBeam",
        "CineLightRigRimGobo",
        "CineLightRigRimOn",
        "CineLightRigBgYaw",
        "CineLightRigBgPitch",
        "CineLightRigBgProfile",
        "CineLightRigBgEV",
        "CineLightRigBgBeam",
        "CineLightRigBgGobo",
        "CineLightRigBgOn",
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
    if (saveScene(name))
    {
        refreshSceneList(name);
    }
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

bool LLFloaterDirector::saveScene(const std::string& name)
{
    // engine data (cast, marks, loco anims, subjects) ...
    LLSD scene = LLDirectorCast::instance().sceneData();
    scene["version"] = SCENE_VERSION;
    const LLSD prism = LLPrismLens::sceneData();
    scene["prism_captures"] = prism["prism_captures"];
    scene["prism_displays"] = prism["prism_displays"];
    scene["prism_gates"] = prism["prism_gates"];
    scene["light_rig"] = ALCineLightRigManager::instance().sceneData();

    // ... plus everything settings-backed
    LLSD settings = LLSD::emptyMap();
    for (const std::string& setting : sceneSettingsList())
    {
        if (LLControlVariable* ctrl = gSavedSettings.getControl(setting))
        {
            settings[setting] = setting == "CineLightRigFX"
                ? LLSD(ALCineLightRigPersistence::legacyFXForStorage(
                    ctrl->getValue().asInteger()))
                : ctrl->getValue();
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
    // Serialize beside the destination, then replace it only after the stream
    // has flushed and closed cleanly. This keeps the previous scene intact on
    // disk-full, serialization, close, or replacement failure.
    const std::string temp_path = path + "." +
        LLUUID::generateNewID().asString() + ".tmp";
    const auto report_save_failure = [&name]()
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE", llformat(
                "Unable to save Director scene '%s'. The previous saved scene, if any, was left unchanged.",
                name.c_str())));
    };
    llofstream out(temp_path.c_str());
    if (!out.is_open())
    {
        LL_WARNS("Director") << "Cannot open temporary scene file "
                             << temp_path << LL_ENDL;
        LLFile::remove(temp_path, ENOENT);
        report_save_failure();
        return false;
    }

    const S32 serialized_count = LLSDSerialize::toPrettyXML(scene, out);
    out.flush();
    const bool write_succeeded = serialized_count > 0 && out.good();
    out.close();
    if (!write_succeeded || out.fail())
    {
        LL_WARNS("Director") << "Failed writing temporary scene file "
                             << temp_path << LL_ENDL;
        LLFile::remove(temp_path, ENOENT);
        report_save_failure();
        return false;
    }

    if (LLFile::rename(temp_path, path) != 0)
    {
        LL_WARNS("Director") << "Cannot replace scene file " << path
                             << " with completed temporary file" << LL_ENDL;
        LLFile::remove(temp_path, ENOENT);
        report_save_failure();
        return false;
    }

    LL_INFOS("Director") << "Saved scene '" << name << "'" << LL_ENDL;
    return true;
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
    const S32 parsed_count = LLSDSerialize::fromXML(scene, in);
    // EOF/failbit is a normal parser end condition for some stream readers;
    // badbit alone identifies a genuine underlying read failure.
    const bool stream_bad = in.bad();
    in.close();
    if (parsed_count == LLSDParser::PARSE_FAILURE || stream_bad || !scene.isMap())
    {
        LL_WARNS("Director") << "Malformed scene file " << path << LL_ENDL;
        LLNotificationsUtil::add(
            "SystemMessageTip",
            LLSD().with("MESSAGE", "Scene was not loaded: the scene file is malformed or incomplete."));
        return;
    }

    const bool has_version = scene.has("version");
    const bool malformed_version = has_version && !scene["version"].isInteger();
    const S32 version = has_version && !malformed_version
        ? scene["version"].asInteger() : 1;
    if (malformed_version)
    {
        LL_WARNS("Director") << "Scene '" << name
                             << "' has a non-integer version; preserving live "
                                "Prism configuration and loading only tolerant "
                                "legacy scene fields" << LL_ENDL;
    }
    else if (version == 3 || version == 4)
    {
        // v3 and v4 share the same Prism scene schema; v4 only raises the capture
        // count ceiling (an older viewer treats v4 as a future version and keeps
        // its live Prism config instead of hard-rejecting the whole scene).
        // Validate and atomically replace Prism before mutating transport, cast,
        // settings, or CineCam state. A malformed payload leaves the live scene
        // and retained Prism outputs wholly untouched.
        std::string prism_reason;
        if (!LLPrismLens::applySceneData(scene, &prism_reason))
        {
            LL_WARNS("Director") << "Rejected Prism data in scene '" << name
                                 << "': " << prism_reason << LL_ENDL;
            LLSD args;
            args["MESSAGE"] = "Scene was not loaded: " + prism_reason;
            LLNotificationsUtil::add("SystemMessageTip", args);
            return;
        }
    }
    else if (version != 1)
    {
        // No version-2 schema was ever shipped. Preserve the current registry
        // for unsupported/future files while allowing their legacy scene data
        // to load through its existing tolerant paths.
        LL_WARNS("Director") << "Scene '" << name << "' uses unsupported version "
                             << version << "; preserving live Prism configuration"
                             << LL_ENDL;
    }

    // A loaded scene starts from a clean transport and a released switcher.
    // Armed is deliberately not scene data: loading must not seize a camera.
    gSavedSettings.setBOOL("DirectorSwitcherArmed", false);
    ALDirectorSwitcher::instance().tick(
        LLPresentationTime::currentFrame().presentation_time);

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

    // Structured state carries the session-only anchor and the deterministic
    // FX phase, plus a denormalized base setup for self-contained scenes.
    ALCineLightRigManager::instance().applySceneData(scene["light_rig"]);

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
    if (LLGhostAvatar::isGhostId(id))
    {
        return "Entity clone " + id.asString().substr(0, 8);
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
            row["columns"][4]["column"] = "look";
            row["columns"][4]["value"] = "";
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
    const S32 look_col = mCastList->getColumn("look")->mIndex;

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
        const LLDirectorCast::CastMember* m = cast.getMember(id);

        std::string name = castMemberName(id);
        if (!in_world)
        {
            name += " (away)";
        }
        if (m && !m->mGroup.empty())
        {
            // the group tag rides the name cell as a middle-dot suffix -- the
            // narrow cast list has no room for a fifth column, and the diffed
            // CastRowState::mName below keys on the combined string anyway
            name += " \xC2\xB7 " + m->mGroup;
        }
        std::string ab;
        if (cast.getSubjectA() == id) ab = "A";
        else if (cast.getSubjectB() == id) ab = "B";
        else if (cast.getSubjectC() == id) ab = "C";
        else if (cast.getSubjectD() == id) ab = "D";
        const std::string mark = (m && m->mHasMark) ? ICON_MARK : "";
        const std::string look = cast.isLookAtCamera(id) ? "L" : "";

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
                // out-of-world members stay in the cast, grayed; grouped
                // members tint to their group's stable pastel hue so the
                // grouping reads at a glance (the diffed mName includes the
                // group suffix, so a retag re-applies the color)
                cell->setColor(!in_world ? LLColor4::grey
                             : (m && !m->mGroup.empty()) ? group_tint(m->mGroup)
                                                         : LLColor4::white);
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
        if (state.mLook != look)
        {
            if (auto* cell = dynamic_cast<LLScrollListText*>(item->getColumn(look_col)))
            {
                cell->setText(look);
            }
            state.mLook = look;
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

// Add every in-world-selected animesh object to the cast, resolving each linkset
// to its control avatar (the same id the right-click Director > Add to Cast uses).
// Non-animesh selections are skipped; add() is idempotent, so re-adds are no-ops.
void LLFloaterDirector::onClickAddAnimesh()
{
    LLObjectSelectionHandle sel = LLSelectMgr::getInstance()->getSelection();
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (LLObjectSelection::valid_iterator it = sel->valid_begin(), end = sel->valid_end();
         it != end; ++it)
    {
        LLViewerObject* obj = (*it)->getObject();
        if (!obj)
        {
            continue;
        }
        LLViewerObject* root = obj->getRootEdit();
        if (!root)
        {
            root = obj;
        }
        if (!root->isAnimatedObject())
        {
            continue;
        }
        if (LLControlAvatar* cav = root->getControlAvatar())
        {
            if (!cav->isDead() && cav->getID().notNull())
            {
                cast.add(cav->getID()); // idempotent
            }
        }
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

void LLFloaterDirector::onCastSetSubject(S32 subject)
{
    const LLUUID id = firstSelectedCastId();
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    assign_subject(cast, subject, id);
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
// groups: assign combo (cast column) + group transport (Move tab)
// ---------------------------------------------------------------------------
void LLFloaterDirector::onCommitGroupAssign()
{
    // typed name or picked row; whitespace trims away so " guards " and
    // "guards" are one group, and an empty commit ungroups. The "(none)" row
    // is the discoverable spelling of the empty commit.
    std::string name = mGroupAssignCombo->getSimple();
    LLStringUtil::trim(name);
    if (name == GROUP_NONE_LABEL)
    {
        name.clear();
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    for (const LLUUID& id : selectedCastIds())
    {
        cast.setGroup(id, name);    // multi-select tags a whole crowd at once
    }
    // force the next refreshGroupControls() to re-mirror the (possibly brand
    // new) name set + the selected member's tag into both combos
    mLastGroupNames.assign(1, std::string());   // impossible value ("" is never listed)
    mGroupShownFor.setNull();
}

void LLFloaterDirector::onClickGroup(bool start)
{
    const std::string name = mGroupRunCombo->getSelectedItemLabel();
    if (name.empty())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (start)
    {
        // delay-aware: the whole group shares one delay, so with one set the
        // members queue and step off together N seconds from now; without one
        // this is exactly the old per-member start loop (each start() captures
        // its own parameters, like pressing Walk on each member)
        cast.startMovesStaggered(cast.membersInGroup(name));
    }
    else
    {
        // stop() also disarms each member's queued staggered start
        LLActorMover& mover = LLActorMover::instance();
        for (const LLUUID& id : cast.membersInGroup(name))
        {
            mover.stop(id);
        }
    }
}

// ---------------------------------------------------------------------------
// Groups management section (Move tab)
// ---------------------------------------------------------------------------
std::string LLFloaterDirector::selectedManagedGroup() const
{
    LLScrollListItem* item = mGroupsList->getFirstSelected();
    return item ? item->getValue().asString() : std::string();
}

void LLFloaterDirector::onGroupsListSelect()
{
    const std::string name = selectedManagedGroup();
    if (name.empty())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    // highlight the group's members in the cast list (cheap: selectMultiple
    // walks the rows once), so "who is in this group" is one click. This also
    // makes the batch ops to the left (assign, marks, Move-tab Walk on the
    // selection) act on exactly this group.
    mCastList->selectMultiple(cast.membersInGroup(name));
    // load the delay spinner + prefill the rename editor with the current name
    mGroupDelayShownFor = name;
    mGroupDelaySpinner->setValue(cast.getGroupDelay(name));
    mGroupRenameEditor->setText(name);
}

void LLFloaterDirector::onCommitGroupDelay()
{
    const std::string name = selectedManagedGroup();
    if (!name.empty())
    {
        LLDirectorCast::instance().setGroupDelay(name, (F32)mGroupDelaySpinner->getValue().asReal());
    }
}

void LLFloaterDirector::onClickGroupRename()
{
    const std::string old_name = selectedManagedGroup();
    std::string new_name = mGroupRenameEditor->getText();
    LLStringUtil::trim(new_name);
    if (old_name.empty() || new_name.empty() || new_name == GROUP_NONE_LABEL)
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    if (cast.renameGroup(old_name, new_name))
    {
        // rebuild everything group-shaped next draw and follow the selection
        // onto the new name so the operator's context is not lost
        mLastGroupNames.assign(1, std::string());
        mGroupShownFor.setNull();
        mGroupsListSnapshot.clear();
        mGroupDelayShownFor.clear();
        refreshGroupControls();
        mGroupsList->setSelectedByValue(new_name, true);
    }
}

void LLFloaterDirector::onClickGroupDissolve()
{
    const std::string name = selectedManagedGroup();
    if (name.empty())
    {
        return;
    }
    // untag only: every member stays in the cast; the delay goes with the tag
    LLDirectorCast::instance().dissolveGroup(name);
    mLastGroupNames.assign(1, std::string());
    mGroupShownFor.setNull();
    mGroupsListSnapshot.clear();
    mGroupDelayShownFor.clear();
}

void LLFloaterDirector::refreshGroupControls()
{
    LLDirectorCast& cast = LLDirectorCast::instance();

    // rebuild both combos only when the distinct-name set changed (rare)
    const std::vector<std::string> names = cast.getGroupNames();
    if (names != mLastGroupNames)
    {
        mLastGroupNames = names;
        const std::string run_sel = mGroupRunCombo->getSelectedItemLabel();
        mGroupAssignCombo->clearRows();
        mGroupRunCombo->clearRows();
        // "(none)" first: the discoverable way to clear one member's tag
        // (commits as the empty name; typing nothing still works too)
        mGroupAssignCombo->add(GROUP_NONE_LABEL);
        for (const std::string& name : names)
        {
            mGroupAssignCombo->add(name);
            mGroupRunCombo->add(name);
        }
        // keep the run combo on the group the operator had picked, when it
        // still exists; otherwise fall back to the prompt label
        if (run_sel.empty() || !mGroupRunCombo->setSimple(LLStringExplicit(run_sel)))
        {
            mGroupRunCombo->setLabel(LLStringExplicit("Group"));
        }
    }

    // mirror the first selected member's tag into the assign box -- but never
    // over the operator's in-progress typing (focus covers the embedded editor)
    const LLUUID sel = firstSelectedCastId();
    if (sel != mGroupShownFor && !mGroupAssignCombo->hasFocus())
    {
        mGroupShownFor = sel;
        mGroupAssignCombo->setTextEntry(LLStringExplicit(cast.getGroup(sel)));
    }
    const bool have_sel = sel.notNull();
    mGroupAssignCombo->setEnabled(have_sel);
    setToolTipIfChanged(mGroupAssignCombo, have_sel
        ? std::string("Group tag for the selected member(s): type a new name or pick an existing one, Enter commits. Empty ungroups")
        : std::string("Select a cast member first"));

    // group transport: Start/Stop need a picked group that still has members
    const std::string run_sel = mGroupRunCombo->getSelectedItemLabel();
    const bool have_group = !run_sel.empty()
                         && !cast.membersInGroup(run_sel).empty();
    mGroupRunCombo->setEnabled(!names.empty());
    setToolTipIfChanged(mGroupRunCombo, names.empty()
        ? std::string("No groups yet: tag cast members via the Group box under the cast list")
        : std::string("The group Start/Stop act on"));
    mGroupStartBtn->setEnabled(have_group);
    mGroupStopBtn->setEnabled(have_group);
    const std::string need_group_tip = names.empty()
        ? std::string("No groups yet: tag cast members via the Group box under the cast list")
        : std::string("Pick a group first");
    const F32 run_delay = have_group ? cast.getGroupDelay(run_sel) : 0.f;
    setToolTipIfChanged(mGroupStartBtn, have_group
        ? (run_delay > 0.01f
            ? llformat("Start every member of '%s' after its %.1f s delay (staggered)", run_sel.c_str(), run_delay)
            : "Start a move for every member of '" + run_sel + "' (same as pressing Walk on each)")
        : need_group_tip);
    setToolTipIfChanged(mGroupStopBtn, have_group
        ? "Stop every member of '" + run_sel + "' (also disarms queued staggered starts)"
        : need_group_tip);

    // ---- Groups management section (Move tab) ----
    // rebuild the list only when its composed snapshot (name, member count,
    // delay) changed; keyed rows keep the selection across rebuilds
    std::vector<std::string> snapshot;
    snapshot.reserve(names.size());
    for (const std::string& name : names)
    {
        snapshot.push_back(llformat("%s|%d|%.1f", name.c_str(),
                                    (S32)cast.membersInGroup(name).size(),
                                    cast.getGroupDelay(name)));
    }
    if (snapshot != mGroupsListSnapshot)
    {
        mGroupsListSnapshot = snapshot;
        const std::string prev_sel = selectedManagedGroup();
        mGroupsList->deleteAllItems();
        for (const std::string& name : names)
        {
            const F32 d = cast.getGroupDelay(name);
            LLSD row;
            row["value"] = name;
            row["columns"][0]["column"] = "group";
            row["columns"][0]["value"] = name;
            row["columns"][1]["column"] = "members";
            row["columns"][1]["value"] = llformat("%d", (S32)cast.membersInGroup(name).size());
            row["columns"][2]["column"] = "delay";
            row["columns"][2]["value"] = d > 0.01f ? llformat("%.1f s", d)
                                                   : std::string("\xE2\x80\x94");   // em dash
            mGroupsList->addElement(row, ADD_BOTTOM);
        }
        if (!prev_sel.empty())
        {
            mGroupsList->setSelectedByValue(prev_sel, true);
        }
    }

    // spinner mirrors the selected group's delay -- but never over the
    // operator's in-progress edit (focus check, same rule as the assign combo)
    const std::string managed = selectedManagedGroup();
    const bool have_managed = !managed.empty();
    if (managed != mGroupDelayShownFor && !mGroupDelaySpinner->hasFocus())
    {
        mGroupDelayShownFor = managed;
        mGroupDelaySpinner->setValue(have_managed ? cast.getGroupDelay(managed) : 0.f);
    }
    mGroupDelaySpinner->setEnabled(have_managed);
    mGroupRenameEditor->setEnabled(have_managed);
    mGroupRenameBtn->setEnabled(have_managed);
    mGroupDissolveBtn->setEnabled(have_managed);
    setToolTipIfChanged(mGroupDelaySpinner, have_managed
        ? llformat("Start delay for '%s' (s): ACTION and Start group launch its members this many seconds late, so groups can enter in waves. 0 = immediate. Saved with the scene", managed.c_str())
        : std::string("Select a group in the list first"));
    setToolTipIfChanged(mGroupDissolveBtn, have_managed
        ? llformat("Dissolve '%s': every member is untagged (nobody leaves the cast); its delay is dropped", managed.c_str())
        : std::string("Select a group in the list first"));
    setToolTipIfChanged(mGroupRenameBtn, have_managed
        ? llformat("Rename '%s' to the name on the left (retags every member; renaming onto an existing group merges them)", managed.c_str())
        : std::string("Select a group in the list first"));
}

// ---------------------------------------------------------------------------
// Move tab: the shared transport panel, targeting the cast selection
// ---------------------------------------------------------------------------
void LLFloaterDirector::refreshMoveTab()
{
    // the shared ALPanelActorMover owns the scope radio, heading dial, params,
    // and Walk/Stop; the console's only job is to hand it the current cast
    // selection each draw. Walk/Stop honor ActorMoverSync internally -- Everyone
    // drives startAll/stopAll, Selected acts on exactly this set, so the buttons
    // behave identically to the standalone floater (which feeds its roster
    // selection the same way).
    const uuid_vec_t selected = selectedCastIds();
    if (mMoverPanel)
    {
        mMoverPanel->setSelectedActors(selected);
    }
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
// Actor FX tab: per-actor live-render styling
// ---------------------------------------------------------------------------
bool LLFloaterDirector::hasActorStyleTarget() const
{
    return mActorStyleTarget && mActorStyleTarget->getCurrentIndex() >= 0;
}

LLUUID LLFloaterDirector::actorStyleTargetId() const
{
    return hasActorStyleTarget()
        ? mActorStyleTarget->getSelectedValue().asUUID()
        : LLUUID::null;
}

void LLFloaterDirector::onActorStyleTargetChanged()
{
    // Null is both a legitimate target (You) and LLUUID's empty value, so the
    // separate mActorStyleHadTarget flag distinguishes it from no combo row.
    mActorStyleHadTarget = false;
    mActorStyleSnapshot.clear();
    refreshActorStyleTab();
}

void LLFloaterDirector::onActorStyleControlChanged()
{
    if (mRefreshingActorStyle || !hasActorStyleTarget())
    {
        return;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID id = actorStyleTargetId();
    // Start from the model record so future fields this UI does not know about
    // survive an edit to one of today's controls.
    LLDirectorCast::ActorStyle style = cast.getActorStyle(id);
    style.mEnabled = mActorStyleEnabled->get();
    style.mMode = static_cast<LLDirectorCast::EActorStyleMode>(llclamp(
        mActorStyleMode->getValue().asInteger(),
        static_cast<S32>(LLDirectorCast::ACTOR_STYLE_LAYER),
        static_cast<S32>(LLDirectorCast::ACTOR_STYLE_REPLACE)));
    style.mStyle = llclamp(
        mActorStyleLook->getValue().asInteger(),
        static_cast<S32>(LLDirectorCast::ACTOR_LOOK_DEFAULT),
        static_cast<S32>(LLDirectorCast::ACTOR_LOOK_MAX));
    style.mUseActorHue = mActorStyleUseActorHue->get();
    style.mHue = (F32)mActorStyleHue->getValue().asReal();
    style.mAlpha = (F32)mActorStyleAlpha->getValue().asReal();
    style.mDissolveProgress =
        (F32)mActorStyleDissolveProgress->getValue().asReal();
    style.mPixelSize = (F32)mActorStylePixel->getValue().asReal();
    style.mShimmerSpeed = (F32)mActorStyleShimmerSpeed->getValue().asReal();
    style.mShimmerAmount = (F32)mActorStyleShimmerAmount->getValue().asReal();
    style.mGlitch = (F32)mActorStyleGlitch->getValue().asReal();
    style.mDistortion = llclamp(
        mActorStyleDistortion->getValue().asInteger(), 0, 8);
    style.mDistortionAmount =
        (F32)mActorStyleDistortionAmount->getValue().asReal();
    style.mBrightness = (F32)mActorStyleBrightness->getValue().asReal();
    style.mEffectFps = (F32)mActorStyleEffectFps->getValue().asReal();
    cast.setActorStyle(id, style);

    // Refresh dependent enablement (master, actor hue, distortion amount) on
    // the next draw without hammering values during a slider drag.
    mActorStyleSnapshot.clear();
}

void LLFloaterDirector::onActorStyleReset()
{
    if (!hasActorStyleTarget())
    {
        return;
    }
    LLDirectorCast::instance().setActorStyle(
        actorStyleTargetId(), LLDirectorCast::ActorStyle());
    mActorStyleSnapshot.clear();
}

void LLFloaterDirector::refreshActorStyleTab()
{
    if (!mActorStyleTarget)
    {
        return;
    }

    LLDirectorCast& cast = LLDirectorCast::instance();
    const bool have_self = isAgentAvatarValid();
    // De-duplicate against the stable agent id even during a transient avatar
    // rebuild. If self is momentarily invalid, do not expose the same body as a
    // CastMember record whose style would disappear when null-id You returns.
    const LLUUID self_id = gAgent.getID();

    // Rebuild only when membership, display names, or world availability
    // changes. The explicit You row maps to null; skip the agent's real UUID
    // when it is also enrolled in Cast so the UI cannot offer two controls for
    // one rendered body.
    std::string roster_signature = have_self
        ? "you:" + self_id.asString() + "|"
        : std::string("no-you|");
    for (const LLDirectorCast::CastMember& member : cast.getCast())
    {
        if (self_id.notNull() && member.mId == self_id)
        {
            continue;
        }
        const bool in_world = cast.resolve(member.mId) != nullptr;
        roster_signature += member.mId.asString() + ":" + member.mLastName
                         + (in_world ? "+|" : "-|");
    }

    if (roster_signature != mActorStyleRosterSignature)
    {
        mActorStyleRosterSignature = roster_signature;
        const bool had_previous = mActorStyleTarget->getCurrentIndex() >= 0;
        const LLSD previous = mActorStyleTarget->getSelectedValue();

        mRefreshingActorStyle = true;
        mActorStyleTarget->clearRows();
        if (have_self)
        {
            mActorStyleTarget->add("You", LLSD(LLUUID::null));
        }
        for (const LLDirectorCast::CastMember& member : cast.getCast())
        {
            if (self_id.notNull() && member.mId == self_id)
            {
                continue;
            }
            const bool in_world = cast.resolve(member.mId) != nullptr;
            std::string label = castMemberName(member.mId);
            if (!in_world)
            {
                label += " (away)";
            }
            mActorStyleTarget->add(label, LLSD(member.mId));
        }
        if (!had_previous ||
            !mActorStyleTarget->setSelectedByValue(previous, true))
        {
            mActorStyleTarget->selectFirstItem();
        }
        mRefreshingActorStyle = false;
        mActorStyleHadTarget = false;
        mActorStyleSnapshot.clear();
    }

    const bool have_target = hasActorStyleTarget();
    mActorStyleTarget->setEnabled(mActorStyleTarget->getItemCount() > 0);
    if (!have_target)
    {
        mActorStyleEnabled->setEnabled(false);
        mActorStyleMode->setEnabled(false);
        mActorStyleLook->setEnabled(false);
        mActorStyleUseActorHue->setEnabled(false);
        mActorStyleHue->setEnabled(false);
        mActorStyleAlpha->setEnabled(false);
        mActorStyleDissolveProgress->setEnabled(false);
        mActorStyleDissolveProgress->setVisible(false);
        mActorStylePixel->setEnabled(false);
        mActorStyleShimmerSpeed->setEnabled(false);
        mActorStyleShimmerAmount->setEnabled(false);
        mActorStyleGlitch->setEnabled(false);
        mActorStyleDistortion->setEnabled(false);
        mActorStyleDistortionAmount->setEnabled(false);
        mActorStyleBrightness->setEnabled(false);
        mActorStyleEffectFps->setEnabled(false);
        mActorStyleReset->setEnabled(false);
        mActorStyleStatus->setText(LLStringExplicit(
            "You is unavailable and the Director cast is empty."));
        mActorStyleHadTarget = false;
        mActorStyleSnapshot.clear();
        return;
    }

    const LLUUID id = actorStyleTargetId();
    const LLDirectorCast::ActorStyle& style = cast.getActorStyle(id);
    const std::string snapshot = llformat(
        "%d|%d|%d|%d|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%d|%.4f|%.4f|%.4f",
        style.mEnabled ? 1 : 0, static_cast<S32>(style.mMode), style.mStyle,
        style.mUseActorHue ? 1 : 0, style.mHue, style.mAlpha,
        style.mDissolveProgress, style.mPixelSize, style.mShimmerSpeed,
        style.mShimmerAmount,
        style.mGlitch, style.mDistortion, style.mDistortionAmount,
        style.mBrightness, style.mEffectFps);

    if (!mActorStyleHadTarget || id != mActorStyleShownFor ||
        snapshot != mActorStyleSnapshot)
    {
        mRefreshingActorStyle = true;
        mActorStyleEnabled->set(style.mEnabled);
        mActorStyleMode->setValue(static_cast<S32>(style.mMode));
        mActorStyleLook->setValue(style.mStyle);
        mActorStyleUseActorHue->set(style.mUseActorHue);
        mActorStyleHue->setValue(style.mHue);
        mActorStyleAlpha->setValue(style.mAlpha);
        mActorStyleDissolveProgress->setValue(style.mDissolveProgress);
        mActorStylePixel->setValue(style.mPixelSize);
        mActorStyleShimmerSpeed->setValue(style.mShimmerSpeed);
        mActorStyleShimmerAmount->setValue(style.mShimmerAmount);
        mActorStyleGlitch->setValue(style.mGlitch);
        mActorStyleDistortion->setValue(style.mDistortion);
        mActorStyleDistortionAmount->setValue(style.mDistortionAmount);
        mActorStyleBrightness->setValue(style.mBrightness);
        mActorStyleEffectFps->setValue(style.mEffectFps);
        mRefreshingActorStyle = false;
        mActorStyleShownFor = id;
        mActorStyleHadTarget = true;
        mActorStyleSnapshot = snapshot;
    }

    mActorStyleEnabled->setEnabled(true);
    const bool active = style.mEnabled;
    mActorStyleMode->setEnabled(active);
    mActorStyleLook->setEnabled(active);
    mActorStyleUseActorHue->setEnabled(active);
    mActorStyleHue->setEnabled(active && !style.mUseActorHue);
    // Effect opacity belongs to the shared replay in both modes. Layer
    // composites it over authored beauty; Cover uses it for the replacement
    // itself (including the intentional fully-invisible zero endpoint).
    mActorStyleAlpha->setEnabled(active);
    const bool dissolve = style.mStyle == 10;
    mActorStyleDissolveProgress->setVisible(dissolve);
    mActorStyleDissolveProgress->setEnabled(active && dissolve);
    mActorStylePixel->setEnabled(active);
    mActorStyleShimmerSpeed->setEnabled(active);
    mActorStyleShimmerAmount->setEnabled(active);
    mActorStyleGlitch->setEnabled(active);
    mActorStyleDistortion->setEnabled(active);
    mActorStyleDistortionAmount->setEnabled(
        active && style.mDistortion != 0);
    mActorStyleBrightness->setEnabled(active);
    mActorStyleEffectFps->setEnabled(active);
    mActorStyleReset->setEnabled(true);

    std::string who = id.isNull() ? std::string("You")
                                  : castMemberName(id);
    if (id.notNull() && cast.resolve(id) == nullptr)
    {
        who += " (away)";
    }
    const char* mode = style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE
        ? "Opaque cover preview"
        : "Layer";
    mActorStyleStatus->setText(LLStringExplicit(
        style.mEnabled ? who + " \xC2\xB7 " + mode + " style active"
                       : who + " \xC2\xB7 styling off"));
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

// Apply the client-side play-local priority (DirectorAnimPlayLocalPriority) to a
// just-started local anim. -1 = baked (no override, byte-identical); 0..4 force
// LOW..HIGHEST. Per-instance (see LLMotion::setPriorityOverride) so other avatars
// are unaffected; cleared automatically when the anim stops.
static void apply_play_local_priority(LLVOAvatar* av, const LLUUID& anim)
{
    if (!av || anim.isNull())
    {
        return;
    }
    static LLCachedControl<S32> prio(gSavedSettings, "DirectorAnimPlayLocalPriority", -1);
    if (prio < 0)
    {
        return;
    }
    if (LLMotion* m = av->findMotion(anim))
    {
        m->setPriorityOverride(prio);
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
    if (play)
    {
        apply_play_local_priority(av, anim);
    }
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
    if (play)
    {
        apply_play_local_priority(av, anim);
    }
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

    // feed the shared preview panel: the selected row (else the pasted UUID) on
    // loop, with its Stop / Revoke / Blacklist pointed at whatever object is
    // playing it on your own avatar. The panel owns the dummy, the mouse
    // handling, and those buttons -- one copy, embedded in both hosts.
    if (mAnimPreviewPanel)
    {
        const LLUUID focus = previewAnimId();
        mAnimPreviewPanel->previewAnim(focus, animSourceObject(focus));
    }
}

// ---------------------------------------------------------------------------
// Animate tab: what the shared preview panel is fed
// ---------------------------------------------------------------------------
// The animation the preview shows and the panel's own-avatar controls act on:
// the selected signaled-animation row, else the pasted UUID.
LLUUID LLFloaterDirector::previewAnimId() const
{
    const LLUUID sel = selectedAnimId();
    return sel.notNull() ? sel : pasteAnimId();
}

// The in-world object playing a given animation on your own avatar (revoke /
// blacklist target), or null when nothing on you is playing it. Host-specific:
// the standalone Explorer instead feeds the "played by" object from its list.
LLUUID LLFloaterDirector::animSourceObject(const LLUUID& anim_id) const
{
    if (anim_id.isNull() || !isAgentAvatarValid())
    {
        return LLUUID::null;
    }
    for (const auto& [object_id, source_anim_id] : gAgentAvatarp->mAnimationSources)
    {
        if (source_anim_id == anim_id)
        {
            return object_id;
        }
    }
    return LLUUID::null;
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

void LLFloaterDirector::onClickSetSubjectFromSelection(S32 subject)
{
    const LLUUID id = avatarFromSelection();
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast& cast = LLDirectorCast::instance();
    cast.add(id);       // subjects are ids into the cast; no-op if present
    assign_subject(cast, subject, id);
}

void LLFloaterDirector::onClickClearSubject(S32 subject)
{
    clear_subject(LLDirectorCast::instance(), subject);
}

void LLFloaterDirector::onOpenActorGaze()
{
    ALFloaterActorGaze::showForSelection(
        ALFloaterActorGaze::ESelectionSource::DIRECTOR,
        selectedCastIds());
}

void LLFloaterDirector::onOpenVirtualCam()
{
    LLFloaterReg::showInstance("virtual_cam");
}

void LLFloaterDirector::refreshCameraTab()
{
    ALFloaterActorGaze::updateSelection(
        ALFloaterActorGaze::ESelectionSource::DIRECTOR,
        selectedCastIds());

    LLDirectorCast& cast = LLDirectorCast::instance();
    const LLUUID a = cast.getSubjectA();
    const LLUUID b = cast.getSubjectB();
    const LLUUID c = cast.getSubjectC();
    const LLUUID d = cast.getSubjectD();

    mSubjectAText->setText("A: " + (a.notNull() ? castMemberName(a)
                                                : std::string("\xE2\x80\x94")));
    mSubjectBText->setText("B: " + (b.notNull() ? castMemberName(b)
                                                 : std::string("\xE2\x80\x94")));
    mSubjectCText->setText("C: " + (c.notNull() ? castMemberName(c)
                                                 : std::string("\xE2\x80\x94")));
    mSubjectDText->setText("D: " + (d.notNull() ? castMemberName(d)
                                                 : std::string("\xE2\x80\x94")));

    const bool have_sel_av = avatarFromSelection().notNull();
    for (LLButton* button : { mSetABtn, mSetBBtn, mSetCBtn, mSetDBtn })
    {
        button->setEnabled(have_sel_av);
    }
    const std::string set_tip = have_sel_av
        ? std::string("Make the selected avatar this subject (adds it to the cast)")
        : std::string("Select an avatar or animesh in world first");
    setToolTipIfChanged(mSetABtn, set_tip);
    setToolTipIfChanged(mSetBBtn, set_tip);
    setToolTipIfChanged(mSetCBtn, set_tip);
    setToolTipIfChanged(mSetDBtn, set_tip);

    mClearABtn->setEnabled(a.notNull());
    setToolTipIfChanged(mClearABtn, a.notNull()
        ? std::string("Clear Subject A; the camera falls back to follow-target / selection / you")
        : std::string("Subject A is not set"));
    mClearBBtn->setEnabled(b.notNull());
    setToolTipIfChanged(mClearBBtn, b.notNull()
        ? std::string("Clear Subject B; Two-Shot/OTS fall back to you + target")
        : std::string("Subject B is not set"));
    mClearCBtn->setEnabled(c.notNull());
    setToolTipIfChanged(mClearCBtn, c.notNull()
        ? std::string("Clear Subject C")
        : std::string("Subject C is not set"));
    mClearDBtn->setEnabled(d.notNull());
    setToolTipIfChanged(mClearDBtn, d.notNull()
        ? std::string("Clear Subject D")
        : std::string("Subject D is not set"));
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
    const ALDirectorSwitcher& switcher = ALDirectorSwitcher::instance();
    const bool switching = switcher.isDrivingCamera();
    const S32 effective_mode =
        switching ? switcher.activeMode() : (S32)cam_mode;
    if (cam_enabled && effective_mode > 0)
    {
        // Name the framing target. A live per-slot mark wins while switching;
        // an empty/dead mark mirrors CineCam's fallback to the legacy chain.
        static LLCachedControl<bool> use_selected(gSavedSettings, "CinematicCamUseSelected", false);
        std::string target = "you";
        const LLUUID slot_subject = switching
            ? subject_mark_id(cast, switcher.activePrimarySubject())
            : LLUUID::null;
        if (slot_subject.notNull() && cast.resolve(slot_subject))
        {
            target = castMemberName(slot_subject);
        }
        else if (cast.getSubjectA().notNull())
        {
            target = castMemberName(cast.getSubjectA());
        }
        else if (use_selected)
        {
            const LLUUID sel = avatarFromSelection();
            target = sel.notNull() ? castMemberName(sel) : std::string("selection");
        }
        cam = switching
            ? llformat("Switcher %d: %s \xE2\x86\x92 %s",
                       switcher.activeSlot() + 1,
                       cinecam_mode_name(effective_mode), target.c_str())
            : llformat("CineCam: %s \xE2\x86\x92 %s",
                       cinecam_mode_name(effective_mode), target.c_str());
    }
    else
    {
        cam = "CineCam: off";
    }

    // queued staggered starts surface here so a delayed wave is never invisible
    std::string queued;
    if (cast.hasPendingStarts())
    {
        queued = llformat(" \xC2\xB7 %d queued", cast.pendingStartCount());
    }
    mStatusStrip->setText(llformat("%d moving%s \xC2\xB7 %s \xC2\xB7 Orbit %s",
                                   moving, queued.c_str(), cam.c_str(),
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
