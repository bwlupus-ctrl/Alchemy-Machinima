"""FIX2 regressions against a source ref, with native C++ fault injection and WGL.

No viewer build. Each numbered check fails on 48f3b6cf255. Production scope
rollback, tile policy, allocation budget and effect-size helpers are compiled
verbatim; GL executes production shaders. No acceptance thresholds are relaxed.
"""
import argparse
import ctypes as C
import json
import os
from pathlib import Path
import re
import subprocess

import numpy as np
import rim_normals_gl_check as R


def compile_run(source, out):
    out.mkdir(parents=True, exist_ok=True)
    cpp, exe, cmd = out/'native.cpp', out/'native.exe', out/'native.cmd'
    cpp.write_text(source, encoding='utf-8')
    vcvars=next((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    cmd.write_text('@echo off\ncall "%s" >nul 2>&1\ncl /nologo /std:c++20 /EHsc /W4 /WX "%s" /Fe:"%s" /Fo:"%s"\nif errorlevel 1 exit /b 1\n"%s"\nexit /b %%ERRORLEVEL%%\n' % (vcvars,cpp,exe,out/'native.obj',exe))
    p=subprocess.run(['cmd','/d','/c',str(cmd.resolve())],capture_output=True,text=True)
    assert p.returncode==0,p.stdout+p.stderr
    return p.stdout


STUBS=r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
using U32=unsigned; using U64=uint64_t; using S32=int; using F32=float;
#define LL_WARNS_ONCE(x) std::ostringstream()
#define LL_ENDL ""
constexpr int UI_CURSOR_ARROW=0;
bool gSnapshotNoPost=false,gDisplaySwapBuffers=true,gDepthDirty=false,gResizeScreenTexture=false;
struct LLRect { int value=7; };
struct LLImageDataLock {};
struct LLViewerCamera {
    int height=1080; float aspect=1.5f, tile=0;
    static LLViewerCamera& instance(){static LLViewerCamera c;return c;}
    int getViewHeightInPixels()const{return height;} float getAspect()const{return aspect;}
    void setViewHeightInPixels(int h){height=h;} void setAspect(float a){aspect=a;}
    void setSnapshotTile(float x,float,float,float){tile=x;}
};
struct LLRenderTarget {
    struct AllocationBudget { bool failed=false; };
    static inline LLRenderTarget* bound=nullptr;
    bool allocated=false;
    static LLRenderTarget* getCurrentBoundTarget(){return bound;}
    void flush(){assert(bound==this);bound=nullptr;}
    void release(){assert(bound!=this);allocated=false;}
    ~LLRenderTarget(){release();}
};
struct LLPipeline {
    static constexpr int RENDER_DEBUG_FEATURE_UI=1;
    static inline bool sShowHUDAttachments=true;
    struct Pack {U32 width=1920,height=1080;} pack,*mRT=&pack;
    bool ui=true,fail_restore=false; int releases=0;
    U32 mSnapshotCoreWidth=0,mSnapshotCoreHeight=0;
    bool hasRenderDebugFeatureMask(int)const{return ui;}
    static void toggleRenderDebugFeature(int);
    void releaseScreenBuffers(){++releases;}
    void invalidateSnapshotEffects(){}
    bool allocateScreenBuffer(U32 w,U32 h){assert(!LLRenderTarget::bound); pack={w,h};return !fail_restore;}
} gPipeline;
void LLPipeline::toggleRenderDebugFeature(int){gPipeline.ui=!gPipeline.ui;}
LLRect mWorldViewRectRaw;
void setCursor(int){} void setBalanceVisible(bool){}
'''


def native_checks(tree,out,require):
    window=tree.read('indra/newview/llviewerwindow.cpp')
    pipe=tree.read('indra/newview/pipeline.cpp')
    ph=tree.read('indra/newview/pipeline.h')
    rh=tree.read('indra/llrender/llrendertarget.h')
    rc=tree.read('indra/llrender/llrendertarget.cpp')
    raw=window[window.index('bool LLViewerWindow::rawSnapshot('):]
    present='struct SnapshotRollback' in raw
    if present:
        guard=raw[raw.index('    // Establish rollback BEFORE'):raw.index('    const bool rim_snapshot')]
        src=STUBS+'\nbool capture(int fault) {\n'+guard+r'''
        buffers_changed=true; scratch_space.allocated=true; LLRenderTarget::bound=&scratch_space;
        mWorldViewRectRaw.value=99; auto& c=LLViewerCamera::instance();
        c.height=4000;c.aspect=2;c.tile=3;
        gPipeline.pack={4000,4000};gPipeline.ui=false;LLPipeline::sShowHUDAttachments=false;
        gPipeline.mSnapshotCoreWidth=512;gPipeline.mSnapshotCoreHeight=512;
        gSnapshotNoPost=true;gDisplaySwapBuffers=false;
        if(fault==1)return false; // raw resize failure
        if(fault==2)return false; // raw buffer invalid
        if(fault==3)throw 17;
        if(fault==4)gPipeline.fail_restore=true;
        return rollback.finish();
}
int main(){
    for(int fault=0;fault<=4;++fault){
        gPipeline.fail_restore=false;gResizeScreenTexture=false;
        try{bool ok=capture(fault);assert(ok==(fault==0));}catch(int v){assert(fault==3&&v==17);}
        assert(!LLRenderTarget::bound&&mWorldViewRectRaw.value==7);
        assert(LLViewerCamera::instance().height==1080&&LLViewerCamera::instance().aspect==1.5f);
        assert(LLViewerCamera::instance().tile==0&&gPipeline.pack.width==1920&&gPipeline.pack.height==1080);
        assert(gPipeline.ui&&LLPipeline::sShowHUDAttachments&&!gSnapshotNoPost&&gDisplaySwapBuffers);
        assert(!gPipeline.mSnapshotCoreWidth&&!gPipeline.mSnapshotCoreHeight);
        assert(gResizeScreenTexture==(fault==4));
    }
}
'''
        compile_run(src,out/'rollback')
    require(present and raw.index('struct SnapshotRollback')<raw.index('gSnapshotNoPost = no_post'),
            '2: native early returns, exception, failed recovery restore dimensions, flags, tile and unbind-before-release')
    policy=tree.exists('indra/newview/alrimsnapshot.h') if tree.ref=='WORKTREE' else 'ALRimSnapshot::plan' in window
    shader_margin=int(re.search(r'constexpr S32 support = (\d+);',raw).group(1))//2
    if policy:
        header=tree.read('indra/newview/alrimsnapshot.h')
        src=header+r'''
#include <cassert>
#include <iostream>
int main(){
    assert(!ALRimSnapshot::plan(0,1080,2,1.f,1048).width);
    assert(!ALRimSnapshot::plan(1920,1080,1,1.e30f,1048).width);
    for(unsigned d: {1u,2u,3u,4u})for(int output:{3840,7680}){
        auto t=ALRimSnapshot::plan(1923,1087,d,1.f,1048);
        assert(t.width>0&&t.height>0&&t.width<=1923&&t.height<=1087);
        assert(t.internal_width%4==0&&t.internal_height%4==0);
        assert(t.margin/int(d)>=1048&&(t.margin/int(d))%4==0);
        assert((t.width+2*t.margin)/int(d)==t.internal_width+2*t.margin/int(d));
        assert(output/t.width>=1);
    }
    for(float scale:{.5f,.75f,1.25f,1.5f,2.f}){
        auto t=ALRimSnapshot::plan(1923,1087,1,scale,1048);
        assert(t.width&&t.internal_width%4==0&&t.internal_height%4==0);
        assert(t.margin*scale>=1048&&int(t.margin*scale)%4==0);
    }
    std::cout<<"INTERNAL_MARGIN "<<ALRimSnapshot::plan(1920,1080,2,1.f,1048).margin/2<<"\n";
}
'''
        src='#include <initializer_list>\n'+src
        output=compile_run(src,out/'policy')
        shader_margin=int(re.search(r'INTERNAL_MARGIN (\d+)',output).group(1))
    require(policy,'3: internal margin >=1048 shader pixels and grid phase aligned at divisors 1/2/3/4 and fractional multipliers')
    effect='F32 effectHeight(' in ph and 'width_value*(effectHeight(h)/1080.f)' in pipe
    if effect:
        begin=ph.index('    F32 effectHeight(');end=ph.index('    F32 effectWidth(',begin)
        src='#include <cassert>\nusing F32=float;using U32=unsigned;\nstruct Pipe {\nstruct Target {float getHeight()const{return 3176;}};struct Pack{Target deferredScreen;} mMainRT,other,*mRT=&mMainRT;\nU32 mSnapshotCoreHeight=1080;\n'+ph[begin:end]+r'''
};
int main(){Pipe p;assert(p.effectHeight(3176)==1080);
assert(p.effectHeight(1588)==540);p.mRT=&p.other;assert(p.effectHeight(3176)==3176);
p.mRT=&p.mMainRT;p.mSnapshotCoreHeight=0;assert(p.effectHeight(3176)==3176);}
'''
        compile_run(src,out/'effect')
        for token in ('effectHeight(rim_h)','effectHeight(static_cast<F32>(height))','effectHeight((F32)dst->getHeight())','const F32 px_scale = effectHeight(h)'):
            assert token in pipe,token
    require(effect,'4: pixel rim width 2.94074x -> 1x; debug/wrap, Roto/Tron, DoF, area filter use core sizes')
    bounded='struct AllocationBudget' in rh and 'overlap_cap = 1024ull * 1024 * 1024' in raw
    if bounded:
        budget=rh[rh.index('    struct AllocationBudget'):rh.index('    static AllocationBudget* sAllocationBudget;')]
        start=rc.index('LLRenderTarget::AllocationBudget* LLRenderTarget::sAllocationBudget')
        end=rc.index('\n}',rc.index('bool LLRenderTarget::reserveAllocation',start))+2
        source='#include <cassert>\n#include <cstdint>\n#include <vector>\nusing U64=uint64_t;\nstruct LLRenderTarget{\n'+budget+'\nstatic AllocationBudget* sAllocationBudget;static bool reserveAllocation(U64);void trackAllocation();AllocationBudget* mAllocationBudget=nullptr;void release(){sAllocationBudget->targets.pop_back();++releases;}static inline int releases=0;};\n'+rc[start:end]+r'''
