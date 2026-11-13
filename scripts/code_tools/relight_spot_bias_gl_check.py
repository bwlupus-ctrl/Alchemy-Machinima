"""Projector receiver bias model (RenderSpotShadowBiasModel) on production GL.

Real shadowV/F projector depth raster and the unchanged shadowUtil.glsl sampler
(sampleSpotShadow), driven through rig_shadow_gl.Runner. Ground truth is an
independent ray cast of the same analytic scene, never the rendering.

  strap     1 cm thick strap 1 cm above a plane, frontal projector, 2048 and 4096:
            the new model must keep its shadow, the legacy model (OFF) must lose it.
  sweep     a lit plane at 0..80 degrees incidence must show no acne (new model,
            ordinary and rig Detail projectors, hard / Poisson / Vogel kernels).
  edges     camera silhouettes of the strap: no leak or acne blocks beside them, and the
            result of a pixel must not depend on its screen neighbours (no derivatives).
  camera    a receiver 100 m from the camera (millimetres of G-buffer depth error) lit by a
            close, fine-texel projector must not acne.
  identity  OFF and every zero-normal (airborne) call are byte-identical to BASE.
  shaft     the New shaft engine's surface rim (a REAL normal) is byte-identical to BASE for
            every model: rim RGB, a marched frame with the rim, and the half-res march.
  calls     several sampleSpotShadow calls in ONE fragment (ordinary surface, rig Detail,
            zero normal, the receiver's early return, in both orders) each equal the same
            call made alone: the per-call globals never leak.
  widen     a slanted receiver under a far-separated blocker drives the soft kernel past one
            texel (pr > 1): the widening bias must be necessary and sufficient there.
  negatives planted shader regressions must each be rejected by the checks above.
  normal    a shading normal tilted 10 degrees off the geometry (a normal map) must not
            acne either, including coarse texels at grazing light; 20 degrees is reported.
"""
import argparse
import json
import math
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import rig_shadow_gl as R
from rig_shadow_presets_check import function, settings_defaults

BASE = 'fb616f2519c'
SHADER = 'indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl'
LIMITS = dict(retention_on=.90, retention_off=.05, mean_visibility=.98, false_dark_percent=.02,
              edge_error_percent=1., expected_pixels=150)
KERNELS = (('hard', False, False), ('poisson', True, False), ('vogel', True, True))
# RenderSpotShadowBiasModel: 1 (default) leaves rig Detail lights on their own receiver; 2 puts every
# projector on the surface model. The measurements below use 2 so 'detail' rows exercise the model.
MODEL_DEFAULT, MODEL_ALL = 1, 2


# The projector fade model (RenderSpotShadowFadeModel, relight_spot_fade_gl_check.py) is a later, separately
# reviewed layer of shadowUtil.glsl. It is EXACTLY the hunks of softshadow_fade_shader_hunks.json on FADE_BASE
# (pinned by digest, like the pipeline allowance layers): the contracts below are stated on the shader with that
# layer reversed, and the reversal must reproduce FADE_BASE byte for byte. Nothing is removed by position or by
# marker: a hunk is replaced only where its complete reviewed text is present, exactly once.
FADE_BASE = 'c63f7ca1b2f'
FADE_SHADER_DIGEST = 'd1d19fa71ae288d9b8287ab679bf2c8420d31217b51c98146878865f0b39a900'


def reverse_fade_layer(shader):
    """shadowUtil.glsl with the fade-model layer reversed to FADE_BASE; any other difference fails."""
    import hashlib
    raw = Path(__file__).with_name('softshadow_fade_shader_hunks.json').read_bytes().replace(b'\r\n', b'\n')
    assert hashlib.sha256(raw).hexdigest() == FADE_SHADER_DIGEST, 'fade shader layer: allowance pin'
    layer = json.loads(raw)
    assert layer['base'] == FADE_BASE and layer['file'] == SHADER and len(layer['hunks']) == 6, 'fade shader layer: scope'
    original = G.SourceTree(FADE_BASE).read(SHADER).replace('\r\n', '\n')
    shader = shader.replace('\r\n', '\n')
    for hunk in layer['hunks']:
        assert original.count(hunk['before']) == 1, 'fade shader layer: baseline hunk'
        assert shader.count(hunk['after']) == 1, ('fade shader layer: exact reviewed hunk', hunk['after'][:80])
        shader = shader.replace(hunk['after'], hunk['before'], 1)
    assert shader == original, 'fade shader layer: reversal must reproduce its exact baseline'
    return shader


def fade_layer_negatives(shader):
    """Planted edits of the fade layer; every one must be refused by reverse_fade_layer. Returns the survivors."""
    uniform = 'uniform int spot_shadow_fade_model = 0;\n'
    gate = '    if (spot_shadow_fade_model != 0 && projvol_rim_strength <= 0.0 && dot(norm,norm) > 0.0)\n'
    weight = '            if (map_bounded) w = 1.0;\n'
    planted = {
        # Executable text inside the uniform's comment block (it would change the sun bias of every program).
        'define_inside_the_uniform_block': (uniform, '#define shadow_bias 0.0\n' + uniform),
        'define_at_the_top_of_the_uniform_block': ('// RenderSpotShadowFadeModel.\n', '// RenderSpotShadowFadeModel.\n#define shadow_bias 0.0\n'),
        'statement_inside_the_gate': (gate, '    pos.z += 0.0;\n' + gate),
        'edited_statement': (weight, weight.replace('1.0', '2.0')),
        'missing_statement': (weight, ''),
        'duplicated_statement': (weight, weight * 2),
        'comment_edited': ('// [SoftShadow fade] Model 1: a weight of exactly 1,', '// [SoftShadow fade] Model 1: a weight of exactly 2,'),
        'unrelated_edit_elsewhere': ('uniform float shadow_bias;\n', 'uniform float shadow_bias;\nuniform float planted_unrelated;\n'),
    }
    survivors = []
    for name, (old, new) in planted.items():
        assert shader.count(old) == 1, ('fade layer negative anchor', name)
        try:
            reverse_fade_layer(shader.replace(old, new, 1))
        except AssertionError:
            continue
        survivors.append(name)
    return survivors


