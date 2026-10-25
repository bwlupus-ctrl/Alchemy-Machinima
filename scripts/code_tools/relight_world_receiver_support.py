"""Independent light/support regressions used by the seeded receiver sweep.

The shadow oracle intersects actual cards, boxes and spheres. Receivers can
be receive-only, as in the existing foreground tests; oracle casters then
match exactly the primitives rasterized into the production shadow atlas.
"""
import argparse
import ctypes as C
import json
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import relight_world_gl_check as W
import relight_world_receiver_check as R
import relight_world_stencil_check as K

PLAN = dict(samples=8, steps=12, half=True, near_steps=0)


def truth(casters, light, points):
    """Double precision independent disk rays, without a millimetre offset."""
    points = np.asarray(points, dtype=np.float64).reshape(-1, 3)
    centre = np.asarray(light.pos)
    axis = centre-points
    axis /= np.linalg.norm(axis, axis=1, keepdims=True)
    helper = np.broadcast_to([0., 0., 1.], axis.shape).copy()
    helper[abs(axis[:, 2]) > .99] = [0., 1., 0.]
    across = np.cross(axis, helper)
    across /= np.linalg.norm(across, axis=1, keepdims=True)
    up = np.cross(axis, across)
    k = np.arange(256)
    radius = light.radius*np.sqrt((k+.5)/len(k))
    phase = k*2.399963229728653
    target = centre+(radius*np.cos(phase))[None, :, None]*across[:, None, :]+(radius*np.sin(phase))[None, :, None]*up[:, None, :]
    delta = target-points[:, None, :]
    length = np.linalg.norm(delta, axis=2)
    rays = (delta/length[:, :, None]).reshape(-1, 3)
    origins = np.broadcast_to(points[:, None, :], delta.shape).reshape(-1, 3)
    blocked = np.zeros(len(rays), bool)
    for caster in casters:
        blocked |= S.isect(caster, origins, rays, eps=1.e-7) < length.reshape(-1)-1.e-7
    return 1.-blocked.reshape(-1, 256).mean(1)


def plane_buffers(cam, n, width=None, gap=.01, tilt=25., toward=None):
    g, _, mask = K.primary(cam, n, width, gap)
    tangent = S.unit(np.cross(n, [0., 0., 1.])) if toward is None else toward
    g['nrm'] = np.broadcast_to(n*np.cos(np.radians(tilt))+tangent*np.sin(np.radians(tilt)), g['pos'].shape).copy()
    return g, S.encode_gbuffer(cam, g)[1:], mask


def draw(r, cam, buffers, light, detail, sculpt=False):
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 0)
    look = dict(strength=1., contrast=1.6, world_amount=1.) if sculpt else dict()
    return r.run(cam, *buffers, [light], PLAN, denoise=0., world=K.world(light, detail), look=look)[0]


