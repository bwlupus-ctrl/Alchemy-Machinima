"""Real-WGL zero-cone startup regression; no viewer build or login required.

Compiles production LLRenderTarget bind/flush bodies and source-extracted shaft
clear/batch/state operations into a DLL running on the existing WGL context.
Exercises the target scopes reached with no eligible cones; allocation and
viewer/shader APIs outside these scopes are omitted. No environment, camera,
exposure or opted-in light is needed to trigger this bug. The release-build
negative control is HEAD 0de20f6fb5e.

python scripts/code_tools/relight_volumetric_login_check.py [--ref WORKTREE]
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import subprocess

import numpy as np

import relight_3d_gl as G
from relight_volumetric_engine_check import function

BAD = '0de20f6fb5e'
PIPE = 'indra/newview/pipeline.cpp'


def zero_cone_body(src, engine):
    """Keep production statements and branch conditions, not a port of the stack."""
    body = function(src, 'renderProjectorVolumetric'+engine)
    start = body.index('    march_target->bindTarget();')
    end = body.index('    // [Phase 1 item 3] union', start)
    initial = body[start:end]
    finish = body[body.rindex('    gGL.setColorMask(true, true);'):body.rindex('\n}')]
    if engine == 'New':
        start = body.index('    for (U32 program_index =')
        end = body.index('        bindDeferredShader(shaft_shader)', start)
        batch = body[start:end]
        # The reference itself is a shader API boundary, with no GL operation.
        a = batch.index('        LLGLSLShader& shaft_shader =')
        b = batch.index('gDeferredProjectorVolumetricFullProgram;', a)+len('gDeferredProjectorVolumetricFullProgram;\n')
        batch = batch[:a]+batch[b:]
        start = body.index('        // Restore full-screen scissor', end)
        end = body.index('        march_target->flush();', start)+len('        march_target->flush();')
        return initial+batch+body[start:end]+'\n    }\n'+finish
    start = body.index('    // Restore full-screen scissor', end)
    end = body.index('    if (dust_on)', start)
    flush = body.index('    march_target->flush();', end)
    return initial+body[start:end]+body[flush:flush+len('    march_target->flush();\n')]+finish


STUB = r'''
#include <windows.h>
#include <GL/gl.h>
#include <vector>
#include <set>
using U32=unsigned; using U64=unsigned long long; using F32=float;
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_COLOR_ATTACHMENT3 0x8CE3
#define LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE(x)
#define LL_PROFILE_ZONE_NUM(x)
#define RT_PROFILE_GPU_ZONE(x)
#define LL_PROFILE_GPU_ZONE(x)
// Record assertions, but continue like the affected release build.
int violations=0;
#define llassert(x) do { if (!(x)) ++violations; } while (0)
auto glBindFramebuffer=(void(APIENTRY*)(GLenum,GLuint))wglGetProcAddress("glBindFramebuffer");
auto glDrawBuffers=(void(APIENTRY*)(GLsizei,const GLenum*))wglGetProcAddress("glDrawBuffers");
auto glGenerateMipmap=(void(APIENTRY*)(GLenum))wglGetProcAddress("glGenerateMipmap");
void check_framebuffer_status() {}
namespace LLTexUnit { enum { TMG_AUTO=1, TFO_TRILINEAR=1 }; }
namespace LLRender { enum { BT_ADD, BT_ALPHA }; }
GLint gGLViewport[4]={0,0,16,16};
struct Render {
    void flush(){glFlush();}
    void setColorMask(bool rgb,bool alpha){glColorMask(rgb,rgb,rgb,alpha);}
    void setSceneBlendType(int type){glBlendFunc(type==LLRender::BT_ADD?GL_ONE:GL_SRC_ALPHA,
                                              type==LLRender::BT_ADD?GL_ONE:GL_ONE_MINUS_SRC_ALPHA);}
} gGL;
struct State {
    GLenum cap; bool previous;
    State(GLenum c,bool on):cap(c),previous(c && glIsEnabled(c)){
        if(cap){if(on)glEnable(cap);else glDisable(cap);}
    }
    ~State(){if(cap){if(previous)glEnable(cap);else glDisable(cap);}}
};
struct LLGLDisable:State {LLGLDisable(GLenum c):State(c,false){}};
struct LLGLEnable:State {LLGLEnable(GLenum c):State(c,true){}};
struct LLGLDepthTest:State {LLGLDepthTest(bool on):State(GL_DEPTH_TEST,on){}};
struct LLRenderTarget {
    static U32 sCurFBO,sCurResX,sCurResY;
    static LLRenderTarget* sBoundTarget;
    U32 mFBO,mResX,mResY;
    std::vector<U32> mTex;
    LLRenderTarget* mPreviousRT=nullptr;
    int mGenerateMipMaps=0;
    LLRenderTarget(U32 f,U32 w,U32 h,U32 n):mFBO(f),mResX(w),mResY(h),mTex(n){}
    U32 getWidth(){return mResX;} U32 getHeight(){return mResY;}
    void bindTexture(int,int,int){}
    void clear(U32 bits){glClear(bits);}
    bool isBoundInStack(){
        std::set<LLRenderTarget*> seen;
        for(auto p=sBoundTarget;p && seen.insert(p).second;p=p->mPreviousRT)
            if(p==this)return true;
        return false;
    }
    void bindTarget(); void flush();
};
U32 LLRenderTarget::sCurFBO=0,LLRenderTarget::sCurResX=16,LLRenderTarget::sCurResY=16;
LLRenderTarget* LLRenderTarget::sBoundTarget=nullptr;
'''


def compile_fixture(out, tree):
    rt = tree.read('indra/llrender/llrendertarget.cpp')
    def rt_body(name, next_name):
        return rt[rt.index('void LLRenderTarget::'+name+'()'):rt.index(next_name, rt.index('void LLRenderTarget::'+name+'()'))]
    source = STUB+rt_body('bindTarget', 'void LLRenderTarget::clear(')
    source += rt_body('flush', '\n#undef RT_PROFILE_GPU_ZONE')
    for engine in ('New', 'Classic'):
        source += '\nvoid '+engine+'(LLRenderTarget* march_target,bool halfres,bool temporal,bool budget_variant,bool scissor) {\n'
        source += 'bool BDMergeProjectorVolumetricsScissor=scissor, BDMergeProjectorVolumetricsTemporalBeamDepth=true;\n'
        source += zero_cone_body(tree.read(PIPE), engine)+'\n}\n'
    source += r'''
extern "C" __declspec(dllexport) int run(int engine,int halfres,int temporal,int budget,int scissor,
                                        unsigned scratch,unsigned screen,unsigned parent,float* pixel) {
    LLRenderTarget::sBoundTarget=nullptr; LLRenderTarget::sCurFBO=0; violations=0;
    glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,16,16);
    LLRenderTarget half(scratch,8,8,engine==0?2:1),scene(screen,16,16,2),outer(parent,16,16,2);
    if(parent){outer.bindTarget();glClearColor(.125f,.25f,.375f,1.f);glClear(GL_COLOR_BUFFER_BIT);}
    if(engine==0)New(halfres?&half:&scene,halfres,temporal,budget,scissor);
    else Classic(halfres?&half:&scene,halfres,temporal,budget,scissor);
    int result=violations;
    if(LLRenderTarget::sBoundTarget != (parent?&outer:nullptr))result+=100;
    if(parent){
        // A real offscreen caller stands in for present/UI: hidden WGL windows
        // do not have a readable back buffer on every Windows driver.
        glClearColor(.5f,.75f,.25f,1.f);glClear(GL_COLOR_BUFFER_BIT);
        GLint destination=0;glGetIntegerv(0x8CA6,&destination);
        glBindFramebuffer(GL_FRAMEBUFFER,parent);glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,pixel);
        glBindFramebuffer(GL_FRAMEBUFFER,destination);
    }
    if(parent)outer.flush();
    if(LLRenderTarget::sBoundTarget)result+=1000;
    // The negative control can leave pointers to locals. Reset logical state
    // only AFTER recording the leak; keep actual GL state for Python to inspect.
    LLRenderTarget::sBoundTarget=nullptr;
    return result;
}
'''
    out.mkdir(parents=True, exist_ok=True)
    src = out/'login_fixture.cpp'; src.write_text(source, encoding='utf-8')
    vswhere = Path(os.environ['ProgramFiles(x86)'])/'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(vswhere), '-latest', '-property', 'installationPath'], text=True, timeout=20).strip()
    cmd = out/'build.cmd'
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc /LD "%s" /link opengl32.lib /OUT:"%s"\n' %
                   (Path(vs)/'VC/Auxiliary/Build/vcvars64.bat',src,out/'login_fixture.dll'))
    result = subprocess.run([str(cmd)],cwd=out,capture_output=True,text=True,shell=True,timeout=90)
    assert result.returncode == 0, result.stdout+result.stderr
    dll = C.CDLL(str(out/'login_fixture.dll'))
    dll.run.argtypes = [C.c_int]*5+[C.c_uint]*3+[C.POINTER(C.c_float)]
    dll.run.restype = C.c_int
    return dll


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ref', default='WORKTREE')
    ap.add_argument('--out', type=Path, default=G.REPO/'.tmp/shaft_login/regression')
    args = ap.parse_args(); args.out = args.out.resolve()
    gl = G.GL(); assert 'RTX 5090' in gl.renderer, gl.renderer
    # Real RGBA16F scene/half/parent FBOs, including the New MRT sidecar.
    targets=[]
    for size in (8,16,16):
        fbo=gl.U();gl.gen_fbo(1,C.byref(fbo));gl.bind_fbo(0x8D40,fbo)
        for attachment in range(2):
            tex=gl.U();gl.gen_tex(1,C.byref(tex));gl.bind_tex(0x0DE1,tex)
            termination=size==8 and attachment==1
            gl.teximage(0x0DE1,0,0x822E if termination else 0x881A,size,size,0,
                        0x1903 if termination else 0x1908,0x1406,None)
            gl.fbo_tex(0x8D40,0x8CE0+attachment,0x0DE1,tex,0)
        assert gl.fbo_status(0x8D40)==0x8CD5
        targets.append(fbo.value)
    get = gl.fn('glGetIntegerv',None,gl.U,C.POINTER(gl.I))
    boolean = gl.fn('glGetBooleanv',None,gl.U,C.POINTER(C.c_ubyte))
    enabled = gl.fn('glIsEnabled',C.c_ubyte,gl.U)
    error = gl.fn('glGetError',gl.U)
    def ints(name,n=1):
        v=(gl.I*n)();get(name,v);return list(v)
    def state():
        mask=(C.c_ubyte*4)();boolean(0x0C23,mask)
        return dict(fbo=ints(0x8CA6),read_fbo=ints(0x8CAA),viewport=ints(0x0BA2,4),
                    draw=ints(0x0C01),read=ints(0x0C02),mask=list(mask),
                    blend=int(enabled(0x0BE2)),depth=int(enabled(0x0B71)),scissor=int(enabled(0x0C11)),
                    program=ints(0x8B8D),blend_src=ints(0x80C9),blend_dst=ints(0x80C8))
    report=dict(renderer=gl.renderer,ref=args.ref,checks=[])
    for label,ref in (('under_test',args.ref),('negative_control',BAD)):
        dll=compile_fixture(args.out/label,G.SourceTree(ref));rows=[]
        for engine in (0,1):
            for half in (0,1):
                for temporal in (0,1):
                    for parent in (0,targets[2]):
                        for budget in (0,1):
                            for scissor in (0,1):
                                # Repeated startup frames, no scene/environment or
                                # light input. A visible marker models present/UI.
                                for frame in range(3):
                                    gl.bind_fbo(0x8D40,0);gl.viewport(0,0,16,16)
                                    for cap in (0x0BE2,0x0B71,0x0C11):gl.disable(cap)
                                    gl.color_mask(1,1,1,1)
                                    gl.fn('glBlendFunc',None,gl.U,gl.U)(0x0302,0x0303)
                                    before=state();pixel=np.zeros(4,np.float32)
                                    violations=dll.run(engine,half,temporal,budget,scissor,*targets[:2],parent,
                                                       pixel.ctypes.data_as(C.POINTER(C.c_float)))
                                    after=state()
                                    visible=bool(np.allclose(pixel[:3],[.5,.75,.25],atol=.01)) if parent else None
                                    rows.append(dict(engine=engine,half=half,temporal=temporal,parent=bool(parent),
                                                     budget=budget,scissor=scissor,frame=frame,violations=violations,
                                                     state_restored=before==after,ui_visible=visible,
                                                     finite=bool(np.isfinite(pixel).all()) if parent else None,
                                                     error=int(error()),after=after if before!=after else None))
        failures=[r for r in rows if r['violations'] or not r['state_restored'] or r['ui_visible'] is False or r['finite'] is False or r['error']]
        passed=not failures if label=='under_test' else len(failures)==len(rows)//2 and all(r['engine']==0 for r in failures)
        report['checks'].append(dict(name=label,ref=ref,passed=passed,cases=len(rows),failures=len(failures),examples=failures[:2]))
        print(label,'PASS' if passed else 'FAIL',f'{len(failures)}/{len(rows)} failing cases',flush=True)
    report['passed']=all(c['passed'] for c in report['checks'])
    (args.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    return 0 if report['passed'] else 1


if __name__=='__main__':
    raise SystemExit(main())
