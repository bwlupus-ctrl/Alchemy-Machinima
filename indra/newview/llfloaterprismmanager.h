/**
 * @file llfloaterprismmanager.h
 * @brief Editor for shared Prism captures and their display bindings.
 */

#ifndef LL_LLFLOATERPRISMMANAGER_H
#define LL_LLFLOATERPRISMMANAGER_H

#include "llfloater.h"
#include "llframetimer.h"
#include "llprismlens.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLPanel;
class LLScrollContainer;
class LLScrollListCtrl;
class LLSpinCtrl;
class LLTextBox;
class LLView;

class LLFloaterPrismManager final : public LLFloater
{
public:
    explicit LLFloaterPrismManager(const LLSD& key);

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;

private:
    void pollSnapshots(bool force);
    void refreshConfiguration();
    void refreshRuntime();
    void refreshPerformance();
    void refreshSelectionActions();
    void refreshCaptureEditor();
    void refreshCaptureRuntimeReadouts();
    void refreshDisplayEditor();
    void refreshDisplayRateReadout();
    void rebuildCaptureList();
    void rebuildDisplayList();

    void installDocumentFocusReveal();
    void revealDocumentView(LLView* view, LLPanel* document,
                            LLScrollContainer* scroller);

    void onCaptureSelectionChanged();
    void onDisplaySelectionChanged();
    void onAddCamera();
    void onAddLens();
    void onRemoveCapture();
    void onSetCamera();
    void onPlaceEyeInFront();
    void onCommitCameraSettings();
    void onCommitRateSettings();
    void onRatePresetChanged();
    void onAddDisplay();
    void onRemoveDisplay();
    void onLocateDisplay();
    void onCommitDisplaySettings();

    const LLPrismLens::CaptureDefinition* selectedCapture() const;
    const LLPrismLens::DisplayDefinition* selectedDisplay() const;
    void setStatus(const std::string& message);
    void invalidateRegistrySnapshot();

    LLPrismLens::CaptureHandle mSelectedCapture;
    LLPrismLens::DisplayHandle mSelectedDisplay;
    LLPrismLens::RegistrySnapshot mRegistrySnapshot;
    LLPrismLens::PerformanceSnapshot mPerformanceSnapshot;
    bool mHaveRegistrySnapshot = false;
    bool mHavePerformanceSnapshot = false;
    U64 mConfigurationRevision = 0;
    U64 mRuntimeRevision = 0;
    U64 mPerformanceRevision = 0;
    LLFrameTimer mRefreshTimer;
    LLFrameTimer mAgeRefreshTimer;

    LLTextBox* mSummaryText = nullptr;
    LLTextBox* mGlobalPerformanceText = nullptr;
    LLTextBox* mGlobalReasonText = nullptr;
    LLTextBox* mStatusText = nullptr;
    LLScrollListCtrl* mCaptureList = nullptr;
    LLScrollListCtrl* mDisplayList = nullptr;

    LLButton* mAddCameraButton = nullptr;
    LLButton* mAddLensButton = nullptr;
    LLButton* mRemoveCaptureButton = nullptr;
    LLButton* mSetCameraButton = nullptr;
    LLButton* mPlaceEyeButton = nullptr;
    LLButton* mAddDisplayButton = nullptr;
    LLButton* mRemoveDisplayButton = nullptr;
    LLButton* mLocateDisplayButton = nullptr;

    LLScrollContainer* mCaptureScroll = nullptr;
    LLPanel* mCaptureDocument = nullptr;
    LLPanel* mCameraSettingsPanel = nullptr;
    LLPanel* mLensSettingsPanel = nullptr;
    LLPanel* mRateSettingsPanel = nullptr;
    LLScrollContainer* mPerformanceScroll = nullptr;
    LLPanel* mPerformanceDocument = nullptr;

    LLTextBox* mCaptureTitle = nullptr;
    LLTextBox* mSourceText = nullptr;
    LLTextBox* mRateReadout = nullptr;
    LLTextBox* mDisplaySourceRate = nullptr;
    LLComboBox* mFovModeCombo = nullptr;
    LLSpinCtrl* mVerticalFovSpinner = nullptr;
    LLSpinCtrl* mNearClipSpinner = nullptr;
    LLSpinCtrl* mFarClipSpinner = nullptr;
    LLSpinCtrl* mEyeXSpinner = nullptr;
    LLSpinCtrl* mEyeYSpinner = nullptr;
    LLSpinCtrl* mEyeZSpinner = nullptr;
    LLComboBox* mAspectCombo = nullptr;
    LLSpinCtrl* mCustomAspectSpinner = nullptr;
    LLSpinCtrl* mChromaticAberrationSpinner = nullptr;
    LLSpinCtrl* mFilmGrainSpinner = nullptr;
    LLSpinCtrl* mCRTScanlinesSpinner = nullptr;
    LLSpinCtrl* mExposureBiasSpinner = nullptr;
    LLComboBox* mRateModeCombo = nullptr;
    LLSpinCtrl* mTargetFpsSpinner = nullptr;
    LLComboBox* mRatePresetCombo = nullptr;
    LLComboBox* mNewDisplayFitCombo = nullptr;
    LLComboBox* mDisplayFitCombo = nullptr;
    LLSpinCtrl* mAnchorXSpinner = nullptr;
    LLSpinCtrl* mAnchorYSpinner = nullptr;
    LLSpinCtrl* mBarRedSpinner = nullptr;
    LLSpinCtrl* mBarGreenSpinner = nullptr;
    LLSpinCtrl* mBarBlueSpinner = nullptr;
};

#endif // LL_LLFLOATERPRISMMANAGER_H
