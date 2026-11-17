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
class ALKeyframePlayer {public:static void clean(bool v){ALCleanFrame::setActive(v);}};
void require(bool v,const char* name){if(!v){std::cout<<"FAIL "<<name<<"\n";std::exit(1);}}
int main(int argc,char** argv){
 std::string selected=argc>1?argv[1]:"all";
 auto run=[&](const char* n){return selected=="all"||selected==n;};
 using G=ALKeyframeInputGate;
 if(run("modifiers")){
  G g;int runDown=0,runUp=0;
  require(!g.allow(87,"move_forward",G::Phase::Down,true,true),"Alt W camera action denied");
  // Alt is released, then Play ends, while the same physical W remains down.
  for(bool clean:{true,false})
   if(g.allow(87,"run_forward",G::Phase::Level,clean))++runDown;
  if(!g.releaseKey(87))++runUp;
  require(runDown==0&&runUp==0,"denied press cannot start modifier action");
  require(g.allow(87,"run_forward",G::Phase::Down,false),"fresh W allowed after release");
  std::cout<<"PASS modifier transition denied Alt W/Level run/Play end/physical UP/fresh W\n";
 }
 if(run("level")){
  // Global voice_follow_key on W (DOWN recorded), then Shift held while W held: the
  // ordinary Shift+W run_forward starts on LEVEL with no recorded DOWN.
  G g;int pttUp=0,runUp=0,runLevel=0;
  require(g.allow(87,"voice_follow_key",G::Phase::Down,true),"global PTT allowed");
  g.started(87,"voice_follow_key",[&]{++pttUp;},true);
  for(int frame=0;frame<3;++frame)
   if(g.allow(87,"run_forward",G::Phase::Level,true)){++runLevel;g.levelStarted(87,"run_forward",[&]{return G::Release([&]{++runUp;});},true);}
  require(runLevel==3,"level action still executes");
  require(g.releaseKey(87),"mixed key UP owned");
  require(pttUp==1&&runUp==1,"mixed global modifier LEVEL action released once");
  require(!g.releaseKey(87),"mixed level record cleared");
  g.levelStarted(88,"run_forward",[&]{return G::Release([&]{++runUp;});},false);
  require(!g.releaseKey(88),"feature off level not recorded");
  std::cout<<"PASS mixed global PTT / modifier LEVEL run_forward released exactly once\n";
 }
 if(run("ptt_preplay")){
  // PTT (global) and run_forward (ordinary) share W. PTT DOWN happened BEFORE Play, so it was
  // never recorded; one LEVEL scan during Play records run_forward; then W is released.
  G g;int pttUp=0,runUp=0;
  g.allow(87,"run_forward",G::Phase::Level,true);
  g.levelStarted(87,"run_forward",[&]{return G::Release([&]{++runUp;});},true);
  const bool swallowed=g.releaseBindings(87);
  for(auto name:{"voice_follow_key","run_forward"})
   if(!swallowed&&!g.skipStockUp(87,name)){ if(std::string(name)=="voice_follow_key")++pttUp; else ++runUp; }
  g.endStockUp(87);
  require(pttUp==1,"pre Play PTT stock UP delivered once");
  require(runUp==1,"recorded run_forward UP delivered once");
  std::cout<<"PASS pre-Play PTT held into Play/one LEVEL scan/release: voice released once, run once\n";
 }
 if(run("hotgaze_preplay")){
  // Pre-Play hot gaze owns its UP through sHotGazeKeys; a LEVEL scan during Play must not
  // add a second owner.
  G g;int hotUp=0;
  g.allow(49,"director_hot_gaze_1",G::Phase::Level,true);
  g.levelStarted(49,"director_hot_gaze_1",[&]{return G::Release([&]{++hotUp;});},true,true);
  require(!g.releaseKey(49),"hot gaze not recorded twice");
  ++hotUp; // the stock sHotGazeKeys owner delivers its one UP
  require(hotUp==1,"pre Play hot gaze one UP");
  std::cout<<"PASS pre-Play hot gaze held through Play: exactly one UP\n";
 }
 if(run("shift_gesture")){
  G g;int pttUp=0,chat=0;
  g.started(87,"voice_follow_key",[&]{++pttUp;},true); // W down in Play: global PTT
  require(!g.allow(87,"start_gesture",G::Phase::Level,true),"denied LEVEL action never starts");
  require(!g.allow(87,"start_gesture",G::Phase::Level,true),"denied LEVEL action never starts");
  const bool swallowed=g.releaseBindings(87);
  if(!swallowed&&!g.skipStockUp(87,"start_gesture"))++chat;
  g.endStockUp(87);
  require(chat==0,"Shift W start_gesture does not open chat on UP");
  require(pttUp==1,"PTT UP delivered with denied LEVEL");
  require(g.allow(87,"start_gesture",G::Phase::Level,false),"denied LEVEL only blocked in clean");
  std::cout<<"PASS Shift+W start_gesture during Play: LEVEL denied, no chat on UP, PTT released once\n";
 }
 if(run("walk_escape")){
  G g;int pushUp=0,runUp=0;
  require(g.allow(87,"push_forward",G::Phase::Down,true),"walk W allowed in Play");
  g.started(87,"push_forward",[&]{++pushUp;},true);
  g.flush(); // Escape ends the lease
  require(pushUp==1,"flush delivers outstanding recorded UP");
  g.flush();
  // Play over: Shift pressed (run_forward LEVEL, not clean), then W released.
  g.levelStarted(87,"run_forward",[&]{return G::Release([&]{++runUp;});},false);
  const bool swallowed=g.releaseBindings(87);
  if(!swallowed&&!g.skipStockUp(87,"run_forward"))++runUp;
  g.endStockUp(87);
  require(pushUp==1,"flushed UP delivered exactly once");
  require(runUp==1,"post Play run_forward gets its UP");
  std::cout<<"PASS hold W in Play/Escape flush/Shift/release: push once, run_forward UP, nothing stuck\n";
 }
 if(run("gesture")){
  G bindings,gestures;int pttUp=0,gestureUp=0;unsigned releaseMask=0;
  bindings.started(65,"voice_follow_key",[&]{++pttUp;},true);
  const unsigned downMask=4;
  gestures.started(65,std::to_string(downMask),[&,downMask]{++gestureUp;releaseMask=downMask;},true);
  // Global UP consumes binding ownership; window UP still runs after modifiers/Play change.
  require(bindings.releaseKey(65),"mixed binding UP owned");
  require(gestures.releaseKey(65),"mixed gesture UP owned");
  require(!bindings.releaseKey(65)&&!gestures.releaseKey(65)&&pttUp==1&&gestureUp==1&&releaseMask==downMask,"mixed PTT gesture releases once with DOWN identity");
  std::cout<<"PASS mixed PTT/gesture independent UP ownership/retained DOWN mask/exactly once\n";
 }
 if(run("keys")){
  G g;int voiceDown=0,voiceUp=0,chat=0;
  require(g.allow(9,"pov_eye_blink",G::Phase::Down,true),"F9 allowed");
  require(g.allow(1,"director_hot_gaze_1",G::Phase::Down,true),"CtrlAlt1 allowed");
  require(g.allow(80,"keyframe_play_stop",G::Phase::Down,true),"CtrlShiftP allowed");
  if(g.allow(65,"voice_follow_key",G::Phase::Down,true)){++voiceDown;g.started(65,"voice_follow_key",[&]{++voiceUp;},true);}
  if(g.allow(65,"start_chat",G::Phase::Down,true))++chat;
  g.started(65,"voice_follow_key",[&]{++voiceUp;},true); // repeat is deduplicated
  require(!g.allow(65,"start_chat",G::Phase::Level,false),"held denied key fires after Play");
  // Modifiers released and mode changed before physical UP. Stock UP must be suppressed.
  if(!g.releaseKey(65)) ++voiceUp;
  require(voiceDown==1 && voiceUp==1 && chat==0,"allowed action UP delivered exactly once");
  require(!g.releaseKey(65),"release record cleared");
  require(!g.releaseKey(66),"pre Play PTT stock UP delivered");
  require(g.allow(65,"start_chat",G::Phase::Down,false),"new press after UP allowed");
  require(!g.allow(67,"move_forward",G::Phase::Down,true,true),"camera zoom denied");
  require(g.allow(87,"push_forward",G::Phase::Down,true),"standing default W walking");
  int agentUp=0;
  require(G::seatedMoveAgent(true,false,false,false),"seated grabbed movement");
  require(G::seatedMoveAgent(false,true,false,false),"sit camera movement");
  require(G::seatedMoveAgent(false,false,true,false),"temporary run movement");
  require(!G::seatedMoveAgent(false,false,true,true),"always run camera branch");
  g.started(87,"move_forward_sitting",[&]{++agentUp;},true);
  // Escape and stand/control ungrab do not change the stored branch release.
  require(g.releaseKey(87) && agentUp==1,"seated release across Escape");
  require(g.mouseDown(1,true) && g.mouseUp(1),"double click canonical owner");
  require(!g.mouseUp(2),"pre Play mouse UP");
  std::cout<<"PASS input F9/gaze/play-stop/mixed PTT/chat/repeat/modifiers/mode/exact UP/walking/seated/mouse\n";
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
def source_rules(text,player=None):
 errors=[]
 global_up=text[text.index('bool LLViewerInput::handleGlobalBindsKeyUp'):text.index('bool LLViewerInput::handleGlobalBindsMouse')]
 if not ('sCleanKeys.releaseBindings(key)' in global_up and global_up.index('sCleanKeys.releaseBindings(key)')<global_up.index('if (mGlobalBindsSuspended)') and 'return gViewerWindow->handleKeyUp(translated_key, translated_mask);' in text and 'if(key_up && (sCleanReleased[key] || sCleanKeys.releaseBindings(key)))' in text):errors.append('I1 physical release routes')
 if not ('res = (*function)(KEYSTATE_DOWN);' in text and 'if(res) sCleanKeys.started' in text and text.index('res = (*function)(KEYSTATE_DOWN);')<text.index('if(res) sCleanKeys.started')):errors.append('I2 record actual DOWN')
 if not ('ALKeyframeInputGate::seatedMoveAgent(gAgent.forwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())' in text and 'ALKeyframeInputGate::seatedMoveAgent(gAgent.backwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())' in text and 'if (!clean && binding[i].mFunctionName' in text and 'const auto button=cleanButton(clicktype);' in text and text.index('sCleanMouse.mouseDown(button,ALCleanFrame::active())')<text.index('gViewerWindow->handleAnyMouseClick')):errors.append('I3 branch and mouse ownership')
 if not ('res = (*function)(KEYSTATE_LEVEL);' in text and 'sCleanKeys.levelStarted(key,name,' in text and text.index('res = (*function)(KEYSTATE_LEVEL);')<text.index('sCleanKeys.levelStarted(key,name,') and 'KEYSTATE_UP); });},clean,' in text):errors.append('I5 LEVEL action takes release ownership')
 player_text=player if player is not None else (NV/'alkeyframeplayer.cpp').read_text(encoding='utf-8')
 rel=player_text[player_text.index('if(fx.release)'):]
 if not ('gViewerInput.flushCleanKeys();' in rel and 'flushCleanGestures();' in rel and rel.index('flushCleanKeys')<rel.index('ALCleanFrame::setActive(false)')):errors.append('I7 lease end flushes recorded input')
 if not ('if(sCleanKeys.skipStockUp(key,name)) return true;' in text and text.index('if(sCleanKeys.skipStockUp(key,name)) return true;')<text.index('res = binding[i].mFunction( KEYSTATE_UP );') and 'hot_gaze && sHotGazeKeys.count(key)!=0' in text):errors.append('I8 stock UP skips only recorded pairs and hot gaze owner')
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
  release.index('sCleanGestures.releaseKey(key)')<release.index('LLSetKeyBindDialog::recordKey') and
  'sCleanGestures.started(key,std::to_string(mask),[key,mask]()' in down and
  '{LLGestureMgr::instance().triggerGestureRelease(key,mask);},true);' in down and
  'if(LLGestureMgr::instance().triggerGesture(key,mask))' in down) else ['I4 gesture release independent of binding UP']

def check():
 d=check_directory('keyframe_input'); originals={f:(NV/f).read_text(encoding='utf-8') for f in ('alkeyframeinputgate.h','alcleantelemetry.h','alkeyframeparticles.h')}
 def build(files,name,run=True):
  for f,s in files.items():(d/f).write_bytes(s.encode())
  return compile_run(d,FIXTURE,name,flags=(f'/I"{d}"',f'/I"{NV}"','/O2'),run=run)
 clean=build(originals,'clean');print(clean.stdout,end='');assert clean.returncode==0,clean.stderr
 negatives=[
 ('skip_frozen_rebuild','particles','frozen prebuilt consumption','alkeyframeparticles.h','for(auto group:groups) rebuild(group);','for(auto group:groups) {}'),
 ('allow_effect_geometry','particles','frozen prebuilt consumption','alkeyframeparticles.h','return !clean || !effect;','return true;'),
 ('forget_swallowed','keys','held denied key fires after Play','alkeyframeinputgate.h','if (mSwallowed.count(key))','if (false)'),
 ('modifier_starts_unowned','modifiers','denied press cannot start modifier action','alkeyframeinputgate.h','if (mSwallowed.count(key))','if (mSwallowed.count(key) && denied(action,cameraOnlyMove))'),
 ('level_action_unowned','level','mixed global modifier LEVEL action released once','alkeyframeinputgate.h','mStarted[key].push_back({std::string(action), makeUp()});','(void)makeUp;'),
 ('suppress_whole_key','ptt_preplay','pre Play PTT stock UP delivered once','alkeyframeinputgate.h','deliver(key, true);\n        return mSwallowed.erase(key) != 0;','bool owned = deliver(key, true);\n        return mSwallowed.erase(key) != 0 || owned;'),
 ('hot_gaze_duplicated','hotgaze_preplay','hot gaze not recorded twice','alkeyframeinputgate.h','if (!clean || stockOwned) return;','if (!clean) return;'),
 ('denied_level_starts','shift_gesture','denied LEVEL action never starts','alkeyframeinputgate.h','            if (!mSkip[key].count(action)) mSkip[key].emplace(action);\n            return false;','            return true;'),
 ('denied_level_up_not_skipped','shift_gesture','Shift W start_gesture does not open chat on UP','alkeyframeinputgate.h','if (!mSkip[key].count(action)) mSkip[key].emplace(action);','(void)0;'),
 ('flush_noop','walk_escape','flush delivers outstanding recorded UP','alkeyframeinputgate.h','for (auto& action : entry.second) action.up();','(void)entry;'),
 ('flush_keeps_records','walk_escape','flushed UP delivered exactly once','alkeyframeinputgate.h','mStarted.clear();\n        for (auto& entry : all)','mStarted = all;\n        for (auto& entry : all)'),
 ('gesture_released_twice','gesture','mixed PTT gesture releases once with DOWN identity','alkeyframeinputgate.h','for (auto& action : actions) action.up();','for (auto& action : actions) {action.up();action.up();}'),
 ('stock_up_also','keys','allowed action UP delivered exactly once','alkeyframeinputgate.h','mSwallowed.erase(key) != 0 || owned','(mSwallowed.erase(key), false)'),
 ('release_by_current_mode','keys','allowed action UP delivered exactly once','alkeyframeinputgate.h','for (auto& action : actions) action.up();','for (auto& action : actions) {}'),
 ('telemetry_ignore_submission','telemetry','telemetry detects leak','alcleantelemetry.h','if(c.forbidden) return "FAIL";','if(false) return "FAIL";')]
 for name,selected,assertion,file,before,after in negatives:
  planted=dict(originals);assert planted[file].count(before)==1;planted[file]=planted[file].replace(before,after)
  exe=build(planted,name,False);result=subprocess.run([str(exe),selected],capture_output=True,text=True)
  assert result.returncode==1 and result.stdout.count('FAIL ')==1 and 'FAIL '+assertion+'\n' in result.stdout,(name,result.stdout,result.stderr)
  print('PASS negative [exec] '+name+': '+result.stdout.strip())
 clean=build(originals,'clean_after_negatives');print(clean.stdout,end='');assert clean.returncode==0
 text=(NV/'llviewerinput.cpp').read_text(encoding='utf-8');assert not source_rules(text),source_rules(text)
 for before,after,assertion in [
  ('return gViewerWindow->handleKeyUp(translated_key, translated_mask);','return true;','I1 physical release routes'),
  ('if(res) sCleanKeys.started','if(false) sCleanKeys.started','I2 record actual DOWN'),
  ('ALKeyframeInputGate::seatedMoveAgent(gAgent.forwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())','false','I3 branch and mouse ownership'),
  ('sCleanKeys.levelStarted(key,name,','(void)(key,name,','I5 LEVEL action takes release ownership')]:
  assert before in text;assert assertion in source_rules(text.replace(before,after,1));print('PASS negative [src]: FAIL '+assertion)
 for before,assertion in [('if(sCleanKeys.skipStockUp(key,name)) return true;','I8 stock UP skips only recorded pairs and hot gaze owner'),('hot_gaze && sHotGazeKeys.count(key)!=0','I8 stock UP skips only recorded pairs and hot gaze owner')]:
  assert before in text;assert assertion in source_rules(text.replace(before,'false' if 'hot_gaze' in before else '',1));print('PASS negative [src]: FAIL '+assertion)
 player=(NV/'alkeyframeplayer.cpp').read_text(encoding='utf-8')
 for planted in (player.replace('gViewerInput.flushCleanKeys(); ','',1),player.replace('flushCleanGestures();','',1)):
  assert planted!=player and 'I7 lease end flushes recorded input' in source_rules(text,planted);print('PASS negative [src]: FAIL I7 lease end flushes recorded input')

 gate=originals['alkeyframeinputgate.h'];assert not floater_rules(text,gate),floater_rules(text,gate)
 for name,i,g in [('floater_action_undenied',text,gate.replace('"start_chat", ','',1)),('new_floater_action',text.replace('bool toggle_enable_media(EKeystate s)'+chr(10)+'{','bool toggle_enable_media(EKeystate s)'+chr(10)+'{LLFloaterReg::showInstance("inventory");',1),gate)]:
  want='I6 floater action '+('start_chat' if name=='floater_action_undenied' else 'toggle_enable_media')+' not denied in clean';assert floater_rules(i,g)==[want],(name,floater_rules(i,g))
  print('PASS negative [src] '+name+': FAIL I6')
 window=(NV/'llviewerwindow.cpp').read_text(encoding='utf-8');assert not gesture_rules(text,window)
 for name,i,w in [('early_binding_return',text.replace('bool LLViewerInput::handleKeyUp(KEY translated_key, MASK translated_mask)\n{','bool LLViewerInput::handleKeyUp(KEY translated_key, MASK translated_mask)\n{\nif(sCleanReleased[translated_key]) return true;'),window),('lost_down_mask',text,window.replace('[key,mask]()','[key,mask=MASK_NONE]()'))]:
  assert gesture_rules(i,w)==['I4 gesture release independent of binding UP'],name
  print('PASS negative [src] '+name+': FAIL I4 gesture release independent of binding UP')
 print('PASS clean input/telemetry after 15 semantic and 12 source negatives')
if __name__=='__main__':check()
