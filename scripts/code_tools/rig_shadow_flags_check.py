"""Execute production pick provenance and readiness without a viewer build.

Called by rig_shadow_presets_check; only UUIDs, logging and object storage are
standalone doubles. LLSD loader/saver and cue blending execute in that gate's
compiled model adapter.
"""
from pathlib import Path
import subprocess
import re


def check(out, source_ref="WORKTREE"):
    from rig_shadow_presets_check import ROOT, function
    out=Path(out)/'flags';out.mkdir(parents=True,exist_ok=True)
    from relight_3d_gl import SourceTree
    tree=SourceTree(source_ref)
    source=tree.read('indra/newview/pipeline.cpp')
    rig=tree.read('indra/newview/alcinelightrig.cpp')
    menu=tree.read('indra/newview/alviewermenu.cpp')
    ordered='S32 fixture_priority' in source
    update=function(rig,'void ALCineLightRig::updateProjectorFlags()')
    assert update.count('setRigWorldShadowPick(')==1
    assert update.count('setProjectorDetailShadows(')==1
    # Execute the complete production update loop. The trailing block is
    # diagnostic logging only; object/shaft/hero storage below are doubles.
    update_loop=update[:update.index('    // [DIAG cine-shadow]')]+'}\n'
    names=['void LLPipeline::setRigWorldShadowPick(', 'void LLPipeline::setWorldShadowPick(',
           'void LLPipeline::toggleWorldShadowPick(', 'bool LLPipeline::isWorldShadowPicked(',
           'void LLPipeline::clearWorldShadowPicks(', 'std::vector<LLUUID> LLPipeline::worldShadowPickList(',
           'std::vector<LLUUID> LLPipeline::manualWorldShadowPickList(', 'void LLPipeline::setWorldShadowPicks(',
           'void LLPipeline::setProjectorDetailShadows(']
    shim=r'''
#include <set>
#include <algorithm>
#include <map>
#include <iostream>
#include <cassert>
#include "alworldshadowmath.h"
using U64=unsigned long long;
using F32=float;using S32=int;
struct LLUUID { int value=0; bool isNull()const{return value==0;} bool operator<(const LLUUID& b)const{return value<b.value;} bool operator==(const LLUUID& b)const{return value==b.value;} };
std::ostream& operator<<(std::ostream& o,const LLUUID& id){return o<<id.value;}
#define LL_WARNS(x) std::cerr
#define LL_ENDL std::endl
struct LLPipeline {
 static inline std::set<LLUUID> sWorldShadowPicks;
 RIG_STORAGE
 static inline std::set<LLUUID> sDetailShadowProjectors;
 static inline std::map<LLUUID,F32> sSoftness;
 static inline std::map<LLUUID,std::string> sWorldShadowPickNames;
 static inline U64 sWorldShadowPickRevision=0;
 RIG_SIGNATURE
 static void setWorldShadowPick(const LLUUID&,bool,const std::string& name="");
 static void toggleWorldShadowPick(const LLUUID&,const std::string& name="");
 static bool isWorldShadowPicked(const LLUUID&);
 static void clearWorldShadowPicks();
 static std::vector<LLUUID> worldShadowPickList();
 static std::vector<LLUUID> manualWorldShadowPickList();
 static void setWorldShadowPicks(const std::vector<LLUUID>&);
 static void setProjectorDetailShadows(const LLUUID&,bool);
 static bool isVolumetricShaftEnabled(const LLUUID&){return false;}
 static bool isVCamVolumetricAutoEnabled(const LLUUID&){return false;}
 static bool isHeroProjector(const LLUUID&){return false;}
 static void toggleVolumetricShaft(const LLUUID&){}
 static void toggleHeroProjector(const LLUUID&){}
 static void setProjectorShadowSoftness(const LLUUID& id,F32 s){sSoftness[id]=s;}
 static void clearProjectorShadowSoftness(const LLUUID& id){sSoftness.erase(id);}
};
struct LLVOVolume {LLUUID id;bool dead=false;LLUUID getID()const{return id;}bool isDead()const{return dead;}};
struct VolumePtr {LLVOVolume* ptr=nullptr;bool isNull()const{return !ptr;}LLVOVolume* operator->()const{return ptr;}};
struct ALCineLightRig {
 static constexpr S32 LIGHT_COUNT=4;
 struct Flags{bool mDetailShadows=false,mWorldShadowPick=false;};
 VolumePtr mProjectors[4];Flags mCurrentLive[4];
 bool mShaftEnabled[4]={},mHeroEnabled[4]={};F32 mShadowSoftness[4]={};
 void updateProjectorFlags();
};
'''
    shim=shim.replace('RIG_STORAGE','static inline std::map<LLUUID,S32> sRigWorldShadowPicks;' if ordered else 'static inline std::set<LLUUID> sRigWorldShadowPicks;')
    shim=shim.replace('RIG_SIGNATURE','static void setRigWorldShadowPick(const LLUUID&,bool,S32=3);' if ordered else 'static void setRigWorldShadowPick(const LLUUID&,bool);')
    import textwrap
    menu_start=menu.index('    void handle_object_world_shadow(')
    menu_end=menu.index('    // [BDMerge G3.3 Batch 3]',menu_start)
    menu_source=textwrap.dedent(menu[menu_start:menu_end])
    handler=function(menu_source,'void handle_object_world_shadow(')
    checked=function(menu_source,'bool check_object_world_shadow(')
    menu_shim=r'''
struct LLSD {};
struct WorldShadowPickLight {LLUUID mId;std::string mName;};
std::vector<WorldShadowPickLight> selected;
std::vector<WorldShadowPickLight> selected_world_shadow_lights(){return selected;}
'''
    tests=r'''
int main(){
 using P=LLPipeline;
 const LLUUID key{10},manual{20},bounce{30},respawned{40};
 // Cold apply: native-mapped key becomes eligible, without a menu click.
 P::setRigWorldShadowPick(key,true);
 assert(P::isWorldShadowPicked(key)&&!P::isWorldShadowPicked(bounce));
 assert(ALWorldShadow::worldEligibleWithNative(true,P::isWorldShadowPicked(key)));
 LLUUID ranked[]={key,bounce};
 ALWorldShadow::Residency<LLUUID> resident;
 ALWorldShadow::updateResidencyPicked(resident,1,[](auto){return true;},P::isWorldShadowPicked,[](auto){return true;},ranked,1,ranked,2,true);
 assert(resident.contains(key)&&!resident.contains(bounce));
 // Native remains sampled on the first frame and on allocation/stamp failure.
 ALWorldShadow::PickedOwnership<LLUUID> ownership;int view=1;
 assert(ALWorldShadow::channelUsesNativeMap(true,true,ownership.owns(key,1,&view)));
 ownership.begin(1,&view);assert(ownership.add(key,1,&view));ownership.commit(1,&view);
 assert(!ALWorldShadow::channelUsesNativeMap(true,true,ownership.owns(key,1,&view)));
 assert(ALWorldShadow::channelUsesNativeMap(true,true,ownership.owns(key,2,&view)));
 // A manual preference for the same key must survive preset cleanup.
 P::setWorldShadowPick(manual,true);P::setWorldShadowPick(key,true);
 auto manual_revision=P::sWorldShadowPickRevision;
 P::setRigWorldShadowPick(key,false);
 assert(P::sWorldShadowPickRevision==manual_revision);
 assert(P::isWorldShadowPicked(key)&&P::isWorldShadowPicked(manual));
 P::setWorldShadowPick(key,false);assert(!P::isWorldShadowPicked(key));
 P::setRigWorldShadowPick(key,true);
 assert(P::manualWorldShadowPickList()==std::vector<LLUUID>{manual});
 auto saved=P::manualWorldShadowPickList();P::setWorldShadowPicks(saved);
 P::setRigWorldShadowPick(key,false);
 assert(P::worldShadowPickList()==saved);
 // Respawn with a stable UUID, and with a new emitter UUID, binds the same
 // fixture intent without retaining an orphan pick or promoting its bounce.
 P::setRigWorldShadowPick(key,true);auto rev=P::sWorldShadowPickRevision;
 P::setRigWorldShadowPick(key,true);assert(rev==P::sWorldShadowPickRevision);
 P::setRigWorldShadowPick(key,false);P::setRigWorldShadowPick(key,true);
 assert(P::isWorldShadowPicked(key));
 P::setRigWorldShadowPick(key,false);P::setRigWorldShadowPick(respawned,true);
 assert(!P::isWorldShadowPicked(key)&&P::isWorldShadowPicked(respawned));
 assert(!P::isWorldShadowPicked(bounce)&&P::isWorldShadowPicked(manual));
 P::setRigWorldShadowPick(respawned,false);assert(P::worldShadowPickList()==saved);
 // A crisp fixture's zero-softness sentinel must not clear either new flag.
 ALCineLightRig rig;LLVOVolume emitter{key};rig.mProjectors[0].ptr=&emitter;
 rig.mCurrentLive[0]={true,true};rig.updateProjectorFlags();
 assert(P::isWorldShadowPicked(key)&&P::sDetailShadowProjectors.count(key));
 assert(P::sSoftness.count(key)==0);
 rig.mShadowSoftness[0]=3.f;rig.updateProjectorFlags();
 assert(P::isWorldShadowPicked(key)&&P::sDetailShadowProjectors.count(key)&&P::sSoftness[key]==3.f);
 rig.mCurrentLive[0]={false,false};rig.updateProjectorFlags();
 assert(!P::isWorldShadowPicked(key)&&!P::sDetailShadowProjectors.count(key)&&P::isWorldShadowPicked(manual));
 // Execute the actual menu handler with 31 manual + 1 rig preference.
 P::clearWorldShadowPicks();P::sRigWorldShadowPicks.clear();
 for(int i=100;i<131;++i)P::setWorldShadowPick(LLUUID{i},true);
 P::setRigWorldShadowPick(key,true);
 selected={{{100},"old"},{{131},"new"}};
 handle_object_world_shadow({});
 auto manual_after=P::manualWorldShadowPickList();
 bool capacity=manual_after.size()==32 && std::find(manual_after.begin(),manual_after.end(),LLUUID{100})!=manual_after.end()
     && std::find(manual_after.begin(),manual_after.end(),LLUUID{131})!=manual_after.end();
 P::setWorldShadowPick(LLUUID{131},false); // Admit the rig preference independently of the capacity assertion.
 selected={{key,"rig only"}};bool membership=!check_object_world_shadow();
 P::clearWorldShadowPicks();P::sRigWorldShadowPicks.clear();
 for(int i=100;i<131;++i)P::setWorldShadowPick(LLUUID{i},true);
 RIG_PRIORITY_TEST
 auto picks=P::worldShadowPickList();
 bool priority=std::find(picks.begin(),picks.end(),LLUUID{90})!=picks.end() && std::find(picks.begin(),picks.end(),LLUUID{1})==picks.end();
 std::cerr<<"manual capacity="<<capacity<<" membership="<<membership<<" fixture priority="<<priority<<std::endl;
 assert(capacity && membership && priority);
 std::cout<<"[shadow-flags] PASS production crisp/soft update loop, cold arbitration, provenance, manual save/load, stable/new-ID respawn, cleanup, first-frame fallback\n";
}
'''
    tests=tests.replace('RIG_PRIORITY_TEST','P::setRigWorldShadowPick(LLUUID{1},true,2);P::setRigWorldShadowPick(LLUUID{90},true,0);' if ordered else 'P::setRigWorldShadowPick(LLUUID{1},true);P::setRigWorldShadowPick(LLUUID{90},true);')
    (out/'flags.cpp').write_text(shim+'\n'.join(function(source,n) for n in names)+update_loop+menu_shim+handler+checked+tests,encoding='utf-8')
    vc=r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
    cmd=out/'build.cmd'
    cmd.write_text(f'@echo off\ncall "{vc}" >nul\ncl /nologo /std:c++20 /EHsc /I"{ROOT/"indra/newview"}" "{out/"flags.cpp"}" /Fe:"{out/"flags.exe"}" /Fo:"{out/"flags.obj"}"\nif errorlevel 1 exit /b %ERRORLEVEL%\n"{out/"flags.exe"}"\n')
    result=subprocess.run(['cmd','/d','/c',str(cmd.resolve())],capture_output=True,text=True)
    (out/'build.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    assert result.returncode==0,result.stdout+result.stderr
    assert 'const LLUUID id = mProjectors[i]->getID();' in update
    assert ('setRigWorldShadowPick(id, mCurrentLive[i].mWorldShadowPick, i)' if ordered else 'setRigWorldShadowPick(id, mCurrentLive[i].mWorldShadowPick)') in update
    assert 'setProjectorDetailShadows(id, mCurrentLive[i].mDetailShadows)' in update
    assert 'mOmnis' not in update
    destroy=function(rig,'void ALCineLightRig::destroyEmitter(')
    assert 'setRigWorldShadowPick(id, false)' in destroy
    assert 'setProjectorDetailShadows(id, false)' in destroy
    assert 'manualWorldShadowPickList()' in (ROOT/'indra/newview/llfloaterdirector.cpp').read_text(encoding='utf-8')
    panel=(ROOT/'indra/newview/alpanelcinelightrig.cpp').read_text(encoding='utf-8')
    actor=function(panel,'void ALPanelCineLightRig::onActorOnlyAllChanged(')
    actor_names=re.search(r'NAMES\[5\]\s*=\s*\{(.*?)\}',actor,re.S)[1]
    assert len(re.findall(r'"[^"]+"',actor_names))==5
    assert 'DetailShadows' not in actor_names and 'WorldShadowPick' not in actor_names
    print(result.stdout.strip(),flush=True)
    return dict(passed=True,cold_arbitration=True,manual_provenance=True,manual_menu_capacity=True,manual_menu_membership=True,fixture_priority=True,stable_and_new_id_respawn=True,first_frame_fallback=True,viewer_call_sites=True)


if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('--source-ref',default='WORKTREE');ap.add_argument('--out',default='.tmp/rig_shadow/fix2')
    args=ap.parse_args();check(args.out,args.source_ref)
