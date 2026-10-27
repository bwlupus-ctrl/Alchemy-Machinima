"""Native chart failure/lifecycle and simulator-message-spy fixtures.

Compiles extracted production bodies against small instrumented boundaries.
No viewer build, GPU context, account access, or network sends.
"""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def method(path, signature):
    source = (ROOT / 'indra/newview' / path).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def source_contracts():
    shader = (ROOT/'indra/newview/llviewershadermgr.cpp').read_text(encoding='utf-8')
    deferred = method('llviewershadermgr.cpp', 'bool LLViewerShaderMgr::loadShadersDeferred()')
    assert 'gColorChartProgram.createShader' not in deferred
    loader = method('llviewershadermgr.cpp', 'bool LLViewerShaderMgr::loadColorChartShaders(')
    assert 'success =' not in loader and 'loadSensorShaders(' not in loader
    assert loader.count('createShader()') == 2  # patch shader + private clone recipe
    assert 'mColorChartAttempted = true' in loader
    assert 'mColorChartLegacyAttempted = true' in loader
    reload = method('llviewershadermgr.cpp', 'void LLViewerShaderMgr::unloadShaders()')
    assert '++mColorChartShaderGeneration' in reload and 'unloadColorChartShaders();' in reload
    chart = (ROOT/'indra/newview/alcolorchart.cpp').read_text(encoding='utf-8')
    assert 'loadSensorShaders(' not in chart
    assert 'getLocalTexture(raw.get(), false)' in chart
    assert 'static_cast<F32>(rect.getWidth())' in chart
    idle = method('alcolorchart.cpp', 'void ALColorChart::idle()')
    assert idle.index('setPositionGlobal(position, false)') < idle.index('updateMoveNormalAsync')
    pin = method('alcolorchart.cpp', 'void ALColorChart::pinHere()')
    for token in ('getPositionAgent()', 'getWorldRotation()', 'getScale()'):
        assert token in pin
    readback = method('alcolorchart.cpp', 'void ALColorChart::debugReadback()')
    assert readback.index('if (draw_framebuffer != 0)') < readback.index('ScopedReadbackState saved_state')
    assert readback.index('INCONCLUSIVE offscreen/snapshot') < readback.index('glReadPixels')
    shutdown = method('alcolorchart.cpp', 'void ALColorChart::shutdownSession()')
    assert 'instance()' not in shutdown and 'if (sChart)' in shutdown
    disconnect = method('llappviewer.cpp', 'void LLAppViewer::disconnectViewer()')
    assert disconnect.index('ALColorChart::shutdownSession()') < disconnect.index('resetClass()')
    assert 'ALColorChart::shutdownSession()' in method('llappviewer.cpp', 'bool LLAppViewer::cleanup()')
    select = method('llselectmgr.cpp', 'bool LLSelectMgr::canSelectObject(')
    assert select.index('object->isColorChart()') < select.index('if (mForceSelection)')
    nearby = method('llagentlistener.cpp', 'void LLAgentListener::getNearbyObjectsList(')
    nearest = method('llagentlistener.cpp', 'LLViewerObject * LLAgentListener::findObjectClosestTo(')
    assert 'object->isLocalOnly()' in nearby and 'cur_object->isLocalOnly()' in nearest
    # Every direct sit sender in this source tree's audited routes is checked.
    for path, signature, guard in (
        ('llagentlistener.cpp', 'void LLAgentListener::requestSit(', 'object->isLocalOnly()'),
        ('llviewermenu.cpp', 'void handle_object_sit(LLViewerObject*', 'object->isLocalOnly()'),
        ('rlvhandler.cpp', 'ERlvCmdRet RlvForceHandler<RLV_BHVR_SIT>::onCommand', 'pObj->isLocalOnly()'),
    ):
        body = method(path, signature)
        assert body.index(guard) < body.index('_PREHASH_AgentRequestSit')
    # Other independent senders discovered in the UUID/selection audit.
    contracts = (
        ('animationexplorer.cpp', 'void AnimationExplorer::requestObjectName(', 'vo->isLocalOnly()', '_PREHASH_ObjectSelect'),
        ('llviewermessage.cpp', 'void give_money(', 'target->isLocalOnly()', '_PREHASH_MoneyTransferRequest'),
        ('llfloaterpay.cpp', 'void LLFloaterPay::payViaObject(', 'object->isLocalOnly()', '_PREHASH_RequestPayPrice'),
        ('llfloaterpay.cpp', 'void LLFloaterPay::give(', 'target->isLocalOnly()', 'mCallback(mTargetUUID'),
        ('llfloaterreporter.cpp', 'void LLFloaterReporter::getObjectInfo(', 'objectp->isLocalOnly()', '_PREHASH_RequestObjectPropertiesFamily'),
        ('animationexplorer.cpp', 'void AnimationExplorer::requestObjectName(', 'vo->isLocalOnly()', '_PREHASH_ObjectDeselect'),
        ('alfloatersceneexplorer.cpp', 'void ALFloaterSceneExplorer::drainPropsQueue(', 'obj->isLocalOnly()', 'local_ids.push_back'),
        ('lltooldraganddrop.cpp', 'void LLToolDragAndDrop::dropObject(', 'raycast_target->isLocalOnly()', '_PREHASH_RezObject'),
        ('rlvlocks.cpp', 'void RlvAttachmentLockWatchdog::detach(const LLViewerObject*', 'pAttachObj->isLocalOnly()', '_PREHASH_ObjectDetach'),
        ('lltoolgrab.cpp', 'void send_ObjectGrab_message(', 'object->isLocalOnly()', '_PREHASH_ObjectGrab'),
        ('lltoolgrab.cpp', 'void send_ObjectDeGrab_message(', 'object->isLocalOnly()', '_PREHASH_ObjectDeGrab'),
    )
    for path, signature, guard, sender in contracts:
        body = method(path, signature)
        assert body.index(guard) < body.index(sender), (path, signature)
    marshal = method('llselectmgr.cpp', 'void LLSelectMgr::sendListToRegions(LLObjectSelectionHandle')
    assert marshal.count('node->getObject() && !node->getObject()->isLocalOnly()') == 2
    derez = method('llviewermenu.cpp', 'static void derez_objects(')
    assert derez.index('!object->isLocalOnly()') < derez.index('objectsp = &sim_objects') < derez.index('_PREHASH_DeRezObject')
    for name,klass in [('llfloaterbuy.cpp','LLFloaterBuy'),('llfloaterbuycontents.cpp','LLFloaterBuyContents')]:
        body = method(name, 'void '+klass+'::show(')
        assert body.index('object->isLocalOnly()') < body.index('showTypedInstance')
    return dict(optional_loader=True, lazy_deferred=True, private_encoder=True,
                disconnect_before_world_teardown=True, undamped_lit=True,
                offscreen_readback_rejected=True, independent_sender_contracts=len(contracts)+3)


