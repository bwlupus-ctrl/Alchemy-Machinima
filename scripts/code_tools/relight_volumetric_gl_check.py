"""Real WGL regression/measurement gate for production projector shafts (no viewer build).

python scripts/code_tools/relight_volumetric_gl_check.py [--width 640] [--height 360]
Baseline is pinned to c4d3614d3e7. Only the named missed-cone control changes its
source; the review-round cost baseline is 167d3c5c44f. Final variants execute
WORKTREE production GLSL and deferred/shadow
modules. Analytic G-buffers AND a light-space analytic depth map describe the
same capsule, wall and floor. Timings are GL_TIME_ELAPSED draw timings (median,
warmup excluded), not viewer FPS. Shared production C++ bounds/uploads/draws
are compiled into a trace host and replayed on WGL. Mutations stay in memory.
Missing GL or a non-5090 renderer is a failure.

--looks-only runs just the Light > Projector Shafts look gate (the full run includes it): the 13 shipped looks
parsed from alprojectorshaftlooks.cpp, executed through the production shaft GLSL (static / motion / crawl / cost /
A-B proofs) plus the backlit-occluder lane fixture for Silhouette Rays. Run it at 1920x1080 for the pinned costs.
"""
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import relight_3d_gl as G
import relight_3d_scene as S

SHAFT = 'class3/deferred/projectorVolumetricF.glsl'
UP = 'class1/deferred/projectorVolumetricUpsampleF.glsl'
BASE = 'c4d3614d3e7'
ROUND1 = '167d3c5c44f'
ROUND2 = '3439a7e8c3d'
BLOOM = 'class1/deferred/projectorVolumetricBloomFeedF.glsl'
G.PREAMBLE += '\n#define SPOT_SHADOW 1\n'


class ControlTree(G.SourceTree):
    """One isolated production-source change, to prove item 1 independently."""
    def read(self, path):
        src = super().read(path)
        if Path(path).as_posix().endswith(SHAFT):
            old = '''        if (ok && ct1 > ct0)
        {
            t0 = ct0;
            t1 = ct1;
        }'''
            assert old in src
            final = (G.REPO/G.SHADERS/SHAFT).read_text()
            start = final.index('        if (ok && ct1 > ct0)')
            end = final.index('\n    }',start)
            replacement = final[start:end]
            assert 'frag_color = vec4(0.0);' in replacement and 'return;' in replacement
            src = src.replace(old, replacement)
        return src


