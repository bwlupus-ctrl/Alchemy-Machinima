"""Real-GL implementation of rig_shadow_presets_check (not a CPU lighting model).

Analytic G-buffer avatar proxy, rasterized production shadowV/F depth, unchanged
spotLightF/pointLightF lighting, area gather/filter/resolve and default tonemap.
The white cookie and matte material are explicit fixtures. Native-only OFF still
uses fixture softness overrides (crisp=0, soft>0). ON uses shipped LOOK/quality defaults.
"""
import ctypes as C
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import relight_3d_gl as G
import relight_3d_scene as S
import relight_3d_plan as PL
import relight_3d_metrics as M
import relight_3d_cube_source as CS
import relight_world_gl_check as WG
from relight_world_gl_check import Raster

LUMA = np.array([.2126,.7152,.0722])
CENTRE = np.array([0.,0.,1.45])
# Fixed before tuning; native PCF is in texels, source diameter is in metres.
TARGETS = dict(crisp=dict(penumbra_max_px=5.,contrast_min=2.),
               soft=dict(penumbra_min_px=1.,penumbra_max_px=30.,contrast_min=1.5),
               silhouette=dict(edge_share_min=.4))
MOTION_P95, MOTION_POP = .06, .05


def proxy():
    brow_x = math.sqrt(.12**2-.045**2-.045**2)+.01-.025
    prims = [S.Prim('ground','obb',c=(0,0,-.08),h=(12,12,.08)),
             S.Prim('head','sphere',c=(0,0,1.67),r=.12),
             S.Prim('nose','sphere',c=(.11,0,1.66),r=.03),
             S.Prim('brow L','sphere',c=(brow_x,.045,1.715),r=.025),
             S.Prim('brow R','sphere',c=(brow_x,-.045,1.715),r=.025),
             S.Prim('neck','capsule',a=(0,0,1.43),b=(0,0,1.56),r=.055),
             S.Prim('torso','capsule',a=(0,0,.52),b=(0,0,1.08),r=.30),
             S.Prim('shoulder L','sphere',c=(0,.29,1.16),r=.18),
             S.Prim('shoulder R','sphere',c=(0,-.29,1.16),r=.18),
             S.Prim('fold L','capsule',a=(.279,.11,.65),b=(.279,.11,1.07),r=.022),
             S.Prim('fold R','capsule',a=(.279,-.10,.67),b=(.279,-.10,1.08),r=.018)]
    return S.Scene('realistic-scale avatar proxy',prims,[],[],{},np.empty((0,3)))

def srgb_linear(rgb):
    a = np.asarray(rgb)
    return np.where(a<=.04045,a/12.92,((a+.055)/1.055)**2.4)


def make_lights(frame):
    lights=[]
    for k,e in enumerate(frame):
        e['pos']=np.array(e['pos'])+CENTRE
        e['color']=srgb_linear(e['rgb'])*e['intensity']
        l=S.Light(str(k),e['pos'],e['radius'],e['range'],
                  'projector' if k<4 else 'point',S.unit(CENTRE-e['pos']),
                  math.degrees(e['fov'])/2,seed=S.fnv_seed('rig-shadow-stable-'+str(k)))
        lights.append(l)
    return lights


def projector_camera(light,fov,res,native):
    # setupSpotLight and generateSunShadow: .25 m rig fixture box, optical
    # origin behind its front plane. Native and cookie use different near/far.
    axis=S.unit(CENTRE-light.pos)
    dist=.125/math.tan(fov*.5)
    origin=light.pos+axis*(.125-dist)
    near=dist+.01 if native else dist
    far=dist+light.range if native else dist+light.range-.25
    cam=S.Camera(origin,origin+axis,res,res,math.degrees(fov),near,far)
    # applyFrame uses shortestArc(local -Z, aim), not a world-Z look-at roll.
    # The distinction matters for the native sampler's anisotropic PCF taps.
    cross=np.cross([0.,0.,-1.],axis)
    skew=np.array([[0,-cross[2],cross[1]],[cross[2],0,-cross[0]],[-cross[1],cross[0],0]])
    rotation=np.eye(3)+skew+skew@skew/(1-axis[2])
    cam.r,cam.u=rotation[:,0],rotation[:,1]
    cam.view[:3,:3]=rotation.T
    cam.view[:3,3]=-rotation.T@origin
    return cam,dist,axis


