"""WP-A real-WGL sensor gate. Run with PYTHONUTF8=1; no viewer build.

Compiles the PRODUCTION sensorUtilF.glsl and pure alsensormodel.cpp, not CPU
copies. The hidden context/float FBO follow relight_3d_gl.GL. Processing tests
link the complete production LGG, tone-map, sRGB and LPM modules. Fixtures are
only flat/gradient light and uniforms. Every mutation is compiled in memory
and must fail its named *numerical* predicate; compilation failures do not count.

Default watchdog: 1800 s, including shader compilation. Statistical gates use
six standard errors with N_eff=N/8 for spatial Gaussian fields, and Poisson
GOF alpha=1e-5 (pooled expected bins >=20). All measured values and tolerances
are saved. Cost gates the upper p95 CI at <=0.25 ms, with an explicit
PENDING-QUIET rule requiring paired baseline evidence of GPU sharing.
"""
import os
os.environ['PYTHONUTF8'] = '1'
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import argparse
import ctypes as C
import hashlib
import json
import math
import re
from pathlib import Path
import subprocess
import sys
import time
import traceback

import numpy as np

sys.dont_write_bytecode = True
import relight_3d_gl as G

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/iso_sensor'
SHADER = ROOT / G.SHADERS / 'class1/alchemy/sensorUtilF.glsl'
LUMA = np.array([.2126, .7152, .0722])
FRAMES = 8
SIZE = 256
CLOCK = 144
BASIS = 40
SUITES = ('poisson','quantisation','ptc','brightness','gain_switch','clip','colour',
          'determinism','black','adversarial','temporal','green_sites','green_peak',
          'poisson_tail','hue_ramp','lattice_phase','layout','model_sensitivity',
          'report_contract','coverage','hires','mutations','cost','scope')


def progress(message):
    print('[SensorISO] ' + message, flush=True)


def gamma_q(a, x):
    """Regularized upper incomplete gamma, for chi-square survival probability."""
    if x <= 0:
        return 1.
    factor = math.exp(-x + a*math.log(x) - math.lgamma(a))
    if x < a + 1:
        term = total = 1/a
        for k in range(1, 1000):
            term *= x/(a+k)
            total += term
            if abs(term) < abs(total)*1e-14:
                return max(0., 1-total*factor)
    else:
        b, c = x+1-a, 1e300
        d = 1/b
        h = d
        for k in range(1, 1000):
            an = -k*(k-a)
            b += 2
            d = an*d+b
            if abs(d) < 1e-300:
                d = 1e-300
            c = b+an/c
            if abs(c) < 1e-300:
                c = 1e-300
            d = 1/d
            delta = d*c
            h *= delta
            if abs(delta-1) < 1e-14:
                return max(0., min(1., factor*h))
    raise ArithmeticError('chi-square survival did not converge')


def correlation(a,b):
    a,b = np.asarray(a).ravel(),np.asarray(b).ravel()
    av,bv = a-a.mean(),b-b.mean()
    return float(np.dot(av,bv)/math.sqrt(np.dot(av,av)*np.dot(bv,bv)))


def matrix(packed,start):
    return packed.view(np.float32)[start:start+12].reshape(3,4)[:,:3].T.astype(float)


def camera_basis(packed,variance):
    m = matrix(packed,12)
    c = np.linalg.cholesky(m@np.diag(variance)@m.T)
    axis = c.T@LUMA; axis /= np.linalg.norm(axis)
    helper = [0,0,1] if abs(axis[2])<.9 else [1,0,0]
    tangent = np.cross(axis,helper); tangent /= np.linalg.norm(tangent)
    return np.linalg.inv(m)@c@np.stack((axis,tangent,np.cross(axis,tangent)),axis=1)


def gate_status(checks,groups,names):
    states = [groups.get(name,'NOT RUN') for name in names]
    rows = [row for row in checks if row.get('group') in names]
    if any(not row['passed'] for row in rows) or 'FAIL' in states:
        return 'FAIL'
    if 'NOT RUN' in states:
        return 'NOT RUN'
    if 'SKIP' in states:
        return 'SKIP'
    if 'PENDING-QUIET' in states:
        return 'PENDING-QUIET'
    return 'PASS' if rows else 'NOT RUN'


def provenance(sensor):
    paths = dict(shader=SHADER,cpp=ROOT/'indra/newview/alsensormodel.cpp',
                 header=ROOT/'indra/newview/alsensormodel.h',tester=Path(__file__).resolve(),
                 tut=ROOT/'indra/newview/tests/alsensormodel_test.cpp')
    return dict(commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True,timeout=10).strip(),
                shader_compiled_sha256=hashlib.sha256(sensor.encode()).hexdigest(),
                sha256={key:hashlib.sha256(path.read_bytes()).hexdigest() for key,path in paths.items()})


class Model:
    """Small DLL probe of the actual pure C++ model; never builds the viewer."""
    def __init__(self, out, model_source=None):
        out.mkdir(parents=True, exist_ok=True)
        # linden_common is only the project's common include, not a dependency
        # of this pure model. An empty include isolates it from viewer libraries.
        (out/'linden_common.h').write_text('// Pure-model standalone fixture.\n', encoding='utf-8')
        source = out/'probe.cpp'
        source.write_text('''#include "alsensormodel.h"
#include <cstring>
extern "C" __declspec(dllexport) void sensor_pack(int index, double iso, int bright,
    unsigned long long frame, unsigned seed, unsigned stream, float amount, float chroma,
    void* packed, double* values) {
    using namespace ALSensorModel;
    const auto& preset = illustrativePresets().at(index);
    auto p = derive(preset, iso, bright ? BrightnessMode::Brightens : BrightnessMode::Neutral);
    auto u = pack(preset,p,frame,seed,stream,amount,chroma);
    std::memcpy(packed,&u,sizeof(u));
    const double v[] = {preset.baseISO,preset.switchISO,preset.headroom,p.gain,p.neutralGain,
        p.analogGain,p.saturation,p.electronsPerLinear,p.headroomStops,p.noiseFloorLinear,
        p.dynamicRangeStops,p.mode == GainMode::HCG ? 1. : 0.};
    std::memcpy(values,v,sizeof(v));
}
''', encoding='utf-8')
        installs = Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio'
        candidates = sorted(installs.glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))
        if not candidates:
            raise RuntimeError('MSVC required for actual pure-model probe')
        dll = out/'sensor_model.dll'
        command = out/'build.cmd'
        command.write_text('@echo off\ncall "' + str(candidates[-1]) + '" >nul 2>&1\n' +
            'cl /nologo /std:c++17 /EHsc /O2 /LD /I"' + str(out) + '" /I"' +
            str(ROOT/'indra/newview') + '" "' + str(model_source or ROOT/'indra/newview/alsensormodel.cpp') +
            '" "' + str(source) + '" /Fe:"' + str(dll) + '" /Fo"' + out.as_posix() +
            '/" /link /IMPLIB:"' + str(out/'sensor_model.lib') + '"\nexit /b %ERRORLEVEL%\n',
            encoding='utf-8')
        run = subprocess.run(['cmd','/d','/c',str(command)], capture_output=True, text=True, timeout=90)
        (out/'build.log').write_text(run.stdout+run.stderr, encoding='utf-8')
        if run.returncode:
            raise AssertionError('pure C++ model compile: '+run.stdout+run.stderr)
        self.dll = C.CDLL(str(dll))
        self.fn = self.dll.sensor_pack
        self.fn.argtypes = [C.c_int,C.c_double,C.c_int,C.c_uint64,C.c_uint,C.c_uint,
                            C.c_float,C.c_float,C.c_void_p,C.c_void_p]
        self.fn.restype = None

    def get(self, preset=0, iso=100, bright=0, frame=0, seed=1234, stream=7, amount=1., chroma=1.):
        packed = np.zeros(208, np.uint8)
        v = np.zeros(12, np.float64)
        self.fn(preset,iso,bright,frame,seed,stream,amount,chroma,packed.ctypes.data,v.ctypes.data)
        return packed, dict(zip(('base','switch','H','g','gN','a','sat','K','headroom','floor','dr','hcg'),v.tolist()))


