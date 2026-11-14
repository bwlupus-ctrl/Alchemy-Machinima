"""World shadows of a wide light (World light size over 0.25 m) on production GL: areaShadowFilterF.glsl.

The report: with World light size 0.30 the wall and floor around an overhead lamp show curved bands that stay put when
the camera zooms. They are not acne: an empty room renders fully lit (asserted here). They are the penumbra of the
lamp's own shade. A source over 0.25 m takes the direction path, whose disk grows with the source (12 r / 0.1 texels:
36 texels at 0.30) while its tap count was halved to 16: taps 16 texels apart, each a one-texel compare, turned by
a rotation that is fixed in the world (it has to be, a per-pixel turn would boil with the camera). Every tap draws
its own hard copy of the shade's edge: 16 nested outlines, 6 % apart.

RenderAreaShadowWorldWideTaps (default 64, 0 = the old count): the tap count follows the disk's area at a small
source's density (32 taps at 12 texels) up to the maximum.

  empty     a floor and a wall under a bare lamp of radius 0.30 m, nothing to cast: every pixel more than a few
            pixels from the floor / wall crease is fully lit, with the old and the new counts.
  bands     the lamp inside a box shade, a close view of the wall inside the shade's penumbra. Against a 512-tap
            reference render (the same shader with the count forced) the old counts are far off and stepped (positive
            control); the default is at most half as far off and its largest step along a column is under 2.5 %
            (measured 2.0 %, the old counts 4.3 %).
  contact   a 10 cm block standing on the floor under that 0.30 m lamp: its geometric shadow on the floor next to
            it stays dark with the new counts and with the softness floor on (the fix must not cost contact shadows).
  scope     a source of 0.10 m renders bit-identically whatever the setting; the maximum is respected.
  identity  0, and never uploading the setting, are byte-identical to BASE.
Planted regressions must each be rejected: the count left at 16, the count not following the disk, the maximum
ignored.
"""
import argparse
import json
import math
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_cube_source as CS
import rig_shadow_gl as R
import relight_spot_metric_gl_check as M
import relight_soft_shadow_floor_gl_check as F
from rig_shadow_presets_check import settings_defaults

BASE = '0494b1f4d19'
FILTER = F.FILTER
LAMP = np.array([0., 0., 3.2])
RADIUS = .3025
DEFAULT = 64
FLOOR = S.Prim('floor', 'obb', c=(0, 0, -.05), h=(8, 8, .05))
WALL = S.Prim('wall', 'obb', c=(0, 4.05, 2.), h=(8, .05, 2.5))
SHADE = [S.Prim('shade%d' % i, 'obb', c=c, h=h) for i, (c, h) in enumerate(
    [((.3, 0, 3.3), (.01, .3, .2)), ((-.3, 0, 3.3), (.01, .3, .2)), ((0, .3, 3.3), (.3, .01, .2)), ((0, -.3, 3.3), (.3, .01, .2)),
     ((0, 0, 3.5), (.3, .3, .01))])]
BLOCK = S.Prim('block', 'obb', c=(1.5, 0., .5), h=(.05, .05, .5))
LIMIT = dict(control_error=3., error_share=.5, step=.025, control_step=.035, retention=.9)

COUNT = ('        filter_taps = clamp(int(ceil(float(AW_FILTER_TAPS)*texels*texels/(AW_MAX_RADIUS*AW_MAX_RADIUS))),AW_FILTER_TAPS/2,'
         'clamp(area_world_wide_taps,AW_FILTER_TAPS/2,AW_WIDE_TAPS_MAX));\n')
GATE = '    if (large && area_world_wide_taps > 0)\n'
MAXIMUM = 'const int AW_WIDE_TAPS_MAX = 128;'
REFERENCE = [(COUNT, '        filter_taps = 512;\n'), (MAXIMUM, 'const int AW_WIDE_TAPS_MAX = 512;')]


def negatives():
    return {
        'count_left_at_16': [(COUNT, '')],
        'count_does_not_follow_the_disk': [(COUNT, '        filter_taps = AW_FILTER_TAPS;\n')],
        'maximum_ignored': [(COUNT, COUNT.replace('clamp(area_world_wide_taps,AW_FILTER_TAPS/2,AW_WIDE_TAPS_MAX)', 'AW_WIDE_TAPS_MAX'))],
    }


