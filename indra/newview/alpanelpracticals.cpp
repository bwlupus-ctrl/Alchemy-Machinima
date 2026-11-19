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
#include "alkithistory.h"
#include "alprojectortracker.h"
#include "altoolvirtuallightedit.h"
#include "altoolvirtuallightplace.h"
#include "llagent.h"
#include "llfloaterdirector.h"
#include "llprismlens.h"
#include "lldirectorcast.h"
#include "llinventorymodel.h"
#include "llmenugl.h"
#include "llvoavatar.h"

#include "aluirefresh.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lltoolmgr.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <set>

// The injector name must match class="panel_practicals" in the generated page XML.
static LLPanelInjector<ALPanelPracticals> t_panel_practicals("panel_practicals");

namespace
{
// A Practicals (sl_*) row aimed at a virtual light records ONE custom light-snapshot step; the kit's own widget
// history entry for the same commit would be a second undo step, so it is suppressed while a virtual light is
// the target. A real projector keeps the kit's widget history exactly as before.
bool practicalsWidgetOwnedByVirtual(LLUICtrl* control)
{
    if (!ALVirtualLights::anyPlaced() || !control || control->getName().compare(0, 3, "sl_") != 0) return false;
    ALVirtualLights& lights = ALVirtualLights::instance();
    return lights.volumeFor(lights.focusKey()) != nullptr;
}
}

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
        const Target target = resolveTarget();
        const bool on = mTrack->getValue().asBoolean();
        edited(target, "Virtual light track", mTrack, [&]()
        {
            if (target.mVirtual) ALVirtualLights::instance().enableTrack(target.mKey, on);
            else ALProjectorTracker::instance().enable(target.mId, on);
        });
        refreshTracking(true);
    });
    for (LLUICtrl* control : {static_cast<LLUICtrl*>(mTrackJoint),
            static_cast<LLUICtrl*>(mTrackMode), static_cast<LLUICtrl*>(mTrackSmoothing)})
        control->setCommitCallback([this, control](LLUICtrl*, const LLSD&) { onTrackingChanged(control); });
    mTrackCapture->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        const Target target = resolveTarget();
        edited(target, "Virtual light track capture", mTrackCapture, [&]()
        {
            if (target.mVirtual) ALVirtualLights::instance().captureTrack(target.mKey);
            else ALProjectorTracker::instance().capture(target.mId);
        });
    });
    mTrackChoose->setCommitCallback([this](LLUICtrl* anchor, const LLSD&)
    {
        // The pick is asynchronous: remember which target it was opened for.
        const ALVirtualLightsModel::TrackTargetRef opened_for = trackTargetNow();
        ALAvatarSubjectPicker::choose(anchor, this, mTrackMenu, true,
            [this, opened_for](const LLUUID& id) { assignTrackingSubject(opened_for, id, true); });
    });
    mTrackSelected->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        auto* avatar = ALAvatarSubjectPicker::fromSelection();
        if (ALAvatarSubjectPicker::valid(avatar, true))
        {
            assignTrackingSubject(trackTargetNow(), avatar->getID(), true);
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
                assignTrackingSubject(trackTargetNow(), selected.front(), true);
            break;
        }
    });
    mTrackYou->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { assignTrackingSubject(trackTargetNow(), gAgentID, true); });
    mTrackClear->setCommitCallback([this](LLUICtrl*, const LLSD&)
        { assignTrackingSubject(trackTargetNow(), LLUUID::null, false); });
    mVirtualUI.attach(this);
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
    ALKitHistory::setWidgetSuppressor(&practicalsWidgetOwnedByVirtual);
    for (LLUICtrl* control : { static_cast<LLUICtrl*>(mGoboPattern), static_cast<LLUICtrl*>(mGoboAnim),
                               static_cast<LLUICtrl*>(mGoboSpeed), static_cast<LLUICtrl*>(mGoboZoom),
                               static_cast<LLUICtrl*>(mGoboDispersion), static_cast<LLUICtrl*>(mGoboTint),
                               static_cast<LLUICtrl*>(mGoboVar1), static_cast<LLUICtrl*>(mGoboVar2),
                               static_cast<LLUICtrl*>(mGoboSoft), static_cast<LLUICtrl*>(mGoboInvert) })
    {
        control->setCommitCallback([this, control](LLUICtrl*, const LLSD&) { onGoboChanged(control); });
    }

    refreshFromSelection();
    return LLPanel::postBuild();
}

void ALPanelPracticals::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    refreshFromSelection();
    mVirtualUI.tick();
    LLPanel::draw();
}

