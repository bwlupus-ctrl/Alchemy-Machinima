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
    ("R_EDIT_LOCK (ungroup)", "studio", "ALGhostStudio::ungroup(", "ALCloneTransition::R_EDIT_LOCK"),
    ("R_EDIT_LOCK (lock mode)", "studio", "ALGhostStudio::setLockMode(", "ALCloneTransition::R_EDIT_LOCK"),
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
    if not native or not layers:
        return ["nativeParamsFor / studioLayersFor not found in alclonetransitions.cpp"]
    # native half: OUTGOING only for entity -> overlay look, otherwise INCOMING
    if not re.search(r"KIND_ENTITY_TO_OVERLAY_LOOK\s*\?\s*ALCloneTransition::SIDE_OUTGOING\s*:\s*ALCloneTransition::SIDE_INCOMING", native):
        problems.append("nativeParamsFor: native half must be SIDE_OUTGOING for entity->overlay look and SIDE_INCOMING for the reverse kind")

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


def load_files() -> tuple[dict[str, str], dict[str, str]]:
    xui = NV / "skins/default/xui/en"
    files = {
        "studio": read(NV / "alghoststudio.cpp"),
        "actions": read(NV / "alghostcloneactions.cpp"),
        "actions_h": read(NV / "alghostcloneactions.h"),
        "panel": read(NV / "alpanelghoststudio.cpp"),
        "viewer": read(NV / "llappviewer.cpp"),
        "alloff": read(NV / "aldirectoralloff.cpp"),
        "manager": read(NV / "alclonetransitions.cpp"),
        "manager_h": read(NV / "alclonetransitions.h"),
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
            + check_ui(files) + check_contract(files) + check_seams(files, tree))


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
    f7["manager"] = mutate(files["manager"], "KIND_ENTITY_TO_OVERLAY_LOOK\n            ? ALCloneTransition::SIDE_OUTGOING : ALCloneTransition::SIDE_INCOMING",
                           "KIND_ENTITY_TO_OVERLAY_LOOK\n            ? ALCloneTransition::SIDE_INCOMING : ALCloneTransition::SIDE_OUTGOING")
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
