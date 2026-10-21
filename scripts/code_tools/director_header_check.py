"""Exercise production header lookup, reveal and Quick binding without a viewer build."""
from pathlib import Path
import os
import re
import subprocess

from relight_area_policy_check import method


def main():
    nv = Path("indra/newview")
    row_source = (nv / "alkitrow.cpp").read_text(encoding="utf-8")
    start = row_source.index("class ALKitHeaderBinding final")
    binding = row_source[start:row_source.index("\n};", start) + 3]
    # Check the production checkbox/button contract represented by the tiny stubs.
    ui = Path("indra/llui")
    assert "mButton->setControlName" in method(ui / "llcheckboxctrl.cpp", "void LLCheckBoxCtrl::setControlName(")
    assert "setControlValue(b)" in method(ui / "llbutton.cpp", "void LLButton::setToggleState(")
    navigate = method(nv / "alkitshell.cpp", "bool ALKitShell::navigateTo(const ALKitTarget&")
    assert navigate.index("revealHeader(widget)") < navigate.index("findRowBySetting(target.setting)")
    assert navigate.index("revealHeader(widget)") < navigate.index("rowRevealAllowed(row)")
    assert "!card->headerQuickSafe(widget)" in method(nv / "alkitrow.cpp", "ALKitRow* ALKitRow::createHeaderProxy(")
    card_source = (nv / "alkitcard.cpp").read_text(encoding="utf-8")
    header_callback = card_source.split("auto header_changed =", 1)[1].split("mHeaderConnections", 1)[0]
    assert "shell->refreshChangedMarkers()" in header_callback and "applyLocalFilter" not in header_callback
    assert card_source.count("headerFavoriteAllowed(sw->getName(),") == 2  # menu and click both enforce removal policy
    fixture = r'''
#include <cassert>
#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
using S32 = int;
using U32 = unsigned;
struct LLSD {
    bool value = false, defined = false;
    LLSD() = default;
    LLSD(bool v) : value(v), defined(true) {}
    bool isDefined() const { return defined; }
    bool asBoolean() const { return value; }
};
namespace boost { namespace signals2 { struct connection {}; } }
template<class T> struct LLHandle {
    std::weak_ptr<T> ref;
    T* get() const { return ref.lock().get(); }
};
// Faithful checkbox/button boundary: setValue only updates the widget; toggle
// writes its child button's bound setting, whose signal updates the widget.
// Commit executes the registered callback against that setting-backed model.
struct LLControlVariable {
    bool value = false;
    int writes = 0;
    std::function<void(bool)> changed;
    LLSD getDefault() const { return LLSD(false); }
    LLSD get() const { return LLSD(value); }
    void set(bool v) { if (value != v) { value = v; ++writes; if (changed) changed(v); } }
};
struct Settings {
    std::shared_ptr<LLControlVariable> setting = std::make_shared<LLControlVariable>();
    std::shared_ptr<LLControlVariable> getControl(const std::string& s) {
        return s == "Setting" ? setting : nullptr;
    }
} gSavedSettings;
struct ALKitSwitch : std::enable_shared_from_this<ALKitSwitch> {
    bool value = false, enabled = true, focused = false;
    int commits = 0, writes = 0;
    std::shared_ptr<LLControlVariable> setting;
    std::function<void(bool)> changed;
    std::function<void()> callback;
    template<class T> LLHandle<T> getDerivedHandle() { return {shared_from_this()}; }
    LLSD getValue() const { return LLSD(value); }
    bool isOn() const { return value; }
    bool isInEnabledChain() const { return enabled; }
    void setValue(LLSD v) { bool old = value; value = v.value; if (old != value && changed) changed(value); }
    void bind() {
        setting = gSavedSettings.getControl("Setting");
        std::weak_ptr<ALKitSwitch> handle = shared_from_this();
        setting->changed = [handle](bool v) { if (auto sw = handle.lock()) sw->setValue(LLSD(v)); };
        setValue(LLSD(setting->value));
    }
    void toggle() {
        ++writes;
        if (setting) setting->set(!value);
        else setValue(LLSD(!value));
    }
    void onCommit() { ++commits; if (callback) callback(); }
    void setBtnFocus() { focused = true; }
    boost::signals2::connection setFlipCallback(std::function<void(bool)> cb) { changed = cb; return {}; }
};
struct ALKitBinding {
    virtual ~ALKitBinding() = default;
    virtual LLSD get() const = 0;
    virtual LLSD getDefault() const = 0;
    virtual bool hasDefault() const = 0;
    virtual bool isDefault(S32) const { return get().value == getDefault().value; }
    virtual bool canSet() const = 0;
    virtual void set(const LLSD&) = 0;
    virtual boost::signals2::connection connectChanged(std::function<void()>) = 0;
    virtual std::string describe() const = 0;
};
struct ALKitOption {
    std::string widget, setting, label;
    LLSD value;
    std::string quick = "controller";
    const std::string& enableQuick() const { return quick; }
    const std::string& enableWidget() const { return widget; }
    const std::string& enableSetting() const { return setting; }
    const std::string& optionLabel() const { return label; }
    LLSD optionValue() const { return value; }
};
int selectorWrites = 0;
void alKitSetTypedControl(Settings&, const std::string&, LLSD) { ++selectorWrites; }
template<class T> T llmax(T a, T b) { return a > b ? a : b; }
template<class T> T llmin(T a, T b) { return a < b ? a : b; }
struct LLFontGL { int getWidth(const std::string& s) const { return static_cast<int>(s.size()) * 7; } };
namespace ALKitStyle {
    constexpr S32 CARD_PAD_L=12, CARD_PAD_R=8, COL_GAP=6, SWITCH_W=34, CARD_PRESET_W=120, CARD_HDR_H=36;
    const LLFontGL* sansFont(const std::string&, bool) { static LLFontGL font; return &font; }
}
namespace ALKitQol { bool valuesEqual(LLSD a, LLSD b, S32) { return a.value == b.value; } }
struct ALKitRow {
    struct Meta { std::string widget; } metadata;
    struct Key { std::string page; } key;
    const Meta& meta() const { return metadata; }
    const Key& favoriteKey() const { return key; }
    ALKitRow* sourceRow() const { return nullptr; }
    bool amber() const { return false; }
    void setFilterVisible(bool) {}
    bool bulkResettable() const { return true; }
    bool cardPreset() const { return false; }
    bool whoPicker() const { return false; }
};
struct ALKitCard {
    bool visible = false;
    bool codeVisible() const { return true; }
    std::string mEnableWidget = "master", mEnableSetting = "Setting", mTitle = "Feature", mSelectorSetting = "Selector";
    std::string mEnableQuick = "setting", mBadge;
    struct HeaderLayout {
        S32 height=36, title_width=0, preset_left=0, preset_width=0, badge_width=0;
        bool title_stacked=false;
    };
    HeaderLayout headerLayout(S32) const;
    bool headerTitleFits(S32) const;
    std::vector<ALKitRow*> mRows;
    std::vector<int> mHeaderPresets;
    void* mMenuButton = nullptr;
    ALKitSwitch* mEnable = nullptr;
    void* mOpen = nullptr;
    int mActiveOption = 0, layouts = 0, syncs = 0;
    std::vector<ALKitOption*> mOptions;
    std::vector<ALKitSwitch*> mOptionSwitches;
    std::map<std::string, ALKitSwitch*> children;
    template<class T> T* findChild(const std::string& s) const {
        auto it = children.find(s); return it == children.end() ? nullptr : it->second;
    }
    ALKitSwitch* headerSwitch(const std::string&) const;
    std::string headerLabel(const std::string&) const;
    std::string headerSetting(const std::string&) const;
    bool revealHeader(const std::string&);
    bool headerQuickSafe(const std::string&) const;
    bool headerFavoriteAllowed(const std::string&, bool) const;
    bool headerChanged(const std::string& widget = std::string()) const;
    ALKitSwitch* activeHeaderSwitch() const;
    void requestLayout() { ++layouts; }
    void syncSelector() { ++syncs; }
    std::string id() const { return "feature"; }
};
struct ALKitPage {
    std::string key = "light_group/light_rim_tab";
    std::string pageKey() const { return key; }
    std::map<std::string, ALKitRow*> mRows;
    bool mSearching = false;
    bool changed() const { return false; }
    ALKitRow* findRow(const std::string&) { return nullptr; }
    ALKitRow* findRowBySetting(const std::string&) { return nullptr; }
    ALKitCard* findCard(const std::string&) { return nullptr; }
    std::map<std::string, ALKitCard*> mCards;
    std::map<std::string, bool> mHeaderHits;
    ALKitSwitch* scrolled = nullptr;
    int reveals = 0;
    ALKitCard* findHeaderCard(const std::string&) const;
    bool revealHeader(const std::string&);
    bool revealCard(ALKitCard*, bool) { ++reveals; return true; }
    void scrollToView(ALKitSwitch* s) { scrolled = s; }
    void applyModeAndChip() {
        for (auto& c : mCards) c.second->visible = !mSearching || mHeaderHits.count(c.first);
    }
    void applySearch(const ALKitSearch::Hits&);
    void clearSearch() { mSearching = false; applyModeAndChip(); }
    void applyLocalFilter(const std::string&, bool) { assert(false && "indexed refresh required"); }
};
'''
    # Compile the actual index tokenizer/query/scoring and shell refresh/search
    # with the same minimal widget/model boundary as the binding fixture.
    search_header = (nv / "alkitsearch.h").read_text(encoding="utf-8")
    search_header = re.sub(r'^#include.*$', '', search_header, flags=re.M).replace("private:", "public:")
    qol_header = (nv / "aldirectorqol.h").read_text(encoding="utf-8")
    qol_api = qol_header[qol_header.index("S32 fuzzyTokenScore"):qol_header.index("std::vector<LLUUID> subjectAssignment")]
    qol_source = (nv / "aldirectorqol.cpp").read_text(encoding="utf-8")
    support = r'''
struct LLTimer { double getElapsedTimeF64() const { return 0; } };
struct LLStringUtil { static void toLower(std::string& s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); } };
struct ALUIPerfSelfTest { static bool isActive() { return false; } };
#define LL_INFOS(x) std::cout
#define LL_ENDL std::endl
'''
    support += "\nnamespace ALDirectorQol {\n" + qol_api + "\n}\n"
    support += qol_source[qol_source.index("namespace\n{"):qol_source.index("std::vector<LLUUID> ALDirectorQol::subjectAssignment")]
    support += search_header
    for signature in ("ALKitSearch::Hits ALKitSearch::query(", "ALKitSearch::Hits ALKitSearch::all(",
                      "U32 ALKitSearch::intern(", "void ALKitSearch::tokenize(", "void ALKitSearch::splitTokens(",
                      "void ALKitSearch::setCurrentPage(", "void ALKitSearch::setQuery(", "const std::string& ALKitSearch::queryText("):
        support += "\n" + method(nv / "alkitsearch.cpp", signature)
    fixture = fixture.replace("struct ALKitPage {", support + "\nstruct ALKitPage {")
    fixture += r'''
struct ALKitShell {
    ALKitSearch mSearch;
    bool mChangedOnly = true, mHasTopSearchHit = false;
    struct Popup { void hide() {} } *mSearchPopover = nullptr;
    ALKitSearch::Hits mSearchHits;
    struct ChangedEntry { std::string setting; bool changed = false; S32 decimals = 0; };
    std::map<std::string, std::vector<ChangedEntry>> mPageSettings;
    std::map<std::string, bool> mPageChanged;
    std::map<std::string, int> mPageChangedCounts;
    std::map<std::string, std::shared_ptr<ALKitPage>> mBuiltPages;
    ALKitPage* currentKitPage() { return mBuiltPages.begin()->second.get(); }
    ALKitPage* findBuiltPage(const std::string& k) { auto it=mBuiltPages.find(k); return it==mBuiltPages.end() ? nullptr : it->second.get(); }
    void refreshPageMarker(const std::string&) {}
    void updatePips() {}
    void updateSearchResults(const ALKitSearch::Hits&, ALKitPage*) {}
    void runSearch(const std::string&);
    void refreshChangedMarkers(ALKitRow* = nullptr);
};
'''
    fixture += binding
    for cls, names, path in (
        ("ALKitCard", ("ALKitSwitch* ALKitCard::headerSwitch(", "std::string ALKitCard::headerLabel(",
                       "std::string ALKitCard::headerSetting(", "bool ALKitCard::headerQuickSafe(", "bool ALKitCard::headerFavoriteAllowed(", "bool ALKitCard::headerChanged(",
                       "ALKitSwitch* ALKitCard::activeHeaderSwitch(", "ALKitCard::HeaderLayout ALKitCard::headerLayout(",
                       "bool ALKitCard::headerTitleFits(", "bool ALKitCard::revealHeader("), "alkitcard.cpp"),
        ("ALKitPage", ("ALKitCard* ALKitPage::findHeaderCard(", "bool ALKitPage::revealHeader("), "alkitpage.cpp")):
        for signature in names:
            fixture += "\n" + method(nv / path, signature)
    fixture += "\n" + method(nv / "alkitpage.cpp", "void ALKitPage::applySearch(")
    fixture += "\n" + method(nv / "alkitshell.cpp", "void ALKitShell::runSearch(")
    fixture += "\n" + method(nv / "alkitshell.cpp", "void ALKitShell::refreshChangedMarkers(")
    # Feed the real generated header's searchable fields into production tokens.
    index = (nv / "skins/default/xui/en/director_kit_index.xml").read_text(encoding="utf-8")
    fields = next(line.split("\t") for line in index.splitlines() if "\trelight_shape_enable\t" in line)
    import json
    fixture += "\nvoid seedSearch(ALKitSearch& search) { ALKitSearch::Entry e;\n"
    for name, value in (("page_key", fields[0] + "/" + fields[1]), ("widget", fields[8]),
                        ("label", fields[9]), ("label_lower", fields[9].lower()), ("setting", fields[11])):
        fixture += f"e.{name} = {json.dumps(value)};\n"
    for name, column in (("label", 9), ("card", 6), ("keyword", 14), ("help", 15),
                         ("setting", 11), ("widget", 8), ("synonym", 16)):
        fixture += f"search.tokenize({json.dumps(fields[column])}, e.{name}_tokens);\n"
    fixture += "search.mEntries.push_back(e); }\n"
    fixture += r'''
int main() {
    auto master = std::make_shared<ALKitSwitch>();
    auto one = std::make_shared<ALKitSwitch>();
    auto two = std::make_shared<ALKitSwitch>();
    ALKitOption opt1{"one", "Setting", "One", LLSD(false)}, opt2{"two", "Setting", "Two", LLSD(true)};
    ALKitCard card;
    card.children = {{"master", master.get()}};
    card.mOptions = {&opt1, &opt2}; card.mOptionSwitches = {one.get(), two.get()};
    ALKitRow resettable;
    card.mRows = {&resettable};
    card.mMenuButton = card.mOpen = &card;
    card.mEnable = master.get();
    card.mBadge = "Advanced";
    card.mTitle = "Cinematic lightning";
    assert(card.headerTitleFits(340) && card.headerLayout(340).title_stacked);
    card.mHeaderPresets = {1, 2};
    const auto stacked = card.headerLayout(340);
    assert(card.headerTitleFits(340) && stacked.title_stacked && stacked.height == 88);
    assert(stacked.preset_width >= 96);
    card.mTitle = "Feature";
    assert(card.headerTitleFits(340) && !card.headerLayout(340).title_stacked);
    ALKitPage page; page.mCards = {{"feature", &card}};
    assert(!page.findHeaderCard("body") && !page.revealHeader("phantom"));
    assert(page.revealHeader("master") && master->focused && page.scrolled == master.get());
    assert(selectorWrites == 0 && card.layouts == 1);
    assert(page.revealHeader("two") && card.mActiveOption == 1 && two->focused);
    assert(selectorWrites == 1 && card.syncs == 1 && page.scrolled == two.get());
    assert(card.headerLabel("two") == "Feature / Two" && card.headerSetting("master") == "Setting");
    assert(card.headerQuickSafe("master") && !card.headerQuickSafe("one") && !card.headerQuickSafe("two"));
    assert(card.headerFavoriteAllowed("master", false));
    for (const std::string policy : {"controller", "tool", "code", "reload", "unknown"}) {
        card.mEnableQuick = policy;
        assert(!card.headerQuickSafe("master")); // proxy creation uses the same guard
        assert(!card.headerFavoriteAllowed("master", false));
        assert(card.headerFavoriteAllowed("master", true)); // legacy removal
    }
    assert(!card.headerFavoriteAllowed("two", false) && card.headerFavoriteAllowed("two", true));
    card.mEnableQuick = "setting";
    master->bind();
    bool renderedModel = false;
    int callbackEffects = 0;
    master->callback = [&]() {
        // This is an actual model update, not a replacement commit counter.
        renderedModel = gSavedSettings.setting->value;
        ++callbackEffects;
    };
    assert(!card.headerChanged());
    assert(card.activeHeaderSwitch() == master.get());
    card.mEnable = nullptr;
    card.mActiveOption = 0;
    assert(card.activeHeaderSwitch() == one.get());
    card.mEnable = master.get();
    ALKitHeaderBinding proxy(master.get(), "Setting");
    int notifications = 0;
    proxy.connectChanged([&]() { ++notifications; });
    assert(proxy.hasDefault() && proxy.isDefault(0));
    proxy.set(LLSD(true));
    assert(master->value && master->commits == 1 && master->writes == 1 && notifications == 1);
    assert(proxy.get().value && !proxy.isDefault(0));
    assert(renderedModel && callbackEffects == 1 && gSavedSettings.setting->writes == 1);
    assert(card.headerChanged()); // no changed body rows: header state alone retains the card
    // A rebuilt binding still targets the original source and dispatches its callback.
    ALKitHeaderBinding rebuilt(master.get(), "Setting");
    assert(rebuilt.get().value);
    gSavedSettings.setting->set(false); // external setting update reaches Quick
    assert(!proxy.get().value && notifications == 2 && master->writes == 1);
    master->enabled = false;
    proxy.set(LLSD(true));
    assert(!master->value && master->commits == 1 && !proxy.canSet());
    master->enabled = true;
    rebuilt.set(LLSD(true));
    assert(renderedModel && callbackEffects == 2 && gSavedSettings.setting->writes == 3);
    assert(master->value && master->commits == 2 && master->writes == 2);
    // Code/controller switches remain discoverable, but no Quick binding is offered.
    assert(!card.headerQuickSafe("two"));
    gSavedSettings.setting->set(false);
    two->setValue(LLSD(true));
    opt2.setting.clear(); // a code-bound active header has no saved default
    assert(card.headerChanged("two") && !card.headerChanged("master"));
    assert(!card.headerChanged("one"));
    ALKitShell shell;
    seedSearch(shell.mSearch);
    auto filtered = std::make_shared<ALKitPage>();
    ALKitCard shape;
    auto shape_switch = std::make_shared<ALKitSwitch>();
    shape.mEnableWidget = "relight_shape_enable";
    shape.children = {{shape.mEnableWidget, shape_switch.get()}};
    filtered->mCards = {{shape.id(), &shape}};
    shell.mBuiltPages = {{filtered->pageKey(), filtered}};
    shape_switch->bind();
    shape_switch->setFlipCallback([&](bool) { shell.refreshChangedMarkers(); });
    shape_switch->toggle(); // enable
    shell.runSearch("relight");
    assert(shape.headerChanged() && shape.visible);
    shape_switch->toggle(); // off: no changed rows or headers
    assert(!shape.headerChanged() && !shape.visible);
    gSavedSettings.setting->set(true); // undo's setting restore, via the actual flip/refresh path
    assert(shape.headerChanged() && shape.visible);
    std::cout << "Director indexed Changed-only PASS: production query/scoring, real header index, enable/relight/off/undo restore\n";
    master.reset(); proxy.set(LLSD(false));
    assert(!proxy.canSet() && !proxy.get().isDefined());
    std::cout << "Director header PASS: real lookup, option reveal, focus/scroll, one commit/write, "
                 "live updates, disabled/re-enabled, defaults, callback model effects, setting/button boundary, safety policy, header-only changed state, option favorite fallback, rebuild, minimum-width title/preset fit, header-first navigation, expired source\n";
}
'''
    directory = Path(".tmp/director_header_check").resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "headers.cpp").write_text(fixture, encoding="utf-8")
    vcvars = next((Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio").glob(
        "*/Community/VC/Auxiliary/Build/vcvars64.bat"))
    (directory / "compile.cmd").write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++17 /EHsc /Fo"{directory / "headers.obj"}" '
        f'/Fe"{directory / "headers.exe"}" "{directory / "headers.cpp"}\nexit /b %ERRORLEVEL%\n', encoding="utf-8")
    subprocess.run(["cmd", "/d", "/c", str(directory / "compile.cmd")], check=True)
    subprocess.run([str(directory / "headers.exe")], check=True)


if __name__ == "__main__":
    main()
