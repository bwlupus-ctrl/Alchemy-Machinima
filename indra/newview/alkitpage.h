#ifndef AL_KIT_PAGE_H
#define AL_KIT_PAGE_H
#include "llpanel.h"
#include "lllineeditor.h"
#include "llscrollcontainer.h"
#include "alscrollfit.h"
#include "alkitsearch.h"
#include "aluirefresh.h"
#include <boost/signals2.hpp>
#include <functional>
#include <utility>
#include <unordered_map>
#include <vector>
class ALFlowGrid;
class ALKitRow;
class ALKitCard;
class ALKitSwitch;
class ALKitShell;
class ALKitPresetStrip;
class ALKitPopover;
class LLButton;
class LLComboBox;
class LLCheckBoxCtrl;
class LLLineEditor;
class LLTextBox;
class ALKitScroller : public LLScrollContainer
{
public:
    struct Params : public LLInitParam::Block<Params, LLScrollContainer::Params>
    {
        Params() = default;
    };
    bool handleKeyHere(KEY key, MASK mask) override;

protected:
    friend class LLUICtrlFactory;
    explicit ALKitScroller(const Params& p);
};
class ALKitSearchEditor final : public LLLineEditor
{
public:
    struct Params : public LLInitParam::Block<Params, LLLineEditor::Params>
    {
        Params() = default;
    };
    bool handleKeyHere(KEY key, MASK mask) override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    void draw() override;

protected:
    friend class LLUICtrlFactory;
    explicit ALKitSearchEditor(const Params& p);
};
class ALKitDoc : public LLPanel, public ALScrollFit::SelfSizedDocument
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Params() = default;
    };
    void setDimmed(bool dim) { mDimmed = dim; }
    void draw() override;

protected:
    friend class LLUICtrlFactory;
    explicit ALKitDoc(const Params& p) : LLPanel(p) {}

private:
    bool mDimmed = false;
};
class ALKitPresetHost : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Params() = default;
    };

