"""Compile production cover preparation/state/publication with scene services.

The native renderShadow entry delegates to simulated AABB culling/render-map
sorting over 256 distinct world rectangles, rasterized on real GL. The viewer's
octree, spatial bridges and VBO pools are not timed by this standalone harness.
"""
import re
from weather_compute_fixture import function


def parts(tree, out):
    (out/'alweathercovercache.h').write_text(tree.read('indra/newview/alweathercovercache.h'), encoding='utf-8')
    budgeted = 'beginTile' in tree.read('indra/newview/alweatherresources.h')
    if budgeted:
        (out/'lldrawbudget.h').write_text(tree.read('indra/llrender/lldrawbudget.h'), encoding='utf-8')
    pipe = tree.read('indra/newview/pipeline.cpp')
    cover = function(pipe, 'void LLPipeline::generateWeatherSurfaceCover(')
    draw = function(pipe, 'bool LLPipeline::drawWeatherCover(') if 'bool LLPipeline::drawWeatherCover(' in pipe else ''
    advanced = function(pipe, 'void LLPipeline::generateWeatherAdvancedShelter(') if 'void LLPipeline::generateWeatherAdvancedShelter(' in pipe else ''
    render_types = sorted(set(re.findall(r'LLPipeline::(RENDER_TYPE_\w+)', cover+draw)))
    passes = sorted(set(re.findall(r'LLRenderPass::(PASS_\w+)', cover+draw)))
    prefix = r'''
#include "alweathercovercache.h"
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE
#define LL_PROFILE_GPU_ZONE(x)
constexpr int VX=0,VY=1,VZ=2;
constexpr unsigned GL_DEPTH_CLAMP=0x864F,GL_LESS=0x0201,GL_DEPTH_BUFFER_BIT=0x100;
U32 gFrameCount=1;bool gCubeSnapshot=false;
struct LLVector3 {float mV[3]{};LLVector3()=default;LLVector3(float x,float y,float z):mV{x,y,z}{}
 bool isFinite()const{return std::isfinite(mV[0])&&std::isfinite(mV[1])&&std::isfinite(mV[2]);}
 inline static const LLVector3 zero;};
const LLVector3 LLVector3::zero{};
struct LLVector3d {double mdV[3]{};bool isFinite()const{return std::isfinite(mdV[0])&&std::isfinite(mdV[1])&&std::isfinite(mdV[2]);}};
struct Agent {LLVector3d origin{};LLVector3d getPosGlobalFromAgent(const LLVector3& p){return {{origin.mdV[0]+p.mV[0],origin.mdV[1]+p.mV[1],origin.mdV[2]+p.mV[2]}};}
 LLVector3 getPosAgentFromGlobal(const LLVector3d& p){return {float(p.mdV[0]-origin.mdV[0]),float(p.mdV[1]-origin.mdV[1]),float(p.mdV[2]-origin.mdV[2])};}} gAgent;
struct LLCamera {LLVector3 eye;LLVector3 getOrigin()const{return eye;}void setOrigin(LLVector3 p){eye=p;}
 void lookDir(LLVector3,LLVector3){}void setNear(float){}void setFar(float){}};
struct LLViewerCamera {enum eCameraID {CAMERA_WORLD,CAMERA_WEATHER_RAIN_OCCLUSION};inline static eCameraID sCurCameraID=CAMERA_WORLD;
 static void updateFrustumPlanes(LLCamera&,bool,bool,bool){}};
glm::mat4 currentView,currentProjection,lastView,lastProjection;
glm::mat4 get_current_modelview(){return currentView;}glm::mat4 get_current_projection(){return currentProjection;}
glm::mat4 get_last_modelview(){return lastView;}glm::mat4 get_last_projection(){return lastProjection;}
void set_current_modelview(const glm::mat4& m){currentView=m;}void set_current_projection(const glm::mat4& m){currentProjection=m;}
void set_last_modelview(const glm::mat4& m){lastView=m;}void set_last_projection(const glm::mat4& m){lastProjection=m;}
int gGLViewport[4]{0,0,512,512};
template<class T>T llclamp(T a,T b,T c){return std::clamp(a,b,c);}
namespace glm {
 mat4 operator*(const mat4& a,const mat4& b){mat4 r(0);for(int i=0;i<4;++i)r[i]=a*b[i];return r;}
 mat4 ortho(float l,float r,float b,float t,float n,float f){mat4 m(1);m[0][0]=2/(r-l);m[1][1]=2/(t-b);m[2][2]=-2/(f-n);m[3]={-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n),1};return m;}
 mat4 make_mat4(const float* p){mat4 m;std::memcpy(&m,p,64);return m;}
}
glm::mat4 look(LLVector3 eye,LLVector3,LLVector3){glm::mat4 m(1);m[3]={-eye.mV[0],-eye.mV[1],-eye.mV[2],1};return m;}
struct LLViewerShaderMgr {static LLViewerShaderMgr* instance(){static LLViewerShaderMgr s;return &s;}bool weatherMaterialsEnabled(){return true;}};
struct ShadowProgram {bool isComplete()const{return true;}} gDeferredShadowProgram,gDeferredShadowAlphaMaskProgram,
 gDeferredShadowFullbrightAlphaMaskProgram,gDeferredTreeShadowProgram,gDeferredShadowGLTFAlphaMaskProgram,gDeferredShadowCubeProgram;
struct LLCullResult {U32 getRenderMapSize(U32 type){return type==0?256:0;}};
struct LLFacePool {std::vector<int> mDrawFace;bool isFacePool(){return true;}};
struct LLRenderPass {enum {PASSES};};
unsigned coverTraversals=0,coverBatches=0;GLuint casterProgram=0;
struct LLColor3 {float mV[3]{};LLColor3()=default;LLColor3(float x):mV{x,x,x}{};static const LLColor3 black;};
const LLColor3 LLColor3::black{};
struct LLColor4 {float mV[4]{};};
LINEAR_COLOUR
struct Sky {bool sun=true,moon=false;LLColor3 diffuse{1.055f*std::pow(10.f/3.f,1.f/2.4f)-0.055f},moonDiffuse{};LLColor4 ambient{};
 bool getIsSunUp(){return sun;}bool getIsMoonUp(){return moon;}LLColor3 getSunDiffuse(){return diffuse;}
 LLColor3 getMoonDiffuse(){return moonDiffuse;}LLColor4 getTotalAmbient(){return ambient;}};
struct LLEnvironment {Sky sky;static LLEnvironment& instance(){static LLEnvironment e;return e;}Sky* getCurrentSky(){return &sky;}};
float gGLLastModelView[16],gGLLastProjection[16];
struct LLPipeline {
 enum {@RENDER_TYPES@,END_RENDER_TYPES};static constexpr bool sRenderDeferred=true;inline static unsigned char sShadowDrawFilter=0;
 ALWeatherResources& mWeatherSurfaces=coverage;ALWeatherParticles& mWeatherParticles=particles;
 ALWeatherCoverCache mWeatherPrecipitationCoverCache;U64 mWeatherGeometryRevision=0;
 std::map<int,LLFacePool*> mTerrainPools;
 struct Targets {LLRenderTarget deferredScreen;} targets;Targets* mRT=&targets;
 void pushRenderTypeMask(){}template<class... T>void andRenderTypeMask(T...){}void popRenderTypeMask(){}
 void renderShadow(const glm::mat4&,const glm::mat4&,LLCamera&,LLCullResult&,bool,bool,bool,bool,bool){
  ++coverTraversals;glUseProgram(casterProgram);for(int i=0;i<256;++i){glDrawArrays(GL_TRIANGLES,0,3);++coverBatches;}glUseProgram(0);}
 DECLARE_SHELTER
 void generateWeatherSurfaceCover(LLCamera&);void renderWeatherParticles(LLRenderTarget*,bool);
} fixturePipeline;
'''.replace('PASSES', ','.join(passes)).replace('@RENDER_TYPES@', ','.join(render_types))
    prefix = prefix.replace('inline static const LLVector3 zero;', 'static const LLVector3 zero;')
    conversion = 'constexpr int VRED=0,VGREEN=1,VBLUE=2;\n'
    conversion += function(tree.read('indra/llmath/llmath.h'),'inline float sRGBtoLinear(')
    colors=tree.read('indra/llmath/v3color.h')
    conversion += function(colors,'inline const LLColor3 linearColor3p(')
    conversion += '\ntemplate<class T>\n'+function(colors,'inline const LLColor3 linearColor3(const T& a)')
    prefix = prefix.replace('LINEAR_COLOUR',conversion)
    prefix=prefix.replace('DECLARE_SHELTER', '''
 ALWeatherShelterMaps mWeatherShelter;
 bool drawWeatherCover(LLCamera&,LLRenderTarget&,const glm::mat4&,const glm::mat4&,const LLVector3&,F32,F32,U32&);
 void generateWeatherAdvancedShelter(LLCamera&);
''' if advanced else '')
    if advanced:
        prefix += '''
struct CoverSettings {} gSavedSettings;
template<class T>struct LLCachedControl {T value;LLCachedControl(CoverSettings&,const char*,T v):value(v){}T operator()()const{return value;}};
struct LLPresentationTime {struct Frame {double presentation_time;};inline static double seconds=0;
 static Frame currentFrame(){return {seconds};}};
'''
    regional = 'mWeatherPrecipitationCoverCache.schedule(' in cover
    if regional:
        prefix=prefix.replace('ALWeatherCoverCache mWeatherPrecipitationCoverCache;U64 mWeatherGeometryRevision=0;',
            'ALWeatherShelter::Updates mWeatherPrecipitationCoverCache;U32 mWeatherCoverWorkFrame=~U32(0);')
        prefix=prefix.replace('F32,F32,U32&);','F32,F32,U32&,ALWeatherShelter::Updates::Tile={},ALWeatherResources* = nullptr);' if budgeted else 'F32,F32,U32&,ALWeatherShelter::Updates::Tile={});')
        prefix=prefix.replace(' void generateWeatherSurfaceCover(LLCamera&);', '''
 void dirtyWeatherRegion(const LLVector3&,const LLVector3&);
 void generateWeatherSurfaceCover(LLCamera&);''')
        cover += '\n'+function(pipe,'void LLPipeline::dirtyWeatherRegion(')
    if 'LLSettingsVOSky::illumination' in pipe:
        # Native illumination helper, with sky/settings services substituted.
        prefix=prefix.replace('bool getIsSunUp()', 'bool getIsSunUp()const').replace('bool getIsMoonUp()', 'bool getIsMoonUp()const')
        for name in ('getSunDiffuse','getMoonDiffuse','getTotalAmbient'):
            prefix=prefix.replace(name+'(){',name+'()const{')
        prefix=prefix.replace('struct Sky {', 'struct Sky {bool classic=false;float ambiance=1;bool canAutoAdjust()const{return classic;}float getReflectionProbeAmbiance(bool)const{return ambiance;}')
        prefix=prefix.replace('struct CoverSettings {} gSavedSettings;', '''
struct CoverSettings {std::map<std::string,double> values;} gSavedSettings;''')
        prefix=prefix.replace('T value;LLCachedControl(CoverSettings&,const char*,T v):value(v){}T operator()()const{return value;}',
            'T value;std::string key;LLCachedControl(CoverSettings&,const char* k,T v):value(v),key(k){}T operator()()const{auto it=gSavedSettings.values.find(key);return it==gSavedSettings.values.end()?value:T(it->second);}')
        prefix+='''
using LLSettingsSky=Sky;
namespace ALEnvIntensity {
 struct Factors {float mSunMul=1,mAmbMul=1;LLColor3 mSunTint{1},mAmbTint{1};};
 Factors supplied;bool isSamplingCapture(){return false;}
 Factors compute(bool classic,bool legacy,bool,bool){auto f=classic?Factors{}:supplied;if(legacy)f.mSunMul=std::min(f.mSunMul,1.f);return f;}
}
struct LLSettingsVOSky {
 struct Illumination {LLColor3 ambient,direct;float sunlightScale=1,ambientScale=1;ALEnvIntensity::Factors factors;bool classic=false;};
 static Illumination illumination(const Sky&);
 static Illumination rawIllumination(const Sky& s){auto r=illumination(s);r.direct=linearColor3(s.getSunDiffuse());return r;}
};
'''
        prefix+='\n'+function(tree.read('indra/newview/llsettingsvo.cpp'),'LLSettingsVOSky::Illumination LLSettingsVOSky::illumination(')+'\n'
    methods = cover + '\n' + draw + '\n' + advanced + '\n' + function(pipe, 'void LLPipeline::renderWeatherParticles(')
    api = r'''
extern "C" __declspec(dllexport) void cover_program(unsigned p){casterProgram=p;}
extern "C" __declspec(dllexport) void cover_prepare(double x,double y,double z,int dirty){
 ++gFrameCount;if(dirty)++fixturePipeline.mWeatherGeometryRevision;
 LLCamera camera;camera.eye={float(x),float(y),float(z)};fixturePipeline.generateWeatherSurfaceCover(camera);
}
extern "C" __declspec(dllexport) void cover_counts(double* out){out[0]=coverTraversals;out[1]=coverBatches;}
extern "C" __declspec(dllexport) void evaluated_sky(int sun,int moon,float ambient,float daylight,float moonlight){
 auto& s=LLEnvironment::instance().sky;s.sun=sun;s.moon=moon;s.ambient={{ambient,ambient,ambient,0}};s.diffuse=LLColor3(daylight);s.moonDiffuse=LLColor3(moonlight);}
'''
    if regional:
        api=api.replace('++gFrameCount;if(dirty)++fixturePipeline.mWeatherGeometryRevision;', '''
 ++gFrameCount;LLPresentationTime::seconds+=1./60;
 if(dirty)fixturePipeline.dirtyWeatherRegion(LLVector3(std::min(oldRoofOffset,roofOffset)-1,-5,0),
  LLVector3(std::max(oldRoofOffset+oldRoofWidth,roofOffset+8.f)+1,5,8));''')
        api+='''
extern "C" __declspec(dllexport) void cover_dirty(double x,double y,double z){
 fixturePipeline.dirtyWeatherRegion(LLVector3(float(x)-.1f,float(y)-.1f,float(z)-.1f),LLVector3(float(x)+.1f,float(y)+.1f,float(z)+.1f));}
'''
        api+='''
extern "C" __declspec(dllexport) void cover_minimum(double* out){
 out[0]=coverage.mMinimum;out[1]=coverage.mMinimumValid;out[2]=coverage.mReduction.isComplete();
 auto tile=fixturePipeline.mWeatherPrecipitationCoverCache.tile();out[3]=double(tile.size*tile.size)/64.;}
'''
    else:
        api+='''
extern "C" __declspec(dllexport) void cover_dirty(double,double,double){++fixturePipeline.mWeatherGeometryRevision;}
'''
    if 'LLSettingsVOSky::illumination' in pipe:
        api+='''
extern "C" __declspec(dllexport) void illumination_setting(const char* name,double v){gSavedSettings.values[name]=v;}
extern "C" __declspec(dllexport) void illumination_factors(float direct,float ambient,float r,float g,float b){
 ALEnvIntensity::supplied.mSunMul=direct;ALEnvIntensity::supplied.mAmbMul=ambient;
 ALEnvIntensity::supplied.mSunTint=LLColor3();ALEnvIntensity::supplied.mAmbTint=LLColor3();
 for(int i=0;i<3;++i){ALEnvIntensity::supplied.mSunTint.mV[i]=i==0?r:i==1?g:b;ALEnvIntensity::supplied.mAmbTint.mV[i]=i==0?r:i==1?g:b;}}
extern "C" __declspec(dllexport) void illumination_mode(int classic,float ambiance){auto& s=LLEnvironment::instance().sky;s.classic=classic;s.ambiance=ambiance;}
'''
    if budgeted:
        api += r'''
extern "C" __declspec(dllexport) unsigned cover_pending(){return fixturePipeline.mWeatherPrecipitationCoverCache.pendingTiles();}
extern "C" __declspec(dllexport) unsigned cover_ready(){return coverage.ready();}
extern "C" __declspec(dllexport) void cover_load(unsigned triangles){sceneVertices=triangles*3;}
extern "C" __declspec(dllexport) void cover_budget(double* out){
 out[0]=double(coverTriangles);out[1]=double(coverage.mRefreshCursor);out[2]=coverage.mRefreshComplete;
 out[3]=fixturePipeline.mWeatherPrecipitationCoverCache.stalenessSeconds();
 out[4]=fixturePipeline.mWeatherPrecipitationCoverCache.oldestSeconds();
}
'''
    # Closest real harness path: compile production drawWeatherCover unchanged,
    # including its cropped native renderShadow call. Scene membership/culling
    # and render-map sorting are simulated over 256 distinct world AABBs; each
    # visible rectangle is rasterized once using a real alpha-mask program.
    # This does not time the viewer octree, spatial bridges or native VBO pools.
    from weather_shelter_fixture import SCENE
    prefix=prefix.replace('struct LLPipeline {', SCENE+'\nstruct LLPipeline {')
    original=function(prefix,' void renderShadow(')
    prefix=prefix.replace(original, ''' void renderShadow(const glm::mat4& v,const glm::mat4& p,LLCamera&,LLCullResult&,bool,bool,bool,bool,bool){sceneDraw(v,p);}''')
    api=api.replace('casterProgram=p;', '''casterProgram=p;
 sceneMVP=glGetUniformLocation(p,"fixture_mvp");sceneRect=glGetUniformLocation(p,"fixture_rect");
 sceneAlpha=glGetUniformLocation(p,"fixture_alpha");sceneZ=glGetUniformLocation(p,"fixture_z");''')
    if budgeted:
        prefix='#include "lldrawbudget.h"\n'+prefix
    from weather_animation_fixture import extend as animation
    prefix,methods=animation(tree,prefix,methods)
    return prefix, methods, api
