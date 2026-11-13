"""Projector shadow fade model (RenderSpotShadowFadeModel) on production GL.

Real shadowV/F projector depth raster and the production shadowUtil.glsl sampler
(sampleSpotShadow), driven through rig_shadow_gl.Runner. Ground truth is an
independent ray cast of the same analytic scene, never the rendering.

  long_lens a block over a plane seen from 20 m with a 8 m sun shadow range, and from 100 m
            with the default 64 m range (the camera far beyond the sun cascades): model 1
            keeps the projector shadow, model 0 (legacy) loses it.
  clips     one view, the sun range moved around it. Model 1 never depends on shadow_clip, and
            its image is byte-identical to the legacy image wherever the legacy weight is
            exactly 1 and its ramp is 0. Where the legacy weight is not 1 (nearer the camera)
            the two agree within float rounding of (pcf * w) / w; inside the ramp the legacy
            image is lifted and beyond shadow_clip.w it is fully lit.
  edges     a caster on the edge texels of the map, all four edges, and beyond the map's far plane:
            receivers just OUTSIDE the map keep the legacy result (lit beyond the sun range,
            byte-identical to model 0 inside it) and never read the clamped edge texels.
  identity  model 0, a program that never receives the uniform, and every zero-normal
            (airborne) call are byte-identical to BASE at every clip.
  shaft     the New shaft engine's surface rim (a REAL normal) is byte-identical to BASE for
            both models, with the rim's receivers inside the legacy ramp and beyond it.
  zero      the legacy weight is exactly 0 at view depth shadow_clip.z * (1 - blend): BASE
            returns a non-finite value there (0/0); every path of the worktree is finite and
            returns the map's own result. A scan over 66 sun ranges lets BASE itself locate the
            singular float depths; the worktree is finite at each and unchanged beside them.
  looks     every shipped shaft look, soft and hard beam, three poses: byte-identical to BASE
            for both models (the draw that exposes a change of the legacy rounding).
  slots     the 16-slot layout (SPOT_SHADOWS_16) and the ten-slot layout both take the model
            on every probed slot.
  wiring    the production spotLightF program takes the model.
  negatives planted shader regressions must each be rejected by the checks above.

The area-shadow hand-over that goes with this model (the CPU "valid everywhere" clip, the
gather, its filter and the spot-light composition) is gated in relight_rank_check.py.
"""
import argparse
import ctypes as C
import json
import math
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import rig_shadow_gl as R
import relight_spot_bias_gl_check as B
from rig_shadow_presets_check import function, settings_defaults

BASE = 'c63f7ca1b2f'
SHADER = B.SHADER
LIMITS = dict(retention_kept=.90, retention_lost=.05, mean_visibility=.98, false_dark_percent=.02,
              expected_pixels=150, rounding=2.e-6, ramp_lift=.2)
BIAS_MODELS = (0, 1, 2)

UNIFORM = 'uniform int spot_shadow_fade_model = 0;\n'
GATE = '    if (spot_shadow_fade_model != 0 && projvol_rim_strength <= 0.0 && dot(norm,norm) > 0.0)\n'
BOUNDS = '        map_bounded = mp.w > 1.e-4 && all(greaterThanEqual(mp.xyz,vec3(0.0))) && all(lessThanEqual(mp.xyz,vec3(mp.w)));\n'
EARLY = '    if (map_bounded || spos.z > -shadow_clip.w)\n'
WEIGHT = '            if (map_bounded) w = 1.0;\n'
ZERO = '            if (max(spos.z-far_split.z, 0.0)/transition_domain.z == 1.0) transition_domain.z *= 2.0;\n'
RAMP = '            if (!map_bounded) shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);\n'
MUTANTS = {
    # name: (single-site edits, the check that must reject it)
    'zero_normal_enters_model': ([(GATE, GATE.replace(' && dot(norm,norm) > 0.0', ''))], 'identity'),
    'shaft_rim_enters_model': ([(GATE, GATE.replace(' && projvol_rim_strength <= 0.0', ''))], 'shaft'),
    'sun_early_out_kept': ([(EARLY, '    if (spos.z > -shadow_clip.w)\n')], 'long_lens'),
    'sun_ramp_kept': ([(RAMP, RAMP.replace('if (!map_bounded) ', ''))], 'long_lens'),
    'sun_weight_kept': ([(WEIGHT, '')], 'clips'),
    'shader_default_is_model_1': ([(UNIFORM, 'uniform int spot_shadow_fade_model = 1;\n')], 'identity'),
    'zero_weight_guard_dropped': ([(ZERO, '')], 'zero'),
    # The same guard written as a select on the weight: finite, but the compiler then rounds the legacy division
    # differently in the airborne march (one float step in the cascade blend).
    'zero_weight_guard_on_the_weight': ([(ZERO, ''), (WEIGHT, WEIGHT + '            if (w == 0.0) w = transition_domain.z;\n')], 'shaft_looks'),
    'model_ignored': ([(BOUNDS, '        map_bounded = false;\n')], 'long_lens'),
    # No map-bounds test: a receiver outside the map reads the clamped edge texels at every camera distance.
    'no_map_bounds': ([(BOUNDS, '        map_bounded = true;\n')], 'edges'),
    'map_bounds_on_one_axis': ([(BOUNDS, BOUNDS.replace('mp.xyz', 'mp.xxz'))], 'edges'),
    'map_bounds_without_depth': ([(BOUNDS, BOUNDS.replace('mp.xyz', 'mp.xyy'))], 'edges'),
}


def visibility(r, pr, cam, d, n, e, run, bias, fade, clip, airborne=False, vogel=False, upload=True, setup=None):
    def both(pr):
        # Absent in BASE (location -1): GL ignores the upload, exactly like the viewer.
        if upload:
            r.gl.ui(r.gl.loc(pr, b'spot_shadow_fade_model'), int(fade))
        if setup:
            setup(pr)
    return B.visibility(r, pr, cam, d, n, e, run, bias, airborne=airborne, vogel=vogel, clip=clip, setup=both)


def block_fixture(distance, width=320, near=.25, far=64.):
    """Plane x = 0 facing +x and a 30 cm block floating 30 cm above it (far more than the legacy
    4 cm receiver offset), seen from `distance` metres through a lens that frames the same 2.4 m."""
    scene = S.Scene('block over plane', [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6)),
                                         S.Prim('block', 'obb', c=(.40, 0, 1.45), h=(.10, .15, .15))],
                    [], [], {}, np.empty((0, 3)))
    eye = R.CENTRE + distance * np.array([.5, math.sqrt(.75), 0.])
    cam = S.Camera(eye, R.CENTRE, width, width, math.degrees(2. * math.atan(1.2 / distance)), near, far)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    return scene, cam, g, d, n


def light_entry(detail):
    return B.entry(R.CENTRE + np.array([3., .5, .3]), 1., detail)


