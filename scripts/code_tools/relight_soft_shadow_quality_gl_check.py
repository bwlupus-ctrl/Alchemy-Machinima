"""Soft shadow quality fixes 2, 3 and 6 on production GL (real shadowUtil.glsl / sunLightF.glsl).

Driven through rig_shadow_gl.Runner like relight_spot_bias_gl_check.py. Every measurement is taken
from the rendered shader output of an analytic scene; planted shader regressions must each be rejected.

  Fix 2  RenderSoftShadowFillStep    (FALSE = the fill step on lit surfaces is retired)
    fill      across a straight umbra edge no pixel value sits in the dead band (0, fill) when the
              step is retired (projector surface model, sun legacy and units kernels), the profile
              across the edge is monotonic, and with the step restored the band is empty (positive
              control: the fixture really sees the old step).
  Fix 3  RenderSoftShadowNoiseModel  (1 = hygiene, 0 = old statements; the default is now 2 = model 1 with the projector
         noise anchored to the shadow map, a later layer with its own gate: relight_soft_shadow_motion_gl_check.py)
    phase     two projector slots with identical maps differ per pixel in the penumbra (and are
              identical with the old model); the sun's phase differs from every slot phase.
    fade      with a penumbra radius under 2 texels the result does not depend on the per-pixel
              hash at all; above 3.5 texels it does.
    fullkernel the new noise model has no early-out: pixels beside an umbra (straight edge, thin post, small box;
              projector and sun) never read exactly lit; a reintroduced 8-tap early-out is rejected.
    camera    a static light and a camera that moves: the Poisson kernel of the old model changes
              per pixel with the camera (the view-space sawtooth), the new one does not.
    isotropy  a shadow edge along map X and one along map Y have the same penumbra width (the old
              kernel is 1.5x wider along one of them).
  Fix 6  RenderSunLightProjectorDeadSamples (FALSE = projector samples for slots 0-1 removed)
    sunlight  sunLightF / sunLightSSAOF: R and G are bit-identical either way, B and A are 1.0
              when removed and equal BASE when restored; every reader of lightMap is accounted for.
  identity  every new setting at 0 (shader defaults) is byte-identical to BASE; airborne (zero normal),
            the rig-rim, RenderSpotShadowBiasModel 0 and the frozen Classic shaft copy ignore the
            new settings completely.
  negatives planted shader regressions, one per sub-change, must each be rejected.
"""
import argparse
import json
import math
import re
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import rig_shadow_gl as R
import relight_spot_bias_gl_check as B
from rig_shadow_presets_check import function, settings_defaults

BASE = 'c63f7ca1b2f'
SHADER = B.SHADER
SUN_F = 'indra/newview/app_settings/shaders/class2/deferred/sunLightF.glsl'
SUN_SSAO_F = 'indra/newview/app_settings/shaders/class2/deferred/sunLightSSAOF.glsl'
CLASSIC = ('indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil.glsl',
           'indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricClassicShadowUtil16.glsl')
FILL = .15
TOL_BIT = 0.0


class Tree(G.SourceTree):
    """The worktree with exact single-site edits, keyed by file name (planted negatives)."""
    def __init__(self, edits=None, ref='WORKTREE'):
        super().__init__(ref)
        self.edits = edits or {}

    def read(self, path):
        text = super().read(path)
        for before, after in self.edits.get(Path(path).name, ()):
            if before.startswith('re:'):
                text, count = re.subn(before[3:], after, text)
                assert count == 1, ('mutation drift', before)
                continue
            assert text.count(before) == 1, ('mutation drift', before)
            text = text.replace(before, after)
        return text


def link_src(r, tree, source, label, defines='#define SPOT_SHADOW 1\n', extra=('deferred/shadowUtil.glsl',), vertex=None):
    saved = r.tree
    r.tree = tree
    try:
        return r.link_text(source, label, extra=r.FRAGMENT_COMMON + list(extra), defines=defines, vertex=vertex)
    finally:
        r.tree = saved


# ---------------------------------------------------------------- fixtures
PLANE = S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6))


def wall_scene(kind, xc=2.2):
    """A big card between lamp (x = 3) and plane (x = 0) at x = xc: its shadow edge on the plane runs along z ('A') or y ('B').

    The blocker search's separation grows with the card's distance from the plane: the kernel radius is
    1 texel at xc = 0.5, about 2.3 at 1.0, 3.7 at 1.5 and 9 (the cap of the 8 texel softness) at 2.2.
    """
    if kind == 'A':
        card = S.Prim('card', 'obb', c=(xc, 1.5, 1.45), h=(.01, 1.5, 3.))
    else:
        card = S.Prim('card', 'obb', c=(xc, 0., 2.95), h=(.01, 3., 1.5))
    return S.Scene('wall edge ' + kind, [PLANE, card], [], [], {}, np.empty((0, 3)))


def wall_view(kind, xc=2.2):
    scene = wall_scene(kind, xc)
    # Oblique, so the card (halfway to the lamp) does not hide the half of the plane it shadows.
    cam = S.Camera((3.5, -2.0, 1.45) if kind == 'A' else (3.5, 0, -.55), R.CENTRE, 320, 320, 12)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    pos = g['pos']
    coord = pos[..., 1] if kind == 'A' else pos[..., 2] - 1.45
    along = pos[..., 2] - 1.45 if kind == 'A' else pos[..., 1]
    mask = (g['surf'] == 0) & (np.abs(coord) < .33) & (np.abs(along) < .3)
    assert mask.sum() > 20000, mask.sum()
    return scene, cam, g, d, n, coord, mask


LIGHT_POS = R.CENTRE + np.array([3., 0, 0])


def lamp(softness=8.):
    return dict(B.entry(LIGHT_POS, 1., False), softness=softness)


def profile(v, coord, mask, bin_m=.004):
    """Mean visibility per bin across the edge (bins hold ~100 pixels): (centres, mean)."""
    idx = np.floor((coord[mask] + .4) / bin_m).astype(int)
    vals = v[mask]
    n = idx.max() + 1
    cnt = np.bincount(idx, minlength=n)
    tot = np.bincount(idx, weights=vals, minlength=n)
    ok = cnt > 20
    centres = (np.arange(n) + .5) * bin_m - .4
    return centres[ok], tot[ok] / cnt[ok]


def width(centres, mean, lo=.1, hi=.9):
    """Distance between the 90% and 10% crossings of a falling profile (linear interpolation)."""
    def crossing(level):
        for i in range(len(mean) - 1):
            if mean[i] >= level > mean[i + 1]:
                return centres[i] + (mean[i] - level) / (mean[i] - mean[i + 1]) * (centres[i + 1] - centres[i])
        return None
    a, b = crossing(hi), crossing(lo)
    return None if a is None or b is None else b - a


# RenderSpotShadowFadeModel (the projector fade model, relight_spot_fade_gl_check.py) for every projector draw of
# this file. 0 = the legacy sampler, which is also the shader's own default and what BASE implements; evaluate()
# repeats the measuring checks with 1 (map-bounded, the setting's default). Stated on every draw: programs keep
# uniform values, and a program without the uniform (BASE) ignores the upload.
FADE_MODEL = 0