// Follows the primary selection: the per-projector controls are enabled only for a spotlight
// projector, and the session-only state (cast-shadows opt-out, volumetric-shaft opt-in, captured
// shaft override, gobo) is mirrored from gPipeline's in-memory sets. Diff-guarded writes only.
ALPanelPracticals::Target ALPanelPracticals::resolveTarget() const
{
    Target target;
    // Feature-off: one integer compare, then exactly the old selection read.
    if (ALVirtualLights::anyPlaced())
    {
        ALVirtualLights& lights = ALVirtualLights::instance();
        if (LLVOVolume* volume = lights.volumeFor(lights.focusKey()))
        {
            target.mObject = volume;
            target.mVolume = volume;
            target.mId = volume->getID();
            target.mKey = lights.focusKey();
            target.mVirtual = true;
            return target;
        }
    }
    target.mObject = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
    target.mVolume = dynamic_cast<LLVOVolume*>(target.mObject);
    target.mId = target.mObject ? target.mObject->getID() : LLUUID::null;
    return target;
}

void ALPanelPracticals::edited(const Target& target, const char* label, LLUICtrl* control,
                               const std::function<void()>& fn)
{
    if (target.mVirtual) mVirtualUI.practicalsEdit(target.mKey, label, control, fn);
    else fn();
}

ALVirtualLightsModel::TrackTargetRef ALPanelPracticals::trackTargetNow() const
{
    const Target target = resolveTarget();
    ALVirtualLightsModel::TrackTargetRef ref;
    ref.mVirtual = target.mVirtual;
    ref.mKey = target.mKey;
    ref.mId = target.mVirtual ? LLUUID::null : target.mId;
    return ref;
}

void ALPanelPracticals::refreshFromSelection()
{
    const Target target = resolveTarget();
    LLViewerObject* object = target.mObject;
    LLVOVolume* volume = target.mVolume;
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
        LLSelectNode* node = target.mVirtual ? nullptr : LLSelectMgr::getInstance()->getSelection()->getFirstRootNode();
        const std::string name = target.mVirtual ? ALVirtualLights::instance().label(target.mKey)
            : ((node && !node->mName.empty()) ? node->mName : std::string("(selected projector)"));
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
        setTextIfChanged(mStatus, target.mVirtual
            ? ALVirtualLights::instance().label(target.mKey) + " is a Point light. Switch it to Spot for shadows, light cone and gobo."
            : std::string(has_volumetric
                ? "Editing selected VCam projector."
                : "Select a spotlight projector in-world, or a virtual light, to edit it here."), true);
    }
}

void ALPanelPracticals::onCastShadows()
{
    const Target target = resolveTarget();
    if (target.mObject && target.mId.notNull())
    {
        edited(target, "Virtual light shadows", mCastShadows,
               [&]() { LLPipeline::toggleProjectorCastShadows(target.mId); });
    }
}

void ALPanelPracticals::onVolumetric()
{
    const Target target = resolveTarget();
    if (target.mObject && target.mId.notNull())
    {
        edited(target, "Virtual light cone", mVolumetric,
               [&]() { LLPipeline::toggleVolumetricShaft(target.mId); });
    }
}

// Snapshots the global projector-shaft look into this projector, so it keeps it when the global
// sliders move afterwards (same fields as the Lightbox button).
void ALPanelPracticals::onCaptureOverride()
{
    const Target target = resolveTarget();
    LLViewerObject* object = target.mObject;
    if (!object || object->getID().isNull())
    {
        return;
    }
    edited(target, "Virtual light shaft", mCapture, [&]()
    {
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
    });
}

void ALPanelPracticals::onClearOverride()
{
    const Target target = resolveTarget();
    if (target.mObject && target.mId.notNull())
    {
        edited(target, "Virtual light overrides", mClear, [&]()
        {
            LLPipeline::clearVolumetricShaftOverride(target.mId);
            LLPipeline::clearGoboOverride(target.mId);
        });
    }
}

void ALPanelPracticals::onGoboChanged(LLUICtrl* source)
{
    const Target target = resolveTarget();
    LLViewerObject* object = target.mObject;
    LLVOVolume* volume = target.mVolume;
    if (!volume || !volume->isLightSpotlight() || object->getID().isNull())
    {
        return;
    }
    edited(target, "Virtual light gobo", source, [&]()
    {
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
    });
}

ALPanelPracticals::~ALPanelPracticals()
{
    ALAvatarSubjectPicker::cancelFor(this);
    if (auto* menu = mTrackMenu.get()) delete menu;
}

