"""Round 2: execute production authored mutations and both menu actions.
Service adapters supply avatar/clone aliases and optional v2 shell; no viewer build.
"""
from pathlib import Path
from native_check_tools import check_directory, compile_run, viewer_native_flags
from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]

def run_unit(root=ROOT):
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    avatar = (root/'indra/newview/alghostclonemenu.cpp').read_text(encoding='utf8')
    director = (root/'indra/newview/llfloaterdirector.cpp').read_text(encoding='utf8')
    ui = (root/'indra/newview/alpanelrigrim.cpp').read_text(encoding='utf8')
    history = (root/'indra/newview/alkithistory.cpp').read_text(encoding='utf8')
    def cheap_refresh(text):
        block = function(text, 'void ALExtraRimUI::refresh(')
        assert 'configurationRevision()) return;' in block and block.index('configurationRevision()) return;') < block.index('rigs.extraRims()'), 'xr_idle_no_rows'
        assert 'if (mSummary != summary)' in block, 'xr_idle_no_rows'
    cheap_refresh(ui)
    try:
        cheap_refresh(ui.replace('        mConfigurationRevision == rigs.configurationRevision()) return;',
                                '        mConfigurationRevision == rigs.configurationRevision()) {}'))
    except AssertionError as error:
        assert str(error) == 'xr_idle_no_rows', error
        print('PASS: rejected planted xr_idle_no_rows at its named source assertion', flush=True)
    else:
        raise AssertionError('xr_idle_no_rows planted bug escaped')
    for action in ('undo', 'redo'):
        block = function(history, 'void ALKitHistory::'+action+'(')
        assert block.index('finishPendingTrims();') < block.index('prepareReplay('), 'xr_history_settles_strength'
    refresh = function(ui, 'void ALExtraRimUI::refresh(')
    assert refresh.index('configurationRevision()) return;') < refresh.index('rigs.extraRims()'), 'xr_idle_no_rows'
    assert 'if (mSummary != summary)' in refresh, 'xr_idle_no_rows'
    assert 'mRecipientObject' not in manager, 'xr_recipient_id'
    source = r"""
#include "linden_common.h"
#include "alextrarimmodel.h"
#include "llsdutil.h"
#include <map>
#include <set>
#include <memory>
#include <iostream>
#include <cstdlib>
#define LL_INFOS(x) std::cerr
#define LL_ENDL std::endl
struct ALGhostStudio {
 enum {BACKING_OVERLAY}; struct Clone {int mKind=1;};
 static ALGhostStudio& instance(){static ALGhostStudio s;return s;}
 const Clone* getInstance(LLUUID)const{return nullptr;}
 const Clone* findInstanceByRuntime(LLUUID)const{return nullptr;}
 bool isPrivateOverlayRuntime(LLUUID)const{return false;}
 bool isDeleting(LLUUID)const{return false;}
};
struct LLDirectorCast {
 static std::map<LLUUID,LLUUID> aliases;
 static LLUUID canonicalActorId(LLUUID id){auto i=aliases.find(id);return i==aliases.end()?id:i->second;}
 static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 bool contains(LLUUID)const{return true;}
};
std::map<LLUUID,LLUUID> LLDirectorCast::aliases;
struct Rig {void shutdown(){}};
struct ALCineLightRigManager {
 enum class ExtraRimState {ACTIVE,SOURCE_OFF,BACK_OFF,AWAY,GEOMETRY,EMITTER_UNAVAILABLE,OWN_RIG,MASK_DISABLED,MASK_OVERFLOW,WAITING};
 struct ExtraRim {ExtraRimState mState=ExtraRimState::WAITING; LLUUID mActor;F32 mTrim=1;bool mEnrolled=false;std::unique_ptr<Rig> mRig;};
 std::vector<ExtraRim> mExtraRims; int mExtraRimSource=0;
 U64 mExtraRimRevision=1,mConfigurationRevision=1;
 bool mExtraRimRegistryDirty=false,mBorrowAnchorsDirty=false;
 std::map<LLUUID,LLUUID> mBorrowRekeys;
 static ALCineLightRigManager& instance(){static ALCineLightRigManager m;return m;}
 bool hasOwnLitRig(LLUUID)const{return false;}
 LLUUID resolveBorrowActor(LLUUID id,bool=true)const{return resolveRigActorIdentity(id);}
 static bool validSlotValue(int s){return s>=0 && s<5;}
 void rebuildActorOnlyRegistry(){}
 void shutdownExtraRims(){}
 LLUUID resolveRigActorIdentity(const LLUUID&)const;
 bool hasExtraRim(const LLUUID&)const;
 bool canAddExtraRim(const LLUUID&,std::string* = nullptr,bool=false)const;
 bool addExtraRim(const LLUUID&);
 void removeExtraRim(const LLUUID&);
 void setExtraRimTrim(const LLUUID&,F32);
 void setExtraRimSource(S32);
 LLSD captureExtraRims()const;
 void restoreExtraRims(const LLSD&);
 void setExtraRimState(ExtraRim&,ExtraRimState);
};
"""
    for sig in ('LLUUID ALCineLightRigManager::resolveRigActorIdentity(', 'bool ALCineLightRigManager::hasExtraRim(', 'bool ALCineLightRigManager::canAddExtraRim(', 'bool ALCineLightRigManager::addExtraRim(', 'void ALCineLightRigManager::removeExtraRim(', 'void ALCineLightRigManager::setExtraRimTrim(', 'void ALCineLightRigManager::setExtraRimSource(', 'LLSD ALCineLightRigManager::captureExtraRims(', 'void ALCineLightRigManager::restoreExtraRims(', 'void ALCineLightRigManager::setExtraRimState('):
        source += function(manager,sig)+'\n'
    source += r"""
struct ALKitShell { void ensurePageBuilt(const char*){} };
bool shellAvailable=false; int historySteps=0, settlements=0;
struct LLFloater {template<class T>T* findChild(const char*){static T shell;return shellAvailable?&shell:nullptr;}};
struct LLFloaterReg {static LLFloater* getInstance(const char*){static LLFloater f;return &f;}};
struct ALExtraRimUI {
 static void finishPendingTrims(){++settlements;}
 static void recordEdit(ALKitShell* shell,const LLSD& before,const char*){
  if(shell && before!=ALCineLightRigManager::instance().captureExtraRims())++historySteps;
 }
};
LLUUID picked;
LLUUID avatarRimActor(){return picked;}
bool ensureAvatarGazeCast(LLUUID){return true;}
void tip(const std::string&){}
struct LLFloaterDirector {
 std::shared_ptr<LLUUID> mExtraRimMenuPick; ALKitShell* mShell=nullptr;
 void onCastExtraRimToggle();
};
"""
    source += function(avatar,'void actAvatarRim()\n{')+'\n'+function(director,'void LLFloaterDirector::onCastExtraRimToggle()')+r"""
void require(bool ok,const char* name){if(!ok){std::cerr<<name<<"\n";std::exit(1);}}
LLUUID id(int n){return LLUUID(llformat("00000000-0000-0000-0000-%012d",n));}
int main(){
 auto& m=ALCineLightRigManager::instance();
 for(bool shell:{false,true}){
  shellAvailable=shell;picked=id(1);
  int steps=historySteps,settled=settlements;
  actAvatarRim();require(m.hasExtraRim(picked),"xr_menu_no_shell");
  actAvatarRim();require(!m.hasExtraRim(picked),"xr_menu_no_shell");
  require(historySteps==steps+(shell?2:0),"xr_menu_no_shell");
  require(settlements==settled+2,"xr_menu_settles_strength");
  LLFloaterDirector d;ALKitShell host;d.mShell=shell?&host:nullptr;d.mExtraRimMenuPick=std::make_shared<LLUUID>(picked);
  d.onCastExtraRimToggle();require(m.hasExtraRim(picked),"xr_menu_no_shell");
  d.onCastExtraRimToggle();require(!m.hasExtraRim(picked),"xr_menu_no_shell");
 }
 // Avatar, clone runtime->instance and a retained clone id rekeyed again.
 LLDirectorCast::aliases[id(20)]=id(2);m.mBorrowRekeys[id(3)]=id(2);
 for(LLUUID actor:{id(1),id(20),id(3)}){
  auto stamp=m.mConfigurationRevision;require(m.addExtraRim(actor),"xr_clone_identity");
  require(m.mConfigurationRevision>stamp,"xr_scene_dirty_stamp");
  require(m.hasExtraRim(actor) && !m.canAddExtraRim(actor),"xr_clone_identity");
  const LLSD before=m.captureExtraRims();stamp=m.mConfigurationRevision;
  m.setExtraRimTrim(actor,.5f);require(m.mConfigurationRevision>stamp,"xr_scene_dirty_stamp");
  LLSD after=m.captureExtraRims();stamp=m.mConfigurationRevision;
  m.restoreExtraRims(before);require(m.mConfigurationRevision>stamp,"xr_scene_dirty_stamp");
  stamp=m.mConfigurationRevision;m.restoreExtraRims(after);require(m.mConfigurationRevision>stamp,"xr_scene_dirty_stamp");
  stamp=m.mConfigurationRevision;m.removeExtraRim(actor);
  require(!m.hasExtraRim(actor) && m.mExtraRims.empty(),"xr_clone_identity");
  require(m.mConfigurationRevision>stamp,"xr_scene_dirty_stamp");
  m.restoreExtraRims(before);require(m.hasExtraRim(actor),"xr_clone_identity");m.removeExtraRim(actor);
 }
 auto clean=m.mConfigurationRevision;
 ALCineLightRigManager::ExtraRim transient;
 m.setExtraRimState(transient,ALCineLightRigManager::ExtraRimState::ACTIVE);
 m.setExtraRimState(transient,ALCineLightRigManager::ExtraRimState::AWAY);
 require(m.mConfigurationRevision==clean,"xr_transient_clean");
 auto stamp=m.mConfigurationRevision;m.setExtraRimSource(2);
 require(m.mConfigurationRevision>stamp,"xr_scene_dirty_stamp");
 stamp=m.mConfigurationRevision;m.setExtraRimSource(2);m.removeExtraRim(id(99));
 require(m.mConfigurationRevision==stamp,"xr_scene_noop_clean");
 std::cout<<"PASS: production dirty stamps, clone identities and optional-shell menu mutations\n";
}
"""
    # Transient status/lighting must never touch the scene revision.
    assert 'mConfigurationRevision' not in function(manager,'void ALCineLightRigManager::setExtraRimState('), 'xr_transient_clean'
    assert 'mConfigurationRevision' not in function(manager,'void ALCineLightRigManager::updateExtraRims('), 'xr_transient_clean'
    flags,libs=viewer_native_flags();directory=check_directory('extra_rims_round2')
    mutations={
      'xr_transient_clean': ('    extra.mState = state;', '    extra.mState = state; ++mConfigurationRevision;'),
      'xr_scene_dirty_stamp':('++mExtraRimRevision; ++mConfigurationRevision;', '++mExtraRimRevision;'),
      'xr_clone_identity':('const LLUUID id = resolveRigActorIdentity(actor);','const LLUUID id = actor;'),
      'xr_menu_no_shell':('    if (shell) shell->ensurePageBuilt("light_group/light_rim_tab");','    if (!shell) return;\n    shell->ensurePageBuilt("light_group/light_rim_tab");'),
      'xr_menu_settles_strength':('    ALExtraRimUI::finishPendingTrims();',''),
    }
    for name,src in [('baseline',source)]+[(name,source.replace(old,new,1)) for name,(old,new) in mutations.items()]:
        if name != 'baseline':
            assert mutations[name][0] in source, (name, 'mutation anchor drift')
        result=compile_run(directory,src,name,flags,libs);output=result.stdout+result.stderr
        (directory/(name+'.log')).write_bytes(output.encode('utf8'))
        if name=='baseline':assert result.returncode==0,output
        else:
            assert result.returncode!=0 and name in output.splitlines(),(name,output)
        print('PASS: '+('production round 2 fixtures' if name=='baseline' else 'rejected planted '+name+' at its named assertion'),flush=True)

if __name__=='__main__':
    run_unit()
