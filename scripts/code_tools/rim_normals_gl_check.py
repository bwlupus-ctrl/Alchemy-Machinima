"""Real-WGL rim normal regressions. No viewer build or external shader inputs.

Runs production aux, smoothing, debug and rim-term GLSL against rasterized
low-poly curved geometry, interpolated normals, a normal map, and silhouettes.
--shader-ref supports the pre-fix commit; --measure-only records failing A/B
measurements without changing any acceptance threshold.
"""
import argparse
import ctypes as C
import json
from pathlib import Path

import numpy as np
from PIL import Image

import relight_3d_gl as G
from rim_alpha_gl_check import Runner as WrapRunner

BASE = 'b0d3b1ac2d0'
PATH = G.SHADERS + '/class1/deferred/'


def unit(n):
    return n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1.e-12)


def angle(a, b):
    # atan2 remains accurate near zero, unlike acos on fp32 unit vectors.
    return np.degrees(np.arctan2(np.linalg.norm(np.cross(a, b), axis=-1),
                                 np.sum(a*b, axis=-1)))


def oct_decode(e):
    e=e*2-1
    n=np.concatenate([e,1-np.sum(np.abs(e),axis=-1,keepdims=True)],-1)
    t=np.clip(-n[...,2:3],0,1)
    n[...,:2]+=np.where(n[...,:2]>=0,-t,t)
    return unit(n)


def projection(w, h, zoom=1):
    f, near, far = 1.8*zoom, .1, 100.
    return np.array([[f*h/w, 0, 0, 0], [0, f, 0, 0],
                     [0, 0, -(far+near)/(far-near), -2*far*near/(far-near)],
                     [0, 0, -1, 0]], np.float32)


def encode(n):
    n=n/np.sum(np.abs(n),axis=-1,keepdims=True)
    xy=np.where(n[...,2:3]>=0,n[...,:2],(1-np.abs(n[...,1::-1]))*np.where(n[...,:2]>=0,1,-1))
    return np.concatenate([xy*.5+.5,np.zeros(n.shape[:-1]+(2,))],-1).astype(np.float32)


def plane_fixture(w,h,shift,distance,zoom):
    """A grazing, normal-mapped floor: its rim MUST be visibly excited."""
    proj=projection(w,h,zoom)
    yy,xx=np.mgrid[:h,:w]
    rays=np.stack([((xx+.5)/w*2-1)/proj[0,0],((yy+.5)/h*2-1)/proj[1,1],-np.ones((h,w))],-1)
    geo=unit(np.array([0.,.99,.14])); tangent=np.array([1.,0.,0.]); bit=np.cross(geo,tangent)
    t=(-distance*geo[2])/np.minimum(np.sum(rays*geo,axis=-1),-1.e-8)
    p=rays*t[...,None]
    body=(np.abs(p[...,0]-shift)<.55)&(np.abs(p[...,1])<.7)&(np.sum(rays*geo,axis=-1)<-.03)
    n=unit(geo+.18*np.sin((p[...,0:1]-shift)*120)*tangent+.13*np.sin(p[...,1:2]*100)*bit)
    p=np.where(body[...,None],p,rays*12)
    n=np.where(body[...,None],n,unit(np.array([-.55,.2,1.])))
    clip=np.concatenate([p,np.ones((h,w,1))],-1)@proj.T
    return dict(depth=(clip[...,2]/clip[...,3]*.5+.5).astype(np.float32),normal=encode(n),
                ids=np.where(body,0,-1),positions=p,shading=n,proj=proj,w=w,h=h)


def join_fixture(w=384,h=384,degrees=6,gap=0,mapped=True):
    f=plane_fixture(w,h,0,2.5,1); proj=f['proj']
    yy,xx=np.mgrid[:h,:w]
    rays=np.stack([((xx+.5)/w*2-1)/proj[0,0],((yy+.5)/h*2-1)/proj[1,1],-np.ones((h,w))],-1)
    side=xx<w//2
    theta=np.radians(degrees/2)
    geo=np.stack([np.where(side,np.sin(theta),-np.sin(theta)),np.zeros((h,w)),np.full((h,w),np.cos(theta))],-1)
    p=rays*((-2.5*geo[...,2]-np.where(side,0,gap))/np.sum(rays*geo,axis=-1))[...,None]
    normal=geo.copy()
    if mapped: normal=unit(geo+np.sin(p[...,1:2]*100)*np.array([0,.16,0]))
    clip=np.concatenate([p,np.ones((h,w,1))],-1)@proj.T
    f.update(depth=(clip[...,2]/clip[...,3]*.5+.5).astype(np.float32),normal=encode(normal),
             shading=normal,positions=p,ids=side.astype(int))
    return f