ALPanelPracticals::TrackView ALPanelPracticals::trackView(const Target& target) const
{
    TrackView view;
    if (target.mVirtual)
    {
        const ALVirtualLights& lights = ALVirtualLights::instance();
        if (const ALVirtualLights::Light* light = lights.find(target.mKey))
        {
            view.eligible = true;
            view.enabled = light->mTrack.mEnabled;
            view.hasSubject = light->mTrack.mHasSubject;
            view.subject = light->mTrack.mSubject;
            view.joint = light->mTrack.mJoint;
            view.mode = light->mTrack.mMode;
            view.smoothing = light->mTrack.mSmoothing;
            view.status = lights.trackStatus(target.mKey);
        }
        return view;
    }
    const auto& s = ALProjectorTracker::instance().state(target.mId);
    view.eligible = ALProjectorTracker::eligible(target.mObject);
    view.enabled = s.enabled;
    view.hasSubject = s.hasSubject;
    view.subject = s.subject;
    view.joint = s.joint;
    view.mode = static_cast<S32>(s.mode);
    view.smoothing = s.smoothing;
    view.status = s.status;
    return view;
}

void ALPanelPracticals::configureTrackFor(const Target& target, const LLUUID& subject, bool has_subject,
                                          S32 joint, S32 mode, F32 smoothing)
{
    if (target.mVirtual)
    {
        ALVirtualLights::instance().configureTrack(target.mKey, subject, has_subject, joint, mode, smoothing);
        return;
    }
    ALProjectorTracker::instance().configure(target.mId, subject, has_subject, joint,
        static_cast<ALProjectorTracker::Mode>(mode), smoothing);
}

void ALPanelPracticals::assignTrackingSubject(const ALVirtualLightsModel::TrackTargetRef& opened_for,
                                              const LLUUID& subject, bool has_subject)
{
    // Resolve and validate exactly the target the pick was opened for, whatever the page edits by now. A target
    // that is gone means nothing happens; a virtual object id is never a real tracker target.
    const bool placed = ALVirtualLights::anyPlaced();
    const auto virtual_alive = [placed](const LLUUID& key)
        { return placed && ALVirtualLights::instance().volumeFor(key) != nullptr; };
    const auto real_alive = [](const LLUUID& id)
        { LLViewerObject* object = gObjectList.findObject(id); return object && !object->isDead(); };
    const auto is_virtual_id = [placed](const LLUUID& id)
        { return placed && ALVirtualLights::instance().isVirtualObject(id); };
    if (!ALVirtualLightsModel::trackTargetValid(opened_for, virtual_alive, real_alive, is_virtual_id)) return;
    Target target;
    if (opened_for.mVirtual)
    {
        LLVOVolume* volume = ALVirtualLights::instance().volumeFor(opened_for.mKey);
        target.mObject = volume;
        target.mVolume = volume;
        target.mId = volume->getID();
        target.mKey = opened_for.mKey;
        target.mVirtual = true;
    }
    else
    {
        target.mObject = gObjectList.findObject(opened_for.mId);
        target.mVolume = dynamic_cast<LLVOVolume*>(target.mObject);
        target.mId = opened_for.mId;
    }
    const TrackView s = trackView(target);
    if (!s.eligible) return;
    edited(target, "Virtual light track subject", mTrack, [&]()
    {
        configureTrackFor(target, LLDirectorCast::canonicalActorId(subject), has_subject,
            s.joint, s.mode, s.smoothing);
    });
    refreshTracking(true);
}

void ALPanelPracticals::onTrackingChanged(LLUICtrl* source)
{
    const Target target = resolveTarget();
    const TrackView s = trackView(target);
    edited(target, "Virtual light track", source, [&]()
    {
        configureTrackFor(target, s.subject, s.hasSubject, mTrackJoint->getValue().asInteger(),
            mTrackMode->getValue().asInteger(),
            static_cast<F32>(mTrackSmoothing->getValue().asReal()));
    });
    refreshTracking(true);
}

void ALPanelPracticals::refreshTracking(bool force)
{
    auto& tracker = ALProjectorTracker::instance();
    auto& cast = LLDirectorCast::instance();
    const U64 virtual_revision = ALVirtualLights::anyPlaced() ? ALVirtualLights::instance().revision() : 0;
    if (!force && mTrackRevision == tracker.revision() && mTrackCastRevision == cast.getCastRevision() &&
        mTrackVCamRevision == LLPrismLens::configurationRevision() &&
        mTrackVirtualRevision == virtual_revision) return;
    mTrackRevision = tracker.revision();
    mTrackCastRevision = cast.getCastRevision();
    mTrackVCamRevision = LLPrismLens::configurationRevision();
    mTrackVirtualRevision = virtual_revision;
    const Target target = resolveTarget();
    const TrackView s = trackView(target);
    const bool enabled = s.eligible;
    for (LLUICtrl* control : {static_cast<LLUICtrl*>(mTrackJoint), static_cast<LLUICtrl*>(mTrackMode),
            static_cast<LLUICtrl*>(mTrackSmoothing), static_cast<LLUICtrl*>(mTrackChoose),
            static_cast<LLUICtrl*>(mTrackSelected), static_cast<LLUICtrl*>(mTrackYou), static_cast<LLUICtrl*>(mTrackClear)})
        setEnabledIfChanged(control, enabled);
    setEnabledIfChanged(mTrack, enabled && s.hasSubject);
    setEnabledIfChanged(mTrackCapture, enabled && s.enabled && s.mode == static_cast<S32>(ALProjectorTracker::FOLLOW));
    setValueIfChanged(mTrack, LLSD(enabled && s.enabled), true);
    setValueIfChanged(mTrackJoint, LLSD(s.joint), force);
    setValueIfChanged(mTrackMode, LLSD(s.mode), force);
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
    if (visible) mVirtualUI.onReveal();
    if (!visible)
    {
        ALAvatarSubjectPicker::cancelFor(this);
        if (auto* menu = mTrackMenu.get()) menu->hide();
        mVirtualUI.onHide();
    }
    LLPanel::onVisibilityChange(visible);
}

