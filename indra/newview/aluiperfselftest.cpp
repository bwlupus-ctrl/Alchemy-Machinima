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

#include "llappviewer.h"
#include "llaccordionctrl.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llerrorcontrol.h"
#include "llfile.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfolderview.h"
#include "llfolderviewitem.h"
#include "llfolderviewmodel.h"
#include "lliconctrl.h"
#include "llimage.h"
#include "llimagepng.h"
#include "llsdjson.h"
#include "llfontregistry.h"
#include "lllineeditor.h"
#include "llmultisliderctrl.h"
#include "llpanel.h"
#include "llscrollcontainer.h"
#include "llscrolllistctrl.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llsdutil.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llview.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>

namespace
{
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
        if (mDirectorV2)
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
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest waiting 3 seconds at login screen" << LL_ENDL;
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
        restoreDirectorAuditSettings();
        restoreInitialState();
        restore();
        sActive = false;
        sSuppressSettingsSave = false;
        sLayoutAuditActive = false;
    }

    bool tick() { return step(); }

    void captureAfterRender()
    {
        if (!mCaptureRequested || mCapturedImage.notNull()) return;
        LLFloater* floater = mFloater.get();
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
                if (!in_floater(second) || sKitTextRuns[i].mText == sKitTextRuns[j].mText ||
                    !sKitTextRuns[i].mRect.overlaps(sKitTextRuns[j].mRect))
                    continue;
                AuditLeaf a = text_leaf(sKitTextRuns[i]);
                AuditLeaf b = text_leaf(sKitTextRuns[j]);
                addFinding("TEXT_ON_TEXT", a, &b,
                           "drawn strings intersect: " + sKitTextRuns[i].mText + " / " + sKitTextRuns[j].mText);
            }

            for (const KitInteractiveRect& interactive : sKitInteractiveRects)
            {
                LLView* control = interactive.mView.get();
                LLView* owner   = sKitTextRuns[i].mOwner.get();
                if (!in_floater(control) || control == owner || control == first ||
                    (owner && owner->hasAncestor(control)) ||
                    first->hasAncestor(control) || !sKitTextRuns[i].mRect.overlaps(interactive.mRect))
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
            if (!visibleWithin(combo, floater))
                continue;
            ++visible_combos;
            LLRect rect;
            combo->localRectToOtherView(combo->getLocalRect(), &rect, floater);
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
        if (mDirectorV2) restoreSettingsSnapshot();
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
    if (request().empty() || sActive)
    {
        return;
    }
    static LLCachedControl<U32> frames(gSavedSettings, "UIPerfSelfTestFrames", 120);
    static LLCachedControl<bool> ab(gSavedSettings, "UIPerfSelfTestAB", false);
    static LLCachedControl<bool> layout(gSavedSettings, "UILayoutAudit", false);
    static LLCachedControl<std::string> audit_sizes(
        gSavedSettings, "UILayoutAuditSizes", std::string());
    static LLCachedControl<U32> inventory_items(gSavedSettings, "UIPerfInventoryStress", 0);
    sActive = true;
    sSuppressSettingsSave = true;
    sLayoutAuditActive = layout;
    sRunner = std::make_unique<Runner>(request(), frames, ab, layout,
                                       inventory_items, audit_sizes());
}

void ALUIPerfSelfTest::tick()
{
    if (!sRunner)
    {
        return;
    }
    if (LLStartUp::getStartupState() > STATE_LOGIN_WAIT)
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
    if (!sLayoutAuditActive || !view || text.empty() || local_rect.getWidth() <= 0 || local_rect.getHeight() <= 0)
        return;
    beginKitTextFrame();
    LLRect screen;
    view->localRectToScreen(local_rect, &screen);
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

        if (font && !text.empty())
        {
            const S32 available = llmax(1, view->getRect().getWidth() - pad_left - pad_right);
            const S32 width     = full_rect ? available : llmin(available, font->getWidth(text));
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

bool ALUIPerfSelfTest::isActive()
{
    return sActive;
}
