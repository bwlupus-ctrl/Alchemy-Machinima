// External scene/avatar seams for the WP1b production-method/WGL harness.
// Core GL, LLRender, shader wrappers and LLVertexBuffer are real Release libraries.
#include "linden_common.h"
#include "llcommon.h"
#include "llgl.h"
#include "llrender.h"
#include "llvertexbuffer.h"
#include "llglslshader.h"
#include "llshadermgr.h"
#include "lltexture.h"
#include "llimagegl.h"
#include "llrendertarget.h"
#include "m4math.h"
#include "v3color.h"
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <iostream>
#include <array>
#include <functional>
#include <map>
#include <set>
#include <unordered_set>
#include "alactorkeycapture.h"
class FixtureShaderMgr:public LLShaderMgr{public:FixtureShaderMgr(){sInstance=this;}std::string getShaderDirPrefix()override{return "";}void updateShaderUniforms(LLGLSLShader*)override{}};
FixtureShaderMgr fixtureShaderMgr;
class LLWindowWin32 {public:static PROC WINAPI getProcAddress(const char*);};
PROC WINAPI LLWindowWin32::getProcAddress(const char* n){PROC p=wglGetProcAddress(n);return p?p:GetProcAddress(GetModuleHandleW(L"opengl32.dll"),n);}
static U32 gFrameCount=42;
struct LLVOAvatar;
struct LLViewerTexture:LLTexture {LLPointer<LLImageGL> image;LLImageGL*getGLTexture()const override{return image;}int getDiscardLevel(){return 0;}void external(GLuint id){image=new LLImageGL(false,false);image->setSize(1,1,4);image->setTexName(id);}};
struct LLGLTFMaterial {virtual ~LLGLTFMaterial()=default;LLColor4 mBaseColor{1,1,1,1};
 enum{GLTF_TEXTURE_INFO_BASE_COLOR,GLTF_TEXTURE_INFO_NORMAL,GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS,GLTF_TEXTURE_INFO_EMISSIVE,ALPHA_MODE_MASK=1};
 struct TextureTransform {using Pack=F32[8];void getPacked(Pack& p){const F32 identity[8]={0,0,1,0,0,0,1,0};std::copy(identity,identity+8,p);}};TextureTransform mTextureTransform[4];
};
struct LLFetchedGLTFMaterial:LLGLTFMaterial,LLRefCount {
 bool mDoubleSided=false;F32 mAlphaCutoff=.2,mRoughnessFactor=.5,mMetallicFactor=0;S32 mAlphaMode=2;
 LLPointer<LLViewerTexture>mBaseColorTexture,mNormalTexture,mMetallicRoughnessTexture,mEmissiveTexture;LLColor3 mEmissiveColor;
 LLGLTFMaterial::TextureTransform mTextureTransform[4];
 void bind(LLViewerTexture*,F32=-2){auto*sh=LLGLSLShader::sCurBoundShaderPtr;for(auto n:{"diffuseMap","bumpMap","specularMap","emissiveMap"}){auto l=sh->getUniformLocation(LLStaticHashedString(n));if(l>=0)glUniform1i(l,0);}gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE,mBaseColorTexture->image->getTexName());}
};
struct LLDrawInfo:LLRefCount {LLPointer<LLRefCount>mMaterial;LLPointer<LLVOAvatar>mAvatar;LLUUID mActorFxOwner,mActorFxFallbackOwner;LLPointer<LLVertexBuffer>mVertexBuffer;LLPointer<LLRefCount>mSkinInfo;LLPointer<LLViewerTexture>mTexture;LLPointer<LLFetchedGLTFMaterial>mGLTFMaterial;std::vector<LLPointer<LLFetchedGLTFMaterial>>mGLTFMaterialList;std::vector<F32>mGLTFAlphaMaskCutoffList;LLMatrix4*mTextureMatrix=nullptr;F32 mAlphaMaskCutoff=.2;struct MaterialSlot{LLPointer<LLViewerTexture>mDiffuse;F32 mAlphaMaskCutoff=.2;};std::vector<MaterialSlot>mMaterialSlotList;std::vector<LLPointer<LLViewerTexture>>mTextureList;U64 getSkinHash(){return 1;}U32 mStart=0,mEnd=2,mCount=3,mOffset=0;};
static bool fixtureIncludeAlpha=true;
struct Settings{}gSavedSettings;
template<class T>struct LLCachedControl {T value;std::string name;LLCachedControl(Settings&,const char*n,T x):value(x),name(n){}T operator()()const{return name=="ActorKeyAlphaCoverage"?static_cast<T>(fixtureIncludeAlpha):value;}operator T()const{return value;}};
struct ALActorChromaKey {inline static bool sActive=true,blocked=false;inline static LLUUID selected=LLUUID("11111111-1111-1111-1111-111111111111");static bool suppressed(){return blocked;}static void refuse(const char*){}static bool ownerKeyed(LLUUID id,LLUUID fallback){return id==selected||fallback==selected;}};
struct LLDirectorCast {
 enum {ACTOR_STYLE_LAYER,ACTOR_STYLE_REPLACE};
 enum {ACTOR_LOOK_DEFAULT=0,ACTOR_LOOK_FORWARD_FALLBACK=1,ACTOR_LOOK_RIM_NOIR=28,ACTOR_LOOK_GEL_SPLIT,ACTOR_LOOK_BASS_SWEEP,ACTOR_LOOK_MOONLIT,ACTOR_LOOK_POSSESSED,ACTOR_LOOK_SERAPH,ACTOR_LOOK_INTERROGATION,ACTOR_LOOK_WRAITH,ACTOR_LOOK_TRON_SUIT,ACTOR_LOOK_MAX=36};
 static bool isLiveActorStyleLook(S32 l){return l>=28&&l<=36;}
 struct ActorStyle {bool mEnabled=true,mUseActorHue=true;S32 mMode=ACTOR_STYLE_REPLACE,mStyle=10,mDistortion=0;F32 mAlpha=1,mHue=0,mPixelSize=0,mShimmerSpeed=0,mShimmerAmount=0,mGlitch=0,mDistortionAmount=0,mBrightness=1,mEffectFps=0,mDissolveProgress=.5;};
 ActorStyle style;static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 const ActorStyle& getRenderActorStyle(LLUUID){return style;}
 const ActorStyle& resolveStoredActorStyle(LLUUID id,LLUUID,LLUUID& out){out=id;return style;}
};
LLUUID gAgentID=ALActorChromaKey::selected;
struct LLVOAvatar;
struct LLGhostAvatar {static bool isRenderHidden(LLVOAvatar*,LLUUID={},LLUUID={});};
struct ALDerenderList {inline static bool hidden=false;static bool keepsAvatarLive(LLVOAvatar*){return hidden;}static bool isHiddenSource(LLUUID){return hidden;}};
static LLPointer<LLVertexBuffer> fixtureBuffer;
static LLGLSLShader nativeShader,gAvatarActorGhostProgram,gAvatarEyeballActorGhostProgram,gSilCoverProgram[6];
static GLuint fixtureTexture=0;
static LLPointer<LLViewerTexture>fixtureDiffuse;
static int fixturePart=0;
static U32 nativeBodyDraws=0,nativeEyeDraws=0,sharedDraws=0;
struct LLDrawPool{enum{POOL_AVATAR=1};virtual U32 getType()const{return POOL_AVATAR;}bool getSkipRenderFlag(){return false;}};
struct LLDrawPoolAvatar:LLDrawPool {
 inline static LLGLSLShader* sVertexProgram=nullptr;
 inline static S32 sDiffuseChannel=0;
 inline static bool sSkipOpaque=false,sSkipTransparent=false;
 inline static F32 sMinimumAlpha=.2;
 inline static bool is_post_deferred_render=false;
 std::vector<struct LLFace*> mDrawFace;
 void renderAvatars(LLVOAvatar*,S32);
 int getNumPasses(){return 3;}void prerender(){}void beginRenderPass(int){}void endRenderPass(int){}
};
static void fixtureDraw(){auto* sh=LLDrawPoolAvatar::sVertexProgram;sh->bind();gGL.getTexUnit(LLDrawPoolAvatar::sDiffuseChannel)->bindManual(LLTexUnit::TT_TEXTURE,fixtureTexture);fixtureBuffer->setBuffer();fixtureBuffer->drawRange(LLRender::TRIANGLES,0,2,3,0);}
struct FakeDrawable{};
struct LLVOAvatar:LLRefCount {
 enum AvatarOverallAppearance {AOA_NORMAL,AOA_INVISIBLE};
 inline static U32 sNumVisibleAvatars=0;
 LLUUID id=ALActorChromaKey::selected;bool hidden=false;LLPointer<FakeDrawable>mDrawable;
 LLVector3 mWindVec;F32 mRipplePhase=0;
 struct Impostor{bool isComplete(){return true;}void bindTexture(int,int){}}mImpostor;
 LLVOAvatar(){mDrawable=reinterpret_cast<FakeDrawable*>(1);}
 ~LLVOAvatar(){mDrawable=nullptr;}
 bool isDead()const{return false;}bool isFullyLoaded()const{return true;}bool isUIAvatar()const{return false;}bool isControlAvatar()const{return false;}bool isSelf()const{return id==gAgentID;}bool isBuddy()const{return true;}bool isGhostAvatar()const{return false;}
 AvatarOverallAppearance getOverallAppearance()const{return AOA_NORMAL;}bool needsImpostorUpdate()const{return false;}bool isImpostor()const{return false;}bool isHardVisualMute()const{return false;}
 LLVOAvatar* getAttachedAvatar()const{return nullptr;}LLUUID getActorFxOwnerId()const{return id;}LLUUID getID()const{return id;}
 struct Root{LLVector3 getWorldPosition(){return {};}}root;Root*getRootJoint(){return &root;}F32 getPelvisToFoot(){return 0;}LLVector3 getPositionAgent(){return {};}LLColor4 getMutedAVColor(){return {};}void renderImpostor(LLColor4,int){}
 U32 renderRigid(){if(fixturePart!=2)return 0;fixtureDraw();++nativeEyeDraws;return 3;}U32 renderSkinned(){if(fixturePart==2||(fixturePart==1&&LLDrawPoolAvatar::sSkipTransparent)||(fixturePart==0&&LLDrawPoolAvatar::sSkipOpaque))return 0;fixtureDraw();++nativeBodyDraws;return 3;}U32 renderTransparent(bool){if(fixturePart!=1)return 0;fixtureDraw();return 3;}void uploadActorGhostPalette(){}
};
bool LLGhostAvatar::isRenderHidden(LLVOAvatar* a,LLUUID,LLUUID){return a&&a->hidden;}
struct LLTextureEntry{LLGLTFMaterial*getGLTFRenderMaterial()const{return nullptr;}};
struct LLFace {LLViewerTexture*getTexture(){return fixtureDiffuse;}const LLTextureEntry*getTextureEntry(){return nullptr;}bool isInAlphaPool(){return true;}LLDrawInfo*mDrawInfo=nullptr;LLMatrix4*mTextureMatrix=nullptr;LLVertexBuffer*getVertexBuffer(){return fixtureBuffer;}LLMatrix4&getRenderMatrix(){static LLMatrix4 m;return m;}int getGeomIndex(){return 0;}int getGeomCount(){return 3;}int getIndicesCount(){return 3;}int getIndicesStart(){return 0;}struct Drawable{LLPointer<LLVOAvatar>p;LLPointer<LLVOAvatar>getVObj(){return p;}}drawable;Drawable*getDrawable()const{return const_cast<Drawable*>(&drawable);}};
struct LLViewerCamera {enum{CAMERA_WORLD};inline static S32 sCurCameraID=0;static LLViewerCamera*getInstance(){static LLViewerCamera c;return &c;}void getMatrixToLocal(LLMatrix4&){}};
struct LLSpatialGroup {struct Partition{bool mRenderByGroup=true;}partition;std::map<U32,std::vector<LLPointer<LLDrawInfo>>>mDrawMap;bool isDead(){return false;}Partition*getSpatialPartition(){return &partition;}};
struct LLCullResult{using sg_iterator=std::vector<LLSpatialGroup*>::iterator;};
struct CoverPolicy;
struct LLPipeline {
 enum {RENDER_TYPE_PARTICLES,RENDER_TYPE_AVATAR};
 inline static bool sImpostorRender=false,sUnderWaterRender=false,sReflectionRender=false,sRenderDeferred=true,RenderAvatarCloth=false,sPrismLensRender=false,sPrismPreviewRender=false,sRenderingHUDs=false;
 std::map<U32,std::vector<LLDrawInfo*>> maps;std::vector<LLSpatialGroup*>groups,riggedGroups;std::vector<LLDrawPool*>mPools;
 auto beginRenderMap(U32 p){return maps[p].begin();}auto endRenderMap(U32 p){return maps[p].end();}
 auto beginAlphaGroups(){return groups.begin();}auto endAlphaGroups(){return groups.end();}auto beginRiggedAlphaGroups(){return riggedGroups.begin();}auto endRiggedAlphaGroups(){return riggedGroups.end();}
 bool hasRenderType(int){return true;}void selectForwardLightSet(int,bool=false){}void bindDeferredShaderFast(LLGLSLShader& sh){sh.bind();}
 struct Pack{}mMainRT;Pack*mRT=&mMainRT;LLRenderTarget mActorKeyCoverage;U32 mActorKeyCoverageFrame=~0u,mActorKeyMatteFrame=~0u;glm::mat4 mActorKeyProjection,mActorKeyModelview;
 bool failEnsure=false;bool ensureKeyCoverage(){return mActorKeyCoverage.isComplete()||mActorKeyCoverage.allocate(64,64,GL_R8,false);}bool ensureKeyPrograms(){return !failEnsure;}
 bool renderCover(U32,const CoverPolicy&);void buildActorKeyCoverageAndMatte();void buildActorKeyMatte(){mActorKeyMatteFrame=gFrameCount;}void renderActorKeyPrismOccluders(){}void renderGhostSilCover(U32,const CoverPolicy&){}
}gPipeline;
static bool gCubeSnapshot=false;

