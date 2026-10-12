/**
 * Login-screen UI draw performance harness.
 */
#include "llviewerprecompiledheaders.h"

#include "aluiperfselftest.h"

#include "alkitbinding.h"
#include "alkitbar.h"
#include "alkitcard.h"
#include "alkithistory.h"
#include "alkitlazypage.h"
#include "alkitpage.h"
#include "alkitrow.h"
#include "alkitsearch.h"
#include "alkitshell.h"
#include "alkitstyle.h"
#include "alkitswitch.h"
#include "aluirefresh.h"
#include "alcompassdial.h"
#include "alsettingsundohistory.h"
#include "alvcampagemodel.h"
#include "alcinerigrim.h"
#include "alcinelightrigmanager.h"
#include "aldanceball.h"
#include "alfloaterlightbox.h"
#include "aldirectoranimswitcher.h"
#include "aldiopterpresetbank.h"
#include "allocalfogmanager.h"
#include "alobjectpathmover.h"
#include "alpanellensgaze.h"
#include "alpanelpatheditor.h"
#include "alghoststudio.h"
#include "altoolghostedit.h"
#include "altoolpathedit.h"
#include "animationexplorer.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llactormover.h"
#include "llaccordionctrl.h"
#include "llaccordionctrltab.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcolorswatch.h"
#include "llclipboard.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llerrorcontrol.h"
#include "llenvironment.h"
#include "llfile.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfolderview.h"
#include "llfolderviewitem.h"
#include "llfolderviewmodel.h"
#include "llflycamrecorder.h"
#include "llfocusmgr.h"
#include "lliconctrl.h"
#include "llimage.h"
#include "llimagepng.h"
#include "llsdjson.h"
#include "llfontregistry.h"
#include "lllineeditor.h"
#include "llmultisliderctrl.h"
#include "llmodaldialog.h"
#include "llnotifications.h"
#include "llpanel.h"
#include "llpanellogin.h"
#include "llpathcamera.h"
#include "llpresentationtime.h"
#include "lldirectorcast.h"
#include "llpopupview.h"
#include "llprogressview.h"
#include "llviewermenu.h"
#include "llradiogroup.h"
#include "llreflectionmapmanager.h"
#include "llrender.h"
#include "llrootview.h"
#include "llscrollcontainer.h"
#include "llscrolllistctrl.h"
#include "llselectmgr.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltoast.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llversioninfo.h"
#include "llview.h"
#include "llvoavatarself.h"
#include "llvolumemgr.h"
#include "llvovolume.h"
#include "pipeline.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>

namespace
{
class FunctionalControlAccess : public LLUICtrl
{
public:
    static commit_signal_t* commitSignal(LLUICtrl* control)
    {
        using member_t = commit_signal_t* LLUICtrl::*;
        const member_t member = &FunctionalControlAccess::mCommitSignal;
        return control ? control->*member : nullptr;
    }

    static const std::string& functionName(LLUICtrl* control)
    {
        using member_t = std::string LLUICtrl::*;
        const member_t member = &FunctionalControlAccess::mFunctionName;
        return control->*member;
    }
};

class FunctionalLineEditorAccess : public LLLineEditor
{
public:
    static S32 textLeft(const LLLineEditor* editor)
    {
        using member_t = S32 LLLineEditor::*;
        const member_t member = &FunctionalLineEditorAccess::mTextLeftEdge;
        return editor->*member;
    }
    static S32 textRight(const LLLineEditor* editor)
    {
        using member_t = S32 LLLineEditor::*;
        const member_t member = &FunctionalLineEditorAccess::mTextRightEdge;
        return editor->*member;
    }
};

class FunctionalCheckBoxAccess : public LLCheckBoxCtrl
{
public:
    static LLButton* button(LLCheckBoxCtrl* control)
    {
        using member_t = LLButton* LLCheckBoxCtrl::*;
        const member_t member = &FunctionalCheckBoxAccess::mButton;
        return control ? control->*member : nullptr;
    }
};

class FunctionalComboAccess : public LLComboBox
{
public:
    static LLButton* button(LLComboBox* control)
    {
        using member_t = LLButton* LLComboBox::*;
        const member_t member = &FunctionalComboAccess::mButton;
        return control ? control->*member : nullptr;
    }
    static LLScrollListCtrl* list(LLComboBox* control)
    {
        using member_t = LLScrollListCtrl* LLComboBox::*;
        const member_t member = &FunctionalComboAccess::mList;
        return control ? control->*member : nullptr;
    }
};


class InventoryStressFilter final : public LLFolderViewFilter
{
public:
    bool check(const LLFolderViewModelItem* item) override;
    bool checkFolder(const LLFolderViewModelItem*) const override { return true; }
    void setEmptyLookupMessage(const std::string& message) override { mEmptyMessage = message; }
    std::string getEmptyLookupMessage(bool = false) const override { return mEmptyMessage; }
    bool showAllResults() const override { return false; }
    std::string::size_type getStringMatchOffset(LLFolderViewModelItem* item) const override;
    std::string::size_type getFilterStringSize() const override { return mText.size(); }
    bool isActive() const override { return !mText.empty(); }
    bool isModified() const override { return mModified; }
    void clearModified() override { mModified = false; }
    const std::string& getName() const override { return mName; }
    const std::string& getFilterText() override { return mText; }
    void setModified(EFilterModified = FILTER_RESTART) override
    {
        ++mGeneration;
        mFirstSuccess = mGeneration;
        mFirstRequired = mGeneration;
        mModified = true;
    }
    void resetTime(S32 timeout) override
    {
        mTimer.reset();
        mTimer.setTimerExpirySec((F32)timeout / 1000.f);
    }
    bool isTimedOut() override { return mTimer.hasExpired(); }
    bool isDefault() const override { return mText.empty(); }
    bool isNotDefault() const override { return !mText.empty(); }
    void markDefault() override {}
    void resetDefault() override { setFilterSubString(std::string()); }
    S32 getCurrentGeneration() const override { return mGeneration; }
    S32 getFirstSuccessGeneration() const override { return mFirstSuccess; }
    S32 getFirstRequiredGeneration() const override { return mFirstRequired; }

    void setFilterSubString(std::string text)
    {
        LLStringUtil::toLower(text);
        if (text == mText) return;
        mText = std::move(text);
        setModified();
    }

private:
    std::string mName{"inventory_stress"};
    std::string mEmptyMessage{"No matching synthetic items"};
    std::string mText;
    bool mModified = false;
    S32 mGeneration = 1;
    S32 mFirstSuccess = 1;
    S32 mFirstRequired = 1;
    LLTimer mTimer;
};

class InventoryStressItem final : public LLFolderViewModelItemCommon
{
public:
    InventoryStressItem(std::string name, bool folder, LLFolderViewModelInterface& model)
    : LLFolderViewModelItemCommon(model), mName(std::move(name)), mFolder(folder)
    {
        mSearchName = mName;
        LLStringUtil::toLower(mSearchName);
    }

    const std::string& getName() const override { return mName; }
    const std::string& getDisplayName() const override { return mName; }
    const std::string& getSearchableName() const override { return mSearchName; }
    std::string getSearchableDescription() const override { return {}; }
    std::string getSearchableCreatorName() const override { return {}; }
    std::string getSearchableUUIDString() const override { return {}; }
    LLPointer<LLUIImage> getIcon() const override
    {
        return LLUI::getUIImage(mFolder ? "Inv_FolderClosed" : "Inv_Object");
    }
    LLFontGL::StyleFlags getLabelStyle() const override { return LLFontGL::NORMAL; }
    std::string getLabelSuffix() const override { return {}; }
    void openItem() override {}
    void closeItem() override {}
    void selectItem() override {}
    void navigateToFolder(bool = false, bool = false) override {}
    bool isFavorite() const override { return false; }
    bool isItemRenameable() const override { return false; }
    bool renameItem(const std::string&) override { return false; }
    bool isItemMovable() const override { return false; }
    void move(LLFolderViewModelItem*) override {}
    bool isItemRemovable(bool = true) const override { return false; }
    bool isItemInTrash() const override { return false; }
    bool removeItem() override { return false; }
    void removeBatch(std::vector<LLFolderViewModelItem*>&) override {}
    bool isItemCopyable(bool = true) const override { return false; }
    bool copyToClipboard() const override { return false; }
    bool cutToClipboard() override { return false; }
    bool isClipboardPasteable() const override { return false; }
    void pasteFromClipboard() override {}
    void pasteLinkFromClipboard() override {}
    bool isAgentInventory() const override { return false; }
    bool isAgentInventoryRoot() const override { return false; }
    void buildContextMenu(LLMenuGL&, U32) override {}
    bool hasChildren() const override { return getChildrenCount() != 0; }
    bool dragOrDrop(MASK, bool, EDragAndDropType, void*, std::string&) override { return false; }
    bool filter(LLFolderViewFilter& filter) override;
    void setPassedFilter(bool passed, S32 filter_generation,
                         std::string::size_type string_offset = std::string::npos,
                         std::string::size_type string_size = 0) override;
    bool isFolder() const { return mFolder; }

private:
    std::string mName;
    std::string mSearchName;
    bool mFolder;
    bool mPrevPassedAllFilters = false;
};

class InventoryStressSort
{
public:
    bool operator()(const InventoryStressItem* lhs, const InventoryStressItem* rhs) const;
};

class InventoryStressModel final
    : public LLFolderViewModel<InventoryStressSort, InventoryStressItem,
                               InventoryStressItem, InventoryStressFilter>
{
public:
    using base_t = LLFolderViewModel<InventoryStressSort, InventoryStressItem,
                                     InventoryStressItem, InventoryStressFilter>;
    InventoryStressModel() : base_t(new InventoryStressSort(), new InventoryStressFilter()) {}
    bool startDrag(std::vector<LLFolderViewModelItem*>&) override { return false; }
};

bool InventoryStressSort::operator()(const InventoryStressItem* lhs,
                                     const InventoryStressItem* rhs) const
{
    return lhs->getName() < rhs->getName();
}

bool InventoryStressFilter::check(const LLFolderViewModelItem* item)
{
    return mText.empty() || item->getSearchableName().find(mText) != std::string::npos;
}

std::string::size_type InventoryStressFilter::getStringMatchOffset(
    LLFolderViewModelItem* item) const
{
    return mText.empty() ? std::string::npos : item->getSearchableName().find(mText);
}

void InventoryStressItem::setPassedFilter(bool passed, S32 filter_generation,
                                          std::string::size_type string_offset,
                                          std::string::size_type string_size)
{
    const bool generation_skip = mMarkedDirtyGeneration >= 0 &&
        mPrevPassedAllFilters &&
        mMarkedDirtyGeneration < mRootViewModel.getFilter().getFirstSuccessGeneration();
    const S32 last_generation = getLastFilterGeneration();
    LLFolderViewModelItemCommon::setPassedFilter(
        passed, filter_generation, string_offset, string_size);

    const bool before = mPrevPassedAllFilters;
    mPrevPassedAllFilters = passedFilter(filter_generation);
    if (before != mPrevPassedAllFilters || generation_skip ||
        (mPrevPassedAllFilters &&
         last_generation < mRootViewModel.getFilter().getFirstRequiredGeneration()))
    {
        // LLFolderViewModelItemInventory has the same model-to-view contract:
        // a filter-generation visibility change must invalidate cached folder
        // layout. LLFolderViewModelItemCommon only records filter state.
        if (mFolderViewItem && mFolderViewItem->getParentFolder())
        {
            mFolderViewItem->getParentFolder()->requestArrange();
        }
    }
}

bool InventoryStressItem::filter(LLFolderViewFilter& filter)
{
    const S32 generation = filter.getCurrentGeneration();
    bool complete = true;
    for (auto& child : mChildren)
    {
        InventoryStressItem* stress_child = static_cast<InventoryStressItem*>(child.get());
        if (stress_child->getLastFilterGeneration() < generation)
        {
            complete = stress_child->filter(filter);
        }
        if (stress_child->passedFilter(generation))
        {
            mMostFilteredDescendantGeneration = generation;
        }
        if (!complete) break;
    }
    if (complete)
    {
        setPassedFolderFilter(!mFolder || filter.checkFolder(this), generation);
        const bool passed = filter.check(this);
        setPassedFilter(passed, generation, filter.getStringMatchOffset(this),
                        filter.getFilterStringSize());
        complete = !filter.isTimedOut();
    }
    return complete;
}

class InventoryStressFloater final : public LLFloater
{
public:
    explicit InventoryStressFloater(U32 item_count)
    : LLFloater(LLSD()), mModel(std::make_unique<InventoryStressModel>())
    {
        setName("inventory_stress");
        setTitle("Synthetic Inventory Stress");
        reshape(360, 520, false);

        mRootItem = new InventoryStressItem("Synthetic Inventory", true, *mModel);
        LLFolderView::Params root_params(LLUICtrlFactory::getDefaultParams<LLFolderView>());
        root_params.name = "inventory_stress_folder_view";
        root_params.title = "Synthetic Inventory";
        root_params.rect = LLRect(0, 0, 330, 0);
        root_params.parent_panel = this;
        root_params.listener = mRootItem;
        root_params.view_model = mModel.get();
        root_params.root = nullptr;
        root_params.allow_drag = false;
        root_params.options_menu = "";
        mFolderView = LLUICtrlFactory::create<LLFolderView>(root_params);

        LLScrollContainer::Params scroll_params(
            LLUICtrlFactory::getDefaultParams<LLFolderViewScrollContainer>());
        scroll_params.name = "inventory_stress_scroller";
        scroll_params.rect = LLRect(10, 485, 350, 10);
        mScroller = LLUICtrlFactory::create<LLFolderViewScrollContainer>(scroll_params);
        mScroller->setFollowsAll();
        addChild(mScroller);
        mScroller->addChild(mFolderView);
        mFolderView->setScrollContainer(mScroller);
        mFolderView->setFollowsAll();

        LLFolderViewFolder* inbox = nullptr;
        InventoryStressItem* inbox_model = nullptr;
        for (U32 top = 0; top < 4; ++top)
        {
            auto* top_model = new InventoryStressItem(llformat("Folder %02u", top), true, *mModel);
            mRootItem->addChild(top_model);
            LLFolderViewFolder* top_view = addFolder(top_model, mFolderView);
            if (top == 0) { inbox = top_view; inbox_model = top_model; }
            for (U32 middle = 0; middle < 3; ++middle)
            {
                auto* middle_model = new InventoryStressItem(
                    llformat("Folder %02u.%02u", top, middle), true, *mModel);
                top_model->addChild(middle_model);
                LLFolderViewFolder* middle_view = addFolder(middle_model, top_view);
                for (U32 leaf = 0; leaf < 2; ++leaf)
                {
                    auto* leaf_model = new InventoryStressItem(
                        llformat("Folder %02u.%02u.%02u", top, middle, leaf), true, *mModel);
                    middle_model->addChild(leaf_model);
                    addFolder(leaf_model, middle_view);
                }
            }
        }
        for (U32 index = 0; index < item_count; ++index)
        {
            auto* item_model = new InventoryStressItem(
                llformat("Received item %06u alpha", index), false, *mModel);
            inbox_model->addChild(item_model);
            LLFolderViewItem::Params item_params(
                LLUICtrlFactory::getDefaultParams<LLFolderViewItem>());
            item_params.name = item_model->getName();
            item_params.root = mFolderView;
            item_params.listener = item_model;
            LLFolderViewItem* item = LLUICtrlFactory::create<LLFolderViewItem>(item_params);
            item->addToFolder(inbox);
        }
        mFolderView->setOpen(true);
        mFolderView->arrangeAll();
    }

    ~InventoryStressFloater() override
    {
        // Folder views hold references into mModel; destroy the owned subtree
        // while the model is still alive. LLFloater owns its chrome separately.
        delete mScroller;
        mScroller = nullptr;
        mFolderView = nullptr;
    }

    LLFolderViewScrollContainer* scroller() const { return mScroller; }
    void updateFolderView() { mFolderView->update(); }
    bool folderViewReady() const
    {
        const InventoryStressFilter& filter = mModel->getFilter();
        return mFolderView->isInVisibleChain() &&
               mScroller->getScrolledView() == mFolderView &&
               !filter.isModified() &&
               mRootItem->getLastFilterGeneration() >= filter.getCurrentGeneration() &&
               !mFolderView->needsArrange() &&
               mFolderView->hasVisibleChildren() &&
               mFolderView->getRect().getHeight() >
                   mScroller->getVisibleContentRect().getHeight();
    }
    std::string folderViewReadyStatus() const
    {
        const InventoryStressFilter& filter = mModel->getFilter();
        std::ostringstream status;
        status << "visible_chain=" << mFolderView->isInVisibleChain()
               << " scroll_wired=" << (mScroller->getScrolledView() == mFolderView)
               << " filter_modified=" << filter.isModified()
               << " filter_generation=" << filter.getCurrentGeneration()
               << " root_filter_generation=" << mRootItem->getLastFilterGeneration()
               << " needs_arrange=" << mFolderView->needsArrange()
               << " visible_children=" << mFolderView->hasVisibleChildren()
               << " document_height=" << mFolderView->getRect().getHeight()
               << " viewport_height=" << mScroller->getVisibleContentRect().getHeight();
        return status.str();
    }
    void setFilter(const std::string& text)
    {
        mModel->getFilter().setFilterSubString(text);
        mFolderView->arrangeAll();
    }
    void hoverAt(F32 fraction)
    {
        const LLRect& rect = mScroller->getRect();
        const S32 x = rect.mLeft + rect.getWidth() / 2;
        const S32 y = rect.mBottom + ll_round(fraction * (F32)rect.getHeight());
        handleHover(x, y, MASK_NONE);
    }

private:
    LLFolderViewFolder* addFolder(InventoryStressItem* model, LLFolderViewFolder* parent)
    {
        LLFolderViewItem::Params params(
            LLUICtrlFactory::getDefaultParams<LLFolderViewItem>());
        params.name = model->getName();
        params.root = mFolderView;
        params.listener = model;
        LLFolderViewFolder* folder = LLUICtrlFactory::create<LLFolderViewFolder>(params);
        folder->setChildrenInited(true);
        folder->addToFolder(parent);
        folder->setOpen(true);
        return folder;
    }

    std::unique_ptr<InventoryStressModel> mModel;
    InventoryStressItem* mRootItem = nullptr;
    LLFolderView* mFolderView = nullptr;
    LLFolderViewScrollContainer* mScroller = nullptr;
};

struct FrameSample
{
    F64 mUiMs = 0.0;
    U32 mDrawn = 0;
    U32 mCulled = 0;
    U32 mFindCalls = 0;
    U32 mFindVisited = 0;
};

struct TabTarget
{
    std::vector<std::pair<LLHandle<LLTabContainer>, S32>> mChain;
    LLHandle<LLPanel> mPanel;
    std::string mPath;
    std::string mRawPath;
    size_t mScrollBegin = 0;
    size_t mScrollEnd = 0;
};

struct ScrollTarget
{
    std::vector<std::pair<LLHandle<LLTabContainer>, S32>> mChain;
    LLHandle<LLScrollContainer> mScroller;
    std::string mPath;
};

struct ResultRow
{
    std::string mPass;
    std::string mFloater;
    std::string mPath;
    std::string mMeasure;
    U32 mFrames = 0;
    F64 mMean = 0.0;
    F64 mP95 = 0.0;
    F64 mMax = 0.0;
    F64 mDrawn = 0.0;
    F64 mCulled = 0.0;
    F64 mFindCalls = 0.0;
    F64 mFindVisited = 0.0;
    F64 mActionMs = 0.0;
    F64 mFirstFrameMs = 0.0;
    U32 mReadyWaitFrames = 0;
};

struct FunctionalResult
{
    std::string mPage, mControl, mKind, mAction, mExpected, mObserved;
    std::string mStatus, mReason;
    bool mInWorld = false;
};

struct LayoutFinding
{
    std::string mSizePass;
    std::string mFloater;
    U32 mPageIndex = 0;
    std::string mPath;
    std::string mFinding;
    std::string mWidgetA;
    std::string mWidgetB;
    LLRect mRectA;
    LLRect mRectB;
    std::string mDetail;
};

struct AuditCoverage
{
    std::string mSizePass;
    std::string mFloater;
    U32 mPageIndex = 0;
    std::string mPath;
    U32 mEnumerated = 0;
    bool mApplied = false;
};

struct SavedControlState
{
    LLSD mActive;
    LLSD mSaved;
    bool mUnsaved = false;
};

LLSD functionalPathOwnerState(const LLUUID& actor, const LLUUID& destination)
{
    LLActorMover& mover = LLActorMover::instance();
    LLSD state = LLSD::emptyMap();
    state["path"] = mover.pathSceneData(actor);
    state["walking"] = mover.isPathWalking(actor);
    state["following"] = mover.isFollowing(actor);
    state["edit_node"] = mover.getEditNode();
    state["undo_depth"] = mover.undoDepth(actor);
    state["redo_depth"] = mover.redoDepth(actor);
    state["previewing"] = LLPathCamera::instance().isPreviewing();
    state["walk_to_armed"] = ALToolPathEdit::instanceExists() &&
        ALToolPathEdit::getInstance()->isWalkToArmed();
    state["render_camera_position"] = ll_sd_from_vector3d(
        gAgent.getPosGlobalFromAgent(LLViewerCamera::getInstance()->getOrigin()));
    state["render_camera_fov"] = LLViewerCamera::getInstance()->getView();
    if (destination.notNull())
        state["destination_path"] = mover.pathSceneData(destination);
    return state;
}

struct FunctionalDiskSnapshot
{
    std::filesystem::path mRoot;
    bool mExisted = false;
    std::map<std::filesystem::path, std::string> mFiles;
};

class SaveControls final : public LLControlGroup::ApplyFunctor
{
public:
    explicit SaveControls(std::map<std::string, SavedControlState>& saved) : mSaved(saved) {}
    void apply(const std::string& name, LLControlVariable* control) override
    {
        mSaved[name] = { control->get(), control->getSaveValue(), control->hasUnsavedValue() };
    }
private:
    std::map<std::string, SavedControlState>& mSaved;
};

struct AuditSize
{
    enum Kind { DEFAULT, MINIMUM, EXPLICIT };
    Kind mKind = DEFAULT;
    std::string mToken;
    S32 mWidth = 0;
    S32 mHeight = 0;
};

struct KitAuditState
{
    enum Kind { BASE, MODE, CHIP, CARD, SELECTOR, SETTING };
    Kind mKind = BASE;
    std::string mKey;
    LLHandle<LLView> mView;
    S32 mIndex = 0;
    bool mBodyOpen = true;
    bool mMoreOpen = false;
    std::string mSetting;
    LLSD mValue;
};

struct KitCardDefault
{
    LLHandle<LLView> mCard;
    bool mBodyOpen = true;
    bool mMoreOpen = false;
};

struct KitPageDefaults
{
    S32 mMode = 0;
    S32 mChip = 0;
    std::vector<KitCardDefault> mCards;
};

struct AuditLeaf
{
    LLView* mView = nullptr;
    LLScrollContainer* mContainingScroller = nullptr;
    LLRect mRect;
    std::string mPath;
    std::set<std::string> mLineage;
};

using NamePair = std::pair<std::string, std::string>;

NamePair orderedPair(std::string a, std::string b)
{
    return a < b ? NamePair(std::move(a), std::move(b))
                 : NamePair(std::move(b), std::move(a));
}

struct OverlapPolicy
{
    std::set<NamePair> mAllowedPairs;
    std::vector<std::set<std::string>> mExclusiveGroups;

    bool allows(const AuditLeaf& a, const AuditLeaf& b) const
    {
        for (const std::string& aname : a.mLineage)
        {
            for (const std::string& bname : b.mLineage)
            {
                if (mAllowedPairs.count(orderedPair(aname, bname)))
                {
                    return true;
                }
                for (const std::set<std::string>& group : mExclusiveGroups)
                {
                    if (aname != bname && group.count(aname) && group.count(bname))
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }
};

std::vector<std::string> splitWords(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '<' || ch == '>') ch = ' ';
    }
    std::vector<std::string> words;
    LLStringUtil::getTokens(value, words, " \t\r\n");
    words.erase(std::remove_if(words.begin(), words.end(),
        [](const std::string& word) { return word == "--" || word == "-->"; }),
        words.end());
    return words;
}

OverlapPolicy loadOverlapPolicy()
{
    OverlapPolicy policy;
    std::set<std::string> directories;
    const std::string delimiter = gDirUtilp->getDirDelimiter();
    for (const std::string& root : { gDirUtilp->getDefaultSkinDir(),
                                     gDirUtilp->getSkinDir(),
                                     gDirUtilp->getUserDefaultSkinDir(),
                                     gDirUtilp->getUserSkinDir() })
    {
        if (!root.empty())
        {
            directories.insert(root + delimiter + "xui" + delimiter + "en");
        }
    }
    const std::regex annotation(
        R"(xui-check:\s*(allow-overlap|overlay|exclusive|independent)\s+([^<]*))",
        std::regex::icase);
    for (const std::string& directory : directories)
    {
        LLDirIterator iterator(directory, "*.xml");
        std::string filename;
        while (iterator.next(filename))
        {
            llifstream input((directory + delimiter + filename).c_str());
            std::string line;
            while (std::getline(input, line))
            {
                if (line.find("xui-check") == std::string::npos) continue;
                std::smatch match;
                if (!std::regex_search(line, match, annotation)) continue;
                std::string kind = match[1].str();
                LLStringUtil::toLower(kind);
                const std::vector<std::string> names = splitWords(match[2].str());
                if ((kind == "allow-overlap" || kind == "overlay") && names.size() >= 2)
                {
                    policy.mAllowedPairs.insert(orderedPair(names[0], names[1]));
                }
                else if (kind == "exclusive" && names.size() >= 2)
                {
                    policy.mExclusiveGroups.emplace_back(names.begin(), names.end());
                }
                // independent deliberately creates no runtime exception: unlike
                // the static checker, this audit does not infer sibling motion.
            }
        }
    }
    return policy;
}

std::string rectString(const LLRect& rect)
{
    return llformat("[%d,%d,%d,%d]", rect.mLeft, rect.mBottom,
                    rect.mRight, rect.mTop);
}

std::string safeComponent(std::string value)
{
    for (char& ch : value)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!std::isalnum(uch) && ch != '-' && ch != '_') ch = '_';
    }
    while (value.find("__") != std::string::npos)
    {
        LLStringUtil::replaceString(value, "__", "_");
    }
    if (value.empty()) value = "default";
    if (value.size() > 100) value.resize(100);
    return value;
}

bool visibleWithin(LLView* view, LLView* page)
{
    for (LLView* cursor = view; cursor; cursor = cursor->getParent())
    {
        if (!cursor->getVisible()) return false;
        if (cursor == page) return true;
        if (cursor->getRect().getWidth() <= 0 || cursor->getRect().getHeight() <= 0)
        {
            return false;
        }
    }
    return false;
}

bool intersectsScrollViewports(LLView* view, LLFloater* floater)
{
    for (LLView* cursor = view->getParent(); cursor && cursor != floater;
         cursor = cursor->getParent())
    {
        LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(cursor);
        if (!scroller || !scroller->getScrolledView())
            continue;
        LLRect rect;
        if (!view->localRectToOtherView(view->getLocalRect(), &rect, scroller))
            return false;
        const LLRect visible = scroller->getVisibleContentRect();
        if (llmin(rect.mRight, visible.mRight) <= llmax(rect.mLeft, visible.mLeft) ||
            llmin(rect.mTop, visible.mTop) <= llmax(rect.mBottom, visible.mBottom))
            return false;
    }
    return true;
}

std::string relativePath(LLView* view, LLView* page)
{
    std::vector<std::string> parts;
    for (LLView* cursor = view; cursor && cursor != page; cursor = cursor->getParent())
    {
        parts.push_back(cursor->getName().empty() ? "<unnamed>" : cursor->getName());
    }
    std::reverse(parts.begin(), parts.end());
    std::ostringstream out;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i) out << '/';
        out << parts[i];
    }
    return out.str();
}

bool isAuditLeaf(LLView* view)
{
    if (dynamic_cast<LLPanel*>(view) || dynamic_cast<LLScrollContainer*>(view))
    {
        return false;
    }
    if (dynamic_cast<LLButton*>(view) || dynamic_cast<LLTextBox*>(view) ||
        dynamic_cast<LLIconCtrl*>(view) || dynamic_cast<LLScrollListCtrl*>(view))
    {
        return true;
    }
    return dynamic_cast<LLUICtrl*>(view) && view->getChildCount() == 0;
}

void collectAuditViews(LLView* root, LLView* page,
                       std::vector<LLView*>& views,
                       std::vector<AuditLeaf>& leaves)
{
    if (!root || !visibleWithin(root, page)) return;
    views.push_back(root);
    if (root != page && isAuditLeaf(root))
    {
        AuditLeaf leaf;
        leaf.mView = root;
        leaf.mPath = relativePath(root, page);
        root->localRectToOtherView(root->getLocalRect(), &leaf.mRect, page);
        if (leaf.mRect.getWidth() > 0 && leaf.mRect.getHeight() > 0)
        {
            for (LLView* cursor = root; cursor && cursor != page; cursor = cursor->getParent())
            {
                if (!cursor->getName().empty()) leaf.mLineage.insert(cursor->getName());
            }
            leaves.push_back(std::move(leaf));
        }
    }
    for (LLView* child : *root->getChildList())
    {
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(root))
        {
            bool is_tab_page = false;
            for (S32 i = 0; i < tabs->getTabCount(); ++i)
            {
                if (child == tabs->getPanelByIndex(i))
                {
                    is_tab_page = true;
                    break;
                }
            }
            if (is_tab_page) continue;
        }
        // A scroll container's bars and corner are implementation chrome.  Its
        // document is the page content that the audit must inspect in full.
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(root))
        {
            if (child != scroller->getScrolledView()) continue;
        }
        collectAuditViews(child, page, views, leaves);
    }
}

LLView* nearestCommonAncestor(LLView* a, LLView* b, LLView* page)
{
    std::set<LLView*> ancestors;
    for (LLView* cursor = a->getParent(); cursor && cursor != page; cursor = cursor->getParent())
    {
        ancestors.insert(cursor);
    }
    for (LLView* cursor = b->getParent(); cursor && cursor != page; cursor = cursor->getParent())
    {
        if (ancestors.count(cursor)) return cursor;
    }
    return nullptr;
}

bool isRealCompositeControl(LLView* view)
{
    return dynamic_cast<LLUICtrl*>(view) &&
        !dynamic_cast<LLPanel*>(view) &&
        !dynamic_cast<LLScrollContainer*>(view) &&
        !dynamic_cast<LLTabContainer*>(view) &&
        !dynamic_cast<LLAccordionCtrl*>(view);
}

LLScrollContainer* containingScroller(LLView* view, LLFloater* floater)
{
    for (LLView* cursor = view->getParent(); cursor && cursor != floater;
         cursor = cursor->getParent())
    {
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(cursor))
        {
            LLView* document = scroller->getScrolledView();
            if (document && (view == document || view->hasAncestor(document)))
            {
                return scroller;
            }
        }
    }
    return nullptr;
}

void containingScrollAxes(LLView* view, LLFloater* floater,
                          bool& horizontal, bool& vertical)
{
    horizontal = false;
    vertical = false;
    for (LLView* cursor = view->getParent(); cursor && cursor != floater;
         cursor = cursor->getParent())
    {
        LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(cursor);
        if (!scroller || !scroller->getScrolledView()) continue;
        const LLRect visible = scroller->getVisibleContentRect();
        const LLRect document = scroller->getScrolledView()->getLocalRect();
        horizontal = horizontal || document.getWidth() > visible.getWidth() + 1;
        vertical = vertical || document.getHeight() > visible.getHeight() + 1;
    }
}

S32 longestLineWidth(const LLFontGL* font, const std::string& text)
{
    S32 width = 0;
    size_t begin = 0;
    while (begin <= text.size())
    {
        const size_t end = text.find('\n', begin);
        width = llmax(width, font->getWidth(
            text.substr(begin, end == std::string::npos ? end : end - begin)));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return width;
}

LLRect overlapRect(const AuditLeaf& leaf)
{
    LLRect rect = leaf.mRect;
    LLTextBox* text = dynamic_cast<LLTextBox*>(leaf.mView);
    if (!text || !text->getFont() || text->getText().empty()) return rect;

    const S32 width = llmin(rect.getWidth(),
                            longestLineWidth(text->getFont(), text->getText()) +
                                text->getHPad());
    S32 left = rect.mLeft;
    if (text->getHAlign() == LLFontGL::RIGHT)
    {
        left = rect.mRight - width;
    }
    else if (text->getHAlign() == LLFontGL::HCENTER)
    {
        left = rect.mLeft + (rect.getWidth() - width) / 2;
    }
    rect.mLeft = llclamp(left, rect.mLeft, rect.mRight);
    rect.mRight = llclamp(rect.mLeft + width, rect.mLeft, rect.mRight);
    return rect;
}

bool sActive = false;
bool sSuppressSettingsSave = false;
bool sLayoutAuditActive = false;
F64 sPendingUiMs = 0.0;
U32 sPendingUiCalls = 0;
U32 sPendingDrawn = 0;
U32 sPendingCulled = 0;
U32 sPendingFindCalls = 0;
U32 sPendingFindVisited = 0;

struct KitTextRun
{
    LLHandle<LLView> mView;
    LLHandle<LLView> mOwner;
    std::string      mText;
    LLRect           mRect;
};

struct KitInteractiveRect
{
    LLHandle<LLView> mView;
    LLRect           mRect;
};

U32                             sKitTextFrame = ~0u;
std::vector<KitTextRun>         sKitTextRuns;
std::vector<KitInteractiveRect> sKitInteractiveRects;
std::set<const LLView*>         sKitRecordedViews;

void beginKitTextFrame()
{
    const U32 frame = LLFrameTimer::getFrameCount();
    if (sKitTextFrame == frame)
        return;
    sKitTextFrame = frame;
    sKitTextRuns.clear();
    sKitInteractiveRects.clear();
    sKitRecordedViews.clear();
}

bool substantialIntersection(const LLRect& a, const LLRect& b)
{
    return llmin(a.mRight, b.mRight) - llmax(a.mLeft, b.mLeft) > 2 &&
           llmin(a.mTop, b.mTop) - llmax(a.mBottom, b.mBottom) > 2;
}

bool clipRectTo(LLRect& rect, const LLRect& clip)
{
    rect.mLeft   = llmax(rect.mLeft, clip.mLeft);
    rect.mRight  = llmin(rect.mRight, clip.mRight);
    rect.mBottom = llmax(rect.mBottom, clip.mBottom);
    rect.mTop    = llmin(rect.mTop, clip.mTop);
    return rect.getWidth() > 0 && rect.getHeight() > 0;
}

std::string csv(const std::string& value)
{
    std::string escaped = value;
    LLStringUtil::replaceString(escaped, "\"", "\"\"");
    return "\"" + escaped + "\"";
}

void collectEmbeddedCoverage(LLView* view, std::set<std::string>& coverage)
{
    if (!view)
    {
        return;
    }
    const std::string& name = view->getName();
    for (const char* marker : { "atmospheric_volumes", "vcam", "dof", "lens_looks" })
    {
        if (name.find(marker) != std::string::npos)
        {
            coverage.insert(marker);
        }
    }
    for (LLView* child : *view->getChildList())
    {
        collectEmbeddedCoverage(child, coverage);
    }
}

std::string tabPath(const std::string& raw_path, LLPanel* panel)
{
    std::set<std::string> coverage;
    collectEmbeddedCoverage(panel, coverage);
    if (coverage.empty())
    {
        return raw_path;
    }
    std::ostringstream out;
    out << raw_path << " [";
    bool first = true;
    for (const std::string& marker : coverage)
    {
        if (!first) out << ',';
        first = false;
        out << marker;
    }
    out << ']';
    return out.str();
}

void collectTabs(LLView* view,
                 const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& ancestors,
                 const std::string& prefix,
                 std::vector<TabTarget>& out)
{
    if (!view)
    {
        return;
    }
    for (LLView* child : *view->getChildList())
    {
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(child))
        {
            for (S32 i = 0; i < tabs->getTabCount(); ++i)
            {
                LLPanel* panel = tabs->getPanelByIndex(i);
                if (!panel)
                {
                    continue;
                }
                TabTarget target;
                target.mChain = ancestors;
                target.mChain.emplace_back(
                    tabs->getDerivedHandle<LLTabContainer>(), i);
                target.mPanel = panel->getHandle();
                const std::string raw_path = prefix.empty()
                    ? panel->getName() : prefix + "/" + panel->getName();
                target.mPath = tabPath(raw_path, panel);
                target.mRawPath = raw_path;
                out.push_back(target);
                collectTabs(panel, target.mChain, raw_path, out);
            }
        }
        else
        {
            collectTabs(child, ancestors, prefix, out);
        }
    }
}

void collectScrollers(LLView* view,
                      const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& chain,
                      const std::string& prefix,
                      std::vector<ScrollTarget>& out)
{
    if (!view)
    {
        return;
    }
    for (LLView* child : *view->getChildList())
    {
        // Nested tab pages get their own target/ancestor chain.
        if (dynamic_cast<LLTabContainer*>(child))
        {
            continue;
        }
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(child))
        {
            out.push_back({ chain,
                scroller->getDerivedHandle<LLScrollContainer>(),
                prefix.empty() ? scroller->getName()
                               : prefix + "/" + scroller->getName() });
        }
        collectScrollers(child, chain, prefix, out);
    }
}

bool applyChain(
    const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& chain)
{
    for (const auto& entry : chain)
    {
        LLTabContainer* tabs = entry.first.get();
        if (!tabs)
        {
            return false;
        }
        tabs->selectTab(entry.second);
    }
    return true;
}

bool chainAlive(
    const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& chain)
{
    return std::all_of(chain.begin(), chain.end(),
        [](const auto& entry) { return entry.first.get() != nullptr; });
}

void collectAllScrollers(LLView* view,
                         std::vector<LLHandle<LLScrollContainer>>& out)
{
    if (!view)
    {
        return;
    }
    for (LLView* child : *view->getChildList())
    {
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(child))
        {
            out.push_back(scroller->getDerivedHandle<LLScrollContainer>());
        }
        collectAllScrollers(child, out);
    }
}

template<typename T>
void collectViewsOfType(LLView* view, std::vector<T*>& out)
{
    if (!view) return;
    if (T* typed = dynamic_cast<T*>(view)) out.push_back(typed);
    for (LLView* child : *view->getChildList())
    {
        collectViewsOfType<T>(child, out);
    }
}

template<typename T>
T* findFirstView(LLView* view)
{
    if (!view) return nullptr;
    if (T* typed = dynamic_cast<T*>(view)) return typed;
    for (LLView* child : *view->getChildList())
    {
        if (T* found = findFirstView<T>(child)) return found;
    }
    return nullptr;
}

std::vector<AuditSize> parseAuditSizes(const std::string& value)
{
    std::vector<AuditSize> sizes;
    if (value.empty())
    {
        sizes.push_back({ AuditSize::DEFAULT, "default", 0, 0 });
        sizes.push_back({ AuditSize::MINIMUM, "min", 0, 0 });
        return sizes;
    }

    std::vector<std::string> tokens;
    LLStringUtil::getTokens(value, tokens, ",");
    const std::regex dimensions("^([0-9]+)[xX]([0-9]+)$");
    for (std::string token : tokens)
    {
        LLStringUtil::trim(token);
        std::string lower = token;
        LLStringUtil::toLower(lower);
        if (lower == "default")
        {
            sizes.push_back({ AuditSize::DEFAULT, "default", 0, 0 });
        }
        else if (lower == "min")
        {
            sizes.push_back({ AuditSize::MINIMUM, "min", 0, 0 });
        }
        else
        {
            std::smatch match;
            if (!std::regex_match(token, match, dimensions))
            {
                LL_WARNS("UILayoutAudit") << "Ignoring invalid UILayoutAuditSizes token: "
                    << token << LL_ENDL;
                continue;
            }
            const S32 width = atoi(match[1].str().c_str());
            const S32 height = atoi(match[2].str().c_str());
            if (width <= 0 || height <= 0)
            {
                LL_WARNS("UILayoutAudit") << "Ignoring invalid UILayoutAuditSizes token: "
                    << token << LL_ENDL;
                continue;
            }
            sizes.push_back({ AuditSize::EXPLICIT,
                llformat("%dx%d", width, height), width, height });
        }
    }
    if (sizes.empty())
    {
        LL_WARNS("UILayoutAudit") << "UILayoutAuditSizes contained no valid tokens; "
            "using default,minimum" << LL_ENDL;
        sizes.push_back({ AuditSize::DEFAULT, "default", 0, 0 });
        sizes.push_back({ AuditSize::MINIMUM, "min", 0, 0 });
    }
    return sizes;
}

class Runner
{
public:
    Runner(std::string request, U32 frames, bool ab, bool layout,
           U32 inventory_items, std::string audit_sizes)
    : mRequest(std::move(request)),
      mFrames(llmax<U32>(frames, 2)),
      mAB(ab),
      mLayout(layout),
      mInventoryItems(inventory_items),
      mPriorCull(gSavedSettings.getBOOL("UIFolderViewCull")),
      mDirectorV2(gSavedSettings.getBOOL("UIPerfDirectorV2")),
      mAuditSizes(parseAuditSizes(audit_sizes))
    {
        static const std::string functional_prefix = "functional|";
        static const std::string inworld_prefix = "functional-inworld|";
        const bool inworld_request =
            mRequest.compare(0, inworld_prefix.size(), inworld_prefix) == 0;
        if (inworld_request ||
            mRequest.compare(0, functional_prefix.size(), functional_prefix) == 0)
        {
            mFunctional = true;
            mInWorld = inworld_request;
            std::vector<std::string> fields;
            LLStringUtil::getTokens(mRequest.substr(
                inworld_request ? inworld_prefix.size() : functional_prefix.size()), fields, "|");
            if (!fields.empty()) mFunctionalManifestPath = fields.front();
            for (size_t i = 1; i < fields.size(); ++i)
            {
                const size_t equals = fields[i].find('=');
                const std::string key = fields[i].substr(0, equals);
                const std::string value = equals == std::string::npos ? "" : fields[i].substr(equals + 1);
                if (key == "pages") LLStringUtil::getTokens(value, mFunctionalPageFilters, ",");
                else if (key == "rows" && !value.empty())
                {
                    try { mFunctionalRowFilter = std::regex(value); mFunctionalHaveRowFilter = true; }
                    catch (const std::regex_error&) { mFunctionalFilterError = "invalid rows regex"; }
                }
                else if (key == "version") mFunctionalVersionFilter = value;
                else if (key == "shots") mFunctionalShots = value == "1";
                else if (key == "dry_reveal") mFunctionalDryReveal = value == "1";
                else if (key == "account") mFunctionalAccount = value;
                else if (key == "watchdog_seconds")
                {
                    S32 seconds = 0;
                    if (LLStringUtil::convertToS32(value, seconds) && seconds > 0)
                        mFunctionalWatchdogSeconds = seconds;
                }
            }
            mRequest = "director";
            captureFunctionalUserFiles();
        }
        if (mDirectorV2 || mFunctional)
        {
            SaveControls save(mSettingsSnapshot);
            gSavedSettings.applyToAll(&save);
        }
        static const std::vector<std::string> all_targets = {
            "director", "lightbox", "exposure_hub", "dance_ball",
            "cine_light_rig", "env_adjust_snapshot", "actor_gaze",
            "avatar_light_response", "flycam_recorder", "phototools",
            "cinematic_camera", "ghost_studio", "ultimate_diopter",
            "pose_polish", "inventory_stress"
        };
        if (mRequest == "all")
        {
            mTargets = all_targets;
        }
        else
        {
            LLStringUtil::getTokens(mRequest, mTargets, ",");
            for (std::string& target : mTargets)
            {
                LLStringUtil::trim(target);
            }
        }

        mInventoryAB = mAB && mInventoryItems > 0 &&
            std::find(mTargets.begin(), mTargets.end(), "inventory_stress") != mTargets.end();

        mCullOn = mAB ? true : mPriorCull;
        if (mAB)
        {
            gSavedSettings.setBOOL("UIFolderViewCull", true);
        }

        for (const std::string& target : mTargets)
        {
            mWasVisible[target] = LLFloaterReg::instanceVisible(target);
            if (mWasVisible[target])
            {
                LLFloaterReg::hideInstance(target);
            }
        }
        mSettle.reset();
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest waiting 3 seconds "
            << (mInWorld ? "for saved-credential login" : "at login screen") << LL_ENDL;
        if (mLayout)
        {
            mOverlapPolicy = loadOverlapPolicy();
            mAuditRoot = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "ui_audit");
            std::error_code remove_error;
            std::filesystem::remove_all(mAuditRoot, remove_error);
            if (remove_error)
            {
                LL_WARNS("UILayoutAudit") << "Unable to clear " << mAuditRoot
                    << ": " << remove_error.message() << LL_ENDL;
            }
            LLFile::mkdir(mAuditRoot);
        }
    }

    // No restore() here: the static owner can be destroyed after viewer cleanup has
    // torn down floaters/settings. FINISH and abortForLogin() restore explicitly.
    ~Runner() = default;

    // The user started a real login mid-test: put everything back and stop.
    void abortForLogin()
    {
        LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest aborted: login started" << LL_ENDL;
        if (LLFloater* floater = mFloater.get())
        {
            if (mOriginalFloaterRect.getWidth() > 0 && mOriginalFloaterRect.getHeight() > 0)
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            if (isInventoryStress())
            {
                floater->closeFloater(false);
                mFloater = LLHandle<LLFloater>();
            }
        }
        if (mFunctional) restoreFunctionalAction();
        if (mFunctional) finishFunctionalShaderBatch();
        restoreDirectorAuditSettings();
        restoreInitialState();
        restore();
        if (mFunctional)
        {
            restoreFunctionalRigState(false);
            restoreFunctionalUserFiles();
        }
        sActive = false;
        sSuppressSettingsSave = false;
        sLayoutAuditActive = false;
    }

    bool tick() { return mFunctional ? functionalStep() : step(); }
    bool inWorld() const { return mInWorld; }

    void captureAfterRender()
    {
        if (!mCaptureRequested || mCapturedImage.notNull()) return;
        LLFloater* floater = mFunctional && mFunctionalHostFloater.get() ?
            mFunctionalHostFloater.get() : mFloater.get();
        if (!floater || !gViewerWindow) return;

        LLRect screen = floater->calcScreenRect();
        const LLVector2& scale = gViewerWindow->getDisplayScale();
        LLRect raw(ll_round((F32)screen.mLeft * scale.mV[VX]),
                   ll_round((F32)screen.mTop * scale.mV[VY]),
                   ll_round((F32)screen.mRight * scale.mV[VX]),
                   ll_round((F32)screen.mBottom * scale.mV[VY]));
        const LLRect window = gViewerWindow->getWindowRectRaw();
        raw.mLeft = llclamp(raw.mLeft, window.mLeft, window.mRight);
        raw.mRight = llclamp(raw.mRight, window.mLeft, window.mRight);
        raw.mBottom = llclamp(raw.mBottom, window.mBottom, window.mTop);
        raw.mTop = llclamp(raw.mTop, window.mBottom, window.mTop);
        if (raw.getWidth() <= 0 || raw.getHeight() <= 0)
        {
            mCaptureRequested = false;
            mCaptureFailed = true;
            return;
        }

        LLPointer<LLImageRaw> image = new LLImageRaw(
            (U16)raw.getWidth(), (U16)raw.getHeight(), 3);
        GLint saved_read_fbo = 0;
        GLint saved_pack_buffer = 0;
        GLint saved_read_buffer = GL_BACK;
        GLint saved_pack_alignment = 4;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &saved_read_fbo);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &saved_pack_buffer);
        glGetIntegerv(GL_READ_BUFFER, &saved_read_buffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &saved_pack_alignment);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        {
            LLImageDataLock lock(image);
            glReadPixels(raw.mLeft, raw.mBottom, raw.getWidth(), raw.getHeight(),
                         GL_RGB, GL_UNSIGNED_BYTE, image->getData());
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(saved_pack_buffer));
        glPixelStorei(GL_PACK_ALIGNMENT, saved_pack_alignment);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(saved_read_fbo));
        glReadBuffer(static_cast<GLenum>(saved_read_buffer));
        mCapturedImage = image;
        mCaptureRequested = false;
    }

private:
    static std::string functionalValue(const LLSD& value)
    {
        return value.isDefined() ? LlsdToJson(value) : std::string();
    }

    static bool functionalEqual(const LLSD& actual, const LLSD& expected,
                                const std::string& type, const LLSD& increment = LLSD())
    {
        if (type == "Boolean") return actual.asBoolean() == expected.asBoolean();
        if (type == "S32" || type == "U32") return actual.asInteger() == expected.asInteger();
        if (type == "F32" || type == "F64")
        {
            const F64 a = actual.asReal();
            const F64 b = expected.asReal();
            const F64 step = increment.isDefined() ? llabs(increment.asReal()) : 0.0;
            const F64 epsilon = llmax(1e-9, llmax(llabs(b) * 1e-7, step * 1e-4));
            return std::isfinite(a) && std::isfinite(b) && llabs(a - b) <= epsilon;
        }
        if (type == "String") return actual.asString() == expected.asString();
        return actual == expected;
    }

    static LLSD functionalGazeField(const LLActorMover::GazeTarget& target,
                                    const std::string& field)
    {
        if (field == "microlife") return target.mMicroLifeOverride;
        if (field == "variation") return target.mVariationOverride;
        if (field == "break_frequency") return target.mBreakFrequencyOverride;
        if (field == "ease_acquire") return target.mEaseAcquireOverride;
        if (field == "ease_release") return target.mEaseReleaseOverride;
        if (field == "deadzone") return target.mDeadZoneDegOverride;
        if (field == "priority") return target.mGazePriorityOverride;
        if (field == "anim_priority") return target.mAnimPriorityOverride;
        if (field == "camera_roll") return target.mCameraRollOverride;
        if (field == "exaggerate") return target.mExaggerateOverride;
        if (field == "camera_mode") return target.mCameraModeOverride;
        if (field == "composition") return target.mCompositionOverride;
        if (field == "composition_mix") return target.mCompositionMixOverride;
        if (field == "chest_share") return target.mChestShareOverride;
        if (field == "limit_mode") return target.mLimitProfileModeOverride;
        if (field == "lean_curve") return target.mLeanCurveOverride;
        if (field == "lean_threshold") return target.mLeanThresholdDegOverride;
        if (field == "lean_softness") return target.mLeanSoftnessDegOverride;
        if (field == "lean_max") return target.mLeanMaxDegOverride;
        if (field == "limit_profile")
        {
            const ALGazeMath::AnatomicalLimitProfile& p = target.mLimitProfile;
            LLSD values = LLSD::emptyArray();
            values.append(p.mEyeYawDeg); values.append(p.mEyePitchDeg);
            values.append(p.mHeadYawDeg); values.append(p.mHeadPitchDeg);
            values.append(p.mNeckYawDeg); values.append(p.mNeckPitchDeg);
            values.append(p.mSpineYawDeg); values.append(p.mSpinePitchDeg);
            values.append(p.mHipsYawDeg); values.append(p.mHipsPitchDeg);
            values.append(p.mEyeApplyYawDeg); values.append(p.mEyeApplyPitchDeg);
            values.append(p.mEyeRadialDeg); values.append(p.mEyePitchUpDeg);
            values.append(p.mHeadPitchUpDeg); values.append(p.mNeckPitchUpDeg);
            values.append(p.mSpinePitchUpDeg); values.append(p.mHipsPitchUpDeg);
            values.append(p.mEyeApplyPitchUpDeg);
            return values;
        }
        return LLSD();
    }

    static void functionalSetGazeField(LLActorMover::GazeTarget& target,
                                       const std::string& field)
    {
        if (field == "microlife") target.mMicroLifeOverride = .25f;
        else if (field == "variation") target.mVariationOverride = .25f;
        else if (field == "break_frequency") target.mBreakFrequencyOverride = .25f;
        else if (field == "ease_acquire") target.mEaseAcquireOverride = .25f;
        else if (field == "ease_release") target.mEaseReleaseOverride = .25f;
        else if (field == "deadzone") target.mDeadZoneDegOverride = 2.f;
        else if (field == "priority") target.mGazePriorityOverride = 0;
        else if (field == "anim_priority") target.mAnimPriorityOverride = 0;
        else if (field == "camera_roll") target.mCameraRollOverride = 1.f;
        else if (field == "exaggerate") target.mExaggerateOverride = 1.f;
        else if (field == "camera_mode") target.mCameraModeOverride = 0;
        else if (field == "composition") target.mCompositionOverride = 0;
        else if (field == "composition_mix") target.mCompositionMixOverride = .25f;
        else if (field == "chest_share") target.mChestShareOverride = .25f;
        else if (field == "limit_mode") target.mLimitProfileModeOverride = 0;
        else if (field == "lean_curve") target.mLeanCurveOverride = 0;
        else if (field == "lean_threshold") target.mLeanThresholdDegOverride = 10.f;
        else if (field == "lean_softness") target.mLeanSoftnessDegOverride = 10.f;
        else if (field == "lean_max") target.mLeanMaxDegOverride = 10.f;
        else if (field == "limit_profile") target.mLimitProfile.mEyeYawDeg += 1.f;
    }

    static void functionalVisibleState(LLView* view, std::ostringstream& out,
                                       const LLView* excluded = nullptr,
                                       const ALKitRow* excluded_row = nullptr)
    {
        if (excluded && !excluded_row)
            for (const LLView* parent = excluded->getParent(); parent; parent = parent->getParent())
                if ((excluded_row = dynamic_cast<const ALKitRow*>(parent))) break;
        if (!view || view == excluded || !view->getVisible() || !view->isInVisibleChain()) return;
        if (excluded_row && view->hasAncestor(excluded_row) && dynamic_cast<LLTextBox*>(view)) return;
        out << view->getName() << ':';
        if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(view))
            out << functionalValue(control->getValue());
        out << ';';
        for (LLView* child : *view->getChildList())
            functionalVisibleState(child, out, excluded, excluded_row);
    }

    static bool functionalHazard(const LLSD& entry, const std::string& wanted)
    {
        const LLSD& hazards = entry["hazards"];
        if (!hazards.isArray()) return false;
        for (LLSD::array_const_iterator it = hazards.beginArray(); it != hazards.endArray(); ++it)
            if (it->asString() == wanted) return true;
        return false;
    }

    static bool functionalSettingsFile(const std::filesystem::path& path)
    {
        std::string name = path.filename().string();
        LLStringUtil::toLower(name);
        return name.compare(0, 8, "settings") == 0 && name.size() >= 4 &&
            name.compare(name.size() - 4, 4, ".xml") == 0;
    }

    void captureFunctionalUserFiles()
    {
        std::set<std::filesystem::path> roots;
        roots.insert(std::filesystem::path(
            gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "")));
        const std::string account = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "");
        if (!account.empty()) roots.insert(std::filesystem::path(account));
        for (const std::filesystem::path& root : roots)
        {
            FunctionalDiskSnapshot snapshot;
            snapshot.mRoot = root;
            std::error_code error;
            snapshot.mExisted = std::filesystem::is_directory(root, error);
            if (snapshot.mExisted)
            {
                for (std::filesystem::recursive_directory_iterator it(root, error), end;
                     !error && it != end; it.increment(error))
                {
                    if (!it->is_regular_file(error) ||
                        !functionalSettingsFile(it->path())) continue;
                    std::ifstream input(it->path(), std::ios::binary);
                    if (!input.good()) continue;
                    snapshot.mFiles[std::filesystem::relative(it->path(), root, error)] =
                        std::string(std::istreambuf_iterator<char>(input),
                                    std::istreambuf_iterator<char>());
                }
            }
            mFunctionalUserFiles.push_back(std::move(snapshot));
        }
    }

    void restoreFunctionalUserFiles()
    {
        if (mFunctionalUserFilesRestored) return;
        mFunctionalUserFilesRestored = true;
        for (const FunctionalDiskSnapshot& snapshot : mFunctionalUserFiles)
        {
            std::error_code error;
            // Never replace the account directory: chat/IM transcripts and
            // other user data may have been created while the test was live.
            // Remove only settings XML files absent from the original snapshot.
            if (std::filesystem::is_directory(snapshot.mRoot, error))
            {
                for (std::filesystem::recursive_directory_iterator it(snapshot.mRoot, error), end;
                     !error && it != end; it.increment(error))
                {
                    if (!it->is_regular_file(error) ||
                        !functionalSettingsFile(it->path())) continue;
                    std::error_code file_error;
                    const std::filesystem::path relative =
                        std::filesystem::relative(it->path(), snapshot.mRoot, file_error);
                    if (!file_error && snapshot.mFiles.find(relative) == snapshot.mFiles.end())
                        std::filesystem::remove(it->path(), file_error);
                }
            }
            if (!snapshot.mExisted && snapshot.mFiles.empty()) continue;
            std::filesystem::create_directories(snapshot.mRoot, error);
            for (const auto& saved : snapshot.mFiles)
            {
                const std::filesystem::path path = snapshot.mRoot / saved.first;
                std::filesystem::create_directories(path.parent_path(), error);
                std::ofstream output(path, std::ios::binary | std::ios::trunc);
                output.write(saved.second.data(), static_cast<std::streamsize>(saved.second.size()));
            }
        }
    }

    static void functionalVisibleFloaters(LLView* view, std::set<LLFloater*>& out)
    {
        if (!view || !view->getVisible() || !view->isInVisibleChain()) return;
        if (LLFloater* floater = dynamic_cast<LLFloater*>(view)) out.insert(floater);
        for (LLView* child : *view->getChildList()) functionalVisibleFloaters(child, out);
    }

    void rememberFunctionalSetting(const std::string& name)
    {
        if (name.empty() || mFunctionalRestore.count(name)) return;
        if (LLControlVariable* control = gSavedSettings.getControl(name).get())
            mFunctionalRestore[name] = {
                control->get(), control->getSaveValue(), control->hasUnsavedValue()
            };
    }

    static bool functionalSetControl(LLControlVariable* control, const LLSD& value)
    {
        if (!control) return false;
        control->setValue(value, false); // ui-draw-lint: allow(setting-write) bounded functional prerequisite
        const LLSD actual = control->get();
        // Manifest JSON has only one numeric representation while controls retain
        // their declared scalar type.  A prerequisite is established by its typed
        // value, not by identical LLSD storage tags.
        switch (control->type())
        {
        case TYPE_BOOLEAN: return actual.asBoolean() == value.asBoolean();
        case TYPE_U32:
        case TYPE_S32:     return actual.asInteger() == value.asInteger();
        case TYPE_F32:     return is_approx_equal((F32)actual.asReal(), (F32)value.asReal());
        default:           return llsd_equals(actual, value);
        }
    }

    static S32 functionalLightIndex(const std::string& setting)
    {
        // Match the light token after the Rig/Rim namespace, not the namespace
        // itself ("CineRigRimBg" used to be mistaken for the Rim light).
        static const std::pair<const char*, S32> suffixes[] = {
            { "Back", 2 }, { "Fill", 1 }, { "Key", 0 }, { "Rim", 2 }, { "Bg", 3 }
        };
        const size_t prefix = setting.compare(0, 12, "CineLightRig") == 0 ? 12 :
                              setting.compare(0, 10, "CineRigRim") == 0 ? 10 :
                              std::string::npos;
        if (prefix == std::string::npos) return -1;
        for (const auto& suffix : suffixes)
            if (setting.compare(prefix, strlen(suffix.first), suffix.first) == 0)
                return suffix.second;
        return -1;
    }

    static bool functionalLightIndexSelfTest()
    {
        static const char* const names[] = { "Key", "Fill", "Rim", "Bg" };
        for (S32 i = 0; i < 4; ++i)
            if (functionalLightIndex(std::string("CineLightRig") + names[i] + "EV") != i ||
                functionalLightIndex(std::string("CineRigRim") + names[i] + "Gain") != i)
                return false;
        return functionalLightIndex("CineLightRigBackEV") == 2 &&
               functionalLightIndex("CineRigRimBackGain") == 2;
    }

    static void functionalTickRig()
    {
        // LLAppViewer::idle() returns before this engine tick at login.  The
        // manager is intentionally avatar-safe, so run the same update here.
        ALCineLightRigManager::instance().tick(
            LLPresentationTime::currentFrame().presentation_time);
    }

    // ui-draw-lint: allow-function(setting,setting-write) isolated test-only selector setup
    void prepareFunctionalSelector(const LLSD& entry)
    {
        if (!mDirectorV2 || entry["probe"].asString() == "selector_segment") return;
        const std::string setting = entry["expect"]["setting"].asString();
        const std::string widget = entry["widget_name"].asString();
        if (entry["probe"].asString() == "rig_projector_binding")
        {
            rememberFunctionalSetting("CineLightRigEnabled");
            rememberFunctionalSetting("CineLightRigKeyOn");
            functionalSetControl(gSavedSettings.getControl("CineLightRigEnabled").get(), true);
            functionalSetControl(gSavedSettings.getControl("CineLightRigKeyOn").get(), true);
            functionalTickRig();
        }
        if (entry["page"].asString() == "light_rim_tab" &&
            widget.compare(0, 18, "cine_rig_rim_tron_") == 0)
        {
            // ALRigRimController::tickRefresh owns this subtree's visibility.
            // Establish its master before the lazy page's first refresh; doing
            // it after reveal can miss the six-frame settlement window.
            rememberFunctionalSetting("CineRigRimEnabled");
            functionalSetControl(gSavedSettings.getControl("CineRigRimEnabled").get(), true);
        }
        if (widget.compare(0, 16, "cine_catchlight_") == 0)
        {
            rememberFunctionalSetting("CineLightRigCatchlight");
            rememberFunctionalSetting("CineLightRigActorOnlyAll");
            if (LLControlVariable* catchlight = gSavedSettings.getControl("CineLightRigCatchlight").get())
                catchlight->setValue(true, false); // ui-draw-lint: allow(setting-write) test-only card prerequisite
            if (LLControlVariable* all = gSavedSettings.getControl("CineLightRigActorOnlyAll").get())
                all->setValue(false, false); // ui-draw-lint: allow(setting-write) test-only catchlight prerequisite
            ALCineLightRigManager::instance().selected().setObjectTarget(LLUUID::null);
        }
        if (setting.compare(0, 12, "CineLightRig") != 0 &&
            setting.compare(0, 10, "CineRigRim") != 0) return;
        const S32 index = functionalLightIndex(setting);
        if (index < 0) return;
        if (LLControlVariable* selected = gSavedSettings.getControl("DirectorV2SelectedLight").get())
        {
            mFunctionalRestore.emplace("DirectorV2SelectedLight",
                SavedControlState{selected->get(), selected->getSaveValue(), selected->hasUnsavedValue()});
            selected->setValue(index, false); // ui-draw-lint: allow(setting-write) test-only selector reveal before input
        }
        if (setting.compare(0, 12, "CineLightRig") == 0)
        {
            static const char* const role_on[] = {
                "CineLightRigKeyOn", "CineLightRigFillOn",
                "CineLightRigRimOn", "CineLightRigBgOn"
            };
            if (index >= 0 && index < 4)
            {
                rememberFunctionalSetting(role_on[index]);
                if (LLControlVariable* enabled = gSavedSettings.getControl(role_on[index]).get())
                    enabled->setValue(true, false); // ui-draw-lint: allow(setting-write) test-only selected-light prerequisite
            }
            if (widget.compare(0, 10, "cine_easy_") != 0)
            {
                rememberFunctionalSetting("CineLightRigEasyMode");
                if (LLControlVariable* easy = gSavedSettings.getControl("CineLightRigEasyMode").get())
                    easy->setValue(false, false); // ui-draw-lint: allow(setting-write) test-only advanced-control prerequisite
            }
            if (widget.find("actor_only") != std::string::npos &&
                widget != "cine_actor_only_all")
            {
                rememberFunctionalSetting("CineLightRigActorOnlyAll");
                if (LLControlVariable* all = gSavedSettings.getControl("CineLightRigActorOnlyAll").get())
                    all->setValue(false, false); // ui-draw-lint: allow(setting-write) test-only per-light prerequisite
                // Per-light actor masking is intentionally unavailable for an
                // object-targeted rig. The page row snapshot restores this
                // public model state after the isolated login-screen action.
                ALCineLightRigManager::instance().selected().setObjectTarget(LLUUID::null);
            }
        }
    }

    // ui-draw-lint: allow-function(setting-write) isolated test-only consumer setup
    void prepareFunctionalConsumerGate(const LLSD& entry)
    {
        const std::string consumer = entry["consumer"].asString();
        auto force = [this](const std::string& name, const LLSD& value)
        {
            LLControlVariable* setting = gSavedSettings.getControl(name).get();
            if (!setting) return;
            mFunctionalRestore.emplace(name, SavedControlState{
                setting->get(), setting->getSaveValue(), setting->hasUnsavedValue()});
            setting->setValue(value, false); // ui-draw-lint: allow(setting-write) test-only consumer prerequisite
        };
        if (consumer == "rim_bind_gain")
        {
            force("CineLightRigEnabled", true);
            force("CineRigRimEnabled", true);
            functionalTickRig();
        }
        if (consumer == "weather_rain_intensity")
        {
            if (entry["expect"]["setting"].asString() != "AlchemyWeatherEnabled")
                force("AlchemyWeatherEnabled", true);
            if (entry["expect"]["setting"].asString() != "AlchemyWeatherRainEnabled")
                force("AlchemyWeatherRainEnabled", true);
        }
    }

    static F32 functionalWeatherCloudFactor()
    {
        F32 factor = 1.f;
        if (LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky())
        {
            const F32 cloud_raw = sky->getCloudShadow();
            const F32 cloud = llclamp(std::isfinite(cloud_raw) ? cloud_raw : 1.f, 0.f, 1.f);
            const F32 coupling_raw = gSavedSettings.getF32("AlchemyWeatherEEPCoupling");
            const F32 coupling = llclamp(
                std::isfinite(coupling_raw) ? coupling_raw : 0.35f, 0.f, 1.f);
            factor = lerp(1.f, cloud, coupling);
        }
        return factor;
    }

    // ui-draw-lint: allow-function(setting) isolated test-only consumer observation
    LLSD functionalConsumerValue(const LLSD& entry)
    {
        const std::string consumer = entry["consumer"].asString();
        if (consumer == "rim_render_active" || consumer == "rim_bind_gain" ||
            consumer == "light_rig_enabled_mask" || consumer == "catchlight_enabled" ||
            consumer == "selected_light_values" || consumer == "selected_rim_values")
            functionalTickRig();
        if (consumer == "rim_render_active") return ALCineRigRim::isEnabled();
        if (consumer == "rim_bind_gain")
        {
            const bool active = ALCineRigRim::isEnabled();
            const F32 gain = llclamp(gSavedSettings.getF32("CineRigRimMasterGain"), 0.f, 4.f);
            const F32 smooth = llclamp(gSavedSettings.getF32("CineRigRimSmoothness"), 0.f, 1.f);
            const F32 shimmer = llclamp(gSavedSettings.getF32("CineRigRimAntiShimmer"), 0.f, 1.f);
            const bool debug = gSavedSettings.getBOOL("CineRigRimDebugRimOnly") ||
                               gSavedSettings.getU32("CineRigRimDebugView") == 1;
            const LLVector4 expected_bind(active ? gain : 0.f, active ? smooth : 0.f,
                                          active ? shimmer : 0.f,
                                          active ? (debug ? 2.f : 1.f) : 0.f);
            // setRigRimActive() returns false only when both the renderer-facing
            // active bit and complete bind vector already match.  Probe after
            // isEnabled() has had the one chance to publish them.
            const bool mismatched = gGL.setRigRimActive(active, expected_bind);
            return mismatched ? LLSD(-1.0) : LLSD(active ? gain : 0.f);
        }
        if (consumer == "light_rig_enabled_mask")
            return static_cast<S32>(ALCineLightRigManager::instance().enabledMask());
        if (consumer.compare(0, 19, "selected_rig_light_") == 0)
        {
            const S32 index = entry["rig_light_index"].asInteger();
            if (index < 0 || index >= ALCineLightRigModel::LIGHT_COUNT) return LLSD();
            const ALCineLightRigParamBlob blob = ALCineLightRigParamBlob::fromSettings(
                &ALCineLightRigManager::instance().selected());
            const std::string field = consumer.substr(19);
            if (field == "shaft") return blob.mShaftEnabled[index];
            if (field == "hero") return blob.mHeroEnabled[index];
            if (field == "cone_width")
                return ALCineLightRigModel::easyConeWidthFromBeam(blob.mLights[index].mBeam);
        }
        if (consumer == "rig_group_slots")
            return static_cast<S32>(ALCineLightRigManager::instance().selected().getGroupSlots());
        if (consumer == "catchlight_enabled")
            return ALCineLightRigParamBlob::fromSettings(
                &ALCineLightRigManager::instance().selected()).mCatchlight;
        if (consumer == "weather_rain_intensity")
        {
            if (!gSavedSettings.getBOOL("AlchemyWeatherEnabled") ||
                !gSavedSettings.getBOOL("AlchemyWeatherRainEnabled")) return 0.0;
            const F32 intensity = gSavedSettings.getF32("AlchemyWeatherRainIntensity");
            return llclamp(std::isfinite(intensity) ? intensity : 0.55f, 0.f, 1.f) *
                functionalWeatherCloudFactor();
        }
        if (consumer == "selected_light_values" || consumer == "selected_rim_values")
        {
            const S32 index = gSavedSettings.getS32("DirectorV2SelectedLight");
            LLSD value = LLSD::emptyMap();
            value["selected"] = index;
            if (index >= 0 && index < ALCineLightRigModel::LIGHT_COUNT)
            {
                const ALCineLightRigParamBlob blob = ALCineLightRigParamBlob::fromSettings(
                    &ALCineLightRigManager::instance().selected());
                const ALCineLightRigParamBlob::Light& light = blob.mLights[index];
                LLSD displayed = LLSD::emptyMap();
                if (consumer == "selected_light_values")
                {
                    displayed["ev"] = light.mEV;
                    displayed["shaft"] = blob.mShaftEnabled[index];
                    displayed["cone_width"] =
                        ALCineLightRigModel::easyConeWidthFromBeam(light.mBeam);
                    displayed["hero"] = blob.mHeroEnabled[index];
                }
                else
                {
                    displayed["gain"] = light.mRimGain;
                    displayed["sharpness"] = light.mRimSharpness;
                    displayed["wrap"] = light.mRimWrap;
                    displayed["back_bias"] = light.mRimBackBias;
                }
                value["value"] = displayed;
            }
            return value;
        }
        if (consumer.compare(0, 18, "localfog_selected_") == 0)
        {
            const ALLocalFogManager& manager = ALLocalFogManager::instance();
            const S32 selected = manager.selectedVolume();
            const ALLocalFogManager::Bank& bank = manager.volumes();
            if (selected < 0 || selected >= static_cast<S32>(bank.size())) return LLSD();
            const ALLocalFogManager::Volume& volume = bank[selected];
            const std::string field = consumer.substr(18);
            if (field == "center_x") return volume.mCenter.mV[VX];
            if (field == "center_y") return volume.mCenter.mV[VY];
            if (field == "center_z") return volume.mCenter.mV[VZ];
            if (field == "size_x") return volume.mSize.mV[VX];
            if (field == "size_y") return volume.mSize.mV[VY];
            if (field == "size_z") return volume.mSize.mV[VZ];
            F32 roll = 0.f, pitch = 0.f, yaw = 0.f;
            volume.mRotation.getEulerAngles(&roll, &pitch, &yaw);
            if (field == "rotation_x") return roll * RAD_TO_DEG;
            if (field == "rotation_y") return pitch * RAD_TO_DEG;
            if (field == "rotation_z") return yaw * RAD_TO_DEG;
        }
        if (consumer.compare(0, 19, "anim_switcher_slot_") == 0)
        {
            const std::vector<ALDirectorAnimSwitcher::Slot> bank =
                ALDirectorAnimSwitcher::loadBank();
            if (bank.empty()) return LLSD();
            const S32 selected = llclamp(mFunctionalAnimSwitcherSlot, 0,
                static_cast<S32>(bank.size()) - 1);
            const ALDirectorAnimSwitcher::Slot& slot = bank[selected];
            const std::string field = consumer.substr(19);
            if (field == "kind") return slot.mKind;
            if (field == "target") return slot.mTarget;
            if (field == "pose") return slot.mPoseName;
            if (field == "pose_load") return slot.mPoseLoadMethod;
            if (field == "priority") return slot.mPriority;
            if (field == "speed") return slot.mSpeed;
            if (field == "loop") return slot.mLoop;
            if (field == "snap") return slot.mSnapOnCut;
            if (field == "enabled") return slot.mEnabled;
        }
        return LLSD();
    }

    // ui-draw-lint: allow-function(lookup) isolated test-only displayed-value observation
    bool functionalConsumerExpected(const LLSD& entry, const LLSD& value)
    {
        const std::string consumer = entry["consumer"].asString();
        if (consumer.empty()) return true;
        const LLSD& expected = entry["expect"]["value"];
        if (consumer == "rim_render_active" || consumer == "catchlight_enabled")
            return value.asBoolean() == expected.asBoolean();
        if (consumer == "rim_bind_gain")
            return functionalEqual(value, expected, "F32", entry["increment"]);
        if (consumer == "weather_rain_intensity")
        {
            const std::string setting = entry["expect"]["setting"].asString();
            if (setting == "AlchemyWeatherEnabled" || setting == "AlchemyWeatherRainEnabled")
                return expected.asBoolean() ? value.asReal() > 0.0 : value.asReal() == 0.0;
            const F32 configured = (F32)expected.asReal();
            const LLSD expected_consumer = llclamp(
                std::isfinite(configured) ? configured : 0.55f, 0.f, 1.f) *
                functionalWeatherCloudFactor();
            return functionalEqual(value, expected_consumer, "F32", entry["increment"]);
        }
        if (consumer == "light_rig_enabled_mask")
        {
            const U32 bit = 1u << static_cast<U32>(ALCineLightRigManager::instance().selectedSlot());
            return expected.asBoolean() ? (static_cast<U32>(value.asInteger()) & bit) != 0
                                        : (static_cast<U32>(value.asInteger()) & bit) == 0;
        }
        if (consumer.compare(0, 19, "selected_rig_light_") == 0)
        {
            LLUICtrl* control = mFunctionalControl.get();
            return control && functionalEqual(
                value, control->getValue(),
                entry["widget_type"].asString() == "toggle" ? "Boolean" : "S32",
                entry["increment"]);
        }
        if (consumer == "rig_group_slots")
        {
            LLUICtrl* control = mFunctionalControl.get();
            const U32 bit = 1u << static_cast<U32>(entry["rig_group_index"].asInteger());
            return control && ((static_cast<U32>(value.asInteger()) & bit) != 0) ==
                control->getValue().asBoolean();
        }
        if (consumer == "selected_light_values" || consumer == "selected_rim_values")
        {
            const S32 index = expected.asInteger();
            if (value["selected"].asInteger() != index || !value["value"].isMap()) return false;
            const ALCineLightRigParamBlob blob = ALCineLightRigParamBlob::fromSettings(
                &ALCineLightRigManager::instance().selected());
            if (index < 0 || index >= ALCineLightRigModel::LIGHT_COUNT) return false;
            const auto& light = blob.mLights[index];
            const LLSD& shown = value["value"];
            static const char* const prefixes[] = { "key", "fill", "rim", "bg" };
            const std::string prefix = prefixes[index];
            if (consumer == "selected_light_values")
            {
                LLUICtrl* ev = mFunctionalPageRoot->findChild<LLUICtrl>(
                    "cine_" + prefix + "_ev", true);
                LLUICtrl* shaft = mFunctionalPageRoot->findChild<LLUICtrl>(
                    "cine_" + prefix + "_shaft", true);
                LLUICtrl* cone = mFunctionalPageRoot->findChild<LLUICtrl>(
                    "cine_" + prefix + "_cone_width", true);
                LLUICtrl* hero = mFunctionalPageRoot->findChild<LLUICtrl>(
                    "cine_" + prefix + "_hero", true);
                const F32 displayed_ev = index == 1 && blob.mRatioLock ?
                    blob.mLights[0].mEV - llclamp(blob.mRatio, 0.f, 5.f) : light.mEV;
                return ev && shaft && cone && hero && ev->isInVisibleChain() &&
                       functionalEqual(ev->getValue(), displayed_ev, "F32") &&
                       shaft->getValue().asBoolean() == blob.mShaftEnabled[index] &&
                       cone->getValue().asInteger() ==
                           ALCineLightRigModel::easyConeWidthFromBeam(light.mBeam) &&
                       hero->getValue().asBoolean() == blob.mHeroEnabled[index] &&
                       functionalEqual(shown["ev"], light.mEV, "F32") &&
                       shown["shaft"].asBoolean() == blob.mShaftEnabled[index] &&
                       shown["cone_width"].asInteger() ==
                           ALCineLightRigModel::easyConeWidthFromBeam(light.mBeam) &&
                       shown["hero"].asBoolean() == blob.mHeroEnabled[index];
            }
            const std::string rim_prefix = "cine_rig_rim_" + prefix + "_";
            LLUICtrl* gain = mFunctionalPageRoot->findChild<LLUICtrl>(rim_prefix + "gain", true);
            LLUICtrl* sharpness = mFunctionalPageRoot->findChild<LLUICtrl>(rim_prefix + "sharpness", true);
            LLUICtrl* wrap = mFunctionalPageRoot->findChild<LLUICtrl>(rim_prefix + "wrap", true);
            LLUICtrl* bias = mFunctionalPageRoot->findChild<LLUICtrl>(rim_prefix + "back_bias", true);
            return functionalEqual(shown["gain"], light.mRimGain, "F32") &&
                   functionalEqual(shown["sharpness"], light.mRimSharpness, "F32") &&
                   functionalEqual(shown["wrap"], light.mRimWrap, "F32") &&
                   functionalEqual(shown["back_bias"], light.mRimBackBias, "F32") &&
                   gain && sharpness && wrap && bias && gain->isInVisibleChain() &&
                   functionalEqual(gain->getValue(), light.mRimGain, "F32") &&
                   functionalEqual(sharpness->getValue(), light.mRimSharpness, "F32") &&
                   functionalEqual(wrap->getValue(), light.mRimWrap, "F32") &&
                   functionalEqual(bias->getValue(), light.mRimBackBias, "F32");
        }
        if (consumer.compare(0, 18, "localfog_selected_") == 0 ||
            consumer.compare(0, 19, "anim_switcher_slot_") == 0)
        {
            LLUICtrl* control = mFunctionalControl.get();
            if (!control) return false;
            std::string value_type = "S32";
            if (dynamic_cast<LLCheckBoxCtrl*>(control)) value_type = "Boolean";
            else if (dynamic_cast<LLSpinCtrl*>(control) ||
                     dynamic_cast<LLSliderCtrl*>(control)) value_type = "F32";
            else if (dynamic_cast<LLLineEditor*>(control) ||
                     (dynamic_cast<LLComboBox*>(control) && value.isString()))
                value_type = "String";
            return functionalEqual(value, control->getValue(), value_type,
                                   entry["increment"]);
        }
        return false;
    }

    static void revealFunctionalAncestors(LLView* view)
    {
        std::vector<LLView*> ancestors;
        for (LLView* child = view; child; child = child->getParent()) ancestors.push_back(child);
        for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it)
        {
            LLView* child = *it;
            if (ALKitLazyPage* lazy = dynamic_cast<ALKitLazyPage*>(child)) lazy->ensureBuilt();
            if (LLAccordionCtrlTab* accordion = dynamic_cast<LLAccordionCtrlTab*>(child))
            {
                // changeOpenClose() receives the current state and toggles it.
                // Passing true after setDisplayChildren(true) closed every
                // accordion the functional reveal had just opened.
                if (!accordion->getDisplayChildren()) accordion->changeOpenClose(false);
            }
            if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(child->getParent()))
                if (LLPanel* panel = dynamic_cast<LLPanel*>(child)) tabs->selectTabPanel(panel);
        }
    }

    // ui-draw-lint: allow-function(lookup) isolated test-only manifest page lookup
    LLView* functionalPage(const LLSD& entry)
    {
        LLFloater* floater = mFloater.get();
        if (!floater) return nullptr;
        std::string page_name = entry["host"]["page_view"].asString();
        if (entry["host"]["via"].asString() == "launcher")
            page_name = entry["host"]["launcher"]["page_view"].asString();
        if (page_name.empty()) page_name = entry["page"].asString();
        if (mDirectorV2)
        {
            ALKitShell* shell = findFirstView<ALKitShell>(floater);
            if (!shell || !shell->selectPage(page_name)) return nullptr;
            return shell->ensurePageBuilt(page_name);
        }
        LLView* page = floater->findChildView(page_name, true);
        if (page) revealFunctionalAncestors(page);
        return page;
    }

    // ui-draw-lint: allow-function(lookup) isolated test-only manifest control lookup
    LLUICtrl* functionalControl(LLView* page, const LLSD& entry)
    {
        if (!page) return nullptr;
        std::string name = entry["widget_name"].asString();
        if (entry["probe"].asString() == "selector_segment")
        {
            const size_t colon = name.find(':');
            if (colon != std::string::npos)
                name = name.substr(0, colon) + "_seg_" +
                    llformat("%d", entry["test_value"].asInteger());
        }
        else if (entry["probe"].asString() == "preset_strip" && mDirectorV2)
        {
            name = entry["page"].asString();
            if (name.size() > 4 && name.compare(name.size() - 4, 4, "_tab") == 0)
                name.erase(name.size() - 4);
            name += "_preset";
        }
        if (name != "check_box")
            if (LLUICtrl* named = page->findChild<LLUICtrl>(name, true)) return named;
        // Some legacy XUI checkboxes have no explicit name; the source table
        // necessarily records their tag ("check_box"). Resolve those rows by
        // the unique public control-variable binding instead of guessing a
        // sibling by label or tree order.
        const std::string setting_name = entry["setting"].asString();
        if (setting_name.empty()) return nullptr;
        LLUICtrl* match = nullptr;
        bool ambiguous = false;
        std::function<void(LLView*)> collect = [&](LLView* view)
        {
            if (ambiguous) return;
            if (LLUICtrl* candidate = dynamic_cast<LLUICtrl*>(view))
                if (LLControlVariable* bound = candidate->getControlVariable();
                    bound && bound->getName() == setting_name)
                {
                    if (match && match != candidate) ambiguous = true;
                    else match = candidate;
                }
            for (LLView* child : *view->getChildList()) collect(child);
        };
        collect(page);
        return ambiguous ? nullptr : match;
    }

    static std::string functionalRedirectFloater(const LLSD& entry)
    {
        const std::string xui = entry["xui_file"].asString();
        if (xui.find("floater_ultimate_diopter.xml") != std::string::npos) return "ultimate_diopter";
        if (xui.find("floater_cine_light_cues.xml") != std::string::npos) return "cine_light_cues";
        if (xui.find("floater_gaze_cues.xml") != std::string::npos) return "gaze_cues";
        if (xui.find("floater_lightbox_settings.xml") != std::string::npos) return "lightbox";
        return {};
    }

    static std::string functionalLauncherName(const LLSD& entry)
    {
        const std::string page = entry["page"].asString();
        if (page == "diopter_tab") return "card_diopter_open";
        if (page == "gaze_cues_tab") return "card_gaze_cues_open";
        if (page == "light_cues_tab") return "card_light_cues_open";
        if (page == "actors_tab") return "card_actor_mask_open";
        if (page == "practicals_tab") return "card_practicals_open";
        if (page == "image_tab") return "card_image_open";
        if (page == "atmosphere_tab") return "card_haze_open";
        if (page == "shadows_probes_tab") return "card_shadows_probes_open";
        if (page == "stylize_tab") return "card_rotoink_open";
        if (page == "tron_tab") return "kc_open_launch";
        return {};
    }

    // ui-draw-lint: allow-function(lookup) isolated test-only redirect target lookup
    bool openFunctionalRedirect(const LLSD& entry, LLView*& page)
    {
        const LLSD& host = entry["host"];
        if (host["via"].asString() != "launcher") return true;
        const std::string launcher_name = host["launcher"]["widget"].asString();
        const std::string floater_name = host["floater"].asString();
        LLUICtrl* launcher = launcher_name.empty() || !page ? nullptr :
            page->findChild<LLUICtrl>(launcher_name, true);
        if (!launcher || floater_name.empty())
        {
            mFunctionalRedirectReason = "launcher: control " + launcher_name + " missing";
            return false;
        }
        std::set<LLFloater*> visible_before;
        functionalVisibleFloaters(gFloaterView, visible_before);
        for (LLFloater* floater : visible_before)
            if (floater->getInstanceName() == floater_name)
            {
                mFunctionalLauncherFloater = floater->getHandle();
                mFunctionalLauncherWasOpen = true;
                floater->closeFloater(false);
            }
        visible_before.clear();
        functionalVisibleFloaters(gFloaterView, visible_before);
        revealFunctionalControl(launcher);
        std::string reason;
        if (!functionalReachable(launcher, reason) || !functionalPointerClick())
        {
            mFunctionalRedirectReason = "launcher: not clickable (" +
                (mFunctionalLastClickReason.empty() ? reason : mFunctionalLastClickReason) + ")";
            return false;
        }
        std::set<LLFloater*> visible_after;
        functionalVisibleFloaters(gFloaterView, visible_after);
        std::vector<LLFloater*> added;
        for (LLFloater* floater : visible_after)
            if (!visible_before.count(floater)) added.push_back(floater);
        LLFloater* target = nullptr;
        for (LLFloater* floater : added)
            if (floater->getInstanceName() == floater_name) target = floater;
        mFunctionalObserved["launcher"]["new_count"] = static_cast<S32>(added.size());
        mFunctionalObserved["launcher"]["click_accepted"] = true;
        if (added.size() != 1 || !target)
        {
            mFunctionalRedirectReason = added.empty() ? "launcher: no new floater" :
                "launcher: wrong floater " + added.front()->getInstanceName();
            return false;
        }
        mFunctionalRedirectFloater = target->getHandle();
        target->setMinimized(false);
        // Saved Lightbox rectangles can be applied after its children were
        // constructed. Re-run the public reshape path even when the outer
        // size is unchanged so follows="all" scroll viewports match the live
        // floater rather than their construction-time height.
        const LLRect target_rect = target->getRect();
        target->reshape(target_rect.getWidth(), target_rect.getHeight(), true);
        if (gFloaterView) gFloaterView->adjustToFitScreen(target, false);
        // Keep the redirect from covering Director controls. Prefer its right
        // side, then its left; adjustToFitScreen remains the final authority.
        if (LLFloater* director = mFloater.get())
        {
            const LLRect director_screen = director->calcScreenRect();
            const LLRect target_screen = target->calcScreenRect();
            const LLRect bounds = gFloaterView ? gFloaterView->calcScreenRect() : LLRect();
            S32 dx = 0;
            if (director_screen.mRight + 8 + target_screen.getWidth() <= bounds.mRight)
                dx = director_screen.mRight + 8 - target_screen.mLeft;
            else if (director_screen.mLeft - 8 - target_screen.getWidth() >= bounds.mLeft)
                dx = director_screen.mLeft - 8 - target_screen.mRight;
            if (dx) target->translate(dx, 0);
            if (gFloaterView) gFloaterView->adjustToFitScreen(target, false);
        }
        target->setFrontmost(true);
        mFunctionalHostFloater = target->getHandle();
        page = target;
        const std::string tab_name = host["tab"].asString();
        if (!tab_name.empty())
        {
            LLView* tab = target->findChildView(tab_name, true);
            if (!tab)
            {
                mFunctionalRedirectReason = "launcher: tab " + tab_name + " missing";
                return false;
            }
            revealFunctionalAncestors(tab);
            LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(tab->getParent());
            if (!tabs || tabs->getCurrentPanel() != tab)
            {
                mFunctionalRedirectReason = "launcher: tab " + tab_name + " not current";
                return false;
            }
            page = tab;
        }
        mFunctionalObserved["launcher"]["floater"] = floater_name;
        mFunctionalObserved["launcher"]["tab"] = tab_name;
        return true;
    }

    // ui-draw-lint: allow-function(setting,lookup) isolated test-only prerequisite setup
    bool prepareFunctionalDeclaredPrerequisites(const LLSD& entry)
    {
        bool ok = true;
        const LLSD* prerequisite_sets[] = {
            &entry["enabled_control"], &entry["reveal_settings"]
        };
        for (const LLSD* prerequisites : prerequisite_sets)
        {
            if (!prerequisites->isArray()) continue;
            for (LLSD::array_const_iterator it = prerequisites->beginArray();
                 it != prerequisites->endArray(); ++it)
            {
                const std::string name = (*it).isMap() ?
                    (*it)["setting"].asString() : it->asString();
                const LLSD value = !(*it).isMap() || !(*it).has("value") ?
                    LLSD(true) : (*it)["value"];
                rememberFunctionalSetting(name);
                ok = functionalSetControl(gSavedSettings.getControl(name).get(), value) && ok;
            }
        }
        return ok;
    }

    // ui-draw-lint: allow-function(setting,lookup,raw-write) isolated test-only prerequisite setup
    bool prepareFunctionalPrerequisite(const LLSD& entry, LLUICtrl* control)
    {
        bool ok = prepareFunctionalDeclaredPrerequisites(entry);
        const std::string widget = entry["widget_name"].asString();
        const std::string prerequisite = entry["prereq"].asString();
        if (mInWorld && (prerequisite == "ghost_selected_entity" ||
                         prerequisite == "ghost_selected_formation") &&
            gAgentAvatarp && mFunctionalGhostFixture.isNull())
        {
            ALGhostStudio& studio = ALGhostStudio::instance();
            mFunctionalGhostSelectionBefore = studio.getSelected();
            ALGhostStudio::Instance* instance = prerequisite == "ghost_selected_entity"
                ? studio.spawnEntityClone(gAgentAvatarp->getID(), "Functional fixture")
                : studio.addInstance(gAgentAvatarp->getID());
            if (instance)
            {
                mFunctionalGhostFixture = instance->mId;
                studio.setSelected(instance->mId);
                if (prerequisite == "ghost_selected_entity")
                    studio.setInstanceDriveMode(instance->mId, ALGhostStudio::DRIVE_DIRECTED);
            }
            ok = instance != nullptr && ok;
        }
        if (mInWorld && prerequisite == "path_actor_edit_2_waypoints" &&
            gAgentAvatarp && !mFunctionalHaveActorPathBefore)
        {
            LLActorMover& mover = LLActorMover::instance();
            mFunctionalActorMoverId = gAgentAvatarp->getID();
            mFunctionalActorPathBefore = mover.pathSceneData(mFunctionalActorMoverId);
            mFunctionalActorPathWalkingBefore = mover.isPathWalking(mFunctionalActorMoverId);
            mFunctionalHaveActorPathBefore = true;
            mover.stop(mFunctionalActorMoverId);
            mover.clearPath(mFunctionalActorMoverId);
            const LLVector3d origin = gAgent.getPositionGlobal();
            mover.appendWaypointAt(mFunctionalActorMoverId, origin);
            mover.appendWaypointAt(mFunctionalActorMoverId,
                                   origin + LLVector3d(2.0, 0.0, 0.0));
            mover.appendWaypointAt(mFunctionalActorMoverId,
                                   origin + LLVector3d(4.0, 2.0, 1.0));
            mover.setEditNode(1);
            if (widget == "btn_edit_undo" || widget == "btn_edit_redo")
            {
                mFunctionalPathExpected = mover.pathSceneData(mFunctionalActorMoverId);
                mover.snapshotForUndo(mFunctionalActorMoverId);
                mover.appendWaypointAt(mFunctionalActorMoverId,
                                       origin + LLVector3d(6.0, -1.0, 0.5));
                if (widget == "btn_edit_redo")
                {
                    mFunctionalPathExpected = mover.pathSceneData(mFunctionalActorMoverId);
                    mover.undoPath(mFunctionalActorMoverId);
                }
            }
            if (widget == "node_cam_transition_combo" || widget == "btn_node_preview")
                mover.setNodeCamera(mFunctionalActorMoverId, 1, origin + LLVector3d(0.0, 0.0, 2.0),
                                    LLQuaternion(), 1.0f, 0);
            if (widget == "sync_offset_spinner")
            {
                LLActorMover::Path& path = mover.editPath(mFunctionalActorMoverId);
                path.mSyncToTake = true;
                path.markDirty();
            }
            if (widget == "node_corner_radius_spinner")
            {
                LLActorMover::Path& path = mover.editPath(mFunctionalActorMoverId);
                path.mEndMode = 1;
                path.markDirty();
            }
            const bool primitive_control = widget == "btn_primitive_center" ||
                widget == "primitive_radius_x" || widget == "primitive_radius_y" ||
                widget == "primitive_start_deg" || widget == "primitive_sweep_deg" ||
                widget == "primitive_rise" || widget == "primitive_yaw_deg" ||
                widget == "primitive_pitch_deg" || widget == "primitive_roll_deg";
            if (primitive_control)
            {
                const S32 shape = widget == "primitive_rise"
                    ? ALPathGeometry::HELIX : ALPathGeometry::ELLIPSE_ARC;
                mover.setPathShape(mFunctionalActorMoverId, shape);
            }
            LLScrollListCtrl* cast_list = mFloater.get()
                ? mFloater.get()->findChild<LLScrollListCtrl>("cast_list", true) : nullptr;
            if (cast_list)
            {
                mFunctionalCastList = cast_list->getDerivedHandle<LLScrollListCtrl>();
                for (LLScrollListItem* item : cast_list->getAllSelected())
                    if (item) mFunctionalCastSelectionBefore.push_back(item->getValue().asUUID());
                mFunctionalPathActorWasCast = LLDirectorCast::instance().contains(mFunctionalActorMoverId);
                if (!mFunctionalPathActorWasCast)
                    LLDirectorCast::instance().add(mFunctionalActorMoverId);
                mFunctionalPathLeaderId = LLUUID::generateNewID();
                LLDirectorCast::instance().add(mFunctionalPathLeaderId);
                if (widget == "follow_offset_spinner" || widget == "btn_stop_following")
                    mover.setFollow(mFunctionalActorMoverId, mFunctionalPathLeaderId, 0, 3.f);
                bool present = false;
                for (LLScrollListItem* item : cast_list->getAllData())
                    present = present || (item && item->getValue().asUUID() == mFunctionalActorMoverId);
                if (!present)
                {
                    LLSD row;
                    row["value"] = mFunctionalActorMoverId;
                    row["columns"][0]["column"] = "name";
                    row["columns"][0]["value"] = "Functional path actor";
                    cast_list->addElement(row, ADD_BOTTOM);
                }
                cast_list->deselectAllItems();
                cast_list->selectByID(mFunctionalActorMoverId);
                cast_list->onCommit();
            }
            if (ALPanelPathEditor* panel = findFirstView<ALPanelPathEditor>(mFunctionalPageRoot))
            {
                mFunctionalPathPanel = panel->getDerivedHandle<ALPanelPathEditor>();
                mFunctionalPathTargetBefore = panel->getTargetActor();
                LLCheckBoxCtrl* edit = panel->findChild<LLCheckBoxCtrl>("edit_mode_check", true);
                if (edit)
                {
                    mFunctionalPathEditCheck = edit->getDerivedHandle<LLCheckBoxCtrl>();
                    mFunctionalPathEditBefore = edit->getValue().asBoolean();
                }
                panel->revealFor(mFunctionalActorMoverId);
                if (edit)
                {
                    edit->setValue(true); // ui-draw-lint: allow(raw-write) test-only one-shot path fixture
                    edit->onCommit();
                }
            }
            if (widget == "btn_stop_path") mover.start(mFunctionalActorMoverId);
        }
        if (mInWorld && (widget == "prop_list" || widget == "node_list"))
        {
            LLScrollListCtrl* prop_list = mFunctionalPageRoot
                ? mFunctionalPageRoot->findChild<LLScrollListCtrl>("prop_list", true) : nullptr;
            if (mFunctionalPropFixtures.empty())
            {
                for (S32 fixture_index = 0; fixture_index < 2; ++fixture_index)
                {
                    const LLUUID id = LLUUID::generateNewID();
                    mFunctionalPropFixtures.push_back(id);
                    ALObjectPathMover::toggleTarget(id);
                    LLActorMover::instance().appendWaypointAt(
                        id, LLVector3d(10.0 + fixture_index * 4.0, 10.0, 20.0));
                    LLActorMover::instance().appendWaypointAt(
                        id, LLVector3d(12.0 + fixture_index * 4.0, 10.0, 20.0));
                    // Distinct per-fixture path speed and node yaw so the owner
                    // oracles can tell which prop/node the panel actually shows.
                    LLActorMover::Path& path = LLActorMover::instance().editPath(id);
                    path.mSpeed = 1.25f + 1.5f * static_cast<F32>(fixture_index);
                    for (size_t node = 0; node < path.mNodes.size(); ++node)
                        path.mNodes[node].mYawOffset = (10.f + 20.f * fixture_index +
                            5.f * static_cast<F32>(node)) * DEG_TO_RAD;
                    path.markDirty();
                }
            }
            if (prop_list)
                for (S32 fixture_index = 0;
                     fixture_index < static_cast<S32>(mFunctionalPropFixtures.size());
                     ++fixture_index)
                {
                    const LLUUID& id = mFunctionalPropFixtures[fixture_index];
                    bool present = false;
                    for (LLScrollListItem* item : prop_list->getAllData())
                        present = present || (item && item->getValue().asUUID() == id);
                    if (!present)
                    {
                        LLSD row;
                        row["value"] = id;
                        row["columns"][0]["column"] = "name";
                        row["columns"][0]["value"] = llformat(
                            "Functional prop %d", fixture_index + 1);
                        row["columns"][1]["column"] = "nodes";
                        row["columns"][1]["value"] = 2;
                        row["columns"][2]["column"] = "state";
                        row["columns"][2]["value"] = "away";
                        prop_list->addElement(row, ADD_BOTTOM);
                    }
                }
            if (widget == "node_list" && prop_list && !mFunctionalPropFixtures.empty())
            {
                prop_list->setSelectedByValue(mFunctionalPropFixtures.front(), true);
                prop_list->onCommit();
            }
            // Sentinels (no commit): only the owner's selection handler
            // (refreshNodePane) can replace them with the selected path's values.
            if (LLSpinCtrl* speed = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLSpinCtrl>("path_speed_spin", true) : nullptr)
                speed->setValue(LLSD(-1.0));
            if (LLSpinCtrl* skid = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLSpinCtrl>("node_skid_spin", true) : nullptr)
                skid->setValue(LLSD(-999.0));
            LLScrollListCtrl* fixture_list = dynamic_cast<LLScrollListCtrl*>(control);
            ok = prop_list && fixture_list && fixture_list->getItemCount() >= 2 && ok;
        }
        if (mInWorld && entry["probe"].asString() == "prop_owner" &&
            widget != "prop_list" && widget != "node_list" && !mFunctionalPropObject)
        {
            LLViewerObject* object = gObjectList.createObjectViewer(LL_PCODE_VOLUME,
                                                                    gAgent.getRegion());
            LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
            if (volume)
            {
                volume->mbCanSelect = true;
                volume->mIsLocalOnly = true;
                volume->setFlagsWithoutUpdate(
                    FLAGS_OBJECT_YOU_OWNER | FLAGS_OBJECT_MODIFY | FLAGS_OBJECT_MOVE |
                    FLAGS_OBJECT_COPY | FLAGS_OBJECT_TRANSFER, true);
                gPipeline.createObject(volume);
                volume->setLOD(LLVolumeLODGroup::NUM_LODS - 1);
                LLVolumeParams params;
                params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
                params.setBeginAndEndS(0.f, 1.f);
                params.setBeginAndEndT(0.f, 1.f);
                params.setRatio(1.f, 1.f);
                params.setShear(0.f, 0.f);
                if (volume->setVolume(params, LLVolumeLODGroup::NUM_LODS - 1, true))
                {
                    volume->setScale(LLVector3(0.5f, 0.5f, 0.5f), false);
                    volume->setPositionAgent(gAgent.getPositionAgent() + LLVector3(2.f, 0.f, 1.f));
                    mFunctionalPropObject = volume;
                }
                else
                    volume->markDead();
            }
            if (mFunctionalPropObject)
            {
                const LLUUID id = mFunctionalPropObject->getID();
                // btn_enroll is excluded in-world (functional_report.py
                // INWORLD_EXCLUDED_CONTROLS): any LLSelectMgr selection, even of
                // a viewer-only object, broadcasts PointAt + AGENT_STATE_EDITING.
                if (widget != "btn_enroll")
                {
                    ALObjectPathMover::toggleTarget(id);
                    ALObjectPathMover::instance().appendWaypointHere(id);
                    mFunctionalPropObject->setPositionAgent(
                        mFunctionalPropObject->getPositionAgent() + LLVector3(2.f, 0.f, 0.f));
                    ALObjectPathMover::instance().appendWaypointHere(id);
                    LLScrollListCtrl* props = mFunctionalPageRoot->findChild<LLScrollListCtrl>(
                        "prop_list", true);
                    if (props)
                    {
                        LLSD row;
                        row["value"] = id;
                        row["columns"][0]["column"] = "name";
                        row["columns"][0]["value"] = "Functional local prop";
                        props->addElement(row, ADD_BOTTOM);
                        props->setSelectedByValue(id, true);
                        props->onCommit();
                    }
                    if (widget == "btn_stop" || widget == "btn_stop_all")
                        ALObjectPathMover::instance().start(id);
                }
            }
            ok = mFunctionalPropObject.notNull() && ok;
        }
        if (mInWorld && widget == "db_playlist_list")
        {
            LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(control);
            if (!mFunctionalDanceBefore.isMap())
            {
                ALDanceBall& ball = ALDanceBall::instance();
                mFunctionalDanceBefore = ball.exportSession();
                ball.addAnimation(LLUUID::generateNewID(), "Functional dance A");
                ball.addAnimation(LLUUID::generateNewID(), "Functional dance B");
            }
            if (list && list->getItemCount() < 2)
            {
                list->clearRows();
                const auto& playlist = ALDanceBall::instance().playlist();
                for (S32 index = 0; index < static_cast<S32>(playlist.size()); ++index)
                {
                    LLSD row;
                    row["id"] = index;
                    row["columns"][0]["column"] = "name";
                    row["columns"][0]["value"] = playlist[index].mLabel;
                    list->addElement(row, ADD_BOTTOM);
                }
            }
            ok = list && list->getItemCount() >= 2 && ok;
        }
        if (mInWorld && entry["probe"].asString() == "dance_owner" &&
            !mFunctionalDanceFixturePrepared)
        {
            mFunctionalDanceFixturePrepared = true;
            ALDanceBall& ball = ALDanceBall::instance();
            if (!mFunctionalDanceBefore.isMap())
            {
                mFunctionalDanceBefore = ball.exportSession();
                mFunctionalDanceEnabledBefore = ball.enabled();
                mFunctionalDanceFrozenBefore = ball.frozen();
            }
            if (ball.playlist().size() < 2)
            {
                ball.addAnimation(LLUUID::generateNewID(), "Functional dance A");
                ball.addAnimation(LLUUID::generateNewID(), "Functional dance B");
            }
            if ((widget == "db_add_ghosts" || widget == "db_add_selected") &&
                mFunctionalGhostFixture.isNull() && gAgentAvatarp)
            {
                mFunctionalGhostSelectionBefore = ALGhostStudio::instance().getSelected();
                if (ALGhostStudio::Instance* instance = ALGhostStudio::instance().spawnEntityClone(
                        gAgentAvatarp->getID(), "Functional dance ghost"))
                    mFunctionalGhostFixture = instance->mId;
            }
            // No LLSelectMgr selection in-world: any selection broadcasts
            // PointAt/AGENT_STATE_EDITING (INWORLD_MODE.md, selection exclusions).
            // db_add_selected is excluded by both manifest and runner guards.
            if (widget == "db_include_me" || widget == "db_add_nearby")
                ball.removeDancer(gAgentID);
            if (widget == "db_add_nearby") ball.setIncludeSelf(true);
            if (widget == "db_add_cast")
            {
                mFunctionalDanceActorWasCast = LLDirectorCast::instance().contains(gAgentID);
                if (!mFunctionalDanceActorWasCast) LLDirectorCast::instance().add(gAgentID);
                ball.setIncludeSelf(true);
                ball.removeDancer(gAgentID);
            }
            if (widget == "db_play_selected" || widget == "db_play_all")
            {
                ball.setIncludeSelf(true);
                ball.addDancer(gAgentID, ALDanceBall::DancerKind::SELF);
            }
            if (widget == "db_stop_all") ball.setEnabled(true);
            if (widget == "db_t_speed")
            {
                ALDanceBall::Timing timing = ball.timing();
                timing.mSpeed = control->getValue().asReal();
                ball.setTiming(timing);
            }
            if (widget == "db_ramp_go")
            {
                if (LLSpinCtrl* target = mFunctionalPageRoot->findChild<LLSpinCtrl>(
                        "db_ramp_target", true))
                    target->setValue(ball.timing().mSpeed < 3.0 ? 3.0 : 1.0);
                if (LLSpinCtrl* seconds = mFunctionalPageRoot->findChild<LLSpinCtrl>(
                        "db_ramp_seconds", true))
                    seconds->setValue(30.0);
            }
            // Never clobber the user's clipboard: only borrow it when it holds
            // plain text (restored exactly); inventory items or non-text OS
            // data leave the row without a fixture (it then FAILs honestly).
            if (widget == "db_paste_uuid" && !LLClipboard::instance().hasContents() &&
                LLClipboard::instance().isTextAvailable())
            {
                mFunctionalClipboardHadText = LLClipboard::instance().pasteFromClipboard(
                    mFunctionalClipboardBefore);
                const LLWString fixture = utf8string_to_wstring(LLUUID::generateNewID().asString());
                LLClipboard::instance().copyToClipboard(fixture, 0,
                    static_cast<S32>(fixture.size()));
                mFunctionalClipboardPrepared = true;
            }
            // db_from_explorer is excluded in-world (INWORLD_EXCLUDED_CONTROLS):
            // opening Animation Explorer or adding a source to it can send
            // ObjectSelect/ObjectDeselect for real objects
            // (animationexplorer.cpp:540-556). Never drive it on a live login.
            if (widget == "db_from_explorer" && !mInWorld)
            {
                AnimationExplorer* explorer =
                    LLFloaterReg::findTypedInstance<AnimationExplorer>("animation_explorer");
                mFunctionalExplorerWasVisible = explorer && explorer->getVisible();
                if (!explorer)
                {
                    LLFloaterReg::showInstance("animation_explorer");
                    explorer = LLFloaterReg::findTypedInstance<AnimationExplorer>(
                        "animation_explorer");
                }
                if (explorer)
                {
                    mFunctionalExplorer = explorer->getDerivedHandle<AnimationExplorer>();
                    mFunctionalExplorerSelectionBefore = explorer->getSelectedAnimId();
                    mFunctionalExplorerFixture = LLUUID::generateNewID();
                    explorer->addAnimation(mFunctionalExplorerFixture,
                        LLUUID::generateNewID(), LLTimer::getElapsedSeconds());
                    if (LLScrollListCtrl* animations = explorer->findChild<LLScrollListCtrl>(
                            "animation_list", true))
                    {
                        mFunctionalExplorerList =
                            animations->getDerivedHandle<LLScrollListCtrl>();
                        animations->selectFirstItem();
                        animations->onCommit();
                    }
                }
            }
            if (widget == "db_exclude")
            {
                ball.setIncludeSelf(true);
                ball.addDancer(gAgentID, ALDanceBall::DancerKind::SELF);
                if (LLScrollListCtrl* dancers = mFunctionalPageRoot->findChild<LLScrollListCtrl>(
                        "db_dancer_list", true))
                {
                    bool present = false;
                    for (LLScrollListItem* item : dancers->getAllData())
                        present = present || (item && item->getValue().asUUID() == gAgentID);
                    if (!present)
                    {
                        LLSD row;
                        row["value"] = gAgentID;
                        row["columns"][0]["column"] = "name";
                        row["columns"][0]["value"] = "You";
                        dancers->addElement(row, ADD_BOTTOM);
                    }
                    dancers->setSelectedByValue(gAgentID, true);
                }
            }
            if (LLScrollListCtrl* playlist = mFunctionalPageRoot->findChild<LLScrollListCtrl>(
                    "db_playlist_list", true); playlist)
            {
                if (playlist->getItemCount() < 2)
                {
                    playlist->clearRows();
                    const auto& entries = ball.playlist();
                    for (S32 index = 0; index < static_cast<S32>(entries.size()); ++index)
                    {
                        LLSD row;
                        row["value"] = index;
                        row["columns"][0]["column"] = "name";
                        row["columns"][0]["value"] = entries[index].mLabel;
                        playlist->addElement(row, ADD_BOTTOM);
                    }
                }
                const S32 index = widget == "db_pl_up" ? 1 : 0;
                playlist->selectNthItem(index);
                playlist->onCommit();
            }
        }
        if (mInWorld && entry["probe"].asString() == "gaze_cast_target" && gAgentAvatarp &&
            !mFunctionalHaveCastGazeBefore)
        {
            ALPanelLensGaze* panel = findFirstView<ALPanelLensGaze>(mFunctionalPageRoot);
            LLComboBox* edit_actor = mFunctionalPageRoot->findChild<LLComboBox>(
                "gaze_edit_actor", true);
            if (panel && edit_actor)
            {
                mFunctionalGazeActor = gAgentAvatarp->getID();
                mFunctionalGazeBefore = LLActorMover::instance().getGazeTargetConfig(
                    mFunctionalGazeActor);
                mFunctionalHaveGazeBefore = true;
                mFunctionalCastGazeBefore = LLDirectorCast::instance().getGazeTarget(
                    mFunctionalGazeActor);
                mFunctionalCastLookBefore = LLDirectorCast::instance().isLookAtCamera(
                    mFunctionalGazeActor);
                mFunctionalHaveCastGazeBefore = true;
                mFunctionalGazeEnabledBefore = LLActorMover::instance().isGazeEnabled(
                    mFunctionalGazeActor);
                mFunctionalGazeEditActorBefore = edit_actor->getValue();
                mFunctionalGazeEditActor = edit_actor->getDerivedHandle<LLComboBox>();
                if (widget == "gaze_cast_combo")
                {
                    mFunctionalGazeCastFixture = LLUUID::generateNewID();
                    mFunctionalGazeCastFixtureAlternate = LLUUID::generateNewID();
                    LLDirectorCast::instance().add(mFunctionalGazeCastFixture);
                    LLDirectorCast::instance().add(mFunctionalGazeCastFixtureAlternate);
                }
                edit_actor->setValue(0);
                edit_actor->onCommit();
                LLActorMover::instance().setGazeEnabled(mFunctionalGazeActor, true);
                LLDirectorCast::instance().setLookAtCamera(mFunctionalGazeActor, true);
                LLActorMover::GazeTarget target = mFunctionalCastGazeBefore;
                target.mMode = LLActorMover::GazeTarget::FIXED_POINT;
                LLDirectorCast::instance().setGazeTarget(mFunctionalGazeActor, target);
            }
        }
        if (mInWorld && entry["probe"].asString() == "gaze_slot" && gAgentAvatarp &&
            !mFunctionalHaveCastLookBefore)
        {
            mFunctionalGazeActor = gAgentAvatarp->getID();
            mFunctionalCastLookOnlyBefore = LLDirectorCast::instance().isLookAtCamera(
                mFunctionalGazeActor);
            mFunctionalHaveCastLookBefore = true;
        }
        if (mInWorld && entry["probe"].asString() == "ghost_owner" &&
            !mFunctionalHaveGhostOwnerBefore)
        {
            mFunctionalGhostShowAllBefore = ALGhostStudio::instance().getShowAll();
            ALToolGhostEdit* tool = ALToolGhostEdit::instanceExists()
                ? ALToolGhostEdit::getInstance() : nullptr;
            mFunctionalGhostEditingBefore = tool && tool->isEditModeActive();
            if (tool) mFunctionalGhostEditOwnerBefore = tool->editOwner();
            mFunctionalHaveGhostOwnerBefore = true;
        }
        if (mInWorld && entry["probe"].asString() == "flycam_stop" &&
            !mFunctionalFlycamFixture)
        {
            LLFlycamRecorder& recorder = LLFlycamRecorder::instance();
            if (recorder.getState() == LLFlycamRecorder::STATE_IDLE &&
                recorder.getNumKeyframes() == 0)
            {
                recorder.startRecording();
                mFunctionalFlycamFixture = true;
            }
        }
        if (mInWorld && widget == "localfog_volume_list" &&
            !mFunctionalHaveLocalFogBefore)
        {
            ALLocalFogManager& manager = ALLocalFogManager::instance();
            mFunctionalLocalFogBefore = manager.volumes();
            mFunctionalLocalFogSelectedBefore = manager.selectedVolume();
            mFunctionalHaveLocalFogBefore = true;
            manager.volumes()[0].mLabel = "Functional fog A";
            manager.volumes()[1].mLabel = "Functional fog B";
            manager.touchBank();
        }
        if (mDirectorV2 && widget.find("actor_only") != std::string::npos &&
            widget != "cine_actor_only_all")
        {
            // Opening/selecting the lazy Rig page can reload its slot after
            // prepareFunctionalSelector().  Re-establish the two documented
            // Actor-only gates now that the real page exists.
            rememberFunctionalSetting("CineLightRigActorOnlyAll");
            ok = functionalSetControl(
                gSavedSettings.getControl("CineLightRigActorOnlyAll").get(), false) && ok;
            ALCineLightRigManager::instance().selected().setObjectTarget(LLUUID::null);
        }
        const std::string enable_widget = entry["enable_widget"].asString();
        if (!enable_widget.empty() && mFunctionalPageRoot)
        {
            LLCheckBoxCtrl* gate = mFunctionalPageRoot->findChild<LLCheckBoxCtrl>(enable_widget, true);
            if (!gate) ok = false;
            else if (!gate->getValue().asBoolean() && gate->isInVisibleChain())
            {
                mFunctionalRevealWidgetValues.emplace_back(gate->getDerivedHandle<LLUICtrl>(),
                                                            gate->getValue());
                std::string reason;
                ok = functionalReachable(gate, reason) && functionalPointerClick() &&
                    gate->getValue().asBoolean() && ok;
            }
        }
        // XUI enabled_control bindings are live control variables.  Walking
        // the tested control and every parent catches nested panel/card gates
        // even when the generated manifest is stale.
        for (LLView* view = control; view; view = view->getParent())
        {
            if (LLUICtrl* ui = dynamic_cast<LLUICtrl*>(view))
            {
                if (LLControlVariable* gate = ui->getEnabledControlVariable())
                {
                    rememberFunctionalSetting(gate->getName());
                    ok = functionalSetControl(gate, true) && ok;
                }
            }
        }
        return ok;
    }

    static bool functionalSpinnerTextFits(LLUICtrl* control)
    {
        LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(control);
        if (!spinner || !spinner->getVisible()) return true;
        LLLineEditor* editor = findFirstView<LLLineEditor>(spinner);
        return editor && !editor->getText().empty() && editor->getRect().getWidth() > 0 &&
               FunctionalLineEditorAccess::textRight(editor) >
                   FunctionalLineEditorAccess::textLeft(editor);
    }

    static bool functionalFocusRing(LLView* view)
    {
        if (ALKitSwitch* sw = dynamic_cast<ALKitSwitch*>(view))
            return sw->isFocusRingVisible();
        if (LLButton* button = dynamic_cast<LLButton*>(view))
            return button->isFocusRingVisible();
        return false;
    }

    static bool functionalAnyFocusRing(LLView* view)
    {
        if (!view || !view->isInVisibleChain()) return false;
        if (functionalFocusRing(view)) return true;
        for (LLView* child : *view->getChildList())
            if (functionalAnyFocusRing(child)) return true;
        return false;
    }

    static LLUICtrl* functionalFirstKitControl(LLView* view)
    {
        if (!view || !view->isInVisibleChain()) return nullptr;
        if (view->getEnabled())
        {
            if (ALKitSwitch* sw = dynamic_cast<ALKitSwitch*>(view)) return sw;
            if (LLButton* button = dynamic_cast<LLButton*>(view))
                for (LLView* parent = button->getParent(); parent; parent = parent->getParent())
                    if (dynamic_cast<ALKitPage*>(parent)) return button;
        }
        for (LLView* child : *view->getChildList())
            if (LLUICtrl* found = functionalFirstKitControl(child)) return found;
        return nullptr;
    }

    static LLButton* functionalVerifiedTabButton(LLTabContainer* tabs)
    {
        if (!tabs) return nullptr;
        // Tab buttons are direct children created one-for-one with mTabList.
        // Match both the panel title and direct membership so a page button can
        // never be mistaken for the sentinel target.
        for (S32 panel_index = 0; panel_index < tabs->getTabCount(); ++panel_index)
        {
            const std::string title = tabs->getPanelTitle(panel_index);
            for (LLView* child : *tabs->getChildList())
            {
                LLButton* button = dynamic_cast<LLButton*>(child);
                if (button && button->getParent() == tabs &&
                    (button->getLabelSelected() == title ||
                     button->getLabelUnselected() == title) &&
                    button->isInVisibleChain() && button->isInEnabledChain())
                    return button;
            }
        }
        return nullptr;
    }

    static LLTabContainer* functionalFirstVisibleTabs(LLView* view)
    {
        if (!view) return nullptr;
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(view);
            tabs && tabs->isInVisibleChain() && tabs->isInEnabledChain()) return tabs;
        for (LLView* child : *view->getChildList())
            if (LLTabContainer* found = functionalFirstVisibleTabs(child)) return found;
        return nullptr;
    }

    void runFunctionalSentinel()
    {
        mFunctionalSentinelDone = true;
        LLSD assertion;
        assertion["version"] = mDirectorV2 ? "v2" : "v1";
        assertion["page"] = "director";
        assertion["widget_name"] = "real_dispatch_sentinel";
        assertion["widget_type"] = "sentinel";
        assertion["exercise"] = "click verified navigation control";
        LLFloater* director = mFloater.get();
        LLButton* nav = nullptr;
        if (mDirectorV2)
        {
            ALKitShell* shell = director ? findFirstView<ALKitShell>(director) : nullptr;
            if (shell) shell->selectPage("temporal_tab");
            LLButton* candidate = shell ? shell->findChild<LLButton>("kit_tab_temporal_tab", true) : nullptr; // ui-draw-lint: allow(lookup) test-only one-shot sentinel at run start
            if (shell && candidate && candidate->getParent() == shell->tabStrip() &&
                std::find(shell->tabButtons().begin(), shell->tabButtons().end(), candidate) !=
                    shell->tabButtons().end())
                nav = candidate;
        }
        else
        {
            LLTabContainer* tabs = director ? functionalFirstVisibleTabs(director) : nullptr;
            nav = functionalVerifiedTabButton(tabs);
        }
        std::string reason;
        bool ok = gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI) &&
            nav && functionalReachable(nav, reason) && functionalPointerClick();
        addFunctionalResult(assertion, ok ? "PASS" : "FAIL",
            ok ? "real dispatch channel accepted a verified navigation click" :
                 "injection channel broken: " + (mFunctionalLastClickReason.empty() ? reason : mFunctionalLastClickReason));
        mFunctionalObserved = LLSD::emptyMap();
        if (!ok)
        {
            mFunctionalInjectionBroken = true;
            mFunctionalIndex = mFunctionalEntries.size();
        }
    }

    void addFunctionalResult(const LLSD& entry, const std::string& status,
                             const std::string& reason, const std::string& observed = {})
    {
        // Every row owns and tears down only the input state it created.  Do
        // this before serializing diagnostics so a cleanup is itself visible.
        functionalResetInputState();
        FunctionalResult row;
        row.mPage = entry["page"].asString();
        row.mControl = entry["widget_name"].asString();
        row.mKind = entry["widget_type"].asString();
        row.mAction = entry["exercise"].asString();
        row.mExpected = functionalValue(entry["expect"]);
        LLSD diagnostics = mFunctionalObserved.isMap() ? mFunctionalObserved : LLSD::emptyMap();
        diagnostics["host"]["floater"] = entry["host"]["floater"].asString().empty() ?
            "director" : entry["host"]["floater"].asString();
        diagnostics["host"]["page"] = entry["page"].asString();
        diagnostics["host"]["frontmost"] = gFloaterView && mFunctionalHostFloater.get() &&
            gFloaterView->getFrontmost() == mFunctionalHostFloater.get();
        diagnostics["manifest_action"] = entry["action"].asString();
        if (!observed.empty()) diagnostics["detail"] = observed;
        if (!diagnostics.has("reveal")) diagnostics["reveal"] = LLSD::emptyArray();
        if (!diagnostics.has("click")) diagnostics["click"] = LLSD::emptyMap();
        row.mObserved = LlsdToJson(diagnostics);
        row.mStatus = status;
        row.mReason = reason;
        row.mInWorld = mInWorld;
        mFunctionalResults.push_back(std::move(row));
        if (status == "FAIL" && mFunctionalShots)
        {
            const std::string quota_key = entry["version"].asString() + "/" +
                entry["page"].asString() + "/" + reason;
            if (mFunctionalShotQuota[quota_key]++ < 2)
            {
                auto safe = [](std::string value)
                {
                    for (char& ch : value) if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_') ch = '_';
                    if (value.size() > 80) value.resize(80);
                    return value.empty() ? std::string("row") : value;
                };
                const std::filesystem::path folder = std::filesystem::path(
                    gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "functional_shots")) /
                    entry["version"].asString() / entry["page"].asString();
                std::error_code error;
                std::filesystem::create_directories(folder, error);
                mCaptureFilename = (folder / (safe(entry["widget_name"].asString()) + "__" +
                    safe(reason) + ".png")).string();
                mCapturedImage = nullptr;
                mCaptureFailed = false;
                mCaptureRequested = true;
                mFunctionalFailureShotPending = true;
                mFunctionalShotWaitFrames = 0;
            }
        }
        LL_INFOS("UIFunctionalSelfTest") << entry["version"].asString() << ' '
            << entry["page"].asString() << '/' << entry["widget_name"].asString()
            << " status=" << status << " reason=" << reason
            << " observed=" << mFunctionalResults.back().mObserved << LL_ENDL;
    }

    bool loadFunctionalManifest()
    {
        if (!mFunctionalFilterError.empty()) return false;
        llifstream stream(mFunctionalManifestPath.c_str(), std::ios::binary);
        if (!stream.is_open()) return false;
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        LLSD root;
        std::string error;
        if (!LlsdFromJsonString(buffer.str(), root, &error))
        {
            LL_WARNS("UIFunctionalSelfTest") << "Manifest parse failed: " << error << LL_ENDL;
            return false;
        }
        const std::string version = mDirectorV2 ? "v2" : "v1";
        const LLSD& entries = root["entries"];
        for (LLSD::array_const_iterator it = entries.beginArray(); it != entries.endArray(); ++it)
        {
            if ((*it)["version"].asString() != version) continue;
            if (!mFunctionalVersionFilter.empty() && mFunctionalVersionFilter != "both" &&
                mFunctionalVersionFilter != version) continue;
            if (!mFunctionalPageFilters.empty() && std::find(mFunctionalPageFilters.begin(),
                    mFunctionalPageFilters.end(), (*it)["page"].asString()) == mFunctionalPageFilters.end()) continue;
            if (mFunctionalHaveRowFilter && !std::regex_search((*it)["widget_name"].asString(),
                                                               mFunctionalRowFilter)) continue;
            mFunctionalEntries.push_back(*it);
        }
        if (!functionalLightIndexSelfTest())
        {
            LLSD failure;
            failure["page"] = "selftest";
            failure["widget_name"] = "functionalLightIndex";
            failure["widget_type"] = "unit";
            failure["exercise"] = "map 4 Rig + 4 Rim light suffixes";
            addFunctionalResult(failure, "FAIL", "per-light suffix mapping unit check failed");
        }
        return !mFunctionalEntries.empty();
    }

    void runFunctionalKeyboardAssertion()
    {
        mFunctionalGlobalAssertionsDone = true;
        if (!mDirectorV2) return;
        LLSD assertion;
        assertion["page"] = "director";
        assertion["widget_name"] = "keyboard_tab_focus";
        assertion["widget_type"] = "focus";
        assertion["exercise"] = "Tab";
        LLFloater* director = mFloater.get();
        LLUICtrl* first = director ? functionalFirstKitControl(director) : nullptr;
        if (first) first->setFocus(true);
        LLView* before = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
        const bool focus_verified = director && first && before == first &&
            (before == director || before->hasAncestor(director));
        if (focus_verified) director->handleKeyHere(KEY_TAB, MASK_NONE);
        LLView* after = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
        const bool ring = focus_verified && after &&
            (after == director || after->hasAncestor(director)) && after != before &&
            functionalFocusRing(after) && !functionalFocusRing(before);
        addFunctionalResult(assertion, ring ? "PASS" : "FAIL",
            ring ? "Tab exposes a kit focus ring" :
                              "Tab from a focused kit control did not move to the next kit focus ring");

        LLSD masking;
        masking["page"] = "director";
        masking["widget_name"] = "masking_count_positive";
        masking["widget_type"] = "gate";
        masking["exercise"] = "in-world actor-only projector";
        addFunctionalResult(masking, "SKIP",
            "positive masking-count sub-check requires a logged-in avatar so the rig can create an actor-only projector; zero-count banner checks still run at login");
    }

    void prepareFunctionalRestore(const LLSD& entry)
    {
        mFunctionalRestore.clear();
        const std::string page = entry["page"].asString();
        const std::string setting = entry["expect"]["setting"].asString();
        if (page == "shadows_probes_tab" &&
            (setting == "RenderShadowDetail" || setting == "RenderDeferredSSAO"))
        {
            if (mFunctionalShaderRestore.empty())
            {
                mFunctionalShaderSkipBefore = LLViewerShaderMgr::sSkipReload;
                LLViewerShaderMgr::sSkipReload = true;
            }
            if (!mFunctionalShaderRestore.count(setting))
                if (LLControlVariable* control = gSavedSettings.getControl(setting).get())
                    mFunctionalShaderRestore[setting] = {
                        control->get(), control->getSaveValue(), control->hasUnsavedValue() };
        }
        rememberFunctionalSetting(entry["expect"]["setting"].asString());
        rememberFunctionalSetting(entry["setting"].asString());
        rememberFunctionalSetting("DirectorV2SelectedLight");
        const LLSD& reveals = entry["reveal"];
        if (reveals.isArray())
            for (LLSD::array_const_iterator it = reveals.beginArray(); it != reveals.endArray(); ++it)
                rememberFunctionalSetting((*it)["selector"].asString());
        if (entry["page"].asString() == "weather_tab")
            rememberFunctionalSetting("AlchemyWeatherEnabled");
        const LLSD& settings = entry["restore"]["snapshot_settings"];
        if (settings.isArray())
            for (LLSD::array_const_iterator it = settings.beginArray(); it != settings.endArray(); ++it)
                rememberFunctionalSetting(it->asString());
        if (entry["probe"].asString() == "preset_strip" ||
            !entry["provider"].asString().empty() || settings.isArray() ||
            entry["snapshot_all"].asBoolean())
        {
            // Presets/providers are allowed to touch broad state.  Snapshot
            // the entire control group at the entry boundary so order cannot
            // leak into the next row.
            SaveControls save(mFunctionalRestore);
            gSavedSettings.applyToAll(&save);
        }
        if (entry["snapshot_rig"].asBoolean() ||
            entry["page"].asString() == "cine_light_rig_tab" ||
            entry["page"].asString() == "light_rim_tab")
        {
            mFunctionalRigRowSnapshot = ALCineLightRigManager::instance().sceneData();
        }
    }

    // ui-draw-lint: allow-function(setting-write,raw-write) isolated test-only typed restore
    void restoreFunctionalAction()
    {
        // addFunctionalResult() normally ended the row already.  This second
        // guard also covers shutdown and any future restore-only failure path.
        functionalResetInputState();
        functionalBeginInputState();
        for (auto& saved : mFunctionalRevealWidgetValues)
            if (LLUICtrl* control = saved.first.get())
            {
                control->setValue(saved.second);
                control->onCommit();
            }
        mFunctionalRevealWidgetValues.clear();
        if (LLUICtrl* secondary = mFunctionalSecondaryControl.get();
            secondary && mFunctionalSecondaryBefore.isDefined())
        {
            secondary->setValue(mFunctionalSecondaryBefore);
            secondary->onCommit();
        }
        if (LLUICtrl* control = mFunctionalControl.get();
            control && mFunctionalOriginalControlValue.isDefined())
        {
            if (!mFunctionalSetting &&
                mFunctionalExercise == "toggle" &&
                control->getValue().asBoolean() !=
                    mFunctionalOriginalControlValue.asBoolean())
            {
                functionalPointerClick();
            }
            control->setValue(mFunctionalOriginalControlValue);
            if (mDirectorV2 &&
                mFunctionalEntries[mFunctionalIndex]["widget_name"].asString() ==
                "cine_fixture_role")
            {
                // The role is code-owned panel state.  Restore through its
                // public commit path so later fixture rows address the role
                // shown by the combo instead of the previously exercised one.
                control->onCommit();
            }
        }
        // Restore auto-Custom preset selectors before their owned settings,
        // and keep every write under the materializing guard.  Otherwise an
        // alphabetical restore of CineDiopter* edits a preset-owned value
        // while the selector is non-Custom and snapshots the preset to disk.
        ALScopedPresetMaterializing preset_guard;
        static const char* const preset_selectors[] = {
            "CineDiopterPreset", "CineDiopterKalPreset"
        };
        for (const char* name : preset_selectors)
        {
            const auto found = mFunctionalRestore.find(name);
            if (found == mFunctionalRestore.end()) continue;
            if (LLControlVariable* control = gSavedSettings.getControl(name).get())
            {
                control->setValue(found->second.mSaved, true); // ui-draw-lint: allow(setting-write) bounded functional selector restore
                if (found->second.mUnsaved)
                    control->setValue(found->second.mActive, false); // ui-draw-lint: allow(setting-write) bounded functional selector restore
            }
        }
        for (const auto& saved : mFunctionalRestore)
        {
            if (saved.first == "CineDiopterPreset" ||
                saved.first == "CineDiopterKalPreset") continue;
            if (mFunctionalShaderRestore.count(saved.first)) continue;
            if (LLControlVariable* control = gSavedSettings.getControl(saved.first).get())
            {
                control->setValue(saved.second.mSaved, true); // ui-draw-lint: allow(setting-write) bounded functional saved-value restore
                if (saved.second.mUnsaved)
                    control->setValue(saved.second.mActive, false); // ui-draw-lint: allow(setting-write) bounded functional active-value restore
            }
        }
        if (mFunctionalRigRowSnapshot.isMap())
            ALCineLightRigManager::instance().applySceneData(mFunctionalRigRowSnapshot);
        if (mFunctionalHaveGazeBefore)
        {
            LLActorMover::instance().setGazeTargetConfig(mFunctionalGazeActor,
                                                          mFunctionalGazeBefore);
            mFunctionalHaveGazeBefore = false;
        }
        if (mFunctionalHaveActorPathBefore)
        {
            LLActorMover& mover = LLActorMover::instance();
            mover.stop(mFunctionalActorMoverId);
            if (mFunctionalActorPathBefore.isMap())
                mover.applyPathSceneData(mFunctionalActorMoverId,
                                         mFunctionalActorPathBefore);
            else
                mover.clearPath(mFunctionalActorMoverId);
            if (mFunctionalActorPathWalkingBefore && mFunctionalActorPathBefore.isMap())
                mover.start(mFunctionalActorMoverId);
            mFunctionalHaveActorPathBefore = false;
            mFunctionalActorPathWalkingBefore = false;
        }
        if (mFunctionalOwnerModelBefore.isMap() &&
            mFunctionalOwnerModelBefore.has("previewing") &&
            !mFunctionalOwnerModelBefore["previewing"].asBoolean())
        {
            LLPathCamera::instance().stopPreview();
        }
        if (ALPanelPathEditor* panel = mFunctionalPathPanel.get())
        {
            panel->exitEditMode();
            panel->setTargetActor(mFunctionalPathTargetBefore);
            if (mFunctionalPathEditBefore && mFunctionalPathTargetBefore.notNull())
            {
                if (LLCheckBoxCtrl* edit = mFunctionalPathEditCheck.get())
                {
                    edit->setValue(true); // ui-draw-lint: allow(raw-write) test-only path fixture restore
                    edit->onCommit();
                }
            }
        }
        mFunctionalPathPanel = LLHandle<ALPanelPathEditor>();
        mFunctionalPathEditCheck = LLHandle<LLCheckBoxCtrl>();
        mFunctionalPathTargetBefore.setNull();
        mFunctionalPathEditBefore = false;
        if (!mFunctionalCastSelectionBefore.empty() || !mFunctionalPathActorWasCast ||
            mFunctionalPathLeaderId.notNull())
        {
            if (LLScrollListCtrl* cast_list = mFunctionalCastList.get())
            {
                cast_list->deselectAllItems();
                for (const LLUUID& id : mFunctionalCastSelectionBefore)
                    cast_list->selectByID(id);
                cast_list->onCommit();
            }
            if (!mFunctionalPathActorWasCast && mFunctionalActorMoverId.notNull())
                LLDirectorCast::instance().remove(mFunctionalActorMoverId);
            if (mFunctionalPathLeaderId.notNull())
                LLDirectorCast::instance().remove(mFunctionalPathLeaderId);
        }
        mFunctionalCastSelectionBefore.clear();
        mFunctionalCastList = LLHandle<LLScrollListCtrl>();
        mFunctionalPathActorWasCast = true;
        mFunctionalPathLeaderId.setNull();
        mFunctionalPathExpected = LLSD();
        mFunctionalDanceSelectedObject = nullptr;
        if (mFunctionalGhostFixture.notNull())
        {
            ALGhostStudio& studio = ALGhostStudio::instance();
            studio.removeInstance(mFunctionalGhostFixture);
            studio.setSelected(mFunctionalGhostSelectionBefore);
            mFunctionalGhostFixture.setNull();
            mFunctionalGhostSelectionBefore.setNull();
        }
        if (mFunctionalHaveActorStyleBefore)
        {
            LLDirectorCast::instance().setActorStyle(LLUUID::null,
                                                     mFunctionalActorStyleBefore);
            mFunctionalHaveActorStyleBefore = false;
        }
        for (const LLUUID& id : mFunctionalPropFixtures)
            if (ALObjectPathMover::isTarget(id)) ALObjectPathMover::toggleTarget(id);
        mFunctionalPropFixtures.clear();
        if (mFunctionalPropObject.notNull())
        {
            const LLUUID id = mFunctionalPropObject->getID();
            if (ALObjectPathMover::isTarget(id)) ALObjectPathMover::toggleTarget(id);
            mFunctionalPropObject->markDead();
            mFunctionalPropObject = nullptr;
            mFunctionalPropSelectionAdded = false;
        }
        if (mFunctionalDanceBefore.isMap())
        {
            ALDanceBall::instance().importSession(mFunctionalDanceBefore);
            ALDanceBall::instance().setSpeed(
                mFunctionalDanceBefore["timing"]["speed"].asReal());
            ALDanceBall::instance().setFrozen(mFunctionalDanceFrozenBefore);
            ALDanceBall::instance().setEnabled(mFunctionalDanceEnabledBefore);
            mFunctionalDanceBefore = LLSD();
        }
        if (mFunctionalClipboardPrepared)
        {
            const LLWString restore = mFunctionalClipboardHadText
                ? mFunctionalClipboardBefore : LLWString();
            LLClipboard::instance().copyToClipboard(restore, 0,
                static_cast<S32>(restore.size()));
            mFunctionalClipboardBefore.clear();
            mFunctionalClipboardHadText = false;
            mFunctionalClipboardPrepared = false;
        }
        if (AnimationExplorer* explorer = mFunctionalExplorer.get())
        {
            if (LLScrollListCtrl* animations = mFunctionalExplorerList.get())
            {
                const S32 animation_column = animations->getColumn("animation_id")->mIndex;
                for (S32 index = animations->getItemCount() - 1; index >= 0; --index)
                    if (LLScrollListItem* item = animations->getItemByIndex(index);
                        item && item->getColumn(animation_column)->getValue().asUUID() ==
                            mFunctionalExplorerFixture)
                        animations->deleteSingleItem(index);
                animations->deselectAllItems();
                if (mFunctionalExplorerSelectionBefore.notNull())
                    for (S32 index = 0; index < animations->getItemCount(); ++index)
                        if (LLScrollListItem* item = animations->getItemByIndex(index);
                            item && item->getColumn(animation_column)->getValue().asUUID() ==
                                mFunctionalExplorerSelectionBefore)
                        {
                            animations->selectNthItem(index);
                            break;
                        }
                animations->onCommit();
            }
            if (!mFunctionalExplorerWasVisible) explorer->closeFloater(false);
        }
        mFunctionalExplorer = LLHandle<AnimationExplorer>();
        mFunctionalExplorerList = LLHandle<LLScrollListCtrl>();
        mFunctionalExplorerFixture.setNull();
        mFunctionalExplorerSelectionBefore.setNull();
        mFunctionalExplorerWasVisible = false;
        if (!mFunctionalDanceActorWasCast && gAgentID.notNull())
            LLDirectorCast::instance().remove(gAgentID);
        mFunctionalDanceActorWasCast = true;
        mFunctionalDanceFixturePrepared = false;
        if (mFunctionalHaveCastGazeBefore)
        {
            LLDirectorCast::instance().setGazeTarget(mFunctionalGazeActor,
                                                      mFunctionalCastGazeBefore);
            LLDirectorCast::instance().setLookAtCamera(mFunctionalGazeActor,
                                                        mFunctionalCastLookBefore);
            LLActorMover::instance().setGazeEnabled(mFunctionalGazeActor,
                                                    mFunctionalGazeEnabledBefore);
            if (LLComboBox* edit_actor = mFunctionalGazeEditActor.get())
            {
                edit_actor->setValue(mFunctionalGazeEditActorBefore);
                edit_actor->onCommit();
            }
            mFunctionalGazeEditActor = LLHandle<LLComboBox>();
            mFunctionalHaveCastGazeBefore = false;
        }
        if (mFunctionalGazeCastFixture.notNull())
        {
            LLDirectorCast::instance().remove(mFunctionalGazeCastFixture);
            mFunctionalGazeCastFixture.setNull();
        }
        if (mFunctionalGazeCastFixtureAlternate.notNull())
        {
            LLDirectorCast::instance().remove(mFunctionalGazeCastFixtureAlternate);
            mFunctionalGazeCastFixtureAlternate.setNull();
        }
        if (mFunctionalHaveCastLookBefore)
        {
            LLDirectorCast::instance().setLookAtCamera(mFunctionalGazeActor,
                                                        mFunctionalCastLookOnlyBefore);
            mFunctionalHaveCastLookBefore = false;
        }
        if (mFunctionalHaveGhostOwnerBefore)
        {
            ALGhostStudio::instance().setShowAll(mFunctionalGhostShowAllBefore);
            ALToolGhostEdit* tool = ALToolGhostEdit::instanceExists()
                ? ALToolGhostEdit::getInstance() : nullptr;
            if (tool)
            {
                if (mFunctionalGhostEditingBefore)
                    tool->beginEditFor(mFunctionalGhostEditOwnerBefore);
                else if (tool->isEditModeActive())
                    tool->stopEditMode();
            }
            mFunctionalHaveGhostOwnerBefore = false;
        }
        if (mFunctionalFlycamFixture)
        {
            if (LLFlycamRecorder::instance().getState() == LLFlycamRecorder::STATE_RECORDING)
                LLFlycamRecorder::instance().stopRecording();
            mFunctionalFlycamFixture = false;
        }
        if (mFunctionalHaveLocalFogBefore)
        {
            ALLocalFogManager& manager = ALLocalFogManager::instance();
            manager.volumes() = mFunctionalLocalFogBefore;
            manager.setSelected(mFunctionalLocalFogSelectedBefore);
            manager.touchBank();
            mFunctionalHaveLocalFogBefore = false;
        }
        if (gFloaterView && !mFunctionalFloatersBefore.empty())
        {
            std::set<LLFloater*> open;
            functionalVisibleFloaters(gFloaterView, open);
            for (LLFloater* floater : open)
                if (!mFunctionalFloatersBefore.count(floater) && floater != mFloater.get())
                    floater->closeFloater(false);
        }
        if (mFunctionalLauncherWasOpen)
            if (LLFloater* floater = mFunctionalLauncherFloater.get())
                floater->openFloater(floater->getKey());
        if (mFunctionalIndex < mFunctionalEntries.size() &&
            mFunctionalEntries[mFunctionalIndex]["probe"].asString() == "exposure_history")
            ALExposureSettingsHistory::clear();
        functionalResetInputState();
        mFunctionalRestore.clear();
        mFunctionalControl = LLHandle<LLUICtrl>();
        mFunctionalSetting = nullptr;
        mFunctionalPageRoot = nullptr;
        if (LLFloater* redirected = mFunctionalRedirectFloater.get())
            if (!mFunctionalFailureShotPending) redirected->closeFloater(false);
        if (!mFunctionalFailureShotPending) mFunctionalRedirectFloater = LLHandle<LLFloater>();
        mFunctionalControlBefore = LLSD();
        mFunctionalRadioExpected = LLSD();
        mFunctionalListExpected = LLSD();
        mFunctionalOriginalControlValue = LLSD();
        mFunctionalSecondaryControl = LLHandle<LLUICtrl>();
        mFunctionalSecondaryBefore = LLSD();
        mFunctionalRigRowSnapshot = LLSD();
        mFunctionalLauncherFloater = LLHandle<LLFloater>();
        mFunctionalLauncherWasOpen = false;
        mFunctionalExercise.clear();
        mFunctionalSettingName.clear();
        mFunctionalDynamicSetting = false;
        mFunctionalGatePrepared = false;
        mFunctionalRevealPending = false;
        mFunctionalClickTarget = LLHandle<LLView>();
        if (!mFunctionalFailureShotPending) mFunctionalHostFloater = LLHandle<LLFloater>();
        mFunctionalRedirectReason.clear();
        mFunctionalCommitConnection.disconnect();
        mFunctionalCommitCount = 0;
        mFunctionalHandlerRunCount = 0;
        mFunctionalObserved = LLSD::emptyMap();
        mFunctionalLastClickReason.clear();
    }

    // ui-draw-lint: allow-function(setting-write) page-batched functional shader restore
    void finishFunctionalShaderBatch()
    {
        if (mFunctionalShaderRestore.empty()) return;
        bool changed = false;
        for (const auto& saved : mFunctionalShaderRestore)
        {
            if (LLControlVariable* control = gSavedSettings.getControl(saved.first).get())
            {
                changed = changed || control->get() != saved.second.mActive ||
                    control->getSaveValue() != saved.second.mSaved;
                control->setValue(saved.second.mSaved, true); // ui-draw-lint: allow(setting-write) exact batched saved-value restore
                if (saved.second.mUnsaved)
                    control->setValue(saved.second.mActive, false); // ui-draw-lint: allow(setting-write) exact batched active-value restore
            }
        }
        LLViewerShaderMgr::sSkipReload = mFunctionalShaderSkipBefore;
        if (changed && !mFunctionalShaderSkipBefore)
            LLViewerShaderMgr::instance()->setShaders();
        mFunctionalShaderRestore.clear();
    }

    static void functionalCollectButtons(LLView* view, std::vector<LLButton*>& buttons)
    {
        if (!view) return;
        if (LLButton* button = dynamic_cast<LLButton*>(view)) buttons.push_back(button);
        for (LLView* child : *view->getChildList()) functionalCollectButtons(child, buttons);
    }

    LLView* functionalClickTarget(LLUICtrl* control)
    {
        if (LLCheckBoxCtrl* box = dynamic_cast<LLCheckBoxCtrl*>(control))
            return FunctionalCheckBoxAccess::button(box);
        if (LLComboBox* combo = dynamic_cast<LLComboBox*>(control))
            return FunctionalComboAccess::button(combo);
        if (LLSpinCtrl* spin = dynamic_cast<LLSpinCtrl*>(control))
        {
            std::vector<LLButton*> buttons;
            functionalCollectButtons(spin, buttons);
            if (buttons.empty()) return spin;
            const bool up = mFunctionalEntries[mFunctionalIndex]["test_value"].asReal() >=
                control->getValue().asReal();
            return up ? *std::max_element(buttons.begin(), buttons.end(), [](LLButton* a, LLButton* b)
                    { return a->getRect().getCenterY() < b->getRect().getCenterY(); }) :
                *std::min_element(buttons.begin(), buttons.end(), [](LLButton* a, LLButton* b)
                    { return a->getRect().getCenterY() < b->getRect().getCenterY(); });
        }
        if (LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(control))
            if (LLSlider* track = findFirstView<LLSlider>(slider)) return track;
        if (LLRadioGroup* radio = dynamic_cast<LLRadioGroup*>(control))
        {
            const LLSD original = radio->getValue();
            S32 wanted_index = mFunctionalEntries[mFunctionalIndex]["test_value"]["item_index"].asInteger();
            if (wanted_index < 0 || wanted_index >= radio->getItemCount() ||
                wanted_index == radio->getSelectedIndex())
            {
                wanted_index = radio->getItemCount() > 1
                    ? (radio->getSelectedIndex() + 1) % radio->getItemCount() : -1;
            }
            if (wanted_index < 0 || !radio->setSelectedIndex(wanted_index, true)) return control;
            mFunctionalRadioExpected = radio->getValue();
            std::vector<LLCheckBoxCtrl*> items;
            std::function<void(LLView*)> collect = [&](LLView* view)
            {
                if (LLCheckBoxCtrl* item = dynamic_cast<LLCheckBoxCtrl*>(view)) items.push_back(item);
                for (LLView* child : *view->getChildList()) collect(child);
            };
            collect(radio);
            LLView* target = nullptr;
            for (LLCheckBoxCtrl* item : items)
                if (item->getValue().asBoolean())
                {
                    target = FunctionalCheckBoxAccess::button(item);
                    break;
                }
            radio->setValue(original);
            if (target) return target;
        }
        return control;
    }

    static bool functionalKitSwitchGeometry(LLView* view, std::string& bad_switch)
    {
        if (!view) return true;
        if (ALKitSwitch* kit_switch = dynamic_cast<ALKitSwitch*>(view))
        {
            const LLRect drawn = kit_switch->drawnTrackRect();
            if (kit_switch->getUseBoundingRect() || drawn.isEmpty() ||
                !kit_switch->getLocalRect().contains(drawn) ||
                !kit_switch->clickableRect().contains(drawn))
            {
                bad_switch = kit_switch->getPathname();
                return false;
            }
        }
        for (LLView* child : *view->getChildList())
            if (!functionalKitSwitchGeometry(child, bad_switch)) return false;
        return true;
    }

    bool functionalKitCardSwitchGeometry(std::string& bad_switch)
    {
        if (!mDirectorV2 || !mFunctionalPageRoot) return true;
        std::vector<ALKitCard*> cards;
        collectViewsOfType<ALKitCard>(mFunctionalPageRoot, cards);
        for (ALKitCard* card : cards)
            if (!functionalKitSwitchGeometry(card, bad_switch)) return false;
        return true;
    }

    static LLSD functionalRect(const LLRect& rect)
    {
        LLSD result = LLSD::emptyArray();
        result.append(rect.mLeft); result.append(rect.mBottom);
        result.append(rect.mRight); result.append(rect.mTop);
        return result;
    }

    static LLSD functionalChainBreak(LLView* control, bool visible)
    {
        for (LLView* view = control; view; view = view->getParent())
        {
            const bool ok = visible ? view->getVisible() : view->getEnabled();
            if (!ok)
            {
                LLSD result = LLSD::emptyMap();
                result["path"] = view->getPathname();
                result["class"] = typeid(*view).name();
                return result;
            }
        }
        return LLSD();
    }

    static LLFloater* functionalOwningFloater(LLView* view)
    {
        for (; view; view = view->getParent())
            if (LLFloater* floater = dynamic_cast<LLFloater*>(view)) return floater;
        return nullptr;
    }

    // ui-draw-lint: allow-function(lookup) bounded login-only non-dispatching hit-test
    static LLView* functionalResolveInView(LLView* parent, S32 screen_x, S32 screen_y)
    {
        if (!parent || !parent->getVisible() || !parent->getEnabled()) return nullptr;
        for (LLView* child : *parent->getChildList())
        {
            S32 local_x = 0;
            S32 local_y = 0;
            child->screenPointToLocal(screen_x, screen_y, &local_x, &local_y);
            if (!child->getVisible() || !child->getEnabled() ||
                !child->pointInView(local_x, local_y, LLView::HIT_TEST_USE_BOUNDING_RECT))
                continue;
            if (LLView* deeper = functionalResolveInView(child, screen_x, screen_y))
                return deeper;
            // This is the non-mutating equivalent of the dispatch fall-through:
            // transparent holders do not win; controls and explicit blockers do.
            if (child->getMouseOpaque() || child->blockMouseEvent(local_x, local_y))
                return child;
        }
        S32 local_x = 0;
        S32 local_y = 0;
        parent->screenPointToLocal(screen_x, screen_y, &local_x, &local_y);
        if (parent->pointInView(local_x, local_y, LLView::HIT_TEST_USE_BOUNDING_RECT) &&
            (parent->getMouseOpaque() || parent->blockMouseEvent(local_x, local_y)))
            return parent;
        return nullptr;
    }

    bool functionalOwnedInputLayer(LLView* layer, LLView* expected, LLFloater* host) const
    {
        if (!layer) return true;
        if (layer == expected || layer->hasAncestor(expected) || expected->hasAncestor(layer))
            return true;
        return host && functionalOwningFloater(layer) == host;
    }

    static bool functionalRelatedView(LLView* view, LLView* root)
    {
        return view && root &&
            (view == root || view->hasAncestor(root) || root->hasAncestor(view));
    }

    static std::string functionalViewPath(LLView* view)
    {
        return view ? view->getPathname() : std::string("<none>");
    }

    void functionalBeginInputState()
    {
        if (mFunctionalInputStateActive) return;
        LLView* focus = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
        mFunctionalFocusBefore = focus ? focus->getHandle() : LLHandle<LLView>();
        mFunctionalFocusWasNull = focus == nullptr;
        mFunctionalInputStateActive = true;
        mFunctionalOpenedCombos.clear();
    }

    void functionalTrackCombo(LLComboBox* combo)
    {
        if (!combo) return;
        functionalBeginInputState();
        const LLHandle<LLView> handle = combo->getHandle();
        if (std::none_of(mFunctionalOpenedCombos.begin(), mFunctionalOpenedCombos.end(),
                         [combo](const LLHandle<LLView>& saved) { return saved.get() == combo; }))
            mFunctionalOpenedCombos.push_back(handle);
    }

    void functionalResetInputState()
    {
        if (!mFunctionalInputStateActive && mFunctionalOpenedCombos.empty())
        {
            mFunctionalAllowedCaptor = nullptr;
            return;
        }

        bool closed_popup = false;
        bool released_capture = false;
        bool released_top = false;
        bool restored_focus = false;
        std::vector<LLView*> owned;
        for (const LLHandle<LLView>& handle : mFunctionalOpenedCombos)
        {
            LLComboBox* combo = dynamic_cast<LLComboBox*>(handle.get());
            if (!combo) continue;
            owned.push_back(combo);
            LLScrollListCtrl* list = FunctionalComboAccess::list(combo);
            if (list) owned.push_back(list);
            if (list && gFocusMgr.getMouseCapture() == list)
            {
                gFocusMgr.setMouseCapture(nullptr);
                released_capture = true;
            }
            if (list && list->getVisible())
            {
                combo->hideList();
                closed_popup = true;
            }
        }

        LLView* control = mFunctionalControl.get();
        LLView* click_target = mFunctionalClickTarget.get();
        auto harness_owned = [&](LLView* view)
        {
            if (functionalRelatedView(view, control) || functionalRelatedView(view, click_target))
                return true;
            return std::any_of(owned.begin(), owned.end(),
                [view](LLView* root) { return functionalRelatedView(view, root); });
        };

        // A modal opened by an action is not harness-owned input residue.  It
        // must be handled by that row's modal oracle, never silently cleared.
        if (LLMouseHandler* capture = gFocusMgr.getMouseCapture())
        {
            LLView* capture_view = dynamic_cast<LLView*>(capture);
            if (capture_view && !dynamic_cast<LLModalDialog*>(capture_view) &&
                harness_owned(capture_view))
            {
                gFocusMgr.setMouseCapture(nullptr);
                released_capture = true;
            }
        }
        if (LLUICtrl* top = gFocusMgr.getTopCtrl(); top &&
            !dynamic_cast<LLModalDialog*>(top) && harness_owned(top))
        {
            gFocusMgr.setTopCtrl(nullptr);
            released_top = true;
        }
        if (LLView* focus = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
            focus && harness_owned(focus))
        {
            LLView* prior = mFunctionalFocusBefore.get();
            gFocusMgr.setKeyboardFocus(mFunctionalFocusWasNull ? nullptr : prior);
            restored_focus = true;
        }
        if (closed_popup || released_capture || released_top || restored_focus)
        {
            mFunctionalObserved["input_reset"]["closed_popup"] = closed_popup;
            mFunctionalObserved["input_reset"]["released_capture"] = released_capture;
            mFunctionalObserved["input_reset"]["released_top_ctrl"] = released_top;
            mFunctionalObserved["input_reset"]["restored_focus"] = restored_focus;
        }
        mFunctionalOpenedCombos.clear();
        mFunctionalFocusBefore = LLHandle<LLView>();
        mFunctionalFocusWasNull = false;
        mFunctionalInputStateActive = false;
        mFunctionalAllowedCaptor = nullptr;
    }

    // ui-draw-lint: allow-function(lookup) bounded login-only input safety preflight
    bool functionalPreDispatch(LLView* expected, std::string& reason)
    {
        if (!expected || !gViewerWindow)
        {
            reason = "not reachable: missing click target";
            return false;
        }
        LLFloater* host = mFunctionalHostFloater.get();
        if (!host) host = functionalOwningFloater(expected);
        if (!host) host = mFloater.get();

        auto blocked = [&](const std::string& condition, const std::string& path)
        {
            mFunctionalObserved["pre_dispatch_block"]["condition"] = condition;
            mFunctionalObserved["pre_dispatch_block"]["path"] = path;
            reason = "foreign modal/captor present: " + condition + " " + path;
            return false;
        };

        if (LLMouseHandler* capture = gFocusMgr.getMouseCapture();
            capture && !(capture == mFunctionalAllowedCaptor && capture == dynamic_cast<LLMouseHandler*>(expected)))
        {
            LLView* capture_view = dynamic_cast<LLView*>(capture);
            if (!capture_view || !functionalOwnedInputLayer(capture_view, expected, host))
                return blocked("mouse capture", capture_view ? functionalViewPath(capture_view) :
                    std::string("<non-view mouse handler:") + capture->getName() + ">");
            // A capture from even our own floater changes dispatch. It is only
            // legitimate while a drag is already in progress, never before down.
            return blocked("pre-existing host mouse capture", functionalViewPath(capture_view));
        }
        if (LLModalDialog::activeCount() > 0 && !dynamic_cast<LLModalDialog*>(host))
        {
            LLView* modal_view = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
            while (modal_view && !dynamic_cast<LLModalDialog*>(modal_view))
                modal_view = modal_view->getParent();
            return blocked("active modal", functionalViewPath(modal_view));
        }
        LLUICtrl* top = gFocusMgr.getTopCtrl();
        if (top && !(top == expected || top->hasAncestor(expected) ||
                     expected->hasAncestor(top)))
            return blocked("top control", functionalViewPath(top));
        // Menus and the progress view sit in front of the Floater View in real dispatch
        // (main_view.xml) and take the click/Return before the target; never inject then.
        if (gMenuHolder && gMenuHolder->hasVisibleMenu())
            return blocked("open menu", functionalViewPath(gMenuHolder));
        if (LLProgressView* progress = gViewerWindow->getProgressView();
            progress && progress->isInVisibleChain())
            return blocked("progress view", functionalViewPath(progress));

        LLRootView* root = gViewerWindow->getRootView();
        if (LLPanelLogin* login = root ? findFirstView<LLPanelLogin>(root) : nullptr)
        {
            const LLRect login_rect = login->calcScreenRect();
            mFunctionalObserved["rect"]["login_forbidden"] = functionalRect(login_rect);
            // The login panel spans the whole window at the login screen, so a rect test
            // forbids everything; instead the resolved handler must never be inside it (below).
            mFunctionalLoginPanel = login->getHandle();
        }
        const bool popup_target = expected == dynamic_cast<LLView*>(mFunctionalAllowedCaptor) &&
            expected->calcScreenRect().pointInRect(mFunctionalScreenX, mFunctionalScreenY);
        if (!host || (!popup_target &&
                      !host->calcScreenRect().pointInRect(mFunctionalScreenX, mFunctionalScreenY)))
        {
            reason = "not reachable: outside Director/redirect floater";
            return false;
        }

        LLView* resolved = nullptr;
        if (top)
            resolved = functionalResolveInView(top, mFunctionalScreenX, mFunctionalScreenY);
        if (!resolved && root)
        {
            if (LLPopupView* holder = root->findChild<LLPopupView>("popup_holder", true))
            {
                for (const LLHandle<LLView>& handle : holder->getCurrentPopups())
                {
                    LLView* popup = handle.get();
                    if (popup && popup->isInVisibleChain() &&
                        !(popup == expected || popup->hasAncestor(expected) ||
                          expected->hasAncestor(popup)))
                        return blocked("popup holder entry", functionalViewPath(popup));
                    if ((resolved = functionalResolveInView(popup, mFunctionalScreenX,
                                                            mFunctionalScreenY)))
                        break;
                }
            }
        }
        // Resolve from the root in real dispatch order (no Floater View pre-pass).
        if (!resolved && root)
            resolved = functionalResolveInView(root, mFunctionalScreenX, mFunctionalScreenY);

        mFunctionalObserved["click"]["preflight_resolved"] =
            resolved ? resolved->getPathname() : "<none>";
        if (LLPanel* login = mFunctionalLoginPanel.get())
            if (resolved && (resolved == login || resolved->hasAncestor(login)))
            {
                reason = "not reachable: login panel forbidden";
                return false;
            }
        if (!resolved || !(resolved == expected || resolved->hasAncestor(expected)))
        {
            reason = "not reachable: " +
                (resolved ? resolved->getPathname() : std::string("<none>"));
            return false;
        }
        return true;
    }

    bool functionalReachable(LLUICtrl* control, std::string& reason)
    {
        std::string bad_switch;
        if ((dynamic_cast<ALKitSwitch*>(control) &&
             !functionalKitSwitchGeometry(control, bad_switch)) ||
            !functionalKitCardSwitchGeometry(bad_switch))
        {
            reason = "drawn switch not clickable";
            mFunctionalObserved["switch_geometry_failure"] = bad_switch;
            return false;
        }
        if (!control->isInVisibleChain())
        {
            reason = "not reachable: hidden";
            mFunctionalObserved["visible_break"] = functionalChainBreak(control, true);
            return false;
        }
        if (!control->isInEnabledChain())
        {
            reason = "not reachable: disabled-by-parent";
            mFunctionalObserved["enabled_break"] = functionalChainBreak(control, false);
            return false;
        }
        control->onUpdateScrollToChild(control);
        for (LLView* parent = control->getParent(); parent; parent = parent->getParent())
        {
            LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(parent);
            if (!scroller || !scroller->getScrolledView()) continue;
            LLRect document_rect;
            if (control->localRectToOtherView(control->getLocalRect(), &document_rect,
                                              scroller->getScrolledView()))
                scroller->scrollToShowRect(document_rect);
        }
        if (!control->isInVisibleChain())
        {
            reason = "not reachable: hidden";
            return false;
        }
        LLView* target = functionalClickTarget(control);
        if (!target || !target->isInVisibleChain() || !target->isInEnabledChain())
        {
            reason = "not reachable: target hidden or disabled";
            return false;
        }
        // Compound controls can be visible while their actual click child
        // protrudes beyond the viewport (notably the lower spinner arrow).
        // Reveal the dispatch target, not only the owning LLUICtrl.
        for (LLView* parent = target->getParent(); parent; parent = parent->getParent())
        {
            LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(parent);
            if (!scroller || !scroller->getScrolledView()) continue;
            LLRect document_rect;
            if (target->localRectToOtherView(target->getLocalRect(), &document_rect,
                                             scroller->getScrolledView()))
            {
                scroller->scrollToShowRect(document_rect);
                // Minimal reveal is insufficient when a viewport or host is clipped.
                // Put the target at the top of the usable page before the on-screen
                // oracle, which also makes the click deterministic across window sizes.
                scroller->setDocPosVertical(llmax(0,
                    scroller->getScrolledViewRect().getHeight() - document_rect.mTop));
            }
        }
        S32 local_x = target->getLocalRect().getCenterX();
        S32 local_y = target->getLocalRect().getCenterY();
        if (ALKitSwitch* kit_switch = mDirectorV2 ?
                dynamic_cast<ALKitSwitch*>(control) : nullptr)
        {
            LLRect drawn_in_target;
            if (!kit_switch->localRectToOtherView(kit_switch->drawnTrackRect(),
                                                  &drawn_in_target, target) ||
                !target->getLocalRect().contains(drawn_in_target))
            {
                reason = "drawn switch not clickable";
                return false;
            }
            // Exercise the centre of the pixels the user sees, not the owner
            // widget's independently derived bounds.
            local_x = drawn_in_target.getCenterX();
            local_y = drawn_in_target.getCenterY();
        }
        if (dynamic_cast<LLSlider*>(target))
        {
            const LLSD& entry = mFunctionalEntries[mFunctionalIndex];
            const F64 low = entry["min"].isDefined() ? entry["min"].asReal() : 0.;
            const F64 high = entry["max"].isDefined() ? entry["max"].asReal() : 1.;
            const F64 ratio = high > low ? llclamp((entry["test_value"].asReal() - low) / (high - low), 0.05, 0.95) : .75;
            local_x = target->getLocalRect().mLeft + ll_round((F32)(target->getLocalRect().getWidth() * ratio));
        }
        target->localPointToScreen(local_x, local_y, &mFunctionalScreenX, &mFunctionalScreenY);
        mFunctionalClickTarget = target->getHandle();
        mFunctionalObserved["rect"]["ctrl"] = functionalRect(control->calcScreenRect());
        mFunctionalObserved["rect"]["target"] = functionalRect(target->calcScreenRect());
        mFunctionalObserved["rect"]["point"] = LLSD::emptyArray();
        mFunctionalObserved["rect"]["point"].append(mFunctionalScreenX);
        mFunctionalObserved["rect"]["point"].append(mFunctionalScreenY);
        LLFloater* host = nullptr;
        LLScrollContainer* viewport = nullptr;
        for (LLView* parent = target; parent; parent = parent->getParent())
        {
            if (!viewport) viewport = dynamic_cast<LLScrollContainer*>(parent);
            if (!host) host = dynamic_cast<LLFloater*>(parent);
        }
        bool on_screen = true;
        if (viewport)
        {
            const LLRect rect = viewport->calcScreenRect();
            mFunctionalObserved["rect"]["viewport"] = functionalRect(rect);
            on_screen = on_screen && rect.pointInRect(mFunctionalScreenX, mFunctionalScreenY);
        }
        if (host)
        {
            const LLRect rect = host->calcScreenRect();
            mFunctionalObserved["rect"]["floater"] = functionalRect(rect);
            on_screen = on_screen && rect.pointInRect(mFunctionalScreenX, mFunctionalScreenY);
        }
        if (!on_screen)
        {
            reason = "not on screen";
            return false;
        }
        return functionalPreDispatch(target, reason);
    }

    // ui-draw-lint: allow-function(lookup) bounded six-frame test-only reveal/selector settlement
    void revealFunctionalControl(LLUICtrl* control)
    {
        if (!control) return;
        revealFunctionalAncestors(control);
        ALKitPage* page = dynamic_cast<ALKitPage*>(mFunctionalPageRoot);
        if (!page) page = findFirstView<ALKitPage>(mFunctionalPageRoot);
        ALKitCard* card = nullptr;
        ALKitRow* row = nullptr;
        for (LLView* parent = control->getParent(); parent; parent = parent->getParent())
        {
            if (!row) row = dynamic_cast<ALKitRow*>(parent);
            if ((card = dynamic_cast<ALKitCard*>(parent))) break;
        }
        if (page)
        {
            page->setSearchText("");
            if (card) page->revealCard(card, false);
        }
        // Both calls are required: revealCard establishes mode/chip state and
        // revealRow establishes body/More/option state. Layout is idle-deferred.
        if (card && row) card->revealRow(row);
        control->onUpdateScrollToChild(control);
        for (LLView* parent = control->getParent(); parent; parent = parent->getParent())
        {
            LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(parent);
            if (!scroller || !scroller->getScrolledView()) continue;
            LLRect document_rect;
            if (control->localRectToOtherView(control->getLocalRect(), &document_rect,
                                              scroller->getScrolledView()))
                scroller->scrollToShowRect(document_rect);
        }
        const LLSD& reveals = mFunctionalEntries[mFunctionalIndex]["reveal"];
        if (reveals.isArray())
        {
            for (LLSD::array_const_iterator it = reveals.beginArray(); it != reveals.endArray(); ++it)
            {
                const std::string selector_name = (*it)["selector"].asString();
                LLComboBox* selector = mFunctionalPageRoot ?
                    mFunctionalPageRoot->findChild<LLComboBox>(selector_name, true) : nullptr;
                if (selector && selector->getValue() != (*it)["value"])
                {
                    const bool already_saved = std::any_of(mFunctionalRevealWidgetValues.begin(),
                        mFunctionalRevealWidgetValues.end(), [selector](const auto& saved)
                            { return saved.first.get() == selector; });
                    if (!already_saved) mFunctionalRevealWidgetValues.emplace_back(
                        selector->getDerivedHandle<LLUICtrl>(), selector->getValue());
                    functionalSelectComboValue(selector, (*it)["value"]);
                }
                LLSD action = LLSD::emptyMap();
                action["selector"] = selector_name;
                action["value"] = (*it)["value"];
                mFunctionalObserved["reveal"].append(action);
            }
        }
        LLFloater* host = nullptr;
        for (LLView* parent = control; parent && !host; parent = parent->getParent())
            host = dynamic_cast<LLFloater*>(parent);
        if (host)
        {
            host->setMinimized(false);
            if (gFloaterView) gFloaterView->adjustToFitScreen(host, false);
            host->setFrontmost(true);
            mFunctionalHostFloater = host->getHandle();
        }
    }

    bool functionalPointerClick()
    {
        LLView* expected = mFunctionalClickTarget.get();
        if (!expected || !gViewerWindow) return false;
        std::string preflight;
        if (!functionalPreDispatch(expected, preflight))
        {
            mFunctionalLastClickReason = preflight;
            return false;
        }
        LLCoordGL gl(ll_round((F32)mFunctionalScreenX * gViewerWindow->getDisplayScale().mV[VX]),
                     ll_round((F32)mFunctionalScreenY * gViewerWindow->getDisplayScale().mV[VY]));
        gViewerWindow->handleMouseMove(gViewerWindow->getWindow(), gl, MASK_NONE);
        LLView* deepest = nullptr;
        struct RecorderGuard
        {
            explicit RecorderGuard(LLView*& deepest)
            {
                LLView::sMouseHandlerRecorder = [&deepest](LLView* view)
                    { if (!deepest) deepest = view; };
                LLView::sDebugMouseHandling = true;
            }
            ~RecorderGuard()
            {
                LLView::sDebugMouseHandling = false;
                LLView::sMouseHandlerRecorder = {};
            }
        } recorder(deepest);
        bool toolmgr = false;
        // Capture geometry after all preflight/hover work and immediately before
        // the down event. A pointer click must not move its target before mouse-up.
        const LLRect rect_before_down = expected->calcScreenRect();
        mFunctionalObserved["click"]["rect_before_down"] = functionalRect(rect_before_down);
        const bool down = gViewerWindow->handleAnyMouseClick(
            gViewerWindow->getWindow(), gl, MASK_NONE, CLICK_LEFT, true, toolmgr);
        const std::string down_path = LLView::sMouseHandlerMessage;
        LLView* down_deepest = deepest;
        LLMouseHandler* captor = gFocusMgr.getMouseCapture();
        const bool capture_expected = dynamic_cast<LLButton*>(expected) ||
            dynamic_cast<LLSlider*>(expected);
        const bool captured = captor && (captor == expected || captor == mFunctionalAllowedCaptor ||
            (dynamic_cast<LLView*>(captor) && dynamic_cast<LLView*>(captor)->hasAncestor(expected)));
        const bool capture_ok = !capture_expected || captured;
        // Keep an ordinary click at one screen coordinate. Drag helpers pump
        // updateUI() explicitly; doing that here can advance deferred kit
        // layout between a button press and release and turn a valid click
        // into an artificial release outside the moving child button.
        LLCoordGL release_gl = gl;
        if (LLScrollListCtrl* combo_list = dynamic_cast<LLScrollListCtrl*>(captor);
            combo_list && captor == mFunctionalAllowedCaptor && combo_list->getVisible() &&
            dynamic_cast<LLButton*>(expected))
        {
            // The combo transfers capture to its list during button-down.  A
            // clamped popup can put stale/undrawn item geometry under the
            // button, so release just outside the list exactly as a user can
            // before making the second, item-selection click.
            const LLRect items = combo_list->getItemListRect();
            S32 release_x = 0, release_y = 0;
            combo_list->localPointToScreen(items.mLeft - 2, items.mBottom - 2,
                                           &release_x, &release_y);
            release_gl.set(ll_round((F32)release_x * gViewerWindow->getDisplayScale().mV[VX]),
                           ll_round((F32)release_y * gViewerWindow->getDisplayScale().mV[VY]));
            mFunctionalObserved["click"]["combo_open_release_outside_list"] = true;
        }
        gViewerWindow->handleMouseMove(gViewerWindow->getWindow(), release_gl, MASK_NONE);
        // Diagnose clicks that are handled but don't commit: did the target move between
        // down and up (e.g. a scroll-to-focus jump), or did something else take the capture?
        const LLRect rect_before_up = expected->calcScreenRect();
        const bool target_stable = rect_before_up == rect_before_down;
        mFunctionalObserved["click"]["rect_before_up"] = functionalRect(rect_before_up);
        mFunctionalObserved["click"]["layout_stable"] = target_stable;
        {
            LLMouseHandler* captor_before_up = gFocusMgr.getMouseCapture();
            LLView* captor_view = dynamic_cast<LLView*>(captor_before_up);
            mFunctionalObserved["click"]["captor_before_up"] = !captor_before_up ? LLSD("<none>") :
                LLSD(captor_view ? captor_view->getPathname() : std::string("<non-view>"));
        }
        deepest = nullptr;
        const bool up = gViewerWindow->handleAnyMouseClick(
            gViewerWindow->getWindow(), release_gl, MASK_NONE, CLICK_LEFT, false, toolmgr);
        const std::string up_path = deepest ? deepest->getPathname() :
            LLView::sMouseHandlerMessage;
        // Opening a combo transfers capture from its button to its list.  The
        // opening up is outside the list: LLScrollListCtrl performs the
        // release/callback, then legitimately returns false because no child
        // handled that coordinate.  It was still delivered and completed.
        const bool allowed_captor_up = !up && captor && captor == mFunctionalAllowedCaptor &&
            deepest == dynamic_cast<LLView*>(captor) && !gFocusMgr.getMouseCapture();
        // LLScrollListCtrl deliberately returns its base mouse-up result after
        // committing (and the combo callback hides the popup).  Delivery to the
        // exact list plus a new combo commit is the user-visible success oracle.
        const bool committed_list_up = !up && dynamic_cast<LLScrollListCtrl*>(expected) &&
            deepest == expected && mFunctionalCommitCount > 0;
        const bool handler = down_deepest == expected ||
            (down_deepest && down_deepest->hasAncestor(expected));
        mFunctionalObserved["click"]["down_handler"] = down_path;
        mFunctionalObserved["click"]["up_handler"] = up_path;
        mFunctionalObserved["click"]["capture_expected"] = capture_expected;
        mFunctionalObserved["click"]["captor_ok"] = capture_ok;
        mFunctionalObserved["click"]["capture_after"] = gFocusMgr.getMouseCapture() ? "set" : LLSD();
        mFunctionalObserved["click"]["target"] = expected->getPathname();
        mFunctionalObserved["click"]["handler_exact"] = handler;
        if (allowed_captor_up)
            mFunctionalObserved["click"]["up_delivered_to_allowed_captor"] = true;
        if (committed_list_up)
            mFunctionalObserved["click"]["up_committed_list"] = true;
        LLMouseHandler* stuck = gFocusMgr.getMouseCapture();
        if (stuck && stuck == mFunctionalAllowedCaptor)
            stuck = nullptr; // combo button handed capture to its own dropdown list (llcombobox.cpp:818-820)
        if (LLNotificationsUI::LLToast* toast = dynamic_cast<LLNotificationsUI::LLToast*>(stuck))
        {
            const LLNotificationPtr note = toast->getNotification();
            const std::string actual_name = note ? note->getName() : std::string();
            const std::string expected_name =
                mFunctionalEntries[mFunctionalIndex]["confirm_notification"].asString();
            mFunctionalObserved["click"]["notification"] = actual_name;
            if (note && !expected_name.empty() && actual_name == expected_name)
            {
                note->respondWithDefault();
                gFocusMgr.setMouseCapture(nullptr);
                stuck = nullptr;
                mFunctionalObserved["click"]["notification_answered"] = true;
            }
            else
            {
                mFunctionalLastClickReason = "alert/refusal notification: " +
                    (actual_name.empty() ? std::string("<unknown>") : actual_name);
                return false;
            }
        }
        if (stuck)
        {
            LLView* stuck_view = dynamic_cast<LLView*>(stuck);
            if (stuck_view && !dynamic_cast<LLModalDialog*>(stuck_view) &&
                functionalRelatedView(stuck_view, expected))
            {
                gFocusMgr.setMouseCapture(nullptr);
                mFunctionalLastClickReason = "stuck target capture: " + functionalViewPath(stuck_view);
            }
            else
                mFunctionalLastClickReason = "foreign capture after dispatch: " +
                    (stuck_view ? functionalViewPath(stuck_view) : std::string("<non-view mouse handler>"));
            return false;
        }
        if (!target_stable)
        {
            mFunctionalLastClickReason = "target moved during click (layout shift)";
            return false;
        }
        mFunctionalLastClickReason = handler ? std::string() :
            "real dispatch was handled outside target: " + down_path;
        return down && (up || allowed_captor_up || committed_list_up) && handler && capture_ok;
    }

    bool functionalPointerDrag(LLSlider* slider, const LLSD& target_value)
    {
        if (!slider || !gViewerWindow) return false;
        const LLRect local = slider->getLocalRect();
        const LLRect thumb = slider->getThumbRect();
        const S32 half_thumb = thumb.getWidth() / 2;
        const S32 left_edge = local.mLeft + half_thumb;
        const S32 right_edge = local.mRight - half_thumb;
        const F32 range = slider->getMaxValue() - slider->getMinValue();
        const F32 ratio = range > 0.f ? llclamp(
            ((F32)target_value.asReal() - slider->getMinValue()) / range, 0.f, 1.f) : 0.f;
        const S32 start_x = thumb.getCenterX();
        const S32 local_y = thumb.getCenterY();
        const S32 end_x = left_edge + ll_round(ratio * (F32)(right_edge - left_edge));
        S32 start_screen_x = 0, start_screen_y = 0, end_screen_x = 0, end_screen_y = 0;
        slider->localPointToScreen(start_x, local_y, &start_screen_x, &start_screen_y);
        slider->localPointToScreen(end_x, local_y, &end_screen_x, &end_screen_y);
        mFunctionalClickTarget = slider->getHandle();

        std::string reason;
        mFunctionalScreenX = start_screen_x; mFunctionalScreenY = start_screen_y;
        if (!functionalPreDispatch(slider, reason))
        {
            mFunctionalLastClickReason = reason;
            return false;
        }
        mFunctionalScreenX = end_screen_x; mFunctionalScreenY = end_screen_y;
        if (!functionalPreDispatch(slider, reason))
        {
            mFunctionalLastClickReason = reason;
            return false;
        }

        const LLVector2 display_scale = gViewerWindow->getDisplayScale();
        LLCoordGL start(ll_round((F32)start_screen_x * display_scale.mV[VX]),
                        ll_round((F32)start_screen_y * display_scale.mV[VY]));
        LLCoordGL end(ll_round((F32)end_screen_x * display_scale.mV[VX]),
                      ll_round((F32)end_screen_y * display_scale.mV[VY]));
        gViewerWindow->handleMouseMove(gViewerWindow->getWindow(), start, MASK_NONE);
        LLView* deepest = nullptr;
        struct RecorderGuard
        {
            explicit RecorderGuard(LLView*& deepest)
            {
                LLView::sMouseHandlerRecorder = [&deepest](LLView* view)
                    { if (!deepest) deepest = view; };
                LLView::sDebugMouseHandling = true;
            }
            ~RecorderGuard()
            {
                LLView::sDebugMouseHandling = false;
                LLView::sMouseHandlerRecorder = {};
            }
        } recorder(deepest);
        bool toolmgr = false;
        const bool down = gViewerWindow->handleAnyMouseClick(
            gViewerWindow->getWindow(), start, MASK_NONE, CLICK_LEFT, true, toolmgr);
        const std::string down_path = LLView::sMouseHandlerMessage;
        LLView* down_deepest = deepest;
        const bool captured = gFocusMgr.getMouseCapture() == slider;
        gViewerWindow->handleMouseMove(gViewerWindow->getWindow(), end, MASK_NONE);
        // handleMouseMove only stores the OS position. updateUI is the shipping
        // per-frame path that delivers hover to the current mouse captor.
        gViewerWindow->updateUI();
        deepest = nullptr;
        const bool up = gViewerWindow->handleAnyMouseClick(
            gViewerWindow->getWindow(), end, MASK_NONE, CLICK_LEFT, false, toolmgr);
        const std::string up_path = deepest ? deepest->getPathname() :
            LLView::sMouseHandlerMessage;
        const bool handler = down_deepest == slider ||
            (down_deepest && down_deepest->hasAncestor(slider));
        mFunctionalObserved["click"]["down_handler"] = down_path;
        mFunctionalObserved["click"]["up_handler"] = up_path;
        mFunctionalObserved["click"]["real_drag"] = true;
        mFunctionalObserved["click"]["handler_exact"] = handler;
        mFunctionalObserved["click"]["drag_start_x"] = start_screen_x;
        mFunctionalObserved["click"]["drag_end_x"] = end_screen_x;
        if (LLMouseHandler* stuck = gFocusMgr.getMouseCapture())
        {
            LLView* stuck_view = dynamic_cast<LLView*>(stuck);
            mFunctionalLastClickReason = "capture after slider drag: " +
                (stuck_view ? functionalViewPath(stuck_view) : std::string("<non-view mouse handler>"));
            return false;
        }
        mFunctionalLastClickReason = handler ? std::string() :
            "real slider drag was handled outside target";
        if (!(down && up && captured && handler)) return false;
        S32 nudges = 0;
        while (!functionalEqual(slider->getValue(), target_value, "F32", slider->getIncrement()) &&
               nudges < 512)
        {
            const KEY key = slider->getValue().asReal() < target_value.asReal() ?
                KEY_RIGHT : KEY_LEFT;
            // Deliver straight to the slider (never window key routing): no keystroke can reach
            // the login panel, a menu or the world camera.
            if (!slider->handleKeyHere(key, MASK_NONE)) break;
            ++nudges;
        }
        if (nudges) mFunctionalObserved["click"]["keyboard_nudges"] = nudges;
        return functionalEqual(slider->getValue(), target_value, "F32", slider->getIncrement());
    }

    bool functionalPointerDrag(ALCompassDial* dial, const LLSD& target_value,
                               bool arc)
    {
        if (!dial || !gViewerWindow) return false;
        const LLRect local = dial->getLocalRect();
        const F32 scale = (F32)llmin(local.getWidth(), local.getHeight()) / 48.f;
        const F32 cy = (F32)local.getHeight() * .5f;
        const F32 cx = arc ? 6.f * scale : (F32)local.getWidth() * .5f;
        const F32 radius = arc ? llmax(1.f, llmin(20.f * scale,
            llmin(cy - 3.f * scale, (F32)local.getWidth() - 9.f * scale))) :
            llmax(1.f, llmin(16.f * scale, cy - 7.f * scale));
        auto point = [arc, cx, cy, radius](F32 degrees)
        {
            const F32 angle = degrees * DEG_TO_RAD;
            return LLVector2(cx + (arc ? cosf(angle) : sinf(angle)) * radius,
                             cy + (arc ? sinf(angle) : cosf(angle)) * radius);
        };
        const LLVector2 start_point = point((F32)dial->getValue().asReal());
        const LLVector2 end_point = point((F32)target_value.asReal());
        const S32 start_x = ll_round(start_point.mV[VX]);
        const S32 start_y = ll_round(start_point.mV[VY]);
        const S32 end_x = ll_round(end_point.mV[VX]);
        const S32 end_y = ll_round(end_point.mV[VY]);
        S32 start_screen_x = 0, start_screen_y = 0, end_screen_x = 0, end_screen_y = 0;
        dial->localPointToScreen(start_x, start_y, &start_screen_x, &start_screen_y);
        dial->localPointToScreen(end_x, end_y, &end_screen_x, &end_screen_y);
        mFunctionalClickTarget = dial->getHandle();
        std::string reason;
        mFunctionalScreenX = start_screen_x; mFunctionalScreenY = start_screen_y;
        if (!functionalPreDispatch(dial, reason))
        {
            mFunctionalLastClickReason = reason;
            return false;
        }
        mFunctionalScreenX = end_screen_x; mFunctionalScreenY = end_screen_y;
        if (!functionalPreDispatch(dial, reason))
        {
            mFunctionalLastClickReason = reason;
            return false;
        }

        // The final check before the only down event is for its exact point.
        mFunctionalScreenX = start_screen_x; mFunctionalScreenY = start_screen_y;
        if (!functionalPreDispatch(dial, reason))
        {
            mFunctionalLastClickReason = reason;
            return false;
        }

        const LLVector2 display_scale = gViewerWindow->getDisplayScale();
        LLCoordGL start(ll_round((F32)start_screen_x * display_scale.mV[VX]),
                        ll_round((F32)start_screen_y * display_scale.mV[VY]));
        LLCoordGL end(ll_round((F32)end_screen_x * display_scale.mV[VX]),
                      ll_round((F32)end_screen_y * display_scale.mV[VY]));
        gViewerWindow->handleMouseMove(gViewerWindow->getWindow(), start, MASK_NONE);
        LLView* deepest = nullptr;
        struct RecorderGuard
        {
            explicit RecorderGuard(LLView*& deepest)
            {
                LLView::sMouseHandlerRecorder = [&deepest](LLView* view)
                    { if (!deepest) deepest = view; };
                LLView::sDebugMouseHandling = true;
            }
            ~RecorderGuard()
            {
                LLView::sDebugMouseHandling = false;
                LLView::sMouseHandlerRecorder = {};
            }
        } recorder(deepest);
        bool toolmgr = false;
        const bool down = gViewerWindow->handleAnyMouseClick(
            gViewerWindow->getWindow(), start, MASK_NONE, CLICK_LEFT, true, toolmgr);
        LLView* down_deepest = deepest;
        const bool captured = gFocusMgr.getMouseCapture() == dial;
        gViewerWindow->handleMouseMove(gViewerWindow->getWindow(), end, MASK_NONE);
        gViewerWindow->updateUI();
        deepest = nullptr;
        const bool up = gViewerWindow->handleAnyMouseClick(
            gViewerWindow->getWindow(), end, MASK_NONE, CLICK_LEFT, false, toolmgr);
        const bool handler = down_deepest == dial ||
            (down_deepest && down_deepest->hasAncestor(dial));
        mFunctionalObserved["click"]["real_drag"] = true;
        mFunctionalObserved["click"]["handler_exact"] = handler;
        LLMouseHandler* stuck = gFocusMgr.getMouseCapture();
        if (stuck && stuck == mFunctionalAllowedCaptor)
            stuck = nullptr; // combo button handed capture to its own dropdown list (llcombobox.cpp:818-820)
        if (stuck)
        {
            LLView* stuck_view = dynamic_cast<LLView*>(stuck);
            if (stuck_view && !dynamic_cast<LLModalDialog*>(stuck_view) &&
                functionalRelatedView(stuck_view, dial))
            {
                gFocusMgr.setMouseCapture(nullptr);
                mFunctionalLastClickReason = "stuck target capture: " + functionalViewPath(stuck_view);
            }
            else
                mFunctionalLastClickReason = "foreign capture after dispatch: " +
                    (stuck_view ? functionalViewPath(stuck_view) : std::string("<non-view mouse handler>"));
            return false;
        }
        mFunctionalLastClickReason = handler ? std::string() :
            "real drag was handled outside target";
        if (!(down && up && captured && handler)) return false;
        S32 nudges = 0;
        const LLSD& increment = mFunctionalEntries[mFunctionalIndex]["increment"];
        while (!functionalEqual(dial->getValue(), target_value, "F32", increment) &&
               nudges < 512)
        {
            const KEY key = dial->getValue().asReal() < target_value.asReal() ?
                KEY_RIGHT : KEY_LEFT;
            if (!dial->handleKeyHere(key, MASK_NONE)) break; // direct, never window key routing
            ++nudges;
        }
        if (nudges) mFunctionalObserved["click"]["keyboard_nudges"] = nudges;
        return functionalEqual(dial->getValue(), target_value, "F32", increment);
    }

    bool functionalSetDialByKeyboard(ALCompassDial* dial, const LLSD& target_value)
    {
        if (!dial) return false;
        const LLSD& increment = mFunctionalEntries[mFunctionalIndex]["increment"];
        S32 nudges = 0;
        while (!functionalEqual(dial->getValue(), target_value, "F32", increment) &&
               nudges < 512)
        {
            const KEY key = dial->getValue().asReal() < target_value.asReal() ?
                KEY_RIGHT : KEY_LEFT;
            if (!dial->handleKeyHere(key, MASK_NONE)) break; // direct target delivery only
            ++nudges;
        }
        mFunctionalObserved["click"]["direct_dial_keys"] = nudges;
        return nudges > 0 &&
            functionalEqual(dial->getValue(), target_value, "F32", increment);
    }

    bool functionalExerciseComboByIndex(LLComboBox* combo, const LLSD& test)
    {
        LLScrollListCtrl* list = FunctionalComboAccess::list(combo);
        const bool owns_list = list && !list->getVisible();
        if (owns_list) functionalTrackCombo(combo);
        if (!functionalPointerClick()) return false;
        mFunctionalObserved["combo"]["open_click"] = mFunctionalObserved["click"];
        S32 index = -1;
        if (test.isMap() && test.has("item_index"))
            index = test["item_index"].asInteger();
        else if (test.isMap() && (test.has("value") || test.has("id")))
        {
            const LLSD wanted = test.has("value") ? test["value"] : test["id"];
            for (S32 i = 0; list && i < list->getItemCount(); ++i)
                if (LLScrollListItem* item = list->getItemByIndex(i);
                    item && item->getEnabled() && item->getValue() == wanted)
                {
                    index = i;
                    break;
                }
        }
        else
            index = 1;
        if (!list || !list->getVisible() || list->getItemCount() <= 0) return false;
        const bool runtime_item = test.isMap() && test["resolve"].asString() == "runtime";
        if (runtime_item && (index < 0 || index >= list->getItemCount() ||
            !list->getItemByIndex(index) || !list->getItemByIndex(index)->getEnabled() ||
            list->getItemByIndex(index)->getValue() == combo->getValue()))
        {
            index = -1;
            for (S32 candidate = 0; candidate < list->getItemCount(); ++candidate)
            {
                LLScrollListItem* item = list->getItemByIndex(candidate);
                if (item && item->getEnabled() && item->getValue() != combo->getValue())
                {
                    index = candidate;
                    break;
                }
            }
        }
        if (index < 0 || index >= list->getItemCount() ||
            !list->getItemByIndex(index) || !list->getItemByIndex(index)->getEnabled())
        {
            mFunctionalLastClickReason = "not reachable: combo item missing or disabled";
            return false;
        }
        // Runtime-populated combos do not necessarily put their current item
        // at zero.  A real click on the already-selected item can commit yet
        // exercise no state; select a reachable alternate when possible.
        if (runtime_item || !mDirectorV2)
        {
            if (LLScrollListItem* selected = list->getItemByIndex(index);
                selected && selected->getValue() == combo->getValue())
            {
                for (S32 candidate = 0; candidate < list->getItemCount(); ++candidate)
                {
                    LLScrollListItem* item = list->getItemByIndex(candidate);
                    if (item && item->getEnabled() && item->getValue() != combo->getValue())
                    {
                        index = candidate;
                        break;
                    }
                }
            }
        }
        index = llclamp(index, 0, list->getItemCount() - 1);
        const S32 page_lines = llmax(1, list->getLinesPerPage());
        const S32 wanted_scroll = index < list->getScrollPos() ? index :
            (index >= list->getScrollPos() + page_lines ? index - page_lines + 1 :
             list->getScrollPos());
        // showList() can clamp/reshape upward and clamp the scrollbar's doc-pos
        // without calling onScrollChange(), leaving mScrollLines stale.  Always
        // round-trip the position so getCellRect() uses the popup's live layout.
        list->setScrollPos(wanted_scroll);
        const LLRect cell = list->getCellRect(index, 0);
        // getCellRect's width comes from the column, which is unsized (-2) for combo lists
        // (llscrolllistctrl.cpp:1887): click the middle of the item area instead of its edge.
        list->localPointToScreen(list->getItemListRect().getCenterX(), cell.getCenterY(),
                                 &mFunctionalScreenX, &mFunctionalScreenY);
        mFunctionalClickTarget = list->getHandle();
        mFunctionalObserved["click"]["combo_index"] = index;
        mFunctionalObserved["click"]["cell_rect_local"] = functionalRect(cell);
        mFunctionalObserved["click"]["item_point"] = LLSD::emptyArray();
        mFunctionalObserved["click"]["item_point"].append(mFunctionalScreenX);
        mFunctionalObserved["click"]["item_point"].append(mFunctionalScreenY);
        const LLRect list_rect = list->calcScreenRect();
        const bool item_visible = index >= list->getScrollPos() &&
            index < list->getScrollPos() + page_lines &&
            list_rect.pointInRect(mFunctionalScreenX, mFunctionalScreenY);
        mFunctionalObserved["click"]["list_rect"] = functionalRect(list_rect);
        mFunctionalObserved["click"]["list_item_visible"] = item_visible;
        if (!item_visible)
        {
            mFunctionalLastClickReason = "not reachable: combo item outside list scroll window";
            return false;
        }
        return functionalPointerClick();
    }

    bool functionalSelectComboValue(LLComboBox* combo, const LLSD& value)
    {
        LLScrollListCtrl* list = combo ? FunctionalComboAccess::list(combo) : nullptr;
        const bool owns_list = list && !list->getVisible();
        if (owns_list) functionalTrackCombo(combo);
        mFunctionalAllowedCaptor = list;
        const bool ok = functionalSelectComboValueImpl(combo, value);
        mFunctionalAllowedCaptor = nullptr;
        if (owns_list && list && gFocusMgr.getMouseCapture() == list)
            gFocusMgr.setMouseCapture(nullptr);
        if (owns_list && combo && list && list->getVisible())
            combo->hideList();
        return ok;
    }

    bool functionalSelectComboValueImpl(LLComboBox* combo, const LLSD& value)
    {
        std::string reason;
        if (!combo || !functionalReachable(combo, reason) || !functionalPointerClick()) return false;
        mFunctionalObserved["combo"]["open_click"] = mFunctionalObserved["click"];
        LLScrollListCtrl* list = FunctionalComboAccess::list(combo);
        if (!list || !list->getVisible()) return false;
        S32 index = -1;
        for (S32 i = 0; i < list->getItemCount(); ++i)
            // XUI stores combo item values as strings even when the bound
            // control and generated reveal manifest are integer typed.
            // Match their serialized value, then exercise the actual popup
            // item through the normal pointer path below.
            if (LLScrollListItem* item = list->getItemByIndex(i);
                item && item->getValue().asString() == value.asString())
            { index = i; break; }
        if (index < 0) return false;
        const S32 page_lines = llmax(1, list->getLinesPerPage());
        const S32 wanted_scroll = index < list->getScrollPos() ? index :
            (index >= list->getScrollPos() + page_lines ? index - page_lines + 1 :
             list->getScrollPos());
        list->setScrollPos(wanted_scroll);
        const LLRect cell = list->getCellRect(index, 0);
        // getCellRect's width comes from the column, which is unsized (-2) for combo lists
        // (llscrolllistctrl.cpp:1887): click the middle of the item area instead of its edge.
        list->localPointToScreen(list->getItemListRect().getCenterX(), cell.getCenterY(),
                                 &mFunctionalScreenX, &mFunctionalScreenY);
        mFunctionalClickTarget = list->getHandle();
        const LLRect list_rect = list->calcScreenRect();
        const bool item_visible = index >= list->getScrollPos() &&
            index < list->getScrollPos() + page_lines &&
            list_rect.pointInRect(mFunctionalScreenX, mFunctionalScreenY);
        mFunctionalObserved["click"]["list_rect"] = functionalRect(list_rect);
        mFunctionalObserved["click"]["list_item_visible"] = item_visible;
        if (!item_visible)
        {
            mFunctionalLastClickReason = "not reachable: combo item outside list scroll window";
            return false;
        }
        return functionalPointerClick();
    }

    bool probeFunctionalEnable(LLUICtrl* control)
    {
        if (!control || control->isInEnabledChain()) return true;
        LLView* scope = control->getParent();
        std::vector<LLCheckBoxCtrl*> candidates;
        std::function<void(LLView*)> collect = [&](LLView* view)
        {
            if (candidates.size() >= 3) return;
            if (LLCheckBoxCtrl* box = dynamic_cast<LLCheckBoxCtrl*>(view);
                box && box != control && box->isInVisibleChain() && box->isInEnabledChain())
                candidates.push_back(box);
            for (LLView* child : *view->getChildList()) collect(child);
        };
        if (scope) collect(scope);
        for (LLCheckBoxCtrl* candidate : candidates)
        {
            const LLSD* manifest = nullptr;
            for (const LLSD& row : mFunctionalEntries)
            {
                if (row["page"].asString() ==
                        mFunctionalEntries[mFunctionalIndex]["page"].asString() &&
                    row["widget_name"].asString() == candidate->getName())
                {
                    manifest = &row;
                    break;
                }
            }
            const bool pure_setting = manifest &&
                (*manifest)["action"].asString() == "RUN" &&
                (*manifest)["hazards"].size() == 0 &&
                !(*manifest)["expect"]["setting"].asString().empty() &&
                (*manifest)["effect_kind"].asString() != "side_effect_only";
            if (!pure_setting) continue;
            mFunctionalRevealWidgetValues.emplace_back(candidate->getDerivedHandle<LLUICtrl>(),
                                                       candidate->getValue());
            std::string reason;
            const bool clicked = functionalReachable(candidate, reason) && functionalPointerClick();
            LLSD probe = LLSD::emptyMap();
            probe["widget"] = candidate->getPathname();
            probe["clicked"] = clicked;
            probe["enabled"] = control->isInEnabledChain();
            mFunctionalObserved["enable_probe"].append(probe);
            if (control->isInEnabledChain()) return true;
        }
        return control->isInEnabledChain();
    }

    bool functionalExerciseListByIndex(LLScrollListCtrl* list, const LLSD& test)
    {
        if (!list || list->getItemCount() < 1) return false;
        S32 index = test.isMap() && test.has("item_index")
            ? test["item_index"].asInteger() : 1;
        if (list->getName() == "prop_list" && !mFunctionalPropFixtures.empty())
        {
            const LLUUID& wanted = mFunctionalPropFixtures.back();
            for (S32 candidate = 0; candidate < list->getItemCount(); ++candidate)
                if (LLScrollListItem* fixture = list->getItemByIndex(candidate);
                    fixture && fixture->getValue().asUUID() == wanted)
                {
                    index = candidate;
                    break;
                }
        }
        else if (list->getName() == "db_playlist_list")
        {
            index = list->getItemCount() - 1;
        }
        index = llclamp(index, 0, list->getItemCount() - 1);
        if (LLScrollListItem* selected = list->getFirstSelected();
            selected && list->getItemIndex(selected) == index && list->getItemCount() > 1)
            index = (index + 1) % list->getItemCount();
        LLScrollListItem* item = list->getItemByIndex(index);
        if (!item || !item->getEnabled()) return false;
        mFunctionalListExpected = item->getValue();
        const S32 page_lines = llmax(1, list->getLinesPerPage());
        list->setScrollPos(index >= page_lines ? index - page_lines + 1 : 0);
        const LLRect cell = list->getCellRect(index, 0);
        list->localPointToScreen(cell.getCenterX(), cell.getCenterY(),
                                 &mFunctionalScreenX, &mFunctionalScreenY);
        mFunctionalClickTarget = list->getHandle();
        return functionalPointerClick();
    }

    // ui-draw-lint: allow-function(setting-write,raw-write) isolated test-only input exercise
    bool exerciseFunctional(const LLSD& entry, LLUICtrl* control)
    {
        const std::string exercise = entry["exercise"].asString();
        const LLSD& test = entry["test_value"];
        if (entry["probe"].asString() == "vc_face_list")
            return functionalPointerClick();
        if (entry["probe"].asString() == "vcam_test_pattern")
        {
            LLView* item = control ? control->findChild<LLView>(
                "vc_source_type_item_3", true) : nullptr;
            if (!item) return false;
            const LLRect rect = item->getLocalRect();
            item->localPointToScreen(rect.getCenterX(), rect.getCenterY(),
                                     &mFunctionalScreenX, &mFunctionalScreenY);
            mFunctionalClickTarget = item->getHandle();
            return functionalPointerClick();
        }
        if (exercise == "toggle" || exercise == "click")
        {
            if (!functionalPointerClick()) return false;
            const std::string confirm = entry["confirm_notification"].asString();
            if (!confirm.empty())
            {
                if (mFunctionalObserved["click"]["notification_answered"].asBoolean())
                    return true;
                bool responded = false;
                if (LLNotificationChannelPtr channel =
                        LLNotifications::instance().getChannel("Visible"))
                {
                    channel->forEachNotification(
                        [&responded, &confirm](LLNotificationPtr note)
                        {
                            if (!responded && note && note->getName() == confirm)
                            {
                                note->respondWithDefault();
                                responded = true;
                            }
                        });
                }
                return responded;
            }
            return true;
        }
        if (exercise == "select_item")
        {
            if (LLComboBox* combo = dynamic_cast<LLComboBox*>(control))
            {
                LLScrollListCtrl* combo_list = FunctionalComboAccess::list(combo);
                const bool owns_list = combo_list && !combo_list->getVisible();
                if (owns_list) functionalTrackCombo(combo);
                mFunctionalAllowedCaptor = combo_list;
                const bool ok = functionalExerciseComboByIndex(combo, test);
                mFunctionalAllowedCaptor = nullptr;
                if (owns_list && combo_list && gFocusMgr.getMouseCapture() == combo_list)
                    gFocusMgr.setMouseCapture(nullptr);
                if (owns_list && combo_list && combo_list->getVisible())
                    combo->hideList();
                return ok;
            }
            if (LLRadioGroup* radio = dynamic_cast<LLRadioGroup*>(control))
            {
                return functionalPointerClick();
            }
            if (LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(control))
            {
                return functionalExerciseListByIndex(list, test);
            }
        }
        if (LLColorSwatchCtrl* swatch = dynamic_cast<LLColorSwatchCtrl*>(control))
        {
            if (!functionalPointerClick()) return false;
            // Mirror LLColorSwatchCtrl::onColorChanged(): update the open picker
            // and write the bound control through the swatch, as a picker event
            // does, instead of synthesizing a bare widget commit.
            swatch->set(LLColor4(test), true, false);
            swatch->onCommit();
            mFunctionalObserved["click"]["programmatic_picker_commit"] = true;
            mFunctionalObserved["effect"]["programmatic"] = true;
            return true;
        }
        if (ALCompassDial* dial = dynamic_cast<ALCompassDial*>(control))
            return functionalSetDialByKeyboard(dial, test);
        if (dynamic_cast<LLLineEditor*>(control) || dynamic_cast<LLTextEditor*>(control))
        {
            if (!functionalPointerClick()) return false;
            auto focus_exact = [this, control]()
            {
                const bool exact = gFocusMgr.getKeyboardFocus() == control;
                mFunctionalObserved["keyboard"]["focus_exact"] = exact;
                if (!exact) mFunctionalLastClickReason = "focus not on target";
                return exact;
            };
            if (!focus_exact() || !gViewerWindow->handleKey('A', MASK_CONTROL)) return false;
            const std::string typed = test.asString();
            for (unsigned char ch : typed)
            {
                if (!focus_exact() || !gViewerWindow->handleUnicodeChar(ch, MASK_NONE)) return false;
            }
            // Re-check immediately before Return; no frame or callback may
            // redirect it to the login form or an alert.
            if (!focus_exact()) return false;
            return gViewerWindow->handleKey(KEY_RETURN, MASK_NONE);
        }
        if (LLSliderCtrl* slider_ctrl = dynamic_cast<LLSliderCtrl*>(control))
        {
            LLSlider* slider = findFirstView<LLSlider>(slider_ctrl);
            return functionalPointerDrag(slider, test);
        }
        if (LLSlider* slider = dynamic_cast<LLSlider*>(control))
            return functionalPointerDrag(slider, test);
        if (dynamic_cast<LLSpinCtrl*>(control) && entry["min"].isDefined() &&
            entry["max"].isDefined())
            return functionalPointerClick();
        const bool has_programmatic_oracle = !mFunctionalSettingName.empty() ||
            !entry["consumer"].asString().empty() || !mFunctionalHandlerName.empty();
        if (!has_programmatic_oracle)
        {
            mFunctionalLastClickReason = "programmatic fallback has no external oracle";
            return false;
        }
        control->setValue(test);
        control->onCommit();
        mFunctionalObserved["effect"]["programmatic_commit_path"] = "generic fallback";
        mFunctionalObserved["effect"]["programmatic"] = true;
        return true;
    }

    // ui-draw-lint: allow-function(lookup) isolated test-only selector alternate lookup
    bool prepareFunctionalAlternate(const LLSD& entry)
    {
        if (entry["probe"].asString() != "selector_segment") return true;
        const S32 target = entry["test_value"].asInteger();
        const std::string base = entry["widget_name"].asString().substr(
            0, entry["widget_name"].asString().find(':'));
        LLUICtrl* other = mFunctionalPageRoot->findChild<LLUICtrl>(
            base + "_seg_" + llformat("%d", target == 0 ? 1 : 0), true); // ui-draw-lint: allow(lookup) test-only selector precondition
        if (!other) return false;
        LLUICtrl* target_control = mFunctionalControl.get();
        mFunctionalControl = other->getDerivedHandle<LLUICtrl>();
        std::string reason;
        const bool reachable = functionalReachable(other, reason);
        const bool clicked = reachable && functionalPointerClick();
        mFunctionalControl = target_control->getDerivedHandle<LLUICtrl>();
        return clicked;
    }

    std::string functionalOracleSetting(const LLSD& entry, LLUICtrl* control)
    {
        std::string setting = entry["expect"]["setting"].asString();
        if (!setting.empty()) return setting;
        const std::string widget = entry["widget_name"].asString();
        if (widget.compare(0, 13, "cine_fixture_") != 0 || !mFunctionalPageRoot)
            return {};
        LLComboBox* role = mFunctionalPageRoot->findChild<LLComboBox>("cine_fixture_role", true);
        static const char* const roles[] = { "Key", "Fill", "Rim", "Bg" };
        const S32 index = role ? llclamp(role->getValue().asInteger(), 0, 3) : 0;
        std::string suffix;
        if (widget == "cine_fixture_mode") suffix = "FixtureMode";
        else if (widget == "cine_fixture_preset") suffix = "FixturePreset";
        else if (widget == "cine_fixture_kelvin") suffix = "Kelvin";
        else if (widget == "cine_fixture_source_size") suffix = "SourceSizeM";
        else if (widget.compare(0, 17, "cine_fixture_gel_") == 0)
            suffix = "GelSlot" + widget.substr(17);
        if (suffix.empty()) return {};
        mFunctionalDynamicSetting = true;
        return std::string("CineLightRig") + roles[index] + suffix;
    }

    // ui-draw-lint: allow-function(setting,lookup) isolated test-only gate assertion
    bool prepareFunctionalRimGate(const LLSD& entry)
    {
        const bool rim_gate = entry["consumer"].asString() == "rim_render_active";
        const bool weather_gate = mDirectorV2 &&
            entry["expect"]["setting"].asString() == "AlchemyWeatherEnabled";
        if (!rim_gate && !weather_gate) return true;
        if (!mFunctionalGatePrepared)
        {
            prepareFunctionalRestore(entry);
            auto force = [this](const std::string& name, const LLSD& value)
            {
                LLControlVariable* setting = gSavedSettings.getControl(name).get();
                if (!setting) return;
                if (!mFunctionalRestore.count(name))
                    mFunctionalRestore[name] = { setting->get(), setting->getSaveValue(),
                                                 setting->hasUnsavedValue() };
                setting->setValue(value, false); // ui-draw-lint: allow(setting-write) isolated Rim gate precondition
            };
            if (rim_gate)
            {
                if (!mFunctionalRigRowSnapshot.isMap())
                    mFunctionalRigRowSnapshot = ALCineLightRigManager::instance().sceneData();
                LLSD disabled = mFunctionalRigRowSnapshot;
                LLSD& instances = disabled["instances"];
                if (instances.isArray())
                    for (LLSD::array_iterator it = instances.beginArray();
                         it != instances.endArray(); ++it)
                        (*it)["CineLightRigEnabled"] = false;
                ALCineLightRigManager::instance().applySceneData(disabled);
                force("CineRigRimEnabled", true);
                force("CineRigRimMasterGain", 1.f);
                functionalTickRig();
                ALCineRigRim::isEnabled();
            }
            else
                force("AlchemyWeatherEnabled", false);
            mFunctionalPageRoot = functionalPage(entry);
            mFunctionalGatePrepared = true;
            mFunctionalGateFrames = 0;
            return false; // allow a normal frame to update the page banner
        }
        ++mFunctionalGateFrames;
        ALKitPage* page = findFirstView<ALKitPage>(mFunctionalPageRoot);
        const std::string prefix = rim_gate ? "light_rim" : "weather";
        LLPanel* banner = page ? page->findChild<LLPanel>(prefix + "_gate_banner", true) : nullptr;
        LLButton* turn_on = page ? page->findChild<LLButton>(prefix + "_gate_on", true) : nullptr;
        const bool predicate_off = rim_gate ? !ALCineRigRim::isEnabled() :
                                             !gSavedSettings.getBOOL("AlchemyWeatherEnabled");
        mFunctionalObserved["gate"]["predicate_off"] = predicate_off;
        mFunctionalObserved["gate"]["banner_exists"] = banner != nullptr;
        mFunctionalObserved["gate"]["banner_visible"] = banner && banner->getVisible();
        mFunctionalObserved["gate"]["button_exists"] = turn_on != nullptr;
        mFunctionalObserved["gate"]["settle_frames"] = mFunctionalGateFrames;
        if (mDirectorV2 && predicate_off && banner && !banner->getVisible() &&
            mFunctionalGateFrames < 6) return false;
        std::string gate_failure;
        if (!predicate_off) gate_failure = prefix + " gate predicate was not forced off";
        else if (mDirectorV2 && !banner) gate_failure = prefix + " gate banner is missing";
        else if (mDirectorV2 && !banner->getVisible()) gate_failure = prefix + " gate banner did not draw within 6 frames";
        else if (mDirectorV2 && !turn_on) gate_failure = prefix + " gate Turn on button is missing";
        if (!gate_failure.empty())
        {
            addFunctionalResult(entry, "FAIL", gate_failure);
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return false;
        }
        LLUICtrl* enable_control = turn_on;
        if (rim_gate && !mDirectorV2 && mFloater.get())
            enable_control = mFloater.get()->findChild<LLUICtrl>("cine_enabled", true);
        if (!enable_control)
        {
            addFunctionalResult(entry, "FAIL", prefix + " gate has no user-clickable master control");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return false;
        }
        revealFunctionalControl(enable_control);
        LLUICtrl* tested_control = mFunctionalControl.get();
        mFunctionalControl = enable_control->getDerivedHandle<LLUICtrl>();
        std::string reason;
        const bool gate_clicked = functionalReachable(enable_control, reason) && functionalPointerClick();
        mFunctionalControl = tested_control ? tested_control->getDerivedHandle<LLUICtrl>() : LLHandle<LLUICtrl>();
        if (!gate_clicked)
        {
            addFunctionalResult(entry, "FAIL", prefix + " gate Turn on is not user-clickable: " +
                (mFunctionalLastClickReason.empty() ? reason : mFunctionalLastClickReason));
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return false;
        }
        if (rim_gate) functionalTickRig();
        const bool enabled = rim_gate ?
            (gSavedSettings.getBOOL("CineLightRigEnabled") && ALCineRigRim::isEnabled()) :
            gSavedSettings.getBOOL("AlchemyWeatherEnabled");
        if (!enabled)
        {
            addFunctionalResult(entry, "FAIL",
                prefix + " gate Turn on did not enable its master/render predicate");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return false;
        }
        return true;
    }

    // ui-draw-lint: allow-function(lookup,raw-write,setting-write) one-shot test-only post-reveal exercise setup
    void continueFunctionalEntry(const LLSD& entry, LLUICtrl* control)
    {
        if (entry["probe"].asString() == "exposure_history")
        {
            rememberFunctionalSetting("RenderExposure");
            if (LLControlVariable* exposure = gSavedSettings.getControl("RenderExposure").get()) // ui-draw-lint: allow(setting) test-only per-row oracle setup
            {
                const F32 original = (F32)exposure->getValue().asReal();
                {
                    ALExposureSettingsHistory::ScopedBatch batch("Functional harness witness");
                    exposure->setValue(original + .125f, false); // ui-draw-lint: allow(setting-write) bounded history oracle
                }
                if (entry["widget_name"].asString() == "exp_redo")
                    ALExposureSettingsHistory::undo();
            }
        }
        if (entry["probe"].asString() == "path_owner")
        {
            const std::string widget = entry["widget_name"].asString();
            const bool needs_node = widget == "btn_wp_insert" ||
                widget.compare(0, 5, "node_") == 0 ||
                widget == "btn_node_setcam" || widget == "btn_node_preview";
            if (needs_node)
            {
                LLScrollListCtrl* waypoints = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLScrollListCtrl>(
                        "waypoint_list", true) : nullptr;
                if (waypoints && waypoints->getItemCount() > 1)
                {
                    // Production selection path: the panel callback owns both
                    // its list selection and LLActorMover's edit-node state.
                    waypoints->selectNthItem(1);
                    waypoints->onCommit();
                }
            }
        }
        std::string reachability;
        if (!control->isInEnabledChain()) probeFunctionalEnable(control);
        if (!functionalReachable(control, reachability))
        {
            std::string world_reason;
            if (reachability == "not reachable: disabled-by-parent")
            {
                const std::string page = entry["page"].asString();
                if (page == "gaze_tab") world_reason = "requires a logged-in edited actor";
                else if (page == "ghosts_tab") world_reason = "requires a populated ghost/crowd selection";
                else if (page == "props_tab") world_reason = "requires a selected prop or path entity";
                else if (page == "actor_style_tab") world_reason = "requires a selected actor";
                else if (page == "practicals_tab") world_reason = "requires a selected in-world light";
                else if (page == "animate_tab") world_reason = "requires a logged-in avatar and animation state";
                else if (page == "move_tab") world_reason = "requires a selected actor group";
                else if (page == "takes_tab") world_reason = "requires a loaded or running take";
                else if (page == "shot_tab") world_reason = "requires a populated cast selection";
                else if (page == "cine_light_rig_tab" &&
                         entry["widget_name"].asString().find("actor_only") != std::string::npos)
                    world_reason = "requires an actor target for the selected rig light";
            }
            addFunctionalResult(entry, world_reason.empty() ? "FAIL" : "SKIP",
                world_reason.empty() ? reachability : world_reason);
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        mFunctionalControl = control->getDerivedHandle<LLUICtrl>();
        mFunctionalOriginalControlValue = control->getValue();
        mFunctionalExercise = entry["exercise"].asString();
        if (mFunctionalDryReveal)
        {
            addFunctionalResult(entry, "PASS", "dry reveal: visible, enabled, on-screen click target resolved");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        if (!prepareFunctionalAlternate(entry))
        {
            addFunctionalResult(entry, "FAIL", "selector alternate state was not reachable");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        if (!functionalReachable(control, reachability))
        {
            addFunctionalResult(entry, "FAIL", reachability);
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        const std::string gaze_field = entry["gaze_oracle_field"].asString();
        if (mInWorld && !gaze_field.empty() && gAgentAvatarp)
        {
            mFunctionalGazeActor = gAgentAvatarp->getID();
            mFunctionalGazeBefore =
                LLActorMover::instance().getGazeTargetConfig(mFunctionalGazeActor);
            mFunctionalHaveGazeBefore = true;
            LLActorMover::GazeTarget target = mFunctionalGazeBefore;
            functionalSetGazeField(target, gaze_field);
            LLActorMover::instance().setGazeTargetConfig(mFunctionalGazeActor, target);
        }
        const std::string reset_widget = entry["reset_widget"].asString();
        if (!reset_widget.empty())
        {
            LLUICtrl* target = mFunctionalPageRoot->findChild<LLUICtrl>(reset_widget, true);
            if (!target)
            {
                addFunctionalResult(entry, "FAIL", "reset target control not found");
                restoreFunctionalAction();
                ++mFunctionalIndex;
                return;
            }
            mFunctionalSecondaryControl = target->getDerivedHandle<LLUICtrl>();
            mFunctionalSecondaryBefore = target->getValue();
            target->setValue(entry["reset_pre_value"]);
            target->onCommit();
        }
        const std::string probe = entry["probe"].asString();
        if (entry["consumer"].asString().compare(0, 18, "localfog_selected_") == 0)
        {
            mFunctionalLocalFogBefore = ALLocalFogManager::instance().volumes();
            mFunctionalLocalFogSelectedBefore =
                ALLocalFogManager::instance().selectedVolume();
            mFunctionalHaveLocalFogBefore = true;
        }
        if (mInWorld && probe == "cine_fx_stop")
        {
            ALCineLightRigManager::instance().selected().startFX(0, 0.0);
        }
        else if (mInWorld && probe == "cine_live_probe_refresh")
        {
            mFunctionalProbeStatusBefore =
                gPipeline.mReflectionMapManager.getCinematicRefreshStatus();
        }
        else if (mInWorld && probe == "vc_snap_view" && mFunctionalVCamModel)
        {
            ALVCamPageModel::currentViewTransform(mFunctionalVCamExpectedPos,
                                                  mFunctionalVCamExpectedRot);
            const LLPrismLens::CaptureHandle capture =
                mFunctionalVCamModel->selectedCapture();
            const LLPrismLens::CaptureDefinition* definition = nullptr;
            for (const LLPrismLens::CaptureDefinition& candidate :
                 mFunctionalVCamModel->registry().mCaptures)
                if (candidate.mHandle.mId == capture.mId) { definition = &candidate; break; }
            if (definition)
            {
                LLPrismLens::CameraSettings camera = definition->mCamera;
                camera.mVirtualPos = mFunctionalVCamExpectedPos + LLVector3(3.f, 0.f, 0.f);
                std::string reason;
                mFunctionalVCamModel->setCamera(capture, camera, &reason);
                mFunctionalVCamModel->refreshNow();
            }
        }
        else if (mInWorld && probe == "actor_mover_active" && gAgentAvatarp)
        {
            LLActorMover& mover = LLActorMover::instance();
            mFunctionalActorMoverId = gAgentAvatarp->getID();
            mFunctionalActorPathBefore = mover.pathSceneData(mFunctionalActorMoverId);
            mFunctionalActorPathWalkingBefore = mover.isPathWalking(mFunctionalActorMoverId);
            mFunctionalHaveActorPathBefore = true;
            mover.stop(mFunctionalActorMoverId);
            mover.clearPath(mFunctionalActorMoverId);
            const LLVector3d origin = gAgent.getPositionGlobal();
            mover.appendWaypointAt(mFunctionalActorMoverId, origin);
            mover.appendWaypointAt(mFunctionalActorMoverId,
                                   origin + LLVector3d(2.0, 0.0, 0.0));
            rememberFunctionalSetting("ActorMoverSync");
            gSavedSettings.setBOOL("ActorMoverSync", true);
            if (!entry["actor_mover_expect_active"].asBoolean())
                mover.start(mFunctionalActorMoverId);
        }
        else if (mInWorld && probe == "actor_style_reset")
        {
            mFunctionalActorStyleBefore =
                LLDirectorCast::instance().getActorStyle(LLUUID::null);
            mFunctionalHaveActorStyleBefore = true;
            LLDirectorCast::ActorStyle changed = mFunctionalActorStyleBefore;
            changed.mEnabled = true;
            changed.mAlpha = .25f;
            changed.mBrightness = 2.f;
            LLDirectorCast::instance().setActorStyle(LLUUID::null, changed);
        }
        if (entry["reset_prestate"].isMap())
        {
            for (LLSD::map_const_iterator it = entry["reset_prestate"].beginMap();
                 it != entry["reset_prestate"].endMap(); ++it)
            {
                rememberFunctionalSetting(it->first);
                if (LLControlVariable* setting = gSavedSettings.getControl(it->first).get())
                    setting->setValue(it->second, false); // ui-draw-lint: allow(setting-write) reset-all pre-state
            }
        }
        if (entry["oracle_pre_value"].isDefined())
        {
            const std::string oracle_setting = entry["expect"]["setting"].asString();
            rememberFunctionalSetting(oracle_setting);
            if (LLControlVariable* setting = gSavedSettings.getControl(oracle_setting).get())
                setting->setValue(entry["oracle_pre_value"], false); // ui-draw-lint: allow(setting-write) bounded button oracle
        }
        if (entry["consumer"].asString() == "selected_rig_light_cone_width" &&
            functionalEqual(control->getValue(), entry["test_value"], "S32"))
        {
            control->setValue(entry["test_value"].asInteger() == 0 ? 1 : 0);
            control->onCommit();
        }
        // The pre-state must hold for the bound SETTING as well as the widget: a prerequisite
        // may already have written the setting while the widget hasn't resynced yet.
        bool toggle_setting_at_target = false;
        LLControlVariable* toggle_setting = nullptr;
        if (mDirectorV2 && entry["exercise"].asString() == "toggle" &&
            entry["expect"]["setting"].isString())
            if (LLControlVariable* bound = gSavedSettings.getControl(entry["expect"]["setting"].asString()).get()) // ui-draw-lint: allow(setting) test-only toggle pre-state
            {
                toggle_setting = bound;
                toggle_setting_at_target = bound->getValue().asBoolean() == entry["test_value"].asBoolean();
            }
        if (entry["exercise"].asString() == "toggle" &&
            (control->getValue().asBoolean() == entry["test_value"].asBoolean() || toggle_setting_at_target))
        {
            const bool pre_value = !entry["test_value"].asBoolean();
            if (toggle_setting)
                toggle_setting->setValue(pre_value, false); // ui-draw-lint: allow(setting-write) bounded toggle pre-state
            else
            {
                control->setValue(pre_value);
                control->onCommit();
            }
            mFunctionalObserved["prestate"]["programmatic"] = true;
            mFunctionalObserved["prestate"]["value"] = control->getValue();
            const bool setting_ready = !toggle_setting ||
                toggle_setting->getValue().asBoolean() == pre_value;
            if (control->getValue().asBoolean() == entry["test_value"].asBoolean() ||
                !setting_ready)
            {
                addFunctionalResult(entry, "FAIL", "toggle pre-state could not be established");
                restoreFunctionalAction();
                ++mFunctionalIndex;
                return;
            }
            if (!functionalReachable(control, reachability))
            {
                addFunctionalResult(entry, "FAIL", reachability);
                restoreFunctionalAction();
                ++mFunctionalIndex;
                return;
            }
        }
        if (LLComboBox* combo = dynamic_cast<LLComboBox*>(control);
            entry["probe"].asString() == "rig_projector_binding" && combo)
        {
            const std::vector<LLScrollListItem*> items = combo->getAllData();
            mFunctionalRigProjectorCapture = items.size() > 1 && items[1]
                ? items[1]->getValue().asUUID() : LLUUID::null;
            mFunctionalRigProjectorAbsentBefore = mFunctionalRigProjectorCapture.notNull();
            const U8 rig_slot = static_cast<U8>(
                ALCineLightRigManager::instance().selectedSlot());
            const LLPrismLens::RegistrySnapshot registry = LLPrismLens::registrySnapshot();
            for (U32 index = 0; index < registry.mProjectorCount; ++index)
                if (LLPrismLens::sameRigProjectorIdentity(
                        registry.mProjectors[index], rig_slot, 0))
                {
                    mFunctionalRigProjectorAbsentBefore = false;
                    break;
                }
            mFunctionalObserved["prestate"]["rig_projector_absent"] =
                mFunctionalRigProjectorAbsentBefore;
            mFunctionalObserved["prestate"]["rig_projector_capture"] =
                mFunctionalRigProjectorCapture;
        }
        if (LLComboBox* combo = dynamic_cast<LLComboBox*>(control);
            mDirectorV2 && combo && entry["exercise"].asString() == "select_item" &&
            entry["test_value"].isMap() &&
            entry["test_value"]["resolve"].asString() != "runtime" &&
            entry["test_value"].has("item_index"))
        {
            const S32 target_index = entry["test_value"]["item_index"].asInteger();
            if (combo->getCurrentIndex() == target_index)
            {
                for (LLScrollListItem* item : combo->getAllData())
                {
                    if (!item || !item->getEnabled() || item->getValue() == combo->getValue())
                        continue;
                    combo->setSelectedByValue(item->getValue(), true);
                    combo->onCommit();
                    mFunctionalObserved["prestate"]["combo_index"] = combo->getCurrentIndex();
                    break;
                }
            }
        }
        if (LLSliderCtrl* slider_ctrl = dynamic_cast<LLSliderCtrl*>(control);
            mDirectorV2 && slider_ctrl && entry["exercise"].asString() == "set_value" &&
            is_approx_equal((F32)slider_ctrl->getValue().asReal(),
                            (F32)entry["test_value"].asReal()))
        {
            const F64 target = entry["test_value"].asReal();
            const F64 step = entry["increment"].isDefined() ?
                llabs(entry["increment"].asReal()) : 0.01;
            const F64 alternate = target - step >= entry["min"].asReal() ?
                target - step : llmin(entry["max"].asReal(), target + step);
            slider_ctrl->setValue((F32)alternate);
            slider_ctrl->onCommit();
            mFunctionalObserved["prestate"]["slider"] = slider_ctrl->getValue();
        }
        if (ALCompassDial* dial = dynamic_cast<ALCompassDial*>(control);
            dial && entry["increment"].isDefined())
        {
            const F64 target = entry["test_value"].asReal();
            const F64 step = llabs(entry["increment"].asReal());
            const F64 minimum = entry["min"].asReal();
            const F64 maximum = entry["max"].asReal();
            const F64 before = target - step >= minimum ? target - step :
                llmin(maximum, target + step);
            dial->setValue(before);
            dial->onCommit();
            mFunctionalObserved["prestate"]["dial"] = dial->getValue();
        }
        const std::string launcher_floater = entry["launcher_floater"].asString();
        if (!launcher_floater.empty())
        {
            if (LLFloater* existing = LLFloaterReg::findInstance(launcher_floater))
            {
                mFunctionalLauncherFloater = existing->getHandle();
                mFunctionalLauncherWasOpen = existing->getVisible();
                if (mFunctionalLauncherWasOpen) existing->closeFloater(false);
            }
        }
        mFunctionalControlBefore = control->getValue();
        const std::string owner_probe = entry["probe"].asString();
        if (owner_probe == "dance_owner")
        {
            mFunctionalOwnerModelBefore = ALDanceBall::instance().exportSession();
            mFunctionalOwnerModelBefore["runtime"]["enabled"] =
                ALDanceBall::instance().enabled();
            mFunctionalOwnerModelBefore["runtime"]["frozen"] =
                ALDanceBall::instance().frozen();
            mFunctionalDanceRampBefore = ALDanceBall::instance().speedRampProgress();
            mFunctionalDanceStatusRevisionBefore =
                ALDanceBall::instance().statusRevision();
        }
        else if (owner_probe == "ghost_owner")
        {
            mFunctionalOwnerModelBefore = LLSD::emptyMap();
            mFunctionalOwnerModelBefore["show_all"] = ALGhostStudio::instance().getShowAll();
            ALToolGhostEdit* tool = ALToolGhostEdit::instanceExists()
                ? ALToolGhostEdit::getInstance() : nullptr;
            mFunctionalOwnerModelBefore["editing"] = tool && tool->isEditModeActive();
        }
        else if (owner_probe == "gaze_slot" && gAgentAvatarp)
        {
            mFunctionalGazeActor = gAgentAvatarp->getID();
            mFunctionalOwnerModelBefore = LLDirectorCast::instance().isLookAtCamera(
                mFunctionalGazeActor);
        }
        else if (owner_probe == "prop_owner")
        {
            mFunctionalOwnerModelBefore = LLSD::emptyMap();
            mFunctionalOwnerModelBefore["roster"] = static_cast<S32>(
                ALObjectPathMover::instance().getRoster().size());
            S32 nodes = 0;
            S32 driving = 0;
            for (const LLUUID& id : ALObjectPathMover::instance().getRoster())
            {
                if (const LLActorMover::Path* path = LLActorMover::instance().getPath(id))
                    nodes += static_cast<S32>(path->mNodes.size());
                if (ALObjectPathMover::instance().isDriving(id)) ++driving;
            }
            mFunctionalOwnerModelBefore["nodes"] = nodes;
            mFunctionalOwnerModelBefore["driving"] = driving;
        }
        else if (owner_probe == "flycam_stop")
            mFunctionalOwnerModelBefore = static_cast<S32>(
                LLFlycamRecorder::instance().getState());
        else if (owner_probe == "path_selection")
            mFunctionalOwnerModelBefore = LLActorMover::instance().getEditNode();
        else if (owner_probe == "path_owner")
            mFunctionalOwnerModelBefore = functionalPathOwnerState(
                mFunctionalActorMoverId,
                entry["widget_name"].asString() == "btn_copy_to"
                    ? mFunctionalPathLeaderId : LLUUID::null);
        const std::string& function_name = FunctionalControlAccess::functionName(control);
        mFunctionalHandlerName.clear();
        if (!function_name.empty() && LLUICtrl::CommitCallbackRegistry::getValue(function_name))
            mFunctionalHandlerName = function_name;
        else if (!entry["owner"].asString().empty())
            // The generated owner is source-proven at the exact postBuild/binder
            // callback site. Its slot and this witness share the same synchronous
            // commit signal, so observing that signal proves the owner ran.
            mFunctionalHandlerName = "source:" + entry["owner"].asString();
        mFunctionalDynamicSetting = false;
        mFunctionalSettingName = functionalOracleSetting(entry, control);
        rememberFunctionalSetting(mFunctionalSettingName);
        mFunctionalSetting = gSavedSettings.getControl(mFunctionalSettingName).get();
        mFunctionalBefore = mFunctionalSetting ? mFunctionalSetting->getValue() : LLSD();
        mFunctionalHistoryBefore = ALExposureSettingsHistory::revision();
        std::ostringstream before;
        functionalVisibleState(mFunctionalPageRoot, before, control);
        mFunctionalVisibleBefore = before.str();
        mFunctionalConsumerBefore = functionalConsumerValue(entry);
        mFunctionalFloatersBefore.clear();
        functionalVisibleFloaters(gFloaterView ? static_cast<LLView*>(gFloaterView) :
                                  control->getRootView(), mFunctionalFloatersBefore);
        mFunctionalCommitCount = 0;
        mFunctionalHandlerRunCount = 0;
        mFunctionalCommitConnection = control->setCommitCallback(
            [this](LLUICtrl*, const LLSD&)
            {
                ++mFunctionalCommitCount;
                if (!mFunctionalHandlerName.empty()) ++mFunctionalHandlerRunCount;
            });
        mFunctionalActionWorked = exerciseFunctional(entry, control);
        mFunctionalPending = true;
    }

    void settleFunctionalReveal()
    {
        const LLSD& entry = mFunctionalEntries[mFunctionalIndex];
        LLUICtrl* control = mFunctionalControl.get();
        ++mFunctionalRevealFrames;
        mFunctionalObserved["frames"]["reveal"] = mFunctionalRevealFrames;
        if (control) revealFunctionalControl(control);
        if (!control || !control->isInVisibleChain())
        {
            if (mFunctionalRevealFrames < 6) return;
            const bool deferred = entry["action"].asString() == "DEFER_INWORLD" &&
                (!entry["enable_when"].asString().empty() ||
                 !entry["visible_when"].asString().empty() ||
                 functionalHazard(entry, "requires_world") ||
                 functionalHazard(entry, "requires_selection"));
            const bool declared = !entry["visible_when"].asString().empty() || deferred;
            mFunctionalObserved["visible_break"] = functionalChainBreak(control, true);
            addFunctionalResult(entry, declared ? "SKIP" : "FAIL",
                declared ? (entry["reason"].asString().empty() ?
                    "DEFER_INWORLD: " + entry["visible_when"].asString() :
                    entry["reason"].asString()) :
                           "reveal did not settle within 6 frames");
            mFunctionalRevealPending = false;
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        // Selector/card changes and scroll-to-child layout are idle-deferred.
        // One complete frame after the first reveal keeps the down/up gesture
        // on the same final control rect instead of the shell beneath it.
        if (mFunctionalRevealFrames < 2) return;
        mFunctionalRevealPending = false;
        if (entry["action"].asString() == "DEFER_INWORLD" && !mInWorld)
        {
            const bool predicate = !entry["enable_when"].asString().empty() ||
                !entry["visible_when"].asString().empty() ||
                functionalHazard(entry, "requires_world") || functionalHazard(entry, "requires_selection");
            addFunctionalResult(entry, predicate && !entry["reason"].asString().empty() ? "SKIP" : "FAIL",
                predicate ? entry["reason"].asString() :
                    "DEFER_INWORLD lacks enable_when/visible_when or a world hazard");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        if (!prepareFunctionalPrerequisite(entry, control))
        {
            addFunctionalResult(entry, "FAIL", "enable chain prerequisite could not be established");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        continueFunctionalEntry(entry, control);
    }

    // ui-draw-lint: allow-function(lookup,raw-write) test-only VCam fixture selection
    void prepareFunctionalVCamSelection(const LLSD& entry)
    {
        if (!mInWorld || !mFunctionalPageRoot || !mFunctionalVCamModel ||
            entry["page"].asString() != "vcam_tab") return;
        mFunctionalVCamModel->refreshNow();
        const LLPrismLens::RegistrySnapshot& registry = mFunctionalVCamModel->registry();
        if (!registry.mCaptureCount) return;
        if (LLUICtrl* selector =
                mFunctionalPageRoot->findChild<LLUICtrl>("vc_camera_selector", true))
        {
            selector->setValue(registry.mCaptures[0].mHandle.mId);
            selector->onCommit();
        }
        const std::string card = entry["card"].asString();
        if (card != "face_mapping" && card != "tv_effects" && card != "raw_output") return;
        LLUICtrl* faces = mFunctionalPageRoot->findChild<LLUICtrl>("vc_face_list", true);
        if (!faces) return;
        if ((card == "face_mapping" || card == "tv_effects") && registry.mDisplayCount)
        {
            faces->setValue(registry.mDisplays[0].mHandle.mId);
            if (entry["probe"].asString() == "vc_face_list")
            {
                mFunctionalVCamModel->setSelectedDisplay(LLPrismLens::DisplayHandle());
                return;
            }
        }
        else
            faces->setValue(LLUUID::null);
        faces->onCommit();
    }

    // ui-draw-lint: allow-function(lookup,raw-write,setting-write) isolated login-only functional pre-state
    void beginFunctionalEntry()
    {
        const LLSD& entry = mFunctionalEntries[mFunctionalIndex];
        if (!mFunctionalShaderRestore.empty() &&
            entry["page"].asString() != "shadows_probes_tab")
            finishFunctionalShaderBatch();
        if (!mFunctionalGatePrepared)
        {
            mFunctionalObserved = LLSD::emptyMap();
            functionalBeginInputState();
        }
        const std::string action = entry["action"].asString();
        if (mInWorld && !entry["inworld_skip_reason"].asString().empty())
        {
            addFunctionalResult(entry, "SKIP", entry["inworld_skip_reason"].asString());
            ++mFunctionalIndex;
            return;
        }
        if (mInWorld && !entry["inworld_known_gap_reason"].asString().empty())
        {
            addFunctionalResult(entry, "KNOWN_GAP",
                                entry["inworld_known_gap_reason"].asString());
            ++mFunctionalIndex;
            return;
        }
        if (action == "NO_EXERCISE")
        {
            const std::string reason = entry["reason"].asString();
            addFunctionalResult(entry, reason.empty() ? "FAIL" : "SKIP",
                reason.empty() ? "NO_EXERCISE lacks an explicit manifest reason" : reason);
            ++mFunctionalIndex;
            return;
        }
        if (action == "SKIP")
        {
            std::string reason = entry["reason"].asString();
            addFunctionalResult(entry, reason.empty() ? "FAIL" : "SKIP",
                                reason.empty() ? "manifest SKIP lacks explicit reason" : reason);
            ++mFunctionalIndex;
            return;
        }
        if (mInWorld && (!entry["inworld_allowed"].asBoolean() ||
                         entry["widget_name"].asString() == "vc_lens_locate" ||
                         entry["widget_name"].asString() == "vc_face_locate" ||
                         // Anim explorer: stop/revoke message the simulator; blacklist
                         // writes the account's asset_blocklist.
                         entry["widget_name"].asString() == "btn_anim_ex_stop" ||
                         entry["widget_name"].asString() == "btn_anim_ex_revoke" ||
                         entry["widget_name"].asString() == "btn_anim_ex_blacklist" ||
                         // Any LLSelectMgr selection broadcasts PointAt/AGENT_STATE_EDITING;
                         // Animation Explorer sends ObjectSelect for animation sources.
                         entry["widget_name"].asString() == "btn_enroll" ||
                         entry["widget_name"].asString() == "db_add_selected" ||
                         entry["widget_name"].asString() == "db_from_explorer" ||
                         // Opens Animation Explorer (ObjectSelect for sources) /
                         // ghost edit mode selects its proxy volume (PointAt).
                         entry["widget_name"].asString() == "btn_open_animexp" ||
                         entry["widget_name"].asString() == "edit_ghosts_check"))
        {
            addFunctionalResult(entry, "SKIP",
                "in-world: not on the viewer-local allow-list");
            ++mFunctionalIndex;
            return;
        }
        if (mInWorld && entry["page"].asString() == "vcam_tab" &&
            entry["card"].asString().compare(0, 9, "projector") == 0 &&
            !mFunctionalHaveProjector)
        {
            addFunctionalResult(entry, "SKIP", "no projector light nearby");
            ++mFunctionalIndex;
            return;
        }
        if (mInWorld && functionalHazard(entry, "handler_unknown"))
        {
            addFunctionalResult(entry, "SKIP", "in-world: hazard unknown");
            ++mFunctionalIndex;
            return;
        }
        const bool server_dangerous = mInWorld && (
            functionalHazard(entry, "server_message") ||
            functionalHazard(entry, "agent_send") ||
            functionalHazard(entry, "object_send") ||
            functionalHazard(entry, "selection_send") ||
            functionalHazard(entry, "parcel_send") ||
            functionalHazard(entry, "chat_im_voice") ||
            functionalHazard(entry, "world_action") ||
            functionalHazard(entry, "scripted_dialog"));
        const bool dangerous = server_dangerous || functionalHazard(entry, "file_picker") ||
            functionalHazard(entry, "destructive") || functionalHazard(entry, "logout") ||
            functionalHazard(entry, "delete") || functionalHazard(entry, "rename") ||
            functionalHazard(entry, "export_import") || functionalHazard(entry, "save_disk");
        if (dangerous)
        {
            const std::string reason = server_dangerous ?
                "in-world: server-side action denied" : entry["reason"].asString();
            addFunctionalResult(entry, reason.empty() ? "FAIL" : "SKIP",
                reason.empty() ? "hazard requires an explicit manifest skip reason" : reason);
            ++mFunctionalIndex;
            return;
        }
        if (!prepareFunctionalRimGate(entry)) return;
        if (!mFunctionalGatePrepared) prepareFunctionalRestore(entry);
        prepareFunctionalSelector(entry);
        prepareFunctionalConsumerGate(entry);
        if (!prepareFunctionalDeclaredPrerequisites(entry))
        {
            addFunctionalResult(entry, "FAIL", "enabled_control prerequisite could not be established");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        mFunctionalPageRoot = functionalPage(entry);
        prepareFunctionalVCamSelection(entry);
        if (mInWorld && mFunctionalHaveProjector &&
            entry["page"].asString() == "vcam_tab" &&
            entry["card"].asString().compare(0, 9, "projector") == 0 &&
            !mFunctionalProjectorSelected)
        {
            LLUICtrl* projector_list = mFunctionalPageRoot ?
                mFunctionalPageRoot->findChild<LLUICtrl>("vc_projector_list", true) : nullptr;
            std::string selection_reason;
            LLUICtrl* saved_control = mFunctionalControl.get();
            if (projector_list)
            {
                revealFunctionalControl(projector_list);
                mFunctionalControl = projector_list->getDerivedHandle<LLUICtrl>();
            }
            bool selected = projector_list &&
                functionalReachable(projector_list, selection_reason);
            if (selected)
            {
                const LLRect local = projector_list->getLocalRect();
                projector_list->localPointToScreen(local.getCenterX(),
                    local.mTop - llmin(10, llmax(1, local.getHeight() / 4)),
                    &mFunctionalScreenX, &mFunctionalScreenY);
                selected = functionalPointerClick();
            }
            mFunctionalControl = saved_control ? saved_control->getDerivedHandle<LLUICtrl>() :
                LLHandle<LLUICtrl>();
            if (!selected)
            {
                addFunctionalResult(entry, "FAIL", "projector binding selection failed: " +
                    (mFunctionalLastClickReason.empty() ? selection_reason :
                                                          mFunctionalLastClickReason));
                mFunctionalHaveProjector = false;
                restoreFunctionalAction();
                ++mFunctionalIndex;
                return;
            }
            mFunctionalProjectorSelected = true;
        }
        // A page may contain both native controls and a Lightbox launcher
        // (Actors and Atmosphere do). Prefer the control on the selected
        // Director page; only follow the launcher for rows not hosted there.
        LLUICtrl* control = functionalControl(mFunctionalPageRoot, entry);
        if (!control && !openFunctionalRedirect(entry, mFunctionalPageRoot))
        {
            addFunctionalResult(entry, "FAIL", mFunctionalRedirectReason.empty() ?
                "launcher: target did not open" : mFunctionalRedirectReason);
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        if (!control) control = functionalControl(mFunctionalPageRoot, entry);
        if (!control)
        {
            // The generated page-level route can only name one Lightbox tab,
            // while Atmosphere, Image and Bloom rows are sourced from several
            // tabs. Resolve the exact widget in the verified redirect floater,
            // then select every owning tab through the public ancestor path.
            if (LLFloater* redirected = mFunctionalRedirectFloater.get())
            {
                control = functionalControl(redirected, entry);
                if (control)
                {
                    revealFunctionalAncestors(control);
                    mFunctionalPageRoot = redirected;
                    mFunctionalObserved["launcher"]["resolved_owning_tab"] = true;
                }
            }
        }
        if (action == "EXPECT_MISSING")
        {
            const bool page_open = mFunctionalPageRoot != nullptr;
            addFunctionalResult(entry, page_open && !control ? "PASS" : "FAIL",
                !page_open ? "page did not open; absence is unproven" :
                (control ? "control unexpectedly present" : "expected missing control"));
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        if (!control)
        {
            addFunctionalResult(entry, "FAIL", "control not found in " +
                entry["host"]["floater"].asString() + "/" +
                (entry["host"]["tab"].asString().empty() ?
                    entry["host"]["page_view"].asString() : entry["host"]["tab"].asString()));
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        revealFunctionalControl(control);
        if (!prepareFunctionalPrerequisite(entry, control))
        {
            addFunctionalResult(entry, "FAIL", "enabled_control prerequisite could not be established");
            restoreFunctionalAction();
            ++mFunctionalIndex;
            return;
        }
        mFunctionalControl = control->getDerivedHandle<LLUICtrl>();
        mFunctionalRevealFrames = 0;
        mFunctionalRevealPending = true;
    }

    // ui-draw-lint: allow-function(setting,lookup) isolated test-only assertion
    void finishFunctionalEntry()
    {
        const LLSD& entry = mFunctionalEntries[mFunctionalIndex];
        std::smatch anim_slot_match;
        const std::string widget_name = entry["widget_name"].asString();
        if (std::regex_match(widget_name, anim_slot_match,
                             std::regex(R"(anim_slot_(\d{2}))")))
            mFunctionalAnimSwitcherSlot = llclamp(
                std::stoi(anim_slot_match[1].str()) - 1, 0,
                ALDirectorAnimSwitcher::SLOT_COUNT - 1);
        LLUICtrl* control = mFunctionalControl.get();
        LLSD expected = entry["expect"]["value"];
        std::string type = entry["expect"]["type"].asString();
        if (mFunctionalDynamicSetting)
        {
            expected = control ? control->getValue() : LLSD();
            if (dynamic_cast<LLCheckBoxCtrl*>(control)) type = "Boolean";
            else if (dynamic_cast<LLComboBox*>(control)) type = "S32";
            else type = "F32";
        }
        if (entry["exercise"].asString() == "toggle")
            expected = !mFunctionalBefore.asBoolean();
        else if (dynamic_cast<LLRadioGroup*>(control))
            expected = mFunctionalRadioExpected;
        else if (expected.isMap() && expected["resolve"].asString() == "runtime" &&
                 dynamic_cast<LLComboBox*>(control))
            // Runtime-populated combos have no stable build-time value.  The
            // independent oracle is that the bound setting equals the item
            // reached through the real popup click.
            expected = control->getValue();
        else if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(control);
                 spinner && entry["increment"].isDefined())
        {
            const F64 minimum = entry["min"].asReal();
            const F64 maximum = entry["max"].asReal();
            const F64 increment = llabs(entry["increment"].asReal());
            const F64 before = mFunctionalControlBefore.asReal();
            const F64 scale = std::pow(10.0, spinner->getPrecision());
            // LLSpinCtrl parses the precision-clamped editor text, then adds one
            // increment and clamps precision again (onUpBtn/onDownBtn).
            const F64 current = scale > 0.0
                ? ll_round(static_cast<F32>(before) * scale) / scale : before;
            const F64 direction = entry["test_value"].asReal() >= before ? 1.0 : -1.0;
            const F64 stepped = current + direction * increment;
            const F64 rounded = scale > 0.0 ? ll_round(stepped * scale) / scale : stepped;
            expected = llclamp(rounded, minimum, maximum);
        }
        const LLSD actual = mFunctionalSetting ? mFunctionalSetting->getValue() : LLSD();
        const bool changed = mFunctionalSetting &&
            !functionalEqual(actual, mFunctionalBefore, type, entry["increment"]);
        bool observable = false;
        bool owner_state_changed = false;
        bool expected_ok = true;
        std::string assertion_failure;
        if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(control))
        {
            const F64 before = mFunctionalSetting
                ? mFunctionalBefore.asReal() : mFunctionalControlBefore.asReal();
            const bool default_in_range = before >= entry["min"].asReal() &&
                before <= entry["max"].asReal();
            const F64 display_scale = std::pow(10.0, spinner->getPrecision());
            const F64 scaled_increment = llabs(entry["increment"].asReal()) * display_scale;
            const bool increment_representable =
                std::fabs(scaled_increment - std::round(scaled_increment)) <= 1e-4;
            const bool display_matches_setting = !mFunctionalSetting ||
                std::round(mFunctionalControlBefore.asReal() * display_scale) ==
                    std::round(mFunctionalBefore.asReal() * display_scale);
            mFunctionalObserved["spinner"]["default_in_range"] = default_in_range;
            mFunctionalObserved["spinner"]["increment_representable"] =
                increment_representable;
            mFunctionalObserved["spinner"]["display_matches_setting_before"] =
                display_matches_setting;
            expected_ok = expected_ok && default_in_range && increment_representable &&
                display_matches_setting;
            if (!display_matches_setting)
                assertion_failure = "spinner display was stale before the exercise";
            else if (!increment_representable)
                assertion_failure = "spinner increment is not representable at its runtime precision";
            else if (!default_in_range)
                assertion_failure = "spinner default is outside the manifest range";
        }
        if (!mFunctionalSettingName.empty())
        {
            bool setting_matches = mFunctionalSetting != nullptr;
            if (setting_matches && dynamic_cast<LLColorSwatchCtrl*>(control) &&
                type == "Color3" && actual.isArray() && expected.isArray())
            {
                setting_matches = actual.size() >= 3 && expected.size() >= 3;
                for (S32 channel = 0; setting_matches && channel < 3; ++channel)
                    setting_matches = functionalEqual(actual[channel], expected[channel], "F32");
            }
            else if (setting_matches)
            {
                setting_matches = functionalEqual(actual, expected, type, entry["increment"]);
            }
            expected_ok = expected_ok && setting_matches;
            observable = changed;
        }
        else
        {
            std::ostringstream after;
            functionalVisibleState(mFunctionalPageRoot, after, control);
            owner_state_changed = after.str() != mFunctionalVisibleBefore;
            // History/undo and the test's own call are deliberately ignored.
            // A commit-only PASS requires a named registry/binder/provider
            // owner and a side effect outside the exercised widget.
            observable = !mFunctionalHandlerName.empty() && owner_state_changed;
        }
        if (LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(control))
        {
            const bool selected_expected = mFunctionalListExpected.isDefined() &&
                list->getSelectedValue() == mFunctionalListExpected;
            bool owner_ok = selected_expected;
            if (widget_name == "localfog_volume_list")
                owner_ok = owner_ok && ALLocalFogManager::instance().selectedVolume() ==
                    mFunctionalListExpected.asInteger();
            else if (widget_name == "db_playlist_list")
            {
                LLLineEditor* rename = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLLineEditor>("db_rename_edit", true)
                    : nullptr;
                const S32 selected = mFunctionalListExpected.asInteger();
                const auto& playlist = ALDanceBall::instance().playlist();
                owner_ok = owner_ok && rename && selected >= 0 &&
                    selected < static_cast<S32>(playlist.size()) &&
                    rename->getText() == playlist[selected].mLabel;
            }
            else if (widget_name == "prop_list" || widget_name == "node_list")
            {
                // Owner oracle: the panel's own detail controls must show the
                // clicked prop's path (distinct per fixture) / the clicked
                // node's yaw. The prep step left sentinels that only the
                // selection handler (refreshNodePane) replaces.
                LLScrollListCtrl* props = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLScrollListCtrl>("prop_list", true) : nullptr;
                LLScrollListCtrl* nodes = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLScrollListCtrl>("node_list", true) : nullptr;
                LLSpinCtrl* speed = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLSpinCtrl>("path_speed_spin", true) : nullptr;
                LLSpinCtrl* skid = mFunctionalPageRoot
                    ? mFunctionalPageRoot->findChild<LLSpinCtrl>("node_skid_spin", true) : nullptr;
                const LLUUID prop_id = props ? props->getSelectedValue().asUUID() : LLUUID::null;
                const bool is_fixture = std::find(mFunctionalPropFixtures.begin(),
                    mFunctionalPropFixtures.end(), prop_id) != mFunctionalPropFixtures.end();
                const LLActorMover::Path* path = is_fixture
                    ? LLActorMover::instance().getPath(prop_id) : nullptr;
                owner_ok = owner_ok && path && nodes && speed &&
                    nodes->getItemCount() == static_cast<S32>(path->mNodes.size()) &&
                    std::fabs(speed->getValueF32() - path->mSpeed) < 0.01f;
                if (widget_name == "prop_list")
                {
                    LLScrollListItem* clicked = control
                        ? static_cast<LLScrollListCtrl*>(control)->getFirstSelected() : nullptr;
                    owner_ok = owner_ok && clicked && clicked->getValue().asUUID() == prop_id;
                }
                else
                {
                    const S32 selected = mFunctionalListExpected.asInteger();
                    LLScrollListItem* clicked = nodes ? nodes->getFirstSelected() : nullptr;
                    owner_ok = owner_ok && skid && clicked && selected >= 0 &&
                        selected < static_cast<S32>(path->mNodes.size()) &&
                        clicked->getValue().asInteger() == selected &&
                        std::fabs(skid->getValueF32() -
                                  path->mNodes[selected].mYawOffset * RAD_TO_DEG) < 0.05f;
                }
                mFunctionalObserved["list"]["prop_fixture_selected"] = is_fixture;
            }
            mFunctionalObserved["list"]["selected_expected"] = selected_expected;
            mFunctionalObserved["list"]["owner_oracle"] = owner_ok;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure =
                "list selection or its viewer-local owner oracle did not match the clicked row";
        }
        const std::string owner_probe = entry["probe"].asString();
        if (owner_probe == "path_selection")
        {
            // The edit node must MOVE to the clicked row: a pre-state that
            // already equals the target cannot prove the handler ran.
            const bool selected = LLActorMover::instance().getEditNode() ==
                    mFunctionalListExpected.asInteger() &&
                mFunctionalOwnerModelBefore.asInteger() != mFunctionalListExpected.asInteger();
            mFunctionalObserved["path"]["edit_node"] =
                LLActorMover::instance().getEditNode();
            observable = selected;
            expected_ok = expected_ok && selected;
            if (!selected) assertion_failure = "waypoint selection did not move LLActorMover edit-node state";
        }
        else if (owner_probe == "path_owner" && mFunctionalActorMoverId.notNull())
        {
            const LLUUID destination = widget_name == "btn_copy_to"
                ? mFunctionalPathLeaderId : LLUUID::null;
            const LLSD after = functionalPathOwnerState(
                mFunctionalActorMoverId, destination);
            const LLSD& before = mFunctionalOwnerModelBefore;
            const LLSD& before_path = before["path"];
            const LLSD& after_path = after["path"];
            const LLSD& before_nodes = before_path["nodes"];
            const LLSD& after_nodes = after_path["nodes"];
            bool owner_ok = after != before;
            if (widget_name == "btn_wp_add" || widget_name == "btn_wp_insert")
                owner_ok = owner_ok && after_nodes.size() == before_nodes.size() + 1;
            else if (widget_name == "btn_edit_undo")
                owner_ok = after_path == mFunctionalPathExpected &&
                    after["undo_depth"].asInteger() + 1 == before["undo_depth"].asInteger() &&
                    after["redo_depth"].asInteger() == before["redo_depth"].asInteger() + 1;
            else if (widget_name == "btn_edit_redo")
                owner_ok = after_path == mFunctionalPathExpected &&
                    after["undo_depth"].asInteger() == before["undo_depth"].asInteger() + 1 &&
                    after["redo_depth"].asInteger() + 1 == before["redo_depth"].asInteger();
            else if (widget_name == "btn_edit_reverse")
            {
                const S32 node_count = static_cast<S32>(before_nodes.size());
                owner_ok = after_nodes.size() == before_nodes.size() &&
                    after_path != before_path;
                for (S32 i = 0; owner_ok && i < static_cast<S32>(after_nodes.size()); ++i)
                    owner_ok = after_nodes[i] == before_nodes[node_count - 1 - i];
            }
            else if (widget_name == "btn_edit_mirror")
            {
                owner_ok = after_nodes.size() == before_nodes.size() &&
                    after_path != before_path && before_nodes.size() >= 2;
                LLVector3d centroid;
                for (S32 i = 0; owner_ok && i < static_cast<S32>(before_nodes.size()); ++i)
                    centroid += ll_vector3d_from_sd(before_nodes[i]["position"]);
                if (owner_ok)
                    centroid *= 1.0 / static_cast<F64>(before_nodes.size());
                const S32 last = static_cast<S32>(before_nodes.size()) - 1;
                LLVector3d direction = ll_vector3d_from_sd(
                    before_nodes[last]["position"]) -
                    ll_vector3d_from_sd(before_nodes[0]["position"]);
                direction.mdV[VZ] = 0.0;
                if (direction.length() < 1e-3)
                    direction = LLVector3d(1.0, 0.0, 0.0);
                else
                    direction.normalize();
                const LLVector3d normal(-direction.mdV[VY], direction.mdV[VX], 0.0);
                for (S32 i = 0; owner_ok && i < static_cast<S32>(before_nodes.size()); ++i)
                {
                    const LLVector3d point = ll_vector3d_from_sd(before_nodes[i]["position"]);
                    const LLVector3d delta = point - centroid;
                    const F64 projection = delta.mdV[VX] * normal.mdV[VX] +
                        delta.mdV[VY] * normal.mdV[VY];
                    const LLVector3d expected_point(
                        point.mdV[VX] - 2.0 * projection * normal.mdV[VX],
                        point.mdV[VY] - 2.0 * projection * normal.mdV[VY],
                        point.mdV[VZ]);
                    owner_ok = (ll_vector3d_from_sd(after_nodes[i]["position"]) -
                                expected_point).length() < 1e-6;
                }
            }
            else if (widget_name == "btn_edit_loopclose")
                owner_ok = after_path != before_path &&
                    after_path["end_mode"].asInteger() == 1 &&
                    after_nodes.size() >= 2 &&
                    after_nodes[0]["position"] ==
                        after_nodes[static_cast<S32>(after_nodes.size()) - 1]["position"];
            else if (widget_name == "btn_play_path")
                owner_ok = !before["walking"].asBoolean() && after["walking"].asBoolean();
            else if (widget_name == "btn_stop_path")
                owner_ok = before["walking"].asBoolean() && !after["walking"].asBoolean();
            else if (widget_name == "btn_walk_here")
                owner_ok = !before["walk_to_armed"].asBoolean() &&
                    after["walk_to_armed"].asBoolean();
            else if (widget_name == "btn_copy_to")
                owner_ok = after["destination_path"] == after_path &&
                    before["destination_path"] != after["destination_path"];
            else if (widget_name == "path_groundfollow_check")
                owner_ok = after_path["ground_follow"].asBoolean() ==
                        control->getValue().asBoolean() &&
                    before_path["ground_follow"] != after_path["ground_follow"];
            else if (widget_name == "path_pitch_check")
                owner_ok = after_path["pitch_to_slope"].asBoolean() ==
                        control->getValue().asBoolean() &&
                    before_path["pitch_to_slope"] != after_path["pitch_to_slope"];
            else if (widget_name == "btn_primitive_center")
            {
                owner_ok = gAgentAvatarp && gAgentAvatarp->getRootJoint() &&
                    after_path["primitive_center"] != before_path["primitive_center"];
                if (owner_ok)
                {
                    LLVector3d expected_center = gAgent.getPosGlobalFromAgent(
                        gAgentAvatarp->getBaseRootPosition());
                    expected_center.mdV[VZ] -= llmax(0.f, gAgentAvatarp->getPelvisToFoot());
                    owner_ok = (ll_vector3d_from_sd(after_path["primitive_center"]) -
                                expected_center).length() < 1e-5;
                }
            }
            else if (widget_name == "btn_fit_half_arc")
                owner_ok = after_path != before_path &&
                    after_path["shape"].asInteger() == ALPathGeometry::CIRCLE_ARC;
            else if (widget_name == "btn_fit_sine")
                owner_ok = after_path != before_path &&
                    after_path["shape"].asInteger() == ALPathGeometry::SINE_WAVE;
            else if (widget_name == "btn_stop_following")
                owner_ok = before["following"].asBoolean() &&
                    !after["following"].asBoolean();
            else if (widget_name == "btn_node_setcam")
            {
                owner_ok = after_path != before_path && after_nodes.size() > 1 &&
                    after_nodes[1]["has_camera"].asBoolean();
                if (owner_ok)
                {
                    const LLVector3d expected_position = ll_vector3d_from_sd(
                        before["render_camera_position"]);
                    owner_ok = (ll_vector3d_from_sd(after_nodes[1]["camera_position"]) -
                                expected_position).length() < 1e-5 &&
                        std::fabs(after_nodes[1]["camera_fov"].asReal() -
                                  before["render_camera_fov"].asReal()) < 1e-5;
                }
            }
            else if (widget_name == "node_cam_transition_combo")
                owner_ok = after_path != before_path && after_nodes.size() > 1 &&
                    after_nodes[1]["camera_transition"].asInteger() ==
                        control->getValue().asInteger();
            else if (widget_name == "btn_node_preview")
                owner_ok = !before["previewing"].asBoolean() &&
                    after["previewing"].asBoolean();
            else if (after_nodes.size() > 1 && before_nodes.size() > 1)
            {
                const char* field = widget_name == "node_height_spinner" ? "ground_offset" :
                    widget_name == "node_dwell_spinner" ? "dwell" :
                    widget_name == "node_speed_spinner" ? "speed_override" :
                    widget_name == "node_corner_radius_spinner" ? "corner_radius" : nullptr;
                if (field)
                    owner_ok = after_nodes[1][field] != before_nodes[1][field] &&
                        std::fabs(after_nodes[1][field].asReal() -
                                  control->getValue().asReal()) < 0.001;
            }
            mFunctionalObserved["path"]["before"] = before;
            mFunctionalObserved["path"]["after"] = after;
            mFunctionalObserved["path"]["owner_oracle"] = owner_ok;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok)
                assertion_failure = "path control did not move LLActorMover state to its expected value";
        }
        else if (owner_probe == "dance_owner" &&
                 widget_name != "db_playlist_list")
        {
            const ALDanceBall& ball = ALDanceBall::instance();
            LLSD dance_after = ball.exportSession();
            dance_after["runtime"]["enabled"] = ball.enabled();
            dance_after["runtime"]["frozen"] = ball.frozen();
            bool owner_changed = dance_after != mFunctionalOwnerModelBefore;
            if (widget_name == "db_ramp_go")
                owner_changed = owner_changed ||
                    ball.speedRampProgress() != mFunctionalDanceRampBefore;
            if (widget_name == "db_reshuffle" || widget_name == "db_play_selected" ||
                widget_name == "db_play_all" || widget_name == "db_ramp_go")
                owner_changed = owner_changed ||
                    ball.statusRevision() != mFunctionalDanceStatusRevisionBefore;
            bool owner_ok = owner_changed;
            if (widget_name == "db_on") owner_ok = ball.enabled() == control->getValue().asBoolean();
            else if (widget_name == "db_header_desync")
                owner_ok = std::fabs(ball.timing().mDesync - control->getValue().asReal()) < 0.001;
            else if (widget_name == "db_t_speed")
                owner_ok = std::fabs(ball.timing().mSpeed - control->getValue().asReal()) < 0.001;
            else if (widget_name == "db_focus")
                owner_ok = static_cast<S32>(ball.autoJoinFocus()) == control->getValue().asInteger();
            else if (widget_name == "db_autojoin") owner_ok = ball.autoJoin() == control->getValue().asBoolean();
            else if (widget_name == "db_include_seated") owner_ok = ball.includeSeated() == control->getValue().asBoolean();
            else if (widget_name == "db_include_me") owner_ok = ball.includeSelf() == control->getValue().asBoolean();
            else if (widget_name == "db_adv_jitter")
                owner_ok = std::fabs(ball.timing().mAdvanceJitterSeconds - control->getValue().asReal()) < 0.001;
            else if (widget_name == "db_t_speed_jitter")
                owner_ok = std::fabs(ball.timing().mSpeedJitter - control->getValue().asReal()) < 0.001;
            else if (widget_name == "db_t_priority") owner_ok = ball.timing().mPriority == control->getValue().asInteger();
            else if (widget_name == "db_t_dance_drives_face")
                owner_ok = ball.timing().mDanceDrivesFace == control->getValue().asBoolean();
            else if (widget_name == "db_wave_focus")
                owner_ok = static_cast<S32>(ball.wave().mFocus) == control->getValue().asInteger();
            else if (widget_name == "db_wave_speed")
                owner_ok = std::fabs(ball.wave().mSpreadMetersPerSecond - control->getValue().asReal()) < 0.001;
            else if (widget_name == "db_wave_dir")
                owner_ok = static_cast<S32>(ball.wave().mDirection) == control->getValue().asInteger();
            else if (widget_name == "db_wave_apply")
                owner_ok = static_cast<S32>(ball.wave().mApply) == control->getValue().asInteger();
            else if (widget_name == "db_freeze") owner_ok = ball.frozen() == control->getValue().asBoolean();
            else if (widget_name == "db_stop_all") owner_ok = !ball.enabled();
            else if (widget_name == "db_play_selected" || widget_name == "db_play_all")
                owner_ok = ball.enabled() &&
                    ball.statusRevision() != mFunctionalDanceStatusRevisionBefore;
            else if (widget_name == "db_reshuffle")
                owner_ok = ball.statusRevision() != mFunctionalDanceStatusRevisionBefore;
            else if (widget_name == "db_ramp_go")
                owner_ok = ball.speedRampProgress() < 1.0 &&
                    ball.statusRevision() != mFunctionalDanceStatusRevisionBefore;
            owner_ok = owner_changed && owner_ok;
            mFunctionalObserved["dance"]["owner_changed"] =
                owner_changed;
            mFunctionalObserved["dance"]["owner_oracle"] = owner_ok;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure = "dance control did not move its ALDanceBall owner state";
        }
        else if (owner_probe == "ghost_owner")
        {
            ALToolGhostEdit* tool = ALToolGhostEdit::instanceExists()
                ? ALToolGhostEdit::getInstance() : nullptr;
            // The owner must match the control AND have changed from its
            // pre-click value (a panel re-sync alone cannot pass).
            const bool show_all = widget_name == "show_all_check";
            const bool owner_after = show_all
                ? ALGhostStudio::instance().getShowAll()
                : (tool && tool->isEditModeActive());
            const bool owner_before = mFunctionalOwnerModelBefore[
                show_all ? "show_all" : "editing"].asBoolean();
            const bool owner_ok = owner_after == control->getValue().asBoolean() &&
                owner_after != owner_before;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure = "ghost control did not move its viewer-local owner state";
        }
        else if (owner_probe == "gaze_slot" && mFunctionalGazeActor.notNull())
        {
            const bool owner_after = LLDirectorCast::instance().isLookAtCamera(
                mFunctionalGazeActor);
            const bool owner_ok = owner_after == control->getValue().asBoolean() &&
                owner_after != mFunctionalOwnerModelBefore.asBoolean();
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure = "gaze slot did not move LLDirectorCast look-at state";
        }
        else if (owner_probe == "gaze_cast_target" && mFunctionalGazeActor.notNull())
        {
            const LLActorMover::GazeTarget target =
                LLDirectorCast::instance().getGazeTarget(mFunctionalGazeActor);
            const S32 wanted = entry["gaze_cast_mode"].asInteger();
            bool owner_ok = static_cast<S32>(target.mMode) == wanted;
            if (widget_name == "gaze_cast_combo")
                owner_ok = owner_ok && target.mCastRef == control->getValue().asUUID();
            mFunctionalObserved["gaze"]["mode"] = static_cast<S32>(target.mMode);
            mFunctionalObserved["gaze"]["cast_target"] = target.mCastRef;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure = "gaze control did not move LLDirectorCast target state";
        }
        else if (owner_probe == "prop_owner" &&
                 widget_name != "prop_list" && widget_name != "node_list")
        {
            LLSD after = LLSD::emptyMap();
            after["roster"] = static_cast<S32>(ALObjectPathMover::instance().getRoster().size());
            S32 nodes = 0;
            S32 driving = 0;
            for (const LLUUID& id : ALObjectPathMover::instance().getRoster())
            {
                if (const LLActorMover::Path* path = LLActorMover::instance().getPath(id))
                    nodes += static_cast<S32>(path->mNodes.size());
                if (ALObjectPathMover::instance().isDriving(id)) ++driving;
            }
            after["nodes"] = nodes;
            after["driving"] = driving;
            const bool owner_ok = after != mFunctionalOwnerModelBefore;
            mFunctionalObserved["prop"]["before"] = mFunctionalOwnerModelBefore;
            mFunctionalObserved["prop"]["after"] = after;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure = "prop control did not move roster, node, or driving owner state";
        }
        else if (owner_probe == "flycam_stop")
        {
            const bool owner_ok = LLFlycamRecorder::instance().getState() ==
                LLFlycamRecorder::STATE_IDLE &&
                mFunctionalOwnerModelBefore.asInteger() != LLFlycamRecorder::STATE_IDLE;
            observable = owner_ok;
            expected_ok = expected_ok && owner_ok;
            if (!owner_ok) assertion_failure = "Stop did not move LLFlycamRecorder transport to idle";
        }
        const LLSD consumer_after = functionalConsumerValue(entry);
        mFunctionalObserved["click"]["commits"] = mFunctionalCommitCount;
        mFunctionalObserved["owner"]["handler"] = mFunctionalHandlerName;
        mFunctionalObserved["owner"]["resolved"] = !mFunctionalHandlerName.empty();
        mFunctionalObserved["owner"]["instrumented_runs"] = mFunctionalHandlerRunCount;
        mFunctionalObserved["owner"]["state_changed"] = owner_state_changed;
        mFunctionalObserved["setting"]["name"] = mFunctionalSettingName;
        mFunctionalObserved["setting"]["before"] = mFunctionalBefore;
        mFunctionalObserved["setting"]["after"] = actual;
        mFunctionalObserved["widget"]["before"] = mFunctionalControlBefore;
        mFunctionalObserved["widget"]["after"] = control ? control->getValue() : LLSD();
        mFunctionalObserved["consumer"]["name"] = entry["consumer"].asString();
        mFunctionalObserved["consumer"]["before"] = mFunctionalConsumerBefore;
        mFunctionalObserved["consumer"]["after"] = consumer_after;
        if (entry["consumer"].isDefined())
        {
            const bool consumer_changed = consumer_after != mFunctionalConsumerBefore;
            observable = entry["expect"].has("setting") ?
                (observable && consumer_changed) : consumer_changed;
            const bool consumer_ok = functionalConsumerExpected(entry, consumer_after);
            expected_ok = expected_ok && consumer_ok;
            if (!consumer_ok)
                assertion_failure = "engine consumer '" + entry["consumer"].asString() +
                    "' did not match the exercised UI state";
        }
        if (entry["probe"].asString() == "selector_segment")
        {
            std::ostringstream after;
            functionalVisibleState(mFunctionalPageRoot, after, control);
            expected_ok = expected_ok && after.str() != mFunctionalVisibleBefore;
            if (after.str() == mFunctionalVisibleBefore)
                assertion_failure = "selector changed no displayed page values";
        }
        if (entry["probe"].asString() == "temporal_preset")
        {
            LLControlVariable* mode = gSavedSettings.getControl("TemporalMode").get();
            const S32 actual_mode = mode ? mode->getValue().asInteger() : -1;
            const S32 expected_mode = entry["temporal_mode"].asInteger();
            const bool mode_ok = mode && actual_mode == expected_mode;
            mFunctionalObserved["temporal_mode"]["expected"] = expected_mode;
            mFunctionalObserved["temporal_mode"]["actual"] = actual_mode;
            expected_ok = expected_ok && mode_ok;
            if (!mode_ok)
                assertion_failure = "temporal preset did not set the expected mode";
        }
        if (entry["probe"].asString() == "gaze_reset")
        {
            const std::string gaze_field = entry["gaze_oracle_field"].asString();
            bool reset_ok = false;
            if (!gaze_field.empty() && mFunctionalGazeActor.notNull())
            {
                const LLActorMover::GazeTarget target =
                    LLActorMover::instance().getGazeTargetConfig(mFunctionalGazeActor);
                const LLSD actual_field = functionalGazeField(target, gaze_field);
                LLSD expected_field = entry["gaze_oracle_value"];
                std::string expected_type = expected_field.isInteger() ? "S32" : "F32";
                if (gaze_field == "limit_profile")
                {
                    expected_field = functionalGazeField(LLActorMover::GazeTarget(), gaze_field);
                    expected_type.clear();
                }
                reset_ok = functionalEqual(actual_field, expected_field, expected_type);
                mFunctionalObserved["gaze"]["field"] = gaze_field;
                mFunctionalObserved["gaze"]["actual"] = actual_field;
                mFunctionalObserved["gaze"]["expected"] = expected_field;
            }
            else
            {
                LLUICtrl* target = mFunctionalSecondaryControl.get();
                LLSD reset_value = entry["reset_value"];
                const std::string reset_setting = entry["reset_setting"].asString();
                if (!reset_setting.empty())
                    if (LLControlVariable* setting = gSavedSettings.getControl(reset_setting).get())
                        reset_value = setting->getValue();
                reset_ok = target && functionalEqual(
                    target->getValue(), reset_value,
                    entry["reset_type"].asString(), entry["increment"]);
            }
            observable = reset_ok;
            expected_ok = expected_ok && reset_ok;
            if (!reset_ok) assertion_failure = "reset did not restore the target control's default";
        }
        if (entry["probe"].asString() == "cine_fx_stop")
        {
            const S32 active = ALCineLightRigManager::instance().selected().activeFX();
            const bool stopped = active == -1;
            mFunctionalObserved["cine_fx"]["active_after"] = active;
            observable = stopped;
            expected_ok = expected_ok && stopped;
            if (!stopped) assertion_failure = "cine FX remained active after Stop";
        }
        if (entry["probe"].asString() == "cine_live_probe_refresh")
        {
            const LLReflectionMapManager::CinematicRefreshStatus after =
                gPipeline.mReflectionMapManager.getCinematicRefreshStatus();
            const bool request_clock_restarted =
                mFunctionalProbeStatusBefore.mReason != ALCineLiveProbeRefresh::Reason::MANUAL ||
                mFunctionalProbeStatusBefore.mSecondsSinceReason < 0.f ||
                after.mSecondsSinceReason < mFunctionalProbeStatusBefore.mSecondsSinceReason;
            const bool requested = after.mStepping &&
                after.mReason == ALCineLiveProbeRefresh::Reason::MANUAL &&
                after.mSecondsSinceReason >= 0.f && request_clock_restarted;
            mFunctionalObserved["live_probe"]["reason_after"] =
                ALCineLiveProbeRefresh::reasonName(after.mReason);
            mFunctionalObserved["live_probe"]["seconds_before"] =
                mFunctionalProbeStatusBefore.mSecondsSinceReason;
            mFunctionalObserved["live_probe"]["seconds_after"] = after.mSecondsSinceReason;
            observable = requested;
            expected_ok = expected_ok && requested;
            if (!requested) assertion_failure = "live-probe refresh request produced no scheduler state transition";
        }
        if (entry["probe"].asString() == "vc_snap_view" && mFunctionalVCamModel)
        {
            mFunctionalVCamModel->refreshNow();
            const LLPrismLens::CaptureHandle selected = mFunctionalVCamModel->selectedCapture();
            const LLPrismLens::CaptureDefinition* capture = nullptr;
            for (const LLPrismLens::CaptureDefinition& candidate :
                 mFunctionalVCamModel->registry().mCaptures)
                if (candidate.mHandle.mId == selected.mId) { capture = &candidate; break; }
            const bool snapped = capture &&
                (capture->mCamera.mVirtualPos - mFunctionalVCamExpectedPos).length() < .001f;
            observable = snapped;
            expected_ok = expected_ok && snapped;
            if (!snapped) assertion_failure = "virtual capture camera did not snap to the current view";
        }
        if (entry["probe"].asString() == "vc_face_list" && mFunctionalVCamModel)
        {
            const LLUUID expected_display = control ? control->getValue().asUUID() : LLUUID::null;
            const LLUUID selected_display = mFunctionalVCamModel->selectedDisplay().mId;
            const bool selected = expected_display.notNull() && selected_display == expected_display;
            observable = selected;
            expected_ok = expected_ok && selected;
            if (!selected) assertion_failure = "face list did not move the selected display handle";
        }
        if (entry["probe"].asString() == "vc_camera_selector" && mFunctionalVCamModel)
        {
            const LLUUID expected_capture = control ? control->getValue().asUUID() : LLUUID::null;
            const LLUUID selected_capture = mFunctionalVCamModel->selectedCapture().mId;
            const bool selected = expected_capture.notNull() && selected_capture == expected_capture;
            observable = selected;
            expected_ok = expected_ok && selected;
            if (!selected) assertion_failure = "camera selector did not move the selected capture handle";
        }
        if (entry["probe"].asString() == "vcam_test_pattern" && mFunctionalVCamModel)
        {
            mFunctionalVCamModel->refreshNow();
            const LLPrismLens::CaptureHandle selected =
                mFunctionalVCamModel->selectedCapture();
            const LLPrismLens::CaptureDefinition* capture = nullptr;
            for (const LLPrismLens::CaptureDefinition& candidate :
                 mFunctionalVCamModel->registry().mCaptures)
                if (candidate.mHandle.mId == selected.mId &&
                    candidate.mHandle.mGeneration == selected.mGeneration)
                {
                    capture = &candidate;
                    break;
                }
            const bool pattern = capture && capture->mCamera.mTestPattern;
            const bool ready = capture &&
                capture->mRuntime.mHealth == LLPrismLens::ECaptureHealth::READY;
            const bool current = capture &&
                capture->mRuntime.mOutput == LLPrismLens::EOutputState::CURRENT &&
                capture->mRuntime.mHasOutput;
            mFunctionalObserved["vcam"]["test_pattern"] = pattern;
            mFunctionalObserved["vcam"]["health_ready"] = ready;
            mFunctionalObserved["vcam"]["output_current"] = current;
            observable = pattern && ready && current;
            expected_ok = expected_ok && observable;
            if (!pattern) assertion_failure = "selected capture source is not the test pattern";
            else if (!ready) assertion_failure = "test-pattern capture did not become READY";
            else if (!current) assertion_failure = "test-pattern capture produced no current output";
        }
        if (entry["probe"].asString() == "rig_projector_binding" &&
            mFunctionalVCamModel)
        {
            mFunctionalVCamModel->refreshNow();
            const U8 rig_slot = static_cast<U8>(
                ALCineLightRigManager::instance().selectedSlot());
            const LLUUID capture_id = mFunctionalRigProjectorCapture;
            bool bound = false;
            for (const LLPrismLens::ProjectorDefinition& projector :
                 mFunctionalVCamModel->registry().mProjectors)
                if (LLPrismLens::sameRigProjectorIdentity(projector, rig_slot, 0) &&
                    projector.mCapture.mId == capture_id)
                {
                    bound = true;
                    break;
                }
            mFunctionalObserved["vcam"]["rig_projector_binding"] = bound;
            observable = mFunctionalRigProjectorAbsentBefore && bound;
            expected_ok = expected_ok && observable;
            if (!mFunctionalRigProjectorAbsentBefore) assertion_failure =
                "rig-light projector binding was already present before the click";
            else if (!bound) assertion_failure =
                "stable rig-light projector binding is absent from the registry";
        }
        if (entry["probe"].asString() == "actor_mover_active")
        {
            const bool active = LLActorMover::instance().isMoving(mFunctionalActorMoverId);
            const bool wanted = entry["actor_mover_expect_active"].asBoolean();
            const bool mover_ok = active == wanted;
            observable = mover_ok;
            expected_ok = expected_ok && mover_ok;
            if (!mover_ok) assertion_failure = "LLActorMover active state did not reach the expected value";
        }
        if (entry["probe"].asString() == "actor_style_reset")
        {
            const LLDirectorCast::ActorStyle& actual_style =
                LLDirectorCast::instance().getActorStyle(LLUUID::null);
            const LLDirectorCast::ActorStyle defaults;
            const bool reset_ok = actual_style.mEnabled == defaults.mEnabled &&
                actual_style.mMode == defaults.mMode && actual_style.mStyle == defaults.mStyle &&
                actual_style.mUseActorHue == defaults.mUseActorHue &&
                actual_style.mHue == defaults.mHue &&
                actual_style.mAlpha == defaults.mAlpha &&
                actual_style.mDissolveProgress == defaults.mDissolveProgress &&
                actual_style.mPixelSize == defaults.mPixelSize &&
                actual_style.mShimmerSpeed == defaults.mShimmerSpeed &&
                actual_style.mShimmerAmount == defaults.mShimmerAmount &&
                actual_style.mGlitch == defaults.mGlitch &&
                actual_style.mDistortion == defaults.mDistortion &&
                actual_style.mDistortionAmount == defaults.mDistortionAmount &&
                actual_style.mBrightness == defaults.mBrightness &&
                actual_style.mEffectFps == defaults.mEffectFps;
            observable = reset_ok;
            expected_ok = expected_ok && reset_ok;
            if (!reset_ok) assertion_failure = "actor-style model fields were not reset to defaults";
        }
        if (entry["probe"].asString() == "exposure_history")
        {
            const U64 history_after = ALExposureSettingsHistory::revision();
            const bool history_changed = history_after != mFunctionalHistoryBefore;
            mFunctionalObserved["history"]["before"] = llformat(
                "%llu", static_cast<unsigned long long>(mFunctionalHistoryBefore));
            mFunctionalObserved["history"]["after"] = llformat(
                "%llu", static_cast<unsigned long long>(history_after));
            observable = history_changed;
            expected_ok = expected_ok && history_changed;
            if (!history_changed) assertion_failure = "exposure history revision did not change";
        }
        if (entry["reset_defaults"].isMap())
        {
            bool defaults_ok = true;
            for (LLSD::map_const_iterator it = entry["reset_defaults"].beginMap();
                 it != entry["reset_defaults"].endMap(); ++it)
            {
                LLControlVariable* setting = gSavedSettings.getControl(it->first).get();
                defaults_ok = defaults_ok && setting && llsd_equals(setting->get(), it->second);
            }
            observable = defaults_ok;
            expected_ok = expected_ok && defaults_ok;
            if (!defaults_ok) assertion_failure = "Reset all left one or more settings away from defaults";
        }
        const bool modal = functionalHazard(entry, "modal") &&
            !dynamic_cast<LLColorSwatchCtrl*>(control) &&
            entry["probe"].asString() != "launcher";
        if (modal)
        {
            std::set<LLFloater*> after;
            functionalVisibleFloaters(control ? control->getRootView() : nullptr, after);
            bool opened = false;
            for (LLFloater* floater : after)
            {
                if (!mFunctionalFloatersBefore.count(floater))
                {
                    opened = true;
                    floater->closeFloater();
                }
            }
            observable = opened;
            expected_ok = expected_ok && opened;
        }
        if (dynamic_cast<LLColorSwatchCtrl*>(control))
        {
            std::set<LLFloater*> after;
            functionalVisibleFloaters(gFloaterView, after);
            bool opened = false;
            for (LLFloater* floater : after)
                if (!mFunctionalFloatersBefore.count(floater))
                {
                    opened = true;
                    floater->closeFloater(false);
                }
            mFunctionalObserved["effect"]["picker_opened"] = opened;
            bool setting_oracle = mFunctionalSetting != nullptr;
            if (setting_oracle && type == "Color3" && actual.isArray() && expected.isArray())
            {
                setting_oracle = actual.size() >= 3 && expected.size() >= 3;
                for (S32 channel = 0; setting_oracle && channel < 3; ++channel)
                    setting_oracle = functionalEqual(actual[channel], expected[channel], "F32");
            }
            else if (setting_oracle)
                setting_oracle = functionalEqual(actual, expected, type, entry["increment"]);
            mFunctionalObserved["effect"]["setting_oracle"] = setting_oracle;
            observable = opened && setting_oracle;
            expected_ok = expected_ok && opened && setting_oracle;
            if (!opened) assertion_failure = "color swatch click did not open a picker floater";
            else if (!setting_oracle) assertion_failure =
                "color swatch did not write the selected color to its bound setting";
        }
        if (entry["no_state_oracle"].asBoolean())
        {
            expected_ok = false;
            assertion_failure = "no independent state oracle; commit/owner/UI state cannot PASS";
        }
        if (entry["probe"].asString() == "launcher")
        {
            std::set<LLFloater*> after;
            functionalVisibleFloaters(gFloaterView, after);
            std::vector<LLFloater*> opened;
            for (LLFloater* floater : after)
                if (!mFunctionalFloatersBefore.count(floater)) opened.push_back(floater);
            const std::string wanted = entry["launcher_floater"].asString();
            const bool exact = opened.size() == 1 &&
                (wanted.empty() || opened.front()->getInstanceName() == wanted);
            mFunctionalObserved["launcher"]["new_count"] = static_cast<S32>(opened.size());
            mFunctionalObserved["launcher"]["expected_floater"] = wanted;
            if (!opened.empty()) mFunctionalObserved["launcher"]["actual_floater"] =
                opened.front()->getInstanceName();
            observable = exact;
            expected_ok = expected_ok && exact;
            if (!exact) assertion_failure = opened.empty() ? "launcher: no new floater" :
                "launcher: wrong floater " + opened.front()->getInstanceName();
        }
        if (ALKitSwitch* sw = dynamic_cast<ALKitSwitch*>(control))
        {
            const bool setting_value = mFunctionalSetting
                ? mFunctionalSetting->getValue().asBoolean() : control->getValue().asBoolean();
            bool switch_ok = sw->isOn() == control->getValue().asBoolean() &&
                sw->isOn() == setting_value && !sw->isFocusRingVisible() && !sw->hasMouseCapture();
            if (LLButton* button = findFirstView<LLButton>(sw))
                switch_ok = switch_ok && !button->isFocusRingVisible() && !button->hasMouseCapture();
            gFocusMgr.setKeyboardFocus(nullptr);
            if (LLFloater* director = mFloater.get()) director->setFocus(true);
            switch_ok = switch_ok && !sw->isFocusRingVisible();
            expected_ok = expected_ok && switch_ok;
            if (!switch_ok)
                assertion_failure = "switch setting/drawn state, mouse capture, or pointer/refocus ring disagrees";
        }
        else if (LLButton* button = dynamic_cast<LLButton*>(control))
        {
            // A mouse-clicked tab-stop button legitimately retains focus; only
            // keyboard-origin focus-ring policy belongs in a visual test.
            mFunctionalObserved["button"]["focus_ring"] = button->isFocusRingVisible();
            expected_ok = expected_ok && !button->hasMouseCapture();
        }
        const bool spinner_fits = functionalSpinnerTextFits(control);
        expected_ok = expected_ok && spinner_fits;
        if (!spinner_fits)
            assertion_failure = "spinner editor text is empty or its cached right edge does not fit";

        if (mDirectorV2)
        {
            ALKitPage* page = findFirstView<ALKitPage>(mFunctionalPageRoot);
            const std::string page_name = entry["page"].asString();
            std::string prefix;
            bool gate_expected = false;
            if (page_name == "light_rim_tab")
            {
                prefix = "light_rim";
                gate_expected = ALCineLightRigManager::instance().enabledMask() == 0;
            }
            else if (page_name == "weather_tab")
            {
                prefix = "weather";
                gate_expected = !gSavedSettings.getBOOL("AlchemyWeatherEnabled");
            }
            else if (page_name == "atmosphere_tab" || page_name == "cine_light_rig_tab")
            {
                prefix = page_name == "atmosphere_tab" ? "atmosphere" : "cine_light_rig";
                gate_expected = gPipeline.actorOnlyVolumetricLightCount() > 0;
            }
            if (page && !prefix.empty())
            {
                LLPanel* banner = page->findChild<LLPanel>(prefix + "_gate_banner", true);
                const bool banner_ok = banner && banner->getVisible() == gate_expected;
                expected_ok = expected_ok && banner_ok;
                if (!banner_ok)
                    assertion_failure = prefix + " banner visibility disagrees with its gate predicate";
                if (banner &&
                    (page_name == "atmosphere_tab" || page_name == "cine_light_rig_tab"))
                {
                    const U32 count = gPipeline.actorOnlyVolumetricLightCount();
                    LLTextBox* text = banner->findChild<LLTextBox>(prefix + "_gate_text", true);
                    const bool count_ok = (count == 0 && !banner->getVisible()) ||
                        (count > 0 && banner->getVisible() && text &&
                         text->getValue().asString().find(
                             llformat("%u light(s)", count)) != std::string::npos);
                    expected_ok = expected_ok && count_ok;
                    if (!count_ok)
                        assertion_failure = "masking banner count disagrees with actorOnlyVolumetricLightCount()";
                }
            }
        }
        if (LLFloater* host = mFunctionalHostFloater.get();
            host && host->getInstanceName() == "lightbox")
        {
            bool scroll_rects_inside = true;
            LLTabContainer* tabs = host->findChild<LLTabContainer>("lightbox_tab_container", true);
            std::function<void(LLView*)> inspect_tabs = [&](LLView* root)
            {
                if (!root) return;
                if (LLTabContainer* tab_container = dynamic_cast<LLTabContainer*>(root))
                {
                    for (S32 index = 0; index < tab_container->getTabCount(); ++index)
                    {
                        LLPanel* panel = tab_container->getPanelByIndex(index);
                        if (!panel) continue;
                        const LLRect panel_rect = panel->getLocalRect();
                        for (LLView* child : *panel->getChildList())
                            if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(child))
                                scroll_rects_inside = scroll_rects_inside &&
                                    panel_rect.contains(scroller->getRect());
                        inspect_tabs(panel);
                    }
                    return;
                }
                for (LLView* child : *root->getChildList()) inspect_tabs(child);
            };
            inspect_tabs(tabs);
            mFunctionalObserved["lightbox"]["scroll_rects_inside_tabs"] = scroll_rects_inside;
            expected_ok = expected_ok && scroll_rects_inside;
            if (!scroll_rects_inside)
                assertion_failure = "Lightbox tab scroll container extends outside its tab panel";
            ALFloaterLightBox* lightbox = dynamic_cast<ALFloaterLightBox*>(host);
            const bool reflow_stable = lightbox && lightbox->runReflowSelfTest();
            mFunctionalObserved["lightbox"]["hidden_row_reflow_stable"] = reflow_stable;
            expected_ok = expected_ok && reflow_stable;
            if (!reflow_stable)
                assertion_failure = "Lightbox hidden-row reflow drifted or did not restore exactly";
        }
        const std::string observed = "value=" + functionalValue(actual) +
            "; consumer=" + functionalValue(consumer_after) +
            "; handler=" + mFunctionalHandlerName;
        std::string reason;
        mFunctionalObserved["effect"]["external_delta"] = observable;
        if (!mFunctionalActionWorked) reason = mFunctionalLastClickReason.empty() ?
            "action could not be applied" : mFunctionalLastClickReason;
        else if (mFunctionalCommitCount == 0)
        {
            mFunctionalObserved["effect"]["category"] = "C";
            reason = "category C: click handled but no commit";
        }
        else if (!observable && mFunctionalHandlerName.empty())
        {
            mFunctionalObserved["effect"]["category"] = "D";
            reason = "category D: commit fired but owner is unresolved";
        }
        else if (!observable)
        {
            mFunctionalObserved["effect"]["category"] = "A";
            reason = "category A: commit fired but the external oracle did not move";
        }
        else if (!expected_ok) reason = assertion_failure.empty() ?
            "observed value/state did not match manifest" : assertion_failure;
        const bool known_gap = entry["known_gap"].asBoolean();
        if (known_gap && reason.empty()) reason = "known gap remains marked UNWIRED";
        addFunctionalResult(entry, known_gap ? "KNOWN_GAP" :
            (reason.empty() ? "PASS" : "FAIL"), reason, observed);
        restoreFunctionalAction();
        mFunctionalPending = false;
        ++mFunctionalIndex;
    }

    void writeFunctionalResults()
    {
        const std::string path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "functional.csv");
        llofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        out << "build,ui_version,inworld,page,control,kind,action,expected,observed,PASS/FAIL/SKIP,reason\n";
        const std::string build = LLVersionInfo::instance().getVersion();
        const std::string version = mDirectorV2 ? "v2" : "v1";
        for (const FunctionalResult& row : mFunctionalResults)
            out << csv(build) << ',' << csv(version) << ',' << (row.mInWorld ? "true" : "false")
                << ',' << csv(row.mPage) << ','
                << csv(row.mControl) << ',' << csv(row.mKind) << ',' << csv(row.mAction) << ','
                << csv(row.mExpected) << ',' << csv(row.mObserved) << ',' << csv(row.mStatus) << ','
                << csv(row.mReason) << '\n';
        out.flush();
        LL_INFOS("UIFunctionalSelfTest") << "UIFunctionalSelfTest summary complete: wrote "
            << mFunctionalResults.size() << " rows to " << path << LL_ENDL;
    }

    // ui-draw-lint: allow-function(setting) one-shot login-only safety snapshot
    void captureFunctionalRigState()
    {
        mFunctionalRigSnapshot = ALCineLightRigManager::instance().sceneData();
        if (LLControlVariable* shadows =
                gSavedSettings.getControl("BDMergeMaxSpotShadows").get())
        {
            mFunctionalShadowSnapshot = {
                shadows->get(), shadows->getSaveValue(), shadows->hasUnsavedValue()
            };
            mFunctionalHaveShadowSnapshot = true;
        }
    }

    // Restore the manager cache before settings saving resumes.  Calling the
    // real shutdown here is intentional: it exercises the later viewer-quit
    // path while saves are suppressed and proves that it persists the same
    // selected slot and all five original instance blobs.
    // ui-draw-lint: allow-function(setting,setting-write) one-shot login-only exact safety restore
    void restoreFunctionalRigState(bool report)
    {
        if (!mFunctionalRigSnapshot.isMap() || mFunctionalRigRestored) return;
        mFunctionalRigRestored = true;
        ALCineLightRigManager& manager = ALCineLightRigManager::instance();
        manager.applySceneData(mFunctionalRigSnapshot);
        manager.shutdown();
        if (mFunctionalHaveShadowSnapshot)
        {
            if (LLControlVariable* shadows =
                    gSavedSettings.getControl("BDMergeMaxSpotShadows").get())
            {
                shadows->setValue(mFunctionalShadowSnapshot.mSaved, true); // ui-draw-lint: allow(setting-write) exact self-test global restore
                if (mFunctionalShadowSnapshot.mUnsaved)
                    shadows->setValue(mFunctionalShadowSnapshot.mActive, false); // ui-draw-lint: allow(setting-write) exact self-test global restore
            }
        }
        const LLSD persisted = gSavedSettings.getLLSD("CineLightRigInstances");
        const bool exact = persisted["selected_slot"].asInteger() ==
                mFunctionalRigSnapshot["selected_slot"].asInteger() &&
            llsd_equals(persisted["instances"], mFunctionalRigSnapshot["instances"]);
        if (report)
        {
            LLSD assertion;
            assertion["page"] = "cine_light_rig_tab";
            assertion["widget_name"] = "rig_shutdown_restore";
            assertion["widget_type"] = "state";
            assertion["exercise"] = "shutdown";
            addFunctionalResult(assertion, exact ? "PASS" : "FAIL",
                exact ? "shutdown persisted the original selected slot and all five rig slots" :
                        "shutdown did not persist the original full rig-slot snapshot");
        }
    }

    static bool functionalAccountMatches(std::string value, std::string expected)
    {
        LLStringUtil::trim(value);
        LLStringUtil::toLower(value);
        LLStringUtil::trim(expected);
        LLStringUtil::toLower(expected);
        return value == expected || value == expected + " resident" ||
            value == expected + ".resident" || value == expected + "_resident";
    }

    void recordFunctionalAccountResult(const LLAvatarName& avatar_name,
                                       const std::string& status,
                                       const std::string& reason)
    {
        mFunctionalObserved = LLSD::emptyMap();
        mFunctionalObserved["account"]["user_name"] = avatar_name.getUserName();
        mFunctionalObserved["account"]["legacy_name"] = avatar_name.getLegacyName();
        LLSD account;
        account["page"] = "login";
        account["widget_name"] = "inworld_account";
        account["widget_type"] = "state";
        account["exercise"] = "saved authorized credential";
        addFunctionalResult(account, status, reason);
        mFunctionalObserved = LLSD::emptyMap();
    }

    void restoreFunctionalVCamState()
    {
        if (!mFunctionalVCamSnapshot.isMap() || mFunctionalVCamRestored) return;
        mFunctionalVCamRestored = true;
        std::string reason;
        const LLPrismLens::ApplyReport applied =
            LLPrismLens::applySceneData(mFunctionalVCamSnapshot, &reason);
        if (!applied)
            LL_WARNS("UIFunctionalSelfTest") << "VCam restore failed: "
                << reason << LL_ENDL;
        mFunctionalVCamModel.reset();
    }

    // ui-draw-lint: allow-function(raw-write) one-shot isolated in-world fixture setup
    bool prepareFunctionalInWorld()
    {
        if (mFunctionalInWorldPrepared) return true;
        mFunctionalVCamSnapshot = LLPrismLens::sceneData();
        LLSD empty_vcam = LLSD::emptyMap();
        empty_vcam["prism_captures"] = LLSD::emptyArray();
        empty_vcam["prism_displays"] = LLSD::emptyArray();
        empty_vcam["prism_projectors"] = LLSD::emptyArray();
        empty_vcam["prism_gates"] = LLSD::emptyArray();
        std::string reason;
        if (!LLPrismLens::applySceneData(empty_vcam, &reason))
        {
            LL_WARNS("UIFunctionalSelfTest") << "Unable to isolate test VCam: "
                << reason << LL_ENDL;
            return false;
        }
        mFunctionalVCamModel = std::make_unique<ALVCamPageModel>();
        if (mFunctionalVCamModel->addVirtualAtView(&reason) !=
            LLPrismLens::ERegistryResult::OK)
        {
            LL_WARNS("UIFunctionalSelfTest") << "Unable to add test VCam: "
                << reason << LL_ENDL;
            return false;
        }
        const LLPrismLens::CaptureHandle capture =
            mFunctionalVCamModel->selectedCapture();
        mFunctionalVCamModel->setLabel(capture, "Virtual at my view", &reason); // ui-draw-lint: allow(raw-write) test-only one-shot in-world setup
        LLVector3 display_pos;
        LLQuaternion display_rot;
        ALVCamPageModel::currentViewTransform(display_pos, display_rot);
        display_pos += LLVector3(0.f, 0.f, -3.f) * display_rot;
        if (mFunctionalVCamModel->addVirtualScreen(
                capture, display_pos, display_rot, 1.6f, .9f, &reason) !=
            LLPrismLens::ERegistryResult::OK)
        {
            LL_WARNS("UIFunctionalSelfTest") << "Unable to add test VCam display: "
                << reason << LL_ENDL;
            return false;
        }
        if (mFunctionalVCamModel->addVirtualAtView(&reason) !=
            LLPrismLens::ERegistryResult::OK)
        {
            LL_WARNS("UIFunctionalSelfTest") << "Unable to add alternate test VCam: "
                << reason << LL_ENDL;
            return false;
        }
        const LLPrismLens::CaptureHandle alternate =
            mFunctionalVCamModel->selectedCapture();
        mFunctionalVCamModel->setLabel(alternate, "Alternate virtual camera", &reason);
        mFunctionalVCamModel->setSelectedCapture(capture);
        mFunctionalVCamModel->refreshNow();

        LLVOVolume* nearest = nullptr;
        F32 nearest_distance = 20.f;
        for (S32 index = 0; index < gObjectList.getNumObjects(); ++index)
        {
            LLVOVolume* volume = dynamic_cast<LLVOVolume*>(
                gObjectList.getObject(index));
            if (!volume || volume->isDead() ||
                volume->getRegion() != gAgent.getRegion() ||
                !volume->getIsLight() || !volume->isLightSpotlight()) continue;
            const F32 distance =
                (volume->getPositionAgent() - gAgent.getPositionAgent()).length();
            if (distance <= nearest_distance)
            {
                nearest = volume;
                nearest_distance = distance;
            }
        }
        if (nearest)
        {
            LLPrismLens::ProjectorHandle binding;
            const ALVCamPageModel::ProjectorDropStatus status =
                ALVCamPageModel::projectorDropStatus(capture.mId, nearest);
            LLPrismLens::ERegistryResult result = status.mAction.mResult;
            if (status.mAlreadyBound)
            {
                binding = status.mExistingProjector;
                result = LLPrismLens::ERegistryResult::OK;
            }
            else if (status.mReassignment)
            {
                binding = status.mExistingProjector;
                result = mFunctionalVCamModel->reassignProjector(
                    binding, capture, &reason);
            }
            else
            {
                result = mFunctionalVCamModel->bindProjector(
                    capture, nearest, LLPrismLens::ProjectorSettings(),
                    &binding, &reason);
            }
            if (result == LLPrismLens::ERegistryResult::OK)
            {
                mFunctionalHaveProjector = true;
                mFunctionalVCamModel->setSelectedProjector(binding);
            }
        }
        mFunctionalInWorldPrepared = true;
        return true;
    }

    bool finishFunctionalInWorldFailure(const std::string& reason,
                                        const std::string& widget = "inworld_account",
                                        const std::string& exercise = "saved-credential auto-login")
    {
        LLSD failure;
        failure["page"] = "login";
        failure["widget_name"] = widget;
        failure["widget_type"] = "state";
        failure["exercise"] = exercise;
        addFunctionalResult(failure, "FAIL", reason);
        restoreFunctionalAction();
        restoreFunctionalVCamState();
        restore();
        restoreFunctionalRigState(false);
        restoreFunctionalUserFiles();
        writeFunctionalResults();
        sActive = false;
        sSuppressSettingsSave = false;
        // The wrapper converts this fresh FAIL row to a non-zero harness exit.
        // requestQuit deliberately uses the standard LogoutRequest/Reply path.
        LLAppViewer::instance()->requestQuit();
        return true;
    }

    bool functionalStep()
    {
        if (mInWorld && !mFunctionalWorldReady)
        {
            if (!mFunctionalAutoLoginStarted &&
                LLStartUp::getStartupState() == STATE_LOGIN_WAIT)
            {
                mFunctionalAutoLoginStarted = true;
                if (!gSavedSettings.getBOOL("AutoLogin")) // ui-draw-lint: allow(setting) test-only one-shot login check
                    return finishFunctionalInWorldFailure(
                        "saved-credential auto-login was not enabled");
                // Use the same saved-user lookup and field population as a
                // username-combo selection. No credential data leaves the
                // login panel, and failure cannot fall back to the last user.
                if (!LLPanelLogin::selectSavedUser(mFunctionalAccount))
                    return finishFunctionalInWorldFailure(
                        "authorized account not saved",
                        "inworld_account", "saved authorized credential");
                LLStartUp::setStartupState(STATE_LOGIN_CLEANUP);
            }
            if (LLStartUp::getStartupState() != STATE_STARTED)
            {
                if (mFunctionalLoginTimer.getElapsedTimeF32() >= 120.f)
                    return finishFunctionalInWorldFailure("login timeout (no retry)");
                return false;
            }

            // This is intentionally the first work performed on the first
            // STATE_STARTED tick. No floater, setting, or VCam is touched until
            // the logged-in account has been positively verified.
            if (!mFunctionalAccountVerified)
            {
                LLAvatarName avatar_name;
                if (!LLAvatarNameCache::get(gAgentID, &avatar_name))
                {
                    if (mFunctionalLoginTimer.getElapsedTimeF32() >= 120.f)
                        return finishFunctionalInWorldFailure(
                            "login timeout (account name unavailable; no retry)");
                    return false;
                }
                if (!functionalAccountMatches(avatar_name.getUserName(), mFunctionalAccount) &&
                    !functionalAccountMatches(avatar_name.getLegacyName(), mFunctionalAccount))
                {
                    recordFunctionalAccountResult(avatar_name, "FAIL", "wrong account");
                    restoreFunctionalAction();
                    restoreFunctionalVCamState();
                    restore();
                    restoreFunctionalRigState(false);
                    restoreFunctionalUserFiles();
                    writeFunctionalResults();
                    sActive = false;
                    // Keep settings saves suppressed: the wrong account's
                    // per-account folder may not be in the wrapper's backup.
                    LLAppViewer::instance()->requestQuit();
                    return true;
                }
                recordFunctionalAccountResult(
                    avatar_name, "PASS", "authorized account verified");
                mFunctionalAccountVerified = true;
                // AO off for the run: gaze turns can make the AO send simulator requests
                // (llactormover.cpp ~6317). Per-account settings files are byte-restored by
                // the wrapper after the viewer exits, so the user's AO setting comes back.
                if (gSavedPerAccountSettings.controlExists("AlchemyAOEnable")) // ui-draw-lint: allow(setting) test-only one-shot in-world setup
                    gSavedPerAccountSettings.setBOOL("AlchemyAOEnable", false); // ui-draw-lint: allow(setting-write) test-only one-shot in-world setup
            }

            if (mFunctionalWatchdog.getElapsedTimeF32() >=
                    static_cast<F32>(mFunctionalWatchdogSeconds))
                return finishFunctionalInWorldFailure(
                    "in-world wall-clock watchdog expired",
                    "inworld_watchdog", "bounded in-world run");

            const bool settled = gAgent.getRegion() && isAgentAvatarValid() &&
                gAgentAvatarp->isFullyLoaded();
            if (!settled)
            {
                mFunctionalWorldSettle.reset();
                mFunctionalWorldFrames = 0;
                return false;
            }
            ++mFunctionalWorldFrames;
            if (mFunctionalWorldFrames < 120 ||
                mFunctionalWorldSettle.getElapsedTimeF32() < 3.f) return false;
            mFunctionalWorldReady = true;
            mSettle.reset();
            if (!prepareFunctionalInWorld())
                return finishFunctionalInWorldFailure("in-world VCam setup failed");
        }
        if (mInWorld && mFunctionalWatchdog.getElapsedTimeF32() >=
                static_cast<F32>(mFunctionalWatchdogSeconds))
            return finishFunctionalInWorldFailure("in-world wall-clock watchdog expired",
                "inworld_watchdog", "bounded in-world run");

        FrameSample ignored;
        takeFrame(ignored);
        if (mSettle.getElapsedTimeF32() < 3.f) return false;
        if (mFunctionalFailureShotPending)
        {
            // The after-render capture hook may never fire (e.g. at the login screen):
            // give up after a bounded wait instead of stalling the whole run.
            if (mCaptureRequested && ++mFunctionalShotWaitFrames < 120) return false;
            if (mCaptureRequested)
            {
                LL_WARNS("UIFunctionalSelfTest") << "Failure screenshot not captured; skipping " << mCaptureFilename << LL_ENDL;
                mCaptureRequested = false;
                mCapturedImage = nullptr;
            }
            if (mCapturedImage.notNull()) saveCapturedImage();
            if (LLFloater* redirected = mFunctionalRedirectFloater.get()) redirected->closeFloater(false);
            mFunctionalRedirectFloater = LLHandle<LLFloater>();
            mFunctionalFailureShotPending = false;
            mFunctionalHostFloater = LLHandle<LLFloater>();
            return false;
        }
        if (!mFunctionalLoaded)
        {
            mFunctionalLoaded = true;
            if (!loadFunctionalManifest())
            {
                LLSD failure;
                failure["page"] = "manifest"; failure["widget_name"] = mFunctionalManifestPath;
                failure["widget_type"] = "file"; failure["exercise"] = "load";
                addFunctionalResult(failure, "FAIL", "manifest missing, invalid, or empty");
                mFunctionalIndex = mFunctionalEntries.size();
            }
            captureFunctionalRigState();
            LLFloater* floater = LLFloaterReg::showInstance("director", LLSD(), false);
            if (floater) mFloater = floater->getHandle();
            else
            {
                LLSD failure;
                failure["page"] = "director"; failure["widget_name"] = "director";
                failure["widget_type"] = "floater"; failure["exercise"] = "open";
                addFunctionalResult(failure, "FAIL", "director floater did not open");
                mFunctionalIndex = mFunctionalEntries.size();
            }
            return false;
        }
        if (mFunctionalPending) finishFunctionalEntry();
        else if (mFunctionalRevealPending) settleFunctionalReveal();
        else if (!mFunctionalSentinelDone) runFunctionalSentinel();
        else if (!mFunctionalGlobalAssertionsDone) runFunctionalKeyboardAssertion();
        else if (mFunctionalIndex < mFunctionalEntries.size()) beginFunctionalEntry();
        else
        {
            restoreFunctionalAction();
            finishFunctionalShaderBatch();
            restoreFunctionalVCamState();
            restore();
            restoreFunctionalRigState(true);
            restoreFunctionalUserFiles();
            writeFunctionalResults();
            sActive = false;
            sSuppressSettingsSave = false;
            LLAppViewer::instance()->requestQuit();
            return true;
        }
        return false;
    }

    enum Phase
    {
        WAIT_SETTLE,
        BASELINE,
        OPEN,
        FIRST_FRAME,
        DEFAULT_MEASURE,
        TAB_SELECT,
        TAB_FIRST_FRAME,
        TAB_MEASURE,
        DIRECTOR_HOVER_SWEEP,
        DIRECTOR_HOVER_CLEAR,
        SCROLL_BOTTOM,
        SCROLL_BOTTOM_MEASURE,
        SCROLL_TOP,
        SCROLL_TOP_MEASURE,
        INVENTORY_TOP,
        INVENTORY_TOP_MEASURE,
        INVENTORY_MIDDLE,
        INVENTORY_MIDDLE_MEASURE,
        INVENTORY_BOTTOM,
        INVENTORY_BOTTOM_MEASURE,
        INVENTORY_SCROLL_SWEEP,
        INVENTORY_FILTER,
        INVENTORY_FILTER_FIRST_FRAME,
        INVENTORY_FILTER_SETTLE,
        INVENTORY_FILTER_MEASURE,
        INVENTORY_HOVER_SWEEP,
        AUDIT_SIZE_PREP,
        AUDIT_SIZE_SETTLE,
        AUDIT_PAGE_SELECT,
        AUDIT_PAGE_SETTLE,
        AUDIT_CAPTURE_SETTLE,
        AUDIT_CAPTURE_WAIT,
        AUDIT_RESTORE,
        CLOSE,
        CLOSE_SETTLE,
        NEXT_PASS,
        FINISH
    };

    bool takeFrame(FrameSample& sample)
    {
        if (!sPendingUiCalls)
        {
            return false;
        }
        sample.mUiMs = sPendingUiMs;
        sample.mDrawn = sPendingDrawn;
        sample.mCulled = sPendingCulled;
        sample.mFindCalls = sPendingFindCalls;
        sample.mFindVisited = sPendingFindVisited;
        sPendingUiMs = 0.0;
        sPendingUiCalls = 0;
        sPendingDrawn = sPendingCulled = 0;
        sPendingFindCalls = sPendingFindVisited = 0;
        mUiFrameStall.reset();
        mUiFrameStallWarned = false;
        return true;
    }

    bool collect(U32 wanted)
    {
        FrameSample sample;
        if (!takeFrame(sample))
        {
            return false;
        }
        mSamples.push_back(sample);
        return mSamples.size() >= wanted;
    }

    bool collectInventoryTop(U32 wanted)
    {
        FrameSample sample;
        if (!takeFrame(sample))
        {
            return false;
        }
        if (sample.mDrawn < 20)
        {
            LL_WARNS("UIPerfSelfTest")
                << "UIPerfSelfTest ERROR abort inventory_stress: only "
                << sample.mDrawn << " views drawn at scroll_top; expected at least 20"
                << LL_ENDL;
            closeCurrentTarget();
            return false;
        }
        mSamples.push_back(sample);
        return mSamples.size() >= wanted;
    }

    bool inventoryReadyToMeasure(const char* measure)
    {
        InventoryStressFloater* stress = inventoryStressFloater();
        if (!stress)
        {
            abortCurrentTarget("synthetic floater died while settling");
            return false;
        }
        if (!stress->folderViewReady())
        {
            FrameSample ignored;
            takeFrame(ignored);
            if (++mReadyWaitFrames >= 600)
            {
                LL_WARNS("UIPerfSelfTest")
                    << "inventory_stress: " << measure
                    << " not ready after 600 frames; "
                    << stress->folderViewReadyStatus() << "; skipping target"
                    << LL_ENDL;
                closeCurrentTarget();
            }
            return false;
        }
        if (!mReadyWaitLogged)
        {
            LL_INFOS("UIPerfSelfTest")
                << "inventory_stress: " << measure << " settled after "
                << mReadyWaitFrames << " frame(s)" << LL_ENDL;
            mReadyWaitLogged = true;
        }
        return true;
    }

    void beginInventorySettle()
    {
        mReadyWaitFrames = 0;
        mReadyWaitLogged = false;
    }

    void addRow(const std::string& floater, const std::string& path,
                const std::string& measure, F64 action_ms = 0.0,
                F64 first_frame_ms = 0.0, U32 ready_wait_frames = 0)
    {
        ResultRow row;
        row.mPass = mCullOn
            ? (mABPass == 2 ? "folder_cull_on_2" : "folder_cull_on")
            : "folder_cull_off";
        row.mFloater = floater;
        row.mPath = path;
        row.mMeasure = measure;
        row.mFrames = (U32)mSamples.size();
        row.mActionMs = action_ms;
        row.mFirstFrameMs = first_frame_ms;
        row.mReadyWaitFrames = ready_wait_frames;
        if (!mSamples.empty())
        {
            std::vector<F64> times;
            times.reserve(mSamples.size());
            for (const FrameSample& sample : mSamples)
            {
                times.push_back(sample.mUiMs);
                row.mDrawn += sample.mDrawn;
                row.mCulled += sample.mCulled;
                row.mFindCalls += sample.mFindCalls;
                row.mFindVisited += sample.mFindVisited;
            }
            std::sort(times.begin(), times.end());
            row.mMean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
            row.mP95 = times[(times.size() - 1) * 95 / 100];
            row.mMax = times.back();
            row.mDrawn /= times.size();
            row.mCulled /= times.size();
            row.mFindCalls /= times.size();
            row.mFindVisited /= times.size();
        }
        mRows.push_back(row);
        mSamples.clear();
    }

    void addMetricRow(const std::string& path, const std::string& measure,
                      F64 milliseconds)
    {
        ResultRow row;
        row.mPass = mCullOn
            ? (mABPass == 2 ? "folder_cull_on_2" : "folder_cull_on")
            : "folder_cull_off";
        row.mFloater = "director";
        row.mPath = path;
        row.mMeasure = measure;
        row.mActionMs = milliseconds;
        mRows.push_back(row);
    }

    bool isDirectorV2() const
    {
        return mDirectorV2 && mTargetIndex >= 0 &&
            mTargetIndex < (S32)mTargets.size() &&
            mTargets[mTargetIndex] == "director";
    }

    static bool isRunnerOverride(const std::string& name)
    {
        static const std::set<std::string> overrides = {
            "UIPerfSelfTest", "UIPerfSelfTestFrames", "UIPerfInventoryStress",
            "UIPerfSelfTestAB", "UILayoutAudit", "UILayoutAuditSizes",
            "UIPerfDirectorV2", "DirectorUIV2"
        };
        return overrides.count(name) != 0;
    }

    bool isRestoredSetting(const std::string& name) const
    {
        if (mFunctional) return true;
        if (isRunnerOverride(name) || name == "UIFolderViewCull" ||
            mDirectorAuditSettings.count(name)) return true;
        static const std::string prefixes[] = {
            "DirectorV2", "CineLightRig", "CineRigRim",
            "RenderCineLensFlare", "RenderLensFlare",
            "BDMergeProjectorVolumetrics"
        };
        for (const std::string& prefix : prefixes)
            if (name.compare(0, prefix.size(), prefix) == 0) return true;
        return false;
    }

    void restoreSettingsSnapshot()
    {
        for (const auto& entry : mSettingsSnapshot)
        {
            if (!isRestoredSetting(entry.first)) continue;
            LLControlVariable* control = gSavedSettings.getControl(entry.first).get();
            if (!control) continue;
            const SavedControlState& saved = entry.second;
            const bool changed = !llsd_equals(control->get(), saved.mActive) ||
                                 !llsd_equals(control->getSaveValue(), saved.mSaved);
            if (!changed && !isRunnerOverride(entry.first)) continue;
            control->setValue(saved.mSaved, true); // ui-draw-lint: allow(setting-write) bounded self-test teardown restore
            if (saved.mUnsaved && !isRunnerOverride(entry.first))
                control->setValue(saved.mActive, false); // ui-draw-lint: allow(setting-write) bounded self-test teardown restore
        }
        LL_INFOS("UIPerfSelfTest")
            << "UIPerfSelfTest restored settings; Director V2 remains session-only"
            << LL_ENDL;
    }

    void collectKitMetrics()
    {
        if (!isDirectorV2() || mKitMetricsCollected) return;
        ALKitShell* shell = findFirstView<ALKitShell>(mFloater.get());
        if (!shell) return;

        ALKitSearch search;
        LLTimer index_timer;
        if (search.load("director_kit_index.xml"))
        {
            while (search.tokenizeBatch(400)) {}
        }
        addMetricRow("kit", "kit_index", index_timer.getElapsedTimeF64() * 1000.0);

        static const char* const queries[] = {
            "key yaw", "rim", "nd", "ev", "exposure",
            "actors only", "froxel", "zzz"
        };
        F64 worst_query_ms = 0.0;
        for (const char* query : queries)
        {
            LLTimer query_timer;
            const ALKitSearch::Hits hits = search.query(query);
            const F64 elapsed = query_timer.getElapsedTimeF64() * 1000.0;
            worst_query_ms = llmax(worst_query_ms, elapsed);
            LL_INFOS("DirectorKit") << "KitSearch query \"" << query << "\" "
                << elapsed << " ms " << hits.size() << LL_ENDL;
        }
        addMetricRow("kit", "kit_search", worst_query_ms);
        addMetricRow("kit", "kit_amber_storm",
                     ALKitSettingWatch::instance().debugRecomputeAll());
        mKitMetricsCollected = true;
    }

    void collectKitBuildMetrics(LLView* root)
    {
        if (!isDirectorV2() || !root) return;
        std::vector<ALKitLazyPage*> pages;
        collectViewsOfType<ALKitLazyPage>(root, pages);
        for (ALKitLazyPage* lazy : pages)
        {
            if (lazy->buildMs() < 0. || !mMeasuredKitBuilds.insert(lazy).second) continue;
            ALKitPage* page = findFirstView<ALKitPage>(lazy);
            const std::string path = page ? page->pageKey() : lazy->getName();
            addMetricRow(path, page && !page->legacy() ? "kit_build" : "kit_build_legacy",
                         lazy->buildMs());
        }
    }

    void prepareFloaterActions()
    {
        mTabs.clear();
        mDefaultScrollers.clear();
        mScrollers.clear();
        LLFloater* floater = mFloater.get();
        if (!floater)
        {
            return;
        }
        collectScrollers(floater, {}, "default", mDefaultScrollers);
        collectTabs(floater, {}, "", mTabs);
        for (TabTarget& tab : mTabs)
        {
            tab.mScrollBegin = mScrollers.size();
            collectScrollers(tab.mPanel.get(), tab.mChain, tab.mPath, mScrollers);
            tab.mScrollEnd = mScrollers.size();
        }
        mTabIndex = 0;
        mScrollIndex = 0;
        mInDefaultScroll = false;

        mInitialTabs.clear();
        std::set<LLTabContainer*> seen_tabs;
        for (const TabTarget& target : mTabs)
        {
            for (const auto& entry : target.mChain)
            {
                LLTabContainer* tabs = entry.first.get();
                if (tabs && seen_tabs.insert(tabs).second)
                {
                    mInitialTabs.emplace_back(
                        entry.first, tabs->getCurrentPanelIndex());
                }
            }
        }
        mInitialScroll.clear();
        std::vector<LLHandle<LLScrollContainer>> all_scrollers;
        collectAllScrollers(floater, all_scrollers);
        for (const LLHandle<LLScrollContainer>& handle : all_scrollers)
        {
            if (LLScrollContainer* scroller = handle.get())
            {
                mInitialScroll.emplace_back(
                    handle, scroller->getDocPosVertical());
            }
        }
        saveDirectorAuditSettings();
    }

    void restoreInitialState()
    {
        for (const auto& entry : mInitialTabs)
        {
            if (LLTabContainer* tabs = entry.first.get())
            {
                tabs->selectTab(entry.second);
            }
        }
        for (const auto& entry : mInitialScroll)
        {
            if (LLScrollContainer* scroller = entry.first.get())
            {
                scroller->setDocPosVertical(entry.second);
            }
        }
    }

    void saveDirectorAuditSettings()
    {
        if (!isDirectorV2() || !mDirectorAuditSettings.empty()) return;
        static const char* const names[] = {
            "DirectorV2PageState", "CineLightRigEasyMode",
            "DirectorV2SelectedLight", "CineLightRigEnabled",
            "CineRigRimEnabled", "DirectorV2LightMapView",
            "DirectorV2LightMapSideFrom", "CineLightRigKeyEV",
            "CineLightRigRimOn", "CineLightRigRimEV",
            "CineLightRigBgOn", "CineLightRigBgEV"
        };
        for (const char* name : names)
        {
            if (gSavedSettings.controlExists(name))
            {
                mDirectorAuditSettings[name] = gSavedSettings.getLLSD(name);
            }
        }
    }

    void restoreDirectorAuditSettings()
    {
        if (mDirectorAuditSettings.empty()) return;
        ALKitHistory::ScopedSuspend suspend;
        for (const auto& saved : mDirectorAuditSettings)
        {
            // ui-draw-lint: allow(setting-write) isolated Director self-test state
            if (LLControlVariable* control = gSavedSettings.getControl(saved.first).get())
                control->setValue(saved.second, false); // ui-draw-lint: allow(setting-write) bounded self-test state restore
        }
        if (ALKitPage* page = findFirstView<ALKitPage>(auditPageRoot()))
        {
            page->restoreState();
            const auto easy = mDirectorAuditSettings.find("CineLightRigEasyMode");
            if (easy != mDirectorAuditSettings.end())
            {
                const std::string wanted = easy->second.asBoolean() ? "Easy" : "Advanced";
                for (S32 i = 0; i < page->modeCount(); ++i)
                    if (page->modeName(i) == wanted) page->setModeIndex(i, false);
            }
            page->relayoutNow();
        }
    }

    void applyDirectorAuditDefaults()
    {
        if (mDirectorAuditSettings.empty()) return;
        ALKitHistory::ScopedSuspend suspend;
        for (const auto& saved : mDirectorAuditSettings)
        {
            LLControlVariable* control = gSavedSettings.getControl(saved.first).get();
            if (!control) continue;
            // ui-draw-lint: allow(setting-write) isolated Director self-test state
            control->setValue(control->getDefault(), false);
        }
        if (ALKitPage* page = findFirstView<ALKitPage>(auditPageRoot()))
        {
            page->restoreState();
            for (S32 i = 0; i < page->modeCount(); ++i)
                if (page->modeName(i) == "Easy") page->setModeIndex(i, false);
            page->relayoutNow();
        }
    }

    void prepareKitAuditStates()
    {
        mKitAuditStates.clear();
        mKitAuditStateIndex = 0;
        KitAuditState base;
        base.mKind = KitAuditState::BASE;
        base.mKey = "default";
        mKitAuditStates.push_back(base);
        if (!isDirectorV2() || mAuditPageIndex == 0) return;

        applyDirectorAuditDefaults();
        ALKitPage* page = mAuditPageIndex == 0 ? nullptr :
            findFirstView<ALKitPage>(auditPageRoot());
        if (!page || page->legacy()) return;
        auto inserted = mKitPageDefaults.emplace(page->pageKey(), KitPageDefaults());
        KitPageDefaults& defaults = inserted.first->second;
        if (inserted.second)
        {
            defaults.mMode = page->modeIndex();
            defaults.mChip = page->chipIndex();
            std::vector<ALKitCard*> default_cards;
            collectViewsOfType<ALKitCard>(page, default_cards);
            for (ALKitCard* card : default_cards)
                defaults.mCards.push_back({ card->getHandle(), card->bodyOpen(), card->moreOpen() });
        }
        mKitDefaultMode = defaults.mMode;
        mKitDefaultChip = defaults.mChip;
        mKitDefaultCards = defaults.mCards;
        for (S32 i = 0; i < page->modeCount(); ++i)
        {
            KitAuditState state;
            state.mKind = KitAuditState::MODE;
            state.mKey = "mode-" + safeComponent(page->modeName(i));
            state.mView = page->getHandle();
            state.mIndex = i;
            mKitAuditStates.push_back(state);
        }

        std::vector<LLButton*> buttons;
        collectViewsOfType<LLButton>(page, buttons);
        for (LLButton* button : buttons)
        {
            if (button->getName().find("_chip_") == std::string::npos) continue;
            KitAuditState state;
            state.mKind = KitAuditState::CHIP;
            state.mKey = "chip-" + safeComponent(button->getName());
            state.mView = button->getHandle();
            state.mIndex = atoi(button->getName().substr(
                button->getName().rfind("_chip_") + 6).c_str());
            mKitAuditStates.push_back(state);
        }

        std::vector<ALKitCard*> cards;
        collectViewsOfType<ALKitCard>(page, cards);
        for (ALKitCard* card : cards)
        {
            KitAuditState expanded;
            expanded.mKind = KitAuditState::CARD;
            expanded.mKey = "card-" + safeComponent(card->id()) + "-expanded";
            expanded.mView = card->getHandle();
            expanded.mBodyOpen = true;
            mKitAuditStates.push_back(expanded);

            KitAuditState closed = expanded;
            closed.mKey = "card-" + safeComponent(card->id()) + "-closed";
            closed.mBodyOpen = false;
            mKitAuditStates.push_back(closed);

            if (card->hasMore())
            {
                KitAuditState more = expanded;
                more.mKey = "card-" + safeComponent(card->id()) + "-more";
                more.mMoreOpen = true;
                mKitAuditStates.push_back(more);
            }
        }

        std::vector<ALKitOption*> options;
        collectViewsOfType<ALKitOption>(page, options);
        for (ALKitOption* option : options)
        {
            ALKitRow* row = findFirstView<ALKitRow>(option);
            if (!row) continue;
            KitAuditState state;
            state.mKind = KitAuditState::SELECTOR;
            state.mKey = "selector-" + safeComponent(option->getParent()->getName()) +
                "-" + safeComponent(option->optionLabel());
            state.mView = row->getHandle();
            mKitAuditStates.push_back(state);
        }

        ALKitLazyPage* lazy = dynamic_cast<ALKitLazyPage*>(auditPageRoot());
        if (lazy && !lazy->masterSetting().empty())
        {
            for (S32 enabled = 0; enabled <= 1; ++enabled)
            {
                KitAuditState state;
                state.mKind = KitAuditState::SETTING;
                state.mKey = std::string("master-") + (enabled ? "on" : "off");
                state.mSetting = lazy->masterSetting();
                state.mValue = enabled != 0;
                mKitAuditStates.push_back(state);
            }
        }
        if (page->pageKey() == "light_group/cine_light_rig_tab")
        {
            ALKitCard* map_card = page->findCard("light_map");
            for (S32 view = 0; view <= 2; ++view)
            {
                KitAuditState state;
                state.mKind = KitAuditState::SETTING;
                state.mKey = view == 0 ? "map-top" : view == 1 ? "map-side-left" : "map-2_5d";
                state.mSetting = "DirectorV2LightMapView";
                state.mValue = view;
                if (map_card) state.mView = map_card->getHandle();
                mKitAuditStates.push_back(state);
            }
            KitAuditState side_right;
            side_right.mKind = KitAuditState::SETTING;
            side_right.mKey = "map-side-right";
            side_right.mSetting = "DirectorV2LightMapSideFrom";
            side_right.mValue = 1;
            side_right.mIndex = 1;
            if (map_card) side_right.mView = map_card->getHandle();
            mKitAuditStates.push_back(side_right);
        }
        if (mKitAuditStates.size() > 256)
        {
            LL_WARNS("UILayoutAudit") << "Capping " << page->pageKey()
                << " audit states at 256 (enumerated " << mKitAuditStates.size() << ")"
                << LL_ENDL;
            mKitAuditStates.resize(256);
        }
    }

    bool applyKitAuditState()
    {
        if (!isDirectorV2()) return true;
        ALKitHistory::ScopedSuspend suspend;
        applyDirectorAuditDefaults();
        if (mKitAuditStateIndex >= mKitAuditStates.size()) return false;
        ALKitPage* page = findFirstView<ALKitPage>(auditPageRoot());
        if (page)
        {
            page->setModeIndex(mKitDefaultMode, false);
            page->setChipIndex(mKitDefaultChip);
            for (const KitCardDefault& saved : mKitDefaultCards)
                if (ALKitCard* card = dynamic_cast<ALKitCard*>(saved.mCard.get()))
                    card->restoreOpenState(saved.mBodyOpen, saved.mMoreOpen);
        }
        const KitAuditState& state = mKitAuditStates[mKitAuditStateIndex];
        bool applied = state.mKind == KitAuditState::BASE;
        if (state.mKind == KitAuditState::MODE)
        {
            if (ALKitPage* target = dynamic_cast<ALKitPage*>(state.mView.get()))
            {
                // A mode state is an intentional synthetic user choice. Using
                // PASSIVE lets page adapters preserve the current effective
                // mode, producing byte-identical Easy/Advanced captures.
                target->setModeIndex(state.mIndex, true);
                applied = target->modeIndex() == state.mIndex;
            }
        }
        else if (state.mKind == KitAuditState::CHIP)
        {
            applied = page && page->revealChip(state.mIndex) &&
                page->chipIndex() == state.mIndex;
        }
        else if (state.mKind == KitAuditState::CARD)
        {
            if (ALKitCard* card = dynamic_cast<ALKitCard*>(state.mView.get()))
            {
                if (ALKitPage* page = findFirstView<ALKitPage>(auditPageRoot()))
                    page->revealCard(card, false);
                card->restoreOpenState(state.mBodyOpen, state.mMoreOpen);
                applied = card->isInVisibleChain() &&
                    card->bodyOpen() == state.mBodyOpen && card->moreOpen() == state.mMoreOpen;
            }
        }
        else if (state.mKind == KitAuditState::SELECTOR)
        {
            ALKitRow* row = dynamic_cast<ALKitRow*>(state.mView.get());
            applied = page && row && page->revealRow(row, false) && row->isEffectivelyVisible();
        }
        else if (state.mKind == KitAuditState::SETTING)
        {
            if (state.mKey == "map-side-right")
            {
                // ui-draw-lint: allow(setting-write) isolated Director self-test state
                if (LLControlVariable* view = gSavedSettings.getControl("DirectorV2LightMapView").get()) // ui-draw-lint: allow(setting) bounded self-test state lookup
                    view->setValue(LLSD(1), false); // ui-draw-lint: allow(setting-write) bounded self-test state application
            }
            // ui-draw-lint: allow(setting-write) isolated Director self-test state
            if (LLControlVariable* control = gSavedSettings.getControl(state.mSetting).get())
            {
                control->setValue(state.mValue, false); // ui-draw-lint: allow(setting-write) bounded self-test state application
                applied = llsd_equals(control->get(), state.mValue);
            }
            if (ALKitCard* card = dynamic_cast<ALKitCard*>(state.mView.get()))
                applied = page && page->revealCard(card, false) && applied;
        }
        if (page)
            page->relayoutNow();
        if (LLView* target = state.mView.get())
            applied = target->isInVisibleChain() && applied;
        return applied;
    }

    std::string auditSizeName() const
    {
        if (mAuditSizeIndex >= mAuditSizes.size()) return "invalid";
        const AuditSize& size = mAuditSizes[mAuditSizeIndex];
        return size.mKind == AuditSize::MINIMUM ? "minimum" : size.mToken;
    }

    std::string auditPagePath() const
    {
        if (mAuditPageIndex == 0) return "default";
        const size_t tab_index = mAuditPageIndex - 1;
        return tab_index < mTabs.size() ? mTabs[tab_index].mPath : "invalid";
    }

    std::string auditStatePath() const
    {
        if (!isDirectorV2() || mKitAuditStates.empty() ||
            mKitAuditStateIndex >= mKitAuditStates.size()) return auditPagePath();
        return auditPagePath() + "@" + mKitAuditStates[mKitAuditStateIndex].mKey;
    }

    std::string auditStateName() const
    {
        return mKitAuditStates.empty() || mKitAuditStateIndex >= mKitAuditStates.size()
            ? "default" : mKitAuditStates[mKitAuditStateIndex].mKey;
    }

    LLView* auditPageRoot() const
    {
        if (mAuditPageIndex == 0) return mFloater.get();
        const size_t tab_index = mAuditPageIndex - 1;
        if (tab_index >= mTabs.size()) return nullptr;
        LLView* root = mTabs[tab_index].mPanel.get();
        if (isDirectorV2())
        {
            if (ALKitPage* page = findFirstView<ALKitPage>(root))
                if (page->legacy() && page->legacyHost()) return page->legacyHost();
        }
        return root;
    }

    bool selectAuditPage()
    {
        restoreInitialState();
        if (mAuditPageIndex == 0) return mFloater.get() != nullptr;
        const size_t tab_index = mAuditPageIndex - 1;
        return tab_index < mTabs.size() && applyChain(mTabs[tab_index].mChain);
    }

    void addFinding(const std::string& type, const AuditLeaf& a,
                    const AuditLeaf* b, const std::string& detail)
    {
        const std::string key = auditSizeName() + "|" + mTargets[mTargetIndex] +
            "|" + auditStatePath() + "|" + type + "|" + a.mPath + "|" +
            (b ? b->mPath : std::string()) + "|" + detail;
        if (!mFindingKeys.insert(key).second) return;
        LayoutFinding finding;
        finding.mSizePass = auditSizeName();
        finding.mFloater = mTargets[mTargetIndex];
        finding.mPageIndex = (U32)mAuditPageIndex;
        finding.mPath = auditStatePath();
        finding.mFinding = type;
        finding.mWidgetA = a.mPath;
        finding.mRectA = a.mRect;
        if (b)
        {
            finding.mWidgetB = b->mPath;
            finding.mRectB = b->mRect;
        }
        finding.mDetail = detail;
        mFindings.push_back(std::move(finding));
    }

    AuditLeaf semanticLeaf(LLView* view, LLView* root) const
    {
        AuditLeaf leaf;
        leaf.mView = view;
        leaf.mPath = view ? relativePath(view, root) : "<missing>";
        if (view)
            view->localRectToOtherView(view->getLocalRect(), &leaf.mRect, root);
        return leaf;
    }

    bool rectContained(const LLRect& inner, const LLRect& outer) const
    {
        return inner.mLeft >= outer.mLeft - 1 && inner.mRight <= outer.mRight + 1 && inner.mBottom >= outer.mBottom - 1 &&
               inner.mTop <= outer.mTop + 1;
    }

    void auditDirectorLiveLayout()
    {
        if (!isDirectorV2())
            return;
        LLFloater*  floater = mFloater.get();
        ALKitShell* shell   = findFirstView<ALKitShell>(floater);
        if (!floater || !shell)
            return;

        LLView*      page_root    = auditPageRoot();
        const LLRect floater_rect = floater->getLocalRect();
        if (page_root)
        {
            std::vector<LLView*>   all_views;
            std::vector<AuditLeaf> ignored;
            collectAuditViews(floater, floater, all_views, ignored);
            for (LLView* view : all_views)
            {
                if (view == floater || !visibleWithin(view, floater))
                    continue;
                if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(view);
                    editor && editor->hasSelection() && !editor->hasFocus())
                {
                    AuditLeaf leaf = semanticLeaf(view, floater);
                    addFinding("LINE_EDITOR_REST_SELECTION", leaf, nullptr,
                               "line editor has a selection without keyboard focus");
                }
                bool in_scroller = false;
                for (LLView* parent = view->getParent(); parent && parent != floater; parent = parent->getParent())
                    if (dynamic_cast<LLScrollContainer*>(parent))
                    {
                        in_scroller = true;
                        break;
                    }
                if (in_scroller)
                    continue;
                AuditLeaf leaf = semanticLeaf(view, floater);
                if (!rectContained(leaf.mRect, floater_rect))
                    addFinding("DIRECTOR_SPILL", leaf, nullptr, "visible descendant outside floater");
            }
        }

        if (LLPanel* roster = shell->roster(); roster && visibleWithin(roster, floater))
        {
            std::vector<LLView*>   roster_views;
            std::vector<AuditLeaf> ignored;
            collectAuditViews(roster, roster, roster_views, ignored);
            const LLRect roster_rect = roster->getLocalRect();
            for (LLView* view : roster_views)
            {
                if (view == roster || !visibleWithin(view, roster))
                    continue;
                AuditLeaf leaf = semanticLeaf(view, roster);
                if (!rectContained(leaf.mRect, roster_rect))
                    addFinding("ROSTER_SPILL", leaf, nullptr, "visible roster descendant outside roster");
            }
        }
        LLButton* cast_chip = shell->findChild<LLButton>("btn_cast_chip"); // ui-draw-lint: allow(lookup) one-shot live layout audit
        const bool roster_reachable = (shell->roster() && visibleWithin(shell->roster(), floater)) ||
                                      (cast_chip && visibleWithin(cast_chip, floater));
        if (!roster_reachable)
            addFinding("CAST_UNREACHABLE", semanticLeaf(cast_chip ? static_cast<LLView*>(cast_chip) : shell, floater), nullptr,
                       "neither the rail roster nor its Cast popover chip is reachable");

        if (mAuditPageIndex > 0)
        {
            LLView*        lazy_root = mAuditPageIndex - 1 < mTabs.size() ? mTabs[mAuditPageIndex - 1].mPanel.get() : page_root;
            ALKitLazyPage* lazy      = dynamic_cast<ALKitLazyPage*>(lazy_root);
            if (!lazy)
                return;
            ALKitPage*     page      = findFirstView<ALKitPage>(lazy_root);
            AuditLeaf      leaf      = semanticLeaf(lazy, floater);
            if (!page || !page->hasAncestor(lazy) || !visibleWithin(page, lazy) || shell->findBuiltPage(page->pageKey()) != page)
                addFinding("LAZY_PAGE_UNATTACHED", leaf, nullptr, "built page is not attached and visible");
            else
            {
                const LLRect& host    = lazy->getLocalRect();
                const LLRect& content = page->getRect();
                if (content.getWidth() != host.getWidth() || content.getHeight() != host.getHeight() || content.mLeft != 0 ||
                    content.mBottom != 0)
                    addFinding("LAZY_PAGE_UNSIZED", semanticLeaf(page, lazy), nullptr, "kit page does not fill lazy host");
                std::vector<ALKitCard*> cards;
                collectViewsOfType<ALKitCard>(page, cards);
                ALKitScroller* scroller = page->scroller();
                const bool visible_card = std::any_of(cards.begin(), cards.end(), [page, scroller](ALKitCard* card)
                {
                    if (!scroller || !visibleWithin(card, page))
                        return false;
                    LLRect card_rect;
                    if (!card->localRectToOtherView(card->getLocalRect(), &card_rect, scroller))
                        return false;
                    const LLRect visible = scroller->getVisibleContentRect();
                    const S32 overlap_w = llmin(card_rect.mRight, visible.mRight) - llmax(card_rect.mLeft, visible.mLeft);
                    const S32 overlap_h = llmin(card_rect.mTop, visible.mTop) - llmax(card_rect.mBottom, visible.mBottom);
                    return overlap_w > 0 && overlap_h >= 40;
                });
                if (!page->legacy() && !visible_card)
                    addFinding("LAZY_PAGE_NO_CARD", semanticLeaf(page, floater), nullptr, "built page has no visible card");
                if (page->legacy())
                {
                    LLPanel* host = page->legacyHost();
                    const S32 minimum = llmax(40, page->getRect().getHeight() - ALKitStyle::CHIPS_H - ALKitStyle::WHO_H -
                                                     ALKitStyle::BAR_H - ALKitStyle::BANNER_H);
                    if (!host || host->getRect().getHeight() < minimum)
                        addFinding("LEGACY_BODY_COLLAPSED", semanticLeaf(host ? static_cast<LLView*>(host) : page, floater), nullptr,
                                   "legacy body does not fill the page below its chrome");
                    bool visible_leaf = false;
                    if (host)
                    {
                        std::vector<LLView*> host_views;
                        std::vector<AuditLeaf> ignored;
                        collectAuditViews(host, host, host_views, ignored);
                        for (LLView* view : host_views)
                        {
                            if (view == host || !view->getChildList()->empty() || !visibleWithin(view, host))
                                continue;
                            LLRect rect;
                            if (view->localRectToOtherView(view->getLocalRect(), &rect, host))
                            {
                                const S32 overlap_h = llmin(rect.mTop, host->getLocalRect().mTop) -
                                                        llmax(rect.mBottom, host->getLocalRect().mBottom);
                                if (overlap_h >= 20)
                                {
                                    visible_leaf = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!visible_leaf)
                        addFinding("LEGACY_BODY_UNBOUND", semanticLeaf(host ? static_cast<LLView*>(host) : page, floater), nullptr,
                                   "legacy body has no visible bound leaf");
                }
                LLPanel* bar = page->pageBar();
                std::vector<LLView*> chrome = { page->resetButton(), page->undoButton(), page->redoButton() };
                for (LLView* widget : chrome)
                {
                    if (!widget || !bar || !visibleWithin(widget, bar) || !rectContained(widget->getRect(), bar->getLocalRect()))
                        addFinding("PAGE_CHROME", semanticLeaf(widget ? widget : page, floater), nullptr,
                                   "page-bar control is missing or outside the page bar");
                }
                if (page->hasMaster())
                {
                    LLRect master_rect;
                    const bool master_ok = page->masterSwitch() && bar &&
                        visibleWithin(page->masterSwitch(), bar) &&
                        page->masterSwitch()->localRectToOtherView(page->masterSwitch()->getLocalRect(), &master_rect, bar) &&
                        rectContained(master_rect, bar->getLocalRect());
                    if (!master_ok)
                        addFinding("PAGE_CHROME", semanticLeaf(page->masterSwitch() ? static_cast<LLView*>(page->masterSwitch()) : page,
                                                               floater), nullptr,
                                   "master switch is missing or outside the page bar");
                }
                if (page->hasPreset())
                {
                    bool preset_in_bar = false;
                    if (bar)
                        for (LLView* child : *bar->getChildList())
                            if (dynamic_cast<LLComboBox*>(child) && visibleWithin(child, bar) &&
                                rectContained(child->getRect(), bar->getLocalRect()))
                            {
                                preset_in_bar = true;
                                break;
                            }
                    if (!preset_in_bar)
                        addFinding("PAGE_CHROME", semanticLeaf(bar ? static_cast<LLView*>(bar) : page, floater), nullptr,
                                   "preset combo is missing from the page bar");
                }
                if (page->whoChip() && (!page->whoBand() || !rectContained(page->whoChip()->getRect(), page->whoBand()->getLocalRect())))
                    addFinding("PAGE_CHROME", semanticLeaf(page->whoChip(), floater), nullptr, "who chip is outside the who band");
                if (!shell->help().hasContent())
                    addFinding("HELP_EMPTY", semanticLeaf(page, floater), nullptr, "page selection did not seed help title and body");

                std::vector<ALKitScroller*> scrollers;
                collectViewsOfType<ALKitScroller>(page, scrollers);
                for (ALKitScroller* scroller : scrollers)
                {
                    AuditLeaf scroller_leaf = semanticLeaf(scroller, floater);
                    if (scroller->getVisibleContentRect().getHeight() <= 0)
                        addFinding("KIT_SCROLLER_EMPTY", scroller_leaf, nullptr, "non-positive viewport height");
                    if (!rectContained(scroller_leaf.mRect, floater_rect))
                        addFinding("KIT_SCROLLER_SPILL", scroller_leaf, nullptr, "scroller outside floater");
                    LLView* document = scroller->getScrolledView();
                    if (!document || document->getRect().getWidth() > scroller->getVisibleContentRect().getWidth() + 1)
                        addFinding("KIT_HORIZONTAL_SCROLL", scroller_leaf, nullptr, "document is wider than the scroller viewport");
                }
            }
        }

        if ((shell->tabPrevious() && shell->tabPrevious()->getVisible()) ||
            (shell->tabNext() && shell->tabNext()->getVisible()))
            addFinding("TAB_OVERFLOW", semanticLeaf(shell->tabStrip(), floater), nullptr,
                       "approved Director tabs require overflow arrows");
        for (LLButton* tab : shell->tabButtons())
        {
            if (!tab || !tab->getVisible())
                continue;
            if (tab->getFont() && tab->getFont()->getWidth(tab->getCurrentLabel()) + tab->getLeftHPad() + tab->getRightHPad() >
                                      tab->getRect().getWidth() + 1)
                addFinding("TAB_LABEL_CLIPPED", semanticLeaf(tab, floater), nullptr, "displayed tab label is clipped");
            if (shell->tabStrip() && !rectContained(tab->getRect(), shell->tabStrip()->getLocalRect()))
                addFinding("TAB_OVERFLOW", semanticLeaf(tab, floater), nullptr, "visible tab leaves the tab strip");
        }
        if (mAuditPageIndex != 0)
            return;
        static const std::vector<std::string> approved_groups = { "cast_group",  "performance_group", "camera_group", "lens_group",
                                                                  "light_group", "look_group",        "world_group",  "capture_group" };
        if (shell->railGroups() != approved_groups)
            addFinding("RAIL_GROUPS", semanticLeaf(shell, floater), nullptr, "rail group list differs from approved eight groups");
        for (LLButton* button : shell->railButtons())
        {
            if (!button || !button->getFont() || button->getText().empty() || button->getImageOverlay().isNull())
                continue;
            const S32 image_width  = button->getImageOverlay()->getWidth();
            const S32 image_height = button->getImageOverlay()->getHeight();
            const S32 text_width = button->getFont()->getWidth(button->getText());
            S32       image_left = button->getImageOverlayHAlign() == LLFontGL::LEFT ? button->getLeftHPad()
                                                                                     : (button->getRect().getWidth() - image_width) / 2;
            S32       text_left  = button->getLeftHPad();
            S32       text_area  = button->getRect().getWidth() - button->getLeftHPad() - button->getRightHPad();
            if (button->getImageOverlayHAlign() == LLFontGL::LEFT)
            {
                text_left += image_width + 4;
                text_area -= image_width + 4;
            }
            if (button->getHAlign() == LLFontGL::HCENTER)
                text_left += (text_area - text_width) / 2;
            else if (button->getHAlign() == LLFontGL::RIGHT)
                text_left += text_area - text_width;
            const LLRect icon_rect(image_left, image_height, image_left + image_width, 0);
            const LLRect label_rect(text_left, button->getRect().getHeight(), text_left + text_width, 0);
            if (icon_rect.overlaps(label_rect))
                addFinding("RAIL_ICON_LABEL_OVERLAP", semanticLeaf(button, floater), nullptr,
                           "rendered icon and label rectangles intersect");
        }

        const S32 width = floater->getRect().getWidth();
        if (width != 850 && width != 900 && width != 1160)
            return;
        ALKitBar* bar = dynamic_cast<ALKitBar*>(shell->transport());
        if (!bar)
            return;
        LLButton*  action         = nullptr;
        LLButton*  cut            = nullptr;
        LLView*     timecode       = nullptr;
        LLView*     status         = nullptr;
        for (LLView* child : *bar->getChildList())
        {
            if (child->getName() == "btn_action") action = dynamic_cast<LLButton*>(child);
            if (child->getName() == "btn_cut") cut = dynamic_cast<LLButton*>(child);
            if (child->getName() == "take_timecode") timecode = child;
            if (child->getName() == "status_strip") status = child;
        }
        if (!timecode || !timecode->getVisible())
            addFinding("TRANSPORT_FLEX_MIN", semanticLeaf(timecode ? timecode : bar, floater), nullptr,
                       "timecode must remain visible at every approved size");
        if (status && status->getVisible() && status->getRect().getWidth() < 120)
            addFinding("TRANSPORT_FLEX_MIN", semanticLeaf(status, floater), nullptr,
                       "visible status text is narrower than 120 px");
        const bool action_visible = action && action->getVisible();
        const bool cut_visible    = cut && cut->getVisible();
        for (S32 recording = 0; recording < 2; ++recording)
        {
            if (action)
                action->setVisible(!recording);
            if (cut)
                cut->setVisible(recording);
            bar->reshape(bar->getRect().getWidth(), bar->getRect().getHeight(), true);
            std::vector<LLView*> direct;
            for (LLView* child : *bar->getChildList())
                if (child->getVisible())
                    direct.push_back(child);
            std::sort(direct.begin(), direct.end(), [](LLView* a, LLView* b) { return a->getRect().mLeft < b->getRect().mLeft; });
            S32                                     previous_right   = bar->getLocalRect().mLeft;
            S32                                     previous_rank    = -1;
            static const std::map<std::string, S32> declaration_rank = { { "btn_action", 0 },     { "btn_cut", 0 },
                                                                         { "btn_arm", 1 },        { "btn_take_play", 2 },
                                                                         { "btn_take_stop", 3 },  { "take_timecode", 4 },
                                                                         { "scene_combo", 5 },    { "scene_name_editor", 5 },
                                                                         { "btn_scene_load", 6 }, { "btn_scene_save", 7 },
                                                                         { "btn_scene_delete", 8 }, { "btn_set_marks", 9 },
                                                                         { "btn_reset_marks", 10 }, { "rec_indicator", 11 },
                                                                         { "show_paths_global_check", 12 }, { "btn_help", 13 },
                                                                         { "status_strip", 14 }, { "btn_cast_chip", 15 },
                                                                         { "kit_ui_ms", 16 }, { "btn_hotkeys", 17 } };
            for (LLView* child : direct)
            {
                AuditLeaf item = semanticLeaf(child, floater);
                if (!rectContained(child->getRect(), bar->getLocalRect()))
                    addFinding("TRANSPORT_SPILL", item, nullptr, recording ? "record state" : "idle state");
                if (child->getRect().mLeft < previous_right)
                    addFinding("TRANSPORT_OVERLAP", item, nullptr, recording ? "record state" : "idle state");
                previous_right  = child->getRect().mRight;
                const auto rank = declaration_rank.find(child->getName());
                if (rank != declaration_rank.end())
                {
                    if (rank->second < previous_rank)
                        addFinding("TRANSPORT_ORDER", item, nullptr, recording ? "record state" : "idle state");
                    previous_rank = rank->second;
                }
                if (LLButton* button = dynamic_cast<LLButton*>(child);
                    button && button->getFont() && !button->getText().empty() &&
                    button->getFont()->getWidth(button->getText()) + button->getLeftHPad() + button->getRightHPad() >
                        button->getRect().getWidth() + 1)
                    addFinding("TRANSPORT_LABEL_CLIPPED", item, nullptr, recording ? "record state" : "idle state");
            }
        }
        if (action)
            action->setVisible(action_visible);
        if (cut)
            cut->setVisible(cut_visible);
        bar->reshape(bar->getRect().getWidth(), bar->getRect().getHeight(), true);
    }

    void auditKitTextGeometry(LLFloater* floater)
    {
        static const bool fixture_ok =
            substantialIntersection(LLRect(0, 14, 40, 0), LLRect(30, 14, 80, 0)) &&
            !substantialIntersection(LLRect(0, 14, 40, 0), LLRect(38, 14, 80, 0));
        if (!fixture_ok)
        {
            AuditLeaf fixture = semanticLeaf(floater, floater);
            addFinding("TEXT_AUDIT_CALIBRATION", fixture, nullptr,
                       "fixture expected Rig light-map key/hint overprint only above the 2 px threshold");
        }
        auto in_floater = [floater](LLView* view)
        {
            return view && (view == floater || view->hasAncestor(floater));
        };
        auto text_leaf = [this, floater](const KitTextRun& run)
        {
            AuditLeaf leaf = semanticLeaf(run.mView.get(), floater);
            floater->screenRectToLocal(run.mRect, &leaf.mRect);
            return leaf;
        };
        for (size_t i = 0; i < sKitTextRuns.size(); ++i)
        {
            LLView* first = sKitTextRuns[i].mView.get();
            if (!in_floater(first) || sKitTextRuns[i].mText.empty())
                continue;
            for (size_t j = i + 1; j < sKitTextRuns.size(); ++j)
            {
                LLView* second = sKitTextRuns[j].mView.get();
                if (!in_floater(second) || first == second ||
                    !substantialIntersection(sKitTextRuns[i].mRect, sKitTextRuns[j].mRect))
                    continue;
                AuditLeaf a = text_leaf(sKitTextRuns[i]);
                AuditLeaf b = text_leaf(sKitTextRuns[j]);
                addFinding("TEXT_OVERLAP", a, &b,
                           "drawn strings intersect: " + sKitTextRuns[i].mText + " / " + sKitTextRuns[j].mText);
            }

            for (const KitInteractiveRect& interactive : sKitInteractiveRects)
            {
                LLView* control = interactive.mView.get();
                LLView* owner   = sKitTextRuns[i].mOwner.get();
                if (!in_floater(control) || control == owner || control == first ||
                    (owner && (owner->hasAncestor(control) || control->hasAncestor(owner))) ||
                    first->hasAncestor(control) || control->hasAncestor(first) ||
                    !substantialIntersection(sKitTextRuns[i].mRect, interactive.mRect))
                    continue;
                AuditLeaf a = text_leaf(sKitTextRuns[i]);
                AuditLeaf b = semanticLeaf(control, floater);
                floater->screenRectToLocal(interactive.mRect, &b.mRect);
                addFinding("TEXT_ON_CONTROL", a, &b,
                           "drawn text intersects an unrelated interactive control: " + sKitTextRuns[i].mText);
            }
        }
    }

    void auditGeometry()
    {
        LLView* page = auditPageRoot();
        LLFloater* floater = mFloater.get();
        if (!page || !floater) return;
        mAuditedPages.insert(auditSizeName() + " " + mTargets[mTargetIndex] +
                             "/" + auditStatePath());
        const std::string coverage_key = auditSizeName() + "|" +
            mTargets[mTargetIndex] + "|" + auditStatePath();
        if (isDirectorV2() && mAuditCoverageKeys.insert(coverage_key).second)
        {
            mAuditCoverage.push_back({ auditSizeName(), mTargets[mTargetIndex],
                                      (U32)mAuditPageIndex, auditStatePath(),
                                      (U32)mKitAuditStates.size(), mAuditStateApplied });
        }
        if (isDirectorV2() && !mAuditStateApplied) return;

        auditDirectorLiveLayout();
        if (isDirectorV2())
            auditKitTextGeometry(floater);

        std::vector<LLView*> views;
        std::vector<AuditLeaf> leaves;
        collectAuditViews(page, page, views, leaves);

        if (isDirectorV2())
        {
            std::vector<ALKitRow*> rows;
            collectViewsOfType<ALKitRow>(page, rows);
            for (ALKitRow* row : rows)
            {
                if (!visibleWithin(row, page))
                    continue;
                LLView* value = nullptr;
                if (LLUICtrl* control = row->control())
                    for (LLView* child : *control->getChildList())
                        if (dynamic_cast<LLLineEditor*>(child) || dynamic_cast<LLTextBox*>(child))
                        {
                            value = child;
                            break;
                        }
                if (!value)
                    value = row->valueView();
                if (!value || !visibleWithin(value, row))
                    continue;
                LLRect value_rect;
                if (value->localRectToOtherView(value->getLocalRect(), &value_rect, row) &&
                    !rectContained(value_rect, row->getLocalRect()))
                {
                    AuditLeaf item = semanticLeaf(value, floater);
                    addFinding("VALUE_OUTSIDE_ROW", item, nullptr,
                               "value rect lies outside its 28 px owner row");
                }
            }
        }
        const LLRect floater_local = floater->getLocalRect();
        for (AuditLeaf& leaf : leaves)
        {
            leaf.mContainingScroller = containingScroller(leaf.mView, floater);
        }

        for (const AuditLeaf& leaf : leaves)
        {
            LLView* parent = leaf.mView->getParent();
            const bool tab_button = dynamic_cast<LLTabContainer*>(parent) != nullptr;
            if (tab_button)
            {
                LLButton* button = dynamic_cast<LLButton*>(leaf.mView);
                if (button && button->getFont() && !button->getText().empty())
                {
                    const S32 required = button->getFont()->getWidth(button->getText()) +
                        button->getLeftHPad() + button->getRightHPad();
                    if (required > button->getRect().getWidth() + 1)
                    {
                        addFinding("TEXT_TRUNCATED", leaf, nullptr,
                            llformat("label+padding=%d available=%d", required,
                                     button->getRect().getWidth()));
                    }
                }
                continue;
            }
            if (parent && !isRealCompositeControl(parent))
            {
                const LLRect& rect = leaf.mView->getRect();
                const LLRect bounds = parent->getLocalRect();
                if (rect.mLeft < bounds.mLeft - 1 || rect.mRight > bounds.mRight + 1)
                {
                    addFinding("CLIPPED_H", leaf, nullptr,
                        "outside parent " + parent->getName());
                }
                if (rect.mBottom < bounds.mBottom - 1 || rect.mTop > bounds.mTop + 1)
                {
                    addFinding("CLIPPED_V", leaf, nullptr,
                        "outside parent " + parent->getName());
                }
            }

            LLScrollContainer* scroller = leaf.mContainingScroller;
            if (scroller && scroller->getScrolledView())
            {
                LLRect in_document;
                leaf.mView->localRectToOtherView(leaf.mView->getLocalRect(),
                                                  &in_document,
                                                  scroller->getScrolledView());
                const LLRect document = scroller->getScrolledView()->getLocalRect();
                if (in_document.mLeft < document.mLeft - 1 ||
                    in_document.mRight > document.mRight + 1)
                {
                    addFinding("CLIPPED_H", leaf, nullptr, "outside scroll document");
                }
                if (in_document.mBottom < document.mBottom - 1 ||
                    in_document.mTop > document.mTop + 1)
                {
                    addFinding("CLIPPED_V", leaf, nullptr, "outside scroll document");
                }
            }
            else
            {
                LLRect in_floater;
                leaf.mView->localRectToOtherView(leaf.mView->getLocalRect(),
                                                  &in_floater, floater);
                if (in_floater.mLeft < floater_local.mLeft - 1 ||
                    in_floater.mRight > floater_local.mRight + 1)
                {
                    addFinding("CLIPPED_H", leaf, nullptr, "outside floater client");
                }
                if (in_floater.mBottom < floater_local.mBottom - 1 ||
                    in_floater.mTop > floater_local.mTop + 1)
                {
                    addFinding("CLIPPED_V", leaf, nullptr, "outside floater client");
                }
            }

            LLRect in_floater;
            leaf.mView->localRectToOtherView(leaf.mView->getLocalRect(),
                                              &in_floater, floater);
            bool horizontal_scroll = false;
            bool vertical_scroll = false;
            containingScrollAxes(leaf.mView, floater,
                                 horizontal_scroll, vertical_scroll);
            if ((!horizontal_scroll && in_floater.mRight > floater_local.mRight + 1) ||
                (!vertical_scroll && in_floater.mBottom < floater_local.mBottom - 1))
            {
                addFinding("OFF_FLOATER", leaf, nullptr,
                    "right/bottom beyond floater without matching scroll axis");
            }

            if (LLTextBox* text = dynamic_cast<LLTextBox*>(leaf.mView))
            {
                if (!text->getWordWrap() && text->getFont() && !text->getText().empty())
                {
                    const S32 width = longestLineWidth(text->getFont(), text->getText());
                    if (width > text->getRect().getWidth() + 1)
                    {
                        addFinding("TEXT_TRUNCATED", leaf, nullptr,
                            llformat("text=%d available=%d", width,
                                     text->getRect().getWidth()));
                    }
                }
            }
            if (LLButton* button = dynamic_cast<LLButton*>(leaf.mView);
                button && !dynamic_cast<LLComboBox*>(parent))
            {
                const std::string label = button->getText();
                if (button->getFont() && !label.empty())
                {
                    const S32 required = button->getFont()->getWidth(label) +
                        button->getLeftHPad() + button->getRightHPad();
                    if (required > button->getRect().getWidth() + 1)
                    {
                        addFinding("TEXT_TRUNCATED", leaf, nullptr,
                            llformat("label+padding=%d available=%d", required,
                                     button->getRect().getWidth()));
                    }
                }
            }
            if (dynamic_cast<LLSlider*>(leaf.mView) && leaf.mRect.getWidth() < 80)
            {
                addFinding("TOO_NARROW", leaf, nullptr,
                    llformat("slider track=%d minimum=80", leaf.mRect.getWidth()));
            }
            if (dynamic_cast<LLLineEditor*>(leaf.mView) && leaf.mRect.getWidth() < 40 &&
                !dynamic_cast<LLSliderCtrl*>(parent) &&
                !dynamic_cast<LLSpinCtrl*>(parent) &&
                !dynamic_cast<LLMultiSliderCtrl*>(parent) &&
                !dynamic_cast<LLComboBox*>(parent))
            {
                addFinding("TOO_NARROW", leaf, nullptr,
                    llformat("line editor=%d minimum=40", leaf.mRect.getWidth()));
            }
        }

        for (LLView* view : views)
        {
            if (LLComboBox* combo = dynamic_cast<LLComboBox*>(view))
            {
                if (combo->getRect().getWidth() < 40)
                {
                    AuditLeaf item;
                    item.mView = combo;
                    item.mPath = relativePath(combo, page);
                    combo->localRectToOtherView(combo->getLocalRect(), &item.mRect, page);
                    addFinding("TOO_NARROW", item, nullptr,
                        llformat("combo=%d minimum=40", combo->getRect().getWidth()));
                }
                const std::string label = combo->getSelectedItemLabel();
                const LLFontGL* font = nullptr;
                for (LLView* child : *combo->getChildList())
                {
                    if (child->getFont()) { font = child->getFont(); break; }
                }
                if (font && !label.empty())
                {
                    const S32 required = font->getWidth(label) + 24;
                    if (required > combo->getRect().getWidth() + 1)
                    {
                        AuditLeaf item;
                        item.mView = combo;
                        item.mPath = relativePath(combo, page);
                        combo->localRectToOtherView(combo->getLocalRect(), &item.mRect, page);
                        addFinding("TEXT_TRUNCATED", item, nullptr,
                            llformat("selected text+arrow=%d available=%d", required,
                                     combo->getRect().getWidth()));
                    }
                }
            }
            if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(view))
            {
                if (spinner->getRect().getWidth() < 40)
                {
                    AuditLeaf item;
                    item.mView = spinner;
                    item.mPath = relativePath(spinner, page);
                    spinner->localRectToOtherView(spinner->getLocalRect(), &item.mRect, page);
                    addFinding("TOO_NARROW", item, nullptr,
                        llformat("spinner=%d minimum=40", spinner->getRect().getWidth()));
                }
            }
        }

        std::vector<AuditLeaf> overlap_units;
        overlap_units.reserve(leaves.size());
        for (const AuditLeaf& leaf : leaves)
        {
            if (!dynamic_cast<LLTabContainer*>(leaf.mView->getParent()))
            {
                AuditLeaf unit = leaf;
                unit.mRect = overlapRect(unit);
                overlap_units.push_back(std::move(unit));
            }
        }
        std::vector<LLHandle<LLScrollContainer>> scrollers;
        collectAllScrollers(page, scrollers);
        for (const LLHandle<LLScrollContainer>& handle : scrollers)
        {
            LLScrollContainer* scroller = handle.get();
            if (!scroller || !visibleWithin(scroller, page)) continue;
            AuditLeaf unit;
            unit.mView = scroller;
            unit.mContainingScroller = containingScroller(scroller, floater);
            unit.mPath = relativePath(scroller, page);
            scroller->localRectToOtherView(scroller->getLocalRect(), &unit.mRect, page);
            if (unit.mRect.getWidth() <= 0 || unit.mRect.getHeight() <= 0) continue;
            for (LLView* cursor = scroller; cursor && cursor != page;
                 cursor = cursor->getParent())
            {
                if (!cursor->getName().empty()) unit.mLineage.insert(cursor->getName());
            }
            overlap_units.push_back(std::move(unit));
        }

        for (size_t i = 0; i < overlap_units.size(); ++i)
        {
            for (size_t j = i + 1; j < overlap_units.size(); ++j)
            {
                const AuditLeaf& a = overlap_units[i];
                const AuditLeaf& b = overlap_units[j];
                const S32 overlap_x = llmin(a.mRect.mRight, b.mRect.mRight) -
                                      llmax(a.mRect.mLeft, b.mRect.mLeft);
                const S32 overlap_y = llmin(a.mRect.mTop, b.mRect.mTop) -
                                      llmax(a.mRect.mBottom, b.mRect.mBottom);
                if (overlap_x <= 2 || overlap_y <= 2) continue;
                if (a.mContainingScroller != b.mContainingScroller) continue;
                if (a.mView->hasAncestor(b.mView) || b.mView->hasAncestor(a.mView)) continue;
                LLView* common = nearestCommonAncestor(a.mView, b.mView, page);
                if ((common && isRealCompositeControl(common)) ||
                    mOverlapPolicy.allows(a, b))
                {
                    continue;
                }
                const bool map_footer_pair =
                    (a.mView->getName() == "cine_light_map" && b.mView->getName().find("kc_light_map_view_") == 0) ||
                    (b.mView->getName() == "cine_light_map" && a.mView->getName().find("kc_light_map_view_") == 0);
                if (map_footer_pair)
                    continue;
                const bool a_icon = dynamic_cast<LLIconCtrl*>(a.mView) != nullptr;
                const bool b_icon = dynamic_cast<LLIconCtrl*>(b.mView) != nullptr;
                const bool a_contains_b = a.mRect.mLeft <= b.mRect.mLeft &&
                    a.mRect.mRight >= b.mRect.mRight &&
                    a.mRect.mBottom <= b.mRect.mBottom && a.mRect.mTop >= b.mRect.mTop;
                const bool b_contains_a = b.mRect.mLeft <= a.mRect.mLeft &&
                    b.mRect.mRight >= a.mRect.mRight &&
                    b.mRect.mBottom <= a.mRect.mBottom && b.mRect.mTop >= a.mRect.mTop;
                if ((a_icon || b_icon) && (a_contains_b || b_contains_a)) continue;
                addFinding("OVERLAP", a, &b,
                    llformat("intersection=%dx%d", overlap_x, overlap_y));
            }
        }
    }

    void prepareCapturePositions()
    {
        mCaptureScroller = LLHandle<LLScrollContainer>();
        mCapturePositions.clear();
        mCapturePositions.push_back(0);
        LLView* page = auditPageRoot();
        if (!page) return;
        std::vector<LLHandle<LLScrollContainer>> scrollers;
        collectAllScrollers(page, scrollers);
        S32 best_overflow = 0;
        S32 best_viewport = 0;
        for (const LLHandle<LLScrollContainer>& handle : scrollers)
        {
            LLScrollContainer* scroller = handle.get();
            if (!scroller || !visibleWithin(scroller, page) || !scroller->getScrolledView()) continue;
            const S32 viewport = scroller->getVisibleContentRect().getHeight();
            const S32 overflow = scroller->getScrolledView()->getLocalRect().getHeight() - viewport;
            if (overflow > best_overflow && viewport > 0)
            {
                best_overflow = overflow;
                best_viewport = viewport;
                mCaptureScroller = handle;
            }
        }
        if (best_overflow <= 0) return;
        mCapturePositions.clear();
        for (S32 pos = 0; pos < best_overflow && mCapturePositions.size() < 5;
             pos += best_viewport)
        {
            mCapturePositions.push_back(pos);
        }
        if (mCapturePositions.empty() || mCapturePositions.back() != best_overflow)
        {
            mCapturePositions.push_back(best_overflow);
        }
    }

    void requestCapture()
    {
        std::string size_dir;
        std::string floater_dir;
        if (isDirectorV2())
        {
            floater_dir = mAuditRoot + gDirUtilp->getDirDelimiter() + "director";
            size_dir = floater_dir + gDirUtilp->getDirDelimiter() +
                "audit-" + auditSizeName();
            LLFile::mkdir(floater_dir);
            LLFile::mkdir(size_dir);
            if (auditPagePath().find("light_group/") == 0)
            {
                size_dir += gDirUtilp->getDirDelimiter() + "light_group";
                LLFile::mkdir(size_dir);
            }
        }
        else
        {
            size_dir = mAuditRoot + gDirUtilp->getDirDelimiter() + auditSizeName();
            floater_dir = size_dir + gDirUtilp->getDirDelimiter() +
                safeComponent(mTargets[mTargetIndex]);
            LLFile::mkdir(size_dir);
            LLFile::mkdir(floater_dir);
        }
        const std::string capture_dir = isDirectorV2() ? size_dir : floater_dir;
        if (isDirectorV2())
        {
            std::string leaf = auditPagePath();
            const size_t slash = leaf.rfind('/');
            if (slash != std::string::npos) leaf = leaf.substr(slash + 1);
            mCaptureFilename = capture_dir + gDirUtilp->getDirDelimiter() +
                safeComponent(leaf) + "__" + llformat("%03u", (U32)mAuditPageIndex) +
                "__" + safeComponent(auditStateName()) + "__" +
                llformat("%u", (U32)mCapturePositionIndex) + ".png";
        }
        else
        {
            mCaptureFilename = capture_dir + gDirUtilp->getDirDelimiter() +
                llformat("%03u_", (U32)mAuditPageIndex) + safeComponent(auditPagePath()) + "_" +
                llformat("%u", (U32)mCapturePositionIndex) + ".png";
        }
        mCaptureFailed = false;
        mCaptureWaitFrames = 0;
        mCaptureRequested = true;
    }

    void writeStyleProbe()
    {
        LLFloater* floater = mFloater.get();
        ALKitShell* shell = floater ? findFirstView<ALKitShell>(floater) : nullptr;
        if (!floater || !shell || mCapturedImage.isNull())
            return;
        if (mAuditPageIndex > 0)
        {
            const size_t index = mAuditPageIndex - 1;
            if (index >= mTabs.size() || !dynamic_cast<ALKitLazyPage*>(mTabs[index].mPanel.get()))
                return;
        }
        LLSD probe = LLSD::emptyMap();
        probe["size"] = auditSizeName();
        probe["page"] = auditPagePath();
        probe["state"] = auditStateName();
        probe["source"] = "captured framebuffer pixels compared with style_tokens.json";
        probe["tolerance_rgb8"] = 28;
        probe["widgets"] = LLSD::emptyArray();
        probe["checks"] = LLSD::emptyArray();
        S32 mismatch_count = 0;
        const S32 image_width = mCapturedImage->getWidth();
        const S32 image_height = mCapturedImage->getHeight();
        const S32 components = mCapturedImage->getComponents();
        const U8* pixels = mCapturedImage->getData();
        const LLRect floater_rect = floater->getLocalRect();
        auto sample = [&](LLView* view, F32 x_fraction, F32 y_fraction, LLColor4U& value)
        {
            if (!view || !pixels || image_width <= 0 || image_height <= 0 || components < 3)
                return false;
            LLRect rect;
            if (!view->localRectToOtherView(view->getLocalRect(), &rect, floater) || rect.getWidth() <= 0 || rect.getHeight() <= 0)
                return false;
            const F32 local_x = rect.mLeft + llclamp(x_fraction, 0.f, 1.f) * llmax(0, rect.getWidth() - 1);
            const F32 local_y = rect.mBottom + llclamp(y_fraction, 0.f, 1.f) * llmax(0, rect.getHeight() - 1);
            const S32 px = llclamp(llfloor(local_x * image_width / llmax(1, floater_rect.getWidth())), 0, image_width - 1);
            const S32 py = llclamp(llfloor(local_y * image_height / llmax(1, floater_rect.getHeight())), 0, image_height - 1);
            const U8* source = pixels + (py * image_width + px) * components;
            value = LLColor4U(source[0], source[1], source[2], 255);
            return true;
        };
        auto delta = [](const LLColor4U& actual, const LLColor4& expected)
        {
            S32 result = 0;
            for (S32 component = 0; component < 3; ++component)
                result = llmax(result, llabs((S32)actual.mV[component] - ll_round(expected.mV[component] * 255.f)));
            return result;
        };
        auto addCheck = [&](const std::string& role, LLView* view, const std::string& token,
                            const LLColor4U& actual, const LLColor4& expected, S32 tolerance)
        {
            if (!view)
                return;
            LLRect rect;
            view->localRectToOtherView(view->getLocalRect(), &rect, floater);
            LLSD item = LLSD::emptyMap();
            item["role"] = role;
            item["name"] = view->getName();
            item["rect"] = LLSD::emptyArray();
            item["rect"].append(rect.mLeft);
            item["rect"].append(rect.mBottom);
            item["rect"].append(rect.mRight);
            item["rect"].append(rect.mTop);
            item["token"] = token;
            item["sample_rgb8"] = LLSD::emptyArray();
            item["expected_rgb8"] = LLSD::emptyArray();
            for (S32 component = 0; component < 3; ++component)
            {
                item["sample_rgb8"].append(actual.mV[component]);
                item["expected_rgb8"].append(ll_round(expected.mV[component] * 255.f));
            }
            const S32 difference = delta(actual, expected);
            item["max_delta"] = difference;
            item["pass"] = difference <= tolerance;
            if (difference > tolerance)
            {
                ++mismatch_count;
                addFinding("STYLE_PIXEL_MISMATCH", semanticLeaf(view, floater), nullptr,
                           role + " expected " + token + " max RGB delta=" + llformat("%d", difference));
            }
            if (const LLFontGL* font = view->getFont())
            {
                item["font"] = font->getFontDesc().getName();
                item["font_size"] = font->getFontDesc().getSize();
                item["font_bold"] = (font->getFontDesc().getStyle() & LLFontGL::BOLD) != 0;
            }
            probe["widgets"].append(item);
        };
        auto addBackground = [&](const std::string& role, LLView* view, const std::string& token,
                                 const LLUIColor& expected, F32 x, F32 y)
        {
            if (!view || !visibleWithin(view, floater))
                return;
            LLColor4U actual;
            if (sample(view, x, y, actual))
                addCheck(role, view, token, actual, expected.get(), 28);
        };
        addBackground("tabs", shell->tabStrip(), "panel_bg", ALKitStyle::color(ALKitStyle::RAIL_BG), .5f, .03f);
        addBackground("transport", shell->transport(), "panel_bg", ALKitStyle::color(ALKitStyle::RAIL_BG), .5f, .08f);
        addBackground("action", shell->findChild<LLView>("btn_action"), "accent_amber", ALKitStyle::color(ALKitStyle::ACCENT), .08f, .2f); // ui-draw-lint: allow(lookup) capture-time probe
        auto addTextPresence = [&](const std::string& role, LLView* view, const std::string& token, const LLUIColor& expected)
        {
            if (!view || !visibleWithin(view, floater))
                return;
            bool present = false;
            for (S32 y = 2; y < view->getRect().getHeight() - 2 && !present; ++y)
                for (S32 x = 2; x < view->getRect().getWidth() - 2; ++x)
                {
                    LLColor4U actual;
                    const F32 fx = x / (F32)llmax(1, view->getRect().getWidth() - 1);
                    const F32 fy = y / (F32)llmax(1, view->getRect().getHeight() - 1);
                    if (sample(view, fx, fy, actual) && delta(actual, expected.get()) <= 48)
                    {
                        present = true;
                        break;
                    }
                }
            LLSD check = LLSD::emptyMap();
            check["name"] = role + "_text_colour";
            check["token"] = token;
            check["pass"] = present;
            probe["checks"].append(check);
            if (!present)
            {
                ++mismatch_count;
                addFinding("STYLE_TEXT_MISSING", semanticLeaf(view, floater), nullptr,
                           role + " has no rendered pixels matching " + token);
            }
        };
        addTextPresence("action", shell->findChild<LLView>("btn_action"), "accent_text_on", ALKitStyle::color(ALKitStyle::KNOB_ON)); // ui-draw-lint: allow(lookup) capture-time probe
        addTextPresence("timecode", shell->findChild<LLView>("take_timecode"), "text_primary", ALKitStyle::color(ALKitStyle::TEXT)); // ui-draw-lint: allow(lookup) capture-time probe
        ALKitPage* probed_page = mAuditPageIndex > 0
                                     ? findFirstView<ALKitPage>(mTabs[mAuditPageIndex - 1].mPanel.get())
                                     : findFirstView<ALKitPage>(auditPageRoot());
        if (ALKitPage* page = probed_page)
        {
            addBackground("page_bar", page->pageBar(), "window_bg", ALKitStyle::color(ALKitStyle::WINDOW_BG), .5f, .08f);
            addBackground("who", page->whoBand(), "inset_bg", ALKitStyle::color(ALKitStyle::INPUT_BG), .98f, .1f);
            addBackground("search", page->searchEditor(), "inset_bg", ALKitStyle::color(ALKitStyle::INPUT_BG), .92f, .5f);
            std::vector<ALKitCard*> cards;
            collectViewsOfType<ALKitCard>(page, cards);
            for (ALKitCard* card : cards)
                if (card->getVisible())
                {
                    LLRect card_rect;
                    ALKitScroller* scroller = page->scroller();
                    if (!scroller || !card->localRectToOtherView(card->getLocalRect(), &card_rect, scroller) ||
                        card_rect.mTop <= scroller->getVisibleContentRect().mBottom ||
                        card_rect.mBottom >= scroller->getVisibleContentRect().mTop)
                        continue;
                    addBackground("card", card, "card_bg", ALKitStyle::color(ALKitStyle::CARD_BG), .04f, .5f);
                    break;
                }
            if (!page->legacy())
            {
                std::vector<LLSpinCtrl*> spinners;
                collectViewsOfType<LLSpinCtrl>(page, spinners);
                for (LLSpinCtrl* spinner : spinners)
                    if (visibleWithin(spinner, floater))
                    {
                        for (LLView* child : *spinner->getChildList())
                            if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                            {
                                addBackground("number_field", editor, "inset_bg", ALKitStyle::color(ALKitStyle::INPUT_BG), .1f, .5f);
                                break;
                            }
                        break;
                    }
                std::vector<LLSliderCtrl*> sliders;
                collectViewsOfType<LLSliderCtrl>(page, sliders);
                for (LLSliderCtrl* slider : sliders)
                    if (visibleWithin(slider, floater))
                    {
                        for (LLView* child : *slider->getChildList())
                            if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                            {
                                addBackground("slider_value", editor, "inset_bg", ALKitStyle::color(ALKitStyle::INPUT_BG), .1f, .5f);
                                break;
                            }
                        break;
                    }
            }
        }

        std::vector<LLScrollbar*> scrollbars;
        collectViewsOfType<LLScrollbar>(floater, scrollbars);
        for (LLScrollbar* scrollbar : scrollbars)
        {
            bool kit_scrollbar = false;
            for (LLView* parent = scrollbar->getParent(); parent && parent != floater; parent = parent->getParent())
                if (dynamic_cast<ALKitScroller*>(parent) || parent->getName() == "cast_list")
                {
                    kit_scrollbar = true;
                    break;
                }
            if (!kit_scrollbar || !visibleWithin(scrollbar, floater))
                continue;
            bool thumb_present = false;
            for (S32 y = 2; y < scrollbar->getRect().getHeight() - 2 && !thumb_present; y += 2)
                for (S32 x = 2; x < scrollbar->getRect().getWidth() - 2; x += 2)
                {
                    LLColor4U actual;
                    const F32 fx = x / (F32)llmax(1, scrollbar->getRect().getWidth() - 1);
                    const F32 fy = y / (F32)llmax(1, scrollbar->getRect().getHeight() - 1);
                    if (sample(scrollbar, fx, fy, actual) && delta(actual, ALKitStyle::color(ALKitStyle::INPUT_BORDER).get()) <= 38)
                    {
                        thumb_present = true;
                        break;
                    }
                }
            LLSD check = LLSD::emptyMap();
            check["name"] = "kit_scrollbar_thumb";
            check["pass"] = thumb_present;
            probe["checks"].append(check);
            if (!thumb_present)
            {
                ++mismatch_count;
                addFinding("STYLE_SCROLLBAR", semanticLeaf(scrollbar, floater), nullptr,
                           "kit scrollbar has no rendered token thumb pixels");
            }
            break;
        }

        std::vector<LLComboBox*> combos;
        collectViewsOfType<LLComboBox>(floater, combos);
        S32 visible_combos = 0;
        S32 missing_carets = 0;
        for (LLComboBox* combo : combos)
        {
            if (!visibleWithin(combo, floater) || !intersectsScrollViewports(combo, floater))
                continue;
            LLRect rect;
            combo->localRectToOtherView(combo->getLocalRect(), &rect, floater);
            if (rect.mRight <= 0 || rect.mLeft >= floater->getLocalRect().mRight ||
                rect.mTop <= 0 || rect.mBottom >= floater->getLocalRect().mTop)
                continue;
            ++visible_combos;
            bool caret = false;
            const LLColor4 expected = ALKitStyle::color(ALKitStyle::DIM).get();
            S32 min_luma = 255;
            S32 max_luma = 0;
            const S32 left = llmax(rect.mLeft, rect.mRight - 20);
            const S32 bottom = llmax(rect.mBottom + 3, (rect.mBottom + rect.mTop) / 2 - 6);
            const S32 top = llmin(rect.mTop - 3, (rect.mBottom + rect.mTop) / 2 + 7);
            for (S32 y = bottom; y < top; ++y)
                for (S32 x = left; x < rect.mRight - 5; ++x)
                {
                    const F32 fx = (x - rect.mLeft) / (F32)llmax(1, rect.getWidth() - 1);
                    const F32 fy = (y - rect.mBottom) / (F32)llmax(1, rect.getHeight() - 1);
                    LLColor4U actual;
                    if (sample(combo, fx, fy, actual))
                    {
                        const S32 luma = (actual.mV[0] * 3 + actual.mV[1] * 6 + actual.mV[2]) / 10;
                        min_luma = llmin(min_luma, luma);
                        max_luma = llmax(max_luma, luma);
                        caret |= delta(actual, expected) <= 52;
                    }
                }
            caret |= max_luma - min_luma >= 18;
            if (!caret)
            {
                ++missing_carets;
                ++mismatch_count;
                addFinding("STYLE_COMBO_CARET", semanticLeaf(combo, floater), nullptr, "visible combo has no rendered caret pixels");
            }
        }
        LLSD caret_check = LLSD::emptyMap();
        caret_check["name"] = "visible_combo_carets";
        caret_check["visible"] = visible_combos;
        caret_check["missing"] = missing_carets;
        caret_check["pass"] = missing_carets == 0;
        probe["checks"].append(caret_check);

        std::vector<LLButton*> buttons;
        collectViewsOfType<LLButton>(floater, buttons);
        LLButton* selected = nullptr;
        LLButton* unselected = nullptr;
        for (LLButton* button : buttons)
        {
            const std::string& name = button->getName();
            const bool state_button = name.rfind("kit_mode_seg_", 0) == 0 || name.find("_seg_") != std::string::npos;
            if (!state_button || !visibleWithin(button, floater))
                continue;
            (button->getToggleState() ? selected : unselected) = button;
        }
        bool states_differ = true;
        S32 state_delta = 0;
        if (selected && unselected)
        {
            LLColor4U selected_pixel, unselected_pixel;
            if (sample(selected, .12f, .18f, selected_pixel) && sample(unselected, .12f, .18f, unselected_pixel))
            {
                for (S32 component = 0; component < 3; ++component)
                    state_delta = llmax(state_delta, llabs((S32)selected_pixel.mV[component] - (S32)unselected_pixel.mV[component]));
                states_differ = state_delta >= 10;
            }
            if (!states_differ)
            {
                ++mismatch_count;
                addFinding("STYLE_STATE_IDENTICAL", semanticLeaf(selected, floater), nullptr,
                           "selected and unselected controls render the same background");
            }
        }
        LLSD state_check = LLSD::emptyMap();
        state_check["name"] = "selected_unselected_differ";
        state_check["max_delta"] = state_delta;
        state_check["pass"] = states_differ;
        probe["checks"].append(state_check);

        if (mAuditPageIndex > 0)
        {
            ALKitPage* page = probed_page;
            if (page && page->legacy() && page->legacyHost())
            {
                LLRect rect;
                page->legacyHost()->localRectToOtherView(page->legacyHost()->getLocalRect(), &rect, floater);
                S32 min_luma = 255, max_luma = 0, changed = 0;
                LLColor4U first;
                bool have_first = false;
                for (S32 y = rect.mBottom + 4; y < rect.mTop - 4; y += 4)
                    for (S32 x = rect.mLeft + 4; x < rect.mRight - 4; x += 4)
                    {
                        LLColor4U pixel;
                        const F32 fx = (x - rect.mLeft) / (F32)llmax(1, rect.getWidth() - 1);
                        const F32 fy = (y - rect.mBottom) / (F32)llmax(1, rect.getHeight() - 1);
                        if (!sample(page->legacyHost(), fx, fy, pixel))
                            continue;
                        const S32 luma = (pixel.mV[0] * 3 + pixel.mV[1] * 6 + pixel.mV[2]) / 10;
                        min_luma = llmin(min_luma, luma);
                        max_luma = llmax(max_luma, luma);
                        if (!have_first)
                        {
                            first = pixel;
                            have_first = true;
                        }
                        else if (llabs((S32)pixel.mV[0] - first.mV[0]) > 6 || llabs((S32)pixel.mV[1] - first.mV[1]) > 6 ||
                                 llabs((S32)pixel.mV[2] - first.mV[2]) > 6)
                            ++changed;
                    }
                const bool body_drawn = max_luma - min_luma >= 10 && changed >= 12;
                LLSD body_check = LLSD::emptyMap();
                body_check["name"] = "legacy_body_pixel_variance";
                body_check["luma_range"] = max_luma - min_luma;
                body_check["changed_samples"] = changed;
                body_check["pass"] = body_drawn;
                probe["checks"].append(body_check);
                if (!body_drawn)
                {
                    ++mismatch_count;
                    addFinding("LEGACY_BODY_BLANK_PNG", semanticLeaf(page->legacyHost(), floater), nullptr,
                               "captured legacy body has no rendered-control pixel variance");
                    addFinding("LEGACY_BODY_UNBOUND", semanticLeaf(page->legacyHost(), floater), nullptr,
                               "legacy descendants exist structurally but did not render into the captured body");
                }
            }
        }
        probe["mismatch_count"] = mismatch_count;
        probe["pass"] = mismatch_count == 0;
        std::string path = mCaptureFilename;
        if (path.size() >= 4)
            path.replace(path.size() - 4, 4, ".style.json");
        llofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        out << LlsdToJson(probe) << '\n';
        out.flush();
    }

    void saveCapturedImage()
    {
        if (mCapturedImage.isNull()) return;
        if (isDirectorV2() && mCapturePositionIndex == 0 && auditStateName() == "default")
            writeStyleProbe();
        LLPointer<LLImagePNG> png = new LLImagePNG;
        if (!png->encode(mCapturedImage, 0.f) || !png->save(mCaptureFilename))
        {
            LL_WARNS("UILayoutAudit") << "Unable to write " << mCaptureFilename << LL_ENDL;
        }
        mCapturedImage = nullptr;
    }

    void beginAuditOrClose()
    {
        collectKitMetrics();
        if (mLayout && (!mAB || !mCullOn))
        {
            mAuditSizeIndex = 0;
            mAuditPageIndex = 0;
            mPhase = AUDIT_SIZE_PREP;
        }
        else
        {
            mPhase = CLOSE;
        }
    }

    void advanceAfterCurrentTab()
    {
        const TabTarget& target = mTabs[mTabIndex];
        mScrollIndex = target.mScrollBegin;
        if (mScrollIndex < target.mScrollEnd) mPhase = SCROLL_BOTTOM;
        else if (++mTabIndex < mTabs.size()) mPhase = TAB_SELECT;
        else beginAuditOrClose();
    }

    void skipAuditSize()
    {
        if (++mAuditSizeIndex < mAuditSizes.size())
            mPhase = AUDIT_SIZE_PREP;
        else
            mPhase = AUDIT_RESTORE;
    }

    void advanceAuditPageOrSize()
    {
        if (++mAuditPageIndex <= mTabs.size()) mPhase = AUDIT_PAGE_SELECT;
        else if (++mAuditSizeIndex < mAuditSizes.size()) mPhase = AUDIT_SIZE_PREP;
        else mPhase = AUDIT_RESTORE;
    }

    void closeCurrentTarget()
    {
        restoreDirectorAuditSettings();
        restoreInitialState();
        if (LLFloater* floater = mFloater.get())
        {
            if (mOriginalFloaterRect.getWidth() > 0 && mOriginalFloaterRect.getHeight() > 0)
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            if (isInventoryStress())
            {
                floater->closeFloater(false);
            }
            else
            {
                LLFloaterReg::hideInstance(mTargets[mTargetIndex]);
            }
        }
        mFloater = LLHandle<LLFloater>();
        mCaptureRequested = false;
        mCaptureFailed = false;
        mCapturedImage = nullptr;
        mSamples.clear();
        mSettleFrames = 0;
        mPhase = CLOSE_SETTLE;
    }

    void abortCurrentTarget(const std::string& reason)
    {
        LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest abort "
            << mTargets[mTargetIndex] << ": " << reason << LL_ENDL;
        closeCurrentTarget();
    }

    bool nextTarget()
    {
        return ++mTargetIndex < (S32)mTargets.size();
    }

    bool isInventoryStress() const
    {
        return mTargetIndex >= 0 && mTargetIndex < (S32)mTargets.size() &&
               mTargets[mTargetIndex] == "inventory_stress";
    }

    InventoryStressFloater* inventoryStressFloater() const
    {
        return dynamic_cast<InventoryStressFloater*>(mFloater.get());
    }

    bool step()
    {
        if (mUiFrameStall.getElapsedTimeF32() >= 10.f &&
            !mUiFrameStallWarned)
        {
            LL_WARNS("UIPerfSelfTest")
                << "UIPerfSelfTest no UI frame for 10 s: window hidden or minimized"
                << LL_ENDL;
            mUiFrameStallWarned = true;
        }
        if (InventoryStressFloater* stress = inventoryStressFloater())
        {
            stress->updateFolderView();
        }
        if (mPhase >= FIRST_FRAME && mPhase <= CLOSE && !mFloater.get())
        {
            abortCurrentTarget("floater died");
            return false;
        }
        if (mPhase >= TAB_SELECT && mPhase <= TAB_MEASURE &&
            (mTabIndex >= mTabs.size() || !mTabs[mTabIndex].mPanel.get() ||
             !chainAlive(mTabs[mTabIndex].mChain)))
        {
            abortCurrentTarget("tab target died");
            return false;
        }
        if (mPhase >= SCROLL_BOTTOM && mPhase <= SCROLL_TOP_MEASURE)
        {
            const std::vector<ScrollTarget>& targets = mInDefaultScroll
                ? mDefaultScrollers : mScrollers;
            if (mScrollIndex >= targets.size() ||
                !targets[mScrollIndex].mScroller.get() ||
                !chainAlive(targets[mScrollIndex].mChain))
            {
                abortCurrentTarget("scroller target died");
                return false;
            }
            if (!mInDefaultScroll &&
                (mTabIndex >= mTabs.size() || !mTabs[mTabIndex].mPanel.get()))
            {
                abortCurrentTarget("tab panel died");
                return false;
            }
        }
        switch (mPhase)
        {
        case WAIT_SETTLE:
        {
            FrameSample ignored;
            takeFrame(ignored);
            if (mSettle.getElapsedTimeF32() < 3.f) return false;
            mPhase = BASELINE;
            break;
        }
        case BASELINE:
            if (collect(mFrames))
            {
                addRow("", "", "baseline");
                mTargetIndex = -1;
                mPhase = nextTarget() ? OPEN : NEXT_PASS;
            }
            break;
        case OPEN:
        {
            const std::string& name = mTargets[mTargetIndex];
            FrameSample ignored;
            takeFrame(ignored);
            const bool prior_flush = LLError::getAlwaysFlush();
            LLError::setAlwaysFlush(true);
            LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest begin " << name << LL_ENDL;
            LLError::setAlwaysFlush(prior_flush);
            LLTimer timer;
            LLFloater* floater = nullptr;
            if (name == "inventory_stress")
            {
                if (mInventoryItems == 0)
                {
                    LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest skip inventory_stress: "
                        "UIPerfInventoryStress is zero" << LL_ENDL;
                    mPhase = nextTarget() ? OPEN : NEXT_PASS;
                    break;
                }
                InventoryStressFloater* stress = new InventoryStressFloater(mInventoryItems);
                gFloaterView->addChild(stress);
                stress->openFloater();
                floater = stress;
            }
            else
            {
                floater = LLFloaterReg::showInstance(name, LLSD(), false);
            }
            mActionMs = timer.getElapsedTimeF64() * 1000.0;
            if (!floater)
            {
                LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest skip " << name
                    << ": registry name did not produce a floater" << LL_ENDL;
                mPhase = nextTarget() ? OPEN : NEXT_PASS;
                break;
            }
            mFloater = floater->getHandle();
            mOriginalFloaterRect = LLRect();
            prepareFloaterActions();
            if (isDirectorV2()) applyDirectorAuditDefaults();
            mReadyWaitFrames = 0;
            mPhase = FIRST_FRAME;
            break;
        }
        case FIRST_FRAME:
        {
            if (InventoryStressFloater* stress = inventoryStressFloater();
                stress && !stress->folderViewReady())
            {
                FrameSample ignored;
                takeFrame(ignored);
                // Never hang the runner on a fixture that never arranges.
                if (++mReadyWaitFrames >= 600)
                {
                    LL_WARNS("UIPerfSelfTest")
                        << "inventory_stress: folder view not ready after 600 frames; "
                        << stress->folderViewReadyStatus() << "; skipping target"
                        << LL_ENDL;
                    mPhase = CLOSE;
                }
                return false;
            }
            FrameSample sample;
            if (!takeFrame(sample)) return false;
            addRow(mTargets[mTargetIndex], "", "open", mActionMs, sample.mUiMs,
                   mReadyWaitFrames);
            if (isDirectorV2())
            {
                collectKitBuildMetrics(mFloater.get());
            }
            mPhase = isInventoryStress() ? INVENTORY_TOP : DEFAULT_MEASURE;
            break;
        }
        case INVENTORY_TOP:
        {
            FrameSample ignored;
            takeFrame(ignored);
            InventoryStressFloater* stress = inventoryStressFloater();
            if (!stress) { abortCurrentTarget("synthetic floater type mismatch"); break; }
            stress->scroller()->goToTop();
            beginInventorySettle();
            mPhase = INVENTORY_TOP_MEASURE;
            break;
        }
        case INVENTORY_TOP_MEASURE:
            if (!inventoryReadyToMeasure("scroll_top")) break;
            if (collectInventoryTop(mFrames))
            {
                addRow("inventory_stress", "synthetic", "scroll_top", 0.0, 0.0,
                       mReadyWaitFrames);
                mPhase = INVENTORY_MIDDLE;
            }
            break;
        case INVENTORY_MIDDLE:
        {
            FrameSample ignored;
            takeFrame(ignored);
            InventoryStressFloater* stress = inventoryStressFloater();
            LLScrollContainer* scroller = stress ? stress->scroller() : nullptr;
            if (!scroller || !scroller->getScrolledView())
            {
                abortCurrentTarget("synthetic scroller died");
                break;
            }
            const S32 overflow = llmax(0, scroller->getScrolledView()->getRect().getHeight() -
                                         scroller->getVisibleContentRect().getHeight());
            scroller->setDocPosVertical(overflow / 2);
            beginInventorySettle();
            mPhase = INVENTORY_MIDDLE_MEASURE;
            break;
        }
        case INVENTORY_MIDDLE_MEASURE:
            if (!inventoryReadyToMeasure("scroll_middle")) break;
            if (collect(mFrames))
            {
                addRow("inventory_stress", "synthetic", "scroll_middle", 0.0, 0.0,
                       mReadyWaitFrames);
                mPhase = INVENTORY_BOTTOM;
            }
            break;
        case INVENTORY_BOTTOM:
        {
            FrameSample ignored;
            takeFrame(ignored);
            InventoryStressFloater* stress = inventoryStressFloater();
            if (!stress) { abortCurrentTarget("synthetic floater died"); break; }
            stress->scroller()->goToBottom();
            beginInventorySettle();
            mPhase = INVENTORY_BOTTOM_MEASURE;
            break;
        }
        case INVENTORY_BOTTOM_MEASURE:
            if (!inventoryReadyToMeasure("scroll_bottom")) break;
            if (collect(mFrames))
            {
                addRow("inventory_stress", "synthetic", "scroll_bottom", 0.0, 0.0,
                       mReadyWaitFrames);
                mPhase = INVENTORY_SCROLL_SWEEP;
            }
            break;
        case INVENTORY_SCROLL_SWEEP:
        {
            InventoryStressFloater* stress = inventoryStressFloater();
            LLScrollContainer* scroller = stress ? stress->scroller() : nullptr;
            if (!scroller || !scroller->getScrolledView())
            {
                abortCurrentTarget("synthetic scroller died during sweep");
                break;
            }
            const S32 overflow = llmax(0, scroller->getScrolledView()->getRect().getHeight() -
                                         scroller->getVisibleContentRect().getHeight());
            const F32 fraction = (mFrames > 1)
                ? (F32)mSamples.size() / (F32)(mFrames - 1) : 1.f;
            scroller->setDocPosVertical(ll_round(fraction * (F32)overflow));
            if (collect(mFrames))
            {
                addRow("inventory_stress", "synthetic", "scroll_sweep");
                mPhase = INVENTORY_FILTER;
            }
            break;
        }
        case INVENTORY_FILTER:
        {
            FrameSample ignored;
            takeFrame(ignored);
            InventoryStressFloater* stress = inventoryStressFloater();
            if (!stress) { abortCurrentTarget("synthetic floater died before filter"); break; }
            LLTimer timer;
            stress->setFilter("a");
            mActionMs = timer.getElapsedTimeF64() * 1000.0;
            stress->updateFolderView();
            beginInventorySettle();
            mPhase = INVENTORY_FILTER_FIRST_FRAME;
            break;
        }
        case INVENTORY_FILTER_FIRST_FRAME:
        {
            FrameSample sample;
            if (!takeFrame(sample)) break;
            mFirstFrameMs = sample.mUiMs;
            mPhase = INVENTORY_FILTER_SETTLE;
            break;
        }
        case INVENTORY_FILTER_SETTLE:
            if (inventoryReadyToMeasure("filter_a"))
            {
                mPhase = INVENTORY_FILTER_MEASURE;
            }
            break;
        case INVENTORY_FILTER_MEASURE:
            if (collect(mFrames))
            {
                addRow("inventory_stress", "synthetic", "filter_a",
                       mActionMs, mFirstFrameMs, mReadyWaitFrames);
                mActionMs = 0.0;
                mPhase = INVENTORY_HOVER_SWEEP;
            }
            break;
        case INVENTORY_HOVER_SWEEP:
        {
            InventoryStressFloater* stress = inventoryStressFloater();
            if (!stress) { abortCurrentTarget("synthetic floater died during hover"); break; }
            const F32 fraction = (mFrames > 1)
                ? (F32)mSamples.size() / (F32)(mFrames - 1) : 1.f;
            LLTimer timer;
            stress->hoverAt(fraction);
            mActionMs += timer.getElapsedTimeF64() * 1000.0;
            if (collect(mFrames))
            {
                const F64 mean_hit_test_ms = mActionMs / (F64)mSamples.size();
                addRow("inventory_stress", "synthetic", "hover_sweep",
                       mean_hit_test_ms);
                mPhase = CLOSE;
            }
            break;
        }
        case DEFAULT_MEASURE:
            if (collect(mFrames))
            {
                LLFloater* floater = mFloater.get();
                if (!floater)
                {
                    abortCurrentTarget("floater died before settled rect capture");
                    break;
                }
                mOriginalFloaterRect = floater->getRect();
                addRow(mTargets[mTargetIndex], "default", "steady");
                if (!mDefaultScrollers.empty())
                {
                    mInDefaultScroll = true;
                    mScrollIndex = 0;
                    mPhase = SCROLL_BOTTOM;
                }
                else
                {
                    if (mTabs.empty()) beginAuditOrClose();
                    else mPhase = TAB_SELECT;
                }
            }
            break;
        case TAB_SELECT:
        {
            const TabTarget target = mTabs[mTabIndex];
            if (!target.mPanel.get())
            {
                abortCurrentTarget("tab panel died: " + target.mPath);
                break;
            }
            FrameSample ignored;
            takeFrame(ignored);
            const bool prior_flush = LLError::getAlwaysFlush();
            LLError::setAlwaysFlush(true);
            LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest begin "
                << mTargets[mTargetIndex] << "/" << target.mPath << LL_ENDL;
            LLError::setAlwaysFlush(prior_flush);
            LLTimer timer;
            const bool lazy_unbuilt = isDirectorV2() &&
                dynamic_cast<ALKitLazyPage*>(target.mPanel.get()) &&
                !findFirstView<ALKitPage>(target.mPanel.get());
            if (!applyChain(target.mChain))
            {
                abortCurrentTarget("tab container died: " + target.mPath);
                break;
            }
            mActionMs = timer.getElapsedTimeF64() * 1000.0;
            mPendingKitBuild = lazy_unbuilt;
            if (lazy_unbuilt)
            {
                TabTarget& current = mTabs[mTabIndex];
                current.mScrollBegin = mScrollers.size();
                collectScrollers(current.mPanel.get(), current.mChain,
                                 current.mPath, mScrollers);
                current.mScrollEnd = mScrollers.size();
                std::vector<TabTarget> nested;
                collectTabs(current.mPanel.get(), current.mChain,
                            current.mRawPath, nested);
                for (TabTarget& nested_target : nested)
                {
                    for (const auto& chain_entry : nested_target.mChain)
                    {
                        LLTabContainer* tabs = chain_entry.first.get();
                        const bool known = std::any_of(mInitialTabs.begin(), mInitialTabs.end(),
                            [tabs](const auto& saved) { return saved.first.get() == tabs; });
                        if (tabs && !known)
                            mInitialTabs.emplace_back(chain_entry.first, tabs->getCurrentPanelIndex());
                    }
                    nested_target.mScrollBegin = mScrollers.size();
                    collectScrollers(nested_target.mPanel.get(), nested_target.mChain,
                                     nested_target.mPath, mScrollers);
                    nested_target.mScrollEnd = mScrollers.size();
                }
                if (!nested.empty())
                {
                    mTabs.insert(mTabs.begin() + mTabIndex + 1,
                                 nested.begin(), nested.end());
                }
            }
            mPhase = TAB_FIRST_FRAME;
            break;
        }
        case TAB_FIRST_FRAME:
        {
            FrameSample sample;
            if (!takeFrame(sample)) return false;
            mFirstFrameMs = sample.mUiMs;
            mPhase = TAB_MEASURE;
            break;
        }
        case TAB_MEASURE:
            if (collect(mFrames / 2))
            {
                const TabTarget& target = mTabs[mTabIndex];
                addRow(mTargets[mTargetIndex], target.mPath, "tab",
                       mActionMs, mFirstFrameMs);
                if (mPendingKitBuild)
                {
                    collectKitBuildMetrics(target.mPanel.get());
                    mPendingKitBuild = false;
                }
                if (isDirectorV2() &&
                    target.mPath.find("cine_light_rig_tab") != std::string::npos)
                {
                    mActionMs = 0.0;
                    mPhase = DIRECTOR_HOVER_SWEEP;
                    break;
                }
                mScrollIndex = target.mScrollBegin;
                if (mScrollIndex < target.mScrollEnd)
                {
                    mPhase = SCROLL_BOTTOM;
                }
                else if (++mTabIndex < mTabs.size())
                {
                    mPhase = TAB_SELECT;
                }
                else
                {
                    beginAuditOrClose();
                }
            }
            break;
        case DIRECTOR_HOVER_SWEEP:
        {
            const TabTarget& target = mTabs[mTabIndex];
            LLFloater* floater = mFloater.get();
            LLPanel* panel = target.mPanel.get();
            if (!floater || !panel)
            {
                abortCurrentTarget("Director Rig panel died during hover sweep");
                break;
            }
            LLRect rect;
            if (!panel->localRectToOtherView(panel->getLocalRect(), &rect, floater))
            {
                abortCurrentTarget("Director Rig hover coordinates unavailable");
                break;
            }
            const F32 fraction = (mFrames > 1)
                ? (F32)mSamples.size() / (F32)(mFrames - 1) : 1.f;
            const S32 x = rect.mLeft + rect.getWidth() / 2;
            const S32 y = rect.mBottom + ll_round(fraction * (F32)rect.getHeight());
            LLTimer timer;
            floater->handleHover(x, y, MASK_NONE);
            const F64 hit_test_ms = timer.getElapsedTimeF64() * 1000.0;
            FrameSample sample;
            if (!takeFrame(sample)) break;
            mActionMs += hit_test_ms;
            mSamples.push_back(sample);
            if (mSamples.size() >= mFrames)
            {
                const F64 mean_hit_test_ms = mActionMs / (F64)mSamples.size();
                addRow("director", target.mPath, "hover_sweep", mean_hit_test_ms);
                floater->handleHover(-1, -1, MASK_NONE);
                mSettleFrames = 0;
                mPhase = DIRECTOR_HOVER_CLEAR;
            }
            break;
        }
        case DIRECTOR_HOVER_CLEAR:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && ++mSettleFrames >= 2) advanceAfterCurrentTab();
            break;
        }
        case SCROLL_BOTTOM:
        {
            FrameSample ignored;
            takeFrame(ignored);
            ScrollTarget& target = mInDefaultScroll
                ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
            LLScrollContainer* scroller = target.mScroller.get();
            if (!scroller || !applyChain(target.mChain))
            {
                abortCurrentTarget("scroller died: " + target.mPath);
                break;
            }
            scroller->goToBottom();
            mPhase = SCROLL_BOTTOM_MEASURE;
            break;
        }
        case SCROLL_BOTTOM_MEASURE:
            if (collect(mFrames / 2))
            {
                const ScrollTarget& target = mInDefaultScroll
                    ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
                addRow(mTargets[mTargetIndex], target.mPath,
                       "scroll_bottom");
                mPhase = SCROLL_TOP;
            }
            break;
        case SCROLL_TOP:
        {
            FrameSample ignored;
            takeFrame(ignored);
            ScrollTarget& target = mInDefaultScroll
                ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
            LLScrollContainer* scroller = target.mScroller.get();
            if (!scroller)
            {
                abortCurrentTarget("scroller died: " + target.mPath);
                break;
            }
            scroller->goToTop();
            mPhase = SCROLL_TOP_MEASURE;
            break;
        }
        case SCROLL_TOP_MEASURE:
            if (collect(mFrames / 2))
            {
                const ScrollTarget& target = mInDefaultScroll
                    ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
                addRow(mTargets[mTargetIndex], target.mPath,
                       "scroll_top");
                ++mScrollIndex;
                if (mInDefaultScroll)
                {
                    if (mScrollIndex < mDefaultScrollers.size())
                    {
                        mPhase = SCROLL_BOTTOM;
                    }
                    else
                    {
                        mInDefaultScroll = false;
                        mScrollIndex = 0;
                        if (mTabs.empty()) beginAuditOrClose();
                        else mPhase = TAB_SELECT;
                    }
                    break;
                }
                if (mScrollIndex < mTabs[mTabIndex].mScrollEnd)
                {
                    mPhase = SCROLL_BOTTOM;
                }
                else
                {
                    ++mTabIndex;
                    if (mTabIndex < mTabs.size()) mPhase = TAB_SELECT;
                    else beginAuditOrClose();
                }
            }
            break;
        case AUDIT_SIZE_PREP:
        {
            FrameSample ignored;
            takeFrame(ignored);
            LLFloater* floater = mFloater.get();
            if (!floater)
            {
                abortCurrentTarget("floater died during layout audit");
                break;
            }
            restoreInitialState();
            if (mAuditSizeIndex >= mAuditSizes.size())
            {
                mPhase = AUDIT_RESTORE;
                break;
            }
            const AuditSize& audit_size = mAuditSizes[mAuditSizeIndex];
            if (audit_size.mKind == AuditSize::DEFAULT)
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            else if (audit_size.mKind == AuditSize::MINIMUM)
            {
                const S32 min_width = floater->getMinWidth();
                const S32 min_height = floater->getMinHeight();
                if (!floater->isResizable())
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit minimum skipped: not resizable "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    skipAuditSize();
                    break;
                }
                if (min_width <= 0 || min_height <= 0)
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit minimum skipped: invalid minimum "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    skipAuditSize();
                    break;
                }
                const S32 width = llmin(min_width, mOriginalFloaterRect.getWidth());
                const S32 height = llmin(min_height, mOriginalFloaterRect.getHeight());
                if (width == mOriginalFloaterRect.getWidth() &&
                    height == mOriginalFloaterRect.getHeight())
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit minimum skipped: matches original "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    skipAuditSize();
                    break;
                }
                floater->reshape(width, height, false);
            }
            else
            {
                const S32 min_width = floater->getMinWidth();
                const S32 min_height = floater->getMinHeight();
                if (!floater->isResizable() || audit_size.mWidth < min_width ||
                    audit_size.mHeight < min_height)
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit "
                        << audit_size.mToken << " skipped: below floater minimum "
                        << min_width << "x" << min_height << " "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    skipAuditSize();
                    break;
                }
                floater->reshape(audit_size.mWidth, audit_size.mHeight, false);
            }
            mAuditPageIndex = 0;
            mAuditSettleFrames = 0;
            mAuditSettleTimer.reset();
            mPhase = AUDIT_SIZE_SETTLE;
            break;
        }
        case AUDIT_SIZE_SETTLE:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && ++mAuditSettleFrames >= 5 &&
                mAuditSettleTimer.getElapsedTimeF32() >= 0.3f)
            {
                mPhase = AUDIT_PAGE_SELECT;
            }
            else if (isDirectorV2() && mAuditSettleFrames >= 600)
            {
                abortCurrentTarget("layout audit size did not settle after 600 frames: " +
                                   auditSizeName());
            }
            break;
        }
        case AUDIT_PAGE_SELECT:
        {
            FrameSample ignored;
            takeFrame(ignored);
            const bool prior_flush = LLError::getAlwaysFlush();
            LLError::setAlwaysFlush(true);
            LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest begin "
                << mTargets[mTargetIndex] << "/audit-" << auditSizeName()
                << "/" << auditPagePath() << LL_ENDL;
            LLError::setAlwaysFlush(prior_flush);
            if (!selectAuditPage() || !auditPageRoot())
            {
                abortCurrentTarget("layout audit page died: " + auditPagePath());
                break;
            }
            prepareKitAuditStates();
            mAuditStateApplied = applyKitAuditState();
            mAuditPageTimer.reset();
            std::vector<LLHandle<LLScrollContainer>> scrollers;
            collectAllScrollers(auditPageRoot(), scrollers);
            for (const LLHandle<LLScrollContainer>& handle : scrollers)
            {
                if (LLScrollContainer* scroller = handle.get()) scroller->goToTop();
            }
            mAuditSettleFrames = 0;
            mAuditSettleTimer.reset();
            mPhase = AUDIT_PAGE_SETTLE;
            break;
        }
        case AUDIT_PAGE_SETTLE:
        {
            FrameSample ignored;
            if (isDirectorV2() && mAuditPageTimer.getElapsedTimeF32() >= 120.f)
            {
                LL_WARNS("UILayoutAudit") << "Skipping audit page after 2 minute cap: "
                    << auditPagePath() << LL_ENDL;
                advanceAuditPageOrSize();
                break;
            }
            if (takeFrame(ignored) && ++mAuditSettleFrames >= 5 &&
                mAuditSettleTimer.getElapsedTimeF32() >= 0.3f)
            {
                auditGeometry();
                prepareCapturePositions();
                mCapturePositionIndex = 0;
                mAuditSettleFrames = 0;
                mPhase = AUDIT_CAPTURE_SETTLE;
            }
            else if (isDirectorV2() && mAuditSettleFrames >= 600)
            {
                abortCurrentTarget("layout audit state did not settle after 600 frames: " +
                                   auditStatePath());
            }
            break;
        }
        case AUDIT_CAPTURE_SETTLE:
        {
            FrameSample ignored;
            if (!takeFrame(ignored)) break;
            if (++mAuditSettleFrames < 2) break;
            requestCapture();
            mPhase = AUDIT_CAPTURE_WAIT;
            break;
        }
        case AUDIT_CAPTURE_WAIT:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && mCaptureRequested && ++mCaptureWaitFrames >= 30)
            {
                mCaptureFailed = true;
                mCaptureRequested = false;
                LL_WARNS("UILayoutAudit") << "UILayoutAudit capture timeout "
                    << mTargets[mTargetIndex] << "/" << auditPagePath() << LL_ENDL;
            }
            if (mCaptureRequested) break;
            if (!mCaptureFailed) saveCapturedImage();
            ++mCapturePositionIndex;
            if (mCapturePositionIndex < mCapturePositions.size())
            {
                if (LLScrollContainer* scroller = mCaptureScroller.get())
                {
                    scroller->setDocPosVertical(mCapturePositions[mCapturePositionIndex]);
                }
                mAuditSettleFrames = 0;
                mPhase = AUDIT_CAPTURE_SETTLE;
                break;
            }
            if (isDirectorV2() && ++mKitAuditStateIndex < mKitAuditStates.size())
            {
                mAuditStateApplied = applyKitAuditState();
                std::vector<LLHandle<LLScrollContainer>> scrollers;
                collectAllScrollers(auditPageRoot(), scrollers);
                for (const LLHandle<LLScrollContainer>& handle : scrollers)
                    if (LLScrollContainer* scroller = handle.get()) scroller->goToTop();
                mAuditSettleFrames = 0;
                mAuditSettleTimer.reset();
                mPhase = AUDIT_PAGE_SETTLE;
            }
            else advanceAuditPageOrSize();
            break;
        }
        case AUDIT_RESTORE:
        {
            FrameSample ignored;
            takeFrame(ignored);
            restoreInitialState();
            if (LLFloater* floater = mFloater.get())
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            mPhase = CLOSE;
            break;
        }
        case CLOSE:
            closeCurrentTarget();
            break;
        case CLOSE_SETTLE:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && ++mSettleFrames >= 30)
            {
                mPhase = nextTarget() ? OPEN : NEXT_PASS;
            }
            break;
        }
        case NEXT_PASS:
            if (mAB && mABPass == 0)
            {
                FrameSample ignored;
                takeFrame(ignored);
                mABPass = 1;
                mCullOn = false;
                // ui-draw-lint: allow(setting-write) one-shot self-test toggle, login screen only
                gSavedSettings.setBOOL("UIFolderViewCull", false);
                mSamples.clear();
                mPhase = BASELINE;
            }
            else if (mAB && mABPass == 1 && mInventoryAB)
            {
                FrameSample ignored;
                takeFrame(ignored);
                mABPass = 2;
                mCullOn = true;
                mTargets.assign(1, "inventory_stress");
                // ui-draw-lint: allow(setting-write) one-shot self-test toggle, login screen only
                gSavedSettings.setBOOL("UIFolderViewCull", true);
                mSamples.clear();
                mPhase = BASELINE;
            }
            else
            {
                mPhase = FINISH;
            }
            break;
        case FINISH:
            writeResults();
            restoreDirectorAuditSettings();
            restore();
            sActive = false;
            sLayoutAuditActive = false;
            LLAppViewer::instance()->requestQuit();
            return true;
        }
        return false;
    }

    void writeResults()
    {
        const std::string path = gDirUtilp->getExpandedFilename(
            LL_PATH_LOGS, "ui_perf_selftest.csv");
        llofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        out << "pass,floater,tab_path,measure,frames,ui_mean_ms,ui_p95_ms,ui_max_ms,"
               "views_drawn_mean,views_culled_mean,find_calls_mean,find_visited_mean,"
               "action_ms,first_frame_ui_ms,ready_wait_frames\n";
        out << std::fixed << std::setprecision(4);
        for (const ResultRow& row : mRows)
        {
            out << csv(row.mPass) << ',' << csv(row.mFloater) << ','
                << csv(row.mPath) << ',' << csv(row.mMeasure) << ','
                << row.mFrames << ',' << row.mMean << ',' << row.mP95 << ','
                << row.mMax << ',' << row.mDrawn << ',' << row.mCulled << ','
                << row.mFindCalls << ',' << row.mFindVisited << ','
                << row.mActionMs << ',' << row.mFirstFrameMs << ','
                << row.mReadyWaitFrames << '\n';
        }
        out.flush();
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest summary: wrote "
            << mRows.size() << " rows to " << path << LL_ENDL;
        for (const ResultRow& row : mRows)
        {
            if (row.mMeasure == "baseline" || row.mMeasure == "steady" ||
                row.mMeasure == "tab")
            {
                LL_INFOS("UIPerfSelfTest") << row.mPass << " " << row.mFloater
                    << " " << row.mPath << " " << row.mMeasure
                    << " mean=" << row.mMean << " p95=" << row.mP95
                    << " max=" << row.mMax << LL_ENDL;
            }
        }
        if (mLayout)
        {
            writeLayoutResults();
        }
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest summary complete" << LL_ENDL;
    }

    void writeLayoutResults()
    {
        const std::string path = gDirUtilp->getExpandedFilename(
            LL_PATH_LOGS, "ui_layout_audit.csv");
        llofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        out << "size_pass,floater,page_index,tab_path,finding,widget_a,widget_b,rect_a,rect_b,detail\n";
        for (const LayoutFinding& finding : mFindings)
        {
            out << csv(finding.mSizePass) << ',' << csv(finding.mFloater) << ','
                << finding.mPageIndex << ',' << csv(finding.mPath) << ','
                << csv(finding.mFinding) << ','
                << csv(finding.mWidgetA) << ',' << csv(finding.mWidgetB) << ','
                << csv(rectString(finding.mRectA)) << ','
                << csv(finding.mWidgetB.empty() ? std::string() : rectString(finding.mRectB))
                << ',' << csv(finding.mDetail) << '\n';
        }
        out.flush();

        if (mDirectorV2)
        {
            const std::string coverage_path = gDirUtilp->getExpandedFilename(
                LL_PATH_LOGS, "ui_layout_audit_states.csv");
            llofstream coverage(coverage_path.c_str(), std::ios::out | std::ios::trunc);
            coverage << "size_pass,floater,page_index,tab_path,enumerated,applied\n";
            for (const AuditCoverage& state : mAuditCoverage)
            {
                coverage << csv(state.mSizePass) << ',' << csv(state.mFloater) << ','
                    << state.mPageIndex << ',' << csv(state.mPath) << ','
                    << state.mEnumerated << ',' << (state.mApplied ? 1 : 0) << '\n';
            }
            coverage.flush();
        }

        std::map<std::string, U32> by_page;
        std::map<std::string, U32> by_type;
        for (const LayoutFinding& finding : mFindings)
        {
            ++by_page[finding.mSizePass + " " + finding.mFloater + "/" + finding.mPath];
            ++by_type[finding.mFinding];
        }
        for (const std::string& page : mAuditedPages)
        {
            by_page.try_emplace(page, 0);
        }
        LL_INFOS("UILayoutAudit") << "UILayoutAudit summary begin: "
            << mFindings.size() << " findings; CSV=" << path
            << " screenshots=" << mAuditRoot << LL_ENDL;
        for (const auto& entry : by_type)
        {
            LL_INFOS("UILayoutAudit") << "type " << entry.first << "=" << entry.second << LL_ENDL;
        }
        for (const auto& entry : by_page)
        {
            LL_INFOS("UILayoutAudit") << "page " << entry.first << "=" << entry.second << LL_ENDL;
        }
        LL_INFOS("UILayoutAudit") << "UILayoutAudit summary complete" << LL_ENDL;
    }

    void restore()
    {
        if (mRestored) return;
        mRestored = true;
        for (const std::string& target : mTargets)
        {
            LLFloaterReg::hideInstance(target);
        }
        for (const auto& saved : mWasVisible)
        {
            if (saved.second)
            {
                LLFloaterReg::showInstance(saved.first, LLSD(), false);
            }
        }
        if (mDirectorV2 || mFunctional) restoreSettingsSnapshot();
        else
        {
            // ui-draw-lint: allow(setting-write) one-shot self-test toggle, login screen only
            gSavedSettings.setBOOL("UIFolderViewCull", mPriorCull);
        }
    }

    std::string mRequest;
    U32 mFrames;
    bool mAB;
    bool mLayout;
    U32 mInventoryItems;
    bool mPriorCull;
    bool mDirectorV2 = false;
    bool mFunctional = false;
    bool mInWorld = false;
    bool mFunctionalAutoLoginStarted = false;
    bool mFunctionalAccountVerified = false;
    bool mFunctionalWorldReady = false;
    bool mFunctionalInWorldPrepared = false;
    bool mFunctionalHaveProjector = false;
    bool mFunctionalProjectorSelected = false;
    bool mFunctionalVCamRestored = false;
    U32 mFunctionalWorldFrames = 0;
    std::string mFunctionalAccount;
    S32 mFunctionalWatchdogSeconds = 12 * 60;
    LLFrameTimer mFunctionalLoginTimer;
    LLFrameTimer mFunctionalWatchdog;
    LLFrameTimer mFunctionalWorldSettle;
    LLSD mFunctionalVCamSnapshot;
    std::unique_ptr<ALVCamPageModel> mFunctionalVCamModel;
    LLVector3 mFunctionalVCamExpectedPos;
    LLQuaternion mFunctionalVCamExpectedRot;
    LLReflectionMapManager::CinematicRefreshStatus mFunctionalProbeStatusBefore;
    LLUUID mFunctionalGazeActor;
    LLActorMover::GazeTarget mFunctionalGazeBefore;
    bool mFunctionalHaveGazeBefore = false;
    LLUUID mFunctionalActorMoverId;
    LLSD mFunctionalActorPathBefore;
    LLSD mFunctionalPathExpected;
    bool mFunctionalHaveActorPathBefore = false;
    bool mFunctionalActorPathWalkingBefore = false;
    LLHandle<ALPanelPathEditor> mFunctionalPathPanel;
    LLHandle<LLCheckBoxCtrl> mFunctionalPathEditCheck;
    LLUUID mFunctionalPathTargetBefore;
    bool mFunctionalPathEditBefore = false;
    uuid_vec_t mFunctionalCastSelectionBefore;
    LLHandle<LLScrollListCtrl> mFunctionalCastList;
    bool mFunctionalPathActorWasCast = true;
    LLUUID mFunctionalPathLeaderId;
    LLUUID mFunctionalGhostFixture;
    LLUUID mFunctionalGhostSelectionBefore;
    LLDirectorCast::ActorStyle mFunctionalActorStyleBefore;
    bool mFunctionalHaveActorStyleBefore = false;
    S32 mFunctionalAnimSwitcherSlot = 0;
    ALLocalFogManager::Bank mFunctionalLocalFogBefore;
    S32 mFunctionalLocalFogSelectedBefore = 0;
    bool mFunctionalHaveLocalFogBefore = false;
    bool mFunctionalLoaded = false;
    bool mFunctionalPending = false;
    bool mFunctionalActionWorked = false;
    bool mFunctionalGlobalAssertionsDone = false;
    bool mFunctionalSentinelDone = false;
    bool mFunctionalInjectionBroken = false;
    bool mFunctionalGatePrepared = false;
    bool mFunctionalRigProjectorAbsentBefore = false;
    bool mFunctionalRevealPending = false;
    bool mFunctionalShots = false;
    bool mFunctionalDryReveal = false;
    bool mFunctionalHaveRowFilter = false;
    bool mFunctionalFailureShotPending = false;
    U32 mFunctionalShotWaitFrames = 0;
    LLHandle<LLPanel> mFunctionalLoginPanel;
    LLHandle<LLView> mFunctionalFocusBefore;
    std::vector<LLHandle<LLView>> mFunctionalOpenedCombos;
    bool mFunctionalFocusWasNull = false;
    bool mFunctionalInputStateActive = false;
    LLMouseHandler* mFunctionalAllowedCaptor = nullptr;
    S32 mFunctionalRevealFrames = 0;
    S32 mFunctionalGateFrames = 0;
    std::string mFunctionalManifestPath;
    std::string mFunctionalVersionFilter;
    std::string mFunctionalFilterError;
    std::string mFunctionalLastClickReason;
    std::string mFunctionalRedirectReason;
    std::vector<std::string> mFunctionalPageFilters;
    std::map<std::string, S32> mFunctionalShotQuota;
    std::regex mFunctionalRowFilter;
    std::vector<LLSD> mFunctionalEntries;
    std::vector<FunctionalResult> mFunctionalResults;
    size_t mFunctionalIndex = 0;
    LLView* mFunctionalPageRoot = nullptr;
    S32 mFunctionalScreenX = 0;
    S32 mFunctionalScreenY = 0;
    LLHandle<LLView> mFunctionalClickTarget;
    LLHandle<LLFloater> mFunctionalHostFloater;
    LLHandle<LLUICtrl> mFunctionalControl;
    LLSD mFunctionalControlBefore;
    LLSD mFunctionalRadioExpected;
    LLSD mFunctionalListExpected;
    LLSD mFunctionalDanceBefore;
    bool mFunctionalDanceEnabledBefore = false;
    bool mFunctionalDanceFrozenBefore = false;
    F64 mFunctionalDanceRampBefore = 1.0;
    U64 mFunctionalDanceStatusRevisionBefore = 0;
    LLSD mFunctionalOwnerModelBefore;
    LLActorMover::GazeTarget mFunctionalCastGazeBefore;
    bool mFunctionalCastLookBefore = false;
    bool mFunctionalHaveCastGazeBefore = false;
    bool mFunctionalGazeEnabledBefore = false;
    LLSD mFunctionalGazeEditActorBefore;
    LLHandle<LLComboBox> mFunctionalGazeEditActor;
    LLUUID mFunctionalGazeCastFixture;
    LLUUID mFunctionalGazeCastFixtureAlternate;
    bool mFunctionalCastLookOnlyBefore = false;
    bool mFunctionalHaveCastLookBefore = false;
    bool mFunctionalDanceActorWasCast = true;
    bool mFunctionalDanceFixturePrepared = false;
    LLPointer<LLViewerObject> mFunctionalDanceSelectedObject;
    LLWString mFunctionalClipboardBefore;
    bool mFunctionalClipboardHadText = false;
    bool mFunctionalClipboardPrepared = false;
    LLHandle<AnimationExplorer> mFunctionalExplorer;
    LLHandle<LLScrollListCtrl> mFunctionalExplorerList;
    LLUUID mFunctionalExplorerFixture;
    LLUUID mFunctionalExplorerSelectionBefore;
    bool mFunctionalExplorerWasVisible = false;
    bool mFunctionalGhostShowAllBefore = true;
    bool mFunctionalGhostEditingBefore = false;
    bool mFunctionalHaveGhostOwnerBefore = false;
    LLUUID mFunctionalGhostEditOwnerBefore;
    bool mFunctionalFlycamFixture = false;
    std::vector<LLUUID> mFunctionalPropFixtures;
    LLPointer<LLVOVolume> mFunctionalPropObject;
    bool mFunctionalPropSelectionAdded = false;
    LLUUID mFunctionalRigProjectorCapture;
    LLSD mFunctionalOriginalControlValue;
    LLHandle<LLUICtrl> mFunctionalSecondaryControl;
    LLSD mFunctionalSecondaryBefore;
    std::string mFunctionalExercise;
    std::string mFunctionalHandlerName;
    std::string mFunctionalSettingName;
    bool mFunctionalDynamicSetting = false;
    boost::signals2::connection mFunctionalCommitConnection;
    S32 mFunctionalCommitCount = 0;
    S32 mFunctionalHandlerRunCount = 0;
    U64 mFunctionalHistoryBefore = 0;
    LLSD mFunctionalObserved = LLSD::emptyMap();
    std::vector<std::pair<LLHandle<LLUICtrl>, LLSD>> mFunctionalRevealWidgetValues;
    LLControlVariable* mFunctionalSetting = nullptr;
    LLSD mFunctionalBefore;
    LLSD mFunctionalConsumerBefore;
    std::string mFunctionalVisibleBefore;
    std::map<std::string, SavedControlState> mFunctionalRestore;
    std::map<std::string, SavedControlState> mFunctionalShaderRestore;
    bool mFunctionalShaderSkipBefore = false;
    std::set<LLFloater*> mFunctionalFloatersBefore;
    LLHandle<LLFloater> mFunctionalRedirectFloater;
    LLHandle<LLFloater> mFunctionalLauncherFloater;
    bool mFunctionalLauncherWasOpen = false;
    LLSD mFunctionalRigSnapshot;
    LLSD mFunctionalRigRowSnapshot;
    SavedControlState mFunctionalShadowSnapshot;
    bool mFunctionalHaveShadowSnapshot = false;
    bool mFunctionalRigRestored = false;
    std::vector<FunctionalDiskSnapshot> mFunctionalUserFiles;
    bool mFunctionalUserFilesRestored = false;
    std::vector<AuditSize> mAuditSizes;
    bool mCullOn = true;
    U32 mABPass = 0;
    bool mInventoryAB = false;
    bool mRestored = false;
    Phase mPhase = WAIT_SETTLE;
    LLFrameTimer mSettle;
    std::vector<std::string> mTargets;
    std::map<std::string, bool> mWasVisible;
    std::vector<ResultRow> mRows;
    std::vector<FrameSample> mSamples;
    LLHandle<LLFloater> mFloater;
    LLRect mOriginalFloaterRect;
    std::vector<TabTarget> mTabs;
    std::vector<ScrollTarget> mDefaultScrollers;
    std::vector<ScrollTarget> mScrollers;
    std::vector<std::pair<LLHandle<LLTabContainer>, S32>> mInitialTabs;
    std::vector<std::pair<LLHandle<LLScrollContainer>, S32>> mInitialScroll;
    S32 mTargetIndex = -1;
    size_t mTabIndex = 0;
    size_t mScrollIndex = 0;
    bool mInDefaultScroll = false;
    U32 mSettleFrames = 0;
    U32 mReadyWaitFrames = 0;
    bool mReadyWaitLogged = false;
    F64 mActionMs = 0.0;
    F64 mFirstFrameMs = 0.0;
    bool mPendingKitBuild = false;
    bool mKitMetricsCollected = false;
    LLFrameTimer mUiFrameStall;
    bool mUiFrameStallWarned = false;
    OverlapPolicy mOverlapPolicy;
    std::vector<LayoutFinding> mFindings;
    std::set<std::string> mFindingKeys;
    std::set<std::string> mAuditedPages;
    std::vector<AuditCoverage> mAuditCoverage;
    std::set<std::string> mAuditCoverageKeys;
    std::string mAuditRoot;
    U32 mAuditSizeIndex = 0;
    size_t mAuditPageIndex = 0;
    std::vector<KitAuditState> mKitAuditStates;
    size_t mKitAuditStateIndex = 0;
    S32 mKitDefaultMode = 0;
    S32 mKitDefaultChip = 0;
    std::vector<KitCardDefault> mKitDefaultCards;
    std::map<std::string, KitPageDefaults> mKitPageDefaults;
    bool mAuditStateApplied = false;
    std::map<std::string, LLSD> mDirectorAuditSettings;
    std::map<std::string, SavedControlState> mSettingsSnapshot;
    std::set<LLView*> mMeasuredKitBuilds;
    U32 mAuditSettleFrames = 0;
    LLFrameTimer mAuditSettleTimer;
    LLFrameTimer mAuditPageTimer;
    LLHandle<LLScrollContainer> mCaptureScroller;
    std::vector<S32> mCapturePositions;
    size_t mCapturePositionIndex = 0;
    bool mCaptureRequested = false;
    bool mCaptureFailed = false;
    U32 mCaptureWaitFrames = 0;
    std::string mCaptureFilename;
    LLPointer<LLImageRaw> mCapturedImage;
};

static std::unique_ptr<Runner> sRunner;
}

void ALUIPerfSelfTest::startIfRequested()
{
    static LLCachedControl<std::string> request(
        gSavedSettings, "UIPerfSelfTest", std::string());
    static LLCachedControl<std::string> audit_sizes(
        gSavedSettings, "UILayoutAuditSizes", std::string());
    const bool inworld_carrier = audit_sizes().compare(
        0, std::string("functional-inworld|").size(), "functional-inworld|") == 0;
    const std::string effective_request = inworld_carrier ? audit_sizes() : request();
    if (effective_request.empty() || sActive)
    {
        return;
    }
    static LLCachedControl<U32> frames(gSavedSettings, "UIPerfSelfTestFrames", 120);
    static LLCachedControl<bool> ab(gSavedSettings, "UIPerfSelfTestAB", false);
    static LLCachedControl<bool> layout(gSavedSettings, "UILayoutAudit", false);
    static LLCachedControl<U32> inventory_items(gSavedSettings, "UIPerfInventoryStress", 0);
    sActive = true;
    sSuppressSettingsSave = true;
    sLayoutAuditActive = layout;
    sRunner = std::make_unique<Runner>(effective_request, frames, ab, layout,
                                       inventory_items,
                                       inworld_carrier ? std::string() : audit_sizes());
}

void ALUIPerfSelfTest::tick()
{
    if (!sRunner)
    {
        return;
    }
    if (LLStartUp::getStartupState() > STATE_LOGIN_WAIT && !sRunner->inWorld())
    {
        sRunner->abortForLogin();
        sRunner.reset();
        return;
    }
    if (sRunner->tick())
    {
        sRunner.reset();
    }
}

void ALUIPerfSelfTest::recordUIDraw(F64 milliseconds)
{
    if (sActive)
    {
        sPendingUiMs += milliseconds;
        ++sPendingUiCalls;
    }
}

void ALUIPerfSelfTest::recordViewCounts(U32 drawn, U32 culled,
                                        U32 find_calls, U32 find_visited)
{
    if (sActive)
    {
        sPendingDrawn += drawn;
        sPendingCulled += culled;
        sPendingFindCalls += find_calls;
        sPendingFindVisited += find_visited;
    }
}

void ALUIPerfSelfTest::captureAfterRender()
{
    if (sRunner)
    {
        sRunner->captureAfterRender();
    }
}

bool ALUIPerfSelfTest::layoutAuditActive()
{
    return sLayoutAuditActive;
}

void ALUIPerfSelfTest::recordKitText(LLView* view, const std::string& text,
                                     const LLRect& local_rect, LLView* owner)
{
    if (!sLayoutAuditActive || !view || text.empty() || !view->getVisible() || !view->isInVisibleChain() ||
        !isWidgetOnScreen(view) || LLView::getDrawContext().mAlpha <= 0.f ||
        local_rect.getWidth() <= 0 || local_rect.getHeight() <= 0)
        return;
    beginKitTextFrame();
    LLRect screen;
    view->localRectToScreen(local_rect, &screen);
    LLRect widget_screen;
    view->localRectToScreen(view->getLocalRect(), &widget_screen);
    if (!clipRectTo(screen, widget_screen))
        return;
    for (LLView* ancestor = view->getParent(); ancestor; ancestor = ancestor->getParent())
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(ancestor))
        {
            LLRect clip_screen;
            scroller->localRectToScreen(scroller->getVisibleContentRect(), &clip_screen);
            if (!clipRectTo(screen, clip_screen))
                return;
        }
    for (const KitTextRun& run : sKitTextRuns)
        if (run.mView.get() == view && run.mText == text && run.mRect == screen)
            return;
    sKitTextRuns.push_back({ view->getHandle(), owner ? owner->getHandle() : LLHandle<LLView>(), text, screen });
}

void ALUIPerfSelfTest::recordKitWidgetTree(LLView* root)
{
    if (!sLayoutAuditActive || !root)
        return;
    beginKitTextFrame();
    std::function<void(LLView*)> visit = [&](LLView* view)
    {
        if (!view || !view->getVisible() || !view->isInVisibleChain() || !isWidgetOnScreen(view) ||
            !sKitRecordedViews.insert(view).second)
            return;

        if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(view))
            if (control->hasTabStop() || dynamic_cast<LLButton*>(view) || dynamic_cast<LLLineEditor*>(view) ||
                dynamic_cast<LLComboBox*>(view) || dynamic_cast<LLSlider*>(view) || dynamic_cast<LLSpinCtrl*>(view))
            {
                LLRect screen;
                view->localRectToScreen(view->getLocalRect(), &screen);
                sKitInteractiveRects.push_back({ view->getHandle(), screen });
            }

        std::string text;
        const LLFontGL* font  = view->getFont();
        LLFontGL::HAlign align = LLFontGL::LEFT;
        S32 pad_left = 0, pad_right = 0;
        LLView* owner = nullptr;
        bool full_rect = false;
        if (LLButton* button = dynamic_cast<LLButton*>(view))
        {
            text      = button->getCurrentLabel().getString();
            align     = button->getHAlign();
            pad_left  = button->getLeftHPad();
            pad_right = button->getRightHPad();
            owner     = button;
        }
        else if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(view))
        {
            text  = editor->getText();
            align = editor->getHAlign();
            editor->getTextPadding(&pad_left, &pad_right);
            owner = editor;
        }
        else if (LLTextBox* box = dynamic_cast<LLTextBox*>(view))
        {
            text      = box->getText();
            align     = box->getHAlign();
            full_rect = box->getWordWrap();
        }
        else if (LLTextEditor* editor = dynamic_cast<LLTextEditor*>(view))
        {
            text      = editor->getText();
            full_rect = true;
        }

        if (!owner)
            for (LLView* ancestor = view->getParent(); ancestor && ancestor != root; ancestor = ancestor->getParent())
                if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(ancestor);
                    control && (control->hasTabStop() || dynamic_cast<LLButton*>(control) ||
                                dynamic_cast<LLComboBox*>(control) || dynamic_cast<LLSpinCtrl*>(control)))
                {
                    owner = control;
                    break;
                }

        F32 text_alpha = LLView::getDrawContext().mAlpha;
        if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(view))
            text_alpha *= llmax(editor->getReadOnlyFgColor().mV[VALPHA], editor->getFgColor().mV[VALPHA]);
        if (font && !text.empty() && text_alpha > 0.f)
        {
            const S32 available = llmax(1, view->getRect().getWidth() - pad_left - pad_right);
            const S32 width     = llmin(available, longestLineWidth(font, text));
            S32 height = llmin(view->getRect().getHeight(), font->getLineHeight());
            if (full_rect)
            {
                height = view->getRect().getHeight();
                if (LLTextBox* box = dynamic_cast<LLTextBox*>(view))
                    height = llmin(height, llmax(font->getLineHeight(), box->getTextPixelHeight()));
            }
            S32 left = pad_left;
            if (align == LLFontGL::RIGHT)
                left = view->getRect().getWidth() - pad_right - width;
            else if (align == LLFontGL::HCENTER)
                left = pad_left + (available - width) / 2;
            const S32 bottom = (view->getRect().getHeight() - height) / 2;
            recordKitText(view, text, LLRect(left, bottom + height, left + width, bottom), owner);
        }

        for (LLView* child : *view->getChildList())
            visit(child);
    };
    visit(root);
}

bool ALUIPerfSelfTest::suppressSettingsSave()
{
    return sSuppressSettingsSave;
}

bool ALUIPerfSelfTest::suppressCredentialSave()
{
    return sActive && sRunner && sRunner->inWorld();
}

bool ALUIPerfSelfTest::isActive()
{
    return sActive;
}
