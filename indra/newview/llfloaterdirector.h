/**
 * @file llfloaterdirector.h
 * @brief Director Console: one floater over the whole machinima rig.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Pure view over LLDirectorCast (D1 engine) plus the existing machinima
 * singletons: ACTION/CUT transport with arming + countdown, the persistent
 * cast column, and Move / Animate / Camera / Takes tabs re-homing the
 * Actor Mover, Cinematic Camera (shared ALPanelCineCamParams instance),
 * Flycam Orbit and Flycam Recorder controls. Everything underneath is the
 * same settings/state the standalone floaters use -- no forks.
 */

#ifndef LL_LLFLOATERDIRECTOR_H
#define LL_LLFLOATERDIRECTOR_H

#include "llfloater.h"

#include <string>
#include <vector>

class ALCompassDial;
class LLButton;
class LLContextMenu;
class LLRadioGroup;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTextBox;

class LLFloaterDirector final : public LLFloater
{
public:
    LLFloaterDirector(const LLSD& key);
    bool postBuild() override;
    void draw() override;

private:
    // ---- transport ----
    void onClickAction();               // ACTION, or CUT while counting/running
    void onClickSetMarks();
    void onClickResetMarks();
    void refreshTransport();

    // ---- cast column ----
    void refreshCastList();
    void onCastRightClick(LLUICtrl* ctrl, S32 x, S32 y);
    void onClickAddYou();
    uuid_vec_t selectedCastIds() const;
    LLUUID     firstSelectedCastId() const;
    // context-menu / button ops (act on the list selection)
    void onCastSetSubject(bool subject_a);
    void onCastSetMarkHere();
    void onCastResetToMark();
    void onCastClearLocoAnim();
    void onCastCopyUUID();
    void onCastRemove();

    // ---- Move tab ----
    void onScopeCommit();
    void onDialCommit();
    void onClickWalk();
    void onClickStop();
    void refreshMoveTab();

    // ---- Camera tab ----
    static LLUUID avatarFromSelection();
    void onClickSetSubjectFromSelection(bool subject_a);
    void onClickClearSubject(bool subject_a);
    void refreshCameraTab();

    // ---- Takes tab ----
    void onTakeRecord();
    void onTakePlayPause();
    void onTakeStop();
    void onTakeScrub();
    void refreshTakesTab();

    // ---- status strip ----
    void refreshStatusStrip();

    // cached name for a cast id (member cache -> name cache -> animesh tag)
    static std::string castMemberName(const LLUUID& id);

    // set a tooltip only when it changed (draw()-rate friendly)
    static void setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip);

    // transport
    LLButton* mActionBtn = nullptr;
    LLButton* mCutBtn = nullptr;
    LLButton* mSetMarksBtn = nullptr;
    LLButton* mResetMarksBtn = nullptr;

    // cast column
    LLScrollListCtrl* mCastList = nullptr;
    LLButton*         mRemoveBtn = nullptr;
    LLTextBox*        mCastHint = nullptr;
    LLHandle<LLContextMenu> mCastMenuHandle;
    // per-row last-applied cell state, parallel to the list rows, so cells
    // are only re-set when something actually changed (v2 mover idiom)
    struct CastRowState
    {
        std::string mIcon;
        std::string mName;
        std::string mAB;
        std::string mMark;
        bool        mInWorld = true;
    };
    std::vector<CastRowState> mRowStates;

    // Move tab
    LLRadioGroup*  mScopeRadio = nullptr;
    ALCompassDial* mHeadingDial = nullptr;
    LLButton*      mWalkBtn = nullptr;
    LLButton*      mStopBtn = nullptr;

    // Camera tab
    LLTextBox* mSubjectAText = nullptr;
    LLTextBox* mSubjectBText = nullptr;
    LLButton*  mSetABtn = nullptr;
    LLButton*  mSetBBtn = nullptr;
    LLButton*  mClearABtn = nullptr;
    LLButton*  mClearBBtn = nullptr;

    // Animate tab
    LLTextBox* mAnimateHeader = nullptr;

    // Takes tab
    LLButton*     mTakeRecordBtn = nullptr;
    LLButton*     mTakePlayBtn = nullptr;
    LLButton*     mTakeStopBtn = nullptr;
    LLSliderCtrl* mTakeScrub = nullptr;
    LLTextBox*    mTakeTimeText = nullptr;
    LLTextBox*    mTakeStatusText = nullptr;

    // status strip
    LLTextBox* mStatusStrip = nullptr;
};

#endif // LL_LLFLOATERDIRECTOR_H
