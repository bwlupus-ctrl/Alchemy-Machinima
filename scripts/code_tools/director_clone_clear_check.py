#!/usr/bin/env python3
"""Check the "Clear animations" wiring for entity clones.

Usage::

    python scripts/code_tools/director_clone_clear_check.py --check
    python scripts/code_tools/director_clone_clear_check.py --selftest

A cleared clone (Directed drive, no animation, neutral pose) must not get a motion
of its own started again by anything that runs by itself. The pure rules live in
indra/newview/alghostclearanim.h; this gate pins the production sites to them
(exit 1 on any finding):
- LLGhostAvatar: motion cues, eye and physics starts go through
  ALGhostClearAnim::mayStart; Clear stops every active motion except the Poser's
  and rests the skeleton; only a drive change ends the cleared state.
- LLGhostAvatar: no unreviewed LLCharacter::startMotion call. Every call is either
  gated by mayStart or is a drive-owned start (mirrored / directed / AutoAnimate
  animation), which cannot run in the cleared drive state.
- ALGhostStudio: Clear switches AutoAnimate, the walk and authored gaze off before
  the drive, snapshots what undo restores, and a respawn re-applies the state.
- FSFloaterPoser: releasing a posed clone does not restore the earlier drive
  while the clone is cleared.
- ALPanelGhostStudio: the button records one undo step and toasts the count; the
  generated Clones page has the row.

Standard library only. Exit 2 is for command-line or missing-input errors.
"""

from __future__ import annotations

import argparse
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
NEWVIEW = os.path.join(REPO, "indra", "newview")
PAGE = "skins/default/xui/en/panel_dpage_cast_ghosts.xml"
FILES = ("llghostavatar.cpp", "alghoststudio.cpp", "alpanelghoststudio.cpp", "fsfloaterposer.cpp",
         "alghostclearanim.h", PAGE)

# Reviewed LLCharacter::startMotion calls in llghostavatar.cpp, by enclosing function.
# "gated": the call must sit behind ALGhostClearAnim::mayStart in that function.
# "drive": started by the drive itself (mirror ledger, directed animation, AutoAnimate
# body step, held animesh on a control avatar), unreachable in Directed with no animation.
START_SITES = {
    "setFormationAnimations": ("gated", 2),
    "startEntityLayerMotions": ("gated", 2),
    "setEntityEyeMotionEnabled": ("gated", 2),
    "setEntityPhysicsEnabled": ("gated", 2),
    "synchronizeCloneAnimations": ("drive", 1),
    "setEntityDriveMode": ("drive", 1),
    "restartEntityAnimation": ("drive", 1),
    "repeatHeldAnimesh": ("drive", 1),
    "idleUpdate": ("drive", 2),
}

