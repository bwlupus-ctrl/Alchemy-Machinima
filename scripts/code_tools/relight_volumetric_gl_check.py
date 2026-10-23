"""Real WGL regression/measurement gate for production projector shafts (no viewer build).

python scripts/code_tools/relight_volumetric_gl_check.py [--width 640] [--height 360]
Baseline is pinned to c4d3614d3e7. Only the named missed-cone control changes its
source; all final variants execute WORKTREE production GLSL and deferred/shadow
modules. Analytic G-buffers AND a light-space analytic depth map describe the
same capsule, wall and floor. Timings are GL_TIME_ELAPSED draw timings (median,
warmup excluded), not viewer FPS. Missing GL or a non-5090 renderer is a failure.
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

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import relight_3d_gl as G
import relight_3d_scene as S

SHAFT = 'class3/deferred/projectorVolumetricF.glsl'
UP = 'class1/deferred/projectorVolumetricUpsampleF.glsl'
BASE = 'c4d3614d3e7'
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
        self.shaft, self.up = self.link(SHAFT, True), self.link(UP, False)
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
        gl.link(pr); ok = gl.I(); gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000); gl.plog(pr,len(log),None,log)
            raise AssertionError(file+' LINK '+log.value.decode())
        return pr

    def ui(self, pr, name, val): self.gl.ui(self.gl.loc(pr,name.encode()),int(val))
    def uf(self, pr, name, *val): self.gl.uf[len(val)](self.gl.loc(pr,name.encode()),*val)

    def target(self, name, w, h):
        tex = self._texture(name, 0, 0x881A, w,h,0x1908,0x1406,None) # RGBA16F
        gl = self.gl
        gl.bind_fbo(0x8D40,self.fbo); gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,tex,0)
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
            air=0, footprint=0, rim=0, soft=True, cookie=False, shadows=True, rect=None, beam=0, conservative=0):
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
        marched = self.target('march',tw,th)
        pr = self.shaft
        if (equi or air or footprint) and self.tree.ref=='WORKTREE':
            if self.ab is None:
                saved=G.PREAMBLE
                try:
                    G.PREAMBLE=self.preamble+'\n#define PROJVOL_INTEGRATION_AB 1\n'
                    self.ab=self.link(SHAFT,True)
                finally: G.PREAMBLE=saved
            pr=self.ab
        self._begin(pr)
        self._mat(pr,b'inv_proj',np.linalg.inv(cam.proj))
        self._mat(pr,b'projvol_inv_modelview',np.linalg.inv(cam.view))
        self._mat(pr,b'proj_mat',light['matrix']@np.linalg.inv(cam.view))
        mats = np.tile(np.eye(4),(14,1,1)); mats[4:] = light['matrix']@np.linalg.inv(cam.view)
        data = np.ascontiguousarray(mats.transpose(0,2,1),np.float32)
        gl.matrix(gl.loc(pr,b'shadow_matrix'),14,0,data.ctypes.data_as(C.POINTER(gl.F)))
        planes = light['planes']@np.linalg.inv(cam.view)
        planes = np.ascontiguousarray(planes/np.linalg.norm(planes[:,:3],axis=1)[:,None],np.float32)
        gl.u4fv(gl.loc(pr,b'projvol_frustum_planes'),6,planes.ctypes.data_as(C.POINTER(gl.F)))
        centre = (cam.view@np.array([*light['pos'],1]))[:3]
        axis = cam.view[:3,:3]@light['dir']
        for name,val in [('center',centre),('color',(1,.8,.55)),('proj_n',axis),('gobo_tint',(1,1,1))]:
            self.uf(pr,name,*val)
        for name,val in {'size':12,'falloff':.3,'godray_multiplier':6,'projvol_g':.8,
                         'projvol_max':100,'projvol_density':1,'projvol_feather':.035,
                         'proj_focus':12 if cookie else 0,'proj_lod':9,'proj_range':12,'projvol_frame':frame,
                         'projvol_rim_strength':rim if not half or self.tree.ref!= 'WORKTREE' else 0,
                         'projvol_rim_power':2,'projvol_rim_wrap':.5,'projvol_rim_threshold':0,
                         'soft_shadow_scale':4,'soft_shadow_max':8,'shadow_split_blend':.25,
                         'gobo_intensity':1}.items(): self.uf(pr,name,val)
        self.uf(pr,'projvol_half_res',*( (tw,th) if half and self.tree.ref=='WORKTREE' else (0,0)))
        self.uf(pr,'proj_shadow_res',*shadow.shape[::-1]); self.uf(pr,'shadow_clip',16,32,48,64)
        self.uf(pr,'gobo_anim_params',0,0,1,0) # production default zoom is 1, not an unset uniform's 0
        for name,val in {'depthMap':1,'normalMap':2,'projectionMap':4,'proj_shadow_idx':0,
                         'godray_res':steps,'projvol_dither':dither,'projvol_frustum_clip':1,
                         'projvol_shadow_samples':1,'projvol_equiangular':equi,
                         'projvol_temporal_beam_depth':beam,
                         'projvol_conservative_shadow':conservative,'projvol_dust_map':6,
                         'projvol_airborne_tap':air,'projvol_cookie_footprint':footprint,
                         'soft_shadow_enable':soft,'gobo_pattern':-1}.items(): self.ui(pr,name,val)
        for i in range(4,14): self.ui(pr,'shadowMap'+str(i),3)
        stats = {}
        if rect is not None:
            self.enable(0x0C11); self.scissor(*rect)
        self.draw(stats,'march_ms'); gl.disable(0x0C11)
        if half:
            output = self.target('composite',W,H)
            up = self.up; self._begin(up); self._mat(up,b'inv_proj',np.linalg.inv(cam.proj))
            self.ui(up,'depthMap',1); self.ui(up,'projectionMap',5)
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
            if rim>0 and self.tree.ref=='WORKTREE':
                self._begin(pr); self.ui(pr,'godray_res',0); self.uf(pr,'projvol_half_res',0,0)
                self.uf(pr,'projvol_rim_strength',rim)
                self.enable(0x0BE2); self.blend(1,1); self.draw(stats,'rim_ms'); gl.disable(0x0BE2)
        gl.finish()
        raw = np.empty((H,W,4),np.uint16) # retain exact RGBA16F bytes
        gl.read(0,0,W,H,0x1908,0x140B,raw.ctypes.data)
        assert self.error()==0, 'OpenGL error'
        image = raw.view(np.float16).astype(np.float32)
        assert np.isfinite(image).all(), 'NaN/Inf in production output'
        return image,raw,stats


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
    light = dict(pos=pos,dir=direction,matrix=bias@vp,planes=planes,depth=depth,scene=scene)
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


def build_scissor_host(out, glm_include):
    """Compile the actual production near-plane clipping helper, not a Python port."""
    if glm_include is None:
        cache = Path.home()/'cxs/zs_flags_cache_cube.json'
        flags = ' '.join(f for v in json.loads(cache.read_text(encoding='utf-8-sig')).values() for f in v['flags'])
        # The JSON cache has ordinary Windows paths; inspect both /I and external /I.
        candidates = re.findall(r'"([A-Z]:[^"\r\n]+)"',flags)
        glm_include = next((Path(p) for p in candidates if (Path(p)/'glm/glm.hpp').exists()),None)
    assert glm_include and (glm_include/'glm/glm.hpp').exists(), 'provide --glm-include'
    pipe = (G.REPO/'indra/newview/pipeline.cpp').read_text()
    start=pipe.index('static bool projvolClipPyramidBounds(')
    helper=pipe[start:pipe.index('\n}\n',start)+3]
    source = '#include <glm/glm.hpp>\n#include <cmath>\n#include <algorithm>\n#include <iostream>\n#include <iomanip>\nusing F32=float; using S32=int;\n'
    source += 'template<class T> T llmin(T a,T b){return std::min(a,b);} template<class T> T llmax(T a,T b){return std::max(a,b);}\n'+helper
    source += '''int main(){ glm::vec4 v[5],b; float w,h;
while(std::cin>>w>>h){ for(auto& q:v) for(int k=0;k<4;++k) std::cin>>q[k];
bool ok=projvolClipPyramidBounds(v,w,h,b);
std::cout<<ok<<" "<<std::setprecision(9)<<b.x<<" "<<b.y<<" "<<b.z<<" "<<b.w<<std::endl; }}'''
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


def scissor_rect(host,fixture,half):
    cam,_,_,_,light=fixture
    # Apex + far cap that contains the entire 12 m sphere. Light-space axes
    # come from the exact projector texture matrix used by the production draw.
    inv=np.linalg.inv(light['matrix'])
    vertices=[np.array([*light['pos'],1.])]
    for x,y in ((0,0),(1,0),(1,1),(0,1)):
        q=inv@np.array([x,y,1,1]); q=q[:3]/q[3]
        vertices.append(np.array([*q,1.]))
    clip=np.array(vertices)@(cam.proj@cam.view).T
    w,h=(cam.W//2,cam.H//2) if half else (cam.W,cam.H)
    host.stdin.write(' '.join(map(str,[w,h,*clip.ravel()]))+'\n'); host.stdin.flush()
    vals=list(map(float,host.stdout.readline().split())); assert len(vals)==5 and vals[0]==1
    x0,y0,x1,y1=vals[1:]
    x0,y0=max(0,int(np.floor(x0))-1),max(0,int(np.floor(y0))-1)
    x1,y1=min(w,int(np.ceil(x1))+1),min(h,int(np.ceil(y1))+1)
    return (x0,y0,max(0,x1-x0),max(0,y1-y0))


def sheet(path, panels):
    h,w = panels[0][1].shape[:2]
    out = Image.new('RGB',(w*len(panels),h+28),'#171923'); draw = ImageDraw.Draw(out)
    for i,(label,im) in enumerate(panels):
        rgb = np.clip(im[:,:,:3]*3,0,None); rgb = rgb/(1+rgb)
        pix = np.uint8(np.clip(rgb**(1/2.2)*255,0,255))
        out.paste(Image.fromarray(pix[::-1]),(i*w,28)); draw.text((i*w+8,8),label,fill='white')
    out.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--width',type=int,default=640); ap.add_argument('--height',type=int,default=360)
    ap.add_argument('--repeats',type=int,default=9); ap.add_argument('--out',type=Path,default=G.REPO/'.tmp/volumetric_gl')
    ap.add_argument('--glm-include',type=Path)
    args = ap.parse_args(); args.out=args.out.resolve(); args.out.mkdir(parents=True,exist_ok=True)
    images = args.out/'images'; images.mkdir(exist_ok=True)
    gl = G.GL(); print('GL:',gl.renderer,flush=True)
    assert 'RTX 5090' in gl.renderer, 'FAIL: expected RTX 5090'
    runners = {name:Runner(tree,gl) for name,tree in [('before',G.SourceTree(BASE)),
               ('fix1',ControlTree(BASE)),('after',G.SourceTree('WORKTREE'))]}
    host=build_scissor_host(args.out,args.glm_include)
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
    # Integration contracts for viewer-only state, exercised GL behavior above.
    pipe=(G.REPO/'indra/newview/pipeline.cpp').read_text()
    body=pipe[pipe.index('void LLPipeline::renderProjectorVolumetric('):pipe.index('void LLPipeline::feedProjectorVolumetricBloom(')]
    report['checks'].append(dict(name='recording_clock_wiring',passed=(
        'LLFrameTimer::getFrameCount()' not in body and 'projvol_dither = 2' not in body and
        'LLPresentationTime::currentFrame().presentation_time * 60.0' in body)))
    report['checks'].append(dict(name='full_resolution_rim_wiring',passed=(
        'sMarchHalfRes, 0.f, 0.f' in body and 'halfres ? 0.f : llmax(e_rimStrength' in body)))
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
    host.stdin.close(); assert host.wait(timeout=5)==0
    report['passed']=all(c['passed'] for c in report['checks'])
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS' if report['passed'] else 'FAIL',args.out/'report.json',flush=True)
    return 0 if report['passed'] else 1


if __name__=='__main__': sys.exit(main())