def fixture(w=384, h=384, shift=0, smooth=False, mapped=False, distance=2.5, zoom=1):
    """Perspective-correct rasterized 12 x 8 faceted sphere / torso proxy.

    Background is actual far geometry with a deliberately different normal,
    not sky. Primitive IDs and world positions are independent fixture data.
    """
    if mapped is True:
        return plane_fixture(w,h,shift,distance,zoom)
    proj = projection(w, h, zoom)
    yy, xx = np.mgrid[:h, :w]
    rays = np.stack([((xx+.5)/w*2-1)/proj[0, 0],
                     ((yy+.5)/h*2-1)/proj[1, 1], -np.ones((h, w))], -1)
    positions = rays*12
    normal = np.zeros((h, w, 3), np.float64)
    normal[:] = unit(np.array([-.55, .2, 1.]))
    ids = np.full((h, w), -1)
    vertices, vertex_n = [], []
    for lat in range(9):
        theta = np.pi*lat/8
        for lon in range(12):
            phi = 2*np.pi*(lon+.17)/12
            n = np.array([np.sin(theta)*np.cos(phi), np.cos(theta), np.sin(theta)*np.sin(phi)])
            p = n*np.array([.55, .7, .55]) + [shift, 0, -distance]
            vertices.append(p)
            vertex_n.append(unit(n/np.array([.55, .7, .55])))
    vertices, vertex_n = np.array(vertices), np.array(vertex_n)
    faces = []
    for lat in range(8):
        for lon in range(12):
            a, b = lat*12+lon, lat*12+(lon+1)%12
            faces += [(a, b, a+12), (b, b+12, a+12)]
    for ident, face in enumerate(faces):
        p = vertices[list(face)]
        geo = np.cross(p[1]-p[0], p[2]-p[0])
        if np.linalg.norm(geo) < 1.e-10:
            continue
        geo = unit(geo)
        if np.dot(geo, -p.mean(0)) < 0:
            geo = -geo
        clip = np.c_[p, np.ones(3)] @ proj.T
        xy = (clip[:, :2]/clip[:, 3:]*.5+.5)*[w, h] - .5
        x0, y0 = np.maximum(np.floor(xy.min(0)).astype(int), 0)
        x1, y1 = np.minimum(np.ceil(xy.max(0)).astype(int)+1, [w, h])
        if x1 <= x0 or y1 <= y0:
            continue
        y, x = np.mgrid[y0:y1, x0:x1]
        mat = np.r_[xy.T, np.ones((1, 3))]
        if abs(np.linalg.det(mat)) < 1.e-8:
            continue
        bary = np.stack([x, y, np.ones_like(x)], -1) @ np.linalg.inv(mat).T
        inside = np.all(bary >= -1.e-8, -1)
        weights = bary / clip[:, 3]
        weights /= weights.sum(-1, keepdims=True)
        hit = weights @ p
        visible = inside & (hit[..., 2] > positions[y0:y1, x0:x1, 2])
        positions[y0:y1, x0:x1][visible] = hit[visible]
        ns = unit(weights @ vertex_n[list(face)]) if smooth else np.broadcast_to(geo, hit.shape)
        if mapped:
            # Continuous surface-space normal map, with no screen-space phase.
            t = unit(np.cross(np.broadcast_to([0., 1., 0.], ns.shape), ns))
            bit = np.cross(ns, t)
            ns = unit(ns + .18*np.sin((hit[..., :1]-shift)*120)*t + .13*np.sin(hit[..., 1:2]*100)*bit)
        normal[y0:y1, x0:x1][visible] = ns[visible]
        ids[y0:y1, x0:x1][visible] = ident
    clip = np.concatenate([positions, np.ones((h, w, 1))], -1) @ proj.T
    depth = (clip[..., 2]/clip[..., 3]*.5+.5).astype(np.float32)
    # Match production globalF.glsl encodeNormal (octahedral RGBA16).
    oct_n = normal / np.sum(np.abs(normal),axis=-1,keepdims=True)
    xy = np.where(oct_n[...,2:3]>=0,oct_n[...,:2],
                  (1-np.abs(oct_n[...,1::-1]))*np.where(oct_n[...,:2]>=0,1,-1))
    encoded = np.concatenate([xy*.5+.5, np.zeros((h,w,2))],-1).astype(np.float32)
    return dict(depth=depth, normal=encoded, ids=ids, positions=positions,
                shading=normal, proj=proj, w=w, h=h)


