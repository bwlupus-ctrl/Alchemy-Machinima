"""Minimum shadow softness (RenderSoftShadowMinSoftness) on production GL: the metric projector kernel
(shadowUtil.glsl) and the world cube filter (areaShadowFilterF.glsl).

A light with a tiny source radius (World light size 0.01 m) has a penumbra far under one texel of its own shadow
map: the filter then resolves the map's texels, and on the 512-texel cube faces the shadow's outline is a staircase
(12 mm steps at 3 m). The floor keeps both kernels at least N texels of the light's own map wide (default 2), so
the outline is the caster's, not the map's. 0 = off, byte-identical to BASE.

One lamp, a ball close to a plane (penumbra well under a texel), an oblique camera that resolves a texel with many
pixels. Measured on the rendered shader output along the ball's round shadow outline, in texels of the light's map:

    stair   RMS distance of the 50 % contour from the true outline
    width   10 % - 90 % width of the edge (median over the directions of the outline)
    pinch   the narrowest direction's width over the median width (a notch where two filters meet)

  projector   the edge widens with the floor (off: the one-texel kernel, positive control) and the kernel radius the
              shader uses is at least the floor wherever it reaches the soft kernel; Max penumbra still wins; a
              penumbra already wider than the floor is bit-identical with the floor on; the lit side of the edge has
              no step (the blocker search covers the floored kernel); on a grazing receiver the slope clamp still
              narrows the kernel under the floor, the default floor keeps the 8 cm shadow the one-texel kernel keeps,
              and the metric gate's widen check passes with the floor on.
  cube        inside a face: the staircase is there with the floor off (positive control) and at most half of it
              is left with the floor on; the edge widens. Across a cube edge (the band where the face path hands
              over to the direction path): the edge widens and has no notch. A wide penumbra is unchanged.
  identity    floor 0, and never uploading it, are byte-identical to BASE: projector and cube.
  source      the setting, its default, both uploads and the clamp.
Planted regressions must each be rejected: the floor ignored by the projector kernel, by the cube face path, by the
cube direction path; the floor applied after the slope clamp; the floor allowed past Max penumbra; a floor that also widens wide penumbrae; the cube edge band left on the offset morph.
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
import relight_spot_bias_gl_check as B
import relight_spot_metric_gl_check as M
from rig_shadow_presets_check import settings_defaults

BASE = '0494b1f4d19'
FLOOR = 2.0
XC = .6                      # the ball's centre: 0.6 m in front of the plane, 2.4 m from the lamp
LAMP = np.array([3., 0., 1.45])
FILTER = 'indra/newview/app_settings/shaders/class3/deferred/areaShadowFilterF.glsl'
LIMIT = dict(widen=.8, stair_control=.12, stair_share=.5, pinch=.8, outer_step=.12)

# Exact statements of the two shaders (a planted regression replaces one; drift fails the run).
FLOOR_UNIFORM = 'uniform float soft_shadow_min_radius = 0.0;'
LOW = '    float low = clamp(soft_shadow_min_radius, 1.0, cap);\n'
PR_FLOOR = '        pr = clamp(k_max * gap / lever, low, cap);\n'
FACE_FLOOR = 'float radius = clamp(r_map*max(zr-z_b,0.0)/z_b*AW_USABLE/(2.0*zr),aw_min_radius,AW_MAX_RADIUS);'
DIR_FLOOR = 'float radius = max(r_map*max(dist-z_b,0.0)/(z_b*dist),max(2.0*AW_MIN_RADIUS/AW_USABLE,aw_min_angular));'
BAND_FACE = 'bool face_path = !large && (edge <= 0.9921875 || floored);'
BAND_DIRECTION = '    if (direction_path) direct = awDirectionVisibility(c,large || floored ? 1.0 : morph);\n'


def projector_negatives():
    return {
        'projector_ignores_floor': [(PR_FLOOR, PR_FLOOR.replace('low, cap', '1.0, cap'))],
        'floor_after_slope_clamp': [(M.SLOPE_CLAMP, M.SLOPE_CLAMP + '        pr = max(pr, low);\n')],
        'floor_past_max_penumbra': [(LOW, '    float low = max(soft_shadow_min_radius, 1.0);\n')],
        'floor_widens_everything': [(PR_FLOOR, '        pr = clamp(k_max * gap / lever + low - 1.0, low, cap);\n')],
    }


def cube_negatives():
    return {
        'cube_face_ignores_floor': [(FACE_FLOOR, FACE_FLOOR.replace('aw_min_radius', 'AW_MIN_RADIUS'))],
        'cube_direction_ignores_floor': [(DIR_FLOOR, DIR_FLOOR.replace(',aw_min_angular', ',0.0'))],
        'cube_edge_band_morphs_offsets': [(BAND_FACE, 'bool face_path = !large && edge <= 0.9921875;'),
                                          (BAND_DIRECTION, BAND_DIRECTION.replace('large || floored', 'large'))],
    }


def cone_look(scene, axis_target, cone, texel, toward, half_texels=14.):
    """An oblique camera on one point of the shadow outline of a ball seen from the lamp under the half-angle `cone` around
    the ray LAMP -> axis_target. `toward`: a world direction; the outline point looked at is the one farthest that way.
    Returns cam, depth, normal, rho (metres across the ray, measured from the cone's axis, minus the outline's own radius
    at that receiver, plus the mean radius: the true outline is rho == mean radius), theta (direction around the axis), mask."""
    axis = S.unit(np.asarray(axis_target, float) - LAMP)
    side = np.asarray(toward, float) - axis * (np.asarray(toward, float) @ axis)
    side /= np.linalg.norm(side)
    edge_ray = axis * math.cos(cone) + side * math.sin(cone)
    target = LAMP + edge_ray * (-LAMP[0] / edge_ray[0])           # where that ray meets the plane x = 0
    eye = target + np.array([2.5, -1.2 if target[1] > 1. else 1.2, .6])
    half = half_texels * texel
    cam = S.Camera(eye, target, 384, 384, math.degrees(2 * math.atan(half / np.linalg.norm(eye - target))))
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    pos = g['pos'].astype(np.float64)
    ray = pos - LAMP
    range_m = np.maximum(np.linalg.norm(ray, axis=2), 1e-9)
    angle = np.arccos(np.clip((ray @ axis) / range_m, -1., 1.))
    radius = range_m * math.tan(cone)
    rho = range_m * np.tan(angle)
    up = np.cross(axis, side)
    theta = np.arctan2(ray @ up, ray @ side)
    mask = (g['surf'] == 0) & (np.abs(rho - radius) < half * .8) & (np.linalg.norm(pos - target, axis=2) < half * .9)
    assert mask.sum() > 20000, mask.sum()
    mean_radius = float(radius[mask].mean())
    return cam, d, n, rho - radius + mean_radius, theta, mask, mean_radius


def outline(v, rho, theta, mask, radius_m, texel_m):
    """dict(stair, width, pinch, directions) of the shadow outline, in texels. The shadow is inside (v rises with rho)."""
    t, r_, val = theta[mask], (rho[mask] - radius_m) / texel_m, v[mask]
    arc = texel_m * .5 / radius_m                       # half a texel of outline per direction bin
    bins = np.floor((t - t.min()) / arc).astype(int)
    step = .2
    crossings, widths = [], []
    for b in range(bins.max() + 1):
        sel = bins == b
        if sel.sum() < 40:
            continue
        idx = np.floor((r_[sel] - r_[sel].min()) / step).astype(int)
        cnt = np.bincount(idx)
        mean = np.bincount(idx, weights=val[sel]) / np.maximum(cnt, 1)
        centre = (np.arange(len(cnt)) + .5) * step + r_[sel].min()
        ok = cnt > 2
        mean, centre = mean[ok], centre[ok]

        def cross(level):
            for i in range(len(mean) - 1):
                if mean[i] < level <= mean[i + 1]:
                    return centre[i] + (level - mean[i]) / (mean[i + 1] - mean[i]) * (centre[i + 1] - centre[i])
            return None
        mid, lo, hi = cross(.5), cross(.1), cross(.9)
        if mid is not None:
            crossings.append(mid)
        if lo is not None and hi is not None:
            widths.append(hi - lo)
    if len(crossings) < 20 or len(widths) < 20:
        return dict(stair=None, width=None, pinch=None, directions=len(crossings))
    c = np.array(crossings)
    return dict(stair=float(np.sqrt(np.mean((c - np.median(c)) ** 2))), width=float(np.median(widths)),
                pinch=float(np.min(widths) / np.median(widths)), directions=len(c))


# ---------------------------------------------------------------- projector
def projector_render(r, prog, cam, d, n, e, run, metric, floor, vogel=False, noise=2):
    def extra(p):
        if floor is not None:
            r.uniform(p, 'soft_shadow_min_radius', float(floor))
    return M.render(r, prog, cam, d, n, e, run, metric, taps=16, vogel=vogel, noise=noise, extra=extra)


def projector_check(r, host, defaults, edits=(), base=None):
    edits = list(edits)
    rows, failures = [], []
    prog = B.link(r, M.Mut(edits), 'floor projector')
    probe = B.link(r, M.Mut(edits + [M.RADIUS_PROBE]), 'floor radius probe')
    e = M.lamp()
    res = 2048
    texel = M.texel_m(e, res)
    scene = M.scene_of([M.ball(XC)])
    cone = math.asin(M.RHO / (3. - XC))
    cam, d, n, rho, theta, mask, radius_m = cone_look(scene, (0., 0., 1.45), cone, texel, (0., 1., 1.), half_texels=30.)
    r.native_map(scene, B.light_of(e), e['fov'], res)
    _, metric = M.metric_of(host, r, e, .002)           # a 2 mm source: a penumbra of a third of a texel
    run = M.run_of(defaults, 8.)
    if base is not None:
        # Before anything uploads the floor: a program keeps its uniform values. Noise model 1: BASE has no model 2.
        fresh = B.link(r, M.Mut(edits), 'floor never uploaded')
        for vogel in (False, True):
            ref = projector_render(r, base, cam, d, n, e, run, metric, None, vogel, noise=1)
            never = projector_render(r, fresh, cam, d, n, e, run, metric, None, vogel, noise=1)
            zero = projector_render(r, prog, cam, d, n, e, run, metric, 0., vogel, noise=1)
            row = dict(check='projector identity', kernel='vogel' if vogel else 'poisson',
                       never_uploaded=bool(np.array_equal(never, ref)), floor_0=bool(np.array_equal(zero, ref)),
                       soft_pixels=int((mask & (ref > .02) & (ref < .98)).sum()))
            rows.append(row)
            if not (row['never_uploaded'] and row['floor_0']) or row['soft_pixels'] < 500:
                failures.append('projector %s: floor 0 is not byte-identical to BASE (%s)' % (row['kernel'], json.dumps(row)))
    images = {}
    for vogel in (False, True):
        images = {f: projector_render(r, prog, cam, d, n, e, run, metric, f, vogel) for f in (0., FLOOR)}
        off, on = (outline(images[f], rho, theta, mask, radius_m, texel) for f in (0., FLOOR))
        radii = projector_render(r, probe, cam, d, n, e, run, metric, FLOOR, vogel) * 64.
        soft = mask & (images[FLOOR] > .02) & (images[FLOOR] < .98)
        v = images[FLOOR]
        lit_side = mask & (rho > radius_m)
        both = lit_side[:, 1:] & lit_side[:, :-1]
        row = dict(check='projector', kernel='vogel' if vogel else 'poisson', off=off, on=on,
                   kernel_radius_min=float(radii[soft].min()) if soft.any() else None,
                   outer_step_max=float(np.abs(v[:, 1:] - v[:, :-1])[both].max()) if both.any() else None)
        rows.append(row)
        tag = 'projector %s: ' % row['kernel']
        if off['width'] is None or on['width'] is None:
            failures.append(tag + 'no outline measured (%s)' % json.dumps(row))
            continue
        if on['width'] < off['width'] + LIMIT['widen']:
            failures.append(tag + 'the edge does not widen with the floor (%.2f against %.2f texels)' % (on['width'], off['width']))
        if row['kernel_radius_min'] is None or row['kernel_radius_min'] < FLOOR - .01:
            failures.append(tag + 'a soft pixel uses a kernel of %s texels under a floor of %g' % (row['kernel_radius_min'], FLOOR))
        if row['outer_step_max'] is None or row['outer_step_max'] > LIMIT['outer_step']:
            failures.append(tag + 'the lit side of the floored edge has a step of %s' % row['outer_step_max'])
    # the cap wins: Max penumbra 1.5 texels under a floor of 2
    capped = projector_render(r, probe, cam, d, n, e, M.run_of(defaults, 1.5), metric, FLOOR) * 64.
    soft = mask & (images[FLOOR] > .02) & (images[FLOOR] < .98)
    row = dict(check='projector cap', kernel_radius_max=float(capped[soft].max()) if soft.any() else None)
    rows.append(row)
    if row['kernel_radius_max'] is None or row['kernel_radius_max'] > 1.5 + .01:
        failures.append('projector: the floor exceeds Max penumbra (%s texels under a cap of 1.5)' % row['kernel_radius_max'])
    # a penumbra wider than the floor: bit-identical
    scene = M.scene_of([M.ball(1.5)])
    cam, d, n, rho, theta, mask, radius_m = cone_look(scene, (0., 0., 1.45), math.asin(M.RHO / 1.5), texel, (0., 1., 1.), half_texels=60.)
    r.native_map(scene, B.light_of(e), e['fov'], res)
    _, metric = M.metric_of(host, r, e, .02)
    run = M.run_of(defaults, 24.)
    a, b = (projector_render(r, prog, cam, d, n, e, run, metric, f) for f in (0., FLOOR))
    radii = projector_render(r, probe, cam, d, n, e, run, metric, 0.) * 64.
    part = mask & (a > .02) & (a < .98) & (radii > FLOOR + .5)
    row = dict(check='projector wide', pixels=int(part.sum()), identical=bool(np.array_equal(a[part], b[part])))
    rows.append(row)
    if part.sum() < 2000 or not row['identical']:
        failures.append('projector: a penumbra wider than the floor changes with it (%s)' % json.dumps(row))
    return rows, failures


def grazing_check(r, host, defaults, edits=()):
    """A lamp at 80 degrees incidence, 3 m away, a 2 mm source; a card 8 cm in front of the plane along the light ray (1.4 cm
    above it), the floor at its maximum (4 texels). The slope bias of a kernel that wide is more than a share of the blocker's
    distance there, so the slope clamp must still narrow the kernel UNDER the floor (a floor applied after the clamp would
    widen it again and pass the blocker). The scene is relight_spot_metric_gl_check's retention scene with a tiny source.
    Reported with it: how much of that 8 cm wide grazing shadow stays darker than one half with the floor at 0, 2 and 4. A
    floor in texels softens what is only a few texels wide (one texel is 9.5 mm of plane at this angle); that is the
    setting's price, stated here, not a defect of the kernel."""
    a = math.radians(80.)
    along = .08
    lamp_pos = R.CENTRE + 3. * np.array([math.cos(a), math.sin(a), 0.])
    blocker = S.Prim('card', 'obb', c=(along * math.cos(a), 0., 1.45), h=(.0005, .15, .15))
    scene = M.scene_of([blocker])
    shift = along * math.sin(a)
    target = np.array([0., -.15 - shift * .5, 1.45])
    eye = target + np.array([1., -2., 0.])
    cam = S.Camera(eye, target, 480, 480, math.degrees(2 * math.atan(shift * 1.5 / np.linalg.norm(eye - target))))
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    plane = g['surf'] == 0
    pts = g['pos'][plane].astype(np.float64)

    def shadowed(points):
        o = points + np.array([1.e-4, 0, 0])
        v = lamp_pos - o
        dist = np.linalg.norm(v, axis=1)
        return S.isect(blocker, o, v / dist[:, None]) < dist
    base = shadowed(pts)
    stable = np.ones(len(pts), bool)
    for dy, dz in ((-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)):
        stable &= shadowed(pts + np.array([0., .3 * shift * dy, .02 * dz])) == base
    dark = np.zeros(plane.shape, bool)
    dark[plane] = stable & base
    e = dict(B.entry(lamp_pos, 1., False), softness=0.)
    edits = list(edits)
    prog = B.link(r, M.Mut(edits), 'grazing')
    probe = B.link(r, M.Mut(edits + [M.RADIUS_PROBE]), 'grazing radius probe')
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    _, metric = M.metric_of(host, r, e, .002)
    run = M.run_of(defaults, 8.)
    top = 4.
    rows, failures = [], []
    for vogel in (False, True):
        kept = {f: float((projector_render(r, prog, cam, d, n, e, run, metric, f, vogel)[dark] < .5).mean()) if dark.any() else None
                for f in (0., FLOOR, top)}
        radii = projector_render(r, probe, cam, d, n, e, run, metric, top, vogel) * 64.
        row = dict(check='grazing', kernel='vogel' if vogel else 'poisson', expected_dark=int(dark.sum()),
                   kept_floor_0=kept[0.], kept_floor_2=kept[FLOOR], kept_floor_4=kept[top],
                   narrowed_share=float((radii[dark] < top - .02).mean()) if dark.any() else None)
        rows.append(row)
        if row['expected_dark'] < 3000:
            failures.append('grazing %s: fixture has no stable shadow' % row['kernel'])
        elif row['narrowed_share'] < .2:
            failures.append('grazing %s: the slope clamp does not narrow the floored kernel (%.0f%% of the shadow under %g texels)' %
                            (row['kernel'], row['narrowed_share'] * 100., top))
        elif kept[FLOOR] < kept[0.] - .05:
            failures.append('grazing %s: the default floor loses a grazing shadow the one-texel kernel keeps (%.3f against %.3f)' %
                            (row['kernel'], kept[FLOOR], kept[0.]))
    return rows, failures


