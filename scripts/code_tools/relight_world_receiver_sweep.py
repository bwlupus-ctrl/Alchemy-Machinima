"""Seeded production-GL receiver geometry sweep, using independent intersections.

Depth is D24 at the angular footprint of a 2160-line / 60-degree camera.
The small image is a crop of that camera, not a lower-resolution substitute.
No CPU visibility implementation is used as the shader under test.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import relight_world_gl_check as W
import relight_world_receiver_check as R
import relight_world_stencil_check as K
import relight_world_receiver_support as U


SEED = 20261025
PLAN = dict(samples=8, steps=12, half=True, near_steps=0)


def configuration(rng, index):
    distance = float(np.exp(rng.uniform(np.log(.3), np.log(600.))))
    near = float(rng.uniform(.1, min(1., distance*.4)))
    far = float(rng.uniform(max(64., distance*1.25), 1024.))
    return dict(index=index, distance=distance, near=near, far=far,
                slope=float(rng.uniform(0., 70.)), curvature=float(np.exp(rng.uniform(np.log(.06), np.log(4.)))),
                kind=('sphere', 'plane', 'thin')[index % 3], gap=float(rng.uniform(.001, .02)),
                width_px=int(rng.choice([1, 3, 7])), radius=float(rng.uniform(.1, 1.)),
                detail=bool(index & 1), sculpt=bool(index & 2), blocked=bool(index & 4),
                tilt=float(rng.choice([-50., -25., 25., 50.])), jitter=float(rng.uniform(-.45, .45)))


def fixture(cfg):
    angle = np.radians(cfg['slope'])
    n = np.array([np.sin(angle), -np.cos(angle), 0.])
    light = S.Light('receiver sweep', tuple(n), cfg['radius'], 16., seed=123)
    receiver = (S.Prim('curved skin receiver', 'sphere', c=-cfg['curvature']*n, r=cfg['curvature'])
                if cfg['kind'] == 'sphere' else K.slab(n))
    # A contact cover is independently verified over the entire shader support,
    # not only one centre ray. This includes the source disk for geometric truth.
    cover = K.slab(n)
    cover.name = 'confirmed foreground slab'
    cover.c = .15*n
    cover.h = (2., 2., .01)
    if cfg.get('blocker_kind') == 'sphere':
        cover = S.Prim('confirmed blocker sphere', 'sphere', c=.15*n, r=.14)
    behind = S.Prim('behind receiver', 'sphere', c=-.25*n, r=.08)
    prims = [receiver, cover] if cfg['blocked'] else [receiver, behind]
    return n, light, R.scene(prims, light), cover


def primary(cfg, distance, n, light):
    cam = K.camera(distance, cfg['near'], cfg['far'], jitter=cfg.get('jitter', 0.))
    if cfg['kind'] == 'sphere':
        receiver = S.Prim('curved skin receiver', 'sphere', c=-cfg['curvature']*n, r=cfg['curvature'])
        g = S.cast_primary(R.scene([receiver], light), cam)
        # Always track the same world point: projection jitter is represented in
        # the stencil/plane cases; these exact-centre sphere walks use zero jitter.
        if cfg.get('jitter', 0.):
            cam = K.camera(distance, cfg['near'], cfg['far'])
            g = S.cast_primary(R.scene([receiver], light), cam)
    else:
        width = cfg['width_px']*(2*cfg['distance']*np.tan(np.radians(30))/2160) if cfg['kind'] == 'thin' else None
        g, _, _ = K.primary(cam, n, width, cfg['gap'])
    # Material normal perturbations point both toward and away from the light.
    shade = g['nrm'].copy()
    across = np.array([n[1], -n[0], 0.])
    tilt = np.radians(cfg['tilt'])
    g['nrm'] = shade*np.cos(tilt)+across*np.sin(tilt)
    g['nrm'] /= np.maximum(np.linalg.norm(g['nrm'], axis=2, keepdims=True), 1.e-20)
    buffers = S.encode_gbuffer(cam, g)[1:]
    return cam, buffers, g, shade


def resolve(r, cfg, distance, n, light):
    cam, buffers, g, shade = primary(cfg, distance, n, light)
    world = K.world(light, cfg['detail'])
    # Shadow Sculpt strength/contrast, with the same production look uniform.
    look = dict(strength=1., contrast=1.6, world_amount=1.) if cfg['sculpt'] else dict()
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 0)
    image = r.run(cam, *buffers, [light], PLAN, denoise=0., world=world, look=look)[0]
    return float(image[K.MID, K.MID]), cam, buffers, g, shade


def geometry_truth(cfg, n, light, sc, g, shade):
    p = g['pos'][K.MID, K.MID][None, :]
    normal = shade[K.MID, K.MID][None, :]
    assert np.isfinite(p).all()
    # Source rays are an independent exact intersection oracle. The surface's
    # own positive roots are excluded by a 0.1-mm geometric normal offset.
    truth = S.trace_light(sc, light, p+normal*.0001, normal, np.array([0]), S=128)['vis'][0]
    return float(truth)


def sweep(r, count, seed, walks=5):
    rng = np.random.default_rng(seed)
    configs = [configuration(rng, i) for i in range(count)]
    # Pin both reviewed camera/clip regimes in addition to the random domain.
    for distance in (40.4179, 40.4224, 512.):
        for blocked, detail, sculpt in ((b, d, s) for b in (False, True) for d in (False, True) for s in (False, True)):
            configs.append(dict(index=len(configs), distance=distance, near=.1,
                                    far=64. if distance < 64 else 1024., slope=0. if distance == 512 else 60.,
                                    curvature=.12, kind='plane' if distance == 512 else 'sphere', gap=.01,
                                    width_px=3, radius=.1, detail=detail, sculpt=sculpt, blocked=blocked, tilt=25., jitter=0.,
                                    blocker_kind='sphere' if distance == 512 else 'slab', walk_steps=21))
    rows = []
    raster = W.Raster(r)
    for cfg in configs:
        n, light, sc, _ = fixture(cfg)
        r.gpu_atlas = raster.atlas(sc, light, .02)
        values, truths = [], []
        # Fine dolly steps leave geometry and lighting unchanged. Also compare
        # a second camera 1% farther away, under the existing 2/255 bound.
        steps = cfg.get('walk_steps', walks)
        distances = [cfg['distance']+k*.00025 for k in range(steps)] + [cfg['distance']*1.01]
        for distance in distances:
            v, cam, buffers, g, shade = resolve(r, cfg, distance, n, light)
            values.append(v)
            truths.append(geometry_truth(cfg, n, light, sc, g, shade))
        truth_ok = all(t <= .00001 if cfg['blocked'] else t >= .99999 for t in truths)
        lit_ok = cfg['blocked'] or min(values) >= .98
        blocked_ok = not cfg['blocked'] or max(values) <= .02
        continuity = float(np.max(np.abs(np.diff(values[:steps]))))
        invariance = float(np.ptp(values))
        row = dict(**cfg, values=values, truth=truths, truth_passed=truth_ok,
                   lit_passed=bool(lit_ok), blocked_passed=bool(blocked_ok),
                   continuity=continuity, invariance=invariance,
                   continuity_passed=continuity <= 2/255, invariance_passed=invariance <= 2/255)
        row['passed'] = all(row[k] for k in ('truth_passed', 'lit_passed', 'blocked_passed', 'continuity_passed', 'invariance_passed'))
        rows.append(row)
        if len(rows) % 16 == 0:
            print('[receiver-sweep] configs', len(rows), '/', len(configs), 'failures', sum(not x['passed'] for x in rows), flush=True)
    return rows


RADIUS_LINE = 'float radius = clamp(uncertainty/max(0.005*footprint,1.e-8)-16.0,1.0,128.0);'


def _radius_continuity(r, curvature):
    """Hold D24 inputs fixed while walking only the actual stencil support.

    This isolates the geometry function's continuity from D24 input rounding.
    It exercises the production weighting code, not a Python copy of it.
    """
    target = 'area_n_geo = awGeoNormal(texel);'
    class DiagnosticTree(G.SourceTree):
        def read(self, path):
            text = r.tree.read(path)
            if str(path).endswith('areaShadowFilterF.glsl'):
                assert RADIUS_LINE in text and target in text
                text = text.replace('uniform mat4 inv_proj;', 'uniform mat4 inv_proj;\nuniform float receiver_sweep_radius;')
                text = text.replace(RADIUS_LINE, 'float radius = receiver_sweep_radius;')
                text = text.replace(target, target+' frag_color=vec4(area_n_geo,aw_plane_valid); return;')
            return text
    nr = R.NormalRunner(DiagnosticTree(r.tree.ref), gl=r.gl)
    cfg = dict(distance=40.42, near=.1, far=64., slope=35., curvature=curvature, kind='sphere',
               gap=.01, width_px=3, radius=.1, detail=False, sculpt=False, blocked=False, tilt=25., jitter=0.)
    n, light, sc, _ = fixture(cfg)
    nr.gpu_atlas = W.Raster(nr).atlas(sc, light, .02)
    cam, buffers, _, _ = primary(cfg, cfg['distance'], n, light)
    rows = []
    for boundary in (2., 4., 8.):
        values = []
        for radius in np.linspace(boundary-.01, boundary+.01, 21):
            nr.gl.use(nr.filter)
            nr.gl.uf[1](nr.gl.loc(nr.filter, b'receiver_sweep_radius'), radius)
            K.draw(nr, cam, buffers, light, False)
            value = np.empty(4, np.float32)
            nr.gl.read(K.MID, K.MID, 1, 1, 0x1908, 0x1406, value.ctypes.data)
            values.append(value.astype(float))
        values = np.array(values)
        a = values[:, :3]/np.linalg.norm(values[:, :3], axis=1, keepdims=True)
        delta = float(np.max(np.degrees(np.arctan2(np.linalg.norm(np.cross(a[:-1], a[1:]), axis=1),
                                                 np.sum(a[:-1]*a[1:], axis=1)))))
        validity = float(np.max(np.abs(np.diff(values[:, 3]))))
        rows.append(dict(curvature=curvature, boundary=boundary, normal_step_deg=delta, validity_step=validity,
                         passed=delta <= .001 and validity <= .005))
    return rows


def radius_continuity(r):
    # Preserve the original curved receiver and also cover coherent wider
    # surfaces: improved silhouette rejection can correctly discard every
    # coarse support on the smallest sphere, masking a discrete-weight mutant.
    return [row for curvature in (.24, .5, 1., 4.) for row in _radius_continuity(r, curvature)]


def offset_bounds(r):
    target = 'area_n_geo = awGeoNormal(texel);'
    output = target+''' frag_color=vec4(awReceiverOffset(1.0,false),
        awReceiverOffset(0.001,false),awReceiverOffset(1.0,true),aw_depth_error);return;'''
    class DiagnosticTree(G.SourceTree):
        def read(self, path):
            text = r.tree.read(path)
            if str(path).endswith('areaShadowFilterF.glsl'):
                assert target in text
                text = text.replace(target, output)
            return text
    nr = R.NormalRunner(DiagnosticTree(r.tree.ref), gl=r.gl)
    n = np.array([0., -1., 0.])
    light = S.Light('offset bound', tuple(n), .1, 16., seed=123)
    cam = K.camera(512., .1, 1024.)
    _, buffers, _ = K.primary(cam, n)
    nr.gpu_atlas = W.Raster(nr).atlas(R.scene([K.slab(n)], light), light, .02)
    K.draw(nr, cam, buffers, light, False)
    values = np.empty((K.SIZE, K.SIZE, 4), np.float32)
    nr.gl.read(0, 0, K.SIZE, K.SIZE, 0x1908, 0x1406, values.ctypes.data)
    maximum = float(values[:, :, :3].max())
    return dict(samples=K.SIZE*K.SIZE*3, maximum_displacement_m=maximum,
                precision_interval_m=float(values[:, :, 3].max()),
                passed=bool(np.isfinite(values).all() and values[:, :, :3].min() >= 0. and
                            maximum <= float(np.float32(.05)) and np.all(values[:, :, 2] == 0.)))


def mutation_controls(r):
    controls = {}
    mutant = R.Runner(W.MutantTree(r.tree, [
        ('float weight = smoothstep(float(r/2),float(r),radius);',
         'float weight = float(radius > float(r/2));')]), gl=r.gl)
    rows = radius_continuity(mutant)
    controls['discrete_radius'] = dict(predicate='stencil geometry continuity', rejected=any(not x['passed'] for x in rows), rows=rows)
    old = 'return detail ? 0.0 : aw_plane_valid*min(AW_NORMAL_TEXELS*texel_w,AW_NORMAL_CAP);'
    mutant = R.Runner(W.MutantTree(r.tree, [(old, 'return max(2.0*aw_depth_error,detail ? 0.0 : aw_plane_valid*min(AW_NORMAL_TEXELS*texel_w,AW_NORMAL_CAP));')]), gl=r.gl)
    cfg = dict(index=0, distance=512., near=.1, far=1024., slope=0., curvature=.12, kind='plane', gap=.01,
               width_px=3, radius=.1, detail=False, sculpt=False, blocked=True, tilt=25., jitter=0., blocker_kind='sphere')
    n, light, sc, _ = fixture(cfg)
    mutant.gpu_atlas = W.Raster(mutant).atlas(sc, light, .02)
    value, _, _, g, shade = resolve(mutant, cfg, cfg['distance'], n, light)
    truth = geometry_truth(cfg, n, light, sc, g, shade)
    bounds = offset_bounds(mutant)
    controls['unbounded_compensation'] = dict(predicate='confirmed sphere blocker retention and displacement cap', visibility=value, truth=truth,
                                             displacement=bounds, rejected=truth == 0. and not bounds['passed'])
    mutant = R.Runner(W.MutantTree(r.tree, [
        ('dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo)',
         'dot(area_world_rot[0],area_n_shade),dot(area_world_rot[1],area_n_shade),dot(area_world_rot[2],area_n_shade)')]), gl=r.gl)
    rows = R.material_checks(mutant, W.Raster(mutant))
    failed = [x for x in rows if x['name'] == 'empty_grazing_floor' and x['minimum'] < .98]
    controls['material_normal'] = dict(predicate='unblocked grazing receiver stays lit', rejected=bool(failed), failures=failed[:4])
    mutant = R.Runner(W.MutantTree(r.tree, [
        ('return vec4(lessThan((z+bias)*max(vec4(den+du,den+dr+du,den+dr,den),vec4(1.e-5)),vec4(receiver2)));',
         'return vec4(1.0);')]), gl=r.gl)
    rows = R.depth_checks(mutant, W.Raster(mutant))
    failed = [x for x in rows if x.get('placement') == 'behind' and not x['passed']]
    controls['no_forward_depth_bound'] = dict(predicate='behind-only geometry never shadows receiver', rejected=bool(failed), failures=failed[:4])
    return controls


def frame_truth_checks(r):
    """Current complete world output on curved skin and avatar-like limbs.

    All visible receiver pixels enter the oracle; a candidate cannot choose its
    protected interiors. Fully covered/open scenes use the sweep's hard bounds.
    A separate partial cover gates the actual penumbra against disk-ray truth.
    """
    rows = []
    cam = S.Camera((0., -.5, 0.), (0., 0., 0.), 160, 120, vfov=40., near=.02, far=16.)
    for kind in ('face', 'limb'):
        receiver = (S.Prim('avatar-like face', 'sphere', c=(0., .12, 0.), r=.12) if kind == 'face' else
                    S.Prim('avatar-like limb', 'capsule', a=(0., .12, -.15), b=(0., .12, .15), r=.12))
        light = S.Light('frame truth source', (0., -1., 0.), .1, 16., seed=123)
        primary_scene = R.scene([receiver], light)
        g = S.cast_primary(primary_scene, cam)
        mask = (g['surf'] == 0)
        ray = np.asarray(light.pos)-g['pos']
        nl = (ray*g['nrm']).sum(2)/np.maximum(np.linalg.norm(ray, axis=2), 1.e-9)
        mask &= nl > .2
        _, depth, normals = S.encode_gbuffer(cam, g)
        for placement in ('unblocked', 'blocked', 'penumbra'):
            cover = K.slab(np.array([0., -1., 0.]))
            cover.c = (0., -.15, 0.)
            cover.h = (2., 2., .01)
            if placement == 'penumbra':
                cover = S.Prim('partial foreground cover', 'sphere', c=(.04, -.4, 0.), r=.045)
            sc = R.scene([receiver]+([] if placement == 'unblocked' else [cover]), light)
            r.gpu_atlas = W.Raster(r).atlas(sc, light, .02)
            points, geometric = g['pos'][mask], g['nrm'][mask]
            truth = S.trace_light(sc, light, points, geometric, np.flatnonzero(mask), S=256)['vis']
            for detail in (False, True):
                r.gl.use(r.filter)
                r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 1)
                value = r.run(cam, depth, normals, [light], PLAN, denoise=0., world=K.world(light, detail), look=dict())[0][mask]
                mid = (truth > .1) & (truth < .9)
                error = float(np.mean(np.abs(value-truth)))
                if placement == 'unblocked':
                    ok = bool(truth.min() >= .99999 and value.min() >= .98)
                elif placement == 'blocked':
                    ok = bool(truth.max() <= .00001 and value.max() <= .02)
                else:
                    ok = bool(mid.sum() >= 100 and error <= .2 and np.any((value[mid] > .02) & (value[mid] < .98)))
                rows.append(dict(kind=kind, placement=placement, detail=detail, pixels=int(mask.sum()),
                                 penumbra_pixels=int(mid.sum()), mean_absolute_error=error,
                                 minimum=float(value.min()), maximum=float(value.max()), passed=ok))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_seethru/FIX5/sweep')
    ap.add_argument('--seed', type=int, default=SEED)
    ap.add_argument('--count', type=int, default=256)
    ap.add_argument('--controls', action='store_true')
    args = ap.parse_args()
    r = R.Runner(G.SourceTree(args.shader_ref))
    exact = U.verify_shortcuts_on_every_draw(r)
    rows = sweep(r, args.count, args.seed)
    result = dict(seed=args.seed, configurations=len(rows), camera_samples=sum(len(row['values']) for row in rows), rows=rows,
                  shader_ref=args.shader_ref, renderer=r.gl.renderer,
                  shader_sha256=hashlib.sha256(r.tree.read(G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl').encode()).hexdigest(),
                  passed=all(row['passed'] for row in rows))
    result['offset_bounds'] = offset_bounds(r)
    result['passed'] &= result['offset_bounds']['passed']
    result['geometry_frames'] = frame_truth_checks(r)
    result['passed'] &= all(x['passed'] for x in result['geometry_frames'])
    result['support_repros'] = U.repros(r)
    result['independent_directions'] = [row for bank in (0, 1) for row in
        U.independent_sweep(r, max(128, args.count//2), args.seed+1, mode_bank=bank)]
    result['penumbra_walks'] = U.penumbra_walks(r)
    result['shortcut_equivalence'] = U.shortcut_equivalence(r)
    result['authority_contract'] = U.authority_contract(r)
    result['passed'] &= all(x['passed'] for key in ('support_repros', 'independent_directions', 'penumbra_walks', 'shortcut_equivalence', 'authority_contract') for x in result[key])
    result['original_configurations'] = len(rows)
    result['configurations'] += len(result['independent_directions'])
    result['camera_sample_groups'] = dict(original=result['camera_samples'],
        independent_directions=5*len(result['independent_directions']),
        support_repros=sum(len(x['values']) for x in result['support_repros']),
        penumbrae=sum(len(x['values']) for x in result['penumbra_walks']),
        authority=sum(len(x['values']) for x in result['authority_contract']))
    result['camera_samples'] = sum(result['camera_sample_groups'].values())
    result['exact_shortcuts'] = dict(exact, passed=exact['different_pixels'] == 0)
    result['passed'] &= result['exact_shortcuts']['passed']
    if RADIUS_LINE in r.tree.read(G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl'):
        result['radius_continuity'] = radius_continuity(r)
        result['passed'] &= all(x['passed'] for x in result['radius_continuity'])
    if args.controls:
        result['controls'] = mutation_controls(r)
        result['controls'].update(U.mutation_controls(r))
        result['passed'] &= all(x['rejected'] for x in result['controls'].values())
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    failures = [row for row in rows if not row['passed']]
    print('[receiver-sweep] failures', len(failures), 'examples', failures[:8], flush=True)
    print('[receiver-sweep] PASS' if result['passed'] else '[receiver-sweep] FAIL', flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
