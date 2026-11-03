// External viewer/GL seams for complete production drawGeometryGhost execution.
// Geometry submission and state/uniforms are recorded, then consumed by driver fixtures.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
using U8=unsigned char;using U32=unsigned;using U64=unsigned long long;using S32=int;using F32=float;using GLfloat=float;using GLint=int;
enum{VX,VY,VZ,VW};
template<class T>T llmin(T a,T b){return std::min(a,b);}template<class T>T llmax(T a,T b){return std::max(a,b);}template<class T>T llclamp(T x,T a,T b){return std::clamp(x,a,b);}bool llfinite(float x){return std::isfinite(x);}
struct LLUUID{int id=0;bool notNull()const{return id!=0;}bool operator<(const LLUUID& x)const{return id<x.id;}static const LLUUID null;};const LLUUID LLUUID::null{};
struct LLVector2{float mV[2];LLVector2(float x=0,float y=0):mV{x,y}{}};
struct LLVector3{float mV[3];LLVector3(float x=0,float y=0,float z=0):mV{x,y,z}{}bool isFinite()const{return true;}};
struct LLColor3{float mV[3];LLColor3(float x=0,float y=0,float z=0):mV{x,y,z}{}};
struct LLVector4{float mV[4];LLVector4(float x=0,float y=0,float z=0,float w=0):mV{x,y,z,w}{}void set(float x,float y,float z,float w){*this={x,y,z,w};}};
using LLColor4=LLVector4;
struct LLQuaternion{bool isIdentity()const{return true;}};
struct LLMatrix4{float mMatrix[4][4]{};LLMatrix4()=default;LLMatrix4(LLQuaternion){}};
template<class T>struct LLPointer{T* p=nullptr;LLPointer()=default;LLPointer(T* x):p(x){}bool isNull()const{return !p;}bool notNull()const{return p;}T* get()const{return p;}T* operator->()const{return p;}operator T*()const{return p;}};
using LLStaticHashedString=std::string;
constexpr int GL_TRUE=1,GL_FALSE=0,GL_BLEND=1,GL_CULL_FACE=2,GL_LESS=3,GL_LEQUAL=4,GL_POLYGON_OFFSET_FILL=5,GL_FRONT_AND_BACK=6,GL_LINE=7,GL_FILL=8;
#define LL_PROFILE_GPU_ZONE(x)
void glPolygonOffset(float,float){}void glPolygonMode(int,int){}
struct LLRender{enum eBlendFactor{BF_ZERO,BF_ONE,BF_SOURCE_ALPHA,BF_ONE_MINUS_SOURCE_ALPHA};enum{BT_ALPHA,BT_ADD,TRIANGLES,MM_TEXTURE,MM_MODELVIEW};};
struct DrawState{bool blend=false,depth=false,write=true,colour=true;LLRender::eBlendFactor cs=LLRender::BF_SOURCE_ALPHA,cd=LLRender::BF_ONE_MINUS_SOURCE_ALPHA,as=LLRender::BF_SOURCE_ALPHA,ad=LLRender::BF_ONE_MINUS_SOURCE_ALPHA;int func=GL_LESS;};
DrawState state;std::map<int,bool> enabled;
struct LLGLDisable{int cap;bool old;LLGLDisable(int c):cap(c),old(enabled[c]){if(c)enabled[c]=false;}~LLGLDisable(){if(cap)enabled[cap]=old;}};
struct LLGLEnable{int cap;bool old;LLGLEnable(int c):cap(c),old(enabled[c]){if(c)enabled[c]=true;}~LLGLEnable(){if(cap)enabled[cap]=old;}};
struct LLGLDepthTest{DrawState old;LLGLDepthTest(bool test,bool write=true,int func=GL_LESS):old(state){state.depth=test;state.write=write;state.func=func;}~LLGLDepthTest(){state.depth=old.depth;state.write=old.write;state.func=old.func;}};
struct LLGLSUIDefault{};
struct LLViewerTexture{};struct LLViewerFetchedTexture:LLViewerTexture{inline static LLViewerTexture* sWhiteImagep=nullptr;};
struct LLTexUnit{enum{TT_TEXTURE};void bind(LLViewerTexture*){}void activate(){}void unbind(int){}};
struct LLGLSLShader{
 int id=1;U32 mProgramObject=1,mAttributeMask=0;LLGLSLShader* mRiggedVariant=nullptr;
 inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;inline static int sIndexedTextureChannels=4;
 std::map<std::string,std::vector<float>> u;
 LLGLSLShader(int n=1):id(n){}
 void bind(){sCurBoundShaderPtr=this;}static void unbind(){sCurBoundShaderPtr=nullptr;}
 void uniform1f(std::string n,float x){u[n]={x};}void uniform1i(std::string n,int x){u[n]={(float)x};}
 void uniform2f(std::string n,float x,float y){u[n]={x,y};}void uniform4f(std::string n,float x,float y,float z,float w){u[n]={x,y,z,w};}
 void uniform1fv(std::string n,int k,const float* p){u[n]={p,p+k};}void uniform1iv(std::string n,int k,const int* p){u[n]=std::vector<float>(p,p+k);}
 void uniform2fv(std::string n,int k,const float* p){u[n]={p,p+k*2};}void uniform3fv(std::string n,int k,const float* p){u[n]={p,p+k*3};}void uniform4fv(std::string n,int k,const float* p){u[n]={p,p+k*4};}
 void uniformMatrix3x4fv(int,int,bool,const float*){}void vertexAttrib4f(int,float,float,float,float){}
};
struct Trace{int program,vb;DrawState state;bool blend;std::map<std::string,std::vector<float>> uniforms;};std::vector<Trace> trace;
struct LLVertexBuffer{enum{TYPE_COLOR,TYPE_TEXTURE_INDEX,MAP_COLOR=16};int id=1;U32 getTypeMask(){return 0xffffffffu;}bool hasDataType(int){return false;}void setBuffer(){}void drawRange(int,int,int,int,int){auto* sh=LLGLSLShader::sCurBoundShaderPtr;assert(sh);trace.push_back({sh->id,id,state,enabled[GL_BLEND],sh->u});}};
struct GL{LLTexUnit unit;LLTexUnit* getTexUnit(int){return &unit;}
 void diffuseColor4f(float x,float y,float z,float w){auto* sh=LLGLSLShader::sCurBoundShaderPtr;if(sh)sh->uniform4f("color",x,y,z,w);}void diffuseColor4fv(const float* p){diffuseColor4f(p[0],p[1],p[2],p[3]);}
 void setColorMask(bool c,bool){state.colour=c;}void setSceneBlendType(int t){state.cs=t==LLRender::BT_ADD?LLRender::BF_ONE:LLRender::BF_SOURCE_ALPHA;state.cd=t==LLRender::BT_ADD?LLRender::BF_ONE:LLRender::BF_ONE_MINUS_SOURCE_ALPHA;state.as=state.cs;state.ad=state.cd;}
 void blendFunc(LLRender::eBlendFactor a,LLRender::eBlendFactor b,LLRender::eBlendFactor c,LLRender::eBlendFactor d){state.cs=a;state.cd=b;state.as=c;state.ad=d;}
 void getBlendFunc(LLRender::eBlendFactor& a,LLRender::eBlendFactor& b,LLRender::eBlendFactor& c,LLRender::eBlendFactor& d){a=state.cs;b=state.cd;c=state.as;d=state.ad;}
 void matrixMode(int){}void loadIdentity(){}void loadMatrix(float*){}void multMatrix(float*){}void pushMatrix(){}void popMatrix(){}void syncMatrices(){}void translatef(float,float,float){}void scalef(float,float,float){}
}gGL;
struct Root{LLVector3 getWorldPosition(){return {};}};
struct LLVOAvatar{Root root;bool isDead(){return false;}Root* getRootJoint(){return &root;}float getPelvisToFoot(){return 0;}LLUUID getID(){return {1};}};
struct LLGLTFMaterial{enum{GLTF_TEXTURE_INFO_BASE_COLOR,GLTF_TEXTURE_INFO_EMISSIVE};struct TextureTransform{};TextureTransform mTextureTransform[2];LLColor4 mBaseColor{1,1,1,1};LLColor3 mEmissiveColor{.3f,.5f,.7f};bool mDoubleSided=false;virtual ~LLGLTFMaterial()=default;};
struct LLFetchedGLTFMaterial:LLGLTFMaterial{LLPointer<LLViewerTexture>mBaseColorTexture,mEmissiveTexture;};
struct LLTextureEntry{LLGLTFMaterial* getGLTFRenderMaterial()const{return nullptr;}};
struct LLDrawInfo{LLPointer<LLVertexBuffer>mVertexBuffer;LLPointer<LLVOAvatar>mAvatar;LLPointer<int>mSkinInfo;LLPointer<LLFetchedGLTFMaterial>mGLTFMaterial;std::vector<LLPointer<LLFetchedGLTFMaterial>>mGLTFMaterialList;
 struct MaterialSlot{LLPointer<LLViewerTexture>mDiffuse;float mAlphaMaskCutoff=.5f;};std::vector<MaterialSlot>mMaterialSlotList;std::vector<LLPointer<LLViewerTexture>>mTextureList;
 LLUUID mActorFxOwner,mActorFxFallbackOwner;int mStart=0,mEnd=2,mCount=3,mOffset=0;LLMatrix4* mTextureMatrix=nullptr;U64 getSkinHash(){return 1;}};