def slope_check(r, host, defaults, edits=()):
    """The grazing contact above, and the metric gate's widen check (a slanted receiver: no lit hole, no acne) with the floor on."""
    rows, failures = grazing_check(r, host, defaults, edits)
    on = [(FLOOR_UNIFORM, 'uniform float soft_shadow_min_radius = %.1f;' % FLOOR)]
    more, f = M.widen_check(r, host, defaults, list(edits) + on)
    return rows + [dict(row, check='slope (widen, floor on)') for row in more], failures + ['slope: ' + x for x in f]


# ---------------------------------------------------------------- cube
class FilterMut(G.SourceTree):
    def __init__(self, edits=()):
        super().__init__('WORKTREE')
        self.edits = list(edits)

    def read(self, path):
        text = super().read(path)
        if Path(path).name == 'areaShadowFilterF.glsl':
            text = M.apply_edits(text, self.edits)
        return text


class AtlasRunner(G.AreaRunner):
    """An area runner of another source tree on the same context, reading the raster atlas of rig_shadow_gl.Runner."""
    gpu_atlas = None

    def _texture(self, key, unit, internal, w, h, fmt, typ, data, nearest=True):
        if key == 'atlas' and self.gpu_atlas is not None:
            self.gl.active(0x84C0 + unit)
            self.gl.bind_tex(0x0DE1, self.gpu_atlas)
            return self.gpu_atlas
        return super()._texture(key, unit, internal, w, h, fmt, typ, data, nearest)