class MutantTree(G.SourceTree):
    """The worktree with exact, single-site edits of shadowUtil.glsl (planted negatives)."""
    def __init__(self, edits):
        super().__init__('WORKTREE')
        self.edits = edits

    def read(self, path):
        text = super().read(path)
        if Path(path).name == 'shadowUtil.glsl':
            for before, after in self.edits:
                assert text.count(before) == 1, ('mutation drift', before)
                text = text.replace(before, after)
        return text


ANGLE = '    float cos_a = min(abs(dot(n,ray))/ray_len,1.0);\n'
# The receiver plane from screen derivatives of the map position instead of the G-buffer normal.
DERIVATIVE_ANGLE = '''    vec3 sdn = cross(dFdx(pos),dFdy(pos));
    float cos_a = dot(sdn,sdn) > 0.0 ? min(abs(dot(normalize(sdn),ray))/ray_len,1.0) : 1.0;
'''
OFFSET = 'float offset = max(clamp(reach*sin_a,g_spot_bias_floor,ceiling),camera);'
GATE = ('if (spot_shadow_bias_model != 0 && projvol_rim_strength <= 0.0 && dot(norm,norm) > 0.0 && '
        '(spot_shadow_bias_model == 2 || !detail))')
RESET = '    g_spot_bias_mode = 0;\n'
WIDEN = '        stc.z -= (pr - 1.0) * g_spot_widen_stc;\n'
BIAS = 'max(floor_m,deficit*reach*tan_s)*ray_stc'
MUTANTS = {
    # name: (single-site edits, the check that must reject it)
    'offset_not_texel_scaled': ([(OFFSET, 'float offset = spot_shadow_offset;')], 'strap'),
    'screen_derivative_plane': ([(ANGLE, DERIVATIVE_ANGLE)], 'edges'),
    'zero_normal_enters_model': ([(GATE, GATE.replace(' && dot(norm,norm) > 0.0', ''))], 'identity'),
    'no_receiver_plane_bias': ([(BIAS, 'floor_m*ray_stc')], 'sweep'),
    'offset_ignores_incidence': ([(OFFSET, 'float offset = max(g_spot_bias_floor,camera);'), (BIAS, 'floor_m*ray_stc')], 'sweep'),
    'no_camera_precision_term': ([('SPOT_BIAS_CAMERA_QUANTA*1.1920929e-7*abs(inv_proj[2][3]*pos.z)*length(pos)', '0.0')], 'camera'),
    'shaft_rim_enters_model': ([(GATE, GATE.replace(' && projvol_rim_strength <= 0.0', ''))], 'shaft'),
    'mode_not_reset_per_call': ([(RESET, '')], 'calls'),
    'no_soft_widening': ([(WIDEN, '')], 'widen'),
    'no_normal_slack': ([('const vec2  SPOT_BIAS_NORMAL_SLACK = vec2(0.98480775, 0.17364818);',
                          'const vec2  SPOT_BIAS_NORMAL_SLACK = vec2(1.0, 0.0);')], 'normal'),
}


def link(r, tree, label):
    saved = r.tree
    r.tree = tree
    try:
        return r.link_text(r.visibility_source, label, extra=r.FRAGMENT_COMMON + ['deferred/shadowUtil.glsl'],
                           defines='#define SPOT_SHADOW 1\n')
    finally:
        r.tree = saved


def visibility(r, pr, cam, d, n, e, defaults, model, airborne=False, vogel=False, clip=None, setup=None):
    gl = r.gl
    r.target_float(cam)
    r._begin(pr)
    r.inputs(pr, cam, d, n)
    r.native_uniforms(pr, cam, e, defaults)
    gl.ui(gl.loc(pr, b'probe_native'), 1)
    gl.ui(gl.loc(pr, b'probe_airborne'), int(airborne))
    gl.ui(gl.loc(pr, b'soft_shadow_vogel'), int(vogel))
    gl.ui(gl.loc(pr, b'soft_shadow_taps'), 16)
    # Absent in BASE (location -1): GL ignores the upload, exactly like the viewer.
    gl.ui(gl.loc(pr, b'spot_shadow_bias_model'), int(model))
    if clip:
        # sampleSpotShadow fades the projector shadow out over the sun cascade range.
        r.uniform(pr, 'shadow_clip', *clip)
    if setup:
        setup(pr)
    return r.read_float(cam)[..., 0]


def entry(pos, fov, detail):
    return dict(pos=np.asarray(pos, float), radius=.01, range=19.8, fov=fov, falloff=.5, color=np.ones(3),
                softness=0., detail_shadows=detail)


def light_of(e):
    return S.Light('bias key', e['pos'], .01, 19.8, 'projector', S.unit(R.CENTRE - e['pos']),
                   math.degrees(e['fov']) / 2, seed=23)


def truth(scene, g, light_pos, margin):
    """Independent lit / shadowed classes, stable over a `margin` metre receiver neighbourhood."""
    valid = g['surf'] != 255
    p = g['pos'][valid].astype(np.float64)
    n = g['nrm'][valid].astype(np.float64)

    def clear(q):
        ray = light_pos - q
        dist = np.linalg.norm(ray, axis=1)
        ray /= dist[:, None]
        ok = np.ones(len(q), bool)
        for prim in scene.prims:
            ok &= S.isect(prim, q, ray) >= dist
        return ok

    base = clear(p + n * .0005)
    to_light = light_pos - p
    nl = (n * to_light).sum(1) / np.linalg.norm(to_light, axis=1)
    axis = np.zeros_like(n)
    axis[:, 2] = 1.
    axis[np.abs(n[:, 2]) > .9] = (0, 1, 0)
    tangent = np.cross(n, axis)
    tangent /= np.linalg.norm(tangent, axis=1, keepdims=True)
    bitangent = np.cross(n, tangent)
    stable = np.ones(len(p), bool)
    for x, y in ((-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)):
        stable &= clear(p + margin * (x * tangent + y * bitangent) + n * .0005) == base
    lit = np.zeros(valid.shape, bool)
    dark = np.zeros(valid.shape, bool)
    lit[valid] = stable & base & (nl > .2)
    dark[valid] = stable & ~base & (nl > .2)
    return lit, dark