def projector(r, pr, cam, d, n, e, run, retire, noise, vogel=True, model=B.MODEL_ALL, airborne=False, setup=None, clip=None):
    uni = dict(soft_shadow_fill_retire=int(retire), soft_shadow_noise_model=int(noise), spot_shadow_fade_model=int(FADE_MODEL))

    def hook(p):
        for k, val in uni.items():
            r.gl.ui(r.gl.loc(p, k.encode()), val)
        if setup:
            setup(p)
    return B.visibility(r, pr, cam, d, n, e, run, model, airborne=airborne, vogel=vogel, clip=clip, setup=hook)


def run_defaults(defaults, **over):
    return dict(defaults, BDMergeSoftProjectorShadows=True, BDMergeSoftShadowFill=FILL,
                BDMergeSoftShadowMaxPenumbra=12., **over)


# ---------------------------------------------------------------- sun harness
SUN_SOURCE = """
out vec4 frag_color;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz;vec3 n=getNorm(tc).xyz;
frag_color=vec4(sampleDirectionalShadow(p,n,tc));}
"""
SUN_DEFINES = '#define SUN_SHADOW 1\n#define SPOT_SHADOW 1\n'


def sun_state(r, p, cam, e, defaults, vogel, units=False, fill=FILL, retire=0, noise=0, removed=0):
    gl = r.gl
    tex, ncam, res = r.native
    bias = np.eye(4)
    bias[:3, :3] *= .5
    bias[:3, 3] = .5
    m = bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view)
    for i in range(4):
        gl.ui(gl.loc(p, ('shadowMap%d' % i).encode()), 12)
        r._mat(p, ('shadow_matrix[%d]' % i).encode(), m)
    to_light = S.unit(e['pos'] - R.CENTRE)
    view_dir = cam.view[:3, :3] @ to_light
    r.uniform(p, 'sun_dir', *view_dir)
    r.uniform(p, 'moon_dir', *view_dir)
    gl.ui(gl.loc(p, b'sun_up_factor'), 1)
    r.uniform(p, 'shadow_res', res, res)
    r.uniform(p, 'shadow_clip', 100, 200, 400, 800)
    r.uniform(p, 'shadow_bias', defaults['RenderShadowBias'])
    r.uniform(p, 'shadow_offset', defaults['RenderShadowOffset'])
    gl.ui(gl.loc(p, b'soft_shadow_enable'), 1)
    gl.ui(gl.loc(p, b'soft_shadow_sun'), 1)
    r.uniform(p, 'soft_shadow_scale', 8.)
    r.uniform(p, 'soft_shadow_max', 12.)
    r.uniform(p, 'soft_shadow_fill', fill)
    gl.ui(gl.loc(p, b'soft_shadow_vogel'), int(vogel))
    gl.ui(gl.loc(p, b'soft_shadow_taps'), 16)
    gl.ui(gl.loc(p, b'shadow_units_mode'), int(units))
    if units:
        r.uniform(p, 'shadow_cascade_coef[0]', .01, .0016, .0016, 1.)
        r.uniform(p, 'shadow_res_cascade', res, res, res, res)
        r.uniform(p, 'shadow_res_cascade_h', res, res, res, res)
    gl.ui(gl.loc(p, b'soft_shadow_fill_retire'), int(retire))
    gl.ui(gl.loc(p, b'soft_shadow_noise_model'), int(noise))
    gl.ui(gl.loc(p, b'sun_projector_samples_removed'), int(removed))


def sun_visibility(r, pr, cam, d, n, e, defaults, **kw):
    gl = r.gl
    r.target_float(cam)
    r._begin(pr)
    r.inputs(pr, cam, d, n)
    sun_state(r, pr, cam, e, defaults, **kw)
    return r.read_float(cam)[..., 0]


# ---------------------------------------------------------------- fix 2
def dead_band(v, mask, fill=FILL):
    """Pixels strictly between 0 and the fill level: empty exactly when the fill step exists."""
    return int(((v[mask] > 1e-4) & (v[mask] < fill - .01)).sum())


def monotonic_violation(centres, mean):
    """Largest rise of a profile that should only fall (bins hold ~100 pixels, so noise is ~1%)."""
    return float(np.max(np.diff(mean))) if len(mean) > 1 else 1.


def fill_check(r, pr, defaults, tree_runner=None):
    scene, cam, g, d, n, coord, mask = wall_view('A')
    e = lamp()
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    run = run_defaults(defaults)
    rows, failures = [], []

    def measure(name, v):
        c, m = profile(v, coord, mask)
        row = dict(check='fill', path=name, dead_band_pixels=dead_band(v, mask), rise=monotonic_violation(c, m),
                   dark_pixels=int((v[mask] < 1e-4).sum()), lit_pixels=int((v[mask] > .999).sum()),
                   partial_pixels=int(((v[mask] > 1e-4) & (v[mask] < .999)).sum()),
                   mean_penumbra_width_mm=None if width(c, m) is None else width(c, m) * 1000)
        return row

    for kernel, vogel in (('vogel', True), ('poisson', False)):
        for noise in (0, 1):
            retired = projector(r, pr, cam, d, n, e, run, 1, noise, vogel=vogel)
            stepped = projector(r, pr, cam, d, n, e, run, 0, noise, vogel=vogel)
            a, b = measure(f'projector {kernel} noise{noise} retired', retired), measure(f'projector {kernel} noise{noise} stepped', stepped)
            rows += [a, b]
            if a['partial_pixels'] < 700 or a['dark_pixels'] < 3000 or a['lit_pixels'] < 3000:
                failures.append(a['path'] + ': the fixture has no umbra edge')
            if a['dead_band_pixels'] < 100:
                failures.append(a['path'] + ': retired fill shows no sub-fill values (fixture cannot see the old step)')
            if a['rise'] > .05:
                failures.append(a['path'] + f": profile across the umbra edge is not monotonic (rise {a['rise']:.3f})")
            if b['dead_band_pixels'] != 0:
                failures.append(b['path'] + ': the old fill no longer produces its step (positive control)')
    # sun, legacy and units kernels
    for units in (False, True):
        for vogel in (True, False):
            retired = sun_visibility(r, r.sun, cam, d, n, e, defaults, vogel=vogel, units=units, retire=1)
            stepped = sun_visibility(r, r.sun, cam, d, n, e, defaults, vogel=vogel, units=units, retire=0)
            name = f"sun {'units' if units else 'legacy'} {'vogel' if vogel else 'poisson'}"
            a, b = measure(name + ' retired', retired), measure(name + ' stepped', stepped)
            rows += [a, b]
            if a['partial_pixels'] < 700 or a['dark_pixels'] < 3000 or a['lit_pixels'] < 3000:
                failures.append(a['path'] + ': the fixture has no umbra edge')
            if a['dead_band_pixels'] < 100:
                failures.append(a['path'] + ': retired fill shows no sub-fill values')
            if a['rise'] > .05:
                failures.append(a['path'] + f": profile across the umbra edge is not monotonic (rise {a['rise']:.3f})")
            if b['dead_band_pixels'] != 0:
                failures.append(b['path'] + ': the old fill no longer produces its step (positive control)')
    return rows, failures


