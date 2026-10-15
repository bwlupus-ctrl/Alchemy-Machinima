#include "llviewerprecompiledheaders.h"
#include "alkitshell.h"
#include "aluiperfselftest.h"
#include "alkitbar.h"
#include "alkitbinding.h"
#include "alkitcard.h"
#include "alkitlazypage.h"
#include "alkitpage.h"
#include "alkitpopover.h"
#include "alkitrow.h"
#include "alkitstyle.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llaccordionctrltab.h"
#include "lleventtimer.h"
#include "llfloater.h"
#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llfontgl.h"
#include "lllineeditor.h"
#include "llrender.h"
#include "llsliderwheel.h"
#include "llscrolllistitem.h"
#include "llscrollcontainer.h"
#include "llstring.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include <algorithm>
#include <functional>
#include <utility>
static LLDefaultChildRegistry::Register<ALKitShell> r("kit_shell");

namespace
{
template<typename T>
T* findFirst(LLView* root)
{
    if (!root)
        return nullptr;
    if (T* result = dynamic_cast<T*>(root))
        return result;
    for (LLView* child : *root->getChildList())
        if (T* result = findFirst<T>(child))
            return result;
    return nullptr;
}

bool selectTabRecursive(LLView* root, const std::string& tab_name)
{
    if (!root || tab_name.empty())
        return false;
    if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(root))
        if (tabs->selectTabByName(tab_name))
            return true;
    for (LLView* child : *root->getChildList())
        if (selectTabRecursive(child, tab_name))
            return true;
    return false;
}

LLView* findExternalTarget(LLView* root, const std::string& widget,
                           const std::string& setting)
{
    if (!root)
        return nullptr;
    if (!widget.empty())
        if (LLView* named = root->findChild<LLView>(widget, true))
            return named;
    LLControlVariable* saved = setting.empty() ? nullptr : gSavedSettings.getControl(setting).get();
    LLControlVariable* account = setting.empty() ? nullptr : gSavedPerAccountSettings.getControl(setting).get();
    if (LLUICtrl* control = dynamic_cast<LLUICtrl*>(root))
        if (control->getControlVariable() == saved || control->getControlVariable() == account)
            return control;
    for (LLView* child : *root->getChildList())
        if (LLView* found = findExternalTarget(child, std::string(), setting))
            return found;
    return nullptr;
}

void centerInAncestorScroller(LLView* target)
{
    for (LLView* ancestor = target ? target->getParent() : nullptr; ancestor; ancestor = ancestor->getParent())
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(ancestor))
        {
            LLView* document = scroller->getScrolledView();
            LLRect rect;
            if (document && target->localRectToOtherView(target->getLocalRect(), &rect, document))
            {
                const LLRect visible = scroller->getVisibleContentRect();
                const S32 center = visible.getHeight() / 2;
                const S32 half = llmax(1, rect.getHeight() / 2);
                scroller->scrollToShowRect(rect,
                    LLRect(0, center + half, visible.getWidth(), center - half));
            }
            return;
        }
}

class ALExternalSearchHighlight final : public LLPanel
{
public:
    explicit ALExternalSearchHighlight(const LLPanel::Params& params) : LLPanel(params) {}
    void pulse(LLView* target)
    {
        mTarget = target ? target->getHandle() : LLHandle<LLView>();
        mTimer.reset();
        setVisible(target != nullptr);
    }
    void draw() override
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
        LLView* target = mTarget.get();
        const F32 elapsed = mTimer.getElapsedTimeF32();
        if (!target || elapsed >= ALKitStyle::FLASH_SEC)
        {
            setVisible(false);
            return;
        }
        LLRect rect;
        if (target->localRectToOtherView(target->getLocalRect(), &rect, this))
        {
            LLColor4 color = ALKitStyle::color(ALKitStyle::ACCENT).get();
            color.mV[VALPHA] *= 1.f - elapsed / ALKitStyle::FLASH_SEC;
            gl_rect_2d(rect, color, false);
        }
    }
private:
    LLHandle<LLView> mTarget;
    LLFrameTimer     mTimer;
};

void pulseExternalTarget(LLFloater* floater, LLView* target)
{
    if (!floater || !target)
        return;
    ALExternalSearchHighlight* highlight = floater->findChild<ALExternalSearchHighlight>("director_search_highlight", false);
    if (!highlight)
    {
        LLPanel::Params params;
        params.name = "director_search_highlight";
        params.rect = floater->getLocalRect();
        params.follows.flags = FOLLOWS_ALL;
        params.mouse_opaque = false;
        highlight = new ALExternalSearchHighlight(params);
        floater->addChild(highlight);
    }
    floater->sendChildToFront(highlight);
    highlight->pulse(target);
}

std::string ellipsizeText(const LLFontGL* font, const std::string& text, S32 max_pixels)
{
    if (!font || text.empty() || max_pixels <= 0)
        return std::string();
    if (font->getWidth(text) <= max_pixels)
        return text;
    static const std::string ellipsis = "\xE2\x80\xA6";
    const S32 ellipsis_width = font->getWidth(ellipsis);
    if (ellipsis_width > max_pixels)
        return std::string();
    const S32 text_pixels = max_pixels - ellipsis_width;
    const LLWString wide = utf8str_to_wstring(text);
    const S32 chars = font->maxDrawableChars(wide.c_str(), (F32)text_pixels,
                                              (S32)wide.size(), LLFontGL::ANYWHERE);
    return wstring_to_utf8str(wide.substr(0, llmax(0, chars))) + ellipsis;
}

} // namespace

class ALKitSearchResultRow final : public LLPanel
{
public:
    using commit_fn_t = std::function<void()>;

    ALKitSearchResultRow(const LLPanel::Params& params, std::string label,
                         std::string context, std::string detail, commit_fn_t commit)
        : LLPanel(params),
          mLabel(std::move(label)),
          mContext(std::move(context)),
          mDetail(std::move(detail)),
          mCommit(std::move(commit))
    {
        setMouseOpaque(true);
        mLabelFont = ALKitStyle::sansFont("Kit12");
        mDetailFont = ALKitStyle::monoFont("Kit11");
        updateTextLayout(getRect().getWidth());
    }

    void setSelected(bool selected) { mSelected = selected; }

    void reshape(S32 width, S32 height, bool called_from_parent = true) override
    {
        LLPanel::reshape(width, height, called_from_parent);
        if (width != mLayoutWidth)
            updateTextLayout(width);
    }

    void draw() override
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
        if (mHovered || mSelected)
            gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::ROW_HOVER).get(), true);

        constexpr S32 LEFT = 8;
        if (mLabelFont)
        {
            mLabelFont->renderUTF8(mDrawLabel, 0, (F32)LEFT, 25.f,
                                   ALKitStyle::color(ALKitStyle::TEXT).get(), LLFontGL::LEFT,
                                   LLFontGL::VCENTER, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
            if (!mDrawContext.empty())
            {
                const S32 context_x = LEFT + mLabelWidth + 4;
                mLabelFont->renderUTF8(mDrawContext, 0, (F32)context_x, 25.f,
                                       ALKitStyle::color(ALKitStyle::MUTED).get(), LLFontGL::LEFT,
                                       LLFontGL::VCENTER, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
            }
        }
        if (mDetailFont && !mDrawDetail.empty())
            mDetailFont->renderUTF8(mDrawDetail, 0, (F32)LEFT, 10.f,
                                    ALKitStyle::color(ALKitStyle::MUTED).get(), LLFontGL::LEFT,
                                    LLFontGL::VCENTER, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
        LLPanel::draw();
    }

    bool handleMouseDown(S32, S32, MASK) override
    {
        if (mCommit)
            mCommit();
        return true;
    }

    void onMouseEnter(S32 x, S32 y, MASK mask) override
    {
        mHovered = true;
        LLPanel::onMouseEnter(x, y, mask);
    }

    void onMouseLeave(S32 x, S32 y, MASK mask) override
    {
        mHovered = false;
        LLPanel::onMouseLeave(x, y, mask);
    }

private:
    void updateTextLayout(S32 width)
    {
        constexpr S32 LEFT = 8;
        constexpr S32 RIGHT = 8;
        mLayoutWidth = width;
        const S32 available = llmax(1, width - LEFT - RIGHT);
        const S32 context_width = mLabelFont ? mLabelFont->getWidth(mContext) : 0;
        const S32 label_limit = mContext.empty() ? available
            : llmax(40, available - llmin(context_width, available / 2));
        mDrawLabel = ellipsizeText(mLabelFont, mLabel, label_limit);
        mLabelWidth = mLabelFont ? mLabelFont->getWidth(mDrawLabel) : 0;
        const S32 context_limit = llmax(1, available - mLabelWidth - 4);
        mDrawContext = ellipsizeText(mLabelFont, mContext, context_limit);
        mDrawDetail = ellipsizeText(mDetailFont, mDetail, available);
    }

    std::string mLabel;
    std::string mContext;
    std::string mDetail;
    std::string mDrawLabel;
    std::string mDrawContext;
    std::string mDrawDetail;
    commit_fn_t mCommit;
    const LLFontGL* mLabelFont = nullptr;
    const LLFontGL* mDetailFont = nullptr;
    S32         mLayoutWidth = -1;
    S32         mLabelWidth = 0;
    bool        mHovered = false;
    bool        mSelected = false;
};

namespace
{
std::string searchDisplayText(std::string value)
{
    std::replace_if(value.begin(), value.end(),
                    [](unsigned char ch) { return ch < 0x20 || ch == 0x7f; }, ' ');
    static const char* unsupported_spaces[] = {
        "\xE2\x80\x80", "\xE2\x80\x81", "\xE2\x80\x82", "\xE2\x80\x83",
        "\xE2\x80\x84", "\xE2\x80\x85", "\xE2\x80\x86", "\xE2\x80\x87",
        "\xE2\x80\x88", "\xE2\x80\x89", "\xE2\x80\x8A", "\xE2\x80\x8B"
    };
    for (const char* separator : unsupported_spaces)
        LLStringUtil::replaceString(value, separator, " ");
    return value;
}

class ALKitSearchGroupRow final : public LLPanel
{
public:
    ALKitSearchGroupRow(const LLPanel::Params& params, std::string label)
        : LLPanel(params), mLabel(std::move(label))
    {
        setMouseOpaque(false);
    }

    void draw() override
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
        if (const LLFontGL* font = ALKitStyle::sansFont("Kit10", true))
            font->renderUTF8(mLabel, 0, 8.f, 9.f, ALKitStyle::color(ALKitStyle::MUTED).get(),
                             LLFontGL::LEFT, LLFontGL::VCENTER, LLFontGL::NORMAL,
                             LLFontGL::NO_SHADOW, S32_MAX, llmax(1, getRect().getWidth() - 16),
                             nullptr, true);
        LLPanel::draw();
    }

private:
    std::string mLabel;
};
} // namespace