def texel_at(e, res):
    distance = float(np.linalg.norm(R.CENTRE - e['pos'])) + .125 / math.tan(e['fov'] * .5) - .125
    return 2. * distance * math.tan(e['fov'] * .5) / res


def strap_fixture():
    # Plane x = 0 facing +x; a strap 1 cm thick, 3 cm wide, its underside 1 cm above the plane.
    scene = S.Scene('strap over plane', [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 4, 4)),
                                         S.Prim('strap', 'obb', c=(.015, 0, 1.45), h=(.005, .015, .6))],
                    [], [], {}, np.empty((0, 3)))
    # 60 degrees off the plane normal, so the shadow under the strap is in view.
    cam = S.Camera(R.CENTRE + 2.5 * np.array([.5, math.sqrt(.75), 0.]), R.CENTRE, 640, 320, 4)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    edge = np.zeros(g['surf'].shape, bool)
    for axis in (0, 1):
        for shift in (-1, 1):
            edge |= np.linalg.norm(g['pos'] - np.roll(g['pos'], shift, axis), axis=2) > .004
    band = edge.copy()
    for axis in (0, 1):
        for shift in (-1, 1):
            band |= np.roll(edge, shift, axis)
    band[:2] = band[-2:] = False
    band[:, :2] = band[:, -2:] = False
    # The same G-buffer with every other pixel (a checkerboard) replaced by sky. A receiver
    # bias that is a function of the pixel alone gives the surviving pixels the identical
    # value; anything built on screen derivatives sees a different 2x2 quad and does not.
    y, x = np.mgrid[:cam.H, :cam.W]
    hole = (x + y) % 2 == 0
    d2, n2 = d.copy(), n.copy()
    d2[hole] = 0xFFFFFFFF
    n2[hole] = (32768, 32768, 0, 0)
    keep = ~hole & (g['surf'] != 255)
    return scene, cam, g, d, n, band, (d2, n2, keep)


def measure(v, lit, dark):
    return dict(expected_dark=int(dark.sum()), clear_pixels=int(lit.sum()),
                retention=float((v[dark] < .5).mean()) if dark.any() else None,
                mean_lit_visibility=float(v[lit].mean()) if lit.any() else None,
                false_dark_percent=float((v[lit] < .5).mean() * 100) if lit.any() else None)


def strap_check(r, pr, defaults, resolutions=(2048, 4096), edges=True):
    """Returns (rows, failures, edge_failures)."""
    scene, cam, g, d, n, band, (d2, n2, keep) = strap_fixture()
    rows, failures, edge_failures = [], [], []
    for res in resolutions:
        for detail in (False, True):
            e = entry(R.CENTRE + np.array([3., 0, 0]), 1., detail)
            lit, dark = truth(scene, g, e['pos'], 4. * texel_at(e, res))
            r.native_map(scene, light_of(e), e['fov'], res)
            for kernel, soft, vogel in KERNELS:
                run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
                for model in (MODEL_ALL, 0):
                    v = visibility(r, pr, cam, d, n, e, run, model, vogel=vogel)
                    row = dict(check='strap', resolution=res, path='detail' if detail else 'standard', kernel=kernel,
                               model=model, texel_mm=texel_at(e, res) * 1000, **measure(v, lit, dark))
                    label = f"{res} {row['path']} {kernel} model {model}"
                    if row['expected_dark'] < LIMITS['expected_pixels'] or row['clear_pixels'] < LIMITS['expected_pixels']:
                        failures.append(label + ': instrument has too few witnesses')
                    if model:
                        if row['retention'] < LIMITS['retention_on']:
                            failures.append(label + ': strap shadow lost')
                        if row['mean_lit_visibility'] < LIMITS['mean_visibility'] or row['false_dark_percent'] > LIMITS['false_dark_percent']:
                            failures.append(label + ': acne')
                        # Camera silhouettes: lit receivers stay lit, shadowed ones stay dark.
                        el, ed = lit & band, dark & band
                        row.update(edge_lit_pixels=int(el.sum()), edge_dark_pixels=int(ed.sum()),
                                   edge_false_dark_percent=float((v[el] < .75).mean() * 100) if el.any() else None,
                                   edge_false_lit_percent=float((v[ed] > .25).mean() * 100) if ed.any() else None)
                        alone = visibility(r, pr, cam, d2, n2, e, run, model, vogel=vogel)
                        row['neighbour_independent'] = bool(np.array_equal(alone[keep], v[keep]))
                        if edges:
                            if not row['neighbour_independent']:
                                edge_failures.append(label + ': result depends on neighbouring pixels (screen derivatives)')
                            if el.sum() < 100 or ed.sum() < 30:
                                edge_failures.append(label + ': too few silhouette witnesses')
                            elif (row['edge_false_dark_percent'] > LIMITS['edge_error_percent'] or
                                  row['edge_false_lit_percent'] > LIMITS['edge_error_percent']):
                                edge_failures.append(label + ': silhouette leak/acne')
                    else:
                        # Informational: the legacy rig Detail receiver is built on screen derivatives.
                        alone = visibility(r, pr, cam, d2, n2, e, run, model, vogel=vogel)
                        row['neighbour_independent'] = bool(np.array_equal(alone[keep], v[keep]))
                    if not model and not detail and row['retention'] > LIMITS['retention_off']:
                        # The legacy 4 cm offset lifts the lookup above the strap. If this ever
                        # passes, the fixture no longer discriminates the two models.
                        failures.append(label + ': legacy model unexpectedly kept the strap shadow')
                    rows.append(row)
    return rows, failures, edge_failures


