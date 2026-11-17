"""Execute the production formation tests and Clone Studio fade/solo transitions without a viewer build."""
from pathlib import Path
import os
import subprocess
from relight_area_policy_check import method


def compile_run(directory, name, fixture, includes=()):
    directory.mkdir(parents=True, exist_ok=True)
    source = directory / (name + '.cpp')
    source.write_text(fixture, encoding='utf-8')
    vcvars = next((Path(os.environ['ProgramFiles']) / 'Microsoft Visual Studio').glob('*/Community/VC/Auxiliary/Build/vcvars64.bat'))
    command = directory / (name + '.cmd')
    flags = ' '.join('/I"' + str(p) + '"' for p in includes)
    command.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /std:c++17 /EHsc /fp:strict {flags} /Fo"{directory / (name + ".obj")}" /Fe"{directory / (name + ".exe")}" "{source}"\nexit /b %ERRORLEVEL%\n', encoding='utf-8')
    subprocess.run(['cmd', '/d', '/c', str(command)], check=True)
    subprocess.run([str(directory / (name + '.exe'))], check=True)


def check():
    nv = Path('indra/newview')
    directory = Path('.tmp/cloneqol_20261102/contract').resolve()
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'stdtypes.h').write_text('#pragma once\n#include <cstdint>\nusing U8=uint8_t;using U32=uint32_t;using U64=uint64_t;using F64=double;using F32=float;\n')
    tut = '''
#include <stdexcept>
#include <iostream>
namespace tut {
inline void ensure(const std::string& label, bool condition) { if(!condition) throw std::runtime_error(label); }
template<class Data> struct test_group {
 struct object : Data { template<int N> void test(); };
 test_group(const char*){}
};
}
'''
    solver = (nv / 'alformationsolver.cpp').read_text(encoding='utf-8').replace('#include "linden_common.h"', '')
    tests = (nv / 'tests/alformationsolver_test.cpp').read_text(encoding='utf-8').replace('#include "linden_common.h"', '').replace('#include "../test/lltut.h"', tut).replace('../alformationsolver.h', 'alformationsolver.h').replace('../alcloneqol.h', 'alcloneqol.h')
    main = '\nint main(){try {tut::formation_solver_object fixture;\n' + ''.join(f'fixture.test<{n}>();\n' for n in range(21,25)) + 'std::cout<<"PASS: production solver tests 21-24, heading-relative ripple, real rows, seeded offsets/mix, fade math and solo composition\\n";}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}\n'
    compile_run(directory, 'solver_qol', solver + '\n' + tests + main, (directory, nv.resolve()))
    fixture = r'''
#include "alcloneqol.h"
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
using F32=float;using F64=double;using U64=unsigned long long;
struct LLUUID {int id=0;bool isNull()const{return !id;}bool notNull()const{return id;}
 bool operator==(LLUUID b)const{return id==b.id;}bool operator!=(LLUUID b)const{return id!=b.id;}bool operator<(LLUUID b)const{return id<b.id;}
 static const LLUUID null;};const LLUUID LLUUID::null{};
template<class T>T llclamp(T x,T lo,T hi){return std::clamp(x,lo,hi);}
struct LLTimer{static double now;static double getTotalSeconds(){return now;}};double LLTimer::now=10.;
struct Region{LLUUID getRegionID(){return {50};}}region;
struct LLAgent {enum{TELEPORT_NONE,TELEPORT_START};int teleport=0;Region* getRegion(){return &region;}int getTeleportState(){return teleport;}}gAgent;
struct Settings{float duration=2.f;}gSavedSettings;
template<class T>struct LLCachedControl{Settings& s;LLCachedControl(Settings& settings,const char*,T):s(settings){}operator T()const{return T(s.duration);}};
struct LLDirectorCast{int touches=0;std::map<LLUUID,float> alpha;
 LLUUID active;LLUUID getActiveActorId(){return active;}void setActiveActorId(LLUUID id){active=id;}
 static LLDirectorCast& instance(){static LLDirectorCast s;return s;}
 void setCloneFadeAlpha(LLUUID id,float a){alpha[id]=a;}void forgetCloneFade(LLUUID){}   // the mock has no non-member map: only a member carries alpha here
 void touchCast(){++touches;}};
struct LLGhostAvatar{bool visible=true;};
namespace ALCloneTransition{enum Reason{R_COMPLETE,R_CANCEL,R_SWAP_SOURCE,R_FADE,R_DELETE,R_GROUP_DELETE,R_RECOVERY,R_SCENE_LOAD,R_SHOW_CLONES_OFF,R_SOLO,R_EDIT_LOCK};}
struct ALCloneTransitions{static void cancelFor(LLUUID,ALCloneTransition::Reason){}static void cancelAllRunning(ALCloneTransition::Reason){}static bool anyRunning(){return false;}};   // clone transitions (11/16): the Studio hooks; no transition runs in this fixture
void set_entity_clone_visible(LLGhostAvatar* ghost,bool visible,bool=false){ghost->visible=visible;}
struct ALGhostStudio{
 enum{BACKING_OVERLAY,BACKING_ENTITY_CLONE,STATE_READY,STATE_TEARING_DOWN};
 struct Instance{LLUUID mId,mEntityId,mGroupId;int mKind=BACKING_ENTITY_CLONE,mState=STATE_READY;
  bool mDeleting=false,mEnabled=true,mSoloVisible=true,mUserHidden=false;float mFadeAlpha=1.f;};
 struct Runtime{ALCloneQol::Fade mFade;};std::map<LLUUID,Runtime>mFades;
 std::vector<Instance>mInstances;std::map<LLUUID,LLGhostAvatar>ghosts;
 LLUUID mSelected,mFadeRegion{50};bool mSolo=false,mShowAll=true;U64 mInstanceRevision=0;int finalDeletes=0;
 Instance* getInstance(LLUUID id)const{for(auto& i:const_cast<ALGhostStudio*>(this)->mInstances)if(i.mId==id)return &i;return nullptr;}
 const Instance* findInstanceByRuntime(LLUUID id)const{for(auto& i:mInstances)if(i.mEntityId==id)return &i;return nullptr;}
 LLGhostAvatar* resolveEntityClone(LLUUID id){auto i=ghosts.find(id);return i==ghosts.end()?nullptr:&i->second;}
 std::vector<LLUUID> groupMembers(LLUUID id){std::vector<LLUUID> result;auto* i=getInstance(id);if(!i)return result;
  for(auto& other:mInstances)if(other.mId==id||(i->mGroupId.notNull()&&i->mGroupId==other.mGroupId))result.push_back(other.mId);return result;}
 struct Member {LLUUID mInstanceId;};struct Group {std::vector<Member>mMembers;};
 const Group* groupForMember(LLUUID)const{return nullptr;}
 bool isVisible(const Instance& i)const{return ALCloneQol::visible(mShowAll,i.mEnabled,mSolo,i.mSoloVisible);}
 bool entityRenderOnly(const Instance& i)const{return mShowAll&&i.mEnabled&&mSolo&&!i.mSoloVisible;}   // the production body adds the overlay-look term; no overlay looks here
 void setSelected(LLUUID id){mSelected=id;}
 void removeInstance(LLUUID id){if(getInstance(id)){++finalDeletes;mInstances.erase(std::remove_if(mInstances.begin(),mInstances.end(),[&](const Instance& i){return i.mId==id;}),mInstances.end());}}
 LLUUID actorOwner(const LLUUID& id)const{return id;}
 bool isDeleting(const LLUUID&)const;void refreshSoloVisibility();void setSolo(const std::vector<LLUUID>&);
 void requestRemoveInstance(const LLUUID&);void updateFades();
};
'''
    for signature in ('bool ALGhostStudio::isDeleting(', 'void ALGhostStudio::refreshSoloVisibility(', 'void ALGhostStudio::setSolo(', 'void ALGhostStudio::requestRemoveInstance(', 'void ALGhostStudio::updateFades('):
        fixture += method(nv / 'alghoststudio.cpp', signature) + '\n'
    fixture += r'''
int main(){ALGhostStudio studio;
 studio.mInstances={{{1},{101},{10}},{{2},{102},{10}},{{3},{103},{}}};
 studio.ghosts={{{1},{}},{{2},{}},{{3},{}}};studio.mInstances[2].mEnabled=false;
 studio.setSolo({{1}});assert(studio.mSolo&&studio.mInstances[0].mSoloVisible&&studio.mInstances[1].mSoloVisible&&!studio.mInstances[2].mSoloVisible);
 assert(!studio.ghosts[{3}].visible);studio.setSolo({});
 assert(studio.ghosts[{1}].visible&&studio.ghosts[{2}].visible&&!studio.ghosts[{3}].visible);
 assert(!studio.mInstances[2].mEnabled); // prior hidden state survives exactly
 studio.mSelected={1};studio.requestRemoveInstance({1});
 assert(studio.isDeleting({1})&&studio.isDeleting({101})&&studio.isDeleting({2}));
 assert(studio.mSelected.isNull()&&studio.finalDeletes==0&&studio.mInstances.size()==3);
 LLTimer::now=11.;studio.updateFades();assert(studio.finalDeletes==0&&studio.mInstances[0].mFadeAlpha==.5f);
 LLTimer::now=12.;studio.updateFades();assert(studio.finalDeletes==2&&studio.mInstances.size()==1);
 // Teleport bypasses an in-flight removal immediately.
 studio.mSelected={3};studio.requestRemoveInstance({3});gAgent.teleport=LLAgent::TELEPORT_START;
 studio.updateFades();assert(studio.finalDeletes==3&&studio.mInstances.empty());
 // Zero-duration remains the old immediate deletion path.
 studio.mInstances={{{4},{104},{}}};gSavedSettings.duration=0.f;studio.requestRemoveInstance({4});
 assert(studio.finalDeletes==4&&studio.mInstances.empty());
 std::cout<<"PASS: production solo group expansion/visibility restore; retiring selection/identity state; deferred group completion; teleport and zero-duration immediate paths\n";}
'''
    compile_run(directory, 'studio_qol', fixture, (nv.resolve(),))
    check_round1(directory, nv)