protected:
    friend class LLUICtrlFactory;
    explicit ALKitPresetHost(const Params& p) : LLPanel(p) {}
};
class ALKitPage : public LLPanel
{
public:
    struct Section : public LLInitParam::Block<Section>
    {
        Mandatory<std::string> id, label, cards;
        Section();
    };
    struct Mode : public LLInitParam::Block<Mode>
    {
        Mandatory<std::string> name, cards;
        Mode();
    };
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Mandatory<std::string> page;
        Optional<std::string>  summary, master_widget, master_setting, master_label, master_tooltip, modes, mode_setting, mode_widget,
            mode_on, mode_tooltip, mode_adapter, who_label, who_picker, who_adapter, preset_mode, preset_provider,
            watch_settings, chip_mode, undo_context;
        Optional<bool>    legacy, actor_target, actor_target_synthetic;
        Multiple<Section> section;
        Multiple<Mode>    mode;
        Params();
    };
    bool               postBuild() override;
    void               buildResetPopover();
    void               draw() override;
    void               reshape(S32, S32, bool = true) override;
    ALKitRow*          findRow(const std::string&) const;
    ALKitRow*          findRowBySetting(const std::string&) const;
    ALKitCard*         findCard(const std::string&) const;
    const std::string& pageKey() const { return mPage; }
    const std::string& summary() const { return mSummary; }
    void               requestLayout();
    void               relayoutNow();
    void               applySearch(const ALKitSearch::Hits&);
    void               clearSearch();
    bool               revealRow(ALKitRow*, bool);
    bool               revealCard(ALKitCard*, bool);
    void               setSearchText(const std::string&);
    void               setChangedOnlyValue(bool);
    void               resetPage();
    bool               cycleChip(S32);
    S32                chipIndex() const { return mChipIndex; }
    bool               setChipIndex(S32);
    bool               revealChip(S32);
    S32                modeCount() const { return (S32)mModeDefs.size(); }
    S32                modeIndex() const { return mModeIndex; }
    const std::string& modeName(S32 i) const { return mModeDefs[i].name; }
    void               setModeIndex(S32, bool, bool = false);
    void               syncEffectiveMode();
    void               rebindRows();
    void               saveState();
    void               restoreState();
    bool               legacy() const { return mLegacy; }
    bool               changed() const;
    void               registerDynamicRow(ALKitRow*);
    void               unregisterDynamicRow(ALKitRow*);
    void               applyLocalFilter(const std::string&, bool);
    void               refreshChangedFilter(ALKitRow*, const std::string&);
    using card_paste_handler_t = std::function<bool(const std::string&, const LLSD&)>;
    void               setCardPasteHandler(card_paste_handler_t handler) { mCardPasteHandler = std::move(handler); }
    bool               pasteCardValues(const std::string& card, const LLSD& values)
                       { return mCardPasteHandler && mCardPasteHandler(card, values); }
    ALKitScroller*     scroller() const { return mScroller; }
    LLPanel*           legacyHost() const { return mLegacyHost; }
    LLPanel*           pageBar() const { return mBar; }
    LLPanel*           whoBand() const { return mWhoContainer; }
    LLButton*          whoChip() const { return mWhoChip; }
    LLLineEditor*      searchEditor() const { return mSearch; }
    ALKitSwitch*       masterSwitch() const { return mMaster; }
    LLButton*          undoButton() const { return mUndo; }
    LLButton*          redoButton() const { return mRedo; }
    LLButton*          resetButton() const { return mResetPage; }
    bool               hasMaster() const { return !mMasterWidget.empty(); }
    bool               hasPreset() const { return mPresetMode != "none"; }
    LLComboBox*        whoCombo() const { return mWhoCombo; }
    bool               whoPinned() const { return mWhoPinned; }
    void               setWhoPin(const LLSD& value);
    LLUUID             actorTarget() const;
    void               undoOnPage(bool redo);
    bool               canUndoOnPage(bool redo);
    void               setOwnedHistory(std::function<U64(bool)> sequence, std::function<void(bool)> replay,
                                      std::function<void()> discard_redo, std::function<bool(bool, U64)> ready,
                                      std::function<void(bool, U64)> discard_step, std::function<U64(bool)> group_sequence)
                       { mOwnedSequence = std::move(sequence); mOwnedReplay = std::move(replay); mOwnedDiscardRedo = std::move(discard_redo);
                         mOwnedReady = std::move(ready); mOwnedDiscardStep = std::move(discard_step);
                         mOwnedGroupSequence = std::move(group_sequence); }
    void               historyEdited(bool owned);
    void               setAvailabilityRefresh(std::function<void()> refresh) { mAvailabilityRefresh = std::move(refresh); }
    void               refreshAvailability() { if (mAvailabilityRefresh) mAvailabilityRefresh(); }
    bool               actorTargetAway() const { return mActorTargetAway; }

protected:
    friend class LLUICtrlFactory;
    explicit ALKitPage(const Params&);