class Runner(G.AreaRunner):
    def __init__(self, shader_ref='WORKTREE'):
        # The proxy is static. Raster.atlas otherwise tessellates every prim on
        # every cube face. Cache by the complete geometry spec, so moved or
        # resized primitives still produce new triangles.
        original_mesh=WG.mesh
        mesh_cache={}
        def cached_mesh(prim):
            key=json.dumps(prim.spec())
            if key not in mesh_cache:mesh_cache[key]=original_mesh(prim)
            return mesh_cache[key]
        WG.mesh=cached_mesh
        self.gpu_atlas=None
        super().__init__(G.SourceTree(shader_ref))
        self.raster=Raster(self)
        self.native=None
        self.spot=self.light_program('spotLightF.glsl')
        self.point=self.light_program('pointLightF.glsl')
        self.visibility=self.link_text('''
out vec4 frag_color;
uniform int probe_native;
uniform vec2 screen_res;
vec4 getPosition(vec2 p); vec4 getNorm(vec2 p);
float sampleSpotShadow(vec3 p,vec3 n,int i,vec2 uv);
void main(){vec2 tc=gl_FragCoord.xy/screen_res;vec3 p=getPosition(tc).xyz;
float v=probe_native!=0?sampleSpotShadow(p,getNorm(tc).xyz,0,tc):1.;frag_color=vec4(v);}
''', 'native visibility',extra=self.FRAGMENT_COMMON+['deferred/shadowUtil.glsl'],defines='#define SPOT_SHADOW 1\n')
        # Compile the unchanged production operator, not a Python tone curve.
        tone=self.tree.read(G.SHADERS+'/class1/deferred/tonemapUtilF.glsl')
        # LPM is a separately linked production module, including unused modes.
        self.tone=self.link_text(tone+'''
uniform sampler2D fixture_hdr; out vec4 frag_color;
vec3 linear_to_srgb(vec3 x);
void main(){vec3 c=texture(fixture_hdr,gl_FragCoord.xy/screen_res).rgb;
frag_color=vec4(linear_to_srgb(applyToneMap(c)),1);}
''', 'tonemap', extra=['environment/srgbF.glsl','deferred/LPMUtil.glsl'])

    def _texture(self,key,unit,internal,w,h,fmt,typ,data,nearest=True):
        if key=='atlas' and self.gpu_atlas is not None:
            self.gl.active(0x84C0+unit);self.gl.bind_tex(0x0DE1,self.gpu_atlas)
            return self.gpu_atlas
        return super()._texture(key,unit,internal,w,h,fmt,typ,data,nearest)

    def link_text(self,text,label,extra=(),vertex=None,defines=''):
        gl=self.gl;pr=gl.program()
        gl.attach(pr,self._compile(defines+text,label))
        for module in extra:
            gl.attach(pr,self._compile(defines+self.tree.read(self._production_file(module,1 if module in self.LEVEL1 else 3)),module))
        vertex=vertex or 'in vec3 position;void main(){gl_Position=vec4(position,1);}'
        gl.attach(pr,self._compile(vertex,label,0x8B31))
        gl.link(pr);ok=gl.I();gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(64000);gl.plog(pr,len(log),None,log)
            raise AssertionError(label+' '+log.value.decode())
        return pr

    def light_program(self,name):
        modules=list(self.FRAGMENT_COMMON)
        if name.startswith('spot'): modules+=['deferred/shadowUtil.glsl']
        source=self.tree.read(G.SHADERS+'/class3/deferred/'+name)
        vertex='''in vec3 position;uniform vec3 fixture_center;
out vec4 vary_fragcoord;out vec3 trans_center;
void main(){gl_Position=vec4(position,1);vary_fragcoord=gl_Position;trans_center=fixture_center;}
'''
        return self.link_text(source,name,extra=modules,vertex=vertex,defines='#define SPOT_SHADOW 1\n')

    def native_map(self,scene,light,fov,res):
        """Same shadowV/F rasterizer as cube host, with projector perspective."""
        gl=self.gl;r=self.raster
        cam,_,_=projector_camera(light,fov,res,True)
        tex=self._texture('native_depth',12,0x8CAC,res,res,0x1902,0x1406,None)
        gl.bind_fbo(0x8D40,r.fbo);gl.fbo_tex(0x8D40,0x8D00,0x0DE1,tex,0)
        r.draw_buffer(0);r.read_buffer(0)
        assert gl.fbo_status(0x8D40)==0x8CD5
        r.enable(0x0B71);r.depth_mask(1);r.depth_func(0x0201)
        gl.disable(0x0B44);gl.disable(0x0BE2)
        gl.viewport(0,0,res,res);gl.use(r.program)
        self._mat(r.program,b'modelview_projection_matrix',cam.proj@cam.view)
        gl.ui(gl.loc(r.program,b'actorFxEnabled'),0)
        r.clear_depth(1);r.clear(0x100)
        from relight_world_gl_check import mesh
        for p in scene.prims:
            data=np.ascontiguousarray(mesh(p),np.float32)
            gl.bind_buf(0x8892,r.vbo);gl.buffer_data(0x8892,data.nbytes,data.ctypes.data,0x88E4)
            idx=gl.attr_loc(r.program,b'position');gl.enable_attr(idx);gl.attr_ptr(idx,3,0x1406,0,0,None)
            gl.draw(4,0,len(data))
        gl.disable(0x0B71);gl.bind_fbo(0x8D40,self.fbo)
        self.native=(tex,cam,res)

    def target_float(self,cam):
        gl=self.gl
        tex=self._texture('float_out',0,0x8814,cam.W,cam.H,0x1908,0x1406,None)
        gl.bind_fbo(0x8D40,self.fbo);gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,tex,0)
        assert gl.fbo_status(0x8D40)==0x8CD5
        self.raster.draw_buffer(0x8CE0);self.raster.read_buffer(0x8CE0)
        gl.viewport(0,0,cam.W,cam.H)
        self.raster.clear(0x4000)

    def uniform(self,pr,name,*values):
        self.gl.uf[len(values)](self.gl.loc(pr,name.encode()),*values)

    def native_uniforms(self,pr,cam,e,defaults):
        gl=self.gl;tex,ncam,res=self.native
        gl.active(0x84CC);gl.bind_tex(0x0DE1,tex)
        gl.param(0x0DE1,0x884C,0x884E);gl.param(0x0DE1,0x884D,0x0203)
        gl.param(0x0DE1,0x2801,0x2601);gl.param(0x0DE1,0x2800,0x2601)
        for i in range(4,14):gl.ui(gl.loc(pr,('shadowMap'+str(i)).encode()),12)
        bias=np.eye(4);bias[:3,:3]*=.5;bias[:3,3]=.5
        self._mat(pr,b'shadow_matrix[4]',bias@ncam.proj@ncam.view@np.linalg.inv(cam.view))
        self.uniform(pr,'proj_shadow_res',res,res)
        self.uniform(pr,'shadow_clip',8,16,32,64)
        self.uniform(pr,'shadow_split_blend',defaults['RenderShadowSplitBlend'])
        self.uniform(pr,'spot_shadow_bias',defaults['RenderSpotShadowBias'])
        self.uniform(pr,'spot_shadow_offset',defaults['RenderSpotShadowOffset'])
        self.uniform(pr,'spot_shadow_softness[0]',e['softness'])
        self.uniform(pr,'spot_shadow_detail[0]',float(e.get('detail_shadows',False) and defaults['RenderSpotShadowDetailBias']))
        for name,key in [('soft_shadow_scale','BDMergeSoftShadowSoftness'),('soft_shadow_max','BDMergeSoftShadowMaxPenumbra'),('soft_shadow_fill','BDMergeSoftShadowFill')]:
            self.uniform(pr,name,defaults[key])
        gl.ui(gl.loc(pr,b'soft_shadow_enable'),int(defaults['BDMergeSoftProjectorShadows']))
        gl.ui(gl.loc(pr,b'soft_shadow_vogel'),0)

    def inputs(self,pr,cam,depth,norm):
        gl=self.gl
        self._texture('depth',1,0x81A6,cam.W,cam.H,0x1902,0x1405,depth)
        self._texture('norm',2,0x805B,cam.W,cam.H,0x1908,0x1403,norm)
        gl.ui(gl.loc(pr,b'depthMap'),1);gl.ui(gl.loc(pr,b'normalMap'),2)
        self._mat(pr,b'inv_proj',np.linalg.inv(cam.proj))
        self.uniform(pr,'screen_res',cam.W,cam.H)

    def read_float(self,cam):
        gl=self.gl;gl.draw(4,0,3)
        data=np.empty((cam.H,cam.W,4),np.float32)
        gl.read(0,0,cam.W,cam.H,0x1908,0x1406,data.ctypes.data)
        assert np.all(np.isfinite(data)), 'nonfinite production output'
        return data

    def native_visibility(self,cam,depth,norm,e,defaults):
        pr=self.visibility;self.target_float(cam);self._begin(pr)
        self.inputs(pr,cam,depth,norm);self.native_uniforms(pr,cam,e,defaults)
        self.gl.ui(self.gl.loc(pr,b'probe_native'),1)
        return self.read_float(cam)[...,0]

    def lighting(self,cam,depth,norm,e,k,visibility,native,defaults):
        gl=self.gl;pr=self.spot if k<4 else self.point
        self.target_float(cam);self._begin(pr);self.inputs(pr,cam,depth,norm)
        # Matte neutral skin/fabric fixture. No artificial ambient or exposure.
        self._texture('albedo',3,0x8814,1,1,0x1908,0x1406,np.array([.58,.48,.40,1],np.float32))
        self._texture('specular',4,0x8814,1,1,0x1908,0x1406,np.array([0,0,0,0],np.float32))
        self._texture('gloss',5,0x822E,1,1,0x1903,0x1406,np.ones(1,np.float32))
        mask=np.repeat(visibility[...,None],4,axis=2).astype(np.float32)
        self._texture('visibility',6,0x8814,cam.W,cam.H,0x1908,0x1406,mask)
        for name,unit in [('diffuseRect',3),('specularRect',4),('lightFunc',5),('area_shadow_map0',6),('area_shadow_map1',6)]:gl.ui(gl.loc(pr,name.encode()),unit)
        center=(cam.view@np.r_[e['pos'],1])[:3]
        self.uniform(pr,'fixture_center',*center)
        self.uniform(pr,'color',*e['color'])
        self.uniform(pr,'size',e['range']);self.uniform(pr,'falloff',e['falloff'])
        self.uniform(pr,'light_shape',0,0,1,0)
        gl.ui(gl.loc(pr,b'relight_enabled'),2)
        gl.ui(gl.loc(pr,b'rig_rim_mode'),0)
        if k<4:
            light=S.Light(str(k),e['pos'],e['radius'],e['range'])
            pc,dist,world_axis=projector_camera(light,e['fov'],1,False)
            bias=np.eye(4);bias[:3,:3]*=.5;bias[:3,3]=.5
            self._mat(pr,b'proj_mat',bias@pc.proj@pc.view@np.linalg.inv(cam.view))
            axis=cam.view[:3,:3]@world_axis
            origin=(cam.view@np.r_[pc.eye,1])[:3]
            plane=origin+axis*(dist+.01)
            self.uniform(pr,'proj_n',*axis);self.uniform(pr,'proj_p',*plane)
            self.uniform(pr,'proj_origin',*origin)
            self.uniform(pr,'proj_range',pc.far-pc.near);self.uniform(pr,'proj_lod',1)
            self.uniform(pr,'proj_focus',0);self.uniform(pr,'proj_ambiance',0)
            self._texture('cookie',7,0x8814,1,1,0x1908,0x1406,np.ones(4,np.float32))
            gl.ui(gl.loc(pr,b'projectionMap'),7);gl.ui(gl.loc(pr,b'gobo_pattern'),-1)
            self.uniform(pr,'gobo_tint',1,1,1);self.uniform(pr,'gobo_anim_params',0,0,1,0)
            gl.ui(gl.loc(pr,b'proj_shadow_idx'),0 if native else -1)
            self.uniform(pr,'shadow_fade',0)
            if native:self.native_uniforms(pr,cam,e,defaults)
        return self.read_float(cam)[...,:3]

    def tonemap(self,cam,hdr,defaults):
        gl=self.gl;pr=self.tone;self.target_float(cam);self._begin(pr)
        self._texture('hdr',3,0x8814,cam.W,cam.H,0x1908,0x1406,np.ascontiguousarray(np.dstack([hdr,np.ones(hdr.shape[:2])]),np.float32))
        gl.ui(gl.loc(pr,b'fixture_hdr'),3)
        self.uniform(pr,'screen_res',cam.W,cam.H)
        self.uniform(pr,'tonemap_khronos_params',.15,.76)
        self.uniform(pr,'tonemap_grade',1,1)
        self.uniform(pr,'tonemap_mix',defaults['RenderTonemapMix'])
        gl.ui(gl.loc(pr,b'tonemap_type'),int(defaults['RenderTonemapType']))
        return self.read_float(cam)[...,:3]


