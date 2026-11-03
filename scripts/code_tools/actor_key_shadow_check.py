"""WP4 native contracts. Executes production scope, prepare, dispatch, predicates,
release and stamp expressions. Draw submissions/GL are recording seams; the complete
production draw body is independently checked against the pre-split body. No viewer build.
"""
from pathlib import Path
import subprocess, re, json, hashlib
from native_check_tools import compile_run, check_directory
from relight_area_policy_check import method
ROOT=Path(__file__).resolve().parents[2]; NV=ROOT/'indra/newview'
OUT=check_directory('actor_key_shadow')
s=(NV/'pipeline.cpp').read_text(encoding='utf-8')
allowance_raw=Path(__file__).with_name('actor_key_shadow_hunks.json').read_bytes().replace(b'\r\n',b'\n')
assert hashlib.sha256(allowance_raw).hexdigest() == 'b1575e291227801c5ed9b2ab5645ed52964b3f1651e7dc9d53a34fb2780d6b67', 'WP4 parity baseline pin'
baseline_ref=json.loads(allowance_raw)['base']
base=subprocess.check_output(['git','show',baseline_ref+':indra/newview/pipeline.cpp'],text=True,encoding='utf-8')
def body(source, signature):
 start=source.index(signature); brace=source.index('{',start); n=1; end=brace+1
 while n:
  n+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
old=body(base,'void LLPipeline::renderShadow('); draw=body(s,'void LLPipeline::drawShadowCascade(')
def normalized(t): return re.sub(r'\s+','',t)
# A strict pure-move gate on every original draw statement; only the two named
# auxiliary policy guards and the sCull assertion may differ.
a=old[old.index('    //generate shadow map'):old.index('    // reset occlusion culling flag')]
b=draw[draw.index('    //generate shadow map'):draw.rindex('    llassert(sCull == &result);')]
# Expand the local matrix RAII guard into its original entry/exit statements
# before the strict draw-body parity comparison.
ms=body(draw,'    struct MatrixScope')
ctor=body(ms,'        MatrixScope(');dtor=body(ms,'        ~MatrixScope()')
entry=ctor[ctor.index('{')+1:ctor.rindex('}')]
exit=dtor[dtor.index('{')+1:dtor.rindex('}')]
b=b.replace(ms+' matrices(view, proj);',entry)+exit
b=b.replace('accounting && LLPipeline::sUseOcclusion','LLPipeline::sUseOcclusion').replace('include_alpha_cutout && sShadowCasterFilter != CASTER_KEY_ONLY','include_alpha_cutout')
assert normalized(a)==normalized(b),'beauty draw body changed beyond explicit replay policies'
prepare=method(NV/'pipeline.cpp','void LLPipeline::prepareShadowCascade(')
assert prepare.count('stateSort(')==1 and 'stateSort(' not in draw and 'updateCull(' not in draw
assert 'renderKeyShadowCascade(0, true, view0, proj0, column_cam, column_result, true, true)' in s
assert 'renderKeyShadowCascade(j, false, view[j], proj[j], shadow_cam, result[j], !have_union_cull, alpha_casters)' in s
for name in ('pushRiggedBatches','pushUntexturedRiggedBatches','pushRiggedMaskBatches','pushRiggedGLTFBatches','pushUntexturedRiggedGLTFBatches','pushRiggedGLTFBatchesScalar','pushRiggedGLTFBatchesIndexed'):
 dp=(NV/'lldrawpool.cpp').read_text(encoding='utf-8');matches=re.findall(r'void LLRenderPass::'+name+r'\([^)]*\)',dp)
 for sig in matches:
  part=body(dp,sig)
  if 'beginRenderMap' in part:
   assert 'skipKeyShadowCaster(' in part,name
   if 'uploadMatrixPalette' in part: assert part.index('skipKeyShadowCaster(')<part.index('uploadMatrixPalette'),name
# Expand only the four shared binder stages, then compare the complete ordered
# upload list to the pinned pre-WP4 baseline. Unrelated reflection/SSAO/spot uploads keep their positions.
binder=body(s,'void LLPipeline::bindSunShadowState(')
newbind=body(s,'void LLPipeline::bindDeferredShader(')
for stage in ('Matrices','Meta','Tuning','Directions'):
 marker='    if (stage == SunShadowStage::All || stage == SunShadowStage::'+stage+')'
 segment=body(binder,marker);segment=segment[segment.index('{')+1:segment.rindex('}')]
 newbind=newbind.replace('bindSunShadowState(shader, nullptr, SunShadowStage::'+stage+');',segment)