PREFIX = r"""
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using U32=std::uint32_t; using U64=std::uint64_t; using F32=float; using S32=int;
using std::make_pair;
#define LL_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define GL_VERTEX_SHADER 1
#define GL_FRAGMENT_SHADER 2
#define GL_RGBA32F 3
static int warnings=0,creates=0,gl_calls=0,fail_at=0,allocations=0,presents=0,draws=0;
static bool fail_alloc=false,gDisconnected=false;
static std::ostringstream messages;
std::ostream& warning(){++warnings;return messages;}
#define LL_WARNS(x) warning()
#define LL_ENDL std::endl
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Settings {
 std::map<std::string,int> values;
 void setBOOL(const char* key,bool value){values[key]=value;}
 void setS32(const char* key,int value){values[key]=value;}
} gSavedSettings;
template<class T> struct LLCachedControl {
 std::string key;
 LLCachedControl(Settings&,const char* name,T value):key(name){gSavedSettings.values.emplace(name,int(value));}
 T operator()()const{return T(gSavedSettings.values[key]);}
};
struct LLShaderFeatures{};
struct LLGLSLShader {
 bool complete=false,mUniformsDirty=false;
 int mShaderLevel=0;
 std::string mName="Scene";
 LLShaderFeatures mFeatures;
 std::vector<std::pair<std::string,int>> mShaderFiles;
 std::map<std::string,std::string> mDefines;
 bool isComplete()const{return complete;}
 bool createShader(){++creates;++gl_calls;complete=creates!=fail_at;return complete;}
 void unload(){++gl_calls;complete=false;mShaderFiles.clear();mDefines.clear();}
 void clearPermutations(){mDefines.clear();}
 void addPermutations(const std::map<std::string,std::string>& d){mDefines=d;}
 void addPermutation(const char* key,const char* value){mDefines[key]=value;}
};
LLGLSLShader gColorChartProgram,gColorChartExposureProgram,gColorChartCameraProgram[4];
LLGLSLShader gCGTonemapProgram,gCGTonemapLegacyGammaProgram,gCGTonemapColorgradeProgram,gCGTonemapColorgradeLegacyGammaProgram;
struct LLViewerShaderMgr {
 enum {SHADER_DEFERRED};
 std::vector<int> mShaderLevel{3};
 std::vector<LLGLSLShader*> mShaderList;
 bool mColorChartAttempted=false,mColorChartLegacyAttempted=false;
 U64 mColorChartShaderGeneration=1;
 static LLViewerShaderMgr* instance(){static LLViewerShaderMgr m;return &m;}
 U64 colorChartShaderGeneration()const{return mColorChartShaderGeneration;}
 bool loadColorChartShaders(bool);
 void unloadColorChartShaders();
};
struct LLVector3 {
 F32 x=0;
 LLVector3()=default;
 LLVector3(F32 a,F32,F32):x(a){}
 LLVector3 operator-(LLVector3 b)const{return LLVector3(x-b.x,0,0);}
 F32 length()const{return std::fabs(x);}
 static const LLVector3 zero;
};
const LLVector3 LLVector3::zero{};
using LLVector3d=LLVector3;
struct LLQuaternion{};
namespace glm {struct mat4 {explicit mat4(float){}};}
namespace ALColorChartData {constexpr int WIDTH=880,HEIGHT=736;}
struct LLRenderTarget {
 bool complete=false;
 bool isComplete()const{return complete;}
 bool allocate(int,int,int,bool){++allocations;complete=!fail_alloc;return complete;}
 void release(){complete=false;}
};
struct LLPipeline {
 enum class SensorStage {Exposure,Camera};
 U32 colorCorrectPermutation(bool,bool){return 4;}
 void colorCorrect(LLRenderTarget*,LLRenderTarget*,bool,bool,SensorStage,LLGLSLShader* p){
  require(p->isComplete(),"unavailable encoder bound");
 }
} gPipeline;
static bool unlit(){return true;}
struct ALColorChart {
 LLRenderTarget mLinear,mExposed;
 bool mShaderResourcesRequested=false,mAtlasFailed=false,mPinned=false,mCheckRegionPin=false,mLastSensor=false;
 U64 mShaderGeneration=0;
 U32 mLastDrawFrame=0,mReadbackFrame=0;
 LLVector3d mPosition;
 LLQuaternion mRotation;
 LLVector3 mScale;
 void* mTexture=nullptr;
 static void shutdownSession();
 void destroyPrim(){}
 void releaseResources();
 bool ensureShaders(bool);
 void renderLegacy(LLRenderTarget*,const glm::mat4&,const glm::mat4&);
 void drawChart(LLRenderTarget*,const glm::mat4&,const glm::mat4&,bool,LLRenderTarget* =nullptr,bool=false){++draws;}
};
static ALColorChart* sChart=nullptr;

// Real requestSit and menu sit bodies; every simulated network call is spied.
struct LLSDValue {
 int value;
 operator int()const{return value;}
 int asUUID()const{return value;}
};
struct LLSD {
 std::map<std::string,int> data;
 bool has(const char* key)const{return data.count(key)!=0;}
 LLSDValue operator[](const char* key)const{return {data.at(key)};}
};
static int sends=0,sit_messages=0,last_id=-1,errors=0;
enum {LL_PCODE_VOLUME=1,AGENT_CONTROL_SIT_ON_GROUND,RLV_BHVR_STANDTP};
enum {_PREHASH_AgentRequestSit,_PREHASH_AgentData,_PREHASH_AgentID,_PREHASH_SessionID,
      _PREHASH_TargetObject,_PREHASH_TargetID,_PREHASH_Offset};
struct Region {void sendReliableMessage(){++sends;}};
struct LLViewerObject {
 bool local=false,dead=false,attachment=false;
 int mID=123;
 LLVector3 position;
 Region region;
 bool isLocalOnly()const{return local;}
 bool isDead()const{return dead;}
 bool isAttachment()const{return attachment;}
 int getPCode()const{return LL_PCODE_VOLUME;}
 LLVector3 getPositionRegion()const{return position;}
 Region* getRegion(){return &region;}
};
struct ObjectList {
 std::vector<LLViewerObject*> objects;
 LLViewerObject* findObject(int id){for(auto* o:objects)if(o->mID==id)return o;return nullptr;}
 int getNumObjects()const{return int(objects.size());}
 LLViewerObject* getObject(int i){return objects.at(std::size_t(i));}
} gObjectList;
struct Agent {
 int getID()const{return 10;}
 int getSessionID()const{return 20;}
 LLVector3 getPositionGlobal()const{return {};}
 void standUp(){}
 void setControlFlags(int){}
} gAgent;
struct Message {
 void newMessageFast(int type){require(type==_PREHASH_AgentRequestSit,"wrong message");++sit_messages;}
 void nextBlockFast(int){}
 void addUUIDFast(int tag,int id){if(tag==_PREHASH_TargetID)last_id=id;}
 void addVector3Fast(int,const LLVector3&){}
} message;
Message* gMessageSystem=&message;
struct Response {Response(const LLSD&,const LLSD&){}void error(const char*){++errors;}};
struct rlv_handler_t {static bool isEnabled(){return false;}};
namespace RlvActions {
 bool canSit(LLViewerObject*,const LLVector3& =LLVector3()){return true;}
}
struct RlvHandler {bool hasBehaviour(int){return false;}void setSitSource(const LLVector3&){}} gRlvHandler;
struct Avatar {bool isSitting(){return false;}} avatar;
Avatar* gAgentAvatarp=&avatar;
bool isAgentAvatarValid(){return false;}
LLVector3 ll_vector3_from_sd(int x){return LLVector3(float(x),0,0);}
struct LLAgentListener {
 Agent& mAgent=gAgent;
 void requestSit(const LLSD&)const;
 LLViewerObject* findObjectClosestTo(const LLVector3&,bool=false)const;
};
void handle_object_sit(LLViewerObject*,const LLVector3&);
"""


