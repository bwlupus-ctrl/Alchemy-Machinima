"""Native regression for the Gaze Cues editor with a Nearby (non-cast) gaze target.

The production ALFloaterGazeCues::extraTargetLabel / refreshCastCombo / loadEditor / readEditor /
relabelRows bodies are extracted and compiled against widget stand-ins (no viewer build):
  - round trip: pick Nearby -> load cue -> edit duration -> read cue : mCastRef unchanged;
  - the extra row is labelled by display name, "(away)" when absent, neutral under RLV name hiding
    AND (independently) under @shownearby, and never shows a UUID;
  - a label/name tick never discards a pending edit: it keeps the user's current combo value and
    every editor field, relabels rows in place, and never labels the wrong id (the row stores its id);
  - the target joining the cast leaves a single row.
Mutated production text must fail.

Run: python scripts/code_tools/gaze_cue_nearby_fixture.py [--negative-check]
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_check_tools import ROOT, check_directory, compile_run  # noqa: E402

CUES = ROOT / 'indra/newview/alfloatergazecues.cpp'

SIGNATURES = (
    'std::string ALFloaterGazeCues::extraTargetLabel(',
    'void ALFloaterGazeCues::refreshCastCombo(',
    'void ALFloaterGazeCues::loadEditor(',
    'LLDirectorCast::GazeCue ALFloaterGazeCues::readEditor(',
    'bool ALFloaterGazeCues::relabelRows(',
)


def body(text, signature):
    start = text.index(signature)
    i = text.index('{', start) + 1
    depth = 1
    while depth:
        c = text[i]
        depth += (c == '{') - (c == '}')
        i += 1
    return text[start:i]


STUBS = r'''
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using S32 = int; using F32 = float; using F64 = double;
template<class T> T llmax(T a, T b) { return a > b ? a : b; }
template<class T> T llclamp(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }
struct LLUUID {
  std::string v; LLUUID() = default; explicit LLUUID(const std::string& s) : v(s) {}
  bool isNull() const { return v.empty(); } bool notNull() const { return !v.empty(); }
  bool operator==(const LLUUID& o) const { return v == o.v; } bool operator!=(const LLUUID& o) const { return v != o.v; }
  std::string asString() const { return v; }
  static const LLUUID null;
};
const LLUUID LLUUID::null;
typedef std::vector<LLUUID> uuid_vec_t;
struct LLSD {
  std::string text; double number = 0;
  LLSD() = default; LLSD(const std::string& s) : text(s) {} LLSD(const char* s) : text(s) {}
  LLSD(int n) : text(std::to_string(n)), number(n) {} LLSD(double n) : text(std::to_string(n)), number(n) {}
  LLSD(float n) : LLSD(double(n)) {} LLSD(bool b) : LLSD(int(b)) {}
  std::string asString() const { return text; } int asInteger() const { return int(number); } double asReal() const { return number; }
};
struct Spin { LLSD v; void setValue(const LLSD& x) { v = x; } LLSD getValue() const { return v; } };
struct Check { bool on = false; void set(bool b) { on = b; } bool get() const { return on; } };
struct Combo {
  struct Item { std::string label; std::string value; }; std::vector<Item> items; int selected = -1;
  bool open = false; int hides = 0; int last_index = -1;   // LLComboBox::mLastSelectedIndex
  LLSD value;
  bool isListVisible() const { return open; }
  // real LLComboBox::hideList: re-selects the REMEMBERED index (setSelectedByValue never updates it)
  void hideList() { if (open && last_index >= 0 && last_index < int(items.size())) { selected = last_index; value = LLSD(items[selected].value); } open = false; ++hides; }
  bool setCurrentByIndex(S32 i) { if (i < 0 || i >= int(items.size())) return false; selected = i; last_index = i; value = LLSD(items[i].value); return true; }
  S32 getCurrentIndex() const { return selected; }
  void removeall() { items.clear(); selected = -1; value = LLSD(); }
  void add(const std::string& label, const LLSD& v) { items.push_back({label, v.text}); }
  bool setSelectedByValue(const LLSD& v, bool) { for (int i = 0; i < int(items.size()); ++i) if (items[i].value == v.text) { selected = i; value = LLSD(v.text); return true; } return false; }
  void selectFirstItem() { setCurrentByIndex(0); }
  LLSD getSelectedValue() const { return selected < 0 ? LLSD(std::string()) : LLSD(items[selected].value); }
  void setValue(const LLSD& v) { value = v; } LLSD getValue() const { return value; }
};
struct LLScrollListCell { std::string text; void setValue(const LLSD& v) { text = v.text; } };
struct LLScrollListItem { std::vector<LLScrollListCell> cells; LLSD value; LLSD getValue() const { return value; } LLScrollListCell* getColumn(S32 i) { return i >= 0 && i < int(cells.size()) ? &cells[i] : nullptr; } };
struct LLScrollListCtrl { std::vector<LLScrollListItem> items; int selected = -1, clears = 0, sortColumn = -1, needSort = 0;
  S32 getSortColumnIndex() const { return sortColumn; } void setNeedsSort(bool b = true) { needSort += b; }
  S32 getItemCount() const { return int(items.size()); } LLScrollListItem* getItemByIndex(S32 i) { return i >= 0 && i < int(items.size()) ? &items[i] : nullptr; } };
typedef Combo LLComboBox;
namespace ALGazeMath { enum EGazeCueMacro { GAZE_MACRO_NONE = 0, GAZE_MACRO_DOUBLE_TAKE, GAZE_MACRO_BUTTON_LOOK, GAZE_MACRO_CREEP_TURN, GAZE_MACRO_OBJECT_GLANCE }; }
struct LLActorMover { struct GazeTarget { enum EMode { MOTION = 0, CAMERA, CAST_MEMBER, FIXED_POINT, OBJECT };
  EMode mMode = MOTION; LLUUID mCastRef; }; };
struct LLDirectorCast {
  enum EGazeCueStyle { GAZE_CUE_STYLE_DEFAULT = 0, GAZE_CUE_STYLE_SNAP, GAZE_CUE_STYLE_SMOOTH, GAZE_CUE_STYLE_CASUAL };
  struct GazeCue { double mStartSec = 0, mDurationSec = 0; LLActorMover::GazeTarget mTarget; ALGazeMath::EGazeCueMacro mMacro = ALGazeMath::GAZE_MACRO_NONE;
    EGazeCueStyle mAcquireStyle = GAZE_CUE_STYLE_DEFAULT, mReleaseStyle = GAZE_CUE_STYLE_DEFAULT;
    float mMacroGapSec = 0, mMacroDwellSec = 0; int mHoldFramesPast = 0; bool mThoughtResidue = false; };
  typedef std::vector<GazeCue> GazeCueList;
  uuid_vec_t ids; bool present = true;
  static LLDirectorCast& instance() { static LLDirectorCast c; return c; }
  const uuid_vec_t& getIds() const { return ids; }
  static bool isSelf(const LLUUID& id) { return id.v == "self"; }
  void* resolve(const LLUUID&, bool) { return present ? this : nullptr; }
};
bool rlv_hides = false;          // RLV name hiding (@shownames)
bool rlv_blocks_nearby = false;  // RLV @shownearby: independent of name hiding
namespace ALGazeNearby {
  inline std::string personDisplayName(const LLUUID& id) { return rlv_hides || id.v != "stranger" ? std::string() : std::string("Display Stranger"); }
  inline bool nearbyAllowed() { return !rlv_blocks_nearby; }
  inline std::string nonCastTargetName(const LLUUID& id) { return nearbyAllowed() ? personDisplayName(id) : std::string(); }
  inline std::string awayLabel(const std::string& n) { return (n.empty() ? std::string("Unknown person") : n) + " (away)"; }
}
std::string actorName(const LLUUID& id) { return id.v == "cast1" ? "Cast One" : "Unknown"; }
struct ALFloaterGazeCues {
  LLUUID mActorId{std::string("actor")}; std::string mCastSignature, mExtraLabel; LLUUID mExtraRef; LLActorMover::GazeTarget mEditorTarget;
  Combo cast, mode, macro, acq, rel; Spin time, dur, gap, dwell, hold; Check residue;
  Combo* mCastTarget = &cast; Combo* mTargetMode = &mode; Combo* mMacro = &macro; Combo* mAcquireStyle = &acq; Combo* mReleaseStyle = &rel;
  Spin* mTime = &time; Spin* mDuration = &dur; Spin* mGap = &gap; Spin* mDwell = &dwell; Spin* mHoldFrames = &hold; Check* mResidue = &residue;
  void refreshEditorVisibility() {}
  static std::string targetLabel(const LLDirectorCast::GazeCue& c) { const std::string n = ALGazeNearby::nonCastTargetName(c.mTarget.mCastRef); return n.empty() ? "Unknown" : n; }
  static bool relabelRows(LLScrollListCtrl* list, const LLDirectorCast::GazeCueList& cues, S32 column);
  std::string extraTargetLabel() const; void refreshCastCombo(bool force = false, bool relabel = false);
  void loadEditor(const LLDirectorCast::GazeCue& cue); LLDirectorCast::GazeCue readEditor(const LLDirectorCast::GazeCue& base) const;
};
'''

TEST = r'''
static void require(bool ok, const char* what) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(2); } }
static LLDirectorCast::GazeCue cue_for(const char* ref, double duration) {
  LLDirectorCast::GazeCue c; c.mStartSec = 1.0; c.mDurationSec = duration; c.mTarget.mMode = LLActorMover::GazeTarget::CAST_MEMBER;
  c.mTarget.mCastRef = LLUUID(std::string(ref)); return c; }
static int rows_with(const Combo& c, const std::string& value) { int n = 0; for (auto& i : c.items) n += i.value == value; return n; }
static bool has_row(const Combo& c, const std::string& value) { return rows_with(c, value) > 0; }
static std::string label_of(const Combo& c, const std::string& value) { for (auto& i : c.items) if (i.value == value) return i.label; return "<none>"; }
static void reset() { auto& dc = LLDirectorCast::instance(); dc.ids = {LLUUID(std::string("cast1")), LLUUID(std::string("actor"))}; dc.present = true; rlv_hides = false; rlv_blocks_nearby = false; }
int main() {
  {
    // pick Nearby -> load cue -> edit duration -> read cue: the saved reference is unchanged
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("stranger", 2.0));
    require(has_row(f.cast, "stranger") && f.cast.getSelectedValue().asString() == "stranger", "saved Nearby target not offered/selected");
    require(label_of(f.cast, "stranger") == "Display Stranger", "extra row not labelled by display name");
    f.dur.setValue(LLSD(3.5));
    auto out = f.readEditor(cue_for("stranger", 2.0));
    require(out.mTarget.mCastRef == LLUUID(std::string("stranger")) && out.mDurationSec == 3.5, "editing a Nearby cue changed its target");
    auto added = f.readEditor(LLDirectorCast::GazeCue());
    require(added.mTarget.mCastRef == LLUUID(std::string("stranger")), "adding a cue lost the Nearby target");
    f.loadEditor(cue_for("cast1", 1.0));
    require(f.mExtraLabel.empty() && f.cast.getSelectedValue().asString() == "cast1", "cast target got an extra row");
    f.loadEditor(cue_for("", 1.0));
    require(f.cast.getSelectedValue().asString().empty(), "unset target not selected");
  }
  {
    // away, then RLV hides the name, then it heals: relabelled on the tick, selection kept, never a UUID
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("stranger", 2.0));
    LLDirectorCast::instance().present = false; f.refreshCastCombo(false, true);
    require(label_of(f.cast, "stranger") == "Display Stranger (away)" && f.cast.getSelectedValue().asString() == "stranger", "away label");
    rlv_hides = true; f.cast.open = true; f.refreshCastCombo(false, true);
    require(label_of(f.cast, "stranger") == "Unknown person (away)" && f.cast.hides == 1, "RLV did not redact the extra row (and close the open list)");
    for (auto& i : f.cast.items) require(i.label.find("stranger") == std::string::npos, "a row shows the id");
    rlv_hides = false; f.refreshCastCombo(false, true);
    require(label_of(f.cast, "stranger") == "Display Stranger (away)", "the name did not heal");
  }
  {
    // @shownearby, independently of name hiding: neutral label only, saved target kept
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("stranger", 2.0));
    require(!rlv_hides && label_of(f.cast, "stranger") == "Display Stranger", "setup");
    rlv_blocks_nearby = true; f.refreshCastCombo(false, true);
    require(label_of(f.cast, "stranger") == "Unknown person" && f.cast.getSelectedValue().asString() == "stranger", "@shownearby did not neutralise the saved target label");
  }
  {
    // a name/RLV tick never discards a pending edit: editor fields and the user's combo choice stay
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("stranger", 2.0));
    f.dur.setValue(LLSD(9.0)); f.time.setValue(LLSD(4.0));
    f.cast.setSelectedByValue(LLSD(std::string("cast1")), true); // changed the target (uncommitted)
    LLDirectorCast::instance().present = false; rlv_hides = true;
    f.refreshCastCombo(false, true);
    require(f.cast.getSelectedValue().asString() == "cast1", "a label tick reset the user's combo selection to the saved target");
    require(f.dur.getValue().asReal() == 9.0 && f.time.getValue().asReal() == 4.0, "a label tick changed an editor field");
    require(label_of(f.cast, "stranger") == "Unknown person (away)", "the extra row was not relabelled");
    auto out = f.readEditor(cue_for("stranger", 2.0));
    require(out.mTarget.mCastRef == LLUUID(std::string("cast1")), "the pending combo choice was not what the editor reads");
  }
  {
    // the target joins the cast between ticks: one row, no stale extra row; the choice is kept
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("stranger", 2.0));
    LLDirectorCast::instance().ids.push_back(LLUUID(std::string("stranger")));
    f.refreshCastCombo(false, false);
    require(rows_with(f.cast, "stranger") == 1 && f.mExtraLabel.empty() && f.mExtraRef.isNull(), "a cast member got a duplicate extra row");
    require(f.cast.getSelectedValue().asString() == "stranger", "the selection was lost when the target joined the cast");
    // an unrelated cast change (no label change) still rebuilds the rows
    LLDirectorCast::instance().ids.push_back(LLUUID(std::string("cast2")));
    f.refreshCastCombo(false, false);
    require(has_row(f.cast, "cast2"), "a cast change did not rebuild the rows");
  }
  {
    // the saved target id changes without a load: the extra row follows the NEW id, never the old name
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("stranger", 2.0));
    f.mEditorTarget.mCastRef = LLUUID(std::string("other"));
    f.refreshCastCombo(false, false);
    require(rows_with(f.cast, "other") == 1 && label_of(f.cast, "other") != "Display Stranger" && f.mExtraRef == LLUUID(std::string("other")),
            "the extra row's label sits on the wrong id");
    require(!has_row(f.cast, "stranger") || label_of(f.cast, "stranger") != "Display Stranger", "the old id kept the name");
  }
  {
    // list rows are relabelled in place: same rows, same selection, nothing cleared
    reset(); LLScrollListCtrl list; list.items.resize(2); for (auto& it : list.items) it.cells.resize(3);
    list.selected = 1;
    LLDirectorCast::GazeCueList cues = {cue_for("stranger", 1.0), cue_for("cast1", 1.0)};
    require(ALFloaterGazeCues::relabelRows(&list, cues, 1) && list.items[0].cells[1].text == "Display Stranger", "rows not labelled");
    rlv_hides = true;
    require(ALFloaterGazeCues::relabelRows(&list, cues, 1) && list.items[0].cells[1].text == "Unknown" && list.selected == 1 && list.clears == 0, "rows not redacted in place");
    cues.pop_back();
    require(!ALFloaterGazeCues::relabelRows(&list, cues, 1), "a row-count mismatch must report a reload");
  }
  {
    // remembered selection: select Nearby P, a cast member joins (P's row moves), open+dismiss -> still P
    reset(); ALFloaterGazeCues f;
    f.loadEditor(cue_for("", 1.0)); // remembers row 0 ("(unset)") like a real combo would
    f.loadEditor(cue_for("stranger", 2.0));
    f.cast.open = true; f.cast.hideList();
    require(f.cast.getSelectedValue().asString() == "stranger", "a forced load left a stale remembered index");
    LLDirectorCast::instance().ids.push_back(LLUUID(std::string("cast2")));
    f.refreshCastCombo(false, false);
    require(f.cast.getSelectedValue().asString() == "stranger", "P not kept after a cast change");
    f.cast.open = true; f.cast.hideList();
    require(f.cast.getSelectedValue().asString() == "stranger", "dismissing the list re-selected a stale row index");
    f.loadEditor(cue_for("stranger", 2.0));
    f.cast.open = true; f.cast.hideList();
    require(f.cast.getSelectedValue().asString() == "stranger", "a forced load left a stale remembered index");
  }
  {
    // sorted list: each row's VALUE is its cue; labels follow the cue, rows/selection stay
    reset(); LLScrollListCtrl list; list.items.resize(2); for (auto& it : list.items) it.cells.resize(3);
    list.items[0].value = LLSD(1); list.items[1].value = LLSD(0); list.selected = 0; list.sortColumn = 1;
    LLDirectorCast::GazeCueList cues = {cue_for("stranger", 1.0), cue_for("cast1", 1.0)};
    require(ALFloaterGazeCues::relabelRows(&list, cues, 1), "sorted relabel failed");
    require(list.items[0].cells[1].text == "Unknown" && list.items[1].cells[1].text == "Display Stranger", "a sorted row got another cue's label");
    require(list.selected == 0 && list.needSort == 1, "selection changed / sorted column not re-sorted");
    list.items[0].value = LLSD(7);
    require(!ALFloaterGazeCues::relabelRows(&list, cues, 1), "an invalid row value must report a reload");
  }
  std::puts("PASS: gaze cue editor keeps a Nearby target (round trip, away, RLV and @shownearby, pending edits, cast join, in-place relabel)");
  return 0;
}
'''


def source(text):
    parts = [STUBS, body(text, 'bool selectComboByValue(')]
    for signature in SIGNATURES:
        parts.append(body(text, signature))
    return '\n'.join(parts) + TEST


MUTANTS = {
    'extra_row_not_added': ('    if (!mExtraLabel.empty())\n    {\n        // The saved non-cast target',
                            '    if (false)\n    {\n        // The saved non-cast target'),
    'load_does_not_offer_row': ('    refreshCastCombo(true); // offers the cue\'s saved non-cast target as a row before it is selected\n', ''),
    'relabel_ignored': ('    if (!force && !relabel && !context_changed)\n    {\n        return;\n    }',
                        '    if (!force && !context_changed)\n    {\n        return;\n    }'),
    'label_is_id': ('    const std::string name = ALGazeNearby::nonCastTargetName(ref);\n',
                    '    const std::string name = ref.asString();\n'),
    'away_not_marked': ('    return ALGazeNearby::awayLabel(name);', '    return name.empty() ? std::string("Unknown person") : name;'),
    'open_list_kept': ('    if (mCastTarget->isListVisible())\n    {\n        mCastTarget->hideList(); // a redaction must not leave the old rows open\n    }\n', ''),
    'read_drops_ref': ('    cue.mTarget.mCastRef = cast_value.empty()\n        ? LLUUID::null : LLUUID(cast_value);',
                       '    cue.mTarget.mCastRef = LLUUID::null;'),
    'shownearby_ignored': ('    const std::string name = ALGazeNearby::nonCastTargetName(ref);\n',
                           '    const std::string name = ALGazeNearby::personDisplayName(ref);\n'),
    'pending_choice_discarded': ('    if (!force && selectComboByValue(mCastTarget, keep_value))\n    {\n        return; // the user\'s current choice (a pending, uncommitted edit) is kept\n    }\n', ''),
    'extra_row_on_wrong_id': ('        mCastTarget->add(mExtraLabel, LLSD(mExtraRef.asString()));',
                              '        mCastTarget->add(mExtraLabel, LLSD(std::string("stranger")));'),
    'cast_join_keeps_row': ('    const bool rows_changed = force || context_changed || label != mExtraLabel || extra_ref != mExtraRef;',
                            '    const bool rows_changed = force || label != mExtraLabel || extra_ref != mExtraRef;'),
    'relabel_rows_rebuilds': ('        cell->setValue(LLSD(targetLabel(cues[cue_index])));', '        list->clears++; cell->setValue(LLSD(targetLabel(cues[cue_index])));'),
    'combo_index_not_synced': ('    combo->setCurrentByIndex(combo->getCurrentIndex());\n', ''),
    'relabel_by_position': ('        const S32 cue_index = item ? item->getValue().asInteger() : -1;', '        const S32 cue_index = row;'),
    'relabel_no_resort': ('        list->setNeedsSort(true);', '        (void)0;'),
    'relabel_ignores_invalid_value': ('        if (!cell || cue_index < 0 || cue_index >= static_cast<S32>(cues.size()))', '        if (!cell)'),
    'relabel_ignores_mismatch': ('    if (list->getItemCount() != static_cast<S32>(cues.size()))\n    {\n        return false;\n    }\n', ''),
}


def main():
    text = CUES.read_text(encoding='utf-8')
    out = check_directory('gaze_cue_nearby')
    result = compile_run(out, source(text), 'cue_real')
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout.strip())
    if '--negative-check' in sys.argv:
        for name, (old, new) in MUTANTS.items():
            assert text.count(old) == 1, ('mutant target not found exactly once', name, text.count(old))
            exe = compile_run(out, source(text.replace(old, new)), 'cue_mutant_' + name, run=False)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            assert run.returncode == 2, ('cue mutant survived: ' + name, run.stdout, run.stderr)
            print('PASS: cue regression rejected: ' + name + ' (' + run.stderr.strip() + ')')


if __name__ == '__main__':
    main()