private:
    struct SectionData
    {
        std::string              id, label;
        std::vector<std::string> cards;
    };
    struct ModeData
    {
        std::string              name;
        std::vector<std::string> cards;
    };
    void        applyModeAndChip(bool request_layout = true);
    void        updateMaster();
    bool        updateGateStatus();
    void        scrollToView(LLView*);
    void        performPageReset();
    void        refreshActorTargetAvailability();
    void        setWhoChipLabel(const std::string& label);
    void        layoutWhoChip(S32 page_width);
    void        refreshSubjectCombos();
    std::string prefix() const;
    std::string mPage, mSummary, mWatchSettings, mMasterWidget, mMasterSetting, mMasterLabel, mMasterTooltip, mModes, mModeSetting,
        mModeWidget, mModeOn, mModeAdapter, mWhoLabel, mWhoPicker, mWhoAdapter, mPresetMode, mPresetProvider, mChipMode;
    bool mLegacy = false, mActorTargetPage = false, mActorTargetSynthetic = false, mActorTargetAway = false, mLayoutPending = false, mSearching = false, mWhoError = false, mWhoPinned = false, mRestoringState = true,
         mUpdatingModeSwitch = false;
    LLUUID mActorTarget;
    S32  mModeIndex = 0, mChipIndex = 0, mBarLeftEnd = 0;
    U64  mHistoryRevision = ~U64(0);
    U64  mSubjectComboRevision = ~U64(0);
    enum class SubjectValueSource
    {
        CONTROL,
        DANCE_FOCUS,
        DANCE_BLOCK_CENTRE,
        DANCE_WAVE_FOCUS
    };
    struct SubjectCombo
    {
        LLComboBox* mCombo = nullptr;
        S32 mSelfValue = 0;
        S32 mSubjectAValue = 1;
        S32 mSubjectCount = 4;
        SubjectValueSource mValueSource = SubjectValueSource::CONTROL;
    };
    std::vector<SubjectCombo>                         mSubjectCombos;
    ALLiveReadoutClock                              mReadoutClock{ 10.f };
    std::vector<SectionData>                        mSections;
    std::vector<ModeData>                           mModeDefs;
    std::vector<LLButton*>                          mChips;
    std::vector<LLPanel*>                           mSectionHeadings;
    ALKitScroller*                                  mScroller       = nullptr;
    ALKitDoc*                                       mDoc            = nullptr;
    ALFlowGrid*                                     mGrid           = nullptr;
    LLPanel*                                        mBar            = nullptr;
    LLPanel*                                        mBanner         = nullptr;
    LLPanel*                                        mGateBanner     = nullptr;
    LLPanel*                                        mChipsContainer = nullptr;
    LLPanel*                                        mWhoContainer   = nullptr;
    LLPanel*                                        mMasterBox      = nullptr;
    LLTextBox*                                      mWhoHint        = nullptr;
    LLTextBox*                                      mWhoLabelText   = nullptr;
    LLTextBox*                                      mWhoCaretText   = nullptr;
    LLPanel*                                        mLegacyHost     = nullptr;
    LLPanel*                                        mPathPlayback   = nullptr;
    ALKitSwitch*                                    mMaster         = nullptr;
    ALKitSwitch*                                    mModeSwitch     = nullptr;
    ALKitPopover*                                   mResetPopover   = nullptr;
    LLTextBox*                                      mMasterLabelText = nullptr;
    LLTextBox*                                      mMasterState    = nullptr;
    LLTextBox*                                      mGateText       = nullptr;
    LLTextBox*                                      mLegacySummary  = nullptr;
    LLButton*                                       mUndo           = nullptr;
    LLButton*                                       mRedo           = nullptr;
    LLButton*                                       mPageUndo       = nullptr;
    LLButton*                                       mPageRedo       = nullptr;
    std::function<U64(bool)>                         mOwnedSequence;
    std::function<void(bool)>                        mOwnedReplay;
    std::function<void()>                            mOwnedDiscardRedo;
    std::function<bool(bool, U64)>                   mOwnedReady;
    std::function<void(bool, U64)>                   mOwnedDiscardStep;
    std::function<U64(bool)>                        mOwnedGroupSequence;
    std::function<void()>                            mAvailabilityRefresh;
    LLButton*                                       mResetPage      = nullptr;
    LLButton*                                       mGateButton     = nullptr;
    LLButton*                                       mWhoChip        = nullptr;
    LLLineEditor*                                   mSearch         = nullptr;
    LLCheckBoxCtrl*                                 mChangedOnly    = nullptr;
    LLComboBox*                                     mWhoCombo       = nullptr;
    std::unordered_map<std::string, ALKitRow*>      mRows;
    std::unordered_map<std::string, ALKitCard*>     mCards;
    std::vector<boost::signals2::scoped_connection> mConnections;
    card_paste_handler_t                             mCardPasteHandler;
};
#endif
