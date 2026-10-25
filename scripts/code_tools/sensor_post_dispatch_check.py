"""Compile complete production finalize/target bodies against instrumented GL seams.

The root renderFinalize body is compiled unchanged, including its master gate,
allocations and ON/OFF branch. Only scene rendering and terminal draw calls are
mocked. This is a dispatch test, not a viewer build.
"""
import ctypes as C
import os
from pathlib import Path
import subprocess


def body(source, name):
    import re
    match=re.search(r'^(?:void|bool|U32|LLGLSLShader\*) LLPipeline::'+name+r'\(',source,re.M)
    assert match,name
    end=source.index('\n}',match.start())+2
    return source[match.start():end]


def run(r, root, out):
    pipe=(root/'indra/newview/pipeline.cpp').read_text(encoding='utf-8')
    methods='\n'.join(body(pipe,n) for n in ('releaseSensorTargets','ensureSensorTargets',
        'sensorDiagnosticView','colorCorrectProgram','sensorAAEncode','sensorVignette','sensorWarpUniforms',
        'compositeSensorOverlay','copyRenderTarget','applyFXAA','generateSMAABuffers','applySMAA','renderVolumetric','renderFinalize'))
    # The no-post predicate counts calls, including the production sky lookup
    # seam. All setting reads use current values, even for static cached controls.
    prefix=r'''
#include <cstdint>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <algorithm>
#include <array>
using U32=std::uint32_t;using U64=std::uint64_t;using F32=float;using S32=int;using GLenum=unsigned;
using GLboolean=bool;using GLfloat=float;
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE
#define LL_PROFILE_GPU_ZONE(x)
#define LL_WARNS(x) std::cerr
#define LL_ENDL std::endl
#define llassert(x) ((void)0)
#define GL_RGBA16F 0x881A
#define GL_RGB10_A2 0x8059
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_ALWAYS 0x207
#define GL_EQUAL 0x202
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_STENCIL_TEST 0xB90
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_STENCIL_BUFFER_BIT 0x400
#define GL_BLEND 1
#define GL_CULL_FACE 2
#define GL_COLOR 3
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
template<class T>T llmax(T a,T b){return std::max(a,b);}
template<class T>T llclamp(T x,T a,T b){return std::clamp(x,a,b);}
struct Vec{float mV[4]={0,0,0,0};};using LLVector3=Vec;
namespace glm {struct mat4{};}
glm::mat4 get_current_projection(){return {};}
glm::mat4 get_current_modelview(){return {};}
using LLStaticHashedString=std::string;
static std::map<std::string,double> settings;
static std::vector<int> events;
static int context_calls=0,cleanup_calls=0,legacy_calls=0,physical_calls=0,uniforms=0,feedback=0;
static int wire_errors=0;
static int predication_bindings=0;
static std::map<std::string,int> uniform_int;
static std::map<std::string,int> texture_bindings;
static std::map<std::string,std::array<float,4>> uniform_vec;
std::string uniform_key(int id,std::string key){return std::to_string(id)+"/"+key;}
std::string uniform_key(int id,int key){return uniform_key(id,std::to_string(key));}
static bool no_post=false,shader_ok=true,fail_alloc=false;
using Callback=int(*)(int,int,int,int,int);static Callback callback;
struct Settings {
 bool getBOOL(std::string n){return settings[n]!=0;}
 U32 getU32(std::string n){return U32(settings[n]);}
 F32 getF32(std::string n){return F32(settings[n]);}
 Vec getVector3(std::string){return {};}
}gSavedSettings;
template<class T>struct LLCachedControl {
 std::string n;
 LLCachedControl(Settings&,const char* k,T value,const char* =nullptr):n(k){if(!settings.count(n))settings[n]=double(value);}
 T operator()()const{return T(settings[n]);}operator T()const{return operator()();}
};
struct LLRenderTarget {
 int id=0;U32 width=0,height=0;bool complete=false;
 bool allocate(U32 w,U32 h,int fmt,bool=false){bool ok=callback(0,id,w,h,fmt)!=0;if(ok){width=w;height=h;complete=true;}return ok;}
 bool addColorAttachment(int fmt){return callback(1,id,width,height,fmt)!=0;}
 void release(){callback(3,id,width,height,0);width=height=0;complete=false;}
 void shareDepthBuffer(LLRenderTarget& t){callback(2,t.id,width,height,0);}
 U32 getWidth()const{return width;}U32 getHeight()const{return height;}
 bool isComplete()const{return complete;}int getUsage(){return 0;}bool hasStencil(){return true;}
 void clear(int){}
 void bindTarget();void flush(){}
 void bindTexture(int,int,int);
};
static LLRenderTarget* bound=nullptr;
void LLRenderTarget::bindTarget(){bound=this;}
void LLRenderTarget::bindTexture(int,int,int){if(bound==this)++feedback;}
struct LLTexUnit{enum {TFO_POINT,TFO_BILINEAR,TT_TEXTURE,TAM_CLAMP};
 void bindManual(int,U32){}void bind(LLRenderTarget*,bool){}
 void setTextureFilteringOption(int){}void setTextureAddressMode(int){}void unbindFast(int){}void disable(){}
};
namespace LLShaderMgr {
 enum {DEFERRED_DIFFUSE,DIFFUSE_MAP,DEFERRED_EMISSIVE,DEPTH_WARP_PARAMS,DIOPTER_WARP_MAP,
 FISHEYE_PARAMS,FISHEYE_PARAMS2,VIGNETTE_AMOUNT,VIGNETTE_RADIUS,VIGNETTE_SOFT,VIGNETTE_SHAPE,
 VIGNETTE_MID_POINT,VIGNETTE_FEATHER,VIGNETTE_CENTER,VIGNETTE_ASPECT,SMAA_PREDICATION_TEX,
 SMAA_EDGE_TEX,SMAA_AREA_TEX,SMAA_SEARCH_TEX,SMAA_BLEND_TEX,DEFERRED_DEPTH,
 FXAA_TC_SCALE,FXAA_RCP_SCREEN_RES,FXAA_RCP_FRAME_OPT,FXAA_RCP_FRAME_OPT2,
 DEFERRED_SCREEN_RES,GODRAY_RES,GODRAY_MULTIPLIER,FALLOFF_MULTIPLIER};
}
struct LLGLSLShader {
 int id=0;bool isComplete()const{return shader_ok;}void bind(){events.push_back(1000+id);}
 void unbind(){}int enableTexture(int key,int=0){if(key==LLShaderMgr::SMAA_PREDICATION_TEX)++predication_bindings;return 0;}
 int getTextureChannel(int){return 0;}template<class... T>void disableTexture(T...){}
 template<class K>int bindTexture(K key,LLRenderTarget* t,bool=false,int=0){if(bound==t)++feedback;texture_bindings[uniform_key(id,key)]=t->id;return 0;}
 template<class K>void uniform1i(K key,int value){++uniforms;uniform_int[uniform_key(id,key)]=value;}
 template<class... T>void uniform1f(T...){++uniforms;}
 template<class... T>void uniform2f(T...){++uniforms;}
 template<class... T>void uniform2fv(T...){++uniforms;}
 template<class... T>void uniform4fv(T...){++uniforms;}
 template<class K>void uniform4f(K key,float x,float y,float z,float w){++uniforms;uniform_vec[uniform_key(id,key)]={x,y,z,w};}
};
LLGLSLShader gSensorAAEncodeProgram{1},gSensorVignetteProgram{2},gSensorOverlayProgram{3},
 gVolumetricLightProgram{4},gDeferredMotionBlurProgram{5},gCineFisheyeProgram{6},gCASProgram{7};
LLGLSLShader gSensorCameraProgram[7]={{300},{301},{302},{303},{304},{305},{306}};
LLGLSLShader gCGGammaProgram{200},gCGLegacyGammaProgram{201},gCGColorgradeGammaProgram{202},gCGColorgradeLegacyGammaProgram{203},
 gCGTonemapProgram{204},gCGTonemapLegacyGammaProgram{205},gCGTonemapColorgradeProgram{206},gCGTonemapColorgradeLegacyGammaProgram{207};
LLGLSLShader gSensorSMAAEdgeProgram[4]={{400},{401},{402},{403}},gSMAABlendWeightsProgram[4]={{420},{421},{422},{423}},
 gSensorSMAABlendProgram[4]={{440},{441},{442},{443}},gSensorFXAAProgram[4]={{460},{461},{462},{463}},
 gSMAAEdgeDetectProgram[4]={{500},{501},{502},{503}},gSMAANeighborhoodBlendProgram[4]={{540},{541},{542},{543}},
 gFXAAProgram[4]={{560},{561},{562},{563}},gGlowCombineFXAAProgram{570};
LLGLSLShader gSensorGodrayProgram{480},gDeferredPostNoDoFProgram{590};
static LLStaticHashedString sSmaaRTMetrics="SMAA_RT_METRICS";
struct LLViewerShaderMgr {static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}bool loadSensorShaders(){events.push_back(20);return shader_ok;}};
struct LLFrameTimer{inline static U32 current=42;static U32 getFrameCount(){return current;}};
struct LLVertexBuffer{static void unbind(){}void setBuffer(){}void drawArrays(int,int,int){events.push_back(30);}};
namespace LLRender{enum{TRIANGLES};}
struct LLGLState{LLGLState(int,bool){}static void checkStates(){}};
struct LLGLDepthTest{template<class... T>explicit LLGLDepthTest(T...){}};
struct LLGLDisable{explicit LLGLDisable(int){}};
struct GL{void color4f(int,int,int,int){}void setColorMask(bool,bool){}LLTexUnit* getTexUnit(int){static LLTexUnit u;return &u;}}gGL;
void glClearColor(int,int,int,int){}void glClearBufferfv(int,int,const F32*){}
void glDrawBuffer(int){}void glDrawBuffers(int,const GLenum*){}
void glStencilFunc(int,int,int){}void glStencilOp(int,int,int){}void glStencilMask(int){}
static bool gCubeSnapshot=false;
static int RenderFSAAType=0,RenderShadowDetail=1;
static bool RenderVolumetricLighting=true,BDMergeMotionBlur=true,RenderDepthOfField=true;
static int RenderVolumetricLightingResolution=16;
static float RenderVolumetricLightingMultiplier=.2f,RenderVolumetricLightingFalloffMultiplier=1.f;
struct LLTool{};struct ALToolPathEdit:LLTool{static ALToolPathEdit* getInstance(){static ALToolPathEdit s;return &s;}};
struct LLToolMgr{static LLToolMgr* getInstance(){static LLToolMgr s;return &s;}bool inBuildMode(){return false;}LLTool* getCurrentTool(){return settings["guides"]?(LLTool*)ALToolPathEdit::getInstance():nullptr;}};
struct LLFloaterReg{static bool instanceVisible(const char*){return settings["guides"]!=0;}};
struct Window{bool getUIVisibility(){return true;}}window;static Window* gViewerWindow=&window;
enum {RLV_BHVR_SETSPHERE,ROTOINK_LAYER_OVERLAY};
struct RlvActions{static bool hasBehaviour(int){return true;}};
struct LLShaderEffectParams{LLShaderEffectParams(LLRenderTarget*,LLRenderTarget*,bool){}};
enum class EVisualEffect{RlvSphere};
struct LLVfxManager{static LLVfxManager& instance(){static LLVfxManager v;return v;}void runEffect(EVisualEffect,LLShaderEffectParams*){events.push_back(45);}};
struct LLPipeline {
 enum class SensorStage{Legacy,Exposure,Optics,Camera,Godrays};
 enum{RENDER_DEBUG_FEATURE_UI};
 struct FinalizeGuides { LLPipeline& pipeline; const glm::mat4& projection; const glm::mat4& modelview; void operator()(LLRenderTarget* target,U32 attachment=0)const{pipeline.renderActorPathGuides(target,projection,modelview,attachment);} };
 struct Pack{LLRenderTarget screen{10,64,48,true},deferredScreen{11,64,48,true},deferredLight{12,64,48,true},postPingMap{14,64,48,true},postPongMap{15,64,48,true};}mMainRT,other;
 Pack* mRT=&mMainRT;
 LLRenderTarget mSensorPing[2]={{0},{1}},mSensorRoto{4},mDiopterMap{7,64,48,true},mVelocityMap{8,64,48,true};
 LLRenderTarget mFXAAMap{20,64,48,true},mSMAABlendBuffer{21,64,48,true};
 U32 mSMAASampleMap=100,mSMAAAreaMap=101,mSMAASearchMap=102;
 bool mSensorAllocated=false,mSensorClockStarted=false,mSensorPhysicalOrder=false,mSensorNoise=false,mSensorFailed=false,mSensorWasEnabled=false,mSensorDiagnostics=false,mReShadeRawSceneValid=false;
 bool sPrismLensRender=false,sPrismPreviewRender=false,sImpostorRender=false,sRenderingHUDs=false;
 U64 mSensorFrame=0;U32 mSensorLastViewerFrame=~0u,mSensorTakeSeed=0;
 LLVertexBuffer vb;LLVertexBuffer* mScreenTriangleVB=&vb;
 bool worldHDREnabled(){return settings["hdr"]!=0;}
 bool colorCorrectWillApplyExposure(bool hdr){++context_calls;return hdr&&!no_post;}
 bool shouldRunRotoInkAt(int){return settings["roto"]!=0;}
 bool hasRenderDebugFeatureMask(int){return true;}
 void assertInitialized(){}void enableLightsFullbright(){}
 void renderActorPathGuides(LLRenderTarget*,const glm::mat4&,const glm::mat4&,U32){events.push_back(80);}
 void renderFinalizeScene(bool,const FinalizeGuides&){events.push_back(21);}
 void renderFinalizeLegacy(bool,LLRenderTarget*& src,LLRenderTarget*&,bool&,bool&,const FinalizeGuides&){++legacy_calls;src=&mMainRT.screen;events.push_back(22);}
 void renderFinalizePresent(LLRenderTarget*,bool,bool){events.push_back(mSensorPhysicalOrder?90:91);}
 void colorCorrect(LLRenderTarget* src,LLRenderTarget* dst,bool tone,bool grade,SensorStage stage){
  if(src==dst)++feedback;
  if(!tone||grade!=(stage==SensorStage::Camera))++wire_errors;
  colorCorrectProgram(stage,4)->bind();events.push_back(40+int(stage));
 }
 void renderVolumetric(LLRenderTarget*,LLRenderTarget*);
 void bindDeferredShader(LLGLSLShader& shader){shader.bind();}
 void unbindDeferredShader(LLGLSLShader& shader){shader.unbind();}
 void renderMotionBlurComposite(LLRenderTarget*,LLRenderTarget*){events.push_back(51);}
 void renderDoF(LLRenderTarget*,LLRenderTarget*){events.push_back(52);}
 void renderCineOutline(LLRenderTarget*,void*,void*,bool){events.push_back(53);}
 bool renderUltimateKaleidoscope(LLRenderTarget*,LLRenderTarget*){events.push_back(sensorDiagnosticView(gSavedSettings.getU32("CineDiopterKalDebugView"))?82:54);return true;}
 bool renderUltimateDiopter(LLRenderTarget*,LLRenderTarget*){events.push_back(sensorDiagnosticView(gSavedSettings.getU32("CineDiopterDebugView"))?83:55);return true;}
 void renderCineFisheye(LLRenderTarget*,LLRenderTarget*){events.push_back(56);}
 void applyFXAA(LLRenderTarget*,LLRenderTarget*);
 void generateSMAABuffers(LLRenderTarget*,LLRenderTarget*);
 void applySMAA(LLRenderTarget*,LLRenderTarget*);
 void copyRenderTarget(LLRenderTarget*,LLRenderTarget*);
 void renderSensor(LLRenderTarget* s,LLRenderTarget* d){if(s==d)++feedback;++physical_calls;events.push_back(mSensorNoise?60:61);}
 void applyCAS(LLRenderTarget*,LLRenderTarget*){events.push_back(70);}
 void releaseSensorTargets();bool ensureSensorTargets();void renderFinalize();
 U32 sensorDiagnosticView(U32)const;
 LLGLSLShader* colorCorrectProgram(SensorStage,U32);
 void sensorAAEncode(LLRenderTarget*,LLRenderTarget*);
 void sensorVignette(LLRenderTarget*,LLRenderTarget*);
 void sensorWarpUniforms(LLGLSLShader&,bool,bool,LLRenderTarget*);
 void compositeSensorOverlay(LLRenderTarget*,LLRenderTarget*,bool,bool,bool);
};
'''
    suffix=r'''
static LLPipeline pipeline;
extern "C" __declspec(dllexport) void configure(Callback cb,unsigned mask,int enabled,int aa,int layers,int debug,int w,int h) {
 callback=cb;settings["hdr"]=mask&1;pipeline.mRT=(mask&2)?&pipeline.mMainRT:&pipeline.other;
 gCubeSnapshot=mask&4;pipeline.sPrismLensRender=mask&8;pipeline.sPrismPreviewRender=mask&16;
 pipeline.sImpostorRender=mask&32;pipeline.sRenderingHUDs=mask&64;no_post=mask&128;
 settings["CineSensorEnabled"]=enabled;settings["CineSensorPhysicalOrderEnabled"]=true;settings["CineSensorNoiseEnabled"]=true;
 settings["CineDiopterEnabled"]=true;settings["CineFisheyeEnabled"]=true;
 settings["CineDiopterDebugView"]=debug;settings["CineDiopterToolMode"]=0;
 settings["roto"]=layers&1;settings["guides"]=layers&2;RenderFSAAType=aa;
 settings["RenderFSAASamples"]=layers%4;settings["RenderSMAAPredication"]=true;
 pipeline.mMainRT.screen.width=w;pipeline.mMainRT.screen.height=h;
 pipeline.mMainRT.postPingMap.width=pipeline.mMainRT.postPongMap.width=w;
 pipeline.mMainRT.postPingMap.height=pipeline.mMainRT.postPongMap.height=h;
 events.clear();context_calls=cleanup_calls=legacy_calls=physical_calls=uniforms=feedback=wire_errors=predication_bindings=0;bound=nullptr;
 uniform_int.clear();uniform_vec.clear();texture_bindings.clear();
}
extern "C" __declspec(dllexport) int execute(){pipeline.renderFinalize();return physical_calls;}
extern "C" __declspec(dllexport) int metric(int k){switch(k){case 0:return context_calls;case 1:return legacy_calls;case 2:return uniforms;case 3:return feedback;case 4:return pipeline.mSensorClockStarted;case 5:return pipeline.mSensorFrame;case 6:return pipeline.mSensorAllocated;case 8:return uniform_int["1/sensor_aa_mode"];case 9:return uniform_int["3/sensor_overlay_mode"];case 10:{auto v=uniform_vec[uniform_key(3,LLShaderMgr::DEPTH_WARP_PARAMS)];return int(v[0]+2*v[1]);}case 11:return wire_errors;case 12:return predication_bindings;case 13:return texture_bindings[uniform_key(3,LLShaderMgr::DIFFUSE_MAP)];case 14:return texture_bindings[uniform_key(3,LLShaderMgr::DEFERRED_EMISSIVE)];}return 0;}
extern "C" __declspec(dllexport) int shader_selection(int stage,int permutation){return pipeline.colorCorrectProgram(LLPipeline::SensorStage(stage),permutation)->id;}
extern "C" __declspec(dllexport) int legacy_smaa(){
 pipeline.mSensorPhysicalOrder=false;RenderFSAAType=2;
 pipeline.generateSMAABuffers(&pipeline.mMainRT.screen,nullptr);
 pipeline.applySMAA(&pipeline.mMainRT.screen,&pipeline.mMainRT.deferredLight);
 return predication_bindings;
}
extern "C" __declspec(dllexport) int event(int n){return n<int(events.size())?events[n]:-1;}
extern "C" __declspec(dllexport) void poison_clock(){pipeline.mSensorClockStarted=true;pipeline.mSensorFrame=123;}
extern "C" __declspec(dllexport) void shader_state(int ok){shader_ok=ok;}
extern "C" __declspec(dllexport) int gate(unsigned mask){
 configure(callback,mask,1,0,0,0,64,48);return execute();
}
extern "C" __declspec(dllexport) int gate_leak(unsigned mask){return gate(mask&~4u);}
extern "C" __declspec(dllexport) int scope_lifetime(int previous){
 pipeline.mSensorPhysicalOrder=pipeline.mSensorNoise=previous!=0;
 pipeline.renderFinalize();
 return pipeline.mSensorPhysicalOrder==(previous!=0)&&pipeline.mSensorNoise==(previous!=0);
}
'''
    # Retain the original independent capture-clock wrap check in addition to
    # exercising the complete finalize dispatcher above.
    start=pipe.index('    const U32 viewer_frame = LLFrameTimer::getFrameCount();',pipe.index('void LLPipeline::renderSensor('))
    clock=pipe[start:pipe.index('    const Parameters parameters',start)]
    suffix+='''
extern "C" __declspec(dllexport) unsigned long long tick(unsigned value,int reset){
 auto& mSensorClockStarted=pipeline.mSensorClockStarted;
 auto& mSensorLastViewerFrame=pipeline.mSensorLastViewerFrame;
 auto& mSensorFrame=pipeline.mSensorFrame;
 if(reset){mSensorClockStarted=false;mSensorFrame=0;}
 LLFrameTimer::current=value;
'''+clock+'''
 return mSensorFrame;
}
'''
    out.mkdir(parents=True,exist_ok=True)
    vc=sorted((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    def build(name, production):
        path=out/(name+'.cpp');dll=out/(name+'.dll');cmd=out/(name+'.cmd')
        path.write_text(prefix+production+suffix,encoding='utf-8')
        cmd.write_text('@echo off\ncall "'+str(vc)+'" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /O2 /LD "'+str(path)+'" /Fe:"'+str(dll)+'" /Fo"'+out.as_posix()+'/" /link /IMPLIB:"'+str(out/(name+'.lib'))+'"\n',encoding='utf-8')
        result=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=root,capture_output=True,text=True,timeout=90)
        (out/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        assert result.returncode==0,result.stdout+result.stderr
        lib=C.CDLL(str(dll));return lib
    lib=build('dispatch',methods)
    # This reviewer-supplied mutation must fail the actual root dispatch.
    mutant=build('dispatch_disabled',methods.replace('mSensorPhysicalOrder = sensor_context && !mSensorFailed;',
        'mSensorPhysicalOrder = false;'))
    off_mutant=build('dispatch_off_work',methods.replace(
        'sensor_enabled() && physical_order() && hdr','hdr').replace(
        'mSensorPhysicalOrder = sensor_context && !mSensorFailed;',
        'mSensorPhysicalOrder = sensor_enabled() && physical_order() && sensor_context && !mSensorFailed;').replace(
        '!sensor_enabled() && mSensorWasEnabled','!sensor_enabled()'))
    debug_mutant=build('dispatch_debug_before_sensor',methods.replace(
        'return mSensorPhysicalOrder && !mSensorDiagnostics ? 0U : view;', 'return view;'))
    memory_mutant=build('dispatch_memory_over_budget',methods.replace('GL_RGB10_A2','GL_RGBA16F'))
    shader_mutant=build('dispatch_wrong_shader',methods.replace('return stage == SensorStage::Legacy ?', 'return true ?'))
    aa_mutant=build('dispatch_wrong_aa_shader',methods.replace(
        'mSensorPhysicalOrder ? gSensorSMAAEdgeProgram[fsaa_quality] : gSMAAEdgeDetectProgram[fsaa_quality]',
        'gSMAAEdgeDetectProgram[fsaa_quality]'))
    depth_mutant=build('dispatch_shared_depth',methods.replace(
        'if (ok) ok = mSensorRoto.addColorAttachment(GL_RGB10_A2);',
        'if (ok) ok = mSensorRoto.addColorAttachment(GL_RGB10_A2); if (ok) mMainRT.deferredScreen.shareDepthBuffer(mSensorRoto);'))
    CB=C.CFUNCTYPE(C.c_int,C.c_int,C.c_int,C.c_int,C.c_int,C.c_int)
    records=[];active={};fail=[-1];serial=[0];errors=[];depth_sharing=[]
    internal_format=r.gl.fn('glGetTexLevelParameteriv',None,C.c_uint,C.c_int,C.c_uint,C.POINTER(C.c_int))
    def callback(op,index,w,h,fmt):
        records.append((op,index,w,h,fmt))
        if op in (0,1):
            if fail[0]==sum(v[0] in (0,1) for v in records):return 0
            if fmt!=(0x881A if index in (0,1) else 0x8059):
                errors.append(('format',index,fmt));return 0
            serial[0]+=1
            tex=r.target('dispatch-'+str(serial[0]),w,h,fmt=fmt)[0]
            active.setdefault(index,[]).append(tex)
            observed=C.c_int();internal_format(0x0DE1,0,0x1003,C.byref(observed))
            if observed.value!=fmt:errors.append(('GL storage',fmt,observed.value));return 0
            r.gl.bind_fbo(0x8D40,r.base.fbo)
            for attachment in range(3):r.gl.fbo_tex(0x8D40,0x8CE0+attachment,0x0DE1,tex if attachment==0 else 0,0)
            if r.gl.fbo_status(0x8D40)!=0x8CD5:errors.append(('GL framebuffer',index));return 0
        elif op==3:
            for tex in active.pop(index,[]):r.gl.del_tex(1,C.byref(tex))
        elif op==2:depth_sharing.append(index)
        return 1
    cb=CB(callback)
    for dll in (lib,mutant,off_mutant,debug_mutant,memory_mutant,shader_mutant,aa_mutant,depth_mutant):
        dll.configure.argtypes=[CB,*([C.c_int]*7)];dll.metric.argtypes=[C.c_int];dll.event.argtypes=[C.c_int]
    def configure(dll=lib,mask=3,enabled=1,aa=2,layers=3,debug=1,w=64,h=48):
        records.clear();dll.configure(cb,mask,enabled,aa,layers,debug,w,h)
    def trace(dll):
        return [dll.event(i) for i in range(1000) if dll.event(i)!=-1]
    configure(enabled=0);lib.poison_clock()
    for _ in range(32):
        assert lib.execute()==0 and lib.metric(0)==0 and lib.metric(5)==123
    assert not records,'disabled path allocated/released'
    for mask in range(256):
        configure(mask=mask,debug=0)
        assert bool(lib.execute())==bool(mask&1 and mask&2 and not mask&252),(mask,trace(lib))
    configure(enabled=0);lib.execute()
    configure(layers=0,debug=0);assert lib.execute()==1 and 4 not in active,'unneeded optional storage'
    for aa in (0,1,2):
        for layers in (0,1,2,3):
            configure(aa=aa,layers=layers)
            assert lib.execute()==1
            t=trace(lib)
            expected=[41,1480,44,51,52,42,45,55,56,1002,60,43,70,83,90]
            positions=[]
            for v in expected:positions.append(t.index(v,positions[-1]+1 if positions else 0))
            assert positions==sorted(positions),(t,positions)
            assert lib.metric(2)>0 and lib.metric(3)==0,('uniforms/feedback',t)
            assert lib.metric(11)==0,'camera tone/grade flags'
            if aa:assert lib.metric(8)==aa,'AA mode uniform'
            if layers:
                assert lib.metric(9)==int(bool(layers&2)),'overlay mode uniform'
                assert lib.metric(10)==3,'overlay warp uniforms'
                assert lib.metric(13)==(15 if layers&2 else 4),'affine addition storage alias'
                if layers&2:assert lib.metric(14)==14,'guide white capture storage alias'
            quality=layers%4
            assert (1400+quality in t)==(aa==2) and (1460+quality in t)==(aa==1),t
            assert (1440+quality in t)==(aa==2),t
            assert lib.metric(12)==0,'sensor samples unwarped predication depth'
            assert (53 in t)==bool(layers&1) and (80 in t)==bool(layers&2),t
            assert (4 in active)==bool(layers&1),('optional storage',layers,active.keys())
            assert t.count(80)==(2 if layers&2 else 0),'guide affine captures'
            assert (1590 in t)==bool(layers&2),'guide black capture copy'
            peak=sum((8 if i in (0,1) else 4)*64*48*len(v) for i,v in active.items())
            assert peak<=64*48*24,peak
    # Resize, divisor, every partial allocation failure, shader failure, transition cleanup.
    configure(w=19,h=11);assert lib.execute()==1
    configure(enabled=0);assert lib.execute()==0 and not active
    assert lib.metric(4)==0 and lib.metric(5)==0
    configure(enabled=0);lib.poison_clock();assert lib.execute()==0 and not records and lib.metric(5)==123
    for failure in range(1,5):
        configure();fail[0]=failure
        assert lib.execute()==0 and not active
        configure(enabled=0);lib.execute()
    fail[0]=-1
    configure();lib.shader_state(0);assert lib.execute()==0 and not active
    lib.shader_state(1);configure(enabled=0);lib.execute()
    configure(w=0,h=0);assert lib.execute()==0
    configure(enabled=0);lib.execute()
    assert not errors,errors
    assert not depth_sharing,'sensor framebuffer shares sampled scene depth'
    assert lib.scope_lifetime(0) and lib.scope_lifetime(1),'context flags not restored'
    lib.tick.argtypes=[C.c_uint,C.c_int];lib.tick.restype=C.c_uint64
    clock_values=[lib.tick(0xfffffffe,1),lib.tick(0xfffffffe,0),lib.tick(0xffffffff,0),lib.tick(0,0),lib.tick(1,0)]
    assert clock_values==[0,0,1,2,3],clock_values
    configure();assert lib.execute()==1
    configure(dll=mutant);assert mutant.execute()==0,'permanently disabled mutation survived'
    configure(dll=off_mutant,enabled=0);off_mutant.poison_clock();off_mutant.execute()
    assert off_mutant.metric(0)>0 and off_mutant.metric(5)!=123,'OFF work/reset mutation survived'
    configure(dll=debug_mutant);debug_mutant.execute()
    t=trace(debug_mutant)
    assert t.index(83)<t.index(60),'diagnostic-before-sensor mutation survived'
    configure(dll=memory_mutant);assert memory_mutant.execute()==0,'253.125 MiB storage mutation survived'
    assert any(error[0]=='format' for error in errors),'storage mutation did not reach the allocation predicate'
    for stage in range(5):
        for permutation in range(8):
            expected=(200+permutation,300,301,302+permutation%4,306)[stage]
            assert lib.shader_selection(stage,permutation)==expected,'production camera shader selection'
    assert shader_mutant.shader_selection(1,4)!=300,'legacy shader selection mutation survived'
    configure(dll=aa_mutant,aa=2,layers=0,debug=0);aa_mutant.execute()
    assert 1400 not in trace(aa_mutant) and 1500 in trace(aa_mutant),'legacy predicated AA shader mutation survived'
    configure(dll=depth_mutant,layers=1);depth_mutant.execute()
    assert depth_sharing,'shared sampled-depth mutation survived'
    configure(enabled=0);lib.execute()
    assert lib.legacy_smaa()==1,'legacy default-ON SMAA predication was changed'
    r.native_lib=lib
    r.dispatch_callback=cb
    return dict(full_production_finalize=True,context_combinations=256,master_off_frames=32,
        allocation_failure_sites=4,optional_layer_combinations=12,peak_4k_mib=189.84375,
        mutation_permanently_disabled='REJECTED',off_work_mutation='REJECTED',debug_mutation='REJECTED',
        memory_mutation='REJECTED',
        shader_selection_mutation='REJECTED',uniform_values_checked=True,
        aa_selection_mutation='REJECTED',legacy_default_predication=True,
        sampled_depth_mutation='REJECTED',guides_reuse_existing_display_targets=True,
        context_flags_restored=True,capture_clock_wrap=clock_values,source_methods=methods)
