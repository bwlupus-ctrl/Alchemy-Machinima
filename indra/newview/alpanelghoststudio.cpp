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

#include "alghoststudio.h"
#include "altoolghostplace.h"
#include "llactormover.h"           // actorPathColor naming consistency (style ids)
#include "llagent.h"                // agent <-> global conversion (position spinners)
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldirectorcast.h"         // source picker = the cast
#include "lljoint.h"
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
} // anonymous namespace

ALPanelGhostStudio::ALPanelGhostStudio() = default;
ALPanelGhostStudio::~ALPanelGhostStudio() = default;

// ---------------------------------------------------------------------------
bool ALPanelGhostStudio::postBuild()
{
    mShowAllCheck = getChild<LLCheckBoxCtrl>("show_all_check");
    mList         = getChild<LLScrollListCtrl>("ghost_list");
    mSourceCombo  = getChild<LLComboBox>("source_combo");
    mAddBtn       = getChild<LLButton>("btn_ghost_add");
    mDupBtn       = getChild<LLButton>("btn_ghost_dup");
    mDelBtn       = getChild<LLButton>("btn_ghost_del");

    mPosX      = getChild<LLSpinCtrl>("pos_x_spinner");
    mPosY      = getChild<LLSpinCtrl>("pos_y_spinner");
    mPosZ      = getChild<LLSpinCtrl>("pos_z_spinner");
    mYawSpin   = getChild<LLSpinCtrl>("yaw_spinner");
    mScaleSpin = getChild<LLSpinCtrl>("scale_spinner");
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

    mFreezeBtn  = getChild<LLButton>("btn_freeze");
    mLiveBtn    = getChild<LLButton>("btn_live");
    mPoseStatus = getChild<LLTextBox>("pose_status");

    mArrayCount   = getChild<LLSpinCtrl>("array_count_spinner");
    mArraySpacing = getChild<LLSpinCtrl>("array_spacing_spinner");
    mArrayLineBtn = getChild<LLButton>("btn_array_line");
    mArrayRingBtn = getChild<LLButton>("btn_array_ring");

    mStatusText = getChild<LLTextBox>("studio_status");

    mShowAllCheck->setCommitCallback([this](LLUICtrl*, const LLSD&) { onShowAllToggle(); });
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { onListSelect(); });
    mList->setDoubleClickCallback([this]() { onListDoubleClick(); });
    mAddBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAdd(); });
    mDupBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDuplicate(); });
    mDelBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDelete(); });

    mPosX->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mPosY->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mPosZ->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPosCommit(); });
    mYawSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onYawCommit(); });
    mScaleSpin->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScaleCommit(); });
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
        // never strand the user in the placement tool when the panel hides
        exitPlaceMode();
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
    // if the placement tool was taken away (build tools, focus loss), nothing
    // to un-stick here -- the tool disarms itself on deselect. Keep the master
    // check honest against external state instead.
    if (mShowAllCheck->get() != ALGhostStudio::instance().getShowAll())
    {
        mShowAllCheck->set(ALGhostStudio::instance().getShowAll());
    }
    refreshSourceCombo();
    refreshList();
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
        sig += inst.mEnabled ? '+' : '-';
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
        row["columns"][1]["column"] = "source";
        row["columns"][1]["value"] = sourceName(inst.mSource);
        row["columns"][2]["column"] = "style";
        row["columns"][2]["value"] = style_name(inst.mStyle);
        row["columns"][3]["column"] = "pose";
        row["columns"][3]["value"] = (inst.mPose == ALGhostStudio::POSE_FROZEN)
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

    // enables (the add row is always live; everything else needs a selection)
    mDupBtn->setEnabled(have);
    mDelBtn->setEnabled(have);
    mPosX->setEnabled(have);
    mPosY->setEnabled(have);
    mPosZ->setEnabled(have);
    mYawSpin->setEnabled(have);
    mScaleSpin->setEnabled(have);
    mPlaceBtn->setEnabled(have);
    mToActorBtn->setEnabled(have);
    mToMeBtn->setEnabled(have);
    mStyleCombo->setEnabled(have);
    mActorTintCheck->setEnabled(have);
    mHueSlider->setEnabled(have && !mActorTintCheck->get());
    mAlphaSlider->setEnabled(have);
    mShimmerSpeedSlider->setEnabled(have);
    mShimmerAmountSlider->setEnabled(have);
    mPixelSlider->setEnabled(have);
    mGlitchSlider->setEnabled(have);
    mBrightnessSlider->setEnabled(have);
    mFreezeBtn->setEnabled(have);
    mLiveBtn->setEnabled(have && inst->mPose == ALGhostStudio::POSE_FROZEN);
    mArrayCount->setEnabled(have);
    mArraySpacing->setEnabled(have);
    mArrayLineBtn->setEnabled(have);
    mArrayRingBtn->setEnabled(have);

    if (!have)
    {
        if (mShownFor.notNull())
        {
            mShownFor.setNull();
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

    // the rest loads on selection change only (never over in-progress edits)
    if (sel != mShownFor)
    {
        mShownFor = sel;
        mYawSpin->setValue(inst->mYaw * RAD_TO_DEG);
        mScaleSpin->setValue(inst->mScale);
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
    // refreshDetail() loads the widgets on the next draw; nothing else to do
}

void ALPanelGhostStudio::onListDoubleClick()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mEnabled = !inst->mEnabled;
    }
}

void ALPanelGhostStudio::onClickAdd()
{
    const LLUUID source = mSourceCombo->getSelectedValue().asUUID();
    if (ALGhostStudio::Instance* inst = ALGhostStudio::instance().addInstance(source))
    {
        const LLUUID id = inst->mId;    // list rebuild invalidates the pointer
        refreshList();
        mList->selectByID(id);
    }
}

void ALPanelGhostStudio::onClickDuplicate()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().duplicateInstance(selectedInstance()))
    {
        const LLUUID id = inst->mId;
        refreshList();
        mList->selectByID(id);
    }
}

void ALPanelGhostStudio::onClickDelete()
{
    ALGhostStudio::instance().removeInstance(selectedInstance());
    mShownFor.setNull();
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
        inst->mFootGlobal = gAgent.getPosGlobalFromAgent(agent_pos);
    }
}

void ALPanelGhostStudio::onYawCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mYaw = (F32)mYawSpin->getValue().asReal() * DEG_TO_RAD;
    }
}

void ALPanelGhostStudio::onScaleCommit()
{
    if (ALGhostStudio::Instance* inst =
            ALGhostStudio::instance().getInstance(selectedInstance()))
    {
        inst->mScale = llclamp((F32)mScaleSpin->getValue().asReal(), 0.05f, 10.f);
    }
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
        inst->mFootGlobal = gAgent.getPosGlobalFromAgent(foot);
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
    inst->mFootGlobal = gAgent.getPosGlobalFromAgent(foot);
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