def sweep_check(r, pr, defaults, resolutions=(2048, 4096), distances=(3.,), angles=range(0, 81, 10), tilt=0., rolls=(0., 45.)):
    """Lit plane, incidence sweep. `tilt` degrees rotates only the G-buffer normal (a normal map).

    `rolls` turns the plane of incidence about the receiver normal, so the receiver's slope
    falls along a map axis (0) and along the map diagonal (45), where the kernels reach furthest.
    """
    scene = S.Scene('lit plane', [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6))], [], [], {}, np.empty((0, 3)))
    cam = S.Camera((3.5, 0, 1.45), R.CENTRE, 320, 320, 12)
    g = S.cast_primary(scene, cam)
    if tilt:
        t = math.radians(tilt)
        rot = np.array([[math.cos(t), -math.sin(t), 0], [math.sin(t), math.cos(t), 0], [0, 0, 1]])
        g = dict(g, nrm=(g['nrm'] @ rot.T).astype(np.float32))
    _, d, n = S.encode_gbuffer(cam, g)
    mask = (g['surf'] == 0) & (np.abs(g['pos'][..., 1]) < .12) & (np.abs(g['pos'][..., 2] - 1.45) < .12)
    assert mask.sum() > 1000
    rows, failures = [], []
    for distance in distances:
        for angle, roll in ((angle, roll) for angle in angles for roll in (rolls if angle else rolls[:1])):
            a, b = math.radians(angle), math.radians(roll)
            pos = R.CENTRE + distance * np.array([math.cos(a), math.sin(a) * math.cos(b), math.sin(a) * math.sin(b)])
            for res in resolutions:
                r.native_map(scene, light_of(entry(pos, 1.5, False)), 1.5, res)
                for detail in (False, True):
                    e = entry(pos, 1.5, detail)
                    for kernel, soft, vogel in KERNELS:
                        run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
                        v = visibility(r, pr, cam, d, n, e, run, MODEL_ALL, vogel=vogel)
                        row = dict(check='sweep', distance_m=distance, angle=angle, roll=roll, resolution=res, tilt=tilt,
                                   path='detail' if detail else 'standard', kernel=kernel,
                                   mean_visibility=float(v[mask].mean()), min_visibility=float(v[mask].min()),
                                   false_dark_percent=float((v[mask] < .5).mean() * 100))
                        if row['mean_visibility'] < LIMITS['mean_visibility'] or row['false_dark_percent'] > LIMITS['false_dark_percent']:
                            failures.append(row)
                        rows.append(row)
    return rows, failures


def neighbour_check(r, pr, defaults):
    """Coarse texels at grazing incidence, where the receiver-plane term IS the bias.

    Every other pixel of the G-buffer becomes sky. A per-pixel receiver gives the surviving
    pixels the identical value (and stays lit); a screen-derivative plane sees a different
    2x2 quad and loses its slope.
    """
    scene = S.Scene('lit plane', [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6))], [], [], {}, np.empty((0, 3)))
    cam = S.Camera((3.5, 0, 1.45), R.CENTRE, 320, 320, 12)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    y, x = np.mgrid[:cam.H, :cam.W]
    hole = (x + y) % 2 == 0
    d2, n2 = d.copy(), n.copy()
    d2[hole] = 0xFFFFFFFF
    n2[hole] = (32768, 32768, 0, 0)
    keep = ~hole & (g['surf'] == 0) & (np.abs(g['pos'][..., 1]) < .12) & (np.abs(g['pos'][..., 2] - 1.45) < .12)
    rows, failures = [], []
    for angle in (60, 70, 80):
        a = math.radians(angle)
        e = entry(R.CENTRE + 8. * np.array([math.cos(a), math.sin(a), 0]), 1.5, False)
        r.native_map(scene, light_of(e), e['fov'], 2048)
        for kernel, soft, vogel in KERNELS:
            run = dict(defaults, BDMergeSoftProjectorShadows=soft)
            full = visibility(r, pr, cam, d, n, e, run, MODEL_ALL, vogel=vogel)
            alone = visibility(r, pr, cam, d2, n2, e, run, MODEL_ALL, vogel=vogel)
            row = dict(check='neighbour', angle=angle, kernel=kernel, pixels=int(keep.sum()),
                       identical=bool(np.array_equal(full[keep], alone[keep])),
                       mean_visibility=float(alone[keep].mean()))
            if not row['identical'] or row['mean_visibility'] < LIMITS['mean_visibility']:
                failures.append(f"{angle}deg {kernel}: result depends on neighbouring pixels (screen derivatives)")
            rows.append(row)
    return rows, failures


def normal_check(r, pr, defaults, tilts=(-10., 10.)):
    """A uniformly tilted shading normal over flat geometry, fine and coarse (clamped) texels."""
    rows = []
    for tilt in tilts:
        rows += sweep_check(r, pr, defaults, resolutions=(1024, 2048), distances=(3., 8.), angles=(0, 30, 60, 80),
                            tilt=tilt, rolls=(0.,))[0]
    failures = [row for row in rows if row['mean_visibility'] < LIMITS['mean_visibility'] or
                row['false_dark_percent'] > LIMITS['false_dark_percent']]
    return rows, failures


