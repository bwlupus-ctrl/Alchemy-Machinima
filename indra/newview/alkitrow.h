#ifndef AL_KIT_ROW_H
#define AL_KIT_ROW_H
#include "llpanel.h"
#include "llframetimer.h"
#include "alkitbinding.h"
#include "alkitqol.h"
#include <boost/signals2.hpp>
#include <array>
#include <memory>
#include <vector>
class LLUICtrl;
class LLTextBox;
class LLButton;
class LLContextMenu;
class ALKitCard;
struct ALKitRowMeta
{
    std::string kind, widget, label, help, keywords, setting, unit, sub, link_to, action, mirror_setting, default_value;
    F32         min_val = 0, max_val = 0, increment = 0.f;
    S32         decimals = 3;
    bool        advanced = false, has_min = false, has_max = false;
    S32         option = -1;
    std::string card_id, page_key;
};
class ALKitRow : public LLPanel
{
public:
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Mandatory<std::string> kind;
        Optional<std::string>  widget, label, label_name, help, keywords, unit, sub, setting, bind, reset_settings, default_value, link_to,
            action, mirror_setting;
        // Undo owner declaration for a code-bound row that keeps widget history (kit_lint enforces it):
        // "global" = state with no selected subject. Subject rows use widget_history=false.
        Optional<std::string> undo_scope;
        Optional<bool> advanced, card_preset, who_picker, hidden, added, widget_history, bulk_reset, unconnected, fill_width, action_row;
        Optional<S32>  row_height, decimals;
        Optional<F32>  min_val, max_val, increment;
        Params();
    };
    ~ALKitRow() override;
    bool        postBuild() override;
    void        draw() override;
    void        onMouseEnter(S32, S32, MASK) override;
    void        onMouseLeave(S32, S32, MASK) override;
    bool        handleKeyHere(KEY, MASK) override;
    bool        handleMouseDown(S32, S32, MASK) override;
    bool        handleMouseUp(S32, S32, MASK) override;
    bool        handleScrollWheel(S32, S32, LLScrollDelta) override;
    bool        handleRightMouseDown(S32, S32, MASK) override;
    S32         heightForWidth(S32) const;
    void        layoutRow(S32);
    void        layoutDialCell(S32);
    void        fitLabel(S32 box_width);
    bool        isEffectivelyVisible() const;
    bool        setControlVisible(bool visible);
    static bool setControlVisible(LLView*, bool);
    void        resolveBinding();
    void        refreshValue();
    void        pollLiveValue();
    using amber_signal_t = boost::signals2::signal<void(bool)>;
    using value_signal_t = boost::signals2::signal<void()>;
    const ALKitRowMeta& meta() const { return mMeta; }
    LLUICtrl*           control() const { return mControl; }
    void                setFilterVisible(bool v) { mFilterVisible = v; }
    bool                filterVisible() const { return mFilterVisible; }
    void                flash()
    {
        mFlashTimer.start();
        mFlashing = true;
    }
    bool                        amber() const;
    boost::signals2::connection setAmberCallback(const amber_signal_t::slot_type& s) { return mAmberSignal.connect(s); }
    boost::signals2::connection setValueCallback(const value_signal_t::slot_type& s) { return mValueSignal.connect(s); }
    // Preferences may keep row reset while opting out of page/card look resets.
    // A row the generator marks unconnected (its control is disabled because nothing reads the
    // setting yet) takes no part in bulk reset, paste, Quick, or Advanced reveal.
    bool                        unconnected() const { return mUnconnected; }
    bool                        bulkResettable() const { return mBulkReset && !mUnconnected && resettable(); }
    bool                        resettable() const { return !mUnconnected && mBinding && mBinding->hasDefault(); }
    LLTextBox*                  valueView() const { return mValueText; }
    void                        refreshValueEditorStyle() { styleValueEditor(mHovered); }
    void                        reset();
    bool cardPreset() const { return mCardPreset; }
    bool whoPicker() const { return mWhoPicker; }
    void setOwnerIdentity(const std::string& page, const std::string& card)
    {
        mMeta.page_key = page;
        mMeta.card_id = card;
    }
    LLSD boundValue() const;
    LLSD boundDefault() const;
    bool setBoundValue(const LLSD&);
    bool canPasteValue(const LLSD&) const;
    bool enabledForEdit() const;
    ALKitQol::Favorite favoriteKey() const;
    ALKitRow* sourceRow() const { return mSourceRow.get(); }
    void refreshFavorite();
    void setSubjectDisplayValues(S32 self_value, S32 subject_a_value,
                                 S32 subject_count);
    static ALKitRow* createProxy(ALKitRow*, const std::string&);

protected:
    friend class LLUICtrlFactory;
    explicit ALKitRow(const Params&);

private:
    friend class ALKitHistory;
    void         replayBoundValue(const LLSD&);
    void         commitCardPreset();
    void         styleValueEditor(bool active);
    void         bindingChanged();
    void         showContextMenu(S32, S32);
    ALKitCard*   card() const;
    ALKitRowMeta mMeta;
    std::string  mBind, mResetSettings, mDefaultValue, mLabelName;
    S32          mRowHeight  = 28;
    bool         mExplicitRowHeight = false;
    bool         mActionRow = false;
    std::vector<std::pair<LLButton*, S32>> mActions;
    bool         mCardPreset = false, mWhoPicker = false, mHidden = false, mAdded = false, mFilterVisible = true, mCodeHidden = false,
                 mHovered = false, mPointerInside = false, mFlashing = false, mAmber = false, mSyncingMirror = false,
                 mWidgetHistory = true, mBulkReset = true, mUnconnected = false, mFillWidth = false;
    LLUICtrl*    mControl = nullptr;
    std::array<LLUICtrl*, 3>           mVec{ { nullptr, nullptr, nullptr } };
    std::vector<std::string>            mSegmentLabels;
    S32                                mVecCount  = 0;
    LLTextBox*                         mLabel     = nullptr;
    LLTextBox*                         mValueText = nullptr;
    LLButton*                          mReset     = nullptr;
    LLButton*                          mFavorite  = nullptr;
    ALKitCard*                         mCard      = nullptr;
    std::unique_ptr<ALKitBinding>      mBinding;
    boost::signals2::scoped_connection mBindingConnection, mFocusConnection, mFocusLostConnection, mMirrorConnection,
        mMirrorCommitConnection, mHistoryConnection, mValueConnection, mActionConnection;
    LLHandle<LLContextMenu> mContextMenu;
    LLHandle<ALKitRow> mSourceRow;
    amber_signal_t mAmberSignal;
    value_signal_t mValueSignal;
    LLFrameTimer   mFlashTimer;
    LLSD           mLastValue, mShownValue;
    bool           mSubjectDisplay = false;
    S32            mSubjectSelfValue = 0;
    S32            mSubjectAValue = 1;
    S32            mSubjectCount = 4;
};
#endif