def cube_render(runner, atlas, cam, d, n, light, near, defaults, floor):
    ch = dict(row=0, near=near, far=light.range, r_map=light.radius, contact=0., seed=light.seed, owned=True, contact_fade=0., detail_shadows=False)
    world = dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), channels=[ch])
    if floor is not None:
        world['min_radius'] = float(floor)
    plan = dict(samples=8, steps=12, half=0, near_steps=0, radius=0.)
    runner.gpu_atlas = atlas
    try:
        return runner.run(cam, d, n, [light], plan, world=world, light_bias=defaults['RenderAreaShadowLightBias'],
                          max_dist=defaults['RenderAreaShadowMaxDist'], thickness=defaults['RenderAreaShadowThickness'])[0]
    finally:
        runner.gpu_atlas = None


def cube_check(r, defaults, edits=(), base=None, fresh=None):
    """place 'face': the shadow falls inside the -X face. place 'edge': 45 degrees off, its top across the -X / +Y cube edge."""
    rows, failures = [], []
    live = AtlasRunner(FilterMut(edits), gl=r.gl)
    for place, lateral in (('face', 0.), ('edge', 3.)):
        plane_point = np.array([0., lateral, 1.45])
        dist = np.linalg.norm(plane_point - LAMP)
        ball = S.Prim('ball', 'sphere', c=tuple(LAMP + .8 * (plane_point - LAMP)), r=M.RHO)
        scene = M.scene_of([ball])
        cone = math.asin(M.RHO / (.8 * dist))
        texel = 2. * dist / 512.          # metres per cube texel across the ray (the on-axis density at that range)
        toward = (0., 1., 1.) if not lateral else (0., 0., 1.)
        cam, d, n, rho, theta, mask, radius_m = cone_look(scene, plane_point, cone, texel, toward)
        for source, label in ((.01, 'tiny'), (.3, 'wide')):
            light = S.Light('floor cube', M.LIGHT, source, 19.8, 'point', seed=23)
            near = CS.emitter_near(scene, light)
            atlas = r.raster.atlas(scene, light, near)
            if label == 'tiny' and base is not None:
                ref = cube_render(base, atlas, cam, d, n, light, near, defaults, None)
                never = cube_render(fresh, atlas, cam, d, n, light, near, defaults, None)
                zero = cube_render(live, atlas, cam, d, n, light, near, defaults, 0.)
                row = dict(check='cube identity', place=place, never_uploaded=bool(np.array_equal(never, ref)), floor_0=bool(np.array_equal(zero, ref)))
                rows.append(row)
                if not (row['never_uploaded'] and row['floor_0']):
                    failures.append('cube %s: floor 0 is not byte-identical to BASE (%s)' % (place, json.dumps(row)))
            images = {f: cube_render(live, atlas, cam, d, n, light, near, defaults, f) for f in (0., FLOOR)}
            row = dict(check='cube', place=place, source=label)
            tag = 'cube %s: ' % place
            if label == 'tiny':
                off, on = (outline(images[f], rho, theta, mask, radius_m, texel) for f in (0., FLOOR))
                row.update(off=off, on=on)
                if off['width'] is None or on['width'] is None:
                    failures.append(tag + 'no outline measured (%s)' % json.dumps(row))
                else:
                    if on['width'] < off['width'] + LIMIT['widen']:
                        failures.append(tag + 'the edge does not widen with the floor (%.2f against %.2f texels)' % (on['width'], off['width']))
                    if place == 'face':
                        if off['stair'] < LIMIT['stair_control']:
                            failures.append(tag + 'positive control failed: no staircase without the floor (%.3f texels)' % off['stair'])
                        if on['stair'] > LIMIT['stair_share'] * off['stair']:
                            failures.append(tag + 'the floored outline still follows the texels (%.3f against %.3f texels)' % (on['stair'], off['stair']))
                    if on['pinch'] < LIMIT['pinch']:
                        failures.append(tag + 'the floored edge has a notch (its narrowest direction is %.2f of the median width)' % on['pinch'])
            else:
                part = mask & (images[0.] > .02) & (images[0.] < .98)
                row.update(pixels=int(part.sum()), max_change=float(np.abs(images[0.] - images[FLOOR])[part].max()) if part.any() else None)
                if part.sum() < 2000 or row['max_change'] > 1.5 / 255.:
                    failures.append(tag + 'a wide penumbra changes with the floor (%s)' % json.dumps(row))
            rows.append(row)
    return rows, failures


