"""Execute production key/mouse ownership and fixed-storage clean telemetry."""
from pathlib import Path
import subprocess
from native_check_tools import check_directory, compile_run
ROOT=Path(__file__).resolve().parents[2]; NV=ROOT/'indra/newview'
FIXTURE=r"""
#include "alkeyframeinputgate.h"
#include "alcleantelemetry.h"
#include <iostream>
#include <cstdlib>
#include <map>
#include <set>
#include <type_traits>
#include <vector>
class ALKeyframePlayer {public:static void clean(bool v){ALCleanFrame::setActive(v);}};
void require(bool v,const char* name){if(!v){std::cout<<"FAIL "<<name<<"\n";std::exit(1);}}
using G=ALKeyframeInputGate;
using Log=std::vector<std::string>;
constexpr unsigned SHIFT=1,CTRLALT=3;
constexpr bool RETAIN_PAIRED=false;
// Executable model of LLViewerInput dispatch: handleGlobalBindsKeyDown/Up (native global route),
// handleKeyUp (stock sHotGazeKeys owner), scanKey(KEY) and scanKey(bindings). The gate calls
// (phaseOf / allow) are the production ones; everything else mirrors stock and holds no clean state.
struct Bind{unsigned key,mask;std::string action;bool global;};
struct Sim{
 std::vector<Bind> binds;Log log;std::map<unsigned,std::string> owner;bool clean=false,agentBranch=false,retainPairedOwner=false;const G gate;
 static bool hot(const std::string& a){return a.rfind("director_hot_gaze_",0)==0;}
 bool scan(bool global,unsigned key,unsigned mask,bool down,bool up,bool level,bool repeat=false){
  for(const auto& b:binds){
   if(b.global!=global||b.key!=key) continue;
   if(hot(b.action)?b.mask!=mask:(b.mask&mask)!=b.mask) continue;
   const bool sitting=b.action.ends_with("_sitting"),agent=clean&&sitting&&agentBranch;
   const std::string action=agent?"agent_seated_control":b.action;
   const bool move=b.action.rfind("move_",0)==0&&!agent;
   if(!gate.allow(action,G::phaseOf(down,up,repeat),clean,move)) return true;
   if(down&&!repeat){if(hot(b.action)&&G::ownsHotGaze(clean))owner[key]=b.action;log.push_back("D:"+action);if(G::pairedUp(clean,down,up,repeat)){log.push_back("U:"+b.action);if(!retainPairedOwner)owner.erase(key);}return true;}
   if(up){log.push_back("U:"+b.action);return true;}
   if(level){log.push_back("L:"+action);return true;}
   return false;}
  return false;}
 void nativeDown(unsigned key,unsigned mask){scan(true,key,mask,true,false,false);}
 void nativeUp(unsigned key,unsigned mask){
  scan(true,key,mask,false,true,false);
  auto h=owner.find(key);if(h!=owner.end()){log.push_back("U:"+h->second);owner.erase(h);}}
 void scanKeyboard(unsigned key,unsigned mask,bool down,bool up,bool level){
  if(up){auto h=owner.find(key);if(h!=owner.end()){log.push_back("U:"+h->second);owner.erase(h);return;}}
  scan(false,key,mask,down,up,level);}
 void idleRelease(const std::set<unsigned>& physicallyDown){for(auto it=owner.begin();it!=owner.end();){if(!physicallyDown.count(it->first)){log.push_back("U:"+it->second);it=owner.erase(it);}else ++it;}} // releaseHotGazeKeys
 void gestureDown(){log.push_back("D:gesture");} // handleKey clean branch: triggerGesture, no record
 void gestureUp(){log.push_back("U:gesture");}   // handleKeyUp: stock triggerGestureRelease
};
int count(const Log& l,const std::string& s){int n=0;for(auto& x:l)n+=x==s;return n;}
int main(int argc,char** argv){
 std::string selected=argc>1?argv[1]:"all";
 auto run=[&](const char* n){return selected=="all"||selected==n;};
 if(run("up_rules")){
  const G g;
  // UP is always delivered, even for denied actions, unless the UP itself has a side effect.
  for(auto a:{"pan_up","spin_around_ccw","roll_left","edit_avatar_spin_ccw","toggle_viewfinder","toggle_colour_chart","toggle_actor_key","teleport_to","walk_to"})
   require(g.allow(a,G::Phase::Up,true),"UP always delivered");
  require(g.allow("move_forward",G::Phase::Up,true,true)&&g.allow("move_backward_fast",G::Phase::Up,true,true),"UP always delivered");
  for(auto a:{"start_chat","start_gesture"}) require(!g.allow(a,G::Phase::Up,true),"no UI from UP during clean");
  for(auto a:{"move_forward_sitting","spin_around_ccw_sitting"}) require(!g.allow(a,G::Phase::Up,true,true),"no UI from UP during clean");
  for(auto a:{"pan_up","start_chat","start_gesture","toggle_viewfinder","teleport_to","walk_to"})
   require(!g.allow(a,G::Phase::Down,true)&&!g.allow(a,G::Phase::Level,true),"denied DOWN and LEVEL dropped");
  require(!g.allow("move_forward",G::Phase::Down,true,true)&&!g.allow("move_forward",G::Phase::Level,true,true),"denied DOWN and LEVEL dropped");
  for(auto a:{"voice_follow_key","toggle_voice","push_forward","run_forward","pov_eye_blink","director_hot_gaze_1","keyframe_play_stop","agent_seated_control","jump"})
   for(auto ph:{G::Phase::Down,G::Phase::Level,G::Phase::Up}) require(g.allow(a,ph,true),"allowed actions keep working in Play");
  std::cout<<"PASS stateless rules: denied DOWN/LEVEL dropped; UP always delivered except opener/camera UPs; walking/PTT/blink/gaze keep working\n";
 }
 if(run("feature_off")){
  // No ALCleanFrame: every route byte-identical to stock, openers and denied actions included.
  const G g;
  for(auto a:{"start_chat","start_gesture","pan_up","toggle_viewfinder","teleport_to","walk_to","move_forward_sitting"})
   for(auto ph:{G::Phase::Down,G::Phase::Level,G::Phase::Up}) require(g.allow(a,ph,false,true),"feature off equals stock");
  Sim s;s.binds={{87,SHIFT,"start_gesture",false},{65,0,"start_chat",false},{66,0,"move_forward",false},{67,0,"pan_up",false},{87,0,"voice_follow_key",true}};
  s.nativeDown(87,0);s.scanKeyboard(87,0,true,false,true);s.scanKeyboard(65,0,true,false,true);s.scanKeyboard(66,0,true,false,true);s.scanKeyboard(67,0,true,false,true);
  s.scanKeyboard(87,SHIFT,false,false,true);
  s.nativeUp(87,SHIFT);s.scanKeyboard(87,SHIFT,false,true,false);s.scanKeyboard(65,0,false,true,false);s.scanKeyboard(66,0,false,true,false);s.scanKeyboard(67,0,false,true,false);
  const Log stock{"D:voice_follow_key","D:start_chat","D:move_forward","D:pan_up","L:start_gesture","U:voice_follow_key","U:start_gesture","U:start_chat","U:move_forward","U:pan_up"};
  require(s.log==stock,"feature off equals stock");
  std::cout<<"PASS feature off = stock on global/native/scan routes incl. denied actions and openers\n";
 }
 if(run("focus_loss")){
  // Hold PTT in Play, lose focus (synthetic native UP; resetKeys leaves no scan UP), Play ends, fresh PTT.
  Sim s;s.binds={{86,0,"voice_follow_key",true}};s.clean=true;
  s.nativeDown(86,0);s.nativeUp(86,0);
  require(s.log==Log{"D:voice_follow_key","U:voice_follow_key"},"focus loss UP delivered once");
  s.clean=false;s.nativeDown(86,0);s.nativeUp(86,0);
  require(count(s.log,"U:voice_follow_key")==2&&count(s.log,"D:voice_follow_key")==2,"fresh PTT after Play gets its UP");
  std::cout<<"PASS focus loss in Play/Play exit/fresh PTT: every press gets exactly its UP\n";
 }
 if(run("shift_unsampled")){
  // W global PTT, Shift+W start_gesture; Shift pressed and W released before ANY LEVEL scan.
  Sim s;s.binds={{87,SHIFT,"start_gesture",false},{87,0,"voice_follow_key",true}};
  auto play=[&](bool clean){s.log.clear();s.clean=clean;s.nativeDown(87,0);s.scanKeyboard(87,0,true,false,true);
   s.nativeUp(87,SHIFT);s.scanKeyboard(87,SHIFT,false,true,false);return s.log;};
  require(play(false)==Log{"D:voice_follow_key","U:voice_follow_key","U:start_gesture"},"stock opens chat on this transition");
  require(play(true)==Log{"D:voice_follow_key","U:voice_follow_key"},"unsampled Shift transition opens no chat");
  std::cout<<"PASS unsampled Shift transition (zero LEVEL scans): PTT released, no chat during Play, stock outside\n";
 }
 if(run("press_release")){
  // One key = global PTT + ordinary run_forward; pressed and released between two scans.
  auto go=[&](bool clean){Sim s;s.binds={{87,0,"voice_follow_key",true},{87,0,"run_forward",false}};s.clean=clean;
   s.nativeDown(87,0);s.nativeUp(87,0);s.scanKeyboard(87,0,true,true,true);return s.log;};
  const Log stock=go(false);
  require(stock==Log{"D:voice_follow_key","U:voice_follow_key","D:run_forward"},"stock press+release baseline");
  require(go(false)==stock,"press+release between scans equals stock");
  const Log play=go(true);
  bool running=false;for(auto& e:play){if(e=="D:run_forward")running=true;if(e=="U:run_forward")running=false;}
  require(!running,"press+release in Play leaves no running state");
  require(play==Log{"D:voice_follow_key","U:voice_follow_key","D:run_forward","U:run_forward"},"press+release in Play delivers DOWN then UP");
  Sim d;d.binds={{70,0,"pan_up",false},{71,0,"start_gesture",false}};d.clean=true;
  d.scanKeyboard(70,0,true,true,true);d.scanKeyboard(71,0,true,true,true); // denied press+release between scans
  require(d.log.empty(),"denied press+release between scans stays dropped");
  std::cout<<"PASS press+release between scans: clean routing identical to stock, no extra or missing event\n";
 }
 if(run("release_after_play")){
  Sim s;s.binds={{86,0,"voice_follow_key",true}};s.clean=true;
  s.nativeDown(86,0);s.gestureDown();s.clean=false; // Play ends while PTT and a gesture key are held
  s.nativeUp(86,0);s.gestureUp();                  // released before any further scan
  require(s.log==Log{"D:voice_follow_key","D:gesture","U:voice_follow_key","U:gesture"},"release after Play ends delivers exactly one UP");
  std::cout<<"PASS release right after Play ends: exactly one UP for PTT and gesture\n";
 }
 if(run("hotgaze_preplay")){
  auto go=[&](bool leaseDuringHold){Sim s;s.binds={{49,CTRLALT,"director_hot_gaze_1",false}};
   s.scanKeyboard(49,CTRLALT,true,false,true);s.clean=leaseDuringHold;s.scanKeyboard(49,CTRLALT,false,false,true);
   s.nativeUp(49,CTRLALT);s.scanKeyboard(49,CTRLALT,false,true,false);return s;};
  const Sim stock=go(false),play=go(true);
  require(stock.log==Log{"D:director_hot_gaze_1","L:director_hot_gaze_1","U:director_hot_gaze_1","U:director_hot_gaze_1"},"stock hot gaze baseline");
  require(play.log==stock.log&&play.owner.empty(),"hot gaze UP routes equal stock");
  std::cout<<"PASS pre-Play hot gaze through native UP (stock owner) and scan UP: identical to stock, owner consumed, nothing recorded\n";
 }
 if(run("hotgaze_play_modifier")){
  // Ctrl+Alt+1 pressed DURING Play, modifiers released first, then the key: stock owner releases the gaze.
  Sim s;s.binds={{49,CTRLALT,"director_hot_gaze_1",false}};s.clean=true;
  s.scanKeyboard(49,CTRLALT,true,false,true);
  s.scanKeyboard(49,0,false,false,true);              // Ctrl/Alt released: exact-mask lookup misses
  s.scanKeyboard(49,0,false,true,false);              // key released
  require(count(s.log,"U:director_hot_gaze_1")==1&&s.owner.empty(),"hot gaze started in Play released after modifier change");
  std::cout<<"PASS hot gaze started in Play, modifiers released before the key: gaze released via stock owner\n";
 }
 if(run("hotgaze_play_paired")){
  // Ctrl+Alt+1 pressed and released between scans during Play (modifiers held): native UP precedes the owner,
  // the combined-edge scan delivers DOWN+UP, then the idle releaseHotGazeKeys pass must add nothing.
  Sim s;s.binds={{49,CTRLALT,"director_hot_gaze_1",false}};s.clean=true;s.retainPairedOwner=RETAIN_PAIRED;
  s.nativeDown(49,CTRLALT);s.nativeUp(49,CTRLALT);s.scanKeyboard(49,CTRLALT,true,true,true);
  s.idleRelease({});
  require(count(s.log,"U:director_hot_gaze_1")==1&&count(s.log,"D:director_hot_gaze_1")==1&&s.owner.empty(),"hot gaze combined edge releases exactly once");
  std::cout<<"PASS hot gaze press+release between scans in Play: one UP, owner consumed, idle release adds nothing\n";
 }
 if(run("hotgaze_play_focus")){
  // Same, but focus loss: resetKeys drops the scan UP, only the synthetic native UP arrives.
  Sim s;s.binds={{49,CTRLALT,"director_hot_gaze_1",false}};s.clean=true;
  s.scanKeyboard(49,CTRLALT,true,false,true);
  s.nativeUp(49,0);
  require(count(s.log,"U:director_hot_gaze_1")==1&&s.owner.empty(),"hot gaze started in Play released after focus loss");
  std::cout<<"PASS hot gaze started in Play, focus loss before release: gaze released via stock owner\n";
 }
 if(run("ptt_preplay")){
  Sim s;s.binds={{87,0,"run_forward",false},{87,0,"voice_follow_key",true}};
  s.nativeDown(87,0);s.clean=true;s.scanKeyboard(87,0,false,false,true);
  s.nativeUp(87,0);s.scanKeyboard(87,0,false,true,false);
  require(s.log==Log{"D:voice_follow_key","L:run_forward","U:voice_follow_key","U:run_forward"},"pre Play PTT and run_forward each get their UP");
  std::cout<<"PASS pre-Play PTT held into Play/one LEVEL scan/release: voice released once, run_forward once\n";
 }
 if(run("shift_gesture")){
  Sim s;s.binds={{87,SHIFT,"start_gesture",false},{87,0,"voice_follow_key",true}};s.clean=true;
  s.nativeDown(87,0);s.scanKeyboard(87,0,true,false,true);
  s.scanKeyboard(87,SHIFT,false,false,true);s.scanKeyboard(87,SHIFT,false,false,true);
  require(count(s.log,"L:start_gesture")==0,"denied LEVEL action never starts");
  s.nativeUp(87,SHIFT);s.scanKeyboard(87,SHIFT,false,true,false);
  require(count(s.log,"U:start_gesture")==0,"Shift W start_gesture does not open chat on UP");
  require(s.log==Log{"D:voice_follow_key","U:voice_follow_key"},"PTT UP delivered with denied LEVEL");
  std::cout<<"PASS Shift+W start_gesture during Play: LEVEL denied, no chat on UP, PTT released once\n";
 }
 if(run("walk_escape")){
  auto go=[&](bool firstClean){Sim s;s.binds={{87,SHIFT,"run_forward",false},{87,0,"push_forward",false}};
   s.clean=firstClean;s.scanKeyboard(87,0,true,false,true);   // walk W in Play
   s.clean=false;                                              // Escape ends Play
   s.scanKeyboard(87,SHIFT,false,false,true);s.scanKeyboard(87,SHIFT,false,true,false);return s.log;};
  const Log stock=go(false);
  require(go(true)==stock&&stock==Log{"D:push_forward","L:run_forward","U:run_forward"},"post Play run_forward gets its UP");
  std::cout<<"PASS hold W in Play/Escape/Shift/release: stock sequence, run_forward UP delivered once\n";
 }
 if(run("stateless")){
  require(std::is_empty_v<G>,"stateless after lease");
  // Same probe after a busy lease and on a fresh gate: identical answers for every action/phase.
  const G used,fresh;
  for(auto a:{"voice_follow_key","start_gesture","pan_up","push_forward","move_forward_sitting"})
   for(auto ph:{G::Phase::Down,G::Phase::Level,G::Phase::Up}){(void)used.allow(a,ph,true,true);(void)used.allow(a,ph,true,false);}
  for(auto a:{"voice_follow_key","start_gesture","pan_up","push_forward","move_forward_sitting","director_hot_gaze_1"})
   for(auto ph:{G::Phase::Down,G::Phase::Level,G::Phase::Up})for(bool cam:{false,true})
    require(used.allow(a,ph,false,cam)==fresh.allow(a,ph,false,cam)&&used.allow(a,ph,false,cam),"stateless after lease");
  std::cout<<"PASS gate is empty: nothing survives the lease, probes after a busy lease equal a fresh gate\n";
 }
 if(run("seated")){
  Sim s;s.binds={{87,0,"move_forward_sitting",false}};s.clean=true;s.agentBranch=true;
  s.scanKeyboard(87,0,true,false,true);s.scanKeyboard(87,0,false,true,false);
  require(s.log==Log{"D:agent_seated_control","U:move_forward_sitting"},"seated agent control starts and releases");
  s.log.clear();s.agentBranch=false; // camera branch: zoom only
  s.scanKeyboard(87,0,true,false,true);s.scanKeyboard(87,0,false,true,false);
  require(s.log.empty(),"camera sitting UP never moves camera");
  require(G::seatedMoveAgent(true,false,false,false)&&G::seatedMoveAgent(false,true,false,false)&&G::seatedMoveAgent(false,false,true,false)&&!G::seatedMoveAgent(false,false,true,true),"seated branch classification");
  std::cout<<"PASS seated movement: agent branch walks and releases; camera branch dropped incl. its UP\n";
 }
 if(run("gesture")){
  // Same physical key = global PTT + gesture. Gestures keep no record: stock window UP.
  Sim s;s.binds={{65,0,"voice_follow_key",true}};s.clean=true;
  s.nativeDown(65,0);s.gestureDown();s.nativeUp(65,0);s.gestureUp();
  require(s.log==Log{"D:voice_follow_key","D:gesture","U:voice_follow_key","U:gesture"},"mixed PTT gesture releases once each");
  std::cout<<"PASS mixed PTT/gesture: each released exactly once through its own stock route\n";
 }
 if(run("keys")){
  const G g;
  require(g.allow("pov_eye_blink",G::Phase::Down,true),"F9 allowed");
  require(g.allow("director_hot_gaze_1",G::Phase::Down,true),"CtrlAlt1 allowed");
  require(g.allow("keyframe_play_stop",G::Phase::Down,true),"CtrlShiftP allowed");
  require(g.allow("push_forward",G::Phase::Down,true),"standing default W walking");
  require(!g.allow("move_forward",G::Phase::Down,true,true),"camera zoom denied");
  require(G::phaseOf(true,false,false)==G::Phase::Down&&G::phaseOf(true,true,false)==G::Phase::Down&&G::phaseOf(true,false,true)==G::Phase::Level&&G::phaseOf(false,true,false)==G::Phase::Up&&G::phaseOf(false,false,true)==G::Phase::Level,"phase mirrors stock branch order");
  ALKeyframeMouseGate m;
  require(m.mouseDown(1,true)&&m.mouseUp(1),"double click canonical owner");
  require(!m.mouseUp(2),"pre Play mouse UP");
  require(!m.mouseDown(3,false),"feature off mouse stock");
  std::cout<<"PASS input F9/gaze/play-stop/walking/zoom denied/phase order/mouse ownership\n";
 }
 if(run("particles")){
  using B=ALCleanParticleBatches;
  struct Group {bool effectFace=true,scriptFace=true,drawEffect=true,drawScript=true;};
  for(bool freeze:{false,true})for(bool hideSetting:{false,true}){
   std::array<Group,1> groups{};
   ALKeyframePlayer::clean(true);
   auto transition=[&](bool clean){
    B::transition(groups,clean,[](const auto&){return true;},
      [](const auto& group){return reinterpret_cast<std::uintptr_t>(&group);},
      [&](const auto& value){auto& group=const_cast<Group&>(value);
        // Synchronous face preparation, independent of simulator freeze/hide setting.
        group.effectFace=B::visible(true,clean);group.scriptFace=B::visible(false,clean);},
      [&](std::uintptr_t address){auto& group=*reinterpret_cast<Group*>(address);
        group.drawEffect=group.effectFace;group.drawScript=group.scriptFace;
        B::built(address,group.drawEffect);});
   };
   // Ordinary frozen/throttled update would leave the prebuilt map unchanged.
   if(!freeze && hideSetting)groups[0].effectFace=false;
   transition(true);
   auto address=reinterpret_cast<std::uintptr_t>(&groups[0]);
   require(!B::forbidden(address),"frozen prebuilt consumption");
   require(!groups[0].drawEffect&&groups[0].drawScript,"mixed scripted content preserved");
   ALKeyframePlayer::clean(false);transition(false);
   require(groups[0].drawEffect&&groups[0].drawScript,"particle release restores geometry");
  }
  B::reset();
  for(unsigned i=1;i<B::Capacity+100;++i)B::built(std::uintptr_t(i)*16,false);
  require(!B::complete(),"bounded batch coverage exhaustion");
  B::reset();
  std::cout<<"PASS particles prebuilt/frozen/both hide settings/mixed script/forced rebuild/release/bounded coverage\n";
 }
 if(run("telemetry")){
  using T=ALCleanTelemetry;ALKeyframePlayer::clean(true);T::begin();
  for(int i=0;i<T::Count;++i)if(i!=T::HUDText)T::guard((T::Site)i,true);
  for(int i=0;i<30;++i)T::frame(.016);++T::releases;
  require(std::string(T::verdict())=="PASS","telemetry ancestor coverage");
  T::submit(T::UI);require(std::string(T::verdict())=="FAIL","telemetry detects leak");
  T::begin();T::frame(.016);++T::releases;
  require(std::string(T::verdict())=="INCONCLUSIVE","telemetry missing root inconclusive");
  ALKeyframePlayer::clean(false);T::guard(T::UI,true);T::submit(T::UI);
  require(T::sites[T::UI].evaluated==0 && T::sites[T::UI].forbidden==0,"feature off telemetry inert");
  std::cout<<"PASS telemetry PASS/leak FAIL/missing root INCONCLUSIVE/off inert\n";
 }
}
"""
def source_rules(text,player=None,window=None):
 errors=[]
 from keyframe_runtime_check import body,code
 ctext=code(text)
 stale=('sCleanReleased','releaseBindings','releaseKey','flushCleanKeys','skipStockUp','levelStarted','endStockUp','sCleanKeys.started','.flush(')
 global_up=ctext[ctext.index('bool LLViewerInput::handleGlobalBindsKeyUp'):ctext.index('bool LLViewerInput::handleGlobalBindsMouse')]
 if not (not any(s in ctext for s in stale) and 'sClean' not in global_up and 'return gViewerWindow->handleKeyUp(translated_key, translated_mask);' in ctext):errors.append('I1 UP routes carry no clean state')
 scan=body(ctext,'bool LLViewerInput::scanKey(const std::vector<LLKeyboardBinding> &binding,')
 allow='sCleanKeys.allow(action,ALKeyframeInputGate::phaseOf(key_down,key_up,repeat),clean,move)'
 at=scan.find(allow);dn=scan.find('res = (*function)(KEYSTATE_DOWN);');upc=scan.find('res = binding[i].mFunction( KEYSTATE_UP );')
 if not (at>=0 and (dn<0 or at<dn) and (upc<0 or at<upc) and scan.find('res = (*function)(KEYSTATE_LEVEL);')>at):errors.append('I2 every phase passes the stateless gate before dispatch')
 if not ('ALKeyframeInputGate::seatedMoveAgent(gAgent.forwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())' in text and 'ALKeyframeInputGate::seatedMoveAgent(gAgent.backwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())' in text and 'if (hot_gaze && ALKeyframeInputGate::ownsHotGaze(clean))' in text and '!clean && binding[i].mFunctionName' not in text and 'if (ALKeyframeInputGate::pairedUp(clean, key_down, key_up, repeat)) { res = binding[i].mFunction(KEYSTATE_UP); sHotGazeKeys.erase(key); }' in text and 'const auto button=cleanButton(clicktype);' in text and text.index('sCleanMouse.mouseDown(button,ALCleanFrame::active())')<text.index('gViewerWindow->handleAnyMouseClick')):errors.append('I3 branch and mouse ownership')
 up=scan[scan.index('else if (key_up)'):scan.index('else if (key_level)')]
 if not ('res = binding[i].mFunction( KEYSTATE_UP );' in up and 'return' not in up and 'sClean' not in up):errors.append('I5 UP reaches the stock binding function')
 player_text=player if player is not None else (NV/'alkeyframeplayer.cpp').read_text(encoding='utf-8')
 rel=body(code(player_text),'if(fx.release)') if 'if(fx.release)' in player_text else ''
 shut=body(code(player_text),'void ALKeyframePlayer::shutdown()')
 if not ('ALCleanFrame::setActive(false)' in rel and 'flushClean' not in player_text and 'ALCleanFrame::setActive(false)' in shut):errors.append('I7 lease end and shutdown release the lease with no input flush')
 w=window if window is not None else (NV/'llviewerwindow.cpp').read_text(encoding='utf-8')
 import re
 if re.search(r'std::(?:set|map|array|vector)<[^;]*>\s+s(?:Clean|Skip)\w*',ctext) or re.search(r'std::(?:set|map|array|vector)<[^;]*>\s+s(?:Clean|Skip)\w*',code(w)) or 'const ALKeyframeInputGate sCleanKeys;' not in text:errors.append('I8 no namespace-scope key state')
 return errors