def probes(cam,g,points):
    p=np.column_stack([points,np.ones(len(points))])@cam.view.T@cam.proj.T
    xy=(p[:,:2]/p[:,3,None]*.5+.5)*[cam.W,cam.H]-.5
    ix=np.rint(xy[:,0]).astype(int);iy=np.rint(xy[:,1]).astype(int)
    valid=(ix>=1)&(iy>=1)&(ix<cam.W-1)&(iy<cam.H-1)&(p[:,3]>0)
    ix=ix.clip(0,cam.W-1);iy=iy.clip(0,cam.H-1)
    valid &= np.linalg.norm(g['pos'][iy,ix]-points,axis=1)<.015
    return xy,valid


def sample(v,xy,valid):
    x,y=xy.T;i=np.floor(x).astype(int).clip(0,v.shape[1]-2);j=np.floor(y).astype(int).clip(0,v.shape[0]-2)
    a=(x-i).clip(0,1);b=(y-j).clip(0,1)
    z=v[j,i]*(1-a)*(1-b)+v[j,i+1]*a*(1-b)+v[j+1,i]*(1-a)*b+v[j+1,i+1]*a*b
    return np.where(valid,z,np.nan)


def penumbra(v,g,region=None):
    """10-90% subpixel crossings on nose/brow -> head and jaw -> neck."""
    mask=np.isin(g['surf'],[1,5])
    if region is not None:mask &= region
    widths=[]
    for axis in (0,1):
        vv=v if axis==0 else v.T;mm=mask if axis==0 else mask.T
        for row,m in zip(vv,mm):
            for i in np.flatnonzero(m[:-1]&m[1:]&((row[:-1]>=.5)!=(row[1:]>=.5))):
                lo=max(0,i-35);hi=min(len(row),i+36)
                x=np.arange(lo,hi);z=row[lo:hi];good=m[lo:hi]
                sign=1 if row[i+1]>row[i] else -1
                zz=z if sign>0 else 1-z
                left=np.flatnonzero(good&(x<=i)&(zz<=.1))
                right=np.flatnonzero(good&(x>i)&(zz>=.9))
                if not len(left) or not len(right):continue
                a,b=left[-1],right[0]
                if b-a>30 or not good[a:b+1].all():continue
                # Reject multiple edges; those are tested independently below.
                if np.any(np.diff(zz[a:b+1])<-.12):continue
                xl=x[a]+(.1-zz[a])/max(zz[a+1]-zz[a],1e-8)
                xr=x[b-1]+(.9-zz[b-1])/max(zz[b]-zz[b-1],1e-8)
                widths.append(float(xr-xl))
    return dict(width_px=float(np.median(widths)) if widths else None,samples=len(widths))