class Runner(G.AreaRunner):
    link_pass = WrapRunner.link_pass
    uniform = WrapRunner.uniform
    sampler = WrapRunner.sampler
    target = WrapRunner.target
    pixels = WrapRunner.pixels

    def __init__(self, tree, gl=None):
        self.tree, self.gl, self.textures = tree, gl or G.GL(), {}
        gl = self.gl
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0):
            gl.disable(cap)
        self.fbo, self.vbo = gl.U(), gl.U()
        gl.gen_fbo(1, C.byref(self.fbo)); gl.gen_buf(1, C.byref(self.vbo))
        gl.bind_buf(0x8892, self.vbo)
        verts = np.array([-1, -1, 0, 3, -1, 0, -1, 3, 0], np.float32)
        gl.buffer_data(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
        self.has_smooth = tree.exists(PATH+'rigRimSmoothF.glsl')
        self.aux = self.link_pass('rigRimAuxF.glsl', common=True)
        self.guide = self.link_pass('rigRimAuxF.glsl', defines='#define RIG_RIM_SMOOTH_GUIDE 1\n', common=True) if self.has_smooth else self.aux
        self.debug = self.link_pass('rigRimDebugF.glsl', common=True)
        self.hierarchical = 'RIG_RIM_BARRIER_EDGES' in tree.read(PATH+'rigRimSmoothF.glsl') if self.has_smooth else False
        self.seed = self.link_pass('rigRimSilSeedF.glsl', common=True)
        if self.has_smooth:
            self.filters = [self.link_pass('rigRimSmoothF.glsl', defines=d, common=True) for d in
                            (('', '#define RIG_RIM_SMOOTH_VERTICAL 1\n') if not self.hierarchical else
                             tuple('#define '+x+' 1\n' if x else '' for x in ('RIG_RIM_SMOOTH_DOWNSAMPLE','RIG_RIM_BARRIER_INIT','RIG_RIM_BARRIER_EDGES','','RIG_RIM_SMOOTH_RESOLVE')))]
        util = tree.read(PATH+'deferredUtil.glsl')
        rim = util[util.index('uniform int  rig_rim_mode;'):util.index('// [RigRim] Forward-alpha punctual light WITH')]
        self.term = self.link_pass(None, source=rim+'''
in vec2 vary_fragcoord;
uniform sampler2D normalMap;
uniform sampler2D depthMap;
uniform mat4 inv_proj;
uniform float rim_test_legacy;
out vec4 frag_color;
void main() {
    vec3 n = rigRimOctDecode(texture(normalMap,vary_fragcoord).xy);
    RigRimPixel px = rigRimLoadPixel(vary_fragcoord,n,2.5);
    vec4 p = inv_proj*vec4(vary_fragcoord*2.0-1.0,texture(depthMap,vary_fragcoord).r*2.0-1.0,1.0);
    vec3 c;
    if (rim_test_legacy>.5)
        c=rigRimTermEx(n,vec3(0,0,1),normalize(vec3(1,0,-1)),vec3(1),vec3(.7),.2,vec4(1,8,.35,0),px);
    else
        c=rigRimTermEx(n,normalize(-p.xyz/p.w),normalize(vec3(1,0,1)),vec3(1),vec3(.7),.2,vec4(1,3,.8,-1),px);
    frag_color = vec4(c,1);
}
''')

    def capture_program(self):
        """Production display encoding, before tracked capture samples."""
        if not hasattr(self,'capture_shader'):
            srgb=self.tree.read(G.SHADERS+'/class1/environment/srgbF.glsl')
            begin=srgb.index('vec3 linear_to_srgb(')
            function=srgb[begin:srgb.index('\n}',begin)+2]
            self.capture_shader=self.link_pass(None,source=function+'''
in vec2 vary_fragcoord;
uniform sampler2D rimTestLinear;
out vec4 frag_color;
void main(){frag_color=vec4(linear_to_srgb(texture(rimTestLinear,vary_fragcoord).rgb),1);}
''')
        return self.capture_shader

    def frame(self, f, smoothness=1, radius=12, detail=0, read=True, stats=None, legacy=False, capture=False):
        gl, w, h = self.gl, f['w'], f['h']
        depth = self._texture('depth', 0, 0x822E, w, h, 0x1903, 0x1406, f['depth'])
        identity=self._texture('identity',7,0x8229,w,h,0x1903,0x1401,f['identity']) if 'identity' in f else None
        norm = self._texture('norm', 1, 0x805B, w, h, 0x1908, 0x1406, f['normal'])
        # Production render targets persist between frames. Do not orphan their
        # storage before each timer query: first-write allocation costs are not
        # steady-state shader cost.
        def output(k,fmt,ow=w,oh=h):
            previous=self.textures.get(k)
            if previous is not None and previous[1:]==(ow,oh,fmt):
                return previous[0]
            return self._texture(k,6,fmt,ow,oh,0x1908,0x1406,None)
        targets = {k: output(k,fmt) for k, fmt in
                   [('raw', 0x805B), ('horizontal', 0x8814), ('filtered', 0x805B),
                    ('debug', 0x8814), ('term', 0x8814)]}
        def setup(pr, target,ow=w,oh=h):
            self.target(targets[target], ow, oh); self._begin(pr)
            self.sampler(pr, 'depthMap', depth, 0)
            self.sampler(pr, 'normalMap', norm, 1)
            self.uniform(pr, 'screen_res', w, h)
            self._mat(pr, b'inv_proj', np.linalg.inv(f['proj']))
            self.uniform(pr, 'rig_rim_quality', smoothness, 1, 0, 0)
        aux_program = self.guide if smoothness > 0 else self.aux
        setup(aux_program, 'raw')
        self.uniform(aux_program, 'rig_rim_aux_params', 0, .02, .5, 0)
        self._timed('aux_ms', stats, lambda: gl.draw(4, 0, 3))
        final = targets['raw']
        if self.has_smooth and smoothness > 0:
            if self.hierarchical:
                grid_size=1 if h<=512 else (2 if h<1024 or radius<=2 else 4)
                qw,qh=(w+grid_size-1)//grid_size,(h+grid_size-1)//grid_size
                for key,fmt in (('small0',0x8814),('small1',0x8814),('bar0',0x881A),('bar1',0x881A)):
                    targets[key]=output(key,fmt,qw,qh)
                targets['edges']=output('edges',0x822B)
                def draw(index,target,source='small0',bar='bar0',step=1,axis=0):
                    pr=self.filters[index]
                    ow,oh=(w,h) if index in (2,4) else (qw,qh)
                    setup(pr,target,ow,oh)
                    self.sampler(pr,'rigRimAuxMap',targets['raw'],2)
                    self.sampler(pr,'rigRimSmoothMap',targets[source],3)
                    self.sampler(pr,'rigRimBarrierMap',targets[bar],4)
                    self.sampler(pr,'rigRimEdgeMap',targets['edges'],5)
                    self.uniform(pr,'rig_rim_smooth_stage',step,axis,grid_size,1 if identity is not None else 0)
                    if identity is not None: self.sampler(pr,'actorMaskMap',identity,7)
                    self.uniform(pr,'rig_rim_smooth_params',radius*.01,w*f['proj'][0,0]*.5,h*f['proj'][1,1]*.5,detail)
                    self._timed('stage_%d_ms'%index,stats,lambda: gl.draw(4,0,3))
                draw(2,'edges')
                draw(1,'bar0',bar='edges')
                draw(0,'small0')
                bar='bar0'
                source='small0'
                for axis in (0,1):
                    dst='small1' if source=='small0' else 'small0'
                    draw(3,dst,source,bar,1,axis)
                    source=dst
                draw(4,'filtered',source,bar)
                self.barrier_texture=targets[bar]
            else:
                for i, pr in enumerate(self.filters):
                    setup(pr, 'horizontal' if i == 0 else 'filtered')
                    self.sampler(pr, 'rigRimAuxMap', targets['raw'], 2)
                    if i:
                        self.sampler(pr, 'rigRimSmoothMap', targets['horizontal'], 3)
                    self.uniform(pr, 'rig_rim_smooth_params', radius*.01,
                                 w*f['proj'][0,0]*.5, h*f['proj'][1,1]*.5, detail)
                    self._timed('smooth_ms', stats, lambda: gl.draw(4, 0, 3))
            if stats is not None and self.hierarchical:
                stats['smooth_ms']=sum(value for key,value in stats.items() if key.startswith('stage_'))
            final = targets['filtered']
        self.geometry_texture=targets['raw']
        self.rim_texture=final
        setup(self.debug, 'debug')
        self.sampler(self.debug, 'rigRimAuxMap', final, 2)
        self.uniform(self.debug, 'rig_rim_aux_params', 0, .02, .5, 3)
        gl.draw(4, 0, 3)
        if not read:
            return
        debug = self.pixels(w, h)
        setup(self.term, 'term')
        self.sampler(self.term, 'rigRimAuxMap', final, 2)
        self.uniform(self.term,'rim_test_legacy',float(legacy))
        self.uniform(self.term, 'rig_rim_globals', 1, .5, .5 if legacy else .01, .25)
        gl.draw(4, 0, 3)
        term = self.pixels(w, h)
        if capture:
            targets['capture']=output('capture',0x8814)
            pr=self.capture_program()
            setup(pr,'capture')
            self.sampler(pr,'rimTestLinear',targets['term'],2)
            gl.draw(4,0,3)
            display=self.pixels(w,h)
        self.target(final, w, h)
        aux = self.pixels(w, h)
        result=(debug[..., :3]*2-1, term, aux)
        return result+(display,) if capture else result


def faceting(n, f):
    values = []
    for axis in (0, 1):
        other = np.roll(n, 1, axis)
        ids = np.roll(f['ids'], 1, axis)
        mask = (ids >= 0) & (f['ids'] >= 0) & (ids != f['ids'])
        # Ignore the highly foreshortened outline, where a pixel spans a face.
        mask &= np.sum(f['shading']*unit(-f['positions']),axis=-1) > .55
        values.extend(angle(n, other)[mask])
    return float(np.mean(values))


def bilinear(a, xy):
    h, w = a.shape[:2]
    x, y = np.clip(xy[:,0], 0, w-1.001), np.clip(xy[:,1], 0, h-1.001)
    ix, iy = x.astype(int), y.astype(int)
    fx, fy = (x-ix)[:,None], (y-iy)[:,None]
    return (a[iy,ix]*(1-fx)+a[iy,ix+1]*fx)*(1-fy)+(a[iy+1,ix]*(1-fx)+a[iy+1,ix+1]*fx)*fy


def crawl(runner, frames=20, mapped=False, detail=0, w=384, h=384, distance=2.5, phase=0, camera=False, legacy=False):
    initial=phase*distance/(h*.9)
    first = fixture(w,h,shift=initial,mapped=mapped,distance=distance)
    # Reprojection is defined only for visible surface footprints. Exclude
    # silhouette coverage/disocclusion pixels, measured separately by no-bleed.
    visible = first['ids'] >= 0
    for axis in (0,1):
        for offset in range(-4,5):
            visible &= np.roll(first['ids'],offset,axis) >= 0
    # Judge normal reconstruction on front-facing patches, but include ALL
    # visible grazing patches in the lighting/crawl metric.
    rim_y,rim_x=np.where(visible)
    select=np.arange(0,len(rim_x),max(1,len(rim_x)//8192))
    rim_y,rim_x=rim_y[select],rim_x[select]
    rim_p=first['positions'][rim_y,rim_x]
    y, x = np.where(visible & ((np.sum(first['shading']*unit(-first['positions']),axis=-1) > .55) if mapped is not True else True))
    select=np.arange(0,len(x),max(1,len(x)//8192))
    y,x=y[select],x[select]
    p = first['positions'][y,x]
    samples, rim_samples, display_samples = [], [], []
    for i in range(frames):
        shift = initial+(-1 if camera else 1)*i*.00035  # ~0.05 pixels/frame, no frame-dependent shader state
        f = fixture(w,h,shift=shift, mapped=mapped,distance=distance)
        n, rim, _, display = runner.frame(f,detail=detail,legacy=legacy,capture=True)
        clip = np.c_[p+[shift-initial,0,0], np.ones(len(p))] @ f['proj'].T
        xy = (clip[:,:2]/clip[:,3:]*.5+.5)*[f['w'],f['h']]-.5
        samples.append(unit(bilinear(n,xy)))
        rim_clip=np.c_[rim_p+[shift-initial,0,0],np.ones(len(rim_p))]@f['proj'].T
        rim_xy=(rim_clip[:,:2]/rim_clip[:,3:]*.5+.5)*[f['w'],f['h']]-.5
        rim_samples.append(bilinear(rim,rim_xy))
        display_samples.append(bilinear(display,rim_xy))
    delta = angle(np.array(samples[1:]),np.array(samples[:-1]))
    light=np.array(rim_samples)[...,:3]
    excitation=float(np.sqrt(np.mean(light**2)))
    # Capture-card output is display encoded. Keep the original linear stress
    # metric too; a relative capture threshold must use the captured domain.
    display=np.array(display_samples)[...,:3]
    display_signal=float(np.sqrt(np.mean(display**2)))
    display_delta=float(np.sqrt(np.mean(np.diff(display,axis=0)**2)))
    return dict(normal_rms_deg=float(np.sqrt(np.mean(delta**2))),
                normal_p99_deg=float(np.percentile(delta,99)),
                rim_rms=float(np.sqrt(np.mean(np.diff(light,axis=0)**2))),
                rim_signal_rms=excitation, rim_display_relative_rms=display_delta/max(display_signal,1.e-8), rim_relative_rms=float(np.sqrt(np.mean(np.diff(light,axis=0)**2)))/max(excitation,1.e-8))


def background_only(f):
    other={k:v.copy() if isinstance(v,np.ndarray) else v for k,v in f.items()}
    mask=f['ids']<0
    other['depth'][:]=f['depth'][mask][0]
    other['normal'][:]=f['normal'][mask][0]
    return other


def radius_measure(runner,w,distance,zoom):
    f=plane_fixture(w,w,0,distance,zoom)
    # Small normal-map bump in world coordinates on a flat, uninterrupted plane.
    f['depth'][:]=f['depth'][w//2,w//2]
    p=f['positions']; yy,xx=np.mgrid[:w,:w]
    x=((xx+.5)/w*2-1)*distance/f['proj'][0,0]
    y=((yy+.5)/w*2-1)*distance/f['proj'][1,1]
    bump=.3*np.exp(-(x*x+y*y)/(.012**2))
    normal=unit(np.stack([bump,np.zeros_like(bump),np.ones_like(bump)],-1))
    enc=normal/np.sum(np.abs(normal),axis=-1,keepdims=True)
    f['normal'][...,:2]=enc[...,:2]*.5+.5
    n,_,_=runner.frame(f,radius=5,detail=0)
    weight=np.maximum(n[...,0],0)
    # Remove octahedral quantization's constant neutral-normal floor.
    weight=np.maximum(weight-np.median(weight),0)
    return float(np.sqrt(np.sum(weight*x*x)/max(np.sum(weight),1.e-12)))


def radius_profile(runner,height,distance,radius):
    w=32; h=height; proj=projection(w,h)
    yy,xx=np.mgrid[:h,:w]
    rays=np.stack([((xx+.5)/w*2-1)/proj[0,0],((yy+.5)/h*2-1)/proj[1,1],-np.ones((h,w))],-1)
    p=rays*distance
    n=unit(np.stack([.3*np.exp(-p[...,1]**2/.012**2),np.zeros((h,w)),np.ones((h,w))],-1))
    clip=np.concatenate([p,np.ones((h,w,1))],-1)@proj.T
    f=dict(w=w,h=h,proj=proj,positions=p,shading=n,normal=encode(n),ids=np.zeros((h,w),int),
           depth=(clip[...,2]/clip[...,3]*.5+.5).astype(np.float32))
    got,_,_=runner.frame(f,radius=radius)
    weights=np.maximum(got[:,w//2,0]-np.mean(got[np.r_[:8,h-8:h],w//2,0]),0)
    sigma=np.sqrt(np.sum(weights*p[:,w//2,1]**2)/max(weights.sum(),1.e-12))
    expected=np.sqrt(.012**2/2+(radius*.01)**2/7)
    return dict(sigma_m=float(sigma),expected_m=float(expected),ratio=float(sigma/expected))


def tiled_snapshot(runner,f,margin):
    w,h=f['w'],f['h']; out=np.empty((h,w,3),np.float32)
    for core_start,core_end in ((0,w//2),(w//2,w)):
        start=max(0,core_start-margin);end=min(w,core_end+margin);tw=end-start
        tile={k:(v[:,start:end].copy() if isinstance(v,np.ndarray) and v.shape[:2]==(h,w) else v) for k,v in f.items()}
        proj=f['proj'].copy()
        proj[0]=w/tw*f['proj'][0]+(w-2*start-tw)/tw*f['proj'][3]
        tile.update(w=tw,proj=proj)
        n,_,_=runner.frame(tile)
        out[:,core_start:core_end]=n[:,core_start-start:core_end-start]
    return out


def expanded_snapshot(runner,f,margin):
    """Four expanded render targets: actual camera scale/offset, then crop.

    Outside-frame pixels extend the fixture's uninterrupted background plane.
    The gutter is never clipped away, so this exercises a different projection
    and target size, including the reduced-grid phase, at every tile.
    """
    w,h=f['w'],f['h'];cw,ch=w//2,h//2
    rw,rh=cw+2*margin,ch+2*margin
    out=np.empty((h,w,3),np.float32)
    for ty in range(2):
        for tx in range(2):
            ox,oy=tx*cw-margin,ty*ch-margin
            x0,y0=max(0,ox),max(0,oy);x1,y1=min(w,ox+rw),min(h,oy+rh)
            padding=((y0-oy,oy+rh-y1),(x0-ox,ox+rw-x1))
            proj=f['proj'].copy()
            proj[0]=w/rw*f['proj'][0]+(w-2*ox-rw)/rw*f['proj'][3]
            proj[1]=h/rh*f['proj'][1]+(h-2*oy-rh)/rh*f['proj'][3]
            tile=dict(w=rw,h=rh,proj=proj,
                depth=np.pad(f['depth'][y0:y1,x0:x1],padding,mode='edge'),
                normal=np.pad(f['normal'][y0:y1,x0:x1],padding+((0,0),),mode='edge'))
            n,_,_=runner.frame(tile)
            out[ty*ch:(ty+1)*ch,tx*cw:(tx+1)*cw]=n[margin:margin+ch,margin:margin+cw]
    return out


def review_checks(runner,require,result,out,label,four_k=True):
    f=join_fixture()
    pure,_,_=runner.frame(f,detail=0); detailed,_,_=runner.frame(f,detail=1)
    band=np.s_[32:-32,f['w']//2-1:f['w']//2+1]
    result['low_angle_texture_jump_deg']=float(np.mean(angle(detailed[band][:,0],detailed[band][:,1])))
    result['low_angle_pure_jump_deg']=float(np.mean(angle(pure[band][:,0],pure[band][:,1])))
    require(result['low_angle_texture_jump_deg']<.6,'six-degree mapped join: Texture=1 adds detail without restoring facets (< .6 deg)')
    require(float(np.sqrt(np.mean(angle(pure,detailed)**2)))>3,'detail transfer is excited independently of geometry steps')
    bare=join_fixture(mapped=False)
    bare_pure,_,_=runner.frame(bare,detail=0)
    bare_detail,_,_=runner.frame(bare,detail=1)
    require(float(np.max(angle(bare_pure,bare_detail)))<.1,
            'Texture=1 transfers no detail from a bare geometry step (< .1 degree)')
    close=join_fixture(degrees=30,gap=.004,mapped=False)
    got,_,_=runner.frame(close)
    right=np.s_[32:-32,close['w']//2+1:close['w']//2+4]
    result['four_mm_bleed_deg']=float(np.max(angle(got[right],close['shading'][right])))
    require(result['four_mm_bleed_deg']<.1,'4 mm separated surfaces: edge error < .1 degree')
    actor=join_fixture(degrees=0,mapped=False)
    normal=np.empty_like(actor['shading']); normal[:,:192]=unit(np.array([.3,0,1.])); normal[:,192:]=unit(np.array([-.3,0,1.]))
    actor.update(normal=encode(normal),shading=normal,identity=np.where(np.indices((384,384))[1]<192,1,2).astype(np.uint8))
    got,_,_=runner.frame(actor)
    result['actor_identity_error_deg']=float(np.max(angle(got[32:-32,194:198],normal[32:-32,194:198])))
    require(result['actor_identity_error_deg']<.1,'available Actor Mask identity prevents coplanar surface transfer (< .1 deg)')
    floor=plane_fixture(384,384,0,2.5,1)
    runner.frame(floor,smoothness=.001,detail=1)
    pr=runner.seed; runner._begin(pr)
    runner.sampler(pr,'depthMap',runner.textures['depth'][0],0)
    runner.sampler(pr,'normalMap',runner.textures['norm'][0],1)
    # Match the production host's geometric/shading choice, not a test-only override.
    source=runner.geometry_texture if runner.hierarchical else runner.rim_texture
    runner.sampler(pr,'rigRimAuxMap',source,2)
    runner.uniform(pr,'screen_res',384,384); runner._mat(pr,b'inv_proj',np.linalg.inv(floor['proj']))
    runner.uniform(pr,'rig_rim_aux_params',0,.002,.02,0)
    runner.uniform(pr,'rig_rim_quality',.001,0,0,0)
    tex=runner._texture('seed',6,0x8814,192,192,0x1908,0x1406,None)
    runner.target(tex,192,192); runner.gl.draw(4,0,3); seeds=runner.pixels(192,192)[...,3]<.5
    mask=floor['ids'][1::2,1::2]>=0
    for axis in (0,1):
        for offset in range(-4,5): mask &= np.roll(floor['ids'][1::2,1::2],offset,axis)>=0
    result['false_interior_seeds']=int(np.count_nonzero(seeds&mask)); result['seed_interior_samples']=int(mask.sum())
    require(result['false_interior_seeds']==0 and mask.sum()>100,'mapped floor uses geometric guide: zero interior silhouette seeds')
    # The host, camera projection and readback crop must all participate.
    pipe=runner.tree.read('indra/newview/llviewerwindow.cpp')
    import re
    match=re.search(r'constexpr S32 support = (\d+);',pipe)
    margin=int(match.group(1)) if match else 0
    f=fixture(512,384)
    whole,_,_=runner.frame(f); tiled=tiled_snapshot(runner,f,margin)
    result['snapshot_seam_error_deg']=float(np.max(angle(whole[:,252:260],tiled[:,252:260])))
    require(result['snapshot_seam_error_deg']<.03,'overlapping snapshot tiles match the uninterrupted normal field (< .03 deg)')
    if margin:
        require(margin>=1048 and 'subimage_x_offset + rim_tile_margin' in pipe and
                'out_y + subimage_y_offset + rim_tile_margin' in pipe and 'setSnapshotTile(scale_factor*sx' in pipe,
                'snapshot overlap covers maximum filter, guide, upsample and derivative support, then crops both axes')
    expanded_fixture=fixture(1024,1024)
    reference,_,_=runner.frame(expanded_fixture)
    assembled=expanded_snapshot(runner,expanded_fixture,margin)
    delta=angle(reference,assembled)
    seams=np.zeros((1024,1024),bool);seams[:,508:516]=True;seams[508:516,:]=True
    result['expanded_snapshot_rms_deg']=float(np.sqrt(np.mean(delta[seams]**2)))
    result['expanded_snapshot_max_deg']=float(np.max(delta[seams]))
    require(result['expanded_snapshot_rms_deg']<.03 and result['expanded_snapshot_max_deg']<.1,
            'four expanded snapshot targets preserve both seam axes (RMS < .03 / max < .1 deg quantization budget)')
    Image.fromarray(np.uint8(np.clip((assembled[::-1]*.5+.5)*255,0,255))).save(out/(label+'_tiled_debug.png'))
    # A real black shader is an adversarial control for the lighting gate.
    live_term=runner.term
    runner.term=runner.link_pass(None,source='in vec2 vary_fragcoord; out vec4 frag_color; void main(){frag_color=vec4(0,0,0,1);}')
    _,black,_=runner.frame(floor,detail=1)
    runner.term=live_term
    black_signal=float(np.sqrt(np.mean(black[...,:3]**2)))
    require(black_signal==0 and not black_signal>.002,'real all-black rim is rejected by the excitation gate')
    _,excited,_=runner.frame(floor,detail=1)
    result['mapped_floor_rim_signal']=float(np.sqrt(np.mean(excited[...,:3][floor['ids']>=0]**2)))
    require(result['mapped_floor_rim_signal']>.002,'mapped floor has meaningful rim excitation (> .002 RMS)')
    result['radius_profiles']={}
    for height in (1080,2160):
        for distance in (1.,3.):
            for radius in (1.,12.,50.):
                profile=radius_profile(runner,height,distance,radius)
                result['radius_profiles']['%s/%s/%s'%(height,distance,radius)]=profile
                require(.88<profile['ratio']<1.12,'requested %.0f cm radius at %sp / %.0f m matches world-space kernel sigma (12%%)'%(radius,height,distance))
    if not four_k: return
    result['four_k']={}
    for distance in (1.,2.,3.):
        f=fixture(3840,2160,distance=distance)
        n,_,_=runner.frame(f)
        jump=faceting(n,f)
        mask=(f['ids']>=0)&(np.sum(f['shading']*unit(-f['positions']),axis=-1)>.55)
        for axis in (0,1):
            for offset in (-8,8): mask &= np.roll(f['ids'],offset,axis)==f['ids']
        variation=np.maximum(angle(n,np.roll(n,4,0)),angle(n,np.roll(n,4,1)))
        flat=float(np.mean(variation[mask]<.01))
        case=dict(faceting_deg=jump,locally_flat_fraction=flat)
        require(jump<1.0 and flat<.1,'4K %.0f m: edge jump < 1 degree and locally flat fraction < 10%%'%distance)
        if distance==2.:
            pixels=np.uint8(np.clip((n[::-1]*.5+.5)*255,0,255))
            Image.fromarray(pixels).save(out/(label+'_4k_debug.png'))
            Image.fromarray(pixels[700:1212,1600:2112]).save(out/(label+'_4k_crop.png'))
        del f,n,variation
        phases=[]
        for phase in (0.,.37,.71):
            c=crawl(runner,frames=4,mapped='curved',detail=1,w=1280,h=2160,distance=distance,phase=phase,camera=phase>.5)
            print('4K',distance,phase,json.dumps(c),flush=True)
            phases.append(c)
            require(c['rim_signal_rms']>.002,'4K rim excitation exceeds .002 RMS (black output fails)')
            require(c['normal_rms_deg']<.2 and c['normal_p99_deg']<.8,'4K curved mapped normal crawl RMS < .2 / p99 < .8 degrees')
            require(c['rim_display_relative_rms']<.01 and c['rim_relative_rms']<.03,'4K captured relative rim variation < 1% (linear < 3%)')
        case['phases']=phases; result['four_k'][str(distance)]=case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shader-ref', default='WORKTREE')
    parser.add_argument('--out', type=Path, default=G.REPO/'.tmp/rim_normals')
    parser.add_argument('--measure-only', action='store_true')
    parser.add_argument('--timing', action='store_true')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    runner = Runner(G.SourceTree(args.shader_ref))
    print('GL',runner.gl.renderer,flush=True)
    failures = []
    def require(ok, message):
        print(('PASS: ' if ok else 'FAIL: ')+message,flush=True)
        if not ok: failures.append(message)
    result = dict(ref=args.shader_ref, renderer=runner.gl.renderer)
    proxy = fixture()
    n, _, _ = runner.frame(proxy)
    result['faceting_deg'] = faceting(n,proxy)
    label = 'after' if args.shader_ref=='WORKTREE' else 'before'
    Image.fromarray(np.uint8(np.clip((n[::-1]*.5+.5)*255,0,255))).save(args.out/(label+'_proxy_debug.png'))
    require(result['faceting_deg'] < 3., 'faceted proxy mean triangle-edge jump < 3 degrees')
    smooth = fixture(smooth=True)
    ns, _, _ = runner.frame(smooth)
    result['interpolated_faceting_deg'] = faceting(ns,smooth)
    require(result['interpolated_faceting_deg'] < 2., 'interpolated proxy mean triangle-edge jump < 2 degrees')
    detailed_vertex,_,_=runner.frame(smooth,detail=1)
    require(faceting(detailed_vertex,smooth)<2.,'Texture=1 does not reintroduce facets on interpolated vertex normals')
    partial,_,partial_aux=runner.frame(smooth,smoothness=.37)
    shading=oct_decode(np.rint(smooth['normal'][...,:2]*65535)/65535)
    expected=unit(shading*.63+oct_decode(partial_aux[...,:2])*.37)
    require(float(np.max(angle(partial,expected)))<.002,
            'debug normal equals the final rim Smoothness blend (including partial strength)')
    # Far background cannot inherit even a single foreground contribution.
    bg = proxy['ids'] < 0
    isolated,_,_=runner.frame(background_only(proxy))
    result['background_error_deg'] = float(np.max(angle(n[bg],isolated[bg])))
    require(result['background_error_deg'] < .03, 'no silhouette bleed into far geometry')
    # A one-pixel far gap must stop the filter, even if a same-depth surface
    # resumes beyond it. This catches sparse kernels that hop over silhouettes.
    thin=plane_fixture(192,192,0,2.5,1)
    thin['depth'][:]=thin['depth'][96,96]
    thin['normal'][:]=[.5,.5,0,0]
    thin['normal'][:,:94,:2]=[.75,.5]
    thin['depth'][:,94:95]=background_only(proxy)['depth'][0,0]
    thin['normal'][:,94:95,:2]=[.35,.5]
    separated,_,_=runner.frame(thin,radius=50)
    require(float(np.max(np.abs(separated[:,95:,0])))<.001,
            'one-pixel far gap prevents transfer between nearby surfaces')
    n0, r0, off_aux = runner.frame(proxy,smoothness=0)
    n1, r1, _ = runner.frame(proxy,smoothness=0,radius=50,detail=0)
    require(np.array_equal(r0,r1), 'Smoothness=0 ignores both controls byte for byte')
    old = Runner(G.SourceTree(BASE), runner.gl)
    _, baseline_rim, baseline_aux = old.frame(proxy,smoothness=0)
    require(np.array_equal(r0,baseline_rim), 'Smoothness=0 rim bytes equal baseline production GLSL')
    require(np.array_equal(off_aux,baseline_aux), 'Smoothness=0 stock aux including Toksvig is byte identical')
    # Default pass-demand predicates must remain identical to baseline C++.
    pipe=runner.tree.read('indra/newview/pipeline.cpp')
    stock=old.tree.read('indra/newview/pipeline.cpp')
    for name in ('want_aux','want_sil'):
        begin='const bool '+name+' = '
        current=pipe[pipe.index(begin):].split(';',1)[0]
        original=stock[stock.index(begin):].split(';',1)[0]
        require(current==original,'default C++ '+name+' pass demand unchanged')
    if runner.has_smooth:
        gate=pipe[pipe.index('const bool smooth_normals ='):].split('if (rim_smooth()',1)[0]
        require('rim_smooth() > 0.f' in gate and 'ensureRigRimSmooth()' in gate,
                'new allocations and passes guarded by positive Smoothness')
    # Static camera output must be completely deterministic.
    again, _, _ = runner.frame(proxy)
    require(np.array_equal(n,again), 'static camera/avatar debug bytes identical')
    result['crawl'] = crawl(runner)
    result['mapped_crawl'] = crawl(runner,mapped=True)
    result['texture_one_crawl'] = crawl(runner,mapped=True,detail=1)
    result['curved_mapped_stress'] = crawl(runner,mapped='curved',detail=1,legacy=True)
    require(result['crawl']['normal_rms_deg'] < .08, 'moving proxy tracked normal RMS < .08 degrees/frame')
    require(result['mapped_crawl']['normal_rms_deg'] < .3, 'moving normal map tracked normal RMS < .3 degrees/frame')
    require(result['texture_one_crawl']['normal_rms_deg'] < .3, 'Texture normals=1 moving map RMS < .3 degrees/frame')
    require(result['curved_mapped_stress']['rim_rms'] < .0002,
            'curved mapped stress rim crawl < .0002 linear RMS/frame')
    require(result['curved_mapped_stress']['normal_rms_deg']<.3 and
            result['curved_mapped_stress']['normal_p99_deg']<1.0,
            'raw curved mapped stress normal crawl RMS < .3 / p99 < 1 degree')
    widths=[radius_measure(runner,*case) for case in ((384,3,1),(768,3,1),(384,6,1),(384,3,1.5))]
    result['radius_world_sigma_m']=widths
    require(min(widths)>.005 and max(widths)/min(widths)<1.12,
            'surface radius invariant under distance, zoom and resolution (12% sampling tolerance)')
    mapped = fixture(mapped=True)
    pure, _, pure_aux = runner.frame(mapped,detail=0)
    detailed, _, detail_aux = runner.frame(mapped,detail=1)
    interior = mapped['ids'] >= 0
    for axis in (0,1):
        for offset in range(-6,7): interior &= np.roll(mapped['ids'],offset,axis)>=0
    result['texture_transfer_rms_deg'] = float(np.sqrt(np.mean(angle(pure[interior],detailed[interior])**2)))
    require(result['texture_transfer_rms_deg'] > 3., 'Texture normals restores normal-map detail')
    full_shading=oct_decode(np.rint(mapped['normal'][...,:2]*65535)/65535)
    result['texture_one_error_deg']=float(np.max(angle(detailed[interior],full_shading[interior])))
    require(result['texture_one_error_deg']<.05,
            'Texture normals=1 preserves full G-buffer perturbation on mapped surface')
    result['toksvig_pure'] = float(np.mean(pure_aux[...,2][interior]))
    result['toksvig_detail'] = float(np.mean(detail_aux[...,2][interior]))
    require(result['toksvig_detail'] < result['toksvig_pure'], 'Toksvig tracks transferred final-normal detail')
    review_checks(runner,require,result,args.out,label)
    if args.timing:
        result['timing'] = {}
        for w,h in ((1920,1080),(3840,2160)):
            f = fixture(w,h)
            for _ in range(3): runner.frame(f,read=False)
            samples = []
            for _ in range(7):
                stats = {}; runner.frame(f,read=False,stats=stats); samples.append(stats)
            result['timing'][str(h)] = {key:float(np.median([s.get(key,0) for s in samples])) for key in ('aux_ms','smooth_ms')}
            values=result['timing'][str(h)]
            total=values['aux_ms']+values['smooth_ms']
            require(total<(.25 if h==1080 else .9),'default-radius guide + filtering cost target at %sp'%h)
            result['timing'][str(h)]['total_ms']=total
            result.setdefault('extra_memory_mib',{})[str(h)]=(w*h*10+((w+3)//4)*((h+3)//4)*40)/(1024**2) if runner.hierarchical else w*h*24/(1024**2)
            print('GPU',w,h,result['timing'][str(h)],flush=True)
    result['failures'] = failures
    (args.out/(label+'_metrics.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2),flush=True)
    if failures and not args.measure_only: raise SystemExit(1)


if __name__=='__main__': main()
