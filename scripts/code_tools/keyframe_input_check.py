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
def source_rules(text):
 errors=[]
 global_up=text[text.index('bool LLViewerInput::handleGlobalBindsKeyUp'):text.index('bool LLViewerInput::handleGlobalBindsMouse')]
 if not ('sCleanKeys.releaseKey(key)' in global_up and global_up.index('sCleanKeys.releaseKey(key)')<global_up.index('if (mGlobalBindsSuspended)') and 'if(sCleanReleased[translated_key]) return true;' in text and 'if(key_up && (sCleanReleased[key] || sCleanKeys.releaseKey(key)))' in text):errors.append('I1 physical release routes')
 if not ('res = (*function)(KEYSTATE_DOWN);' in text and 'if(res) sCleanKeys.started' in text and text.index('res = (*function)(KEYSTATE_DOWN);')<text.index('if(res) sCleanKeys.started')):errors.append('I2 record actual DOWN')
 if not ('ALKeyframeInputGate::seatedMoveAgent(gAgent.forwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())' in text and 'ALKeyframeInputGate::seatedMoveAgent(gAgent.backwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())' in text and 'if (!clean && binding[i].mFunctionName' in text and 'const auto button=cleanButton(clicktype);' in text and text.index('sCleanMouse.mouseDown(button,ALCleanFrame::active())')<text.index('gViewerWindow->handleAnyMouseClick')):errors.append('I3 branch and mouse ownership')
 return errors

def check():
 d=check_directory('keyframe_input'); originals={f:(NV/f).read_text(encoding='utf-8') for f in ('alkeyframeinputgate.h','alcleantelemetry.h','alkeyframeparticles.h')}
 def build(files,name,run=True):
  for f,s in files.items():(d/f).write_bytes(s.encode())
  return compile_run(d,FIXTURE,name,flags=(f'/I"{d}"',f'/I"{NV}"','/O2'),run=run)
 clean=build(originals,'clean');print(clean.stdout,end='');assert clean.returncode==0,clean.stderr
 negatives=[
 ('skip_frozen_rebuild','particles','frozen prebuilt consumption','alkeyframeparticles.h','for(auto group:groups) rebuild(group);','for(auto group:groups) {}'),
 ('allow_effect_geometry','particles','frozen prebuilt consumption','alkeyframeparticles.h','return !clean || !effect;','return true;'),
 ('forget_swallowed','keys','held denied key fires after Play','alkeyframeinputgate.h','if (mSwallowed.count(key) && denied(action, cameraOnlyMove)) return false;','if (false) return false;'),
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
  ('if(sCleanReleased[translated_key]) return true;','if(false) return true;','I1 physical release routes'),
  ('if(res) sCleanKeys.started','if(false) sCleanKeys.started','I2 record actual DOWN'),
  ('ALKeyframeInputGate::seatedMoveAgent(gAgent.forwardGrabbed(),sit,gAgent.getRunning(),gAgent.getAlwaysRun())','false','I3 branch and mouse ownership')]:
  assert before in text;assert assertion in source_rules(text.replace(before,after,1));print('PASS negative [src]: FAIL '+assertion)
 print('PASS clean input/telemetry after 6 semantic and 3 source negatives')
if __name__=='__main__':check()
