/**
 * @file alpanellensgaze.h
 * @brief Shared Lens Gaze controls for the two Actor Mover hosts.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALPANELLENSGAZE_H
#define AL_ALPANELLENSGAZE_H

#include "llpanel.h"
#include "lluuid.h"

#include <string>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLSliderCtrl;
class LLSpinCtrl;
class LLTextBox;

class ALPanelLensGaze final : public LLPanel
{
public:
    ALPanelLensGaze() = default;
    ~ALPanelLensGaze() override = default;

    bool postBuild() override;
    void draw() override;

    // Commits address every selected actor. Empty selection deliberately means
    // the agent avatar; refresh presents the first selected actor's state.
    void setSelectedActors(const uuid_vec_t& ids) { mSelected = ids; }

private:
    LLUUID displayActor() const;
    uuid_vec_t commitActors() const;
    void refreshCastCombo();
    void refreshControls();

    void onEnableCommit();
    void onTargetCommit();
    void onCastCommit();
    void onSetPoint();
    void onBlendCommit();
    void onTorsoCommit();
    void onIntensityCommit();
    void onSmoothingCommit();
    void onBreakoffCommit();
    void onEyelineCommit();

    uuid_vec_t mSelected;
    std::string mCastSignature;

    LLTextBox* mStatus = nullptr;
    LLCheckBoxCtrl* mEnable = nullptr;
    LLComboBox* mTarget = nullptr;
    LLComboBox* mCast = nullptr;
    LLButton* mSetPoint = nullptr;
    LLSliderCtrl* mBlend = nullptr;
    LLSliderCtrl* mTorso = nullptr;
    LLSliderCtrl* mIntensity = nullptr;
    LLSliderCtrl* mSmoothing = nullptr;
    LLSpinCtrl* mDeadZone = nullptr;
    LLCheckBoxCtrl* mBreakoff = nullptr;
    LLSpinCtrl* mBreakoffAngle = nullptr;
    LLSliderCtrl* mEyelineYaw = nullptr;
    LLSliderCtrl* mEyelinePitch = nullptr;
};

#endif // AL_ALPANELLENSGAZE_H
