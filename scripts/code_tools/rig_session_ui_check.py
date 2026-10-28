"""Extract the real kit visibility/chrome methods for the Release rig harness."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

from rig_shadow_presets_check import function


SUPPORT = r'''
#include <cstdio>
#include <cctype>
#include <sstream>
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
    void updateMaster();
    void setMasterContext(const std::string&, const std::string&);
    bool revealCard(ALKitCard*, bool);
};
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
                      'void ALKitPage::updateMaster()', 'void ALKitPage::setMasterContext(',
                      'bool ALKitPage::revealCard(', 'void ALKitPage::clearSearch()',
                      'void ALKitPage::setSearchText(', 'void ALKitPage::applyLocalFilter('):
        lines.append(function(page_source, signature))
    # Compile a negative-control callback from the same production method,
    # removing only its shell/checkbox clearing block. No production file changes.
    chrome = function(page_source, 'void ALKitPage::buildPersistentChrome()')
    start = chrome.index('                ALKitShell* shell = ALKitShell::find(this);')
    end = chrome.index('                setSearchText("");', start)
    lines.append((chrome[:start] + chrome[end:]).replace(
        'ALKitPage::buildPersistentChrome()', 'ALKitPage::buildPersistentChromeWithoutFilterClear()', 1))
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
    management_widgets = ['cine_create_rig_group', 'cine_leave_rig_group'] + [
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
    status->text="Group: 2 rigs on";
    page.setMasterContext("This rig", "This rig is off — 2 other rigs on");
    page.updateMaster();
    assert(page.mMasterLabel=="This rig" && state->text=="Off");
    assert(banner->visible && text->text=="This rig is off — 2 other rigs on");
    int hidden_source=0;
    for (int mode=0; mode<static_cast<int>(page.mModeDefs.size()); ++mode)
        for (int section=0; section<static_cast<int>(page.mSections.size()); ++section) {
            page.mModeIndex=mode; page.mChipIndex=section;
            page.applyModeAndChip(); // full production mode + section gates
            hidden_source += !source->visible;
            assert(status->isInVisibleChain() && off->isInVisibleChain());
            assert(page.mPersistentManage->isInVisibleChain());
            assert(status->text=="Group: 2 rigs on");
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
    std::cout << "PASS: Changed only ON + Rig group... clears shell/checkbox and reveals destination, Create/Leave and five roster rows; callback without filter clearing fails the same visibility oracle\n";
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
    return '\n'.join(lines)