# ---------------------------------------------------------------- fix 3
SLOT_SOURCE = """
out vec4 frag_color;
uniform int probe_slot;
uniform vec3 probe_shift;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz-probe_shift;vec3 n=getNorm(tc).xyz;
frag_color=vec4(sampleSpotShadow(p,n,probe_slot,tc));}
"""


def slot_render(r, prog, cam, d, n, e, run, retire, noise, slot=0, shift=(0., 0., 0.), vogel=True):
    def setup(p):
        tex, ncam, res = r.native
        bias = np.eye(4)
        bias[:3, :3] *= .5
        bias[:3, 3] = .5
        m = bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view)
        t = np.eye(4)
        t[:3, 3] = shift
        r._mat(p, ('shadow_matrix[%d]' % (4 + slot)).encode(), m @ t)
        r.uniform(p, 'spot_shadow_softness[%d]' % slot, e['softness'])
        r.uniform(p, 'spot_shadow_detail[%d]' % slot, 0.)
        r.gl.ui(r.gl.loc(p, b'probe_slot'), slot)
        r.uniform(p, 'probe_shift', *shift)
    return projector(r, prog, cam, d, n, e, run, retire, noise, vogel=vogel, setup=setup)


def kernel_radius(r, tree, cam, d, n, e, run, retire=1, noise=1):
    """pr / 16 for every pixel that reaches the soft kernel (lit early-outs return 1.0)."""
    prog = link_src(r, Tree({'shadowUtil.glsl': [(B.WIDEN, B.WIDEN + '        if (true) return pr / 16.0;\n')]}), SLOT_SOURCE, 'radius probe')
    return slot_render(r, prog, cam, d, n, e, run, retire, noise) * 16.


def phase_check(r, prog, defaults, edits=()):
    scene, cam, g, d, n, coord, mask = wall_view('A')
    e = lamp()
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    run = run_defaults(defaults)
    rows, failures = [], []
    radius = kernel_radius(r, None, cam, d, n, e, run)
    first = slot_render(r, prog, cam, d, n, e, run, 1, 1)
    wide = mask & (radius > 4.5) & (radius < 15.9) & (first > .02) & (first < .98)
    for noise in (0, 1):
        a = slot_render(r, prog, cam, d, n, e, run, 1, noise, slot=0)
        b = slot_render(r, prog, cam, d, n, e, run, 1, noise, slot=1)
        c = slot_render(r, prog, cam, d, n, e, run, 1, noise, slot=8)
        differ = float((np.abs(a - b)[wide] > 1e-6).mean())
        differ8 = float((np.abs(a - c)[wide] > 1e-6).mean())
        row = dict(check='phase', noise_model=noise, wide_pixels=int(wide.sum()), slot1_differs=differ, slot8_differs=differ8)
        rows.append(row)
        if wide.sum() < 1000:
            failures.append('phase: fixture has too few wide-penumbra pixels')
        if noise and (differ < .5 or differ8 < .5):
            failures.append(f'phase: slots with identical maps use the same rotation (differ {differ:.2f}/{differ8:.2f})')
        if not noise and (differ != 0. or differ8 != 0.):
            failures.append('phase: the old model must not depend on the slot')
    # the sun's own phase: its output differs from the same kernel with phase 0 (slot 0's phase)
    sun_edits = list(edits)
    sun_cur = link_src(r, Tree({'shadowUtil.glsl': sun_edits}), SUN_SOURCE, 'sun phase', defines=SUN_DEFINES)
    sun_zero = link_src(r, Tree({'shadowUtil.glsl': sun_edits + [('re:const float SOFT_NOISE_SUN_PHASE  = [0-9.]+;', 'const float SOFT_NOISE_SUN_PHASE  = 0.0;')]}),
                        SUN_SOURCE, 'sun phase zero', defines=SUN_DEFINES)
    a = sun_visibility(r, sun_cur, cam, d, n, e, defaults, vogel=True, noise=1, retire=1)
    b = sun_visibility(r, sun_zero, cam, d, n, e, defaults, vogel=True, noise=1, retire=1)
    sun_wide = mask & (a > .02) & (a < .98)
    sun_differs = float((np.abs(a - b)[sun_wide] > 1e-6).mean()) if sun_wide.any() else float('nan')
    rows.append(dict(check='phase', sun_wide_pixels=int(sun_wide.sum()), sun_phase_differs_from_slot0=sun_differs))
    if sun_wide.sum() < 500 or not sun_differs >= .5:
        failures.append(f'phase: the sun uses the same rotation phase as slot 0 ({sun_differs})')
    return rows, failures


def fade_check(r, prog, defaults, edits=()):
    """Small radius (a contact shadow, 1 texel) and large radius (9 texels) in two scenes."""
    run = run_defaults(defaults)
    e = lamp()
    flat = Tree({'shadowUtil.glsl': list(edits) + [('    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n',
                                       '    return 0.37;\n')]})
    prog_flat = link_src(r, flat, SLOT_SOURCE, 'flat hash')
    rows, failures = [], []
    for name, xc, lo, hi in (('contact', .5, .99, 1.9), ('mid', 1.5, 3.55, 3.8), ('wide', 2.2, 3.6, 15.9)):
        scene, cam, g, d, n, coord, mask = wall_view('A', xc)
        r.native_map(scene, B.light_of(e), e['fov'], 2048)
        radius = kernel_radius(r, None, cam, d, n, e, run)
        for noise in (0, 1):
            v = slot_render(r, prog, cam, d, n, e, run, 1, noise)
            w = slot_render(r, prog_flat, cam, d, n, e, run, 1, noise)
            # pixels the kernel really ran on (partially lit under either hash), at this radius
            sel = mask & (radius > lo) & (radius < hi) & ((v > .02) & (v < .98) | (w > .02) & (w < .98))
            same = float((np.abs(v - w)[sel] < 1e-6).mean()) if sel.any() else None
            rows.append(dict(check='fade', scene=name, noise_model=noise, pixels=int(sel.sum()), independent_of_hash=same))
            if sel.sum() < 100:
                failures.append(f'fade {name}: fixture has too few penumbra pixels ({int(sel.sum())})')
            elif name == 'contact':
                if noise and same < .999:
                    failures.append(f'fade: below 2 texels the kernel still depends on the per-pixel hash ({same:.3f})')
                if not noise and same > .5:
                    failures.append(f'fade: positive control failed (old model should depend on the hash at 1 texel, {same:.3f})')
            else:   # mid (3.7 texels, just above the 3.5 ramp end) and wide: the rotation is fully on
                if noise and same > .2:
                    failures.append(f'fade: above 3.5 texels the kernel no longer depends on the per-pixel hash ({same:.3f})')
    return rows, failures