class Runner(G.AreaRunner):
    def __init__(self, tree, gl):
        self.tree, self.gl, self.textures = tree, gl, {}
        self.preamble=G.PREAMBLE
        shaft_source = tree.read(G.SHADERS+'/'+SHAFT)
        self.modern = tree.ref != BASE
        self.mrt = 'out float projvol_termination' in shaft_source
        self.host = None
        self.termination = None
        self.shaft, self.up = self.link(SHAFT, True), self.link(UP, False)
        self.performance_variants = 'PROJVOL_BUDGET_ENABLE' in shaft_source
        self.fine_permutation = 'PROJVOL_FINE_MEDIUM' in shaft_source
        self.variants = {(False,False,False,False):self.shaft}
        self.ab=None
        self.fbo, self.vbo = gl.U(), gl.U()
        gl.gen_fbo(1, C.byref(self.fbo)); gl.gen_buf(1, C.byref(self.vbo))
        gl.bind_buf(0x8892, self.vbo)
        verts = np.array([-1,-1,0, 3,-1,0, -1,3,0], np.float32)
        gl.buffer_data(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
        for cap in (0x0B71,0x0BE2,0x0C11,0x8DB9,0x0B44,0x0BD0): gl.disable(cap)
        self.enable = gl.fn('glEnable', None, gl.U)
        self.blend = gl.fn('glBlendFunc', None, gl.U, gl.U)
        self.error = gl.fn('glGetError', gl.U)
        self.delete_query = gl.fn('glDeleteQueries', None, gl.I, C.POINTER(gl.U))
        self.mips = gl.fn('glGenerateMipmap', None, gl.U)
        self.scissor = gl.fn('glScissor', None, gl.I, gl.I, gl.I, gl.I)
        self.clear_color = gl.fn('glClearColor', None, gl.F, gl.F, gl.F, gl.F)
        self.draw_buffers = gl.fn('glDrawBuffers', None, gl.I, C.POINTER(gl.U))
        self.read_buffer = gl.fn('glReadBuffer', None, gl.U)
        self.blend_i = gl.fn('glBlendFunci', None, gl.U, gl.U, gl.U)
        self.clear = gl.fn('glClear', None, gl.U)

    def link(self, file, shadows):
        gl = self.gl
        pr = gl.program()
        mods = list(self.FRAGMENT_COMMON)
        if shadows: mods += ['deferred/shadowUtil.glsl']
        for mod in mods:
            path = self._production_file(mod, 1 if mod in self.LEVEL1 else 3)
            gl.attach(pr, self._compile(self.tree.read(path), path))
        gl.attach(pr, self._compile(self.tree.read(G.SHADERS+'/'+file), file))
        path = self._production_file('deferred/postDeferredNoTCV.glsl')
        gl.attach(pr, self._compile(self.tree.read(path), path, 0x8B31))
        gl.fn('glBindFragDataLocation',None,gl.U,gl.U,C.c_char_p)(pr,0,b'frag_color')
        gl.link(pr); ok = gl.I(); gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000); gl.plog(pr,len(log),None,log)
            raise AssertionError(file+' LINK '+log.value.decode())
        return pr

    def ui(self, pr, name, val): self.gl.ui(self.gl.loc(pr,name.encode()),int(val))
    def uf(self, pr, name, *val): self.gl.uf[len(val)](self.gl.loc(pr,name.encode()),*val)

    def target(self, name, w, h, mrt=False):
        tex = self._texture(name, 0, 0x881A, w,h,0x1908,0x1406,None) # RGBA16F
        gl = self.gl
        gl.bind_fbo(0x8D40,self.fbo); gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,tex,0)
        dt = self._texture('termination',7,0x822E,w,h,0x1903,0x1406,None) if mrt else 0
        gl.fbo_tex(0x8D40,0x8CE1,0x0DE1,dt,0)
        self.draw_buffers(2 if mrt else 1,(gl.U*2)(0x8CE0,0x8CE1))
        self.read_buffer(0x8CE0)
        assert gl.fbo_status(0x8D40)==0x8CD5
        gl.viewport(0,0,w,h)
        gl.disable(0x0C11); self.clear_color(0,0,0,0); self.clear(0x4000)
        return tex

    def draw(self, stats, key):
        gl = self.gl
        q = gl.U(); gl.gen_queries(1,C.byref(q))
        gl.begin_query(0x88BF,q); gl.draw(4,0,3); gl.end_query(0x88BF)
        t = C.c_uint64(); gl.query_u64(q,0x8866,C.byref(t))
        self.delete_query(1,C.byref(q))
        stats[key] = stats.get(key,0)+t.value*1e-6

    def run(self, fixture, half=False, steps=24, dither=0, frame=0, equi=0,
            air=0, footprint=0, rim=0, soft=True, cookie=False, shadows=True, rect=None, beam=0, conservative=0, maximum=100, bloom=False,
            planes_override=None, count=False, layers=1, cap=0, rim_rect=None, uniforms=None,
            budget_steps=0., hard_beam=False):
        cam,g,depth,norm,light = fixture
        gl = self.gl; W,H = cam.W,cam.H
        tw,th = (W//2,H//2) if half else (W,H)
        self._texture('depth',1,0x81A6,W,H,0x1902,0x1405,depth)
        self._texture('normal',2,0x805B,W,H,0x1908,0x1403,norm)
        shadow = light['depth'] if shadows else np.ones_like(light['depth'])
        self._texture('shadow',3,0x8CAC,shadow.shape[1],shadow.shape[0],0x1902,0x1406,shadow)
        gl.param(0x0DE1,0x884C,0x884E); gl.param(0x0DE1,0x884D,0x0203)
        gl.param(0x0DE1,0x2801,0x2601); gl.param(0x0DE1,0x2800,0x2601)
        ck = np.ones((512,512,4),np.float32)
        if cookie:
            y,x = np.mgrid[:512,:512]; pattern = ((x//3+y//3)%2).astype(np.float32)
            ck[:,:,:3] = pattern[:,:,None]
        self._texture('cookie',4,0x8058,512,512,0x1908,0x1406,ck)
        self.mips(0x0DE1); gl.param(0x0DE1,0x2801,0x2703)
        marched = self.target('march',tw,th,half and self.mrt)
        pr = self.shaft
        integration = bool(equi or air or footprint) and self.modern
        # Density is a runtime control; it must not select a shader or reset history.
        fine = self.fine_permutation and budget_steps>0 and (uniforms or {}).get('projvol_noise_strength',0.)>.5
        variant = (integration,bool(budget_steps>0),bool(hard_beam),bool(fine))
        if self.performance_variants or integration:
            if variant not in self.variants:
                saved=G.PREAMBLE
                try:
                    G.PREAMBLE=self.preamble+'\n'+''.join('#undef %s\n#define %s %d\n'%(key,key,int(value)) for key,value in zip(
                        ('PROJVOL_INTEGRATION_AB','PROJVOL_BUDGET_ENABLE','PROJVOL_HARD_BEAM','PROJVOL_FINE_MEDIUM'),variant))
                    self.variants[variant]=self.link(SHAFT,True)
                finally: G.PREAMBLE=saved
            pr=self.variants[variant]
            if integration:self.ab=pr
        self._begin(pr)
        self._mat(pr,b'inv_proj',np.linalg.inv(cam.proj))
        self._mat(pr,b'projvol_inv_modelview',np.linalg.inv(cam.view))
        self._mat(pr,b'proj_mat',light['matrix']@np.linalg.inv(cam.view))
        mats = np.tile(np.eye(4),(14,1,1)); mats[4:] = light['matrix']@np.linalg.inv(cam.view)
        data = np.ascontiguousarray(mats.transpose(0,2,1),np.float32)
        gl.matrix(gl.loc(pr,b'shadow_matrix'),14,0,data.ctypes.data_as(C.POINTER(gl.F)))
        planes = (light['planes'] if planes_override is None else planes_override)@np.linalg.inv(cam.view)
        planes = np.ascontiguousarray(planes/np.linalg.norm(planes[:,:3],axis=1)[:,None],np.float32)
        gl.u4fv(gl.loc(pr,b'projvol_frustum_planes'),6,planes.ctypes.data_as(C.POINTER(gl.F)))
        centre = (cam.view@np.array([*light['pos'],1]))[:3]
        axis = cam.view[:3,:3]@light['dir']
        for name,val in [('center',centre),('color',(1,.8,.55)),('proj_n',axis),('gobo_tint',(1,1,1))]:
            self.uf(pr,name,*val)
        for name,val in {'size':12,'falloff':.3,'godray_multiplier':6,'projvol_g':.8,
                         'projvol_max':maximum,'projvol_density':1,'projvol_feather':.035,
                         'projvol_max_distance':cap,
                         'proj_focus':12 if cookie else 0,'proj_lod':9,'proj_range':12,'projvol_frame':frame,
                         'projvol_rim_strength':rim if not half or not self.modern else 0,
                         'projvol_rim_power':2,'projvol_rim_wrap':.5,'projvol_rim_threshold':0,
                         'soft_shadow_scale':4,'soft_shadow_max':8,'shadow_split_blend':.25,
                         'gobo_intensity':1}.items(): self.uf(pr,name,val)
        self.uf(pr,'projvol_half_res',*( (tw,th) if half and self.modern else (0,0)))
        self.uf(pr,'projvol_budget_steps',float(budget_steps))
        self.ui(pr,'projvol_hard_beam_shadows',int(hard_beam))
        self.uf(pr,'proj_shadow_res',*shadow.shape[::-1]); self.uf(pr,'shadow_clip',16,32,48,64)
        self.uf(pr,'gobo_anim_params',0,0,1,0) # production default zoom is 1, not an unset uniform's 0
        for name,val in {'depthMap':1,'normalMap':2,'projectionMap':4,'proj_shadow_idx':0,
                         'godray_res':steps,'projvol_dither':dither,'projvol_frustum_clip':1,
                         'projvol_shadow_samples':1,'projvol_equiangular':equi,
                         'projvol_temporal_beam_depth':beam,
                         'projvol_conservative_shadow':conservative,'projvol_dust_map':6,
                         'projvol_airborne_tap':air,'projvol_cookie_footprint':footprint,
                         'soft_shadow_enable':soft,'gobo_pattern':-1}.items(): self.ui(pr,name,val)
        # Per-call uniform overrides (looks gate): applied last so they win over the fixture bases above. Ints use
        # glUniform1i, tuples glUniform2/3/4f, everything else glUniform1f. Unknown names are ignored by GL (loc -1).
        for name,val in (uniforms or {}).items():
            if isinstance(val,tuple): self.uf(pr,name,*map(float,val))
            elif isinstance(val,(bool,int,np.integer)): self.ui(pr,name,int(val))
            else: self.uf(pr,name,float(val))
        for i in range(4,14): self.ui(pr,'shadowMap'+str(i),3)
        if self.host is not None:
            commands = host_request(self.host, 'U', [dither,1,frame/60.0])
            self.execute(pr,commands,None,None)
        stats = {}
        if count: self.reset_counter()
        if half and self.mrt:
            self.enable(0x0BE2); self.blend(1,1); self.blend_i(1,1,0)
        if rect is not None:
            self.enable(0x0C11); self.scissor(*rect)
        for _ in range(layers): self.draw(stats,'march_ms')
        gl.disable(0x0C11)
        if count: stats['iterations'] = self.read_counter()
        if half and self.mrt:
            self.blend_i(1,1,1); gl.disable(0x0BE2)
            self.termination = np.empty((th,tw),np.float32)
            self.read_buffer(0x8CE1)
            gl.read(0,0,tw,th,0x1903,0x1406,self.termination.ctypes.data)
            self.read_buffer(0x8CE0)
        if half:
            output = self.target('composite',W,H)
            up = self.up; self._begin(up); self._mat(up,b'inv_proj',np.linalg.inv(cam.proj))
            self.ui(up,'depthMap',1); self.ui(up,'projectionMap',5); self.ui(up,'lightMap',7)
            self.uf(up,'projvol_half_res',tw,th)
            gl.active(0x84C5); gl.bind_tex(0x0DE1,marched)
            gl.param(0x0DE1,0x2801,0x2601); gl.param(0x0DE1,0x2800,0x2601)
            if rect is not None:
                self.enable(0x0C11)
                sx,sy,sw,sh=rect
                x0,y0=int(np.floor(sx*W/tw)),int(np.floor(sy*H/th))
                x1,y1=int(np.ceil((sx+sw)*W/tw)),int(np.ceil((sy+sh)*H/th))
                self.scissor(x0,y0,x1-x0,y1-y0)
            self.draw(stats,'upsample_ms'); gl.disable(0x0C11)
            if rim>0 and self.modern:
                if bloom: self.target('rim_source',W,H)
                self._begin(pr)
                self.enable(0x0BE2); self.blend(1,1)
                if self.host is not None:
                    r = rim_rect if rim_rect is not None else rect if rect is not None else (0,0,tw,th)
                    commands=host_request(self.host,'R',[int(rect is not None),*r,tw,th,W,H,rim])
                    self.execute(pr,commands,stats,'rim_ms')
                else:
                    self.ui(pr,'godray_res',0); self.uf(pr,'projvol_half_res',0,0)
                    self.uf(pr,'projvol_rim_strength',rim); self.draw(stats,'rim_ms')
                gl.disable(0x0C11); gl.disable(0x0BE2)
                if bloom:
                    rim_tex=self.textures['rim_source'][0]
                    self.target('rim_bloom',W,H)
                    if not hasattr(self,'bloom_program'): self.bloom_program=self.link(BLOOM,False)
                    bp=self.bloom_program; self._begin(bp)
                    commands=host_request(self.host,'F',[1,1/tw,1/th,.3,1])
                    self.execute(bp,commands,stats,'bloom_ms')
        gl.finish()
        raw = np.empty((H,W,4),np.uint16) # retain exact RGBA16F bytes
        gl.read(0,0,W,H,0x1908,0x140B,raw.ctypes.data)
        assert self.error()==0, 'OpenGL error'
        image = raw.view(np.float16).astype(np.float32)
        assert np.isfinite(image).all(), 'NaN/Inf in production output'
        return image,raw,stats

    def temporal_resolve(self, fixture, previous, previous_camera, valid, blend=.75, reject=3.5):
        cam=fixture[0]; gl=self.gl; w,h=cam.W//2,cam.H//2
        if not hasattr(self,'temporal_program'):
            self.temporal_program=self.link('class1/deferred/projectorVolumetricTemporalF.glsl',False)
        self._texture('previous_history',9,0x881A,w,h,0x1908,0x1406,previous)
        gl.param(0x0DE1,0x2801,0x2601);gl.param(0x0DE1,0x2800,0x2601)
        self.target('temporal_output',w,h)
        pr=self.temporal_program; self._begin(pr)
        self._mat(pr,b'inv_proj',np.linalg.inv(cam.proj))
        self._mat(pr,b'projvol_inv_modelview',np.linalg.inv(cam.view))
        self._mat(pr,b'projvol_prev_viewproj',previous_camera.proj@previous_camera.view)
        self._mat(pr,b'projvol_prev_modelview',previous_camera.view)
        self.ui(pr,'depthMap',1);self.ui(pr,'normalMap',2);self.ui(pr,'projectionMap',5);self.ui(pr,'projvol_history',9)
        self.ui(pr,'projvol_temporal_beam_depth',1)
        self.uf(pr,'projvol_half_res',w,h)
        self.uf(pr,'projvol_temporal_blend',blend if valid else 0)
        self.uf(pr,'projvol_temporal_reject',reject)
        gl.active(0x84C5);gl.bind_tex(0x0DE1,self.textures['march'][0])
        gl.param(0x0DE1,0x2801,0x2600);gl.param(0x0DE1,0x2800,0x2600)
        timing = {}; self.draw(timing,'temporal_ms'); self.temporal_ms = timing['temporal_ms']; gl.finish()
        out=np.empty((h,w,4),np.float32)
        gl.read(0,0,w,h,0x1908,0x1406,out.ctypes.data)
        assert self.error()==0
        return out

    def execute(self, pr, commands, stats, key):
        for op,*v in commands:
            if op=='scissor':
                self.enable(0x0C11); self.scissor(*map(int,v))
            elif op=='i': self.ui(pr,v[0],int(v[1]))
            elif op=='f': self.uf(pr,v[0],*map(float,v[1:]))
            elif op=='texture':
                self.ui(pr,'diffuseMap',8)
                self.gl.active(0x84C8); self.gl.bind_tex(0x0DE1,self.textures[getattr(self,'bloom_source','rim_source')][0])
            elif op=='buffer': self._begin(pr)
            elif op=='draw': self.draw(stats,key)
            else: raise AssertionError(op)

    def bloom_feed(self, fixture, feed, anamorphic, stats):
        """The production bloom-feed draw (host op F) over the half-res composite; adds 'bloom_ms' to stats."""
        if feed <= 0 or self.host is None or 'composite' not in self.textures: return
        cam=fixture[0]; W,H=cam.W,cam.H; tw,th=W//2,H//2
        self.target('rim_bloom',W,H)
        if not hasattr(self,'bloom_program'): self.bloom_program=self.link(BLOOM,False)
        bp=self.bloom_program; self._begin(bp)
        self.bloom_source='composite'
        try: self.execute(bp,host_request(self.host,'F',[1,1/tw,1/th,feed,anamorphic]),stats,'bloom_ms')
        finally: self.bloom_source='rim_source'

    def reset_counter(self):
        gl=self.gl
        if not hasattr(self,'counter'):
            self.counter=gl.U(); gl.gen_buf(1,C.byref(self.counter))
        gl.bind_buf(0x92C0,self.counter)
        zero=gl.U(0); gl.buffer_data(0x92C0,4,C.addressof(zero),0x88E8)
        gl.fn('glBindBufferBase',None,gl.U,gl.U,gl.U)(0x92C0,0,self.counter)

    def read_counter(self):
        gl=self.gl; gl.finish(); value=gl.U()
        gl.fn('glGetBufferSubData',None,gl.U,C.c_ssize_t,C.c_ssize_t,gl.P)(0x92C0,0,4,C.byref(value))
        return value.value


class InstrumentedTree(G.SourceTree):
    def __init__(self, ref, missed=False, saturation=False, grazing=False):
        super().__init__(ref); self.missed=missed; self.saturation=saturation; self.grazing=grazing
    def read(self,path):
        src=super().read(path)
        if Path(path).as_posix().endswith(SHAFT):
            src=src.replace('void main()', 'layout(binding=0,offset=0) uniform atomic_uint vol_steps;\nvoid main()',1)
            src=re.sub(r'(for \(int i = 0; i < (?:shaft_steps|godray_res); \+\+i\)\s*\{)',
                       r'\1\n        atomicCounterIncrement(vol_steps);',src,count=1)
            if self.missed:
                src=src.replace('else if (projvol_rim_strength <= 0.0)', 'else if (false && projvol_rim_strength <= 0.0)')
                src=src.replace('            t1 = t0;', '            /* no missed-cone early-out control */',1)
            if self.saturation:
                src=src.replace('(projvol_equiangular == 0 || projvol_temporal_beam_depth == 0)', 'projvol_temporal_beam_depth == 0')
            if self.grazing: src=src.replace('bool discontinuity = residual > max((far_z-near_z)*0.25, 2e-6);','bool discontinuity = true;')
        return src


def host_request(host,op,values):
    host.stdin.write(op+' '+' '.join(map(str,values))+'\n'); host.stdin.flush()
    line=host.stdout.readline(); assert line, 'C++ host exited'
    line=line.strip()
    if op in ('U','R','F','Q'): return [cmd.split() for cmd in line.split(';') if cmd]
    return list(map(float,line.split()))


def fixtures(w,h,cone=42):
    prims = [S.Prim('floor','obb',c=(0,2,-.2),h=(15,15,.2)),
             S.Prim('wall','obb',c=(0,7,2.5),h=(15,.1,2.5)),
             S.Prim('avatar','capsule',r=.25,a=(0,1,.25),b=(0,1,1.55))]
    scene = S.Scene('vol',prims,[],[],{},np.empty((0,3)))
    pos = np.array([-1.,-2.,2.5]); target = np.array([0.,2.,.8]); direction = S.unit(target-pos)
    lc = S.Camera(pos,target,768,768,vfov=cone,near=.15,far=12)
    lg = S.cast_primary(scene,lc)
    zf = np.where(np.isfinite(lg['zf']),lg['zf'],12)
    z = (12+.15)/(12-.15)-2*12*.15/((12-.15)*zf)
    depth = np.ascontiguousarray(np.clip(z*.5+.5,0,1),np.float32)
    bias = np.eye(4); bias[:3,:3] *= .5; bias[:3,3]=.5
    vp = lc.proj@lc.view
    planes = np.array([vp[3]+vp[0],vp[3]-vp[0],vp[3]+vp[1],vp[3]-vp[1],vp[3]+vp[2],vp[3]-vp[2]])
    light = dict(pos=pos,dir=direction,matrix=bias@vp,planes=planes,depth=depth,scene=scene,view=lc.view,fov=np.deg2rad(cone))
    poses = [('outside',(3,-4,1.7),(0,1,1)),('inside',(-.5,0,1.7),(0,3,1)),
             ('toward_apex',(.5,3,1.6),(-1,-2,2.5))]
    out = {}
    for name,eye,tgt in poses:
        cam = S.Camera(eye,tgt,w,h); g = S.cast_primary(scene,cam)
        _,d,n = S.encode_gbuffer(cam,g); out[name] = (cam,g,d,n,light)
    return out


def edge_mask(g):
    actor = g['surf']==2
    edge = np.zeros_like(actor)
    for dy,dx in ((0,1),(0,-1),(1,0),(-1,0)):
        edge |= actor != np.roll(actor,(dy,dx),(0,1))
    for _ in range(2):
        edge |= np.roll(edge,1,0)|np.roll(edge,-1,0)|np.roll(edge,1,1)|np.roll(edge,-1,1)
    return edge


def build_scissor_host(out, glm_include, mutation=None, ref='WORKTREE'):
    """Compile the actual production near-plane clipping helper, not a Python port."""
    if glm_include is None:
        cache = Path.home()/'cxs/zs_flags_cache_cube.json'
        flags = ' '.join(f for v in json.loads(cache.read_text(encoding='utf-8-sig')).values() for f in v['flags'])
        # The JSON cache has ordinary Windows paths; inspect both /I and external /I.
        candidates = re.findall(r'"([A-Z]:[^"\r\n]+)"',flags)
        glm_include = next((Path(p) for p in candidates if (Path(p)/'glm/glm.hpp').exists()),None)
    assert glm_include and (glm_include/'glm/glm.hpp').exists(), 'provide --glm-include'
    pipe = G.SourceTree(ref).read('indra/newview/pipeline.cpp')
    start=pipe.index('static bool projvolClipPyramidBounds(')
    helper=pipe[start:pipe.index('// End standalone-tested projector decisions.',start)]
    if mutation=='near_coverage':
        old=G.SourceTree(ROUND2).read('indra/newview/pipeline.cpp')
        a='static bool projvolSphereBounds('; b='static U32 projvolAdaptiveResolution('
        helper=helper[:helper.index(a)]+old[old.index(a):old.index(b)]+helper[helper.index(b):]
    if mutation=='rim_buffer': helper=helper.replace('    triangle->setBuffer();', '    /* missing buffer */')
    march_rim=re.search(r'llclamp\(e_max_dist, 0.f, 64.f\), (.*?), tgt_w, tgt_h, pyramid\)',pipe).group(1)
    if mutation=='march_cap': march_rim='e_rimStrength > 0.f'
    if mutation=='pyramid': helper=helper.replace('const F32 fx = fy * scale.x / scale.y;', 'const F32 fx = 0.f;')
    if mutation=='rim': helper=helper.replace('triangle->drawArrays(LLRender::TRIANGLES, 0, 3);','')
    if mutation=='rim_scissor': helper=helper.replace('if (bounds_valid)', 'if (false)')
    if mutation=='rim_feed':
        helper=helper.replace('    if (!valid) return;', '    if (true) return;')
    if mutation=='dither': helper=helper.replace('llclamp(dither, (U32)0, (U32)2)', '(temporal && dither == 1 ? 2u : llclamp(dither, (U32)0, (U32)2))')
    adaptive=re.search(r'const U32 cone_res = (projvolAdaptiveResolution\(.*?\));',pipe,re.S).group(1)
    adaptive=adaptive.replace('BDMergeProjectorVolumetricsAdaptive','true').replace('tgt_h','h')
    if mutation=='adaptive': adaptive=adaptive.replace('sphere_valid, sphere','ok, bounds')
    source = r'''#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
using F32=float; using F64=double; using S32=int; using U32=unsigned;
template<class T> T llmin(T a,T b){return std::min(a,b);}
template<class T> T llmax(T a,T b){return std::max(a,b);}
template<class T> T llclamp(T x,T a,T b){return std::clamp(x,a,b);}
using LLStaticHashedString=std::string;
namespace LLShaderMgr {
const char* PROJVOL_DITHER="projvol_dither"; const char* PROJVOL_FRAME="projvol_frame";
const char* GODRAY_RES="godray_res"; const char* PROJVOL_TEMPORAL_BEAM_DEPTH="projvol_temporal_beam_depth";
const char* PROJVOL_RIM_STRENGTH="projvol_rim_strength";
const char* DIFFUSE_MAP="diffuseMap"; const char* BLOOM_TEXEL_SIZE="bloom_texel_size";
const char* PROJVOL_BLOOM_FEED="projvol_bloom_feed"; const char* PROJVOL_BLOOM_ANAMORPHIC="projvol_bloom_anamorphic";
}
namespace LLRender { const int TRIANGLES=4; }
namespace LLTexUnit {const int TFO_BILINEAR=1;}
struct LLRenderTarget {};
struct LLGLSLShader {
void bindTexture(const std::string&,LLRenderTarget*,bool,int){std::cout<<"texture rim;";}
void uniform1i(const std::string& n,int x){std::cout<<"i "<<n<<" "<<x<<";";}
void uniform1f(const std::string& n,float x){std::cout<<"f "<<n<<" "<<x<<";";}
void uniform2f(const std::string& n,float x,float y){std::cout<<"f "<<n<<" "<<x<<" "<<y<<";";}
};
struct LLVertexBuffer {
void setBuffer(){std::cout<<"buffer;";}
void drawArrays(int,int,int){std::cout<<"draw;";}
};
void glScissor(int x,int y,int w,int h){std::cout<<"scissor "<<x<<" "<<y<<" "<<w<<" "<<h<<";";}
'''+helper
    budget_op = r'''
else if(op=="P"){unsigned mode,lo,hi;bool adaptive;float coverage,height,maximum,cap,budget,total;
std::cin>>mode>>adaptive>>coverage>>height>>lo>>hi>>maximum>>cap>>budget>>total;
std::cout<<projvolStepBudget(mode,adaptive,coverage,height,lo,hi,maximum,cap,budget,total)<<std::endl;}
else if(op=="Q"){bool rim;unsigned mode,legacy,look;float budget;std::cin>>rim>>mode>>legacy>>look>>budget;
projvolUploadStepBudget(shader,rim,mode,legacy,look,budget);std::cout<<std::endl;}
''' if 'static F32 projvolStepBudget(' in helper else ''
    if 'static bool projvolVariantReady(' in helper:
        header = G.SourceTree(ref).read('indra/newview/pipeline.h')
        loaded = re.search(r'void projectorVolumetricShaderLoaded\([^)]*\)\s*\{(.*?)\n    \}',header,re.S).group(1)
        manager = G.SourceTree(ref).read('indra/newview/llviewershadermgr.cpp')
        restore = re.search(r'if \(gPipeline.mProjVolVariantFailed\)\s*\{(.*?)\n        \}',manager,re.S).group(1)
        budget_op += r'''
else if(op=="V"){bool mProjVolVariantFailed=true;
auto reload=[&](bool success,int actual_level,int requested_level){'''+loaded+r'''};
std::cout<<projvolVariantReady(mProjVolVariantFailed,true)<<" ";
reload(false,3,3);std::cout<<projvolVariantReady(mProjVolVariantFailed,true)<<" ";
reload(true,1,3);std::cout<<projvolVariantReady(mProjVolVariantFailed,true)<<" ";
reload(true,3,3);std::cout<<projvolVariantReady(mProjVolVariantFailed,true)<<" ";
struct {int mShaderLevel=1;} gDeferredProjectorVolumetricProgram;
int mShaderLevel[1]={3};const int SHADER_DEFERRED=0;bool success=true;
reload(success,1,3);
if(mProjVolVariantFailed){'''+restore+r'''}
int retry=gDeferredProjectorVolumetricProgram.mShaderLevel;
reload(true,retry,3);std::cout<<retry<<" "<<projvolVariantReady(mProjVolVariantFailed,true)<<std::endl;}
'''
    source += r'''
void matrix(glm::mat4& m){for(int c=0;c<4;++c)for(int r=0;r<4;++r)std::cin>>m[c][r];}
int main(){std::cout<<std::setprecision(9); std::string op; LLGLSLShader shader; LLVertexBuffer triangle;
while(std::cin>>op){
if(op=="U"){unsigned d;bool t;double f;std::cin>>d>>t>>f;projvolUploadRecordingUniforms(shader,d,t,f);std::cout<<std::endl;}
else if(op=="R"){bool valid;float x,y,w,h,mw,mh,rim;int W,H;std::cin>>valid>>x>>y>>w>>h>>mw>>mh>>W>>H>>rim;
projvolDrawFullResRim(shader,&triangle,true,valid,glm::vec4(x,y,x+w,y+h),mw,mh,W,H,rim);std::cout<<std::endl;}
else if(op=="F"){bool valid;float x,y,feed,ana;std::cin>>valid>>x>>y>>feed>>ana;LLRenderTarget rim;
projvolDrawRimTexture(shader,&triangle,&rim,valid,glm::vec2(x,y),feed,ana);std::cout<<std::endl;}
else if(op=="A"){float w,h,radius;glm::vec3 c;glm::mat4 proj;std::cin>>w>>h>>radius>>c.x>>c.y>>c.z;matrix(proj);
glm::vec4 sphere;bool ok=projvolSphereBounds(c,radius,proj,w,h,sphere);
std::cout<<ok<<" "<<sphere.x<<" "<<sphere.y<<" "<<sphere.z<<" "<<sphere.w<<" "
<<projvolAdaptiveResolution(true,ok,sphere,h,6,16)<<std::endl;}
else if(op=="B" || op=="C" || op=="E"){float w,h,fov,radius,cap;bool rim;glm::vec3 position,scale;glm::mat4 rotation,view,proj;
std::cin>>w>>h>>fov>>radius>>cap>>rim;
for(int k=0;k<3;++k)std::cin>>position[k];for(int k=0;k<3;++k)std::cin>>scale[k];
matrix(rotation);matrix(view);matrix(proj);U32 min_res=6,max_res=16;if(op=="E")std::cin>>min_res>>max_res;glm::vec4 bounds,sphere;
bool halfres=true;float e_rimStrength=rim?1.f:0.f;
bool include_rim=op=="C" ? ('''+march_rim+r''') : rim;
bool ok=projvolPyramidGeometry(position,scale,fov,rotation,view,proj,radius,cap,include_rim,w,h,bounds);
bounds=glm::clamp(glm::vec4(floorf(bounds.x)-1,floorf(bounds.y)-1,ceilf(bounds.z)+1,ceilf(bounds.w)+1),glm::vec4(0),glm::vec4(w,h,w,h));
bool sphere_valid=projvolSphereBounds(glm::vec3(view*glm::vec4(position,1)),radius,proj,w,h,sphere);
// settings.xml recording defaults (op E overrides min/max with the caller's MinResolution / Resolution)
U32 steps='''+adaptive+r''';
std::cout<<ok<<" "<<bounds.x<<" "<<bounds.y<<" "<<bounds.z<<" "<<bounds.w<<" "<<steps<<" "
<<projvolAdaptiveResolution(true,ok,bounds,h,min_res,max_res)<<std::endl;
}
'''+budget_op+r'''
else return 2;
}}
'''
    src=out/'scissor_host.cpp'; src.write_text(source)
    exe=out/'scissor_host.exe'
    vswhere=Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
    vs=subprocess.check_output([str(vswhere),'-latest','-property','installationPath'],text=True).strip()
    vcvars=Path(vs)/'VC/Auxiliary/Build/vcvars64.bat'
    cmd=out/'build_scissor.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc /DGLM_FORCE_DEFAULT_ALIGNED_GENTYPES /DGLM_FORCE_INTRINSICS /I "%s" "%s" /Fe:"%s" /Fo:"%s"\n' %
                   (vcvars,glm_include,src,exe,out/'scissor_host.obj'))
    r=subprocess.run([str(cmd)],cwd=out,capture_output=True,text=True,shell=True)
    assert r.returncode==0, 'scissor helper compile failed:\n'+r.stdout+r.stderr
    return subprocess.Popen([str(exe)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)



def production_bounds(host,fixture,half,cap=0,rim=True,op='B'):
    cam,_,_,_,light=fixture
    w,h=(cam.W//2,cam.H//2) if half else (cam.W,cam.H)
    rotation=np.eye(4); rotation[:3,:3]=np.linalg.inv(light['view'])[:3,:3]
    scale=light.get('scale',(.3*np.tan(light['fov']/2),)*2+(.3,))
    values=[w,h,light['fov'],12,cap,int(rim),*light.get('sphere_pos',light['pos']),*scale,
            *rotation.T.ravel(),*cam.view.T.ravel(),*cam.proj.T.ravel()]
    return host_request(host,op,values)


def scissor_rect(host,fixture,half):
    vals=production_bounds(host,fixture,half)
    assert len(vals)==7 and vals[0]==1
    x0,y0,x1,y1=vals[1:5]
    return (int(x0),int(y0),max(0,int(x1-x0)),max(0,int(y1-y0)))


class CenteredTree(G.SourceTree):
    def read(self,path):
        src=super().read(path)
        if Path(path).as_posix().endswith(SHAFT):
            src=src.replace('if (discontinuity)', 'if (false)')
        return src


def grazing_fixture(fixture, motion):
    cam,_,_,_,light=fixture
    # One exact world plane, almost edge-on. Window depth is affine; a large
    # first derivative is not an occlusion boundary. Move the camera 0.2 mm.
    delta=cam.r*motion
    c=S.Camera(cam.eye+delta,cam.eye+cam.f+delta,cam.W,cam.H)
    y,x=np.mgrid[:c.H,:c.W]
    vx=((x+.5)/c.W*2-1)*c.tan*c.aspect
    denom=.015-vx
    zf=np.where(denom>0,(.08+motion)/np.maximum(denom,1e-12),np.inf)
    zf=np.where(zf<c.far,zf,np.inf)
    normal=S.unit(c.view[:3,:3].T@np.array([-1.,0,-.015]))
    g=dict(zf=zf,nrm=np.broadcast_to(normal,(c.H,c.W,3)).copy(),surf=np.zeros((c.H,c.W),np.int32))
    _,depth,norm=S.encode_gbuffer(c,g)
    return c,g,depth,norm,light


def round1_checks(args,gl,host,runners,report,measure):
    def check(name,passed,**kw):
        report['checks'].append(dict(name=name,passed=bool(passed),**kw))
        print(name,'PASS' if passed else 'FAIL',kw,flush=True)
    after=runners['after']
    before=Runner(G.SourceTree(ROUND1),gl)
    outside=fixtures(args.width,args.height)['outside']
    # Workload counters measure executed loop bodies, independent of noisy GPU
    # timings. They instrument production GLSL without changing its decisions.
    active=Runner(InstrumentedTree('WORKTREE'),gl)
    wasted=Runner(InstrumentedTree('WORKTREE',missed=True),gl)
    narrow=fixtures(320,180,14)['outside']
    _,a,ta=active.run(narrow,steps=64,count=True)
    _,b,tb=wasted.run(narrow,steps=64,count=True)
    check('missed_cone_workload',ta['iterations']<tb['iterations']*.9 and a.tobytes()==b.tobytes(),
          after_iterations=ta['iterations'],disabled_iterations=tb['iterations'])
    saturated=Runner(InstrumentedTree('WORKTREE',saturation=True),gl)
    bright=fixtures(320,180)['inside']
    a,_,ta=active.run(bright,steps=64,maximum=.0001,beam=1,count=True,shadows=False)
    b,_,tb=saturated.run(bright,steps=64,maximum=.0001,beam=1,count=True,shadows=False)
    check('ordinary_saturation_workload',ta['iterations']<tb['iterations']*.9 and np.array_equal(a[:,:,:3],b[:,:,:3]),
          after_iterations=ta['iterations'],disabled_iterations=tb['iterations'])
    _,_,ta=active.run(bright,steps=64,maximum=.0001,beam=1,equi=1,count=True,shadows=False)
    _,_,tb=saturated.run(bright,steps=64,maximum=.0001,beam=1,equi=1,count=True,shadows=False)
    check('equiangular_complete_moments',ta['iterations']==tb['iterations'] and ta['iterations']>0,
          iterations=ta['iterations'])
    # Find a real production pyramid sample threshold, then bracket it with a
    # 20 micrometre motion. Sphere coverage must keep the previous full budget.
    f0=fixtures(160,90)['outside']; light=f0[-1]
    def at(distance):
        eye=light['pos']+np.array([1.0,-1.0,.2])*distance
        cam=S.Camera(eye,light['pos']+light['dir']*6,160,90)
        return cam,None,None,None,light
    crossing=None; prev=None
    for distance in np.linspace(2,60,300):
        v=production_bounds(host,at(distance),True)
        if prev is not None and prev[1][6]==15 and v[6]==14:
            lo,hi=prev[0],distance
            for _ in range(30):
                mid=(lo+hi)/2
                if production_bounds(host,at(mid),True)[6]==prev[1][6]:lo=mid
                else:hi=mid
            crossing=(lo+hi)/2; break
        prev=distance,v
    assert crossing is not None,'motion fixture must cross a pyramid threshold'
    counts=[production_bounds(host,at(crossing+d),True) for d in (-.00001,.00001)]
    check('adaptive_motion_threshold',counts[0][5]==counts[1][5] and counts[0][6]!=counts[1][6],
          sphere_steps=[v[5] for v in counts],pyramid_steps=[v[6] for v in counts],distance=crossing)
    # Render at the production-selected budgets over that actual camera move.
    imgs=[]; oldimgs=[]
    for d,v in zip((-.00001,.00001),counts):
        c=at(crossing+d)[0]; g=S.cast_primary(light['scene'],c); _,z,n=S.encode_gbuffer(c,g)
        f=(c,g,z,n,light)
        imgs.append(after.run(f,steps=int(v[5]),dither=1)[0][:,:,:3])
        oldimgs.append(after.run(f,steps=int(v[6]),dither=1)[0][:,:,:3])
    def relative_changes(ims):
        mask=ims[0].max(2)>1e-4
        return int(np.count_nonzero(mask & (np.max(np.abs(ims[1]-ims[0]),2)>.05*np.maximum(ims[0].max(2),1e-4))))
    check('adaptive_motion_interior',relative_changes(imgs)<=relative_changes(oldimgs),
          before_pixels_over_5pct=relative_changes(oldimgs),after_pixels_over_5pct=relative_changes(imgs))
    # Execute the production temporal resolve, including default blend/rejection
    # and beam-depth moments, through eight frames after the threshold crossing.
    temporal_counts=[]
    for budget_index in (6,5):
        history=np.zeros((45,80,4),np.float32); reference=None; pc=None
        for frame in range(9):
            idx=0 if frame==0 else 1
            c=at(crossing+(-.00001 if idx==0 else .00001))[0]
            g=S.cast_primary(light['scene'],c);_,z,n=S.encode_gbuffer(c,g);f=(c,g,z,n,light)
            after.run(f,half=True,steps=int(counts[idx][budget_index]),beam=1,dither=1)
            history=after.temporal_resolve(f,history,c if pc is None else pc,frame>0);pc=c
            if frame==0:reference=history[:,:,:3].copy()
        temporal_counts.append(relative_changes([reference,history[:,:,:3]]))
    check('adaptive_motion_temporal_8frames',temporal_counts[1]<=temporal_counts[0],
          before_pixels_over_5pct=temporal_counts[0],after_pixels_over_5pct=temporal_counts[1])
    # Full-res rim shader draw comes from the production C++ routine's trace.
    rect=scissor_rect(host,outside,True)
    rim,raw,_=after.run(outside,half=True,steps=0,rim=1,rect=rect)
    full,_,_=after.run(outside,steps=0,rim=1)
    check('real_full_res_rim',np.max(full[:,:,:3])>0 and np.array_equal(rim[:,:,:3],full[:,:,:3]),
          lit_pixels=int(np.count_nonzero(rim[:,:,:3].max(2)>0)))
    cmds=host_request(host,'R',[1,*rect,args.width//2,args.height//2,args.width,args.height,1])
    sc=[list(map(int,c[1:])) for c in cmds if c[0]=='scissor'][0]
    expected=[int(np.floor(rect[0]*args.width/(args.width//2))),int(np.floor(rect[1]*args.height/(args.height//2))),
              int(np.ceil((rect[0]+rect[2])*args.width/(args.width//2))-np.floor(rect[0]*args.width/(args.width//2))),
              int(np.ceil((rect[1]+rect[3])*args.height/(args.height//2))-np.floor(rect[1]*args.height/(args.height//2)))]
    check('rim_scaled_scissor',sc==expected and sc[2]*sc[3]<args.width*args.height,rect=sc,
          shaded_fraction=sc[2]*sc[3]/(args.width*args.height))
    cmds=host_request(host,'R',[0,args.width//2,args.height//2,-args.width//2,-args.height//2,
                                 args.width//2,args.height//2,args.width,args.height,1])
    sc=[list(map(int,c[1:])) for c in cmds if c[0]=='scissor'][0]
    check('invalid_bounds_scissor',sc==[0,0,args.width,args.height],rect=sc)
    halo,_,_=after.run(outside,half=True,steps=0,rim=1,bloom=True,rect=rect)
    check('rim_bloom_feed',np.count_nonzero(halo[:,:,:3])>0,
          before_lit_pixels=0,after_lit_pixels=int(np.count_nonzero(halo[:,:,:3].max(2)>0)))
    after.target('rim_scene_copy',args.width,args.height)
    bp=after.bloom_program;after._begin(bp)
    after.execute(bp,host_request(host,'F',[1,0,0,1,1]),{},'rim_copy_ms')
    copied=np.empty_like(full);gl.read(0,0,args.width,args.height,0x1908,0x1406,copied.ctypes.data)
    check('rim_scene_composited_once',np.array_equal(copied[:,:,:3],full[:,:,:3]),
          scene_rim_max=float(copied[:,:,:3].max()),direct_rim_max=float(full[:,:,:3].max()))
    # With the air interval deliberately empty, rim uses its own surface test.
    planes=outside[-1]['planes'].copy(); planes[0]=[0,0,1,-100]
    a,_,_=after.run(outside,steps=0,rim=1,air=1,planes_override=planes)
    b,_,_=before.run(outside,steps=0,rim=1,air=1,planes_override=planes)
    check('airborne_frustum_preserves_rim',np.max(a[:,:,:3])>0 and np.max(b[:,:,:3])==0,
          before_lit_pixels=int(np.count_nonzero(b[:,:,:3].max(2)>0)),after_lit_pixels=int(np.count_nonzero(a[:,:,:3].max(2)>0)))
    # Grazing continuous-wall motion, compared with a centered-depth control.
    centered=Runner(CenteredTree('WORKTREE'),gl)
    grazing=Runner(InstrumentedTree('WORKTREE',grazing=True),gl)
    base=fixtures(640,360)['inside']; errs=[]; olderrs=[]; depth_errors=[]
    for motion in (-.0002,0,.0002):
        f=grazing_fixture(base,motion)
        a,_,_=after.run(f,half=True,steps=32,shadows=False); term=after.termination.copy()
        c,_,_=centered.run(f,half=True,steps=32,shadows=False); ct=centered.termination.copy()
        b,_,_=grazing.run(f,half=True,steps=32,shadows=False)
        # Omit the true plane/sky boundary and outer clamp boundary.
        mask=(ct>1)&(ct<20);mask[:2]=False;mask[-2:]=False;mask[:,:2]=False;mask[:,-2:]=False
        depth_errors.append(float(np.max(np.abs(term-ct)[mask])))
        mask=np.repeat(np.repeat(mask,2,0),2,1)
        errs.append(float(np.mean(np.abs(a[:,:,:3]-c[:,:,:3])[mask])))
        olderrs.append(float(np.mean(np.abs(b[:,:,:3]-c[:,:,:3])[mask])))
    check('grazing_wall_motion',max(depth_errors)<1e-4 and max(errs)<max(olderrs)*.1,
          before_mae=olderrs,after_mae=errs,termination_max_error=depth_errors)
    for angle in (14,140):
        for pose,f in fixtures(320,180,angle).items():
            r=scissor_rect(host,f,True)
            _,a,_=after.run(f,half=True,rim=1)
            _,b,_=after.run(f,half=True,rim=1,rect=r)
            check(f'production_pyramid/{angle}/{pose}',a.tobytes()==b.tobytes(),rect=r)
    # An offset projector eye exercises the production centre-depth/cap math.
    f=fixtures(320,180)['outside'];cam,g,z,n,lg=f;lg=lg.copy()
    lc=S.Camera(lg['pos']+.4*lg['dir'],lg['pos']+lg['dir']*6,768,768,vfov=42,near=.15,far=12)
    bias=np.eye(4);bias[:3,:3]*=.5;bias[:3,3]=.5;vp=lc.proj@lc.view
    lg.update(view=lc.view,matrix=bias@vp,scale=(.3*np.tan(lg['fov']/2),)*2+(1.1,),
              planes=np.array([vp[3]+vp[0],vp[3]-vp[0],vp[3]+vp[1],vp[3]-vp[1],vp[3]+vp[2],vp[3]-vp[2]]))
    f=cam,g,z,n,lg;r=scissor_rect(host,f,True)
    _,a,_=after.run(f,half=True,rim=1,shadows=False)
    _,b,_=after.run(f,half=True,rim=1,shadows=False,rect=r)
    check('production_pyramid/offset_eye',a.tobytes()==b.tobytes(),rect=r)
    # Exact even mapping is a structural contract; upsample now reads cached
    # distance and cannot independently round the full/half coordinate ratio.
    shader=(G.REPO/G.SHADERS/SHAFT).read_text(); up=(G.REPO/G.SHADERS/UP).read_text()
    check('exact_half_depth_mapping','ivec2 base = p * 2;' in shader and 'vec2(res) / projvol_half_res' not in shader and
          'texelFetch(lightMap, p, 0).r' in up and 'vec2(res) / projvol_half_res' not in up)
    pp=np.arange(8198//2,dtype=np.int32)
    floating=(pp.astype(np.float32)*np.float32(8198)/np.float32(8198//2)).astype(np.int32)
    check('exact_mapping_roundoff_control',np.any(floating!=pp*2),
          width=8198,before_misaddressed_texels=int(np.count_nonzero(floating!=pp*2)),after_misaddressed_texels=0)
    check('cached_termination_cost','getPositionWithDepth' not in up and 'texelFetch(depthMap' not in up,
          before_depth_fetches=17,after_depth_fetches=1,after_cached_fetches=4)
    after.run(outside,half=True); one_depth=after.termination.copy()
    after.run(outside,half=True,layers=2); two_depth=after.termination.copy()
    check('overlapping_termination_replaces',np.array_equal(one_depth,two_depth),
          layers=2,max_depth_error=float(np.max(np.abs(one_depth-two_depth))))
    # A second overlapping draw must replace termination, while RGB adds.
    # Runner uses the same blend indices as production; source wiring prevents
    # silently dropping the attachment or replacement blend in the viewer.
    pipe=(G.REPO/'indra/newview/pipeline.cpp').read_text()
    check('termination_mrt_wiring','mProjVolHalf.addColorAttachment(GL_R32F)' in pipe and
          'if (halfres) glBlendFunci(1, GL_ONE, GL_ZERO);' in pipe and
          'mProjVolHalf.bindTexture(1, depth_ch' in pipe and 'if (!halfres) glDrawBuffer(GL_COLOR_ATTACHMENT0);' in pipe)
    startup=(G.REPO/'indra/newview/llviewershadermgr.cpp').read_text()
    check('startup_explicit_off_variant','addPermutation("PROJVOL_INTEGRATION_AB", "0")' in startup,
          before_off_cache_keys=['absent','0'],after_off_cache_keys=['0'])
    for name in ('before','after'):
        runner=before if name=='before' else after
        measure('round1_cost/'+name,runner,outside,half=True,steps=24)
    old_shader=G.SourceTree(ROUND1).read(G.SHADERS+'/'+SHAFT)
    old_up=G.SourceTree(ROUND1).read(G.SHADERS+'/'+UP)
    old_startup=G.SourceTree(ROUND1).read('indra/newview/llviewershadermgr.cpp')
    check('before_rejected/exact_mapping','ivec2 base = p * 2;' not in old_shader)
    check('before_rejected/cached_depth','texelFetch(depthMap' in old_up)
    check('before_rejected/startup_key','addPermutation("PROJVOL_INTEGRATION_AB", "0")' not in old_startup)
    # Mutation sensitivity runs the exact same shared routines in isolated hosts.
    # No source files are mutated, and every altered behavior must be detected.
    for mutation in ('adaptive','rim','rim_scissor','rim_feed','dither','pyramid'):
        out=args.out/('mutation_'+mutation);out.mkdir(exist_ok=True)
        mh=build_scissor_host(out,args.glm_include,mutation)
        if mutation=='adaptive':
            vals=[production_bounds(mh,at(crossing+d),True)[5] for d in (-.00001,.00001)]
            killed=vals[0]!=vals[1]
        elif mutation=='rim':
            saved=after.host;after.host=mh
            im,_,_=after.run(outside,half=True,steps=0,rim=1,rect=rect)
            after.host=saved;killed=np.max(im[:,:,:3])==0
        elif mutation=='rim_feed':
            saved=after.host;after.host=mh
            im,_,_=after.run(outside,half=True,steps=0,rim=1,bloom=True,rect=rect)
            after.host=saved;killed=np.max(im[:,:,:3])==0
        elif mutation=='rim_scissor':
            vals=host_request(mh,'R',[1,*rect,args.width//2,args.height//2,args.width,args.height,1])
            mutated=[list(map(int,c[1:])) for c in vals if c[0]=='scissor'][0]
            killed=mutated!=expected
        elif mutation=='dither':
            saved=after.host;after.host=mh
            _,ra,_=after.run(outside,half=True,dither=1,frame=1)
            _,rb,_=after.run(outside,half=True,dither=1,frame=2)
            after.host=saved;killed=ra.tobytes()!=rb.tobytes()
        else:
            bad=scissor_rect(mh,outside,True)
            _,ra,_=after.run(outside,half=True)
            _,rb,_=after.run(outside,half=True,rect=bad)
            killed=ra.tobytes()!=rb.tobytes()
        mh.stdin.close();assert mh.wait(timeout=5)==0
        check('mutation_killed/'+mutation,killed)


def sheet(path, panels):
    h,w = panels[0][1].shape[:2]
    out = Image.new('RGB',(w*len(panels),h+28),'#171923'); draw = ImageDraw.Draw(out)
    for i,(label,im) in enumerate(panels):
        rgb = np.clip(im[:,:,:3]*3,0,None); rgb = rgb/(1+rgb)
        pix = np.uint8(np.clip(rgb**(1/2.2)*255,0,255))
        out.paste(Image.fromarray(pix[::-1]),(i*w,28)); draw.text((i*w+8,8),label,fill='white')
    out.save(path)


def border_plane_fixture(fixture, edge, motion):
    """An affine window-depth plane grazing each viewport edge, in motion."""
    cam,_,_,_,light=fixture
    delta=(cam.r if edge in ('left','right') else cam.u)*motion
    c=S.Camera(cam.eye+delta,cam.eye+cam.f+delta,cam.W,cam.H,far=1000)
    y,x=np.mgrid[:c.H,:c.W]
    inward={'left':(x+.5)/c.W,'right':1-(x+.5)/c.W,
            'bottom':(y+.5)/c.H,'top':1-(y+.5)/c.H}[edge]
    zf=(1+motion)/(.004+.5*inward)
    g=dict(zf=zf,nrm=np.broadcast_to(-c.f,(c.H,c.W,3)).copy(),surf=np.zeros((c.H,c.W),np.int32))
    _,depth,norm=S.encode_gbuffer(c,g)
    return c,g,depth,norm,light


def round2_checks(args,gl,host,runners,report):
    def check(name,passed,**kw):
        report['checks'].append(dict(name='round2/'+name,passed=bool(passed),**kw))
        print('round2/'+name,'PASS' if passed else 'FAIL',kw,flush=True)
    old_out=args.out/'round2_before'; old_out.mkdir(exist_ok=True)
    oldhost=build_scissor_host(old_out,args.glm_include,ref=ROUND2)
    old=Runner(G.SourceTree(ROUND2),gl); after=runners['after']
    centered=Runner(CenteredTree('WORKTREE'),gl)
    scenes=fixtures(args.width,args.height); outside=scenes['outside']
    pipe=(G.REPO/'indra/newview/pipeline.cpp').read_text()
    body=pipe[pipe.index('void LLPipeline::renderProjectorVolumetric('):pipe.index('void LLPipeline::feedProjectorVolumetricBloom(')]

    # Claim 3: execute the actual march's include-rim expression from C++ with
    # the actual pyramid geometry. Then replay separate march/rim scissors on GL.
    def rect(v):
        x,y,z,w=map(int,v[1:5]);return x,y,max(0,z-x),max(0,w-y)
    cap=3
    before=production_bounds(oldhost,outside,True,cap=cap,op='C')
    capped=production_bounds(host,outside,True,cap=cap,op='C')
    uncapped=production_bounds(host,outside,True,cap=cap,rim=True)
    area=lambda v:rect(v)[2]*rect(v)[3]
    ref,_,_=after.run(outside,half=True,rim=1,cap=cap)
    im,_,_=after.run(outside,half=True,rim=1,cap=cap,rect=rect(capped),rim_rect=rect(uncapped))
    independent=('rim_bounds_valid, rim_bounds' in body and
                 body.index('rim_draws.push_back(')<body.index('if (max_x <= min_x || max_y <= min_y)') and
                 'if (halfres && mProjVolRimValid) mProjVolHalfValid = true;' in body)
    check('capped_march_separate_rim',area(capped)<area(uncapped) and np.array_equal(im,ref) and independent,
          before_fragments=area(before),after_fragments=area(capped),rim_fragments=area(uncapped),bytes_equal=im.tobytes()==ref.tobytes())
    check('before_rejected/march_cap',area(before)==area(uncapped) and area(capped)<area(before))
    inside=scenes['inside']; m=production_bounds(host,inside,True,cap=1,op='C'); r=production_bounds(host,inside,True,cap=1)
    ref,_,_=after.run(inside,steps=0,rim=1,cap=1)
    im,_,_=after.run(inside,half=True,steps=0,rim=1,cap=1,rect=rect(m),rim_rect=rect(r))
    check('empty_capped_march_preserves_rim',area(m)==0 and np.max(ref[:,:,:3])>0 and np.array_equal(im,ref),
          march_rect=rect(m),rim_rect=rect(r),lit_pixels=int(np.count_nonzero(im[:,:,:3].max(2))))

    # Claim 7: bind()/unbind() in the viewer resets the VB binding, including
    # when both programs use the same position attribute. Trace the production
    # helper instead of relying on Runner._begin(), which ordinarily hides it.
    def rim_buffer_ok(h):
        cmds=host_request(h,'F',[1,0,0,1,1]); ops=[c[0] for c in cmds]
        return 'buffer' in ops and ops.index('buffer')<ops.index('draw')
    shader_cpp=(G.REPO/'indra/llrender/llglslshader.cpp').read_text()
    bind=shader_cpp[shader_cpp.index('void LLGLSLShader::bind()'):shader_cpp.index('void LLGLSLShader::bind(U8')]
    before_buffer=rim_buffer_ok(oldhost); after_buffer=rim_buffer_ok(host)
    check('rim_composite_vertex_buffer',after_buffer and 'LLVertexBuffer::unbind();' in bind,
          before_bound=before_buffer,after_bound=after_buffer)
    check('before_rejected/rim_buffer',not before_buffer and after_buffer)

    # Claim 9: densely sweep a camera-plane crossing with an off-axis sphere.
    # Check raw coverage as well as the quantized integer step budget. New
    # bounds must enclose every old valid bound throughout the transition.
    cam=outside[0]; h=args.height//2; w=args.width//2
    def sphere(hst,closest):
        return host_request(hst,'A',[w,h,1,0,1.5,-1-closest,*cam.proj.T.ravel()])
    closest=np.unique(np.r_[np.linspace(-.05,.6,1001),.0001-1e-6,.0001+1e-6,.25-1e-6,.25+1e-6,.5-1e-6,.5+1e-6])
    prev=np.array([sphere(oldhost,d) for d in closest]); now=np.array([sphere(host,d) for d in closest])
    def coverage(v):return np.where(v[:,0]>0,np.clip((v[:,4]-v[:,2])/h,0,1),1)
    old_jump=float(np.max(np.abs(np.diff(coverage(prev))))); jump=float(np.max(np.abs(np.diff(coverage(now)))))
    valid=prev[:,0]>0
    conservative=bool(np.all(now[valid,1:3]<=prev[valid,1:3]+1e-3) and np.all(now[valid,3:5]>=prev[valid,3:5]-1e-3))
    continuous=jump<.01 and np.max(np.abs(np.diff(now[:,5])))<=1 and conservative
    check('continuous_near_plane_budget',continuous,before_max_coverage_jump=old_jump,after_max_coverage_jump=jump,
          before_max_step_jump=float(np.max(np.abs(np.diff(prev[:,5])))),after_max_step_jump=float(np.max(np.abs(np.diff(now[:,5])))),conservative=conservative)
    check('before_rejected/near_coverage',old_jump>.1 and continuous)

    # Claim 8: centered-depth control on a moving plane at ALL four edges.
    # Odd sizes intentionally bypass min/max selection; also run an even-sized
    # control there so this regression remains sensitive at the odd-size gate.
    captures={}; edge_failures={}; edge_success={}
    sizes=[(args.width,args.height)]
    if args.width%2 or args.height%2:sizes.append((args.width//2*2,args.height//2*2))
    for W,H in sizes:
        base=outside if (W,H)==(args.width,args.height) else fixtures(W,H)['outside']
        for edge in ('left','right','bottom','top'):
            old_errors=[]; errors=[]; panels=[]
            for motion in (-.0002,0,.0002):
                f=border_plane_fixture(base,edge,motion)
                old.run(f,half=True,steps=0); a=old.termination.copy()
                after.run(f,half=True,steps=0); b=after.termination.copy()
                centered.run(f,half=True,steps=0); c=centered.termination.copy()
                mask=np.zeros_like(c,dtype=bool)
                if edge=='left':mask[1:-1,0]=True
                elif edge=='right':mask[1:-1,-1]=True
                elif edge=='bottom':mask[0,1:-1]=True
                else:mask[-1,1:-1]=True
                old_errors.append(float(np.max(np.abs(a-c)[mask])))
                errors.append(float(np.max(np.abs(b-c)[mask])))
                key=f'{W}x{H}_{edge}_{motion}'
                captures[key+'_before']=a;captures[key+'_after']=b;captures[key+'_centered']=c
                # Bright residuals expose alternating border termination in the
                # before capture, with the same fixed scale for all motions.
                for label,v in (('before',a),('after',b)):
                    rgb=np.repeat(np.minimum(np.abs(v-c),10)[:,:,None],3,2)
                    panels.append((f'{label} {edge} {motion:+.4f}m',rgb))
            key=f'{W}x{H}/{edge}';edge_failures[key]=max(old_errors);edge_success[key]=max(errors)
            check('border_motion/'+key,max(errors)<1e-4,before_max_depth_error=old_errors,after_max_depth_error=errors)
            sheet(args.out/'images'/f'border_motion_{W}x{H}_{edge}.png',panels)
    np.savez_compressed(args.out/'border_motion_termination.npz',**captures)
    check('before_rejected/border_neighbours',max(edge_failures.values())>.01 and max(edge_success.values())<1e-4)

    # Refuted claims still get explicit contracts and relevant GL observations.
    after.run(outside,half=True,rect=(4,4,8,8)); term=after.termination.copy()
    mask=np.ones_like(term,dtype=bool);mask[4:12,4:12]=False
    rt=(G.REPO/'indra/llrender/llrendertarget.cpp').read_text()
    rtbind=rt[rt.index('void LLRenderTarget::bindTarget()'):rt.index('void LLRenderTarget::clear(')]
    clear=body[body.index('march_target->bindTarget();'):body.index('LLGLDepthTest depth(GL_FALSE);')]
    check('termination_clear_outside_scissor',np.all(term[mask]==0) and
          'LLGLDisable no_scissor(GL_SCISSOR_TEST);' in clear and 'gGL.setColorMask(true, true);' in clear and
          'march_target->clear(GL_COLOR_BUFFER_BIT);' in clear and 'if (!halfres) glDrawBuffer' in clear,
          outside_depth_max=float(np.max(term[mask])))
    check('draw_buffers_reset_on_bind','glDrawBuffers(static_cast<GLsizei>(mTex.size()), drawbuffers);' in rtbind)
    a,_,_=after.run(outside,steps=0,rim=1,cap=0)
    b,_,_=after.run(outside,steps=0,rim=1,cap=.1)
    setup=pipe[pipe.index('F32 LLPipeline::setupSpotLightVolumetric('):pipe.index('void LLPipeline::unbindDeferredShader(')]
    rimloop=body[body.index('for (const auto& rim : rim_draws)'):body.index('if (dust_on)',body.index('for (const auto& rim : rim_draws)'))]
    check('rim_batch_state_complete',np.array_equal(a,b) and
          'setupSpotLightVolumetric(' in rimloop and 'rim.power' in rimloop and 'rim.wrap' in rimloop and 'rim.softness' in rimloop and
          all(s in setup for s in ('PROJECTOR_MATRIX','PROJVOL_FRUSTUM_PLANES','PROJECTOR_SHADOW_INDEX','bindProjectorCookie')),
          shaft_cap_changes_rim=not np.array_equal(a,b))
    _,raw,_=after.run(outside,half=True,steps=0,rim=1)
    # The final contains the full-res rim, but the half-res march remains black.
    after.read_buffer(0x8CE0);gl.bind_fbo(0x8D40,after.fbo)
    gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,after.textures['march'][0],0)
    march=np.empty((args.height//2,args.width//2,4),np.float32)
    gl.read(0,0,args.width//2,args.height//2,0x1908,0x1406,march.ctypes.data)
    check('half_res_rim_zero_per_cone',np.max(march[:,:,:3])==0 and np.max(raw.view(np.float16)[:,:,:3])>0 and
          'PROJVOL_RIM_STRENGTH, halfres ? 0.f : llmax(e_rimStrength, 0.f)' in body)
    ext=gl.fn('glGetString',C.c_char_p,gl.U)(0x1F03).split()
    check('desktop_r32f_replacement_blend',after.error()==0 and 'OpenGL ES' not in gl.renderer,
          renderer=gl.renderer,EXT_float_blend=b'GL_EXT_float_blend' in ext)

    # Round-2 mutations execute the same C++/GLSL changes in isolation in memory.
    for mutation in ('march_cap','rim_buffer','near_coverage'):
        out=args.out/('mutation_'+mutation);out.mkdir(exist_ok=True)
        mh=build_scissor_host(out,args.glm_include,mutation)
        if mutation=='march_cap':killed=area(production_bounds(mh,outside,True,cap=cap,op='C'))>=area(uncapped)
        elif mutation=='rim_buffer':killed=not rim_buffer_ok(mh)
        else:
            v=np.array([sphere(mh,d) for d in (.0001-1e-6,.0001+1e-6)])
            killed=abs(coverage(v)[0]-coverage(v)[1])>.1
        mh.stdin.close();assert mh.wait(timeout=5)==0
        check('mutation_killed/'+mutation,killed)
    # Reverting the GLSL neighbour guards is the independently compiled ROUND2
    # shader above; all four even-sized edge fixtures must reject that mutation.
    check('mutation_killed/border_neighbours',all(v>.01 for k,v in edge_failures.items() if
          all(int(n)%2==0 for n in k.split('/')[0].split('x'))))
    oldhost.stdin.close();assert oldhost.wait(timeout=5)==0


# ---------------------------------------------------------------------------------------------------------
# Light > Projector Shafts looks gate (WP4). Executes the 13 shipped looks through the PRODUCTION shaft GLSL.
#
# The look table is parsed from indra/newview/alprojectorshaftlooks.cpp (the exact rows the viewer ships). Each
# look is resolved to a full setting state (settings.xml default for every owned key + the look's values) and
# mapped to the uniforms LLPipeline::renderProjectorVolumetric uploads, with the same clamps. The mapping is
# pinned to pipeline.cpp by source assertions, so a drifted upload line fails the gate instead of silently
# measuring the wrong thing. Dust renders with a SYNTHETIC seeded 64^3 volume (the real asset is not loaded by
# the harness); its cost is therefore indicative only (checked in-world with Tracy).
# ---------------------------------------------------------------------------------------------------------
LOOKS_SRC = 'indra/newview/alprojectorshaftlooks.cpp'
SETTINGS_XML = 'indra/newview/app_settings/settings.xml'
PV = 'BDMergeProjectorVolumetrics'
FIXTURE_BRIGHTNESS = 6.0                   # harness base for godray_multiplier: a look's Multiplier scales it
FIXTURE_COLOR = np.array((1.0, .8, .55))   # harness base light colour; TintStrength lerps it toward Tint
TIER_CEILING_MS = {'Low': 0.40, 'Med': 0.70, 'High': 1.20}   # D3: planner estimates; measured margins reported
LOOK_TIER = {'b:clean': 'Low', 'b:cinematic': 'Med', 'b:stage': 'Med', 'b:clear_night': 'Low',
             'b:searchlight': 'High', 'b:dust_storm': 'High', 'b:dream_mist': 'Med', 'b:silhouette': 'High',
             'b:pipe_fog': 'High', 'b:smoky_club': 'Med', 'b:cathedral': 'High', 'b:underwater': 'Med',
             'b:stained_glass': 'High'}
COST_REPEATS = 25                          # median of at least this many warmed draws for the pinned cost
AB_CANDIDATES = [('b:searchlight', 'equi'), ('b:pipe_fog', 'equi'), ('b:smoky_club', 'air'), ('b:stained_glass', 'footprint')]
MOTION_PIXEL_BOUND = 0.001                 # D3: relative-change (>5%) pixels per frame pair / lit pixels
LANE_CONTRAST_MIN = 0.35                   # D3
LANE_DRIFT_MAX = 0.08
CLAMP_FRACTION_MAX = 0.005
OWNED_EXPECTED = [PV + n for n in (
    'Multiplier', 'Density', 'Anisotropy', 'Tint', 'TintStrength', 'MaxLuminance', 'MaxDistance', 'Feather',
    'BloomFeed', 'BloomFeedAnamorphic', 'RimStrength', 'RimPower', 'RimWrap', 'RimThreshold', 'RimSoftness',
    'NoiseStrength', 'NoiseScale', 'NoiseSpeed', 'DustIntensity', 'DustScale', 'DustDrift', 'FogStrength',
    'FogGroundDensity', 'FogFalloff', 'ShadowSamples', 'ShadowTint', 'Resolution', 'Equiangular', 'AirborneTap',
    'CookieFootprint')] + ['BDMergeSoftProjectorShadows', 'BDMergeSoftShadowSoftness',
                           'BDMergeSoftShadowMaxPenumbra', 'BDMergeSoftShadowFill']
DUST_SETTING = PV + 'Dust'
# setting -> (uniform, pipeline.cpp source fragment that uploads it). A missing fragment means the map drifted.
UNIFORM_SOURCES = [
    ('godray_multiplier', 'LLShaderMgr::GODRAY_MULTIPLIER, e_mult'),
    ('projvol_g', 'LLShaderMgr::PROJVOL_G, e_g'),
    ('projvol_feather', 'LLShaderMgr::PROJVOL_FEATHER, e_feather'),
    ('projvol_density', 'LLShaderMgr::PROJVOL_DENSITY, llmax(e_density, 0.f)'),
    ('projvol_max_distance', 'sProjVolMaxDistance, llclamp(e_max_dist, 0.f, 64.f)'),
    ('projvol_max', 'LLShaderMgr::PROJVOL_MAX, BDMergeProjectorVolumetricsMaxLuminance'),
    ('projvol_shadow_samples', 'LLShaderMgr::PROJVOL_SHADOW_SAMPLES, (S32)llclamp(BDMergeProjectorVolumetricsShadowSamples, (U32)1, (U32)4)'),
    ('projvol_shadow_tint', 'LLShaderMgr::PROJVOL_SHADOW_TINT, llclamp(BDMergeProjectorVolumetricsShadowTint, 0.f, 1.f)'),
    ('projvol_noise_strength', 'LLShaderMgr::PROJVOL_NOISE_STRENGTH, llclamp(BDMergeProjectorVolumetricsNoiseStrength, 0.f, 1.f)'),
    ('projvol_noise_scale', 'LLShaderMgr::PROJVOL_NOISE_SCALE, llmax(BDMergeProjectorVolumetricsNoiseScale, 0.f)'),
    ('projvol_noise_speed', 'LLShaderMgr::PROJVOL_NOISE_SPEED, BDMergeProjectorVolumetricsNoiseSpeed'),
    ('projvol_fog_strength', 'LLShaderMgr::PROJVOL_FOG_STRENGTH, llclamp(BDMergeProjectorVolumetricsFogStrength, 0.f, 1.f)'),
    ('projvol_fog_ground_density', 'LLShaderMgr::PROJVOL_FOG_GROUND, llmax(BDMergeProjectorVolumetricsFogGroundDensity, 0.f)'),
    ('projvol_fog_falloff', 'LLShaderMgr::PROJVOL_FOG_FALLOFF, llmax(BDMergeProjectorVolumetricsFogFalloff, 0.01f)'),
    ('projvol_fog_base', 'LLShaderMgr::PROJVOL_FOG_BASE, BDMergeProjectorVolumetricsFogBase'),
    ('projvol_rim_strength', 'LLShaderMgr::PROJVOL_RIM_STRENGTH, halfres ? 0.f : llmax(e_rimStrength, 0.f)'),
    ('projvol_rim_power', 'LLShaderMgr::PROJVOL_RIM_POWER, llmax(e_rimPower, 0.01f)'),
    ('projvol_rim_wrap', 'LLShaderMgr::PROJVOL_RIM_WRAP, llclamp(e_rimWrap, 0.f, 1.f)'),
    ('projvol_rim_softness', 'LLShaderMgr::PROJVOL_RIM_SOFTNESS, llclamp(e_rimSoftness, 0.f, 1.f)'),
    ('projvol_rim_threshold', 'LLShaderMgr::PROJVOL_RIM_THRESHOLD, llmax(BDMergeProjectorVolumetricsRimThreshold, 0.f)'),
    ('projvol_equiangular', 'sEquiangular, equiangular() ? 1 : 0'),
    ('projvol_airborne_tap', 'sAirborneTap, airborne_tap() ? 1 : 0'),
    ('projvol_cookie_footprint', 'sCookieFootprint, cookie_footprint() ? 1 : 0'),
    ('color (tint lerp)', 'col = col * (1.f - t) + e_tint * (ls * t);'),
    ('soft_shadow_enable', 'LLShaderMgr::SOFT_SHADOW_ENABLE, BDMergeSoftProjectorShadows ? 1 : 0'),
    ('soft_shadow_scale', 'LLShaderMgr::SOFT_SHADOW_SCALE, BDMergeSoftShadowSoftness'),
    ('soft_shadow_max', 'LLShaderMgr::SOFT_SHADOW_MAX, llmax(BDMergeSoftShadowMaxPenumbra, 1.f)'),
    ('soft_shadow_fill', 'LLShaderMgr::SOFT_SHADOW_FILL, llclamp(BDMergeSoftShadowFill, 0.f, 1.f)'),
    ('projvol_dust_intensity', 'dust_intensity = llfinite(dust_intensity) ? llclamp(dust_intensity, 0.f, 2.f) : 0.f;'),
    ('projvol_dust_scale', 'dust_scale = llfinite(dust_scale) ? llclamp(dust_scale, 0.f, 64.f) : 0.5f;'),
    ('projvol_dust_wind', 'LLShaderMgr::PROJVOL_DUST_WIND, wx, wy, wz'),
    ('projvol_bloom_feed', 'LLShaderMgr::PROJVOL_BLOOM_FEED, BDMergeProjectorVolumetricsBloomFeed'),
    ('projvol_bloom_anamorphic', 'LLShaderMgr::PROJVOL_BLOOM_ANAMORPHIC, llmax(BDMergeProjectorVolumetricsBloomFeedAnamorphic, 1.f)'),
]


def parse_looks():
    """Parse the shipped look table (one array per look, one row per line, then the Look table)."""
    src = (G.REPO/LOOKS_SRC).read_text(encoding='utf-8')
    arrays = {}
    for m in re.finditer(r'static const Value (k\w+)\[\] = \{\n(.*?)\n\};', src, re.S):
        lines = [l for l in m.group(2).split('\n') if l.strip()]
        rows = []
        for line in lines:
            r = re.fullmatch(r'\s*\{"(\w+)", Type::(F32|U32|BOOL|COLOR3), \{([^}]*)\}\},', line)
            assert r, 'look row format drifted (one {"Setting", Type::X, {v}}, per line): ' + line
            rows.append((r.group(1), r.group(2), tuple(float(x.strip().rstrip('f')) for x in r.group(3).split(','))))
        arrays[m.group(1)] = rows
    looks = []
    for m in re.finditer(r'\{"(b:\w+)", "([^"]+)", "((?:[^"\\]|\\.)*)", (k\w+), sizeof', src):
        looks.append(dict(id=m.group(1), label=m.group(2), description=m.group(3), rows=arrays[m.group(4)]))
    owned_block = src[src.index('static const char* const kOwned[] = {'):]
    owned = re.findall(r'"(BDMerge\w+)"', owned_block[:owned_block.index('};')])
    return dict(looks=looks, owned=owned)


def settings_defaults():
    text = (G.REPO/SETTINGS_XML).read_text(encoding='utf-8')
    out = {}
    for m in re.finditer(r'<key>(BDMergeProjectorVolumetrics\w*|BDMergeSoft\w+|RenderSpotShadowBias|RenderSpotShadowOffset)</key>\s*<map>(.*?)</map>', text, re.S):
        body = m.group(2)
        typ = re.search(r'<key>Type</key>\s*<string>(\w+)</string>', body).group(1)
        val = re.search(r'<key>Value</key>\s*(.*)$', body, re.S).group(1)
        out[m.group(1)] = (typ, tuple(float(x) for x in re.findall(r'<(?:real|integer)>([^<]+)</', val)))
    return out


def look_state(look, defaults):
    state = {k: v[1] for k, v in defaults.items()}
    for setting, typ, vals in look['rows']:
        assert setting in state, setting
        state[setting] = vals if typ == 'COLOR3' else (vals[0],)
    return state


def look_spec(state):
    """setting state -> (run kwargs, uniform overrides). Same clamps as LLPipeline::renderProjectorVolumetric."""
    f = lambda name: state[PV+name][0]
    clamp = lambda x, lo, hi: min(max(x, lo), hi)
    t = clamp(f('TintStrength'), 0, 1)
    color = FIXTURE_COLOR*(1-t) + np.array(state[PV+'Tint'])*t if f('TintStrength') > 0 else FIXTURE_COLOR
    dust = bool(f('Dust')) and clamp(f('DustIntensity'), 0, 2) > 0
    uniforms = {
        'godray_multiplier': FIXTURE_BRIGHTNESS*f('Multiplier'), 'projvol_g': f('Anisotropy'),
        'projvol_density': max(f('Density'), 0.), 'projvol_feather': f('Feather'),
        'color': tuple(float(c) for c in color),
        'projvol_noise_strength': clamp(f('NoiseStrength'), 0, 1), 'projvol_noise_scale': max(f('NoiseScale'), 0.),
        'projvol_noise_speed': f('NoiseSpeed'),
        'projvol_fog_strength': clamp(f('FogStrength'), 0, 1), 'projvol_fog_ground_density': max(f('FogGroundDensity'), 0.),
        'projvol_fog_falloff': max(f('FogFalloff'), .01), 'projvol_fog_base': f('FogBase'),
        'projvol_shadow_tint': clamp(f('ShadowTint'), 0, 1), 'projvol_shadow_samples': int(clamp(f('ShadowSamples'), 1, 4)),
        'projvol_rim_power': max(f('RimPower'), .01), 'projvol_rim_wrap': clamp(f('RimWrap'), 0, 1),
        'projvol_rim_softness': clamp(f('RimSoftness'), 0, 1), 'projvol_rim_threshold': max(f('RimThreshold'), 0.),
        'soft_shadow_scale': state['BDMergeSoftShadowSoftness'][0],
        'soft_shadow_max': max(state['BDMergeSoftShadowMaxPenumbra'][0], 1.),
        'soft_shadow_fill': clamp(state['BDMergeSoftShadowFill'][0], 0, 1),
        'spot_shadow_bias': state['RenderSpotShadowBias'][0],
        'spot_shadow_offset': state['RenderSpotShadowOffset'][0],
        'projvol_time': 0.0,   # frozen: the medium is judged on geometry stability, not on its animation
    }
    if dust:
        drift = clamp(f('DustDrift'), -10, 10)
        uniforms.update(projvol_dust=1, projvol_dust_intensity=clamp(f('DustIntensity'), 0, 2),
                        projvol_dust_scale=clamp(f('DustScale'), 0, 64), projvol_dust_wind=(0., -drift, 0.))
    kwargs = dict(half=True, steps=int(f('Resolution')), maximum=f('MaxLuminance'), equi=int(f('Equiangular')),
                  air=int(f('AirborneTap')), footprint=int(f('CookieFootprint')),
                  soft=bool(state['BDMergeSoftProjectorShadows'][0]), cap=clamp(f('MaxDistance'), 0, 64),
                  rim=max(f('RimStrength'), 0.), dither=1, beam=1)
    return dict(kwargs=kwargs, uniforms=uniforms, dust=dust, bloom=(f('BloomFeed'), max(f('BloomFeedAnamorphic'), 1.)),
                resolution=int(f('Resolution')))


def strip_medium(spec):
    """The same look with the participating medium (noise, ground fog, dust) off: isolates geometry from the medium."""
    s = dict(spec, uniforms=dict(spec['uniforms']))
    s['uniforms'].update(projvol_noise_strength=0., projvol_fog_strength=0.)
    if 'projvol_dust' in s['uniforms']:
        s['uniforms']['projvol_dust'] = 0   # uniforms persist in the program: switch the dust gate off explicitly
    return s


def has_medium(state):
    return state[PV+'NoiseStrength'][0] > 0 or state[PV+'FogStrength'][0] > 0 or (
        state[DUST_SETTING][0] > 0 and state[PV+'DustIntensity'][0] > 0)


def synthetic_dust_volume(gl):
    """Seeded tileable 64^3 RGBA8 (R macro haze, G sparse motes, B turbulence, A phase). SYNTHETIC: not the shipped asset."""
    rs = np.random.RandomState(0xD057C1A5 & 0x7fffffff)
    n = 64

    def tile(coarse):
        v = rs.rand(coarse, coarse, coarse).astype(np.float32)
        out = v
        for axis in range(3):
            idx = (np.arange(n)*coarse/n)
            i0 = np.floor(idx).astype(int) % coarse
            i1 = (i0+1) % coarse
            w = idx-np.floor(idx)
            w = (w*w*(3-2*w)).astype(np.float32)
            shape = [1, 1, 1]; shape[axis] = n
            a = np.take(out, i0, axis=axis); b = np.take(out, i1, axis=axis)
            out = a*(1-w.reshape(shape))+b*w.reshape(shape)
        return out
    vol = np.zeros((n, n, n, 4), np.uint8)
    vol[..., 0] = np.uint8(np.clip(tile(8)*255, 0, 255))
    vol[..., 1] = np.uint8(np.clip(rs.beta(.5, 4, (n, n, n))*255*2.2, 0, 255))
    vol[..., 2] = np.uint8(np.clip(tile(16)*255, 0, 255))
    vol[..., 3] = np.uint8(rs.randint(0, 256, (n, n, n)))
    t = gl.U(); gl.gen_tex(1, C.byref(t))
    gl.active(0x84C6); gl.bind_tex(0x806F, t)
    for p, v in ((0x2801, 0x2601), (0x2800, 0x2601), (0x2802, 0x2901), (0x2803, 0x2901), (0x8072, 0x2901)):
        gl.param(0x806F, p, v)
    gl.fn('glTexImage3D', None, gl.U, gl.I, gl.I, gl.I, gl.I, gl.I, gl.I, gl.U, gl.U, gl.P)(
        0x806F, 0, 0x8058, n, n, n, 0, 0x1908, 0x1401, vol.ctypes.data)
    return t, int(np.count_nonzero(vol[..., 1] > 199))


class DustRunner(Runner):
    """A runner whose shaft program was compiled with PROJVOL_DUST_ENABLE; binds the synthetic volume to unit 6."""
    def run(self, *a, **k):
        self.gl.active(0x84C6); self.gl.bind_tex(0x806F, self.volume)
        return super().run(*a, **k)


class LookRig:
    def __init__(self, gl, host):
        self.gl, self.host, self.sets = gl, host, {}
        self.volume, self.mote_texels = synthetic_dust_volume(gl)

    def runners(self, dust):
        if dust not in self.sets:
            made = {}
            for name, tree in (('after', G.SourceTree('WORKTREE')), ('centered', CenteredTree('WORKTREE')),
                               ('grazing', InstrumentedTree('WORKTREE', grazing=True))):
                saved = G.PREAMBLE
                try:
                    if dust:
                        G.PREAMBLE += '\n#define PROJVOL_DUST_ENABLE 1\n'
                    r = Runner(tree, self.gl)
                finally:
                    G.PREAMBLE = saved
                if dust:
                    r.__class__ = DustRunner; r.volume = self.volume
                r.host = self.host
                made[name] = r
            self.sets[dust] = made
        return self.sets[dust]


def shaft_changes(a, b):
    """(pixels whose colour changed >5% relative, lit pixels) between two frames (same rule as the adaptive motion gates)."""
    mask = a[:, :, :3].max(2) > 1e-4
    changed = mask & (np.max(np.abs(b[:, :, :3]-a[:, :, :3]), 2) > .05*np.maximum(a[:, :, :3].max(2), 1e-4))
    return int(np.count_nonzero(changed)), int(np.count_nonzero(mask))


def micro_frames(fixture, count=9, dolly=0.001, yaw_deg=0.02):
    """Micro-dolly: 1 mm along the view axis plus 0.02 degrees of yaw per frame."""
    cam, _, _, _, light = fixture
    out = []
    for k in range(count):
        a = np.deg2rad(yaw_deg*k); c, s = np.cos(a), np.sin(a)
        f = np.array([cam.f[0]*c-cam.f[1]*s, cam.f[0]*s+cam.f[1]*c, cam.f[2]])
        eye = cam.eye+cam.f*(dolly*k)
        cm = S.Camera(eye, eye+f, cam.W, cam.H)
        g = S.cast_primary(light['scene'], cm); _, d, n = S.encode_gbuffer(cm, g)
        out.append((cm, g, d, n, light))
    return out


def production_steps(host, fixture, max_res, half=True):
    """(sphere steps, pyramid steps) the production adaptive rule picks for this camera with Resolution = max_res."""
    cam, _, _, _, light = fixture
    w, h = (cam.W//2, cam.H//2) if half else (cam.W, cam.H)
    rotation = np.eye(4); rotation[:3, :3] = np.linalg.inv(light['view'])[:3, :3]
    scale = light.get('scale', (.3*np.tan(light['fov']/2),)*2+(.3,))
    values = [w, h, light['fov'], 12, 0, 0, *light.get('sphere_pos', light['pos']), *scale,
              *rotation.T.ravel(), *cam.view.T.ravel(), *cam.proj.T.ravel(), 6, max_res]
    v = host_request(host, 'E', values)
    return int(v[5]), int(v[6])


def render_look(runner, fixture, spec, **over):
    kw = dict(spec['kwargs']); kw.update(over)
    return runner.run(fixture, uniforms=spec['uniforms'], **kw)


def temporal_sequence(runner, frames, spec, budgets, preroll=0):
    """Viewer order: raw half-res march, resolve, retain history. Include every moving pair.

    A stationary pre-roll is a separate experiment, never a replacement for the
    cold/cut start. Screen-locked dither must survive the first move in BOTH cases.
    """
    cam = frames[0][0]
    history = np.zeros((cam.H//2, cam.W//2, 4), np.float32)
    previous = cam; valid = False; result = []
    for k in [0]*preroll + list(range(len(frames))):
        f = frames[k]
        render_look(runner, f, spec, steps=budgets[k][0], frame=k)
        history = runner.temporal_resolve(f, history, previous, valid)
        previous = f[0]; valid = True
        result.append(history[:, :, :3].copy())
    return result[preroll:]


def motion_fractions(images):
    return [float(c/max(n, 1)) for c, n in
            (shaft_changes(a, b) for a, b in zip(images, images[1:]))]


class UnfilteredHistoryTree(G.SourceTree):
    """Negative control: the original temporal rejection, before the first-move fix."""
    def read(self, path):
        src = super().read(path)
        if Path(path).name == 'projectorVolumetricTemporalF.glsl':
            guard = '        w *= 1.0 - smoothstep(0.01, 0.02, relative_delta);'
            assert guard in src
            src = src.replace(guard, '')
            src, count = re.subn(r'vec3 current_filtered = .*?;', 'vec3 current_filtered = cur;', src, flags=re.S)
            assert count == 1
        return src


class FlickeringRimTree(G.SourceTree):
    """Inject a real radiance pop to prove the surface stability check detects flicker."""
    def read(self, path):
        src = super().read(path)
        if Path(path).as_posix().endswith(SHAFT):
            line = '                shaft += rim;'
            assert line in src
            src = src.replace(line, '                rim *= 1.0 + 0.2 * mod(projvol_frame, 2.0);\n'+line)
        return src


def rim_surface_probes(runner, frames, spec):
    """Track the SAME visible world surfaces, instead of comparing moving silhouette coverage.

    A 3x3 camera aimed at each point evaluates that point at the exact centre
    pixel through production depth/normal decoding and rim GLSL. Only the eye
    moves. The 5% radiance and 0.1% changed-sample limits are unchanged from P2:
    <=144 probes means even one unstable probe fails. Analytic visibility removes
    only real disocclusions, independently of the measured radiance.
    """
    f = frames[0]; scene = f[4]['scene']
    rim = render_look(runner, f, spec, half=False, steps=0)[0]
    lit = luminance(rim) > 1e-4
    unshadowed = render_look(runner, f, spec, half=False, steps=0, shadows=False)[0]
    visibility = luminance(rim)/np.maximum(luminance(unshadowed), 1e-8)
    points = []
    for surf in np.unique(f[1]['surf'][lit]):
        if surf == 255:
            continue
        ids = np.flatnonzero(lit & (f[1]['surf'] == surf))
        chosen = ids[np.linspace(0, len(ids)-1, min(24, len(ids))).astype(int)]
        points.extend(f[1]['pos'].reshape(-1, 3)[chosen])
        # Include actual shadow boundaries, not just uniformly spaced lit points.
        # A camera-dependent PCF footprint is easiest to miss on fully lit surfaces.
        edge_ids = np.flatnonzero(lit & (f[1]['surf'] == surf) & (visibility > .02) & (visibility < .98))
        if len(edge_ids):
            chosen = edge_ids[np.linspace(0, len(edge_ids)-1, min(24, len(edge_ids))).astype(int)]
            points.extend(f[1]['pos'].reshape(-1, 3)[chosen])
    images = []; visibility = []
    for k, frame in enumerate(frames):
        values = []; visible = []
        for point in points:
            cam = S.Camera(frame[0].eye, point, 3, 3)
            g = S.cast_primary(scene, cam); _, depth, normal = S.encode_gbuffer(cam, g)
            probe = (cam, g, depth, normal, f[4])
            values.append(render_look(runner, probe, spec, half=False, steps=0, frame=k)[0][1, 1, :3])
            visible.append(np.linalg.norm(g['pos'][1, 1]-point) < 1e-3)
        images.append(values); visibility.append(visible)
    images = np.asarray(images)
    persistent = np.all(visibility, axis=0) & (images[0].max(1) > 1e-4)
    delta = np.max(np.abs(np.diff(images, axis=0)), 2)/np.maximum(images[:-1].max(2), 1e-4)
    measured = delta[:, persistent]
    return dict(probes=int(persistent.sum()), occluded_probes=int(len(points)-persistent.sum()),
                max_relative_change=float(measured.max()) if measured.size else 1.,
                max_pair_fraction=float((measured > .05).mean(1).max()) if measured.size else 1.)


def timed_look(runner, fixture, spec, repeats, **over):
    """Median GPU ms of march + upsample + rim + bloom-feed (warmup excluded). Returns (median stats dict, total)."""
    def once():
        _, _, stats = render_look(runner, fixture, spec, **over)
        runner.bloom_feed(fixture, spec['bloom'][0], spec['bloom'][1], stats)
        return stats
    once()
    runs = [once() for _ in range(repeats)]
    med = {k: float(np.median([r.get(k, 0) for r in runs])) for k in set().union(*runs)}
    totals = [sum(r.values()) for r in runs]
    return med, float(np.median(totals)), [float(min(totals)), float(max(totals))]


def backlit_fixture(w, h, occluders=True, bar_radius=.035, blocker_fraction=1.):
    """Reference-style fan: low projector aimed up behind seven thin vertical/diagonal bars.

    The camera looks toward the lamp from below the cone axis. Unlike looking
    straight down the cone, the upper rays enter the haze beyond the bars, so
    light in front of the occluders cannot fill all of their dark lanes.
    """
    prims = [S.Prim('floor', 'obb', c=(0, 3, -.2), h=(15, 15, .2)),
             S.Prim('wall', 'obb', c=(0, -1, 3), h=(15, .1, 3))]
    tops = []
    for i in range(7):
        x = (i-3)*.24
        dx = (0., .3, -.2)[i % 3]
        lamp = np.array([0.,0.,.18])
        top = lamp+(np.array([x+dx,1.2,1.8])-lamp)*blocker_fraction
        bottom = lamp+(np.array([x,1.2,.65])-lamp)*blocker_fraction
        tops.append(top)
        if occluders:
            prims.append(S.Prim('bar%d' % i, 'capsule', r=bar_radius*blocker_fraction, a=bottom, b=top))
    scene = S.Scene('backlit', prims, [], [], {}, np.empty((0, 3)))
    pos = np.array([0., 0., .18]); direction = S.unit([0., 1., 1.4])
    lc = S.Camera(pos, pos+direction*4, 1024, 1024, vfov=60, near=.1, far=12)
    lg = S.cast_primary(scene, lc)
    zf = np.where(np.isfinite(lg['zf']), lg['zf'], 12)
    z = (12+.1)/(12-.1)-2*12*.1/((12-.1)*zf)
    depth = np.ascontiguousarray(np.clip(z*.5+.5, 0, 1), np.float32)
    bias = np.eye(4); bias[:3, :3] *= .5; bias[:3, 3] = .5
    vp = lc.proj@lc.view
    planes = np.array([vp[3]+vp[0], vp[3]-vp[0], vp[3]+vp[1], vp[3]-vp[1], vp[3]+vp[2], vp[3]-vp[2]])
    light = dict(pos=pos, dir=direction, matrix=bias@vp, planes=planes, depth=depth, scene=scene, view=lc.view,
                 fov=np.deg2rad(60.), lane_tops=np.array(tops))
    cam = S.Camera((.2, 5., 1.4), (0., .1, 1.5), w, h)
    g = S.cast_primary(scene, cam); _, d, n = S.encode_gbuffer(cam, g)
    return cam, g, d, n, light


def luminance(im):
    return .2126*im[:, :, 0]+.7152*im[:, :, 1]+.0722*im[:, :, 2]


class LaneBand:
    """Radial columns above ALL bar tops, normalized by the same camera without shadows.

    Rays diverge from the projected lamp, so averaging ordinary image columns
    washes the fan out. Removing the unshadowed profile prevents beam falloff
    from passing. No opaque bar pixels participate in this air-only measurement.
    """
    def __init__(self, fixture, unshadowed):
        self.cam = cam = fixture[0]
        px, py, _ = cam.project(np.array([fixture[4]['pos']]))
        _, tops, _ = cam.project(fixture[4]['lane_tops'])
        self.j0 = int(tops.max()) + max(3, round(cam.H*.01))
        self.j1 = int(cam.H*.94)
        assert self.j1-self.j0 >= 20, 'air lane band above the bars is empty'
        rows = np.arange(self.j0, self.j1)
        slopes = np.linspace(-2.5, 2.5, 501)
        self.y = np.broadcast_to(rows, (len(slopes), len(rows)))
        self.x = px[0]+slopes[:, None]*(rows[None, :]-py[0])
        ref = self.samples(unshadowed)
        self.valid = (self.x > 1) & (self.x < cam.W-2) & (ref > .05*ref.max())
        self.cols = self.valid.sum(1) >= .8*len(rows)
        assert self.cols.sum() > 50, 'radial cone mask is empty'
        self.reference = self.column_mean(ref)

    def samples(self, im):
        # Map pixel centres by UV, including odd full/half target dimensions.
        h, w = im.shape[:2]
        x = (self.x+.5)*w/self.cam.W-.5
        y = (self.y+.5)*h/self.cam.H-.5
        x = np.clip(x, 0, w-1); y = np.clip(y, 0, h-1)
        x0 = x.astype(int); y0 = y.astype(int)
        x1 = np.minimum(x0+1, w-1); y1 = np.minimum(y0+1, h-1)
        fx = x-x0; fy = y-y0; lum = luminance(im)
        return (lum[y0, x0]*(1-fx)+lum[y0, x1]*fx)*(1-fy) + (lum[y1, x0]*(1-fx)+lum[y1, x1]*fx)*fy

    def column_mean(self, samples):
        return (samples*self.valid).sum(1)/np.maximum(self.valid.sum(1), 1)

    def profile(self, im, normalized=True):
        col = self.column_mean(self.samples(im))
        if normalized:
            col = col/np.maximum(self.reference, 1e-9)
        return col[self.cols]

    def contrast(self, im, scale=1, normalized=True):
        p10, p90 = np.percentile(self.profile(im, normalized), [10, 90])
        return float(1-p10/p90) if p90 > 0 else 0.

    def lane_count(self, im):
        col = self.profile(im)
        smooth = np.convolve(col, np.ones(5)/5, mode='valid')
        dark = smooth < .65*np.percentile(smooth, 90)
        return int(np.count_nonzero(dark[1:] & ~dark[:-1]) + bool(dark[0]))


def grid_sheet(path, tiles, cols=4, tile_w=480):
    th = int(tiles[0][1].shape[0]*tile_w/tiles[0][1].shape[1])
    rows = (len(tiles)+cols-1)//cols
    out = Image.new('RGB', (cols*tile_w, rows*(th+22)), '#171923'); draw = ImageDraw.Draw(out)
    for i, (label, im) in enumerate(tiles):
        rgb = np.clip(im[:, :, :3]*3, 0, None); rgb = rgb/(1+rgb)
        pix = np.uint8(np.clip(rgb**(1/2.2)*255, 0, 255))
        tile = Image.fromarray(pix[::-1]).resize((tile_w, th), Image.LANCZOS)
        x, y = (i % cols)*tile_w, (i//cols)*(th+22)
        out.paste(tile, (x, y+22)); draw.text((x+8, y+5), label, fill='white')
    out.save(path)


def looks_checks(args, gl, host, report):
    def check(name, passed, **kw):
        report['checks'].append(dict(name='looks/'+name, passed=bool(passed), **kw))
        print('looks/'+name, 'PASS' if passed else 'FAIL', kw, flush=True)
    images = args.out/'images'; images.mkdir(exist_ok=True)
    full_hd = (args.width, args.height) == (1920, 1080)
    cfg = parse_looks(); defaults = settings_defaults()
    # ---- table / ownership / mapping drift
    check('table_13_looks', len(cfg['looks']) == 13 and [l['id'] for l in cfg['looks']] == list(LOOK_TIER),
          ids=[l['id'] for l in cfg['looks']])
    check('owned_key_coverage', sorted(cfg['owned']) == sorted(OWNED_EXPECTED) and len(set(cfg['owned'])) == len(cfg['owned']),
          owned=len(cfg['owned']))
    allowed = set(OWNED_EXPECTED) | {DUST_SETTING}
    stray = sorted({s for l in cfg['looks'] for s, _, _ in l['rows'] if s not in allowed})
    check('look_keys_owned', not stray and all(k in defaults for k in allowed), stray=stray)
    pipe = (G.REPO/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    shader = (G.REPO/G.SHADERS/SHAFT).read_text(encoding='utf-8')
    shadow_util = (G.REPO/G.SHADERS/'class1/deferred/shadowUtil.glsl').read_text(encoding='utf-8')
    header = (G.REPO/'indra/llrender/llshadermgr.h').read_text(encoding='utf-8')
    missing = [u for u, frag in UNIFORM_SOURCES if frag not in pipe]
    check('uniform_map_matches_pipeline', not missing, missing=missing, mapped=len(UNIFORM_SOURCES))
    undeclared = [u for u, _ in UNIFORM_SOURCES if re.fullmatch(r'(projvol|soft_shadow|godray)\w*', u) and
                  not re.search(r'\buniform\s+\w+\s+%s\b' % u, shader+shadow_util) and u != 'projvol_bloom_feed' and
                  u != 'projvol_bloom_anamorphic']
    check('mapped_uniforms_declared_in_glsl', not undeclared, undeclared=undeclared)
    documented = dict(re.findall(r'(\w+),\s*//\s*"(\w+)"', header))
    wrong = [(c, documented.get(c)) for c, n in (('PROJVOL_DENSITY', 'projvol_density'), ('PROJVOL_G', 'projvol_g'),
             ('PROJVOL_SHADOW_TINT', 'projvol_shadow_tint'), ('PROJVOL_RIM_SOFTNESS', 'projvol_rim_softness'))
             if documented.get(c) != n]
    check('uniform_names_documented', not wrong, wrong=wrong)

    rig = LookRig(gl, host)
    # Compile the shared shadow module with BOTH production shadow paths present,
    # even though this fixture only draws projectors.
    shadow_probe = rig.runners(False)['after']
    saved_preamble = G.PREAMBLE
    try:
        G.PREAMBLE += '\n#undef SUN_SHADOW\n#define SUN_SHADOW 1\n'
        shadow_probe._compile(shadow_util, 'shared shadowUtil: SUN_SHADOW + SPOT_SHADOW')
    finally:
        G.PREAMBLE = saved_preamble
    check('shared_sun_spot_shadow_module_compiles', True)
    check('synthetic_dust_volume', rig.mote_texels > 0, mote_texels_over_0p78=rig.mote_texels,
          note='SYNTHETIC seeded 64^3 volume; dust cost is indicative only')
    t0 = time.time()
    scenes = fixtures(args.width, args.height)
    frames = {pose: micro_frames(scenes[pose]) for pose in ('inside', 'toward_apex')}
    graze_base = scenes['inside']
    print('looks: fixtures + micro frames %.1fs' % (time.time()-t0), flush=True)
    results = {}; tiles = []
    for look in cfg['looks']:
        lid = look['id']; t0 = time.time()
        state = look_state(look, defaults); spec = look_spec(state)
        rs = rig.runners(spec['dust']); after = rs['after']
        res = dict(id=lid, label=look['label'], tier=LOOK_TIER[lid], dust=spec['dust'],
                   ab=[k for k in ('equi', 'air', 'footprint') if spec['kwargs'][k]], resolution=spec['resolution'])
        # P1 static: two frames at one camera are byte-identical (recording-safe)
        p1 = {}
        for pose in ('inside', 'toward_apex'):
            _, a, _ = render_look(after, scenes[pose], spec, frame=1)
            _, b, _ = render_look(after, scenes[pose], spec, frame=2)
            p1[pose] = a.tobytes() == b.tobytes()
        check(lid+'/P1_static', all(p1.values()), poses=p1)
        # P2: every one of the eight transitions, both immediately after a cut
        # and after stationary viewer-order history. Rims have an independent
        # world-surface test so translating a silhouette is not called flicker.
        p2 = {}; rim_probes = {}
        for pose, fs in frames.items():
            budgets = [production_steps(host, f, spec['resolution']) for f in fs]
            sphere = [b[0] for b in budgets]
            imgs = [render_look(after, f, spec, steps=b[0], rim=0.)[0] for f, b in zip(fs, budgets)]
            pair = motion_fractions(imgs)
            rim_fraction = None
            if spec['kwargs']['rim'] > 0:
                rimmed = [render_look(after, f, spec, steps=b[0])[0] for f, b in zip(fs, budgets)]
                rim_fraction = max(motion_fractions(rimmed))
                rim_probes[pose] = rim_surface_probes(after, fs, spec)
            hist = temporal_sequence(after, fs, spec, budgets)
            warm = temporal_sequence(after, fs, spec, budgets, preroll=3)
            tpair = motion_fractions(hist); wpair = motion_fractions(warm)
            p2[pose] = dict(
                steps=sphere, max_step_delta=int(max(abs(sphere[i+1]-sphere[i]) for i in range(len(sphere)-1))),
                max_pair_fraction=max(pair), max_temporal_fraction=max(tpair),
                max_warm_temporal_fraction=max(wpair), temporal_pairs=tpair, warm_temporal_pairs=wpair,
                seed_pair_fraction=tpair[0], rim_pair_fraction=rim_fraction,
                lit_pixels=shaft_changes(imgs[0], imgs[1])[1])
        res['P2'] = p2
        check(lid+'/P2_motion', all(v['max_step_delta'] <= 1 and v['max_pair_fraction'] <= MOTION_PIXEL_BOUND and
                                    v['max_temporal_fraction'] <= MOTION_PIXEL_BOUND and
                                    v['max_warm_temporal_fraction'] <= MOTION_PIXEL_BOUND and
                                    (v['rim_pair_fraction'] is None or v['rim_pair_fraction'] <= MOTION_PIXEL_BOUND) and
                                    len(v['temporal_pairs']) == len(v['warm_temporal_pairs']) == 8 for v in p2.values()),
              bound=MOTION_PIXEL_BOUND, **{p+'_max_pair': round(v['max_pair_fraction'], 6) for p, v in p2.items()},
              **{p+'_max_temporal': round(v['max_temporal_fraction'], 6) for p, v in p2.items()},
              **{p+'_max_warm_temporal': round(v['max_warm_temporal_fraction'], 6) for p, v in p2.items()},
              **{p+'_max_step_delta': v['max_step_delta'] for p, v in p2.items()},
              **{p+'_seed_pair': round(v['seed_pair_fraction'], 6) for p, v in p2.items()})
        if rim_probes:
            res['rim_probes'] = rim_probes
            check(lid+'/P2_rim_surface_stability', all(v['probes'] >= 16 and v['max_pair_fraction'] <= MOTION_PIXEL_BOUND
                                                      for v in rim_probes.values()),
                  bound=MOTION_PIXEL_BOUND, screen_coverage_fraction={p: v['rim_pair_fraction'] for p, v in p2.items()},
                  probes=rim_probes)
            if lid == 'b:silhouette':
                flickering = Runner(FlickeringRimTree('WORKTREE'), gl)
                injected = rim_surface_probes(flickering, frames['inside'], spec)
                check('rim_injected_flicker_negative_control', injected['probes'] >= 16 and
                      injected['max_pair_fraction'] > MOTION_PIXEL_BOUND,
                      injected=injected, bound=MOTION_PIXEL_BOUND)
        if lid == 'b:clean':
            old = Runner(UnfilteredHistoryTree('WORKTREE'), gl)
            proof = {}
            for pose, fs in frames.items():
                budgets = [production_steps(host, f, spec['resolution']) for f in fs]
                cold = motion_fractions(temporal_sequence(old, fs, spec, budgets))
                warm = motion_fractions(temporal_sequence(old, fs, spec, budgets, preroll=3))
                proof[pose] = dict(cold_first=cold[0], stationary_preroll_first=warm[0])
            check('first_move_is_engine_artifact',
                  max(v['stationary_preroll_first'] for v in proof.values()) > MOTION_PIXEL_BOUND and
                  all(abs(v['cold_first']-v['stationary_preroll_first']) < 1e-5 for v in proof.values()),
                  original_shader=proof, bound=MOTION_PIXEL_BOUND,
                  note='Original shader fails after stationary pre-roll too; no transition may be excluded')
        # P3 termination crawl (geometry: the look's uniforms with the medium stripped), P4 medium crawl (full look)
        def crawl(sp):
            depth_err = []; mae = []; mutant = []
            for motion in (-2e-4, 0., 2e-4):
                f = grazing_fixture(graze_base, motion)
                a, _, _ = render_look(after, f, sp, shadows=False); term = after.termination.copy()
                c, _, _ = render_look(rs['centered'], f, sp, shadows=False); ct = rs['centered'].termination.copy()
                m, _, _ = render_look(rs['grazing'], f, sp, shadows=False)
                mask = (ct > 1) & (ct < 20); mask[:2] = False; mask[-2:] = False; mask[:, :2] = False; mask[:, -2:] = False
                depth_err.append(float(np.max(np.abs(term-ct)[mask])))
                full = np.zeros(a.shape[:2], bool)   # odd sizes: the half-res mask covers 2*floor(n/2) rows/columns
                up = np.repeat(np.repeat(mask, 2, 0), 2, 1)[:a.shape[0], :a.shape[1]]
                full[:up.shape[0], :up.shape[1]] = up
                mae.append(float(np.mean(np.abs(a[:, :, :3]-c[:, :, :3])[full])))
                mutant.append(float(np.mean(np.abs(m[:, :, :3]-c[:, :, :3])[full])))
            return depth_err, mae, mutant
        d3, m3, o3 = crawl(strip_medium(spec) if has_medium(state) else spec)
        res['P3'] = dict(termination_max_error=max(d3), mae=m3, mutant_mae=o3)
        check(lid+'/P3_termination', max(d3) < 1e-4, termination_max_error=d3)
        if has_medium(state):
            d4, m4, o4 = crawl(spec)
            res['P4'] = dict(termination_max_error=max(d4), mae=m4, mutant_mae=o4)
            check(lid+'/P4_medium', max(d4) < 1e-4 and (max(o4) < 1e-6 or max(m4) <= .1*max(o4)),
                  termination_max_error=d4, mae_after=m4, mae_without_fix=o4)
        # P5 cost on the inside pose (the ceiling); toward_apex recorded. Median of COST_REPEATS (>= the 9 asked for): on this
        # GPU a median of 9 wanders by +-4% between runs, a median of 25 by +-2%, which matters against a 0.70 ms ceiling.
        med_in, tot_in, spread_in = timed_look(after, scenes['inside'], spec, max(args.repeats, COST_REPEATS))
        med_ta, tot_ta, _ = timed_look(after, scenes['toward_apex'], spec, max(args.repeats, COST_REPEATS))
        ceiling = TIER_CEILING_MS[LOOK_TIER[lid]]
        res['P5'] = dict(inside_ms=tot_in, inside_parts=med_in, inside_range=spread_in, toward_apex_ms=tot_ta,
                         ceiling_ms=ceiling, margin_ms=ceiling-tot_in, enforced=full_hd)
        check(lid+'/P5_cost', (tot_in <= ceiling) if full_hd else True, inside_ms=round(tot_in, 4),
              toward_apex_ms=round(tot_ta, 4), ceiling_ms=ceiling, margin_ms=round(ceiling-tot_in, 4), enforced=full_hd)
        # P6 A/B proofs. Every candidate switch is proven here; the table must carry the switch EXACTLY when its proof
        # passes (a failed proof means the switch is OFF in the table, an enabled-but-unproven switch fails the gate).
        for cand_id, switch in AB_CANDIDATES:
            if cand_id != lid:
                continue
            kw_on = {'equi': 0, 'air': 0, 'footprint': 0}; kw_on[switch] = 1
            kw_off = {'equi': 0, 'air': 0, 'footprint': 0}
            ta = scenes['toward_apex']
            if switch == 'equi':
                ref, _, _ = render_look(after, ta, spec, steps=1024, shadows=False, half=False, **kw_off)
                mask = ref[:, :, :3].max(2) > 1e-5
                err = {}
                for on, kw in ((0, kw_off), (1, kw_on)):
                    im, _, _ = render_look(after, ta, spec, shadows=False, half=False, **kw)
                    err[on] = float(np.sqrt(np.mean((im[:, :, :3]-ref[:, :, :3])[mask]**2)))
                ratio = err[0]/err[1] if err[1] > 0 else float('inf')
                proof = dict(equi_off_rmse=err[0], equi_on_rmse=err[1], rmse_ratio=ratio, required_ratio=1.5)
                proof_ok = ratio >= 1.5
            elif switch == 'air':
                ms = {on: timed_look(after, scenes['inside'], spec, max(args.repeats, COST_REPEATS), **kw)[1]
                      for on, kw in ((0, kw_off), (1, kw_on))}
                proof = dict(air_off_ms=ms[0], air_on_ms=ms[1], cost_ratio=ms[1]/ms[0], required_max_ratio=0.8)
                proof_ok = ms[1] <= .8*ms[0]
            else:
                ref, _, _ = render_look(after, scenes['outside'], spec, steps=1024, cookie=True, half=False, **kw_off)
                hp = {}; mae = {}
                for on, kw in ((0, kw_off), (1, kw_on)):
                    im, _, _ = render_look(after, scenes['outside'], spec, cookie=True, half=False, **kw)
                    residual = im[:, :, :3]-ref[:, :, :3]
                    local = sum(np.roll(residual, (y, x), (0, 1)) for y in (-1, 0, 1) for x in (-1, 0, 1))/9
                    hp[on] = float(np.sqrt(np.mean((residual-local)**2))); mae[on] = float(np.mean(np.abs(residual)))
                proof = dict(highpass_off=hp[0], highpass_on=hp[1], highpass_ratio=hp[0]/hp[1] if hp[1] > 0 else float('inf'),
                             mae_off=mae[0], mae_on=mae[1], mae_ratio=mae[1]/mae[0] if mae[0] > 0 else 1.,
                             required_highpass_ratio=1.3, required_mae_ratio_max=1.2)
                proof_ok = proof['highpass_ratio'] >= 1.3 and proof['mae_ratio'] <= 1.2
            in_table = bool(spec['kwargs'][switch])
            res['P6'] = dict(switch=switch, proof_passed=bool(proof_ok), switch_in_table=in_table, **proof)
            check(lid+'/P6_'+switch, proof_ok == in_table, proof_passed=bool(proof_ok), switch_in_table=in_table, **proof)
        # contact-sheet tile (outside pose, production half-res composite)
        tiles.append((look['label'] + ' [%s %.2fms]' % (LOOK_TIER[lid], tot_in), render_look(after, scenes['outside'], spec)[0]))
        results[lid] = res
        print('looks: %s done in %.1fs' % (lid, time.time()-t0), flush=True)

    # ---- backlit-occluder fixture, Silhouette Rays
    sil = next(l for l in cfg['looks'] if l['id'] == 'b:silhouette')
    state = look_state(sil, defaults); spec = look_spec(state); after = rig.runners(spec['dust'])['after']
    back = backlit_fixture(args.width, args.height)
    removed = backlit_fixture(args.width, args.height, occluders=False)
    unshadowed = render_look(after, back, spec, shadows=False, rim=0.)[0]
    removed_img = render_look(after, removed, spec, rim=0.)[0]
    band = LaneBand(back, unshadowed)
    base_img = render_look(after, back, spec)[0]
    contrast0 = band.contrast(base_img)
    control = band.contrast(unshadowed)
    # The removed fixture is evaluated against the SAME shadow-free reference;
    # using a control as its own denominator would make its failure tautological.
    removed_contrast = band.contrast(removed_img)
    lanes = band.lane_count(base_img)
    check('backlit/a_lane_contrast', contrast0 >= LANE_CONTRAST_MIN and lanes >= 3,
          normalized_contrast=contrast0, required=LANE_CONTRAST_MIN, dark_lanes=lanes,
          air_band_rows=[band.j0, band.j1], radial_columns=int(band.cols.sum()))
    check('backlit/a2_occluders_removed_negative_control',
          removed_contrast < LANE_CONTRAST_MIN and control < LANE_CONTRAST_MIN and
          contrast0-removed_contrast >= LANE_CONTRAST_MIN,
          no_occluders=removed_contrast, no_shadows=control, with_occluders=contrast0,
          note='Both controls must FAIL the same normalized lane threshold')
    bframes = micro_frames(back)
    budgets = [production_steps(host, f, spec['resolution']) for f in bframes]
    imgs = []; bands = []
    for f, budget in zip(bframes, budgets):
        ref = render_look(after, f, spec, steps=budget[0], shadows=False, rim=0.)[0]
        bands.append(LaneBand(f, ref))
        imgs.append(render_look(after, f, spec, steps=budget[0], rim=0.)[0])
    hist = temporal_sequence(after, bframes, spec, budgets)
    contrasts = [b.contrast(im) for b, im in zip(bands, imgs)]
    t_contrast = [b.contrast(im) for b, im in zip(bands, hist)]
    drift = max(abs(c/contrasts[0]-1) for c in contrasts)
    t_drift = max(abs(c/t_contrast[0]-1) for c in t_contrast)
    # Measure the transported fan, not movement of its silhouette across fixed
    # screen pixels. Every radial column is normalized by its matching camera's
    # unshadowed beam. The original >5% / <=0.1% bound remains: with <=501
    # columns a single unstable lane column fails. Report raw coverage separately.
    common = np.logical_and.reduce([b.cols for b in bands])
    def fan_profiles(images):
        return [b.column_mean(b.samples(im))[common]/np.maximum(b.reference[common], 1e-9)
                for b, im in zip(bands, images)]
    def fan_motion(images):
        profiles = np.asarray(fan_profiles(images))
        return [float(np.mean(np.abs(b-a) > .05*np.maximum(a, 1e-4)))
                for a, b in zip(profiles, profiles[1:])]
    pair = fan_motion(imgs); tpair = fan_motion(hist)
    pair_fraction = max(pair); t_fraction = max(tpair)
    # A real rendered 20% brightness pop must fail this transported metric.
    pop_spec = dict(spec, uniforms=dict(spec['uniforms']))
    pop_spec['uniforms']['godray_multiplier'] *= 1.2
    popped = list(imgs); popped[1] = render_look(after, bframes[1], pop_spec, steps=budgets[1][0], rim=0.)[0]
    pop_fraction = max(fan_motion(popped))
    check('backlit/b_motion_stable', drift <= LANE_DRIFT_MAX and t_drift <= LANE_DRIFT_MAX and
          pair_fraction <= MOTION_PIXEL_BOUND and t_fraction <= MOTION_PIXEL_BOUND and
          len(pair) == len(tpair) == 8 and pop_fraction > MOTION_PIXEL_BOUND,
          contrast_drift=drift, temporal_contrast_drift=t_drift,
          max_pair_fraction=pair_fraction, temporal_pair_fraction=t_fraction, injected_pop_fraction=pop_fraction,
          raw_screen_coverage_fraction=max(motion_fractions(imgs)),
          contrasts=[round(c, 4) for c in contrasts], pairs=pair, temporal_pairs=tpair)
    clamp_fractions = {}
    def clamp_fraction(im):
        peak = im[:, :, :3].max(2); lit = peak > 1e-4
        return float(np.count_nonzero(lit & (peak >= spec['kwargs']['maximum']*.999))/max(lit.sum(), 1))
    for pose, f in dict(scenes, backlit=back).items():
        clamp_fractions[pose] = clamp_fraction(render_look(after, f, spec)[0])
    at_clamp = max(clamp_fractions.values())
    check('backlit/c_core_not_flattened', at_clamp < CLAMP_FRACTION_MAX,
          fraction_at_clamp=at_clamp, poses=clamp_fractions, limit=CLAMP_FRACTION_MAX,
          max_luminance=spec['kwargs']['maximum'])
    sweep = {}
    for multiplier in (1.3, 1.4, 2.2):
        candidate = dict(spec, uniforms=dict(spec['uniforms']))
        candidate['uniforms']['godray_multiplier'] = FIXTURE_BRIGHTNESS*multiplier
        fractions = {pose: clamp_fraction(render_look(after, f, candidate)[0])
                     for pose, f in dict(scenes, backlit=back).items()}
        sweep[str(multiplier)] = dict(max_fraction=max(fractions.values()), poses=fractions)
    soft_spec = dict(spec, kwargs=dict(spec['kwargs'], soft=True))
    soft_img = render_look(after, back, soft_spec)[0]
    full_img = render_look(after, back, spec, half=False)[0]
    c_soft, c_full = band.contrast(soft_img), band.contrast(full_img)
    check('backlit/d_documented_limits', c_soft < contrast0 and c_full >= contrast0-1e-4,
          contrast_soft_on=c_soft, contrast_soft_off=contrast0, contrast_full_res=c_full,
          note='Soft shadows ON must lower normalized contrast; full res must not lower it (1e-4 float slack)')
    air_spec = dict(spec, kwargs=dict(spec['kwargs'], air=1))
    air_img = render_look(after, back, air_spec)[0]
    air_ms = timed_look(after, back, air_spec, args.repeats)[1]; off_ms = timed_look(after, back, spec, args.repeats)[1]
    tiles.append(('Silhouette Rays: backlit fan (normalized %.2f)' % contrast0, base_img))
    grid_sheet(images/'looks_contact_sheet.png', tiles)
    sheet(images/'looks_backlit_silhouette.png', [('backlit fan', base_img), ('occluders removed', removed_img),
                                               ('soft shadows ON', soft_img), ('full-res march', full_img)])
    report['looks'] = dict(table=results, backlit=dict(
        lane_contrast=contrast0, lane_contrast_control=control, no_occluders_contrast=removed_contrast, dark_lanes=lanes,
        motion_contrasts=contrasts, contrast_drift=drift, pair_fraction=pair_fraction,
        temporal_contrast_drift=t_drift, temporal_pair_fraction=t_fraction, fraction_at_clamp=at_clamp,
        multiplier_sweep=sweep, contrast_soft_on=c_soft, contrast_full_res=c_full,
        contrast_airborne=band.contrast(air_img), airborne_ms=air_ms, off_ms=off_ms),
        thresholds=dict(motion_pixel_bound=MOTION_PIXEL_BOUND, lane_contrast_min=LANE_CONTRAST_MIN,
                        lane_drift_max=LANE_DRIFT_MAX, clamp_fraction_max=CLAMP_FRACTION_MAX, tier_ceiling_ms=TIER_CEILING_MS),
        size=[args.width, args.height], cost_enforced=full_hd)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--width',type=int,default=640); ap.add_argument('--height',type=int,default=360)
    ap.add_argument('--repeats',type=int,default=9); ap.add_argument('--out',type=Path,default=G.REPO/'.tmp/volumetric_gl')
    ap.add_argument('--glm-include',type=Path)
    ap.add_argument('--round1-only',action='store_true',help='run only the review-round regression suite')
    ap.add_argument('--round2-only',action='store_true',help='run only the second review-round regression suite')
    ap.add_argument('--looks-only',action='store_true',help='run only the Light > Projector Shafts look gate')
    ap.add_argument('--engines-only',action='store_true',help='frozen Classic/New GL identity, engine switches, presets and scenes')
    ap.add_argument('--performance-only',action='store_true',help='Full identity, continuous-budget motion and hard-beam regression gates')
    ap.add_argument('--performance-cost-only',action='store_true',help='1/3/6 overlapping cones at this resolution, Full/Auto/Fast, soft/hard beam shadows')
    ap.add_argument('--review-fix-only',action='store_true',help='FIX1 regressions, including explicit startup defines and compiled UI/reservation decisions')
    args = ap.parse_args(); args.out=args.out.resolve(); args.out.mkdir(parents=True,exist_ok=True)
    images = args.out/'images'; images.mkdir(exist_ok=True)
    gl = G.GL(); print('GL:',gl.renderer,flush=True)
    assert 'RTX 5090' in gl.renderer, 'FAIL: expected RTX 5090'
    if args.engines_only:
        from relight_volumetric_engine_check import engine_checks
        report = dict(renderer=gl.renderer,size=[args.width,args.height],classic='c4d3614d3e7',new='824f65e6df7',checks=[])
        engine_checks(sys.modules[__name__],args,gl,report)
        report['passed'] = all(c['passed'] for c in report['checks'])
        (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('PASS' if report['passed'] else 'FAIL',args.out/'report.json',flush=True)
        return 0 if report['passed'] else 1
    if args.performance_only or args.performance_cost_only or args.review_fix_only:
        from relight_volumetric_perf import performance_checks
        host = build_scissor_host(args.out,args.glm_include)
        report = dict(renderer=gl.renderer,size=[args.width,args.height],baseline='13028c968b4',
                      timing='warmed GL_TIME_ELAPSED medians; no upload/readback/CPU cost; fixture costs, not viewer FPS',
                      checks=[],measurements=[])
        try:
            if args.review_fix_only:
                from relight_volumetric_fix1_check import review_checks
                review_checks(sys.modules[__name__],args,gl,host,report)
            else:
                performance_checks(sys.modules[__name__],args,gl,host,report)
        finally:
            host.stdin.close(); assert host.wait(timeout=5)==0
        report['passed'] = all(c['passed'] for c in report['checks'])
        (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('PASS' if report['passed'] else 'FAIL',args.out/'report.json',flush=True)
        return 0 if report['passed'] else 1
    if args.looks_only:
        host=build_scissor_host(args.out,args.glm_include)
        report = dict(renderer=gl.renderer,size=[args.width,args.height],
                      timing='median of warmed GL_TIME_ELAPSED draws; no upload/readback/CPU cost',checks=[],measurements=[])
        looks_checks(args,gl,host,report)
        host.stdin.close(); assert host.wait(timeout=5)==0
        report['passed']=all(c['passed'] for c in report['checks'])
        (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('PASS' if report['passed'] else 'FAIL',args.out/'report.json',flush=True)
        return 0 if report['passed'] else 1
    runners = {name:Runner(tree,gl) for name,tree in [('before',G.SourceTree(BASE)),
               ('fix1',ControlTree(BASE)),('after',G.SourceTree('WORKTREE'))]}
    host=build_scissor_host(args.out,args.glm_include)
    runners['after'].host=host
    scenes = fixtures(args.width,args.height)
    report = dict(renderer=gl.renderer,baseline=BASE,size=[args.width,args.height],
                  timing='median of warmed GL_TIME_ELAPSED draws; no upload/readback/CPU cost',checks=[],measurements=[])
    def measure(label, runner, fixture, **kw):
        runner.run(fixture,**kw) # shader/driver warmup excluded
        times=[]
        for _ in range(args.repeats):
            im,raw,t = runner.run(fixture,**kw); times.append(t)
        med = {key:float(np.median([v.get(key,0) for v in times])) for key in set().union(*times)}
        spread = {key:[float(np.min([v.get(key,0) for v in times])),
                       float(np.max([v.get(key,0) for v in times]))] for key in med}
        report['measurements'].append(dict(name=label,**med,total_ms=sum(med.values()),range_ms=spread,repeats=args.repeats))
        print(label,med,flush=True)
        return im,raw
    if args.round1_only or args.round2_only:
        if args.round1_only:round1_checks(args,gl,host,runners,report,measure)
        if args.round2_only:round2_checks(args,gl,host,runners,report)
        host.stdin.close(); assert host.wait(timeout=5)==0
        report['passed']=all(c['passed'] for c in report['checks'])
        (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('PASS' if report['passed'] else 'FAIL',args.out/'report.json',flush=True)
        return 0 if report['passed'] else 1
    # A narrow, partially visible cone maximizes the verified missed-cone waste.
    narrow=fixtures(args.width,args.height,cone=14)['outside']
    a,ar=measure('missed_cone_14deg/before',runners['before'],narrow,steps=64)
    b,br=measure('missed_cone_14deg/fix1',runners['fix1'],narrow,steps=64)
    report['checks'].append(dict(name='missed_cone_14deg_bytes',passed=ar.tobytes()==br.tobytes() and bool(a[:,:,:3].max()>0),
        sha256_before=hashlib.sha256(ar.tobytes()).hexdigest(),sha256_after=hashlib.sha256(br.tobytes()).hexdigest()))
    sheet(images/'missed_cone.png',[('before 14 deg / 64 steps',a),('fix1 identical',b)])
    for scene,fixture in scenes.items():
        for half in (False,True):
            a,ar=measure(f'{scene}/half={half}/before',runners['before'],fixture,half=half)
            b,br=measure(f'{scene}/half={half}/fix1',runners['fix1'],fixture,half=half)
            identical=ar.tobytes()==br.tobytes()
            report['checks'].append(dict(name=f'fix1_bytes/{scene}/{half}',passed=identical,
                sha256_before=hashlib.sha256(ar.tobytes()).hexdigest(),sha256_after=hashlib.sha256(br.tobytes()).hexdigest()))
            c,cr=measure(f'{scene}/half={half}/after_edges',runners['after'],fixture,half=half)
            rect=scissor_rect(host,fixture,half)
            _,clipped=measure(f'{scene}/half={half}/pyramid_scissor',runners['after'],fixture,half=half,rect=rect)
            report['checks'].append(dict(name=f'scissor_bytes/{scene}/{half}',passed=clipped.tobytes()==cr.tobytes(),rect=rect))
            _,next_raw,_=runners['after'].run(fixture,half=half,dither=1,frame=2)
            _,prev_raw,_=runners['after'].run(fixture,half=half,dither=1,frame=1)
            report['checks'].append(dict(name=f'static_bytes/{scene}/{half}',passed=next_raw.tobytes()==prev_raw.tobytes()))
            if half:
                ref,_,_=runners['after'].run(fixture,steps=512)
                equal_steps,_,_=runners['after'].run(fixture,steps=24)
                mask=edge_mask(fixture[1]); assert mask.any()
                err0=float(np.mean(np.abs(a[:,:,:3]-ref[:,:,:3])[mask])); err1=float(np.mean(np.abs(c[:,:,:3]-ref[:,:,:3])[mask]))
                same0=float(np.mean(np.abs(a[:,:,:3]-equal_steps[:,:,:3])[mask]))
                same1=float(np.mean(np.abs(c[:,:,:3]-equal_steps[:,:,:3])[mask]))
                aligned=(args.width%2==0 and args.height%2==0)
                improved=(err1<err0 and same1<same0) if aligned else (err1<=err0+1e-6 and same1<=same0+1e-6)
                report['checks'].append(dict(name=f'halo/{scene}',passed=improved,checkerboard_active=aligned,
                    before_mae=err0,after_mae=err1,same_steps_before_mae=same0,same_steps_after_mae=same1,pixels=int(mask.sum())))
                sheet(images/f'{scene}_silhouette.png',[('before half',a),('after half',c),('full 512 steps',ref)])
            # Include the rim in the isolated item-1 proof too.
            _,ra,_=runners['before'].run(fixture,half=half,rim=1)
            _,rb,_=runners['fix1'].run(fixture,half=half,rim=1)
            report['checks'].append(dict(name=f'fix1_rim_bytes/{scene}/{half}',passed=ra.tobytes()==rb.tobytes()))
    fixture=scenes['toward_apex']
    ref,_,_=runners['after'].run(fixture,steps=1024,shadows=False)
    panels=[]
    banding=[]
    for equi in (0,1):
        im,raw=measure('banding/equi='+str(equi),runners['after'],fixture,steps=16,equi=equi,shadows=False)
        mask=ref[:,:,:3].max(2)>1e-5
        err=float(np.sqrt(np.mean((im[:,:,:3]-ref[:,:,:3])[mask]**2)))
        report['checks'].append(dict(name='banding_rmse/equi='+str(equi),passed=True,rmse=err))
        banding.append(err)
        panels.append(('equi='+str(equi),im))
    sheet(images/'banding.png',panels+[('uniform 1024 reference',ref)])
    report['checks'].append(dict(name='equiangular_banding_improves',passed=banding[1]<banding[0],ratio=banding[0]/banding[1]))
    fixture=scenes['outside']; panels=[]
    for air in (0,1):
        im,_=measure('airborne/tap='+str(air),runners['after'],fixture,half=True,steps=48,air=air)
        panels.append(('hardware tap='+str(air),im))
    sheet(images/'airborne.png',panels)
    # The half-res dither path runs at one IGN value per half texel (2x2 full
    # pixels), so execute mode 1 as well as the centered halo measurements.
    panels=[]
    for name in ('before','after'):
        im,_=measure('recording/static_dither/'+name,runners[name],fixture,half=True,dither=1)
        panels.append((name+' half/static IGN',im))
    sheet(images/'dither.png',panels)
    panels=[]
    ref,_,_=runners['after'].run(fixture,steps=1024,cookie=True)
    for fp in (0,1):
        im,_=measure('cookie/footprint='+str(fp),runners['after'],fixture,steps=16,cookie=True,footprint=fp)
        err=float(np.mean(np.abs(im[:,:,:3]-ref[:,:,:3])))
        residual=im[:,:,:3]-ref[:,:,:3]
        local=sum(np.roll(residual,(y,x),(0,1)) for y in (-1,0,1) for x in (-1,0,1))/9
        highpass=float(np.sqrt(np.mean((residual-local)**2)))
        report['checks'].append(dict(name='cookie_mae/footprint='+str(fp),passed=True,mae=err,highpass_rmse=highpass))
        panels.append(('step LOD='+str(fp),im))
    sheet(images/'cookie.png',panels+[('1024 steps',ref)])
    panels=[]
    for name in ('before','after'):
        im,_=measure('rim/'+name,runners[name],fixture,half=True,steps=24,rim=1)
        panels.append((name+' half + rim',im))
    ref,_,_=runners['after'].run(fixture,steps=512,rim=1)
    sheet(images/'rim.png',panels+[('full 512 + rim',ref)])
    mask=edge_mask(fixture[1])
    rim_err=[float(np.mean(np.abs(im[:,:,:3]-ref[:,:,:3])[mask])) for _,im in panels]
    report['checks'].append(dict(name='full_resolution_rim_halo',passed=rim_err[1]<rim_err[0],before_mae=rim_err[0],after_mae=rim_err[1]))
    # Exercise the new combinations and their additive first moment, including
    # the 0.1 m clamp case when looking almost directly down the light axis.
    for equi,air,fp in ((1,0,0),(0,1,0),(1,1,1)):
        im,raw,_=runners['after'].run(scenes['toward_apex'],half=True,equi=equi,air=air,footprint=fp,beam=1,dither=1,frame=1)
        _,other,_=runners['after'].run(scenes['toward_apex'],half=True,equi=equi,air=air,footprint=fp,beam=1,dither=1,frame=2)
        report['checks'].append(dict(name=f'combined_static/equi={equi}/air={air}/footprint={fp}',
            passed=raw.tobytes()==other.tobytes() and bool(im[:,:,:3].max()>0)))
    pipe=(G.REPO/'indra/newview/pipeline.cpp').read_text()
    body=pipe[pipe.index('void LLPipeline::renderProjectorVolumetric('):pipe.index('void LLPipeline::feedProjectorVolumetricBloom(')]
    report['checks'].append(dict(name='shared_production_wiring',passed=(
        body.count('projvolDrawFullResRim(')==1 and
        pipe.count('projvolDrawRimTexture(')==3 and body.count('projvolUploadRecordingUniforms(')==1 and
        'LLFrameTimer::getFrameCount()' not in body and
        'LLPresentationTime::currentFrame().presentation_time' in body and
        'uniform1i(LLShaderMgr::PROJVOL_DITHER' not in body and
        'projvolPyramidBounds(drawablep, mat, proj, radius,' in body)))
    # Near-plane clipping and looking away: compare the C++ helper's scissored
    # GL draws to full draws over an orbit, not just a projected-vertex assertion.
    light=fixture[-1]
    for k in range(12):
        angle=k*2*np.pi/12
        cam=S.Camera((3*np.cos(angle),1+3*np.sin(angle),1.7),(0,1,1),args.width,args.height)
        g=S.cast_primary(light['scene'],cam); _,depth,norm=S.encode_gbuffer(cam,g)
        f=(cam,g,depth,norm,light); rect=scissor_rect(host,f,True)
        _,full,_=runners['after'].run(f,half=True)
        _,clipped,_=runners['after'].run(f,half=True,rect=rect)
        report['checks'].append(dict(name=f'scissor_orbit/{k}',passed=full.tobytes()==clipped.tobytes(),rect=rect))
    for name,defines in [('conservative','#define PROJVOL_CONSERVATIVE_SHADOW 1'),
                         ('dust','#define PROJVOL_DUST_ENABLE 1'),
                         ('both','#define PROJVOL_CONSERVATIVE_SHADOW 1\n#define PROJVOL_DUST_ENABLE 1')]:
        saved=G.PREAMBLE
        try:
            G.PREAMBLE+='\n'+defines+'\n'
            r=Runner(G.SourceTree('WORKTREE'),gl)
        finally: G.PREAMBLE=saved
        im,_,_=r.run(fixture,half=True,equi=1,air=1,conservative=1,beam=1)
        report['checks'].append(dict(name='permutation/'+name,passed=bool(np.isfinite(im).all())))
    round1_checks(args,gl,host,runners,report,measure)
    round2_checks(args,gl,host,runners,report)
    looks_checks(args,gl,host,report)
    host.stdin.close(); assert host.wait(timeout=5)==0
    report['passed']=all(c['passed'] for c in report['checks'])
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS' if report['passed'] else 'FAIL',args.out/'report.json',flush=True)
    return 0 if report['passed'] else 1


if __name__=='__main__': sys.exit(main())