def camera_check(r, pr, defaults):
    """Lit plane seen from 100 m: the 24-bit G-buffer depth puts the reconstructed receiver up to
    a few millimetres off the surface, more than the model's floors under a fine-texel projector."""
    scene = S.Scene('lit plane', [S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6))], [], [], {}, np.empty((0, 3)))
    cam = S.Camera(R.CENTRE + np.array([95., 30., 10.]), R.CENTRE, 320, 320, .15, .25, 256.)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    mask = (g['surf'] == 0) & (np.abs(g['pos'][..., 1]) < .1) & (np.abs(g['pos'][..., 2] - 1.45) < .1)
    assert mask.sum() > 1000
    step_mm = 1000. * 100. ** 2 * (cam.far - cam.near) / (cam.far * cam.near) / 16777215.
    rows, failures = [], []
    for angle in (0, 45, 70):
        a = math.radians(angle)
        pos = R.CENTRE + 2. * np.array([math.cos(a), math.sin(a), 0])
        r.native_map(scene, light_of(entry(pos, 1., False)), 1., 4096)
        for detail in (False, True):
            e = entry(pos, 1., detail)
            for kernel, soft, vogel in KERNELS:
                run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
                v = visibility(r, pr, cam, d, n, e, run, MODEL_ALL, vogel=vogel, clip=(64, 128, 256, 512))
                row = dict(check='camera', angle=angle, path='detail' if detail else 'standard', kernel=kernel,
                           depth_step_mm=step_mm, texel_mm=texel_at(e, 4096) * 1000,
                           mean_visibility=float(v[mask].mean()), false_dark_percent=float((v[mask] < .5).mean() * 100))
                if row['mean_visibility'] < LIMITS['mean_visibility'] or row['false_dark_percent'] > LIMITS['false_dark_percent']:
                    failures.append(f"far camera acne {angle}deg {row['path']} {kernel}")
                if detail:
                    # Informational: the legacy rig Detail receiver (kept by the default model 1) at this distance.
                    old = visibility(r, pr, cam, d, n, e, run, 0, vogel=vogel, clip=(64, 128, 256, 512))
                    row['legacy_detail_false_dark_percent'] = float((old[mask] < .5).mean() * 100)
                rows.append(row)
    return rows, failures


def identity_check(r, pr, base, defaults):
    """OFF == BASE on surfaces; airborne (zero normal) == BASE with the model ON and OFF."""
    scene, cam, g, d, n, _, _ = strap_fixture()
    checks = {}
    e0 = entry(R.CENTRE + np.array([3., .8, .4]), 1., False)
    r.native_map(scene, light_of(e0), e0['fov'], 2048)
    for detail in (False, True):
        e = dict(e0, detail_shadows=detail)
        for kernel, soft, vogel in KERNELS:
            run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)
            name = ('detail' if detail else 'standard') + '_' + kernel
            reference = visibility(r, base, cam, d, n, e, run, 0, vogel=vogel)
            checks['off_surface_' + name] = bool(np.array_equal(visibility(r, pr, cam, d, n, e, run, 0, vogel=vogel), reference))
            air = visibility(r, base, cam, d, n, e, run, 0, airborne=True, vogel=vogel)
            for model in (0, MODEL_DEFAULT, MODEL_ALL):
                try:
                    current = visibility(r, pr, cam, d, n, e, run, model, airborne=True, vogel=vogel)
                    checks[f'airborne_model{model}_{name}'] = bool(np.array_equal(current, air))
                except AssertionError:   # non-finite output
                    checks[f'airborne_model{model}_{name}'] = False
            default = visibility(r, pr, cam, d, n, e, run, MODEL_DEFAULT, vogel=vogel)
            everything = visibility(r, pr, cam, d, n, e, run, MODEL_ALL, vogel=vogel)
            # Positive control: model 2 must actually change a lit surface for either kind of light.
            checks['model2_differs_' + name] = not np.array_equal(everything, reference)
            # Model 1 (default): ordinary projectors as model 2, rig Detail lights byte-identical to BASE.
            checks['model1_' + name] = bool(np.array_equal(default, reference if detail else everything))
    return checks


# Five calls in ONE fragment invocation. probe_order 0 / 1 runs them forwards / backwards and
# returns call probe_pick; probe_order 2 makes that one call alone.
#   0 ordinary projector on a surface      1 rig Detail projector on a surface (slot 1)
#   2 zero normal (airborne)               3 a slot whose matrix is degenerate: the receiver's early return
#   4 ordinary projector on a surface again, after everything else has run
CALLS_SOURCE = """
out vec4 frag_color;
uniform int probe_order;
uniform int probe_pick;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
float one(int k,vec3 p,vec3 n,vec2 tc)
{
    if (k == 0) return sampleSpotShadow(p,n,0,tc);
    if (k == 1) return sampleSpotShadow(p,n,1,tc);
    if (k == 2) return sampleSpotShadow(p,vec3(0),0,tc);
    if (k == 3) return sampleSpotShadow(p,n,3,tc);
    return sampleSpotShadow(p,n,0,tc);
}
void main()
{
    vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz;vec3 n=getNorm(tc).xyz;
    float result=-1.;
    if (probe_order == 2) result=one(probe_pick,p,n,tc);
    else for (int i=0;i<5;++i)
    {
        int k=probe_order == 0 ? i : 4-i;
        float v=one(k,p,n,tc);
        if (k == probe_pick) result=v;
    }
    frag_color=vec4(result);
}
"""


