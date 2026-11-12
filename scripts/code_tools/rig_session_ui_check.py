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
    std::string lit_group;
    const std::string& getLitGroup() const { return lit_group; }
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
    LLUUID groupSlotActor(Slot slot) const {return static_cast<int>(slot)==0?LLUUID{"self"}:slotActor(slot);}
    LLUUID slotActor(Slot slot) const {
        int i=static_cast<int>(slot);
        return rigs[i].anchor.notNull()?rigs[i].anchor:i==0?LLUUID{"self"}:LLUUID{"subject-"+std::string(1,'A'+i-1)};
    }
};
struct ALPanelCineLightRig { std::string rigTargetDescription(S32,const char* area="anchors");std::string rigActorName(const LLUUID&,const char*); };
''')
    lines.append('std::string ALPanelCineLightRig::rigActorName(const LLUUID& id,const char*) { if(LLDirectorCast::isSelf(id))return "You";auto* m=LLDirectorCast::instance().getMember(id);return m&&!m->mLastName.empty()?m->mLastName:"Actor"; }')
    panel_source = (root / 'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf8')
    lines.append(function(panel_source, 'std::string ALPanelCineLightRig::rigTargetDescription('))
    lines.append(r'''
void test() {
    auto& rig=ALCineLightRigManager::instance().rigs[0];
    auto& cast=LLDirectorCast::instance();
    ALPanelCineLightRig panel;
    assert(panel.rigTargetDescription(0)=="You");
    rig.anchor={"subject-A"}; cast.members["subject-A"].mLastName="Alice";
    assert(panel.rigTargetDescription(0)=="Alice");
    rig.anchor={"legacy-other-avatar"};
    assert(panel.rigTargetDescription(0)=="Actor");
    rig.anchor={"self"}; assert(panel.rigTargetDescription(0)=="You");
    rig.lit_group="guards"; assert(panel.rigTargetDescription(0)=="group guards");
    rig.lit_group.clear();
    rig.anchor={"legacy-other-avatar"}; rig.group=true;
    assert(panel.rigTargetDescription(0)=="group: You");
    cast.members["legacy-other-avatar"].mLastName="Borrowed name";
    assert(panel.rigTargetDescription(0)=="group: You");
    std::cout << "PASS: production target description resolves legacy SELF anchors by cast name/neutral fallback and preserves Who's lit semantics\n";
}
} // namespace RigTargetUI
''')
    lines.append(combo_poll_fixture(root))
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
    from rig_borrow_check import check_contract
    check_contract(root, source)
    easy = next(mode for mode in page.findall('kit_page.mode') if mode.get('name') == 'Easy')
    cards = easy.get('cards').split(',')
    assert cards[cards.index('easy') + 1] == 'light_group'
    section = next(s for s in page.findall('kit_page.section') if s.get('id') == 'easy')
    assert section.get('cards').split(',')[:2] == ['easy', 'light_group']
    rows = list(page.iter('kit_row'))
    old = {'cine_group_enable', 'cine_group_self', 'cine_group_a', 'cine_group_b', 'cine_group_c', 'cine_group_d'}
    def valid_rows(rows):
        return not any(row.get('widget') in old for row in rows)
    assert valid_rows(rows), "Who's lit ticks reappeared"
    assert not valid_rows(rows + [ET.Element('kit_row', widget='cine_group_a')]), 'planted tick escaped'
    target = next(row for row in rows if row.get('widget') == 'cine_lit_target')
    assert target.get('widget_history') != 'false', 'Lights must use normal widget history'
    import director_kit_gen as gen
    import copy
    data = json.loads((root / 'scripts/code_tools/director_kit/director_mockup_data.json').read_text(encoding='utf8'))
    gen.validate_lit_group_card(data)
    bad = copy.deepcopy(data)
    pg = next(p for g in bad['groups'] for p in g['pages'] if p['name']=='cine_light_rig_tab')
    next(c for c in pg['cards'] if c['id']=='light_group')['rows'].append({'w':'cine_group_a','k':'toggle'})
    try: gen.validate_lit_group_card(bad)
    except gen.GenError: pass
    else: raise AssertionError('generator accepted planted old tick')




def lighting_fixture(source: str, root: Path) -> str:
    lines = [r"""
namespace RigLightingUI {
using RigSessionUI::llformat;
HISTORY_SUPPORT
struct Check : LLUICtrl {
    bool enabled=true, visible=true; std::string label, tip, text;
    const LLSD& getValue() const { return value; }
    const std::string& getName() const { return name; }
    bool focus=false;
    std::vector<std::pair<std::string,std::string>> items;
    bool hasFocus() const { return focus; }
    bool open=false;
    bool isListVisible() const { return open; }
    void removeall() { items.clear(); }
    void add(const std::string& label, const std::string& value) { items.push_back({label,value}); }
    void setValue(const LLSD& v) override {
        if(name=="cine_lit_target" && v.asString().compare(0,6,"group:")==0 &&
            std::none_of(items.begin(),items.end(),[&](const auto& item){return item.second==v.asString();})) return;
        value=v;
    }
    void setColor(int) {}
};
inline void setValueIfChanged(Check* c, const LLSD& v, bool) { c->value=v; }
inline void setEnabledIfChanged(Check* c, bool v) { c->enabled=v; }
inline void setLabelIfChanged(Check* c, const std::string& v) { c->label=v; }
inline void setToolTipIfChanged(Check* c, const std::string& v) { c->tip=v; }
inline void setTextIfChanged(Check* c, const std::string& v) { c->text=v; }
using LLTextBox = Check;
struct KitPage {
    std::map<std::string,ALKitRow> rows;
    ALKitRow* findRow(const std::string& key) { return &rows[key]; }
};
struct LLDirectorCast {
    struct Member { std::string mLastName, mGroup; LLUUID mId; };
    ALDirectorGroups::GroupSnapshot captureGroups() const {
        ALDirectorGroups::GroupSnapshot result;
        for(const auto& m:members)result.tags[LLUUID{m.first}]=m.second.mGroup;
        return result;
    }
    void restoreGroups(const ALDirectorGroups::GroupSnapshot& s) {
        groups.clear();
        for(const auto& tag:s.tags){members[tag.first.value].mGroup=tag.second;if(!tag.second.empty())groups[tag.second].push_back(tag.first);}
    }

    U64 getActorChoiceRevision() const { return 1; }
    U64 getGroupRevision() const { return 1; }
    std::map<std::string,uuid_vec_t> groups;
    mutable S32 group_scans=0;
    std::vector<std::string> getGroupNames() const { ++group_scans;std::vector<std::string> names; for(const auto& g:groups) names.push_back(g.first); return names; }
    uuid_vec_t membersInGroup(const std::string& n) const { auto it=groups.find(n); return it==groups.end()?uuid_vec_t():it->second; }
    bool contains(const LLUUID& id) const { return members.count(id.value)!=0; }
    std::vector<Member> getCast() const {std::vector<Member> rows;for(const auto& entry:members){auto m=entry.second;m.mId={entry.first};rows.push_back(m);}return rows;}
    void addToGroup(const uuid_vec_t& ids,const std::string& name){for(const auto& id:ids){members[id.value].mGroup=name;auto& group=groups[name];if(std::find(group.begin(),group.end(),id)==group.end())group.push_back(id);}}
    void add(const LLUUID&) {}
    std::string makeGroup(const uuid_vec_t& ids) { std::string name="Group 1";for(int i=2;groups.count(name);++i)name="Group "+std::to_string(i);for(const auto& id:ids)members[id.value].mGroup=name;groups[name]=ids;return name; }
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
    bool enabled=false; U32 slots=0; LLUUID object, anchor;
    std::string group; uuid_vec_t resolved, stored; S32 total=0; bool fallback=false, wide=false;
    const std::string& getLitGroup() const { return group; }
    void setLitGroup(const std::string& v) { group=v; }
    const LLUUID& getAnchor() const { return anchor; }
    const uuid_vec_t& lastResolvedMembers() const { return resolved; }
    const uuid_vec_t& lastResolvedStoredMembers() const { return stored.empty()?resolved:stored; }
    S32 present=0;
    S32 litGroupPresent() const { return present; }
    S32 litGroupSize() const { return total; }
    bool litGroupFallback() const { return fallback; }
    bool litGroupWide() const { return wide; }
    bool actorOnlyAll() const { return true; }
    bool catchlightActorOnlyEffective() const { return false; }
    bool lightActorOnly(S32) const { return false; }
    bool isGroupEnabled() const { return enabled; }
    U32 getGroupSlots() const { return slots; }
    U32 lastResolvedGroupSlots() const { return slots; }
    const LLUUID& getObjectTarget() const { return object; }
    void setObjectTarget(const LLUUID& id) { object=id; }
    void setGroupSlots(U32 v) { slots=v; }
    void setGroupEnabled(bool v) { enabled=v; }
};
struct ALCineLightRigManager {
    using Slot=S32;
    static constexpr S32 SLOT_COUNT=5;
    static constexpr Slot SLOT_A=1, SLOT_B=2, SLOT_C=3, SLOT_D=4;
    LLUUID resolveRigActorIdentity(const LLUUID& id)const{return LLDirectorCast::canonicalActorId(id);}
    LLUUID resolveBorrowActor(const LLUUID& id,bool=false)const{return id;}
    LLUUID groupSlotActor(Slot slot) const {return static_cast<int>(slot)==0?LLUUID{"self"}:slotActor(slot);}
    LLUUID slotActor(Slot slot) const {
        const auto& c=LLDirectorCast::instance();
        return slot==0?(rig.anchor.notNull()?rig.anchor:LLUUID{"self"}):
            other.anchor.notNull()?other.anchor:slot==1?c.getSubjectA():slot==2?c.getSubjectB():slot==3?c.getSubjectC():c.getSubjectD();
    }
    bool power=true;
    bool isSlotEnabled(Slot) const { return power; }
    void setLitGroup(Slot slot,const std::string& name){at(slot).setLitGroup(name);}
    LLSD captureLitGroups()const {LLSD s=LLSD::emptyArray();s.append(rig.group);s.append(other.group);return s;}
    void restoreLitGroups(const LLSD& s){rig.group=s[0].asString();other.group=s[1].asString();}
    ALCineLightRig rig, other;
    S32 editing=0; bool overflow=false;
    bool actorOnlyOverflow() const { return overflow; }
    S32 actorOnlyUnlit(const uuid_vec_t& ids) const { return ids.size()>6 ? static_cast<S32>(ids.size()-6) : 0; }
    S32 selectedSlot() const { return editing; }
    void setSelectedSlot(Slot slot){editing=slot;}
    void touchSlot(Slot){}
    ALCineLightRig& at(S32 slot) { return slot == 0 ? rig : other; }
    static ALCineLightRigManager& instance() { static ALCineLightRigManager m; return m; }
    ALCineLightRig& selected() { return at(editing); }
};
inline const LLUUID gAgentID{"self"};
struct ALPanelCineLightRig : LLView {
    template<class T> LLHandle<T> getDerivedHandle() { return {static_cast<T*>(this)}; }
    Check target, members, fallback, cap, actor_only, wide;
    Check *mLitTarget=&target, *mLitMembers=&members, *mLitFallback=&fallback,
        *mLitCap=&cap, *mLitActorOnly=&actor_only, *mLitWide=&wide;
    std::vector<std::string> mLitTargetLabels, mLitTargetValues;
    U64 mLitComboCastRevision=0,mLitComboGroupRevision=0;
    S32 mLitComboSlot=-1; std::string mLitComboGroup;
    U32 mLitComboSlots=~0u; bool mLitComboLegacy=false,mLitTargetForceRefresh=true;
    bool mCastListInitialized=false;
    void updateAnchorList() {}
    std::string rigTargetDescription(S32,const char* area="anchors") { return "You"; }
    static std::string rigLabel(S32 slot,const char* area="anchors") { return slot==0?"You":"Rig "+std::to_string(slot+1); }
    static std::string rigLabelBase(S32 slot) { return rigLabel(slot); }
    void clearRigLabels(const char*){}
    void showRigNotice(const std::string&){}
    std::string rigActorName(const LLUUID& id,const char*){if(LLDirectorCast::isSelf(id))return "You";auto* m=LLDirectorCast::instance().getMember(id);return m?m->mLastName:"Actor";}
    void onLitTargetCommit();
    void shareRig(const LLUUID&,S32);void shareRigMissingFallback(const LLUUID&,S32);void shareRigStaleObjectSetting(const LLUUID&,S32);
    void ensureRigActorInCast(const LLUUID& id){LLDirectorCast::instance().members[id.value];}
    void finishRigAction(S32){}
    void installHistoryValidator();
    static bool validateLitTarget(const LLSD&, std::string&);
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
    for signature in ('bool ALPanelCineLightRig::validateLitTarget(',
                      'void ALPanelCineLightRig::onLitTargetCommit()',
                      'void ALPanelCineLightRig::syncGroupControls()'):
        lines.append(function(source, signature))
    share = function(source, 'void ALPanelCineLightRig::shareRig(')
    lines.append(share)
    lines.append(share.replace('::shareRig(', '::shareRigMissingFallback(', 1).replace(
        'group.empty() || cast.membersInGroup(group).empty()', 'group.empty()', 1))
    lines.append(share.replace('::shareRig(', '::shareRigStaleObjectSetting(', 1).replace(
        'gSavedSettings.setString("CineLightRigObjectTarget", "");', '', 1))
    start = source.index('[handle](const std::string& widget, const LLSD& value, std::string& reason)')
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    lines.append('void ALPanelCineLightRig::installHistoryValidator(){'
                 'auto handle=getDerivedHandle<ALPanelCineLightRig>();'
                 'ALKitShell::find(this)->history().registerWidgetValidator("light_group/cine_light_rig_tab",'
                 + source[start:end] + ');}')
    groups = (root / 'indra/newview/aldirectorgroups.h').read_text(encoding='utf8')
    # GroupSnapshot and patchDelta are independent of renderer/UI services.
    start = groups.index('struct GroupSnapshot')
    # Full namespace uses only standard containers and UUID/LLSD services.
    lines[0] = lines[0].replace('HISTORY_SUPPORT', history_fixture(root) + '\nnamespace ALDirectorGroups {\n' + groups[start:groups.index('} // namespace ALDirectorGroups')] + '\n}\n')
    lines[0] = lines[0].replace('struct HistorySettings {',
        'struct HistorySettings { std::map<std::string,LLSD> writes; '
        'void setString(const std::string& key,const std::string& value){writes[key]=value;} '
        'void setBOOL(const std::string& key,bool value){writes[key]=value;}')
    lines.append(r"""
void test() {
    auto& manager=ALCineLightRigManager::instance();
    auto& rig=manager.rig;
    auto& cast=LLDirectorCast::instance();
    KitComboPoll::test();
    ALPanelCineLightRig panel;
    cast.members["ana"].mLastName="Ana"; cast.members["ben"].mLastName="Ben";
    cast.groups["guards"]={{"ana"},{"ben"},{"away"}};
    rig.group="guards"; rig.resolved={{"ana"},{"ben"}}; rig.total=3;
    panel.syncGroupControls();
    assert(panel.members.text=="Lit now: Ana, Ben (2 of 3 here)");
    assert(panel.target.value.asString()=="group:guards" && !panel.fallback.visible);
    rig.resolved={{"self"}}; rig.fallback=true; panel.syncGroupControls();
    assert(panel.fallback.visible && panel.fallback.text.find("lighting this rig's subject")!=std::string::npos);
    rig.total=20; rig.present=20; rig.wide=true; manager.overflow=true; rig.resolved.resize(9);
    panel.syncGroupControls();
    assert(panel.cap.visible && panel.cap.text=="Lighting the first 16 of 20");
    assert(panel.actor_only.visible && panel.actor_only.text.find("3 members aren't lit")!=std::string::npos);
    assert(panel.wide.visible);
    rig.object={"object"};panel.syncGroupControls();
    assert(!panel.fallback.visible && !panel.cap.visible && !panel.actor_only.visible && !panel.wide.visible);
    rig.object={};
    panel.mLitTarget=nullptr; rig.group.clear(); rig.enabled=true;rig.slots=6u;
    cast.subjects[2]={};cast.subjects[3]={};panel.syncGroupControls();
    assert(!panel.controls[3].visible && !panel.controls[4].visible);
    panel.syncGroupControls();assert(!panel.controls[3].visible && !panel.controls[4].visible);
    panel.mLitTarget=&panel.target;
    panel.target.value="subject"; panel.onLitTargetCommit(); assert(rig.group.empty() && !rig.enabled);
    panel.target.focus=true;
    panel.target.value="make"; panel.onLitTargetCommit(); assert(rig.group=="Group 1" && !rig.enabled);
    assert(panel.target.value.asString()=="group:Group 1");
    assert(std::find(panel.target.items.begin(),panel.target.items.end(),
        std::make_pair(std::string("Group: Group 1 (3)"),std::string("group:Group 1")))!=panel.target.items.end());
    panel.target.focus=false;
    assert(cast.groups["Group 1"].size()==3);
    // Normal widget recording, preflight and replay, including a dissolved group.
    auto& history=ALKitShell::find(&panel)->history();
    panel.target.name="cine_lit_target";
    panel.target.commit=[&](){panel.onLitTargetCommit();};
    const std::string page="light_group/cine_light_rig_tab";
    panel.installHistoryValidator();
    panel.target.value="subject"; panel.onLitTargetCommit();
    panel.target.value="group:guards"; panel.onLitTargetCommit();
    history.recordWidget(&panel.target,LLSD("subject"),LLSD("group:guards"),"Lights");
    history.finishGesture();
    history.undo(page); assert(rig.group.empty());
    history.redo(page); assert(rig.group=="guards");
    history.undo(page); cast.groups.erase("guards");
    panel.target.items.erase(std::remove_if(panel.target.items.begin(),panel.target.items.end(),
        [](const auto& item){return item.second=="group:guards";}),panel.target.items.end());
    panel.mLitTargetValues.erase(std::remove(panel.mLitTargetValues.begin(),panel.mLitTargetValues.end(),"group:guards"),panel.mLitTargetValues.end());
    std::string reason; assert(history.prepareReplay(page,true,reason));
    history.redo(page); assert(rig.group=="guards");
    history.undo(page); assert(rig.group.empty());
    // Real row observer precedes the panel observer: transient make is normalized.
    history.mUndo.clear(); history.mRedo.clear();
    ALKitRow row; row.bound=&panel.target; panel.target.parent=&row;
    row.mLastValue="subject";
    row.connectHistory(&panel.target); // actual ALKitRow observer, installed in child construction
    panel.target.setCommitCallback([&](LLUICtrl*,const LLSD&){panel.onLitTargetCommit();}); // panel postBuild
    cast.members["self"].mGroup="old";
    cast.members["alice"].mGroup="old";
    cast.members["bob"].mGroup="old";
    cast.groups.erase("Group 1"); // first Make above used a direct callback
    // Real row -> recordWidget -> addEntry coalescing: select A, then Make twice
    // without a timeout/mouse-up/finishGesture between any of the commits.
    cast.groups["A"]={};panel.mLitTargetForceRefresh=true;panel.syncGroupControls();
    panel.target.value="group:A";panel.target.onCommit();
    assert(history.mOpen && history.mOpen->entries[0].oldv.asString()=="subject");
    panel.target.value="make";panel.target.onCommit();
    assert(!history.mOpen && history.mUndo.size()==2);
    assert(history.mUndo.back().entries.size()==1 && history.mUndo.back().custom.size()==1);
    assert(history.mUndo.back().entries[0].oldv.asString()=="group:A");
    assert(history.mUndo.back().entries[0].newv.asString()=="group:Group 1");
    assert(row.mLastValue.asString()=="group:Group 1");
    panel.target.value="make";panel.target.onCommit();
    assert(!history.mOpen && history.mUndo.size()==3 && rig.group=="Group 2");
    history.undo(page);assert(rig.group=="Group 1" && panel.target.value.asString()=="group:Group 1" && cast.members["alice"].mGroup=="Group 1");
    history.undo(page);assert(rig.group=="A" && panel.target.value.asString()=="group:A" && cast.members["alice"].mGroup=="old");
    history.undo(page);assert(rig.group.empty() && panel.target.value.asString()=="subject");
    history.redo(page);assert(rig.group=="A");
    history.redo(page);assert(rig.group=="Group 1" && panel.target.value.asString()=="group:Group 1" && cast.members["alice"].mGroup=="Group 1");
    history.redo(page);assert(rig.group=="Group 2" && panel.target.value.asString()=="group:Group 2" && cast.members["alice"].mGroup=="Group 2");
    manager.power=false;panel.syncGroupControls();
    assert(panel.members.text=="Lit now: Off" && !panel.fallback.visible && !panel.cap.visible && !panel.wide.visible);
    manager.power=true;rig.group.clear();manager.editing=1;
    manager.other.group="guards";manager.other.fallback=true;manager.other.resolved={{"alice"}};
    panel.syncGroupControls();assert(panel.fallback.text.find("lighting this rig's subject")!=std::string::npos);
    cast.subjects[0].setNull();manager.other.resolved.clear();panel.syncGroupControls();
    assert(panel.members.text=="Lit now: No subject assigned");
    manager.editing=0;panel.syncGroupControls();
    assert(panel.target.items[0].first=="This rig's subject: You");
    S32 scans=cast.group_scans;for(S32 i=0;i<10;++i)panel.syncGroupControls();assert(cast.group_scans==scans);
    // An open list defers construction and publication of new options.
    auto old_items=panel.target.items;panel.target.open=true;panel.mLitTargetForceRefresh=true;
    panel.target.value="group:A"; // keyboard highlight, deliberately not committed
    cast.groups["new"]={};
    for(int tick=0;tick<10;++tick){panel.syncGroupControls();assert(panel.target.value.asString()=="group:A");}
    assert(panel.target.items==old_items && cast.group_scans==scans);
    panel.target.open=false;panel.syncGroupControls();assert(panel.target.items!=old_items && panel.target.value.asString()=="subject");
    rig.group="guards";rig.total=20;rig.present=10;panel.syncGroupControls();assert(!panel.cap.visible);
    cast.members["stored-clone"].mLastName="Clone Ana";rig.resolved={{"runtime-clone"}};rig.stored={{"stored-clone"}};
    panel.syncGroupControls();assert(panel.members.text.find("Clone Ana")!=std::string::npos);
    // Two missing-name steps must both replay, leaving earlier undo reachable.
    history.mUndo.clear();history.mRedo.clear();panel.target.value="subject";panel.onLitTargetCommit();row.mLastValue="subject";
    for(const char* name:{"earlier","later"}){
        cast.groups[name]={};panel.mLitTargetForceRefresh=true;panel.syncGroupControls();
        panel.target.value=std::string("group:")+name;panel.target.onCommit();history.finishGesture();
    }
    cast.groups.erase("earlier");cast.groups.erase("later");
    panel.target.items.clear();panel.mLitTargetValues.clear();
    history.undo(page);assert(rig.group=="earlier");
    history.undo(page);assert(rig.group.empty() && history.mUndo.empty());
    std::cout<<"PASS: production Lights combo/readouts/chips, Make a group, normal widget undo/redo and missing-group fallback replay\n";
    auto shareOracle=[&](S32 fault){
        cast.groups.clear();cast.members.clear();
        gSavedSettings.writes["CineLightRigObjectTarget"]="old-object";
        gSavedSettings.writes["CineLightRigEnabled"]=false;
        manager.other.anchor={"original"};manager.other.group="missing-saved-group";
        manager.other.object={"old-object"};cast.members["original"].mLastName="Original";
        if(fault==1)panel.shareRigMissingFallback({"new"},1);
        else if(fault==2)panel.shareRigStaleObjectSetting({"new"},1);
        else panel.shareRig({"new"},1);
        const auto ids=cast.membersInGroup(manager.other.group);
        return manager.other.group!="missing-saved-group" && std::find(ids.begin(),ids.end(),LLUUID{"original"})!=ids.end() &&
            std::find(ids.begin(),ids.end(),LLUUID{"new"})!=ids.end() && manager.other.object.isNull() &&
            gSavedSettings.writes["CineLightRigEnabled"].asBoolean() && gSavedSettings.writes["CineLightRigObjectTarget"].asString().empty();
    };
    assert(shareOracle(0));assert(!shareOracle(1));assert(!shareOracle(2));
    cast.groups["away-group"]={{"away"}};cast.members["away"].mGroup="away-group";
    manager.other.anchor={"Bob"};manager.other.object={};manager.other.resolved={{"Bob"}};
    manager.other.group="away-group";panel.shareRig({"next"},1);
    const auto shared=cast.membersInGroup("away-group");
    assert(std::find(shared.begin(),shared.end(),LLUUID{"Bob"})!=shared.end());
    assert(manager.other.group=="away-group" && std::find(shared.begin(),shared.end(),LLUUID{"away"})!=shared.end() && std::find(shared.begin(),shared.end(),LLUUID{"next"})!=shared.end());
    // Standalone legacy checkboxes have no kit row. They still receive the
    // same current actor caption rather than retaining a stale Subject label.
    manager.editing=1;manager.other.group.clear();manager.other.object={};manager.other.anchor={"Bob"};
    cast.members["Bob"].mLastName="Bob";panel.mRigKitPage=nullptr;panel.mLitTarget=nullptr;panel.mDisplayedGroupSlots=~0u;
    manager.other.enabled=true;manager.other.slots=2;
    panel.syncGroupControls();assert(panel.controls[1].label=="Rig 2 - Bob");panel.mRigKitPage=&panel.page;panel.mLitTarget=&panel.target;
    std::cout<<"PASS: production Share rebuilds a missing saved group with both actors, retains away groups; planted lost-original-actor bug rejected\n";

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



def combo_poll_fixture(root: Path) -> str:
    """Run the shared row poll and observer between highlight and Enter commit."""
    row = (root / 'indra/newview/alkitrow.cpp').read_text(encoding='utf8')
    history = history_fixture(root)
    history = history.replace('LLView* getParent() const { return parent; }',
        'LLView* getParent() const { return parent; }\n'
        'bool hasAncestor(const LLView* p) const { return parent==p; }')
    history = history.replace('struct LLControlVariable {', r"""
struct LLComboBox : LLUICtrl {
    bool open=false;
    bool isListVisible() const { return open; }
};
struct LLLineEditor : LLUICtrl {};
struct LLTextEditor : LLUICtrl {};
struct LLTextBox : LLUICtrl { bool getWordWrap() const { return false; } };
struct Focus {
    LLView* keyboard=nullptr; bool captured=false;
    bool childHasMouseCapture(const LLView*) const { return captured; }
    LLView* getKeyboardFocus() const { return keyboard; }
} gFocusMgr;
struct ALKitCard { void requestLayout() {} };
struct PollBinding { LLSD get() const { return {}; } };
inline bool alSameValue(const LLSD& a,const LLSD& b) { return a==b; }
struct LLControlVariable {
""")
    history = history.replace('struct LLTimer { static F64 getTotalSeconds() { return 1.; } };',
        'struct LLTimer { static inline F64 now=1.; static F64 getTotalSeconds() { return now; } };')
    history = history.replace('std::string setting, label, page_key;', 'std::string setting, label, page_key, kind;')
    history = history.replace('    LLSD mLastValue;', r"""
    LLSD mLastValue, mShownValue;
    LLUICtrl* mControl=nullptr;
    bool mHeaderProxy=false; PollBinding* mBinding=nullptr;
    bool enabledForEdit() const { return true; }
    struct Rect { int getWidth() const { return 1; } int getHeight() const { return 1; } };
    Rect getRect() const { return {}; }
    int heightForWidth(int) const { return 1; }
    void layoutRow(int) {}
    ALKitCard* card() { return nullptr; }
    void refreshValue() { mShownValue=mControl->getValue(); }
    void pollLiveValue();
""")
    history = history.replace('    LLSD value;',
        '    bool enabled=true; bool getEnabled() const { return enabled; }\n'
        '    void setEnabled(bool v) { enabled=v; }\n    LLSD value;', 1)
    return '\n'.join(['namespace KitComboPoll {', history,
        function(row, 'bool baselineEditActive('),
        function(row, 'void ALKitRow::pollLiveValue()'), r"""
void test() {
    // Arbitrary kit combos: no Lights name, binding or panel-specific guard.
    for(const char* name:{"kit_combo_one","kit_combo_two"}) {
        auto& history=ALKitShell::find(nullptr)->history();
        history.finishGesture(); history.mUndo.clear(); history.mRedo.clear();
        LLComboBox combo; combo.name=name; combo.value="old";
        ALKitRow row; row.mControl=row.bound=&combo; combo.parent=&row;
        row.mLastValue=row.mShownValue="old"; row.connectHistory(&combo);
        combo.open=true; gFocusMgr.keyboard=&combo; combo.value="highlighted";
        for(int tick=0;tick<3;++tick) { LLTimer::now+=.15; row.pollLiveValue(); }
        assert(row.mLastValue.asString()=="old" && !history.mOpen && history.mUndo.empty());
        // Enter closes the list and commits through the real row observer.
        combo.open=false; combo.onCommit(); history.finishGesture();
        assert(history.mUndo.size()==1 && history.mUndo.back().entries.size()==1);
        const auto& entry=history.mUndo.back().entries[0];
        assert(entry.oldv.asString()=="old" && entry.newv.asString()=="highlighted");
        const std::string page="light_group/cine_light_rig_tab";
        history.undo(page); assert(combo.value.asString()=="old");
        history.redo(page); assert(combo.value.asString()=="highlighted");
        // Cancel restores the committed value without adding a step.
        combo.open=true; combo.value="cancelled"; row.pollLiveValue();
        combo.value="highlighted"; combo.open=false; row.pollLiveValue();
        assert(row.mLastValue.asString()=="highlighted" && history.mUndo.size()==1);
        combo.value="external"; row.pollLiveValue();
        assert(row.mLastValue.asString()=="external");
        gFocusMgr.keyboard=nullptr;
    }
    std::cout<<"PASS: shared kit combo keyboard highlight, >100 ms live poll, Enter old/new history and cancel\n";
}
} // namespace KitComboPoll
"""])