PINS = {
    "llghostavatar.cpp": (
        ("(mEntityDriveMode == ALGhostStudio::DRIVE_MIRROR || mEntityDriveMode == ALGhostStudio::DRIVE_DIRECTED) && "
         "ALGhostClearAnim::mayStart(mEntityAnimCleared, ALGhostClearAnim::OWNER_CUE);",
         "motion cues are not gated by the cleared state"),
        ("if (!ALGhostClearAnim::canHold(mEntityDriveMode, mEntityDirectedAnim.notNull())) { return; }",
         "Clear no longer requires Directed with no animation"),
        ("mEntityAnimCleared = true; setFormationAnimations(std::vector<LLUUID>());",
         "Clear no longer releases the motion cues"),
        ("for (LLMotion* motion : getMotionController().getActiveMotions()) { if (motion && "
         "ALGhostClearAnim::stopsOnClear(dynamic_cast<FSPosingMotion*>(motion) != nullptr)) "
         "{ running.push_back(motion->getID()); } }",
         "Clear no longer collects every active motion except the Poser's"),
        ("for (const LLUUID& id : running) { autoAnimAuditBodyStop(id); LLCharacter::stopMotion(id, true); }",
         "Clear no longer stops the collected motions immediately"),
        ("for (LLJoint* joint : mSkeleton) { if (joint) { joint->setRotation(LLQuaternion::DEFAULT); } } "
         "if (mPelvisp) { mPelvisp->setPosition(LLVector3::zero); }",
         "Clear no longer rests the skeleton in the neutral pose"),
        ("if (!ALGhostClearAnim::holds(mEntityAnimCleared, mode, directed_anim.notNull())) "
         "{ setEntityAnimationsCleared(false); } autoAnimOnDriveModeChanged(old_mode, mode);",
         "a drive change no longer ends the cleared state through the pure rule"),
    ),
    "alghoststudio.cpp": (
        ("if (inst->mAutoAnimate) { setInstanceAutoAnimate(id, false); } if (mover.isMoving(actor)) { mover.stop(actor); } "
         "if (mover.isGazeEnabled(actor)) { mover.setGazeEnabled(actor, false); } inst->mPendingFreezeFrames = 0; "
         "if (!setInstanceDriveMode(id, DRIVE_DIRECTED, LLUUID::null)) { return false; }",
         "Clear no longer switches AutoAnimate, the walk and gaze off before the drive"),
        ("ghost->setEntityAnimationsCleared(true); inst->mAnimCleared = ghost->isEntityAnimationsCleared();",
         "Clear no longer records the clone's real cleared state"),
        ("before->mMode = inst->mDriveMode; before->mAnim = inst->mDirectedAnim; before->mAutoAnimate = inst->mAutoAnimate; "
         "before->mGazeEnabled = mover.isGazeEnabled(actor); before->mWasCleared = inst->mAnimCleared;",
         "Clear no longer snapshots drive, animation, AutoAnimate and gaze for undo"),
        ("if (!ALGhostClearAnim::clearedAfterUndo(rule)) { inst->mAnimCleared = false; ghost->setEntityAnimationsCleared(false); } "
         "if (!setInstanceDriveMode(before.mId, before.mMode, before.mAnim)) { return false; } "
         "if (before.mAutoAnimate != inst->mAutoAnimate) { setInstanceAutoAnimate(before.mId, before.mAutoAnimate); }",
         "undo no longer restores drive, animation and AutoAnimate"),
        ("if (before.mGazeEnabled != mover.isGazeEnabled(ghost->getID())) { mover.setGazeEnabled(ghost->getID(), before.mGazeEnabled); }",
         "undo no longer restores gaze"),
        ("inst.mAnimCleared = ALGhostClearAnim::holds( inst.mAnimCleared, inst.mDriveMode, inst.mDirectedAnim.notNull()); "
         "ghost->setEntityAnimationsCleared(inst.mAnimCleared);",
         "a respawned clone body no longer re-applies the cleared state"),
        ("if (inst->mAnimCleared != ALGhostClearAnim::holds(inst->mAnimCleared, mode, next_anim.notNull())) { inst->mAnimCleared = false;",
         "setInstanceDriveMode no longer ends the cleared state"),
    ),
    "fsfloaterposer.cpp": (
        ("if (ghost && !ghost->isDead() && !ghost->isEntityAnimationsCleared() && "
         "ghost->getEntityDriveMode() == ALGhostStudio::DRIVE_DIRECTED && ghost->getEntityDirectedAnim().isNull()) "
         "{ ghost->setEntityDriveMode(saved->second.first, saved->second.second); }",
         "Poser release restores the earlier drive on a cleared clone"),
    ),
    "alpanelghoststudio.cpp": (
        ("if (studio.clearInstanceAnimations(id, &before)) { snapshot->push_back(before); }",
         "the button no longer snapshots each cleared clone"),
        ('shell->history().recordCustom(host, "clone_clear_anim", "Clear animations on clones",',
         "the button no longer records one undo step"),
        ("any = (redo ? studio.clearInstanceAnimations(before.mId) : studio.restoreInstanceAnimations(before)) || any;",
         "the undo step no longer restores / re-clears every clone"),
        ("ALGhostClearAnim::clearedToast((size_t)cleared)", "the count toast is gone"),
        ("setEnabledIfChanged(mClearAnimBtn, have_selected_entity);", "the button is no longer entity-clone only"),
        ("ALGhostClearAnim::buttonTooltip() : ALGhostClearAnim::overlayTooltip()", "the overlay-clone tooltip is gone"),
    ),
    "alghostclearanim.h": (
        ("return !cleared || owner == OWNER_POSER;", "mayStart no longer blocks a cleared clone"),
        ("return drive == kDriveDirected && !has_directed_anim;", "canHold changed"),
        ('return "Cleared animations on " + std::to_string(count) + " clone(s)";', "toast text changed"),
    ),
    PAGE: (
        ('kind="button" label="Clear" widget="btn_clear_animations"', "the Clones page has no Clear animations row"),
        ('<button name="btn_clear_animations" label="Clear animations"', "the Clear animations button is missing"),
    ),
}


