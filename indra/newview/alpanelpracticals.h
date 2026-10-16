/**
 * @file alpanelpracticals.h
 * @brief Director > Light > Practicals page logic: shadows, light cone and gobo for the selected projector.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The page root of the generated kit page (class "panel_practicals"). It does for the Director page
 * what the "Selected Light" tab of the Lightbox does (ALFloaterLightBox::updateSelectedLightPanel,
 * onSelLight* and onSelGoboChanged): it follows the primary selection, enables the per-projector
 * controls only for a spotlight projector, mirrors the session-only pipeline state into them, and
 * writes edits back through the same LLPipeline entry points. Nothing here is persisted.
 */

#ifndef AL_ALPANELPRACTICALS_H
#define AL_ALPANELPRACTICALS_H

#include "llpanel.h"
#include "lluuid.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLSliderCtrl;
class LLTextBox;

class ALPanelPracticals final : public LLPanel
{
public:
    ALPanelPracticals() = default;
    ~ALPanelPracticals() override = default;

    bool postBuild() override;
    void draw() override;

private:
    void refreshFromSelection();
    void onCastShadows();
    void onVolumetric();
    void onCaptureOverride();
    void onClearOverride();
    void onGoboChanged();

    LLCheckBoxCtrl*    mCastShadows = nullptr;
    LLCheckBoxCtrl*    mVolumetric = nullptr;
    LLButton*          mCapture = nullptr;
    LLButton*          mClear = nullptr;
    LLTextBox*         mStatus = nullptr;
    LLComboBox*        mGoboPattern = nullptr;
    LLComboBox*        mGoboAnim = nullptr;
    LLSliderCtrl*      mGoboSpeed = nullptr;
    LLSliderCtrl*      mGoboZoom = nullptr;
    LLSliderCtrl*      mGoboDispersion = nullptr;
    LLColorSwatchCtrl* mGoboTint = nullptr;
    LLSliderCtrl*      mGoboVar1 = nullptr;
    LLSliderCtrl*      mGoboVar2 = nullptr;
    LLSliderCtrl*      mGoboSoft = nullptr;
    LLCheckBoxCtrl*    mGoboInvert = nullptr;

    LLUUID mLastId;
    bool   mLastProjector = false;
};

#endif // AL_ALPANELPRACTICALS_H
