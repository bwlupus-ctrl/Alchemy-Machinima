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


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_pop/motion')
    ap.add_argument('--bisect', action='store_true')
    args = ap.parse_args()
    out = Path(args.out)
    gl = G.GL()
    results = []
    for ref in REFS if args.bisect else [args.shader_ref]:
        tree = ShaderTree(ref, out/'shaders')
        r = R.Runner(tree, gl=gl)
        rows = checks(r, W.Raster(r))
        result = dict(shader_ref=ref, renderer=gl.renderer, rows=rows,
                      shader_sha256=hashlib.sha256(tree.shader.encode()).hexdigest(),
                      passed=all(row['passed'] for row in rows))
        results.append(result)
        (out/(ref+'.json')).write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    (out/'results.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    return 0 if all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