def witnesses(scene,g,light,illuminated=None):
    """Independent point-source blocker witnesses, never inferred from output."""
    shape=g['surf'].shape
    result={}
    for name,receivers,blockers in [('nose_brow',[1],[2,3,4]),('jaw',[5],[1]),('ground',[0],list(range(1,len(scene.prims))))]:
        mask=np.isin(g['surf'],receivers)
        if illuminated is not None:mask &= illuminated
        ids=np.flatnonzero(mask)
        p=g['pos'].reshape(-1,3)[ids];n=g['nrm'].reshape(-1,3)[ids]
        delta=np.asarray(light.pos)-p;distance=np.linalg.norm(delta,axis=1)
        direction=delta/np.maximum(distance[:,None],1e-9)
        blocked=np.zeros(len(p),bool)
        for b in blockers:
            blocked |= S.isect(scene.prims[b],p+n*.0005,direction)<distance
        blocked &= (n*direction).sum(1)>.1
        image=np.zeros(shape,bool);image.ravel()[ids]=blocked
        result[name]=image
    detail=result['nose_brow']|result['jaw']
    # Named shadow and its immediate receiver neighbourhood, to include both
    # endpoints of 10-90% edge profiles without accepting arbitrary terminators.
    region=detail.copy()
    for axis in (0,1):
        for shift in range(-24,25):region |= np.roll(detail,shift,axis=axis)
    result['region']=region
    return result


def spatial_grain(v,g,receiver_interior=None):
    # Local quadratic detrending retains noise in penumbrae, where a plain
    # variance statistic would confuse the intended smooth gradient with grain.
    coords=np.array([(x,y) for y in range(-2,3) for x in range(-2,3)])
    x,y=coords.T
    design=np.column_stack([np.ones(25),x,y,x*x,x*y,y*y])
    weights=np.linalg.pinv(design)[0]
    interior=np.ones(v.shape,bool)
    smooth=np.zeros_like(v)
    for (x,y),weight in zip(coords,weights):
        z=np.roll(np.roll(v,y,0),x,1)
        interior &= np.roll(np.roll(g['surf'],y,0),x,1)==g['surf']
        smooth+=z*weight
    interior &= g['surf']!=255
    if receiver_interior is not None: interior &= receiver_interior
    interior[:2]=False;interior[-2:]=False;interior[:,:2]=False;interior[:,-2:]=False
    residual=np.abs(v-smooth)
    values=[]
    for mask in (interior&((v<.05)|(v>.95)),interior&(v>=.05)&(v<=.95)):
        if mask.any():values.append(float(np.percentile(residual[mask],95)))
    return max(values) if values else None


