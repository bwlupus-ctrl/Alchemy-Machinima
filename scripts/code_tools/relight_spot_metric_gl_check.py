"""Soft shadow fix 4: metric projector penumbra (RenderSpotShadowPenumbraModel) on production GL.

Driven through rig_shadow_gl.Runner like relight_spot_bias_gl_check.py: the real shadowUtil.glsl on the
GPU over analytic scenes, a real rasterised projector map, and the per-slot uniform computed by the
PRODUCTION header (alrelightmath.h is compiled into a small host, so the numbers uploaded here are the
ones LLPipeline::bindDeferredShader uploads). Planted regressions must each be rejected.

  formula    a straight card edge over a plane: the measured penumbra radius is within 15% of
             R*(d_r-d_b)/d_b (distances from the light's centre) for two FOVs (the narrow one has
             the projection origin 0.36 m behind the lamp), two map resolutions and four gaps.
  sphere     sphere + plane: the same within 15%, and within 15% of the WORLD CUBE path
             (areaShadowFilterF.glsl through the area runner) for the same light, sphere and plane.
  hardening  the radius grows with the caster-receiver gap, strictly and without steps
             (13 gaps: every one within 15% of the formula, so no plateau and no jump).
  resolution the same metres at 512, 1024 and 2048 texel maps.
  cap        BDMergeSoftShadowMaxPenumbra still bounds the kernel radius in texels.
  aspect     a 2:1 projector on a square map: the penumbra is the same width in metres along both
             map axes (the kernel is an ellipse in texels).
  widen      a slanted receiver inside a wide metric penumbra does not shadow itself (the surface
             bias model's widened-kernel term follows the metric radius), and a bare slanted plane
             finds no blocker (the search pays the plane's slope).
  retention  (review round 1) 80 degrees incidence, a blocker 10 cm away along the ray, source 0.4 m:
             the shadow stays (geometric truth; the reference never touches the shader's bias) and
             the grazing receiver does not shadow itself.
  small      (round 1) a blocker 1 to 3 map texels wide under a 16 texel cap, swept by fractions
             of a texel: no dark core, no receiver under it that misses it, never a narrow kernel.
  clear      (round 1) the lit return is bit-identical to running the 1-texel kernel.
  cost       (round 1) shadow-map compares per pixel counted by the shader itself, old vs metric,
             typical and worst, against ceilings.
  timing     (round 1) GL timer queries at 3840 x 2160 over sixteen slots: BASE's shader, this
             shader with the setting at 0, and with the metric.
  noise      the noise model of fix 3 still applies: slots differ by phase, a contact shadow does
             not depend on the per-pixel hash, the fill step stays retired; with noise model 0
             the search and the kernel both use the old rotation.
  identity   no upload / setting 0 is bit-identical to BASE for every kernel; airborne (zero normal),
             the shaft rim, RenderSpotShadowBiasModel 0 and rig Detail lights on model 1 ignore an
             uploaded metric completely; the frozen Classic copies are untouched.
  contracts  one authored source radius on the CPU for cube, area and projector paths, one default
             (0.10 m) and one cap (2 m) for all three; settings default;
             no new sampler; the upload site.
  negatives  planted regressions (old 7-level gap, wrong k, radius not shared, ...) are rejected.
"""
import argparse
import hashlib
import json
import math
import os
import re
import subprocess
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_cube_source as CS
import rig_shadow_gl as R
import relight_spot_bias_gl_check as B
import relight_soft_shadow_quality_gl_check as Q
from rig_shadow_presets_check import function, settings_defaults

BASE = '28c8c83af9d'   # main with the projector fade layer (RenderSpotShadowFadeModel), the next layer down
FADE = 1                # spot_shadow_fade_model uploaded by every render below; evaluate() runs the checks under 0 and 1
SHADER = B.SHADER
HEADER = 'indra/newview/alrelightmath.h'
PIPELINE = 'indra/newview/pipeline.cpp'
AREA_F = 'indra/newview/app_settings/shaders/class3/deferred/areaShadowF.glsl'
TOL = .15        # the task's bound: round caster against the silhouette formula, and against the world cube
CARD_TOL = .06   # a flat caster has one blocker depth: the estimator itself (measured within 1.1%)
LIGHT = R.CENTRE + np.array([3., 0, 0])
PLANE = S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6))
RHO = .2            # sphere radius
DISK_10_90 = 1.374  # 10%..90% width of a uniform disk kernel crossing a straight edge, in radii


# ---------------------------------------------------------------- the production header, compiled
HOST = r'''
#include "alrelightmath.h"
#include <cstdio>
int main()
{
    double authored, fixture, override_softness, global_softness, res_x, res_y, p00, p11, p22, p32, centre;
    while (std::scanf("%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &authored, &fixture, &override_softness, &global_softness,
                      &res_x, &res_y, &p00, &p11, &p22, &p32, &centre) == 11)
    {
        // The statements of LLPipeline::bindDeferredShader ([SoftShadow metric]).
        const double radius = ALRelight::shadowSourceRadius(authored) *
            ALRelight::spotPenumbraTrim(fixture != 0.0, override_softness, global_softness);
        const double scale_x = ALRelight::spotPenumbraScale(radius, res_x, p00, p32);
        const double scale_y = ALRelight::spotPenumbraScale(radius, res_y, p11, p32);
        const double depth_a = ALRelight::spotDepthA(p22);
        const double setback = ALRelight::spotPenumbraSetback(centre, p32);
        std::printf("%.17g %.17g %.17g %.17g %.17g\n", radius, scale_x, scale_y, depth_a, setback);
    }
    return 0;
}
'''


