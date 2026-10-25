"""Real-GL world receiver regression; no viewer build or CPU shadow atlas.

Run with --shader-ref 77348de6593 for the required failing baseline. Geometry
behind the receiver must not cast; front/contact blockers must still cast.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import sys

import numpy as np

import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_cube_source as CS
import relight_world_gl_check as WG


class Runner(G.AreaRunner):
    def _texture(self, key, unit, internal, w, h, fmt, typ, data, **kwargs):
        if key == 'atlas' and getattr(self, 'gpu_atlas', None):
            self.gl.active(0x84C0 + unit)
            self.gl.bind_tex(0x0DE1, self.gpu_atlas)
            return self.gpu_atlas
        return super()._texture(key, unit, internal, w, h, fmt, typ, data, **kwargs)


def scene(prims, light):
    return S.Scene('world receiver', prims, [[light]], [], {}, np.empty((0, 3)))


def snapshot(r, raster, sc, light, near=.02):
    atlas = raster.atlas(sc, light, near)
    saved = r._texture('saved_' + str(len(r.textures)), 7, 0x8CAC,
                       CS.ATLAS_W, CS.ATLAS_H_PER_LIGHT, 0x1902, 0x1406, None)
    raster.copy(atlas, 0x0DE1, 0, 0, 0, 0, saved, 0x0DE1, 0, 0, 0, 0,
                CS.ATLAS_W, CS.ATLAS_H_PER_LIGHT, 1)
    return saved


def camera_checks(r, raster, elevation=.455, radius=.1):
    light = S.Light('horizon lamp', (0, 0, elevation), radius, 8., seed=123)
    actor = [S.Prim('curved receiver', 'sphere', c=(.5, .5, .4), r=.05)]
    background = [S.Prim('leaf', 'sphere', c=(.7+i*.04, .7-i*.04, .40+j*.025), r=.017)
                  for i in range(-3, 4) for j in range(5) if (i+j) % 3]
    foreground = [S.Prim('front blocker', 'sphere', c=(.25, .25, (elevation+.45)*.5), r=.15)]
    sc = scene(actor+background+foreground, light)
    r.gpu_atlas = snapshot(r, raster, sc, light)
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 1)
    rows = []
    world = dict(atlas=np.empty((1088, 1632), np.float32), channels=[dict(
        row=0, near=.02, far=8., r_map=radius, contact=.0, seed=123)])
    for edge in (-1, 1):
        normal = S.unit(np.array([edge*.005, -edge*.005, 1.]))
        point = np.array([.5, .5, .4]) + .05*normal
        values = []
        for distance in (.3, 1., 4.):
            for side in (-1, 1):
                cam = S.Camera(point+S.unit(np.array([side, side, 1.]))*distance,
                               point, 241, 181, near=.02)
                g = S.cast_primary(sc, cam)
                # Fixed material detail in world space, then encoded by the
                # actual G-buffer encoder for this view. Never camera noise.
                tangent = np.cross(g['nrm'], np.asarray(light.pos)-g['pos'])
                tangent /= np.maximum(np.linalg.norm(tangent, axis=2, keepdims=True), 1.e-9)
                phase = np.sin(g['pos'][..., 0]*137.+g['pos'][..., 2]*211.)
                g['nrm'] = g['nrm'] + .35*phase[..., None]*tangent
                g['nrm'] /= np.maximum(np.linalg.norm(g['nrm'], axis=2, keepdims=True), 1.e-9)
                _, depth, normals = S.encode_gbuffer(cam, g)
                for half in (True, False):
                    v = r.run(cam, depth, normals, [light], dict(samples=8, steps=12,
                              half=half, near_steps=0), denoise=0., world=world)[0]
                    val = float(v[90, 120]); values.append(val)
                    rows.append(dict(edge=edge, radius=radius, elevation=elevation, distance=distance, side=side, half=half,
                                     point=g['pos'][90, 120].tolist(), visibility=val))
        rows.append(dict(edge=edge, radius=radius, elevation=elevation, minimum=min(values),
                         spread=float(np.ptp(values)), passed=bool(np.ptp(values) <= 2/255 and
                         (elevation < .5 or min(values) < .9))))
    return rows


class TapProbe:
    """Production world functions at exact points/normals; raster stays real GL."""
    def __init__(self, r):
        self.r = r
        text = r.tree.read(G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl')
        body = '''uniform sampler2D receiver_points;
uniform sampler2D receiver_normals;
uniform int receiver_path;
void main() {
    ivec2 xy = ivec2(gl_FragCoord.xy);
    area_p = texelFetch(receiver_points,xy,0).xyz;
    area_n_geo = texelFetch(receiver_normals,xy,0).xyz;
    area_n_shade = area_n_geo;
    float v = receiver_path == 1 ? awFaceVisibility(0) :
              receiver_path == 2 ? awDirectionVisibility(0,1.0) : awVisibility(0);
    frag_color = vec4(vec3(v),1);
}'''
        self.pr = r._link('class3/deferred/areaShadowFilterF.glsl',
                          [(text, text[:text.index('void main()')]+body)])

    def run(self, points, normals, light, detail=False, path=0):
        r, gl, pr = self.r, self.r.gl, self.pr
        data = np.asarray(points, np.float32).reshape(1, -1, 3)
        n = np.asarray(normals, np.float32).reshape(data.shape)
        r._texture('receiver_points', 6, 0x8815, data.shape[1], 1, 0x1907, 0x1406, data)
        r._texture('receiver_normals', 7, 0x8815, data.shape[1], 1, 0x1907, 0x1406, n)
        r._target('probe_output', data.shape[1], 1); r._begin(pr)
        gl.ui(gl.loc(pr, b'receiver_points'), 6); gl.ui(gl.loc(pr, b'receiver_normals'), 7)
        gl.ui(gl.loc(pr, b'receiver_path'), path)
        gl.ui(gl.loc(pr, b'area_world_atlas'), 4)
        gl.active(0x84C4); gl.bind_tex(0x0DE1, r.gpu_atlas)
        info = np.zeros((4, 4), np.float32); info[0] = (0, .02, light.radius, float(detail))
        lights = np.zeros((4, 4), np.float32); lights[0] = (*light.pos, light.range)
        seed = np.array([light.seed, 0, 0, 0], np.uint32)
        rot = G.pack_view_rotation(np.eye(4))
        for name, value in [(b'area_world_info', info), (b'area_world_light', lights)]:
            gl.u4fv(gl.loc(pr, name), 4, value.ctypes.data_as(C.POINTER(gl.F)))
        gl.u4uiv(gl.loc(pr, b'area_world_seed'), 1, seed.ctypes.data_as(C.POINTER(gl.U)))
        gl.u3fv(gl.loc(pr, b'area_world_rot'), 3, rot.ctypes.data_as(C.POINTER(gl.F)))
        gl.draw(4, 0, 3); gl.finish()
        result = np.empty((1, data.shape[1], 4), np.uint8)
        gl.read(0, 0, data.shape[1], 1, 0x1908, 0x1401, result.ctypes.data)
        return result[0, :, 0].astype(float)/255


def depth_checks(r, raster):
    probe = TapProbe(r); rows = []
    for radius in (.1, 1.):
        light = S.Light('close area source', (0, 0, 0), radius, 8., seed=123)
        for seam in (False, True):
            direction = S.unit(np.array([1., 1. if seam else .25, .1]))
            tangent = S.unit(np.cross(direction, (0, 0, 1.)))
            normal = S.unit(-.035*direction+tangent)
            for dist in (.3, 1.):
                centre = dist*direction
                # Dappled thin blockers entirely farther from the light than
                # every receiver. Deliberately omit the receiver's self depth:
                # a missing caster must not allow a background to become one.
                behind = [S.Prim('behind leaf', 'sphere', c=(dist+.020)*direction+i*.009*tangent,
                                 r=.008) for i in range(-8, 9) if i % 3]
                front = [S.Prim('contact blocker', 'sphere', c=(dist-.105)*direction, r=.10)]
                points = centre + np.linspace(-.002, .002, 17)[:, None]*tangent
                normals = np.tile(normal, (len(points), 1))
                for placement, prims in [('behind', behind), ('front', front)]:
                    test_normals = normals if placement == 'behind' else np.tile(-direction, (len(points), 1))
                    r.gpu_atlas = raster.atlas(scene(prims, light), light, .02)
                    for detail in (False, True):
                        for path in (0, 1, 2):
                            v = probe.run(points, test_normals, light, detail, path)
                            passed = v.min() >= 1.-2/255 if placement == 'behind' else v.min() < .9
                            rows.append(dict(radius=radius, seam=seam, distance=dist,
                                             placement=placement, detail=detail, path=path,
                                             minimum=float(v.min()), mean=float(v.mean()), passed=bool(passed)))
                # Independent background-add/remove control with a real curved
                # receiver in BOTH rasters. Its absence above is a separate
                # missing-caster stress test, never the only H2 reproduction.
                self_prim = S.Prim('curved receiver', 'sphere', c=centre-.05*normal, r=.05)
                curved_normals = normal + np.linspace(-.002,.002,17)[:,None]*tangent/.05
                curved_normals /= np.linalg.norm(curved_normals,axis=1,keepdims=True)
                curved_points = centre-.05*normal+.05*curved_normals
                outputs = {}
                for placement, prims in [('self', [self_prim]), ('self_behind', [self_prim]+behind)]:
                    r.gpu_atlas = raster.atlas(scene(prims,light),light,.02)
                    for detail in (False,True):
                        for path in (0,1,2):
                            v = probe.run(curved_points,curved_normals,light,detail,path)
                            if placement=='self':
                                outputs[detail,path]=v
                            else:
                                delta=float(np.max(np.abs(v-outputs[detail,path])))
                                rows.append(dict(radius=radius,seam=seam,distance=dist,
                                                 placement=placement,detail=detail,path=path,
                                                 background_delta=delta,passed=delta<=2/255))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_seethru/receiver')
    ap.add_argument('--controls', action='store_true', help='also require independent old-normal and unbounded-plane mutants to fail')
    args = ap.parse_args()
    out = Path(args.out); out.mkdir(parents=True, exist_ok=True)
    original_mesh = WG.mesh
    mesh_cache = {}
    def cached_mesh(prim):
        key = json.dumps(prim.spec())
        if key not in mesh_cache:
            mesh_cache[key] = original_mesh(prim)
        return mesh_cache[key]
    WG.mesh = cached_mesh
    r = Runner(G.SourceTree(args.shader_ref)); raster = WG.Raster(r)
    camera = [row for elevation in (.455,.55) for radius in (.1,1.)
              for row in camera_checks(r, raster, elevation, radius)]
    depths = depth_checks(r, raster)
    result = dict(shader_ref=args.shader_ref, renderer=r.gl.renderer, camera=camera,
                  depth=depths, passed=all(x['passed'] for x in camera+depths if 'passed' in x))
    if args.controls:
        normal_tree = WG.MutantTree(r.tree, [('dot(area_world_rot[0],area_n_shade),dot(area_world_rot[1],area_n_shade),dot(area_world_rot[2],area_n_shade)',
                                            'dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo)')])
        mutant = Runner(normal_tree, gl=r.gl)
        normal_rows = camera_checks(mutant, WG.Raster(mutant))
        plane_tree = WG.MutantTree(r.tree, [('return vec4(lessThan((z+bias)*max(vec4(den+du,den+dr+du,den+dr,den),vec4(1.e-5)),vec4(receiver2)));',
                                           'return vec4(1.0);')])
        mutant = Runner(plane_tree, gl=r.gl)
        plane_rows = depth_checks(mutant, WG.Raster(mutant))
        controls = dict(old_normal_rejected=any(not x['passed'] for x in normal_rows if 'passed' in x),
                        unbounded_face_rejected=any(not x['passed'] for x in plane_rows if x['path']==1 and not x['detail'] and x['placement']=='behind'),
                        unbounded_detail_rejected=any(not x['passed'] for x in plane_rows if x['path']==1 and x['detail'] and x['placement']=='behind'),
                        unbounded_direction_rejected=any(not x['passed'] for x in plane_rows if x['path']==2 and x['placement']=='behind'))
        result.update(controls=controls, passed=result['passed'] and all(controls.values()))
        print('[receiver] must-fail controls', controls, flush=True)
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('[receiver]', args.shader_ref, 'camera', [x for x in camera if 'spread' in x], flush=True)
    print('[receiver] depth failures', [x for x in depths if not x['passed']], flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