def floater_rules(inp,gate):
 # G3: during Play the clean key branch returns false into the ordinary bindings. No bound
 # action may open or toggle a floater/chat unless the gate denies it in clean.
 import re
 bs=chr(92)
 denied=set(re.findall(r'"([a-z_]+)"',gate[gate.index('for (auto name'):gate.index('for (auto prefix')]))
 tokens=('LLFloaterReg::','startChat','toggleInstance','showInstance','gMenuBarView','LLNotificationsUtil','LLNotifications','LLFloaterIMNearbyChat')
 errors=[]
 for m in re.finditer(r'REGISTER_KEYBOARD_ACTION\("([^"]+)",'+bs+'s*('+bs+'w+)'+bs+')',inp):
  name,fn=m.groups();d=re.search(chr(10)+'bool '+fn+bs+'s*'+bs+'('+bs+'s*EKeystate'+bs+'s+'+bs+'w+'+bs+'s*'+bs+')',inp)
  if not d:
   if name not in denied:errors.append('I6 unlocated action '+name)
   continue
  rest=inp[d.end():];ends=[rest.find(x) for x in (chr(10)+'}'+chr(10),chr(10)+'bool ') if rest.find(x)>=0]
  if any(t in rest[:min(ends or [len(rest)])] for t in tokens) and name not in denied:errors.append('I6 floater action '+name+' not denied in clean')
 return errors

