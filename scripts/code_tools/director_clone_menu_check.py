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
   the Actor bar unchanged, a Nearby people list that shows a UUID or is not nearest
   first / capped, a Recent animations list that exposes an asset id or is not newest
   first / capped, a Look / turn item that touches a clone outside the targets, a Fade that
   deletes a clone or is saved in a scene, a Look at that leaves an armed Turn).
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
#include <cmath>
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

// [ClonePick] Which clone a world click means: the WORLD ray under the cursor against each body box, ranked by
// ENTRY distance. The fixture builds candidates through BuildFn so planted mutants can break each rule.
typedef std::vector<PickCandidate<int>> Cands;
struct PBox { int mId; BodyBox mBox; float mProbeD2; };
struct Scene { float mEye[3]; float mDir[3]; float mForward[3] = { 0.f, 1.f, 0.f }; float mNear = .1f; std::vector<PBox> mBoxes; };
struct Picked { int mId; float mPoint[3]; };
typedef Picked (*PickFn)(const Scene&);

BodyBox mkBox(float cx, float cy, float cz, float hx, float hy, float hz)
{
    BodyBox b; b.mCenter[0] = cx; b.mCenter[1] = cy; b.mCenter[2] = cz; b.mHalf[0] = hx; b.mHalf[1] = hy; b.mHalf[2] = hz; return b;
}
float dist3(const float* a, const float* b) { return std::sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2])); }
void entryPoint(const Scene& sc, float t, float* out) { for (int i = 0; i < 3; ++i) out[i] = sc.mEye[i] + sc.mDir[i] * t; }

Picked pickReal(const Scene& sc)
{
    Cands c;
    float near_t = 0.f;
    const bool near_ok = rayNearDistance(sc.mNear, sc.mDir, sc.mForward, near_t);
    for (const PBox& b : sc.mBoxes)
    {
        PickCandidate<int> k; k.mId = b.mId; k.mProbeD2 = b.mProbeD2; float t = 0.f;
        if (near_ok && rayBoxEntry(sc.mEye, sc.mDir, b.mBox, near_t, t)) { k.mHasBox = true; k.mEntryT = t; }
        c.push_back(k);
    }
    bool from_box = false;
    Picked r; r.mId = choosePickedClone<int>(c, 0, &from_box);
    r.mPoint[0] = r.mPoint[1] = r.mPoint[2] = 0.f;
    for (size_t i = 0; i < c.size(); ++i)
        if (c[i].mId == r.mId)
        {
            if (from_box) entryPoint(sc, c[i].mEntryT, r.mPoint);
            else for (const PBox& b : sc.mBoxes) if (b.mId == r.mId) for (int a = 0; a < 3; ++a) r.mPoint[a] = b.mBox.mCenter[a];
        }
    return r;
}
// mutant: rank by the distance to the box CENTRE instead of the ray's entry
Picked pickCentreRank(const Scene& sc)
{
    Cands c;
    for (const PBox& b : sc.mBoxes)
    {
        PickCandidate<int> k; k.mId = b.mId; k.mProbeD2 = b.mProbeD2; float t = 0.f;
        if (rayBoxEntry(sc.mEye, sc.mDir, b.mBox, sc.mNear, t)) { k.mHasBox = true; k.mEntryT = dist3(sc.mEye, b.mBox.mCenter); }
        c.push_back(k);
    }
    Picked r = pickReal(sc);
    r.mId = choosePickedClone<int>(c, 0);
    return r;
}
// mutant: the eye inside a box still counts as a hit (entry distance 0)
Picked pickEyeInside(const Scene& sc)
{
    Cands c;
    for (const PBox& b : sc.mBoxes)
    {
        PickCandidate<int> k; k.mId = b.mId; k.mProbeD2 = b.mProbeD2; float t = 0.f;
        bool inside = true;
        for (int a = 0; a < 3; ++a) if (std::fabs(sc.mEye[a] - b.mBox.mCenter[a]) > b.mBox.mHalf[a]) inside = false;
        if (inside) { k.mHasBox = true; k.mEntryT = 0.f; }
        else if (rayBoxEntry(sc.mEye, sc.mDir, b.mBox, sc.mNear, t)) { k.mHasBox = true; k.mEntryT = t; }
        c.push_back(k);
    }
    Picked r = pickReal(sc);
    r.mId = choosePickedClone<int>(c, 0);
    return r;
}
// mutant: the reported depth point is the box centre, not the ray's entry point
Picked pickCentrePoint(const Scene& sc)
{
    Picked r = pickReal(sc);
    for (const PBox& b : sc.mBoxes) if (b.mId == r.mId) for (int a = 0; a < 3; ++a) r.mPoint[a] = b.mBox.mCenter[a];
    return r;
}
// mutant: a box with any corner off screen (pinhole 90 degree view) is dropped, like a projected-rectangle test
Picked pickScreenRect(const Scene& sc)
{
    Scene kept = sc; kept.mBoxes.clear();
    for (const PBox& b : sc.mBoxes)
    {
        bool on = true;
        for (int corner = 0; corner < 8; ++corner)
        {
            float p[3] = { b.mBox.mCenter[0] + ((corner & 1) ? b.mBox.mHalf[0] : -b.mBox.mHalf[0]),
                           b.mBox.mCenter[1] + ((corner & 2) ? b.mBox.mHalf[1] : -b.mBox.mHalf[1]),
                           b.mBox.mCenter[2] + ((corner & 4) ? b.mBox.mHalf[2] : -b.mBox.mHalf[2]) };
            const float fwd = p[1] - sc.mEye[1];
            if (fwd <= 0.1f || std::fabs(p[0] - sc.mEye[0]) > fwd || std::fabs(p[2] - sc.mEye[2]) > fwd) on = false;
        }
        PBox moved = b;
        if (!on) moved.mBox = mkBox(0, -1000, 0, 1, 1, 1);       // no box hit; the probe candidate stays
        kept.mBoxes.push_back(moved);
    }
    return pickReal(kept);
}
// mutant: the near plane's forward depth is compared with the ray distance (no 1 / cos)
Picked pickRawNear(const Scene& sc)
{
    Cands c;
    for (const PBox& b : sc.mBoxes)
    {
        PickCandidate<int> k; k.mId = b.mId; k.mProbeD2 = b.mProbeD2; float t = 0.f;
        if (rayBoxEntry(sc.mEye, sc.mDir, b.mBox, sc.mNear, t)) { k.mHasBox = true; k.mEntryT = t; }
        c.push_back(k);
    }
    Picked r = pickReal(sc);
    r.mId = choosePickedClone<int>(c, 0);
    float near_t = 0.f, t = 0.f; (void)near_t;
    for (const PBox& b : sc.mBoxes) if (b.mId == r.mId && rayBoxEntry(sc.mEye, sc.mDir, b.mBox, sc.mNear, t)) entryPoint(sc, t, r.mPoint);
    return r;
}
// mutant: nearest probe only
Picked pickProbeOnly(const Scene& sc)
{
    Picked r; r.mId = 0; r.mPoint[0] = r.mPoint[1] = r.mPoint[2] = 0.f; float best = -1.f;
    for (const PBox& b : sc.mBoxes) if (b.mProbeD2 >= 0.f && (best < 0.f || b.mProbeD2 < best)) { best = b.mProbeD2; r.mId = b.mId; }
    return r;
}
// mutant: no probe fallback when no box is hit
Picked pickNoFallback(const Scene& sc)
{
    Picked r = pickReal(sc);
    float t = 0.f; bool any = false;
    for (const PBox& b : sc.mBoxes) any = any || rayBoxEntry(sc.mEye, sc.mDir, b.mBox, sc.mNear, t);
    if (!any) r.mId = 0;
    return r;
}

bool near3(const float* a, const float* b) { return dist3(a, b) < .01f; }
bool pickOk(PickFn pick)
{
    // Camera at the origin looking along +Y (unit direction straight ahead, or tilted by the scene).
    Scene sc; sc.mEye[0] = sc.mEye[1] = sc.mEye[2] = 0.f; sc.mDir[0] = 0.f; sc.mDir[1] = 1.f; sc.mDir[2] = 0.f;
    // (1) A neighbour's probe is nearer the cursor, but the ray only enters A: A.
    sc.mBoxes = { { 1, mkBox(0, 10, 0, .4f, .4f, 1), 400.f }, { 2, mkBox(3, 10, 0, .4f, .4f, 1), 25.f } };
    if (pick(sc).mId != 1) return false;
    // (2) Ranking is by ENTRY depth: a big clone entered at t = 5 (centre at 9) beats a small one entered at t = 7.5
    // whose centre (8) is nearer the eye; the reported point is the ENTRY point on the big clone.
    sc.mBoxes = { { 1, mkBox(0, 9, 0, 2, 4, 2), 900.f }, { 2, mkBox(0, 8, 0, .5f, .5f, .5f), 25.f } };
    { const Picked r = pick(sc); const float entry[3] = { 0, 5, 0 }; if (r.mId != 1 || !near3(r.mPoint, entry)) return false; }
    // (3) The eye inside a box: that box is not a candidate; the other clone is picked, never the one we stand in.
    sc.mBoxes = { { 1, mkBox(0, 0, 0, 3, 3, 3), 900.f }, { 2, mkBox(0, 10, 0, .4f, .4f, 1), 25.f } };
    if (pick(sc).mId != 2) return false;
    // (3b) Standing inside the only box: no box hit, so the probe rule answers (here a probe within the radius).
    sc.mBoxes = { { 1, mkBox(0, 0, 0, 3, 3, 3), 49.f } };
    if (pick(sc).mId != 1) return false;
    // (4) A box crossing the top screen edge (90 degree view: |z| <= y) is still picked by the ray through its lower part.
    sc.mBoxes = { { 1, mkBox(0, 5, 0, 1, 1, 8), -1.f } };
    if (pick(sc).mId != 1) return false;
    // (5) Occlusion depth: the reported point is the entry on the clone, in front of a wall hit 6 m away.
    sc.mBoxes = { { 1, mkBox(0, 4.5f, 0, 1, 0.5f, 1), -1.f } };
    { const Picked r = pick(sc); const float entry[3] = { 0, 4, 0 }; if (r.mId != 1 || !near3(r.mPoint, entry) || !(r.mPoint[1] < 6.f)) return false; }
    // (6) Outside every box: the nearest probe in the radius still catches the click; none in the radius: no pick.
    sc.mBoxes = { { 1, mkBox(5, 10, 0, .4f, .4f, 1), 144.f }, { 2, mkBox(9, 10, 0, .4f, .4f, 1), 900.f } };
    if (pick(sc).mId != 1) return false;
    sc.mBoxes = { { 1, mkBox(5, 10, 0, .4f, .4f, 1), -1.f } };
    if (pick(sc).mId != 0) return false;
    // (7) Behind the eye: not a hit.
    sc.mBoxes = { { 1, mkBox(0, -10, 0, 1, 1, 1), -1.f } };
    if (pick(sc).mId != 0) return false;
    // (8) Near plane off axis: forward +Y, ray (0.8, 0.6, 0), near 0.1 -> the plane is at ray distance 0.1667. A small
    // box spanning X [0.09, 0.11], Y [0.06, 0.08] (ray distance 0.1125 .. 0.1333) is wholly nearer: NOT a hit.
    sc.mDir[0] = .8f; sc.mDir[1] = .6f; sc.mDir[2] = 0.f;
    sc.mBoxes = { { 1, mkBox(.1f, .07f, 0, .01f, .01f, .01f), -1.f } };
    if (pick(sc).mId != 0) return false;
    // (9) The near plane cuts a box: it is a hit, entered AT the near plane's ray distance (0.1667), not before.
    sc.mBoxes = { { 1, mkBox(.2f, .15f, 0, .1f, .1f, .1f), -1.f } };
    { const Picked r = pick(sc); float want[3]; entryPoint(sc, .1f / .6f, want); if (r.mId != 1 || !near3(r.mPoint, want)) return false; }
    // (10) A ray parallel to the near plane (cos <= 0) never hits a box.
    sc.mDir[0] = 1.f; sc.mDir[1] = 0.f; sc.mDir[2] = 0.f;
    sc.mBoxes = { { 1, mkBox(5, 0, 0, 1, 1, 1), -1.f } };
    return pick(sc).mId == 0;
}

// Resized clones: the box is the clone's scaled body (BoxFn); the ray runs through the DRAWN body and through the
// region the unscaled body would have covered.
typedef BodyBox (*BoxFn)(const float*, float);
BodyBox boxScaled(const float* foot, float scale) { return scaledBodyBox(foot, scale); }
BodyBox boxUnscaled(const float* foot, float) { return scaledBodyBox(foot, 1.f); }   // mutant: ignores the clone's size
BodyBox boxRendered(const float* foot, float scale)                                   // rendered extents of a 0.35 x 1 x 2 m body
{
    float lo[3] = { foot[0] - .35f, foot[1] - .35f, foot[2] }, hi[3] = { foot[0] + .35f, foot[1] + .35f, foot[2] + 2.f };
    scaleExtentsAboutPivot(lo, hi, foot, scale);
    BodyBox b;
    for (int a = 0; a < 3; ++a) { b.mCenter[a] = .5f * (lo[a] + hi[a]); b.mHalf[a] = .5f * (hi[a] - lo[a]); }
    return b;
}
int resizedPick(BoxFn box, float scale, float height)
{
    const float foot[3] = { 0.f, 10.f, 0.f };
    Scene sc; sc.mEye[0] = sc.mEye[1] = 0.f; sc.mEye[2] = 0.f;          // eye at foot height, ray aimed at (0, 10, height)
    const float len = std::sqrt(100.f + height * height);
    sc.mDir[0] = 0.f; sc.mDir[1] = 10.f / len; sc.mDir[2] = height / len;
    sc.mBoxes = { { 1, box(foot, scale), -1.f } };
    return pickReal(sc).mId;
}
bool resizedPickOk(BoxFn box)
{
    // 0.5x: the drawn body is 1 m tall: its head (0.9 m) is picked, the unscaled region (1.7 m up) is not.
    if (resizedPick(box, .5f, .9f) != 1 || resizedPick(box, .5f, 1.7f) != 0) return false;
    // 2x: the drawn body is 4 m tall: its head (3.8 m) is picked, 4.6 m above is not.
    if (resizedPick(box, 2.f, 3.8f) != 1 || resizedPick(box, 2.f, 4.6f) != 0) return false;
    return resizedPick(box, 1.f, 1.9f) == 1 && resizedPick(box, 1.f, 2.6f) == 0;
}

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

// ---- Look / turn, Animation and Appearance lists ---------------------------------
static const char* const UUID_A = "01234567-89ab-cdef-0123-456789abcdef";

// Nearby people: nearest first, capped. Entry needs mDistance.
struct Near { double mDistance; std::string mName; };
typedef std::vector<Near> NearVec;
typedef NearVec (*NearFn)(NearVec);
NearVec nearReal(NearVec v) { return nearbySelect(v); }
NearVec nearUnsorted(NearVec v) { if (v.size() > NEARBY_MAX) v.resize(NEARBY_MAX); return v; }               // list order, not distance
NearVec nearUncapped(NearVec v)
{ std::stable_sort(v.begin(), v.end(), [](const Near& a, const Near& b) { return a.mDistance < b.mDistance; }); return v; }
NearVec nearFarthestFirst(NearVec v)
{ std::stable_sort(v.begin(), v.end(), [](const Near& a, const Near& b) { return a.mDistance > b.mDistance; }); if (v.size() > NEARBY_MAX) v.resize(NEARBY_MAX); return v; }
bool nearbyOk(NearFn fn)
{
    NearVec v;
    for (int i = 0; i < 20; ++i) v.push_back({ double((i * 7) % 20), "p" + std::to_string(i) });        // a permutation of 0..19
    const NearVec r = fn(v);
    if (r.size() != NEARBY_MAX) return false;                                                           // capped at 16
    for (size_t i = 0; i < r.size(); ++i) if (r[i].mDistance != double(i)) return false;                // the 16 nearest, nearest first
    const NearVec few = fn(NearVec{ { 5, "b" }, { 1, "a" } });
    return few.size() == 2 && few[0].mName == "a";
}

// A nearby person's label: a name, never an id.
typedef std::string (*NearLabelFn)(const std::string& name, bool cast, const std::string& suffix, const std::string& id);
std::string labelReal(const std::string& n, bool c, const std::string& s, const std::string&) { return nearbyLabel(n, c, s); }
std::string labelFallsBackToId(const std::string& n, bool c, const std::string& s, const std::string& id)
{ return n.empty() ? id : nearbyLabel(n, c, s); }                                                       // lists a nameless person by UUID
std::string labelNoUuidGuard(const std::string& n, bool c, const std::string& s, const std::string&)
{ return n.empty() ? std::string() : (c && !s.empty() ? n + " (" + s + ")" : n); }                      // a UUID "name" passes through
bool nearbyLabelOk(NearLabelFn f)
{
    if (f("Alice", false, "cast", UUID_A) != "Alice") return false;
    if (f("Alice", true, "cast", UUID_A) != "Alice (cast)") return false;                              // cast members are marked
    for (const char* name : { "", UUID_A, "0123456789abcdef0123456789abcdef" })
    {
        const std::string r = f(name, false, "cast", UUID_A);
        if (!r.empty() || containsUuidText(r)) return false;                                            // left out, never listed by id
    }
    return true;
}

// Recent animations: newest first, one per animation, capped.
struct Recent { int mAsset = 0; std::string mName; };
typedef std::vector<Recent> RecentVec;
typedef void (*RecentPushFn)(RecentVec&, int, const std::string&);
void recentReal(RecentVec& l, int a, const std::string& n) { recentPush(l, a, n); }
void recentNoDedupe(RecentVec& l, int a, const std::string& n)
{ Recent e; e.mAsset = a; e.mName = n; l.insert(l.begin(), e); if (l.size() > RECENT_ANIM_MAX) l.resize(RECENT_ANIM_MAX); }
void recentOldestFirst(RecentVec& l, int a, const std::string& n)
{ Recent e; e.mAsset = a; e.mName = n; l.push_back(e); if (l.size() > RECENT_ANIM_MAX) l.erase(l.begin()); }
void recentUncapped(RecentVec& l, int a, const std::string& n)
{ for (size_t i = 0; i < l.size();) { if (l[i].mAsset == a) l.erase(l.begin() + i); else ++i; } Recent e; e.mAsset = a; e.mName = n; l.insert(l.begin(), e); }
bool recentOk(RecentPushFn push)
{
    RecentVec l;
    for (int i = 1; i <= 10; ++i) push(l, i, "anim" + std::to_string(i));
    if (l.size() != RECENT_ANIM_MAX || l.front().mAsset != 10 || l.back().mAsset != 3) return false;   // newest first, the last 8
    push(l, 5, "anim5");                                                                                // replaying moves it to the front
    if (l.size() != RECENT_ANIM_MAX || l.front().mAsset != 5) return false;
    int fives = 0;
    for (const Recent& e : l) fives += e.mAsset == 5 ? 1 : 0;
    return fives == 1;
}

// A recent animation's label: its name only, never the asset id.
typedef std::string (*RecentLabelFn)(const std::string& name, const std::string& fallback, const std::string& asset);
std::string recentLabelReal(const std::string& n, const std::string& f, const std::string&) { return recentLabel(n, f); }
std::string recentLabelShowsAsset(const std::string& n, const std::string&, const std::string& a) { return n.empty() ? a : n; }
std::string recentLabelNoGuard(const std::string& n, const std::string& f, const std::string&) { return n.empty() ? f : n; }
bool recentLabelOk(RecentLabelFn fn)
{
    if (fn("Wave", "Animation", UUID_A) != "Wave") return false;
    for (const char* name : { "", UUID_A })
    {
        const std::string r = fn(name, "Animation", UUID_A);
        if (r != "Animation" || containsUuidText(r)) return false;
    }
    return true;
}
bool recentVisibleOk(bool (*visible)(size_t)) { return !visible(0) && visible(1) && visible(8); }
bool badRecentAlwaysVisible(size_t) { return true; }

// Look / turn / animation actions touch EXACTLY the picked-or-selected set.
typedef Ids (*LookScopeFn)(int picked, const Ids& selection, const Ids& all);
Ids lookReal(int picked, const Ids& sel, const Ids&) { return planTargets(picked, sel).mTargets; }
Ids lookEveryClone(int, const Ids&, const Ids& all) { return all; }                                      // a look target set outside the picked clones
Ids lookPickedOnly(int picked, const Ids&, const Ids&) { return Ids{ picked }; }                         // ignores the multi-selection
Ids lookSelectionOnly(int picked, const Ids& sel, const Ids&) { return sel.empty() ? Ids{ picked } : sel; } // acts on a selection the picked clone is not in
Ids lookOneExtra(int picked, const Ids& sel, const Ids& all)
{ Ids r = planTargets(picked, sel).mTargets; r.push_back(all.back()); return r; }
bool lookScopeOk(LookScopeFn fn)
{
    const Ids all{ 1, 2, 3, 4, 5 };
    return targetsExact(2, Ids{}, fn(2, Ids{}, all)) &&
           targetsExact(2, Ids{ 1, 2, 3 }, fn(2, Ids{ 1, 2, 3 }, all)) &&
           targetsExact(2, Ids{ 1, 3 }, fn(2, Ids{ 1, 3 }, all));
}

// ---- Fade out / Fade in, Look at clears Turn -------------------------------------------
typedef bool (*FadeEligibleFn)(bool fade_out, bool hidden);
bool eligibleReal(bool o, bool h) { return fadeEligible(o, h); }
bool eligibleAlways(bool, bool) { return true; }                                     // Fade out on an already faded-out clone
bool eligibleSwapped(bool o, bool h) { return fadeEligible(!o, h); }
bool fadeEligibleOk(FadeEligibleFn f)
{
    return f(true, false) && !f(true, true) &&                                       // Fade out: not an already faded-out clone
           f(false, true) && !f(false, false);                                       // Fade in: only a clone the user hid
}
// Fade acts on ENTITY clones only: an overlay hidden from the menu would have no way back.
typedef bool (*FadeAppliesFn)(bool entity, bool fade_out, bool hidden);
bool appliesReal(bool e, bool o, bool h) { return fadeApplies(e, o, h); }
bool appliesOverlaysToo(bool, bool o, bool h) { return fadeEligible(o, h); }          // an overlay faded out and stranded
bool fadeAppliesOk(FadeAppliesFn f)
{
    return f(true, true, false) && f(true, false, true) && !f(false, true, false) && !f(false, false, true);
}
// A hidden clone is a resting state, never an inferred mid-ramp coverage: undo restores the state the clone
// was heading to, and a swap / respawn keeps it hidden.
struct FadeClone { bool hidden = false; double alpha = 1.0; };                       // alpha is the instantaneous coverage
typedef bool (*FadeSaveFn)(const FadeClone&);
bool saveRestingReal(const FadeClone& c) { return c.hidden; }
bool saveMidRamp(const FadeClone& c) { return c.alpha < 0.5; }                       // infers "hidden" from the instant coverage
bool fadeUndoOk(FadeSaveFn save)
{
    FadeClone fadingIn; fadingIn.hidden = false; fadingIn.alpha = 0.2;               // a crowd fade-in at 20 percent, then Fade out
    FadeClone ramping; ramping.hidden = true; ramping.alpha = 0.8;                   // a Fade out at 80 percent, then Fade in
    return !save(fadingIn) && save(ramping);
}
typedef double (*ReplaceFn)(const FadeClone&);                                       // coverage on the replaced body
double replaceKeepsHidden(const FadeClone& c) { return c.hidden ? 0.0 : 1.0; }
double replaceRevealsHidden(const FadeClone&) { return 1.0; }
bool replaceKeepsHiddenOk(ReplaceFn f)
{
    FadeClone hidden; hidden.hidden = true; hidden.alpha = 0.0;
    FadeClone shown;
    return f(hidden) == 0.0 && f(shown) == 1.0;
}
// A hidden clone intercepts no click and shows no nameplate.
typedef bool (*PickableFn)(bool visible, bool deleting, bool hidden);
bool pickableReal(bool v, bool d, bool h) { return v && !d && !h; }
bool pickableIgnoresHidden(bool v, bool d, bool) { return v && !d; }
bool pickableOk(PickableFn f) { return f(true, false, false) && !f(true, false, true) && !f(false, false, false) && !f(true, true, false); }
// A finished fade removes its clone only when the clone was marked deleting.
typedef bool (*FadeRemovesFn)(bool removing, bool deleting);
bool removesReal(bool r, bool d) { return fadeRemovesClone(r, d); }
bool removesWheneverFadingOut(bool r, bool) { return r; }                            // a hidden-only fade deleting its clone
bool fadeNeverDeletesOk(FadeRemovesFn f)
{
    return f(true, true) &&                                                          // a delete fade still deletes
           !f(true, false) &&                                                        // Fade out only hides
           !f(false, false) && !f(false, true);                                      // Fade in never removes
}
// Scene data: a fade is session-only, so no scene key mentions it.
typedef std::vector<std::string> (*SceneKeysFn)();
std::vector<std::string> sceneKeysReal() { return { "actors", "groups", "subjects" }; }
std::vector<std::string> sceneKeysWithFade() { return { "actors", "groups", "fadeAlpha" }; }
bool sceneHasNoFadeOk(SceneKeysFn f)
{
    for (const std::string& key : f()) if (key.find("ade") != std::string::npos) return false;
    return true;
}
// Look at disarms an armed Turn, then looks; Turn to face leaves the look alone.
struct FaceInst { bool turnArmed = true; bool lookSet = false; };
typedef void (*LookAtFn)(FaceInst&);
void lookAtReal(FaceInst& i) { i.turnArmed = false; i.lookSet = true; }              // stopTurning, then setLookTarget
void lookAtKeepsTurn(FaceInst& i) { i.lookSet = true; }                              // look stored, turn still owns the body
bool lookAtOk(LookAtFn f)
{
    FaceInst i; f(i);
    return i.lookSet && !i.turnArmed;
}

