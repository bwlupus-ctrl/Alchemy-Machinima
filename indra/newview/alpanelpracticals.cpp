/**
 * @file alpanelpracticals.cpp
 * @brief Director > Light > Practicals page logic -- see alpanelpracticals.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelpracticals.h"
#include "alavatarsubjectpicker.h"
#include "alprojectortracker.h"
#include "llagent.h"
#include "llfloaterdirector.h"
#include "llprismlens.h"
#include "lldirectorcast.h"
#include "llmenugl.h"
#include "llvoavatar.h"

#include "aluirefresh.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llvovolume.h"
#include "pipeline.h"

// The injector name must match class="panel_practicals" in the generated page XML.
static LLPanelInjector<ALPanelPracticals> t_panel_practicals("panel_practicals");

bool ALPanelPracticals::postBuild()
{
    mTrack = getChild<LLCheckBoxCtrl>("sl_track");
    mTrackJoint = getChild<LLComboBox>("sl_track_joint");
    mTrackMode = getChild<LLComboBox>("sl_track_mode");
    mTrackSmoothing = getChild<LLSliderCtrl>("sl_track_smoothing");
    mTrackCapture = getChild<LLButton>("sl_track_capture");
    mTrackChoose = getChild<LLButton>("sl_track_subject_choose");
    mTrackSelected = getChild<LLButton>("sl_track_subject_selected");
    mTrackYou = getChild<LLButton>("sl_track_subject_you");
    mTrackClear = getChild<LLButton>("sl_track_subject_clear");
    mTrackSubject = getChild<LLTextBox>("sl_track_subject_name");
    mTrackStatus = getChild<LLTextBox>("sl_track_status");
    mTrack->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        ALProjectorTracker::instance().enable(mLastId, mTrack->getValue().asBoolean());
        refreshTracking(true);
    });
    for (LLUICtrl* control : {static_cast<LLUICtrl*>(mTrackJoint),
            static_cast<LLUICtrl*>(mTrackMode), static_cast<LLUICtrl*>(mTrackSmoothing)})
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTrackingChanged(); });
    mTrackCapture->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { ALProjectorTracker::instance().capture(mLastId); });
    mTrackChoose->setCommitCallback([this](LLUICtrl* anchor, const LLSD&)
    {
        const LLUUID projector = mLastId;
        ALAvatarSubjectPicker::choose(anchor, this, mTrackMenu, true,
            [this, projector](const LLUUID& id) { assignTrackingSubject(projector, id, true); });
    });
    mTrackSelected->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        auto* avatar = ALAvatarSubjectPicker::fromSelection();
        if (ALAvatarSubjectPicker::valid(avatar, true))
        {
            assignTrackingSubject(mLastId, avatar->getID(), true);
            return;
        }
        // The projector must remain the world selection. A cast-row selection
        // is independent, so Use selected can still assign an actor here.
        for (LLView* parent = getParent(); parent; parent = parent->getParent())
        {
            auto* director = dynamic_cast<LLFloaterDirector*>(parent);
            if (!director) continue;
            const auto selected = director->currentCastSelection();
            if (!selected.empty() && ALAvatarSubjectPicker::valid(
                    LLDirectorCast::instance().resolve(selected.front()), true))
                assignTrackingSubject(mLastId, selected.front(), true);
            break;
        }
    });
    mTrackYou->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { assignTrackingSubject(mLastId, gAgentID, true); });
    mTrackClear->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { assignTrackingSubject(mLastId, LLUUID::null, false); });
    mCastShadows = getChild<LLCheckBoxCtrl>("sl_cast_shadows");
    mVolumetric = getChild<LLCheckBoxCtrl>("sl_volumetric");
    mCapture = getChild<LLButton>("sl_capture");
    mClear = getChild<LLButton>("sl_clear");
    mStatus = getChild<LLTextBox>("sl_status");
    mGoboPattern = getChild<LLComboBox>("sl_gobo");
    mGoboAnim = getChild<LLComboBox>("sl_gobo_animmode");
    mGoboSpeed = getChild<LLSliderCtrl>("sl_gobo_speed");
    mGoboZoom = getChild<LLSliderCtrl>("sl_gobo_zoom");
    mGoboDispersion = getChild<LLSliderCtrl>("sl_gobo_dispersion");
    mGoboTint = getChild<LLColorSwatchCtrl>("sl_gobo_tint");
    mGoboVar1 = getChild<LLSliderCtrl>("sl_gobo_var1");
    mGoboVar2 = getChild<LLSliderCtrl>("sl_gobo_var2");
    mGoboSoft = getChild<LLSliderCtrl>("sl_gobo_soft");
    mGoboInvert = getChild<LLCheckBoxCtrl>("sl_gobo_invert");

    mCastShadows->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCastShadows(); });
    mVolumetric->setCommitCallback([this](LLUICtrl*, const LLSD&) { onVolumetric(); });
    mCapture->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCaptureOverride(); });
    mClear->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClearOverride(); });
    for (LLUICtrl* control : { static_cast<LLUICtrl*>(mGoboPattern), static_cast<LLUICtrl*>(mGoboAnim),
                               static_cast<LLUICtrl*>(mGoboSpeed), static_cast<LLUICtrl*>(mGoboZoom),
                               static_cast<LLUICtrl*>(mGoboDispersion), static_cast<LLUICtrl*>(mGoboTint),
                               static_cast<LLUICtrl*>(mGoboVar1), static_cast<LLUICtrl*>(mGoboVar2),
                               static_cast<LLUICtrl*>(mGoboSoft), static_cast<LLUICtrl*>(mGoboInvert) })
    {
        control->setCommitCallback([this](LLUICtrl*, const LLSD&) { onGoboChanged(); });
    }

    refreshFromSelection();
    return LLPanel::postBuild();
}

void ALPanelPracticals::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    refreshFromSelection();
    LLPanel::draw();
}

// Follows the primary selection: the per-projector controls are enabled only for a spotlight
// projector, and the session-only state (cast-shadows opt-out, volumetric-shaft opt-in, captured
// shaft override, gobo) is mirrored from gPipeline's in-memory sets. Diff-guarded writes only.
void ALPanelPracticals::refreshFromSelection()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    const bool is_projector = volume && volume->isLightSpotlight();

    // Identity of what is mirrored. In the frame it changes (another object, or projector <-> not),
    // every mirror write bypasses the keyboard-focus guard, so a newly selected light never shows
    // the previous light's state because a control kept focus.
    const LLUUID selected_id = object ? object->getID() : LLUUID::null;
    const bool identity_changed = selected_id != mLastId || is_projector != mLastProjector;
    mLastId = selected_id;
    mLastProjector = is_projector;
    refreshTracking(identity_changed);
    const bool force_ctrl = identity_changed;

    setEnabledIfChanged(mCastShadows, is_projector);
    // A VCam-bound point light also has a viewer-local projector. Mirror its
    // flag without broadening the other spotlight-only editing controls.
    const bool has_volumetric = volume && (is_projector ||
        LLPipeline::hasVCamVolumetricOverride(selected_id));
    setEnabledIfChanged(mVolumetric, has_volumetric);
    setValueIfChanged(mVolumetric, LLSD(has_volumetric &&
        LLPipeline::isVolumetricShaftEnabled(selected_id)), true);
    setEnabledIfChanged(mCapture, is_projector);
    setEnabledIfChanged(mGoboPattern, is_projector);
    setEnabledIfChanged(mGoboAnim, is_projector);
    setEnabledIfChanged(mGoboSpeed, is_projector);
    setEnabledIfChanged(mGoboZoom, is_projector);
    setEnabledIfChanged(mGoboDispersion, is_projector);
    setEnabledIfChanged(mGoboTint, is_projector);
    setEnabledIfChanged(mGoboVar1, is_projector);
    setEnabledIfChanged(mGoboVar2, is_projector);
    setEnabledIfChanged(mGoboSoft, is_projector);
    setEnabledIfChanged(mGoboInvert, is_projector);

    if (is_projector)
    {
        const LLUUID& id = object->getID();
        // Checked = casts shadows (the default), i.e. NOT in the opt-out set.
        setValueIfChanged(mCastShadows, LLSD(!LLPipeline::isProjectorNoShadow(id)), true);
        setEnabledIfChanged(mClear, LLPipeline::hasVolumetricShaftOverride(id) ||
                                    LLPipeline::hasGoboOverride(id));
        const LLPipeline::GoboOverride gobo = LLPipeline::getGoboOverride(id);
        setValueIfChanged(mGoboPattern, LLSD(gobo.mPattern), force_ctrl);
        setValueIfChanged(mGoboAnim, LLSD(gobo.mAnimMode), force_ctrl);
        setValueIfChanged(mGoboSpeed, LLSD(static_cast<F64>(gobo.mSpeed)), force_ctrl);
        setValueIfChanged(mGoboZoom, LLSD(static_cast<F64>(gobo.mZoom)), force_ctrl);
        setValueIfChanged(mGoboDispersion, LLSD(static_cast<F64>(gobo.mDispersion)), force_ctrl);
        // Refresh the swatch (and an open picker) only when the model colour changes.
        const LLColor4 tint(gobo.mTint);
        if (mGoboTint->get() != tint)
        {
            mGoboTint->set(tint, TRUE);
        }
        setValueIfChanged(mGoboVar1, LLSD(static_cast<F64>(gobo.mPatternParams.mV[0])), force_ctrl);
        setValueIfChanged(mGoboVar2, LLSD(static_cast<F64>(gobo.mPatternParams.mV[1])), force_ctrl);
        setValueIfChanged(mGoboSoft, LLSD(static_cast<F64>(gobo.mPatternParams.mV[2])), force_ctrl);
        setValueIfChanged(mGoboInvert, LLSD(gobo.mPatternParams.mV[3] >= 0.5f), true);
        LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode();
        const std::string name = (node && !node->mName.empty()) ? node->mName : std::string("(selected projector)");
        setTextIfChanged(mStatus, "Editing: " + name, true);
    }
    else
    {
        setValueIfChanged(mCastShadows, LLSD(false), true);
        setEnabledIfChanged(mClear, false);
        setValueIfChanged(mGoboPattern, LLSD(-1), force_ctrl);
        setValueIfChanged(mGoboAnim, LLSD(0), force_ctrl);
        setValueIfChanged(mGoboSpeed, LLSD(1.0), force_ctrl);
        setValueIfChanged(mGoboZoom, LLSD(1.0), force_ctrl);
        setValueIfChanged(mGoboDispersion, LLSD(0.0), force_ctrl);
        setTextIfChanged(mStatus, std::string(has_volumetric
            ? "Editing selected VCam projector."
            : "Select a spotlight projector in-world to edit it here."), true);
    }
}

void ALPanelPracticals::onCastShadows()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (object && object->getID().notNull())
    {
        LLPipeline::toggleProjectorCastShadows(object->getID());
    }
}

void ALPanelPracticals::onVolumetric()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (object && object->getID().notNull())
    {
        LLPipeline::toggleVolumetricShaft(object->getID());
    }
}

// Snapshots the global projector-shaft look into this projector, so it keeps it when the global
// sliders move afterwards (same fields as the Lightbox button).
void ALPanelPracticals::onCaptureOverride()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (!object || object->getID().isNull())
    {
        return;
    }
    LLPipeline::VolumetricShaftOverride override_values;
    override_values.multiplier = gSavedSettings.getF32("BDMergeProjectorVolumetricsMultiplier");
    override_values.maxDistance = gSavedSettings.getF32("BDMergeProjectorVolumetricsMaxDistance");
    override_values.feather = gSavedSettings.getF32("BDMergeProjectorVolumetricsFeather");
    override_values.anisotropy = gSavedSettings.getF32("BDMergeProjectorVolumetricsAnisotropy");
    override_values.density = gSavedSettings.getF32("BDMergeProjectorVolumetricsDensity");
    override_values.tint = gSavedSettings.getColor3("BDMergeProjectorVolumetricsTint");
    override_values.tintStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsTintStrength");
    override_values.rimStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimStrength");
    override_values.rimPower = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimPower");
    override_values.rimWrap = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimWrap");
    override_values.rimSoftness = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimSoftness");
    LLPipeline::setVolumetricShaftOverride(object->getID(), override_values);
}

void ALPanelPracticals::onClearOverride()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    if (object && object->getID().notNull())
    {
        LLPipeline::clearVolumetricShaftOverride(object->getID());
        LLPipeline::clearGoboOverride(object->getID());
    }
}

void ALPanelPracticals::onGoboChanged()
{
    LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    if (!volume || !volume->isLightSpotlight() || object->getID().isNull())
    {
        return;
    }
    LLPipeline::GoboOverride gobo;
    gobo.mPattern = mGoboPattern->getValue().asInteger();
    gobo.mAnimMode = mGoboAnim->getValue().asInteger();
    gobo.mSpeed = static_cast<F32>(mGoboSpeed->getValue().asReal());
    gobo.mZoom = static_cast<F32>(mGoboZoom->getValue().asReal());
    gobo.mDispersion = static_cast<F32>(mGoboDispersion->getValue().asReal());
    const LLColor4 tint = mGoboTint->get();
    gobo.mTint = LLColor3(tint.mV[0], tint.mV[1], tint.mV[2]);
    gobo.mPatternParams.mV[0] = static_cast<F32>(mGoboVar1->getValue().asReal());
    gobo.mPatternParams.mV[1] = static_cast<F32>(mGoboVar2->getValue().asReal());
    gobo.mPatternParams.mV[2] = static_cast<F32>(mGoboSoft->getValue().asReal());
    gobo.mPatternParams.mV[3] = mGoboInvert->get() ? 1.f : 0.f;
    LLPipeline::setGoboOverride(object->getID(), gobo);
}

ALPanelPracticals::~ALPanelPracticals()
{
    ALAvatarSubjectPicker::cancelFor(this);
    if (auto* menu = mTrackMenu.get()) delete menu;
}

void ALPanelPracticals::assignTrackingSubject(const LLUUID& projector, const LLUUID& subject, bool has_subject)
{
    auto& tracker = ALProjectorTracker::instance();
    const auto s = tracker.state(projector);
    tracker.configure(projector, LLDirectorCast::canonicalActorId(subject), has_subject,
        s.joint, s.mode, s.smoothing);
    refreshTracking(true);
}

void ALPanelPracticals::onTrackingChanged()
{
    auto& tracker = ALProjectorTracker::instance();
    const auto s = tracker.state(mLastId);
    tracker.configure(mLastId, s.subject, s.hasSubject, mTrackJoint->getValue().asInteger(),
        static_cast<ALProjectorTracker::Mode>(mTrackMode->getValue().asInteger()),
        static_cast<F32>(mTrackSmoothing->getValue().asReal()));
    refreshTracking(true);
}

void ALPanelPracticals::refreshTracking(bool force)
{
    auto& tracker = ALProjectorTracker::instance();
    auto& cast = LLDirectorCast::instance();
    if (!force && mTrackRevision == tracker.revision() && mTrackCastRevision == cast.getCastRevision() &&
        mTrackVCamRevision == LLPrismLens::configurationRevision()) return;
    mTrackRevision = tracker.revision();
    mTrackCastRevision = cast.getCastRevision();
    mTrackVCamRevision = LLPrismLens::configurationRevision();
    auto* object = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    const bool enabled = ALProjectorTracker::eligible(object);
    const auto& s = tracker.state(mLastId);
    for (LLUICtrl* control : {static_cast<LLUICtrl*>(mTrackJoint), static_cast<LLUICtrl*>(mTrackMode),
            static_cast<LLUICtrl*>(mTrackSmoothing), static_cast<LLUICtrl*>(mTrackChoose),
            static_cast<LLUICtrl*>(mTrackSelected), static_cast<LLUICtrl*>(mTrackYou), static_cast<LLUICtrl*>(mTrackClear)})
        setEnabledIfChanged(control, enabled);
    setEnabledIfChanged(mTrack, enabled && s.hasSubject);
    setEnabledIfChanged(mTrackCapture, enabled && s.enabled && s.mode == ALProjectorTracker::FOLLOW);
    setValueIfChanged(mTrack, LLSD(enabled && s.enabled), true);
    setValueIfChanged(mTrackJoint, LLSD(s.joint), force);
    setValueIfChanged(mTrackMode, LLSD(static_cast<S32>(s.mode)), force);
    setValueIfChanged(mTrackSmoothing, LLSD(s.smoothing), force);
    std::string name = "Choose a subject";
    if (s.hasSubject)
    {
        if (LLDirectorCast::isSelf(s.subject)) name = "You";
        else if (auto* member = cast.getMember(s.subject)) name = member->mLastName;
        else if (auto* avatar = cast.resolve(s.subject)) name = avatar->getFullname();
        else name = "Subject unavailable";
    }
    setTextIfChanged(mTrackSubject, name, true);
    setTextIfChanged(mTrackStatus, enabled ? (s.status.empty() ? "Track off" : s.status) : "Select a projector", true);
}

void ALPanelPracticals::onVisibilityChange(bool visible)
{
    if (!visible)
    {
        ALAvatarSubjectPicker::cancelFor(this);
        if (auto* menu = mTrackMenu.get()) menu->hide();
    }
    LLPanel::onVisibilityChange(visible);
}
