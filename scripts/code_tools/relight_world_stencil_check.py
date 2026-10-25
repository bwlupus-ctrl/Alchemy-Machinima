"""Production-GL thin-receiver and distant-plane regressions (24-bit depth).

Receivers use analytic primary intersections; shadow cubes use production GL
rasterization. The independent geometry oracle verifies every foreground ray.
No thresholds from the existing receiver/camera gates are changed.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import relight_world_gl_check as WG
import relight_world_receiver_check as R


PLAN = dict(samples=8, steps=12, half=True, near_steps=0)
SIZE = 257
MID = SIZE // 2
FOV = np.degrees(2*np.arctan(np.tan(np.radians(30))*SIZE/2160))


def camera(distance, near=.25, far=1024., shift=0., jitter=0.):
    cam = S.Camera((shift, -distance, 0), (shift, 0, 0), SIZE, SIZE,
                   vfov=FOV, near=near, far=far)
    # TAA-like projection jitter, in full-resolution pixels. The same shifted
    # rays generate the primary depth; this is not noise added to stored depth.
    cam.proj[0, 2] = 2*jitter/SIZE
    return cam


def primary(cam, normal, strip_width=None, gap=.01):
    rays = cam.rays(0, SIZE)
    rays /= (rays@cam.f)[..., None]
    rays += cam.r*cam.proj[0, 2]*cam.tan*cam.aspect
    rays /= np.linalg.norm(rays, axis=2, keepdims=True)
    n = np.asarray(normal)
    z = -(cam.eye@n)/(rays@n)
    p = cam.eye+z[..., None]*rays
    mask = np.ones((SIZE, SIZE), bool)
    if strip_width is not None:
        mask = abs(p[..., 0]) < strip_width/2
        zb = (gap-cam.eye[1])/rays[..., 1]
        p = np.where(mask[..., None], p, cam.eye+zb[..., None]*rays)
    g = dict(pos=p, nrm=np.broadcast_to(n, p.shape).copy(),
             zf=(p-cam.eye)@cam.f)
    return g, S.encode_gbuffer(cam, g)[1:], mask


def slab(normal):
    n = np.asarray(normal)
    a = S.unit(np.cross(n, [0, 0, 1]))
    b = np.cross(n, a)
    prim = S.Prim('receiver plane', 'obb', c=-.02*n, h=(100, 100, .02))
    prim.rot = lambda: np.column_stack((a, b, n))
    return prim


def world(light, detail):
    return dict(atlas=np.empty((1088, 1632), np.float32), channels=[dict(
        row=0, near=.02, far=light.range, r_map=light.radius, contact=0.,
        seed=light.seed, detail_shadows=detail)])


def normal_runner(r):
    target = 'area_n_geo = awGeoNormal(texel);'
    output = target+'''
        frag_color = vec4(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo),aw_plane_valid);
        return;'''
    class DiagnosticTree(G.SourceTree):
        # Preserve an outer mutation too: wrapping MutantTree in another
        # MutantTree would read the underlying ref and discard its mutation.
        def read(self, path):
            text = r.tree.read(path)
            if str(path).endswith('areaShadowFilterF.glsl'):
                assert target in text
                text = text.replace(target, output)
            return text
    return R.NormalRunner(DiagnosticTree(r.tree.ref), gl=r.gl)


def draw(r, cam, buffers, light, detail):
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 1)
    return r.run(cam, *buffers, [light], PLAN, denoise=0., world=world(light, detail))[0]


def thin_checks(r, quick=False):
    nr = normal_runner(r)
    rows = []
    for radius in (.1, 1.):
        light = S.Light('thin foreground', (-1., -.05, 0), radius, 16., seed=123)
        direction = S.unit(light.pos)
        blocker = S.Prim('confirmed foreground cover', 'sphere', c=.4*direction, r=.30)
        r.gpu_atlas = WG.Raster(r).atlas(R.scene([blocker], light), light, .02)
        nr.gpu_atlas = r.gpu_atlas
        for width in (1, 3):
            footprint = 2*2*np.tan(np.radians(30))/2160
            motions = ((0., 0.),) if quick else ((0., 0.), (.19, -.375), (-.23, .25), (.31, .4375))
            for gap, move, jitter in ((gap, *motion) for gap in (.001, .003, .01) for motion in motions):
                cam = camera(2., near=.25, far=256., shift=move*footprint, jitter=jitter)
                g, buffers, mask = primary(cam, [0, -1, 0], width*footprint, gap)
                select = mask & (abs(np.indices(mask.shape)[0]-MID) <= 4)
                points = g['pos'][select]
                ray = np.asarray(light.pos)-points
                distance = np.linalg.norm(ray, axis=1)
                truth = S.isect(blocker, points, ray/distance[:, None]) < distance
                assert len(points) and truth.all()
                draw(nr, cam, buffers, light, False)
                value = np.empty(4, np.float32)
                nr.gl.read(MID, MID, 1, 1, 0x1908, 0x1406, value.ctypes.data)
                for detail in (False, True):
                    values = draw(r, cam, buffers, light, detail)[select]
                    rows.append(dict(width_px=width, gap=gap, radius=radius, motion_px=move,
                                     jitter_px=jitter, detail=detail, samples=len(values),
                                     truth_blocked=int(truth.sum()), normal=value.tolist(),
                                     minimum=float(values.min()), maximum=float(values.max()),
                                     passed=bool(values.max() <= 2/255)))
    return rows


def distance_checks(r, quick=False):
    nr = normal_runner(r)
    rows = []
    n = S.unit([np.sin(np.radians(60)), -np.cos(np.radians(60)), 0])
    light = S.Light('unobstructed sloping plane', (0, -1, 0), .1, 16., seed=123)
    r.gpu_atlas = WG.Raster(r).atlas(R.scene([slab(n)], light), light, .02)
    nr.gpu_atlas = r.gpu_atlas
    distances = (2., 127., 128., 130., 512.) if quick else (1., 2., 8., 32., 64., 95., 96., 100., 112., 120., 126., 127., 127.5, 128., 128.5, 129., 130., 192., 256., 384., 512.)
    for near in ((.25,) if quick else (.02, .1, .25, 1.)):
        for far in (256., 1024.):
            for distance in distances:
                if distance <= near or distance*1.15 >= far:
                    continue
                for jitter in ((0.,) if quick else (-.375, 0., .4375)):
                    cam = camera(distance, near, far, jitter=jitter)
                    _, buffers, _ = primary(cam, n)
                    draw(nr, cam, buffers, light, False)
                    value = np.empty(4, np.float32)
                    nr.gl.read(MID, MID, 1, 1, 0x1908, 0x1406, value.ctypes.data)
                    angle = float(np.degrees(np.arccos(np.clip(value[:3]@n/np.linalg.norm(value[:3]), -1, 1))))
                    for detail in (False, True):
                        values = draw(r, cam, buffers, light, detail)[MID-2:MID+3, MID-2:MID+3]
                        rows.append(dict(distance=distance, near=near, far=far, jitter_px=jitter,
                                         detail=detail, angular_error_deg=angle, validity=float(value[3]),
                                         minimum=float(values.min()), maximum=float(values.max()),
                                         passed=bool(values.min() >= 1-2/255 and angle <= 1. and value[3] >= .999)))
            print('[stencil] distance progress', near, far, 'rows', len(rows), flush=True)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_seethru/FIX4/stencil')
    ap.add_argument('--quick', action='store_true')
    ap.add_argument('--controls', action='store_true')
    args = ap.parse_args()
    r = R.Runner(G.SourceTree(args.shader_ref))
    thin = thin_checks(r, args.quick)
    distant = distance_checks(r, args.quick)
    result = dict(shader_ref=args.shader_ref, renderer=r.gl.renderer, thin=thin, distant=distant,
                  shader_sha256=hashlib.sha256(r.tree.read(G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl').encode('utf-8')).hexdigest(),
                  passed=all(row['passed'] for row in thin+distant))
    if args.controls:
        controls = {}
        for name, old, new, check in (
                ('inconsistent_tangents', 'return valid;', 'tangent = abs(l.z) < abs(r.z) ? l : r; return 1.0;', thin_checks),
                ('distance_cutoff', 'aw_plane_valid = geometry.w;',
                 'aw_plane_valid = geometry.w*(1.0-smoothstep(96.0,128.0,-area_p.z));', distance_checks),
                ('adaptive_radius', 'float radius = clamp(uncertainty/max(0.005*footprint,1.e-8)-16.0,1.0,128.0);',
                 'float radius = 1.0;', distance_checks),
                ('depth_precision_compare', 'bias += depth_interval.x;',
                 'bias += 0.0;', distance_checks)):
            mutant = R.Runner(WG.MutantTree(r.tree, [(old, new)]), gl=r.gl)
            controls[name+'_rejected'] = any(not row['passed'] for row in check(mutant, True))
        result['controls'] = controls
        result['passed'] &= all(controls.values())
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    for label, rows in (('thin', thin), ('distant', distant)):
        failures = [row for row in rows if not row['passed']]
        print('[stencil]', label, 'cases', len(rows), 'failures', len(failures), 'worst', min(row['minimum'] for row in rows), flush=True)
        print('[stencil]', label, 'failure examples', failures[:8], flush=True)
    print('[stencil] passed', result['passed'], result.get('controls', {}), flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