def classes(scene, g, e, res):
    """Independent lit / shadowed receivers on the plane, well inside the projector's own map.

    The ray cast knows nothing about the map's frustum: a receiver outside it has no shadow-map answer
    (the lighting shader clips the projection before it reads the shadow), so it is no witness here."""
    lit, dark = B.truth(scene, g, e['pos'], 4. * B.texel_at(e, res))
    lcam = R.projector_camera(B.light_of(e), e['fov'], res, True)[0]
    p = np.concatenate([g['pos'].astype(np.float64), np.ones(g['pos'].shape[:2] + (1,))], axis=2) @ (lcam.proj @ lcam.view).T
    inside = (p[..., 3] > 0) & (np.abs(p[..., 0]) < .9 * p[..., 3]) & (np.abs(p[..., 1]) < .9 * p[..., 3])
    plane = (g['surf'] == 0) & inside
    return lit & plane, dark & plane


def map_coverage(g, e, res, margin=.02):
    """Receivers certainly inside / certainly outside this projector's map frustum (u, v and depth), from the
    same projection the sampler uses. A band of `margin` around each bound belongs to neither class."""
    lcam = R.projector_camera(B.light_of(e), e['fov'], res, True)[0]
    p = np.concatenate([g['pos'].astype(np.float64), np.ones(g['pos'].shape[:2] + (1,))], axis=2) @ (lcam.proj @ lcam.view).T
    solid = g['surf'] != 255
    w = np.where(p[..., 3] > 1.e-6, p[..., 3], 1.)
    ndc = p[..., :3] / w[..., None]
    reach = np.abs(ndc).max(axis=2)
    inside = solid & (p[..., 3] > 1.e-6) & (reach < 1. - margin)
    outside = solid & ((p[..., 3] <= 0.) | (reach > 1. + margin))
    return inside, outside, ndc


def view_depth(cam, g):
    """Positive view depth of every pixel's receiver (the plane and the block)."""
    p = g['pos'].astype(np.float64)
    return -(p @ cam.view[2, :3] + cam.view[2, 3])


def long_lens_check(r, pr, defaults, shots=None, kernels=B.KERNELS, bias_models=BIAS_MODELS):
    """The camera is beyond shadow_clip.w: legacy lights everything, the map-bounded model keeps the shadow."""
    shots = shots or (('20 m, 8 m sun range', 20., (1., 2., 4., 8.), .25, 64.),
                      ('100 m, default 64 m sun range', 100., (8., 16., 32., 64.), .5, 512.))
    rows, failures = [], []
    res = 2048
    for name, distance, clip, near, far in shots:
        scene, cam, g, d, n = block_fixture(distance, near=near, far=far)
        depth = view_depth(cam, g)[g['surf'] != 255]
        assert depth.min() > clip[3], ('the long-lens fixture is beyond the sun range', name, float(depth.min()))
        e0 = light_entry(False)
        r.native_map(scene, B.light_of(e0), e0['fov'], res)
        for detail in (False, True):
            e = dict(e0, detail_shadows=detail)
            lit, dark = classes(scene, g, e, res)
            for kernel, soft, vogel in kernels:
                run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
                for bias in bias_models:
                    # The legacy rig Detail receiver (bias model 0 / 1 with Detail on) has sub-millimetre floors:
                    # at these distances the 24-bit G-buffer depth acnes it whatever the fade model is (the bias
                    # fixture reports the same). Its lit side is informational; shadow retention is still gated.
                    legacy_detail = detail and bias != 2
                    for fade in (1, 0):
                        v = visibility(r, pr, cam, d, n, e, run, bias, fade, clip, vogel=vogel)
                        row = dict(check='long_lens', shot=name, path='detail' if detail else 'standard', kernel=kernel,
                                   bias_model=bias, fade_model=fade, **B.measure(v, lit, dark))
                        label = f"{name} {row['path']} {kernel} bias {bias} fade {fade}"
                        if row['expected_dark'] < LIMITS['expected_pixels'] or row['clear_pixels'] < LIMITS['expected_pixels']:
                            failures.append(label + ': instrument has too few witnesses')
                        elif fade:
                            if row['retention'] < LIMITS['retention_kept']:
                                failures.append(label + ': projector shadow lost beyond the sun range')
                            if not legacy_detail and (row['mean_lit_visibility'] < LIMITS['mean_visibility'] or
                                                      row['false_dark_percent'] > LIMITS['false_dark_percent']):
                                failures.append(label + ': acne')
                        else:
                            # If the legacy model ever keeps it, the fixture no longer discriminates the two.
                            if row['retention'] > LIMITS['retention_lost'] or float(v.min()) < 1.:
                                failures.append(label + ': legacy model unexpectedly kept the shadow beyond the sun range')
                        rows.append(row)
    return rows, failures


# One 10 m view; every pixel's receiver is 8.2 .. 12.7 m deep (asserted below). The sun range moves around it.
#   exact    1.25 * clip.z <= depth <= (clip.z + clip.w) / 2 : legacy weight exactly 1, ramp exactly 0
#   weighted depth < 0.75 * clip.z                            : legacy weight negative, ramp 0
#   blend    0.75 * clip.z < depth < 1.25 * clip.z            : legacy weight in (0, 1), ramp 0
#   ramp     depth in the second half of [clip.z, clip.w]     : legacy lifts toward lit
#   beyond   depth > clip.w                                   : legacy is lit
CLIPS = (('exact', (1., 3., 6., 30.)), ('weighted', (8., 16., 32., 64.)), ('blend', (2.5, 5., 10.5, 40.)),
         ('ramp', (.5, 1., 2., 13.)), ('beyond', (1., 2., 4., 8.)))


def clip_region(name, clip, depth, blend):
    z, w = clip[2], clip[3]
    return dict(exact=(depth.min() >= (1. + blend) * z) and (depth.max() <= .5 * (z + w)),
                weighted=depth.max() < (1. - blend) * z,
                blend=(depth.min() > (1. - blend) * z) and (depth.max() < (1. + blend) * z),
                ramp=(depth.min() > .5 * (z + w) + .05 * (w - z)) and (depth.max() < w),
                beyond=depth.min() > w)[name]