struct LLViewerPartSim {static int getMaxPartCount(){return 100;}};
struct Sphere{void renderGGL(){}}gSphere;
struct LLViewerFetchedTexture{inline static LLPointer<LLViewerTexture>sWhiteImagep,sFlatNormalImagep;};
struct ALCineLightRigManager{static bool anyActorOnly(){return false;}static ALCineLightRigManager&instance(){static ALCineLightRigManager m;return m;}int recipientBit(LLVOAvatar*){return 0;}};
struct ALEnvIntensity{static bool envOnlyActive(){return false;}};
struct LLActorMaskEligibility {inline static bool sArmed=false;static LLActorMaskEligibility&instance(){static LLActorMaskEligibility m;return m;}template<class T>void suppress(T){}void coverageChanged(LLUUID){}};
struct LLViewerShaderMgr {static bool hasSharedActorFxPBRShaders(){return true;}enum {DIFFUSE_MAP=LLShaderMgr::DIFFUSE_MAP,AVATAR_MATRIX=LLShaderMgr::AVATAR_MATRIX,AVATAR_WIND=LLShaderMgr::AVATAR_WIND,AVATAR_SINWAVE=LLShaderMgr::AVATAR_SINWAVE,AVATAR_GRAVITY=LLShaderMgr::AVATAR_GRAVITY};};
struct LLRenderPass {enum {PASS_SIMPLE=10,PASS_ALPHA,PASS_ALPHA_RIGGED,PASS_FULLBRIGHT,PASS_FULLBRIGHT_RIGGED,PASS_FULLBRIGHT_SHINY,PASS_FULLBRIGHT_SHINY_RIGGED,PASS_FULLBRIGHT_ALPHA_MASK,PASS_FULLBRIGHT_ALPHA_MASK_RIGGED};
 enum eGLTFIndexedMaps{GLTF_MAPS_FULL,GLTF_MAPS_NONE,GLTF_MAPS_BASE_COLOR,GLTF_MAPS_GLOW};
 static void pushGLTFBatchIndexed(LLDrawInfo&,eGLTFIndexedMaps,bool=false);
 static bool uploadMatrixPalette(LLPointer<LLVOAvatar>,LLPointer<LLRefCount>){return true;}
 static bool uploadMatrixPalette(LLPointer<LLVOAvatar>,LLPointer<LLRefCount>,const LLVOAvatar*&,U64&,bool&){return true;}
 static void applyModelMatrix(const LLDrawInfo&){}static void applyModelMatrix(const LLMatrix4*){}
 static void setupGLTFTextureMatrix(const LLMatrix4*){}static void teardownGLTFTextureMatrix(const LLMatrix4*){}
 static void uploadActorFx(const LLDrawInfo&){}
 static void uploadActorFxStyleOnly(LLUUID,bool,bool);