def check_round1(directory, nv):
    fixture = Path('scripts/code_tools/fixtures/clone_qol_fixture.cpp.in').read_text(encoding='utf-8')
    policy = '\n'.join(method(nv / 'alghoststudio.h', sig) for sig in (
        'static S32 remapRetiredOverlayStyle(', 'static bool isOverlayStyle(',
        'static S32 resolveOverlayStyle('))
    fixture = fixture.replace('@OVERLAY_STYLE_POLICY@', policy)
    production = []
    for file, signatures in {
        'alghoststudio.cpp': ('static F32 clone_capture_radius(', 'bool ALGhostStudio::isDeleting(', 'bool ALGhostStudio::refreshPoseCadence(', 'void ALGhostStudio::getWantedSources(', 'LLUUID ALGhostStudio::actorOwner(',
            'bool ALGhostStudio::isPrivateOverlayRuntime(', 'void ALGhostStudio::rememberOverlayDomain(', 'void ALGhostStudio::invalidateOverlayPose(',
            'void ALGhostStudio::releaseOverlayHelper(', 'void ALGhostStudio::releaseOverlayReference(', 'bool ALGhostStudio::acquireOverlayHelper(', 'LLUUID ALGhostStudio::reserveOverlayHelper(',
            'void ALGhostStudio::beginAppearance(', 'bool ALGhostStudio::swapSource(',
            'bool ALGhostStudio::setStartOffset(', 'void ALGhostStudio::syncOffsets(',
            'void ALGhostStudio::applyCrowdVariation(', 'void ALGhostStudio::refreshOverlayOffset(',
            'LLVOAvatar* ALGhostStudio::resolveOverlayAvatar(', 'bool ALGhostStudio::freezeInstance(', 'void ALGhostStudio::unfreezeInstance('),
        'pipeline.cpp': ('bool uploadGhostPalette(',),
        'llactormover.cpp': ('void LLActorMover::computeProxyBounds(',),
        'llghostavatar.cpp': ('void LLGhostAvatar::setRenderOnlyHidden(', 'bool LLGhostAvatar::isRenderHidden(const LLViewerObject* avatar,', 'bool LLGhostAvatar::isRenderHidden(const LLViewerObject* object)', 'F32 LLGhostAvatar::phaseOffset(', 'void LLGhostAvatar::setStartOffset(',
            'void LLGhostAvatar::applyOffsetHistory(', 'void LLGhostAvatar::restoreOffsetPose(',
            'void LLGhostAvatar::prepareOffsetDrive('),
        'lldirectorcast.cpp': ('const LLDirectorCast::ActorStyle& LLDirectorCast::getActorStyle(',
            'const LLDirectorCast::ActorStyle& LLDirectorCast::getRenderActorStyle(',
            'void LLDirectorCast::setActorStyle(', 'void LLDirectorCast::setCloneFadeAlpha(',
            'std::vector<LLDirectorCast::ActorChoice> LLDirectorCast::buildActorChoices('),
    }.items():
        production.extend(method(nv / file, sig) for sig in signatures)
    production.insert(0,method(nv / 'alghoststudio.cpp', 'void set_entity_clone_visible('))
    merger = (nv / 'aldirectoractorchoice.cpp').read_text(encoding='utf-8')
    merger = '\n'.join(line for line in merger.splitlines() if not line.startswith('#include "'))
    fixture = fixture.replace('@PRODUCTION@', merger+'\n'+'\n'.join(production))
    compile_run(directory, 'round1_qol', fixture, (nv.resolve(),))
    # Negative controls mutate only assembled fixture text, never viewer files.
    controls = {
        'crowd_early_return': ('void ALGhostStudio::applyCrowdVariation', '\n    const Instance* member =', '\n    return;\n    const Instance* member ='),
        'skip_history_apply': ('void LLGhostAvatar::applyOffsetHistory', '\n    if (mStartOffset', '\n    return;\n    if (mStartOffset'),
        'skip_overlay_history': ('void ALGhostStudio::refreshOverlayOffset', 'inst.mOffsetPose = frame.second;', '(void)frame;'),
        'swap_early_return': ('bool ALGhostStudio::swapSource', '\n    Instance* inst =', '\n    return false;\n    Instance* inst ='),
        'held_bounds_live_extents': ('void LLActorMover::computeProxyBounds', 'proxy.mHeldBounds ? proxy.mHeldBounds : av->getLastAnimExtents()', 'av->getLastAnimExtents()'),
        'swap_failure_loses_prior': ('bool ALGhostStudio::swapSource', 'if (replacement) {', 'releaseOverlayHelper(id); if (replacement) {'),
        'inactive_policy_resolves_owners': ('bool LLGhostAvatar::isRenderHidden(const LLViewerObject* avatar,', 'if (!hasRenderHiddenPolicy()) return false;', ''),
        'resume_discards_clock_shift': ('void ALGhostStudio::unfreezeInstance', 'frame.first += paused;', 'frame.first += 0.;'),
        'drop_subject_filter': ('std::vector<LLDirectorCast::ActorChoice> LLDirectorCast::buildActorChoices', 'return ALGhostStudio::instance().isDeleting(choice.mId)', 'return false && ALGhostStudio::instance().isDeleting(choice.mId)'),
        'fade_authoring_leak': ('const LLDirectorCast::ActorStyle& LLDirectorCast::getActorStyle', 'return member.mActorStyle;', 'return member.mCloneFading ? member.mCloneFadeStyle : member.mActorStyle;'),
    }
    for name, (signature, old, new) in controls.items():
        start = fixture.index(signature if '(' in signature else signature+'(')
        stop = fixture.index('\n}', start)+2
        body = fixture[start:stop]
        assert old in body, name
        broken = fixture[:start]+body.replace(old,new,1)+fixture[stop:]
        try:
            compile_run(directory, name, broken, (nv.resolve(),))
        except subprocess.CalledProcessError as failure:
            assert failure.cmd[0].endswith('.exe'), (name, 'negative control must compile')
            print('PASS: rejected negative control', name, flush=True)
        else:
            raise AssertionError('negative control escaped: '+name)
    assert 'for (' not in method(nv/'alghoststudio.cpp','ALGhostStudio::Instance* ALGhostStudio::getInstance(')
    assert 'for (' not in method(nv/'alghoststudio.cpp','LLGhostAvatar* ALGhostStudio::resolveEntityClone(')
    # Wiring checks complement execution at viewer-only boundaries.
    setup = method(nv/'alghoststudio.cpp', 'void ALGhostStudio::applyEntityRuntimeState(')
    assert 'resolve(inst.mAnimationSource)' in setup and 'animation->getID()' in setup
    for signature in ('bool LLActorMover::canRenderSystemActorGhost(', 'bool LLActorMover::actorMaskAvatarEligible(',
                      'bool LLActorMover::renderSharedActorStyleProxyItem('):
        assert 'isRenderHidden' in method(nv/'llactormover.cpp',signature)
    assert 'LLGhostAvatar::isRenderHidden(avatar)' in method(nv/'llactormover.cpp','void LLActorMover::buildSharedActorStyleQueue(')
    # Viewer-only consumer wiring complements the executed production bounds and history.
    queue = method(nv/'llactormover.cpp','void LLActorMover::buildGhostDeferredQueue(')
    assert 'proxy.mHeldBounds = inst.mOffsetPose->mFrozenBounds' in queue
    capture = method(nv/'alghoststudio.cpp','bool ALGhostStudio::captureOverlayPose(')
    assert 'getLastAnimExtents()' in capture and 'mFrozenBounds[0] = ext[0]' in capture
    cover = method(nv/'pipeline.cpp','bool drawSilCoverBatch(LLDrawInfo& di, const SilCoverPass& pass,\n                       const LLVector2& cover, bool blended,\n                       bool apply_model_matrix)')
    assert 'uploadGhostPalette(di)' in cover and 'sDrawingGhostProxy->mFadeAlpha' in cover
    replay = method(nv/'pipeline.cpp','void LLPipeline::renderGhostSilCover(')
    assert 'proxy.mHeldAttachMats->find(gf.mObjectId)' in replay and '1.f-proxy.mFadeAlpha' in replay
    setter = method(nv/'llghostavatar.cpp','void LLGhostAvatar::setEntityCloneVisible(')
    assert 'if (visible) --sRenderHiddenReasons; else ++sRenderHiddenReasons;' in setter
    directed=(nv/'llghostavatar.cpp').read_text(encoding='utf-8')
    retrigger=directed[directed.index('        if (!active &&'):directed.index('        mEntityDirectedWasActive = active;')]
    assert 'startMotion(mEntityDirectedAnim, 0.f)' in retrigger and 'phaseOffset(' not in retrigger


if __name__ == '__main__':
    check()