def scene_of(prims):
    return S.Scene('wide source', list(prims), [], [], {}, np.empty((0, 3)))


def shot(scene, eye, target, fov, size=(640, 360)):
    cam = S.Camera(eye, target, size[0], size[1], fov)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    return cam, g, d, n


def render(runner, atlas, cam, d, n, light, near, taps, floor=2., contact=0.):
    ch = dict(row=0, near=near, far=light.range, r_map=light.radius, contact=contact, seed=light.seed, owned=True, contact_fade=0., detail_shadows=False)
    world = dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), channels=[ch])
    if floor is not None:
        world['min_radius'] = float(floor)
    if taps is not None:
        world['wide_taps'] = int(taps)
    runner.gpu_atlas = atlas
    try:
        return runner.run(cam, d, n, [light], dict(samples=8, steps=12, half=0, near_steps=0, radius=0.), world=world)[0]
    finally:
        runner.gpu_atlas = None


def lamp(radius=RADIUS):
    return S.Light('wide lamp', LAMP, radius, 10., 'point', seed=23)


def empty_check(r, live):
    """Nothing casts: every pixel away from the floor / wall crease is fully lit."""
    rows, failures = [], []
    scene = scene_of([FLOOR, WALL])
    light = lamp()
    near = CS.emitter_near(scene, light)
    atlas = r.raster.atlas(scene, light, near)
    for name, eye, target, fov in (('wall', (0., -3., 1.6), (0., 4., 1.8), 60), ('floor', (0., -5., 2.5), (0., 0., 0.), 50),
                                   ('wall close', (0., 2., 1.6), (.5, 4., 2.2), 30)):
        cam, g, d, n = shot(scene, eye, target, fov)
        pos = g['pos']
        # the crease is the line y = 4, z = 0: keep the receivers more than 10 cm from it
        away = (g['surf'] != 255) & (np.hypot(pos[..., 1] - 4., pos[..., 2]) > .1)
        for taps in (0, DEFAULT):
            v = render(live, atlas, cam, d, n, light, near, taps)
            row = dict(check='empty', view=name, wide_taps=taps, pixels=int(away.sum()), darkened=int((v[away] < .999).sum()),
                       minimum=float(v[away].min()), crease_pixels_darkened=int((v[(g['surf'] != 255) & ~away] < .999).sum()))
            rows.append(row)
            if row['pixels'] < 50000 or row['darkened']:
                failures.append('empty %s, %d taps: %d pixels of a bare wall / floor are darkened (minimum %.3f)' % (name, taps, row['darkened'], row['minimum']))
    return rows, failures


def bands_check(r, live, reference):
    """The shade's penumbra on the wall: error against the 512-tap reference, and the largest step down a column."""
    rows, failures = [], []
    scene = scene_of([FLOOR, WALL] + SHADE)
    light = lamp()
    near = CS.emitter_near(scene, light)
    atlas = r.raster.atlas(scene, light, near)
    cam, g, d, n = shot(scene, (0., 2., 1.6), (.5, 4., 2.2), 30)
    ref = render(reference, atlas, cam, d, n, light, near, 512)
    part = (g['surf'] != 255) & (ref > .05) & (ref < .95)
    out = {}
    for taps in (0, DEFAULT, 128):
        v = render(live, atlas, cam, d, n, light, near, taps)
        step = np.abs(np.diff(v, axis=0))[part[1:] & part[:-1]]
        out[taps] = dict(error=float(np.sqrt(np.mean((v - ref)[part] ** 2)) * 255.), largest_step=float(step.max()))
    row = dict(check='bands', penumbra_pixels=int(part.sum()), **{'taps_%d' % k: val for k, val in out.items()})
    rows.append(row)
    if part.sum() < 50000:
        failures.append('bands: the view is not inside the penumbra (%d pixels)' % part.sum())
    elif out[0]['error'] < LIMIT['control_error'] or out[0]['largest_step'] < LIMIT['control_step']:
        failures.append('bands: positive control failed, the old counts do not band here (%s)' % json.dumps(out[0]))
    else:
        if out[DEFAULT]['error'] > LIMIT['error_share'] * out[0]['error']:
            failures.append('bands: the default count is still banded (error %.2f against %.2f of the old counts)' % (out[DEFAULT]['error'], out[0]['error']))
        if out[DEFAULT]['largest_step'] > LIMIT['step']:
            failures.append('bands: the default count still steps by %.3f' % out[DEFAULT]['largest_step'])
        if out[128]['error'] >= out[DEFAULT]['error']:
            failures.append('bands: the maximum is not respected (128 taps are no closer than %d)' % DEFAULT)
    return rows, failures


