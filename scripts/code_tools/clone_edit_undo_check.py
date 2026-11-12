#!/usr/bin/env python3
"""Native fixture for clone Edit-box undo/redo (move / rotate / scale).

Extracts the production block ([CloneEditUndo-BEGIN]..END) from
indra/newview/alghostmanipproxy.cpp, compiles it unchanged against small doubles
for ALGhostStudio / ALKitHistory / the edit host, and EXECUTES it: the drag
snapshot (beginDrag), commit vs cancel (endDrag), recording, replay, and the
history begin/end hooks (capture release + drag cancel + proxy resync). The
production beginDrag/endDrag/historyBegin/historyEnd are thin callers of that
block. Planted negatives (mutated copies of the block) must make it fail.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_check_tools import ROOT, check_directory, compile_run  # noqa: E402

NV = ROOT / "indra" / "newview"
BEGIN = "// [CloneEditUndo-BEGIN]"
END = "// [CloneEditUndo-END]"

DOUBLES = r'''
#include <windows.h>
#include <stdlib.h>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>
typedef int S32;
typedef float F32;
template <class T> T llmax(T a, T b) { return a > b ? a : b; }
struct LLUUID {
    int v = 0;
    LLUUID() = default;
    LLUUID(int x) : v(x) {}
    bool isNull() const { return v == 0; }
    bool notNull() const { return v != 0; }
    bool operator==(const LLUUID& o) const { return v == o.v; }
    bool operator!=(const LLUUID& o) const { return v != o.v; }
};
struct LLVector3d { double mdV[3] = {0, 0, 0}; };
struct LLQuaternion { F32 mQ[4] = {0, 0, 0, 1}; };
static double dist_vec(const LLVector3d& a, const LLVector3d& b)
{
    double s = 0;
    for (int i = 0; i < 3; ++i) s += (a.mdV[i] - b.mdV[i]) * (a.mdV[i] - b.mdV[i]);
    return std::sqrt(s);
}
struct Xf { LLVector3d f; LLQuaternion r; F32 s = 1.f; };
struct ALGhostManipProxy { enum EUndoRoute { ROUTE_STOCK, ROUTE_UNDO, ROUTE_REDO }; };
struct LLGhostAvatar {};
constexpr F32 GHOST_SCALE_MIN = 0.05f;
constexpr F32 GHOST_SCALE_MAX = 150.f;
namespace ALGhostGroupModel {
struct Member { LLUUID mInstanceId; bool mPinned = false; Xf authored; };
struct Group {
    LLUUID mId; std::string mName; std::vector<Member> mMembers;
    bool mEditMembers = false; unsigned long long mRevision = 1;
};
}
struct ALKitHistory {
    struct Step { std::string label; std::function<bool(bool, std::string&)> fn, validate; };
    std::vector<Step> undoStack, redoStack;
    std::string lastReason;
    template <class T>
    void recordCustom(T*, const std::string&, const std::string& label,
                      std::function<bool(bool, std::string&)> fn, bool join = false,
                      std::function<bool(bool, std::string&)> validate = {})
    { (void)join; undoStack.push_back({label, fn, validate}); redoStack.clear(); }
    // Mirrors ALKitHistory::undo/redo: a failed validator discards the step;
    // a failed replay keeps it.
    bool run(bool redo)
    {
        auto& from = redo ? redoStack : undoStack;
        auto& to = redo ? undoStack : redoStack;
        lastReason.clear();
        if (from.empty()) return false;
        Step s = from.back();
        if (s.validate && !s.validate(redo, lastReason)) { from.pop_back(); return false; }
        if (!s.fn(redo, lastReason)) return false;
        from.pop_back();
        to.push_back(s);
        return true;
    }
    bool undo() { return run(false); }
    bool redo() { return run(true); }
};
struct ALKitShell { ALKitHistory h; ALKitHistory& history() { return h; } };

struct ALGhostStudio {
    struct Instance {
        LLUUID mId; LLVector3d mFootGlobal; LLQuaternion mRotation; F32 mScale = 1.f;
        int mRuntime = 0;
        LLUUID mGroupId; int mMotion = 0; bool mMotionHasBase = false;
        bool mChaosEnabled = false, mChaosHasBase = false;
        int mKind = 0; bool mRuntimeLive = true;
        Xf get() const { Xf x; x.f = mFootGlobal; x.r = mRotation; x.s = mScale; return x; }
        void set(const Xf& x) { mFootGlobal = x.f; mRotation = x.r; mScale = x.s; }
    };
    enum { MOTION_OFF = 0, MOTION_ORBIT };
    enum { BACKING_OVERLAY = 0, BACKING_ENTITY_CLONE };
    std::map<int, Instance> inst;
    std::vector<ALGhostGroupModel::Group> groups;
    std::map<int, Xf> groupX;
    std::vector<int> reflected;
    void invalidateReflectPlane(const LLUUID& id) { reflected.push_back(id.v); }
    static ALGhostStudio& instance() { static ALGhostStudio s; return s; }
    void reset() { inst.clear(); groups.clear(); groupX.clear(); reflected.clear(); }
    void add(int id, const Xf& x) { Instance i; i.mId = id; i.set(x); inst[id] = i; }
    Instance* getInstance(const LLUUID& id)
    {
        auto it = inst.find(id.v);
        if (it == inst.end()) return nullptr;
        it->second.mGroupId = LLUUID();                    // membership is derived from the groups
        for (auto& g : groups) for (auto& m : g.mMembers) if (m.mInstanceId == id) it->second.mGroupId = g.mId;
        return &it->second;
    }
    LLGhostAvatar* resolveEntityClone(const LLUUID& id)
    {
        static LLGhostAvatar live;
        Instance* i = getInstance(id);
        return i && i->mRuntimeLive ? &live : nullptr;
    }
    ALGhostGroupModel::Group* groupForMember(const LLUUID& id)
    {
        for (auto& g : groups) {
            if (g.mId == id) return &g;
            for (auto& m : g.mMembers) if (m.mInstanceId == id) return &g;
        }
        return nullptr;
    }
    std::string cloneDisplayName(const LLUUID& id) const { return "Clone" + std::to_string(id.v); }
    bool getUnitTransform(const LLUUID& id, LLVector3d& f, LLQuaternion& r, F32& s)
    {
        ALGhostGroupModel::Group* g = groupForMember(id);
        if (g && (g->mId == id || !g->mEditMembers)) { Xf& x = groupX[g->mId.v]; f = x.f; r = x.r; s = x.s; return true; }
        Instance* i = getInstance(id);
        if (!i) return false;
        f = i->mFootGlobal; r = i->mRotation; s = i->mScale; return true;
    }
    // Members keep a LOCAL scale; their world scale is local * group factor.
    bool getGroupScaleLimits(const LLUUID& id, F32& mn, F32& mx)
    {
        ALGhostGroupModel::Group* g = groupForMember(id);
        if (!g) return false;
        const F32 f = groupX[g->mId.v].s;
        mn = 0.f; mx = 1.0e30f;
        for (auto& m : g->mMembers) if (Instance* i = getInstance(m.mInstanceId)) {
            const F32 local = i->mScale / f;
            mn = llmax(mn, GHOST_SCALE_MIN / local);
            if (GHOST_SCALE_MAX / local < mx) mx = GHOST_SCALE_MAX / local;
        }
        return mn <= mx;
    }
    bool transformGroup(const LLUUID& id, const LLVector3d& f, const LLQuaternion& r, F32 s)
    {
        ALGhostGroupModel::Group* g = groupForMember(id);
        if (!g || (g->mId != id && g->mEditMembers)) return false;
        F32 mn, mx;
        if (!getGroupScaleLimits(id, mn, mx) || s < mn || s > mx) return false;
        const F32 old = groupX[g->mId.v].s;
        for (auto& m : g->mMembers) if (Instance* i = getInstance(m.mInstanceId)) i->mScale = i->mScale / old * s;
        groupX[g->mId.v] = Xf{f, r, s}; return true;
    }
    bool transformUnit(const LLUUID& id, const LLVector3d& f, const LLQuaternion& r, F32 s)
    {
        Instance* i = getInstance(id);
        if (!i) return false;
        ALGhostGroupModel::Group* g = groupForMember(id);
        if (!g) { i->set(Xf{f, r, s}); return true; }
        if (!g->mEditMembers) { groupX[g->mId.v] = Xf{f, r, s}; return true; }  // follows CURRENT mode
        i->set(Xf{f, r, s});
        for (auto& m : g->mMembers) if (m.mInstanceId == id) m.mPinned = true;
        return true;
    }
    // Production semantics: unpinning RESETS the member to its authored slot.
    bool setGroupMemberPinned(const LLUUID& id, bool p)
    {
        ALGhostGroupModel::Group* g = groupForMember(id);
        if (!g) return false;
        for (auto& m : g->mMembers) if (m.mInstanceId == id) {
            if (!p) { if (Instance* i = getInstance(id)) i->set(m.authored); }
            m.mPinned = p;
        }
        return true;
    }
};

// Doubles for the proxy side: live-drag state a tick would re-apply.
static bool g_editing = true, g_dragging = false, g_capture = false, g_hasProxy = true;
static int g_released = 0, g_pushed = 0, g_synced = 0;
static Xf g_liveXform;
static LLUUID g_dragId;
namespace {
void cloneEditHistoryBegin();
void cloneEditHistoryEnd();
'''

BLOCK_TAIL = r'''
DragSnapshot g_snap;
struct FixtureHost : EditHost {
    bool editing() override { return g_editing; }
    void releaseManip() override { ++g_released; g_capture = false; }
    bool dragging() override { return g_dragging; }
    void cancelDrag() override
    {
        g_dragging = false;
        dragFinish(g_snap, ALGhostStudio::instance(), nullptr, g_dragId, false);
    }
    bool hasProxy() override { return g_hasProxy; }
    void pushProxy() override { ++g_pushed; }
    void syncRevision() override { ++g_synced; }
};
FixtureHost g_host;
void cloneEditHistoryBegin() { historyBeginImpl(g_host); }
void cloneEditHistoryEnd() { historyEndImpl(g_host); }
}  // namespace
'''

TEST_MAIN = r'''
static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)
static Xf mk(double x, double y, double z, F32 qz, F32 qw, F32 s)
{ Xf v; v.f.mdV[0] = x; v.f.mdV[1] = y; v.f.mdV[2] = z; v.r.mQ[2] = qz; v.r.mQ[3] = qw; v.s = s; return v; }
static UnitXform ux(const Xf& a) { UnitXform u; u.mFoot = a.f; u.mRotation = a.r; u.mScale = a.s; return u; }
static bool same(const Xf& a, const Xf& b) { return !unitXformChanged(ux(a), ux(b)); }
static Xf cur(ALGhostStudio& st, int id)
{ Xf x; st.getUnitTransform(LLUUID(id), x.f, x.r, x.s); return x; }
static bool has(const std::string& s, const char* t) { return s.find(t) != std::string::npos; }
static void resetHooks()
{ g_editing = true; g_dragging = false; g_capture = false; g_hasProxy = true; g_released = g_pushed = g_synced = 0; }

// One full drag through the production snapshot/finish code: begin, live edit, end.
static bool drag(ALGhostStudio& st, ALKitShell* shell, int unit, S32 kind, const Xf& to, bool commit,
                 bool whole = false)
{
    // Same forwarding the production wrappers do: members in, members out.
    LLVector3d f; LLQuaternion r; F32 s = 99.f; S32 k = -1; bool pk = true, pp = true;
    dragBeginInto(st, unit, kind, f, r, s, k, pk, pp);
    CHECK(k == kind);
    if (whole) st.transformGroup(unit, to.f, to.r, to.s);
    else st.transformUnit(unit, to.f, to.r, to.s);
    return dragFinishFrom(f, r, s, k, pk, pp, st, shell, unit, commit);
}

static void commitIndividual(S32 kind, const char* verb)
{
    ALGhostStudio& st = ALGhostStudio::instance(); st.reset(); resetHooks();
    ALKitShell shell;
    st.add(1, mk(0, 0, 0, 0, 1, 1.5f));
    const Xf before = cur(st, 1), after = mk(1, 2, 3, 0.5f, 0.866f, 2.5f);
    CHECK(drag(st, &shell, 1, kind, after, true));
    CHECK(shell.h.undoStack.size() == 1);
    CHECK(has(shell.h.undoStack[0].label, verb) && has(shell.h.undoStack[0].label, "Clone1"));
    CHECK(shell.h.undo()); CHECK(same(cur(st, 1), before));
    CHECK(shell.h.redo()); CHECK(same(cur(st, 1), after));
    CHECK(shell.h.undo()); CHECK(same(cur(st, 1), before));
}

static void commitGroup(S32 kind, const char* verb)
{
    ALGhostStudio& st = ALGhostStudio::instance(); st.reset(); resetHooks();
    ALKitShell shell;
    st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(1, 0, 0, 0, 1, 1));
    ALGhostGroupModel::Group g; g.mId = 100; g.mName = "Crew";
    g.mMembers.resize(2); g.mMembers[0].mInstanceId = 1; g.mMembers[1].mInstanceId = 2;
    st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1.25f);
    const Xf before = cur(st, 1), after = mk(9, 9, 0, 0.5f, 0.866f, 2.f);
    CHECK(drag(st, &shell, 1, kind, after, true, true));   // selected through a MEMBER id
    CHECK(has(shell.h.undoStack[0].label, verb) && has(shell.h.undoStack[0].label, "Crew"));
    CHECK(shell.h.undo()); CHECK(same(cur(st, 100), before));
    CHECK(shell.h.redo()); CHECK(same(cur(st, 100), after));
    st.groups[0].mEditMembers = true;                      // editing flips on: rejected, no mutation
    const Xf snap = cur(st, 100);
    CHECK(!shell.h.undo());
    st.groups[0].mEditMembers = false;
    CHECK(same(cur(st, 100), snap)); CHECK(shell.h.undoStack.empty());
}

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    ALGhostStudio& st = ALGhostStudio::instance();

    // 1. individual and whole-group move / rotate / scale, each separately
    commitIndividual(0, "Move "); commitIndividual(1, "Rotate "); commitIndividual(2, "Scale ");
    commitGroup(0, "Move "); commitGroup(1, "Rotate "); commitGroup(2, "Scale ");

    // 2. unchanged drag and null shell (Director absent) record nothing, no crash
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1));
        CHECK(!drag(st, &shell, 1, 0, mk(0, 0, 0, 0, 1, 1), true));
        CHECK(shell.h.undoStack.empty());
        CHECK(!drag(st, nullptr, 1, 0, mk(5, 0, 0, 0, 1, 1), true));
    }

    // 3. cancelled drag restores the BEFORE snapshot and records nothing
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1.5f));
        const Xf before = cur(st, 1);
        CHECK(!drag(st, &shell, 1, 0, mk(3, 3, 3, 0.5f, 0.866f, 2.f), false));
        CHECK(shell.h.undoStack.empty()); CHECK(same(cur(st, 1), before));
    }

    // 4. regrouped under a new id: old whole-group step rejected
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1));
        ALGhostGroupModel::Group g; g.mId = 100; g.mMembers.resize(1); g.mMembers[0].mInstanceId = 1;
        st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1);
        CHECK(drag(st, &shell, 1, 0, mk(4, 0, 0, 0, 1, 1), true, true));
        st.groups.clear(); g.mId = 200; st.groups.push_back(g); st.groupX[200] = mk(7, 7, 7, 0, 1, 1);
        CHECK(!shell.h.undo()); CHECK(same(cur(st, 200), mk(7, 7, 7, 0, 1, 1)));
    }

    // 5. member-only replay; an unpinned member AIMED away from its authored slot
    {
        st.reset(); resetHooks(); ALKitShell shell;
        const Xf authored = mk(0, 0, 0, 0, 1, 1), aimed = mk(0, 0, 0, 0.7071f, 0.7071f, 1);
        st.add(1, aimed); st.add(2, mk(1, 0, 0, 0, 1, 1));
        ALGhostGroupModel::Group g; g.mId = 100; g.mEditMembers = true; g.mMembers.resize(2);
        g.mMembers[0].mInstanceId = 1; g.mMembers[0].authored = authored;   // unpinned, aimed != authored
        g.mMembers[1].mInstanceId = 2; g.mMembers[1].authored = mk(1, 0, 0, 0, 1, 1);
        st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1);
        const Xf after = mk(3, 1, 0, 0, 1, 1);
        CHECK(drag(st, &shell, 1, 0, after, true));
        CHECK(st.groups[0].mMembers[0].mPinned);
        CHECK(shell.h.undo());
        CHECK(same(cur(st, 1), aimed));                      // aimed facing survives the unpin
        CHECK(!st.groups[0].mMembers[0].mPinned);
        CHECK(same(cur(st, 100), mk(0, 0, 0, 0, 1, 1)));
        CHECK(shell.h.redo()); CHECK(same(cur(st, 1), after)); CHECK(st.groups[0].mMembers[0].mPinned);
        // a cancelled drag is just as neutral for an aimed unpinned member
        CHECK(shell.h.undo());
        const unsigned long long rev = st.groups[0].mRevision;
        CHECK(!drag(st, &shell, 1, 0, mk(8, 8, 0, 0, 1, 1), false));
        CHECK(same(cur(st, 1), aimed)); CHECK(!st.groups[0].mMembers[0].mPinned);
        CHECK(st.groups[0].mRevision > rev);
        // editing turned off: transformUnit would move the whole group -> must reject
        CHECK(shell.h.redo());
        st.groups[0].mEditMembers = false;
        const Xf gsnap = cur(st, 100);
        CHECK(!shell.h.undo()); CHECK(same(cur(st, 100), gsnap)); CHECK(has(shell.h.lastReason, "no longer"));
    }

    // 6. ungrouped clone later joins a group: rejected, not applied to the group
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1));
        CHECK(drag(st, &shell, 1, 0, mk(2, 0, 0, 0, 1, 1), true));
        ALGhostGroupModel::Group g; g.mId = 100; g.mMembers.resize(1); g.mMembers[0].mInstanceId = 1;
        st.groups.push_back(g); st.groupX[100] = mk(5, 5, 5, 0, 1, 1);
        CHECK(!shell.h.undo()); CHECK(same(cur(st, 100), mk(5, 5, 5, 0, 1, 1)));
    }

    // 7. deleted clone is discarded and does not block an older step
    {
        st.reset(); resetHooks(); ALKitShell shell;
        st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(0, 0, 0, 0, 1, 1));
        const Xf a0 = cur(st, 1);
        CHECK(drag(st, &shell, 1, 0, mk(1, 0, 0, 0, 1, 1), true));
        CHECK(drag(st, &shell, 2, 0, mk(2, 0, 0, 0, 1, 1), true));
        st.inst.erase(2);
        CHECK(!shell.h.undo());
        CHECK(has(shell.h.lastReason, "That clone is no longer available"));
        CHECK(shell.h.undoStack.size() == 1);
        CHECK(shell.h.undo()); CHECK(same(cur(st, 1), a0));
    }

    // 8. runtime rekey: same instance id, new entity runtime -> step still replays
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1.5f));
        const Xf before = cur(st, 1), after = mk(6, 0, 0, 0, 1, 1.5f);
        CHECK(drag(st, &shell, 1, 0, after, true));
        st.add(1, after); st.inst[1].mRuntime = 2;           // respawned runtime, same id
        CHECK(shell.h.undo()); CHECK(same(cur(st, 1), before));
        CHECK(shell.h.redo()); CHECK(same(cur(st, 1), after));
    }

    // 9. undo while a drag is live: mDragging path cancels the drag first, restores
    //    its snapshot, replays, then resyncs the proxy.
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(5, 0, 0, 0, 1, 1));
        const Xf a0 = cur(st, 1), b0 = cur(st, 2);
        CHECK(drag(st, &shell, 1, 0, mk(1, 0, 0, 0, 1, 1), true));
        CHECK(dragBegin(g_snap, st, 2, 0));                  // drag B starts
        g_dragging = true; g_capture = true; g_dragId = 2; g_liveXform = mk(8, 8, 8, 0, 1, 1);
        st.transformUnit(2, g_liveXform.f, g_liveXform.r, g_liveXform.s);
        CHECK(shell.h.undo());
        if (g_capture) st.transformUnit(2, g_liveXform.f, g_liveXform.r, g_liveXform.s);   // next tick
        CHECK(!g_dragging && !g_capture && g_released == 1);
        CHECK(same(cur(st, 2), b0));                         // B restored, not left half-dragged
        CHECK(same(cur(st, 1), a0));                         // A undone
        CHECK(g_pushed == 1 && g_synced == 1);               // proxy re-pushed and revision resynced
        CHECK(shell.h.undoStack.empty());                    // the cancelled drag recorded nothing
    }

    // 10. capture held but mDragging not yet begun: capture is still released
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1));
        const Xf a0 = cur(st, 1);
        CHECK(drag(st, &shell, 1, 0, mk(1, 0, 0, 0, 1, 1), true));
        g_capture = true; g_dragging = false;
        CHECK(shell.h.undo());
        CHECK(!g_capture && g_released == 1); CHECK(same(cur(st, 1), a0));
    }

    // 11. replay with Edit mode off (Director buttons) or no proxy: applies, touches no proxy
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1));
        const Xf a0 = cur(st, 1);
        CHECK(drag(st, &shell, 1, 0, mk(1, 0, 0, 0, 1, 1), true));
        g_editing = false; g_capture = true;
        CHECK(shell.h.undo());
        CHECK(same(cur(st, 1), a0)); CHECK(g_released == 0 && g_pushed == 0 && g_synced == 0 && g_capture);
        resetHooks(); g_hasProxy = false;
        CHECK(shell.h.redo()); CHECK(g_pushed == 0 && g_synced == 0);
    }

    // 12. replay invalidates Reflect planes like a committed endDrag (clone / every group member)
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(1, 0, 0, 0, 1, 1));
        CHECK(drag(st, &shell, 1, 0, mk(1, 0, 0, 0, 1, 1), true));
        st.reflected.clear();
        CHECK(shell.h.undo());
        CHECK(st.reflected.size() == 1 && st.reflected[0] == 1);
        st.reflected.clear();
        CHECK(shell.h.redo());
        CHECK(st.reflected.size() == 1 && st.reflected[0] == 1);
        ALGhostGroupModel::Group g; g.mId = 100; g.mMembers.resize(2);
        g.mMembers[0].mInstanceId = 1; g.mMembers[1].mInstanceId = 2;
        st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1);
        ALKitShell gshell;
        CHECK(drag(st, &gshell, 1, 0, mk(5, 0, 0, 0, 1, 1), true, true));
        st.reflected.clear();
        CHECK(gshell.h.undo());
        CHECK(st.reflected.size() == 2 && st.reflected[0] == 1 && st.reflected[1] == 2);
        // a rejected replay invalidates nothing
        st.groups[0].mEditMembers = true; st.reflected.clear();
        CHECK(!gshell.h.redo()); CHECK(st.reflected.empty());
    }

    // 13. wrapper state handoff: begin snapshot into members, tool kind mapping
    {
        st.reset(); resetHooks(); st.add(1, mk(2, 3, 4, 0.5f, 0.866f, 1.75f));
        LLVector3d f; LLQuaternion r; F32 s = 99.f; S32 k = -1; bool pk = true, pp = true;
        dragBeginInto(st, 1, 2, f, r, s, k, pk, pp);
        CHECK(s == 1.75f && f.mdV[2] == 4 && r.mQ[2] == 0.5f && k == 2 && !pk && !pp);
        s = 5.f; k = 7; pk = true;                         // unknown unit: members keep, pin flags reset
        dragBeginInto(st, 77, 1, f, r, s, k, pk, pp);
        CHECK(s == 5.f && k == 1 && !pk);
        CHECK(dragKindForTool(true, false) == 2 && dragKindForTool(false, true) == 1 &&
              dragKindForTool(false, false) == 0 && dragKindForTool(true, true) == 2);
        // finish forwards the members unchanged: a cancel restores scale exactly
        st.reset(); st.add(1, mk(2, 3, 4, 0.5f, 0.866f, 1.75f));
        ALKitShell shell;
        LLVector3d f2; LLQuaternion r2; F32 s2 = 0.f; S32 k2 = 0; bool pk2 = false, pp2 = false;
        dragBeginInto(st, 1, 0, f2, r2, s2, k2, pk2, pp2);
        st.transformUnit(1, mk(9, 9, 9, 0, 1, 3.f).f, mk(9, 9, 9, 0, 1, 3.f).r, 3.f);
        CHECK(!dragFinishFrom(f2, r2, s2, k2, pk2, pp2, st, &shell, 1, false));
        CHECK(cur(st, 1).s == 1.75f);
    }

    // 14. Ctrl+Z routing decision
    {
        typedef ALGhostManipProxy P;
        CHECK(cloneUndoRouteImpl(true, true, false) == P::ROUTE_UNDO);
        CHECK(cloneUndoRouteImpl(true, true, true) == P::ROUTE_REDO);
        CHECK(cloneUndoRouteImpl(false, true, false) == P::ROUTE_STOCK);
        CHECK(cloneUndoRouteImpl(false, true, true) == P::ROUTE_STOCK);
        CHECK(cloneUndoRouteImpl(true, false, false) == P::ROUTE_STOCK);
        CHECK(cloneUndoRouteImpl(true, false, true) == P::ROUTE_STOCK);
        CHECK(cloneUndoRouteImpl(false, false, false) == P::ROUTE_STOCK);
    }

    // 15. an infeasible target is rejected in the VALIDATOR and never blocks older steps:
    //     group scale 1 -> 0.5, member numeric scale 100, undo would need member 200 > 150.
    {
        st.reset(); resetHooks(); ALKitShell shell;
        st.add(3, mk(0, 0, 0, 0, 1, 1)); st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(1, 0, 0, 0, 1, 1));
        ALGhostGroupModel::Group g; g.mId = 100; g.mMembers.resize(2);
        g.mMembers[0].mInstanceId = 1; g.mMembers[1].mInstanceId = 2;
        st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1);
        const Xf c0 = cur(st, 3);
        CHECK(drag(st, &shell, 3, 0, mk(4, 0, 0, 0, 1, 1), true));            // older step
        CHECK(drag(st, &shell, 1, 2, mk(0, 0, 0, 0, 1, 0.5f), true, true));   // group 1 -> 0.5
        st.inst[2].mScale = 100.f;                                             // numeric member rescale
        CHECK(shell.h.undoStack.size() == 2);
        CHECK(!shell.h.undo());
        CHECK(has(shell.h.lastReason, "can no longer be scaled"));
        CHECK(shell.h.undoStack.size() == 1);                                  // discarded, not stuck
        CHECK(st.inst[2].mScale == 100.f && cur(st, 100).s == 0.5f);           // nothing mutated
        CHECK(shell.h.undo()); CHECK(same(cur(st, 3), c0));                    // older step reachable
    }

    // 16. unit target outside the scale range is rejected in the validator
    {
        st.reset(); resetHooks(); ALKitShell shell; st.add(1, mk(0, 0, 0, 0, 1, 500.f));
        CHECK(drag(st, &shell, 1, 2, mk(0, 0, 0, 0, 1, 2.f), true));
        CHECK(!shell.h.undo());
        CHECK(has(shell.h.lastReason, "can no longer be scaled"));
        CHECK(shell.h.undoStack.size() == 0);
    }

    // 17. a group member without a live runtime (or in motion) makes the step infeasible NOW:
    //     whole-group and member-only, undo and redo are discarded in the validator and the
    //     older step stays reachable.
    for (int mode = 0; mode < 4; ++mode)
    {
        const bool whole = mode < 2, redoCase = (mode % 2) == 1;
        st.reset(); resetHooks(); ALKitShell shell;
        st.add(3, mk(0, 0, 0, 0, 1, 1)); st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(1, 0, 0, 0, 1, 1));
        st.inst[1].mKind = st.inst[2].mKind = ALGhostStudio::BACKING_ENTITY_CLONE;
        ALGhostGroupModel::Group g; g.mId = 100; g.mMembers.resize(2); g.mEditMembers = !whole;
        g.mMembers[0].mInstanceId = 1; g.mMembers[1].mInstanceId = 2;
        st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1);
        const Xf c0 = cur(st, 3);
        CHECK(drag(st, &shell, 3, 0, mk(4, 0, 0, 0, 1, 1), true));
        CHECK(drag(st, &shell, 1, 0, mk(2, 0, 0, 0, 1, 1), true, whole));
        if (redoCase) CHECK(shell.h.undo());               // step now sits on the redo stack
        st.inst[2].mRuntimeLive = false;                   // region teardown: runtime gone
        const Xf snap1 = cur(st, 1);
        CHECK(!(redoCase ? shell.h.redo() : shell.h.undo()));
        CHECK(has(shell.h.lastReason, "not loaded right now"));
        CHECK(same(cur(st, 1), snap1));                    // nothing mutated
        CHECK((redoCase ? shell.h.redoStack.size() : shell.h.undoStack.size()) == (redoCase ? 0u : 1u));
        st.inst[2].mRuntimeLive = true;
        if (!redoCase) { CHECK(shell.h.undo()); CHECK(same(cur(st, 3), c0)); }   // older step reachable
        else { CHECK(shell.h.undo()); CHECK(same(cur(st, 3), c0)); }
    }
    {
        st.reset(); resetHooks(); ALKitShell shell;                // a member driven by motion
        st.add(1, mk(0, 0, 0, 0, 1, 1)); st.add(2, mk(1, 0, 0, 0, 1, 1));
        ALGhostGroupModel::Group g; g.mId = 100; g.mMembers.resize(2);
        g.mMembers[0].mInstanceId = 1; g.mMembers[1].mInstanceId = 2;
        st.groups.push_back(g); st.groupX[100] = mk(0, 0, 0, 0, 1, 1);
        CHECK(drag(st, &shell, 1, 0, mk(2, 0, 0, 0, 1, 1), true, true));
        st.inst[2].mMotion = ALGhostStudio::MOTION_ORBIT;
        CHECK(!shell.h.undo()); CHECK(has(shell.h.lastReason, "cannot be moved right now"));
        CHECK(shell.h.undoStack.empty());
    }

    std::printf(g_fail ? "FIXTURE FAIL (%d)\n" : "FIXTURE OK\n", g_fail);
    return g_fail ? 1 : 0;
}
'''


def extract_block(src):
    a = src.index(BEGIN) + len(BEGIN)
    b = src.index(END)
    return src[a:b]


def run_fixture(block, name):
    d = check_directory("clone_edit_undo")
    source = DOUBLES + block + BLOCK_TAIL + TEST_MAIN
    try:
        res = compile_run(d, source, name)
    except RuntimeError as e:
        return False, "compile error: " + str(e)[-1500:]
    return res.returncode == 0, res.stdout + res.stderr


def mutate(block, old, new):
    assert old in block, "mutation target missing: " + old[:50]
    return block.replace(old, new, 1)


def source_checks():
    """Thin-caller and routing checks for the code outside the block."""
    errs = []
    proxy = (NV / "alghostmanipproxy.cpp").read_text(encoding="utf-8", errors="replace")
    select = (NV / "llselectmgr.cpp").read_text(encoding="utf-8", errors="replace")
    i = proxy.index("void ALGhostManipProxy::endDrag(bool commit)")
    body = proxy[i:i + 6000]
    parts = body.split("dragFinishFrom(")
    if len(parts) < 3:
        errs.append("endDrag does not forward through dragFinishFrom twice")
    else:
        commit_call = parts[1].split(");")[0]
        cancel_call = parts[2].split(");")[0]
        for call in (commit_call, cancel_call):
            for member in ("mDragStartFoot", "mDragStartRotation", "mDragStartScale", "mDragKind",
                           "mDragStartMemberPinKnown", "mDragStartMemberPinned", "mInstanceId"):
                if member not in call:
                    errs.append("endDrag wrapper does not forward " + member)
        if "directorShell()" not in commit_call or not commit_call.rstrip().endswith("true"):
            errs.append("committed endDrag does not pass the Director shell and commit=true")
        if "nullptr" not in cancel_call or not cancel_call.rstrip().endswith("false"):
            errs.append("cancelled endDrag does not pass no shell and commit=false")
    if "recordCommittedDrag(" in body or "recordCustom" in body or "DragSnapshot" in body:
        errs.append("endDrag does logic outside the exercised block")
    bi = proxy.index("void ALGhostManipProxy::beginDrag()")
    begin = proxy[bi:proxy.index("void ALGhostManipProxy::endDrag(bool commit)")]
    for need in ("dragBeginInto(", "dragKindForTool(", "LLToolCompScale::getInstance()",
                 "LLToolCompRotate::getInstance()", "mDragStartFoot, mDragStartRotation, mDragStartScale",
                 "mDragKind, mDragStartMemberPinKnown, mDragStartMemberPinned"):
        if need not in begin:
            errs.append("beginDrag wrapper missing: " + need)
    if "DragSnapshot" in begin:
        errs.append("beginDrag does logic outside the exercised block")
    for fn, impl in (("historyBegin", "historyBeginImpl(host)"), ("historyEnd", "historyEndImpl(host)")):
        seg = proxy[proxy.index("void ALGhostManipProxy::%s()" % fn):][:1800]
        if impl not in seg:
            errs.append(fn + " does not call its production impl")
    seg = proxy[proxy.index("void ALGhostManipProxy::historyBegin()"):][:1200]
    if "comp->releaseManip()" not in seg or "p.endDrag(false)" not in seg:
        errs.append("historyBegin host lacks release/cancel wiring")
    seg = proxy[proxy.index("void ALGhostManipProxy::historyEnd()"):][:1200]
    if "pushInstanceToProxy()" not in seg or "mSeenRevision = inst->mTransformRevision" not in seg:
        errs.append("historyEnd host lacks proxy resync wiring")
    want = (("bool LLSelectMgr::canUndo() const", "cloneUndoRoute(const_cast<LLSelectMgr*>(this), false)"),
            ("void LLSelectMgr::undo()", "cloneUndoRoute(this, false)"),
            ("bool LLSelectMgr::canRedo() const", "cloneUndoRoute(const_cast<LLSelectMgr*>(this), true)"),
            ("void LLSelectMgr::redo()", "cloneUndoRoute(this, true)"))
    for sig, call in want:
        j = select.index(sig)
        seg = select[j:j + 500]
        if call not in seg:
            errs.append(sig + " does not route with " + call)
        if "route == ALGhostManipProxy::ROUTE_REDO" not in seg or "ROUTE_STOCK" not in seg:
            errs.append(sig + " does not dispatch on the route")
    k = select.index("static ALGhostManipProxy::EUndoRoute cloneUndoRoute(")
    gate = select[k:k + 500]
    if "isEditModeActive()" not in gate or "owns(" not in gate or "redo)" not in gate:
        errs.append("route inputs are not edit mode + proxy selection + redo")
    if "return cloneUndoRouteImpl(edit_active, owns_selection, redo);" not in proxy:
        errs.append("undoRoute does not forward to the exercised decision")
    return errs


def main():
    src = (NV / "alghostmanipproxy.cpp").read_text(encoding="utf-8", errors="replace")
    block = extract_block(src)
    ok, out = run_fixture(block, "fixture")
    print(out.strip())
    if not ok:
        print("FAIL: production block fixture")
        return 1
    errs = source_checks()
    for e in errs:
        print("FAIL: " + e)
    if errs:
        return 1
    negatives = [
        ("swapped before/after", "const UnitXform& target = redo ? after : before;",
         "const UnitXform& target = redo ? before : after;"),
        ("replay does nothing", "const UnitXform& target = redo ? after : before;",
         "const UnitXform& target = before; if (true) { cloneEditHistoryEnd(); return true; }"),
        ("group step keyed by member id", "ctx.mGroupId = group->mId;", "ctx.mGroupId = unit_id;"),
        ("no validator", "return editStepValid(ALGhostStudio::instance(), ctx, before, after, redo, reason);",
         "(void)ctx; (void)reason; return true;"),
        ("no feasibility check",
         "           editStepFeasible(studio, ctx, redo ? after : before, reason);", "           true;"),
        ("no group runtime preflight",
         "            !studio.resolveEntityClone(member.mInstanceId))", "            false)"),
        ("no group motion/membership preflight",
         "inst->mMotion != ALGhostStudio::MOTION_OFF || inst->mMotionHasBase ||\n            inst->mChaosEnabled || inst->mChaosHasBase)",
         "false)"),
        ("preflight skipped for member-only steps",
         "if (ctx.mWholeGroup || !ctx.mGroupId.isNull())\n    {\n        const ALGhostGroupModel::Group* group",
         "if (ctx.mWholeGroup)\n    {\n        const ALGhostGroupModel::Group* group"),
        ("no unit scale range check",
         "target.mScale < GHOST_SCALE_MIN || target.mScale > GHOST_SCALE_MAX", "false"),
        ("pin restore resets the transform",
         "    studio.setGroupMemberPinned(unit_id, true);\n    if (pinned)\n    {\n        return;\n    }",
         "    if (!studio.setGroupMemberPinned(unit_id, pinned) || pinned) return;"),
        ("no pin restore",
         "restoreMemberPin(studio, ctx.mUnitId,\n                             redo ? ctx.mPinnedAfter : ctx.mPinnedBefore);",
         ";"),
        ("no drag cancel", "    cloneEditHistoryBegin();\n    bool ok = editStepValid",
         "    bool ok = editStepValid"),
        ("member replay ignores context",
         "if (!group || group->mId != ctx.mGroupId || !group->mEditMembers)", "if (false)"),
        ("records unchanged drags", "if (!unitXformChanged(before, after))", "if (false)"),
        ("wrong BEFORE snapshot (scale)",
         "snap.mFoot, snap.mRotation, snap.mScale))\n    {\n        snap.mFoot = inst->mFootGlobal;",
         "snap.mFoot, snap.mRotation, snap.mScale) || (snap.mScale = 1.f, false))\n    {\n        snap.mFoot = inst->mFootGlobal;"),
        ("cancelled drag records a step",
         "    if (!backingInstanceForUnit(studio, unit_id))\n    {\n        return false;\n    }\n    const ALGhostGroupModel::Group* group = studio.groupForMember(unit_id);\n    const bool whole_group =",
         "    recordCommittedDrag(studio, shell, unit_id, snap.mKind, before, false, false);\n    if (!backingInstanceForUnit(studio, unit_id))\n    {\n        return false;\n    }\n    const ALGhostGroupModel::Group* group = studio.groupForMember(unit_id);\n    const bool whole_group ="),
        ("cancel does not restore",
         "studio.transformUnit(unit_id, before.mFoot, before.mRotation, before.mScale);", ";"),
        ("missing proxy re-push in historyEnd", "    host.pushProxy();\n", ""),
        ("missing revision resync in historyEnd", "    host.syncRevision();\n", ""),
        ("missing capture release in historyBegin", "    host.releaseManip();\n", ""),
        ("missing drag cancel in historyBegin", "        host.cancelDrag();\n", ""),
        ("no reflect invalidation on replay",
         "        if (ok)\n        {\n            invalidateEditReflect(studio, ctx);\n        }", ""),
        ("swapped undo/redo route",
         "return redo ? ALGhostManipProxy::ROUTE_REDO : ALGhostManipProxy::ROUTE_UNDO;",
         "return redo ? ALGhostManipProxy::ROUTE_UNDO : ALGhostManipProxy::ROUTE_REDO;"),
        ("routing with edit mode off", "if (!edit_active || !owns_selection)", "if (!owns_selection)"),
        ("routing when selection is not the proxy", "if (!edit_active || !owns_selection)", "if (!edit_active)"),
        ("wrapper snapshot forces scale to 1", "        scale = snap.mScale;", "        scale = 1.f;"),
        ("wrapper finish forces scale to 1", "    snap.mScale = scale;", "    snap.mScale = 1.f;"),
        ("tool kind mapping swapped", "return scale_tool ? 2 : rotate_tool ? 1 : 0;",
         "return scale_tool ? 1 : rotate_tool ? 2 : 0;"),
        ("hooks run with edit mode off",
         "    if (!host.editing())\n    {\n        return;\n    }\n    host.releaseManip();",
         "    host.releaseManip();"),
    ]
    for label, old, new in negatives:
        n_ok, n_out = run_fixture(mutate(block, old, new), "neg")
        if n_out.startswith("compile error"):
            print("FAIL: planted negative did not compile (invalid mutation): " + label)
            print(n_out)
            return 1
        if n_ok:
            print("FAIL: planted negative not detected: " + label)
            return 1
    print("clone_edit_undo_check: PASS (native fixture + %d planted negatives rejected)" % len(negatives))
    return 0


if __name__ == "__main__":
    sys.exit(main())