def receiver_interiors(scene,g,light,pixels=4):
    """Independent receiver interiors; never threshold the rendering.

    Justification: raw quadratic residual counts a true step as grain (the
    diagnosis measured 0.298 on the legacy native kernel). Preserve that raw
    statistic and its original gate. The interior statistic is additional
    evidence; acne uses independently clear, light-facing pixels from this mask.
    """
    receiver=(g['surf']>0)&(g['surf']!=255);p=g['pos'][receiver];normal=g['nrm'][receiver]
    delta=light.pos-p;dist=np.linalg.norm(delta,axis=1);ray=delta/dist[:,None]
    clear=np.ones(len(p),bool)
    for prim in scene.prims: clear &= S.isect(prim,p+normal*.0005,ray)>=dist
    facing=(normal*ray).sum(1)>.05
    truth=np.zeros(receiver.shape,np.uint8);truth[receiver]=np.where(facing,np.where(clear,1,2),3)
    interior=truth!=0
    for dy in range(-pixels,pixels+1):
        for dx in range(-pixels,pixels+1):
            interior &= np.roll(np.roll(truth,dy,0),dx,1)==truth
            interior &= np.roll(np.roll(g['surf'],dy,0),dx,1)==g['surf']
    return dict(lit=interior&(truth==1),dark=interior&(truth==2),interior=interior)


def overlap_edges(off,on,g,region):
    """Count new secondary slopes near each named native 50% crossing.

    A native edge plus a displaced contact edge can remain monotone while
    creating two slope lobes. Do not discard such profiles as penumbra() does.
    """
    tested=bad=0
    for axis in (0,1):
        aa,bb=(off,on) if axis==0 else (off.T,on.T)
        mm=region if axis==0 else region.T
        ss=g['surf'] if axis==0 else g['surf'].T
        for a,b,m,s in zip(aa,bb,mm,ss):
            for i in np.flatnonzero(m[:-1]&m[1:]&((a[:-1]>=.5)!=(a[1:]>=.5))):
                lo,hi=i-12,i+14
                if lo<0 or hi>len(a) or not np.all(s[lo:hi]==s[i]):continue
                if s[i] not in (1,5):continue
                sign=1 if a[i+1]>a[i] else -1
                def lobes(z):
                    z=np.convolve(z,np.array([1,2,1])/4,mode='valid')
                    d=np.diff(z)*sign
                    mask=d>max(.035,float(d.max())*.3)
                    # Close a single-texel hole in a slope lobe.
                    mask[1:-1] |= mask[:-2]&mask[2:]
                    return int(np.count_nonzero(mask&~np.r_[False,mask[:-1]]))
                before,after=lobes(a[lo:hi]),lobes(b[lo:hi])
                if before==1:
                    tested+=1;bad+=after>1
    return dict(profiles=tested,secondary=bad,fraction=bad/max(tested,1))


def instrument_checks():
    x=np.linspace(0,1,96,dtype=np.float32)
    v=np.tile(x,(64,1));g=dict(surf=np.ones(v.shape,np.uint8))
    rng=np.random.default_rng(918)
    noisy=np.clip(v+rng.normal(0,.10,v.shape),0,1).astype(np.float32)
    assert spatial_grain(v,g)<.001 and spatial_grain(noisy,g)>.04,'grain instrument insensitive'
    a=np.tile(np.clip((np.arange(96)-32)/2,0,1),(64,1)).astype(np.float32)
    b=np.minimum(a,.55+.45*np.clip((np.arange(96)-40)/2,0,1))
    region=np.ones(a.shape,bool)
    assert overlap_edges(a,a,g,region)['fraction']==0
    assert overlap_edges(a,b,g,region)['fraction']>.9,'double-edge instrument insensitive'
    return dict(grain_negative_control=True,double_edge_negative_control=True)