class HeaderHost:
    """alrelightmath.h (optionally mutated) compiled once; evaluates the pipeline's upload statements."""
    def __init__(self, out, text):
        key = hashlib.sha1((text + HOST).encode()).hexdigest()[:12]
        d = Path(out) / 'host' / key
        d.mkdir(parents=True, exist_ok=True)
        self.exe = d / 'metric_host.exe'
        if not self.exe.exists():
            (d / 'alrelightmath.h').write_text(text, encoding='utf-8')
            (d / 'metric_host.cpp').write_text(HOST, encoding='utf-8')
            vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            vs = subprocess.check_output([str(vswhere), '-latest', '-property', 'installationPath'], text=True).strip()
            cmd = d / 'compile.cmd'
            cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc "%s" /Fe:"%s" /Fo:"%s"\n' %
                           (Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat', d / 'metric_host.cpp', self.exe, d / 'metric_host.obj'))
            done = subprocess.run(['cmd', '/d', '/c', str(cmd)], cwd=d, capture_output=True, text=True)
            assert done.returncode == 0 and self.exe.exists(), 'alrelightmath.h does not compile: ' + (done.stdout + done.stderr)[-600:]

    def __call__(self, authored, fixture, override, glob, res_x, res_y, proj, centre):
        line = ' '.join(repr(float(x)) for x in (authored, fixture, override, glob, res_x, res_y, proj[0, 0], proj[1, 1], proj[2, 2], proj[2, 3], centre))
        out = subprocess.run([str(self.exe)], input=line + '\n', capture_output=True, text=True, check=True).stdout.split()
        radius, kx, ky, a, s = map(float, out)
        return radius, (kx, ky, a, s)


def metric_of(host, r, e, radius, softness=3., override=0., fixture=False):
    """What the viewer uploads for slot 0 of the current native map: (effective radius, vec4)."""
    tex, ncam, res = r.native
    centre = -(ncam.view @ np.array([*e['pos'], 1.]))[2]
    return host(radius, fixture, override, softness, res, res, ncam.proj, centre)


class Mut(G.SourceTree):
    """The worktree with exact edits of shadowUtil.glsl (planted negatives, probes). An edit's text must occur exactly
    once; a `before` starting with 'all:' replaces every occurrence (at least two)."""
    def __init__(self, edits=()):
        super().__init__('WORKTREE')
        self.edits = list(edits)

    def read(self, path):
        text = super().read(path)
        if Path(path).name == 'shadowUtil.glsl':
            text = apply_edits(text, self.edits)
        return text


def apply_edits(text, edits):
    for before, after in edits:
        if before.startswith('all:'):
            assert text.count(before[4:]) >= 2, ('mutation drift', before)
            text = text.replace(before[4:], after)
            continue
        assert text.count(before) == 1, ('mutation drift', before)
        text = text.replace(before, after)
    return text


METRIC_WIDEN = '    float tap_z = stc.z - (pr - 1.0) * g_spot_widen_stc;\n'
KERNEL_RADIUS = 'max(texel.x * proj_shadow_res.x, texel.y * proj_shadow_res.y)'   # texels, from the UV radii the kernel loops use
RADIUS_PROBE = (METRIC_WIDEN, '    if (true) return ' + KERNEL_RADIUS + ' / 64.0;\n' + METRIC_WIDEN)


# ---------------------------------------------------------------- scenes and measurement
def scene_of(prims):
    return S.Scene('metric', [PLANE] + list(prims), [], [], {}, np.empty((0, 3)))


def card(xc):
    """A big thin card whose straight edge (y = 0, along z) is in line with the lamp: its shadow covers y > 0."""
    return S.Prim('card', 'obb', c=(xc, 1.5, 1.45), h=(.002, 1.5, 3.))


def ball(xc):
    return S.Prim('ball', 'sphere', c=(xc, 0, 1.45), r=RHO)


def card_depth(xc):
    return 3. - xc - .002          # the face the lamp sees


def ball_depth(xc):
    dc = 3. - xc
    return dc - RHO * RHO / dc     # the tangent circle


def ball_edge(xc):
    return 3. * math.tan(math.asin(RHO / (3. - xc)))


def predicted(radius, blocker_depth, receiver_depth=3.):
    return radius * (receiver_depth - blocker_depth) / blocker_depth


def edge_view(scene, edge_y, half, res=384, radial=False):
    """A narrow camera on the plane point (0, edge_y, 1.45); coord = metres across the shadow edge."""
    target = np.array([0, edge_y, 1.45])
    eye = np.array([2.5, edge_y - 1.2, 1.45]) if not radial else np.array([2.5, edge_y + 1.2, 1.45])
    cam = S.Camera(eye, target, res, res, math.degrees(2 * math.atan(half / np.linalg.norm(eye - target))))
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    pos = g['pos']
    coord = (np.hypot(pos[..., 1], pos[..., 2] - 1.45) if radial else pos[..., 1]) - edge_y
    mask = (g['surf'] == 0) & (np.abs(pos[..., 2] - 1.45) < half * .5) & (np.abs(coord) < half * .9)
    assert mask.sum() > 20000, mask.sum()
    return cam, d, n, coord, mask


def profile(v, coord, mask, bin_m):
    c = coord[mask]
    idx = np.floor((c - c.min()) / bin_m).astype(int)
    cnt = np.bincount(idx)
    tot = np.bincount(idx, weights=v[mask])
    ok = cnt > 20
    centres = (np.arange(len(cnt)) + .5) * bin_m + c.min()
    return centres[ok], tot[ok] / cnt[ok]


def crossing(c, m, level):
    found = []
    for i in range(len(m) - 1):
        lo, hi = sorted((m[i], m[i + 1]))
        if lo < level <= hi:
            found.append(c[i] + (level - m[i]) / (m[i + 1] - m[i]) * (c[i + 1] - c[i]))
    return found


def kernel_radius_m(v, coord, mask, expect):
    """Radius of the (disk) penumbra kernel in metres from the 10%..90% width of the edge profile; None when the
    profile is not a single clean ramp between an umbra and full light."""
    c, m = profile(v, coord, mask, max(expect / 12., .0003))
    if m.min() > .02 or m.max() < .98:
        return None
    a, b = crossing(c, m, .1), crossing(c, m, .9)
    if len(a) != 1 or len(b) != 1:
        return None
    return abs(b[0] - a[0]) / DISK_10_90


def run_of(defaults, cap, **over):
    return dict(defaults, BDMergeSoftProjectorShadows=True, BDMergeSoftShadowFill=Q.FILL, BDMergeSoftShadowMaxPenumbra=cap, **over)


def render(r, pr, cam, d, n, e, run, metric, taps=32, vogel=True, noise=1, retire=1, model=B.MODEL_ALL, airborne=False, rim=0., extra=None):
    """One projector visibility image. metric: the vec4 for slot 0, or None = never uploaded."""
    def hook(p):
        gl = r.gl
        gl.ui(gl.loc(p, b'soft_shadow_noise_model'), int(noise))
        gl.ui(gl.loc(p, b'soft_shadow_fill_retire'), int(retire))
        gl.ui(gl.loc(p, b'soft_shadow_taps'), int(taps))
        r.uniform(p, 'projvol_rim_strength', float(rim))
        gl.ui(gl.loc(p, b'spot_shadow_fade_model'), int(FADE))
        # Location -1 in BASE: GL ignores the upload, like the viewer on a program without the uniform.
        r.uniform(p, 'spot_shadow_metric[0]', *(metric if metric is not None else (0., 0., 0., 0.)))
        if extra:
            extra(p)
    return B.visibility(r, pr, cam, d, n, e, run, model, airborne=airborne, vogel=vogel, setup=hook)


def lamp(fov=1., softness=0., detail=False):
    return dict(B.entry(LIGHT, fov, detail), softness=softness)


def texel_m(e, res):
    return 2. * (3. + .125 / math.tan(e['fov'] * .5) - .125) * math.tan(e['fov'] * .5) / res


def measure(r, pr, host, defaults, prim, edge_y, blocker_depth, radius, res, fov=1., cap=None, radial=False, upload=None, **kw):
    """cap None: Max penumbra 1.6 x the predicted radius (at least 6 texels), the relation a tuned scene has
    (the blocker search covers the cap, so a far larger cap searches far more of a round caster than its edge)."""
    e = lamp(fov)
    scene = scene_of([prim])
    want = predicted(radius, blocker_depth)
    if cap is None:
        cap = max(1.6 * want / texel_m(e, res), 6.)
    cam, d, n, coord, mask = edge_view(scene, edge_y, max(4. * want, .02), radial=radial)
    r.native_map(scene, B.light_of(e), e['fov'], res)
    eff, metric = metric_of(host, r, e, radius)
    if upload:
        metric = upload(metric, e, res)
    v = render(r, pr, cam, d, n, e, run_of(defaults, cap), metric, **kw)
    got = kernel_radius_m(v, coord, mask, want)
    return dict(fov=fov, res=res, source_radius_m=radius, blocker_depth_m=round(blocker_depth, 4), cap_texels=cap, predicted_mm=want * 1000.,
                predicted_texels=want / texel_m(e, res), measured_mm=None if got is None else got * 1000.,
                ratio=None if got is None else got / want)


# ---------------------------------------------------------------- checks
def formula_check(r, pr, host, defaults, upload=None, quick=False):
    rows, failures = [], []
    cases = [(1., 2048, .05), (1., 1024, .1), (.5, 2048, .03), (.5, 1024, .06)]
    for fov, res, radius in cases[:2] + cases[2:3] if quick else cases:
        for xc in ((1., 2.) if quick else (.5, 1., 1.5, 2.)):
            row = dict(check='formula', gap_m=xc, **measure(r, pr, host, defaults, card(xc), 0., card_depth(xc), radius, res, fov=fov, upload=upload))
            rows.append(row)
            if row['ratio'] is None or abs(row['ratio'] - 1.) > CARD_TOL:
                failures.append(f"formula fov {fov} res {res} gap {xc}: measured {row['measured_mm']} mm, predicted {row['predicted_mm']:.2f} mm")
    return rows, failures


def cube_radius(r, defaults, xc, radius):
    """The world cube path (area runner) on the sphere + plane: its penumbra radius in metres."""
    scene = scene_of([ball(xc)])
    want = predicted(radius, ball_depth(xc))
    cam, d, n, coord, mask = edge_view(scene, ball_edge(xc), max(4. * want, .02), radial=True)
    light = S.Light('metric cube', LIGHT, radius, 19.8, 'point', seed=23)
    near = CS.emitter_near(scene, light)
    r.gpu_atlas = r.raster.atlas(scene, light, near)
    try:
        ch = dict(row=0, near=near, far=light.range, r_map=radius, contact=CS.contact_distance(light.range), seed=light.seed,
                  owned=True, contact_fade=0., detail_shadows=False)
        world = dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT, CS.ATLAS_W), np.float32), channels=[ch])
        plan = dict(samples=8, steps=12, half=0, near_steps=0, radius=0.)
        v = r.run(cam, d, n, [light], plan, world=world, light_bias=defaults['RenderAreaShadowLightBias'],
                  max_dist=defaults['RenderAreaShadowMaxDist'], thickness=defaults['RenderAreaShadowThickness'])[0]
    finally:
        r.gpu_atlas = None
    return kernel_radius_m(v, coord, mask, want), want


def sphere_check(r, pr, host, defaults, upload=None, cube=True):
    rows, failures = [], []
    for res, radius, xc in ((2048, .03, 1.), (2048, .03, 1.5), (1024, .1, 1.25), (1024, .1, 1.5)):
        row = dict(check='sphere', gap_m=xc, **measure(r, pr, host, defaults, ball(xc), ball_edge(xc), ball_depth(xc), radius, res,
                                                       radial=True, upload=upload))
        if cube and radius >= .1:
            got, want = cube_radius(r, defaults, xc, radius)
            row['cube_mm'] = None if got is None else got * 1000.
            row['projector_over_cube'] = None if got is None or row['measured_mm'] is None else row['measured_mm'] / (got * 1000.)
            if row['projector_over_cube'] is None or abs(row['projector_over_cube'] - 1.) > TOL:
                failures.append(f"sphere gap {xc}: projector {row['measured_mm']} mm vs world cube {row['cube_mm']} mm")
        rows.append(row)
        if row['ratio'] is None or abs(row['ratio'] - 1.) > TOL:
            failures.append(f"sphere res {res} gap {xc}: measured {row['measured_mm']} mm, predicted {row['predicted_mm']:.2f} mm")
    return rows, failures


def hardening_check(r, pr, host, defaults):
    """13 gaps from 0.3 m to 2.1 m: the radius follows the formula at every one (no plateau, no step) and only grows."""
    rows, failures = [], []
    gaps = [.3 + .15 * i for i in range(13)]
    for xc in gaps:
        rows.append(dict(check='hardening', gap_m=round(xc, 3), **measure(r, pr, host, defaults, card(xc), 0., card_depth(xc), .04, 2048)))
    got = [row['measured_mm'] for row in rows]
    if any(g is None for g in got):
        failures.append('hardening: a gap has no clean penumbra')
        return rows, failures
    if any(b <= a for a, b in zip(got, got[1:])):
        failures.append('hardening: the penumbra does not grow strictly with the gap: ' + ', '.join('%.2f' % g for g in got))
    worst = max(abs(row['ratio'] - 1.) for row in rows)
    if worst > CARD_TOL:
        failures.append(f'hardening: a gap is {worst * 100:.0f}% off the formula (contour step or plateau)')
    if got[-1] < 8. * got[0]:
        failures.append('hardening: no contact hardening (far / near radius %.1f)' % (got[-1] / got[0]))
    return rows, failures


def resolution_check(r, pr, host, defaults):
    rows = [dict(check='resolution', **measure(r, pr, host, defaults, card(1.5), 0., card_depth(1.5), .1, res)) for res in (512, 1024, 2048)]
    got = [row['measured_mm'] for row in rows]
    failures = []
    if any(g is None for g in got) or max(got) / min(got) > 1.08:
        failures.append('resolution: the penumbra in metres depends on the map resolution: ' + str(got))
    return rows, failures


def cap_check(r, pr, host, defaults):
    """Predicted 18 texels against a cap of 6: the kernel stops at the cap."""
    row = dict(check='cap', **measure(r, pr, host, defaults, card(1.5), 0., card_depth(1.5), .03, 2048, cap=6.))
    e = lamp()
    row['measured_texels'] = None if row['measured_mm'] is None else row['measured_mm'] / 1000. / texel_m(e, 2048)
    failures = []
    if row['measured_texels'] is None or not 4.8 < row['measured_texels'] < 7.2:
        failures.append(f"cap: kernel radius {row['measured_texels']} texels with BDMergeSoftShadowMaxPenumbra 6")
    return [row], failures


def aspect_check(r, pr, host, defaults):
    """A projector twice as wide as tall on a square map (texels twice as wide as tall in world space): edges along
    both map axes get the same width in metres. The fixture halves the map's X focal length and the header's k_x."""
    e = lamp()
    rows, failures = [], []
    got = {}
    for axis in ('y', 'z'):
        xc, radius = 1.5, .05
        prim = card(xc) if axis == 'y' else S.Prim('card', 'obb', c=(xc, 0., 1.45 + 1.5), h=(.002, 3., 1.5))
        scene = scene_of([prim])
        want = predicted(radius, card_depth(xc))
        if axis == 'y':
            cam, d, n, coord, mask = edge_view(scene, 0., 4. * want)
        else:
            target = np.array([0, 0, 1.45])
            eye = np.array([2.5, 0, 1.45 - 1.2])
            cam = S.Camera(eye, target, 384, 384, math.degrees(2 * math.atan(4. * want / np.linalg.norm(eye - target))))
            g = S.cast_primary(scene, cam)
            _, d, n = S.encode_gbuffer(cam, g)
            coord = g['pos'][..., 2] - 1.45
            mask = (g['surf'] == 0) & (np.abs(g['pos'][..., 1]) < 2. * want) & (np.abs(coord) < 3.6 * want)
        r.native_map(scene, B.light_of(e), e['fov'], 1024)
        tex, ncam, res = r.native
        # Re-render the map with a 2:1 frustum: scale X of the projection by 1/2 (wider), as glm::perspective(aspect 2) does.
        wide = ncam.proj.copy()
        wide[0, 0] *= .5
        saved = ncam.proj
        ncam.proj = wide
        try:
            rerender_native(r, scene, ncam, res)
            centre = -(ncam.view @ np.array([*e['pos'], 1.]))[2]
            _, metric = host(radius, False, 0., 3., res, res, wide, centre)
            v = render(r, pr, cam, d, n, e, run_of(defaults, 64.), metric)
        finally:
            ncam.proj = saved
        got[axis] = kernel_radius_m(v, coord, mask, want)
        rows.append(dict(check='aspect', edge_across=axis, predicted_mm=want * 1000., measured_mm=None if got[axis] is None else got[axis] * 1000.,
                         k_x_over_k_y=metric[0] / metric[1]))
    if any(g is None for g in got.values()) or abs(got['y'] / got['z'] - 1.) > .1 or any(abs(g / want - 1.) > CARD_TOL for g in got.values()):
        failures.append('aspect: the penumbra differs along the two map axes of a 2:1 projector: ' + str(got))
    return rows, failures


