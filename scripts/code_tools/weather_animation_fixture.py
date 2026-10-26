"""Execute the complete production texture-animation method with viewer services.

Face/VBO/matrix services are inert (zero faces); the running animation event and
its production weather invalidation are real. GL tests separately change the
caster's alpha frame and observe the actual published cover texel.
"""
from weather_compute_fixture import function


def extend(tree, prefix, methods):
    regional='mWeatherPrecipitationCoverCache.schedule(' in methods
    event=('dirtyWeatherRegion(LLVector3(-1,-5,0),LLVector3(9,5,8));' if regional
           else '++mWeatherGeometryRevision;')
    prefix=prefix.replace('struct LLPipeline {','struct TextureDrawable;\nstruct LLPipeline {')
    prefix=prefix.replace(' void generateWeatherSurfaceCover(LLCamera&);', '''
 void dirtyWeatherDrawable(TextureDrawable*){EVENT}
 void markTextured(TextureDrawable*){EVENT}
 template<class... T>void markRebuild(T...){}
 void generateWeatherSurfaceCover(LLCamera&);'''.replace('EVENT',event))
    prefix=prefix.replace('bool isFinite()const{return std::isfinite(mV[0])',
        'void set(LLVector3 p){*this=p;}void set(float x,float y,float z){mV[0]=x;mV[1]=y;mV[2]=z;}bool isFinite()const{return std::isfinite(mV[0])')
    prefix+=r'''
using U8=unsigned char;
constexpr float MIN_TEX_ANIM_SIZE=1;
struct LLQuaternion {void setQuat(float,int,int,float){}};
struct LLMatrix4 {
 void setIdentity(){}void translate(LLVector3){}void rotate(LLQuaternion){}
 void initAll(LLVector3,LLQuaternion,LLVector3){}void operator*=(const LLMatrix4&){}
};
struct LLTextureEntry {void getRotation(float*)const{}void getOffset(float*,float*)const{}void getScale(float*,float*)const{}};
struct LLSpatialGroup {void dirtyGeom(){}};
struct LLFace {LLMatrix4* mTextureMatrix=nullptr;float getVirtualSize(){return 0;}const LLTextureEntry* getTextureEntry(){return nullptr;}};
struct LLDrawable {enum {REBUILD_TCOORD=1};};
struct TextureDrawable {int getNumFaces(){return 0;}LLFace* getFace(int){return nullptr;}LLSpatialGroup* getSpatialGroup(){return nullptr;}};
struct LLViewerTextureAnim {
 enum {ROTATE=1,TRANSLATE=2,SCALE=4};float mRate=1,mOffS=0,mOffT=0,mScaleS=1,mScaleT=1,mRot=0;int mMode=0,mFace=-1;
 int animateTextures(float&,float&,float&,float&,float&){return TRANSLATE;}void advanceSteppedTime(){}
};
struct ALSteppedAnimRuntime {inline static bool sStepTextureAnim=false;static ALSteppedAnimRuntime& instance(){static ALSteppedAnimRuntime r;return r;}bool holdsTextureAnim(int){return false;}};
namespace ALProbeSched {enum {R_TEX};enum class Motion {TEXANIM};}
namespace ALProbeDirty {enum {TAG_TEXANIM_TOGGLE};bool recording(){return false;}template<class... T>void noteDrawable(T...){}struct ScopedTag {ScopedTag(int){}};}
auto& gPipeline=fixturePipeline;
struct LLVOVolume {
 bool mDead=false,mFaceMappingChanged=false;int mTexAnimMode=0;
 TextureDrawable drawable;TextureDrawable* mDrawable=&drawable;LLViewerTextureAnim anim;LLViewerTextureAnim* mTextureAnimp=&anim;
 void shrinkWrap(){}bool isAttachment(){return false;}bool isHUDAttachment(){return false;}int getAvatar(){return 0;}
 int getNumTEs(){return 0;}void setTEOffset(int,float,float){}void setTEScale(int,float,float){}void setTERotation(int,float){}
 void animateTextures();
};
'''
    methods+='\n'+function(tree.read('indra/newview/llvovolume.cpp'),'void LLVOVolume::animateTextures(')
    methods+=r'''
extern "C" __declspec(dllexport) void cover_animate(float alpha){static LLVOVolume volume;roofAlpha=alpha;volume.animateTextures();}
extern "C" __declspec(dllexport) unsigned cover_texture(){return coverage.mCover[coverage.mRead].texture;}
'''
    return prefix,methods