def calls_check(r, tree, defaults):
    """Per-call state: each call inside a multi-call fragment equals the same call made alone."""
    saved = r.tree
    r.tree = tree
    try:
        pr = r.link_text(CALLS_SOURCE, 'consecutive calls', extra=r.FRAGMENT_COMMON + ['deferred/shadowUtil.glsl'],
                         defines='#define SPOT_SHADOW 1\n')
    finally:
        r.tree = saved
    scene, cam, g, d, n, _, _ = strap_fixture()
    e = entry(R.CENTRE + np.array([3., .8, .4]), 1., False)
    r.native_map(scene, light_of(e), e['fov'], 2048)
    valid = g['surf'] != 255
    rows, failures = [], []
    for model in (MODEL_DEFAULT, MODEL_ALL):
        for kernel, soft, vogel in KERNELS[:2]:
            run = dict(defaults, RenderSpotShadowDetailBias=True, BDMergeSoftProjectorShadows=soft)

            def render(order, pick):
                def setup(pr):
                    tex, ncam, res = r.native
                    bias = np.eye(4); bias[:3, :3] *= .5; bias[:3, 3] = .5
                    r._mat(pr, b'shadow_matrix[5]', bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view))
                    r.uniform(pr, 'spot_shadow_detail[1]', 1.)   # slot 1 is a rig Detail light; slot 3 keeps a zero matrix
                    r.gl.ui(r.gl.loc(pr, b'probe_order'), order)
                    r.gl.ui(r.gl.loc(pr, b'probe_pick'), pick)
                return visibility(r, pr, cam, d, n, e, run, model, setup=setup)

            alone = [render(2, pick) for pick in range(5)]
            row = dict(check='calls', model=model, kernel=kernel,
                       # The fixture must really mix the paths: surface != airborne, and the two ordinary calls agree.
                       surface_differs_from_airborne=not np.array_equal(alone[0][valid], alone[2][valid]),
                       early_return_is_lit=bool(np.all(alone[3][valid] > .9999)),
                       detail_slot_shadows=bool((alone[1][valid] < .5).any()),
                       leaks=[])
            for order in (0, 1):
                for pick in range(5):
                    if not np.array_equal(render(order, pick)[valid], alone[pick][valid]):
                        row['leaks'].append([order, pick])
            if not (row['surface_differs_from_airborne'] and row['early_return_is_lit'] and row['detail_slot_shadows']):
                failures.append(f"model {model} {kernel}: the consecutive-call fixture does not exercise every path")
            if row['leaks']:
                failures.append(f"model {model} {kernel}: per-call state leaks between calls {row['leaks']}")
            rows.append(row)
    return rows, failures


def widen_check(r, pr, defaults):
    """A soft kernel wider than one texel on a slanted receiver.

    A card 0.8 m from the lamp shadows a plane 3 m away at 60 degrees incidence, so the blocker
    search returns a gap and the kernel grows to several texels. Reference: the same shader with
    an enormous bias in the widening term (15 cm and more per texel of radius): the plane cannot
    shadow itself there, and the card, 2 m nearer the lamp, is unaffected. The production term
    must reproduce that reference exactly.
    """
    a = math.radians(60.)
    toward = np.array([math.cos(a), math.sin(a), 0.])
    scene = S.Scene('card over slanted plane', [
        S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6)),
        S.Prim('card', 'obb', c=tuple(R.CENTRE + 2.2 * toward), h=(.004, .05, .05), yaw=60.)], [], [], {}, np.empty((0, 3)))
    cam = S.Camera((3.5, 0, 1.45), R.CENTRE, 480, 480, 20)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    plane = g['surf'] == 0
    e = dict(entry(R.CENTRE + 3. * toward, 1., False), softness=8.)
    r.native_map(scene, light_of(e), e['fov'], 2048)
    reference = link(r, MutantTree([(WIDEN, '        stc.z -= (pr - 1.0) * 0.004;\n')]), 'widen reference')
    radius = link(r, MutantTree([(WIDEN, WIDEN + '        if (true) return pr / 16.0;\n')]), 'kernel radius probe')
    rows, failures = [], []
    for kernel, soft, vogel in KERNELS[1:]:
        run = dict(defaults, BDMergeSoftProjectorShadows=soft)
        v = visibility(r, pr, cam, d, n, e, run, MODEL_ALL, vogel=vogel)
        want = visibility(r, reference, cam, d, n, e, run, MODEL_ALL, vogel=vogel)
        pr_map = visibility(r, radius, cam, d, n, e, run, MODEL_ALL, vogel=vogel) * 16.
        wide = plane & (pr_map > 1.5) & (pr_map < 15.9)
        row = dict(check='widen', kernel=kernel, wide_pixels=int(wide.sum()),
                   max_radius_texels=float(pr_map[wide].max()) if wide.any() else 1.,
                   shadowed_pixels=int((plane & (want < .05)).sum()), partial_pixels=int((plane & (want > .05) & (want < .95)).sum()),
                   max_difference=float(np.abs(v - want)[plane].max()),
                   differing_pixels=int((np.abs(v - want)[plane] > 1.e-6).sum()))
        if row['wide_pixels'] < 500 or row['max_radius_texels'] < 2. or row['shadowed_pixels'] < 500 or row['partial_pixels'] < 200:
            failures.append(f"{kernel}: fixture does not reach a kernel wider than one texel")
        if row['differing_pixels']:
            failures.append(f"{kernel}: slanted receiver shadows itself inside the wide kernel "
                            f"({row['differing_pixels']} px, up to {row['max_difference']:.3f})")
        rows.append(row)
    return rows, failures


def shaft_check(gl, tree, base_runner=None):
    """New shaft engine, surface rim on: byte-identical to BASE whatever the switch says."""
    import relight_volumetric_gl_check as V
    base_runner = base_runner or V.Runner(G.SourceTree(BASE), gl)
    current = V.Runner(tree, gl)
    fixture = V.fixtures(320, 180)['outside']
    passes = dict(rim_only=dict(steps=0, rim=1), march_with_rim=dict(steps=24, rim=1, dither=1),
                  half_res_march_beam_depth=dict(half=True, steps=24, rim=1, beam=1, dither=1))
    rows, failures = [], []
    for name, kwargs in passes.items():
        for vogel in (0, 1):
            # Every uniform is stated on both sides: programs keep their uniform values between draws.
            kernel = dict(soft_shadow_vogel=vogel, soft_shadow_taps=16)
            ref = base_runner.run(fixture, uniforms=dict(kernel, spot_shadow_bias_model=0), **kwargs)[:2]
            for model in (0, MODEL_DEFAULT, MODEL_ALL):
                got = current.run(fixture, uniforms=dict(kernel, spot_shadow_bias_model=model), **kwargs)[:2]
                same = all(np.array_equal(np.asarray(x), np.asarray(y)) for x, y in zip(got, ref))
                lit = int(np.count_nonzero(np.asarray(got[0])[:, :, :3].max(2) > 0))
                rows.append(dict(check='shaft', draw=name, model=model, vogel=vogel, identical=bool(same), lit_pixels=lit))
                if not same or lit < 100:
                    failures.append(f'shaft {name} model {model} vogel {vogel}: differs from BASE' if lit >= 100 else
                                    f'shaft {name}: fixture draws nothing')
    return rows, failures, base_runner


