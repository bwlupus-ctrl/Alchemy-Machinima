#ifndef AL_KIT_SHELL_H
#define AL_KIT_SHELL_H
#include "llpanel.h"
#include "alkithistory.h"
#include "alkithelp.h"
#include "alkitsearch.h"
#include "alkitqol.h"
#include "alscrollfit.h"
#include "llframetimer.h"
#include "lleventtimer.h"
#include "lluuid.h"
#include <boost/signals2.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
class ALKitPage;
class ALKitLazyPage;
class ALKitPopover;
class LLButton;
class LLLineEditor;
class LLPanel;
class LLScrollContainer;
class LLTabContainer;
class LLTextBox;
class ALKitShell : public LLPanel
{
public:
    using uuid_vec_t          = std::vector<LLUUID>;
    using page_built_signal_t = boost::signals2::signal<void(const std::string&, LLPanel*)>;
    using cast_signal_t       = boost::signals2::signal<void(const uuid_vec_t&)>;
    struct RailItem : public LLInitParam::Block<RailItem>
    {
        Mandatory<std::string> group, label, icon;
        Optional<std::string>  master_settings;
        RailItem() : group("group"), label("label"), icon("icon"), master_settings("master_settings", "") {};
    };
    struct ALKitTarget
    {
        std::string page_key, card_id, widget, setting;
        S32         option = -1;
    };
    struct Params : public LLInitParam::Block<Params, LLPanel::Params>
    {
        Optional<std::string> index_file;
        Multiple<RailItem>    rail_item;
        Params() : index_file("index_file", "director_kit_index.xml"), rail_item("rail_item") {};
    };
    ~ALKitShell() override;
    bool                        postBuild() override;
    void                        reshape(S32, S32, bool = true) override;
    void                        draw() override;
    bool                        handleMouseDown(S32, S32, MASK) override;
    bool                        handleScrollWheel(S32, S32, LLScrollDelta) override;
    ALKitHistory&               history() { return mHistory; }
    ALKitHelp&                  help() { return mHelp; }
    ALKitSearch&                search() { return mSearch; }
    static ALKitShell*          find(LLView*);
    bool                        navigateTo(const std::string& path);
    bool                        navigateTo(const ALKitTarget&);
    bool                        selectPage(const std::string& page_name);
    void                        syncSelection();
    void                        setReady();
    bool                        isReady() const { return mReady; }
    LLPanel*                    ensurePageBuilt(const std::string& page_name);
    ALKitPage*                  currentKitPage() const;
    ALKitPage*                  findBuiltPage(const std::string&) const;
    std::string                 currentPageKey() const;
    std::string                 pageKeyAt(S32, S32) const;
    std::string                 pagePathLabel(const std::string&) const;
    std::vector<std::string>    unwrappedPages() const;
    const std::vector<std::string> railGroups() const;
    const std::vector<LLButton*>&  railButtons() const { return mRailButtons; }
    LLPanel*                       roster() const { return mRoster; }
    LLPanel*                       transport() const { return mTransport; }
    LLPanel*                       tabStrip() const { return mTabStrip; }
    const std::vector<LLButton*>&  tabButtons() const { return mTabButtons; }
    LLButton*                      tabPrevious() const { return mTabPrev; }
    LLButton*                      tabNext() const { return mTabNext; }
    void                        startSearchLoad();
    void                        clearSearch();
    bool                        handleSearchKey(KEY key);
    bool                        cycleChip(S32 delta);
    bool                        cyclePage(S32 delta);
    bool                        cycleGroup(S32 delta);
    bool                        closePopover();
    bool                        isFavorite(const std::string&, const std::string&) const;
    void                        toggleFavorite(const std::string&, const std::string&);
    void                        showRowInSearch(ALKitRow*);
    void                        refreshChangedMarkers(ALKitRow* changed_row = nullptr);
    void                        setChangedOnly(bool);
    bool                        changedOnly() const { return mChangedOnly; }
    void                        setOpenPopover(ALKitPopover*);
    boost::signals2::connection setPageBuiltCallback(const page_built_signal_t::slot_type& s) { return mPageBuilt.connect(s); }
    boost::signals2::connection setCastSelectionCallback(const cast_signal_t::slot_type& s) { return mCastSelection.connect(s); }
    void                        emitPageBuilt(const std::string& name, LLPanel* p) { mPageBuilt(name, p); }
    void                        setCastSelection(const uuid_vec_t& ids) { mCurrentCastSelection = ids; mCastSelection(ids); }
    const uuid_vec_t&           castSelection() const { return mCurrentCastSelection; }
    ALScrollFit::Controller&    scrollFit() { return mExternalScrollFit ? *mExternalScrollFit : mScrollFit; }
    void                        setScrollFit(ALScrollFit::Controller* controller) { mExternalScrollFit = controller; }

protected:
    friend class LLUICtrlFactory;
    explicit ALKitShell(const Params& p);

private:
    struct RailData
    {
        std::string group, label, icon, master_settings;
    };
    struct TabData
    {
        std::string group, page, label, master_setting;
        LLButton*   button = nullptr;
    };
    void                                                 buildChrome();
    void                                                 layoutChrome();
    void                                                 refreshModeProxy();
    void                                                 updatePips();
    void                                                 layoutRoster();
    void                                                 pageBuilt(const std::string&, LLPanel*);
    void                                                 runSearch(const std::string&);
    void                                                 queueSearch(const std::string&);
    void                                                 updateSearchResults(const ALKitSearch::Hits&, ALKitPage*);
    void                                                 activateSearchResult();
    bool                                                 openExternalResult(const ALKitSearch::Hit&);
    void                                                 scheduleSearchBatch();
    void                                                 rebuildQuick();
    void                                                 buildChangedIndex();
    void                                                 refreshChangedSetting(const std::string&);
    void                                                 refreshPageMarker(const std::string&);
    struct ChangedEntry
    {
        std::string setting;
        S32         decimals = 3;
        bool        changed = false;
    };
    struct ChangedLocation
    {
        std::string page;
        U32         index = 0;
    };
    std::string                                          mIndexFile;
    std::vector<RailData>                                mRailData;
    ALKitHistory                                         mHistory;
    ALKitHelp                                            mHelp;
    ALKitSearch                                          mSearch;
    ALScrollFit::Controller                              mScrollFit;
    ALScrollFit::Controller*                             mExternalScrollFit = nullptr;
    page_built_signal_t                                  mPageBuilt;
    cast_signal_t                                        mCastSelection;
    uuid_vec_t                                           mCurrentCastSelection;
    LLTabContainer*                                      mDirectorTabs = nullptr;
    LLPanel*                                             mRoster       = nullptr;
    LLPanel*                                             mRail         = nullptr;
    LLPanel*                                             mTabStrip     = nullptr;
    LLPanel*                                             mHelpPanel    = nullptr;
    LLPanel*                                             mHelpStrip    = nullptr;
    LLPanel*                                             mTransport    = nullptr;
    std::vector<LLButton*>                               mRailButtons;
    std::vector<LLButton*>                               mTabButtons;
    std::vector<LLButton*>                               mModeButtons;
    LLButton*                                            mTabPrev   = nullptr;
    LLButton*                                            mTabNext   = nullptr;
    S32                                                  mTabOffset = 0;
    std::vector<TabData>                                 mTabs;
    ALKitPopover*                                        mOpenPopover   = nullptr;
    ALKitPopover*                                        mCastPopover   = nullptr;
    ALKitPopover*                                        mSearchPopover = nullptr;
    LLTextBox*                                           mSearchHeader  = nullptr;
    LLTextBox*                                           mSearchFooter  = nullptr;
    LLScrollContainer*                                   mSearchScroller = nullptr;
    LLPanel*                                             mSearchResultsPanel = nullptr;
    std::vector<LLButton*>                               mSearchButtons;
    ALKitSearch::Hits                                    mSearchHits;
    ALKitSearch::Hit                                     mTopSearchHit;
    bool                                                 mHasTopSearchHit      = false;
    S32                                                  mSearchSelection      = 0;
    bool                                                 mSearchSelectionMoved = false;
    LLEventTimer::weak_t                                 mSearchDebounce;
    std::string                                          mPendingQuery;
    bool                                                 mRailLabels    = true;
    bool                                                 mWideHelp      = true;
    bool                                                 mSearchStarted = false;
    bool                                                 mRosterOpen    = true;
    bool                                                 mReady         = false;
    LLFrameTimer                                         mHighlightTimer;
    LLHandle<LLView>                                     mHighlight;
    LLHandle<LLView>                                     mCurrentPage;
    std::unordered_map<std::string, LLHandle<ALKitPage>> mBuiltPages;
    std::vector<boost::signals2::scoped_connection>      mConnections;
    std::vector<ALKitQol::Favorite>                      mFavorites;
    std::unordered_map<std::string, bool>                mPageChanged;
    std::unordered_map<std::string, std::vector<ChangedEntry>> mPageSettings;
    std::unordered_map<std::string, std::vector<ChangedLocation>> mSettingLocations;
    std::unordered_map<std::string, S32>                 mPageChangedCounts;
    bool                                                 mChangedOnly = false;
    bool                                                 mRebuildingQuick = false;
};
#endif