# ---------------------------------------------------------------- early-out == full kernel on straight edges
def floor_view(theta):
    """A floor lit from above, a big card 2 m under the lamp whose straight shadow edge runs through the
    origin at angle theta (degrees) on the floor; the camera is under the card, so it sees the floor."""
    a = math.radians(theta)
    centre = (-1.5 * math.sin(a), 1.5 * math.cos(a), 3.45)
    scene = S.Scene('floor edge', [S.Prim('floor', 'obb', c=(0, 0, 1.44), h=(6, 6, .01)),
                                   S.Prim('card', 'obb', c=centre, h=(3., 1.5, .01), yaw=theta)], [], [], {}, np.empty((0, 3)))
    cam = S.Camera((2.0, 0, 2.85), R.CENTRE, 320, 320, 12)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    pos = g['pos']
    mask = (g['surf'] == 0) & (np.abs(pos[..., 0]) < .3) & (np.abs(pos[..., 1]) < .3)
    assert mask.sum() > 10000, mask.sum()
    return scene, cam, d, n, mask


def post_view(kind):
    """A thin post (a 15 mm shadow) or a small box (11 mm) between lamp and plane: narrower than the spacing of an 8-tap
    probe ring around the kernel, i.e. exactly what an early-out shortcut misses."""
    if kind == 'post':
        blocker = S.Prim('post', 'obb', c=(2.2, 0., 1.45), h=(.02, .0005, 1.5))
    else:
        blocker = S.Prim('box', 'obb', c=(2.2, 0., 1.45), h=(.02, .0015, .0015))
    scene = S.Scene('small blocker ' + kind, [PLANE, blocker], [], [], {}, np.empty((0, 3)))
    cam = S.Camera((3.5, -2.0, 1.45), R.CENTRE, 320, 320, 12)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    pos = g['pos']
    mask = (g['surf'] == 0) & (np.abs(pos[..., 1]) < .3) & (np.abs(pos[..., 2] - 1.45) < .3)
    return scene, cam, d, n, mask


def dilate(m, steps):
    out = m.copy()
    for _ in range(steps):
        grown = out.copy()
        grown[1:] |= out[:-1]
        grown[:-1] |= out[1:]
        grown[:, 1:] |= out[:, :-1]
        grown[:, :-1] |= out[:, 1:]
        out = grown
    return out


def full_kernel_check(r, defaults, edits=()):
    """The new noise model evaluates the whole blocker search and kernel on every pixel.

    `edits` is applied to the build under test; the reference is the production worktree. They must agree bit for
    bit on a straight edge (floor scene), a thin post (3 mm) and a small box (6 mm): blockers narrower than the
    spacing of any 8-tap probe ring, which is exactly what an early-out shortcut misses. Projector, sun legacy
    and sun units; Vogel and Poisson. A reintroduced 8-tap early-out therefore differs from the reference."""
    run = run_defaults(defaults)
    edits = list(edits)
    slot = link_src(r, Tree({'shadowUtil.glsl': edits}), SLOT_SOURCE, 'full slot')
    sun = link_src(r, Tree({'shadowUtil.glsl': edits}), SUN_SOURCE, 'full sun', defines=SUN_DEFINES)
    ref_slot = slot if not edits else link_src(r, Tree(), SLOT_SOURCE, 'full slot ref')
    ref_sun = sun if not edits else link_src(r, Tree(), SUN_SOURCE, 'full sun ref', defines=SUN_DEFINES)
    rows, failures = [], []
    cases = [('edge45', floor_view(45.)), ('edge0', floor_view(0.)), ('post', post_view('post')), ('box', post_view('box'))]
    for scene_name, (scene, cam, d, n, mask) in cases:
        edge = scene_name.startswith('edge')
        e = dict(B.entry(R.CENTRE + np.array([0., 0., 3.]) if edge else LIGHT_POS, 1., False), softness=8.)
        r.native_map(scene, B.light_of(e), e['fov'], 2048)
        for kernel, vogel in (('vogel', True), ('poisson', False)):
            pairs = [('projector', slot_render(r, slot, cam, d, n, e, run, 1, 1, vogel=vogel),
                      slot_render(r, ref_slot, cam, d, n, e, run, 1, 1, vogel=vogel))]
            for units in (False, True):
                pairs.append(('sun-units' if units else 'sun',
                              sun_visibility(r, sun, cam, d, n, e, defaults, vogel=vogel, units=units, noise=1, retire=1),
                              sun_visibility(r, ref_sun, cam, d, n, e, defaults, vogel=vogel, units=units, noise=1, retire=1)))
            for name, got, want in pairs:
                partial = int((mask & (want > .02) & (want < .98)).sum())
                wrong = int((got[mask] != want[mask]).sum())
                rows.append(dict(check='full_kernel', scene=scene_name, kernel=kernel, path=name, partial_pixels=partial, differing_pixels=wrong))
                if partial < 20:
                    failures.append(f'full kernel {scene_name} {name} {kernel}: fixture has no shadow ({partial})')
                if wrong:
                    failures.append(f'full kernel {scene_name} {name} {kernel}: differs from the full-kernel reference on {wrong} pixels')
    return rows, failures


def camera_check(r, prog, defaults):
    """Poisson kernel, static light, camera moving along view-space Y: per-pixel std over 8 frames."""
    scene, cam, g, d, n, coord, mask = wall_view('B')   # edge along map U: the jitter moves the kernel across it
    e = lamp()
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    run = run_defaults(defaults)
    radius = kernel_radius(r, None, cam, d, n, e, run)
    first = slot_render(r, prog, cam, d, n, e, run, 1, 1, vogel=False)
    pen = mask & (radius > 2.5) & (radius < 15.9) & (first > .02) & (first < .98)
    rows, failures = [], []
    for noise in (0, 1):
        frames = np.stack([slot_render(r, prog, cam, d, n, e, run, 1, noise, shift=(0., .19 * k, 0.), vogel=False) for k in range(8)])
        std = frames.std(axis=0)
        row = dict(check='camera', noise_model=noise, penumbra_pixels=int(pen.sum()), mean_per_pixel_std=float(std[pen].mean()),
                   p95_per_pixel_std=float(np.percentile(std[pen], 95)))
        rows.append(row)
        if pen.sum() < 700:
            failures.append('camera: too few penumbra pixels')
        if noise == 0 and row['mean_per_pixel_std'] < .0005:
            failures.append('camera: positive control failed (the old Poisson kernel is not camera dependent here)')
        if noise == 1 and row['mean_per_pixel_std'] > .0001:
            failures.append(f"camera: the new Poisson kernel still changes with the camera (std {row['mean_per_pixel_std']:.4f})")
    return rows, failures


def isotropy_check(r, prog, defaults):
    rows, failures = [], []
    run = run_defaults(defaults)
    widths = {}
    for noise in (0, 1):
        for kind in ('A', 'B'):
            scene, cam, g, d, n, coord, mask = wall_view(kind)
            e = lamp()
            r.native_map(scene, B.light_of(e), e['fov'], 2048)
            v = slot_render(r, prog, cam, d, n, e, run, 1, noise)
            c, m = profile(v, coord, mask)
            widths[(noise, kind)] = width(c, m)
        wa, wb = widths[(noise, 'A')], widths[(noise, 'B')]
        ratio = None if not wa or not wb else max(wa, wb) / min(wa, wb)
        rows.append(dict(check='isotropy', noise_model=noise, width_a_mm=None if wa is None else wa * 1000,
                         width_b_mm=None if wb is None else wb * 1000, ratio=ratio))
        if ratio is None:
            failures.append('isotropy: no penumbra width')
        elif noise and ratio > 1.12:
            failures.append(f'isotropy: penumbra is anisotropic ({ratio:.2f})')
        elif not noise and ratio < 1.15:
            failures.append(f'isotropy: positive control failed ({ratio:.2f}; the old kernel is 1.5x along map Y)')
    return rows, failures