// ===========================================================================================
// ALVirtualLightsUI: the "Virtual lights" card of this page. The model and every edit rule live
// in ALVirtualLights; this class only mirrors it into the widgets and turns commits into edits,
// each one undoable on the Practicals page (a burst of edits is one step).
// ===========================================================================================
namespace
{
std::set<ALVirtualLightsUI*> sVirtualEditors;
constexpr F64 kEditCoalesceSeconds = 0.5;

bool sliderHeld(LLUICtrl* control)
{
    auto* slider = dynamic_cast<LLSliderCtrl*>(control);
    return slider && (slider->isMouseHeldDown() || slider->isWheelSettlePending());
}

// How a control's edits form gestures. DISCRETE (checkbox, combo, button): every commit is its own step, closed
// at once. SLIDER: one step from the first drag / wheel tick until the mouse-up / wheel settle. TIMED (colour
// picker, spinner, whose gesture end is not reported): same-control edits within the burst window are one step.
enum class GestureKind { DISCRETE, TIMED, SLIDER };

GestureKind gestureKind(LLUICtrl* control)
{
    if (dynamic_cast<LLSliderCtrl*>(control)) return GestureKind::SLIDER;
    if (dynamic_cast<LLColorSwatchCtrl*>(control) || dynamic_cast<LLSpinCtrl*>(control)) return GestureKind::TIMED;
    return GestureKind::DISCRETE;
}
}

ALVirtualLightsUI::~ALVirtualLightsUI()
{
    // A pick armed from this card must not call back into a destroyed page.
    if (ALToolVirtualLightPlace::instanceExists()) ALToolVirtualLightPlace::getInstance()->cancel();
    // Teardown DISCARDS a pending edit: nothing is recorded into undo while the page is being destroyed
    // (the visibility hook already flushed it when the page was hidden).
    mPendingBefore = LLSD();
    mPendingControl = nullptr;
    ALVirtualLights::releaseEditor();
    sVirtualEditors.erase(this);
}

void ALVirtualLightsUI::onHide()
{
    finishPending(true);
    if (ALToolVirtualLightPlace::instanceExists()) ALToolVirtualLightPlace::getInstance()->cancel();
    ALVirtualLights::releaseEditor();
}

void ALVirtualLightsUI::finishAllPending()
{
    for (ALVirtualLightsUI* editor : sVirtualEditors) editor->finishPending(true);
}

