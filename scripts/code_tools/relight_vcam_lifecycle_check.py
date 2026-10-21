"""Execute production camera removal, bucket ownership and scratch acquisition."""
from pathlib import Path
import os
import subprocess
from relight_area_policy_check import method


def check_lifecycle():
    nv = Path('indra/newview')
    prism = nv / 'llprismlens.cpp'
    fixture = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
using U32=unsigned; using S32=int; using LLUUID=int;
namespace LLPrismLens {
constexpr unsigned MAX_CAPTURES=8,MAX_LENSES=8;
struct CaptureHandle { int mId=0,mGeneration=0; };
}
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY
constexpr int GL_SRGB8_ALPHA8=1,GL_RGBA16F=2,GL_R8=3;
unsigned gFrameCount=1; bool RenderDeferredSSAO=false; int RenderShadowDetail=0;
struct LLRenderTarget {
    unsigned width=0,height=0,releases=0,allocations=0;
    bool isComplete() const { return width!=0; }
    unsigned getWidth() const { return width; }
    bool allocate(unsigned w,unsigned h,int,bool=false) { width=w;height=h;++allocations;return true; }
    void shareDepthBuffer(LLRenderTarget&) {}
    void release() { if(width) ++releases;width=height=0; }
};
struct LLPipeline {
    struct RenderTargetPack {
        LLRenderTarget areaGather[2],areaScratch[2],areaVisibility[2];
        LLRenderTarget actorMask,silCover,screen,deferredScreen,deferredLight;
        unsigned actorMaskDepthSrc=0,silCoverDepthSrc=0,width=0,height=0;
    };
    struct PrismLensScratch {
        RenderTargetPack rt; LLRenderTarget waterDis,waterExclusionMask;
        unsigned width=0,height=0,lastUsedFrame=0;
    } mPrismLensScratchPool[8];
    PrismLensScratch* mActivePrismLensScratch=nullptr;
    bool addDeferredAttachments(LLRenderTarget&) { return true; }
    bool acquirePrismLensScratch(U32,U32);
    void releaseUnusedPrismAreaTargets(const std::array<std::pair<U32,U32>,8>&);
    void releasePrismLensBuffers();
    void releasePrismPreviewScratch() {}
    void releasePrismLensOutput(U32) {}
    void releasePrismProjectorFeed(int) {}
} gPipeline;
struct Preview {
    static Preview& instance() { static Preview p;return p; }
    void setDemand(LLPrismLens::CaptureHandle,bool) {}
};
using LLPrismPreviewService=Preview;
struct PrismInstance {
    bool mOccupied=false; int kind=0;
    LLPrismLens::CaptureHandle mHandle;
    unsigned mAreaScratchWidth=0,mAreaScratchHeight=0;
    std::vector<int> mSources;
    bool isCameraFeed() const { return kind==0; }
    const PrismInstance* cameraData() const { return kind==0 || kind==3 ? this : nullptr; }
    bool isGate() const { return kind==2; }
    PrismInstance* gateData() { return this; }
};
struct PrismDisplay { bool mOccupied=false;unsigned mCaptureSlot=0; };
struct PrismProjector {
    bool mOccupied=false;unsigned mCaptureSlot=0;int mCaptureGeneration=0,mHandle=0;
};
struct Registry {
    std::array<PrismInstance,8> mLenses;
    std::array<PrismDisplay,8> mDisplays;
    std::array<PrismProjector,8> mProjectors;
    std::map<int,int> mGateGrants;
    int mActiveSlot=-1,mLastRenderedSlot=-1,mRevision=0,mRuntimeRevision=0;
    unsigned count() const { unsigned n=0;for(auto& c:mLenses) n+=c.mOccupied;return n; }
    void stopPrivate(U32) {} void syncMediaTracking() {} void reconcileGateAudio() {}
    void releaseProjectorVolumetrics(PrismProjector&) {} void resetRuntimeHistory() {}
    void retainAreaScratch(U32,U32,U32);void releaseUnusedAreaScratch();void clearSlot(U32);
};
'''
    fixture += '\n'.join(method(nv / 'pipeline.cpp', signature) for signature in (
        'static void release_prism_scratch_entry(',
        'bool LLPipeline::acquirePrismLensScratch(',
        'void LLPipeline::releaseUnusedPrismAreaTargets(',
        'void LLPipeline::releasePrismLensBuffers()'))
    for signature in ('void retainAreaScratch(', 'void releaseUnusedAreaScratch()', 'void clearSlot('):
        body = method(prism, signature)
        fixture += '\n' + body.replace('void ', 'void Registry::', 1)
    # Execute the production preview-claim guard as well as the registry methods.
    source = prism.read_text(encoding='utf-8')
    claim_end = source.index('            LLPipeline::sPrismLensRender = true;',
                             source.index('const bool scratch_ready = preview_only'))
    claim_start = source.rfind('            // Preview uses separate scratch;', 0, claim_end)
    assert claim_start >= 0
    fixture += '\nvoid retainAfterAcquire(Registry& registry, U32 slot, bool preview_only, '
    fixture += 'U32 render_width, U32 render_height) {\n' + source[claim_start:claim_end] + '}\n'
    fixture += r'''
