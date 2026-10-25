"""Real-GL world receiver regression; no viewer build or CPU shadow atlas.

Run with --shader-ref fbc92fca99c for the round-two failing baseline. Geometry
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
    direction = S.unit(np.array([.5,.5,.45])-np.asarray(light.pos))
    across = S.unit(np.cross(direction,[0,0,1]))
    wall = S.Prim('full foreground wall','obb',c=np.asarray(light.pos)+.35*direction,h=(.025,2.,2.))
    wall.rot = lambda: np.column_stack([direction,across,np.cross(direction,across)])
    foreground = [wall]
    # Hold camera-visible geometry fixed. The foreground control is added to
    # the light raster; a primary ray must never silently measure that blocker
    # instead of the receiver whose invariance we claim to check.
    sc = scene(actor, light)
    r.gpu_atlas = snapshot(r, raster, scene(actor+background+(foreground if elevation >= .5 else []), light), light)
    r.gl.use(r.filter)
    r.gl.ui(r.gl.loc(r.filter, b'area_world_debug'), 1)
    rows = []
    world = dict(atlas=np.empty((1088, 1632), np.float32), channels=[dict(
        row=0, near=.02, far=8., r_map=radius, contact=.0, seed=123)])
    for edge, patch in ((edge, patch) for edge in (-1, 1) for patch in (-.02, 0., .02)):
        normal = S.unit(np.array([edge*.005+patch, -edge*.005-patch, 1.]))
        point = np.array([.5, .5, .4]) + .05*normal
        values = []
        for distance in (.3, 1., 4.):
            for side in (-1, 1):
                cam = S.Camera(point+S.unit(np.array([side, side, 1.]))*distance,
                               point, 241, 181,
                               # A centre crop of a 2160-line, 60-degree view:
                               # full-resolution depth footprints, no 181-line
                               # sphere reduced to two pixels at the far view.
                               vfov=np.degrees(2*np.arctan(np.tan(np.radians(30))*181/2160)), near=.02)
                g = S.cast_primary(sc, cam)
                # Fixed material detail in world space, then encoded by the
                # actual G-buffer encoder for this view. Never camera noise.
                geometric = g['nrm'].copy()
                light_ray = np.asarray(light.pos)-g['pos']
                tangent = light_ray-(light_ray*geometric).sum(2)[..., None]*geometric
                tangent /= np.maximum(np.linalg.norm(tangent, axis=2, keepdims=True), 1.e-9)
                phase = np.sin(g['pos'][..., 0]*137.+g['pos'][..., 2]*211.)
                for sign in (-1, 1):
                    g['nrm'] = geometric + sign*.4663*(.75+.25*phase)[..., None]*tangent
                    g['nrm'] /= np.maximum(np.linalg.norm(g['nrm'], axis=2, keepdims=True), 1.e-9)
                    _, depth, normals = S.encode_gbuffer(cam, g)
                    for half in (True, False):
                        v = r.run(cam, depth, normals, [light], dict(samples=8, steps=12,
                                  half=half, near_steps=0), denoise=0., world=world)[0]
                        val = float(v[90, 120]); values.append(val)
                        rows.append(dict(edge=edge, patch=patch, radius=radius, elevation=elevation, distance=distance, side=side, half=half, tilt_sign=sign,
                                         point=g['pos'][90, 120].tolist(), visibility=val))
        rows.append(dict(edge=edge, patch=patch, radius=radius, elevation=elevation, minimum=min(values),
                         spread=float(np.ptp(values)), passed=bool(np.ptp(values) <= 2/255 and
                         (max(values) <= .1 if elevation >= .5 else min(values) >= 1-2/255))))
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
    area_n_shade = texelFetch(receiver_shading,xy,0).xyz;
    float v = receiver_path == 1 ? awFaceVisibility(0) :
              receiver_path == 2 ? awDirectionVisibility(0,1.0) : awVisibility(0);
    frag_color = vec4(vec3(v),1);
}'''
        body = 'uniform sampler2D receiver_shading;\n' + body
        self.pr = r._link('class3/deferred/areaShadowFilterF.glsl',
                          [(text, text[:text.index('void main()')]+body)])

    def run(self, points, normals, light, detail=False, path=0, shading=None):
        r, gl, pr = self.r, self.r.gl, self.pr
        data = np.asarray(points, np.float32).reshape(1, -1, 3)
        n = np.asarray(normals, np.float32).reshape(data.shape)
        r._texture('receiver_points', 6, 0x8815, data.shape[1], 1, 0x1907, 0x1406, data)
        r._texture('receiver_normals', 7, 0x8815, data.shape[1], 1, 0x1907, 0x1406, n)
        shade = n if shading is None else np.asarray(shading, np.float32).reshape(data.shape)
        r._texture('receiver_shading', 5, 0x8815, data.shape[1], 1, 0x1907, 0x1406, shade)
        r._target('probe_output', data.shape[1], 1); r._begin(pr)
        gl.ui(gl.loc(pr, b'receiver_points'), 6); gl.ui(gl.loc(pr, b'receiver_normals'), 7)
        gl.ui(gl.loc(pr, b'receiver_shading'), 5)
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
                    test_normals = normals
                    r.gpu_atlas = raster.atlas(scene(prims, light), light, .02)
                    for detail in (False, True):
                        for path in (0, 1, 2):
                            v = probe.run(points, test_normals, light, detail, path)
                            # All centre rays must independently hit the real
                            # foreground geometry, with the SAME grazing normal.
                            ray = np.asarray(light.pos)-points
                            distance = np.linalg.norm(ray, axis=1)
                            ray /= distance[:, None]
                            truth = np.zeros(len(points), bool)
                            for prim in prims:
                                truth |= S.isect(prim, points, ray) < distance
                            passed = v.min() >= 1.-2/255 if placement == 'behind' else bool(truth.all() and v.max() < .9)
                            rows.append(dict(radius=radius, seam=seam, distance=dist,
                                             placement=placement, detail=detail, path=path,
                                             minimum=float(v.min()), maximum=float(v.max()), mean=float(v.mean()),
                                             truth_blocked=int(truth.sum()), samples=len(points), passed=bool(passed)))
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


def material_checks(r, raster):
    """Independent geometry, geometric normals and material normals.

    The full foreground slabs cover every receiver and every filter ray; the
    analytic intersection oracle must confirm ALL receivers before measuring
    retention. Both signs tilt along the light tangent, never perpendicular.
    """
    probe = TapProbe(r); rows = []
    for edge in (-1, 1):
        centre = np.array([.5+edge*.0005, .5-edge*.0005, 0.])
        tangent = S.unit(np.array([-centre[0], -centre[1], 0.]))
        across = np.cross(tangent, [0., 0., 1.])
        floor = S.Prim('empty floor', 'obb', c=(0, 0, -.02), h=(4, 4, .02))
        sphere = S.Prim('10 cm skin receiver', 'sphere', c=centre-[0, 0, .05], r=.05)
        offsets = np.linspace(-.002, .002, 17)[:, None]*across
        for name, radius, height, tilt, curved, clearance in (
                ('empty_grazing_floor', 1., .025, 25., False, None),
                ('curved_skin_empty', 1., .025, 25., True, None),
                ('thin_foreground_5mm', .1, .025, 50., False, .005),
                ('grazing_full_wall', .1, .0173, 0., False, .15),
                ('hand_over_curved_skin', .1, .025, 50., True, .005)):
            light = S.Light(name, (0, 0, height), radius, 8., seed=123)
            normals = np.tile([0., 0., 1.], (17, 1))
            if curved:
                normals = normals+offsets/.05
                normals /= np.linalg.norm(normals, axis=1, keepdims=True)
                points = np.asarray(sphere.c)+.05*normals
            else:
                points = centre+offsets
            prims = [sphere if curved else floor]
            if clearance is not None:
                direction = S.unit(centre-np.asarray(light.pos))
                slab = S.Prim('foreground slab', 'obb', c=centre-(clearance+.0025)*direction,
                              h=(.0025, 2., 2.))
                slab.rot = lambda d=direction, a=across: np.column_stack([d, a, np.cross(d, a)])
                prims += [slab]
            r.gpu_atlas = raster.atlas(scene(prims, light), light, .02)
            ray = np.asarray(light.pos)-points
            distance = np.linalg.norm(ray, axis=1); ray /= distance[:, None]
            truth = np.zeros(len(points), bool)
            # Do not intersect the receiving surface itself at t ~= 0.
            for prim in prims[1:]:
                truth |= S.isect(prim, points, ray) < distance
            assert bool(truth.all()) == (clearance is not None), name
            for sign in (-1, 0, 1):
                shade = normals*np.cos(np.radians(tilt*sign))+tangent*np.sin(np.radians(tilt*sign))
                shade /= np.linalg.norm(shade, axis=1, keepdims=True)
                for detail in (False, True):
                    for path in (0, 1, 2):
                        v = probe.run(points, normals, light, detail, path, shade)
                        ok = v.max() <= 2/255 if clearance is not None else v.min() >= 1-2/255
                        rows.append(dict(name=name, edge=edge, tilt=tilt*sign, detail=detail, path=path,
                                         minimum=float(v.min()), mean=float(v.mean()), maximum=float(v.max()),
                                         truth_blocked=int(truth.sum()), samples=len(v), passed=bool(ok)))
    return rows


def historical_checks(r):
    """Current world functions versus real 77348de6593 world functions.

    Exact receiver inputs isolate unchanged fully lit/covered geometry from
    the intentional normal reconstruction, phase and background fixes. LOOK,
    EV and detail additionally compare entire frames with frozen history.
    """
    old = Runner(G.SourceTree('77348de6593'), gl=r.gl)
    probes = [TapProbe(x) for x in (r, old)]
    rows = []
    for face in range(6):
        direction = np.array(CS.face_basis(face)[0])
        across = np.array(CS.face_basis(face)[1])
        for radius in (.1, 1.):
            light = S.Light('historical', (0,0,0), radius, 8., seed=123)
            points = 2*direction+np.linspace(-.002,.002,17)[:,None]*across
            normals = np.tile(-direction, (17,1))
            for covered in (False, True):
                prims = [S.Prim('cover', 'sphere', c=direction, r=.8)] if covered else []
                r.gpu_atlas = WG.Raster(r).atlas(scene(prims,light),light,.02)
                old.gpu_atlas = r.gpu_atlas
                for detail in (False, True):
                    for path in (0,1,2):
                        values = [p.run(points,normals,light,detail,path) for p in probes]
                        truth = 0. if covered else 1.
                        rows.append(dict(face=face,radius=radius,covered=covered,detail=detail,path=path,
                                         reference='77348de6593',samples=17,
                                         passed=bool(np.array_equal(*values) and np.all(values[0]==truth))))
    return rows


def footprint_checks(r, raster):
    """A near, broad source whose filter is wider than its search disk.

    Independent disk rays see both blocker and open source. Search coverage
    must not be mistaken for full-filter coverage and harden this penumbra.
    """
    probe=TapProbe(r);rows=[]
    for edge in (-1,1):
        direction=S.unit([1.,1.+edge*.001,.1]);across=S.unit(np.cross(direction,[0,0,1]))
        light=S.Light('near broad source',(0,0,0),1.,8.,seed=123)
        points=.7*direction+np.linspace(-.002,.002,17)[:,None]*across
        normals=np.tile(-direction,(17,1))
        sc=scene([S.Prim('partial cover','sphere',c=.35*direction,r=.08)],light)
        truth=S.trace_light(sc,light,points,normals,np.arange(17),S=256)['vis']
        r.gpu_atlas=raster.atlas(sc,light,.02)
        for detail in (False,True):
            for path in (0,2):
                value=probe.run(points,normals,light,detail,path)
                rows.append(dict(edge=edge,detail=detail,path=path,truth_mean=float(truth.mean()),
                                 minimum=float(value.min()),maximum=float(value.max()),
                                 passed=bool(np.all((truth>.5)&(truth<1.)) and value.min()>.05 and value.max()<.98)))
    return rows


class NormalRunner(Runner):
    def _texture(self, key, unit, internal, w, h, fmt, typ, data, **kwargs):
        if key == 'vis': internal, typ = 0x8814, 0x1406  # RGBA32F normal/validity readback
        return super()._texture(key,unit,internal,w,h,fmt,typ,data,**kwargs)


def normal_checks(r):
    text = r.tree.read(G.SHADERS+'/class3/deferred/areaShadowFilterF.glsl')
    # Baseline lacks the new reconstruction; this is itself a failed design
    # contract, rather than silently substituting the test's own normal code.
    target = 'area_n_geo = awGeoNormal(texel);'
    if target not in text:
        return [dict(name='full_resolution_geometric_normal',passed=False)]
    output = target+'''
        frag_color = vec4(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo),aw_plane_valid);
        return;'''
    nr = NormalRunner(WG.MutantTree(r.tree,[(target,output)]), gl=r.gl)
    light = S.Light('normal lamp',(0,0,.55),.1,8.,seed=123)
    sc = scene([S.Prim('skin','sphere',c=(.5,.5,.4),r=.05)], light)
    nr.gpu_atlas = r.gpu_atlas
    world = dict(atlas=np.empty((1088,1632),np.float32),channels=[dict(row=0,near=.02,far=8.,r_map=.1,contact=0.,seed=123)])
    rows=[]; measured=[]
    for distance in (.3,1.,4.):
        for view in ((1,1,1),(-1,-1,1),(.7,-.2,1)):
            point=np.array([.5,.5,.45])
            cam=S.Camera(point+distance*S.unit(view),point,241,181,
                         vfov=np.degrees(2*np.arctan(np.tan(np.radians(30))*181/2160)),near=.02)
            g=S.cast_primary(sc,cam)
            _,depth,normals=S.encode_gbuffer(cam,g)
            for half in (False,True):
                nr.run(cam,depth,normals,[light],dict(samples=8,steps=12,half=half,near_steps=0),denoise=0.,world=world)
                value=np.empty(4,np.float32)
                nr.gl.read(120,90,1,1,0x1908,0x1406,value.ctypes.data)
                n=value[:3]/np.linalg.norm(value[:3]);measured.append(n)
                angle=float(np.degrees(np.arccos(np.clip(n@[0,0,1],-1,1))))
                rows.append(dict(name='full_resolution_geometric_normal',distance=distance,view=view,half=half,
                                 angular_error_deg=angle,validity=float(value[3]),passed=bool(angle<=1. and value[3]>=.999)))
    spread=max(float(np.degrees(np.arccos(np.clip(a@b,-1,1)))) for a in measured for b in measured)
    rows.append(dict(name='geometric_normal_camera_spread',bound_deg=1.,spread_deg=spread,passed=spread<=1.))
    # A one-pixel receiver surrounded by sky has no usable depth tangent.
    # It must use finite view-facing geometry, never the material normal.
    depth[:]=4294967295;depth[90,120]=cam.depth24(np.array([distance]))[0]* (4294967295/16777215)
    nr.run(cam,depth,normals,[light],dict(samples=8,steps=12,half=True,near_steps=0),denoise=0.,world=world)
    value=np.empty(4,np.float32);nr.gl.read(120,90,1,1,0x1908,0x1406,value.ctypes.data)
    rows.append(dict(name='invalid_stencil_view_facing_fallback',value=value.tolist(),
                     passed=bool(np.isfinite(value).all() and value[3]==0. and np.dot(value[:3],S.unit(view))>.999)))
    # Exercise the ACTUAL visibility fallback too, independently of its normal
    # diagnostic. Neither a foreground cover nor background rejection may be
    # lost just because this tiny receiver has no valid depth neighbours.
    direction=S.unit(point-np.asarray(light.pos))
    r.gl.use(r.filter);r.gl.ui(r.gl.loc(r.filter,b'area_world_debug'),1)
    for placement,centre in (('front',point-.2*direction),('thin_front',point-.125*direction),('behind',point+.2*direction)):
        caster=S.Prim(placement,'sphere',c=centre,r=.12)
        r.gpu_atlas=WG.Raster(r).atlas(scene([caster],light),light,.02)
        value=r.run(cam,depth,normals,[light],dict(samples=8,steps=12,half=True,near_steps=0),denoise=0.,world=world)[0,90,120]
        rows.append(dict(name='invalid_stencil_'+placement,visibility=float(value),
                         passed=bool(value>=1-2/255 if placement=='behind' else value<=2/255)))
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--shader-ref', default='WORKTREE')
    ap.add_argument('--out', default='.tmp/world_seethru/receiver')
    ap.add_argument('--controls', action='store_true', help='also require independent old-normal and unbounded-plane mutants to fail')
    ap.add_argument('--material-only', action='store_true', help='run the round-two point regressions only')
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
    camera = [] if args.material_only else [row for elevation in (.455,.55) for radius in (.1,1.)
              for row in camera_checks(r, raster, elevation, radius)]
    depths = [] if args.material_only else depth_checks(r, raster)
    material = material_checks(r, raster)
    history = historical_checks(r)
    normals = normal_checks(r)
    footprints = footprint_checks(r,raster)
    result = dict(shader_ref=args.shader_ref, renderer=r.gl.renderer, camera=camera,
                  depth=depths, material=material, historical=history, normals=normals,
                  footprints=footprints,passed=all(x['passed'] for x in camera+depths+material+history+normals+footprints if 'passed' in x))
    if args.controls:
        normal_tree = WG.MutantTree(r.tree, [('dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo)',
                                            'dot(area_world_rot[0],area_n_shade),dot(area_world_rot[1],area_n_shade),dot(area_world_rot[2],area_n_shade)')])
        mutant = Runner(normal_tree, gl=r.gl)
        normal_rows = material_checks(mutant, WG.Raster(mutant))
        plane_tree = WG.MutantTree(r.tree, [('return vec4(lessThan((z+bias)*max(vec4(den+du,den+dr+du,den+dr,den),vec4(1.e-5)),vec4(receiver2)));',
                                           'return vec4(1.0);')])
        mutant = Runner(plane_tree, gl=r.gl)
        plane_rows = depth_checks(mutant, WG.Raster(mutant))
        retention_tree = WG.MutantTree(r.tree, [('float visibility = min(lit/float(AW_FILTER_TAPS),smoothstep(0.0,1.0/float(AW_FILTER_TAPS),front_lit));',
                                               'float visibility = lit/float(AW_FILTER_TAPS);')])
        mutant = Runner(retention_tree, gl=r.gl)
        retention_rows = material_checks(mutant, WG.Raster(mutant))
        search_tree = WG.MutantTree(r.tree, [('float radius = max(r_map*max(dist-z_b,0.0)/(z_b*dist),2.0/AW_USABLE);',
                                            'if (blocker_n >= float(search_taps)*4.0-0.01) return 0.0;\n    float radius = max(r_map*max(dist-z_b,0.0)/(z_b*dist),2.0/AW_USABLE);')])
        mutant = Runner(search_tree, gl=r.gl)
        search_rows = footprint_checks(mutant,WG.Raster(mutant))
        controls = dict(material_normal_rejected=any(not x['passed'] for x in normal_rows if 'passed' in x),
                        unbounded_face_rejected=any(not x['passed'] for x in plane_rows if x['path']==1 and not x['detail'] and x['placement']=='behind'),
                        unbounded_detail_rejected=any(not x['passed'] for x in plane_rows if x['path']==1 and x['detail'] and x['placement']=='behind'),
                        unbounded_direction_rejected=any(not x['passed'] for x in plane_rows if x['path']==2 and x['placement']=='behind'),
                        lost_foreground_cover_rejected=any(not x['passed'] for x in retention_rows if x['name']=='grazing_full_wall' and x['path']==1),
                        search_coverage_shortcut_rejected=any(not x['passed'] for x in search_rows))
        result.update(controls=controls, passed=result['passed'] and all(controls.values()))
        print('[receiver] must-fail controls', controls, flush=True)
    (out/'results.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('[receiver]', args.shader_ref, 'camera', [x for x in camera if 'spread' in x], flush=True)
    print('[receiver] depth failures', [x for x in depths if not x['passed']], flush=True)
    print('[receiver] material failures', [x for x in material if not x['passed']], flush=True)
    print('[receiver] historical failures', [x for x in history if not x['passed']], flush=True)
    print('[receiver] filter footprint', footprints, flush=True)
    print('[receiver] normal diagnostics', normals, flush=True)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