def evaluate(presets,frames,defaults,out):
    from rig_shadow_presets_check import FAMILIES,PREFIX
    instruments=instrument_checks()
    r=Runner();sc=proxy();W,H=1920,1080
    print('[shadow-presets] GPU',r.gl.renderer,flush=True)
    plan=PL.require_compiled(PL.compute_plans_status(r.tree,out,width=W,height=H))
    cams=[];gb=[]
    for angle in (-.1,0,.1):
        a=math.radians(angle)
        cam=S.Camera((3.5*math.cos(a),3.5*math.sin(a),1.65),(0,0,1.2),W,H,36)
        g=S.cast_primary(sc,cam);_,d,n=S.encode_gbuffer(cam,g)
        cams.append(cam);gb.append((g,d,n))
    points=gb[1][0]['pos'][::3,::3].reshape(-1,3)
    valid=gb[1][0]['surf'][::3,::3].ravel()!=255
    points=points[valid]
    rows=[];failures=[];panels=[]
    for preset,frame in zip(presets,frames):
        family=FAMILIES[preset['name'].removeprefix(PREFIX)]
        lights=make_lights(frame)
        # Rig ShadowMode KEY: only projector 0 + its bounce omni qualify for area.
        active=[k for k,e in enumerate(frame) if e['on']]
        custom=int(defaults['BDMergeProjectorShadowResolution'])
        native_res=min(max(custom,256),8192) if custom>=256 else int(W*defaults['RenderShadowResolutionScale'])
        r.native_map(sc,lights[0],frame[0]['fov'],native_res)
        modes={}
        key_rank=[(frame[k]['score'],k) for k in (0,4) if frame[k]['on']]
        key_rank.sort(reverse=True)
        reference=r.lighting(cams[1],gb[1][1],gb[1][2],frame[0],0,np.ones((H,W),np.float32),False,defaults)
        named=witnesses(sc,gb[1][0],lights[0],reference@LUMA>.001)
        interiors={k:receiver_interiors(sc,gb[1][0],lights[k]) for k in (0,4)}
        centre_cache={}
        # Default world eligibility: native key excluded, key bounce eligible.
        for mode in ('off','on','picked'):
            views=[];sequences={key:[] for key in ('key','bounce','beauty')};repeat_equal=True
            owned_ids=([0,4] if mode=='picked' or (mode=='on' and frame[0]['world_shadow_pick']) else [4]) if mode!='off' else []
            owned_ids=[k for k in owned_ids if frame[k]['on']]
            for view,(cam,(g,d,n)) in enumerate(zip(cams,gb)):
                masks={};key_vis=None;native_centre=None
                for k in (0,4):
                    if not frame[k]['on']:continue
                    native=(k==0 and not (mode=='picked' or (mode=='on' and frame[0]['world_shadow_pick'])))
                    if mode=='off':
                        mask=np.ones((H,W),np.float32)
                    else:
                        owned=k in owned_ids
                        near=CS.emitter_near(sc,lights[k]) if owned else .1
                        if owned:r.gpu_atlas=r.raster.atlas(sc,lights[k],near)
                        ch=dict(row=0,near=near,far=lights[k].range,r_map=lights[k].radius,
                                contact=CS.contact_distance(lights[k].range) if owned else defaults['RenderAreaShadowMappedContact'],
                                seed=lights[k].seed,owned=owned,contact_fade=1. if native else 0.,detail_shadows=frame[k]['detail_shadows'] and defaults['RenderSpotShadowDetailBias'])
                        world=dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),channels=[ch])
                        kwargs=dict(world=world,look=dict(strength=defaults['RenderAreaShadowStrength'],contrast=defaults['RenderAreaShadowContrast'],world_amount=defaults['RenderAreaShadowWorldAmount'],reach=defaults['RenderAreaShadowWorldScreenReach'],screen_amount=defaults['RenderAreaShadowScreenAmount']),light_bias=defaults['RenderAreaShadowLightBias'],max_dist=defaults['RenderAreaShadowMaxDist'],thickness=defaults['RenderAreaShadowThickness'])
                        mask=r.run(cam,d,n,[lights[k]],plan.plans['Default'][2],**kwargs)[0]
                        repeat=r.run(cam,d,n,[lights[k]],plan.plans['Default'][2],**kwargs)[0]
                        repeat_equal &= np.array_equal(mask,repeat)
                    masks[k]=mask
                    if k==0:
                        nv=r.native_visibility(cam,d,n,frame[0],defaults) if native else np.ones_like(mask)
                        key_vis=np.minimum(nv,mask)
                        if view==1:
                            native_centre=nv.copy()
                contributions=[]
                for k in active:
                    mask=masks.get(k,np.ones((H,W),np.float32))
                    contributions.append((k,r.lighting(cam,d,n,frame[k],k,mask,k==0 and not (mode=='picked' or (mode=='on' and frame[0]['world_shadow_pick'])),defaults)))
                hdr=sum(c for _,c in contributions)
                if view==1:
                    display=r.tonemap(cam,hdr,defaults)
                    face=np.isin(g['surf'],[1,2,3,4])
                    key=sum(c for k,c in contributions if k in (0,4))@LUMA
                    fill=sum((c for k,c in contributions if k not in (0,4)),np.zeros_like(hdr))@LUMA
                    ratio=math.log2(max(float(key[face].mean()),1e-10)/max(float(fill[face].mean()),1e-10))
                    lit=face&(key_vis>.9)&(key>.001)
                    detail=named['nose_brow']|named['jaw']
                    shadow_receiver=named['ground'] if family=='silhouette' else detail
                    dark=shadow_receiver&(key_vis<.5)
                    lum=hdr@LUMA
                    contrast=float(np.median(lum[lit])/max(np.median(lum[dark]),1e-9)) if lit.any() and dark.any() else None
                    nv=(g['nrm']*(cam.eye-g['pos'])).sum(2)/np.maximum(np.linalg.norm(cam.eye-g['pos'],axis=2),1e-8)
                    subject=(g['surf']>0)&(g['surf']!=255)
                    edge=subject&(nv<.5)
                    share=float(lum[edge].sum()/max(lum[subject].sum(),1e-9))
                    pen=penumbra(key_vis,g,named['region'])
                    metrics=dict(face_ratio_stops=ratio,contrast=contrast,penumbra=pen,
                                 clipping_percent=float((display[subject]>1.00001).any(1).mean()*100),
                                 saturated_percent=float((display[subject]>.999).any(1).mean()*100),
                                 edge_share=share,key_dark_pixels=int(dark.sum()),key_lit_pixels=int(lit.sum()),
                                 fabric_lit_pixels=int((np.isin(g['surf'],[9,10])&(lum>.001)).sum()),
                                 # A picked key's sole shadow is the cube mask. This
                                 # catches retaining the native edge in the host.
                                 native_sampled=mode=='off' or (mode=='on' and not frame[0]['world_shadow_pick']),world_key=mode=='picked' or (mode=='on' and frame[0]['world_shadow_pick']))
                    metrics['named_shadow_pixels']={name:int((named[name]&(key_vis<.5)).sum()) for name in ('nose_brow','jaw','ground')}
                    metrics['named_shadow_expected']={name:int(named[name].sum()) for name in ('nose_brow','jaw','ground')}
                    metrics['raw_grain_p95']=max(spatial_grain(mask,g) for mask in masks.values())
                    metrics['interior_grain_p95']=max(spatial_grain(mask,g,interiors[k]['interior']) for k,mask in masks.items())
                    # Preserve the original grain gate and its 0.04 target.
                    # The independent interior statistic is additional evidence,
                    # not a waiver for a failing original measurement.
                    metrics['grain_p95']=metrics['raw_grain_p95']
                    # Geometric centre-ray clearance alone is insufficient for
                    # a projector: outside its cookie/cone the receiver is not
                    # independently illuminated. Reference has all shadows off.
                    acne_lit=interiors[0]['lit']&(reference@LUMA>.001)
                    metrics['acne_false_dark_percent']=float((key_vis[acne_lit]<.5).mean()*100) if acne_lit.any() else None
                    metrics['acne_clear_pixels']=int(acne_lit.sum())
                    views.append((display,key_vis.copy(),metrics))
                    centre_cache[mode]=dict(mask=masks[0].copy(),key_vis=key_vis.copy(),key_lighting=contributions[0][1].copy())
                xy,ok=probes(cam,g,points)
                signals=dict(key=key_vis,bounce=masks[4],beauty=np.minimum((hdr@LUMA)/.25,1.))
                for signal,v in signals.items():
                    pv=sample(v,xy,ok)[None,:]
                    sequences[signal].append(dict(probe=pv,cls=np.zeros_like(pv,dtype=np.uint8),cfg=0))
            motion=[]
            for signal,sequence in sequences.items():
                # Static world illumination: the ground truth change is zero.
                # Any finite constant truth gives zero world-lighting change;
                # ones let temporal() choose the production-shadowed probes.
                # A sampled first-pose truth can contain NaNs for points that
                # become visible later, corrupting the second pair's result.
                gt=np.ones_like(sequence[1]['probe'])
                pairs=M.temporal(sequence,[True]*3,{0:gt})
                # Native-only bounce has no shadow signal, hence no active
                # points. All actual shadow and beauty signals require samples.
                if signal=='bounce' and mode=='off':continue
                assert len(pairs)==2 and all('p95' in p for p in pairs),'insufficient '+signal+' motion probes'
                assert all(np.isfinite(p['p95']) and np.isfinite(p['pop']) for p in pairs),'nonfinite motion measurement'
                motion += [dict(p,signal=signal) for p in pairs]
            metrics=views[0][2]
            metrics['motion_p95']=max(m['p95'] for m in motion)
            metrics['motion_pop']=max(m['pop'] for m in motion)
            metrics['static_repeat_equal']=repeat_equal
            metrics['motion_pairs']=motion
            if family=='silhouette':
                # Back-key floor shadows often fall behind the beauty camera.
                # Test retention in a wide, elevated diagnostic view instead
                # of counting the deliberately black front face as a failure.
                # Look from the key's side so neck/head receiving surfaces are
                # available when a low cone has no ground intersection.
                horizontal=S.unit(np.array([lights[0].pos[0],lights[0].pos[1],0.]))
                side=np.array([-horizontal[1],horizontal[0],0.])
                dc=S.Camera(horizontal*7+side*2+np.array([0,0,4.]),CENTRE,W,H,55)
                dg=S.cast_primary(sc,dc);_,dd,dn=S.encode_gbuffer(dc,dg)
                ref=r.lighting(dc,dd,dn,frame[0],0,np.ones((H,W),np.float32),False,defaults)
                diagnostic=witnesses(sc,dg,lights[0],ref@LUMA>.001)
                nv=r.native_visibility(dc,dd,dn,frame[0],defaults) if mode=='off' or (mode=='on' and not frame[0]['world_shadow_pick']) else np.ones((H,W),np.float32)
                av=np.ones_like(nv)
                if mode!='off':
                    owned=mode=='picked' or frame[0]['world_shadow_pick'];near=CS.emitter_near(sc,lights[0])
                    if owned:r.gpu_atlas=r.raster.atlas(sc,lights[0],near)
                    ch=dict(row=0,near=near,far=lights[0].range,r_map=lights[0].radius,contact=CS.contact_distance(lights[0].range) if owned else defaults['RenderAreaShadowMappedContact'],seed=lights[0].seed,owned=owned,contact_fade=0. if owned else 1.,detail_shadows=frame[0]['detail_shadows'] and defaults['RenderSpotShadowDetailBias'])
                    av=r.run(dc,dd,dn,[lights[0]],plan.plans['Default'][2],world=dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),channels=[ch]),look=dict(strength=1.,contrast=1.,world_amount=1.,reach=0.,screen_amount=1.))[0]
                vv=np.minimum(nv,av)
                metrics['diagnostic_shadow']={name:dict(expected=int(diagnostic[name].sum()),retained=int((diagnostic[name]&(vv<.5)).sum())) for name in ('nose_brow','jaw','ground')}
                metrics['diagnostic_shadow']['status']='applicable' if any(m['expected']>=30 for m in metrics['diagnostic_shadow'].values()) else 'no_in_cone_shadow_witness'
            modes[mode]=metrics
            file=out/(preset['name'].removeprefix(PREFIX).replace(' ','_')+'_'+mode+'.png')
            Image.fromarray(np.uint8(np.clip(views[0][0][::-1],0,1)*255)).save(file)
            # Avatar crop, identical exposure/framing for the complete sheet.
            crop=Image.open(file).crop((590,40,1330,1060))
            crop.save(file.with_name(file.stem+'_crop.png'))
            crop=crop.resize((222,306))
            if mode=='off': pair=[crop]
            elif mode=='on':pair.append(crop)
        # Test suppression with a poisoned native depth map, including a
        # positive control that would turn the key black if it were sampled.
        cam=cams[1];g,d,n=gb[1];e=frame[0]
        r._texture('native_depth',12,0x8CAC,native_res,native_res,0x1902,0x1406,np.zeros((native_res,native_res),np.float32))
        mask=centre_cache['picked']['mask']
        poisoned=r.lighting(cam,d,n,e,0,mask,False,defaults)
        wrong=r.lighting(cam,d,n,e,0,mask,True,defaults)
        correct=centre_cache['picked']['key_lighting']
        no_double=np.array_equal(poisoned,correct)
        positive=float(np.abs(wrong-correct).max())>.001
        if not no_double or not positive:failures.append(preset['name']+' native/world double-edge isolation')
        # Fixed-position, identical optics small-source control: only source
        # radius/derived native softness changes, never framing or exposure.
        r.native_map(sc,lights[0],e['fov'],native_res)
        hard=dict(e,softness=0.)
        control_native=r.native_visibility(cam,d,n,hard,defaults)
        control_pen=penumbra(control_native,g,named['region'])
        causal=dict(native_small_source=control_pen)
        if family=='soft':
            old_radius=lights[0].radius;lights[0].radius=.005
            near=CS.emitter_near(sc,lights[0]);r.gpu_atlas=r.raster.atlas(sc,lights[0],near)
            ch=dict(row=0,near=near,far=lights[0].range,r_map=.005,contact=CS.contact_distance(lights[0].range),seed=lights[0].seed,detail_shadows=frame[0]['detail_shadows'] and defaults['RenderSpotShadowDetailBias'])
            control_world=r.run(cam,d,n,[lights[0]],plan.plans['Default'][2],world=dict(atlas=np.empty((CS.ATLAS_H_PER_LIGHT,CS.ATLAS_W),np.float32),channels=[ch]),look=dict(strength=1.,contrast=1.,world_amount=1.,reach=0.,screen_amount=1.))[0]
            lights[0].radius=old_radius
            cp=penumbra(control_world,g,named['region']);actual=modes['picked']['penumbra']['width_px']
            causal['world_small_source']=cp
            causal['world_source_width_gain']=actual/cp['width_px'] if actual is not None and cp['width_px'] else None
            if causal['world_source_width_gain'] is None or causal['world_source_width_gain']<1.2:failures.append(preset['name']+' source-size penumbra control')
        row=dict(name=preset['name'],family=family,intent=preset['intent'],yaw=preset['lights'][0]['yaw'],pitch=preset['lights'][0]['pitch'],source_diameter_m=preset['lights'][0]['source_size_m'],ranking=key_rank,modes=modes)
        row['source_control']=causal
        row['double_edge_isolation']=dict(passed=no_double,poison_positive_control=positive)
        row['default_overlap']=overlap_edges(centre_cache['off']['key_vis'],centre_cache['on']['key_vis'],g,named['region'])
        # Overlap compares two simultaneous native/contact edges. A saved world
        # pick replaces native sampling; that path must pass poisoned-map
        # isolation above, rather than match the unrelated native filter edge.
        if family!='silhouette' and modes['on']['native_sampled'] and (row['default_overlap']['profiles']<10 or row['default_overlap']['fraction']>.05):failures.append(preset['name']+' default overlap secondary edge')
        row['soft_on_minus_off']={k:modes['on'][k]-modes['off'][k] for k in ('face_ratio_stops','edge_share','motion_p95','saturated_percent')}
        for key in ('contrast',):
            row['soft_on_minus_off'][key]=modes['on'][key]-modes['off'][key] if modes['on'][key] is not None and modes['off'][key] is not None else None
        row['soft_on_minus_off']['penumbra_px']=modes['on']['penumbra']['width_px']-modes['off']['penumbra']['width_px'] if modes['on']['penumbra']['width_px'] is not None and modes['off']['penumbra']['width_px'] is not None else None
        # Strict schema requirement is reported separately from measured renderer
        # gates. No forged world ownership in the ON/default measurements.
        for mode,m in modes.items():
            label=preset['name']+' '+mode
            if not m['static_repeat_equal'] or m['motion_p95']>MOTION_P95 or m['motion_pop']>MOTION_POP:failures.append(label+' motion/crawl')
            if m['grain_p95'] is None or m['grain_p95']>.04:failures.append(label+' grain')
            if m['clipping_percent']>.1 or m['saturated_percent']>1:failures.append(label+' clipping')
            if preset['name'].endswith('Crisp Fabric Rake') and m['fabric_lit_pixels']<100:failures.append(label+' unlit fabric fixture')
            if family=='silhouette':
                diag=m['diagnostic_shadow']
                for detail in ('nose_brow','jaw','ground'):
                    if diag[detail]['expected']>=30 and diag[detail]['retained']<diag[detail]['expected']*.2:failures.append(label+' lost diagnostic '+detail+' shadow')
            elif m['key_dark_pixels']<30 or m['key_lit_pixels']<30:failures.append(label+' lost key shadow')
            for detail in ('ground',) if family=='silhouette' else ('nose_brow','jaw'):
                expected=m['named_shadow_expected'][detail]
                if expected>=30 and m['named_shadow_pixels'][detail]<expected*.2:failures.append(label+' lost '+detail+' shadow')
            pen=m['penumbra']['width_px']
            if family=='crisp' and (pen is None or pen>TARGETS['crisp']['penumbra_max_px'] or m['contrast'] is None or m['contrast']<TARGETS['crisp']['contrast_min']):failures.append(label+' crisp target')
            if family=='soft' and (pen is None or not TARGETS['soft']['penumbra_min_px']<=pen<=TARGETS['soft']['penumbra_max_px'] or m['contrast'] is None or m['contrast']<TARGETS['soft']['contrast_min']):failures.append(label+' soft target')
            if family=='silhouette' and m['edge_share']<TARGETS['silhouette']['edge_share_min']:failures.append(label+' silhouette target')
        if key_rank[0][1]!=0:failures.append(preset['name']+' key not brightest/closest')
        tile=Image.new('RGB',(452,350),(18,20,25));tile.paste(pair[0],(0,38));tile.paste(pair[1],(230,38))
        draw=ImageDraw.Draw(tile);draw.text((5,3),preset['name'].removeprefix(PREFIX),fill='white');draw.text((5,20),'Relight OFF (native)       Relight ON (saved pick)',fill=(180,185,195))
        panels.append(tile);rows.append(row)
        (out/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
        print('[shadow-presets]',preset['name'],json.dumps({mode:{k:modes[mode][k] for k in ('face_ratio_stops','contrast','penumbra','motion_p95','edge_share')} for mode in modes}),flush=True)
    sheet=Image.new('RGB',(452*4,350*3),(18,20,25))
    for i,p in enumerate(panels):sheet.paste(p,((i%4)*452,(i//4)*350))
    sheet.save(out/'sheet.png')
    return dict(rows=rows,failures=failures,targets=TARGETS,instruments=instruments,planner=plan.source,gpu=r.gl.renderer,
                fixture='Matte neutral proxy; flat white projector cookie; default bounce 0.45; default ACES/mix tone map; no environment light',
                motion_bounds=dict(p95=MOTION_P95,pop=MOTION_POP,step_degrees=.1),sheet=str(out/'sheet.png'))