def contact_check(r, live):
    """A block standing on the floor: the floor in its geometric shadow, next to its foot, stays dark."""
    rows, failures = [], []
    scene = scene_of([FLOOR, WALL, BLOCK])
    light = lamp()
    near = CS.emitter_near(scene, light)
    atlas = r.raster.atlas(scene, light, near)
    cam, g, d, n = shot(scene, (3.2, -1.6, 1.2), (1.75, 0., 0.), 20)
    floor_px = g['surf'] == 0
    pts = g['pos'][floor_px].astype(np.float64)

    def umbra(points):
        # inside the umbra of the whole 0.30 m source: blocked from the centre and from six points of its rim
        o = points + np.array([0., 0., 1.e-4])
        ok = np.ones(len(o), bool)
        for off in ((0, 0, 0), (RADIUS, 0, 0), (-RADIUS, 0, 0), (0, RADIUS, 0), (0, -RADIUS, 0), (0, 0, RADIUS), (0, 0, -RADIUS)):
            v = LAMP + np.array(off) - o
            dist = np.linalg.norm(v, axis=1)
            ok &= S.isect(BLOCK, o, v / dist[:, None]) < dist
        return ok
    dark = np.zeros(floor_px.shape, bool)
    dark[floor_px] = umbra(pts)
    lit = np.zeros(floor_px.shape, bool)
    lit[floor_px] = np.hypot(pts[:, 0] - 1.5, pts[:, 1]) > 1.2
    for taps, floor in ((0, 0.), (DEFAULT, 0.), (DEFAULT, F.FLOOR)):   # the last row: the shipped defaults
        v = render(live, atlas, cam, d, n, light, near, taps, floor=floor)
        row = dict(check='contact', wide_taps=taps, softness_floor=floor, umbra_pixels=int(dark.sum()),
                   kept=float((v[dark] < .5).mean()) if dark.any() else None, mean_in_umbra=float(v[dark].mean()) if dark.any() else None,
                   far_floor_lit=float(v[lit].min()) if lit.any() else None)
        rows.append(row)
        if dark.sum() < 300 or lit.sum() < 1000:
            failures.append('contact: the fixture has no umbra beside the block (%d px)' % dark.sum())
        elif row['kept'] < LIMIT['retention']:
            failures.append('contact, %d taps, floor %g: the contact shadow of the block is lost (%.0f%% of %d px kept)' % (taps, floor, row['kept'] * 100., dark.sum()))
        elif row['far_floor_lit'] < .999:
            failures.append('contact, %d taps: the floor far from the block is darkened (%.3f)' % (taps, row['far_floor_lit']))
    return rows, failures


def scope_check(r, live, base=None, fresh=None):
    rows, failures = [], []
    scene = scene_of([FLOOR, WALL] + SHADE)
    cam, g, d, n = shot(scene, (0., 2., 1.6), (.5, 4., 2.2), 30)
    small = lamp(.1)
    near = CS.emitter_near(scene, small)
    atlas = r.raster.atlas(scene, small, near)
    a, b = render(live, atlas, cam, d, n, small, near, 0), render(live, atlas, cam, d, n, small, near, 128)
    row = dict(check='scope', small_source_identical=bool(np.array_equal(a, b)), small_source_soft_pixels=int(((a > .02) & (a < .98)).sum()))
    wide = lamp()
    near = CS.emitter_near(scene, wide)
    atlas = r.raster.atlas(scene, wide, near)
    c32, c128 = render(live, atlas, cam, d, n, wide, near, 32), render(live, atlas, cam, d, n, wide, near, 128)
    row['maximum_changes_the_image'] = bool(not np.array_equal(c32, c128))
    # a maximum under the old count is the old count, not an inverted clamp
    row['below_16_is_16'] = bool(np.array_equal(render(live, atlas, cam, d, n, wide, near, 5), render(live, atlas, cam, d, n, wide, near, 16)))
    if base is not None:
        ref = render(base, atlas, cam, d, n, wide, near, None, floor=None)
        row['never_uploaded_is_base'] = bool(np.array_equal(render(fresh, atlas, cam, d, n, wide, near, None, floor=None), ref))
        row['zero_is_base'] = bool(np.array_equal(render(live, atlas, cam, d, n, wide, near, 0, floor=0.), ref))
    rows.append(row)
    if not row['small_source_identical'] or row['small_source_soft_pixels'] < 1000:
        failures.append('scope: a 0.10 m source changes with the setting (%s)' % json.dumps(row))
    if not row['below_16_is_16']:
        failures.append('scope: a maximum under 16 does not render as 16')
    if not row['maximum_changes_the_image']:
        failures.append('scope: the maximum is ignored (32 and 128 render the same)')
    if base is not None and not (row['never_uploaded_is_base'] and row['zero_is_base']):
        failures.append('identity: 0 or never uploading is not byte-identical to BASE (%s)' % json.dumps(row))
    return rows, failures


