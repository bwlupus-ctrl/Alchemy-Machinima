"""WP3 real-GL particle owner, world identity, depth, cap, veil and Director gate.

Production C++ loader, particle owner, cover/state methods and native GLSL execute.
Geometry and GL backend services are fixtures. --mutations must reject all seven
plan controls with behavioural assertions. --source-ref 28a5d96de19 must fail.
Standalone results remain provisional until final viewer build/user acceptance.
"""
import os
os.environ['PYTHONUTF8']='1'
import argparse
import ctypes as C
import json
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np
from relight_3d_gl import GL,SourceTree
from weather_cpu_check import ROOT
from weather_particles_fixture import build
from weather_compute_fixture import function


def enforce_cost(result):
    assert result['memory_bytes']<=16*1024*1024,'WP3 exceeds 16 MiB ceiling'
    tails=[r['cpu_plus_gpu_ms']['p95'] for r in result['cost'] if r['on']]
    tails.append(result['main_plus_remote_cpu_plus_gpu_ms']['p95'])
    assert tails and max(tails)<=.65,'combined preparation/cover/render p95 exceeds 0.65 ms: '+str(tails)


def run(ref='WORKTREE',mutation=None,cost=True):
    gl=GL();print('WGL',gl.renderer,flush=True)
    tree=SourceTree(ref)
    pipe=tree.read('indra/newview/pipeline.cpp')
    main=function(pipe,'void LLPipeline::renderWeather(')
    hook=function(pipe,'void LLPipeline::renderWeatherParticles(')
    assert main.count('if (library) renderWeatherParticles(target, auxiliary);')==1,'disconnected native snowfall call'
    assert 'auxiliary ? get_current_modelview() : glm::make_mat4(gGLLastModelView)' in hook and 'mRT->deferredScreen' in hook,'incorrect native view/depth handoff'
    assert 'gPipeline.renderWeather(&rt.screen, /*auxiliary=*/true, &capture->mWeatherExposure);' in tree.read('indra/newview/llprismlens.cpp'),'disconnected Prism/VCam precipitation'
    native=build(tree,ROOT/'.tmp/weather_port/particles'/(mutation or 'baseline')/str(os.getpid()),gl,mutation)
    def set_(key,value):native.fixture_weather_set(('AlchemyWeather'+key).encode(),value)
    def step(t):native.fixture_weather_step(t)
    def metrics():
        a=np.zeros(11,np.float64);native.metrics(a.ctypes.data);return a
    def values():
        a=np.zeros(8,np.float64);native.weather_values(a.ctypes.data);return a
    def work():
        a=np.zeros(5,np.float64);native.work_counts(a.ctypes.data);return a
    def cover_counts():
        a=np.zeros(2,np.float64);native.cover_counts(a.ctypes.data);return a
    # Real float colour + depth attachments; occluders below are actually rasterized.
    fbo=C.c_uint();gl.gen_fbo(1,C.byref(fbo));textures=[]
    def tex(w,h,internal,fmt,data=None):
        t=C.c_uint();gl.gen_tex(1,C.byref(t));textures.append(t)
        gl.active(0x84C0);gl.bind_tex(0x0DE1,t)
        for p,v in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)):gl.param(0x0DE1,p,v)
        gl.teximage(0x0DE1,0,internal,w,h,0,fmt,0x1406,None if data is None else data.ctypes.data)
        return t
    cover=tex(16,16,0x822E,0x1903,np.ones((16,16),np.float32))
    width,height=257,129
    color=tex(width,height,0x8814,0x1908);depth=tex(width,height,0x8CAC,0x1902)
    sidecars=[tex(width,height,0x8814,0x1908) for _ in range(2)]
    drawbuffers=(C.c_uint*3)(0x8CE0,0x8CE1,0x8CE2)
    def attach():
        gl.bind_fbo(0x8D40,fbo);gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,color,0);gl.fbo_tex(0x8D40,0x8D00,0x0DE1,depth,0)
        for i,t in enumerate(sidecars):gl.fbo_tex(0x8D40,0x8CE1+i,0x0DE1,t,0)
        gl.fn('glDrawBuffers',None,C.c_int,C.c_void_p)(3,drawbuffers)
        assert gl.fbo_status(0x8D40)==0x8CD5
        gl.viewport(0,0,width,height)
    attach()
    identity=np.eye(4,dtype=np.float32)
    projection=np.zeros((4,4),np.float32);near,far=.1,200.
    projection[0,0]=1.;projection[1,1]=width/height
    projection[2,2]=-(far+near)/(far-near);projection[2,3]=-2*far*near/(far-near);projection[3,2]=-1
    cover_matrix=np.diag([1/128,1/128,1/128,1]).astype(np.float32)
    origin=np.zeros(3,np.float64)
    enable=gl.fn('glEnable',None,C.c_uint);depthmask=gl.fn('glDepthMask',None,C.c_ubyte)
    clearcolor=gl.fn('glClearColor',None,*[C.c_float]*4);clear=gl.fn('glClear',None,C.c_uint)
    def reset_image():
        attach();gl.disable(0x0C11);gl.color_mask(1,1,1,1);depthmask(1)
        gl.fn('glClearDepth',None,C.c_double)(1.);clearcolor(0,0,0,1);clear(0x4000|0x100)
        for i in (1,2):
            sentinel=np.array([.25,.5,.75,1],np.float32)*i
            gl.fn('glClearBufferfv',None,C.c_uint,C.c_int,C.c_void_p)(0x1800,i,sentinel.ctypes.data)
        gl.fn('glDepthFunc',None,C.c_uint)(0x0203)
        gl.disable(0x0B71);gl.disable(0x0BE2)
    def check_sidecars():
        attach()
        for i in (1,2):
            gl.fn('glReadBuffer',None,C.c_uint)(0x8CE0+i)
            a=np.empty((height,width,4),np.float32);gl.read(0,0,width,height,0x1908,0x1406,a.ctypes.data)
            assert np.all(a==np.array([.25,.5,.75,1],np.float32)*i),'MRT sidecar sentinel overwritten'
        gl.fn('glReadBuffer',None,C.c_uint)(0x8CE0)
    def render(camera=(0,0,0),frame=1,yaw=0):
        angle=np.deg2rad(yaw);c,s=np.cos(angle),np.sin(angle)
        view=identity.copy();view[:3,:3]=[[c,0,s],[0,1,0],[-s,0,c]]
        view[:3,3]=-view[:3,:3]@np.asarray(camera)
        v,p,c=view.T.copy(),projection.T.copy(),cover_matrix.T.copy()
        native.render(fbo.value,depth.value,cover.value,width,height,v.ctypes.data,
            p.ctypes.data,c.ctypes.data,origin.ctypes.data,frame)
    def image():
        attach();a=np.empty((height,width,4),np.float32);gl.read(0,0,width,height,0x1908,0x1406,a.ctypes.data);return a
    def particles():
        m=metrics();buf=native.buffer();assert buf
        gl.bind_buf(0x90D2,buf);a=np.zeros((int(m[4]),8),np.float32)
        gl.fn('glGetBufferSubData',None,C.c_uint,C.c_ssize_t,C.c_ssize_t,C.c_void_p)(0x90D2,0,a.nbytes,a.ctypes.data)
        gl.bind_buf(0x90D2,0);return a
    def state():
        result=[]
        get=gl.fn('glGetIntegerv',None,C.c_uint,C.c_void_p)
        for name,size in ((0x8CA6,1),(0x8CAA,1),(0x0C02,1),(0x0BA2,4),(0x0B74,1),(0x85B5,1),(0x8894,1),(0x8895,1),(0x90D3,1),(0x84E0,1)):
            v=np.zeros(size,np.int32);get(name,v.ctypes.data);result+=v.tolist()
        for name in (0x0B71,0x0BE2,0x0B44,0x0C11):result.append(gl.fn('glIsEnabled',C.c_ubyte,C.c_uint)(name))
        for index in range(3):
            v=C.c_int();gl.fn('glGetVertexAttribiv',None,C.c_uint,C.c_uint,C.c_void_p)(index,0x8622,C.byref(v));result.append(v.value)
        return result
    reset_image();step(0)
    before=metrics()
    for i in range(12):render()
    assert np.array_equal(metrics(),before),'zero allocation/work while off'
    set_('Enabled',1);set_('ControlMode',1);set_('LocalEnabled',1);set_('TransitionSeconds',0)
    step(.1);step(.2);render();before=work()
    for i in range(60):step(.2+i/60);render()
    assert np.array_equal(work(),before),'library-on/WP3-off performs GL work'
    print('PASS library-on/WP3-off: zero queries, programs, draws or state snapshots',flush=True)
    set_('PrecipVariation',0);set_('PrecipType',1);set_('SnowEnabled',1);set_('SnowRate',12)
    set_('SnowFlakeSize',.08);set_('SnowRange',32);set_('SnowParticleLimit',32768)
    cold=time.perf_counter();step(1);cold_ms=(time.perf_counter()-cold)*1000;step(1.1)
    assert work()[2]==2,'snow-only enable compiles the disabled veil'
    reset_image();render();open_image=image();a=particles();m=metrics()
    assert native.draw_contract()&1,'MRT draw exposes sidecar attachments'
    assert native.draw_contract()&2,'depth texture barrier missing'
    assert native.draw_contract()&4,'sampled depth is writable'
    check_sidecars()
    allocations=metrics()[3];programs=work()[2];set_('SnowEnabled',0);step(1.1)
    reenable=time.perf_counter();set_('SnowEnabled',1);step(1.1);reenable_ms=(time.perf_counter()-reenable)*1000
    assert metrics()[3]==allocations and work()[2]==programs,'re-enable recreates session resources'
    q=work().copy();render();assert work()[1]==q[1] and work()[4]==q[4],'draw queries capabilities/attributes'
    native.evaluated_sky(0,0,0,100,0);reset_image();render()
    assert image()[...,:3].sum()==0,'night/no-moon illuminates flakes with raw sunlight'
    native.evaluated_sky(0,1,0,100,1);reset_image();render()
    assert image()[...,:3].sum()>0,'evaluated moon illumination missing'
    native.evaluated_sky(1,0,0,1.055*(10/3)**(1/2.4)-.055,0)
    print('PASS cold/re-enable lifecycle, cached attributes/capabilities and evaluated night/moon lighting',cold_ms,reenable_ms,flush=True)
    if open_image[:,:,:3].sum()==0:
        pid=native.draw_program();get=gl.fn('glGetUniformfv',None,C.c_uint,C.c_int,C.c_void_p)
        for name in ('snow_view','snow_projection','snow_inverse_projection','snow_screen','snow_size','snow_range','snow_light','snow_cover_valid','weather_surface_cover_matrix','depthMap','weather_surface_cover_map'):
            u=np.zeros(16,np.float32);get(pid,gl.loc(pid,name.encode()),u.ctypes.data);print(name,u.tolist(),flush=True)
        d=np.zeros((height,width),np.float32);gl.read(0,0,width,height,0x1902,0x1406,d.ctypes.data);print('depth',d.min(),d.max(),'error',gl.fn('glGetError',C.c_uint)(),flush=True)
    assert m[0]>0 and open_image[:,:,:3].sum()>0,'native snowfall draw missing '+str((m.tolist(),values().tolist(),native.status(),float(open_image[:,:,:3].sum()),a[:2].tolist()))
    assert m[5]==1,'hardware-depth-enabled assertion'
    assert int(m[6])&0x2001==0x2001,'compute-to-draw publication barrier assertion'
    assert int(m[10])&0x2000,'prior-view-read reuse barrier assertion'
    assert m[4]<=32768 and m[8]<=np.ceil(32768/128),'workgroup-cap assertion'
    assert np.isfinite(open_image).all() and np.isfinite(a[:,:7]).all()
    assert values()[1]==0 and values()[6]==0,'snow-to-rain leakage'
    # Sentinel state after a real instanced draw, including native attribute
    # enables. The next native triangle must retain its position input.
    native.attribute_mask(1);gl.viewport(3,4,width-6,height-8);enable(0x0C11)
    gl.fn('glDepthFunc',None,C.c_uint)(0x0204)
    before=state();render();assert state()==before,'native raster/VAO/cache state leaked'
    native.attribute_mask(0);gl.disable(0x0C11)
    # Same fixed IDs and world positions after micro pan and cell crossing.
    render((.001,0,0));b=particles()
    def shared(x,y,label):
        dx={int(row[7:].view(np.uint32)[0]):row[:3] for row in x}
        dy={int(row[7:].view(np.uint32)[0]):row[:3] for row in y}
        common=dx.keys()&dy.keys();assert len(common)>len(x)*.4,label+' IDs reseeded'
        assert max(np.max(np.abs(dx[k]-dy[k])) for k in common)<2e-4,label+' trajectories moved with camera'
    shared(a,b,'micro-pan')
    reset_image();render(yaw=.02);shared(a,particles(),'micro-yaw')
    assert not np.array_equal(image(),open_image),'screen-fixed flakes ignore camera rotation'
    render(frame=2);shared(a,particles(),'draw-frame')
    render((32.001,0,0));shared(a,particles(),'cell crossing')
    # A remote auxiliary camera evaluates its own source cells at frozen weather time.
    state=values().copy();reset_image();render((4096,0,0));remote=particles()
    assert np.any(np.linalg.norm(remote[:,:3]-[4096,0,0],axis=1)<32),'remote-camera-volume assertion'
    assert image()[:,:,:3].sum()>0,'remote camera sees an empty snow volume'
    assert np.array_equal(values(),state),'auxiliary draw advanced weather'
    assert b'insufficient cover' in native.status(),'remote cover status missing'
    # Region-origin rebases retain the very same global trajectories.
    render((0,0,0));rebased_before=particles();origin[0]=256
    render((-256,0,0));rebased_after=particles();rebased_after[:,0]+=256
    shared(rebased_before,rebased_after,'origin rebase');origin[0]=0
    # Rasterized opaque silhouettes: a solid left half at 2 m, with a narrow
    # silhouette crossing the center. This writes real scene depth before snow.
    def program(v,f):
        p=gl.program()
        for stage,s in ((0x8B31,v),(0x8B30,f)):
            sh=gl.create_shader(stage);raw=C.c_char_p(('#version 420\n'+s).encode());gl.source(sh,1,C.byref(raw),None);gl.compile(sh)
            ok=C.c_int();gl.getshader(sh,0x8B81,C.byref(ok));assert ok.value
            gl.attach(p,sh)
        gl.link(p);ok=C.c_int();gl.getprog(p,0x8B82,C.byref(ok));assert ok.value;return p
    occ=program('void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}',
        'out vec4 frag_color;uniform float fixture_depth;void main(){if(gl_FragCoord.x>128. && abs(gl_FragCoord.x-164.)>3.)discard;gl_FragDepth=fixture_depth;frag_color=vec4(0,0,0,1);}')
    reset_image();enable(0x0B71);depthmask(1);gl.use(occ)
    z=(-projection[2,2]*2+projection[2,3])/2*.5+.5
    gl.uf[1](gl.loc(occ,b'fixture_depth'),z);gl.draw(4,0,3);gl.use(0)
    render();occluded=image()
    assert occluded[:,:128,:3].sum()<open_image[:,:128,:3].sum()*.2,'opaque silhouette occlusion'
    assert occluded[:,130:,:3].sum()>0,'occluder hides all outdoor flakes'
    # Move a depth plane toward fixed flake depths: soft intersections fade
    # continuously, with the exact same native compute list and IDs.
    record=a[(a[:,2]<-1)&(a[:,2]>-24)&(np.abs(a[:,0])<-a[:,2]*.4)&
             (np.abs(a[:,1])<-a[:,2]*.2)&(a[:,3]>0)]
    assert len(record),'soft depth fixture has no flakes'
    selected=record[0];d=-selected[2]
    # Full-depth planes are rendered by the same solid triangle via a second fragment.
    plane=program('void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.,1.);}',
        'out vec4 frag_color;uniform float fixture_depth;void main(){gl_FragDepth=fixture_depth;frag_color=vec4(0,0,0,1);}')
    soft=[]
    # Isolate this particle's pixel; surrounding flakes contribute equally or
    # monotonically as the plane recedes.
    px=int((selected[0]/d*projection[0,0]*.5+.5)*width)
    py=int((selected[1]/d*projection[1,1]*.5+.5)*height)
    for gap in (-.05,.05,.15,.30):
        reset_image();enable(0x0B71);depthmask(1);gl.use(plane)
        z=(-projection[2,2]*(d+gap)+projection[2,3])/(d+gap)*.5+.5
        gl.uf[1](gl.loc(plane,b'fixture_depth'),z);gl.draw(4,0,3);gl.use(0);render()
        soft.append(float(image()[max(0,py-1):py+2,max(0,px-1):px+2,:3].sum()))
    assert soft[-1]>soft[1]>soft[0] and soft[2]>soft[1],'depth-soft-edge assertion '+str(soft)
    print('PASS world IDs/crossing, remote views, real silhouettes/soft depth, cap/barrier/off',flush=True)
    # Every setting row reaches an observable production shader/model consumer.
    # The native reader drives these mutations, with the generated row binding
    # checked against the settings name in binding_check below.
    reset_image();render();initial=image()
    set_('SnowFlakeSize',.015);step(1.1);reset_image();render();assert initial[...,:3].sum()>image()[...,:3].sum(),'flake-size row disconnected'
    extent_before=np.max(np.abs(particles()[:,:3]))
    set_('SnowRange',64);step(1.1);render();assert np.max(np.abs(particles()[:,:3]))>extent_before+20,'snow-range row disconnected'
    for cap in (4096,131072,32768):
        set_('SnowParticleLimit',cap);step(1.1);reset_image();render();m=metrics()
        assert 0<m[4]<=cap and m[7]==cap*32 and m[8]<=np.ceil(cap/128),'flake-cap row disconnected'
    # Rain, Snow, Mixed use the same instanced/compute program for all views.
    set_('SnowRate',0);step(1.5);before=metrics()[0];render();assert metrics()[0]==before,'zero snowfall still draws'
    set_('SnowRate',12);set_('PrecipType',0);step(1.6);assert values()[2]==0,'Rain type keeps snow'
    set_('PrecipType',2);set_('RainRate',200);set_('SnowRate',360);step(1.7)
    assert values()[1]>0 and values()[2]>0 and values()[6]+values()[7]<=1.000001,'Mixed normalization'
    reset_image();render();assert image()[...,:3].sum()>0,'mixed instanced permutation missing'
    set_('PrecipType',1);set_('SnowRate',12);step(1.8)
    # Wind advances once through the native fixed clock, regardless of render count.
    set_('WindSource',1);set_('WindDirection',90);set_('WindSpeed',10);set_('WindVariation',0);set_('SnowFallSpeed',2)
    step(2);old=values();step(2.5);new=values()
    assert new[3]<old[3] and abs((new[5]-old[5])+2*(new[0]-old[0]))<1e-6,'weather-clock wind/fall'
    for i in range(3):render((i*100,0,0))
    assert np.array_equal(values(),new),'double simulation across views'
    # Veil only: measured once, nonnegative, rate/amount/depth monotonic and
    # unknown/covered depth suppressed. Existing mist is explicitly replaced.
    set_('SnowEnabled',0);set_('PrecipVisibilityEnabled',1);step(3)
    veil=[]
    for amount in (0,.25,.5,1):
        set_('PrecipVisibilityAmount',amount);step(3);reset_image();v=metrics()[1];snow_draws=metrics()[0];render();m=metrics()
        assert m[0]==snow_draws and m[7]<=32768*32,'falling-snow row disconnected'
        assert m[1]-v==(1 if amount else 0),'veil-once assertion'
        pixels=image();assert pixels.min()>=0 and np.isfinite(pixels).all();veil.append(float(pixels[height//2,width//2,0]))
    assert all(a<b for a,b in zip(veil,veil[1:])),'veil monotonic amount'
    check_sidecars()
    def veil_at_distance(distance):
        reset_image();enable(0x0B71);depthmask(1);gl.use(plane)
        z=(-projection[2,2]*distance+projection[2,3])/distance*.5+.5
        gl.uf[1](gl.loc(plane,b'fixture_depth'),z);gl.draw(4,0,3);gl.use(0);render()
        return float(image()[height//2,width//2,0])
    distances=[veil_at_distance(d) for d in (2,8,32)]
    rates=values();sigma=3e-4*rates[1]**.75+2.5e-3*rates[2]**.8
    for d,observed in zip((2,8,32),distances):
        assert abs(observed-.8*(1-np.exp(-sigma*d)))<2e-4,'physical veil depth scaling'
    assert distances[0]<distances[1]<distances[2],'veil depth monotonic'
    set_('PrecipType',0);set_('RainRate',5);step(3.5);light_rain=veil_at_distance(8)
    set_('RainRate',80);step(3.6);assert veil_at_distance(8)>light_rain,'veil physical rain-rate scaling'
    reset_image();render(frame=2);assert image()[...,:3].sum()==0,'unknown cover veil'
    gl.active(0x84C0);gl.bind_tex(0x0DE1,cover);roof=np.zeros((16,16),np.float32)
    gl.teximage(0x0DE1,0,0x822E,16,16,0,0x1903,0x1406,roof.ctypes.data)
    reset_image();render();assert image()[...,:3].sum()==0,'covered depth veil'
    # Centre rays travel along +X: the endpoint alone cannot classify exposure.
    def ray_veil(distance, roof):
        gl.active(0x84C0);gl.bind_tex(0x0DE1,cover)
        gl.teximage(0x0DE1,0,0x822E,16,16,0,0x1903,0x1406,roof.ctypes.data)
        reset_image()
        if distance is not None:
            enable(0x0B71);depthmask(1);gl.use(plane)
            z=(-projection[2,2]*distance+projection[2,3])/distance*.5+.5
            gl.uf[1](gl.loc(plane,b'fixture_depth'),z);gl.draw(4,0,3);gl.use(0)
        render(yaw=90)
        return float(image()[height//2,width//2,0])
    rates=values();sigma=3e-4*rates[1]**.75+2.5e-3*rates[2]**.8
    expected=.8*(1-np.exp(-sigma*64))
    doorway=np.zeros((16,16),np.float32);doorway[:,8:12]=1
    foreground=np.zeros((16,16),np.float32);foreground[:,12:]=1
    for roof,label in ((doorway,'doorway toward covered endpoint'),(foreground,'covered foreground toward exposed endpoint')):
        observed=ray_veil(128,roof)
        assert abs(observed-expected)<2e-3,label+' exposed-distance integration '+str((observed,expected))
        outside=ray_veil(180,roof);sky=ray_veil(None,roof)
        assert abs(outside-expected)<2e-3,'geometry crossing cover bounds loses outdoor segment'
        assert abs(outside-sky)<2e-3,'sky/geometry cover-boundary discontinuity'
    print('PASS MRT sentinels, doorway/foreground exposed length, map bounds and sky continuity',flush=True)
    assert '!mWeatherParticles.veilActive()' in tree.read('indra/newview/pipeline.cpp'),'existing mist double counting'
    binding_check(tree,native.status(),mutation)
    # Odd viewport was used above. Repeat the production path at native 4K,
    # including a remote Prism/VCam volume, before optional paired measurement.
    gl.active(0x84C0);gl.bind_tex(0x0DE1,cover)
    gl.teximage(0x0DE1,0,0x822E,16,16,0,0x1903,0x1406,np.ones((16,16),np.float32).ctypes.data)
    # Match production HDR beauty and visible-diffuse sidecar formats.
    width,height=3840,2160;color=tex(width,height,0x881A,0x1908);depth=tex(width,height,0x8CAC,0x1902)
    sidecars=[tex(width,height,0x8C43,0x1908),tex(width,height,0x822B,0x8227)]
    projection[1,1]=width/height
    set_('SnowEnabled',1);set_('PrecipType',1);set_('SnowFlakeSize',.015);set_('SnowRate',1.5);set_('SnowRange',64);step(4)
    reset_image();render();render((4096,0,0));assert np.isfinite(image()).all(),'4K native permutations'
    result={'device':gl.renderer,'memory_bytes':32768*32+2*512*512*4,'veil_samples':veil,
            'cold_prepare_ms':cold_ms,'reenable_prepare_ms':reenable_ms,'hdr_format':'RGBA16F',
            'cost_scope':'Production idle, cover preparation/cache, 256-batch GL fixture shadow submissions, and production particles; viewer geometry/culling services are fixtures.'}
    # Execute the whole production cover call with a 256-batch GL scene.
    caster=program('void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0.8,1.);}', 'void main(){}')
    native.cover_program(caster)
    native.cover_prepare(0,0,0,1);a=cover_counts()
    for i in range(60):native.cover_prepare(i*.01,0,0,0)
    assert np.array_equal(a,cover_counts()),'stationary precipitation repeats full cover traversal'
    native.cover_prepare(32.1,0,0,0);assert cover_counts()[0]==a[0]+1,'camera-cell cover invalidation missing'
    native.cover_prepare(32.1,0,0,1);assert cover_counts()[0]==a[0]+2,'geometry revision cover invalidation missing'
    print('PASS production cover traversal/cache: stationary reuse, camera cell and geometry revision',flush=True)
    if cost:
        # Queries are read after the whole sample batch, never at the draw's
        # current frame. No timer wait/readback exists in the production owner.
        def distribution(a):return {k:float(v) for k,v in zip(('median','p95','p99','min','max'),np.percentile(a,[50,95,99,0,100]))}
        tick=0
        def combined(remote=False):
            nonlocal tick
            # Preparation, real cover traversal/cache and all target rendering
            # share one timed region. Include moving-cell/dirty frames in batch.
            tick+=1;step(20+tick/60)
            cell=(tick//120)*32.1
            native.cover_prepare(cell,0,0,int(tick%120==0))
            render((cell,0,0),frame=-1)
            if remote:render((4096,0,0),frame=-1)
        runs=[]
        for repeat in range(3):
            for on in ((False,True) if repeat%2==0 else (True,False)):
                set_('SnowEnabled',int(on));set_('PrecipVisibilityEnabled',int(on));step(5+repeat)
                reset_image();warm=time.perf_counter()
                for _ in range(120):combined()
                warm=time.perf_counter()-warm
                queries=(C.c_uint*600)();gl.gen_queries(600,queries);cpu=[]
                for q in queries:
                    gl.begin_query(0x88BF,q);t=time.perf_counter();combined();cpu.append((time.perf_counter()-t)*1000);gl.end_query(0x88BF)
                gl.finish();gpu=[]
                for q in queries:
                    n=C.c_uint64();gl.query_u64(q,0x8866,C.byref(n));gpu.append(n.value/1e6)
                gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(600,queries)
                runs.append({'repeat':repeat,'on':on,'warmup_seconds':warm,'gpu_ms':distribution(gpu),'cpu_ms':distribution(cpu),
                             'cpu_plus_gpu_ms':distribution(np.asarray(cpu)+gpu)})
                print('4K paired cost',json.dumps(runs[-1]),flush=True)
        set_('SnowEnabled',1);set_('PrecipVisibilityEnabled',1);step(9)
        for _ in range(120):combined(True)
        queries=(C.c_uint*600)();gl.gen_queries(600,queries)
        dual_cpu=[]
        for q in queries:
            gl.begin_query(0x88BF,q);t=time.perf_counter();combined(True);dual_cpu.append((time.perf_counter()-t)*1000);gl.end_query(0x88BF)
        gl.finish();dual=[]
        for q in queries:
            n=C.c_uint64();gl.query_u64(q,0x8866,C.byref(n));dual.append(n.value/1e6)
        dual_total=distribution(np.asarray(dual_cpu)+dual)
        result.update(cost=runs,main_plus_remote_gpu_ms=distribution(dual),main_plus_remote_cpu_plus_gpu_ms=dual_total,cover_traversal_batches=cover_counts().tolist(),
            cost_status='PASS' if all(r['cpu_plus_gpu_ms']['p95']<=.65 for r in runs if r['on']) else 'FAIL')
        if dual_total['p95']>.65:result['cost_status']='FAIL'
        (ROOT/'.tmp/weather_port/WP3_COST.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        enforce_cost(result)
    snow_draws_before_unsupported=metrics()[0]
    native.texture_capability(0,1);reset_image();render()
    assert b'Unavailable' in native.status() and metrics()[0]==snow_draws_before_unsupported,'GL 4.3 feedback path accepted'
    native.texture_capability(1,0);render()
    assert b'Unavailable' in native.status() and metrics()[0]==snow_draws_before_unsupported,'missing texture barrier accepted'
    native.capability(0,1);reset_image();render()
    assert b'Unavailable' in native.status(),'unsupported capability status'
    assert metrics()[0]==snow_draws_before_unsupported,'unsupported GL draws particles'
    assert gl.fn('glGetError',C.c_uint)()==0,'GL state error'
    print('PASS weather-clock wind, single physical veil, cover, unsupported status',flush=True)
    return result


def binding_check(tree,status,mutation):
    from weather_binding_check import build as bindings,settings_table
    table=settings_table(tree.read('indra/newview/app_settings/settings.xml'))
    tracked=tree.read('indra/newview/alpanelweathersettings.cpp')
    rows={r.get('setting'):r for r in ET.fromstring(tree.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml')).iter('kit_row')}
    suffixes=('SnowEnabled','PrecipType','SnowRate','SnowFlakeSize','SnowFallSpeed','SnowRange','SnowParticleLimit','PrecipVisibilityEnabled','PrecipVisibilityAmount')
    for suffix in suffixes:
        key='AlchemyWeather'+suffix;row=rows[key]
        assert row.get('bind')=='setting' and row[0].get('control_name')==key,'disconnected Director row '+key
        assert '"'+key+'"' in tracked,'missing preset/reset tracking '+key
    assert table['AlchemyWeatherSnowEnabled']['Value']==table['AlchemyWeatherPrecipVisibilityEnabled']['Value']=='0','expensive defaults enabled'
    native=bindings(ROOT/'.tmp/weather_port/particles/bindings'/str(os.getpid()),mutation,tree.ref)
    for preset,rate in ((5,.3),(6,1.5),(7,5),(8,12)):
        native.set(b'AlchemyWeatherPrecipVisibilityEnabled',0)
        native.command(preset,0,b'')
        assert abs(native.get(b'AlchemyWeatherSnowRate')-rate)<1e-5 and native.get(b'AlchemyWeatherSnowEnabled')==1
        assert native.get(b'AlchemyWeatherPrecipType')==1
        assert native.get(b'AlchemyWeatherSnowReliefEnabled')==0,'Snow preset enables relief'
        assert native.get(b'AlchemyWeatherSnowAccumEnabled')==0 and native.get(b'AlchemyWeatherPrecipVisibilityEnabled')==0,'Snow preset enables secondary features'
    assert native.get(b'AlchemyWeatherWindSpeed')>=15 and native.get(b'AlchemyWeatherWindSource')==1,'Blizzard wind'
    native.command(0,1,b'');assert native.get(b'AlchemyWeatherSnowRate')==0,'Stop misses snow'
    native.snow_capability(0)
    for suffix in suffixes:
        widget=rows['AlchemyWeather'+suffix].get('widget')
        assert native.surface_row_enabled(widget.encode())==0,'unsupported row remains enabled '+widget
    native.particle_readout.argtypes=[C.c_char_p];native.particle_readout.restype=C.c_char_p
    assert b'Unavailable' in native.particle_readout(status),'unsupported status is hidden while off'
    native.snow_capability(1)
    assert native.particle_readout(status)==status,'Director particle status disconnected'
    assert 'preset["version"].asInteger() < 4' in tracked,'old presets do not disable WP3 gates'
    print('PASS all nine Director rows, snow presets/Blizzard/Stop/reset tracking/status/capability',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-ref',default='WORKTREE')
    p.add_argument('--mutation',choices=['depth','view-seed','frame-seed','barrier','reuse-barrier','cap','main-camera','veil-twice','snow-relief',
                                        'mrt','ray-endpoint','cover-cache','texture-barrier','cap-cache','raw-light'])
    p.add_argument('--mutations',action='store_true');p.add_argument('--no-cost',action='store_true');a=p.parse_args()
    result=run(a.source_ref,a.mutation,not a.no_cost)
    if a.mutations:
        for mutation,assertion in [('depth','hardware-depth-enabled'),('view-seed','IDs reseeded'),('frame-seed','IDs reseeded'),
            ('barrier','publication barrier'),('reuse-barrier','reuse barrier'),('cap','workgroup-cap'),('main-camera','remote-camera-volume'),
            ('veil-twice','veil-once'),('snow-relief','Snow preset enables relief'),
            ('mrt','MRT draw exposes sidecar attachments'),('ray-endpoint','doorway toward covered endpoint exposed-distance integration'),
            ('cover-cache','stationary precipitation repeats full cover traversal'),
            ('texture-barrier','depth texture barrier missing'),('cap-cache','library-on/WP3-off performs GL work'),
            ('raw-light','night/no-moon illuminates flakes with raw sunlight')]:
            r=subprocess.run([sys.executable,__file__,'--source-ref',a.source_ref,'--mutation',mutation,'--no-cost'],capture_output=True,text=True,encoding='utf-8',timeout=240)
            (ROOT/'.tmp/weather_port'/('wp3_mutation_'+mutation+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
            assert r.returncode!=0 and assertion in r.stderr,'mutation survived or missed assertion '+mutation+'\n'+r.stdout+r.stderr
            print('mutation rejected:',mutation,'('+assertion+')',flush=True)
    print(json.dumps(result),flush=True)


if __name__=='__main__':main()
