/**
 * @file alfloateravatarlightresponse.h
 * @brief [AvatarLightResponse] Floater: per-avatar Diffuse / Tame reflections /
 *        Brightness / Glow trim, target list, presets and the instrument readout.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALFLOATERAVATARLIGHTRESPONSE_H
#define AL_ALFLOATERAVATARLIGHTRESPONSE_H

#include "alavatarlightresponse.h"
#include "alavatarlightresponsemodel.h"

#include "llfloater.h"
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
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTextBox;

class ALFloaterAvatarLightResponse final : public LLFloater
{
public:
    explicit ALFloaterAvatarLightResponse(const LLSD& key);
    ~ALFloaterAvatarLightResponse() override;

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void draw() override;

private:
    enum EField
    {
        FIELD_DIFFUSE = 0,
        FIELD_TAME,
        FIELD_BRIGHTNESS,
        FIELD_GLOW
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

    std::vector<LLUUID> selectedKeys() const;
    ALLightResponse::Params currentParams(const LLUUID& key) const;
    void applyParams(const LLUUID& key, const ALLightResponse::Params& params, bool save_now);
    void selectKeys(const std::vector<LLUUID>& keys);

    void onSelectionChanged();
    void onSliderCommit(EField field);
    void onSliderMouseUp();
    void onBypass();
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
    std::set<LLUUID>             mPinnedKeys;    // rows requested through onOpen(key) that no other source lists
    bool                         mUpdating = false;   // widgets are being set from code: ignore commits
    bool                         mStatesDirty = true;
    bool                         mSliderDragging = false;   // a slider has mouse capture: never reset it from a refresh
    bool                         mHaveClip = false;
    ALLightResponse::Params      mClip;
    LLFrameTimer                 mRefreshTimer;
    boost::signals2::connection  mChangeConnection;
};

#endif // AL_ALFLOATERAVATARLIGHTRESPONSE_H
