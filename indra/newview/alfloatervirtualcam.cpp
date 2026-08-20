/**
 * @file alfloatervirtualcam.cpp
 * @brief Standalone host for the Virtual Cam controls.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alfloatervirtualcam.h"

#include "aldirectorswitcher.h"
#include "llbutton.h"
#include "llcinematiccamera.h"
#include "lldirectorcast.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llprismlens.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"

#include <set>

namespace
{
void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}
}

ALFloaterVirtualCam::ALFloaterVirtualCam(const LLSD& key)
    : LLFloater(key)
{
}

bool ALFloaterVirtualCam::postBuild()
{
    mPrismSummaryText = getChild<LLTextBox>("prism_summary");
    mPrismManageBtn = getChild<LLButton>("btn_prism_manage");
    mOtsPairBtn = getChild<LLButton>("btn_build_ots_pair");
    mZollyBtn = getChild<LLButton>("btn_start_zolly");
    mHeroArcBtn = getChild<LLButton>("btn_start_hero_arc");

    mPrismManageBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickManagePrism(); });
    mOtsPairBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickBuildOtsPair(); });
    mZollyBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickZolly(); });
    mHeroArcBtn->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onClickHeroArc(); });

    return LLFloater::postBuild();
}

void ALFloaterVirtualCam::draw()
{
    refreshControls();
    LLFloater::draw();
}

void ALFloaterVirtualCam::onClickManagePrism()
{
    LLFloaterReg::showInstance("prism_manager");
}

void ALFloaterVirtualCam::onClickBuildOtsPair()
{
    std::string reason;
    if (!LLPrismLens::buildOtsPairFromSubjects(nullptr, nullptr, &reason))
    {
        LLNotificationsUtil::add(
            "GenericAlert",
            LLSD().with("MESSAGE", reason.empty()
                ? "Unable to build the OTS camera pair." : reason));
        return;
    }
    mHavePrismSummary = false;
}

void ALFloaterVirtualCam::onClickZolly()
{
    const F32 first = gSavedSettings.getF32("CinematicCamVertigoStartDist");
    const F32 second = gSavedSettings.getF32("CinematicCamVertigoEndDist");
    const F32 near_distance = llmin(first, second);
    const F32 far_distance = llmax(first, second);
    const bool dolly_in =
        gSavedSettings.getS32("CinematicCamVertigoDirection") != 0;
    gSavedSettings.setF32("CinematicCamVertigoStartDist",
                          dolly_in ? far_distance : near_distance);
    gSavedSettings.setF32("CinematicCamVertigoEndDist",
                          dolly_in ? near_distance : far_distance);
    gSavedSettings.setS32("CinematicCamVertigoEndMode", 0);
    LLCinematicCamera::instance().triggerMode(
        LLCinematicCamera::MODE_DOLLY_ZOOM, true);
}

void ALFloaterVirtualCam::onClickHeroArc()
{
    const F32 angle = llclamp(
        gSavedSettings.getF32("CinematicCamArcGeneratorAngle"), 30.f, 180.f);
    gSavedSettings.setF32("CinematicCamArcFrom", -0.5f * angle);
    gSavedSettings.setF32("CinematicCamArcTo", 0.5f * angle);
    gSavedSettings.setS32("CinematicCamArcEndMode", 0);
    LLCinematicCamera::instance().triggerMode(
        LLCinematicCamera::MODE_ARC, true);
}

void ALFloaterVirtualCam::refreshControls()
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    LLVOAvatar* live_a = cast.resolveSubjectA();
    LLVOAvatar* live_b = cast.resolveSubjectB();
    const bool distinct_live_pair = live_a && live_b &&
        live_a->getID() != live_b->getID();
    const LLPrismLens::RegistrySnapshot ots_registry =
        LLPrismLens::registrySnapshot();
    const LLPrismLens::GateSnapshot ots_gate = LLPrismLens::gateSnapshot();
    std::set<LLUUID> replaceable_ots_captures;
    std::size_t replaceable_ots_arms = 0;
    for (const LLPrismLens::GateArmedCamera& armed :
         ots_gate.mSettings.mArmed)
    {
        if (LLPrismLens::isOtsPairArmLabel(armed.mLabel))
        {
            ++replaceable_ots_arms;
            replaceable_ots_captures.insert(armed.mCaptureId);
        }
    }
    U32 replaceable_ots_capture_count = 0;
    for (U32 index = 0; index < ots_registry.mCaptureCount; ++index)
    {
        if (replaceable_ots_captures.count(
                ots_registry.mCaptures[index].mHandle.mId) != 0)
        {
            ++replaceable_ots_capture_count;
        }
    }
    const bool ots_capacity =
        ots_registry.mCaptureCount - replaceable_ots_capture_count <=
            LLPrismLens::MAX_CAPTURES - 2 &&
        ots_gate.mSettings.mArmed.size() - replaceable_ots_arms <=
            LLPrismLens::MAX_CAPTURES - 2;
    mOtsPairBtn->setEnabled(distinct_live_pair && ots_capacity);
    setToolTipIfChanged(mOtsPairBtn,
        !live_a || !live_b
            ? std::string("Set live Director Subjects A and B first")
            : !distinct_live_pair
                ? std::string("Subjects A and B must be different avatars")
                : !ots_capacity
                    ? std::string("Two free VCam capture and Gate arm slots are required")
                    : std::string("Create reciprocal matched-lens OTS A/B virtual cameras and arm both in the Gate"));

    const bool switcher_driving = ALDirectorSwitcher::instance().isDrivingCamera();
    const bool can_trigger_subject_shot = live_a && !switcher_driving;
    mZollyBtn->setEnabled(can_trigger_subject_shot);
    mHeroArcBtn->setEnabled(can_trigger_subject_shot);
    const std::string subject_shot_tip = !live_a
        ? std::string("Set a live Director Subject A first")
        : switcher_driving
            ? std::string("Stop the Director Switcher before starting a manual shot")
            : std::string("Start this one-shot move locked to Subject A's head");
    setToolTipIfChanged(mZollyBtn, subject_shot_tip);
    setToolTipIfChanged(mHeroArcBtn, subject_shot_tip);

    const U64 prism_configuration_revision = LLPrismLens::configurationRevision();
    const U64 prism_runtime_revision = LLPrismLens::runtimeRevision();
    if (!mHavePrismSummary ||
        prism_configuration_revision != mPrismConfigurationRevision ||
        prism_runtime_revision != mPrismRuntimeRevision)
    {
        const LLPrismLens::RegistrySnapshot snapshot = LLPrismLens::registrySnapshot();
        U32 capture_attention = 0;
        U32 display_attention = 0;
        U32 updating = 0;
        for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
        {
            const LLPrismLens::CaptureDefinition& capture = snapshot.mCaptures[index];
            capture_attention += capture.mRuntime.mHealth != LLPrismLens::ECaptureHealth::READY;
            updating += capture.mRuntime.mActivity == LLPrismLens::EActivityState::LIVE ||
                        capture.mRuntime.mActivity == LLPrismLens::EActivityState::THROTTLED;
        }
        for (U32 index = 0; index < snapshot.mDisplayCount; ++index)
        {
            display_attention += snapshot.mDisplays[index].mRuntime.mHealth !=
                LLPrismLens::EDisplayHealth::READY;
        }
        const bool needs_attention = capture_attention != 0 || display_attention != 0;

        mPrismSummaryText->setText(llformat(
            "Prism %u/%u captures | %u/%u faces | %u live%s",
            snapshot.mCaptureCount, LLPrismLens::MAX_CAPTURES,
            snapshot.mDisplayCount, LLPrismLens::MAX_DISPLAY_BINDINGS,
            updating, needs_attention ? " | attention" : ""));
        setToolTipIfChanged(mPrismSummaryText, llformat(
            "%u captures and %u display bindings; attention: %u capture%s, %u display%s. Open Prism Manager for sources, faces, optics, picture FPS, and adaptive performance.",
            snapshot.mCaptureCount, snapshot.mDisplayCount,
            capture_attention, capture_attention == 1 ? "" : "s",
            display_attention, display_attention == 1 ? "" : "s"));
        mPrismConfigurationRevision = prism_configuration_revision;
        mPrismRuntimeRevision = prism_runtime_revision;
        mHavePrismSummary = true;
    }
}