class Runner:
    def __init__(self, baseline_ref=None):
        self.gl = gl = G.GL()
        self.sensor = SHADER.read_text(encoding='utf-8')
        self.baseline_ref = baseline_ref
        if baseline_ref:
            self.sensor = subprocess.check_output(['git','show',baseline_ref+':'+SHADER.relative_to(ROOT).as_posix()],
                cwd=ROOT,text=True,encoding='utf-8',timeout=10)
        self.programs = {}
        self.targets = {}
        self.matrix3 = gl.fn('glUniformMatrix3fv',None,C.c_int,C.c_int,C.c_ubyte,C.POINTER(C.c_float))
        self.bind_base = gl.fn('glBindBufferBase',None,C.c_uint,C.c_uint,C.c_uint)
        self.error = gl.fn('glGetError',C.c_uint)
        self.delete_program = gl.fn('glDeleteProgram',None,C.c_uint)
        self.delete_shader = gl.fn('glDeleteShader',None,C.c_uint)
        self.delete_query = gl.fn('glDeleteQueries',None,C.c_int,C.POINTER(C.c_uint))
        for cap in (0x0B71,0x0BE2,0x0C11,0x8DB9,0x0B44,0x0BD0):
            gl.disable(cap)
        self.vbo, self.ubo, self.fbo = C.c_uint(), C.c_uint(), C.c_uint()
        gl.gen_buf(1,C.byref(self.vbo))
        gl.bind_buf(0x8892,self.vbo)
        # A literal full-screen quad, matching the cost requirement.
        verts = np.array([-1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1], np.float32)
        gl.buffer_data(0x8892,verts.nbytes,verts.ctypes.data,0x88E4)
        gl.gen_buf(1,C.byref(self.ubo))
        self.bind_base(0x8A11,0,self.ubo)
        gl.gen_fbo(1,C.byref(self.fbo))
        self.vertex = self.compile('''#version 420
in vec2 position;
out vec2 vary_fragcoord;
void main() { gl_Position=vec4(position,0,1); vary_fragcoord=position*.5+.5; }
''',0x8B31)

    def compile(self, source, kind=0x8B30):
        gl = self.gl
        sh = gl.create_shader(kind)
        raw = C.c_char_p(source.encode())
        gl.source(sh,1,C.byref(raw),None)
        gl.compile(sh)
        ok = C.c_int()
        gl.getshader(sh,0x8B81,C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.slog(sh,len(log),None,log)
            raise AssertionError('COMPILE '+log.value.decode())
        return sh

    def program(self, mutation=None, cost=False, baseline=False):
        key = (mutation,cost,baseline)
        if key in self.programs:
            return self.programs[key]
        source = self.sensor
        if self.baseline_ref:
            # Expose original intermediates without changing their arithmetic.
            # Remove only the dead UBO member to use the current CPU fixture.
            source = re.sub(r'    mat3 readCholesky;[^\n]*\n','',source)
            source = source.replace('struct SensorReadout\n{','struct SensorReadout\n{\n    vec2 greenPreClipElectrons;\n    vec2 greenCameraElectrons;')
            source = source.replace('    vec2 greenDN =',
                '    result.greenPreClipElectrons = greenPhotons+greenRead;\n    vec2 greenDN =')
            source = source.replace('    return result;\n}\n\nvec3 sensorHueClip',
                '    result.greenCameraElectrons = (greenDN-p.physics.w)/p.physics.z;\n    return result;\n}\n\nvec3 sensorHueClip')
            start = source.index('vec4 sensorPoisson4(')
            end = source.index('\nfloat sensorPoisson(',start)
            quantile = source[start:end].replace('sensorPoisson4(vec4 lambda, uvec4 key)','sensorPoissonQuantile(vec4 lambda, vec4 u)')
            quantile = re.sub(r'    vec4 u = vec4\(sensorUniform\(key.x\).*?;\n','',quantile)
            source += '\n'+quantile
        if mutation and not mutation.startswith('processed_'):
            replacements = MUTATIONS[mutation]
            for old,new in (replacements if isinstance(replacements,list) else [replacements]):
                assert old in source, 'mutation target absent: '+mutation
                source = source.replace(old,new,1)
        fragment = '#version 420\n' + source
        fragment += '\nlayout(std140,binding=0) uniform SensorBlock { SensorParameters sensor; };\n'
        fragment += 'uniform vec3 testLight; uniform vec2 testGradient; uniform int testKind; uniform sampler2D oracleImage;\nout vec4 result;\n'
        if cost:
            expression = 'light' if baseline else 'applySensor(light,ivec2(gl_FragCoord.xy),sensor)'
            fragment += 'void main() { vec3 light=testLight*(testGradient.x+testGradient.y*gl_FragCoord.x/3840.0); result=vec4('+expression+',1); }'
        else:
            for path in ('environment/srgbF.glsl','alchemy/colorGradeUtilF.glsl',
                         'deferred/LPMUtil.glsl','deferred/tonemapUtilF.glsl'):
                fragment += '\n'+(ROOT/G.SHADERS/'class1'/path).read_text(encoding='utf-8')
            fragment += '''
void main() {
    ivec2 pixel=ivec2(gl_FragCoord.xy);
    vec3 light=testLight*(testGradient.x+testGradient.y*gl_FragCoord.x);
    vec3 value;
    if (testKind==2) {
        value=vec3(sensorShot(light.x,sensorKey(pixel,sensor,0x101u)));
    } else if (testKind==5) {
        value=vec3(sensorChroma(pixel,sensor,0xbeefu),0);
    } else if (testKind==6) {
        float companion;
        value=sensor.cameraToWorking*sensorReadGaussian(pixel,sensor,companion);
    } else if (testKind==8 || testKind==9) {
        SensorReadout readout=sensorReadCamera(light,pixel,sensor);
        value=testKind==8 ? vec3(readout.greenPreClipElectrons,readout.preClipElectrons.y) :
                           vec3(readout.greenCameraElectrons,readout.cameraElectrons.y);
    } else if (testKind==12) {
        float uniformValue=1.0-(float(pixel.y)+0.5)/8388608.0;
        value=vec3(sensorPoissonQuantile(vec4(light.x),vec4(uniformValue)).x);
    } else if (testKind==13) {
        float saturation=min(sensor.physics.x,(sensor.scale.x-sensor.physics.w)/sensor.physics.z);
        value=sensorHueClip(testLight/saturation,light,sensor);
    } else if (testKind==1 || testKind==4) {
        SensorReadout readout=sensorReadCamera(light,pixel,sensor);
        value=testKind==1 ? readout.cameraElectrons : readout.preClipElectrons;
    } else {
        value=(testKind==10 || testKind==11) ? texelFetch(oracleImage,pixel,0).rgb : applySensor(light,pixel,sensor);
        if (testKind==3 || testKind==7 || testKind==10 || testKind==11) {
            // EXACT colorCorrectF type-8 grade bypass, with actual functions.
            if (tonemap_type!=8) value=applyLiftGammaGain(value);
            value=applyToneMap(value);
            if (testKind==7 || testKind==11) value=linear_to_srgb(value);
            // Processing mutation target is outside the independent oracle path.
            if (testKind==3 || testKind==7) value=value;
        }
    }
    result=vec4(value,1);
}
'''
        if mutation and mutation.startswith('processed_'):
            old,new=MUTATIONS[mutation]
            assert old in fragment
            fragment=fragment.replace(old,new,1)
        gl = self.gl
        pr = gl.program()
        gl.attach(pr,self.vertex)
        sh = self.compile(fragment)
        gl.attach(pr,sh)
        gl.link(pr)
        ok = C.c_int()
        gl.getprog(pr,0x8B82,C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000)
            gl.plog(pr,len(log),None,log)
            raise AssertionError('LINK '+log.value.decode())
        self.delete_shader(sh)
        self.programs[key] = pr
        return pr

    def target(self,w,h,half=False):
        gl = self.gl
        key = (w,h,half)
        if key not in self.targets:
            tex = C.c_uint()
            gl.gen_tex(1,C.byref(tex))
            gl.bind_tex(0x0DE1,tex)
            for p in (0x2801,0x2800):
                gl.param(0x0DE1,p,0x2600)
            gl.teximage(0x0DE1,0,0x881A if half else 0x8814,w,h,0,0x1908,0x1406,None)
            self.targets[key] = tex
        gl.bind_fbo(0x8D40,self.fbo)
        gl.fbo_tex(0x8D40,0x8CE0,0x0DE1,self.targets[key],0)
        assert gl.fbo_status(0x8D40)==0x8CD5, 'float framebuffer'
        gl.viewport(0,0,w,h)

    def uniform(self,pr,name,*v):
        loc = self.gl.loc(pr,name.encode())
        if loc>=0:
            self.gl.uf[len(v)](loc,*v)

    def prepare(self,packed,light,kind=0,gradient=(1.,0.),w=SIZE,h=SIZE,mutation=None,
                processing=None,cost=False,baseline=False):
        gl = self.gl
        pr = self.program(mutation,cost,baseline)
        self.target(w,h,cost)
        gl.use(pr)
        gl.bind_buf(0x8A11,self.ubo)
        gl.buffer_data(0x8A11,packed.nbytes,packed.ctypes.data,0x88E8)
        self.bind_base(0x8A11,0,self.ubo)
        if np.isscalar(light):
            light = [light]*3
        self.uniform(pr,'testLight',*light)
        self.uniform(pr,'testGradient',*gradient)
        gl.ui(gl.loc(pr,b'testKind'),kind)
        if processing:
            self.uniform(pr,'uLift',0.,0.,0.)
            self.uniform(pr,'uGain',1.,1.,1.)
            self.uniform(pr,'uInvGammaCC',*([processing.get('gamma',1.)]*3))
            self.uniform(pr,'tonemap_grade',1.,1.)
            self.uniform(pr,'tonemap_khronos_params',.15,.76)
            self.uniform(pr,'tonemap_mix',1.)
            self.uniform(pr,'tonemap_params',*processing.get('params',(1.,0.,0.,0.)))
            gl.ui(gl.loc(pr,b'tonemap_type'),processing['type'])
        index = gl.attr_loc(pr,b'position')
        gl.bind_buf(0x8892,self.vbo)
        gl.enable_attr(index)
        gl.attr_ptr(index,2,0x1406,0,0,None)

    def render(self,packed,light,**kw):
        self.prepare(packed,light,**kw)
        self.gl.draw(0x0004,0,6)
        w,h = kw.get('w',SIZE),kw.get('h',SIZE)
        result = np.empty((h,w,4),np.float32)
        self.gl.read(0,0,w,h,0x1908,0x1406,result.ctypes.data)
        assert self.error()==0, 'OpenGL error'
        assert np.isfinite(result).all(), 'nonfinite readback'
        return result[:,:,:3].astype(np.float64)

    def process_pixels(self,packed,pixels,settings,code=False):
        pixels = np.ascontiguousarray(pixels,dtype=np.float32)
        height,width,_ = pixels.shape
        tex = C.c_uint()
        self.gl.gen_tex(1,C.byref(tex))
        self.gl.bind_tex(0x0DE1,tex)
        for param in (0x2801,0x2800):
            self.gl.param(0x0DE1,param,0x2600)
        self.gl.teximage(0x0DE1,0,0x8814,width,height,0,0x1907,0x1406,pixels.ctypes.data)
        self.prepare(packed,0.,kind=11 if code else 10,w=width,h=height,processing=settings)
        self.gl.bind_tex(0x0DE1,tex)
        self.gl.draw(0x0004,0,6)
        result = np.empty((height,width,4),np.float32)
        self.gl.read(0,0,width,height,0x1908,0x1406,result.ctypes.data)
        self.gl.fn('glDeleteTextures',None,C.c_int,C.POINTER(C.c_uint))(1,C.byref(tex))
        assert self.error()==0 and np.isfinite(result).all()
        return result[:,:,:3].astype(np.float64)

    def samples(self,packed,light,frames=FRAMES,**kw):
        values = []
        for frame in range(frames):
            u = packed.copy()
            u[CLOCK:CLOCK+4].view(np.uint32)[0] = frame
            values.append(self.render(u,light,**kw))
        return np.asarray(values)


MUTATIONS = {
    'output_light': (
        'vec3 shotVariance = vec3(mu.x >= 20.0 ? mu.x : 0.0,\n                            mu.y >= 20.0 ? mu.y*0.5 : 0.0,\n                            mu.z >= 20.0 ? mu.z : 0.0);',
        'vec3 shotVariance = mu * p.controls.z * vec3(1,.5,1);'),
    'gaussian_low': ('vec4 u = vec4(sensorUniform(key.x),sensorUniform(key.y),sensorUniform(key.z),sensorUniform(key.w));',
                     'return lambda + sqrt(lambda)*vec4(sensorNormalPair(key.x).x,sensorNormalPair(key.y).x,sensorNormalPair(key.z).x,sensorNormalPair(key.w).x);\n    vec4 u = vec4(sensorUniform(key.x),sensorUniform(key.y),sensorUniform(key.z),sensorUniform(key.w));'),
    'gain_reset': ('vec3 linear = p.cameraToWorking * result.cameraElectrons * (p.controls.z / p.scale.y);',
                   'float badGain = p.physics.y < 2.0 ? p.controls.z*0.25 : p.controls.z;\n    vec3 linear = p.cameraToWorking * result.cameraElectrons * (badGain / p.scale.y);'),
    'clamp_black': ('result.cameraElectrons.xz = (dn - p.physics.w) / p.physics.z;',
                    'result.cameraElectrons.xz = max(dn - p.physics.w,0.0) / p.physics.z;'),
    'rgb_confetti': [('return c * (axis*luma + u*chroma.x + v*chroma.y);',
                     'return vec3(length(c[0]),length(c[1]),length(c[2])) * vec3(luma,sensorNormalPair(sensorKey(pixel,p,domain^17u)));'),
                    ('return p.readCameraBasis * vec3(fullResolution.x,chroma);',
                     'return p.physics.y*vec3(fullResolution.x,companion*0.707106781186548,sensorNormalPair(sensorKey(pixel,p,0xabcdu)).x);')],
    'clip_capture': ('result.preClipElectrons = photons + readResidual;',
                     'result.preClipElectrons = min(photons+readResidual,vec3(min(p.physics.x,(p.scale.x-p.physics.w)/p.physics.z)));'),
    'frame_only': ('h = sensorHash(h ^ p.clock.z);\n    return sensorHash(h ^ p.clock.w);',
                   'h = sensorHash(h ^ 0u);\n    return sensorHash(h ^ 0u);'),
    'unnormalised_lattice': ('return value / sqrt(dot(w, w));','return value;'),
    'linear_key': ('''uint spatial = sensorHash(uint(pixel.x) ^ 0x85ebca6bu);
    spatial = sensorHash(spatial ^ uint(pixel.y));
    return sensorHash(spatial ^ sensorHash(sensorContext(p) ^ domain));''','''uint h = p.clock.x ^ (p.clock.y*0xc2b2ae35u);
    h ^= p.clock.z*0x27d4eb2du;
    h ^= p.clock.w*0x165667b1u;
    return sensorHash(h ^ (uint(pixel.x)*0x9e3779b9u) ^ (uint(pixel.y)*0x85ebca6bu) ^ domain);'''),
    'green_difference': [('''companion = fullResolution.y * sqrt(greenLuma*greenLuma +
        p.controls.y*p.controls.y*(greenChromaU*greenChromaU+greenChromaV*greenChromaV));''',
        'companion = fullResolution.y * length(greenRow);'),
        ('companion = fullResolution.y * p.controls.w;',
         'companion = fullResolution.y * p.physics.y * 0.707106781186548;')],
    'average_green_peak': ('result.preClipElectrons.y = max(result.greenPreClipElectrons.x,result.greenPreClipElectrons.y);',
                           'result.preClipElectrons.y = (result.greenPreClipElectrons.x+result.greenPreClipElectrons.y)*0.5;'),
    'stalled_cdf': ('vec4 value = sensorPoissonCDF(lambda,min(u,vec4(0.9999)));',
                    'return sensorPoissonCDF(lambda,u);\n    vec4 value = sensorPoissonCDF(lambda,min(u,vec4(0.9999)));'),
    'peak_hue': ('float t = (coverage.x+coverage.y+coverage.z)/3.0;',
                 'float t = max(coverage.x,max(coverage.y,coverage.z));'),
    'fixed_lattice': ('vec2 position = vec2(pixel) * 0.5 + origin;','vec2 position = vec2(pixel) * 0.5;'),
    'processed_1234': ('if (testKind==3 || testKind==7) value=value;',
                       'if (testKind==3 || testKind==7) value=vec3(1234);'),
    'processed_code_zero': ('if (testKind==3 || testKind==7) value=value;',
                            'if (testKind==7) value=vec3(0);'),
}


class Suite:
    def __init__(self,runner,model):
        self.r,self.m = runner,model
        self.tables = {}
        self.checks = []
        self.failures = []
        self.groups = {}
        self.current_group = None

    def check(self,name,passed,detail):
        self.checks.append(dict(name=name,passed=bool(passed),detail=detail,group=self.current_group))
        if not passed:
            self.failures.append(name)

    def ptc(self):
        rows, fits = [], []
        for preset,base in enumerate((100,800,200)):
            isos = sorted(set([base*2**k for k in range(10) if base*2**k<=25600]+[25600]))
            for bright in (0,1):
                for iso in isos:
                    u,p = self.m.get(preset,iso,bright)
                    f = u.view(np.float32)
                    sr = float(f[1])
                    q = 1/(12*p['a']**2)
                    mus = sorted(set([0.,2.,12.,min(40.,p['sat']*.15),p['sat']*.08,p['sat']*.25]))
                    measured = []
                    for mu in mus:
                        values = self.r.samples(u,mu*p['gN']/p['K'],kind=1)[:,:,:,0].ravel()
                        mean,var = float(values.mean()),float(values.var(ddof=1))
                        expected = mu+sr**2+q
                        neff = values.size/8
                        mtol = 6*math.sqrt(expected/neff)+.003
                        vtol = 6*math.sqrt(2/neff)*expected
                        self.check('PTC mean',abs(mean-mu)<mtol,dict(preset=preset,iso=iso,bright=bright,mu=mu,mean=mean,tolerance=mtol))
                        self.check('PTC variance',abs(var-expected)<vtol,dict(preset=preset,iso=iso,bright=bright,mu=mu,variance=var,oracle=expected,tolerance=vtol))
                        rows.append(dict(preset=preset,mode='Brightens' if bright else 'Neutral',gain='HCG' if p['hcg'] else 'LCG',iso=iso,
                                         mu=mu,mean=mean,variance=var,oracle=expected,relative_error=var/expected-1,confidence_6se=vtol))
                        measured.append((mu,var))
                    slope,intercept = np.polyfit(np.array(measured)[:,0],np.array(measured)[:,1],1)
                    self.check('PTC slope',abs(slope-1)<.05,dict(preset=preset,iso=iso,bright=bright,slope=slope,intercept=intercept,read_plus_adc=sr**2+q))
                    fits.append(dict(preset=preset,mode='Brightens' if bright else 'Neutral',iso=iso,
                                     gain='HCG' if p['hcg'] else 'LCG',slope=float(slope),intercept=float(intercept),
                                     read_plus_adc=sr**2+q,black_variance=measured[0][1]))
            progress('PTC preset %d complete (both modes, base..25600)'%preset)
        self.tables['ptc'] = rows
        self.tables['ptc_fit'] = fits

    def poisson(self,mutation=None):
        rows,hist = [],[]
        u,_ = self.m.get()
        passed = True
        for lam in (.5,2.,5.,12.,19.9):
            values = self.r.samples(u,lam,kind=2,frames=4,w=512,h=512,mutation=mutation)[:,:,:,0].ravel()
            integer = bool(np.all(values==np.floor(values))) and bool(np.all(values>=0))
            counts = np.bincount(np.clip(np.rint(values),0,1000).astype(int),minlength=80)[:80]
            mass = math.exp(-lam)
            probabilities = []
            for k in range(80):
                probabilities.append(mass)
                mass *= lam/(k+1)
            expected = np.asarray(probabilities)*values.size
            pooled,observed,acc_e,acc_o = [],[],0.,0.
            for k,(e,o) in enumerate(zip(expected,counts)):
                hist.append(dict(lam=lam,k=k,observed=int(o),expected=float(e)))
                acc_e += e
                acc_o += o
                if acc_e>=20:
                    pooled.append(acc_e); observed.append(acc_o); acc_e=acc_o=0.
            if acc_e:
                pooled[-1]+=acc_e; observed[-1]+=acc_o
            chi = float(np.sum((np.asarray(observed)-pooled)**2/pooled))
            df = len(pooled)-1
            prob = gamma_q(df/2,chi/2)
            mean,var = float(values.mean()),float(values.var(ddof=1))
            okay = integer and prob>1e-5 and abs(mean-lam)<6*math.sqrt(lam/values.size) and abs(var-lam)<6*math.sqrt((lam+2*lam**2)/values.size)
            passed &= okay
            rows.append(dict(lam=lam,n=values.size,mean=mean,variance=var,chi2=chi,df=df,p_value=prob,integer=integer,passed=okay))
        if mutation:
            return passed,rows
        self.check('Poisson GOF',passed,rows)
        self.tables['poisson'],self.tables['histograms'] = rows,hist
        boundary = []
        for lam in (19.99,20.,20.01):
            v = self.r.samples(u,lam,kind=2,frames=4,w=512,h=512)[:,:,:,0].ravel()
            boundary.append(dict(lam=lam,mean=float(v.mean()),variance=float(v.var(ddof=1))))
        self.check('lambda=20 continuity',all(abs(x['mean']-x['lam'])<.04 and abs(x['variance']/x['lam']-1)<.015 for x in boundary),boundary)
        self.tables['boundary'] = boundary
        adc_boundary = []
        u,p = self.m.get()
        for lam in (19.99,20.,20.01):
            v = self.r.samples(u,lam*p['gN']/p['K'],kind=1)[:,:,:,0].ravel()
            oracle = lam+9+1/(12*p['a']**2)
            adc_boundary.append(dict(lam=lam,mean=float(v.mean()),variance=float(v.var(ddof=1)),variance_oracle=oracle))
        self.check('full sensor lambda=20 continuity',all(abs(x['mean']-x['lam'])<.15 and abs(x['variance']/x['variance_oracle']-1)<.035 for x in adc_boundary),adc_boundary)
        self.tables['adc_boundary'] = adc_boundary

    def quantisation(self):
        rows = []
        for analog in (1.,2.,4.):
            u,_ = self.m.get()
            f = u.view(np.float32)
            f[0:12] = [50000,1,analog,512,16383,1,1,.8,1,1,1,math.sqrt(.5)]
            m = np.eye(3)
            for start,matrix in ((12,m),(24,m)):
                for c in range(3):
                    f[start+c*4:start+c*4+3]=matrix[:,c]
            chol = np.diag([1,math.sqrt(.5),1])
            axis = chol.T@LUMA; axis /= np.linalg.norm(axis)
            tangent = np.cross(axis,[0,0,1]); tangent /= np.linalg.norm(tangent)
            basis = chol@np.stack((axis,tangent,np.cross(axis,tangent)),axis=1)
            for c in range(3):
                f[BASIS+c*4:BASIS+c*4+3]=basis[:,c]
            rounded = self.r.samples(u,0.,kind=1,frames=8,w=512,h=512)[:,:,:,0].ravel()
            raw = self.r.samples(u,0.,kind=6,frames=8,w=512,h=512)[:,:,:,0].ravel()
            delta = rounded**2-raw**2
            estimate = float(delta.mean()-rounded.mean()**2+raw.mean()**2)
            oracle = 1/(12*analog**2)
            tolerance = 6*float(delta.std())/math.sqrt(delta.size/8)
            self.check('ADC quantisation',abs(estimate-oracle)<tolerance,dict(a=analog,estimate=estimate,oracle=oracle,tolerance=tolerance))
            rows.append(dict(a=analog,measured=estimate,oracle=oracle,confidence_6se=tolerance))
        self.tables['quantisation'] = rows

    def brightness(self,mutation=None):
        rows = []
        okay = True
        for bright in (0,1):
            prev = None
            for iso in (800,1600,3199.99999,3200,6400,12800,25600):
                u,p = self.m.get(1,iso,bright)
                light = .002
                x = self.r.samples(u,light,mutation=mutation)[:,:,:,0].ravel()
                mean = float(x.mean())
                target = light*p['g']/p['gN']
                tol = 6*float(x.std())/math.sqrt(x.size/8)+target*.002
                okay &= abs(mean-target)<tol
                step = math.log2(mean/prev[1]) if prev else 0.
                if prev:
                    expected_step = math.log2(iso/prev[0]) if bright else 0.
                    okay &= abs(step-expected_step)<.025
                rows.append(dict(mode='Brightens' if bright else 'Neutral',iso=iso,mean=mean,oracle=target,confidence_6se=tol,step_stops=step))
                prev = (iso,mean)
        if mutation:
            return okay,rows
        self.check('brightness and dual-gain continuity',okay,rows)
        self.tables['brightness'] = rows

    def gain_switch(self):
        rows = []
        for iso in (3199.99999,3200.):
            u,p = self.m.get(1,iso)
            x = self.r.samples(u,0.,kind=1)[:,:,:,0].ravel()
            q = 1/(12*p['a']**2)
            read_sigma = math.sqrt(float(x.var(ddof=1))-q)
            target = float(u.view(np.float32)[1])
            self.check('dual-gain read-noise step',abs(read_sigma/target-1)<.02,
                       dict(iso=iso,measured_sigma=read_sigma,preset_sigma=target,headroom=p['headroom']))
            rows.append(dict(iso=iso,gain='HCG' if p['hcg'] else 'LCG',sigma_measured=read_sigma,sigma_preset=target,
                             headroom=p['headroom'],e_sat=p['sat'],noise_floor_stops=math.log2(.18/p['floor']),DR_stops=p['dr']))
        ratio = rows[1]['sigma_measured']/rows[0]['sigma_measured']
        self.check('dual-gain sigma ratio',abs(ratio-.25)<.005,dict(measured=ratio,preset=.25))
        self.check('dual-gain headroom step',abs((rows[1]['headroom']-rows[0]['headroom'])-
                   math.log2(rows[1]['e_sat']/rows[0]['e_sat']))<1e-7,rows)
        self.tables['gain_switch'] = rows

    def clip(self,mutation=None):
        rows = []
        capture_okay = True
        for preset,base in enumerate((100,800,200)):
            for bright in (0,1):
                for iso in sorted(set([base*.5,3200,25600]+[base*2**k for k in range(10) if base*2**k<=25600])):
                    u,p = self.m.get(preset,iso,bright,amount=0.)
                    limit = p['sat']*p['gN']/p['K']
                    value = self.r.render(u,limit,kind=1,w=2048,h=1,gradient=(0.,1.3/2048))[:,:,0].ravel()
                    plateau = float(value[-1])
                    ix = int(np.flatnonzero(value>=plateau-1e-5)[0])
                    observed = (ix+.5)*1.3/2048*p['sat']
                    quant_tol = .51/p['a']
                    self.check('clip point',abs(observed-p['sat'])<.002*p['sat']+quant_tol,dict(preset=preset,iso=iso,bright=bright,observed=observed,oracle=p['sat']))
                    # The extrapolated (unclipped) output mid-grey is .18*g/gN.
                    headroom = math.log2((plateau*p['g']/p['K'])/(.18*p['g']/p['gN']))
                    self.check('headroom',abs(headroom-p['headroom'])<.01,dict(preset=preset,iso=iso,bright=bright,measured=headroom,model=p['headroom']))
                    rows.append(dict(preset=preset,mode='Brightens' if bright else 'Neutral',iso=iso,region='FW' if p['sat']==float(u.view(np.float32)[0]) else 'ADC',
                                     gain='HCG' if p['hcg'] else 'LCG',e_sat=p['sat'],observed_clip=observed,headroom_stops=headroom))
        # A coloured gradient crosses the knee and rails; raw capture must retain
        # >e_sat. Clipping before capture is otherwise indistinguishable once
        # smoothstep has reached 1, so explicitly test this production contract.
        u,p = self.m.get(amount=0.)
        raw = self.r.render(u,[p['sat']/p['K']*x for x in (1,.9,.1)],kind=4,
                            w=2048,h=1,gradient=(.6,.8/2048),mutation=mutation)[0]
        target = (.6+.8*(np.arange(2048)+.5)/2048)*p['sat']
        capture_okay = bool(np.allclose(raw[:,0],target,rtol=2e-6,atol=.01))
        if mutation:
            return capture_okay,dict(max_preclip=float(raw[:,0].max()),e_sat=p['sat'])
        self.check('pre-clip capture',capture_okay,dict(max_preclip=float(raw[:,0].max()),e_sat=p['sat']))
        # Camera-to-working conversion of the same camera-coloured fixture.
        m = u.view(np.float32)[12:24].reshape(3,4)[:,:3].T.astype(float)
        rgb = self.r.render(u,m@np.array([p['sat']/p['K']*x for x in (1,.9,.1)]),
                            w=2048,h=1,gradient=(.6,.8/2048))[0]
        tail = rgb[-200:]
        mu = target[:,None]*np.array([1,.9,.1])
        f = u.view(np.float32)
        measured = (np.rint(np.clip(p['a']*np.minimum(mu,float(f[0]))+float(f[3]),0,float(f[4])))-float(f[3]))/p['a']
        clipped = measured@m.T*p['g']/p['K']
        t = np.clip((mu/p['sat']-.8)/.2,0,1)
        t = (t*t*(3-2*t)).mean(axis=1)
        oracle = clipped*(1-t[:,None])+(clipped@LUMA)[:,None]*t[:,None]
        self.check('hue clip equation',np.max(np.abs(rgb-oracle))<2e-5,dict(max_error=float(np.max(np.abs(rgb-oracle)))))
        neutral = self.r.render(u,m@np.array([p['sat']/p['K']*x for x in (12,10.8,1.2)]),w=1,h=1)[0]
        self.check('highlight neutral/no magenta',np.max(np.ptp(neutral,axis=1))<1e-5 and np.max(np.abs(np.diff(rgb,axis=0)))<.01,
                   dict(tail_chroma=float(np.max(np.ptp(neutral,axis=1))),max_step=float(np.max(np.abs(np.diff(rgb,axis=0))))))
        self.tables['headroom'] = rows
        self.tables['highlight'] = [dict(x=float(x),r=float(v[0]),g=float(v[1]),b=float(v[2])) for x,v in zip(target/p['sat'],rgb)]

    def colour(self,mutation=None):
        u,p = self.m.get()
        x = self.r.samples(u,100*p['gN']/p['K'],mutation=mutation)*p['K']/p['g']
        flat = x.reshape(-1,3)
        covariance = np.cov(flat.T)
        f = u.view(np.float32)
        m = f[12:24].reshape(3,4)[:,:3].T.astype(float)
        v = 100+float(f[1])**2+1/(12*p['a']**2)
        oracle = m@np.diag([v,v*.5,v])@m.T
        neff = flat.shape[0]/8
        tol = 6*np.sqrt((np.outer(np.diag(oracle),np.diag(oracle))+oracle**2)/neff)
        cov_okay = bool(np.all(np.abs(covariance-oracle)<tol))
        cam = flat@np.linalg.inv(m).T
        green_ratio = float(cam[:,1].var()/((cam[:,0].var()+cam[:,2].var())*.5))
        # Isolate Gaussian luma/chroma to test the spatial contract without
        # imposing a filter on discrete low-count Poisson or quantisation.
        fields = self.r.samples(u,0.,kind=6,mutation=mutation)
        c = np.linalg.cholesky(m@np.diag([float(f[1])**2,float(f[1])**2*.5,float(f[1])**2])@m.T)
        axis = c.T@LUMA
        axis /= np.linalg.norm(axis)
        helper = np.array([0,0,1]) if abs(axis[2])<.9 else np.array([1,0,0])
        a = np.cross(axis,helper); a /= np.linalg.norm(a)
        b = np.cross(axis,a)
        white = fields@np.linalg.inv(c).T
        lum,chr1,chr2 = white@axis,white@a,white@b
        freq = np.fft.fftfreq(SIZE)
        yy,xx = np.meshgrid(freq,freq,indexing='ij')
        high = np.sqrt(xx**2+yy**2)>.3
        def power(field):
            spectrum = np.abs(np.fft.fft2(field,axes=(-2,-1)))**2
            spectrum[:,0,0]=0
            return float(spectrum[:,high].sum()/spectrum.sum())
        lpower,cpower = power(lum),.5*(power(chr1)+power(chr2))
        parity = [float(chr1[:,y::2,x::2].var()) for y in (0,1) for x in (0,1)]
        lattice_okay = all(abs(v-1)<.05 for v in parity)
        spatial_okay = lpower>.65 and cpower<.25 and cpower<lpower*.4
        detail = dict(measured=covariance.tolist(),oracle=oracle.tolist(),confidence_6se=tol.tolist(),green_ratio=green_ratio,
                      luma_high_frequency=lpower,chroma_high_frequency=cpower,lattice_parity_variance=parity)
        if mutation:
            return cov_okay and spatial_okay and lattice_okay,detail
        self.check('RGB covariance',cov_okay,detail)
        self.check('Bayer green',abs(green_ratio-.5)<.025,green_ratio)
        self.check('luma/chroma spectra',spatial_okay,detail)
        self.check('normalised lattice variance',lattice_okay,detail)
        self.tables['colour'] = detail
        self.tables['spectra'] = [dict(frequency=float(freq[i]),luma=float(np.mean(np.abs(np.fft.fft(lum,axis=2))**2,axis=(0,1))[i]),
                                      chroma=float(np.mean(np.abs(np.fft.fft(chr1,axis=2))**2,axis=(0,1))[i])) for i in range(SIZE//2)]

    def determinism(self,mutation=None):
        u,p = self.m.get()
        light = 100/p['K']
        x = self.r.render(u,light,mutation=mutation)
        again = self.r.render(u,light,mutation=mutation)
        correlations = []
        for word in range(4):
            v = u.copy()
            v[CLOCK:CLOCK+16].view(np.uint32)[word] += 1
            other = self.r.render(v,light,mutation=mutation)
            correlations.append(float(np.corrcoef(x[:,:,0].ravel(),other[:,:,0].ravel())[0,1]))
        detail = dict(bit_identical=x.tobytes()==again.tobytes(),frame_low=correlations[0],frame_high=correlations[1],seed=correlations[2],stream=correlations[3])
        okay = detail['bit_identical'] and all(abs(c)<.035 for c in correlations)
        if mutation:
            return okay,detail
        self.check('determinism and stream isolation',okay,detail)
        self.tables['determinism'] = detail

    def black(self,mutation=None):
        rows = []
        okay = True
        processing_mutation = mutation in ('processed_1234','processed_code_zero')
        for iso in (100,800,3200,25600):
            u,p = self.m.get(0,iso)
            x = self.r.samples(u,0.,mutation=mutation)
            mean = float(x.mean())
            tol = 6*float(x[:,:, :,0].std())/math.sqrt(x.shape[0]*SIZE*SIZE/8)
            okay &= abs(mean)<tol
            rows.append(dict(iso=iso,operator='signed raw',mean=mean,confidence_6se=tol))
            if mutation and not processing_mutation:
                continue
            # Independent physical black oracle: four IID camera-site read
            # normals, each ADC quantised before the two greens are averaged.
            f=u.view(np.float32)
            rng=np.random.default_rng(0xb1ac+iso)
            sites=rng.standard_normal((512,512,4))*float(f[1])
            dn=np.rint(np.clip(sites*p['a']+float(f[3]),0,float(f[4])))
            electrons=(dn-float(f[3]))/p['a']
            camera=np.stack((electrons[:,:,0],electrons[:,:,1:3].mean(axis=2),electrons[:,:,3]),axis=2)
            oracle_input=(camera@matrix(u,12).T)*p['g']/p['K']
            for name,typ,params in (('default/Khronos',0,(1,0,0,0)),('ACES',1,(1,0,0,0)),('AgX',6,agx_params()),('S-Log3',8,(0,0,0,0))):
                for gamma in (1.,.999):
                    settings = dict(type=typ,params=params,gamma=gamma)
                    value = self.r.samples(u,0.,kind=3,processing=settings,mutation=mutation)
                    oracle=self.r.process_pixels(u,oracle_input,settings)
                    vflat,oflat=value.reshape(-1,3),oracle.reshape(-1,3)
                    tolerance=6*np.sqrt(vflat.var(axis=0)/(len(vflat)/8)+oflat.var(axis=0)/len(oflat))+2e-7
                    processed_okay=bool(np.all(np.abs(vflat.mean(axis=0)-oflat.mean(axis=0))<tolerance))
                    row = dict(iso=iso,operator=name,LGG='identity' if gamma==1. else 'active inverse-gamma .999',
                               mean=float(value.mean()),r=float(value[:,:,:,0].mean()),g=float(value[:,:,:,1].mean()),b=float(value[:,:,:,2].mean()),
                               oracle=oflat.mean(axis=0).tolist(),confidence_6se=tolerance.tolist(),passed=processed_okay)
                    if typ==8:
                        code=self.r.samples(u,0.,kind=7,processing=settings,mutation=mutation)
                        expected_code=self.r.process_pixels(u,oracle_input,settings,code=True)
                        code_tolerance=6*math.sqrt(float(code.var())/(code.size/24)+float(expected_code.var())/(expected_code.size/3))+2e-7
                        row['code_mean'] = float(code.mean())
                        row['code_oracle'] = float(expected_code.mean())
                        row['code_confidence_6se'] = code_tolerance
                        row['zero_noise_code'] = 95/1023
                        # Production linear_to_srgb uses exponent .41666,
                        # rather than exact 1/2.4; assert that actual equation.
                        nominal_linear=((95/1023+.055)/1.055)**2.4
                        zero_oracle=1.055*nominal_linear**.41666-.055
                        row['zero_noise_code_oracle']=zero_oracle
                        noiseless,_=self.m.get(0,iso,amount=0.)
                        zero=self.r.render(noiseless,0.,kind=7,processing=settings,w=1,h=1,mutation=mutation)
                        row['zero_noise_measured']=float(zero.mean())
                        processed_okay &= abs(row['code_mean']-row['code_oracle'])<code_tolerance and abs(row['zero_noise_measured']-zero_oracle)<2e-7
                    row['passed']=bool(processed_okay)
                    okay &= processed_okay
                    if not mutation:
                        self.check('processed black means and S-Log3 codes',processed_okay,row)
                    rows.append(row)
        if mutation:
            return okay,rows
        self.check('signed and processed black',okay,rows)
        self.tables['black'] = rows
        # All preset modes must preserve signed black, including the largest
        # analog gain, where insufficient physical pedestal becomes visible.
        for preset,base in enumerate((100,800,200)):
            for iso in sorted(set((base,3200,25600))):
                u,p = self.m.get(preset,iso)
                x = self.r.samples(u,0.)[:,:,:,0].ravel()
                tol = 6*float(x.std())/math.sqrt(x.size/8)
                self.check('all-preset signed black',abs(float(x.mean()))<tol,
                           dict(preset=preset,iso=iso,mean=float(x.mean()),confidence_6se=tol))

    def adversarial(self):
        u,_ = self.m.get()
        cases = [(-1,-2,-3),(float('nan'),)*3,(float('inf'),)*3,(-float('inf'),)*3,
                 (3e38,1e30,2e35),(float('nan'),float('inf'),-1),(0,1000000,.1),(.001,1e-9,10)]
        for value in cases:
            self.r.render(u,value,w=32,h=32)
        self.check('NaN/Inf/negative/huge HDR finite',True,len(cases))

    def mutations(self):
        rows = []
        for name in MUTATIONS:
            # Link first: a syntax error is a gate failure, never a killed mutant.
            self.r.program(name)
            if name=='gaussian_low':
                passed,detail = self.poisson(name)
            elif name=='gain_reset':
                passed,detail = self.brightness(name)
            elif name=='clamp_black':
                passed,detail = self.black(name)
            elif name=='clip_capture':
                passed,detail = self.clip(name)
            elif name=='frame_only':
                passed,detail = self.determinism(name)
            elif name=='linear_key':
                passed,detail = self.temporal(name)
            elif name=='green_difference':
                passed,detail = self.green_sites(name)
            elif name=='average_green_peak':
                passed,detail = self.green_peak(name)
            elif name=='stalled_cdf':
                passed,detail = self.poisson_tail(name)
            elif name=='peak_hue':
                passed,detail = self.hue_ramp(name)
            elif name=='fixed_lattice':
                passed,detail = self.lattice_phase(name)
            elif name.startswith('processed_'):
                passed,detail = self.black(name)
            elif name in ('rgb_confetti','unnormalised_lattice'):
                passed,detail = self.colour(name)
            else:
                u,p = self.m.get(0,1600)
                mu = 80.
                x = self.r.samples(u,mu*p['gN']/p['K'],kind=1,mutation=name)[:,:,:,0].ravel()
                expected = mu+9+1/(12*p['a']**2)
                detail = dict(variance=float(x.var()),oracle=expected,tolerance=.04*expected)
                passed = abs(x.var()-expected)<.04*expected
            self.check('mutation killed: '+name,not passed,detail)
            rows.append(dict(mutation=name,killed=not passed,predicate=dict(output_light='PTC variance',gaussian_low='Poisson GOF/discreteness',gain_reset='brightness continuity',
                        clamp_black='signed black mean',rgb_confetti='covariance and spectra',clip_capture='unbounded pre-clip capture',frame_only='stream decorrelation',
                        unnormalised_lattice='lattice parity variance',linear_key='all-channel temporal correlation',
                        green_difference='green site variance/covariance',average_green_peak='single-site saturation capture',
                        stalled_cdf='upper-tail exact quantiles',peak_hue='progressive saturation loss',
                        fixed_lattice='parity lag correlation',processed_1234='processed black mean',
                        processed_code_zero='S-Log3 black code')[name],evidence=detail))
            progress('mutation '+name+': '+('KILLED' if not passed else 'SURVIVED'))
        self.tables['mutations'] = rows

    def temporal(self,mutation=None,hires=False):
        pairs = ((0,1),(1,2),(13,0x12abcdef98765432),
                 (0xfffffffe,0xffffffff),(0xffffffff,0x100000000),
                 (0x100000000,0x100000001),(0x12345678ffffffff,0x1234567900000000))
        rows = []
        for lam in (.5,19.9,100.):
            for first,second in pairs:
                fields = []
                for frame in (first,second):
                    u,p = self.m.get(1,3200,frame=frame)
                    fields.append(self.r.render(u,lam*p['gN']/p['K'],kind=1,
                        w=3840 if hires else SIZE,h=2160 if hires else SIZE,mutation=mutation))
                rho = [correlation(fields[0][:,:,channel],fields[1][:,:,channel]) for channel in range(3)]
                rows.append(dict(lam=lam,first=hex(first),second=hex(second),rho=rho))
        limit = .008 if hires else .035
        okay = all(abs(value)<limit for row in rows for value in row['rho'])
        if mutation:
            return okay,rows
        self.check('4K all-channel temporal correlation' if hires else 'all-channel temporal correlation',okay,dict(limit=limit,rows=rows))
        self.tables['temporal_4k' if hires else 'temporal'] = rows

    def green_sites(self,mutation=None):
        rows = []
        okay = True
        for lam in (0.,19.9,100.):
            for amount in (0.,.5,2.,4.):
                for chroma in (0.,.5,1.,2.,4.):
                    u,p = self.m.get(1,3200,amount=amount,chroma=chroma)
                    x = self.r.samples(u,lam*p['gN']/p['K'],kind=8,frames=4,mutation=mutation)[:,:,:,:2].reshape(-1,2)
                    read_basis = matrix(u,BASIS)
                    scale = np.array([1,chroma,chroma])
                    vg = float(np.sum((read_basis[1]*scale)**2))
                    if lam>=20:
                        shot_basis = camera_basis(u,[lam,lam*.5,lam])
                        vg += float(np.sum((shot_basis[1]*scale)**2))
                    else:
                        vg += lam*.5
                    expected = 2*amount**2*vg
                    measured = np.cov(x.T)
                    neff = len(x)/8
                    tolerance = 6*math.sqrt(2/neff)*expected+.0001
                    cov_tolerance = 6*expected/math.sqrt(neff)+.0001
                    row = dict(lam=lam,amount=amount,chroma=chroma,oracle=expected,
                        variance=np.diag(measured).tolist(),covariance=float(measured[0,1]),
                        variance_6se=tolerance,covariance_6se=cov_tolerance)
                    row['passed'] = bool(np.max(np.abs(np.diag(measured)-expected))<tolerance and abs(measured[0,1])<cov_tolerance)
                    okay &= row['passed']; rows.append(row)
        # Near the ADC rails compare each digitised site with its own preclip
        # value. Also check the independent-site marginal/covariance there.
        rails = []
        for rail,lam in (('black',0.),('ADC white',98.),('full well low-count',19.9),('full well Gaussian',98.)):
            for amount,chroma in ((.5,0.),(2.,.5),(4.,4.)):
                u,p = self.m.get(1,3200,amount=amount,chroma=chroma)
                f = u.view(np.float32)
                f[0] = 20 if 'low-count' in rail else 100 if 'full well' in rail else 100000
                f[2:8] = [4,2 if rail=='black' else 512,912 if rail=='ADC white' else 4095,1,1,.8]
                if 'full well' in rail:
                    f[1] = 0
                    f[11] = 0
                    f[BASIS:BASIS+12] = 0
                pre = self.r.samples(u,lam,kind=8,frames=4,mutation=mutation)[:,:,:,:2]
                digitised = self.r.samples(u,lam,kind=9,frames=4,mutation=mutation)[:,:,:,:2]
                physical = np.minimum(pre,float(f[0])) if 'full well' in rail else pre
                oracle = (np.rint(np.clip(4*physical+float(f[3]),0,float(f[4])))-float(f[3]))/4
                # round()'s tie convention is implementation-defined in GLSL;
                # random continuous inputs do not sit exactly on half codes.
                signal=4*physical+float(f[3])
                ulp_bound=8*np.finfo(np.float32).eps*np.maximum(np.abs(signal),1.)
                lower=(np.rint(np.clip(signal-ulp_bound,0,float(f[4])))-float(f[3]))/4
                upper=(np.rint(np.clip(signal+ulp_bound,0,float(f[4])))-float(f[3]))/4
                error=float(max(np.max(lower-digitised),np.max(digitised-upper),0.))
                # Independent marginal oracle at the rails, including the
                # full-well-before-read order. Pointwise clipping preserves
                # independent green sites, hence their post-ADC rho is zero.
                rng=np.random.default_rng(0x600d+len(rails))
                count=int(np.prod(digitised.shape[:-1]))
                shape=(count,2)
                if lam<20:
                    reference_photons=lam+amount*(rng.poisson(lam,size=shape)-lam)
                else:
                    basis=camera_basis(u,[lam,lam*.5,lam])
                    shot_variance=2*amount**2*float(np.sum((basis[1]*np.array([1,chroma,chroma]))**2))
                    reference_photons=lam+rng.standard_normal(shape)*math.sqrt(shot_variance)
                read_variance=2*amount**2*float(np.sum((matrix(u,BASIS)[1]*np.array([1,chroma,chroma]))**2))
                reference_signal=np.minimum(reference_photons,float(f[0]))+rng.standard_normal(shape)*math.sqrt(read_variance)
                reference=(np.rint(np.clip(4*reference_signal+float(f[3]),0,float(f[4])))-float(f[3]))/4
                actual=digitised.reshape(-1,2)
                actual_var,reference_var=actual.var(axis=0),reference.var(axis=0)
                neff=count/8
                mean_bound=6*np.sqrt(actual_var/neff+reference_var/count)+.0001
                fourth_actual=np.mean((actual-actual.mean(axis=0))**4,axis=0)
                fourth_reference=np.mean((reference-reference.mean(axis=0))**4,axis=0)
                variance_bound=6*np.sqrt((fourth_actual-actual_var**2)/neff+(fourth_reference-reference_var**2)/count)+.0001
                rho=correlation(actual[:,0],actual[:,1])
                rho_bound=6/math.sqrt(neff)+.0001
                row = dict(rail=rail,amount=amount,chroma=chroma,max_adc_error=error,
                    max_nominal_adc_error=float(np.max(np.abs(digitised-oracle))),float32_signal_bound=float(ulp_bound.max()),
                    post_variance=actual_var.tolist(),oracle_variance=reference_var.tolist(),variance_6se=variance_bound.tolist(),
                    post_mean=actual.mean(axis=0).tolist(),oracle_mean=reference.mean(axis=0).tolist(),mean_6se=mean_bound.tolist(),
                    post_correlation=rho,correlation_6se=rho_bound)
                row['passed']=bool(error<1e-5 and np.all(np.abs(actual_var-reference_var)<variance_bound) and
                    np.all(np.abs(actual.mean(axis=0)-reference.mean(axis=0))<mean_bound) and abs(rho)<rho_bound)
                okay &= row['passed']
                rails.append(row)
        if mutation:
            return okay,dict(marginals=rows,rails=rails)
        self.check('green per-site variance and covariance',all(row['passed'] for row in rows),rows)
        self.check('green per-site rails',all(row['passed'] for row in rails),rails)
        self.tables['green_sites'],self.tables['green_rails'] = rows,rails

    def green_peak(self,mutation=None):
        u,p = self.m.get(1,3200,amount=2.)
        f = u.view(np.float32)
        f[0],f[2],f[3],f[4],f[5],f[6] = 100000,4,512,912,1,1
        x = self.r.render(u,[0,98,0],kind=8,mutation=mutation)
        peak = np.maximum(x[:,:,0],x[:,:,1])
        single = (x[:,:,0]>100) ^ (x[:,:,1]>100)
        detail = dict(single_site_saturations=int(single.sum()),max_error=float(np.max(np.abs(peak-x[:,:,2]))),
            missed=int(np.count_nonzero(single & (x[:,:,2]<100))))
        okay = detail['single_site_saturations']>100 and detail['max_error']<1e-5 and detail['missed']==0
        if mutation:
            return okay,detail
        self.check('single-green-site preclip maximum',okay,detail)
        self.tables['green_peak'] = detail

    def poisson_tail(self,mutation=None):
        u,_ = self.m.get()
        values = []
        for width,offset,step in ((1024,0.,20/1024),(1,16.056196,0.),(1,1e-7,0.),(1,.0001,0.),(1,.01,0.)):
            x = self.r.render(u,1.,kind=12,w=width,h=8,gradient=(offset,step),mutation=mutation)[:,:,0]
            lambdas = np.float32(offset)+np.float32(step)*(np.arange(width,dtype=np.float32)+np.float32(.5))
            expected = np.empty_like(x)
            for column,lam in enumerate(lambdas):
                mass = math.exp(-float(lam)); terms = [mass]
                for k in range(1,100):
                    mass *= float(lam)/k; terms.append(mass)
                cdf = np.array([math.fsum(terms[:k+1]) for k in range(100)])
                expected[:,column] = np.searchsorted(cdf,1-(np.arange(8)+.5)/8388608.)
            values.append(dict(offset=offset,step=step,n=x.size,max_quantile_error=float(np.max(np.abs(x-expected))),
                largest_uniform=float(x[0,0]),oracle=float(expected[0,0])))
        # Validate the omitted-tail bound independently in double precision.
        largest_tail = 0.
        for lam in np.geomspace(1e-9,19.999999,5000):
            top = 3 if lam<1e-4 else 6 if lam<.01 else min(64,math.ceil(lam+10*math.sqrt(lam)+6))
            mass = math.exp(-lam+(top+1)*math.log(lam)-math.lgamma(top+2)); tail=mass
            for k in range(top+2,140):
                mass *= lam/k; tail+=mass
            largest_tail=max(largest_tail,tail)
        okay = all(row['max_quantile_error']==0 for row in values) and largest_tail<2e-14
        detail = dict(rows=values,omitted_tail_bound_measured=largest_tail)
        if mutation:
            return okay,detail
        self.check('float32 Poisson upper-tail quantiles',okay,detail)
        self.tables['poisson_tail'] = detail

    def hue_ramp(self,mutation=None):
        u,p = self.m.get(amount=0.)
        # Direct helper avoids integer-ADC stairs when checking the smooth
        # reconstruction function itself. End-to-end clip() checks the ADC.
        rows = []
        okay = True
        for ratios in ((1,.85,.1),(.1,.85,1),(1,.1,.85),(.85,1,.1)):
            # Keep a fixed chromatic vector: the measured mixing coefficient
            # must be monotonic even as individual camera channels saturate.
            rgb = self.r.render(u,np.array(ratios)*p['sat'],kind=13,w=8192,h=1,
                gradient=(.6,11.4/8192),mutation=mutation)[0]
            sat = np.ptp(rgb,axis=1)/(max(ratios)-min(ratios))
            row = dict(ratios=ratios,first_clip_saturation=float(sat[np.argmin(np.abs(np.linspace(.6,12,8192)-1))]),
                maximum_upward_step=float(np.max(np.diff(sat))),max_step=float(np.max(np.abs(np.diff(rgb,axis=0)))),
                final_chroma=float(sat[-1]))
            okay &= row['first_clip_saturation']>.2 and row['maximum_upward_step']<2e-6 and row['max_step']<.01 and row['final_chroma']<1e-6
            rows.append(row)
        # Full sensor cyan/yellow ramps: green must never fall below both R/B
        # (a magenta cast); retain finite output through complete saturation.
        for ratios in ((.1,1,1),(1,1,.1)):
            rgb = self.r.render(u,matrix(u,12)@(np.array(ratios)*p['sat']/p['K']),
                w=8192,h=1,gradient=(.6,11.4/8192),mutation=mutation)[0]
            okay &= bool(np.all(rgb[:,1]>=np.minimum(rgb[:,0],rgb[:,2])-1e-5))
        if mutation:
            return okay,rows
        self.check('progressive continuous monotonic highlight desaturation',okay,rows)
        self.tables['hue_ramp'] = rows

    def lattice_phase(self,mutation=None,hires=False):
        differences = {(axis,lag):[] for axis in (0,1) for lag in (1,2)}
        rows = []
        frames=32 if hires else 64
        for frame in range(frames):
            u,_ = self.m.get(frame=frame)
            field = self.r.render(u,0.,kind=5,mutation=mutation,w=3840 if hires else SIZE,h=2160 if hires else SIZE)[:,:,0]
            for axis in (0,1):
                image = field if axis==1 else field.T
                if hires:
                    image=image[::4,:]
                for lag in (1,2):
                    rhos = [correlation(image[:,parity:image.shape[1]-lag:2],image[:,parity+lag::2]) for parity in (0,1)]
                    differences[(axis,lag)].append(rhos[0]-rhos[1])
        for (axis,lag),delta in differences.items():
            bound = 6*float(np.std(delta,ddof=1))/math.sqrt(frames)+.008
            rows.append(dict(axis=axis,lag=lag,parity_difference=float(np.mean(delta)),confidence_6se=bound))
        okay = all(abs(row['parity_difference'])<row['confidence_6se'] for row in rows)
        if mutation:
            return okay,rows
        self.check('4K no fixed chroma correlation phase' if hires else 'no fixed two-pixel chroma correlation phase',okay,rows)
        self.tables['lattice_phase_4k' if hires else 'lattice_phase'] = rows

    def layout(self):
        gl = self.r.gl
        pr = self.r.program()
        names = (C.c_char_p*7)(*[('sensor.'+name).encode() for name in
            ('physics','scale','controls','cameraToWorking','workingToCamera','clock','readCameraBasis')])
        indices = (C.c_uint*7)(); offsets = (C.c_int*7)()
        gl.fn('glGetUniformIndices',None,C.c_uint,C.c_int,C.POINTER(C.c_char_p),C.POINTER(C.c_uint))(pr,7,names,indices)
        gl.fn('glGetActiveUniformsiv',None,C.c_uint,C.c_int,C.POINTER(C.c_uint),C.c_uint,C.POINTER(C.c_int))(pr,7,indices,0x8A3B,offsets)
        size = C.c_int()
        gl.fn('glGetActiveUniformBlockiv',None,C.c_uint,C.c_uint,C.c_uint,C.POINTER(C.c_int))(pr,0,0x8A40,C.byref(size))
        expected = [0,16,32,48,96,CLOCK,BASIS*4]
        self.check('std140 CPU/GLSL compact layout',list(offsets)==expected and size.value==208 and 'readCholesky' not in self.r.sensor,
            dict(offsets=list(offsets),expected=expected,block_size=size.value,unused_readCholesky='readCholesky' in self.r.sensor))

    def model_sensitivity(self):
        def predicate(model):
            rows=[]
            for preset in range(3):
                for bright in (0,1):
                    for iso in (3200,25600):
                        u,p=model.get(preset,iso,bright)
                        sr=float(u.view(np.float32)[1])
                        expected=math.sqrt(sr*sr+1/(12*p['a']**2))*(iso/p['base'])/p['K']
                        rows.append(dict(preset=preset,iso=iso,bright=bright,floor=p['floor'],oracle=expected,
                            passed=abs(p['floor']-expected)<1e-10 and abs(p['dr']-math.log2(p['sat']/sr))<1e-10))
            return all(row['passed'] for row in rows),rows
        passed,rows=predicate(self.m)
        self.check('non-base ISO noise floor and DR both modes',passed,rows)
        source=(ROOT/'indra/newview/alsensormodel.cpp').read_text(encoding='utf-8')
        old='p.gain / p.electronsPerLinear;'
        assert old in source
        path=OUT/'floor_mutation_probe'/'floor_mutation.cpp'
        path.parent.mkdir(exist_ok=True)
        path.write_text(source.replace(old,'1.0 / p.electronsPerLinear;',1),encoding='utf-8')
        mutant=Model(OUT/'floor_mutation_probe',path)
        survived,detail=predicate(mutant)
        self.check('mutation killed: CPU floor missing gain',not survived,detail)

    def report_contract(self):
        checks=[dict(group='a',passed=False),dict(group='b',passed=True)]
        groups=dict(a='FAIL',b='PASS',c='SKIP',d='PENDING-QUIET')
        def evaluate(function):
            return [function(checks,groups,[key]) for key in ('a','b','c','absent','d')]
        observed=evaluate(gate_status)
        expected=['FAIL','PASS','SKIP','NOT RUN','PENDING-QUIET']
        # A summary that unconditionally returns PASS must fail this predicate.
        self.check('report statuses and must-fail unconditional PASS',observed==expected and evaluate(lambda *args:'PASS')!=expected,observed)
        ids=provenance(self.r.sensor)
        self.check('report commit and source SHA256',len(ids['commit'])==40 and all(len(value)==64 for value in ids['sha256'].values()),ids)

    def coverage(self):
        def read(path):
            if self.r.baseline_ref:
                return subprocess.check_output(['git','show',self.r.baseline_ref+':'+path],cwd=ROOT,text=True,encoding='utf-8',timeout=10)
            return (ROOT/path).read_text(encoding='utf-8')
        tester=read('scripts/code_tools/sensor_iso_gl_check.py')
        tut=read('indra/newview/tests/alsensormodel_test.cpp')
        cases={
            'processed-black acceptance coverage': 'self.check(\'processed black means and S-Log3 codes\'' in tester and "'processed_1234':" in tester and "'processed_code_zero':" in tester,
            'computed report and provenance coverage': 'def gate_status(' in tester and 'shader_compiled_sha256' in tester and "tut=ROOT/" in tester,
            '4K statistical readback coverage': 'def hires(' in tester and 'self.temporal(hires=True)' in tester and 'np.fft.rfft2(image)' in tester,
            'paired p95 uncertainty coverage': 'range(1088)' in tester and 'sensor_p95_ci=sensor_ci' in tester and '2000,len(diff)//16' in tester,
            'TUT non-base ISO floor/DR coverage': 'for (double iso : {preset.baseISO,3200.,25600.})' in tut.split('sensor_model_object::test<2>')[0] and 'noise floor stops' in tut,
        }
        for name,passed in cases.items():
            self.check(name,passed,dict(source_ref=self.r.baseline_ref or 'WORKTREE'))

    def hires(self):
        self.temporal(hires=True)
        self.lattice_phase(hires=True)
        u,_=self.m.get()
        field=self.r.render(u,0.,kind=6,w=3840,h=2160)
        m=matrix(u,12)
        c=np.linalg.cholesky(m@np.diag([9,4.5,9])@m.T)
        axis=c.T@LUMA; axis/=np.linalg.norm(axis)
        tangent=np.cross(axis,[0,0,1]); tangent/=np.linalg.norm(tangent)
        rotation=np.stack((axis,tangent,np.cross(axis,tangent)),axis=1)
        white=field@np.linalg.inv(c).T@rotation
        del field
        rows=[]
        for channel,name in enumerate(('luma','chroma1','chroma2')):
            image=white[:,:,channel]
            adjacent=[correlation(image[:,:-1],image[:,1:]),correlation(image[:-1,:],image[1:,:])]
            parity=[float(image[y::2,x::2].var()) for y in (0,1) for x in (0,1)]
            power=np.abs(np.fft.rfft2(image))**2
            power[0,0]=0
            # Compare each bin with a local 16x16 spectral neighbourhood, so
            # the intended low-pass chroma envelope is not mistaken for a line.
            blocks=power[:,:1920].reshape(135,16,120,16)
            envelope=blocks.mean(axis=(1,3),keepdims=True)
            spectral_peak=float(np.max(blocks/np.maximum(envelope,1e-12)))
            nyquist=float(max(power[1080,0],power[0,1920],power[1080,1920])/max(power.mean(),1e-12))
            row=dict(field=name,adjacent=adjacent,parity_variance=parity,local_spectral_peak=spectral_peak,parity_spectral_peak=nyquist)
            self.check('4K spatial autocorrelation and 2D spectra',
                (all(abs(value)<.008 for value in adjacent) if channel==0 else all(.2<value<.95 for value in adjacent)) and
                all(abs(value-1)<.02 for value in parity) and spectral_peak<35 and nyquist<25,row)
            rows.append(row)
        self.tables['spatial_4k']=rows

    def cost(self):
        gl = self.r.gl
        rows = []
        for iso,fixture in ((100,'gradient'),(25600,'gradient'),(25600,'flat lambda=0.5'),
                            (25600,'flat lambda=5'),(25600,'flat lambda=19.9'),(25600,'flat lambda=20')):
            u,_ = self.m.get(0,iso)
            _,p = self.m.get(0,iso)
            gradient = (.00001,1.) if fixture=='gradient' else (1.,0.)
            light = 1. if fixture=='gradient' else float(fixture.split('=')[1])*p['gN']/p['K']
            samples = [[],[]]
            # Interleaved baseline/sensor queries; both write the SAME 4K 16F
            # quad. Report raw times AND differences, not CPU wall-clock time.
            for index in range(1088):
                # Balance AB and BA to remove a consistent order advantage.
                for baseline in ((True,False) if index%2==0 else (False,True)):
                    self.r.prepare(u,light,w=3840,h=2160,cost=True,baseline=baseline,gradient=gradient)
                    query = C.c_uint()
                    gl.gen_queries(1,C.byref(query))
                    gl.begin_query(0x88BF,query)
                    gl.draw(0x0004,0,6)
                    gl.end_query(0x88BF)
                    ns = C.c_uint64()
                    gl.query_u64(query,0x8866,C.byref(ns))
                    self.r.delete_query(1,C.byref(query))
                    if index>=64:
                        samples[int(not baseline)].append(ns.value*1e-6)
            base,sensor = np.asarray(samples)
            diff = sensor-base
            rng=np.random.default_rng(0x51c057)
            # Predeclared 95% moving-block bootstrap interval for p95, block
            # length 16, 2,000 replicates; retain all 1,024 interleaved pairs.
            starts=rng.integers(0,len(diff),size=(2000,len(diff)//16))
            indices=(starts[:,:,None]+np.arange(16)[None,None,:])%len(diff)
            indices=indices.reshape(2000,len(diff))
            def interval(values):
                return np.percentile(np.percentile(values[indices],95,axis=1),[2.5,97.5]).tolist()
            sensor_ci,delta_ci=interval(sensor),interval(diff)
            baseline_rho=correlation(base,sensor)
            shared=(np.percentile(base,95)>max(.25,2*np.median(base)) and baseline_rho>.3)
            status='PASS' if sensor_ci[1]<=.25 else 'PENDING-QUIET' if shared else 'FAIL'
            row = dict(iso=iso,fixture=fixture,n=len(diff),baseline_p50=float(np.median(base)),baseline_p95=float(np.percentile(base,95)),
                       sensor_p50=float(np.median(sensor)),sensor_p95=float(np.percentile(sensor,95)),
                       paired_delta_p50=float(np.median(diff)),paired_delta_p95=float(np.percentile(diff,95)),
                       sensor_p95_ci=sensor_ci,paired_delta_p95_ci=delta_ci,baseline_sensor_correlation=baseline_rho,
                       status=status,baseline_samples=base.tolist(),sensor_samples=sensor.tolist(),paired_samples=diff.tolist())
            rows.append(row)
            self.check('4K sensor p95 upper CI <=0.25ms',status in ('PASS','PENDING-QUIET'),
                {key:value for key,value in row.items() if not key.endswith('_samples')})
            if status=='PENDING-QUIET':
                self.groups['cost']='PENDING-QUIET'
            progress('4K cost '+fixture+': '+status+' p95 CI='+str(sensor_ci))
        self.tables['cost'] = rows

    def scope(self):
        diff=subprocess.run(['git','diff','--check'],cwd=ROOT,text=True,capture_output=True,timeout=10)
        paths=subprocess.check_output(['git','diff','--name-only'],cwd=ROOT,text=True,timeout=10).splitlines()
        shaders=[path for path in paths if '/app_settings/shaders/' in path]
        self.check('git diff --check',diff.returncode==0,diff.stdout+diff.stderr)
        self.check('no other production shader changed',shaders==[SHADER.relative_to(ROOT).as_posix()],shaders)


def agx_params():
    contrast,white = 1.25,16.29
    toe = (1/.18-1)*.18**contrast
    slope = contrast*.18**(contrast-1)*toe/(.18**contrast+toe)**2
    return (contrast,toe,slope,(white-.18)**2/.82*slope)


def save_csv(name,rows,folder):
    import csv
    if not rows:
        return
    fields = list(dict.fromkeys(k for row in rows for k in row))
    with (folder/(name+'.csv')).open('w',newline='',encoding='utf-8') as out:
        writer = csv.DictWriter(out,fieldnames=fields)
        writer.writeheader(); writer.writerows(rows)


def table(rows,columns):
    lines = ['| '+' | '.join(columns)+' |','| '+' | '.join('---' for _ in columns)+' |']
    for row in rows:
        values = [row.get(c,'') for c in columns]
        lines.append('| '+' | '.join(('%.6g'%v if isinstance(v,float) else str(v)) for v in values)+' |')
    return '\n'.join(lines)


def report(suite,error=None):
    OUT.mkdir(parents=True,exist_ok=True)
    plots=OUT/'wpa_plots'; plots.mkdir(exist_ok=True)
    prefix='baseline_' if suite.r.baseline_ref else ''
    for name,rows in suite.tables.items():
        if isinstance(rows,list) and rows and isinstance(rows[0],dict):
            save_csv(prefix+name,rows,plots)
    ids=suite.source_ids
    overall=gate_status(suite.checks,suite.groups,SUITES)
    summary=dict(status=overall,renderer=suite.r.gl.renderer,provenance=ids,
        baseline_ref=suite.r.baseline_ref,groups=suite.groups,checks=suite.checks,
        failures=suite.failures,tables=suite.tables,error=error)
    (OUT/(prefix+'wpa_results.json')).write_text(json.dumps(summary,indent=2,allow_nan=False),encoding='utf-8')
    def state(*names):
        return gate_status(suite.checks,suite.groups,names)
    descriptions=[
        ('Tuple avalanche; every-channel low/high frame/wrap correlations',('temporal','hires')),
        ('Verified chroma defect; equal green difference/average variance; rail tests',('green_sites',)),
        ('Green-site maximum; single-site saturation test',('green_peak',)),
        ('Survival-tail Poisson; largest-uniform quantiles',('poisson_tail',)),
        ('Progressive highlight coverage; monotonic ramps; equation corrected',('hue_ramp','clip')),
        ('Symmetric hashed lattice origin; parity lag tests',('lattice_phase',)),
        ('Processed-black means/codes; independent ADC oracle; injection mutations',('black','mutations')),
        ('Computed statuses, commit/source hashes; summary mutation',('report_contract',)),
        ('4K spatial/2D spectra/parity/wide-frame readbacks',('hires',)),
        ('1,024 AB/BA pairs; p95 bootstrap CI; predeclared gate',('cost',)),
        ('Non-base ISO floor/DR, both modes; missing-gain mutation',('model_sensitivity',)),
        ('Removed GPU readCholesky; 208-byte CPU/GL std140 check',('layout',))]
    lines=['WP-A fixes: '+overall+'.']
    lines += [str(i)+'. '+description+': '+state(*groups)+'.' for i,(description,groups) in enumerate(descriptions,1)]
    fits=suite.tables.get('ptc_fit',[]); pois=suite.tables.get('poisson',[])
    numbers=[]
    if fits:
        numbers.append('PTC slopes %.4f..%.4f'%(min(r['slope'] for r in fits),max(r['slope'] for r in fits)))
    if pois:
        numbers.append('Poisson p %.3g..%.3g'%(min(r['p_value'] for r in pois),max(r['p_value'] for r in pois)))
    costs=suite.tables.get('cost',[])
    if costs:
        worst=max(costs,key=lambda row:row['sensor_p95_ci'][1])
        numbers.append('worst p95 %.3f ms, CI [%.3f,%.3f]'%(worst['sensor_p95'],*worst['sensor_p95_ci']))
    mutations=suite.tables.get('mutations',[])
    numbers.append('mutations %d/%d'%(sum(row['killed'] for row in mutations),len(MUTATIONS)))
    lines += ['; '.join(numbers)+'.',
        'Files: sensorUtilF.glsl, alsensormodel.{h,cpp}, alsensormodel_test.cpp, sensor_iso_gl_check.py, SENSOR_MODEL.md.',
        'Uncommitted. Lead: zs_check/TUT for indra/newview/alsensormodel.cpp and indra/newview/tests/alsensormodel_test.cpp.',
        'Commit: '+ids['commit']+'. Evidence and SHA256: '+prefix+'wpa_results.json.']
    short='\n'.join(lines)+'\n'
    assert len(short.split())<220, len(short.split())
    (OUT/('WPA_BASELINE_REPORT.md' if prefix else 'WPA_REPORT.md')).write_text(short,encoding='utf-8')
    evidence=['# WP-A gate evidence',short,'Renderer: '+suite.r.gl.renderer,
        '```json',json.dumps(ids,indent=2),'```',
        '## Named group states',table([dict(group=name,status=suite.groups.get(name,'NOT RUN')) for name in SUITES],('group','status')),
        '## Named checks',table([dict(name=name,checks=sum(r['name']==name for r in suite.checks),
            failed=sum(not r['passed'] for r in suite.checks if r['name']==name)) for name in dict.fromkeys(r['name'] for r in suite.checks)],('name','checks','failed')),
        '## Statistical rules',
        'All original gates and tolerances are retained. Gaussian statistical bounds use six SE with N_eff=N/8. '
        'Independent Poisson GOF alpha=1e-5, tail bins pooled to at least 20 expected counts. '
        'Processed black uses an independent CPU four-site Gaussian/ADC reference, passed through the actual production LGG/tonemap functions. '
        'Its six-SE bound combines both sample variances. Type-8 nominal noiseless code is 95/1023; '
        'the actual production round-trip uses the .41666 sRGB exponent, giving 0.09286875, checked within 2e-7. '
        'Near-rail tests compare each physical site with its independently computed ADC result. '
        '4K statistical readbacks are separate from timing. Local 2D spectral bins must be below 35 times their 16x16 neighbourhood mean; '
        'parity/Nyquist bins below 25 times global mean. Full-res luma adjacent correlation <0.008. '
        'Temporal tests cover every RGB channel, low/high counts, adjacent/distant clocks and low-word wraps. '
        'The cost gate is the absolute sensor p95 upper 95% moving-block bootstrap bound <=0.25 ms (2,000 replicates, blocks of 16, 1,024 retained pairs). '
        'PENDING-QUIET requires baseline p95 > max(0.25 ms,2*baseline median) and baseline/sensor paired correlation >0.3. '
        'A quiet failure remains FAIL. AB/BA order alternates; all raw samples are retained. '
        'Performance options if this gate fails: retain these validated statistics and optimise shader work further, '
        'or have the lead explicitly revisit the budget; this run never changes the 0.25 ms gate.',
        'VERIFY evidence: original NoiseAmount scales each residual once. At ISO 3200, original read-site correlations were '
        '-0.07697 at chroma 0 and +0.51739 at chroma 4. The chroma claim is confirmed; the double-amount claim is false. '
        'Original adjacent lattice correlations are symmetric, contrary to the disjoint-block claim; lag-two parity modulation is real and tested.',
        '## Artifacts','All tables and raw timestamps: '+prefix+'wpa_results.json. CSV tables: wpa_plots/.',
        '## Lead-only gates','zs_check and TUT execution: NOT RUN. No viewer build, commit, push, or HEAD movement.']
    if suite.failures or error:
        evidence += ['## Failures','\n'.join(dict.fromkeys(suite.failures)),error or '']
    (OUT/(prefix+'WPA_EVIDENCE.md')).write_text('\n\n'.join(evidence)+'\n',encoding='utf-8')
    progress(short)


def worker(args):
    OUT.mkdir(parents=True,exist_ok=True)
    model = Model(OUT/'native_probe')
    runner = Runner(args.baseline_ref)
    suite = Suite(runner,model)
    suite.source_ids=provenance(runner.sensor)
    progress(runner.gl.renderer)
    error = None
    try:
        for name in (args.only.split(',') if args.only else SUITES):
            assert name in SUITES,name
            suite.current_group=name
            progress(name+' starting')
            count=len(suite.checks)
            suite.groups[name]='NOT RUN'
            getattr(suite,name)()
            if suite.groups[name]!='PENDING-QUIET':
                suite.groups[name]='PASS' if len(suite.checks)>count and all(row['passed'] for row in suite.checks[count:]) else 'FAIL'
            progress(name+' complete; failures='+str(len(suite.failures)))
        suite.check('tested sources unchanged during run',suite.source_ids==provenance(runner.sensor),suite.source_ids)
    except Exception:
        error = traceback.format_exc()
        progress(error)
        suite.failures.append('uncaught fixture failure')
        if suite.current_group:
            suite.groups[suite.current_group]='FAIL'
    report(suite,error)
    return 1 if suite.failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker',action='store_true',help=argparse.SUPPRESS)
    parser.add_argument('--timeout',type=int,default=1800)
    parser.add_argument('--only',help='Comma-separated named groups; omitted groups are NOT RUN')
    parser.add_argument('--baseline-ref',help='Read-only baseline shader comparison; expose intermediates only')
    args = parser.parse_args()
    if args.worker:
        return worker(args)
    # Kill a hung driver/compiler worker instead of polling it indefinitely.
    env = dict(os.environ,PYTHONUTF8='1',PYTHONDONTWRITEBYTECODE='1')
    try:
        options=(['--only',args.only] if args.only else [])+(['--baseline-ref',args.baseline_ref] if args.baseline_ref else [])
        return subprocess.run([sys.executable,str(Path(__file__).resolve()),'--worker',*options],cwd=ROOT,env=env,timeout=args.timeout).returncode
    except subprocess.TimeoutExpired:
        progress('FAIL: explicit %ds watchdog expired'%args.timeout)
        return 1


if __name__=='__main__':
    raise SystemExit(main())