# ---------------------------------------------------------------- identity
def identity_check(r, base, defaults):
    """New settings at the shader defaults == BASE; airborne / rim / bias model 0 ignore them when ON."""
    scene, cam, g, d, n, coord, mask = wall_view('A')
    e = lamp()
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    checks = {}
    for kernel, soft, vogel in B.KERNELS:
        run = dict(defaults, BDMergeSoftProjectorShadows=soft, BDMergeSoftShadowFill=FILL, RenderSpotShadowDetailBias=True)
        for detail in (False, True):
            ee = dict(e, detail_shadows=detail)
            tag = ('detail_' if detail else 'standard_') + kernel
            for model in (0, B.MODEL_DEFAULT, B.MODEL_ALL):
                ref = B.visibility(r, base, cam, d, n, ee, run, model, vogel=vogel)
                off = projector(r, r.visibility, cam, d, n, ee, run, 0, 0, vogel=vogel, model=model)
                checks[f'defaults_equal_base_model{model}_{tag}'] = bool(np.array_equal(off, ref))
            ref_air = B.visibility(r, base, cam, d, n, ee, run, 0, airborne=True, vogel=vogel)
            on_air = projector(r, r.visibility, cam, d, n, ee, run, 1, 1, vogel=vogel, airborne=True)
            checks[f'airborne_ignores_new_settings_{tag}'] = bool(np.array_equal(on_air, ref_air))
            ref0 = B.visibility(r, base, cam, d, n, ee, run, 0, vogel=vogel)
            on0 = projector(r, r.visibility, cam, d, n, ee, run, 1, 1, vogel=vogel, model=0)
            checks[f'bias_model0_ignores_new_settings_{tag}'] = bool(np.array_equal(on0, ref0))
        # positive control: ON must change a lit surface in the soft kernels
        if soft:
            ref = projector(r, r.visibility, cam, d, n, e, run, 0, 0, vogel=vogel)
            on = projector(r, r.visibility, cam, d, n, e, run, 1, 1, vogel=vogel)
            checks['new_settings_change_surface_' + kernel] = not np.array_equal(on, ref)
    return checks


def shaft_check(r, defaults):
    """New shaft engine: byte-identical to BASE with the new settings both off and on."""
    import relight_volumetric_gl_check as V
    base_runner = V.Runner(G.SourceTree(BASE), r.gl)
    current = V.Runner(r.tree, r.gl)
    fixture = V.fixtures(320, 180)['outside']
    rows, failures = [], []
    passes = dict(rim_only=dict(steps=0, rim=1), march_with_rim=dict(steps=24, rim=1, dither=1),
                  march_no_rim=dict(steps=24, rim=0, dither=1))
    for name, kwargs in passes.items():
        for vogel in (0, 1):
            kernel = dict(soft_shadow_vogel=vogel, soft_shadow_taps=16, spot_shadow_bias_model=B.MODEL_ALL)
            ref = base_runner.run(fixture, uniforms=dict(kernel, soft_shadow_fill_retire=0, soft_shadow_noise_model=0), **kwargs)[:2]
            for retire, noise in ((0, 0), (1, 1)):
                got = current.run(fixture, uniforms=dict(kernel, soft_shadow_fill_retire=retire, soft_shadow_noise_model=noise), **kwargs)[:2]
                same = all(np.array_equal(np.asarray(x), np.asarray(y)) for x, y in zip(got, ref))
                lit = int(np.count_nonzero(np.asarray(got[0])[:, :, :3].max(2) > 0))
                rows.append(dict(check='shaft', draw=name, vogel=vogel, retire=retire, noise=noise, identical=bool(same), lit_pixels=lit))
                if not same or lit < 100:
                    failures.append(f'shaft {name} vogel {vogel} retire {retire} noise {noise}: ' + ('differs from BASE' if lit >= 100 else 'fixture draws nothing'))
    return rows, failures


# ---------------------------------------------------------------- fix 6
SUNLIGHT_VERTEX = 'in vec3 position;out vec2 vary_fragcoord;void main(){gl_Position=vec4(position,1);vary_fragcoord=position.xy*.5+.5;}'


def sunlight_programs(r, tree, with_ssao):
    path = SUN_SSAO_F if with_ssao else SUN_F
    saved = r.tree
    r.tree = tree
    try:
        source = tree.read(path)
        extra = ['deferred/shadowUtil.glsl'] + (['deferred/aoUtil.glsl'] if with_ssao else [])
        return r.link_text(source, 'sunLight ' + ('SSAO' if with_ssao else 'plain'), extra=r.FRAGMENT_COMMON + extra,
                           defines=SUN_DEFINES, vertex=SUNLIGHT_VERTEX)
    finally:
        r.tree = saved


def sunlight_render(r, prog, cam, d, n, e, defaults, removed, retire=0, noise=0):
    gl = r.gl
    r.target_float(cam)
    r._begin(prog)
    r.inputs(prog, cam, d, n)
    r.native_uniforms(prog, cam, e, run_defaults(defaults))
    sun_state(r, prog, cam, e, defaults, vogel=True, removed=removed, retire=retire, noise=noise)
    gl.ui(gl.loc(prog, b'spot_shadow_bias_model'), B.MODEL_ALL)
    # slot 1 reads its own matrix: give it the same real map so B and A carry real shadow
    tex, ncam, res = r.native
    bias = np.eye(4)
    bias[:3, :3] *= .5
    bias[:3, 3] = .5
    r._mat(prog, b'shadow_matrix[4]', bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view))
    r._mat(prog, b'shadow_matrix[5]', bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view))
    for name in ('shadowMap0', 'shadowMap1', 'shadowMap2', 'shadowMap3'):
        gl.ui(gl.loc(prog, name.encode()), 12)
    gl.ui(gl.loc(prog, b'render_debug_lighting_term'), 0)
    gl.ui(gl.loc(prog, b'sun_projector_samples_removed'), int(removed))
    return r.read_float(cam)


def sunlight_check(r, base_tree, defaults):
    scene, cam, g, d, n, coord, mask = wall_view('A')
    e = lamp()
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    rows, failures = [], []
    for ssao in (False, True):
        try:
            base = sunlight_programs(r, base_tree, ssao)
            cur = sunlight_programs(r, r.tree, ssao)
        except AssertionError as ex:
            if ssao:   # the SSAO program needs more of the real pipeline than the harness links
                rows.append(dict(check='sunlight', program='SSAO', skipped=str(ex)[:200]))
                continue
            raise
        ref = sunlight_render(r, base, cam, d, n, e, defaults, 0)
        on = sunlight_render(r, cur, cam, d, n, e, defaults, 0)
        off = sunlight_render(r, cur, cam, d, n, e, defaults, 1)
        v = mask
        row = dict(check='sunlight', program='SSAO' if ssao else 'plain',
                   restored_equals_base=bool(np.array_equal(on, ref)),
                   rg_identical_when_removed=bool(np.array_equal(off[..., :2], ref[..., :2])),
                   ba_are_one_when_removed=bool(np.all(off[..., 2:4][v] == 1.)),
                   base_ba_carry_shadow=float((ref[..., 2][v] < .9).mean()),
                   base_ba_differ_from_one=bool(np.any(ref[..., 2:4][v] < .999)))
        rows.append(row)
        if not row['restored_equals_base']:
            failures.append(f"sunlight {row['program']}: RenderSunLightProjectorDeadSamples TRUE differs from BASE")
        if not row['rg_identical_when_removed']:
            failures.append(f"sunlight {row['program']}: R/G changed when the samples were removed")
        if not row['ba_are_one_when_removed']:
            failures.append(f"sunlight {row['program']}: B/A are not 1.0 when removed")
        if not row['base_ba_differ_from_one']:
            failures.append(f"sunlight {row['program']}: fixture's B/A never differ from 1 (the removal would be unobservable)")
    return rows, failures


