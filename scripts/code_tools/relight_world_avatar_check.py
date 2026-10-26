"""Real-GL avatar/rig-source shadow retention against eb1cb4a2b32.

Sphere and cylinder-barrel receivers have radii 0.15/0.5 m. A separate finite
caster partially covers sources of radius 0.1/0.5/1 m. Five fixed WORLD points
are measured at 3..60 m and during 0.25-degree yaw sweeps. The primary depth is
D24; both the receiver and caster enter the production rasterized cube atlas.
The cylinder barrel belongs to a capped capsule whose ends are outside the
tracked region; its primary intersections are exactly cylindrical there.

Mean darkness must match history within 20%, every frame must retain at least
80% of historical darkness. Adjacent-frame jumps may exceed the historical
maximum by at most 2/255, with a minimum allowance of 2/255. No existing gate
is changed. Round-three checks extend tracking to 128 m, add 0.25-degree
yaw/orbit walks at light incidence 0.01..0.1, and isolate receiver/caster
atlases with production debug=3 readbacks to attribute extra darkness.
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
import relight_world_receiver_support as U
import relight_world_stencil_check as K
import relight_world_motion_check as M


REFERENCE = 'eb1cb4a2b32'
DISTANCES = (3., 6., 12., 24., 36., 45., 60.)
YAW_DISTANCES = (30., 45., 60.)


def fixture(kind, radius, source, incidence=.5):
    normal = (np.array([0., 0., 1.]) if kind == 'floor' else
              np.array([np.sin(np.radians(60)), -np.cos(np.radians(60)), 0.]))
    across = np.array([1., 0., 0.]) if kind == 'floor' else np.array([-normal[1], normal[0], 0.])
    up = np.cross(normal, across)
    direction = incidence*normal+np.sqrt(1.-incidence**2)*across
    light = S.Light('avatar rig source', tuple(5*direction), source, 32., seed=123)
    if kind == 'floor':
        receiver = M.plane(normal)
        points = np.array([np.zeros(3), .003*across, -.003*across, .003*up, -.003*up])
    else:
        centre = -radius*normal
        receiver = (S.Prim('avatar sphere', 'sphere', c=centre, r=radius) if kind == 'sphere' else
                    S.Prim('avatar cylinder barrel', 'capsule', a=centre+[0., 0., -1.],
                           b=centre+[0., 0., 1.], r=radius))
        points = np.array([np.zeros(3),
            centre+radius*(normal*np.cos(.015)+across*np.sin(.015)),
            centre+radius*(normal*np.cos(.015)-across*np.sin(.015)),
            centre+radius*(normal*np.cos(.015)+up*np.sin(.015)),
            centre+radius*(normal*np.cos(.015)-up*np.sin(.015))])
        if kind == 'cylinder':
            points[-2:] = [[0., 0., .003], [0., 0., -.003]]
    cover = S.Prim('separate foreground caster', 'sphere', c=.4*direction+.098*up, r=.1)
    return normal, light, receiver, cover, points


def primary(point, receiver, normal, distance, yaw, kind, motion='yaw'):
    a = np.radians(yaw)
    rotation = np.array([[np.cos(a), -np.sin(a), 0.],
                         [np.sin(a), np.cos(a), 0.], [0., 0., 1.]])
    eye = distance*(np.array([0., -.6, .8]) if kind == 'floor' else np.array([0., -1., 0.]))
    if motion == 'orbit':
        eye = rotation@eye
    forward = S.unit(-eye)
    if motion == 'yaw':
        forward = rotation@forward
    cam = S.Camera(eye, eye+forward, M.SIZE, M.SIZE, M.FOV, .1, 256.)
    view_point = cam.view@np.r_[point, 1.]
    cam.proj[0, 2] = view_point[0]/(-view_point[2]*cam.tan*cam.aspect)
    cam.proj[1, 2] = view_point[1]/(-view_point[2]*cam.tan)
    h = M.NDC@np.linalg.inv(cam.proj).T
    rays = (h[..., :3]/h[..., 3:])@cam.view[:3, :3]
    rays /= np.linalg.norm(rays, axis=2, keepdims=True)
    if kind == 'floor':
        length = -(eye@normal)/(rays@normal)
    else:
        length = S.isect(receiver, np.broadcast_to(eye, rays.shape).reshape(-1, 3),
                         rays.reshape(-1, 3), eps=1.e-7).reshape(M.SIZE, M.SIZE)
    hit = np.isfinite(length) & (length < 1.e29)
    positions = eye+np.where(hit, length, 0.)[..., None]*rays
    normals = (np.broadcast_to(normal, positions.shape).copy() if kind == 'floor' else
               S.normal_at(receiver, positions.reshape(-1, 3)).reshape(positions.shape))
    assert np.linalg.norm(positions[M.MID, M.MID]-point) < 1.e-7
    g = dict(pos=positions, nrm=normals, zf=np.where(hit, (positions-eye)@cam.f, np.inf))
    return cam, S.encode_gbuffer(cam, g)[1:]


def round3_checks(r, raster, reference):
    """Absolute caster retention beyond historical failures, plus attribution."""
    rows = []
    nr = K.normal_runner(r) if 'awGeoNormal(texel)' in r.tree.read(M.SHADER) else None
    cases = [(kind, radius, source, .5) for kind, radius in
             [('sphere', .15), ('sphere', .5), ('cylinder', .15), ('cylinder', .5), ('floor', 0.)]
             for source in (.1, .5, 1.)]
    cases += [(kind, .15 if kind != 'floor' else 0., .5, incidence)
              for kind in ('sphere', 'cylinder', 'floor') for incidence in (.01, .025, .05, .1)]
    for kind, radius, source, incidence in cases:
        normal, light, receiver, cover, points = fixture(kind, radius, source, incidence)
        oracle = U.truth([cover], light, points)
        r.gpu_atlas = raster.atlas(R.scene([receiver, cover], light), light, .02)
        reference.gpu_atlas = r.gpu_atlas
        if nr:
            nr.gpu_atlas = r.gpu_atlas
        coordinates = [(distance, 0., 'yaw') for distance in
                       (3., 6., 12., 24., 36., 45., 60., 80., 96., 100., 112., 128.)]
        coordinates += [(distance, angle, motion) for distance in (80., 100., 128.)
                        for motion in ('yaw', 'orbit') for angle in np.arange(-1., 1.01, .25)]
        values, historical, certificates = [], [], []
        for distance, angle, motion in coordinates:
            actual, old = [], []
            for point in points:
                cam, buffers = primary(point, receiver, normal, distance, angle, kind, motion)
                for runner, destination in ((r, actual), (reference, old)):
                    destination.append(float(K.draw(runner, cam, buffers, light, False)[M.MID, M.MID]))
            values.append(actual)
            historical.append(old)
            if nr:
                cam, buffers = primary(points[0], receiver, normal, distance, angle, kind, motion)
                K.draw(nr, cam, buffers, light, False)
                state = np.empty(4, np.float32)
                nr.gl.read(M.MID, M.MID, 1, 1, 0x1908, 0x1406, state.ctypes.data)
                error = float(np.degrees(np.arccos(np.clip(state[:3]@normal, -1., 1.))))
                certificates.append(dict(error_deg=error, validity=float(state[3]),
                                         passed=bool(error <= 5. or state[3] < .5)))
        means = np.mean(values, axis=1)
        near_visibility = float(means[0])
        jumps = [float(np.max(np.abs(np.diff(means[:12]))))]
        jumps += [float(np.max(np.abs(np.diff(means[start:start+9])))) for start in range(12, len(means), 9)]
        row = dict(name='distant_'+kind, radius=radius, source_radius=source, incidence=incidence,
            coordinates=[(float(d), float(a), m) for d, a, m in coordinates], values=values,
            reference=historical, truth=oracle.tolist(), certificates=certificates,
            maximum=float(means.max()), jump=max(jumps),
            passed=bool(means.max() <= near_visibility+.2*(1.-near_visibility) and
                        max(jumps) <= 2/255 and all(x['passed'] for x in certificates)))
        rows.append(row)
        print('[world-avatar] round3', {k: v for k, v in row.items() if k not in
              ('coordinates', 'values', 'reference', 'truth', 'certificates')}, flush=True)
        # Attribute increased darkness with exactly the production debug=3
        # sample and isolated receiver/caster atlases, at the same world points.
        if incidence == .5:
            evidence = []
            for bank, prims in (('receiver', [receiver]), ('caster', [cover]),
                                ('combined', [receiver, cover])):
                r.gpu_atlas = raster.atlas(R.scene(prims, light), light, .02)
                for distance in (3., 60., 128.):
                    visibility, debug = [], []
                    for point in points:
                        cam, buffers = primary(point, receiver, normal, distance, 0., kind)
                        visibility.append(float(K.draw(r, cam, buffers, light, False)[M.MID, M.MID]))
                        r.gl.use(r.filter)
                        r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 3)
                        image = r.run(cam, *buffers, [light], K.PLAN, denoise=0., world=K.world(light, False))[0]
                        debug.append(float(image[M.MID, M.MID]))
                    evidence.append(dict(bank=bank, distance=distance, visibility=visibility, debug3=debug))
            clean = all(min(x['visibility']) >= 1.-2/255 for x in evidence if x['bank'] == 'receiver')
            for distance in (3., 60., 128.):
                caster, combined = [np.array(next(x['visibility'] for x in evidence
                    if x['bank'] == bank and x['distance'] == distance)) for bank in ('caster', 'combined')]
                clean &= bool(np.max(np.abs(caster-combined)) <= 2/255)
            rows.append(dict(name='caster_attribution_'+kind, radius=radius, source_radius=source,
                             evidence=evidence, passed=clean))
    return rows


def checks(r, raster, out, round3_only=False):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    reference_tree = M.ShaderTree(REFERENCE, out/'shaders')
    reference = R.Runner(reference_tree, gl=r.gl)
    rows = []
    for kind, radius in ([] if round3_only else [('sphere', .15), ('sphere', .5), ('cylinder', .15),
                         ('cylinder', .5), ('floor', 0.)]):
        for source in (.1, .5, 1.):
            normal, light, receiver, cover, points = fixture(kind, radius, source)
            truth = U.truth([cover], light, points)
            # The patch can include umbra pixels alongside the penumbra; it
            # must contain actual partial source coverage, not merely mix
            # independent fully lit and fully blocked samples.
            assert .05 < truth.mean() < .95 and np.count_nonzero((truth > .05) & (truth < .95)) >= 3, truth
            r.gpu_atlas = raster.atlas(R.scene([receiver, cover], light), light, .02)
            reference.gpu_atlas = r.gpu_atlas
            walks = []
            for motion in ('distance', *('yaw%g' % distance for distance in YAW_DISTANCES)):
                coordinates = DISTANCES if motion == 'distance' else np.arange(-10., 10.01, .25)
                actual, historical = [], []
                for coordinate in coordinates:
                    distance = float(coordinate) if motion == 'distance' else float(motion[3:])
                    yaw = 0. if motion == 'distance' else float(coordinate)
                    values, old = [], []
                    for point in points:
                        cam, buffers = primary(point, receiver, normal, distance, yaw, kind)
                        for runner, destination in ((r, values), (reference, old)):
                            runner.gl.use(runner.filter)
                            runner.gl.ui(runner.gl.loc(runner.filter, b'area_world_debug'), 1)
                            image = runner.run(cam, *buffers, [light], K.PLAN, denoise=0.,
                                               world=K.world(light, False), look=dict())[0]
                            destination.append(float(image[M.MID, M.MID]))
                    actual.append(values)
                    historical.append(old)
                means = np.mean(actual, axis=1)
                old_means = np.mean(historical, axis=1)
                jump = float(np.max(np.abs(np.diff(means))))
                old_jump = float(np.max(np.abs(np.diff(old_means))))
                retention = (1.-means)/np.maximum(1.-old_means, 1.e-9)
                walks.append(dict(motion=motion, coordinates=list(map(float, coordinates)),
                    values=actual, reference=historical, means=means.tolist(),
                    reference_means=old_means.tolist(), jump=jump, reference_jump=old_jump,
                    minimum_darkness_ratio=float(retention.min()),
                    passed=bool(retention.min() >= .8 and jump <= max(2/255, old_jump+2/255))))
            # Give distance and yaw walks equal weight: adding more yaw samples
            # must not hide loss confined to the distance bank, or vice versa.
            darkness = float(np.mean([np.mean(1.-np.array(walk['means'])) for walk in walks]))
            old_darkness = float(np.mean([np.mean(1.-np.array(walk['reference_means'])) for walk in walks]))
            ratio = darkness/max(old_darkness, 1.e-9)
            row = dict(name='avatar_'+kind, radius=radius, source_radius=source, walks=walks,
                truth=truth.tolist(), darkness=darkness, reference_darkness=old_darkness,
                darkness_ratio=ratio, max_jump=max(walk['jump'] for walk in walks),
                passed=bool(.8 <= ratio <= 1.2 and all(walk['passed'] for walk in walks)))
            rows.append(row)
            print('[world-avatar]', {k: v for k, v in row.items() if k not in ('walks', 'truth')}, flush=True)
    rows += round3_checks(r, raster, reference)
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 0)
    result = dict(reference=REFERENCE, renderer=r.gl.renderer, rows=rows,
        shader_sha256=hashlib.sha256(r.tree.read(M.SHADER).encode()).hexdigest(),
        reference_sha256=hashlib.sha256(reference_tree.shader.encode()).hexdigest(),
        passed=all(row['passed'] for row in rows))
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_pop/avatar')
    ap.add_argument('--round3-only', action='store_true')
    args = ap.parse_args()
    out = Path(args.out)
    tree = M.ShaderTree(args.shader_ref, out/'shaders')
    r = R.Runner(tree)
    rows = checks(r, W.Raster(r), out, args.round3_only)
    return 0 if all(row['passed'] for row in rows) else 1


if __name__ == '__main__':
    raise SystemExit(main())