ALKitShell::ALKitShell(const Params& p) : LLPanel(p), mIndexFile(p.index_file)
{
    for (const RailItem& item : p.rail_item)
    {
        mRailData.push_back({ item.group, item.label, item.icon, item.master_settings });
    }
    mHistory.setOwner(this);
}

ALKitShell::~ALKitShell()
{
    if (auto timer = mSearchDebounce.lock())
        delete timer.get();
}

bool ALKitShell::postBuild()
{
    if (!LLPanel::postBuild())
        return false;
    mDirectorTabs = findChild<LLTabContainer>("director_tabs");
    mRoster       = findChild<LLPanel>("kit_cast_roster");
    mTransport    = findChild<LLPanel>("kit_transport");
    mTransportCut = findChild<LLButton>("btn_cut");
    mTransportArm = findChild<LLButton>("btn_arm");
    mRosterOpen   = gSavedSettings.getBOOL("DirectorV2CastRosterOpen");
    const LLSD stored_favorites = gSavedSettings.getLLSD("DirectorFavorites");
    mFavorites = ALKitQol::favoritesFromLLSD(stored_favorites);
    const LLSD cleaned_favorites = ALKitQol::favoritesToLLSD(mFavorites);
    if (!llsd_equals(stored_favorites, cleaned_favorites))
        gSavedSettings.setLLSD("DirectorFavorites", cleaned_favorites);
    buildChrome();
    mConnections.emplace_back(setPageBuiltCallback([this](const std::string& name, LLPanel* panel) { pageBuilt(name, panel); }));
    layoutChrome();
    ALKitStyle::styleTree(this);
    if (LLButton* cast_chip = findChild<LLButton>("btn_cast_chip"))
    {
        ALKitStyle::stylePill(cast_chip);
        cast_chip->setFont(ALKitStyle::sansFont("Kit12", true));
        cast_chip->setUnselectedLabelColor(ALKitStyle::color(ALKitStyle::TEXT_STRONG));
    }
    for (LLButton* button : mRailButtons)
        button->setSelectedLabelColor(ALKitStyle::color(ALKitStyle::ACCENT));
    for (LLButton* button : mTabButtons)
    {
        button->setUnselectedLabelColor(ALKitStyle::color(ALKitStyle::DIM));
        button->setSelectedLabelColor(ALKitStyle::color(ALKitStyle::TEXT_STRONG));
    }
    return true;
}

void ALKitShell::setReady()
{
    if (mReady)
        return;
    // XUI tab containers select their first child while the tree is still being
    // constructed.  Deferring this transition until the floater has installed
    // its page-built binder is what makes the first page obey the same lazy
    // build contract as every later selection.
    layoutChrome();
    mReady = true;
    LLPanel* group = mDirectorTabs ? mDirectorTabs->getCurrentPanel() : nullptr;
    LLTabContainer* tabs = findFirst<LLTabContainer>(group);
    ALKitLazyPage* page = tabs ? dynamic_cast<ALKitLazyPage*>(tabs->getCurrentPanel()) : nullptr;
    if (page)
        page->ensureBuilt();
    syncSelection();
}

void ALKitShell::buildChrome()
{
    LLPanel::Params panel_params;
    panel_params.mouse_opaque = false;
    panel_params.rect         = LLRect(0, 1, 1, 0);
    panel_params.name         = "kit_rail";
    mRail                     = LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mRail);
    panel_params.name = "kit_tabstrip";
    mTabStrip         = LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mTabStrip);
    panel_params.name = "kit_help";
    mHelpPanel        = LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mHelpPanel);
    panel_params.name = "kit_help_strip";
    mHelpStrip        = LLUICtrlFactory::create<LLPanel>(panel_params);
    addChild(mHelpStrip);
    mHelp.attach(mHelpPanel, mHelpStrip);

    LLButton::Params arrow_params;
    arrow_params.name  = "kit_tab_prev";
    arrow_params.label = "<";
    arrow_params.rect  = LLRect(0, 28, 24, 0);
    mTabPrev           = LLUICtrlFactory::create<LLButton>(arrow_params);
    mTabStrip->addChild(mTabPrev);
    arrow_params.name  = "kit_tab_next";
    arrow_params.label = ">";
    mTabNext           = LLUICtrlFactory::create<LLButton>(arrow_params);
    mTabStrip->addChild(mTabNext);
    LLHandle<ALKitShell> tab_handle = getDerivedHandle<ALKitShell>();
    mTabPrev->setCommitCallback(
        [tab_handle](LLUICtrl*, const LLSD&)
        {
            if (ALKitShell* shell = tab_handle.get())
            {
                shell->mTabOffset = llmax(0, shell->mTabOffset - 1);
                shell->layoutChrome();
            }
        });
    mTabNext->setCommitCallback(
        [tab_handle](LLUICtrl*, const LLSD&)
        {
            if (ALKitShell* shell = tab_handle.get())
            {
                ++shell->mTabOffset;
                shell->layoutChrome();
            }
        });

    ALKitPopover::Params search_params;
    search_params.name    = "kit_search_results";
    search_params.rect    = LLRect(0, 390, 430, 0);
    search_params.side    = "below";
    search_params.visible = false;
    mSearchPopover        = LLUICtrlFactory::create<ALKitPopover>(search_params);
    addChild(mSearchPopover);
    mSearchPopover->setFocusLostCallback(
        [tab_handle](LLFocusableElement*)
        {
            doOnIdleOneTime([tab_handle]()
            {
                ALKitShell* shell = tab_handle.get();
                if (!shell || !shell->mSearchPopover)
                    return;
                LLView* focus = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
                if (focus && (focus->hasAncestor(shell->mSearchPopover) ||
                              dynamic_cast<ALKitSearchEditor*>(focus)))
                    return;
                shell->setSearchPopupArmed(nullptr, false);
            });
        });
    LLTextBox::Params text_params;
    text_params.name = "kit_search_header";
    text_params.rect = LLRect(10, 380, 420, 358);
    mSearchHeader    = LLUICtrlFactory::create<LLTextBox>(text_params);
    mSearchPopover->addChild(mSearchHeader);
    LLScrollContainer::Params scroll_params;
    scroll_params.name = "kit_search_scroller";
    scroll_params.rect = LLRect(10, 354, 420, 54);
    scroll_params.is_opaque = false;
    scroll_params.reserve_scroll_corner = false;
    mSearchScroller = LLUICtrlFactory::create<LLScrollContainer>(scroll_params);
    mSearchPopover->addChild(mSearchScroller);
    LLPanel::Params results_params;
    results_params.name = "kit_search_results_panel";
    results_params.rect = LLRect(0, 1, 390, 0);
    results_params.background_visible = false;
    mSearchResultsPanel = LLUICtrlFactory::create<LLPanel>(results_params);
    mSearchScroller->addChild(mSearchResultsPanel);
    text_params.name         = "kit_search_footer";
    text_params.rect         = LLRect(10, 48, 420, 20);
    text_params.use_ellipses = true;
    mSearchFooter            = LLUICtrlFactory::create<LLTextBox>(text_params);
    mSearchPopover->addChild(mSearchFooter);

    if (LLButton* cast_chip = findChild<LLButton>("btn_cast_chip"))
    {
        ALKitPopover::Params params;
        params.name    = "kit_cast_popover";
        params.rect    = LLRect(0, 360, 300, 0);
        params.side    = "above";
        params.visible = false;
        mCastPopover   = LLUICtrlFactory::create<ALKitPopover>(params);
        addChild(mCastPopover);
        mCastPopover->bindAnchor(cast_chip);
        LLHandle<ALKitPopover> handle = mCastPopover->getDerivedHandle<ALKitPopover>();
        cast_chip->setCommitCallback(
            [handle](LLUICtrl*, const LLSD&)
            {
                if (ALKitPopover* popover = handle.get())
                    popover->toggle();
            });
    }

    struct PopoverButton
    {
        const char* button;
        const char* popover;
    };
    static const PopoverButton popover_buttons[] = { { "btn_arm", "kit_arm_popover" },
                                                      { "btn_scene_menu", "kit_scene_popover" } };
    for (const PopoverButton& binding : popover_buttons)
    {
        LLButton*     button  = findChild<LLButton>(binding.button);
        ALKitPopover* popover = findChild<ALKitPopover>(binding.popover);
        if (!button || !popover)
            continue;
        LLHandle<ALKitPopover> handle = popover->getDerivedHandle<ALKitPopover>();
        button->setCommitCallback(
            [handle](LLUICtrl*, const LLSD&)
            {
                if (ALKitPopover* target = handle.get())
                    target->toggle();
            });
    }

    for (const RailData& item : mRailData)
    {
        LLButton::Params params;
        params.name      = "kit_rail_" + item.group;
        params.label     = item.label;
        params.rect      = LLRect(0, ALKitStyle::RAIL_ITEM_H, ALKitStyle::RAIL_W, 0);
        LLButton* button = LLUICtrlFactory::create<LLButton>(params);
        button->setLeftHPad(12);
        button->setRightHPad(8);
        button->setHAlign(LLFontGL::LEFT);
        button->setImageOverlay(item.icon, LLFontGL::LEFT);
        button->setImageOverlaySelectedColor(ALKitStyle::color(ALKitStyle::ACCENT));
        button->setSelectedLabelColor(ALKitStyle::color(ALKitStyle::ACCENT));
        const std::string    group        = item.group;
        LLHandle<ALKitShell> shell_handle = getDerivedHandle<ALKitShell>();
        button->setCommitCallback(
            [shell_handle, group](LLUICtrl*, const LLSD&)
            {
                if (ALKitShell* shell = shell_handle.get())
                {
                    if (shell->mDirectorTabs)
                        shell->mDirectorTabs->selectTabByName(group);
                    shell->layoutChrome();
                }
            });
        mRail->addChild(button);
        mRailButtons.push_back(button);
    }
    if (mDirectorTabs)
    {
        for (S32 group_index = 0; group_index < mDirectorTabs->getTabCount(); ++group_index)
        {
            LLPanel* group = mDirectorTabs->getPanelByIndex(group_index);
            if (!group)
                continue;
            LLTabContainer* tabs = findFirst<LLTabContainer>(group);
            if (!tabs)
                continue;
            for (S32 page_index = 0; page_index < tabs->getTabCount(); ++page_index)
            {
                LLPanel* page = tabs->getPanelByIndex(page_index);
                if (!page)
                    continue;
                LLButton::Params params;
                params.name                       = "kit_tab_" + page->getName();
                params.label                      = page->getLabel();
                params.rect                       = LLRect(0, 32, 100, 0);
                LLButton*            button       = LLUICtrlFactory::create<LLButton>(params);
                const std::string    page_name    = page->getName();
                LLHandle<ALKitShell> shell_handle = getDerivedHandle<ALKitShell>();
                button->setCommitCallback(
                    [shell_handle, page_name](LLUICtrl*, const LLSD&)
                    {
                        if (ALKitShell* shell = shell_handle.get())
                        {
                            shell->selectPage(page_name);
                            shell->layoutChrome();
                        }
                    });
                mTabStrip->addChild(button);
                mTabButtons.push_back(button);
                ALKitLazyPage* lazy = dynamic_cast<ALKitLazyPage*>(page);
                mTabs.push_back({ group->getName(), page_name, page->getLabel(), lazy ? lazy->masterSetting() : std::string(), button });
            }
        }
    }
    for (const RailData& item : mRailData)
        for (const std::string& setting : LLStringUtil::getTokens(item.master_settings, ","))
        {
            mConnections.emplace_back(
                ALKitSettingWatch::instance().watch(setting,
                                                    [handle = getDerivedHandle<ALKitShell>()](const LLSD&, const LLSD&)
                                                    {
                                                        if (ALKitShell* shell = handle.get())
                                                            shell->updatePips();
                                                    }));
        }
    for (const TabData& tab : mTabs)
        if (!tab.master_setting.empty())
        {
            mConnections.emplace_back(
                ALKitSettingWatch::instance().watch(tab.master_setting,
                                                    [handle = getDerivedHandle<ALKitShell>()](const LLSD&, const LLSD&)
                                                    {
                                                        if (ALKitShell* shell = handle.get())
                                                            shell->updatePips();
                                                    }));
        }
    updatePips();
}

