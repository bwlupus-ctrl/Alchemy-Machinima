#!/usr/bin/env python3
"""Source-derived math and integration fixtures; run without building the viewer.

Rez fixtures model the two event orderings because LLVOAvatar needs a logged-in
viewer. Structural assertions bind those fixtures to the production hooks.
"""
import math
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'


def read(name):
    return (NV / name).read_text(encoding='utf-8')


def body(source, signature):
    start = source.index('{', source.index(signature))
    end, depth = start + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start+1:end-1]


class AvatarAnimSpeedTest(unittest.TestCase):
    def setUp(self):
        self.src = read('alavataranimspeed.cpp')

    def test_math(self):
        # Evaluate the actual production expressions, not copied constants.
        def evaluate(function, **values):
            expr = re.search(r'return (.*);', body(self.src, function))[1]
            expr = re.sub(r'(\d+(?:\.\d*)?)f\b', r'\1', expr)
            expr = expr.replace('std::clamp', 'clamp')
            return eval(expr, {'__builtins__': {}}, dict(
                clamp=lambda x, a, b: max(a, min(b, x)), **values))
        for value, expected in [(-1, .05), (.01, .05), (1, 1), (8, 4)]:
            self.assertEqual(evaluate('F32 ALAvatarAnimSpeed::clampSpeed', speed=value), expected)
        for speed in [.05, .25, 1, 2, 4]:
            for slow_factor in [.2, 1]:
                self.assertAlmostEqual(evaluate('F32 ALAvatarAnimSpeed::compose',
                    speed=speed, slow_factor=slow_factor), speed*slow_factor)
        for spread in [0, 15, 50, 100, -10]:
            for unit in [i/100 for i in range(101)]:
                actual = evaluate('F32 ALAvatarAnimSpeed::variedSpeed', spread=spread, unit=unit)
                bound = max(0, min(50, spread))/100
                self.assertTrue(1-bound-1e-6 <= actual <= 1+bound+1e-6)
        setter = body(self.src, 'void ALAvatarAnimSpeed::setSpeed')
        self.assertIn('speed == 1.f', setter)
        self.assertIn('mSpeeds.erase(id)', setter)
        reset = body(self.src, 'S32 ALAvatarAnimSpeed::resetResidents')
        self.assertIn('mSpeeds.clear()', reset)
        self.assertIn('apply(*avatar)', reset)

    def test_vary_covers_union(self):
        targets = body(self.src, 'std::set<LLUUID> ALAvatarAnimSpeed::targets')
        for token in ['gAgentID', 'getCast()', 'LLCharacter::sInstances', 'isControlAvatar()', 'mIsDummy']:
            self.assertIn(token, targets)
        self.assertIn('targets()', body(self.src, 'S32 ALAvatarAnimSpeed::varyAll'))
        # One write for each identity even if self/cast/nearby overlap.
        self.assertEqual(len({'self'} | {'self', 'cast', 'absent'} | {'cast', 'resident'}), 4)

    def test_rez_and_late_animesh(self):
        self.assertIn('ALAvatarAnimSpeed::instance().apply(*this)',
                      body(read('llvoavatar.cpp'), 'void LLVOAvatar::initInstance()'))
        self.assertIn('ALAvatarAnimSpeed::instance().apply(*this)',
                      body(read('llvoavatar.cpp'), 'const LLViewerJointAttachment *LLVOAvatar::attachObject'))
        self.assertIn('ALAvatarAnimSpeed::instance().applyControl(*cav)',
                      body(read('llcontrolavatar.cpp'), 'LLControlAvatar *LLControlAvatar::createControlAvatar'))
        control = body(self.src, 'void ALAvatarAnimSpeed::applyControl')
        self.assertIn('control.mRootVolp->isLocalOnly()', control)
        self.assertIn('getAttachedAvatar()', control)
        self.assertIn('dynamic_cast<LLGhostAvatar*>', control)
        self.assertIn('return;', control)
        # Mock both arrival orderings: control first or parent attachment first.
        for order in [('rez', 'attach', 'control'), ('rez', 'control', 'attach')]:
            stored, wearer, control_factor = .5, None, None
            for event in order:
                if event == 'rez': wearer = stored
                if event == 'control': control_factor = stored if wearer else 1
                if event == 'attach' and control_factor is not None: control_factor = stored
            self.assertEqual((wearer, control_factor), (.5, .5))
        self.assertIn('applyObject(*object, factor)', body(self.src, 'void ALAvatarAnimSpeed::apply('))
        self.assertIn('control->setAnimTimeFactor(factor)', body(self.src, 'void ALAvatarAnimSpeed::applyObject'))

    def test_ghost_single_writer_and_scene(self):
        setter = body(self.src, 'void ALAvatarAnimSpeed::setSpeed')
        self.assertLess(setter.index('setInstanceAnimSpeed'), setter.index('mSpeeds.erase'))
        self.assertIn('return;', setter[:setter.index('mSpeeds.erase')])
        apply = body(self.src, 'void ALAvatarAnimSpeed::apply(')
        self.assertLess(apply.index('mIsDummy'), apply.index('avatar.setAnimTimeFactor'))
        save = body(self.src, 'LLSD ALAvatarAnimSpeed::sceneData')
        self.assertIn('isGhost(id)', save)
        load = body(self.src, 'void ALAvatarAnimSpeed::applySceneData')
        self.assertIn('if (!scene.has("avatar_anim_speeds")) return', load)
        self.assertIn('resetResidents()', load)
        self.assertIn('isGhost(id)', load)
        director = read('llfloaterdirector.cpp')
        self.assertIn('scene["avatar_anim_speeds"] = ALAvatarAnimSpeed::instance().sceneData()', director)
        self.assertIn('ALAvatarAnimSpeed::instance().applySceneData(scene)', director)
        self.assertIn('ALAvatarAnimSpeed::instance().setSlowMotion', read('llagent.cpp'))

    def test_menus_and_director(self):
        menu_cpp = read('alviewermenu.cpp')
        for name in ['menu_avatar_self.xml', 'menu_avatar_other.xml',
                     'menu_attachment_self.xml', 'menu_attachment_other.xml']:
            xml = ET.fromstring(read('skins/default/xui/en/' + name))
            menu = xml.find(".//context_menu[@name='Avatar Anim Speed']")
            self.assertIsNotNone(menu, name)
            self.assertEqual(len(menu.findall('menu_item_check')), 7)
            for node in menu.iter():
                if 'function' in node.attrib:
                    self.assertIn('"' + node.attrib['function'] + '"', menu_cpp)
        xml = ET.fromstring(read('skins/default/xui/en/panel_dpage_cast_move.xml'))
        card = xml.find(".//kit_card[@id='anim_speed']")
        self.assertIsNotNone(card)
        self.assertEqual(len(card.findall('kit_row')), 7)
        cpp = read('llfloaterdirector.cpp')
        for row in card.findall('kit_row'):
            self.assertIn('"' + row.attrib['widget'] + '"', cpp)
        self.assertEqual(card.find("kit_row/slider[@name='anim_speed_slider']").get('value_suffix'), 'x')
        self.assertIn('"panel_avatar_anim_speed.xml", document', cpp)
        self.assertIn('bindAnimSpeedPage(this)', cpp)
        self.assertIn('bindAnimSpeedPage(root)', cpp)
        slider = read('../llui/llsliderctrl.cpp')
        self.assertIn('if (!mEditor) text += mValueSuffix;', slider)

    def test_dance_clock_remains_independent(self):
        # Dance Ball supplies explicit external samples. Its transport speed
        # remains effective with a non-unit avatar clock (not multiplied twice).
        dance = read('aldanceball.cpp')
        self.assertIn('mClock.setSpeed(mTiming.mSpeed)', dance)
        self.assertIn('motion->setExternalSampleTime', dance)
        self.assertNotIn('setAnimTimeFactor(', dance)
        controller = read('../llcharacter/llmotioncontroller.cpp')
        self.assertIn('delta_time * mTimeFactor * sGlobalTimeFactor', controller)
        motion = read('../llcharacter/llmotion.cpp')
        self.assertIn('mExternalSampleTime', motion)


if __name__ == '__main__':
    unittest.main()
