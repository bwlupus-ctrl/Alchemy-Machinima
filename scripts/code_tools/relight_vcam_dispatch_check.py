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
    # The dispatch now consults cube_on directly for look reach. Its cached
    # control lives in the scene-collection section omitted by this fixture;
    # retain the production declaration and default rather than a second value.
    cube_start = render.index('    static LLCachedControl<bool> cube_on(')
    cube_control = render[cube_start:render.index(';', cube_start)+1]
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
#include "alworldshadowmath.h"
#include <array>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
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
struct Unit { int index; void activate() { glActiveTexture(0x84c0+index); }
    template<class T> void bind(T*,bool) {} void setTextureFilteringOption(int) {} void setTextureAddressMode(int) {} };
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
template<class T> T llmax(T a,T b) { return std::max(a,b); }
struct Log { template<class T> Log& operator<<(const T&) { return *this; } } logSink;
#define LL_WARNS(x) logSink
#define LL_DEBUGS(x) logSink
#define LL_INFOS(x) logSink
#define LL_INFOS_ONCE(x) logSink
#define LL_ENDL 0
#define LL_PROFILE_GPU_ZONE(x)
#define LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE(x)
struct Settings {} gSavedSettings;
bool master=true,feedSetting=true,thinSetting=false; int budgetSetting=1500;
template<class T> struct LLCachedControl {
    std::string key; T value;
    LLCachedControl(Settings&,const char* k,T v):key(k),value(v) {}
    operator T() const { return operator()(); }
    T operator()() const {
        if(key=="RenderAreaShadows") return T(master);
        if(key=="RenderAreaShadowsVCam") return T(feedSetting);
        if(key=="RenderAreaShadowLights") return T(8);
        if(key=="RenderAreaShadowThinObjects") return T(thinSetting);
        if(key=="RenderAreaShadowBudget") return T(budgetSetting);
        return value;
    }
};
struct LLViewerCamera {
    static LLViewerCamera& instance() { static LLViewerCamera v; return v; }
    float getZoomFactor() const { return 1; }
};
namespace LLPrismLens { constexpr unsigned MAX_CAPTURES=8; }
// The Prism registry's live camera feeds of this frame (fix rounds 4-5): count, fixed capacity target sizes and slot identities (0..n-1),
// settable by the harness, and the slot of the feed render that is running (set by the harness for each pass).
unsigned liveFeedCount=8;
int activeFeedSlot=0;
unsigned liveFeedQueries=0;
namespace LLPrismLens {
inline U32 liveAreaFeeds(U32 (&widths)[MAX_CAPTURES],U32 (&heights)[MAX_CAPTURES],U32 (&slots)[MAX_CAPTURES]) {
    ++liveFeedQueries;
    for(unsigned i=0;i<liveFeedCount;++i) { widths[i]=64;heights[i]=48;slots[i]=i; }
    return liveFeedCount;
}
inline S32 activeAreaFeedSlot() { return activeFeedSlot; }
}
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
struct mat4 {
    float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    const float* operator[](int column) const { return m[column]; }
};
struct vec3 { float x,y,z; };
vec3 make_vec3(const float* p) { return {p[0],p[1],p[2]}; }
struct mat3 { float m[9]={}; mat3(const mat4&) {} };
inline const float* value_ptr(const mat3& v) { return v.m; }
}
glm::mat4 get_current_modelview() { return {}; }
glm::vec3 mul_mat4_vec3(glm::mat4,glm::vec3 p) { return p; }
struct LLVOVolume { LLUUID id; LLUUID getID() const { return id; } float getLightRadius() const { return 5.f; } };
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
    void uniform3fv(LLStaticHashedString,int,const float*) {} // the world resolve's rotation (never uploaded with the maps off)
    int enableTexture(int uniform) { return uniform==0 ? 3 : 3+uniform; }
    void disableTexture(int) {}
} gAreaShadowProgram{0},gAreaShadowFilterProgram{1};
using LLGLSLShader=Shader;
namespace LLShaderMgr { constexpr int AREA_SHADOW_SOURCE=0,AREA_SHADOW_MAP0=1,AREA_SHADOW_MAP1=2,AREA_WORLD_ATLAS=3; }
namespace LLTexUnit { constexpr int TFO_POINT=0,TAM_CLAMP=1; }
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
struct Take { ALAreaShadow::Workload work{}; int authoredSteps=0; bool feed=false,segmentedRequested=false; bool planned=false; std::string status; unsigned width=0,height=0,frame=0; };
struct LLPipeline {
    struct RenderTargetPack {
        LLRenderTarget areaGather[2],areaScratch[2],areaVisibility[2];
        bool areaAllocFailed=false; // log state only
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
    unsigned mAreaShadowCount=2,mPrismSavedAreaCount=0,mAreaShadowWanted=0,mAreaShadowAdmitted=0,mAreaShadowThin=0;
    bool mAreaShadowThinRequested=false,mAreaShadowLowMemory=false;
    std::array<LLUUID,8> mAreaShadowIDs{},mPrismSavedAreaIDs{};
    Take mRelightAreaTake,mRelightFeedAreaTake,mPrismSavedAreaTake;
    std::array<LLUUID,10> mRelightAreaSlotIDs{},mPrismSavedAreaSlotIDs{};
    ALAreaShadow::FeedPasses mPrismAreaPasses;
    ALAreaShadow::FeedSet mAreaLiveFeeds; unsigned mAreaLiveFeedsFrame=~0u;
    // Stand-in for the native sampler's shadow_clip; the production dispatch must upload z and w.
    LLVector4 nativeSpotShadowClip() const { LLVector4 v; v.mV[2]=64; v.mV[3]=128; return v; }
    unsigned mAreaShadowQueries[2]={}; bool mAreaShadowQueryPending=false;
    // P1-lite world maps: the fixture runs with them OFF (cube_main is false); these only let the world branches compile.
    LLRenderTarget mWorldShadowAtlas; bool mWorldShadowUniformsLive=false;
    bool worldShadowOwned(const LLUUID&,U32&) const { return false; }
    // World Shadow pick: nothing is picked here (cube_main is false); these only let the pick branches compile. The ownership record is the
    // production struct (begin on a live main-view pass, commit at the end of the pass), so the dispatch below runs the real begin/commit.
    ALWorldShadow::PickedOwnership<LLUUID> mWorldPickedOwned; bool gSnapshot=false;
    bool isLiveMainAreaShadowPass() const;   // the production body is compiled below
    static bool isWorldShadowPicked(const LLUUID&) { return false; }
    static F32 worldShadowLightNear(LLDrawable*,F32) { return 0.05f; } // world branch is OFF; real near is executed by world_gl_check
    static bool projectorDetailShadows(const LLUUID&) { return false; } // world branch is OFF; no detail-projector registry in this fixture
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
    static inline F32 RenderSpotShadowOffset=0.04f; // the native sampler's receiver offset (settings default)
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
        'bool LLPipeline::isLiveMainAreaShadowPass() const',
        'LLVector4 LLPipeline::deferredLightShape('))
    fixture += '\nvoid LLPipeline::begin() {\n'+begin+'}\nvoid LLPipeline::end() {\n'+end+'}\n'
    fixture += '\nvoid LLPipeline::bindDeferredShader(Shader& shader) { shader.bind();\n'+area_binding+'}\n'
    fixture += release + '\n' + method(nv / 'pipeline.cpp', 'void LLPipeline::releasePrismLensBuffers()')
    fixture += '\n' + method(nv / 'pipeline.cpp', 'void LLPipeline::releaseUnusedPrismAreaTargets(')
    # Execute the production entry, including every OFF/view/limit/program gate.
    # Only scene collection is supplied by the fixed fixture objects.
    fixture += '\n'+entry
    fixture += 'ALAreaShadow::Selection<LLUUID> selected; selected.size=count;\n'
    fixture += 'for(unsigned i=0;i<count;++i) selected.lights[i].id=drawables[i].volume.id;\n'
    # Light 0 owns native slot 0 (fade .75); the others have no native map.
    fixture += 'selected.lights[0].shadowMap=true; mShadowSpotLight[0]=&drawables[0]; mSpotLightFade[0]=.75f;\n'
    # Importance scores for the smooth group fade: light i has score 8 - 0.02 i, so every admitted light is within 5 percent of
    # the first dropped one whenever lights are dropped, and fades.
    fixture += 'for(unsigned i=0;i<8;++i) selected.lights[i].score=8.0-0.02*i;\n'
    fixture += cube_control + '\nconst bool cube_main = cube_on() && !sPrismLensRender;\n'
    # The production ownership-record prologue (the shared live-main predicate, then begin) sits before the sliced dispatch body, which adds and commits.
    fixture += 'const bool pick_record_pass = isLiveMainAreaShadowPass();\nif (pick_record_pass) mWorldPickedOwned.begin(gFrameCount, mRT);\n'
    fixture += state+dispatch
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
        cb(26,5,0,0,nullptr,nullptr)==int(mRT->areaVisibility[mAreaShadowCount>4 ? 1 : 0].texture);
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
        activeFeedSlot=slot;
        p.begin();
        if(p.areaShadowsActive() || p.mAreaShadowCount || p.deferredLightShape(&p.drawables[0].volume).mV[3]!=-1) return -1;
        const auto record_before=p.mWorldPickedOwned;
        p.dispatch();
        // A VCam feed pass neither begins, writes nor commits the main view's pick record.
        if(p.mWorldPickedOwned.frame!=record_before.frame || p.mWorldPickedOwned.view!=record_before.view || p.mWorldPickedOwned.committed!=record_before.committed ||
           p.mWorldPickedOwned.active!=record_before.active || p.mWorldPickedOwned.count!=record_before.count) return -9;
        int valid=p.mAreaShadowValid;
        if(valid && !p.routed) return -6;
        // A failed second plane keeps plane 0 only (the four most important lights); the others
        // fall back to no soft shadow (-1), never to a stale channel.
        const unsigned expect=(p.count>4 && !p.mRT->areaVisibility[1].isComplete()) ? 4u : p.count;
        if(valid) {
            if(p.mAreaShadowCount!=expect) return -2;
            for(unsigned i=0;i<p.count;++i) {
                if(i<expect && (p.mAreaShadowIDs[i]!=p.drawables[i].volume.id ||
                    p.deferredLightShape(&p.drawables[i].volume).mV[3]!=float(i))) return -2;
                if(i>=expect && p.deferredLightShape(&p.drawables[i].volume).mV[3]!=-1) return -2;
            }
        }
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
    if(action==15) liveFeedCount=slot;
    if(action==16) return int(liveFeedQueries);
    if(action==14) {
        // Thin objects ON at a work budget (millions, = slot) that cannot cover every light at Thin quality.
        p.mRT=&p.mMainRT;p.sPrismLensRender=false;p.count=8;
        p.mRT->deferredScreen.width=width;p.mRT->deferredScreen.height=height;
        p.setLights(101,light);
        thinSetting=true;feedSetting=false;budgetSetting=slot;
        p.dispatch();
        thinSetting=false;feedSetting=true;budgetSetting=1500;p.sPrismLensRender=true;
        return p.mAreaShadowValid;
    }
    if(action==13) {
        p.mRT=&p.mMainRT;p.sPrismLensRender=false;p.count=slot;
        p.mRT->deferredScreen.width=width;p.mRT->deferredScreen.height=height;
        p.setLights(101,light);
        p.dispatch();p.sPrismLensRender=true;
        return p.mAreaShadowValid;
    }
    if(action==6) {
        p.mRT=&p.mMainRT;p.sPrismLensRender=false;p.count=2;
        p.mRT->deferredScreen.width=width;p.mRT->deferredScreen.height=height;
        p.setLights(101,light);
        p.dispatch();p.sPrismLensRender=true;
        // The live main pass records and commits ownership for exactly (this frame, the main render targets).
        if(p.mAreaShadowValid && (!p.mWorldPickedOwned.committed || p.mWorldPickedOwned.active || p.mWorldPickedOwned.view!=&p.mMainRT || p.mWorldPickedOwned.frame!=p.gFrameCount)) return -9;
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
        seen={}
        history={}   # every vec4 upload by name, in order (one per plane): the `seen` value is the LAST plane's
        calls=[]
        errors=[]
        fail_at=0
        fail_from=0
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
                    if allocations==fail_at or (fail_from and allocations>=fail_from): return 0
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
                if event==4:
                    calls.append(('i',name.decode(),a))
                    g['ui'](g['loc'](current_program,name),a)
                if event==5:
                    seen[('filter:' if current_program==g['area_filter'] else '')+name.decode()]=list(C.cast(pointer,C.POINTER(F))[:a])
                    g['uf'][a](g['loc'](current_program,name),*C.cast(pointer,C.POINTER(F))[:a])
                if event==6:
                    seen[('filter:' if current_program==g['area_filter'] else '')+name.decode()]=list(C.cast(pointer,C.POINTER(F))[:4*a])
                    history.setdefault(('filter:' if current_program==g['area_filter'] else '')+name.decode(),[]).append(list(C.cast(pointer,C.POINTER(F))[:4*a]))
                    g['uniform4fv'](g['loc'](current_program,name),a,C.cast(pointer,C.POINTER(F)))
                if event==7: g['uniform4uiv'](g['loc'](current_program,name),a,C.cast(pointer,C.POINTER(U)))
                if event==8:
                    calls.append(('mask',a))
                    color_mask(*[bool(a&(1<<i)) for i in range(4)])
                if event==9:
                    g['active'](0x84C0+b);g['bindTex'](0x0DE1,a)
                if event==10:
                    calls.append(('draw',))
                    g['draw'](4,0,3)
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
        # The production dispatch uploads the native map's clip and receiver offset (housing is the global
        # light bias only: no per-light box is uploaded).
        assert 'area_box' not in seen,('no per-light housing box uniform',seen.get('area_box'))
        assert seen.get('area_native_clip')==[64.0,128.0],('native clip upload',seen.get('area_native_clip'))
        assert len(seen.get('area_native_offset',[]))==1 and abs(seen['area_native_offset'][0]-0.04)<1.e-6,\
            ('native receiver offset upload',seen.get('area_native_offset'))
        # The bilateral filter takes the same hand-over inputs (P0 fix round 1): without them its one-sided hand-over stop is
        # inert and contact-only taps leak across the band. Every pass of it carries them; the dropout uniform reaches the gather.
        assert seen.get('filter:area_native_clip')==[64.0,128.0],('filter native clip upload',seen.get('filter:area_native_clip'))
        assert len(seen.get('filter:area_native_offset',[]))==1 and abs(seen['filter:area_native_offset'][0]-0.04)<1.e-6,\
            ('filter native receiver offset upload',seen.get('filter:area_native_offset'))
        assert seen.get('area_dropout')==[0.0,0.0,0.0,0.0],('admission dropout upload',seen.get('area_dropout'))
        # Fix round 2: the filter takes the per-channel contact fade (a light with no native map keeps its whole kernel across
        # the hand-over band). Light 0 is mapped (native slot 0 at fade .75), the others are not.
        assert seen.get('area_contact_fade')==[0.75,0.0,0.0,0.0],('gather contact fade upload',seen.get('area_contact_fade'))
        assert seen.get('filter:area_contact_fade')==[0.75,0.0,0.0,0.0],('filter contact fade upload',seen.get('filter:area_contact_fade'))

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
        # Allocation failure (K 8, two planes). Plane 0 failing leaves nothing behind and no soft
        # shadows this frame; plane 1 failing keeps plane 0 (the four most important lights)
        # instead of releasing everything. Every failure is retried on the next frame.
        run(5,8)
        for failed_target in range(1,7):
            run(1)  # clean state: every target index is attempted from scratch
            assert live==main_live
            fail_at=allocations+failed_target
            result=run()
            if failed_target<=3:
                assert result==0 and live==main_live,('plane 0 allocation failure cleanup',failed_target)
            else:
                assert result==1 and len(live-main_live)==3,('plane 1 failure keeps plane 0',failed_target)
            fail_at=0
            before=allocations
            assert run()==1 and len(live-main_live)==6,('the next frame retries and recovers',failed_target)
        # Equivalent inputs give equivalent output regardless of failure history: after any number
        # of failed frames the first successful frame is byte-identical to a fresh one, and every
        # failed frame attempted the allocation again (nothing is remembered).
        run(1);assert live==main_live
        run(5,8)
        assert run()==1
        fresh=read_last().copy()
        run(1);assert live==main_live
        for pattern,expect in ((1,0),(4,1)):  # plane 0 never allocates / only plane 1 never allocates
            fail_from=allocations+pattern
            for _ in range(5):
                before=allocations
                assert run()==expect,('persistent failure renders only what fits',pattern)
                assert allocations>before,'a failed allocation is retried every frame'
            fail_from=0
            assert run()==1,('the first frame with memory renders at once',pattern)
            assert g['np'].array_equal(fresh,read_last()),('output after a failure history equals a fresh frame',pattern)
            run(1);assert live==main_live
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
        # Fix round 5 (GLM, DeepSeek P1): feed passes are admitted by FEED IDENTITY (the registry slot), once per frame. With all eight
        # slots live, a Gate that renders the SAME source over and over gets only its first pass; the other live feeds still get theirs.
        before_draws=len(outputs)
        assert run()==1
        draws=len(outputs)
        assert draws>before_draws
        for _ in range(7): assert run(same_frame=True)==0 and len(outputs)==draws,'a repeat of one feed used another live feed\'s pass'
        for other in range(1,8):
            assert run(slot=other,same_frame=True)==1,('a live feed was starved by the repeats of another',other)
        assert run(slot=3,same_frame=True)==0
        assert run()==1  # next frame resets accounting, no stale visibility
        # Fix rounds 4-5: a feed pass is planned from the feeds LIVE this frame. With none live (a stale frame) the pass is refused and
        # draws nothing; with two live (slots 0 and 1), a pass of any other feed, or a third pass, is refused.
        run(15,0)
        draws=len(outputs)
        assert run()==0 and len(outputs)==draws,'a feed pass with no live feed drew work'
        run(15,2)
        assert run()==1 and run(slot=1,same_frame=True)==1 and run(same_frame=True)==0 and run(slot=1,same_frame=True)==0,\
            'passes per frame are one per live feed'
        assert run(slot=5,same_frame=True)==0,'a feed that is not in the live set drew work'
        # The live set is read from the registry ONCE per frame (the main plan and every feed pass reuse the snapshot).
        queries=run(16)
        run(15,3)
        assert run()==1 and run(slot=2,same_frame=True)==1
        assert run(16)-queries==1,'the live feed set was read more than once in a frame'
        run(15,8)
        run(3)
        assert live==main_live
        outputs.append(main_outputs[-1])
        assert g['np'].array_equal(main_visibility,read_last()),'feeds overwrote main production visibility'
        run(7)
        assert not live
        # Main view, requested K 8: the spare plane survives a fifth light leaving the frustum
        # (no free and reallocation of three RGBA8 targets) and returns without allocating.
        assert run(13,6)==1
        kept=allocations,releases
        assert run(13,3)==1 and (allocations,releases)==kept,'spare plane churned when a fifth light left'
        assert run(13,6)==1 and (allocations,releases)==kept,'spare plane reallocated for the returning light'
        # Plane 0 failing leaves an already-allocated spare plane 1 alone: back at the old size only plane 0
        # is rebuilt (three targets), not both planes (the hitch the spare plane exists to avoid).
        spare=len(live)
        fail_at=allocations+1          # the resize reallocates plane 0 first, and that fails
        assert run(13,6,width=80,height=60)==0
        fail_at=0
        assert len(live)==spare-3,('plane 0 released, plane 1 kept',len(live),spare)
        before=allocations
        assert run(13,6)==1 and allocations-before==3 and len(live)==spare,\
            ('plane 0 failure freed the spare plane',allocations-before)
        run(7)
        assert not live
        def gather_draws():
            """The gather draws of one dispatch, in order: (samples, steps, thin, near, first, last, plane), plane counted from
            the draws themselves (a plane's draws have strictly increasing first channels)."""
            state_ints={}
            out=[]
            previous_first=-1
            plane=-1
            for i,call in enumerate(calls):
                if call[0]=='i': state_ints[call[1]]=call[2]
                if call[0]=='draw' and i>0 and calls[i-1][0]=='i' and calls[i-1][1]=='area_count':
                    first=state_ints['area_first']
                    if first<=previous_first or plane<0: plane+=1
                    previous_first=first
                    out.append((state_ints['area_samples'],state_ints['area_steps'],state_ints['area_thin_objects'],
                                state_ints['area_near_steps'],first,state_ints['area_count'],plane))
            return out

        def hybrid_dispatch(millions,width=1280,height=720):
            calls.clear()
            history.clear()
            assert run(14,millions,width,height)==1
            draws=gather_draws()
            assert draws,'the hybrid dispatch drew nothing'
            channels={}
            for samples,steps,thin,near,first,last,plane in draws:
                for c in range(first,last):
                    assert plane*4+c not in channels,('a channel is drawn twice',plane*4+c)
                    channels[plane*4+c]=(samples,steps,thin,near)
            return draws,channels

        # Thin objects hybrid through the production dispatch (fix round 2, U-thin): at a work budget that cannot cover every
        # light at Thin quality the highest ranked lights march Thin objects and the others the regular march, in separate
        # draws with their own uniforms; every admitted channel is drawn exactly once, the Thin ones first.
        draws,channels=hybrid_dispatch(400)
        marches=sorted(set(channels.values()))
        thin_channels=sorted(c for c,m in channels.items() if m[2]==1)
        rest_channels=sorted(c for c,m in channels.items() if m[2]==0)
        assert sorted(channels)==list(range(len(channels))) and len(channels)==8,('every admitted light is drawn once',sorted(channels))
        assert thin_channels and rest_channels and thin_channels==list(range(len(thin_channels))),\
            ('the top ranked lights march Thin, the others regular',thin_channels,rest_channels)
        assert all(channels[c][3]==48 for c in thin_channels) and all(channels[c][3]==0 for c in rest_channels),'near segment only on Thin lights'
        assert len(marches)==2 and all(m[0]>=4 for m in marches),('both groups keep at least 4 rays',marches)
        assert min(channels[c][1] for c in rest_channels)>=8 and min(channels[c][1]-48 for c in thin_channels)>=8,'step floors'
        hybrid_marches=marches
        # Lights dropped for want of budget: every admitted light fades (smooth group fade, smoothstep(1, 1.05, score / first
        # dropped score)), uploaded per channel.
        draws,channels=hybrid_dispatch(24)
        admitted=len(channels)
        assert 1<=admitted<8,('this budget drops lights',admitted)
        expect=[]
        last_plane=(admitted-1)//4        # the dispatch observation holds the LAST plane's upload
        for i in range(last_plane*4,admitted):
            ratio=(8.0-0.02*i)/(8.0-0.02*admitted)
            t=min(max((ratio-1.0)/0.05,0.0),1.0)
            # Lead decision 10/22 (tied lamps): an admitted light never fades below half strength.
            expect.append(1.0-max(0.5,t*t*(3.0-2.0*t)))
        got=seen.get('area_dropout')
        assert got is not None and all(abs(got[i]-expect[i])<1.e-5 for i in range(len(expect))) and all(v==0.0 for v in got[len(expect):]),\
            ('per-channel smooth group fade upload',got,expect)
        assert all(0.45<v<=0.5 for v in got[:len(expect)]),\
            'every admitted light within 5 percent of the first dropped one stays lit at half strength (the tied-lamps floor), never faded out'
        # Hybrid boundary (fix round 4): the Thin lights upload the share of rays that march the regular way,
        # 1 - max(0.5, smoothstep(1, 1.05, score / score of the first regular light)), per channel (the tied-lamps floor); the
        # regular march's step count follows.
        draws,channels=hybrid_dispatch(400)
        thin_channels=sorted(c for c,m in channels.items() if m[2]==1)
        lead=len(thin_channels)
        assert 1<=lead<len(channels),('this budget makes a hybrid',lead,len(channels))
        planes=(len(channels)+3)//4
        uploads=history.get('area_regular_mix',[])[-planes:]      # one upload per plane, in plane order
        assert len(uploads)==planes,('one regular-share upload per plane',len(uploads),planes)
        mix=uploads[-1]
        want_all=[]
        for plane in range(planes):
            want=[]
            for i in range(plane*4,plane*4+4):
                if i<lead:
                    ratio=(8.0-0.02*i)/(8.0-0.02*lead)
                    t=min(max((ratio-1.0)/0.05,0.0),1.0)
                    want.append(1.0-max(0.5,t*t*(3.0-2.0*t)))
                else: want.append(0.0)
            want_all.append(want)
            assert all(abs(uploads[plane][i]-want[i])<1.e-5 for i in range(4)),('regular-march share upload, plane',plane,uploads[plane],want)
        assert any(v>0.0 for plane in want_all for v in plane),('the Thin lights carry a regular share',want_all)
        assert all(v<=0.5+1.e-6 for plane in uploads for v in plane),('a Thin light never marches more than half of its rays regular',uploads)
        # The Thin and the regular group march the SAME ray count (the disk strata must match at the boundary).
        assert len({m[0] for m in marches})==1,('both groups of a hybrid march the same ray count',marches)
        rest_steps=[call[2] for call in calls if call[0]=='i' and call[1]=='area_rest_steps']
        rest_march=[m for m in channels.values() if m[2]==0][0]
        assert rest_steps and rest_steps[-1]==rest_march[1],('regular-march step count upload',rest_steps,rest_march)
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
        'unbounded_passes': fixture.replace('if (feed_slot < 0 || !mPrismAreaPasses.admit(gFrameCount,static_cast<unsigned>(feed_slot),live_feeds)) return;',''),
        'passes_not_by_identity': fixture.replace('mPrismAreaPasses.admit(gFrameCount,static_cast<unsigned>(feed_slot),live_feeds)','mPrismAreaPasses.admit(gFrameCount,live_feeds.id[0],live_feeds)'),
        'live_feeds_ignored': fixture.replace('for (U32 i=0; i<live; ++i) mAreaLiveFeeds.add(feed_widths[i],feed_heights[i],feed_slots[i]);',''),
        'live_set_read_every_pass': fixture.replace('if (mAreaLiveFeedsFrame != gFrameCount)','if (true)'),
        'missing_gl_restore': fixture.replace('glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);',''),
        'native_clip_dropped': fixture.replace('gAreaShadowProgram.uniform2f(LLStaticHashedString("area_native_clip"),native_clip.mV[2],native_clip.mV[3]);',''),
        'sticky_failure': fixture.replace('take.status = "allocation_failed";',
            'take.status = "allocation_failed"; static unsigned sticky_frame = 0; if (sticky_frame && gFrameCount - sticky_frame < 300) return;').replace(
            'plane_ok[plane] = false;','plane_ok[plane] = false; sticky_frame = gFrameCount ? gFrameCount : 1;'),
        'plane1_failure_drops_all': fixture.replace('plane_ok[plane] = false;','plane_ok[plane] = false; plane_ok[0] = false;'),
        'plane0_failure_frees_spare': fixture.replace('if (!plane_ok[0]) break;','if (!plane_ok[0]) { release_plane(1); break; }'),
        'native_offset_dropped': fixture.replace('gAreaShadowProgram.uniform1f(LLStaticHashedString("area_native_offset"),native_offset);',''),
        'filter_native_clip_dropped': fixture.replace('gAreaShadowFilterProgram.uniform2f(LLStaticHashedString("area_native_clip"),native_clip.mV[2],native_clip.mV[3]);',''),
        'filter_native_offset_dropped': fixture.replace('gAreaShadowFilterProgram.uniform1f(LLStaticHashedString("area_native_offset"),native_offset);',''),
        'dropout_dropped': fixture.replace('gAreaShadowProgram.uniform4fv(LLStaticHashedString("area_dropout"),1,dropout.mV);',''),
        'dropout_not_per_channel': fixture.replace('            dropout.mV[i] = static_cast<F32>(1.0-admission[plane*4+i]);',
            '            if (i+1==count) dropout.mV[i] = static_cast<F32>(1.0-admission[plane*4+i]);'),
        'tie_floor_dropped': fixture.replace('ALAreaShadow::admittedStrength(','ALAreaShadow::admissionStrength('),
        'regular_mix_dropped': fixture.replace('gAreaShadowProgram.uniform4fv(LLStaticHashedString("area_regular_mix"),1,regular.mV);',''),
        'regular_mix_not_per_channel': fixture.replace('            regular.mV[i] = static_cast<F32>(regular_mix[plane*4+i]);',
            '            if (i+1==count) regular.mV[i] = static_cast<F32>(regular_mix[plane*4+i]);'),
        'regular_steps_dropped': fixture.replace('gAreaShadowProgram.uniform1i(LLStaticHashedString("area_rest_steps"),work.lead < work.admitted ? work.rest.steps : 0);',''),
        'regular_mix_from_rank_not_score': fixture.replace('ALAreaShadow::regularShare(selected.lights[i].score,selected.lights[work.lead].score)','ALAreaShadow::regularShare(1.0,0.0)'),
        'regular_share_floor_dropped': fixture.replace('ALAreaShadow::regularShare(selected.lights[i].score,selected.lights[work.lead].score)','1.0-ALAreaShadow::admissionStrength(selected.lights[i].score,selected.lights[work.lead].score)'),
        'filter_contact_fade_dropped': fixture.replace('gAreaShadowFilterProgram.uniform4fv(LLStaticHashedString("area_contact_fade"),1,contact_fade.mV);',''),
        'hybrid_rest_march_ignored': fixture.replace('{work.lead, 8, work.rest, work.restChannels}','{work.lead, 8, work.q, work.restChannels}'),
        'hybrid_lead_march_ignored': fixture.replace('{0, work.lead, work.q, work.channels}','{0, work.lead, work.rest, work.channels}'),
        'spare_plane_churn': fixture.replace('const U32 kept = llmax(planes_wanted,static_cast<U32>(llclamp(cap(),1,8)) > 4 ? 2u : 1u);','const U32 kept = planes_wanted;'),
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
    # The production uploads above leave a native clip on the shared linked gather; later fixtures
    # assume the GL defaults (no cutoff, no receiver offset).
    g['area_use'](g['area_gather'])
    g['uf'][2](g['loc'](g['area_gather'],b'area_native_clip'),0,0)
    g['uf'][1](g['loc'](g['area_gather'],b'area_native_offset'),0)
    # ... and the per-channel uniforms (a mapped light 0 at fade .75, the group fade, the Thin march of the hybrid dispatch).
    for name in (b'area_contact_fade',b'area_dropout',b'area_contact_dist',b'area_regular_mix'):
        g['uf'][4](g['loc'](g['area_gather'],name),0,0,0,0)
    for name in (b'area_thin_objects',b'area_near_steps',b'area_rest_steps'):
        g['ui'](g['loc'](g['area_gather'],name),0)
    g['area_use'](g['area_filter'])
    g['uf'][2](g['loc'](g['area_filter'],b'area_native_clip'),0,0)
    g['uf'][1](g['loc'](g['area_filter'],b'area_native_offset'),0)
    g['uf'][4](g['loc'](g['area_filter'],b'area_contact_fade'),0,0,0,0)
    print('[RelightQ] VCam production dispatch/GL ownership PASS: zero steady allocations/releases, one allocation per target on resize; Gate cuts, OFF pool cleanup, failure/retry, eviction, close, actual-pass cap and negative mutations',flush=True)