// An array / crowd copy of a HIDDEN prototype inherits the hidden flag and fades in like any new clone; a copy of a visible
// prototype keeps the creation fade-in it already started (a jump would cancel it).
struct CopyState { bool hidden = false; bool ramping = false; };
typedef void (*FormationCopyFn)(CopyState&, bool proto_hidden);
void copyReal(CopyState& c, bool proto_hidden)
{
    c.hidden = proto_hidden; c.ramping = !proto_hidden;               // beginAppearance: hidden stays at 0, a visible copy starts its creation ramp
    if (proto_hidden) { c.hidden = false; c.ramping = true; }          // fadeInstance(copy, false, true)
}
void copyAlwaysJumps(CopyState& c, bool proto_hidden)
{
    c.hidden = proto_hidden; c.ramping = !proto_hidden;
    c.hidden = false; c.ramping = false;                                // fadeInstance(copy, false, false) on EVERY copy: cancels the creation ramp
}
void copyHiddenPops(CopyState& c, bool proto_hidden)
{
    c.hidden = proto_hidden; c.ramping = !proto_hidden;
    if (proto_hidden) { c.hidden = false; c.ramping = false; }          // the copy of a hidden prototype pops in instead of fading in
}
bool formationCopyOk(FormationCopyFn f)
{
    CopyState visible; f(visible, false);
    CopyState hidden; f(hidden, true);
    return !visible.hidden && visible.ramping && !hidden.hidden && hidden.ramping;
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

    // Look / turn, Animation, Appearance
    expect(containsUuidText(UUID_A) && containsUuidText(std::string("x ") + UUID_A + " y") &&
           containsUuidText("0123456789abcdef0123456789abcdef") && !containsUuidText("Alice Resident") &&
           !containsUuidText("0123456789abcdef0123456789abcde") && !containsUuidText("deadbeef-dead-beef") &&
           !containsUuidText("01234567-89ab-cdef-0123-456789abcde"), "UUID text detector: dashed and 32-digit forms only");
    expect(NEARBY_MAX == 16 && RECENT_ANIM_MAX == 8, "caps: 16 people, 8 recent animations");
    expect(nearbyOk(nearReal), "Nearby people: the 16 nearest, nearest first");
    expect(!nearbyOk(nearUnsorted), "REJECT: Nearby people in list order instead of nearest first");
    expect(!nearbyOk(nearUncapped), "REJECT: Nearby people without a cap");
    expect(!nearbyOk(nearFarthestFirst), "REJECT: Nearby people farthest first");
    expect(nearbyLabelOk(labelReal), "Nearby label: display name, cast suffix, nameless or UUID names left out");
    expect(!nearbyLabelOk(labelFallsBackToId), "REJECT: Nearby list showing a UUID for a nameless person");
    expect(!nearbyLabelOk(labelNoUuidGuard), "REJECT: Nearby list showing a name that is a UUID");
    expect(recentOk(recentReal), "Recent animations: newest first, one per animation, the last 8");
    expect(!recentOk(recentNoDedupe), "REJECT: Recent animations listing a replayed animation twice");
    expect(!recentOk(recentOldestFirst), "REJECT: Recent animations oldest first");
    expect(!recentOk(recentUncapped), "REJECT: Recent animations without a cap");
    expect(recentLabelOk(recentLabelReal), "Recent label: the name only");
    expect(!recentLabelOk(recentLabelShowsAsset), "REJECT: Recent list exposing an asset id");
    expect(!recentLabelOk(recentLabelNoGuard), "REJECT: Recent list showing a name that is the asset id");
    expect(recentVisibleOk(recentSubmenuVisible), "Recent submenu is hidden when empty");
    expect(!recentVisibleOk(badRecentAlwaysVisible), "REJECT: Recent submenu visible when empty");
    expect(lookScopeOk(lookReal), "look / turn / animation actions touch exactly the picked-or-selected clones");
    expect(!lookScopeOk(lookEveryClone), "REJECT: look target set on every clone");
    expect(!lookScopeOk(lookPickedOnly), "REJECT: look ignoring the multi-selection the picked clone belongs to");
    expect(!lookScopeOk(lookSelectionOnly), "REJECT: look acting on a selection the picked clone is not in");
    expect(!lookScopeOk(lookOneExtra), "REJECT: look touching a clone outside the targets");

    expect(fadeEligibleOk(eligibleReal), "Fade out takes clones that are not user-hidden, Fade in clones that are");
    expect(!fadeEligibleOk(eligibleAlways), "REJECT: Fade out enabled on a clone that is already faded out");
    expect(!fadeEligibleOk(eligibleSwapped), "REJECT: Fade in enabled on a clone that is shown");
    expect(fadeAppliesOk(appliesReal), "Fade acts on entity clones only");
    expect(!fadeAppliesOk(appliesOverlaysToo), "REJECT: Fade out on an overlay clone (stranded with no way back)");
    expect(fadeUndoOk(saveRestingReal), "Fade undo restores the resting state the clone was heading to");
    expect(!fadeUndoOk(saveMidRamp), "REJECT: Fade undo inferring the state from a mid-ramp coverage");
    expect(replaceKeepsHiddenOk(replaceKeepsHidden), "a swap / respawn keeps a user-hidden clone hidden");
    expect(!replaceKeepsHiddenOk(replaceRevealsHidden), "REJECT: a swap or respawn revealing a hidden clone");
    expect(pickableOk(pickableReal), "a hidden clone is not picked and shows no nameplate");
    expect(!pickableOk(pickableIgnoresHidden), "REJECT: a hidden clone intercepting clicks");
    expect(fadeNeverDeletesOk(removesReal), "a hidden-only fade ends at alpha 0; only a clone marked deleting is removed");
    expect(!fadeNeverDeletesOk(removesWheneverFadingOut), "REJECT: Fade out deleting a clone");
    expect(sceneHasNoFadeOk(sceneKeysReal), "no scene key carries a fade");
    expect(!sceneHasNoFadeOk(sceneKeysWithFade), "REJECT: fade state saved in a scene");
    expect(lookAtOk(lookAtReal), "Look at clears an armed Turn and then looks");
    expect(!lookAtOk(lookAtKeepsTurn), "REJECT: Look at leaving an armed Turn");

    expect(formationCopyOk(copyReal), "an array / crowd copy of a hidden prototype fades in; a visible prototype's copy keeps its creation ramp");
    expect(!formationCopyOk(copyAlwaysJumps), "REJECT: array / crowd copies losing their creation fade-in (visible-prototype copy jumps)");
    expect(!formationCopyOk(copyHiddenPops), "REJECT: a copy of a hidden prototype popping in instead of fading in");

    expect(pickOk(pickReal), "the cursor's world ray picks the clone it enters first, reports the entry point, skips the box the eye is in, and falls back to the nearest probe");
    expect(!pickOk(pickProbeOnly), "REJECT: nearest-probe pick (a neighbour or the clone picked last wins over the clone under the cursor)");
    expect(!pickOk(pickCentreRank), "REJECT: overlapping clones ranked by centre distance instead of ray entry depth");
    expect(!pickOk(pickEyeInside), "REJECT: standing inside a clone's box still picks it");
    expect(!pickOk(pickCentrePoint), "REJECT: the reported click depth is the box centre, not the ray entry point");
    expect(!pickOk(pickRawNear), "REJECT: the near plane's forward depth compared with the ray distance (off-axis clipping wrong)");
    expect(!pickOk(pickScreenRect), "REJECT: a box crossing the screen edge is dropped (projected-rectangle pick)");
    expect(!pickOk(pickNoFallback), "REJECT: no probe fallback outside every box");
    expect(resizedPickOk(boxScaled), "the click region follows a resized clone (0.5x, 1x, 2x): the drawn body picks, the unscaled region does not");
    expect(resizedPickOk(boxRendered), "rendered extents scaled about the foot pivot give the same region as the scaled body box");
    expect(!resizedPickOk(boxUnscaled), "REJECT: a click region that ignores the clone's size");

    if (failures) return 1;
    std::printf("PASS: clone menu logic, 61 planted negatives rejected\n");
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
             "lltoolmgr.cpp", "llagentcamera.cpp", "lltooldraganddrop.cpp", "alghoststudio.cpp",
             "lldirectorcast.cpp", "lldirectorcast.h", "llghostavatar.cpp"]
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
// clone transitions (fix round 1): applyCrowdVariation cancels affected transitions before a look assignment; none runs here
namespace ALCloneTransition { enum Reason { R_EDIT_LOCK }; }
struct ALCloneTransitions { static bool anyRunning() { return false; } static void cancelFor(const LLUUID&, ALCloneTransition::Reason) {} };
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

RECORD_FIXTURE = r"""
#include "alghostclonemenulogic.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>
struct LLUUID
{
    int v = 0;
    static const LLUUID null;
    bool isNull() const { return v == 0; }
    bool notNull() const { return v != 0; }
    void setNull() { v = 0; }
    bool operator==(const LLUUID& o) const { return v == o.v; }
    bool operator!=(const LLUUID& o) const { return v != o.v; }
    bool operator<(const LLUUID& o) const { return v < o.v; }
};
const LLUUID LLUUID::null;
struct LLVector3d {};
namespace ALGhostGroupModel
{
struct Member { LLUUID mInstanceId; };
struct Group { LLUUID mId; std::vector<Member> mMembers; bool mEditMembers = false; };
}
struct GroupsMock
{
    std::vector<ALGhostGroupModel::Group> mList;
    std::vector<LLUUID> authoringUnitMembers(const LLUUID& id) const
    {
        for (const auto& g : mList) if (g.mId == id) { std::vector<LLUUID> r; for (const auto& m : g.mMembers) r.push_back(m.mInstanceId); return r; }
        return std::vector<LLUUID>(1, id);
    }
};
// Every assignment to a look field is recorded against its owner, even when it writes the value already there.
static std::set<int>* gWrites = nullptr;
template <class T> struct Rec
{
    T v{};
    const int* owner = nullptr;
    Rec& operator=(const T& x) { v = x; if (gWrites && owner) gWrites->insert(*owner); return *this; }
    operator T() const { return v; }
};
struct ALGhostStudio
{
    enum ELookTarget { LOOK_TARGET_CAMERA, LOOK_TARGET_ME, LOOK_TARGET_ACTOR };
    enum ETurnMode { TURN_MODE_OFF, TURN_MODE_ONCE, TURN_MODE_TRACK };
    enum { BACKING_OVERLAY, BACKING_ENTITY_CLONE };
    struct Instance
    {
        LLUUID mId; bool mDeleting = false; int mKind = BACKING_ENTITY_CLONE;
        Rec<ELookTarget> mLookTarget; Rec<LLUUID> mLookTargetId; Rec<bool> mKeepFacing;
        ETurnMode mTurnMode = TURN_MODE_OFF; bool mUserHidden = false;
        void bind() { mLookTarget.owner = &mId.v; mLookTargetId.owner = &mId.v; mKeepFacing.owner = &mId.v; }
    };
    std::vector<Instance> mInstances;
    GroupsMock mGroups;
    std::set<int> mCalled;                                   // ids a call was made for (face / restart / fade / turn)
    static ALGhostStudio& instance() { static ALGhostStudio s; return s; }
    const std::vector<Instance>& getInstances() const { return mInstances; }
    Instance* getInstance(const LLUUID& id) { for (auto& i : mInstances) if (i.mId == id) return &i; return nullptr; }
    const Instance* getInstance(const LLUUID& id) const { for (auto& i : mInstances) if (i.mId == id) return &i; return nullptr; }
    bool isDeleting(const LLUUID& id) const { const Instance* i = getInstance(id); return i && i->mDeleting; }
    const ALGhostGroupModel::Group* groupForMember(const LLUUID& id) const
    {
        for (const auto& g : mGroups.mList)
        {
            if (g.mId == id) return &g;
            for (const auto& m : g.mMembers) if (m.mInstanceId == id) return &g;
        }
        return nullptr;
    }
    std::vector<LLUUID> groupMembers(const LLUUID& id) const;
    void setLookTarget(const LLUUID& id, ELookTarget target, const LLUUID& target_id, bool keep_facing);
    bool faceInstance(const LLUUID& id) { mCalled.insert(id.v); return true; }
    void setTurnTarget(const LLUUID& id, ETurnMode mode, ELookTarget target, const LLUUID& target_id, const LLVector3d&)
    { mCalled.insert(id.v); if (Instance* i = getInstance(id)) { i->mTurnMode = mode; (void)target; (void)target_id; } }
    bool restartInstanceAnimation(const LLUUID& id) { const Instance* i = getInstance(id); if (!i || i->mKind != BACKING_ENTITY_CLONE) return false; mCalled.insert(id.v); return true; }
    bool fadeInstance(const LLUUID& id, bool hidden, bool) { Instance* i = getInstance(id); if (!i || i->mDeleting || i->mKind != BACKING_ENTITY_CLONE) return false; i->mUserHidden = hidden; mCalled.insert(id.v); return true; }
};
@STUDIO@
namespace ALGhostCloneActions
{
void stopTurning(const std::vector<LLUUID>& rows);
@ACTIONS@
}
typedef std::set<int> Set;
static int failures = 0;
static void expect(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++failures; } }
static LLUUID id(int v) { LLUUID u; u.v = v; return u; }
static std::vector<LLUUID> rows(std::initializer_list<int> v) { std::vector<LLUUID> r; for (int i : v) r.push_back(id(i)); return r; }
struct Snap { int t; int tid; bool keep; ALGhostStudio::ETurnMode turn; bool hidden; };
static std::map<int, Snap> snap(ALGhostStudio& s)
{
    std::map<int, Snap> m;
    for (auto& i : s.mInstances) m[i.mId.v] = { (int)(ALGhostStudio::ELookTarget)i.mLookTarget, ((LLUUID)i.mLookTargetId).v, (bool)i.mKeepFacing, i.mTurnMode, i.mUserHidden };
    return m;
}
// Every instance whose state changed, every look field written (even with the same value) and every id a studio call was made for.
static Set touched(ALGhostStudio& s, const std::map<int, Snap>& before, const Set& writes)
{
    Set out = s.mCalled;
    out.insert(writes.begin(), writes.end());
    for (auto& i : s.mInstances)
    {
        const Snap& b = before.at(i.mId.v);
        if (b.t != (int)(ALGhostStudio::ELookTarget)i.mLookTarget || b.tid != ((LLUUID)i.mLookTargetId).v || b.keep != (bool)i.mKeepFacing || b.turn != i.mTurnMode || b.hidden != i.mUserHidden) out.insert(i.mId.v);
    }
    return out;
}
static void world(ALGhostStudio& s)
{
    s = ALGhostStudio();
    for (int v : { 1, 2, 3 }) { ALGhostStudio::Instance i; i.mId = id(v); s.mInstances.push_back(i); }          // ungrouped A B C
    for (int v : { 11, 12, 13 }) { ALGhostStudio::Instance i; i.mId = id(v); i.mTurnMode = ALGhostStudio::TURN_MODE_ONCE; s.mInstances.push_back(i); }   // collapsed group members
    for (int v : { 21, 22, 23 }) { ALGhostStudio::Instance i; i.mId = id(v); i.mTurnMode = ALGhostStudio::TURN_MODE_ONCE; s.mInstances.push_back(i); }   // expanded group members
    { ALGhostStudio::Instance i; i.mId = id(31); i.mKind = ALGhostStudio::BACKING_OVERLAY; s.mInstances.push_back(i); }  // an overlay
    for (auto& i : s.mInstances) i.bind();                                                                      // the vector no longer grows
    ALGhostGroupModel::Group collapsed; collapsed.mId = id(100); collapsed.mEditMembers = false;
    for (int v : { 11, 12, 13 }) { ALGhostGroupModel::Member m; m.mInstanceId = id(v); collapsed.mMembers.push_back(m); }
    ALGhostGroupModel::Group expanded; expanded.mId = id(200); expanded.mEditMembers = true;
    for (int v : { 21, 22, 23 }) { ALGhostGroupModel::Member m; m.mInstanceId = id(v); expanded.mMembers.push_back(m); }
    s.mGroups.mList = { collapsed, expanded };
}
// The complete touched set of one action on one selection must be EXACTLY the targets (empty when the selection is refused).
static void scenario(const char* name, const std::vector<LLUUID>& sel, const Set& want_look, const Set& want_turn, const Set& want_restart)
{
    ALGhostStudio& s = ALGhostStudio::instance();
    Set writes;
    gWrites = &writes;
    { world(s); writes.clear(); auto b = snap(s); ALGhostCloneActions::lookAt(sel, ALGhostStudio::LOOK_TARGET_ACTOR, id(77)); Set t = touched(s, b, writes);
      expect(t == want_look, (std::string(name) + ": Look at touches exactly the targets").c_str()); }
    { world(s); for (auto& i : s.mInstances) i.mKeepFacing = true; writes.clear(); auto b = snap(s); ALGhostCloneActions::stopLooking(sel); Set t = touched(s, b, writes);
      expect(t == want_look, (std::string(name) + ": Stop looking touches exactly the targets").c_str()); }
    { world(s); writes.clear(); auto b = snap(s); ALGhostCloneActions::turnTo(sel, ALGhostStudio::LOOK_TARGET_ME, LLUUID::null); Set t = touched(s, b, writes);
      expect(t == want_turn, (std::string(name) + ": Turn to face touches exactly the targets").c_str()); }
    { world(s); for (auto& i : s.mInstances) i.mTurnMode = ALGhostStudio::TURN_MODE_ONCE; writes.clear(); auto b = snap(s); ALGhostCloneActions::stopTurning(sel); Set t = touched(s, b, writes);
      expect(t == want_turn, (std::string(name) + ": Stop turning touches exactly the targets").c_str()); }
    { world(s); writes.clear(); auto b = snap(s); ALGhostCloneActions::restartRows(sel); Set t = touched(s, b, writes);
      expect(t == want_restart, (std::string(name) + ": Restart animation touches exactly the entity targets").c_str()); }
    gWrites = nullptr;
}
int main()
{
    scenario("ungrouped B", rows({ 2 }), Set{ 2 }, Set{ 2 }, Set{ 2 });
    scenario("selection A+C", rows({ 1, 3 }), Set{ 1, 3 }, Set{ 1, 3 }, Set{ 1, 3 });
    scenario("collapsed group header", rows({ 100 }), Set{ 11, 12, 13 }, Set{ 11, 12, 13 }, Set{ 11, 12, 13 });
    scenario("collapsed group member row", rows({ 12 }), Set{ 11, 12, 13 }, Set{ 12 }, Set{ 12 });     // a collapsed group looks as a unit; turn is per instance
    scenario("expanded member alone", rows({ 22 }), Set{}, Set{}, Set{ 22 });                            // refused: nothing is written
    scenario("expanded group header", rows({ 200 }), Set{ 21, 22, 23 }, Set{ 21, 22, 23 }, Set{ 21, 22, 23 });   // the group row works as before
    scenario("expanded member + loose", rows({ 22, 1 }), Set{}, Set{}, Set{ 22, 1 });                   // one expanded member refuses the whole set
    scenario("expanded header + its members", rows({ 200, 21, 22, 23 }), Set{ 21, 22, 23 }, Set{ 21, 22, 23 }, Set{ 21, 22, 23 });   // the selected group row owns its members
    scenario("overlay", rows({ 31 }), Set{ 31 }, Set{ 31 }, Set{});
    {
        // Look at on ONE member row of a collapsed group writes the whole unit's look, so every sibling's armed Turn is cleared
        // too (a sibling left armed would keep owning its yaw); nothing outside the unit is touched.
        ALGhostStudio& s = ALGhostStudio::instance(); world(s);
        for (auto& i : s.mInstances) i.mTurnMode = ALGhostStudio::TURN_MODE_ONCE;
        ALGhostCloneActions::lookAt(rows({ 12 }), ALGhostStudio::LOOK_TARGET_ACTOR, id(77));
        expect(s.getInstance(id(11))->mTurnMode == ALGhostStudio::TURN_MODE_OFF && s.getInstance(id(12))->mTurnMode == ALGhostStudio::TURN_MODE_OFF &&
               s.getInstance(id(13))->mTurnMode == ALGhostStudio::TURN_MODE_OFF, "Look at on a collapsed member row clears Turn on every sibling");
        expect(s.getInstance(id(1))->mTurnMode == ALGhostStudio::TURN_MODE_ONCE && s.getInstance(id(21))->mTurnMode == ALGhostStudio::TURN_MODE_ONCE,
               "... and only on the unit the look writes");
        world(s);
        for (auto& i : s.mInstances) i.mTurnMode = ALGhostStudio::TURN_MODE_ONCE;
        ALGhostCloneActions::lookAt(rows({ 100 }), ALGhostStudio::LOOK_TARGET_ACTOR, id(77));
        expect(s.getInstance(id(11))->mTurnMode == ALGhostStudio::TURN_MODE_OFF && s.getInstance(id(13))->mTurnMode == ALGhostStudio::TURN_MODE_OFF,
               "Look at on the group header clears Turn on its members");
    }
    expect(ALGhostCloneActions::hasExpandedMember(rows({ 22 })) && !ALGhostCloneActions::hasExpandedMember(rows({ 200 })) &&
           !ALGhostCloneActions::hasExpandedMember(rows({ 12 })) && !ALGhostCloneActions::hasExpandedMember(rows({ 2 })), "only a member of an EXPANDED group is refused");
    // Fade: entity clones only, eligibility from the explicit state, undo restores the resting state
    {
        ALGhostStudio& s = ALGhostStudio::instance(); world(s);
        s.getInstance(id(2))->mUserHidden = true;                                                // B is hidden already
        auto out = ALGhostCloneActions::fadeRows(rows({ 1, 2, 31 }), true);                       // Fade out: only A (B is hidden, the overlay is never faded)
        expect(out.size() == 1 && out[0].mId == id(1) && !out[0].mHidden && s.getInstance(id(1))->mUserHidden && !s.getInstance(id(31))->mUserHidden,
               "Fade out touches only entity clones that are not hidden");
        auto in = ALGhostCloneActions::fadeRows(rows({ 1, 2, 3, 31 }), false);                    // Fade in: A and B are hidden now, C is shown, the overlay is skipped
        expect(in.size() == 2 && !s.getInstance(id(1))->mUserHidden && !s.getInstance(id(2))->mUserHidden && !s.getInstance(id(31))->mUserHidden,
               "Fade in touches only hidden entity clones");
        ALGhostCloneActions::restoreFade(in, false, false);                                       // undo: A and B hidden again
        expect(s.getInstance(id(1))->mUserHidden && s.getInstance(id(2))->mUserHidden && !s.getInstance(id(3))->mUserHidden, "undo restores the resting state");
        expect(ALGhostCloneActions::fadeRows(rows({ 31 }), true).empty(), "an overlay is never faded");
    }
    if (failures) return 1;
    std::printf("PASS: recording fixture\n");
    return 0;
}
"""

_RECORD_CACHE: dict = {}


def record_findings(src: dict[str, str]) -> list[str]:
    """Run the EXTRACTED production look / turn / restart / fade actions (and the studio's setLookTarget with its
    member-edit rule) against a recording Studio and require the complete touched-id set to equal the targets."""
    try:
        studio = "\n".join(definition(src["alghoststudio.cpp"], sig) for sig in (
            "std::vector<LLUUID> ALGhostStudio::groupMembers(", "void ALGhostStudio::setLookTarget("))
        actions = "\n".join(definition(src["alghostcloneactions.cpp"], sig) for sig in (
            "std::vector<LLUUID> instances(const std::vector<LLUUID>& rows)", "std::vector<LLUUID> units(const std::vector<LLUUID>& rows)",
            "std::vector<LLUUID> unitRepresentatives(", "bool hasExpandedMember(", "bool lookAt(", "void stopLooking(", "bool turnTo(", "void stopTurning(",
            "bool restartRows(", "std::vector<LLUUID> fadeTargets(", "std::vector<FadeBefore> fadeRows(", "bool restoreFade("))
    except ValueError:
        return ["the recording fixture could not extract the production actions"]
    actions = actions.replace("struct FadeBefore", "struct FadeBefore")
    key = (studio, actions)
    if key not in _RECORD_CACHE:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from native_check_tools import check_directory, compile_run
        head = "namespace ALGhostCloneActions { struct FadeBefore { LLUUID mId; bool mHidden = false; }; }\n"
        source = RECORD_FIXTURE.replace("@STUDIO@", head + studio).replace("@ACTIONS@", actions)
        try:
            result = compile_run(check_directory("clone_menu_record"), source, "record", flags=['/I"%s"' % NV])
            ok = result.returncode == 0 and "PASS" in result.stdout
            _RECORD_CACHE[key] = [] if ok else ["look / turn / fade actions touch more or less than their targets: " + (result.stdout + result.stderr).strip()[:400]]
        except RuntimeError as exc:
            _RECORD_CACHE[key] = ["the recording fixture did not compile: " + str(exc).strip().splitlines()[-1][:300]]
    return _RECORD_CACHE[key]