def rerender_native(r, scene, cam, res):
    """Runner.native_map with the caller's projector camera (same rasteriser and state)."""
    gl = r.gl
    ra = r.raster
    tex = r._texture('native_depth', 12, 0x8CAC, res, res, 0x1902, 0x1406, None)
    gl.bind_fbo(0x8D40, ra.fbo)
    gl.fbo_tex(0x8D40, 0x8D00, 0x0DE1, tex, 0)
    ra.draw_buffer(0)
    ra.read_buffer(0)
    assert gl.fbo_status(0x8D40) == 0x8CD5
    ra.enable(0x0B71)
    ra.depth_mask(1)
    ra.depth_func(0x0201)
    gl.disable(0x0B44)
    gl.disable(0x0BE2)
    gl.viewport(0, 0, res, res)
    gl.use(ra.program)
    r._mat(ra.program, b'modelview_projection_matrix', cam.proj @ cam.view)
    gl.ui(gl.loc(ra.program, b'actorFxEnabled'), 0)
    ra.clear_depth(1)
    ra.clear(0x100)
    from relight_world_gl_check import mesh
    for p in scene.prims:
        data = np.ascontiguousarray(mesh(p), np.float32)
        gl.bind_buf(0x8892, ra.vbo)
        gl.buffer_data(0x8892, data.nbytes, data.ctypes.data, 0x88E4)
        idx = gl.attr_loc(ra.program, b'position')
        gl.enable_attr(idx)
        gl.attr_ptr(idx, 3, 0x1406, 0, 0, None)
        gl.draw(4, 0, len(data))
    gl.disable(0x0B71)
    gl.bind_fbo(0x8D40, r.fbo)
    r.native = (tex, cam, res)




def widen_check(r, host, defaults, edits=()):
    """B.widen_check's scene with the metric model: a card 0.8 m from the lamp over a plane at 60 degrees incidence.
    Reference: the same shader with an enormous bias in the widening term. Also: the bare slanted plane has no blocker."""
    a = math.radians(60.)
    toward = np.array([math.cos(a), math.sin(a), 0.])
    slanted = S.Prim('plane', 'obb', c=(-.01, 0, 1.45), h=(.01, 6, 6))
    scene = S.Scene('card over slanted plane', [slanted, S.Prim('card', 'obb', c=tuple(R.CENTRE + 2.2 * toward), h=(.004, .05, .05), yaw=60.)],
                    [], [], {}, np.empty((0, 3)))
    bare = S.Scene('slanted plane', [slanted], [], [], {}, np.empty((0, 3)))
    cam = S.Camera((3.5, 0, 1.45), R.CENTRE, 480, 480, 20)
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    plane = g['surf'] == 0
    gb = S.cast_primary(bare, cam)
    _, db, nb = S.encode_gbuffer(cam, gb)
    e = dict(B.entry(R.CENTRE + 3. * toward, 1., False), softness=0.)
    edits = list(edits)
    current = B.link(r, Mut(edits), 'widen current')
    reference = B.link(r, Mut(edits + [(METRIC_WIDEN, '    float tap_z = stc.z - (' + KERNEL_RADIUS + ' - 1.0) * 0.004;\n')]), 'widen reference')
    radius = B.link(r, Mut(edits + [RADIUS_PROBE]), 'kernel radius probe')
    rows, failures = [], []
    run = run_of(defaults, 24.)
    for kernel, vogel in (('poisson', False), ('vogel', True)):
        r.native_map(scene, B.light_of(e), e['fov'], 2048)
        _, metric = metric_of(host, r, e, .02)
        v = render(r, current, cam, d, n, e, run, metric, taps=16, vogel=vogel)
        want = render(r, reference, cam, d, n, e, run, metric, taps=16, vogel=vogel)
        pr_map = render(r, radius, cam, d, n, e, run, metric, taps=16, vogel=vogel) * 64.
        wide = plane & (pr_map > 1.5) & (pr_map < 63.9)
        row = dict(check='widen', kernel=kernel, wide_pixels=int(wide.sum()),
                   max_radius_texels=float(pr_map[wide].max()) if wide.any() else 1.,
                   shadowed_pixels=int((plane & (want < .05)).sum()), partial_pixels=int((plane & (want > .05) & (want < .95)).sum()),
                   max_difference=float(np.abs(v - want)[plane].max()), differing_pixels=int((np.abs(v - want)[plane] > 1.e-6).sum()))
        if row['wide_pixels'] < 500 or row['max_radius_texels'] < 4. or row['shadowed_pixels'] < 200 or row['partial_pixels'] < 200:
            failures.append(f'widen {kernel}: fixture does not reach a wide metric kernel')
        if row['differing_pixels']:
            failures.append(f"widen {kernel}: slanted receiver shadows itself inside the wide metric kernel ({row['differing_pixels']} px)")
        rows.append(row)
    # The largest source (2 m): a tap that took the plane itself for a blocker would read a gap of several texels here.
    r.native_map(bare, B.light_of(e), e['fov'], 2048)
    _, metric = metric_of(host, r, e, 2.)
    pr_bare = render(r, radius, cam, db, nb, e, run, metric, taps=16) * 64.
    v_bare = render(r, current, cam, db, nb, e, run, metric, taps=16)
    lit = (gb['surf'] == 0)
    row = dict(check='widen', kernel='bare slanted plane', pixels=int(lit.sum()), self_blocked_pixels=int((pr_bare[lit] > 1.001).sum() - (pr_bare[lit] > 63.9).sum()),
               dark_pixels=int((v_bare[lit] < .999).sum()))
    rows.append(row)
    if row['pixels'] < 50000 or row['self_blocked_pixels'] or row['dark_pixels']:
        failures.append(f"widen: a bare plane at 60 degrees is its own blocker ({row['self_blocked_pixels']} px search, {row['dark_pixels']} px dark)")
    return rows, failures


def noise_check(r, host, defaults, edits=()):
    """Fix 3's noise model on the metric kernel."""
    edits = list(edits)
    rows, failures = [], []
    e = lamp()
    run = run_of(defaults, 24.)
    slot_prog = Q.link_src(r, Q.Tree({'shadowUtil.glsl': edits}), Q.SLOT_SOURCE, 'metric slot probe')
    flat_prog = Q.link_src(r, Q.Tree({'shadowUtil.glsl': edits + [(
        '    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));\n', '    return 0.37;\n')]}),
        Q.SLOT_SOURCE, 'metric flat hash')

    def slot(prog, cam, d, n, metric, s, noise=1, retire=1, vogel=True):
        def setup(p):
            r.uniform(p, 'spot_shadow_metric[%d]' % s, *metric)
            r.gl.ui(r.gl.loc(p, b'soft_shadow_taps'), 16)
        tex, ncam, res = r.native

        def full(p):
            bias = np.eye(4)
            bias[:3, :3] *= .5
            bias[:3, 3] = .5
            r._mat(p, ('shadow_matrix[%d]' % (4 + s)).encode(), bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view))
            r.uniform(p, 'spot_shadow_softness[%d]' % s, 0.)
            r.uniform(p, 'spot_shadow_detail[%d]' % s, 0.)
            r.gl.ui(r.gl.loc(p, b'probe_slot'), s)
            r.uniform(p, 'probe_shift', 0., 0., 0.)
            r.gl.ui(r.gl.loc(p, b'spot_shadow_fade_model'), int(FADE))
            setup(p)
        return Q.projector(r, prog, cam, d, n, e, run, retire, noise, vogel=vogel, setup=full)

    # wide penumbra: the per-slot phase
    scene = scene_of([card(1.5)])
    want = predicted(.04, card_depth(1.5))
    cam, d, n, coord, mask = edge_view(scene, 0., 4. * want)
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    _, metric = metric_of(host, r, e, .04)
    a, b = slot(slot_prog, cam, d, n, metric, 0), slot(slot_prog, cam, d, n, metric, 1)
    part = mask & (a > .02) & (a < .98)
    differs = float((np.abs(a - b)[part] > 1e-6).mean()) if part.any() else 0.
    hashed = float((np.abs(a - slot(flat_prog, cam, d, n, metric, 0))[part] > 1e-6).mean()) if part.any() else 0.
    stepped = slot(slot_prog, cam, d, n, metric, 0, retire=0)
    retired_band, stepped_band = Q.dead_band(a, mask), Q.dead_band(stepped, mask)
    rows.append(dict(check='noise', scene='wide', pixels=int(part.sum()), slot1_differs=differs, depends_on_hash=hashed,
                     dead_band_retired=retired_band, dead_band_stepped=stepped_band))
    if part.sum() < 2000 or differs < .5:
        failures.append(f'noise: two slots with identical maps use the same rotation in the metric kernel ({differs:.2f})')
    if hashed < .5:
        failures.append(f'noise: a wide metric penumbra does not use the per-pixel rotation ({hashed:.2f})')
    if retired_band < 100 or stepped_band != 0:
        failures.append(f'noise: fill step on the metric kernel (retired {retired_band} sub-fill pixels, stepped {stepped_band})')
    # RenderSoftShadowNoiseModel 0 = the old rotation for the search AND the kernel (softShadowPhi is the one switch for
    # both): the plain per-pixel hash, no slot phase, so two slots are identical; and it does depend on the hash.
    a0, b0 = slot(slot_prog, cam, d, n, metric, 0, noise=0), slot(slot_prog, cam, d, n, metric, 1, noise=0)
    part0 = mask & (a0 > .02) & (a0 < .98)
    old_slots_identical = bool(np.array_equal(a0, b0))
    old_hashed = float((np.abs(a0 - slot(flat_prog, cam, d, n, metric, 0, noise=0))[part0] > 1e-6).mean()) if part0.any() else 0.
    rows.append(dict(check='noise', scene='wide, noise model 0', pixels=int(part0.sum()), slots_identical=old_slots_identical, depends_on_hash=old_hashed))
    if not old_slots_identical or old_hashed < .5:
        failures.append(f'noise: with noise model 0 the metric search or kernel does not use the old rotation (slots identical {old_slots_identical}, hash {old_hashed:.2f})')
    # contact shadow: the card 4 cm over the plane, radius about half a texel: no dependence on the hash at all
    scene = scene_of([card(.04)])
    cam, d, n, coord, mask = edge_view(scene, 0., .02)
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    _, metric = metric_of(host, r, e, .04)
    a, f = slot(slot_prog, cam, d, n, metric, 0), slot(flat_prog, cam, d, n, metric, 0)
    part = mask & ((a > .02) & (a < .98) | (f > .02) & (f < .98))
    same = float((np.abs(a - f)[part] < 1e-6).mean()) if part.any() else 0.
    rows.append(dict(check='noise', scene='contact', pixels=int(part.sum()), independent_of_hash=same))
    if part.sum() < 100 or same < .999:
        failures.append(f'noise: a contact shadow depends on the per-pixel hash in the metric kernel ({same:.3f}, {int(part.sum())} px)')
    return rows, failures


