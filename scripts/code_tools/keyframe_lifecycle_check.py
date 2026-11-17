"""Execute the production transport and RLVa dispatch/pose handback model."""
from pathlib import Path
import subprocess
from native_check_tools import check_directory, compile_run
ROOT=Path(__file__).resolve().parents[2]
NV=ROOT/'indra/newview'
FIXTURE=r"""
#include "alkeyframetransport.h"
#include <cmath>
#include <iostream>
#include <cstdlib>
void require(bool value,const char* name) { if(!value) { std::cout<<"FAIL "<<name<<"\n"; std::exit(1); } }
int main(int argc,char** argv) {
 const std::string selected=argc>1?argv[1]:"all";
 auto run=[&](const char* name){return selected=="all" || selected==name;};
 using T=ALKfTransport;
 if(run("lifecycle")) {
  for(int reason=0;reason<6;++reason) {
   T t;t.play(true,2,false,2);require(t.takeEffects().acquire,"lease acquired");
   T::Lifecycle l;switch(reason) {case 0:l.teleport=true;break;case 1:l.disconnect=true;break;case 2:l.restricted=true;break;case 3:l.pilot=true;break;case 4:l.recorder=true;break;case 5:l.scene=true;break;}
   t.lifecycleCheck(l);require(!t.ownsCamera() && t.takeEffects().release,"lifecycle releases");
   t.lifecycleCheck(l);require(!t.takeEffects().release,"idempotent release");
  }
  for(auto reason:{T::Exit::CUT,T::Exit::ESCAPE}) {T t;t.play(true,2,false,2);t.takeEffects();t.stop(reason);require(t.takeEffects().release && !t.ownsCamera(),"delay Escape CUT");}
  std::cout<<"PASS lifecycle teleport/disconnect/RLVa/PILOT/recorder/scene/CUT/Escape\n";
 }
 if(run("timing")) {
  T empty;require(!empty.play(false,0,true,0),"empty refused");
  T zero;zero.play(true,0,true,0);zero.takeEffects();zero.advance({1,1000,0,false});
  require(zero.state()==T::State::HOLD_END && zero.time()==0,"zero loop hold");
  require(!zero.takeEffects().release && zero.clean(),"HOLD_END retains lease");
  zero.advance({2,1,1,false});require(zero.ownsCamera(),"HOLD_END persists");
  T loop;loop.play(true,3,true,2);loop.advance({1,1000,0,false});require(std::abs(loop.time()-2)<1e-9,"large dt loop");
  T once;once.play(true,20,false,0);once.advance({4,.7,.1,false});once.advance({4,.7,.1,false});
  require(std::abs(once.time()-.7)<1e-12,"one idle advances once");
  once.advance({5,1000,0,false});require(once.state()==T::State::HOLD_END,"stall hold");
  T driven;driven.play(true,2,false,2,true);driven.advance({0,1,.25,true});require(driven.time()==.25,"camera drive clock");
  std::cout<<"PASS timing empty/zero/1000s/stall/generation/drive clock\n";
 }
 if(run("handback")) {
  T t;t.play(true,5,false,0);t.takeEffects();
  T::Lifecycle l;l.restricted=true;t.lifecycleCheck(l);auto fx=t.takeEffects();
  require(fx.resetAgent && fx.release,"RLVa reset once");
  double presented=20;bool cameraNeedsUpdate=false;
  for(int i=0;i<3;++i) {
   t.lifecycleCheck(l); require(t.rlvHandbackActive(),"RLVa latch retained");
   // Mirrors the source-gated dispatch: force actual agent write, not only owner tag.
   if(t.rlvHandbackActive()) {cameraNeedsUpdate=true;if(cameraNeedsUpdate) presented=2;}
   require(presented==2 && presented<=3,"RLVa presented pose clamp");
   require(!t.takeEffects().resetAgent,"RLVa reset one shot");
  }
  l.restricted=false;t.lifecycleCheck(l);require(!t.rlvHandbackActive() && t.takeEffects().resetFlycam,"RLVa lift reseeds");
  std::cout<<"PASS handback KEYFRAME -> AGENT latched/pose clamp -> normal\n";
 }
}
"""
def check():
 directory=check_directory('keyframe_lifecycle')
 source=(NV/'alkeyframetransport.cpp').read_text(encoding='utf-8')
 def build(code,name,run=True):return compile_run(directory,code+'\n'+FIXTURE,name,flags=(f'/I"{NV}"','/O2'),run=run)
 clean=build(source,'clean');print(clean.stdout,end='');assert clean.returncode==0,clean.stderr
 negatives=[
  ('no_teleport_release','lifecycle','lifecycle releases','if (l.disconnect || l.teleport)','if (l.disconnect)'),
  ('release_at_end','timing','HOLD_END retains lease','mTime = mLength; mState = State::HOLD_END;','mTime = mLength; mState = State::HOLD_END; mEffects.release = true;'),
  ('double_advance','timing','one idle advances once','if (mAdvanced && mGeneration == frame.generation) return;','if (false) return;'),
  ('premature_handback_clear','handback','RLVa latch retained','if (mHandback && !l.restricted)','if (mHandback)')]
 for name,selected,assertion,before,after in negatives:
  assert source.count(before)==1
  exe=build(source.replace(before,after),name,False)
  result=subprocess.run([str(exe),selected],capture_output=True,text=True)
  assert result.returncode==1 and result.stdout.count('FAIL ')==1 and 'FAIL '+assertion+'\n' in result.stdout,(name,result.stdout,result.stderr)
  print('PASS negative [exec] '+name+': '+result.stdout.strip())
 clean=build(source,'clean_after_negatives');print(clean.stdout,end='');assert clean.returncode==0
 print('PASS clean lifecycle after 4 semantic negatives')
if __name__=='__main__':check()