def clips_check(r, pr, defaults, kernels=B.KERNELS, bias_models=BIAS_MODELS):
    scene, cam, g, d, n = block_fixture(10.)
    # The legacy sampler compares the receiver moved up to spot_shadow_offset along its normal.
    depth = view_depth(cam, g)[g['surf'] != 255]
    margin = float(defaults['RenderSpotShadowOffset'])
    span = np.array([depth.min() - margin, depth.max() + margin])
    blend = float(defaults['RenderShadowSplitBlend'])
    for name, clip in CLIPS:
        assert clip_region(name, clip, span, blend), ('clip fixture', name, clip, span.tolist(), blend)
    res = 2048
    e0 = light_entry(False)
    r.native_map(scene, B.light_of(e0), e0['fov'], res)
    rows, failures = [], []
    for detail in (False, True):
        e = dict(e0, detail_shadows=detail)
        lit, dark = classes(scene, g, e, res)
        inside, outside, _ = map_coverage(g, e, res)
        assert min(lit.sum(), dark.sum()) >= LIMITS['expected_pixels']
        assert inside.sum() > 20000 and outside.sum() > 5000, ('the clip view shows receivers inside and outside the map', int(inside.sum()), int(outside.sum()))
        for kernel, soft, vogel in kernels:
            run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
            for bias in bias_models:
                images = {(name, fade): visibility(r, pr, cam, d, n, e, run, bias, fade, clip, vogel=vogel)
                          for name, clip in CLIPS for fade in (0, 1)}
                reference = images[('exact', 0)]            # the map's own result: legacy weight 1, ramp 0
                row = dict(check='clips', path='detail' if detail else 'standard', kernel=kernel, bias_model=bias,
                           shadow_pixels=int((reference[dark] < .5).sum()),
                           # Inside the map: the map's own result whatever the sun range is.
                           model1_independent_of_clip=all(np.array_equal(images[(name, 1)][inside], reference[inside]) for name, _ in CLIPS),
                           # Outside the map: the legacy statements, byte for byte, at every sun range.
                           outside_the_map_is_legacy=all(np.array_equal(images[(name, 1)][outside], images[(name, 0)][outside]) for name, _ in CLIPS),
                           weighted_max_difference=float(np.abs(images[('weighted', 0)] - reference).max()),
                           blend_max_difference=float(np.abs(images[('blend', 0)] - reference).max()),
                           ramp_legacy_lift=float((images[('ramp', 0)] - reference)[dark].mean()),
                           ramp_model1_lift=float((images[('ramp', 1)] - reference)[dark].mean()),
                           beyond_legacy_min=float(images[('beyond', 0)].min()),
                           beyond_model1_retention=float((images[('beyond', 1)][dark] < .5).mean()))
                label = f"{row['path']} {kernel} bias {bias}"
                if row['shadow_pixels'] < LIMITS['expected_pixels']:
                    failures.append(label + ': the reference image has no shadow')
                if not row['model1_independent_of_clip']:
                    failures.append(label + ': model 1 differs from the legacy image where the legacy weight is 1, or depends on shadow_clip')
                if not row['outside_the_map_is_legacy']:
                    failures.append(label + ': model 1 changes a receiver outside the projector map')
                if max(row['weighted_max_difference'], row['blend_max_difference']) > LIMITS['rounding']:
                    failures.append(label + ': the models differ by more than rounding in front of the legacy ramp')
                if row['ramp_legacy_lift'] < LIMITS['ramp_lift'] or row['beyond_legacy_min'] < 1.:
                    failures.append(label + ': the legacy ramp fixture does not lift / light the shadow')
                if abs(row['ramp_model1_lift']) > 0. or row['beyond_model1_retention'] < LIMITS['retention_kept']:
                    failures.append(label + ': model 1 loses the shadow in the legacy ramp or beyond it')
                rows.append(row)
    return rows, failures


def edge_fixture(distance=20.):
    """A frontal projector 3 m from the plane x = 0 and four bars floating 30 cm above the plane, each straddling
    one edge of the projector's map, so the map's edge texels hold a caster on all four sides. A fifth scene
    puts the receiving wall behind the map's far plane."""
    e = B.entry(R.CENTRE + np.array([3., 0, 0]), 1., False)
    lcam, dist, _ = R.projector_camera(B.light_of(e), e['fov'], 2048, True)
    half = (3. + dist - .125) * math.tan(e['fov'] * .5) * (3. + dist - .125 - .30) / (3. + dist - .125)   # the map edge at the bars' height
    prims = [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 8, 8))]
    # Each bar reaches 2 cm (a dozen texels) past the edge and 22 cm inward, so it covers the edge texels
    # without hiding the receivers just outside the edge from the (nearly frontal) camera.
    inner = half - .10
    for name, c, h in (('+y', (.30, inner, 1.45), (.02, .12, 2.2)), ('-y', (.30, -inner, 1.45), (.02, .12, 2.2)),
                       ('+z', (.30, 0, 1.45 + inner), (.02, 2.2, .12)), ('-z', (.30, 0, 1.45 - inner), (.02, 2.2, .12))):
        prims.append(S.Prim('bar' + name, 'obb', c=c, h=h))
    scene = S.Scene('bars on the map edges', prims, [], [], {}, np.empty((0, 3)))
    eye = R.CENTRE + distance * S.unit(np.array([1., .05, .04]))
    cam = S.Camera(eye, R.CENTRE, 400, 400, math.degrees(2. * math.atan(2.7 / distance)), .25, 64.)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    return scene, cam, g, d, n, e


