"""Round 3: same-UUID body replacement and replay-finalizer boundaries."""
from pathlib import Path
from native_check_tools import check_directory, compile_run, viewer_native_flags
from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]


def run_unit(root=ROOT):
    manager = (root/'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    header = (root/'indra/newview/alcinelightrigmanager.h').read_text(encoding='utf8')
    page = (root/'indra/newview/alkitpage.cpp').read_text(encoding='utf8')
    history = (root/'indra/newview/alkithistory.cpp').read_text(encoding='utf8')
    ui = (root/'indra/newview/alpanelrigrim.cpp').read_text(encoding='utf8')

    def replay_contract(page_text, history_text):
        block = function(page_text, 'void ALKitPage::undoOnPage(')
        assert 0 <= block.find('ALExtraRimUI::finishPendingTrims();') < block.index('stepSequence('), 'xr_page_settles_strength'
        block = function(history_text, 'bool ALKitHistory::prepareReplay(')
        assert 0 <= block.find('ALExtraRimUI::finishPendingTrims();') < block.index('ScopedSuspend guard;'), 'xr_prevalidate_settles_strength'
        assert 'for (auto* editor : extraRimEditors) editor->finishTrim(true);' in function(ui, 'void ALExtraRimUI::finishPendingTrims('), 'xr_registered_finalizers_only'
    replay_contract(page, history)
    for name, p, h in (
        ('xr_page_settles_strength', page.replace('    ALExtraRimUI::finishPendingTrims();', '', 1), history),
        ('xr_prevalidate_settles_strength', page, history.replace('    ALExtraRimUI::finishPendingTrims();', '', 1))):
        try:
            replay_contract(p, h)
        except AssertionError as error:
            assert str(error) == name, error
        else:
            raise AssertionError(name+' escaped')
        print('PASS: rejected planted '+name+' at its named source assertion', flush=True)
    source = r'''
#include "linden_common.h"
#include "alextrarimmodel.h"
#include "llpointer.h"
#include "llrefcount.h"
#include <memory>
#include <vector>
#include <iostream>
#include <cstdlib>
struct LLVOAvatar : LLRefCount {
 LLUUID id, owner; bool dead=false;
 bool isDead()const{return dead;}
 LLUUID getID()const{if(dead)std::abort();return id;}
 LLUUID getActorFxOwnerId()const{if(dead)std::abort();return owner;}
 bool isControlAvatar()const{return false;}
 LLVOAvatar* getAttachedAvatar()const{return nullptr;}
};
struct LLDirectorCast {
 LLVOAvatar* body=nullptr;
 static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
 LLVOAvatar* resolve(LLUUID,bool){return body && !body->isDead()?body:nullptr;}
};
int gSavedSettings=0;
template<class T>struct LLCachedControl {T value;LLCachedControl(int,const char*,T v):value(v){} operator T()const{return value;}};
struct Emitter {bool isDead()const{return false;} bool getIsLight()const{return true;}};
struct ALCineLightRig {
 enum class ExtraRimResult{ACTIVE,WAITING,AWAY,GEOMETRY,EMITTER_UNAVAILABLE};
 LLPointer<LLRefCount> unused;
 struct Projector {Emitter value;bool notNull()const{return true;} Emitter* operator->()const{return const_cast<Emitter*>(&value);}} mProjectors[4];
 LLUUID projectorId(int)const{return LLUUID("00000000-0000-0000-0000-000000000008");}
 ExtraRimResult extraRimFrom(const ALCineLightRig&,LLVOAvatar*,F32,F64){return ExtraRimResult::ACTIVE;}
 void shutdown(){}
};
struct ALCineLightRigManager {
 using Slot=int;
 enum class ExtraRimState : U8 {WAITING,ACTIVE,SOURCE_OFF,BACK_OFF,AWAY,OWN_RIG,MASK_DISABLED,MASK_OVERFLOW,GEOMETRY,EMITTER_UNAVAILABLE};
 EXTRA
 std::vector<ExtraRim> mExtraRims; int mExtraRimSource=0;
 bool mExtraRimRegistryDirty=false,mExtraRimRegistryOnly=true;
 int mActorOnlyObjectSlots=0;std::vector<int> mMirrors;
 ALCineLightRig source;
 const ALCineLightRig& at(Slot)const{return source;}
 bool isSlotLit(Slot)const{return true;}
 bool hasOwnLitRig(LLUUID)const{return false;}
 void pruneRemovedExtraRims(){}
 void setExtraRimState(ExtraRim& extra,ExtraRimState state){extra.mState=state;}
 void updateExtraRims(F64);
};
'''
    extra = header[header.index('    struct ExtraRim\n'):header.index('    void updateExtraRims(', header.index('    struct ExtraRim\n'))]
    source = source.replace(' EXTRA',extra)
    source += function(manager,'void ALCineLightRigManager::updateExtraRims(') + r'''
void require(bool ok,const char* name){if(!ok){std::cerr<<name<<"\n";std::exit(1);}}
int main(){
 ALCineLightRigManager m;m.mExtraRims.emplace_back();
 LLPointer<LLVOAvatar> first=new LLVOAvatar;
 first->id=first->owner=LLUUID("00000000-0000-0000-0000-000000000001");
 LLDirectorCast::instance().body=first.get();m.updateExtraRims(1);
 require(m.mExtraRimRegistryDirty,"xr_body_initial_binding");
 LLVOAvatar* registryBody=first.get();
 m.mExtraRims.front().mState=ALCineLightRigManager::ExtraRimState::ACTIVE; // registry assigned its bit
 m.mExtraRimRegistryDirty=false;
 m.updateExtraRims(2);require(!m.mExtraRimRegistryDirty,"xr_body_steady_no_rebuild");
 LLPointer<LLVOAvatar> second=new LLVOAvatar;
 second->id=first->id;second->owner=first->owner;
 first->dead=true;first=nullptr; // cache is the sole safe reference to the dead instance
 LLDirectorCast::instance().body=second.get();m.updateExtraRims(3); // no away tick
 require(m.mExtraRimRegistryDirty,"xr_same_uuid_body_replacement");
 if(m.mExtraRimRegistryDirty)registryBody=LLDirectorCast::instance().body;
 require(registryBody==second.get(),"xr_same_uuid_body_replacement");
 m.mExtraRimRegistryDirty=false;m.updateExtraRims(4);
 require(!m.mExtraRimRegistryDirty,"xr_body_steady_no_rebuild");
 std::cout<<"PASS: same UUID/owner replacement invalidates without an away tick or dead-body dereference; stable body keeps cache\n";
}
'''
    flags, libs = viewer_native_flags()
    directory = check_directory('extra_rims_round3')
    old = 'extra.mRecipientBody.get() != avatar || '
    assert old in source
    for name, src in [('baseline',source), ('xr_same_uuid_body_replacement',source.replace(old,'',1))]:
        result = compile_run(directory,src,name,flags,libs)
        output=result.stdout+result.stderr
        (directory/(name+'.log')).write_bytes(output.encode('utf8'))
        if name=='baseline':
            assert result.returncode==0,output
            print(result.stdout,end='',flush=True)
        else:
            assert result.returncode!=0 and name in output.splitlines(),output
            print('PASS: rejected planted '+name+' at its named assertion',flush=True)


if __name__=='__main__':
    run_unit()
