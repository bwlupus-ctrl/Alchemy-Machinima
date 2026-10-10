#ifndef AL_KIT_PAGE_H
#define AL_KIT_PAGE_H
#include "llpanel.h"
#include "lllineeditor.h"
#include "llscrollcontainer.h"
#include "alscrollfit.h"
#include "alkitsearch.h"
#include "aluirefresh.h"
#include <boost/signals2.hpp>
#include <unordered_map>
#include <vector>
class ALFlowGrid;class ALKitRow;class ALKitCard;class ALKitSwitch;class ALKitShell;class ALKitPresetStrip;class LLButton;class LLComboBox;class LLLineEditor;class LLTextBox;
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
    explicit ALKitScroller(const Params& p) : LLScrollContainer(p) {}
};
class ALKitSearchEditor final : public LLLineEditor
{
public:
    struct Params : public LLInitParam::Block<Params, LLLineEditor::Params>
    {
        Params() = default;
    };
    bool handleKeyHere(KEY key, MASK mask) override;
protected:
    friend class LLUICtrlFactory;
    explicit ALKitSearchEditor(const Params& p) : LLLineEditor(p) {}
};
class ALKitDoc:public LLPanel,public ALScrollFit::SelfSizedDocument{public:struct Params:public LLInitParam::Block<Params,LLPanel::Params>{Params()=default;};void setDimmed(bool dim){mDimmed=dim;}void draw()override;protected:friend class LLUICtrlFactory;explicit ALKitDoc(const Params&p):LLPanel(p){}private:bool mDimmed=false;};
class ALKitPresetHost:public LLPanel{public:struct Params:public LLInitParam::Block<Params,LLPanel::Params>{Params()=default;};protected:friend class LLUICtrlFactory;explicit ALKitPresetHost(const Params&p):LLPanel(p){}};
class ALKitPage:public LLPanel
{
public:
 struct Section:public LLInitParam::Block<Section>{Mandatory<std::string>id,label,cards;Section();};struct Mode:public LLInitParam::Block<Mode>{Mandatory<std::string>name,cards;Mode();};
 struct Params:public LLInitParam::Block<Params,LLPanel::Params>{Mandatory<std::string>page;Optional<std::string>summary,master_widget,master_setting,master_label,master_tooltip,modes,mode_setting,mode_widget,mode_on,mode_tooltip,mode_adapter,who_label,who_picker,who_adapter,preset_mode,preset_provider,undo_context,watch_settings,chip_mode;Optional<bool>legacy;Multiple<Section>section;Multiple<Mode>mode;Params();};
 bool postBuild()override;void draw()override;void reshape(S32,S32,bool=true)override;ALKitRow*findRow(const std::string&)const;ALKitRow*findRowBySetting(const std::string&)const;ALKitCard*findCard(const std::string&)const;const std::string&pageKey()const{return mPage;}void requestLayout();void relayoutNow();void applySearch(const ALKitSearch::Hits&);void clearSearch();bool revealRow(ALKitRow*,bool);bool revealCard(ALKitCard*,bool);void setSearchText(const std::string&);void resetPage();bool cycleChip(S32);S32 modeCount()const{return (S32)mModeDefs.size();}S32 modeIndex()const{return mModeIndex;}const std::string&modeName(S32 i)const{return mModeDefs[i].name;}void setModeIndex(S32,bool);void syncEffectiveMode();void rebindRows();void saveState();void restoreState();bool legacy()const{return mLegacy;}ALKitScroller*scroller()const{return mScroller;}LLPanel*legacyHost()const{return mLegacyHost;}
protected:friend class LLUICtrlFactory;explicit ALKitPage(const Params&);
private:
 struct SectionData{std::string id,label;std::vector<std::string>cards;};struct ModeData{std::string name;std::vector<std::string>cards;};
 void applyModeAndChip();void updateMaster();void scrollToView(LLView*);std::string prefix()const;
 std::string mPage,mSummary,mWatchSettings,mMasterWidget,mMasterSetting,mMasterLabel,mMasterTooltip,mModes,mModeSetting,mModeWidget,mModeOn,mModeAdapter,mWhoLabel,mWhoPicker,mWhoAdapter,mPresetMode,mPresetProvider,mChipMode;
 bool mLegacy=false,mLayoutPending=false,mSearching=false,mWhoError=false;S32 mModeIndex=0,mChipIndex=0,mBarLeftEnd=0;U64 mHistoryRevision=~U64(0);ALLiveReadoutClock mReadoutClock{10.f};
 std::vector<SectionData>mSections;std::vector<ModeData>mModeDefs;std::vector<LLButton*>mChips;std::vector<LLPanel*>mSectionHeadings;
 ALKitScroller*mScroller=nullptr;ALKitDoc*mDoc=nullptr;ALFlowGrid*mGrid=nullptr;LLPanel*mBar=nullptr;LLPanel*mBanner=nullptr;LLPanel*mChipsContainer=nullptr;LLPanel*mWhoContainer=nullptr;LLPanel*mMasterBox=nullptr;LLPanel*mLegacyHost=nullptr;ALKitSwitch*mMaster=nullptr;ALKitSwitch*mModeSwitch=nullptr;ALKitShell*mShell=nullptr;LLTextBox*mMasterState=nullptr;LLTextBox*mLegacySummary=nullptr;LLButton*mUndo=nullptr;LLButton*mRedo=nullptr;LLButton*mWhoChip=nullptr;LLLineEditor*mSearch=nullptr;LLComboBox*mWhoCombo=nullptr;
 std::unordered_map<std::string,ALKitRow*>mRows;std::unordered_map<std::string,ALKitCard*>mCards;std::vector<boost::signals2::scoped_connection>mConnections;
};
#endif