ORPHAN_FIXTURE = r"""
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
typedef float F32;
struct LLUUID
{
    int v = 0;
    bool isNull() const { return v == 0; }
    bool notNull() const { return v != 0; }
    bool operator==(const LLUUID& o) const { return v == o.v; }
    bool operator<(const LLUUID& o) const { return v < o.v; }
};
static LLUUID gAgentID;
struct LLGhostAvatar { static bool isGhostId(const LLUUID& id) { return id.v >= 1000; } };      // clones; real and control avatars are below
struct LLDirectorCast
{
    struct ActorStyle { bool mEnabled = false; int mMode = 0; int mStyle = 3; F32 mAlpha = .7f; F32 mDissolveProgress = .13f; };
    enum { ACTOR_STYLE_REPLACE = 1 };
    struct CastMember { LLUUID mId; ActorStyle mActorStyle, mCloneFadeStyle; bool mCloneFading = false; };
    std::vector<CastMember> mCast;
    std::map<LLUUID, size_t> mMemberIndex;
    ActorStyle mSelfActorStyle;
    std::map<LLUUID, ActorStyle> mCloneFadeOrphans;
    CastMember* getMember(const LLUUID& id) { auto it = mMemberIndex.find(id); return it == mMemberIndex.end() ? nullptr : &mCast[it->second]; }
    void setCloneFadeAlpha(const LLUUID& id, F32 alpha);
    void forgetCloneFade(const LLUUID& runtime_id);
    const ActorStyle& getRenderActorStyle(const LLUUID& id) const;
    const ActorStyle& resolveStoredActorStyle(const LLUUID& primary, const LLUUID& fallback, LLUUID& resolved_id) const;
    void enroll(int v) { CastMember m; m.mId.v = v; mMemberIndex[m.mId] = mCast.size(); mCast.push_back(m); }
    void drop(int v) { mMemberIndex.clear(); std::vector<CastMember> keep; for (auto& m : mCast) if (m.mId.v != v) keep.push_back(m); mCast = keep; for (size_t i = 0; i < mCast.size(); ++i) mMemberIndex[mCast[i].mId] = i; }
};
@CAST@
static int failures = 0;
static void expect(bool ok, const char* what) { if (!ok) { std::printf("FAIL %s\n", what); ++failures; } }
static LLUUID id(int v) { LLUUID u; u.v = v; return u; }
static bool isDefault(const LLDirectorCast::ActorStyle& s) { return !s.mEnabled && s.mDissolveProgress == .13f && s.mStyle == 3; }
static bool dissolve(const LLDirectorCast::ActorStyle& s, F32 want) { return s.mEnabled && s.mStyle == 10 && s.mAlpha == 1.f && s.mDissolveProgress > want - .001f && s.mDissolveProgress < want + .001f; }
int main()
{
    LLDirectorCast c;
    c.setCloneFadeAlpha(id(1001), .4f);                                                       // a clone that is not a Cast member
    expect(dissolve(c.getRenderActorStyle(id(1001)), .6f), "a non-member clone's ramp reaches the renderer getter");
    LLUUID resolved;
    expect(dissolve(c.resolveStoredActorStyle(id(1001), LLUUID(), resolved), .6f) && resolved.v == 1001, "... and the resolved getter (primary)");
    expect(dissolve(c.resolveStoredActorStyle(id(7), id(1001), resolved), .6f) && resolved.v == 1001, "a worn animesh (control avatar primary, clone fallback) fades with its non-member wearer");
    c.setCloneFadeAlpha(id(5), .4f);                                                          // a real avatar (not a clone)
    expect(isDefault(c.getRenderActorStyle(id(5))) && c.mCloneFadeOrphans.size() == 1, "a real avatar is never given a fade");
    c.setCloneFadeAlpha(id(1001), 1.f);                                                       // completion at exactly 1
    expect(isDefault(c.getRenderActorStyle(id(1001))) && c.mCloneFadeOrphans.empty(), "a completed fade (alpha 1) retires the entry");
    c.setCloneFadeAlpha(id(1001), .999f);
    expect(dissolve(c.getRenderActorStyle(id(1001)), .001f), "a barely-faded clone still carries its dissolve");
    c.forgetCloneFade(id(1001));
    expect(isDefault(c.getRenderActorStyle(id(1001))) && c.mCloneFadeOrphans.empty(), "forgetCloneFade retires the entry with its body");
    expect(isDefault(c.getRenderActorStyle(id(9))) && c.mCloneFadeOrphans.empty(), "an empty map changes nothing");
    // membership transitions
    c.enroll(1003);
    c.setCloneFadeAlpha(id(1003), .5f);
    expect(c.getMember(id(1003))->mCloneFading && c.mCloneFadeOrphans.empty() && dissolve(c.getRenderActorStyle(id(1003)), .5f), "a Cast member keeps its own path");
    c.drop(1003);                                                                              // removed from the Cast mid-ramp
    c.setCloneFadeAlpha(id(1003), .5f);
    expect(c.mCloneFadeOrphans.size() == 1 && dissolve(c.getRenderActorStyle(id(1003)), .5f), "Cast removal mid-ramp: the ramp continues without membership");
    c.enroll(1003);                                                                            // re-enrolled
    c.setCloneFadeAlpha(id(1003), .5f);
    expect(c.mCloneFadeOrphans.empty() && c.getMember(id(1003))->mCloneFading, "re-enrolment: the member carries the fade and the orphan entry is gone");
    // precedence: a Cast-member primary wins over an orphan fallback
    c.enroll(7);
    c.setCloneFadeAlpha(id(1004), .2f);
    expect(isDefault(c.resolveStoredActorStyle(id(7), id(1004), resolved)) == true && resolved.v == 7, "a Cast-member primary keeps its precedence over a fallback orphan");
    if (failures) return 1;
    std::printf("PASS: orphan fixture\n");
    return 0;
}
"""

_ORPHAN_CACHE: dict = {}


def orphan_findings(src: dict[str, str]) -> list[str]:
    """Run the EXTRACTED production fade coverage for a clone that is not a Cast member against a mock Cast: ramp reaches both
    getters (and a worn animesh through the fallback), real avatars are refused, completion and retirement erase, Cast membership
    transitions hand over without a pop, and a member primary keeps its precedence."""
    try:
        cast = "\n".join(definition(src["lldirectorcast.cpp"], sig) for sig in (
            "void LLDirectorCast::setCloneFadeAlpha(", "void LLDirectorCast::forgetCloneFade(",
            "const LLDirectorCast::ActorStyle& LLDirectorCast::getRenderActorStyle(const LLUUID& id) const",
            "const LLDirectorCast::ActorStyle& LLDirectorCast::resolveStoredActorStyle("))
    except ValueError:
        return ["the orphan fixture could not extract the production fade coverage"]
    if cast not in _ORPHAN_CACHE:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from native_check_tools import check_directory, compile_run
        source = ORPHAN_FIXTURE.replace("@CAST@", cast)
        try:
            result = compile_run(check_directory("clone_menu_orphan"), source, "orphan", flags=['/I"%s"' % NV])
            ok = result.returncode == 0 and "PASS" in result.stdout
            _ORPHAN_CACHE[cast] = [] if ok else ["a non-Cast clone's fade coverage is wrong: " + (result.stdout + result.stderr).strip()[:400]]
        except RuntimeError as exc:
            _ORPHAN_CACHE[cast] = ["the orphan fixture did not compile: " + str(exc).strip().splitlines()[-1][:300]]
    return _ORPHAN_CACHE[cast]


def style_items(branch):
    """Every check item of an overlay Style branch in document order:
    (group label, label, name, click parameter, check parameter)."""
    rows = []

    def walk(node, group):
        for child in node:
            if child.tag == "context_menu":
                walk(child, child.get("label"))
            elif child.tag == "menu_item_check":
                params = {e.tag.split(".")[-1]: e.get("parameter") for e in child}
                rows.append((group, child.get("label"), child.get("name"), params.get("on_click"),
                             params.get("on_check")))
    walk(branch, None)
    return rows


def overlay_menu_findings(src: dict[str, str], cpp: str) -> list[str]:
    """[OverlayMenu] The clone menu adapts to its targets' kinds: a Style branch identical to the avatar
    menus' Overlay Style with its own callbacks, entity-only items hidden for all-overlay targets,
    tidy separators, the hidden Entity Look, Back to live, and the Gaze submenu."""
    out: list[str] = []
    try:
        clone_root = ET.fromstring(src["menu_ghost_clone.xml"])
        avatar_root = ET.fromstring(src["menu_avatar_self.xml"])
    except ET.ParseError:
        return out
    style = clone_root.find("context_menu[@name='Clone Style']")
    avatar_style = next((c for c in avatar_root.find("context_menu[@name='Clone Overlay']")
                         if c.get("name") == "Overlay Style"), None)
    if style is None or avatar_style is None:
        return ["menu_ghost_clone.xml: the Style branch (name Clone Style) is missing"]
    if style.get("label") != avatar_style.get("label"):
        out.append("menu_ghost_clone.xml: the Style branch label differs from the avatar menus' Overlay Style")
    want = style_items(avatar_style)
    have = style_items(style)
    if [r[:3] for r in have] != [r[:3] for r in want]:
        out.append("menu_ghost_clone.xml: Style groups / labels / order differ from the avatar menus' Overlay Style")
    if [r[3] for r in have] != [r[3] for r in want]:
        out.append("menu_ghost_clone.xml: Style item ids differ from the avatar menus' Overlay Style")
    for row in have:
        if row[3] != row[4]:
            out.append(f"menu_ghost_clone.xml: Style item {row[1]} click / check parameters differ")
    for node in style.iter():
        function = node.get("function") or ""
        if node.tag.endswith("on_click") and function != "GhostClone.Style":
            out.append(f"menu_ghost_clone.xml: a Style item clicks {function}, not GhostClone.Style")
        if node.tag.endswith("on_check") and function != "GhostClone.CheckStyle":
            out.append(f"menu_ghost_clone.xml: a Style item checks {function}, not GhostClone.CheckStyle")
        if node.tag.endswith("on_enable") and function != "GhostClone.EnableStyle":
            out.append(f"menu_ghost_clone.xml: a Style item enables with {function}, not GhostClone.EnableStyle")
    for name in AVATAR_MENUS:        # the avatar menus keep their own GhostOverlay.* path
        if 'function="GhostOverlay.Style"' not in src[name] or "GhostClone.Style" in src[name]:
            out.append(f"{name}: the avatar menus' Overlay Style must keep GhostOverlay.Style only")
    style_commit = function_body(cpp, 'commit.add("GhostClone.Style"')
    if "Actions::setOverlayStyle(overlays, param.asInteger());" not in style_commit or \
            "const std::vector<LLUUID> overlays = liveOverlayTargets();" not in style_commit:
        out.append("GhostClone.Style must set the style on the overlay targets only (Actions::setOverlayStyle)")
    overlays_fn = function_body(cpp, "std::vector<LLUUID> liveOverlayTargets")
    if "mKind == ALGhostStudio::BACKING_OVERLAY" not in overlays_fn:
        out.append("liveOverlayTargets must keep only overlay clones")
    agg_fn = function_body(cpp, "Aggregate computeAggregate")
    if "agg.mOverlayStyle = -1;" not in agg_fn or "agg.mOverlayStyle != inst->mStyle" not in agg_fn:
        out.append("the Style check mark shows a style only when every overlay target has it")
    if "return state(ctrl, sAgg.mOverlays > 0," not in cpp or \
            "return sAgg.mOverlayStyle == param.asInteger();" not in cpp:
        out.append("GhostClone.EnableStyle / CheckStyle must read the overlay aggregate")
    layout = function_body(cpp, "void applyKindLayout")
    for needle in ("const bool entity_items = Logic::entityItemsVisible(agg.mEntities);",
                   "item->setVisible(entity_items);",
                   'sMenu->getItem(std::string("Stagger"))', "stagger->setVisible(false);",
                   'sMenu->getItem(std::string("Clone Style"))',
                   "style->setVisible(Logic::styleVisible(agg.mOverlays));",
                   "tidySeparators(sMenu);",
                   'subMenu(sMenu, "Appearance")', 'appearance->getItem(std::string("Entity Look"))',
                   "look->setVisible(false);"):
        if needle not in layout:
            out.append(f"applyKindLayout: missing {needle}")
    hidden = re.search(r"ENTITY_ONLY_ITEMS\[\] = \{([^}]*)\}", cpp)
    names = re.findall(r'"([^"]+)"', hidden.group(1)) if hidden else []
    if sorted(names) != sorted(["Pause", "Play", "Mirror", "Look Turn", "Gaze", "Animation", "Appearance",
                                "Clear animations"]):
        out.append(f"applyKindLayout must hide exactly Pause, Play, Mirror, Look Turn, Gaze, Animation, "
                   f"Appearance, Clear animations for all-overlay targets (has {names})")
    root_names = {c.get("name") for c in clone_root}
    for name in names:
        if name not in root_names:
            out.append(f"ENTITY_ONLY_ITEMS names {name}, which is not an item of menu_ghost_clone.xml")
    for keep in ("Select in Director", "Size", "Edit", "Freeze pose", "Solo", "Duplicate", "Delete",
                 "Open in Director"):
        if keep in names:
            out.append(f"{keep} supports overlay clones and must stay visible")
    present = function_body(cpp, "void presentEntityMenu")
    if "applyKindLayout(sAgg);" not in present:
        out.append("presentEntityMenu must adapt the menu to its targets' kinds on every open")
    if present.find("stagger->setVisible(Logic::staggerVisible(unit_is_group));") > present.find("applyKindLayout(sAgg);"):
        out.append("presentEntityMenu: the kind layout must run after the Stagger rule")
    if "Logic::entityItemsVisible(sAgg.mEntities) && !Logic::allOf(sAgg.mPaused, sAgg.mEntities)" not in cpp:
        out.append("GhostClone.VisiblePause must stay hidden for all-overlay targets (its on_visible would re-show it)")
    ties = function_body(cpp, "void tidySeparators")
    for needle in ("item->setVisible(real_since_separator);", "last_shown_separator->setVisible(false);"):
        if needle not in ties:
            out.append(f"tidySeparators: missing {needle}")
    logic = src["alghostclonemenulogic.h"]
    if "inline bool entityItemsVisible(std::size_t entity_targets)\n{\n    return entity_targets > 0;" not in logic or \
            "inline bool styleVisible(std::size_t overlay_targets)\n{\n    return overlay_targets > 0;" not in logic:
        out.append("alghostclonemenulogic.h: entityItemsVisible / styleVisible changed")

    # ---- Back to live ----------------------------------------------------------------
    animation = clone_root.find("context_menu[@name='Animation']")
    first = list(animation)[0] if animation is not None and len(animation) else None
    if first is None or first.get("name") != "Back to live" or first.get("label") != "Back to live":
        out.append("menu_ghost_clone.xml: Back to live must be the first item of the Animation submenu")
    elif [e.get("function") for e in first] != ["GhostClone.BackToLive", "GhostClone.EnableBackToLive"]:
        out.append("menu_ghost_clone.xml: Back to live callbacks changed")
    live_fn = function_body(cpp, "void actBackToLive")
    for needle in ("Actions::setPaused(entities, false);", "inst->mDriveMode != ALGhostStudio::DRIVE_MIRROR",
                   "Actions::setDriveMode(off_mirror, ALGhostStudio::DRIVE_MIRROR, LLUUID::null);",
                   "liveEntityTargets()"):
        if needle not in live_fn:
            out.append(f"actBackToLive: missing {needle}")
    if live_fn.find("Actions::setPaused(entities, false);") > live_fn.find("Actions::setDriveMode("):
        out.append("actBackToLive: a paused clone must be resumed before it is set to the live mirror")
    if "agg.mNotLive += inst->mDriveMode != ALGhostStudio::DRIVE_MIRROR ? 1 : 0;" not in agg_fn or \
            "return state(ctrl, sAgg.mNotLive > 0," not in cpp or '"GhostCloneMenuAlreadyLive"' not in cpp or \
            'name="GhostCloneMenuAlreadyLive"' not in src["strings.xml"]:
        out.append("Back to live is enabled only while an entity target is off the live mirror, with an 'already live' reason")

    # ---- Gaze -----------------------------------------------------------------------------
    gaze = clone_root.find("context_menu[@name='Gaze']")
    order = [c.get("name") for c in clone_root]
    if gaze is None or order.index("Gaze") != order.index("Look Turn") + 1:
        out.append("menu_ghost_clone.xml: Gaze must follow Look / turn")
    else:
        labels = [c.get("label") for c in gaze]
        if labels != ["Camera", "Me", "Cast member", "Nearby people", "Travel direction", "Off"]:
            out.append(f"menu_ghost_clone.xml: Gaze items are {labels}")
        wanted = {"Gaze Camera": "camera", "Gaze Me": "me", "Gaze Travel": "travel", "Gaze Off": "off"}
        for item in gaze:
            kind = wanted.get(item.get("name"))
            if kind is None:
                continue
            calls = {e.tag.split(".")[-1]: (e.get("function"), e.get("parameter")) for e in item}
            if calls.get("on_click") != ("GhostClone.Gaze", kind) or calls.get("on_check") != ("GhostClone.CheckGaze", kind) \
                    or calls.get("on_enable", (None,))[0] != "GhostClone.EnableGaze":
                out.append(f"menu_ghost_clone.xml: Gaze item {item.get('label')} is not wired to {kind}")
    gaze_fn = function_body(cpp, "void actGazeParam")
    for needle in ("actGaze(-1, LLUUID::null, true);", "actGaze(LLActorMover::GAZE_CAMERA, LLUUID::null, false);",
                   "actGaze(LLActorMover::GAZE_TANGENT, LLUUID::null, false);",
                   "actGaze(LLActorMover::GAZE_CAST, gAgentID, false);"):
        if needle not in gaze_fn:
            out.append(f"actGazeParam: missing {needle}")
    act = function_body(cpp, "void actGaze(S32 mode")
    for needle in ("if (Actions::hasExpandedMember(sRows))", "GhostCloneMenuWholeGroup",
                   "Actions::applyGazeTarget(entities, mode, cast_target, !off);",
                   "Actions::snapshotGaze(sRows)", 'recordCustom(shell, "clone_gaze"',
                   "Actions::restoreGaze(redo ? history->mAfter : history->mBefore)", "if (!gazeUnchanged(before, after))",
                   "Actions::rewriteGazeReferences(history->mBefore, old_id, new_id);",
                   "Actions::rewriteGazeReferences(history->mAfter, old_id, new_id);", "std::make_shared<History>()"):
        if needle not in act:
            out.append(f"actGaze: missing {needle}")
    if act.count("Actions::snapshotGaze(sRows)") != 2:
        out.append("actGaze must snapshot before and after for undo / redo")
    for forbidden in ("setGazeTargetMode", "setGazeEnabled", "setGazeCastTarget"):
        if forbidden in cpp:
            out.append(f"alghostclonemenu.cpp: {forbidden} belongs to the shared applyGazeTarget, not the menu")
    populate = function_body(cpp, "void populateGazeMenus")
    for needle in ("LLDirectorCast::isSelf(choice.mId)", "std::find(own.begin(), own.end(), choice.mId) != own.end()",
                   "gazeCastLabel(choice)", "[id]() { actGazeCast(id); }", "ALGazeNearby::nearbyAllowed()", "LLDirectorCast::instance().getMember(person.mId)",
                   "offered.count(person.mId)", "personNearby(id)", "GhostCloneMenuNoCast", "GhostCloneMenuNobodyNearby",
                   "actGaze(LLActorMover::GAZE_CAST, id, false);"):
        if needle not in populate:
            out.append(f"populateGazeMenus: missing {needle}")
    label_fn = function_body(cpp, "std::string gazeCastLabel")
    for needle in ("nameHiddenByRlva(choice.mId)", "Logic::containsUuidText(choice.mLabel)", "castMemberName(choice.mId)",
                   'std::string("Unknown")', "return choice.mLabel;"):
        if needle not in label_fn:
            out.append(f"gazeCastLabel: missing {needle} (no RLV-hidden name, never a UUID)")
    cast_fn = function_body(cpp, "void actGazeCast")
    for needle in ("LLDirectorCast::instance().getActorChoices()", "!LLDirectorCast::isSelf(choice.mId)",
                   "if (!current || std::find(own.begin(), own.end(), id) != own.end())", '"GhostCloneMenuCastGone"',
                   "actGaze(LLActorMover::GAZE_CAST, id, false);"):
        if needle not in cast_fn:
            out.append(f"actGazeCast: missing {needle} (a retired cast id is refused at click time)")
    if 'name="GhostCloneMenuCastGone"' not in src["strings.xml"]:
        out.append("strings.xml: GhostCloneMenuCastGone is missing")
    rewrite_fn = function_body(strip_comments(src["alghostcloneactions.cpp"]), "void rewriteGazeReferences")
    for needle in ("saved.mCast = new_id;", "new_id.isNull() && saved.mMode == LLActorMover::GAZE_CAST",
                   "saved.mMode = LLActorMover::GAZE_TANGENT;"):
        if needle not in rewrite_fn:
            out.append(f"rewriteGazeReferences: missing {needle}")
    if "mGazeBodies" not in gazeItemEnabled_text(cpp) or "sAgg.mGazeBodies == 0" not in function_body(cpp, "bool gazeChecked"):
        out.append("Gaze items need a spawned body: disabled, and no check mark (not even Off), without one")
    speed_head = function_body(cpp, "void actSpeed")
    gaze_head = function_body(cpp, "void actGazeParam")
    if speed_head.find('kind != "faster"') < 0 or speed_head.find('kind != "faster"') > speed_head.find("liveEntityTargets()"):
        out.append("actSpeed must validate its parameter before any tip")
    live = function_body(cpp, "void actBackToLive")
    if "resumed = resumed ||" not in live or "if (!resumed)" not in live or \
            live.find("resumed = resumed ||") > live.find("Actions::setPaused(entities, false);"):
        out.append("actBackToLive: no 'already live' tip after a successful unpause")
    if "Pause" not in "".join(e.get("tool_tip", "") for e in clone_root.iter("menu_item_call") if e.get("name") == "Back to live") \
            or "agg.mNotLive += inst->mDriveMode != ALGhostStudio::DRIVE_MIRROR ? 1 : 0;" not in agg_fn:
        out.append("Back to live promises Pause: a paused target (not on DRIVE_MIRROR) must count as not live")
    if "populateGazeMenus(people);" not in function_body(cpp, "void populateDynamicMenus"):
        out.append("populateDynamicMenus must rebuild the Gaze lists")
    if 'return state(ctrl, false, "GhostCloneMenuWholeGroup");' not in function_body(cpp, "void registerEntityCallbacks") or \
            '"GhostClone.EnableGaze"' not in cpp:
        out.append("Gaze items are greyed with the whole-group reason for a single member of an expanded group")
    agg_gaze = ("agg.mGazeAgree = false;", "mover.isGazeEnabled(inst->mEntityId)")
    for needle in agg_gaze:
        if needle not in agg_fn:
            out.append(f"computeAggregate: Gaze check marks need {needle}")
    check = function_body(cpp, "bool gazeChecked")
    if "!sAgg.mGazeAgree" not in check or "sAgg.mGazeCast == gAgentID" not in check:
        out.append("the Gaze check mark shows a target only when every entity target agrees")
    acts_cpp = strip_comments(src["alghostcloneactions.cpp"])
    apply_fn = function_body(acts_cpp, "void applyGazeTarget")
    for needle in ("mover.setGazeTargetMode(entity, mode);", "mover.setGazeCastTarget(entity, cast_target);",
                   "mover.setGazeEnabled(entity, enabled);"):
        if needle not in apply_fn:
            out.append(f"applyGazeTarget: missing {needle}")
    ents = function_body(acts_cpp, "std::vector<LLUUID> gazeEntities")
    if "BACKING_ENTITY_CLONE" not in ents or "inst->mEntityId.notNull()" not in ents:
        out.append("gazeEntities must be the live entity clones' actor ids only")
    panel = strip_comments(src["alpanelghoststudio.cpp"])
    for fn in ("void ALPanelGhostStudio::onLensGazeToggle", "void ALPanelGhostStudio::onClickLensGazeSelection"):
        if "ALGhostCloneActions::applyGazeTarget(" not in function_body(panel, fn) or \
                "ALGhostCloneActions::gazeEntities(selectedInstances())" not in function_body(panel, fn):
            out.append(f"{fn} must write through the shared applyGazeTarget (the page and the menu cannot drift)")
    out += speed_findings(src, cpp, clone_root, agg_fn)
    out += flip_findings(src, cpp, clone_root, agg_fn)
    return out


SPEED_FIXTURE = r"""
#include "alghostclonemenulogic.h"
#include <cstdio>
#include <cmath>
using namespace ALGhostCloneMenuLogic;
static int fails = 0;
static void expect(bool ok, const char* what) { if (!ok) { std::printf("FAIL: %s\n", what); ++fails; } }
static bool near_(float a, float b) { return std::fabs(a - b) < 1e-5f; }
int main()
{
    const float ladder[] = { 0.25f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 3.f, 4.f };
    for (int i = 0; i < 9; ++i)
        expect(near_(ANIM_SPEED_LADDER[i], ladder[i]), "ladder rung");
    expect(ANIM_SPEED_RUNGS == 9, "nine rungs");
    for (int i = 0; i < 8; ++i)
        expect(near_(steppedAnimSpeed(ladder[i], 1), ladder[i + 1]), "faster from a rung goes to the next rung");
    for (int i = 1; i < 9; ++i)
        expect(near_(steppedAnimSpeed(ladder[i], -1), ladder[i - 1]), "slower from a rung goes to the previous rung");
    expect(near_(steppedAnimSpeed(4.f, 1), 4.f), "faster at the top stays");
    expect(near_(steppedAnimSpeed(0.25f, -1), 0.25f), "slower at the bottom stays");
    expect(near_(steppedAnimSpeed(0.1f, -1), 0.1f), "slower below the bottom rung never raises the speed");
    expect(near_(steppedAnimSpeed(0.1f, 1), 0.25f), "faster below the bottom rung goes to 0.25");
    expect(near_(steppedAnimSpeed(1.1f, 1), 1.25f), "between rungs: faster goes to the next rung up");
    expect(near_(steppedAnimSpeed(1.1f, -1), 1.f), "between rungs: slower goes to the next rung down");
    expect(near_(steppedAnimSpeed(2.5f, 1), 3.f), "2.5 faster is 3");
    expect(near_(steppedAnimSpeed(2.5f, -1), 2.f), "2.5 slower is 2");
    expect(near_(steppedAnimSpeed(1.00001f, 1), 1.25f), "a hair above a rung is that rung");
    expect(near_(steppedAnimSpeed(3.99999f, 1), 3.99999f), "a hair under 4 is the top");
    if (fails) return 1;
    std::printf("PASS: animation speed ladder\n");
    return 0;
}
"""


