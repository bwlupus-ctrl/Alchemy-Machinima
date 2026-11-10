#!/usr/bin/env python3
"""Check the clone right-click menus, the Clone overlay submenu and Director "Select".

Usage::

    python scripts/code_tools/director_clone_menu_check.py --check
    python scripts/code_tools/director_clone_menu_check.py --selftest

Two layers, both exit 1 on any finding:

1. NATIVE: indra/newview/alghostclonemenulogic.h (header-only, standard library) is
   compiled and run. Every rule has a planted negative: a deliberately broken variant
   must be REJECTED by the same oracle that accepts the production rule
   (right-drag opens the menu, an item acting on a clone outside the picked/selected
   set, the overlay submenu on an avatar with no overlay, pick mode left on after the
   Director closes, Ctrl-click adding an avatar to a set, a non-cast avatar leaving
   the Actor bar unchanged).
2. STATIC: the production sites are pinned to those rules (call sites in
   lltoolpie.cpp / llviewerwindow.cpp, the four avatar menus carrying one identical
   overlay fragment whose Style items equal the Clones page style combo, menu item
   order, every XUI callback registered, strings in strings.xml, pick-mode exits).
   --selftest mutates the sources and demands each mutation is found.

Standard library only (plus native_check_tools for the MSVC compile).
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / "indra/newview"
XUI = NV / "skins/default/xui/en"

AVATAR_MENUS = ("menu_avatar_other.xml", "menu_avatar_self.xml",
                "menu_attachment_other.xml", "menu_attachment_self.xml")

NATIVE = r'''
#include "alghostclonemenulogic.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace ALGhostCloneMenuLogic;

static int failures = 0;
static void expect(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++failures; } }

// ---- oracles: true = the behaviour is correct ---------------------------------
template <class G> bool dragNeverOpens()
{
    G g; g.press(100, 100); g.move(100 + 40, 100); return !g.release(100, 100);          // dragged away and back
}
template <class G> bool dragAwayNeverOpens()
{
    G g; g.press(100, 100); return !g.release(100 + 40, 100 + 10);
}
template <class G> bool clickOpens()
{
    G g; g.press(100, 100); g.move(101, 101); return g.release(102, 101);              // 3 px per axis in total
}
template <class G> bool boundaryIsCamerasSlop()
{
    // LLToolCamera: outside at accumulated travel >= 4 on either axis
    G a; a.press(10, 10); a.move(12, 10); a.move(10, 10);                                // 2 + 2 = 4: a drag
    G b; b.press(10, 10); b.move(13, 10);                                                // 3: still a click
    G c; c.press(10, 10); c.move(10, 14);                                                // 4 on y: a drag
    return !a.release(10, 10) && b.release(13, 10) && !c.release(10, 14);
}
template <class G> bool releaseWithoutPressNeverOpens()
{
    G g; return !g.release(5, 5);
}
// mutant: ignores movement entirely (a drag opens the menu)
struct BadGestureNoDrag
{
    bool a = false;
    void press(int, int) { a = true; }
    void move(int, int) {}
    bool release(int, int) { bool o = a; a = false; return o; }
};
// mutant: only looks at where the pointer ended (a drag that returns opens the menu)
struct BadGestureEndpointOnly
{
    bool a = false; int px = 0, py = 0;
    void press(int x, int y) { a = true; px = x; py = y; }
    void move(int, int) {}
    bool release(int x, int y) { bool o = a && (x - px) * (x - px) + (y - py) * (y - py) <= 16; a = false; return o; }
};

typedef std::vector<int> Ids;
// A menu action is acceptable only when what it touched stays inside the plan.
bool actionStaysInPlan(int picked, const Ids& sel, const Ids& touched) { return targetsAllowed(picked, sel, touched); }
// mutant planner: acts on every clone the Studio has
Ids badPlanAllClones(int, const Ids&) { return Ids{1, 2, 3, 4, 5}; }

// overlay submenu visibility
template <class F> bool overlayRuleOk(F visible) { return !visible(0) && visible(1) && visible(3); }
bool badAlwaysVisible(size_t) { return true; }

// Director Select, executed as real state transitions: what the Cast and the
// Actor bar hold afterwards. The plan is a parameter so broken planners can be
// run through the same oracle.
struct World { bool inCast = false; int actor = 0; };
void execute(World& w, SelectAction a)
{
    if (a == SelectAction::AddToCastThenSetActor) { w.inCast = true; w.actor = 1; }
    else if (a == SelectAction::SetActor) { w.actor = 1; }
    // Refuse: nothing changes
}
typedef SelectAction (*Planner)(const SelectInput&);
SelectAction badPlanNoAdd(const SelectInput& in)      // forgets the Cast add
{ return in.mIsClone ? planSelect(in) : SelectAction::SetActor; }
SelectAction badPlanSilentRefuse(const SelectInput& in) // refuses a plain non-cast avatar
{ return !in.mIsClone && !in.mInCast ? SelectAction::Refuse : planSelect(in); }
SelectAction badPlanCtrlAvatar(const SelectInput& in)   // Ctrl on an avatar does something else
{ return !in.mIsClone && in.mAdditive ? SelectAction::CloneListAdd : planSelect(in); }
bool avatarSelectOk(Planner plan)
{
    for (int cast = 0; cast < 2; ++cast)
        for (int ctrl = 0; ctrl < 2; ++ctrl)
        {
            SelectInput in; in.mInCast = cast; in.mAdditive = ctrl;
            World w; w.inCast = cast;
            execute(w, plan(in));
            if (!(w.inCast && w.actor == 1)) return false;      // always in the Cast and the actor
        }
    SelectInput refused; refused.mCastRefuses = true;
    World w; execute(w, plan(refused));
    return !w.inCast && w.actor == 0;                           // a refusal changes nothing
}

// pick mode exits
template <class P> bool exitsAll()
{
    for (auto e : { PickMode::Exit::Esc, PickMode::Exit::Button, PickMode::Exit::DirectorClosed, PickMode::Exit::ToolReplaced })
    { P p; p.start(); p.exit(e); if (p.active()) return false; }
    return true;
}
struct BadPickIgnoresClose
{
    bool a = false; void start() { a = true; } bool active() const { return a; }
    void exit(PickMode::Exit e) { if (e != PickMode::Exit::DirectorClosed) a = false; }
};

bool ctrlRuleOk(bool (*f)(bool, bool))
{
    return f(true, true) && !f(true, false) && !f(false, true) && !f(false, false);
}
bool badCtrlAddsAvatar(bool, bool ctrl) { return ctrl; }

// Ctrl-add onto a list that may be stale: A+B on Clones, leave the page,
// plain-select C, Ctrl-add D must give C+D (never A+B+D, never just D).
template <class F> bool ctrlAddOk(F plan)
{
    const Ids stale{1, 2};
    const bool moved = true;                     // the shared selection moved since the list last saw it
    Ids r = plan(stale, moved, 3, 4);
    if (r != Ids{3, 4}) return false;
    Ids unchanged = plan(Ids{1, 2}, false, 2, 4);   // list in sync: plain Ctrl-add
    if (unchanged != Ids{1, 2, 4}) return false;
    Ids fresh = plan(Ids{}, true, 0, 4);              // freshly built host, nothing shared
    if (fresh != Ids{4}) return false;
    Ids again = plan(Ids{3, 4}, false, 4, 4);         // already there
    return again == Ids{3, 4};
}
Ids realCtrlAdd(const Ids& l, bool m, int shared, int add) { return additiveRows(l, m, shared, add, 0); }
Ids badKeepsStale(const Ids& l, bool, int, int add) { Ids r = l; r.push_back(add); return r; }          // resurrects A+B
Ids badDropsShared(const Ids&, bool, int, int add) { return Ids{add}; }                                   // forgets C
Ids badAlwaysReplace(const Ids&, bool, int shared, int add) { return Ids{shared, add}; }                  // loses A+B in sync

// Pick mode re-arm: a tiny model of the picker, the transient slot and the idle callback.
struct PickSim
{
    bool active = true, installed = false, slotOther = false, directorVisible = true, mouselook = false;
    unsigned generation = 1, queuedGeneration = 0;
    int base = 1, queuedBase = 0;
    bool queued = false;
    void forwardAlt() { queued = true; queuedGeneration = generation; queuedBase = base; installed = false; }   // camera cleared the slot
    void stop() { ++generation; active = false; installed = false; }
    void start() { ++generation; active = true; installed = true; }
};
typedef RearmAction (*RearmFn)(bool, bool, bool, bool, bool, bool, bool);
RearmAction badRearm(bool active, bool, bool, bool dv, bool ml, bool base, bool slotOther)   // the round-2 callback
{
    if (!active) return RearmAction::Ignore;
    if (!dv || ml || !base || slotOther) return RearmAction::EndMode;       // "slot busy" includes the picker itself
    return RearmAction::Install;
}
void idle(PickSim& w, RearmFn decide)
{
    if (!w.queued) return;
    w.queued = false;
    const bool same = w.queuedGeneration == w.generation;
    const RearmAction a = decide(w.active, same, w.installed, w.directorVisible, w.mouselook,
                                 w.queuedBase == w.base, w.slotOther || w.installed);
    if (a == RearmAction::Install) w.installed = true;
    else if (a == RearmAction::EndMode) w.active = false;
}
// no state in which the mode is off while its tool stays installed; never installs over a new base tool
bool rearmOk(RearmFn decide)
{
    { PickSim w; w.forwardAlt(); w.stop(); w.start(); idle(w, decide);                 // stop + start before idle
      if (!(w.active && w.installed)) return false; }
    { PickSim w; w.forwardAlt(); w.base = 2; w.slotOther = false; idle(w, decide);       // base tool replaced mid drag
      if (w.active && w.installed) return false;                                         // must not reinstall over it
      if (w.active) return false; }                                                      // and the mode ends
    { PickSim w; w.forwardAlt(); idle(w, decide);                                        // the ordinary Alt-release
      if (!(w.active && w.installed)) return false; }
    { PickSim w; w.forwardAlt(); w.directorVisible = false; idle(w, decide);
      if (w.active || w.installed) return false; }
    { PickSim w; w.forwardAlt(); w.mouselook = true; idle(w, decide);
      if (w.active || w.installed) return false; }
    { PickSim w; w.forwardAlt(); w.slotOther = true; idle(w, decide);
      if (w.active || w.installed) return false; }
    return true;
}

// Esc after a camera gesture cleared the slot (re-arm pending, picker not current): the mode must end
// and the queued re-arm must not bring it back.
typedef void (*EscFn)(PickSim&);
void escapeReal(PickSim& w) { if (w.active && !w.installed) w.stop(); else if (w.installed) w.stop(); }
void escapeBad(PickSim& w) { if (w.installed) w.stop(); }       // only a current picker sees Esc
bool escapeOk(EscFn escape)
{
    PickSim w; w.forwardAlt(); escape(w); idle(w, rearmDecision);
    if (w.active || w.installed) return false;
    PickSim cur; cur.installed = true; escape(cur);              // picker current: Esc ends it too
    return !cur.active;
}

// Animation drop: the clone a drop lands on stands for its COLLAPSED group, nothing else.
struct DropWorld { int hit; bool collapsed; Ids members; };
typedef Ids (*DropFn)(const DropWorld&);
Ids dropReal(const DropWorld& w) { return dropTargets(w.hit, w.collapsed, w.members); }
Ids dropHitOnly(const DropWorld& w) { return Ids{ w.hit }; }                                  // group drop playing on only the hit clone
Ids dropWholeGroup(const DropWorld& w) { return w.members.empty() ? Ids{ w.hit } : w.members; } // expanded member playing on the group
bool dropOk(DropFn plan)
{
    const Ids group{ 1, 2, 3 };
    if (plan({ 2, true, group }) != group) return false;                  // collapsed group: every member
    if (plan({ 2, false, Ids{} }) != Ids{ 2 }) return false;              // ungrouped: just the clone
    if (plan({ 2, false, group }) != Ids{ 2 }) return false;              // expanded group member: just the clone
    return true;
}
bool staggerVisibleOk(bool (*visible)(bool)) { return visible(true) && !visible(false); }
// Stagger scope: a group of 2 inside a crowd of 4. The operation reports what it touched.
struct StaggerWorld { Ids touched; int settingWrites = 0; int selectionChanges = 0; };
typedef void (*StaggerFn)(StaggerWorld&, const Ids& members, const Ids& crowd);
void staggerReal(StaggerWorld& w, const Ids& members, const Ids&) { w.touched = members; }
void staggerWholeCrowd(StaggerWorld& w, const Ids&, const Ids& crowd) { w.touched = crowd; }
void staggerWritesSetting(StaggerWorld& w, const Ids& members, const Ids&) { w.touched = members; ++w.settingWrites; }
void staggerChangesSelection(StaggerWorld& w, const Ids& members, const Ids&) { w.touched = members; ++w.selectionChanges; }
bool staggerScopeOk(StaggerFn op)
{
    StaggerWorld w; const Ids members{ 1, 2 }, crowd{ 1, 2, 3, 4 };
    op(w, members, crowd);
    return w.touched == members && w.settingWrites == 0 && w.selectionChanges == 0;
}
// Undo restores only the saved members that survive.
typedef Ids (*UndoFn)(const Ids& saved, const Ids& alive);
Ids undoReal(const Ids& saved, const Ids& alive)
{ Ids r; for (int id : saved) for (int a : alive) if (id == a) r.push_back(id); return r; }
Ids undoWrongMember(const Ids&, const Ids& alive) { return alive; }
bool undoOk(UndoFn undo) { return undo(Ids{ 1, 2, 3 }, Ids{ 2, 3, 9 }) == Ids{ 2, 3 }; }
bool badStaggerAlways(bool) { return true; }

// Spawn sentinel: the stock LLMenuItemGL::handleRightMouseUp rule treats any spawn point other
// than (S32_MAX, S32_MAX) as an ARMED context-menu spawn.
bool menuArmed(int sx, int sy) { return sx != 2147483647 || sy != 2147483647; }
bool sentinelOk(int sentinel)
{
    if (menuArmed(sentinel, sentinel)) return false;
    // and the holder's own +-2 px window never matches a real pixel
    for (int x = 0; x < 4096; x += 37) { long long dx = (long long)x - sentinel; if (dx >= -2 && dx <= 2) return false; }
    return true;
}

// Edit's selection rule: only the entity menu may keep a multi-selection; an overlay Edit always
// selects its own target, whatever an earlier (dismissed) clone menu left in sRows.
bool editKeeps(bool keep_flag, bool shared_in_rows) { return keep_flag && shared_in_rows; }
typedef bool (*EditKeepFn)(bool, bool);
bool badEditKeepsAlways(bool, bool shared_in_rows) { return shared_in_rows; }
bool editSelectionOk(EditKeepFn keeps)
{
    return keeps(true, true) && !keeps(true, false) && !keeps(false, true) && !keeps(false, false);
}

int main()
{
    expect(editSelectionOk(editKeeps), "Edit keeps a multi-selection only for the entity menu");
    expect(!editSelectionOk(badEditKeepsAlways), "REJECT: overlay Edit keeping a stale clone selection");
    expect(sentinelOk(2147483647), "the disarmed spawn point is the stock value");
    expect(!sentinelOk(2147483647 / 2), "REJECT: a half-S32_MAX sentinel leaves every menu item armed");
    expect(undoOk(undoReal), "Stagger undo restores only the saved members that survive");
    expect(dropOk(dropReal), "animation drop: collapsed group plays on every member, expanded member and ungrouped clone on themselves");
    expect(!dropOk(dropHitOnly), "REJECT: a drop on a collapsed group playing on only the hit clone");
    expect(!dropOk(dropWholeGroup), "REJECT: a drop on an expanded group member playing on the whole group");
    expect(staggerVisibleOk(staggerVisible), "Stagger is visible only for a collapsed group");
    expect(!staggerVisibleOk(badStaggerAlways), "REJECT: Stagger visible for a non-group target");
    expect(staggerChecked(-1, true) == 0 && staggerChecked(1, true) == 0 && staggerChecked(1, false) == 1 &&
           staggerChecked(2, false) == 2 && staggerChecked(-1, false) == -1 && staggerChecked(0, false) == -1,
           "Stagger check: all-zero is In sync, else the mode last applied to THIS group, else none");
    expect(staggerScopeOk(staggerReal), "Stagger changes only the group's members, writes no setting, touches no selection");
    expect(!staggerScopeOk(staggerWholeCrowd), "REJECT: Stagger changing a clone outside the picked group");
    expect(!staggerScopeOk(staggerWritesSetting), "REJECT: Stagger writing the global Start offset setting");
    expect(!staggerScopeOk(staggerChangesSelection), "REJECT: Stagger changing the selection");
    expect(!undoOk(undoWrongMember), "REJECT: Stagger undo acting on a clone that was not saved");
    expect(escapeOk(escapeReal), "Esc during a pending camera re-arm ends the mode and the re-arm is ignored");
    expect(!escapeOk(escapeBad), "REJECT: Esc that cannot reach a pick mode waiting for its camera re-arm");
    expect(rearmOk(rearmDecision), "pick mode re-arm: ordinary release re-arms; stop+start, base change, hidden Director, mouselook, taken slot are all safe");
    expect(!rearmOk(badRearm), "REJECT: re-arm that switches the mode off while its tool stays installed / reinstalls over a new base tool");
    expect(ctrlAddOk(realCtrlAdd), "Ctrl-add reconciles a stale list with the shared selection first");
    expect(!ctrlAddOk(badKeepsStale), "REJECT: Ctrl-add onto a stale list (A+B+D instead of C+D)");
    expect(!ctrlAddOk(badDropsShared), "REJECT: Ctrl-add that drops the shared selection");
    expect(!ctrlAddOk(badAlwaysReplace), "REJECT: Ctrl-add that replaces an up-to-date list");
    // production rules pass
    expect(boundaryIsCamerasSlop<RightClickGesture>(), "slop is LLToolCamera's accumulated 4 px per axis");
    expect(dragNeverOpens<RightClickGesture>(), "drag and return must not open");
    expect(dragAwayNeverOpens<RightClickGesture>(), "drag away must not open");
    expect(clickOpens<RightClickGesture>(), "a still click opens");
    expect(releaseWithoutPressNeverOpens<RightClickGesture>(), "release without press must not open");
    { RightClickGesture g; g.press(1, 1); g.cancel(); expect(!g.release(1, 1), "cancel ends the gesture"); }
    { RightClickGesture g; g.press(1, 1); g.release(1, 1); expect(!g.release(1, 1), "release ends the gesture"); }

    expect(actionStaysInPlan(2, Ids{}, Ids{2}), "picked alone acts on itself");
    expect(actionStaysInPlan(2, Ids{1, 2, 3}, Ids{1, 2, 3}), "picked in the multi-selection acts on all of it");
    expect(planTargets(2, Ids{1, 3}).mSelectPickedFirst, "picked outside the selection is selected first");
    expect(planTargets(2, Ids{1, 3}).mTargets == Ids{2}, "picked outside the selection acts alone");
    expect(!planTargets(2, Ids{1, 2}).mSelectPickedFirst, "picked inside the selection keeps it");
    expect(pauseAction(0, 3) && pauseAction(2, 3) && !pauseAction(3, 3) && pauseAction(0, 0), "play/pause toggle");
    expect(allOf(2, 2) && !allOf(1, 2) && !allOf(0, 0), "mirror check needs every target");

    expect(overlayRuleOk([](size_t n) { return overlaySubmenuVisible(n); }), "overlay submenu only with overlays");
    expect(chooseOverlay(Ids{4, 5, 6}, 0, -1) == 6, "topmost overlay by default");
    expect(chooseOverlay(Ids{4, 5, 6}, 5, -1) == 5, "chosen overlay wins");
    expect(chooseOverlay(Ids{4, 5, 6}, 9, -1) == 6, "a vanished choice falls back to the topmost");
    expect(chooseOverlay(Ids{}, 5, -1) == -1, "no overlay, no target");

    expect(steppedScale(1.f, 1, 0.05f, 150.f) > 1.f && steppedScale(1.f, -1, 0.05f, 150.f) < 1.f &&
           steppedScale(7.f, 0, 0.05f, 150.f) == 1.f && steppedScale(149.f, 1, 0.05f, 150.f) == 150.f &&
           steppedScale(0.06f, -1, 0.05f, 150.f) == 0.05f, "size steps clamp");

    SelectInput in;
    in.mIsClone = false;
    in.mInCast = false;
    expect(planSelect(in) == SelectAction::AddToCastThenSetActor, "non-cast avatar is added to the Cast first");
    expect(avatarSelectOk(planSelect), "avatar select: always in the Cast and the actor; a refusal changes nothing");
    in.mInCast = true;
    expect(planSelect(in) == SelectAction::SetActor, "cast avatar sets the actor");
    in.mInCast = false; in.mCastRefuses = true;
    expect(planSelect(in) == SelectAction::Refuse, "a Cast refusal is reported, not swallowed");
    for (int cast = 0; cast < 2; ++cast)
    {
        SelectInput a; a.mInCast = cast; SelectInput b = a; b.mAdditive = true;
        expect(planSelect(a) == planSelect(b), "Ctrl on an avatar is a plain click");
    }
    SelectInput clone; clone.mIsClone = true;
    expect(planSelect(clone) == SelectAction::CloneActor, "clone off the Clones page becomes the Actor bar actor");
    clone.mOnClonesPage = true;
    expect(planSelect(clone) == SelectAction::CloneListReplace, "clone on the Clones page replaces the selection");
    clone.mAdditive = true;
    expect(planSelect(clone) == SelectAction::CloneListAdd, "Ctrl-click adds a clone");
    expect(ctrlRuleOk(ctrlAddsToSet), "Ctrl adds clones only");
    expect(exitsAll<PickMode>(), "every pick mode exit ends the mode");
    { PickMode p; p.toggle(); expect(p.active(), "toggle on"); p.toggle(); expect(!p.active(), "toggle off"); }

    // planted negatives: each broken variant must be REJECTED by the oracle
    expect(!dragNeverOpens<BadGestureNoDrag>(), "REJECT: right-drag opens the menu");
    expect(!dragAwayNeverOpens<BadGestureNoDrag>(), "REJECT: right-drag away opens the menu");
    expect(!dragNeverOpens<BadGestureEndpointOnly>(), "REJECT: a drag that returns to the press point opens the menu");
    expect(!boundaryIsCamerasSlop<BadGestureEndpointOnly>(), "REJECT: slop that is not accumulated travel");
    expect(!actionStaysInPlan(2, Ids{}, badPlanAllClones(2, Ids{})), "REJECT: action on a clone outside the picked set");
    expect(!actionStaysInPlan(2, Ids{1, 2}, Ids{3}), "REJECT: action on an unselected clone");
    expect(!actionStaysInPlan(2, Ids{}, Ids{}), "REJECT: action that touched nothing");
    expect(!overlayRuleOk(badAlwaysVisible), "REJECT: overlay submenu on an avatar without overlays");
    expect(!exitsAll<BadPickIgnoresClose>(), "REJECT: pick mode left active after the Director closes");
    expect(!ctrlRuleOk(badCtrlAddsAvatar), "REJECT: Ctrl-click adding an avatar to a set");
    expect(!avatarSelectOk(badPlanNoAdd), "REJECT: avatar select that skips the Cast add");
    expect(!avatarSelectOk(badPlanSilentRefuse), "REJECT: picking a non-cast avatar leaves the Actor bar unchanged");
    expect(!avatarSelectOk(badPlanCtrlAvatar), "REJECT: Ctrl-click on an avatar behaving differently");

    if (failures) return 1;
    std::printf("PASS: clone menu logic, 28 planted negatives rejected\n");
    return 0;
}
'''


def read(path: Path) -> str:
    return path.read_bytes().decode("utf-8").replace("\r\n", "\n")


def load_sources() -> dict[str, str]:
    names = ["alghostclonemenu.cpp", "alghostclonemenu.h", "aldirectorselect.cpp", "aldirectorselect.h",
             "altooldirectorpick.cpp", "altooldirectorpick.h", "alghostclonemenulogic.h", "lltoolpie.cpp",
             "llviewerwindow.cpp", "alviewermenu.cpp", "llviewermenu.cpp", "alkitshell.cpp", "alkitshell.h",
             "altoolghostedit.cpp", "alghoststudio.h", "alpanelghoststudio.h", "CMakeLists.txt",
             "alghostcloneactions.cpp", "alghostcloneactions.h", "alpanelghoststudio.cpp",
             "lltoolmgr.cpp", "llagentcamera.cpp", "lltooldraganddrop.cpp", "alghoststudio.cpp"]
    sources = {name: read(NV / name) for name in names}
    for name in AVATAR_MENUS + ("menu_ghost_clone.xml", "panel_ghost_studio.xml", "strings.xml",
                                "floater_director_v2.xml", "director_kit_index.xml"):
        sources[name] = read(XUI / name)
    sources["director_kit_gen.py"] = read(ROOT / "scripts/code_tools/director_kit_gen.py")
    return sources


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    return re.sub(r"//[^\n]*", "", source)


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start < 0:
        return ""
    brace = source.find("{", start)
    depth = 0
    for i in range(brace, len(source)):
        depth += source[i] == "{"
        depth -= source[i] == "}"
        if depth == 0:
            return source[brace:i + 1]
    return ""


def names_in_xml(text: str):
    return re.findall(r'function="([^"]+)"', text)


def overlay_fragment(text: str) -> str:
    start = text.find("<!-- [CloneOverlay]")
    end = text.find('name="Select in Director"', start)
    return text[start:end] if start >= 0 and end > start else ""


PARITY_FIXTURE = r"""
#include "alcloneqol.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <vector>
typedef int S32; typedef unsigned U32; typedef unsigned char U8; typedef unsigned long long U64; typedef double F64; typedef float F32;
template <class T> T llclamp(T v, T lo, T hi) { return std::min(hi, std::max(lo, v)); }
template <class T> T llmin(T a, T b) { return a < b ? a : b; }
template <class T> T llmax(T a, T b) { return a > b ? a : b; }
struct LLUUID
{
    U8 mData[16];
    LLUUID() { std::fill_n(mData, 16, (U8)0); }
    bool isNull() const { for (U8 b : mData) if (b) return false; return true; }
    bool notNull() const { return !isNull(); }
    bool operator==(const LLUUID& o) const { return std::equal(mData, mData + 16, o.mData); }
    bool operator!=(const LLUUID& o) const { return !(*this == o); }
};
struct Settings
{
    S32 mode = 0, direction = 0; F32 spread = 0.f;
    S32 getS32(const std::string& n) const { return n == "GhostStudioStartMode" ? mode : n == "GhostStudioRippleFrom" ? direction : 0; }
    F32 getF32(const std::string& n) const { return n == "GhostStudioStartSpread" ? spread : 0.f; }
} gSavedSettings;
struct ALGhostStudio
{
    enum { BACKING_OVERLAY, BACKING_ENTITY_CLONE };
    struct Instance
    {
        LLUUID mId, mCrowdId; bool mDeleting = false; int mKind = BACKING_ENTITY_CLONE; U32 mCrowdSeed = 0;
        F64 mCrowdLocalX = 0., mCrowdLocalY = 0.; F32 mStartOffset = 0.f;
        bool mCrowdMixed = false; S32 mCrowdSingleStyle = 0, mStyle = 0; U32 mCrowdRow = 0;
    };
    std::vector<Instance> mInstances; U64 mInstanceRevision = 0;
    static ALGhostStudio& instance() { static ALGhostStudio s; return s; }
    static S32 remapRetiredOverlayStyle(S32 s) { return s; }
    static bool isOverlayStyle(S32) { return true; }
    static S32 resolveOverlayStyle(S32 s) { return s; }
    Instance* getInstance(const LLUUID& id) { for (auto& i : mInstances) if (i.mId == id) return &i; return nullptr; }
    bool setStartOffset(const LLUUID& id, F32 seconds) { Instance* i = getInstance(id); if (!i) return false; i->mStartOffset = llclamp(seconds, 0.f, 10.f); return true; }
    void touchInstances() { ++mInstanceRevision; }
    void applyCrowdVariation(const LLUUID& member_id, U32 seed, bool timing, bool assign_looks, bool building);
};
@ORIGINAL@
namespace ALGhostCloneActions
{
@MINE@
}
static LLUUID id_of(int crowd, int index) { LLUUID u; u.mData[0] = (U8)(index + 1); u.mData[1] = (U8)(crowd * 17 + 3); u.mData[7] = (U8)(index * 29 + crowd); return u; }
int main()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    int checked = 0;
    for (S32 mode = 0; mode < 3; ++mode)
    for (U32 seed : { 0u, 1u, 12345u, 0xFFFFFFFFu })
    for (F32 spread : { 0.f, 0.5f, 0.8f, 3.3f, 10.f, 14.f })
    for (S32 direction = 0; direction < 3; ++direction)
    for (int count : { 1, 2, 6 })
    {
        studio.mInstances.clear();
        std::vector<LLUUID> ids;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < (c == 0 ? count : 3); ++i)
            {
                ALGhostStudio::Instance inst; inst.mId = id_of(c, i); inst.mCrowdId = id_of(c + 50, 0);
                inst.mCrowdLocalX = (i * 37 % 11) * 0.9 - 2.; inst.mCrowdLocalY = (i * 53 % 7) * 1.3 - 1.5 + c;
                inst.mStartOffset = 0.25f; inst.mCrowdSeed = 77u;
                studio.mInstances.push_back(inst);
                if (c == 0) ids.push_back(inst.mId);
            }
        gSavedSettings.mode = mode; gSavedSettings.direction = direction; gSavedSettings.spread = spread;
        studio.applyCrowdVariation(ids.front(), seed, true, false, false);                  // the Crowd tab, whole crowd
        std::vector<F32> original;
        for (const auto& id : ids) original.push_back(studio.getInstance(id)->mStartOffset);
        for (auto& inst : studio.mInstances) inst.mStartOffset = 0.25f;
        if (!ALGhostCloneActions::applyStartOffsets(ids, mode, seed)) { std::printf("FAIL: refused\n"); return 1; }
        for (size_t i = 0; i < ids.size(); ++i)
            if (studio.getInstance(ids[i])->mStartOffset != original[i])
            { std::printf("FAIL: offsets differ (mode %d seed %u spread %g dir %d count %d member %zu: %g vs %g)\n", mode, seed, spread, direction, count, i, original[i], studio.getInstance(ids[i])->mStartOffset); return 1; }
        for (const auto& inst : studio.mInstances)                                          // the other crowd is untouched by the explicit form
            if (inst.mCrowdId == id_of(51, 0) && inst.mStartOffset != 0.25f) { std::printf("FAIL: touched another crowd\n"); return 1; }
        ++checked;
    }
    std::printf("PASS: %d layouts\n", checked);
    return 0;
}
"""


def definition(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        depth += source[i] == "{"
        depth -= source[i] == "}"
        if depth == 0:
            return source[start:i + 1]
    return ""


_PARITY_CACHE: dict = {}


def parity_findings(src: dict[str, str]) -> list[str]:
    """Compile BOTH the Crowd tab's applyCrowdVariation (extracted from alghoststudio.cpp) and the
    menu's applyStartOffsets and require identical start offsets over a grid of layouts, modes,
    seeds, spreads and ripple directions."""
    try:
        original = definition(src["alghoststudio.cpp"], "void ALGhostStudio::applyCrowdVariation(")
        mine = definition(src["alghostcloneactions.cpp"], "bool applyStartOffsets(")
    except ValueError:
        return ["applyCrowdVariation / applyStartOffsets not found for the parity fixture"]
    key = (original, mine)
    if key not in _PARITY_CACHE:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from native_check_tools import check_directory, compile_run
        source = PARITY_FIXTURE.replace("@ORIGINAL@", original).replace("@MINE@", mine)
        try:
            result = compile_run(check_directory("clone_menu_parity"), source, "parity", flags=['/I"%s"' % NV])
            ok = result.returncode == 0 and "PASS" in result.stdout
            _PARITY_CACHE[key] = [] if ok else ["Stagger math differs from the Crowd tab's: " + (result.stdout + result.stderr).strip()[:300]]
        except RuntimeError as exc:
            _PARITY_CACHE[key] = ["Stagger parity fixture did not compile: " + str(exc).strip().splitlines()[-1][:200]]
    return _PARITY_CACHE[key]


def findings(src: dict[str, str]) -> list[str]:
    out: list[str] = []
    out += parity_findings(src)
    cpp = strip_comments(src["alghostclonemenu.cpp"])

    # ---- every XUI callback is registered ----------------------------------------
    used = set(names_in_xml(src["menu_ghost_clone.xml"]))
    for name in AVATAR_MENUS:
        used |= {f for f in names_in_xml(src[name]) if f.startswith(("GhostOverlay.", "Avatar.SelectInDirector",
                                                                    "Avatar.EnableSelectInDirector"))}
    registered = set(re.findall(r'(?:enable|commit)\.add\("([^"]+)"', cpp))
    for function in sorted(used - registered):
        out.append(f"XUI callback {function} is not registered in alghostclonemenu.cpp")

    # ---- entity menu order --------------------------------------------------------
    try:
        root = ET.fromstring(src["menu_ghost_clone.xml"])
        labels = [c.get("label") if c.tag != "menu_item_separator" else "-" for c in root]
        want = ["Select in Director", "-", "Pause", "Play", "Mirror", "Size", "Stagger", "Edit", "-", "Freeze pose",
                "Clear animations", "Solo", "Duplicate", "Delete", "-", "Open in Director"]
        if labels != want:
            out.append(f"menu_ghost_clone.xml item order is {labels}")
        size = [c.get("label") for c in root.find("context_menu[@name='Size']")]
        if size != ["Smaller", "Larger", "Reset size"]:
            out.append("menu_ghost_clone.xml Size submenu is not Smaller / Larger / Reset size")
    except ET.ParseError as exc:
        out.append(f"menu_ghost_clone.xml does not parse: {exc}")

    # ---- the four avatar menus ----------------------------------------------------
    fragments = {}
    for name in AVATAR_MENUS:
        try:
            menu = ET.fromstring(src[name])
        except ET.ParseError as exc:
            out.append(f"{name} does not parse: {exc}")
            continue
        kids = list(menu)
        if len(kids) < 4 or kids[0].get("name") != "Clone Overlay" or \
                kids[1].get("name") != "Select in Director" or kids[2].tag != "menu_item_separator":
            out.append(f"{name}: top must be Clone overlay, Select in Director, separator")
        sub = [c.get("name") for c in kids[0]]
        want = ["Overlay Select in Director", "Overlay Style", "Overlay Size", "Overlay Mirror", "Overlay Edit",
                "Which Overlay", None, "Overlay Remove", "Overlay Open in Director"]
        if sub != want:
            out.append(f"{name}: Clone overlay submenu order is {sub}")
        fragments[name] = overlay_fragment(src[name])
        # no stray Select item left in the old Director submenu
        director = [c for c in menu.iter("context_menu") if c.get("name") == "Director"]
        for d in director:
            if any(c.get("name") == "Select in Director" for c in d):
                out.append(f"{name}: Select in Director must not live in the Director submenu")
    if len(set(fragments.values())) > 1:
        out.append("the Clone overlay fragment differs between the four avatar menus")
    if fragments and not next(iter(fragments.values())):
        out.append("Clone overlay fragment marker missing")

    # ---- Style items == the Clones page style combo --------------------------------
    combo = src["panel_ghost_studio.xml"].split('name="style_combo"', 1)[1].split("</combo_box>", 1)[0]
    page = sorted((m.group(1), int(m.group(2))) for m in
                  re.finditer(r'<combo_box\.item label="([^"]+)"[^>]*value="(\d+)"', combo))
    header = src["alghoststudio.h"]
    if "(style >= 0 && style <= 27)" not in header or \
            "(style >= 64 && style <= 100 && style != 68 && style != 72)" not in header:
        out.append("isOverlayStyle ranges changed: update the overlay Style menu and this check")
    legal = set(range(0, 28)) | (set(range(64, 101)) - {68, 72})
    if {v for _, v in page} != legal:
        out.append("Clones page style combo no longer matches isOverlayStyle")
    for name in AVATAR_MENUS:
        try:
            menu = ET.fromstring(src[name])
        except ET.ParseError:
            continue
        style = next((c for c in menu.find("context_menu[@name='Clone Overlay']") if c.get("name") == "Overlay Style"), None)
        if style is None:
            out.append(f"{name}: Style submenu missing")
            continue
        items = []
        for check in style.iter("menu_item_check"):
            params = {e.tag.split(".")[-1]: e.get("parameter") for e in check}
            if params.get("on_click") != params.get("on_check"):
                out.append(f"{name}: Style item {check.get('label')} click/check parameters differ")
            items.append((check.get("label"), int(params["on_click"])))
        if sorted(items) != page:
            out.append(f"{name}: Style items differ from the Clones page style combo")

    # ---- call sites -----------------------------------------------------------------
    pie = src["lltoolpie.cpp"]
    for menu_name in ("gMenuAvatarSelf", "gMenuAvatarOther", "gMenuAttachmentSelf", "gMenuAttachmentOther"):
        if not re.search(r"ALGhostCloneMenu::prepareAvatarMenu\(" + menu_name + r",[^;]*\);\s*" + menu_name + r"->show\(x, y\);", pie):
            out.append(f"lltoolpie.cpp: {menu_name}->show is not preceded by prepareAvatarMenu")
    body = function_body(pie, "bool LLToolPie::handleRightMouseDown")
    if not re.search(r"if \(ALGhostCloneMenu::pressRight\(x, y, mPick\)\)\s*\{\s*return false;\s*\}\s*handleRightClickPick\(\);", body):
        out.append("lltoolpie.cpp: a clone hit must skip the world menu and return false (no capture)")
    win = src["llviewerwindow.cpp"]
    if "ALGhostCloneMenu::cancelRight();" not in function_body(win, "bool LLViewerWindow::handleRightMouseDown"):
        out.append("llviewerwindow.cpp: right press must cancel a stale gesture")
    up = function_body(win, "bool LLViewerWindow::handleRightMouseUp")
    if not re.search(r"gViewerInput\.handleMouse\([^;]*CLICK_RIGHT, down\);.*ALGhostCloneMenu::releaseRight\(", up, re.S):
        out.append("llviewerwindow.cpp: the clone menu must open AFTER the release is dispatched")
    if "ALGhostCloneMenu::noteMouseMove(x, y);" not in function_body(win, "void LLViewerWindow::handleMouseMove"):
        out.append("llviewerwindow.cpp: mouse moves must reach the right-click gesture (drag detection)")
    edit = function_body(src["altoolghostedit.cpp"], "bool ALToolGhostEdit::handleRightMouseDown")
    if "return false;" not in edit or "return true;" in edit:
        out.append("altoolghostedit.cpp: right-drag must stay camera control (handleRightMouseDown returns false)")
    if "ALGhostCloneMenu::registerCallbacks();" not in src["alviewermenu.cpp"]:
        out.append("alviewermenu.cpp: callbacks are not registered before the menus load")
    if "ALGhostCloneMenu::createMenu();" not in src["llviewermenu.cpp"] or \
            "ALGhostCloneMenu::destroyMenu();" not in src["llviewermenu.cpp"]:
        out.append("llviewermenu.cpp: clone menu is not created / destroyed")

    # ---- the menu acts only on the planned set ------------------------------------
    show = function_body(cpp, "void showEntityMenu")
    if "const bool unit_is_group = Actions::collapsedGroup(clone) != nullptr;" not in show or \
            "stagger->setVisible(Logic::staggerVisible(unit_is_group));" not in show or \
            'sMenu->getItem(std::string("Stagger"))' not in show:
        out.append("showEntityMenu: Stagger is shown only when the picked clone's unit is a collapsed group")
    try:
        stagger = ET.fromstring(src["menu_ghost_clone.xml"]).find("context_menu[@name='Stagger']")
        names = [c.get("label") for c in stagger] if stagger is not None else None
        if names != ["In sync", "Random", "Ripple"]:
            out.append(f"menu_ghost_clone.xml Stagger submenu is {names}")
        else:
            values = [[e.get("parameter") for e in c if e.tag.endswith("on_click")][0] for c in stagger]
            if values != ["0", "1", "2"]:
                out.append("menu_ghost_clone.xml Stagger items must pass 0 / 1 / 2 (In sync / Random / Ripple)")
    except ET.ParseError:
        pass
    stagger_cb = function_body(cpp, "void actStagger")
    if "if (before.mOffsets == after.mOffsets)" not in stagger_cb or \
            stagger_cb.find("before.mOffsets == after.mOffsets") > stagger_cb.find("recordCustom"):
        out.append("Stagger: a no-op (nothing changed) must not record an undo step")
    size_fn = function_body(cpp, "void registerOverlayCallbacks")
    if "Actions::collapsedGroup(sOverlayTarget)" not in size_fn or "GhostCloneMenuOverlayInGroup" not in size_fn or \
            "if (live(target) && !Actions::collapsedGroup(target))" not in size_fn:
        out.append("overlay Size must not scale a whole collapsed group: disabled with a reason, and refused at commit")
    for name in AVATAR_MENUS:
        if src[name].count('function="GhostOverlay.EnableSize"') != 3:
            out.append(f"{name}: the overlay Size items need the group-aware enable check")
    if "Actions::collapsedGroup(target) ? \"GhostCloneMenuOverlayInGroup\"" not in size_fn:
        out.append("overlay Remove must explain a grouped overlay with the right text")
    edit_fn = function_body(cpp, "void toggleEdit")
    if "!keep_multi_selection || shared_row.isNull() || std::find(sRows.begin(), sRows.end(), shared_row) == sRows.end()" not in edit_fn or \
            "toggleEdit(target, /*keep_multi_selection=*/true)" not in src["alghostclonemenu.cpp"] or \
            "toggleEdit(target, /*keep_multi_selection=*/false)" not in src["alghostclonemenu.cpp"]:
        out.append("menu Edit must not collapse a multi-selection that already holds the shared selection row")
    if "sMenuHud ? str(\"DirectorSelectHud\")" not in function_body(cpp, "void registerAvatarCallbacks") or \
            "object->isHUDAttachment());" not in src["lltoolpie.cpp"] or "bool hud_attachment = false" not in src["alghostclonemenu.h"]:
        out.append("Select in Director is disabled on your own HUD attachment menu")
    delete_fn = function_body(cpp, "void actDelete")
    if "live(row)" not in delete_fn or "Actions::deleteRows(live_rows, &skipped)" not in delete_fn:
        out.append("Delete must only act on rows that are still alive (a vanished clone must not clear the selection)")
    for needle in ("Actions::staggerRefusal(sPicked)", "Actions::setStagger(sPicked, mode)", "Actions::restoreStagger(redo ? after : before)",
                   'recordCustom(shell, "clone_stagger"'):
        if needle not in stagger_cb:
            out.append(f"Stagger must go through the shared action and record one undo step: missing {needle}")
    if stagger_cb.count("Actions::snapshotStagger(sPicked)") != 2:
        out.append("Stagger must snapshot before and after for undo / redo")
    if not (stagger_cb.find("Actions::staggerRefusal(sPicked)") < stagger_cb.find("Actions::setStagger(sPicked, mode)") <
            stagger_cb.find("LLFloaterReg::getInstance")):
        out.append("Stagger: refuse first and look up the Director shell only after a successful apply")
    for forbidden in ("setStartOffset", "applyCrowdVariation", "applyStartOffsets", "syncOffsets", "GhostStudioStartMode", "setSelected"):
        if forbidden in stagger_cb:
            out.append(f"actStagger must not call {forbidden} itself")
    for forbidden in ("setStartOffset", "applyCrowdVariation", "syncOffsets", "GhostStudioStartMode"):
        if forbidden in cpp:
            out.append(f"alghostclonemenu.cpp: {forbidden} belongs to the shared Crowd tab action, not the menu")
    actions_cpp = strip_comments(src["alghostcloneactions.cpp"])
    set_stagger = function_body(actions_cpp, "bool setStagger")
    if "!applyStartOffsets(members, mode, seed)" not in set_stagger:
        out.append("setStagger must apply the Crowd tab's Start offset math to exactly the group's members (applyStartOffsets)")
    for name in ("bool setStagger", "bool restoreStagger", "StaggerState snapshotStagger", "S32 staggerCurrent", "const char* staggerRefusal"):
        body = function_body(actions_cpp, name)
        for forbidden in ("gSavedSettings", "setSelected", "applyCrowdVariation", "syncOffsets", "syncRows"):
            if forbidden in body:
                out.append(f"{name}: must not use {forbidden} (no setting write, no selection change, no crowd-wide apply)")
    cur_fn = function_body(actions_cpp, "S32 staggerCurrent")
    if "found->second.mOffsets == offsets" not in cur_fn or "pruneStaggerModes();" not in cur_fn:
        out.append("the Stagger check mark counts only while the members still hold the offsets it applied; dissolved groups are pruned")
    if "pruneStaggerModes();" not in function_body(actions_cpp, "StaggerState snapshotStagger") or \
            "found->second.mOffsets == state.mOffsets" not in function_body(actions_cpp, "StaggerState snapshotStagger"):
        out.append("snapshotStagger must record the mode only while it still matches the offsets")
    refusal_fn = function_body(actions_cpp, "const char* staggerRefusal")
    if "inst->mCrowdId.isNull() || (crowd.notNull() && inst->mCrowdId != crowd)" not in refusal_fn or \
            "collapsedGroup(instance_id)" not in refusal_fn:
        out.append("staggerRefusal: only a collapsed group whose live members share one crowd")
    restore_fn = function_body(actions_cpp, "bool restoreStagger")
    if restore_fn.find("if (alive.empty())") < 0 or restore_fn.find("if (alive.empty())") > restore_fn.find("setStartOffset") or \
            "restartInstanceAnimation(offset.first)" not in restore_fn:
        out.append("restoreStagger: validate that a saved member survives before mutating; restart each restored animation")
    studio_src = src["alghoststudio.cpp"]
    if "startOffsetUnit" in studio_src or "stableCloneKey" in studio_src:
        out.append("alghoststudio.cpp is pinned and extracted by fixtures: applyCrowdVariation must stay as it was")

    def norm(text):
        return " ".join(text.replace("inst.", "inst->").split())
    crowd_fn = function_body(studio_src, "void ALGhostStudio::applyCrowdVariation")
    mine_fn = function_body(actions_cpp, "bool applyStartOffsets")
    unit_re = r"const F64 unit = mode == 1 \?.*?: 0\.;"
    stable_re = r"U64 stable = 14695981039346656037ULL;\s*for \(U8 byte : inst(?:\.|->)mId\.mData\) stable = \(stable \^ byte\)\*1099511628211ULL;"
    low_re = r"const F64 v = ALCloneQol::ripple\(inst(?:\.|->)mCrowdLocalX, inst(?:\.|->)mCrowdLocalY, direction\);\s*low = llmin\(low, v\); high = llmax\(high, v\);"
    for label, rx in (("unit", unit_re), ("stable key", stable_re), ("ripple range", low_re)):
        a_m = re.search(rx, crowd_fn, re.S)
        b_m = re.search(rx, mine_fn, re.S)
        if not a_m or not b_m or norm(a_m.group(0)) != norm(b_m.group(0)):
            out.append(f"applyStartOffsets must use the Crowd tab's {label} expression, text for text")
    if 'llclamp(gSavedSettings.getF32("GhostStudioStartSpread"), 0.f, 10.f)' not in mine_fn or \
            'gSavedSettings.getS32("GhostStudioRippleFrom")' not in mine_fn:
        out.append("applyStartOffsets must use the Crowd tab's current Spread and Ripple from")
    if "gSavedSettings.set" in mine_fn or "setSelected" in mine_fn or "applyCrowdVariation" in mine_fn:
        out.append("applyStartOffsets must not write a setting or touch the selection")
    if "ALGhostCloneActions::syncRows(selectedRowValues())" not in strip_comments(src["alpanelghoststudio.cpp"]):
        out.append("alpanelghoststudio.cpp: the Sync button must call the shared syncRows")
    if "return group && !group->mEditMembers ? group : nullptr;" not in function_body(actions_cpp, "const ALGhostGroupModel::Group* collapsedGroup"):
        out.append("collapsedGroup: only a COLLAPSED group stands for its members")
    plan_fn = function_body(actions_cpp, "AnimationDropPlan planAnimationDrop")
    if "ALGhostCloneMenuLogic::dropTargets(hit_instance, group != nullptr, members)" not in plan_fn or \
            "instances(std::vector<LLUUID>{ group->mId })" not in plan_fn:
        out.append("planAnimationDrop must expand a collapsed group through the page's unit planner and dropTargets")
    panel_play = function_body(strip_comments(src["alpanelghoststudio.cpp"]), "S32 ALPanelGhostStudio::playAnimationOnClonesRecorded")
    if "if (!shell && !host && director_fallback && played > 0)" not in panel_play or 'LLFloaterReg::getInstance("director")' not in panel_play:
        out.append("the world drop must always record its undo step: Director shell fallback in playAnimationOnClonesRecorded")
    if "studio.resolveEntityClone(id)" not in plan_fn:
        out.append("planAnimationDrop: every target needs a live clone body (hover and toast counts match what plays)")
    dnd = strip_comments(src["lltooldraganddrop.cpp"])
    drop_fn = function_body(dnd, "EAcceptance LLToolDragAndDrop::dad3dPlayOnGhostClone")
    if "ALGhostCloneActions::planAnimationDrop(mGhostCloneTarget)" not in drop_fn or \
            "playAnimationOnClonesRecorded(\n        drop_plan.mTargets" not in drop_fn.replace("                                                          ", "") .replace("\r", "") and \
            "drop_plan.mTargets, real->getAssetUUID()" not in drop_fn:
        out.append("lltooldraganddrop.cpp: the drop must play on the planned targets (the whole collapsed group), one undo step")
    if "/*director_fallback=*/true" not in src["lltooldraganddrop.cpp"] or "push_back(mGhostCloneTarget)" in drop_fn or \
            "ACCEPT_NO_CUSTOM" not in drop_fn.split("drop_plan.mTargets.empty()")[1][:200]:
        out.append("lltooldraganddrop.cpp: the drop asks for the undo fallback and refuses an empty plan (never the hit clone)")
    if "{ mGhostCloneTarget }" in drop_fn:
        out.append("lltooldraganddrop.cpp: the drop must not play on only the hit clone")
    if "std::to_string(drop_plan.mTargets.size())" not in drop_fn or "drop_plan.mGroupName" not in drop_fn or \
            "playingToast(real->getName(), (size_t)played)" not in drop_fn or \
            "played == 1 && drop_plan.mTargets.size() == 1" not in drop_fn:
        out.append("lltooldraganddrop.cpp: the hover and the toast name the group and the count")
    if "Logic::planTargets(clone, selection)" not in show:
        out.append("showEntityMenu does not use planTargets")
    if "sTargets" in cpp:
        out.append("sTargets is gone: actions consume sRows, so only sRows may carry the plan")
    if len(re.findall(r"\bsRows\s*=", cpp)) != 2:
        out.append("sRows may be assigned only by showEntityMenu's two plan branches (the rows actions consume)")
    if not re.search(r"if \(plan\.mSelectPickedFirst\)\s*\{[^{}]*sRows = std::vector<LLUUID>\{ Actions::rowFor\(clone\) \};"
                     r"[^{}]*studio\.setSelected\(sRows\.front\(\)\);\s*\}\s*else\s*\{[^{}]*sRows = host_rows;[^{}]*\}\s*sPicked = clone;", show):
        out.append("showEntityMenu: sRows must come from the plan (picked unit when not selected, else the page's rows) before sPicked is set")
    if "{ sPicked }" in cpp or "{sPicked}" in cpp:
        out.append("actions must not be pointed at the previous menu's clone")
    if "ALDirectorSelect::hostRows()" not in show or "Actions::instances(host_rows)" not in show:
        out.append("showEntityMenu does not read the Clones list rows")
    if not re.search(r"sRows = std::vector<LLUUID>\{ Actions::rowFor\(clone\) \};\s*studio\.setSelected\(sRows\.front\(\)\);", show) \
            or "sRows = host_rows;" not in show:
        out.append("showEntityMenu: rows are the page's rows, or the picked unit selected first")
    if "sMenu->show(x, y);" not in show:
        out.append("showEntityMenu never shows the menu")
    if "constexpr S32 SPAWN_DISARMED = S32_MAX;" not in cpp or "S32_MAX / 2" in cpp or "S32_MAX, S32_MAX" in cpp:
        out.append("the spawn point is disarmed with exactly S32_MAX, the stock value (any other value arms every menu item's right-release)")
    if "sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);" not in function_body(cpp, "bool pressRight"):
        out.append("pressRight must disarm an earlier menu's spawn point once a clone is hit")
    if "LLMenuHolderGL::sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);" not in show.split("sMenu->show(x, y);")[-1]:
        out.append("showEntityMenu must disarm the holder's spawn point after showing (the next right-click would be swallowed)")
    camera_src = src["llagentcamera.cpp"]
    for mode in ("MOUSELOOK", "FOLLOW", "THIRD_PERSON", "CUSTOMIZE_AVATAR"):
        if not re.search(r"mCameraMode = CAMERA_MODE_%s;\s*ALGhostCloneMenu::cancelRight\(\);" % mode, camera_src):
            out.append(f"llagentcamera.cpp: setting the camera mode to {mode} must cancel a pending clone right-click")
    if "checkInterruption" in cpp or "checkInterruption" in src["alghostclonemenu.h"] or "checkInterruption" in camera_src:
        out.append("the sampled camera check is gone: every camera-mode change cancels in the setter path")
    tool_mgr = function_body(src["lltoolmgr.cpp"], "LLTool* LLToolMgr::getCurrentTool")
    if not re.search(r"if \(prev_tool != cur_tool\)\s*\{\s*ALGhostCloneMenu::cancelRight\(\);", tool_mgr):
        out.append("lltoolmgr.cpp: a tool change must cancel the pending right-click at the transition")
    press_body = function_body(cpp, "bool pressRight")
    if press_body.find("getCurrentTool()") > press_body.find("sGesture.press(x, y);") >= 0:
        out.append("pressRight must read the tool before arming the gesture (a tool change cancels it)")
    dsel = strip_comments(src["aldirectorselect.cpp"])
    rows_fn = function_body(dsel, "std::vector<LLUUID> ALGhostCloneMenuAccess::hostRows")
    if "hidden->mObservedSelectionRevision == ALGhostStudio::instance().getSelectionRevision()" not in rows_fn or \
            "ensureBuilt" in rows_fn or "findHostBelow(lazy)" not in rows_fn:
        out.append("hostRows: with no visible host read the already-built Director host (never build it), only when it mirrors the shared selection")
    sel_hosts = function_body(strip_comments(src["aldirectorselect.cpp"]), "void ALGhostCloneMenuAccess::selectInHosts")
    if not re.search(r"std::find\(now_selected\.begin\(\), now_selected\.end\(\), row\) == now_selected\.end\(\)\)\s*\{[^{}]*studio\.setSelected\(row\);\s*return;\s*\}\s*host->mPrimaryRow = row;", sel_hosts):
        out.append("selectInHosts: when the row did not get selected, only the shared selection changes (no mirror bookkeeping)")
    for needle in ("host->refreshList(true);", "host->mObservedSelectionRevision != studio.getSelectionRevision()",
                   "ALGhostCloneMenuLogic::additiveRows(", "host->mList->deselectAllItems(true);",
                   "ALGhostCloneActions::rowFor(shared)"):
        if needle not in sel_hosts:
            out.append(f"hidden-host Ctrl-add must bring the list up to date and reconcile with the shared selection: missing {needle}")
    if not re.search(r"if \(shared_moved\)\s*\{\s*host->mList->deselectAllItems\(true\);\s*host->mList->selectMultiple\(rows\);", sel_hosts):
        out.append("hidden-host Ctrl-add must replace the list with the reconciled rows when the shared selection moved")
    if not re.search(r"~ScanGuard\(\) \{ mFlag = mPrevious; \}.*?scan_guard\(host->mSuppressLibraryScan\);\s*host->refreshList\(true\);", sel_hosts, re.S):
        out.append("the hidden-host refresh must not request the animation-library inventory scan")
    if "!studio.animLibraryScanned() && !mSuppressLibraryScan) studio.requestAnimLibraryScan();" not in " ".join(strip_comments(src["alpanelghoststudio.cpp"]).split()):
        out.append("alpanelghoststudio.cpp: the row build must honour mSuppressLibraryScan")
    if not re.search(r"if \(host->isInVisibleChain\(\)\)\s*\{\s*host->noteActiveHost\(\);", sel_hosts):
        out.append("a hidden host must not be noted as the active host")
    window_src = src["llviewerwindow.cpp"]
    if not re.search(r"key == KEY_ESCAPE && mask == MASK_NONE && ALToolDirectorPick::handlePendingEscape\(\)\)\s*\{[^{}]*return true;\s*\}\s*"
                     r"if\( LLToolMgr::getInstance\(\)->getCurrentTool\(\)->handleKey\(key, mask\) \)", window_src):
        out.append("llviewerwindow.cpp: Esc must reach pick mode before the current tool (a pending camera re-arm)")
    pending_esc = function_body(strip_comments(src["altooldirectorpick.cpp"]), "bool ALToolDirectorPick::handlePendingEscape")
    if "getCurrentTool() == getInstance()" not in pending_esc or "getInstance()->stop();" not in pending_esc:
        out.append("pick mode: pending-re-arm Esc must call stop() (a new generation) unless the picker is current")
    if "bool handleDoubleClick" not in src["altooldirectorpick.h"] or \
            "return handleMouseDown(x, y, mask);" not in function_body(strip_comments(src["altooldirectorpick.cpp"]), "bool ALToolDirectorPick::handleDoubleClick"):
        out.append("pick mode: a double-click is consumed like a click (no walk-to / teleport)")
    if sel_hosts.find("additiveRows(") < sel_hosts.find("host->refreshList(true);") or "isInVisibleChain()" not in sel_hosts:
        out.append("hidden-host Ctrl-add must refresh the hidden list before reconciling")
    for fn, marker in (("bool selectAvatar", "cast.add(avatar_id);"),):
        body = function_body(strip_comments(src["aldirectorselect.cpp"]), fn)
        if body.find("raiseDirector();") < body.find(marker) or marker not in body:
            out.append("selectAvatar must not raise the Director before the Cast accepted the avatar")
    refusals = (function_body(cpp, "void registerEntityCallbacks"), function_body(cpp, "void registerOverlayCallbacks"),
                function_body(cpp, "void registerAvatarCallbacks"))
    if "cloneRefusal(sPicked)" not in refusals[0] or "sEntitySelectReason" in cpp:
        out.append("Select / Open in Director enable checks must revalidate the picked clone on every check")
    if "cloneRefusal(sOverlayTarget)" not in function_body(cpp, "std::string overlayRefusal") or \
            "live(sOverlayTarget)" not in function_body(cpp, "std::string overlayRefusal"):
        out.append("overlay enable checks must revalidate the overlay target on every check")
    if 'enable.add("GhostOverlay.Enable", [](LLUICtrl* ctrl, const LLSD&)\n    {\n        return state(ctrl, live(sOverlayTarget) != nullptr, "GhostCloneMenuNotReady");' not in refusals[1] or \
            'return state(ctrl, live(sOverlayTarget) != nullptr && !reason' not in refusals[1] or "sOverlaySelectReason" in cpp:
        out.append("overlay Enable / EnableEdit must test that the target is still alive")
    if refusals[1].count("refreshOverlayState();") < 2 or "refreshOverlayState();" not in function_body(cpp, "void populateWhichMenu"):
        out.append("changing the overlay or its look must refresh the overlay state")
    if "avatarRefusal(sMenuAvatar)" not in refusals[2]:
        out.append("Select in Director on an avatar menu must revalidate the avatar on every check")
    release = function_body(cpp, "void releaseRight")
    if not re.search(r"const bool open = sGesture\.release\(x, y\) && gestureContextValid\(\);", release) or \
            not re.search(r"if \(open\)\s*\{\s*showEntityMenu\(clone, down_x, down_y\);\s*\}", release):
        out.append("releaseRight must open the menu only for a click whose context is still valid")
    context = function_body(cpp, "bool gestureContextValid")
    for needle in ("getCurrentTool() == sPressTool", "getCameraMode() == sPressCameraMode",
                   "getCameraMode() != CAMERA_MODE_MOUSELOOK", "LLModalDialog::activeCount() == 0",
                   "!gFocusMgr.getTopCtrl()", "gFocusMgr.getAppHasFocus()", "!gFocusMgr.getMouseCapture()"):
        if needle not in context:
            out.append(f"gestureContextValid must check {needle}")
    if "gFocusMgr.getMouseCapture()" not in function_body(cpp, "void noteMouseMove") or \
            "cancelRight();" not in function_body(cpp, "void noteMouseMove"):
        out.append("noteMouseMove must cancel when a tool grabs the mouse (camera drag)")
    press = function_body(cpp, "bool pressRight")
    if "LLTool* press_tool = LLToolMgr::getInstance()->getCurrentTool();" not in press or "sPressTool = press_tool;" not in press or \
            "sPressCameraMode = gAgentCamera.getCameraMode();" not in press:
        out.append("pressRight must remember the tool and camera mode it started in")
    if "ALGhostCloneMenu::cancelRight();" not in function_body(src["llviewerwindow.cpp"], "void LLViewerWindow::handleFocusLost"):
        out.append("llviewerwindow.cpp: focus loss must cancel a pending clone right-click")
    if "base->second.mItem.get() != ctrl" not in function_body(cpp, "void setReason"):
        out.append("setReason must not trust a widget address alone (rebuilt items reuse addresses)")
    if "reason.empty() ? base->second.mTip : reason" not in function_body(cpp, "void setReason"):
        out.append("setReason must restore the XML tooltip when the item is enabled")
    if not re.search(r"on_click function=\"GhostClone\.OpenInDirector\" />\s*<menu_item_call\.on_enable", src["menu_ghost_clone.xml"]):
        out.append("menu_ghost_clone.xml: Open in Director needs an enable check")
    edit_commit = function_body(cpp, "void registerEntityCallbacks")
    if "live(sPicked) ? sPicked : LLUUID::null" not in edit_commit or "alive.front()" not in edit_commit:
        out.append("Edit must act on a live target when the picked clone is gone")
    if "submenu->arrange();" not in function_body(cpp, "void prepareAvatarMenu"):
        out.append("prepareAvatarMenu must re-arrange the submenu after toggling Which overlay")
    window = src["llviewerwindow.cpp"]
    if len(re.findall(r"if \(!down && clicktype == CLICK_RIGHT\) ALGhostCloneMenu::cancelRight\(\);", window)) != 2:
        out.append("llviewerwindow.cpp: a UI view or captor that handled the right release must end the gesture")
    prepare_body = function_body(cpp, "void prepareAvatarMenu")
    visible_calls = re.findall(r"(\w+)->setVisible\(([^;]*)\);", prepare_body)
    if visible_calls != [("branch", "visible"), ("which_item", "sOverlays.size() > 1")]:
        out.append(f"prepareAvatarMenu may only show the branch by overlaySubmenuVisible and Which overlay by count: {visible_calls}")
    if "!inst.mSourceRemoved" not in prepare_body:
        out.append("prepareAvatarMenu must skip overlays whose source was removed (orphans sit on no avatar)")
    if "prepareAvatarMenu(menu, nullptr);" not in function_body(cpp, "void createMenu"):
        out.append("createMenu must start the Clone overlay branch hidden in every avatar menu")
    if "item.on_enable.function = [id](LLUICtrl*, const LLSD&) { return live(id) != nullptr; };" not in function_body(cpp, "void populateWhichMenu"):
        out.append("Which overlay items need an enable check on their overlay")
    if "populateWhichMenu(which);" not in prepare_body or "for (size_t i = 0; i < sOverlays.size(); ++i)" not in function_body(cpp, "void populateWhichMenu"):
        out.append("Which overlay must list every overlay (no fixed cap)")
    if len(re.findall(r"getInstances\(\)", cpp)) != 1 or \
            "getInstances()" not in function_body(cpp, "void prepareAvatarMenu"):
        out.append("only prepareAvatarMenu may walk every instance (menu actions use liveTargets / overlayTarget)")
    if "Logic::overlaySubmenuVisible(sOverlays.size())" not in cpp:
        out.append("overlay submenu visibility must come from overlaySubmenuVisible")
    if "branch->setVisible(visible);" not in cpp:
        out.append("overlay submenu branch visibility is never set")
    if "Logic::chooseOverlay(" not in cpp:
        out.append("overlay target must come from chooseOverlay")
    for needle in ("Actions::setPaused(sRows, pause)", "Actions::setDriveMode(sRows, mode, LLUUID::null)",
                   "Actions::setPaused(sRows, true)", "Actions::freezeOverlays(overlay_rows)",
                   "ALPanelGhostStudio::clearAnimationsOnClonesRecorded(liveEntityTargets()",
                   "Actions::toggleSolo(sRows)", "Actions::duplicateRows(sRows, true)",
                   "Actions::deleteRows(live_rows, &skipped)", "Actions::units(rows)", "Actions::applyUnitTransform(unit, foot, rotation, next)",
                   "Actions::setOverlayStyle(std::vector<LLUUID>{ target }, param.asInteger())",
                   "studio.getUnitTransform(unit, foot, rotation, scale)",
                   "ALGhostClearAnim::canClear(agg.mEntities > 0, agg.mPosed > 0)", "clearAllowed(agg)",
                   "beginEditFor(", "stopEditMode()"):
        if needle not in cpp:
            out.append(f"alghostclonemenu.cpp: expected the page's shared action {needle}")
    # The menu never reaches past the shared row actions to a studio setter.
    for forbidden in ("setInstancePaused", "setInstanceDriveMode", "freezeInstance(", "duplicateInstance",
                      "duplicateGroup", "requestRemoveInstance", "transformUnit", "transformGroup", ".mStyle =",
                      "->mStyle ="):
        if forbidden in cpp:
            out.append(f"alghostclonemenu.cpp: {forbidden} bypasses the shared Clones page action")
    # Freeze / Duplicate parity with the page: entity clones pause / duplicate in place.
    if "freezeOverlays(" in function_body(cpp, "void actPlayPause"):
        out.append("Play/Pause must not use the overlay freezer")
    # The page handlers call the SAME shared actions.
    panel = strip_comments(src["alpanelghoststudio.cpp"])
    for needle in ("ALGhostCloneActions::instances(selectedRowValues())", "ALGhostCloneActions::units(selectedRowValues())",
                   "ALGhostCloneActions::applyUnitTransform(unit_id, foot, rotation, scale)",
                   "ALGhostCloneActions::setPaused(selectedRowValues(), true)",
                   "ALGhostCloneActions::setPaused(selectedRowValues(), false)",
                   "ALGhostCloneActions::setDriveMode(selectedRowValues(), mode, anim)",
                   "ALGhostCloneActions::setOverlayStyle(selectedRowValues(), style)",
                   "ALGhostCloneActions::freezeOverlays(selectedRowValues())",
                   "ALGhostCloneActions::deleteRows(selectedRowValues())",
                   "ALGhostCloneActions::duplicateRows(selectedRowValues(), false)",
                   "ALGhostCloneActions::duplicateRows(selectedRowValues(), true)",
                   "ALGhostCloneActions::toggleSolo(selectedRowValues())"):
        if needle not in panel:
            out.append(f"alpanelghoststudio.cpp: the page no longer calls the shared action {needle}")
    if "ALPanelGhostStudio::clearAnimationsOnClonesRecorded(liveEntityTargets(), /*director_fallback=*/true)" not in src["alghostclonemenu.cpp"] \
            or "clearAnimationsOnClonesRecorded(ids)" not in panel.replace("const S32 cleared = clearAnimationsOnClonesRecorded(ids);", "clearAnimationsOnClonesRecorded(ids)"):
        out.append("only the world menu asks for the Director undo fallback; the page Clear path is unchanged")
    clear_fn = function_body(panel, "S32 ALPanelGhostStudio::clearAnimationsOnClonesRecorded")
    if 'LLFloaterReg::getInstance("director")' not in clear_fn or "LLView* host = activeHost();" not in clear_fn or \
            "if (!shell && !host && director_fallback && cleared > 0)" not in clear_fn or \
            'shell->history().recordCustom(host, "clone_clear_anim"' not in clear_fn:
        out.append("Clear animations must record undo even without a visible Clones host (Director shell fallback)")
    for name in ("alghostclonemenu.cpp", "aldirectorselect.cpp", "altooldirectorpick.cpp"):
        code = strip_comments(src[name])
        if re.search(r"(?i)id\w*\.asString\(\)|getAssetUUID|mDirectedAnim", code):
            out.append(f"{name}: must not format an id or touch an animation asset (never show or copy asset ids)")

    # ---- Select in Director ----------------------------------------------------------
    sel = strip_comments(src["aldirectorselect.cpp"])
    avatar = function_body(sel, "bool selectAvatar")
    if not re.search(r"cast\.add\(avatar_id\);.*cast\.setActiveActorId\(avatar_id\);", avatar, re.S):
        out.append("selectAvatar must add to the Cast, then set the Actor bar actor")
    if "planSelect(in)" not in avatar:
        out.append("selectAvatar must use planSelect")
    if "DirectorSelectCastRefused" not in avatar:
        out.append("selectAvatar must report a Cast refusal")
    clone = function_body(sel, "bool selectClone")
    if "planSelect(in)" not in clone or "CloneListAdd" not in clone or "CloneActor" not in clone:
        out.append("selectClone must follow planSelect (list add / replace / Actor bar)")
    raise_fn = function_body(sel, "void raiseDirector")
    if "LLFloaterReg::showInstance(\"director\")" not in raise_fn or "setFrontmost(false)" not in raise_fn:
        out.append("aldirectorselect.cpp: Select must open the Director when closed and raise it when open")
    for fn in ("bool selectAvatar", "bool selectClone", "bool openCloneInDirector"):
        if "raiseDirector();" not in function_body(sel, fn):
            out.append(f"aldirectorselect.cpp: {fn} must raise the Director")
    if "isHUDAttachment()" not in function_body(sel, "LLVOAvatar* avatarAtPick"):
        out.append("aldirectorselect.cpp: a HUD click must not select the wearer")
    if "ensureBuilt()" not in function_body(sel, "ALPanelGhostStudio* selectionHost"):
        out.append("aldirectorselect.cpp: additive selection must reach the Director's Clones host from any page")
    if "ALGhostCloneActions::rowFor(instance_id)" not in function_body(sel, "void ALGhostCloneMenuAccess::selectInHosts"):
        out.append("aldirectorselect.cpp: selection must use the clone's unit row")

    # ---- pick mode -----------------------------------------------------------------
    tool = strip_comments(src["altooldirectorpick.cpp"])
    if "handleRightMouseDown" in tool or "handleRightMouseDown" in src["altooldirectorpick.h"]:
        out.append("pick mode must not take right-click (world menu and camera stay)")
    down = function_body(src["altooldirectorpick.cpp"], "bool ALToolDirectorPick::handleMouseDown")
    if "ctrlAddsToSet(/*is_clone=*/true, ctrl)" not in down or "ctrlAddsToSet(/*is_clone=*/false, ctrl)" not in down:
        out.append("pick mode: Ctrl may add clones only; an avatar click is always plain")
    if "directorVisible()" not in down or "stop();" not in down:
        out.append("pick mode: a click after the Director closed must end the mode")
    if "directorVisible()" not in function_body(tool, "bool ALToolDirectorPick::handleHover"):
        out.append("pick mode: hover must end the mode when the Director is gone")
    start = function_body(src["altooldirectorpick.cpp"], "bool ALToolDirectorPick::start")
    for needle in ("tool_mgr->usingTransientTool()", "isEditModeActive()", "armedOwner().notNull()",
                   "getCrowdPlacementDraft().mActive", "DirectorSelectPickBusy", "return false;"):
        if needle not in start:
            out.append(f"pick mode must refuse to start while another tool or clone session is active: missing {needle}")
    if not re.search(r"if \(busy\)\s*\{.*?return false;\s*\}\s*\+\+mGeneration;.*?mActive = true;\s*tool_mgr->setTransientTool\(this\);",
                     start, re.S):
        out.append("pick mode must take the transient slot only after the busy refusal")
    down_body = function_body(src["altooldirectorpick.cpp"], "bool ALToolDirectorPick::handleMouseDown")
    if "override_tool == LLToolCamera::getInstance()" not in down_body or \
            "override_tool->handleMouseDown(x, y, mask)" not in down_body:
        out.append("pick mode: Alt-drag must reach the camera tool (Alt-orbit)")
    desel_body = function_body(tool, "void ALToolDirectorPick::handleDeselect")
    if "mCameraForwarded" not in down_body or "mCameraForwarded = true;" not in down_body or \
            not re.search(r"if \(mCameraForwarded\)\s*\{\s*mCameraForwarded = false;\s*rearmAfterCamera\(\);\s*return;", desel_body):
        out.append("pick mode: a forwarded camera gesture (which clears the transient slot) must not end the mode")
    if "setTransientTool" in desel_body:
        out.append("pick mode: never take the transient slot inside handleDeselect (re-enters getCurrentTool)")
    rearm = function_body(tool, "void ALToolDirectorPick::rearmAfterCamera")
    for needle in ("doOnIdleOneTime", "if (mRearmPending)", "mRearmPending = true;", "const U32 generation = mGeneration;",
                   "self->mGeneration != generation", "ALGhostCloneMenuLogic::rearmDecision(", "tool_mgr->getCurrentTool() == self",
                   "tool_mgr->getBaseTool() == forward_base", "CAMERA_MODE_MOUSELOOK", "tool_tool_placeholder"):
        if needle == "tool_tool_placeholder":
            continue
        if needle not in rearm:
            out.append(f"pick mode re-arm must be bound to its activation and base tool: missing {needle}")
    if rearm.find("self->mGeneration != generation") > rearm.find("self->mRearmPending = false;"):
        out.append("pick mode re-arm: a callback from an earlier activation must return before touching any state")
    if not re.search(r"case ALGhostCloneMenuLogic::RearmAction::Install:\s*tool_mgr->setTransientTool\(self\);", rearm) or \
            not re.search(r"case ALGhostCloneMenuLogic::RearmAction::EndMode:\s*self->mActive = false;", rearm):
        out.append("pick mode re-arm: Install and EndMode must follow the decision")
    start_fn = function_body(tool, "bool ALToolDirectorPick::start")
    stop_fn = function_body(tool, "void ALToolDirectorPick::stop()")
    for name, fn in (("start", start_fn), ("stop", stop_fn)):
        if "++mGeneration;" not in fn or "mCameraForwarded = false;" not in fn or "mRearmPending = false;" not in fn:
            out.append(f"pick mode {name}() must start a new generation and clear the camera-forward and re-arm state")
    if "mForwardBase = base;" not in down_body:
        out.append("pick mode: the base tool is recorded when the camera gesture is forwarded")
    keys = function_body(tool, "bool ALToolDirectorPick::handleKey")
    if not re.search(r"if \(key == KEY_ESCAPE && mask == MASK_NONE\)\s*\{\s*stop\(\);\s*return true;\s*\}", keys):
        out.append("pick mode: Esc must reach stop()")
    desel = function_body(tool, "void ALToolDirectorPick::handleDeselect")
    if "mActive = false;" not in desel:
        out.append("pick mode: a replaced tool must end the mode")
    shell = strip_comments(src["alkitshell.cpp"])
    if 'getName() == "director_shell"' not in function_body(shell, "void ALKitShell::onVisibilityChange") or \
            'getName() == "director_shell"' not in function_body(shell, "ALKitShell::~ALKitShell"):
        out.append("alkitshell.cpp: only the Director's own shell may end pick mode")
    if "ALToolDirectorPick::stopIfActive();" not in function_body(shell, "void ALKitShell::onVisibilityChange"):
        out.append("alkitshell.cpp: closing the Director must end pick mode")
    if "ALToolDirectorPick::stopIfActive();" not in function_body(shell, "ALKitShell::~ALKitShell"):
        out.append("alkitshell.cpp: destroying the shell must end pick mode")
    if "ALToolDirectorPick::getInstance()->toggle();" not in shell:
        out.append("alkitshell.cpp: the Select button must toggle pick mode")
    if "setValueIfChanged(mActorPick, LLSD(ALToolDirectorPick::isActive()), true);" not in function_body(shell, "void ALKitShell::draw"):
        out.append("alkitshell.cpp: the Select button must show the mode's state")
    if 'name="kit_actor_pick"' not in src["floater_director_v2.xml"] or 'is_toggle="true"' not in \
            re.search(r'<button name="kit_actor_pick"[^>]*>', src["floater_director_v2.xml"]).group(0):
        out.append("floater_director_v2.xml: kit_actor_pick toggle button missing (regenerate the kit)")
    if "\tkit_actor_pick\t" not in src["director_kit_index.xml"]:
        out.append("director_kit_index.xml: kit_actor_pick search row missing (regenerate the kit)")
    if '("pick", "Select")' not in src["director_kit_gen.py"]:
        out.append("director_kit_gen.py: the Select button is not generated")

    # ---- strings + build ---------------------------------------------------------------
    keys = set(re.findall(r'<string name="([^"]+)"', src["strings.xml"]))
    for name in ("alghostclonemenu.cpp", "aldirectorselect.cpp", "altooldirectorpick.cpp"):
        for key in re.findall(r'(?:str|LLTrans::getString)\("([^"]+)"(?:, args)?\)', strip_comments(src[name])):
            if key not in keys:
                out.append(f"{name}: string {key} is missing from strings.xml")
    for key in re.findall(r'reason_key = nullptr|"(GhostCloneMenu\w+)"', cpp):
        if key and key not in keys:
            out.append(f"string {key} is missing from strings.xml")
    for name in ("alghostclonemenu.cpp", "aldirectorselect.cpp", "altooldirectorpick.cpp", "alghostclonemenu.h",
                 "alghostcloneactions.cpp", "alghostcloneactions.h",
                 "alghostclonemenulogic.h", "aldirectorselect.h", "altooldirectorpick.h"):
        if name not in src["CMakeLists.txt"]:
            out.append(f"CMakeLists.txt does not list {name}")
    return out


def native() -> int:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from native_check_tools import check_directory, compile_run
    directory = check_directory("clone_menu")
    result = compile_run(directory, NATIVE, "clone_menu_logic", flags=['/I"%s"' % NV])
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    return result.returncode


def run_check() -> int:
    found = findings(load_sources())
    for item in found:
        print("FINDING:", item)
    code = 1 if found else 0
    code |= native()
    if not found:
        print("clone menu static checks passed")
    return code


def swap(src, name, old, new):
    mutated = dict(src)
    mutated[name] = src[name].replace(old, new, 1)
    return mutated


def selftest() -> int:
    src = load_sources()
    if findings(src):
        print("SELFTEST FAIL: the unmodified sources already have findings")
        return 1
    mutations = [
        ("avatar menu shown without prepareAvatarMenu",
         swap(src, "lltoolpie.cpp", "ALGhostCloneMenu::prepareAvatarMenu(gMenuAvatarOther, avatar);", "")),
        ("self menu shown without prepareAvatarMenu",
         swap(src, "lltoolpie.cpp", "ALGhostCloneMenu::prepareAvatarMenu(gMenuAvatarSelf,", "(void)(gMenuAvatarSelf,")),
        ("overlay submenu always visible",
         swap(src, "alghostclonemenu.cpp", "Logic::overlaySubmenuVisible(sOverlays.size())", "true")),
        ("clone menu opens on press (no gesture)",
         swap(src, "lltoolpie.cpp", "if (ALGhostCloneMenu::pressRight(x, y, mPick))\n        {\n            return false;\n        }\n        ", "")),
        ("release hook missing",
         swap(src, "llviewerwindow.cpp", "ALGhostCloneMenu::releaseRight(", "(void)(")),
        ("mouse moves never reach the gesture",
         swap(src, "llviewerwindow.cpp", "ALGhostCloneMenu::noteMouseMove(x, y);", "")),
        ("edit tool traps right-drag",
         swap(src, "altoolghostedit.cpp", "    // let the normal right-drag camera work -- never trap the user without\n    // camera control (the path tool's rule)\n    return false;", "    return true;")),
        ("menu acts on every instance",
         swap(src, "alghostclonemenu.cpp", "const std::vector<LLUUID> selection = Actions::instances(host_rows);",
              "std::vector<LLUUID> selection; for (const auto& i : studio.getInstances()) selection.push_back(i.mId);")),
        ("targets ignore the plan",
         swap(src, "alghostclonemenu.cpp", "Logic::planTargets(clone, selection)", "Logic::planTargets(clone, std::vector<LLUUID>())")),
        ("Ctrl adds an avatar",
         swap(src, "altooldirectorpick.cpp", "ctrlAddsToSet(/*is_clone=*/false, ctrl)", "ctrlAddsToSet(/*is_clone=*/true, ctrl)")),
        ("pick mode survives Director close",
         swap(src, "alkitshell.cpp", "    if (!new_visibility && getName() == \"director_shell\")\n        ALToolDirectorPick::stopIfActive();\n", "")),
        ("pick mode survives shell destruction",
         swap(src, "alkitshell.cpp", "    if (getName() == \"director_shell\")\n        ALToolDirectorPick::stopIfActive();\n    ALAvatarSubjectPicker::cancelFor(this);", "    ALAvatarSubjectPicker::cancelFor(this);")),
        ("pick mode takes right-click",
         swap(src, "altooldirectorpick.h", "    bool handleKey(KEY key, MASK mask) override;", "    bool handleKey(KEY key, MASK mask) override;\n    bool handleRightMouseDown(S32, S32, MASK) override { return true; }")),
        ("non-cast avatar not added to the Cast",
         swap(src, "aldirectorselect.cpp", "cast.add(avatar_id);      // the Cast's own add path and refusals", "")),
        ("selectAvatar skips planSelect",
         swap(src, "aldirectorselect.cpp", "ALGhostCloneMenuLogic::planSelect(in);\n    if (action", "ALGhostCloneMenuLogic::SelectAction::SetActor;\n    if (action")),
        ("Select in Director left in the Director submenu",
         swap(src, "menu_avatar_other.xml", '<context_menu\n   label="Director"\n   name="Director">\n',
              '<context_menu\n   label="Director"\n   name="Director">\n    <menu_item_call label="x" name="Select in Director" />\n')),
        ("one avatar menu fragment differs",
         swap(src, "menu_avatar_self.xml", 'label="Ghost"', 'label="Ghost2"')),
        ("overlay Style item with a wrong value",
         swap(src, "menu_attachment_self.xml", 'name="ov_style_9">\n            <menu_item_check.on_click function="GhostOverlay.Style" parameter="9" />',
              'name="ov_style_9">\n            <menu_item_check.on_click function="GhostOverlay.Style" parameter="8" />')),
        ("unregistered callback",
         swap(src, "alghostclonemenu.cpp", 'commit.add("GhostClone.Delete"', 'commit.add("GhostClone.Delete2"')),
        ("missing string",
         swap(src, "strings.xml", 'name="GhostCloneMenuPosed"', 'name="GhostCloneMenuPosedX"')),
        ("entity menu reordered",
         swap(src, "menu_ghost_clone.xml", 'label="Open in Director"', 'label="Open in Directorx"')),
        ("Select button missing from the generator",
         swap(src, "director_kit_gen.py", '("pick", "Select")', '("pick", "Pick")')),
        ("clone menu not created",
         swap(src, "llviewermenu.cpp", "ALGhostCloneMenu::createMenu();", "")),
        # the four mutations the round-1 review applied by hand
        ("release opens the menu for a drag (if (open) -> if (true))",
         swap(src, "alghostclonemenu.cpp", "if (open)", "if (true)")),
        ("actions pointed at the previous menu's clone (sRows = { sPicked } before sPicked updates)",
         swap(src, "alghostclonemenu.cpp", "    sPicked = clone;\n    sAgg = computeAggregate();",
              "    sRows = std::vector<LLUUID>{ sPicked };\n    sPicked = clone;\n    sAgg = computeAggregate();")),
        ("hidden host list not refreshed before Ctrl-add",
         swap(src, "aldirectorselect.cpp", "        host->refreshList(true);\n", "")),
        ("hidden host list not reconciled with the shared selection",
         swap(src, "aldirectorselect.cpp", "host->mObservedSelectionRevision != studio.getSelectionRevision()", "false")),
        ("Ctrl-add keeps a stale list (no replace)",
         swap(src, "aldirectorselect.cpp", "    if (shared_moved)\n    {\n        host->mList->deselectAllItems(true);", "    if (false)\n    {\n        host->mList->deselectAllItems(true);")),
        ("spawn point stays armed after the menu",
         swap(src, "alghostclonemenu.cpp", "    LLMenuHolderGL::sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);\n", "")),
        ("spawn point sentinel is not the stock disarmed value",
         swap(src, "alghostclonemenu.cpp", "constexpr S32 SPAWN_DISARMED = S32_MAX;", "constexpr S32 SPAWN_DISARMED = S32_MAX / 2;")),
        ("press leaves an earlier menu's spawn point armed",
         swap(src, "alghostclonemenu.cpp", "    // An earlier menu's spawn point must not swallow this click's release.\n    LLMenuHolderGL::sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);\n", "")),
        ("Esc cannot end a pick mode waiting for its re-arm",
         swap(src, "llviewerwindow.cpp", "    if (key == KEY_ESCAPE && mask == MASK_NONE && ALToolDirectorPick::handlePendingEscape())", "    if (false)")),
        ("pending Esc does not stop the activation",
         swap(src, "altooldirectorpick.cpp", "    getInstance()->stop();\n    return true;", "    return true;")),
        ("double-click falls through to walk-to",
         swap(src, "altooldirectorpick.cpp", "bool ALToolDirectorPick::handleDoubleClick(S32 x, S32 y, MASK mask)\n{\n    return handleMouseDown(x, y, mask);", "bool ALToolDirectorPick::handleDoubleClick(S32 x, S32 y, MASK mask)\n{\n    return false;")),
        ("scan suppression not exception safe",
         swap(src, "aldirectorselect.cpp", "~ScanGuard() { mFlag = mPrevious; }", "~ScanGuard() {}")),
        ("hidden host noted as the active host",
         swap(src, "aldirectorselect.cpp", "    if (host->isInVisibleChain())\n    {\n        host->noteActiveHost();", "    if (true)\n    {\n        host->noteActiveHost();")),
        ("tool change does not cancel at the transition",
         swap(src, "lltoolmgr.cpp", "        ALGhostCloneMenu::cancelRight(); // [CloneMenu] a tool change ends a pending clone right-click\n", "")),
        ("camera-mode change does not cancel (setter path)",
         swap(src, "llagentcamera.cpp", "        mCameraMode = CAMERA_MODE_FOLLOW;\n        ALGhostCloneMenu::cancelRight(); // [CloneMenu] every camera-mode change ends a pending clone right-click\n",
              "        mCameraMode = CAMERA_MODE_FOLLOW;\n")),
        ("re-arm ignores the activation generation",
         swap(src, "altooldirectorpick.cpp", "        if (self->mGeneration != generation)\n        {\n            return;                     // stopped / restarted since: not this activation's business\n        }\n", "")),
        ("re-arm ignores the base tool",
         swap(src, "altooldirectorpick.cpp", "tool_mgr->getBaseTool() == forward_base,", "true,")),
        ("re-arm ignores that the picker is already installed",
         swap(src, "altooldirectorpick.cpp", "tool_mgr->getCurrentTool() == self,", "false,")),
        ("start does not begin a new generation",
         swap(src, "altooldirectorpick.cpp", "    ++mGeneration;\n    mCameraForwarded = false;\n    mRearmPending = false;\n    mActive = true;", "    mCameraForwarded = false;\n    mRearmPending = false;\n    mActive = true;")),
        ("stop leaves a queued re-arm alive",
         swap(src, "altooldirectorpick.cpp", "    ++mGeneration;                      // any queued re-arm belongs to a finished activation\n", "")),
        ("several re-arms queue at once",
         swap(src, "altooldirectorpick.cpp", "    if (mRearmPending)\n    {\n        return;                         // one pending re-arm at a time\n    }\n", "")),
        ("group drop plays on only the hit clone",
         swap(src, "alghostcloneactions.cpp", "ALGhostCloneMenuLogic::dropTargets(hit_instance, group != nullptr, members)",
              "ALGhostCloneMenuLogic::dropTargets(hit_instance, false, members)")),
        ("expanded member drop plays on the whole group",
         swap(src, "alghostcloneactions.cpp", "return group && !group->mEditMembers ? group : nullptr;", "return group;")),
        ("drop plays on the hit clone only (lltooldraganddrop)",
         swap(src, "lltooldraganddrop.cpp", "        drop_plan.mTargets, real->getAssetUUID(), real->getName(), /*director_fallback=*/true);", "        { mGhostCloneTarget }, real->getAssetUUID(), real->getName(), /*director_fallback=*/true);")),
        ("Stagger visible for a non-group target",
         swap(src, "alghostclonemenu.cpp", "stagger->setVisible(Logic::staggerVisible(unit_is_group));", "stagger->setVisible(true);")),
        ("Stagger reimplements the offsets in the menu",
         swap(src, "alghostclonemenu.cpp", "    const Actions::StaggerState before = Actions::snapshotStagger(sPicked);\n",
              "    const Actions::StaggerState before = Actions::snapshotStagger(sPicked);\n    ALGhostStudio::instance().setStartOffset(sPicked, 1.f);\n")),
        ("Stagger math: the Spread multiply removed",
         swap(src, "alghostcloneactions.cpp", "studio.setStartOffset(id, F32(unit*spread));", "studio.setStartOffset(id, F32(unit));")),
        ("Stagger math: the ripple range starts at zero",
         swap(src, "alghostcloneactions.cpp", "F64 low = std::numeric_limits<F64>::max(), high = -low;", "F64 low = 0., high = 0.;")),
        ("Stagger math: the seed is overwritten",
         swap(src, "alghostcloneactions.cpp", "    if (ids.empty() || mode < 0 || mode > 2) return false;\n    const S32 direction", "    if (ids.empty() || mode < 0 || mode > 2) return false;\n    seed = 0;\n    const S32 direction")),
        ("Stagger math: the Spread clamp removed",
         swap(src, "alghostcloneactions.cpp", 'const F32 spread = llclamp(gSavedSettings.getF32("GhostStudioStartSpread"), 0.f, 10.f);', 'const F32 spread = gSavedSettings.getF32("GhostStudioStartSpread");')),
        ("Stagger check kept after the Crowd tab changed the offsets",
         swap(src, "alghostcloneactions.cpp", "found != staggerModes().end() && found->second.mOffsets == offsets;", "found != staggerModes().end();")),
        ("Stagger check never prunes dissolved groups",
         swap(src, "alghostcloneactions.cpp", "    pruneStaggerModes();\n    const auto found = staggerModes().find(group->mId);\n    const bool still_applied", "    const auto found = staggerModes().find(group->mId);\n    const bool still_applied")),
        ("a no-op Stagger records an undo step",
         swap(src, "alghostclonemenu.cpp", "    if (before.mOffsets == after.mOffsets)\n    {\n        return;                         // nothing changed: no undo step\n    }\n", "")),
        ("Delete acts on a vanished clone",
         swap(src, "alghostclonemenu.cpp", "Actions::deleteRows(live_rows, &skipped)", "Actions::deleteRows(sRows, &skipped)")),
        ("single-clone toast lost",
         swap(src, "lltooldraganddrop.cpp", "played == 1 && drop_plan.mTargets.size() == 1", "false")),
        ("Stagger changes a clone outside the group (crowd-wide apply)",
         swap(src, "alghostcloneactions.cpp", "!applyStartOffsets(members, mode, seed)", "(studio.applyCrowdVariation(members.front(), seed, true, false), false)")),
        ("Stagger writes the global Start offset setting",
         swap(src, "alghostcloneactions.cpp", "    staggerModes()[group->mId] = applied;\n    return true;", "    gSavedSettings.setS32(\"GhostStudioStartMode\", mode);\n    staggerModes()[group->mId] = applied;\n    return true;")),
        ("Stagger changes the selection",
         swap(src, "alghostcloneactions.cpp", "    const U32 seed = studio.getInstance(members.front())->mCrowdSeed;\n", "    const U32 seed = studio.getInstance(members.front())->mCrowdSeed;\n    studio.setSelected(group->mId);\n")),
        ("Stagger accepts members of different crowds",
         swap(src, "alghostcloneactions.cpp", "inst->mCrowdId.isNull() || (crowd.notNull() && inst->mCrowdId != crowd)", "inst->mCrowdId.isNull()")),
        ("Stagger undo acts without checking a saved member survives",
         swap(src, "alghostcloneactions.cpp", "    if (alive.empty())\n    {\n        return false;\n    }\n", "")),
        ("Stagger undo does not restart the animations",
         swap(src, "alghostcloneactions.cpp", "        studio.restartInstanceAnimation(offset.first);\n", "")),
        ("Stagger looks up the Director before a refusal",
         swap(src, "alghostclonemenu.cpp", "    if (const char* refusal = Actions::staggerRefusal(sPicked))\n    {\n        tip(str(refusal));\n        return;\n    }\n    const Actions::StaggerState before",
              "    LLFloaterReg::getInstance(\"director\");\n    if (const char* refusal = Actions::staggerRefusal(sPicked))\n    {\n        tip(str(refusal));\n        return;\n    }\n    const Actions::StaggerState before")),
        ("Stagger math forked from the Crowd tab's (unit expression)",
         swap(src, "alghostcloneactions.cpp", "const F64 unit = mode == 1 ? ALCloneQol::randomUnit(seed, stable) : mode == 2 ?\n            ALCloneQol::normalise(ALCloneQol::ripple(inst->mCrowdLocalX, inst->mCrowdLocalY, direction), low, high) : 0.;",
              "const F64 unit = mode == 1 ? ALCloneQol::randomUnit(seed, stable) : 0.;")),
        ("Stagger math forked from the Crowd tab's (stable key)",
         swap(src, "alghostcloneactions.cpp", "stable = (stable ^ byte)*1099511628211ULL;", "stable = (stable ^ byte)*31ULL;")),
        ("Crowd tab code edited (pinned file)",
         swap(src, "alghoststudio.cpp", "void ALGhostStudio::applyCrowdVariation(const LLUUID& member_id, U32 seed, bool timing, bool assign_looks, bool building)\n{",
              "void ALGhostStudio::applyCrowdVariation(const LLUUID& member_id, U32 seed, bool timing, bool assign_looks, bool building)\n{ stableCloneKey(member_id);")),
        ("world drop records no undo without a host",
         swap(src, "alpanelghoststudio.cpp", "    if (!shell && !host && director_fallback && played > 0)", "    if (false)")),
        ("world drop does not ask for the undo fallback",
         swap(src, "lltooldraganddrop.cpp", ", /*director_fallback=*/true);", ");")),
        ("drop plan includes a clone without a body",
         swap(src, "alghostcloneactions.cpp", "inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&\n            studio.resolveEntityClone(id))", "inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)")),
        ("empty drop plan falls back to the hit clone",
         swap(src, "lltooldraganddrop.cpp", "        mCustomMsg = \"The clone is not ready to play an animation yet.\";\n        return ACCEPT_NO_CUSTOM;", "        drop_plan.mTargets.push_back(mGhostCloneTarget);")),
        ("Stagger has no undo step",
         swap(src, "alghostclonemenu.cpp", '        shell->history().recordCustom(shell, "clone_stagger", "Stagger clones",', '        shell->history().barrier(//')),
        ("overlay Size scales a whole collapsed group",
         swap(src, "alghostclonemenu.cpp", "if (live(target) && !Actions::collapsedGroup(target))", "if (live(target))")),
        ("overlay Size item without the group check",
         swap(src, "menu_avatar_self.xml", 'function="GhostOverlay.EnableSize"', 'function="GhostOverlay.Enable"')),
        ("overlay Edit keeps a stale clone selection",
         swap(src, "alghostclonemenu.cpp", "toggleEdit(target, /*keep_multi_selection=*/false)", "toggleEdit(target, /*keep_multi_selection=*/true)")),
        ("menu Edit collapses the multi-selection",
         swap(src, "alghostclonemenu.cpp", "std::find(sRows.begin(), sRows.end(), shared_row) == sRows.end()", "true")),
        ("hidden Ctrl-pick selection ignored by the menu (no hidden host rows)",
         swap(src, "aldirectorselect.cpp", "    if (hidden && hidden->mObservedSelectionRevision == ALGhostStudio::instance().getSelectionRevision())", "    if (false)")),
        ("menu builds the Director host to read rows",
         swap(src, "aldirectorselect.cpp", "    ALPanelGhostStudio* hidden = findHostBelow(lazy);", "    if (lazy) lazy->ensureBuilt();\n    ALPanelGhostStudio* hidden = findHostBelow(lazy);")),
        ("additive selection of a row that is not in the list still marks the host mirrored",
         swap(src, "aldirectorselect.cpp", "        studio.setSelected(row);\n        return;\n    }\n    host->mPrimaryRow = row;", "    }\n    host->mPrimaryRow = row;")),
        ("HUD attachment menu offers Select in Director",
         swap(src, "alghostclonemenu.cpp", "sMenuHud ? str(\"DirectorSelectHud\") : ALDirectorSelect::avatarRefusal(sMenuAvatar)", "ALDirectorSelect::avatarRefusal(sMenuAvatar)")),
        ("Stagger no-op guard looks at the mode",
         swap(src, "alghostclonemenu.cpp", "    if (before.mOffsets == after.mOffsets)\n", "    if (before.mOffsets == after.mOffsets && before.mMode == after.mMode)\n")),
        ("recorded fallback builds the Director while a host is visible",
         swap(src, "alpanelghoststudio.cpp", "    if (!shell && !host && director_fallback && played > 0)", "    if (!shell && director_fallback && played > 0)")),
        ("hidden Ctrl-add starts the inventory scan",
         swap(src, "aldirectorselect.cpp", "        } scan_guard(host->mSuppressLibraryScan);\n", "        } scan_guard(dummy);\n")),
        ("any kit shell ends pick mode",
         swap(src, "alkitshell.cpp", "    if (!new_visibility && getName() == \"director_shell\")", "    if (!new_visibility)")),
        ("page Clear gets the Director fallback",
         swap(src, "alpanelghoststudio.cpp", "if (!shell && !host && director_fallback && cleared > 0)", "if (!shell && cleared > 0)")),
        ("Which overlay items stay enabled for a dead overlay",
         swap(src, "alghostclonemenu.cpp", "        item.on_enable.function = [id](LLUICtrl*, const LLSD&) { return live(id) != nullptr; };\n", "")),
        ("pressRight arms before reading the tool",
         swap(swap(src, "alghostclonemenu.cpp", "    LLTool* press_tool = LLToolMgr::getInstance()->getCurrentTool();\n", ""),
               "alghostclonemenu.cpp", "    sGesture.press(x, y);\n    sPendingClone = clone;",
               "    sGesture.press(x, y);\n    LLTool* press_tool = LLToolMgr::getInstance()->getCurrentTool();\n    sPendingClone = clone;")),
        ("Alt-orbit ends pick mode (no re-arm)",
         swap(src, "altooldirectorpick.cpp", "        rearmAfterCamera();\n        return;", "        return;")),
        ("pick mode re-arms inside handleDeselect",
         swap(src, "altooldirectorpick.cpp", "        mCameraForwarded = false;\n        rearmAfterCamera();", "        mCameraForwarded = false;\n        LLToolMgr::getInstance()->setTransientTool(this);\n        rearmAfterCamera();")),
        ("Open in Director uses a refusal cached at open",
         swap(src, "alghostclonemenu.cpp", "const std::string reason = ALDirectorSelect::cloneRefusal(sPicked);", "static const std::string reason;")),
        ("overlay Enable trusts the cached target",
         swap(src, "alghostclonemenu.cpp", "enable.add(\"GhostOverlay.Enable\", [](LLUICtrl* ctrl, const LLSD&)\n    {\n        return state(ctrl, live(sOverlayTarget) != nullptr, \"GhostCloneMenuNotReady\");", "enable.add(\"GhostOverlay.Enable\", [](LLUICtrl* ctrl, const LLSD&)\n    {\n        return state(ctrl, sOverlayTarget.notNull(), \"GhostCloneMenuNotReady\");")),
        ("Which overlay pick leaves the overlay state stale",
         swap(src, "alghostclonemenu.cpp", "sChosenOverlay = id; refreshOverlayState(); };", "sChosenOverlay = id; };")),
        ("Director raised before the Cast accepted the avatar",
         swap(src, "aldirectorselect.cpp", "    const ALGhostCloneMenuLogic::SelectAction action = ALGhostCloneMenuLogic::planSelect(in);\n    if (action",
              "    const ALGhostCloneMenuLogic::SelectAction action = ALGhostCloneMenuLogic::planSelect(in);\n    raiseDirector();\n    if (action")),
        ("overlay branch forced visible (branch->setVisible(true) appended)",
         swap(src, "alghostclonemenu.cpp", "branch->setVisible(visible);", "branch->setVisible(visible);\n    branch->setVisible(true);")),
        ("Escape cannot reach stop() (unreachable condition)",
         swap(src, "altooldirectorpick.cpp", "if (key == KEY_ESCAPE && mask == MASK_NONE)",
              "if (key == KEY_ESCAPE && mask == MASK_NONE && mask != MASK_NONE)")),
        ("Escape exit removed",
         swap(src, "altooldirectorpick.cpp", "if (key == KEY_ESCAPE && mask == MASK_NONE)", "if (false)")),
        # round-1 findings
        ("gesture forgets focus loss",
         swap(src, "llviewerwindow.cpp", "ALGhostCloneMenu::cancelRight(); // [CloneMenu] a pending clone right-click never survives focus loss", "")),
        ("gesture ignores tool change",
         swap(src, "alghostclonemenu.cpp", "getCurrentTool() == sPressTool &&", "true &&")),
        ("gesture ignores a modal dialog",
         swap(src, "alghostclonemenu.cpp", "LLModalDialog::activeCount() == 0 && ", "")),
        ("UI-handled release still opens the menu",
         swap(src, "llviewerwindow.cpp", "            if (!down && clicktype == CLICK_RIGHT) ALGhostCloneMenu::cancelRight(); // [CloneMenu] a UI view handled the release\n", "")),
        ("menu bypasses the page's Delete",
         swap(src, "alghostclonemenu.cpp", "Actions::deleteRows(live_rows, &skipped)", "ALGhostStudio::instance().requestRemoveInstance(sPicked)")),
        ("menu Duplicate uses the entity-refusing duplicate",
         swap(src, "alghostclonemenu.cpp", "Actions::duplicateRows(sRows, true)", "Actions::duplicateRows(sRows, false)")),
        ("Freeze pose uses the overlay freezer on entities",
         swap(src, "alghostclonemenu.cpp", "Actions::setPaused(sRows, true);\n    if (!overlay_rows.empty()", "if (!overlay_rows.empty()")),
        ("Clear animations ignores a posed clone in a mixed selection",
         swap(src, "alghostclonemenu.cpp", "ALGhostClearAnim::canClear(agg.mEntities > 0, agg.mPosed > 0)", "ALGhostClearAnim::canClear(agg.mEntities > 0, agg.mPosed == agg.mEntities)")),
        ("Clear animations undo needs a visible Clones host",
         swap(src, "alpanelghoststudio.cpp", "if (LLFloater* director = LLFloaterReg::getInstance(\"director\"))", "if (LLFloater* director = nullptr)")),
        ("page no longer calls the shared Delete",
         swap(src, "alpanelghoststudio.cpp", "ALGhostCloneActions::deleteRows(selectedRowValues())", "false")),
        ("orphaned overlay attributed to you",
         swap(src, "alghostclonemenu.cpp", " && !inst.mSourceRemoved", "")),
        ("pick mode displaces another transient tool",
         swap(src, "altooldirectorpick.cpp", "        tool_mgr->usingTransientTool() ||\n", "")),
        ("pick mode starts over an armed Place",
         swap(src, "altooldirectorpick.cpp", "(ALToolGhostPlace::instanceExists() && ALToolGhostPlace::getInstance()->armedOwner().notNull()) ||\n        ", "")),
        ("Ctrl-click cannot reach a hidden Clones host",
         swap(src, "aldirectorselect.cpp", "lazy->ensureBuilt();", "")),
        ("Select does not raise the Director",
         swap(src, "aldirectorselect.cpp", "    floater->setFrontmost(false);\n", "")),
        ("HUD click selects the wearer",
         swap(src, "aldirectorselect.cpp", "    if (object && object->isHUDAttachment())\n    {\n        return nullptr;\n    }\n    LLVOAvatar* avatar = ALAvatarSubjectPicker", "    LLVOAvatar* avatar = ALAvatarSubjectPicker")),
        ("Alt-orbit not forwarded",
         swap(src, "altooldirectorpick.cpp", "override_tool == LLToolCamera::getInstance()", "false")),
        ("Open in Director has no enable check",
         swap(src, "menu_ghost_clone.xml", '    <menu_item_call.on_enable function="GhostClone.EnableSelectInDirector" />\n  </menu_item_call>\n</context_menu>', '  </menu_item_call>\n</context_menu>')),
        ("tooltips never restored",
         swap(src, "alghostclonemenu.cpp", "reason.empty() ? base->second.mTip : reason", "reason")),
        ("widget address reuse trusted",
         swap(src, "alghostclonemenu.cpp", "base == sBase.end() || base->second.mItem.get() != ctrl", "base == sBase.end()")),
        ("Which overlay not re-arranged",
         swap(src, "alghostclonemenu.cpp", "        submenu->arrange();\n", "")),
        ("Which overlay capped",
         swap(src, "alghostclonemenu.cpp", "for (size_t i = 0; i < sOverlays.size(); ++i)\n    {\n        const LLUUID id = sOverlays[sOverlays.size() - 1 - i];", "for (size_t i = 0; i < 12; ++i)\n    {\n        const LLUUID id = sOverlays[sOverlays.size() - 1 - i];")),
    ]
    failures = []
    for label, mutated in mutations:
        if all(mutated[k] == src[k] for k in src):
            failures.append(f"mutation '{label}' did not apply")
        elif not findings(mutated):
            failures.append(f"mutation '{label}' passes")
        else:
            print(f"ok    rejected: {label}")
    if failures:
        for item in failures:
            print("SELFTEST FAIL:", item)
        return 1
    print("selftest passed")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="check the clone menu wiring and logic")
    parser.add_argument("--selftest", action="store_true", help="run planted must-fail mutations")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.check:
        return run_check()
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