def repros(r, names=None):
    rows = []
    raster = W.Raster(r)
    for name in ('far_slab', 'grazing_blocker', 'behind_strip', 'backlit_source_support'):
        if names is not None and name not in names:
            continue
        for radius in (.1, 1.):
            n = np.array([0., -1., 0.])
            direction = n.copy()
            if name == 'grazing_blocker':
                angle = np.radians(25.)
                n = np.array([np.sin(angle), -np.cos(angle), 0.])
                direction = np.array([-np.cos(angle), -np.sin(angle), 0.])
            if name == 'behind_strip':
                direction = np.array([np.sqrt(.91), .3, 0.])
            if name == 'backlit_source_support':
                direction = np.array([np.sqrt(1.-.025**2), .025, 0.])
            light = S.Light(name, tuple(direction*(16. if name == 'behind_strip' else 1.)), radius, 32., seed=123)
            if name == 'far_slab':
                cover = K.slab(direction)
                cover.c = .16*direction
                cover.h = (2., 2., .01)  # nearest face is 15 cm toward the source
                casters = [K.slab(n), cover]
                distances = [512.+i*.00025 for i in range(9)]
            elif name == 'grazing_blocker':
                cover = S.Prim('confirmed grazing foreground', 'sphere', c=.4*direction, r=.30)
                casters = [cover]
                distances = [2.00050+i*.00025 for i in range(9)]
            elif name == 'backlit_source_support':
                cover = K.slab(direction)
                cover.c = .15*direction
                cover.h = (2., 2., .01)
                casters = [cover]
                distances = [2.+i*.00025 for i in range(9)]
            else:
                cover = K.slab(direction)
                cover.c = -.018*direction
                cover.h = (2., 2., .01)  # every point is at least 8 mm behind
                casters = [cover]
                distances = [2.+i*.00025 for i in range(9)]
            r.gpu_atlas = raster.atlas(R.scene(casters, light), light, .02)
            for detail in (False, True):
                values, truths = [], []
                for distance in distances:
                    cam = K.camera(distance, .1, 1024. if name == 'far_slab' else 64.)
                    width = .6*(4*np.tan(np.radians(30))/2160) if name == 'behind_strip' else None
                    toward = (S.unit(direction-n*np.dot(n,direction)) if name in
                              ('grazing_blocker', 'backlit_source_support') else None)
                    g, buffers, _ = plane_buffers(cam, n, width, .0006625, toward=toward)
                    values.append(float(draw(r, cam, buffers, light, detail)[K.MID, K.MID]))
                    # Only foreground cover is a caster for the independent
                    # contact oracle; the primary receiver is not an occluder.
                    truths.append(float(truth([cover], light, g['pos'][K.MID, K.MID])[0]))
                blocked = name != 'behind_strip'
                oracle_ok = max(truths) == 0. if blocked else min(truths) == 1.
                bound_ok = max(values) <= .02 if blocked else min(values) >= .98
                continuity = float(np.max(np.abs(np.diff(values))))
                rows.append(dict(name=name, radius=radius, detail=detail, distances=distances,
                                 values=values, truth=truths, continuity=continuity,
                                 passed=bool(oracle_ok and bound_ok and continuity <= 2/255)))
    return rows


def receivers(kind, n, distance, width_px, gap):
    """Actual ray-cast and rasterized thin cards and stepped solid geometry."""
    width = width_px*(2*distance*np.tan(np.radians(30))/2160)
    if kind == 'card':
        card = K.slab(n)
        card.name = 'two-sided foliage/hair card'
        card.c = -.00001*n
        card.h = (width/2, 2., .00001)
        card.cull = 'none'
        background = K.slab(n)
        background.c = -(gap+.02)*n
        return [card, background]
    if kind == 'step':
        front = S.Prim('front step', 'obb', c=(-.5*width, .1, 0.), h=(.75*width, .1, 2.))
        back = S.Prim('recessed step', 'obb', c=(.75*width, gap+.1, 0.), h=(.5*width, .1, 2.))
        return [front, back]
    return [K.slab(n)]


def authority_contract(r):
    """Hold a partially valid geometry result fixed to isolate the bound.

    Stencil improvements can otherwise mask a reintroduced offset bug by
    assigning zero validity (and therefore zero offset) to the reviewed strip.
    This tests the forward-depth contract at every supported nonzero offset.
    """
    class FixedGeometry(G.SourceTree):
        def read(self, path):
            text = r.tree.read(path)
            if str(path).endswith('areaShadowFilterF.glsl'):
                target = 'area_n_geo = awGeoNormal(texel);'
                assert target in text
                text = text.replace(target, target+' area_n_geo=vec3(0,0,1); aw_plane_valid=0.966876;')
            return text
    fixed = R.Runner(FixedGeometry(r.tree.ref), gl=r.gl)
    rows = repros(fixed, ['behind_strip'])
    for row in rows:
        row['fixed_validity'] = .966876
    return rows


