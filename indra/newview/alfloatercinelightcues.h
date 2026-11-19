/**
 * @file alfloatercinelightcues.h
 * @brief Theatrical lighting cue-list editor and transport.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_FLOATER_CINE_LIGHT_CUES_H
#define AL_FLOATER_CINE_LIGHT_CUES_H

#include "alcinelightrigmodel.h"
#include "llfloater.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLScrollListCtrl;
class LLSpinCtrl;
class LLTextBox;

class ALFloaterCineLightCues final : public LLFloater
{
public:
    ALFloaterCineLightCues(const LLSD& key);
    ~ALFloaterCineLightCues() override = default;

    bool postBuild() override;
    void draw() override;
    bool handleKeyHere(KEY key, MASK mask) override;

private:
    S32 selectedCueIndex() const;
    void refreshListNames(bool force = false);
    void refreshCueRows(bool force = false);
    void refreshStatus();
    void loadEditor(const ALCineLightRigModel::Cue& cue);
    ALCineLightRigModel::Cue readEditor(
        const ALCineLightRigModel::Cue& base) const;

    void onListLoad();
    void onListSave();
    void onListDelete();
    void onCueSelected();
    void onCapture();
    void onUpdate();
    void onDeleteCue();
    void onTimecodeMode();
    void onGo();
    void onBack();
    void onGoto(bool snap);
    void onRelease();

    LLComboBox* mListName = nullptr;
    LLCheckBoxCtrl* mTimecodeMode = nullptr;
    LLScrollListCtrl* mCueRows = nullptr;
    LLLineEditor* mLabel = nullptr;
    LLSpinCtrl* mAtSec = nullptr;
    LLSpinCtrl* mFadeSec = nullptr;
    LLSpinCtrl* mDelaySec = nullptr;
    LLSpinCtrl* mFollowSec = nullptr;
    LLComboBox* mProfile = nullptr;
    LLComboBox* mFX = nullptr;
    LLButton* mUpdate = nullptr;
    LLButton* mDeleteCue = nullptr;
    LLTextBox* mStatus = nullptr;
    U64 mSeenRevision = U64(-1);
    S32 mSeenSlot = -1;
    std::string mListSignature;
};

#endif // AL_FLOATER_CINE_LIGHT_CUES_H
