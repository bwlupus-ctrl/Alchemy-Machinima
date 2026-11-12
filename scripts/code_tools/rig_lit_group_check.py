"""Production named-group gather/cache/aggregate path, run by rig_sway_check."""
from rig_shadow_presets_check import function


def fixture(root):
    source = (root / 'indra/newview/alcinelightrig.cpp').read_text(encoding='utf8')
    # Renderer/joint services are adapters. The cache, resolver, legacy gather,
    # named gather and aggregate body below are the current worktree's code.
    support = r'''
#include <set>
#include <memory>
#ifndef LL_INFOS
#define LL_INFOS(...) std::cerr
#endif
#ifndef LL_WARNS
#define LL_WARNS(...) std::cerr
#endif
namespace RigRegistryRound2 {void test();}
namespace RigLitGroupModel {
enum {VX,VY,VZ};
const LLUUID gAgentID{"self"};
struct LLTimer { static inline F64 now=1.;static F64 getTotalSeconds(){return now;} };
struct LLVector3 {
    F32 mV[3]={}; void set(F32 x,F32 y,F32 z){mV[0]=x;mV[1]=y;mV[2]=z;}
};
struct LLVOAvatar {
    LLUUID id; F32 x=0; bool dead=false;
    LLVOAvatar* asAvatar(){return this;}
    const LLUUID& getID() const {return id;}
    bool isDead() const {return dead;}
    F32 getUniformScale() const {return 1.f;}
};
struct LLDirectorCast {
    struct GroupEvent {enum Type{MEMBERS,RENAMED,DISSOLVED}type;std::string oldName,newName;};
    std::map<LLUUID,LLVOAvatar> avatars;
    std::map<LLUUID,LLUUID> runtime_ids;
    std::map<LLUUID,int> calls;
    uuid_vec_t ids; U64 cast_revision=0, group_revision=0; int scans=0, name_work=0, resolves=0;
    static LLDirectorCast& instance(){static LLDirectorCast cast;return cast;}
    U64 getCastRevision()const{return cast_revision;}
    U64 getGroupRevision()const{return group_revision;}
    uuid_vec_t membersInGroup(const std::string& name){++scans;return name=="guards"?ids:uuid_vec_t();}
    static bool isSelf(const LLUUID& id){return id.isNull()||id==gAgentID;}
    static LLUUID canonicalActorId(const LLUUID& id){return id;}
    LLVOAvatar* resolve(const LLUUID& id,bool refresh=true){++resolves;++calls[id];name_work+=refresh;auto key=isSelf(id)?gAgentID:id;
        auto mapped=runtime_ids.find(key);if(mapped!=runtime_ids.end())key=mapped->second;
        auto it=avatars.find(key);return it==avatars.end()?nullptr:&it->second;}
    LLUUID subjects[4]={{"1"},{"2"},{"3"},{"4"}};
    LLUUID getSubjectA() const{return subjects[0];}LLUUID getSubjectB() const{return subjects[1];}
    LLUUID getSubjectC() const{return subjects[2];}LLUUID getSubjectD() const{return subjects[3];}
    LLVOAvatar* resolveSubjectA(){return resolve({"1"});}
    LLVOAvatar* resolveSubjectB(){return resolve({"2"});}
    LLVOAvatar* resolveSubjectC(){return resolve({"3"});}
    LLVOAvatar* resolveSubjectD(){return resolve({"4"});}
};
struct ObjectList {
    LLVOAvatar* findObject(const LLUUID& id){auto& avatars=LLDirectorCast::instance().avatars;auto it=avatars.find(id);return it==avatars.end()?nullptr:&it->second;}
} gObjectList;
struct ALCineLightRig {
    static constexpr U32 GROUP_SLOT_SELF=1,GROUP_SLOT_A=2,GROUP_SLOT_B=4,GROUP_SLOT_C=8,GROUP_SLOT_D=16;
    LLUUID object;
    const LLUUID& getObjectTarget() const {return object;}
    const std::string& getLitGroup() const {return mLitGroup;}
    const uuid_vec_t& lastResolvedMembers() const {return mLastResolvedMembers;}
    bool isGroupEnabled() const {return mGroupEnabled;}
    U32 lastResolvedGroupSlots() const {return mLastResolvedGroupSlots;}
    void renameLitGroup(const std::string&,const std::string&);
    std::string mLitGroup="guards";
    uuid_vec_t mLitGroupMembers,mLitRuntimeMembers,mLastResolvedMembers,mLastResolvedStoredMembers;
    LLUUID mGroupRepresentative, mAnchor;
    bool mLitMembersDirty=true; S32 mLitGroupPresent=0;
    F64 mLitMemberRetryAt=0;
    U64 mLitCastRevision=0,mLitGroupRevision=0;
    S32 mLitGroupSize=0; bool mLitGroupFallback=false,mLitGroupWide=false,mGroupEnabled=false;
    U32 mGroupSlots=0,mLastResolvedGroupSlots=0;
    bool mHaveSmoothedCentre=true; F32 mSmoothedScale=1;
    int mProjectorRetryTicks=0,mOmniRetryTicks=0,mCatchlightRetryTicks=0,mLastFrame=0;
    F64 mLastPresentationTime=0;
    bool destroyed=false; void destroyEmitters(){destroyed=true;}
    ALCineLightRigSlot mSlot=ALCineLightRigSlot::SELF;
    LLVOAvatar* resolveSlotAvatar() const;
    LLUUID representative;F32 centre=0,scale=1;int count=0;
    void tick(bool object_targeted=false);
    void tickDark(bool object_targeted=false);
    void tickStrings(bool object_targeted=false);
    void tickSnap(bool object_targeted=false);
    F32 mLiveProbeBounceScale=1;bool mHaveTarget=false,mTransitionActive=false;
    int mActiveFX=-1,mPendingFXId=-1; F64 mPendingFXPhase=-1;
    void shutdown();void shutdownStale();
};
bool memberTrackPoint(LLVOAvatar* avatar,S32,F32,LLVector3& point){point.set(avatar->x,0,0);return true;}
'''
    begin = source.index('    LLVOAvatar* group_members[LIT_GROUP_MAX] = {};')
    end = source.index('\n    if (!object_targeted)\n', source.index('    if (group_target && group_count > 0)', begin))
    body = source[begin:end]
    support += r'''
using LLViewerObject=LLVOAvatar;

struct ALCineLightRigManager {
    using Slot=S32;ALCineLightRig rig;bool enabled=true;
    bool isSlotEnabled(Slot)const{return enabled;}
    const ALCineLightRig& at(Slot)const{return rig;}
    bool isSlotLit(Slot)const;
    static ALCineLightRigManager& instance(){static ALCineLightRigManager m;return m;}
    LLUUID anchors[5];
    LLUUID groupSlotActor(ALCineLightRigSlot) const;
    LLUUID slotActor(ALCineLightRigSlot slot) const {
        const int i=static_cast<int>(slot);
        return anchors[i].notNull()?anchors[i]:i==0?gAgentID:LLDirectorCast::instance().subjects[i-1];
    }
};
'''
    result = [support + function(source, 'LLVOAvatar* ALCineLightRig::resolveSlotAvatar()'), function(source, 'S32 gatherGroupMembers('), function(source, 'S32 gatherMembers('),
              'void ALCineLightRig::tick(bool object_targeted){'
              'Setup setup; setup.mRadius=2.f; S32 track_mode_setting=0;'
              'bool scale_aware_setting=true; F64 presentation_time=1;\n' + body +
              '\nrepresentative=avatar?avatar->getID():LLUUID::null;count=group_count;centre=aggregate_group?aggregate_centre_agent.mV[0]:(avatar?avatar->x:-1.f);'
              'scale=aggregate_subject_scale;}']
    negative = result[-1].replace('::tick(', '::tickDark(', 1)
    old = '(group_target ? (group_count > 0 ? group_members[0] : nullptr) : resolveSlotAvatar())'
    assert old in negative
    negative = negative.replace(old, '(group_target ? (group_count > 0 ? group_members[0] : nullptr) : nullptr)')
    result.append(function(source, 'void ALCineLightRig::shutdown()'))
    result.append(result[-1].replace('::shutdown()', '::shutdownStale()').replace('    mLastResolvedMembers.clear();', '').replace('    mLitGroupFallback = false;', '').replace('    mLitGroupWide = false;', ''))
    result.append(negative)
    result.append(result[3].replace('::tick(', '::tickStrings(', 1).replace('    mLastResolvedMembers.clear();', '    cast.resolve(gAgentID); mLastResolvedMembers.clear();'))
    result.append(result[3].replace('::tick(', '::tickSnap(', 1).replace('== mGroupRepresentative', '== LLUUID::null'))
    manager = (root / 'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    begin = manager.index('            if (event.type != LLDirectorCast::GroupEvent::RENAMED) return;')
    end = manager.index('        });', begin)
    result += [function(source, 'void ALCineLightRig::renameLitGroup('),
               'struct Record {std::string mLitGroup="guards";};'
               'void applyEvent(ALCineLightRig (&mInstances)[5], Record (&mBlobs)[5],const LLDirectorCast::GroupEvent& event){'
               'const S32 SLOT_COUNT=5;' + manager[begin:end] + '}']
    result.append(function(manager, 'bool ALCineLightRigManager::isSlotLit('))
    result.append(function(manager, 'LLUUID ALCineLightRigManager::groupSlotActor(')
                  .replace('Slot slot', 'ALCineLightRigSlot slot').replace('SLOT_SELF', 'ALCineLightRigSlot::SELF'))
    result.append(function(source, 'S32 gatherGroupMembers(').replace('gatherGroupMembers(', 'gatherGroupMembersWrongSelf(', 1).replace(
        'manager.groupSlotActor', 'manager.slotActor', 1))
    registry = function(manager, 'void ALCineLightRigManager::rebuildActorOnlyRegistry()')
    start = registry.index('            if (!rig.getLitGroup().empty())')
    end = registry.index('            rigs.push_back(input);', start)
    recipients = ('uuid_vec_t recipients(const ALCineLightRig& rig){uuid_vec_t ids;'
                  'const S32 SLOT_COUNT=5;using Slot=ALCineLightRigSlot;'
                  'auto& manager=ALCineLightRigManager::instance();'
                  'auto groupSlotActor=[&](Slot slot){return manager.groupSlotActor(slot);};'
                  'auto slotActor=[&](Slot slot){return manager.slotActor(slot);};'
                  'auto add=[&](LLVOAvatar* avatar){if(avatar)ids.push_back(avatar->getID());};'
                  + registry[start:end] + 'return ids;}')
    result.append(recipients)
    result.append(recipients.replace('recipients(', 'recipientsWrongSelf(', 1)
                  .replace('const LLUUID actor = groupSlotActor(', 'const LLUUID actor = slotActor('))
    result.append(r'''
void test(){
    RigRegistryRound2::test();
    auto& cast=LLDirectorCast::instance();
    cast.avatars[gAgentID]={gAgentID,99};
    for(int i=0;i<20;++i){LLUUID id{std::to_string(i)};cast.ids.push_back(id);cast.avatars[id]={id,F32(i*2)};}
    ALCineLightRig rig;
    rig.tick();assert(rig.mLitGroupSize==20 && rig.count==16 && rig.centre==15 && rig.scale==8.5f && rig.mLitGroupWide);
    assert(rig.mLastResolvedMembers.size()==16 && rig.mLastResolvedGroupSlots==0);
    int scans=cast.scans, names=cast.name_work;rig.tick();assert(cast.scans==scans && cast.name_work==names);
    rig.tickStrings();assert(cast.name_work>names); // planted per-tick name work rejected
    cast.name_work=names;
    assert(rig.mLitGroupPresent==20);
    auto all=cast.avatars;
    for(int i=0;i<16;++i)cast.avatars.erase({std::to_string(i)});
    rig.tick();assert(rig.count==4 && !rig.mLitGroupFallback && rig.centre==35);
    cast.avatars=all;
    cast.ids.resize(6);++cast.group_revision;rig.tick();assert(rig.count==6 && rig.centre==5);
    cast.avatars.erase({"0"});rig.tick();assert(rig.count==5 && rig.centre==6 && cast.scans==scans+1);
    assert(rig.representative==LLUUID{"1"});
    cast.avatars[{"0"}]={{"0"},0};rig.tick();assert(rig.representative==LLUUID{"1"});
    rig.tickSnap();assert(rig.representative!=LLUUID{"1"}); // planted snap rejected
    cast.avatars.clear();cast.avatars[gAgentID]={gAgentID,99};rig.tick();
    const auto fallbackLit = [](const ALCineLightRig& r){return r.mLitGroupFallback && r.centre==99 && !r.destroyed;};
    assert(fallbackLit(rig));
    ALCineLightRig dark;dark.tickDark();
    assert(!fallbackLit(dark)); // same oracle rejects planted nobody-lit path
    cast.ids.clear();++cast.group_revision;rig.tick();assert(rig.mLitGroupFallback && rig.centre==99);
    rig.mLitGroup="dissolved";rig.mLitMembersDirty=true;rig.tick();assert(rig.centre==99 && rig.mLitGroup=="dissolved");
    rig.mLitGroup="guards";cast.ids={gAgentID,LLUUID::null,gAgentID};++cast.group_revision;
    rig.tick();assert(rig.count==1 && rig.mLastResolvedMembers.size()==1 && !rig.mLitGroupFallback);
    rig.tick(true);assert(rig.mLastResolvedMembers.empty() && !rig.mLitGroupFallback);
    ALCineLightRig rigs[5]; Record records[5];
    for(auto& r:rigs){r.mHaveSmoothedCentre=true;r.mLitGroupRevision=50;}
    applyEvent(rigs,records,{LLDirectorCast::GroupEvent::RENAMED,"guards","crew"});
    for(S32 i=0;i<5;++i){assert(rigs[i].mLitGroup=="crew" && records[i].mLitGroup=="crew");
        assert(rigs[i].mHaveSmoothedCentre && rigs[i].mLitMembersDirty);}
    applyEvent(rigs,records,{LLDirectorCast::GroupEvent::DISSOLVED,"crew",""});
    for(S32 i=0;i<5;++i)assert(rigs[i].mLitGroup=="crew" && records[i].mLitGroup=="crew");
    ALCineLightRigManager manager; manager.rig.mLastResolvedMembers={gAgentID};
    assert(manager.isSlotLit(0));manager.enabled=false;assert(!manager.isSlotLit(0));manager.enabled=true;
    manager.rig.mLastResolvedMembers.clear();assert(!manager.isSlotLit(0));
    cast.avatars[{"object"}]={{"object"},0};manager.rig.object={"object"};assert(manager.isSlotLit(0));
    rig.mLitGroup.clear();rig.mGroupEnabled=true;rig.mGroupSlots=1;rig.tick();
    assert(rig.mLastResolvedGroupSlots==1 && rig.count==1 && !rig.destroyed);
    cast.avatars[{"1"}]={{"1"},2};cast.avatars[{"3"}]={{"3"},6};
    rig.mGroupSlots=11;rig.tick();
    assert(rig.mLastResolvedGroupSlots==11 && rig.count==3 && rig.centre==50.5f);
    // Legacy SELF anchor affects its own rig, never the custom group's You bit.
    auto& owner=ALCineLightRigManager::instance();owner.anchors[0]={"1"};owner.anchors[1]={"3"};
    rig.mAnchor={"1"};rig.mGroupSlots=3;rig.tick();
    assert(rig.mLastResolvedMembers==uuid_vec_t({gAgentID,LLUUID{"3"}}));
    assert(rig.mLastResolvedGroupSlots==3 && rig.centre==52.5f);
    assert(recipients(rig)==uuid_vec_t({gAgentID,LLUUID{"3"}}));
    assert(recipientsWrongSelf(rig)!=recipients(rig));
    auto youAndExtra=[](auto gather){LLVOAvatar* members[GROUP_MAX_MEMBERS]={};U32 mask=0;
        return gather(3,members,mask)==2 && mask==3 && members[0]->getID()==gAgentID && members[1]->getID()==LLUUID{"3"};};
    assert(youAndExtra(gatherGroupMembers) && !youAndExtra(gatherGroupMembersWrongSelf));
    owner.anchors[0].setNull();owner.anchors[1].setNull();rig.mAnchor.setNull();
    rig.mLastResolvedMembers={gAgentID};rig.mLitGroupFallback=true;rig.mLitGroupWide=true;
    auto off=[](const ALCineLightRig& r){return r.mLastResolvedMembers.empty() && !r.mLitGroupFallback && !r.mLitGroupWide;};
    rig.shutdownStale();assert(!off(rig));rig.shutdown();assert(off(rig));
    rig.mSlot=ALCineLightRigSlot::A;cast.subjects[0].setNull();assert(!rig.resolveSlotAvatar());
    cast.subjects[0]={"1"};rig.mLitGroup="missing";rig.mLitMembersDirty=true;rig.tick();
    assert(rig.mLitGroupFallback && rig.mLastResolvedStoredMembers[0]==LLUUID{"1"});
    cast.runtime_ids[LLUUID{"stored-clone"}]={"runtime-clone"};
    cast.avatars[LLUUID{"runtime-clone"}]={{"runtime-clone"},4};
    cast.ids={LLUUID{"stored-clone"},gAgentID};++cast.cast_revision;rig.mLitGroup="guards";rig.mGroupEnabled=false;
    rig.tick();assert(rig.mLastResolvedStoredMembers[0]==LLUUID{"stored-clone"});
    assert(rig.mLastResolvedMembers[0]==LLUUID{"runtime-clone"});
    assert(recipients(rig)==rig.mLastResolvedMembers);
    // Control avatar recreation changes only the stored->runtime mapping.
    // Neither group nor cast revisions change. Failed lookup retries at 4 Hz.
    cast.ids={LLUUID{"animesh-root"}};++cast.cast_revision;
    cast.runtime_ids[LLUUID{"animesh-root"}]={"control-old"};
    cast.avatars[LLUUID{"control-old"}]={{"control-old"},12};
    rig.tick();assert(rig.mLastResolvedMembers==uuid_vec_t{LLUUID{"control-old"}});
    cast.avatars.erase(LLUUID{"control-old"});
    cast.runtime_ids[LLUUID{"animesh-root"}]={"control-new"};
    cast.avatars[LLUUID{"control-new"}]={{"control-new"},14};
    const auto revision=cast.cast_revision;const int retries=cast.calls[LLUUID{"animesh-root"}],work=cast.name_work;
    for(int i=0;i<20;++i){rig.tick();assert(rig.mLitGroupFallback);}
    assert(cast.calls[LLUUID{"animesh-root"}]==retries && cast.name_work==work);
    LLTimer::now+=.25;rig.tick();
    assert(rig.mLastResolvedMembers==uuid_vec_t{LLUUID{"control-new"}} && cast.calls[LLUUID{"animesh-root"}]==retries+1);
    assert(rig.mLastResolvedStoredMembers==uuid_vec_t{LLUUID{"animesh-root"}} && cast.cast_revision==revision);
    // Root initially unresolved: no revision is needed when its avatar appears.
    cast.ids={LLUUID{"late-root"}};++cast.group_revision;rig.tick();assert(rig.mLitGroupFallback);
    const int failed=cast.calls[LLUUID{"late-root"}];
    for(int interval=0;interval<3;++interval){
        for(int i=0;i<20;++i)rig.tick();assert(cast.calls[LLUUID{"late-root"}]==failed+interval);
        LLTimer::now+=.25;rig.tick();assert(cast.calls[LLUUID{"late-root"}]==failed+interval+1);
    }
    cast.runtime_ids[LLUUID{"late-root"}]={"late-control"};
    cast.avatars[LLUUID{"late-control"}]={{"late-control"},16};
    LLTimer::now+=.25;rig.tick();
    assert(rig.mLastResolvedMembers==uuid_vec_t{LLUUID{"late-control"}} && cast.name_work==work);
    // Two previously unresolved identities may recover to the same avatar.
    cast.ids={LLUUID{"late-root"},LLUUID{"alias-root"}};++cast.group_revision;rig.tick();
    cast.runtime_ids[LLUUID{"alias-root"}]={"late-control"};
    LLTimer::now+=.25;rig.tick();assert(rig.count==1 && rig.mLitGroupPresent==1);
    std::cout<<"PASS: production 6/16-member aggregate, cast-order cap, revision cache, per-tick away resolution, own-subject fallback/rebind, identity dedupe and legacy mask\n";
}
} // RigLitGroupModel
''')
    from rig_borrow_round2 import registry_fixture
    result.append(registry_fixture(root))
    return '\n'.join(result)


def run_registered_unit(root):
    import re
    from native_check_tools import check_directory, compile_run, viewer_native_flags
    tests = (root / 'indra/newview/tests/alcinelightrigmodel_test.cpp').read_text(encoding='utf8')
    registration = re.search(r'typedef test_group<cine_light_rig_model_data, (\d+)> cine_light_rig_model_group;', tests)
    assert registration and int(registration[1]) >= 136, 'lit-group tests must be registered, not merely compiled'
    source = ['''
#include "linden_common.h"
#include "lltut.h"
#include "alcinelightrigmodel.cpp"
namespace tut {
test_runner_singleton runner;
using namespace ALCineLightRigModel;
struct cine_light_rig_model_data {};
''', registration[0],
              'typedef cine_light_rig_model_group::object cine_light_rig_model_object;',
              'cine_light_rig_model_group cine_light_rig_model_tests("ALCineLightRigModel lit groups");']
    for number in (134, 135, 136):
        source += ['template<> template<>', function(tests, f'void cine_light_rig_model_object::test<{number}>()')]
    source.append('''
}
int main(){
 for(int n=134;n<=136;++n){auto result=tut::cine_light_rig_model_tests.run_test(n);
  if(result.result!=tut::test_result::ok){std::cerr<<result.message;return 1;}}
 std::cout<<"PASS: actual TUT registration and execution of lit-group tests 134-136\\n";
}
''')
    flags, libraries = viewer_native_flags()
    flags += ['/UNDEBUG', f'/I"{root / "indra/externals/tut"}"']
    result = compile_run(check_directory('rig_lit_group_tut'), '\n'.join(source),
                         'lit_group_tut', flags=flags, libraries=libraries)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    print(result.stdout.strip(), flush=True)