def independent_sweep(r, count=128, seed=20261026, mode_bank=0, indices=None):
    rng = np.random.default_rng(seed)
    rows = []
    raster = W.Raster(r)
    for index in range(count):
        # Light directions have their own random stream, independent of the
        # surface slope. Pin grazing/back-lit samples as well as oblique ones.
        kind = ('plane', 'thin', 'card', 'step')[index % 4]
        distance = float(np.exp(rng.uniform(np.log(.3), np.log(600.))))
        clip_near = float(rng.uniform(.1, min(1., distance*.4)))
        clip_far = max(64., distance*1.3)
        slope = float(rng.uniform(0., 65.)) if kind in ('plane', 'thin') else 0.
        n = np.array([np.sin(np.radians(slope)), -np.cos(np.radians(slope)), 0.])
        tangent = np.array([-n[1], n[0], 0.])
        nl = (0., -.3, float(rng.uniform(.15, 1.)))[(index//4) % 3]
        direction = nl*n+np.sqrt(1-nl*nl)*tangent
        light_distance = float(rng.uniform(1., 16.))
        radius = float(rng.uniform(.1, 1.))
        light = S.Light('independent direction', tuple(light_distance*direction), radius, 32., seed=123)
        blocked = bool((index//12) % 2)
        # Back-lit open surfaces test false shadows; confirmed-cover tests
        # include the lighting horizon and independent front-light directions.
        if blocked and nl < 0.:
            nl = 0.
            direction = tangent
            light.pos = tuple(light_distance*direction)
        detail, sculpt = ((bool(index & 1), bool(index & 2)) if mode_bank == 0 else
                          (bool((index//4) & 1), bool((index//8) & 1)))
        gap = float(rng.uniform(.0005, .02))
        width_px = float(rng.choice([.6, 1., 3., 7.]))
        if indices is not None and index not in indices:
            continue
        primary_prims = receivers(kind, n, distance, width_px, gap)
        cover = K.slab(direction)
        cover.c = (.15 if blocked else -.15)*direction
        cover.h = (4., 4., .01)
        # Receive-only analytic planes isolate back lighting from opaque self
        # occlusion. Real cards/steps with front light also enter the cube.
        casters = [cover]+(primary_prims if nl >= .15 and kind in ('card', 'step') else [])
        r.gpu_atlas = raster.atlas(R.scene(casters, light), light, .02)
        values, oracles = [], []
        for delta in (0., .00025, .00050, .00075, .001):
            cam = K.camera(distance+delta, clip_near, clip_far)
            if kind in ('card', 'step'):
                g = S.cast_primary(R.scene(primary_prims, light), cam)
                mask = (g['surf'] == 0)
                buffers = S.encode_gbuffer(cam, g)[1:]
            else:
                width = width_px*(2*distance*np.tan(np.radians(30))/2160) if kind == 'thin' else None
                g, buffers, mask = plane_buffers(cam, n, width, gap)
            select = mask.copy()
            yy, xx = np.indices(mask.shape)
            select &= (abs(yy-K.MID) <= 2) & (abs(xx-K.MID) <= 2)
            # At long distances a five-pixel crop spans metres. Keep receiver
            # points on the specified side of the independent 15-cm cover.
            select &= abs(g['pos']@direction) < .05
            assert select.any(), (kind, distance, width_px)
            values.append(draw(r, cam, buffers, light, detail, sculpt)[select])
            oracles.append(truth(casters, light, g['pos'][select]))
        bounds = max(float(v.max()) for v in values) <= .02 if blocked else min(float(v.min()) for v in values) >= .98
        oracle_ok = all(np.all(t == (0. if blocked else 1.)) for t in oracles)
        continuity = max(float(np.max(np.abs(b-a))) for a, b in zip(values[:-1], values[1:]))
        rows.append(dict(index=index, mode_bank=mode_bank, kind=kind, distance=distance, near=clip_near, far=clip_far,
                         slope=slope, nl=nl, radius=radius, light_distance=light_distance, width_px=width_px,
                         gap=gap, blocked=blocked, detail=detail, sculpt=sculpt, pixels=len(values[0]),
                         minimum=min(float(v.min()) for v in values), maximum=max(float(v.max()) for v in values),
                         truth_min=min(float(t.min()) for t in oracles), truth_max=max(float(t.max()) for t in oracles),
                         continuity=continuity, passed=bool(bounds and oracle_ok and continuity <= 2/255)))
        if not rows[-1]['passed']:
            print('[receiver-support] failure', rows[-1], flush=True)
        if (index+1) % 16 == 0:
            print('[receiver-support] independent', index+1, '/', count, 'failures', sum(not row['passed'] for row in rows), flush=True)
    return rows


def penumbra_walks(r):
    rows = []
    n = np.array([0., -1., 0.])
    light = S.Light('penumbra camera walks', (0., -1., 0.), .1, 16., seed=123)
    receiver = K.slab(n)
    cover = S.Prim('partial foreground', 'sphere', c=(.04, -.4, 0.), r=.045)
    r.gpu_atlas = W.Raster(r).atlas(R.scene([receiver, cover], light), light, .02)
    # Track multiple fixed WORLD points in the penumbra, using a fresh camera
    # projection and D24 buffer at every distance. The oracle must be partial.
    for x in (-.025, 0., .025):
        point = np.array([x, 0., 0.])
        oracle = float(truth([cover], light, point)[0])
        if not .1 < oracle < .9:
            continue
        for detail in (False, True):
            for sculpt in (False, True):
                values = []
                distances = np.r_[np.arange(21)*.00025+2., 2.02, 4.]
                for distance in distances:
                    cam = K.camera(float(distance), .1, 64., shift=x)
                    _, buffers, _ = plane_buffers(cam, n)
                    values.append(float(draw(r, cam, buffers, light, detail, sculpt)[K.MID, K.MID]))
                expected = oracle**(1.6 if sculpt else 1.)
                continuity = float(np.max(np.abs(np.diff(values[:21]))))
                invariance = float(np.ptp(values))
                rows.append(dict(x=x, detail=detail, sculpt=sculpt, truth=oracle, values=values,
                                 continuity=continuity, invariance=invariance, expected=expected,
                                 mean_absolute_error=float(np.mean(np.abs(np.array(values)-expected))),
                                 passed=bool(continuity <= 2/255 and invariance <= 2/255)))
    assert len(rows) >= 8, 'At least two actual penumbra points, both detail/sculpt modes'
    for detail in (False, True):
        for sculpt in (False, True):
            group = [row for row in rows if row['detail'] == detail and row['sculpt'] == sculpt]
            valid = np.mean([row['mean_absolute_error'] for row in group]) <= .2 and any(
                any(.02 < value < .98 for value in row['values']) for row in group)
            for row in group:
                row['passed'] &= bool(valid)
    return rows


def mutation_controls(r):
    controls = {}
    mutations = [
        ('precision_ceiling', 'min(plane_error,AW_NORMAL_CAP)', 'plane_error', 'far_slab'),
        ('hard_backface', 'float backface = awBackfaceWeight(dot(normal,-d)/max(distance,1.e-9),distance,area_world_info[c].z);',
         'float backface = dot(normal,-d) <= 0.0 && aw_plane_valid >= 1.0 ? 0.0 : 1.0;', 'grazing_blocker'),
        ('offset_forward_bound', 'vec3 receiver = d;',
         'vec3 receiver = d+n_a*awReceiverOffset(texel_w,area_world_info[c].w > 0.5);', 'behind_strip'),
    ]
    for name, old, new, predicate in mutations:
        mutant = R.Runner(W.MutantTree(r.tree, [(old, new)]), gl=r.gl)
        rows = repros(mutant, [predicate])
        if name == 'offset_forward_bound':
            rows += authority_contract(mutant)
        controls[name] = dict(predicate=predicate, rejected=any(not row['passed'] for row in rows), rows=rows)
    mutant = R.Runner(W.MutantTree(r.tree, [
        ('sqrt(2.0*max(1.0-aw_plane_valid*aw_plane_valid,0.0))', '0.0')]), gl=r.gl)
    rows = independent_sweep(mutant, count=94, indices=[93])
    controls['partial_normal_uncertainty'] = dict(predicate='partially valid subpixel receivers retain their cover',
                                                  rejected=any(not row['passed'] for row in rows), rows=rows)
    mutant = R.Runner(W.MutantTree(r.tree, [
        ('float source = source_radius/sqrt(distance*distance+source_radius*source_radius);',
         'float source = 0.0;')]), gl=r.gl)
    rows = repros(mutant, ['backlit_source_support'])
    controls['source_support_band'] = dict(predicate='full evaluation over the source angular support',
                                          rejected=any(not row['passed'] for row in rows), rows=rows)
    mutant = R.Runner(W.MutantTree(r.tree, [
        ('return mix(1.0,visibility,backface);',
         'return clamp(mix(1.0,visibility,backface)+0.1*sin(10000.0*area_p.z),0.0,1.0);')]), gl=r.gl)
    rows = penumbra_walks(mutant)
    controls['penumbra_continuity'] = dict(predicate='camera walks at independently partial source coverage',
                                          rejected=any(row['continuity'] > 2/255 for row in rows), rows=rows)
    return controls


def full_compare_tree(tree):
    """Disable proven shortcuts while keeping every footprint and compare."""
    text = tree.read(G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl')
    predicates = ['if (blend >= 1.0 && nd < 0.0)',
                  'if (blend >= 1.0 && nd >= 0.0)',
                  'if (nd < 0.0 && aw_plane_valid >= 1.0)']
    changes = [(predicate, 'if (false)') for predicate in predicates if predicate in text]
    zero_search = 'if (!(aw_plane_valid >= 1.0 && nd >= 0.0))'
    if zero_search in text:
        changes.append((zero_search, 'if (true)'))
    return W.MutantTree(tree, changes)


def verify_shortcuts_on_every_draw(r):
    """Pair complete sweep frames against the full path, including look/debug."""
    full = R.Runner(full_compare_tree(r.tree), gl=r.gl)
    get_uniform = r.gl.fn('glGetUniformiv', None, r.gl.U, r.gl.I, C.POINTER(r.gl.I))
    run = r.run
    stats = dict(frames=0, pixels=0, different_pixels=0)
    def checked(*args, **kwargs):
        debug = r.gl.I()
        get_uniform(r.filter, r.gl.loc(r.filter,b'area_world_debug'), C.byref(debug))
        a = run(*args, **kwargs)
        full.gpu_atlas = r.gpu_atlas
        full.gl.use(full.filter)
        full.gl.ui(full.gl.loc(full.filter,b'area_world_debug'), debug.value)
        b = full.run(*args, **kwargs)
        stats['frames'] += 1
        stats['pixels'] += int(a.size)
        stats['different_pixels'] += int(np.count_nonzero(a != b))
        return a
    r.run = checked
    return stats


def shortcut_equivalence(r):
    """Complete production frames with every proven shortcut disabled."""
    full = R.Runner(full_compare_tree(r.tree), gl=r.gl)
    n = np.array([0., -1., 0.])
    cam = K.camera(2., .1, 64.)
    _, buffers, _ = plane_buffers(cam, n)
    rows = []
    for radius in (.1, 1.):
        light = S.Light('proof equivalence', (-1., -1., 0.), radius, 16., seed=123)
        direction = S.unit(light.pos)
        for partial in (False, True):
            cover = K.slab(direction)
            cover.c = .15*direction
            cover.h = (2., 2., .01)
            if partial:
                cover = S.Prim('partial proof footprint', 'sphere', c=(-.15, -.3, 0.), r=.13)
            r.gpu_atlas = W.Raster(r).atlas(R.scene([K.slab(n), cover], light), light, .02)
            full.gpu_atlas = r.gpu_atlas
            for detail in (False, True):
                a = draw(r, cam, buffers, light, detail)
                b = draw(full, cam, buffers, light, detail)
                rows.append(dict(radius=radius, partial=partial, detail=detail,
                                 pixels=int(a.size), different_pixels=int(np.count_nonzero(a != b)),
                                 passed=bool(np.array_equal(a, b))))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_seethru/FIX6/support')
    ap.add_argument('--count', type=int, default=128)
    ap.add_argument('--repros-only', action='store_true')
    ap.add_argument('--controls', action='store_true')
    args = ap.parse_args()
    r = R.Runner(G.SourceTree(args.shader_ref))
    result = dict(shader_ref=args.shader_ref, repros=repros(r))
    if not args.repros_only:
        result.update(independent=[row for bank in (0, 1) for row in independent_sweep(r, args.count, mode_bank=bank)],
                      penumbrae=penumbra_walks(r), shortcut=shortcut_equivalence(r), authority=authority_contract(r))
    result['passed'] = all(row['passed'] for key in ('repros', 'independent', 'penumbrae', 'shortcut', 'authority') for row in result.get(key, []))
    if args.controls:
        result['controls'] = mutation_controls(r)
        result['passed'] &= all(row['rejected'] for row in result['controls'].values())
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    for key in ('repros', 'independent', 'penumbrae'):
        failures = [row for row in result.get(key, []) if not row['passed']]
        print('[receiver-support]', key, 'failures', len(failures), 'examples', failures[:4], flush=True)
    print('[receiver-support] passed', result['passed'], flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
