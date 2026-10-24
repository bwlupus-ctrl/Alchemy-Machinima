#!/usr/bin/env python3
"""Source-derived heading math and static integration checks; no viewer build.

Run with PYTHONUTF8=1: python scripts/code_tools/test_my_heading.py
Evaluate the actual read/write expressions, then check the agent frame API,
commit-time ownership guards, normal network path and shared clone dial kit.
"""
import math
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding='utf-8')


def function(source, signature):
    start = source.index(signature)
    brace = source.index('\n{', start) + 1
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[brace + 1:end - 1]


PANEL = read('indra/newview/alpanelactormover.cpp')
REFRESH = function(PANEL, 'void ALPanelActorMover::refreshMyHeading()')
COMMIT = function(PANEL, 'void ALPanelActorMover::onMyHeadingCommit()')
GUARD = function(PANEL, 'const char* ALPanelActorMover::myHeadingDisabledReason() const')
DIAL = read('indra/llui/alcompassdial.cpp')
CONSTANTS = {'RAD_TO_DEG': 180 / math.pi, 'DEG_TO_RAD': math.pi / 180,
             'atan2f': math.atan2, 'cosf': math.cos, 'sinf': math.sin,
             'fmodf': math.fmod, 'VX': 0, 'VY': 1}


def evaluate(expression, **values):
    # Only repository expressions with numeric constants and the named math functions.
    expression = re.sub(r'(?<=\d)\.f\b', '.0', expression)
    expression = expression.replace('at.mV', 'at')
    return eval(expression, {'__builtins__': {}}, CONSTANTS | values)


def heading(at):
    expr = re.search(r'const F32 degrees = (.*);', REFRESH)[1]
    return evaluate(expr, at=at)


def look(degrees):
    expr = re.search(r'const F32 radians = (.*);', COMMIT)[1]
    expr = expr.replace('(F32)mMyHeadingDial->getValue().asReal()', 'degrees')
    radians = evaluate(expr, degrees=degrees)
    args = re.search(r'gAgent.resetAxes\(LLVector3\((.*)\)\);', COMMIT)[1]
    return evaluate('(' + args + ')', radians=radians)


