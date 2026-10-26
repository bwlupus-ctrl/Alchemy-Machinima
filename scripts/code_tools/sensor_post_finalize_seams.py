"""Instrument leaf scene/present dependencies; finalize control flow is production."""
import re


def extend(prefix, pipe):
    from sensor_post_dispatch_check import body
    helpers='\n'.join(body(pipe,n) for n in ('renderFinalizeScene','renderFinalizeLegacy','renderFinalizePresent'))
    constants=sorted(set(re.findall(r'LLShaderMgr::(\w+)',helpers)))
    enum=prefix.index(' enum {DEFERRED_DIFFUSE')
    end=prefix.index('};',enum)
    missing=[c for c in constants if not re.search(r'\b'+c+r'\b',prefix[enum:end])]
    prefix=prefix[:end]+(','+','.join(missing) if missing else '')+prefix[end:]
    prefix=prefix.replace('#include <array>','#include <array>\n#include <cmath>\n#include <cstring>')
    prefix=prefix.replace('using GLboolean=bool;using GLfloat=float;','using GLboolean=bool;using GLfloat=float;using GLint=int;')
    prefix=prefix.replace('struct Vec{float mV[4]={0,0,0,0};};using LLVector3=Vec;',
        'struct Vec{float mV[4]={0,0,0,0};Vec(float x=0,float y=0,float z=0){mV[0]=x;mV[1]=y;mV[2]=z;}operator double()const{return 0;}};using LLVector3=Vec;using LLVector2=Vec;using LLColor3=Vec;')
    prefix=prefix.replace('namespace glm {struct mat4{};}','namespace glm {struct mat4{};const float* value_ptr(const mat4&){static float m[16];return m;}}')
    prefix=prefix.replace(' void clear(int){}',' U32 getNumTextures(){return 4;}void clear(int){}')
    prefix=prefix.replace('void bindTarget();void flush(){}','void bindTarget();void flush();')
    prefix=prefix.replace('void LLRenderTarget::bindTarget(){bound=this;}',
        'void LLRenderTarget::bindTarget(){bound=this;}void LLRenderTarget::flush(){bound=nullptr;}')
    prefix=prefix.replace('namespace LLRender{enum{TRIANGLES};}',
        'namespace LLRender{enum{TRIANGLES,BT_ALPHA};enum eMatrixMode{MM_PROJECTION,MM_MODELVIEW};static bool s10bitBackBuffer=false;}struct LLMatrix4{};')
    prefix=prefix.replace('struct GL{void color4f',
        'struct GL{LLRender::eMatrixMode getMatrixMode(){return LLRender::MM_MODELVIEW;}void matrixMode(LLRender::eMatrixMode){}void pushMatrix(){}void popMatrix(){}void loadMatrix(const float*){}void setSceneBlendType(int){}void color4f')
    prefix=prefix.replace('struct Window{bool getUIVisibility()',
        'struct Rect{int mLeft=0,mBottom=0;int getWidth(){return 64;}int getHeight(){return 48;}};struct Window{Rect getWorldViewRectRaw(){return {};}bool getUIVisibility()')
    prefix=prefix.replace('enum {RLV_BHVR_SETSPHERE,ROTOINK_LAYER_OVERLAY};','enum {RLV_BHVR_SETSPHERE,ROTOINK_LAYER_OVERLAY,ROTOINK_LAYER_CAMERA,ROTOINK_LAYER_ATMOSPHERE};')
    prefix=prefix.replace('void bind(){events.push_back(1000+id);}',
        'void bind(){events.push_back(1000+id);if(id==900||id==901)events.push_back(id-810);}')
    prefix=prefix.replace('struct LLGLSLShader {','struct LLGLSLShader {\n template<class... T>void uniform1ui(T...){++uniforms;}\n template<class... T>void uniform3fv(T...){++uniforms;}')
    prefix=prefix.replace(' enum{RENDER_DEBUG_FEATURE_UI};',' enum{RENDER_DEBUG_FEATURE_UI,RENDER_DEBUG_PHYSICS_SHAPES};')
    prefix=prefix.replace('LLRenderTarget screen{10,64,48,true},',
        'LLRenderTarget rigRimWrapScene,rigRimSmooth[2],rigRimAux,rigRimSil[2],areaVisibility[2],screen{10,64,48,true},')
    prefix=prefix.replace(' void assertInitialized()',r'''
 bool mRigRimWrapSceneValid=false,mRigRimAuxValid=false,mRigRimSilValid=false,mRigRimSmoothValid=false,mSilCoverValid=false;
 int mRigRimSubjectSource=0,mAreaShadowCount=0;
 LLRenderTarget mSceneMap,mLuminanceMap,mExposureMap;
 bool hasRenderDebugMask(int){return false;}bool areaShadowsActive(){return false;}
 void copyScreenSpaceReflections(LLRenderTarget*,LLRenderTarget*,LLRenderTarget*){}
 void renderWeather(LLRenderTarget*){events.push_back(21);}
 void renderRigRimWrap(bool){}void updateNightMaskAnchor(){}
 void generateLuminance(LLRenderTarget*,LLRenderTarget*){}void generateExposure(LLRenderTarget*,LLRenderTarget*){}
 void renderTronWorld(LLRenderTarget*,int,bool,const float*,const float*){}
 void applyOnLensFilters(LLRenderTarget*){}void applyLensSoftEdge(LLRenderTarget*){}void generateBloomHDR(LLRenderTarget*){}
 void renderSilCover(int){}void renderFroxelVolumetrics(LLRenderTarget*){}void renderProjectorVolumetric(LLRenderTarget*){}
 void feedProjectorVolumetricBloom(){}void generateGlow(LLRenderTarget*){}void combineGlow(LLRenderTarget*,LLRenderTarget*){}
 void visualizeBuffers(LLRenderTarget*,LLRenderTarget*,int){}void renderVelocityDebug(LLRenderTarget*){}
 void renderCompositionGuideOverlay(){}void renderFocusPoint(){}void renderPhysicsDisplay(){}void recordTrianglesDrawn(){}
 void assertInitialized()''')
    extra=r'''
#define LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE(x)
#define LL_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define GL_VIEWPORT 0
template<class T>T llmin(T a,T b){return std::min(a,b);}
static bool gSnapshotNoPost=false,BDMergeFroxelVolumetrics=false,BDMergeProjectorVolumetrics=false;
static bool BDMergeVelocityBuffer=false,BDMergeVelocityDebug=false;
static int RenderBufferVisualization=-1;
static float gGLLastProjection[16],gGLLastModelView[16],gGLProjection[16],gGLModelView[16];
static int gGLViewport[4];static const LLMatrix4* gGLLastMatrix=nullptr;
static const unsigned SL_SIDECAR_ATTACHMENT=1,SL_COVERAGE_ATTACHMENT=2,ALR_RIM_PARITY_ATTACHMENT=3;
static int TRON_DRAW_WORLD_AND_TRAILS=0,TRON_DRAW_WORLD_ONLY=1,TRON_DRAW_TRAILS_ONLY=2;
float bdmerge_snapshot_autoscale_multiplier(){return 1;}
void glGetIntegerv(int,int*){}void glViewport(int,int,int,int){}
struct LLScopedIndexedDrawBufferGuard{LLScopedIndexedDrawBufferGuard(bool,const unsigned*,unsigned){}};
namespace ALAvatarLightResponse{static unsigned sSilFrameMask=0;}
namespace ALTron{enum{LAYER_CAMERA};void updateTrails(){}void resolveFrame(){}bool isActiveForCurrentPass(){return false;}int layer(){return 0;}bool worldHasWork(){return false;}bool trailsWanted(){return false;}}
namespace ALLensLooks{void resolveFrame(){}}
LLGLSLShader gRigRimDebugProgram{800},gFroxelMediaProgram{801},gDeferredProjectorVolumetricProgram{802},
 gUltimateKaleidoProgram{803},gUltimateDiopterProgram{804},gAreaShadowFilterProgram{805},
 gSensorBlitProgram{900},gBlitWithEffectsProgram{901};
'''
    return prefix.replace('struct LLPipeline {',extra+'\nstruct LLPipeline {')
