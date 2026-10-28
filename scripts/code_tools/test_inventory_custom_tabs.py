#!/usr/bin/env python3
"""Headless integration contracts for the custom inventory tabs port.

The LLSD/reconciliation behavior is exercised by the C++ TUT suite. These checks
cover the real UI wiring that cannot be instantiated without the viewer runtime.
"""
from pathlib import Path
import re
import subprocess
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"


def body(source: str, name: str) -> str:
    """Extract a function whose fully qualified signature is unique."""
    start = source.index(name)
    start = source.index("{", start)
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def setting(file: str, name: str) -> dict[str, ET.Element]:
    root_map = ET.parse(VIEW / "app_settings" / file).getroot().find("map")
    values = list(root_map)
    value = next(values[i + 1] for i, key in enumerate(values[:-1])
                 if key.tag == "key" and key.text == name)
    pairs = list(value)
    return {pairs[i].text: pairs[i + 1] for i in range(0, len(pairs), 2)}


class InventoryCustomTabsContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tabs = (VIEW / "fsinventorycustomtabs.cpp").read_text(encoding="utf-8")
        cls.main = (VIEW / "llpanelmaininventory.cpp").read_text(encoding="utf-8")
        cls.model = (VIEW / "fsinventorycustomtabmodel.cpp").read_text(encoding="utf-8")
        cls.base_main = subprocess.run(
            ["git", "show", "949c91fa933:indra/newview/llpanelmaininventory.cpp"], cwd=ROOT,
            capture_output=True, text=True, check=True, timeout=30).stdout

    def test_settings_and_preference(self):
        master = setting("settings_alchemy.xml", "AlchemyInventoryCustomTabs")
        self.assertEqual(master["Type"].text, "Boolean")
        self.assertEqual(master["Value"].text, "1")
        self.assertEqual(master["Persist"].text, "1")
        saved = setting("settings_per_account.xml", "FSInventoryCustomTabs")
        self.assertEqual(saved["Type"].text, "LLSD")
        self.assertEqual(saved["HideFromEditor"].text, "1")
        self.assertEqual(saved["Value"].tag, "array")
        panel = ET.parse(XUI / "panel_preferences_interface.xml").getroot()
        check = panel.find("check_box[@control_name='AlchemyInventoryCustomTabs']")
        self.assertIsNotNone(check)
        self.assertTrue(check.get("tool_tip"))
        children = list(panel)
        self.assertLess(children.index(panel.find("text[@name='InventoryText']")), children.index(check))
        self.assertLess(children.index(check), children.index(panel.find("text[@name='Appearance Editor']")))

    def test_off_returns_before_any_controller_or_panels_exist(self):
        install = body(self.tabs, "void FSInventoryCustomTabs::install(")
        guard = install.index('!gSavedSettings.getBOOL("AlchemyInventoryCustomTabs")')
        creation = install.index("std::make_unique<FSInventoryCustomTabs>")
        self.assertLess(guard, install.index("return;", guard))
        self.assertLess(install.index("return;", guard), creation)
        toggle = body(self.main, "void LLPanelMainInventory::updateCustomTabsEnabled()")
        self.assertLess(toggle.index("mCustomTabs->disable()"), toggle.index("mCustomTabs.reset()"))
        self.assertNotIn("setLLSD", toggle)

    def test_partial_load_cannot_write(self):
        save = body(self.tabs, "void FSInventoryCustomTabs::save()")
        self.assertLess(save.index("!mLoadedOk"), save.index("LLSD array"))
        self.assertIn("array, mLoadedOk, mLoading", save)
        transaction = body(self.model, "SaveResult save(")
        self.assertLess(transaction.index("!loaded_ok"), transaction.index("write(result.tabs)"))
        self.assertLess(transaction.index("!normalized.loadedOk"), transaction.index("write(result.tabs)"))
        self.assertLess(save.index("SaveStatus::WRITTEN"), save.index("mEntries[panels["))

    def test_multi_window_signal_does_not_rebuild(self):
        signal = body(self.tabs, "void FSInventoryCustomTabs::onSettingChangedExternally()")
        self.assertLess(signal.index("SaveGuard::saving()"), signal.index("doLoad()"))
        self.assertNotIn("clearCustomTabs", signal)
        load = body(self.tabs, "void FSInventoryCustomTabs::doLoad()")
        self.assertIn("FSInventoryCustomTabModel::reconcile", load)
        self.assertNotIn("clearCustomTabs", load)
        self.assertIn("applyEntry(panel, entry)", load)
        self.assertIn("if (refresh_active) mParent->onFilterSelected();", load)

    def test_stock_finder_draw_identity(self):
        finder = body(self.main, "void LLFloaterInventoryFinder::draw()")
        # Remove only the three authorized instrumentation lines; all stock
        # algebra/order must remain byte-for-byte identical to the base commit.
        stripped = "\n".join(line for line in finder.split("\n") if not any(key in line for key in (
            "LL_PROFILE_ZONE_SCOPED_CATEGORY_UI", "captureFilterGeneration", "notifyIfFilterChanged")))
        self.assertEqual(stripped, body(self.base_main, "void LLFloaterInventoryFinder::draw()"))
        self.assertEqual(body(self.main, "bool LLFloaterInventoryFinder::postBuild()"),
                         body(self.base_main, "bool LLFloaterInventoryFinder::postBuild()"))
        notify = body(self.tabs, "void FSInventoryCustomTabs::notifyIfFilterChanged(")
        self.assertIn("getCurrentGeneration() != prev_generation", notify)

    def test_no_unconditional_draw_saves(self):
        for function in ("void FSInventoryCustomTabs::onParentDraw(",
                         "void FSInventoryCustomTabs::ensureRootOpenForActivePanel()"):
            self.assertNotIn("save(", body(self.tabs, function))
        main_draw = body(self.main, "void LLPanelMainInventory::draw()")
        self.assertNotRegex(main_draw, r"\bsave\s*\(")
        self.assertNotIn("notifyFilterStateChanged", main_draw)
        self.assertNotIn("notifyActiveFilterStateChanged", main_draw)

    def test_stock_off_editor_inbox_and_ellipses(self):
        draw = body(self.main, "void LLPanelMainInventory::draw()")
        stock_write = body(draw, "else")
        self.assertEqual(stock_write, "{\n            mFilterEditor->setText(mFilterSubString);\n        }")
        self.assertIn("mFilterEditor->setText(mFilterSubString);", body(self.base_main, "void LLPanelMainInventory::draw()"))
        selected = body(self.main, "void LLPanelMainInventory::onFilterSelected()")
        enabled = body(selected, "if (mCustomTabs && (was_custom")
        outside = selected.replace(enabled, "")
        self.assertNotIn("mInboxPanel->onFilterEdit", outside)
        self.assertNotIn("setTextIfChanged", outside)
        self.assertIn("!mCustomTabs->isLoading()", enabled)
        panel = ET.parse(XUI / "panel_main_inventory.xml").getroot()
        tabs = panel.find(".//tab_container[@name='inventory filter tabs']")
        self.assertNotIn("use_ellipses", tabs.attrib)
        constructor = body(self.tabs, "FSInventoryCustomTabs::FSInventoryCustomTabs(")
        destructor = body(self.tabs, "FSInventoryCustomTabs::~FSInventoryCustomTabs()")
        self.assertIn("mOriginalTabEllipses = mTabs->getUseTabEllipses()", constructor)
        self.assertIn("setUseTabEllipses(true)", constructor)
        self.assertIn("setUseTabEllipses(mOriginalTabEllipses)", destructor)

    def test_hidden_container_does_not_steal_item_view_input(self):
        for signature in ("LLInventoryPanel* FSInventoryCustomTabs::hitTestCustomTab(",
                          "LLPanel* FSInventoryCustomTabs::hitTestAddTab("):
            hit = body(self.tabs, signature)
            guard = hit[:hit.index("tab_local_x = 0")]
            self.assertIn("!mTabs->isInVisibleChain()", guard)
            self.assertIn("mParent->isSingleFolderMode()", guard)
            self.assertIn("return nullptr;", guard)
            self.assertLess(hit.index("return nullptr;"), hit.index("getTabContainedAtPoint"))

    def test_metadata_changes_do_not_refilter(self):
        apply = body(self.tabs, "void FSInventoryCustomTabs::applyEntry(")
        branch = body(apply, "if (filter_changed)")
        self.assertIn("FSInventoryCustomTabModel::filterStateChanged", apply)
        self.assertIn("filter.resetDefault()", branch)
        self.assertIn("filter.fromParams(params)", branch)
        outside = apply.replace(branch, "")
        self.assertNotIn("resetDefault", outside)
        self.assertNotIn("fromParams", outside)
        self.assertIn("name_changed", outside)
        self.assertIn('previous["tabhidden"]', outside)

    def test_bad_states_do_not_abort_save_and_snapshots_follow_write(self):
        save = body(self.tabs, "void FSInventoryCustomTabs::save()")
        self.assertIn("const bool states_valid = filter_params.validateBlock(false);", save)
        self.assertIn("else if (!mWarnedInvalidStates)", save)
        self.assertNotIn("return", body(save, "else if (!mWarnedInvalidStates)"))
        self.assertEqual(save[:save.index("FSInventoryCustomTabModel::save(")].count("return;"), 1)
        self.assertNotRegex(save, r"if\s*\(\s*!filter_params.validateBlock.*return")
        self.assertIn("states_sd, states_valid", save)
        self.assertNotRegex(save[:save.index("FSInventoryCustomTabModel::save(")], r"mEntries\[.*?\]\s*=")

    def test_deferred_close_and_single_teardown_selection(self):
        for signature in ("void FSInventoryCustomTabs::clearCustomTabs()", "void FSInventoryCustomTabs::doLoad()",
                          "void FSInventoryCustomTabs::onCloseConfirmed("):
            section = body(self.tabs, signature)
            self.assertNotRegex(section, r"delete (?:panel|to_remove)")
            self.assertIn("->die()", section)
        decoration = body(self.tabs, "void FSInventoryCustomTabs::decorateCustomTabButton(")
        self.assertNotIn("click_callback", decoration)
        close = body(self.tabs, "void FSInventoryCustomTabs::onCloseClicked()")
        self.assertIn("auto pending_notification = LLNotificationsUtil::add", close)
        self.assertIn("!pending_notification->isRespondedTo()", close)
        detach = body(self.tabs, "void FSInventoryCustomTabs::detachTabPanel(")
        self.assertIn("removeTabPanel(panel, false)", detach)
        clear = body(self.tabs, "void FSInventoryCustomTabs::clearCustomTabs()")
        self.assertEqual(clear.count("selectFirstTab()"), 1)
        self.assertLess(clear.index("selectFirstTab()"), clear.index("for (auto* panel"))

    def test_sync_keeps_unrelated_dialogs_and_typing(self):
        signal = body(self.tabs, "void FSInventoryCustomTabs::onSettingChangedExternally()")
        self.assertNotIn("cancel(", signal)
        load = body(self.tabs, "void FSInventoryCustomTabs::doLoad()")
        self.assertLess(load.index("if (retained.count(panel)) continue;"), load.index("cancelNotificationsForPanel(panel)"))
        cancel = body(self.tabs, "void FSInventoryCustomTabs::cancelNotificationsForPanel(")
        self.assertEqual(cancel.count('getPayload()["panel_name"].asString() == panel->getName()'), 2)
        selected = body(self.main, "void LLPanelMainInventory::onFilterSelected()")
        self.assertIn("!mCustomTabs->isLoading()", selected)
        title = body(self.main, "void LLPanelMainInventory::refreshFinderFromFilter()")
        self.assertNotIn("changeFilter", title)
        self.assertNotIn("mFilterEditor", title)

    def test_partial_load_add_notice_padding_and_counters(self):
        load = body(self.tabs, "void FSInventoryCustomTabs::doLoad()")
        self.assertIn("enableTabButton(mTabs->getIndexForPanel(mAddTabPanel), mLoadedOk)", load)
        self.assertIn("notifyIncompleteLoad();", load)
        add = body(self.tabs, "LLInventoryPanel* FSInventoryCustomTabs::addTab(")
        self.assertIn("!mLoading && !mLoadedOk", add)
        self.assertIn("notifyIncompleteLoad();", add)
        label = body(self.tabs, "void FSInventoryCustomTabs::applyTabLabel(")
        self.assertLess(label.index("setPanelTitle"), label.index("setTabPadding(idx, CLOSE_BUTTON_RESERVE)"))
        insert = body(self.tabs, "void FSInventoryCustomTabs::insertTabPanel(")
        self.assertIn("else mTabs->setCurrentPanelIndex(-1)", insert)
        for counter in ("sSaveCalls", "sSaveWrites", "sPanelBuilds"):
            self.assertIn(f"++{counter};", self.tabs)
            self.assertNotRegex(self.tabs, rf'LL_DEBUGS\("InvCustomTabs"\)[^;]*\+\+{counter}')

    def assert_survivor_contracts(self, tabs, main, model):
        load = body(tabs, "void FSInventoryCustomTabs::doLoad()")
        self.assertIn("bool complete = loaded.loadedOk;", load)
        self.assertIn("mLoadedOk = complete;", load)
        self.assertNotRegex(load, r"mLoadedOk\s*=\s*true")
        save = body(tabs, "void FSInventoryCustomTabs::save()")
        self.assertNotIn("clearCustomTabs", save)
        self.assertIn("other->onSettingChangedExternally()", save)
        self.assertIn("FSInventoryCustomTabModel::save(", save)
        transaction = body(model, "SaveResult save(")
        self.assertIn("SaveGuard guard;", transaction)
        self.assertLess(transaction.index("SaveGuard guard;"), transaction.index("write(result.tabs)"))
        self.assertNotRegex(body(main, "void LLPanelMainInventory::draw()"), r"\bsave\s*\(")

    def test_surviving_mutants_are_rejected(self):
        self.assert_survivor_contracts(self.tabs, self.main, self.model)
        variants = (
            (self.tabs.replace("mLoadedOk = complete;", "mLoadedOk = true;"), self.main, self.model),
            (self.tabs, self.main, self.model.replace("SaveGuard guard;", "/* removed guard */")),
            (self.tabs.replace("if (other != this) other->onSettingChangedExternally();",
                               "if (other != this) other->clearCustomTabs();"), self.main, self.model),
            (self.tabs, self.main.replace("void LLPanelMainInventory::draw()\n{",
                                        "void LLPanelMainInventory::draw()\n{\n    mCustomTabs->save();"), self.model),
        )
        for index, sources in enumerate(variants):
            with self.subTest(mutant=index + 1):
                self.assertNotEqual(sources, (self.tabs, self.main, self.model))
                with self.assertRaises(AssertionError):
                    self.assert_survivor_contracts(*sources)

    def test_lazy_real_tabs_and_observer_free_actions(self):
        add = body(self.tabs, "LLInventoryPanel* FSInventoryCustomTabs::addTab(")
        self.assertLess(add.index("params.preinitialize_views(false)"),
                        add.index("create<LLInventoryPanel>"))
        for action in ("installAddTab", "installShowHiddenTab"):
            action_body = body(self.tabs, f"void FSInventoryCustomTabs::{action}()")
            self.assertIn("create<LLPanel>", action_body)
            self.assertNotIn("LLInventoryPanel::Params", action_body)

    def test_custom_panel_matches_stock_view_parameters(self):
        add = body(self.tabs, "LLInventoryPanel* FSInventoryCustomTabs::addTab(")
        for parameter in ("params.background_visible(true)", "params.has_border(false)",
                          "params.border.bevel_style(LLViewBorder::BEVEL_NONE)",
                          "params.scroll.reserve_scroll_corner(false)", "params.folder.double_click_override(true)"):
            self.assertIn(parameter, add)
        self.assertIn('params.bg_opaque_color(LLUIColorTable::instance().getColor("DkGray2"))', add)
        self.assertIn('params.bg_alpha_color(LLUIColorTable::instance().getColor("DkGray2"))', add)

    def test_menu_callbacks_work_on_runtime_enable(self):
        constructor = body(self.tabs, "FSInventoryCustomTabs::FSInventoryCustomTabs(")
        self.assertLess(constructor.index("registrar.pushScope()"), constructor.index("createFromFile<LLMenuGL>"))
        self.assertLess(constructor.index("createFromFile<LLMenuGL>"), constructor.index("registrar.popScope()"))
        menu = ET.parse(XUI / "menu_inventory_custom_tab.xml").getroot()
        functions = {node.get("function") for node in menu.iter("on_click")}
        self.assertEqual(functions, {f"FSInventoryCustomTab.{action}" for action in ("Rename", "Clone", "Hide", "Close")})

    def test_substring_restored_before_only_links(self):
        apply = body(self.tabs, "void FSInventoryCustomTabs::applyEntry(")
        self.assertLess(apply.index("filter.fromParams(params)"), apply.index("panel->setFilterSubString"))
        self.assertLess(apply.index("panel->setFilterSubString"), apply.index("filter.setFilterLinks"))
        selected = body(self.main, "void LLPanelMainInventory::onFilterSelected()")
        self.assertIn("if (!FSInventoryCustomTabs::activePanelIsCustom(this)) setFilterSubString", selected)

    def test_drag_guard_and_textures(self):
        drag = body(self.main, "bool LLPanelMainInventory::handleDragAndDrop(")
        self.assertLess(drag.index("mCustomTabs->handleDragAndDrop"), drag.index("LLPanel::handleDragAndDrop"))
        self.assertRegex(drag, r"panel && panel->getScrollableContainer\(\)")
        textures = ET.parse(VIEW / "skins/default/textures/textures.xml").getroot()
        names = {node.get("name") for node in textures}
        self.assertTrue({"Icon_Close_Foreground", "Icon_Close_Press", "Arrow_Down"} <= names)

    def test_no_dropped_firestorm_features(self):
        self.assertNotRegex(self.tabs + self.main, re.compile(r"FSSplitInventorySearchOverTabs|fscommon\.h|onFilterTypeSelected|onSearchTypeChecked"))


if __name__ == "__main__":
    unittest.main()