void ALVirtualLightsUI::attach(LLPanel* root)
{
    mRoot = root;
    mPlace = root->findChild<LLButton>("vl_place");
    mDelete = root->findChild<LLButton>("vl_delete");
    mAimPick = root->findChild<LLButton>("vl_aim_pick");
    mEditToggle = root->findChild<LLCheckBoxCtrl>("vl_edit");
    mOn = root->findChild<LLCheckBoxCtrl>("vl_on");
    mList = root->findChild<LLScrollListCtrl>("vl_list");
    mStatus = root->findChild<LLTextBox>("vl_status");
    mType = root->findChild<LLComboBox>("vl_type");
    mVCam = root->findChild<LLComboBox>("vl_vcam");
    mTexture = root->findChild<LLButton>("vl_texture");
    mTextureName = root->findChild<LLTextBox>("vl_texture_name");
    mColor = root->findChild<LLColorSwatchCtrl>("vl_color");
    mIntensity = root->findChild<LLSliderCtrl>("vl_intensity");
    mRadius = root->findChild<LLSliderCtrl>("vl_radius");
    mFalloff = root->findChild<LLSliderCtrl>("vl_falloff");
    mFov = root->findChild<LLSliderCtrl>("vl_fov");
    mFocus = root->findChild<LLSliderCtrl>("vl_focus");
    mAmbiance = root->findChild<LLSliderCtrl>("vl_ambiance");
    mX = root->findChild<LLSpinCtrl>("vl_x");
    mY = root->findChild<LLSpinCtrl>("vl_y");
    mZ = root->findChild<LLSpinCtrl>("vl_z");
    mYaw = root->findChild<LLSpinCtrl>("vl_yaw");
    mPitch = root->findChild<LLSpinCtrl>("vl_pitch");
    if (!mPlace || !mDelete || !mAimPick || !mEditToggle || !mOn || !mList || !mStatus || !mType || !mVCam ||
        !mColor || !mIntensity || !mRadius || !mFalloff || !mFov || !mFocus || !mAmbiance || !mX || !mY ||
        !mZ || !mYaw || !mPitch || !mTexture || !mTextureName)
    {
        LL_WARNS_ONCE("VirtualLight") << "Virtual lights card is missing required widgets" << LL_ENDL;
        mList = nullptr;
        return;
    }
    sVirtualEditors.insert(this);
    ALVirtualLights::setPendingFlusher([]() { ALVirtualLightsUI::finishAllPending(); });

    mPlace->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlace(); });
    mDelete->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDelete(); });
    mAimPick->setCommitCallback([this](LLUICtrl*, const LLSD&) { onAimPick(); });
    mEditToggle->setCommitCallback([this](LLUICtrl*, const LLSD&) { onEditToggle(); });
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        const LLSD value = mList->getSelectedValue();
        if (value.isUndefined()) return;
        finishPending(true);
        ALVirtualLights::instance().setFocus(value.asUUID());
        mForce = true;
    });
    mOn->setCommitCallback([this](LLUICtrl* ctrl, const LLSD&)
    {
        const bool on = mOn->getValue().asBoolean();
        edit("Virtual light on/off", ctrl, [on](ALVirtualLights::Light& light) { light.mOn = on; });
    });
    mType->setCommitCallback([this](LLUICtrl* ctrl, const LLSD&)
    {
        const bool spot = mType->getValue().asInteger() == 1;
        edit("Virtual light type", ctrl, [spot](ALVirtualLights::Light& light) { light.mSpot = spot; });
    });
    mColor->setCommitCallback([this](LLUICtrl* ctrl, const LLSD&)
    {
        const LLColor4 color = mColor->get();
        edit("Virtual light colour", ctrl, [color](ALVirtualLights::Light& light)
        {
            for (S32 i = 0; i < 3; ++i) light.mColor[i] = color.mV[i];
        });
    });
    struct SliderBinding { LLSliderCtrl* mCtrl; const char* mLabel; F32 ALVirtualLights::Light::* mField; };
    for (const SliderBinding& binding : {
            SliderBinding{mIntensity, "Virtual light intensity", &ALVirtualLights::Light::mIntensity},
            SliderBinding{mRadius, "Virtual light radius", &ALVirtualLights::Light::mRadius},
            SliderBinding{mFalloff, "Virtual light falloff", &ALVirtualLights::Light::mFalloff},
            SliderBinding{mFov, "Virtual light field of view", &ALVirtualLights::Light::mFov},
            SliderBinding{mFocus, "Virtual light focus", &ALVirtualLights::Light::mFocus},
            SliderBinding{mAmbiance, "Virtual light ambiance", &ALVirtualLights::Light::mAmbiance}})
    {
        LLSliderCtrl* ctrl = binding.mCtrl;
        const char* label = binding.mLabel;
        auto field = binding.mField;
        ctrl->setCommitCallback([this, ctrl, label, field](LLUICtrl*, const LLSD&)
        {
            const F32 value = static_cast<F32>(ctrl->getValue().asReal());
            edit(label, ctrl, [value, field](ALVirtualLights::Light& light) { light.*field = value; });
        });
    }
    struct SpinBinding { LLSpinCtrl* mCtrl; const char* mLabel; S32 mAxis; };   // 0..2 position, 3 yaw, 4 pitch
    for (const SpinBinding& binding : {
            SpinBinding{mX, "Move virtual light", 0}, SpinBinding{mY, "Move virtual light", 1},
            SpinBinding{mZ, "Move virtual light", 2}, SpinBinding{mYaw, "Aim virtual light", 3},
            SpinBinding{mPitch, "Aim virtual light", 4}})
    {
        LLSpinCtrl* ctrl = binding.mCtrl;
        const char* label = binding.mLabel;
        const S32 axis = binding.mAxis;
        ctrl->setCommitCallback([this, ctrl, label, axis](LLUICtrl*, const LLSD&)
        {
            const F32 value = static_cast<F32>(ctrl->getValue().asReal());
            edit(label, ctrl, [value, axis](ALVirtualLights::Light& light)
            {
                if (axis < 3) light.mPos[axis] = value;
                else
                {
                    // A hand-typed aim replaces "aim at an object".
                    light.mAimObject.setNull();
                    (axis == 3 ? light.mYawDeg : light.mPitchDeg) = value;
                }
            });
        });
    }
    mVCam->setCommitCallback([this](LLUICtrl*, const LLSD&) { onVCam(); });
    // The standard viewer texture picker (the one Build > Features uses for the projector texture), driven by
    // a hidden texture control so no new kit row kind is needed.
    LLTextureCtrl::Params picker;
    picker.name("vl_texture_picker");
    picker.rect(LLRect(0, 64, 64, 0));
    picker.visible(false);
    picker.allow_no_texture(true);
    mTexturePicker = LLUICtrlFactory::create<LLTextureCtrl>(picker);
    if (mTexturePicker)
    {
        root->addChild(mTexturePicker);
        mTexturePicker->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTexturePicked(); });
    }
    mTexture->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTexturePick(); });
    refresh(true);
}