def edges_check(r, pr, tree, defaults, kernels=B.KERNELS, bias_models=BIAS_MODELS):
    """Receivers just outside each map edge, a caster on the edge texels: no false shadow."""
    scene, cam, g, d, n, e0 = edge_fixture()
    res = 2048
    r.native_map(scene, B.light_of(e0), e0['fov'], res)
    inside, outside, ndc = map_coverage(g, e0, res)
    plane = g['surf'] == 0
    depth = view_depth(cam, g)[g['surf'] != 255]
    beyond, near = (1., 2., 4., 8.), (2., 6., 12., 60.)
    blend = float(defaults['RenderShadowSplitBlend'])
    assert depth.min() > beyond[3] and clip_region('exact', near, np.array([depth.min() - .2, depth.max() + .2]), blend), 'edge fixture sun ranges'
    # Just outside one edge (2 .. 10 % of the half width), well inside along the other axis.
    bands = {}
    for name, axis, sign in (('+u', 0, 1.), ('-u', 0, -1.), ('+v', 1, 1.), ('-v', 1, -1.)):
        along = sign * ndc[..., axis]
        bands[name] = plane & outside & (along > 1.02) & (along < 1.10) & (np.abs(ndc[..., 1 - axis]) < .8)
    # The instrument's own planted negative: the same shader without the bounds test must read the edge casters.
    control = B.link(r, B.MutantTree([(BOUNDS, '        map_bounded = true;\n')]), 'edge control (no map bounds)')
    saved = r.tree
    rows, failures = [], []
    for detail in (False, True):
        e = dict(e0, detail_shadows=detail)
        for kernel, soft, vogel in kernels:
            run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
            for bias in bias_models:
                far1 = visibility(r, pr, cam, d, n, e, run, bias, 1, beyond, vogel=vogel)
                far0 = visibility(r, pr, cam, d, n, e, run, bias, 0, beyond, vogel=vogel)
                near1 = visibility(r, pr, cam, d, n, e, run, bias, 1, near, vogel=vogel)
                near0 = visibility(r, pr, cam, d, n, e, run, bias, 0, near, vogel=vogel)
                unbounded = visibility(r, control, cam, d, n, e, run, bias, 1, beyond, vogel=vogel)
                row = dict(check='edges', path='detail' if detail else 'standard', kernel=kernel, bias_model=bias,
                           band_pixels={k: int(m.sum()) for k, m in bands.items()},
                           false_shadow={k: float((far1[m] < 1.).mean()) if m.any() else 1. for k, m in bands.items()},
                           control_shadow={k: float((unbounded[m] < .5).mean()) if m.any() else 0. for k, m in bands.items()},
                           outside_far_is_legacy=bool(np.array_equal(far1[outside], far0[outside])),
                           outside_near_is_legacy=bool(np.array_equal(near1[outside], near0[outside])),
                           inside_far_is_the_map=bool(np.array_equal(far1[inside], near0[inside])),
                           inside_shadow_pixels=int((far1[inside & plane] < .5).sum()))
                label = f"{row['path']} {kernel} bias {bias}"
                if min(row['band_pixels'].values()) < LIMITS['expected_pixels']:
                    failures.append(label + ': too few receivers outside a map edge %s' % row['band_pixels'])
                elif min(row['control_shadow'].values()) < .5:
                    failures.append(label + ': the edge texels hold no caster on some edge (instrument) %s' % row['control_shadow'])
                if max(row['false_shadow'].values()) > 0.:
                    failures.append(label + ': a receiver outside the map reads the edge texels %s' % row['false_shadow'])
                if not (row['outside_far_is_legacy'] and row['outside_near_is_legacy']):
                    failures.append(label + ': outside the map model 1 is not the legacy result')
                if not row['inside_far_is_the_map'] or row['inside_shadow_pixels'] < LIMITS['expected_pixels']:
                    failures.append(label + ': inside the map the shadow is not the map result beyond the sun range')
                rows.append(row)
    # Depth bound: a wall 1.5 m BEHIND the map's far plane (the projector's range) while u and v are inside.
    far_e = dict(B.entry(R.CENTRE + np.array([3., 0, 0]), 1., False), range=1.5)
    far_scene = S.Scene('wall behind the far plane', [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 8, 8)),
                                                         S.Prim('card', 'obb', c=(2.2, 0, 1.45), h=(.01, .3, .3))], [], [], {}, np.empty((0, 3)))
    light = S.Light('short key', far_e['pos'], .01, far_e['range'], 'projector', S.unit(R.CENTRE - far_e['pos']), math.degrees(far_e['fov']) / 2, seed=23)
    fg = S.cast_primary(far_scene, cam)
    _, fd, fn = S.encode_gbuffer(cam, fg)
    r.native_map(far_scene, light, far_e['fov'], res)
    lcam = R.projector_camera(light, far_e['fov'], res, True)[0]
    fp = np.concatenate([fg['pos'].astype(np.float64), np.ones(fg['pos'].shape[:2] + (1,))], axis=2) @ (lcam.proj @ lcam.view).T
    behind = (fg['surf'] == 0) & (fp[..., 2] > 1.02 * fp[..., 3]) & (np.abs(fp[..., 0]) < .8 * fp[..., 3]) & (np.abs(fp[..., 1]) < .8 * fp[..., 3])
    run = dict(defaults, BDMergeSoftProjectorShadows=False)
    f1 = visibility(r, pr, cam, fd, fn, far_e, run, 1, 1, beyond)
    n1 = visibility(r, pr, cam, fd, fn, far_e, run, 1, 1, near)
    n0 = visibility(r, pr, cam, fd, fn, far_e, run, 1, 0, near)
    row = dict(check='edges_depth', pixels=int(behind.sum()), far_min=float(f1[behind].min()) if behind.any() else None,
               near_is_legacy=bool(np.array_equal(n1[behind], n0[behind])))
    if row['pixels'] < 1000 or row['far_min'] < 1. or not row['near_is_legacy']:
        failures.append('a receiver behind the map far plane is not on the legacy statements: %s' % row)
    rows.append(row)
    return rows, failures


def identity_check(r, pr, base, tree, defaults):
    """Fade model 0, a program that never receives the uniform, and every airborne call == BASE at every clip."""
    scene, cam, g, d, n = block_fixture(10.)
    res = 2048
    e0 = light_entry(False)
    r.native_map(scene, B.light_of(e0), e0['fov'], res)
    never = B.link(r, tree, 'visibility that never receives the fade model')
    checks = {}
    differs = False
    for detail in (False, True):
        e = dict(e0, detail_shadows=detail)
        for kernel, soft, vogel in B.KERNELS:
            run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
            for bias in BIAS_MODELS:
                ok = dict(model0=True, no_upload=True, airborne0=True, airborne1=True)
                for name, clip in CLIPS:
                    reference = visibility(r, base, cam, d, n, e, run, bias, 0, clip, vogel=vogel)
                    ok['model0'] &= np.array_equal(visibility(r, pr, cam, d, n, e, run, bias, 0, clip, vogel=vogel), reference)
                    ok['no_upload'] &= np.array_equal(visibility(r, never, cam, d, n, e, run, bias, 0, clip, vogel=vogel, upload=False), reference)
                    air = visibility(r, base, cam, d, n, e, run, bias, 0, clip, airborne=True, vogel=vogel)
                    for fade in (0, 1):
                        try:
                            current = visibility(r, pr, cam, d, n, e, run, bias, fade, clip, airborne=True, vogel=vogel)
                            ok['airborne%d' % fade] &= np.array_equal(current, air)
                        except AssertionError:   # non-finite output
                            ok['airborne%d' % fade] = False
                    if name in ('ramp', 'beyond'):
                        # Positive control: the airborne and the legacy images must really sit in the fade here.
                        differs |= not np.array_equal(visibility(r, pr, cam, d, n, e, run, bias, 1, clip, vogel=vogel), reference)
                for key, value in ok.items():
                    checks['%s_%s_%s_bias%d' % (key, 'detail' if detail else 'standard', kernel, bias)] = bool(value)
    checks['model1_differs_in_the_fade'] = bool(differs)
    return checks


def shaft_check(gl, tree, base_runner=None):
    """New shaft engine, surface rim on: byte-identical to BASE whatever the fade model says."""
    import relight_volumetric_gl_check as V
    base_runner = base_runner or V.Runner(G.SourceTree(BASE), gl)
    current = V.Runner(tree, gl)
    fixture = V.fixtures(320, 180)['outside']
    passes = dict(rim_only=dict(steps=0, rim=1), march_with_rim=dict(steps=24, rim=1, dither=1),
                  half_res_march_beam_depth=dict(half=True, steps=24, rim=1, beam=1, dither=1))
    rows, failures = [], []
    # The fixture's receivers are 4 .. 12 m deep: inside the legacy ramp, beyond shadow_clip.w, and inside the
    # cascade blend, where the legacy weight is a fraction and the legacy division's rounding is exposed.
    for clip_name, clip in (('ramp', (2., 4., 6., 12.)), ('beyond', (.5, 1., 2., 3.)), ('blend', (2., 4., 8., 40.))):
        for name, kwargs in passes.items():
            # Every uniform is stated on both sides: programs keep their uniform values between draws.
            common = dict(soft_shadow_vogel=0, soft_shadow_taps=16, shadow_clip=clip)
            ref = base_runner.run(fixture, uniforms=dict(common, spot_shadow_fade_model=0), **kwargs)[:2]
            for fade in (0, 1):
                got = current.run(fixture, uniforms=dict(common, spot_shadow_fade_model=fade), **kwargs)[:2]
                same = all(np.array_equal(np.asarray(x), np.asarray(y)) for x, y in zip(got, ref))
                lit = int(np.count_nonzero(np.asarray(got[0])[:, :, :3].max(2) > 0))
                rows.append(dict(check='shaft', clip=clip_name, draw=name, fade_model=fade, identical=bool(same), lit_pixels=lit))
                if not same or lit < 100:
                    failures.append(f'shaft {clip_name} {name} fade {fade}: differs from BASE' if lit >= 100 else
                                    f'shaft {clip_name} {name}: fixture draws nothing')
    return rows, failures, base_runner


