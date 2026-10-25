"""Real WGL production rain readbacks and native uniform-adapter regression.

Compiles the rain upload block extracted from pipeline.cpp for WORKTREE and
330d012e161; then renders the production GLSL with those exact uploaded values.
Depth/camera inputs are fixtures. This is not a viewer build/in-world self-test.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import re

import numpy as np
from relight_3d_gl import GL, SourceTree, SHADERS
from weather_cpu_check import ROOT, SOURCES, compiler_command, prepare

BASE = '330d012e161'
UNIFORMS = ['weather_wind', 'weather_rain_color', 'weather_time', 'weather_intensity',
            'weather_density', 'weather_fall_speed', 'weather_max_distance', 'weather_lightning',
            'weather_virtual_shutter', 'weather_near_emphasis', 'weather_gust_strength',
            'weather_layers', 'weather_samples']


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def native_adapter(tree, out, mutation=None):
    prepare(out)
    pipe = function(tree.read('indra/newview/pipeline.cpp'), 'void LLPipeline::renderWeather(')
    if mutation == 'default':
        pipe = pipe.replace('const bool library = control_mode() == 1;', 'const bool library = true;')
    if mutation == 'upload':
        pipe = pipe.replace('rain_program.uniform1f(sIntensity, rain_pass_intensity);', '')
    body = pipe[pipe.index('            const F32 wind_scale ='):pipe.index('            mScreenTriangleVB->setBuffer();', pipe.index('            const F32 wind_scale ='))]
    rate = pipe[pipe.index('    const F32 base_rain_intensity ='):pipe.index('    const bool splash_active =')]
    if 'const auto library_rain' in pipe:
        rate = pipe[pipe.index('    const auto library_rain ='):pipe.index('    const bool splash_active =')]
    clock = pipe[pipe.index('    const F64 now ='):pipe.index('    LLVector3 anchor_agent')]
    mode = 'const bool library = false;'
    if 'const bool library = control_mode() == 1;' in pipe:
        mode = 'const bool library = control_mode() == 1;'
    elif mutation == 'default':
        mode = 'const bool library = true;'
    names = '\n'.join('const char* %s = "%s";' % pair for pair in re.findall(
        r'static const LLStaticHashedString (\w+)\(\s*"([^"]+)"\);', pipe))
    prefix = r'''
#include "alweatherbridge.h"
#include <map>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>
template<class T> struct Value { T v; operator T() const{return v;} T operator()() const{return v;} };
template<class T> T llclamp(T value,T low,T high){return std::clamp(value,low,high);}
struct LLColor3 { float mV[3]; };
namespace glm { template<class T> const float* value_ptr(const T& t){return t.data();} }
struct Upload {
 std::map<std::string,std::array<float,4>> values;
 void uniform1f(const char* n,float a){values[n]={a,0,0,0};}
 void uniform1i(const char* n,int a){values[n]={float(a),0,0,0};}
 void uniform3f(const char* n,float a,float b,float c){values[n]={a,b,c,0};}
 void uniformMatrix4fv(const char*,int,bool,const float*){}
};
constexpr int VX=0,VY=1;
struct Temporal { double presentation_time; };
struct LLPresentationTime { static Temporal frame; static Temporal currentFrame(){return frame;} };
Temporal LLPresentationTime::frame{};
extern "C" __declspec(dllexport) void uniforms(unsigned mode,float rate,double time,float cloud,float* output) {
 Value<unsigned> control_mode{mode};
 ALWeatherBridge::Controller library_frame;
 ALWeatherBridge::Settings settings;
 settings.mode=1;settings.enabled=settings.local=true;settings.transition=settings.bands=0;
 settings.windSource=1;settings.windSpeed=7;settings.windDirection=225;
 settings.rainRate=rate;settings.windVariation=0.3f;
 ALWeatherModel::Config lightning;
 library_frame.update(0,0,settings,2,3,1,lightning,{});
 U64 frame=1;
 do { library_frame.update(frame++,time,settings,2,3,1,lightning,{}); } while(library_frame.catchingUp());
 LLPresentationTime::frame.presentation_time=time;
 const auto finite_clamp=[](float value,float fallback,float lo,float hi){return std::clamp(std::isfinite(value)?value:fallback,lo,hi);};
 Value<bool> rain_enabled{true};
 Value<float> rain_intensity_setting{0.55f}, rain_density_setting{0.65f},wind_scale_setting{1.f};
 Value<float> rain_fall_speed_setting{28.f},rain_distance_setting{80.f},rain_shutter_setting{0.04f},rain_near_setting{0.65f},rain_gust_setting{0.35f};
 Value<unsigned> rain_layers_setting{3},rain_samples_setting{12};
 LLColor3 rain_color_setting{{0.55f,0.68f,0.78f}},gWindVec{{2.f,3.f,0.f}};
 std::array<float,16> inverse_modelview{};
 const float cloud_factor=cloud, weather_flash=0.f;
 Upload rain_program;
'''
    suffix = '\nconst char* keys[]={' + ','.join('"%s"' % n for n in UNIFORMS) + '};\n'
    suffix += 'for(unsigned n=0;n<13;++n) for(unsigned k=0;k<4;++k) output[n*4+k]=rain_program.values[keys[n]][k];\n}\n'
    source = out / 'adapter.cpp'
    source.write_text(prefix + mode + names + clock + rate + 'const float rain_pass_intensity=base_rain_intensity;\n' + body + suffix, encoding='utf-8')
    dll = compiler_command(out, [source, *(ROOT / ('indra/newview/' + n + '.cpp') for n in SOURCES)], 'adapter', dll=True)
    library = C.CDLL(str(dll))
    library.uniforms.argtypes = [C.c_uint, C.c_float, C.c_double, C.c_float, C.POINTER(C.c_float)]
    def values(mode, rate, time=12.5, cloud=0.7):
        data = np.zeros((len(UNIFORMS), 4), np.float32)
        library.uniforms(mode, rate, time, cloud, data.ctypes.data_as(C.POINTER(C.c_float)))
        return dict(zip(UNIFORMS, data))
    return values


class RainRunner:
    def __init__(self, tree, gl, occlusion=False, shader='weatherRainF.glsl'):
        self.gl = gl
        self.occlusion = occlusion
        def compile_(source, stage):
            shader = gl.create_shader(stage)
            raw = ('#version 420\n' + ('#define WEATHER_RAIN_OCCLUSION 1\n' if occlusion else '') + source).encode()
            pointer = C.c_char_p(raw)
            gl.source(shader, 1, C.byref(pointer), None); gl.compile(shader)
            ok = C.c_int(); gl.getshader(shader, 0x8B81, C.byref(ok))
            if not ok.value:
                log = C.create_string_buffer(32000); gl.slog(shader, len(log), None, log)
                raise AssertionError(log.value.decode())
            return shader
        util = tree.read(SHADERS + '/class1/deferred/deferredUtil.glsl')
        depth = 'uniform sampler2D depthMap; uniform mat4 inv_proj;\n'
        for signature in ('vec2 getScreenCoordinate(', 'float getDepth(', 'vec4 getPosition('):
            depth += function(util, signature) + '\n'
        source = tree.read(SHADERS + '/class1/deferred/' + shader)
        vertex = tree.read(SHADERS + '/class1/deferred/postDeferredNoTCV.glsl')
        self.program = gl.program()
        gl.attach(self.program, compile_(depth + source, 0x8B30))
        gl.attach(self.program, compile_(vertex, 0x8B31)); gl.link(self.program)
        ok = C.c_int(); gl.getprog(self.program, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(32000); gl.plog(self.program, len(log), None, log)
            raise AssertionError(log.value.decode())
        self.fbo = C.c_uint(); gl.gen_fbo(1, C.byref(self.fbo))
        self.vbo = C.c_uint(); gl.gen_buf(1, C.byref(self.vbo))
        gl.bind_buf(0x8892, self.vbo)
        verts = np.array([-1,-1,0,3,-1,0,-1,3,0], np.float32)
        gl.buffer_data(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
        self.textures = []

    def texture(self, unit, data, internal, fmt):
        gl = self.gl; tex = C.c_uint(); gl.gen_tex(1, C.byref(tex)); self.textures.append(tex)
        gl.active(0x84C0 + unit); gl.bind_tex(0x0DE1, tex)
        for key, value in [(0x2801,0x2600),(0x2800,0x2600),(0x2802,0x812F),(0x2803,0x812F)]: gl.param(0x0DE1,key,value)
        gl.teximage(0x0DE1,0,internal,data.shape[1],data.shape[0],0,fmt,0x1406,data.ctypes.data)
        return tex

    def render(self, values, auxiliary=False, covered=False, size=(127,73), render_size=None, low=None):
        gl = self.gl; width,height=size
        draw_width,draw_height=render_size or size
        for cap in [0x0B71,0x0BE2,0x0C11,0x8DB9,0x0B44]: gl.disable(cap)
        gl.color_mask(1,1,1,1)
        gl.use(self.program); gl.bind_fbo(0x8D40,self.fbo)
        output = self.texture(0,np.zeros((draw_height,draw_width,4),np.float32),0x881A if render_size else 0x8814,0x1908)
        gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,output,0)
        assert gl.fbo_status(0x8D40)==0x8CD5
        depth=np.ones((height,width),np.float32)
        # Left half has a near silhouette, right half an open scene. Aux has its own depth.
        depth[:,:width//2]=0.1 if auxiliary else 0.25
        self.texture(1,depth,0x822E,0x1903)
        gl.ui(gl.loc(self.program,b'depthMap'),1)
        inverse_projection=np.diag([45.,26.,-40.,1.]).astype(np.float32)
        world=np.eye(4,dtype=np.float32)
        if auxiliary: world[:3,3]=[13.,-9.,4.]
        for key,matrix in [('inv_proj',inverse_projection),('weather_inv_modelview',world)]:
            packed=np.ascontiguousarray(matrix.T)
            gl.matrix(gl.loc(self.program,key.encode()),1,0,packed.ctypes.data_as(C.POINTER(C.c_float)))
        for key,value in values.items():
            location=gl.loc(self.program,key.encode())
            if key in ('weather_layers','weather_samples'): gl.ui(location,int(value[0]))
            elif key in ('weather_wind','weather_rain_color'): gl.uf[3](location,*map(float,value[:3]))
            else: gl.uf[1](location,float(value[0]))
        if low is not None:
            self.texture(3,low,0x881A,0x1908)
            gl.ui(gl.loc(self.program,b'weather_rain_map'),3)
            gl.uf[2](gl.loc(self.program,b'weather_rain_map_res'),float(low.shape[1]),float(low.shape[0]))
        if self.occlusion:
            self.texture(2,np.full((16,16),0.0 if covered else 1.0,np.float32),0x822E,0x1903)
            gl.ui(gl.loc(self.program,b'weather_rain_occlusion_map'),2)
            gl.ui(gl.loc(self.program,b'weather_rain_occlusion_enabled'),1)
            packed=np.ascontiguousarray((np.eye(4,dtype=np.float32)/128).T);packed[3,3]=1
            gl.matrix(gl.loc(self.program,b'weather_rain_occlusion_matrix'),1,0,packed.ctypes.data_as(C.POINTER(C.c_float)))
            for key,val in [('depth_range',256),('bias',0.12),('softness',0.35)]:
                gl.uf[1](gl.loc(self.program,('weather_rain_occlusion_'+key).encode()),val)
        index=gl.attr_loc(self.program,b'position');gl.bind_buf(0x8892,self.vbo)
        gl.enable_attr(index);gl.attr_ptr(index,3,0x1406,0,0,None)
        gl.viewport(0,0,draw_width,draw_height);gl.draw(4,0,3)
        pixels=np.empty((draw_height,draw_width,4),np.float32);gl.read(0,0,draw_width,draw_height,0x1908,0x1406,pixels.ctypes.data)
        assert np.all(np.isfinite(pixels)), 'non-finite rain'
        return pixels


def run(mutation=None):
    current, baseline=SourceTree('WORKTREE'),SourceTree(BASE)
    pipe=current.read('indra/newview/pipeline.cpp')
    prism=current.read('indra/newview/llprismlens.cpp')
    if mutation=='aux': prism=prism.replace('gPipeline.renderWeather(&rt.screen, /*auxiliary=*/true);','')
    assert 'gPipeline.renderWeather(&rt.screen, /*auxiliary=*/true);' in prism, 'missing auxiliary weather consumer'
    assert '!auxiliary && divisor > 1U' in pipe, 'aux shares main rain scratch'
    assert '!auxiliary && rain_occlusion_setting()' in pipe, 'aux samples main cover map'
    assert 'library ? library_frame.lightning() :' in pipe, 'library draw advances scheduler'
    for name in ['weatherRainF.glsl','weatherRainUpsampleF.glsl','weatherSurfaceF.glsl','weatherLightningF.glsl']:
        path=SHADERS+'/class1/deferred/'+name
        assert current.read(path)==baseline.read(path), 'default shader changed: '+name
    for tree in (current,baseline):
        mgr=tree.read('indra/newview/llviewershadermgr.cpp')
        section=mgr[mgr.index('gDeferredWeatherRainProgram.mName ='):mgr.index('gDeferredWeatherRainUpsampleProgram.mName =')]
        if tree is current: current_permutations=section
        else: assert section==current_permutations, 'rain permutations changed'
    out=ROOT/'.tmp/weather_port/gl'/ (mutation or 'baseline')
    old=native_adapter(baseline,out/'old')
    new=native_adapter(current,out/'new',mutation)
    gl=GL()
    getstr=gl.fn('glGetString',C.c_char_p,C.c_uint)
    device={key:getstr(enum).decode() for key,enum in [('vendor',0x1F00),('renderer',0x1F01),('version',0x1F02)]}
    print('WGL',device,flush=True)
    oldrunner,newrunner=RainRunner(baseline,gl),RainRunner(current,gl)
    oldresolve,newresolve=(RainRunner(tree,gl,shader='weatherRainUpsampleF.glsl') for tree in (baseline,current))
    identities=0
    for time in (0.,1.25,12.5,3599.75):
        for cloud in (0.,0.7,1.):
            a,b=old(0,5,time,cloud),new(0,5,time,cloud)
            assert all(np.array_equal(a[key],b[key]) for key in a), 'mode-0 native uploads changed'
            for divisor in (1,2,3,4):
                if divisor==1:
                    before,after=oldrunner.render(a),newrunner.render(b)
                else:
                    resolution=(127//divisor,73//divisor)
                    before=oldresolve.render(a,low=oldrunner.render(a,render_size=resolution))
                    after=newresolve.render(b,low=newrunner.render(b,render_size=resolution))
                assert np.array_equal(before,after), 'mode-0 GL bytes changed at divisor '+str(divisor)
                identities+=1
    light,heavy,clear=new(1,1),new(1,80),new(1,0)
    for aux in (False,True):
        a,b,z=newrunner.render(light,aux),newrunner.render(heavy,aux),newrunner.render(clear,aux)
        assert np.count_nonzero(b[:,:,:3])>100, 'library rain missing in '+('auxiliary' if aux else 'main')
        assert np.sum(b)>np.sum(a)>np.sum(z)==0, 'physical rain has no observable monotonic GL response'
        assert np.array_equal(b,newrunner.render(heavy,aux)), 'view ordering changes frozen rain'
    assert not np.array_equal(newrunner.render(heavy),newrunner.render(heavy,True)), 'aux camera/depth ignored'
    cover=RainRunner(current,gl,True)
    exposed,covered=cover.render(heavy),cover.render(heavy,covered=True)
    assert exposed.sum()>covered.sum(), 'production cover permutation has no effect'
    result=dict(base=BASE,mode0_byte_identical_fixtures=identities,library_main=True,library_auxiliary=True,device=device,
                main_sha256=hashlib.sha256(newrunner.render(heavy).tobytes()).hexdigest(),
                limitation='Standalone production GLSL/upload blocks; native scene/state integration awaits final viewer build and user in-world test.')
    out.mkdir(parents=True,exist_ok=True);(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print('weather rain GL: PASS',json.dumps(result),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--mutation',choices=['default','upload','aux']);args=parser.parse_args();run(args.mutation)