def identity_check(r, base, host, defaults):
    """No upload == BASE bit for bit; callers outside the surface model ignore an uploaded metric."""
    scene = scene_of([ball(1.5)])
    cam, d, n, coord, mask = edge_view(scene, ball_edge(1.5), .12, res=320, radial=True)
    checks = {}
    for kernel, soft, vogel in B.KERNELS:
        for detail in (False, True):
            e = lamp(softness=4. if soft else 0., detail=detail)
            r.native_map(scene, B.light_of(e), e['fov'], 2048)
            _, metric = metric_of(host, r, e, .1)
            run = dict(defaults, BDMergeSoftProjectorShadows=soft, BDMergeSoftShadowFill=Q.FILL, RenderSpotShadowDetailBias=True,
                       BDMergeSoftShadowMaxPenumbra=12.)
            tag = ('detail_' if detail else 'standard_') + kernel
            for noise, retire in ((0, 0), (1, 1)):
                for model in (0, B.MODEL_DEFAULT, B.MODEL_ALL):
                    ref = render(r, base, cam, d, n, e, run, None, taps=16, vogel=vogel, noise=noise, retire=retire, model=model)
                    off = render(r, r.visibility, cam, d, n, e, run, None, taps=16, vogel=vogel, noise=noise, retire=retire, model=model)
                    checks[f'no_upload_equals_base_model{model}_noise{noise}_{tag}'] = bool(np.array_equal(off, ref)) and bool((ref[mask] < .5).any())
            ref = render(r, base, cam, d, n, e, run, None, taps=16, vogel=vogel, airborne=True)
            got = render(r, r.visibility, cam, d, n, e, run, metric, taps=16, vogel=vogel, airborne=True)
            checks[f'airborne_ignores_metric_{tag}'] = bool(np.array_equal(got, ref))
            ref = render(r, base, cam, d, n, e, run, None, taps=16, vogel=vogel, rim=1.)
            got = render(r, r.visibility, cam, d, n, e, run, metric, taps=16, vogel=vogel, rim=1.)
            checks[f'shaft_rim_ignores_metric_{tag}'] = bool(np.array_equal(got, ref))
            ref = render(r, base, cam, d, n, e, run, None, taps=16, vogel=vogel, model=0)
            got = render(r, r.visibility, cam, d, n, e, run, metric, taps=16, vogel=vogel, model=0)
            checks[f'bias_model0_ignores_metric_{tag}'] = bool(np.array_equal(got, ref))
            if detail:
                ref = render(r, base, cam, d, n, e, run, None, taps=16, vogel=vogel, model=B.MODEL_DEFAULT)
                got = render(r, r.visibility, cam, d, n, e, run, metric, taps=16, vogel=vogel, model=B.MODEL_DEFAULT)
                checks[f'rig_detail_on_model1_ignores_metric_{tag}'] = bool(np.array_equal(got, ref))
            if not soft:   # no soft kernel requested (soft shadows off, no rig softness): the hard kernel, unchanged
                ref = render(r, base, cam, d, n, e, run, None, taps=16, vogel=vogel)
                got = render(r, r.visibility, cam, d, n, e, run, metric, taps=16, vogel=vogel)
                checks[f'hard_kernel_ignores_metric_{tag}'] = bool(np.array_equal(got, ref))
            elif not detail:   # positive control: an uploaded metric changes a lit surface
                ref = render(r, r.visibility, cam, d, n, e, run, None, taps=16, vogel=vogel)
                got = render(r, r.visibility, cam, d, n, e, run, metric, taps=16, vogel=vogel)
                checks['metric_changes_surface_' + kernel] = not np.array_equal(got, ref)
    return checks


def shaft_check(r):
    """New shaft engine, rim and march: byte-identical to BASE (the shaft program's own code path)."""
    import relight_volumetric_gl_check as V
    base_runner = V.Runner(G.SourceTree(BASE), r.gl)
    current = V.Runner(r.tree, r.gl)
    fixture = V.fixtures(320, 180)['outside']
    rows, failures = [], []
    for name, kwargs in dict(rim_only=dict(steps=0, rim=1), march_with_rim=dict(steps=24, rim=1, dither=1),
                             march_no_rim=dict(steps=24, rim=0, dither=1)).items():
        for vogel in (0, 1):
            uniforms = dict(soft_shadow_vogel=vogel, soft_shadow_taps=16, spot_shadow_bias_model=B.MODEL_ALL,
                            soft_shadow_fill_retire=1, soft_shadow_noise_model=1, spot_shadow_fade_model=int(FADE))
            ref = base_runner.run(fixture, uniforms=uniforms, **kwargs)[:2]
            got = current.run(fixture, uniforms=uniforms, **kwargs)[:2]
            same = all(np.array_equal(np.asarray(x), np.asarray(y)) for x, y in zip(got, ref))
            lit = int(np.count_nonzero(np.asarray(got[0])[:, :, :3].max(2) > 0))
            rows.append(dict(check='shaft', draw=name, vogel=vogel, identical=bool(same), lit_pixels=lit))
            if not same or lit < 100:
                failures.append(f'shaft {name} vogel {vogel}: ' + ('differs from BASE' if lit >= 100 else 'fixture draws nothing'))
    return rows, failures


# ---------------------------------------------------------------- review round 1: grazing retention, small blockers, cost
SLOPE_CLAMP = '        pr = min(pr, 1.0 + SPOT_METRIC_SLOPE_SHARE * gap / max(g_spot_widen_stc, 1.e-12));\n'
CLEAR_RETURN = '    if (clear) return 1.0;\n'
NEAR_TEST = 'all:< 0.875) { tap = t; how = 0.875; }'
NEAR_COLUMNS = '    vec3 column = (soft_shadow_vogel != 0 ? vec3(0.0, 2.0, 2.0) : vec3(-1.0, 1.0, 3.0)) * inv_res.x;\n'
NEAR_BLOCK = '    vec2 block = floor(stc.xy * proj_shadow_res - 0.5) * inv_res;'
SEARCH_FETCH = '    return textureLod(shadowMap, tap, 0.0);\n'


def retention_check(r, host, defaults, edits=()):
    """Codex round 1, finding 1. A lamp at 80 degrees incidence, 3 m away, source radius 0.4 m; a card 10 cm in front of
    the plane ALONG THE LIGHT RAY (1.7 cm above it). The metric radius there is about 8 texels, and the slope bias of a
    kernel that wide (about 14 cm) is more than the blocker is away. Truth is geometric (rays from the plane to the lamp
    against the card), stable over a 2 cm neighbourhood; nothing in the reference touches the shader's bias."""
    a = math.radians(80.)
    toward = np.array([math.cos(a), math.sin(a), 0.])
    lamp_pos = R.CENTRE + 3. * toward
    height = .1 * math.cos(a)
    blocker = S.Prim('card', 'obb', c=(height, 0., 1.45), h=(.0005, .15, .15))
    scene = scene_of([blocker])
    target = np.array([0., -.2, 1.45])
    eye = np.array([1., -2., 1.45])
    cam = S.Camera(eye, target, 480, 480, math.degrees(2 * math.atan(.12 / np.linalg.norm(eye - target))))
    g = S.cast_primary(scene, cam)
    _, d, n = S.encode_gbuffer(cam, g)
    plane = g['surf'] == 0
    pts = g['pos'][plane].astype(np.float64)

    def shadowed(points):
        o = points + np.array([1.e-4, 0, 0])
        v = lamp_pos - o
        dist = np.linalg.norm(v, axis=1)
        t = S.isect(blocker, o, v / dist[:, None])
        return t < dist
    base = shadowed(pts)
    stable = np.ones(len(pts), bool)
    # Along y one map texel is 9.5 mm of plane at this angle: the shadow edge sits up to 3.2 cm inside the geometric one
    # (the surface bias model's normal offset, here 0.57 cm, times tan 80) and the clamped kernel spans 3 cm more.
    for dy, dz in ((-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)):
        stable &= shadowed(pts + np.array([0., .065 * dy, .02 * dz])) == base
    dark = np.zeros(plane.shape, bool)
    lit = np.zeros(plane.shape, bool)
    dark[plane] = stable & base
    lit[plane] = stable & ~base
    e = dict(B.entry(lamp_pos, 1., False), softness=0.)
    edits = list(edits)
    current = B.link(r, Mut(edits), 'retention current')
    radius = B.link(r, Mut(edits + [RADIUS_PROBE]), 'retention radius probe')
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    _, metric = metric_of(host, r, e, .4)
    run = run_of(defaults, 16.)
    rows, failures = [], []
    for kernel, vogel in (('poisson', False), ('vogel', True)):
        v = render(r, current, cam, d, n, e, run, metric, taps=16, vogel=vogel)
        pr_map = render(r, radius, cam, d, n, e, run, metric, taps=16, vogel=vogel) * 64.
        row = dict(check='retention', kernel=kernel, expected_dark=int(dark.sum()), clear_pixels=int(lit.sum()),
                   retention=float((v[dark] < .5).mean()) if dark.any() else None,
                   mean_visibility_in_shadow=float(v[dark].mean()) if dark.any() else None,
                   mean_lit_visibility=float(v[lit].mean()) if lit.any() else None,
                   false_dark_percent=float((v[lit] < .98).mean() * 100.) if lit.any() else None,
                   median_radius_texels_in_shadow=float(np.median(pr_map[dark])) if dark.any() else None)
        rows.append(row)
        if row['expected_dark'] < 3000 or row['clear_pixels'] < 20000:
            failures.append(f'retention {kernel}: fixture has no stable shadow')
        elif row['median_radius_texels_in_shadow'] < 1.5:
            failures.append(f"retention {kernel}: the kernel is not wide here ({row['median_radius_texels_in_shadow']:.2f} texels)")
        elif row['retention'] < .98 or row['mean_visibility_in_shadow'] > .01:
            failures.append(f"retention {kernel}: a grazing shadow is lit ({row['retention'] * 100:.1f}% of {row['expected_dark']} px kept, "
                            f"mean visibility inside {row['mean_visibility_in_shadow']:.3f})")
        elif row['mean_lit_visibility'] < .995 or row['false_dark_percent'] > .5:
            failures.append(f"retention {kernel}: the grazing receiver shadows itself (lit mean {row['mean_lit_visibility']:.4f})")
    return rows, failures


