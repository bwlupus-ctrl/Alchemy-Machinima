/**
 * @file fsinventorycustomtabs.cpp
 * @brief User-defined inventory filter tabs (FIRE-35598) - feature idea: Catznip
 * Ported from Firestorm (PanteraPolnocy, FIRE-35598); concept from Catznip
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsinventorycustomtabs.h"

#include "llbutton.h"
#include "llfiltereditor.h"
#include "llfolderviewitem.h"
#include "llinventoryfilter.h"
#include "llinventorymodel.h"
#include "llinventorypanel.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpanelmaininventory.h"
#include "llsdparam.h"
#include "llsdutil.h"
#include "llstring.h"
#include "lltabcontainer.h"
#include "lltrans.h"
#include "llui.h"
#include "lluicolortable.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"

namespace
{
    constexpr char SETTING_KEY[]        = "FSInventoryCustomTabs";
    constexpr char DEFAULT_NAME_KEY[]   = "FSInventoryCustomTabDefaultName";
    constexpr char RENAME_NOTIFY[]      = "FSInventoryCustomTabRename";
    constexpr char CLOSE_NOTIFY[]       = "FSInventoryCustomTabClose";
    constexpr char MENU_FILE[]          = "menu_inventory_custom_tab.xml";
    constexpr char ADD_TAB_PANEL_NAME[] = "FSInventoryCustomTabAdd";
    constexpr char ADD_TAB_LABEL[]      = "+";
    constexpr char FALLBACK_LABEL[]     = "Custom";
    constexpr char CLOSE_ICON_NAME[]    = "Icon_Close_Foreground";
    constexpr char CLOSE_ICON_PRESS[]   = "Icon_Close_Press";
    constexpr char TAB_BUTTON_PREFIX[]  = "htab_";
    constexpr char SHOW_HIDDEN_PANEL_NAME[] = "FSInventoryCustomTabShowHidden";
    constexpr char SHOW_HIDDEN_ICON_NAME[]  = "Arrow_Down";
    constexpr char HIDDEN_MENU_NAME[]   = "fs_inv_hidden_tabs_menu";
    constexpr char TABHIDDEN_KEY[]      = "tabhidden";
    constexpr S32  CLOSE_ICON_SIZE      = 16;
    constexpr S32  CLOSE_ICON_MARGIN    = 4;
    constexpr S32  CLOSE_BUTTON_RESERVE = CLOSE_ICON_SIZE + CLOSE_ICON_MARGIN + 2;
    constexpr S32  ADD_TAB_MIN_WIDTH    = 18;
    using namespace FSInventoryCustomTabModel;
    std::set<FSInventoryCustomTabs*>& allInstances()
    {
        static std::set<FSInventoryCustomTabs*> s;
        return s;
    }
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

FSInventoryCustomTabs::FSInventoryCustomTabs(LLPanelMainInventory* parent)
    : mParent(parent)
{
    if (!mParent)
    {
        return;
    }

    mTabs = mParent->mFilterTabs;
    if (!mTabs)
    {
        return;
    }

    mOriginalTabEllipses = mTabs->getUseTabEllipses();
    mTabs->setUseTabEllipses(true);

    // Runtime enable happens outside XUI construction's callback scope.
    auto& registrar = mParent->getCommitCallbackRegistrar();
    registrar.pushScope();
    if (gMenuHolder)
    {
        if (auto* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>(MENU_FILE, gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()))
        {
            mMenuHandle = menu->getHandle();
        }
    }

    registrar.popScope();

    installAddTab();
    installShowHiddenTab();
    allInstances().insert(this);
}

FSInventoryCustomTabs::~FSInventoryCustomTabs()
{
    allInstances().erase(this);
    if (mSettingConnection.connected())
    {
        mSettingConnection.disconnect();
    }
    if (mRenameNotification)
    {
        LLNotifications::instance().cancel(mRenameNotification);
        mRenameNotification.reset();
    }
    if (mCloseNotification)
    {
        LLNotifications::instance().cancel(mCloseNotification);
        mCloseNotification.reset();
    }
    if (mAddClickConnection.connected())
    {
        mAddClickConnection.disconnect();
    }
    if (mShowHiddenClickConnection.connected())
    {
        mShowHiddenClickConnection.disconnect();
    }
    if (mTabs && !mPanels.empty() && !mLoading)
    {
        save();
    }
    mFocusLostConnection.disconnect();
    clearCustomTabs();
    for (auto* panel : { mShowHiddenTabPanel, mAddTabPanel })
    {
        if (panel && mTabs)
        {
            detachTabPanel(panel);
            panel->die();
        }
    }
    if (mTabs) mTabs->setUseTabEllipses(mOriginalTabEllipses);
    if (auto* menu = mMenuHandle.get())
    {
        menu->die();
        mMenuHandle.markDead();
    }
    if (auto* hidden_menu = mHiddenMenuHandle.get())
    {
        if (gMenuHolder)
        {
            gMenuHolder->removeChild(hidden_menu);
        }
        delete hidden_menu;
        mHiddenMenuHandle.markDead();
    }
}

// ---------------------------------------------------------------------------
// Public hooks called from LLPanelMainInventory
// ---------------------------------------------------------------------------

void FSInventoryCustomTabs::registerCommitCallbacks(LLPanelMainInventory* parent)
{
    if (!parent)
    {
        return;
    }
    auto& reg = parent->getCommitCallbackRegistrar();
    reg.add("FSInventoryCustomTab.Rename", [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onRenameClicked(); });
    reg.add("FSInventoryCustomTab.Clone",  [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onCloneClicked();  });
    reg.add("FSInventoryCustomTab.Close",  [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onCloseClicked();  });
    reg.add("FSInventoryCustomTab.Hide",   [parent](LLUICtrl*, const LLSD&) { if (auto* self = instanceFor(parent)) self->onHideClicked();   });
}

bool FSInventoryCustomTabs::handleRightMouseDown(S32 x, S32 y)
{
    auto* menu = static_cast<LLMenuGL*>(mMenuHandle.get());
    if (!menu)
    {
        return false;
    }

    S32 local_x = 0;
    S32 local_y = 0;
    auto* inv_panel = hitTestCustomTab(x, y, local_x, local_y);
    if (!inv_panel)
    {
        return false;
    }

    mContextPanel = inv_panel;
    menu->buildDrawLabels();
    menu->updateParent(LLMenuGL::sMenuContainer);
    LLMenuGL::showPopup(mParent, menu, x, y);
    return true;
}

bool FSInventoryCustomTabs::handleMouseDown(S32 x, S32 y)
{
    S32 local_x = 0;
    S32 local_y = 0;
    auto* inv_panel = hitTestCustomTab(x, y, local_x, local_y);
    if (!inv_panel)
    {
        return false;
    }

    auto* tab_btn = findTabButton(inv_panel);
    if (!tab_btn)
    {
        return false;
    }

    const S32 btn_local_x = local_x - tab_btn->getRect().mLeft;
    if (btn_local_x < tab_btn->getRect().getWidth() - CLOSE_BUTTON_RESERVE)
    {
        return false;
    }

    onClosePressed(inv_panel);
    return true;
}

bool FSInventoryCustomTabs::handleDragAndDrop(S32 x, S32 y)
{
    S32 local_x = 0;
    S32 local_y = 0;
    auto* inv_panel = hitTestAddTab(x, y, local_x, local_y);
    if (!inv_panel)
    {
        return false;
    }
    // Drag and drop onto the Add button should not do anything
    // Return true here to prevent calling LLPanel::handleDragAndDrop
    return true;
}

void FSInventoryCustomTabs::onFilterFocusLost()
{
    if (mParent)
    {
        auto* active = dynamic_cast<LLInventoryPanel*>(mParent->getActivePanel());
        if (active && isCustomTab(active) && !mExplicitlyNamed.count(active))
        {
            updateAutoLabel(active);
        }
    }
    save();
}

void FSInventoryCustomTabs::onAddTabSelected()
{
    if (!mTabs)
    {
        return;
    }

    const S32 add_idx = mTabs->getIndexForPanel(mAddTabPanel);
    const S32 last_idx = mLastActivePanel ? mTabs->getIndexForPanel(mLastActivePanel) : -1;
    const S32 count = mTabs->getTabCount();

    if (add_idx >= 0 && last_idx >= 0)
    {
        if (last_idx < add_idx && add_idx + 1 < count)
        {
            mTabs->selectTab(add_idx + 1);
            return;
        }
        if (last_idx > add_idx && add_idx - 1 >= 0)
        {
            mTabs->selectTab(add_idx - 1);
            return;
        }
    }

    if (mLastActivePanel && last_idx >= 0)
    {
        mTabs->selectTabPanel(mLastActivePanel);
    }
    else
    {
        mTabs->selectFirstTab();
    }
}

void FSInventoryCustomTabs::onAddClicked()
{
    if (addTab(std::string(), std::string(), true))
    {
        save();
    }
}

void FSInventoryCustomTabs::onShowHiddenClicked()
{
    if (!mTabs || mHidden.empty())
    {
        return;
    }

    if (auto* old_menu = mHiddenMenuHandle.get())
    {
        if (gMenuHolder)
        {
            gMenuHolder->removeChild(old_menu);
        }
        delete old_menu;
        mHiddenMenuHandle.markDead();
    }

    LLContextMenu::Params menu_params;
    menu_params.name(HIDDEN_MENU_NAME);
    menu_params.visible(false);
    LLContextMenu* menu = LLUICtrlFactory::create<LLContextMenu>(menu_params);
    if (!menu)
    {
        return;
    }

    auto handle = getHandle();
    const S32 count = mTabs->getTabCount();
    for (S32 i = 0; i < count; ++i)
    {
        auto* inv_panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByIndex(i));
        if (!inv_panel || !mHidden.count(inv_panel))
        {
            continue;
        }

        std::string label = computeDisplayLabel(inv_panel);
        if (label.empty())
        {
            label = getDefaultTabName();
        }

        const std::string panel_name = inv_panel->getName();
        LLMenuItemCallGL::Params item_params;
        item_params.name(panel_name);
        item_params.label(label);
        item_params.on_click.function([handle, panel_name](LLUICtrl*, const LLSD&)
        {
            if (auto* self = handle.get())
            {
                if (self->mTabs)
                {
                    self->showTab(dynamic_cast<LLInventoryPanel*>(self->mTabs->getPanelByName(panel_name)));
                }
            }
        });
        menu->addChild(LLUICtrlFactory::create<LLMenuItemCallGL>(item_params));
    }

    mHiddenMenuHandle = menu->getHandle();
    if (gMenuHolder)
    {
        gMenuHolder->addChild(menu);
    }

    if (auto* btn = findTabButton(mShowHiddenTabPanel))
    {
        S32 screen_x = 0;
        S32 screen_y = 0;
        btn->localPointToScreen(0, 0, &screen_x, &screen_y);
        menu->show(screen_x, screen_y, btn);
    }
}

void FSInventoryCustomTabs::noteActivePanel(LLInventoryPanel* panel)
{
    if (mLastActivePanel && mLastActivePanel != panel && isCustomTab(mLastActivePanel))
    {
        if (!mExplicitlyNamed.count(mLastActivePanel))
        {
            updateAutoLabel(mLastActivePanel);
        }
        save();
    }
    if (panel && panel != mAddTabPanel && panel != mShowHiddenTabPanel && mTabs && mTabs->getIndexForPanel(panel) >= 0)
    {
        mLastActivePanel = panel;
    }
}

void FSInventoryCustomTabs::onClosePressed(LLInventoryPanel* panel)
{
    if (!isCustomTab(panel))
    {
        return;
    }
    mContextPanel = panel;
    onCloseClicked();
}

void FSInventoryCustomTabs::notifyActiveFilterStateChanged()
{
    if (!mParent || !isCustomTab(mParent->getActivePanel()))
    {
        return;
    }
    save();
}

FSInventoryCustomTabs* FSInventoryCustomTabs::instanceFor(LLPanelMainInventory* parent)
{
    return parent ? parent->mCustomTabs.get() : nullptr;
}

const FSInventoryCustomTabs* FSInventoryCustomTabs::instanceFor(const LLPanelMainInventory* parent)
{
    return parent ? parent->mCustomTabs.get() : nullptr;
}

void FSInventoryCustomTabs::install(LLPanelMainInventory* parent, LLFilterEditor* filter_editor)
{
    if (!parent)
    {
        return;
    }
    if (!gSavedSettings.getBOOL("AlchemyInventoryCustomTabs") || parent->mCustomTabs)
    {
        return;
    }
    parent->mCustomTabs = std::make_unique<FSInventoryCustomTabs>(parent);
    parent->mCustomTabs->load();
    parent->mCustomTabs->attachToFilterEditor(filter_editor);
}

void FSInventoryCustomTabs::notifyFilterStateChanged(LLPanelMainInventory* parent)
{
    if (auto* self = instanceFor(parent))
    {
        self->notifyActiveFilterStateChanged();
    }
}

bool FSInventoryCustomTabs::activePanelIsCustom(const LLPanelMainInventory* parent)
{
    const auto* self = instanceFor(parent);
    return self && self->isCustomTab(parent->getActivePanel());
}

bool FSInventoryCustomTabs::maybeHandleAddTabSelected(LLPanelMainInventory* parent)
{
    auto* self = instanceFor(parent);
    if (!self || !parent->mFilterTabs)
    {
        return false;
    }
    const LLPanel* current = parent->mFilterTabs->getCurrentPanel();
    if (!self->isAddTab(current) && !self->isShowHiddenTab(current))
    {
        return false;
    }
    self->onAddTabSelected();
    return true;
}

void FSInventoryCustomTabs::noteActivePanel(LLPanelMainInventory* parent, LLInventoryPanel* panel)
{
    if (auto* self = instanceFor(parent))
    {
        self->noteActivePanel(panel);
    }
}

S32 FSInventoryCustomTabs::captureFilterGeneration(const LLPanelMainInventory* parent)
{
    if (!parent || !instanceFor(parent))
    {
        return 0;
    }
    const auto* active = parent->getActivePanel();
    return active ? active->getFilter().getCurrentGeneration() : 0;
}

void FSInventoryCustomTabs::notifyIfFilterChanged(LLPanelMainInventory* parent, S32 prev_generation)
{
    auto* self = instanceFor(parent);
    if (!self)
    {
        return;
    }
    const auto* active = parent->getActivePanel();
    if (active && active->getFilter().getCurrentGeneration() != prev_generation)
    {
        self->notifyActiveFilterStateChanged();
    }
}

bool FSInventoryCustomTabs::handleRightMouseDown(LLPanelMainInventory* parent, S32 x, S32 y)
{
    auto* self = instanceFor(parent);
    return self && self->handleRightMouseDown(x, y);
}

bool FSInventoryCustomTabs::handleMouseDown(LLPanelMainInventory* parent, S32 x, S32 y)
{
    auto* self = instanceFor(parent);
    return self && self->handleMouseDown(x, y);
}

bool FSInventoryCustomTabs::handleDragAndDrop(LLPanelMainInventory* parent, S32 x, S32 y)
{
    auto* self = instanceFor(parent);
    return self && self->handleDragAndDrop(x, y);
}

void FSInventoryCustomTabs::onParentDraw(LLPanelMainInventory* parent)
{
    if (auto* self = instanceFor(parent))
    {
        self->ensureRootOpenForActivePanel();
    }
}

void FSInventoryCustomTabs::ensureRootOpenForActivePanel()
{
    if (mPendingRootOpen.empty() || !mParent)
    {
        return;
    }
    auto* active = dynamic_cast<LLInventoryPanel*>(mParent->getActivePanel());
    if (!active)
    {
        return;
    }
    const auto it = mPendingRootOpen.find(active);
    if (it == mPendingRootOpen.end())
    {
        return;
    }
    if (auto* folder = dynamic_cast<LLFolderViewFolder*>(active->getItemByID(gInventory.getRootFolderID())))
    {
        static U64 sPanelBuilds = 0;
        ++sPanelBuilds;
        LL_DEBUGS("InvCustomTabs") << "panel_build=" << sPanelBuilds
            << " panel=" << active->getName() << LL_ENDL;
        folder->setOpen(true);
        active->getFilter().setModified(LLInventoryFilter::FILTER_RESTART);
        mPendingRootOpen.erase(it);
    }
}

// ---------------------------------------------------------------------------
// Tab management
// ---------------------------------------------------------------------------

void FSInventoryCustomTabs::insertTabPanel(LLPanel* panel, const std::string& label, LLPanel* before)
{
    LLPanel* current = mTabs->getCurrentPanel();
    if (!current)
    {
        mTabs->selectFirstTab();
        current = mTabs->getCurrentPanel();
    }
    const S32 before_index = before ? mTabs->getIndexForPanel(before) : -1;
    if (before_index >= 0) mTabs->setCurrentPanelIndex(before_index);
    mTabs->addTabPanel(LLTabContainer::TabPanelParams().panel(panel).label(label).select_tab(false)
        .insert_at(before_index >= 0 ? LLTabContainer::LEFT_OF_CURRENT : LLTabContainer::END));
    if (current) mTabs->setCurrentPanelIndex(mTabs->getIndexForPanel(current));
    else mTabs->setCurrentPanelIndex(-1); // No invisible inserted tab may become current.
}

void FSInventoryCustomTabs::detachTabPanel(LLPanel* panel)
{
    // removeTabPanel selects by numeric index after erasing. Preserve the current
    // panel so deleting an earlier tab cannot reveal/build an unrelated lazy tab.
    if (mTabs->getCurrentPanel() == panel) mTabs->selectFirstTab();
    const S32 index = mTabs->getIndexForPanel(panel);
    const S32 current = mTabs->getCurrentPanelIndex();
    if (index >= 0 && index < current) mTabs->setCurrentPanelIndex(current - 1);
    mTabs->removeTabPanel(panel, false);
}

void FSInventoryCustomTabs::installAddTab()
{
    if (!mTabs || mAddTabPanel)
    {
        return;
    }

    LLPanel::Params params;
    params.name(ADD_TAB_PANEL_NAME);
    params.follows.flags(FOLLOWS_ALL);

    mAddTabPanel = LLUICtrlFactory::create<LLPanel>(params);
    if (!mAddTabPanel)
    {
        return;
    }

    mTabs->addTabPanel(LLTabContainer::TabPanelParams()
        .panel(mAddTabPanel)
        .label(ADD_TAB_LABEL)
        .select_tab(false)
        .insert_at(LLTabContainer::END));

    if (auto* btn = findTabButton(mAddTabPanel))
    {
        const S32 minimum = mTabs->getMinTabWidth();
        mTabs->setMinTabWidth(llmax(minimum / 2, ADD_TAB_MIN_WIDTH));
        mTabs->setTabPadding(mTabs->getIndexForPanel(mAddTabPanel), 0);
        mTabs->setMinTabWidth(minimum);
        btn->setLeftHPad(2);
        btn->setRightHPad(2);
        mAddClickConnection = btn->setClickedCallback(boost::bind(&FSInventoryCustomTabs::onAddClicked, this));
    }
}

void FSInventoryCustomTabs::installShowHiddenTab()
{
    if (!mTabs || mShowHiddenTabPanel)
    {
        return;
    }

    LLPanel::Params params;
    params.name(SHOW_HIDDEN_PANEL_NAME);
    params.follows.flags(FOLLOWS_ALL);

    mShowHiddenTabPanel = LLUICtrlFactory::create<LLPanel>(params);
    if (!mShowHiddenTabPanel)
    {
        return;
    }

    insertTabPanel(mShowHiddenTabPanel, LLStringUtil::null, mAddTabPanel);

    if (auto* btn = findTabButton(mShowHiddenTabPanel))
    {
        const S32 minimum = mTabs->getMinTabWidth();
        mTabs->setMinTabWidth(llmax(minimum / 2, ADD_TAB_MIN_WIDTH));
        mTabs->setTabPadding(mTabs->getIndexForPanel(mShowHiddenTabPanel), 0);
        mTabs->setMinTabWidth(minimum);
        btn->setLeftHPad(2);
        btn->setRightHPad(2);
        btn->setImageOverlay(std::string(SHOW_HIDDEN_ICON_NAME));
        std::string tooltip;
        if (LLTrans::findString(tooltip, SHOW_HIDDEN_PANEL_NAME) && !tooltip.empty())
        {
            btn->setToolTip(tooltip);
        }
        mShowHiddenClickConnection = btn->setClickedCallback(boost::bind(&FSInventoryCustomTabs::onShowHiddenClicked, this));
    }

    mTabs->setTabButtonVisible(mShowHiddenTabPanel, false);
}

LLInventoryPanel* FSInventoryCustomTabs::addTab(const std::string& explicit_name, const std::string& filter, bool select)
{
    if (!mTabs || !mParent)
    {
        return nullptr;
    }
    if (!mLoading && !mLoadedOk)
    {
        notifyIncompleteLoad();
        return nullptr;
    }

    if (mPanels.size() >= static_cast<size_t>(MAX_TABS))
    {
        LL_WARNS() << "FSInventoryCustomTabs: refusing to add tab; reached cap of " << MAX_TABS << LL_ENDL;
        return nullptr;
    }

    static S32 sCounter = 0;
    const auto internal_name = llformat("FSCustomInvTab_%d", ++sCounter);

    LLInventoryPanel::Params params;
    params.name(internal_name);
    params.sort_order_setting(LLInventoryPanel::INHERIT_SORT_ORDER);
    params.show_item_link_overlays(true);
    params.background_visible(true);
    params.bg_opaque_color(LLUIColorTable::instance().getColor("DkGray2"));
    params.bg_alpha_color(LLUIColorTable::instance().getColor("DkGray2"));
    params.has_border(false);
    params.border.bevel_style(LLViewBorder::BEVEL_NONE);
    params.scroll.reserve_scroll_corner(false);
    params.folder.double_click_override(true);
    params.preinitialize_views(false);
    params.follows.flags(FOLLOWS_ALL);

    auto* panel = LLUICtrlFactory::create<LLInventoryPanel>(params);
    if (!panel)
    {
        return nullptr;
    }

    panel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::DEFAULT_SORT_ORDER));
    panel->getFilter().markDefault();
    // Bind through a handle: a closed tab's panel lives on until LLMortician runs,
    // and its window may be destroyed first.
    LLHandle<LLPanelMainInventory> parent_handle = mParent->getDerivedHandle<LLPanelMainInventory>();
    panel->setSelectCallback([parent_handle, panel](const std::deque<LLFolderViewItem*>& items, bool user_action)
    {
        if (LLPanelMainInventory* parent = parent_handle.get())
        {
            parent->onSelectionChange(panel, items, user_action);
        }
    });
    mPanels.insert(panel);
    mPendingRootOpen.insert(panel);

    const std::string auto_label = filter.empty() ? getDefaultTabName() : filter;
    if (!explicit_name.empty())
    {
        mExplicitlyNamed.insert(panel);
    }
    if (!filter.empty())
    {
        panel->setFilterSubString(filter);
    }

    const std::string& display_label = explicit_name.empty() ? auto_label : explicit_name;

    LLPanel* anchor = mShowHiddenTabPanel ? static_cast<LLPanel*>(mShowHiddenTabPanel) : static_cast<LLPanel*>(mAddTabPanel);
    insertTabPanel(panel, display_label, anchor);

    decorateCustomTabButton(panel);
    if (auto* tab_btn = findTabButton(panel))
    {
        tab_btn->setToolTip(display_label);
    }
    if (const S32 idx = mTabs->getIndexForPanel(panel); idx >= 0)
    {
        mTabs->setTabPadding(idx, CLOSE_BUTTON_RESERVE);
    }
    if (select)
    {
        mTabs->selectTabPanel(panel);
    }
    return panel;
}

void FSInventoryCustomTabs::onRenameClicked()
{
    if (!isCustomTab(mContextPanel))
    {
        return;
    }

    if (mRenameNotification)
    {
        LLNotifications::instance().cancel(mRenameNotification);
        mRenameNotification.reset();
    }

    LLSD args;
    args["NAME"] = mContextPanel->getLabel();
    LLSD payload;
    payload["panel_name"] = mContextPanel->getName();

    auto handle = getHandle();
    mRenameNotification = LLNotificationsUtil::add(RENAME_NOTIFY, args, payload,
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (auto* self = handle.get())
            {
                self->onRenameConfirmed(notification, response);
            }
        });
}

void FSInventoryCustomTabs::onRenameConfirmed(const LLSD& notification, const LLSD& response)
{
    mRenameNotification.reset();

    if (!mTabs || LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return;
    }

    std::string new_name = sanitizeTabName(response["new_name"].asString());
    if (new_name.empty())
    {
        return;
    }

    auto* panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByName(notification["payload"]["panel_name"].asString()));
    if (!isCustomTab(panel))
    {
        return;
    }

    mExplicitlyNamed.insert(panel);
    applyTabLabel(panel, new_name);
    save();
}

void FSInventoryCustomTabs::onCloneClicked()
{
    if (!isCustomTab(mContextPanel))
    {
        return;
    }
    auto& src_filter = mContextPanel->getFilter();
    const std::string explicit_name = mExplicitlyNamed.count(mContextPanel) ? mContextPanel->getLabel() : std::string();
    auto* new_panel = addTab(explicit_name, src_filter.getFilterSubStringOrig(), true);
    if (!new_panel)
    {
        return;
    }
    LLInventoryFilter::Params filter_params;
    src_filter.toParams(filter_params);
    if (filter_params.validateBlock(false))
    {
        auto& new_filter = new_panel->getFilter();
        new_filter.fromParams(filter_params);
        new_panel->setFilterSubString(src_filter.getFilterSubStringOrig());
        new_filter.setShowFolderState(src_filter.getShowFolderState());
        new_filter.setFilterCreator(src_filter.getFilterCreatorType());
        new_filter.setFilterLinks(src_filter.getFilterLinks());
        new_filter.setSearchType(src_filter.getSearchType());
    }
    save();
}

void FSInventoryCustomTabs::onCloseClicked()
{
    if (!isCustomTab(mContextPanel) || !mTabs)
    {
        return;
    }

    if (mCloseNotification)
    {
        LLNotifications::instance().cancel(mCloseNotification);
        mCloseNotification.reset();
    }

    LLSD args;
    args["NAME"] = mContextPanel->getLabel();
    LLSD payload;
    payload["panel_name"] = mContextPanel->getName();

    auto handle = getHandle();
    auto pending_notification = LLNotificationsUtil::add(CLOSE_NOTIFY, args, payload,
        [handle](const LLSD& notification, const LLSD& response)
        {
            if (auto* self = handle.get())
            {
                self->onCloseConfirmed(notification, response);
            }
        });
    mCloseNotification = pending_notification && !pending_notification->isRespondedTo() && !pending_notification->isCancelled()
        ? pending_notification : LLNotificationPtr();
}

void FSInventoryCustomTabs::onCloseConfirmed(const LLSD& notification, const LLSD& response)
{
    mCloseNotification.reset();

    if (!mTabs || LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return;
    }

    auto* to_remove = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByName(notification["payload"]["panel_name"].asString()));
    if (!isCustomTab(to_remove) || !mTabs)
    {
        return;
    }

    if (mContextPanel == to_remove)
    {
        mContextPanel = nullptr;
    }
    if (mLastActivePanel == to_remove)
    {
        mLastActivePanel = nullptr;
    }

    if (mTabs->getCurrentPanel() == to_remove)
    {
        mTabs->selectFirstTab();
    }

    mPanels.erase(to_remove);
    mEntries.erase(to_remove);
    mExplicitlyNamed.erase(to_remove);
    mPendingRootOpen.erase(to_remove);
    mHidden.erase(to_remove);
    detachTabPanel(to_remove);
    to_remove->die();
    updateShowHiddenTabVisibility();
    save();
}

void FSInventoryCustomTabs::onHideClicked()
{
    if (isCustomTab(mContextPanel))
    {
        hideTab(mContextPanel);
    }
}

void FSInventoryCustomTabs::hideTab(LLInventoryPanel* panel)
{
    if (!mTabs || !isCustomTab(panel) || mHidden.count(panel))
    {
        return;
    }
    const bool was_active = (mTabs->getCurrentPanel() == panel);
    mHidden.insert(panel);
    mTabs->setTabButtonVisible(panel, false);
    if (mContextPanel == panel)
    {
        mContextPanel = nullptr;
    }
    if (mLastActivePanel == panel)
    {
        mLastActivePanel = nullptr;
    }
    if (was_active)
    {
        selectNearestVisibleTab(panel);
    }
    updateShowHiddenTabVisibility();
    save();
}

void FSInventoryCustomTabs::showTab(LLInventoryPanel* panel)
{
    if (!mTabs || !isCustomTab(panel) || !mHidden.count(panel))
    {
        return;
    }
    mHidden.erase(panel);
    mTabs->setTabButtonVisible(panel, true);
    updateShowHiddenTabVisibility();
    mTabs->selectTabPanel(panel);
    save();
}

void FSInventoryCustomTabs::updateShowHiddenTabVisibility()
{
    if (mTabs && mShowHiddenTabPanel)
    {
        mTabs->setTabButtonVisible(mShowHiddenTabPanel, !mHidden.empty());
    }
}

void FSInventoryCustomTabs::selectNearestVisibleTab(LLInventoryPanel* skip_panel)
{
    if (!mTabs)
    {
        return;
    }

    const S32 skip_idx = mTabs->getIndexForPanel(skip_panel);
    const S32 count = mTabs->getTabCount();
    const auto selectable = [this](LLPanel* p) -> bool
    {
        return p && p != static_cast<LLPanel*>(mAddTabPanel) && p != static_cast<LLPanel*>(mShowHiddenTabPanel) && mTabs->getTabVisibility(p);
    };

    for (S32 i = skip_idx - 1; i >= 0; --i)
    {
        if (selectable(mTabs->getPanelByIndex(i)))
        {
            mTabs->selectTab(i);
            return;
        }
    }
    for (S32 i = skip_idx + 1; i < count; ++i)
    {
        if (selectable(mTabs->getPanelByIndex(i)))
        {
            mTabs->selectTab(i);
            return;
        }
    }
    mTabs->selectFirstTab();
}

void FSInventoryCustomTabs::clearCustomTabs()
{
    if (!mTabs)
    {
        return;
    }

    mLoadedOk = false;
    mContextPanel = nullptr;
    mLastActivePanel = nullptr;

    if (mTabs->getCurrentPanelIndex() != 0) mTabs->selectFirstTab();
    // All owned tabs follow All Items. Removal must not dispatch tab commits.

    auto panels_copy = mPanels;
    mPanels.clear();
    mEntries.clear();
    mExplicitlyNamed.clear();
    mPendingRootOpen.clear();
    mHidden.clear();
    for (auto* panel : panels_copy)
    {
        detachTabPanel(panel);
        panel->die();
    }

    mLastActivePanel = nullptr;
    mContextPanel = nullptr;
    updateShowHiddenTabVisibility();
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void FSInventoryCustomTabs::load()
{
    if (mLoading)
    {
        return;
    }
    mLoading = true;

    for (auto* other : allInstances())
    {
        if (other != this)
        {
            other->save();
        }
    }

    doLoad();
    subscribeToSetting();
    mLoading = false;
}

void FSInventoryCustomTabs::applyEntry(LLInventoryPanel* panel, const LLSD& entry)
{
    const bool filter_changed = FSInventoryCustomTabModel::filterStateChanged(mEntries[panel], entry);
    if (filter_changed)
    {
        const LLSD& states = entry["states"];
        auto& filter = panel->getFilter();
        filter.resetDefault();
        filter.setSearchType(LLInventoryFilter::SEARCHTYPE_NAME);
        if (states.isMap())
        {
            LLInventoryFilter::Params params;
            LLParamSDParser().readSD(states, params, true);
            filter.fromParams(params);
            // fromParams clears an omitted substring. Restore it before links, since
            // changing the substring cancels LLInventoryFilter's only-links mode.
            panel->setFilterSubString(entry["filter"].asString());
            const LLSD& ops = states["filter_ops"];
            if (ops.has("show_folder_state"))
                filter.setShowFolderState(static_cast<LLInventoryFilter::EFolderShow>(ops["show_folder_state"].asInteger()));
            if (ops.has("creator_type"))
                filter.setFilterCreator(static_cast<LLInventoryFilter::EFilterCreatorType>(ops["creator_type"].asInteger()));
            if (ops.has("links"))
                filter.setFilterLinks(static_cast<U64>(ops["links"].asInteger()));
            if (states.has("search_type"))
                filter.setSearchType(static_cast<LLInventoryFilter::ESearchType>(states["search_type"].asInteger()));
        }
        if (!states.isMap()) panel->setFilterSubString(entry["filter"].asString());
    }
    const LLSD& previous = mEntries[panel];
    const bool name_changed = !previous.isMap() || previous["name"].asString() != entry["name"].asString()
        || (filter_changed && entry["name"].asString().empty());
    if (name_changed)
    {
        if (entry["name"].asString().empty()) mExplicitlyNamed.erase(panel);
        else mExplicitlyNamed.insert(panel);
        applyTabLabel(panel, entry["name"].asString().empty() ? computeDisplayLabel(panel) : entry["name"].asString());
    }
    if (!previous.isMap() || previous["tabhidden"].asBoolean() != entry["tabhidden"].asBoolean())
    {
        if (entry["tabhidden"].asBoolean()) mHidden.insert(panel);
        else mHidden.erase(panel);
        mTabs->setTabButtonVisible(panel, !mHidden.count(panel));
    }
    mEntries[panel] = entry;
}

void FSInventoryCustomTabs::doLoad()
{
    const auto loaded = FSInventoryCustomTabModel::load(gSavedPerAccountSettings.getLLSD(SETTING_KEY));
    mLoadedOk = false;
    bool refresh_active = false;
    LLSD previous = LLSD::emptyArray();
    std::vector<LLInventoryPanel*> panels;
    for (S32 i = 0; i < mTabs->getTabCount(); ++i)
    {
        auto* panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByIndex(i));
        if (isCustomTab(panel))
        {
            panels.push_back(panel);
            previous.append(mEntries[panel]);
        }
    }
    const auto matches = FSInventoryCustomTabModel::reconcile(previous, loaded.tabs);
    std::set<LLInventoryPanel*> retained;
    for (S32 match : matches)
    {
        if (match >= 0) retained.insert(panels[static_cast<size_t>(match)]);
    }
    // Remove only deleted tabs. Updating a filter never recreates an inventory tree.
    for (auto* panel : panels)
    {
        if (retained.count(panel)) continue;
        cancelNotificationsForPanel(panel);
        if (mTabs->getCurrentPanel() == panel) mTabs->selectFirstTab();
        if (mLastActivePanel == panel) mLastActivePanel = nullptr;
        if (mContextPanel == panel) mContextPanel = nullptr;
        mPanels.erase(panel);
        mExplicitlyNamed.erase(panel);
        mPendingRootOpen.erase(panel);
        mHidden.erase(panel);
        mEntries.erase(panel);
        detachTabPanel(panel);
        panel->die();
    }
    bool complete = loaded.loadedOk;
    // The fixed tabs precede custom tabs; the two pseudo-tabs follow them.
    const S32 first = mTabs->getTabCount() - static_cast<S32>(mPanels.size())
        - (mAddTabPanel ? 1 : 0) - (mShowHiddenTabPanel ? 1 : 0);
    for (LLSD::Integer i = 0; i < loaded.tabs.size(); ++i)
    {
        LLSD entry = loaded.tabs[i];
        const S32 match = matches[static_cast<size_t>(i)];
        auto* panel = match >= 0 ? panels[static_cast<size_t>(match)] : addTab(entry["name"].asString(), entry["filter"].asString(), false);
        if (!panel)
        {
            complete = false;
            continue;
        }
        if (!entry.has("id"))
        {
            // Deterministic legacy IDs make independently opened windows agree.
            entry["id"] = "legacy-" + std::to_string(i);
        }
        if (!llsd_equals(mEntries[panel], entry))
        {
            refresh_active |= mParent->getActivePanel() == panel
                && FSInventoryCustomTabModel::filterStateChanged(mEntries[panel], entry);
            applyEntry(panel, entry);
        }
        if (mTabs->getIndexForPanel(panel) != first + i)
        {
            // Reorder only the button/host relationship, retaining the panel and tree.
            const bool active = mTabs->getCurrentPanel() == panel;
            detachTabPanel(panel);
            insertTabPanel(panel, panel->getLabel(), mTabs->getPanelByIndex(first + i));
            decorateCustomTabButton(panel);
            mTabs->setTabPadding(first + i, CLOSE_BUTTON_RESERVE);
            mTabs->setTabButtonVisible(panel, !mHidden.count(panel));
            if (active && !mHidden.count(panel)) mTabs->selectTabPanel(panel);
        }
    }
    auto* current = dynamic_cast<LLInventoryPanel*>(mTabs->getCurrentPanel());
    if (mHidden.count(current)) mTabs->selectFirstTab();
    updateShowHiddenTabVisibility();
    mLoadedOk = complete;
    mTabs->enableTabButton(mTabs->getIndexForPanel(mAddTabPanel), mLoadedOk);
    if (refresh_active) mParent->onFilterSelected();
    if (!mLoadedOk)
    {
        LL_WARNS("InvCustomTabs") << "Partial/invalid load; refusing to save custom tabs" << LL_ENDL;
        notifyIncompleteLoad();
    }
}

void FSInventoryCustomTabs::notifyIncompleteLoad()
{
    static bool sNotified = false;
    if (!sNotified)
    {
        sNotified = true;
        LLNotificationsUtil::add("FSInventoryCustomTabsLoadIncomplete");
    }
}

void FSInventoryCustomTabs::cancelNotificationsForPanel(const LLPanel* panel)
{
    if (mRenameNotification && mRenameNotification->getPayload()["panel_name"].asString() == panel->getName())
    {
        LLNotifications::instance().cancel(mRenameNotification);
        mRenameNotification.reset();
    }
    if (mCloseNotification && mCloseNotification->getPayload()["panel_name"].asString() == panel->getName())
    {
        LLNotifications::instance().cancel(mCloseNotification);
        mCloseNotification.reset();
    }
}

void FSInventoryCustomTabs::subscribeToSetting()
{
    if (mSettingConnection.connected())
    {
        return;
    }
    if (auto control = gSavedPerAccountSettings.getControl(SETTING_KEY))
    {
        mSettingConnection = control->getSignal()->connect(boost::bind(&FSInventoryCustomTabs::onSettingChangedExternally, this));
    }
}

void FSInventoryCustomTabs::attachToFilterEditor(LLFilterEditor* filter_editor)
{
    if (!filter_editor)
    {
        return;
    }
    auto* inner = filter_editor->findChild<LLLineEditor>("filter edit box");
    if (!inner)
    {
        return;
    }
    auto handle = getHandle();
    mFocusLostConnection = inner->setFocusLostCallback([handle](LLFocusableElement*)
    {
        if (auto* self = handle.get())
        {
            self->onFilterFocusLost();
        }
    });
}

void FSInventoryCustomTabs::save()
{
    static U64 sSaveCalls = 0;
    ++sSaveCalls;
    LL_DEBUGS("InvCustomTabs") << "save_call=" << sSaveCalls
        << " loading=" << mLoading << " loaded_ok=" << mLoadedOk << LL_ENDL;
    if (!mTabs || mLoading || !mLoadedOk || FSInventoryCustomTabModel::SaveGuard::saving())
    {
        return;
    }

    LLSD array = LLSD::emptyArray();
    std::vector<LLInventoryPanel*> panels;
    const S32 count = mTabs->getTabCount();
    for (S32 i = 0; i < count; ++i)
    {
        auto* inv_panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByIndex(i));
        if (!inv_panel || !mPanels.count(inv_panel))
        {
            continue;
        }
        const auto previous = mEntries.find(inv_panel);
        std::string id = previous == mEntries.end() ? std::string() : previous->second["id"].asString();
        if (id.empty()) id = LLUUID::generateNewID().asString();

        auto& filter = inv_panel->getFilter();
        LLInventoryFilter::Params filter_params;
        filter.toParams(filter_params);
        const bool states_valid = filter_params.validateBlock(false);
        LLSD states_sd;
        if (states_valid)
        {
            LLParamSDParser().writeSD(states_sd, filter_params);
            if (states_sd.isMap() && states_sd.has("substring"))
            {
                states_sd.erase("substring");
            }
            states_sd["filter_ops"]["show_folder_state"] = static_cast<S32>(filter.getShowFolderState());
            states_sd["filter_ops"]["creator_type"] = static_cast<S32>(filter.getFilterCreatorType());
            states_sd["filter_ops"]["links"] = static_cast<S32>(filter.getFilterLinks());
            states_sd["search_type"] = static_cast<S32>(filter.getSearchType());
        }
        else if (!mWarnedInvalidStates)
        {
            mWarnedInvalidStates = true;
            LL_WARNS("InvCustomTabs") << "Invalid filter parameters; saving tab without states" << LL_ENDL;
        }
        array.append(FSInventoryCustomTabModel::makeEntry(id,
            mExplicitlyNamed.count(inv_panel) ? inv_panel->getLabel() : std::string(),
            filter.getFilterSubStringOrig(), states_sd, states_valid, mHidden.count(inv_panel) != 0));
        panels.push_back(inv_panel);
    }

    // The same production transaction is exercised by the headless signal fixture.
    const auto result = FSInventoryCustomTabModel::save(gSavedPerAccountSettings.getLLSD(SETTING_KEY),
        array, mLoadedOk, mLoading, [](const LLSD& value)
        {
            gSavedPerAccountSettings.setLLSD(SETTING_KEY, value);
        });
    if (result.status != FSInventoryCustomTabModel::SaveStatus::WRITTEN) return;

    static U64 sSaveWrites = 0;
    ++sSaveWrites;
    LL_DEBUGS("InvCustomTabs") << "save_write=" << sSaveWrites << " tabs=" << result.tabs.size() << LL_ENDL;
    // Snapshots describe successfully written storage, never a refused proposal.
    for (LLSD::Integer i = 0; i < result.tabs.size(); ++i)
    {
        mEntries[panels[static_cast<size_t>(i)]] = result.tabs[i];
    }
    for (auto* other : allInstances())
    {
        if (other != this) other->onSettingChangedExternally();
    }
}

bool FSInventoryCustomTabs::isUnchangedSetting(const LLSD& array) const
{
    return FSInventoryCustomTabModel::isUnchangedSetting(gSavedPerAccountSettings.getLLSD(SETTING_KEY), array);
}

void FSInventoryCustomTabs::onSettingChangedExternally()
{
    if (FSInventoryCustomTabModel::SaveGuard::saving() || mLoading)
    {
        return;
    }
    mLoading = true;
    doLoad();
    mLoading = false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool FSInventoryCustomTabs::isCustomTab(const LLPanel* panel) const
{
    auto* nonconst = const_cast<LLInventoryPanel*>(dynamic_cast<const LLInventoryPanel*>(panel));
    return nonconst && mPanels.count(nonconst);
}

bool FSInventoryCustomTabs::isAddTab(const LLPanel* panel) const
{
    return panel && panel == static_cast<const LLPanel*>(mAddTabPanel);
}

bool FSInventoryCustomTabs::isShowHiddenTab(const LLPanel* panel) const
{
    return panel && panel == static_cast<const LLPanel*>(mShowHiddenTabPanel);
}

LLButton* FSInventoryCustomTabs::findTabButton(const LLPanel* panel) const
{
    if (!mTabs || !panel)
    {
        return nullptr;
    }
    return mTabs->findChild<LLButton>(TAB_BUTTON_PREFIX + panel->getName());
}

LLInventoryPanel* FSInventoryCustomTabs::hitTestCustomTab(S32 x, S32 y, S32& tab_local_x, S32& tab_local_y) const
{
    if (!mTabs || !mParent || !mTabs->isInVisibleChain() || mParent->isSingleFolderMode())
    {
        return nullptr;
    }
    tab_local_x = 0;
    tab_local_y = 0;
    if (!mParent->localPointToOtherView(x, y, &tab_local_x, &tab_local_y, mTabs))
    {
        return nullptr;
    }
    const S32 idx = mTabs->getTabContainedAtPoint(tab_local_x, tab_local_y);
    if (idx < 0)
    {
        return nullptr;
    }
    auto* inv_panel = dynamic_cast<LLInventoryPanel*>(mTabs->getPanelByIndex(idx));
    if (!inv_panel || !mPanels.count(inv_panel))
    {
        return nullptr;
    }
    return inv_panel;
}

LLPanel* FSInventoryCustomTabs::hitTestAddTab(S32 x, S32 y, S32& tab_local_x, S32& tab_local_y) const
{
    if (!mTabs || !mParent || !mAddTabPanel || !mTabs->isInVisibleChain() || mParent->isSingleFolderMode())
    {
        return nullptr;
    }
    tab_local_x = 0;
    tab_local_y = 0;
    if (!mParent->localPointToOtherView(x, y, &tab_local_x, &tab_local_y, mTabs))
    {
        return nullptr;
    }
    const S32 idx = mTabs->getTabContainedAtPoint(tab_local_x, tab_local_y);
    if (idx < 0)
    {
        return nullptr;
    }
    auto* inv_panel = mTabs->getPanelByIndex(idx);
    if (!isAddTab(inv_panel))
    {
        return nullptr;
    }
    return inv_panel;
}

void FSInventoryCustomTabs::decorateCustomTabButton(LLInventoryPanel* panel)
{
    if (!mTabs || !panel)
    {
        return;
    }
    auto* btn = findTabButton(panel);
    if (!btn)
    {
        return;
    }

    const S32 btn_w = btn->getRect().getWidth();
    const S32 btn_h = btn->getRect().getHeight();
    if (btn_w < CLOSE_BUTTON_RESERVE || btn_h < CLOSE_ICON_SIZE)
    {
        return;
    }

    btn->setRightHPad(CLOSE_BUTTON_RESERVE);

    LLButton::Params close_params;
    close_params.name("fs_inv_tab_close_" + panel->getName());
    close_params.rect(LLRect(btn_w - CLOSE_ICON_MARGIN - CLOSE_ICON_SIZE, (btn_h + CLOSE_ICON_SIZE) / 2, btn_w - CLOSE_ICON_MARGIN, (btn_h - CLOSE_ICON_SIZE) / 2));
    close_params.follows.flags(FOLLOWS_RIGHT);
    close_params.image_unselected.name(CLOSE_ICON_NAME);
    close_params.image_selected.name(CLOSE_ICON_PRESS);
    close_params.image_hover_unselected.name(CLOSE_ICON_PRESS);
    close_params.image_pressed.name(CLOSE_ICON_PRESS);
    close_params.tab_stop(false);
    close_params.label(LLStringUtil::null);


    if (auto* close_btn = LLUICtrlFactory::create<LLButton>(close_params))
    {
        btn->addChild(close_btn);
    }
}

void FSInventoryCustomTabs::applyTabLabel(LLInventoryPanel* panel, const std::string& label)
{
    if (!panel || !mTabs)
    {
        return;
    }
    panel->setLabel(label);
    if (const S32 idx = mTabs->getIndexForPanel(panel); idx >= 0)
    {
        mTabs->setPanelTitle(idx, label);
        mTabs->setTabPadding(idx, CLOSE_BUTTON_RESERVE);
    }
    if (auto* btn = findTabButton(panel))
    {
        btn->setToolTip(label);
    }
    if (mParent && mParent->getActivePanel() == panel)
    {
        mParent->refreshFinderFromFilter();
    }
}

std::string FSInventoryCustomTabs::computeDisplayLabel(LLInventoryPanel* panel) const
{
    if (!panel)
    {
        return getDefaultTabName();
    }
    if (mExplicitlyNamed.count(panel))
    {
        return panel->getLabel();
    }
    const std::string& filter = panel->getFilter().getFilterSubStringOrig();
    return filter.empty() ? getDefaultTabName() : filter;
}

void FSInventoryCustomTabs::updateAutoLabel(LLInventoryPanel* panel)
{
    if (!panel || mExplicitlyNamed.count(panel))
    {
        return;
    }
    const std::string new_label = computeDisplayLabel(panel);
    if (panel->getLabel() == new_label)
    {
        return;
    }
    applyTabLabel(panel, new_label);
}

std::string FSInventoryCustomTabs::getDefaultTabName()
{
    std::string name;
    if (LLTrans::findString(name, DEFAULT_NAME_KEY) && !name.empty())
    {
        return name;
    }
    return FALLBACK_LABEL;
}

std::string FSInventoryCustomTabs::sanitizeTabName(std::string name)
{
    std::string out;
    out.reserve(name.size());

    for (size_t i = 0; i < name.size(); )
    {
        const auto c = static_cast<unsigned char>(name[i]);
        if (c < 0x20 || c == 0x7F)
        {
            ++i;
            continue;
        }
        if (c == 0xC2 && i + 1 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xAD)
        {
            i += 2;
            continue;
        }
        if (c == 0xE2 && i + 2 < name.size())
        {
            const auto c1 = static_cast<unsigned char>(name[i + 1]);
            const auto c2 = static_cast<unsigned char>(name[i + 2]);
            if (c1 == 0x80 && ((c2 >= 0x8B && c2 <= 0x8F) // U+200B ... U+200F
                || (c2 >= 0xA8 && c2 <= 0xA9) // U+2028 ... U+2029
                || (c2 >= 0xAA && c2 <= 0xAE))) // U+202A ... U+202E
            {
                i += 3;
                continue;
            }
            if (c1 == 0x81 && c2 >= 0xA6 && c2 <= 0xA9) // U+2066 ... U+2069
            {
                i += 3;
                continue;
            }
        }
        if (c == 0xEF && i + 2 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xBB && static_cast<unsigned char>(name[i + 2]) == 0xBF)
        {
            i += 3; // U+FEFF (BOM)
            continue;
        }
        out += static_cast<char>(c);
        ++i;
    }

    LLStringUtil::trim(out);
    if (out.size() > static_cast<size_t>(MAX_NAME_LENGTH))
    {
        out = utf8str_truncate(out, static_cast<S32>(MAX_NAME_LENGTH));
    }

    return out;
}