def reader_contracts(tree):
    """Every reader of the sun light map accounted for: lightMap .b/.a are never consumed."""
    import re
    shaders = Path(G.REPO) / 'indra/newview/app_settings/shaders'
    reads = {}
    for path in sorted(shaders.rglob('*.glsl')):
        text = path.read_text(encoding='utf-8', errors='replace')
        for m in re.finditer(r'(texture|texelFetch|textureLod)\(\s*lightMap\b[^;]*;', text):
            reads.setdefault(path.name, []).append(m.group(0))
    pipeline = tree.read('indra/newview/pipeline.cpp')
    soften = tree.read('indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl')
    blur = tree.read('indra/newview/app_settings/shaders/class1/deferred/blurLightF.glsl')
    upsample = tree.read('indra/newview/app_settings/shaders/class1/deferred/projectorVolumetricUpsampleF.glsl')
    sun, sun_ssao = tree.read(SUN_F), tree.read(SUN_SSAO_F)

    def swizzles(lines):
        out = set()
        for line in lines:
            out |= set(re.findall(r'\)\s*\.([rgba]+)', line) or ['(whole vec4)'])
        return out
    return {
        'readers_are_exactly_known': set(reads) == {'blurLightF.glsl', 'dofCombineF.glsl', 'projectorVolumetricUpsampleF.glsl', 'softenLightF.glsl'},
        # softenLight reads .rg for lighting; its only .rgb read is the cascade debug view (term 2), for which the
        # sun programs return the cascade colour BEFORE any projector sample is taken.
        'soften_reads_rg_and_debug_rgb': swizzles(reads['softenLightF.glsl']) == {'rg', 'rgb'} and
            soften.count('texture(lightMap, vary_fragcoord.xy).rg') == 2 and
            'render_debug_lighting_term == 2) debug_color = texture(lightMap, tc).rgb' in soften,
        'sun_programs_return_cascade_debug_before_samples':
            all(s.index('render_debug_lighting_term == 2') < s.index('col.b = 1.0;') and 'return;' in s[s.index('render_debug_lighting_term == 2'):s.index('col.b = 1.0;')]
                for s in (sun, sun_ssao)),
        'sun_programs_sample_projectors_only_behind_the_switch':
            all(s.count('sampleSpotShadow(pos.xyz, norm.xyz,') == 2 and s.index('if (sun_projector_samples_removed == 0)') < s.index('sampleSpotShadow(pos.xyz, norm.xyz, 0')
                for s in (sun, sun_ssao)),
        # The blur is channel-separable: .b/.a only ever feed .b/.a (weights k.xyxx, divide by defined_weight.xyxx).
        'blur_channels_separable': 'col += texture(lightMap, samptc)*k[i].xyxx;' in blur and 'col /= defined_weight.xyxx;' in blur and
            'ccol.r' in blur and 'ccol.g' not in blur and 'ccol.b' not in blur and 'ccol.a' not in blur,
        # The other sampler named lightMap are bound to other textures at their draw.
        'upsample_binds_projvol_half': 'enableTexture(LLShaderMgr::DEFERRED_LIGHT)' in pipeline and 'mProjVolHalf.bindTexture(1, depth_ch' in pipeline and
            'return texelFetch(lightMap, p, 0).r;' in upsample,
        'dof_overwrites_light_target_first': pipeline.index('mRT->deferredLight.bindTarget();') <
            pipeline.index('combine_program.bindTexture(LLShaderMgr::DEFERRED_LIGHT, &mRT->deferredLight'),
        'light_target_has_no_cpu_readers': 'deferredLight.getTexture' not in pipeline and 'deferredLight.readPixels' not in pipeline,
        'declared_but_unsampled': all(name not in reads for name in ('spotLightF.glsl', 'pbralphaF.glsl', 'sharedActorFxPbrF.glsl')),
    }


# ---------------------------------------------------------------- source contracts
def source_contracts(tree):
    # The projector fade model is a newer, separately reviewed layer on top of this series: it is reversed first,
    # exactly (softshadow_fade_shader_hunks.json, digest-pinned; the result must be this series' complete reviewed
    # file), and every contract below is stated on that text.
    shader = B.reverse_fade_layer(tree.read(SHADER))
    base = G.SourceTree(BASE).read(SHADER)
    settings = settings_defaults()
    pipeline = tree.read('indra/newview/pipeline.cpp')
    sample = function(shader, 'float sampleSpotShadow(')
    sample_base = function(base, 'float sampleSpotShadow(')
    slot_line = '        g_spot_slot = float(index);   // [SoftShadow noise] per-light Vogel phase\n'
    phases = [(k * 2.4) % (2 * math.pi) for k in range(16)]
    sun_phase = float(shader.split('SOFT_NOISE_SUN_PHASE  = ')[1].split(';')[0])
    gap = min(min(abs(sun_phase - p), 2 * math.pi - abs(sun_phase - p)) for p in phases)
    surface = function(shader, 'float pcfSpotShadowSurface(')
    sun_functions = ''.join(function(shader, s) for s in ('float pcfShadow(', 'float pcfShadowC(', 'float pcfShadowRes('))
    return {
        'legacy_functions_verbatim': all(function(shader, s) == function(base, s) for s in (
            'float pcfSpotShadow(', 'float estimateSpotBlockerGap(', 'float detailSpotTap(', 'float estimateDetailSpotBlockerGap(',
            'vec4 spotSurfaceReceiver(', 'vec4 detailSpotReceiver(', 'float pcfSpotShadowConservative(', 'float sampleSpotShadowConservative(')),
        'sample_differs_only_by_slot_line': sample.count(slot_line) == 1 and sample.replace(slot_line, '') == sample_base,
        'slot_set_inside_surface_gate': sample.index('g_spot_bias_mode = 1;') < sample.index(slot_line) < sample.index('detail = true;'),
        'fill_sites': shader.count('&& soft_shadow_fill_retire == 0)') == 4 and
            function(shader, 'float pcfSpotShadow(').count('soft_shadow_fill > 0.0 && shadow > 0.0)') == 1 and
            surface.count('soft_shadow_fill_retire == 0') == 1 and sun_functions.count('soft_shadow_fill_retire == 0') == 3,
        'noise_sites': surface.count('noise') >= 4 and
            'softShadowProbe' not in shader and 'SOFT_PROBE' not in shader and sun_functions.count('softShadowPhi(') == 3 and
            surface.count('softShadowPhi(pr, g_spot_slot * SOFT_NOISE_SLOT_PHASE)') == 1,
        'shader_defaults_are_old': 'uniform int soft_shadow_fill_retire = 0;' in shader and 'uniform int soft_shadow_noise_model = 0;' in shader and
            'uniform int sun_projector_samples_removed;' in tree.read(SUN_F),
        'phases_distinct': gap > .2,
        'frozen_classic_copies_untouched': all(tree.read(p) == G.SourceTree(BASE).read(p) for p in CLASSIC),
        'zero_normal_gate_unchanged': B.GATE in sample,
        'settings_defaults': settings.get('RenderSoftShadowFillStep') in (0, False) and settings.get('RenderSoftShadowNoiseModel') == 2 and
            settings.get('RenderSunLightProjectorDeadSamples') in (0, False),
        'pipeline_uploads': pipeline.count('"RenderSoftShadowFillStep", false);') == 1 and pipeline.count('"RenderSoftShadowNoiseModel", 2);') == 1 and
            pipeline.count('"RenderSunLightProjectorDeadSamples", false);') == 1 and
            pipeline.count('LLStaticHashedString("soft_shadow_fill_retire")') == 1 and
            pipeline.count('LLStaticHashedString("soft_shadow_noise_model")') == 1 and
            pipeline.count('LLStaticHashedString("sun_projector_samples_removed")') == 1,
    }


