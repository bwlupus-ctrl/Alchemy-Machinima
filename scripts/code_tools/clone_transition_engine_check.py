#!/usr/bin/env python3
"""Clone transitions, ENGINE side gate (workstream E).

1. Compiles and RUNS the production state machine + maths (indra/newview/alclonetransition.h) under the
   production unit test (tests/alclonetransition_test.cpp) with a TUT stand-in.
2. Source lifecycle checks on the production files: every row of the cancellation table (PLAN 2.6) is wired
   to ALCloneTransitions, mOverlayLook survives entity recovery, nothing transition-related reaches scene LLSD
   or settings.
3. Planted negatives that MUST FAIL: finalize called twice releases twice; stale generation accepted; the
   third concurrent transition accepted; recovery leaving mOverlayLook unset; transition data written to scene
   LLSD; a cancellation-table row not wired.

Run from the repository root:  python -I scripts/code_tools/clone_transition_engine_check.py
Exit 0 only when every check passes AND every planted negative fails as planted.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / "indra/newview"
WORK = ROOT / ".tmp/clonetrans/engine_check"


def read(path: Path) -> str:
    return path.read_bytes().decode("utf-8")


# ---------------------------------------------------------------------------------------------
# native fixture
# ---------------------------------------------------------------------------------------------
TUT_STUB = r'''
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>
namespace tut {
inline void ensure(const std::string& label, bool condition) { if (!condition) throw std::runtime_error(label); }
template<class A, class B> void ensure_equals(const std::string& label, const A& a, const B& b)
{ if (!(a == b)) { std::ostringstream s; s << label << ": expected " << b << " got " << a; throw std::runtime_error(s.str()); } }
template<class Data> struct test_group {
    struct object : Data { template<int N> void test(); };
    test_group(const char*) {}
};
}
'''


def build_fixture(header_text: str, test_text: str) -> str:
    test = test_text.replace('#include "linden_common.h"', "")
    test = test.replace('#include "../test/lltut.h"', TUT_STUB)
    test = test.replace('#include "../alclonetransition.h"', header_text)
    count = len(re.findall(r"alclonetransition_object::test<(\d+)>", test))
    main = "\nint main(){\n int failed = 0;\n"
    for n in range(1, count + 1):
        main += (f" try {{ tut::alclonetransition_object o; o.test<{n}>(); }}"
                 f" catch (const std::exception& e) {{ std::cerr << \"FAIL test {n}: \" << e.what() << std::endl; failed = 1; }}\n")
    main += f" if (!failed) std::cout << \"PASS: {count} native alclonetransition tests\" << std::endl;\n return failed;\n}}\n"
    return test + main


def compile_run(name: str, source: str) -> tuple[bool, str]:
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / (name + ".cpp")
    src.write_bytes(source.encode("utf-8"))
    vcvars = None
    for base in (os.environ.get("ProgramFiles", r"C:\Program Files"), os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")):
        found = list((Path(base) / "Microsoft Visual Studio").glob("*/*/VC/Auxiliary/Build/vcvars64.bat"))
        if found:
            vcvars = found[0]
            break
    cmd = WORK / (name + ".cmd")
    exe = WORK / (name + ".exe")
    if exe.exists():
        exe.unlink()
    if vcvars is not None:
        cmd.write_bytes((f'@echo off\r\ncall "{vcvars}" >nul\r\ncl /nologo /std:c++17 /EHsc /fp:strict /Fo"{WORK / (name + ".obj")}" /Fe"{exe}" "{src}" >"{WORK / (name + ".build.txt")}" 2>&1\r\nexit /b %ERRORLEVEL%\r\n').encode())
        build = subprocess.run(["cmd", "/d", "/c", str(cmd)], capture_output=True, text=True)
    else:
        build = subprocess.run(["g++", "-std=c++17", "-o", str(exe), str(src)], capture_output=True, text=True)
    if build.returncode != 0 or not exe.exists():
        log = WORK / (name + ".build.txt")
        detail = log.read_text(errors="replace")[-1500:] if log.exists() else build.stderr[-1500:]
        return False, "build failed: " + detail
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    return run.returncode == 0, (run.stdout + run.stderr).strip()


def mutate(text: str, old: str, new: str) -> str:
    assert old in text, "mutation anchor missing: " + old[:60]
    return text.replace(old, new, 1)


# ---------------------------------------------------------------------------------------------
# source checks (pure functions of the file texts so the planted negatives can reuse them)
# ---------------------------------------------------------------------------------------------
def body(text: str, signature: str) -> str:
    """Brace-matched body of the first function whose definition line contains `signature`."""
    m = re.search(r"^[^\n;]*" + re.escape(signature) + r"[^\n;]*\n", text, re.M)
    if not m:
        return ""
    start = text.find("{", m.end() - 1)
    if start < 0:
        return ""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return ""


# (reason, file key, function signature, required text)
TABLE = [
    ("R_SWAP_SOURCE", "studio", "ALGhostStudio::swapSource(", "ALCloneTransitions::cancelFor(id, ALCloneTransition::R_SWAP_SOURCE)"),
    ("R_FADE", "studio", "ALGhostStudio::fadeInstance(", "ALCloneTransitions::cancelFor(id, ALCloneTransition::R_FADE)"),
    ("R_DELETE", "studio", "ALGhostStudio::removeInstance(", "ALCloneTransition::R_DELETE"),
    ("R_GROUP_DELETE", "studio", "ALGhostStudio::requestRemoveInstance(", "ALCloneTransition::R_GROUP_DELETE"),
    ("R_RECOVERY (entity)", "studio", "ALGhostStudio::onEntityRuntimeReplaced(", "ALCloneTransition::R_RECOVERY"),
    ("R_RECOVERY (overlay helper)", "studio", "ALGhostStudio::updatePerFrame(", "ALCloneTransition::R_RECOVERY"),
    ("R_SHOW_CLONES_OFF", "studio", "ALGhostStudio::setShowAll(", "ALCloneTransitions::cancelAllRunning(ALCloneTransition::R_SHOW_CLONES_OFF)"),
    ("R_SOLO", "studio", "ALGhostStudio::setSolo(", "ALCloneTransitions::cancelAllRunning(ALCloneTransition::R_SOLO)"),
    ("R_EDIT_LOCK (enable)", "studio", "ALGhostStudio::setInstanceEnabled(", "ALCloneTransition::R_EDIT_LOCK"),
    ("R_EDIT_LOCK (lockGroup)", "studio", "ALGhostStudio::lockGroup(", "ALCloneTransition::R_EDIT_LOCK"),
    ("R_EDIT_LOCK (ungroup)", "studio", "ALGhostStudio::ungroup(", "cancelGroupTransitions(id)"),
    ("R_EDIT_LOCK (lock mode)", "studio", "ALGhostStudio::setLockMode(", "cancelGroupTransitions(id)"),
    ("R_EDIT_LOCK (group helper cancels every member)", "studio", "ALGhostStudio::cancelGroupTransitions(", "ALCloneTransition::R_EDIT_LOCK"),
    ("R_EDIT_LOCK (look edit)", "actions", "void setOverlayStyle(", "ALCloneTransition::R_EDIT_LOCK"),
    ("R_CANCEL (button)", "panel", "ALPanelGhostStudio::postBuild(", "cancelTransition("),
    ("R_DISCONNECT", "viewer", "LLAppViewer::disconnectViewer(", "ALCloneTransitions::onDisconnect()"),
    ("R_ALL_OFF", "alloff", "bool reset(", "ALCloneTransitions::onAllOff("),
    ("R_TELEPORT (clock)", "manager", "void tick()", "ALCloneTransition::R_TELEPORT"),
    ("R_TELEPORT (test)", "manager", "void tick()", "LLAgent::TELEPORT_NONE"),
    ("R_SCENE_LOAD (hook)", "manager", "ALCloneTransitions::onSceneReplaced(", "R_SCENE_LOAD"),
    ("R_SHUTDOWN (hook)", "manager", "ALCloneTransitions::shutdown(", "OUT_DISCARD"),
    ("R_DISCONNECT (hook)", "manager", "ALCloneTransitions::onDisconnect(", "OUT_DISCARD"),
    ("ALL OFF rolls back when clones are hidden", "manager", "ALCloneTransitions::onAllOff(", "clones_hidden ? ALCloneTransition::OUT_ROLLBACK : ALCloneTransition::OUT_COMMIT"),
]


def check_table(files: dict[str, str]) -> list[str]:
    problems = []
    for label, key, signature, needle in TABLE:
        func = body(files[key], signature)
        if not func:
            problems.append(f"cancellation table: function not found for {label}: {signature}")
        elif needle not in func:
            problems.append(f"cancellation table row not wired: {label} (missing `{needle}` in {signature})")
    # shutdown order: ALCloneTransitions::shutdown() immediately before disconnectViewer() in the shutdown path
    viewer = files["viewer"]
    m = re.search(r"ALCloneTransitions::shutdown\(\);\s*\n\s*disconnectViewer\(\);", viewer)
    if not m:
        problems.append("cancellation table row not wired: R_SHUTDOWN (ALCloneTransitions::shutdown() must precede disconnectViewer())")
    if "ALCloneTransitions::tick();" not in viewer or "ALCloneTransitions::refreshBodyFrames();" not in viewer:
        problems.append("llappviewer.cpp: per-frame tick / refreshBodyFrames not wired")
    if viewer.find("ALCloneTransitions::tick();") > viewer.find("ALGhostStudio::instance().updatePostObjectList();") > 0:
        problems.append("llappviewer.cpp: the clock tick must run before gObjectList.update()/updatePostObjectList")
    return problems


def check_overlay_look(studio: str) -> list[str]:
    """mOverlayLook must survive entity recovery: applyEntityRuntimeState re-applies the render-only hide."""
    problems = []
    render_only = body(studio, "bool ALGhostStudio::entityRenderOnly(")
    if "mOverlayLook >= 0" not in render_only:
        problems.append("recovery would leave the entity body visible: entityRenderOnly ignores mOverlayLook")
    apply = body(studio, "void ALGhostStudio::applyEntityRuntimeState(")
    if "entityRenderOnly(inst)" not in apply or "set_entity_clone_visible(ghost, isVisible(inst)" not in apply:
        problems.append("applyEntityRuntimeState does not re-apply the overlay-look hide to the new body")
    callers = re.findall(r"set_entity_clone_visible\(([^;]*)\);", studio)
    for args in callers:
        top = args.split(",")
        if len(top) >= 2 and "isVisible" in top[1] and "entityRenderOnly" not in args:
            problems.append("set_entity_clone_visible caller drops the overlay-look hide: " + args.strip()[:80])
    if "S32         mOverlayLook = -1;" not in read(NV / "alghoststudio.h"):
        problems.append("Instance::mOverlayLook default -1 missing")
    return problems


def check_session_only(files: dict[str, str], tree: dict[str, str]) -> list[str]:
    """No transition data or mOverlayLook in scene LLSD, saved scenes or persistent settings."""
    problems = []
    manager = files["manager"]
    for token in ("LLSD", "gSavedSettings", "gSavedPerAccountSettings", "toPrettyXML", "LLSDSerialize"):
        if token in manager:
            problems.append(f"alclonetransitions.cpp touches persistence ({token}): transitions are session only")
    for name, text in tree.items():
        if name in ("alghoststudio.cpp", "alghoststudio.h", "alclonetransitions.cpp", "alclonetransitions.h"):
            continue
        if "mOverlayLook" in text and name not in ("alghostcloneactions.cpp", "alghostclonemenu.cpp", "llactormover.cpp"):
            problems.append(f"{name} references mOverlayLook: it must never reach a serializer")
    for fname in ("lldirectorcast.cpp",):
        text = tree.get(fname, "")
        if "mOverlayLook" in text:
            problems.append(f"{fname} writes mOverlayLook into the scene")
        if re.search(r"\[\"[^\"]*(?:transition|overlay_look)[^\"]*\"\]\s*=", text, re.I):
            problems.append(f"{fname} writes transition data into the scene")
    studio = files["studio"]
    for m in re.finditer(r"\[\"[^\"]*\"\]\s*=[^;\n]*mOverlayLook|mOverlayLook[^;\n]*\[\"", studio):
        problems.append("alghoststudio.cpp serializes mOverlayLook: " + m.group(0)[:60])
    return problems


def check_ui(files: dict[str, str]) -> list[str]:
    problems = []
    page = files["page"]
    widgets = ["transition_target_combo", "transition_style_combo", "transition_duration_spinner", "transition_ease_combo",
               "transition_direction_combo", "btn_transition_go", "btn_transition_cancel", "transition_status"]
    for w in widgets:
        if f'name="{w}"' not in page:
            problems.append(f"Clones page XUI lacks {w}")
        if f'"{w}"' not in files["panel"]:
            problems.append(f"panel does not bind {w}")
    if 'name="transition"' not in page and 'id="transition"' not in page:
        problems.append("Clones page has no Transition section")
    menu = files["menu"]
    for item in ("Transition Look", "Transition Overlay Look", "Transition Normal Look"):
        if f'name="{item}"' not in menu:
            problems.append(f"clone menu lacks {item}")
    if menu.find('name="Transition To"') < menu.find('name="Swap Source"'):
        problems.append("Transition to... must sit beside Swap source (after it)")
    actions_h = files["actions_h"]
    if "transitionTo(" not in actions_h:
        problems.append("ALGhostCloneActions::transitionTo missing")
    panel = files["panel"]
    if "transitionTo(" not in panel:
        problems.append("page Go does not use the shared transitionTo action")
    if "hasExpandedMember(rows)" not in files["actions"]:
        problems.append("transitionTo does not refuse expanded group members")
    return problems


def check_contract(files: dict[str, str]) -> list[str]:
    problems = []
    h = files["manager_h"]
    for sig in ("nativeParamsFor(", "shellParamsFor(", "studioLayersFor(", "overlayLookFor(", "anyRunning()"):
        if sig not in h:
            problems.append(f"alclonetransitions.h lost the render-facing query {sig}")
    return problems


def check_seams(files: dict[str, str], tree: dict[str, str]) -> list[str]:
    """Integration seams between the engine (manager) and the render side. The render side draws each half
    with the params the manager hands out; the two halves of one transition must be COMPLEMENTARY and share one
    BodyFrame, or a point is kept by both halves / neither (the 'ghost body' failure)."""
    problems = []
    m = files["manager"]
    native = body(m, "ALCloneTransitions::nativeParamsFor(")
    layers = body(m, "ALCloneTransitions::studioLayersFor(")
    start = body(m, "bool ALCloneTransitions::start(")
    if not native or not layers or not start:
        return ["nativeParamsFor / studioLayersFor / start not found in alclonetransitions.cpp"]
    # native half (fixed ONCE at start(); S7: the getter never mutates): OUTGOING only for entity -> overlay look, otherwise INCOMING
    if not re.search(r"extra\.params\.side\s*=\s*rq\.kind\s*==\s*ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK\s*\?\s*ALCloneTransition::SIDE_OUTGOING\s*:\s*ALCloneTransition::SIDE_INCOMING", start):
        problems.append("start(): native half must be SIDE_OUTGOING for entity->overlay look and SIDE_INCOMING for the reverse kind")
    if re.search(r"\.side\s*=[^=]", native):
        problems.append("nativeParamsFor mutates the params (S7): the side is decided once in start()")

    def case(kind):
        mm = re.search(r"case ALCloneTransition::" + kind + r":(.*?)break;", layers, re.S)
        return mm.group(1) if mm else ""
    l2l, e2o, o2e = case("KIND_LOOK_TO_LOOK"), case("KIND_ENTITY_TO_OVERLAY_LOOK"), case("KIND_OVERLAY_LOOK_TO_ENTITY")
    if not (re.search(r"layer\[0\]\.params\.side = ALCloneTransition::SIDE_OUTGOING", l2l)
            and re.search(r"layer\[1\]\.params\.side = ALCloneTransition::SIDE_INCOMING", l2l) and "count = 2" in l2l):
        problems.append("studioLayersFor: Look->Look must give two layers, outgoing (fromLook) then incoming (toLook)")
    if not (re.search(r"layer\[0\]\.params\.side = ALCloneTransition::SIDE_INCOMING", e2o) and "attachedPlacement = true" in e2o):
        problems.append("studioLayersFor: entity->overlay look Studio layer must be SIDE_INCOMING with attached placement")
    if not (re.search(r"layer\[0\]\.params\.side = ALCloneTransition::SIDE_OUTGOING", o2e) and "attachedPlacement = true" in o2e):
        problems.append("studioLayersFor: overlay look->entity Studio layer must be SIDE_OUTGOING with attached placement")
    # one frame source: both halves are built from the SAME per-record params (frame filled by fillFrame only)
    if "it->second.params" not in native or "it->second.params" not in layers:
        problems.append("native and Studio halves must both be built from the one per-record params (shared BodyFrame)")
    if m.count("rootAgent[") != 1:
        problems.append("BodyFrame must be written in exactly one place (fillFrame)")
    for name in ("llactormover.cpp", "lldrawpool.cpp", "pipeline.cpp", "lldirectorcast.cpp"):
        if re.search(r"\brootAgent\b|BodyFrame\s+\w+\s*[;=]", tree.get(name, "")):
            problems.append(name + " builds its own BodyFrame; the frame comes from ALCloneTransitions only")
    am = tree.get("llactormover.cpp", "")
    first = am.find("studioLayersFor(inst.mId, layers)")
    judge = am.find("inst.mOverlayLook < 0 && layers.count == 0")
    if first < 0 or judge < 0 or first > judge:
        problems.append("renderStudioGhosts must query studioLayersFor before judging inst.mOverlayLook")
    return problems


def check_fix_r1(files: dict[str, str]) -> list[str]:
    """Fix round 1 (review findings M1, M2, M6, M7, M8, S2, S6, S7): each rule is a source pattern on the production
    files; every one has a planted negative in main() that must fail it."""
    problems = []
    m, studio, panel = files["manager"], files["studio"], files["panel"]
    # M1: the running state is published BEFORE the visibility callback reads it
    started = body(m, "void onStarted(")
    pub = started.find("sRunning = mImpl->mMachine.running();")
    if not started or pub < 0 or pub > started.find("ALGhostStudio::instance().reapplyEntityBodyFlags"):
        problems.append("M1: Host::onStarted must publish sRunning BEFORE reapplyEntityBodyFlags (keepsBodyShown is gated on it)")
    # M2: a duplicate start is refused before anything touches mExtras
    start = body(m, "bool ALCloneTransitions::start(")
    dup = start.find("if (mImpl->mMachine.find(instance_id)) {")
    extras = start.find("mImpl->mExtras[instance_id] = extra")
    if dup < 0 or extras < 0 or dup > extras:
        problems.append("M2: start() must reject an already-running clone BEFORE writing mExtras")
    if "setEnabledIfChanged(mTransitionGoBtn, have && !active)" not in panel:
        problems.append("M2: Go must be disabled while the selected clone's transition runs")
    # M6: system parts the Studio entity branch cannot draw refuse at start and at commit
    if "ALCloneTransitions::systemPartsVisible(ghost)" not in start:
        problems.append("M6: start() does not refuse an entity whose system body / eyes are visible")
    if "ALCloneTransitions::systemPartsVisible(ghost)" not in body(m, "bool bodyValid("):
        problems.append("M6: bodyValid (tick + commit re-check) does not re-test the visible system parts")
    # M7: group edits cancel every member, snapshotted before the change
    cg = body(studio, "void ALGhostStudio::cancelGroupTransitions(")
    if "groupMembers(id)" not in cg or "mGroups.members(" not in cg:
        problems.append("M7: cancelGroupTransitions must cancel groupMembers(id) and the full group member list")
    for sig in ("ALGhostStudio::setLockMode(", "ALGhostStudio::ungroup("):
        if "cancelGroupTransitions(id)" not in body(studio, sig):
            problems.append("M7: %s does not cancel every group member" % sig)
    lm = body(studio, "ALGhostStudio::setLockMode(")
    if lm.find("cancelGroupTransitions(id)") < 0 or lm.find("cancelGroupTransitions(id)") > lm.find("mGroups.setMode("):
        problems.append("M7: setLockMode changes the mode before cancelling the members")
    # M8: a direct writer of Instance::mStyle (crowd look assignment) cancels first
    cv = body(studio, "void ALGhostStudio::applyCrowdVariation(")
    first_cancel = cv.find("ALCloneTransitions::cancelFor(")
    if first_cancel < 0 or first_cancel > cv.find("inst.mStyle = "):
        problems.append("M8: applyCrowdVariation writes mStyle without cancelling the affected transitions first")
    # S2: bodyHeight derived per record
    ff = body(m, "void fillFrame(")
    if "params.bodyHeight" not in ff or "mBodySize" not in ff:
        problems.append("S2: fillFrame does not derive bodyHeight from the body")
    # S6: the idle per-frame scan is O(1)
    if "!studio.anyEntityOverlayLook() && !ALCloneTransitions::anyRunning()" not in read(NV / "llactormover.cpp"):
        problems.append("S6: studio_entity_overlay_wanted is not guarded by the O(1) overlay-look counter")
    if "S32 mOverlayLookCount = 0;" not in files["studio_h"]:
        problems.append("S6: overlay-look counter missing")
    # fix round 2: the counter is maintained through the ONE boundary (setOverlayLookField / accountOverlayLook)
    if "setOverlayLookField(" not in body(studio, "bool ALGhostStudio::setEntityOverlayLook(") or \
            "accountOverlayLook(" not in body(studio, "void ALGhostStudio::removeInstance("):
        problems.append("S6: counter not maintained by setEntityOverlayLook and removeInstance")
    # S7: Machine::start range-checks the kind
    if "rq.kind < KIND_LOOK_TO_LOOK || rq.kind > KIND_OVERLAY_LOOK_TO_ENTITY" not in read(NV / "alclonetransition.h"):
        problems.append("S7: Machine::start does not range-check rq.kind")
    return problems


def signature_line(text: str, signature: str) -> str:
    m = re.search(r"^[^\n;]*" + re.escape(signature) + r"[^\n;]*\n", text, re.M)
    return m.group(0).strip() if m else ""


COUNTER_MOCK = r"""
#include <vector>
#include <cstdio>
typedef int S32;
struct Mock
{
    struct Instance { int mId = 0; S32 mOverlayLook = -1; };
    std::vector<Instance> mInstances;
    S32 mOverlayLookCount = 0;
    void accountOverlayLook(S32 before, S32 after);
    void setOverlayLookField(Instance& inst, S32 look);
    void pushInstance(const Instance& inst);
    void assignRecord(Instance& dst, const Instance& src);
    // removeInstance's statement: account the retiring record, then erase it
    void removeInstance(int id)
    {
        for (size_t i = 0; i < mInstances.size(); ++i)
            if (mInstances[i].mId == id) { accountOverlayLook(mInstances[i].mOverlayLook, -1); mInstances.erase(mInstances.begin() + i); return; }
    }
    Instance* find(int id) { for (auto& i : mInstances) if (i.mId == id) return &i; return nullptr; }
    S32 recount() const { S32 n = 0; for (const auto& i : mInstances) n += i.mOverlayLook >= 0; return n; }
};
@BODIES@
int main()
{
    int bad = 0;
    auto check = [&](const char* what, Mock& m) { if (m.mOverlayLookCount != m.recount()) { std::printf("INVARIANT BROKEN after %s: counter %d vs %d\n", what, m.mOverlayLookCount, m.recount()); bad = 1; } };
    for (int order = 0; order < 2; ++order)
    {
        Mock m;
        Mock::Instance a; a.mId = 1;
        m.pushInstance(a);
        m.setOverlayLookField(*m.find(1), 10);            // committed overlay look
        check("commit", m);
        Mock::Instance spawned; spawned.mId = 2;           // duplicateInstanceSnapshotInPlace: spawn, then copy the prototype in place
        m.pushInstance(spawned);
        Mock::Instance proto = *m.find(1);
        const int new_id = 2;
        Mock::Instance* dup = m.find(2);
        m.assignRecord(*dup, proto);
        dup->mId = new_id;                                 // production restores the stable id after the copy
        check("duplicate", m);
        if (order == 0)
        {
            m.setOverlayLookField(*m.find(1), -1);          // original reverse-commit
            check("original reverse-commit", m);
            if (!(m.mOverlayLookCount > 0)) { std::printf("duplicate lost its look accounting after the original returned to Normal\n"); bad = 1; }
            m.removeInstance(1);
            check("original delete", m);
        }
        else
        {
            m.removeInstance(1);
            check("original delete", m);
            if (!(m.mOverlayLookCount > 0)) { std::printf("duplicate lost its look accounting after the original was deleted\n"); bad = 1; }
        }
        Mock::Instance prebuilt; prebuilt.mId = 3; prebuilt.mOverlayLook = 7;   // a record inserted already carrying a look
        m.pushInstance(prebuilt);
        check("insertion of a record with a look", m);
        m.removeInstance(3);
        check("delete of that record", m);
        m.removeInstance(2);
        check("duplicate delete", m);
        if (m.mOverlayLookCount != 0) bad = 1;
    }
    if (!bad) std::printf("PASS counter invariant\n");
    return bad;
}
"""


def counter_fixture(studio: str) -> str:
    bodies = ""
    for sig in ("void ALGhostStudio::accountOverlayLook(", "void ALGhostStudio::setOverlayLookField(",
                "void ALGhostStudio::pushInstance(", "void ALGhostStudio::assignRecord("):
        line = signature_line(studio, sig)
        text = body(studio, sig)
        if not line or not text:
            return ""
        bodies += line.replace("ALGhostStudio::", "Mock::") + "\n" + text + "\n"
    return COUNTER_MOCK.replace("@BODIES@", bodies)


def check_fix_r2(files: dict[str, str], tree: dict[str, str]) -> list[str]:
    """Fix round 2 (N2 counter boundary, N3 replacement validation, T1 clock, T2 system-part rule, T4)."""
    problems = []
    m, studio, mover = files["manager"], files["studio"], tree.get("llactormover.cpp", "")
    # N2: ONE state-setting boundary for the overlay-look counter
    for sig in ("accountOverlayLook(", "setOverlayLookField(", "pushInstance(", "assignRecord("):
        if not body(studio, "void ALGhostStudio::" + sig):
            problems.append("N2: ALGhostStudio::" + sig + " missing")
    if studio.count("mInstances.push_back(") != 1 or "mInstances.push_back(inst);" not in body(studio, "void ALGhostStudio::pushInstance("):
        problems.append("N2: an instance is inserted outside pushInstance")
    if "accountOverlayLook(-1, inst.mOverlayLook)" not in body(studio, "void ALGhostStudio::pushInstance("):
        problems.append("N2: pushInstance does not account the inserted record")
    ar = body(studio, "void ALGhostStudio::assignRecord(")
    if "accountOverlayLook(before, dst.mOverlayLook)" not in ar or "dst = src;" not in ar:
        problems.append("N2: assignRecord does not account the whole-record copy")
    dup = body(studio, "ALGhostStudio::duplicateInstanceSnapshotInPlace(")
    if "assignRecord(*copy, proto)" not in dup or re.search(r"\*copy\s*=\s*proto\s*;", dup):
        problems.append("N2: duplicateInstanceSnapshotInPlace copies the record without the counter boundary")
    for mm in re.finditer(r"mOverlayLook\s*=[^=]", studio):
        # the only direct field writes: the Instance default (header) and setOverlayLookField
        start = studio.rfind("\nvoid ALGhostStudio::", 0, mm.start())
        head = studio[start:studio.find("\n", start + 1)] if start >= 0 else ""
        if "setOverlayLookField(" not in head:
            problems.append("N2: mOverlayLook written outside setOverlayLookField: " + studio[mm.start() - 30:mm.start() + 30].strip())
    for mm in re.finditer(r"mOverlayLookCount\s*(\+|-)?=[^=]", studio):
        start = studio.rfind("\nvoid ALGhostStudio::", 0, mm.start())
        head = studio[start:studio.find("\n", start + 1)] if start >= 0 else ""
        if "accountOverlayLook(" not in head and "ALGhostStudio::removeAll(" not in head:
            problems.append("N2: mOverlayLookCount written outside accountOverlayLook / removeAll")
    # R3 Q2: every wholesale change of the instance list stays inside the accounting boundary
    for bad_op in ("mInstances.clear(", "mInstances.swap(", "mInstances = ", "mInstances.assign(", "mInstances.insert(", "mInstances.emplace"):
        if bad_op in studio or any(bad_op in t for k, t in tree.items() if k.startswith("alghoststudio") and k != "alghoststudio.cpp"):
            problems.append("R3Q2: wholesale instance-list operation outside the counter boundary: " + bad_op)
    if studio.count("mInstances.erase(") != 1 or "mInstances.erase(" not in body(studio, "void ALGhostStudio::removeInstance("):
        problems.append("R3Q2: an instance is erased outside removeInstance")
    ra = body(studio, "void ALGhostStudio::removeAll(")
    if "removeInstance(mInstances.back().mId)" not in ra or "mOverlayLookCount = 0;" not in ra or ra.index("mOverlayLookCount = 0;") < ra.index("removeInstance("):
        problems.append("R3Q2: removeAll must drain through removeInstance and then reset the overlay-look counter")
    if "accountOverlayLook(retiring->mOverlayLook, -1)" not in body(studio, "void ALGhostStudio::removeInstance("):
        problems.append("N2: removeInstance does not account the retiring record")
    if "setOverlayLookField(*inst, look)" not in body(studio, "bool ALGhostStudio::setEntityOverlayLook("):
        problems.append("N2: setEntityOverlayLook bypasses setOverlayLookField")
    # N3: every body installation validates the overlay look first
    apply_fn = body(studio, "void ALGhostStudio::applyEntityRuntimeState(")
    v = apply_fn.find("dropUnsupportedOverlayLookOf(inst, ghost)")
    h = apply_fn.find("set_entity_clone_visible(ghost, isVisible(inst), entityRenderOnly(inst))")
    if v < 0 or h < 0 or v > h:
        problems.append("N3: applyEntityRuntimeState must validate the overlay look BEFORE the hide is applied to the new body")
    drop = body(studio, "bool ALGhostStudio::dropUnsupportedOverlayLookOf(")
    if "ALCloneTransitions::systemPartsVisible(body)" not in drop or "setOverlayLookField(inst, -1)" not in drop or "ALCloneTransitions::setNote(" not in drop:
        problems.append("N3: the replacement validation must drop the look to Normal (-1) with a status note when system parts are visible")
    if "onEntityRuntimeReplaced(" not in body(studio, "ALGhostStudio::swapSource(") or \
            "onEntityRuntimeReplaced(" not in body(studio, "ALGhostStudio::refreshEntityClone(") or \
            "applyEntityRuntimeState(inst, new_ghost)" not in body(studio, "ALGhostStudio::onEntityRuntimeReplaced("):
        problems.append("N3: swap / refresh / recovery no longer install their body through applyEntityRuntimeState")
    # R3 Q1: the periodic re-validation (a bake that loads late) runs BEFORE rendering, never inside the render loop
    upf = body(studio, "void ALGhostStudio::updatePerFrame(")
    if "dropUnsupportedOverlayLook(id)" not in upf or "mOverlayLookCount > 0" not in upf or "& 15u" not in upf or ".isRunning(id)" not in upf:
        problems.append("N3/Q1: updatePerFrame does not periodically re-validate steady overlay looks (O(1) idle guard, 1-in-16, skips running transitions)")
    if "dropUnsupportedOverlayLook(" in mover:
        problems.append("Q1: the periodic overlay-look validation mutates state from the render loop (one invisible frame)")
    # T1: the manager clock holds while the world is frozen
    tick = body(m, "void tick()")
    if "ALCloneTransition::clockDelta(" not in tick or 'ALCloneTransition::clockDelta(LLPipeline::FreezeTime,' not in tick or "frame.presentation_delta, frame.wall_delta" not in tick:
        problems.append("T1: the manager clock does not use clockDelta with the FreezeTime state")
    if "if (worldFrozen) return 0.0;" not in files["header"]:
        problems.append("T1: clockDelta no longer returns 0 for a frozen world")
    # T2: the system-part rule is the native decision (bake visible AND not hidden by Bakes on Mesh AND not invisible)
    sv = body(m, "ALCloneTransitions::systemPartsVisible(")
    for token in ("isTextureVisible(TEX_HEAD_BAKED)", "isTextureVisible(TEX_UPPER_BAKED)", "isTextureVisible(TEX_LOWER_BAKED)",
                  "isTextureVisible(TEX_EYES_BAKED)", "isTextureVisible(TEX_HAIR_BAKED)", "isTextureVisible(TEX_SKIRT_BAKED)",
                  "in.bomHead = bom_hides(avatar, IMG_USE_BAKED_HEAD)", "in.bomUpper = bom_hides(avatar, IMG_USE_BAKED_UPPER)",
                  "in.bomLower = bom_hides(avatar, IMG_USE_BAKED_LOWER)", "in.bomEyes = bom_hides(avatar, IMG_USE_BAKED_EYES)",
                  "in.bomHair = bom_hides(avatar, IMG_USE_BAKED_HAIR)", "in.bomSkirt = bom_hides(avatar, IMG_USE_BAKED_SKIRT)",
                  "AOA_INVISIBLE", "ALCloneTransition::systemPartsDrawn(in)"):
        if token not in sv:
            problems.append("T2: systemPartsVisible lost `%s` (the native draw rule)" % token)
    # T4: the dead duplicate-find in the MAX_RUNNING guard is gone
    guard = re.search(r"if \(mImpl->mMachine\.running\(\) >= ALCloneTransition::MAX_RUNNING[^\n]*\n[^\n]*\n", body(m, "bool ALCloneTransitions::start("))
    if not guard or ".find(instance_id)" in guard.group(0):
        problems.append("T4: the MAX_RUNNING guard is missing or still carries the unreachable find(instance_id)")
    return problems


def load_files() -> tuple[dict[str, str], dict[str, str]]:
    xui = NV / "skins/default/xui/en"
    files = {
        "studio": read(NV / "alghoststudio.cpp"),
        "studio_h": read(NV / "alghoststudio.h"),
        "actions": read(NV / "alghostcloneactions.cpp"),
        "actions_h": read(NV / "alghostcloneactions.h"),
        "panel": read(NV / "alpanelghoststudio.cpp"),
        "viewer": read(NV / "llappviewer.cpp"),
        "alloff": read(NV / "aldirectoralloff.cpp"),
        "manager": read(NV / "alclonetransitions.cpp"),
        "manager_h": read(NV / "alclonetransitions.h"),
        "header": read(NV / "alclonetransition.h"),
        "page": read(xui / "panel_dpage_cast_ghosts.xml"),
        "menu": read(xui / "menu_ghost_clone.xml"),
    }
    tree = {}
    for path in NV.glob("*.cpp"):
        tree[path.name] = read(path)
    for path in NV.glob("*.h"):
        tree[path.name] = read(path)
    return files, tree


def all_problems(files, tree) -> list[str]:
    return (check_table(files) + check_overlay_look(files["studio"]) + check_session_only(files, tree)
            + check_ui(files) + check_contract(files) + check_seams(files, tree) + check_fix_r1(files)
            + check_fix_r2(files, tree))


def main() -> int:
    failures: list[str] = []
    header = read(NV / "alclonetransition.h")
    test = read(NV / "tests/alclonetransition_test.cpp")

    ok, out = compile_run("fixture", build_fixture(header, test))
    print(("PASS " if ok else "FAIL ") + "native fixture: " + out.splitlines()[-1] if out else ("PASS native fixture" if ok else "FAIL native fixture"))
    if not ok:
        failures.append("native fixture failed:\n" + out)

    files, tree = load_files()
    problems = all_problems(files, tree)
    for p in problems:
        failures.append(p)
    print(("PASS" if not problems else "FAIL") + f" source lifecycle checks ({len(TABLE)} table rows + shutdown order + overlay-look recovery + session-only + UI + contract)")
    # the scene-load call site lives in lldirectorcast.cpp (integrated): first statement of applySceneData
    scene_body = body(tree.get("lldirectorcast.cpp", ""), "void LLDirectorCast::applySceneData(")
    hook_ok = scene_body.lstrip("{\n ").startswith("ALCloneTransitions::onSceneReplaced();")
    print(("PASS" if hook_ok else "FAIL") + " scene load / cast clear: onSceneReplaced() is the first statement of LLDirectorCast::applySceneData")
    if not hook_ok:
        failures.append("lldirectorcast.cpp applySceneData must call ALCloneTransitions::onSceneReplaced() first")

    fixture_src = counter_fixture(files["studio"])
    okc, outc = compile_run("counter", fixture_src) if fixture_src else (False, "counter fixture could not be built from the production helpers")
    print(("PASS " if okc else "FAIL ") + "overlay-look counter invariant (production helpers; duplicate, original reverse-commit, original delete): " + (outc.splitlines()[-1] if outc else ""))
    if not okc:
        failures.append("counter invariant fixture failed:\n" + outc)

    # ---------------- planted negatives: each MUST fail ----------------
    negatives = []

    def native_negative(label, old, new):
        mutated = mutate(header, old, new)
        okm, outm = compile_run("neg_" + re.sub(r"\W+", "_", label)[:30], build_fixture(mutated, test))
        negatives.append((label, not okm, outm.splitlines()[0] if outm else ""))

    native_negative("finalize called twice releases twice",
                    "        mRecords.erase(it);\n        const bool cloneOk",
                    "        const bool cloneOk")
    native_negative("stale generation accepted",
                    "        if (generation != 0 && it->generation != generation) return false;\n", "")
    native_negative("third concurrent transition accepted", "inline constexpr int MAX_RUNNING = 2;", "inline constexpr int MAX_RUNNING = 3;")
    native_negative("both-invalid not discarded",
                    "        if (!cloneOk && !bodyOk) o = OUT_DISCARD;", "        if (false) o = OUT_DISCARD;")

    # recovery during RUNNING leaving mOverlayLook unset
    f2 = dict(files)
    f2["studio"] = mutate(files["studio"], "(inst.mOverlayLook >= 0 && isVisible(inst) && !ALCloneTransitions::keepsBodyShown(inst.mId))", "false")
    negatives.append(("recovery during RUNNING leaves mOverlayLook unset", bool(check_overlay_look(f2["studio"])), ""))
    f2b = dict(files)
    f2b["studio"] = mutate(files["studio"], "    set_entity_clone_visible(ghost, isVisible(inst), entityRenderOnly(inst));\n\n    // A fresh skeleton",
                           "    set_entity_clone_visible(ghost, isVisible(inst));\n\n    // A fresh skeleton")
    negatives.append(("recovery builds the new body without the overlay hide", bool(check_overlay_look(f2b["studio"])), ""))

    # transition data written to scene LLSD
    f3 = dict(files)
    f3["manager"] = files["manager"] + "\nstatic void planted(LLSD& scene) { scene[\"transition\"] = LLSD(1); }\n"
    negatives.append(("transition data written to scene LLSD", bool(check_session_only(f3, tree)), ""))
    tree3 = dict(tree)
    tree3["lldirectorcast.cpp"] = tree.get("lldirectorcast.cpp", "") + "\n data[\"overlay_look\"] = inst.mOverlayLook;\n"
    negatives.append(("mOverlayLook written by the scene serializer", bool(check_session_only(files, tree3)), ""))

    # a cancellation-table row not wired
    for label, old in (
            ("R_FADE row not wired", "    ALCloneTransitions::cancelFor(id, ALCloneTransition::R_FADE); // [CloneTransition] ROLLBACK, then the fade (PLAN 2.6)\n"),
            ("R_SOLO row not wired", "    ALCloneTransitions::cancelAllRunning(ALCloneTransition::R_SOLO); // [CloneTransition] visibility edit (D18)\n"),
            ("R_RECOVERY row not wired", "    ALCloneTransitions::cancelFor(stable_id, removing_instance ? ALCloneTransition::R_DELETE : ALCloneTransition::R_RECOVERY);\n")):
        f4 = dict(files)
        f4["studio"] = mutate(files["studio"], old, "")
        negatives.append((label, bool(check_table(f4)), ""))
    f5 = dict(files)
    f5["viewer"] = mutate(files["viewer"], "    ALCloneTransitions::shutdown();\n", "")
    negatives.append(("R_SHUTDOWN row not wired", bool(check_table(f5)), ""))
    f6 = dict(files)
    f6["alloff"] = mutate(files["alloff"], "    ALCloneTransitions::onAllOff(!ALGhostStudio::instance().getShowAll());\n", "")
    negatives.append(("ALL OFF row not wired", bool(check_table(f6)), ""))

    # seam negatives
    f7 = dict(files)
    f7["manager"] = mutate(files["manager"], "KIND_ENTITY_TO_OVERLAY_LOOK\n        ? ALCloneTransition::SIDE_OUTGOING : ALCloneTransition::SIDE_INCOMING",
                           "KIND_ENTITY_TO_OVERLAY_LOOK\n        ? ALCloneTransition::SIDE_INCOMING : ALCloneTransition::SIDE_OUTGOING")
    negatives.append(("native half sides swapped (a1 / reverse)", bool(check_seams(f7, tree)), ""))
    f8 = dict(files)
    f8["manager"] = mutate(files["manager"], "out.layer[1].params = params; out.layer[1].params.side = ALCloneTransition::SIDE_INCOMING;",
                           "out.layer[1].params = params; out.layer[1].params.side = ALCloneTransition::SIDE_OUTGOING;")
    negatives.append(("Look->Look layers not complementary", bool(check_seams(f8, tree)), ""))
    f9 = dict(files)
    f9["manager"] = mutate(files["manager"], "        out.layer[0].look = rec->fromLook;\n        out.layer[0].params = params; out.layer[0].params.side = ALCloneTransition::SIDE_OUTGOING;\n        break;\n    default:",
                           "        out.layer[0].look = rec->fromLook;\n        out.layer[0].params = params; out.layer[0].params.side = ALCloneTransition::SIDE_INCOMING;\n        break;\n    default:")
    negatives.append(("reverse kind Studio layer not outgoing", bool(check_seams(f9, tree)), ""))
    tree9 = dict(tree)
    tree9["llactormover.cpp"] = tree["llactormover.cpp"] + "\nstatic void planted(){ ALCloneTransition::BodyFrame f; f.rootAgent[0] = 1.f; }\n"
    negatives.append(("render side builds its own BodyFrame", bool(check_seams(files, tree9)), ""))
    tree10 = dict(tree)
    tree10["lldirectorcast.cpp"] = tree["lldirectorcast.cpp"].replace("    ALCloneTransitions::onSceneReplaced();", "    ", 1)
    sb = body(tree10["lldirectorcast.cpp"], "void LLDirectorCast::applySceneData(")
    negatives.append(("scene load hook missing", not sb.lstrip("{\n ").startswith("ALCloneTransitions::onSceneReplaced();"), ""))

    # ---- fix round 1 negatives ----
    def neg_r1(label, key, old, new):
        ff = dict(files)
        ff[key] = mutate(files[key], old, new)
        negatives.append((label, bool(check_fix_r1(ff)), ""))

    neg_r1("M1: running state published after the callback", "manager",
           "            sRunning = mImpl->mMachine.running();\n            // The reverse kind needs", "            // The reverse kind needs")
    neg_r1("M2: duplicate start not rejected before mExtras", "manager",
           "    if (mImpl->mMachine.find(instance_id)) { why = \"This clone already has a transition running; Cancel it first\"; return false; }\n", "")
    neg_r1("M2: Go stays enabled while running", "panel", "setEnabledIfChanged(mTransitionGoBtn, have && !active);", "setEnabledIfChanged(mTransitionGoBtn, have);")
    neg_r1("M6: start accepts a clone with visible system parts", "manager",
           "        if (request.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK && ALCloneTransitions::systemPartsVisible(ghost))\n        { why = SYSTEM_PARTS_REFUSAL; return false; }\n", "")
    neg_r1("M6: commit does not re-test the system parts", "manager",
           "if (r.kind == ALCloneTransition::KIND_ENTITY_TO_OVERLAY_LOOK && ALCloneTransitions::systemPartsVisible(ghost)) return false;", "")
    neg_r1("M7: setLockMode cancels only one member", "studio",
           "    cancelGroupTransitions(id); // [CloneTransition] group edit (D18): snapshot + cancel every member BEFORE the mode changes\n",
           "    ALCloneTransitions::cancelFor(id, ALCloneTransition::R_EDIT_LOCK);\n")
    neg_r1("M8: crowd look assignment does not cancel", "studio",
           "        for (const LLUUID& id : affected) ALCloneTransitions::cancelFor(id, ALCloneTransition::R_EDIT_LOCK);\n", "")
    neg_r1("S6: idle scan counter missing", "studio_h", "    S32 mOverlayLookCount = 0;", "    S32 mOverlayLookCountX = 0;")
    neg_r1("S2: bodyHeight never derived", "manager", "        params.bodyHeight = height;\n", "")
    native_negative("kind range check removed (native test 15)",
                    "        if (rq.kind < KIND_LOOK_TO_LOOK || rq.kind > KIND_OVERLAY_LOOK_TO_ENTITY) { why = \"Unknown transition kind\"; return 0; }\n", "")
    native_negative("M1: onStarted runs before the record is visible (native test 14)",
                    "        mRecords.push_back(r);\n        mHost.onStarted(mRecords.back());",
                    "        mHost.onStarted(r);\n        mRecords.push_back(r);")
    native_negative("M2: duplicate start overwrites the running record (native test 15)",
                    "        if (find(clone)) { why = \"This clone already has a transition running; Cancel it first\"; return 0; }\n", "")

    # ---- fix round 2 negatives ----
    def neg_r2(label, key, old, new):
        ff = dict(files)
        ff[key] = mutate(files[key], old, new)
        negatives.append((label, bool(check_fix_r2(ff, tree)), ""))

    def counter_negative(label, old, new):
        src = counter_fixture(mutate(files["studio"], old, new))
        okn, outn = compile_run("neg_counter", src) if src else (True, "")
        negatives.append((label, not okn, outn.splitlines()[0] if outn else ""))

    counter_negative("N2: whole-record copy without accounting (the review's duplicate bug)",
                     "    accountOverlayLook(before, dst.mOverlayLook);\n", "")
    counter_negative("N2: insertion without accounting", "    accountOverlayLook(-1, inst.mOverlayLook);\n", "")
    counter_negative("N2: field write without accounting", "    accountOverlayLook(inst.mOverlayLook, look);\n", "")
    neg_r2("N2: duplicate copies the record raw", "studio", "    assignRecord(*copy, proto); // keeps mOverlayLookCount exact: the duplicate keeps the prototype's overlay look",
           "    *copy = proto;")
    neg_r2("N2: a raw push_back bypasses the boundary", "studio", "    mInstances.push_back(inst);\n    accountOverlayLook(-1, inst.mOverlayLook);",
           "    mInstances.push_back(inst);\n    mInstances.push_back(inst);")
    neg_r2("N2: removeInstance does not account", "studio", "accountOverlayLook(retiring->mOverlayLook, -1); // [CloneTransition]", "(void)retiring; // [CloneTransition]")
    neg_r2("Q1: periodic validation removed from updatePerFrame", "studio", "            if (!ALCloneTransitions::instance().isRunning(id)) dropUnsupportedOverlayLook(id);\n", "")
    tree_q1 = dict(tree)
    tree_q1["llactormover.cpp"] = mutate(tree["llactormover.cpp"], "        // N3: the periodic system-part validation of a steady overlay look runs BEFORE rendering, in\n",
                                         "        if (studio.dropUnsupportedOverlayLook(inst.mId)) continue;\n        // N3: the periodic system-part validation of a steady overlay look runs BEFORE rendering, in\n")
    negatives.append(("Q1: periodic validation moved back into the render loop", bool(check_fix_r2(files, tree_q1)), ""))
    neg_r2("Q2: removeAll does not reset the counter", "studio", "    mOverlayLookCount = 0; // [CloneTransition] wholesale clear", "    // [CloneTransition] wholesale clear")
    neg_r2("Q2: a wholesale list clear bypasses the boundary", "studio", "    mOverlayHistory.clear();\n    mSolo = false;\n    while (!mInstances.empty())",
           "    mOverlayHistory.clear();\n    mSolo = false;\n    mInstances.clear();\n    while (!mInstances.empty())")
    neg_r2("N3: replacement body not validated before the hide", "studio", "    dropUnsupportedOverlayLookOf(inst, ghost);\n", "")
    neg_r2("N3: validation does not drop the look", "studio", "    setOverlayLookField(inst, -1);\n    ALCloneTransitions::setNote(", "    ALCloneTransitions::setNote(")
    neg_r2("N3: validation runs after the hide is applied", "studio",
           "    dropUnsupportedOverlayLookOf(inst, ghost);\n    set_entity_clone_visible(ghost, isVisible(inst), entityRenderOnly(inst));\n",
           "    set_entity_clone_visible(ghost, isVisible(inst), entityRenderOnly(inst));\n    dropUnsupportedOverlayLookOf(inst, ghost);\n")
    neg_r2("T1: the manager clock ignores the frozen world", "manager", 'ALCloneTransition::clockDelta(LLPipeline::FreezeTime,',
           'ALCloneTransition::clockDelta(false,')
    neg_r2("T2: BOM-hidden head ignored by the system-part rule", "manager", "in.bomHead = bom_hides(avatar, IMG_USE_BAKED_HEAD);", "")
    neg_r2("T2: invisible-appearance test dropped", "manager", "in.invisibleAppearance = avatar->getOverallAppearance() == LLVOAvatar::AOA_INVISIBLE;", "")
    neg_r2("T4: the dead duplicate find is back", "manager", "    if (mImpl->mMachine.running() >= ALCloneTransition::MAX_RUNNING)\n",
           "    if (mImpl->mMachine.running() >= ALCloneTransition::MAX_RUNNING && !mImpl->mMachine.find(instance_id))\n")
    native_negative("T1: a frozen world still advances the clock (native test 16)", "    if (worldFrozen) return 0.0;\n", "")
    native_negative("T1: stalled-frame cap removed (native test 16)", "std::min(raw, 0.25) : 0.0;", "raw : 0.0;")
    native_negative("T2: BOM-hidden meshes still counted as drawn (native test 17)", "(in.upperBake && !in.bomUpper)", "(in.upperBake)")
    native_negative("T2: an alpha-masked body counted as drawn (native test 17)", "(in.headBake && !in.bomHead)", "(true && !in.bomHead)")
    native_negative("T2: invisible appearance still draws (native test 17)", "    if (in.invisibleAppearance) return false;\n", "")
    native_negative("T2: skirt without the wearable draws (native test 17)", "(in.skirtWorn && in.skirtBake && !in.bomSkirt)", "(in.skirtBake && !in.bomSkirt)")

    for label, failed_as_planted, detail in negatives:
        print(("PASS " if failed_as_planted else "FAIL ") + "planted negative fails as planted: " + label)
        if not failed_as_planted:
            failures.append("planted negative did NOT fail: " + label)

    if failures:
        print("\nclone_transition_engine_check: FAIL")
        for f in failures:
            print(" - " + f)
        return 1
    print("\nclone_transition_engine_check: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