def lighting_wiring(r, defaults):
    """The production spotLightF program must actually take the model (not only the fixture's own caller)."""
    scene, cam, g, d, n, _, _ = strap_fixture()
    e = dict(entry(R.CENTRE + np.array([3., 0, 0]), 1., False))
    r.native_map(scene, light_of(e), e['fov'], 2048)
    original = r.native_uniforms
    images = {}
    try:
        for model in (0, MODEL_DEFAULT):
            def patched(pr, cam_, e_, defaults_, model=model):
                original(pr, cam_, e_, defaults_)
                r.gl.ui(r.gl.loc(pr, b'spot_shadow_bias_model'), model)
            r.native_uniforms = patched
            images[model] = r.lighting(cam, d, n, e, 0, np.ones((cam.H, cam.W), np.float32), True, defaults)
    finally:
        del r.native_uniforms
    lum = {k: v @ R.LUMA for k, v in images.items()}
    lit, dark = truth(scene, g, e['pos'], 4. * texel_at(e, 2048))
    return dict(strap_shadow_dark_model0=float((lum[0][dark] < .5 * np.median(lum[0][lit])).mean()),
                strap_shadow_dark_model1=float((lum[MODEL_DEFAULT][dark] < .5 * np.median(lum[MODEL_DEFAULT][lit])).mean()))


def source_contracts(tree):
    # The newer fade-model layer is reversed first, exactly (softshadow_fade_shader_hunks.json); everything
    # below is the reviewed bias contract on the remaining text.
    worktree_shader = tree.read(SHADER)
    try:
        shader = reverse_fade_layer(worktree_shader)
        fade_layer_exact = shader == G.SourceTree(FADE_BASE).read(SHADER)
        fade_layer_survivors = fade_layer_negatives(worktree_shader)
    except AssertionError as error:
        shader, fade_layer_exact, fade_layer_survivors = G.SourceTree(FADE_BASE).read(SHADER), False, [str(error)]
    # The other reviewed feature widens only dispatch/layout. Removing the bias
    # insertions must reproduce that exact feature tree, never arbitrary WORKTREE.
    # GPU model-0/airborne/rim parity below still uses the original common BASE.
    base = G.SourceTree('148442e419c').read(SHADER)
    sample = function(shader, 'float sampleSpotShadow(')
    new = function(shader, 'vec4 spotSurfaceReceiver(') + function(shader, 'float pcfSpotShadowSurface(')
    block = sample[sample.index('    // [SoftShadow bias]'):sample.index('\n    vec4 spos = vec4(pos,1.0);')]
    # Every legacy statement survives verbatim: removing the model's own insertions
    # from the three touched functions reproduces spot16, and the rest are untouched.
    verbatim = (
        sample.replace(block, '', 1) == function(base, 'float sampleSpotShadow(') and
        function(shader, 'float pcfSpotShadow(').replace(
            '    // [SoftShadow bias] surface model (never set for a zero-normal caller).\n'
            '    if (g_spot_bias_mode == 1) return pcfSpotShadowSurface(shadowMap, stc, pos_screen, projector_softness);\n', '', 1)
        == function(base, 'float pcfSpotShadow(') and
        function(shader, 'vec4 detailSpotReceiver(').replace(
            '    if (g_spot_bias_mode == 1) return spotSurfaceReceiver(pos,norm,index);\n', '', 1)
        == function(base, 'vec4 detailSpotReceiver(') and
        all(function(shader, name) == function(base, name) for name in (
            'float estimateSpotBlockerGap(', 'float detailSpotTap(', 'float estimateDetailSpotBlockerGap(',
            'float pcfSpotShadowConservative(', 'float sampleSpotShadowConservative(')))
    settings = settings_defaults()
    pipeline = tree.read('indra/newview/pipeline.cpp')
    shaders = 'indra/newview/app_settings/shaders/'
    shaft = tree.read(shaders + 'class3/deferred/projectorVolumetricF.glsl')
    froxel = tree.read(shaders + 'class1/deferred/froxelInjectF.glsl')
    calls = lambda text: [line for line in text.splitlines()
                          if 'sampleSpotShadow(' in line and not line.lstrip().startswith(('//', 'float sampleSpotShadow'))]
    rim_calls = [line for line in calls(shaft) if 'vec3(0.0)' not in line]
    conservative = function(shader, 'float pcfSpotShadowConservative(') + function(shader, 'float sampleSpotShadowConservative(')
    return {
        # The shaft program: every sampleSpotShadow call is zero-normal except the one surface rim,
        # which only runs under the program's own rim uniform -- the uniform the model is gated on.
        'shaft_rim_is_the_only_real_normal_call': len(rim_calls) == 1 and
            shaft.index('if (projvol_rim_strength > 0.0)') < shaft.index(rim_calls[0]) and
            all('vec3(0.0)' in line for line in calls(froxel)) and len(calls(froxel)) >= 1,
        'shaft_rim_uniform_shared': shader.count('uniform float projvol_rim_strength;') == 1 and
                                    'uniform float projvol_rim_strength;' in shaft,
        # Only sampleSpotShadow reaches the two functions that read the per-call mode, the
        # conservative samplers never touch it, and the receiver always gets the UNSHIFTED position.
        'mode_readers_reached_only_from_sampleSpotShadow':
            shader.count('pcfSpotShadow(') - 1 == sample.count('pcfSpotShadow(') >= 10 and
            shader.count('detailSpotReceiver(') - 1 == sample.count('detailSpotReceiver(') >= 10 and
            shader.count('spotSurfaceReceiver(') == 2 and shader.count('pcfSpotShadowSurface(') == 2,
        'conservative_samplers_stateless': 'g_spot' not in conservative and 'pcfSpotShadow(' not in conservative,
        'dispatch_passes_unshifted_receiver':
            sample.count('detailSpotReceiver(original_pos,norm,') == sample.count('detailSpotReceiver(') and
            'detailSpotReceiver(pos' not in sample,
        'no_screen_derivative_in_model': 'dFd' not in new and 'fwidth' not in new,
        'legacy_functions_verbatim': verbatim,
        'fade_layer_is_exact': fade_layer_exact,
        'fade_layer_rejects_planted_edits': not fade_layer_survivors,
        'gate_is_nonzero_normal': block.count(GATE) == 1 and block.count('g_spot_bias_mode = 0;') == 1 and
                                  block.index('g_spot_bias_mode = 0;') < block.index(GATE) and
                                  shader.count('g_spot_bias_mode = 1;') == 1 and 'g_spot_bias_mode = 1;' in block,
        'shader_default_is_legacy': 'uniform int spot_shadow_bias_model = 0;' in shader,
        'offset_is_texel_scaled': OFFSET in new and 'float ceiling = max(0.25*spot_shadow_offset,g_spot_bias_floor);' in new,
        'setting_default_on': settings.get('RenderSpotShadowBiasModel') == 1,
        'pipeline_upload': pipeline.count('"RenderSpotShadowBiasModel", 1);') == 1 and
                           pipeline.count('LLStaticHashedString("spot_shadow_bias_model")') == 1,
        'frozen_shaft_copy_untouched': 'spot_shadow_bias_model' not in tree.read(
            'indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl'),
    }