def strip_comments(source):
    source = re.sub(r"/\*.*?\*/", " ", source, flags=re.S)
    return re.sub(r"//[^\n]*", " ", source)


def normalise(source):
    return re.sub(r"\s+", " ", strip_comments(source)).strip()


def functions(source, cls):
    """{name: body} for every `cls::name(...) {` definition (comments stripped)."""
    source = strip_comments(source)
    out = {}
    for match in re.finditer(rf"\b{cls}::(\w+)\s*\(", source):
        brace = source.find("{", match.end())
        semi = source.find(";", match.end())
        if brace < 0 or (0 <= semi < brace):
            continue
        depth, index = 0, brace
        while index < len(source):
            depth += {"{": 1, "}": -1}.get(source[index], 0)
            if depth == 0:
                break
            index += 1
        out[match.group(1)] = out.get(match.group(1), "") + source[brace:index + 1]
    return out


def findings(sources):
    out = []
    for name in FILES:
        if name not in sources:
            out.append(f"{name}: missing")
    for name, pins in PINS.items():
        text = sources.get(name)
        if text is None:
            continue
        plain = normalise(text) if not name.endswith(".xml") else text
        for needle, message in pins:
            if (normalise(needle) if not name.endswith(".xml") else needle) not in plain:
                out.append(f"{name}: {message}")
    ghost = sources.get("llghostavatar.cpp")
    if ghost is not None:
        seen = {}
        for name, body in functions(ghost, "LLGhostAvatar").items():
            count = len(re.findall(r"LLCharacter::startMotion\s*\(", body))
            if count:
                seen[name] = (count, body)
        for name, (count, body) in sorted(seen.items()):
            site = START_SITES.get(name)
            if site is None:
                out.append(f"llghostavatar.cpp: unreviewed startMotion in {name}() (START_SITES)")
            elif count != site[1]:
                out.append(f"llghostavatar.cpp: {name}() has {count} startMotion call(s), reviewed {site[1]}")
            elif site[0] == "gated" and "ALGhostClearAnim::mayStart(mEntityAnimCleared" not in body:
                out.append(f"llghostavatar.cpp: {name}() starts a motion without the cleared-state gate")
        for name in sorted(set(START_SITES) - set(seen)):
            out.append(f"llghostavatar.cpp: reviewed start site {name}() no longer starts a motion")
        if re.search(r"(?<!LLCharacter::)\bstartMotion\s*\(", "".join(body for _c, body in seen.values())):
            out.append("llghostavatar.cpp: an unqualified startMotion call escapes the review")
        for name, owner in (("setEntityEyeMotionEnabled", "OWNER_EYE"), ("setEntityPhysicsEnabled", "OWNER_PHYSICS")):
            body = normalise(seen.get(name, (0, ""))[1])
            starts = len(re.findall(r"LLCharacter::startMotion\(", body))
            guarded = len(re.findall(r"may_start[^;{}]*\)? ?\{? ?LLCharacter::startMotion\(|may_start\) LLCharacter::startMotion\(|"
                                     r"may_start && !isMotionActive\([A-Z_]+\)\) \{ LLCharacter::startMotion\(", body))
            if f"ALGhostClearAnim::{owner})" not in body or guarded != starts:
                out.append(f"llghostavatar.cpp: {name}() has a start that is not behind may_start")
    return out


def load_sources():
    sources = {}
    for name in FILES:
        path = os.path.join(NEWVIEW, name)
        if os.path.isfile(path):
            with open(path, encoding="utf-8", errors="replace") as fh:
                sources[name] = fh.read()
    return sources


def run_check():
    sources = load_sources()
    if "llghostavatar.cpp" not in sources:
        print("llghostavatar.cpp not found", file=sys.stderr)
        return 2
    found = findings(sources)
    for item in found:
        print(f"FAIL  {item}")
    print(f"Clear animations wiring: {len(found)} finding(s)")
    return 1 if found else 0