def gazeItemEnabled_text(cpp: str) -> str:
    return function_body(cpp, "bool gazeItemEnabled")


def speed_ladder_native(logic_text: str) -> list[str]:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from native_check_tools import check_directory, compile_run
    directory = check_directory("clone_menu_speed")
    # The fixture includes THIS text of the header (next to the source, found before the repo copy), so
    # a mutated ladder is compiled and run, not the file on disk.
    (Path(directory) / "alghostclonemenulogic.h").write_text(logic_text, encoding="utf-8", newline="\n")
    try:
        result = compile_run(directory, SPEED_FIXTURE, "speed_ladder",
                             flags=['/I"%s"' % NV])
    except RuntimeError as exc:
        return ["speed ladder fixture did not compile: " + str(exc).strip().splitlines()[-1][:200]]
    if result.returncode != 0 or "PASS" not in result.stdout:
        return ["Animation > Speed ladder: " + (result.stdout + result.stderr).strip()[:300]]
    return []


def speed_findings(src, cpp, clone_root, agg_fn) -> list[str]:
    out: list[str] = []
    animation = clone_root.find("context_menu[@name='Animation']")
    speed = animation.find("context_menu[@name='Anim Speed']") if animation is not None else None
    if speed is None or speed.get("label") != "Speed":
        return ["menu_ghost_clone.xml: Animation > Speed submenu is missing"]
    kids = list(speed)
    if [c.get("label") for c in kids][1:] != ["Faster", "Slower", "Reset speed"] or kids[0].get("name") != "Speed Info" \
            or kids[0].get("enabled") != "false":
        out.append("menu_ghost_clone.xml: Speed must be a disabled info line, Faster, Slower, Reset speed")
    else:
        for item, kind in zip(kids[1:], ("faster", "slower", "reset")):
            calls = {e.tag.split(".")[-1]: (e.get("function"), e.get("parameter")) for e in item}
            if calls.get("on_click") != ("GhostClone.Speed", kind) or calls.get("on_enable") != ("GhostClone.EnableSpeed", kind):
                out.append(f"menu_ghost_clone.xml: Speed item {item.get('label')} is not wired to {kind}")
    logic = src["alghostclonemenulogic.h"]
    if "{ 0.25f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 3.f, 4.f }" not in logic:
        out.append("alghostclonemenulogic.h: the speed ladder changed")
    out += speed_ladder_native(logic)
    act = function_body(cpp, "void actSpeed")
    for needle in ("if (Actions::hasExpandedMember(sRows))", "GhostCloneMenuWholeGroup", "Actions::setAnimSpeed(sRows, 1.f);",
                   "Actions::stepAnimSpeed(sRows, kind == \"faster\" ? 1 : -1);", "Actions::snapshotAnimSpeed(sRows)",
                   'recordCustom(shell, "clone_anim_speed"', "Actions::restoreAnimSpeed(redo ? after : before)",
                   "if (changed)"):
        if needle not in act:
            out.append(f"actSpeed: missing {needle}")
    if act.count("Actions::snapshotAnimSpeed(sRows)") != 2:
        out.append("actSpeed must snapshot before and after for undo / redo")
    if "setInstanceAnimSpeed" in cpp:
        out.append("alghostclonemenu.cpp: setInstanceAnimSpeed belongs to the shared setAnimSpeed, not the menu")
    enable = function_body(cpp, "void registerEntityCallbacks")
    for needle in ('"GhostCloneMenuSpeedAtMax"', '"GhostCloneMenuSpeedAtMin"', '"GhostCloneMenuSpeedAtOne"',
                   "sAgg.mAtFastest < sAgg.mEntities", "sAgg.mAtSlowest < sAgg.mEntities", "sAgg.mAtNormal < sAgg.mEntities",
                   '"GhostClone.EnableSpeed"', 'state(ctrl, false, "GhostCloneMenuWholeGroup")'):
        if needle not in enable:
            out.append(f"GhostClone.EnableSpeed: missing {needle}")
    for key in ("GhostCloneMenuSpeedInfo", "GhostCloneMenuSpeedMixed", "GhostCloneMenuSpeedAtMax", "GhostCloneMenuSpeedAtMin",
                "GhostCloneMenuSpeedAtOne"):
        if f'name="{key}"' not in src["strings.xml"]:
            out.append(f"strings.xml: {key} is missing")
    for needle in ("agg.mSpeedSame = false;", "agg.mAtFastest +=", "agg.mAtSlowest +=", "agg.mAtNormal +=",
                   "Logic::ANIM_SPEED_LADDER[Logic::ANIM_SPEED_RUNGS - 1] - Logic::ANIM_SPEED_EPSILON",
                   "Logic::ANIM_SPEED_LADDER[0] + Logic::ANIM_SPEED_EPSILON", "std::fabs(speed - 1.f) <= Logic::ANIM_SPEED_EPSILON"):
        if needle not in agg_fn:
            out.append(f"computeAggregate: Speed needs {needle}")
    info = function_body(cpp, "void refreshSpeedInfo")
    if "sAgg.mEntities > 0 && sAgg.mSpeedSame" not in info or "GhostCloneMenuSpeedMixed" not in info:
        out.append("refreshSpeedInfo: shows the common speed, or 'mixed' when the targets differ")
    if "refreshSpeedInfo();" not in function_body(cpp, "void populateDynamicMenus"):
        out.append("populateDynamicMenus must refresh the Speed info line")
    acts = strip_comments(src["alghostcloneactions.cpp"])
    set_fn = function_body(acts, "void setAnimSpeed")
    if "speed = llclamp(speed, 0.05f, 4.f);" not in set_fn or "studio.setInstanceAnimSpeed(id, speed);" not in set_fn:
        out.append("setAnimSpeed must clamp 0.05..4 and call setInstanceAnimSpeed (the spinner's path)")
    step = function_body(acts, "void stepAnimSpeed")
    if "ALGhostCloneMenuLogic::steppedAnimSpeed(inst->mAnimSpeed, direction)" not in step or "BACKING_ENTITY_CLONE" not in step:
        out.append("stepAnimSpeed must step each entity target from its own speed on the ladder")
    panel = strip_comments(src["alpanelghoststudio.cpp"])
    if "ALGhostCloneActions::setAnimSpeed(selectedInstances()," not in function_body(panel, "void ALPanelGhostStudio::onAnimSpeedCommit"):
        out.append("onAnimSpeedCommit must write through the shared setAnimSpeed (the page and the menu cannot drift)")
    return out


def flip_findings(src, cpp, clone_root, agg_fn) -> list[str]:
    """Animation > Flip left/right: the Director's Flip pose L/R row's state, through ONE shared writer."""
    out: list[str] = []
    animation = clone_root.find("context_menu[@name='Animation']")
    item = None
    if animation is not None:
        for child in animation:
            if child.get("name") == "Flip left right":
                item = child
    if item is None or item.tag != "menu_item_check" or item.get("label") != "Flip left/right":
        return ["menu_ghost_clone.xml: Animation > Flip left/right check item is missing"]
    calls = {e.tag.split(".")[-1]: e.get("function") for e in item}
    if calls != {"on_click": "GhostClone.FlipLR", "on_check": "GhostClone.CheckFlipLR", "on_enable": "GhostClone.EnableFlipLR"}:
        out.append(f"menu_ghost_clone.xml: Flip left/right callbacks are {calls}")
    if "Flip pose L/R" not in (item.get("tool_tip") or ""):
        out.append("menu_ghost_clone.xml: the Flip left/right tooltip must name the Director's Flip pose L/R row")
    kids = [c.get("label") for c in animation]
    if "Restart animation" not in kids or kids.index("Flip left/right") != kids.index("Restart animation") + 1:
        out.append("menu_ghost_clone.xml: Flip left/right must follow Restart animation")
    act = function_body(cpp, "void actFlipLR")
    for needle in ("Actions::snapshotDirectedMirror(sRows)", "const bool changed = Actions::setDirectedMirror(sRows, !all_on);", "all_on = all_on && saved.mOn;",
                   "if (changed)", 'recordCustom(shell, "clone_flip_lr"', "Actions::restoreDirectedMirror(redo ? history->mAfter : history->mBefore)",
                   "Actions::directedMirrorReplayable(redo ? history->mAfter : history->mBefore)", "if (before.empty())", "false,\n"):
        if needle not in act:
            out.append(f"actFlipLR: missing {needle}")
    if act.count("Actions::snapshotDirectedMirror(sRows)") != 2:
        out.append("actFlipLR must snapshot before and after: a click that changes nothing records no history step")
    if act.find("if (changed)") > act.find("recordCustom") or act.find("recordCustom") < 0:
        out.append("actFlipLR: history is recorded only inside if (changed)")
    for bad in ("setInstanceDirectedMirror", "mDirectedMirror ="):
        if bad in cpp:
            out.append(f"alghostclonemenu.cpp: {bad} belongs to the shared setDirectedMirror, not the menu")
    reg = function_body(cpp, "void registerEntityCallbacks")
    enable = reg[reg.find('"GhostClone.EnableFlipLR"'):reg.find('commit.add("GhostClone.FlipLR"')] if '"GhostClone.EnableFlipLR"' in reg else ""
    for needle in ('state(ctrl, false, "GhostCloneMenuMirrorEntityOnly")', 'state(ctrl, sAgg.mFlipEligible > 0, "GhostCloneMenuFlipNotActive")',
                   "sAgg.mEntities == 0", "return sAgg.mFlipEligible > 0 && sAgg.mFlipOn == sAgg.mFlipEligible;"):
        if needle not in enable:
            out.append(f"GhostClone.EnableFlipLR / CheckFlipLR: missing {needle}")
    if 'commit.add("GhostClone.FlipLR", [](LLUICtrl*, const LLSD&) { actFlipLR(); });' not in reg:
        out.append("registerEntityCallbacks: GhostClone.FlipLR must call actFlipLR")
    if "if (Actions::directedMirrorEligible(id))" not in agg_fn or "++agg.mFlipEligible;" not in agg_fn or \
            "agg.mFlipOn += inst->mDirectedMirror ? 1 : 0;" not in agg_fn:
        out.append("computeAggregate: Flip counts eligible and flipped entity targets through directedMirrorEligible")
    if 'name="GhostCloneMenuFlipNotActive"' not in src["strings.xml"]:
        out.append("strings.xml: GhostCloneMenuFlipNotActive is missing")
    acts = strip_comments(src["alghostcloneactions.cpp"])
    elig = function_body(acts, "bool directedMirrorEligible")
    for needle in ("inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE", "!inst->mDeleting",
                   "ALGhostAnimMirrorMath::directedMirrorActive(inst->mDriveMode, inst->mDirectedMirrorFreezeDrive, true)"):
        if needle not in elig:
            out.append(f"directedMirrorEligible: missing {needle} (the Director row's enable rule)")
    setter = function_body(acts, "bool setDirectedMirror")
    if "if (!directedMirrorEligible(id))" not in setter or "continue;" not in setter or \
            setter.find("continue;") > setter.find("setInstanceDirectedMirror(id, enabled)") or "setInstanceDirectedMirror(id, enabled)" not in setter:
        out.append("setDirectedMirror: ineligible clones are skipped (never written), eligible ones go through setInstanceDirectedMirror")
    replay = function_body(acts, "bool directedMirrorReplayable")
    if "if (directedMirrorEligible(saved.mId))" not in replay or "getInstance" in replay:
        out.append("directedMirrorReplayable: the history preflight must use the SAME directedMirrorEligible predicate as replay "
                   "(an existing-but-ineligible clone is not replayable)")
    if not replay.lstrip("{").strip().startswith("for (const FlipBefore& saved : state)"):
        out.append("directedMirrorReplayable: must start with the per-saved-clone loop (no early accept)")
    snap = function_body(acts, "std::vector<FlipBefore> snapshotDirectedMirror")
    if "if (directedMirrorEligible(id))" not in snap:
        out.append("snapshotDirectedMirror: eligible clones only")
    if "if (directedMirrorEligible(saved.mId))" not in function_body(acts, "bool restoreDirectedMirror"):
        out.append("restoreDirectedMirror: eligible clones only")
    panel = strip_comments(src["alpanelghoststudio.cpp"])
    commit = function_body(panel, "void ALPanelGhostStudio::onDirectedMirrorCommit")
    if "ALGhostCloneActions::setDirectedMirror(selectedInstances(), enabled);" not in commit or "setInstanceDirectedMirror" in commit:
        out.append("onDirectedMirrorCommit must write through the shared setDirectedMirror (the page and the menu cannot drift)")
    if "ALGhostAnimMirrorMath::directedMirrorActive(\n        inst->mDriveMode, inst->mDirectedMirrorFreezeDrive, true)" not in panel:
        out.append("the page's Flip pose L/R enable rule changed: keep directedMirrorEligible identical to it")
    return out


def findings(src: dict[str, str]) -> list[str]:
    out: list[str] = []
    out += parity_findings(src)
    out += record_findings(src)
    out += orphan_findings(src)
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
        want = ["Select in Director", "-", "Pause", "Play", "Mirror", "Size", "Stagger", "Edit", "Look / turn", "Gaze", "Animation",
                "Appearance", "Style", "-", "Freeze pose", "Clear animations", "Solo", "Duplicate", "Delete", "-", "Open in Director"]
        if labels != want:
            out.append(f"menu_ghost_clone.xml item order is {labels}")
        size = [c.get("label") for c in root.find("context_menu[@name='Size']")]
        if size != ["Smaller", "Larger", "Reset size"]:
            out.append("menu_ghost_clone.xml Size submenu is not Smaller / Larger / Reset size")
    except ET.ParseError as exc:
        out.append(f"menu_ghost_clone.xml does not parse: {exc}")

    out += overlay_menu_findings(src, cpp)

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
    present = function_body(cpp, "void presentEntityMenu")
    if "const bool unit_is_group = Actions::collapsedGroup(sPicked) != nullptr;" not in present or \
            "stagger->setVisible(Logic::staggerVisible(unit_is_group));" not in present or \
            'sMenu->getItem(std::string("Stagger"))' not in present:
        out.append("presentEntityMenu: Stagger is shown only when the picked clone's unit is a collapsed group")
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
    if "if (!shell && director_fallback && played > 0)" not in panel_play or 'LLFloaterReg::getInstance("director")' not in panel_play:
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
    if len(re.findall(r"\bsRows\s*=", cpp)) != 4:
        out.append("sRows may be assigned only by showEntityMenu's two plan branches and openForListRow's two (the rows actions consume)")
    list_open = function_body(cpp, "bool openForListRow")
    if not re.search(r"if \(std::find\(host_rows\.begin\(\), host_rows\.end\(\), row\) != host_rows\.end\(\)\)\s*\{\s*sRows = host_rows;\s*\}\s*else\s*\{\s*"
                     r"sRows = std::vector<LLUUID>\{ row \};\s*studio\.setSelected\(row\);\s*\}\s*sPicked = picked;\s*presentEntityMenu\(x, y, /\*opened_on_press=\*/true\);",
                     src["alghostclonemenu.cpp"].split("bool openForListRow", 1)[1].split("bool pressRight", 1)[0]) or \
            "ALDirectorSelect::hostRows()" not in list_open or "const LLUUID picked = listRowInstance(row);" not in list_open:
        out.append("openForListRow: a Clones list row targets the row, or the whole list selection when the row is in it (the world click's rule)")
    if "bool openForListRow(const LLUUID& row, S32 screen_x, S32 screen_y);" not in src["alghostclonemenu.h"]:
        out.append("alghostclonemenu.h: openForListRow is the list row entry point")
    page_click = function_body(strip_comments(src["alpanelghoststudio.cpp"]), "bool ALPanelGhostStudio::handleRightMouseDown")
    for needle in ("mList->getEnabled()", "mList->getItemAtPoint(list_x, list_y)", "ALGhostCloneMenu::canOpenForListRow(row)", "noteActiveHost();",
                   "mList->selectByID(row);", "onListSelect();",
                   "ALGhostCloneMenu::openForListRow(row, screen_x, screen_y)", "return LLPanel::handleRightMouseDown(x, y, mask);"):
        if needle not in page_click:
            out.append(f"alpanelghoststudio.cpp handleRightMouseDown: a Clones list row opens the clone menu: missing {needle}")
    if page_click.find("canOpenForListRow") > page_click.find("noteActiveHost();") or page_click.find("canOpenForListRow") > page_click.find("selectByID"):
        out.append("handleRightMouseDown: a row is validated BEFORE the selection or the active host changes (an unopenable row falls through untouched)")
    if "bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;" not in src["alpanelghoststudio.h"]:
        out.append("alpanelghoststudio.h: the Clones panel takes the list-row right click")
    if 'return "hidden";' not in strip_comments(src["alpanelghoststudio.cpp"]) or "inst.mUserHidden" not in function_body(strip_comments(src["alpanelghoststudio.cpp"]), "const char* state_name"):
        out.append("alpanelghoststudio.cpp: the Clones list State column marks a hidden clone")
    if not re.search(r"if \(plan\.mSelectPickedFirst\)\s*\{[^{}]*sRows = std::vector<LLUUID>\{ Actions::rowFor\(clone\) \};"
                     r"[^{}]*studio\.setSelected\(sRows\.front\(\)\);\s*\}\s*else\s*\{[^{}]*sRows = host_rows;[^{}]*\}\s*sPicked = clone;", show):
        out.append("showEntityMenu: sRows must come from the plan (picked unit when not selected, else the page's rows) before sPicked is set")
    # [ClonePick] right-click in a deliberate multi-selection makes the clicked clone the primary row.
    if not re.search(r"if \(!plan\.mSelectPickedFirst\)\s*\{\s*"
                     r"ALDirectorSelect::makePrimaryInSelection\(clone\);\s*\}\s*presentEntityMenu\(x, y, false\);", show):
        out.append("showEntityMenu: a clone right-clicked inside the multi-selection must become the primary row (the highlight follows the click)")
    select_cpp = strip_comments(src["aldirectorselect.cpp"])
    if "ALGhostCloneMenuAccess::promotePrimary(instance_id);" not in function_body(select_cpp, "void makePrimaryInSelection"):
        out.append("aldirectorselect.cpp: makePrimaryInSelection must promote the primary row through promotePrimary")
    promote = function_body(select_cpp, "void ALGhostCloneMenuAccess::promotePrimary")
    for needle in ("host->mPrimaryRow = row;", "studio.setSelected(row);", "row = group->mId;", "ALGhostCloneActions::rowFor(studio.getSelected())",
                   "if (current == row)", "host->mObservedSelectionRevision = studio.getSelectionRevision();"):
        if needle not in promote:
            out.append(f"promotePrimary must keep the selection and promote one existing row (header for a group member, idempotent): missing {needle}")
    for forbidden in ("selectMultiple", "selectByID", "deselect", "raiseDirector", "ensureBuilt", "selectInHosts", "refreshList"):
        if forbidden in promote:
            out.append(f"promotePrimary must not toggle, add, raise or rebuild: found {forbidden}")
    if "void makePrimaryInSelection(const LLUUID& instance_id);" not in src["aldirectorselect.h"]:
        out.append("aldirectorselect.h: makePrimaryInSelection is the right-click primary entry point")
    studio_cpp = strip_comments(src["alghoststudio.cpp"])
    pick_fn = function_body(studio_cpp, "LLUUID ALGhostStudio::pickInstanceAtScreen")
    if "ALGhostCloneMenuLogic::choosePickedClone<LLUUID>(" not in pick_fn or "gViewerWindow->mouseDirectionGlobal(x, y)" not in pick_fn or \
            "ALGhostCloneMenuLogic::rayBoxEntry(eye.mV, ray_dir.mV, box, near_t, entry_t)" not in pick_fn or \
            "eye + ray_dir * candidate.mEntryT" not in pick_fn or "cam->getNear()" not in pick_fn or \
            "candidates.reserve(mInstances.size())" not in pick_fn or "alghostclonemenulogic.h" not in src["alghoststudio.cpp"]:
        out.append("alghoststudio.cpp: pickInstanceAtScreen must rank world-ray box entries (choosePickedClone), report the entry point and reserve its storage")
    if "ALGhostCloneMenuLogic::rayNearDistance(cam->getNear(), ray_dir.mV, cam->getAtAxis().mV, near_t)" not in pick_fn or \
            "ray_ok && has_body_box &&" not in pick_fn or "box, near_t, entry_t)" not in pick_fn:
        out.append("alghoststudio.cpp: the near plane is a forward depth: rank/clamp boxes at near / cos(angle to the view axis) (rayNearDistance), never the raw getNear()")
    if "has_body_box = ghost && !ghost->isDead() && ghost->mDrawable.notNull() && !ghost->mDrawable->isDead();" not in pick_fn:
        out.append("alghoststudio.cpp: an entity clone with no live drawable body gets no pick box (probes only), as before the box pick")
    if "mCameraDist" in pick_fn or "dist_vec(box_center" in pick_fn:
        out.append("alghoststudio.cpp: pickInstanceAtScreen must not rank boxes by centre distance")
    if "ghost->getRenderedBounds(center, size)" not in pick_fn or "ALGhostCloneMenuLogic::scaledBodyBox(foot.mV, inst.mScale)" not in pick_fn or \
            "getVisualBounds" in pick_fn:
        out.append("alghoststudio.cpp: the pick region must follow the clone's rendered (scaled) extents, or a body box scaled with the clone, never the selection-box-only bounds")
    rendered = function_body(strip_comments(src["llghostavatar.cpp"]), "bool LLGhostAvatar::getRenderedBounds") \
        if "bool LLGhostAvatar::getRenderedBounds" in src["llghostavatar.cpp"] else ""
    if "mUseVisualBounds" in rendered or "getLastAnimExtents()" not in rendered or "!isBuilt()" not in rendered or \
            "ALGhostBounds::centerAndSize(" not in rendered or "center.setVec(" not in rendered or "return true;" not in rendered or \
            0 <= rendered.find("return false;") < rendered.find("getLastAnimExtents()"):
        out.append("llghostavatar.cpp: getRenderedBounds returns the rendered (outer-scale) extents from the anim extents (false before they are valid, never a stub)")
    if "best_d2" in pick_fn:
        out.append("alghoststudio.cpp: pickInstanceAtScreen must not go back to a nearest-probe-only loop")
    edit_stock = function_body(strip_comments(src["altoolghostedit.cpp"]), "bool ALToolGhostEdit::handleStockPick")
    if "ALDirectorSelect::cloneInFrontOfPick(pick_info, probe_agent)" not in edit_stock or "pickInstanceAtScreen(" not in edit_stock or \
            "&probe_agent" not in edit_stock:
        out.append("altoolghostedit.cpp: the Edit-clones stock pick must apply the foreground rule (cloneInFrontOfPick with the entry point) before taking a clone")
    if "pickInstanceAtScreen(x, y, GHOST_PICK_PX)" not in function_body(strip_comments(src["altoolghostedit.cpp"]), "LLUUID ALToolGhostEdit::pickInstance"):
        out.append("altoolghostedit.cpp: the Edit tool's click pick goes through the shared ALGhostStudio::pickInstanceAtScreen")
    dsel_cpp = strip_comments(src["aldirectorselect.cpp"])
    if "return cloneInFrontOfPick(pick, probe_agent) ? nearby : LLUUID::null;" not in function_body(dsel_cpp, "LLUUID cloneAtPick") or             "ALGhostAnimDrop::cloneInFront(real_pick, hud_pick" not in function_body(dsel_cpp, "bool cloneInFrontOfPick") or             "ALGhostAnimDrop::cloneInFront(real_pick, hud_pick" not in strip_comments(src["lltooldraganddrop.cpp"]):
        out.append("every clone pick caller applies the foreground rule: cloneAtPick and the Edit tool via cloneInFrontOfPick, the 3D drop via ALGhostAnimDrop::cloneInFront")
    for name, needle in (("aldirectorselect.cpp", "pickInstanceAtScreen("), ("altoolghostedit.cpp", "pickInstanceAtScreen("),
                         ("lltooldraganddrop.cpp", "pickInstanceAtScreen(")):
        if needle not in src[name]:
            out.append(f"{name}: clone picking goes through the shared ALGhostStudio::pickInstanceAtScreen")
    if "{ sPicked }" in cpp or "{sPicked}" in cpp:
        out.append("actions must not be pointed at the previous menu's clone")
    if "ALDirectorSelect::hostRows()" not in show or "Actions::instances(host_rows)" not in show:
        out.append("showEntityMenu does not read the Clones list rows")
    if not re.search(r"sRows = std::vector<LLUUID>\{ Actions::rowFor\(clone\) \};\s*studio\.setSelected\(sRows\.front\(\)\);", show) \
            or "sRows = host_rows;" not in show:
        out.append("showEntityMenu: rows are the page's rows, or the picked unit selected first")
    if "presentEntityMenu(x, y, /*opened_on_press=*/false);" not in src["alghostclonemenu.cpp"].split("void showEntityMenu", 1)[1].split("} // namespace", 1)[0] or \
            "sMenu->show(x, y);" not in present:
        out.append("showEntityMenu never shows the menu (through presentEntityMenu)")
    if "constexpr S32 SPAWN_DISARMED = S32_MAX;" not in cpp or "S32_MAX / 2" in cpp or "S32_MAX, S32_MAX" in cpp:
        out.append("the spawn point is disarmed with exactly S32_MAX, the stock value (any other value arms every menu item's right-release)")
    if "sContextMenuSpawnPos.set(SPAWN_DISARMED, SPAWN_DISARMED);" not in function_body(cpp, "bool pressRight"):
        out.append("pressRight must disarm an earlier menu's spawn point once a clone is hit")
    if not re.search(r"if \(!opened_on_press\)\s*\{\s*LLMenuHolderGL::sContextMenuSpawnPos\.set\(SPAWN_DISARMED, SPAWN_DISARMED\);\s*\}", present.split("sMenu->show(x, y);")[-1]):
        out.append("a world click's menu must disarm the holder's spawn point after showing (the next right-click would be swallowed); a list row's press-opened menu keeps it armed")
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
    rows_fn = function_body(dsel, "ALPanelGhostStudio* ALGhostCloneMenuAccess::menuHost")
    if "hidden->mObservedSelectionRevision == ALGhostStudio::instance().getSelectionRevision()" not in rows_fn or \
            "ensureBuilt" in rows_fn or "findHostBelow(lazy)" not in rows_fn or "menuHost()" not in function_body(dsel, "std::vector<LLUUID> ALGhostCloneMenuAccess::hostRows"):
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
        # the Recent replay compares an inventory item against its asset id in ONE place (never shown, logged or copied)
        code = code.replace("item->getAssetUUID() == mAsset", "")
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

    # ---- Look / turn, Animation, Appearance ----------------------------------------------
    out += look_turn_findings(src, cpp)

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