struct LLFace{LLDrawInfo* mDrawInfo=nullptr;LLMatrix4* mTextureMatrix=nullptr;LLVertexBuffer* vb=nullptr;LLVertexBuffer* getVertexBuffer(){return vb;}const LLTextureEntry* getTextureEntry(){return nullptr;}LLViewerTexture* getTexture(){return nullptr;}bool isInAlphaPool(){return false;}LLMatrix4 getRenderMatrix(){return {};}int getGeomIndex(){return 0;}int getGeomCount(){return 3;}int getIndicesCount(){return 3;}int getIndicesStart(){return 0;}};
struct CoverPolicy{bool sharedCover(LLUUID,LLUUID,LLUUID,LLVector2&)const{return false;}};
using GhostCoverageMask=unsigned;constexpr unsigned GHOST_COVERAGE_NONE=0,GHOST_COVERAGE_RIGGED_SOLID=1,GHOST_COVERAGE_RIGGED_BLEND=2,GHOST_COVERAGE_RIGGED_GLOW=4,GHOST_COVERAGE_STATIC_SOLID=8,GHOST_COVERAGE_STATIC_BLEND=16,GHOST_COVERAGE_ALL=31;
struct LLActorMover{struct GhostBatch{LLDrawInfo* mInfo=nullptr;int mPass=0;};struct GhostStaticFace{LLFace* mFace=nullptr;int mAlphaKind=0;float mCutoff=.5f;bool mDoubleSided=false;LLUUID mObjectId;};
// GP_INSERT
};
struct LLDirectorCast{enum{ACTOR_LOOK_RIM_NOIR=28,ACTOR_LOOK_GEL_SPLIT,ACTOR_LOOK_BASS_SWEEP,ACTOR_LOOK_MOONLIT,ACTOR_LOOK_POSSESSED,ACTOR_LOOK_SERAPH,ACTOR_LOOK_INTERROGATION,ACTOR_LOOK_WRAITH,ACTOR_LOOK_TRON_SUIT};static bool isLiveActorStyleLook(int look){return look>=28&&look<64;}};
struct LLRenderPass{enum{ALR_SLOT_BEAUTY,ALR_SLOT_GLOW,ALR_SLOT_SIL_ONLY};static float actorFxFrameTime(float){return 2;}static LLVector2 actorFxFragOffset(){return {};}static LLVector2 actorFxScreenSize(){return {256,256};}static bool actorFxLookNeedsSyntheticBloom(int){return false;}
 static bool uploadMatrixPalette(LLPointer<LLVOAvatar>,LLPointer<int>,const LLVOAvatar*&,U64&,bool&){return true;}static void uploadAvatarLightResponseTo(LLGLSLShader& sh,LLUUID,LLUUID,LLUUID,int){sh.uniform1i("alrEnabled",0);}
};struct LLViewerShaderMgr{enum{AVATAR_MATRIX};};struct LLDrawPoolAlpha{};
struct Pipeline{void bindDeferredShaderFast(LLGLSLShader& sh){sh.bind();}}gPipeline;
struct ScopedSharedActorHDRDrawBuffer{explicit ScopedSharedActorHDRDrawBuffer(bool){}};struct ScopedStudioTexture{explicit ScopedStudioTexture(int){}};
struct StudioLookFrame{bool scene=false;};
void studio_apply_phase2(LLGLSLShader* sh,const StudioLookFrame* f,float d){sh->uniform1i("ghostSceneValid",f&&f->scene);sh->uniform1f("ghostDisplace",d);}
template<class D>void studio_draw_displaced(int look,bool studio,float alpha,bool world,bool fx,D draw){if(studio&&alpha>0&&!world&&fx&&(look==68||look==72))draw(look==68?1.f:2.f);}
template<class A>void studio_reset_loaded(LLGLSLShader* a,LLGLSLShader* b,A apply){apply(a);apply(b);}
void upload_ghost_tron_params(LLGLSLShader&,bool){}int actorMaskTextureChannels(){return 4;}LLVector2 actor_ghost_frag_offset(){return {};}float actor_ghost_studio_frame_time(float){return 2;}
constexpr float GHOST_SCALE_MIN=.01f,GHOST_SCALE_MAX=100;bool gUseWireframe=false;
constexpr int GHOST_CUTOFF_NONE=0,GHOST_CUTOFF_TEXTURE_VERTEX_ALPHA=2;
bool ghost_pass_is_blend(int p){return p==1;}bool ghost_pass_is_glow(int p){return p==2;}bool ghost_pass_is_mask(int p){return p==3;}
int ghost_legacy_indexed_slot_count(LLDrawInfo*){return 1;}int ghost_batch_slot_count(LLDrawInfo*){return 1;}int ghost_batch_alpha_cutoff_mode(int){return 2;}int ghost_static_alpha_cutoff_mode(const LLActorMover::GhostStaticFace&){return 2;}
LLViewerTexture* ghost_batch_texture(LLDrawInfo*){static LLViewerTexture tex;return &tex;}float ghost_batch_cutoff(LLDrawInfo*,int p){return p==3?.5f:0.f;}
LLViewerTexture* ghost_batch_slot_texture(LLDrawInfo*,int,bool,float&,LLColor4&){return nullptr;}bool ghost_compose_khr_uv(const LLGLTFMaterial::TextureTransform&,LLMatrix4&){return false;}bool ghost_batch_uv_matrix(LLDrawInfo*,LLMatrix4&){return false;}
LLGLSLShader gActorGhostProgram{1},gSkinnedActorGhostProgram{2},gWorldActorGhostProgram{5},gWorldSkinnedActorGhostIndexedProgram{6},gHighlightProgram{7},gHighlightSkin{8},gSilCoverProgram[2];
enum{ACTOR_MASK_RIGGED,ACTOR_MASK_STATIC};struct KeyPrograms{LLGLSLShader gActorGhostKeyFillProgram{3},gSkinnedActorGhostKeyFillProgram{4};};auto sStudioKeyPrograms=std::make_unique<KeyPrograms>();

// The legacy parity fixture is deliberately Key OFF. WP1b has its own armed path fixture.
struct ALActorKeyCapture {inline static bool sArmed=false;static ALActorKeyCapture& instance(){static ALActorKeyCapture x;return x;}template<class... T>void capture(T...){}};