 enum{ALR_SLOT_SIL_ONLY,ALR_SLOT_GLOW,ALR_SLOT_BEAUTY};
 static bool shouldSuppressSharedActorFx(const LLUUID&);static bool shouldSuppressSharedActorFx(const LLDrawInfo&,bool=false);static bool shouldSuppressSharedActorFxPBR(const LLDrawInfo&,bool=false);
 static void uploadActorFxDisabled(){}static void uploadAvatarLightResponse(LLUUID){}static void uploadActorFx(LLUUID,bool=false){}
 static F32 actorFxFrameTime(F32){return 2;}static LLVector2 actorFxFragOffset(){return {};}static LLVector2 actorFxScreenSize(){return {64,64};}static bool actorFxLookNeedsSyntheticBloom(S32){return false;}
 static void uploadAvatarLightResponseTo(LLGLSLShader&,LLUUID,LLUUID,LLUUID,int){}
};
using GhostCoverageMask=U32;constexpr U32 GHOST_COVERAGE_NONE=0,GHOST_COVERAGE_RIGGED_SOLID=1,GHOST_COVERAGE_RIGGED_BLEND=2,GHOST_COVERAGE_RIGGED_GLOW=4,GHOST_COVERAGE_STATIC_SOLID=8,GHOST_COVERAGE_STATIC_BLEND=16,GHOST_COVERAGE_ALL=31;
struct CoverPolicy;
struct LLActorMover {
 static LLColor4 actorPathColor(LLUUID){return {1,1,1,1};}
 struct GhostBatch{LLDrawInfo*mInfo=nullptr;S32 mPass=0;};
 struct GhostStaticFace{LLFace*mFace=nullptr;S32 mAlphaKind=0;F32 mCutoff=.2;bool mDoubleSided=false;LLUUID mObjectId;};
 struct SharedActorPBRCommand {GhostBatch mBatch;GhostStaticFace mStaticFace;LLFetchedGLTFMaterial*mMaterial=nullptr;LLDrawInfo*mGlowInfo=nullptr;U8 mAlphaMode=2;bool mRigged=true,mIndexed=false,mAuthoredGlow=false;};
 struct SharedActorStyleProxy {std::vector<SharedActorPBRCommand>mPBRCommands;F32 mAlpha=1;bool mSuppressible=true;S32 mStyle=9;LLUUID mStyleId;LLVOAvatar*mAvatar=nullptr;};
 void renderSharedProxySilCover(U32,const CoverPolicy&){}
 bool renderSharedActorPBRCommands(const SharedActorStyleProxy&,bool);