# ---------------------------------------------------------------- negatives
LEGACY_FILL = '''        if (soft_shadow_fill > 0.0 && shadow > 0.0)
            shadow = mix(soft_shadow_fill, 1.0, shadow);
        return clamp(shadow, 0.0, 1.0);
    }

    vec2 unsnapped = stc.xy;'''
PHI = '    return soft_shadow_noise_model != 0 ? phase + phi * smoothstep(2.0, 3.5, radius_texels) : phi;\n'
JIT = '    return soft_shadow_noise_model != 0 ? softShadowIGN() : legacy;\n'
SLOTPH = 'softShadowPhi(pr, g_spot_slot * SOFT_NOISE_SLOT_PHASE)'
ANISO = '        if (!noise) texel.y *= 1.5;   // [SoftShadow noise] the isotropic kernel drops the 1.5x\n'
SUN_RETIRE = '''        if (soft_shadow_fill > 0.0 && shadow > 0.0 && soft_shadow_fill_retire == 0)
            shadow = mix(soft_shadow_fill, 1.0, shadow);
        return clamp(shadow, 0.0, 1.0);
    }

    stc.x = floor(stc.x*shadow_res.x'''


def negatives():
    """name -> (edits {file: [(before, after)]}, the check that must reject it)."""
    sh = 'shadowUtil.glsl'
    surf_fill = '''        if (soft_shadow_fill > 0.0 && shadow > 0.0 && soft_shadow_fill_retire == 0)
            shadow = mix(soft_shadow_fill, 1.0, shadow);
        return clamp(shadow, 0.0, 1.0);
    }

    stc.x = floor(proj_shadow_res.x * stc.x'''
    return {
        'surface_fill_not_retired': ({sh: [(surf_fill, surf_fill.replace(' && soft_shadow_fill_retire == 0', ''))]}, 'fill'),
        'sun_fill_not_retired': ({sh: [(SUN_RETIRE, SUN_RETIRE.replace(' && soft_shadow_fill_retire == 0', ''))]}, 'fill'),
        'legacy_projector_fill_retired': ({sh: [(LEGACY_FILL, LEGACY_FILL.replace('shadow > 0.0)', 'shadow > 0.0 && soft_shadow_fill_retire == 0)'))]}, 'identity'),
        'no_slot_phase': ({sh: [(SLOTPH, 'softShadowPhi(pr, 0.0)')]}, 'phase'),
        'early_out_reintroduced_projector': ({sh: [("        bool noise = soft_shadow_noise_model != 0;\n", "        bool noise = soft_shadow_noise_model != 0;\n" + "        if (noise)\n        {\n            float lit = texture(shadowMap, stc.xyz);\n            for (int i = 0; i < 7; ++i)\n            {\n                float a = 6.2831853 * float(i) / 7.0;\n                lit += texture(shadowMap, vec3(stc.xy + vec2(cos(a), sin(a)) * (6.0 / proj_shadow_res), stc.z));\n            }\n            if (lit > 7.999) return 1.0;\n        }\n")]}, 'full'),
        'early_out_reintroduced_sun': ({sh: [("        vec2 texel = pr / shadow_res;\n        float shadow = 0.0;\n", "        vec2 texel = pr / shadow_res;\n        float shadow = 0.0;\n" + "        if (soft_shadow_noise_model != 0)\n        {\n            float lit = texture(shadowMap, stc.xyz);\n            for (int i = 0; i < 7; ++i)\n            {\n                float a = 6.2831853 * float(i) / 7.0;\n                lit += texture(shadowMap, vec3(stc.xy + vec2(cos(a), sin(a)) * (6.0 / shadow_res), stc.z));\n            }\n            if (lit > 7.999) return 1.0;\n        }\n")]}, 'full'),
        'early_out_reintroduced_sun_units': ({sh: [("        vec2 texel = pr / res;\n        float shadow = 0.0;\n        if (soft_shadow_vogel != 0)\n        {\n            int taps", "        vec2 texel = pr / res;\n        float shadow = 0.0;\n" + "        if (soft_shadow_noise_model != 0)\n        {\n            float lit = texture(shadowMap, stc.xyz);\n            for (int i = 0; i < 7; ++i)\n            {\n                float a = 6.2831853 * float(i) / 7.0;\n                lit += texture(shadowMap, vec3(stc.xy + vec2(cos(a), sin(a)) * (6.0 / res), stc.z));\n            }\n            if (lit > 7.999) return 1.0;\n        }\n" + "        if (soft_shadow_vogel != 0)\n        {\n            int taps")]}, 'full'),
        'no_sun_phase': ({sh: [('const float SOFT_NOISE_SUN_PHASE  = 1.55;', 'const float SOFT_NOISE_SUN_PHASE  = 0.0;')]}, 'phase'),
        'no_rotation_fade': ({sh: [(PHI, PHI.replace('smoothstep(2.0, 3.5, radius_texels)', '1.0'))]}, 'fade'),
        'wide_fade': ({sh: [(PHI, PHI.replace('smoothstep(2.0, 3.5, radius_texels)', 'smoothstep(8.0, 9.0, radius_texels)'))]}, 'fade'),
        'poisson_jitter_still_view_space': ({sh: [(JIT, '    return legacy;\n')]}, 'camera'),
        'anisotropy_kept': ({sh: [(ANISO, '        texel.y *= 1.5;\n')]}, 'isotropy'),
        'airborne_enters_new_kernel': ({sh: [(B.GATE, B.GATE.replace(' && dot(norm,norm) > 0.0', '')),
                                              ]}, 'identity'),
        'sun_samples_not_removed': ({'sunLightF.glsl': [('    if (sun_projector_samples_removed == 0)\n', '    if (true)\n')],
                                     'sunLightSSAOF.glsl': [('    if (sun_projector_samples_removed == 0)\n', '    if (true)\n')]}, 'sunlight'),
        'sun_removal_clobbers_red': ({'sunLightF.glsl': [('    col.b = 1.0;\n    col.a = 1.0;\n', '    col.b = 1.0;\n    col.a = 1.0;\n    if (sun_projector_samples_removed != 0) col.r = 1.0;\n')]}, 'sunlight'),
    }