void ALKitShell::updatePips()
{
    for (S32 index = 0; index < (S32)mRailButtons.size(); ++index)
    {
        bool off = false;
        for (const std::string& setting : LLStringUtil::getTokens(mRailData[index].master_settings, ","))
        {
            if (gSavedSettings.controlExists(setting) && !gSavedSettings.getBOOL(setting))
                off = true;
        }
        bool changed = false;
        for (const TabData& tab : mTabs)
            if (tab.group == mRailData[index].group && mPageChanged[tab.group + "/" + tab.page])
                changed = true;
        const std::string label = mRailLabels ? mRailData[index].label + (changed ? "  \xE2\x80\xA2" : "") +
                                                (off ? "  \xE2\x97\x8B" : "") : std::string();
        if (mRailButtons[index]->getLabelUnselected() != label)
            mRailButtons[index]->setLabelUnselected(label);
        if (mRailButtons[index]->getLabelSelected() != label)
            mRailButtons[index]->setLabelSelected(label);
        mRailButtons[index]->setImageOverlay(mRailData[index].icon, mRailLabels ? LLFontGL::LEFT : LLFontGL::HCENTER);
        mRailButtons[index]->setFont(ALKitStyle::sansFont("Kit13", mRailButtons[index]->getToggleState()));
    }
    for (TabData& tab : mTabs)
    {
        const bool off =
            !tab.master_setting.empty() && gSavedSettings.controlExists(tab.master_setting) && !gSavedSettings.getBOOL(tab.master_setting);
        const bool changed = mPageChanged[tab.group + "/" + tab.page];
        const std::string label = tab.label + (changed ? "  \xE2\x80\xA2" : "") + (off ? "  \xE2\x97\x8B" : "");
        if (tab.button->getLabelSelected() != label)
            tab.button->setLabel(label);
    }
}

void ALKitShell::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLPanel::reshape(width, height, called_from_parent);
    const bool old_labels = mRailLabels;
    const bool labels     = mRailLabels ? width >= ALKitStyle::BP_RAIL : width >= ALKitStyle::BP_RAIL + ALKitStyle::BP_HYST;
    const bool help       = mWideHelp ? width >= ALKitStyle::BP_FULL : width >= ALKitStyle::BP_FULL + ALKitStyle::BP_HYST;
    mRailLabels           = labels;
    mWideHelp             = help;
    layoutChrome();
    if (old_labels != mRailLabels)
        updatePips();
}

void ALKitShell::layoutChrome()
{
    if (!mDirectorTabs || !mRail || !mTabStrip || !mTransport)
        return;
    const S32 width             = getRect().getWidth();
    const S32 height            = getRect().getHeight();
    const S32 rail_width        = mRailLabels ? ALKitStyle::RAIL_W : ALKitStyle::RAIL_ICON_W;
    const S32 help_width        = mWideHelp ? ALKitStyle::HELP_W : 0;
    const S32 transport_height  = ALKitStyle::TRANSPORT_H;
    const S32 help_strip_height = mWideHelp ? 0 : ALKitStyle::HELP_STRIP_H;
    mTransport->reshape(width, transport_height, true);
    mTransport->setOrigin(0, 0);
    mRail->reshape(rail_width, height - transport_height, true);
    mRail->setOrigin(0, transport_height);
    mTabStrip->reshape(width - rail_width - help_width, ALKitStyle::TABSTRIP_H, true);
    mTabStrip->setOrigin(rail_width, height - ALKitStyle::TABSTRIP_H);
    mDirectorTabs->reshape(width - rail_width - help_width, height - transport_height - ALKitStyle::TABSTRIP_H - help_strip_height, true);
    mDirectorTabs->setOrigin(rail_width, transport_height + help_strip_height);
    mHelpPanel->setVisible(mWideHelp);
    mHelpPanel->reshape(help_width, height - transport_height, true);
    mHelpPanel->setOrigin(width - help_width, transport_height);
    mHelpStrip->setVisible(!mWideHelp);
    mHelpStrip->reshape(width - rail_width, help_strip_height, true);
    mHelpStrip->setOrigin(rail_width, transport_height);
    mHelp.layout(help_width, height - transport_height, width - rail_width);
    S32 y = height - transport_height - ALKitStyle::RAIL_ITEM_H;
    for (S32 i = 0; i < (S32)mRailButtons.size(); ++i)
    {
        LLButton* button = mRailButtons[i];
        button->reshape(rail_width, ALKitStyle::RAIL_ITEM_H, true);
        button->setOrigin(0, y);
        y -= ALKitStyle::RAIL_ITEM_H;
    }
    if (mRoster)
    {
        const bool roster_in_rail = mRosterOpen && mRailLabels && y >= 120;
        mRoster->setVisible(roster_in_rail);
        if (roster_in_rail)
        {
            if (mRoster->getParent() != mRail)
                mRail->addChild(mRoster);
            mRoster->reshape(rail_width, llmax(120, y), true);
            mRoster->setOrigin(0, 0);
        }
        else if (mCastPopover)
        {
            if (mRoster->getParent() != mCastPopover)
                mCastPopover->addChild(mRoster);
            mRoster->setVisible(true);
            mRoster->reshape(280, 340, true);
            mRoster->setOrigin(10, 10);
        }
        layoutRoster();
    }
    LLPanel*          current_group    = mDirectorTabs->getCurrentPanel();
    const std::string group_name       = current_group ? current_group->getName() : std::string();
    LLTabContainer*   current_tabs     = findFirst<LLTabContainer>(current_group);
    LLPanel*          current_leaf     = current_tabs ? current_tabs->getCurrentPanel() : nullptr;
    ALKitPage*        current_kit_page = findFirst<ALKitPage>(current_leaf);
    if (current_kit_page)
    {
        mCurrentPage = current_kit_page->getHandle();
    }
    else
    {
        mCurrentPage = LLHandle<LLView>();
    }
    const std::string current_page = current_leaf ? current_leaf->getName() : std::string();
    if (LLFloater* floater = dynamic_cast<LLFloater*>(getParent()))
    {
        std::string group_label = group_name;
        for (const RailData& rail : mRailData)
            if (rail.group == group_name)
            {
                group_label = rail.label;
                break;
            }
        std::string page_label = current_page;
        for (const TabData& tab : mTabs)
            if (tab.page == current_page)
            {
                page_label = tab.label;
                break;
            }
        const std::string title = "Director \xE2\x80\xBA " + group_label + " \xE2\x80\xBA " + page_label;
        if (floater->getTitle() != title)
            floater->setTitle(title);
    }
    for (S32 index = 0; index < (S32)mRailButtons.size(); ++index)
    {
        const bool selected = index < (S32)mRailData.size() && mRailData[index].group == group_name;
        mRailButtons[index]->setToggleState(selected);
        mRailButtons[index]->setFont(ALKitStyle::sansFont("Kit13", selected));
    }
    refreshModeProxy();
    ALKitPage* mode_page    = currentKitPage();
    const S32  mode_width   = (mode_page ? mode_page->modeCount() : 0) * 86;
    S32        visible_tabs = 0;
    for (const TabData& tab : mTabs)
        if (tab.group == group_name)
            ++visible_tabs;
    const S32        available = llmax(100, mTabStrip->getRect().getWidth() - mode_width - 16);
    std::vector<S32> tab_widths;
    S32              total_width = 0;
    for (const TabData& tab : mTabs)
        if (tab.group == group_name)
        {
            const S32 measured = tab.button->getFont() ? tab.button->getFont()->getWidth(tab.label) : 48;
            const S32 compact  = llclamp(measured + 24, 56, 112);
            tab_widths.push_back(compact);
            total_width += compact + 4;
        }
    const bool overflow       = total_width > available;
    const S32  tabs_available = llmax(56, available - (overflow ? 56 : 0));
    if (!overflow)
        mTabOffset = 0;
    mTabOffset = llclamp(mTabOffset, 0, llmax(0, visible_tabs - 1));
    S32 slots                 = 0;
    S32 used                  = 0;
    for (S32 index = mTabOffset; index < (S32)tab_widths.size(); ++index)
    {
        const S32 need = tab_widths[index] + (slots ? 4 : 0);
        if (slots && used + need > tabs_available)
            break;
        used += need;
        ++slots;
    }
    slots = llmax(1, slots);
    mTabPrev->setVisible(overflow);
    mTabNext->setVisible(overflow);
    if (overflow)
    {
        mTabPrev->setOrigin(4, 8);
        mTabNext->setOrigin(available - 28, 8);
    }
    S32 tab_x     = overflow ? 32 : 8;
    S32 group_tab = 0;
    for (TabData& tab : mTabs)
    {
        const bool in_group = tab.group == group_name;
        const bool visible  = in_group && group_tab >= mTabOffset && group_tab < mTabOffset + slots;
        tab.button->setVisible(visible);
        tab.button->setToggleState(tab.page == current_page);
        tab.button->setFont(ALKitStyle::sansFont("Kit13", tab.page == current_page));
        if (visible)
        {
            const S32 button_width = tab_widths[group_tab];
            tab.button->reshape(button_width, 32, true);
            tab.button->setOrigin(tab_x, 4);
            tab_x += button_width + 4;
        }
        if (in_group)
            ++group_tab;
    }
}