def shaft_looks_check(gl, tree, base_runner=None):
    """New shaft engine, every shipped look (soft and hard beam), three poses: byte-identical to BASE for both
    fade models. This is the form of relight_volumetric_engine_check's new_bytes; it is the draw that exposes a
    one-float-step change of the legacy (pcf * w + ramp) / w rounding in the airborne march."""
    import relight_volumetric_gl_check as V
    base_runner = base_runner or V.Runner(G.SourceTree(BASE), gl)
    current = V.Runner(tree, gl)
    defaults = V.settings_defaults()
    looks = [('defaults', V.look_spec({k: v[1] for k, v in defaults.items()}), (None,))]
    looks += [(look['id'], V.look_spec(V.look_state(look, defaults)), (False, True)) for look in V.parse_looks()['looks']]
    rows, failures = [], []
    for pose, fixture in V.fixtures(320, 180).items():
        for look, spec, beams in looks:
            for hard in beams:
                over = {} if hard is None else dict(hard_beam=hard)

                def render(runner, fade):
                    return V.render_look(runner, fixture, dict(spec, uniforms=dict(spec['uniforms'], spot_shadow_fade_model=fade)), **over)[1]
                ref = render(base_runner, 0)
                same = [bool(ref.tobytes() == render(current, fade).tobytes()) for fade in (0, 1)]
                rows.append(dict(check='shaft_looks', pose=pose, look=look, hard_beam=hard, identical_model0=same[0], identical_model1=same[1],
                                 lit_pixels=int(np.count_nonzero(np.asarray(ref)[:, :, :3].max(2) > 0))))
                if not all(same):
                    failures.append(f'shaft look {pose} {look} hard={hard}: differs from BASE (model 0 {same[0]}, model 1 {same[1]})')
    if sum(row['lit_pixels'] > 100 for row in rows) < len(rows) // 2:
        failures.append('shaft looks: the fixture draws nothing')
    return rows, failures, base_runner


ZERO_SOURCE = """
out vec4 frag_color;
uniform vec3 probe_pos;
uniform vec3 probe_norm;
uniform int probe_slot;
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
void main(){frag_color=vec4(sampleSpotShadow(probe_pos,probe_norm,probe_slot,vec2(.5)));}
"""


ZERO_SCAN_SOURCE = """
out vec4 frag_color;
uniform vec3 probe_pos;
uniform vec3 probe_norm;
uniform float probe_z[32];
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
void main(){int i=int(gl_FragCoord.x);
if (i >= 32) {frag_color=vec4(.5);return;}
frag_color=vec4(sampleSpotShadow(vec3(probe_pos.xy,probe_z[i]),probe_norm,0,vec2(.5)));}
"""
ZERO_SCAN = 32   # candidate depths per sun range: the predicted zero of the weight and 31 float steps around it


