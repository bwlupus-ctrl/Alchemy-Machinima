"""Execute production KfDoc/LLSD/file/scene bridge, plus validate-first scene source rules."""
from pathlib import Path
import subprocess
from native_check_tools import check_directory, compile_run, viewer_native_flags
ROOT=Path(__file__).resolve().parents[2]; NV=ROOT/'indra/newview'
FIXTURE=r"""
#include <iostream>
#include <cstdlib>
void require(bool v,const char* name){if(!v){std::cout<<"FAIL "<<name<<"\n";std::exit(1);}}
int main(int argc,char** argv){
 std::string selected=argc>1?argv[1]:"all";
 ALKf::Sequence original;original.uid="scene sequence";original.name="Camera";
 auto track=ALKf::cameraTrack();ALKf::Key key;key.values={ALKf::Vec3{250001,250002,100},ALKf::axisAngle({1,0,0},.3),1.1,.3};
 track.lock.kind=ALKf::RefKind::Clone;track.lock.id[0]=123;track.lock.stableId[0]=42;track.lock.fallback={1,2,3};
 auto id=ALKf::appendTrack(original,track);ALKf::appendKey(original,id,key);
 ALKf::Track generic;generic.kind="future";generic.schema={{"enabled",ALKf::ChannelType::Step,ALKf::Interp::Step,ALKf::Space::Linear}};
 ALKf::Key step;step.values={std::int64_t{5}};generic.keys.push_back(step);generic.keys[0].id=1;generic.nextKeyId=2;generic.uid="unknown";
 ALKf::appendTrack(original,generic);
 original.revision=9007199254741001ULL;
 std::string reason;require(ALKf::validate(original,reason),"scene valid fixture");
 if(selected=="all"||selected=="bridge"){
  const auto doc=ALKf::encode(original);KfDoc back;
  require(ALKeyframeLibrary::fromLLSD(ALKeyframeLibrary::toLLSD(doc),back,reason)&&back==doc,"generic LLSD round trip");
  auto result=original;LLSD malformed;malformed["keyframes"]["payload_version"]=1;malformed["keyframes"]["sequence"]=LLSD::emptyMap();
  require(!ALKeyframeLibrary::loadScene(malformed,result,reason)&&result==original,"invalid scene atomic");
  require(ALKeyframeLibrary::loadScene(LLSD::emptyMap(),result,reason)&&result==original,"absent scene untouched");
  LLSD scene;scene["keyframes"]=ALKeyframeLibrary::saveScene(original);
  require(ALKeyframeLibrary::loadScene(scene,result,reason),"scene round trip");
  require(result.tracks[0].lock.id==ALKf::Id{}&&result.tracks[0].lock.stableId[0]==42&&result.tracks[0].lock.fallback==ALKf::Vec3{1,2,3},"clone fallback without spawn");
  require(result.tracks[1].kind=="future"&&result.tracks[0].keys[0].values==original.tracks[0].keys[0].values,"unknown kind and roll preserved");
  scene["keyframes"]["payload_version"]=2;auto before=result;
  require(!ALKeyframeLibrary::loadScene(scene,result,reason)&&result==before,"version validation atomic");
  std::cout<<"PASS scene LLSD/unknown kind/64bit revision/roll/absent/invalid atomic/clone fallback\n";
 }
 if(selected=="all"||selected=="file"){
  const std::string file=".tmp/native_checks/keyframe_scene/verified.xml";
  require(ALKeyframeLibrary::saveVerified(file,original,reason),"verified file write");
  auto expected=original;
  ALKf::rewriteRefs(expected,[](ALKf::RefSlot& ref){if(ref.kind==ALKf::RefKind::Clone){ref.kind=ALKf::RefKind::Point;ref.id={};}});
  ALKf::Sequence loaded;require(ALKeyframeLibrary::load(file,loaded,reason)&&loaded==expected,"verified file read");
  original.name="Replacement";require(ALKeyframeLibrary::saveVerified(file,original,reason),"verified file replace");
  expected=original;ALKf::rewriteRefs(expected,[](ALKf::RefSlot& ref){if(ref.kind==ALKf::RefKind::Clone){ref.kind=ALKf::RefKind::Point;ref.id={};}});
  require(ALKeyframeLibrary::load(file,loaded,reason)&&loaded==expected,"verified replacement read");
  std::cout<<"PASS scene verified write/read/replace\n";
 }
}
"""
NL=chr(10)
def source_rules(files):
 f=files['llfloaterdirector.cpp']; h=files['llfloaterdirector.h']; lib=files['alkeyframelibrary.cpp']; errors=[]
 load=f[f.index('bool LLFloaterDirector::loadScene('):f.index('LLFloaterDirector::SceneChangeStamp LLFloaterDirector::currentSceneStamp()')]
 if not load.index('ALKeyframeLibrary::loadScene(scene, keyframe_candidate, keyframe_reason)')<load.index('LLPrismLens::applySceneData'):errors.append('S1 validation before Prism')
 if 'if (scene.has("keyframes"))\n        ALKeyframePlayer::instance().replaceSequence' not in load:errors.append('S2 absent session untouched')
 if 'stamp.mKeyframes = ALKeyframePlayer::instance().revision();' not in f or 'mKeyframes == other.mKeyframes' not in h:errors.append('S3 scene stamp')
 if 'ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()) return;' not in f:errors.append('S4 autosave deferred')
 quit_body=f[f.index('void LLFloaterDirector::prepareRecoveryForShutdown()'):f.index('void LLFloaterDirector::cancelRecoveryShutdown()')]
 idle=f[f.index('void LLFloaterDirector::idleSceneRecovery(bool force_write)'):f.index('void LLFloaterDirector::sceneRecoveryIdle(void*)')]
 if not ('if (!force_write)'+NL+'        if (ALKeyframePlayer::instance().ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()) return;' in idle and idle.index('if (!force_write)')<idle.index('updateSceneDirty();') and 'idleSceneRecovery(true);' in quit_body and quit_body.index('sRecoveryShutdownPrepared = true;')<quit_body.index('idleSceneRecovery(true);')):errors.append('S7 forced shutdown capture never deferred')
 if 'SCENE_VERSION   = 6;' not in f or 'block["payload_version"]=1' not in lib:errors.append('S5 independent payload version')
 if not ('\n    ALKeyframePlayer::instance().stop(ALKfTransport::Exit::SCENE);\n    if (scene.has("keyframes"))' in load and
  load.index('LLPrismLens::applySceneData')<load.index('ALKeyframePlayer::instance().stop(ALKfTransport::Exit::SCENE);')<load.index('ALKeyframePlayer::instance().replaceSequence')):errors.append('S6 every validated scene releases keyframes')
 return errors

