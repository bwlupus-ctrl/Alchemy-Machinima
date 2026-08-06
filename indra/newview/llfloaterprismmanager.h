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
class LLSliderCtrl;
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
    void onCommitDisplayEffects();
    void onApplyEffectsPreset(const LLPrismLens::ScreenEffects& preset);
    LLPrismLens::ScreenEffects effectsFromUI() const;
    void setUIFromEffects(const LLPrismLens::ScreenEffects& effects);

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
    // Last Add-Display reject reason surfaced through the status line by
    // refreshSelectionActions(). That poll runs ~4x/sec, so the reason is shown
    // only when it CHANGES; identical re-polls leave any transient
    // success/error message from a button handler untouched. Cleared when the
    // action becomes allowed (or no Camera Feed capture is selected) so a later
    // identical rejection is displayed again.
    std::string mLastSelectionActionStatus;
    // One-shot guard set by a success handler (Add camera / Add selected face)
    // immediately before it forces the synchronous snapshot refresh that
    // follows its own setStatus(). The just-completed action flips the
    // still-current selection into a rejected state (the face Add just linked
    // IS "already a Prism display"), and without this guard the synchronous
    // refreshSelectionActions() would overwrite the success message within the
    // same click. The very next refreshSelectionActions() consumes the flag:
    // it records the reason in mLastSelectionActionStatus WITHOUT surfacing
    // it, so the confirmation stays visible until the selection changes, while
    // a different reject on a fresh selection still surfaces once.
    bool mSuppressSelectionRejectOnce = false;

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

    // Per-display TV screen effects (Displays tab). Applied at composite time
    // only; every slider at 0 is a strict shader no-op for that display.
    LLScrollContainer* mEffectsScroll = nullptr;
    LLPanel* mEffectsDocument = nullptr;
    LLButton* mPresetCleanButton = nullptr;
    LLButton* mPresetCRTButton = nullptr;
    LLButton* mPresetBrokenButton = nullptr;
    LLButton* mPresetVHSButton = nullptr;
    LLButton* mPresetCCTVButton = nullptr;
    LLSliderCtrl* mEffectScanlinesSlider = nullptr;
    LLSliderCtrl* mEffectScanlineCountSlider = nullptr;
    LLSliderCtrl* mEffectPixelateSlider = nullptr;
    LLSliderCtrl* mEffectGrayscaleSlider = nullptr;
    LLSliderCtrl* mEffectSepiaSlider = nullptr;
    LLSliderCtrl* mEffectStaticSlider = nullptr;
    LLSliderCtrl* mEffectVerticalRollSlider = nullptr;
    LLSliderCtrl* mEffectRollSpeedSlider = nullptr;
    LLSliderCtrl* mEffectTrackingSlider = nullptr;
    LLSliderCtrl* mEffectFlickerSlider = nullptr;
    LLSliderCtrl* mEffectChromaBleedSlider = nullptr;
    LLSliderCtrl* mEffectVignetteSlider = nullptr;
    LLSliderCtrl* mEffectInterlaceSlider = nullptr;
    LLSliderCtrl* mEffectDropoutSlider = nullptr;
    LLSliderCtrl* mEffectBrightnessSlider = nullptr;
};

#endif // LL_LLFLOATERPRISMMANAGER_H