def zero_scan(r, programs, base, cam, d, n, e0, defaults, pos, norm):
    """Many sun ranges (shadow_clip.z, shadow_split_blend). For each, 32 consecutive float depths around the
    predicted zero of the legacy weight are sampled; BASE itself reports which of them divide 0 by 0."""
    gl = r.gl
    rng = np.random.default_rng(5)
    # Whether some float depth makes the weight exactly zero depends on the range: it always does where the
    # products are exact in binary (the first list), and only for a few of the arbitrary ranges (the second).
    combos = [(float(z), b) for z in (2., 4., 8., 12., 16., 24., 32., 48., 64., 96., 128.) for b in (.25, .125)]
    combos += [(10., .25), (6.5, .1), (27., .08), (110., .2)]
    combos += [(float(z), float(b)) for z, b in zip(rng.uniform(1., 200., 40), rng.uniform(.08, .25, 40))]

    def render(pr, clip_z, blend, zs, real_normal, fade):
        run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=False)
        r.target_float(cam)
        r._begin(pr)
        r.inputs(pr, cam, d, n)
        r.native_uniforms(pr, cam, e0, run)
        r.uniform(pr, 'shadow_clip', clip_z / 4., clip_z / 2., clip_z, clip_z * 4.)
        r.uniform(pr, 'shadow_split_blend', blend)
        r.uniform(pr, 'spot_shadow_offset', 0.)
        gl.ui(gl.loc(pr, b'spot_shadow_bias_model'), 0)
        gl.ui(gl.loc(pr, b'spot_shadow_fade_model'), fade)
        r.uniform(pr, 'probe_pos', pos[0], pos[1], pos[2])
        r.uniform(pr, 'probe_norm', *(norm if real_normal else np.zeros(3)))
        for i, z in enumerate(zs):
            r.uniform(pr, 'probe_z[%d]' % i, float(z))
        gl.draw(4, 0, 3)
        data = np.empty((1, ZERO_SCAN, 4), np.float32)
        gl.read(0, 0, ZERO_SCAN, 1, 0x1908, 0x1406, data.ctypes.data)
        return data[0, :, 0]

    hits, singular, bad, where = 0, 0, [], []
    for clip_z, blend in combos:
        cz, b, one = np.float32(clip_z), np.float32(blend), np.float32(1.)
        far, near = cz * -(one + b), cz * -(one - b)
        centre = np.float32(far + (near - far))
        zs = [centre]
        for _ in range(ZERO_SCAN // 2):
            zs.append(np.nextafter(zs[-1], np.float32(0.)))
        low = centre
        for _ in range(ZERO_SCAN - len(zs)):
            low = np.nextafter(low, np.float32(-1.e9))
            zs.insert(0, low)
        found = False
        for real_normal in (False, True):
            reference = render(base, clip_z, blend, zs, real_normal, 0)
            nan_at = ~np.isfinite(reference)
            found |= bool(nan_at.any())
            singular += int(nan_at.sum())
            for layout, pr in programs.items():
                for fade in ((0, 1) if real_normal else (0,)):
                    got = render(pr, clip_z, blend, zs, real_normal, fade)
                    if not np.all(np.isfinite(got)):
                        bad.append((clip_z, blend, 'surface' if real_normal else 'airborne', layout, fade))
                    elif not fade and not np.array_equal(got[~nan_at], reference[~nan_at]):
                        # Every depth that was finite in BASE keeps its exact legacy value.
                        bad.append((clip_z, blend, 'surface' if real_normal else 'airborne', layout, 'legacy value moved'))
        hits += int(found)
        if found:
            where.append([clip_z, blend])
    return dict(check='zero_scan', sun_ranges=len(combos), ranges_where_base_is_nonfinite=hits, base_nonfinite_samples=singular,
                base_nonfinite_ranges=where, worktree_failures=bad)


def zero_check(r, tree, defaults, base_tree=None):
    """The legacy weight is exactly 0 at view depth clip.z * (1 - blend) = 24 m for clip.z 32, blend .25."""
    def link(t, extended, label):
        saved = r.tree
        r.tree = t
        try:
            return r.link_text(ZERO_SOURCE, label, extra=r.FRAGMENT_COMMON + ['deferred/shadowUtil.glsl'],
                               defines='#define SPOT_SHADOW 1\n' + ('#define SPOT_SHADOWS_16 1\n' if extended else ''))
        finally:
            r.tree = saved
    programs = dict(ten=link(tree, False, 'zero weight probe'), sixteen=link(tree, True, 'zero weight probe (16 slots)'))
    base = link(base_tree or G.SourceTree(BASE), False, 'zero weight probe (BASE)')

    def link_scan(t, extended, label):
        saved = r.tree
        r.tree = t
        try:
            return r.link_text(ZERO_SCAN_SOURCE, label, extra=r.FRAGMENT_COMMON + ['deferred/shadowUtil.glsl'],
                               defines='#define SPOT_SHADOW 1\n' + ('#define SPOT_SHADOWS_16 1\n' if extended else ''))
        finally:
            r.tree = saved
    scan_programs = dict(ten=link_scan(tree, False, 'zero weight scan'), sixteen=link_scan(tree, True, 'zero weight scan (16 slots)'))
    scan_base = link_scan(base_tree or G.SourceTree(BASE), False, 'zero weight scan (BASE)')
    scene, cam, g, d, n = block_fixture(24., near=.25, far=64.)
    res = 2048
    e0 = light_entry(False)
    r.native_map(scene, B.light_of(e0), e0['fov'], res)
    lit, dark = classes(scene, g, e0, res)
    clip, blend, depth = (8., 16., 32., 64.), .25, 24.
    # The shader's own float32 arithmetic: w = 1 - max(spos.z - far_split.z, 0) / transition_domain.z.
    far_split, near_split = np.float32(clip[2]) * -np.float32(1. + blend), np.float32(clip[2]) * -np.float32(1. - blend)
    assert np.float32(1.) - max(np.float32(-depth) - far_split, np.float32(0.)) / (near_split - far_split) == 0., 'the probe depth is the zero of the legacy weight'
    # Two receivers on the plane, one in the block's shadow and one lit, pulled along their view ray to
    # exactly 24 m. Each is the receiver of its class nearest to 24 m from behind, so the probe ends up
    # at most millimetres in FRONT of the plane (never inside it) and keeps its class.
    targets = {}
    depths = view_depth(cam, g)
    for name, mask in (('shadowed', dark), ('lit', lit)):
        candidates = np.argwhere(mask & (depths >= depth))
        assert len(candidates), ('zero-weight instrument: no receiver at the probed depth', name)
        j, i = candidates[np.argmin(depths[mask & (depths >= depth)])]
        p = cam.view @ np.r_[g['pos'][j, i].astype(np.float64), 1.]
        assert 0. <= -p[2] - depth < .01, ('zero-weight instrument: receiver too far from the probed depth', name, float(-p[2]))
        nv = cam.view[:3, :3] @ g['nrm'][j, i].astype(np.float64)
        targets[name] = (np.array([p[0] * depth / -p[2], p[1] * depth / -p[2], -depth]), nv, 0. if name == 'shadowed' else 1.)

    def probe(pr, pos, norm, fade, detail, bias, z=None):
        gl = r.gl
        run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=False)
        r.target_float(cam)
        r._begin(pr)
        r.inputs(pr, cam, d, n)
        r.native_uniforms(pr, cam, dict(e0, detail_shadows=detail), run)
        r.uniform(pr, 'shadow_clip', *clip)
        r.uniform(pr, 'shadow_split_blend', blend)
        # No receiver shift: the probe position IS the depth the weight is computed from.
        r.uniform(pr, 'spot_shadow_offset', 0.)
        gl.ui(gl.loc(pr, b'spot_shadow_bias_model'), bias)
        gl.ui(gl.loc(pr, b'spot_shadow_fade_model'), fade)
        gl.ui(gl.loc(pr, b'probe_slot'), 0)
        r.uniform(pr, 'probe_pos', pos[0], pos[1], pos[2] if z is None else z)
        r.uniform(pr, 'probe_norm', *norm)
        gl.draw(4, 0, 3)
        data = np.empty((cam.H, cam.W, 4), np.float32)
        gl.read(0, 0, cam.W, cam.H, 0x1908, 0x1406, data.ctypes.data)
        assert np.all(data == data[0, 0]) or not np.all(np.isfinite(data)), 'the probe is one constant'
        return float(data[0, 0, 0])

    rows, failures = [], []
    paths = (('airborne', None, False, 0), ('surface', 1, False, 0), ('surface_detail', 1, True, 0),
             ('surface_bias_model', 1, False, 1), ('surface_detail_bias_model', 1, True, 2))
    base_nonfinite = 0
    for target, (pos, nv, expected) in targets.items():
        for path, real_normal, detail, bias in paths:
            norm = nv if real_normal else np.zeros(3)
            before = probe(base, pos, norm, 0, detail, 0)
            base_nonfinite += int(not math.isfinite(before))
            for layout, pr in programs.items():
                for fade in (0, 1):
                    at = probe(pr, pos, norm, fade, detail, bias)
                    # The map's own result just beside the singular depth (the legacy weight is +-1e-4 there).
                    beside = [probe(pr, pos, norm, fade, detail, bias, z=-depth + s) for s in (-.002, .002)]
                    row = dict(check='zero', target=target, path=path, layout=layout, fade_model=fade, base=repr(before),
                               value=repr(at), beside=[repr(x) for x in beside])
                    finite = math.isfinite(at) and all(math.isfinite(x) for x in beside)
                    if not finite or not 0. <= at <= 1.:
                        failures.append(f'{target} {path} {layout} fade {fade}: non-finite at the zero weight ({at})')
                    elif max(abs(at - x) for x in beside) > .26 or (path not in ('surface', 'airborne') and abs(at - expected) > .26):
                        # A hard 5-tap kernel moves in steps of 0.2; anything larger is not the neighbouring value.
                        # 'surface' and 'airborne' run the legacy receiver with NO offset here (the probe owns the
                        # depth), which may acne on the plane; the other paths carry their own bias and must also
                        # return the receiver's independently known class.
                        failures.append(f'{target} {path} {layout} fade {fade}: {at} is not the map result (beside it {beside}, expected {expected})')
                    rows.append(row)
    # The instrument must really sit on the singularity: BASE divides 0 by 0 there.
    if not base_nonfinite:
        failures.append('zero-weight instrument: BASE is finite at the probed depth, the probe misses the singularity')
    scan = zero_scan(r, scan_programs, scan_base, cam, d, n, e0, defaults, *targets['lit'][:2])
    rows.append(scan)
    if scan['ranges_where_base_is_nonfinite'] < 12:
        failures.append('zero-weight scan: BASE is non-finite in only %d of %d sun ranges, the scan misses the singularity'
                        % (scan['ranges_where_base_is_nonfinite'], scan['sun_ranges']))
    if scan['worktree_failures']:
        failures.append('zero-weight scan: %d failures, first %s' % (len(scan['worktree_failures']), scan['worktree_failures'][:4]))
    return rows, failures