def source_contracts(tree):
    pipeline = tree.read('indra/newview/pipeline.cpp')
    settings = tree.read('indra/newview/app_settings/settings.xml')
    shader = tree.read(B.SHADER)
    filt = tree.read(FILTER)
    header = tree.read(M.HEADER)
    block = settings.split('<key>RenderSoftShadowMinSoftness</key>')[1].split('</map>')[0] if '<key>RenderSoftShadowMinSoftness</key>' in settings else ''
    return {
        'setting_f32_default_2': '<string>F32</string>' in block and '<real>2.0</real>' in block,
        'projector_upload': pipeline.count('shader.uniform1f(LLStaticHashedString("soft_shadow_min_radius"), static_cast<F32>(ALRelight::softShadowFloor(soft_min_softness)));') == 1,
        'cube_upload': pipeline.count('gAreaShadowFilterProgram.uniform1f(LLStaticHashedString("area_world_min_radius"), static_cast<F32>(ALRelight::softShadowFloor(world_min_softness)));') == 1,
        'one_setting_both_uploads': pipeline.count('"RenderSoftShadowMinSoftness", 2.f)') == 2,
        'header_clamp': 'constexpr double kSoftShadowFloorMax = 4.0;' in header and 'softShadowFloor(99.0) == 4.0' in header,
        'shader_default_off': FLOOR_UNIFORM in shader and 'uniform float area_world_min_radius;' in filt,
        'projector_floor_once': shader.count(PR_FLOOR) == 1 and shader.count(LOW) == 1,
        'cube_floor_both_paths': filt.count(FACE_FLOOR) == 1 and filt.count(DIR_FLOOR) == 1 and filt.count(BAND_FACE) == 1,
        'legacy_kernel_untouched': 'soft_shadow_min_radius' not in shader.split('float pcfSpotShadowSurface')[1],
    }


