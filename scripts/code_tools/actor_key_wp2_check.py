"""WP2 production C++ policy/serial/latch/copy/preflight contracts, no viewer build.

Uses extracted complete functions and instrumented external seams. Pixel/real
viewer integration gates are reported separately; a spy is never a viewer draw.
"""
from pathlib import Path
import subprocess
from native_check_tools import compile_run
from relight_area_policy_check import method

ROOT=Path(__file__).resolve().parents[2]
NV=ROOT/'indra/newview'
OUT=ROOT/'.tmp/ckey_20261102/wp2/native'

def contracts():
    OUT.mkdir(parents=True,exist_ok=True)
    cpp=(NV/'alactorchromakey.cpp').read_text(encoding='utf-8')
    state=cpp[cpp.index('U64 sFinalizeSerial = 0;'):cpp.index('} sRefusal;')+len('} sRefusal;')]
    fixture=r"""
#include <cassert>
#include <atomic>
#include <string>
#include <iostream>
#include <set>
using U64=unsigned long long;using U32=unsigned;using S32=int;using F32=float;
struct LLUUID {int id;};
U32 gFrameCount=42;bool gDisconnected=false,gWindowResized=false;
struct Window{int w=1920,h=1080;int getWidth()const{return w;}int getHeight()const{return h;}};
struct ViewerWindow{Window window;const Window& getWindowRectRaw(){return window;}} viewer;
auto* gViewerWindow=&viewer;
struct LLWindow {inline static std::atomic<bool> sPresentBlocked=false;inline static std::atomic<U64> sBlockedPresents=0;};
struct LLSD {std::string value;LLSD& operator[](const char*){return *this;}void operator=(std::string s){value=s;}};
int notifications=0;
struct LLNotificationsUtil{static void add(const char*,const LLSD&){++notifications;}};
#define LL_WARNS(x) std::cout
#define LL_WARNS_ONCE(x) std::cout
#define LL_ENDL '\n'
class ALActorChromaKey {
public:
 inline static bool sActive=true,included=true;
 inline static U32 selectedMode=0;
 inline static U64 revisionValue=1;
 enum class StudioPolicy{Look,Skip,Fill};
 static bool instanceKeyed(LLUUID){return included;}
 static U32 mode(){return selectedMode;}static U64 revision(){return revisionValue;}
 static StudioPolicy requestedStudioPolicy(const LLUUID&);static StudioPolicy studioPolicy(const LLUUID&);
 static bool keyedThisFrame();static U64 finalizeSerial();static void beginFinalize();
 static void refuse(const char*);static const std::string& reason();static void provision(S32,S32);
 static void studioFailure(const char*);static void finishStudioFrame(U64);
 static bool snapshotComplete(U64,S32,S32);static bool snapshotRequestAllowed(S32,S32);
 static void snapshotRefused(S32,S32);static const std::string& snapshotPlaceholder();
 static void explicitSnapshotRequest();static bool snapshotRequestExplicit();
 struct SnapshotScope{SnapshotScope();~SnapshotScope();bool mActive,mPrevious;};
 struct SnapshotRequestScope{explicit SnapshotRequestScope(bool);~SnapshotRequestScope();bool mPrevious;};
};
"""+state+'\n'
    signatures=['bool ALActorChromaKey::keyedThisFrame()',
      'ALActorChromaKey::StudioPolicy ALActorChromaKey::requestedStudioPolicy(',
      'ALActorChromaKey::StudioPolicy ALActorChromaKey::studioPolicy(',
      'U64 ALActorChromaKey::finalizeSerial()', 'void ALActorChromaKey::beginFinalize()',
      'void ALActorChromaKey::refuse(', 'const std::string& ALActorChromaKey::reason()',
      'void ALActorChromaKey::provision(', 'void ALActorChromaKey::studioFailure(',
      'void ALActorChromaKey::finishStudioFrame(', 'bool ALActorChromaKey::snapshotComplete(',
      'ALActorChromaKey::SnapshotScope::SnapshotScope()', 'ALActorChromaKey::SnapshotScope::~SnapshotScope()',
      'ALActorChromaKey::SnapshotRequestScope::SnapshotRequestScope(',
      'ALActorChromaKey::SnapshotRequestScope::~SnapshotRequestScope()',
      'bool ALActorChromaKey::snapshotRequestExplicit()', 'void ALActorChromaKey::explicitSnapshotRequest()',
      'bool ALActorChromaKey::snapshotRequestAllowed(', 'void ALActorChromaKey::snapshotRefused(',
      'const std::string& ALActorChromaKey::snapshotPlaceholder()']
    fixture+='\n'.join(method(NV/'alactorchromakey.cpp',sig) for sig in signatures)
    fixture+=r"""
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_WIN32
#define LL_PROFILE_ZONE_NAMED_CATEGORY_WIN32(x)
#define LL_PROFILER_GPU_COLLECT
int presents=0;void SwapBuffers(int){++presents;}
struct LLWindowWin32:LLWindow{int mhDC=1;void swapBuffers();};
"""+method(ROOT/'indra/llwindow/llwindowwin32.cpp','void LLWindowWin32::swapBuffers()')
    fixture+=r"""
int main(){LLWindowWin32 window;using K=ALActorChromaKey;using P=K::StudioPolicy;
for(U32 mode=0;mode<3;++mode)for(bool included:{false,true}){
 K::selectedMode=mode;K::included=included;K::beginFinalize();assert(K::studioPolicy({1})==P::Look);
 auto before=K::finalizeSerial();K::provision(3840,2160);
 P expected=mode==0?(included?P::Look:P::Skip):mode==1?(included?P::Fill:P::Look):(included?P::Fill:P::Skip);
 assert(K::studioPolicy({1})==expected);assert(!K::snapshotComplete(before-1,3840,2160));
 K::finishStudioFrame(before);assert(K::snapshotComplete(before-1,3840,2160));
 assert(!K::snapshotComplete(before,3840,2160));assert(!K::snapshotComplete(before-1,1920,1080));
 K::studioFailure("Glass");K::finishStudioFrame(before);assert(!K::snapshotComplete(before-1,3840,2160));
 K::beginFinalize();K::finishStudioFrame(before);assert(!K::snapshotComplete(before-1,3840,2160));
}
// All Studio early-exit categories have the same unconditional caller contract.
for(int category=0;category<5;++category){auto before=K::finalizeSerial();K::beginFinalize();K::provision(64,64);
 K::finishStudioFrame(K::finalizeSerial());assert(K::snapshotComplete(before,64,64));}
{K::SnapshotScope a;window.swapBuffers();assert(presents==0&&LLWindow::sBlockedPresents==1);assert(LLWindow::sPresentBlocked);{K::SnapshotScope b;assert(LLWindow::sPresentBlocked);}assert(LLWindow::sPresentBlocked);}assert(!LLWindow::sPresentBlocked);
K::sActive=false;{K::SnapshotScope a;window.swapBuffers();assert(presents==1);assert(!LLWindow::sPresentBlocked);}K::sActive=true;
K::refuse("sphere");
{K::SnapshotRequestScope automatic(false);
 assert(K::snapshotRequestAllowed(3840,2160));K::snapshotRefused(3840,2160);assert(notifications==1);
 for(int i=0;i<10;++i)assert(!K::snapshotRequestAllowed(3840,2160));
 K::refuse("missing program");gDisconnected=true;gWindowResized=true;viewer.window.w=2000;
 assert(!K::snapshotRequestAllowed(3840,2160));assert(notifications==1);
 viewer.window.w=2100;assert(!K::snapshotRequestAllowed(3840,2160));
 gWindowResized=false;assert(K::snapshotRequestAllowed(3840,2160));K::snapshotRefused(3840,2160);
 assert(!K::snapshotRequestAllowed(3840,2160));gDisconnected=false;
 assert(K::snapshotRequestAllowed(3840,2160));K::snapshotRefused(3840,2160);
 K::revisionValue++;assert(K::snapshotRequestAllowed(3840,2160));K::snapshotRefused(3840,2160);
 assert(K::snapshotRequestAllowed(4096,2160));K::snapshotRefused(4096,2160);
 K::provision(4096,2160);assert(K::snapshotRequestAllowed(4096,2160));K::refuse("cache");K::snapshotRefused(4096,2160);
 K::explicitSnapshotRequest();assert(K::snapshotRequestAllowed(4096,2160));K::snapshotRefused(4096,2160);
 assert(!K::snapshotRequestAllowed(4096,2160));
 {K::SnapshotRequestScope explicit_action(true);assert(K::snapshotRequestAllowed(4096,2160));}
}
std::cout<<"PASS WP2: production Studio policy table; provisional/complete serial and size; Glass failure; refused/stale serial; R41 no-op completion; nested present scope; refusal latch/settled resize/reconnect/revision/size/suspension/explicit triggers\n";
}
"""
    result=compile_run(OUT,fixture,'contract');print(result.stdout,end='');result.check_returncode()
    # Negative controls mutate complete production functions, requiring behavioral rejection.
    for name,before,after in [('premature_completion','sComplete = false;\n    sKeyReason.clear();','sComplete = true;\n    sKeyReason.clear();'),
                             ('bad_resize_latch','sRefusal.settledWidth != window.getWidth()','sRefusal.windowWidth != window.getWidth()'),
                             ('wrong_instance_policy','case 1: return included ? StudioPolicy::Fill : StudioPolicy::Look;','case 1: return StudioPolicy::Look;')]:
        assert fixture.count(before)==1,name
        result=compile_run(OUT,fixture.replace(before,after),name)
        assert result.returncode,name
        print('PASS negative control:',name)
    display=(NV/'llviewerdisplay.cpp').read_text(encoding='utf-8')
    studio=display.index('LLActorMover::instance().renderStudioGhosts();')
    complete=display.index('ALActorChromaKey::finishStudioFrame(key_serial);',studio)
    blink=display.index('ALPOVBlink::renderLastSceneImage(',complete)
    assert studio<complete<blink
    assert display.count('ALPOVBlink::renderLastSceneImage(')==1
    raw=method(NV/'llviewerwindow.cpp','bool LLViewerWindow::rawSnapshot(')
    assert raw.index('SnapshotScope key_snapshot')<raw.index('glClear(')
    assert raw.index('snapshotComplete(')<raw.index('glReadPixels(')
    assert 'bool ret = !key_refused;' in raw
    print('PASS: actual unconditional Studio completion precedes single blink; rawSnapshot scope/readback/failure seams')
    finalize=method(NV/'pipeline.cpp','void LLPipeline::renderFinalize()')
    # The beauty chart is unchanged until K0 decides; only its post-key overlay
    # must be absent from Alpha matte. A pre-K0 stamp test can never do that.
    presentation=finalize.index('renderFinalizePresent(sourceBuffer,')
    assert 'alphaMatte()' not in finalize[:presentation]
    chart=finalize.index('ALColorChart::instance().presentUnlit(',presentation)
    assert '!ALActorChromaKey::alphaMatte()' in finalize[presentation:chart]
    print('PASS: Alpha-matte chart suppression guards the actual post-K2 overlay; refused preflight retains the original beauty chart')


