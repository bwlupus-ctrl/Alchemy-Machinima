"""Scan Build production-default front travel and report-only default/time audit.

Uses the shipped ground-origin avatar, native upload-spy defaults, full shader
and real GL draws. The pre-fix committed shader must fail the same classifier.
"""
import json
import re
import xml.etree.ElementTree as ET

import numpy as np

BUILD_PERIOD = 2 * np.pi / .3
PRE_FIX = '9548462eb06'


class ScanBuildReference:
    """Migrate only the old Scan geometry/clock contract for shading comparisons.

    Keep the old shading, alpha, DC and clipping reference independent of the
    candidate. Raw pre-fix renders remain the front-travel negative control and
    the before strip. No limits or other-look references change.
    """
    def __init__(self, tree): self.tree = tree

    def read(self, path):
        source = self.tree.read(path)
        if path.endswith('actorghostF.glsl'):
            pattern = r'    else if \(ghostLook == 76\) // Studio:[\s\S]*?(?=    else if \(ghostLook ==)'
            match = re.search(pattern, source)
            assert match, 'missing reference Scan branch'
            branch = match.group()
            branch = branch.replace('(P.z + 1.15) / 2.25', '(P.z - sl_foot) / (sl_top - sl_foot)')
            branch = branch.replace('(2.0 / 2.25)', '(2.0 / (sl_top - sl_foot))')
            if 'float cycle =' not in branch:
                branch = branch.replace('        float prog =', '        float cycle = (ghostTime > 314.159265359 ? ghostTime - 628.318530718 : ghostTime) * 0.01;\n        float prog =')
            branch, count = re.subn(r'float prog = ghostDissolveProgress > 0\.0 \? ghostDissolveProgress\s*:[^;]+;',
                                   'float prog = 0.5 + 0.5 * sin(cycle * 30.0 + ghostAux.w);', branch)
            assert count == 1, 'reference must be a committed pre-fix shader'
            source = source[:match.start()] + branch + source[match.end():]
        return source


class HeldScanFront:
    """Test-only hold for the existing endpoint shading/sheen safety gates."""
    def __init__(self, tree): self.tree = tree

    def read(self, path):
        source = self.tree.read(path)
        if path.endswith('actorghostF.glsl'):
            automatic = 'float prog = 0.5 + 0.5 * sin(cycle * 30.0 + ghostAux.w);'
            assert source.count(automatic) == 1
            source = source.replace(automatic, 'float prog = clamp(ghostDissolveProgress, 0.0, 1.0);')
        return source


def front_failures(metrics):
    return [reason for reason, failed in (
        ('height is not skeleton-normalized', metrics['height_error'] > 1e-4),
        ('default front travel < 80% body height', metrics['travel_fraction'] < .8),
        ('visible built coverage travel < 80% body height', metrics['visible_travel_fraction'] < .8),
        ('missing built hold or empty hold', min(metrics['built_hold_samples'], metrics['empty_hold_samples']) < 2),
        ('build cycle does not close', metrics['cycle_error'] > 1e-5),
    ) if failed]


def measure_front(r, tree, current, seed):
    from clone_looks_safety_check import Variant
    marker = 'float d     = h - front;'
    source = tree.read('indra/newview/app_settings/shaders/class1/interface/actorghostF.glsl')
    assert source.count(marker) == 1
    diagnostic = r.program(Variant(tree, [(marker, marker + '\n        frag_color = vec4(front, h, P.z, 1.0); return;')]), ('HAS_SKIN',))
    a = r.anatomy
    height = a['top'] - a['foot']
    fronts, visible, height_error, samples = [], [], 0., []
    for t in np.linspace(0., BUILD_PERIOD, 65):
        frame = r.frame(diagnostic, 76, time=t, phase=seed)
        assert r.render_uniforms['ghostDissolveProgress'][1] == (.5,)
        assert r.render_uniforms['ghostDecodeScrub'][1] == (0,)
        mask = frame[..., 3] > .99
        assert mask.sum() > 100, 'empty real-avatar front probe'
        front = float(np.median(frame[mask, 0]))
        expected_h = np.clip((frame[mask, 2] - a['foot']) / height, 0, 1)
        height_error = max(height_error, float(np.max(abs(frame[mask, 1] - expected_h))))
        fronts.append(float(np.clip(front, 0, 1)))
        styled = r.frame(current, 76, time=t, phase=seed)
        built = mask & (styled[..., 3] > .95)
        z = float(frame[..., 2][built].max()) if built.any() else a['foot']
        visible.append(float(np.clip((z - a['foot']) / height, 0, 1)))
        samples.append(dict(time=float(t), front=front, visible_built_top=visible[-1]))
    r.gl.fn('glDeleteProgram', None, r.gl.U)(diagnostic)
    return dict(seed=seed, period=BUILD_PERIOD, height_error=height_error,
                travel_fraction=max(fronts)-min(fronts),
                visible_travel_fraction=max(visible)-min(visible),
                built_hold_samples=sum(s['front'] > 1. for s in samples),
                empty_hold_samples=sum(s['front'] < 0. for s in samples),
                cycle_error=abs(samples[0]['front']-samples[-1]['front']), samples=samples)