void ALKitShell::layoutRoster()
{
    if (!mRoster)
        return;
    const S32         width  = mRoster->getRect().getWidth();
    const S32         height = mRoster->getRect().getHeight();
    LLTextBox*        header = mRoster->findChild<LLTextBox>("cast_roster_header", false);
    LLScrollListCtrl* list   = mRoster->findChild<LLScrollListCtrl>("cast_list", false);
    LLComboBox*       group  = mRoster->findChild<LLComboBox>("group_assign_combo", false);
    LLTextBox*        hint   = mRoster->findChild<LLTextBox>("cast_hint", false);
    if (header)
    {
        const S32 tools_x = width - 4 - 4 * ALKitStyle::ROSTER_TOOL - 3 * 4;
        header->reshape(llmax(1, tools_x - 8), ALKitStyle::ROSTER_HDR_H, true);
        header->setOrigin(4, height - ALKitStyle::ROSTER_HDR_H);
    }
    static const char* const tools[] = { "btn_add_you", "btn_add_animesh", "btn_focus", "btn_remove" };
    S32                      x       = width - 4 - 4 * ALKitStyle::ROSTER_TOOL - 3 * 4;
    const S32                tools_y = height - ALKitStyle::ROSTER_HDR_H + 1;
    for (const char* name : tools)
        if (LLButton* button = mRoster->findChild<LLButton>(name, false))
        {
            button->reshape(ALKitStyle::ROSTER_TOOL, ALKitStyle::ROSTER_TOOL, true);
            button->setOrigin(x, tools_y);
            x += ALKitStyle::ROSTER_TOOL + 4;
        }
    if (hint)
    {
        const S32 hint_height = width < 180 ? 44 : 28;
        hint->reshape(llmax(1, width - 8), hint_height, true);
        hint->setOrigin(4, 2);
    }
    // Group tag box sits directly under the cast list (above the hint); the
    // list's bottom edge is raised by its height. Empty-state help text points
    // users here ("tag cast members via the Group box under the cast list").
    // The rail admits the roster at 120 px, so when the list would be left
    // under ~3 rows (heading + 3 * ~18 px row, approximate) the Group box is
    // hidden first and the cast list keeps the space.
    constexpr S32 ROSTER_MIN_LIST_H = 23 + 3 * 18;
    const S32     list_ceiling      = height - ALKitStyle::ROSTER_HDR_H - 4;
    S32           list_bottom       = hint ? hint->getRect().mTop + 4 : 34;
    if (group)
    {
        const bool show_group = list_ceiling - (list_bottom + ALKitStyle::ROSTER_TOOL + 4) >= ROSTER_MIN_LIST_H;
        group->setVisible(show_group);
        if (show_group)
        {
            group->reshape(llmax(1, width - 8), ALKitStyle::ROSTER_TOOL, true);
            group->setOrigin(4, list_bottom);
            list_bottom += ALKitStyle::ROSTER_TOOL + 4;
        }
    }
    if (list)
    {
        const S32 list_top = llmax(list_bottom + 1, list_ceiling);
        list->reshape(llmax(1, width - 8), llmax(1, list_top - list_bottom), true);
        list->setOrigin(4, list_bottom);
    }
}

void ALKitShell::refreshModeProxy()
{
    ALKitPage* page  = currentKitPage();
    const S32  count = page ? page->modeCount() : 0;
    while ((S32)mModeButtons.size() < count)
    {
        const S32        index = (S32)mModeButtons.size();
        LLButton::Params params;
        params.name      = "kit_mode_seg_" + llformat("%d", index);
        params.rect      = LLRect(0, 28, 82, 0);
        LLButton* button = LLUICtrlFactory::create<LLButton>(params);
        mTabStrip->addChild(button);
        LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
        button->setCommitCallback(
            [handle, index](LLUICtrl*, const LLSD&)
            {
                if (ALKitShell* shell = handle.get())
                    if (ALKitPage* current = shell->currentKitPage())
                    {
                        current->setModeIndex(index, true);
                        shell->refreshModeProxy();
                    }
            });
        mModeButtons.push_back(button);
    }
    S32 x = mTabStrip->getRect().getWidth() - count * 86 - 4;
    for (S32 index = 0; index < (S32)mModeButtons.size(); ++index)
    {
        const bool visible = index < count;
        mModeButtons[index]->setVisible(visible);
        if (visible)
        {
            if (mModeButtons[index]->getLabelSelected() != page->modeName(index))
                mModeButtons[index]->setLabel(page->modeName(index));
            mModeButtons[index]->setToggleState(index == page->modeIndex());
            ALKitStyle::styleButton(mModeButtons[index], false);
            mModeButtons[index]->setImageColor(index == page->modeIndex() ? ALKitStyle::color(ALKitStyle::TEXT)
                                                                          : ALKitStyle::color(ALKitStyle::INPUT_BG));
            mModeButtons[index]->setSelectedImageColor(ALKitStyle::color(ALKitStyle::TEXT));
            mModeButtons[index]->setUnselectedLabelColor(index == page->modeIndex() ? ALKitStyle::color(ALKitStyle::KNOB_ON)
                                                                                    : ALKitStyle::color(ALKitStyle::LABEL));
            mModeButtons[index]->setSelectedLabelColor(index == page->modeIndex() ? ALKitStyle::color(ALKitStyle::KNOB_ON)
                                                                                   : ALKitStyle::color(ALKitStyle::LABEL));
            mModeButtons[index]->setFont(ALKitStyle::sansFont("Kit12", index == page->modeIndex()));
            mModeButtons[index]->setOrigin(x + index * 86, 8);
        }
    }
}

