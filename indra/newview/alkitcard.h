#ifndef AL_KIT_CARD_H
#define AL_KIT_CARD_H
#include "alflowgrid.h"
#include "llpanel.h"
#include "lluiimage.h"
#include <boost/signals2.hpp>
#include <unordered_map>
#include <vector>
class ALKitRow;
class LLContextMenu;
class ALKitSwitch;
class ALLightRigMap;
class LLButton;
class LLComboBox;
class LLControlVariable;
class LLTextBox;
class ALKitOption : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Mandatory<std::string> label;
        Mandatory<LLSD>        value;
        Optional<std::string>  enable_widget, enable_setting;
        Params();
    };
    const LLSD&        optionValue() const { return mValue; }
    void               setOptionValue(const LLSD& value) { mValue = value; }
    const std::string& optionLabel() const { return mLabel; }
    const std::string& enableWidget() const { return mEnableWidget; }
    const std::string& enableSetting() const { return mEnableSetting; }

protected:
    friend class LLUICtrlFactory;
    explicit ALKitOption(const Params& p) :
        LLPanel(p),
        mValue(p.value),
        mLabel(p.label),
        mEnableWidget(p.enable_widget),
        mEnableSetting(p.enable_setting)
    { setMouseOpaque(false); }

private:
    LLSD        mValue;
    std::string mLabel, mEnableWidget, mEnableSetting;
};
class ALKitCard : public LLPanel, public ALFlowGridItem
{
public:
    struct Summary : public LLInitParam::Block<Summary>
    {
        Mandatory<std::string> setting, label;
        Optional<std::string>  kind, unit;
        Optional<S32>          decimals;
        Summary();
    };
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Mandatory<std::string> id;
        Optional<std::string>  title, kind, badge, help, enable_widget, enable_setting, enable_tooltip, sel_label, sel_setting, link_to,
            link_label, target, target_tab, button_label, copy_type;
        Optional<bool>    advanced;
        Multiple<Summary> summary;
        Params();
    };
    ~ALKitCard() override;
    bool                          postBuild() override;
    void                          draw() override;
    void                          reshape(S32, S32, bool = true) override;
    S32                           heightForWidth(S32) const override;
    bool                          spansAllColumns() const override { return false; }
    const std::string&            id() const { return mId; }
    const std::string&            title() const { return mTitle; }
    bool                          setBadge(const std::string& badge);
    bool                          setBadgePlaceholder(const std::string& placeholder,
                                                      const std::string& value);
    bool                          setCodeVisible(bool visible);
    bool                          codeVisible() const { return !mCodeHidden; }
    const std::string&            helpText() const { return mHelp; }
    bool                          hasMore() const { return mMore != nullptr; }
    bool                          moreOpen() const { return mOptionMore.empty() ? mMoreOpen : mOptionMore[mActiveOption]; }
    LLSD                          moreState() const;
    bool                          bodyOpen() const { return mBodyOpen; }
    void                          restoreOpenState(bool, const LLSD&);
    void                          requestLayout();
    bool                          revealRow(ALKitRow*);
    const std::vector<ALKitRow*>& rows() const { return mRows; }
    bool                          changed() const { return mAmberCount > 0; }
    void                          setQuickRows(std::vector<ALKitRow*> rows);

protected:
    friend class LLUICtrlFactory;
    explicit ALKitCard(const Params&);

private:
    struct HeaderLayout
    {
        S32 height = 36, title_width = 0, preset_left = 0, preset_width = 0, badge_width = 0;
    };
    HeaderLayout headerLayout(S32 width) const;
    HeaderLayout mHeaderLayout;
    S32 subHeaderHeight(const std::string& title, S32 width, bool first) const;
    struct SummaryData
    {
        std::string setting, label, kind, unit;
        S32         decimals  = 3;
        LLTextBox*  labelView = nullptr;
        LLTextBox*  valueView = nullptr;
    };
    void        layoutBody(S32);
    void        syncSelector();
    void        updateResetColor();
    void        refreshSummary(S32);
    void        copySettings();
    void        pasteSettings();
    std::string copySourceName() const;
    std::string mId, mTitle, mKind, mBadge, mBadgeTemplate, mEnableWidget, mEnableSetting, mEnableTooltip, mSelectorLabel, mSelectorSetting, mLinkTo,
        mLinkLabel, mTarget, mTargetTab, mButtonLabel, mHelp, mCopyType;
    std::unordered_map<std::string, std::string> mBadgeSubstitutions;
    bool                                            mAdvanced = false, mMoreOpen = false, mLayoutPending = false, mBodyOpen = true,
                                                    mCodeHidden = false;
    S32                                             mMoreCount = 0;
    S32                                             mActiveOption = 0, mAmberCount = 0;
    ALKitSwitch*                                    mEnable        = nullptr;
    ALLightRigMap*                                  mRigMap        = nullptr;
    LLButton*                                       mReset         = nullptr;
    LLButton*                                       mMore          = nullptr;
    LLButton*                                       mOpen          = nullptr;
    LLButton*                                       mAction        = nullptr;
    LLButton*                                       mMenuButton    = nullptr;
    LLComboBox*                                     mSelectorCombo = nullptr;
    LLTextBox*                                      mBodyText      = nullptr;
    LLHandle<LLContextMenu>                         mResetMenuHandle;
    LLHandle<LLContextMenu>                         mCardMenuHandle;
    LLUIImagePtr                                    mOuterImage, mInnerImage;
    std::vector<LLButton*>                          mSegments, mViewSegments;
    std::vector<ALKitSwitch*>                       mOptionSwitches;
    std::vector<LLUICtrl*>                          mHeaderPresets;
    std::vector<std::pair<ALKitRow*, LLUICtrl*>>    mSubPresets;
    std::vector<std::pair<ALKitRow*, LLTextBox*>>   mHeadings;
    std::vector<ALKitRow*>                          mRows;
    std::vector<ALKitRow*>                          mQuickRows;
    std::vector<ALKitOption*>                       mOptions;
    std::vector<SummaryData>                        mSummaries;
    std::vector<bool>                               mVisibility, mOptionMore;
    std::vector<boost::signals2::scoped_connection> mAmberConnections;
    std::vector<boost::signals2::scoped_connection> mQuickAmberConnections;
    boost::signals2::scoped_connection              mSelectorConnection;
};
#endif
