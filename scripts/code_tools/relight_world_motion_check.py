"""Real-GL camera regression: fixed world receivers, light and rasterized atlas.

The five tracked world points never change with camera distance or orientation.
Each is the centre of a 65-pixel crop of a 2160-line/60-degree D24 view, avoiding
the false distance loss from averaging a pixel patch whose world area grows.
Only the production filter shader changes in --bisect; copies go under --out.
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


SHADER = G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl'
REFS = ['eb1cb4a2b32', 'fbc92fca99c', '3cf662f1aca', '35ba32ce354',
        'cd2dd250991', '2f8883ee11e', '4e47debbf18']
SIZE = 65
MID = SIZE//2
FOV = np.degrees(2*np.arctan(np.tan(np.radians(30))*SIZE/2160))
YY, XX = np.indices((SIZE, SIZE))
NDC = np.stack([(XX+.5)/SIZE*2-1, (YY+.5)/SIZE*2-1,
                np.ones_like(XX), np.ones_like(XX)], axis=-1)


class ShaderTree(G.SourceTree):
    """Isolate a shader revision from unrelated historical harness modules."""
    def __init__(self, ref, out):
        super().__init__('WORKTREE')
        self.shader_ref = ref
        self.shader = G.SourceTree(ref).read(SHADER)
        out.mkdir(parents=True, exist_ok=True)
        (out/(ref+'.glsl')).write_text(self.shader, encoding='utf-8')

    def read(self, path):
        return self.shader if str(path) == SHADER else super().read(path)


def primary(point, normal, distance, angle, motion, floor):
    """A real yaw/orbit, cropped around the projection of one fixed point."""
    a = np.radians(angle)
    rotation = np.array([[np.cos(a), -np.sin(a), 0.],
                         [np.sin(a), np.cos(a), 0.], [0., 0., 1.]])
    offset = np.array([0., -.6, .8]) if floor else np.array([0., -1., 0.])
    eye = distance*(rotation@offset if motion == 'orbit' else offset)
    forward = S.unit(-eye)
    if motion == 'yaw':
        forward = rotation@forward
    cam = S.Camera(eye, eye+forward, SIZE, SIZE, FOV, .1, 256.)
    view_point = cam.view@np.r_[point, 1.]
    cam.proj[0, 2] = view_point[0]/(-view_point[2]*cam.tan*cam.aspect)
    cam.proj[1, 2] = view_point[1]/(-view_point[2]*cam.tan)
    h = NDC@np.linalg.inv(cam.proj).T
    rays = (h[..., :3]/h[..., 3:])@cam.view[:3, :3]
    rays /= np.linalg.norm(rays, axis=2, keepdims=True)
    length = -(cam.eye@normal)/(rays@normal)
    positions = cam.eye+length[..., None]*rays
    assert np.linalg.norm(positions[MID, MID]-point) < 1.e-8
    g = dict(pos=positions, nrm=np.broadcast_to(normal, positions.shape).copy(),
             zf=(positions-cam.eye)@cam.f)
    return cam, S.encode_gbuffer(cam, g)[1:]


def plane(normal):
    across = S.unit(np.cross(normal, [0., 1., 1.]))
    along = np.cross(normal, across)
    receiver = S.Prim('receiver plane', 'obb', c=-.02*normal, h=(100., 100., .02))
    receiver.rot = lambda: np.column_stack((across, along, normal))
    return receiver


def fixture(floor):
    normal = np.array([0., 0., 1.]) if floor else np.array([0., -1., 0.])
    light = S.Light('fixed camera regression lamp', tuple(5*normal), .5, 32., seed=123)
    cover = plane(normal)
    cover.name = 'finite foreground strip'
    cover.c = .15*normal
    cover.h = (.01, .2, .001)
    # Use the actual box axes, so the same patch lies inside either strip.
    across, along = cover.rot()[:, 0], cover.rot()[:, 1]
    points = np.array([np.zeros(3), .001*across, -.001*across,
                       .025*along, -.025*along])
    return normal, light, cover, points


def checks(r, raster):
    rows = []
    for floor in (False, True):
        normal, light, cover, points = fixture(floor)
        oracle = U.truth([cover], light, points)
        assert np.all(oracle < .35), oracle
        r.gpu_atlas = raster.atlas(R.scene([plane(normal), cover], light), light, .02)
        for half in (True, False):
            plan = dict(K.PLAN, half=half)
            for motion in ('yaw', 'orbit', 'distance'):
                coordinates = (np.arange(-10., 10.01, .25) if motion != 'distance' else
                               np.unique(np.r_[np.arange(2., 129., 2.), 44.25, 44.5, 44.75, 45.]))
                values = []
                for coordinate in coordinates:
                    distance = float(coordinate) if motion == 'distance' else 44.5
                    angle = 0. if motion == 'distance' else float(coordinate)
                    samples = []
                    for point in points:
                        cam, buffers = primary(point, normal, distance, angle, motion, floor)
                        r.gl.use(r.filter)
                        r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 1)
                        image = r.run(cam, *buffers, [light], plan, denoise=0.,
                                      world=K.world(light, False), look=dict())[0]
                        samples.append(float(image[MID, MID]))
                    values.append(samples)
                means = np.mean(values, axis=1)
                jump = float(np.max(np.abs(np.diff(means))))
                spread = float(np.ptp(means))
                row = dict(name=('floor' if floor else 'wall')+'_'+motion,
                           half=half, coordinates=coordinates.tolist(), values=values,
                           means=means.tolist(), truth=oracle.tolist(), jump=jump,
                           spread=spread, minimum=float(means.min()), maximum=float(means.max()),
                           passed=bool(means.max() <= .35 and jump <= 2/255 and spread <= 4/255))
                rows.append(row)
                print('[world-motion]', {k: v for k, v in row.items() if k not in
                      ('coordinates', 'values', 'means', 'truth')}, flush=True)
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 0)
    return rows


def grazing_checks(r, raster):
    """Fixed near-horizon caster, including the exact round-three yaw pop."""
    rows = []
    normal = np.array([0., -1., 0.])
    for incidence in (.01, .025, .05, .1):
        direction = np.array([np.sqrt(1.-incidence**2), -incidence, 0.])
        light = S.Light('grazing camera regression', tuple(5.*direction), .5, 32., seed=123)
        cover = plane(direction)
        cover.c = .1*direction
        cover.h = (.02, .2, .001)
        point = np.zeros(3)
        oracle = float(U.truth([cover], light, point[None, :])[0])
        assert oracle == 0., oracle
        r.gpu_atlas = raster.atlas(R.scene([plane(normal), cover], light), light, .02)
        for half in (True, False):
            plan = dict(K.PLAN, half=half)
            anchor = None
            for distance in (3., 12., 44.5, 60., 80., 100., 128.):
                for motion in ('yaw', 'orbit'):
                    angles = np.arange(-1., 1.01, .25)
                    values = []
                    for angle in angles:
                        cam, buffers = primary(point, normal, distance, angle, motion, False)
                        r.gl.use(r.filter)
                        r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 1)
                        image = r.run(cam, *buffers, [light], plan, denoise=0.,
                                      world=K.world(light, False))[0]
                        values.append(float(image[MID, MID]))
                    if anchor is None:
                        anchor = values[len(values)//2]
                    jump = float(np.max(np.abs(np.diff(values))))
                    row = dict(name='grazing_'+motion, incidence=incidence, distance=distance,
                        half=half, coordinates=angles.tolist(), values=values, truth=oracle,
                        anchor=anchor, maximum=max(values), jump=jump,
                        passed=bool(max(values) <= min(.9, anchor+4/255) and
                                    np.ptp(values) <= 4/255 and jump <= 2/255))
                    rows.append(row)
        print('[world-motion] grazing', incidence, 'failures',
              sum(not row['passed'] for row in rows if row['incidence'] == incidence), flush=True)
    return rows


def precision_checks(r):
    """Execute production confidence contracts and the degenerate return in GL."""
    text = r.tree.read(SHADER)
    target = 'area_n_geo = awGeoNormal(texel);'
    if target not in text:
        return [dict(name='precision_contract', applicable=False,
                     reason='pre-series shader has no reconstruction support state', passed=True)]
    normal = np.array([0., -1., 0.])
    light = S.Light('precision contract', (0., -5., 0.), .1, 32., seed=123)
    rows = []
    for name, distance, body, extra in (
        ('degenerate_support_reset', 12., '''
            aw_depth_support=1.0; aw_precision_support=1.0;
            awGeoStencil(ivec2(gl_FragCoord.xy),textureSize(depthMap,0),1,0.01,1.0);
            frag_color=vec4(aw_depth_support,aw_precision_support,0,1);''',
         [('if (!(n2 > 1.e-20))', 'n2=0.0; if (!(n2 > 1.e-20))')]),
        ('backface_precision_support', 16., '''
            aw_plane_valid=1.0; aw_precision_support=1.0;
            frag_color=vec4(awBackfaceWeight(-0.08,5.0,0.1),aw_precision_support,0,1);''', []),
        ('backface_angular_cap', 32., '''
            aw_plane_valid=1.0; aw_precision_support=0.0;
            frag_color=vec4(awBackfaceWeight(-0.33,5.0,0.1),aw_precision_support,0,1);''', []),
        ('compare_precision_support', 24., '''
            aw_precision_support=1.0;
            frag_color=vec4(awPrecision(1.0,0.02).x-min(awDepthError(),AW_NORMAL_CAP),aw_precision_support,0,1);''', [])):
        class DiagnosticTree(G.SourceTree):
            def read(self, path):
                source = r.tree.read(path)
                if str(path) == SHADER:
                    for before, after in extra+[(target, target+body+'return;')]:
                        assert before in source, before
                        source = source.replace(before, after)
                return source
        mutant = DiagnosticTree('WORKTREE')
        nr = R.NormalRunner(mutant, gl=r.gl)
        cam, buffers = primary(np.zeros(3), normal, distance, 0., 'yaw', False)
        K.draw(nr, cam, buffers, light, False)
        value = np.empty(4, np.float32)
        nr.gl.read(MID, MID, 1, 1, 0x1908, 0x1406, value.ctypes.data)
        passed = (bool(np.max(np.abs(value[:2])) < 1.e-6) if name in ('degenerate_support_reset', 'backface_angular_cap')
                  else bool(abs(value[0]) < 1.e-6 and value[1] >= .999))
        rows.append(dict(name=name, value=value.tolist(), passed=passed))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_pop/motion')
    ap.add_argument('--bisect', action='store_true')
    ap.add_argument('--round3-only', action='store_true')
    ap.add_argument('--controls', action='store_true')
    args = ap.parse_args()
    out = Path(args.out)
    gl = G.GL()
    results = []
    for ref in REFS if args.bisect else [args.shader_ref]:
        tree = ShaderTree(ref, out/'shaders')
        r = R.Runner(tree, gl=gl)
        raster = W.Raster(r)
        rows = [] if args.round3_only else checks(r, raster)
        rows += grazing_checks(r, raster)+precision_checks(r)
        result = dict(shader_ref=ref, renderer=gl.renderer, rows=rows,
                      shader_sha256=hashlib.sha256(tree.shader.encode()).hexdigest(),
                      passed=all(row['passed'] for row in rows))
        if args.controls:
            mutant = R.Runner(W.MutantTree(tree, [
                ('(1.0-aw_precision_support)*', '')]), gl=gl)
            controls = precision_checks(mutant)
            result['controls'] = dict(precision_weight_removed=dict(rows=controls,
                rejected=any(not row['passed'] for row in controls
                             if row['name'] == 'compare_precision_support')))
            capped = 'float reconstruction = min((1.0-aw_precision_support)*2.0*awDepthFraction()/max(pixel_angle,1.e-9),0.05);'
            mutant = R.Runner(W.MutantTree(tree, [(capped,
                'float reconstruction = (1.0-aw_precision_support)*2.0*awDepthFraction()/max(pixel_angle,1.e-9);')]), gl=gl)
            controls = precision_checks(mutant)
            result['controls']['uncapped_backface'] = dict(rows=controls,
                rejected=any(not row['passed'] for row in controls if row['name'] == 'backface_angular_cap'))
            result['passed'] &= all(control['rejected'] for control in result['controls'].values())
        results.append(result)
        (out/(ref+'.json')).write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    return 0 if all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