void ALKitShell::draw()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALKitShell draw");
    ALUIPerfSelfTest::recordKitWidgetTree(this);
    auto fill_panel = [this](LLView* panel, const LLUIColor& color)
    {
        LLRect rect;
        if (panel && panel->getVisible() && panel->localRectToOtherView(panel->getLocalRect(), &rect, this))
            gl_rect_2d(rect, color.get(), true);
    };
    fill_panel(mRail, ALKitStyle::color(ALKitStyle::RAIL_BG));
    fill_panel(mTabStrip, ALKitStyle::color(ALKitStyle::RAIL_BG));
    fill_panel(mHelpPanel, ALKitStyle::color(ALKitStyle::RAIL_BG));
    fill_panel(mHelpStrip, ALKitStyle::color(ALKitStyle::RAIL_BG));
    fill_panel(mTransport, ALKitStyle::color(ALKitStyle::RAIL_BG));
    if (mRail)
        gl_line_2d(mRail->getRect().mRight - 1, mRail->getRect().mBottom, mRail->getRect().mRight - 1,
                   mRail->getRect().mTop, ALKitStyle::color(ALKitStyle::DIVIDER2).get());
    if (mTabStrip)
        gl_line_2d(mTabStrip->getRect().mLeft, mTabStrip->getRect().mBottom, mTabStrip->getRect().mRight,
                   mTabStrip->getRect().mBottom, ALKitStyle::color(ALKitStyle::DIVIDER2).get());
    if (mHelpPanel && mHelpPanel->getVisible())
        gl_line_2d(mHelpPanel->getRect().mLeft, mHelpPanel->getRect().mBottom, mHelpPanel->getRect().mLeft,
                   mHelpPanel->getRect().mTop, ALKitStyle::color(ALKitStyle::DIVIDER2).get());
    if (mTransport)
        gl_line_2d(mTransport->getRect().mLeft, mTransport->getRect().mTop - 1, mTransport->getRect().mRight,
                   mTransport->getRect().mTop - 1, ALKitStyle::color(ALKitStyle::DIVIDER).get());
    for (LLButton* button : mRailButtons)
    {
        if (!button || !button->getVisible() || !button->getToggleState())
            continue;
        LLRect rect;
        if (button->localRectToOtherView(button->getLocalRect(), &rect, this))
        {
            gl_rect_2d(rect, ALKitStyle::color(ALKitStyle::CONTROL_BG).get(), true);
            gGL.color4fv(ALKitStyle::color(ALKitStyle::ACCENT).get().mV);
            gl_circle_2d((F32)rect.mRight - 13.f, (F32)(rect.mBottom + rect.mTop) * .5f, 3.f, 12, true);
        }
    }
    for (LLButton* button : mTabButtons)
    {
        if (!button || !button->getVisible() || !button->getToggleState())
            continue;
        LLRect rect;
        if (button->localRectToOtherView(button->getLocalRect(), &rect, this))
            gl_rect_2d(rect.mLeft, rect.mBottom, rect.mRight, rect.mBottom + 2,
                       ALKitStyle::color(ALKitStyle::ACCENT).get(), true);
    }
    LLPanel::draw();
    if (mTransportCut && mTransportCut->getVisible())
    {
        LLRect rect;
        if (mTransportCut->localRectToOtherView(mTransportCut->getLocalRect(), &rect, this))
        {
            for (S32 inset = 1; inset <= 3; ++inset)
            {
                LLColor4 ring = ALKitStyle::color(ALKitStyle::RECORD).get();
                ring.mV[VALPHA] = (4 - inset) * .07f;
                LLRect expanded(rect.mLeft - inset, rect.mTop + inset,
                                (mTransportArm && mTransportArm->getVisible() ? mTransportArm->getRect().mRight - mTransportArm->getRect().mLeft : 0) + rect.mRight + inset,
                                rect.mBottom - inset);
                gl_rect_2d(expanded, ring, false);
            }
        }
    }
    if (mTransportArm && mTransportArm->getVisible())
    {
        LLRect rect;
        if (mTransportArm->localRectToOtherView(mTransportArm->getLocalRect(), &rect, this))
            gl_line_2d(rect.mLeft, rect.mBottom + 3, rect.mLeft, rect.mTop - 3,
                       LLColor4(0.078f, 0.078f, 0.075f, .3f));
    }
    LLView* view = mHighlight.get();
    if (view && mHighlightTimer.getElapsedTimeF32() < ALKitStyle::FLASH_SEC)
    {
        LLRect rect;
        if (view->localRectToOtherView(view->getLocalRect(), &rect, this))
        {
            LLColor4 color = ALKitStyle::color(ALKitStyle::ACCENT).get();
            color.mV[VALPHA] *= 1.f - mHighlightTimer.getElapsedTimeF32() / ALKitStyle::FLASH_SEC;
            gl_rect_2d(rect, color, false);
        }
    }
}

ALKitShell* ALKitShell::find(LLView* v)
{
    for (; v; v = v->getParent())
    {
        if (auto* s = dynamic_cast<ALKitShell*>(v))
        {
            return s;
        }
    }
    return nullptr;
}

bool ALKitShell::navigateTo(const std::string& path)
{
    ALKitTarget  target;
    const size_t slash = path.find('/');
    if (slash == std::string::npos)
        target.page_key = path;
    else
    {
        target.page_key = path.substr(0, slash);
        target.widget   = path.substr(slash + 1);
    }
    return navigateTo(target);
}
bool ALKitShell::navigateTo(const ALKitTarget& target)
{
    ALKitHistory::ScopedSuspend suspend;
    if (mChangedOnly)
        setChangedOnly(false);
    clearSearch();
    LLPanel* panel = ensurePageBuilt(target.page_key);
    if (!panel)
        return false;
    if (ALKitPage* page = findFirst<ALKitPage>(panel))
    {
        ALKitRow* row = !target.widget.empty() ? page->findRow(target.widget) : page->findRowBySetting(target.setting);
        if (row)
            return page->revealRow(row, true);
        if (ALKitCard* card = page->findCard(target.card_id))
            return page->revealCard(card, true);
    }
    if (!target.widget.empty())
        if (LLView* view = panel->findChild<LLView>(target.widget))
        {
            for (LLView* ancestor = view->getParent(); ancestor && ancestor != panel; ancestor = ancestor->getParent())
                if (LLAccordionCtrlTab* accordion = dynamic_cast<LLAccordionCtrlTab*>(ancestor))
                    accordion->setDisplayChildren(true);
            for (LLView* ancestor = view->getParent(); ancestor; ancestor = ancestor->getParent())
            {
                if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(ancestor))
                {
                    LLRect rect;
                    if (LLView* document = scroller->getScrolledView())
                    {
                        if (view->localRectToOtherView(view->getLocalRect(), &rect, document))
                        {
                            const LLRect visible = scroller->getVisibleContentRect();
                            const S32 center = visible.getHeight() / 2;
                            const S32 half = llmax(1, rect.getHeight() / 2);
                            scroller->scrollToShowRect(rect,
                                LLRect(0, center + half, visible.getWidth(), center - half));
                        }
                    }
                    break;
                }
            }
            mHighlight = view->getHandle();
            mHighlightTimer.reset();
            return true;
        }
    return false;
}

bool ALKitShell::selectPage(const std::string& page_name)
{
    if (ALKitPage* current = currentKitPage())
        current->saveState();
    ALKitLazyPage* page = findChild<ALKitLazyPage>(page_name);
    if (!page)
        return false;
    LLTabContainer* inner = dynamic_cast<LLTabContainer*>(page->getParent());
    LLPanel*        group = inner ? dynamic_cast<LLPanel*>(inner->getParent()) : nullptr;
    if (mDirectorTabs && group)
        mDirectorTabs->selectTabByName(group->getName());
    if (inner)
        inner->selectTabByName(page_name);
    const bool built = page->ensureBuilt() != nullptr;
    syncSelection();
    if (built)
    {
        if (ALKitPage* selected = currentKitPage())
        {
            selected->setChangedOnlyValue(mChangedOnly);
            mPageSelected(selected->pageKey());
        }
        if (mChangedOnly || !mSearch.queryText().empty())
            runSearch(mSearch.queryText());
    }
    return built;
}

void ALKitShell::syncSelection()
{
    // Page/group navigation always closes the results popup and never reopens
    // it; only typing (or clicking back into a non-empty box) re-arms it.
    setSearchPopupArmed(nullptr, false);
    layoutChrome();
    if (ALKitPage* page = currentKitPage())
        mHelp.showPage(page);
}

LLPanel* ALKitShell::ensurePageBuilt(const std::string& page_name)
{
    std::string  leaf  = page_name;
    const size_t slash = leaf.rfind('/');
    if (slash != std::string::npos)
        leaf = leaf.substr(slash + 1);
    if (!selectPage(leaf))
        return nullptr;
    ALKitLazyPage* page = findChild<ALKitLazyPage>(leaf);
    return page ? page->ensureBuilt() : nullptr;
}

ALKitPage* ALKitShell::currentKitPage() const
{ return dynamic_cast<ALKitPage*>(mCurrentPage.get()); }

ALKitPage* ALKitShell::findBuiltPage(const std::string& key) const
{
    const auto found = mBuiltPages.find(key);
    return found == mBuiltPages.end() ? nullptr : found->second.get();
}

std::string ALKitShell::currentPageKey() const
{
    ALKitPage* page = currentKitPage();
    return page ? page->pageKey() : std::string();
}

std::string ALKitShell::pageKeyAt(S32 x, S32 y) const
{
    ALKitPage* page = currentKitPage();
    if (!page)
        return "chrome";
    if (mTabStrip)
    {
        S32 strip_x = 0;
        S32 strip_y = 0;
        const_cast<ALKitShell*>(this)->localPointToOtherView(x, y, &strip_x, &strip_y, mTabStrip);
        if (mTabStrip->pointInView(strip_x, strip_y))
            return page->pageKey();
    }
    S32 local_x = 0;
    S32 local_y = 0;
    const_cast<ALKitShell*>(this)->localPointToOtherView(x, y, &local_x, &local_y, page);
    return page->pointInView(local_x, local_y) ? page->pageKey() : "chrome";
}

std::string ALKitShell::pagePathLabel(const std::string& key) const
{
    std::string  leaf  = key;
    const size_t slash = leaf.rfind('/');
    if (slash != std::string::npos)
        leaf = leaf.substr(slash + 1);
    for (const TabData& tab : mTabs)
        if (tab.page == leaf)
        {
            std::string group = tab.group;
            for (const RailData& rail : mRailData)
                if (rail.group == group)
                {
                    return rail.label + " \xE2\x80\xBA " + tab.label;
                }
            return tab.label;
        }
    return key;
}