def blocker_texel_m(e, res, depth):
    return 2. * (depth + .125 / math.tan(e['fov'] * .5) - .125) * math.tan(e['fov'] * .5) / res


def small_scene(texels, shift, e, res=2048):
    """A square card `texels` map texels wide, 1.5 m from the lamp, moved by `shift` texels (sub-texel sweep)."""
    side = texels * blocker_texel_m(e, res, 1.5)
    step = shift * blocker_texel_m(e, res, 1.5)
    return scene_of([S.Prim('speck', 'obb', c=(1.5, step, 1.45 + .37 * step), h=(.0005, side / 2, side / 2))])


def small_blocker_check(r, host, defaults, edits=()):
    """Codex round 1, finding 2. A blocker 1 to 3 map texels wide whose true penumbra is far wider than the 16 texel
    cap: every receiver that sees it at all must see it through the WIDE kernel (a faint smear), never through the
    1-texel kernel (a hard dark core that pops as the receiver moves). The image is a dense sub-texel sweep of
    receivers (6 pixels per map texel), repeated for blockers moved by fractions of a texel."""
    e = lamp()
    edits = list(edits)
    current = B.link(r, Mut(edits), 'small current')
    radius = B.link(r, Mut(edits + [RADIUS_PROBE]), 'small radius probe')
    run = run_of(defaults, 16.)
    rows, failures = [], []
    for texels in (1, 2, 3):
        for kernel, vogel in (('poisson', False), ('vogel', True)):
            worst = dict(min_visibility=1., narrow_shadowed_pixels=0, shadowed_pixels=0, under_blocker_pixels=0, unseen_under_blocker_pixels=0)
            for shift in (0., .25, .5, .75):
                scene = small_scene(texels, shift, e)
                cam, d, n, coord, mask = edge_view(scene, 0., .04, res=256)
                r.native_map(scene, B.light_of(e), e['fov'], 2048)
                _, metric = metric_of(host, r, e, .04)
                v = render(r, current, cam, d, n, e, run, metric, taps=16, vogel=vogel)
                pr_map = render(r, radius, cam, d, n, e, run, metric, taps=16, vogel=vogel) * 64.
                shadowed = mask & (v < .999)
                # Receivers whose ray to the lamp hits the blocker (geometry, not the shader): the search must have found
                # it there, i.e. the probe reads a kernel radius (at most the cap), not the lit return (64).
                g = S.cast_primary(scene, cam)
                points = g['pos'][mask].astype(np.float64) + np.array([1.e-4, 0, 0])
                to_lamp = LIGHT - points
                reach = np.linalg.norm(to_lamp, axis=1)
                under = np.zeros(mask.shape, bool)
                under[mask] = S.isect(scene.prims[1], points, to_lamp / reach[:, None]) < reach
                worst['under_blocker_pixels'] += int(under.sum())
                worst['unseen_under_blocker_pixels'] += int((under & (pr_map > 16.5)).sum())
                worst['min_visibility'] = min(worst['min_visibility'], float(v[mask].min()))
                worst['narrow_shadowed_pixels'] += int((shadowed & (pr_map < 12.)).sum())
                worst['shadowed_pixels'] += int(shadowed.sum())
            row = dict(check='small', blocker_texels=texels, kernel=kernel, cap_texels=16., **worst)
            rows.append(row)
            if texels > 1 and row['shadowed_pixels'] < 200:
                failures.append(f'small {texels} texel {kernel}: the blocker casts nothing (lost)')
            if row['min_visibility'] < .6:
                failures.append(f"small {texels} texel {kernel}: dark core (visibility {row['min_visibility']:.2f}) under a blocker whose penumbra is 16 texels")
            if texels > 1 and (row['under_blocker_pixels'] < 50 or row['unseen_under_blocker_pixels']):
                failures.append(f"small {texels} texel {kernel}: {row['unseen_under_blocker_pixels']} of {row['under_blocker_pixels']} receivers under the blocker "
                                f"did not find it (lit hole)")
            if row['narrow_shadowed_pixels']:
                failures.append(f"small {texels} texel {kernel}: {row['narrow_shadowed_pixels']} px shadowed through a kernel under 12 texels (pop)")
    return rows, failures


def clear_check(r, host, defaults, edits=()):
    """The lit return (nothing occluded in the search or in the texel block the 1-texel kernel reads) is exact: the
    same shader WITHOUT that return, i.e. always running the kernel, renders the same bits. Card edge, sphere edge and
    1 to 3 texel blockers; both kernels; both noise models."""
    e = lamp()
    edits = list(edits)
    current = B.link(r, Mut(edits), 'clear current')
    always = B.link(r, Mut(edits + [(CLEAR_RETURN, '')]), 'clear: kernel always run')
    run = run_of(defaults, 16.)
    rows, failures = [], []
    scenes = [('card', scene_of([card(1.5)]), 0., False), ('sphere', scene_of([ball(1.5)]), ball_edge(1.5), True)]
    scenes += [('speck %d' % t, small_scene(t, .3, e), 0., False) for t in (1, 2, 3)]
    for name, scene, edge, radial in scenes:
        cam, d, n, coord, mask = edge_view(scene, edge, .04, res=256, radial=radial)
        r.native_map(scene, B.light_of(e), e['fov'], 2048)
        _, metric = metric_of(host, r, e, .04)
        for kernel, vogel in (('poisson', False), ('vogel', True)):
            for noise in (0, 1):
                a = render(r, current, cam, d, n, e, run, metric, taps=16, vogel=vogel, noise=noise)
                b = render(r, always, cam, d, n, e, run, metric, taps=16, vogel=vogel, noise=noise)
                row = dict(check='clear', scene=name, kernel=kernel, noise_model=noise, lit_pixels=int((a == 1.).sum()),
                           differing_pixels=int((a != b).sum()), max_difference=float(np.abs(a - b).max()))
                rows.append(row)
                if row['differing_pixels']:
                    failures.append(f"clear {name} {kernel} noise {noise}: the lit return differs from the kernel on {row['differing_pixels']} px "
                                    f"(up to {row['max_difference']:.3f})")
    return rows, failures


COUNT_DECL = 'uniform float spot_shadow_detail[SPOT_SHADOW_COUNT];\n'
COUNT_HELPERS = ('float g_compare_calls = 0.0;\n'
                 'float countedCompare(sampler2DShadow s, vec3 p) { g_compare_calls += 1.0; return texture(s, p); }\n'
                 'float countedCompareLod(sampler2DShadow s, vec3 p, float l) { g_compare_calls += 1.0; return textureLod(s, p, l); }\n')


class CountTree(G.SourceTree):
    """shadowUtil.glsl (with optional edits) where every shadow-map compare is counted and sampleSpotShadow returns the count."""
    def __init__(self, edits=()):
        super().__init__('WORKTREE')
        self.edits = list(edits)

    def read(self, path):
        text = super().read(path)
        if Path(path).name != 'shadowUtil.glsl':
            return text
        text = apply_edits(text, self.edits)
        calls = len(re.findall(r'\btexture(?:Lod)?\(shadowMap', text))
        text = re.sub(r'\btextureLod\(shadowMap', 'countedCompareLod(shadowMap', text)
        text = re.sub(r'\btexture\(shadowMap', 'countedCompare(shadowMap', text)
        assert calls > 40 and text.count(COUNT_DECL) == 1
        text = text.replace(COUNT_DECL, COUNT_DECL + COUNT_HELPERS)
        sample = function(text, 'float sampleSpotShadow(')
        assert sample.count('    return shadow;\n') == 1
        return text.replace(sample, sample.replace('    return shadow;\n', '    return g_compare_calls;\n'))


def cost_check(r, host, defaults, edits=()):
    """Codex round 1, finding 3 (a): shadow-map compares per pixel per light, counted by the shader itself (every
    texture() on a shadow map in shadowUtil.glsl goes through a counter), old model vs metric, on the sphere + plane.
    typical = a lit pixel (the median: most of any frame), worst = the maximum over the image (umbra and penumbra)."""
    e = lamp()
    scene = scene_of([ball(1.5)])
    cam, d, n, coord, mask = edge_view(scene, ball_edge(1.5), .2, res=256, radial=True)
    counter = B.link(r, CountTree(edits), 'compare counter')
    r.native_map(scene, B.light_of(e), e['fov'], 2048)
    _, metric = metric_of(host, r, e, .1)
    run = run_of(defaults, 16.)
    rows, failures = [], []
    for kernel, vogel, taps in (('poisson', False, 12), ('vogel', True, 16)):
        old = render(r, counter, cam, d, n, e, run, None, taps=16, vogel=vogel)[mask]
        new = render(r, counter, cam, d, n, e, run, metric, taps=16, vogel=vogel)[mask]
        row = dict(check='cost', kernel=kernel, kernel_taps=taps,
                   old_typical=float(np.median(old)), old_worst=float(old.max()), old_mean=float(old.mean()),
                   metric_typical=float(np.median(new)), metric_worst=float(new.max()), metric_mean=float(new.mean()),
                   metric_typical_ceiling=12., metric_worst_ceiling=float(12 + 5 + taps))
        rows.append(row)
        if row['old_typical'] != 4 + taps or row['old_worst'] != 28 + taps:
            failures.append(f"cost {kernel}: the counter does not read the old model as 4+{taps} / 28+{taps} ({row['old_typical']}, {row['old_worst']})")
        if row['metric_typical'] > row['metric_typical_ceiling'] or row['metric_typical'] > row['old_typical']:
            failures.append(f"cost {kernel}: a lit pixel costs {row['metric_typical']:.0f} compares (ceiling {row['metric_typical_ceiling']:.0f}, old {row['old_typical']:.0f})")
        if row['metric_worst'] > row['metric_worst_ceiling'] or row['metric_worst'] > row['old_worst']:
            failures.append(f"cost {kernel}: the worst pixel costs {row['metric_worst']:.0f} compares (ceiling {row['metric_worst_ceiling']:.0f}, old {row['old_worst']:.0f})")
        if row['metric_mean'] > row['old_mean']:
            failures.append(f"cost {kernel}: the mean over the image rose ({row['metric_mean']:.1f} vs {row['old_mean']:.1f})")
    return rows, failures


SIXTEEN_SOURCE = """
out vec4 frag_color;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz;vec3 n=getNorm(tc).xyz;float v=0.;
for(int i=0;i<16;++i)v+=sampleSpotShadow(p,n,i,tc);frag_color=vec4(v/16.);}
"""
PASS_SOURCE = """
out vec4 frag_color;
uniform int probe_slot;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz;vec3 n=getNorm(tc).xyz;
frag_color=vec4(sampleSpotShadow(p,n,probe_slot,tc));}
"""
TIMING_LIMIT_MS = 2.0   # per sixteen full-screen lights at 4K; the aim is 1 ms, the measurement repeats to about +-0.5 ms


