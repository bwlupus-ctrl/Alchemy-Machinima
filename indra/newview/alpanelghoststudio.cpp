/**
 * @file alpanelghoststudio.cpp
 * @brief Shared Ghost Studio panel -- see alpanelghoststudio.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelghoststudio.h"

#include "alcompassdial.h"          // live direct-manipulation heading control
#include "alghoststudio.h"
#include "altoolghostedit.h"        // [R2-3] persistent in-world edit mode
#include "altoolghostplace.h"
#include "llactormover.h"           // actorPathColor naming consistency (style ids)
#include "llagent.h"                // agent <-> global conversion (position spinners)
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llflyoutbutton.h"
#include "lldirectorcast.h"         // source picker = the cast
#include "lljoint.h"
#include "lllineeditor.h"
#include "llghostavatar.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp ("To me" snap)

// both the standalone Ghost Studio floater and the Director Console Ghosts tab
// embed this via <panel class="panel_ghost_studio" filename="panel_ghost_studio.xml"/>
static LLPanelInjector<ALPanelGhostStudio> t_panel_ghost_studio("panel_ghost_studio");

namespace
{
// style id -> short list label (keep in sync with EGhostStyle /
// panel_path_editor.xml's ghost_style_combo)
const char* style_name(S32 style)
{
    switch (style)
    {
    case 1:  return "Clone";
    case 2:  return "Hologram";
    case 3:  return "Wireframe";
    case 4:  return "X-ray";
    default: return "Ghost";
    }
}

const char* kind_name(ALGhostStudio::EBackingKind kind)
{
    return kind == ALGhostStudio::BACKING_ENTITY_CLONE ? "Entity" : "Overlay";
}

const char* state_name(const ALGhostStudio::Instance& inst)
{
    switch (inst.mState)
    {
    case ALGhostStudio::STATE_SPAWNING:       return "spawning";
    case ALGhostStudio::STATE_SOURCE_MISSING: return "source-missing";
    case ALGhostStudio::STATE_LOCKED:         return "locked";
    case ALGhostStudio::STATE_TEARING_DOWN:   return "teardown";
    case ALGhostStudio::STATE_ERROR:           return "error";
    default:
        if (inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            inst.mDriveMode == ALGhostStudio::DRIVE_FROZEN)
        {
            return "frozen";
        }
        return inst.mPose == ALGhostStudio::POSE_FROZEN ? "frozen" : "live";
    }
}
} // anonymous namespace

ALPanelGhostStudio::ALPanelGhostStudio() = default;
ALPanelGhostStudio::~ALPanelGhostStudio() = default;

// ---------------------------------------------------------------------------
bool ALPanelGhostStudio::postBuild()
{
    mShowAllCheck  = getChild<LLCheckBoxCtrl>("show_all_check");
    mEditModeCheck = getChild<LLCheckBoxCtrl>("edit_ghosts_check");
    mHint          = getChild<LLTextBox>("studio_hint");
    mList         = getChild<LLScrollListCtrl>("ghost_list");
    mSourceCombo  = getChild<LLComboBox>("source_combo");
    mAddBtn       = getChild<LLFlyoutButton>("btn_ghost_add");
    mDupBtn       = getChild<LLButton>("btn_ghost_dup");
    mDelBtn       = getChild<LLButton>("btn_ghost_del");
    mRefreshBtn   = getChild<LLButton>("btn_ghost_refresh");
    mTypeText     = getChild<LLTextBox>("ghost_type");
    mNameEdit     = getChild<LLLineEditor>("ghost_name");

    mPosX      = getChild<LLSpinCtrl>("pos_x_spinner");
    mPosY      = getChild<LLSpinCtrl>("pos_y_spinner");
    mPosZ      = getChild<LLSpinCtrl>("pos_z_spinner");
    mHeadingDial = getChild<ALCompassDial>("heading_dial");
    mYawSpin   = getChild<LLSpinCtrl>("yaw_spinner");
    mScaleSpin = getChild<LLSpinCtrl>("scale_spinner");
    mChaosCheck = getChild<LLCheckBoxCtrl>("chaos_check");
    mChaosSlider = getChild<LLSliderCtrl>("chaos_slider");
    mDriveModeCombo = getChild<LLComboBox>("drive_mode_combo");
    mDirectedAnimEdit = getChild<LLLineEditor>("directed_anim_editor");
    mPlaceBtn  = getChild<LLButton>("btn_place");
    mToActorBtn = getChild<LLButton>("btn_to_actor");
    mToMeBtn   = getChild<LLButton>("btn_to_me");

    mStyleCombo     = getChild<LLComboBox>("style_combo");
    mActorTintCheck = getChild<LLCheckBoxCtrl>("actor_tint_check");
    mHueSlider      = getChild<LLSliderCtrl>("hue_slider");
    mAlphaSlider    = getChild<LLSliderCtrl>("alpha_slider");
    mShimmerSpeedSlider  = getChild<LLSliderCtrl>("shimmer_speed_slider");
    mShimmerAmountSlider = getChild<LLSliderCtrl>("shimmer_amount_slider");
    mPixelSlider    = getChild<LLSliderCtrl>("pixel_slider");
    mGlitchSlider   = getChild<LLSliderCtrl>("glitch_slider");
    mBrightnessSlider = getChild<LLSliderCtrl>("brightness_slider");
    mEntityLookCombo = getChild<LLComboBox>("entity_look_combo");

    mFreezeBtn  = getChild<LLButton>("btn_freeze");
    mLiveBtn    = getChild<LLButton>("btn_live");
    mPoseStatus = getChild<LLTextBox>("pose_status");

    mArrayCount   = getChild<LLSpinCtrl>("array_count_spinner");
    mArraySpacing = getChild<LLSpinCtrl>("array_spacing_spinner");
    mArrayLineBtn = getChild<LLButton>("btn_array_line");
    mArrayRingBtn = getChild<LLButton>("btn_array_ring");

    mStatusText = getChild<LLTextBox>("studio_status");

    mShowAllCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShowAllToggle(); });
    mEditModeCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onToggleEditMode(); });
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onListSelect(); });
    mList->setDoubleClickCallback([this]() { onListDoubleClick(); });
    mAddBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAdd(); });
    mNameEdit->setCommitCallback([this](LLUICtrl*, const LLSD&) { onNameCommit(); });
    mDupBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDuplicate(); });
    mDelBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDelete(); });
    mRefreshBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickRefresh(); });

    mPosX->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mPosY->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mPosZ->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    // ALCompassDial commits on mouse-down and every hover while captured, so
    // this is deliberately the live-drag callback (same idiom as Actor Mover).
    mHeadingDial->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onHeadingDialCommit(); });
    mYawSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onYawCommit(); });
    mScaleSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScaleCommit(); });
    mChaosCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onChaosCommit(); });
    mChaosSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onChaosCommit(); });
    mDriveModeCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDriveModeCommit(); });
    mDirectedAnimEdit->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDirectedAnimCommit(); });
    mPlaceBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPlace(); });
    mToActorBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickToActor(); });
    mToMeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickToMe(); });

    mStyleCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStyleCommit(); });
    mActorTintCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onActorTintToggle(); });
    mHueSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onHueCommit(); });
    mAlphaSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAlphaCommit(); });
    mShimmerSpeedSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShimmerSpeedCommit(); });
    mShimmerAmountSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShimmerAmountCommit(); });
    mPixelSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPixelCommit(); });
    mGlitchSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGlitchCommit(); });
    mBrightnessSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBrightnessCommit(); });
    mEntityLookCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEntityLookCommit(); });

    mFreezeBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickFreeze(); });
    mLiveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickLive(); });

    mArrayLineBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickArray(false); });
    mArrayRingBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickArray(true); });

    mShowAllCheck->set(ALGhostStudio::instance().getShowAll());
    return true;
}

void ALPanelGhostStudio::onVisibilityChange(bool new_visibility)
{
    if (!new_visibility)
    {
        // never strand the user in a tool when the panel hides
        exitPlaceMode();
        exitEditMode();
    }
    LLPanel::onVisibilityChange(new_visibility);
}

// ---------------------------------------------------------------------------
std::string ALPanelGhostStudio::sourceName(const LLUUID& id)
{
    if (id.isNull())
    {
        return std::string("You");
    }
    if (const LLDirectorCast::CastMember* m = LLDirectorCast::instance().getMember(id))
    {
        if (!m->mLastName.empty())
        {
            return m->mLastName;
        }
    }
    if (LLGhostAvatar::isGhostId(id))
    {
        return std::string("Entity clone ") + id.asString().substr(0, 8);
    }
    if (LLAvatarName av_name; LLAvatarNameCache::get(id, &av_name))
    {
        return av_name.getCompleteName();
    }
    return id.asString().substr(0, 8);
}

LLUUID ALPanelGhostStudio::selectedInstance() const
{
    LLScrollListItem* item = mList->getFirstSelected();
    return item ? item->getValue().asUUID() : LLUUID::null;
}

// ---------------------------------------------------------------------------
void ALPanelGhostStudio::draw()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    studio.refreshLifecycleStates();

    // keep the master check honest against external state
    if (mShowAllCheck->get() != studio.getShowAll())
    {
        mShowAllCheck->set(studio.getShowAll());
    }

    // [R2-3] if our edit tool was taken away (build tools, another picker, Esc
    // inside the tool), reflect that in the toggle instead of a stale "on" --
    // the path panel's exact idiom
    // Edit mode is the tool's EXPLICIT state, NOT "ALToolGhostEdit is current":
    // once a ghost is selected the proxy hands off to the stock translate/rotate
    // tool, so the ghost tool is no longer current while editing continues.
    if (mEditMode && !ALToolGhostEdit::getInstance()->isEditModeActive())
    {
        mEditMode = false;
    }
    if (mEditModeCheck->get() != mEditMode)
    {
        mEditModeCheck->set(mEditMode);
    }
    // hint line follows the mode (diffed; tells the hands what they can do)
    if (mHintEdit != mEditMode)
    {
        mHintEdit = mEditMode;
        mHint->setText(mEditMode
            ? std::string("Click a ghost to select \xC2\xB7 move/rotate with the gizmo \xC2\xB7 scale below \xC2\xB7 Del removes \xC2\xB7 Esc exits")
            : std::string("Styled copies of cast bodies: place, pose, multiply. Double-click a row to show/hide it"));
    }

    refreshSourceCombo();
    refreshList();

    // [R2-3] mirror the SHARED selection (the in-world edit tool writes it;
    // both panel hosts follow). selectByID is programmatic -- no commit loop.
    const LLUUID shared_sel = studio.getSelected();
    if (shared_sel != selectedInstance())
    {
        if (shared_sel.isNull())
        {
            mList->deselectAllItems(true);
        }
        else
        {
            mList->selectByID(shared_sel);
        }
    }

    refreshDetail();
    refreshStatus();
    LLPanel::draw();
}

// ---------------------------------------------------------------------------
// refreshers
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::refreshSourceCombo()
{
    // "You" plus every cast member; rebuilt only when the cast (or a cached
    // name) changes so the dropdown never shuffles mid-interaction
    LLDirectorCast& cast = LLDirectorCast::instance();
    std::string sig;
    for (const LLDirectorCast::CastMember& m : cast.getCast())
    {
        sig += m.mId.asString();
        sig += m.mLastName;
        sig += '|';
    }
    if (sig == mSourceSig)
    {
        return;
    }
    mSourceSig = sig;
    const LLSD prev = mSourceCombo->getSelectedValue();
    mSourceCombo->clearRows();
    mSourceCombo->add("You", LLSD(LLUUID::null));
    for (const LLDirectorCast::CastMember& m : cast.getCast())
    {
        mSourceCombo->add(sourceName(m.mId), LLSD(m.mId));
    }
    if (!prev.isDefined() || !mSourceCombo->setSelectedByValue(prev, true))
    {
        mSourceCombo->selectFirstItem();    // "You"
    }
}

void ALPanelGhostStudio::refreshList()
{
    ALGhostStudio& studio = ALGhostStudio::instance();

    // composed signature: rebuild rows only when something row-visible changed
    std::string sig;
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        sig += inst.mId.asString();
        sig += inst.mName;
        sig += inst.mEnabled ? '+' : '-';
        sig += kind_name(inst.mKind);
        sig += state_name(inst);
        sig += style_name(inst.mStyle);
        sig += (inst.mPose == ALGhostStudio::POSE_FROZEN) ? 'F' : 'L';
        sig += sourceName(inst.mSource);
        sig += '|';
    }
    if (sig == mListSig)
    {
        return;
    }
    mListSig = sig;

    const LLUUID prev_sel = selectedInstance();
    mList->deleteAllItems();
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        LLSD row;
        row["value"] = inst.mId;
        row["columns"][0]["column"] = "on";
        row["columns"][0]["value"] = inst.mEnabled ? "\xE2\x97\x8F" : "\xE2\x97\x8B";  // filled / hollow dot
        row["columns"][1]["column"] = "name";
        row["columns"][1]["value"] = inst.mName;
        row["columns"][2]["column"] = "kind";
        row["columns"][2]["value"] = kind_name(inst.mKind);
        row["columns"][3]["column"] = "state";
        row["columns"][3]["value"] = state_name(inst);
        row["columns"][4]["column"] = "source";
        row["columns"][4]["value"] = inst.mSourceLabel.empty()
            ? sourceName(inst.mSource) : inst.mSourceLabel;
        row["columns"][5]["column"] = "style";
        row["columns"][5]["value"] = inst.mKind == ALGhostStudio::BACKING_ENTITY_CLONE
            ? std::string("Scene-lit") : std::string(style_name(inst.mStyle));
        row["columns"][6]["column"] = "pose";
        row["columns"][6]["value"] = (inst.mPose == ALGhostStudio::POSE_FROZEN)
            ? std::string("FROZEN") : std::string("live");
        mList->addElement(row, ADD_BOTTOM);
    }
    if (prev_sel.notNull())
    {
        mList->selectByID(prev_sel);
    }
}

void ALPanelGhostStudio::refreshDetail()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const LLUUID sel = selectedInstance();
    ALGhostStudio::Instance* inst = studio.getInstance(sel);
    const bool have = inst != nullptr;
    const bool overlay = have && inst->mKind == ALGhostStudio::BACKING_OVERLAY;

    // enables (the add row is always live; everything else needs a selection)
    mDupBtn->setEnabled(overlay);
    mDelBtn->setEnabled(have);
    mNameEdit->setEnabled(have);
    mPosX->setEnabled(have);
    mPosY->setEnabled(have);
    mPosZ->setEnabled(have);
    mHeadingDial->setEnabled(have);
    mYawSpin->setEnabled(have);
    mScaleSpin->setEnabled(have);
    const bool entity = have && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
    mRefreshBtn->setVisible(entity);
    mRefreshBtn->setEnabled(entity);
    std::string type = "Type: No selection";
    if (overlay)
    {
        type = "Type: Overlay ghost";
    }
    else if (entity)
    {
        type = std::string("Type: Entity clone \xC2\xB7 ") + state_name(*inst);
    }
    if (mTypeText->getText() != type)
    {
        mTypeText->setText(type);
    }
    mChaosCheck->setEnabled(entity);
    mChaosSlider->setEnabled(entity && inst->mChaosEnabled);
    mEntityLookCombo->setEnabled(entity);
    mDriveModeCombo->setEnabled(entity);
    mDirectedAnimEdit->setEnabled(entity &&
        inst->mDriveMode == ALGhostStudio::DRIVE_DIRECTED);
    mPlaceBtn->setEnabled(overlay);
    mToActorBtn->setEnabled(overlay);
    mToMeBtn->setEnabled(overlay);
    mStyleCombo->setEnabled(overlay);
    mActorTintCheck->setEnabled(overlay);
    mHueSlider->setEnabled(overlay && !mActorTintCheck->get());
    mAlphaSlider->setEnabled(overlay);
    mShimmerSpeedSlider->setEnabled(overlay);
    mShimmerAmountSlider->setEnabled(overlay);
    mPixelSlider->setEnabled(overlay);
    mGlitchSlider->setEnabled(overlay);
    mBrightnessSlider->setEnabled(overlay);
    mFreezeBtn->setEnabled(overlay);
    mLiveBtn->setEnabled(overlay && inst->mPose == ALGhostStudio::POSE_FROZEN);
    mArrayCount->setEnabled(overlay);
    mArraySpacing->setEnabled(overlay);
    mArrayLineBtn->setEnabled(overlay);
    mArrayRingBtn->setEnabled(overlay);

    if (!have)
    {
        if (mShownFor.notNull())
        {
            mShownFor.setNull();
            mNameEdit->setText(std::string());
            mPoseStatus->setText(std::string("Select a ghost"));
        }
        return;
    }

    // POSITION mirrors every frame (cheap; a LIVE ghost never moves itself,
    // but Place / snaps / arrays move it from outside the spinners) -- unless
    // the operator is typing in one
    if (!mPosX->hasFocus() && !mPosY->hasFocus() && !mPosZ->hasFocus())
    {
        const LLVector3 agent_pos = gAgent.getPosAgentFromGlobal(inst->mFootGlobal);
        mPosX->setValue(agent_pos.mV[VX]);
        mPosY->setValue(agent_pos.mV[VY]);
        mPosZ->setValue(agent_pos.mV[VZ]);
    }
    if (!mScaleSpin->hasFocus())
    {
        mScaleSpin->setValue(inst->mScale);
        mChaosCheck->set(inst->mChaosEnabled);
        mChaosSlider->setValue(inst->mChaosAmount);
        mEntityLookCombo->setValue(inst->mLook);
    }
    if (entity && !mDriveModeCombo->hasFocus())
    {
        mDriveModeCombo->setValue(inst->mDriveMode);
    }
    if (entity && !mDirectedAnimEdit->hasFocus())
    {
        mDirectedAnimEdit->setText(inst->mDirectedAnim.asString());
    }

    // the rest loads on selection change only (never over in-progress edits)
    if (sel != mShownFor)
    {
        mShownFor = sel;
        mNameEdit->setText(inst->mName);
        mHeadingDial->setValue(inst->getYaw() * RAD_TO_DEG);
        mYawSpin->setValue(inst->getYaw() * RAD_TO_DEG);
        mScaleSpin->setValue(inst->mScale);
        mDriveModeCombo->setValue(inst->mDriveMode);
        mDirectedAnimEdit->setText(inst->mDirectedAnim.asString());
        mStyleCombo->setValue(inst->mStyle);
        mActorTintCheck->set(inst->mUseActorTint);
        mHueSlider->setValue(inst->mHue);
        mAlphaSlider->setValue(inst->mAlpha);
        mShimmerSpeedSlider->setValue(inst->mShimmerSpeed);
        mShimmerAmountSlider->setValue(inst->mShimmerIntensity);
        mPixelSlider->setValue(inst->mPixelSize);
        mGlitchSlider->setValue(inst->mGlitch);
        mBrightnessSlider->setValue(inst->mBrightness);
    }

    // pose status: what the ghost is doing, and why a freeze might not bite
    std::string pose;
    if (inst->mPose == ALGhostStudio::POSE_FROZEN)
    {
        pose = "FROZEN pose held";
    }
    else if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        pose = "Scene-lit entity; overlay style controls unavailable";
    }
    else
    {
        LLVOAvatar* av = LLDirectorCast::instance().resolve(inst->mSource);
        pose = (av && LLActorMover::instance().ghostBatchesFor(av->getID()))
             ? "Following live pose"
             : "Source not rendering (no ghost)";
    }
    if (mPoseStatus->getText() != pose)
    {
        mPoseStatus->setText(pose);
    }
}

void ALPanelGhostStudio::refreshStatus()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    S32 shown = 0;
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        if (inst.mEnabled)
        {
            ++shown;
        }
    }
    std::string txt = llformat("%d ghost(s), %d shown%s",
                               (S32)studio.getInstances().size(), shown,
                               studio.getShowAll() ? "" : " (all hidden)");
    if (mStatusText->getText() != txt)
    {
        mStatusText->setText(txt);
    }
}

// ---------------------------------------------------------------------------
// list + CRUD
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onListSelect()
{
    // [R2-3] the list drives the SHARED selection (tool + both hosts follow);
    // refreshDetail() loads the widgets on the next draw
    ALGhostStudio::instance().setSelected(selectedInstance());
}

void ALPanelGhostStudio::onListDoubleClick()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        ALGhostStudio::instance().setInstanceEnabled(inst->mId, !inst->mEnabled);
    }
}

void ALPanelGhostStudio::onClickAdd()
{
    const LLUUID source = mSourceCombo->getSelectedValue().asUUID();
    ALGhostStudio::Instance* inst = nullptr;
    if (mAddBtn->getValue().asInteger() == ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        inst = ALGhostStudio::instance().spawnEntityClone(source, sourceName(source));
    }
    else
    {
        inst = ALGhostStudio::instance().addInstance(source);
    }
    if (inst)
    {
        const LLUUID id = inst->mId;    // list rebuild invalidates the pointer
        ALGhostStudio::instance().setSelected(id);
        refreshList();
        mList->selectByID(id);
    }
}

void ALPanelGhostStudio::onNameCommit()
{
    ALGhostStudio::instance().renameInstance(selectedInstance(), mNameEdit->getText());
}

void ALPanelGhostStudio::onClickDuplicate()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().duplicateInstance(selectedInstance()))
    {
        const LLUUID id = inst->mId;
        ALGhostStudio::instance().setSelected(id);
        refreshList();
        mList->selectByID(id);
    }
}

void ALPanelGhostStudio::onClickDelete()
{
    ALGhostStudio::instance().removeInstance(selectedInstance());
    mShownFor.setNull();
}

void ALPanelGhostStudio::onClickRefresh()
{
    ALGhostStudio::instance().refreshEntityClone(selectedInstance());
}

// ---------------------------------------------------------------------------
// placement
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onPosCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        const LLVector3 agent_pos((F32)mPosX->getValue().asReal(),
                                  (F32)mPosY->getValue().asReal(),
                                  (F32)mPosZ->getValue().asReal());
        inst->setFootGlobal(gAgent.getPosGlobalFromAgent(agent_pos));
        ALGhostStudio::instance().applyEntityTransform(inst->mId);
    }
}

void ALPanelGhostStudio::onYawCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        const F32 yaw_degrees = (F32)mYawSpin->getValue().asReal();
        mHeadingDial->setValue(yaw_degrees);
        inst->setYaw(yaw_degrees * DEG_TO_RAD);
        if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            ALGhostStudio::instance().applyEntityTransform(inst->mId);
        }
    }
}

void ALPanelGhostStudio::onHeadingDialCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        const F32 yaw_degrees = (F32)mHeadingDial->getValue().asReal();
        mYawSpin->setValue(yaw_degrees);
        // setYaw immediately updates mRotation and mTransformRevision. Overlay
        // rendering consumes mRotation directly; entity clones additionally
        // need their local LLGhostAvatar transform pushed on every drag tick.
        inst->setYaw(yaw_degrees * DEG_TO_RAD);
        if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            ALGhostStudio::instance().applyEntityTransform(inst->mId);
        }
    }
}

void ALPanelGhostStudio::onChaosCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        const F32 amount = mChaosCheck->get()
            ? (F32)mChaosSlider->getValue().asReal() : 0.f;
        ALGhostStudio::instance().setInstanceChaos(inst->mId, amount);
        mChaosSlider->setEnabled(mChaosCheck->get());
    }
}

void ALPanelGhostStudio::onScaleCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        ALGhostStudio::instance().setInstanceScale(
            inst->mId, llclamp((F32)mScaleSpin->getValue().asReal(), 0.05f, 10.f));
    }
}

void ALPanelGhostStudio::onDriveModeCommit()
{
    ALGhostStudio::Instance* inst =
        ALGhostStudio::instance().getInstance(selectedInstance());
    if (!inst || inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        return;
    }
    const ALGhostStudio::EDriveMode mode =
        (ALGhostStudio::EDriveMode)mDriveModeCombo->getValue().asInteger();
    LLUUID anim(mDirectedAnimEdit->getText());
    ALGhostStudio::instance().setInstanceDriveMode(inst->mId, mode, anim);
}

void ALPanelGhostStudio::onDirectedAnimCommit()
{
    ALGhostStudio::Instance* inst =
        ALGhostStudio::instance().getInstance(selectedInstance());
    if (!inst || inst->mKind != ALGhostStudio::BACKING_ENTITY_CLONE ||
        inst->mDriveMode != ALGhostStudio::DRIVE_DIRECTED)
    {
        return;
    }
    LLUUID anim(mDirectedAnimEdit->getText());
    if (anim.isNull())
    {
        return;
    }
    ALGhostStudio::instance().setInstanceDriveMode(
        inst->mId, ALGhostStudio::DRIVE_DIRECTED, anim);
}

void ALPanelGhostStudio::onClickPlace()
{
    const LLUUID sel = selectedInstance();
    if (sel.isNull())
    {
        return;
    }
    // one-shot: the tool places on the next ground click and hands the camera
    // straight back (Esc / right-click cancels) -- ALToolPathEdit's walk-to idiom
    ALToolGhostPlace* tool = ALToolGhostPlace::getInstance();
    tool->armFor(sel);
    LLToolMgr::getInstance()->setTransientTool(tool);
}

void ALPanelGhostStudio::onClickToActor()
{
    ALGhostStudio::Instance* inst =
        ALGhostStudio::instance().getInstance(selectedInstance());
    if (!inst)
    {
        return;
    }
    LLVOAvatar* av = LLDirectorCast::instance().resolve(inst->mSource);
    if (av && av->getRootJoint())
    {
        LLVector3 foot = av->getRootJoint()->getWorldPosition();
        foot.mV[VZ] -= av->getPelvisToFoot();
        inst->setFootGlobal(gAgent.getPosGlobalFromAgent(foot));
    }
}

void ALPanelGhostStudio::onClickToMe()
{
    ALGhostStudio::Instance* inst =
        ALGhostStudio::instance().getInstance(selectedInstance());
    if (!inst || !isAgentAvatarValid() || !gAgentAvatarp->getRootJoint())
    {
        return;
    }
    LLVector3 foot = gAgentAvatarp->getRootJoint()->getWorldPosition();
    foot.mV[VZ] -= gAgentAvatarp->getPelvisToFoot();
    inst->setFootGlobal(gAgent.getPosGlobalFromAgent(foot));
}

void ALPanelGhostStudio::exitPlaceMode()
{
    LLToolMgr* tm = LLToolMgr::getInstance();
    if (tm->usingTransientTool()
        && tm->getCurrentTool() == ALToolGhostPlace::getInstance())
    {
        tm->clearTransientTool();
    }
}

// ---------------------------------------------------------------------------
// [R2-3] in-world edit mode (persistent ALToolGhostEdit)
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onToggleEditMode()
{
    const bool want = mEditModeCheck->get();
    if (want)
    {
        // the one-shot Place arm and the persistent edit mode are exclusive
        exitPlaceMode();
        LLToolMgr::getInstance()->setTransientTool(ALToolGhostEdit::getInstance());
        mEditMode = true;
    }
    else
    {
        exitEditMode();
    }
}

void ALPanelGhostStudio::exitEditMode()
{
    if (mEditMode)
    {
        mEditMode = false;
        // The proxy may have handed off to the stock toolset, so ALToolGhostEdit
        // is no longer current/transient -- stopEditMode() tears the proxy down
        // and restores the prior toolset regardless. Also clear the transient
        // tool in the (no-selection) case where the ghost tool is still current.
        ALToolGhostEdit::getInstance()->stopEditMode();
        LLToolMgr* tm = LLToolMgr::getInstance();
        if (tm->usingTransientTool()
            && tm->getCurrentTool() == ALToolGhostEdit::getInstance())
        {
            tm->clearTransientTool();   // restores the prior tool (camera control)
        }
    }
    if (mEditModeCheck)
    {
        mEditModeCheck->set(false);
    }
}

// ---------------------------------------------------------------------------
// look
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onStyleCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mStyle = mStyleCombo->getValue().asInteger();
    }
}

void ALPanelGhostStudio::onActorTintToggle()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mUseActorTint = mActorTintCheck->get();
    }
}

void ALPanelGhostStudio::onHueCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mHue = (F32)mHueSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onAlphaCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mAlpha = llclamp((F32)mAlphaSlider->getValue().asReal(), 0.f, 1.f);
    }
}

void ALPanelGhostStudio::onShimmerSpeedCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mShimmerSpeed = (F32)mShimmerSpeedSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onShimmerAmountCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mShimmerIntensity = (F32)mShimmerAmountSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onPixelCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mPixelSize = (F32)mPixelSlider->getValue().asReal();
    }
}

void ALPanelGhostStudio::onGlitchCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mGlitch = (F32)mGlitchSlider->getValue().asReal();
    }
}

// [R2-1] output brightness -- the unlit clone's night-scene dimmer. A
// "match scene" helper was considered and SKIPPED: the ghost draws into the
// post-tonemap overlay, and a sky ambient/sun probe yields scene-referred
// linear values that do not map to a post-tonemap multiplier without
// inverting exposure + tonemap and ignoring local lights -- the estimate
// would be least reliable exactly in the night-with-practicals shots it is
// meant for. The slider is the honest control.
void ALPanelGhostStudio::onBrightnessCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mBrightness = llclamp((F32)mBrightnessSlider->getValue().asReal(), 0.05f, 1.5f);
    }
}

void ALPanelGhostStudio::onEntityLookCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        ALGhostStudio::instance().setInstanceLook(
            inst->mId, (ALGhostStudio::EGhostLook)mEntityLookCombo->getValue().asInteger());
    }
}

// ---------------------------------------------------------------------------
// pose
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onClickFreeze()
{
    const LLUUID sel = selectedInstance();
    if (sel.isNull())
    {
        return;
    }
    if (!ALGhostStudio::instance().freezeInstance(sel))
    {
        // most common miss: the pipeline is not rendering the source this
        // frame (instance disabled / master off / skins still loading)
        mPoseStatus->setText(std::string("Nothing to grab yet -- ghost must be rendering"));
    }
}

void ALPanelGhostStudio::onClickLive()
{
    ALGhostStudio::instance().unfreezeInstance(selectedInstance());
}

// ---------------------------------------------------------------------------
// array helper + master toggle
// ---------------------------------------------------------------------------
void ALPanelGhostStudio::onClickArray(bool ring)
{
    const LLUUID sel = selectedInstance();
    if (sel.isNull())
    {
        return;
    }
    ALGhostStudio::instance().makeArray(sel,
                                        mArrayCount->getValue().asInteger(),
                                        (F32)mArraySpacing->getValue().asReal(),
                                        ring);
}

void ALPanelGhostStudio::onShowAllToggle()
{
    ALGhostStudio::instance().setShowAll(mShowAllCheck->get());
}