void ALVirtualLightsUI::setMessage(const std::string& text)
{
    mMessage = text;
    mMessageTime = LLTimer::getElapsedSeconds();
    mForce = true;
}

void ALVirtualLightsUI::tick()
{
    if (!mList) return;
    finishPending(false);
    if (!isWidgetOnScreen(mRoot)) return;
    if (ALVirtualLights::anyPlaced()) ALVirtualLights::instance().noteEditorVisible();
    if (mForce || mClock.due()) refresh(mForce);
}

void ALVirtualLightsUI::finishPending(bool force)
{
    if (mPendingBefore.isUndefined()) return;
    // Real gesture boundaries: a slider ends at mouse-up / wheel settle, a spinner or colour picker after its
    // burst window; a discrete control never reaches here pending (it closes at once).
    if (!force && (sliderHeld(mPendingControl) ||
                   (gestureKind(mPendingControl) == GestureKind::TIMED &&
                    LLTimer::getElapsedSeconds() - mLastEdit < kEditCoalesceSeconds))) return;
    const LLSD before = mPendingBefore;
    const std::string label = mPendingLabel;
    const U64 epoch = mPendingEpoch;
    mPendingBefore = LLSD();
    mPendingControl = nullptr;
    if (ALVirtualLights::instance().sceneEpoch() != epoch) return;   // the lights were cleared meanwhile
    ALVirtualLights::recordUndo(before, label);
}

void ALVirtualLightsUI::notePending(const LLSD& before, const char* label, LLUICtrl* control)
{
    if (mPendingBefore.isUndefined())
    {
        mPendingBefore = before;
        mPendingLabel = label;
        mPendingEpoch = ALVirtualLights::instance().sceneEpoch();
    }
    mPendingControl = control;
    mLastEdit = LLTimer::getElapsedSeconds();
    if (gestureKind(control) == GestureKind::DISCRETE) finishPending(true);
}

LLSD ALVirtualLightsUI::beginEdit(LLUICtrl* control)
{
    ALVirtualLights& lights = ALVirtualLights::instance();
    if (!mPendingBefore.isUndefined())
    {
        const GestureKind kind = gestureKind(control);
        const bool same = control && control == mPendingControl && lights.sceneEpoch() == mPendingEpoch;
        const bool continues = same && ((kind == GestureKind::SLIDER && sliderHeld(control)) ||
            (kind == GestureKind::TIMED && LLTimer::getElapsedSeconds() - mLastEdit < kEditCoalesceSeconds));
        if (continues) return LLSD();
        finishPending(true);   // a different control or a new gesture: its own undo step
    }
    return lights.capture();
}

void ALVirtualLightsUI::editKey(const LLUUID& key, const char* label, LLUICtrl* control,
                                const std::function<void(ALVirtualLights::Light&)>& change)
{
    ALVirtualLights& lights = ALVirtualLights::instance();
    if (key.isNull() || !lights.find(key)) return;
    const LLSD before = beginEdit(control);
    if (lights.edit(key, change)) notePending(before, label, control);
}

void ALVirtualLightsUI::edit(const char* label, LLUICtrl* control,
                             const std::function<void(ALVirtualLights::Light&)>& change)
{
    editKey(ALVirtualLights::instance().focusKey(), label, control, change);
}

void ALVirtualLightsUI::practicalsEdit(const LLUUID& key, const char* label, LLUICtrl* control,
                                       const std::function<void()>& fn)
{
    ALVirtualLights& lights = ALVirtualLights::instance();
    const LLSD before = key.notNull() ? beginEdit(control) : LLSD();
    fn();
    if (key.isNull()) return;
    lights.notePracticalsEdit(key);
    notePending(before, label, control);
}

void ALVirtualLightsUI::onTexturePick()
{
    ALVirtualLights& lights = ALVirtualLights::instance();
    const ALVirtualLights::Light* light = lights.find(lights.focusKey());
    if (!light || !mTexturePicker) return;
    finishPending(true);
    mTextureKey = light->mKey;
    mTexturePicker->setImageAssetID(light->mTexture);
    mTexturePicker->showPicker(true);
}

