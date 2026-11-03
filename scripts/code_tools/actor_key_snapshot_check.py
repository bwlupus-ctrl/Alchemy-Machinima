"""Execute complete production rawSnapshot with instrumented viewer seams and WGL.

The real snapshot allocator/tile/readback/cleanup body and key completion methods
execute. display() supplies a synthetic scene using the production key shader.
This is a native integration fixture, not a running-viewer acceptance capture.
"""
from pathlib import Path
import ctypes as C
import subprocess

from native_check_tools import compile_run
from relight_area_policy_check import method

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
OUT = ROOT / '.tmp/ckey_20261102/wp2/snapshot'


def native(refuse_reshade=False):
    OUT.mkdir(parents=True, exist_ok=True)
    source = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using U8=unsigned char;using U32=unsigned;using U64=unsigned long long;
using S32=int;using F32=float;using GLfloat=float;
using Callback=int(*)(int,int,int,int,int,void*);Callback callback;
constexpr int GL_DEPTH_BUFFER_BIT=1,GL_COLOR_BUFFER_BIT=2,GL_DEPTH_COMPONENT=3,
 GL_RGBA=4,GL_RGB=5,GL_UNSIGNED_BYTE=6,GL_FLOAT=7,UI_CURSOR_WAIT=1,UI_CURSOR_ARROW=0;
#define LL_WARNS(...) std::cerr
#define LL_ENDL '\n'
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_WIN32
#define LL_PROFILE_ZONE_NAMED_CATEGORY_WIN32(x)
#define LL_PROFILER_GPU_COLLECT
template<class T>T llmin(T a,T b){return std::min(a,b);}
template<class T>T llmax(T a,T b){return std::max(a,b);}
template<class T>T llclamp(T a,T b,T c){return std::clamp(a,b,c);}
int llfloor(float a){return int(std::floor(a));}int llceil(float a){return int(std::ceil(a));}
int llabs(int a){return std::abs(a);}U32 F32_to_U32(float,float,float){return 0;}
U8 F32_to_U8(float,float,float){return 0;}
struct LLRect{int w=1920,h=1080;int getWidth()const{return w;}int getHeight()const{return h;}
 void set(int,int top,int right,int){w=right;h=top;}};
template<class T>using LLPointer=std::shared_ptr<T>;
struct LLImageRaw{int w=0,h=0,c=3;std::vector<U8>data;
 LLImageRaw()=default;LLImageRaw(int W,int H,int C){resize(W,H,C);}
 void resize(int W,int H,int C){w=W;h=H;c=C;data.resize(size_t(w)*h*c);}
 bool isBufferInvalid(){return data.empty();}int getWidth(){return w;}int getHeight(){return h;}
 int getComponents(){return c;}U8*getData(){return data.data();}
 bool scale(int W,int H,bool=true){assert(W==w&&H==h);return true;}
};
struct LLImageDataLock{explicit LLImageDataLock(LLImageRaw*){}};
struct LLMemory{static bool tryToAlloc(void*,size_t){return true;}};
struct LLSnapshotModel{enum ESnapshotLayerType{SNAPSHOT_TYPE_COLOR,SNAPSHOT_TYPE_DEPTH,SNAPSHOT_TYPE_DEPTH24};};
struct LLRenderTarget{int w=1920,h=1080;
 bool allocate(int W,int H,int,bool){w=W;h=H;return callback(0,w,h,0,0,nullptr)!=0;}
 void bindTarget(){callback(1,w,h,0,0,nullptr);}void flush(){callback(4,0,0,0,0,nullptr);}
 void release(){}int getWidth(){return w;}int getHeight(){return h;}
};
struct LLPipeline{inline static bool sRenderDeferred=true,sShowHUDAttachments=true,ui=true;
 enum{RENDER_DEBUG_FEATURE_UI};struct Pack{LLRenderTarget deferredScreen;}main;Pack*mRT=&main;
 bool hasRenderDebugFeatureMask(int){return ui;}static void toggleRenderDebugFeature(int){ui=!ui;}
 bool allocateScreenBuffer(int w,int h){main.deferredScreen.w=w;main.deferredScreen.h=h;return true;}
 void resetDrawOrders(){}
}gPipeline;
struct Camera{static Camera*getInstance(){static Camera c;return &c;}static Camera&instance(){return *getInstance();}
 void setViewHeightInPixels(int){}void setAspect(float){}float getFar(){return 512;}float getNear(){return .1f;}
};using LLViewerCamera=Camera;
struct LLAppViewer{static LLAppViewer*instance(){static LLAppViewer a;return &a;}void pingMainloopTimeout(const char*){}};
struct LLRender{inline static bool sNsightDebugSupport=false;};
struct Manager{int mGLMaxTextureSize=16384;}gGLManager;
int scratchReleases=0;struct LLActorMover{static void releaseStudioSnapshotScratch(){++scratchReleases;}};
bool gSnapshotNoPost=false,gDisplaySwapBuffers=false,gDepthDirty=false;
void glClear(int){}void stop_glerror(){}void send_agent_resume(){}
void glReadPixels(int x,int y,int w,int h,int,int,void*p){callback(3,x,y,w,h,p);}
struct LLWindow{inline static std::atomic<bool>sPresentBlocked=false;inline static std::atomic<U64>sBlockedPresents=0;};
int presents=0;void SwapBuffers(int){++presents;}
struct LLWindowWin32:LLWindow{int mhDC=0;void swapBuffers();}window;
U32 gFrameCount=42;
bool reshadePresent=true,effectsEnabled=true;
struct LLReShadeBridge{static LLReShadeBridge& instance(){static LLReShadeBridge b;return b;}
 bool keyGuardAllows(U32=0){return !reshadePresent;} };