TESTS = r"""
void clear_counts(){warnings=creates=gl_calls=allocations=presents=draws=0;fail_at=0;fail_alloc=false;}
void frame(ALColorChart& chart){
 ++presents; // scene presentation is independent of optional chart success
 chart.renderLegacy(nullptr,glm::mat4(1.f),glm::mat4(1.f));
}
int main(){
 try {
  auto& manager=*LLViewerShaderMgr::instance();
  // Startup/reload while hidden: no program creation, bind, or unload GL work.
  clear_counts();
  for(int i=0;i<5;++i){manager.unloadColorChartShaders();++manager.mColorChartShaderGeneration;}
  require(gl_calls==0 && creates==0 && manager.mShaderList.empty(),"hidden reload performed GL work");
  ALColorChart::shutdownSession();
  require(gl_calls==0 && sChart==nullptr,"shutdown constructed hidden chart");
  gCGTonemapProgram.complete=gCGTonemapLegacyGammaProgram.complete=
    gCGTonemapColorgradeProgram.complete=gCGTonemapColorgradeLegacyGammaProgram.complete=true;
  ALColorChart chart;
  fail_at=1;
  for(int i=0;i<50;++i)frame(chart);
  require(creates==1 && warnings==1 && presents==50 && draws==0,"chart compile failure not isolated/latched");
  require(gCGTonemapProgram.isComplete(),"chart failure broke scene program");

  manager.unloadColorChartShaders();clear_counts();fail_at=2;
  for(int i=0;i<50;++i)frame(chart);
  require(creates==2 && warnings==1 && presents==50 && draws==0,"encoder failure retried or broke presentation");
  require(!gColorChartExposureProgram.isComplete() && gCGTonemapProgram.isComplete(),"encoder failure contaminated scene");

  manager.unloadColorChartShaders();clear_counts();
  gSavedSettings.values["AlchemyColorChartForceShaderFailure"]=1;
  for(int i=0;i<10;++i)frame(chart);
  require(creates==0 && warnings==1,"forced chart failure not latched");
  manager.unloadColorChartShaders();clear_counts();
  gSavedSettings.values["AlchemyColorChartForceShaderFailure"]=2;
  for(int i=0;i<10;++i)frame(chart);
  require(creates==1 && warnings==1,"forced encoder failure not latched");
  manager.unloadColorChartShaders();clear_counts();
  gSavedSettings.values["AlchemyColorChartForceShaderFailure"]=0;
  fail_alloc=true;
  for(int i=0;i<50;++i)frame(chart);
  require(allocations==1 && presents==50 && warnings==1,"allocation failure retried");
  // The same reset used on shader reload permits one new attempt.
  manager.unloadColorChartShaders();++manager.mColorChartShaderGeneration;clear_counts();
  frame(chart);
  require(creates==6 && allocations==2 && draws==2,"shader generation did not retry/recover");
  require(manager.mShaderList.size()==6,"private shader registration drift");
  for(int i=0;i<20;++i)frame(chart);
  require(creates==6 && allocations==2 && draws==42,"ready chart reinitialized each frame");
  sChart=&chart;chart.mPinned=true;chart.mCheckRegionPin=true;
  ALColorChart::shutdownSession();
  require(!chart.mPinned && !chart.mCheckRegionPin && !chart.mShaderResourcesRequested,"session state retained");
  require(manager.mShaderList.empty() && !gColorChartProgram.isComplete(),"shutdown retained GPU programs");
  require(gSavedSettings.values["AlchemyColorChartShow"]==0 && gSavedSettings.values["AlchemyColorChartPlacement"]==0,"session settings retained");
  int before=gl_calls;
  ALColorChart::shutdownSession();
  require(before==gl_calls,"shutdown not idempotent");

  // UUID and nearest-position paths for all six local object kinds.
  LLAgentListener listener;
  LLViewerObject object,remote;
  object.local=true;remote.mID=124;remote.position=LLVector3(20,0,0);
  gObjectList.objects={&object,&remote};
  for(int kind=0;kind<6;++kind){
   object.mID=200+kind;
   LLSD event;event.data["obj_uuid"]=object.mID;
   listener.requestSit(event);
   LLSD position;position.data["position"]=0;
   listener.requestSit(position); // nearest skips local object, chooses remote
   handle_object_sit(&object,LLVector3());
  }
  require(sends==6 && sit_messages==6 && last_id==remote.mID && errors==6,"local-only sit escaped message spy");
  require(listener.findObjectClosestTo(LLVector3())==&remote,"nearest chose local object");
  LLSD event;event.data["obj_uuid"]=remote.mID;
  listener.requestSit(event);
  handle_object_sit(&remote,LLVector3());
  require(sends==8 && sit_messages==8 && last_id==remote.mID,"normal sit positive control failed");
  std::cout<<"PASS native chart policy: optional failure, retry latch, hidden reload, session reset, sit spy"<<std::endl;
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
}
"""