void ALVirtualLightsUI::onTexturePicked()
{
    if (!mTexturePicker) return;
    const LLUUID id = mTexturePicker->getImageAssetID();
    if (const LLInventoryItem* item = gInventory.getItem(mTexturePicker->getImageItemID()))
    {
        mNamedTexture = id;
        mNamedTextureName = item->getName();
    }
    // The light the picker was opened for, not whatever has the focus now.
    editKey(mTextureKey, "Virtual light projector texture", mTexture,
            [id](ALVirtualLights::Light& light) { light.mTexture = id; });
    mForce = true;
}

void ALVirtualLightsUI::onPlace()
{
    finishPending(true);
    setMessage("Click a spot in the world to place the light. Esc cancels.");
    ALToolVirtualLightPlace::getInstance()->armPlace([this](bool ok, const std::string& text)
    {
        setMessage(ok ? "Placed " + text + "." : (text == "Cancelled" ? std::string() : text));
        mForce = true;
    });
}

void ALVirtualLightsUI::onAimPick()
{
    finishPending(true);
    const LLUUID key = ALVirtualLights::instance().focusKey();
    setMessage("Click an object to follow it, or a spot to aim there. Esc cancels.");
    ALToolVirtualLightPlace::getInstance()->armAim(key, [this](bool ok, const std::string& text)
    {
        setMessage(ok ? text + "." : (text == "Cancelled" ? std::string() : text));
        mForce = true;
    });
}

void ALVirtualLightsUI::onDelete()
{
    finishPending(true);
    ALVirtualLights& lights = ALVirtualLights::instance();
    const LLUUID key = lights.focusKey();
    if (key.isNull()) return;
    const LLSD before = lights.capture();
    if (lights.remove(key)) ALVirtualLights::recordUndo(before, "Delete virtual light");
    mForce = true;
}

void ALVirtualLightsUI::onEditToggle()
{
    LLToolMgr* tools = LLToolMgr::getInstance();
    ALToolVirtualLightEdit* tool = ALToolVirtualLightEdit::getInstance();
    if (mEditToggle->getValue().asBoolean() && ALVirtualLights::instance().count() > 0)
        tools->setTransientTool(tool);
    else if (tools->getCurrentTool() == tool)
        tools->clearTransientTool();
    mForce = true;
}

void ALVirtualLightsUI::onVCam()
{
    finishPending(true);
    ALVirtualLights& lights = ALVirtualLights::instance();
    const LLUUID key = lights.focusKey();
    if (key.isNull()) return;
    const LLSD before = lights.capture();
    std::string reason;
    if (lights.setVCam(key, mVCam->getValue().asUUID(), &reason))
        ALVirtualLights::recordUndo(before, "Virtual light VCam feed");
    else
        setMessage(reason);
    mForce = true;
}