std::vector<std::string> ALKitShell::unwrappedPages() const
{
    std::vector<std::string> pages;
    pages.reserve(mRailData.size() + mTabs.size() + 3);
    for (const RailData& rail : mRailData)
        pages.push_back(rail.group);
    for (const TabData& tab : mTabs)
        pages.push_back(tab.page);
    pages.push_back("dance_legacy_host");
    pages.push_back("exposure_legacy_host");
    pages.push_back("ghosts_legacy_host");
    return pages;
}

const std::vector<std::string> ALKitShell::railGroups() const
{
    std::vector<std::string> groups;
    groups.reserve(mRailData.size());
    for (const RailData& item : mRailData)
        groups.push_back(item.group);
    return groups;
}

void ALKitShell::pageBuilt(const std::string&, LLPanel* panel)
{
    ALKitPage* page = dynamic_cast<ALKitPage*>(panel);
    if (!page)
        page = findFirst<ALKitPage>(panel);
    if (!page)
        return;
    mBuiltPages[page->pageKey()] = page->getDerivedHandle<ALKitPage>();
    refreshChangedMarkers();
    layoutChrome();
    if (page == currentKitPage())
        mHelp.showPage(page);
    std::string  prefix = page->pageKey();
    const size_t slash  = prefix.rfind('/');
    if (slash != std::string::npos)
        prefix = prefix.substr(slash + 1);
    if (prefix.size() > 4 && prefix.compare(prefix.size() - 4, 4, "_tab") == 0)
        prefix.erase(prefix.size() - 4);
    if (LLLineEditor* editor = page->findChild<LLLineEditor>(prefix + "_search"))
    {
        LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
        editor->setKeystrokeCallback(
            [handle](LLLineEditor* line, void*)
            {
                if (ALKitShell* shell = handle.get())
                {
                    // Typing is the only thing that arms the results popup;
                    // emptying the box closes it.
                    shell->mSearchPopupArmed = line->hasFocus() && !line->getText().empty();
                    if (shell->mSearchPopover)
                    {
                        if (shell->mSearchPopupArmed)
                            shell->mSearchPopover->bindAnchor(line);
                        else
                            shell->mSearchPopover->hide();
                    }
                    shell->queueSearch(line->getText());
                }
            },
            nullptr);
        editor->setFocusLostCallback(
            [handle, editor_handle = editor->getHandle()](LLFocusableElement*)
            {
                // Focus callbacks run during the transition, so inspect the
                // settled destination on idle before deciding to disarm.
                doOnIdleOneTime([handle, editor_handle]()
                {
                    ALKitShell* shell = handle.get();
                    LLView* editor_view = editor_handle.get();
                    if (!shell || !shell->mSearchPopover)
                        return;
                    if (!editor_view)
                    {
                        shell->setSearchPopupArmed(nullptr, false);
                        return;
                    }
                    LLView* focus = dynamic_cast<LLView*>(gFocusMgr.getKeyboardFocus());
                    if (focus == editor_view || (focus && focus->hasAncestor(shell->mSearchPopover)))
                        return;
                    shell->setSearchPopupArmed(nullptr, false);
                });
            });
        if (mSearchPopover)
            mSearchPopover->bindAnchor(editor);
    }
    if (LLButton* who = page->findChild<LLButton>(prefix + "_who_chip"))
    {
        LLComboBox* picker = page->whoCombo();
        if (picker)
        {
            const std::vector<LLScrollListItem*> items       = picker->getAllData();
            const S32                            item_height = 24;
            ALKitPopover::Params                 params;
            params.name           = prefix + "_who_popover";
            params.rect           = LLRect(0, (S32)(items.size() + 1) * item_height + 16, 360, 0);
            params.visible        = false;
            ALKitPopover* popover = LLUICtrlFactory::create<ALKitPopover>(params);
            addChild(popover);
            popover->bindAnchor(who);
            LLHandle<ALKitPopover>          popover_handle = popover->getDerivedHandle<ALKitPopover>();
            LLHandle<ALKitPage>             page_handle    = page->getDerivedHandle<ALKitPage>();
            LLHandle<LLComboBox>            picker_handle  = picker->getDerivedHandle<LLComboBox>();
            std::vector<LLHandle<LLButton>> choice_handles;
            LLButton::Params                button_params;
            button_params.rect  = LLRect(10, item_height, 350, 0);
            button_params.name  = prefix + "_who_follow";
            button_params.label = "Follow cast selection (" + picker->getSelectedItemLabel() + ")";
            LLButton* follow    = LLUICtrlFactory::create<LLButton>(button_params);
            popover->addChild(follow);
            follow->setOrigin(10, (S32)items.size() * item_height + 8);
            follow->setCommitCallback(
                [page_handle, popover_handle](LLUICtrl*, const LLSD&)
                {
                    if (ALKitPage* target = page_handle.get())
                        target->setWhoPin(LLSD());
                    if (ALKitPopover* target = popover_handle.get())
                        target->hide();
                });
            S32 index = 0;
            for (LLScrollListItem* item : items)
            {
                if (!item || !item->getEnabled() || !item->getColumn(0))
                    continue;
                const LLSD value    = item->getValue();
                button_params.name  = prefix + "_who_item_" + llformat("%d", index);
                button_params.label = item->getColumn(0)->getValue().asString();
                LLButton* choice    = LLUICtrlFactory::create<LLButton>(button_params);
                popover->addChild(choice);
                choice_handles.push_back(choice->getDerivedHandle<LLButton>());
                choice->setOrigin(10, (S32)(items.size() - index - 1) * item_height + 8);
                choice->setCommitCallback(
                    [page_handle, picker_handle, popover_handle, value](LLUICtrl*, const LLSD&)
                    {
                        if (LLComboBox* combo = picker_handle.get())
                        {
                            combo->setValue(value);
                            combo->onCommit();
                        }
                        if (ALKitPage* target = page_handle.get())
                            target->setWhoPin(value);
                        if (ALKitPopover* target = popover_handle.get())
                            target->hide();
                    });
                ++index;
            }
            LLHandle<LLButton> follow_handle = follow->getDerivedHandle<LLButton>();
            who->setCommitCallback(
                [popover_handle, picker_handle, follow_handle, choice_handles](LLUICtrl*, const LLSD&)
                {
                    LLComboBox*   combo  = picker_handle.get();
                    ALKitPopover* target = popover_handle.get();
                    if (!combo || !target)
                        return;
                    if (!target->getVisible())
                    {
                        if (LLButton* follow_button = follow_handle.get())
                        {
                            follow_button->setLabel("Follow cast selection (" + combo->getSelectedItemLabel() + ")");
                        }
                        const std::vector<LLScrollListItem*> current_items = combo->getAllData();
                        S32                                  choice_index  = 0;
                        for (LLScrollListItem* item : current_items)
                        {
                            if (!item || !item->getEnabled() || !item->getColumn(0))
                                continue;
                            if (choice_index < (S32)choice_handles.size())
                            {
                                if (LLButton* choice = choice_handles[choice_index].get())
                                {
                                    choice->setLabel(item->getColumn(0)->getValue().asString());
                                }
                            }
                            ++choice_index;
                        }
                    }
                    target->toggle();
                });
            ALKitStyle::styleTree(popover);
        }
    }
    startSearchLoad();
    rebuildQuick();
}

void ALKitShell::runSearch(const std::string& query)
{
    mSearch.setQuery(query);
    ALKitPage* page = currentKitPage();
    if (!page)
        return;
    if (page->pageKey() == "quick_group/quick_tab")
    {
        page->applyLocalFilter(query, mChangedOnly);
        ALKitSearch::Hits hits = query.empty() ? ALKitSearch::Hits() : mSearch.query(query);
        updateSearchResults(hits, page);
        return;
    }
    if (query.empty() && !mChangedOnly)
    {
        page->clearSearch();
        if (mSearchPopover)
            mSearchPopover->hide();
        mSearchHits.clear();
        mHasTopSearchHit = false;
        return;
    }
    mSearch.setCurrentPage(page->pageKey());
    ALKitSearch::Hits hits = query.empty() ? mSearch.all() : mSearch.query(query);
    if (mChangedOnly)
        hits.erase(std::remove_if(hits.begin(), hits.end(),
            [this](const ALKitSearch::Hit& hit)
            {
                if (ALKitPage* built = findBuiltPage(hit.page_key))
                    if (ALKitRow* row = !hit.widget.empty() ? built->findRow(hit.widget) : built->findRowBySetting(hit.setting))
                        return !row->amber();
                LLControlVariable* control = hit.setting.empty() ? nullptr : gSavedSettings.getControl(hit.setting).get();
                return !control || ALKitQol::valuesEqual(control->get(), control->getDefault(), hit.decimals);
            }), hits.end());
    ALKitSearch::Hits local;
    for (ALKitSearch::Hit hit : hits)
        if (hit.page_key == page->pageKey())
        {
            hit.row = !hit.widget.empty() ? page->findRow(hit.widget) : page->findRowBySetting(hit.setting);
            if (hit.row)
                local.push_back(std::move(hit));
        }
    page->applySearch(local);
    updateSearchResults(hits, page);
}

void ALKitShell::queueSearch(const std::string& query)
{
    mPendingQuery = query;
    mSearch.setQuery(query);
    if (auto timer = mSearchDebounce.lock())
        delete timer.get();
    LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
    LLEventTimer*        timer  = LLEventTimer::run_after(.12f,
                                                          [handle]()
                                                          {
                                                      if (ALKitShell* shell = handle.get())
                                                      {
                                                          shell->mSearchDebounce.reset();
                                                          shell->runSearch(shell->mPendingQuery);
                                                      }
                                                          });
    mSearchDebounce             = timer->getWeak();
}

