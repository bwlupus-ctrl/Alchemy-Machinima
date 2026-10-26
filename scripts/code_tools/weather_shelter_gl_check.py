"""WP7 real-WGL ownership, directional cover, publication, bindings and cost gate.

Production CPU planner, map preparation/traversal, native masked-roof policy,
publication, sampler mapping and GLSL consumers execute. Viewer geometry/culling
services are a bounded 256-object fixture. Native scene acceptance is provisional.
All deliberate mutations must fail named assertions; immutable pre-WP7 must fail.
"""
import os
os.environ['PYTHONUTF8']='1'
import argparse
import ctypes as C
import json
from pathlib import Path
import subprocess
import sys
import time
import gc
from functools import lru_cache
import xml.etree.ElementTree as ET
import numpy as np
from relight_3d_gl import GL, SourceTree, SHADERS
from weather_compute_fixture import function
from weather_cpu_check import ROOT
from weather_shelter_fixture import build

PRE_WP7='80abb6c19d6b0e601a56ebf28ca05d6e6058c74e'
MUTATIONS={
 'reverse-shear':'windward doorway', 'tilted-height':'untilted height source',
 'last-view':'windward doorway', 'forged-allocation':'failed allocation must remain unknown',
 'publish-barrier':'raster publication barrier', 'mixed-generation':'complete current publication',
 'blend-leak':'render-state sentinel', 'draw-buffer-leak':'render-state sentinel',
 'listener-wait':'current-frame listener wait', 'moving-roof':'windward wall and leeward wall',
 'border-blend':'near/far border blend',
 'surface-self':'sloped exposed roof shelters itself',
 'numeric-publication':'non-finite matrix falsely published',
}


def contracts(tree):
    pipe=tree.read('indra/newview/pipeline.cpp')
    assert 'void LLPipeline::generateWeatherAdvancedShelter(' in pipe,'WP7 production map owner missing'
    advanced=function(pipe,'void LLPipeline::generateWeatherAdvancedShelter(')
    draw=function(pipe,'bool LLPipeline::drawWeatherCover(')
    assert 'renderShadow(' in draw and 'sShadowDrawFilter = 1' in draw,'native opaque/masked roof traversal disconnected'
    assert 'glDrawBuffers(1, &no_colour)' in draw,'map exposes unwritten attachments'
    assert all(name not in advanced for name in ('mShadow[','mSpotShadow','mHeroProbe','glReadPixels','glClientWaitSync')),'foreign target/readback owner'
    assert 'mWeatherShelter.prepare()' in tree.read('indra/newview/alpanelweathersettings.cpp'),'idle preparation disconnected'
    display=tree.read('indra/newview/llviewerdisplay.cpp')
    assert 'generateWeatherAdvancedShelter(*LLViewerCamera::getInstance())' in display,'map preparation call disconnected'
    rain=function(pipe,'void LLPipeline::renderWeather(')
    assert 'ALWeatherShelter::auxiliaryAllowed(library, library_frame.active(), advanced_auxiliary)' in rain,'legacy auxiliary shelter gate disconnected'
    assert 'legacy_auxiliary ? sWeatherController.sample(now, lightning_config)' in rain,'legacy auxiliary advances a scheduler'
    from weather_binding_check import settings_table
    import re
    settings=settings_table(tree.read('indra/newview/app_settings/settings.xml'))
    reader=function(tree.read('indra/newview/alpanelweathersettings.cpp'),'ALWeatherBridge::Settings readLibrarySettings(')
    for key in re.findall(r'gSavedSettings,\s*"([^"]+)"',reader):
        assert key in settings,'missing production setting: '+key
    assert 'advanced_rain ? mWeatherShelter.rainProgram()' in rain and 'mWeatherShelter.bind(&rain_program' in rain,'rain consumer disconnected'
    assert 'gPipeline.renderWeather(&rt.screen, /*auxiliary=*/true, &capture->mWeatherExposure);' in tree.read('indra/newview/llprismlens.cpp'),'auxiliary consumer disconnected'
    # Pin unchanged legacy producer; no new default shelter choice.
    assert function(pipe,'void LLPipeline::generateWeatherRainOcclusion(')==function(SourceTree('330d012e161').read('indra/newview/pipeline.cpp'),'void LLPipeline::generateWeatherRainOcclusion('),'default legacy shelter changed'