def gesture_rules(inp,window):
 from keyframe_runtime_check import body,code
 inp=code(inp);window=code(window)
 up=body(inp,'bool LLViewerInput::handleKeyUp(')
 release=body(window,'bool LLViewerWindow::handleKeyUp(')
 down=body(window,'bool LLViewerWindow::handleKey(KEY key, MASK mask)')
 return [] if ('sCleanReleased' not in up and 'return gViewerWindow->handleKeyUp(translated_key, translated_mask);' in up and
  'sCleanGestures' not in window and 'sClean' not in release and
  'if(LLGestureMgr::instance().triggerGesture(key,mask)) return true;' in down and 'triggerGestureRelease' in release) else ['I4 gesture release is the stock UP route']

def check():
 d=check_directory('keyframe_input'); originals={f:(NV/f).read_text(encoding='utf-8') for f in ('alkeyframeinputgate.h','alcleantelemetry.h','alkeyframeparticles.h')}
 def build(files,name,run=True):
  for f,s in files.items():(d/f).write_bytes(s.encode())
  return compile_run(d,FIXTURE,name,flags=(f'/I"{d}"',f'/I"{NV}"','/O2'),run=run)
 clean=build(originals,'clean');print(clean.stdout,end='');assert clean.returncode==0,clean.stdout+clean.stderr
 G='alkeyframeinputgate.h'
 ALLOW='        if (!clean || !denied(action, cameraOnlyMove)) return true;'
 UPFX='        return action == "start_chat" || action == "start_gesture" || action.ends_with("_sitting");'
 MEMBER='    // no data members: stateless by construction'
 PHASE='{ return (down && !repeat) ? Phase::Down : up ? Phase::Up : Phase::Level; }'
 # Each negative plants one semantic bug; edits are (file,before,after) and each `before` must occur once.
 sticky=[(G,MEMBER,'    mutable std::set<std::string> mSkip;'),(G,ALLOW,'        if (phase == Phase::Up) { if (!clean && mSkip.erase(std::string(action))) return false; if (clean) mSkip.insert(std::string(action)); }\n'+ALLOW)]
 armed=[(G,MEMBER,'    mutable bool mArm = false;'),(G,ALLOW,'        if (clean && phase == Phase::Down) mArm = true;\n        if (phase == Phase::Up && mArm) { mArm = false; return false; }\n'+ALLOW)]
 drop_up=[(G,ALLOW,'        if (clean && phase == Phase::Up) return false;\n'+ALLOW)]
 opener_up=[(G,UPFX,'        return false;')]
 level_starts=[(G,'return phase == Phase::Up && !upHasSideEffect(action);','return phase != Phase::Down && (phase != Phase::Up || !upHasSideEffect(action));')]
 negatives=[
 ('skip_frozen_rebuild','particles','frozen prebuilt consumption',[('alkeyframeparticles.h','for(auto group:groups) rebuild(group);','for(auto group:groups) {}')]),
 ('allow_effect_geometry','particles','frozen prebuilt consumption',[('alkeyframeparticles.h','return !clean || !effect;','return true;')]),
 ('telemetry_ignore_submission','telemetry','telemetry detects leak',[('alcleantelemetry.h','if(c.forbidden) return "FAIL";','if(false) return "FAIL";')]),
 # (a) swallowing a non-opener UP
 ('swallow_nonopener_up','up_rules','UP always delivered',[(G,'return phase == Phase::Up && !upHasSideEffect(action);','return false;')]),
 # (b) a denied opener UP passes during clean
 ('opener_up_passes','up_rules','no UI from UP during clean',opener_up),
 ('opener_up_passes_unsampled','shift_unsampled','unsampled Shift transition opens no chat',opener_up),
 ('opener_up_passes_shift_gesture','shift_gesture','Shift W start_gesture does not open chat on UP',opener_up),
 ('sitting_camera_up_orbits','seated','camera sitting UP never moves camera',[(G,' || action.ends_with("_sitting");',';')]),
 ('denied_level_starts','shift_gesture','denied LEVEL action never starts',level_starts),
 # (c) reintroduced state surviving lease end
 ('member_state_added','stateless','stateless after lease',[(G,MEMBER,'    mutable std::set<int> mSkip;')]),
 ('skip_set_survives_lease','focus_loss','fresh PTT after Play gets its UP',sticky),
 ('skip_set_survives_lease_stateless','stateless','stateless after lease',sticky),
 ('armed_record_survives_lease','release_after_play','release after Play ends delivers exactly one UP',armed),
 ('armed_record_walk_escape','walk_escape','post Play run_forward gets its UP',armed),
 ('clean_drops_all_up_hotgaze','hotgaze_preplay','hot gaze UP routes equal stock',drop_up),
 ('clean_drops_all_up_ptt','ptt_preplay','pre Play PTT and run_forward each get their UP',drop_up),
 ('up_before_down','press_release','denied press+release between scans stays dropped',[(G,PHASE,'{ return up ? Phase::Up : (down && !repeat) ? Phase::Down : Phase::Level; }')]),
 ('hotgaze_owner_excluded_in_clean','hotgaze_play_modifier','hot gaze started in Play released after modifier change',[(G,'static bool ownsHotGaze(bool clean) { (void)clean; return true; }','static bool ownsHotGaze(bool clean) { return !clean; }')]),
 ('hotgaze_owner_excluded_focus','hotgaze_play_focus','hot gaze started in Play released after focus loss',[(G,'static bool ownsHotGaze(bool clean) { (void)clean; return true; }','static bool ownsHotGaze(bool clean) { return !clean; }')]),
 ('paired_up_omitted','press_release','press+release in Play leaves no running state',[(G,'{ return clean && down && !repeat && up; }','{ return false; }')]),
 ('feature_off_gated','feature_off','feature off equals stock',[(G,ALLOW,'        if (!denied(action, cameraOnlyMove)) return true;')]),
 ]
 for name,selected,assertion,edits in negatives:
  planted=dict(originals)
  for file,before,after in edits:
   assert planted[file].count(before)==1,(name,before);planted[file]=planted[file].replace(before,after)
  exe=build(planted,name,False);result=subprocess.run([str(exe),selected],capture_output=True,text=True)
  assert result.returncode==1 and result.stdout.count('FAIL ')==1 and 'FAIL '+assertion+'\n' in result.stdout,(name,result.stdout,result.stderr)
  print('PASS negative [exec] '+name+': '+result.stdout.strip())
 planted_fixture=FIXTURE.replace('constexpr bool RETAIN_PAIRED=false;','constexpr bool RETAIN_PAIRED=true;',1);assert planted_fixture!=FIXTURE
 for f,s2 in originals.items():(d/f).write_bytes(s2.encode())
 exe=compile_run(d,planted_fixture,'retain_paired_owner',flags=(f'/I"{d}"',f'/I"{NV}"','/O2'),run=False);result=subprocess.run([str(exe),'hotgaze_play_paired'],capture_output=True,text=True)
 assert result.returncode==1 and result.stdout.count('FAIL ')==1 and 'FAIL hot gaze combined edge releases exactly once\n' in result.stdout,(result.stdout,result.stderr)
 print('PASS negative [exec] retain_paired_owner: '+result.stdout.strip())
 clean=build(originals,'clean_after_negatives');print(clean.stdout,end='');assert clean.returncode==0
 text=(NV/'llviewerinput.cpp').read_text(encoding='utf-8');player=(NV/'alkeyframeplayer.cpp').read_text(encoding='utf-8');window=(NV/'llviewerwindow.cpp').read_text(encoding='utf-8')
 assert not source_rules(text,player,window),source_rules(text,player,window)
 allow_call='sCleanKeys.allow(action,ALKeyframeInputGate::phaseOf(key_down,key_up,repeat),clean,move)'
 src=[('handleGlobalBindsKeyUp_state',text.replace('bool LLViewerInput::handleGlobalBindsKeyUp(KEY key, MASK mask)\n{','bool LLViewerInput::handleGlobalBindsKeyUp(KEY key, MASK mask)\n{\n    sCleanReleased[key]=true;',1),player,window,'I1 UP routes carry no clean state'),
  ('handleKeyUp_swallow',text.replace('return gViewerWindow->handleKeyUp(translated_key, translated_mask);','return true;',1),player,window,'I1 UP routes carry no clean state'),
  ('phase_forced_down',text.replace(allow_call,'sCleanKeys.allow(action,ALKeyframeInputGate::Phase::Down,clean,move)',1),player,window,'I2 every phase passes the stateless gate before dispatch'),
  ('seated_branch_removed',text.replace('ALKeyframeInputGate::seatedMoveAgent(gAgent.forwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())','false',1),player,window,'I3 branch and mouse ownership'),
  ('up_swallowed_in_scan',text.replace('res = binding[i].mFunction( KEYSTATE_UP );','res = true;',1),player,window,'I5 UP reaches the stock binding function'),
  ('lease_end_flush',text,player.replace('ALCleanFrame::setActive(false); ALKeyframeViewer::cleanParticleTransition(); p.broadcast(true);','gViewerInput.flushCleanKeys(); ALCleanFrame::setActive(false); ALKeyframeViewer::cleanParticleTransition(); p.broadcast(true);',1),window,'I7 lease end and shutdown release the lease with no input flush'),
  ('shutdown_keeps_lease',text,player.replace('ALCleanFrame::setActive(false); // shutdown: no UI or renderer calls','',1),window,'I7 lease end and shutdown release the lease with no input flush'),
  ('hotgaze_owner_excluded_src',text.replace('if (hot_gaze && ALKeyframeInputGate::ownsHotGaze(clean))','if (!clean && binding[i].mFunctionName.compare(0, 18, "director_hot_gaze_") == 0)',1),player,window,'I3 branch and mouse ownership'),
  ('paired_up_owner_retained_src',text.replace(' sHotGazeKeys.erase(key); }',' }',1),player,window,'I3 branch and mouse ownership'),
  ('paired_up_removed_src',text.replace('if (ALKeyframeInputGate::pairedUp(clean, key_down, key_up, repeat)) { res = binding[i].mFunction(KEYSTATE_UP); sHotGazeKeys.erase(key); }','',1),player,window,'I3 branch and mouse ownership'),
  ('namespace_skip_set',text.replace('const ALKeyframeInputGate sCleanKeys;','const ALKeyframeInputGate sCleanKeys;\nstd::set<KEY> sSkipUp;',1),player,window,'I8 no namespace-scope key state')]
 for name,t2,p2,w2,assertion in src:
  assert (t2,p2,w2)!=(text,player,window),name
  assert assertion in source_rules(t2,p2,w2),(name,source_rules(t2,p2,w2));print('PASS negative [src] '+name+': FAIL '+assertion)
 gate=originals[G];assert not floater_rules(text,gate),floater_rules(text,gate)
 for name,i,g in [('floater_action_undenied',text,gate.replace('"start_chat", ','',1)),('new_floater_action',text.replace('bool toggle_enable_media(EKeystate s)'+chr(10)+'{','bool toggle_enable_media(EKeystate s)'+chr(10)+'{LLFloaterReg::showInstance("inventory");',1),gate)]:
  want='I6 floater action '+('start_chat' if name=='floater_action_undenied' else 'toggle_enable_media')+' not denied in clean';assert floater_rules(i,g)==[want],(name,floater_rules(i,g))
  print('PASS negative [src] '+name+': FAIL I6')
 assert not gesture_rules(text,window)
 for name,i,w in [('window_gesture_record',text,window.replace('bool LLViewerWindow::handleKeyUp(KEY key, MASK mask)\n{','bool LLViewerWindow::handleKeyUp(KEY key, MASK mask)\n{\n    if (sCleanGestures.releaseKey(key)) return true;',1)),('gesture_down_not_dispatched',text,window.replace('if(LLGestureMgr::instance().triggerGesture(key,mask)) return true;','if(false) return true;',1))]:
  assert w!=window and gesture_rules(i,w)==['I4 gesture release is the stock UP route'],name
  print('PASS negative [src] '+name+': FAIL I4 gesture release is the stock UP route')
 print('PASS clean input/telemetry after %d semantic and %d source negatives'%(len(negatives),len(src)+4))
if __name__=='__main__':check()