void ALKitShell::updateSearchResults(const ALKitSearch::Hits& hits, ALKitPage* page)
{
    mSearchHits.clear();
    mHasTopSearchHit = !hits.empty();
    if (mHasTopSearchHit)
        mTopSearchHit = hits.front();
    S32        local_count = 0;
    S32        elsewhere   = 0;
    const bool global      = gSavedSettings.getBOOL("KitSearchGlobal");
    std::vector<std::string> page_order;
    std::unordered_map<std::string, ALKitSearch::Hits> page_hits;
    for (const ALKitSearch::Hit& hit : hits)
    {
        if (hit.page_key == page->pageKey())
            ++local_count;
        else
            ++elsewhere;
        if (global)
        {
            if (page_hits.emplace(hit.page_key, ALKitSearch::Hits()).second)
                page_order.push_back(hit.page_key);
            page_hits[hit.page_key].push_back(hit);
        }
    }
    for (const std::string& page_key : page_order)
        mSearchHits.insert(mSearchHits.end(), page_hits[page_key].begin(), page_hits[page_key].end());
    mSearchSelection      = 0;
    mSearchSelectionMoved = false;
    if (mSearchHeader)
        mSearchHeader->setText(llformat("%d on this page \xC2\xB7 %d elsewhere", local_count, elsewhere));
    mSearchRows.clear();
    if (mSearchResultsPanel)
        mSearchResultsPanel->deleteAllChildren();
    constexpr S32 RESULT_HEIGHT = 36;
    constexpr S32 GROUP_HEIGHT  = 22;
    S32 content_height = 0;
    std::string prior_page;
    for (S32 index = 0; index < (S32)mSearchHits.size(); ++index)
    {
        const ALKitSearch::Hit& hit = mSearchHits[index];
        if (hit.page_key != prior_page)
        {
            std::string group = hit.external ? hit.external_label : hit.group_label;
            const std::string page_label = hit.external ? hit.external_tab_label : hit.page_label;
            if (!page_label.empty())
                group += " \xE2\x80\xBA " + page_label;
            group = searchDisplayText(group);
            LLStringUtil::toUpper(group);
            LLPanel::Params params;
            params.name = "kit_search_group_" + llformat("%d", index);
            params.rect = LLRect(0, GROUP_HEIGHT, 390, 0);
            params.background_visible = false;
            ALKitSearchGroupRow* header = new ALKitSearchGroupRow(params, std::move(group));
            mSearchResultsPanel->addChildInBack(header);
            content_height += GROUP_HEIGHT;
        }

        std::string context;
        if (!hit.card_label.empty())
            context = "\xC2\xB7  " + searchDisplayText(hit.card_label);
        if (hit.advanced)
            context += (context.empty() ? std::string() : "  \xC2\xB7  ") + std::string("More");

        std::string detail = searchDisplayText(hit.setting);
        if (hit.external)
        {
            detail = "Opens in " + searchDisplayText(hit.external_label);
            if (!hit.external_tab_label.empty())
                detail += " \xE2\x80\xBA " + searchDisplayText(hit.external_tab_label);
        }

        LLPanel::Params params;
        params.name = "kit_search_result_" + llformat("%d", index);
        params.rect = LLRect(0, RESULT_HEIGHT, 390, 0);
        params.background_visible = false;
        LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
        ALKitSearchResultRow* row = new ALKitSearchResultRow(
            params, searchDisplayText(hit.label), std::move(context), std::move(detail),
            [handle, index]()
            {
                if (ALKitShell* shell = handle.get())
                {
                    shell->mSearchSelection = index;
                    shell->activateSearchResult();
                }
            });
        mSearchResultsPanel->addChildInBack(row);
        mSearchRows.push_back(row);
        content_height += RESULT_HEIGHT;
        prior_page = hit.page_key;
    }
    content_height = llmax(1, content_height);
    if (mSearchResultsPanel)
        mSearchResultsPanel->reshape(390, content_height, true);
    S32 y = content_height;
    for (LLView* child : *mSearchResultsPanel->getChildList())
    {
        y -= child->getRect().getHeight();
        child->setOrigin(0, y);
    }
    if (mSearchFooter)
        mSearchFooter->setText(llformat("All %d matches, grouped by page.", (S32)mSearchHits.size()));
    if (mSearchPopover)
    {
        // Recomputing results (page change, Changed-only toggle, refresh) never
        // opens the popup by itself; only an armed, non-empty search box does.
        if (global && !mSearchHits.empty() && mSearchPopupArmed && !mSearch.queryText().empty())
            mSearchPopover->show();
        else
            mSearchPopover->hide();
    }
}

bool ALKitShell::handleSearchKey(KEY key)
{
    if (key == KEY_ESCAPE)
    {
        // Esc closes an open results popup (text and filters are kept); with no
        // popup open it falls through to the floater's Esc handling.
        if (!mSearchPopover || !mSearchPopover->getVisible())
            return false;
        setSearchPopupArmed(nullptr, false);
        return true;
    }
    if (key == KEY_RETURN && mSearchSelectionMoved && !mSearchHits.empty())
    {
        activateSearchResult();
        return true;
    }
    if (key == KEY_RETURN && mHasTopSearchHit)
    {
        const ALKitSearch::Hit hit = mTopSearchHit;
        LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
        doOnIdleOneTime([handle, hit]()
        {
            if (ALKitShell* shell = handle.get())
                shell->activateSearchResult(hit);
        });
        return true;
    }
    if (mSearchHits.empty())
        return false;
    if (key == KEY_UP)
        mSearchSelection = (mSearchSelection + (S32)mSearchHits.size() - 1) % (S32)mSearchHits.size();
    else if (key == KEY_DOWN)
        mSearchSelection = (mSearchSelection + 1) % (S32)mSearchHits.size();
    else
        return false;
    mSearchSelectionMoved = true;
    for (S32 index = 0; index < (S32)mSearchRows.size(); ++index)
        mSearchRows[index]->setSelected(index == mSearchSelection);
    if (mSearchScroller && mSearchSelection < (S32)mSearchRows.size())
        mSearchScroller->scrollToShowRect(mSearchRows[mSearchSelection]->getRect());
    return true;
}

void ALKitShell::activateSearchResult()
{
    if (mSearchSelection < 0 || mSearchSelection >= (S32)mSearchHits.size())
        return;
    const ALKitSearch::Hit hit = mSearchHits[mSearchSelection];
    LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
    doOnIdleOneTime([handle, hit]()
    {
        if (ALKitShell* shell = handle.get())
            shell->activateSearchResult(hit);
    });
}

void ALKitShell::activateSearchResult(const ALKitSearch::Hit& hit)
{
    if (hit.external)
    {
        openExternalResult(hit);
        return;
    }
    ALKitTarget             target;
    target.page_key = hit.page_key;
    target.card_id  = hit.card_id;
    target.widget   = hit.widget;
    target.setting  = hit.setting;
    target.option   = hit.option;
    navigateTo(target);
}

bool ALKitShell::openExternalResult(const ALKitSearch::Hit& hit)
{
    setSearchPopupArmed(nullptr, false);
    LLFloater* floater = LLFloaterReg::showInstance(hit.external_floater);
    if (!floater)
        return false;
    selectTabRecursive(floater, hit.external_tab);
    LLView* target_root = hit.external_tab.empty()
        ? static_cast<LLView*>(floater)
        : floater->findChild<LLView>(hit.external_tab, true);
    if (!target_root)
        target_root = floater;
    LLView* target = findExternalTarget(target_root, hit.widget, hit.setting);
    if (!target)
        return false;
    for (LLView* ancestor = target->getParent(); ancestor && ancestor != floater; ancestor = ancestor->getParent())
        if (LLAccordionCtrlTab* accordion = dynamic_cast<LLAccordionCtrlTab*>(ancestor))
            accordion->setDisplayChildren(true);
    LLHandle<LLFloater> floater_handle = floater->getHandle();
    LLHandle<LLView> target_handle = target->getHandle();
    doOnIdleOneTime([floater_handle, target_handle]()
    {
        LLFloater* opened = floater_handle.get();
        LLView* control = target_handle.get();
        if (!opened || !control)
            return;
        centerInAncestorScroller(control);
        pulseExternalTarget(opened, control);
    });
    return true;
}

bool ALKitShell::cycleChip(S32 delta)
{
    ALKitPage* page = currentKitPage();
    return page && page->cycleChip(delta);
}

bool ALKitShell::cyclePage(S32 delta)
{
    if (!mDirectorTabs)
        return false;
    LLPanel*        group = mDirectorTabs->getCurrentPanel();
    LLTabContainer* tabs  = findFirst<LLTabContainer>(group);
    if (!tabs || tabs->getTabCount() < 1)
        return false;
    const S32 current = tabs->getCurrentPanelIndex();
    LLPanel*  next    = tabs->getPanelByIndex((current + delta + tabs->getTabCount()) % tabs->getTabCount());
    return next && selectPage(next->getName());
}

bool ALKitShell::cycleGroup(S32 delta)
{
    if (!mDirectorTabs || mDirectorTabs->getTabCount() < 1)
        return false;
    const S32 current = mDirectorTabs->getCurrentPanelIndex();
    LLPanel*  group   = mDirectorTabs->getPanelByIndex((current + delta + mDirectorTabs->getTabCount()) % mDirectorTabs->getTabCount());
    if (!group)
        return false;
    mDirectorTabs->selectTabPanel(group);
    LLTabContainer* tabs = findFirst<LLTabContainer>(group);
    LLPanel*        page = tabs ? tabs->getCurrentPanel() : nullptr;
    return page && selectPage(page->getName());
}

void ALKitShell::startSearchLoad()
{
    if (mSearchStarted)
        return;
    mSearchStarted              = true;
    LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
    doOnIdleOneTime(
        [handle]()
        {
            if (ALKitShell* shell = handle.get())
                if (shell->mSearch.load(shell->mIndexFile))
                    shell->scheduleSearchBatch();
        });
}