def run(ref='WORKTREE',mutation=None,cost=True,benchmark_context=False,gl=None,context_key=''):
    tree=SourceTree(ref);contracts(tree)
    if mutation in ('border-blend','surface-self'):
        original=tree
        class BorderMutant:
            ref=original.ref
            def exists(self,path):return original.exists(path)
            def read(self,path):
                source=original.read(path)
                if path.endswith('/weatherShelterF.glsl'):
                    if mutation=='border-blend':
                        a='return mix(close_open,distant_open,smoothstep(0.70,0.95,border));'
                        assert a in source;source=source.replace(a,'return border<0.95?close_open:distant_open;')
                    else:
                        a='float receiver=q.z+dot(slope,(texel+0.5)/size-q.xy);'
                        assert a in source;source=source.replace(a,'float receiver=q.z;')
                return source
        tree=BorderMutant()
    gl=gl or GL();print('WGL',gl.renderer,flush=True)
    # Resolve fixture dispatch/uniform locations once, just as the viewer does.
    # Dynamic ctypes wrapper creation/GC is not weather frame work.
    gl.fn=lru_cache(None)(gl.fn)
    gl.loc=lru_cache(None)(gl.loc)
    native=build(tree,ROOT/'.tmp/weather_port/shelter'/(mutation or 'baseline')/(str(os.getpid())+('_'+context_key if context_key else '')),gl,mutation)
    def set_(name,value):native.fixture_weather_set(('AlchemyWeather'+name).encode(),value)
    def step(t):native.fixture_weather_step(t)
    def metrics():
        a=np.zeros(14,np.float64);native.shelter_metrics(a.ctypes.data);return a
    budgeted=hasattr(native,'shelter_pending')
    actual_time=0.;request_time=0.
    def prepare(t=1,x=0,y=0,z=0,dirty=0,settle=True):
        nonlocal actual_time,request_time
        if not budgeted:
            native.shelter_prepare(t,x,y,z,dirty);return
        actual_time+=max(1/60,t-request_time);request_time=t
        native.shelter_prepare(actual_time,x,y,z,dirty)
        if settle and np.isfinite([x,y,z]).all():
            # Readbacks assert eventual correctness after the documented tiled
            # latency. Cost/rate tests use one real display frame per call.
            for _ in range(1024):
                if native.shelter_pending()==0 and (not native.shelter_basic_allocated() or native.cover_pending()==0):break
                actual_time+=1/60
                native.shelter_prepare(actual_time,x,y,z,0)
            else:raise AssertionError('shelter refresh exceeded bounded settling frames')
    def program(v,f):
        pr=gl.program()
        for source,stage in ((v,0x8B31),(f,0x8B30)):
            shader=gl.create_shader(stage);raw=('#version 420\n'+source).encode();ptr=C.c_char_p(raw)
            gl.source(shader,1,C.byref(ptr),None);gl.compile(shader);ok=C.c_int();gl.getshader(shader,0x8B81,C.byref(ok))
            if not ok.value:
                log=C.create_string_buffer(32000);gl.slog(shader,len(log),None,log);raise AssertionError(log.value.decode())
            gl.attach(pr,shader)
        gl.link(pr);ok=C.c_int();gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log=C.create_string_buffer(32000);gl.plog(pr,len(log),None,log);raise AssertionError(log.value.decode())
        return pr
    full_vertex='''out vec2 uv;void main(){vec2 p=vec2((gl_VertexID==1)?3:-1,(gl_VertexID==2)?3:-1);uv=p*.5+.5;gl_Position=vec4(p,0,1);}'''
    caster_vertex='''uniform mat4 fixture_mvp;uniform vec4 fixture_rect;uniform float fixture_z;
out vec4 post_pos;out float target_pos_x;out vec4 vertex_color;out vec2 vary_texcoord0;out vec3 vary_actor_fx_position;
void main(){vec2 v[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
vary_texcoord0=v[gl_VertexID];vary_actor_fx_position=vec3(0);vertex_color=vec4(1);
vec3 agent=vec3(fixture_rect.xy+v[gl_VertexID]*fixture_rect.zw,fixture_z);
if(fixture_rect.w<0)agent=vec3(fixture_rect.x,fixture_rect.y+v[gl_VertexID].x*fixture_rect.z,-v[gl_VertexID].y*fixture_rect.w);
if(fixture_z<0)agent.z=-fixture_z+.5*agent.x;
post_pos=fixture_mvp*vec4(agent,1);target_pos_x=0;gl_Position=post_pos;}'''
    caster_fragment='''uniform float fixture_alpha;vec4 diffuseLookup(vec2 uv){return vec4(1,1,1,fixture_alpha);}
bool actorFxDissolveDiscard(vec3 p){return false;}\n'''+tree.read(SHADERS+'/class1/deferred/shadowAlphaMaskF.glsl')
    caster=program(caster_vertex,caster_fragment);gl.use(caster)
    gl.uf[1](gl.loc(caster,b'minimum_alpha'),.5)
    native.shelter_program(caster)
    helper=tree.read(SHADERS+'/class1/deferred/weatherShelterF.glsl')
    precip=tree.read(SHADERS+'/class1/deferred/weatherPrecipCoverF.glsl')
    # Native links separate fragment objects, where shared uniform declarations
    # are legal. These compact fixture programs concatenate their modules.
    precip=precip.replace('uniform mat4 weather_shelter_close_matrix, weather_shelter_distant_matrix;','')
    material=tree.read(SHADERS+'/class1/deferred/weatherMaterialF.glsl')
    wrappers='''in vec2 uv;uniform vec4 sample_line;out vec4 result;
vec3 sampleAgent(){return vec3(mix(sample_line.x,sample_line.y,uv.x),sample_line.z,sample_line.w);}
'''
    consumers=[program(full_vertex,helper+wrappers+'void main(){float a=weatherShelterOpen(sampleAgent());result=vec4(a,a,a,1);}')]
    consumers.append(program(full_vertex,helper+precip+wrappers+'void main(){float a=weatherPrecipOpen(sampleAgent());result=vec4(a,a,a,1);}'))
    consumers.append(program(full_vertex,helper+material+wrappers+'''vec3 srgb_to_linear(vec3 a){return a;}vec3 linear_to_srgb(vec3 a){return a;}
void main(){float a=weatherSurfaceOpen(sampleAgent());result=vec4(a,a,a,1);}'''))
    surface_probe=program(full_vertex,helper+material+'''in vec2 uv;uniform vec4 sample_line;out vec4 result;
vec3 srgb_to_linear(vec3 a){return a;}vec3 linear_to_srgb(vec3 a){return a;}
void main(){float x=mix(sample_line.x,sample_line.y,uv.x);
float a=weatherSurfaceOpen(vec3(x,uv.y*2-1,4+.5*x+sample_line.w));result=vec4(a,a,a,1);}''')
    debug=program(full_vertex,helper+wrappers+'void main(){result=vec4(weather_shelter_debug!=0?weatherShelterDebug(sampleAgent()):vec3(0),1);}')
    veil_cost=program(full_vertex,helper+precip+wrappers+'''void main(){
float d=weatherPrecipExposedDistance(sampleAgent(),normalize(vec3(1,.2,.1)),3000.0);result=vec4(d*.001); }''')
    veil_probe=program(full_vertex,helper+precip+wrappers+'''void main(){
float d=weatherPrecipExposedDistance(sampleAgent(),vec3(1,0,0),10.0);result=vec4(d); }''')
    rain_source=tree.read(SHADERS+'/class1/deferred/weatherAdvancedRainF.glsl')
    rain_declarations=rain_source[:rain_source.index('vec4 getPosition(')]
    rain_declarations=rain_declarations.replace('uniform int weather_shelter_debug;','')
    rain_cost=program(full_vertex,'#define WEATHER_ADVANCED_SHELTER 1\n#define WEATHER_RAIN_OCCLUSION 1\n'+helper+rain_declarations+
        function(rain_source,'float weatherRainExposure(')+wrappers+'''uniform int fixture_advanced;
void main(){float open=0;for(int i=0;i<12;++i)open+=fixture_advanced!=0?
weatherRainExposure(sampleAgent()+vec3(0,0,float(i)*.5)):1.0;result=vec4(open/12.0);}''')
    fbo=C.c_uint();gl.gen_fbo(1,C.byref(fbo));textures=[]
    def texture(w,h):
        tex=C.c_uint();gl.gen_tex(1,C.byref(tex));textures.append(tex)
        gl.active(0x84C0);gl.bind_tex(0x0DE1,tex)
        for p,v in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)):gl.param(0x0DE1,p,v)
        gl.teximage(0x0DE1,0,0x881A,w,h,0,0x1908,0x1406,None);return tex
    width,height=257,1;outputs=[texture(width,height) for _ in range(3)]
    db=(C.c_uint*3)(0x8CE0,0x8CE1,0x8CE2)
    def target():
        gl.bind_fbo(0x8D40,fbo)
        for i,tex in enumerate(outputs):gl.fbo_tex(0x8D40,0x8CE0+i,0x0DE1,tex,0)
        gl.fn('glDrawBuffers',None,C.c_int,C.c_void_p)(3,db);gl.viewport(0,0,width,height)
        gl.disable(0x0B71);gl.disable(0x0BE2);gl.disable(0x0B44);gl.disable(0x0C11);gl.color_mask(1,1,1,1)
    def state():
        result=[];get=gl.fn('glGetIntegerv',None,C.c_uint,C.c_void_p)
        for key,size in ((0x8CA6,1),(0x8CAA,1),(0x0C02,1),(0x0BA2,4),(0x0B74,1),(0x8825,1),(0x8826,1),(0x8827,1),(0x84E0,1),(0x8B8D,1)):
            a=np.zeros(size,np.int32);get(key,a.ctypes.data);result+=a.tolist()
        result += [gl.fn('glIsEnabled',C.c_ubyte,C.c_uint)(n) for n in (0x0B71,0x0BE2,0x0B44,0x0C11)]
        return result
    def sample(x,x1=None,z=0,kind=0,read=True,pid=None,y=0,scale=1,advanced=False):
        target();pr=pid or consumers[kind];native.shelter_bind(pr,kind)
        gl.viewport(0,0,width//scale,height//scale)
        gl.ui(gl.loc(pr,b'fixture_advanced'),int(advanced))
        gl.fn('glUniform4f',None,C.c_int,*[C.c_float]*4)(gl.loc(pr,b'sample_line'),x,x if x1 is None else x1,y,z)
        beauty=C.c_uint(0x8CE0);gl.fn('glDrawBuffers',None,C.c_int,C.c_void_p)(1,C.byref(beauty))
        gl.draw(4,0,3)
        if not read:return
        gl.fn('glReadBuffer',None,C.c_uint)(0x8CE0)
        a=np.empty((height,width,4),np.float32);gl.read(0,0,width,height,0x1908,0x1406,a.ctypes.data);return a
    def guarded(t,**kw):
        target()
        sentinel=np.array([.25,.5,.75,1],np.float32)
        for attachment in (1,2):
            gl.fn('glClearBufferfv',None,C.c_uint,C.c_int,C.c_void_p)(0x1800,attachment,sentinel.ctypes.data)
        before=state();prepare(t,**kw)
        assert before==state(),'render-state sentinel'
        for attachment in (1,2):
            gl.fn('glReadBuffer',None,C.c_uint)(0x8CE0+attachment)
            pixels=np.empty((height,width,4),np.float32);gl.read(0,0,width,height,0x1908,0x1406,pixels.ctypes.data)
            assert np.all(pixels==sentinel),'unwritten attachment sentinel'
        gl.fn('glReadBuffer',None,C.c_uint)(0x8CE0)
        assert metrics()[7]==0,'current-frame listener wait'
    target();native.shelter_sync();before=metrics().copy()
    for i in range(10):prepare(i/60)
    assert np.array_equal(metrics(),before),'zero allocation/work while WP7 off'
    set_('Enabled',1);set_('ControlMode',1);set_('LocalEnabled',1);set_('TransitionSeconds',0);step(0)
    before=metrics().copy()
    for i in range(10):step(i/60);prepare(i/60)
    assert np.array_equal(metrics()[:13],before[:13]),'library-on/WP7-off performs GL/map work'
    set_('WindSource',1);set_('WindDirection',270);set_('WindSpeed',14);set_('PrecipVariation',0)
    set_('AdvancedShelterEnabled',1);step(.2);step(.3)
    if mutation=='forged-allocation':
        native.shelter_failure(1);set_('ShelterNearResolution',512);step(.4)
        guarded(.4)
        assert metrics()[2]==0,'failed allocation must remain unknown'
        return {}
    guarded(.9)
    assert metrics()[2]==1,'complete current publication'
    assert native.shelter_basic_allocated()==0,'rain-only shelter allocates an unused second vertical map'
    set_('PersistentWetEnabled',1);set_('StartWetness',1);step(.31);guarded(1)
    assert int(metrics()[6])&0x408==0x408,'raster publication barrier'
    # Known falling direction +X: open windward doorway, leeward overhang cover.
    assert sample(1)[0,0,0]>.99 and abs(sample(9)[0,0,0])<.01,'windward doorway / leeward overhang'
    for kind in (1,2):
        assert sample(1,kind=kind)[0,0,0]>.99 and abs(sample(9,kind=kind)[0,0,0])<.01,'production snow/material consumer'
    assert abs(sample(-5,kind=1,pid=veil_probe)[0,0,0]-7)<.7,'tilted doorway exposed-distance integration'
    assert sample(200,kind=1,pid=veil_probe)[0,0,0]==0,'unknown veil interval invents coverage'
    native.shelter_scene(2,0,4,1);guarded(1.04,x=.125,dirty=1)
    assert sample(-1,z=2)[0,0,0]>.99 and abs(sample(.5,z=2)[0,0,0])<.01,'windward wall and leeward wall'
    native.shelter_scene(0,0,4,1);guarded(1.08,dirty=1)
    # Basic vertical data must remain independent of the tilted reach map.
    native.shelter_bind(consumers[1],1)
    mat=np.zeros(16,np.float32);gl.fn('glGetUniformfv',None,C.c_uint,C.c_int,C.c_void_p)(consumers[1],gl.loc(consumers[1],b'weather_surface_cover_matrix'),mat.ctypes.data)
    assert mat[8]==mat[9]==0,'untilted height source'
    first=sample(-2,12)
    for view in ('main','probe','Prism','VCam'):
        guarded(1.1,x=.001,y=.001)
        assert np.array_equal(first,sample(-2,12)),view+' no snapping/swimming'
    for x in (.125,.5):
        guarded(1.15+x*.01,x=x,y=x)
        assert np.array_equal(first,sample(-2,12)),'world cells swim under texel crossing'
    native.shelter_origin(256,256,0);guarded(1.2,x=-256,y=-256)
    rebased=sample(-258,-244,y=-256)
    assert np.max(abs(first-rebased))<.002,'origin rebase exposure changes'
    native.shelter_origin(0,0,0);guarded(1.3)
    # Moving roof revisions coalesce, then update within a bounded interval.
    native.shelter_scene(0,20,4,1);guarded(1.4,dirty=1)
    assert sample(9)[0,0,0]>.99,'moving roof dirty update'
    counts=metrics()[0]
    for i in range(1,10):guarded(1.4+i*.001,dirty=1,settle=False)
    assert metrics()[0]-counts<=9,'unbounded dirty traversals'
    native.shelter_scene(0,0,4,.4);guarded(1.5,dirty=1)
    assert sample(4)[0,0,0]>.99,'native masked roof cutoff rejected transparent texel'
    native.shelter_scene(0,0,4,1);guarded(1.6,dirty=1)
    assert abs(sample(4)[0,0,0])<.01,'native opaque/masked roof cutoff'
    # Bias is metres, not NDC or an unconditional open/closed switch.
    assert sample(4,z=3.8)[0,0,0]<.95,'bias leak under thin roof'
    set_('ShelterBias',1);step(.5);guarded(1.7)
    assert sample(4,z=3.8)[0,0,0]>.99,'Bias row has no observable effect'
    set_('ShelterBias',.12);set_('ShelterSoftness',2);step(.6);guarded(1.8)
    assert .1<sample(4,z=3.5)[0,0,0]<1,'Softness row has no observable effect'
    set_('ShelterSoftness',.35);step(.7)
    native.shelter_scene(1,0,4,1);guarded(2,dirty=1)
    line=sample(18,40)[0,:,0]
    assert np.max(abs(np.diff(line)))<.01 and np.max(line)<.01,'near/far seam'
    # Coarse and fine levels can disagree at a narrow roof edge. Overwrite the
    # completed distant depth as an open coarse cell to test compare-first
    # blending itself, rather than allowing two equal levels to hide a seam.
    native.shelter_layer.restype=C.c_uint
    distant=native.shelter_layer(1);n=int(metrics()[4]);open_depth=np.ones((n,n),np.float32)
    gl.active(0x84C0);gl.bind_tex(0x0DE1,distant)
    gl.fn('glTexSubImage2D',None,C.c_uint,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_uint,C.c_uint,C.c_void_p)(0x0DE1,0,0,0,n,n,0x1902,0x1406,open_depth.ctypes.data)
    native.shelter_sync()
    blend=sample(22,32)[0,:,0]
    assert np.max(abs(np.diff(blend)))<.04 and np.any((blend>.1)&(blend<.9)),'near/far border blend'
    guarded(2.04,dirty=1)
    assert abs(sample(90)[0,0,0])<.01 and sample(200)[0,0,0]<0,'far-camera coverage/unknown'
    # Follow wind off is strictly vertical and must change overhang exposure.
    set_('ShelterTiltEnabled',0);step(.8);native.shelter_scene(0,0,4,1);guarded(2.1,dirty=1)
    assert sample(9)[0,0,0]>.99,'Follow weather wind row has no observable effect'
    set_('ShelterTiltEnabled',1);step(.9);guarded(2.2)
    assert abs(sample(9)[0,0,0])<.01,'Follow weather wind tilt missing'
    # Historical paired-cost refs predate Wind scale shelter support. The final
    # regression gate separately requires the shared helper on current code.
    if 'ALWeatherShelter::effectiveWind(' in function(tree.read('indra/newview/pipeline.cpp'),'void LLPipeline::generateWeatherAdvancedShelter('):
        native.illumination_setting.argtypes=[C.c_char_p,C.c_double]
        native.illumination_setting(b'AlchemyWeatherWindScale',0)
        guarded(2.21,dirty=1)
        assert sample(9)[0,0,0]>.99,'Wind scale zero still tilts shelter away from vertical rain'
        native.illumination_setting(b'AlchemyWeatherWindScale',1)
        guarded(2.22,dirty=1)
        assert abs(sample(9)[0,0,0])<.01,'Wind scale one loses nonzero weather wind shelter'
    # Tiny changes in mean wind stay spatially continuous at stable receivers;
    # gusts are never inputs to the map planner.
    before_mean=sample(-2,12)
    set_('WindSpeed',14.01);step(.91);guarded(2.24,dirty=1)
    assert np.mean(abs(before_mean-sample(-2,12)))<.02,'mean wind discontinuity'
    set_('RainFallSpeed',80);step(.92);guarded(2.28)
    assert sample(9)[0,0,0]>.99,'existing Fall speed is disconnected from shelter angle'
    set_('RainFallSpeed',28);step(.93);guarded(2.32)
    before_debug=sample(4,pid=debug)
    set_('ShelterDebug',1);step(.95);guarded(2.36)
    assert not np.array_equal(before_debug,sample(4,pid=debug)),'Show shelter row has no observable effect'
    assert not np.array_equal(sample(4,pid=debug),sample(160,pid=debug)),'Show shelter coverage visualization'
    set_('ShelterDebug',0);step(.96)
    set_('ShelterBias',0);step(.97)
    native.shelter_scene(4,0,4,1);guarded(2.4,dirty=1)
    assert np.min(sample(40,60,kind=2,pid=surface_probe)[...,:3])>.99,'sloped exposed roof shelters itself'
    assert np.max(abs(sample(40,60,z=-1,kind=2,pid=surface_probe)[...,:3]))<.01,'sloped covered receiver leaks'
    set_('ShelterBias',.12);step(.98)
    native.shelter_scene(0,0,4,1);guarded(2.5,dirty=1)
    bindings(tree,native,metrics,set_,step,prepare)
    # Every run exercises a real allocation failure, not just the mutation run.
    native.shelter_failure(1);set_('ShelterNearResolution',512);step(7);guarded(7)
    assert metrics()[2]==0 and b'failed' in native.shelter_status(),'failed allocation must remain unknown'
    native.shelter_failure(0);set_('ShelterNearResolution',1024);step(8);guarded(8)
    assert metrics()[2]==1,'allocation recovery publication'
    guarded(8.1,x=float('nan'),dirty=1)
    assert metrics()[2]==0,'invalid camera falsely published'
    guarded(8.2,dirty=1)
    assert metrics()[2]==1,'bounded traversal failure recovery'
    native.shelter_invalid_publish()
    assert metrics()[2]==0,'non-finite matrix falsely published'
    guarded(8.3,dirty=1)
    assert metrics()[2]==1,'invalid publication recovery'
    session=metrics()[[3,4,5,11]].copy()
    set_('Enabled',0);step(9)
    assert metrics()[2]==0,'master-off retained valid shelter stamp'
    set_('Enabled',1);step(10);guarded(10)
    assert np.array_equal(session,metrics()[[3,4,5,11]]),'master toggle recreated session resources/program'
    result={'device':gl.renderer,'source_ref':ref,'memory_bytes':int(metrics()[5]),'scene_objects':256,
        'scope':'Whole production idle, basic + tilted cover traversal/cull services, dirty/angular/moving-roof updates, publication, full production rain shaders/native uploads (half-resolution main, full-resolution auxiliary), production snowfall/veil owner and wet-material coverage; scene services are fixtures.'}
    assert result['memory_bytes']<=16*1024*1024,'WP7 exceeds 16 MiB'
    if cost:
        gc.collect();gc.disable()
        width,height=3840,2160;outputs[:]=[texture(width,height) for _ in range(3)]
        # Use the actual bounded snowfall/veil owner, not a full-screen stand-in
        # for every flake. Its idle resource preparation is outside timing; its
        # per-view dispatch, coverage, render and veil work are inside timing.
        depth=C.c_uint();gl.gen_tex(1,C.byref(depth));gl.active(0x84C0);gl.bind_tex(0x0DE1,depth)
        for p,v in ((0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)):gl.param(0x0DE1,p,v)
        sky_depth=np.ones((height,width),np.float32)
        gl.teximage(0x0DE1,0,0x8CAC,width,height,0,0x1902,0x1406,sky_depth.ctypes.data)
        native.shelter_sync()
        set_('RainRate',20);set_('SnowEnabled',1);set_('PrecipType',2);set_('SnowRate',5)
        set_('WindSpeed',14)
        set_('PrecipVisibilityEnabled',1);set_('PrecipVisibilityAmount',1);step(10)
        view=np.eye(4,dtype=np.float32)
        projection=np.zeros((4,4),np.float32);clip_start,clip_end=.1,256.
        projection[0,0]=1;projection[1,1]=width/height
        projection[2,2]=-(clip_end+clip_start)/(clip_end-clip_start)
        projection[2,3]=-2*clip_end*clip_start/(clip_end-clip_start);projection[3,2]=-1
        views=[view.T.copy(),view.T.copy()];views[1][3,0]=-20
        proj=projection.T.copy();identity=np.eye(4,dtype=np.float32);origin=np.zeros(3,np.float64)
        from weather_rain_gl_check import RainRunner,native_adapter,UNIFORMS
        rain_base=RainRunner(tree,gl)
        upload=native_adapter(tree,ROOT/'.tmp/weather_port/shelter/rain_upload'/(str(os.getpid())+('_'+context_key if context_key else '')))
        rain_values=upload(1,20,12.5)
        native.shelter_rain_program.restype=C.c_uint
        rain_programs={False:rain_base.program,True:native.shelter_rain_program()}
        assert rain_programs[True],'advanced rain program not prepared'
        rain_vao=C.c_uint();gl.fn('glGenVertexArrays',None,C.c_int,C.c_void_p)(1,C.byref(rain_vao))
        gl.fn('glBindVertexArray',None,C.c_uint)(rain_vao);gl.bind_buf(0x8892,rain_base.vbo)
        assert all(gl.attr_loc(p,b'position')==0 for p in rain_programs.values()),'rain position attribute contract'
        gl.enable_attr(0);gl.attr_ptr(0,3,0x1406,0,0,None)
        gl.fn('glBindVertexArray',None,C.c_uint)(0);gl.bind_buf(0x8892,0)
        inverse_projection=np.linalg.inv(projection).T.copy();inverse_views=[identity.copy(),identity.copy()]
        inverse_views[1][3,0]=20
        assert list(rain_values)==UNIFORMS,'native rain upload order'
        uniform_names=[*UNIFORMS,'depthMap','inv_proj','weather_inv_modelview']
        locations=np.array([[gl.loc(p,name.encode()) for name in uniform_names] for p in rain_programs.values()],np.int32)
        objects=np.array([fbo.value,depth.value,rain_vao.value,consumers[2],*rain_programs.values(),*[o.value for o in outputs]],np.uint32)
        values=np.array(list(rain_values.values()),np.float32)
        packed_views=np.array(views,np.float32);packed_inverse_views=np.array(inverse_views,np.float32)
        native.shelter_cost_setup.argtypes=[C.c_void_p]*7+[C.c_int]*2
        native.shelter_cost_frame.argtypes=[C.c_int]*3
        native.shelter_cost_rain.argtypes=[C.c_int]*2
        native.shelter_cost_setup(objects.ctypes.data,locations.ctypes.data,values.ctypes.data,
            proj.ctypes.data,inverse_projection.ctypes.data,packed_views.ctypes.data,packed_inverse_views.ctypes.data,width,height)
        native.shelter_sync()
        native.shelter_cost_rain(1,0)
        gl.fn('glReadBuffer',None,C.c_uint)(0x8CE0)
        rain_pixels=np.empty((height//2,width//2,4),np.float32)
        gl.read(0,0,width//2,height//2,0x1908,0x1406,rain_pixels.ctypes.data)
        assert np.isfinite(rain_pixels).all() and np.count_nonzero(rain_pixels[...,:3])>100,'production rain cost draw is missing'
        if benchmark_context:
            return dict(gl=gl,native=native,set=set_,step=step,reset=target,counts=metrics,result=result)
        records=[];tick=0
        def work(on,dual=False,sample_index=None):
            nonlocal tick
            tick=tick+1 if sample_index is None else sample_index
            # Every sample includes a moving roof and mean angle change. Native
            # update-rate coalescing determines whether maps actually refresh.
            native.shelter_cost_frame(int(on),int(dual),tick)
        def dist(a):return dict(zip(('median','p95','p99','max'),map(float,np.percentile(a,[50,95,99,100]))))
        failures=[]
        for dual in (False,True):
            # Interleave matched A/B samples and reverse order on alternate
            # pairs. Both samples see the moving roof/angular workload. This
            # prevents unrelated block-to-block CPU/GPU load from masquerading
            # as the incremental shelter cost.
            for i in range(80):
                on=bool(i%2);set_('AdvancedShelterEnabled',int(on));work(on,dual)
            gl.finish()
            queries=(C.c_uint*480)();gl.gen_queries(480,queries)
            cpu={False:[],True:[]};gpu={False:[],True:[]};order=[];refresh={False:[],True:[]}
            first_sample=tick+1
            for i in range(240):
                for on in (bool(i%2),not bool(i%2)):
                    set_('AdvancedShelterEnabled',int(on))
                    q=queries[len(order)];order.append(on)
                    before_refresh=native.shelter_traversals()
                    gl.begin_query(0x88BF,q);start=time.perf_counter();work(on,dual,first_sample+i)
                    cpu[on].append((time.perf_counter()-start)*1000);gl.end_query(0x88BF)
                    refresh[on].append(native.shelter_traversals()>before_refresh)
            gl.finish() # One benchmark drain after the interleaved batch.
            for q,on in zip(queries,order):
                n=C.c_uint64();gl.query_u64(q,0x8866,C.byref(n));gpu[on].append(n.value/1e6)
            gl.fn('glDeleteQueries',None,C.c_int,C.c_void_p)(480,queries)
            paired={on:np.asarray(cpu[on])+gpu[on] for on in (False,True)}
            for on in (False,True):
                records.append({'dual':dual,'advanced':on,'gpu_ms':dist(gpu[on]),'cpu_ms':dist(cpu[on]),'total_ms':dist(paired[on])})
                print('4K complete pipeline',json.dumps(records[-1]),flush=True)
            delta=np.maximum(0,paired[True]-paired[False])
            result['two_views_delta_ms' if dual else 'main_delta_ms']=dist(delta)
            refresh_delta=delta[np.asarray(refresh[True],bool)]
            result['two_views_refresh_ms' if dual else 'main_refresh_ms']=dist(refresh_delta) if len(refresh_delta) else dist([0.])
            # The plan's ceiling is per 4K view; the simultaneous second 4K
            # feed is reported separately. Shared culls/maps remain counted
            # once, rather than charging another owner to that second feed.
            budget=.60*(2 if dual else 1)
            result['cost']=records
            if np.percentile(delta,95)>budget:
                failures.append('whole shelter pipeline p95 exceeds '+str(budget)+' ms: '+str(dist(delta)))
            if len(refresh_delta) and np.max(refresh_delta)>budget:
                failures.append('shelter sustained refresh max exceeds '+str(budget)+' ms: '+str(dist(refresh_delta)))
            result['cost_status']='FAIL' if failures else 'IN_PROGRESS'
            (ROOT/'.tmp/weather_port/WP7_COST.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        result['cost']=records
        from weather_shelter_baseline_fixture import compare
        result['paired_vs_80abb6c19d6']=compare(gl,native,program,full_vertex,wrappers,caster,objects,values,
            proj,inverse_projection,packed_views,packed_inverse_views,width,height)
        result['cost_status']='FAIL' if failures else 'PASS'
        if failures:
            comparison=result['paired_vs_80abb6c19d6'][0]
            baseline=comparison['runs']['80abb6c19d6']['gpu_ms']
            if comparison['paired_increment_ms']['p50']<=.6 and (baseline['p95']>baseline['p50']*2 or baseline['max']>baseline['p50']*4):
                result['cost_status']='PENDING-QUIET'
        gc.enable()
        (ROOT/'.tmp/weather_port/WP7_COST.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        assert not failures,'; '.join(failures)
    print('PASS directional/masked/moving roof, near/far seam, world anchoring, bias, unknown, state, auxiliary consumers and bindings',flush=True)
    return result


def bindings(tree,native,metrics,set_,step,prepare):
    from weather_binding_check import settings_table
    table=settings_table(tree.read('indra/newview/app_settings/settings.xml'))
    rows={r.get('setting'):r for r in ET.fromstring(tree.read('indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml')).iter('kit_row')}
    fields={'AdvancedShelterEnabled':0,'ShelterNearResolution':1024,'ShelterFarResolution':512,
        'ShelterNearExtent':32,'ShelterFarExtent':128,'ShelterTiltEnabled':1,'ShelterUpdateHz':30,'ShelterBias':.12,'ShelterSoftness':.35,'ShelterDebug':0}
    panel=tree.read('indra/newview/alpanelweathersettings.cpp')
    for suffix,default in fields.items():
        key='AlchemyWeather'+suffix;row=rows[key]
        assert row.get('bind')=='setting' and row[0].get('control_name')==key,'disconnected Director '+key
        assert abs(float(table[key]['Value'])-default)<1e-5,'wrong default '+key
        assert '"'+key+'"' in panel,'missing preset/reset tracking '+key
    assert 'weather_shelter_status' in tree.read('indra/newview/llfloaterdirector.cpp'),'searchable status disconnected'
    for suffix,value,index in [('ShelterNearResolution',512,3),('ShelterFarResolution',256,4),
        ('ShelterNearExtent',24,9),('ShelterFarExtent',192,10),('ShelterUpdateHz',5,8)]:
        set_(suffix,value);step(3);prepare(3)
        assert metrics()[index]==value,'Director row has no production effect '+suffix
    for suffix in fields:
        set_(suffix,fields[suffix] if suffix!='AdvancedShelterEnabled' else 1)
    step(4);prepare(4)
    assert b'update <=' in native.shelter_status() and b'unknown' in native.shelter_status(),'update/coverage status'
    refresh_counts=[]
    for hz,start in ((5,20),(60,21)):
        set_('ShelterUpdateHz',hz);step(start);prepare(start,dirty=1,settle=False)
        before=native.shelter_traversals()
        for frame in range(1,31):prepare(start+frame/60,dirty=1,settle=False)
        refresh_counts.append(native.shelter_traversals()-before)
    assert refresh_counts[0]<=6 and refresh_counts[1]>refresh_counts[0]*3,'Update rate row has no observable bounded effect'
    set_('ShelterUpdateHz',30);step(4);prepare(4)
    # Above-cap recipes fail explicitly and never allocate or publish as valid.
    set_('ShelterNearResolution',2048);step(5);prepare(5)
    assert metrics()[2]==0 and b'16 MiB' in native.shelter_status(),'over-budget detail falsely valid'
    set_('ShelterNearResolution',1024);step(6);prepare(6)
    assert metrics()[2]==1,'detail recovery publication'


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-ref',default='WORKTREE')
    p.add_argument('--mutation',choices=list(MUTATIONS));p.add_argument('--mutations',action='store_true');p.add_argument('--no-cost',action='store_true');p.add_argument('--before-after',action='store_true');a=p.parse_args()
    if a.before_after:
        r=subprocess.run([sys.executable,__file__,'--source-ref',PRE_WP7,'--no-cost'],capture_output=True,text=True,encoding='utf-8',timeout=240)
        assert r.returncode!=0 and 'WP7 production map owner missing' in r.stderr,'pre-WP7 unexpectedly passes'
        print('PASS immutable pre-WP7 must-fail',PRE_WP7,flush=True)
    result=run(a.source_ref,a.mutation,not a.no_cost)
    if a.mutations:
        for mutation,named in MUTATIONS.items():
            r=subprocess.run([sys.executable,__file__,'--source-ref',a.source_ref,'--mutation',mutation,'--no-cost'],capture_output=True,text=True,encoding='utf-8',timeout=240)
            (ROOT/'.tmp/weather_port'/('wp7_mutation_'+mutation+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
            assert r.returncode!=0 and named in r.stderr,'mutation survived or failed wrong assertion '+mutation+'\n'+r.stdout+r.stderr
            print('mutation rejected:',mutation,'('+named+')',flush=True)
    print(json.dumps(result),flush=True)


if __name__=='__main__':main()