def check():
 d=check_directory('keyframe_scene');flags,libraries=viewer_native_flags();flags+=['/Gy']; build_root=Path(libraries[0].strip(chr(34))).parents[2]; libraries.insert(0,chr(34)+str(build_root/'llfilesystem/Release/llfilesystem.lib')+chr(34))
 sources={f:(NV/f).read_text(encoding='utf-8') for f in ('alkeyframetransport.cpp','alkeyframecore.cpp','alkeyframecodec.cpp','alkeyframelibrary.cpp')}
 def build(files,name,run=True):
  for f,s in files.items():(d/f).write_bytes(s.encode())
  return compile_run(d,'\n'.join('#include "'+f+'"' for f in files)+'\n'+FIXTURE,name,flags=flags,libraries=libraries,run=run)
 clean=build(sources,'clean');print(clean.stdout,end='',flush=True);assert clean.returncode==0,(clean.stdout,clean.stderr)
 # Actual bridge mutation must compile and fail the intended runtime assertion.
 before='if(!fromLLSD(block["sequence"],doc,reason)) return false;'
 mutant=dict(sources);assert before in mutant['alkeyframelibrary.cpp'];mutant['alkeyframelibrary.cpp']=mutant['alkeyframelibrary.cpp'].replace(before,'result={}; '+before)
 exe=build(mutant,'premature_scene_mutation',False);result=subprocess.run([str(exe),'bridge'],capture_output=True,text=True)
 assert result.returncode==1 and result.stdout.count('FAIL ')==1 and 'FAIL invalid scene atomic\n' in result.stdout,(result.stdout,result.stderr)
 print('PASS negative [exec] premature_scene_mutation: '+result.stdout.strip(),flush=True)
 files={f:(NV/f).read_text(encoding='utf-8') for f in ('llfloaterdirector.cpp','llfloaterdirector.h','alkeyframelibrary.cpp')};assert not source_rules(files),source_rules(files)
 for file,before,after,assertion in [
 ('llfloaterdirector.cpp','\n    ALKeyframePlayer::instance().stop(ALKfTransport::Exit::SCENE);\n    if (scene.has("keyframes"))','\n    if (scene.has("keyframes")) ALKeyframePlayer::instance().stop(ALKfTransport::Exit::SCENE);\n    if (scene.has("keyframes"))','S6 every validated scene releases keyframes'),
 ('llfloaterdirector.cpp','if (scene.has("keyframes"))\n        ALKeyframePlayer::instance().replaceSequence','if (true)\n        ALKeyframePlayer::instance().replaceSequence','S2 absent session untouched'),
 ('llfloaterdirector.h','mKeyframes == other.mKeyframes','true','S3 scene stamp'),
 ('llfloaterdirector.cpp','    if (!force_write)\n        if (ALKeyframePlayer::instance().ownsCamera()','    if (true)\n        if (ALKeyframePlayer::instance().ownsCamera()','S7 forced shutdown capture never deferred'),
 ('llfloaterdirector.cpp','ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()) return;','ownsCamera()) return;','S4 autosave deferred'),
 ('llfloaterdirector.cpp','SCENE_VERSION   = 6;','SCENE_VERSION   = 7;','S5 independent payload version')]:
  planted=dict(files);assert before in planted[file];planted[file]=planted[file].replace(before,after,1);assert assertion in source_rules(planted)
  print('PASS negative [src]: FAIL '+assertion,flush=True)
 # Move the validation call after Prism, without changing the production file.
 planted=dict(files);text=planted['llfloaterdirector.cpp'];a='ALKeyframeLibrary::loadScene(scene, keyframe_candidate, keyframe_reason)';b='LLPrismLens::applySceneData';text=text.replace(a,'__VALIDATE__',1).replace(b,a,1).replace('__VALIDATE__',b,1);planted['llfloaterdirector.cpp']=text;assert 'S1 validation before Prism' in source_rules(planted);print('PASS negative [src]: FAIL S1 validation before Prism',flush=True)
 clean=build(sources,'clean_after_negatives');print(clean.stdout,end='',flush=True);assert clean.returncode==0
 print('PASS clean scene after 1 executable and 7 source negatives')
if __name__=='__main__':check()