def selftest():
    sources = load_sources()
    failures = []
    if findings(sources):
        failures.append(f"production sources fail: {findings(sources)}")

    def swap(name, old, new):
        return dict(sources, **{name: sources.get(name, "").replace(old, new)})

    G, S, P, F = "llghostavatar.cpp", "alghoststudio.cpp", "alpanelghoststudio.cpp", "fsfloaterposer.cpp"
    mutations = [
        ("motion cue restarts on a cleared clone",
         swap(G, " &&\n        ALGhostClearAnim::mayStart(mEntityAnimCleared, ALGhostClearAnim::OWNER_CUE);", ";")),
        ("eye motion restarts on a cleared clone",
         swap(G, "        if (may_start) LLCharacter::startMotion(ANIM_AGENT_EYE);", "        LLCharacter::startMotion(ANIM_AGENT_EYE);")),
        ("physics motion restarts on a cleared clone",
         swap(G, "        if (enabled && mEntityCloneVisible && may_start &&\n            !isMotionActive(ANIM_AGENT_PHYSICS_MOTION))",
              "        if (enabled && mEntityCloneVisible &&\n            !isMotionActive(ANIM_AGENT_PHYSICS_MOTION))")),
        ("showing a hidden cleared clone restarts its layers",
         swap(G, "        startEntityLayerMotions(); // [ClearAnim] gated: a cleared clone starts none",
              "        LLCharacter::startMotion(ANIM_AGENT_PHYSICS_MOTION);")),
        ("Clear leaves motions running",
         swap(G, "        LLCharacter::stopMotion(id, true);\n    }\n    mEntityDirectedStarted = false;", "    }\n    mEntityDirectedStarted = false;")),
        ("Clear stops the Poser's motion too",
         swap(G, "ALGhostClearAnim::stopsOnClear(dynamic_cast<FSPosingMotion*>(motion) != nullptr)", "true")),
        ("Clear keeps the last animation pose",
         swap(G, "            joint->setRotation(LLQuaternion::DEFAULT);", "")),
        ("cleared state survives a new animation",
         swap(G, "    if (!ALGhostClearAnim::holds(mEntityAnimCleared, mode, directed_anim.notNull()))\n    {\n        setEntityAnimationsCleared(false);\n    }\n", "")),
        ("new unreviewed start site",
         swap(G, "void LLGhostAvatar::setEntityLook(S32 look, F32 alpha)\n{",
              "void LLGhostAvatar::setEntityLook(S32 look, F32 alpha)\n{\n    LLCharacter::startMotion(ANIM_AGENT_STAND);")),
        ("AutoAnimate left on by Clear",
         swap(S, "    if (inst->mAutoAnimate)\n    {\n        setInstanceAutoAnimate(id, false);\n    }\n    if (mover.isMoving(actor))", "    if (mover.isMoving(actor))")),
        ("gaze left on by Clear",
         swap(S, "        mover.setGazeEnabled(actor, false); // head / eye look-at layer and its eye motion", "")),
        ("undo forgets AutoAnimate",
         swap(S, "        setInstanceAutoAnimate(before.mId, before.mAutoAnimate);", "")),
        ("undo leaves the clone cleared",
         swap(S, "        inst->mAnimCleared = false;\n        ghost->setEntityAnimationsCleared(false);\n    }\n    if (!setInstanceDriveMode(before.mId",
              "    }\n    if (!setInstanceDriveMode(before.mId")),
        ("respawn drops the cleared state",
         swap(S, "    ghost->setEntityAnimationsCleared(inst.mAnimCleared);\n", "")),
        ("Poser release restarts the earlier drive",
         swap(F, "        !ghost->isEntityAnimationsCleared() &&\n", "")),
        ("no undo step",
         swap(P, 'shell->history().recordCustom(host, "clone_clear_anim"', 'shell->history().barrier("clone_clear_anim"); //')),
        ("no toast", swap(P, "ALGhostClearAnim::clearedToast((size_t)cleared)", 'std::string("done")')),
        ("mayStart always true", swap("alghostclearanim.h", "return !cleared || owner == OWNER_POSER;", "return true;")),
        ("kit row removed", swap(PAGE, 'widget="btn_clear_animations"', 'widget="btn_other"')),
    ]
    for label, mutated in mutations:
        if mutated == sources:
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
    parser.add_argument("--check", action="store_true", help="check the Clear animations wiring")
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
