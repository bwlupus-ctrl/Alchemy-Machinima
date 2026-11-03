"""Execute the UI shadow callback, production renderer/compare/status/result path
and complete K2 resource selection. GL/scene calls are deterministic recording
seams; actual debug shader pixels are covered by actor_key_gl_check.py.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from native_check_tools import compile_run, check_directory
from relight_area_policy_check import method

NV = Path(__file__).resolve().parents[2] / 'indra/newview'
pipeline = (NV / 'pipeline.cpp').read_text(encoding='utf-8')
panel = (NV / 'alpanelactorkey.cpp').read_text(encoding='utf-8')
callback = method(NV / 'alpanelactorkey.cpp', '    getChild<LLButton>("ak_shadowtest")->setCommitCallback(') + ');'
refresh = method(NV/'alpanelactorkey.cpp', 'void ALPanelActorKey::refreshReadouts()')
result_readout = refresh[refresh.index('    static LLCachedControl<U32> test('):refresh.index('    if (mRetry)')]
status_readout = '    setTextIfChanged(mReadouts[5], ALActorKeyUI::shadowStatus());'
assert status_readout in refresh
combo = ET.parse(NV/'skins/default/xui/en/panel_dpage_capture_key.xml').find('.//combo_box[@name="ak_debug"]')
views = {item.attrib['label']:int(item.attrib['value']) for item in combo.findall('combo_box.item')}
assert views == {'Off':0,'Coverage':1,'Matte':2,'Actor shadow':3,'Lens coverage':4}
source = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
using U32=unsigned;using U64=unsigned long long;using S32=int;using F32=float;using GLint=int;using GLboolean=bool;
enum {GL_READ_FRAMEBUFFER_BINDING=1,GL_READ_BUFFER,GL_PIXEL_PACK_BUFFER_BINDING,GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,
 GL_PACK_SKIP_ROWS,GL_PACK_SKIP_PIXELS,GL_PACK_SWAP_BYTES,GL_READ_FRAMEBUFFER,GL_PIXEL_PACK_BUFFER,GL_COLOR_ATTACHMENT0,
 GL_RED,GL_FLOAT,GL_R8,GL_DRAW_FRAMEBUFFER_BINDING,GL_VIEWPORT,GL_BLEND,GL_FRAMEBUFFER_SRGB,GL_DITHER,GL_COLOR_WRITEMASK};
constexpr int GL_NO_ERROR=0,GL_FALSE=0;
#define LL_PROFILE_ZONE_NAMED(x)
#define llassert assert
#define LL_INFOS(x) std::cout
#define LL_ENDL std::endl
template<class T>T llclamp(T v,T a,T b){return std::clamp(v,a,b);}
template<class T>T llmin(T a,T b){return std::min(a,b);}
template<class... A>std::string llformat(const char* f,A...a){char b[512];std::snprintf(b,sizeof b,f,a...);return b;}
struct Settings{std::map<std::string,U32>v;U32 getU32(const char*n){return v[n];}bool getBOOL(const char*n){return v[n]!=0;}void setU32(const char*n,U32 x){v[n]=x;}}gSavedSettings;
template<class T>struct LLCachedControl{std::string n;LLCachedControl(Settings&,const char*s,T):n(s){}T operator()()const{return T(gSavedSettings.v[n]);}operator T()const{return operator()();}};
U32 gFrameCount=42,sActorKeyShadowTermFrame=0;int RenderShadowDetail=1;
struct LLStaticHashedString{std::string s;LLStaticHashedString(const char*n):s(n){}};
struct LLTexUnit{enum{TFO_BILINEAR,TFO_POINT};};
struct LLRender{enum{TRIANGLES};};
int nextID=1,boundRead=0,boundTarget=0;bool allocationOK=true;int error=0;
std::map<int,int>packState;std::map<int,std::vector<float>>pixels;int units[4]{};
void glGetIntegerv(int n,int*v){if(n==GL_VIEWPORT){int a[]={7,9,16,16};std::copy(a,a+4,v);}else *v=n==GL_READ_FRAMEBUFFER_BINDING?boundRead:n==GL_DRAW_FRAMEBUFFER_BINDING?11:packState[n];}
void glGetBooleanv(int,GLboolean*v){std::fill(v,v+4,true);}bool glIsEnabled(int){return false;}
void glBindFramebuffer(int,int id){boundRead=id;}void glReadBuffer(int n){packState[GL_READ_BUFFER]=n;}
void glBindBuffer(int,int n){packState[GL_PIXEL_PACK_BUFFER_BINDING]=n;}void glPixelStorei(int n,int v){packState[n]=v;}
void glReadPixels(int,int,int,int,int,int,void*out){assert(packState[GL_PIXEL_PACK_BUFFER_BINDING]==0);std::copy(pixels[boundRead].begin(),pixels[boundRead].end(),static_cast<float*>(out));}
int glGetError(){int e=error;error=0;return e;}
struct LLRenderTarget{int id=nextID++;U32 w=16,h=16;bool complete=true;U32 getWidth()const{return w;}U32 getHeight()const{return h;}int getFBO(){return id;}
 bool allocate(U32 a,U32 b,int,bool){w=a;h=b;pixels[id].assign(w*h,1.f);return allocationOK;}void release(){complete=false;}bool isComplete()const{return complete;}
 void bindTarget(){boundTarget=id;}void flush(){}void bindTexture(int,int unit,int){units[unit]=id;}};
struct LLGLSLShader{std::map<std::string,int>u;void bind(){}void unbind(){}void uniform1i(LLStaticHashedString n,int v){u[n.s]=v;}void uniform1f(LLStaticHashedString,float){}void uniform2f(LLStaticHashedString,float,float){}void uniform3fv(LLStaticHashedString,int,const float*){}};
struct Programs{LLGLSLShader shadow,shadowDensity,blur;};auto sActorKeyPrograms=std::make_unique<Programs>();auto gActorKeyCompositeProgram=std::make_unique<LLGLSLShader>();
struct LLGLDisable{LLGLDisable(int){}};struct LLGLDepthTest{LLGLDepthTest(int,int){}};struct ActorKeyTextureScope{ActorKeyTextureScope(int){}};
struct GL{void flush(){}void setColorMask(bool,bool){}void setColorMask(bool,bool,bool,bool){}}gGL;
struct Colour{float mV[3]={0,.7f,.25f};};using LLColor3=Colour;
struct ALActorChromaKey{inline static bool sActive=true;inline static U64 rev=5;inline static std::string reason;static bool suppressed(){return false;}static U64 revision(){return rev;}static Colour colour(){return {};}static U32 mode(){return 0;}static void refuse(const char*s){reason=s;}static void provision(int,int){}};
struct LLActorMover{static bool captureStudioKeyScene(const S32(&)[4]){return true;}};
bool actor_key_copy(LLRenderTarget&,const S32(&)[4]){return true;}
struct LLEnvironment{static LLEnvironment&instance(){static LLEnvironment e;return e;}bool getIsSunUp(){return true;}};
struct LLShaderMgr{enum{SUN_UP_FACTOR};};
// Enum uniforms share the same recording interface as hashed uniforms.
struct Shader:LLGLSLShader{};
struct Triangle{int draws=0;void setBuffer(){}void drawArrays(int,int,int){++draws;}}triangle;
struct LLPipeline{
 struct Pack{LLRenderTarget deferredScreen,shadow[4];struct Meta{bool subject=false;}shadowMeta;}mMainRT;
 struct Stamp{U32 frame=42;U64 generation=7;bool column=false,valid=true;}mActorShadowSlice[4];
 U64 mActorShadowGeneration=7,mActorShadowRevision=5;U32 mActorShadowFrame=42,mActorShadowCascades=2;
 bool mActorShadowReady=true,mActorShadowFallback=false,mMainSunShadowToken=true;int mActorShadowIsolation=1;
 LLRenderTarget mActorShadow[4],mSetShadow[4],mActorShadowLit,mActorKeyShadowVisibility,mActorKeySetVisibility,mActorKeyShadowBlurA,mActorKeyShadowBlurB;
 LLRenderTarget mActorKeyScene,mActorKeyMatte,mActorKeyCoverage,lens;LLRenderTarget*mKeyLensCurrent=&lens;
 S32 mActorKeyDrawFBO=11,mActorKeyViewport[4]={7,9,16,16};U32 mActorKeySceneFrame=0;
 Pack*mRT=&mMainRT;const Pack*mActorKeyMatteRT=&mMainRT;
 U32 mActorKeyCoverageFrame=42,mActorKeyMatteFrame=42,mActorKeyMatteWidth=16,mActorKeyMatteHeight=16;
 U32 mKeyLensFrame=42,mKeyLensRan=1,mKeyLensComplete=1;
 Triangle*mScreenTriangleVB=&triangle;
 bool actorKeyMatteValid()const;bool lensCoverageValid()const;void selfTestActorKey(bool){assert(false);}
 void bindDeferredShader(LLGLSLShader&){}void unbindDeferredShader(LLGLSLShader&){}
 void bindSunShadowState(LLGLSLShader&,LLRenderTarget*const*maps){pixels[boundTarget]=pixels[maps[0]->id];}
 bool buildActorKeyShadowTerm(U32,U32);void compositeActorKey();
 static inline int RenderShadowDetail=1;
}gPipeline;
namespace ALActorKeyUI {std::string shadowStatus();void shadowTestUnavailable();void recordSelfTest(const std::string&);}
std::string sLastResult;
struct LLUICtrl{};struct LLSD{};
struct LLButton{std::function<void(LLUICtrl*,const LLSD&)>commit;void setCommitCallback(std::function<void(LLUICtrl*,const LLSD&)> f){commit=f;}}button;
struct Readout{std::string text;};Readout readouts[9];void setTextIfChanged(Readout*w,const std::string&s){w->text=s;}
struct ALPanelActorKey{Readout*mReadouts[9];ALPanelActorKey(){for(int i=0;i<9;++i)mReadouts[i]=&readouts[i];}
 void refreshReadouts(){
''' + status_readout + result_readout + r'''
 }
 template<class T>T*getChild(const char*){return &button;}void connect(){
''' + callback + r'''
}};
'''
# Keep enum-name overloads explicit in the seam, without editing the extracted methods.
source = source.replace('void uniform1i(LLStaticHashedString n,int v)', 'void uniform1i(int,int){}void uniform1i(LLStaticHashedString n,int v)')
for path, signature in [(NV/'alpanelactorkey.cpp', 'void ALActorKeyUI::recordSelfTest('),
                        (NV/'alpanelactorkey.cpp', 'void ALActorKeyUI::shadowTestUnavailable()'),
                        (NV/'pipeline.cpp', 'static std::string actor_key_compare_shadows('),
                        (NV/'pipeline.cpp', 'std::string ALActorKeyUI::shadowStatus()'),
                        (NV/'pipeline.cpp', 'bool LLPipeline::buildActorKeyShadowTerm('),
                        (NV/'pipeline.cpp', 'bool LLPipeline::actorKeyMatteValid() const'),
                        (NV/'pipeline.cpp', 'bool LLPipeline::lensCoverageValid() const'),
                        (NV/'pipeline.cpp', 'void LLPipeline::compositeActorKey()')]:
    source += method(path, signature) + '\n'
source += r'''
int main(){
 auto&p=gPipeline;ALPanelActorKey ui;ui.connect();
 gSavedSettings.v["ActorKeyShadow"]=1;
 packState={{GL_PIXEL_PACK_BUFFER_BINDING,99},{GL_PACK_ALIGNMENT,8},{GL_PACK_ROW_LENGTH,37},{GL_PACK_SKIP_ROWS,2},{GL_PACK_SKIP_PIXELS,3},{GL_PACK_SWAP_BYTES,1},{GL_READ_BUFFER,GL_COLOR_ATTACHMENT0}};
 for(int scenario=0;scenario<8;++scenario){
  ALActorChromaKey::sActive=true;p.mActorShadowReady=true;p.mActorShadowFrame=gFrameCount;p.mActorShadowIsolation=1;p.mActorShadowFallback=false;
  for(auto&s:p.mActorShadowSlice)s={42,7,false,true};allocationOK=true;
  pixels[p.mMainRT.shadow[0].id].assign(256,.2f);pixels[p.mActorShadow[0].id].assign(256,.2f);pixels[p.mSetShadow[0].id].assign(256,1.f);
  if(scenario==1)pixels[p.mActorShadow[0].id].assign(256,1.f);
  if(scenario==2)pixels[p.mMainRT.shadow[0].id].assign(256,1.f);
  if(scenario==3)p.mActorShadowFallback=true;
  if(scenario==4)p.mActorShadowFrame=41;
  if(scenario==5)p.mActorShadowSlice[0].valid=false;
  if(scenario==6)allocationOK=false;
  if(scenario==7)ALActorChromaKey::sActive=false;
  auto saved=packState;int read=boundRead;button.commit(nullptr,{});assert(gSavedSettings.getU32("ActorKeyShadowSelfTest")==1);
  ui.refreshReadouts();assert(readouts[7].text.find("Exact shadow test pending")!=std::string::npos);
  if(ALActorChromaKey::sActive)p.buildActorKeyShadowTerm(16,16);
  ALActorKeyUI::shadowTestUnavailable();
  assert(!gSavedSettings.getU32("ActorKeyShadowSelfTest"));
  assert(sLastResult.find(scenario==0?"PASS":scenario==1?"FAIL":"INCONCLUSIVE")!=std::string::npos);
  ui.refreshReadouts();assert(readouts[7].text==sLastResult);assert(readouts[5].text==ALActorKeyUI::shadowStatus());
  assert(packState==saved&&boundRead==read);
  if(scenario==3)assert(ALActorKeyUI::shadowStatus().find("fallback")!=std::string::npos);
  if(scenario==4)assert(ALActorKeyUI::shadowStatus().find("skipped generation")!=std::string::npos);
  if(scenario==5)assert(ALActorKeyUI::shadowStatus().find("skipped slices")!=std::string::npos);
 }
 ALActorChromaKey::sActive=true;allocationOK=true;
 for(unsigned view:UI_DEBUG_VALUES){gSavedSettings.v["ActorKeyDebugView"]=view;p.compositeActorKey();
  assert(gActorKeyCompositeProgram->u["keyDebugView"]==view);
  assert(units[1]==(view==1?p.mActorKeyCoverage.id:view==2||view==3?p.mActorKeyMatte.id:p.lens.id));
  assert(gActorKeyCompositeProgram->u["keyPerChannel"]==int(view==0||view==4));
 }
 for(unsigned view:UI_DEBUG_VALUES){gSavedSettings.v["ActorKeyDebugView"]=view;
  for(int invalid=0;invalid<6;++invalid){
   p.mActorKeyCoverageFrame=invalid==0?41:42;p.mActorKeyMatteFrame=invalid==1?41:42;
   p.mKeyLensFrame=invalid==2?41:42;p.mKeyLensComplete=invalid==3?0:1;
   p.mActorKeyMatteWidth=invalid==4?15:16;p.mActorKeyMatte.complete=invalid!=5;
   int draws=triangle.draws;p.compositeActorKey();assert(draws==triangle.draws);
  }
 }
 p.mActorKeyMatte.complete=true;p.mKeyLensCurrent=nullptr;p.mKeyLensRan=p.mKeyLensComplete=0;
 for(unsigned view:UI_DEBUG_VALUES){gSavedSettings.v["ActorKeyDebugView"]=view;p.compositeActorKey();
  assert(units[1]==(view==1?p.mActorKeyCoverage.id:p.mActorKeyMatte.id));assert(gActorKeyCompositeProgram->u["keyPerChannel"]==0);
 }
 std::cout<<"PASS UI button -> production shadow renderer/readback -> shared PASS/FAIL/INCONCLUSIVE; request reset, pack/PBO restore, fallback/skipped status; complete K2 debug selection and stale stamp refusal\n";
}
'''
source = source.replace('UI_DEBUG_VALUES', 'std::vector<unsigned>{' + ','.join(str(v) for v in views.values()) + '}')
out = check_directory('actor_key_diagnostics')
run = compile_run(out, source, 'diagnostics')
assert run.returncode == 0, run.stdout + run.stderr
assert 'setTextIfChanged(mReadouts[7], sLastResult' in panel
assert 'setTextIfChanged(mReadouts[5], ALActorKeyUI::shadowStatus())' in panel
assert 'ALActorKeyUI::shadowTestUnavailable();' in method(NV/'pipeline.cpp', 'void LLPipeline::renderFinalizePresent(')
assert 'ActorKeyDebugView' not in method(NV/'pipeline.cpp', 'void LLPipeline::buildActorKeyMatte()')
print(run.stdout, end='')
# Removing either consumer or the shared sink must fail the full path.
for label, before, after in [('consumer', 'result = actor_key_compare_shadows(beauty, mActorKeyShadowVisibility, mActorKeySetVisibility);', 'result = "INCONCLUSIVE";'),
                             ('result', 'ALActorKeyUI::recordSelfTest(result);', '(void)result;')]:
    assert source.count(before) == 1
    negative = compile_run(out, source.replace(before, after), 'negative_' + label)
    assert negative.returncode != 0, label + ' mutation survived'
print('PASS shadow comparison and result-sink negative controls rejected')