 // GP_INSERT
 bool ready=true,pbrReady=true;
 static LLActorMover&instance(){static LLActorMover m;return m;}
 bool isSharedActorStyleReady(LLUUID){return ready;}bool isSharedActorStylePBRReady(LLUUID){return pbrReady;}
 bool canRenderSystemActorGhost(LLVOAvatar*,LLUUID){return ready;}
 bool actorMaskAvatarEligible(LLVOAvatar* a){return !a->hidden&&!ALDerenderList::hidden;}
 bool renderSystemActorGhost(LLVOAvatar*,const LLUUID&,bool,bool,bool);
 U32 renderSystemSilCoverBaseline(LLVOAvatar*,const LLVector2&,U32);
 U32 renderSystemSilCover(LLVOAvatar*,const LLVector2&,U32,bool=false);
};
static bool shared_actor_fx_beauty_context(){return true;}static bool shared_actor_fx_zero_cover_context(){return true;}
static bool is_hidden_entity_clone(const LLVOAvatar* a){return a->hidden||ALDerenderList::hidden;}
static const LLVOAvatar*get_actor_fx_wearer(const LLVOAvatar* a){return a;}
static bool has_enabled_actor_fx(const LLVOAvatar*){return LLDirectorCast::instance().style.mEnabled;}
static bool use_system_actor_ghost_replay(LLVOAvatar*,LLUUID){return false;}static bool has_actor_fx_wireframe(LLVOAvatar*){return false;}
static constexpr F32 CLOTHING_GRAVITY_EFFECT=.7;
static const LLMatrix4* gGLLastMatrix=nullptr;
static F32 noise1(F32){return 0;}
static LLColor4 actorPathColor(LLUUID){return {1,1,1,1};}
static void upload_ghost_tron_params(LLGLSLShader&,bool){}
static bool gUseWireframe=false;
static S32 normal_channel=-1,specular_channel=-1;
struct ScopedSharedActorHDRDrawBuffer{ScopedSharedActorHDRDrawBuffer(bool){}};
static constexpr S32 ACTOR_MASK_RIGGED=0,ACTOR_MASK_STATIC=1,ACTOR_MASK_BODY=4,ACTOR_MASK_EYES=5;
static constexpr S32 GHOST_CUTOFF_TEXTURE_ONLY=1,GHOST_CUTOFF_NONE=0;