void ALVirtualLightsUI::refresh(bool force)
{
    ALVirtualLights& lights = ALVirtualLights::instance();
    mForce = false;
    const LLUUID focus = lights.focusKey();
    const ALVirtualLights::Light* light = lights.find(focus);
    const bool has = light != nullptr;
    const bool spot = has && light->mSpot;
    const bool focus_changed = focus != mShownFocus;
    mShownFocus = focus;
    const bool force_ctrl = force || focus_changed;

    // The list is rebuilt only when its shape or a row's state changed (never for a position drag).
    if (force || mRevision != lights.listRevision() || focus_changed)
    {
        mRevision = lights.listRevision();
        mList->deleteAllItems(); // ui-draw-lint: allow(raw-write) at most eight rows, only on listRevision change, throttled to 10 Hz
        for (const LLUUID& key : lights.keys())
        {
            const ALVirtualLights::Light* row_light = lights.find(key);
            if (!row_light) continue;
            LLSD row;
            row["value"] = key;
            row["columns"][0]["column"] = "vl_light_col";
            row["columns"][0]["value"] = lights.label(key);
            row["columns"][1]["column"] = "vl_state_col";
            row["columns"][1]["value"] = row_light->mOn ? "On" : "Off";
            row["columns"][2]["column"] = "vl_type_col";
            row["columns"][2]["value"] = row_light->mSpot ? "Spot" : "Point";
            mList->addElement(row, ADD_BOTTOM); // ui-draw-lint: allow(raw-write) same bounded revision-only list refresh
        }
        mList->setSelectedByValue(LLSD(focus), true);
    }
    // VCam feeds: refilled only when the Prism registry changed.
    const bool prism_changed = force || mPrismRevision != LLPrismLens::configurationRevision();
    if (prism_changed)
    {
        mPrismRevision = LLPrismLens::configurationRevision();
        mVCam->clearRows(); // ui-draw-lint: allow(raw-write) a handful of feeds, only on Prism configuration change
        mVCam->add("Off", LLUUID::null, ADD_BOTTOM); // ui-draw-lint: allow(raw-write) same bounded refill
        if (LLPrismLens::instanceExists())
        {
            const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
            for (U32 index = 0; index < snapshot.mSlotCount; ++index)
            {
                const LLPrismLens::CaptureDefinition& capture = snapshot.mCaptures[index];
                if (!capture.isGate() && !capture.isCameraFeed()) continue;
                mVCam->add(capture.isGate() ? std::string("Gate") : llformat("Cam %u", capture.mSlot + 1),
                           capture.mHandle.mId, ADD_BOTTOM); // ui-draw-lint: allow(raw-write) same bounded refill
            }
        }
    }

    // A tool message (placed, refused, picking) shows for a few seconds, then the summary returns.
    if (!mMessage.empty() && LLTimer::getElapsedSeconds() - mMessageTime > 6.0) mMessage.clear();
    std::string status = mMessage;
    if (status.empty())
    {
        status = lights.count() == 0 ? std::string("No virtual lights. Place one, then click a spot in the world.")
            : llformat("%d of %d virtual lights. ", lights.count(), ALVirtualLightsModel::kMaxLights) +
              (has ? "Editing " + lights.label(focus) + "." : std::string("Select one in the list."));
    }
    setTextIfChanged(mStatus, status, true);

    setEnabledIfChanged(mPlace, lights.count() < ALVirtualLightsModel::kMaxLights);
    for (LLUICtrl* control : {static_cast<LLUICtrl*>(mDelete), static_cast<LLUICtrl*>(mOn),
            static_cast<LLUICtrl*>(mType), static_cast<LLUICtrl*>(mColor), static_cast<LLUICtrl*>(mIntensity),
            static_cast<LLUICtrl*>(mRadius), static_cast<LLUICtrl*>(mFalloff), static_cast<LLUICtrl*>(mX),
            static_cast<LLUICtrl*>(mY), static_cast<LLUICtrl*>(mZ), static_cast<LLUICtrl*>(mVCam)})
        setEnabledIfChanged(control, has);
    for (LLUICtrl* control : {static_cast<LLUICtrl*>(mFov), static_cast<LLUICtrl*>(mFocus),
            static_cast<LLUICtrl*>(mAmbiance), static_cast<LLUICtrl*>(mYaw), static_cast<LLUICtrl*>(mPitch),
            static_cast<LLUICtrl*>(mAimPick)})
        setEnabledIfChanged(control, spot);
    setEnabledIfChanged(mTexture, spot);
    setEnabledIfChanged(mEditToggle, lights.count() > 0);
    setValueIfChanged(mEditToggle, LLSD(LLToolMgr::getInstance()->getCurrentTool() ==
        static_cast<LLTool*>(ALToolVirtualLightEdit::getInstance())), true);

    if (!has) return;
    setValueIfChanged(mOn, LLSD(light->mOn), force_ctrl);
    setValueIfChanged(mType, LLSD(light->mSpot ? 1 : 0), force_ctrl);
    const LLColor4 color(light->mColor[0], light->mColor[1], light->mColor[2], 1.f);
    if (mColor->get() != color) mColor->set(color, TRUE);
    setValueIfChanged(mIntensity, LLSD(static_cast<F64>(light->mIntensity)), force_ctrl);
    setValueIfChanged(mRadius, LLSD(static_cast<F64>(light->mRadius)), force_ctrl);
    setValueIfChanged(mFalloff, LLSD(static_cast<F64>(light->mFalloff)), force_ctrl);
    setValueIfChanged(mFov, LLSD(static_cast<F64>(light->mFov)), force_ctrl);
    setValueIfChanged(mFocus, LLSD(static_cast<F64>(light->mFocus)), force_ctrl);
    setValueIfChanged(mAmbiance, LLSD(static_cast<F64>(light->mAmbiance)), force_ctrl);
    setValueIfChanged(mX, LLSD(static_cast<F64>(light->mPos[0])), force_ctrl);
    setValueIfChanged(mY, LLSD(static_cast<F64>(light->mPos[1])), force_ctrl);
    setValueIfChanged(mZ, LLSD(static_cast<F64>(light->mPos[2])), force_ctrl);
    setValueIfChanged(mYaw, LLSD(static_cast<F64>(light->mYawDeg)), force_ctrl);
    setValueIfChanged(mPitch, LLSD(static_cast<F64>(light->mPitchDeg)), force_ctrl);
    std::string texture_text = "Default";
    if (light->mTexture.notNull())
        texture_text = light->mTexture == mNamedTexture && !mNamedTextureName.empty()
            ? mNamedTextureName : "Custom texture " + light->mTexture.asString().substr(0, 8);
    setTextIfChanged(mTextureName, spot ? texture_text : std::string("Spot only"), true);
    // registrySnapshot() copies the whole registry: read the feed only when something could have changed.
    if (force_ctrl || prism_changed) setValueIfChanged(mVCam, LLSD(lights.vcamOf(focus)), true);
}
