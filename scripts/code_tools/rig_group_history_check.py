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
template<class T> struct LLHandle {
    T* ptr=nullptr;
    LLHandle()=default; LLHandle(T* p):ptr(p){}
    T* get() const { return ptr; }
};
struct LLView {
    virtual ~LLView()=default;
    LLView* getParent() const { return nullptr; }
};
struct LLUICtrl : LLView {
    LLSD value;
    LLHandle<LLUICtrl> getHandle() { return {this}; }
    const LLSD& getValue() const { return value; }
    void setValue(const LLSD& v) { value=v; }
    void onCommit() { assert(false && "Unexpected widget replay"); }
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
namespace boost { namespace signals2 { struct scoped_connection {}; } }
struct ALKitRow : LLView {
    std::string label;
    void setDisplayLabel(const std::string& text) { label=text; }
    template<class C> static void setControlVisible(C* c, bool v) { c->visible=v; }
    struct Meta { std::string setting; };
    Meta meta() const { return {}; }
    LLUICtrl* control() { return nullptr; }
    void replayBoundValue(const LLSD&) { assert(false); }
};
struct ALKitPage : LLView {
    std::string pageKey() const { return "light_group/cine_light_rig_tab"; }
    void historyEdited(bool) {}
};
struct ALKitShell;
struct ALPanelWeatherSettings { struct SettingsBatch { void finish() {} }; };
namespace LLNotificationsUtil {
inline void add(const char*, const LLSD&) {}
}
static int sKitHistorySuspend=0, sKitHistoryReplay=0;
static bool sKitInputSourceSet=false;
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
    ALKitPage* findBuiltPage(const std::string&) { return nullptr; }
    ALKitRow* resolveHistoryRow(const std::string&, const std::string&) { return nullptr; }
};
ALKitHistory::ALKitHistory()=default;
ALKitHistory::~ALKitHistory()=default;
LLView* ALKitHistory::inputPage() { return nullptr; }
''']
    for signature in (
            'ALKitHistory::ScopedSuspend::ScopedSuspend()',
            'ALKitHistory::ScopedSuspend::~ScopedSuspend()',
            'bool ALKitHistory::suspended()',
            'ALKitHistory::ScopedReplay::ScopedReplay()',
            'ALKitHistory::ScopedReplay::~ScopedReplay()',
            'bool ALKitHistory::replaying()',
            'void ALKitHistory::beginGesture(',
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