def timing_check(r, host, defaults, repeats=20, shapes=('passes', 'one pass'), caps=(6., 16.), tree=None):
    """Codex round 1, finding 3 (b): GPU time (GL_TIME_ELAPSED) of SIXTEEN overlapping projector slots over a
    3840 x 2160 screen, production sampler, 16-slot layout. Three programs' worth of numbers per row:
      base    BASE's shader (no metric code at all), old model
      off     this tree's shader with RenderSpotShadowPenumbraModel 0 (no metric uploaded)
      metric  this tree's shader with the metric uploaded
    The three are timed in rotation, `repeats` times, and the MINIMUM of each is kept (another process on the GPU only
    ever adds time). shape 'passes' = sixteen full-screen draws, one slot each: the viewer's shape (one light per draw).
    shape 'one pass' = one draw whose shader samples all sixteen slots: not a viewer path, reported as a stress figure.
    'typical' = the sphere + plane (mostly lit, some umbra and penumbra). 'worst' = every pixel in the umbra of a wall
    0.4 m from the lamp, where BOTH models run their widest kernel: the old model's softness is cap - 1, so its radius
    there is the cap too and the difference is the search, not the radius. Max penumbra 6 (the default) and 16.
    Gated: 'passes' shape, metric - base and off - base, each at most TIMING_LIMIT_MS for the sixteen lights together."""
    gl = r.gl
    defines = '#define SPOT_SHADOW 1\n#define SPOT_SHADOWS_16 1\n'
    e = lamp()
    rows, failures = [], []
    wall = S.Prim('wall', 'obb', c=(2.6, 0., 1.45), h=(.002, 3., 3.))
    views = []
    for name, prim, eye, target in (('typical', ball(1.5), (3.5, 1.2, 1.45), (0, .3, 1.45)), ('worst', wall, (1.2, 1.2, 1.45), (0, 0, 1.45))):
        scene = scene_of([prim])
        cam = S.Camera(eye, target, 3840, 2160, 30)
        g = S.cast_primary(scene, cam)
        _, d, n = S.encode_gbuffer(cam, g)
        views.append((name, scene, cam, d, n, g['surf'] == 0))
    for shape in shapes:
        passes = shape == 'passes'
        source = PASS_SOURCE if passes else SIXTEEN_SOURCE
        programs = {'base': Q.link_src(r, G.SourceTree(BASE), source, 'timing base', defines=defines),
                    'current': Q.link_src(r, tree or r.tree, source, 'timing current', defines=defines)}
        for name, scene, cam, d, n, plane in views:
            r.native_map(scene, B.light_of(e), e['fov'], 2048)
            tex, ncam, res = r.native
            _, metric = metric_of(host, r, e, .1)
            bias = np.eye(4)
            bias[:3, :3] *= .5
            bias[:3, 3] = .5
            matrix = bias @ ncam.proj @ ncam.view @ np.linalg.inv(cam.view)
            for cap in caps:
                for kernel, vogel in (('poisson', False), ('vogel', True)):
                    run = run_of(defaults, cap, BDMergeSoftShadowSoftness=cap - 1.)

                    def bind(prog, m):
                        r.target_float(cam)
                        r._begin(prog)
                        r.inputs(prog, cam, d, n)
                        r.native_uniforms(prog, cam, e, run)
                        for slot in range(16):
                            gl.ui(gl.loc(prog, ('shadowMap%d' % (4 + slot)).encode()), 12)
                            r._mat(prog, ('shadow_matrix[%d]' % (4 + slot)).encode(), matrix)
                            r.uniform(prog, 'spot_shadow_softness[%d]' % slot, 0.)
                            r.uniform(prog, 'spot_shadow_detail[%d]' % slot, 0.)
                            r.uniform(prog, 'spot_shadow_metric[%d]' % slot, *m)
                        for key, value in (('soft_shadow_vogel', int(vogel)), ('soft_shadow_taps', 16), ('spot_shadow_bias_model', B.MODEL_ALL),
                                           ('soft_shadow_noise_model', 1), ('soft_shadow_fill_retire', 1), ('spot_shadow_fade_model', int(FADE))):
                            gl.ui(gl.loc(prog, key.encode()), value)
                        gl.ui(gl.loc(prog, b'probe_slot'), 0)

                    def draws(prog):
                        if not passes:
                            gl.draw(4, 0, 3)
                            return
                        slot_loc = gl.loc(prog, b'probe_slot')
                        for slot in range(16):
                            gl.ui(slot_loc, slot)
                            gl.draw(4, 0, 3)
                    modes = (('base', programs['base'], (0., 0., 0., 0.)), ('off', programs['current'], (0., 0., 0., 0.)),
                             ('metric', programs['current'], metric))
                    row = dict(check='timing', shape=shape, scene=name, kernel=kernel, cap_texels=cap, resolution='3840x2160', slots=16)
                    best = {}
                    for mode, prog, m in modes:
                        bind(prog, m)
                        v = r.read_float(cam)[..., 0]
                        row[mode + '_dark_share'] = float((v[plane] < .01).mean())
                    for _ in range(repeats):
                        for mode, prog, m in modes:
                            bind(prog, m)
                            stats = {}
                            r._timed('ms', stats, lambda: draws(prog))
                            best[mode] = min(best.get(mode, 1.e9), stats['ms'])
                    row.update({mode + '_ms': best[mode] for mode in best})
                    row['off_minus_base_ms'] = best['off'] - best['base']
                    row['metric_minus_base_ms'] = best['metric'] - best['base']
                    rows.append(row)
                    if passes:
                        for key, label in (('metric_minus_base_ms', 'the metric model'), ('off_minus_base_ms', 'the setting at 0')):
                            if row[key] > TIMING_LIMIT_MS:
                                failures.append(f"timing {name} {kernel} cap {cap:.0f}: {label} costs {row[key]:.2f} ms more than BASE over 16 lights at 4K "
                                                f"(base {best['base']:.2f}, off {best['off']:.2f}, metric {best['metric']:.2f} ms; limit {TIMING_LIMIT_MS} ms)")
                    if name == 'worst' and row['base_dark_share'] < .95:
                        failures.append('timing worst: the fixture is not in the umbra')
    return rows, failures


def layer_contracts(tree):
    """GLM round 1, finding 3: the metric layers end in an exact byte compare with BASE, and are really present."""
    from director_actor_bar_check import reverse_softshadow_metric, reverse_softshadow_metric_shader, reverse_softshadow_bias
    pipeline = tree.read(PIPELINE).replace('\r\n', '\n')
    shader = tree.read(SHADER).replace('\r\n', '\n')
    base_pipeline = G.SourceTree(BASE).read(PIPELINE).replace('\r\n', '\n')
    base_shader = G.SourceTree(BASE).read(SHADER).replace('\r\n', '\n')
    root = Path(__file__).parent
    pipeline_hunks = json.loads((root / 'softshadow_metric_hunks.json').read_text(encoding='utf-8'))['files']['pipeline.cpp']
    shader_hunks = json.loads((root / 'softshadow_metric_shader_hunks.json').read_text(encoding='utf-8'))['files']['shadowUtil.glsl']

    def refused(fn):
        try:
            return fn() is None
        except AssertionError:
            return True
    return {
        'pipeline_hunk_present_once': all(pipeline.count(h['after']) == 1 for h in pipeline_hunks) and len(pipeline_hunks) == 1,
        'shader_hunks_present_once': all(shader.count(h['after']) == 1 for h in shader_hunks) and len(shader_hunks) == 2,
        'pipeline_reverses_to_base_bytes': reverse_softshadow_metric(pipeline, 'pipeline.cpp') == base_pipeline and pipeline != base_pipeline,
        'shader_reverses_to_base_bytes': reverse_softshadow_metric_shader(shader) == base_shader and shader != base_shader,
        'whole_chain_reverses_to_the_oldest_base': reverse_softshadow_bias(pipeline, 'pipeline.cpp') ==
            G.SourceTree('fb616f2519c').read(PIPELINE).replace('\r\n', '\n'),
        'pipeline_unrelated_edit_refused': refused(lambda: reverse_softshadow_bias(pipeline + '\n// planted\n', 'pipeline.cpp')),
        'pipeline_edit_inside_the_hunk_refused': refused(lambda: reverse_softshadow_bias(
            pipeline.replace('static_cast<F32>(depth_a);', 'static_cast<F32>(depth_a) ;', 1), 'pipeline.cpp')),
        'shader_unrelated_edit_refused': refused(lambda: B.reverse_fade_layer(shader + '\n// planted\n')),
        'shader_edit_inside_a_hunk_refused': refused(lambda: B.reverse_fade_layer(shader.replace(SLOPE_CLAMP, SLOPE_CLAMP.replace('1.e-12', '1.e-11'), 1))),
    }


# ---------------------------------------------------------------- source contracts
UPLOAD_RADIUS = ('const double radius = ALRelight::shadowSourceRadius(area_source_radius(volume)) *\n'
                 '                ALRelight::spotPenumbraTrim(fixture_sized, spot_shadow_softness[i], BDMergeSoftShadowSoftness);')
CUBE_RADIUS = re.compile(r'const F32 source = area_source_radius\(volume\);(?:(?!const F32 source)[\s\S])*?world_info\[i\]\.set\([^;]*?'
                         r'llmin\(source > 0\.f \? source : ([0-9.]+)f,([0-9.]+)f\),projectorDetailShadows\(id\)')