oldbind=body(base,'void LLPipeline::bindDeferredShader(')
uploads=lambda source: re.findall(r'(shader\.uniform\w+|uploadShadowUniforms)\(\s*([^,;\n]+)',source)
assert uploads(oldbind)==uploads(newbind),'shared binder reordered or lost beauty uniforms'
assert 'shader.addPermutation("SOFT_SHADOW_VOGEL_MAX"' not in s,'Vogel max is a GLSL constant, not a permutation'
scope=s[s.index('class LLPipeline::LLShadowCascadeScope'):s.index('void LLPipeline::prepareShadowCascade')]
cap=body(dp,'static bool skip_shadow_draw(')
valid=re.search(r'const bool valid = (.*?);',method(NV/'pipeline.cpp','bool LLPipeline::buildActorKeyShadowTerm('),re.S).group(1)
eligible=method(NV/'pipeline.cpp','bool LLPipeline::buildActorKeyShadowTerm(')
eligible=eligible[eligible.index('    if (!mActorShadowReady'):eligible.index('    LL_PROFILE_ZONE_NAMED')]
head=r'''
#include <cassert>
#include <vector>
#include <set>
#include <iostream>
#include <stdexcept>
#include <array>
#include <utility>
#include <optional>
using U8=unsigned char;using U32=unsigned;using U64=unsigned long long;using S32=int;
#define NDEBUG 1
#define llassert assert
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE
#define LL_PROFILE_GPU_ZONE(x)
#define LL_PROFILE_ZONE_NAMED(x)
#define LL_WARNS(x) std::cerr
#define LL_ENDL std::endl
constexpr int GL_CULL_FACE=1,GL_DEPTH_CLAMP=2,GL_TRUE=1,GL_LESS=3;
namespace glm {struct mat4{float data[16]{};};const float* value_ptr(const mat4& m){return m.data;}}
struct LLCamera{};
struct LLCullResult {std::vector<int> maps{1,2};};
struct LLGLState {enum{ENABLED_STATE=1,DISABLED_STATE=0};LLGLState(int,int){}};
struct LLGLEnable {LLGLEnable(int){}};struct LLGLDepthTest{LLGLDepthTest(int,int,int){}};
struct LLUUID {int id=0;bool isNull()const{return !id;}};
struct LLVOAvatar {int id=0;};
struct LLDrawInfo{LLUUID mActorFxOwner,mActorFxFallbackOwner;bool mAttachedToAvatar=false;LLVOAvatar* mAvatar=nullptr;};
struct LLGhostAvatar{static bool isRenderHidden(LLVOAvatar*,LLUUID,LLUUID){return false;}};
struct ALActorChromaKey{inline static bool sActive=true;inline static U64 rev=4;
 static U64 revision(){return rev;}static bool keepsLive(const LLVOAvatar* a){return a&&a->id==1;}
 static bool ownerKeyed(LLUUID a,LLUUID b){return a.id==1||b.id==1;}};
struct AlphaCapCounts{bool active=false;U32 rejected=0,attached=0,expected=0;bool treeSkipped=false;};
AlphaCapCounts sAlphaCapCounts;bool alphaCapDebug(){return true;}
U32 gFrameCount=42;bool gCubeSnapshot=false;int RenderShadowDetail=1;int stack=1;
struct LLRenderTarget{int releases=0;void release(){++releases;}void bindTarget(){++stack;}
 void getViewport(int*){}void clear(){}void flush(){--stack;}};
struct LLRender{enum{MM_PROJECTION,MM_MODELVIEW};};
struct GL{int draws=0,mode=0;int stacks[2]{};void matrixMode(int m){mode=m;}void pushMatrix(){++stacks[mode];}void popMatrix(){--stacks[mode];}void loadMatrix(const float*){}}gGL;int gGLViewport[4];void* gGLLastMatrix=nullptr;
#define LL_WARNS_ONCE(x) std::cerr
struct Program{bool complete=true;bool isComplete(){return complete;}}gDeferredShadowProgram,gDeferredShadowAlphaMaskProgram,gDeferredShadowGLTFAlphaMaskProgram;
struct LLPipeline{
 enum{CASTER_ALL,CASTER_KEY_ONLY,CASTER_SET_ONLY};
 inline static U8 sShadowCasterFilter=0,sShadowDrawFilter=0;inline static U32 sUseOcclusion=2;
 inline static bool sShadowRender=false;inline static LLCullResult* sCull=nullptr;
 struct Pack{struct Meta{bool subject=false;}shadowMeta;}mMainRT,aux;Pack* mRT=&mMainRT;
 struct ActorShadowSlice{U32 frame=~0u;U64 generation=0;bool column=false,valid=false;};
 ActorShadowSlice mActorShadowSlice[4];U64 mActorShadowGeneration=7,mActorShadowRevision=4;
 U32 mActorShadowFrame=42,mActorShadowCascades=2;S32 mActorShadowIsolation=1;
 bool mActorShadowReady=true,mActorShadowFallback=false,mMainSunShadowToken=true;
 LLRenderTarget mActorShadow[4],mSetShadow[4],mActorShadowLit,mActorKeyShadowVisibility,mActorKeySetVisibility,mActorKeyShadowBlurA,mActorKeyShadowBlurB;
 int sorts=0,culls=0,throwsAt=-1;std::vector<int>calls;
 class LLShadowCascadeScope;
 void updateCull(LLCamera&,LLCullResult& r){++culls;r.maps.clear();}
 void stateSort(LLCamera&,LLCullResult& r){++sorts;sCull=&r;r.maps.push_back(17);r.maps.push_back(23);}
 void prepareShadowCascade(LLCamera&,LLCullResult&,bool);
 void drawShadowCascade(const glm::mat4&,const glm::mat4&,LLCamera&,LLCullResult& r,bool,bool,bool,bool accounting){
  assert(sCull==&r&&sShadowRender&&sUseOcclusion==0);calls.push_back(sShadowCasterFilter);
  if(accounting){sAlphaCapCounts.expected=13;sAlphaCapCounts.attached=3;sAlphaCapCounts.rejected=10;}
  else assert(!sAlphaCapCounts.active);
  if(int(calls.size())==throwsAt)throw std::runtime_error("draw bail");
 }
 void renderShadow(const glm::mat4&,const glm::mat4&,LLCamera&,LLCullResult&,bool,bool,bool,bool,bool,bool);
 void renderKeyShadowCascade(U32,bool,const glm::mat4&,const glm::mat4&,LLCamera&,LLCullResult&,bool,bool);
 static bool skipKeyShadowCaster(const LLDrawInfo*);static bool skipKeyShadowAvatar(const LLVOAvatar*);
 static void countShadowAlphaCapDraw(bool b){if(b)++sAlphaCapCounts.attached;else ++sAlphaCapCounts.rejected;}
 void releaseActorShadowTargets();
 void diagnoseKeyShadowOwners(LLCullResult&,U32){}
 bool eligible(){
'''+eligible+'''return true;}
 bool validSlice(U32 i){const auto& stamp=mActorShadowSlice[i];return '''+valid+''';}
};
'''
production=ms+';\n'+scope+prepare+method(NV/'pipeline.cpp','void LLPipeline::renderShadow(')+method(NV/'pipeline.cpp','void LLPipeline::renderKeyShadowCascade(')+method(NV/'pipeline.cpp','bool LLPipeline::skipKeyShadowCaster(')+method(NV/'pipeline.cpp','bool LLPipeline::skipKeyShadowAvatar(')+method(NV/'pipeline.cpp','void LLPipeline::releaseActorShadowTargets()')+cap
checks=r'''
int main(){
 glm::mat4 view,proj;LLCamera cam;
 for(bool bail:{false,true}){
  try { MatrixScope matrices(view,proj);assert(gGL.stacks[0]==1&&gGL.stacks[1]==1);if(bail)throw std::runtime_error("matrix bail"); }
  catch(const std::runtime_error&){}
  assert(gGL.stacks[0]==0&&gGL.stacks[1]==0&&gGLLastMatrix==nullptr);
 }
 for(bool column:{false,true})for(bool cull:{false,true})for(int mode=0;mode<3;++mode){
  LLPipeline p;LLCullResult r;p.mActorShadowIsolation=mode;
  p.renderKeyShadowCascade(0,column,view,proj,cam,r,cull,false);
  assert(p.sorts==1&&p.culls==int(cull));assert(r.maps.size()==(cull?2:4));
  assert(p.calls==(mode==0?std::vector<int>{0,1}:mode==1?std::vector<int>{0,1,2}:std::vector<int>{0}));
  assert(LLPipeline::sUseOcclusion==2&&!LLPipeline::sShadowRender&&!sAlphaCapCounts.active&&LLPipeline::sShadowCasterFilter==0&&stack==1);
  assert(sAlphaCapCounts.expected==13&&sAlphaCapCounts.rejected==10&&sAlphaCapCounts.attached==3);
  if(mode<2){assert(p.mActorShadowSlice[0].valid&&p.mActorShadowSlice[0].column==column);p.mMainRT.shadowMeta.subject=column;assert(p.validSlice(0));}
 }
 for(int bail:{1,2,3}){LLPipeline p;LLCullResult r;p.throwsAt=bail;
  try{p.renderKeyShadowCascade(0,false,view,proj,cam,r,true,false);assert(false);}catch(const std::runtime_error&){}
  assert(!p.mActorShadowSlice[0].valid&&stack==1&&LLPipeline::sShadowCasterFilter==0&&!LLPipeline::sShadowRender&&LLPipeline::sUseOcclusion==2&&!sAlphaCapCounts.active);
 }
 {LLPipeline p;LLCullResult r;ALActorChromaKey::sActive=false;p.renderKeyShadowCascade(0,false,view,proj,cam,r,true,true);assert(p.calls==std::vector<int>{0}&&p.sorts==1);ALActorChromaKey::sActive=true;}
 {LLPipeline p;LLCullResult r;gDeferredShadowProgram.complete=false;
  p.renderKeyShadowCascade(0,false,view,proj,cam,r,true,true);assert(p.mActorShadowFallback&&p.calls==std::vector<int>{0}&&!p.mActorShadowSlice[0].valid);gDeferredShadowProgram.complete=true;}
 LLVOAvatar keyed{1},unkeyed{2};
 for(int actor=0;actor<3;++actor)for(int capf=0;capf<4;++capf)for(bool attached:{false,true})for(int owner:{0,1,2})for(int fallback:{0,1,2}){
  LLPipeline::sShadowCasterFilter=actor;LLPipeline::sShadowDrawFilter=capf;
  LLDrawInfo d{{owner},{fallback},attached};bool k=owner==1||fallback==1;
  bool capSkip=capf!=0&&attached==(capf==1);bool casterSkip=actor==1?!k:actor==2?k:false;
  assert(skip_shadow_draw(&d)==(capSkip||casterSkip));
  assert(LLPipeline::skipKeyShadowAvatar(&keyed)==(actor==2));assert(LLPipeline::skipKeyShadowAvatar(&unkeyed)==(actor==1));
 }
 LLPipeline::sShadowCasterFilter=LLPipeline::sShadowDrawFilter=0;
 {LLPipeline p;assert(p.eligible());p.mActorShadowFrame=41;assert(!p.eligible());p.mActorShadowFrame=42;
  p.mActorShadowRevision=3;assert(!p.eligible());p.mActorShadowRevision=4;p.mMainSunShadowToken=false;assert(!p.eligible());
  p.mMainSunShadowToken=true;RenderShadowDetail=0;assert(!p.eligible());RenderShadowDetail=1;p.mActorShadowReady=false;assert(!p.eligible());
  p.mActorShadowSlice[0]={42,7,false,true};assert(p.validSlice(0));p.mMainRT.shadowMeta.subject=true;assert(!p.validSlice(0));
  p.mActorShadowSlice[0].column=true;assert(p.validSlice(0));++p.mActorShadowGeneration;assert(!p.validSlice(0));--p.mActorShadowGeneration;
  ++gFrameCount;assert(!p.validSlice(0));--gFrameCount;p.mActorShadowSlice[0].valid=false;assert(!p.validSlice(0));
  p.releaseActorShadowTargets();assert(!p.mActorShadowReady&&p.mActorShadowFrame==~0u);for(int i=0;i<4;++i)assert(p.mActorShadow[i].releases==1&&p.mSetShadow[i].releases==1&&!p.mActorShadowSlice[i].valid);
  assert(p.mActorShadowLit.releases==1&&p.mActorKeyShadowVisibility.releases==1&&p.mActorKeySetVisibility.releases==1&&p.mActorKeyShadowBlurA.releases==1&&p.mActorKeyShadowBlurB.releases==1);
 }
 std::cout<<"PASS U-SPLIT (one sort, immutable lists, 1/2/3 draws, normal/throw unwind, target nesting), U-CASTER (cap x keyed-owner/fallback truth table, avatar identity), column/ordinary stamps, skipped generation/current frame/revision, OFF dispatch and teardown\n";
}
'''
result=compile_run(OUT,head+production+checks,'wp4');print(result.stdout,end='');result.check_returncode()
# Deliberate production mutations must be detected by the same executable assertions.
for name,bad in [('resort',production.replace('replay(mActorShadow[slice], CASTER_KEY_ONLY);','prepareShadowCascade(camera, result, false); replay(mActorShadow[slice], CASTER_KEY_ONLY);')),('owner',production.replace('draw->mActorFxOwner, draw->mActorFxFallbackOwner','draw->mActorFxOwner, draw->mActorFxOwner')),('frame',(head+production).replace('mActorShadowFrame != gFrameCount','false'))]:
 source=(bad if name=='frame' else head+bad)+checks
 assert source!=head+production+checks
 r=compile_run(OUT,source,'bad_'+name);assert r.returncode,(name,r.stdout)
 print('PASS rejected mutation:',name)