def preflight_contracts():
    pipeline=(NV/'pipeline.cpp').read_text(encoding='utf-8')
    scope=pipeline[pipeline.index('struct ActorKeyFramebufferScope'):pipeline.index('// Copy from DRAW,')]
    fixture=r"""
#include <cassert>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <utility>
#include <algorithm>
#include <iostream>
using U32=unsigned;using S32=int;using GLint=int;using GLenum=unsigned;using F32=float;
constexpr int GL_DRAW_FRAMEBUFFER_BINDING=1,GL_READ_FRAMEBUFFER_BINDING=2,GL_VIEWPORT=3,GL_DRAW_BUFFER0=4,GL_READ_BUFFER=5,
 GL_DRAW_FRAMEBUFFER=6,GL_READ_FRAMEBUFFER=7,GL_FRONT=8,GL_FRONT_LEFT=9,GL_BACK_LEFT=10,GL_NONE=0,
 GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE=11,GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE=12,GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE=13,
 GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE=14,GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE=15,GL_UNSIGNED_NORMALIZED=16,
 GL_RGB10_A2=17,GL_RGBA8=18,GL_RGB8=19,GL_VERTEX_SHADER=20,GL_FRAGMENT_SHADER=21,GL_NO_ERROR=0,GL_TEXTURE_2D=22;
#define LL_PROFILE_ZONE_NAMED(x)
int draw=7,read=2,readBuffer=102,drawBuffer=103,pack=77,viewport[4]={11,13,64,32},red=8;
int allocations=0,copies=0,error=0,copySource=0,copyReadBuffer=0;
std::map<int,int> buffers={{7,101},{2,102}};
void glGetIntegerv(int name,int* v){if(name==GL_VIEWPORT)std::copy(viewport,viewport+4,v);
 else *v=name==GL_DRAW_FRAMEBUFFER_BINDING?draw:name==GL_READ_FRAMEBUFFER_BINDING?read:name==GL_DRAW_BUFFER0?drawBuffer:buffers[read];}
void glBindFramebuffer(int target,int value){(target==GL_DRAW_FRAMEBUFFER?draw:read)=value;}
void glReadBuffer(int value){buffers[read]=value;}
void glViewport(int x,int y,int w,int h){viewport[0]=x;viewport[1]=y;viewport[2]=w;viewport[3]=h;}
void glGetFramebufferAttachmentParameteriv(int,int,int name,int* v){*v=name==GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE?GL_UNSIGNED_NORMALIZED:name==GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE?2:red;}
int glGetError(){int value=error;error=0;return value;}
void glCopyTexSubImage2D(int,int,int x,int y,int sx,int sy,int w,int h){assert(x==0&&y==0&&sx==11&&sy==13&&w==64&&h==32);assert(pack==77);++copies;copySource=read;copyReadBuffer=buffers[read];}
struct Unit{void bindManual(int,unsigned){}}unit;
struct GL{Unit* getTexUnit(int){return &unit;}}gGL;
struct ActorKeyTextureScope{explicit ActorKeyTextureScope(int){}};
struct LLTexUnit{enum{TT_TEXTURE};};
std::map<std::string,unsigned> settings;
struct Settings{U32 getU32(const char* n){return settings[n];}}gSavedSettings;
template<class T>struct LLCachedControl{
 std::string name;LLCachedControl(Settings&,const char* n,T):name(n){}
 T operator()()const{return T(settings[name]);}operator T()const{return operator()();}
};
std::string refused;
struct ALActorChromaKey{static void preflightControls(){}static bool keyGuardAllows(){return true;}static void refuse(const char* r){refused=r;}static bool suppressed(){return settings["suppress"]!=0;}static U32 mode(){return settings["mode"];}};
struct LLShaderFeatures{};
bool programOK=true,studioOK=true,allocationOK=true;
struct LLGLSLShader{
 std::string mName;LLShaderFeatures mFeatures;int mShaderLevel;bool complete=false;
 std::vector<std::pair<std::string,int>>mShaderFiles;
 bool isComplete(){return complete;}void clearPermutations(){}bool createShader(){return complete=programOK;}void unload(){complete=false;}
};
std::unique_ptr<LLGLSLShader>gActorKeyCompositeProgram;
struct ActorKeyPrograms{LLGLSLShader blur,matte,occluder;};std::unique_ptr<ActorKeyPrograms>sActorKeyPrograms;
struct ActorKeyLensProgram{LLGLSLShader shader;};
std::map<LLGLSLShader*,std::unique_ptr<ActorKeyLensProgram>>sActorKeyLensPrograms;
unsigned gFrameCount=42;
int liveAllocations=0;
struct LLRenderTarget{
 unsigned width=0,height=0,format=0;bool owned=false;
 void release(){if(owned)--liveAllocations;owned=false;width=height=0;}
 ~LLRenderTarget(){release();}
 bool isComplete()const{return width!=0;}U32 getWidth()const{return width;}U32 getHeight()const{return height;}U32 getInternalFormat()const{return format;}
 bool allocate(int w,int h,unsigned f,bool){++allocations;draw=999;read=998;glViewport(0,0,1,1);if(!allocationOK)return false;if(!owned)++liveAllocations;owned=true;width=w;height=h;format=f;return true;}
 unsigned getTexture(){return 123;}
};
// WP1b owns recorder release; its real allocation contract is exercised by actor_key_wp1b_check.
struct ALActorKeyCapture{static ALActorKeyCapture& instance(){static ALActorKeyCapture c;return c;}void release(){}};
struct LLActorMover{static void releaseStudioKeyPrograms(){}static bool preflightStudioKey(const S32 (&v)[4]){assert(draw==7&&v[0]==11&&viewport[2]==64);return studioOK;}};
bool gDisconnected=false,gCubeSnapshot=false,sPrismLensRender=false,sPrismPreviewRender=false,sImpostorRender=false,sRenderingHUDs=false;
int RenderBufferVisualization=-1;bool BDMergeVelocityBuffer=false,BDMergeVelocityDebug=false;
struct LLPipeline{
 struct Pack{}mMainRT,aux;Pack* mRT=&mMainRT;
 LLRenderTarget mActorKeyScene,mActorKeyMatte{32,16,18};S32 mActorKeyDrawFBO=0,mActorKeyViewport[4]={};
 LLRenderTarget mActorKeyCoverage,mActorKeyBlurA,mActorKeyBlurB;
 LLRenderTarget mKeyDiopterCov,mKeyLensStage[3];LLRenderTarget* mKeyLensCurrent=nullptr;
 unsigned mKeyLensFrame=42,mKeyLensRan=0,mKeyLensComplete=0;
 bool lensCoverageValid()const;
 bool mActorKeyReady=false;U32 mActorKeySceneFrame=0,mActorKeyDepthSrc=0,mActorKeyCoverageFrame=0,mActorKeyMatteFrame=0;
 const void* mActorKeyMatteRT=nullptr;
 std::vector<float>mActorKeyTestMatte,mActorKeyTestScene,mActorKeyTestOutput;unsigned mActorKeyTestFrames=0;
 bool matte=true;bool actorKeyMatteValid(){return matte;}
 bool preflightActorKey(bool,bool,bool);
 void releaseActorKeyTargets();
};
"""+scope+method(NV/'pipeline.cpp','bool LLPipeline::lensCoverageValid() const')+method(NV/'pipeline.cpp','bool actor_key_copy(')+'\n'+method(NV/'pipeline.cpp','bool LLPipeline::preflightActorKey(')+'\n'+method(NV/'pipeline.cpp','void LLPipeline::releaseActorKeyTargets()')
    fixture+=r"""
int main(){
 for(bool snapshot:{false,true})for(unsigned mode=0;mode<3;++mode)for(unsigned fail=1;fail<=5;++fail){
  settings["mode"]=mode;settings["ActorKeyDebugFailPreflight"]=fail;studioOK=fail!=3&&fail!=5;
  LLPipeline p;int original=draw;assert(!p.preflightActorKey(false,false,false));assert(draw==original&&read==2&&viewport[0]==11&&viewport[2]==64);assert(copies==0);(void)snapshot;
 }
 settings.clear();studioOK=true;
 for(int bits:{8,10}){red=bits;LLPipeline p;assert(p.preflightActorKey(false,false,false));
  assert(p.mActorKeyScene.getInternalFormat()==(bits==8?GL_RGBA8:GL_RGB10_A2));
  assert(draw==7&&read==2&&viewport[0]==11&&viewport[2]==64);assert(actor_key_copy(p.mActorKeyScene,p.mActorKeyViewport));
  assert(copySource==7&&copyReadBuffer==103&&read==2&&buffers[2]==102&&buffers[7]==101&&pack==77);
  assert(!p.preflightActorKey(true,false,false));assert(!p.preflightActorKey(false,true,false));assert(!p.preflightActorKey(false,false,true));
  p.mKeyLensRan=8;assert(!p.preflightActorKey(false,false,true));
  p.mKeyLensComplete=8;p.mKeyLensCurrent=&p.mActorKeyMatte;
  assert(p.preflightActorKey(false,false,true)); // same-frame first activation
  p.mKeyLensFrame=41;assert(!p.preflightActorKey(false,false,true));
  p.mKeyLensFrame=42;p.mKeyLensRan=p.mKeyLensComplete=0;p.mKeyLensCurrent=nullptr;
  p.matte=false;assert(!p.preflightActorKey(false,false,false));
 }
 LLPipeline p;settings["ActorKeySelfTest"]=1;assert(p.preflightActorKey(false,false,false));assert(p.mActorKeyTestOutput.size()==64*32*4);
 allocationOK=false;LLPipeline q;assert(!q.preflightActorKey(false,false,false));assert(draw==7&&read==2);
 p.releaseActorKeyTargets();assert(liveAllocations==0&&!gActorKeyCompositeProgram);
 assert(p.mActorKeyTestMatte.capacity()==0&&p.mActorKeyTestScene.capacity()==0&&p.mActorKeyTestOutput.capacity()==0);
 assert(p.mActorKeySceneFrame==~0u&&!p.mActorKeyReady);
 std::cout<<"PASS I-OFFPATH2 resource contract: actual releaseActorKeyTargets returns all live key target counts and diagnostic capacities to baseline after ON -> OFF\n";
 std::cout<<"PASS WP2: complete production K0 injection 1..5 across live/snapshot context and three modes; no pre-key framebuffer writes; format/extent; restored framebuffer/viewport; actual DRAW copy preserves unrelated READ buffer and pack buffer; lens/sphere/stale matte refusal; diagnostic storage preallocation\n";
}
"""
    result=compile_run(OUT,fixture,'preflight_copy');print(result.stdout,end='');result.check_returncode()
    bad=fixture.replace('glBindFramebuffer(GL_READ_FRAMEBUFFER, draw);','glBindFramebuffer(GL_READ_FRAMEBUFFER, read);')
    assert bad!=fixture
    result=compile_run(OUT,bad,'bad_copy_source');assert result.returncode
    print('PASS negative control: copying from unrelated READ rejected')

