"""POV blink native timing + production GLSL render fixture (hidden WGL, no viewer build).

Outputs Dark/Warm 4K PNGs and GPU-only timing under .tmp/blink_20261102.
Run in a VS developer environment, or supply --native-compiler and its environment.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import subprocess

import numpy as np
from PIL import Image
import relight_3d_gl as G

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.tmp/blink_20261102'
NATIVE = r'''
#include "alpovblinkmath.h"
#include <cassert>
#include <iostream>
using namespace ALPOVBlinkMath;
int main() {
    Params p;
    State s; s.blink(0., p);
    double previous = 0.;
    for (int i=0;i<=90;++i) {
        double value=s.sample(i*.001);
        assert(value+1e-10 >= previous); previous=value;
    }
    assert(std::abs(s.sample(.09)-1.)<1e-9);
    assert(s.sample(.11)==1. && s.sample(.129)==1.);
    previous=1.;
    for (int i=130;i<=310;++i) {
        double value=s.sample(i*.001);
        assert(value<=previous+1e-10); previous=value;
    }
    assert(s.sample(.31)==0. && s.sample(2.)==0.);
    p.depth=.5; s.blink(3.,p); assert(std::abs(s.sample(3.1)-.5)<1e-9);
    s.hold(4.,true,p); assert(s.sample(100.)==.5);
    s.hold(100.,false,p); assert(s.sample(100.)==.5);
    assert(s.sample(100.09)>.24 && s.sample(100.09)<.26);
    assert(s.sample(100.181)==0.);
    // Releasing during closure is continuous, including immediate/zero durations.
    s.hold(101.,true,p); double a=s.sample(101.04);
    s.hold(101.04,false,p); assert(std::abs(s.sample(101.04)-a)<1e-10);
    State instant; instant.blink(0.,{0.,0.,0.,1.}); assert(instant.sample(0.)==0.);
    for (double rate : {1.,15.,60.}) {
        Scheduler scheduler;
        for (int i=0;i<10000;++i) {
            double d=scheduler.delay(rate);
            assert(d>=Scheduler::refractory);
            assert(d>=std::max(.65,60./rate*.55));
            assert(d<=std::max(.65,60./rate*1.45));
        }
        State automatic; double now=0., last_end=-100.; bool active=false; int blinks=0;
        for (int i=0;i<120000;++i) {
            now=i*.01;
            double v=automatic.update(now,Params{},true,rate);
            if(v>0. && !active) { assert(now-last_end>=.65); active=true; ++blinks; }
            if(v==0. && active) { active=false; last_end=now; }
        }
        assert(blinks>0);
    }
    assert(!shouldDraw(0.,true) && !shouldDraw(.5,false));
    assert(shouldDraw(.5,true) && shouldDraw(1.,true));
    PadGesture pad;
    assert(!pad.sample(0.,0,64,64,true,true));
    assert(!pad.sample(.01,0,64,64,false,true));
    assert(!pad.sample(.02,64,64,64,false,true) && !pad.held);
    assert(pad.sample(.1,0,64,64,false,true) && !pad.held); // short click
    assert(!pad.sample(.2,64,64,64,false,true));
    assert(!pad.sample(.4,64,64,64,false,true) && pad.held);
    assert(!pad.sample(.5,0,64,64,false,true) && !pad.held); // release long hold
    assert(!pad.sample(.6,64,64,64,true,true) && !pad.held); // focus/device prime
    assert(!pad.sample(.9,64,64,64,false,true) && !pad.held);
    pad.sample(1.,0,64,64,false,true);
    pad.sample(1.01,64,64,64,false,true);
    pad.sample(1.21,64,64,64,false,true); assert(pad.held);
    assert(!pad.sample(1.3,64,64,64,false,false) && !pad.held);
    pad.sample(2.,0,32,16,true,true); pad.sample(2.01,0,32,16,false,true);
    assert(pad.sample(2.02,32,32,16,false,true));
    assert(!pad.sample(2.03,16,32,16,false,true) && pad.held);
    assert(!pad.sample(2.04,0,32,16,false,true) && !pad.held);
    std::cout << "PASS native gamepad: shared L3 tap/hold, separate buttons, prime/focus release\n";
    std::cout << "PASS native: asymmetric quintic timings/monotonicity, partial depth, hold/release continuity, zero durations, randomized bounds/refractory, idle/capture skip\n";
}
'''


def native(compiler):
    source = OUT / 'pov_blink_native.cpp'
    source.write_text(NATIVE, encoding='utf-8')
    exe = OUT / 'pov_blink_native.exe'
    command = [compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
               '/I' + str(ROOT / 'indra/newview'), str(source),
               '/Fe:' + str(exe), '/Fo:' + str(OUT / 'pov_blink_native.obj')]
    subprocess.run(command, check=True, cwd=ROOT)
    subprocess.run([str(exe)], check=True, cwd=ROOT)
    # Use the existing fixture's parser doubles with the production migration
    # function. B represents the new camera key (the double accepts ASCII only).
    from test_key_binding_migration import build_source
    migration = build_source().split('int main() {', 1)[0] + r'''
int main() {
    using Mode=LLViewerInput::KeyMode;
    Mode defaults;
    defaults.bindings.add({"V","CTL_ALT","toggle_viewfinder"});
    defaults.bindings.add({"B","NONE","pov_eye_blink"});
    for(int version : {0,1,2,3}) {
        Mode user; migrateBindingMode(user,defaults,version,1);
        assert(user.bindings.size()==(version<2 ? 2u : version<3 ? 1u : 0u));
    }
    Mode occupied; occupied.bindings.add({"B","ALT","custom"});
    migrateBindingMode(occupied,defaults,2,1); assert(occupied.bindings.size()==1);
    Mode cleared; cleared.bindings.add({"","NONE","pov_eye_blink"});
    migrateBindingMode(cleared,defaults,2,1); assert(cleared.bindings.size()==1);
    Mode empty; migrateBindingMode(empty,defaults,0,0); assert(empty.bindings.empty());
    Mode user, additions; migrateBindingMode(user,defaults,2,1,&additions);
    migrateBindingMode(user,defaults,2,1); assert(user.bindings.size()==1 && additions.bindings.size()==1);
    assert(user.bindings[0].command.getValue()=="pov_eye_blink");
    std::cout << "PASS native keyboard migration: v2 adds blink, v3 preserves omissions, conflicts/custom/cleared bindings preserved, idempotent\n";
}
'''
    source.write_text(migration, encoding='utf-8')
    subprocess.run(command, check=True, cwd=ROOT)
    subprocess.run([str(exe)], check=True, cwd=ROOT)
    # Production admission must precede any shader bind/target allocation.
    cpp = (ROOT / 'indra/newview/alpovblink.cpp').read_text(encoding='utf-8')
    begin = cpp[cpp.index('LLRenderTarget* ALPOVBlink::beginPresentation'):cpp.index('void ALPOVBlink::endPresentation')]
    assert begin.index('shouldDraw') < begin.index('presentation.allocate') < begin.index('presentation.bindTarget')
    pipe = (ROOT / 'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    call = pipe[pipe.index('// POV eyelids intercept'):pipe.index('// Present the screen target.', pipe.index('// POV eyelids intercept'))]
    assert all(flag in call for flag in ('!gSnapshot', '!gCubeSnapshot', '!sPrismLensRender', '!sPrismPreviewRender', '!sReflectionRender', '!sImpostorRender', '!sRenderingHUDs', 'mRT == &mMainRT'))
    print('PASS production admission: idle skips all GL work; captures/auxiliary views excluded')


class Runner:
    def __init__(self):
        self.g = g = G.GL()
        pr = g.program()
        for file, stage in [('postDeferredNoTCV.glsl', 0x8B31), ('povBlinkF.glsl', 0x8B30)]:
            source = (ROOT / 'indra/newview/app_settings/shaders/class1/deferred' / file).read_text(encoding='utf-8')
            sh = g.create_shader(stage)
            raw = ('#version 420\n' + source).encode()
            text = C.c_char_p(raw)
            g.source(sh, 1, C.byref(text), None); g.compile(sh)
            ok = g.I(); g.getshader(sh, 0x8B81, C.byref(ok))
            if not ok.value:
                log = C.create_string_buffer(64000); g.slog(sh, len(log), None, log)
                raise AssertionError(log.value.decode())
            g.attach(pr, sh)
        g.link(pr)
        ok = g.I(); g.getprog(pr, 0x8B82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(64000); g.plog(pr, len(log), None, log)
            raise AssertionError(log.value.decode())
        self.pr = pr
        g.use(pr)
        self.uniform = lambda name, *values: g.uf[len(values)](g.loc(pr, name.encode()), *values)
        for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0): g.disable(cap)
        self.fbo, self.vbo = g.U(), g.U()
        g.gen_fbo(1, C.byref(self.fbo)); g.bind_fbo(0x8D40, self.fbo)
        self.depth_target = g.U()
        g.fn('glGenRenderbuffers',None,g.I,C.POINTER(g.U))(1,C.byref(self.depth_target))
        g.gen_buf(1, C.byref(self.vbo)); g.bind_buf(0x8892, self.vbo)
        verts = np.array([[-1.,-1.,0.], [3.,-1.,0.], [-1.,3.,0.]], np.float32)
        g.buffer_data(0x8892, verts.nbytes, verts.ctypes.data, 0x88E4)
        attr = g.attr_loc(pr, b'position'); g.enable_attr(attr); g.attr_ptr(attr, 3, 0x1406, 0, 12, None)
        self.textures = []

    def texture(self, pixels, width, height):
        g = self.g; tex = g.U(); g.gen_tex(1, C.byref(tex)); self.textures.append(tex)
        g.bind_tex(0x0DE1, tex)
        g.teximage(0x0DE1, 0, 0x8814, width, height, 0, 0x1908, 0x1406, None if pixels is None else pixels.ctypes.data)
        for key in (0x2800,0x2801): g.param(0x0DE1, key, 0x2601)
        for key in (0x2802,0x2803): g.param(0x0DE1, key, 0x812F)
        return tex

    def setup(self, width, height, patterned=False):
        g = self.g
        for tex in self.textures: g.del_tex(1, C.byref(tex))
        self.textures = []
        self.w, self.h = width, height
        y,x = np.mgrid[:height,:width]
        checker = ((x//max(1,width//30) + y//max(1,height//18))%2).astype(np.float32)
        pixels = np.ones((height,width,4),np.float32)
        if patterned:
            pixels[:,:,0] = .12+.6*checker
            pixels[:,:,1] = .15+.65*x/width
            pixels[:,:,2] = .12+.65*y/height
        self.pixels = pixels
        target = self.texture(None,width,height)
        g.fbo_tex(0x8D40,0x8CE0,0x0DE1,target,0)
        g.fn('glBindRenderbuffer',None,g.U,g.U)(0x8D41,self.depth_target)
        g.fn('glRenderbufferStorage',None,g.U,g.U,g.I,g.I)(0x8D41,0x81A6,width,height)
        g.fn('glFramebufferRenderbuffer',None,g.U,g.U,g.U,g.U)(0x8D40,0x8D00,0x8D41,self.depth_target)
        g.fn('glEnable',None,g.U)(0x0B71)
        g.fn('glDepthFunc',None,g.U)(0x0207)  # GL_ALWAYS, as final presentation
        assert g.fbo_status(0x8D40)==0x8CD5
        source = self.texture(pixels,width,height)
        depth = self.texture(np.full((1,1,4),.6,np.float32),1,1)
        g.active(0x84C0); g.bind_tex(0x0DE1,source); g.ui(g.loc(self.pr,b'diffuseMap'),0)
        g.active(0x84C1); g.bind_tex(0x0DE1,depth); g.ui(g.loc(self.pr,b'depthMap'),1)
        g.active(0x84C0)
        g.viewport(0,0,width,height)
        self.uniform('screen_res',float(width),float(height))

    def draw(self, closure, style, curve=.35, softness=.02, dark=.35, blur=.4, read=True):
        g = self.g
        self.uniform('blink_shape',closure,curve,softness,float(style))
        self.uniform('blink_look',dark,blur)
        g.draw(0x0004,0,3)
        if read:
            pixels=np.zeros((self.h,self.w,4),np.float32)
            g.read(0,0,self.w,self.h,0x1908,0x1406,pixels.ctypes.data)
            assert np.isfinite(pixels).all()
            depth=np.zeros((self.h,self.w),np.float32)
            g.read(0,0,self.w,self.h,0x1902,0x1406,depth.ctypes.data)
            assert np.max(np.abs(depth-.6))<1e-6, 'Final scene depth must survive eyelids'
            return pixels

    def timing(self, closure, style):
        g=self.g; q=g.U(); g.gen_queries(1,C.byref(q))
        for _ in range(5): self.draw(closure,style,read=False)
        g.finish(); g.begin_query(0x88BF,q)
        for _ in range(30): self.draw(closure,style,read=False)
        g.end_query(0x88BF); g.finish()
        elapsed=C.c_uint64(); g.query_u64(q,0x8866,C.byref(elapsed))
        return elapsed.value/30/1e6


def render():
    r = Runner(); g=r.g
    for w,h in ((384,216),(504,216),(256,256)):
        r.setup(w,h)
        for curve in (0.,.35,1.):
            for softness in (0.,.02,1.):
                closed=r.draw(1.,0,curve,softness,dark=0.,blur=0.)
                assert np.max(np.abs(closed[:,:,:3]-[.004,.003,.005]))<1e-6, (w,h,curve,softness)
                partial=r.draw(.5,0,curve,softness,dark=0.,blur=0.)
                assert partial[int(h*.46),w//2,:3].min()>.9, (w,h,curve,softness)
                assert partial[0,w//2,0]<.01 and partial[-1,w//2,0]<.01
        identity=r.draw(0.,0)
        assert np.max(np.abs(identity-r.pixels))<1e-5
    print('PASS production GLSL: complete closed coverage / centred partial gap / identity at 16:9, 21:9, 1:1; curve and feather extrema')
    r.setup(3840,2160,True)
    renders=OUT/'renders'; renders.mkdir(exist_ok=True)
    metrics={'renderer':g.renderer,'resolution':[3840,2160],'gpu_ms_per_pass':{}}
    for style,name in enumerate(('Dark','Warm')):
        for closure in (0.,.5,1.):
            pixels=r.draw(closure,style)
            if closure==0.: assert np.max(np.abs(pixels-r.pixels))<2e-4
            if closure==1. and style==1:
                assert pixels[:,:,0].mean()>.11 and pixels[:,:,0].mean()<.17
                assert pixels[:,:,0].std()>0.001, 'Warm should retain faint scene transmission'
            Image.fromarray((np.clip(pixels[::-1,:,:3],0,1)*255+.5).astype(np.uint8)).save(renders/f'{name.lower()}_{closure:g}.png')
            if closure>0.:
                metrics['gpu_ms_per_pass'][f'{name}_{closure:g}']=r.timing(closure,style)
    (OUT/'render_metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
    print(json.dumps(metrics,indent=2))
    print('PASS six 4K PNGs; production vertex/fragment compiled and rendered on driver')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-compiler',default='cl.exe')
    parser.add_argument('--gl-only',action='store_true')
    args=parser.parse_args(); OUT.mkdir(parents=True,exist_ok=True)
    if not args.gl_only: native(args.native_compiler)
    render()


if __name__=='__main__': main()