int main(){for(U64 output:{3840ull*2160,7680ull*4320}){
    (void)output;LLRenderTarget lazy;{LLRenderTarget::AllocationBudget b(1024ull*1024*1024);
    assert(LLRenderTarget::reserveAllocation(800ull*1024*1024));
    assert(!LLRenderTarget::reserveAllocation(300ull*1024*1024));assert(b.failed);
    assert(b.remaining==224ull*1024*1024);
    lazy.trackAllocation();assert(b.targets.size()==1);
    {LLRenderTarget::AllocationBudget inner(1);assert(!LLRenderTarget::reserveAllocation(2));}
    assert(LLRenderTarget::sAllocationBudget==&b);}
    assert(!LLRenderTarget::sAllocationBudget);assert(LLRenderTarget::reserveAllocation(1ull<<40));}
    assert(LLRenderTarget::releases==2);}
'''
        compile_run('#include <initializer_list>\n'+source,out/'budget')
        assert rc.count('sAllocationBudget != nullptr')==5
        assert 'rim_budget && rim_budget->failed' in raw and 'image_lock.reset();' in raw
        assert 'actual_width != U64(render_width) * tile.internal_width / tile.width' in raw
        assert raw.index('if (buffers_changed) gPipeline.releaseScreenBuffers();')<raw.index('rim_budget.reset();')
    require(bounded,'memory: cumulative 1024 MiB at 4K/8K, no release refunds, scoped immediate deletion and clean fallback')
    return shader_margin


def seed_pixels(runner,f,program):
    runner.frame(f,smoothness=0)
    runner._begin(program)
    for name,key,unit in (('depthMap','depth',0),('normalMap','norm',1),('rigRimAuxMap','raw',2)):
        runner.sampler(program,name,runner.textures[key][0],unit)
    runner.uniform(program,'screen_res',f['w'],f['h'])
    runner.uniform(program,'rig_rim_aux_params',0,.002,.02,0)
    runner.uniform(program,'rig_rim_quality',0,0,0,0)
    runner._mat(program,b'inv_proj',np.linalg.inv(f['proj']))
    w,h=(f['w']+1)//2,(f['h']+1)//2
    t=runner._texture('zero_seed',6,0x881A,w,h,0x1908,0x1406,None)
    runner.target(t,w,h);runner.gl.draw(4,0,3)
    return runner.pixels(w,h)


def gl_checks(tree,out,require,metrics,shader_margin):
    runner=R.Runner(tree)
    print('GL',runner.gl.renderer,flush=True)
    # RenderResolutionDivisor=2: distant shading support lies beyond the old
    # 524-pixel halo. Execute the same expanded camera projection and crop on
    # real GL, using the margin returned by the compiled production policy.
    f=R.join_fixture(3072,32,degrees=0,mapped=False)
    f['proj'][0,0]=1.8
    f['depth'][:]=(-f['proj'][2,2]+f['proj'][2,3])*.5+.5
    shading=np.zeros((32,3072,3));shading[...,2]=1;shading[:,:836,0]=.8
    f['normal']=R.encode(R.unit(shading))
    class WideRadius:
        def frame(self,fixture):return runner.frame(fixture,radius=50)
    whole,_,_=runner.frame(f,radius=50)
    assembled=R.tiled_snapshot(WideRadius(),f,shader_margin)
    seam=float(np.max(R.angle(whole[:,1536:1544],assembled[:,1536:1544])))
    metrics['divisor2_internal_margin']=shader_margin
    metrics['divisor2_seam_deg']=seam
    require(seam<.03,f'3: divisor 2 wide-support GL seam {seam:.6f} deg < .03, halo {shader_margin} shader pixels')
    metrics['barriers']={}
    for w,h in ((384,384),(1920,1080)):
        for barrier in ('depth','actor','subject'):
            f=R.fixture(w,h,mapped='curved')
            foreground=f['ids']>=0
            if barrier in ('actor','subject'):
                foreground &= np.indices((h,w))[1]<w//2
                f['identity']=np.where(foreground,1,2).astype(np.uint8)
            if barrier=='subject':
                f.update(subject_source=2,certify_identity=False)
            a,_,_=runner.frame(f,detail=1)
            normal=f['normal'].copy()
            normal[~foreground]=R.encode(np.broadcast_to(R.unit(np.array([.94,-.2,.25])),(h,w,3)))[~foreground]
            f['normal']=normal
            b,_,_=runner.frame(f,detail=1)
            error=float(np.max(R.angle(a[foreground],b[foreground])))
            metrics['barriers'][f'{h}/{barrier}']=error
            require(error<.03,f'1: {h}p {barrier}: other-side normals do not change foreground, Texture=1 ({error:.6f} deg < .03)')
    old=R.Runner(R.G.SourceTree(R.BASE),runner.gl)
    zero=runner.gl.loc(runner.seed,b'rig_rim_quality')<0
    equal=True
    for f in (R.fixture(),R.plane_fixture(383,385,0,2.5,1)):
        equal &= np.array_equal(seed_pixels(runner,f,runner.seed),seed_pixels(old,f,old.seed))
    host=tree.read('indra/newview/pipeline.cpp')
    require(zero and equal and 'smooth_normals ? gRigRimSmoothSeedProgram : gRigRimSilSeedProgram' in host,
            '5: zero seed has no active quality uniform and equals baseline fp16 bytes; positive seed has its own program')
    metrics['zero_seed_bytes_equal']=bool(equal)
    metrics['zero_seed_no_quality_uniform']=bool(zero)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--shader-ref',default='WORKTREE')
    p.add_argument('--out',type=Path,default=R.G.REPO/'.tmp/rim_normals/fix2')
    args=p.parse_args();args.out=args.out.resolve();args.out.mkdir(parents=True,exist_ok=True)
    tree=R.G.SourceTree(args.shader_ref); failures=[];metrics={'ref':args.shader_ref}
    def require(ok,message):
        print(('PASS: ' if ok else 'FAIL: ')+message,flush=True)
        if not ok:failures.append(message)
    shader_margin=native_checks(tree,args.out,require)
    gl_checks(tree,args.out,require,metrics,shader_margin)
    metrics['failures']=failures
    (args.out/'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf-8')
    return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