void ALKitShell::scheduleSearchBatch()
{
    LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
    doOnIdleOneTime(
        [handle]()
        {
            if (ALKitShell* shell = handle.get())
                if (shell->mSearch.tokenizeBatch())
                    shell->scheduleSearchBatch();
                else
                    shell->buildChangedIndex();
        });
}

void ALKitShell::buildChangedIndex()
{
    mPageSettings.clear();
    mSettingLocations.clear();
    mPageChangedCounts.clear();
    std::unordered_map<std::string, bool> watched;
    for (const ALKitSearch::Hit& hit : mSearch.all())
        if (!hit.external && !hit.setting.empty())
        {
            std::vector<ChangedEntry>& settings = mPageSettings[hit.page_key];
            const U32 index = static_cast<U32>(settings.size());
            settings.push_back({ hit.setting, hit.decimals, false });
            mSettingLocations[hit.setting].push_back({ hit.page_key, index });
            if (!watched[hit.setting])
            {
                watched[hit.setting] = true;
                mConnections.emplace_back(ALKitSettingWatch::instance().watch(hit.setting,
                    [handle = getDerivedHandle<ALKitShell>(), setting = hit.setting](const LLSD&, const LLSD&)
                    {
                        if (ALKitShell* shell = handle.get())
                            shell->refreshChangedSetting(setting);
                    }));
            }
        }
    refreshChangedMarkers();
}

void ALKitShell::clearSearch()
{
    if (auto timer = mSearchDebounce.lock())
        delete timer.get();
    mSearchDebounce.reset();
    mSearch.setQuery("");
    if (ALKitPage* page = currentKitPage())
    {
        page->setSearchText("");
        if (mChangedOnly)
            page->applyLocalFilter("", true);
        else
            page->clearSearch();
    }
    mSearchHits.clear();
    mHasTopSearchHit = false;
    setSearchPopupArmed(nullptr, false);
}

void ALKitShell::setSearchPopupArmed(LLLineEditor* editor, bool armed)
{
    mSearchPopupArmed = armed && editor && editor->hasFocus() && !editor->getText().empty();
    if (!mSearchPopupArmed)
    {
        if (mSearchPopover)
            mSearchPopover->hide();
        return;
    }
    if (!mSearchPopover)
        return;
    mSearchPopover->bindAnchor(editor);
    // Re-show for a click back into a non-empty box (one search, on click only).
    if (!mSearchPopover->getVisible())
        runSearch(editor->getText());
}

bool ALKitShell::isFavorite(const std::string& page, const std::string& widget) const
{
    return std::find(mFavorites.begin(), mFavorites.end(), ALKitQol::Favorite{ page, widget }) != mFavorites.end();
}

void ALKitShell::toggleFavorite(const std::string& page, const std::string& widget)
{
    if (!ALKitQol::isValidFavorite({ page, widget }))
        return;
    const ALKitQol::Favorite key{ page, widget };
    const auto found = std::find(mFavorites.begin(), mFavorites.end(), key);
    if (found == mFavorites.end())
        mFavorites.push_back(key);
    else
        mFavorites.erase(found);
    gSavedSettings.setLLSD("DirectorFavorites", ALKitQol::favoritesToLLSD(mFavorites));
    for (const auto& entry : mBuiltPages)
        if (ALKitPage* built = entry.second.get())
            if (ALKitRow* row = built->findRow(widget); row && built->pageKey() == page)
                row->refreshFavorite();
    LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
    doOnIdleOneTime([handle]() { if (ALKitShell* shell = handle.get()) shell->rebuildQuick(); });
}

void ALKitShell::rebuildQuick()
{
    if (mRebuildingQuick)
        return;
    ALKitPage* quick = findBuiltPage("quick_group/quick_tab");
    if (!quick)
        return;
    ALKitCard* card = quick->findCard("quick");
    if (!card)
        return;
    mRebuildingQuick = true;
    std::vector<ALKitRow*> proxies;
    proxies.reserve(mFavorites.size());
    S32 index = 0;
    for (const ALKitQol::Favorite& favorite : mFavorites)
    {
        ALKitPage* source_page = findBuiltPage(favorite.page);
        if (!source_page)
        {
            std::string leaf = favorite.page;
            const size_t slash = leaf.rfind('/');
            if (slash != std::string::npos)
                leaf = leaf.substr(slash + 1);
            if (ALKitLazyPage* lazy = findChild<ALKitLazyPage>(leaf))
                lazy->ensureBuilt();
            source_page = findBuiltPage(favorite.page);
        }
        ALKitRow* source = source_page ? source_page->findRow(favorite.widget) : nullptr;
        if (ALKitRow* proxy = ALKitRow::createProxy(source, "quick_proxy_" + llformat("%d", index++)))
            proxies.push_back(proxy);
    }
    card->setQuickRows(std::move(proxies));
    if (mChangedOnly || !mSearch.queryText().empty())
        quick->applyLocalFilter(mSearch.queryText(), mChangedOnly);
    quick->requestLayout();
    mRebuildingQuick = false;
}

void ALKitShell::showRowInSearch(ALKitRow* row)
{
    if (!row)
        return;
    if (ALKitRow* source = row->sourceRow())
        row = source;
    const ALKitRowMeta meta = row->meta();
    navigateTo(meta.page_key + "/" + meta.widget);
    if (ALKitPage* page = currentKitPage())
    {
        page->setSearchText(meta.label);
        queueSearch(meta.label);
    }
}

void ALKitShell::refreshPageMarker(const std::string& page_key)
{
    bool changed = mPageChangedCounts[page_key] > 0;
    if (ALKitPage* page = findBuiltPage(page_key))
        changed = changed || page->changed();
    mPageChanged[page_key] = changed;
}

void ALKitShell::refreshChangedSetting(const std::string& setting)
{
    const auto found = mSettingLocations.find(setting);
    if (found == mSettingLocations.end())
        return;
    LLControlVariable* control = gSavedSettings.getControl(setting).get();
    std::vector<std::string> touched;
    for (const ChangedLocation& location : found->second)
    {
        ChangedEntry& entry = mPageSettings[location.page][location.index];
        const bool changed = control && !ALKitQol::valuesEqual(
            control->get(), control->getDefault(), entry.decimals);
        if (changed != entry.changed)
        {
            mPageChangedCounts[location.page] += changed ? 1 : -1;
            entry.changed = changed;
        }
        if (std::find(touched.begin(), touched.end(), location.page) == touched.end())
            touched.push_back(location.page);
    }
    for (const std::string& page : touched)
        refreshPageMarker(page);
    updatePips();
}

void ALKitShell::refreshChangedMarkers(ALKitRow* changed_row)
{
    if (changed_row)
    {
        refreshPageMarker(changed_row->favoriteKey().page);
        if (changed_row->sourceRow())
            refreshPageMarker("quick_group/quick_tab");
        updatePips();
        if (mChangedOnly && currentKitPage())
        {
            ALKitPage* current = currentKitPage();
            const bool owns_row = current->findRow(changed_row->meta().widget) == changed_row ||
                                  (current->pageKey() == "quick_group/quick_tab" && changed_row->sourceRow());
            if (owns_row)
                current->refreshChangedFilter(changed_row, mSearch.queryText());
        }
        return;
    }
    mPageChanged.clear();
    mPageChangedCounts.clear();
    for (auto& page : mPageSettings)
        for (ChangedEntry& entry : page.second)
        {
            LLControlVariable* control = gSavedSettings.getControl(entry.setting).get();
            entry.changed = control && !ALKitQol::valuesEqual(
                control->get(), control->getDefault(), entry.decimals);
            if (entry.changed)
                ++mPageChangedCounts[page.first];
        }
    for (const auto& page : mPageSettings)
        refreshPageMarker(page.first);
    for (const auto& page : mBuiltPages)
        refreshPageMarker(page.first);
    updatePips();
}

void ALKitShell::setChangedOnly(bool enabled)
{
    if (mChangedOnly == enabled)
        return;
    mChangedOnly = enabled;
    for (const auto& entry : mBuiltPages)
        if (ALKitPage* page = entry.second.get())
            page->setChangedOnlyValue(enabled);
    const std::string query = mSearch.queryText();
    runSearch(query);
}

bool ALKitShell::closePopover()
{
    if (!mOpenPopover || !mOpenPopover->getVisible())
        return false;
    mOpenPopover->hide();
    mOpenPopover = nullptr;
    return true;
}

void ALKitShell::setOpenPopover(ALKitPopover* popover)
{ mOpenPopover = popover; }

void ALKitShell::popoverHidden(ALKitPopover* popover)
{
    if (mOpenPopover == popover)
        mOpenPopover = nullptr;
}

bool ALKitShell::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (mOpenPopover && mOpenPopover->getVisible())
    {
        S32 local_x = 0;
        S32 local_y = 0;
        localPointToOtherView(x, y, &local_x, &local_y, mOpenPopover);
        if (!mOpenPopover->pointInView(local_x, local_y))
        {
            // A click outside the results popup disarms it so a pending debounced
            // search cannot reopen it; clicking into the box re-arms it.
            if (mOpenPopover == mSearchPopover)
                mSearchPopupArmed = false;
            closePopover();
        }
    }
    return LLPanel::handleMouseDown(x, y, mask);
}

bool ALKitShell::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    if (mTabStrip && mTabNext && mTabNext->getVisible())
    {
        S32 local_x = 0;
        S32 local_y = 0;
        localPointToOtherView(x, y, &local_x, &local_y, mTabStrip);
        if (mTabStrip->pointInView(local_x, local_y) && delta.mClicks != 0)
        {
            mTabOffset = llmax(0, mTabOffset + (delta.mClicks > 0 ? 1 : -1));
            layoutChrome();
            LLSliderWheel::noteScroll(this);
            return true;
        }
    }
    return LLPanel::handleScrollWheel(x, y, delta);
}