namespace {
U64 sFinalizeSerial=0;U32 sKeyFrame=~0u;S32 sKeyWidth=0,sKeyHeight=0;
bool sComplete=false,sStudioFailed=false;std::string sKeyReason="no keyed finalize";
}
struct ALActorChromaKey{inline static bool sActive=true;
 static U64 finalizeSerial();static void beginFinalize();static void provision(S32,S32);
 static bool keyedThisFrame();static void refuse(const char*);static const std::string&reason();
 static void finishStudioFrame(U64);static bool snapshotComplete(U64,S32,S32);
 static void studioFailure(const char*why){sStudioFailed=true;sKeyReason=why;}
 static bool snapshotRequestAllowed(int,int){return true;}
 static void snapshotRefused(int,int){}
 struct SnapshotScope{bool mActive,mPrevious;SnapshotScope();~SnapshotScope();};
};
struct LLViewerWindow{LLRect mWorldViewRectRaw;
 void setCursor(int){}void setBalanceVisible(bool){}void updateWorldViewRect(bool){}
 LLRect getWindowRectRaw(){return {};}LLRect getWorldViewRectRaw(){return mWorldViewRectRaw;}
 float getWorldViewAspectRatio(){return float(mWorldViewRectRaw.w)/mWorldViewRectRaw.h;}
 bool rawSnapshot(LLImageRaw*,S32,S32,bool,bool,bool,bool,bool,bool,bool,LLSnapshotModel::ESnapshotLayerType,S32);
}viewer;
int injected=0,displayCalls=0,refusedReads=0;
void swap(){window.swapBuffers();}
void render_ui(float,int){}
void display(bool,float,int,bool){++displayCalls;window.swapBuffers();window.swapBuffers();
 using K=ALActorChromaKey;const auto&v=viewer.mWorldViewRectRaw;
 assert(!LLPipeline::ui&&!LLPipeline::sShowHUDAttachments);
 // The real display scene seam is replaced with the production GLSL driver.
 callback(2,v.w,v.h,injected,0,nullptr);
 if(injected==7)return; // resize/start-up/disconnected early display exit
 K::beginFinalize();
 if(injected>=1&&injected<=5){K::refuse("injected K0 refusal");return;}
 K::provision(v.w,v.h);
 if(injected==6)K::studioFailure("injected post-K2 Glass failure");
 K::finishStudioFrame(K::finalizeSerial());
}
'''
    # Keep the complete production function, including all depth branches. The
    # LLPointer seam above models the viewer's implicit intrusive-pointer ctor.
    source = source.replace('template<class T>using LLPointer=std::shared_ptr<T>;', r'''
template<class T>struct LLPointer:std::shared_ptr<T>{LLPointer(T*p):std::shared_ptr<T>(p){}};
''')
    for sig in ('U64 ALActorChromaKey::finalizeSerial()', 'void ALActorChromaKey::beginFinalize()',
                'void ALActorChromaKey::provision(', 'bool ALActorChromaKey::keyedThisFrame()',
                'void ALActorChromaKey::refuse(', 'const std::string& ALActorChromaKey::reason()',
                'void ALActorChromaKey::finishStudioFrame(', 'bool ALActorChromaKey::snapshotComplete(',
                'ALActorChromaKey::SnapshotScope::SnapshotScope()',
                'ALActorChromaKey::SnapshotScope::~SnapshotScope()'):
        source += '\n' + method(NV / 'alactorchromakey.cpp', sig)
    source += '\n' + method(ROOT / 'indra/llwindow/llwindowwin32.cpp', 'void LLWindowWin32::swapBuffers()')
    raw = method(NV / 'llviewerwindow.cpp', 'bool LLViewerWindow::rawSnapshot(')
    if refuse_reshade:
        assert raw.count('bool key_refused = false;') == 1
        raw = raw.replace('bool key_refused = false;', 'bool key_refused = reshadePresent;')
    source += '\n' + raw
    source += r'''
LLImageRaw image;
extern "C" __declspec(dllexport) void reshade(int present){reshadePresent=present;}