def source_contracts(tree):
    pipeline = tree.read('indra/newview/pipeline.cpp')
    settings = tree.read('indra/newview/app_settings/settings.xml')
    filt = tree.read(FILTER)
    block = settings.split('<key>RenderAreaShadowWorldWideTaps</key>')[1].split('</map>')[0] if '<key>RenderAreaShadowWorldWideTaps</key>' in settings else ''
    return {
        'setting_u32_default_64': '<string>U32</string>' in block and '<integer>64</integer>' in block,
        # review round 1: the upload is 0 or 16..128 (alrelightmath.h wideSourceTaps, static_asserted), and the shader's own
        # clamp has min <= max for any value
        'upload_clamped_to_0_or_16_128': pipeline.count('gAreaShadowFilterProgram.uniform1i(LLStaticHashedString("area_world_wide_taps"), ALWorldShadow::wideSourceTaps(world_wide_taps));') == 1 and
            'wideSourceTaps(0) == 0 && wideSourceTaps(1) == 16 && wideSourceTaps(15) == 16 && wideSourceTaps(64) == 64' in tree.read('indra/newview/alworldshadowmath.h') and
            'clamp(area_world_wide_taps,AW_FILTER_TAPS/2,AW_WIDE_TAPS_MAX)' in filt and
            pipeline.count('static LLCachedControl<U32> world_wide_taps(gSavedSettings, "RenderAreaShadowWorldWideTaps", 64);') == 1,
        'shader_default_is_old': 'uniform int area_world_wide_taps;' in filt and filt.count(COUNT) == 1 and filt.count(GATE) == 1,
        'rotation_stays_in_the_world': 'gl_FragCoord' not in filt.split('float awDirectionVisibility(')[1].split('float awVisibility(')[0],
    }


def run_checks(r, edits=(), base=None, fresh=None):
    live = F.AtlasRunner(F.FilterMut(list(edits)), gl=r.gl)
    reference = F.AtlasRunner(F.FilterMut(REFERENCE), gl=r.gl)
    rows, failures = [], []
    for fn in (lambda: empty_check(r, live), lambda: bands_check(r, live, reference), lambda: contact_check(r, live),
               lambda: scope_check(r, live, base, fresh)):
        more, f = fn()
        rows += more
        failures += f
    return rows, failures


def evaluate(out, do_negatives=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    r = R.Runner('WORKTREE')
    print('[wide-source] GPU', r.gl.renderer, flush=True)
    failures, result = [], {}
    contracts = source_contracts(r.tree)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    rows, f = run_checks(r, base=F.AtlasRunner(G.SourceTree(BASE), gl=r.gl), fresh=F.AtlasRunner(G.SourceTree('WORKTREE'), gl=r.gl))
    failures += f
    for row in rows:
        print('[wide-source]', json.dumps(row), flush=True)
    result.update(contracts=contracts, rows=rows)
    if do_negatives:
        rejected = {}
        for name, edits in negatives().items():
            try:
                f = run_checks(r, edits)[1]
                caught, why = bool(f), f[0] if f else ''
            except AssertionError as ex:
                caught, why = True, 'raised ' + str(ex)[:100]
            rejected[name] = caught
            print('[wide-source]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, '|', why[:170], flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, default=str) + '\n', encoding='utf-8')
    print('[wide-source]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_world_wide_source')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