class MyHeadingTest(unittest.TestCase):
    def test_frame_yaw_and_upright_roundtrip(self):
        self.assertIn('gAgent.getFrameAgent().getAtAxis()', REFRESH)
        for degrees in range(-1080, 1081):
            at = look(degrees)
            self.assertAlmostEqual(sum(v*v for v in at), 1)
            self.assertEqual(at[2], 0)  # removes any prior body pitch / roll
            actual = heading(at)
            self.assertLessEqual(abs(actual), 180)
            self.assertAlmostEqual((actual - degrees + 180) % 360, 180, places=8)
            # Pitch in the input frame does not alter the projected world yaw.
            self.assertAlmostEqual(heading((at[0]*.3, at[1]*.3, .95)), actual)
        for at, expected in [((1, 0, 0), 0), ((0, 1, 0), 90),
                             ((-1, 0, 0), 180), ((0, -1, 0), -90)]:
            self.assertAlmostEqual(heading(at), expected)

    def test_agent_axes_and_network_path(self):
        agent = read('indra/newview/llagent.cpp')
        reset = function(agent, 'void LLAgent::resetAxes(const LLVector3 &look_at)')
        self.assertIn('LLVector3 left(skyward % look_at);', reset)
        self.assertIn('LLVector3 up(look_at % left);', reset)
        self.assertIn('mFrameAgent.setAxes(look_at, left, up);', reset)
        self.assertIn('LLVector3 up_vector = LLVector3::z_axis;', agent)
        network = read('indra/newview/llviewermessage.cpp')
        self.assertIn('body_rotation = gAgent.getFrameAgent().getQuaternion()', network)
        self.assertIn('addQuatFast(_PREHASH_BodyRotation, body_rotation)', network)
        self.assertNotIn('gAgentCamera.', COMMIT)  # camera follows the ordinary agent frame

    def test_commit_guards_seated_and_owned(self):
        check = COMMIT.index('if (!mMyHeadingDial || myHeadingDisabledReason()) return;')
        self.assertLess(check, COMMIT.index('gAgent.resetAxes'))
        for condition in ['!isAgentAvatarValid()', 'gAgent.isSitting()',
                          'gAgentCamera.cameraMouselook()',
                          'LLActorMover::instance().isDriving(gAgent.getID())',
                          'poser.isPosingAvatar(gAgentAvatarp)', 'gAgent.rotateGrabbed()']:
            self.assertRegex(GUARD, r'if \(' + re.escape(condition) + r'\)\s*return "')
        self.assertIn('Not available while seated: the seat owns', GUARD)
        self.assertIn('setEnabledIfChanged(mMyHeadingDial, reason == nullptr)', REFRESH)

    def test_live_refresh(self):
        draw = function(PANEL, 'void ALPanelActorMover::draw()')
        self.assertIn('isWidgetOnScreen(mMyHeadingDial) && mMyHeadingClock.due()', draw)
        self.assertIn('ALLiveReadoutClock mMyHeadingClock{10.f}',
                      read('indra/newview/alpanelactormover.h'))
        self.assertIn('!mMyHeadingDial->hasMouseCapture()', REFRESH)
        self.assertIn('setValueIfChanged(mMyHeadingDial, LLSD(degrees), true)', REFRESH)

    def test_clone_kit_and_wheel_wrap(self):
        xui = ROOT / 'indra/newview/skins/default/xui/en'
        move = ET.parse(xui / 'panel_dpage_cast_move.xml')
        ghosts = ET.parse(xui / 'panel_dpage_cast_ghosts.xml')
        row = move.find('.//kit_row[@widget="my_heading_dial"]')
        clone = ghosts.find('.//kit_row[@widget="heading_dial"]')
        self.assertEqual(len(move.findall('.//kit_row[@widget="my_heading_dial"]')), 1)
        self.assertEqual(row.get('label'), 'My heading')
        self.assertEqual(row.get('kind'), clone.get('kind'))
        self.assertEqual(row.get('widget_history'), 'false')
        self.assertEqual(row.get('keywords'), 'heading turn rotate facing yaw direction my avatar dial')
        self.assertEqual(row.get('help'), 'Drag the needle to turn your avatar in-world, as if you turned with the keys. Others see it. The wheel trims 5 degrees per click. Not available while seated.')
        dial, original = row.find('compass_dial'), clone.find('compass_dial')
        # Kit controls own geometry; name and tooltips describe the different targets.
        ignored = {'name', 'tool_tip', 'right'}
        self.assertEqual({k: v for k, v in dial.attrib.items() if k not in ignored},
                         {k: v for k, v in original.attrib.items() if k not in ignored})
        step = float(dial.get('wheel_step', re.search(r'wheel_step\("wheel_step", ([\d.]+)f\)', DIAL)[1]))
        self.assertEqual(step, 5)
        self.assertIn('setValue(mValueDegrees - (F32)delta.mClicks * mWheelStep);', DIAL)
        wrap = function(DIAL, 'F32 wrap_range(F32 value, F32 minimum, F32 maximum)')
        self.assertIn('value = fmodf(value - minimum, span);', wrap)
        self.assertIn('if (value < 0.f) value += span;', wrap)
        self.assertIn('return minimum + value;', wrap)
        for value, clicks, expected in [(179, -1, -176), (-179, 1, 176),
                                        (0, 1, -5), (0, -1, 5), (175, -1, -180)]:
            result = math.fmod(value - clicks * step + 180, 360)
            if result < 0:
                result += 360
            self.assertEqual(result - 180, expected)


if __name__ == '__main__':
    unittest.main()