extern "C" __declspec(dllexport) int snapshot(Callback cb,int fail,int tiled){
 callback=cb;injected=fail;displayCalls=0;scratchReleases=0;
 LLPipeline::ui=LLPipeline::sShowHUDAttachments=true;viewer.mWorldViewRectRaw={};
 gGLManager.mGLMaxTextureSize=tiled?2048:16384;
 bool ok=viewer.rawSnapshot(&image,3840,2160,false,false,false,false,false,false,false,LLSnapshotModel::SNAPSHOT_TYPE_COLOR,8192);
 assert(!LLWindow::sPresentBlocked&&presents==0);
 assert(LLPipeline::ui&&LLPipeline::sShowHUDAttachments);
 assert(gPipeline.main.deferredScreen.w==1920&&gPipeline.main.deferredScreen.h==1080);
 assert(viewer.mWorldViewRectRaw.w==1920&&viewer.mWorldViewRectRaw.h==1080);
 assert(scratchReleases==1&&!gSnapshotNoPost&&effectsEnabled);
 return ok;
}
extern "C" __declspec(dllexport) void* pixels(){return image.data.data();}
extern "C" __declspec(dllexport) int calls(){return displayCalls;}
'''
    # A DLL only for this fixture; no viewer target is built or linked.
    binary = compile_run(OUT, source, 'snapshot_reshade_refusal' if refuse_reshade else 'snapshot_contract', flags=('/LD',), run=False)
    dll = C.CDLL(str(binary))
    return dll


def main():
    import numpy as np
    from PIL import Image
    from actor_key_gl_check import Fixture, SH

    dll = native()
    fixture = Fixture()
    g = fixture.g
    w, h = 3840, 2160
    scene = np.zeros((h, w, 4), np.float32)
    scene[..., :3] = (.17, .36, .73)
    scene[..., 3] = 1
    cover = np.zeros_like(scene)
    cover[:, w // 2:, 0] = 1
    s = fixture.texture(scene, 0x8058, (w, h))
    m = fixture.texture(cover, 0x822B, (w, h), True)
    target = fixture.target(0x8058, (w, h))
    program = fixture.program((SH / 'alchemy/actorKeyCompositeF.glsl').read_text())
    read = g.fn('glReadPixels', None, g.I, g.I, g.I, g.I, g.U, g.U, C.c_void_p)
    cb_type = C.CFUNCTYPE(C.c_int, *([C.c_int] * 5), C.c_void_p)
    operations = []
    errors = []

    def callback(op, a, b, c, d, data):
        operations.append((op, a, b, c, d))
        try:
            if op == 2:
                # Readback guard cases do not need a second expensive render.
                if c:
                    return 1
                g.disable(0x0BD0)
                g.disable(0x8DB9)
                fixture.draw(program, target, [('keyScene', 0), ('keyCoverage', 1),
                    ('keyOrigin', (0., 0.)), ('keyColour', (0., 177/255, 64/255)),
                    ('keyMode', 0), ('keyPerChannel', 0)], [s, m])
            elif op == 3:
                read(a, b, c, d, 0x1907, 0x1401, data)
        except Exception as exc:
            errors.append(repr(exc))
            return 0
        return 1

    cb = cb_type(callback)
    dll.snapshot.argtypes = [cb_type, C.c_int, C.c_int]
    dll.pixels.restype = C.c_void_p
    def successful_snapshot(library):
        assert library.snapshot(cb, 0, 0) == 1
    for present in (0, 1):
        dll.reshade(present)
        operations.clear()
        successful_snapshot(dll)
    print('PASS D56 production rawSnapshot succeeds with ReShade absent/present and effects ON', flush=True)
    assert not errors, errors
    assert dll.calls() == 1
    assert (0, w, h, 0, 0) in operations
    assert sum(op[0] == 3 for op in operations) == h
    pixels = np.frombuffer(C.string_at(dll.pixels(), w*h*3), np.uint8).reshape(h, w, 3)
    assert np.all(pixels[:, :w//2] == (0, 177, 64))
    assert np.all(pixels[:, w//2:] == np.rint(np.array([.17, .36, .73])*255).astype(int))
    path = OUT / 'raw_snapshot_fixture_4k.png'
    Image.fromarray(pixels).save(path)
    assert np.array_equal(np.array(Image.open(path)), pixels)
    print('PASS complete production rawSnapshot: 1920x1080 window -> 3840x2160 scratch COLOR, UI/HUD hidden, full GLSL exact key/actor PNG, 2160 guarded scanlines, dimensions/UI/HUD/present/scratch cleanup restored', flush=True)
    for tiled in (0, 1):
        for fail in range(1, 8):
            operations.clear()
            assert dll.snapshot(cb, fail, tiled) == 0, (fail, tiled)
            assert dll.calls() == 1
            assert not any(op[0] == 3 for op in operations), (fail, tiled, 'refused readback')
    assert not errors, errors
    assert g.fn('glGetError', g.U)() == 0
    print('PASS production rawSnapshot scratch/tiled refusals: K0 injections 1..5, post-K2 Glass failure, early display return; no readback or present; cleanup executes', flush=True)
    bad = native(refuse_reshade=True)
    bad.snapshot.argtypes = [cb_type, C.c_int, C.c_int]
    try:
        successful_snapshot(bad)
    except AssertionError:
        print('PASS negative control: reintroduced ReShade rawSnapshot refusal rejected', flush=True)
    else:
        raise AssertionError('ReShade snapshot refusal escaped success contract')
    print('LIMIT: scene/display and window seams are instrumented; this is not a running-viewer in-world acceptance test', flush=True)


if __name__ == '__main__':
    main()