def run(out):
    out.mkdir(parents=True,exist_ok=True)
    contracts=source_contracts()
    bodies='\n\n'.join(method(p,s) for p,s in (
        ('llviewershadermgr.cpp','void LLViewerShaderMgr::unloadColorChartShaders()'),
        ('llviewershadermgr.cpp','bool LLViewerShaderMgr::loadColorChartShaders('),
        ('alcolorchart.cpp','void ALColorChart::shutdownSession()'),
        ('alcolorchart.cpp','void ALColorChart::releaseResources()'),
        ('alcolorchart.cpp','bool ALColorChart::ensureShaders('),
        ('alcolorchart.cpp','void ALColorChart::renderLegacy('),
        ('llagentlistener.cpp','void LLAgentListener::requestSit('),
        ('llagentlistener.cpp','LLViewerObject * LLAgentListener::findObjectClosestTo('),
        ('llviewermenu.cpp','void handle_object_sit(LLViewerObject*'),
    ))
    vc=sorted((Path(os.environ['ProgramFiles'])/'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))[-1]
    def build(name,production):
        source=out/(name+'.cpp')
        binary=out/(name+'.exe')
        source.write_text(PREFIX+production+TESTS,encoding='utf-8')
        command=out/(name+'.cmd')
        command.write_text('@echo off\ncall "'+str(vc)+'" >nul 2>&1\ncl /nologo /std:c++17 /EHsc /W3 /WX "'+
            str(source)+'" /Fe:"'+str(binary)+'" /Fo"'+out.as_posix()+'/"\n',encoding='utf-8')
        compiled=subprocess.run(['cmd','/d','/c',str(command)],cwd=ROOT,capture_output=True,text=True,timeout=90)
        (out/(name+'-compile.log')).write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
        assert compiled.returncode==0,compiled.stdout+compiled.stderr
        result=subprocess.run([str(binary)],cwd=ROOT,capture_output=True,text=True,timeout=15)
        (out/(name+'-run.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
        return result
    good=build('policy',bodies)
    assert good.returncode==0,good.stdout+good.stderr
    print(good.stdout.strip(),flush=True)
    # A compiling regression must fail its behavioral checks, not its build.
    mutations={
        'retry':('if (!mColorChartAttempted)','if (true)'),
        'sit':('if (object && object->isLocalOnly())','if (false)'),
        'nearest':('!cur_object->isLocalOnly() && ',''),
        'hidden_reload':('if (shader.isComplete())','if (true)'),
        'shutdown':('sChart->mPinned = false;','sChart->mPinned = true;'),
    }
    for name,(old,new) in mutations.items():
        assert old in bodies,name
        rejected=build('policy_mutant_'+name,bodies.replace(old,new,1))
        assert rejected.returncode!=0,'NEGATIVE CONTROL SURVIVED: '+name
    evidence=dict(contracts,production_methods=9,native_WX=True,frames_per_failure=50,
                  local_kinds=6,negative_mutations=list(mutations),status='PASS')
    (out/'results.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    return evidence


if __name__=='__main__':
    print(json.dumps(run(ROOT/'.tmp/color_chart_20261027/tests/policy'),indent=2))