def slots_check(r, tree, defaults):
    """Both shader layouts take the fade model on every probed slot (independent map/matrix isolation)."""
    source = r.visibility_source
    assert source.count(',0,tc)') == 1
    source = 'uniform int probe_slot;\n' + source.replace(',0,tc)', ',probe_slot,tc)')
    saved = r.tree
    r.tree = tree
    try:
        programs = {count: r.link_text(source, '%d-slot fade probe' % count, extra=r.FRAGMENT_COMMON + ['deferred/shadowUtil.glsl'],
                                       defines='#define SPOT_SHADOW 1\n' + ('#define SPOT_SHADOWS_16 1\n' if count == 16 else ''))
                    for count in (10, 16)}
    finally:
        r.tree = saved
    scene, cam, g, d, n = block_fixture(20.)
    clip = (1., 2., 4., 8.)
    res = 2048
    e = light_entry(False)
    r.native_map(scene, B.light_of(e), e['fov'], res)
    lit, dark = classes(scene, g, e, res)
    gl = r.gl
    poison = r._texture('slot_poison', 13, 0x8CAC, 1, 1, 0x1902, 0x1406, np.ones((1, 1), np.float32))
    for key, value in ((0x884C, 0x884E), (0x884D, 0x0203), (0x2801, 0x2601), (0x2800, 0x2601)):
        gl.param(0x0DE1, key, value)
    rows, failures = [], []
    for count, pr in programs.items():
        for slot in ((0, 5, 9) if count == 10 else (0, 9, 10, 15)):
            def setup(pr, slot=slot, count=count):
                tex, ncam, _ = r.native
                bias = np.eye(4); bias[:3, :3] *= .5; bias[:3, 3] = .5
                selected = bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view)
                for j in range(count + 4):
                    matrix = np.eye(4)
                    matrix[0, 3] = 100. + j
                    r._mat(pr, ('shadow_matrix[%d]' % j).encode(), selected if j == 4 + slot else matrix)
                for j in range(count):
                    unit = 13 + j
                    gl.active(0x84C0 + unit)
                    gl.bind_tex(0x0DE1, tex if j == slot else poison)
                    location = gl.loc(pr, ('shadowMap%d' % (4 + j)).encode())
                    assert location >= 0
                    gl.ui(location, unit)
                    r.uniform(pr, 'spot_shadow_detail[%d]' % j, 0.)
                    r.uniform(pr, 'spot_shadow_softness[%d]' % j, 0.)
                gl.ui(gl.loc(pr, b'probe_slot'), slot)
            run = dict(defaults, BDMergeSoftProjectorShadows=False)
            kept = B.measure(visibility(r, pr, cam, d, n, e, run, 1, 1, clip, setup=setup), lit, dark)
            lost = B.measure(visibility(r, pr, cam, d, n, e, run, 1, 0, clip, setup=setup), lit, dark)
            rows.append(dict(check='slots', layout=count, slot=slot, retention_model1=kept['retention'], retention_model0=lost['retention'],
                             mean_lit_model1=kept['mean_lit_visibility']))
            if (kept['retention'] < LIMITS['retention_kept'] or kept['mean_lit_visibility'] < LIMITS['mean_visibility'] or
                    lost['retention'] > LIMITS['retention_lost']):
                failures.append('%d-slot layout, slot %d does not take the fade model' % (count, slot))
    return rows, failures


def lighting_wiring(r, defaults):
    """The production spotLightF program must take the model (not only the fixture's own caller)."""
    scene, cam, g, d, n = block_fixture(100., near=.5, far=512.)
    e = light_entry(False)
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    lit, dark = classes(scene, g, e, 2048)
    original = r.native_uniforms
    images = {}
    try:
        for fade in (0, 1):
            def patched(pr, cam_, e_, defaults_, fade=fade):
                original(pr, cam_, e_, defaults_)
                r.gl.ui(r.gl.loc(pr, b'spot_shadow_bias_model'), 1)
                r.gl.ui(r.gl.loc(pr, b'spot_shadow_fade_model'), fade)
            r.native_uniforms = patched
            images[fade] = r.lighting(cam, d, n, e, 0, np.ones((cam.H, cam.W), np.float32), True, defaults)
    finally:
        del r.native_uniforms
    lum = {k: v @ R.LUMA for k, v in images.items()}
    return dict(lit_pixels=int(lit.sum()), shadow_pixels=int(dark.sum()), lit_median=float(np.median(lum[1][lit])),
                shadow_dark_model0=float((lum[0][dark] < .5 * np.median(lum[0][lit])).mean()),
                shadow_dark_model1=float((lum[1][dark] < .5 * np.median(lum[1][lit])).mean()))


def reverse_fade_layer(shader):
    """Remove this feature's exact insertions from shadowUtil.glsl (each must be present exactly once)."""
    return B.reverse_fade_layer(shader)


def source_contracts(tree):
    shader = tree.read(SHADER)
    base = G.SourceTree(BASE).read(SHADER)
    sample = function(shader, 'float sampleSpotShadow(')
    settings = settings_defaults()
    pipeline = tree.read('indra/newview/pipeline.cpp')
    shaders = 'indra/newview/app_settings/shaders/'
    clip_body = function(pipeline, 'LLVector4 LLPipeline::nativeSpotShadowClip() const')
    upload = function(pipeline, 'void LLPipeline::bindSunShadowState(')
    untouched = ('class3/deferred/areaShadowF.glsl', 'class3/deferred/areaShadowFilterF.glsl', 'class3/deferred/spotLightF.glsl',
                 'class3/deferred/projectorVolumetricF.glsl', 'class1/deferred/froxelInjectF.glsl',
                 'class1/deferred/projectorVolumetricClassicShadowUtil.glsl', 'class1/deferred/projectorVolumetricClassicShadowUtil16.glsl')
    header = 'indra/newview/alareashadowmath.h'
    try:
        reversed_ok = reverse_fade_layer(shader) == base
    except AssertionError:
        reversed_ok = False
    return {
        # The whole feature is four exact insertions; removing them reproduces BASE byte for byte, so
        # every legacy statement (and every function outside sampleSpotShadow) survives verbatim.
        'shader_is_base_plus_the_exact_layer': reversed_ok,
        'shader_default_is_legacy': shader.count(UNIFORM) == 1,
        'gate_is_surface_only': sample.count('    bool map_bounded = false;\n' + GATE + '    {\n'
                                                '        vec4 mp = shadow_matrix[4+index]*vec4(original_pos,1.0);\n' + BOUNDS + '    }\n') == 1 and
                                shader.count('map_bounded =') == 2 and
                                sample.index(GATE) < sample.index(EARLY) < sample.index(ZERO) < sample.index(WEIGHT) < sample.index(RAMP),
        'model_read_only_in_sampleSpotShadow': shader.count('spot_shadow_fade_model') == 2 and sample.count('spot_shadow_fade_model') == 1 and
                                               shader.count('map_bounded') == sample.count('map_bounded') == 5,
        'conservative_samplers_untouched': 'map_bounded' not in function(shader, 'float pcfSpotShadowConservative(') +
                                           function(shader, 'float sampleSpotShadowConservative('),
        'other_shaders_and_frozen_copies_untouched': all(tree.read(shaders + name) == G.SourceTree(BASE).read(shaders + name) for name in untouched),
        'handover_header_untouched': tree.read(header) == G.SourceTree(BASE).read(header),
        'setting_default_on': settings.get('RenderSpotShadowFadeModel') == 1,
        'pipeline_upload': pipeline.count('"RenderSpotShadowFadeModel", 1);') == 2 and
                           upload.count('shader.uniform1i(LLStaticHashedString("spot_shadow_fade_model"), (S32)llmin((U32)spot_shadow_fade_model, 1U));') == 1 and
                           pipeline.count('LLStaticHashedString("spot_shadow_fade_model")') == 1,
        # The hand-over reads the same setting: all zeros = "valid everywhere" for the ranking, the gather and its filter.
        'handover_clip_follows_the_model': clip_body.count('if (spot_shadow_fade_model != 0) return LLVector4(0.f, 0.f, 0.f, 0.f);') == 1 and
                                           clip_body.index('if (spot_shadow_fade_model != 0)') < clip_body.index('const RenderTargetPack& pack') and
                                           pipeline.count(' = nativeSpotShadowClip();') == 2,
    }


