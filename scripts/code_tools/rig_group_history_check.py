"""Standalone UI services for running the production ALKitHistory replay path.

Only platform/UI services are doubled. The history container, recording, step
validation, undo, redo and replay methods are extracted from the worktree.
"""
from pathlib import Path
from rig_shadow_presets_check import function


def history_fixture(root: Path) -> str:
    header = (root / 'indra/newview/alkithistory.h').read_text(encoding='utf8')
    source = (root / 'indra/newview/alkithistory.cpp').read_text(encoding='utf8')
    support = r'''
// This fixture has no Rim editors; gesture settlement is exercised by extra_rims_ui_check.
struct ALExtraRimUI { static void finishPendingTrims() {} };
template<class T> struct LLHandle {
    T* ptr=nullptr;
    LLHandle()=default; LLHandle(T* p):ptr(p){}
    T* get() const { return ptr; }
};
struct LLView {
    virtual ~LLView()=default;
    LLView* parent=nullptr;
    LLView* getParent() const { return parent; }
    LLHandle<LLView> getHandle() { return {this}; }
};
struct LLUICtrl : LLView {
    using commit_signal_t = boost::signals2::signal<void (LLUICtrl*, const LLSD&)>;
    commit_signal_t* mCommitSignal=nullptr;
    boost::signals2::connection setCommitCallback(const commit_signal_t::slot_type&);
    LLSD value;
    std::string name;
    std::function<void()> commit;
    const std::string& getName() const { return name; }
    LLHandle<LLUICtrl> getHandle() { return {this}; }
    const LLSD& getValue() const { return value; }
    virtual void setValue(const LLSD& v) { value=v; }
    void onCommit() { if (mCommitSignal) (*mCommitSignal)(this,value); else if (commit) commit(); }
};
struct LLControlVariable {
    LLSD get() const { return {}; } int type() const { return 0; }
    void set(const LLSD&) { assert(false && "Unexpected setting replay"); }
};
struct HistorySettings {
    LLHandle<LLControlVariable> getControl(const std::string&) { return {}; }
} gSavedSettings;
inline bool alUndoValuesEqual(int, const LLSD& a, const LLSD& b) { return a==b; }
inline bool alUndoWidgetValuesEqual(const LLSD& a, const LLSD& b) { return a==b; }
struct LLTimer { static F64 getTotalSeconds() { return 1.; } };
inline U64 alNextUndoSequence() { static U64 sequence=0; return ++sequence; }
struct ALKitRow : LLView {
    std::string label;
    LLSD mLastValue;
    void setDisplayLabel(const std::string& text) { label=text; }
    template<class C> static void setControlVisible(C* c, bool v) { c->visible=v; }
    struct Meta { std::string setting, label, page_key; };
    Meta mMeta;
    ALKitRow* original=nullptr;ALKitRow* sourceRow(){return original;}
    void bindingChanged() {}
    void connectHistory(LLUICtrl*);
    Meta meta() const { return mMeta; }
    LLUICtrl* bound=nullptr;
    LLUICtrl* control() { return bound; }
    void replayBoundValue(const LLSD& v) { assert(bound); bound->setValue(v); bound->onCommit(); }
};
struct ALKitPage : LLView {
    std::string key="light_group/cine_light_rig_tab";
    std::string pageKey() const { return key; }
    ALKitRow* findRowBySetting(const std::string&) { return nullptr; }
    void historyEdited(bool) {}
};
struct ALKitShell;
struct ALPanelWeatherSettings { struct SettingsBatch { void finish() {} }; };
namespace LLNotificationsUtil {
inline void add(const char*, const LLSD&) {}
}
static int sKitHistorySuspend=0, sKitHistoryReplay=0;
static bool sKitInputSourceSet=false;
static LLHandle<LLView> sKitBindingSource;
inline std::string rowPage(LLView*) { return "light_group/cine_light_rig_tab"; }
'''
    cls = function(header, 'class ALKitHistory\n').replace('private:', 'public:')
    lines = [support, cls + ';', r'''
class ALKitHistory::GestureTimer {
public: explicit GestureTimer(ALKitHistory*) {}
};
struct ALKitShell : LLView {
    ALKitHistory history_value;
    static ALKitShell* find(LLView*) { static ALKitShell shell; return &shell; }
    ALKitHistory& history() { return history_value; }
    std::string pagePathLabel(const std::string&) { return {}; }
    ALKitPage* findBuiltPage(const std::string&) { return nullptr; }
    ALKitRow* resolveHistoryRow(const std::string&, const std::string&) { return nullptr; }
};
ALKitHistory::ALKitHistory()=default;
ALKitHistory::~ALKitHistory()=default;
static LLView* fixtureInput=nullptr;
LLView* ALKitHistory::inputPage() { return fixtureInput; }
''']
    ui = (root / 'indra/llui/lluictrl.cpp').read_text(encoding='utf8')
    lines.append(function(ui, 'boost::signals2::connection LLUICtrl::setCommitCallback( const commit_signal_t::slot_type& cb )'))
    row = (root / 'indra/newview/alkitrow.cpp').read_text(encoding='utf8')
    start = row.index('[this](LLUICtrl* c, const LLSD&)', row.index('mHistoryConnection ='))
    brace = row.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (row[end] == '{') - (row[end] == '}')
        end += 1
    observer = row[start:end]
    lines.append('void ALKitRow::connectHistory(LLUICtrl* c){c->setCommitCallback(' + observer + ');}')
    for signature in (
            'ALKitHistory::ScopedSuspend::ScopedSuspend()',
            'ALKitHistory::ScopedSuspend::~ScopedSuspend()',
            'bool ALKitHistory::suspended()',
            'ALKitHistory::ScopedReplay::ScopedReplay()',
            'ALKitHistory::ScopedReplay::~ScopedReplay()',
            'bool ALKitHistory::replaying()',
            'void ALKitHistory::registerContext(',
            'void ALKitHistory::beginGesture(',
            'void ALKitHistory::recordWidget(',
            'void ALKitHistory::addEntry(',
            'void ALKitHistory::normalizeWidgetValue(',
            'void ALKitHistory::recordCustom(',
            'void ALKitHistory::finishGesture()',
            'std::string ALKitHistory::currentContext(',
            'LLUICtrl* ALKitHistory::resolveWidget(',
            'bool ALKitHistory::entryMatches(',
            'bool ALKitHistory::prepareReplay(',
            'void ALKitHistory::discardStep(',
            'bool ALKitHistory::replay(',
            'void ALKitHistory::undo(',
            'void ALKitHistory::redo('):
        lines.append(function(source, signature))
    return '\n'.join(lines)