def evaluate(out, negatives=True, normal_map=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    defaults = settings_defaults()
    r = R.Runner('WORKTREE')
    print('[spot-bias] GPU', r.gl.renderer, flush=True)
    pr = r.visibility
    base = link(r, G.SourceTree(BASE), 'BASE visibility')
    failures = []
    contracts = source_contracts(r.tree)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    strap, strap_failures, edge_failures = strap_check(r, pr, defaults)
    neighbour, neighbour_failures = neighbour_check(r, pr, defaults)
    camera, camera_failures = camera_check(r, pr, defaults)
    failures += strap_failures + edge_failures + neighbour_failures + camera_failures
    sweep, sweep_failures = sweep_check(r, pr, defaults, distances=(1., 3., 8.))
    failures += [f"sweep acne {f['distance_m']}m {f['angle']}deg roll {f['roll']} {f['resolution']} {f['path']} {f['kernel']}" for f in sweep_failures]
    identity = identity_check(r, pr, base, defaults)
    failures += ['identity ' + k for k, ok in identity.items() if not ok]
    result = dict(base=BASE, limits=LIMITS, contracts=contracts, identity=identity, strap=strap, sweep=sweep, neighbour=neighbour, camera=camera)
    normal, normal_failures = normal_check(r, pr, defaults)
    failures += [f"normal-map acne tilt {f['tilt']} {f['distance_m']}m {f['angle']}deg {f['resolution']} {f['path']} {f['kernel']}"
                 for f in normal_failures]
    result['normal_map_10deg'] = normal
    calls, calls_failures = calls_check(r, r.tree, defaults)
    widen, widen_failures = widen_check(r, pr, defaults)
    wiring = lighting_wiring(r, defaults)
    if not (wiring['strap_shadow_dark_model0'] < .05 and wiring['strap_shadow_dark_model1'] > .9):
        failures.append('production spotLightF does not take the bias model: %s' % wiring)
    shaft, shaft_failures, shaft_base = shaft_check(r.gl, r.tree)
    failures += calls_failures + widen_failures + shaft_failures
    result.update(calls=calls, widen=widen, spotLightF_wiring=wiring, shaft=shaft)
    from spot_shadow_bias_slots import check as slots_check
    slots, slots_failures = slots_check(r, defaults, negatives=negatives)
    result['combined_slots'] = slots
    failures += slots_failures
    if normal_map:
        # Informational: twice the designed slack. Reported, not gated.
        rows = normal_check(r, pr, defaults, tilts=(-20., 20.))[0]
        worst = min(rows, key=lambda x: x['mean_visibility'])
        result['normal_map_20deg'] = rows
        print('[spot-bias] normal map 20 deg (informational) worst', json.dumps(worst), flush=True)
    if negatives:
        rejected = {}
        for name, (edits, target) in MUTANTS.items():
            mutant = link(r, MutantTree(edits), name)
            if target == 'shaft':
                caught = bool(shaft_check(r.gl, MutantTree(edits), shaft_base)[1])
            elif target == 'calls':
                caught = bool(calls_check(r, MutantTree(edits), defaults)[1])
            elif target == 'widen':
                caught = bool(widen_check(r, mutant, defaults)[1])
            elif target == 'strap':
                caught = bool(strap_check(r, mutant, defaults, resolutions=(2048,), edges=False)[1])
            elif target == 'edges':
                caught = bool(strap_check(r, mutant, defaults, resolutions=(2048,))[2] + neighbour_check(r, mutant, defaults)[1])
            elif target == 'camera':
                caught = bool(camera_check(r, mutant, defaults)[1])
            elif target == 'normal':
                caught = bool(normal_check(r, mutant, defaults)[1])
            elif target == 'sweep':
                caught = bool(sweep_check(r, mutant, defaults, resolutions=(2048,), distances=(3., 8.))[1])
            else:
                caught = not all(identity_check(r, mutant, base, defaults).values())
            rejected[name] = caught
            print('[spot-bias]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    for row in strap:
        print('[spot-bias] strap', json.dumps(row), flush=True)
    print('[spot-bias] sweep worst', json.dumps(min(sweep, key=lambda x: x['mean_visibility'])), flush=True)
    print('[spot-bias]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_spot_bias')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
