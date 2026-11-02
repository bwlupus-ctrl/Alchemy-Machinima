"""No-build numeric oracle + source contracts; C++ runtime tests are separate.

Run: python indra/newview/tests/alghostbounds_check.py
This does not execute viewer code or require a viewer build/login.
"""
from pathlib import Path
import math
import random
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[3]
VIEW = ROOT / "indra/newview"
HEADER = (VIEW / "alghostbounds.h").read_text(encoding="utf-8")


def threshold(name):
    return float(re.search(rf"{name} = ([.0-9]+)f;", HEADER)[1])


def displaced(previous, seated, center, root):
    # Independent numeric oracle for the production rule. Constants are read
    # from the header; source contracts below pin the production expressions.
    limit = threshold("DISPLACED_LEAVE" if previous else "DISPLACED_ENTER")
    return seated or math.hypot(center[0] - root[0], center[1] - root[1]) > limit


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def rotate(v, axis, angle):
    # Rodrigues, independently checking yaw, pitch and roll.
    c, s = math.cos(angle), math.sin(angle)
    cross = (axis[1]*v[2]-axis[2]*v[1], axis[2]*v[0]-axis[0]*v[2], axis[0]*v[1]-axis[1]*v[0])
    dot = sum(x*y for x, y in zip(axis, v))
    return tuple(v[i]*c + cross[i]*s + axis[i]*dot*(1-c) for i in range(3))