def allowance_contracts(tree):
    """pipeline.cpp is byte-pinned: this feature is exactly the two hunks of softshadow_fade_hunks.json on BASE."""
    from director_actor_bar_check import reverse_softshadow_fade, reverse_softshadow_bias
    pipeline = tree.read('indra/newview/pipeline.cpp').replace('\r\n', '\n')
    base = G.SourceTree(BASE).read('indra/newview/pipeline.cpp').replace('\r\n', '\n')
    hunks = json.loads((Path(__file__).with_name('softshadow_fade_hunks.json')).read_text(encoding='utf-8'))['files']['pipeline.cpp']

    def accepted(text, reverse=reverse_softshadow_fade):
        try:
            return reverse(text, 'pipeline.cpp') == base
        except AssertionError:
            return False
    checks = {'exact_reversal_to_base': accepted(pipeline), 'two_hunks': len(hunks) == 2,
              # The older layers peel this one first: their own exact reversal still sees its complete baseline.
              'chained_before_the_bias_layer': reverse_softshadow_bias(pipeline, 'pipeline.cpp') ==
                                               G.SourceTree('fb616f2519c').read('indra/newview/pipeline.cpp').replace('\r\n', '\n'),
              'rejects_unrelated_edit': not accepted(pipeline + '\n// planted unrelated edit\n'),
              'other_files_pass_through': reverse_softshadow_fade('unchanged', 'pipeline.h') == 'unchanged'}
    for index, hunk in enumerate(hunks):
        assert pipeline.count(hunk['after']) == 1, 'allowance hunk present once'
        checks['rejects_missing_hunk_%d' % index] = not accepted(pipeline.replace(hunk['after'], hunk['before'], 1))
        checks['rejects_duplicate_hunk_%d' % index] = not accepted(pipeline + hunk['after'])
        checks['rejects_edited_hunk_%d' % index] = not accepted(pipeline.replace(hunk['after'], hunk['after'] + '// planted edit\n', 1))
    return checks


def evaluate(out, negatives=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    defaults = settings_defaults()
    r = R.Runner('WORKTREE')
    print('[spot-fade] GPU', r.gl.renderer, flush=True)
    pr = r.visibility
    base = B.link(r, G.SourceTree(BASE), 'BASE visibility')
    failures = []
    contracts = source_contracts(r.tree)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    allowance = allowance_contracts(r.tree)
    failures += ['pipeline allowance ' + k for k, ok in allowance.items() if not ok]
    long_lens, long_failures = long_lens_check(r, pr, defaults)
    clips, clips_failures = clips_check(r, pr, defaults)
    identity = identity_check(r, pr, base, r.tree, defaults)
    zero, zero_failures = zero_check(r, r.tree, defaults)
    slots, slots_failures = slots_check(r, r.tree, defaults)
    edges, edges_failures = edges_check(r, pr, r.tree, defaults)
    wiring = lighting_wiring(r, defaults)
    if not (wiring['shadow_dark_model0'] < .05 and wiring['shadow_dark_model1'] > .9 and wiring['lit_median'] > 0.):
        failures.append('production spotLightF does not take the fade model: %s' % wiring)
    shaft, shaft_failures, shaft_base = shaft_check(r.gl, r.tree)
    shaft_looks, looks_failures, _ = shaft_looks_check(r.gl, r.tree, shaft_base)
    shaft_failures = shaft_failures + looks_failures
    failures += long_failures + clips_failures + ['identity ' + k for k, ok in identity.items() if not ok]
    failures += zero_failures + slots_failures + edges_failures + shaft_failures
    result = dict(base=BASE, limits=LIMITS, contracts=contracts, allowance=allowance, long_lens=long_lens, clips=clips, identity=identity, zero=zero,
                  slots=slots, edges=edges, spotLightF_wiring=wiring, shaft=shaft, shaft_looks=shaft_looks)
    if negatives:
        rejected = {}
        short = dict(kernels=B.KERNELS[:1], bias_models=(1,))
        for name, (edits, target) in MUTANTS.items():
            tree = B.MutantTree(edits)
            mutant = B.link(r, tree, name)
            if target == 'shaft':
                caught = bool(shaft_check(r.gl, tree, shaft_base)[1])
            elif target == 'shaft_looks':
                caught = bool(shaft_looks_check(r.gl, tree, shaft_base)[1])
            elif target == 'long_lens':
                caught = bool(long_lens_check(r, mutant, defaults, **short)[1])
            elif target == 'clips':
                caught = bool(clips_check(r, mutant, defaults, **short)[1])
            elif target == 'zero':
                caught = bool(zero_check(r, tree, defaults)[1])
            elif target == 'edges':
                caught = bool(edges_check(r, mutant, tree, defaults, **short)[1])
            else:
                caught = not all(identity_check(r, mutant, base, tree, defaults).values())
            rejected[name] = caught
            print('[spot-fade]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)
        # Instrument negatives: the limits themselves must be able to fail.
        for name, (edits, target) in (('contract_layer_edited', ([(WEIGHT, WEIGHT + '            w = w;\n')], None)),):
            try:
                ok = reverse_fade_layer(B.MutantTree(edits).read(SHADER)) == G.SourceTree(BASE).read(SHADER)
            except AssertionError:
                ok = False
            rejected[name] = not ok
            print('[spot-fade]', 'PASS rejected' if not ok else 'FAIL survived', 'planted regression:', name, flush=True)
            if ok:
                failures.append('planted regression survived: ' + name)
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    for row in long_lens:
        if row['kernel'] == 'hard' and row['bias_model'] == 1:
            print('[spot-fade] long lens', json.dumps(row), flush=True)
    print('[spot-fade] clips worst rounding', max(max(x['weighted_max_difference'], x['blend_max_difference']) for x in clips),
          'legacy ramp lift', min(x['ramp_legacy_lift'] for x in clips), flush=True)
    print('[spot-fade] wiring', json.dumps(wiring), flush=True)
    print('[spot-fade]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_spot_fade')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