int main() {
    Registry r;
    // A media-only survivor must never retain a removed camera's area targets.
    r.mLenses[0].mOccupied=r.mLenses[1].mOccupied=r.mLenses[2].mOccupied=true;
    r.mLenses[2].kind=1;
    auto acquire=[&](int slot,unsigned w,unsigned h) {
        assert(gPipeline.acquirePrismLensScratch(w,h));
        r.retainAreaScratch(slot,w,h);
        return gPipeline.mActivePrismLensScratch;
    };
    auto warm=[](auto* e) {
        for(auto* targets:{&e->rt.areaGather,&e->rt.areaScratch,&e->rt.areaVisibility})
            for(auto& t:*targets) t.allocate(e->width,e->height,1);
    };
    auto empty=[](auto* e) {
        for(auto* targets:{&e->rt.areaGather,&e->rt.areaScratch,&e->rt.areaVisibility})
            for(auto& t:*targets) if(t.isComplete()) return false;
        return true;
    };
    auto* first=acquire(0,1024,1024);warm(first);
    assert(acquire(1,1024,1024)==first);
    for(int i=0;i<8;++i) { ++gFrameCount;assert(acquire(0,1024,1024)==first); }
    assert(first->rt.areaVisibility[0].allocations==1 && first->rt.areaVisibility[0].releases==0);
    r.clearSlot(0);assert(!empty(first)); // same-size sibling still owns it
    r.clearSlot(1);assert(empty(first)); // media remains
    assert(first->rt.screen.isComplete()); // release area only, preserve base scratch
    r.mLenses[0].mOccupied=true;
    assert(acquire(0,1024,1024)==first);warm(first);
    auto* second=acquire(0,512,288);
    assert(second!=first && empty(first));warm(second);
    auto before=second->rt.areaVisibility[0].allocations;
    assert(acquire(0,512,288)==second && !empty(second));
    assert(second->rt.areaVisibility[0].allocations==before);
    r.clearSlot(0);assert(empty(second)); // media still remains
    assert(r.count()==1 && second->rt.screen.isComplete());
    // Surface lenses share the camera payload and auxiliary render path.
    r.mLenses[3].mOccupied=true;r.mLenses[3].kind=3;
    auto* surface=acquire(3,640,360);warm(surface);
    for(int i=0;i<8;++i) {
        ++gFrameCount;assert(acquire(3,640,360)==surface);
        assert(!empty(surface));
        assert(surface->rt.areaVisibility[0].allocations==1);
        assert(surface->rt.areaVisibility[0].releases==0);
    }
    // A preview-only render uses different scratch and must keep the real claim.
    retainAfterAcquire(r,3,true,256,144);
    assert(r.mLenses[3].mAreaScratchWidth==640 && r.mLenses[3].mAreaScratchHeight==360);
    assert(!empty(surface));
    r.mLenses[0].mOccupied=true;
    assert(acquire(0,640,360)==surface && !empty(surface));
    retainAfterAcquire(r,0,true,256,144);
    assert(r.mLenses[0].mAreaScratchWidth==640 && !empty(surface));
    r.clearSlot(0);assert(!empty(surface)); // surface survives camera removal
    r.mLenses[0].mOccupied=true;
    assert(acquire(0,640,360)==surface);
    r.clearSlot(3);assert(!empty(surface)); // camera survives surface removal
    r.clearSlot(0);assert(empty(surface)); // no claimant remains (media survives)
    assert(surface->rt.areaVisibility[0].allocations==1 && surface->rt.areaVisibility[0].releases==1);
    assert(surface->rt.screen.isComplete());
    r.clearSlot(2);assert(!second->rt.screen.isComplete()); // last consumer full release
    std::cout << "[RelightQ] VCam lifecycle PASS: production clearSlot/acquire, surface/camera shared ownership, preview preservation, media survivor, re-bucketing, steady reuse and last close\n";
}
'''
    directory = Path('.tmp/relight_cleanup_lifecycle').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'lifecycle.cpp').write_text(fixture, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob(
        '*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    (directory / 'compile.cmd').write_text(
        f'@echo off\ncall "{vcvars}" >nul\n'
        f'cl /nologo /std:c++20 /EHsc /Fo"{directory / "lifecycle.obj"}" '
        f'/Fe"{directory / "lifecycle.exe"}" "{directory / "lifecycle.cpp"}"\nexit /b %ERRORLEVEL%\n',
        encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(directory / 'compile.cmd')], check=True)
    subprocess.run([str(directory / 'lifecycle.exe')], check=True)
    mutated = fixture.replace('capture.mOccupied && capture.cameraData()',
                              'capture.mOccupied && capture.isCameraFeed()')
    assert mutated != fixture
    (directory / 'lifecycle.cpp').write_text(mutated, encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(directory / 'compile.cmd')], check=True)
    result = subprocess.run([str(directory / 'lifecycle.exe')], capture_output=True)
    assert result.returncode != 0, 'camera-feed-only ownership mutation survived'
    print('[RelightQ] VCam lifecycle mutation rejected: camera-feed-only claim filter', flush=True)


if __name__ == '__main__':
    check_lifecycle()