def glass_contracts():
    # Existing clone fixture supplies external seams. Execute the current complete
    # K0/cache functions; this tests cache lifetime, not a substitute viewer draw.
    runtime=ROOT/'.tmp/looks2_20261102/runtime/runtime.cpp'
    if not runtime.exists():
        subprocess.run(['python','scripts/code_tools/clone_looks_runtime_check.py'],check=True,cwd=ROOT)
    fixture=runtime.read_text(encoding='utf-8').split('\nint main(){')[0]
    fixture=fixture.replace(' struct Instance {',''' enum{BACKING_OVERLAY};
 struct Instance;
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 std::vector<Instance>& getInstances();
 bool isVisible(const Instance& i);
 struct Instance {
 LLUUID mId;int mKind=BACKING_OVERLAY,mStyle=64;float mFadeAlpha=1;bool visible=true;''',1)
    fixture+=r'''
std::vector<ALGhostStudio::Instance> glassInstances;
std::vector<ALGhostStudio::Instance>& ALGhostStudio::getInstances(){return glassInstances;}
bool ALGhostStudio::isVisible(const Instance& i){return i.visible;}
U32& gFrameCount=frame;
struct Settings{}gSavedSettings;
unsigned injected=0;bool fillOK=true;U64 serial=1;
template<class T>struct LLCachedControl{LLCachedControl(Settings&,const char*,T){}T operator()()const{return (T)injected;}};
constexpr int GL_NO_ERROR=0;int copyError=0;int glGetError(){int e=copyError;copyError=0;return e;}
struct ALActorChromaKey{
 enum class StudioPolicy{Look,Skip,Fill};inline static StudioPolicy policy=StudioPolicy::Look;
 inline static int failures=0;inline static bool scene=true;inline static U32 selectedMode=0;
 static StudioPolicy requestedStudioPolicy(LLUUID){return policy;}
 static U64 finalizeSerial(){return serial;}static U32 mode(){return selectedMode;}
 static bool sceneGlass(){return scene;}static void studioFailure(const char*){++failures;}
};
struct LLActorMover{
 static bool preflightStudioKey(const S32 (&)[4]);static bool captureStudioKeyScene(const S32 (&)[4]);
};
U64 sStudioKeyBegin=0,sStudioForbiddenAllocations=0;
StudioSceneCache* sStudioKeyCache=nullptr;
bool studio_key_fill_programs(){return fillOK;}
'''
    for sig in ('U32 studio_key_failure()', 'LLImageGL* studio_key_scene(', 'bool LLActorMover::preflightStudioKey(', 'bool LLActorMover::captureStudioKeyScene('):
        fixture+='\n'+method(NV/'llactormover.cpp',sig)
    fixture+='\n'+method(NV/'pipeline.cpp','struct ActorKeyTextureScope')+';\n'
    fixture+='struct LLRenderTarget{U32 getTexture(){return 991;}};\n'
    fixture+=method(NV/'pipeline.cpp','bool actor_key_copy(')
    fixture+=r'''
int main(){using K=ALActorChromaKey;
std::array<LLTexUnit,8> units;for(U32 i=0;i<8;++i){units[i].index=i;gGL.units[i]=&units[i];}
S32 viewport[4]={17,23,3840,2160};glassInstances.resize(1);
units[0].bindManual(LLTexUnit::TT_CUBE_MAP,49,true);binding2d[0]=781;gGL.active=3;
LLRenderTarget target;assert(actor_key_copy(target,viewport));
assert(units[0].type==LLTexUnit::TT_CUBE_MAP&&units[0].texture==49&&units[0].mips&&binding2d[0]==781&&gGL.active==3);
assert(read_fbo==2&&read_buffers[2]==102&&read_buffers[7]==101);
for(bool snapshot:{false,true})for(double now:{1.,10.}){
 gSnapshot=snapshot;LLTimer::now=now;++serial;++frame;
 assert(LLActorMover::preflightStudioKey(viewport));auto& cache=studio_scene_caches().get(snapshot,viewport);
 assert(cache.mReservedSerial==serial&&cache.mReservedFrame==frame&&cache.mLastUse==now&&cache.mInvocation==0);
 int before=allocations;assert(LLActorMover::captureStudioKeyScene(viewport));assert(allocations==before);
 assert(cache.mPreKeySerial==serial);
 assert(studio_key_scene(cache,44,now,viewport,{1,2,20,30},true));assert(allocations==before);
 assert(studio_key_scene(cache,45,now,viewport,{1,2,20,30},false));assert(allocations==before);
 cache.mScene=nullptr;assert(!studio_key_scene(cache,46,now,viewport,{1,2,20,30},false));assert(allocations==before);
 assert(sStudioForbiddenAllocations>0);
 ++serial;++frame;viewport[2]+=8;assert(LLActorMover::preflightStudioKey(viewport));
 before=allocations;assert(LLActorMover::captureStudioKeyScene(viewport));assert(allocations==before);
 injected=6;assert(!studio_key_scene(cache,47,now,viewport,{1,2,20,30},false));injected=0;
}
K::policy=K::StudioPolicy::Fill;int before=allocations;++serial;assert(LLActorMover::preflightStudioKey(viewport));assert(!sStudioKeyCache&&allocations==before);
fillOK=false;assert(!LLActorMover::preflightStudioKey(viewport));fillOK=true;
K::policy=K::StudioPolicy::Look;injected=5;assert(!LLActorMover::preflightStudioKey(viewport));injected=0;
std::cout<<"PASS WP2 D51: actual K0 begin/reserve/Scene-copy/cache guard; first use, >3s idle, live/snapshot, resize, fill no cache, allocator refusal, injected Glass copy failure; zero post-K2 allocations and rejected missing reservation\n";
std::cout<<"PASS actual K2 texture/cache scope preserves cube-map cache, inactive 2D binding, mip flag, active unit and independent READ buffer\n";
}
'''
    result=compile_run(OUT,fixture,'glass_cache');print(result.stdout,end='');result.check_returncode()


if __name__=='__main__':
    contracts()
    preflight_contracts()
    glass_contracts()