class BoundsCheck(unittest.TestCase):
    def test_production_contract(self):
        compact = " ".join(HEADER.split())
        for expression in (
            "x = center[0] - root[0]", "y = center[1] - root[1]",
            "previous ? DISPLACED_LEAVE : DISPLACED_ENTER",
            "return seated || x * x + y * y > threshold * threshold;",
            "c[axis] = minimum[axis] * .5f + maximum[axis] * .5f;",
            "s[axis] = maximum[axis] - minimum[axis];",
        ):
            self.assertIn(expression, compact)
        ghost = (VIEW / "llghostavatar.cpp").read_text(encoding="utf-8")
        self.assertIn("if (!mUseVisualBounds || !isBuilt()", ghost)
        # Shared core file is byte-for-byte unchanged. The other has exactly
        # two RTTI-gated query additions, retaining the complete old fallback.
        base = subprocess.check_output(["git", "show", "HEAD:indra/newview/llvovolume.cpp"], cwd=ROOT)
        self.assertEqual(base.decode().replace("\r\n", "\n"), (VIEW / "llvovolume.cpp").read_text(encoding="utf-8"))
        current = (VIEW / "llviewerobject.cpp").read_text(encoding="utf-8")
        old = subprocess.check_output(["git", "show", "HEAD:indra/newview/llviewerobject.cpp"], cwd=ROOT).decode()
        for signature in ("LLVector3 LLViewerObject::getManipPivotPositionAgent() const", "LLBBox LLViewerObject::getBoundingBoxAgent() const"):
            start = current.index(signature)
            gate_start = current.index("    if (const auto* ghost = dynamic_cast<const LLGhostAvatar*>(this))", start)
            gate_end = current.index("\n    }", gate_start) + len("\n    }\n")
            current = current[:gate_start] + current[gate_end:]
        self.assertEqual(old.replace("\r\n", "\n"), current)

    def test_seated_and_standing(self):
        minimum, maximum = (10.35, 19.7, .45), (11.25, 20.3, 1.85)
        center = tuple((a+b)/2 for a, b in zip(minimum, maximum))
        self.assertAlmostEqual(center[0], 10.8)
        self.assertAlmostEqual(center[2], 1.15)
        self.assertTrue(displaced(False, True, center, (10, 20, 1)))
        self.assertTrue(displaced(False, False, center, (10, 20, 1)))
        self.assertFalse(displaced(False, False, (.05, 0, 1), (0, 0, 1)))
        self.assertTrue(displaced(False, True, (0, 0, .3), (0, 0, 1)))

    def test_forced_refresh_gate(self):
        ghost = (VIEW / "llghostavatar.cpp").read_text(encoding="utf-8")
        refresh = ghost.split("void LLGhostAvatar::refreshVisualBounds(bool force_refresh)", 1)[1].split(
            "void LLGhostAvatar::ghostSlamPosition", 1)[0]
        header = (VIEW / "llghostavatar.h").read_text(encoding="utf-8")
        self.assertIn("void refreshVisualBounds(bool force_refresh = false);", header)
        gate = "if (selected || fitted || force_refresh)"
        start = refresh.index(gate)
        end = refresh.index("\n    }", start)
        forced = refresh[start:end]
        for operation in ("mRoot->updateWorldMatrixChildren();", "setNeedsExtentUpdate(true);",
                          "mDrawable->updateSpatialExtents();", "mDrawable->movePartition();"):
            self.assertEqual(refresh.count(operation), 1)
            self.assertIn(operation, forced)
        self.assertLess(refresh.index("classify();"), start)
        self.assertIn("const bool fitted = previous_mode || mUseVisualBounds || seated;", refresh)
        self.assertIn("mUseVisualBounds = previous_mode;\n        classify();", forced)
        for selection in ("getSelection()->contains(this)",
                          "studio.getSelected() == mClientHeightKey",
                          "group && !group->mEditMembers",
                          "studio.groupForMember(studio.getSelected()) == group"):
            self.assertIn(selection, refresh)
        studio = (VIEW / "alghoststudio.cpp").read_text(encoding="utf-8")
        # Exactly one event-driven bypass; the per-frame pass uses the default
        # false, so its forced path is selected || fitted, including entry.
        calls = re.findall(r"ghost->refreshVisualBounds\((.*?)\);", studio)
        self.assertEqual(calls, ["true", ""])
        event = studio.split("bool ALGhostStudio::applyEntityTransform(", 1)[1].split(
            "bool ALGhostStudio::aimInstanceAt", 1)[0]
        self.assertIn("ghost->refreshVisualBounds(true);", event)
        for selected in (False, True):
            for prior in (False, True):
                for seated in (False, True):
                    for offset in (.05, .18, .21):
                        entered = displaced(prior, seated, (offset, 0, 0), (0, 0, 0))
                        fitted = prior or entered or seated
                        forced_now = selected or fitted  # per-frame force_refresh=false
                        self.assertEqual(forced_now, selected or prior or entered or seated)
                        if selected or (entered and not prior):
                            self.assertTrue(forced_now)

    def test_hysteresis(self):
        for prior in (False, True):
            self.assertEqual(displaced(prior, False, (.18, 0, 0), (0, 0, 0)), prior)
        self.assertTrue(displaced(False, False, (.201, 0, 0), (0, 0, 0)))
        self.assertFalse(displaced(True, False, (.149, 0, 0), (0, 0, 0)))

    def test_drag_algebra(self):
        rng = random.Random(20261102)
        for _ in range(1000):
            foot = tuple(rng.uniform(-200, 200) for _ in range(3))
            lift = (0, 0, rng.uniform(.2, 4))
            offset = tuple(rng.uniform(-3, 3) for _ in range(3))
            delta = tuple(rng.uniform(-10, 10) for _ in range(3))
            center = add(foot, add(lift, offset))
            moved_foot = sub(add(center, delta), add(lift, offset))
            self.assertLess(math.dist(moved_foot, add(foot, delta)), 1e-10)
            axis = rng.choice(((1, 0, 0), (0, 1, 0), (0, 0, 1)))
            angle = rng.uniform(-math.pi, math.pi)
            # Production inverse-centre conversion, then world-Z lift correction.
            rotated_foot = add(sub(center, rotate(add(lift, offset), axis, angle)), sub(rotate(lift, axis, angle), lift))
            result_center = add(rotated_foot, add(lift, rotate(offset, axis, angle)))
            self.assertLess(math.dist(result_center, center), 1e-10)


if __name__ == "__main__":
    unittest.main(verbosity=2)
