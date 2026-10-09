/**
 * @file alpanelavatarlightresponse.h
 * @brief [AvatarLightResponse] Shared panel_avatar_light_response.xml: per-avatar Diffuse / Tame reflections /
 *        Brightness / Glow trim, target list, presets and the instrument readout.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALPANELAVATARLIGHTRESPONSE_H
#define AL_ALPANELAVATARLIGHTRESPONSE_H

#include "alavatarlightresponse.h"
#include "alavatarlightresponsemodel.h"
#include "aluirefresh.h"

#include "llpanel.h"
#include "llframetimer.h"
#include "lluuid.h"

#include <boost/signals2.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLColorSwatchCtrl;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTextBox;

class ALPanelAvatarLightResponse final : public LLPanel
{
public:
    ALPanelAvatarLightResponse();
    ~ALPanelAvatarLightResponse() override;

    bool postBuild() override;
    void focusTarget(const LLUUID& target);
    void onVisibilityChange(bool new_visibility) override;
    void draw() override;

private:
    enum EField
    {
        FIELD_DIFFUSE = 0,
        FIELD_TAME,
        FIELD_BRIGHTNESS,
        FIELD_GLOW,
        FIELD_SIL_AMOUNT
    };

    struct RowInfo
    {
        ALAvatarLightResponse::EKind mKind = ALAvatarLightResponse::KIND_RESIDENT;
        std::string                  mName;
        bool                         mAway = false;
    };

    void collectRows(std::vector<LLUUID>& order, std::map<LLUUID, RowInfo>& info) const;
    void refreshRows(bool force);
    void refreshStateCells();
    void refreshEditors();
    void refreshStatus();
    void refreshBrightnessValue();
    void refreshSilhouetteNote();
    static void onIdle(void* user_data);
    void idleUpdate();

    std::vector<LLUUID> selectedKeys() const;
    ALLightResponse::Params currentParams(const LLUUID& key) const;
    void applyParams(const LLUUID& key, const ALLightResponse::Params& params, bool save_now);
    void selectKeys(const std::vector<LLUUID>& keys);

    void onSelectionChanged();
    void onSliderCommit(EField field);
    void onSliderMouseUp();
    void onBypass();
    void onSilFill(bool save_now, bool custom);
    void onSilFillPreset();
    void onSilCheckbox(bool haze);
    void onApplyPreset();
    void onResetSelected();
    void onInheritSelected();
    void onResetAll();
    void onCopy();
    void onPaste();

    std::string rowName(const LLUUID& key, ALAvatarLightResponse::EKind kind, bool& resolved) const;

    LLScrollListCtrl* mTargetList = nullptr;
    LLCheckBoxCtrl*   mOnlyAdjusted = nullptr;
    LLSliderCtrl*     mDiffuse = nullptr;
    LLSliderCtrl*     mTame = nullptr;
    LLSliderCtrl*     mBrightness = nullptr;
    LLSliderCtrl*     mGlow = nullptr;
    LLCheckBoxCtrl*   mBypass = nullptr;
    LLSliderCtrl*     mSilAmount = nullptr;
    LLColorSwatchCtrl* mSilFill = nullptr;
    LLComboBox*       mSilFillPreset = nullptr;
    LLCheckBoxCtrl*   mSilHaze = nullptr;
    LLCheckBoxCtrl*   mSilKeepRim = nullptr;
    LLTextBox*        mSilNote = nullptr;
    LLComboBox*       mPreset = nullptr;
    LLButton*         mApplyPreset = nullptr;
    LLButton*         mResetSelected = nullptr;
    LLButton*         mInheritSelected = nullptr;
    LLButton*         mCopy = nullptr;
    LLButton*         mPaste = nullptr;
    LLTextBox*        mBrightnessValue = nullptr;   // "+0.00 EV" readout beside the Brightness slider
    LLTextBox*        mStatus = nullptr;
    LLTextBox*        mBudget = nullptr;

    std::vector<LLUUID>          mRowKeys;       // keys of the rows currently in the list, in order
    std::map<LLUUID, RowInfo>    mRowInfo;
    std::map<LLUUID, S32>        mStateVisuals;  // diff guard for dot text/colour writes
    std::set<LLUUID>             mPinnedKeys;    // rows requested through focusTarget() that no other source lists
    bool                         mUpdating = false;   // widgets are being set from code: ignore commits
    bool                         mIdleRowsPending = true;
    bool                         mIdleRowsImmediate = false;
    bool                         mIdleStatePending = true;
    bool                         mIdleEditorsPending = true;
    bool                         mSliderDragging = false;   // a slider has mouse capture: never reset it from a refresh
    U64                          mSeenClipRevision = 0;
    static bool                  mHaveClip;
    static U64                   mClipRevision;
    static ALLightResponse::Params mClip;
    LLFrameTimer                 mRefreshTimer;
    ALRevisionGate               mStoreGate;
    boost::signals2::connection  mChangeConnection;
};

#endif // AL_ALPANELAVATARLIGHTRESPONSE_H