def evaluate(out, do_negatives=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    defaults = settings_defaults()
    r = R.Runner('WORKTREE')
    print('[soft-quality] GPU', r.gl.renderer, flush=True)
    r.sun = link_src(r, r.tree, SUN_SOURCE, 'sun visibility', defines=SUN_DEFINES)
    failures, result = [], {}

    contracts = source_contracts(r.tree)
    readers = reader_contracts(r.tree)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    failures += ['reader contract ' + k for k, ok in readers.items() if not ok]
    result.update(contracts=contracts, readers=readers)

    base = B.link(r, G.SourceTree(BASE), 'BASE visibility')
    slot_prog = link_src(r, r.tree, SLOT_SOURCE, 'slot probe')
    fill_rows, f = fill_check(r, r.visibility, defaults)
    failures += f
    phase_rows, f = phase_check(r, slot_prog, defaults)
    failures += f
    fade_rows, f = fade_check(r, slot_prog, defaults)
    failures += f
    camera_rows, f = camera_check(r, slot_prog, defaults)
    failures += f
    iso_rows, f = isotropy_check(r, slot_prog, defaults)
    failures += f
    full_rows, f = full_kernel_check(r, defaults)
    failures += f
    identity = identity_check(r, base, defaults)
    failures += ['identity ' + k for k, ok in identity.items() if not ok]
    shaft_rows, f = shaft_check(r, defaults)
    failures += f
    sun_rows, f = sunlight_check(r, G.SourceTree(BASE), defaults)
    failures += f
    result.update(identity=identity, fill=fill_rows, phase=phase_rows, fade=fade_rows,
                  camera=camera_rows, isotropy=iso_rows, full_kernel=full_rows, shaft=shaft_rows, sunlight=sun_rows)
    # The same measurements with the map-bounded projector fade model (RenderSpotShadowFadeModel 1, the default):
    # the fill, noise and full-kernel results must hold under both models. The byte-identity checks stay on
    # model 0 above, which is what BASE implements.
    global FADE_MODEL
    FADE_MODEL = 1
    try:
        model1 = {}
        for key, run_check in (('fill', lambda: fill_check(r, r.visibility, defaults)), ('phase', lambda: phase_check(r, slot_prog, defaults)),
                               ('fade', lambda: fade_check(r, slot_prog, defaults)), ('camera', lambda: camera_check(r, slot_prog, defaults)),
                               ('isotropy', lambda: isotropy_check(r, slot_prog, defaults)), ('full_kernel', lambda: full_kernel_check(r, defaults))):
            rows, f = run_check()
            model1[key] = rows
            failures += ['fade model 1: ' + str(x) for x in f]
        # Positive control: the uniform reaches these draws and changes the sampler (a receiver beyond the sun range).
        scene, cam, g, d, n, coord, mask = wall_view('A')
        e = lamp()
        r.native_map(scene, B.light_of(e), e['fov'], 2048)
        run = run_defaults(defaults)
        kept = projector(r, r.visibility, cam, d, n, e, run, 1, 1, clip=(.1, .2, .4, .8))
        FADE_MODEL = 0
        lost = projector(r, r.visibility, cam, d, n, e, run, 1, 1, clip=(.1, .2, .4, .8))
        model1['control'] = dict(shadow_pixels_model1=int((kept[mask] < .5).sum()), shadow_pixels_model0=int((lost[mask] < .5).sum()))
        if not (model1['control']['shadow_pixels_model1'] > 1000 and model1['control']['shadow_pixels_model0'] == 0):
            failures.append('fade model 1: the model does not reach the quality draws %s' % model1['control'])
    finally:
        FADE_MODEL = 0
    result['fade_model_1'] = model1
    print('[soft-quality] fade model 1 repeat', json.dumps(model1['control']), flush=True)
    if do_negatives:
        rejected = {}
        for name, (edits, target) in negatives().items():
            tree = Tree(edits)
            try:
                if target == 'fill':
                    mutant = link_src(r, tree, r.visibility_source, name)
                    sun = link_src(r, tree, SUN_SOURCE, name, defines=SUN_DEFINES)
                    saved = r.sun
                    r.sun = sun
                    try:
                        caught = bool(fill_check(r, mutant, defaults)[1])
                    finally:
                        r.sun = saved
                elif target == 'identity':
                    mutant = link_src(r, tree, r.visibility_source, name)
                    saved = r.visibility
                    r.visibility = mutant
                    try:
                        caught = not all(identity_check(r, base, defaults).values())
                    finally:
                        r.visibility = saved
                elif target == 'sunlight':
                    caught = bool(sunlight_check_mutant(r, tree, defaults))
                elif target == 'full':
                    caught = bool(full_kernel_check(r, defaults, edits.get('shadowUtil.glsl', ()))[1])
                else:
                    prog = link_src(r, tree, SLOT_SOURCE, name)
                    fn = dict(phase=phase_check, fade=fade_check, camera=camera_check, isotropy=isotropy_check)[target]
                    caught = bool(fn(r, prog, defaults, edits.get('shadowUtil.glsl', ()))[1]) if target in ('fade', 'phase') else bool(fn(r, prog, defaults)[1])
            except AssertionError as ex:
                caught = True   # a mutant that no longer links / runs cannot pass
                print('[soft-quality] mutant', name, 'raised', str(ex)[:120], flush=True)
            rejected[name] = caught
            print('[soft-quality]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, default=str) + '\n', encoding='utf-8')
    for key in ('fill', 'phase', 'fade', 'camera', 'isotropy', 'full_kernel', 'sunlight'):
        for row in result[key]:
            print('[soft-quality]', key, json.dumps(row, default=str), flush=True)
    print('[soft-quality]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


def sunlight_check_mutant(r, tree, defaults):
    """Mutated sunLightF / sunLightSSAOF programs must fail the sunlight comparison against BASE."""
    scene, cam, g, d, n, coord, mask = wall_view('A')
    e = lamp()
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    base_tree = G.SourceTree(BASE)
    failures = []
    base = sunlight_programs(r, base_tree, False)
    cur = sunlight_programs(r, tree, False)
    ref = sunlight_render(r, base, cam, d, n, e, defaults, 0)
    on = sunlight_render(r, cur, cam, d, n, e, defaults, 0)
    off = sunlight_render(r, cur, cam, d, n, e, defaults, 1)
    if not np.array_equal(on, ref):
        failures.append('restored differs')
    if not np.array_equal(off[..., :2], ref[..., :2]):
        failures.append('rg changed')
    if not np.all(off[..., 2:4][mask] == 1.):
        failures.append('ba not one')
    return failures


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_soft_shadow_quality')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