print('PASS W-17 structural beauty parity: complete pre-split draw statements preserved; original rain/spot/world wrapper BOUND lifecycle unchanged')
# U-SPLIT recorded-call gate executes the COMPLETE pre-split and current beauty
# draw bodies, not just the dispatch seam used above. Rendering APIs record calls;
# GL shader pixel behavior is covered by the hardware companion harness.
passes=sorted(set(re.findall(r'LLRenderPass::(PASS_\w+)',old+draw)))
extra=r'''
#define LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE(x)
#define LLGLSLShader_Shadow_Stub 1
constexpr int GL_FALSE=0;
std::vector<int> recorded;
struct LLVertexBuffer{static void unbind(){recorded.push_back(700);}};
struct LLTexUnit{enum{TT_TEXTURE};};
struct Unit{void unbind(int){recorded.push_back(701);}void disable(){recorded.push_back(702);}void enable(int){recorded.push_back(703);}};
struct LLGLSLShader{
 inline static bool sIndexedLegacyMaterials=false;inline static int sIndexedGLTFChannels=0;
 inline static LLGLSLShader* sCurBoundShaderPtr=nullptr;
 int id=0;void bind(bool rigged=false){sCurBoundShaderPtr=this;recorded.push_back(1000+id*2+rigged);}
 bool isComplete()const{return true;}void uniform1i(int,int v){recorded.push_back(1300+v);}
 void setMinimumAlpha(float){recorded.push_back(1400);}
};
struct LLShaderMgr{enum{SUN_UP_FACTOR};};
struct LLEnvironment{static LLEnvironment& instance(){static LLEnvironment e;return e;}bool getIsSunUp(){return true;}};
void stop_glerror(){}
struct LLRenderPass{enum{
'''+','.join(passes)+r''',NUM_RENDER_TYPES};};
struct Pool{
 void pushMaskBatchesIndexed(U32 type,bool rigged){recorded.push_back(2000+type*2+rigged);}
 void pushRiggedGLTFBatchesScalar(U32 type){recorded.push_back(2100+type);}
 void pushRiggedGLTFBatchesIndexed(U32 type,U32){recorded.push_back(2200+type);}
 void pushRiggedGLTFBatches(U32 type){recorded.push_back(2300+type);}
 void pushGLTFBatchesScalar(U32 type){recorded.push_back(2400+type);}
 void pushGLTFBatchesIndexed(U32 type,U32){recorded.push_back(2500+type);}
 void pushGLTFBatches(U32 type){recorded.push_back(2600+type);}
};
'''
shader_names=sorted(set(re.findall(r'\b(gDeferred\w+Program)\b',old+draw)))
extra+='\n'.join('LLGLSLShader '+name+'{'+str(i+1)+'};' for i,name in enumerate(shader_names))+'\n'
h=head.replace('struct LLPipeline{',extra+'struct LLPipeline{')
h=h.replace('struct Program{bool complete=true;bool isComplete(){return complete;}}gDeferredShadowProgram,gDeferredShadowAlphaMaskProgram,gDeferredShadowGLTFAlphaMaskProgram;','')
h=h.replace('enum{\n'+','.join(passes)+',NUM_RENDER_TYPES}', 'enum{\n'+','.join(passes)+',NUM_RENDER_TYPES,GLTF_MAPS_BASE_COLOR}')
h=h.replace('struct LLCullResult {std::vector<int> maps{1,2};};','struct LLCullResult;')
h=h.replace('bool mAttachedToAvatar=false;LLVOAvatar* mAvatar=nullptr;','bool mAttachedToAvatar=false;LLVOAvatar* mAvatar=nullptr;int mVertexBuffer=0;std::vector<int>mMaterialSlotList;')
h=h.replace('struct LLGhostAvatar{',r'''struct LLCullResult {
 std::vector<int> maps{1,2};using drawinfo_iterator=LLDrawInfo**;
 LLDrawInfo di;LLDrawInfo* ptr=&di;
 U32 getRenderMapSize(U32){return 0;}LLDrawInfo** beginRenderMap(U32){return &ptr;}
 LLDrawInfo** endRenderMap(U32){return &ptr;}
 static void increment_iterator(LLDrawInfo**& it,LLDrawInfo**){++it;}
};
struct LLGhostAvatar{''')
# GL types appear before the shader/pool declarations in the shared fixture head.
h=h.replace('struct LLRender{enum{MM_PROJECTION,MM_MODELVIEW};};',r'''struct Unit{void unbind(int){}void disable(){}void enable(int){}};
struct LLRender{enum{MM_PROJECTION,MM_MODELVIEW};};''')
h=h.replace('struct Unit{void unbind(int){recorded.push_back(701);}void disable(){recorded.push_back(702);}void enable(int){recorded.push_back(703);}};','')
h=h.replace('void loadMatrix(const float*){}}gGL;', 'void loadMatrix(const float*){}Unit unit;Unit* getTexUnit(int){return &unit;}void diffuseColor4f(float,float,float,float){}void setColorMask(bool,bool){}}gGL;')
h=h.replace('void* gGLLastMatrix=nullptr;', 'void* gGLLastMatrix=nullptr;float gGLModelView[16]{};constexpr float ALPHA_BLEND_CUTOFF=.5f;')
inline=body(h,' void drawShadowCascade(')
h=h.replace(inline,inline[:inline.index('{')]+';')
needle=' void releaseActorShadowTargets();'
h=h.replace(needle,r'''
 Pool pool;Pool* mAlphaMaskPool=&pool;
 void doOcclusion(LLCamera&){recorded.push_back(3000);}
 void renderObjects(U32 type,bool texture,bool batch=false,bool rigged=false){recorded.push_back(3100+type*8+texture*4+batch*2+rigged);}
 void renderGLTFObjects(U32 type,bool texture,bool rigged){recorded.push_back(4000+type*4+texture*2+rigged);}
 void renderGeomShadow(LLCamera&,bool cutout){recorded.push_back(4100+cutout);}
 void renderMaskedObjects(U32 type,bool texture,bool batch,bool rigged){recorded.push_back(4200+type*8+texture*4+batch*2+rigged);}
 void renderFullbrightMaskedObjects(U32 type,bool texture,bool batch,bool rigged){recorded.push_back(5000+type*8+texture*4+batch*2+rigged);}
 void renderAlphaObjects(bool rigged){recorded.push_back(5100+rigged);}
 void uploadShadowAlphaMode(int v){recorded.push_back(5200+v);}
 void legacyRenderShadow(const glm::mat4&,const glm::mat4&,LLCamera&,LLCullResult&,bool,bool,bool,bool,bool,bool);
'''+needle)
filter_scope=body(s,'class ShadowDrawFilterScope')+';\n'
legacy=old.replace('void LLPipeline::renderShadow(', 'void LLPipeline::legacyRenderShadow(',1)
source=h+filter_scope+scope+prepare+draw+method(NV/'pipeline.cpp','void LLPipeline::renderShadow(')+legacy+r'''
int main(){glm::mat4 view,proj;LLCamera camera;
 for(bool indexed:{false,true})for(bool rigged:{false,true})for(bool alpha:{false,true})for(bool cutout:{false,true})for(bool cull:{false,true}){
  LLGLSLShader::sIndexedLegacyMaterials=indexed;LLGLSLShader::sIndexedGLTFChannels=indexed?2:0;
  LLPipeline p,q;LLCullResult a,b;recorded.clear();
  p.legacyRenderShadow(view,proj,camera,a,true,cull,rigged,alpha,true,cutout);auto before=recorded;
  recorded.clear();q.renderShadow(view,proj,camera,b,true,cull,rigged,alpha,true,cutout);
  assert(before==recorded&&p.sorts==q.sorts&&p.culls==q.culls&&gGL.stacks[0]==0&&gGL.stacks[1]==0);
 }
 std::cout<<"PASS U-SPLIT / W-17 complete production old/new beauty call sequences: 32 indexed/rigged/alpha/cutout/cull combinations, matrix state and one-sort parity\n";
}
'''
r=compile_run(OUT,source,'beauty_sequence');print(r.stdout,end='');r.check_returncode()