def source_contracts(tree, header_text=None, pipeline_text=None):
    shader = tree.read(SHADER)
    base = G.SourceTree(BASE).read(SHADER)
    header = header_text if header_text is not None else tree.read(HEADER)
    pipeline = pipeline_text if pipeline_text is not None else tree.read(PIPELINE)
    area = tree.read(AREA_F)
    settings = settings_defaults()
    surface = function(shader, 'float pcfSpotShadowSurface(')
    metric_fn = function(shader, 'float spotMetricPenumbra(')
    kernel_fn = function(shader, 'float pcfSpotShadowMetric(')
    tap_fn = function(shader, 'float spotMetricSearchTap(')
    samplers = lambda text: sorted(re.findall(r'^\s*uniform\s+sampler\w+\s+\w+', text, re.M))
    default = re.search(r'constexpr double kShadowSourceDefaultRadius = ([0-9.]+);', header)
    maximum = re.search(r'constexpr double kShadowSourceMaxRadius = ([0-9.]+);', header)
    unit = re.search(r'constexpr double kSpotPenumbraSoftnessUnit = ([0-9.]+);', header)
    cube = CUBE_RADIUS.findall(pipeline)
    area_default = re.search(r'float source_radius = area_lights\[light\]\.w > 0\.0 \? area_lights\[light\]\.w : ([0-9.]+);', area)
    upload = pipeline[pipeline.index('// [SoftShadow metric] RenderSpotShadowPenumbraModel 1'):pipeline.index('bindSunShadowState(shader, nullptr, SunShadowStage::Directions);')] \
        if '// [SoftShadow metric] RenderSpotShadowPenumbraModel 1' in pipeline else ''
    return {
        # every function outside the two touched ones is BASE's, statement for statement
        'other_functions_verbatim': all(function(shader, s) == function(base, s) for s in (
            'float pcfSpotShadow(', 'float estimateSpotBlockerGap(', 'float detailSpotTap(', 'float estimateDetailSpotBlockerGap(',
            'vec4 spotSurfaceReceiver(', 'vec4 detailSpotReceiver(', 'float sampleSpotShadow(', 'float pcfSpotShadowConservative(',
            'float sampleSpotShadowConservative(', 'float softShadowPhi(', 'float softShadowJitter(', 'float pcfShadow(', 'float pcfShadowC(')),
        'surface_kernel_is_base_plus_the_metric_call': surface.count(METRIC_CALL) == 1 and
            surface.replace(METRIC_CALL, '') == function(base, 'float pcfSpotShadowSurface('),
        'metric_reached_only_from_surface_kernel': shader.count('pcfSpotShadowMetric(') == 2 and shader.count('spotMetricPenumbra(') == 2 and
            kernel_fn.count('spotMetricPenumbra(shadowMap, stc, m, texel, clear)') == 1,
        'slot_uniform_read_once': shader.count('spot_shadow_metric[') == 2 and surface.count('spot_shadow_metric[int(g_spot_slot + 0.5)]') == 1,
        # the metric path never takes an implicit-LOD fetch (it sits behind branches and leaves early on lit pixels)
        'metric_fetches_are_explicit_lod': all('texture(' not in fn for fn in (metric_fn, kernel_fn, tap_fn)) and
            (metric_fn + kernel_fn + tap_fn).count('textureLod(shadowMap,') == 6 + 1 + 1 + 2,
        'widen_follows_metric_radius': kernel_fn.count(METRIC_WIDEN) == 1 and kernel_fn.count('tap_z), 0.0)') == 2,
        'noise_model_kept': kernel_fn.count('softShadowPhi(pr, slot_phase)') == 1 and kernel_fn.count('softShadowJitter(fract(pos_screen.y * 0.666666666))') == 1 and
            'softShadowPhi(search, g_spot_slot * SOFT_NOISE_SLOT_PHASE + SPOT_METRIC_SEARCH_PHASE)' in metric_fn,
        'no_new_sampler': samplers(shader) == samplers(base),
        'shader_default_is_old_model': 'uniform vec4 spot_shadow_metric[SPOT_SHADOW_COUNT];' in shader and '= vec4' not in shader.split('uniform vec4 spot_shadow_metric')[1].split(';')[0],
        'frozen_classic_copies_untouched': all(tree.read(p) == G.SourceTree(BASE).read(p) for p in Q.CLASSIC),
        'zero_normal_and_rim_gate_unchanged': B.GATE in function(shader, 'float sampleSpotShadow('),
        'setting_default_metric': settings.get('RenderSpotShadowPenumbraModel') == 1,
        'softness_unit_is_setting_default': unit is not None and float(unit.group(1)) == float(settings.get('BDMergeSoftShadowSoftness')),
        # one source radius: the cube, the area trace and the projector all start from area_source_radius(volume), and
        # the default for an unsized light and the cap are the same numbers in the cube upload, the area trace shader
        # and the header function the projector upload calls
        'cube_path_radius_rule': len(cube) == 1 and float(cube[0][0]) == .1 and float(cube[0][1]) == 2.,
        'projector_uses_shared_radius': upload.count(UPLOAD_RADIUS) == 1 and pipeline.count('ALRelight::shadowSourceRadius(') == 1,
        'area_trace_uses_same_authored_radius': 'lights[i].set(centre.x,centre.y,centre.z,area_source_radius(volume));' in pipeline,
        'one_default_for_unsized_lights': None not in (default, maximum, area_default) and len(cube) == 1 and
            float(default.group(1)) == float(area_default.group(1)) == float(cube[0][0]) and float(maximum.group(1)) == float(cube[0][1]),
        'upload_site': upload.count('static LLCachedControl<U32> spot_penumbra_model(gSavedSettings, "RenderSpotShadowPenumbraModel", 1);') == 1 and
            upload.count('LLRenderTarget* metric_map = spot_penumbra_model != 0 ? getSpotShadowTarget(0) : nullptr;') == 1 and
            upload.count('if (metric_map)') == 1 and
            upload.count('shader.uniform4fv(LLStaticHashedString("spot_shadow_metric"), MAX_SPOT_SHADOWS, spot_shadow_metric);') == 1 and
            pipeline.count('"spot_shadow_metric"') == 1 and
            'ALRelight::spotPenumbraScale(radius, metric_map->getWidth(), projection[0][0], projection[3][2])' in upload and
            'ALRelight::spotPenumbraScale(radius, metric_map->getHeight(), projection[1][1], projection[3][2])' in upload and
            'ALRelight::spotDepthA(projection[2][2])' in upload and 'ALRelight::spotPenumbraSetback(-centre.z, projection[3][2])' in upload and
            'mShadowProjection[map_index]' in upload and 'mShadowModelview[map_index]' in upload and 'mShadowSpotLight[i]' in upload,
        # the host above runs the same statements
        'host_mirrors_upload': all(s in HOST for s in (
            'ALRelight::shadowSourceRadius(authored) *', 'ALRelight::spotPenumbraScale(radius, res_x, p00, p32)',
            'ALRelight::spotPenumbraScale(radius, res_y, p11, p32)', 'ALRelight::spotDepthA(p22)', 'ALRelight::spotPenumbraSetback(centre, p32)')),
    }


def trim_check(host):
    """ALRelight::spotPenumbraTrim through the compiled header: (authored, fixture, override, global) -> effective radius."""
    eye = np.eye(4)
    eye[2, 3] = -1.
    cases = {'unsized_default_softness': ((0., 0, 0., 3.), .1), 'unsized_softness_6': ((0., 0, 0., 6.), .2),
             'rig_override_wins': ((0., 0, 1.5, 6.), .05), 'authored_radius': ((.25, 0, 0., 3.), .25),
             'fixture_ignores_softness': ((.4, 1, 8., 1.), .4), 'capped_at_2m': ((5., 1, 0., 3.), 2.), 'zero_softness': ((.3, 0, 0., 0.), 0.)}
    return {name: abs(host(*args, 1024, 1024, eye, 0.)[0] - want) < 1e-12 for name, (args, want) in cases.items()}


# ---------------------------------------------------------------- negatives
METRIC_CALL = '''        // [SoftShadow metric] A slot with a metric (RenderSpotShadowPenumbraModel 1) takes the
        // metric search and kernel; x or y <= 0 (setting off, or no upload) runs the old
        // statements below, unchanged.
        vec4 metric = spot_shadow_metric[int(g_spot_slot + 0.5)];
        if (min(metric.x, metric.y) > 0.0) return pcfSpotShadowMetric(shadowMap, stc.xyz, pos_screen, metric);
'''
METRIC_IF = '        if (min(metric.x, metric.y) > 0.0) return pcfSpotShadowMetric('
OLD_GAP = '        float gap_scale = clamp(estimateSpotBlockerGap(shadowMap, stc.xyz, softness), 0.0, 1.0);\n'
LEVER = '        float lever = max(1.0 - m.w * (m.z - (stc.z - gap)), SPOT_METRIC_MIN_LEVER);\n'
LIFT = '    tap.z = stc.z - max(at.z * search - 1.0, 0.0) * g_spot_widen_stc;\n'
BISECT = '''        for (int s = 0; s < SPOT_METRIC_BISECT_STEPS; ++s)
        {
            float mid = 0.5 * (lo + hi);
            if (textureLod(shadowMap, vec3(tap.xy, tap.z + shift - mid), 0.0) < how) lo = mid; else hi = mid;
        }
        float gap = stc.z - tap.z + max(0.5 * (lo + hi) - shift, 0.0);   // d_r - d_b
'''
SEVEN_LEVELS = '''        float sep = 0.0;
        for (int s = 1; s <= 6; ++s)
            if (textureLod(shadowMap, vec3(tap.xy, max(tap.z - hi * float(s) / 6.0, 0.0)), 0.0) < how) sep += 1.0;
        float gap = stc.z - tap.z + hi * sep / 6.0;
'''
SEARCH_PHI = 'softShadowPhi(search, g_spot_slot * SOFT_NOISE_SLOT_PHASE + SPOT_METRIC_SEARCH_PHASE)'
METRIC_TEXEL = '    texel = max(pr * shape, vec2(1.0)) / proj_shadow_res;\n'
METRIC_FILL = '    if (soft_shadow_fill_retire == 0 && soft_shadow_fill > 0.0 && shadow > 0.0)\n'
LEGACY_KERNEL = ('        float pr = clamp(1.0 + softness * gap_scale,\n                          1.0, max(soft_shadow_max, 1.0));\n'
                 '        vec2 texel = pr / proj_shadow_res;\n        texel.y *= 1.5;\n')