static constexpr U32 SHARED_ACTOR_FX_PBR_ALPHA_COUNT=3,SHARED_ACTOR_FX_PBR_ALPHA_BLEND=2,SHARED_ACTOR_FX_PBR_ALPHA_MASK=1;
static void upload_shared_replay_light_response(const LLDrawInfo*,LLUUID){}
static LLGLSLShader gSharedActorFxPBRDepthProgram[3];
static LLGLSLShader gSharedActorFxPBRDepthSlotProgram[3];
static LLGLSLShader gSharedActorFxPBRGlowProgram[3];
static LLGLSLShader gSharedActorFxPBRGlowSlotProgram[3];
static LLGLSLShader gSharedActorFxPBRProgram[3];
static LLGLSLShader gSharedActorFxPBRSlotProgram[3];
static LLGLSLShader gSharedActorFxPBRSyntheticGlowProgram[3];
static LLGLSLShader gSharedActorFxPBRSyntheticGlowSlotProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRDepthProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRDepthSlotProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRGlowProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRGlowSlotProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRSlotProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRSyntheticGlowProgram[3];
static LLGLSLShader gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram[3];

struct CoverPolicy{LLRenderTarget&target;F32 bias;const char*tag;bool key=false;bool nativeCover(LLUUID o,LLUUID f,LLVector2&c)const{c=LLVector2(1,0);return ALActorChromaKey::ownerKeyed(o,f);}bool sharedCover(LLUUID,LLUUID,LLUUID,LLVector2&)const{return false;}};
static constexpr F32 GHOST_SCALE_MIN=.01,GHOST_SCALE_MAX=100;
struct StudioLookFrame{};struct ScopedStudioTexture{ScopedStudioTexture(S32){}};
static void studio_apply_phase2(LLGLSLShader*,const StudioLookFrame*,F32){}
template<class A>void studio_reset_loaded(LLGLSLShader*a,LLGLSLShader*b,A apply){apply(a);apply(b);}
template<class D>void studio_draw_displaced(S32,bool,F32,bool,bool,D){}
static LLVector2 actor_ghost_frag_offset(){return {};}static F32 actor_ghost_studio_frame_time(F32){return 2;}
static S32 actorMaskTextureChannels(){return 4;}
static constexpr S32 GHOST_CUTOFF_TEXTURE_VERTEX_ALPHA=2;
static bool ghost_pass_is_blend(S32 p){return p==1;}static bool ghost_pass_is_glow(S32 p){return p==2;}static bool ghost_pass_is_mask(S32 p){return p==3;}
static S32 ghost_legacy_indexed_slot_count(LLDrawInfo*d){return llmax((S32)d->mTextureList.size(),1);}static S32 ghost_batch_slot_count(LLDrawInfo*d){return ghost_legacy_indexed_slot_count(d);}static S32 ghost_batch_alpha_cutoff_mode(S32){return 2;}
static S32 ghost_static_alpha_cutoff_mode(const LLActorMover::GhostStaticFace&){return 2;}
static LLViewerTexture*ghost_batch_texture(LLDrawInfo*){return fixtureDiffuse;}
static F32 ghost_batch_cutoff(LLDrawInfo*,S32 p){return p==3?.2f:0.f;}
static LLViewerTexture*ghost_batch_slot_texture(LLDrawInfo*,S32,bool,F32&,LLColor4&){return fixtureDiffuse;}
static bool ghost_compose_khr_uv(const LLGLTFMaterial::TextureTransform&,LLMatrix4&){return false;}static bool ghost_batch_uv_matrix(LLDrawInfo*,LLMatrix4&){return false;}
static LLGLSLShader gWorldActorGhostProgram,gWorldSkinnedActorGhostIndexedProgram,gActorGhostProgram,gHighlightProgram;
struct StudioKeyPrograms{LLGLSLShader gActorGhostKeyFillProgram;};static auto sStudioKeyPrograms=std::make_unique<StudioKeyPrograms>();