def look_turn_findings(src: dict[str, str], cpp: str) -> list[str]:
    out: list[str] = []
    actions_cpp = strip_comments(src["alghostcloneactions.cpp"])
    panel = strip_comments(src["alpanelghoststudio.cpp"])
    logic = src["alghostclonemenulogic.h"]

    # menu structure: items, callbacks, parameters
    try:
        root = ET.fromstring(src["menu_ghost_clone.xml"])
    except ET.ParseError:
        return out

    def sub(parent, name):
        return parent.find(f"context_menu[@name='{name}']") if parent is not None else None

    def call(item):
        data = {e.tag.split(".")[-1]: (e.get("function"), e.get("parameter")) for e in item}
        return data.get("on_click", (None, None))

    look_turn = sub(root, "Look Turn")
    if look_turn is None or [c.get("label") for c in look_turn] != ["Look at", "Turn to face"]:
        out.append("menu_ghost_clone.xml: Look / turn must hold Look at and Turn to face")
    else:
        for name, label_stop, function in (("Look At", "Stop looking", "GhostClone.Look"), ("Turn To Face", "Stop turning", "GhostClone.Turn")):
            node = sub(look_turn, name)
            if node is None or [c.get("label") for c in node] != ["Camera", "Me", "Nearby people", label_stop]:
                out.append(f"menu_ghost_clone.xml: {name} must be Camera, Me, Nearby people, {label_stop}")
                continue
            got = [call(c) for c in node if c.tag == "menu_item_call"]
            if got != [(function, "camera"), (function, "me"), (function, "stop")]:
                out.append(f"menu_ghost_clone.xml: {name} items must call {function} with camera / me / stop: {got}")
        if sub(sub(look_turn, "Look At"), "Look Nearby") is None or sub(sub(look_turn, "Turn To Face"), "Turn Nearby") is None:
            out.append("menu_ghost_clone.xml: the Nearby people submenus are named Look Nearby / Turn Nearby (the code fills them)")
    animation = sub(root, "Animation")
    if animation is None or [c.get("label") for c in animation] != ["Back to live", "Restart animation", "Flip left/right", "Speed", "Recent"] or \
            sub(animation, "Recent Animations") is None:
        out.append("menu_ghost_clone.xml: Animation must hold Back to live, Restart animation, Flip left/right, Speed and Recent")
    appearance = sub(root, "Appearance")
    # Clone transitions (11/16): "Transition to..." sits beside Swap source; the expected list is re-pinned with it.
    if appearance is None or [c.get("label") for c in appearance] != ["Swap source", "Transition to...", "Look", "Fade out", "Fade in"] or \
            sub(appearance, "Swap Source") is None:
        out.append("menu_ghost_clone.xml: Appearance must hold Swap source, Transition to..., Look, Fade out and Fade in")
    elif (lambda node: node is None or [(c.get("name"), call(c)) for c in node if c.tag == "menu_item_call"] != [
            ("Transition Look", ("GhostClone.TransitionTo", "look")),
            ("Transition Overlay Look", ("GhostClone.TransitionTo", "overlay_look")),
            ("Transition Normal Look", ("GhostClone.TransitionTo", "normal"))])(sub(appearance, "Transition To")):
        out.append("menu_ghost_clone.xml: Transition to... must hold Look..., Overlay look... and Normal look, each calling GhostClone.TransitionTo")
    elif [(call(c), [(e.get("function"), e.get("parameter")) for e in c if e.tag.endswith("on_enable")])
          for c in appearance if c.tag == "menu_item_call"] != [
            (("GhostClone.Fade", "out"), [("GhostClone.EnableFade", "out")]),
            (("GhostClone.Fade", "in"), [("GhostClone.EnableFade", "in")])]:
        out.append("menu_ghost_clone.xml: Fade out / Fade in must call GhostClone.Fade and be enabled by GhostClone.EnableFade (out / in)")
    # Look items == the Clones page Render look combo
    look = sub(appearance, "Entity Look")
    page_look = src["panel_ghost_studio.xml"].split('name="entity_look_combo"', 1)
    if look is None or len(page_look) < 2:
        out.append("menu_ghost_clone.xml: Appearance > Look missing")
    else:
        page = sorted((m.group(1), int(m.group(2))) for m in
                      re.finditer(r'<combo_box\.item label="([^"]+)"[^>]*value="(\d+)"', page_look[1].split("</combo_box>", 1)[0]))
        items = []
        for check in look.iter("menu_item_check"):
            params = {e.tag.split(".")[-1]: e.get("parameter") for e in check}
            if params.get("on_click") != params.get("on_check"):
                out.append(f"menu_ghost_clone.xml: Look item {check.get('label')} click/check parameters differ")
            items.append((check.get("label"), int(params.get("on_click") or -1)))
        if sorted(items) != page:
            out.append("menu_ghost_clone.xml: Appearance > Look items differ from the Clones page Render look combo")
    header = src["alghoststudio.h"]
    if "LOOK_NORMAL = 0, LOOK_APPARITION, LOOK_HOLOGRAM, LOOK_CHROME,\n        LOOK_TOON, LOOK_SILHOUETTE" not in header:
        out.append("EGhostLook changed: update Appearance > Look and this check")
    if "look > ALGhostStudio::LOOK_SILHOUETTE" not in function_body(cpp, "void actEntityLook"):
        out.append("actEntityLook must refuse a look outside the EGhostLook range")

    # the look target: no new enum value; the one resolver already finds any avatar by id
    if "LOOK_TARGET_AVATAR" in header or "LOOK_TARGET_AVATAR" in src["alghoststudio.cpp"] or "LOOK_TARGET_AVATAR" in cpp:
        out.append("ELookTarget must not gain a value: LOOK_TARGET_ACTOR already resolves any avatar by agent id")
    if "        LOOK_TARGET_GHOST,\n        // A bare world position" not in header or "LOOK_TARGET_POINT\n    };" not in header:
        out.append("ELookTarget values changed: saved / numeric values must not shift")
    studio_cpp = src["alghoststudio.cpp"]
    target_fn = function_body(studio_cpp, "bool ALGhostStudio::resolveTargetGlobal")
    if "case LOOK_TARGET_ME:\n    case LOOK_TARGET_ACTOR:" not in target_fn or "source_foot_agent(actor, foot)" not in target_fn:
        out.append("resolveTargetGlobal: LOOK_TARGET_ACTOR must resolve through source_foot_agent (LLDirectorCast::resolve)")
    resolve = function_body(strip_comments(src["lldirectorcast.cpp"]), "LLVOAvatar* LLDirectorCast::resolve(const LLUUID& id, bool refresh_name)")
    if "gObjectList.findObject(actor_id)" not in resolve or "obj->asAvatar()" not in resolve or \
            re.search(r"getMember\(|getStoredMember\(|containsStored\(", resolve.split("gObjectList.findObject(actor_id)")[0]):
        out.append("LLDirectorCast::resolve must find any avatar by agent id through gObjectList (no cast membership needed)")

    # shared actions
    for fn, needles in (
            ("bool lookAt", ("studio.setLookTarget(rep, target, target_id, true);", "studio.faceInstance(rep);", "unitRepresentatives(rows)")),
            ("void stopLooking", ("studio.setLookTarget(rep, target, target_id, false);", "unitRepresentatives(rows)")),
            ("bool turnTo", ("ALGhostStudio::TURN_MODE_ONCE", "studio.setTurnTarget(id, ", "instances(rows)")),
            ("void stopTurning", ("ALGhostStudio::TURN_MODE_OFF", "studio.setTurnTarget(id, ", "instances(rows)")),
            ("bool restartRows", ("studio.restartInstanceAnimation(id)", "instances(rows)")),
            ("std::string swapSource", ("studio.swapSource(id, source, label)", "instances(rows)", "findInstanceByRuntime(source)")),
            ("bool setEntityLook", ("studio.setInstanceLook(id, look)", "instances(rows)"))):
        body = function_body(actions_cpp, fn)
        for needle in needles:
            if needle not in body:
                out.append(f"alghostcloneactions.cpp {fn}: expected the page's path {needle}")
    for fn in ("bool lookAt", "void stopLooking", "bool turnTo", "void stopTurning", "bool restartRows", "std::string swapSource",
               "bool setEntityLook", "void noteRecentAnimation", "const std::vector<RecentAnimation>& recentAnimations"):
        if re.search(r"gSavedSettings|LL_INFOS|LL_WARNS|LL_DEBUGS|LLSD|asString\(|setSelected|getString", function_body(actions_cpp, fn)):
            out.append(f"alghostcloneactions.cpp {fn}: session-only, no setting / log / id text / selection change")
    # the Recent list: the one funnel, name only, never an id
    note = function_body(actions_cpp, "void noteRecentAnimation")
    if "ALGhostAnimDrop::mentionsUuid(anim_name, asset_id) ? std::string() : anim_name" not in note or \
            "ALGhostCloneMenuLogic::recentPush(recentList(), asset_id, name)" not in note or "asset_id.isNull()" not in note:
        out.append("noteRecentAnimation: a name that carries the asset id is dropped; the list is the shared newest-first push")
    funnel = function_body(panel, "S32 ALPanelGhostStudio::playAnimationOnClonesRecorded")
    if not re.search(r"const S32 played = studio\.playAnimationOnClones\(ids, asset_id\);\s*if \(played > 0\)\s*\{\s*"
                     r"ALGhostCloneActions::noteRecentAnimation\(asset_id, anim_name\);", funnel):
        out.append("playAnimationOnClonesRecorded (the one funnel: drop, Play on selected clones, Inventory combo, the menu) must note a successful play")
    if len(re.findall(r"noteRecentAnimation\(", panel)) != 1 or "noteRecentAnimation(" in cpp or "recentList()" in cpp:
        out.append("only the play funnel may add to the Recent list")
    # the page handlers call the same actions
    if "ALGhostCloneActions::swapSource(selectedRowValues(), source, label)" not in panel or \
            "ALGhostCloneActions::setEntityLook(selectedRowValues(), look)" not in panel:
        out.append("alpanelghoststudio.cpp: the page's Swap source and Render look handlers must call the shared actions")
    apply_fn = function_body(panel, "void ALPanelGhostStudio::applyInventoryAnimationTo")
    if "playAnimationOnClonesRecorded(plan.mPlay, asset_id, anim_name, director_fallback)" not in apply_fn or \
            "ALGhostAnimDrop::planApply(selected)" not in apply_fn or "bool director_fallback)" not in panel.split("applyInventoryAnimationTo(", 1)[1][:400]:
        out.append("applyInventoryAnimationTo keeps the page's Directed rule and passes the undo fallback through")
    if "bool director_fallback = false);" not in src["alpanelghoststudio.h"].split("applyInventoryAnimationTo(", 1)[1][:400]:
        out.append("alpanelghoststudio.h: applyInventoryAnimationTo(..., bool director_fallback = false) is public for the menu")
    if "applyInventoryAnimationTo(const std::vector<ALGhostAnimDrop::SelectedClone>& selected," not in \
            src["alpanelghoststudio.h"].split("private:", 1)[0]:
        out.append("alpanelghoststudio.h: applyInventoryAnimationTo must be public")
    if "NEARBY_MAX = 16;" not in logic or "RECENT_ANIM_MAX = 8;" not in logic:
        out.append("alghostclonemenulogic.h: Nearby people are capped at 16, Recent animations at 8")

    # the menu: targets, names only, build order
    for m in re.finditer(r"Actions::(lookAt|turnTo|stopLooking|stopTurning|restartRows|swapSource|setEntityLook|fadeRows)\(\s*([^,)]*)", cpp):
        if m.group(2).strip() != "sRows":
            out.append(f"alghostclonemenu.cpp: Actions::{m.group(1)} must act on sRows (the picked clone or the whole multi-selection), not {m.group(2).strip()!r}")
    for fn in ("lookAt", "turnTo", "stopLooking", "stopTurning", "restartRows", "swapSource", "setEntityLook", "fadeRows"):
        if f"Actions::{fn}(sRows" not in cpp:
            out.append(f"alghostclonemenu.cpp: expected Actions::{fn}(sRows ...)")
    face = function_body(cpp, "void actFace")
    for needle in ("liveTargets().empty()", "personNearby(target_id)", "GhostCloneMenuPersonGone",
                   "Actions::turnTo(sRows, target, target_id)", "Actions::lookAt(sRows, target, target_id)"):
        if needle not in face:
            out.append(f"actFace: missing {needle}")
    if "ALGhostStudio::LOOK_TARGET_ACTOR" not in function_body(cpp, "void populateNearbyMenu") or \
            "ALGhostStudio::LOOK_TARGET_ME" not in function_body(cpp, "void actFaceParam") or \
            "ALGhostStudio::LOOK_TARGET_CAMERA" not in function_body(cpp, "void actFaceParam"):
        out.append("Look / turn targets: camera, me, and a nearby avatar through LOOK_TARGET_ACTOR")
    person = function_body(cpp, "bool personNearby")
    if "ALAvatarSubjectPicker::valid(avatar)" not in person or "!avatar->isSelf()" not in person:
        out.append("personNearby: a real avatar (not a clone, animesh or yourself) still in the world")
    nearby = function_body(cpp, "std::vector<NearbyEntry> nearbyPeople")
    for needle in ("ALAvatarSubjectPicker::valid(avatar)", "avatar->isSelf()", "LLCharacter::sInstances", "getFar()",
                   "LLAvatarNameCache::get(avatar->getID(), &av_name)", "getDisplayName()", "nameHiddenByRlva(avatar->getID())",
                   "if (dist_vec(position, camera) > draw_distance)\n        {\n            continue;\n        }",
                   "Logic::nearbyLabel(name, cast.getMember(avatar->getID()) != nullptr, suffix)", "if (entry.mLabel.empty())",
                   "return Logic::nearbySelect(found);", "entry.mDistance = dist_vec(position, origin);"):
        if needle not in nearby:
            out.append(f"nearbyPeople: missing {needle}")
    if "getFullname" in nearby:
        out.append("nearbyPeople: display names only (never the legacy full name); an uncached name is left out")
    if cpp.count("nearbyPeople()") != 2 or "const std::vector<NearbyEntry> people = nearbyPeople();" not in function_body(cpp, "void populateDynamicMenus"):
        out.append("the Nearby people scan runs once per menu open and is shared by Look and Turn")
    rlva = function_body(cpp, "bool nameHiddenByRlva")
    if "RlvActions::isRlvEnabled() && id != gAgentID && !RlvActions::canShowName(RlvActions::SNC_DEFAULT, id)" not in rlva or \
            "if (nameHiddenByRlva(id))" not in function_body(cpp, "std::string castMemberName"):
        out.append("a name RLVa hides (@shownames) is never listed: Nearby people and Swap source")
    face_param = function_body(cpp, "void actFaceParam")
    if 'if (kind != "camera" && kind != "me")' not in face_param:
        out.append("actFaceParam refuses an unknown parameter instead of looking at the camera")
    pop = function_body(cpp, "void populateNearbyMenu")
    if "addListItem(menu, person.mLabel," not in pop or "GhostCloneMenuNobodyNearby" not in pop or "menu->empty();" not in pop:
        out.append("populateNearbyMenu: label is the person's name; a placeholder when nobody is nearby; rebuilt on every open")
    recent = function_body(cpp, "void populateRecentMenu")
    for needle in ("Logic::recentLabel(entry.mName, fallback)", "addListItem(recent, label,", "recent->empty();",
                   "branch->setVisible(Logic::recentSubmenuVisible(list.size()));", "actPlayRecent(asset, label)"):
        if needle not in recent:
            out.append(f"populateRecentMenu: missing {needle}")
    play = function_body(cpp, "void actPlayRecent")
    if "ALPanelGhostStudio::applyInventoryAnimationTo(selected, asset, name, /*director_fallback=*/true)" not in src["alghostclonemenu.cpp"] or \
            "applyInventoryAnimationTo(selected, asset, name," not in play or "liveEntityTargets()" not in play or "DRIVE_DIRECTED" not in play:
        out.append("actPlayRecent: the page's apply (Directed rule, one undo step) on the live entity targets, with the Director undo fallback")
    swap_menu = function_body(cpp, "void populateSwapMenu")
    for needle in ("LLDirectorCast::instance().getActorChoices()", "LLGhostAvatar::isGhostId(choice.mId)", "castMemberName(choice.mId)",
                   "const LLUUID source = self ? LLUUID::null : choice.mId;", "addListItem(menu, name,"):
        if needle not in swap_menu:
            out.append(f"populateSwapMenu: missing {needle}")
    if "return Logic::containsUuidText(name) ? std::string() : name;" not in function_body(cpp, "std::string castMemberName"):
        out.append("castMemberName: a cast member is named, never listed by id")
    dynamic = "\n".join(function_body(cpp, f) for f in ("void populateNearbyMenu", "void populateRecentMenu", "void populateSwapMenu",
                                                         "std::vector<NearbyEntry> nearbyPeople", "std::string castMemberName",
                                                         "void addListItem", "void addPlaceholder",
                                                         "void populateGazeMenus", "void addCheckItem", "std::string gazeCastLabel", "void actGazeCast"))
    if re.search(r"(?:getID\(\)|asset\w*|mAsset|source|target_id|\bid|choice\.mId)\.asString\(\)|LL_INFOS|LL_WARNS|LL_DEBUGS|getAssetUUID", dynamic):
        out.append("the dynamic lists must never format, log or show an id (names only)")
    if "list_item" not in function_body(cpp, "void addListItem") or "Nobody" not in function_body(cpp, "void addPlaceholder"):
        out.append("dynamic list items: names come from the labels passed in")
    present = function_body(cpp, "void presentEntityMenu")
    if not re.search(r"sStaggerCurrent = [^;]*;\s*applyKindLayout\(sAgg\);\s*populateDynamicMenus\(\);\s*sMenu->show\(x, y\);", present):
        out.append("presentEntityMenu: the lists are built for this clone after the targets are fixed and before the menu shows")
    dyn = function_body(cpp, "void populateDynamicMenus")
    for needle in ('subMenu(sMenu, "Look Turn")', 'subMenu(look_turn, "Look At"), "Look Nearby"', 'subMenu(look_turn, "Turn To Face"), "Turn Nearby"',
                   'populateRecentMenu(subMenu(sMenu, "Animation"))', 'subMenu(subMenu(sMenu, "Appearance"), "Swap Source")'):
        if needle not in dyn:
            out.append(f"populateDynamicMenus: missing {needle}")
    reg = function_body(cpp, "void registerEntityCallbacks")
    for needle in ('commit.add("GhostClone.Look", [](LLUICtrl*, const LLSD& param) { actFaceParam(false, param); });',
                   'commit.add("GhostClone.Turn", [](LLUICtrl*, const LLSD& param) { actFaceParam(true, param); });',
                   'commit.add("GhostClone.RestartAnim"', 'commit.add("GhostClone.EntityLook"', "sAgg.mLook == param.asInteger()",
                   'state(ctrl, sAgg.mEntities > 0, "GhostCloneMenuNotReady")'):
        if needle not in reg:
            out.append(f"registerEntityCallbacks: missing {needle}")

    # ---- Fade out / Fade in (hidden-only), Look at clears Turn --------------------------------
    for needle in ('commit.add("GhostClone.Fade", [](LLUICtrl*, const LLSD& param) { actFade(param.asString() == "out"); });',
                   "(fade_out ? sAgg.mFadeOutable : sAgg.mFadeInable) > 0"):
        if needle not in reg:
            out.append(f"registerEntityCallbacks: missing {needle}")
    fade_cb = function_body(cpp, "void actFade")
    for needle in ("Actions::fadeRows(sRows, fade_out)", "if (before.empty())", 'recordCustom(shell, "clone_fade"',
                   "Actions::restoreFade(before, redo, fade_out)", "GhostCloneMenuAlreadyFaded", "GhostCloneMenuNotFaded"):
        if needle not in fade_cb:
            out.append(f"actFade: missing {needle}")
    if not (fade_cb.find("if (before.empty())") < fade_cb.find("LLFloaterReg::getInstance")):
        out.append("actFade: a refusal never constructs the Director")
    agg_body = function_body(cpp, "Aggregate computeAggregate")
    if "Logic::fadeEligible(true, inst->mUserHidden)" not in agg_body or "Logic::fadeEligible(false, inst->mUserHidden)" not in agg_body or \
            agg_body.find("continue;") > agg_body.find("Logic::fadeEligible(true"):
        out.append("computeAggregate: the Fade items count ENTITY targets only, from the explicit hidden state")
    enable_fade = src["alghostclonemenu.cpp"].split('enable.add("GhostClone.EnableFade"', 1)[1][:700]
    if "sAgg.mLive == 0" not in enable_fade or "sAgg.mEntities == 0" not in enable_fade or \
            not (enable_fade.find("sAgg.mLive == 0") < enable_fade.find("sAgg.mEntities == 0") < enable_fade.find("sAgg.mFadeOutable")) or \
            "GhostCloneMenuMirrorEntityOnly" not in enable_fade:
        out.append("EnableFade: not ready first, then entity clones only (an overlay is never faded), then the hidden state")
    if re.search(r"gSavedSettings|requestRemoveInstance|removeInstance|setEnabled|setInstanceEnabled", fade_cb):
        out.append("actFade: hidden-only: no setting write, no delete, no hide switch")
    for fn, needles in (
            ("std::vector<LLUUID> fadeTargets", ("ALGhostCloneMenuLogic::fadeApplies(inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE, fade_out, inst->mUserHidden)", "!inst->mDeleting", "instances(rows)")),
            ("std::vector<FadeBefore> fadeRows", ("fadeTargets(rows, fade_out)", "studio.fadeInstance(id, fade_out, true)", "saved.mHidden = studio.getInstance(id)->mUserHidden")),
            ("bool restoreFade", ("studio.fadeInstance(saved.mId, saved.mHidden, false)", "studio.fadeInstance(saved.mId, fade_out, true)", "!inst->mDeleting"))):
        body = function_body(actions_cpp, fn)
        for needle in needles:
            if needle not in body:
                out.append(f"alghostcloneactions.cpp {fn}: missing {needle}")
        if re.search(r"gSavedSettings|requestRemoveInstance|removeInstance|LL_INFOS|LL_WARNS|LLSD|setSelected", body):
            out.append(f"alghostcloneactions.cpp {fn}: hidden-only and session-only: no delete, setting, log or selection change")
    look_fn = function_body(actions_cpp, "bool lookAt")
    turn_clear = "for (const LLUUID& member : studio.groupMembers(rep))"
    if turn_clear not in look_fn or "studio.setTurnTarget(member, ALGhostStudio::TURN_MODE_OFF" not in look_fn or \
            look_fn.find("hasExpandedMember(rows)") > look_fn.find("unitRepresentatives(rows)") or \
            look_fn.find("unitRepresentatives(rows)") > look_fn.find("reps.empty()") or \
            look_fn.find("reps.empty()") > look_fn.find(turn_clear) or look_fn.find(turn_clear) > look_fn.find("studio.setLookTarget"):
        out.append("lookAt: find the representatives, return when there are none, THEN clear an armed Turn over every member of each representative's unit, THEN set the look target")
    if "stopTurning(rows)" in function_body(actions_cpp, "bool turnTo"):
        out.append("Turn to face is unchanged: it does not touch the look")
    studio_cpp2 = src["alghoststudio.cpp"]
    fade_fn = function_body(studio_cpp2, "bool ALGhostStudio::fadeInstance")
    for needle in ("if (!inst || inst->mDeleting || inst->mKind != BACKING_ENTITY_CLONE) return false;", "isVisible(*inst)",
                   "mFades[id].mFade = {LLTimer::getTotalSeconds(), seconds, inst->mFadeAlpha, hidden};",
                   "inst->mUserHidden = hidden;", "mFades.erase(id);", "++mInstanceRevision;", 'LLCachedControl<F32> fade_out(gSavedSettings, "GhostStudioFadeOut", 0.f)',
                   'LLCachedControl<F32> fade_in(gSavedSettings, "GhostStudioFadeIn", 0.f)'):
        if needle not in fade_fn:
            out.append(f"ALGhostStudio::fadeInstance: missing {needle}")
    if re.search(r"requestRemoveInstance|removeInstance|mDeleting = true|gSavedSettings\.set|mState = STATE_TEARING_DOWN|LLSD", fade_fn):
        out.append("ALGhostStudio::fadeInstance is hidden-only: it never deletes or tears a clone down, never writes a setting, never saves")
    if "if (it->second.mFade.removing && inst->mDeleting) finished.push_back(it->first);" not in function_body(studio_cpp2, "void ALGhostStudio::updateFades") or \
            "if (it->second.mFade.removing) finished" in studio_cpp2:
        out.append("updateFades: a finished fade removes a clone ONLY when it was marked deleting (ALGhostCloneMenuLogic::fadeRemovesClone)")
    if "inst->mDeleting = true;" not in function_body(studio_cpp2, "void ALGhostStudio::requestRemoveInstance"):
        out.append("requestRemoveInstance must still mark every retiring clone deleting (the delete fade ends in removeInstance)")
    cast_cpp = src["lldirectorcast.cpp"]
    if "mFadeAlpha" in cast_cpp or "GhostStudioFade" in cast_cpp:
        out.append("lldirectorcast.cpp: a clone fade is session-only, never part of scene data")
    for scene_fn in ("LLSD LLDirectorCast::sceneData() const", "void LLDirectorCast::applySceneData("):
        if re.search(r"(?i)fade", function_body(cast_cpp, scene_fn)):
            out.append(f"{scene_fn}: no fade state in a scene")
    if not (ROOT / "scripts/code_tools/clone_menu_fade_hunks.json").exists():
        out.append("the reviewed fade allowance layer clone_menu_fade_hunks.json is missing")
    if "bool fadeInstance(const LLUUID& id, bool hidden, bool ramp);" not in src["alghoststudio.h"] or \
            "mUserHidden = false;" not in src["alghoststudio.h"]:
        out.append("alghoststudio.h: fadeInstance and mUserHidden must be declared")
    # the explicit hidden state: Studio owns the hidden coverage (the isVisible gate), whether or not the clone is a Cast member
    if "!(inst.mUserHidden && !(inst.mFadeAlpha > 0.f))" not in src["alghoststudio.h"]:
        out.append("alghoststudio.h: isVisible hides a settled user-hidden clone (the Show clones / Solo gate: neither drawn nor picked, no Cast dissolve needed)")
    if function_body(strip_comments(src["alghoststudio.h"]), "bool isVisible(const Instance& inst) const").count("mUserHidden") != 1:
        out.append("alghoststudio.h: isVisible reads mUserHidden once")
    ramp_part = fade_fn.split("mFades[id].mFade = ", 1)[-1]
    if "setCloneFadeAlpha(inst->mEntityId, inst->mFadeAlpha)" not in ramp_part or \
            ramp_part.find("setCloneFadeAlpha") > ramp_part.find("set_entity_clone_visible"):
        out.append("fadeInstance ramp branch: push the current coverage BEFORE the body is shown (no full-opacity flash on a fade-in)")
    cast_set = function_body(strip_comments(src["lldirectorcast.cpp"]), "void LLDirectorCast::setCloneFadeAlpha")
    cast_gets = strip_comments(src["lldirectorcast.cpp"])
    if "mCloneFadeOrphans" not in cast_set or "mCloneFadeOrphans.erase(id);" not in cast_set or "for (auto it" in cast_set or "gObjectList" in cast_set or \
            "orphan.mDissolveProgress = 1.f-alpha;" not in cast_set or \
            cast_set.find("if (!member)") > cast_set.find("mCloneFadeOrphans.erase(id);        ") or \
            function_body(cast_gets, "const LLDirectorCast::ActorStyle& LLDirectorCast::getRenderActorStyle(const LLUUID& id) const").count("mCloneFadeOrphans.find(id)") != 1 or \
            cast_gets.count("mCloneFadeOrphans.find(primary)") != 1 or cast_gets.count("mCloneFadeOrphans.find(fallback)") != 1 or \
            cast_gets.count("return orphan->second;") != 3 or cast_gets.count("if (!mCloneFadeOrphans.empty())") != 3 or \
            "resolved_id = primary;\n            return orphan->second;" not in cast_gets or \
            "void LLDirectorCast::forgetCloneFade(const LLUUID& runtime_id)\n{\n    mCloneFadeOrphans.erase(runtime_id);\n}" not in cast_gets or \
            "std::map<LLUUID, ActorStyle> mCloneFadeOrphans;" not in src["lldirectorcast.h"] or \
            "void              forgetCloneFade(const LLUUID& runtime_id);" not in src["lldirectorcast.h"]:
        out.append("LLDirectorCast: the fade ramp of a clone that is NOT a Cast member is kept (mCloneFadeOrphans) and read by the renderer getters; a Cast member's path is unchanged")
    if "mCloneFadeOrphans" in function_body(cast_gets, "LLSD LLDirectorCast::sceneData() const") or "mCloneFadeOrphans" in function_body(cast_gets, "void LLDirectorCast::applySceneData("):
        out.append("LLDirectorCast: the orphan fade is session-only, never part of scene data")
    if "headers.find(group->mId) == headers.end()" not in function_body(actions_cpp, "bool hasExpandedMember"):
        out.append("hasExpandedMember: a selected group row owns its member rows (the units() rule)")
    if "GhostCloneMenuInventoryLoading" not in function_body(cpp, "void actPlayRecent") or "GhostCloneMenuInventoryLoading" not in src["strings.xml"]:
        out.append("actPlayRecent: a half-fetched inventory says so (its own string)")
    page_menu = strip_comments(src["alpanelghoststudio.cpp"]).split('mLookTargetCombo->add("(menu target)"', 1)[0][-900:]
    if strip_comments(src["alpanelghoststudio.cpp"]).count('remove("(menu target)")') != 2 or "menu_row" not in page_menu:
        out.append("the Look combo keeps ONE neutral (menu target) row and drops it when stale or listed")
    if fade_fn.count("set_entity_clone_visible(") != 2:
        out.append("fadeInstance refreshes the body's visibility on both the instant and the ramp path (a fade-in shows the body at once)")
    fades = function_body(studio_cpp2, "void ALGhostStudio::updateFades")
    if not re.search(r"else if \(inst->mUserHidden && inst->mKind == BACKING_ENTITY_CLONE\)\s*\{[^{}]*resolveEntityClone\(inst->mId\)[^{}]*set_entity_clone_visible\(ghost, isVisible\(\*inst\)", fades):
        out.append("updateFades: a finished hide re-gates the body through isVisible")
    if not re.search(r"set_entity_clone_visible\(ghost, isVisible\(\*inst\)[^;]*;\s*(?://[^\n]*\n\s*)?if \(inst->mEntityId\.notNull\(\) && !\(inst->mFadeAlpha > 0\.f\)\)\s*LLDirectorCast::instance\(\)\.forgetCloneFade\(inst->mEntityId\);\s*\}\s*it = mFades\.erase\(it\);", fades):
        out.append("updateFades: a clone settled hidden forgets its alpha-0 coverage entry (the empty-map fast path returns)")
    fade_fn = function_body(studio_cpp2, "bool ALGhostStudio::fadeInstance")
    if not re.search(r"if \(inst->mEntityId\.notNull\(\) && !\(inst->mFadeAlpha > 0\.f\)\)\s*LLDirectorCast::instance\(\)\.forgetCloneFade\(inst->mEntityId\);[^\n]*\n\s*return true;\s*\}\s*if \(gAgent\.getRegion\(\)\)", fade_fn):
        out.append("fadeInstance: an instant hide forgets its alpha-0 coverage entry")
    begin = function_body(studio_cpp2, "void ALGhostStudio::beginAppearance")
    if not re.search(r"if \(inst->mUserHidden && !inst->mDeleting\)\s*\{[^{}]*mFades\.erase\(id\);[^{}]*inst->mFadeAlpha = 0\.f;[^{}]*setCloneFadeAlpha\(inst->mEntityId, 0\.f\);[^{}]*set_entity_clone_visible\(ghost, isVisible\(\*inst\)[^{}]*forgetCloneFade\(inst->mEntityId\);[^{}]*return;\s*\}\s*if \(!fade\) return;", begin):
        out.append("beginAppearance: a user-hidden clone stays hidden (swap, duplicate, crowd fade-in) before any creation fade starts")
    replaced = function_body(studio_cpp2, "void ALGhostStudio::onEntityRuntimeReplaced")
    if not re.search(r"if \(!removing_instance && old_runtime != new_runtime\)\s*if \(Instance\* current = getInstance\(stable_id\)\)\s*\{\s*if \(current->mUserHidden && !current->mDeleting\)\s*\{\s*mFades\.erase\(stable_id\);\s*current->mFadeAlpha = 0\.f;\s*"
                     r"LLDirectorCast::instance\(\)\.setCloneFadeAlpha\(new_runtime, 0\.f\);\s*if \(LLGhostAvatar\* replaced = resolveEntityClone\(stable_id\)\)\s*"
                     r"set_entity_clone_visible\(replaced, isVisible\(\*current\)[^;]*;\s*LLDirectorCast::instance\(\)\.forgetCloneFade\(new_runtime\);[^\n]*\n\s*\}\s*else if \(mFades\.count\(stable_id\)\)\s*\{\s*"
                     r"LLDirectorCast::instance\(\)\.setCloneFadeAlpha\(new_runtime, current->mFadeAlpha\);", replaced) or \
            "if (old_runtime.notNull() && old_runtime != new_runtime)\n    {\n        LLDirectorCast::instance().forgetCloneFade(old_runtime);" not in replaced or \
            replaced.find("consumer(stable_id") > replaced.find("current->mUserHidden") or replaced.find("consumer(stable_id") > replaced.find("forgetCloneFade"):
        out.append("onEntityRuntimeReplaced: a replaced body of a user-hidden clone is hidden again AFTER the Cast rekey, and a ramp in flight is cancelled at 0 (stored and rendered coverage agree)")
    remove_fn = function_body(studio_cpp2, "void ALGhostStudio::requestRemoveInstance")
    if "if (seconds <= 0.f || all_invisible) { removeInstance(id); return; }" not in remove_fn or "!(item->mFadeAlpha > 0.f)" not in remove_fn:
        out.append("requestRemoveInstance: a clone that is already invisible is removed at once (no 0-to-0 ramp)")
    if studio_cpp2.count("if (copy->mUserHidden) fadeInstance(copy->mId, false, true);") != 2 or "fadeInstance(copy->mId, false, false)" in studio_cpp2:
        out.append("arrays / formations and crowds: ONLY a copy that inherited a hidden prototype is unhidden, with the normal Fade-in ramp (a visible prototype's copy keeps its creation fade-in); Duplicate keeps copying the flag")
    # Studio look / turn is exactly the base: no member-edit special case anywhere
    if "isExpandedMember" in studio_cpp2 or "isExpandedMember" in src["alghoststudio.h"] or "look_members" in studio_cpp2:
        out.append("Studio look / turn behaviour and per-frame cost are the base's: no isExpandedMember special case")
    if "for (const LLUUID& member_id : groupMembers(id))\n    {\n        if (Instance* member = getInstance(member_id))\n        {\n            member->mLookTarget = target;" not in \
            function_body(studio_cpp2, "void ALGhostStudio::setLookTarget") and \
            "for (const LLUUID& member_id : groupMembers(id))" not in function_body(studio_cpp2, "void ALGhostStudio::setLookTarget"):
        out.append("setLookTarget acts on the whole authoring unit (groupMembers), as the base does")
    if "hasExpandedMember(rows)" not in function_body(actions_cpp, "bool lookAt") or "hasExpandedMember(rows)" not in function_body(actions_cpp, "void stopLooking") or \
            "hasExpandedMember(rows)" not in function_body(actions_cpp, "bool turnTo") or "hasExpandedMember(rows)" not in function_body(actions_cpp, "void stopTurning"):
        out.append("look / stop looking / turn / stop turning refuse a target set that holds a member of an expanded group (nothing is written)")
    if "mEditMembers" in function_body(actions_cpp, "std::vector<LLUUID> unitRepresentatives"):
        out.append("unitRepresentatives: a group header is one unit (its first live member), as before; expanded members are refused by hasExpandedMember")
    if "agg.mExpandedMember = Actions::hasExpandedMember(sRows);" not in function_body(cpp, "Aggregate computeAggregate") or \
            'return state(ctrl, false, "GhostCloneMenuWholeGroup");' not in src["alghostclonemenu.cpp"].split('enable.add("GhostClone.EnableFace"', 1)[1][:500] or \
            "sAgg.mLive > 0 && sAgg.mExpandedMember" not in src["alghostclonemenu.cpp"]:
        out.append("EnableFace: Look / turn items are disabled with 'applies to the whole group - use the group row' on an expanded member")
    for fn in ("void actFace", "void actFaceParam"):
        if "Actions::hasExpandedMember(sRows)" not in function_body(cpp, fn) or "GhostCloneMenuWholeGroup" not in function_body(cpp, fn):
            out.append(f"{fn} refuses an expanded member at commit too")
    if "sAgg.mLive > 0 && !sAgg.mExpandedMember" not in function_body(cpp, "void populateNearbyMenu"):
        out.append("Nearby people items follow the same expanded-member refusal")
    try:
        for name in ("Look Turn",):
            lt = ET.fromstring(src["menu_ghost_clone.xml"]).find(f"context_menu[@name='{name}']")
            funcs = [e.get("function") for e in lt.iter() if e.tag.endswith("on_enable")] if lt is not None else []
            if funcs != ["GhostClone.EnableFace"] * 6:
                out.append(f"menu_ghost_clone.xml: every Look / turn item is enabled by GhostClone.EnableFace: {funcs}")
    except ET.ParseError:
        pass
    if "GhostCloneMenuWholeGroup" not in src["strings.xml"]:
        out.append("strings.xml: GhostCloneMenuWholeGroup")
    swap_fn = function_body(cpp, "void actSwapSource")
    if "ALPanelGhostStudio::sourceName(source)" not in swap_fn:
        out.append("actSwapSource: the Source column label comes from the page's own sourceName helper")
    hdr_public = src["alpanelghoststudio.h"].split("private:", 1)[0]
    if "static std::string sourceName(const LLUUID& id);" not in hdr_public:
        out.append("alpanelghoststudio.h: sourceName is public (shared with the clone menu)")
    if 'mLookTargetCombo->add("(menu target)", LLSD(target));' not in src["alpanelghoststudio.cpp"]:
        out.append("the page's Look combo shows a neutral (menu target) row for a look target it does not list")
    if "bool canOpenForListRow(const LLUUID& row);" not in src["alghostclonemenu.h"] or "bool canOpenForListRow(const LLUUID& row)" not in cpp:
        out.append("canOpenForListRow is the validation the panel asks before it changes anything")
    if "LLScrollListItem* getItemAtPoint(S32 x, S32 y)" not in read(ROOT / "indra/llui/llscrolllistctrl.h"):
        out.append("llscrolllistctrl.h: getItemAtPoint (the row under a point) is how the Clones list right click finds its row")
    recent_play = function_body(cpp, "void actPlayRecent")
    for needle in ("struct AssetMatcher : public LLInventoryCollectFunctor", "gInventory.collectDescendentsIf(", "item->getPermissions().getOwner() == gAgentID",
                   "if (items.empty())", "LLInventoryModelBackgroundFetch::instance().isEverythingFetched()", "Actions::removeRecentAnimation(asset);",
                   'args["[NAME]"] = name;'):
        if needle not in recent_play:
            out.append(f"actPlayRecent: an asset-id inventory matcher; an entry is dropped only when the inventory is fully fetched and the asset is not in it (name-only toast): missing {needle}")
    if "refreshInventory" in recent_play or recent_play.find("collectDescendentsIf") > recent_play.find("applyInventoryAnimationTo"):
        out.append("actPlayRecent: no whole-inventory rebuild; the matcher runs before anything plays")
    if recent_play.find("isEverythingFetched") > recent_play.find("removeRecentAnimation"):
        out.append("actPlayRecent: an entry is removed only after the inventory is known to be fully fetched")
    page_look = function_body(strip_comments(src["alpanelghoststudio.cpp"]), "void ALPanelGhostStudio::onLookTargetCommit")
    if "if (!from_combo && inst->mLookTarget == ALGhostStudio::LOOK_TARGET_ACTOR)" not in page_look or "representable" not in page_look:
        out.append("the page's Look combo must not rewrite a look target it cannot show (Keep facing / Face now keep a non-cast avatar target)")
    if "return fade_out ? !hidden : hidden;" not in logic or "return removing && deleting;" not in logic or "return entity && fadeEligible(fade_out, hidden);" not in logic:
        out.append("alghostclonemenulogic.h: fadeEligible / fadeRemovesClone are the rules the native fixture proves")
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
        ("right-click inside the multi-selection leaves the old primary highlighted",
         swap(src, "alghostclonemenu.cpp", "ALDirectorSelect::makePrimaryInSelection(clone);", "")),
        ("right-click primary change always rewrites the shared selection",
         swap(src, "aldirectorselect.cpp", "    if (current == row)\n    {\n        return;\n    }\n", "")),
        ("makePrimaryInSelection toggles through the additive path",
         swap(src, "aldirectorselect.cpp", "ALGhostCloneMenuAccess::promotePrimary(instance_id);", "ALGhostCloneMenuAccess::selectInHosts(instance_id, true);")),
        ("promotion adds a child row of a selected group header",
         swap(src, "aldirectorselect.cpp", "        row = group->mId;\n", "        row = instance_id;\n")),
        ("promotion compares the raw selection (collapsed group churn)",
         swap(src, "aldirectorselect.cpp", "ALGhostCloneActions::rowFor(studio.getSelected())", "studio.getSelected()")),
        ("makePrimaryInSelection no longer primes the host list",
         swap(src, "aldirectorselect.cpp", "ALGhostCloneMenuAccess::promotePrimary(instance_id);\n}", "(void)instance_id;\n}")),
        ("clone pick back to the nearest probe only",
         swap(src, "alghoststudio.cpp", "ALGhostCloneMenuLogic::choosePickedClone<LLUUID>(", "pickNearestProbeOnly<LLUUID>(")),
        ("clone pick no longer uses the world ray",
         swap(src, "alghoststudio.cpp", "gViewerWindow->mouseDirectionGlobal(x, y)", "cam->getAtAxis()")),
        ("clone pick reports the box centre instead of the entry point",
         swap(src, "alghoststudio.cpp", "eye + ray_dir * candidate.mEntryT", "LLVector3(box.mCenter)")),
        ("clone pick ignores the near clip",
         swap(src, "alghoststudio.cpp", "box, near_t, entry_t)", "box, 0.f, entry_t)")),
        ("clone pick compares the raw forward near depth with the ray distance",
         swap(src, "alghoststudio.cpp", "ALGhostCloneMenuLogic::rayNearDistance(cam->getNear(), ray_dir.mV, cam->getAtAxis().mV, near_t)", "(near_t = cam->getNear()) > 0.f")),
        ("clone pick gives a bodiless entity clone a fallback box",
         swap(src, "alghoststudio.cpp", "has_body_box = ghost && !ghost->isDead() && ghost->mDrawable.notNull() && !ghost->mDrawable->isDead();", "has_body_box = true;")),
        ("Edit tool stock pick ignores the foreground rule",
         swap(src, "altoolghostedit.cpp", "ALDirectorSelect::cloneInFrontOfPick(pick_info, probe_agent)", "true")),
        ("menu pick ignores the foreground rule",
         swap(src, "aldirectorselect.cpp", "return cloneInFrontOfPick(pick, probe_agent) ? nearby : LLUUID::null;", "return nearby;")),
        ("clone pick stops reserving candidate storage",
         swap(src, "alghoststudio.cpp", "    candidates.reserve(mInstances.size());\n", "")),
        ("getRenderedBounds stubbed to false",
         swap(src, "llghostavatar.cpp", "bool LLGhostAvatar::getRenderedBounds(LLVector3& center, LLVector3& size) const\n{\n",
              "bool LLGhostAvatar::getRenderedBounds(LLVector3& center, LLVector3& size) const\n{\n    return false;\n")),
        ("clone pick drops the shared include",
         swap(src, "alghoststudio.cpp", '#include "alghostclonemenulogic.h" // [ClonePick] which clone is under the cursor\n', "")),
        ("clone pick region back to the selection-box-only bounds",
         swap(src, "alghoststudio.cpp", "ghost->getRenderedBounds(center, size)", "ghost->getVisualBounds(center, size)")),
        ("clone pick fallback box ignores the clone size",
         swap(src, "alghoststudio.cpp", "ALGhostCloneMenuLogic::scaledBodyBox(foot.mV, inst.mScale)", "ALGhostCloneMenuLogic::scaledBodyBox(foot.mV, 1.f)")),
        ("rendered bounds gated on the selection-box mode",
         swap(src, "llghostavatar.cpp", "if (!isBuilt() || !ALGhostBounds::centerAndSize(", "if (!mUseVisualBounds || !isBuilt() || !ALGhostBounds::centerAndSize(")),
        ("Edit clones tool stops using the shared pick",
         swap(src, "altoolghostedit.cpp", "pickInstanceAtScreen(x, y, GHOST_PICK_PX)", "pickInstanceNearest(x, y, GHOST_PICK_PX)")),
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
        ("Style branch: one item with a swapped style id",
         swap(src, "menu_ghost_clone.xml", 'name="ov_style_9">\n          <menu_item_check.on_click function="GhostClone.Style" parameter="9" />',
              'name="ov_style_9">\n          <menu_item_check.on_click function="GhostClone.Style" parameter="8" />')),
        ("Style branch: a label differs from the avatar menus",
         swap(src, "menu_ghost_clone.xml", 'label="Neon Outline"', 'label="Neon Outlinex"')),
        ("Style branch: a group is renamed",
         swap(src, "menu_ghost_clone.xml", 'label="Classic"', 'label="Classicx"')),
        ("Style branch: an item clicks the avatar callback",
         swap(src, "menu_ghost_clone.xml", 'function="GhostClone.Style" parameter="3"', 'function="GhostOverlay.Style" parameter="3"')),
        ("Style branch missing from the clone menu",
         swap(src, "menu_ghost_clone.xml", 'name="Clone Style"', 'name="Clone Stylex"')),
        ("Style commit acts on every target, not only overlays",
         swap(src, "alghostclonemenu.cpp", "const std::vector<LLUUID> overlays = liveOverlayTargets();", "const std::vector<LLUUID> overlays = liveTargets();")),
        ("Style check shows a style when the overlays differ",
         swap(src, "alghostclonemenu.cpp", "agg.mOverlayStyle = -1;", "agg.mOverlayStyle = agg.mOverlayStyle;")),
        ("avatar menus' Overlay Style callback changed",
         swap(src, "menu_avatar_self.xml", 'function="GhostOverlay.Style" parameter="0"', 'function="GhostClone.Style" parameter="0"')),
        ("all-overlay menu forgets to hide the entity-only items",
         swap(src, "alghostclonemenu.cpp", "item->setVisible(entity_items);", "item->setVisible(true);")),
        ("all-overlay menu forgets to hide Stagger",
         swap(src, "alghostclonemenu.cpp", "stagger->setVisible(false);     // an overlay group has no start offsets to stagger", "")),
        ("all-overlay menu no longer hides Appearance",
         swap(src, "alghostclonemenu.cpp", '"Appearance", "Clear animations" };', '"Clear animations" };')),
        ("all-overlay menu no longer hides Gaze",
         swap(src, "alghostclonemenu.cpp", '"Look Turn", "Gaze", "Animation",', '"Look Turn", "Animation",')),
        ("overlay menu hides Edit",
         swap(src, "alghostclonemenu.cpp", '"Appearance", "Clear animations" };', '"Appearance", "Clear animations", "Edit" };')),
        ("Style branch always visible",
         swap(src, "alghostclonemenu.cpp", "style->setVisible(Logic::styleVisible(agg.mOverlays));", "style->setVisible(true);")),
        ("kind layout never applied",
         swap(src, "alghostclonemenu.cpp", "    applyKindLayout(sAgg);\n    populateDynamicMenus();", "    populateDynamicMenus();")),
        ("separators left untidy",
         swap(src, "alghostclonemenu.cpp", "    tidySeparators(sMenu);\n    sMenu->arrange();", "    sMenu->arrange();")),
        ("trailing separator kept",
         swap(src, "alghostclonemenu.cpp", "last_shown_separator->setVisible(false);    // nothing visible after it", "")),
        ("Pause re-shown for overlay targets by its on_visible",
         swap(src, "alghostclonemenu.cpp", "return Logic::entityItemsVisible(sAgg.mEntities) && !Logic::allOf(sAgg.mPaused, sAgg.mEntities);", "return !Logic::allOf(sAgg.mPaused, sAgg.mEntities);")),
        ("Style shown without an overlay target",
         swap(src, "alghostclonemenulogic.h", "inline bool styleVisible(std::size_t overlay_targets)\n{\n    return overlay_targets > 0;", "inline bool styleVisible(std::size_t overlay_targets)\n{\n    return true;")),
        ("Entity Look no longer hidden",
         swap(src, "alghostclonemenu.cpp", "look->setVisible(false);", "look->setVisible(true);")),
        ("Back to live missing from the Animation submenu",
         swap(src, "menu_ghost_clone.xml", 'name="Back to live"', 'name="Back to livex"')),
        ("Back to live skips the unpause",
         swap(src, "alghostclonemenu.cpp", "    Actions::setPaused(entities, false);\n    std::vector<LLUUID> off_mirror;", "    std::vector<LLUUID> off_mirror;")),
        ("Back to live sets the wrong drive",
         swap(src, "alghostclonemenu.cpp", "Actions::setDriveMode(off_mirror, ALGhostStudio::DRIVE_MIRROR, LLUUID::null);", "Actions::setDriveMode(off_mirror, ALGhostStudio::DRIVE_REFLECT, LLUUID::null);")),
        ("Back to live always enabled",
         swap(src, "alghostclonemenu.cpp", "return state(ctrl, sAgg.mNotLive > 0,", "return state(ctrl, true,")),
        ("Gaze menu after Animation instead of Look / turn",
         swap(src, "menu_ghost_clone.xml", '  <context_menu\n   label="Gaze"\n   name="Gaze">', '  <context_menu\n   label="Gazex"\n   name="Gazex">')),
        ("Gaze Camera sends the wrong target",
         swap(src, "menu_ghost_clone.xml", 'function="GhostClone.Gaze" parameter="camera"', 'function="GhostClone.Gaze" parameter="travel"')),
        ("Gaze Me targets the camera",
         swap(src, "alghostclonemenu.cpp", "actGaze(LLActorMover::GAZE_CAST, gAgentID, false);", "actGaze(LLActorMover::GAZE_CAMERA, LLUUID::null, false);")),
        ("Gaze Off does not turn gaze off",
         swap(src, "alghostclonemenu.cpp", "actGaze(-1, LLUUID::null, true);", "actGaze(-1, LLUUID::null, false);")),
        ("Gaze writes the mover directly",
         swap(src, "alghostclonemenu.cpp", "Actions::applyGazeTarget(entities, mode, cast_target, !off);", "LLActorMover::instance().setGazeEnabled(entities.front(), !off);")),
        ("Gaze ignores the expanded-member rule",
         swap(src, "alghostclonemenu.cpp", "    if (Actions::hasExpandedMember(sRows))\n    {\n        tip(str(\"GhostCloneMenuWholeGroup\"));\n        return;\n    }\n    const std::vector<LLUUID> entities", "    const std::vector<LLUUID> entities")),
        ("Gaze records no undo step",
         swap(src, "alghostclonemenu.cpp", 'recordCustom(shell, "clone_gaze"', 'recordCustom(shell, "clone_gazex"')),
        ("Gaze undo restores the new state",
         swap(src, "alghostclonemenu.cpp", "Actions::restoreGaze(redo ? history->mAfter : history->mBefore)", "Actions::restoreGaze(history->mAfter)")),
        ("Gaze cast list offers the clone itself",
         swap(src, "alghostclonemenu.cpp", "std::find(own.begin(), own.end(), choice.mId) != own.end()", "false")),
        ("Gaze cast list offers You",
         swap(src, "alghostclonemenu.cpp", "if (LLDirectorCast::isSelf(choice.mId) ||\n                std::find(", "if (std::find(")),
        ("Gaze nearby ignores RLV @shownearby",
         swap(src, "alghostclonemenu.cpp", "if (ALGazeNearby::nearbyAllowed())", "if (true)")),
        ("Gaze nearby lists cast members again",
         swap(src, "alghostclonemenu.cpp", "offered.count(person.mId) || LLDirectorCast::instance().getMember(person.mId)", "false")),
        ("Gaze check mark ignores disagreeing targets",
         swap(src, "alghostclonemenu.cpp", "if (!sAgg.mGazeAgree || sAgg.mEntities == 0 || sAgg.mGazeBodies == 0)", "if (sAgg.mEntities == 0 || sAgg.mGazeBodies == 0)")),
        ("Gaze lists not rebuilt on open",
         swap(src, "alghostclonemenu.cpp", "    populateGazeMenus(people);\n", "")),
        ("page gaze toggle bypasses the shared write",
         swap(src, "alpanelghoststudio.cpp", "ALGhostCloneActions::applyGazeTarget(\n        ALGhostCloneActions::gazeEntities(selectedInstances()),\n        mLensGazeTargetMode->getValue().asInteger(), LLUUID::null, mLensGazeCheck->get());",
              "LLActorMover::instance().setGazeEnabled(LLUUID::null, mLensGazeCheck->get());")),
        ("shared gaze write forgets the enabled flag",
         swap(src, "alghostcloneactions.cpp", "        mover.setGazeEnabled(entity, enabled);\n    }\n}\n\nstd::vector<GazeBefore>", "    }\n}\n\nstd::vector<GazeBefore>")),
        ("Speed ladder: a wrong rung",
         swap(src, "alghostclonemenulogic.h", "{ 0.25f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 3.f, 4.f }", "{ 0.25f, 0.5f, 0.75f, 1.f, 1.25f, 1.5f, 2.f, 2.5f, 4.f }")),
        ("Speed ladder: faster skips a rung",
         swap(src, "alghostclonemenulogic.h", "if (ANIM_SPEED_LADDER[i] > current + ANIM_SPEED_EPSILON)", "if (ANIM_SPEED_LADDER[i] > current + 0.3f)")),
        ("Speed ladder: slower can raise a very slow clone",
         swap(src, "alghostclonemenulogic.h", "        }\n    }\n    return current;\n}\n\n// Which Stagger", "        }\n    }\n    return ANIM_SPEED_LADDER[0];\n}\n\n// Which Stagger")),
        ("Speed ladder: the top clamp removed",
         swap(src, "alghostclonemenulogic.h", "        return current;\n    }\n    for (int i = ANIM_SPEED_RUNGS - 1;", "        return ANIM_SPEED_LADDER[ANIM_SPEED_RUNGS - 1] + 1.f;\n    }\n    for (int i = ANIM_SPEED_RUNGS - 1;")),
        ("Speed shared write loses its clamp",
         swap(src, "alghostcloneactions.cpp", "    speed = llclamp(speed, 0.05f, 4.f);\n    ALGhostStudio& studio = ALGhostStudio::instance();\n    for (const LLUUID& id : instances(rows))\n    {\n        studio.setInstanceAnimSpeed(id, speed);", "    ALGhostStudio& studio = ALGhostStudio::instance();\n    for (const LLUUID& id : instances(rows))\n    {\n        studio.setInstanceAnimSpeed(id, speed);")),
        ("Speed step uses one speed for every target",
         swap(src, "alghostcloneactions.cpp", "ALGhostCloneMenuLogic::steppedAnimSpeed(inst->mAnimSpeed, direction)", "ALGhostCloneMenuLogic::steppedAnimSpeed(1.f, direction)")),
        ("page Anim speed spinner bypasses the shared write",
         swap(src, "alpanelghoststudio.cpp", "    ALGhostCloneActions::setAnimSpeed(selectedInstances(), (F32)mAnimSpeedSpin->getValue().asReal());", "    for (const LLUUID& id : selectedInstances()) ALGhostStudio::instance().setInstanceAnimSpeed(id, 1.f);")),
        ("Speed menu writes the studio directly",
         swap(src, "alghostclonemenu.cpp", "Actions::setAnimSpeed(sRows, 1.f);", "ALGhostStudio::instance().setInstanceAnimSpeed(sPicked, 1.f);")),
        ("Speed Faster stays enabled at the top",
         swap(src, "alghostclonemenu.cpp", "sAgg.mAtFastest < sAgg.mEntities", "true")),
        ("Speed Slower enabled at the bottom",
         swap(src, "alghostclonemenu.cpp", "sAgg.mAtSlowest < sAgg.mEntities", "true")),
        ("Speed Reset enabled when already 1",
         swap(src, "alghostclonemenu.cpp", "sAgg.mAtNormal < sAgg.mEntities", "true")),
        ("Speed ignores the expanded-member rule",
         swap(src, "alghostclonemenu.cpp", "    if (Actions::hasExpandedMember(sRows))\n    {\n        tip(str(\"GhostCloneMenuWholeGroup\"));\n        return;\n    }\n    const std::vector<Actions::SpeedBefore> before", "    const std::vector<Actions::SpeedBefore> before")),
        ("Speed records no undo step",
         swap(src, "alghostclonemenu.cpp", 'recordCustom(shell, "clone_anim_speed"', 'recordCustom(shell, "clone_anim_speedx"')),
        ("Speed undo restores the new speeds",
         swap(src, "alghostclonemenu.cpp", "Actions::restoreAnimSpeed(redo ? after : before)", "Actions::restoreAnimSpeed(after)")),
        ("Speed info shows a speed when the targets differ",
         swap(src, "alghostclonemenu.cpp", "if (sAgg.mEntities > 0 && sAgg.mSpeedSame)", "if (sAgg.mEntities > 0)")),
        ("Speed info never refreshed",
         swap(src, "alghostclonemenu.cpp", "    refreshSpeedInfo();\n", "")),
        ("Speed Faster wired to Slower",
         swap(src, "menu_ghost_clone.xml", 'function="GhostClone.Speed" parameter="faster"', 'function="GhostClone.Speed" parameter="slower"')),
        ("Gaze cast label ignores RLV name hiding",
         swap(src, "alghostclonemenu.cpp", "if (nameHiddenByRlva(choice.mId) || choice.mLabel.empty() || Logic::containsUuidText(choice.mLabel))", "if (choice.mLabel.empty() || Logic::containsUuidText(choice.mLabel))")),
        ("Gaze cast label may be a UUID",
         swap(src, "alghostclonemenu.cpp", "if (nameHiddenByRlva(choice.mId) || choice.mLabel.empty() || Logic::containsUuidText(choice.mLabel))", "if (nameHiddenByRlva(choice.mId) || choice.mLabel.empty())")),
        ("Gaze cast label falls back to the id",
         swap(src, "alghostclonemenu.cpp", 'return name.empty() ? std::string("Unknown") : name;\n    }\n    return choice.mLabel;', 'return name.empty() ? choice.mId.asString() : name;\n    }\n    return choice.mLabel;')),
        ("Gaze history never rewrites a respawned target",
         swap(src, "alghostclonemenu.cpp", "                    Actions::rewriteGazeReferences(history->mAfter, old_id, new_id);\n", "")),
        ("Gaze history rewrites only the before snapshot",
         swap(src, "alghostclonemenu.cpp", "Actions::rewriteGazeReferences(history->mAfter, old_id, new_id);", "(void)history;")),
        ("Gaze rewrite keeps CAST mode for a deleted target",
         swap(src, "alghostcloneactions.cpp", "saved.mMode = LLActorMover::GAZE_TANGENT;", "saved.mMode = saved.mMode;")),
        ("Gaze rewrite does not follow the new id",
         swap(src, "alghostcloneactions.cpp", "saved.mCast = new_id;", "saved.mCast = old_id;")),
        ("Gaze cast click does not revalidate",
         swap(src, "alghostclonemenu.cpp", "[id]() { actGazeCast(id); }", "[id]() { actGaze(LLActorMover::GAZE_CAST, id, false); }")),
        ("Gaze cast click accepts a retired id",
         swap(src, "alghostclonemenu.cpp", "    if (!current || std::find(own.begin(), own.end(), id) != own.end())", "    if (std::find(own.begin(), own.end(), id) != own.end())")),
        ("Gaze items enabled without a spawned body",
         swap(src, "alghostclonemenu.cpp", "    return sAgg.mGazeBodies > 0 && !sAgg.mExpandedMember;", "    return sAgg.mEntities > 0 && !sAgg.mExpandedMember;")),
        ("Gaze Off checked without a spawned body",
         swap(src, "alghostclonemenu.cpp", "sAgg.mEntities == 0 || sAgg.mGazeBodies == 0", "sAgg.mEntities == 0")),
        ("Back to live tips 'already live' after a successful unpause",
         swap(src, "alghostclonemenu.cpp", "        if (!resumed)\n        {", "        if (true)\n        {")),
        ("Back to live tooltip no longer matches its enable rule",
         swap(src, "menu_ghost_clone.xml", "Reflect or Pause).", "or Reflect).")),
        ("Speed validates its parameter after the tips",
         swap(src, "alghostclonemenu.cpp", "void actSpeed(const std::string& kind)\n{\n    if (kind != \"faster\" && kind != \"slower\" && kind != \"reset\")\n    {\n        return;                         // an unknown parameter never produces a tip or a change\n    }\n    if (liveEntityTargets().empty())", "void actSpeed(const std::string& kind)\n{\n    if (liveEntityTargets().empty())")),
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
         swap(src, "alghostclonemenu.cpp", "    sPicked = clone;\n    // [ClonePick]",
              "    sRows = std::vector<LLUUID>{ sPicked };\n    sPicked = clone;\n    // [ClonePick]")),
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
         swap(src, "alpanelghoststudio.cpp", "    if (!shell && director_fallback && played > 0)", "    if (false)")),
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
         swap(src, "alpanelghoststudio.cpp", "    if (!shell && director_fallback && played > 0)", "    if (!shell && !host && director_fallback && played > 0)")),
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
        # Look / turn, Animation, Appearance
        ("Nearby list shows a UUID for a nameless person",
         swap(src, "alghostclonemenu.cpp", "entry.mLabel = Logic::nearbyLabel(name, cast.getMember(avatar->getID()) != nullptr, suffix);",
              "entry.mLabel = name.empty() ? avatar->getID().asString() : name;")),
        ("Nearby list not nearest first",
         swap(src, "alghostclonemenu.cpp", "return Logic::nearbySelect(found);", "return found;")),
        ("Nearby list includes yourself",
         swap(src, "alghostclonemenu.cpp", "if (!avatar || avatar->isSelf() || !ALAvatarSubjectPicker::valid(avatar))", "if (!avatar || !ALAvatarSubjectPicker::valid(avatar))")),
        ("Nearby list includes clones and animesh",
         swap(src, "alghostclonemenu.cpp", "avatar->isSelf() || !ALAvatarSubjectPicker::valid(avatar))\n        {\n            continue;", "avatar->isSelf())\n        {\n            continue;")),
        ("Nearby list labels by the avatar id",
         swap(src, "alghostclonemenu.cpp", "addListItem(menu, person.mLabel,", "addListItem(menu, person.mId.asString(),")),
        ("Recent list exposes the asset id",
         swap(src, "alghostclonemenu.cpp", "Logic::recentLabel(entry.mName, fallback)", "entry.mName.empty() ? entry.mAsset.asString() : entry.mName")),
        ("Recent list keeps a name that is the asset id",
         swap(src, "alghostcloneactions.cpp", "ALGhostAnimDrop::mentionsUuid(anim_name, asset_id) ? std::string() : anim_name", "anim_name")),
        ("Recent list never hides when empty",
         swap(src, "alghostclonemenu.cpp", "branch->setVisible(Logic::recentSubmenuVisible(list.size()));", "branch->setVisible(true);")),
        ("Recent list capped at 80",
         swap(src, "alghostclonemenulogic.h", "RECENT_ANIM_MAX = 8;", "RECENT_ANIM_MAX = 80;")),
        ("Nearby list capped at 160",
         swap(src, "alghostclonemenulogic.h", "NEARBY_MAX = 16;", "NEARBY_MAX = 160;")),
        ("the play funnel no longer feeds the Recent list",
         swap(src, "alpanelghoststudio.cpp", "        ALGhostCloneActions::noteRecentAnimation(asset_id, anim_name);\n", "")),
        ("the Recent list is also fed from the menu",
         swap(src, "alghostclonemenu.cpp", "void actPlayRecent(const LLUUID& asset, const std::string& name)\n{\n",
              "void actPlayRecent(const LLUUID& asset, const std::string& name)\n{\n    Actions::noteRecentAnimation(asset, name);\n")),
        ("Recent replay without the Director undo fallback",
         swap(src, "alghostclonemenu.cpp", "selected, asset, name, /*director_fallback=*/true", "selected, asset, name")),
        ("Look at acts on the picked clone only",
         swap(src, "alghostclonemenu.cpp", "Actions::lookAt(sRows, target, target_id)", "Actions::lookAt(std::vector<LLUUID>{ sPicked }, target, target_id)")),
        ("Look at acts outside the selection (every clone)",
         swap(src, "alghostclonemenu.cpp", "Actions::lookAt(sRows, target, target_id)", "Actions::lookAt(ALDirectorSelect::hostRows(), target, target_id)")),
        ("Turn acts on a different set",
         swap(src, "alghostclonemenu.cpp", "Actions::turnTo(sRows, target, target_id)", "Actions::turnTo(liveTargets(), target, target_id)")),
        ("Stop looking acts on a different set",
         swap(src, "alghostclonemenu.cpp", "Actions::stopLooking(sRows)", "Actions::stopLooking(std::vector<LLUUID>{ sPicked })")),
        ("Restart animation acts on a non-target",
         swap(src, "alghostclonemenu.cpp", "Actions::restartRows(sRows)", "Actions::restartRows(ALDirectorSelect::hostRows())")),
        ("Swap source acts on a non-target",
         swap(src, "alghostclonemenu.cpp", "Actions::swapSource(sRows, source, ALPanelGhostStudio::sourceName(source))", "Actions::swapSource(std::vector<LLUUID>{ sPicked }, source, ALPanelGhostStudio::sourceName(source))")),
        ("Render look acts on a non-target",
         swap(src, "alghostclonemenu.cpp", "Actions::setEntityLook(sRows,", "Actions::setEntityLook(ALDirectorSelect::hostRows(),")),
        ("Look at skips the page's Keep facing",
         swap(src, "alghostcloneactions.cpp", "studio.setLookTarget(rep, target, target_id, true);", "studio.setLookTarget(rep, target, target_id, false);")),
        ("Turn is not once",
         swap(src, "alghostcloneactions.cpp", "ALGhostStudio::TURN_MODE_ONCE", "ALGhostStudio::TURN_MODE_TRACK")),
        ("Stop turning leaves the turn armed",
         swap(src, "alghostcloneactions.cpp", "ALGhostStudio::TURN_MODE_OFF", "ALGhostStudio::TURN_MODE_ONCE")),
        ("a nearby person who left is still targeted",
         swap(src, "alghostclonemenu.cpp", "    if (target == ALGhostStudio::LOOK_TARGET_ACTOR && !personNearby(target_id))\n", "    if (false)\n")),
        ("a new ELookTarget value is added",
         swap(src, "alghoststudio.h", "LOOK_TARGET_POINT\n    };", "LOOK_TARGET_POINT, LOOK_TARGET_AVATAR\n    };")),
        ("the actor resolver needs cast membership",
         swap(src, "lldirectorcast.cpp", "    LLViewerObject* obj = gObjectList.findObject(actor_id);", "    LLViewerObject* obj = getMember(actor_id) ? gObjectList.findObject(actor_id) : nullptr;")),
        ("Fade items removed from the menu",
         swap(src, "menu_ghost_clone.xml", 'label="Fade out"', 'label="Fade outx"')),
        ("Fade out deletes the clone when its ramp ends",
         swap(src, "alghoststudio.cpp", "if (it->second.mFade.removing && inst->mDeleting) finished.push_back(it->first);   // a hidden-only fade ends at 0, never deletes", "if (it->second.mFade.removing) finished.push_back(it->first);")),
        ("fadeInstance deletes at zero",
         swap(src, "alghoststudio.cpp", "    if (!inst || inst->mDeleting || inst->mKind != BACKING_ENTITY_CLONE) return false;\n    ALCloneTransitions::cancelFor(id, ALCloneTransition::R_FADE); // [CloneTransition] ROLLBACK, then the fade (PLAN 2.6)\n    inst = getInstance(id);\n    if (!inst) return false;\n    static LLCachedControl<F32> fade_in", "    if (!inst || inst->mDeleting || inst->mKind != BACKING_ENTITY_CLONE) return false;\n    if (hidden) { requestRemoveInstance(id); return true; }\n    static LLCachedControl<F32> fade_in")),
        ("fadeInstance writes a setting",
         swap(src, "alghoststudio.cpp", "    const F32 target = hidden ? 0.f : 1.f;", "    gSavedSettings.setF32(\"GhostStudioFadeOut\", 0.f);\n    const F32 target = hidden ? 0.f : 1.f;")),
        ("fade state saved in a scene",
         swap(src, "lldirectorcast.cpp", "LLSD LLDirectorCast::sceneData() const\n{", "LLSD LLDirectorCast::sceneData() const\n{\n    (void)ALGhostStudio::instance().getInstances().front().mFadeAlpha;")),
        ("Fade out acts on a non-target",
         swap(src, "alghostclonemenu.cpp", "Actions::fadeRows(sRows, fade_out)", "Actions::fadeRows(ALDirectorSelect::hostRows(), fade_out)")),
        ("Fade in enabled on a shown clone",
         swap(src, "alghostclonemenu.cpp", "(fade_out ? sAgg.mFadeOutable : sAgg.mFadeInable) > 0", "true")),
        ("Fade eligibility ignores the coverage",
         swap(src, "alghostclonemenulogic.h", "return fade_out ? !hidden : hidden;", "return true;")),
        ("Fade undo does not restore the resting state",
         swap(src, "alghostcloneactions.cpp", "studio.fadeInstance(saved.mId, saved.mHidden, false)", "studio.fadeInstance(saved.mId, false, false)")),
        ("Fade records no undo step",
         swap(src, "alghostclonemenu.cpp", '        shell->history().recordCustom(shell, "clone_fade",', '        shell->history().barrier(//')),
        ("Fade looks the Director up before refusing",
         swap(src, "alghostclonemenu.cpp", "    const std::vector<Actions::FadeBefore> before = Actions::fadeRows(sRows, fade_out);\n", "    LLFloaterReg::getInstance(\"director\");\n    const std::vector<Actions::FadeBefore> before = Actions::fadeRows(sRows, fade_out);\n")),
        ("Fade writes the Crowd tab setting",
         swap(src, "alghostcloneactions.cpp", "        saved.mHidden = studio.getInstance(id)->mUserHidden;", "        saved.mHidden = studio.getInstance(id)->mUserHidden;\n        gSavedSettings.setF32(\"GhostStudioFadeOut\", 0.f);")),
        ("Look at leaves an armed Turn",
         swap(src, "alghostcloneactions.cpp", "    for (const LLUUID& rep : reps)\n    {\n        for (const LLUUID& member : studio.groupMembers(rep))\n        {\n            if (studio.getInstance(member) && !studio.isDeleting(member))\n            {\n                studio.setTurnTarget(member, ALGhostStudio::TURN_MODE_OFF, ALGhostStudio::LOOK_TARGET_CAMERA,\n                                     LLUUID::null, LLVector3d());\n            }\n        }\n    }\n", "")),
        ("Look at clears the turn after the look",
         swap(swap(src, "alghostcloneactions.cpp", "    for (const LLUUID& rep : reps)\n    {\n        for (const LLUUID& member : studio.groupMembers(rep))\n        {\n            if (studio.getInstance(member) && !studio.isDeleting(member))\n            {\n                studio.setTurnTarget(member, ALGhostStudio::TURN_MODE_OFF, ALGhostStudio::LOOK_TARGET_CAMERA,\n                                     LLUUID::null, LLVector3d());\n            }\n        }\n    }\n", ""),
              "alghostcloneactions.cpp", "        studio.faceInstance(rep);\n        any = true;", "        studio.faceInstance(rep);\n        stopTurning(rows);\n        any = true;")),
        ("Look at clears Turn only on the picked rows (a collapsed sibling keeps its Turn)",
         swap(src, "alghostcloneactions.cpp", "    for (const LLUUID& rep : reps)\n    {\n        for (const LLUUID& member : studio.groupMembers(rep))\n        {\n            if (studio.getInstance(member) && !studio.isDeleting(member))\n            {\n                studio.setTurnTarget(member, ALGhostStudio::TURN_MODE_OFF, ALGhostStudio::LOOK_TARGET_CAMERA,\n                                     LLUUID::null, LLVector3d());\n            }\n        }\n    }\n", "    stopTurning(rows);\n")),
        ("Look at clears Turn on the representative only",
         swap(src, "alghostcloneactions.cpp", "        for (const LLUUID& member : studio.groupMembers(rep))\n        {\n            if (studio.getInstance(member) && !studio.isDeleting(member))\n            {\n                studio.setTurnTarget(member,", "        for (const LLUUID& member : std::vector<LLUUID>(1, rep))\n        {\n            if (studio.getInstance(member) && !studio.isDeleting(member))\n            {\n                studio.setTurnTarget(member,")),
        # fix round 1
        ("an overlay can be faded out",
         swap(src, "alghostcloneactions.cpp", "fadeApplies(inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE, fade_out", "fadeApplies(true, fade_out")),
        ("Look at touches an extra instance (recording fixture)",
         swap(src, "alghostcloneactions.cpp", "        studio.faceInstance(rep);\n        any = true;", "        studio.faceInstance(rep);\n        studio.setLookTarget(studio.getInstances().back().mId, target, target_id, true);\n        any = true;")),
        ("Stop looking touches an extra instance",
         swap(src, "alghostcloneactions.cpp", "            studio.setLookTarget(rep, target, target_id, false);      // Keep facing off", "            studio.setLookTarget(rep, target, target_id, false);      // Keep facing off\n            studio.setLookTarget(studio.getInstances().back().mId, target, target_id, false);")),
        ("Turn to face touches an extra instance",
         swap(src, "alghostcloneactions.cpp", "            studio.setTurnTarget(id, ALGhostStudio::TURN_MODE_ONCE, target, target_id, LLVector3d());\n            any = true;", "            studio.setTurnTarget(id, ALGhostStudio::TURN_MODE_ONCE, target, target_id, LLVector3d());\n            studio.setTurnTarget(studio.getInstances().back().mId, ALGhostStudio::TURN_MODE_ONCE, target, target_id, LLVector3d());\n            any = true;")),
        ("Nearby people ignore the draw distance",
         swap(src, "alghostclonemenu.cpp", "        if (dist_vec(position, camera) > draw_distance)\n        {\n            continue;\n        }", "        if (false)\n        {\n            continue;\n        }")),
        ("Nearby people fall back to the legacy full name",
         swap(src, "alghostclonemenu.cpp", "            name = av_name.getDisplayName();\n        }\n        NearbyEntry entry;", "            name = av_name.getDisplayName();\n        }\n        if (name.empty()) { name = avatar->getFullname(); }\n        NearbyEntry entry;")),
        ("Nearby people list a name RLVa hides",
         swap(src, "alghostclonemenu.cpp", "        if (nameHiddenByRlva(avatar->getID()))\n        {\n            continue;               // RLVa hides this name: the person is omitted\n        }\n", "")),
        ("Swap source lists a name RLVa hides",
         swap(src, "alghostclonemenu.cpp", "    if (nameHiddenByRlva(id))\n    {\n        return std::string();       // RLVa hides this name: the member is left out\n    }\n", "")),
        ("Nearby people scanned once per list",
         swap(src, "alghostclonemenu.cpp", "    const std::vector<NearbyEntry> people = nearbyPeople();       // one scan per open, shared by both lists\n", "    const std::vector<NearbyEntry> people = nearbyPeople();\n    const std::vector<NearbyEntry> again = nearbyPeople();\n")),
        ("an unknown Look / turn parameter looks at the camera",
         swap(src, "alghostclonemenu.cpp", '    if (kind != "camera" && kind != "me")\n    {\n        return;                         // an unknown parameter is never a quiet "camera"\n    }\n', "")),
        ("Fade enable misreports a vanished clone",
         swap(src, "alghostclonemenu.cpp", '        if (sAgg.mLive == 0)\n        {\n            return state(ctrl, false, "GhostCloneMenuNotReady");\n        }\n        if (sAgg.mEntities == 0)', "        if (sAgg.mEntities == 0)")),
        ("Fade enabled on an overlay-only selection",
         swap(src, "alghostclonemenu.cpp", '            return state(ctrl, false, "GhostCloneMenuMirrorEntityOnly");       // an overlay is never faded\n', "")),
        ("Clones list row right-click not wired",
         swap(src, "alpanelghoststudio.cpp", "                if (ALGhostCloneMenu::openForListRow(row, screen_x, screen_y))", "                if (false)")),
        ("Clones list row right-click ignores the list selection",
         swap(src, "alghostclonemenu.cpp", "    if (std::find(host_rows.begin(), host_rows.end(), row) != host_rows.end())\n    {\n        sRows = host_rows;", "    if (false)\n    {\n        sRows = host_rows;")),
        ("a press-opened list menu disarms its spawn point",
         swap(src, "alghostclonemenu.cpp", "    if (!opened_on_press)\n    {", "    if (true)\n    {")),
        ("the State column does not mark a hidden clone",
         swap(src, "alpanelghoststudio.cpp", '        if (inst.mUserHidden)\n        {\n            return "hidden";', '        if (false)\n        {\n            return "hidden";')),
        ("Look combo rewrites a non-cast look target",
         swap(src, "alpanelghoststudio.cpp", "    if (!from_combo && inst->mLookTarget == ALGhostStudio::LOOK_TARGET_ACTOR)", "    if (false)")),
        ("Look at clears the turn before checking there is anything to look at",
         swap(src, "alghostcloneactions.cpp", "    const std::vector<LLUUID> reps = unitRepresentatives(rows);\n    if (reps.empty())\n    {\n        return false;\n    }\n", "    const std::vector<LLUUID> reps = unitRepresentatives(rows);\n")),
        # fix round 2
        ("a swap reveals a hidden clone (beginAppearance)",
         swap(src, "alghoststudio.cpp", "    if (inst->mUserHidden && !inst->mDeleting)\n    {\n        // A clone the user faded out stays hidden through a swap, a duplicate or a crowd fade-in.", "    if (false)\n    {\n        // A clone the user faded out stays hidden through a swap, a duplicate or a crowd fade-in.")),
        ("beginAppearance cancels the delete fade of a hidden clone",
         swap(src, "alghoststudio.cpp", "    if (inst->mUserHidden && !inst->mDeleting)\n    {\n        // A clone the user faded out stays hidden", "    if (inst->mUserHidden)\n    {\n        // A clone the user faded out stays hidden")),
        ("a respawned body reveals a hidden clone",
         swap(src, "alghoststudio.cpp", "if (current->mUserHidden && !current->mDeleting)\n            {\n                mFades.erase(stable_id);", "if (false)\n            {\n                mFades.erase(stable_id);")),
        ("a replaced body of a clone being deleted loses its delete fade",
         swap(src, "alghoststudio.cpp", "if (current->mUserHidden && !current->mDeleting)", "if (current->mUserHidden)")),
        ("a replaced body of a clone in a fade starts at full coverage",
         swap(src, "alghoststudio.cpp", "            else if (mFades.count(stable_id))\n            {\n                LLDirectorCast::instance().setCloneFadeAlpha(new_runtime, current->mFadeAlpha);\n            }\n", "")),
        ("a retired body keeps its orphan fade",
         swap(src, "alghoststudio.cpp", "        LLDirectorCast::instance().forgetCloneFade(old_runtime);\n", "")),
        ("a replaced body leaves the fade ramp running",
         swap(src, "alghoststudio.cpp", "                mFades.erase(stable_id);\n                current->mFadeAlpha = 0.f;", "                current->mFadeAlpha = 0.f;")),
        ("settled hide is not through isVisible",
         swap(src, "alghoststudio.h", " &&\n             !(inst.mUserHidden && !(inst.mFadeAlpha > 0.f));", ";")),
        ("a finished hide never re-gates the body",
         swap(src, "alghoststudio.cpp", "        else if (inst->mUserHidden && inst->mKind == BACKING_ENTITY_CLONE)", "        else if (false)")),
        ("fadeInstance does not refresh the instant path visibility",
         swap(src, "alghoststudio.cpp", "        if (inst->mEntityId.notNull()) LLDirectorCast::instance().setCloneFadeAlpha(inst->mEntityId, target);\n        if (LLGhostAvatar* ghost = resolveEntityClone(id))\n            set_entity_clone_visible(ghost, isVisible(*inst), entityRenderOnly(*inst));\n        if (inst->mEntityId.notNull() && !(inst->mFadeAlpha > 0.f))\n            LLDirectorCast::instance().forgetCloneFade(inst->mEntityId);      // settled hidden: no alpha-0 coverage entry kept\n        return true;",
              "        if (inst->mEntityId.notNull()) LLDirectorCast::instance().setCloneFadeAlpha(inst->mEntityId, target);\n        return true;")),
        ("Delete of an invisible clone ramps 0 to 0",
         swap(src, "alghoststudio.cpp", "if (seconds <= 0.f || all_invisible)", "if (seconds <= 0.f)")),
        ("an array of a hidden clone is built hidden",
         swap(src, "alghoststudio.cpp", "        if (copy->mUserHidden) fadeInstance(copy->mId, false, true);      // a copy that inherited a hidden prototype fades in like any new clone\n        copy->setFootGlobal(foot);", "        copy->setFootGlobal(foot);")),
        ("a crowd of a hidden clone is built hidden",
         swap(src, "alghoststudio.cpp", "        if (copy->mUserHidden) fadeInstance(copy->mId, false, true);      // a copy that inherited a hidden prototype fades in like any new clone\n    }", "    }")),
        ("array copies lose their creation fade-in",
         swap(src, "alghoststudio.cpp", "        if (copy->mUserHidden) fadeInstance(copy->mId, false, true);      // a copy that inherited a hidden prototype fades in like any new clone\n        copy->setFootGlobal(foot);", "        fadeInstance(copy->mId, false, false);\n        copy->setFootGlobal(foot);")),
        ("crowd copies lose their creation fade-in",
         swap(src, "alghoststudio.cpp", "        if (copy->mUserHidden) fadeInstance(copy->mId, false, true);      // a copy that inherited a hidden prototype fades in like any new clone\n    }", "        fadeInstance(copy->mId, false, false);\n    }")),
        ("a hidden prototype's copies pop in",
         swap(src, "alghoststudio.cpp", "        if (copy->mUserHidden) fadeInstance(copy->mId, false, true);      // a copy that inherited a hidden prototype fades in like any new clone\n    }", "        if (copy->mUserHidden) fadeInstance(copy->mId, false, false);\n    }")),
        ("the ramp shows the body before its coverage",
         swap(src, "alghoststudio.cpp", "    if (inst->mEntityId.notNull()) LLDirectorCast::instance().setCloneFadeAlpha(inst->mEntityId, inst->mFadeAlpha);   // coverage first, so a fade-in never flashes\n", "")),
        ("a settled hide keeps an alpha-0 entry (updateFades)",
         swap(src, "alghoststudio.cpp", "            if (inst->mEntityId.notNull() && !(inst->mFadeAlpha > 0.f))\n                LLDirectorCast::instance().forgetCloneFade(inst->mEntityId);\n        }\n        it = mFades.erase(it);", "        }\n        it = mFades.erase(it);")),
        ("a settled hide keeps an alpha-0 entry (instant fade)",
         swap(src, "alghoststudio.cpp", "            LLDirectorCast::instance().forgetCloneFade(inst->mEntityId);      // settled hidden: no alpha-0 coverage entry kept\n        return true;", "        return true;")),
        ("a settled hide keeps an alpha-0 entry (beginAppearance)",
         swap(src, "alghoststudio.cpp", "        if (inst->mEntityId.notNull()) LLDirectorCast::instance().forgetCloneFade(inst->mEntityId);      // settled hidden: no alpha-0 coverage entry kept\n        return;", "        return;")),
        ("a settled hide keeps an alpha-0 entry (replaced body)",
         swap(src, "alghoststudio.cpp", "                LLDirectorCast::instance().forgetCloneFade(new_runtime);      // settled hidden: no alpha-0 coverage entry kept\n", "")),
        ("the replacement hook acts on a removed instance",
         swap(src, "alghoststudio.cpp", "        if (!removing_instance && old_runtime != new_runtime)\n        if (Instance* current", "        if (Instance* current")),
        ("the replacement hook acts on an unchanged runtime",
         swap(src, "alghoststudio.cpp", "        if (!removing_instance && old_runtime != new_runtime)\n        if (Instance* current", "        if (!removing_instance)\n        if (Instance* current")),
        ("a clone that is not a Cast member gets no ramp (renderer getter)",
         swap(src, "lldirectorcast.cpp", "        return orphan->second;      // a clone that is not a Cast member: Studio's own fade\n", "        return default_style;\n")),
        ("a clone that is not a Cast member gets no ramp (resolved style)",
         swap(src, "lldirectorcast.cpp", "            resolved_id = primary;\n            return orphan->second;", "            resolved_id = primary;\n            return mSelfActorStyle;")),
        ("orphan fades are scanned in the setter",
         swap(src, "lldirectorcast.cpp", "        if (alpha >= 1.f || !LLGhostAvatar::isGhostId(id)) { mCloneFadeOrphans.erase(id); return; }\n        ActorStyle& orphan", "        for (auto it = mCloneFadeOrphans.begin(); it != mCloneFadeOrphans.end();)\n            it = gObjectList.findObject(it->first) ? std::next(it) : mCloneFadeOrphans.erase(it);\n        if (alpha >= 1.f || !LLGhostAvatar::isGhostId(id)) { mCloneFadeOrphans.erase(id); return; }\n        ActorStyle& orphan")),
        ("orphan fades are never retired with their body",
         swap(src, "lldirectorcast.cpp", "void LLDirectorCast::forgetCloneFade(const LLUUID& runtime_id)\n{\n    mCloneFadeOrphans.erase(runtime_id);\n}", "void LLDirectorCast::forgetCloneFade(const LLUUID& runtime_id)\n{\n    (void)runtime_id;\n}")),
        ("a real avatar is given an orphan fade",
         swap(src, "lldirectorcast.cpp", "if (alpha >= 1.f || !LLGhostAvatar::isGhostId(id)) { mCloneFadeOrphans.erase(id); return; }", "if (alpha >= 1.f) { mCloneFadeOrphans.erase(id); return; }")),
        ("an orphan fade completes only above 1",
         swap(src, "lldirectorcast.cpp", "if (alpha >= 1.f || !LLGhostAvatar::isGhostId(id)) { mCloneFadeOrphans.erase(id); return; }", "if (alpha > 1.f || !LLGhostAvatar::isGhostId(id)) { mCloneFadeOrphans.erase(id); return; }")),
        ("a worn animesh of a non-member clone does not fade",
         swap(src, "lldirectorcast.cpp", "            const auto orphan = mCloneFadeOrphans.find(fallback);\n            if (orphan != mCloneFadeOrphans.end())", "            const auto orphan = mCloneFadeOrphans.end();\n            if (orphan != mCloneFadeOrphans.end())")),
        ("a Cast member primary returns the fallback orphan's style",
         swap(src, "lldirectorcast.cpp", "        const auto& member = mCast[primary_it->second];\n        return member.mCloneFading ? member.mCloneFadeStyle : member.mActorStyle;\n    }\n    if (!mCloneFadeOrphans.empty())\n    {\n        const auto orphan = mCloneFadeOrphans.find(primary);",
              "        const auto& member = mCast[primary_it->second];\n        if (!mCloneFadeOrphans.empty() && fallback.notNull())\n        {\n            const auto fallback_orphan = mCloneFadeOrphans.find(fallback);\n            if (fallback_orphan != mCloneFadeOrphans.end()) return fallback_orphan->second;\n        }\n        return member.mCloneFading ? member.mCloneFadeStyle : member.mActorStyle;\n    }\n    if (!mCloneFadeOrphans.empty())\n    {\n        const auto orphan = mCloneFadeOrphans.find(primary);")),
        ("the orphan map is read without an empty fast path",
         swap(src, "lldirectorcast.cpp", "    if (!mCloneFadeOrphans.empty())\n    {\n        const auto orphan = mCloneFadeOrphans.find(id);", "    {\n        const auto orphan = mCloneFadeOrphans.find(id);")),
        ("a re-enrolled clone keeps a stale orphan fade",
         swap(src, "lldirectorcast.cpp", "    mCloneFadeOrphans.erase(id);        // a (re-)enrolled clone: the member carries its fade\n", "")),
        ("an orphan fade is saved in the scene",
         swap(src, "lldirectorcast.cpp", "LLSD LLDirectorCast::sceneData() const\n{", "LLSD LLDirectorCast::sceneData() const\n{\n    (void)mCloneFadeOrphans.size();")),
        ("a selected group row no longer owns its members",
         swap(src, "alghostcloneactions.cpp", " && headers.find(group->mId) == headers.end())\n        {\n            return true;", ")\n        {\n            return true;")),
        ("the half-fetched inventory message is lost",
         swap(src, "alghostclonemenu.cpp", 'tip(str("GhostCloneMenuInventoryLoading"));', 'tip(str("GhostCloneMenuNotReady"));')),
        ("the Look combo accumulates menu target rows",
         swap(src, "alpanelghoststudio.cpp", 'mLookTargetCombo->remove("(menu target)");', 'static_cast<void>(0);')),
        ("expanded-member Look at is allowed",
         swap(src, "alghostcloneactions.cpp", "    if (hasExpandedMember(rows))\n    {\n        return false;\n    }\n    const std::vector<LLUUID> reps = unitRepresentatives(rows);", "    const std::vector<LLUUID> reps = unitRepresentatives(rows);")),
        ("expanded-member Stop looking is allowed",
         swap(src, "alghostcloneactions.cpp", "    if (hasExpandedMember(rows))\n    {\n        return;\n    }\n    for (const LLUUID& rep : unitRepresentatives(rows))", "    for (const LLUUID& rep : unitRepresentatives(rows))")),
        ("expanded-member Turn to face is allowed",
         swap(src, "alghostcloneactions.cpp", "    if (hasExpandedMember(rows))\n    {\n        return false;\n    }\n    for (const LLUUID& id : instances(rows))", "    for (const LLUUID& id : instances(rows))")),
        ("expanded-member Stop turning is allowed",
         swap(src, "alghostcloneactions.cpp", "    if (hasExpandedMember(rows))\n    {\n        return;\n    }\n    for (const LLUUID& id : instances(rows))", "    for (const LLUUID& id : instances(rows))")),
        ("only some expanded members are refused",
         swap(src, "alghostcloneactions.cpp", "headers.find(group->mId) == headers.end())\n        {\n            return true;", "headers.find(group->mId) == headers.end())\n        {\n            continue;")),
        ("Look / turn items stay enabled on an expanded member",
         swap(src, "alghostclonemenu.cpp", "        if (sAgg.mLive > 0 && sAgg.mExpandedMember)", "        if (false)")),
        ("Look / turn commit without the expanded-member refusal",
         swap(src, "alghostclonemenu.cpp", "    if (Actions::hasExpandedMember(sRows))\n    {\n        tip(str(\"GhostCloneMenuWholeGroup\"));\n        return;\n    }\n    if (target == ALGhostStudio::LOOK_TARGET_ACTOR", "    if (target == ALGhostStudio::LOOK_TARGET_ACTOR")),
        ("a Look / turn item uses the plain target enable",
         swap(src, "menu_ghost_clone.xml", 'function="GhostClone.EnableFace"', 'function="GhostClone.EnableTargets"')),
        ("a Nearby person item ignores the expanded-member refusal",
         swap(src, "alghostclonemenu.cpp", "return sAgg.mLive > 0 && !sAgg.mExpandedMember; }", "return sAgg.mLive > 0; }")),
        ("Studio setLookTarget changed from the base",
         swap(src, "alghoststudio.cpp", "    for (const LLUUID& member_id : groupMembers(id))\n    {\n        if (Instance* member = getInstance(member_id))\n        {\n            member->mLookTarget = target;", "    for (const LLUUID& member_id : std::vector<LLUUID>(1, id))\n    {\n        if (Instance* member = getInstance(member_id))\n        {\n            member->mLookTarget = target;")),
        ("a member-edit special case returns to the studio",
         swap(src, "alghoststudio.h", "    bool fadeInstance(const LLUUID& id, bool hidden, bool ramp);\n", "    bool fadeInstance(const LLUUID& id, bool hidden, bool ramp);\n    bool isExpandedMember(const LLUUID& id) const;\n")),
        ("Recent replay skips the inventory matcher",
         swap(src, "alghostclonemenu.cpp", "    if (items.empty())\n    {\n        if (LLInventoryModelBackgroundFetch", "    if (false)\n    {\n        if (LLInventoryModelBackgroundFetch")),
        ("Recent drops an entry while the inventory is still fetching",
         swap(src, "alghostclonemenu.cpp", "        if (LLInventoryModelBackgroundFetch::instance().isEverythingFetched())", "        if (true)")),
        ("Recent validation rebuilds the whole inventory index",
         swap(src, "alghostclonemenu.cpp", "    if (items.empty())\n    {\n        if (LLInventoryModelBackgroundFetch", "    ALGhostAnimAssetIndex::instance().refreshInventory();\n    if (items.empty())\n    {\n        if (LLInventoryModelBackgroundFetch")),
        ("list row selection changes before validation",
         swap(src, "alpanelghoststudio.cpp", "            if (ALGhostCloneMenu::canOpenForListRow(row))", "            if (true)")),
        ("list row right-click ignores a disabled list",
         swap(src, "alpanelghoststudio.cpp", "mList->getEnabled() && ", "")),
        ("Swap source label differs from the page's",
         swap(src, "alghostclonemenu.cpp", "ALPanelGhostStudio::sourceName(source))", "label)")),
        ("the Look combo has no neutral menu target row",
         swap(src, "alpanelghoststudio.cpp", 'mLookTargetCombo->add("(menu target)"', 'mLookTargetCombo->add("x"')),
        ("Turn to face clears the look",
         swap(src, "alghostcloneactions.cpp", "bool turnTo(const std::vector<LLUUID>& rows, ALGhostStudio::ELookTarget target, const LLUUID& target_id)\n{", "bool turnTo(const std::vector<LLUUID>& rows, ALGhostStudio::ELookTarget target, const LLUUID& target_id)\n{\n    stopTurning(rows);")),
        ("Appearance > Look item with a wrong value",
         swap(src, "menu_ghost_clone.xml", '<menu_item_check.on_click function="GhostClone.EntityLook" parameter="3" />', '<menu_item_check.on_click function="GhostClone.EntityLook" parameter="2" />')),
        ("Appearance > Look label differs from the page",
         swap(src, "menu_ghost_clone.xml", 'label="Chrome*"', 'label="Chrome"')),
        ("Look / turn moved",
         swap(src, "menu_ghost_clone.xml", 'label="Animation"', 'label="Animationx"')),
        ("Look at items call the wrong function",
         swap(src, "menu_ghost_clone.xml", '<menu_item_call.on_click function="GhostClone.Look" parameter="me" />', '<menu_item_call.on_click function="GhostClone.Turn" parameter="me" />')),
        ("Swap source lists a cast member by id",
         swap(src, "alghostclonemenu.cpp", "return Logic::containsUuidText(name) ? std::string() : name;", "return name;")),
        ("Swap source lists Studio clones",
         swap(src, "alghostclonemenu.cpp", "        if (!self && LLGhostAvatar::isGhostId(choice.mId))\n", "        if (false)\n")),
        ("the page Swap source handler no longer uses the shared action",
         swap(src, "alpanelghoststudio.cpp", "ALGhostCloneActions::swapSource(selectedRowValues(), source, label)", "std::string()")),
        ("the page Render look handler no longer uses the shared action",
         swap(src, "alpanelghoststudio.cpp", "ALGhostCloneActions::setEntityLook(selectedRowValues(), look);", "")),
        ("the lists are built after the menu shows",
         swap(src, "alghostclonemenu.cpp", "    populateDynamicMenus();\n    sMenu->show(x, y);", "    sMenu->show(x, y);\n    populateDynamicMenus();")),
        ("Recent replay bypasses the page's Directed rule",
         swap(src, "alghostclonemenu.cpp", "ALPanelGhostStudio::applyInventoryAnimationTo(selected, asset, name, /*director_fallback=*/true);",
              "ALPanelGhostStudio::playAnimationOnClonesRecorded(liveEntityTargets(), asset, name, true);")),
        ("Flip menu path writes the studio directly, not the shared helper",
         swap(src, "alghostclonemenu.cpp", "const bool changed = Actions::setDirectedMirror(sRows, !all_on);",
              "for (const LLUUID& id : sRows) ALGhostStudio::instance().setInstanceDirectedMirror(id, !all_on); const bool changed = true;")),
        ("Flip page commit writes the studio directly, not the shared helper",
         swap(src, "alpanelghoststudio.cpp", "ALGhostCloneActions::setDirectedMirror(selectedInstances(), enabled);",
              "for (const LLUUID& id : selectedInstances()) ALGhostStudio::instance().setInstanceDirectedMirror(id, enabled);")),
        ("Flip eligibility accepts overlay clones",
         swap(src, "alghostcloneactions.cpp", "inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&\n        ALGhostAnimMirrorMath::directedMirrorActive",
              "ALGhostAnimMirrorMath::directedMirrorActive")),
        ("Flip item enabled for overlay-only targets",
         swap(src, "alghostclonemenu.cpp", "        if (sAgg.mEntities == 0)\n        {\n            return state(ctrl, false, \"GhostCloneMenuMirrorEntityOnly\");     // an overlay has no pose to flip",
              "        if (false)\n        {\n            return state(ctrl, false, \"GhostCloneMenuMirrorEntityOnly\");     // an overlay has no pose to flip")),
        ("Flip writes ineligible clones",
         swap(src, "alghostcloneactions.cpp", "        if (!directedMirrorEligible(id))\n        {\n            continue;\n        }\n        changed = changed",
              "        changed = changed")),
        ("Flip eligibility drops the Director row's drive rule",
         swap(src, "alghostcloneactions.cpp", "ALGhostAnimMirrorMath::directedMirrorActive(inst->mDriveMode, inst->mDirectedMirrorFreezeDrive, true)", "true")),
        ("Flip no-op click still records a history step",
         swap(src, "alghostclonemenu.cpp", "    if (changed)\n    {\n        LLFloater* director = LLFloaterReg::getInstance(\"director\");\n        ALKitShell* shell = director ? director->findChild<ALKitShell>(\"director_shell\") : nullptr;\n        if (shell)\n        {\n            struct History\n            {\n                std::vector<Actions::FlipBefore>",
              "    if (true)\n    {\n        LLFloater* director = LLFloaterReg::getInstance(\"director\");\n        ALKitShell* shell = director ? director->findChild<ALKitShell>(\"director_shell\") : nullptr;\n        if (shell)\n        {\n            struct History\n            {\n                std::vector<Actions::FlipBefore>")),
        ("Flip preflight accepts an ineligible-only step (instance exists but replay would skip it)",
         swap(src, "alghostcloneactions.cpp", "        if (directedMirrorEligible(saved.mId))\n        {\n            return true;\n        }",
              "        if (ALGhostStudio::instance().getInstance(saved.mId))\n        {\n            return true;\n        }")),
        ("Flip preflight accepts a step whose eligible targets were deleted",
         swap(src, "alghostcloneactions.cpp", "bool directedMirrorReplayable(const std::vector<FlipBefore>& state)\n{\n",
              "bool directedMirrorReplayable(const std::vector<FlipBefore>& state)\n{\n    return true;\n")),
        ("Flip history has no preflight validator",
         swap(src, "alghostclonemenu.cpp", "Actions::directedMirrorReplayable(redo ? history->mAfter : history->mBefore)", "true")),
        ("Flip checked when only some eligible clones are flipped",
         swap(src, "alghostclonemenu.cpp", "sAgg.mFlipOn == sAgg.mFlipEligible;", "sAgg.mFlipOn > 0;")),
        ("Flip item callbacks mis-wired",
         swap(src, "menu_ghost_clone.xml", '<menu_item_check.on_click function="GhostClone.FlipLR" />', '<menu_item_check.on_click function="GhostClone.RestartAnim" />')),
        ("Flip item moved before Restart animation",
         swap(src, "menu_ghost_clone.xml", 'label="Restart animation"', 'label="Restart animationx"')),
        ("Look is applied while a look outside the range is allowed",
         swap(src, "alghostclonemenu.cpp", "look < ALGhostStudio::LOOK_NORMAL || look > ALGhostStudio::LOOK_SILHOUETTE ||\n        ", "")),
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