def shader_negatives():
    """name -> (shadowUtil.glsl edits, the check that must reject it)."""
    return {
        # the old model: the dimensionless 7-level gap drives the radius again
        'old_seven_level_gap': ([(METRIC_IF, METRIC_IF.replace('> 0.0)', '> 1.e30)'))], 'formula'),
        # the metric search quantised to 7 levels of the capped range: contour steps
        'seven_level_bisection': ([(BISECT, SEVEN_LEVELS)], 'hardening'),
        'no_setback': ([(LEVER, '        float lever = 1.0;\n')], 'formula'),
        'radius_from_receiver_depth': ([('        pr = clamp(k_max * gap / lever, 1.0, cap);\n', '        pr = clamp(k_max * stc.z * 0.02, 1.0, cap);\n')], 'hardening'),
        'gap_without_the_bisection': ([(BISECT.splitlines(True)[-1], '        float gap = stc.z - tap.z + 0.5 * hi;\n')], 'hardening'),
        'kernel_keeps_texel_shape': ([(METRIC_TEXEL, '    texel = vec2(pr) / proj_shadow_res;\n')], 'aspect'),
        'widen_uses_old_radius': ([(CLEAR_RETURN, CLEAR_RETURN + '    pr = 1.0;\n')], 'widen'),
        'search_without_slope': ([(LIFT, '    tap.z = stc.z;\n')], 'widen'),
        'no_slot_phase_in_metric_search_and_kernel': ([(SEARCH_PHI, 'softShadowPhi(search, SPOT_METRIC_SEARCH_PHASE)'),
                                                       ('        float slot_phase = g_spot_slot * SOFT_NOISE_SLOT_PHASE;\n', '        float slot_phase = 0.0;\n')], 'noise'),
        'metric_fill_step_back': ([(METRIC_FILL, '    if (soft_shadow_fill > 0.0 && shadow > 0.0)\n')], 'noise'),
        # review round 1
        'no_slope_clamp_on_the_radius': ([(SLOPE_CLAMP, '')], 'retention'),
        'slope_bias_may_exceed_the_gap': ([('const float SPOT_METRIC_SLOPE_SHARE  = 0.25;', 'const float SPOT_METRIC_SLOPE_SHARE  = 4.0;')], 'retention'),
        'slope_bias_half_the_gap': ([('const float SPOT_METRIC_SLOPE_SHARE  = 0.25;', 'const float SPOT_METRIC_SLOPE_SHARE  = 0.5;')], 'retention'),
        'no_near_taps': ([(NEAR_TEST, '< -1.0) { tap = t; how = 0.875; }')], 'small'),
        'no_near_taps_lit_return_kept': ([(NEAR_TEST, '< -1.0) { tap = t; how = 0.875; }')], 'clear'),
        'near_taps_need_half_a_quad': ([(NEAR_TEST, '< 0.5) { tap = t; how = 0.875; }')], 'clear'),
        'near_block_of_vogel_for_poisson': ([(NEAR_COLUMNS, '    vec3 column = vec3(0.0, 2.0, 2.0) * inv_res.x;\n')], 'clear'),
        'near_block_shifted_one_texel': ([(NEAR_BLOCK, NEAR_BLOCK.replace('- 0.5)', '+ 0.5)'))], 'clear'),
        'bisection_of_twelve_steps': ([('const int   SPOT_METRIC_BISECT_STEPS = 5;', 'const int   SPOT_METRIC_BISECT_STEPS = 12;')], 'cost'),
        'kernel_always_run': ([(CLEAR_RETURN, '')], 'cost'),
        'second_search_ring': ([(SEARCH_FETCH, '    return min(textureLod(shadowMap, tap, 0.0), textureLod(shadowMap, vec3(stc.xy + 0.5 * (tap.xy - stc.xy), tap.z), 0.0));\n')], 'cost'),
        'airborne_enters_metric': ([(B.GATE, B.GATE.replace(' && dot(norm,norm) > 0.0', ''))], 'identity'),
        'rim_enters_metric': ([(B.GATE, B.GATE.replace(' && projvol_rim_strength <= 0.0', ''))], 'identity'),
        'legacy_kernel_reads_metric': ([(LEGACY_KERNEL, LEGACY_KERNEL + '        if (spot_shadow_metric[0].x > 0.0) texel *= 2.0;\n')], 'identity'),
        'off_is_not_the_old_statements': ([(OLD_GAP, OLD_GAP.replace('softness), 0.0, 1.0)', 'softness) * 1.01, 0.0, 1.0)'))], 'identity'),
    }


def upload_negatives():
    """Wrong k at the upload (the fixture's copy of the viewer's numbers): name -> transform of the vec4."""
    return {
        'k_from_full_fov': lambda m, e, res: (m[0] * math.tan(e['fov'] * .5) / math.tan(e['fov']), m[1] * math.tan(e['fov'] * .5) / math.tan(e['fov']), m[2], m[3]),
        'k_without_depth_constant': lambda m, e, res: (m[0] * .25, m[1] * .25, m[2], m[3]),
        'k_ignores_resolution': lambda m, e, res: (m[0] * 512. / res, m[1] * 512. / res, m[2], m[3]),
    }


def header_negatives():
    """Mutations of alrelightmath.h: (before, after)."""
    return {
        'header_k_without_the_half': ('radius_m * map_texels * focal / (2.0 * depth_k)', 'radius_m * map_texels * focal / depth_k'),
        'header_depth_k_sign': ('constexpr double spotDepthK(double p32) { return -0.5 * p32; }', 'constexpr double spotDepthK(double p32) { return 0.5 * p32; }'),
        'header_setback_dropped': ('    return depth_k > 1.e-9 ? centre_depth / depth_k : 0.0;', '    return 0.0;'),
        'header_default_radius_not_shared': ('constexpr double kShadowSourceDefaultRadius = 0.1;', 'constexpr double kShadowSourceDefaultRadius = 0.05;'),
        'header_fixture_softness_double_counted': ('    if (fixture_sized) return 1.0;\n', ''),
    }


def pipeline_negatives():
    return {
        'pipeline_cube_default_not_shared': ('llmin(source > 0.f ? source : .1f,2.f),projectorDetailShadows(id)', 'llmin(source > 0.f ? source : .2f,2.f),projectorDetailShadows(id)'),
        'pipeline_cube_cap_not_shared': ('llmin(source > 0.f ? source : .1f,2.f),projectorDetailShadows(id)', 'llmin(source > 0.f ? source : .1f,1.f),projectorDetailShadows(id)'),
        'pipeline_cube_radius_other_source': ('                    const F32 source = area_source_radius(volume);\n                    world_flags.mV[i] = 1.f;',
                                              '                    const F32 source = volume->getLightRadius() * .05f;\n                    world_flags.mV[i] = 1.f;'),
        'pipeline_projector_radius_not_shared': (UPLOAD_RADIUS, UPLOAD_RADIUS.replace('ALRelight::shadowSourceRadius(area_source_radius(volume))', '0.1')),
        'pipeline_setting_default_off': ('"RenderSpotShadowPenumbraModel", 1);', '"RenderSpotShadowPenumbraModel", 0);'),
    }


# ---------------------------------------------------------------- driver
def evaluate(out, do_negatives=True):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    defaults = settings_defaults()
    r = R.Runner('WORKTREE')
    print('[spot-metric] GPU', r.gl.renderer, flush=True)
    host = HeaderHost(out, r.tree.read(HEADER))
    failures, result = [], {}

    contracts = dict(source_contracts(r.tree), **{'layer_' + k: v for k, v in layer_contracts(r.tree).items()})
    trims = trim_check(host)
    failures += ['source contract ' + k for k, ok in contracts.items() if not ok]
    failures += ['trim ' + k for k, ok in trims.items() if not ok]
    result.update(contracts=contracts, trim=trims)

    global FADE
    base = B.link(r, G.SourceTree(BASE), 'BASE visibility')
    pr = r.visibility
    sections, identity = {}, {}
    # Every check under both projector fade models (RenderSpotShadowFadeModel 0 legacy, 1 map-bounded = the default).
    for FADE in (0, 1):
        for name, fn in (('formula', lambda: formula_check(r, pr, host, defaults)),
                         ('sphere', lambda: sphere_check(r, pr, host, defaults)),
                         ('hardening', lambda: hardening_check(r, pr, host, defaults)),
                         ('resolution', lambda: resolution_check(r, pr, host, defaults)),
                         ('cap', lambda: cap_check(r, pr, host, defaults)),
                         ('aspect', lambda: aspect_check(r, pr, host, defaults)),
                         ('widen', lambda: widen_check(r, host, defaults)),
                         ('retention', lambda: retention_check(r, host, defaults)),
                         ('small', lambda: small_blocker_check(r, host, defaults)),
                         ('clear', lambda: clear_check(r, host, defaults)),
                         ('cost', lambda: cost_check(r, host, defaults)),
                         ('timing', lambda: timing_check(r, host, defaults)),
                         ('noise', lambda: noise_check(r, host, defaults)),
                         ('shaft', lambda: shaft_check(r))):
            if name == 'timing' and FADE == 0:
                continue   # the search does not read the fade model: timed once, under the default
            rows, f = fn()
            rows = [dict(row, fade_model=FADE) for row in rows]
            sections.setdefault(name, []).extend(rows)
            failures += ['fade model %d: %s' % (FADE, x) for x in f]
            for row in rows:
                print('[spot-metric]', name, json.dumps(row, default=str), flush=True)
        checks = identity_check(r, base, host, defaults)
        failures += ['identity fade model %d %s' % (FADE, k) for k, ok in checks.items() if not ok]
        identity.update({'fade%d_%s' % (FADE, k): ok for k, ok in checks.items()})
    result.update(sections, identity=identity)   # the planted regressions below run under the default fade model (1)

    if do_negatives:
        rejected = {}

        def note(name, caught):
            rejected[name] = bool(caught)
            print('[spot-metric]', 'PASS rejected' if caught else 'FAIL survived', 'planted regression:', name, flush=True)
            if not caught:
                failures.append('planted regression survived: ' + name)

        for name, (edits, target) in shader_negatives().items():
            try:
                mutant = B.link(r, Mut(edits), name)
                if target == 'formula':
                    caught = bool(formula_check(r, mutant, host, defaults, quick=True)[1])
                elif target == 'sphere':
                    caught = bool(sphere_check(r, mutant, host, defaults, cube=False)[1])
                elif target == 'hardening':
                    caught = bool(hardening_check(r, mutant, host, defaults)[1])
                elif target == 'aspect':
                    caught = bool(aspect_check(r, mutant, host, defaults)[1])
                elif target == 'widen':
                    caught = bool(widen_check(r, host, defaults, edits)[1])
                elif target == 'noise':
                    caught = bool(noise_check(r, host, defaults, edits)[1])
                elif target in ('retention', 'small', 'clear', 'cost'):
                    caught = bool(dict(retention=retention_check, small=small_blocker_check, clear=clear_check, cost=cost_check)[target](r, host, defaults, edits)[1])
                else:
                    saved = r.visibility
                    r.visibility = mutant
                    try:
                        caught = not all(identity_check(r, base, host, defaults).values())
                    finally:
                        r.visibility = saved
            except AssertionError as ex:
                caught = True   # a mutant that no longer links / runs cannot pass
                print('[spot-metric] mutant', name, 'raised', str(ex)[:120], flush=True)
            note(name, caught)
        for name, transform in upload_negatives().items():
            note(name, bool(formula_check(r, pr, host, defaults, upload=transform, quick=True)[1]))
        header = r.tree.read(HEADER)
        for name, (before, after) in header_negatives().items():
            assert header.count(before) == 1, ('mutation drift', name)
            mutated = header.replace(before, after)
            try:
                bad = HeaderHost(out, mutated)   # the header's own static_asserts reject most of these at compile time
                caught = (not all(source_contracts(r.tree, header_text=mutated).values()) or not all(trim_check(bad).values()) or
                          bool(formula_check(r, pr, bad, defaults, quick=True)[1]))
            except AssertionError as ex:
                caught = True
                print('[spot-metric] header mutant', name, 'does not compile:', str(ex).strip().splitlines()[-1][:160] if str(ex).strip() else '', flush=True)
            note(name, caught)
        pipeline = r.tree.read(PIPELINE)
        for name, (before, after) in pipeline_negatives().items():
            assert pipeline.count(before) == 1, ('mutation drift', name)
            note(name, not all(source_contracts(r.tree, pipeline_text=pipeline.replace(before, after)).values()))
        result['planted_negatives'] = rejected
    result.update(passed=not failures, failures=failures, gpu=r.gl.renderer)
    (out / 'results.json').write_text(json.dumps(result, indent=2, default=str) + '\n', encoding='utf-8')
    print('[spot-metric]', 'PASS' if result['passed'] else 'FAIL', failures, flush=True)
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default='.tmp/relight_spot_metric')
    ap.add_argument('--no-negatives', action='store_true', help='skip the planted regressions (faster iteration)')
    args = ap.parse_args()
    raise SystemExit(0 if evaluate(G.REPO / args.out, not args.no_negatives)['passed'] else 1)