#undef LL_PROFILE_GPU_ZONE
#define LL_PROFILE_GPU_ZONE(x)

struct ActorMaskPass{U32 pass;bool rigged,pbr,masked,material,vec3Position;};static const ActorMaskPass kActorMaskPasses[]={{LLRenderPass::PASS_SIMPLE,false,false,false,false,false}};
struct SilCoverPass{U32 pass;bool rigged,pbr,masked,material,vec3Position;};
static bool drawSilCoverBatch(LLDrawInfo&,const SilCoverPass&,const LLVector2&c,bool blended){auto&shader=gSilCoverProgram[ACTOR_MASK_BODY];shader.bind();shader.uniform2fv(LLStaticHashedString("sil_cover"),1,c.mV);shader.uniform1i(LLStaticHashedString("sil_blended"),blended);LLDrawPoolAvatar::sVertexProgram=&shader;fixtureDraw();return true;}
struct ScopedActorMaskState{};struct ScopedSilCoverInvariant{ScopedSilCoverInvariant(const char*){}};

struct ALAvatarLightResponse{static LLUUID keyForAvatar(LLVOAvatar*a){return a->id;}};

static LLGLSLShader* get_actor_fx_shader(){return LLGLSLShader::sCurBoundShaderPtr;}
static bool actor_mask_native_changes_coverage(const LLDirectorCast::ActorStyle&){return false;}
struct ALTron{struct Palette{LLColor3 mPrimary;};static bool isEnabledMaster(){return false;}static Palette palette(){return {};}static F32 pulse01(){return 0;}struct Clock{F32 mPulsePhase01=0;};static Clock clock(){return {};}};