def check(out, r, tree, current):
    from clone_looks_safety_check import G, ROOT, SEEDS, Variant, discover_looks, note
    out.mkdir(parents=True, exist_ok=True)
    r.pose = 'bind'; r.camera = 0.; r.camera_center_z = sum(r.pose_bounds['bind']) * .5
    r.only = {'head', 'upper_body', 'lower_body', 'mEyeLeft', 'mEyeRight'}
    r.raw_hdr = True
    result = dict(defaults=r.draw_defaults, minimum_travel_fraction=.8, looks={})
    assert r.draw_defaults == dict(ghostDissolveProgress=.5, ghostDecodeScrub=0)
    try:
        result['fronts'] = [measure_front(r, tree, current, seed) for seed in SEEDS]
        for row in result['fronts']:
            assert not front_failures(row), ('Scan Build real defaults', front_failures(row), row)
        old_tree = G.SourceTree(PRE_FIX)
        old = r.program(old_tree, ('HAS_SKIN',))
        before = measure_front(r, old_tree, old, SEEDS[0])
        assert 'default front travel < 80% body height' in front_failures(before), 'pre-fix parked shader escaped'
        result['negative_control'] = dict(commit=PRE_FIX, rejected=front_failures(before), **before)
        result['isolated_negative_controls'] = {}
        for name, replacements in (
            ('parked_only', [('float prog = 0.5 + 0.5 * sin(cycle * 30.0 + ghostAux.w);',
                              'float prog = ghostDissolveProgress > 0.0 ? ghostDissolveProgress : 0.5 + 0.5 * sin(cycle * 30.0 + ghostAux.w);')]),
            ('fixture_frame_only', [('float h = clamp((P.z - sl_foot) / (sl_top - sl_foot), 0.0, 1.0);',
                                    'float h = clamp((P.z + 1.15) / 2.25, 0.0, 1.0);')]),
        ):
            bad_tree = Variant(tree, replacements)
            bad = r.program(bad_tree, ('HAS_SKIN',))
            metrics = measure_front(r, bad_tree, bad, SEEDS[0])
            failures = front_failures(metrics)
            assert failures, ('isolated Scan bug escaped', name)
            result['isolated_negative_controls'][name] = dict(rejected=failures, **metrics)
            r.gl.fn('glDeleteProgram', None, r.gl.U)(bad)
        # Changing only time must move the front differently for UUID seeds.
        curves = [np.array([s['front'] for s in row['samples']]) for row in result['fronts']]
        result['seed_front_max_difference'] = float(np.max(abs(curves[0]-curves[1])))
        assert result['seed_front_max_difference'] > .1, 'Scan fronts synchronized'
        # Report only: distinguish intentionally static styles from animation
        # parked specifically by the shared .5 default. Hold all other inputs.
        selectable = set(discover_looks())
        for combo in ET.parse(ROOT/'indra/newview/skins/default/xui/en/panel_ghost_studio.xml').iter('combo_box'):
            if combo.get('name') == 'style_combo':
                selectable.update(int(n.get('value')) for n in combo.findall('combo_box.item') if n.get('value', '').isdigit())
        for look in sorted(selectable - {68, 72}):
            changes = {}
            for label, progress in (('default', None), ('zero', 0.)):
                frames = [r.frame(current, look, time=t, dissolve_progress=progress) for t in (0., .73, 2., 4., 7.85, 11.3, BUILD_PERIOD)]
                changes[label] = max(float(np.max(abs(f-frames[0]))) for f in frames[1:])
            result['looks'][str(look)] = changes
        result['time_static_default'] = [int(k) for k,v in result['looks'].items() if v['default'] <= 1e-6]
        result['time_static_zero'] = [int(k) for k,v in result['looks'].items() if v['zero'] <= 1e-6]
        result['default_only_stationary'] = [int(k) for k,v in result['looks'].items() if v['default'] <= 1e-6 < v['zero']]
        # The other direct reader, Dissolve 10, has a real manual threshold
        # only in its shared beauty permutation. Audit that path as well.
        shared = r.program(tree, ('HAS_SKIN', 'GHOST_WORLD_PASS', 'GHOST_SHARED_DISSOLVE'))
        result['shared_dissolve_10'] = {}
        for label, progress in (('default', None), ('zero', 0.)):
            frames = [r.frame(shared, 10, time=t, dissolve_progress=progress) for t in (0., .73, 2., 4., 7.85)]
            result['shared_dissolve_10'][label] = max(float(np.max(abs(f-frames[0]))) for f in frames[1:])
        r.gl.fn('glDeleteProgram', None, r.gl.U)(shared)
        note('- REPORT ONLY dissolve/time audit (all other uniforms held): time-static at progress .5 = '+str(result['time_static_default'])+'; time-static at progress 0 = '+str(result['time_static_zero'])+'; parked only by .5 = '+str(result['default_only_stationary'])+'.')
        note('- REPORT ONLY shared Dissolve 10 time deltas (.5 default / 0 endpoint): '+str(result['shared_dissolve_10'])+'.')
        note('- PASS Scan Build production .5 defaults: >=80% skeleton-height front AND visible built-coverage travel, UUID phase separation; committed fix-3 shader rejected by the same classifier.')
    finally:
        r.only = None; r.raw_hdr = False
    (out/'scan_build_defaults_metrics.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    return result


if __name__ == '__main__':
    from clone_looks_safety_check import G, ROOT, RealAvatar
    r = RealAvatar(512, 640); tree = G.SourceTree('WORKTREE')
    check(ROOT/'.tmp/looks16/renders/scan_build', r, tree, r.program(tree, ('HAS_SKIN',)))