def evaluate(out, do_negatives=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    defaults = settings_defaults()
    r = R.Runner('WORKTREE')
    print('[soft-floor] GPU', r.gl.renderer, flush=True)
    host = M.HeaderHost(out, r.tree.read(M.HEADER))
    failures, result = [], {}
    contracts = source_contracts(r.tree)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    base_prog = B.link(r, G.SourceTree(BASE), 'BASE visibility')
    base_area = AtlasRunner(G.SourceTree(BASE), gl=r.gl)
    fresh_area = AtlasRunner(G.SourceTree('WORKTREE'), gl=r.gl)
    sections = {}
    for name, fn in (('projector', lambda: projector_check(r, host, defaults, base=base_prog)),
                     ('slope', lambda: slope_check(r, host, defaults)),
                     ('cube', lambda: cube_check(r, defaults, base=base_area, fresh=fresh_area))):
        rows, f = fn()
        sections[name] = rows
        failures += f
        for row in rows:
            print('[soft-floor]', json.dumps(row, default=str), flush=True)
    result.update(contracts=contracts, **sections)
    if do_negatives:
        rejected = {}

        def note(name, caught, why):
            rejected[name] = bool(caught)
            print('[soft-floor]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, '|', str(why)[:170], flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)
        for name, edits in projector_negatives().items():
            try:
                f = projector_check(r, host, defaults, edits)[1] + slope_check(r, host, defaults, edits)[1]
                note(name, bool(f), f[0] if f else '')
            except AssertionError as ex:
                note(name, True, 'raised ' + str(ex)[:100])
        for name, edits in cube_negatives().items():
            try:
                f = cube_check(r, defaults, edits)[1]
                note(name, bool(f), f[0] if f else '')
            except AssertionError as ex:
                note(name, True, 'raised ' + str(ex)[:100])
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, default=str) + '\n', encoding='utf-8')
    print('[soft-floor]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_soft_shadow_floor')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
