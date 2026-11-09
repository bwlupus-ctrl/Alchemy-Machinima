"""Extract the real kit visibility/chrome methods for the Release rig harness."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from rig_shadow_presets_check import function
from rig_group_history_check import history_fixture


SUPPORT = r'''
#include <cstdio>
#include <cctype>
#include <sstream>
#include <set>
#include <unordered_set>
namespace RigSessionUI {
namespace LLStringUtil {
inline void toLower(std::string& text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}
inline std::vector<std::string> getTokens(const std::string& text, const char*) {
    std::istringstream input(text); std::vector<std::string> tokens; std::string token;
    while (input >> token) tokens.push_back(token);
    return tokens;
}
}
struct LLView {
    std::string name;
    LLView* parent = nullptr;
    std::vector<LLView*> children;
    bool visible = true;
    virtual ~LLView() { for (auto* child : children) delete child; }
    void addChild(LLView* child) {
        if (child->parent) {
            auto& old = child->parent->children;
            old.erase(std::remove(old.begin(), old.end(), child), old.end());
        }
        children.push_back(child); child->parent = this;
    }
    template<class T> T* findChild(const std::string& key) {
        for (auto* child : children) {
            if (child->name == key) return dynamic_cast<T*>(child);
            if (auto* found = child->findChild<T>(key)) return found;
        }
        return nullptr;
    }
    LLView* getParent() const { return parent; }
    void setVisible(bool v) { visible = v; }
    virtual bool isInVisibleChain() const { return visible && (!parent || parent->isInVisibleChain()); }
};
struct LLUICtrl : LLView {};
struct LLCheckBoxCtrl : LLUICtrl {
    bool value = false;
    void setValue(bool v) { value = v; }
};
struct LLSearchEditor : LLUICtrl {
    LLSD value;
    const LLSD& getValue() const { return value; }
    void setValue(const std::string& v) { value = v.c_str(); }
};
struct LLTextBox : LLUICtrl { std::string text; };
struct LLButton : LLUICtrl {
    std::string label;
    std::function<void(LLUICtrl*, const LLSD&)> callback;
    void setCommitCallback(std::function<void(LLUICtrl*, const LLSD&)> cb) { callback = std::move(cb); }
    void setToggleState(bool) {}
    void setImageColor(int) {}
    void setSelectedImageColor(int) {}
    void setUnselectedLabelColor(int) {}
    void setSelectedLabelColor(int) {}
    const std::string& getLabelUnselected() const { return label; }
    void setLabel(const std::string& value) { label = value; }
};
struct LLPanel : LLView {
    struct Rect { int getWidth() const { return 600; } int getHeight() const { return 800; } };
    Rect getRect() const { return {}; }
};
struct ALKitShell : LLPanel {
    bool changed = false;
    static ALKitShell* find(LLView* view) {
        for (; view; view = view->getParent())
            if (auto* shell = dynamic_cast<ALKitShell*>(view)) return shell;
        return nullptr;
    }
    bool changedOnly() const { return changed; }
    void setChangedOnly(bool enabled);
    void clearSearch();
};
struct ALKitSwitch { bool on = false; bool isOn() const { return on; } };
struct ALKitDoc { void setDimmed(bool) {} };
struct ALKitRow : LLPanel {
    struct Meta { std::string widget, label, help, keywords; } metadata;
    bool filtered = true;
    const Meta& meta() const { return metadata; }
    bool amber() const { return false; } // Unchanged management/readout rows.
    bool filterVisible() const { return filtered; }
    void setFilterVisible(bool v) { filtered = v; }
    // Stand in for row layout's filter gate while retaining parent visibility.
    bool isInVisibleChain() const override { return filtered && LLPanel::isInVisibleChain(); }
};
struct ALKitCard : LLPanel {
    std::vector<ALKitRow*> content;
    const std::vector<ALKitRow*>& rows() const { return content; }
    const std::string& id() const { return name; }
    bool codeVisible() const { return true; }
    std::string title() const { return name; }
    std::string helpText() const { return {}; }
    bool headerChanged() const { return false; }
};
namespace ALKitStyle {
enum { WHO_H=34, PAGE_PAD_LR=12, ACTIVE_FILL, CONTROL_BG, ACCENT, LABEL };
inline int color(int c) { return c; }
inline void stylePill(LLButton*) {}
}
inline LLPanel* makePanel(const std::string& name, int, int) { auto* p = new LLPanel; p->name = name; return p; }
inline LLButton* makeButton(const std::string& name, const std::string& label) {
    auto* b = new LLButton; b->name = name; b->label = label; return b;
}
inline void setTextIfChanged(LLTextBox* p, const std::string& v) { if (p) p->text = v; }
inline void setVisibleIfChanged(LLView* p, bool v) { if (p) p->setVisible(v); }
inline void setLabelIfChanged(LLButton* p, const std::string& v) { if (p) p->label = v; }
inline void placeSectionChips(const std::vector<LLButton*>&, int) {}
template<class... A> std::string llformat(const char* fmt, A... args) {
    char out[512]; std::snprintf(out, sizeof(out), fmt, args...); return out;
}
class ALKitPage : public LLPanel {
public:
    struct ModeData { std::string name; std::vector<std::string> cards; };
    struct SectionData { std::string id, label; std::vector<std::string> cards; };
    std::vector<ModeData> mModeDefs;
    std::vector<SectionData> mSections;
    std::vector<LLPanel*> mSectionHeadings;
    std::vector<LLButton*> mChips;
    std::map<std::string, ALKitCard*> mCards;
    std::map<std::string, ALKitRow*> mRows;
    std::map<std::string, bool> mHeaderHits;
    int mModeIndex=0, mChipIndex=0;
    bool mSearching=false;
    std::string mChipMode, mPersistentStatusWidget, mPersistentActionWidget, mPersistentCard, mPersistentLabel;
    std::string mMasterLabel;
    LLPanel* mPersistentBand=nullptr;
    LLTextBox* mPersistentStatus=nullptr;
    LLButton* mPersistentAction=nullptr;
    LLButton* mPersistentManage=nullptr;
    LLPanel* mBanner=nullptr;
    LLTextBox* mBannerText=nullptr;
    LLTextBox* mMasterState=nullptr;
    ALKitSwitch* mMaster=nullptr;
    ALKitDoc* mDoc=nullptr;
    LLCheckBoxCtrl* mChangedOnly=nullptr;
    LLSearchEditor* mSearch=nullptr;
    std::string prefix() const { return "test"; }
    void requestLayout() {}
    void reshape(int, int, bool) {}
    void scrollToView(LLView*) {}
    void setSearchText(const std::string&);
    void clearSearch();
    void applyLocalFilter(const std::string&, bool);
    ALKitCard* findCard(const std::string& id) { return mCards.at(id); }
    void setModeIndex(int i, bool, bool) { mModeIndex=i; }
    void applyModeAndChip(bool request_layout=true);
    void buildPersistentChrome();
    void buildPersistentChromeWithoutFilterClear();
    void revealPersistentCard();
    void revealPersistentCardWithoutFilterClear();
    std::function<void(LLUICtrl*)> mPersistentMenu;
    void updateMaster();
    void setMasterContext(const std::string&, const std::string&);
    bool revealCard(ALKitCard*, bool);
};
inline void ALKitShell::clearSearch() {
    for (auto* child : children)
        if (auto* page = dynamic_cast<ALKitPage*>(child)) {
            if (changed) page->applyLocalFilter("", true); else page->clearSearch();
        }
}
inline void ALKitShell::setChangedOnly(bool enabled) {
    if (changed == enabled) return;
    changed = enabled;
    for (auto* child : children)
        if (auto* page = dynamic_cast<ALKitPage*>(child)) {
            if (page->mChangedOnly) page->mChangedOnly->setValue(enabled);
            page->setSearchText("");
        }
}
'''


def fixture(root: Path) -> str:
    page_source = (root / 'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    tree = ET.parse(root / 'indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml')
    page = next(tree.getroot().iter('kit_page'))
    quote = json.dumps
    strings = lambda xs: '{' + ','.join(quote(x) for x in xs) + '}'
    lines = [SUPPORT]
    for signature in ('void ALKitPage::applyModeAndChip(', 'void ALKitPage::buildPersistentChrome()',
                      'void ALKitPage::revealPersistentCard()',
                      'void ALKitPage::updateMaster()', 'void ALKitPage::setMasterContext(',
                      'bool ALKitPage::revealCard(', 'void ALKitPage::clearSearch()',
                      'void ALKitPage::setSearchText(', 'void ALKitPage::applyLocalFilter('):
        lines.append(function(page_source, signature))
    # Compile a negative-control callback from the same production method,
    # removing only its shell/checkbox clearing block. No production file changes.
    chrome = function(page_source, 'void ALKitPage::buildPersistentChrome()')
    reveal = function(page_source, 'void ALKitPage::revealPersistentCard()')
    start = reveal.index('    ALKitShell* shell = ALKitShell::find(this);')
    end = reveal.index('    setSearchText("");', start)
    lines.append((reveal[:start] + reveal[end:]).replace(
        'ALKitPage::revealPersistentCard()', 'ALKitPage::revealPersistentCardWithoutFilterClear()', 1))
    lines.append(chrome.replace('ALKitPage::buildPersistentChrome()',
        'ALKitPage::buildPersistentChromeWithoutFilterClear()', 1).replace(
        'revealPersistentCard();', 'revealPersistentCardWithoutFilterClear();'))
    lines.append('void testCase(bool negative_control) { ALKitShell shell; '
                 'auto* owned_page = new ALKitPage; shell.addChild(owned_page); ALKitPage& page = *owned_page; '
                 'page.mChangedOnly = new LLCheckBoxCtrl; page.addChild(page.mChangedOnly);')
    for attr, member in [('chip_mode', 'mChipMode'), ('persistent_status_widget', 'mPersistentStatusWidget'),
                         ('persistent_action_widget', 'mPersistentActionWidget'),
                         ('persistent_card', 'mPersistentCard'), ('persistent_label', 'mPersistentLabel')]:
        lines.append(f'page.{member}={quote(page.get(attr, ""))};')
    for mode in page.findall('kit_page.mode'):
        lines.append('page.mModeDefs.push_back({' + quote(mode.get('name')) + ',' + strings(mode.get('cards').split(',')) + '});')
    for section in page.findall('kit_page.section'):
        lines.append('page.mSections.push_back({' + quote(section.get('id')) + ',' + quote(section.get('label')) + ',' +
                     strings(section.get('cards').split(',')) + '});')
    for card in page.iter('kit_card'):
        lines.append('{ auto* card=new ALKitCard; card->name=' + quote(card.get('id')) +
                     '; page.addChild(card); page.mCards[card->name]=card; }')
    management_widgets = ['cine_add_avatar_rig_group'] + [
        'cine_rig_roster_' + suffix for suffix in ('self', 'a', 'b', 'c', 'd')]
    rows = {row.get('widget'): (card, row) for card in page.iter('kit_card') for row in card.iter('kit_row')}
    for widget in management_widgets:
        card, row = rows[widget]
        assert card.get('id') == 'rig_status'
        assert all(row.get(gate, 'false') == 'false' for gate in ('hidden', 'advanced', 'who_picker'))
        lines.append('{ auto* row = new ALKitRow; row->name=' + quote(widget) +
                     '; row->metadata.widget=row->name; auto* card=page.findCard("rig_status"); '
                     'card->addChild(row); card->content.push_back(row); page.mRows[row->name]=row; }')
    # Start the controls under their real source card; buildPersistentChrome
    # itself must detach them, or the selected-off/other-on checks fail.
    lines.append(r'''
    auto* status = new LLTextBox; status->name = "cine_rig_group_status";
    auto* off = new LLButton; off->name = "cine_all_rigs_off";
    auto* source = page.findCard("rig_status");
    source->addChild(status); source->addChild(off);
    if (negative_control) page.buildPersistentChromeWithoutFilterClear();
    else page.buildPersistentChrome();
    assert(page.mPersistentBand && status->getParent()==page.mPersistentBand);
    assert(off->getParent()==page.mPersistentBand && page.mPersistentManage);
    ALKitSwitch master; master.on=false; page.mMaster=&master;
    auto* banner=new LLPanel; page.addChild(banner); page.mBanner=banner;
    auto* text=new LLTextBox; banner->addChild(text); page.mBannerText=text;
    auto* state=new LLTextBox; page.addChild(state); page.mMasterState=state;
    status->text="2 rigs on";
    page.setMasterContext("This rig", "This rig is off â€” 2 other rigs on");
    page.updateMaster();
    assert(page.mMasterLabel=="This rig" && state->text=="Off");
    assert(banner->visible && text->text=="This rig is off â€” 2 other rigs on");
    page.mModeIndex=0; page.mChipIndex=0; page.applyModeAndChip();
    assert(page.findCard("light_group")->isInVisibleChain());
    assert(page.mPersistentManage->label=="Rigs");
    int hidden_source=0;
    for (int mode=0; mode<static_cast<int>(page.mModeDefs.size()); ++mode)
        for (int section=0; section<static_cast<int>(page.mSections.size()); ++section) {
            page.mModeIndex=mode; page.mChipIndex=section;
            page.applyModeAndChip(); // full production mode + section gates
            hidden_source += !source->visible;
            assert(status->isInVisibleChain() && off->isInVisibleChain());
            assert(page.mPersistentManage->isInVisibleChain());
            assert(status->text=="2 rigs on");
            shell.setChangedOnly(true);
            assert(shell.changedOnly() && page.mChangedOnly->value && page.mSearching);
            assert(!source->isInVisibleChain());
            for (const auto& entry : page.mRows) {
                assert(!entry.second->filterVisible());
                assert(!entry.second->isInVisibleChain());
            }
            assert(page.mPersistentManage->isInVisibleChain());
            // The management affordance must reveal the actual roster card.
            page.mPersistentManage->callback(nullptr, LLSD());
            const bool revealed = source->isInVisibleChain() && !shell.changedOnly() &&
                                  !page.mChangedOnly->value && !page.mSearching;
            assert(revealed == !negative_control);
            for (const auto& entry : page.mRows) {
                assert(entry.second->filterVisible() == !negative_control);
                assert(entry.second->isInVisibleChain() == !negative_control);
            }
            if (!negative_control) assert(source->visible);
            else assert(shell.changedOnly() && !source->visible);
            shell.setChangedOnly(false);
        }
    assert(hidden_source>0); // negative control: old card placement really hides
    page.setMasterContext("Light rig", "Light rig is off. Controls stay editable.");
    assert(page.mMasterLabel=="Light rig");
}
void test() {
    testCase(true);
    testCase(false);
    std::cout << "PASS: real kit chrome reparenting, mode/section gates and management reveal; selected-off/other-on stays visible in Easy and every Advanced section\n";
    std::cout << "PASS: Changed only ON + Rigs clears shell/checkbox and reveals destination, Add avatar and five roster rows; callback without filter clearing fails the same visibility oracle\n";
}
} // namespace RigSessionUI
''')
    lines.append(r'''
namespace RigTargetUI {
struct Rig {
    LLUUID anchor, object;
    bool group = false;
    const LLUUID& getAnchor() const { return anchor; }
    const LLUUID& getObjectTarget() const { return object; }
    bool isGroupEnabled() const { return group; }
    U32 getGroupSlots() const { return 1u; }
    U32 lastResolvedGroupSlots() const { return 1u; }
};
struct LLDirectorCast {
    struct Member { std::string mLastName; };
    std::map<std::string, Member> members;
    static LLDirectorCast& instance() { static LLDirectorCast cast; return cast; }
    LLUUID getSubjectA() const { return {"subject-A"}; }
    LLUUID getSubjectB() const { return {"subject-B"}; }
    LLUUID getSubjectC() const { return {"subject-C"}; }
    LLUUID getSubjectD() const { return {"subject-D"}; }
    static bool isSelf(const LLUUID& id) { return id.value=="self"; }
    static LLUUID canonicalActorId(const LLUUID& id) { return id; }
    const Member* getMember(const LLUUID& id) const {
        auto it=members.find(id.value); return it==members.end() ? nullptr : &it->second;
    }
};
struct ALCineLightRigManager {
    using Slot=ALCineLightRigSlot;
    Rig rigs[5];
    static ALCineLightRigManager& instance() { static ALCineLightRigManager manager; return manager; }
    const Rig& at(Slot slot) const { return rigs[static_cast<int>(slot)]; }
};
struct ALPanelCineLightRig { static std::string rigTargetDescription(S32); };
''')
    panel_source = (root / 'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    lines.append(function(panel_source, 'std::string ALPanelCineLightRig::rigTargetDescription('))
    lines.append(r'''
void test() {
    auto& rig=ALCineLightRigManager::instance().rigs[0];
    auto& cast=LLDirectorCast::instance();
    assert(ALPanelCineLightRig::rigTargetDescription(0)=="You");
    rig.anchor={"subject-A"}; cast.members["subject-A"].mLastName="Alice";
    assert(ALPanelCineLightRig::rigTargetDescription(0)=="Alice");
    rig.anchor={"legacy-other-avatar"};
    assert(ALPanelCineLightRig::rigTargetDescription(0)=="legacy-other-avatar");
    rig.anchor={"self"}; assert(ALPanelCineLightRig::rigTargetDescription(0)=="You");
    rig.anchor={"legacy-other-avatar"}; rig.group=true;
    assert(ALPanelCineLightRig::rigTargetDescription(0)=="group: You");
    std::cout << "PASS: production target description resolves legacy SELF anchors by cast name/UUID and preserves Who's lit semantics\n";
}
} // namespace RigTargetUI
''')
    lines.append(lighting_fixture(panel_source, root))
    lines.append(policy_fixture(root))
    check_ui_contract(root, panel_source, page)
    return '\n'.join(lines)



def check_ui_contract(root: Path, source: str, page) -> None:
    import re
    xml = (root / 'indra/newview/skins/default/xui/en/panel_cine_light_rig.xml').read_text(encoding='utf8')
    kit = (root / 'indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml').read_text(encoding='utf8')
    def valid(text):
        return not re.search(r'create (?:a )?rig group', text, re.I)
    assert valid(source + xml + kit), 'obsolete rig-group instruction in UI'
    assert not valid(kit + '<button label="Create rig group"/>'), 'negative UI string escaped'
    assert 'isRigGroup()' not in function(source, 'void ALPanelCineLightRig::updateAnchorList(')
    assert 'setWhoUnavailable(' not in function(source, 'bool ALPanelCineLightRig::postBuild()')
    easy = next(mode for mode in page.findall('kit_page.mode') if mode.get('name') == 'Easy')
    cards = easy.get('cards').split(',')
    assert cards[cards.index('easy') + 1] == 'light_group'
    section = next(s for s in page.findall('kit_page.section') if s.get('id') == 'easy')
    assert section.get('cards').split(',')[:2] == ['easy', 'light_group']
    group_rows = [row for row in page.iter('kit_row') if row.get('widget') in
                  {'cine_group_self', 'cine_group_a', 'cine_group_b', 'cine_group_c', 'cine_group_d'}]
    def snapshot_owned(rows):
        return len(rows) == 5 and all(row.get('widget_history') == 'false' for row in rows)
    assert snapshot_owned(group_rows), 'checkbox history must not replay auto-on policy'
    import copy
    bad_rows = copy.deepcopy(group_rows)
    bad_rows[0].set('widget_history', 'true')
    assert not snapshot_owned(bad_rows), 'planted checkbox-only history escaped'



def lighting_fixture(source: str, root: Path) -> str:
    lines = [r"""
