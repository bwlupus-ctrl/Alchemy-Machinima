"""Run production target ownership and dispatch blocks through the gate's real GL.

Only viewer wrappers are replaced: allocation/binding/uniform/draw operations
call back into the existing WGL context and linked production GLSL programs.
No manually published channels or parallel implementation of the dispatch.
"""
from pathlib import Path
import ctypes as C
import os
import subprocess
from relight_area_policy_check import method


def check_dispatch(g):
    nv = Path('indra/newview')
    render = method(nv / 'pipeline.cpp', 'void LLPipeline::renderAreaShadows(')
    lighting = method(nv / 'pipeline.cpp', 'void LLPipeline::renderDeferredLighting(')
    assert lighting.index('renderAreaShadows(debugLightingTerm()') < lighting.index('deferredLightShape(')
    binding = method(nv / 'pipeline.cpp', 'void LLPipeline::bindDeferredShader(')
    area_binding = binding[binding.index('    if (areaShadowsActive())'):binding.index('    bindLightFunc(shader);')]
    begin = method(nv / 'pipeline.cpp', 'bool LLPipeline::beginPrismAuxiliaryState()')
    begin = begin[begin.index('    static LLCachedControl<bool> area('):begin.index('    mPrismSavedNearbyLights =')]
    end = method(nv / 'pipeline.cpp', 'void LLPipeline::endPrismAuxiliaryState()')
    end = end[end.index('    if (mPrismAreaStateSaved)'):end.index('    // Discard the temporary')]
    entry = render[:render.index('    const auto& registry =')]
    call_start = lighting.index('        renderAreaShadows(debugLightingTerm()')
    production_call = lighting[call_start:lighting.index(';', call_start)+1]
    dispatch = render[render.index('    const U32 width = mRT->deferredScreen.getWidth()'):render.index('    static LLFrameTimer summary;')]
    state = render[render.index('    struct State\n'):render.index('    const U32 width = mRT->deferredScreen.getWidth()')]
    release = method(nv / 'pipeline.cpp', 'static void release_prism_scratch_entry(')
    assert 'release_prism_scratch_entry(*target);' in method(nv / 'pipeline.cpp', 'bool LLPipeline::acquirePrismLensScratch(')
    # Verify the real last-close and screen-buffer paths reach the tested owner.
    assert 'releasePrismLensBuffers();' in method(nv / 'pipeline.cpp', 'void LLPipeline::releaseScreenBuffers()')
    prism = (nv / 'llprismlens.cpp').read_text(encoding='utf-8')
    close = prism[prism.index('        if (count() == 0)'):]
    assert close.index('gPipeline.releasePrismLensBuffers();') < close.index('++mRevision;')

    fixture = r'''
#include "alareashadowmath.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
using U32=unsigned; using U8=unsigned char; using GLuint=unsigned;
using S32=int; using F32=float; using F64=double; using GLint=int;
using GLenum=unsigned; using GLboolean=unsigned char;
using GLuint64=unsigned long long;
constexpr int GL_RGBA8=0x8058,GL_TRUE=1,GL_FALSE=0,GL_QUERY_RESULT_AVAILABLE=1,GL_QUERY_RESULT=2,GL_TIMESTAMP=3;
using Callback=int(__cdecl*)(int,int,int,int,const char*,const void*);
Callback cb;
constexpr unsigned GL_COLOR_WRITEMASK=0x0c23,GL_ACTIVE_TEXTURE=0x84e0,GL_SCISSOR_TEST=0x0c11;
constexpr unsigned GL_MAX_DRAW_BUFFERS=0x8824,GL_DRAW_FRAMEBUFFER_BINDING=0x8ca6,GL_READ_FRAMEBUFFER_BINDING=0x8caa;
constexpr unsigned GL_READ_BUFFER=0x0c02,GL_VIEWPORT=0x0ba2,GL_DRAW_BUFFER0=0x8825;
constexpr unsigned GL_BLEND=0x0be2,GL_DRAW_FRAMEBUFFER=0x8ca9,GL_READ_FRAMEBUFFER=0x8ca8;
constexpr unsigned GL_CULL_FACE=0x0b44,GL_STENCIL_TEST=0x0b90,GL_DEPTH_TEST=0x0b71;
constexpr unsigned LL_NUM_TEXTURE_LAYERS=6;
void glGetIntegerv(unsigned n,int* p) { cb(11,n,0,0,nullptr,p); }
void glGetBooleanv(unsigned n,unsigned char* p) { cb(12,n,0,0,nullptr,p); }
void glGetBooleani_v(unsigned n,int i,unsigned char* p) { cb(13,n,i,0,nullptr,p); }
bool glIsEnabled(unsigned n) { return cb(14,n,0,0,nullptr,nullptr)!=0; }
bool glIsEnabledi(unsigned n,int i) { return cb(15,n,i,0,nullptr,nullptr)!=0; }
void glEnable(unsigned n) { cb(16,n,1,0,nullptr,nullptr); }
void glDisable(unsigned n) { cb(16,n,0,0,nullptr,nullptr); }
void glEnablei(unsigned n,int i) { cb(17,n,i,1,nullptr,nullptr); }
void glDisablei(unsigned n,int i) { cb(17,n,i,0,nullptr,nullptr); }
void glColorMaski(int i,bool a,bool b,bool c,bool d) { cb(18,i,int(a)|(int(b)<<1)|(int(c)<<2)|(int(d)<<3),0,nullptr,nullptr); }
void glBindFramebuffer(unsigned n,int i) { cb(19,n,i,0,nullptr,nullptr); }
void glActiveTexture(int i) { cb(20,i,0,0,nullptr,nullptr); }
void glDrawBuffers(int n,const unsigned* p) { cb(21,n,0,0,nullptr,p); }
void glDrawBuffer(unsigned n) { cb(22,n,0,0,nullptr,nullptr); }
void glReadBuffer(unsigned n) { cb(23,n,0,0,nullptr,nullptr); }
void glViewport(int x,int y,int w,int h) { int p[]={x,y,w,h};cb(24,0,0,0,nullptr,p); }
void glColorMask(bool,bool,bool,bool);
struct Unit { int index; void activate() { glActiveTexture(0x84c0+index); } };
struct GL {
    std::array<Unit,6> units={Unit{0},Unit{1},Unit{2},Unit{3},Unit{4},Unit{5}};
    void flush() {}
    unsigned getCurrentTexUnitIndex() { int n=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&n);return n-0x84c0; }
    Unit* getTexUnit(unsigned n) { return &units[n]; }
    void setColorMask(bool a,bool b,bool c,bool d) { glColorMask(a,b,c,d); }
} gGL;
struct LLActorMaskTextureState {
    int texture=0;
    LLActorMaskTextureState(Unit& u) { u.activate();glGetIntegerv(0x8069,&texture); }
    void restore(Unit& u) { cb(9,texture,u.index,0,nullptr,nullptr); }
};
struct LLGLDisable {
    unsigned cap;bool enabled;
    LLGLDisable(unsigned c):cap(c),enabled(glIsEnabled(c)) { glDisable(c); }
    ~LLGLDisable() { if(enabled) glEnable(cap); else glDisable(cap); }
};
struct LLGLDepthTest : LLGLDisable { LLGLDepthTest(int,int):LLGLDisable(GL_DEPTH_TEST) {} };
struct LLVertexBuffer { static void unbind() {} };
template<class T> T llmin(T a,T b) { return std::min(a,b); }
template<class T> T llclamp(T v,T a,T b) { return std::clamp(v,a,b); }
struct Log { template<class T> Log& operator<<(const T&) { return *this; } } logSink;
#define LL_WARNS(x) logSink
#define LL_DEBUGS(x) logSink
#define LL_INFOS(x) logSink
#define LL_INFOS_ONCE(x) logSink
#define LL_ENDL 0
#define LL_PROFILE_GPU_ZONE(x)
#define LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE(x)
struct Settings {} gSavedSettings;
bool master=true,feedSetting=true;
template<class T> struct LLCachedControl {
    std::string key; T value;
    LLCachedControl(Settings&,const char* k,T v):key(k),value(v) {}
    T operator()() const {
        if(key=="RenderAreaShadows") return T(master);
        if(key=="RenderAreaShadowsVCam") return T(feedSetting);
        if(key=="RenderAreaShadowLights") return T(8);
        return value;
    }
};
struct LLViewerCamera {
    static LLViewerCamera& instance() { static LLViewerCamera v; return v; }
    float getZoomFactor() const { return 1; }
};
namespace LLPrismLens { constexpr unsigned MAX_CAPTURES=8; }
struct LLUUID {
    unsigned char mData[16]={};
    static const LLUUID null;
    LLUUID(int id=0) { mData[0]=id; }
    bool operator!=(const LLUUID& b) const { return mData[0]!=b.mData[0]; }
    bool operator==(const LLUUID& b) const { return mData[0]==b.mData[0]; }
    bool operator<(const LLUUID& b) const { return mData[0]<b.mData[0]; }
};
const LLUUID LLUUID::null{};
struct LLVector4 { float mV[4]={}; void clear() { for(auto& v:mV) v=0; } void set(float x,float y,float z,float r) { mV[0]=x;mV[1]=y;mV[2]=z;mV[3]=r; } };
namespace glm {
struct mat4 {};
struct vec3 { float x,y,z; };
vec3 make_vec3(const float* p) { return {p[0],p[1],p[2]}; }
}
glm::mat4 get_current_modelview() { return {}; }
glm::vec3 mul_mat4_vec3(glm::mat4,glm::vec3 p) { return p; }
struct LLVOVolume { LLUUID id; LLUUID getID() const { return id; } };
LLVector4 light_shape_for_volume(const LLVOVolume*) { LLVector4 v;v.mV[3]=-1;return v; }
float area_source_radius(LLVOVolume*) { return .001f; }
struct LLDrawable { LLVOVolume volume; LLVector4 position;
    LLVOVolume* getVOVolume() { return &volume; }
    const LLVOVolume* getVObj() const { return &volume; }
    LLVector4 getPositionAgent() { return position; }
};
struct LLStaticHashedString { const char* text; LLStaticHashedString(const char* s):text(s) {} };
bool programReady=true;
struct Shader {
    int program;
    bool isComplete() const { return programReady; }
    static inline Shader* sCurBoundShaderPtr=nullptr;
    void bind() { cb(3,program,0,0,nullptr,nullptr);sCurBoundShaderPtr=this; }
    static void unbind() { cb(25,0,0,0,nullptr,nullptr);sCurBoundShaderPtr=nullptr; }
    void uniform1i(LLStaticHashedString s,int v) { cb(4,v,0,0,s.text,nullptr); }
    void uniform1f(LLStaticHashedString s,float x) { cb(5,1,0,0,s.text,&x); }
    void uniform2f(LLStaticHashedString s,float x,float y) { float p[]={x,y};cb(5,2,0,0,s.text,p); }
    void uniform3f(LLStaticHashedString s,float x,float y,float z) { float p[]={x,y,z};cb(5,3,0,0,s.text,p); }
    void uniform4fv(LLStaticHashedString s,int n,const float* p) { cb(6,n,0,0,s.text,p); }
    void uniform4uiv(LLStaticHashedString s,int n,const unsigned* p) { cb(7,n,0,0,s.text,p); }
    int enableTexture(int uniform) { return uniform==0 ? 3 : 3+uniform; }
    void disableTexture(int) {}
} gAreaShadowProgram{0},gAreaShadowFilterProgram{1};
using LLGLSLShader=Shader;
namespace LLShaderMgr { constexpr int AREA_SHADOW_SOURCE=0,AREA_SHADOW_MAP0=1,AREA_SHADOW_MAP1=2; }
namespace LLTexUnit { constexpr int TFO_POINT=0; }
namespace LLRender { constexpr int TRIANGLES=4; }
void glColorMask(bool a,bool b,bool c,bool d) { cb(8,int(a)|(int(b)<<1)|(int(c)<<2)|(int(d)<<3),0,0,nullptr,nullptr); }
void glGenQueries(int,unsigned*) {}
void glDeleteQueries(int,unsigned*) {}
void glGetQueryObjectiv(unsigned,int,int*) {}
void glGetQueryObjectui64v(unsigned,int,unsigned long long*) {}
void glQueryCounter(unsigned,int) {}
struct LLRenderTarget {
    unsigned texture=0,width=0,height=0;
    bool isComplete() const { return texture!=0; }
    unsigned getWidth() const { return width; } unsigned getHeight() const { return height; }
    bool allocate(unsigned w,unsigned h,int,bool=false) {
        release(); texture=cb(0,w,h,0,nullptr,nullptr);
        width=w;height=h; return isComplete();
    }
    void release() { if(texture) cb(1,texture,0,0,nullptr,nullptr);texture=width=height=0; }
    void bindTarget() { cb(2,texture,width,height,nullptr,nullptr); }
    void bindTexture(int,int unit,int) { cb(9,texture,unit,0,nullptr,nullptr); }
    void flush() {}
};
struct VB { void setBuffer() {} void drawArrays(int,int,int) { cb(10,0,0,0,nullptr,nullptr); } } vb;
struct Take { ALAreaShadow::Workload work{}; int authoredSteps=0; bool feed=false,segmentedRequested=false; bool planned=false; std::string status; unsigned width=0,height=0; };
struct LLPipeline {
    struct RenderTargetPack {
        LLRenderTarget areaGather[2],areaScratch[2],areaVisibility[2];
        LLRenderTarget actorMask,silCover,screen,deferredScreen,deferredLight;
        unsigned actorMaskDepthSrc=0,silCoverDepthSrc=0,width=0,height=0;
    } mMainRT;
    struct PrismLensScratch {
        RenderTargetPack rt;
        LLRenderTarget waterDis,waterExclusionMask;
        unsigned width=0,height=0,lastUsedFrame=0;
    } mPrismLensScratchPool[8];
    RenderTargetPack* mRT=&mMainRT;
    bool sPrismLensRender=true,mPrismAreaStateSaved=false,mPrismAreaTiled=false;
    bool sPrismPreviewRender=false,sRenderingHUDs=false,sImpostorRender=false,sReflectionRender=false;
    bool routed=false;
    bool mAreaShadowValid=true,mPrismSavedAreaValid=false;
    unsigned mAreaShadowCount=2,mPrismSavedAreaCount=0;
    std::array<LLUUID,8> mAreaShadowIDs{},mPrismSavedAreaIDs{};
    Take mRelightAreaTake,mPrismSavedAreaTake;
    std::array<LLUUID,10> mRelightAreaSlotIDs{},mPrismSavedAreaSlotIDs{};
    ALAreaShadow::FeedPasses mPrismAreaPasses;
    unsigned mAreaShadowQueries[2]={}; bool mAreaShadowQueryPending=false;
    double mAreaShadowGPUms=-1;
    VB* mScreenTriangleVB=&vb;
    std::array<LLDrawable,8> drawables;
    std::array<LLDrawable*,8> mLights;
    static constexpr unsigned MAX_SPOT_SHADOWS=10;
    std::array<LLDrawable*,10> mShadowSpotLight{};
    float mSpotLightFade[10]={};
    LLPipeline() { for(unsigned i=0;i<8;++i) mLights[i]=&drawables[i]; }
    void setLights(int first,const float* light) {
        for(unsigned i=0;i<8;++i) {
            drawables[i].volume.id=LLUUID(first+i);
            drawables[i].position.set(light[0],light[1],light[2],0);
        }
    }
    unsigned gFrameCount=0, count=8;
    bool half() const { return true; } int samples() const { return 8; }
    int steps() const { return 12; } int cap() const { return 8; }
    float denoise() const { return 2; } float light_bias() const { return .25f; }
    float max_dist() const { return 8; } float thickness() const { return .1f; }
    void bindDeferredShader(Shader& shader);
    void unbindDeferredShader(Shader&) { Shader::unbind(); }
    void begin(); void end(); void dispatch(); void releasePrismLensBuffers();
    void renderAreaShadows(S32,F32,bool,bool,bool,bool);
    void releaseUnusedPrismAreaTargets(const std::array<std::pair<U32,U32>,8>&);
    int debugLightingTerm() const { return 0; }
    bool areaShadowsAllowedThisView() const;
    bool areaShadowsActive() const;
    LLVector4 deferredLightShape(const LLVOVolume*) const;
    void releasePrismPreviewScratch() {}
    PrismLensScratch* mActivePrismLensScratch=nullptr;
};
bool gCubeSnapshot=false;
'''
    fixture += '\n'.join(method(nv / 'pipeline.cpp', signature) for signature in (
        'bool LLPipeline::areaShadowsAllowedThisView() const',
        'bool LLPipeline::areaShadowsActive() const',
        'LLVector4 LLPipeline::deferredLightShape('))
    fixture += '\nvoid LLPipeline::begin() {\n'+begin+'}\nvoid LLPipeline::end() {\n'+end+'}\n'
    fixture += '\nvoid LLPipeline::bindDeferredShader(Shader& shader) { shader.bind();\n'+area_binding+'}\n'
    fixture += release + '\n' + method(nv / 'pipeline.cpp', 'void LLPipeline::releasePrismLensBuffers()')
    fixture += '\n' + method(nv / 'pipeline.cpp', 'void LLPipeline::releaseUnusedPrismAreaTargets(')
    # Execute the production entry, including every OFF/view/limit/program gate.
    # Only scene collection is supplied by the fixed fixture objects.
    fixture += '\n'+entry
    fixture += 'ALAreaShadow::Selection<LLUUID> selected; selected.size=count;\n'
    fixture += 'for(unsigned i=0;i<count;++i) selected.lights[i].id=drawables[i].volume.id;\n'+state+dispatch
    fixture += r'''
}
void LLPipeline::dispatch() {
    const int effective_local_light_count=count;
    const float light_scale=1;
    const bool actor_only=false,actor_mask_ready=false,actor_as_world=false,actor_excluded=false;
'''+production_call+r'''
    cb(27,0,0,0,nullptr,nullptr); // Observe the production guard before consumer binding.
    // Execute the actual production visibility consumer binding after publish.
    bindDeferredShader(gAreaShadowProgram);
    routed=cb(26,4,0,0,nullptr,nullptr)==int(mRT->areaVisibility[0].texture) &&
        cb(26,5,0,0,nullptr,nullptr)==int(mRT->areaVisibility[count>4 ? 1 : 0].texture);
}
'''
    fixture += r'''
LLPipeline p;
extern "C" __declspec(dllexport) int run(Callback callback,int action,int slot,int width,int height,int frame,const float* light) {
    cb=callback;
    if(action==0) {
        p.sPrismLensRender=true;
        p.mRT=&p.mPrismLensScratchPool[slot].rt;
        p.mActivePrismLensScratch=&p.mPrismLensScratchPool[slot];
        p.mActivePrismLensScratch->width=width;p.mActivePrismLensScratch->height=height;
        p.mRT->deferredScreen.width=width;p.mRT->deferredScreen.height=height;
        p.mMainRT.deferredScreen.width=32;p.mMainRT.deferredScreen.height=24;
        p.gFrameCount=frame;p.setLights(17+slot*8,light);
        p.begin();
        if(p.areaShadowsActive() || p.mAreaShadowCount || p.deferredLightShape(&p.drawables[0].volume).mV[3]!=-1) return -1;
        p.dispatch();
        int valid=p.mAreaShadowValid;
        if(valid && !p.routed) return -6;
        if(valid) for(unsigned i=0;i<p.count;++i)
            if(p.mAreaShadowCount!=p.count || p.mAreaShadowIDs[i]!=p.drawables[i].volume.id ||
                p.deferredLightShape(&p.drawables[i].volume).mV[3]!=float(i)) return -2;
        p.sPrismLensRender=false;p.mRT=&p.mMainRT;p.end();
        if(!p.mAreaShadowValid || p.mAreaShadowCount!=2) return -3;
        if(p.deferredLightShape(&p.drawables[0].volume).mV[3]!=-1) return -4;
        LLVOVolume mainLight{LLUUID(101)};
        if(p.deferredLightShape(&mainLight).mV[3]!=0) return -5;
        return valid;
    }
    if(action==1 || action==2) {
        p.begin();p.sPrismLensRender=true;p.mRT=&p.mPrismLensScratchPool[slot].rt;
        if(action==1) feedSetting=false;else master=false;
        p.dispatch();feedSetting=master=true;
        p.sPrismLensRender=false;p.mRT=&p.mMainRT;p.end();
    }
    if(action==3) p.releasePrismLensBuffers();
    if(action==4) release_prism_scratch_entry(p.mPrismLensScratchPool[slot]);
    if(action==5) p.count=slot;
    if(action==6) {
        p.mRT=&p.mMainRT;p.sPrismLensRender=false;p.count=2;
        p.mRT->deferredScreen.width=width;p.mRT->deferredScreen.height=height;
        p.setLights(101,light);
        p.dispatch();p.sPrismLensRender=true;
        return p.mAreaShadowValid;
    }
    if(action==7) {
        for(auto& t:p.mMainRT.areaGather) t.release();
        for(auto& t:p.mMainRT.areaScratch) t.release();
        for(auto& t:p.mMainRT.areaVisibility) t.release();
    }
    if(action==8 || action==9 || action==10) {
        p.begin();p.sPrismLensRender=true;p.mRT=&p.mPrismLensScratchPool[slot].rt;
        auto count=p.count;
        if(action==8) p.sPrismPreviewRender=true;
        if(action==9) p.count=0;
        if(action==10) programReady=false;
        p.dispatch();
        const bool invalid=!p.mAreaShadowValid && !p.mAreaShadowCount;
        p.count=count;programReady=true;p.sPrismPreviewRender=false;p.sPrismLensRender=false;p.mRT=&p.mMainRT;p.end();
        return invalid ? 0 : -8;
    }
    if(action==11) {
        std::array<std::pair<U32,U32>,8> sizes{};
        sizes[slot]={width,height};
        p.releaseUnusedPrismAreaTargets(sizes);
    }
    return 1;
}
'''
    directory = Path(f'.tmp/relight_step6_dispatch_{os.getpid()}').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))

    def compile_fixture(source, label):
        (directory / (label+'.cpp')).write_text(source,encoding='utf-8')
        dll = directory / (label+'.dll')
        command = (f'@echo off\ncall "{vcvars}" >nul\n'
                   f'cl /nologo /std:c++17 /EHsc /LD /I"{nv.resolve()}" '
                   f'/Fo"{directory / (label+".obj")}" /Fe"{dll}" '
                   f'"{directory / (label+".cpp")}" /link /IMPLIB:"{directory / (label+".lib")}"\nexit /b %ERRORLEVEL%\n')
        (directory / 'compile.cmd').write_text(command,encoding='utf-8')
        subprocess.run(['cmd','/d','/c',str(directory / 'compile.cmd')],check=True)
        lib = C.CDLL(str(dll))
        return lib

    callback_type = C.CFUNCTYPE(C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_char_p,C.c_void_p)
    F,U = g['F'],g['U']
    delete_tex = g['gl']('glDeleteTextures',None,g['I'],C.POINTER(U))
    color_mask = g['gl']('glColorMask',None,U,U,U,U)
    get_int = g['gl']('glGetIntegerv',None,U,C.POINTER(g['I']))
    raw = {
        11:get_int,
        12:g['gl']('glGetBooleanv',None,U,C.c_void_p),
        13:g['gl']('glGetBooleani_v',None,U,U,C.c_void_p),
        14:g['gl']('glIsEnabled',C.c_ubyte,U),
        15:g['gl']('glIsEnabledi',C.c_ubyte,U,U),
        16:(g['gl']('glDisable',None,U),g['gl']('glEnable',None,U)),
        17:(g['gl']('glDisablei',None,U,U),g['gl']('glEnablei',None,U,U)),
        18:g['gl']('glColorMaski',None,U,C.c_ubyte,C.c_ubyte,C.c_ubyte,C.c_ubyte),
        21:g['gl']('glDrawBuffers',None,g['I'],C.POINTER(U)),
        22:g['gl']('glDrawBuffer',None,U),
        23:g['gl']('glReadBuffer',None,U),
    }
    resource_sets=[]

    def exercise(lib):
        allocations=0
        releases=0
        live=set()
        resource_sets.append(live)
        outputs=[]
        errors=[]
        fail_at=0
        current_program=0
        fixture_data=None
        dispatch_state=[]
        observe_state=None

        @callback_type
        def callback(event,a,b,c,name,pointer):
            nonlocal allocations,releases,current_program
            try:
                if event==0:
                    allocations+=1
                    if allocations==fail_at: return 0
                    texture=g['area_texture'](0,g['np'].zeros((b,a,4),g['np'].float32),0x8058)
                    live.add(texture.value)
                    return texture.value
                if event==1:
                    assert a in live
                    live.remove(a);releases+=1
                    texture=U(a);delete_tex(1,C.byref(texture))
                if event==2:
                    assert a in live,'production dispatch targeted an unowned texture'
                    g['bindfbo'](0x8D40,g['area_fbo'])
                    g['attachment'](0x8D40,0x8CE0,0x0DE1,a,0)
                    assert g['status'](0x8D40)==0x8CD5
                    g['viewport'](0,0,b,c)
                    outputs.append((a,b,c))
                if event==3:
                    current_program=g['area_gather'] if a==0 else g['area_filter']
                    g['area_use'](current_program)
                    view,proj,depth,normals,*_=fixture_data
                    for key,value in [(b'projection_matrix',proj),(b'inv_proj',g['np'].linalg.inv(proj))]:
                        value=g['np'].array(value.T,dtype=g['np'].float32).copy()
                        g['matrix'](g['loc'](current_program,key),1,0,value.ctypes.data_as(C.POINTER(F)))
                    g['ui'](g['loc'](current_program,b'depthMap'),1)
                    g['ui'](g['loc'](current_program,b'normalMap'),2)
                    g['ui'](g['loc'](current_program,b'area_source'),3)
                if event==4: g['ui'](g['loc'](current_program,name),a)
                if event==5: g['uf'][a](g['loc'](current_program,name),*C.cast(pointer,C.POINTER(F))[:a])
                if event==6: g['uniform4fv'](g['loc'](current_program,name),a,C.cast(pointer,C.POINTER(F)))
                if event==7: g['uniform4uiv'](g['loc'](current_program,name),a,C.cast(pointer,C.POINTER(U)))
                if event==8: color_mask(*[bool(a&(1<<i)) for i in range(4)])
                if event==9:
                    g['active'](0x84C0+b);g['bindTex'](0x0DE1,a)
                if event==10: g['draw'](4,0,3)
                if event==11: raw[11](a,C.cast(pointer,C.POINTER(g['I'])))
                if event==12: raw[12](a,pointer)
                if event==13: raw[13](a,b,pointer)
                if event==14: return raw[14](a)
                if event==15: return raw[15](a,b)
                if event==16: raw[16][b](a)
                if event==17: raw[17][c](a,b)
                if event==18: raw[18](a,*[bool(b&(1<<i)) for i in range(4)])
                if event==19: g['bindfbo'](a,b)
                if event==20: g['active'](a)
                if event==21: raw[21](a,C.cast(pointer,C.POINTER(U)))
                if event==22: raw[22](a)
                if event==23: raw[23](a)
                if event==24: g['viewport'](*C.cast(pointer,C.POINTER(g['I']))[:4])
                if event==25: g['use'](0)
                if event==26:
                    g['active'](0x84c0+a)
                    value=g['I']();get_int(0x8069,C.byref(value));return value.value
                if event==27: dispatch_state[:]=observe_state()
                return 1
            except Exception as error:
                errors.append(error)
                return 0

        lib.run.argtypes=[callback_type,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.POINTER(F)]
        lib.run.restype=C.c_int
        frame=0

        def run(action=0,slot=0,width=64,height=48,eye=0,same_frame=False):
            nonlocal frame,fixture_data,observe_state
            if not same_frame: frame+=1
            fixture_data=g['camera_fixture'](width,height,eye)
            view,proj,depth,normals,wall,blocker,light=fixture_data
            g['area_texture'](1,depth);g['area_texture'](2,normals)
            position=g['np'].array(light[:3],dtype=g['np'].float32)
            g['use'](0)
            def state_snapshot():
                # Independent real GL observations across the production guard.
                values=[]
                for token,count in ((0x8ca6,1),(0x8caa,1),(0x0ba2,4),(0x84e0,1),(0x0c23,4),(0x0c02,1)):
                    data=(g['I']*count)();get_int(token,data);values.extend(data)
                values.extend(raw[14](token) for token in (0x0be2,0x0b44,0x0b90,0x0b71,0x0c11))
                return values
            before=state_snapshot()
            observe_state=state_snapshot
            result=lib.run(callback,action,slot,width,height,frame,position.ctypes.data_as(C.POINTER(F)))
            assert not errors,errors
            if action in (0,6): assert dispatch_state==before,'production GL state restoration'
            assert result>=0,('transaction/channel isolation',result)
            return result

        def read_last():
            assert outputs,'production dispatch issued no target binds'
            texture,width,height=outputs[-1]
            assert texture in live,'scope exit discarded a production visibility target'
            g['bindfbo'](0x8D40,g['area_fbo'])
            g['attachment'](0x8D40,0x8CE0,0x0DE1,texture,0)
            pixels=g['np'].empty((height,width,4),g['np'].uint8)
            g['read'](0,0,width,height,0x1908,0x1401,pixels.ctypes.data)
            return pixels

        # Render and retain MAIN through the SAME production dispatch/owner.
        # Every feed receives this pack as well as its own pack.
        assert run(6)==1
        main_outputs=outputs.copy()
        main_live=live.copy()
        main_visibility=read_last().copy()
        main_allocations=allocations
        for action in (8,9,10):
            before=allocations,len(outputs)
            assert run(action)==0
            assert (allocations,len(outputs))==before,'production early return dispatched work'
        run(5,1)  # one selected light initially: one trio of targets
        assert run()==1
        assert allocations-main_allocations==3 and len(live-main_live)==3
        first=read_last()
        assert g['np'].any(first[:,:,0]<255),'production gather must really execute'
        for _ in range(6):
            assert run()==1
            assert g['np'].array_equal(first,read_last()),'steady current-frame identity'
        assert allocations-main_allocations==3 and releases==0,'steady updates must allocate and release ZERO targets'
        before=allocations
        run(width=63,height=45)
        assert allocations-before==3 and releases==3,'one resize allocation per target'
        assert len(live-main_live)==3
        # Gate camera cuts re-use the same size bucket and overwrite channels.
        camera_a=read_last()
        before=allocations
        run(width=63,height=45,eye=1)
        camera_b=read_last()
        assert not g['np'].array_equal(camera_a,camera_b),'Gate cut must use the incoming camera'
        run(width=63,height=45,eye=0)
        assert g['np'].array_equal(camera_a,read_last())
        assert allocations==before,'Gate camera changes must reuse storage'
        for slot in (1,2,3): run(slot=slot)
        assert len(live-main_live)==12
        # Warm both RGBA planes, then changing selected K must not tear down
        # the spare plane or allocate again when that plane returns.
        before=allocations
        run(5,8);run(width=63,height=45)
        assert allocations-before==3 and len(live-main_live)==15
        before=allocations,releases
        for count in (1,4,8,7,1,8):
            run(5,count);run(width=63,height=45)
        assert (allocations,releases)==before,'K changes must retain both feed planes'
        run(5,1)
        run(1)  # opt-in OFF cleans every bucket, not just active pack
        assert live==main_live
        before=allocations,releases
        run(1)
        assert (allocations,releases)==before,'steady OFF has no allocation/deletion'
        run();run(2)  # master OFF
        assert not live,'master OFF must release main and feed area targets'
        assert run(6)==1
        assert g['np'].array_equal(main_visibility,read_last()),'master OFF/ON changed main output'
        main_live=live.copy()
        main_outputs=outputs.copy()
        # Allocation failure after a successful first target must clean all
        # partial allocations and publish no channels, then recover on retry.
        run(5,8)
        for failed_target in range(1,7):
            fail_at=allocations+failed_target
            assert run()==0 and live==main_live,('partial allocation cleanup',failed_target)
        fail_at=0
        run(5,1)
        assert run()==1 and len(live-main_live)==3
        run(4)  # real recycle/eviction owner
        assert live==main_live
        run();run(slot=1);run(3)  # last consumer closes / releaseScreenBuffers
        assert live==main_live
        run(width=63,height=45);run(slot=1)
        retained=read_last().copy()
        before=allocations,releases
        run(11,slot=1)
        assert allocations==before[0] and releases==before[1]+3
        assert len(live-main_live)==3 and g['np'].array_equal(retained,read_last()),'unused size pruning touched a live camera bucket'
        run(3)
        assert live==main_live
        # All eight passes may be the SAME feed in one frame. Ninth skips.
        before_draws=len(outputs)
        assert run()==1
        for _ in range(7): assert run(same_frame=True)==1
        draws=len(outputs)
        assert draws>before_draws and run(same_frame=True)==0 and len(outputs)==draws
        assert run()==1  # next frame resets accounting, no stale visibility
        run(3)
        assert live==main_live
        outputs.append(main_outputs[-1])
        assert g['np'].array_equal(main_visibility,read_last()),'feeds overwrote main production visibility'
        run(7)
        assert not live

    exercise(compile_fixture(fixture,'dispatch'))
    # Executable mutations: compile the same assertions against broken
    # production blocks without changing any worktree source.
    mutations = {
        'per_update_release': fixture.replace(end,end+'\nfor(auto& e:mPrismLensScratchPool) for(auto& t:e.rt.areaVisibility) t.release();'),
        'missing_dispatch': fixture.replace(production_call,'/* production call removed */'),
        'feed_early_return': fixture.replace('if (!allowed)\n','if (!allowed || sPrismLensRender)\n'),
        'wrong_owner': fixture.replace('p.dispatch();','p.mRT=&p.mMainRT;p.dispatch();',1),
        'wrong_consumer': fixture.replace('if (unit >= 0) mRT->areaVisibility','if (unit >= 0) mMainRT.areaVisibility'),
        'stale_prepass': fixture.replace('mAreaShadowValid = false;\n        mAreaShadowCount = 0;',''),
        'missing_eviction': fixture.replace('for (auto& target : entry.rt.areaVisibility) target.release();',''),
        'unbounded_passes': fixture.replace('if (sPrismLensRender && !mPrismAreaPasses.admit(gFrameCount,LLPrismLens::MAX_CAPTURES)) return;',''),
        'missing_gl_restore': fixture.replace('glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);',''),
    }
    for label,mutated in mutations.items():
        assert mutated!=fixture,label
        try:
            exercise(compile_fixture(mutated,label))
        except AssertionError:
            print('[RelightQ] VCam mutation rejected:',label,flush=True)
        else:
            raise AssertionError('mutation survived: '+label)
        finally:
            for resources in resource_sets:
                for texture in resources:
                    handle=U(texture);delete_tex(1,C.byref(handle))
                resources.clear()
    print('[RelightQ] VCam production dispatch/GL ownership PASS: zero steady allocations/releases, one allocation per target on resize; Gate cuts, OFF pool cleanup, failure/retry, eviction, close, actual-pass cap and negative mutations',flush=True)
