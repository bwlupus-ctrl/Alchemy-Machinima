/**
 * @file llfloaterprismmanager.cpp
 * @brief Editor for shared Prism captures and their display bindings.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterprismmanager.h"

#include "alscrollfocus.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llnotificationsutil.h"
#include "llpanel.h"
#include "llscrollcontainer.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"

namespace
{
constexpr F32 SNAPSHOT_POLL_SECONDS = 0.25f;
constexpr F32 AGE_REFRESH_SECONDS = 1.f;

bool sameHandle(const LLPrismLens::CaptureHandle& left,
                const LLPrismLens::CaptureHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

bool sameHandle(const LLPrismLens::DisplayHandle& left,
                const LLPrismLens::DisplayHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

std::string captureModeText(LLPrismLens::ECaptureMode mode)
{
    switch (mode)
    {
        case LLPrismLens::ECaptureMode::SURFACE_LENS: return "Lens";
        case LLPrismLens::ECaptureMode::CAMERA_FEED: return "Camera";
    }
    return "Unknown";
}

std::string captureHealthText(LLPrismLens::ECaptureHealth health)
{
    switch (health)
    {
        case LLPrismLens::ECaptureHealth::READY: return "READY";
        case LLPrismLens::ECaptureHealth::UNBOUND_SOURCE: return "UNBOUND";
        case LLPrismLens::ECaptureHealth::SOURCE_OFFLINE: return "SOURCE OFFLINE";
        case LLPrismLens::ECaptureHealth::INVALID_SOURCE: return "INVALID SOURCE";
        case LLPrismLens::ECaptureHealth::LENS_SURFACE_OFFLINE: return "LENS OFFLINE";
        case LLPrismLens::ECaptureHealth::INVALID_LENS_SURFACE: return "INVALID LENS";
    }
    return "UNKNOWN";
}

std::string outputStateText(LLPrismLens::EOutputState state)
{
    switch (state)
    {
        case LLPrismLens::EOutputState::EMPTY: return "EMPTY";
        case LLPrismLens::EOutputState::CURRENT: return "CURRENT";
        case LLPrismLens::EOutputState::HELD: return "HELD";
        case LLPrismLens::EOutputState::SUPPRESSED: return "SUPPRESSED";
    }
    return "UNKNOWN";
}

std::string activityStateText(LLPrismLens::EActivityState state)
{
    switch (state)
    {
        case LLPrismLens::EActivityState::IDLE: return "IDLE";
        case LLPrismLens::EActivityState::WAITING: return "WAITING";
        case LLPrismLens::EActivityState::LIVE: return "LIVE";
        case LLPrismLens::EActivityState::PAUSED: return "PAUSED";
        case LLPrismLens::EActivityState::THROTTLED: return "THROTTLED";
    }
    return "UNKNOWN";
}

std::string displayHealthText(LLPrismLens::EDisplayHealth health)
{
    switch (health)
    {
        case LLPrismLens::EDisplayHealth::READY: return "READY";
        case LLPrismLens::EDisplayHealth::OFFLINE: return "OFFLINE";
        case LLPrismLens::EDisplayHealth::INVALID: return "INVALID";
    }
    return "UNKNOWN";
}

std::string displayVisibilityText(LLPrismLens::EDisplayVisibility visibility)
{
    switch (visibility)
    {
        case LLPrismLens::EDisplayVisibility::UNKNOWN: return "UNKNOWN";
        case LLPrismLens::EDisplayVisibility::OFFSCREEN: return "OFFSCREEN";
        case LLPrismLens::EDisplayVisibility::VISIBLE: return "VISIBLE";
    }
    return "UNKNOWN";
}

std::string fitModeText(LLPrismLens::EFitMode mode)
{
    switch (mode)
    {
        case LLPrismLens::EFitMode::FIT: return "Fit";
        case LLPrismLens::EFitMode::FILL: return "Fill";
        case LLPrismLens::EFitMode::STRETCH: return "Stretch";
    }
    return "Unknown";
}

std::string performanceStateText(LLPrismLens::EPerformanceState state)
{
    switch (state)
    {
        case LLPrismLens::EPerformanceState::DISABLED: return "DISABLED";
        case LLPrismLens::EPerformanceState::LEARNING: return "LEARNING";
        case LLPrismLens::EPerformanceState::STEADY: return "STEADY";
        case LLPrismLens::EPerformanceState::PROTECTING: return "PROTECTING";
        case LLPrismLens::EPerformanceState::SUSPENDED: return "SUSPENDED";
        case LLPrismLens::EPerformanceState::TIMING_UNAVAILABLE: return "TIMING UNAVAILABLE";
    }
    return "UNKNOWN";
}

std::string requestedRateText(const LLPrismLens::CaptureDefinition& capture)
{
    if (capture.mRate.mMode == LLPrismLens::EOutputRateMode::AUTOMATIC)
    {
        return "AUTO";
    }
    return llformat("%.3g", capture.mRate.mTargetFps);
}

LLPrismLens::EFitMode fitModeFromValue(const LLSD& value)
{
    const std::string mode = value.asString();
    if (mode == "fill")
    {
        return LLPrismLens::EFitMode::FILL;
    }
    if (mode == "stretch")
    {
        return LLPrismLens::EFitMode::STRETCH;
    }
    return LLPrismLens::EFitMode::FIT;
}

std::string fitModeValue(LLPrismLens::EFitMode mode)
{
    switch (mode)
    {
        case LLPrismLens::EFitMode::FILL: return "fill";
        case LLPrismLens::EFitMode::STRETCH: return "stretch";
        case LLPrismLens::EFitMode::FIT: break;
    }
    return "fit";
}

void setActionState(LLUICtrl* control, bool enabled, const std::string& tooltip)
{
    control->setEnabled(enabled);
    if (control->getToolTip() != tooltip)
    {
        control->setToolTip(tooltip);
    }
}
}

LLFloaterPrismManager::LLFloaterPrismManager(const LLSD& key)
: LLFloater(key)
{
}

bool LLFloaterPrismManager::postBuild()
{
    mSummaryText = getChild<LLTextBox>("prism_summary");
    mGlobalPerformanceText = getChild<LLTextBox>("global_performance");
    mGlobalReasonText = getChild<LLTextBox>("global_reason");
    mStatusText = getChild<LLTextBox>("status_text");
    mCaptureList = getChild<LLScrollListCtrl>("capture_list");
    mDisplayList = getChild<LLScrollListCtrl>("display_list");

    mAddCameraButton = getChild<LLButton>("add_camera");
    mAddLensButton = getChild<LLButton>("add_lens");
    mRemoveCaptureButton = getChild<LLButton>("remove_capture");
    mSetCameraButton = getChild<LLButton>("set_camera");
    mPlaceEyeButton = getChild<LLButton>("place_eye_in_front");
    mAddDisplayButton = getChild<LLButton>("add_display");
    mRemoveDisplayButton = getChild<LLButton>("remove_display");
    mLocateDisplayButton = getChild<LLButton>("locate_display");

    mCaptureScroll = getChild<LLScrollContainer>("capture_settings_scroll");
    mCaptureDocument = getChild<LLPanel>("capture_settings_document");
    mCameraSettingsPanel = getChild<LLPanel>("camera_settings_panel");
    mLensSettingsPanel = getChild<LLPanel>("lens_settings_panel");
    mRateSettingsPanel = getChild<LLPanel>("rate_settings_panel");
    mPerformanceScroll = getChild<LLScrollContainer>("performance_settings_scroll");
    mPerformanceDocument = getChild<LLPanel>("performance_settings_document");

    mCaptureTitle = getChild<LLTextBox>("capture_title");
    mSourceText = getChild<LLTextBox>("camera_source");
    mRateReadout = getChild<LLTextBox>("rate_readout");
    mDisplaySourceRate = getChild<LLTextBox>("display_source_rate");
    mFovModeCombo = getChild<LLComboBox>("fov_mode");
    mVerticalFovSpinner = getChild<LLSpinCtrl>("vertical_fov");
    mNearClipSpinner = getChild<LLSpinCtrl>("near_clip");
    mFarClipSpinner = getChild<LLSpinCtrl>("far_clip");
    mEyeXSpinner = getChild<LLSpinCtrl>("eye_offset_x");
    mEyeYSpinner = getChild<LLSpinCtrl>("eye_offset_y");
    mEyeZSpinner = getChild<LLSpinCtrl>("eye_offset_z");
    mAspectCombo = getChild<LLComboBox>("output_aspect");
    mCustomAspectSpinner = getChild<LLSpinCtrl>("custom_aspect");
    mRateModeCombo = getChild<LLComboBox>("rate_mode");
    mTargetFpsSpinner = getChild<LLSpinCtrl>("target_fps");
    mRatePresetCombo = getChild<LLComboBox>("rate_preset");
    mNewDisplayFitCombo = getChild<LLComboBox>("new_display_fit");
    mDisplayFitCombo = getChild<LLComboBox>("display_fit");
    mAnchorXSpinner = getChild<LLSpinCtrl>("anchor_x");
    mAnchorYSpinner = getChild<LLSpinCtrl>("anchor_y");
    mBarRedSpinner = getChild<LLSpinCtrl>("bar_red");
    mBarGreenSpinner = getChild<LLSpinCtrl>("bar_green");
    mBarBlueSpinner = getChild<LLSpinCtrl>("bar_blue");

    mCaptureList->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onCaptureSelectionChanged(); });
    mDisplayList->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onDisplaySelectionChanged(); });
    mAddCameraButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAddCamera(); });
    mAddLensButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAddLens(); });
    mRemoveCaptureButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onRemoveCapture(); });
    mSetCameraButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSetCamera(); });
    mPlaceEyeButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onPlaceEyeInFront(); });
    mAddDisplayButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAddDisplay(); });
    mRemoveDisplayButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onRemoveDisplay(); });
    mLocateDisplayButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onLocateDisplay(); });

    const auto camera_commit = [this](LLUICtrl*, const LLSD&)
    {
        onCommitCameraSettings();
    };
    mFovModeCombo->setCommitCallback(camera_commit);
    mVerticalFovSpinner->setCommitCallback(camera_commit);
    mNearClipSpinner->setCommitCallback(camera_commit);
    mFarClipSpinner->setCommitCallback(camera_commit);
    mEyeXSpinner->setCommitCallback(camera_commit);
    mEyeYSpinner->setCommitCallback(camera_commit);
    mEyeZSpinner->setCommitCallback(camera_commit);
    mAspectCombo->setCommitCallback(camera_commit);
    mCustomAspectSpinner->setCommitCallback(camera_commit);

    const auto rate_commit = [this](LLUICtrl*, const LLSD&)
    {
        onCommitRateSettings();
    };
    mRateModeCombo->setCommitCallback(rate_commit);
    mTargetFpsSpinner->setCommitCallback(rate_commit);
    mRatePresetCombo->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onRatePresetChanged(); });

    const auto display_commit = [this](LLUICtrl*, const LLSD&)
    {
        onCommitDisplaySettings();
    };
    mDisplayFitCombo->setCommitCallback(display_commit);
    mAnchorXSpinner->setCommitCallback(display_commit);
    mAnchorYSpinner->setCommitCallback(display_commit);
    mBarRedSpinner->setCommitCallback(display_commit);
    mBarGreenSpinner->setCommitCallback(display_commit);
    mBarBlueSpinner->setCommitCallback(display_commit);

    installDocumentFocusReveal();
    setStatus("Ready. Select a capture or add one from the current world selection.");
    pollSnapshots(true);
    return true;
}

void LLFloaterPrismManager::onOpen(const LLSD& key)
{
    LLFloater::onOpen(key);
    if (!mHaveRegistrySnapshot ||
        mRefreshTimer.getElapsedTimeF32() >= SNAPSHOT_POLL_SECONDS)
    {
        pollSnapshots(true);
    }
}

void LLFloaterPrismManager::draw()
{
    pollSnapshots(false);
    LLFloater::draw();
}

void LLFloaterPrismManager::pollSnapshots(bool force)
{
    if (!force && mRefreshTimer.getElapsedTimeF32() < SNAPSHOT_POLL_SECONDS)
    {
        return;
    }

    LLPrismLens::RegistrySnapshot registry = LLPrismLens::registrySnapshot();
    LLPrismLens::PerformanceSnapshot performance = LLPrismLens::performanceSnapshot();
    const bool configuration_changed = force || !mHaveRegistrySnapshot ||
        registry.mConfigurationRevision != mConfigurationRevision;
    const bool runtime_changed = force || !mHaveRegistrySnapshot ||
        registry.mRuntimeRevision != mRuntimeRevision;
    const bool performance_changed = force || !mHavePerformanceSnapshot ||
        performance.mRevision != mPerformanceRevision;
    const bool age_due = force || mAgeRefreshTimer.getElapsedTimeF32() >= AGE_REFRESH_SECONDS;

    mRegistrySnapshot = registry;
    mPerformanceSnapshot = performance;
    mHaveRegistrySnapshot = true;
    mHavePerformanceSnapshot = true;
    mConfigurationRevision = registry.mConfigurationRevision;
    mRuntimeRevision = registry.mRuntimeRevision;
    mPerformanceRevision = performance.mRevision;

    if (configuration_changed)
    {
        refreshConfiguration();
    }
    else if (runtime_changed || age_due)
    {
        refreshRuntime();
    }
    if (configuration_changed || performance_changed || age_due)
    {
        refreshPerformance();
    }
    refreshSelectionActions();

    mRefreshTimer.reset();
    if (age_due)
    {
        mAgeRefreshTimer.reset();
    }
}

void LLFloaterPrismManager::refreshConfiguration()
{
    if (!selectedCapture())
    {
        mSelectedCapture = LLPrismLens::CaptureHandle();
        if (mRegistrySnapshot.mCaptureCount > 0)
        {
            mSelectedCapture = mRegistrySnapshot.mCaptures[0].mHandle;
        }
    }

    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    if (!display || !sameHandle(display->mCapture, mSelectedCapture))
    {
        mSelectedDisplay = LLPrismLens::DisplayHandle();
    }

    rebuildCaptureList();
    rebuildDisplayList();
    refreshCaptureEditor();
    refreshDisplayEditor();
}

void LLFloaterPrismManager::refreshRuntime()
{
    rebuildCaptureList();
    rebuildDisplayList();
    refreshCaptureRuntimeReadouts();
    refreshDisplayRateReadout();
}

void LLFloaterPrismManager::refreshCaptureRuntimeReadouts()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture)
    {
        mCaptureTitle->setText(LLStringExplicit("No capture selected"));
        mCaptureTitle->setToolTip(std::string());
        mRateReadout->setText(LLStringExplicit("Requested / Entitled / Observed: -"));
        return;
    }
    mCaptureTitle->setText(llformat("%s %u | %s / %s / %s",
        captureModeText(capture->mMode).c_str(), capture->mSlot + 1,
        captureHealthText(capture->mRuntime.mHealth).c_str(),
        outputStateText(capture->mRuntime.mOutput).c_str(),
        activityStateText(capture->mRuntime.mActivity).c_str()));
    mCaptureTitle->setToolTip(capture->mRuntime.mReason);
    mRateReadout->setText(llformat(
        "Requested / Entitled / Observed: %s / %.1f / %.1f FPS | output age %.1f s",
        requestedRateText(*capture).c_str(),
        capture->mRuntime.mCadenceEntitlementHz,
        capture->mRuntime.mObservedPublicationHz,
        capture->mRuntime.mOutputAgeSeconds));
}

void LLFloaterPrismManager::refreshPerformance()
{
    if (!mHavePerformanceSnapshot)
    {
        return;
    }

    const LLPrismLens::PerformanceSnapshot& p = mPerformanceSnapshot;
    mSummaryText->setText(llformat(
        "Prism %u/%u captures | %u/%u displays | %s",
        mRegistrySnapshot.mCaptureCount, LLPrismLens::MAX_CAPTURES,
        mRegistrySnapshot.mDisplayCount, LLPrismLens::MAX_DISPLAY_BINDINGS,
        performanceStateText(p.mState).c_str()));
    const std::string timing = p.mGpuTimingReliable
        ? llformat("render p95 %.1f ms", p.mMainRenderP95Milliseconds)
        : "render timing unavailable";
    mGlobalPerformanceText->setText(llformat(
        "Presented %.1f FPS | %s | scale %.2f-%.2fx | admitted %.1f attempts/s",
        p.mPresentedFps, timing.c_str(),
        p.mMinimumAppliedResolutionScale, p.mMaximumAppliedResolutionScale,
        p.mAdmittedGlobalAttemptRateHz));
    mGlobalReasonText->setText(p.mReason.empty()
        ? llformat("Protected target %.0f FPS | observed %.1f attempts/s%s",
                   p.mEffectiveProtectedMainFps, p.mObservedGlobalAttemptRateHz,
                   p.mGpuTimingReliable ? " | GPU timing ready" : " | FPS-only protection")
        : p.mReason);
    mGlobalReasonText->setToolTip(mGlobalReasonText->getText());
}

void LLFloaterPrismManager::rebuildCaptureList()
{
    mCaptureList->deleteAllItems();
    for (U32 index = 0; index < mRegistrySnapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& capture = mRegistrySnapshot.mCaptures[index];
        const std::string rate = llformat("%s / %.1f / %.1f",
            requestedRateText(capture).c_str(),
            capture.mRuntime.mCadenceEntitlementHz,
            capture.mRuntime.mObservedPublicationHz);
        const std::string tooltip = llformat(
            "Capture %s\nGeneration %llu\n%s\nEffective FOV %.1f deg, far %.1f m\nOutput age %.1f s",
            capture.mHandle.mId.asString().c_str(),
            static_cast<unsigned long long>(capture.mHandle.mGeneration),
            capture.mRuntime.mReason.c_str(),
            capture.mRuntime.mEffectiveVerticalFovRad * RAD_TO_DEG,
            capture.mRuntime.mEffectiveFarClip,
            capture.mRuntime.mOutputAgeSeconds);

        LLSD row;
        row["value"] = capture.mHandle.mId;
        row["columns"][0]["column"] = "number";
        row["columns"][0]["value"] = llformat("%u", capture.mSlot + 1);
        row["columns"][1]["column"] = "type";
        row["columns"][1]["value"] = captureModeText(capture.mMode);
        row["columns"][2]["column"] = "health";
        row["columns"][2]["value"] = captureHealthText(capture.mRuntime.mHealth);
        row["columns"][3]["column"] = "output";
        row["columns"][3]["value"] = outputStateText(capture.mRuntime.mOutput);
        row["columns"][4]["column"] = "activity";
        row["columns"][4]["value"] = activityStateText(capture.mRuntime.mActivity);
        row["columns"][5]["column"] = "displays";
        row["columns"][5]["value"] = llformat("%u", capture.mDisplayCount);
        row["columns"][6]["column"] = "scale";
        row["columns"][6]["value"] = llformat("%.2fx", capture.mRuntime.mEffectiveResolutionScale);
        row["columns"][7]["column"] = "rate";
        row["columns"][7]["value"] = rate;
        for (S32 column = 0; column < 8; ++column)
        {
            row["columns"][column]["tool_tip"] = tooltip;
        }
        mCaptureList->addElement(row, ADD_BOTTOM);
    }

    if (!mSelectedCapture.mId.isNull())
    {
        mCaptureList->selectByValue(LLSD(mSelectedCapture.mId));
    }
}

void LLFloaterPrismManager::rebuildDisplayList()
{
    mDisplayList->deleteAllItems();
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture)
    {
        mSelectedDisplay = LLPrismLens::DisplayHandle();
        return;
    }

    bool selected_still_present = false;
    LLPrismLens::DisplayHandle first_display;
    for (U32 index = 0; index < mRegistrySnapshot.mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& display = mRegistrySnapshot.mDisplays[index];
        if (!sameHandle(display.mCapture, capture->mHandle))
        {
            continue;
        }
        if (first_display.mId.isNull())
        {
            first_display = display.mHandle;
        }
        selected_still_present |= sameHandle(display.mHandle, mSelectedDisplay);

        const std::string face = llformat("%s f%d",
            display.mDisplayObjectId.asString().substr(0, 8).c_str(),
            display.mDisplayTextureEntry);
        const std::string inherited = llformat("%s %u | %.1f FPS",
            captureModeText(capture->mMode).c_str(), capture->mSlot + 1,
            capture->mRuntime.mObservedPublicationHz);
        const std::string tooltip = llformat(
            "Display %s\nGeneration %llu\nObject %s face %d\nHealth: %s\nVisibility: %s\nInherited capture %s",
            display.mHandle.mId.asString().c_str(),
            static_cast<unsigned long long>(display.mHandle.mGeneration),
            display.mDisplayObjectId.asString().c_str(),
            display.mDisplayTextureEntry,
            display.mRuntime.mHealthReason.c_str(),
            display.mRuntime.mVisibilityReason.c_str(),
            capture->mHandle.mId.asString().c_str());

        LLSD row;
        row["value"] = display.mHandle.mId;
        row["columns"][0]["column"] = "display_health";
        row["columns"][0]["value"] = displayHealthText(display.mRuntime.mHealth);
        row["columns"][1]["column"] = "visibility";
        row["columns"][1]["value"] = displayVisibilityText(display.mRuntime.mVisibility);
        row["columns"][2]["column"] = "face";
        row["columns"][2]["value"] = face;
        row["columns"][3]["column"] = "mapping";
        row["columns"][3]["value"] = fitModeText(display.mSettings.mFitMode);
        row["columns"][4]["column"] = "picture_rate";
        row["columns"][4]["value"] = inherited;
        for (S32 column = 0; column < 5; ++column)
        {
            row["columns"][column]["tool_tip"] = tooltip;
        }
        mDisplayList->addElement(row, ADD_BOTTOM);
    }

    if (!selected_still_present)
    {
        mSelectedDisplay = first_display;
    }
    if (!mSelectedDisplay.mId.isNull())
    {
        mDisplayList->selectByValue(LLSD(mSelectedDisplay.mId));
    }
}

void LLFloaterPrismManager::refreshCaptureEditor()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    mCaptureDocument->setEnabled(capture != nullptr);
    if (!capture)
    {
        mCaptureTitle->setText(LLStringExplicit("No capture selected"));
        mSourceText->setText(LLStringExplicit("Camera source: -"));
        mRateReadout->setText(LLStringExplicit("Requested / Entitled / Observed: -"));
        mCameraSettingsPanel->setVisible(false);
        mLensSettingsPanel->setVisible(false);
        mRateSettingsPanel->setVisible(false);
        return;
    }

    const bool is_camera = capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED;
    refreshCaptureRuntimeReadouts();
    mCameraSettingsPanel->setVisible(is_camera);
    mLensSettingsPanel->setVisible(!is_camera);
    mRateSettingsPanel->setVisible(true);

    if (is_camera)
    {
        mSourceText->setText(capture->mCameraObjectId.isNull()
            ? "Camera source: unbound"
            : "Camera source: " + capture->mCameraObjectId.asString());
        mFovModeCombo->setValue(capture->mCamera.mFovMode == LLPrismLens::EFovMode::FOLLOW_PROJECTOR
            ? LLSD("projector") : LLSD("fixed"));
        mVerticalFovSpinner->setValue(capture->mCamera.mFixedVerticalFovRad * RAD_TO_DEG);
        mVerticalFovSpinner->setEnabled(capture->mCamera.mFovMode == LLPrismLens::EFovMode::FIXED);
        mNearClipSpinner->setValue(capture->mCamera.mNearClip);
        mFarClipSpinner->setValue(capture->mCamera.mFarClip);
        mEyeXSpinner->setValue(capture->mCamera.mLocalEyeOffset.mV[VX]);
        mEyeYSpinner->setValue(capture->mCamera.mLocalEyeOffset.mV[VY]);
        mEyeZSpinner->setValue(capture->mCamera.mLocalEyeOffset.mV[VZ]);

        const F32 aspect = capture->mCamera.mOutputAspect;
        std::string aspect_value = "custom";
        if (is_approx_equal(aspect, 16.f / 9.f)) aspect_value = "16:9";
        else if (is_approx_equal(aspect, 4.f / 3.f)) aspect_value = "4:3";
        else if (is_approx_equal(aspect, 1.f)) aspect_value = "1:1";
        mAspectCombo->setValue(aspect_value);
        mCustomAspectSpinner->setValue(aspect);
        mCustomAspectSpinner->setEnabled(aspect_value == "custom");
    }

    mRateModeCombo->setValue(capture->mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS
        ? LLSD("target") : LLSD("automatic"));
    mTargetFpsSpinner->setValue(capture->mRate.mTargetFps);
    const bool target_mode = capture->mRate.mMode == LLPrismLens::EOutputRateMode::TARGET_FPS;
    mTargetFpsSpinner->setEnabled(target_mode);
    mRatePresetCombo->setEnabled(target_mode);
}

void LLFloaterPrismManager::refreshDisplayEditor()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    const bool editable = capture && display &&
        capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED;

    mDisplayFitCombo->setEnabled(editable);
    mAnchorXSpinner->setEnabled(editable);
    mAnchorYSpinner->setEnabled(editable);
    mBarRedSpinner->setEnabled(editable);
    mBarGreenSpinner->setEnabled(editable);
    mBarBlueSpinner->setEnabled(editable);
    refreshDisplayRateReadout();
    if (!capture || !display)
    {
        return;
    }
    mDisplayFitCombo->setValue(fitModeValue(display->mSettings.mFitMode));
    mAnchorXSpinner->setValue(display->mSettings.mAnchor[0]);
    mAnchorYSpinner->setValue(display->mSettings.mAnchor[1]);
    mBarRedSpinner->setValue(display->mSettings.mBarColorLinear[0]);
    mBarGreenSpinner->setValue(display->mSettings.mBarColorLinear[1]);
    mBarBlueSpinner->setValue(display->mSettings.mBarColorLinear[2]);
}

void LLFloaterPrismManager::refreshDisplayRateReadout()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    if (!capture)
    {
        mDisplaySourceRate->setText(LLStringExplicit("Select a capture to view its displays."));
        return;
    }
    if (!display)
    {
        if (capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED && capture->mDisplayCount == 0)
        {
            mDisplaySourceRate->setText(LLStringExplicit(
                "Select exactly one flat world face, then click Add selected face."));
            return;
        }
        mDisplaySourceRate->setText(llformat(
            "No display selected | inherited from %s %u at %.1f FPS",
            captureModeText(capture->mMode).c_str(), capture->mSlot + 1,
            capture->mRuntime.mObservedPublicationHz));
        return;
    }

    const bool editable = capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED;
    mDisplaySourceRate->setText(llformat(
        "Inherited from %s %u | requested %s | observed %.1f FPS%s",
        captureModeText(capture->mMode).c_str(), capture->mSlot + 1,
        requestedRateText(*capture).c_str(), capture->mRuntime.mObservedPublicationHz,
        editable ? "" : " | Lens mapping is fixed"));
}

void LLFloaterPrismManager::refreshSelectionActions()
{
    const LLPrismLens::ActionStatus camera_status = LLPrismLens::addCameraSelectionStatus();
    const LLPrismLens::ActionStatus lens_status = LLPrismLens::addLensSelectionStatus();
    setActionState(mAddCameraButton, camera_status.allowed(), camera_status.allowed()
        ? "Create one Camera Feed producer from the exactly-one selected world object"
        : camera_status.mReason);
    setActionState(mAddLensButton, lens_status.allowed(), lens_status.allowed()
        ? "Create one Surface Lens producer from the exactly-one selected world face"
        : lens_status.mReason);

    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    setActionState(mRemoveCaptureButton, capture != nullptr,
        capture ? "Remove this capture and all of its display bindings"
                : "Select a capture first");

    bool can_set_camera = false;
    std::string set_camera_reason = "Select a Camera Feed capture first";
    bool can_add_display = false;
    std::string add_display_reason = "Select a Camera Feed capture first";
    if (capture && capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED)
    {
        const LLPrismLens::ActionStatus source_status =
            LLPrismLens::setCameraSelectionStatus(capture->mHandle);
        can_set_camera = source_status.allowed();
        set_camera_reason = source_status.allowed()
            ? "Rebind this Camera Feed to the exactly-one selected world object"
            : source_status.mReason;
        const LLPrismLens::ActionStatus display_status =
            LLPrismLens::addDisplaySelectionStatus(capture->mHandle);
        can_add_display = display_status.allowed();
        add_display_reason = display_status.allowed()
            ? "Link the exactly-one selected face to this retained Camera Feed output"
            : display_status.mReason;
    }
    setActionState(mSetCameraButton, can_set_camera, set_camera_reason);
    setActionState(mAddDisplayButton, can_add_display, add_display_reason);
    setActionState(mNewDisplayFitCombo, can_add_display, can_add_display
        ? "Initial mapping for the new display face"
        : add_display_reason);
    setActionState(mPlaceEyeButton,
        capture && capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED &&
            !capture->mCameraObjectId.isNull(),
        capture && capture->mMode == LLPrismLens::ECaptureMode::CAMERA_FEED
            ? "Move the local camera eye just beyond the marker's front (-Z) face"
            : "This control applies only to Camera Feed captures");

    const bool lens_display = capture &&
        capture->mMode == LLPrismLens::ECaptureMode::SURFACE_LENS;
    setActionState(mRemoveDisplayButton, display && !lens_display,
        !display ? "Select a display first"
                 : lens_display ? "A Lens owns its aperture display; remove the capture instead"
                                : "Remove only this display binding");
    setActionState(mLocateDisplayButton, display != nullptr,
        display ? "Select this face in-world and frame its object"
                : "Select a display first");
}

void LLFloaterPrismManager::installDocumentFocusReveal()
{
    ALScrollFocus::install(mCaptureScroll, mCaptureDocument, mCaptureDocument);
    ALScrollFocus::install(mPerformanceScroll, mPerformanceDocument, mPerformanceDocument);
}

void LLFloaterPrismManager::revealDocumentView(
    LLView* view, LLPanel* document, LLScrollContainer* scroller)
{
    LLRect document_rect;
    if (view && view->localRectToOtherView(
            view->getLocalRect(), &document_rect, document))
    {
        scroller->scrollToShowRect(document_rect);
    }
}

void LLFloaterPrismManager::onCaptureSelectionChanged()
{
    const LLUUID id = mCaptureList->getSelectedValue().asUUID();
    for (U32 index = 0; index < mRegistrySnapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& capture = mRegistrySnapshot.mCaptures[index];
        if (capture.mHandle.mId == id)
        {
            mSelectedCapture = capture.mHandle;
            mSelectedDisplay = LLPrismLens::DisplayHandle();
            rebuildDisplayList();
            refreshCaptureEditor();
            refreshDisplayEditor();
            refreshSelectionActions();
            revealDocumentView(mCaptureTitle, mCaptureDocument, mCaptureScroll);
            return;
        }
    }
    mSelectedCapture = LLPrismLens::CaptureHandle();
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    refreshConfiguration();
}

void LLFloaterPrismManager::onDisplaySelectionChanged()
{
    const LLUUID id = mDisplayList->getSelectedValue().asUUID();
    for (U32 index = 0; index < mRegistrySnapshot.mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& display = mRegistrySnapshot.mDisplays[index];
        if (display.mHandle.mId == id && sameHandle(display.mCapture, mSelectedCapture))
        {
            mSelectedDisplay = display.mHandle;
            refreshDisplayEditor();
            refreshSelectionActions();
            return;
        }
    }
    mSelectedDisplay = LLPrismLens::DisplayHandle();
    refreshDisplayEditor();
    refreshSelectionActions();
}

void LLFloaterPrismManager::onAddCamera()
{
    LLPrismLens::CaptureHandle capture;
    std::string reason;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::addCameraCaptureFromSelectedObject(&capture, &reason);
    if (result == LLPrismLens::ERegistryResult::OK)
    {
        mSelectedCapture = capture;
        mSelectedDisplay = LLPrismLens::DisplayHandle();
        setStatus("Added a Camera Feed capture from the selected object.");
        invalidateRegistrySnapshot();
        revealDocumentView(mCaptureTitle, mCaptureDocument, mCaptureScroll);
        return;
    }
    setStatus("Could not add camera: " + reason);
}

void LLFloaterPrismManager::onAddLens()
{
    LLPrismLens::CaptureHandle capture;
    std::string reason;
    const LLPrismLens::ERegistryResult result =
        LLPrismLens::addSurfaceLensFromSelectedFace(&capture, &reason);
    if (result == LLPrismLens::ERegistryResult::OK)
    {
        mSelectedCapture = capture;
        mSelectedDisplay = LLPrismLens::DisplayHandle();
        setStatus("Added a Surface Lens capture from the selected face.");
        invalidateRegistrySnapshot();
        revealDocumentView(mCaptureTitle, mCaptureDocument, mCaptureScroll);
        return;
    }
    setStatus("Could not add lens: " + reason);
}

void LLFloaterPrismManager::onRemoveCapture()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture)
    {
        return;
    }

    const LLPrismLens::CaptureHandle handle = capture->mHandle;
    LLSD args;
    args["MESSAGE"] = llformat(
        "Remove %s %u and all %u of its display bindings?",
        captureModeText(capture->mMode).c_str(), capture->mSlot + 1,
        capture->mDisplayCount);
    LLHandle<LLFloater> floater_handle = getHandle();
    LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(),
        [floater_handle, handle](const LLSD& notification, const LLSD& response)
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
            {
                return;
            }
            LLFloaterPrismManager* self =
                static_cast<LLFloaterPrismManager*>(floater_handle.get());
            if (!self)
            {
                return;
            }
            if (LLPrismLens::removeCapture(handle))
            {
                self->mSelectedCapture = LLPrismLens::CaptureHandle();
                self->mSelectedDisplay = LLPrismLens::DisplayHandle();
                self->setStatus("Removed the capture and its display bindings.");
                self->invalidateRegistrySnapshot();
            }
            else
            {
                self->setStatus("The capture changed before it could be removed. Refresh and try again.");
                self->invalidateRegistrySnapshot();
            }
        });
}

void LLFloaterPrismManager::onSetCamera()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture)
    {
        return;
    }
    std::string reason;
    if (LLPrismLens::setSelectedCamera(capture->mHandle, &reason))
    {
        setStatus("Rebound the Camera Feed to the selected object.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Could not set camera: " + reason);
}

void LLFloaterPrismManager::onPlaceEyeInFront()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture || capture->mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
    {
        return;
    }
    LLViewerObject* source = gObjectList.findObject(capture->mCameraObjectId);
    if (!source)
    {
        setStatus("The camera source is currently offline; its scale is unavailable.");
        return;
    }

    LLPrismLens::CameraSettings settings = capture->mCamera;
    settings.mLocalEyeOffset.mV[VZ] =
        -(0.5f * source->getScale().mV[VZ] + settings.mNearClip + 0.01f);
    std::string reason;
    if (LLPrismLens::setCameraSettings(capture->mHandle, settings, &reason))
    {
        setStatus("Placed the eye just beyond the camera marker's front (-Z) face.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Could not place eye: " + reason);
}

void LLFloaterPrismManager::onCommitCameraSettings()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture || capture->mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
    {
        return;
    }

    LLPrismLens::CameraSettings settings = capture->mCamera;
    settings.mFovMode = mFovModeCombo->getValue().asString() == "projector"
        ? LLPrismLens::EFovMode::FOLLOW_PROJECTOR : LLPrismLens::EFovMode::FIXED;
    settings.mFixedVerticalFovRad = static_cast<F32>(mVerticalFovSpinner->getValue().asReal() * DEG_TO_RAD);
    settings.mNearClip = static_cast<F32>(mNearClipSpinner->getValue().asReal());
    settings.mFarClip = static_cast<F32>(mFarClipSpinner->getValue().asReal());
    settings.mLocalEyeOffset.set(
        static_cast<F32>(mEyeXSpinner->getValue().asReal()),
        static_cast<F32>(mEyeYSpinner->getValue().asReal()),
        static_cast<F32>(mEyeZSpinner->getValue().asReal()));

    const std::string aspect = mAspectCombo->getValue().asString();
    if (aspect == "16:9") settings.mOutputAspect = 16.f / 9.f;
    else if (aspect == "4:3") settings.mOutputAspect = 4.f / 3.f;
    else if (aspect == "1:1") settings.mOutputAspect = 1.f;
    else settings.mOutputAspect = static_cast<F32>(mCustomAspectSpinner->getValue().asReal());

    std::string reason;
    if (LLPrismLens::setCameraSettings(capture->mHandle, settings, &reason))
    {
        setStatus("Updated camera optics. The retained output will refresh without changing its bindings.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Camera settings were rejected: " + reason);
    refreshCaptureEditor();
}

void LLFloaterPrismManager::onCommitRateSettings()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture)
    {
        return;
    }

    LLPrismLens::CaptureRateSettings settings = capture->mRate;
    settings.mMode = mRateModeCombo->getValue().asString() == "target"
        ? LLPrismLens::EOutputRateMode::TARGET_FPS
        : LLPrismLens::EOutputRateMode::AUTOMATIC;
    settings.mTargetFps = static_cast<F32>(mTargetFpsSpinner->getValue().asReal());

    std::string reason;
    if (LLPrismLens::setCaptureRateSettings(capture->mHandle, settings, &reason))
    {
        setStatus("Updated the producer's picture rate; every linked face inherits it.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Picture rate was rejected: " + reason);
    refreshCaptureEditor();
}

void LLFloaterPrismManager::onRatePresetChanged()
{
    const F32 preset = static_cast<F32>(mRatePresetCombo->getValue().asReal());
    if (preset >= 1.f && preset <= 30.f)
    {
        mTargetFpsSpinner->setValue(preset);
        onCommitRateSettings();
    }
}

void LLFloaterPrismManager::onAddDisplay()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    if (!capture)
    {
        return;
    }
    LLPrismLens::DisplayHandle display;
    std::string reason;
    const LLPrismLens::ERegistryResult result = LLPrismLens::addSelectedDisplay(
        capture->mHandle, fitModeFromValue(mNewDisplayFitCombo->getValue()),
        &display, &reason);
    if (result == LLPrismLens::ERegistryResult::OK)
    {
        mSelectedDisplay = display;
        setStatus("Linked the selected face to this capture; no extra scene render was created.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Could not add display: " + reason);
}

void LLFloaterPrismManager::onRemoveDisplay()
{
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    if (!display)
    {
        return;
    }
    std::string reason;
    if (LLPrismLens::removeDisplay(display->mHandle, &reason))
    {
        mSelectedDisplay = LLPrismLens::DisplayHandle();
        setStatus("Removed this display binding. The capture and sibling displays remain.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Could not remove display: " + reason);
}

void LLFloaterPrismManager::onLocateDisplay()
{
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    if (!display)
    {
        return;
    }
    LLViewerObject* object = gObjectList.findObject(display->mDisplayObjectId);
    if (!object)
    {
        setStatus("That display object is currently offline.");
        return;
    }

    LLSelectMgr::getInstance()->deselectAll();
    LLSelectMgr::getInstance()->selectObjectOnly(object, display->mDisplayTextureEntry);
    handle_zoom_to_object(object->getID());
    setStatus("Selected the display face in-world and framed its object.");
}

void LLFloaterPrismManager::onCommitDisplaySettings()
{
    const LLPrismLens::CaptureDefinition* capture = selectedCapture();
    const LLPrismLens::DisplayDefinition* display = selectedDisplay();
    if (!capture || !display || capture->mMode != LLPrismLens::ECaptureMode::CAMERA_FEED)
    {
        return;
    }

    LLPrismLens::DisplaySettings settings = display->mSettings;
    settings.mFitMode = fitModeFromValue(mDisplayFitCombo->getValue());
    settings.mAnchor[0] = static_cast<F32>(mAnchorXSpinner->getValue().asReal());
    settings.mAnchor[1] = static_cast<F32>(mAnchorYSpinner->getValue().asReal());
    settings.mBarColorLinear[0] = static_cast<F32>(mBarRedSpinner->getValue().asReal());
    settings.mBarColorLinear[1] = static_cast<F32>(mBarGreenSpinner->getValue().asReal());
    settings.mBarColorLinear[2] = static_cast<F32>(mBarBlueSpinner->getValue().asReal());

    std::string reason;
    if (LLPrismLens::setDisplaySettings(display->mHandle, settings, &reason))
    {
        setStatus("Updated only this face's mapping; the shared capture was not re-rendered.");
        invalidateRegistrySnapshot();
        return;
    }
    setStatus("Display settings were rejected: " + reason);
    refreshDisplayEditor();
}

const LLPrismLens::CaptureDefinition* LLFloaterPrismManager::selectedCapture() const
{
    if (!mHaveRegistrySnapshot || mSelectedCapture.mId.isNull())
    {
        return nullptr;
    }
    for (U32 index = 0; index < mRegistrySnapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& capture = mRegistrySnapshot.mCaptures[index];
        if (sameHandle(capture.mHandle, mSelectedCapture))
        {
            return &capture;
        }
    }
    return nullptr;
}

const LLPrismLens::DisplayDefinition* LLFloaterPrismManager::selectedDisplay() const
{
    if (!mHaveRegistrySnapshot || mSelectedDisplay.mId.isNull())
    {
        return nullptr;
    }
    for (U32 index = 0; index < mRegistrySnapshot.mDisplayCount; ++index)
    {
        const LLPrismLens::DisplayDefinition& display = mRegistrySnapshot.mDisplays[index];
        if (sameHandle(display.mHandle, mSelectedDisplay))
        {
            return &display;
        }
    }
    return nullptr;
}

void LLFloaterPrismManager::setStatus(const std::string& message)
{
    mStatusText->setText(message);
    if (mStatusText->getToolTip() != message)
    {
        mStatusText->setToolTip(message);
    }
}

void LLFloaterPrismManager::invalidateRegistrySnapshot()
{
    mHaveRegistrySnapshot = false;
    pollSnapshots(true);
}
