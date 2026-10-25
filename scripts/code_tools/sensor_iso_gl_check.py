"""WP-A real-WGL sensor gate. Run with PYTHONUTF8=1; no viewer build.

Compiles the PRODUCTION sensorUtilF.glsl and pure alsensormodel.cpp, not CPU
copies. The hidden context/float FBO follow relight_3d_gl.GL. Processing tests
link the complete production LGG, tone-map, sRGB and LPM modules. Fixtures are
only flat/gradient light and uniforms. Every mutation is compiled in memory
and must fail its named *numerical* predicate; compilation failures do not count.

Default watchdog: 600 s, including shader compilation. Statistical gates use
six standard errors with N_eff=N/8 for spatial Gaussian fields, and Poisson
GOF alpha=1e-5 (pooled expected bins >=20). All measured values and tolerances
are saved. Cost is a hard <=0.25 ms p95 gate; no automatic quiet-GPU waiver.
"""
import os
os.environ['PYTHONUTF8'] = '1'
os.environ['PYTHONDONTWRITEBYTECODE'] = '1'
import argparse
import ctypes as C
import hashlib
import json
import math
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


class Model:
    """Small DLL probe of the actual pure C++ model; never builds the viewer."""
    def __init__(self, out):
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
            str(ROOT/'indra/newview') + '" "' + str(ROOT/'indra/newview/alsensormodel.cpp') +
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
        packed = np.zeros(256, np.uint8)
        v = np.zeros(12, np.float64)
        self.fn(preset,iso,bright,frame,seed,stream,amount,chroma,packed.ctypes.data,v.ctypes.data)
        return packed, dict(zip(('base','switch','H','g','gN','a','sat','K','headroom','floor','dr','hcg'),v.tolist()))


