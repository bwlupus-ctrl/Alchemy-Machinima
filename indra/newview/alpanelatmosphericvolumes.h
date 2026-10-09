/**
 * @file alpanelatmosphericvolumes.h
 * @brief Shared editor for client-only atmospheric fog volumes.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALPANELATMOSPHERICVOLUMES_H
#define AL_ALPANELATMOSPHERICVOLUMES_H

#include "allocalfogmanager.h"
#include "aluirefresh.h"
#include "llpanel.h"

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLLineEditor;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLSpinCtrl;

class ALPanelAtmosphericVolumes final : public LLPanel
{
public:
    ALPanelAtmosphericVolumes();
    ~ALPanelAtmosphericVolumes() override;

    bool postBuild() override;
    void draw() override;
    void onVisibilityChange(bool new_visibility) override;

private:
    using Volume = ALLocalFogManager::Volume;
    using Bank = ALLocalFogManager::Bank;
    static std::string displayLabel(const Volume& volume, S32 index);

    void onSelectionChanged();
    void onAdd();
    void onDuplicate();
    void onDelete();
    void onMoveToCamera();
    void onEditorCommit(LLUICtrl* source);
    void onSliderMouseUp();
    void flushPendingSave();
    void saveBank();
    void refreshBank(bool force);
    void refreshList();
    void refreshListRow(S32 index);
    void refreshEditor(bool force = false);

    LLScrollListCtrl* mVolumeList = nullptr;
    LLButton* mAddButton = nullptr;
    LLButton* mDuplicateButton = nullptr;
    LLButton* mDeleteButton = nullptr;
    LLButton* mMoveToCameraButton = nullptr;
    LLCheckBoxCtrl* mEnabledCheck = nullptr;
    LLLineEditor* mLabelEditor = nullptr;
    LLSpinCtrl* mCenter[3]{};
    LLSpinCtrl* mSize[3]{};
    LLSpinCtrl* mRotation[3]{};
    LLComboBox* mShapeCombo = nullptr;
    LLSliderCtrl* mDensitySlider = nullptr;
    LLSliderCtrl* mFeatherSlider = nullptr;
    LLSliderCtrl* mHeightSlider = nullptr;
    LLSliderCtrl* mNoiseScaleSlider = nullptr;
    LLSliderCtrl* mNoiseSpeedSlider = nullptr;
    LLColorSwatchCtrl* mTintSwatch = nullptr;

    S32 mSelected = 0;
    U32 mSeenRevision = 0;
    Bank mSeenBank;
    bool mRefreshing = false;
    bool mSavePending = false;
};

#endif // AL_ALPANELATMOSPHERICVOLUMES_H
