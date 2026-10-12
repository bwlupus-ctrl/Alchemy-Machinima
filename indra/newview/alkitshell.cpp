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
#include "lleventtimer.h"
#include "llfloater.h"
#include "lllineeditor.h"
#include "llrender.h"
#include "llsliderwheel.h"
#include "llscrolllistitem.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
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
    mRosterOpen   = gSavedSettings.getBOOL("DirectorV2CastRosterOpen");
    buildChrome();
    mConnections.emplace_back(setPageBuiltCallback([this](const std::string& name, LLPanel* panel) { pageBuilt(name, panel); }));
    layoutChrome();
    ALKitStyle::styleTree(this);
    ALKitStyle::stylePill(findChild<LLButton>("btn_cast_chip"));
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
    LLTextBox::Params text_params;
    text_params.name = "kit_search_header";
    text_params.rect = LLRect(10, 380, 420, 358);
    mSearchHeader    = LLUICtrlFactory::create<LLTextBox>(text_params);
    mSearchPopover->addChild(mSearchHeader);
    for (S32 index = 0; index < 12; ++index)
    {
        LLButton::Params result_params;
        result_params.name  = "kit_search_result_" + llformat("%d", index);
        result_params.rect  = LLRect(10, 354 - index * 25, 420, 331 - index * 25);
        result_params.label = "";
        LLButton* button    = LLUICtrlFactory::create<LLButton>(result_params);
        mSearchPopover->addChild(button);
        LLHandle<ALKitShell> handle = getDerivedHandle<ALKitShell>();
        button->setCommitCallback(
            [handle, index](LLUICtrl*, const LLSD&)
            {
                if (ALKitShell* shell = handle.get())
                {
                    shell->mSearchSelection = index;
                    shell->activateSearchResult();
                }
            });
        mSearchButtons.push_back(button);
    }
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
    static const PopoverButton popover_buttons[] = { { "btn_arm", "kit_arm_popover" }, { "btn_hotkeys", "kit_hotkeys_popover" } };
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
        const std::string label = mRailLabels ? mRailData[index].label + (off ? "  \xE2\x97\x8B" : "") : std::string();
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
        const std::string label = tab.label + (off ? "  \xE2\x97\x8B" : "");
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
    if (group)
    {
        group->setVisible(false);
    }
    if (list)
    {
        const S32 list_bottom = hint ? hint->getRect().mTop + 4 : 34;
        const S32 list_top = llmax(list_bottom + 1, height - ALKitStyle::ROSTER_HDR_H - 4);
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
                   mTransport->getRect().mTop - 1, ALKitStyle::color(ALKitStyle::DIVIDER2).get());
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
    LLView* view = mHighlight.get();
    if (view && mHighlightTimer.getElapsedTimeF32() < ALKitStyle::FLASH_SEC)
    {
        LLRect rect;
        if (view->localRectToOtherView(view->getLocalRect(), &rect, this))
        {
            gGL.color4fv(ALKitStyle::color(ALKitStyle::ACCENT).get().mV);
            gl_rect_2d(rect, false);
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
            for (LLView* ancestor = view->getParent(); ancestor; ancestor = ancestor->getParent())
            {
                if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(ancestor))
                {
                    LLRect rect;
                    if (LLView* document = scroller->getScrolledView())
                    {
                        if (view->localRectToOtherView(view->getLocalRect(), &rect, document))
                            scroller->scrollToShowRect(rect);
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
    return built;
}

void ALKitShell::syncSelection()
{
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
                    shell->queueSearch(line->getText());
            },
            nullptr);
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
}

void ALKitShell::runSearch(const std::string& query)
{
    mSearch.setQuery(query);
    ALKitPage* page = currentKitPage();
    if (!page)
        return;
    if (query.empty())
    {
        page->clearSearch();
        if (mSearchPopover)
            mSearchPopover->hide();
        mSearchHits.clear();
        mHasTopSearchHit = false;
        return;
    }
    mSearch.setCurrentPage(page->pageKey());
    ALKitSearch::Hits hits = mSearch.query(query);
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
    for (const ALKitSearch::Hit& hit : hits)
    {
        if (hit.page_key == page->pageKey())
            ++local_count;
        else if (global)
        {
            ++elsewhere;
            if (mSearchHits.size() < 12)
                mSearchHits.push_back(hit);
        }
    }
    mSearchSelection      = 0;
    mSearchSelectionMoved = false;
    if (mSearchHeader)
        mSearchHeader->setText(llformat("%d on this page \xC2\xB7 %d elsewhere", local_count, elsewhere));
    for (S32 index = 0; index < (S32)mSearchButtons.size(); ++index)
    {
        const bool visible = index < (S32)mSearchHits.size();
        mSearchButtons[index]->setVisible(visible);
        if (visible)
        {
            std::string label = mSearchHits[index].path;
            if (mSearchHits[index].advanced)
                label += "  More";
            mSearchButtons[index]->setLabel(label);
        }
    }
    if (mSearchFooter)
        mSearchFooter->setText(llformat("Showing the best %d of %d. Add a word to narrow it down.", (S32)mSearchHits.size(), elsewhere));
    if (mSearchPopover)
    {
        if (global && !mSearchHits.empty())
            mSearchPopover->show();
        else
            mSearchPopover->hide();
    }
}

bool ALKitShell::handleSearchKey(KEY key)
{
    if (key == KEY_RETURN && mSearchSelectionMoved && !mSearchHits.empty())
    {
        activateSearchResult();
        return true;
    }
    if (key == KEY_RETURN && mHasTopSearchHit)
    {
        ALKitTarget target;
        target.page_key = mTopSearchHit.page_key;
        target.card_id  = mTopSearchHit.card_id;
        target.widget   = mTopSearchHit.widget;
        target.setting  = mTopSearchHit.setting;
        target.option   = mTopSearchHit.option;
        navigateTo(target);
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
    for (S32 index = 0; index < (S32)mSearchButtons.size(); ++index)
        mSearchButtons[index]->setToggleState(index == mSearchSelection);
    return true;
}

void ALKitShell::activateSearchResult()
{
    if (mSearchSelection < 0 || mSearchSelection >= (S32)mSearchHits.size())
        return;
    const ALKitSearch::Hit& hit = mSearchHits[mSearchSelection];
    ALKitTarget             target;
    target.page_key = hit.page_key;
    target.card_id  = hit.card_id;
    target.widget   = hit.widget;
    target.setting  = hit.setting;
    target.option   = hit.option;
    navigateTo(target);
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
        });
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
        page->clearSearch();
    }
    mSearchHits.clear();
    mHasTopSearchHit = false;
    if (mSearchPopover)
        mSearchPopover->hide();
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

bool ALKitShell::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (mOpenPopover && mOpenPopover->getVisible())
    {
        S32 local_x = 0;
        S32 local_y = 0;
        localPointToOtherView(x, y, &local_x, &local_y, mOpenPopover);
        if (!mOpenPopover->pointInView(local_x, local_y))
            closePopover();
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