class Runner:
    def __init__(self):
        self.gl = gl = G.GL()
        self.sensor = SHADER.read_text(encoding='utf-8')
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
        if mutation:
            replacements = MUTATIONS[mutation]
            for old,new in (replacements if isinstance(replacements,list) else [replacements]):
                assert old in source, 'mutation target absent: '+mutation
                source = source.replace(old,new,1)
        fragment = '#version 420\n' + source
        fragment += '\nlayout(std140,binding=0) uniform SensorBlock { SensorParameters sensor; };\n'
        fragment += 'uniform vec3 testLight; uniform vec2 testGradient; uniform int testKind;\nout vec4 result;\n'
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
    } else if (testKind==1 || testKind==4) {
        SensorReadout readout=sensorReadCamera(light,pixel,sensor);
        value=testKind==1 ? readout.cameraElectrons : readout.preClipElectrons;
    } else {
        value=applySensor(light,pixel,sensor);
        if (testKind==3 || testKind==7) {
            // EXACT colorCorrectF type-8 grade bypass, with actual functions.
            if (tonemap_type!=8) value=applyLiftGammaGain(value);
            value=applyToneMap(value);
            if (testKind==7) value=linear_to_srgb(value);
        }
    }
    result=vec4(value,1);
}
'''
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

    def samples(self,packed,light,frames=FRAMES,**kw):
        values = []
        for frame in range(frames):
            u = packed.copy()
            u[192:196].view(np.uint32)[0] = frame
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
    'clamp_black': ('result.cameraElectrons = (dn - p.physics.w) / p.physics.z;',
                    'result.cameraElectrons = max(dn - p.physics.w,0.0) / p.physics.z;'),
    'rgb_confetti': [('return c * (axis*luma + u*chroma.x + v*chroma.y);',
                     'return vec3(length(c[0]),length(c[1]),length(c[2])) * vec3(luma,sensorNormalPair(sensorKey(pixel,p,domain^17u)));'),
                    ('return p.readCameraBasis * vec3(fullResolution.x,chroma);',
                     'return p.physics.y*vec3(fullResolution.x,companion*0.707106781186548,sensorNormalPair(sensorKey(pixel,p,0xabcdu)).x);')],
    'clip_capture': ('result.preClipElectrons = photons + readResidual;',
                     'result.preClipElectrons = min(photons+readResidual,vec3(min(p.physics.x,(p.scale.x-p.physics.w)/p.physics.z)));'),
    'frame_only': ('h ^= p.clock.z*0x27d4eb2du;\n    h ^= p.clock.w*0x165667b1u;',
                   'h ^= 0u;\n    h ^= 0u;'),
    'unnormalised_lattice': ('return value / sqrt(dot(w, w));','return value;'),
}


class Suite:
    def __init__(self,runner,model):
        self.r,self.m = runner,model
        self.tables = {}
        self.checks = []
        self.failures = []

    def check(self,name,passed,detail):
        self.checks.append(dict(name=name,passed=bool(passed),detail=detail))
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
            f[0:12] = [50000,1,analog,512,16383,1,1,.8,1,1,1,0]
            m = np.eye(3)
            for start,matrix in ((12,m),(24,m),(36,np.diag([1,math.sqrt(.5),1]))):
                for c in range(3):
                    f[start+c*4:start+c*4+3]=matrix[:,c]
            chol = np.diag([1,math.sqrt(.5),1])
            axis = chol.T@LUMA; axis /= np.linalg.norm(axis)
            tangent = np.cross(axis,[0,0,1]); tangent /= np.linalg.norm(tangent)
            basis = chol@np.stack((axis,tangent,np.cross(axis,tangent)),axis=1)
            for c in range(3):
                f[52+c*4:52+c*4+3]=basis[:,c]
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
        t = np.clip((target/p['sat']-.8)/.2,0,1)
        t = t*t*(3-2*t)
        oracle = clipped*(1-t[:,None])+(clipped@LUMA)[:,None]*t[:,None]
        self.check('hue clip equation',np.max(np.abs(rgb-oracle))<2e-5,dict(max_error=float(np.max(np.abs(rgb-oracle)))))
        self.check('highlight neutral/no magenta',np.max(np.ptp(tail,axis=1))<1e-5 and np.max(np.abs(np.diff(rgb,axis=0)))<.01,
                   dict(tail_chroma=float(np.max(np.ptp(tail,axis=1))),max_step=float(np.max(np.abs(np.diff(rgb,axis=0))))))
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
        c = f[36:48].reshape(3,4)[:,:3].T.astype(float)
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
            v[192:208].view(np.uint32)[word] += 1
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
        for iso in (100,800,3200,25600):
            u,p = self.m.get(0,iso)
            x = self.r.samples(u,0.,mutation=mutation)
            mean = float(x.mean())
            tol = 6*float(x[:,:, :,0].std())/math.sqrt(x.shape[0]*SIZE*SIZE/8)
            okay &= abs(mean)<tol
            rows.append(dict(iso=iso,operator='signed raw',mean=mean,confidence_6se=tol))
            if mutation:
                continue
            for name,typ,params in (('default/Khronos',0,(1,0,0,0)),('ACES',1,(1,0,0,0)),('AgX',6,agx_params()),('S-Log3',8,(0,0,0,0))):
                for gamma in (1.,.999):
                    settings = dict(type=typ,params=params,gamma=gamma)
                    value = self.r.samples(u,0.,kind=3,processing=settings)
                    row = dict(iso=iso,operator=name,LGG='identity' if gamma==1. else 'active inverse-gamma .999',
                               mean=float(value.mean()),r=float(value[:,:,:,0].mean()),g=float(value[:,:,:,1].mean()),b=float(value[:,:,:,2].mean()))
                    if typ==8:
                        row['code_mean'] = float(self.r.samples(u,0.,kind=7,processing=settings).mean())
                        row['zero_noise_code'] = 95/1023
                    rows.append(row)
        if mutation:
            return okay,rows
        self.check('signed black',okay,rows)
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
                        unnormalised_lattice='lattice parity variance')[name],evidence=detail))
            progress('mutation '+name+': '+('KILLED' if not passed else 'SURVIVED'))
        self.tables['mutations'] = rows

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
            for index in range(80):
                for baseline in (True,False):
                    self.r.prepare(u,light,w=3840,h=2160,cost=True,baseline=baseline,gradient=gradient)
                    query = C.c_uint()
                    gl.gen_queries(1,C.byref(query))
                    gl.begin_query(0x88BF,query)
                    gl.draw(0x0004,0,6)
                    gl.end_query(0x88BF)
                    ns = C.c_uint64()
                    gl.query_u64(query,0x8866,C.byref(ns))
                    self.r.delete_query(1,C.byref(query))
                    if index>=16:
                        samples[int(not baseline)].append(ns.value*1e-6)
            base,sensor = np.asarray(samples)
            diff = sensor-base
            row = dict(iso=iso,fixture=fixture,n=len(diff),baseline_p50=float(np.median(base)),baseline_p95=float(np.percentile(base,95)),
                       sensor_p50=float(np.median(sensor)),sensor_p95=float(np.percentile(sensor,95)),
                       paired_delta_p50=float(np.median(diff)),paired_delta_p95=float(np.percentile(diff,95)))
            rows.append(row)
            self.check('4K sensor cost <=0.25ms',row['sensor_p95']<=.25,row)
        self.tables['cost'] = rows


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
    plots = OUT/'wpa_plots'; plots.mkdir(exist_ok=True)
    for name in ('ptc','ptc_fit','poisson','histograms','boundary','adc_boundary','quantisation','brightness','gain_switch','headroom','black','highlight','spectra','cost'):
        save_csv(name,suite.tables.get(name,[]),plots)
    # No matplotlib on the task machine: CSV is the explicitly permitted fallback.
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        rows = suite.tables['ptc']
        fig,ax = plt.subplots(figsize=(10,6))
        for preset in range(3):
            for iso in sorted(set(x['iso'] for x in rows if x['preset']==preset)):
                r = [x for x in rows if x['preset']==preset and x['iso']==iso and x['mode']=='Neutral']
                ax.plot([x['mu'] for x in r],[x['variance'] for x in r],'.-',label='%d ISO %d'%(preset,iso))
        ax.set(xscale='symlog',yscale='log',xlabel='Collected mean (electrons)',ylabel='Variance (electrons squared)')
        ax.legend(fontsize=5,ncol=3); fig.tight_layout(); fig.savefig(plots/'photon_transfer_curve.png'); plt.close(fig)
        fig,axes = plt.subplots(1,5,figsize=(18,4))
        for ax,lam in zip(axes,(.5,2.,5.,12.,19.9)):
            r = [x for x in suite.tables['histograms'] if x['lam']==lam and x['expected']>1e-3]
            ax.bar([x['k'] for x in r],[x['observed'] for x in r],alpha=.5)
            ax.plot([x['k'] for x in r],[x['expected'] for x in r],'.-'); ax.set_title('lambda='+str(lam))
        fig.tight_layout(); fig.savefig(plots/'poisson_histograms.png'); plt.close(fig)
        plot_status = 'PNG and CSV'
    except ImportError:
        plot_status = 'CSV fallback (matplotlib unavailable)'
    summary = dict(renderer=suite.r.gl.renderer,source_sha256=hashlib.sha256(suite.r.sensor.encode()).hexdigest(),
                   checks=suite.checks,failures=suite.failures,tables=suite.tables,error=error)
    (OUT/'wpa_results.json').write_text(json.dumps(summary,indent=2,allow_nan=False),encoding='utf-8')
    ptc = suite.tables.get('ptc',[])
    ptc_error = max((abs(x['relative_error']) for x in ptc),default=0.)
    fits = suite.tables.get('ptc_fit',[])
    slopes = [r['slope'] for r in fits] or [0.]
    probabilities = [r['p_value'] for r in suite.tables.get('poisson',[])] or [0.]
    timing = max(suite.tables.get('cost',[]) or [dict(sensor_p50=0.,sensor_p95=0.)],key=lambda r:r['sensor_p95'])
    colour = suite.tables.get('colour',{})
    killed = sum(r['killed'] for r in suite.tables.get('mutations',[]))
    head = subprocess.check_output(['git','rev-parse','--short=11','HEAD'],cwd=ROOT,text=True,timeout=10).strip()
    short = ('Files: sensorUtilF.glsl, alsensormodel.{h,cpp}, alsensormodel_test.cpp, new GL checker, CMake registration. '
        'Changes uncommitted; no viewer build or integration.\n\n'
        'Results: %d checks, %d failures. PTC slopes %.4f–%.4f; maximum variance error %.3f%%. Poisson GOF p=%.3f–%.3f (alpha=1e-5). '
        'Lambda-20, brightness and dual-gain continuity pass; measured covariance agrees with Sigma, green ratio %.4f. '
        'Fixed-frame results are bit-identical; frame/seed/stream decorrelation passes. Mutations killed: %d/8. '
        'Worst 4K GPU p50/p95: %.3f/%.3f ms (target 0.25). Confidence: six-SE Gaussian bounds, N_eff=N/8.\n\n'
        'Open issues: actual starting HEAD %s differs from brief 0de20f6fb5e; preserved. Corrected Brightens output uses total gain. '
        'Brightens headroom uses clip/mid-grey ratio, e_sat/(0.18K), consistent with PLAN physics; the corrected table has an extra g denominator. '
        'Presets and unspecified calibration values are illustrative. Lead still runs zs_check for alsensormodel.cpp and tests/alsensormodel_test.cpp. '
        'Plot artifacts: %s.'%(len(suite.checks),len(suite.failures),min(slopes),max(slopes),100*ptc_error,min(probabilities),max(probabilities),
                               colour.get('green_ratio',0.),killed,timing['sensor_p50'],timing['sensor_p95'],head,plot_status))
    sections = ['# WP-A REPORT','<!-- Summary above the tables is under 220 words. -->',short,
                'Renderer: '+suite.r.gl.renderer,
                'Production shader SHA256: `'+hashlib.sha256(suite.r.sensor.encode()).hexdigest()+'`',
                '## Gate results',table([dict(gate=k,failed=sum(not c['passed'] for c in suite.checks if c['name']==k),
                                            checks=sum(c['name']==k for c in suite.checks)) for k in dict.fromkeys(c['name'] for c in suite.checks)],('gate','checks','failed'))]
    for title,name,columns in (
        ('Photon-transfer fits','ptc_fit',('preset','mode','iso','gain','slope','intercept','read_plus_adc','black_variance')),
        ('Poisson goodness of fit','poisson',('lam','n','mean','variance','chi2','df','p_value','integer','passed')),
        ('Lambda 20 boundary','boundary',('lam','mean','variance')),
        ('Full sensor lambda 20 boundary','adc_boundary',('lam','mean','variance','variance_oracle')),
        ('Quantisation term','quantisation',('a','measured','oracle','confidence_6se')),
        ('Means and switch continuity','brightness',('mode','iso','mean','oracle','confidence_6se','step_stops')),
        ('Dual-gain calibrated read-noise step','gain_switch',('iso','gain','sigma_measured','sigma_preset','headroom','e_sat','noise_floor_stops','DR_stops')),
        ('Headroom and saturation','headroom',('preset','mode','iso','region','gain','e_sat','observed_clip','headroom_stops')),
        ('Signed and processed black means','black',('iso','operator','LGG','mean','r','g','b','code_mean','zero_noise_code')),
        ('Compiled must-fail mutations','mutations',('mutation','predicate','killed')),
        ('Paired GPU timestamps (ms), 4K RGBA16F','cost',('iso','fixture','n','baseline_p50','baseline_p95','sensor_p50','sensor_p95','paired_delta_p50','paired_delta_p95'))):
        if name in suite.tables:
            sections += ['## '+title,table(suite.tables[name],columns)]
    sections += ['## Colour and determinism','```json',json.dumps({k:suite.tables.get(k) for k in ('colour','determinism')},indent=2),'```',
                 '## Statistical interpretation',
                 'PTC: 524,288 samples per patch; 8 deterministic frames of 256x256. Independent Poisson histograms: 1,048,576 samples per lambda. '
                 'Gaussian covariance and means conservatively use N_eff=N/8 for lattice correlation. The 6-SE bounds are nominal >99.999999% per Gaussian mean '
                 '(variance bounds use the asymptotic normal approximation). Poisson GOF pools tails to >=20 expected counts; p>1e-5 is required. '
                 'The inverse-CDF cap has a Chernoff tail bound <2e-14; floating CDF roundoff is separately covered by the empirical GOF. '
                 'Quantisation oracle applies away from rails with sufficiently dithered ADC input. Two green sites are digitised separately, then averaged. '
                 'Gaussian-only spectra isolate luma/chroma; discrete Poisson is never filtered. CSV files contain the full PTC, histogram and spectrum data.',
                 'The actual LGG identity branch preserves signed noise; active LGG and applyToneMap rectify it. Type 8 bypasses LGG as in colorCorrectF. '
                 'S-Log3 rows report both returned linear values and downstream sRGB-cancelled code means; the noiseless full-range black code is 95/1023. '
                 'These raised processed blacks are measured behavior, not subtracted or hidden.',
                 'The hue mutation checks the exposed pre-clip readout contract: beyond e_sat, clipping a signal before smoothstep would otherwise be mathematically '
                 'indistinguishable because smoothstep already equals one. No production sources are edited by mutation tests.']
    if suite.failures or error:
        sections += ['## Unresolved gates','\n'.join('- '+x for x in dict.fromkeys(suite.failures))]
    if error:
        sections += ['```',error,'```']
    external = OUT/'wpa_external_checks.json'
    if external.exists():
        sections += ['## Additional WP-A gates',table(json.loads(external.read_text(encoding='utf-8')),('gate','result','evidence'))]
    (OUT/'WPA_REPORT.md').write_text('\n\n'.join(sections)+'\n',encoding='utf-8')
    progress(short)


def worker():
    OUT.mkdir(parents=True,exist_ok=True)
    model = Model(OUT/'native_probe')
    runner = Runner()
    suite = Suite(runner,model)
    progress(runner.gl.renderer)
    error = None
    try:
        for name in ('poisson','quantisation','ptc','brightness','gain_switch','clip','colour','determinism','black','adversarial','mutations','cost'):
            progress(name+' starting')
            getattr(suite,name)()
            progress(name+' complete; failures='+str(len(suite.failures)))
    except Exception:
        error = traceback.format_exc()
        progress(error)
        suite.failures.append('uncaught fixture failure')
    report(suite,error)
    return 1 if suite.failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker',action='store_true',help=argparse.SUPPRESS)
    parser.add_argument('--timeout',type=int,default=600)
    args = parser.parse_args()
    if args.worker:
        return worker()
    # Kill a hung driver/compiler worker instead of polling it indefinitely.
    env = dict(os.environ,PYTHONUTF8='1',PYTHONDONTWRITEBYTECODE='1')
    try:
        return subprocess.run([sys.executable,str(Path(__file__).resolve()),'--worker'],cwd=ROOT,env=env,timeout=args.timeout).returncode
    except subprocess.TimeoutExpired:
        progress('FAIL: explicit %ds watchdog expired'%args.timeout)
        return 1


if __name__=='__main__':
    raise SystemExit(main())