namespace RigLightingUI {
using RigSessionUI::llformat;
HISTORY_SUPPORT
struct Check {
    LLSD value; bool enabled=true, visible=true; std::string name, label, tip, text;
    const LLSD& getValue() const { return value; }
    const std::string& getName() const { return name; }
    void setColor(int) {}
};
inline void setValueIfChanged(Check* c, const LLSD& v, bool) { c->value=v; }
inline void setEnabledIfChanged(Check* c, bool v) { c->enabled=v; }
inline void setLabelIfChanged(Check* c, const std::string& v) { c->label=v; }
inline void setToolTipIfChanged(Check* c, const std::string& v) { c->tip=v; }
inline void setTextIfChanged(Check* c, const std::string& v) { c->text=v; }
struct KitPage {
    std::map<std::string,ALKitRow> rows;
    ALKitRow* findRow(const std::string& key) { return &rows[key]; }
};
struct LLDirectorCast {
    struct Member { std::string mLastName; };
    LLUUID subjects[4]={{"alice"},{"bob"},{},{}};
    std::map<std::string,Member> members;
    static LLDirectorCast& instance() { static LLDirectorCast cast; return cast; }
    LLUUID getSubjectA() const { return subjects[0]; }
    LLUUID getSubjectB() const { return subjects[1]; }
    LLUUID getSubjectC() const { return subjects[2]; }
    LLUUID getSubjectD() const { return subjects[3]; }
    static bool isSelf(const LLUUID& id) { return id.value=="self"; }
    static LLUUID canonicalActorId(const LLUUID& id) { return id; }
    const Member* getMember(const LLUUID& id) const {
        auto it=members.find(id.value); return it==members.end() ? nullptr : &it->second;
    }
};
struct ALCineLightRig {
    bool enabled=false; U32 slots=0; LLUUID object;
    bool isGroupEnabled() const { return enabled; }
    U32 getGroupSlots() const { return slots; }
    U32 lastResolvedGroupSlots() const { return slots; }
    const LLUUID& getObjectTarget() const { return object; }
    void setGroupSlots(U32 v) { slots=v; }
    void setGroupEnabled(bool v) { enabled=v; }
};
struct ALCineLightRigManager {
    ALCineLightRig rig, other;
    S32 editing=0;
    S32 selectedSlot() const { return editing; }
    ALCineLightRig& at(S32 slot) { return slot == 0 ? rig : other; }
    static ALCineLightRigManager& instance() { static ALCineLightRigManager m; return m; }
    ALCineLightRig& selected() { return at(editing); }
};
inline const LLUUID gAgentID{"self"};
struct ALPanelCineLightRig : LLView {
    template<class T> LLHandle<T> getDerivedHandle() { return {static_cast<T*>(this)}; }
    Check controls[5], enable, status;
    Check* mGroupSlotChecks[5]={&controls[0],&controls[1],&controls[2],&controls[3],&controls[4]};
    Check* mGroupEnable=&enable; Check* mGroupStatus=&status;
    KitPage page; KitPage* mRigKitPage=&page;
    bool mDisplayedGroupEnabled=false;
    U32 mDisplayedGroupSlots=~0u, mDisplayedResolvedSlots=~0u;
    LLUUID mDisplayedGroupObjectTarget;
    int mRadiusDefaultColor=0, mRadiusWarningColor=1;
    void onGroupEnabledCommit(); void onGroupSlotsCommit(); void syncGroupControls();
    void commitGroupState(U32, bool); void commitGroupStateBad(U32, bool);
    void onGroupSlotsCommitManual(); void syncGroupControlsGated();
    void onGroupSlotsCommitBad(); void syncGroupControlsStale();
};
"""]
    for signature in ('void ALPanelCineLightRig::onGroupEnabledCommit()',
                      'void ALPanelCineLightRig::onGroupSlotsCommit()',
                      'void ALPanelCineLightRig::syncGroupControls()',
                      'void ALPanelCineLightRig::commitGroupState('):
        lines.append(function(source, signature))
    lines[0] = lines[0].replace('HISTORY_SUPPORT', history_fixture(root))
    slots = function(source, 'void ALPanelCineLightRig::onGroupSlotsCommit()')
    assert 'commitGroupState(slots, people.size() >= 2);' in slots
    # Keep people in use so the planted original manual-switch behavior compiles /WX.
    lines.append(slots.replace('onGroupSlotsCommit()', 'onGroupSlotsCommitManual()', 1).replace(
        'commitGroupState(slots, people.size() >= 2);', 'commitGroupState(slots, false);'))
    commit = function(source, 'void ALPanelCineLightRig::commitGroupState(')
    replay_enable = 'rig.setGroupEnabled(redo ? enabled : old_enabled);'
    assert replay_enable in commit
    lines.append(commit.replace('::commitGroupState(', '::commitGroupStateBad(', 1).replace(
        replay_enable, 'rig.setGroupEnabled(rig.getGroupSlots() == 6u);'))
    lines.append(slots.replace('::onGroupSlotsCommit()', '::onGroupSlotsCommitBad()', 1).replace(
        'commitGroupState(', 'commitGroupStateBad('))
    sync = function(source, 'void ALPanelCineLightRig::syncGroupControls()')
    switch_sync = 'setValueIfChanged(mGroupEnable, LLSD(enabled), true);'
    assert sync.count(switch_sync) == 1
    lines.append(sync.replace('::syncGroupControls()', '::syncGroupControlsStale()', 1).replace(
        switch_sync, '').replace('setEnabledIfChanged(mGroupEnable,',
                                switch_sync + '\n    setEnabledIfChanged(mGroupEnable,'))
    before = 'setEnabledIfChanged(mGroupSlotChecks[i], object_target.isNull());'
    assert before in sync
    lines.append(sync.replace('syncGroupControls()', 'syncGroupControlsGated()', 1).replace(
        before, 'setEnabledIfChanged(mGroupSlotChecks[i], enabled && object_target.isNull());'))
    lines.append(r"""
bool autoLighting(bool manual) {
    auto& rig=ALCineLightRigManager::instance().rig; rig={};
    ALPanelCineLightRig panel;
    panel.controls[1].value=true; // first tick while the group switch is OFF
    if(manual) panel.onGroupSlotsCommitManual(); else panel.onGroupSlotsCommit();
    if(rig.enabled || rig.slots!=2u) return false;
    panel.controls[2].value=true;
    if(manual) panel.onGroupSlotsCommitManual(); else panel.onGroupSlotsCommit();
    if(!rig.enabled || rig.slots!=6u || !panel.enable.value.asBoolean()) return false;
    panel.controls[1].value=false;
    if(manual) panel.onGroupSlotsCommitManual(); else panel.onGroupSlotsCommit();
    return !rig.enabled && rig.slots==4u && !panel.enable.value.asBoolean();
}
void test() {
    auto& cast=LLDirectorCast::instance();
    cast.members["alice"].mLastName="Audrina";
    cast.members["bob"].mLastName="Bobby";
    assert(autoLighting(false)); assert(!autoLighting(true));
    auto& rig=ALCineLightRigManager::instance().rig; rig={};
    ALPanelCineLightRig panel;
    for(int i=0;i<5;++i){panel.controls[i].name=std::to_string(i); panel.controls[i].label=i==0?"You":std::string(1, char('A'+i-1));}
    panel.syncGroupControls();
    assert(panel.controls[1].enabled && panel.controls[2].enabled);
    assert(panel.controls[1].visible && !panel.controls[3].visible && !panel.controls[4].visible);
    for(int i=1;i<5;++i) assert(panel.controls[i].label==std::string(1,char('A'+i-1)));
    assert(panel.page.rows["1"].label=="Subject A - Audrina");
    panel.mDisplayedGroupSlots=~0u; panel.syncGroupControlsGated();
    assert(!panel.controls[1].enabled); // same oracle rejects original click gate
    rig.object={"object"}; panel.syncGroupControls();
    for(const auto& check:panel.controls) assert(!check.enabled);
    rig={}; panel.mDisplayedGroupSlots=~0u;
    panel.controls[1].value=true;panel.controls[2].value=true;panel.onGroupSlotsCommit();
    panel.enable.value=false;panel.onGroupEnabledCommit();
    assert(!rig.enabled && rig.slots==6u); // manual off retains tick-set storage
    cast.subjects[0]={"self"}; panel.controls[0].value=true;panel.controls[2].value=false;
    panel.onGroupSlotsCommit();
    assert(!rig.enabled && rig.slots==3u); // duplicate self counts once
    assert(!panel.controls[1].visible && panel.controls[0].label=="You");
    assert(panel.page.rows["0"].label=="You (Subject A)");
    cast.subjects[0]={"alice"};
    // Duplicate non-self Subject assignments represent one person.
    cast.subjects[1]={"alice"}; rig={}; panel.mDisplayedGroupSlots=~0u;
    panel.controls[0].value=false;
    panel.controls[1].value=true; panel.controls[2].value=true;
    panel.onGroupSlotsCommit(); assert(!rig.enabled && rig.slots==6u);
    cast.subjects[1]={"bob"};
    for(bool stale : {false, true}) for(int count : {0, 1}) {
        rig={}; panel.mDisplayedGroupSlots=~0u;
        rig.slots=count==0 ? 0u : 2u; panel.syncGroupControls();
        panel.enable.value=true;
        if(stale) panel.syncGroupControlsStale(); else panel.onGroupEnabledCommit();
        assert(panel.enable.value.asBoolean()==stale);
        assert(!rig.enabled);
    }
    for(bool bad : {false,true}) {
        auto& history=ALKitShell::find(&panel)->history();
        history.mOpen.reset(); history.mUndo.clear(); history.mRedo.clear();
        rig={}; panel.mDisplayedGroupSlots=~0u;
        panel.controls[0].value=false; panel.controls[1].value=true; panel.controls[2].value=true;
        panel.onGroupSlotsCommit(); // one snapshot, not one checkbox entry
        assert(history.mUndo.size()==1 && history.mUndo.back().custom.size()==1 && history.mUndo.back().entries.empty());
        panel.enable.value=false; panel.onGroupEnabledCommit();
        assert(!rig.enabled && rig.slots==6u);
        panel.controls[2].value=false;
        if(bad) panel.onGroupSlotsCommitBad(); else panel.onGroupSlotsCommit();
        assert(!rig.enabled && rig.slots==2u);
        const std::string page="light_group/cine_light_rig_tab";
        history.undo(page); // actual production prepareReplay + replay + undo
        assert(rig.slots==6u && rig.enabled==bad);
        if(bad) continue; // the planted auto-on replay fails the disabled oracle
        assert(!panel.enable.value.asBoolean() && panel.controls[2].value.asBoolean());
        history.redo(page); assert(rig.slots==2u && !rig.enabled);
        history.undo(page); assert(rig.slots==6u && !rig.enabled);
        history.undo(page); assert(rig.slots==6u && rig.enabled); // undo switch Off
        history.redo(page); assert(rig.slots==6u && !rig.enabled);
        history.undo(page); history.undo(page); assert(rig.slots==0u && !rig.enabled);
        history.redo(page); assert(rig.slots==6u && rig.enabled);
        assert(!ALCineLightRigManager::instance().other.enabled); // correct slot only
        panel.enable.value=false; panel.onGroupEnabledCommit();
        panel.enable.value=true; panel.onGroupEnabledCommit();
        assert(rig.enabled);
        history.undo(page); assert(!rig.enabled && rig.slots==6u); // undo explicit On
        history.redo(page); assert(rig.enabled && rig.slots==6u);
    }
    {
        auto& manager=ALCineLightRigManager::instance();
        auto& history=ALKitShell::find(&panel)->history();
        history.mOpen.reset(); history.mUndo.clear(); history.mRedo.clear();
        manager.rig.enabled=true; manager.rig.slots=17u;
        manager.other={}; manager.editing=1;
        panel.controls[0].value=false; panel.controls[1].value=true; panel.controls[2].value=true;
        panel.onGroupSlotsCommit();
        const std::string page="light_group/cine_light_rig_tab";
        history.undo(page); assert(manager.other.slots==0u && !manager.other.enabled);
        history.redo(page); assert(manager.other.slots==6u && manager.other.enabled);
        assert(manager.rig.slots==17u && manager.rig.enabled);
        manager.editing=0;
        history.undo(page); // real replay validation refuses the wrong selected slot
        assert(manager.other.slots==6u && manager.other.enabled);
    }
    std::cout<<"PASS: real ALKitHistory undo/redo restores mask + enable; auto-on replay and stale-switch negatives rejected\n";
    std::cout<<"PASS: production Who's lit auto-on at 2/auto-off at 1; manual-switch and disabled-ticks negatives rejected; names/hidden/unassigned/self/object/manual-off checked\n";
}
} // namespace RigLightingUI
""")
    return '\n'.join(lines)



def policy_fixture(root: Path) -> str:
    header = (root / 'indra/newview/alcinelightrigmanager.h').read_text(encoding='utf8')
    policy = function(header, 'struct RigSession')
    select = 'slot < ALCineLightRigSlot::COUNT;'
    off = '        const S32 index = static_cast<S32>(slot);'
    assert select in policy and off in policy
    lines = ['namespace RigPolicyUI {', policy + ';']
    lines.append(policy.replace('struct RigSession', 'struct OldSelectionGate', 1).replace(
        select, 'slot < ALCineLightRigSlot::COUNT && (mGroup || slot == ALCineLightRigSlot::SELF);') + ';')
    lines.append(policy.replace('struct RigSession', 'struct OldOffGate', 1).replace(
        off, '        if (!mGroup && !enabled) { allOff(blobs, shutdown); return; }\n' + off) + ';')
    service = (root / 'indra/newview/aldirectoralloff.cpp').read_text(encoding='utf8')
    begin = service.index('    const S32 selected = root["rig_selected"].asInteger();')
    end = service.index('    std::set<LLUUID> ids;', begin)
    validation = service[begin:end]
    prefix = 'if (root["settings"]["CineLightRigEnabled"].asBoolean()'
    assert prefix in validation
    lines.append('bool consistent(const LLSD& root) { std::string reason;\n' + validation + 'return true; }')
    lines.append('bool consistentOldGate(const LLSD& root) { std::string reason;\n' + validation.replace(
        prefix, 'if ((!root["rig_group"].asBoolean() && (selected != 0 || (mask & ~1))) || root["settings"]["CineLightRigEnabled"].asBoolean()') + 'return true; }')
    lines.append(r"""
