"""Execute the production extra emitter and own-rig predicates with native math.

Viewer object/renderer services are adapters; placement, damping, rotation and
admission/pause predicates come directly from production. Products stay on I:.
"""
from pathlib import Path
from native_check_tools import check_directory, compile_run, viewer_native_flags
from rig_shadow_presets_check import function

ROOT = Path(__file__).resolve().parents[2]


def run_unit(root=ROOT):
    rig = (root / 'indra/newview/alcinelightrig.cpp').read_text(encoding='utf8')
    manager = (root / 'indra/newview/alcinelightrigmanager.cpp').read_text(encoding='utf8')
    derived = function(rig, 'ALCineLightRig::ExtraRimResult ALCineLightRig::extraRimFrom(')
    own = function(manager, 'ALCineLightRigManager::ExtraRimRecipientCache ALCineLightRigManager::extraRimRecipientCache(') + '\n' + function(manager, 'bool ALCineLightRigManager::hasOwnLitRig(')
    add = function(manager, 'bool ALCineLightRigManager::canAddExtraRim(')
    update = function(manager, 'void ALCineLightRigManager::updateExtraRims(')
    admission = 'if (hasOwnLitRig(id)) return refuse("Has own rig");'
    pause = 'if (hasOwnLitRig(extra.mActor, &recipients)) state = ExtraRimState::OWN_RIG;'
    assert admission in add and pause in update, 'xr_mirror_double_rim'
    basis_begin = rig.index('    if (mCaptureExtraRimBasis)')
    basis = rig[basis_begin:rig.index('    for (S32 i = 0; i < LIGHT_COUNT;', basis_begin)]
    enable = 'if (!mExtraRims.empty()) mInstances[mExtraRimSource].mCaptureExtraRimBasis = true;'
    disable = enable.replace('true;', 'false;')
    assert enable in manager and disable in manager, 'xr_basis_off'
    source = r'''
#include "linden_common.h"
#include "alextrarimmodel.h"
#include "llquaternion.h"
#include "v4math.h"
#include <memory>
#include <map>
#include <array>
#include <iostream>
#include <cstdlib>
struct Region {};
Region sharedRegion;
int emitterCreates=0;
struct Drawable { enum { FORCE_INVISIBLE }; void setState(int) {} };
using LLDrawable=Drawable;
template<class T> struct Ptr : std::shared_ptr<T> {
    using std::shared_ptr<T>::shared_ptr;
    bool isNull()const{return !*this;} bool notNull()const{return bool(*this);}
};
struct Emitter {
    LLVector3d pos; LLQuaternion rot; bool dead=false, light=true;
    Ptr<Drawable> mDrawable; Region* region=&sharedRegion;
    bool isDead()const{return dead;} bool getIsLight()const{return light;}
    Region* getRegion()const{return region;}
    LLVector3d getPositionGlobal()const{return pos;}
    LLQuaternion getRotation()const{return rot;}
    LLUUID getID()const{return LLUUID::null;}
    LLUUID getLightTextureID()const{return LLUUID::null;}
    LLVector3 getLightSRGBBaseColor()const{return LLVector3(1,1,1);}
    float getLightIntensity()const{return 1;} float getLightRadius()const{return 5;}
    float getLightFalloff()const{return 1;} float getLightCutoff()const{return 1;}
    LLVector3 getSpotLightParams()const{return LLVector3();}
    void setPositionGlobal(const LLVector3d& p,bool){pos=p;}
    void setRotation(const LLQuaternion& q,bool){rot=q;}
    void setScale(LLVector3,bool){} void setIsLight(bool b){light=b;}
    void setLightSRGBColor(LLVector3){} void setLightIntensity(float){}
    void setLightRadius(float){} void setLightFalloff(float){} void setLightCutoff(float){}
    void setLightTextureID(LLUUID){} void setSpotLightParams(LLVector3){}
    bool isLightSpotlight()const{return true;} void updateSpotLightPriority(){}
};
struct LLVOAvatar {
    LLVector3 point; bool dead=false;
    LLVOAvatar* asAvatar(){return this;}
    bool isDead()const{return dead;} float getUniformScale()const{return 1;}
};
struct Agent { LLVector3d getPosGlobalFromAgent(LLVector3 p){return LLVector3d(p);} } gAgent;
struct Pipeline {
    static bool hasGoboOverride(LLUUID){return false;}
    static int getGoboOverride(LLUUID){return 0;}
    static void setGoboOverride(LLUUID,int){} static void clearGoboOverride(LLUUID){}
    static bool isProjectorNoShadow(LLUUID){return true;}
    static void toggleProjectorCastShadows(LLUUID){}
    void updateMoveNormalAsync(Ptr<Drawable>){}
} gPipeline;
using LLPipeline=Pipeline;
float sanitizeSubjectScale(float s){return s;}
float emitterBoxEdge(float,float){return 1;}
bool memberTrackPoint(LLVOAvatar* a,int,float,LLVector3& p){p=a->point;return true;}
struct ALCineLightRig {
    enum class ExtraRimResult { ACTIVE, WAITING, AWAY, GEOMETRY, EMITTER_UNAVAILABLE };
    Ptr<Emitter> mProjectors[4]; bool mHaveSmoothedCentre=false;
    LLVector3d mSmoothedCentre; float mSmoothedScale=1, mCurrentRadius=5;
    double mLastPresentationTime=-1;
    struct Basis { int mTrackMode=0; float mOffsetZ=0,mDamping=1; bool mScaleAware=true; } mExtraRimBasis;
    bool mCaptureExtraRimBasis=false;
    LLUUID objectTarget; std::string group; bool grouped=false; U32 groupSlots=0;
    std::vector<LLUUID> members; LLVOAvatar* anchor=nullptr;
    LLUUID getObjectTarget()const{return objectTarget;}
    const std::string& getLitGroup()const{return group;}
    const std::vector<LLUUID>& lastResolvedMembers()const{return members;}
    bool isGroupEnabled()const{return grouped;} U32 lastResolvedGroupSlots()const{return groupSlots;}
    LLVOAvatar* resolveSlotAvatar()const{return anchor;}
    void captureBasis(int track_mode_setting,float offset_z_setting,float damping_setting,bool scale_aware_setting) {
        BASIS
    }
    LLVector4 mRigRim[4],mLightShapeSource[4];
    void shutdown(){for(auto& p:mProjectors)p.reset();mHaveSmoothedCentre=false;}
    void destroyEmitter(Ptr<Emitter>& p){p.reset();}
    bool createEmitter(Region* region,bool,const std::string&,Ptr<Emitter>& p){++emitterCreates;p=Ptr<Emitter>(new Emitter);p->region=region;return true;}
    ExtraRimResult extraRimFrom(const ALCineLightRig&,LLVOAvatar*,float,double);
};
struct LLDirectorCast {
    static LLUUID canonicalActorId(LLUUID id){return id;}
    static LLDirectorCast& instance(){static LLDirectorCast c;return c;}
    std::map<LLUUID,LLVOAvatar*> bodies;
    LLVOAvatar* resolve(LLUUID id,bool){auto i=bodies.find(id);return i==bodies.end()?nullptr:i->second;}
};
struct ObjectList {
    LLVOAvatar* findObject(LLUUID id){return LLDirectorCast::instance().resolve(id,false);}
} gObjectList;
struct ALCineLightRigManager {
    using Slot=int; static constexpr int SLOT_COUNT=5,SLOT_SELF=0;
    std::map<LLUUID,std::unique_ptr<ALCineLightRig>> mMirrors;
    ALCineLightRig mInstances[5]; std::vector<int> mExtraRims; int mExtraRimSource=3;
    void beginCapture(){ENABLE}
    void endCapture(){DISABLE}
    LLUUID actors[5]; bool lit[5]{};
    LLUUID slotActor(Slot s)const{return actors[s];}
    LLUUID groupSlotActor(Slot s)const{return actors[s];} bool isSlotLit(Slot s)const{return lit[s];}
        struct ExtraRimRecipientCache {
        U32 mLit=0; std::array<LLVOAvatar*,SLOT_COUNT> mAnchors{},mGroupSlots{};
    };
    ExtraRimRecipientCache extraRimRecipientCache()const;
    bool hasOwnLitRig(const LLUUID&,const ExtraRimRecipientCache* = nullptr)const;
    bool admit(LLUUID id)const {
        const auto refuse=[](const char*){return false;};
        ADMISSION
        return true;
    }
    enum class ExtraRimState { ACTIVE,OWN_RIG };
    ExtraRimState stateFor(LLUUID actor)const {
        struct { LLUUID mActor; } extra{actor};
        ExtraRimState state=ExtraRimState::ACTIVE;
        ExtraRimRecipientCache recipients=extraRimRecipientCache();
        PAUSE
        return state;
    }
};
'''.replace('ADMISSION', admission).replace('PAUSE', pause).replace('BASIS', basis).replace('ENABLE', enable).replace('DISABLE', disable)
    source += derived + '\n' + own + r'''
void require(bool ok,const char* name){if(!ok){std::cerr<<name<<"\n";std::exit(1);}}
int main(){
    ALCineLightRig src,dst; LLVOAvatar actor; actor.point=LLVector3(10,2,0);
    src.mProjectors[2]=Ptr<Emitter>(new Emitter);
    src.mHaveSmoothedCentre=true; src.mExtraRimBasis.mDamping=1;
    LLVector3d true_centre;
    for(int frame=0;frame<20;++frame){
        true_centre=LLVector3d(frame*.6,frame*.2,0);
        src.mSmoothedCentre+=(true_centre-src.mSmoothedCentre)*.1;
        src.mProjectors[2]->pos=src.mSmoothedCentre+LLVector3d(0,-3,2);
        LLVector3 aim(true_centre-src.mProjectors[2]->pos); aim.normalize();
        src.mProjectors[2]->rot.shortestArc(LLVector3(0,0,-1),aim);
        actor.point+=LLVector3(.2f,0,0);
        require(dst.extraRimFrom(src,&actor,1,frame*.1)==ALCineLightRig::ExtraRimResult::ACTIVE,"xr_damped_parallel");
        const LLVector3 actual=LLVector3(0,0,-1)*dst.mProjectors[2]->rot;
        require((actual-aim).length()<1.e-5f,"xr_damped_parallel");
        const auto expected=ALExtraRimModel::placement(src.mProjectors[2]->pos,src.mSmoothedCentre,
            src.mCurrentRadius,src.mSmoothedScale,dst.mSmoothedCentre,dst.mSmoothedScale);
        require((dst.mProjectors[2]->pos-expected.mPosition).length()<1.e-6,"xr_scaled_world_offset");
    }
    require(emitterCreates==1,"xr_emitter_churn");
    src.mHaveSmoothedCentre=false;
    require(dst.extraRimFrom(src,&actor,1,3)==ALCineLightRig::ExtraRimResult::WAITING,"xr_failure_reason");
    src.mHaveSmoothedCentre=true; src.mProjectors[2]->pos=src.mSmoothedCentre;
    require(dst.extraRimFrom(src,&actor,1,4)==ALCineLightRig::ExtraRimResult::GEOMETRY,"xr_failure_reason");
    actor.dead=true;
    require(dst.extraRimFrom(src,&actor,1,5)==ALCineLightRig::ExtraRimResult::AWAY,"xr_failure_reason");
    ALCineLightRigManager mgr; LLUUID clone("00000000-0000-0000-0000-000000000001"); mgr.lit[0]=true;
    mgr.mMirrors[clone]=std::make_unique<ALCineLightRig>();
    require(!mgr.admit(clone),"xr_mirror_double_rim"); // Mirror before Add.
    mgr.mMirrors.clear();
    require(mgr.admit(clone) && mgr.stateFor(clone)==ALCineLightRigManager::ExtraRimState::ACTIVE,"xr_mirror_double_rim");
    mgr.mMirrors[clone]=std::make_unique<ALCineLightRig>(); // Add before Mirror.
    require(mgr.stateFor(clone)==ALCineLightRigManager::ExtraRimState::OWN_RIG,"xr_mirror_double_rim");
    mgr.mMirrors.clear();
    require(mgr.stateFor(clone)==ALCineLightRigManager::ExtraRimState::ACTIVE,"xr_mirror_double_rim");
    LLVOAvatar member; LLDirectorCast::instance().bodies[clone]=&member;
    mgr.mInstances[0].group="cast group";mgr.mInstances[0].members.push_back(clone);
    require(!mgr.admit(clone) && mgr.stateFor(clone)==ALCineLightRigManager::ExtraRimState::OWN_RIG,"xr_double_rim_group_member");
    mgr.mInstances[0].group.clear();mgr.mInstances[0].members.clear();
    mgr.actors[2]=clone;mgr.mInstances[0].grouped=true;mgr.mInstances[0].groupSlots=1u<<2;
    require(!mgr.admit(clone) && mgr.stateFor(clone)==ALCineLightRigManager::ExtraRimState::OWN_RIG,"xr_double_rim_group_slots");
    // Cast treats null as You; the registry deliberately skips an empty slot.
    mgr.actors[2]=LLUUID::null;LLDirectorCast::instance().bodies[LLUUID::null]=&member;
    require(mgr.admit(clone),"xr_empty_group_slot_is_not_self");
    mgr.mInstances[0].grouped=false;mgr.mInstances[0].groupSlots=0;
    mgr.mInstances[0].anchor=&member;
    require(!mgr.admit(clone),"xr_anchor_recipient");
    mgr.actors[0]=clone;mgr.mInstances[0].objectTarget=LLUUID("00000000-0000-0000-0000-000000000002");
    require(mgr.admit(clone) && mgr.stateFor(clone)==ALCineLightRigManager::ExtraRimState::ACTIVE,"xr_self_object_rig_pauses");
    mgr.beginCapture();
    for(auto& instance:mgr.mInstances){
        instance.captureBasis(1,3,2,false);
        require(!instance.mCaptureExtraRimBasis && instance.mExtraRimBasis.mOffsetZ==0,"xr_basis_off");
    }
    mgr.mExtraRims.push_back(1);mgr.beginCapture();
    for(int i=0;i<5;++i){
        auto& instance=mgr.mInstances[i];instance.captureBasis(1,3,2,false);
        require(instance.mCaptureExtraRimBasis==(i==3) &&
            instance.mExtraRimBasis.mOffsetZ==(i==3?3:0),"xr_basis_off");
    }
    mgr.endCapture();require(!mgr.mInstances[3].mCaptureExtraRimBasis,"xr_basis_off");
    std::cout<<"PASS: production moving/damped extra remains parallel; placement preserved; truthful failures; both mirror orders pause/reject and resume\n";
}
'''
    flags, libraries = viewer_native_flags()
    directory = check_directory('extra_rims_engine')
    baseline = compile_run(directory, source, 'baseline', flags, libraries)
    output = baseline.stdout + baseline.stderr
    (directory / 'baseline.log').write_bytes(output.encode('utf8'))
    assert baseline.returncode == 0, output
    print(baseline.stdout, end='', flush=True)
    mutations = {
        'xr_emitter_churn': ('if (dst.notNull() && (dst->isDead() || dst->getRegion() != src->getRegion()))', 'if (dst.notNull())'),
        'xr_double_rim_group_member': ('if (object && object->asAvatar() == avatar) return true;', 'if (false) return true;'),
        'xr_double_rim_group_slots': ('if ((slots & (1u << j)) && cache.mGroupSlots[j] == avatar) return true;', 'if (false) return true;'),
        'xr_empty_group_slot_is_not_self': ('if (actor.notNull()) cache.mGroupSlots[i]', 'cache.mGroupSlots[i]'),
        'xr_self_object_rig_pauses': ('|| rig.getObjectTarget().notNull()) continue;', ') continue;'),
        'xr_damped_parallel': ('dst->setRotation(src->getRotation(), false);',
            'LLVector3 aim(mSmoothedCentre - placement.mPosition); aim.normalize(); '
            'LLQuaternion rotation; rotation.shortestArc(LLVector3(0,0,-1),aim); dst->setRotation(rotation,false);'),
        'xr_mirror_double_rim': ('if (mirror != mMirrors.end() && isSlotLit(SLOT_SELF)) return true;',
                               'if (false) return true;'),
        'xr_failure_reason': ('return ExtraRimResult::WAITING;', 'return ExtraRimResult::AWAY;'),
        'xr_basis_off': ('if (mCaptureExtraRimBasis)', 'if (true)'),
    }
    for name, (before, after) in mutations.items():
        assert source.count(before) == 1, (name, 'anchor drift')
        result = compile_run(directory, source.replace(before, after, 1), name, flags, libraries)
        output = result.stdout + result.stderr
        (directory / (name + '.log')).write_bytes(output.encode('utf8'))
        assert result.returncode != 0 and name in output.splitlines(), (name, output)
        print('PASS: rejected planted ' + name + ' at its named assertion', flush=True)


if __name__ == '__main__':
    run_unit()