template<class Policy> bool independent() {
    Policy policy; ALCineLightRigParamBlob blobs[5];
    for(int i=0;i<5;++i) {
        auto slot=static_cast<ALCineLightRigSlot>(i);
        if(!policy.canSelect(slot)) return false;
        policy.setEnabled(blobs, slot, true, [](S32){});
        if(!blobs[i].mEnabled) return false;
    }
    U32 shutdown=0;
    policy.setEnabled(blobs, ALCineLightRigSlot::B, false, [&](S32 i){shutdown|=1u<<i;});
    return shutdown==4u && blobs[0].mEnabled && blobs[1].mEnabled && !blobs[2].mEnabled && blobs[3].mEnabled && blobs[4].mEnabled;
}
void test() {
    assert(independent<RigSession>());
    assert(!independent<OldSelectionGate>() && !independent<OldOffGate>());
    LLSD root; root["rig_selected"]=2; root["rig_mask"]=14; root["rig_group"]=false;
    root["settings"]["CineLightRigEnabled"]=true;
    assert(consistent(root) && !consistentOldGate(root));
    root["settings"]["CineLightRigEnabled"]=false;
    assert(!consistent(root)); // still reject inconsistent selected-master backups
    std::cout<<"PASS: selectable/per-slot-off/ALL OFF backup oracles reject planted old group gates\n";
}
} // namespace RigPolicyUI
""")
    return '\n'.join(lines)
